#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_NUM_THREADS 4096
#define MAX_NUM_COUNTERS 100
#define MAX_COUNTER_FILE_NAME 10
#define MAX_LINE_SIZE 1024
#define MUTEX_INIT_SUCESS 0

/*
 * This data struct holds data to exe job.
 */
typedef struct job_data {
    int num_threads;
    int num_counters;
    int log_enable;
    FILE *files_arr[MAX_NUM_COUNTERS];
    pthread_mutex_t file_mutex_arr[MAX_NUM_COUNTERS];
    pthread_t theards_arr[MAX_NUM_THREADS];
} data_t;


/*
 * This job struct holds the worker commands.
 */
typedef struct job {
    char *worker_commands[MAX_LINE_SIZE];
    int num_of_commands_in_line;
} job_t;

/*
 * This is the share Q for threads.
 */
data_t job_data;
job_t job_queue[256];
int job_count = 0;

pthread_mutex_t mutex_queue;
pthread_cond_t cond_queue;


void execute_worker_job(job_t *job)
{
    int i, sleep_time_mili_sec, file_number;

    for (i = 0; i < job->num_of_commands_in_line; i++) {
        if (strncmp(job->worker_commands[i], "msleep", strlen("msleep")) == 0) {
            sleep_time_mili_sec = atoi(&job->worker_commands[i][7]) / 1000.0000;
            sleep(sleep_time_mili_sec);

        } else if (strncmp(job->worker_commands[i], "increment", strlen("increment")) == 0) {
            /* increment function with protection */
            file_number = atoi(&(job->worker_commands[i][10]));
            pthread_mutex_lock(&job_data.file_mutex_arr[file_number]);
            /*increament*/
            pthread_mutex_unlock(&job_data.file_mutex_arr[file_number]);

        } else if (strncmp(job->worker_commands[i], "decrement", strlen("decrement")) == 0) {
            /* decrement function with protection */
            file_number = atoi(&(job->worker_commands[i][10]));
            pthread_mutex_lock(&job_data.file_mutex_arr[file_number]);
            /*decreament*/
            pthread_mutex_unlock(&job_data.file_mutex_arr[file_number]);
        }
    }
}

void submit_job(job_t *job)
{
    pthread_mutex_lock(&mutex_queue);
    job_queue[job_count] = *job;
    job_count++;
    pthread_mutex_unlock(&mutex_queue);
    pthread_cond_signal(&cond_queue);
}

/*
 * This is the worker function.
 */
void *worker_start_thread(void *args)
{
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
        execute_worker_job(&job);
    }
}

/*
 * This function check that args fron user in they limit.
 */
bool validate_args(int argc, char **argv)
{

	if(argc < 5) {
		return false;
	}

	job_data.num_threads = atoi(argv[2]);
    job_data.num_counters = atoi(argv[3]);

    job_data.log_enable = atoi(argv[4]);

    if (job_data.num_threads > MAX_NUM_THREADS || job_data.num_counters > MAX_NUM_COUNTERS ||
        (job_data.log_enable != 0 && job_data.log_enable != 1) ) {
        return false;
	}

    return true;
}

/*
 * This function open counter files.
 */
int create_file_array(void)
{
    int i;
    char file_name_buffer[MAX_COUNTER_FILE_NAME] = {0};

    for (i = 0; i < job_data.num_counters; i++) {
        sprintf(file_name_buffer, "counter%.2d", i); // create name
        job_data.files_arr[i] = fopen("counter%.2d", "w+"); // create file and store into files_arr
        fprintf(job_data.files_arr[i], "%d", 0); // write 0 to file
    }

    return 0;
}

/*
 * This function close counter files.
 */
int close_files_arr()
{
    int i;

    for (i = 0; i < job_data.num_counters; i++)
        fclose(job_data.files_arr[i]);

    return 0;
}

/*
 * This function create the pthreads -> workers.
 */
int init_pthread_arr()
{
    int i;
	printf("numthreads = %d\n", job_data.num_threads);
    for (i = 0; i < job_data.num_threads; i++) {

        if (pthread_create(&job_data.theards_arr[i], NULL, &worker_start_thread, NULL) != 0) {
            perror("Failed to create thread");
            return 1;
        }
        printf("Thread %d has started\n", i);
    }
	printf("done");

    return 0;
}

/*
 * This function cancel the pthreads.
 */
int finish_pthread_exe()
{
    int i;

    for (i = 0; i < job_data.num_threads; i++) {
        if (pthread_cancel(job_data.theards_arr[i]) != 0) {
            return 2;
        }
        printf("Thread %d has finished execution\n", i);
    }

    return 0;
}

/*
 * Wait for all pending background commands to complete .
 */
