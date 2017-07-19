#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cipher.h"
#include "opt.h"
#include "server.h"
#include "stringer.h"
#include "threadpool.h"

threadpool thpool;
char aux_log[100];
char *arg_folder;
char *arg_config;
int arg_port = 0;
int arg_threads = 0;
int exit_asked = 0;

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

void list_opt(char *ret_out, int recursive, char *folder, char *folder_suffix) {
        char *folder_abs;
        if (folder_suffix == NULL) {
                folder_abs = calloc(strlen(folder), sizeof(char));
                strcpy(folder_abs, folder);
        } else {
                folder_abs = calloc(strlen(folder) + 1 + strlen(folder_suffix), sizeof(char));
                sprintf(folder_abs, "%s/%s", folder, folder_suffix); // superunix
        }
        DIR *d;
        struct dirent *dir;
        d = opendir(folder_abs);
        if (d) {
                while ((dir = readdir(d)) != NULL) {
                        if (dir->d_type == DT_REG) {
                                char *filename_path = calloc(strlen(folder_abs) + "/" + strlen(dir->d_name), sizeof(char)); // superunix
                                sprintf(filename_path, "%s/%s", folder_abs, dir->d_name);
                                FILE *filename = fopen(filename_path, "r+");
                                if (filename == NULL) {
                                        sprintf(aux_log, "Unable to access file \"%s\".", filename_path);
                                        std_err(aux_log);
                                        continue;
                                }
                                fseek(filename, 0, SEEK_END);
                                int size = ftell(filename);
                                fseek(filename, 0, SEEK_SET);
                                char size_str[5];
                                sprintf(size_str, "%d", size);
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
                                free(filename_path);
                        } else if (dir->d_type == DT_DIR && recursive == LST_R && strcasecmp(dir->d_name, "..") != 0 && strcasecmp(dir->d_name, ".") != 0) { // superunix
                                char *folder_suffix_sub;
                                if (folder_suffix == NULL) {
                                        folder_suffix_sub = calloc(strlen(dir->d_name), sizeof(char));
                                        strcpy(folder_suffix_sub, dir->d_name);
                                } else {
                                        folder_suffix_sub = calloc(strlen(folder_suffix) + 1 + strlen(dir->d_name), sizeof(char)); // superunix
                                        sprintf(folder_suffix_sub, "%s/%s", folder_suffix, dir->d_name);
                                }
                                list_opt(ret_out, LST_R, folder, folder_suffix_sub);
                                free(folder_suffix_sub);
                        }
                }
                closedir(d);
        }
        free(folder_abs);
}

void list(char *ret_out, int recursive) {
        list_opt(ret_out, recursive, arg_folder, NULL);
}

int toggle_cipher(char *str_seed, char *enc_path_from, char *enc_path_to) {
        int enc_seed = atoi(str_seed);
        srand(enc_seed);
        unsigned int enc_key = rand();

        sprintf(aux_log, "Gonna cipher \"%s\" to \"%s\" using key \"%d\"", enc_path_from, enc_path_to, enc_key);
        std_out(aux_log);

        int enc_out = cipher(enc_path_from, enc_path_to, enc_key, aux_log);
        if (enc_out != CMD_CPH_OK) {
                std_err(aux_log);
                return enc_out;
        }

        sprintf(aux_log, "Everything gone as expected: going to unlink \"%s\".", enc_path_from);
        std_out(aux_log);

        int deletion = unlink(enc_path_from);
        if (deletion < 0) {
                std_err("Something went wrong during file deletion.");
                return CMD_NOK;
        } else {
                std_out("Succesfully ciphered.");
        }

        return CMD_CPH_OK;
}

