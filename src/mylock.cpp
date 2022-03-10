#include "mylock.h"

int nreader;
sem_t mymutex, noreader;

//useful for one sender and multiple reveivers

void initMyLock() {
    sem_init(&mymutex, 0, 1);
    sem_init(&noreader, 0, 1);
    nreader = 0; // n receivers are waiting for output
}

void ReaderIn() {
    sem_wait(&mymutex);
    ++nreader;
    if (nreader == 1) sem_wait(&noreader);
    sem_post(&mymutex);
}
void ReaderOut() {
    sem_wait(&mymutex);
    --nreader;
    if (nreader == 0) sem_post(&noreader);
    sem_post(&mymutex);
}

void WriterIn() {
    sem_wait(&noreader);
}
void WriterOut() {
    sem_post(&noreader);
}


