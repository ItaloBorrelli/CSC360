/***** pman.c ******************************************************************
 *
 * University of Victoria
 * CSC 360 Fall 2018
 * Italo Borrelli
 * V00884840
 *
 *******************************************************************************
 * pman is a process manager for creating and changing the status of processes.
 *
 * pman allows the user to perform the following commands:
 *
 * bg <process>		creates a new process as inputed by the user
 * bglist		lists all user created processes
 * bgkill <pid>		kills the process identified by it's pid
 * bgstop <pid>		stops the process identified by it's pid
 * bgstart <pid>	starts the process identified by it's pid
 * pstat <pid>		gives data on the process identified by it's pid
 * exit			kills all user created processes and exits the program
 ******************************************************************************/

#include<stdio.h>
#include<stdlib.h>

#include<ctype.h>			//isdigit()
#include<string.h>			//atoi()

#include<unistd.h>			//exec*(), fork(), sysconf()
#include<signal.h>			//kill()
#include<sys/types.h>			//pid_t
#include<sys/wait.h>			//waitpid(), WIFCONTINUED(), WIFEXITED(),
					//WIFSIGNALED(), WIFSTOPPED()

#include<readline/readline.h>		//readline()

#define MAX_INPUT 128			//limits user input for commands

typedef enum {false, true} bool;	//defined for greater code coherency


/*******************************************************************************
 * PARSING HELPER FUNCTIONS
 ******************************************************************************/

/*******************************************************************************
 * function: parse
 *******************************************************************************
 * Takes a raw input and parses it on spaces into an array of arguments.
 *
 * @param	char* input	points to the input string
 * @param	char** command	pointers that will identify the location of each
 * 				argument in the users string
 * @param	char* tok	char to use for parsing
 *
 * @return	void		no return value
 ******************************************************************************/

void parse(char* input, char** array, char* tok) {
	char* token = strtok(input, tok);

	int i;
	for(i = 0; i < MAX_INPUT; i++) {
		array[i] = token;
		token = strtok(NULL, tok);
	}
}


/*******************************************************************************
 * function: parse_pid
 *******************************************************************************
 * Takes the pid inputed by the user and converts it into an integer as a pid_t.
 *
 * Iterates through the individual characters of the array to ensure all the
 * values are digits before using atoi().
 *
 * If the users input has any non-number values in it the function returns -1.
 *
 * @param	char pid_string[]	char array of number values
 * @return	pid_t			return pid converted from string
 *
 * @error_handling			if not an integer returns -1
 ******************************************************************************/

pid_t parse_pid(char pid_string[]) {
	int i;
	for(i = 0; i < strlen(pid_string); i++) {
		if(!isdigit(pid_string[i])) return -1;
		i++;
	}

	return atoi(pid_string);
}


/*******************************************************************************
 * PROCESS LINKED LIST
 ******************************************************************************/

/*******************************************************************************
 * struct: node
 *******************************************************************************
 * Node for a linked list reprenting a process started by a user.
 *
 * @attr	pid_t pid	pid of this process
 * @attr	char* process	process name
 * @attr	bool running	true if running, false if stopped
 * @attr	node* next	pointer to next process node in list
 ******************************************************************************/

typedef struct node {
	pid_t pid;
	char* process;
	bool running;
	struct node* next;
} node;


/*******************************************************************************
 * global var: process_list_head
 *******************************************************************************
 * Global pointer pointing to the head of our process linked list.
 *
 * This is a global variable because it is simpler than declaring it in main
 * and passing it as a parameter because of it's ubiquitous use througout the
 * functions as well as the fact that it would be passed through functions that
 * do not directly use it.
 ******************************************************************************/

node* process_list_head = NULL;


/*******************************************************************************
 * function: add_process
 *******************************************************************************
 * Adds a new process to our linked list.
 *
 * Allocates memory for a new node then assigns the appropriate values of the
 * new node to the node. Then places the node at the end of the linked list.
 *
 * @param	pid_t pid	the pid of the process to add
 * @param	char* process	the name of the process to add
 *
 * @return	void		no return value
 *
 * @see				struct node
 * @see				node* process_list_head
 *
 * @error_handling		it is expected that other functions do not send
 *				an existing pid to add_process as it will be
 *				added again regardless
 ******************************************************************************/

void add_process(pid_t pid, char* process) {
	node* new_node = (node*)malloc(sizeof(node));

	new_node->pid = pid;
	new_node->process = process;
	new_node->running = true;
	new_node->next = NULL;

	if(process_list_head == NULL) {
		process_list_head = new_node;
	} else {
		node* current = process_list_head;
		while(current->next != NULL) current = current->next;
		current->next = new_node;
	}
}


