#include "quill.h"
#include "quill-runtime.h"
#include <pthread.h>
#include <unistd.h>
#include <functional>
#include <iomanip>
#include <cstring>
#include <time.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

int num_workers=1;

typedef struct {
    std::function<void()> *call_from_thread;
} for_function;

#define MAX_THREADS 20

class tasks_deque
{
private:
    int id, deque_size, deque_head, deque_tail;
    pthread_mutex_t deque_lock;
    // void* deque_array[200];
    std::function<void()> deque_array[200];

public:
    tasks_deque(int id_){
        id=id_;
        deque_size=200;
        deque_head=deque_size;
        deque_tail=deque_size;
        for(int i=0; i<200; ++i){
            deque_array[i]=NULL;
        }
        pthread_mutex_init(&deque_lock, NULL);
    }

    // void push_task(void* task){
    void push_task(std::function<void()> task) {
        if( ((deque_tail+1)%deque_size) == deque_head ) {   // Deque full
            // Throw an error here
            perror("Deque Full");
            exit(1);
        }

        bool locking=false;
        if(deque_head == deque_size) {     // If the Deque is empty
            locking=true;
            pthread_mutex_lock(&deque_lock);
            deque_tail=deque_size-1;
        }

        deque_head = (deque_head-1+deque_size)%deque_size;
        deque_array[deque_head]=task;

        if(locking) pthread_mutex_unlock(&deque_lock);
    }

    bool isEmpty(){
        return deque_tail==deque_size;
    }

    // void* pop_task() {
    std::function<void()> pop_task() {
        // void* task=NULL;
        std::function<void()> task=NULL;
        pthread_mutex_lock(&deque_lock);
        if(!isEmpty()){
            task=deque_array[deque_head];
            deque_array[deque_head]=NULL;
            if(deque_head==deque_tail){     // If Deque becomes empty after pop
                deque_head=deque_size;
                deque_tail=deque_size;
            }
            else{
                deque_head = (deque_head+1)%deque_size;
            }
        }
        pthread_mutex_unlock(&deque_lock);

        return task;
    } 

    // void* steal_task() {
    std::function<void()> steal_task() {

        std::function<void()> task=NULL;
        pthread_mutex_lock(&deque_lock);
        if(!isEmpty()){
            task=deque_array[deque_tail];
            deque_array[deque_tail]=NULL;
            if(deque_head==deque_tail){     // If Deque becomes empty after pop
                deque_head=deque_size;
                deque_tail=deque_size;
            }
            else{
                deque_tail = (deque_tail-1+deque_size)%deque_size;
            }
        }
        pthread_mutex_unlock(&deque_lock);

        return task;
    }

    int getId() {
        return id;
    }
};


static pthread_key_t id_key;

volatile bool shutdown=false;
volatile int finish_counter=0;
int nums[MAX_THREADS] = {0};
pthread_mutex_t finish_lock;

pthread_t thread_ids[MAX_THREADS];

tasks_deque *deque_ptrs;

// void* pop_task_from_runtime(){
std::function<void()> pop_task_from_runtime() {
    // void* task = NULL;
    std::function<void()> task = NULL;

    // Get the ID of the thread on which the task is running
    int *thread_id_ptr = (int*)pthread_getspecific(id_key); // get the thread's ID
    int thread_id;
    if(thread_id_ptr!=NULL){
        thread_id=(*thread_id_ptr);
    }
    else{
        printf("Problem Here\n");
    }

    // Checking if pop is possible
    task = deque_ptrs[thread_id].pop_task();
    if(task!=NULL) {
        return task;
    }

    // Attempting steal
    srand(time(0));
    int size = num_workers;
    int random_id = thread_id;
    while(random_id==thread_id) {   // Loop ensure the random number is not equal to current thread's id
        random_id = rand()%size;
    }
    task = deque_ptrs[random_id].steal_task();

    return task;
}

void find_and_execute_task() {

    std::function<void()> task = pop_task_from_runtime();

    if(task!=NULL){

        int t = *((int*)pthread_getspecific(id_key));
        task();

        pthread_mutex_lock(&finish_lock);
        finish_counter--;
        pthread_mutex_unlock(&finish_lock);
    }
    
    return;
}

void *worker_routine(void *arg){

    pthread_setspecific(id_key, arg);

    while(!shutdown) {
        find_and_execute_task();
    }

    int *thread_id = (int*)pthread_getspecific(id_key); // get the thread's ID

    return NULL;
}

void quill::init_runtime() {
    // printf("Here 1\n");
    // int size = thread_pool_size();
    if (getenv("QUILL_WORKERS") != NULL) num_workers=atoi(getenv("QUILL_WORKERS"));
    else{
        printf("Defaulting to 1 thread in thread pool\n");
    }

    // size is the number of worker threads in the thread pool
    int size=num_workers;
    for(int i=0; i<size; ++i) nums[i]=i;    // This loop will run from 0 to size as I want to initialize the array

    // pthread-specific key creation:
    pthread_key_create(&id_key, NULL);

    // Set thread id for main thread:
    pthread_setspecific(id_key, (void*)&(nums[0]));

    for(int i=1; i<size; ++i){  // Creating n-1 threads
        // printf("Here 2\n");
        pthread_create(&(thread_ids[i]), NULL, worker_routine, (void*)&(nums[i]));
    }

    // Instantiate the class objects
    deque_ptrs = (tasks_deque*) malloc(size*sizeof(tasks_deque));
    for(int i=0; i<size; ++i){
        deque_ptrs[i] = tasks_deque(i);
    }

}

void quill::start_finish() {
    finish_counter=0;

    // Initializing the mutex lock for finish_counter variable
    int ret = pthread_mutex_init(&finish_lock, NULL);
    if (ret != 0) {
        perror("Error in Finish Mutex Lock Initialization");
        exit(1);
    }
}


void quill::async(std::function<void()> &&lambda) {
    
    pthread_mutex_lock(&finish_lock);
    finish_counter++;
    pthread_mutex_unlock(&finish_lock);

    
    // Get the ID of the thread on which the task is running
    int *thread_id_ptr = (int*)pthread_getspecific(id_key); // get the thread's ID
    int thread_id=-1;
    if(thread_id_ptr!=NULL){
        thread_id = (*thread_id_ptr);
    }
    else{
        printf("Problem Here Too\n");
        exit(1);
    }
    
    if(thread_id==-1){
        printf("Faulty thread id\n");
        exit(1);
    }
    // Push the task on array
    // for_function func_arg;
    // func_arg.call_from_thread = new std::function<void()>(lambda);
    // deque_ptrs[thread_id].push_task((void*) &func_arg);

    deque_ptrs[thread_id].push_task(*(new std::function<void()> (lambda)));

    return;
}

void quill::end_finish() {
    // printf("No of remaining tasks = %d\n", finish_counter);
    while(finish_counter!=0) {
        find_and_execute_task();
    }
}

void quill::finalize_runtime(){

    shutdown = true;
    int size =num_workers;

    for(int i=1; i<size; ++i){  // Creating n-1 threads
        pthread_join(thread_ids[i], NULL);
    }

    free(deque_ptrs);
    pthread_key_delete(id_key);
}
