#include <functional>

void find_and_execute_task();

void* worker_routine(void* arg);

std::function<void()> pop_task_from_runtime();