/*******************************************************************************
 * function: remove_process
 *******************************************************************************
 * Removes a process from our linked list.
 *
 * Iterates through the linked list until the node with the given pid is found
 * and removes it by readjusting the pointers around it.
 *
 * @param	pid_t pid	the pid of the process to remove
 *
 * @return	void		no return value
 *
 * @see				struct node
 * @see				node* process_list_head
 *
 * @error_handling		there is no error handling for this function and
 * 				other functions using this function are expected
 * 				to ensure that if a pid is passed here it exists
 * 				in the linked list
 ******************************************************************************/

void remove_process(pid_t pid) {
	node* previous;
	node* current = process_list_head;

	while(current != NULL) {
		if(current->pid == pid) {
			if(current == process_list_head) {
				process_list_head = current->next;
				free(current);
			} else {
				previous->next = current->next;
				free(current);
			}
		}

		previous = current;
		current = current->next;
	}
}


/*******************************************************************************
 * function: find_process
 *******************************************************************************
 * Removes a process from our linked list.
 *
 * Iterates through the linked list until the node with the given pid is found
 * then returns a pointer to it or NULL if the node doesn't exist.
 *
 * @param	pid_t pid	the pid of the process to find
 *
 * @return	node*		returns pointer to node with pid parameter
 * 				returns NULL if there is no node with given pid
 *
 * @see				struct node
 * @see				node* process_list_head
 ******************************************************************************/

node* find_process(pid_t pid) {
	node* current = process_list_head;

	while(current != NULL) {
		if(current->pid == pid) {
			return current;
		}

		current = current->next;
	}

	return NULL;
}


/*******************************************************************************
 * PROCESS COMMANDS
 ******************************************************************************/

/*******************************************************************************
 * function: bg
 *******************************************************************************
 * Starts a new process in the background from the user inputed command.
 *
 * The function creates a fork of the process. When pid is 0 this is in the
 * child process. In this case a new process image with the users command is
 * created to replace this one.
 *
 * If the pid is greater than 0 this is a parent process and the newly created
 * process is added to the process linked list.
 *
 * If the pid is negative the fork didn't work and the user receives an error
 * message.
 *
 * @param	char** command	the pointers to the command array
 *
 * @return	void		no return value
 *
 * @see				void add_process(pid_t, char*)
 *
 * @error_handling		ERROR: Unable to execute command
 * 				given if execvp() was unable to execute
 *
 * @error_handling		ERROR: Failed to fork
 * 				given if fork() didn't execute properly
 ******************************************************************************/

void bg(char** command) {
	pid_t pid = fork();

	//child process
	if(pid == 0) {
		execvp(command[1], &command[1]);

		printf("ERROR: Unable to execute command\n");
		exit(1);
	//parent process
	} else if(pid > 0) {
		printf("Process started with PID %d\n", pid);

		add_process(pid, command[1]);
		sleep(1);
	//fork failure
	} else {
		printf("ERROR: Failed to fork");

	}
}


/*******************************************************************************
 * function: bglist
 *******************************************************************************
 * Iterates through the process linked list and prints off the pid and process
 * name of each node as well as the nodes state if it was stopped.
 *
 * @return	void		no return value
 *
 * @see				struct node
 * @see				node* process_list_head
 ******************************************************************************/

void bglist() {
	int n = 0;
	node* current = process_list_head;

	while(current != NULL) {
		printf("%d: %s", current->pid, current->process);
		if(!current->running) printf("\t(stopped)");
		printf("\n");
		n++;
		current = current->next;
	}

	printf("Total background jobs: %d\n", n);
}


/*******************************************************************************
 * function: bgkill
 *******************************************************************************
 * Kills the process given by the pid parameter with the TERM signal.
 *
 * @param	pid_t pid	pid of process to kill
 *
 * @return	void		no return value
 *
 * @see				node* find_process(pid)
 *
 * @error_handling		ERROR: PID doesn't exist
 * 				given if the pid isn't found by find_process()
 *
 * @error_handling		ERROR: Process termination failed
 * 				given if an error occurs from kill
 ******************************************************************************/

void bgkill(pid_t pid) {
	if(find_process(pid) == NULL) {
		printf("ERROR: PID doesn't exist\n");
		return;
	}

	if(!kill(pid, SIGTERM)) sleep(1);
	else printf("ERROR: Process termination failed\n");
}


/*******************************************************************************
 * function: bgstop
 *******************************************************************************
 * Stops the process given by the pid parameter with the STOP signal.
 *
 * @param	pid_t pid	pid of process to stop
 *
 * @return	void		no return value
 *
 * @see				node* find_process(pid)
 *
 * @error_handling		ERROR: PID doesn't exist
 * 				given if the pid isn't found by find_process()
 *
 * @error_handling		ERROR: Failed to stop process
 * 				given if an error occurs from kill
 ******************************************************************************/

