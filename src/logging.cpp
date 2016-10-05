#include "logging.h"

#include <stdio.h>
#include <stdarg.h>

FileLogger::FileLogger(const char* filename)
{
    outFile = fopen(filename, "w");
}

FileLogger::~FileLogger()
{
    if(outFile)
    {
        fclose(outFile);
        outFile = nullptr;
    }
}

void FileLogger::log(const char* message, ...)
{
    if(outFile)
    {
        va_list args;
        va_start(args, message);
        vfprintf(outFile, message, args);
        va_end(args);
        fflush(outFile);
    }
}
