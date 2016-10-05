#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fstream>

#include "jzon.h"

#define CSV_READER_IMPLEMENTATION
#include "csvreader.h"

#include "fundmatch.h"

static const int START_DATE_OFFSET = 0;
static const int TENOR_OFFSET = 1;
static const int AMOUNT_OFFSET = 2;

Vector::Vector(int dimCount)
    : dimensions(dimCount)
{
    this->coords = new float[dimensions];
}

Vector::Vector(const Vector& other)
    : dimensions(other.dimensions)
{
    this->coords = new float[this->dimensions];
    memcpy(this->coords, other.coords, this->dimensions*sizeof(float));
}

Vector::~Vector()
{
    if(coords)
    {
        delete[] coords;
    }
}

Vector& Vector::operator =(const Vector& other)
{
    this->~Vector();
    this->dimensions = other.dimensions;
    this->coords = new float[this->dimensions];
    memcpy(this->coords, other.coords, this->dimensions*sizeof(float));

    return *this;
}

float AllocationPointer::getStartDate(Vector& data)
{
    return data[this->allocStartDimension + START_DATE_OFFSET];
}
float AllocationPointer::getTenor(Vector& data)
{
    return data[this->allocStartDimension + TENOR_OFFSET];
}
float AllocationPointer::getAmount(Vector& data)
{
    return data[this->allocStartDimension + AMOUNT_OFFSET];
}
float AllocationPointer::getEndDate(Vector& data)
{
    return getStartDate(data) + getTenor(data);
}

void AllocationPointer::setStartDate(Vector& data, float value)
{
    data[this->allocStartDimension + START_DATE_OFFSET] = value;
}
void AllocationPointer::setTenor(Vector& data, float value)
{
    data[this->allocStartDimension + TENOR_OFFSET] = value;
}
void AllocationPointer::setAmount(Vector& data, float value)
{
    data[this->allocStartDimension + AMOUNT_OFFSET] = value;
}

bool isFeasible(Vector position, int allocationCount, AllocationPointer* allocations)
{
    for(int allocID=0; allocID<allocationCount; allocID++)
    {
        // TODO: Balance pools
        SourceInfo& source = g_input.sources[allocations[allocID].sourceIndex];
        RequirementInfo& req = g_input.requirements[allocations[allocID].requirementIndex];
        float allocStart = allocations[allocID].getStartDate(position);
        float allocEnd = allocations[allocID].getEndDate(position);
        float allocTenor = allocations[allocID].getTenor(position);
        float allocAmount = allocations[allocID].getAmount(position);

        float sourceStart = (float)source.startDate;
        float sourceEnd = (float)(source.startDate + source.tenor);
        float sourceAmount = (float)source.amount;

        if((allocStart < sourceStart) || (allocStart > sourceEnd))
            return false;
        if((allocTenor < 0.0f) || (allocEnd > sourceEnd))
            return false;
        if((allocAmount < 0.0f) || (allocAmount > sourceAmount))
            return false;
    }

    // Check that no sources ever get over-used (IE 2 allocations with value 300 from a source of 500 at the same time)
    vector<AllocationPointer*> allocationsByStart;
    for(int i=0; i<allocationCount; i++)
        allocationsByStart.push_back(&allocations[i]);
    vector<AllocationPointer*> allocationsByEnd(allocationsByStart);

    auto allocStartDateComparison = [&position](AllocationPointer* a, AllocationPointer* b)
    {
        return a->getStartDate(position) < b->getStartDate(position);
    };
    auto allocEndDateComparison = [&position](AllocationPointer* a, AllocationPointer* b)
    {
        return a->getEndDate(position) < b->getEndDate(position);
    };
    sort(allocationsByStart.begin(), allocationsByStart.end(), allocStartDateComparison);
    sort(allocationsByEnd.begin(), allocationsByEnd.end(), allocEndDateComparison);

    float* sourceValueRemaining = new float[g_input.sourceCount];
    for(int i=0; i<g_input.sourceCount; i++)
    {
        sourceValueRemaining[i] = (float)g_input.sources[i].amount;
    }

    int allocStartIndex = 0;
    int allocEndIndex = 0;
    vector<AllocationPointer*> activeAllocations;
    while((allocStartIndex < allocationCount) || (allocEndIndex < allocationCount))
    {
        float nextAllocStartTime = FLT_MAX;
        float nextAllocEndTime = FLT_MAX;
        if(allocStartIndex < allocationCount)
            nextAllocStartTime = allocationsByStart[allocStartIndex]->getStartDate(position);
        if(allocEndIndex < allocationCount)
            nextAllocEndTime = allocationsByEnd[allocEndIndex]->getEndDate(position);

        // Handle the allocation event
        // NOTE: It is significant that this is a strict inequality, because for very small
        //       tenor, we still want to handle the allocation start first
        // TODO: Balance pools
        if(nextAllocEndTime < nextAllocStartTime)
        {
            if(allocationsByEnd[allocEndIndex]->sourceIndex >= 0)
            {
                // Handle the allocation-end event
                AllocationPointer* alloc = nullptr;
                for(auto iter = activeAllocations.begin(); iter != activeAllocations.end(); iter++)
                {
                    if(*iter == allocationsByEnd[allocEndIndex])
                    {
                        alloc = *iter;
                        activeAllocations.erase(iter);
                        break;
                    }
                }
                assert(alloc != nullptr);
                sourceValueRemaining[alloc->sourceIndex] += alloc->getAmount(position);
            }
            allocEndIndex++;
        }
        else
        {
            // Handle the allocation-start event
            AllocationPointer* alloc = allocationsByStart[allocStartIndex];
            allocStartIndex++;
            if(alloc->sourceIndex >= 0)
            {
                activeAllocations.push_back(alloc);
                sourceValueRemaining[alloc->sourceIndex] -= alloc->getAmount(position);
                if(sourceValueRemaining[alloc->sourceIndex] < 0.0f)
                    return false;
            }
        }
    }
    delete[] sourceValueRemaining;

    return true;
}