void wait_pending_jobs(data_t job_data)
{
    for (int i = 0; i < job_data.num_threads; i++)
        pthread_join(job_data.theards_arr[i], NULL);
}

/*
 * This function parse the line buf-> array of str(=="argv").
 */
int parse_line_args(char *line_args[MAX_LINE_SIZE], char *line, size_t line_size)
{
    int i = 0;
	remove_new_line_char(line);

    /* Get the first token (cmd name) */
    line_args[i] = strtok(line, ";");

    /* Walk through the other tokens (parameters) */
    while ( (i < MAX_LINE_SIZE) && (line_args[i] != NULL) ) {
        line_args[++i] = strtok(NULL, ";");
    }

    return i;
}

/*
 * This function remoce an \n' character from line buf.
 */
void remove_new_line_char(char *string) { string[strcspn(string, "\n")] = 0; }

/*
 * This function convert char to int.
 */
int char_to_int(char c) { return c - '0'; }

/*
 * This function exe dispatcher job.
 */
int exe_dispatcher_job(char *line_args[MAX_LINE_SIZE], int args_line_num)
{
    int msleep_val;

    if (strcmp(line_args[0], "dispatcher_wait") == 0) {
        wait_pending_jobs(job_data);
    } else if (strncmp(line_args[0], "dispatcher_msleep", strlen("dispatcher_msleep")) == 0) {
        msleep_val = atoi(&line_args[0][18]);
        if (!line_args[0][18]) {
            fprintf(stdout, "args missing...\n");
        } else {
            msleep_val = atoi(&line_args[0][18]);
            sleep(msleep_val);
        }
    }

    return 0;
}

/*
 * This function decide if job is dispatcher or worker job.
 */
int exe_job(job_t *current_job, char *line_args[MAX_LINE_SIZE], int args_line_num)
{
    pthread_t *free_worker;

    /* If line is empty continue */
    if (!args_line_num)
        return 0;

    if (strcmp(line_args[0], "worker") == 0) {
        /* create job */
        create_job_for_worker(current_job, line_args, args_line_num);
        /* Submit job*/
        submit_job(current_job);
    } else {
        exe_dispatcher_job(line_args, args_line_num);
    }

    return 0;
}


bool init_file_mutex_arr()
{
    int i;
    for (i = 0; i < job_data.num_counters; i++) {
        if (pthread_mutex_init(&(job_data.file_mutex_arr[i]), NULL) != MUTEX_INIT_SUCESS) {
            printf("mutex init has failed\n");
            return false;
        }
    }

    return true;
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

int create_job_for_worker(job_t *job, char *line_args[MAX_LINE_SIZE], int args_line_num)
{
    int i;

    job->num_of_commands_in_line = args_line_num;
    for (i = 0; i < job->num_of_commands_in_line; i++)
        strcpy(job->worker_commands[i], line_args[i]);

    return 0;
}

int main(int argc, char **argv)
{
	printf("Starting..\n");
	int valid_args,line_size, number_of_commands, line_buf_size = MAX_LINE_SIZE;
    char *line_args[MAX_LINE_SIZE], *line_buf = NULL;
	size_t line_buffer_size = 0;
	job_t current_job;

	/* Check user args */
	printf("Validating args...\n");
    valid_args = validate_args(argc, argv);
    if (valid_args == false) {
        fprintf(stderr, "Invalid argv...\n");
        exit(0);
    }
	
	printf("opening commands file...\n");
	FILE *cmd_file = fopen(argv[1], "r");
    if (!cmd_file) {
        fprintf(stderr, "Error opening file '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }


    /* Init files & pthreads arrays */
	printf("creating array of files\n");
    create_file_array();

	printf("creating array of thread identifiers and starting threads..\n");
    init_pthread_arr();

	printf("Creating mutex for each file\n");
    init_file_mutex_arr();


	printf("Parsing text file\n");
    
	/* Get the first line of the file. */

    /* Loop through until we are done with the file. */
    while ((line_size = getline(&line_buf, &line_buffer_size, cmd_file))  != EOF) {

        /* Parse line to arr of arguments(strings) */
        number_of_commands = parse_line_args(line_args, line_buf, line_buf_size);
		printf("new command:\n");
        for(int x = 0; x < number_of_commands; x++) {
            printf("argument number %d = %s\n",x, line_args[x] );
        }
        printf("executing job...\n\n");
        // exe_job(&current_job, line_args, number_of_commands);

        /* Get the next line */
    }

    
    printf("ASASA");
    /* add mutex destroy to both arrays */
    pthread_mutex_destroy(&mutex_queue);
    pthread_cond_destroy(&cond_queue);
    /* destroy all muteses and cond*/
    free(line_buf);
    fclose(cmd_file);
    finish_pthread_exe();
    close_files_arr();
    return 0;
}
