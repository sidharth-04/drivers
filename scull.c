#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/errorno.h>

struct scull_qset {
	scull_qset *next;
	void **data;
};

struct scull_dev = {
	struct scull_qset *data;
	int quantum;
	int qset;
	unsigned long size;
	unsigned int access_key;
	struct semaphore sem;
	struct cdev cdev;
};

int scull_trim(struct scull_dev *dev) {
	struct scull_qset *next, *dtpt;
	int qset = dev->qset;
	for (dtpt = dev->data; dtpt; dtpt = next) {
		for (int i = 0; i < qset; i ++) {
			kfree(dtpt->data[i]);
		}
		kfree(dtpt->data);
		next = dtpt->next;
		kfree(dtpt);
	}
	dev->size = 0;
	dev->data = NULL;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	return 0;
}

int scull_open(struct inode *inode, struct file *filp) {
	struct scull_dev *dev
	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev;
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		scull_trim(dev);
	}
	return 0;
}

int scull_release(struct inode *inode, struct file *filp) {
	scull_trim(dev);	
	return 0;
}

ssize_t scull_read(struct file *filp, char __user *buff, size_t count, loff_t *offp) {
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dtpt;
	int quantum = dev->quantum, qset = dev->qset;
	int item_size = quantum * qset;
	
	int item, s_pos, q_pos, rest;
	ssize_t ret_val = 0;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
	if (*offp > dev->size) goto out;
	if (*offp + count > dev->size) count = dev->size - *offp;
	item = (long)*offp / itemsize;
	rest = (long)*offp % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	dtpt = scull_follow(dev, item);
	if (dtpt == NULL || !dtpt->data || !dtpt->data[s_pos]) goto out;
	if (count > quantum - q_pos) count = quantum - q_pos;
	if (copy_to_user(buf, dtpt->data[s_pos] + q_pos, count)) {
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;
out:
	up(&dev->sem);
	return retval;
}

ssize_t scull_write(struct file *filp, char __user *buff, size_t count, loff_t *f_pos) {
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dtpt;
	int quantum = dev->quantum, qset = dev->qset;
	int item_size = quantum * qset;

	int item, s_pos, q_pos, rest;
	ssize_t ret_val = -ENOMEM;

	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
	item = (long)*offp / itemsize;
	rest = (long)*offp % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	dtpt = scull_follow(dev, item);
	if (dtpt == NULL) goto out;
	if (!dtpt->data) {
		dtpt->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
		if (!dtpt->data) goto out;
		memset(dptpt->data, 0, qset * sizeof(char *));
	}

	if (!dtpt->data[s_pos]) {
		dtpt->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if (!dtpt->data[s_pos]) goto out;
	}

	if (count > quantum - q_pos) count = quantum - q_pos;
	if (copy_from_user(dtpt->data[s_pos] + q_pos, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
	retval = count;
	if (dev->size < *f_pos) dev->size = *f_pos;
out:
	up(&dev->sem);
	return retval;
}

void scull_setup_cdev(struct scull_dev *dev, int index) {
	int err, devno = MKDEV(scull_major, scull_minor + index);
	struct file_operations scull_fopsj = {
		.owner = THIS_MODULE,
		.read = scull_read,
		.open = scull_open,
		.release = scull_release
	};
	cdev_init(&dev->cdev, &scull_fops);
	dev->owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err) printk(KERN_NOTICE "Error %d adding scull%d", err, index);

int scull_init(void) {
	dev_t dev;
	alloc_chrdev_region(&dev, 0, 4, "scull");
	if (register_chrdev_region(dev, 4, "scull") != 0) return 1;
	cdev *scull_dev = cdev_alloc();	
	return 0;
}

void scull_exit(void) {
	scull_trim(dev);	
	unregister_chrdev_region(dev, 4);
}

module_init(scull_init);
moudle_exit(scull_exit);
