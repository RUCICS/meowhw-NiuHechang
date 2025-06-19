// mycat3.c
#include <unistd.h> // for read, write, close, sysconf
#include <fcntl.h>  // for open
#include <stdio.h>  // for perror
#include <stdlib.h> // for malloc, free, exit
#include <errno.h>  // for errno
#include <stdint.h> // for uintptr_t
#include <string.h> // for memset

// 获取系统页面大小作为缓冲区大小
long io_blocksize(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1)
    {
        return 4096; // fallback
    }
    return page_size;
}

// 分配对齐到页大小的内存
void *align_alloc(size_t size)
{
    long page_size = io_blocksize();
    if (page_size <= 0)
        page_size = 4096;

    // 多分配 page_size 字节，用于存放原始指针
    void *original = malloc(size + page_size);
    if (!original)
        return NULL;

    uintptr_t raw_addr = (uintptr_t)original + page_size;
    uintptr_t aligned_addr = raw_addr & ~(page_size - 1);

    // 在对齐地址前面预留一个指针长度，存放原始 malloc 返回值
    ((void **)aligned_addr)[-1] = original;
    return (void *)aligned_addr;
}

// 释放 align_alloc 分配的内存
void align_free(void *ptr)
{
    if (ptr)
    {
        void *original = ((void **)ptr)[-1];
        free(original);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        const char *msg = "Usage: mycat3 <filename>\n";
        write(STDERR_FILENO, msg, sizeof("Usage: mycat3 <filename>\n") - 1);
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    long bufsize = io_blocksize();
    char *buffer = (char *)align_alloc(bufsize);
    if (!buffer)
    {
        perror("align_alloc");
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
                align_free(buffer);
                close(fd);
                exit(EXIT_FAILURE);
            }
            total_written += bytes_written;
        }
    }

    if (bytes_read == -1)
    {
        perror("read");
        align_free(buffer);
        close(fd);
        exit(EXIT_FAILURE);
    }

    align_free(buffer);

    if (close(fd) == -1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}
