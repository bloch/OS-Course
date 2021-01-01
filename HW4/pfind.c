#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdatomic.h>

#define SUCCESS 0

struct directory_node {
	char* dir;
	struct directory_node* next;
	struct directory_node* prev;
};

typedef struct directory_node dir_node;

struct queue_obj {
	int len;
	dir_node* start;
	dir_node* end;
};

typedef struct queue_obj queue;

int enqueue(queue* q, char* dir) {
	dir_node* new_node = (dir_node*) malloc(sizeof(dir_node));
	if(!new_node) {
		return -1;  //FAILED.
	}
	int len = strlen(dir);
	new_node->dir = (char*)malloc((len+1)*sizeof(char));
	memcpy(new_node->dir, dir,len+1);
	if(q->len == 0) {
		new_node->next = NULL;
		new_node->prev = NULL;
		q->start = new_node;
		q->end = new_node;
	}
	else {
		new_node->next = q->start;
		new_node->prev = NULL;
		q->start->prev = new_node;
		q->start = new_node;
	}
	q->len++;
	return SUCCESS; //1 for success.
}

//ASSUMPTION: q is not empty.
char* dequeue(queue* q) {
	dir_node* last = q->end;
	if(q->len > 1) {
		dir_node* new_end = q->end->prev;
		last->prev = NULL;
		new_end->next = NULL;
		q->end = new_end;
	}
	else {	//WHEN SIZE OF Q IS 1, WE ACTUALLY DELETE ALL OF Q.
		q->start = NULL;
		q->end = NULL;
	}
	q->len--;
	return last->dir;	
}

// ========================= GLOBAL VARIABLES ====================================================

pthread_mutex_t queue_lock; 			//QUEUE LOCK - for operations on queue.
pthread_cond_t queueNotEmpty;			//CV - Condition Variable - for awakening threads.
queue* q;								//THE GLOBAL QUEUE - MAIN DATA STRUCTURE OF THIS PROGRAM.
char* term;								//THE TERM FOR WHICH WE LOOK AT THE DIRECTORIES - ARGV[2].
atomic_int counter = 0;					//Counter to counter the number of files found.
atomic_int work;						//WORK WHILE work=1, AND EXIT THREADS ON work=0.

atomic_int start_search;						//START SEARCHING DIRECTORIES ONLY AFTER start is 1.
atomic_int num_threads_sleeping;
atomic_int num_threads_exited;
atomic_int total;
atomic_int RET_VAL;

// ============================================================================================

void search_dir(char* dir_to_search) {
	struct dirent* entry; struct stat file_info; int op_val;
	DIR* dir = opendir(dir_to_search);
	if(dir == NULL) {	//CHECK IF DIR IS SEARCHABLE.
		if(errno == EACCES) {
			printf("Directory %s: Permission denied.\n", dir_to_search);
		}
		else {
			num_threads_exited++;
			RET_VAL = 1;
			pthread_exit(NULL);
		}
	}
	while((entry = readdir(dir)) != NULL) {
		if(strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name, "..") == 0) {
			//do nothing with this directories...
		}
		else {
			//Generating the full path..
			char fullPath[PATH_MAX];
			strcpy(fullPath, dir_to_search);
			if(strcmp(dir_to_search, "/") == 0) {
			}
			else { 
				strcat(fullPath, "/");
			}
			strcat(fullPath, entry->d_name);
			
			op_val = lstat(fullPath, &file_info);
			if(op_val < 0) {
				num_threads_exited++;
				if(num_threads_sleeping == total - num_threads_exited) {
					work = 0;
					pthread_cond_broadcast(&queueNotEmpty);
				}
				RET_VAL = 1;
				pthread_exit(NULL);
			}

			if(S_ISDIR(file_info.st_mode)) { //IF IT'S A DIRECTORY..
				DIR* new_dir = opendir(fullPath);
				if(new_dir == NULL) { //CHECK IF NEWLY FOUND DIRECTORY IS SEARCHABLE..
					if(errno == EACCES) {
						printf("Directory %s: Permission denied.\n", fullPath);
					}
					else {	//ERROR WHICH IS NOT EACCES -> EXIT THE THREAD.
						num_threads_exited++;
						if(num_threads_sleeping == total - num_threads_exited) {
							work = 0;
							pthread_cond_broadcast(&queueNotEmpty);
						}
						RET_VAL = 1;
						pthread_exit(NULL);
					}
				}
				else {  //Searchable directory -> enqueue this to be searched. 
					closedir(new_dir);   //OPENED NEW_DIR WITH SUCCESS, NEED TO CLOSE IT BACK.
					pthread_mutex_lock(&queue_lock);
					enqueue(q, fullPath);
					pthread_mutex_unlock(&queue_lock);
					pthread_cond_broadcast(&queueNotEmpty);  //SIGNAL THREADS ABOUT NEW DIRECTORY!
				}
			}
			else {
				if(strstr(entry->d_name, term) != NULL) {	//FOUND A MATCHING FILE.
					printf("%s\n", fullPath);
					counter++;
				}
			}	
		}
	}
	closedir(dir);
}

