#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

int checkForPipe(int count, char** arglist);
int SIGINT_handler_default();
void child_handler(int signum);

int prepare() {
	//Handler for SIGINT.
	struct sigaction new_action;
	memset(&new_action, 0, sizeof(new_action));
	new_action.sa_handler = SIG_IGN; 			//Assign handler pointer of IGNORE.
	new_action.sa_flags = SA_RESTART;	    	//Assign flags to handler.
	if(sigaction(SIGINT, &new_action, NULL) != 0) {
		//REPORT ERROR ON REGISTERING THE NEW SIGINT SIGNAL HANDLER.
		perror("Failure of SIGINT handler initialization.");
		return -1;		// return value non-zero on prepare() errors.
	}

	//Handler for SIGCHLD.
	struct sigaction new_CHLD_action;
	memset(&new_CHLD_action, 0, sizeof(new_CHLD_action));
	new_CHLD_action.sa_handler = child_handler; 		//Assign handler pointer.
	new_CHLD_action.sa_flags = SA_RESTART;	    		//Assign flags to handler.
	if(sigaction(SIGCHLD, &new_CHLD_action, NULL) != 0) {
		//REPORT ERROR ON REGISTERING THE NEW SIGCHLD SIGNAL HANDLER.
		perror("Failure of SIGCHLD handler initialization.");
		return -1;		// return value non-zero on prepare() errors.
	}
	return 0; 			// prepare() returns 0 on success.
}

