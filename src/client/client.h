#ifndef CLIENT_H
#define CLIENT_H

#define LOG_FILE "cryptoloacker.log"

int send_message(int sock, char *message);
void close_socket(int sock);
int create_socket(char* dest, int port);
int command_fire(int sock);

#endif // CLIENT_H
