#include "timer.h"
#include <unistd.h>
#include <condition_variable>

//每个host上有一个独立的kernel锁，表示host上不会出现socket操作的并行
extern std :: mutex kernel_mutex;
extern std :: condition_variable kernel_cv; 
extern bool ARQflag;

void *TimerThread(void *vargp) {
    while (true) {
        usleep(50 * 1000);
        kernel_cv.notify_all();
        usleep(50 * 1000);
        kernel_cv.notify_all();
        usleep(50 * 1000);
        kernel_cv.notify_all();
        ARQflag = true;
    }

    return NULL;
}