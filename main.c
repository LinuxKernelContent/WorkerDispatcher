#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_NUM_THREADS		4096
#define MAX_NUM_COUNTERS	100
#define MAX_COUNTER_NAME	10
#define MAX_LINE_SIZE		1024

/*
 * This job struct holds the worker commands.
 */
typedef struct job {
	char *worker_cmd[MAX_LINE_SIZE];
	int num_cmd;
} job_t;

/*
 * This is the share Q for threads.
 */
job_t job_queue[256];
int job_count = 0;

pthread_mutex_t mutex_queue;
pthread_cond_t cond_queue;

void execute_job(job_t* job) {
}

void submit_job(job_t job) {
	pthread_mutex_lock(&mutex_queue);
	job_queue[job_count] = job;
	job_count++;
	pthread_mutex_unlock(&mutex_queue);
	pthread_cond_signal(&cond_queue);
}

void* start_thread(void* args) {
	while (1) {
		job_t job;

		pthread_mutex_lock(&mutex_queue);
		while (job_count == 0) {
			pthread_cond_wait(&cond_queue, &mutex_queue);
		}

		job = job_queue[0];
		int i;
		for (i = 0; i < job_count - 1; i++) {
			job_queue[i] = job_queue[i + 1];
		}
		job_count--;
		pthread_mutex_unlock(&mutex_queue);
		execute_job(&job);
	}
}


/*
 * This function check that args fron user in they limit.
 */
int validate_args(int num_threads, int num_counters, int log_enable)
{
	if (num_threads > MAX_NUM_THREADS || num_counters > MAX_NUM_COUNTERS ||
		(log_enable != 0 && log_enable != 1))
		return 1;

	return 0;
}

/*
 * This function open counter files.
 */
int init_file_arr(FILE **files_arr, int num_counters)
{
	int i;
	char counter_file_name[MAX_COUNTER_NAME];

	for (i = 0; i < num_counters; i++) {
		sprintf(counter_file_name, "counter%.2d", i);
		files_arr[i] = fopen(counter_file_name, "w+");
		fprintf(files_arr[i], "%d", 0);
	}

	return 0; 
}

/*
 * This function close counter files.
 */
int close_files_arr(FILE **files_arr, int num_counters)
{
	int i;

	for (i = 0; i < num_counters; i++) {
		fclose(files_arr[i]);
	}

	return 0;
}

/*
 * This is the worker function.
 */
void *worker_function(void *vargp)
{
	/* need to sleep untill dispatcher wake him -> start work */
	sleep(1000);
}

/*
 * This function create the pthreads -> workers.
 */
int init_pthread_arr(pthread_t *theards_arr, int num_threads)
{
	int i;

	for (i = 0; i < num_threads; i++) {
		if (pthread_create(&theards_arr[i], NULL, &worker_function, NULL) != 0) {
			perror("Failed to create thread");
			return 1;
		}
		printf("Thread %d has started\n", i);
	}

	return 0; 
}

/*
 * This function cancel the pthreads.
 */
int finish_pthread_exe(pthread_t *theards_arr, int num_threads)
{
	int i;

	for (i = 0; i < num_threads; i++) {
		if (pthread_cancel(theards_arr[i]) != 0) {
			return 2;
		}
		printf("Thread %d has finished execution\n", i);
	}

	return 0;
}

/*
 * Wait for all pending background commands to complete .
 */
void wait_pending_jobs(pthread_t *theards_arr, int num_threads)
{
	for (int i = 0; i < num_threads; i++)
		pthread_join(theards_arr[i], NULL);
}

/*
 * This function parse the line buf-> array of str(=="argv").
 */
int parse_line_args(char *line_args[MAX_LINE_SIZE], char *line_buf,
	size_t line_buf_size)
{
	int argc = 0;

	/* Get the first token (cmd name) */
	line_args[argc] = strtok(line_buf, ";");

	/* Walk through the other tokens (parameters) */
	while((line_args[argc] != NULL) && (argc < line_buf_size)) {
		line_args[++argc] = strtok(NULL, ";");
	}

	return argc;
}

/*
 * This function remoce an \n' character from line buf.
 */
void remove_new_line_char(char *string)
{
	string[strcspn(string, "\n")] = 0;	
}

/*
 * This function exe worker job, walking for all the commands of worker.
 */
int exe_worker_job(char *line_args[MAX_LINE_SIZE], int args_line_num, int num_threads,
	int num_counters, int log_enable, FILE **files_arr, pthread_t *theards_arr)
{
	int i;
	float sleep_time_mili_sec;
 
	/* start from 1 due to "worker" is in index 0 */
	for (i = 1; i < args_line_num; i++) {
		if (strncmp(line_args[i], "msleep", strlen("msleep")) == 0) {
			sleep_time_mili_sec = atoi(&line_args[i][7]) / 1000.0000;
			sleep(sleep_time_mili_sec);
	} else if (strncmp(line_args[i], "increment", strlen("increment")) == 0) {
			/* increment function with protection */
	} else if (strncmp(line_args[i], "decrement", strlen("decrement")) == 0) {
			/* decrement function with protection */
	}
	}
}

/*
 * This function convert char to int.
 */
int char_to_int(char c)
{
	return c - '0';
}

/*
 * This function exe dispatcher job.
 */
