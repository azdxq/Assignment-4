// The purpose of this project is to simulate an OS and how it schedules processes
// You can input the name of the logfile and time in which it will stop, but apart from that this simulation works on it's own
// It is highly recommended to run this program multiple times, as various speeds can happen because of some of the randomness

#include "oss.h"

USP* userProcess(int, pid_t); // Process and queue related declarations
void copyUser(PCB*, USP*);
void pushQueue(struct queue*, int);
struct QNode * popQueue(queues*);

void semLockClock(); // Simulation clock related declarations 
void semReleaseClock();
void fixTime();

void setupTimer(int); // Signals and termination related declarations
void myHandler(int);
void endProcesses();
void helpMenu();

PCB* pcb; // Special type declarations
simulatedClock* clockpoint;
message msg;
struct queue* lowQueue; // Queue delcarations
struct queue* medQueue;
struct queue* highQueue;

pid_t parentPID, childID;
struct sembuf sem_op; // Semaphore buffer declaration
int semID, clockSharedID, pcbSharedID, msgQueueID; // ID delcarations
long int totalNano; // Totals
bool quit;
float totalWait = 0.0;
unsigned char bit_map[3];

int main(int argc, char *argv[]) { // Main function of oss.c
	int character, workingID, timeSec = 3, optargCount = 0, totalInQueue = 0, id = 0, totalUsers = 0, whichQueue = 0;
	char* logfile;
	bool allDigit = true, fileCreated = false;
	quit = false;
	
	parentPID = getpid();

	while ((character = getopt(argc, argv, "l:s:h")) != -1) {
		switch (character) {
			case 'l':
				logfile = optarg; // Put down what the log file is
				createFile(logfile); // Create it
				fileCreated = true;
                                continue;
			case 's':
				allDigit = true;
				for (optargCount = 0; optargCount < strlen(optarg); optargCount++) { // Check if optarg is digit
					if(!isdigit(optarg[optargCount])) {
						allDigit = false;
						break;
					}
				}
				if (allDigit) { // See if we meet requirements to change timer
					if (atoi(optarg) > 0) {
						timeSec = atoi(optarg);
					}
				} else {
					errno = 22; // Show that this is not valid
					perror("-t requires a valid argument\n");
					helpMenu();
					return EXIT_FAILURE;
				}
				continue;
			case 'h': // Display help menu and end
				helpMenu();
                                return EXIT_SUCCESS;
			default:
				printf("Input not valid\n"); // Display help menu if something is wrong
				helpMenu();
				return EXIT_FAILURE;
		}
	}

	if (!fileCreated) { // If file was not specified, make a default one
		logfile = "logfile";
		createFile(logfile);
	}

	key_t msgQueueKey = ftok("./oss.c", 21); // Creation of keys used for messages, shared memory, and semaphores 
	key_t clockKey = ftok("./oss.c", 22);
	key_t pcbKey = ftok("./oss.c", 23);
	key_t semKey = ftok("./oss.c", 24);

	msgQueueID = msgget(msgQueueKey, IPC_CREAT | 0666); // Creation of message queue	
	if ((clockSharedID = shmget(clockKey, sizeof(clockpoint), IPC_CREAT | 0600)) < 0) { // Create shared memory for the clock
                printf("CLOCK OSS: shmat failed on shared message\n");
                return EXIT_FAILURE;
        }
	if ((pcbSharedID = shmget(pcbKey, sizeof(struct processControlBlock) * MAX_PROCESSES, IPC_CREAT | 0666)) < 0) { // Create shared memory for process control block
                printf("PCB OSS: shmat failed on shared message\n");
                return EXIT_FAILURE;
        }
        semID = semget(semKey, 2, IPC_CREAT | IPC_EXCL | 0666); // Creation of the semaphores
        if(semID == -1) {
                printf("SEM OSS: Failed semget\n");
                return EXIT_FAILURE;
        }
        semctl(semID, 0, SETVAL, 1); // Setting the different semaphores
        semctl(semID, 1, SETVAL, 1);

	sigact(SIGINT, myHandler);
	setupTimer(timeSec); // Setting up the timer	

    	clockpoint = shmat(clockSharedID, NULL, 0); // Create our simulation clock after setting up our timer
	clockpoint->seconds = 0;
	clockpoint->nanoseconds = 0;
    	pcb = shmat(pcbSharedID, NULL, 0);
	highQueue = generateQueue(); // Creation of our different queues
	medQueue = generateQueue();
	lowQueue = generateQueue();
	char index_str[MAXCHAR];
	float medQueueWait = 0.0; // Establish wait time for specific queues
	float lowQueueWait = 0.0;
	bool bitAccessable;
	
	while(!quit) { // Make sure we have no quit the simulation
		bitAccessable = false;
		int count = 0;
		while(1) { // Loop to keep checking if we are allowed to create another user
			id = (id + 1) % 3;
			uint32_t bit = bit_map[id / 8] & (1 << (id % 8));
			
			if(bit == 0){ // Good to go for another user
				bitAccessable = true;
				break;
			} else { // Do not make another user
				bitAccessable = false;
			}

			if(count >= 3 - 1) { // Show that we cannot make another user right this moment
				break;
			}
			count++;
		}

		if(totalUsers < 100) { // Make sure we are not over the 100 processes allowed total
			if(bitAccessable) {
				childID = fork(); // Creation of user
				if(childID == 0) {	
					execl("./user", "./user", index_str, NULL); // Send user directly to start doing busywork
				} else if (childID < 0) { // If problem with fork
					endProcesses();
					break;
				} else { // Parent will go here and update information, such as updated total users
					totalUsers++;
					bit_map[id / 8] |= (1 << (id % 8));	
					USP* userPro = userProcess(id, childID);
					copyUser(&pcb[id], userPro);	
					printf("OSS: Generating process with PID %d and putting it in queue %d at time %d:%d\n", pcb[id].processID, pcb[id].priority, clockpoint->seconds, clockpoint->nanoseconds);
					logOutput(logfile,"%d OSS: Generating process with PID %d and putting it in queue %d at time %d:%d\n", 
						getFormattedTime(), pcb[id].processID, pcb[id].priority, clockpoint->seconds, clockpoint->nanoseconds); // Log that a process was generated
					pushQueue(highQueue, id); // Push the new id into the queue
					pcb[id].priority = 0; // Default priority will be highest
				}
			}
		} else { 
			printf("OSS: Reached max 100 processes\n"); // We want to end after 100 processes
			endProcesses(); // Call out terminator 
			break;
			
		}

		struct QNode nextProcess;
		if(whichQueue == 0) { // Check which queue the next process will go into
			nextProcess.next = highQueue->front;
		} else if(whichQueue == 1) { 
			nextProcess.next = medQueue->front;
		} else {
			nextProcess.next = lowQueue->front;
		}

		struct queue *tempQueue = generateQueue(); // temperary queue for later

		while(nextProcess.next != NULL) { // Make sure there is a next process
			totalInQueue++; // Add to the total of processes waiting in queue
			
			semLockClock(); // Simulate the OS' clock
			clockpoint->nanoseconds += 1000;
			fixTime();
			semReleaseClock();

			workingID = nextProcess.next->id; 
			msg.mtype = pcb[workingID].processID; // Make sure to properly put id's for new message
			msg.id = workingID;
			msg.processID = pcb[workingID].processID;
			msg.priority = whichQueue;
			pcb[workingID].priority = whichQueue;

			msgsnd(msgQueueID, &msg, (sizeof(message) - sizeof(long)), 0);
			printf("OSS: Signaling process with PID %d from queue %d to dispatch\n", msg.processID, whichQueue);	
			logOutput(logfile, "%d OSS: Signaling process with PID %d from queue %d to dispatch\n", getFormattedTime(), msg.processID, whichQueue); // Show process' intial signal to dispatch
		
			semLockClock(); // Simulate the OS' clock
			clockpoint->nanoseconds += 1000;
			fixTime();
			semReleaseClock();

			msgrcv(msgQueueID, &msg, (sizeof(message) - sizeof(long)), 1, 0);
			printf("OSS: Dispatching process with PID %d from queue %d at time %d.%d\n", msg.processID, whichQueue, msg.seconds, msg.nanoseconds);
			logOutput(logfile, "%d OSS: Dispatching process with PID %d from queue %d at time %d.%d\n", getFormattedTime(), msg.processID, whichQueue, msg.seconds, msg.nanoseconds); // Show process' intial dispatch and queue

			semLockClock(); // Simulate the OS' clock
			clockpoint->nanoseconds += 1000;
			fixTime();
			semReleaseClock();

			totalNano = convertNano(clockpoint->seconds) + clockpoint->nanoseconds - convertNano(msg.seconds) + msg.nanoseconds; // Calculating of total time	
			printf("OSS: total time in dispatch was %li nanoseconds\n", totalNano);
			logOutput(logfile, "%d OSS: total time in dispatch was %li nanoseconds\n", getFormattedTime(), totalNano); // Checking of total dispatch

			while(1) { // Want to check until it has left critical
				semLockClock(); //Simulate the OS' Clock
				clockpoint->nanoseconds += 1000;
				fixTime();
				semReleaseClock();
				
				int leftCritical = msgrcv(msgQueueID, &msg, (sizeof(message) - sizeof(long)), 1, IPC_NOWAIT); // Check if we have left the critical section
				if(leftCritical != -1) {
					printf("OSS: Receiving that process with PID %d ran for %li nanoseconds\n", msg.processID, (convertNano(msg.seconds) + msg.nanoseconds));
					logOutput(logfile, "%d OSS: Recieved that process with PID %d ran for %li nanoseconds\n", getFormattedTime(), msg.processID, (convertNano(msg.seconds) + msg.nanoseconds)); 
					// Recieve how long a signle process ran
					break;
				}
			}
			
			semLockClock(); // Simulate the OS' Clock
			clockpoint->nanoseconds += 1000;
			fixTime();
			semReleaseClock();

			msgrcv(msgQueueID, &msg, (sizeof(message) - sizeof(long)), 1, 0); // Allocate size and test if we are finished with this 
			if(msg.finishFlag == 0){
				printf("OSS: OSS has ran for %d nanoseconds\n", clockpoint->nanoseconds);
				logOutput(logfile, "%d OSS: OSS has ran for %d nanoseconds\n", getFormattedTime(), clockpoint->nanoseconds); // Log total os time
				totalWait += msg.waitTime; // Add to the total wait time as well
			} else {
				if(whichQueue == 0) { // Check which queue this was apart of
					if(msg.waitTime > (ALPHA * medQueueWait)) { // Simulate quantum accordingly
						printf("OSS: Putting process with PID %d to queue 1\n", msg.processID);
						logOutput(logfile, "%d OSS: Putting process with PID %d to queue 1\n", getFormattedTime(), msg.processID); // Print which queue it is going to
						pushQueue(medQueue, workingID);
						pcb[workingID].priority = 1; // Update the priority
					} else {
						printf("OSS: Not using its entire time quantum.\n"); // If not using entire time quantum
						logOutput(logfile, "%d OSS: Not using its entire time quantum\n", getFormattedTime());
						pushQueue(tempQueue, workingID);
						pcb[workingID].priority = 0;
					}
				} else if(whichQueue == 1) {
					if(msg.waitTime > (BETTA * lowQueueWait)) { // Simulate quantum accordingly
						printf("OSS: Putting process with PID %d to queue 2\n", msg.processID);
						logOutput(logfile, "%d OSS: Putting process with PID %d to queue 2\n", getFormattedTime(), msg.processID);
						pushQueue(lowQueue, workingID);
						pcb[workingID].priority = 2; // Update priority
					} else {
						printf("OSS: Not using its entire time quantum.\n"); // If not using entire time quantum
						logOutput(logfile, "%d OSS: Not using its entire time quantum.\n", getFormattedTime());
						pushQueue(tempQueue, workingID);
						pcb[workingID].priority = 1;
					}
				} else { // In case of problem, default to queue 2
					printf("OSS: Keeping process with PID %d in queue 2\n", msg.processID);
					logOutput(logfile, "%d OSS: Keeping process with PID %d in queue 2\n", getFormattedTime(), msg.processID); // Log which queue it is going to
					pushQueue(tempQueue, workingID);
					pcb[workingID].priority = 2;
				}
				totalWait += msg.waitTime; // Storing the total wait time of the system
			}

			if(nextProcess.next->next != NULL) { // Update what the next process is
				nextProcess.next = nextProcess.next->next;
			} else {
				nextProcess.next = NULL;
			}
		}

		if(totalInQueue == 0) { // Assume that one is always in the queue
			totalInQueue = 1;
		}

		if(whichQueue == 1) { // Storing separate wait's for which queue it is
			medQueueWait = (totalWait / totalInQueue);
		} else if(whichQueue == 2) {
			lowQueueWait = (totalWait / totalInQueue);
		}
		

		int tempID = 0;
		if(whichQueue == 0) { // Figure out which queue we are going to use
			while(highQueue->rear != NULL) { // High queue scenario
				popQueue(highQueue);
			}
			while(tempQueue->rear != NULL) {
				tempID = tempQueue->front->id;
				pushQueue(highQueue, tempID);
				popQueue(tempQueue);
			}
		}
		else if(whichQueue == 1) {
			while(medQueue->rear != NULL) { // Medium queue scenario
				popQueue(medQueue);
			}
			while(tempQueue->rear != NULL) {
				tempID = tempQueue->front->id;
				pushQueue(medQueue, tempID);
				popQueue(tempQueue);
			}
		} else {
			while(lowQueue->rear != NULL) { // Low queue senario
				popQueue(lowQueue);
			}
			while(tempQueue->rear != NULL) {
				tempID = tempQueue->front->id;
				pushQueue(lowQueue, tempID);
				popQueue(tempQueue);
			}
		}
		free(tempQueue); // Free this up before going through the loop again
		
		whichQueue = (whichQueue + 1) % 3;
		
		semLockClock(); // Shifting the clock to simulate the OS' time
		clockpoint->nanoseconds += 1000;
		fixTime();
		semReleaseClock();

		int stat;
		pid_t removePID = waitpid(-1, &stat, WNOHANG);	// Non block wait for parent

		if(removePID > 0){ // Removal of all those pid's from the bit map and control block
			int pos;
			for(pos=0; pos<18;pos++){
				if(pcb[pos].processID == removePID){
					bit_map[pcb[pos].id / 8] &= ~(1 << (pcb[pos].id % 8));
				}
			}
		}

	}

	shmdt(clockpoint); // Deallocate everything we have used
	shmdt(pcb);
	msgctl(msgQueueID, IPC_RMID, NULL);
	shmctl(clockSharedID, IPC_RMID, NULL);
	shmctl(pcbSharedID, IPC_RMID, NULL);
        semctl(semID, 0, IPC_RMID);
        semctl(semID, 1, IPC_RMID);
	return EXIT_SUCCESS;
}

