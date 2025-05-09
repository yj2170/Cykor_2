#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1

#define PATH_MAX 4096
#define MAX_SEQ 16
#define MAX_PIPE 16
#define MAX_ARGS 64

typedef enum 
{
    CMD_SIMPLE,
    CMD_SEQ,   // ;
    CMD_AND,   // &&
    CMD_OR,    // ||
    CMD_PIPE,  // |
    // CMD_BG     // &
} CmdType;

typedef struct
{
    char *cmd;
    CmdType type;

} ParsedCmd;

int main();
void print_prompt();
int ls();
int cd_cmd(char *input);
int pwd();
void trim(char **str);
void process_line(char *line);
int parsing(char *input, ParsedCmd cmds[]);
int executing_cmd(char *cmd);
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
    }

    free(input);

    return 0;
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


// executing cmd
int executing_cmd (char *cmd)
{
    if (strncmp(cmd, "cd", 2) == 0) return cd_cmd(cmd);
    else if (strncmp(cmd, "pwd", 3) == 0) return pwd();
    else if (strncmp(cmd, "ls", 2) == 0) return ls();
    else
    {
        // 외부 명령일 경우
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            return 1;
        }
        else if (pid == 0)
        {
            // child
            char *argv[MAX_ARGS];
            int argc = 0;
            char *token = strtok(cmd, " ");
            while (token != NULL && argc < MAX_ARGS - 1)
            {
                argv[argc++] = token;
                token = strtok(NULL, " ");
            }
            argv[argc] = NULL;

            execvp(argv[0], argv);
            perror("execvp failed");
            exit(1);
            return 0;
        }
        else
        {
            // parent
            waitpid(pid, NULL, 0);
            return 0;
        }
    }
}


// 전체 process
void process_line (char *input) {
    ParsedCmd cmds[MAX_SEQ];
    int seq_cnt = parsing(input, cmds); // parsing cmds
    int i = 0;

    while (i < seq_cnt)
    {
        ParsedCmd current = cmds[i];
        int status = 0;

        // BG 
        int is_bg = 0;
        char *amp = strrchr(current.cmd, '&');
        if (amp && *(amp + 1) == '\0')
        {
            is_bg = 1;
            *amp = '\0';
            trim(&current.cmd);
        }

        if (i > 0)
        {
            CmdType prev_type = cmds[i - 1].type;
            if ((prev_type == CMD_AND && status != 0) || (prev_type == CMD_OR && status == 0))
            {
                i++;
                continue;
            }
        }

        if (current.type == CMD_PIPE)
        {
            int j = i;
            char *pipe_cmds[MAX_PIPE];
            int pipe_cnt = 0;

            pipe_cmds[pipe_cnt++] = cmds[j].cmd;

            while (cmds[j + 1].type == CMD_PIPE)
            {
                j++;
                pipe_cmds[pipe_cnt++] = cmds[j].cmd;
            }

            execute_pipeline(pipe_cmds, pipe_cnt, 0);

            i = j;
            status = 0;
        }
        else if (is_bg) // case of bg
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                // child
                char *argv[MAX_ARGS];
                int argc = 0;
                char *token = strtok(current.cmd, " ");
                while (token != NULL && argc < MAX_ARGS - 1)
                {
                    argv[argc++] = token;
                    token = strtok(NULL, " ");
                }
                argv[argc] = NULL;
                execvp(argv[0], argv);
                perror("exec failed");
                exit(1);
            }
            else
            {
                printf("Started process %d in background\n", pid);
                status = 0;
            }
        }
        else // 일반
        {
            status = executing_cmd(current.cmd);
        }


        i++;       
    }
}

