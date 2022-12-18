#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define USER_INPUT_ARGC 5
#define MAX_NUM_THREADS 4096
#define MAX_NUM_COUNTERS 100
#define MAX_LINE_SIZE 1024
#define MUTEX_INIT_SUCESS 0
#define DISPATCHER_MSLEEP_STRING_LEN 17
#define MAX_FILE_NAME_LEN 15  // counter99.txt0 or thread4096.txt0

typedef struct program_data {
    int num_threads;
    int num_of_sleeping_threads;
    int num_counters;
    int log_enable;
    char files_arr[MAX_NUM_COUNTERS][MAX_FILE_NAME_LEN];

    /* closed at the end of runtime */
    FILE *log_files_arr[MAX_NUM_THREADS];

    /* destroyed at the end of runtime */
    pthread_mutex_t counter_mutex_arr[MAX_NUM_COUNTERS];

    /* destroyed at the end of runtime*/
    pthread_t theards_arr[MAX_NUM_THREADS];

} Program_Data;

typedef struct job {
    long long int submission_time;
    char *line_copy;
    char *commands_to_execute[MAX_LINE_SIZE];
    int num_of_commands_to_execute;
    struct job *next_job;

} Job;

typedef struct queue {
    Job *first_job;
    Job *last_job;
    int num_of_pending_jobs;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_not_empty_cond_var;
    pthread_cond_t all_work_done;

} Queue;

typedef struct stats {
    long long int start_time;
    long long int end_time;
    long long int max_turnaround;
    long long int min_turnaround;
    long long int total_turnaround;
    long long int total_jobs;
    double average_turnaround;
} Stats;

Stats program_stats;
Program_Data *program_data;
Queue *JobQueue;

long long int getCurrentTime();
long long int readNumFromCounter(int counter_number);
void increment(int counter_number);
void decrement(int counter_number);
void remove_new_line_char(char *string);
void submitJob(Job *job);
void *workerThreadFunction();
bool initProgramData(int argc, char **argv);
void killAllThreads();
void waitPendingJobs();
void freeJob(Job *job);
Job *createJob(char *line);
void sleep_ms(int ms);
void executeWorkerJob(Job *job);
void executeDispatcherJob(Job *job);
void handleJob(Job *job);
void initCounterMutexs();
void initThreadsArr();
void initCounters();
bool isEmpty();
void Enqueue(Job *job);
Job *Dequeue();

/* FUNCTIONS */

/* sleep for <ms> milliseconds */
void sleep_ms(int ms) { usleep(ms * 1000); }

void remove_new_line_char(char *string) { string[strcspn(string, "\n")] = 0; }

/* Queue functions */
bool isEmpty() { return JobQueue->num_of_pending_jobs == 0; }

/* Insert a new job into the queue */
void Enqueue(Job *job)
{

    if (isEmpty()) {
        JobQueue->first_job = job;
        JobQueue->last_job = job;
        JobQueue->num_of_pending_jobs = 1;
    }

    else {
        JobQueue->last_job->next_job = job;
        JobQueue->last_job = job;
        JobQueue->num_of_pending_jobs++;
    }
}

/* Pop a job from the queue*/
Job *Dequeue()
{

    if (isEmpty() == true) {
        return NULL;
    }

    else {
        Job *first_job = JobQueue->first_job;

        JobQueue->first_job = JobQueue->first_job->next_job;
        JobQueue->num_of_pending_jobs--;

        return first_job;
    }
}

/* insert a job to the queue*/
void submitJob(Job *job)
{
    /* lock and submit the job to the queue */
    pthread_mutex_lock(&(JobQueue->queue_mutex));
    Enqueue(job);
    pthread_mutex_unlock(&(JobQueue->queue_mutex));

    /* signal all sleeping threads to wake up */
    pthread_cond_broadcast(&(JobQueue->queue_not_empty_cond_var));
}

/* get the number inside the counter file */
long long int readNumFromCounter(int counter_number)
{
    long long int cur_num;
    FILE *fp = fopen(program_data->files_arr[counter_number], "r");
    fscanf(fp, "%lld", &cur_num);
    fclose(fp);
    return cur_num;
}

/* increment the counter <counter_number_to_increment>*/
void increment(int counter_number)
{
    long long int counter;
    counter = readNumFromCounter(counter_number);
    counter++;

    FILE *fp = fopen(program_data->files_arr[counter_number], "w+");
    fprintf(fp, "%lld", counter);
    fclose(fp);
}

