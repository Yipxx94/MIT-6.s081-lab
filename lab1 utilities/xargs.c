#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    char buf[512];
    char *args[MAXARG];    // 二维数组，其中每个元素指向一个字符串
    
    if (argc < 2)
    {
        fprintf(2, "Usage: xargs <command>\n");
        exit(1);
    }

    // 初始化参数列表
    for (int i = 1; i < argc; i ++ )
        args[i - 1] = argv[i];
    args[argc] = 0;    // 终止0

    while (1)    // 每一行调用命令
    {
        int k = 0;
        // 读取一行
        while (1)
        {
            int len = read(0, &buf[k], sizeof buf[k]);
            if (!len || buf[k] == '\n')
                break;
            k ++ ;
        }
        if (!k)
            break;
        buf[k] = 0;    // 终止0
        args[argc - 1] = buf;    // 将该行的标准输入添加到参数最后
        
        // 调用子进程处理命令
        int pid = fork();
        if (pid == 0)
        {
            exec(args[0], args);    // 要清楚该函数的逻辑
            exit(0);
        }
        else if (pid > 0)
            wait(0);
        else
        {
            fprintf(2, "Failed to create a child process\n");
            exit(1);
        }
    }
    exit(0);
}