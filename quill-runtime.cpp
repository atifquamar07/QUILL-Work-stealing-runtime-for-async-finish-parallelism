#include <bits/stdc++.h>
#include <pthread.h>
#include "quill.h"
#include <cstring>
#include <functional>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// queue class
class CircularDeque{

    public:
        std::function<void()> *arr;
        int head;
        int tail;
        int capacity;
        int size;

    public:

        CircularDeque(){
            capacity = 1000;
            arr = new std::function<void()>[capacity];
            head = capacity;
            tail = capacity;
            size = 0;
        }

        bool isEmpty(){
            return size == 0;
        }

        bool isFull(){
            return size == capacity;
        }

        void insertHead(std::function<void()> func){

            if (isFull()){
                std::cout << "Deque is full\n";
                // exit(EXIT_FAILURE);
                return;
            }
            // If queue is initially empty
            if (head == capacity) {
                head = capacity - 1;
                tail = capacity - 1;
                arr[head] = func;
                size++;
                return;
            }
            // head is at first position of queue
            else if (head == 0){
                head = capacity - 1;
            }
            // decrement head end by '1'
            else {
                head = head - 1;
            }
            
            // insert current element into Deque
            arr[head] = func;
            size++;
        }

        std::function<void()> popHead(){

            if (isEmpty()){
                // std::cout << "Deque is empty\n";
                return NULL;
            }
            std::function<void()> func = arr[head];
            arr[head] == NULL;

            if(head == tail){
                head = capacity-1;
                tail = capacity-1;
            }
            else {
                // head was at last of queue
                if(head == 0){
                    head = capacity - 1;
                }
                // head was somewhere else
                else {
                    head++;
                }
            }

            size--;
            return func;
        }

        std::function<void()> popBack(){

            if (isEmpty()){
                // std::cout << "Deque is empty\n";
                return NULL;
            }
            std::function<void()> func = arr[tail];
            arr[tail] == NULL;

            if(head == tail){
                head = capacity-1;
                tail = capacity-1;
            }
            else {
                // Tail was at the beginning
                if (tail == 0) {
                    tail = capacity - 1;
                }
                else {
                    tail--;
                }
            }
            
            size--;
            return func;
        }

};

int status;
int num = num_workers; 
pthread_t tid[10];
pthread_mutex_t lock; 
pthread_mutex_t queueLock; 
static pthread_key_t key;

CircularDeque *queues = (CircularDeque *)malloc(num*sizeof(CircularDeque));

volatile bool shutdown = false;
volatile int finish_counter = 0;

void lock_finish(){
    pthread_mutex_lock(&lock);
}

void unlock_finish(){
    pthread_mutex_unlock(&lock);
}

void lock_queue(){
    pthread_mutex_lock(&queueLock);
}

void unlock_queue(){
    pthread_mutex_unlock(&queueLock);
}

std::function<void()> grab_task_from_runtime(){

    int *pid = (int*)pthread_getspecific(key);
    int id;
    if(pid != NULL){
        id = *pid;
    }
    else {
        return NULL;
    }

    std::function<void()> func = NULL;

    // popping from head is lockless operation
    if(!queues[id].isEmpty()){

        func = queues[id].popHead();
        if(func != NULL){
            pthread_mutex_unlock(&queueLock);
            return func;
        }
        else {
            printf("popHead() is trying to return NULL function in grab_task_from_runtime()\n");

        }
    }

    // try to steal
    srand(time(0));
    while (1){
        
        int randNum = (rand()%(num));

        lock_queue();

        if(queues[randNum].isEmpty() || randNum == id){
            unlock_queue();
            continue;
        }
        if(!queues[randNum].isEmpty()){
            func = queues[randNum].popBack();
            if(func != NULL){
                unlock_queue();
                break;
            }
            else {
                printf("popBack() is trying to return NULL function in grab_task_from_runtime()\n");
            } 
        }
        else {
            unlock_queue();
            continue;
        } 

        unlock_queue();
    }
    unlock_queue();

    return func;
    
}

void push_task_to_runtime(std::function<void()> new_func, int id){

    if(queues[id].isFull()){
        printf("Queue is full, cannot push_task_to_runtime\n");
        exit(EXIT_FAILURE);
    }
    else {
        queues[id].insertHead(new_func);
    }

}

void execute_task(std::function<void()> task){

    if(task != NULL){
        task();
    }

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
    int ret = pthread_setspecific(key, ptr);  
    if(ret != 0){
        perror("worker_routine, set problem");
        exit(EXIT_FAILURE);
    }
    while(shutdown == false) {
        find_and_execute_task();
    }
    
    return NULL;
}

// initialize the runtime
void quill::init_runtime(){

    if(getenv("QUILL_WORKERS")!=NULL){
        num_workers = atoi(getenv("QUILL_WORKERS"));
    }

    int i;
    int *ids;
    ids=(int*)malloc(num*sizeof(int));
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&queueLock, NULL);


    for (int j = 0; j < num; j++){
        ids[j] = j;
    }

    for (int j = 0; j < num; j++){
        queues[j] = CircularDeque();
    }

    int ret = pthread_key_create(&key, NULL);
    if(ret != 0){
        perror("init_runtime, create  problem");
        exit(EXIT_FAILURE);
    }
    ret = pthread_setspecific(key, (void *)&(ids[0]));
    if(ret != 0){
        perror("worker_routine, set problem");
        exit(EXIT_FAILURE);
    }

    for (i = 1; i < num; i++){
        status = pthread_create(&(tid[i]), NULL, worker_routine, (void *)(&(ids[i])));
        if(status == -1){
            printf("Error in creation of thread %d\n", i);
        }
    }
    
}

//accepts a C++11 lambda function
void quill::async(std::function<void()> &&lambda){

    lock_finish();
    finish_counter++;  //concurrent access
    unlock_finish();

    int *pid = ((int*)(pthread_getspecific(key)));
    if(pid == NULL){
        printf("Error in push task\n");
        exit(EXIT_FAILURE);
    }
    int id = *pid;

    //thread-safe push_task_to_runtime
    push_task_to_runtime(lambda, id);
    return;
}

void quill::start_finish(){

    lock_finish();
    finish_counter = 0; //reset
    unlock_finish();

}

void quill::end_finish(){

    while(finish_counter != 0){
        find_and_execute_task();
    }

}

void quill::finalize_runtime(){

    shutdown = true;
    
    for(int i = 1; i < num; i++) {
        pthread_join(tid[i], NULL);
    }

    pthread_key_delete(key);
    exit(EXIT_SUCCESS);
    return;

}

