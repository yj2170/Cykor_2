#include "config.h"

void print_prompt ()
{
    const char *color_reset = "\033[0m";
    const char *color_user = "\033[1;32m";  // green
    const char *color_host = "\033[1;34m";  // blue
    const char *color_dir = "\033[1;36m";   // turquoise
    const char *color_symbol = "\033[1;33m"; // yellow

    // cwd 유효성 검사
    if (cwd == NULL) {
        cwd = getcwd(NULL, 0);
        if (cwd == NULL) {
            perror("getcwd");
            printf("unknown$ ");
            return;
        }
    }

    // 유저명 받기
    char *username = getenv("USER");
    if (username == NULL)
    { // username이 없을 때
        username = "unknown_user";
    }

    // 호스트명 받기
    char hostname[256];
    gethostname(hostname, sizeof(hostname));

    // ~ 설정
    char *home_dir = getenv("HOME");
    if (strncmp(cwd, home_dir, strlen(home_dir)) == 0)
    { // home_dir 건너뛰고 출력
        printf("%s%s@%s%s:%s~%s%s$ ", color_user, username, color_host, hostname, color_dir, cwd + strlen(home_dir), color_symbol);
    }
    else
    {
        printf("%s%s@%s%s:%s%s%s$ ", color_user, username, color_host, hostname, color_dir, cwd, color_symbol);
    }

    printf("%s", color_reset);
}