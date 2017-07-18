#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "opt.h"

int sock;

void close_socket(int sock) {
        close(sock);
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

        // recv timeout set
        struct timeval val;
        val.tv_sec = 1; // second(s)
        val.tv_usec = 0;
        int enable = 1;
        if (setsockopt(sock, SOL_SOCKET,
                       SO_RCVTIMEO,(struct timeval *)&val,
                       sizeof(struct timeval)) < 0) {
                std_err("setsockopt(SO_RCVTIMEO) failed.");
                exit(EXIT_FAILURE);
        }

        error = connect(sock, (struct sockaddr*) &temp, sizeof(temp));
        return sock;
}

void send_message(int sock, char* message) {
        strcat(message, "\r\n");
        if (write(sock, message, strlen(message)) < 0) {
                std_err("Unable to send message.");
                close_socket(sock);
                exit(EXIT_FAILURE);
        }
        return;
}

int main(int argc,char* argv[]) {
        char server_reply[5000];
        sock = create_socket("127.0.0.1", 8888);

        if (argc == 2) {
                send_message(sock, argv[1]);
                while (recv(sock, server_reply, 5000, 0) > 0) {
                        printf("%s\n", server_reply);
                }
        }

        close_socket(sock);
        return 0;
}
