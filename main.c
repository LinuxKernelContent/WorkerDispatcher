#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>


#define MAX_NUM_THREADS 4096
#define MAX_NUM_COUNTERS 100
#define MAX_COUNTER_FILE_NAME 10
#define MAX_LINE_SIZE 1024
#define MUTEX_INIT_SUCESS 0
#define DISPATCHER_MSLEEP_STRING_LEN 17

/*
 * This data struct holds data to exe job.
 */
typedef struct job_data {
    int num_threads;
    int num_counters;
    int log_enable;
    char files_arr[MAX_NUM_COUNTERS][MAX_COUNTER_FILE_NAME];

    pthread_mutex_t file_mutex_arr[MAX_NUM_COUNTERS];
    pthread_t theards_arr[MAX_NUM_THREADS];
} data_t;


/*
 * This job struct holds the worker commands.
 */
typedef struct job {
    char *commands_to_execute[MAX_LINE_SIZE];
    int num_of_commands_to_execute;
} job_t;


/*
 * This is the share Q for threads.
 */
data_t job_data;
job_t *job_queue[256];
int job_queue_size = 0;

pthread_mutex_t printmutex;
pthread_mutex_t mutex_queue;
pthread_cond_t cond_queue;


/*
 * This function remoce an \n' character from line buf.
 */
void remove_new_line_char(char *string) { string[strcspn(string, "\n")] = 0; }

/*
 * This function convert char to int.
 */
int char_to_int(char c) { return c - '0'; }


void execute_worker_job(job_t *job)
{
    int i = 1, ms_sleep_val, file_number;
    int repeat_value = 1;


    if(strncmp(job->commands_to_execute[1], "repeat", strlen("repeat")) == 0) {
        repeat_value = atoi(job->commands_to_execute[1] + strlen("repeat"));
        /* if the command starts with repeat - start executing from the next command*/
        i = 2; 
    }

    for(int j = 0; j < repeat_value; j++) {        
        printf("\n");
        for (i = 1; i < job->num_of_commands_to_execute; i++) {

            if (strncmp(job->commands_to_execute[i], "msleep", strlen("msleep")) == 0) {

                ms_sleep_val = atoi(job->commands_to_execute[i] + strlen("msleep"));
                printf("time to sleep = %d ", ms_sleep_val);
                usleep(ms_sleep_val * 1000);

            } 
            
            else if (strncmp(job->commands_to_execute[i], "increment", strlen("increment")) == 0) {
                
                /* increment function with protection */
                file_number = atoi( job->commands_to_execute[i] + strlen("increment") );
                printf("increment file %d ", file_number);
                pthread_mutex_lock( & (job_data.file_mutex_arr[file_number]) );
                
                /*increament*/
                long long int cur_num;

                FILE *fp = fopen(job_data.files_arr[file_number], "r");
                fscanf(fp, "%lld", &cur_num);
                fclose(fp);


                fp = fopen(job_data.files_arr[file_number], "w+");
                cur_num++;
                fprintf(fp, "%lld", cur_num);
                
                fclose(fp);

                pthread_mutex_unlock(& (job_data.file_mutex_arr[file_number]) );

            } 
            
            
            else if (strncmp(job->commands_to_execute[i], "decrement", strlen("decrement")) == 0) {
                
                /* decrement function with protection */
                file_number = atoi( job->commands_to_execute[i] + strlen("decrement") );
                printf("decrement file %d ", file_number);
                pthread_mutex_lock( & (job_data.file_mutex_arr[file_number]) );
                
                /*decrement*/
                long long int cur_num;
                FILE *fp = fopen(job_data.files_arr[file_number], "r");
                
                fscanf(fp, "%lld", &cur_num);
                fclose(fp);

                fp = fopen(job_data.files_arr[file_number], "w+");
                cur_num--;
                fprintf(fp, "%lld", cur_num);
                
                fclose(fp);

                pthread_mutex_unlock(& (job_data.file_mutex_arr[file_number]) );

            }
        }
    }
}



/* insert a job to the queue*/
void submit_job(job_t **job)
{
    pthread_mutex_lock(&mutex_queue);
    
    printf("inserting job with pointer = %p\n", *job);
    
    job_queue[job_queue_size] = job;

    job_queue_size++;

    pthread_mutex_unlock(&mutex_queue);
    pthread_cond_signal(&cond_queue);
}

