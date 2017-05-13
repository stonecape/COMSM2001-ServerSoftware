/* Server program for key-value store. */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <poll.h>
#include <string.h> 

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "kv.h"
#include "parser.h"


#define NTHREADS 4
#define BACKLOG 10
#define BACKLOG_CONTROL 1

/* Add anything you want here. */
int cport, dport; /* control and data ports. */
int worker_con_fd[NTHREADS];/*assign con fd to different worker threads*/
int handling_worker_count;/*count the number of worker threads handling con*/
int global_run;

static pthread_mutex_t lock;
pthread_cond_t handle_condition;


void doLock() {
	int err = pthread_mutex_lock(&lock);
	if (err) { 
		printf("doLock occur error -> abort\n");
		abort(); 
	}

}

void doUnlock() {
	int err = pthread_mutex_unlock(&lock);
	if (err) { 
		printf("doUnlock occur error -> abort\n");
		abort(); 
	}
}

/* A worker thread. You should write the code of this function. */
void* worker(void* p) {
	int workerID = *(int *)p;
	char buffer[LINE];
	char msg_1[LINE];
	
	while(1) {
		// waiting for connection
		doLock();
		if(global_run == 0) {
				doUnlock();
				return NULL;
		}
		while(worker_con_fd[workerID] == -1) {
			int err;
			err = pthread_cond_wait(&handle_condition, &lock);
			if(err){
				printf("worker waiting task occur error, workerID:%d\n", workerID);
				abort();
			}
			
			if(global_run == 0) {
				doUnlock();
				return NULL;
			}
		}
		
		// increase handling_worker_count
		
		handling_worker_count++;
		doUnlock();
		
		int flag = 1;
		int con_fd;
		con_fd = worker_con_fd[workerID];
		
		// write welcome msg
		stpcpy(msg_1, "Welcome to the KV store\n"); 
		int writeErr = write(con_fd, msg_1, strlen(msg_1));
		if(writeErr < 0) {
			flag = 0;
		}
		
		// read and parse data
		enum DATA_CMD cmd;
		char* key;
		char* text;
		
		
		while(flag) {
			printf("go into while loop\n");
			int read_count;
			read_count = read(con_fd, buffer, LINE);
			if(read_count <= 0) {
				printf("read occur error, workerID:%d\n", workerID);
				break;
			}
			
			printf("read buffer=%s\n",buffer);
			parse_d(buffer, &cmd, &key, &text);
			printf("parse_d succeed\n");
			
			int exist_ret, result_ret;
			doLock();
			if(key != NULL) {
				exist_ret = itemExists(key);
				printf("exist_ret=%d\n",exist_ret);
			}
			doUnlock();
			
			
			
			switch(cmd) {

			   case D_PUT:
			   
					doLock();
					char* value_cp = malloc(strlen(text) + 1);
					strncpy(value_cp, text, strlen(text) + 1);
					if(exist_ret == 1) {
					  result_ret = updateItem(key, value_cp);
					} else {
					  result_ret = createItem(key, value_cp);
					}
					doUnlock();
					
					if(result_ret >= 0) {
					  stpcpy(msg_1, "Stored key successfully.\n"); 
					} else {
					  stpcpy(msg_1, "Error storing key.\n"); 
					}
					break; 
				
				case D_GET:
					doLock();
					if(exist_ret == 1) {
					  strcpy(msg_1,findValue(key));
					  strcat(msg_1, "\n");
					} else {
					  stpcpy(msg_1, "No such key.\n"); 
					}
					doUnlock();
					break; 
				
				case D_COUNT:
					doLock();
					sprintf(msg_1,"%d",countItems());
					strcat(msg_1, "\n");
					doUnlock();
					break;
				
				case D_DELETE:
					doLock();
					if(exist_ret == 1) {
						result_ret = deleteItem(key, 1);// 1:the value under this key is freed
						if(result_ret == 0) {
							stpcpy(msg_1, "Deleted key successfully.\n"); 
						} else{
							stpcpy(msg_1, "Error deleting key.\n"); 
						}
					} else {
						stpcpy(msg_1, "No such key.\n");
					}
					doUnlock();
					break;
				
				case D_EXISTS:
					if(exist_ret == 1) {
						stpcpy(msg_1, "1\n");
					} else {
						stpcpy(msg_1, "0\n");
					}
					break;
					
				case D_END:
					stpcpy(msg_1, "---END---\n");
					printf("%s",msg_1);
					flag = 0;
					break;
				
				case D_ERR_OL:
					stpcpy(msg_1, "Error: the line is too long.\n");
					break;
				
				case D_ERR_INVALID:
					stpcpy(msg_1, "Error: invalid command.\n");
					break;
				
				case D_ERR_SHORT:
					stpcpy(msg_1, "Error: too few parameters (e.g. “put 1”, which is missing a value).\n");
					break;
					
				case D_ERR_LONG:
					stpcpy(msg_1, "Error: too many parameters (e.g. “count x”, count does not take a parameter).\n");
					break;
				  
				default : 
					printf("---Magical Error---\n");
					flag = 0;
					break;
			}
			
			if(flag) {
				if(msg_1 == NULL || strlen(msg_1) == 0) {
					stpcpy(msg_1, "return msg is null or length == 0\n");
				}
				write(con_fd, msg_1, strlen(msg_1));
			}
		}
		
		// close connection
		close(con_fd);
		
		// change the con_fd for this worker thread to -1
		// decrease handling_worker_count
		// broadcast handle_condition in order to handle new connection
		doLock();
		worker_con_fd[workerID] = -1;
		handling_worker_count--;
		pthread_cond_broadcast(&handle_condition);
		doUnlock();

	}
	

}

