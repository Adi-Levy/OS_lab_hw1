/* pacman.c: Example char device module.
 *
 */
/* Kernel Programming */
#define MODULE
#define LINUX
#define __KERNEL__

#include <linux/kernel.h>  	
#include <linux/module.h>
#include <linux/fs.h>       		
#include <asm/uaccess.h>
#include <linux/errno.h>  
#include <asm/segment.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>

#include "pacman.h"

#define MY_DEVICE "pacman"
#define NOTREADY '0'

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anonymous");

/* globals */
int my_major = 0; /* will hold the major # of my device driver */

struct file_operations my_fops = {
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .ioctl = my_ioctl
};

typedef struct my_private_data {
    char buffer[3030];
	int points;
}*MyPrivateData; 

int init_module(void)
{
    printk("!!!!!!!!!!!!!!!in init_module\n");
    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0)
    {
	printk(KERN_WARNING "can't get dynamic major\n");
	return my_major;
    }

    //
    // do_init();
    //
    return 0;
}


void cleanup_module(void)
{
    unregister_chrdev(my_major, MY_DEVICE);

    //
    // do clean_up();
    //
    return;
}


int my_open(struct inode *inode, struct file *filp)
{
    // handle open
    printk("!!!!!!!!!!!!!!in my_open\n");
    int minor = MINOR(inode->i_rdev);
    printk("!!!!!!!!!!!minor is: %d\n", minor);

    /// TODO: handle ref counting
    /// TODO: handle extra errors with return -EFAULT
    MyPrivateData mpd = (MyPrivateData)kmalloc(sizeof(struct my_private_data),GFP_KERNEL); 
    if(mpd == NULL) {
        return -ENOMEM;
    }
    int i;
    for(i = 0; i < 3030; i++) {
        mpd->buffer[i] = NOTREADY;
    }
	mpd->points = 0
    filp->private_data = mpd;

    return 0;
}


int my_release(struct inode *inode, struct file *filp)
{
    // handle read closing
    kfree(filp->private_data);
    return 0;
}

ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    //
    // Do read operation.
    // Return number of bytes read.
    return 0; 
}

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch(cmd)
    {
    case NEWGAME:
	//
	// handle 
	//
	break;
    case GAMESTAT:
	//
	// handle 
	//
	break;

    default:
	return -ENOTTY;
    }

    return 0;
}

struct file *file_open(const char *path)
{
    printk("!!!!!!!!!!!!!in file_open\n");
    
    struct file *filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, O_RDWR, O_APPEND);
    set_fs(oldfs);
    if (IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

void file_close(struct file *file)
{
    filp_close(file, NULL);
}


int file_read(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size)
{
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = kernel_read(file, offset, data, size);

    set_fs(oldfs);
    return ret;
}
