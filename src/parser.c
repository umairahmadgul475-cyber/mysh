#include "mysh.h"

void copy_str(char *dst, size_t n, const char *src) {
    size_t l = strlen(src);
    if (l >= n) l = n - 1;
    memcpy(dst, src, l);
    dst[l] = '\0';
}

int ask_yes_no(const char *prompt) {
    printf("%s", prompt);
    fflush(stdout);

    if (!isatty(STDIN_FILENO)) {
        char ans[16];
        if (fgets(ans, sizeof ans, stdin) && (ans[0] == 'y' || ans[0] == 'Y')) {
            return 1;
        }
        return 0;
    }

    struct termios orig, raw;
    if (tcgetattr(STDIN_FILENO, &orig) < 0) {
        char ans[16];
        if (fgets(ans, sizeof ans, stdin) && (ans[0] == 'y' || ans[0] == 'Y')) {
            return 1;
        }
        return 0;
    }

    raw = orig;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    char choice = 0;
    for (;;) {
        unsigned char ch;
        ssize_t r = read(STDIN_FILENO, &ch, 1);
        if (r <= 0) {
            putchar('\n');
            break;
        }
        if (ch == '\n' || ch == '\r') {
            putchar('\n');
            break;
        }
        if (ch == 3 || ch == 4) {
            choice = 0;
            putchar('\n');
            break;
        }
        if (ch == 27) {
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 1;
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            unsigned char seq;
            if (read(STDIN_FILENO, &seq, 1) == 1 && (seq == '[' || seq == 'O')) {
                while (read(STDIN_FILENO, &seq, 1) == 1) {
                    if (seq >= 0x40 && seq <= 0x7E) break;
                }
            }
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            tcsetattr(STDIN_FILENO, TCSANOW, &raw);
            continue;
        }
        if (ch == 127 || ch == '\b') {
            if (choice != 0) {
                choice = 0;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (choice == 0 && (ch == 'y' || ch == 'Y' || ch == 'n' || ch == 'N')) {
            choice = (char)ch;
            putchar(choice);
            fflush(stdout);
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    return (choice == 'y' || choice == 'Y');
}

int tokenize(const char *s, char *out, char *tok[], int isop[]) {
    int n = 0;
    while (*s && n < MAXTOK - 1) {
        while (isspace((unsigned char)*s)) s++;
        if (!*s) break;
        tok[n] = out;
        isop[n] = 0;
        if (strchr("|<>&", *s)) {
            isop[n] = 1;
            *out++ = *s++;
            if (out[-1] == '>' && *s == '>') *out++ = *s++;
        } else {
            while (*s && !isspace((unsigned char)*s) && !strchr("|<>&", *s)) {
                if (*s == '\'' || *s == '"') {
                    char q = *s++;
                    while (*s && *s != q) *out++ = *s++;
                    if (*s) s++;
                } else {
                    *out++ = *s++;
                }
            }
        }
        *out++ = '\0';
        n++;
    }
    return n;
}