# About
In this project we explore basic Multi-threading in C.

Once the program starts:
- multiple `Worker` threads are created, the main thread will be the `Dispatcher` which will read Jobs and store them in a Queue.
the Queue is accessible to the worker threads.
Each Worker thread reads a job from the queue and executes it.

- Multiple counters will be created in the form of files.
the workers' job is to manipulate those counters according to the `jobs list`.

- Mutex is used to protect the counters from being overwritten by multiple threads 
- conditional variables are used to wake up sleeping Worker threads.

### for example: given the following jobs list:
```
worker;repeat 1;increment 5 ;decrement 6 ;msleep 1000
worker;repeat 1;increment 5 ;decrement 6 ;msleep 1000
dispatcher_msleep 1000;
worker;increment 5;;msleep 1000
```

- the dispatcher inserts the first two jobs into the Queue, goes to sleep for 1 second and inserts the last job to the queue.

- as soon as the queue is not empty a worker will read the first job:  increment the counter 5 (file #5) and decrement the counter 6 (file #6) and go to sleep for 1 second
- another worker will read the second line and do the same
- lastly a worker will increment the counter 5 and go to sleep for 1 second.


## Summary

- ### Dispatcher inserts jobs into a queue serially.
- ### In the background, worker threads read from the queue and executes the pending jobs.


# How to run the code ?
## note:
- Maximum threads number is 4096
- Maximum counters number is 100 

run the following commands:

1. `make`

2. `worker_dispatcher <command file> <#Threads> <#Counters> <log enabled>` 

for example, if we want 5 worker threads, 10 counters, threads log files, and the commands are specified in `test.txt`:

2. `worker_dispatcher test.txt 5 10 1`

Without threads log files:

2. `worker_dispatcher test.txt 5 10 0`

to clean up after the program finishes running:

3. `make clean `
