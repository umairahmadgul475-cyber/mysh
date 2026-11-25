#include "mysh.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void child_exec(Cmd *c, int in, int out, int *pp) {
    signal(SIGINT, SIG_DFL);
    if (in >= 0) dup2(in, STDIN_FILENO);
    if (out >= 0) dup2(out, STDOUT_FILENO);
    if (pp[0] >= 0) { close(pp[0]); close(pp[1]); }
    if (c->in) {
        int fd = open(c->in, O_RDONLY);
        if (fd < 0) { perror(c->in); _exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (c->out) {
        int fd = open(c->out, O_WRONLY | O_CREAT | (c->append ? O_APPEND : O_TRUNC), 0644);
        if (fd < 0) { perror(c->out); _exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    execvp(c->argv[0], c->argv);
    perror(c->argv[0]);
    _exit(1);
}

void run_line(const char *line) {
    char buf[2 * LINE + 2];
    char *tok[MAXTOK];
    int isop[MAXTOK];
    Cmd cmd[2];
    int nc = 1, bg = 0;
    memset(cmd, 0, sizeof cmd);
    int n = tokenize(line, buf, tok, isop);
    for (int i = 0; i < n; i++) {
        Cmd *c = &cmd[nc - 1];
        if (!isop[i]) {
            if (c->argc < MAXTOK - 1) c->argv[c->argc++] = tok[i];
            continue;
        }
        char o = tok[i][0];
        if (o == '|') {
            if (nc == 2 || c->argc == 0) { fprintf(stderr, "mysh: syntax error near '|'\n"); return; }
            nc = 2;
        } else if (o == '&') {
            if (i != n - 1) { fprintf(stderr, "mysh: '&' must be last\n"); return; }
We'll upgrade `mysh` cleanly in 4 short steps so nothing breaks and you never have to paste giant blocks at once:

1. **Step 1 (Right now):** Back up your current working files, create a `src/` folder, update `Makefile`, and create the shared header `src/mysh.h`.
2. **Step 2:** Write `src/parser.c` and `src/executor.c` (adds multi-pipe like `a | b | c` and `&&` / `;` command chaining).
3. **Step 3:** Write `src/builtins.c` (adds `sysinfo` reading live Linux `/proc` kernel data, the Avengers `theme` command, and a startup banner).
4. **Step 4:** Write `src/ai_bridge.c` (adds a real `pthread` loading spinner while waiting for Gemini) and `src/main.c`, then compile and test everything.

## Step 1: Backup, Folder Setup, and Header File

Make sure you're at your **normal Debian prompt** (`umairahmad@debain`), not inside `[mysh]`. If you still see `[mysh]`, type `exit` first.

1. **Back up your working prototype and create `src/`** (this keeps your old working version safe inside `backup/` no matter what):
```bash
cd ~/mysh
mkdir -p backup src
cp mysh.c ai_helper.py Makefile backup/