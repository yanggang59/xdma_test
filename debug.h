#ifndef __DEBUG_CDEV__
#define __DEBUG_CDEV__

#include <linux/cdev.h>
#include <linux/ioctl.h>


//CMD         31:30       29:16          15:8            7:0
//meaning     dir         data size      device type     function code
//bit         2           14             8               8
#define IOCTL_MAGIC                      ('D')
#define IOCTL_RAW_WRITE                  _IO(IOCTL_MAGIC, 1)
#define IOCTL_RAW_READ                   _IO(IOCTL_MAGIC, 2)
#define IOCTL_DUMP_MSG_ON                _IO(IOCTL_MAGIC, 3)
#define IOCTL_DUMP_MSG_OFF               _IO(IOCTL_MAGIC, 4)

#define NAME                          "debug"
#define BUF_LENGTH                    (8 << 12)

#define DEBUG_USE_MMAP                1


struct debug_cdev{
    dev_t cdevno;
    struct class *class;
    char *buf;
    int buf_size;
    struct cdev cdev;

    char* info_buf;
    int info_len;

    int dump_ctl;
};

void delete_debug_cdev(struct debug_cdev* debug);
int create_debug_cdev(struct debug_cdev* debug, char* info_buf, int info_len);

#endif
