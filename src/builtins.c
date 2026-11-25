#include "mysh.h"

static Job jobs[MAXJOBS];
static int njobs = 0;

void add_job(pid_t pid, const char *cmd) {
    if (njobs < MAXJOBS) {
        jobs[njobs].pid = pid;
        copy_str(jobs[njobs].cmd, LINE, cmd);
        njobs++;
    }
}

void reap_jobs(void) {
    pid_t p;
    while ((p = waitpid(-1, NULL, WNOHANG)) > 0) {
        for (int j = 0; j < njobs; j++) {
            if (jobs[j].pid == p) {
                printf("\n[done] %d  %s\n", (int)p, jobs[j].cmd);
                jobs[j] = jobs[--njobs];
                break;
            }
        }
    }
}

void print_banner(void) {
    printf(CYN
           "  __  __ __   __ ____  _   _ \n"
           " |  \\/  |\\ \\ / // ___|| | | |\n"
           " | |\\/| | \\ V / \\___ \\| |_| |\n"
           " | |  | |  | |   ___) |  _  |\n"
           " |_|  |_|  |_|  |____/|_| |_|\n" RST);
    printf(YEL " AI-Assisted Linux Mini Shell" RST " (OS Lab Project)\n");
    printf(" Type " GRN "help" RST ", " GRN "sysinfo" RST ", " GRN "procs" RST
           ", " GRN "theme ironman" RST ", " GRN "bg <pic>" RST ", " GRN "chat" RST
           ", or " GRN "? <english>" RST "\n\n");
}

static void set_theme(const char *bg, const char *fg, const char *pcol) {
    printf("\033]11;%s\007\033]10;%s\007", bg, fg);
    fflush(stdout);
    copy_str(prompt_color, sizeof prompt_color, pcol);
}

static int run_xfconf(const char *prop, const char *type, const char *val) {
    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        char *args[] = {
            "xfconf-query", "-c", "xfce4-terminal",
            "-p", (char *)prop, "-n", "-t", (char *)type, "-s", (char *)val, NULL
        };
        execvp(args[0], args);
        perror("xfconf-query");
        _exit(127);
    }
    int st = 0;
    waitpid(pid, &st, 0);
    return (WIFEXITED(st) && WEXITSTATUS(st) == 0) ? 0 : -1;
}

static int handle_bg(char **a) {
    if (!a[1]) {
        printf("Usage:\n"
               "  bg <image_path>    set terminal background image (jpg/png)\n"
               "  bg shade <0.0-1.0> adjust darkness (higher = darker, e.g. 0.85)\n"
               "  bg reset           switch back to solid background\n");
        return 1;
    }
    if (!strcmp(a[1], "reset") || !strcmp(a[1], "off")) {
        if (run_xfconf("/background-mode", "string", "TERMINAL_BACKGROUND_SOLID") < 0) {
            fprintf(stderr, "bg: failed to reset background via xfconf-query\n");
            return -1;
        }
        printf("Background reset to solid color.\n");
        return 1;
    }
    if (!strcmp(a[1], "shade")) {
        if (!a[2]) {
            printf("Usage: bg shade <0.0 to 1.0> (higher = darker)\n");
            return -1;
        }
        char *end = NULL;
        double val = strtod(a[2], &end);
        if (end == a[2] || *end != '\0' || val < 0.0 || val > 1.0) {
            printf("Shade must be a number between 0.0 and 1.0 (e.g. 0.80)\n");
            return -1;
        }
        char sbuf[32];
        snprintf(sbuf, sizeof sbuf, "%.2f", val);
        if (run_xfconf("/background-image-shading", "double", sbuf) < 0) {
            fprintf(stderr, "bg: failed to update shading\n");
            return -1;
        }
        printf("Background shading set to %s\n", sbuf);
        return 1;
    }

    char abs_path[4096];
    if (a[1][0] == '/') {
        copy_str(abs_path, sizeof abs_path, a[1]);
    } else if (a[1][0] == '~' && (a[1][1] == '/' || a[1][1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "bg: HOME not set\n");
            return -1;
        }
        snprintf(abs_path, sizeof abs_path, "%s%s", home, a[1] + 1);
    } else {
        char cwd[3000];
        if (!getcwd(cwd, sizeof cwd)) {
            perror("getcwd");
            return -1;
        }
        snprintf(abs_path, sizeof abs_path, "%s/%s", cwd, a[1]);
    }

    if (access(abs_path, R_OK) != 0) {
        perror(abs_path);
        return -1;
    }

    if (run_xfconf("/background-image-file", "string", abs_path) < 0 ||
        run_xfconf("/background-image-style", "string", "TERMINAL_BACKGROUND_STYLE_SCALED") < 0 ||
        run_xfconf("/background-image-shading", "double", "0.80") < 0 ||
        run_xfconf("/background-mode", "string", "TERMINAL_BACKGROUND_IMAGE") < 0) {
        fprintf(stderr, "bg: failed to update xfce4-terminal settings via xfconf-query\n");
        return -1;
    }

    printf("Background image set to %s (shading 0.80)\n", abs_path);
    return 1;
}

