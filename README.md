# Scheduling-simulator-for-general-OS
Building own discrete-event process scheduling simulator for a general OS, with processes that are generated randomly in time where the inter-arrival time between two processes is chosen from an exponential distribution.

This program implements and compares the following strategies to organize process execution :
1. First come first serve
2. Round robin
3. Shortest job first
4. Shortest remaining time first
5. Multi level feedback queue

Processes are compared on parameters of average turnaround and response time. 

The program accepts input from a text file, the path to which can be specified as a command line argument. The input consists for one workload of processes. Each line of the input file represents a process. The structure of the line is as follows:
PID ArrivalTime JobTime

The output consists of the average turnaround and response times along with the blueprint for the given strategy. The blueprint consists of the job ID followed by the time it started, which is followed by the time it context switched out of that job ID.