USP* userProcess(int index, pid_t childID) {
	USP* userPro = (struct userProcess*) malloc(sizeof(struct userProcess)); // Creation of a user process in shared
	userPro->id = index;
	userPro->processID = childID; // Storing it's id
	userPro->priority = 0; // Default priority
	return userPro;
}

void copyUser(PCB *pcb, USP *userProcess) { // Copying the user process
	pcb->id = userProcess->id;
    	pcb->processID = userProcess->processID;
	pcb->priority = userProcess->priority;
}

void pushQueue(struct queue* q, int id) { // Pushing to the queue
	qNode *newNode = (struct QNode *)malloc(sizeof(struct QNode)); // Creation of a new node in the queue
	newNode->id = id; // Store it's new id
	newNode->next = NULL;	

	if(q->rear == NULL){ 
		q->front = q->rear = newNode;
		return;
	}

	q->rear->next = newNode;
	q->rear = newNode;	
}

qNode* popQueue(queues* queue) { // Popping from the queue to check what processes are there
        if(queue->front == NULL) { // Check if there are any to pop
                return NULL;
        }

        qNode *temp = queue->front; // Otherwise grab the top
        free(temp);

        queue->front = queue->front->next;

        if (queue->front == NULL) { // If after that top is null, nothing on bottom either
                queue->rear = NULL;
        }

        return temp;
}

