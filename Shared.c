// March 2021
// The user process does busy work to simulate working for oss.c so it can update things like time and how many processes have gone through

#include "oss.h"

simulatedClock *clockpoint; // Special type declarations
PCB *pcb;
struct messages msg;

int main(int argc, char *argv[]) {
	int shmID, msgID, pcbSharedID, quantum, burstTime; // Creation of ID's and other variables
	pid_t userID = getpid();
	int pcbID = atoi(argv[1]);
	bool quit = false; // Assume we aren't done once we reach here

	key_t msgQueueKey = ftok("./oss.c", 21); // Creation of our keys 
	key_t clockKey = ftok("./oss.c", 22);
	key_t pcbKey = ftok("./oss.c", 23);
	
	msgID = msgget(msgQueueKey, 0666); // Allocation of messages and shared memory
	if ((shmID = shmget(clockKey, sizeof(struct Clock), IPC_CREAT | 0644)) < 0){
                printf("CLOCK USER: Failed access to shared clock\n");
                return EXIT_FAILURE;
        }	
	if ((pcbSharedID = shmget(pcbKey, sizeof(struct processControlBlock) * MAX_PROCESSES, IPC_CREAT | 0644)) < 0) {
        	printf("PCB USER: shmat failed on shared control block\n");
		return EXIT_FAILURE;
 	}

	pcb = shmat(pcbSharedID, NULL, 0); // Creation of our process block
	clockpoint = shmat(shmID, NULL, 0); // Creation of our clockpoint

	while (!quit) {  // We want to be here while we are not allowed out
		pcb[pcbID].terminate = false; // Say that it is not time to terminate
		while(1) { // By default keep looping
			int result = msgrcv(msgID, &msg, (sizeof(message) - sizeof(long)), getpid(), IPC_NOWAIT);
			
			if(result != -1) {
				printf("USER: recieved msg pid: %d\n", msg.processID);
				
				if(msg.processID == getpid()) { // Randomly decide the quantum
					quantum = rand()%2 == 0? QUANTUM : QUANTUM/2;
					int burstRand = rand()%2 == 1? 1 : 0; // creation of our random

					if(burstRand == 1) {
						burstTime = quantum;
					} else {
						burstTime = randomNumber(0, quantum);
					}
					
					pcb[pcbID].burstTime = burstTime;
					int randomNum = randomNumber(0, 4); // Grab a random number for duration of process
					
					if(randomNum == 0) { // In case it ends instantly
					   	pcb[pcbID].duration = 0;
					   	pcb[pcbID].waitTime = 0;
					   	pcb[pcbID].terminate = true;									
	
					} else if(randomNum == 1) { // Ends in a short amount of time
					   	pcb[pcbID].duration = burstTime;
					   	pcb[pcbID].waitTime = 0;
				
					} else if(randomNum == 2) { // Takes a bit longer
						int newSeconds = randomNumber(0, 5);
						int newMilis = randomNumber(0, 1000) * 1000000; // Creation of random milis
						int secondsToNano = convertNano(newSeconds); // Convert that and simulate that amount of time passing
						int totalNano = secondsToNano + newMilis;
						pcb[pcbID].waitTime = newMilis + totalNano;
						pcb[pcbID].duration = burstTime; // Stoe taht uration
				
					} else { 
					   double pValue = randomNumber(1, 99) / 100;
					   pcb[pcbID].duration = pValue * burstTime;
					   pcb[pcbID].waitTime = 0;
					}	
					
					msg.mtype = 1; // Store our information for the message queue
					msg.id = pcbID;
					msg.processID = userID;
					msg.seconds = clockpoint->seconds;
					msg.nanoseconds = clockpoint->nanoseconds;
					msgsnd(msgID, &msg, (sizeof(message) - sizeof(long)), 0);
					break;
				}
			}
		}
		
		unsigned int beginning = convertNano(clockpoint->seconds) + clockpoint->nanoseconds; // Start keeping track of time for this
		while(1) {
			pcb[pcbID].duration -= convertNano(clockpoint->seconds) + clockpoint->nanoseconds - beginning; // Keep adding on to the time

			if(pcb[pcbID].duration <= 0) {
				msg.mtype = 1; // Write necessary information to message
				msg.id = pcbID;
				msg.processID = userID;
				msg.nanoseconds = pcb[pcbID].burstTime;
				msgsnd(msgID, &msg, (sizeof(message) - sizeof(long)), 0);
				break;
			}
		}
		
		if(pcb[pcbID].terminate){ // Figure out if we are ready to terminate
			msg.finishFlag = 0;
		} else {
			msg.finishFlag = 1;
		}
		
		msg.mtype = 1; // Write the necessary information that will be displayed for the message
		msg.id = pcbID;
		msg.processID = userID;
		msg.burstTime = pcb[pcbID].burstTime;
		msg.duration = pcb[pcbID].duration;
		msg.waitTime = pcb[pcbID].waitTime + convertNano(clockpoint->seconds) + clockpoint->nanoseconds - beginning;
		
		msgsnd(msgID, &msg, (sizeof(message) - sizeof(long)), 0); // Allocate space for the message block

		msg.processID = -1;
		if(pcb[pcbID].terminate) { // Show that it is time to terminate
			printf("USER: termination\n");
			break;
		}
	}
	
	printf("USER: exiting\n"); // User exists and destroys some things that were created
	shmdt(clockpoint);
	shmdt(pcb);
	return EXIT_SUCCESS;
}
