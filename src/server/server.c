#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#elif _WIN32
#include <inttypes.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "cipher.h"
#include "threadpool.h"
#include "opt.h"
#include "server.h"
#include "stringer.h"

static threadpool_t threadpool;
static char aux_log[100];
static char *arg_folder;
static char *arg_config;
static int arg_port = 0;
static int arg_threads = 0;
static int exit_asked = 0;

int inherit_configuration(char *config_file) {
        if (access(arg_config, F_OK) == -1) {
                sprintf(aux_log, "config: file \"%s\" seems not to be existing.", arg_config);
                std_err(aux_log);
                return 1;
        } else {
                FILE *config;
                char line[1000];

                config = fopen(config_file, "r");
                if (config == NULL) {
                        return 1;
                }
                char *config_key, *config_value;
                while(fgets(line, 1000, config)) {
                        config_key = strtok(line, "=");
                        config_value = strtok(NULL, "=");
                        config_key = trim(config_key);
                        config_value = trim(config_value);
                        if (strcasecmp(config_key, "port") == 0) {
                                sprintf(aux_log, "config: found \"%s\" value \"%s\".", config_key, config_value);
                                std_out(aux_log);
                                if (!isdigit(*config_value)) {
                                        sprintf(aux_log, "config: \"%s\" key value \"%s\" must be integer parseable.", config_key, config_value);
                                        std_err(aux_log);
                                        return 1;
                                }
                                arg_port = atoi(config_value);
                        } else if (strcasecmp(config_key, "folder") == 0) {
                                sprintf(aux_log, "config: found \"%s\" value \"%s\".", config_key, config_value);
                                std_out(aux_log);
                                arg_folder = calloc(strlen(config_value) + 1, sizeof(char));
                                strcpy(arg_folder, config_value);
                        } else {
                                sprintf(aux_log, "config: key \"%s\" not recognized, ignored.", config_key);
                                std_err(aux_log);
                        }
                }

                fclose(config);
        }
}

#ifdef __linux__
void sig_hup_handler(int sig) {
        if (sig == SIGHUP) {
                std_out("SIGHUP: flushing configuration.");
                int status = inherit_configuration(arg_config);
                if (status != 0) {
                        std_err("Unable to read configuration. Check syntax.");
                }
        }
}

void sig_int_handler(int sig) {
        if (sig == SIGINT) {
                std_out("SIGINT: triggering exit.");
                exit_asked = 1;
        }
}
#endif

