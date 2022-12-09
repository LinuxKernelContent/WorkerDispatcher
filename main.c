#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_NUM_THREADS 4096
#define MAX_NUM_COUNTERS 100
#define MAX_COUNTER_NAME  10

int validate_args(int num_threads, int num_counters, int log_enable)
{
	if (num_threads > MAX_NUM_THREADS || num_counters > MAX_NUM_COUNTERS ||
		(log_enable != 0 && log_enable != 1))
		return 1;

	return 0;
}

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

int close_files_arr(FILE **files_arr, int num_counters)
{
	int i;

	for (i = 0; i < num_counters; i++) {
		fclose(files_arr[i]);
	}

	return 0;
}

void *worker_function(void *vargp)
{
	sleep(1000);
}

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

int main(int argc, char **argv)
{
   int num_threads = atoi(argv[2]), num_counters = atoi(argv[3]),
   		log_enable = atoi(argv[4]), ret;
	FILE **files_arr = malloc(sizeof(FILE*) * (MAX_NUM_COUNTERS - 1));
	pthread_t theards_arr[MAX_NUM_THREADS];

	/* Check user args */
	ret = validate_args(num_threads, num_counters, log_enable);
	if (ret) {
		fprintf(stdout, "Invalid argv...\n");
		exit(0);
	}

	/* Init files & pthreads arrays */
	init_file_arr(files_arr, num_counters);
	init_pthread_arr(theards_arr, num_threads);

	/* Parse cmd file line by line 
	 * implement sidpatcher commands
	 * implement parsing job string by strtok(";")
	 * for worker run allover the commands
	 * else(dispatcher) run on lines
	 * if its worker commands need to choose worker and protect
	 * 	file with mutex.
	 * Do we need to implement fifo?
	 */

	finish_pthread_exe(theards_arr, num_threads);
	close_files_arr(files_arr, num_counters);
	free(files_arr);
	return 0;
}