/*
 * This is the worker function.
 */
void *worker_start_thread()
{
    while (true) {
        job_t *job;
        
        // get job
        pthread_mutex_lock(&mutex_queue);
        while (job_queue_size == 0) {
            pthread_cond_wait(&cond_queue, &mutex_queue);
        }

        job = job_queue[0];
        printf("worker took job with pointer = %p\n", job);
        
        int i;
        for (i = 0; i < job_queue_size - 1; i++) {
            job_queue[i] = job_queue[i+1];
        }

        job_queue_size--;




        pthread_mutex_unlock(&mutex_queue);

        // execute job
        execute_worker_job(job);
    }
}

/*
 * This function validates user input.
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
     
        strcpy(job_data.files_arr[i], file_name_buffer); // store the name

        FILE* fp = fopen(job_data.files_arr[i], "w+");
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
    for (i = 0; i < job_data.num_threads; i++) {

        if (pthread_create(&job_data.theards_arr[i], NULL, &worker_start_thread, NULL) != 0) {
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

    for (i = 0; i < job_data.num_threads; i++) {
        if (pthread_cancel(job_data.theards_arr[i]) != 0) {
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
    for (int i = 0; i < job_data.num_threads; i++)
        pthread_join(job_data.theards_arr[i], NULL);
}

/*
 * This function parse the line buf-> array of str(=="argv").
 */
job_t *createJob(char *line)
{       
    job_t *job  = (job_t*)malloc(sizeof(job_t));

    int num_of_commands= 0;
	remove_new_line_char(line);

    /* Get the first token (cmd name) */
    job->commands_to_execute[num_of_commands] = strtok(line, ";");

    /* Walk through the other tokens (parameters) */

    while ( (job->commands_to_execute[num_of_commands] != NULL) ) {
        job->commands_to_execute[++num_of_commands] = strtok(NULL, ";");
    }
    job->num_of_commands_to_execute = num_of_commands;

    return job;
}




/*
 * This function exe dispatcher job.
 */
void execute_dispatcher_job(job_t **job)
{
    int msleep_val;

    if (strcmp((*job)->commands_to_execute[0] , "dispatcher_wait") == 0) {
        /* possibly we should wait untill queue is emptyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy*/
        wait_pending_jobs(job_data);
    } 

    else if (strncmp((*job)->commands_to_execute[0], "dispatcher_msleep", strlen("dispatcher_msleep")) == 0) {
        msleep_val = atoi((*job)->commands_to_execute[0] + strlen("dispatcher_msleep") );  

        usleep(msleep_val * 1000);
        printf("dispatcher sleep done\n");
        
    }

}

/*
 * This function decide if job is dispatcher or worker job.
 */
int execute_job(job_t **job)
{

    /* If line is empty continue */
    if ((*job)->num_of_commands_to_execute == 0)
        return 0;

    if (strcmp((*job)->commands_to_execute[0], "worker") == 0) {
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


int main(int argc, char **argv)
{
	int valid_args, line_size;
    char *line_buf = NULL;
	size_t line_buffer_size = 0;
	

	/* Check user args */
    valid_args = validate_args(argc, argv);
    if (valid_args == false) {
        fprintf(stderr, "Invalid argv...\n");
        exit(0);
    }
	
	FILE *cmd_file = fopen(argv[1], "r");
    if (!cmd_file) {
        fprintf(stderr, "Error opening file '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }


    /* Init files & pthreads arrays */
    create_file_array();
    init_pthread_arr();
    init_file_mutex_arr();

    int counter = 0;
    /* Loop through until we are done with the file. */
    while ((line_size = getline(&line_buf, &line_buffer_size, cmd_file))  != EOF) {

        job_t *current_job = createJob(line_buf);
        printf("job number %d with pointer %p\n", counter, current_job);
        counter++;
        // /* print job */
        // for(int cmdidx = 0; cmdidx < current_job->num_of_commands_to_execute; cmdidx++) 
        //     printf("%s\n", current_job->commands_to_execute[cmdidx]);


        execute_job(&current_job);
        printf("\n");

    }


    finish_pthread_exe();
    pthread_cond_destroy(&cond_queue);
    pthread_mutex_destroy(&mutex_queue);
    free(line_buf);
    fclose(cmd_file);
    // close_files_arr();
    printf("\n");
    return 0;
}
