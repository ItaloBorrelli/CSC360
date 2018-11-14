/***** acs.c *******************************************************************
 * University of Victoria
 * CSC 360 Fall 2018
 * Italo Borrelli
 * V00884840
 *******************************************************************************
 * acs.c creates an executable that represents an airline check-in system with
 * POSIX threads running simultaneously.
 ******************************************************************************/

#include<stdio.h>
#include<stdlib.h>

#include<pthread.h>
#include<unistd.h>

#include<readline/readline.h>
#include<sys/time.h>

#define MAX_CUST 265			// limit for number of customers
#define DECI_TO_NANO 100000		// value to convert decisec to nanosec
#define SEC_TO_NANO 1000000		// value to convert sec to nanosec

typedef enum {false, true} bool;	// defined for greater code coherency
typedef enum {id, priority, arrival, service} data;
					// defined for customer data access

// value holding the start time of customer arrivals
struct timeval start_time;

// shared resources for the threads
int customers[MAX_CUST][4];
int queue[2][MAX_CUST];
int queue_length[] = {0, 0};
int at_clerk[4] = {-1, -1, -1, -1};

// all shared POSIX thread functions
pthread_t threads[MAX_CUST];
pthread_mutex_t mutex;
pthread_cond_t cond;

// total counts for line waits
double economy_wait;
double business_wait;


/*******************************************************************************
 * HELPER FUNCTIONS: emptyClerk, parseCustomer, timeDiff
 ******************************************************************************/

/*******************************************************************************
 * function: emptyClerk
 *******************************************************************************
 * Get the first empty clerk available
 *
 * @return	int		index position of available clerk or -1 if none
 ******************************************************************************/

int emptyClerk() {
	int i;
	for(i = 0; i < 4; i++) {
		if(at_clerk[i] == -1) return i;
	}

	return -1;
}


/*******************************************************************************
 * function: parseCustomer
 *******************************************************************************
 * Use strtok to convert line describing customer into an array of ints.
 *
 * @param	int customer[]	pointer to where to put customer information
 * @param	char line[]	line to put into array
 * @param	int length	length of line
 * @param	int cust_num	customer number
 *
 * @return	void		no return value
 *
 * @error_handling		ERROR: Too many values for customer cust_num+1
 * 				If a customer has too many values associated
 * @error_handling		ERROR: Too few values for customer cust_num+1
 * 				If a customer has too few values associated
 ******************************************************************************/

void parseCustomer(int customer[], char line[], int length, int cust_num) {
	// change : to , to make it easier to parse
	int i;
	for(i = 0; i < length; i++) {
		if(line[i] == ':') { line[i] = ','; break; }
	}

	// use tokenizer to parse comma separated string into array
	int cnt = 0;
	char *ptr = strtok(line, ",");

	while (ptr != NULL) {
		// see @error_handling
		if (cnt == 4) {
			printf("ERROR: Too many values for customer %d\n", cust_num+1);
			exit(EXIT_FAILURE);
		}

		// put the value into position and update tokenizer
		customer[cnt] = atoi(ptr);
		ptr = strtok(NULL, ",");
		cnt++;
	}

	// see @error_handling
	if (cnt != 4) {
		printf("ERROR: Too few values for customer %d\n", cust_num+1);
		exit(EXIT_FAILURE);
	}
}


/*******************************************************************************
 * function: timeDiff
 *******************************************************************************
 * Return time relative to the start time in seconds.
 *
 * @return	float		difference between start and now
 ******************************************************************************/

double timeDiff() {
	struct timeval now_time;
	gettimeofday(&now_time, NULL);

	double start_us = (double)start_time.tv_sec*SEC_TO_NANO + (double)start_time.tv_usec;
	double now_us = (double)now_time.tv_sec*SEC_TO_NANO + (double)now_time.tv_usec;

	return (now_us - start_us) / SEC_TO_NANO;
}


