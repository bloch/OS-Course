#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");

#include "message_slot.h"

//================== DEVICE ATTRIBUTES ===============================

static msg_node* minors_arr[256];

//================== DEVICE FUNCTIONS ===========================

static int device_open(struct inode* inode, struct file*  file) {
	int* channel_id_pointer = (int*)kmalloc(sizeof(int), GFP_KERNEL);
	*channel_id_pointer = 0;
	file->private_data = (void *)channel_id_pointer;
	return SUCCESS;
}

static int device_release(struct inode* inode, struct file*  file) {
	kfree((int*)file->private_data);
	return SUCCESS;
}

static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {
	msg_node* curNode; int i;
	int channel_id = *((int*)file->private_data);
	int minor = iminor(file->f_inode);
	if(channel_id == 0) { //FIRST ERROR CASE OF READ.
		return -EINVAL;
	}
	if(buffer == NULL) {  //THIRD ERROR CASE OF READ.
		return -EINVAL;
	}
	curNode = minors_arr[minor];
	while(curNode != NULL) {
		if(curNode->channel_id == channel_id)
			break;
		curNode = curNode->next;
	}
	if(curNode != NULL) {
		if(curNode->len == 0) { //SECOND ERROR CASE OF READ.
			return -EWOULDBLOCK;
		}
		if(length < curNode->len) { //THIRD ERROR CASE OF READ.
			return -ENOSPC;
		}
		//ELSE: MESSAGE EXISTS, ENOUGH SPACE IN BUFFER. ALL GOOD.
		for(i = 0;i < curNode->len;i++) {
			if(put_user(curNode->message[i], &buffer[i]) < 0) {
				return 0;	//RETURNS 0 FOR ATOMIC OPERATIONS.
			}
		}	
		return i;		//RETURNS MSG LENGTH.
	}
	else {  //SECOND ERROR CASE OF READ.
		return -EWOULDBLOCK;
	}
}

static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {
	msg_node* curNode; char tmp_buffer[MSG_LEN];
	int channel_id = *((int*)file->private_data);
	int minor = iminor(file->f_inode);
	if(channel_id == 0 || buffer == NULL) { //FIRST ERROR CASE OF WRITE.
		return -EINVAL;
	}
	if(length == 0 || length > MSG_LEN) { //SECOND ERROR CASE OF WRITE.
		return -EMSGSIZE;
	}
	curNode = minors_arr[minor];
	while(curNode != NULL) {
		if(curNode->channel_id == channel_id)
			break;
		curNode = curNode->next;
	}

	if(curNode != NULL) {
		int i;
		for(i = 0; i < length && i < MSG_LEN;i++) {
			if(get_user(tmp_buffer[i], &buffer[i]) < 0)  {
				return 0;	//RETURNS 0 FOR ATOMIC OPERATIONS.
			}
		}
		for(i = 0; i < length && i < MSG_LEN;i++) {
			curNode->message[i] = tmp_buffer[i];
		}
		curNode->len = i;
		return i;
	}
	else { // curNODE == NULL -> BUILD NEW NODE
		int i;
		msg_node* new_node = (msg_node*)kmalloc(sizeof(msg_node), GFP_KERNEL);
		if(!new_node) {
 			return -EINVAL; //CHECK THIS IF TO RETURN -1...
		}
		new_node->channel_id = channel_id;
		new_node->next = minors_arr[minor];
		minors_arr[minor] = new_node;
		for(i = 0; i < length && i < MSG_LEN;i++) {
			if(get_user(tmp_buffer[i], &buffer[i]) < 0)  {
				new_node->len = 0;
				return 0;	//RETURNS 0 FOR ATOMIC OPERATIONS.
			}
		}
		for(i = 0; i < length && i < MSG_LEN;i++) {
			new_node->message[i] = tmp_buffer[i];
		}
		new_node->len = i;
		return new_node->len;
	}
}

static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
	if(ioctl_param == 0) { //SECOND ERROR CASE OF IOCTL. 
		return -EINVAL;
	}
	if(ioctl_command_id == MSG_SLOT_CHANNEL) {
		*((int*)file->private_data) = ioctl_param;
		return SUCCESS;
	}
	return -EINVAL;		//FIRST ERROR CASE OF IOCTL.
}

//==================== DEVICE SETUP =============================

struct file_operations Fops =
{
  .owner	      = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
  .release        = device_release,
};

static int __init slot_init(void) {
  if(register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops) < 0) {
    printk( KERN_ALERT "%s registraion failed for  %d\n", DEVICE_RANGE_NAME, MAJOR_NUM);
    return -1;
  }
  printk("Registeration of message_slot module is successful.");
  return 0;
}

static void __exit slot_cleanup(void) {
	int i; msg_node* tmp; msg_node* lst;
	unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
	for(i = 0; i < 256; i++) {  //FREE DATA OF MODULE.
		lst = minors_arr[i];
		while(lst != NULL) {
			tmp = lst->next;
			kfree(lst);
			lst = tmp;
		}
	}
}

module_init(slot_init);
module_exit(slot_cleanup);