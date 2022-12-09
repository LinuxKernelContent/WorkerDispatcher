#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
		fprintf(files_arr[i], "%lld", 0);
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

int main(int argc, char **argv)
{
   int num_threads = atoi(argv[2]), num_counters = atoi(argv[3]),
		log_enable = atoi(argv[4]), ret;
	FILE** files_arr = malloc(sizeof(FILE*) * (MAX_NUM_COUNTERS - 1));

	ret = validate_args(num_threads, num_counters, log_enable);
	if (ret) {
		fprintf(stdout, "Invalid argv...\n");
		exit(0);
	}

	init_file_arr(files_arr, num_counters);
	
	




	close_files_arr(files_arr, num_counters);
	free(files_arr);
	return 0;
}