int exe_dispatcher_job(char *line_args[MAX_LINE_SIZE], int args_line_num, int num_threads,
	int num_counters, int log_enable, FILE **files_arr, pthread_t *theards_arr)
{
	int msleep_val;

	if (strcmp(line_args[0], "dispatcher_wait") == 0) {
		wait_pending_jobs(theards_arr, num_threads);
	} else if (strncmp(line_args[0], "dispatcher_msleep", strlen("dispatcher_msleep")) == 0) {
		msleep_val =  atoi(&line_args[0][18]);
		if (!line_args[0][18]) {
			fprintf(stdout, "args missing...\n");
		} else {
			msleep_val =  atoi(&line_args[0][18]);
			sleep(msleep_val);
		}
	}

	return 0;
}

/*
 * This function decide if job is dispatcher or worker job.
 */
int exe_job(char *line_args[MAX_LINE_SIZE], int args_line_num, int num_threads,
	int num_counters, int log_enable, FILE **files_arr, pthread_t *theards_arr,
	pthread_mutex_t *file_mutex_arr, bool *is_worker_busy)
{
	pthread_t *free_worker;

	/* If line is empty continue */
	if (!args_line_num)
		return 0;
	
	if (strcmp(line_args[0], "worker") == 0) {
		/*
		 * if its worker commands need to choose worker and protect
		 * 	file with mutex.
		 * Do we need to implement fifo?
		 * choose worker
		 */

		/* get free worker */
		/*get_free_worker(free_worker, theards_arr, num_threads);

		worker_murtex_lock(get_free_worker());
		consume();
		worker_murtex_lock(get_free_worker());*/
		/* memcpy from line args to job struct */
		exe_worker_job(line_args, args_line_num, num_threads, num_counters,
			log_enable, files_arr, theards_arr);
	} else {
		exe_dispatcher_job(line_args, args_line_num, num_threads, num_counters,
			log_enable, files_arr, theards_arr);
	}

	return 0;
}

int init_file_mutex_arr(pthread_mutex_t *file_mutex_arr, int num_counters)
{
	int i;

	for (i = 0; i < num_counters; i++) {
		if (pthread_mutex_init(&file_mutex_arr[i], NULL) != 0) {
			printf("mutex init has failed\n");
			return 1;
		}
	}

	return 0;
}

int init_worker_mutex_arr(pthread_mutex_t *worker_mutex_arr, int num_threads)
{
	int i;

	for (i = 0; i < num_threads; i++) {
		if (pthread_mutex_init(&worker_mutex_arr[i], NULL) != 0) {
			printf("mutex init has failed\n");
			return 1;
		} else
			printf("mutex init!\n");
	}

	return 0;
}

int main(int argc, char **argv)
{
   int num_threads = atoi(argv[2]), num_counters = atoi(argv[3]),
   		log_enable = atoi(argv[4]), ret, line_count, line_size,
		line_buf_size = MAX_LINE_SIZE, args_line_num;
	char *line_args[MAX_LINE_SIZE], *line_buf = NULL;
	
	/* Resorces are files with counter inside */
	FILE **files_arr = malloc(sizeof(FILE*) * (MAX_NUM_COUNTERS - 1));
	/* mutex for each file */
	pthread_mutex_t file_mutex_arr[MAX_NUM_COUNTERS];
	/* Each thread is a worker */
	pthread_t theards_arr[MAX_NUM_THREADS];
	/* Each theard is a worker ,busy -> true, free -> false */
	bool is_worker_busy[MAX_NUM_THREADS] = {0};
	
	/*
	 * mutex for each worker thread 
	 * is it really needed?
	 * /
	// pthread_mutex_t worker_mutex_arr[MAX_NUM_THREADS];  

	/* Check user args */
	ret = validate_args(num_threads, num_counters, log_enable);
	if (ret) {
		fprintf(stdout, "Invalid argv...\n");
		exit(0);
	}

	/* Init files & pthreads arrays */
	init_file_arr(files_arr, num_counters);
	init_pthread_arr(theards_arr, num_threads);
	init_file_mutex_arr(file_mutex_arr, num_counters);

	FILE *cmd_file = fopen(argv[1], "r");
	if (!cmd_file) {
		fprintf(stderr, "Error opening file '%s'\n", argv[1]);
		return EXIT_FAILURE;
	}

	/* Get the first line of the file. */
	line_size = getline(&line_buf, &line_buf, cmd_file);

	/* Loop through until we are done with the file. */
	while (line_size >= 0) {
		/* Increment our line count */
		line_count++;
	
		/* Parse line to arr of arguments(strings) */
		remove_new_line_char(line_buf);
		args_line_num = parse_line_args(line_args, line_buf, line_buf_size);

		/* choose the worker and lock ->spin lock */
		/* Each line is a worker job or dispatcher job, lets run them. */
		exe_job(line_args, args_line_num, num_threads, num_counters,
			log_enable, files_arr, theards_arr, file_mutex_arr, is_worker_busy);
		/* unlock the worker */
		/* Get the next line */
		line_size = getline(&line_buf, &line_buf_size, cmd_file);
	}
	/* add mutex destroy to both arrays */
	free(line_buf);
	fclose(cmd_file);
	finish_pthread_exe(theards_arr, num_threads);
	close_files_arr(files_arr, num_counters);
	free(files_arr);
	return 0;
}
