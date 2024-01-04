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

void dump_buf(char* buf, int len)
{
	int i,j;
    printk("**************************************************************************************\r\n");
    printk("     ");
    for(i = 0; i < 16; i++) 
        printk("%4X ", i);

    for(j = 0; j < len; j++) {
    	if(j % 16 == 0) {
      	printk("\n%4X ", j);
    }
    printk("%4X ", buf[j]);
  }

  printk("\n**************************************************************************************\r\n");
}

static int dma_to_device(struct debug_cdev* debug, int pos, char* buf, int length)
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
	NUPA_DEBUG("dma_to_device \r\n");
	sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);

	adapter = container_of(debug, struct nupanet_adapter, debug);
	xdev = adapter->xdev;
	engine = &xdev->engine_h2c[0];
	pos = 0;
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

	res = xdma_xfer_submit(xdev, engine->channel, 1, pos, sgt, dma_mapped, 0);
out:
	kfree(sgt);
	return res;
}

static int dma_from_device(struct debug_cdev* debug, int pos, char* buf, int length)
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
	NUPA_DEBUG("dma read test \r\n");
	sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);

	adapter = container_of(debug, struct nupanet_adapter, debug);
	xdev = adapter->xdev;
	engine = &xdev->engine_c2h[0];
	pos = 0;
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

	res = xdma_xfer_submit(xdev, engine->channel, 0, pos, sgt, dma_mapped, 0);

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

static ssize_t debug_read(struct file *file, char *dst, size_t count, loff_t *f_offset) 
{
#if 1
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	NUPA_DEBUG("dma read: count = %ld, offset = %lld \r\n", count, *f_offset);
	if(*f_offset + count > debug->buf_size) {
		NUPA_ERROR("over read debug file\r\n");
		return -EINVAL;
	}
	dma_from_device(debug, *f_offset, debug->buf, count);
	if(copy_to_user(dst, debug->buf + *f_offset, count))
		return -EFAULT;
	*f_offset += count;
	return count;
#else
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	int length = 4096;
	int pos = 0;
	char* buf = kmalloc(length, GFP_KERNEL);
	NUPA_DEBUG("dma read \r\n");
	dma_from_device(debug, pos, buf, length);
	dump_buf(buf, length);
	kfree(buf);
	return count;
#endif
}

static ssize_t debug_write(struct file *file, const char *src, size_t count, loff_t *f_offset) 
{
#if 1
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	NUPA_DEBUG("dma write: count = %ld, offset = %lld \r\n", count, *f_offset);
	if(*f_offset + count > debug->buf_size) {
		NUPA_ERROR("over write debug file\r\n");
		return -EINVAL;
	}
	if (copy_from_user(debug->buf, src, count) != 0)
		return -EFAULT;
	dma_to_device(debug, *f_offset, debug->buf, count);
	*f_offset += count;
	return count;
#else
	struct debug_cdev *debug = (struct debug_cdev *)file->private_data;
	int length = 4096;
	int pos = 0;
	char* buf = kmalloc(length, GFP_KERNEL);
	NUPA_DEBUG("dma write \r\n");
	memset(buf, 'X', length);
	dma_to_device(debug, pos, buf, length);
	kfree(buf);
	return count;
#endif
}

static long debug_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	NUPA_DEBUG("debug_ioctl , cmd = %#x, arg = %#lx\r\n", cmd, arg);
	return 0;
}

static int debug_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct debug_cdev *debug;
	unsigned long start, size;
	start = (unsigned long)vma->vm_start;
    size = (unsigned long)(vma->vm_end - vma->vm_start);
	debug = (struct debug_cdev *)filp->private_data;
	if(remap_pfn_range(vma, start, virt_to_phys(debug->buf) >> PAGE_SHIFT, size, PAGE_SHARED))
         return -1;
	return 0;
}

static struct file_operations debug_fops = {
	.owner = THIS_MODULE,
	.open = debug_open,
	.mmap = debug_mmap,
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

	debug->buf_size = BUF_LENGTH;
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
