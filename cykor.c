#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>

#define PATH_MAX 4096
#define MAX_SEQ 16
#define MAX_PIPE 16
#define MAX_ARGS 64

int main();
void print_prompt();
void ls();
void cd_cmd(char *input);
void pwd();
void process_line(char *line);
int split_sequences(char *line, char *seqs[]);
int strip_background_flag(char *cmd);
int split_pipes(char *seq, char *pipes[]);
void execute_pipeline(char *cmds[], int n, int background);

char *cwd = NULL;

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

        process_line(input);
    }

    free(input);

    return 0;
}


// 전체 process
void process_line(char *input) {
    char *seqs[MAX_SEQ];
    int seq_cnt = split_sequences(input, seqs); // 토큰 수 (;만 취급)

    for (int i = 0; i < seq_cnt; i++) {
        // & 검사
        int background = strip_background_flag(seqs[i]); // 마지막에 &있으면 1반환


        // 파이프라인
        char *pipes[MAX_PIPE];
        int pipe_cnt = split_pipes(seqs[i], pipes);
        execute_pipeline(pipes, pipe_cnt, background);
    }
}


// ; 명령어 처리 - token화, 토큰 수 반환
int split_sequences(char *input, char *seqs[])
{
    char *save1;
    int cnt = 0;
    for (char *tok = strtok_r(input, ";", &save1); tok && cnt < MAX_SEQ; tok = strtok_r(NULL, ";", &save1))
    {
        // trim 앞뒤 공백
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') *end-- = '\0';
        seqs[cnt++] = tok;
    }
    return cnt;
}


// background - 맨 끝 & 있으면 제거하고 flag 리턴
int strip_background_flag(char *input) {
    size_t len = strlen(input);
    if (len > 0 && input[len-1] == '&') {
        input[--len] = '\0';
        while (len > 0 && input[len-1] == ' ')
            input[--len] = '\0';
        return 1;
    }
    return 0;
}


// split pipeline
int split_pipes(char *seq, char *pipes[]) {
    char *save2;
    int cnt = 0;
    for (char *tok = strtok_r(seq, "|", &save2);
         tok && cnt < MAX_PIPE;
         tok = strtok_r(NULL, "|", &save2)) {
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') *end-- = '\0';
        pipes[cnt++] = tok;
    }
    return cnt;
}


// execute pipline + background
void execute_pipeline(char *cmds[], int n, int background) {
    int pipe_fds[MAX_PIPE-1][2];
    pid_t pids[MAX_PIPE];

    for (int i = 0; i < n-1; i++) {
        if (pipe(pipe_fds[i]) < 0) { perror("pipe"); return; }
    }

    // fork+exec
    for (int i = 0; i < n; i++) {
        if ((pids[i] = fork()) == 0) {
            if (i > 0)      dup2(pipe_fds[i-1][0], STDIN_FILENO);
            if (i < n-1)    dup2(pipe_fds[i][1], STDOUT_FILENO);
            for (int j = 0; j < n-1; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            
            char *argv[MAX_ARGS];
            int argc = 0;
            char *save3;
            char *arg = strtok_r(cmds[i], " ", &save3);
            while (arg && argc < MAX_ARGS-1) {
                argv[argc++] = arg;
                arg = strtok_r(NULL, " ", &save3);
            }
            argv[argc] = NULL;
            
            if (argv[0] && strcmp(argv[0], "cd") == 0) {
                cd_cmd(argc > 1 ? argv[1] : "");
                exit(0);
            }
            // 외부 명령 실행
            execvp(argv[0], argv);
            perror("execvp");
            exit(1);
        }
    }

    // 부모: 파이프 닫기
    for (int i = 0; i < n-1; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }
    // foreground면 대기, background면 PID 출력
    if (!background) {
        for (int i = 0; i < n; i++)
            waitpid(pids[i], NULL, 0);
    } else {
        printf("[bg pid %d]\n", pids[n-1]);
    }
}

void print_prompt ()
{ // 색 추가, 호스트명 크기, 메모리 해제 전 검증

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


void ls ()
{ // 알파벳 정렬
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


void cd_cmd (char *input)
{
    char path[PATH_MAX];
    char *home_dir = getenv("HOME");

    // cd 일 경우
    if (strlen(input) == 0)
    {
        if(home_dir == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }

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