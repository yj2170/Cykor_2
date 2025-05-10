#include "config.h"

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

            status = execute_pipeline(pipe_cmds, pipe_cnt, is_bg);

            i = j + 1;
            status = 0;
            continue;
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
                printf("[bg] pid: %d\n", pid);
                status = 0; // 좀비 프로세스?
            }
        }
        else // 일반
        {
            status = execute_cmd(current.cmd);
        }

        i++;
    }
}

int execute_cmd (char *cmd)
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
            waitpid(pid, &wstatus, 0);
            if (WIFEXITED(wstatus)) {
                return WEXITSTATUS(wstatus);
            } else {
                return 1;
            }
        }
    }
}

int execute_pipeline(char *cmds[], int count, int is_bg)
{
    int pipefd[2];
    int prev_fd = -1;
    pid_t pids[MAX_PIPE];

    for (int i = 0; i < count; i++)
    {
        if (i < count - 1)
        {
            pipe(pipefd);
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

            // 명령어 파싱 및 실행
            char *argv[MAX_ARGS];
            int argc = 0;
            char *token = strtok(cmds[i], " ");
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

    if (!is_bg)
    {
        for (int i = 0; i < count; i++)
        {
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
    }

    return status;
}