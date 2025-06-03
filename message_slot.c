// I am using what we saw in the recitation as a base for my code
// Declare what kind of code we want
// from the header files. Defining __KERNEL__
// and MODULE allows us to access kernel-level
// code not usually available to userspace programs.
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE


#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>     /* for kmalloc and kfree */

MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations
#include "message_slot.h"

static slots *minors[256]; // array of slots for each minor device

//================== DEVICE FUNCTIONS ===========================
static int device_open( struct inode* inode,
                        struct file*  file )
{
    int minor;
    printk("Invoking device_open(%p, %p)\n", inode, file);
    minor = iminor(inode); 
    if (!minors[minor]) {
        minors[minor] = kmalloc(sizeof(slots), GFP_KERNEL);
        if (!minors[minor]) {
            return -ENOMEM; 
        }
        minors[minor]->head = NULL; 
    }
    return SUCCESS;
}

//---------------------------------------------------------------
static int device_release( struct inode* inode,
                           struct file*  file)
{
    printk("Invoking device_release(%p, %p)\n", inode, file);
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read( struct file* file,
                            char __user* buffer,
                            size_t       length,
                            loff_t*      offset )
{
    int i;
    message_slot *slot= (message_slot *)file->private_data; 
    printk("Invoking device_read(%p, %p, %zu, %lld)\n", file, buffer, length, *offset);
    if (!slot) {
        return -EINVAL;
    }
    if (!slot->message || slot->length <= 0) {
        return -EWOULDBLOCK; 
    }
    if (length < slot->length || buffer == NULL) {
        return -ENOSPC;
    }
    for (i = 0; i < slot->length; i++) {
        if (put_user(slot->message[i], buffer + i) != 0) {
            return -EFAULT;
        }
    }
    return slot->length; 
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
    int e;
    int i;
    char nbuff[BUF_LEN];
    message_slot *slot = (message_slot *)file->private_data; 
    printk("Invoking device_write(%p, %p, %zu, %lld)\n", file, buffer, length, *offset);
    if (!slot) {
        return -EINVAL;
    }
    if (length <= 0 || length > BUF_LEN) {
        return -EMSGSIZE;
    }
    e=slot->enc;
    memset(slot->message, 0, BUF_LEN); 
    for (i = 0; i < length; i++) {
        if((e>0) && (i%3 == 2)){
            nbuff[i] = '#';
	}
        else{
        if (get_user(nbuff[i], buffer+i) != 0) {
            return -EFAULT;
        }
	}
    }
    slot->length = length; 
    for(i=0;i<length;i++){
	    slot->message[i]=nbuff[i];
    }
    file->private_data=slot;
    return length;
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
    int minor;
    message_slot *slot;
    printk("Invoking device_ioctl(%p, %u, %lu)\n", file, ioctl_command_id, ioctl_param);
        
    switch (ioctl_command_id) {
        case MSG_SLOT_CHANNEL:
            if (ioctl_param <= 0) {
                return -EINVAL;
            }
	    minor = iminor(file->f_inode); 
	    if(minor<0 || minor> 256){
		    return -EINVAL;
	    }
            if (!minors[minor]) {
                minors[minor] = kmalloc(sizeof(slots), GFP_KERNEL);
		if(!minors[minor]){
			return -EINVAL; 
		}
		minors[minor]->head = NULL;
            }
            if (!minors[minor]->head) {
                minors[minor]->head = kmalloc(sizeof(message_slot), GFP_KERNEL);
                if (!minors[minor]->head) {
                    return -ENOMEM;
                }
                minors[minor]->head->channel_id = ioctl_param; 
                minors[minor]->head->length = 0; 
                minors[minor]->head->enc = 0; 
                minors[minor]->head->next = NULL; 
		file->private_data = minors[minor]->head;
            } 
            else {
                message_slot *curr;
		curr = minors[minor]->head;
                while (curr && curr->channel_id != ioctl_param) {
                    curr = curr->next;
                }
                if (!curr) {
                    curr = kmalloc(sizeof(message_slot), GFP_KERNEL);
                    if (!curr) {
                        return -ENOMEM;
                    }
                    curr->channel_id = ioctl_param; 
                    curr->length = 0; 
                    curr->enc = 0; 
                    curr->next = minors[minor]->head; 
                    minors[minor]->head = curr;
                }
                file->private_data = curr; 
            } 
	    return SUCCESS;
        case MSG_SLOT_SET_ENC: ;
            slot = file->private_data; 
                if (!slot) {
                    return -EINVAL;
                }
            slot->enc = ioctl_param; 
	    file->private_data=slot;
            return SUCCESS;
        default:
            return -EINVAL;
    }
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release,
};

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
    int rc;
    int i;
    rc  = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
    if (rc < 0) {
        printk(KERN_ERR "Failed to register character device with major number %d\n", MAJOR_NUM);
        return rc; // Return the error code
    }
    // Initialize the minors array
    for (i = 0; i < 256; i++) {
        minors[i] = kmalloc(sizeof(slots), GFP_KERNEL);
        /*if(!minors[i]) {
            if (!minors[i]) {
                printk(KERN_ERR "Failed to allocate memory for minor %d\n", i);
                return -ENOMEM; // Return memory allocation error
            }
            minors[i]->head = NULL; // Initialize the head of the slots list
        }*/
        /*else
            minors[i]->head = NULL; // Ensure the head is initialized*/
	minors[i]->head = NULL;
    }
    printk("Registered character device with major number %d\n", MAJOR_NUM);
    return SUCCESS; // Return success
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
    int i;
    printk("Unregistering character device with major number %d\n", MAJOR_NUM);
    
    // Free the minors array
    for (i = 0; i < 256; i++) {
        if (minors[i]) {
            message_slot *curr = minors[i]->head;
            while (curr) {
                message_slot *temp = curr;
                curr = curr->next;
                kfree(temp); 
            }
            kfree(minors[i]); 
            minors[i] = NULL;
        }
    }
    printk("Character device unregistered successfully\n");
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
