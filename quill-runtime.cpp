#include <bits/stdc++.h>
#include <pthread.h>
#include "quill.h"

struct Pointers {
    int head = queue_size - 1;
    int tail = queue_size - 1;
};

const int n = 5;
const int queue_size = 10000;
int status;
pthread_t tid[n];
pthread_mutex_t lock; 
pthread_key_t key;

std::function<void()> queues[n][queue_size];

Pointers pointers_list[n];

volatile bool shutdown = false;
volatile int finish_counter = 0;

void lock_finish(){
    pthread_mutex_lock(&lock);
}

void unlock_finish(){
    pthread_mutex_unlock(&lock);
}

void destructor(void* value) {
    printf("Destroying thread-specific data\n");
    free(value);
}

std::function<void()> grab_task_from_runtime(){

    int id = *(int*)pthread_getspecific(key);

    std::function<void()> func;

    while (1){
        int randNum = rand()%(queue_size);
        if(pointers_list[randNum].tail == pointers_list[randNum].head){
            continue;
        }
        else {
            int ptr = pointers_list[randNum].tail;
            func = queues[randNum][ptr];
            pointers_list[randNum].tail--;
            break;
        }
    }

    return func;
    
}

void push_task_to_runtime(std::function<void()> new_func){

    int id = *(int*)pthread_getspecific(key);

    if(pointers_list[id].head == 0){
        // throw exception
    }
    else {
        pointers_list[id].head--;
        queues[id][pointers_list[id].head] = new_func;
    }

}

void execute_task(std::function<void()> task){
    task();
}

void find_and_execute_task() {

    std::function<void()> task = grab_task_from_runtime();

    if(task != NULL) {
        execute_task(task);
        lock_finish();
        finish_counter--;
        unlock_finish();
    }
}

void *worker_routine(void *ptr) {

    int *id = (int*)ptr;
    pthread_setspecific(key, (void*)&id);  // set thread-specific data

    while(shutdown == false) {
        find_and_execute_task();
    }
    
    return NULL;
}

void quill::init_runtime(){

    int i, ids[5] = {1, 2, 3, 4, 5};

    pthread_key_create(&key, NULL); //set_specific

    for (i = 1; i < n; i++){
        status = pthread_create(&tid[i], NULL, worker_routine, &ids[i]);
        if(status == -1){
            printf("Error in creation of thread %d\n", i);
        }
    }
    
}

//accepts a C++11 lambda function
void quill::async(std::function<void()> &&lambda){

    lock_finish();
    finish_counter++;//concurrent access
    unlock_finish();
    int id = *(int*)pthread_getspecific(key);
    int task_size = sizeof(lambda);
    // copy task on heap
    std::function<void()> *new_func = new std::function<void()> (lambda);
    // queues[id][] = *(new_func);
    //thread-safe push_task_to_runtime
    push_task_to_runtime(*(new_func));
    return;
}

void quill::start_finish(){

    finish_counter = 0; //reset

}

void quill::end_finish(){

    while(finish_counter != 0){
        find_and_execute_task();
    }

}

void quill::finalize_runtime(){

    shutdown = true;
    
    for(int i = 1; i < n; i++) {
        pthread_join(tid[i], NULL);
    }

    pthread_key_delete(key);

}