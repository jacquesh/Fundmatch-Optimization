#include "platform.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

struct Mutex
{
    CRITICAL_SECTION critSec;
};

Mutex* createMutex()
{
    Mutex* result = (Mutex*)malloc(sizeof(Mutex));
    InitializeCriticalSection(&result->critSec);
    return result;
};

void destroyMutex(Mutex* mutex)
{
    DeleteCriticalSection(&mutex->critSec);
    free(mutex);
}

void lockMutex(Mutex* mutex)
{
    EnterCriticalSection(&mutex->critSec);
}

void unlockMutex(Mutex* mutex)
{
    LeaveCriticalSection(&mutex->critSec);
}

void sleepForMilliseconds(uint32_t milliseconds)
{
    Sleep(milliseconds);
}

int64_t getClockValue()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result.QuadPart;
}

int64_t getClockFrequency()
{
    LARGE_INTEGER result;
    QueryPerformanceFrequency(&result);
    return result.QuadPart;
}

int getCurrentUserName(size_t bufferLen, char* buffer)
{
    int result = GetUserName(buffer, (LPDWORD)&bufferLen);
    return result;
}
