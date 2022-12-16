#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>


#define USER_INPUT_ARGC 5
#define MAX_NUM_THREADS 4096
#define MAX_NUM_COUNTERS 100
#define MAX_COUNTER_FILE_NAME 10 // remove it
#define MAX_LINE_SIZE 1024
#define MUTEX_INIT_SUCESS 0 
#define DISPATCHER_MSLEEP_STRING_LEN 17 // remove it if you can


typedef struct program_data {
    int num_threads;
    int num_of_sleeping_threads;
    int num_counters;
    int log_enable;
    char files_arr[MAX_NUM_COUNTERS][MAX_COUNTER_FILE_NAME];
    pthread_mutex_t file_mutex_arr[MAX_NUM_COUNTERS];
    pthread_t theards_arr[MAX_NUM_THREADS];

} Program_Data;


typedef struct job {
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

Program_Data *program_data;
Queue *JobQueue;


pthread_mutex_t printmutex = PTHREAD_MUTEX_INITIALIZER;
void printJob(Job *job);
void freeJob(Job *job);
void sleep_ms(int ms);


long long int readNumFromCounter(int counter_number) {
    long long int cur_num;
    FILE *fp = fopen(program_data->files_arr[counter_number], "r");
    fscanf(fp, "%lld", &cur_num);
    fclose(fp);
    return cur_num;
}

/* increment the counter <counter_number_to_increment>*/
void increment(int counter_number) {
    long long int counter;
    counter = readNumFromCounter(counter_number);
    counter++;

    FILE *fp = fopen(program_data->files_arr[counter_number], "w+");
    fprintf(fp, "%lld", counter);
    fclose(fp);

}

/* decrement the counter <counter_number_to_increment>*/
void decrement(int counter_number) {
    long long int counter;
    counter = readNumFromCounter(counter_number);
    counter--;

    FILE *fp = fopen(program_data->files_arr[counter_number], "w+");
    fprintf(fp, "%lld", counter);
    fclose(fp);

}




/* Queue functions */
bool isEmpty() {
    return JobQueue->num_of_pending_jobs == 0;
}

void Enqueue(Job *job) {
    
    printf("Enqueing: \n");
    printJob(job);

    if(isEmpty())
    {
        JobQueue->first_job = job;
        JobQueue->last_job = job;
        JobQueue->num_of_pending_jobs = 1;
    }
    else 
    {
        JobQueue->last_job->next_job = job;
        JobQueue->last_job = job;
        JobQueue->num_of_pending_jobs++;
    }


}

Job *Dequeue() {
    
    if(isEmpty() == true) {
        return NULL;
    }

    else
    {
        Job *first_job = JobQueue->first_job;
        
        printf("Dequeing: \n");
        printJob(first_job);

        JobQueue->first_job = JobQueue->first_job->next_job;
        JobQueue->num_of_pending_jobs--;

        return first_job;
    }
}



void printJob(Job *job) {
    if(job == NULL)
        printf("NULL\n");

    else {
        pthread_mutex_lock(&printmutex);
        for(int i = 0; i < job->num_of_commands_to_execute; i++){
            printf("command %d: %s\n", i, job->commands_to_execute[i]);
        }
        printf("\n");

        pthread_mutex_unlock(&printmutex);
    }

}

void remove_new_line_char(char *string) { string[strcspn(string, "\n")] = 0; }

int char_to_int(char c) { return c - '0'; }


void execute_worker_job(Job *job)
{
    int i = 1, ms_sleep_val, file_number;
    int repeat_value = 1;


    if(strncmp(job->commands_to_execute[1], "repeat", strlen("repeat")) == 0) {
        repeat_value = atoi(job->commands_to_execute[1] + strlen("repeat"));
        /* if the command starts with repeat - start executing from the next command*/
        i = 2; 
    }

    for(int j = 0; j < repeat_value; j++) {    

        for (i = 1; i < job->num_of_commands_to_execute; i++) {

            if (strncmp(job->commands_to_execute[i], "msleep", strlen("msleep")) == 0) {
                ms_sleep_val = atoi(job->commands_to_execute[i] + strlen("msleep"));
                sleep_ms(ms_sleep_val);
            } 
            
            else if (strncmp(job->commands_to_execute[i], "increment", strlen("increment")) == 0) {
                
                file_number = atoi( job->commands_to_execute[i] + strlen("increment") );
                pthread_mutex_lock( & (program_data->file_mutex_arr[file_number]) );
                
                increment(file_number);
                
                pthread_mutex_unlock(& (program_data->file_mutex_arr[file_number]) );


            } 
            
            
            else if (strncmp(job->commands_to_execute[i], "decrement", strlen("decrement")) == 0) {
                
                file_number = atoi( job->commands_to_execute[i] + strlen("decrement") );
                pthread_mutex_lock( & (program_data->file_mutex_arr[file_number]) );
                
                decrement(file_number);
                
                pthread_mutex_unlock(& (program_data->file_mutex_arr[file_number]) );

            }
        }
    }

    freeJob(job);
}



/* insert a job to the queue*/
void submit_job(Job *job)
{
    pthread_mutex_lock(&(JobQueue->queue_mutex));
    Enqueue(job);
    pthread_mutex_unlock(&(JobQueue->queue_mutex));

    pthread_cond_broadcast(&(JobQueue->queue_not_empty_cond_var));
    // pthread_cond_signal(&(JobQueue->queue_not_empty_cond_var));

}

/*
 * This is the worker function.
 */
void *worker_start_thread()
{
    while (true) {
        Job *job;
        
        pthread_mutex_lock(&(JobQueue->queue_mutex));
        
        while (isEmpty()) {
            printf("Queue empty, thread is sleeping...\n");
            program_data->num_of_sleeping_threads++;
            
            if(program_data->num_of_sleeping_threads == program_data->num_threads) {
                    pthread_cond_signal(&(JobQueue->all_work_done));
            }

            pthread_cond_wait(&(JobQueue->queue_not_empty_cond_var), &(JobQueue->queue_mutex));
            program_data->num_of_sleeping_threads--;

            printf("Thread woke up, queue has jobs to do!\n");
        }

        job = Dequeue();

        pthread_mutex_unlock(&(JobQueue->queue_mutex));
        // execute job
        execute_worker_job(job);
    }
}

/*
 * This function validates user input.
 */
bool validate_args(int argc, char **argv)
{

	if(argc < USER_INPUT_ARGC) {
		return false;
	}

	program_data->num_threads = atoi(argv[2]);
    program_data->num_counters = atoi(argv[3]);
    program_data->log_enable = atoi(argv[4]);

    if (program_data->num_threads > MAX_NUM_THREADS || program_data->num_counters > MAX_NUM_COUNTERS ||
        (program_data->log_enable != 0 && program_data->log_enable != 1) ) {
        return false;
	}

    return true;
}

/*
 * This function open counter files.
 */
int init_counters(void)
{
    int i;
    char file_name_buffer[MAX_COUNTER_FILE_NAME] = {0};

    for (i = 0; i < program_data->num_counters; i++) {

        sprintf(file_name_buffer, "counter%.2d", i); // create name
     
        strcpy(program_data->files_arr[i], file_name_buffer); // store the name

        FILE* fp = fopen(program_data->files_arr[i], "w+");
        fprintf(fp, "0"); // write 0 to file

        fclose(fp);
    }

    return 0;
}


/*
 * This function creates the worker threads.
 */
int init_pthread_arr()
{
    int i;
    for (i = 0; i < program_data->num_threads; i++) {

        if (pthread_create(&program_data->theards_arr[i], NULL, &worker_start_thread, NULL) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    return 0;
}

/*
 * This function cancel the pthreads.
 */
int finish_pthread_exe()
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
 * Wait for all pending background commands to complete .
 */
void wait_pending_jobs()
{
    sleep(10);
}


void freeJob(Job *job) {
    if(job != NULL) {
        for(int i = 0; i < job->num_of_commands_to_execute; i++) {
            free(job->commands_to_execute[i]);
        }
        
        free(job);
    }
}



Job *createJob(char *line)
{       
    Job *job  = (Job*)malloc(sizeof(Job));
	
    remove_new_line_char(line);
    job->next_job = NULL;
    int num_of_commands = 0;
    

    job->commands_to_execute[num_of_commands] = strtok(line, ";");

    while ( (job->commands_to_execute[num_of_commands] != NULL) ) {
       job->commands_to_execute[++num_of_commands] = strtok(NULL, ";");
    }
    job->num_of_commands_to_execute = num_of_commands;


    for(int i = 0; i < num_of_commands; i++) {
        job->commands_to_execute[i] = strdup(job->commands_to_execute[i]);
    }

    return job;
}


/* sleep for <ms> milliseconds */
void sleep_ms(int ms) {
    usleep(ms * 1000);
    
}



void execute_dispatcher_job(Job *job)
{
    int sleep_val;

    if (strcmp(job->commands_to_execute[0] , "dispatcher_wait") == 0) {
        printf("Dispatcher waiting untill all jobs are done...\n");
        
        pthread_mutex_lock(&(JobQueue->queue_mutex));

        pthread_cond_wait(&(JobQueue->all_work_done), &(JobQueue->queue_mutex));
        
        pthread_mutex_unlock(&(JobQueue->queue_mutex));
        printf("all work done, dispatcher waking up..\n");
    } 

    else if (strncmp(job->commands_to_execute[0], "dispatcher_msleep", strlen("dispatcher_msleep")) == 0) {
        sleep_val = atoi(job->commands_to_execute[0] + strlen("dispatcher_msleep") );  

        printf("Dispatcher sleepin' for: %d\n\n", sleep_val );

        sleep_ms(sleep_val);

        printf("Dispatcher done sleeping");
        
    }

    freeJob(job);
}

/*
 * This function decide if job is dispatcher or worker job.
 */
int execute_job(Job *job)
{

    /* If line is empty continue */
    if (job->num_of_commands_to_execute == 0)
        return 0;

    if (strcmp(job->commands_to_execute[0], "worker") == 0) {
        /* Submit job to worker queue*/
        submit_job(job);
    } 
    
    else {
        /* submit job to dispatcher */
        execute_dispatcher_job(job);
    }

    return 0;
}


bool init_file_mutex_arr()
{
    int i;
    for (i = 0; i < program_data->num_counters; i++) {

        if (pthread_mutex_init(&(program_data->file_mutex_arr[i]), NULL) != MUTEX_INIT_SUCESS) {

            fprintf(stderr, "pthread_mutex_init #%d has failed\n", i);
            return false;
        }
    }

    return true;
}


int main(int argc, char **argv)
{
    clock_t start=clock();

    program_data = (Program_Data*)malloc(sizeof(Program_Data));
    program_data->num_of_sleeping_threads = program_data->num_threads;

    JobQueue = (Queue*)malloc(sizeof(Queue));
    pthread_mutex_init(&(JobQueue->queue_mutex), NULL);

    JobQueue->first_job = NULL;
    JobQueue->last_job = NULL;
    JobQueue->num_of_pending_jobs = 0;


	int valid_args, line_size;
    char *line_buf = NULL;
	size_t line_buffer_size = 0;
	

	/* Check user args */
    valid_args = validate_args(argc, argv);
    if (valid_args == false) {
        fprintf(stderr, "Invalid argv...\n");
        exit(EXIT_FAILURE);
    }
	
	FILE *cmd_file = fopen(argv[1], "r");
    if (!cmd_file) {
        fprintf(stderr, "Error opening file '%s'\n", argv[1]);
        exit(EXIT_FAILURE);
    }


    /* Init files & pthreads arrays */
    init_counters();
    init_pthread_arr();
    init_file_mutex_arr();

    Job *current_job;
    int counter = 1;

    /* Loop through until we are done with the file. */
    while ((line_size = getline(&line_buf, &line_buffer_size, cmd_file))  != EOF) {
        printf("Line: %d\n", counter++);

        current_job = createJob(line_buf);
        
        execute_job(current_job);
    }



    finish_pthread_exe();
    pthread_cond_destroy(&(JobQueue->queue_not_empty_cond_var));
    pthread_mutex_destroy(&(JobQueue->queue_mutex));
    free(line_buf);
    fclose(cmd_file);
    free(JobQueue);
    free(program_data);
    clock_t end =  clock();
    printf("Program taken: %lf To complete.\n", (double)(end-start) / CLOCKS_PER_SEC) ;
    return 0;
}
