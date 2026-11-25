#include "mysh.h"
#include <ctype.h>
#include <string.h>

void copy_str(char *dst, size_t n, const char *src) {
    size_t l = strlen(src);
    if (l >= n) l = n - 1;
    memcpy(dst, src, l);
    dst[l] = '\0';
}

int tokenize(const char *s, char *o, char *tok[], int isop[]) {
    int n = 0;
    while (*s && n < MAXTOK - 1) {
        while (isspace((unsigned char)*s)) s++;
        if (!*s) break;
        tok[n] = o;
        isop[n] = 0;
        if (strchr("|<>&", *s)) {
            isop[n] = 1;
            *o++ = *s++;
            if (o[-1] == '>' && *s == '>') *o++ = *s++;
        } else {
            while (*s && !isspace((unsigned char)*s) && !strchr("|<>&", *s)) {
                if (*s == '\'' || *s == '"') {
                    char q = *s++;
                    while (*s && *s != q) *o++ = *s++;
                    if (*s) s++;
                } else {
                    *o++ = *s++;
                }
            }
        }
        *o++ = '\0';
        n++;
    }
    return n;
}