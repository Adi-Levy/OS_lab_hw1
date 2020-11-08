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
#include <linux/limits.h>

#include "pacman.h"

#define MY_DEVICE "pacman"
#define NOTREADY '0'

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anonymous");


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


typedef struct minor_list{
    int minor;
    int ref_count;
    MyPrivateData private_data;

    struct minor_list* next;
}*MinorList;


/* globals */
int my_major = 0; /* will hold the major # of my device driver */
MinorList m_list;


int init_module(void)
{
    printk("!!!!!!!!!!!!!!!in init_module\n");
    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0)
    {
	printk(KERN_WARNING "can't get dynamic major\n");
	return my_major;
    }

    m_list = NULL;
    printk("!!!!!! m_list is NULL\n");

    return 0;
}


void cleanup_module(void)
{
    unregister_chrdev(my_major, MY_DEVICE);

    // TODO: clean up list
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

    //add minor to the list:
    MinorList iter = m_list;
    while(iter != NULL)
    {
        printk("!!!!!!!! in minor list loop in open\n");
        if(iter->minor == minor)
        {
            // minor already has an open file so we use it's private data with buffer
            iter->ref_count++;
            filp->private_data = iter->private_data;
            return 0;
        }
        if(iter->next == NULL)
        {
            break;
        }

        iter = iter->next;
    }
    
    // minor does not exist yet
    // allocate private data with buffer:
    MyPrivateData mpd = (MyPrivateData)kmalloc(sizeof(struct my_private_data),GFP_KERNEL);
    if(mpd == NULL) {
        return -ENOMEM;
    }
    

    int i;
    for(i = 0; i < 3030; i++) {
        mpd->buffer[i] = NOTREADY;
    }
    
    mpd->points = 0;
    filp->private_data = mpd;

    MinorList new_minor = (MinorList)kmalloc(sizeof(struct minor_list),GFP_KERNEL);
    if(new_minor == NULL) {
        return -EFAULT;
    }
    printk("!!!!!!! got to here\n");
    new_minor->private_data = mpd;
    new_minor->ref_count = 1;
    new_minor->minor = minor;
    new_minor->next = NULL;
    
    if(iter == NULL) 
    {
        m_list = new_minor;
    }
    else
    {
        iter->next = new_minor;
    }
    
    printk("!!!!!!! Added new private data\n");
    /// TODO: handle extra errors with return -EFAULT

    return 0;
}


int my_release(struct inode *inode, struct file *filp)
{
    // handle read closing
    int minor = MINOR(inode->i_rdev);

    MinorList iter = m_list;
    MinorList iter_prev = NULL;
    while(iter != NULL)
    {
        if(iter->minor == minor)
        {
            if(iter->ref_count == 1)
            {
                // this is the only file controlling this minor, therefore we can delete it
                if(iter_prev == NULL)
                { //deleting the first one
                    m_list = iter->next;
                }
                else
                {
                    iter_prev->next = iter->next;
                }
                kfree(iter);
                kfree(filp->private_data);
            }
            else
            {
                iter->ref_count--;

            }

            return 0;
        }
        iter_prev = iter;
        iter = iter->next;
    }
    return 0;
}

ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    //
    // Do read operation.
    // Return number of bytes read.
    return 0; 
}

void PrintArgString(unsigned long arg)
{
    printk("!!!!!!!");
    char* c = (char*)arg;
    int str_len = strnlen_user(c,PATH_MAX);
    char* k_string_buffer = (char*)kmalloc(sizeof(char)*str_len + 1, GFP_KERNEL); 
    copy_from_user(k_string_buffer, c, str_len);
    char* k = k_string_buffer;
    int i = 0;
    for(;i < str_len; i++)
    {
        printk("%c", k[i]);
    }
    printk("\n");
}

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch(cmd)
    {
    case NEWGAME:
        printk("!!!!!!!!!%lu\n", arg);
        PrintArgString(arg);
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
