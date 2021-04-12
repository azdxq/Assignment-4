# Assignment-4
NOTE: This entire project is CPU bound as I have yet to figure out how to make it I/O bound
NOTE: The signal handlers do work, there is just so much printing that it could take a second for them to happen. 
NOTE: The randomness of this project can lead to varying speeds, so I would suggest running it a couple of times.

This project is invoked by the following command:

oss [-h] [-s t] [-l f]
-h Describe how the project should be run and then, terminate.
-s t Indicate how many maximum seconds before the system terminates
-l f Specify a particular name for the log file

This program simulates the part of the Operating System that schedules process and puts them into queues.
We are to have at max 18 processes at a time, and we are simulating a clock using seconds and nanoseconds as a way to show the scheduling at work.
It also can make the process either I/O Bound or CPU Bound.

Please comiple this project using the word "make" to run the file using the above commands.



Our suggested implementation steps for this project:

1. Create your Makefile and bare executables for oss and the child process.

2. Set up a system clock in shared memory and create a single user process, testing your synchronization method to
pass information and control between them.

3. Have oss simulate the scheduling of one user process over and over, logging the data but have no blocked queueProcess Scheduling 4

4. Now add the chance for your single child process to sometimes block and have oss wait until it is ready again to
reschedule

5. Create your round robin ready queue, add additional user processes, making all user processes alternate in it

6. Add the distinction between i/o bound and cpu bound processes

7. Add the chance for user processes to be blocked on an event, keep track of statistics on this.

8. Keep track of and output statistics like throughput, wait time, etc
