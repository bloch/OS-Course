#include <fcntl.h> 
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "message_slot.h"

int main(int argc, char* argv[]) {
	int fd; char BUF[MSG_LEN]; int msg_len;
	if(argc != 3) {
		perror("Invalid # of parameters for message_reader");
		exit(1);
	}
	fd = open(argv[1], O_RDWR);
    if(fd < 0) {
    	perror("Can't open device file IN MESSAGE_READER");
    	exit(1);
    }
 	if(ioctl(fd, MSG_SLOT_CHANNEL, atoi(argv[2])) != SUCCESS) {
 		perror("IOCTL ERROR IN MESSAGE_READER");
    	exit(1);
 	}
 	msg_len = read(fd, &BUF, MSG_LEN);
 	if(msg_len <= SUCCESS) {
 		perror("READ ERROR IN MESSAGE_READER");
    	exit(1);
 	}
 	if(close(fd) != SUCCESS) {
 		perror("CLOSE ERROR IN MESSAGE_READER");
    	exit(1);
 	}
 	if(write(1, &BUF, msg_len) != msg_len) {
 		perror("WRITE ERROR IN MESSAGE_READER");
    	exit(1);
 	}
 	exit(0);
}