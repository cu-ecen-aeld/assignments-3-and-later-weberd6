#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param) {

    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    usleep(1000*thread_func_args->wait_to_obtain_ms);

    if (0 != pthread_mutex_lock(thread_func_args->mutex)) {
        thread_func_args->thread_complete_success = false;
        pthread_exit(thread_param);
    }

    usleep(1000*thread_func_args->wait_to_release_ms);

    if (0 != pthread_mutex_unlock(thread_func_args->mutex)) {
        thread_func_args->thread_complete_success = false;
        pthread_exit(thread_param);
    }

    thread_func_args->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms) {

    /**
     * allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    struct thread_data *td = (struct thread_data *) malloc(sizeof(struct thread_data));
    td->wait_to_obtain_ms = wait_to_obtain_ms;
    td->wait_to_release_ms = wait_to_release_ms;
    td->mutex = mutex;

    return (0 == pthread_create(thread, NULL, threadfunc, (void*)td));
}