void list_opt(char *ret_out, int recursive, char *folder, char *folder_suffix) {
#ifdef __linux__
        char folder_abs[strlen(folder) + 2 + (folder_suffix != NULL ? strlen(folder_suffix) + 1 : 0)];
        if (folder_suffix == NULL) {
                strcpy(folder_abs, folder);
        } else {
                sprintf(folder_abs, "%s/%s", folder, folder_suffix);
        }
        DIR *d = opendir(folder_abs);
        FILE *filename;
        struct dirent *dir;
        if (d) {
                while ((dir = readdir(d)) != NULL) {
                        if (dir->d_type == DT_REG) {
                                char *filename_path = calloc(strlen(folder_abs) + 2 + strlen(dir->d_name) + 1, sizeof(char));
                                sprintf(filename_path, "%s/%s", folder_abs, dir->d_name);
                                filename = fopen(filename_path, "r");
                                if (filename == NULL) {
                                        sprintf(aux_log, "Unable to access file \"%s\".", filename_path);
                                        std_err(aux_log);
                                        continue;
                                }
                                struct stat filename_stat;
                                if (fstat(fileno(filename), &filename_stat) == -1) {
                                        sprintf(aux_log, "Unable to read file \"%s\" stat.", filename_path);
                                        std_err(aux_log);
                                        continue;
                                }
                                char size_str[5];
                                sprintf(size_str, "%d", filename_stat.st_size);
                                strcat(ret_out, size_str);
                                if (strlen(size_str) < TAB_SIZE) {
                                        strcat(ret_out, "\t\t");
                                } else {
                                        strcat(ret_out, "\t");
                                }
                                if (folder_suffix != NULL) {
                                        strcat(ret_out, folder_suffix);
                                        strcat(ret_out, "/");
                                }
                                strcat(ret_out, dir->d_name);
                                strcat(ret_out, "\r\n");
                                if (filename != NULL) {
                                        fclose(filename);
                                }
                                if (filename_path != NULL) {
                                        free(filename_path);
                                }
                        } else if (dir->d_type == DT_DIR && recursive == LST_R && strcasecmp(dir->d_name, "..") != 0 && strcasecmp(dir->d_name, ".") != 0) {
                                char *folder_suffix_sub;
                                if (folder_suffix == NULL) {
                                        folder_suffix_sub = calloc(strlen(dir->d_name) + 1, sizeof(char));
                                        strcpy(folder_suffix_sub, dir->d_name);
                                } else {
                                        folder_suffix_sub = calloc(strlen(folder_suffix) + 2 + strlen(dir->d_name) + 1, sizeof(char));
                                        sprintf(folder_suffix_sub, "%s/%s", folder_suffix, dir->d_name);
                                }
                                list_opt(ret_out, LST_R, folder, folder_suffix_sub);
                                if (folder_suffix_sub) {
                                        free(folder_suffix_sub);
                                }
                        }
                }
                closedir(d);
        }
#elif _WIN32
        char folder_abs[strlen(folder) + 2 + (folder_suffix != NULL ? strlen(folder_suffix) + 1 : 0) + 3];
        if (folder_suffix == NULL) {
                sprintf(folder_abs, "%s\\*.*", folder);
        } else {
                sprintf(folder_abs, "%s\\%s\\*.*", folder, folder_suffix);
        }
        WIN32_FIND_DATA find_data;
        HANDLE dir = FindFirstFile(folder_abs, &find_data);
        if (dir == INVALID_HANDLE_VALUE) {
                sprintf(aux_log, "Invalid handle value for dir \"%s\".", dir);
                std_err(aux_log);
        }
        do {
                if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                        char *filename_path = calloc(strlen(folder_abs) + 2 + strlen(find_data.cFileName) + 1, sizeof(char));
                        sprintf(filename_path, "%s\\%s", folder_abs, find_data.cFileName);
                        char size_str[5];
                        uint64_t size = (((uint64_t) find_data.nFileSizeHigh) << 32) | find_data.nFileSizeLow;
                        sprintf(size_str, "%" PRIu64 " ", (uintmax_t) size);
                        strcat(ret_out, size_str);
                        if (strlen(size_str) < TAB_SIZE) {
                                strcat(ret_out, "\t\t");
                        } else {
                                strcat(ret_out, "\t");
                        }
                        if (folder_suffix != NULL) {
                                strcat(ret_out, folder_suffix);
                                strcat(ret_out, "\\");
                        }
                        strcat(ret_out, find_data.cFileName);
                        strcat(ret_out, "\r\n");
                        if (filename_path != NULL) {
                                free(filename_path);
                        }
                } else if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && recursive == LST_R
                           && strcasecmp(find_data.cFileName, "..") != 0
                           && strcasecmp(find_data.cFileName, ".") != 0) {
                        char *folder_suffix_sub;
                        if (folder_suffix == NULL) {
                                folder_suffix_sub = calloc(strlen(find_data.cFileName) + 1, sizeof(char));
                                strcpy(folder_suffix_sub, find_data.cFileName);
                        } else {
                                folder_suffix_sub = calloc(strlen(folder_suffix) + 2 + strlen(find_data.cFileName) + 1, sizeof(char));
                                sprintf(folder_suffix_sub, "%s\\%s", folder_suffix, find_data.cFileName);
                        }
                        list_opt(ret_out, LST_R, folder, folder_suffix_sub);
                        if (folder_suffix_sub) {
                                free(folder_suffix_sub);
                        }
                }
        } while (FindNextFile(dir, &find_data));
        FindClose(dir);
