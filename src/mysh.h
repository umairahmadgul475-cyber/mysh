#ifndef MYSH_H
#define MYSH_H

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#define LINE 1024
#define MAXTOK 128
#define MAXCMDS 16
#define MAXJOBS 64
#define HIST 100

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
extern char helper_path[4200];
extern char prompt_color[32];

void copy_str(char *dst, size_t n, const char *src);
int ask_yes_no(const char *prompt);
int tokenize(const char *s, char *out, char *tok[], int isop[]);
int run_chain(const char *line);
int run_pipeline(const char *segment);
void add_job(pid_t pid, const char *cmd);
void reap_jobs(void);
int run_builtin(char **argv);
void print_banner(void);
void ai_mode(const char *req);
void ai_explain(char **argv);

#endif