#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#define CHAR_DEV "/dev/debug"

int main()
{
    int fd;
    void* buf;
    int length = 4096;
    buf = malloc(length);
    if(!buf) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        exit(-1);
    }
    fd = open(CHAR_DEV, O_RDWR);
    if( fd < 0) {
        fprintf(stderr, "open: %s\n", strerror(errno));
        exit(-1);
    }
    write(fd, buf, sizeof(buf));
    read(fd, buf, sizeof(buf));
    close(fd);
    return 0;
}