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
    printk("Invoking device_open(%p, %p)\n", inode, file);
    int minor = iminor(inode); // Get the minor number from the inode
    if (!minors[minor]) {
        minors[minor] = kmalloc(sizeof(slots), GFP_KERNEL);
        if (!minors[minor]) {
            return -ENOMEM; // Return memory allocation error
        }
        minors[minor]->head = NULL; // Initialize the head of the slots list
    }
    return SUCCESS;
}

//---------------------------------------------------------------
static int device_release( struct inode* inode,
                           struct file*  file)
{
    printk("Invoking device_release(%p, %p)\n", inode, file);
    // Free the slots structure if it exists
    if (minors[minor]) {
        kfree(minors[minor]);
        minors[minor] = NULL; // Clear the pointer to avoid dangling pointer
    }
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
    printk("Invoking device_read(%p, %p, %zu, %lld)\n", file, buffer, length, *offset);
    message_slot *slot = file->private_data; // Get the private data associated with the file
    if (!slot) {
        return -EINVAL;
    }
    if (!slot->message || slot->length <= 0) {
        return -EWOULDBLOCK; 
    }
    if (length < slot->length) {
        return -ENOSPC;
    }
    // Copy the message to user space
    for (int i = 0; i < slot->length; i++) {
        if (put_user(slot->message[i], buffer + i) != 0) {
            return -EFAULT;
        }
    }
    return slot->length; // Return the length of the message read
}

//---------------------------------------------------------------
// a processs which has already opened
// the device file attempts to write to it
static ssize_t device_write( struct file*       file,
                             const char __user* buffer,
                             size_t             length,
                             loff_t*            offset)
{
    printk("Invoking device_write(%p, %p, %zu, %lld)\n", file, buffer, length, *offset);
    message_slot *slot = file->private_data; // Get the private data associated with the file
    if (!slot) {
        return -EINVAL;
    }
    if (length <= 0 || length > BUF_LEN) {
        return -EMSGSIZE;
    }
    // Copy the message from user space
    memset(slot->message, 0, BUF_LEN); // Clear the message buffer
    for (int i = 0; i < length; i++) {
        char c;
        if(slot->enc && i%3 == 2)
            c = '#';
        else
            c = buffer[i];
        if (get_user(c, buffer + i) != 0) {
            eturn -EFAULT;
        }
    }
    slot->length = length; // Set the length of the message
    return length; // Return the length of the message written
}

//----------------------------------------------------------------
static long device_ioctl( struct   file* file,
                          unsigned int   ioctl_command_id,
                          unsigned long  ioctl_param )
{
    printk("Invoking device_ioctl(%p, %u, %lu)\n", file, ioctl_command_id, ioctl_param);
        
    switch (ioctl_command_id) {
        case MSG_SLOT_CHANNEL:
            if (ioctl_param <= 0) {
                return -EINVAL;
            }
            int minor = iminor(file->f_inode); // Get the minor number from the inode
            if (!minors[minor]) {
                return -EINVAL;
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
            } 
            else {
                message_slot *current = minors[minor]->head;
                while (current && current->channel_id != ioctl_param) {
                    current = current->next;
                }
                if (!current) {
                    current = kmalloc(sizeof(message_slot), GFP_KERNEL);
                    if (!current) {
                        rturn -ENOMEM;
                    }
                    current->channel_id = ioctl_param; 
                    current->length = 0; 
                    current->enc = 0; 
                    current->next = minors[minor]->head; 
                    minors[minor]->head = current;
                }
                file->private_data = current; // Set the private data of the file to the current message slot
            } 
        case MSG_SLOT_SET_ENC:
            message_slot *slot = file->private_data; // Get the private data associated with the file
                if (!slot) {
                    return EINVAL;
                }
            slot->enc = ioctl_param; // Set the encryption flag
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
    int rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
    if (rc < 0) {
        printk(KERN_ERR "Failed to register character device with major number %d\n", MAJOR_NUM);
        return rc; // Return the error code
    }
    // Initialize the minors array
    for (int i = 0; i < 256; i++) {
        if(!minors[i]) {
            minors[i] = kmalloc(sizeof(slots), GFP_KERNEL);
            if (!minors[i]) {
                printk(KERN_ERR "Failed to allocate memory for minor %d\n", i);
                return -ENOMEM; // Return memory allocation error
            }
            minors[i]->head = NULL; // Initialize the head of the slots list
        }
        else
            minors[i]->head = NULL; // Ensure the head is initialized
    }
    printk("Registered character device with major number %d\n", MAJOR_NUM);
    return SUCCESS; // Return success
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
    printk("Unregistering character device with major number %d\n", MAJOR_NUM);
    
    // Free the minors array
    for (int i = 0; i < 256; i++) {
        if (minors[i]) {
            message_slot *current = minors[i]->head;
            while (current) {
                message_slot *temp = current;
                current = current->next;
                kfree(temp); // Free each message slot
            }
            kfree(minors[i]); // Free the slots structure
            minors[i] = NULL; // Clear the pointer
        }
    }
    printk("Character device unregistered successfully\n");
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME); // Unregister the character device
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