int process_arglist(int count, char** arglist) {
	if(strcmp(arglist[count-1],"&") == 0) {     // BACKGROUND OPTION.
		arglist[count-1] = NULL;				//NOT TO PASS & TO THE EXECVP.
		int pid = fork();
		if(pid == -1) {
			perror("Fork failed.");
			return 0;
		}
		if(pid == 0) {  //CHILD PROCESS.
			int exec_success = execvp(arglist[0], arglist); 	// EXECUTION.
			if(exec_success == -1) {
				perror("Execution of process failed");
				exit(1);
			}
		}
		// PARENT: DOESN'T WAIT FOR CHILD.
	}
	else {
		int pipIndex = checkForPipe(count, arglist);
		if(pipIndex >= 0) { // PIPE OPTION.
			int pipefd[2];
			int pipe_success = pipe(pipefd);
			if(pipe_success == -1) {
				perror("Pipe failed.");
				return 0;
			}
			int pid1 = fork();
			if(pid1 == -1) {
				perror("Fork failed.");
				return 0;
			}
			if(pid1 == 0) { // CHILD 1: PROCESS 1.
				int handler_success = SIGINT_handler_default();
				if(handler_success == -1) {
					perror("Failure of SIGINT handler init to default.");
					exit(1);
				}
				int dup2_success = dup2(pipefd[1], 1);
				if(dup2_success == -1) {
					perror("Failure of dup2(): unable to change stdout.");
					exit(1);
				}
				int close_success = close(pipefd[0]);  //CLOSE READ END OF PIPE.
				if(close_success == -1) {
					perror("Failure of close(): unable to close READ END of pipe.");
					exit(1);
				}
				close_success = close(pipefd[1]);   //CLOSE WRITE END OF PIPE.
				if(close_success == -1) {
					perror("Failure of close(): unable to close WRITE END of pipe.");
					exit(1);
				}
				arglist[pipIndex] = NULL;		    //PROCESS 1 EXECS FIRST CMD.
				int exec_success = execvp(arglist[0], arglist);  	// EXECUTION.
				if(exec_success == -1) {
					perror("Execution of process 1 failed");
					exit(1);
				}
			}
			else {  //PARENT: FORK ANOTHER PROCESS.
				int pid2 = fork();
				if(pid2 == -1) {
					perror("Fork failed.");
					return 0;
				}
				if(pid2 == 0) { //CHILD 2: PROCESS 2.
					int handler_success = SIGINT_handler_default();
					if(handler_success == -1) {
						perror("Failure of SIGINT handler init to default.");
						exit(1);
					}
					int dup2_success = dup2(pipefd[0], 0);
					if(dup2_success == -1) {
						perror("Failure of dup2(): unable to change stdin.");
						exit(1);
					}					
					int close_success = close(pipefd[0]);  //CLOSE READ END OF PIPE.
					if(close_success == -1) {
						perror("Failure of close(): unable to close READ END of pipe.");
						exit(1);
					}
					close_success = close(pipefd[1]);   //CLOSE WRITE END OF PIPE.
					if(close_success == -1) {
						perror("Failure of close(): unable to close WRITE END of pipe.");
						exit(1);
					}
					int exec_success = execvp(arglist[pipIndex+1], arglist + (pipIndex+1));
					if(exec_success == -1) {
						perror("Execution of process 2 failed");
						exit(1);
					}
				}
				else {   //PARENT PROCESS.
					int close_success = close(pipefd[0]);  //CLOSE READ END OF PIPE.
					if(close_success == -1) {
						perror("Failure of close(): unable to close READ END of pipe.");
						return 0;
					}
					close_success = close(pipefd[1]);   //CLOSE WRITE END OF PIPE.
					if(close_success == -1) {
						perror("Failure of close(): unable to close WRITE END of pipe.");
						return 0;
					}
					// Shell is waiting for the two processes to finish.
					int pid_1 = waitpid(pid1, NULL, 0);
					if(pid_1 == -1 && errno != ECHILD && errno != EINTR) {
						perror("Pipe (1) Process failed");
						return 0;
					}
					int pid_2 = waitpid(pid2, NULL, 0);
					if(pid_2 == -1 && errno != ECHILD && errno != EINTR) {
						perror("Pipe (2) Process failed");
						return 0;
					}
				}
			}
		}
		else { // FOREGROUND EXECUTION OF COMMANDS.
			int pid = fork();
			if(pid == -1) {
				perror("Fork failed.");
				return 0;
			}
			if(pid == 0) {    // CHILD:TO BE EXECUTING CMD.
				int handler_success = SIGINT_handler_default(); //SIGINT HANDLER=DEFAULT.
				if(handler_success == -1) {
					perror("Failure of SIGINT handler init to default.");
					exit(1);
				}
				int exec_success = execvp(arglist[0], arglist); //EXECUTION.
				if(exec_success == -1) {
					perror("Execution of process failed");
					exit(1);
				}
			}
			else { // PARENT(SHELL).
				int proc_pid = waitpid(pid, NULL, 0);
				if(proc_pid == -1 && errno != ECHILD && errno != EINTR){
					perror("Foreground process failed for some reason.");
					return 0;
				}
			}
		}
	}
	return 1;
}

int finalize() {
	return 0; 		// finalize() returns 0 on success.
}

// returns index of '|' in arglist, or -1 otherwise.
int checkForPipe(int count, char** arglist) {
	for(int i = 0; i < count; i++) {
		if(strcmp(arglist[i],"|") == 0) {
			return i;
		}
	}
	return -1;
}

int SIGINT_handler_default() {
	struct sigaction new_action;
	memset(&new_action, 0, sizeof(new_action));
	new_action.sa_handler = SIG_DFL; 		//Assign handler pointer: DEFAULT!
	new_action.sa_flags = SA_RESTART;	    //Assign flags to handler.
	if(sigaction(SIGINT,&new_action,NULL) != 0) {
		return -1;   // returns -1 on failure.
	}
	return 0;       // returns 0 on success.
}

void child_handler(int signum) {
  int pid_res;
  while((pid_res = waitpid(-1, NULL, WNOHANG)) > 0);
  if(pid_res == -1 && errno != EINTR && errno != ECHILD) {
  		perror("An error has occured in the child handler.");
  		exit(1);
  }
}