float computeFitness(Vector position, int allocationCount, AllocationPointer* allocations)
{
    vector<AllocationPointer*> allocationsByStart;
    for(int i=0; i<allocationCount; i++)
        allocationsByStart.push_back(&allocations[i]);
    vector<AllocationPointer*> allocationsByEnd(allocationsByStart);

    auto allocStartDateComparison = [&position](AllocationPointer* a, AllocationPointer* b)
    {
        return a->getStartDate(position) < b->getStartDate(position);
    };
    auto allocEndDateComparison = [&position](AllocationPointer* a, AllocationPointer* b)
    {
        return a->getEndDate(position) < b->getEndDate(position);
    };
    sort(allocationsByStart.begin(), allocationsByStart.end(), allocStartDateComparison);
    sort(allocationsByEnd.begin(), allocationsByEnd.end(), allocEndDateComparison);

    float* requirementValueRemaining = new float[g_input.requirementCount];
    for(int i=0; i<g_input.requirementCount; i++)
    {
        requirementValueRemaining[i] = (float)g_input.requirements[i].amount;
    }
    bool* requirementActive = new bool[g_input.requirementCount];
    for(int i=0; i<g_input.requirementCount; i++)
        requirementActive[i] = false;

    float firstAllocationTime = allocationsByStart[0]->getStartDate(position);
    float firstRequirementTime = (float)g_input.requirements[requirementsByStart[0]].startDate;

    float result = 0.0f;
    float currentTime = min(firstAllocationTime, firstRequirementTime);
    int allocStartIndex = 0;
    int allocEndIndex = 0;
    int reqStartIndex = 0;
    int reqEndIndex = 0;
    vector<AllocationPointer*> activeAllocations;
    while((allocStartIndex < allocationCount) || (allocEndIndex < allocationCount) ||
          (reqStartIndex < g_input.requirementCount) || (reqEndIndex < g_input.requirementCount))
    {
        float nextAllocStartTime = FLT_MAX;
        float nextAllocEndTime = FLT_MAX;
        float nextReqStartTime = FLT_MAX;
        float nextReqEndTime = FLT_MAX;
        if(allocStartIndex < allocationCount)
            nextAllocStartTime = allocationsByStart[allocStartIndex]->getStartDate(position);
        if(allocEndIndex < allocationCount)
            nextAllocEndTime = allocationsByEnd[allocEndIndex]->getEndDate(position);
        if(reqStartIndex < g_input.requirementCount)
            nextReqStartTime = (float)g_input.requirements[reqStartIndex].startDate;
        if(reqEndIndex < g_input.requirementCount)
            nextReqEndTime = (float)g_input.requirements[reqEndIndex].startDate +
                             (float)g_input.requirements[reqEndIndex].tenor;

        float nextAllocEventTime = min(nextAllocStartTime, nextAllocEndTime);
        float nextReqEventTime = min(nextReqStartTime, nextReqEndTime);

        float previousTime = currentTime;
        currentTime = min(nextAllocEventTime, nextReqEventTime);
        float timeElapsed = currentTime - previousTime;

        // Add up the costs of the allocations for this timestep
        for(int j=0; j<activeAllocations.size(); j++)
        {
            AllocationPointer* activeAlloc = activeAllocations[j];
            float interestRate = BALANCEPOOL_INTEREST_RATE;
            if(activeAlloc->sourceIndex != -1)
                interestRate = g_input.sources[activeAlloc->sourceIndex].interestRate;

            result += timeElapsed * activeAlloc->getAmount(position) * interestRate;
        }

        // Add the cost of the unsatisfied requirements (IE the cost to satisfy them via RCF)
        for(int reqID=0; reqID<g_input.requirementCount; reqID++)
        {
            if(requirementActive[reqID] && (requirementValueRemaining[reqID] > 0.0f))
                result += timeElapsed * requirementValueRemaining[reqID] * RCF_INTEREST_RATE;
        }

        // Handle the event that we stopped on, depending on what type it is
        if(nextReqEventTime <= nextAllocEventTime)
        {
            // Handle the requirement event
            if(nextReqEndTime <= nextReqStartTime)
            {
                // Handle the requirement-end event
                int reqIndex = requirementsByEnd[reqEndIndex];
                requirementActive[reqIndex] = false;
                reqEndIndex++;
            }
            else
            {
                // Handle the requirement-start event
                int reqIndex = requirementsByStart[reqStartIndex];
                requirementActive[reqIndex] = true;
                reqStartIndex++;
            }
        }
        else
        {
            // Handle the allocation event
            // NOTE: It is significant that this is a strict inequality, because for very small
            //       tenor, we still want to handle the allocation start first
            // TODO: This will break if we every get a negative tenor, we currently don't
            //       check that solutions are feasible before computing cost, that needs to be done
            if(nextAllocEndTime < nextAllocStartTime)
            {
                // Handle the allocation-end event
                AllocationPointer* alloc = nullptr;
                for(auto iter = activeAllocations.begin(); iter != activeAllocations.end(); iter++)
                {
                    if(*iter == allocationsByEnd[allocEndIndex])
                    {
                        alloc = *iter;
                        activeAllocations.erase(iter);
                        break;
                    }
                }
                assert(alloc != nullptr);
                allocEndIndex++;
                requirementValueRemaining[alloc->requirementIndex] += alloc->getAmount(position);
            }
            else
            {
                // Handle the allocation-start event
                AllocationPointer* alloc = allocationsByStart[allocStartIndex];
                activeAllocations.push_back(alloc);
                allocStartIndex++;
                requirementValueRemaining[alloc->requirementIndex] -= alloc->getAmount(position);
            }
        }
    }

    delete[] requirementValueRemaining;
    delete[] requirementActive;

    return result;
}

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

#if 0
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
#endif

void writeOutputData(InputData input, int allocCount, AllocationPointer* allocations,
                     Vector solution, const char* outFilename)
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
        int startDate = (int)(allocations[allocID].getStartDate(solution) + 0.5f);
        int tenor = (int)(allocations[allocID].getTenor(solution) + 0.5f);
        int amount = (int)(allocations[allocID].getAmount(solution) + 0.5f);

        int month = startDate % 12;
        int year = startDate / 12;
        snprintf(dateStr, MAX_DATE_STR_LEN, "01-%02d-%04d", month, year);

        Jzon::Node allocNode = Jzon::object();
        allocNode.add("sourceIndex", allocations[allocID].sourceIndex);
        allocNode.add("balancePoolIndex", allocations[allocID].balanceIndex);
        allocNode.add("requirementIndex", allocations[allocID].requirementIndex);
        allocNode.add("startDate", dateStr);
        allocNode.add("tenor", tenor);
        allocNode.add("amount", amount);
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
