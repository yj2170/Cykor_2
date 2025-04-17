#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#define PATH_MAX 4096
// 입력 에러 처리, 파이프라인, 다중 명령어, 백그라운드 실행

int main();
void print_prompt();
void ls();
void cd();
void pwd();

char *cwd = NULL; // cwd를 프롬프트에 갱신하고 싶어서 전역변수로 선언함

int main () 
{
    char *input = NULL;
    size_t bufsize = 0;

    // 현재 디렉토리 받기
    cwd = getcwd(NULL, 0); // 자동 메모리 할당 및 디렉토리 경로 저장
    if (cwd == NULL) {
        // 실패 시 에러 출력
        perror("getcwd");
        return 1;
    }

    while (1)
    {
        print_prompt();

        // input 크기 할당, 저장 및 개행 포함 길이 반환
        ssize_t nread = getline(&input, &bufsize, stdin);
        if (nread == -1)
        { // 실패 시 에러 출력
            perror("getline");
            break;
        }

        // \n 위치(인덱스) 반환 후 제거
        input[strcspn(input, "\n")] = '\0';

        // exit 명령 처리
        if (strcmp(input, "exit") == 0)
        {
            break;
        }

        execute_command();
        
    }

    free(input);

    return 0;
}

void execute_command (input)
{
    if (strcmp(input, "ls") == 0)
    {
        ls(input);
    }
    else if (strcmp(input, "pwd") == 0)
    {
        pwd();
    }
    else if (strncmp(input, "cd ", 3) == 0)
    {
        cd(input + 3);
    }
}

void print_prompt ()
{ // 색 추가하기, 호스트명 크기 재정의, 메모리 해제 전 검증

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
        printf("%s@%s:~%s$ ", username, hostname, cwd + strlen(home_dir));
    }
    else
    {
        printf("%s@%s:%s$ ", username, hostname, cwd);
    }
}



void ls (char *input)
{ // 미완성... - 알파벳 정렬하기
    struct dirent *entry;
    DIR *dp = opendir("."); // 현재 디렉토리 오픈

    if (dp == NULL)
    { // 실패 시 에러 출력
        perror("opendir");
    }

    while ((entry = readdir(dp)) != NULL) // 문서 끝 도달 전까지
    {
        // 숨김 파일 출력 x
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && entry->d_name[0] != '.')
        {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dp);
}



void cd (char *input)
{
    char path[PATH_MAX];
    char *home_dir = getenv("HOME");

    // cd 일 경우
    if (strlen(input) == 0)
    {
        input = home_dir;
    }

    // ~ 처리
    if (input[0] == '~')
    {
        if (home_dir == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }

        if (input[1] == '\0')
        {
            snprintf(path, sizeof(path), "%s", home_dir);
        }
        else if (input[1] == '/')
        {
            snprintf(path, sizeof(path), "%s%s", home_dir, input + 1);
        }
        else
        {
            fprintf(stderr, "cd: unsupported ~ syntax\n");
            return;
        }

        input = path;
    }

    // 디렉토리 변경
    if (chdir(input) != 0)
    {
        perror("cd");
        return;
    }

    // cwd 갱신
    free(cwd);
    cwd = getcwd(NULL, 0);
    if (cwd == NULL)
    {
        perror("getcwd");
    }
}



void pwd ()
{
    printf("%s\n", cwd);
}