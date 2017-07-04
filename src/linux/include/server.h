#ifndef SERVER_H
#define SERVER_H

#define NAME "CRYPTOLOACKER"

#define CMD_EXIT "EXIT"
#define CMD_LSTF "LSTF"
#define CMD_ENCR "ENCR"
#define CMD_DECR "DECR"

#define CFG_DEFAULT_PORT    8888
#define CFG_DEFAULT_THREADS 4

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GREY    "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define std_out(arg) printf(ANSI_COLOR_GREEN "%s" ANSI_COLOR_GREY " > " ANSI_COLOR_RESET "%s\n", NAME, arg)
#define std_err(arg) printf(stderr, ANSI_COLOR_RED "%s" ANSI_COLOR_GREY " > " ANSI_COLOR_RESET "%s\n", NAME, arg)

#endif // SERVER_H
