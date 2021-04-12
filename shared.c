#include "oss.h"

void createFile(char* path) { // Creation of file
	FILE* newFile = fopen(path, "w"); // Take whatever char* was passed
		if (newFile == NULL) { // If there is a problem creating the file
			perror("Failed to touch file");
			exit(EXIT_FAILURE);
		}
	fclose(newFile); // Close file at end regardless
}

int linesInFile = 0;

void logOutput(char* path, char* fmt, ...) {
	if (linesInFile < 10000) {
		linesInFile++;
		FILE* fp = fopen(path, "a+"); // Open a file for writing
	
		if (fp == NULL) { // In case of failed logging to file
			perror("Failed to open file for logging output");
			exit(EXIT_FAILURE);
		}
		
		int n = 4096; 
		char buf[n];
		va_list args; // Intialize to grab all arguments for logging
		va_start(args, fmt);
		vsprintf(buf, fmt, args);
		va_end(args);	
		fprintf(fp, buf); // Writing to the file 
		fclose(fp);
	} 
}

int randomNumber(int min, int max) { // Function for creating random numbers
	srand(time(NULL));
	return rand()%(max - min) + min;
}

long int convertNano(int q){ // Function for converting to nanoseconds so we can print
	return q * 1000000000;

}

queues* generateQueue(){ // generation of different queues
        queues* starter = (struct queue*) malloc(sizeof(struct queue));
        starter->front = NULL; // default them as empty
        starter->rear = NULL;
        return starter;
}

char* getFormattedTime() { // Creation of formatted time, mostly for log file
        char* formattedTime = malloc(FORMATTED_TIME_SIZE * sizeof(char)); // allocate memory for it
        time_t now = time(NULL);
        strftime(formattedTime, FORMATTED_TIME_SIZE, FORMATTED_TIME_FORMAT, localtime(&now)); // format time we just recieved
        return formattedTime;
}

void sigact(int signum, void handler(int)) { // Creation of signals for timer and ctrl c in various spots of the program
	struct sigaction sa;
	if (sigemptyset(&sa.sa_mask) == -1) {
		perror("Sig set error");
		exit(EXIT_FAILURE);
	}
	sa.sa_handler = handler;
	sa.sa_flags = 0;
	if (sigaction(signum, &sa, NULL) == -1) { // Making sure sigaction is set correctly
		perror("Sigaction error");
		exit(EXIT_FAILURE);
	}
}
