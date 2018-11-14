University of Victoria
CSC 360 Fall 2018
Italo Borrelli
V00884840

PMan is a process manager for creating and changing the status of processes. PMan allows the user to perform the following commands:

bg <process>         creates a new process as inputed by the user
bglist               lists all user created processes
bgkill <pid>         kills the process identified by it's pid
bgstop <pid>         stops the process identified by it's pid
bgstart <pid>        starts the process identified by it's pid
pstat <pid>          gives data on the process identified by it's pid
exit                 kills all user created processes and exits the program
