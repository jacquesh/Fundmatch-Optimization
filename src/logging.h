#ifndef _LOGGING_H
#define _LOGGING_H

#include <stdio.h>

class FileLogger
{
public:
    FileLogger(const char* filename);
    ~FileLogger();

    void log(const char* message, ...);

private:
    FILE* outFile;
};

#endif // _LOGGING_H
