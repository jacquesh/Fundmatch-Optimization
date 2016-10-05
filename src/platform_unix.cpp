#include "platform.h"

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

struct Mutex
{
    pthread_mutex_t mutex;
};

Mutex* createMutex()
{
    Mutex* result = (Mutex*)malloc(sizeof(Mutex));
    pthread_mutex_init(&result->mutex, NULL);
    return result;
};

void destroyMutex(Mutex* mutex)
{
    pthread_mutex_destroy(&mutex->mutex);
    free(mutex);
}

void lockMutex(Mutex* mutex)
{
    pthread_mutex_lock(&mutex->mutex);
}

void unlockMutex(Mutex* mutex)
{
    pthread_mutex_unlock(&mutex->mutex);
}

void sleepForMilliseconds(uint32_t milliseconds)
{
    usleep(1000*milliseconds);
}

int getCurrentUsername(size_t bufferLen, char* buffer)
{
    int result = getlogin_r(buffer, bufferLen);
    return result;
}