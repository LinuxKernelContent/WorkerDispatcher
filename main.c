#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
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
    char *commands_to_execute[MAX_LINE_SIZE];  // line_copy - split into tokens
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

Program_Data *program_data;
Queue *JobQueue;
clock_t start;

long long int readNumFromCounter(int counter_number);
void increment(int counter_number);
void decrement(int counter_number);
void remove_new_line_char(char *string);
void submitJob(Job *job);
void *workerThreadFunction();
bool initProgramData(int argc, char **argv);
int killAllThreads();
void waitPendingJobs();
void freeJob(Job *job);
Job *createJob(char *line);
void sleep_ms(int ms);
void executeWorkerJob(Job *job);
void executeDispatcherJob(Job *job);
void handleJob(Job *job);
bool initCounterMutexs();
int initThreadsArr();
int initCounters(void);
bool isEmpty();
void Enqueue(Job *job);
Job *Dequeue();

/* sleep for <ms> milliseconds */
void sleep_ms(int ms) { usleep(ms * 1000); }

void remove_new_line_char(char *string) { string[strcspn(string, "\n")] = 0; }

/* Queue functions */
bool isEmpty() { return JobQueue->num_of_pending_jobs == 0; }

void Enqueue(Job *job)
{

    printf("Enqueing, num of jobs pending %d \n", JobQueue->num_of_pending_jobs + 1);

    if (isEmpty()) {
        JobQueue->first_job = job;
        JobQueue->last_job = job;
        JobQueue->num_of_pending_jobs = 1;
    } else {
        JobQueue->last_job->next_job = job;
        JobQueue->last_job = job;
        JobQueue->num_of_pending_jobs++;
    }
}

Job *Dequeue()
{

    if (isEmpty() == true) {
        return NULL;
    }

    else {
        Job *first_job = JobQueue->first_job;

        printf("Dequeing, num of jobs pending: %d\n", JobQueue->num_of_pending_jobs - 1);

        JobQueue->first_job = JobQueue->first_job->next_job;
        JobQueue->num_of_pending_jobs--;

        return first_job;
    }
}

/* insert a job to the queue*/
void submitJob(Job *job)
{
    pthread_mutex_lock(&(JobQueue->queue_mutex));
    Enqueue(job);
    pthread_mutex_unlock(&(JobQueue->queue_mutex));

    pthread_cond_broadcast(&(JobQueue->queue_not_empty_cond_var));
}

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

void freeJob(Job *job)
{
    if (job != NULL) {
        for (int i = 0; i < job->num_of_commands_to_execute; i++) {
            free(job->commands_to_execute[i]);
        }
        free(job->line_copy);
        free(job);
    }
}

Job *createJob(char *line)
{
    Job *job = (Job *)malloc(sizeof(Job));
    job->line_copy = strdup(line);
    job->next_job = NULL;
    job->submission_time = gettimeofday();
    remove_new_line_char(line);
    
    int num_of_commands = 0;

    job->commands_to_execute[num_of_commands] = strtok(line, ";");

    while ((job->commands_to_execute[num_of_commands] != NULL)) {
        job->commands_to_execute[++num_of_commands] = strtok(NULL, ";");
    }
    job->num_of_commands_to_execute = num_of_commands;

    for (int i = 0; i < num_of_commands; i++) {
        job->commands_to_execute[i] = strdup(job->commands_to_execute[i]);
    }

    return job;
}

/*
 * Wait for all pending background commands to complete .
 */
void waitPendingJobs()
{
    printf("Dispatcher waiting untill all jobs are done...\n");

    pthread_mutex_lock(&(JobQueue->queue_mutex));

    pthread_cond_wait(&(JobQueue->all_work_done), &(JobQueue->queue_mutex));

    pthread_mutex_unlock(&(JobQueue->queue_mutex));
    printf("all work done, dispatcher waking up..\n");
}

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

void executeDispatcherJob(Job *job)
{

    if (strcmp(job->commands_to_execute[0], "dispatcher_wait") == 0) {
        waitPendingJobs();
    }

    else if (strncmp(job->commands_to_execute[0], "dispatcher_msleep", strlen("dispatcher_msleep")) == 0) {

        int sleep_val = atoi(job->commands_to_execute[0] + strlen("dispatcher_msleep"));

        printf("Dispatcher sleepin' for: %d\n\n", sleep_val);

        sleep_ms(sleep_val);

        printf("Dispatcher done sleeping");
    }
}

/*
 * This function decide if job is dispatcher or worker job.
 */
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

/*
 * This function cancel the pthreads.
 */
int killAllThreads()
{
    int i;

    for (i = 0; i < program_data->num_threads; i++) {
        if (pthread_cancel(program_data->theards_arr[i]) != 0) {
            return 2;
        }
    }

    return 0;
}

/*
 * This is the worker function.
 */
void *workerThreadFunction(void *arg)
{
    int id = *((int *)arg);
    free(arg);

    clock_t temp;
    Job *job;

    while (true) {

        pthread_mutex_lock(&(JobQueue->queue_mutex));

        while (isEmpty()) {
            printf("Queue empty, thread is sleeping...\n");

            program_data->num_of_sleeping_threads++;

            /* "signal" the main thread that this thread is sleeping.*/
            printf("%d threads are sleeping out of %d total threads...\n", program_data->num_of_sleeping_threads, program_data->num_threads);

            if (program_data->num_of_sleeping_threads == program_data->num_threads) {
                printf("all Jobs done\n.");
                pthread_cond_signal(&(JobQueue->all_work_done));
            }

            pthread_cond_wait(&(JobQueue->queue_not_empty_cond_var), &(JobQueue->queue_mutex));
            program_data->num_of_sleeping_threads--;

            printf("Thread woke up, queue has jobs to do!\n");
        }

        job = Dequeue();

        if (program_data->log_enable) {
            temp = clock();
            fprintf(program_data->log_files_arr[id], "TIME %lld: START job %s", (long long int)temp - start,
                    job->line_copy);
        }
        pthread_mutex_unlock(&(JobQueue->queue_mutex));

        executeWorkerJob(job);

        if (program_data->log_enable) {
            temp = clock();
            fprintf(program_data->log_files_arr[id], "TIME %lld: END job %s", (long long int)temp - start,
                    job->line_copy);
        }
        printf("Time took for job = %lld\n", (long long int)gettimeofday() - job->submission_time);
        freeJob(job);
    }
}