void run() {
	handling_worker_count = 0;
	global_run = 1;
	int data_socket, control_socket; /*sockets*/
	struct sockaddr_in data_socket_sa, control_socket_sa;/*socketaddr_in*/
	struct pollfd fds[2];/*poll fd*/
	
	/*create sockets*/
	data_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(data_socket == -1) {
		printf("Fail to create Data socket.\n");
		exit(1);
	}
	
	control_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(control_socket == -1) {
		printf("Fail to create Control socket.\n");
		exit(1);
	}
	
	printf("Succeed to create Data and Control socket.\n");
	
	/*bind sockets*/
	socklen_t data_socket_len = sizeof(data_socket_sa);
	memset(&data_socket_sa, 0, data_socket_len);
	data_socket_sa.sin_family = AF_INET;
	data_socket_sa.sin_addr.s_addr = htonl(INADDR_ANY);
	data_socket_sa.sin_port = htons(dport);
	
	int data_socket_bind = bind(data_socket, (struct sockaddr*) &data_socket_sa, data_socket_len);
	if(data_socket_bind < 0) {
		printf("Fail to bind Data socket.\n");
		exit(1);
	}
	
	socklen_t control_socket_len = sizeof(control_socket_sa);
	memset(&control_socket_sa, 0, control_socket_len);
	control_socket_sa.sin_family = AF_INET;
	control_socket_sa.sin_addr.s_addr = htonl(INADDR_ANY);
	control_socket_sa.sin_port = htons(cport);
	
	int control_socket_bind = bind(control_socket, (struct sockaddr*) &control_socket_sa, control_socket_len);
	if(control_socket_bind < 0) {
		printf("Fail to bind Control socket.\n");
		exit(1);
	}
	
	printf("Succeed to bind Data and Control socket.\n");
	
	
	/*listen*/
	int errLisD = listen(data_socket, BACKLOG);
	int errLisC = listen(control_socket, BACKLOG_CONTROL);
	if(errLisD < 0 || errLisC) {
		printf("Fail to listen Data or Control port. DportErr=%d, CportErr=%d\n", errLisD, errLisC);
		exit(1);
	}
	
	/* initialize mutex and cond
	 * create worker threads
	 */
	pthread_cond_init(&handle_condition, NULL);
	pthread_mutex_init(&lock, NULL);
	int i;
	int workerID[NTHREADS];
    pthread_t workerT[NTHREADS];
	for (i = 0; i<NTHREADS; i++) {
        workerID[i] = i;
		worker_con_fd[i] = -1;
        pthread_create(&workerT[i], NULL, worker, &workerID[i]);
    }
	
	/*set poll*/
	fds[0].fd = data_socket;
	fds[1].fd = control_socket;
	fds[0].events = POLLIN;
    fds[1].events = POLLIN;
	
	/*accept*/
	while(global_run) {
		int poll_ret;
		//poll
		poll_ret = poll(fds, 2, 0);
		if(poll_ret > 0) {
			
			if(fds[1].revents & POLLIN) {
				// control port
				int connection = accept(control_socket, (struct sockaddr*) &control_socket_sa, &control_socket_len);
				if(connection == -1) {
					printf("accept control socket occur error\n");
				} else {
					
					char buffer[LINE];
					char msg[LINE];
					enum CONTROL_CMD control_cmd;
					int flag = 1;
					
					int read_count = read(connection, buffer, LINE);
					if(read_count <= 0) {
						printf("control port read occur error\n");
						flag = 0;
					}
		
					if(flag) {
						control_cmd = parse_c(buffer);
						switch(control_cmd) {
							case C_SHUTDOWN:
								doLock();
								global_run = 0;
								doUnlock();
								break;
							
							case C_COUNT:
								doLock();
								sprintf(msg,"%d",countItems());
								doUnlock();
								strcat(msg, "\n");
								write(connection, msg, strlen(msg));
								break;
							
							case C_ERROR:
								stpcpy(msg, "Control Error\n");
								write(connection, msg, strlen(msg));
								printf("Control Command Error\n");
								break;
							
							default:
								printf("---Magical Error in Control port---\n");
						}
							
						close(connection);
						
						if(global_run == 0) {
							doLock();
							while(handling_worker_count > 0) {
								int err;
								err = pthread_cond_wait(&handle_condition, &lock);
								if(err){
									printf("pthread_cond_wait in main run waiting NO handling workers error\n");
									abort();
								}
							}
							doUnlock();
							close(data_socket);
							close(control_socket);
							
							doLock();
							pthread_cond_broadcast(&handle_condition);
							doUnlock();
						}
					}

					
				}
								
			} else if(fds[0].revents & POLLIN) {
				// data port
				doLock();
				while(handling_worker_count >= NTHREADS) {
					int err;
					err = pthread_cond_wait(&handle_condition, &lock);
					if(err){
						printf("pthread_cond_wait in main run waiting available worker error\n");
						abort();
					}
				}
				
				int connection = accept(data_socket, (struct sockaddr*) &data_socket_sa, &data_socket_len);
				if(connection == -1) {
					printf("accept data socket occur error\n");
				} else {
					for (i = 0; i<NTHREADS; i++) {
						if(worker_con_fd[i] == -1) {
							worker_con_fd[i] = connection;
							break;
						}
					}
					pthread_cond_broadcast(&handle_condition);
				}
				doUnlock();
			}
		}
		
		
		
	}
	
	
	printf("---run end---\n");
	
	/*Wait for worker threads to finish*/
	for (i = 0; i<NTHREADS; i++) {
        pthread_join(workerT[i], NULL);
    }
	
}

/* You may add code to the main() function. */
int main(int argc, char** argv) {

	if (argc < 3) {
        printf("Usage: %s data-port control-port\n", argv[0]);
        exit(1);
	} else {
        cport = atoi(argv[1]);
        dport = atoi(argv[2]);
		printf("cport: %d dport: %d\n", cport, dport);
	}

	run();
	printf("run end in main thread\n");
    return 0;
}

