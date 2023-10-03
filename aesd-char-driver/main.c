/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesd-circular-buffer.h"
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Doug Weber");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;

    PDEBUG("open");

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct aesd_dev *dev = filp->private_data; 
    struct aesd_buffer_entry *entry;
    size_t entry_offset;
    ssize_t retval = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset);

    if (!entry)
		goto out;

    if ((entry->size - entry_offset) < count) {
        count = entry->size - entry_offset;
    }

	if (copy_to_user(buf, entry->buffptr + entry_offset, count)) {
		retval = -EFAULT;
		goto out;
	}

	*f_pos += count;
    retval = count;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct aesd_dev *dev = filp->private_data; 
    ssize_t retval = -ENOMEM;
    char *buffptr_new;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    // Allocate or reallocate depending if add entry contains unterminated commands.

    if (!dev->add_entry.buffptr) {
        buffptr_new = kmalloc(count, GFP_KERNEL);
    } else {
        buffptr_new = krealloc(dev->add_entry.buffptr, dev->add_entry.size + count, GFP_KERNEL);
    }

    if (!buffptr_new)
        goto out;

    if (copy_from_user(buffptr_new + dev->add_entry.size, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    dev->add_entry.buffptr = buffptr_new;
    dev->add_entry.size = dev->add_entry.size + count;

    // If terminated, write to buffer

    if (dev->add_entry.buffptr[dev->add_entry.size-1] == '\n') {

        // Free next entry if buffer is already full

        if (dev->buffer.full) {
            kfree(dev->buffer.entry[dev->buffer.in_offs].buffptr);
            dev->buffer_size -= dev->buffer.entry[dev->buffer.in_offs].size;
        } else {
            dev->buffer_len++;
        }

        aesd_circular_buffer_add_entry(&dev->buffer, &dev->add_entry);
        dev->buffer_size += dev->add_entry.size;

        dev->add_entry.buffptr = NULL;
        dev->add_entry.size = 0;
    }

	*f_pos += count;
    retval = count;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
	struct aesd_dev *dev = filp->private_data; 
    loff_t retval;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    retval = fixed_size_llseek(filp, off, whence, dev->buffer_size);

    mutex_unlock(&dev->lock);

    return retval;
}

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
	struct aesd_dev *dev = filp->private_data; 
    struct aesd_buffer_entry *entry;
    unsigned int i;
    long retval = 0;
    size_t start_offset = 0;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    if (write_cmd >= dev->buffer_len) {
        retval = -EINVAL;
        goto out;
    }

    entry = &dev->buffer.entry[(dev->buffer.out_offs + write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED];

    if (write_cmd_offset >= entry->size) {
        retval = -EINVAL;
        goto out;
    }

    for (i = dev->buffer.out_offs;
            i != ((dev->buffer.out_offs + write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED);
            i = (i + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
    {
        start_offset += dev->buffer.entry[i].size;
    }

    filp->f_pos = start_offset + write_cmd_offset;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long retval;

    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC)
        return -ENOTTY;

    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
        return -ENOTTY;

    if (!access_ok((void __user *)arg, _IOC_SIZE(cmd)))
        return -EFAULT;

    switch (cmd) {

        case AESDCHAR_IOCSEEKTO:
        {
            struct aesd_seekto seekto;
            if (copy_from_user(&seekto, (const void __user*)arg, sizeof(seekto)) != 0) {
                retval = -EFAULT;
            } else {
                retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
            }

            break;
        }

        default:
            return -ENOTTY;
    }

    return retval;
}

struct file_operations aesd_fops = {
    .owner =            THIS_MODULE,
    .read =             aesd_read,
    .write =            aesd_write,
    .llseek =           aesd_llseek,
    .unlocked_ioctl =   aesd_unlocked_ioctl,
    .open =             aesd_open,
    .release =          aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

    memset(&aesd_device,0,sizeof(struct aesd_dev));

    aesd_circular_buffer_init(&aesd_device.buffer);

    memset(&aesd_device.add_entry, 0, sizeof(struct aesd_buffer_entry));

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if (result) {
        unregister_chrdev_region(dev, 1);
    }

    return result;
}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
        kfree(entry->buffptr);
    }

    // Free any unterminated commands

    if (aesd_device.add_entry.buffptr) {
        kfree(aesd_device.add_entry.buffptr);
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