#endif
}

void list(char *ret_out, int recursive) {
        list_opt(ret_out, recursive, arg_folder, NULL);
}

sock_t create_socket(int port) {
        std_out("Allocating variables.");
        sock_t sock;
        struct sockaddr_in sock_conf;
        int status;

        sprintf(aux_log, "Initializing socket on 0.0.0.0:%d", port);
        std_out(aux_log);
        sock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef __linux__
        int enable = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                std_err("setsockopt(SO_REUSEADDR) failed.");
#elif _WIN32
        if (sock == INVALID_SOCKET) {
                std_err("Unable to initialize socket.");
#endif
                exit(EXIT_FAILURE);
        }

        sock_conf.sin_family = AF_INET;
        sock_conf.sin_addr.s_addr = INADDR_ANY;
        sock_conf.sin_port = htons(port);

#ifdef __linux__
        status = fcntl(sock, F_SETFL, O_NONBLOCK);
#elif _WIN32
        u_long sock_nonblock = 1;
        status = ioctlsocket(sock, FIONBIO, &sock_nonblock);
#endif
        if (status != 0) {
                sprintf(aux_log, "Error %d manipulating socket file descriptor.", status);
                std_err(aux_log);
                exit(EXIT_FAILURE);
        }

        std_out("Binding socket.");
        status = bind(sock,(struct sockaddr*) &sock_conf, sizeof(sock_conf));
#ifdef __linux__
        if (status != 0) {
#elif _WIN32
        if (status == SOCKET_ERROR) {
#endif
                sprintf(aux_log, "Error %d binding socket.", status);
                std_err(aux_log);
                exit(EXIT_FAILURE);
        }

        std_out("Listening for connection.");
        status = listen(sock, 10);
        if (status != 0) {
                sprintf(aux_log, "Error %d making socket listening.", status);
                std_err(aux_log);
                exit(EXIT_FAILURE);
        }

        return sock;
}

void close_socket(sock_t sock) {
        sprintf(aux_log, "Closing socket.");
        std_sck(sock, aux_log);
#ifdef __linux__
        close(sock);
#elif _WIN32
        closesocket(sock);
#endif
}

int write_socket(sock_t sock, char *message, int last) {
        int status;
        const char *suffix = &message[strlen(message)-2];
        if (strcasecmp(suffix, "\r\n") != 0) {
                strcat(message, "\r\n");
        }
        if (last) {
                strcat(message, ".\r\n");
        }
#ifdef __linux__
        if ((status = write(sock, message, strlen(message))) < 0) {
#elif _WIN32
        if ((status = send(sock, message, strlen(message), 0)) < 0) {
#endif
                sprintf(aux_log, "Unable to send message.");
                std_sck(sock, aux_log);
                return -1;
        }
        return 0;
}

void handle_connection(void *incoming_conn) {
        sock_t sock = *((sock_t *) incoming_conn);
        free(incoming_conn);

        char buffer[512+1];
        int packet_length = 0;
        int packet_empty = 0;
        buffer[512] = 0;
        for (;; ) {
                packet_length = recv(sock, buffer, 512, 0);
#ifdef __linux__
                if (packet_length < 0) {
#elif _WIN32
                if (packet_length == SOCKET_ERROR) {
#endif
                        sprintf(aux_log, "Unable to read message.");
                        std_sck(sock, aux_log);
                } else {
                        buffer[packet_length] = '\0';
                        if (packet_length <= 2) {
                                packet_empty++;
                                sprintf(aux_log, "Packet too small (%d).", packet_empty+1);
                                std_sck(sock, aux_log);
                                if (packet_empty >= 3) {
                                        packet_empty = 0;
                                        // close_socket(sock);
                                        break;
                                } else {
                                        continue;
                                }
                        }
                        for (int index = packet_length-2; index < 512; index++) {
                                buffer[index] = 0;
                        }

                        // sprintf(aux_log, "Message received (%d): %s", packet_length, buffer);
                        // std_out(aux_log);

                        char *message = calloc(strlen(buffer) + 1, sizeof(char));
                        strcpy(message, buffer);
                        char *command = strtok(buffer, " ");
                        for(int i = 0; command[i]; i++) {
                                command[i] = tolower(command[i]);
                        }
                        sprintf(aux_log, "Message \"%s\" (command: \"%s\").", message, command);
                        std_sck(sock, aux_log);

                        char ret_out[1000000];
                        if (strcasecmp(command, CMD_EXIT) == 0) {
                                close_socket(sock);
                                break;
                        } else if (strcasecmp(command, CMD_LSTF) == 0) {
                                sprintf(aux_log, "300");
                                write_socket(sock, aux_log, 0);
                                sprintf(ret_out, "");
                                list(ret_out, LST_F);
                                write_socket(sock, ret_out, 1);
                        } else if (strcasecmp(command, CMD_LSTR) == 0) {
                                sprintf(aux_log, "300");
                                write_socket(sock, aux_log, 0);
                                sprintf(ret_out, "");
                                list(ret_out, LST_R);
                                write_socket(sock, ret_out, 1);
                        } else if (strcasecmp(command, CMD_ENCR) == 0 || strcasecmp(command, CMD_DECR) == 0) {
                                char *str_seed;
                                char *enc_path;
                                for (int i=0; i<3; i++) {
                                        switch (i) {
                                        case 0:
                                                strtok(message, " ");
                                                break;
                                        case 1:
                                                str_seed = strtok(NULL, " ");
                                                break;
                                        case 2:
                                                enc_path = strtok(NULL, " ");
                                                break;
                                        }
                                }
                                char enc_path_from[strlen(arg_folder) + 1 + strlen(enc_path)];
                                char enc_path_to[strlen(arg_folder) + 1 + strlen(enc_path) + 4];
                                if (strcasecmp(command, CMD_ENCR) == 0) {
#ifdef __linux__
                                        sprintf(enc_path_from, "%s/%s\0", arg_folder, enc_path);
#elif _WIN32
                                        sprintf(enc_path_from, "%s\\%s\0", arg_folder, enc_path);
#endif
                                        sprintf(enc_path_to, "%s_enc\0", enc_path_from);
                                } else {
#ifdef __linux__
                                        sprintf(enc_path_from, "%s/%s\0", arg_folder, enc_path);
#elif _WIN32
                                        sprintf(enc_path_from, "%s\\%s\0", arg_folder, enc_path);
#endif
                                        sprintf(enc_path_to, "%s", enc_path_from);
                                        enc_path_to[strlen(enc_path_to) - 4] = 0;
                                }
                                if (access(enc_path_from, F_OK) == -1) {
                                        sprintf(aux_log, "Input file \"%s\" does not exist.", enc_path_from);
                                        std_sck(sock, aux_log);
                                        sprintf(aux_log, "%d Cannot access to \"%s\"", CMD_NOK, enc_path_from);
                                        write_socket(sock, aux_log, 1);
                                        continue;
                                }

                                char *ptr;
                                unsigned long seed;
                                seed = strtoul(str_seed, &ptr, 10);
                                sprintf(aux_log, "Gonna cipher using seed \"%lu\"", seed);
                                std_out(aux_log);

                                int status = cipher(enc_path_from, enc_path_to, seed);
                                if (status != CMD_CPH_OK) {
                                        sprintf(aux_log, "%d Unable to apply %s on \"%s\"", status, command, enc_path_from);
                                        std_err(aux_log);
                                        write_socket(sock, aux_log, 1);
                                } else {
                                        sprintf(aux_log, "Unlinking \"%s\".", enc_path_from);
                                        std_out(aux_log);

                                        if (unlink(enc_path_from)) {
                                                std_err("Something went wrong during file deletion.");
                                        } else {
                                                sprintf(aux_log, "%d Applied %s on \"%s\"", status, command, enc_path_from);
                                                write_socket(sock, aux_log, 1);
                                        }
                                }
                        } else {
                                sprintf(ret_out, "Command not recognized.");
                                write_socket(sock, ret_out, 1);
                        }
                        // buffer cleanup
                        for (int index = 0; index < 512; index++) {
                                buffer[index] = 0;
                        }
                        free(message);
                }
        }
}

int main(int argc, char *argv[]) {
        int opt = 0;

        while ((opt = getopt(argc, argv, "c:f:p:n:")) != -1) {
                switch(opt) {
                case 'c':
                        arg_folder = optarg;
                        break;
                case 'f':
                        arg_config = optarg;
                        break;
                case 'n':
                        arg_threads = *optarg;
                        break;
                case 'p':
                        if (!isdigit(*optarg)) {
                                sprintf(aux_log, "'-p' input must be an integer parseable value (passed: \"%s\").", optarg);
                                std_err(aux_log);
                                exit(EXIT_FAILURE);
                        }
                        arg_port = atoi(optarg);
                        break;
                case '?':
                        if (optopt == 'f' && (optopt == 'c' || optopt == 'p')) {
                                sprintf(aux_log, "Missing '-%c' mandatory input.", optopt);
                                std_err(aux_log);
                        } else {
                                std_err("Invalid option received.");
                        }
                        exit(EXIT_FAILURE);
                        break;
                }
        }
        if (arg_config != NULL) {
                int status = inherit_configuration(arg_config);
                if (status != 0) {
                        std_err("Unable to load configuration from file. Exiting.");
                        exit(EXIT_FAILURE);
                }
        }

        if (access(arg_folder, F_OK) == -1) {
                std_out("'-c' input must be an existing filesystem path.");
                exit(EXIT_FAILURE);
        }
        if (arg_port == 0) {
                arg_port = CFG_DEFAULT_PORT;
        }
        if (arg_threads == 0) {
                arg_threads = CFG_DEFAULT_THREADS;
        }

        sprintf(aux_log, "Config { port=%d, folder=\"%s\", threads=%d }", arg_port, arg_folder, arg_threads);
        std_out(aux_log);

        sprintf(aux_log, "Creating threadpool by %d.", arg_threads);
        std_out(aux_log);
        threadpool_init(&threadpool, arg_threads);

#ifdef __linux__
        std_out("Configuring SIGHUP signals catching.");
        if (signal(SIGHUP, sig_hup_handler) == SIG_ERR) {
                std_err("Unable to handle SIGHUP.");
        }

        std_out("Configuring SIGINT signals catching.");
        if (signal(SIGINT, sig_int_handler) == SIG_ERR) {
                std_err("Unable to handle SIGINT.");
        }
#elif _WIN32
        WSADATA wsa;
        std_out("Initializing WinSock.");
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
                sprintf(aux_log, "Initialization failed. Code: %d.", WSAGetLastError());
                std_err(aux_log);
                exit(EXIT_FAILURE);
        }
#endif

        sock_t sock_passive;
        int exit_condition = 0;

        sock_passive = create_socket(arg_port);

        std_out("Waiting for connections.");
        for (;; ) {
                sock_t sock_active;
                if (exit_asked) {
                        break;
                }
                if ((sock_active = accept(sock_passive, 0, 0)) != -1) {
                        sprintf(aux_log, "Welcomed new connection: %d.", sock_active);
                        std_out(aux_log);
                        int *sock_incoming = malloc(sizeof(int));
                        *sock_incoming = sock_active;
                        threadpool_add_job(&threadpool, &handle_connection, sock_incoming);
                }
        }

        threadpool_bye(&threadpool);

        close_socket(sock_passive);
#ifdef _WIN32
        WSACleanup();
#endif
        std_out("Exited.");

        return 0;
}
