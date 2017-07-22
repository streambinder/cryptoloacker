#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "client.h"
#include "opt.h"

int arg_port = 0;
int command_threaded = 0;
int command_log = 0;
int sock;

char aux_log[100];
char *arg_host;
char *arg_command;

FILE *log_file;

int send_message(int sock, char *message) {
        strcat(message, "\r\n");
        if (write(sock, message, strlen(message)) < 0) {
                std_err("Unable to send message.");
                return -1;
        }
        return 0;
}

void close_socket(int sock) {
        // sprintf(aux_log, "Closing socket %d.", sock);
        // std_out(aux_log);
        char *cmd = calloc(strlen(CMD_EXIT), sizeof(char));
        strcpy(cmd, CMD_EXIT);
        send_message(sock, cmd);
        close(sock);
        free(cmd);
        return;
}

int create_socket(char* dest, int port) {
        struct sockaddr_in temp;
        struct hostent *h;
        int sock;
        int error;

        temp.sin_family = AF_INET;
        temp.sin_port = htons(port);
        h = gethostbyname(dest);
        if (h == 0) {
                std_err("Failed on gethostbyname.");
                exit(EXIT_FAILURE);
        }

        bcopy(h->h_addr, &temp.sin_addr, h->h_length);
        sock = socket(AF_INET, SOCK_STREAM, 0);

        error = connect(sock, (struct sockaddr*) &temp, sizeof(temp));
        if (error != 0) {
                return -1;
        }

        return sock;
}

int command_fire(int sock) {
        int status = send_message(sock, arg_command);
        if (status != 0) {
                return status;
        }

        if (log_file != NULL && command_log) {
                fprintf(log_file, "%s", arg_command);
        }

        char server_reply[5000];
        for (;; ) {
                recv(sock, server_reply, 5000, 0);
                printf("%s", server_reply);

                const char *suffix = &server_reply[strlen(server_reply)-5];
                if (strcasecmp(suffix, "\r\n.\r\n") == 0) {
                        break;
                }
        }
        return 0;
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
                                arg_command = calloc(strlen(CMD_ENCR) + 1 + strlen(argv[i+1]) + strlen(argv[i+2]), sizeof(char));
                                sprintf(arg_command, "%s %s %s", (strcasecmp(argv[i], "-e") == 0) ? CMD_ENCR : CMD_DECR, argv[i+1], argv[i+2]);
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

        // sprintf(aux_log, "Config { host=\"%s\", port=%d, command=\"%s\" }", arg_host, arg_port, arg_command);
        // std_out(aux_log);

        if (command_log) {
                log_file = fopen(LOG_FILE, "a");
                if (log_file == NULL) {
                        sprintf(aux_log, "Unable to open log %s.", LOG_FILE);
                        std_err(aux_log);
                        exit(EXIT_FAILURE);
                } else {
                        // sprintf(aux_log, "Logging to %s.", LOG_FILE);
                        // std_out(aux_log);
                }
        }

        sock = create_socket(arg_host, 8888);
        if (sock == -1) {
                std_err("Something went wrong while instanciating socket. Exiting.");
                exit(EXIT_FAILURE);
        }

        int status = 0;
        if (command_threaded == 1) {
                pthread_t thread;
                int thread_ret = pthread_create(&thread, NULL, (void *) command_fire, (void *)(intptr_t) sock);
                if (thread_ret) {
                        sprintf(aux_log, "Unable to create pthread: return code %d", thread_ret);
                        std_err(aux_log);
                        status = 1;
                }
                pthread_join(thread, NULL);
        } else {
                status = command_fire(sock);
                if (status != 0) {
                        std_err("Something went wrong while firing command request over socket.");
                }
        }

        close_socket(sock);

        if (log_file != NULL) {
                fclose(log_file);
        }
        if (arg_host != NULL) {
                free(arg_host);
        }
        if (arg_command != NULL) {
                free(arg_command);
        }

        if (status) {
                exit(EXIT_FAILURE);
        } else {
                exit(EXIT_SUCCESS);
        }
}
