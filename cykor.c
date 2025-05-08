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
void ls();
void cd_cmd(char *input);
void pwd();
void trim(char **str);
void process_line(char *line);
int parsing(char *input, ParsedCmd cmds[]);
void executing_cmd(char *cmd);
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
void executing_cmd (char *cmd)
{
    if (strncmp(cmd, "cd", 2) == 0) cd_cmd(cmd);
    else if (strncmp(cmd, "pwd", 3) == 0) pwd();
    else if (strncmp(cmd, "ls", 2) == 0) ls();
    else
    {
        // 외부 명령일 경우
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            return;
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
        }
        else
        {
            // parent
            waitpid(pid, NULL, 0);
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
        if (cmds[i].type == CMD_SIMPLE)
        {
            if (strrchr(cmds[i].cmd, '&') == NULL)
            {
                // not BG
                executing_cmd(cmds[i].cmd);
            }
            else
            {
                // BG
                pid_t pid = fork();

                if (pid < 0)
                {
                    perror("fork failed");
                }
                else if (pid == 0)
                {
                    // child
                    char *amp = strrchr(cmds[i].cmd, '&');
                    *amp = '\0'; // & 제거
                    trim(&cmds[i].cmd);
                    
                    char *argv[MAX_ARGS];
                    int argc = 0;
                    char *token = strtok(cmds[i].cmd, " ");
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
                    // parent
                    printf("Started process %d in background\n", pid);
                }
            }
        }
        else if (cmds[i].type == CMD_AND) // 마지막 명령어는 무조건 type = CMD_SIMPLE 이므로 다른 type에서는 무조건 다음 cmd가 존재함
        {
            //
        }
        i++;       
    }
}


// cmd parsing
int parsing (char *input, ParsedCmd cmds[])
{
    int count = 0;
    char *p = input;
    trim(&p);

    while (*p && count < MAX_SEQ)
    {
        char *op = strpbrk(p, "&|;");

        if (!op)
        {
            cmds[count++] = (ParsedCmd){ .cmd = p, .type = CMD_SIMPLE };
            break;
        }

        // check cmd type
        CmdType type;
        if (op[0] == '&' && op[1] == '&') type = CMD_AND;
        else if (op[0] == '|' && op[1] == '|') type = CMD_OR;
        else if (op[0] == '|') type = CMD_PIPE;
        else if (op[0] == ';') type = CMD_SEQ;
        else {
            // BG
            cmds[count++] = (ParsedCmd){ .cmd = p, .type = CMD_SIMPLE };
            break;
        }

        p = op + ((type == CMD_AND || type == CMD_OR) ? 2 : 1);
        trim(&p);

        // 연산자 명시적 저장 (cmd=NULL일 수도 있음)
        cmds[count++] = (ParsedCmd){ .cmd = p, .type = type };
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


void ls ()
{ // 알파벳 정렬
    struct dirent *entry;
    DIR *dp = opendir("."); // 현재 디렉토리 오픈

    if (dp == NULL)
    { // 실패 시 에러 출력
        perror("opendir");
        return;
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


// 성공하면 0 반환
void cd_cmd (char *input)
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
            return;
        }

        cmd = home_dir;
    }

    // ~ 처리
    if (cmd[0] == '~')
    {
        if (home_dir == NULL)
        {
            fprintf(stderr, "cd: HOME not set\n");
            return;
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
            return;
        }

        cmd = path;
    }

    // 디렉토리 변경
    if (chdir(cmd) != 0)
    {
        perror("cd");
        return;
    }

    // cwd 갱신
    char *new_cwd = getcwd(NULL, 0);
    if (new_cwd == NULL)
    {
        perror("getcwd");
        return;
    }
    free(cwd);
    cwd = new_cwd;
}


void pwd ()
{
    printf("%s\n", cwd);
}