void fixTime(){ // Function used to fix the time if seconds have passed
    	if((int)(clockpoint->nanoseconds / 1000000000) == 1){
        	clockpoint->seconds++;
        	clockpoint->nanoseconds -= 1000000000;
    	}
}


void semLockClock() { // Function to lock the clock
	sem_op.sem_num = 0;
	sem_op.sem_op = -1;
	sem_op.sem_flg = 0;
	semop(semID, &sem_op, 1);
}


void semReleaseClock() { // Function to release the clock
    	sem_op.sem_num = 0;
    	sem_op.sem_op = 1;
    	sem_op.sem_flg = 0;
    	semop(semID, &sem_op, 1);
}


void setupTimer(int timeSec) { // Creation of timout incase we run out of time
	sigact(SIGALRM, myHandler);
	sigact(SIGUSR1, myHandler);    

	struct itimerval timer;
    	timer.it_value.tv_sec = timeSec;
    	timer.it_value.tv_usec = 0;
    	timer.it_interval.tv_sec = 0;
    	timer.it_interval.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
		perror("Failed to set timer");
		exit(EXIT_FAILURE);		
	}
}

void myHandler(int s){ // When a signal is caught
	printf("\nObtained interrupt signal, terminating...\n"); // Display for the user that we are terminating
	int i;
	for(i=0; i<20; i++){ // Delete all of the possible processes
        	if(pcb[i].processID != 0){
            		if(kill(pcb[i].processID, 0) == 0){ // Kill them
                		if(kill(pcb[i].processID, SIGTERM) != 0){
                    			perror("Child unable to terminate");
                		}
            		}
        	}
    	}

    	for(i=0;i<20;i++){ // Wait just incase something is wrong
		if(pcb[i].processID != 0){
            		waitpid(pcb[i].processID, NULL, 0);
        	}
    	}

	shmdt(clockpoint); // make sure to deallocate everything we used
	shmdt(pcb);
	msgctl(msgQueueID, IPC_RMID, NULL);
    	shmctl(clockSharedID, IPC_RMID, NULL);
    	shmctl(pcbSharedID, IPC_RMID, NULL);
    	semctl(semID, 0, IPC_RMID);
    	semctl(semID, 1, IPC_RMID);
	printf("\nTerminated.\n");
    	abort();
}

void endProcesses(){ // Terminator function when we reach 100 processes
	printf("Terminating...\n");
	int i;

	for(i=0; i<20; i++){ // Delete all possible processes in case more are waiting beyound the 100
        	if(pcb[i].processID != 0){
            		if(kill(pcb[i].processID, 0) == 0){
                		if(kill(pcb[i].processID, SIGTERM) != 0){
                    			perror("Child unable to terminate\n");
                		}
            		}	
        	}
    	}

    	for(i=0;i<20;i++){ // Wait on those possible processes
		if(pcb[i].processID != 0){
            		waitpid(pcb[i].processID, NULL, 0);
        	}
    	}
	printf("Terminated.\n");
	quit = true; // Mark that we are done with the oss simulation
}

void helpMenu() { // Help menu
	printf("This program can only take in a time, a name of a log file, and this current help function.  The rest is done by simulating an Operating System's scheduling.\n");
	printf("The follow commands would be inputting after ./oss : -s *number* to specify the amount of seconds until timeout.\n");
	printf("-l *name* to specify the name of the logfile you wish to have the information stored into.\n");
	printf("-h  which simply displays this help menu again.\n");
}
