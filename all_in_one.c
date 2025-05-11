#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

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
} CmdType;

typedef struct
{
    char *cmd;
    CmdType type;
    int is_bg;

} ParsedCmd;

int main();
void print_prompt();
void sigchld_handler(int sig);
void trim(char **str);
int cmpfunc(const void *a, const void *b);
int ls();
int cd_cmd(char *input);
int pwd();
int parsing(char *input, ParsedCmd cmds[]);
void execute_exec(char *cmd);
void process_line(char *line);
int execute_cmd(char *cmd);
int execute_pipeline(char *cmds[], int n);

char *cwd;
int seq_cnt;
ParsedCmd cmds[MAX_SEQ];
int status;

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

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
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


void execute_exec(char *cmd)
{
    char *argv[MAX_ARGS];
    int argc = 0;
    char *token = strtok(cmd, " ");
    
    while (token != NULL)
    {
        if (argc >= MAX_ARGS - 1)
        {
            fprintf(stderr, "Too many arguments\n");
            exit(1);
        }

        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    argv[argc] = NULL;
    execvp(argv[0], argv);
    perror("execvp failed");
    exit(1);
}


// executing cmd
int execute_cmd (char *exeCmd)
{
    if (strncmp(exeCmd, "cd", 2) == 0) return cd_cmd(exeCmd);
    else if (strncmp(exeCmd, "pwd", 3) == 0) return pwd();
    else if (strncmp(exeCmd, "ls", 2) == 0) return ls();
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
            execute_exec(exeCmd);
            return 0;
        }
        else
        {
            int wstatus;
            waitpid(pid, &wstatus, 0);
            if (WIFEXITED(wstatus)) {
                return WEXITSTATUS(wstatus);
            } else {
                return 1;
            }
        }
    }
}


// 전체 process
void process_line (char *input)
{
    seq_cnt = parsing(input, cmds); // parsing cmds
    int i = 0;

    while (i < seq_cnt)
    {
        ParsedCmd current = cmds[i];

        // and or 처리
        if (i > 0)
        {
            CmdType prev_type = cmds[i - 1].type;
            if ((prev_type == CMD_AND && status != 0) || (prev_type == CMD_OR && status == 0))
            {
                i++;
                continue;
            }
        }

        // pipe 명령 수 세고 execute_pipeline 호출
        if (current.type == CMD_PIPE)
        {
            int j = i;
            char *pipe_cmds[MAX_PIPE];
            int pipe_cnt = 0;

            pipe_cmds[pipe_cnt++] = current.cmd;

            while (cmds[j].type == CMD_PIPE)
            {
                j++;
                pipe_cmds[pipe_cnt++] = cmds[j].cmd;
            }

            status = execute_pipeline(pipe_cmds, pipe_cnt);
            i = j + 1;
            continue;
        }
        else if (current.is_bg) // case of bg
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                // child
                execute_exec(current.cmd);
            }
            else
            {
                // parent
                printf("[bg] pid: %d\n", pid);
                status = 0;
            }
        }
        else // 일반
        {
            status = execute_cmd(current.cmd);
        }

        i++;
    }
}


int execute_pipeline(char *pipeCmds[], int count)
{
    int pipefd[2];
    int prev_fd = -1;
    pid_t pids[MAX_PIPE];

    for (int i = 0; i < count; i++)
    {
        if (pipe(pipefd) == -1)
        {
            perror("pipe");
            exit(1);
        }

        pid_t pid = fork();
        if (pid == 0)
        {
            if (i > 0)
            {
                dup2(prev_fd, 0); // 이전 명령의 출력을 현재 명령의 입력으로
                close(prev_fd);
            }

            if (i < count - 1)
            {
                dup2(pipefd[1], 1); // 현재 명령의 출력을 pipe로
                close(pipefd[0]);
                close(pipefd[1]);
            }

            execute_exec(pipeCmds[i]);
        }
        else if (pid > 0)
        {
            pids[i] = pid;
            if (prev_fd != -1) close(prev_fd);
            if (i < count - 1)
            {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            }
        }
        else
        {
            perror("fork failed");
            return 1;
        }
    }

    for (int i = 0; i < count; i++)
    {
        int wstatus;
        waitpid(pids[i], &wstatus, 0);

        if (i == count - 1)
        { // 마지막 명령의 status만 기록
            if (WIFEXITED(wstatus))
            {
                status = WEXITSTATUS(wstatus);
            }
            else
            {
                status = 1;
            }
        }
    }

    return status;
}


// cmd parsing
int parsing(char *input, ParsedCmd cmds[])
{
    int count = 0;
    char *p = input;
    trim(&p);

    while (*p && count < MAX_SEQ)
    {
        int bg = 0;
        char *op = strpbrk(p, "&|;");

        // nontype
        if (!op)
        {
            cmds[count++] = (ParsedCmd){ .cmd = strdup(p), .type = CMD_SIMPLE, .is_bg = bg };
            break;
        }

        // check type
        CmdType type;
        if (op[0] == '&' && op[1] == '&')
        {
            type = CMD_AND;
        }
        else if (op[0] == '|' && op[1] == '|')
        {
            type = CMD_OR;
        }
        else if (op[0] == '|')
        {
            type = CMD_PIPE;
        }
        else if (op[0] == ';')
        {
            type = CMD_SEQ;
        }
        else
        {
            type = CMD_SIMPLE; // bg
            bg = 1;
        }

        char *end = op;
        *end = '\0';
        trim(&p);
        cmds[count++] = (ParsedCmd){ .cmd = strdup(p), .type = type, .is_bg = bg };

        if (cmds[count - 1].type == CMD_AND || cmds[count - 1].type == CMD_OR)
        {
            p = op + 2;
        } else p = op + 1;

        trim(&p);
    }

    return count;
}


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


int cmpfunc(const void *a, const void *b)
{
    const char **str1 = (const char **)a;
    const char **str2 = (const char **)b;
    return strcmp(*str1, *str2);
}


int ls()
{
    struct dirent *entry;
    DIR *dp = opendir(".");
    if (dp == NULL)
    {
        perror("opendir");
        return 1;
    }

    // 파일 이름들 저장
    char **files = NULL;
    int count = 0;

    while ((entry = readdir(dp)) != NULL)
    {
        // 숨김 파일 제외
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && entry->d_name[0] != '.')
        {
            files = realloc(files, sizeof(char *) * (count + 1));
            if (files == NULL)
            {
                perror("realloc");
                closedir(dp);
                return 1;
            }

            files[count] = strdup(entry->d_name);
            if (files[count] == NULL)
            {
                perror("strdup");
                closedir(dp);
                return 1;
            }

            count++;
        }
    }

    closedir(dp);

    qsort(files, count, sizeof(char *), cmpfunc);

    for (int i = 0; i < count; i++)
    {
        printf("%s  ", files[i]);
        free(files[i]);
    }

    printf("\n");

    free(files);
    return 0;
}


// 성공하면 0 반환
int cd_cmd (char *input)
{
    char *cmd = input + 2;
    trim(&cmd);
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
            if (snprintf(path, sizeof(path), "%s%s", home_dir, cmd + 1) >= sizeof(path))
            {
                fprintf(stderr, "cd path too long\n");
                return 1;
            }
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