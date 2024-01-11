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
#define IOCTL_DUMP_MSG_ON                _IO(IOCTL_MAGIC, 3)
#define IOCTL_DUMP_MSG_OFF               _IO(IOCTL_MAGIC, 4)

void dump_usr_buf(char* buf, int len)
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

#ifdef SIMPLE_IO_TEST
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
    close(fd);
}
#endif

#ifdef READ_WRITE_TEST
static void read_write_test(char cont)
{
    int fd;
    void* buf;
    int length = 44, buf_len = 2048;
    buf = malloc(buf_len);
    memset(buf, 0, buf_len);
    memset(buf, cont, length);
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
    dump_usr_buf(buf, buf_len);
    close(fd);

    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open 2: %s\n", strerror(errno));
        exit(-1);
    }
    access_address = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("dump info addr start \r\n");
    dump_usr_buf(access_address, 4096);
    printf("dump info addr done \r\n");
    close(fd);
}
#endif

static void dump_msg_switch(int flag)
{
    int fd, ret;
    printf("dump msg switch \r\n");
    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open: %s\n", strerror(errno));
        exit(-1);
    }
    if(flag) {
        printf("dump msg on \r\n");
        ret = ioctl(fd, IOCTL_DUMP_MSG_ON, NULL);
    } else {
        printf("dump msg off \r\n");
        ret = ioctl(fd, IOCTL_DUMP_MSG_OFF, NULL);
    }
    if(ret < 0) {
        fprintf(stderr, "ioctl: %s\n", strerror(errno));
        exit(-1);
    }
    close(fd);
}

int main()
{
#ifdef READ_WRITE_TEST
    read_write_test('K');
#endif

#ifdef SIMPLE_IO_TEST
    simple_io_test();
#endif

#ifdef DUMP_MSG_ON
    dump_msg_switch(1);
#endif

#ifdef DUMP_MSG_OFF
    dump_msg_switch(0);
#endif
    return 0;
}
