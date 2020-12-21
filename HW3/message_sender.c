#include <fcntl.h> 
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "message_slot.h"

int main(int argc, char* argv[]) {
	int fd;
	if(argc != 4) {
		perror("Invalid # of parameters for message_sender");
		exit(1);
	}
	fd = open(argv[1], O_RDWR);
    if(fd < 0) {
    	perror("Can't open device file");
    	exit(1);
    }
 	if(ioctl(fd, MSG_SLOT_CHANNEL, atoi(argv[2])) != SUCCESS) {
 		perror("IOCTL ERROR IN MESSAGE_SENDER");
    	exit(1);
 	}	
 	if(write(fd, argv[3], strlen(argv[3])) != strlen(argv[3])) {
 		perror("WRITE ERROR IN MESSAGE_SENDER");
    	exit(1);
 	}
 	if(close(fd) != SUCCESS) {
 		perror("CLOSE ERROR IN MESSAGE_SENDER");
    	exit(1);
 	}
 	exit(0);
}
