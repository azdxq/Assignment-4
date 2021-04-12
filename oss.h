#pragma once
#ifndef OSS_H
#define OSS_H

#include <stdlib.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <math.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include<sys/msg.h>

#define FORMATTED_TIME_SIZE 50
#define FORMATTED_TIME_FORMAT "%H:%M:%S"
#define thresh_hold_oss 100000
#define thresh_hold_user 70000
#define QUANTUM 50000
#define ALPHA 2.2
#define BETTA 2.8
#define MAX_PROCESSES 18
#define MAXCHAR 1024

typedef struct Clock {
	unsigned int seconds;
	unsigned int nanoseconds;
} simulatedClock;

typedef struct processControlBlock {
	pid_t processID;
    	int id;
    	int priority;
    	int burstTime;
    	int duration;
    	int waitTime;
	bool terminate;
} PCB;

typedef struct userProcess {
	pid_t processID;
	int id;
	int priority;
	int burstTime;
	int duration;
	int waitTime;
	bool terminate;
} USP;

typedef struct QNode {
	int id;
	struct QNode *next;
} qNode;

typedef struct queue {
	struct QNode *front;
	struct QNode *rear;
} queues;

typedef struct messages {
	long int mtype;
	int processID;
	int finishFlag;
	int id;
	int priority;
    	int duration;
	int burstTime;
	int seconds;
    	int nanoseconds;
    	char* message;
	int waitTime;
} message;

void createFile(char*);
void logOutput(char*, char*, ...);
int randomNumber(int, int);
long int convertNano(int);
queues* generateQueue();
char* getFormattedTime();
void sigact(int, void(int));

#endif
