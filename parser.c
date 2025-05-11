#include "config.h"

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