#include "mysh.h"

int running = 1;
char helper_path[4200] = "ai_helper.py";
char prompt_color[32] = GRN;

static volatile sig_atomic_t child_exited = 0;

static void sigchld_handler(int sig) {
    (void)sig;
    child_exited = 1;
}

static void setup_signals(void) {
    signal(SIGINT, SIG_IGN);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

int main(void) {
    char cwd[4096], prompt[4300], hist_path[4200] = "";
    const char *home = getenv("HOME");

    setup_signals();
    if (getcwd(cwd, sizeof cwd)) {
        snprintf(helper_path, sizeof helper_path, "%s/ai_helper.py", cwd);
    }

    using_history();
    stifle_history(HIST);
    if (home) {
        snprintf(hist_path, sizeof hist_path, "%s/.mysh_history", home);
        read_history(hist_path);
    }

    print_banner();

    while (running) {
        if (child_exited) {
            child_exited = 0;
            reap_jobs();
        }

        if (!getcwd(cwd, sizeof cwd)) copy_str(cwd, sizeof cwd, "?");
        snprintf(prompt, sizeof prompt, "\001%s\002[mysh]\001" RST "\002 \001" BLU "\002%s\001" RST "\002$ ",
                 prompt_color, cwd);

        char *raw = readline(prompt);
        if (!raw) {
            putchar('\n');
            break;
        }

        if (child_exited) {
            child_exited = 0;
            reap_jobs();
        }

        char *s = raw;
        while (isspace((unsigned char)*s)) s++;
        if (!*s) {
            free(raw);
            continue;
        }

        char expanded[LINE];
        if (!strcmp(s, "!!")) {
            if (history_length == 0) {
                printf("mysh: no previous command\n");
                free(raw);
                continue;
            }
            HIST_ENTRY *prev = history_get(history_base + history_length - 1);
            if (!prev) {
                free(raw);
                continue;
            }
            copy_str(expanded, sizeof expanded, prev->line);
            printf("%s\n", expanded);
            s = expanded;
        }

        add_history(s);

        if (*s == '?') {
            ai_mode(s + 1);
        } else {
            int status = run_chain(s);
            int is_multiword = (strchr(s, ' ') != NULL);
            if (running && (status == 127 || (status != 0 && is_multiword))) {
                if (ask_yes_no(YEL "[mysh AI]" RST " That didn't run cleanly. Ask AI to translate or fix it? (y/n): ")) {
                    ai_mode(s);
                }
            }
        }

        free(raw);
    }

    if (hist_path[0]) {
        write_history(hist_path);
    }
    return 0;
}