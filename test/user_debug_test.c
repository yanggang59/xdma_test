#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#define DBG_CHAR_DEV "/dev/debug"

#define IOCTL_MAGIC                   ('D')
#define IOCTL_RAW_WRITE               _IO(IOCTL_MAGIC, 1)
#define IOCTL_RAW_READ                _IO(IOCTL_MAGIC, 2)

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


static void simple_io_test(void)
{
	int fd, ret;
    fd = open(DBG_CHAR_DEV, O_RDWR);
	if(fd < 0) {
        fprintf(stderr, "open 2: %s\n", strerror(errno));
        exit(-1);
    }
	ret = ioctl(fd, IOCTL_RAW_WRITE, NULL);
	ret = ioctl(fd, IOCTL_RAW_READ, NULL);

}

int main()
{
    int fd;
    void* buf;
    int length = 44, buf_len = 2048;
    buf = malloc(buf_len);
    memset(buf, 0, buf_len);
    memset(buf, 'K', length);
    char* access_address;
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
    memset(buf, 0, buf_len);
    read(fd, buf, length);
    dump_buf(buf, buf_len);
    close(fd);

    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open 2: %s\n", strerror(errno));
        exit(-1);
    }
    access_address = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("dump info addr start \r\n");
    dump_buf(access_address, 4096);
    printf("dump info addr done \r\n");
    close(fd);
	  simple_io_test();
    return 0;
}
