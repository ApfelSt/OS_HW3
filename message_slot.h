#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H
#include <linux/ioctl.h>
#define MAJOR_NUM 235
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)
#define MSG_SLOT_SET_ENC _IOW(MAJOR_NUM, 1, unsigned int)
#define DEVICE_RANGE_NAME "message_slot"
#define BUF_LEN 128
#define SUCCESS 0

// Structure to describe individual message slots
typedef struct message_slot {
    unsigned long channel_id;
    char message[BUF_LEN];
    int length; 
    int enc; // Encryption flag
    struct message_slot *next;
} message_slot;

typedef struct slots {
    message_slot *head;
} slots;

#endif