void execute_pipeline(char *cmds[], int n, int background) {
    int pipefd[2], prev_fd = -1;
    pid_t pids[MAX_PIPE];

    for (int i = 0; i < n; i++) {
        if (i < n - 1 && pipe(pipefd) < 0) {
            perror("pipe");
            exit(1);
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i < n - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            for (int j = 0; j < i; j++) close(pipefd[j]);

            char *argv[MAX_ARGS];
            int argc = 0;
            char *token = strtok(cmds[i], " ");
            while (token && argc < MAX_ARGS - 1) {
                argv[argc++] = token;
                token = strtok(NULL, " ");
            }
            argv[argc] = NULL;

            execvp(argv[0], argv);
            perror("exec failed");
            exit(1);
        } else if (pid > 0) {
            pids[i] = pid;
            if (prev_fd != -1) close(prev_fd);
            if (i < n - 1) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            }
        } else {
            perror("fork");
            exit(1);
        }
    }

    if (!background) {
        for (int i = 0; i < n; i++) {
            waitpid(pids[i], NULL, 0);
        }
    }

    if (prev_fd != -1) close(prev_fd);
}


// cmd parsing
int parsing(char *input, ParsedCmd cmds[])
{
    int count = 0;
    char *p = input;
    trim(&p);

    while (*p && count < MAX_SEQ)
    {
        char *op = strpbrk(p, "&|;");

        // nontype
        if (!op)
        {
            cmds[count++] = (ParsedCmd){ .cmd = p, .type = CMD_SIMPLE };
            break;
        }

        char *end = op;
        *end = '\0';
        trim(&p);
        cmds[count].cmd = p;

        // check type
        CmdType next_type;
        if (op[0] == '&' && op[1] == '&')
        {
            next_type = CMD_AND;
            p = op + 2;
        }
        else if (op[0] == '|' && op[1] == '|')
        {
            next_type = CMD_OR;
            p = op + 2;
        }
        else if (op[0] == '|')
        {
            next_type = CMD_PIPE;
            p = op + 1;
        }
        else if (op[0] == ';')
        {
            next_type = CMD_SEQ;
            p = op + 1;
        }
        else
        {
            next_type = CMD_SIMPLE; // bg
            p = op + 1;
        }

        trim(&p);
        cmds[count++].type = next_type;
    }

    return count;
}


void print_prompt ()
{
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
        printf("%s@%s:~%s$ ", username, hostname, cwd + strlen(home_dir));
    }
    else
    {
        printf("%s@%s:%s$ ", username, hostname, cwd);
    }
}


int ls ()
{ // 알파벳 정렬
    struct dirent *entry;
    DIR *dp = opendir("."); // 현재 디렉토리 오픈

    if (dp == NULL)
    { // 실패 시 에러 출력
        perror("opendir");
        return 1;
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
    return 0;
}


// 성공하면 0 반환
int cd_cmd (char *input)
{
    char *cmd = input + 2;
    trim(&cmd);
    while(*cmd == ' ') cmd++;
    char path[PATH_MAX];
    char *home_dir = getenv("HOME");

    // cd 일 경우
    if (*cmd == '\0')
    {
        if(home_dir == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }

        cmd = home_dir;
    }

    // ~ 처리
    if (cmd[0] == '~')
    {
        if (home_dir == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }

        if (cmd[1] == '\0')
        {
            snprintf(path, sizeof(path), "%s", home_dir);
        }
        else if (cmd[1] == '/')
        {
            snprintf(path, sizeof(path), "%s%s", home_dir, cmd + 1);
        }
        else
        {
            fprintf(stderr, "cd: unsupported ~ syntax\n");
            return 1;
        }

        cmd = path;
    }

    // 디렉토리 변경
    if (chdir(cmd) != 0)
    {
        perror("cd");
        return 1;
    }

    // cwd 갱신
    char *new_cwd = getcwd(NULL, 0);
    if (new_cwd == NULL)
    {
        perror("getcwd");
        return 1;
    }
    free(cwd);
    cwd = new_cwd;
    return 0;
}


int pwd ()
{
    printf("%s\n", cwd);
    return 0;
}