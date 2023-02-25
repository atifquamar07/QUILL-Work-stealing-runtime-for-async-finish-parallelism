#include <bits/stdc++.h>
#include <cstring>
#include <functional>
#include <stdlib.h>

int num_workers = 1;

class quillRuntime {

    public:

        static void lock_finish();

        static void unlock_finish();

        static void lock_queue();

        static void unlock_queue();

        static std::function<void()> grab_task_from_runtime();

        static void push_task_to_runtime();

        static void execute_task();

        static void find_and_execute_task(std::function<void()> task);

        static void *worker_routine(void *ptr);

};