/* decrement the counter <counter_number_to_increment>*/
void decrement(int counter_number)
{
    long long int counter;
    counter = readNumFromCounter(counter_number);
    counter--;

    FILE *fp = fopen(program_data->files_arr[counter_number], "w+");
    fprintf(fp, "%lld", counter);
    fclose(fp);
}

/* Free the struct job, and all allocated memory inside it */
void freeJob(Job *job)
{

    if (job != NULL) {
        /* free deep copies of tokens*/
        for (int i = 0; i < job->num_of_commands_to_execute; i++) {
            free(job->commands_to_execute[i]);
        }
        /* free the copy of line*/
        free(job->line_copy);

        free(job);
    }
}

/* allocate memory and create the Job */
Job *createJob(char *line)
{
    Job *job = (Job *)malloc(sizeof(Job));
    job->line_copy = strdup(line);
    job->next_job = NULL;
    job->submission_time = getCurrentTime();

    remove_new_line_char(line);

    /* split line into tokens */
    int num_of_commands = 0;
    job->commands_to_execute[num_of_commands] = strtok(line, ";");

    while ((job->commands_to_execute[num_of_commands] != NULL)) {
        job->commands_to_execute[++num_of_commands] = strtok(NULL, ";");
    }
    job->num_of_commands_to_execute = num_of_commands;

    /* copy and store tokens, we perform a deep copy so different threads would not interfere */
    for (int i = 0; i < num_of_commands; i++) {
        job->commands_to_execute[i] = strdup(job->commands_to_execute[i]);
    }

    return job;
}

/*
 * Wait for all pending jobs to complete .
 */
void waitPendingJobs()
{
    pthread_mutex_lock(&(JobQueue->queue_mutex));
    pthread_cond_wait(&(JobQueue->all_work_done), &(JobQueue->queue_mutex));
    pthread_mutex_unlock(&(JobQueue->queue_mutex));
}

/* Execute the commands (job) given to a worker thread */
void executeWorkerJob(Job *job)
{
    int i = 1, ms_sleep_val, file_number;
    int repeat_value = 1;

    if (strncmp(job->commands_to_execute[1], "repeat", strlen("repeat")) == 0) {
        repeat_value = atoi(job->commands_to_execute[1] + strlen("repeat"));
        /* if the command starts with repeat - start executing from the next command*/
        i = 2;
    }

    for (int j = 0; j < repeat_value; j++) {

        for (i = 1; i < job->num_of_commands_to_execute; i++) {

            if (strncmp(job->commands_to_execute[i], "msleep", strlen("msleep")) == 0) {
                ms_sleep_val = atoi(job->commands_to_execute[i] + strlen("msleep"));
                sleep_ms(ms_sleep_val);
            }

            else if (strncmp(job->commands_to_execute[i], "increment", strlen("increment")) == 0) {

                file_number = atoi(job->commands_to_execute[i] + strlen("increment"));

                pthread_mutex_lock(&(program_data->counter_mutex_arr[file_number]));
                increment(file_number);
                pthread_mutex_unlock(&(program_data->counter_mutex_arr[file_number]));

            }

            else if (strncmp(job->commands_to_execute[i], "decrement", strlen("decrement")) == 0) {

                file_number = atoi(job->commands_to_execute[i] + strlen("decrement"));

                pthread_mutex_lock(&(program_data->counter_mutex_arr[file_number]));
                decrement(file_number);
                pthread_mutex_unlock(&(program_data->counter_mutex_arr[file_number]));
            }
        }
    }
}

/* Execute the commands (job) given to the dispatcher (main thread) */
void executeDispatcherJob(Job *job)
{

    if (strcmp(job->commands_to_execute[0], "dispatcher_wait") == 0) {
        waitPendingJobs();
    }

    else if (strncmp(job->commands_to_execute[0], "dispatcher_msleep", strlen("dispatcher_msleep")) == 0) {
        int sleep_val = atoi(job->commands_to_execute[0] + strlen("dispatcher_msleep"));
        sleep_ms(sleep_val);
    }
}

/* Refer the job either to be executed serially on the main thread, or on a worker thread */
void handleJob(Job *job)
{

    /* If line is empty continue */
    if (job->num_of_commands_to_execute != 0) {

        if (strcmp(job->commands_to_execute[0], "worker") == 0) {
            /* Submit job to worker queue*/
            submitJob(job);
        }

        else {
            /* submit job to dispatcher to execute before proceeding */
            executeDispatcherJob(job);
            freeJob(job);
        }
    }
}

