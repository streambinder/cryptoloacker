#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "server.h"
#include "threadpool.h"

threadpool thpool;
char aux_log[100];

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
        }

        sock_conf.sin_family = AF_INET;
        sock_conf.sin_addr.s_addr = INADDR_ANY;
        sock_conf.sin_port = htons(port);

        status = fcntl(sock, F_SETFL, O_NONBLOCK);
        if (status != 0) {
                sprintf(aux_log, "Error %d manipulating socket file descriptor.", status);
                std_err(aux_log);
        }

        std_out("Binding socket.");
        status = bind(sock,(struct sockaddr*) &sock_conf, sizeof(sock_conf));
        if (status != 0) {
                sprintf(aux_log, "Error %d binding socket.", status);
                std_err(aux_log);
        }

        std_out("Listening for connection.");
        status = listen(sock, 10);
        if (status != 0) {
                sprintf(aux_log, "Error %d making socket listening.", status);
                std_err(aux_log);
        }

        return sock;
}

void close_socket(int sock) {
        std_out("Closing socket.");
        close(sock);
        return;
}

void handle_connection(int sock) {
        char buffer[512+1];
        int packet_length;
        buffer[512] = "\0";
        for (;; ) {
                if ((packet_length = read(sock, buffer, 512)) < 0) {
                        std_out("Unable to read message.");
                } else {
                        // clean buffer (not on packet)
                        for (int index = packet_length-2; index < 512; index++) {
                                buffer[index] = 0;
                        }

                        sprintf(aux_log, "Message received (%d): %s", packet_length, buffer);
                        std_out(aux_log);

                        // trap command a reduce it to lowercase
                        char *command = strtok(buffer, " ");
                        for(int i = 0; command[i]; i++) {
                                command[i] = tolower(command[i]);
                        }
                        sprintf(aux_log, "Trapped %s command.", command);
                        std_out(aux_log);

                        if (strcasecmp(command, CMD_EXIT) == 0) {
                                std_out("Exit asked. Quitting.");
                                close_socket(sock);
                                break;
                        }

                        // buffer cleanup
                        for (int index = 0; index < 512; index++) {
                                buffer[index] = 0;
                        }
                }
        }
}

int main(int argc, char *argv[]) {
        char *arg_folder;
        int arg_port = 0;
        int arg_threads = 0;
        int opt = 0;

        while ((opt = getopt(argc, argv, "c:p:n:")) != -1) {
                switch(opt) {
                case 'c':
                        arg_folder = optarg;
                        break;
                case 'n':
                        arg_threads = optarg;
                        break;
                case 'p':
                        if (!isdigit(*optarg)) {
                                std_out("'-p' input must be an integer parseable value.");
                                exit(EXIT_FAILURE);
                        }
                        arg_port = atoi(optarg);
                        break;
                case '?':
                        if (optopt == 'c' || optopt == 'p') {
                                sprintf(aux_log, "Missing '-%c' mandatory input.", optopt);
                                std_out(aux_log);
                        } else {
                                std_out("Invalid option received.");
                        }
                        exit(EXIT_FAILURE);
                        break;
                }
        }

        int arg_folder_exist = 1;
        struct stat s;
        int err = stat(arg_folder, &s);
        if(err == -1) {
                if(errno == ENOENT) {
                        arg_folder_exist = 0;
                }
        } else {
                if(!S_ISDIR(s.st_mode)) {
                        arg_folder_exist = 0;
                }
        }
        if (!arg_folder_exist) {
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

        int sock_descriptor, sock_new;
        int exit_condition = 0;

        sock_descriptor = create_socket(arg_port);

        std_out("Waiting for connections.");
        for (;; ) {
                if ((sock_new = accept(sock_descriptor, 0, 0)) != -1) {
                        handle_connection(sock_new);
                }
        }

        close_socket(sock_new);
        close_socket(sock_descriptor);
        std_out("Exited.");

        return 0;
}
