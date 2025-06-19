// mycat1.c
#include <unistd.h> // for read, write, close
#include <fcntl.h>  // for open
#include <stdio.h>  // for perror
#include <stdlib.h> // for exit

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        const char *msg = "Usage: mycat1 <filename>\n";
        write(STDERR_FILENO, msg, sizeof("Usage: mycat1 <filename>\n") - 1);
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char c;
    ssize_t bytes_read;
    while ((bytes_read = read(fd, &c, 1)) > 0)
    {
        if (write(STDOUT_FILENO, &c, 1) != 1)
        {
            perror("write");
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_read == -1)
    {
        perror("read");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (close(fd) == -1)
    {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return 0;
}
