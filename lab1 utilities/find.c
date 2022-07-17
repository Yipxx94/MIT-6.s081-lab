#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* fmtname(char *path)
{
    // 找到最后一个'/'
    char *p;
    for (p = path + strlen(path); p >= path && *p != '/'; p -- )
        ;
    p ++ ;

    return p;
}

void find(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type)
    {
        case T_FILE:    // 找到目标文件
            if (strcmp(fmtname(path), filename) == 0)
                printf("%s\n", path);
            break;
        case T_DIR:
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
            {
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);    // 移到 buf 的末尾
            *p ++ = '/';
            // 遍历所有的文件和目录
            while (read(fd, &de, sizeof de) > 0)
            {
                // 跳过 . 和 .. 或者当前文件夹无文件
                if (!de.inum || !strcmp(de.name, "..") || !strcmp(de.name, "."))
                    continue;
                // 拼接路径
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;    // 字符串结束符
                
                find(buf, filename);    // 向下递归
            }
            break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(2, "Usage: find <directory> <filename>\n");
        exit(1);
    }
    
    find(argv[1], argv[2]);

    exit(0);
}