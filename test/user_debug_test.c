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
#define IOCTL_HACK_CTL_ON                _IO(IOCTL_MAGIC, 5)
#define IOCTL_HACK_CTL_OFF               _IO(IOCTL_MAGIC, 6)
#define IOCTL_HOLD_IC_ON                 _IO(IOCTL_MAGIC, 7)
#define IOCTL_HOLD_IC_OFF                _IO(IOCTL_MAGIC, 8)

void dump_usr_buf(unsigned char* buf, int len, char* info)
{
  printf("************************************* DUMP %s BUF START *************************************************\r\n", info);
  printf("     ");
  for(int i = 0; i < 16; i++) 
    printf("%4X ", i);

  for(int j = 0; j < len; j++) {
    if(j % 16 == 0) {
      printf("\n%4X ", j);
    }
    printf("%4X ", buf[j]);
  }
  printf("\n************************************ DUMP %s BUF END **************************************************\r\n", info);
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

#if defined(RND_RW_TEST) || defined(HOLD_IC_CTL_ON) || defined(HOLD_IC_CTL_OFF)
static void hold_ic_ctl_switch(int flag)
{
    int fd, ret;
    printf("hold_ic_ctl_switch switch \r\n");
    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open: %s\n", strerror(errno));
        exit(-1);
    }
    if(flag) {
        printf("hold_ic_ctl_switch on \r\n");
        ret = ioctl(fd, IOCTL_HOLD_IC_ON, NULL);
    } else {
        printf("hold_ic_ctl_switch off \r\n");
        ret = ioctl(fd, IOCTL_HOLD_IC_OFF, NULL);
    }
    if(ret < 0) {
        fprintf(stderr, "ioctl: %s\n", strerror(errno));
        exit(-1);
    }
    close(fd);
}
#endif

#ifdef RND_RW_TEST 
static int random_read_write_test()
{
    int fd;
    unsigned char* w_buf;
    unsigned char* r_buf;
    int len = 100;
    int ret;
    printf("[Info] hold ic switch on\r\n");
    hold_ic_ctl_switch(1);

    w_buf = malloc(4096);
    memset(w_buf, 0, 4096);
    if(!w_buf) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        return -ENOMEM;
    }
    fd = open("./test8K_ramdom", O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open: %s\n", strerror(errno));
        exit(-1);
    }
    read(fd, w_buf, 4096);
    dump_usr_buf(w_buf, 1024, "write");
    close(fd);

    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open 1: %s\n", strerror(errno));
        exit(-1);
    }
    write(fd, w_buf, len);
    close(fd);

    r_buf = malloc(4096);
    memset(r_buf, 0, 4096);
    if(!r_buf) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        return -ENOMEM;
    }
    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open 2: %s\n", strerror(errno));
        exit(-1);
    }
    read(fd, r_buf, len);
    dump_usr_buf(r_buf, 1024, "read");
    close(fd);

    ret = memcmp(w_buf, r_buf, len);
    if(!ret) {
        printf("read write buf is the same \r\n");
    } else {
        printf("read write buf is not the same, ret = %d \r\n", ret);
    }
    printf("[Info] hold ic switch off\r\n");
    hold_ic_ctl_switch(0);
}
#endif

#ifdef READ_WRITE_TEST
static void read_write_test(char cont)
{
    int fd;
    unsigned char* w_buf;
    unsigned char* r_buf;
    int length = 44, buf_len = 2048;
    w_buf = malloc(buf_len);
    r_buf = malloc(buf_len);
    memset(w_buf, 0, buf_len);
    memset(w_buf, cont, length);

    dump_usr_buf(w_buf, length, "write");

    memset(r_buf, 0, buf_len);
    if((!w_buf) || (!r_buf)) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        exit(-1);
    }
    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open 0: %s\n", strerror(errno));
        exit(-1);
    }
    write(fd, w_buf, length);
    close(fd);
    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open 1: %s\n", strerror(errno));
        exit(-1);
    }
    read(fd, r_buf, length);
    dump_usr_buf(r_buf, length, "read");
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

static void hack_control_switch(int flag)
{
    int fd, ret;
    printf("hack control switch \r\n");
    fd = open(DBG_CHAR_DEV, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "open: %s\n", strerror(errno));
        exit(-1);
    }
    if(flag) {
        printf("hack ctl on \r\n");
        ret = ioctl(fd, IOCTL_HACK_CTL_ON, NULL);
    } else {
        printf("hack ctl off \r\n");
        ret = ioctl(fd, IOCTL_HACK_CTL_OFF, NULL);
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

#ifdef HACK_CTL_ON
    hack_control_switch(1);
#endif

#ifdef HACK_CTL_OFF
    hack_control_switch(0);
#endif

#ifdef RND_RW_TEST 
    random_read_write_test();
#endif

#ifdef HOLD_IC_CTL_ON
    hold_ic_ctl_switch(1);
#endif

#ifdef HOLD_IC_CTL_OFF
    hold_ic_ctl_switch(0);
#endif
    return 0;
}