void bgstop(pid_t pid) {
	if(find_process(pid) == NULL) {
		printf("ERROR: PID doesn't exist\n");
		return;
	}

	if(!kill(pid, SIGSTOP)) sleep(1);
	else printf("ERROR: Failed to stop process\n");
}


/*******************************************************************************
 * function: bgstart
 *******************************************************************************
 * Starts or continues the process given by the pid parameter with the CONT
 * signal.
 *
 * @param	pid_t pid	pid of process to start/continue
 *
 * @return	void		no return value
 *
 * @see				node* find_process(pid_t)
 *
 * @error_handling		ERROR: PID doesn't exist
 * 				given if the pid isn't found by find_process()
 *
 * @error_handling		ERROR: Process execution failed
 * 				given if an error occurs from kill
 ******************************************************************************/

void bgstart(pid_t pid) {
	if(find_process(pid) == NULL) {
		printf("ERROR: PID doesn't exist\n");
		return;
	}

	if(!kill(pid, SIGCONT)) sleep(1);
	else printf("ERROR: Process execution failed\n");
}


/*******************************************************************************
 * function: pstat
 *******************************************************************************
 * Gives details on the process identified by the given pid.
 *
 * pstat prints off, in order, the processes filename, state, utime, stime,
 * rss and the voluntary and nonvoluntary context switches.
 *
 * pstat parses the stat file of the process first for the first five items
 * then the status file of the process for the context switches.
 *
 * The file library is used for the opening of the files and is parsed with the
 * parse() function.
 *
 * @param	pid_t pid	pid of process to find stats and status on
 *
 * @return	void		no return value
 *
 * @see				void parse(char*, char**, char*)
 * @see				node* find_process(pid_t)
 *
 * @error_handling		ERROR: PID doesn't exist
 * 				given if the pid isn't found by find_process()
 ******************************************************************************/

void pstat(pid_t pid) {
	if(find_process(pid) == NULL) {
		printf("ERROR: PID doesn't exist\n");
		return;
	}

	//open stat file
	char stat_path[MAX_INPUT];
	sprintf(stat_path, "/proc/%d/stat", pid);
	FILE* stat_file = fopen(stat_path, "r");

	//get stat line from stat file
	char stat_line[MAX_INPUT];
	fgets(stat_line, sizeof(stat_line), stat_file);

	//create destination array of stat parsed by spaces
	char* stat_array[MAX_INPUT];
	parse(stat_line, stat_array, " ");

	//convert time strings to %lu
	char* p;
	unsigned long utime = strtoul(stat_array[13], &p, 10);
	unsigned long stime = strtoul(stat_array[14], &p, 10);

	//open status file
	char status_path[MAX_INPUT];
	sprintf(status_path, "/proc/%d/status", pid);
	FILE* status_file = fopen(status_path, "rt");

	//status variables
	char status_line[MAX_INPUT];
	char* vol_ctxt[MAX_INPUT];
	char* nonvol_ctxt[MAX_INPUT];
	char* parsed[MAX_INPUT];

	//iterates through all lines of the file until a match is found for
	//ctxt_switches
	while(fgets(status_line, MAX_INPUT, status_file) != NULL) {
		if(strncmp(status_line, "voluntary_ctxt_switches:", 24) == 0) {
			parse(status_line, parsed, "\t");
			strcpy(*vol_ctxt, parsed[1]);
		}

		if(strncmp(status_line, "nonvoluntary_ctxt_switches:", 27) == 0) {
			parse(status_line, parsed, "\t");
			strcpy(*nonvol_ctxt, parsed[1]);
		}
	}

	//print process details
	printf("comm:\t\t\t\t%s\n", stat_array[1]);
	printf("state:\t\t\t\t%s\n", stat_array[2]);
	printf("utime:\t\t\t\t%lu\n", utime / sysconf(_SC_CLK_TCK));
	printf("stime:\t\t\t\t%lu\n", stime / sysconf(_SC_CLK_TCK));
	printf("rss:\t\t\t\t%ld\n", (long)stat_array[23]);
	printf("voluntary_ctxt_switches:\t%d\n", atoi(*vol_ctxt));
	printf("nonvoluntary_ctxt_switches:\t%d\n", atoi(*nonvol_ctxt));

	//close files
	fclose(stat_file);
	fclose(status_file);
}


/*******************************************************************************
 * function: update
 *******************************************************************************
 * Checks the status of all child processes and updates their state in the
 * process linked list.
 *
 * Uses waitpid to find all child process and identify a status change.
 *
 * If the process was continued changes the running value of the node to true.
 *
 * If the process was stopped changes the running value of the node to false.
 *
 * If the process was killed or terminated changes the running value of the node
 * to true.
 *
 * WNOHANG used in case the child doesn't change process while being examined.
 *
 * @return	void		no return value
 *
 * @see				struct node
 * @see				void remove_process(pid_t)
 * @see				node* find_process(pid_t)
 ******************************************************************************/

