#ifndef SERVER_H
#define SERVER_H

#define CFG_DEFAULT_PORT    8888
#define CFG_DEFAULT_THREADS 4

#define LST_F 0
#define LST_R 1

#define TAB_SIZE 8

int inherit_configuration(char *config_file);
void sig_hup_handler(int sig);
void sig_int_handler(int sig);
void list_opt(char *ret_out, int recursive, char *folder, char *folder_suffix);
void list(char *ret_out, int recursive);
int create_socket(int port);
void close_socket(int sock);
int write_socket(int sock, char *message, int last);
void handle_connection(int sock);

#endif // SERVER_H
