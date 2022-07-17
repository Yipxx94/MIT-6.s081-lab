#include "kernel/types.h"
#include "user/user.h"

int main()
{
    int pipefd[2];
    int res = pipe(pipefd);
    if (res == -1)
    {
        fprintf(2, "Failed to create pipe\n");
        exit(1);
    }
    
    int pid = fork();
    if (pid > 0)    // 父进程
    {
        char w_buf = 'Y';
        char r_buf;
        write(pipefd[1], &w_buf, sizeof w_buf);
        close(pipefd[1]);

        wait(0);    // 等待子进程写完以后才能读

        read(pipefd[0], &r_buf, sizeof r_buf);
        printf("%d: received pong\n", getpid());
        close(pipefd[0]);

        exit(0);
    }
    else if (pid == 0)    // 子进程
    {
        char c_w_buf = 'X';
        char c_r_buf;
        read(pipefd[0], &c_r_buf, sizeof c_r_buf);
        printf("%d: received ping\n", getpid());
        close(pipefd[0]);

        write(pipefd[1], &c_w_buf, sizeof c_w_buf);
        close(pipefd[1]);

        exit(0);
    }
    else
    {
        fprintf(2, "Failed to create a child process\n");
        exit(1);
    }
    exit(0);
}