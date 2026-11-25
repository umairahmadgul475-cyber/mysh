#include "mysh.h"

typedef struct {
    pthread_mutex_t lock;
    int active;
} Spinner;

static void *spinner_func(void *arg) {
    Spinner *sp = (Spinner *)arg;
    const char frames[] = "|/-\\";
    struct timespec ts = {0, 100000000L};
    int idx = 0;

    for (;;) {
        pthread_mutex_lock(&sp->lock);
        int go = sp->active;
        pthread_mutex_unlock(&sp->lock);
        if (!go) break;

        printf("\r" CYN "[mysh AI thinking %c]" RST " ", frames[idx % 4]);
        fflush(stdout);
        idx++;
        nanosleep(&ts, NULL);
    }

    printf("\r                        \r");
    fflush(stdout);
    return NULL;
}

static int run_helper(const char *flag, const char *arg, char *out, size_t n) {
    int p[2], st = 0;
    size_t len = 0;
    ssize_t r;

    if (pipe(p) < 0) {
        perror("pipe");
        return 0;
    }

    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(p[0]);
        close(p[1]);
        return 0;
    }

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        close(p[0]);
        dup2(p[1], STDOUT_FILENO);
        close(p[1]);
        if (flag) {
            char *a[] = {"python3", helper_path, (char *)flag, (char *)arg, NULL};
            execvp(a[0], a);
        } else {
            char *a[] = {"python3", helper_path, (char *)arg, NULL};
            execvp(a[0], a);
        }
        perror("python3");
        _exit(1);
    }

    close(p[1]);

    Spinner sp;
    pthread_mutex_init(&sp.lock, NULL);
    sp.active = 1;
    pthread_t tid;
    int started = (pthread_create(&tid, NULL, spinner_func, &sp) == 0);

    while (len < n - 1 && (r = read(p[0], out + len, n - 1 - len)) > 0) {
        len += (size_t)r;
    }
    close(p[0]);
    waitpid(pid, &st, 0);

    if (started) {
        pthread_mutex_lock(&sp.lock);
        sp.active = 0;
        pthread_mutex_unlock(&sp.lock);
        pthread_join(tid, NULL);
    }
    pthread_mutex_destroy(&sp.lock);

    out[len] = '\0';
    while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r')) {
        out[--len] = '\0';
    }
    return WIFEXITED(st) && WEXITSTATUS(st) == 0 && out[0] != '\0';
}

static int is_risky(const char *c) {
    static const char *bad[] = {
        "rm", "sudo", "dd", "mkfs", "chmod", "chown", "shutdown", "reboot"
    };
    if (strstr(c, ":(){")) return 1;

    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        size_t L = strlen(bad[i]);
        for (const char *p = c; (p = strstr(p, bad[i])) != NULL; p++) {
            int before = (p == c) || !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
            int after = !(isalnum((unsigned char)p[L]) || p[L] == '_');
            if (before && after) return 1;
        }
    }
    return 0;
}

void ai_explain(char **argv) {
    if (!argv[1]) {
        printf("Usage: explain <linux command>\n");
        return;
    }
    char target[LINE] = "";
    for (int i = 1; argv[i]; i++) {
        if (i > 1 && strlen(target) + 1 < sizeof target) strcat(target, " ");
        if (strlen(target) + strlen(argv[i]) + 1 < sizeof target) strcat(target, argv[i]);
    }
    char resp[4096];
    if (!run_helper("--explain", target, resp, sizeof resp)) {
        fprintf(stderr, "mysh: AI helper failed\n");
        return;
    }
    printf(CYN "[mysh Explain]" RST " %s\n", resp);
}

void ai_mode(const char *req) {
    char resp[4096];
    while (isspace((unsigned char)*req)) req++;
    if (!*req) {
        printf("usage: ? <command in English or any question>\n");
        return;
    }

    if (!run_helper(NULL, req, resp, sizeof resp)) {
        fprintf(stderr, "mysh: AI helper failed (is python3 installed?)\n");
        return;
    }

    if (!strncmp(resp, "CHAT:", 5)) {
        char *msg = resp + 5;
        while (*msg == ' ') msg++;
        printf(CYN "[mysh AI]" RST " %s\n", msg);
        return;
    }

    char *cmd = !strncmp(resp, "CMD:", 4) ? resp + 4 : resp;
    while (*cmd == ' ') cmd++;
    cmd[strcspn(cmd, "\n")] = '\0';
    if (!*cmd) return;
    if (strlen(cmd) >= LINE) cmd[LINE - 1] = '\0';

    if (is_risky(cmd)) {
        printf(RED "[WARNING: Potentially destructive command detected!]" RST "\n");
    }
    printf(YEL "[AI Suggested]" RST " -> %s\n", cmd);

    if (ask_yes_no("Run this command? (y/n): ")) {
        run_chain(cmd);
    } else {
        printf("Command cancelled.\n");
    }
}