#include "config.h"

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