#ifndef CONFIG_H
#define CONFIG_H

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
} CmdType;

typedef struct
{
    char *cmd;
    CmdType type;

} ParsedCmd;

int main();
void print_prompt();
int cmpfunc(const void *a, const void *b);
int ls();
int cd_cmd(char *input);
int pwd();
void trim(char **str);
void process_line(char *line);
int parsing(char *input, ParsedCmd cmds[]);
int execute_cmd(char *cmd);
int execute_pipeline(char *cmds[], int n, int background);

extern char *cwd;
extern int seq_cnt;
extern ParsedCmd cmds[MAX_SEQ];
extern int is_bg;
extern int status;
extern int wstatus;

#endif