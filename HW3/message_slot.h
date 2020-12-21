#include <linux/ioctl.h>

#define MAJOR_NUM 240
#define DEVICE_RANGE_NAME "message_slot"
#define MSG_LEN 128
#define SUCCESS 0
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)

struct message_node {
	int channel_id;
	char message[MSG_LEN];
	int len;
	struct message_node* next;
};

typedef struct message_node msg_node;