/*******************************************************************************
 * QUEUE FUNCTIONS: enterQueue, exitQueue
 ******************************************************************************/

/*******************************************************************************
 * function: enterQueue
 *******************************************************************************
 * Put an arriving customer at the end of the queue according to the customers
 * priority.
 *
 * @param	int customer	arriving customer position in customers array
 *
 * @return	void		no return value
 ******************************************************************************/

void enterQueue(int customer) {
	int queue_num = customers[customer][priority];
	queue[queue_num][queue_length[queue_num]] = customer;
	queue_length[queue_num]++;
}


/*******************************************************************************
 * function: exitQueue
 *******************************************************************************
 * Take customer at front of specified queue out of that queue.
 *
 * @param	int queue_num	number of the queue we are dequeueing
 *
 * @return	void		no return value
 ******************************************************************************/

void exitQueue(int queue_num) {
	int i;
	for(i = 0; i < queue_length[queue_num]-1; i++) {
		queue[queue_num][i] = queue[queue_num][i+1];
	}

	queue_length[queue_num]--;
}


/*******************************************************************************
 * THREAD FUNCTIONS: getClerk, leaveClerk, threadMain
 ******************************************************************************/

/*******************************************************************************
 * function: getClerk
 *******************************************************************************
 * Get a clerk for the customer.
 *
 * Take the mutex lock and then wait for the customer to be in the front of the
 * queue, for an available clerk, and if in economy class for there to be no
 * business customers waiting.
 *
 * Broadcast the cond var after putting the customer at the first available
 * clerk then unlock the mutex.
 *
 * @param	int customer	arriving customer position in customers array
 *
 * @return	int		clerk the customer will be at
 *
 * @see				void enterQueue(int)
 * @see				double timeDiff()
 * @see				int emptyClerk()
 * @see				void exitQueue(int)
 *
 * @error_handling		ERROR: Failed to lock mutex
 * 				error state for mutex lock
 * @error_handling		ERROR: Failed to wait for cond signal
 * 				error state for condition wait
 * @error_handling		ERROR: Failed to broadcast signal
 * 				error state for broadcast
 * @error_handling		ERROR: Failed to unlock mutex
 * 				error state for mutex unlock
 ******************************************************************************/