int create_socket(int port) {
        std_out("Allocating variables.");
        int sock, status;
        struct sockaddr_in sock_conf;

        sprintf(aux_log, "Initializing socket on 0.0.0.0:%d", port);
        std_out(aux_log);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        int enable = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                std_err("setsockopt(SO_REUSEADDR) failed.");
                exit(EXIT_FAILURE);
        }

        sock_conf.sin_family = AF_INET;
        sock_conf.sin_addr.s_addr = INADDR_ANY;
        sock_conf.sin_port = htons(port);

        status = fcntl(sock, F_SETFL, O_NONBLOCK);
        if (status != 0) {
                sprintf(aux_log, "Error %d manipulating socket file descriptor.", status);
                std_err(aux_log);
                exit(EXIT_FAILURE);
        }

        std_out("Binding socket.");
        status = bind(sock,(struct sockaddr*) &sock_conf, sizeof(sock_conf));
        if (status != 0) {
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

void close_socket(int sock) {
        sprintf(aux_log, "Closing socket %d.", sock);
        std_out(aux_log);
        close(sock);
}

int write_socket(int sock, char *message, int last) {
        int status;
        strcat(message, "\r\n");
        if (last) {
                strcat(message, ".\r\n");
        }
        if ((status = write(sock, message, strlen(message))) < 0) {
                sprintf(aux_log, "Unable to send message: error %d.", status);
                std_err(aux_log);
                return -1;
        }
        return 0;
}

void handle_connection(int sock) {
        char buffer[512+1];
        int packet_length;
        int packet_empty = 0;
        buffer[512] = 0;
        for (;; ) {
                if ((packet_length = read(sock, buffer, 512)) < 0) {
                        std_err("Unable to read message.");
                        break;
                } else {
                        if (packet_length <= 2) {
                                packet_empty++;
                                sprintf(aux_log, "Packet too small (%d).", packet_empty+1);
                                std_err(aux_log);
                                if (packet_empty >= 3) {
                                        packet_empty = 0;
                                        close_socket(sock);
                                        break;
                                } else {
                                        continue;
                                }
                        }
                        for (int index = packet_length-2; index < 512; index++) {
                                buffer[index] = 0;
                        }

                        sprintf(aux_log, "Message received (%d): %s", packet_length, buffer);
                        std_out(aux_log);

                        char* message = calloc(strlen(buffer)+1, sizeof(char));
                        strcpy(message, buffer);
                        char *command = strtok(buffer, " ");
                        for(int i = 0; command[i]; i++) {
                                command[i] = tolower(command[i]);
                        }
                        sprintf(aux_log, "Message \"%s\" (command: \"%s\").", message, command);
                        std_out(aux_log);

                        char ret_out[5000];
                        if (strcasecmp(command, CMD_EXIT) == 0) {
                                std_out("Exit asked. Releasing socket.");
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
                                char *enc_path_from;
                                char *enc_path_to;
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

                                if (strcasecmp(command, CMD_ENCR) == 0) {
                                        enc_path_from = calloc(strlen(arg_folder) + 1 + strlen(enc_path), sizeof(char));
                                        enc_path_to = calloc(strlen(arg_folder) + 1 + strlen(enc_path) + 4, sizeof(char));
                                        sprintf(enc_path_from, "%s/%s", arg_folder, enc_path); // superunix
                                        sprintf(enc_path_to, "%s_enc", enc_path_from);
                                } else {
                                        enc_path_from = calloc(strlen(arg_folder) + 1 + strlen(enc_path), sizeof(char));
                                        enc_path_to = calloc(strlen(arg_folder) + 1 + strlen(enc_path) - 4, sizeof(char));
                                        sprintf(enc_path_from, "%s/%s", arg_folder, enc_path); // superunix
                                        sprintf(enc_path_to, "%s", enc_path_from);
                                        enc_path_to[strlen(enc_path_to) - 4] = 0;
                                }
                                if (access(enc_path_from, F_OK) == -1) {
                                        sprintf(aux_log, "Input file \"%s\" does not exist.", enc_path_from);
                                        std_err(aux_log);
                                        sprintf(aux_log, "%d Cannot access to \"%s\"", CMD_NOK, enc_path_from);
                                        write_socket(sock, aux_log, 1);
                                        continue;
                                }
                                int ret_code = toggle_cipher(str_seed, enc_path_from, enc_path_to);
                                if (ret_code == CMD_CPH_OK) {
                                        sprintf(aux_log, "%d Applied %s on \"%s\"", ret_code, command, enc_path_from);
                                } else {
                                        sprintf(aux_log, "%d Unable to apply %s on \"%s\"", ret_code, command, enc_path_from);
                                }
                                write_socket(sock, aux_log, 1);
                        } else {
                                sprintf(ret_out, "Command not recognized.");
                                write_socket(sock, ret_out, 1);
                        }

                        // buffer cleanup
                        for (int index = 0; index < 512; index++) {
                                buffer[index] = 0;
                        }
                }
        }
}

int inherit_configuration(char *config_file) {
        if (access(arg_config, F_OK) == -1) {
                sprintf(aux_log, "config: file \"%s\" seems not to be existing.", arg_config);
                std_err(aux_log);
                return 1;
        } else {
                FILE *config;
                char *line = NULL;
                size_t len = 0;
                ssize_t read;

                config = fopen(config_file, "r");
                if (config == NULL) {
                        return 1;
                }
                char *config_key, *config_value;
                while ((read = getline(&line, &len, config)) != -1) {
                        line[read-1] = 0;
                        sprintf(aux_log, "config: line of lenght %zu: \"%s\"", read, line);
                        std_out(aux_log);
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
                                arg_folder = calloc(strlen(config_value), sizeof(char));
                                strcpy(arg_folder, config_value);
                        } else {
                                sprintf(aux_log, "config: key \"%s\" not recognized, ignored.", config_key);
                                std_err(aux_log);
                        }
                }

                fclose(config);
                if (line) {
                        free(line);
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
        thpool = thpool_init(arg_threads);

        std_out("Configuring SIGHUP signals catching.");
        if (signal(SIGHUP, sig_hup_handler) == SIG_ERR) {
                std_err("Unable to handle SIGHUP.");
        }

        std_out("Configuring SIGINT signals catching.");
        if (signal(SIGINT, sig_int_handler) == SIG_ERR) {
                std_err("Unable to handle SIGINT.");
        }

        int sock_passive;
        int exit_condition = 0;

        sock_passive = create_socket(arg_port);

        std_out("Waiting for connections.");
        for (;; ) {
                int sock_active;
                if (exit_asked) {
                        break;
                }
                if ((sock_active = accept(sock_passive, 0, 0)) != -1) {
                        thpool_add_work(thpool, (void*)handle_connection, sock_active);
                }
        }

        thpool_wait(thpool);
        thpool_destroy(thpool);

        close_socket(sock_passive);
        std_out("Exited.");

        return 0;
}
