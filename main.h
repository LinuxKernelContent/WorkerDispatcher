#ifndef MAIN_H
#define MAIN_H

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

typedef struct stats {
    long long int start_time;
    long long int end_time;
    long long int max_turnaround;
    long long int min_turnaround;
    long long int total_turnaround;
    long long int total_jobs;
    double average_turnaround;
} Stats;


long long int getCurrentTime();
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
void initCounterMutexs();
void initThreadsArr();
void initCounters();
bool isEmpty();
void Enqueue(Job *job);
Job *Dequeue();



#endif