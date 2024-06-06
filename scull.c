#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <asm/uaccess.h>

#include "scull.h"

MODULE_AUTHOR("Sidharth Roy");
MODULE_LICENSE("Dual BSD/GPL");

#define SCULL_QUANTUM 4000
#define SCULL_QSET 1000

struct scull_qset {
	void **data;
	struct scull_qset *next;
};

struct scull_dev {
	struct scull_qset *data;
	int quantum;
	int qset;
	unsigned long size;
	unsigned int access_key;
	struct semaphore sem;
	struct cdev cdev;
};

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;

struct scull_dev *scull_devices;

int scull_trim(struct scull_dev *dev) {
	struct scull_qset *next, *dptr;
	int qset = dev->qset;
	int i;
	for (dptr = dev->data; dptr; dptr = next) {
		if (dptr->data) {
			for (i = 0; i < qset; i ++) kfree(dptr->data[i]);
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		kfree(dptr);
	}
	dev->size = 0;
	dev->quantum = SCULL_QUANTUM;
	dev->qset = SCULL_QSET;
	dev->data = NULL;
	return 0;
}

int scull_open(struct inode *inode, struct file *filp) {
	struct scull_dev *dev;
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		scull_trim(dev);
		up(&dev->sem);
	}
	return 0;
}

int scull_release(struct inode *inode, struct file *filp) {
	return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int n) {
	struct scull_qset *qs = dev->data;
	if (!qs) {
		qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if (qs == NULL) return NULL;
		memset(qs, 0, sizeof(struct scull_qset));
	}

	while (n --) {
		if (!qs->next) {
			qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if (qs->next == NULL) return NULL;
			memset(qs->next, 0, sizeof(struct scull_qset));
		}
		qs = qs->next;
	}
	return qs;
}

ssize_t scull_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos) {
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;
	int quantum = dev->quantum, qset = dev->qset;
	int item_size = quantum * qset;
	
	int item, s_pos, q_pos, rest;
	ssize_t ret_val = 0;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
	if (*f_pos > dev->size) goto out;
	if (*f_pos + count > dev->size) count = dev->size - *f_pos;
	item = (long)*f_pos / item_size;
	rest = (long)*f_pos % item_size;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	dptr = scull_follow(dev, item);
	if (dptr == NULL || !dptr->data || !dptr->data[s_pos]) goto out;
	if (count > quantum - q_pos) count = quantum - q_pos;
	if (copy_to_user(buff, dptr->data[s_pos] + q_pos, count)) {
		ret_val = -EFAULT;
		goto out;
	}
	*f_pos += count;
	ret_val = count;
out:
	up(&dev->sem);
	return ret_val;
}

ssize_t scull_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos) {
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;
	int quantum = dev->quantum, qset = dev->qset;
	int item_size = quantum * qset;

	int item, s_pos, q_pos, rest;
	ssize_t ret_val = -ENOMEM;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
	item = (long)*f_pos / item_size;
	rest = (long)*f_pos % item_size;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	dptr = scull_follow(dev, item);
	if (dptr == NULL) goto out;
	if (!dptr->data) {
		dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
		if (!dptr->data) goto out;
		memset(dptr->data, 0, qset * sizeof(char *));
	}

	if (!dptr->data[s_pos]) {
		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if (!dptr->data[s_pos]) goto out;
	}

	if (count > quantum - q_pos) count = quantum - q_pos;
	if (copy_from_user(dptr->data[s_pos] + q_pos, buff, count)) {
		ret_val = -EFAULT;
		goto out;
	}

	*f_pos += count;
	ret_val = count;
	if (dev->size < *f_pos) dev->size = *f_pos;
out:
	up(&dev->sem);
	return ret_val;
}

void scull_exit(void) {
	int i;
	dev_t devno = MKDEV(scull_major, scull_minor);
	if (scull_devices) {
		for (i = 0; i < scull_nr_devs; i ++) {
			scull_trim(scull_devices + i);
			cdev_del(&scull_devices[i].cdev);
		}
		kfree(scull_devices);
	}
	unregister_chrdev_region(devno, scull_nr_devs);
}

struct file_operations scull_fops = {
	.owner = THIS_MODULE,
	.read = scull_read,
	.write = scull_write,
	.open = scull_open,
	.release = scull_release
};

void scull_setup_cdev(struct scull_dev *dev, int index) {
	int err, devno = MKDEV(scull_major, scull_minor + index);	
	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops; // Doesn't make sense
	err = cdev_add(&dev->cdev, devno, 1); // Device is now live
	if (err) printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

int scull_init(void) {
	int result, i;
	dev_t dev = 0;
	result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs, "scull");
	scull_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
	if (!scull_devices) {
		result = -ENOMEM;
		goto fail;
	}
	memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

	for (int i = 0; i < scull_nr_devs; i ++) {
		scull_devices[i].qset = scull_qset;
		scull_devices[i].quantum = scull_quantum;
		sema_init(&scull_devices[i].sem, 1);
		scull_setup_cdev(&scull_devices[i], i);
	}

	dev = MKDEV(scull_major, scull_minor + scull_nr_devs);

	return 0;

fail:
	scull_exit();
	return result;
}

module_init(scull_init);
module_exit(scull_exit);