int getClerk(int customer) {
	// see @error_handling
	if(pthread_mutex_lock(&mutex) != 0) {
		printf("ERROR: Failed to lock mutex\n");
		exit(EXIT_FAILURE);
	}

	enterQueue(customer);

	double enter_queue_time = timeDiff();
	printf("%.2fs\tCustomer %2d enters Queue%1d with a new length of %2d\n",
			enter_queue_time,
			customers[customer][id],
			customers[customer][priority],
			queue_length[customers[customer][priority]]);

	// condition wait until: empty clerk, front of queue
	if(customers[customer][priority] == 0) {
		// if priority is zero also wait for no business customers
		while(emptyClerk() == -1 || queue[0][0] != customer || queue_length[1] != 0) {
			// see @error_handling
			if(pthread_cond_wait(&cond, &mutex) != 0) {
				printf("ERROR: Failed to wait for cond signal\n");
				exit(EXIT_FAILURE);
			}
		}
	} else {
		while(emptyClerk() == -1 || queue[1][0] != customer) {
			// see @error_handling
			if(pthread_cond_wait(&cond, &mutex) != 0) {
				printf("ERROR: Failed to wait for cond signal\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	// leave the queue and put it at an available clerk
	int clerk = emptyClerk();
	at_clerk[clerk] = customer;
	exitQueue(customers[customer][priority]);

	// put wait times in counter
	if(customers[customer][priority] == 0) {
		economy_wait = timeDiff() - enter_queue_time;
	} else {
		business_wait = timeDiff() - enter_queue_time;
	}

	// see @error_handling
	if(pthread_cond_broadcast(&cond) != 0) {
		printf("ERROR: Failed to broadcast signal\n");
		exit(EXIT_FAILURE);
	}

	// see @error_handling
	if(pthread_mutex_unlock(&mutex) != 0) {
		printf("ERROR: Failed to unlock mutex\n");
		exit(EXIT_FAILURE);
	}

	return clerk;
}


/*******************************************************************************
 * function: leaveClerk
 *******************************************************************************
 * Take a customer off the clerk serving it.
 *
 * Take the mutex lock and then leave the clerk so it is available for a new
 * customer. Broadcast the cond var and release the lock.
 *
 * @param	int customer	customer position in customers array
 *
 * @return	void		no return value
 *
 * @error_handling		ERROR: Failed to lock mutex
 * 				error state for mutex lock
 * @error_handling		ERROR: Failed to broadcast signal
 * 				error state for broadcast
 * @error_handling		ERROR: Failed to unlock mutex
 * 				error state for mutex unlock
 ******************************************************************************/

void leaveClerk(int customer) {
	// see @error_handling
	if(pthread_mutex_lock(&mutex) != 0) {
		printf("ERROR: Failed to lock mutex\n");
		exit(EXIT_FAILURE);
	}

	// replace the clerk for this customer with -1
	int i;
	for(i = 0; i < 4; i++) {
		if(at_clerk[i] == customer) at_clerk[i] = -1;
	}

	// see @error_handling
	if(pthread_cond_broadcast(&cond) != 0) {
		printf("ERROR: Failed to broadcast signal\n");
		exit(EXIT_FAILURE);
	}

	// see @error_handling
	if(pthread_mutex_unlock(&mutex) != 0) {
		printf("ERROR: Failed to unlock mutex\n");
		exit(EXIT_FAILURE);
	}
}


/*******************************************************************************
 * function: threadMain
 *******************************************************************************
 * Start point for customer threads.
 *
 * Wait for customer arrival, get a clerk for it after waiting in queue, wait
 * for the service time to pass and then leave the clerk.
 *
 * @param	void* param	param of int cast to a void pointer
 *
 * @return	void*		no return value
 *
 * @see				double timeDiff()
 * @see				int getClerk(int)
 * @see				void leaveClerk(int)
 ******************************************************************************/

void *threadMain(void *param) {
	int customer = *((int*) param);

	// wait for arrival
	usleep(customers[customer][arrival] * DECI_TO_NANO);
	printf("%.2fs\tCustomer %2d arrives\n",
			timeDiff(),
			customers[customer][id]);

	// get a clerk for the customer
	int clerk = getClerk(customer);

	// start service and wait for service to complete
	printf("%.2fs\tClerk     %1d begins servicing Customer %2d\n",
			timeDiff(),
			clerk+1,
			customers[customer][id]);

	usleep(customers[customer][service] * DECI_TO_NANO);

	printf("%.2fs\tClerk     %1d finishes servicing Customer %2d\n",
			timeDiff(),
			clerk+1,
			customers[customer][id]);

	// make the clerk available again
	leaveClerk(customer);

	pthread_exit(NULL);
}


/*******************************************************************************
 * GENERAL FUNCTIONS: main
 ******************************************************************************/

/*******************************************************************************
 * function: main
 *******************************************************************************
 * Entry point for acs.
 *
 * Open the file and create a list of all customers, prepare all pthread
 * functions and attributes then send it to threadMain.
 *
 * @param	int argc	number of commandline arguments
 * @param	char *argv[]	array of commandline arguments
 *
 * @return	int		N/A
 *
 * @see				void parse_customer(int[], char[], int, int)
 * @see				void *threadMain(void *)
 *
 * @error_handling		ERROR: Usage "ACS <customer_file_name>"
 * 				user passed wrong number of arguments
 * @error_handling		ERROR: No file
 * 				file passed to fopen does not exist
 * @error_handling		ERROR: Less customers than declared
 * 				there are not enough customer lines
 * @error_handling		ERROR: More customers than declared
 * 				there are too many customer lines
 * @error_handling		ERROR: Failed to initialize attribute
 * 				attribute init failed
 * @error_handling		ERROR: Failed to make pthread joinable
 * 				making attribute failed
 * @error_handling		ERROR: Failed to initialize mutex
 * 				mutex init fail state
 * @error_handling		ERROR: Failed to initialize cond var
 * 				cond var init fail state
 * @error_handling		ERROR: Failed to create pthread
 * 				an instance of creating the pthread failed
 * @error_handling		ERROR: Failed to join pthread
 * 				some pthread was not able to be joined
 ******************************************************************************/

int main(int argc, char *argv[]) {
	// see @error_handling
	if(argc != 2) {
		printf("ERROR: Usage \"ACS <customer_file_name>\"\n");
		exit(EXIT_FAILURE);
	}

	// open file
	FILE *fptr = fopen(argv[1], "r");
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	// see @error_handling
	if(fptr == NULL) {
		printf("ERROR: No file\n");
		exit(EXIT_FAILURE);
	}

	// get first line with number of customers
	read = getline(&line, &len, fptr);

	// initialize values for customer data
	int num_customers = atoi(line), economy_count, business_count;

	economy_count = 0;
	business_count = 0;
	// go through each customer line and parse line
	int i;
	for(i = 0; i < num_customers; i++) {
		read = getline(&line, &len, fptr);

		// see @error_handling
		if(read < 1) {
			printf("ERROR: Less customers than declared in first line\n");
			exit(EXIT_FAILURE);
		}

		parseCustomer(customers[i], line, read, i);
	}

	// see @error_handling
	if(getline(&line, &len, fptr) > 0) {
		printf("ERROR: More customers than declared in first line\n");
		exit(EXIT_FAILURE);
	}

	// make pthread attribute and make it joinable
	pthread_attr_t attr;
	// see @error_handling
	if(pthread_attr_init(&attr) != 0) {
		printf("ERROR: Failed to initialize attribute\n");
		exit(EXIT_FAILURE);
	}

	// see @error_handling
	if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE) != 0) {
		printf("ERROR: Failed to make pthread joinable\n");
		exit(EXIT_FAILURE);
	}

	// see @error_handling
	if(pthread_mutex_init(&mutex, NULL) != 0) {
		printf("ERROR: Failed to initialize mutex\n");
		exit(EXIT_FAILURE);
	}

	// see @error_handling
	if(pthread_cond_init(&cond, NULL) != 0) {
		printf("ERROR: Failed to initialize cond var\n");
		exit(EXIT_FAILURE);
	}

	// set start_time
	gettimeofday(&start_time, NULL);

	// create thread for each customer
	// using array of cust numbers because i is not safe to pass
	int safe_array[num_customers];
	for(i = 0; i < num_customers; i++) {
		if(customers[i][priority] == 0) economy_count++;
		else business_count++;

		safe_array[i] = i;

		// see @error_handling
		if(pthread_create(&threads[i], &attr, threadMain, (void*)&safe_array[i]) != 0) {
			printf("ERROR: Failed to create pthread\n");
			exit(EXIT_FAILURE);
		}
	}

	// join all pthreads when they are complete
	for(i = 0; i < num_customers; i++) {
		// see @error_handling
		if(pthread_join(threads[i], NULL) != 0) {
			printf("ERROR: Failed to join pthread\n");
			exit(EXIT_FAILURE);
		}
	}

	printf("\n");

	if(economy_count != 0) {
		double economy_avg = economy_wait / economy_count;
		printf("The average waiting time for all economy-class customers is: %.2f seconds\n",
				economy_avg);
	}

	if(business_count != 0) {
		double business_avg = business_wait / business_count;
		printf("The average waiting time for all business-class customers is: %.2f seconds\n",
				business_avg);
	}

	if(economy_count != 0 && business_count != 0) {
		printf("The average waiting time for all customers in the system is: %.2f seconds\n",
				(economy_wait + business_wait) / num_customers);
	}
}
