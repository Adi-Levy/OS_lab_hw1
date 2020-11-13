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
#include <stdbool.h>

#include "pacman.h"

#define MY_DEVICE "pacman"
#define NOTREADY '0'

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anonymous");

struct file_operations my_fops = {
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
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

int move(char* buffer, char move);
bool game_is_over(MyPrivateData pd);

int init_module(void)
{
    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0)
    {
	printk(KERN_WARNING "can't get dynamic major\n");
	return my_major;
    }

    m_list = NULL;

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
    int minor = MINOR(inode->i_rdev);
    //add minor to the list:
    MinorList iter = m_list;
    while(iter != NULL)
    {
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
    MyPrivateData pd = (MyPrivateData)(filp->private_data);
    if(pd->buffer[0] == '0') 
    {
        return 0;
    }

    if(count > 3030) 
    {
        count = 3030;
    }

    int success = copy_to_user(buf, pd->buffer, count);
    if(success != 0) 
    {
        return -EBADF;
    }

    return count; 
}

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    MyPrivateData pd = (MyPrivateData)(filp->private_data);
    if(pd->buffer[0] == '0') 
    {
        return -EINVAL;
    }

    char* instructions = (char*)kmalloc(count*sizeof(char), GFP_KERNEL);
    if(instructions == NULL)
    {
        return -EFAULT;
    }

    copy_from_user(instructions, buf, count);

    int i = 0;
    for(; i < count; i++)
    {
        if(game_is_over(pd))
        {
            return count;
        }
        if(move(pd->buffer, instructions[i]) == 0)
        {
            pd->points++;
        }
    }
    return count;
}

bool game_is_over(MyPrivateData pd) 
{
    char* buffer = pd->buffer;
    int i = 0;
    for(; i < 3030; i++) 
    {
        if(buffer[i] == '*')
        {
            return false;
        }
    }
    return true;
}

int getArgString(unsigned long arg, char** buffer, int *buffer_size)
{
	//two last arguments are return
 
    char* c = (char*)arg; // cast argument to pointer 
    
    int str_len = strnlen_user(c,PATH_MAX);
	if(str_len < 0)
		return -EFAULT;
	
    *buffer = (char*)kmalloc(sizeof(char)*str_len, GFP_KERNEL); 
	if (*buffer == NULL)
    {
		return -ENOMEM;
    }

    if(copy_from_user(*buffer, c, str_len) < 0)
	{
    	return -EFAULT;
    }

	*buffer_size = str_len;

    return 0;
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

struct file *file_open(const char *path)
{   
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

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    MyPrivateData pd = (MyPrivateData)(filp->private_data);
    struct file *map_filp;

    switch(cmd)
    {
    case NEWGAME:
        pd->points = 0;
        if(arg == 0) 
        {
            return 0;
        }
        
        char* buffer = NULL;
		int buffer_size;
        int res = getArgString(arg, &buffer, &buffer_size);
        if(res < 0) 
        {
            return res;
        }
		// path is now in buffer

		map_filp = file_open(buffer);
		// map file opened?
        if(map_filp == NULL) 
        {
            //path does not exist
            return -ENOENT;
        }
		// fill the private data of the file with game screen
        file_read(map_filp, 0, pd->buffer,(size_t)3030);
        kfree(buffer);
        file_close(map_filp);
	break;
    case GAMESTAT:
        if(pd->buffer[0] == '0') 
        {
            return -EINVAL;
        }
        int game_score_and_state = 0;
        game_score_and_state = pd->points;
        if(game_is_over(pd))
        {
            game_score_and_state |= ((unsigned int)1 << 31);
        }
        copy_to_user((void*)arg, &game_score_and_state, 4);
        return 0;
	break;

    default:
	return -ENOTTY;
    }

    /// TODO: free all alocated memory!!!!!
    return 0;
}

struct loc 
{
    int x;
    int y;
};
// returns 0 if moved and -1 if illigal move
int move(char* buffer, char move)
{   
    struct loc player = {-1,-1};
    struct loc dest = {-1,-1};
    int player_offset = -1;
    int dest_offset = -1;

    //find player location:
    int i = 0;
    for(; i < 3030; i++)
    {
        if(buffer[i] == 'x')
        {
            player_offset = i;
            break;
        }
    }
    player.x = player_offset%101;
    player.y = player_offset/101;

    //determinate dest and check if not out of bounds:
    switch(move) 
    {
    case 'U':
        if(player.y == 0) //moving out of bound
            return -1;
        dest.x = player.x;
        dest.y = player.y - 1;
        break;
    case 'D': 
        if(player.y == 29)
            return -1;
        dest.x = player.x;
        dest.y = player.y + 1;
        break;
    case 'R':
        if(player.x == 99)
            return -1;
        dest.x = player.x + 1;
        dest.y = player.y;
        break;
    case 'L':
        if(player.x == 0)
            return -1;
        dest.x = player.x - 1;
        dest.y = player.y;
        break;
    default:
        return -1;
    }
    dest_offset = dest.y*101 + dest.x;

    // update board:
    char dest_char = buffer[dest_offset];
    if(dest_char == '*')
    {  
        buffer[dest_offset] = 'x';
        buffer[player_offset] = ' ';
        return 0;
    }
    else if(dest_char == ' ')
    {   // actualy both if's are the same 
        buffer[dest_offset] = 'x';
        buffer[player_offset] = ' ';
        return 0;
    }

    // else: illigal move
        return -1;
}


