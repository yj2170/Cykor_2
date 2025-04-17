#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

int main ();
void print_prompt ();
void ls ();
void cd ();


int main () 
{
    char *input = NULL;
    size_t bufsize = 0;

    while (1)
    {
        print_prompt();

        ssize_t nread = getline(&input, &bufsize, stdin); // input 크기 할당, 저장 및 개행 포함 길이 반환
        if (nread == -1)
        { // 실패 시 에러 출력
            perror("getline");
            break;
        }

        input[strcspn(input, "\n")] = '\0'; // \n 위치(인덱스) 반환 후 제거

        if (strcmp(input, "exit") == 0)
        { // exit 명령 처리
            break;
        }

        ls(input);
        // printf("\n");
    }

    free(input);

    return 0;
}

void print_prompt ()
{ // /home/username 이라면 ~로 표기하기, 색 추가하기, 호스트명 크기 재정의, 메모리 해제 전 검증
    // 파일 거르기, 알파벳 정렬하기
    char *username = getenv("USER"); // 유저명 받기
    if (username == NULL)
    { // username이 없을 때
        username = "unknown_user";
    }

    char hostname[256];
    gethostname(hostname, sizeof(hostname)); // 호스트명 받기
    char *cwd = getcwd(NULL, 0); // 자동 메모리 할당 및 디렉토리 경로 저장
    if (cwd == NULL) {
        // 실패 시 에러 출력
        perror("getcwd");
        return;
    }

    printf("%s@%s:%s$ ", username, hostname, cwd);
    
    free(cwd);
}

void ls (char *input)
{
    if (strcmp(input, "ls") == 0)
    {
        struct dirent *entry;
        DIR *dp = opendir("."); // 현재 디렉토리 오픈

        if (dp == NULL)
        { // 실패 시 에러 출력
            perror("opendir");
        }

        while ((entry = readdir(dp)) != NULL) // 문서 끝 도달 전까지
        {
            if (entry->d_name != '.') // 숨김 파일 출력 x
            {
                printf("%s\n", entry->d_name);
            }
        }

        closedir(dp);
    }
}

// void cd ()
// {

// }