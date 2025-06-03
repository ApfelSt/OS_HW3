#include "message_slot.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <msg_slot_path> <channel_id>\n", argv[0]);
        exit(1);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("Failed to open message slot");
        exit(1);
    }

    unsigned int channel_id = atoi(argv[2]);
    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) < 0) {
        perror("Failed to set channel ID");
        close(fd);
        exit(1);
    }

    char buffer[BUF_LEN];
    ssize_t bytes_read = read(fd, buffer, BUF_LEN);
    if (bytes_read < 0) {
        perror("Failed to read message");
        close(fd);
        exit(1);
    }

   if(write(STDOUT_FILENO, buffer, bytes_read) < 0) {
        perror("Failed to write message to stdout");
        close(fd);
        exit(1);
    }
    //printf("%s",buffer);
    close(fd);
    exit(0);
}
