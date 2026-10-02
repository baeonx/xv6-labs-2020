#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

char** splitArgs(int len, char* s, int* args_len)
{
    static char* args[MAXARG];
    char* p = s;
    char* end = s + len - 1;
    int argc = 0;
    args[argc ++] = p;
    while(1)
    {
        // 将p移动到 '\0'
        while(*p != '\0') p ++;
        // 如果是最后一个end直接退出
        if(p == end) break;
        // 移动到'\0'的下一个元素
        p ++;
        args[argc ++] = p;
    }
    *args_len = argc;
    return args;
}

char* getArg(int *len)
{
    char buf[512];
    int i = 0, n = 0;
    // 提取""
    while((n = read(0, buf + i, sizeof(buf) - i)) > 0){   // 读 stdin(管道)
        i += n;
    }
    buf[i] = '\0';
    
    static char arg[512];
    int args = 0;
    char* s = buf;
    while(*s != '\0')
    {
        if(*s == '"')
        {
            s += 1;
            continue;
        }
        if(*s == ' ' || (*s == '\n') || (*s == '\t') || (*s == '\r'))
        {
            arg[args++] = '\0';        // 空格/换行/制表符都作为分隔符
            s += 1;
            continue;
        }
        if((*s == '\\') && (*(s+1) == 't' || *(s+1) == 'n' || *(s+1) == 'r'))
        {
            arg[args++] = '\0';
            s += 2;
            continue;
        }
        arg[args++] = *s;
        s ++;
    }
    arg[args ++] = '\0';
    *len = args;
    return arg;
}

// 在 cmd 数组里执行命令:cmd 前 cmd_len 个是有效项,后面补 NULL
void run(char** cmd, int cmd_len)
{
    cmd[cmd_len] = 0;             // NULL 结尾
    exec(cmd[0], cmd);
    fprintf(2, "exec %s failed\n", cmd[0]);
    exit(1);
}

int
main(int argc, char *argv[])
{
    // 得到管道输入过来的数据
    int arg_len;
    char* arg = getArg(&arg_len);
    // for(int i = 0; i < arg_len; i ++) printf("args[%d]: %c\n", i, arg[i]);
    // printf("arg_len:%d\n", arg_len);

    int args_len;
    char** args = splitArgs(arg_len, arg, &args_len);
    // printf("%s\n", args[0]);
    // printf("%s\n", args[1]);
    // printf("%d\n", args_len);

    static char* xargs_buf[MAXARG];
    char* param_n = "-n";
    if(strcmp(argv[1], param_n) == 0)
    {
        // 命令 = argv[3..] (跳过 xargs -n 1)
        int op_len = argc - 3;
        for(int i = 3; i < argc; i++)
            xargs_buf[i-3] = argv[i];
        for(int i = 0; i < args_len; i ++)
        {
            if(fork() == 0)
            {
                xargs_buf[op_len ++] = args[i];
                run(xargs_buf, op_len);
                exit(0);
            }
            else
            {
                wait(0);
            }
        }
        exit(0);
    }
    // 无 -n:一次执行,命令 + 所有切出的词
    int op_len = 0;
    for(int i = 1; i < argc; i++)
        xargs_buf[op_len++] = argv[i];
    for(int i = 0; i < args_len; i++)
        xargs_buf[op_len++] = args[i];
    if(fork() == 0)
    {
        run(xargs_buf, op_len);
        exit(0);
    }
    wait(0);

    exit(0);
}