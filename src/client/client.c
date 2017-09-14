#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#elif _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include "client.h"
#include "opt.h"

static int arg_port = 0;
static int command_threaded = 0;
static int command_log = 0;
static char aux_log[100];
static char *arg_host;
static char *arg_command;
static sock_t sock;

int send_message(sock_t sock, char *message) {
        strcat(message, "\r\n");
#ifdef __linux__
        if (write(sock, message, strlen(message)) < 0) {
#elif _WIN32
        if (send(sock, message, strlen(message), 0) < 0) {
#endif
                std_err("Unable to send message.");
                return -1;
        }
        return 0;
}

void close_socket(sock_t sock) {
        // sprintf(aux_log, "Closing socket %d.", sock);
        // std_out(aux_log);
        char *cmd = calloc(strlen(CMD_EXIT), sizeof(char));
        strcpy(cmd, CMD_EXIT);
        send_message(sock, cmd);
#ifdef __linux__
        close(sock);
#elif _WIN32
        closesocket(sock);
#endif
        if (cmd != NULL) {
                free(cmd);
        }
        return;
}

sock_t create_socket(char* dest, int port) {
        struct sockaddr_in server;
        struct hostent *host;
        sock_t sock;

        sock = socket(AF_INET, SOCK_STREAM, 0);
#ifdef __linux__
        if (sock == -1) {
                sprintf(aux_log, "Unable to create socket: %d.", errno);
#elif _WIN32
        if (sock == INVALID_SOCKET) {
                sprintf(aux_log, "Unable to create socket: %d.", WSAGetLastError());
#endif
                std_err(aux_log);
                exit(EXIT_FAILURE);
        }

        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        host = gethostbyname(dest);
        if (host == NULL) {
#ifdef __linux__
                sprintf(aux_log, "Failed on gethostbyname: %s.", hstrerror(h_errno));
#elif _WIN32
                sprintf(aux_log, "Failed on gethostbyname: %d.", WSAGetLastError());
#endif
                std_err(aux_log);
                exit(EXIT_FAILURE);
        }
        memcpy(&server.sin_addr, host->h_addr, host->h_length);

        if (connect(sock, (struct sockaddr*) &server, sizeof(server)) < 0) {
                std_err("Failed on connecting to server socket.");
                return -1;
        }

        return sock;
}

void command_fire(sock_t sock) {
        int status = send_message(sock, arg_command);
        if (status != 0) {
                return;
        }

        if (command_log) {
                FILE *log_file = fopen(LOG_FILE, "a");
                if (log_file == NULL) {
                        sprintf(aux_log, "Unable to open log %s.", LOG_FILE);
                        std_err(aux_log);
                        exit(EXIT_FAILURE);
                }
                fprintf(log_file, "%s", arg_command);
                fflush(log_file);
                fclose(log_file);
        }

        int server_reply_size = 0;
        char server_reply[5000];
        for (;; ) {
                server_reply_size = recv(sock, server_reply, 5000, 0);
#ifdef __linux__
                if (server_reply_size == -1) {
#elif _WIN32
                if (server_reply_size == SOCKET_ERROR) {
#endif
                        std_err("Unable to read message.");

                }
                server_reply[server_reply_size] = '\0';
                printf("%s", server_reply);

                //const char *suffix = &server_reply[strlen(server_reply)-5];
                //if (strcasecmp(suffix, "\r\n.\r\n") == 0) {
                if (strstr(server_reply, "\r\n.\r\n")) {
                        break;
                }
        }
}

int main(int argc, char *argv[]) {
        if (argc < 6) {
                std_err("At least a host, a port and a command have to be specified.");
                exit(EXIT_FAILURE);
        }

        for (int i = 0; i < argc; i++) {
                if (strcasecmp(argv[i], "-l") == 0 || strcasecmp(argv[i], "-R") == 0) {
                        arg_command = calloc(strlen(CMD_LSTF), sizeof(char));
                        strcpy(arg_command, (strcasecmp(argv[i], "-l") == 0) ? CMD_LSTF : CMD_LSTR);
                        continue;
                }
                if (strcasecmp(argv[i], "-e") == 0 || strcasecmp(argv[i], "-d") == 0) {
                        command_threaded = 1;
                        command_log = 1;
                        if ((i+2) < argc) {
                                arg_command = calloc(strlen(CMD_ENCR) + 1 + strlen(argv[i+1]) + 2 + strlen(argv[i+2]), sizeof(char));
                                sprintf(arg_command, "%s %s %s", (strcasecmp(argv[i], "-e") == 0) ? CMD_ENCR : CMD_DECR, argv[i+1], argv[i+2]);
                                arg_command[strlen(arg_command)] = 0;
                        } else {
                                sprintf(aux_log, "Malformed %s input.", argv[i]);
                                std_err(aux_log);
                                exit(EXIT_FAILURE);
                        }
                        continue;
                }
                if (strcasecmp(argv[i], "-h") == 0) {
                        if ((i+1) < argc) {
                                arg_host = calloc(strlen(argv[i+1]), sizeof(char));
                                strcpy(arg_host, argv[i+1]);
                        } else {
                                sprintf(aux_log, "Malformed -h input.");
                                std_err(aux_log);
                                exit(EXIT_FAILURE);
                        }
                        continue;
                }
                if (strcasecmp(argv[i], "-p") == 0) {
                        if ((i+1) < argc) {
                                if (!isdigit(*argv[i+1])) {
                                        sprintf(aux_log, "'-p' input must be an integer parseable value (passed: \"%s\").", argv[i+1]);
                                        std_err(aux_log);
                                        exit(EXIT_FAILURE);
                                }
                                arg_port = atoi(argv[i+1]);
                        } else {
                                sprintf(aux_log, "Malformed -p input.");
                                std_err(aux_log);
                                exit(EXIT_FAILURE);
                        }
                        continue;
                }
        }

        if (arg_host == NULL) {
                std_err("No host server has been specified. Exiting.");
                exit(EXIT_FAILURE);
        }

        if (arg_port == 0) {
                std_err("No host server port has been specified. Exiting.");
                exit(EXIT_FAILURE);
        }

        if (arg_command == NULL) {
                std_err("No command has been specified. Exiting.");
                exit(EXIT_FAILURE);
        }

#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
                sprintf(aux_log, "WSA initialization failed. Code: %d.", WSAGetLastError());
                std_err(aux_log);
                exit(EXIT_FAILURE);
        }
#endif

        sock = create_socket(arg_host, 8888);
        if (sock == -1) {
                std_err("Something went wrong while instanciating socket. Exiting.");
                exit(EXIT_FAILURE);
        }

        int status = 0;
        if (command_threaded == 1) {
#ifdef __linux__
                pthread_t thread;
                int thread_ret = pthread_create(&thread, NULL, (void *) command_fire, (void *)(intptr_t) sock);
                if (thread_ret) {
                        sprintf(aux_log, "Unable to create thread: %d", thread_ret);
                        std_err(aux_log);
                        status = 1;
                }
                pthread_join(thread, NULL);
#elif _WIN32
                DWORD t_id;
                HANDLE thread = CreateThread(NULL, 0, (void *) command_fire, (void *)(intptr_t) sock, 0, &t_id);
                if (thread == INVALID_HANDLE_VALUE) {
                        sprintf(aux_log, "Unable to create thread: %d.", thread);
                        std_err(aux_log);
                        status = 1;
                } else {
                        WaitForSingleObject(thread, INFINITE);
                }
#endif
        } else {
                command_fire(sock);
        }

        close_socket(sock);
#ifdef _WIN32
        WSACleanup();
#endif

        if (status) {
                exit(EXIT_FAILURE);
        } else {
                exit(EXIT_SUCCESS);
        }
}
