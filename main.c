#include "config.h"

char *cwd = NULL;
int seq_cnt = 0;
ParsedCmd cmds[MAX_SEQ] = {0};
int is_bg = 0;
int status = 0;
int wstatus = 0;

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
        { // error
            perror("getline");
            break;
        }

        // \n 위치(인덱스) 반환 후 제거
        input[strcspn(input, "\n")] = '\0';

        // exit
        if (strcmp(input, "exit") == 0)
        {
            break;
        }

        process_line(input);

        for (int i = 0; i < seq_cnt; i++) {
            free(cmds[i].cmd);  // strdup() 된 문자열을 해제
            cmds[i].cmd = NULL; // 포인터 초기화
        }
    }

    free(input);
    free(cwd);

    return 0;
}