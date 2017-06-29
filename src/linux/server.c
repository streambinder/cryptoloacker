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

#define DEFAULT_PORT 8888

char tmp_log_string[100];

void printer(char message[]) {
        printf("SERVER: %s\n", message);
}

int create_socket(int port) {
        printer("Allocating variables.");
        int sock, error;
        struct sockaddr_in sock_conf;

        sprintf(tmp_log_string, "Initializing socket on 0.0.0.0:%d", port);
        printer(tmp_log_string);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        sock_conf.sin_family = AF_INET;
        sock_conf.sin_addr.s_addr = INADDR_ANY;
        sock_conf.sin_port = htons(port);

        error = fcntl(sock, F_SETFL, O_NONBLOCK);

        printer("Binding socket.");
        error = bind(sock,(struct sockaddr*) &sock_conf, sizeof(sock_conf));

        printer("Listening for connection.");
        error = listen(sock, 10);

        return sock;
}

void close_socket(int sock) {
        printer("Closing socket.");
        close(sock);
        return;
}

void handle_connection(int sock) {
        char buffer[512+1];
        int packet_length;
        buffer[512] = "\0";
        for (;; ) {
                if ((packet_length = read(sock, buffer, 512)) < 0) {
                        printer("Unable to read message.");
                } else {
                        for (int index = packet_length-2; index < 512; index++) {
                                buffer[index] = 0;
                        }

                        sprintf(tmp_log_string, "Message received (%d): %s", packet_length, buffer);
                        printer(tmp_log_string);

                        if (strcmp(buffer, "exit") == 0) {
                                printer("Exit asked. Quitting.");
                                close_socket(sock);
                                break;
                        }
                        for (int index = 0; index < 512; index++) {
                                buffer[index] = 0;
                        }
                }
        }
}

int main(int argc, char *argv[]) {
        char *arg_folder;
        int arg_port = 0;
        int opt = 0;

        while ((opt = getopt(argc, argv, "c:p:")) != -1) {
                switch(opt) {
                case 'c':
                        arg_folder = optarg;
                        break;
                case 'p':
                        if (!isdigit(*optarg)) {
                                printer("'-p' input must be an integer parseable value.");
                                exit(EXIT_FAILURE);
                        }
                        arg_port = atoi(optarg);
                        break;
                case '?':
                        if (optopt == 'c' || optopt == 'p') {
                                sprintf(tmp_log_string, "Missing '-%c' mandatory input.", optopt);
                                printer(tmp_log_string);
                        } else {
                                printer("Invalid option received.");
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
                printer("'-c' input must be an existing filesystem path.");
                exit(EXIT_FAILURE);
        }

        if (arg_port == 0) {
                arg_port = DEFAULT_PORT;
        }
        sprintf(tmp_log_string, "Config => port=%d, folder=%s", arg_port, arg_folder);
        printer(tmp_log_string);

        int sock_descriptor, sock_new;
        int exit_condition = 0;

        sock_descriptor = create_socket(arg_port);

        printer("Attendo connessioni.");
        for (;; ) {
                if ((sock_new = accept(sock_descriptor, 0, 0)) != -1) {
                        handle_connection(sock_new);
                }
        }

        close_socket(sock_new);
        close_socket(sock_descriptor);
        printer("Terminato.");

        return 0;
}
