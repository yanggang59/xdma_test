#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include "debug.h"
#include "xdma_mod.h"
#include "libxdma_api.h"

void free_resource(struct debug_cdev* debug)
{
	if (debug->buf)
		kfree(debug->buf);	
	if (debug->class && debug->cdevno) {
		device_destroy(debug->class, debug->cdevno);
		class_destroy(debug->class);
		unregister_chrdev_region(debug->cdevno, 1);
	}
}

static int dma_write_test(struct debug_cdev* debug, int pos, char* buf, int length)
{
	printk("[DEBUG] dma test \r\n");
	int res;
	int i;
	bool dma_mapped;
	struct xdma_pci_dev* xpdev;
	struct xdma_dev *xdev;
	struct xdma_engine *engine;
	struct scatterlist *sg;
	unsigned int pages_nr;
	struct sg_table *sgt;

	sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);

	xpdev = container_of(debug, struct xdma_pci_dev, debug);
	xdev = xpdev->xdev;
	engine = &xdev->engine_h2c[0];
	pos = 0;
	dma_mapped = false;
	
	pages_nr = (((unsigned long)buf + length + PAGE_SIZE - 1) - ((unsigned long)buf & PAGE_MASK)) >> PAGE_SHIFT;
	if (pages_nr == 0)
		return -EINVAL;

	if (sg_alloc_table(sgt, pages_nr, GFP_KERNEL)) {
		pr_err("sgl OOM.\n");
		res = -ENOMEM;
		goto out;
	}
	sg = &sgt->sgl[0];
	for (i = 0; i < pages_nr; i++) {
		unsigned int offset = offset_in_page(buf);
		unsigned int nbytes = min_t(unsigned int, PAGE_SIZE - offset, length);
		sg_set_buf(sg, buf, nbytes);
		buf += nbytes;
		length -= nbytes;
		sg = sg_next(sg);
	}

	res = xdma_xfer_submit(xdev, engine->channel, 1, pos, sgt, dma_mapped, 0);

out:
	kfree(sgt);
	kfree(buf);
	return res;
}


static int debug_open(struct inode *inode, struct file *file)
{ 
    struct debug_cdev *debug;
	printk(KERN_INFO "\n%s: opened device", NAME);
    debug = container_of(inode->i_cdev, struct debug_cdev, cdev);
    file->private_data = debug;
	return 0;
}

static int debug_close(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "\n%s: device close", NAME);	
	return 0;
}

static ssize_t debug_read(struct file *file, char *dst, size_t count, loff_t *f_offset) 
{
#if 0
    struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	int ret;
	printk(KERN_INFO "\n%s: reading from device", NAME);
    printk("[Info] count = %ld, offset = %lld \r\n", count, *f_offset);
	ret = copy_to_user(dst, debug->buf + *f_offset, count);
	*f_offset += count;
	return count;
#else
	return count;
#endif
}

static ssize_t debug_write(struct file *file, const char *src, size_t count, loff_t *f_offset) 
{
#if 0
    struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	printk(KERN_INFO "\n%s: writing to device", NAME);
	if (copy_from_user(debug->buf, src, count) != 0)
		return -EFAULT;
	*f_offset += count;
	return count;
#else
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	printk("[DEBUG] dma write \r\n");
	int length = 4096;
	int pos = 0;
	char* buf = kmalloc(length, GFP_KERNEL);
	memset(buf, 'X', length);
	dma_write_test(debug, pos, buf, length);
	kfree(buf);
	return count;
#endif
}

/* Global structures */
static struct file_operations debug_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.release = debug_close,
	.write = debug_write,
	.read = debug_read
};

void delete_debug_cdev(struct debug_cdev* debug)
{
	printk(KERN_INFO "\n%s: free module", NAME);
    free_resource(debug);
}

int create_debug_cdev(struct debug_cdev* debug)
{
    if (alloc_chrdev_region(&debug->cdevno, 0, 1, NAME) < 0) {
		printk(KERN_ALERT "\n%s: failed to allocate a major number", NAME);
		return -ENOMEM;
	}

    // allocate a device class
	if ((debug->class = class_create(THIS_MODULE, NAME)) == NULL) {
		printk(KERN_ALERT "\n%s: failed to allocate class", NAME);
		free_resource(debug);
		return -ENOMEM;
	}
	// allocate a device file
	if (device_create(debug->class, NULL, debug->cdevno, NULL, NAME) == NULL) {
		printk(KERN_ALERT "\n%s: failed to allocate device file", NAME);
		free_resource(debug);
		return -ENOMEM;
	}	

	cdev_init(&debug->cdev, &debug_fops);
	debug->cdev.owner = THIS_MODULE;

	// allocates a buffer of size BUF_LENGTH
	if ((debug->buf = kcalloc(BUF_LENGTH, sizeof(char), GFP_KERNEL)) == NULL) {
		printk(KERN_ALERT "\n%s: failed to allocate buffer", NAME);
		free_resource(debug);
		return -ENOMEM;
	}
	// add device to the kernel 
	if (cdev_add(&debug->cdev, debug->cdevno, 1)) {
		printk(KERN_ALERT "\n%s: unable to add char device", NAME);
		free_resource(debug);
		return -ENOMEM;
	}

	printk(KERN_INFO "\n%s: loaded module", NAME);
	return 0;

}