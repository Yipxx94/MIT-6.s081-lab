#include "kernel/types.h"
#include "user/user.h"

void child_primes(int * pipefd)
{
    close(pipefd[1]);
    int num;
    int ne_pipefd[2];
    int k = pipe(ne_pipefd);
    if (k == -1)
        exit(1);
    read(pipefd[0], &num, sizeof num);
    printf("prime %d\n", num);
    int pid = fork();
    if (pid > 0)
    {
        child_primes(ne_pipefd);
        exit(0);
    }
    else if (pid == 0)
    {
        // 把筛过的数传给子进程
        int tmp;
        close(ne_pipefd[0]);
        while (read(pipefd[0], &tmp, sizeof tmp) > 0)
        {
            if (tmp % num != 0)
                write(ne_pipefd[1], &tmp, sizeof tmp);
        }
        close(ne_pipefd[1]);
        exit(0);
    }
    else
    {
        fprintf(2, "Failed to create a child process\n");
        exit(1);
    }
    exit(0);
}

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
    if (pid == 0)    // 子进程
    {
        child_primes(pipefd);
        exit(0);
    }
    else if (pid > 0)    // 父进程
    {
        close(pipefd[0]);
        for (int i = 2; i <= 35; i ++ )
        {
            write(pipefd[1], &i, sizeof i);
        }
        close(pipefd[1]);

        wait(0);

        exit(0);
    }
    else
    {
        fprintf(2, "Failed to create a child process\n");
        exit(1);
    }
    exit(0);
}