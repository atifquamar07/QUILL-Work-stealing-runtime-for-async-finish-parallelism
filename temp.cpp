#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define MAX_THREADS 256
#define CACHE_LINE_SIZE 64

/* Data structures */
typedef struct thread_info
{
    pthread_t thread_id;
    int thread_num;
    int num_threads;
    pthread_mutex_t steal_mutex;
    int top;
    int bottom;
    int max_size;
    void **queue;
} thread_info_t;

typedef struct runtime_info
{
    int num_threads;
    thread_info_t *thread_info_array;
} runtime_info_t;

/* Function prototypes */
void *worker_thread(void *arg);
int get_thread_num();
void push_task(void *task);
void *pop_task();
void *steal_task();
void init_runtime(int num_threads);
void start_finish();
void async(void (*func)(void *));
void end_finish();
void finalize_runtime();

/* Global variables */
static pthread_key_t thread_num_key;
static pthread_mutex_t finish_mutex;
static pthread_cond_t finish_cond;
static int num_active_finishes = 0;
static int num_threads;
static runtime_info_t runtime_info;

/* Function definitions */

/* Worker thread function */
void *worker_thread(void *arg)
{
    thread_info_t *thread_info = (thread_info_t *)arg;
    pthread_setspecific(thread_num_key, &thread_info->thread_num);

    // Run the thread loop
    while (true)
    {
        void *task = pop_task();
        if (task != NULL)
        {
            // Execute the task
            void (*func)(void *) = (void (*)(void *))task;
            func(NULL);
        }
        else
        {
            // Try to steal a task from another thread
            task = steal_task();
            if (task != NULL)
            {
                // Execute the stolen task
                void (*func)(void *) = (void (*)(void *))task;
                func(NULL);
            }
            else
            {
                // Wait for work
                pthread_mutex_lock(&thread_info->steal_mutex);
                while (thread_info->top <= thread_info->bottom)
                {
                    pthread_cond_wait(&finish_cond, &thread_info->steal_mutex);
                }
                pthread_mutex_unlock(&thread_info->steal_mutex);
            }
        }
    }

    return NULL;
}

/* Get the current thread number */
int get_thread_num()
{
    int *thread_num_ptr = (int *)pthread_getspecific(thread_num_key);
    return *thread_num_ptr;
}

/* Push a task onto the current thread's queue */
void push_task(void *task)
{
    int thread_num = get_thread_num();
    thread_info_t *thread_info = &runtime_info.thread_info_array[thread_num];
    int bottom = thread_info->bottom;
    thread_info->queue[bottom] = task;
    thread_info->bottom = (bottom + 1) % thread_info->max_size;
}

/* Pop a task from the current thread's queue */
void *pop_task()
{
    int thread_num = get_thread_num();
    thread_info_t *thread_info = &runtime_info.thread_info_array[thread_num];
    int top = thread_info->top;
    int bottom = thread_info->bottom;
    if (top == bottom)
    {
        return NULL;
    }
    void *task = thread_info->queue[top];
    thread_info->top = (top + 1) % thread_info->max_size;
    return task;
}

/* Steal a task from another thread's queue */
void *steal_task()
{
    int thread_num = get_thread_num();
    int victim_num = rand() % runtime_info.num_threads;
    if (victim_num == thread_num)
    {
        return NULL;
    }

    thread_info_t *victim_thread_info = &runtime_info.thread_info_array[victim_num];
    pthread_mutex_lock(&victim_thread_info->steal_mutex);
    int top = victim_thread_info->top;
    int bottom = victim_thread_info->bottom;
    int victim_queue_size = bottom - top;
    if (victim_queue_size <= 0)
    {
        pthread_mutex_unlock(&victim_thread_info->steal_mutex);
        return NULL;
    }
    int steal_index = top + rand() % victim_queue_size;
    void *task = victim_thread_info->queue[steal_index];
    victim_thread_info->top = steal_index + 1;
    pthread_mutex_unlock(&victim_thread_info->steal_mutex);
    return task;
}

/* Initialize the work-stealing runtime with the given number of threads */
void init_runtime(int num_threads)
{
    // Initialize the thread-specific data
    pthread_key_create(&thread_num_key, NULL);

    // Initialize the finish mutex and condition variable
    pthread_mutex_init(&finish_mutex, NULL);
    pthread_cond_init(&finish_cond, NULL);

    // Initialize the runtime info
    runtime_info.num_threads = num_threads;
    runtime_info.thread_info_array = malloc(num_threads * sizeof(thread_info_t));
    assert(runtime_info.thread_info_array != NULL);

    // Initialize each thread's data
    for (int i = 0; i < num_threads; i++)
    {
        thread_info_t *thread_info = &runtime_info.thread_info_array[i];
        thread_info->thread_num = i;
        thread_info->num_threads = num_threads;
        pthread_mutex_init(&thread_info->steal_mutex, NULL);
        thread_info->max_size = 1024;
        thread_info->queue = malloc(thread_info->max_size * sizeof(void *));
        assert(thread_info->queue != NULL);
        thread_info->top = 0;
        thread_info->bottom = 0;
        // Spawn the worker thread
        pthread_create(&thread_info->thread_id, NULL, worker_thread, thread_info);
    }
}
/* Start a "flat-finish" scope */
void start_finish()
{
    pthread_mutex_lock(&finish_mutex);
    num_active_finishes++;
}

/* Spawn an async task */
void async(void (*func)(void *))
{
    push_task((void *)func);
}

/* End a "flat-finish" scope */
void end_finish()
{
    num_active_finishes--;
    if (num_active_finishes == 0)
    {
        pthread_cond_broadcast(&finish_cond);
    }
    pthread_mutex_unlock(&finish_mutex);
}

/* Finalize and release all resources used by the runtime */
void finalize_runtime()
{
    // Wait for all threads to finish
    for (int i = 0; i < runtime_info.num_threads; i++)
    {
        pthread_join(runtime_info.thread_info_array[i].thread_id, NULL);
    }

    // Destroy the thread-specific data
    pthread_key_delete(thread_num_key);

    // Destroy the finish mutex and condition variable
    pthread_mutex_destroy(&finish_mutex);
    pthread_cond_destroy(&finish_cond);

    // Free the runtime info
    for (int i = 0; i < runtime_info.num_threads; i++)
    {
        free(runtime_info.thread_info_array[i].queue);
    }
    free(runtime_info.thread_info_array);
}
