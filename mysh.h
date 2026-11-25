#ifndef MYSH_H
#define MYSH_H

#include <sys/types.h>
#include <stddef.h>

#define LINE 1024
#define MAXTOK 128
#define MAXJOBS 64
#define HIST 20

#define RED "\033[1;31m"
#define GRN "\033[1;32m"
#define BLU "\033[1;34m"
#define YEL "\033[1;33m"
#define CYN "\033[1;36m"
#define RST "\033[0m"

typedef struct {
    char *argv[MAXTOK];
    int argc;
    char *in;
    char *out;
    int append;
} Cmd;

typedef struct {
    pid_t pid;
    char cmd[LINE];
} Job;

extern int running;
extern char helper[4200];

void copy_str(char *dst, size_t n, const char *src);
int tokenize(const char *s, char *o, char *tok[], int isop[]);
void add_job(pid_t pid, const char *cmd);
void reap(void);
void add_hist(const char *s);
int handle_builtin(char **a);
void run_line(const char *line);
void ai_mode(char *req);

#endif