/* stop all running threads*/
void killAllThreads()
{
    for (int i = 0; i < program_data->num_threads; i++) {
        if (pthread_cancel(program_data->theards_arr[i]) != 0) {
            fprintf(stderr, "Error stopping thread #%d", i);
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * This is the worker function.
 */
void *workerThreadFunction(void *arg)
{
    int id = *((int *)arg);
    free(arg);
    Job *job;

    while (true) {

        pthread_mutex_lock(&(JobQueue->queue_mutex));

        while (isEmpty()) {

            program_data->num_of_sleeping_threads++;

            /* "signal" the main thread if all theads are sleeping */
            if (program_data->num_of_sleeping_threads == program_data->num_threads) {
                pthread_cond_signal(&(JobQueue->all_work_done));
            }
            pthread_cond_wait(&(JobQueue->queue_not_empty_cond_var), &(JobQueue->queue_mutex));
            program_data->num_of_sleeping_threads--;
        }

        job = Dequeue();
        pthread_mutex_unlock(&(JobQueue->queue_mutex));

        if (program_data->log_enable) {
            fprintf(program_data->log_files_arr[id], "TIME %lld: START job %s",
                    getCurrentTime() - program_stats.start_time, job->line_copy);
        }

        executeWorkerJob(job);

        if (program_data->log_enable) {
            fprintf(program_data->log_files_arr[id], "TIME %lld: END job %s",
                    getCurrentTime() - program_stats.start_time, job->line_copy);
        }

        /* CALCULATE STATS */
        long long int turnaround_time = getCurrentTime() - job->submission_time;

        program_stats.total_jobs++;
        program_stats.total_turnaround += turnaround_time;

        if (turnaround_time > program_stats.max_turnaround) {
            program_stats.max_turnaround = turnaround_time;
        }

        if (turnaround_time < program_stats.min_turnaround || program_stats.min_turnaround == -1) {
            program_stats.min_turnaround = turnaround_time;
        }

        /* Job done */
        freeJob(job);
    }
}

void initCounterMutexs()
{
    int i;
    for (i = 0; i < program_data->num_counters; i++) {

        if (pthread_mutex_init(&(program_data->counter_mutex_arr[i]), NULL) != MUTEX_INIT_SUCESS) {
            fprintf(stderr, "pthread_mutex_init #%d has failed\n", i);
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * This function creates the worker threads.
 */
void initThreadsArr()
{
    char file_name_buffer[MAX_FILE_NAME_LEN] = {0};

    int i;
    for (i = 0; i < program_data->num_threads; i++) {
        int *arg = malloc(sizeof(*arg));
        *arg = i;
        if (pthread_create(&program_data->theards_arr[i], NULL, &workerThreadFunction, arg) != 0) {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }

        /* Create log file for thread #<i> */
        if (program_data->log_enable) {
            sprintf(file_name_buffer, "thread%.2d.txt", i);  // create name
            program_data->log_files_arr[i] = fopen(file_name_buffer, "w+");
        }
    }
}

/*
 * This function open counter files.
 */
void initCounters()
{
    char file_name_buffer[MAX_FILE_NAME_LEN] = {0};
    FILE *fp;

    for (int i = 0; i < program_data->num_counters; i++) {
        sprintf(file_name_buffer, "counter%.2d.txt", i);       // create name
        strcpy(program_data->files_arr[i], file_name_buffer);  // store the name

        /* create and write 0*/
        fp = fopen(program_data->files_arr[i], "w+");
        if (!fp) {
            fprintf(stderr, "Error creating file %s\n", file_name_buffer);
            exit(EXIT_FAILURE);
        }

        fprintf(fp, "0");  // write 0 to file
        fclose(fp);
    }
}

/* initialize the program stats struct*/
void initProgramStats()
{
    program_stats.average_turnaround = 0;
    program_stats.end_time = 0;
    program_stats.total_jobs = 0;
    program_stats.total_turnaround = 0;
    program_stats.min_turnaround = -1;
    program_stats.max_turnaround = -1;

    program_stats.start_time = getCurrentTime();
}

/* This function validates user input, and builds the struct Program_Data */
bool initProgramData(int argc, char **argv)
{
    if (argc < USER_INPUT_ARGC) {
        return false;
    }

    program_data = (Program_Data *)malloc(sizeof(Program_Data));
    program_data->num_threads = atoi(argv[2]);
    program_data->num_counters = atoi(argv[3]);
    program_data->log_enable = atoi(argv[4]);
    program_data->num_of_sleeping_threads = 0;

    /* validate input */
    if (program_data->num_threads > MAX_NUM_THREADS || program_data->num_counters > MAX_NUM_COUNTERS ||
        (program_data->log_enable != 0 && program_data->log_enable != 1)) {
        return false;
    }

    return true;
}

/* initialize the queue struct */
void initQueue()
{
    JobQueue = (Queue *)malloc(sizeof(Queue));
    if (!JobQueue) {
        fprintf(stderr, "Memory allocation failed..\n");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&(JobQueue->queue_mutex), NULL);
    JobQueue->first_job = NULL;
    JobQueue->last_job = NULL;
    JobQueue->num_of_pending_jobs = 0;
}

/* Writes statistic data to the stats_file */
void writeStats(FILE *stats_file)
{
    program_stats.average_turnaround = program_stats.total_turnaround / program_stats.total_jobs;

    fprintf(stats_file, "total running time: %lld milliseconds\n",
            (long long int)(program_stats.end_time - program_stats.start_time));
    fprintf(stats_file, "sum of jobs turnaround time: %lld milliseconds\n",
            (long long int)program_stats.total_turnaround);
    fprintf(stats_file, "min job turnaround time: %lld milliseconds\n", (long long int)program_stats.min_turnaround);
    fprintf(stats_file, "average job turnaround time: %f milliseconds\n", (double)program_stats.average_turnaround);
    fprintf(stats_file, "max job turnaround time: %lld milliseconds\n", (long long int)program_stats.max_turnaround);
}

/* get the current time in MS */
long long int getCurrentTime()
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (long long int)ts.tv_nsec * 0.000001 + ts.tv_sec * 1000;
}

int main(int argc, char **argv)
{
    bool valid_args;
    char *line_buf = NULL;
    size_t line_buffer_size = 0;
    Job *current_job;
    FILE *dispatcher_log, *stats_file, *cmd_file;

    /* INITIALIZATIONS AND VALIDATIONS */

    initProgramStats();

    valid_args = initProgramData(argc, argv);
    if (valid_args == false) {
        fprintf(stderr, "Invalid argv...\n");
        exit(EXIT_FAILURE);
    }

    if (program_data->log_enable) {
        dispatcher_log = fopen("dispatcher.txt", "w+");
        if (!dispatcher_log) {
            fprintf(stderr, "Error creating file: dispatcher.txt");
            exit(EXIT_FAILURE);
        }
    }

    stats_file = fopen("file_stats.txt", "w+");
    if (!stats_file) {
        fprintf(stderr, "Error creating file: file stats.txt");
        exit(EXIT_FAILURE);
    }

    cmd_file = fopen(argv[1], "r");
    if (!cmd_file) {
        fprintf(stderr, "Error opening file '%s'\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    initQueue();
    initCounters();
    initThreadsArr();
    initCounterMutexs();

    /* Loop through until we are done with the file. */
    while (getline(&line_buf, &line_buffer_size, cmd_file) != EOF) {
        /* write to dispatcher log file*/
        if (program_data->log_enable) {

            fprintf(dispatcher_log, ": TIME %lld: read cmd line: %s", getCurrentTime() - program_stats.start_time,
                    line_buf);
        }

        current_job = createJob(line_buf);
        handleJob(current_job);
    }

    /* WAIT FOR PENDING JOBS TO COMPLETE*/
    if (program_data->num_of_sleeping_threads != program_data->num_threads) {
        waitPendingJobs();
    }

    program_stats.end_time = getCurrentTime();

    /* WRITE STATS */
    writeStats(stats_file);
    // stop threads
    killAllThreads();

    // destroy queue mutex \ cond vars
    pthread_cond_destroy(&(JobQueue->queue_not_empty_cond_var));
    pthread_cond_destroy(&(JobQueue->all_work_done));
    pthread_mutex_destroy(&(JobQueue->queue_mutex));

    // destroy file mutex'
    for (int i = 0; i < program_data->num_counters; i++) {
        pthread_mutex_destroy(&(program_data->counter_mutex_arr[i]));
    }

    /* close files */
    fclose(cmd_file);
    if (program_data->log_enable) {
        for (int i = 0; i < program_data->num_threads; i++) {
            fclose(program_data->log_files_arr[i]);
        }
        fclose(dispatcher_log);
    }
    fclose(stats_file);

    // free structs | heap variables
    free(line_buf);
    free(JobQueue);
    free(program_data);

    return 0;
}
