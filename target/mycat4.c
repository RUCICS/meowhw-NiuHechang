// mycat4.c
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

// 获取缓冲区大小：内存页大小 & 文件系统块大小的 LCM 或最大值（取决于对齐）
long io_blocksize(int fd)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
    {
        page_size = 4096;
    }

    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        perror("fstat");
        return page_size; // fallback
    }

    long fs_block_size = st.st_blksize;

    // 检查文件系统块大小是否合理（正数且为2的幂）
    if (fs_block_size <= 0 || (fs_block_size & (fs_block_size - 1)) != 0)
    {
        fs_block_size = page_size;
    }

    // 选择更大的对齐单位（对齐到两者中较大的一个，确保兼容）
    return (page_size > fs_block_size) ? page_size : fs_block_size;
}

// 分配对齐到页面起始地址的内存
void *align_alloc(size_t size)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
        page_size = 4096;

    void *original = malloc(size + page_size);
    if (!original)
        return NULL;

    uintptr_t raw = (uintptr_t)original + page_size;
    uintptr_t aligned = raw & ~(page_size - 1);
    ((void **)aligned)[-1] = original;

    return (void *)aligned;
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
        const char *msg = "Usage: mycat4 <filename>\n";
        write(STDERR_FILENO, msg, sizeof("Usage: mycat4 <filename>\n") - 1);
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    long bufsize = io_blocksize(fd);
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
            ssize_t written = write(STDOUT_FILENO, buffer + total_written, bytes_read - total_written);
            if (written == -1)
            {
                perror("write");
                align_free(buffer);
                close(fd);
                exit(EXIT_FAILURE);
            }
            total_written += written;
        }
    }

    if (bytes_read == -1)
    {
        perror("read");
    }

    align_free(buffer);
    close(fd);
    return (bytes_read == -1) ? EXIT_FAILURE : EXIT_SUCCESS;
}
