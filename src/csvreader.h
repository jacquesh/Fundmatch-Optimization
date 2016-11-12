#ifndef _CSV_READER_H
#define _CSV_READER_H

#include <stdio.h>

const int MAX_CSV_FIELD_SIZE = 1023;

class CsvReader
{
public:
    CsvReader();
    ~CsvReader();

    int entryCount();
    int fieldCount();
    bool initialize(const char* filename);
    bool readNextEntry();
    char* field(int index);
    int fieldLength(int index);
    void copyFieldStr(int index, char** targetStr); // NOTE: Allocates the right amount of space and copies the field into it

private:
    FILE* fileHandle;
    char** fieldValues;
    int* fieldValueLengths;
    int _fieldCount;
    int _entryCount;
};

#ifdef CSV_READER_IMPLEMENTATION
#include <string.h>

// TODO: Handle escaped commas (or commas inside quotes, whatever)
CsvReader::CsvReader() :
    fileHandle(NULL), fieldValues(NULL), fieldValueLengths(0),
    _fieldCount(0), _entryCount(0) {}

CsvReader::~CsvReader()
{
    if(fileHandle != NULL)
    {
        fclose(fileHandle);
        fileHandle = NULL;
    }
    if(fieldValues != NULL)
    {
        for(int i=0; i<_fieldCount; i++)
        {
            delete[] fieldValues[i];
        }
        delete[] fieldValues;
        fieldValues = 0;

        delete[] fieldValueLengths;
        fieldValueLengths = 0;
    }
}

int CsvReader::entryCount()
{
    return _entryCount;
}

int CsvReader::fieldCount()
{
    return _fieldCount;
}

bool CsvReader::initialize(const char* filename)
{
    fileHandle = fopen(filename, "r");
    if(!fileHandle)
        return false;

    char c;
    int fileEntryCount = 0;
    while((c = (char)fgetc(fileHandle)) != EOF)
    {
        if(c == '\n')
            fileEntryCount++;
    }
    _entryCount = fileEntryCount - 1; // Ignore headings
    rewind(fileHandle);

    // Fast-forward past the first (heading) row, counting fields as we go
    int entryCommaCount = 0;
    while((c = (char)fgetc(fileHandle)) != '\n')
    {
        if(c == ',')
            entryCommaCount++;
    }
    _fieldCount = entryCommaCount + 1; // We don't have a comma at the end of the line
    fieldValueLengths = new int[_fieldCount];
    fieldValues = new char*[_fieldCount];
    for(int i=0; i<_fieldCount; i++)
    {
        fieldValues[i] = new char[MAX_CSV_FIELD_SIZE+1]; // For the trailing 0
        memset(fieldValues[i], 0, MAX_CSV_FIELD_SIZE+1);
    }

    // TODO: Read the headings into some or other buffer
    return true;
}

bool CsvReader::readNextEntry()
{
    int fieldIndex = 0;
    int fieldCharIndex = 0;
    memset(fieldValueLengths, 0, _fieldCount*sizeof(int));

    char c;
    while((c = (char)fgetc(fileHandle)) != '\n')
    {
        if(c == '\r')
            continue;

        if(c == ',')
        {
            fieldValueLengths[fieldIndex] = fieldCharIndex;
            fieldValues[fieldIndex][fieldCharIndex] = 0;
            fieldCharIndex = 0;
            fieldIndex++;
        }
        else
        {
            if(fieldCharIndex == MAX_CSV_FIELD_SIZE)
            {
                fprintf(stderr, "ERROR: Field string is too long: %s at index %d\n",
                        fieldValues[fieldIndex], fieldIndex);
                return false;
            }
            fieldValues[fieldIndex][fieldCharIndex++] = c;
        }
    }
    fieldValues[fieldIndex][fieldCharIndex] = 0; // Null-terminate the last field's string
    return true;
}

char* CsvReader::field(int index)
{
    return fieldValues[index];
}

int CsvReader::fieldLength(int index)
{
    return fieldValueLengths[index];
}

void CsvReader::copyFieldStr(int index, char** targetStr)
{
    char* strBuffer = new char[fieldLength(index) + 1];
    strcpy(strBuffer, field(index));

    *targetStr = strBuffer;
}

#endif // CSV_READER_IMPLEMENTATION

#endif // _CSV_READER_H
