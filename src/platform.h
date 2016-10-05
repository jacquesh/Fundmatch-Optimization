#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <stdint.h>

struct Mutex;
Mutex* createMutex();
void destroyMutex(Mutex* mutex);
void lockMutex(Mutex* mutex);
void unlockMutex(Mutex* mutex);

void sleepForMilliseconds(uint32_t milliseconds);

int64_t getClockValue();
int64_t getClockFrequency();

int getCurrentUserName(size_t bufferLen, char* buffer);

#endif
