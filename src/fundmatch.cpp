#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include <fstream>

#include "jzon.h"

#define CSV_READER_IMPLEMENTATION
#include "csvreader.h"

#include "fundmatch.h"

TaxClass str2TaxClass(const char* str)
{
    switch(*str)
    {
        case '\0':
            return TaxClass::None;
        case 'I':
            return TaxClass::IPF;
        case 'U':
            return TaxClass::UPF;
        case 'C':
            return TaxClass::CF;
        default:
            fprintf(stderr, "ERROR: Unrecognized tax class %s\n", str);
            return TaxClass::None;
    }
}

const char* taxClass2Str(TaxClass cls)
{
    switch(cls)
    {
        case TaxClass::None:
            return "";
        case TaxClass::IPF:
            return "IPF";
        case TaxClass::UPF:
            return "UPF";
        case TaxClass::CF:
            return "CF";
        default:
            return "(Unkown)";
    }
}

bool loadSourceData(const char* inputFilename, InputData& input)
{
    CsvReader csvIn;
    if(!csvIn.initialize(inputFilename))
        return false;

    int entryCount = csvIn.entryCount();
    if(entryCount == 0)
        return false;

    assert(csvIn.fieldCount() == 9);
    SourceInfo* outArray = new SourceInfo[entryCount];
    for(int i=0; i<entryCount; i++)
    {
        csvIn.readNextEntry();
        csvIn.copyFieldStr(1, &outArray[i].segment);

        memset(&outArray[i].startDate, 0, sizeof(tm));
        sscanf(csvIn.field(2), "%d/%d/%d", &outArray[i].startDate.tm_mday,
                                           &outArray[i].startDate.tm_mon,
                                           &outArray[i].startDate.tm_year);
        outArray[i].startDate.tm_mon -= 1; // Month is in the range [0,11] not [1,12]
        outArray[i].startDate.tm_year -= 1900; // tm_year is the number of years since 1900

        outArray[i].tenor = atoi(csvIn.field(3));
        outArray[i].amount = atoi(csvIn.field(4));
        csvIn.copyFieldStr(5, &outArray[i].sourceType);
        csvIn.copyFieldStr(6, &outArray[i].sourceTypeCategory);
        outArray[i].taxClass = str2TaxClass(csvIn.field(7));
        outArray[i].interestRate = (float)atof(csvIn.field(8));
    }
    input.sourceCount = entryCount;
    input.sources = outArray;

    return true;
}

bool loadRequirementData(const char* inputFilename, InputData& input)
{
    CsvReader csvIn;
    if(!csvIn.initialize(inputFilename))
        return false;

    int entryCount = csvIn.entryCount();
    if(entryCount == 0)
        return false;

    assert(csvIn.fieldCount() == 8);
    RequirementInfo* outArray = new RequirementInfo[entryCount];
    for(int i=0; i<entryCount; i++)
    {
        csvIn.readNextEntry();
        csvIn.copyFieldStr(1, &outArray[i].segment);

        memset(&outArray[i].startDate, 0, sizeof(tm));
        sscanf(csvIn.field(2), "%d/%d/%d", &outArray[i].startDate.tm_mday,
                                           &outArray[i].startDate.tm_mon,
                                           &outArray[i].startDate.tm_year);
        outArray[i].startDate.tm_mon -= 1; // Month is in the range [0,11] not [1,12]
        outArray[i].startDate.tm_year -= 1900; // tm_year is the number of years since 1900

        outArray[i].tenor = atoi(csvIn.field(3));
        outArray[i].amount = atoi(csvIn.field(4));
        csvIn.copyFieldStr(5, &outArray[i].tier);
        csvIn.copyFieldStr(6, &outArray[i].purpose);
        outArray[i].taxClass = str2TaxClass(csvIn.field(7));
    }
    input.requirementCount = entryCount;
    input.requirements = outArray;

    return true;
}

void writeOutputData(InputData input, int allocCount, AllocationInfo* allocations,
                     const char* outFilename)
{
    Jzon::Node rootNode = Jzon::object();

    Jzon::Node sourceNodeList = Jzon::array();
    const int MAX_DATE_STR_LEN = 12;
    char dateStr[MAX_DATE_STR_LEN];
    for(int sourceID=0; sourceID<input.sourceCount; sourceID++)
    {
        strftime(dateStr, MAX_DATE_STR_LEN,
                 "%d-%m-%Y", &input.sources[sourceID].startDate);

        Jzon::Node sourceNode = Jzon::object();
        sourceNode.add("startDate", dateStr);
        sourceNode.add("tenor", input.sources[sourceID].tenor);
        sourceNode.add("amount", input.sources[sourceID].amount);
        sourceNode.add("taxClass", taxClass2Str(input.sources[sourceID].taxClass));
        sourceNode.add("interestRate", input.sources[sourceID].interestRate);
        sourceNodeList.add(sourceNode);
    }

    Jzon::Node reqNodeList = Jzon::array();
    for(int reqID=0; reqID<input.requirementCount; reqID++)
    {
        strftime(dateStr, MAX_DATE_STR_LEN,
                 "%d-%m-%Y", &input.requirements[reqID].startDate);

        Jzon::Node reqNode = Jzon::object();
        reqNode.add("startDate", dateStr);
        reqNode.add("tenor", input.requirements[reqID].tenor);
        reqNode.add("amount", input.requirements[reqID].amount);
        reqNode.add("taxClass", taxClass2Str(input.requirements[reqID].taxClass));
        reqNodeList.add(reqNode);
    }

    Jzon::Node allocNodeList = Jzon::array();
    for(int allocID=0; allocID<allocCount; allocID++)
    {
        strftime(dateStr, MAX_DATE_STR_LEN,
                 "%d-%m-%Y", &allocations[allocID].startDate);

        Jzon::Node allocNode = Jzon::object();
        allocNode.add("sourceIndex", allocations[allocID].sourceIndex);
        allocNode.add("requirementIndex", allocations[allocID].requirementIndex);
        allocNode.add("startDate", dateStr);
        allocNode.add("tenor", allocations[allocID].tenor);
        allocNode.add("amount", allocations[allocID].amount);
        allocNodeList.add(allocNode);
    }
    rootNode.add("sources", sourceNodeList);
    rootNode.add("requirements", reqNodeList);
    rootNode.add("allocations", allocNodeList);

    Jzon::Writer writer;
    std::ofstream outFile(outFilename, std::ofstream::out);
    writer.writeStream(rootNode, outFile);
    outFile.close();
}
