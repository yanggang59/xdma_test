#ifndef __DEBUG_CDEV__
#define __DEBUG_CDEV__

#include <linux/cdev.h>


#define IOCTL_MAGIC              ('f')
#define IOCTL_ADDR               _IOR(IOCTL_MAGIC, 1, u32)

#define NAME                     "debug"
#define BUF_LENGTH               1024


struct debug_cdev{
    dev_t cdevno;
    struct class *class;
    char *buf;
    struct cdev cdev;
};

void delete_debug_cdev(struct debug_cdev* debug);
int create_debug_cdev(struct debug_cdev* debug);

#endif