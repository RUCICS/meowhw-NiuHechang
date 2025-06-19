// mycat2.c
#include <unistd.h> // for read, write, close, sysconf
#include <fcntl.h>  // for open
#include <stdio.h>  // for perror
#include <stdlib.h> // for malloc, free, exit
#include <errno.h>  // for errno

// 获取 I/O 缓冲区大小（本任务中即为系统页面大小）
long io_blocksize(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1)
    {
        // fallback：默认返回 4KB（4096字节）
        return 4096;
    }
    return page_size;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        const char *msg = "Usage: mycat2 <filename>\n";
        write(STDERR_FILENO, msg, sizeof("Usage: mycat2 <filename>\n") - 1);
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    long bufsize = io_blocksize();
    char *buffer = (char *)malloc(bufsize);
    if (buffer == NULL)
    {
        perror("malloc");
        close(fd);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, bufsize)) > 0)
    {
        ssize_t total_written = 0;
        while (total_written < bytes_read)
        {
            ssize_t bytes_written = write(STDOUT_FILENO, buffer + total_written, bytes_read - total_written);
            if (bytes_written == -1)
            {
                perror("write");
                free(buffer);
                close(fd);
                exit(EXIT_FAILURE);
            }
            total_written += bytes_written;
        }
    }

    if (bytes_read == -1)
    {
        perror("read");
        free(buffer);
        close(fd);
        exit(EXIT_FAILURE);
    }

    free(buffer);

    if (close(fd) == -1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}
