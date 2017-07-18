#ifndef COMMON_H
#define COMMON_H

#define NAME "CRYPTOLOACKER"

#define RTRN_CPH_OK 200
#define RTRN_LST_OK 300
#define RTRN_TRNS_NOK 400
#define RTRN_NOK 500

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GREY    "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define std_out(arg) fprintf(stdout, ANSI_COLOR_GREEN "%s" ANSI_COLOR_GREY " > " ANSI_COLOR_RESET "%s\n", NAME, arg)
#define std_err(arg) fprintf(stderr, ANSI_COLOR_RED "%s" ANSI_COLOR_GREY " > " ANSI_COLOR_RESET "%s\n", NAME, arg)

#endif // COMMON_H
