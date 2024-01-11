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
#include "nupanet_main.h"
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

static int dma_xfer_data(struct debug_cdev* debug, int pos, char* buf, int length, bool is_h2c)
{
	int res;
	int i;
	bool dma_mapped;
	struct nupanet_adapter* adapter;
	struct xdma_dev *xdev;
	struct xdma_engine *engine;
	struct scatterlist *sg;
	unsigned int pages_nr;
	struct sg_table *sgt;
	if(is_h2c) {
		NUPA_DEBUG("dma_to_device \r\n");
	} else {
		NUPA_DEBUG("dma_from_device \r\n");
	}
	sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);

	adapter = container_of(debug, struct nupanet_adapter, debug);
	xdev = adapter->xdev;
	if(is_h2c)
		engine = &xdev->engine_h2c[0];
	else
		engine = &xdev->engine_c2h[0];

	dma_mapped = false;
	
	pages_nr = (((unsigned long)buf + length + PAGE_SIZE - 1) - ((unsigned long)buf & PAGE_MASK)) >> PAGE_SHIFT;
	if (pages_nr == 0)
		return -EINVAL;

	if (sg_alloc_table(sgt, pages_nr, GFP_KERNEL)) {
		NUPA_ERROR("sgl OOM.\n");
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

	res = xdma_xfer_submit(xdev, engine->channel, is_h2c, pos, sgt, dma_mapped, 0);
out:
	kfree(sgt);
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

static void do_raw_read_test(struct file *file)
{
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	int length = 4096;
	int pos = 0;
	int res;
	char* buf = kmalloc(length, GFP_KERNEL);
	NUPA_DEBUG("dma raw read \r\n");
	res = dma_xfer_data(debug, pos, buf, length, false);
	NUPA_DEBUG("dma raw read done , res = %d \r\n", res);
	kfree(buf);
}

static void do_raw_write_test(struct file *file, char content)
{
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	int length = 4096;
	int pos = 0;
	int res;
	char* buf = kmalloc(length, GFP_KERNEL);
	NUPA_DEBUG("dma raw write \r\n");
	memset(buf, content, length);
	res = dma_xfer_data(debug, pos, buf, length, true);
	NUPA_DEBUG("dma raw write done , res = %d \r\n", res);
	kfree(buf);
}


static ssize_t debug_read(struct file *file, char *dst, size_t count, loff_t *f_offset) 
{
	int res;
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	NUPA_DEBUG("debug_read: count = %ld, offset = %lld \r\n", count, *f_offset);
	if(*f_offset + count > debug->buf_size) {
		NUPA_ERROR("over read debug file\r\n");
		return -EINVAL;
	}
	res = dma_xfer_data(debug, *f_offset, debug->buf, count, false);
	if(copy_to_user(dst, debug->buf + *f_offset, count))
		return -EFAULT;
	*f_offset += count;
	NUPA_DEBUG("debug_read done : count = %ld, offset = %lld , res = %d \r\n", count, *f_offset, res);
	return count;
}

static ssize_t debug_write(struct file *file, const char *src, size_t count, loff_t *f_offset) 
{
	int res;
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	NUPA_DEBUG("debug_write: count = %ld, offset = %lld \r\n", count, *f_offset);
	if(*f_offset + count > debug->buf_size) {
		NUPA_ERROR("over write debug file\r\n");
		return -EINVAL;
	}
	if (copy_from_user(debug->buf, src, count) != 0)
		return -EFAULT;
	res = dma_xfer_data(debug, *f_offset, debug->buf, count, true);
	*f_offset += count;
	NUPA_DEBUG("debug_write done : count = %ld, offset = %lld , res = %d \r\n", count, *f_offset, res);
	return count;
}

static long debug_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	NUPA_DEBUG("debug_ioctl , cmd = %#x, arg = %#lx\r\n", cmd, arg);
	switch(cmd)
	{
		case IOCTL_RAW_WRITE:
			NUPA_DEBUG("debug_ioctl raw write test \r\n");
			do_raw_write_test(file, 'X');
			break;

		case IOCTL_RAW_READ:
			NUPA_DEBUG("debug_ioctl raw read test \r\n");
			do_raw_read_test(file);
			break;

		case IOCTL_DUMP_MSG_ON:
			NUPA_DEBUG("debug_ioctl dump msg on\r\n");
			debug->dump_ctl = 1;
			break;

		case IOCTL_DUMP_MSG_OFF:
			NUPA_DEBUG("debug_ioctl dump msg off\r\n");
			debug->dump_ctl = 0;
			break;

		case IOCTL_HACK_CTL_ON:
			NUPA_DEBUG("debug_ioctl hack control on \r\n");
			debug->hack_ctl = 1;
			break;

		case IOCTL_HACK_CTL_OFF:
			NUPA_DEBUG("debug_ioctl hack control on \r\n");
			debug->hack_ctl = 0;
			break;

		case IOCTL_HOLD_IC_ON:
			NUPA_DEBUG("debug_ioctl hold ic control on \r\n");
			debug->hold_ic_ctl = 1;
			break;
		
		case IOCTL_HOLD_IC_OFF:
			NUPA_DEBUG("debug_ioctl hold ic control off \r\n");
			debug->hold_ic_ctl = 0;
			break;

		default:
			break;
	}
	return 0;
}

#if DEBUG_USE_MMAP
static int debug_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct debug_cdev *debug;
	unsigned long start, size;
	start = (unsigned long)vma->vm_start;
    size = (unsigned long)(vma->vm_end - vma->vm_start);
	debug = (struct debug_cdev *)filp->private_data;
	NUPA_DEBUG("debug_mmap , size = %#lx \r\n", size);
	if(size >= debug->info_len) {
		size = debug->info_len;
	}
	if(remap_pfn_range(vma, start, virt_to_phys(debug->info_buf) >> PAGE_SHIFT, size, PAGE_SHARED)) {
		NUPA_ERROR("debug_mmap failed\r\n");
		return -1;
	}
	return 0;
}
#endif

static struct file_operations debug_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
#if DEBUG_USE_MMAP
	.mmap = debug_mmap,
#endif
	.release = debug_close,
	.write = debug_write,
	.read = debug_read,
	.unlocked_ioctl = debug_ioctl,
};

void delete_debug_cdev(struct debug_cdev* debug)
{
	printk(KERN_INFO "\n%s: free module", NAME);
    free_resource(debug);
}

int create_debug_cdev(struct debug_cdev* debug, char* info_buf, int info_len)
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

	debug->buf_size = BUF_LENGTH;
	// allocates a buffer of size BUF_LENGTH
	if ((debug->buf = kcalloc(BUF_LENGTH, sizeof(char), GFP_KERNEL)) == NULL) {
		printk(KERN_ALERT "\n%s: failed to allocate buffer", NAME);
		free_resource(debug);
		return -ENOMEM;
	}

	if(info_buf && info_len) {
		debug->info_buf = info_buf;
		debug->info_len = info_len;
	}

	debug->dump_ctl = 0;
	debug->hack_ctl = 0;
	debug->hold_ic_ctl = 0;

	// add device to the kernel 
	if (cdev_add(&debug->cdev, debug->cdevno, 1)) {
		printk(KERN_ALERT "\n%s: unable to add char device", NAME);
		free_resource(debug);
		return -ENOMEM;
	}

	printk(KERN_INFO "\n%s: loaded module", NAME);
	return 0;

}