bool initCounterMutexs()
{
    int i;
    for (i = 0; i < program_data->num_counters; i++) {

        if (pthread_mutex_init(&(program_data->counter_mutex_arr[i]), NULL) != MUTEX_INIT_SUCESS) {
            fprintf(stderr, "pthread_mutex_init #%d has failed\n", i);
            return false;
        }
    }

    return true;
}

/*
 * This function creates the worker threads.
 */
int initThreadsArr()
{
    char file_name_buffer[MAX_FILE_NAME_LEN] = {0};

    int i;
    for (i = 0; i < program_data->num_threads; i++) {
        int *arg = malloc(sizeof(*arg));
        *arg = i;
        if (pthread_create(&program_data->theards_arr[i], NULL, &workerThreadFunction, arg) != 0) {
            perror("Failed to create thread");
            return 1;
        }

        if (program_data->log_enable) {
            sprintf(file_name_buffer, "thread%.2d.txt", i);  // create name
            program_data->log_files_arr[i] = fopen(file_name_buffer, "w+");
        }
    }

    return 0;
}

/*
 * This function open counter files.
 */
int initCounters(void)
{
    int i;
    char file_name_buffer[MAX_FILE_NAME_LEN] = {0};

    for (i = 0; i < program_data->num_counters; i++) {

        sprintf(file_name_buffer, "counter%.2d.txt", i);  // create name

        strcpy(program_data->files_arr[i], file_name_buffer);  // store the name

        FILE *fp = fopen(program_data->files_arr[i], "w+");
        fprintf(fp, "0");  // write 0 to file

        fclose(fp);
    }

    return 0;
}

/*
 * This function validates user input, and builds the struct Program_Data
 */
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

void initQueue()
{
    JobQueue = (Queue *)malloc(sizeof(Queue));
    pthread_mutex_init(&(JobQueue->queue_mutex), NULL);
    JobQueue->first_job = NULL;
    JobQueue->last_job = NULL;
    JobQueue->num_of_pending_jobs = 0;
}

int main(int argc, char **argv)
{
    start = clock();
    int valid_args, line_size;
    char *line_buf = NULL;
    size_t line_buffer_size = 0;
    Job *current_job;
    clock_t temp;
    int counter = 1;

    
    /* Check user args */
    valid_args = initProgramData(argc, argv);
    if (valid_args == false) {
        fprintf(stderr, "Invalid argv...\n");
        exit(EXIT_FAILURE);
    }

    FILE *dispatcher_log = fopen("dispatcher.txt", "w+");
    if (!dispatcher_log) {
        fprintf(stderr, "Error creating file: dispatcher.txt");
        exit(EXIT_FAILURE);
    }
    FILE *stats_file = fopen("file stats.txt", "w+");
    if (!stats_file) {
        fprintf(stderr, "Error creating file: file stats.txt");
        exit(EXIT_FAILURE);
    }

    FILE *cmd_file = fopen(argv[1], "r");
    if (!cmd_file) {
        fprintf(stderr, "Error opening file '%s'\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    initQueue();
    initCounters();
    initThreadsArr();
    initCounterMutexs();


    /* Loop through until we are done with the file. */
    while ((line_size = getline(&line_buf, &line_buffer_size, cmd_file)) != EOF) {
        if(program_data->log_enable) {
            temp = clock();
            fprintf(dispatcher_log, ": TIME %lld: read cmd line: %s", (long long int)temp - start, line_buf);
        }
        printf("Line: %d\n", counter++);

        current_job = createJob(line_buf);

        handleJob(current_job);
    }

    if (program_data->num_of_sleeping_threads != program_data->num_threads) {
        printf("Waiting for all jobs to complete before exiting...\n");
        waitPendingJobs();
    }
    printf("work done, exiting program...\n");
    
    /* create stats */
    if(program_data->log_enable) {
        temp = clock();
        fprintf(stats_file, "total running time: %lld milliseconds", (long long int)temp - start);
        // fprintf(stats_file, "sum of jobs turnaround time: %lld milliseconds",                   );
        // fprintf(stats_file, "min job turnaround time: %lld milliseconds",                       );
        // fprintf(stats_file, "average job turnaround time: %f milliseconds",                     );
        // fprintf(stats_file, "max job turnaround time: %lld milliseconds",                       );
    }                      

    
    // stop threads
    killAllThreads();

    // destroy queue mutex \ cond vars
    pthread_cond_destroy(&(JobQueue->queue_not_empty_cond_var));
    pthread_cond_destroy(&(JobQueue->all_work_done));
    pthread_mutex_destroy(&(JobQueue->queue_mutex));

    // destroy file mutex'
    for(int i = 0; i < program_data->num_counters; i++) {
        pthread_mutex_destroy(&(program_data->counter_mutex_arr[i]));
    }

    /* close files */
    fclose(cmd_file);
    for (int i = 0; i < program_data->num_threads; i++) {
        fclose(program_data->log_files_arr[i]);
    }
    fclose(stats_file);
    fclose(dispatcher_log);

    // free structs | heap variables
    free(line_buf);
    free(JobQueue);
    free(program_data);

    return 0;
}
