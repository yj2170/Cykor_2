#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

void sigchld_handler(int sig) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

void trim(char **str)
{
    if (*str == NULL || **str == '\0') return;

    // 앞쪽 공백 제거
    while(**str == ' ') (*str)++;

    // 뒷쪽 공백 제거
    char *end = *str + strlen(*str) - 1;
    while (end > *str && *end == ' ')
    {
        *end = '\0';
        end--;
    }
}

int cmpfunc(const void *a, const void *b)
{
    const char **str1 = (const char **)a;
    const char **str2 = (const char **)b;
    return strcmp(*str1, *str2);
}