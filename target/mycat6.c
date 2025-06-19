#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/mman.h>

// 获取缓冲区大小（由 mycat5 推导出的实验性最佳大小）
size_t io_blocksize(int fd)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
        page_size = 4096;

    struct stat st;
    size_t file_block_size = 4096; // fallback 默认
    if (fstat(fd, &st) == 0 && st.st_blksize > 0 && (st.st_blksize & (st.st_blksize - 1)) == 0)
    {
        file_block_size = st.st_blksize;
    }

    // 实验发现最佳缓冲区为 page_size * 32
    size_t base = page_size > file_block_size ? page_size : file_block_size;
    return base * 128;
}

// 分配页对齐的缓冲区
void *align_alloc(size_t size)
{
    void *ptr = NULL;
    if (posix_memalign(&ptr, sysconf(_SC_PAGESIZE), size) != 0)
    {
        return NULL;
    }
    return ptr;
}

void align_free(void *ptr)
{
    free(ptr);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        return EXIT_FAILURE;
    }

    // 使用 fadvise 提示顺序访问
    if (posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL) != 0)
    {
        perror("posix_fadvise");
        // 非致命错误，继续运行
    }

    size_t buf_size = io_blocksize(fd);
    char *buf = align_alloc(buf_size);
    if (!buf)
    {
        perror("align_alloc");
        close(fd);
        return EXIT_FAILURE;
    }

    ssize_t n;
    while ((n = read(fd, buf, buf_size)) > 0)
    {
        ssize_t written = 0;
        while (written < n)
        {
            ssize_t w = write(STDOUT_FILENO, buf + written, n - written);
            if (w < 0)
            {
                perror("write");
                align_free(buf);
                close(fd);
                return EXIT_FAILURE;
            }
            written += w;
        }
    }

    if (n < 0)
    {
        perror("read");
    }

    align_free(buf);
    close(fd);
    return n < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
