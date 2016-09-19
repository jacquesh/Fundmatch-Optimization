#include <stdio.h>
#include <stdlib.h>
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
        int sourceID = atoi(csvIn.field(0));
        assert(sourceID == i+1);

        csvIn.copyFieldStr(1, &outArray[i].segment);

        int day, month, year;
        sscanf(csvIn.field(2), "%d/%d/%d", &day, &month, &year);
        outArray[i].startDate = year*12 + month;

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

bool loadBalancePoolData(const char* inputFilename, InputData& input)
{
    CsvReader csvIn;
    if(!csvIn.initialize(inputFilename))
        return false;

    int entryCount = csvIn.entryCount();
    if(entryCount == 0)
        return false;

    assert(csvIn.fieldCount() == 10);
    BalancePoolInfo* outArray = new BalancePoolInfo[entryCount];
    for(int i=0; i<entryCount; i++)
    {
        csvIn.readNextEntry();
        int balanceID = atoi(csvIn.field(0));
        assert(balanceID == i+1);

        outArray[i].amount = (float)atof(csvIn.field(9));
    }

    input.balancePoolCount = entryCount;
    input.balancePools = outArray;
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
        int reqID = atoi(csvIn.field(0));
        assert(reqID == i+1);

        csvIn.copyFieldStr(1, &outArray[i].segment);

        int day, month, year;
        sscanf(csvIn.field(2), "%d/%d/%d", &day, &month, &year);
        outArray[i].startDate = year*12 + month;

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

bool loadAllocationData(const char* inputFilename, AllocationInfo** allocations, int& allocationCount)
{
    CsvReader csvIn;
    if(!csvIn.initialize(inputFilename))
        return false;

    int entryCount = csvIn.entryCount();
    if(entryCount == 0)
        return false;

    assert(csvIn.fieldCount() == 7);
    AllocationInfo* outArray = new AllocationInfo[entryCount];
    for(int i=0; i<entryCount; i++)
    {
        csvIn.readNextEntry();
        outArray[i].requirementIndex = atoi(csvIn.field(1));
        outArray[i].sourceIndex = atoi(csvIn.field(2));
        outArray[i].balanceIndex = atoi(csvIn.field(3));

        int day, month, year;
        sscanf(csvIn.field(2), "%d/%d/%d", &day, &month, &year);
        outArray[i].startDate = year*12 + month;

        outArray[i].tenor = atoi(csvIn.field(5));
        outArray[i].amount = atoi(csvIn.field(6));
    }

    *allocations = outArray;
    allocationCount = entryCount;
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
        int month = input.sources[sourceID].startDate % 12;
        int year = input.sources[sourceID].startDate / 12;
        snprintf(dateStr, MAX_DATE_STR_LEN, "01-%02d-%04d", month, year);

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
        int month = input.requirements[reqID].startDate % 12;
        int year = input.requirements[reqID].startDate / 12;
        snprintf(dateStr, MAX_DATE_STR_LEN, "01-%02d-%04d", month, year);

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
        int month = allocations[allocID].startDate % 12;
        int year = allocations[allocID].startDate / 12;
        snprintf(dateStr, MAX_DATE_STR_LEN, "01-%02d-%04d", month, year);

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