static void show_sysinfo(void) {
    FILE *f;
    char buf[256];
    char cpu[128] = "Unknown";
    int cores = 0;
    long mem_total = 0, mem_avail = 0;
    double up = 0.0;

    f = fopen("/proc/cpuinfo", "r");
    if (f) {
        while (fgets(buf, sizeof buf, f)) {
            if (!strncmp(buf, "processor", 9)) cores++;
            if (!strncmp(buf, "model name", 10) && !strcmp(cpu, "Unknown")) {
                char *col = strchr(buf, ':');
                if (col) {
                    col++;
                    while (*col == ' ') col++;
                    col[strcspn(col, "\n")] = '\0';
                    copy_str(cpu, sizeof cpu, col);
                }
            }
        }
        fclose(f);
    }

    f = fopen("/proc/meminfo", "r");
    if (f) {
        while (fgets(buf, sizeof buf, f)) {
            if (!strncmp(buf, "MemTotal:", 9)) {
                if (sscanf(buf + 9, "%ld", &mem_total) != 1) mem_total = 0;
            } else if (!strncmp(buf, "MemAvailable:", 13)) {
                if (sscanf(buf + 13, "%ld", &mem_avail) != 1) mem_avail = 0;
            }
        }
        fclose(f);
    }

    f = fopen("/proc/uptime", "r");
    if (f) {
        if (fscanf(f, "%lf", &up) != 1) up = 0.0;
        fclose(f);
    }

    int hrs = (int)(up / 3600);
    int mins = ((int)up % 3600) / 60;
    long used_mb = (mem_total >= mem_avail) ? (mem_total - mem_avail) / 1024 : 0;
    long total_mb = mem_total / 1024;

    printf(CYN "=== System Info (live from /proc) ===" RST "\n");
    printf("CPU    : %s (%d cores)\n", cpu, cores);
    printf("Memory : %ld MB used / %ld MB total\n", used_mb, total_mb);
    printf("Uptime : %d hr %d min\n", hrs, mins);
    printf("Shell  : mysh (PID %d)\n", (int)getpid());
}

static int is_digits(const char *s) {
    if (!*s) return 0;
    for (; *s; s++) {
        if (!isdigit((unsigned char)*s)) return 0;
    }
    return 1;
}

static void show_procs(void) {
    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("/proc");
        return;
    }

    printf(CYN "%-8s %-8s %-12s %s" RST "\n", "PID", "STATE", "MEM (KB)", "COMMAND");
    struct dirent *ent;
    int shown = 0;

    while ((ent = readdir(dir)) != NULL && shown < 25) {
        if (!is_digits(ent->d_name)) continue;

        char path[300];
        snprintf(path, sizeof path, "/proc/%s/status", ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;

        char line[256], name[64] = "?", state[32] = "?";
        long rss = 0;

        while (fgets(line, sizeof line, f)) {
            if (!strncmp(line, "Name:", 5)) {
                (void)sscanf(line + 5, "%63s", name);
            } else if (!strncmp(line, "State:", 6)) {
                (void)sscanf(line + 6, "%31s", state);
            } else if (!strncmp(line, "VmRSS:", 6)) {
                (void)sscanf(line + 6, "%ld", &rss);
            }
        }
        fclose(f);

        if (rss > 0) {
            printf("%-8s %-8s %-12ld %s\n", ent->d_name, state, rss, name);
            shown++;
        }
    }
    closedir(dir);
}

