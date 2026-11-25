#include "mysh.h"

static void apply_redirs(Cmd *c) {
    if (c->in) {
        int fd = open(c->in, O_RDONLY);
        if (fd < 0) { perror(c->in); _exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (c->out) {
        int flags = O_WRONLY | O_CREAT | (c->append ? O_APPEND : O_TRUNC);
        int fd = open(c->out, flags, 0644);
        if (fd < 0) { perror(c->out); _exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

int run_pipeline(const char *segment) {
    char buf[2 * LINE + 2];
    char *tok[MAXTOK];
    int isop[MAXTOK];
    Cmd cmds[MAXCMDS];
    int nc = 1, bg = 0;

    memset(cmds, 0, sizeof cmds);
    int n = tokenize(segment, buf, tok, isop);
    if (n == 0) return 0;

    for (int i = 0; i < n; i++) {
        Cmd *c = &cmds[nc - 1];
        if (!isop[i]) {
            if (c->argc < MAXTOK - 1) c->argv[c->argc++] = tok[i];
            continue;
        }
        char o = tok[i][0];
        if (o == '|') {
            if (nc >= MAXCMDS || c->argc == 0) {
                fprintf(stderr, "mysh: syntax error near '|'\n");
                return 1;
            }
            nc++;
        } else if (o == '&') {
            if (i != n - 1) {
                fprintf(stderr, "mysh: '&' must be last\n");
                return 1;
            }
            bg = 1;
        } else {
            int app = (tok[i][1] == '>');
            if (i + 1 >= n || isop[i + 1]) {
                fprintf(stderr, "mysh: missing filename\n");
                return 1;
            }
            char *f = tok[++i];
            if (o == '<') c->in = f;
            else { c->out = f; c->append = app; }
        }
    }

    if (cmds[nc - 1].argc == 0) {
        fprintf(stderr, "mysh: syntax error\n");
        return 1;
    }

    if (nc == 1) {
        int b = run_builtin(cmds[0].argv);
        if (b != 0) return (b == 1) ? 0 : 1;
    }

    int pipes[MAXCMDS - 1][2];
    for (int i = 0; i < nc - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            for (int j = 0; j < i; j++) { close(pipes[j][0]); close(pipes[j][1]); }
            return 1;
        }
    }

    struct timespec t0, t1;
    pid_t pids[MAXCMDS];
    for (int i = 0; i < nc; i++) pids[i] = -1;

    fflush(NULL);
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < nc; i++) {
        pids[i] = fork();
        if (pids[i] < 0) { perror("fork"); break; }
        if (pids[i] == 0) {
            signal(SIGINT, SIG_DFL);
            if (i > 0) dup2(pipes[i - 1][0], STDIN_FILENO);
            if (i < nc - 1) dup2(pipes[i][1], STDOUT_FILENO);
            for (int j = 0; j < nc - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            apply_redirs(&cmds[i]);
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror(cmds[i].argv[0]);
            _exit(127);
        }
    }

    for (int j = 0; j < nc - 1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    int last_code = 0;
    for (int i = 0; i < nc; i++) {
        if (pids[i] <= 0) continue;
        if (bg) {
            add_job(pids[i], segment);
            printf("[bg] pid %d\n", (int)pids[i]);
        } else {
            int st = 0;
            waitpid(pids[i], &st, 0);
            if (i == nc - 1) {
                last_code = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
            }
        }
    }

    if (!bg) {
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double elapsed = (double)(t1.tv_sec - t0.tv_sec) +
                         (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
        printf("(done in %.3fs)\n", elapsed);
    }
    return last_code;
}

int run_chain(const char *line) {
    char seg[LINE];
    int len = 0, in_s = 0, in_d = 0;
    int last_status = 0, prev_op = 0;

    for (size_t i = 0; ; i++) {
        char c = line[i];
        int op = -1;

        if (c == '\'' && !in_d) in_s = !in_s;
        else if (c == '"' && !in_s) in_d = !in_d;

        if (!in_s && !in_d) {
            if (c == '\0' || c == ';') op = 0;
            else if (c == '&' && line[i + 1] == '&') { op = 1; i++; }
            else if (c == '|' && line[i + 1] == '|') { op = 2; i++; }
        }

        if (op != -1) {
            seg[len] = '\0';
            int should_run = (prev_op == 0) ||
                             (prev_op == 1 && last_status == 0) ||
                             (prev_op == 2 && last_status != 0);
            if (should_run) {
                char *t = seg;
                while (isspace((unsigned char)*t)) t++;
                if (*t) last_status = run_pipeline(t);
            }
            if (!running) break;
            len = 0;
            prev_op = op;
            if (c == '\0') break;
        } else if (len < LINE - 1) {
            seg[len++] = c;
        }
    }
    return last_status;
}