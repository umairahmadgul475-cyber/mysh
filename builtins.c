#include "mysh.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static Job jobs[MAXJOBS];
static int njobs = 0;
static char hist[HIST][LINE];
static int hcount = 0;

void add_job(pid_t pid, const char *cmd) {
    if (njobs < MAXJOBS) {
        jobs[njobs].pid = pid;
        copy_str(jobs[njobs].cmd, LINE, cmd);
        njobs++;
    }
}

void reap(void) {
    pid_t p;
    while ((p = waitpid(-1, NULL, WNOHANG)) > 0) {
        for (int j = 0; j < njobs; j++) {
            if (jobs[j].pid == p) {
                printf("[done] %d  %s\n", (int)p, jobs[j].cmd);
                jobs[j] = jobs[--njobs];
                break;
            }
        }
    }
}

void add_hist(const char *s) {
    copy_str(hist[hcount % HIST], LINE, s);
    hcount++;
}

static void set_theme(const char *bg, const char *fg) {
    printf("\033]11;%s\007\033]10;%s\007", bg, fg);
    fflush(stdout);
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
            if (!strncmp(buf, "MemTotal:", 9)) sscanf(buf + 9, "%ld", &mem_total);
            else if (!strncmp(buf, "MemAvailable:", 13)) sscanf(buf + 13, "%ld", &mem_avail);
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
    long used_mb = (mem_total - mem_avail) / 1024;
    long total_mb = mem_total / 1024;

    printf(CYN "=== System Info (from /proc) ===" RST "\n");
    printf("CPU    : %s (%d cores)\n", cpu, cores);
    printf("Memory : %ld MB used / %ld MB total\n", used_mb, total_mb);
    printf("Uptime : %d hr %d min\n", hrs, mins);
    printf("Shell  : mysh (PID %d)\n", (int)getpid());
}

int handle_builtin(char **a) {
    if (!strcmp(a[0], "cd")) {
        const char *d = a[1] ? a[1] : getenv("HOME");
        if (!d) fprintf(stderr, "cd: HOME not set\n");
        else if (chdir(d) < 0) perror("cd");
        return 1;
    }
    if (!strcmp(a[0], "help")) {
        printf("mysh built-ins: cd [dir], chat, sysinfo, theme [name], help, history, jobs, exit\n"
               "  ls -la                   run any Linux command\n"
               "  ls | grep .c             pipe output\n"
               "  echo hi > f.txt          redirect: >  >>  <\n"
               "  sleep 10 &               run in background\n"
               "  theme ironman            themes: ironman, cap, hulk, thor, reset\n"
               "  sysinfo                  live CPU, RAM, uptime from /proc\n"
               "  ? make a folder demo     AI command or quick question\n"
               "  chat                     full multi-turn AI chatbot\n");
        return 1;
    }
    if (!strcmp(a[0], "history")) {
        for (int i = hcount > HIST ? hcount - HIST : 0; i < hcount; i++)
            printf("%4d  %s\n", i + 1, hist[i % HIST]);
        return 1;
    }
    if (!strcmp(a[0], "jobs")) {
        if (!njobs) printf("no active background jobs\n");
        for (int j = 0; j < njobs; j++)
            printf("[%d] pid %d  %s\n", j + 1, (int)jobs[j].pid, jobs[j].cmd);
        return 1;
    }
    if (!strcmp(a[0], "sysinfo")) {
        show_sysinfo();
        return 1;
    }
    if (!strcmp(a[0], "theme")) {
        if (!a[1]) printf("Usage: theme <ironman | cap | hulk | thor | reset>\n");
        else if (!strcmp(a[1], "ironman")) set_theme("#1a0000", "#ffd700");
        else if (!strcmp(a[1], "cap")) set_theme("#0a1a3a", "#ffffff");
        else if (!strcmp(a[1], "hulk")) set_theme("#000000", "#39ff14");
        else if (!strcmp(a[1], "thor")) set_theme("#101820", "#00bfff");
        else if (!strcmp(a[1], "reset")) { printf("\033]110\007\033]111\007"); fflush(stdout); }
        else printf("Unknown theme. Try: ironman, cap, hulk, thor, reset\n");
        return 1;
    }
    if (!strcmp(a[0], "chat")) {
        fflush(NULL);
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            char *ca[] = {"python3", helper, "--chat", NULL};
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