int run_builtin(char **a) {
    if (!strcmp(a[0], "cd")) {
        const char *d = a[1] ? a[1] : getenv("HOME");
        if (!d) {
            fprintf(stderr, "cd: HOME not set\n");
            return -1;
        }
        if (chdir(d) < 0) {
            perror("cd");
            return -1;
        }
        return 1;
    }
    if (!strcmp(a[0], "help")) {
        printf("mysh built-ins: cd [dir], sysinfo, procs, explain <cmd>, theme [name], bg <pic>, chat, history, jobs, help, exit\n"
               "  ls -la | grep .c | wc -l   multi-pipe commands\n"
               "  date && cal                chain with &&, ||, or ;\n"
               "  !!                         repeat previous command\n"
               "  sysinfo / procs            live kernel info & process table from /proc\n"
               "  explain chmod 755 app.sh   plain-English breakdown of any command\n"
               "  theme ironman              themes: ironman, cap, hulk, thor, reset\n"
               "  bg ~/Pictures/pic.jpg      set terminal wallpaper (bg shade 0.85, bg reset)\n"
               "  ? make a folder demo       AI command or quick question\n"
               "  chat                       full multi-turn AI chatbot\n");
        return 1;
    }
    if (!strcmp(a[0], "history")) {
        HIST_ENTRY **list = history_list();
        if (list) {
            for (int i = 0; list[i]; i++) {
                printf("%4d  %s\n", i + 1, list[i]->line);
            }
        }
        return 1;
    }
    if (!strcmp(a[0], "jobs")) {
        if (!njobs) printf("no active background jobs\n");
        for (int j = 0; j < njobs; j++) {
            printf("[%d] pid %d  %s\n", j + 1, (int)jobs[j].pid, jobs[j].cmd);
        }
        return 1;
    }
    if (!strcmp(a[0], "sysinfo")) {
        show_sysinfo();
        return 1;
    }
    if (!strcmp(a[0], "procs")) {
        show_procs();
        return 1;
    }
    if (!strcmp(a[0], "explain")) {
        ai_explain(a);
        return 1;
    }
    if (!strcmp(a[0], "theme")) {
        if (!a[1]) {
            printf("Themes: ironman, cap, hulk, thor, reset\n");
            return 1;
        }
        if (!strcmp(a[1], "ironman")) {
            set_theme("#1a0000", "#ffd700", YEL);
        } else if (!strcmp(a[1], "cap")) {
            set_theme("#0a1a3a", "#ffffff", RED);
        } else if (!strcmp(a[1], "hulk")) {
            set_theme("#000000", "#39ff14", GRN);
        } else if (!strcmp(a[1], "thor")) {
            set_theme("#101820", "#00bfff", CYN);
        } else if (!strcmp(a[1], "reset")) {
            printf("\033]110\007\033]111\007");
            fflush(stdout);
            copy_str(prompt_color, sizeof prompt_color, GRN);
        } else {
            printf("Unknown theme. Try: ironman, cap, hulk, thor, reset\n");
            return -1;
        }
        return 1;
    }
    if (!strcmp(a[0], "bg")) {
        return handle_bg(a);
    }
    if (!strcmp(a[0], "chat")) {
        fflush(NULL);
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return -1; }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            char *ca[] = {"python3", helper_path, "--chat", NULL};
            execvp(ca[0], ca);
            perror("python3");
            _exit(1);
        }
        waitpid(pid, NULL, 0);
        return 1;
    }
    if (!strcmp(a[0], "exit")) {
        running = 0;
        return 1;
    }
    return 0;
}