#include "dataio.h"

#include "assert.h"

#include <fstream>
#include <vector>

#define CSV_READER_IMPLEMENTATION
#include "csvreader.h"

#include "fundmatch.h"
#include "jzon.h"

using namespace std;

static TaxClass str2TaxClass(const char* str)
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

static const char* taxClass2Str(TaxClass cls)
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

    assert(csvIn.fieldCount() == 9);
    int entryCount = csvIn.entryCount();
    for(int i=0; i<entryCount; i++)
    {
        SourceInfo newInfo = {};
        csvIn.readNextEntry();
        int sourceID = atoi(csvIn.field(0));
        assert(sourceID == i+1);

        csvIn.copyFieldStr(1, &newInfo.segment);

        int day, month, year;
        sscanf(csvIn.field(2), "%d/%d/%d", &day, &month, &year);
        newInfo.startDate = year*12 + month;

        newInfo.tenor = atoi(csvIn.field(3));
        newInfo.amount = atoi(csvIn.field(4));
        csvIn.copyFieldStr(5, &newInfo.sourceType);
        csvIn.copyFieldStr(6, &newInfo.sourceTypeCategory);
        newInfo.taxClass = str2TaxClass(csvIn.field(7));
        newInfo.interestRate = (float)atof(csvIn.field(8));

        input.sources.push_back(newInfo);
    }

    return true;
}

bool loadBalancePoolData(const char* inputFilename, InputData& input)
{
    CsvReader csvIn;
    if(!csvIn.initialize(inputFilename))
        return false;

    assert(csvIn.fieldCount() == 10);
    int entryCount = csvIn.entryCount();
    for(int i=0; i<entryCount; i++)
    {
        BalancePoolInfo newInfo = {};
        csvIn.readNextEntry();
        int balanceID = atoi(csvIn.field(0));
        assert(balanceID == i+1);

        newInfo.amount = (int)atof(csvIn.field(9));

        input.balancePools.push_back(newInfo);
    }

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
    for(int i=0; i<entryCount; i++)
    {
        RequirementInfo newInfo = {};

        csvIn.readNextEntry();
        int reqID = atoi(csvIn.field(0));
        assert(reqID == i+1);

        csvIn.copyFieldStr(1, &newInfo.segment);

        int day, month, year;
        sscanf(csvIn.field(2), "%d/%d/%d", &day, &month, &year);
        newInfo.startDate = year*12 + month;

        newInfo.tenor = atoi(csvIn.field(3));
        newInfo.amount = atoi(csvIn.field(4));
        csvIn.copyFieldStr(5, &newInfo.tier);
        csvIn.copyFieldStr(6, &newInfo.purpose);
        newInfo.taxClass = str2TaxClass(csvIn.field(7));

        input.requirements.push_back(newInfo);
    }

    return true;
}

Vector loadAllocationData(const char* inputFilename, vector<AllocationPointer>& allocations)
{
    CsvReader csvIn;
    if(!csvIn.initialize(inputFilename))
        return Vector();

    int entryCount = csvIn.entryCount();
    if(entryCount == 0)
        return Vector();

    Vector result(entryCount * DIMENSIONS_PER_ALLOCATION);
    assert(csvIn.fieldCount() == 7);
    for(int i=0; i<entryCount; i++)
    {
        csvIn.readNextEntry();
        AllocationPointer newAlloc = {};

        // NOTE: We subtract 1 here because we're using 0-based indices and the data uses 1-based
        newAlloc.requirementIndex = atoi(csvIn.field(1)) - 1;
        newAlloc.sourceIndex = atoi(csvIn.field(2)) - 1;
        newAlloc.balancePoolIndex = atoi(csvIn.field(3)) - 1;

        int day, month, year;
        sscanf(csvIn.field(4), "%d/%d/%d", &day, &month, &year);
        int startDate = year*12 + month;

        int tenor = atoi(csvIn.field(5));
        int amount = atoi(csvIn.field(6));

        newAlloc.setStartDate(result, (float)startDate);
        newAlloc.setTenor(result, (float)tenor);
        newAlloc.setAmount(result, (float)amount);

        allocations.push_back(newAlloc);
    }
    return result;
}

int writeOutputData(InputData input, int allocCount, AllocationPointer* allocations,
                     Vector solution, const char* outFilename)
{
    Jzon::Node rootNode = Jzon::object();

    Jzon::Node sourceNodeList = Jzon::array();
    const int MAX_DATE_STR_LEN = 12;
    char dateStr[MAX_DATE_STR_LEN];
    for(int sourceID=0; sourceID<input.sources.size(); sourceID++)
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
    for(int reqID=0; reqID<input.requirements.size(); reqID++)
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

    Jzon::Node bplNodeList = Jzon::array();
    for(int balanceID=0; balanceID<input.balancePools.size(); balanceID++)
    {
        // TODO
    }

    int nonEmptyAllocationCount = 0;
    Jzon::Node allocNodeList = Jzon::array();
    for(int allocID=0; allocID<allocCount; allocID++)
    {
        int startDate = (int)(allocations[allocID].getStartDate(solution) + 0.5f);
        int tenor = (int)(allocations[allocID].getTenor(solution) + 0.5f);
        int amount = (int)(allocations[allocID].getAmount(solution) + 0.5f);

        if((tenor <= 0) || (amount <= 0))
            continue;

        int month = startDate % 12;
        int year = startDate / 12;
        snprintf(dateStr, MAX_DATE_STR_LEN, "01-%02d-%04d", month, year);

        Jzon::Node allocNode = Jzon::object();
        allocNode.add("sourceIndex", allocations[allocID].sourceIndex);
        allocNode.add("balancePoolIndex", allocations[allocID].balancePoolIndex);
        allocNode.add("requirementIndex", allocations[allocID].requirementIndex);
        allocNode.add("startDate", dateStr);
        allocNode.add("tenor", tenor);
        allocNode.add("amount", amount);
        allocNodeList.add(allocNode);
        nonEmptyAllocationCount++;
    }
    rootNode.add("sources", sourceNodeList);
    rootNode.add("requirements", reqNodeList);
    rootNode.add("balancePools", bplNodeList);
    rootNode.add("allocations", allocNodeList);

    Jzon::Writer writer;
    std::ofstream outFile(outFilename, std::ofstream::out);
    writer.writeStream(rootNode, outFile);
    outFile.close();

    return nonEmptyAllocationCount;
}