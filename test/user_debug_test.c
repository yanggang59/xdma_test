#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#define DBG_CHAR_DEV "/dev/debug"

void dump_buf(char* buf, int len)
{
  printf("************************************* DUMP BUF START *************************************************\r\n");
  printf("     ");
  for(int i = 0; i < 16; i++) 
    printf("%4X ", i);

  for(int j = 0; j < len; j++) {
    if(j % 16 == 0) {
      printf("\n%4X ", j);
    }
    printf("%4X ", buf[j]);
  }
  printf("\n************************************ DUMP BUF END **************************************************\r\n");
}

int main()
{
    int fd;
    void* buf;
    int length = 2048;
    buf = malloc(length);
    memset(buf, 0, length);
    if(!buf) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        exit(-1);
    }
    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open 0: %s\n", strerror(errno));
        exit(-1);
    }
    write(fd, buf, length);
    close(fd);
    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open 1: %s\n", strerror(errno));
        exit(-1);
    }
    memset(buf, 0, length);
    read(fd, buf, length);
    dump_buf(buf, length);
    close(fd);
    return 0;
}