void* SOME_FUNCTION2() {
	while(1) {
		if(work == 0) {
			num_threads_exited++;
			pthread_exit(NULL);
		}
		pthread_mutex_lock(&queue_lock);
		if(!start_search) {
			num_threads_sleeping++;
			pthread_cond_wait(&queueNotEmpty, &queue_lock);
			num_threads_sleeping--;
			if(work == 0) {	//IF DONE - EXIT THREADS.
				num_threads_sleeping++;
				num_threads_exited++;
				pthread_mutex_unlock(&queue_lock);
				pthread_exit(NULL);
			}
		}
		else { //started working, the signal is on!
			if(q->len == 0) {
				num_threads_sleeping++;

				//THIS PREVENTS DEADLOCKS!
				if(num_threads_sleeping == total - num_threads_exited) {
					pthread_cond_broadcast(&queueNotEmpty);
					work = 0;
					pthread_mutex_unlock(&queue_lock);
					break;
				}
				pthread_cond_wait(&queueNotEmpty, &queue_lock);
				num_threads_sleeping--;
				if(work == 0) {		//IF DONE -> threads exiting..
					num_threads_sleeping++;
					num_threads_exited++;
					pthread_mutex_unlock(&queue_lock);
					pthread_exit(NULL);
				}
			}
		}
		if(q->len == 0) {
			//NOTHING TO DO... GO TO SLEEP BY RETURNING TO THE NEXT ITERATION OF THE WHILE!
			pthread_mutex_unlock(&queue_lock);
		}
		else {
			//SOMETHING TO SEARCH AS QUEUE LEN > 0, TAKE NEXT DIR AND GO TO SEARCH!
			char* dir_to_search = dequeue(q);
			pthread_mutex_unlock(&queue_lock);
			search_dir(dir_to_search);
		}
	}
	return NULL;
}

int main(int argc, char* argv[]) {
	int op_val;
	//CHECK THAT NUMBER OF PARAMETERS IS VALID.
	if(argc != 4) {
		fprintf(stderr, "Invalid number of arguments.\n");
		exit(1);
	}

	//CHECK IF THE ARGUMENT DIRECTORY IS SEARCHABLE.
	DIR* dir = opendir(argv[1]);
	if(dir == NULL) {   //NULL RETURNS INDICATES AN ERROR.
		if(errno == EACCES) {
			printf("Directory %s: Permission denied.\n", argv[1]);
		}
		else {
			fprintf(stderr, "Directory given in the program argument failed to open.\n");
		}
		exit(1);
	}
	else {                  //OPENED DIR WITH SUCCESS, NEED TO CLOSE IT BACK.
		closedir(dir);
	}

	//STEP 1 OF THE DESCRIBED FLOW: CREATE A FIFO QUEUE THAT HOLDS DIRECTORIES.
	q = (queue*) malloc(sizeof(queue));
	if(!q) {
		fprintf(stderr, "Allocation of queue failed.\n");
		exit(1);
	}
	q->len = 0;

	work = 1;
	term = argv[2];
	counter = 0;
	start_search = 0;
	num_threads_sleeping = 0;
	num_threads_exited = 0;
	RET_VAL = 0;

	total = atoi(argv[3]);

	//STEP 2 OF THE DESCRIBED FLOW: ENQUEUE ROOT DIR.
	op_val = enqueue(q, argv[1]);
	if(op_val < 0) {
		fprintf(stderr, "Failed to enter root directory to queue.");
		exit(1);
	}

	//STEP 3 OF THE DESCRIBED FLOW: CREATE N SEARCHING THREADS.
	pthread_t* thread_ids = (pthread_t*) malloc(atoi(argv[3]) * sizeof(pthread_t));
	for(int i = 0; i < atoi(argv[3]); i++) {
		 op_val = pthread_create(&thread_ids[i], NULL, SOME_FUNCTION2, NULL);
		 if(op_val != 0) { //0 for success.
		 	fprintf(stderr, "Failed to create thread.\n");
		 	exit(1);
		 }
	}

	//STEP 4 OF THE DESCRIBED FLOW: SIGNAL THEM.
	start_search = 1;
	pthread_cond_broadcast(&queueNotEmpty);
	

	// WAITING FOR THE THREADS TO FINISH.
	void* status;
  	for (int i = 0; i < atoi(argv[3]); i++) {
    	op_val = pthread_join(thread_ids[i], &status);
    	if (op_val) {
     	 	fprintf(stderr, "ERROR in pthread_join().\n");
      		exit(1);
    	}
	}

	//STEP 5 OF THE DESCRIBED FLOW: # of matching files.
	printf("Done searching, found %d files\n", counter);
	return RET_VAL;
}