void update() {
	pid_t pid;
	int wstatus;

	while(true) {
		pid = waitpid(-1, &wstatus, WNOHANG | WUNTRACED | WCONTINUED);
		if(pid > 0) {
			if(WIFCONTINUED(wstatus)) {
				printf("Process %d started\n", pid);

				node* process = find_process(pid);
				process->running = true;
			}

			if(WIFSTOPPED(wstatus)) {
				printf("Process %d stopped\n", pid);

				node* process = find_process(pid);
				process->running = false;
			}

			if(WIFSIGNALED(wstatus)) {
				printf("Process %d killed\n", pid);

				remove_process(pid);
			}

			if(WIFEXITED(wstatus)) {
				printf("Process %d terminated\n", pid);

				remove_process(pid);
			}
		} else break;
	}
}


/*******************************************************************************
 * GENERAL FUNCTIONS
 ******************************************************************************/

/*******************************************************************************
 * function: execute_command
 *******************************************************************************
 * Executes the command given in the command array
 *
 * Each if statement check if the command given is in the valid list of
 * commands and identifies which it is.
 *
 * If "bg" is passed first it is checked to ensure there is a command to
 * execute in the second index of the array before passing to bg().
 *
 * If "bglist" is passed bglist() is called.
 *
 * If "exit" is passed return true and bgkill all processes in linked list.
 *
 * If any of the other commands are passed the second index is checked to
 * ensure that it is a numerical value that can be used as a pid_t.
 *
 * @param	char** command	the pointers to the command array
 *
 * @return	bool		true only if exiting pman from "exit" command
 *
 * @see				struct node
 * @see				node* find_process(pid)
 *
 * @error_handling		ERROR: Invalid command (no process name)
 * 				given if there is no command given with bg
 *
 * @error_handling		ERROR: No PID or invalid PID
 * 				given if there is no pid given or the value is
 * 				not a number
 *
 * @error_handling		ERROR: Invalid command
 * 				given if the command given is not one of the
 * 				valid commands
 ******************************************************************************/

bool execute_command(char** command) {
	if(strcmp(command[0], "bg") == 0) {
		if(command[1] == NULL) {
			printf("ERROR: Invalid command (no process name)\n");
			return false;
		}

		bg(command);

		return false;
	} else if(strcmp(command[0], "bglist") == 0) {
		bglist();

		return false;
	} else if(strcmp(command[0], "bgkill") == 0) {
		pid_t pid = parse_pid(command[1]);

		if(command[1] == NULL || pid == -1) {
			printf("ERROR: No PID or invalid PID\n");
			return false;
		}

		bgkill(pid);

		return false;
	} else if(strcmp(command[0], "bgstop") == 0) {
		pid_t pid = parse_pid(command[1]);

		if(command[1] == NULL || pid == -1) {
			printf("ERROR: No PID or invalid PID\n");
			return false;
		}

		bgstop(pid);

		return false;
	} else if(strcmp(command[0], "bgstart") == 0) {
		pid_t pid = parse_pid(command[1]);

		if(command[1] == NULL || pid == -1) {
			printf("ERROR: No PID or invalid PID\n");
			return false;
		}

		bgstart(pid);

		return false;
	} else if(strcmp(command[0], "pstat") == 0) {
		pid_t pid = parse_pid(command[1]);

		if(command[1] == NULL || pid == -1) {
			printf("ERROR: No PID or invalid PID\n");
			return false;
		}

		pstat(pid);

		return false;
	} else if(strcmp(command[0], "exit") == 0) {
		node* current = process_list_head;
		while(current != NULL) {
			bgkill(current->pid);
			current = current->next;
		}

		return true;
	} else {
		printf("ERROR: Invalid command\n");

		return false;
	}
}


/*******************************************************************************
 * function: main
 *******************************************************************************
 * Iterates until process cancelled taking input to adjust processes.
 *
 * Prints a prompt for user input. Processes are updated, commands are executed
 * and processes are updated again so the process linked list is constantly up
 * to date with the statuses of all processes started by the user.
 *
 * If the execute_command() returns true the loop will update one more time to
 * deallocate memory of all nodes in the linked list.
 *
 * @return	int		N/A
 *
 * @see				void parse(char*, char**, char*)
 * @see				void update()
 * @see				void execute_command(char**)
 ******************************************************************************/

int main() {
	bool exiting = false;
	while(!exiting) {
		char* input = readline("PMan: > ");

		char* command[MAX_INPUT];

		if(strcmp(input, "") == 0) continue;
		else parse(input, command, " ");

		update();

		exiting = execute_command(command);

		update();
	}
}
