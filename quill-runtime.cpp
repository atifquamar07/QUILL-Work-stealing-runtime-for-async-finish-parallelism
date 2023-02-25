#include <bits/stdc++.h>
#include <pthread.h>
#include "quill.h"
#include <cstring>
#include <functional>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
// #include "CircularDeque.cpp"

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
int num = num_workers; // change to 1
pthread_t tid[10];
pthread_mutex_t lock; 
pthread_mutex_t queueLock; 
static pthread_key_t key;

CircularDeque *queues = (CircularDeque *)malloc(num*sizeof(CircularDeque));
// CircularDeque queues[num];

volatile bool shutdown = false;
volatile int finish_counter = 0;

void lock_finish(){
    pthread_mutex_lock(&lock);
}

void unlock_finish(){
    pthread_mutex_unlock(&lock);
}


std::function<void()> grab_task_from_runtime(){

    printf("grab_task_from_runtime\n");
    int *pid = (int*)pthread_getspecific(key);
    int id;
    if(pid != NULL){
        id = *pid;
    }
    else {
        return NULL;
    }
    printf("%d", id);
    // else {
    //     printf("pid was null in grab_task\n");
    //     exit(EXIT_FAILURE);
    // }
    // printf("Id in grab_task_from_runtime %d\n", id);
    std::function<void()> func = NULL;

    pthread_mutex_lock(&queueLock);
    if(!queues[id].isEmpty()){
        // pthread_mutex_lock(&queueLock);
        func = queues[id].popHead();
        // pthread_mutex_unlock(&queueLock);
        if(func != NULL){
            pthread_mutex_unlock(&queueLock);
            return func;
        }
        else {
            printf("popHead() is trying to return NULL function in grab_task_from_runtime()\n");
            // exit(EXIT_FAILURE);
        }
    }
    pthread_mutex_unlock(&queueLock);

    // try to steal
    // pthread_mutex_lock(&queueLock);
    // int i=0;
    srand(time(0));
    while (1){
        // printf("itr no %d", i);
        // i++;
        
        int randNum = (rand()%(num));

        pthread_mutex_lock(&queueLock);

        if(queues[randNum].isEmpty() || randNum == id){
            // printf("main q - %d empty  ", randNum);
            pthread_mutex_unlock(&queueLock);
            continue;
        }
        if(!queues[randNum].isEmpty()){
            // pthread_mutex_lock(&queueLock);
            func = queues[randNum].popBack();
            // pthread_mutex_unlock(&queueLock);
            if(func != NULL){
                pthread_mutex_unlock(&queueLock);
                break;
            }
            else {
                printf("popBack() is trying to return NULL function in grab_task_from_runtime()\n");
                // exit(EXIT_FAILURE);
            } 
        }
        else {
            printf("random q empty  ");
            pthread_mutex_unlock(&queueLock);
            continue;
        } 
        

        // for(int i = 0 ; i < num ;i++){
        //     func = queues[i].popBack();
        //     if(func != NULL){
        //         break;
        //     }
        //     func = queues[i].popHead();
        //     if(func != NULL){
        //         break;
        //     }
        // }


        pthread_mutex_unlock(&queueLock);
    }
    pthread_mutex_unlock(&queueLock);

    return func;
    
}

void push_task_to_runtime(std::function<void()> new_func, int id){

    printf("push_task_to_runtime\n");

    // if(new_func == NULL){
    //     printf("Task received in push_task_to_runtime is NULL\n");
    //     exit(EXIT_FAILURE);
    // }
    if(queues[id].isFull()){
        printf("Queue is full, cannot push_task_to_runtime\n");
        exit(EXIT_FAILURE);
    }
    else {
        // pthread_mutex_lock(&queueLock);
        queues[id].insertHead(new_func);
        // pthread_mutex_unlock(&queueLock);
    }

}

void execute_task(std::function<void()> task){

    printf("execute_task\n");
    if(task != NULL){
        task();
        printf("TASK EXECUTED SUCCESSFULLY\n\n");
    }
    // else {
    //     printf("TAASK IS NULL\n");
    //     exit(EXIT_FAILURE);
    // }
    
    
}

void find_and_execute_task() {

    // printf("find_and_execute_task\n");
    std::function<void()> task = grab_task_from_runtime();

    if(task != NULL) {
        execute_task(task);
        lock_finish();
        finish_counter--;
        unlock_finish();
    }
    // else {
    //     printf("Task grabbed from runtime is NULL in find_and_execute_task()\n");
    //     exit(EXIT_FAILURE);
    // }
}

void *worker_routine(void *ptr) {

    // printf("worker routine tid %ld\n", tid[0]);

    int *id = (int*)ptr;
    // printf("Id generated in worker_rotuine id %d\n", (*id));
    // pthread_setspecific(key, (void*)&id);  // set thread-specific data
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

void quill::init_runtime(){

    if(getenv("QUILL_WORKERS")!=NULL){
        num_workers = atoi(getenv("QUILL_WORKERS"));
    }

    int i;
    int *ids;
    ids=(int*)malloc(num*sizeof(int));
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&queueLock, NULL);

    // printf("initruntime\n");

    for (int j = 0; j < num; j++){
        ids[j] = j;
    }
    for (int j = 0; j < num; j++){
        printf("%d ", ids[j]);
    }
    printf("\n");


    for (int j = 0; j < num; j++){
        queues[j] = CircularDeque();
    }

    int ret = pthread_key_create(&key, NULL); //vishesh btaega after if afetr 340
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

    printf("async\n");

    lock_finish();
    finish_counter++;//concurrent access
    unlock_finish();

    int *pid = ((int*)(pthread_getspecific(key)));
    if(pid == NULL){
        printf("Error in push task\n");
        exit(EXIT_FAILURE);
    }
    int id = *pid;
    // printf("Id in received from get_specific in async %d\n", id);
    //thread-safe push_task_to_runtime
    push_task_to_runtime(lambda, id);
    return;
}

void quill::start_finish(){
    // printf("start_finish\n");
    lock_finish();
    finish_counter = 0; //reset
    unlock_finish();

}

void quill::end_finish(){
    // printf("end_finish\n");
    while(finish_counter != 0){
        find_and_execute_task();
    }

}

void quill::finalize_runtime(){
    printf("finalize_runtime\n");
    shutdown = true;
    
    for(int i = 1; i < num; i++) {
        pthread_join(tid[i], NULL);
    }

    pthread_key_delete(key);
    exit(EXIT_SUCCESS);
    return;

}

