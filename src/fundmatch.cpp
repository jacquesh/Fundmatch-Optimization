#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fstream>
#include <vector>
#include <random>

#include "jzon.h"

#define CSV_READER_IMPLEMENTATION
#include "csvreader.h"

#include "fundmatch.h"

using namespace std;

static const int START_DATE_OFFSET = 0;
static const int TENOR_OFFSET = 1;
static const int AMOUNT_OFFSET = 2;

InputData g_input;

Vector::Vector()
    : dimensions(0), coords(nullptr)
{
}

Vector::Vector(int dimCount)
    : dimensions(dimCount)
{
    if(this->dimensions > 0)
        this->coords = new float[this->dimensions];
    else
        this->coords = nullptr;
}

Vector::Vector(const Vector& other)
    : dimensions(other.dimensions)
{
    if(this->dimensions > 0)
    {
        this->coords = new float[this->dimensions];
        memcpy(this->coords, other.coords, this->dimensions*sizeof(float));
    }
    else
    {
        this->coords = nullptr;
    }
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

void initializeAllocation(AllocationPointer& alloc, Vector& position,
        uniform_real_distribution<float>& uniformf, mt19937& rng)
{
    int allocSourceIndex = alloc.sourceIndex;
    int allocBalanceIndex = alloc.balanceIndex;
    assert(((allocSourceIndex == -1) && (allocBalanceIndex >= 0)) ||
            ((allocSourceIndex >= 0) && (allocBalanceIndex == -1)));

    RequirementInfo& firstReq = g_input.requirements[g_input.requirementsByStart[0]];
    RequirementInfo& lastReq = g_input.requirements[g_input.requirementsByEnd[g_input.requirements.size()-1]];
    int minReqDate = firstReq.startDate;
    int maxReqDate = lastReq.startDate + lastReq.tenor;
    assert(maxReqDate > minReqDate);

    float minStartDate = (float)minReqDate;
    float maxStartDate = (float)maxReqDate;
    float maxTenor = -1;
    float maxAmount = -1;
    if(allocSourceIndex >= 0)
    {
        SourceInfo& allocSource = g_input.sources[allocSourceIndex];
        int sourceEndDate = allocSource.startDate + allocSource.tenor;
        if(allocSource.startDate > minReqDate)
            minStartDate = (float)allocSource.startDate;
        if(sourceEndDate < maxReqDate)
            maxStartDate = (float)sourceEndDate;
        maxTenor = (float)allocSource.tenor;
        maxAmount = (float)allocSource.amount;
    }
    else // NOTE: By the assert above, we necessarily have a balance pool here
    {
        BalancePoolInfo& allocBalance = g_input.balancePools[allocBalanceIndex];
        maxTenor = 1; // TODO
        maxAmount = (float)allocBalance.amount;
    }

    float dateRange = maxStartDate - minStartDate;
    float startDate = minStartDate + (uniformf(rng) * dateRange);
    alloc.setStartDate(position, startDate);

    // NOTE: We use maxValidStarting tenor so that we never generate an infeasible tenor.
    //       This is valid for velocities in PSO as well, because if the startDate is close to
    //       the max tenor, then we don't want a large velocity anyways.
    //       We technically would want a velocity that can be large and negative in this case,
    //       but it shouldn't make too much of a difference since other particles will attract it.
    float maxValidStartingTenor = maxStartDate - startDate;
    float tenor = uniformf(rng) * maxValidStartingTenor;
    alloc.setTenor(position, tenor);

    float amount = uniformf(rng) * maxAmount;
    alloc.setAmount(position, amount);
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

    float* sourceValueRemaining = new float[g_input.sources.size()];
    for(int i=0; i<g_input.sources.size(); i++)
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

    float* requirementValueRemaining = new float[g_input.requirements.size()];
    for(int i=0; i<g_input.requirements.size(); i++)
    {
        requirementValueRemaining[i] = (float)g_input.requirements[i].amount;
    }
    bool* requirementActive = new bool[g_input.requirements.size()];
    for(int i=0; i<g_input.requirements.size(); i++)
        requirementActive[i] = false;

    float firstAllocationTime = allocationsByStart[0]->getStartDate(position);
    float firstRequirementTime = (float)g_input.requirements[g_input.requirementsByStart[0]].startDate;

    float result = 0.0f;
    float currentTime = min(firstAllocationTime, firstRequirementTime);
    int allocStartIndex = 0;
    int allocEndIndex = 0;
    int reqStartIndex = 0;
    int reqEndIndex = 0;
    vector<AllocationPointer*> activeAllocations;
    while((allocStartIndex < allocationCount) || (allocEndIndex < allocationCount) ||
          (reqStartIndex < g_input.requirements.size()) || (reqEndIndex < g_input.requirements.size()))
    {
        float nextAllocStartTime = FLT_MAX;
        float nextAllocEndTime = FLT_MAX;
        float nextReqStartTime = FLT_MAX;
        float nextReqEndTime = FLT_MAX;
        if(allocStartIndex < allocationCount)
            nextAllocStartTime = allocationsByStart[allocStartIndex]->getStartDate(position);
        if(allocEndIndex < allocationCount)
            nextAllocEndTime = allocationsByEnd[allocEndIndex]->getEndDate(position);
        if(reqStartIndex < g_input.requirements.size())
            nextReqStartTime = (float)g_input.requirements[reqStartIndex].startDate;
        if(reqEndIndex < g_input.requirements.size())
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
        for(int reqID=0; reqID<g_input.requirements.size(); reqID++)
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
                int reqIndex = g_input.requirementsByEnd[reqEndIndex];
                requirementActive[reqIndex] = false;
                reqEndIndex++;
            }
            else
            {
                // Handle the requirement-start event
                int reqIndex = g_input.requirementsByStart[reqStartIndex];
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

    int entryCount = csvIn.entryCount();
    if(entryCount == 0)
        return false;

    assert(csvIn.fieldCount() == 10);
    for(int i=0; i<entryCount; i++)
    {
        BalancePoolInfo newInfo = {};
        csvIn.readNextEntry();
        int balanceID = atoi(csvIn.field(0));
        assert(balanceID == i+1);

        newInfo.amount = (float)atof(csvIn.field(9));

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
    for(int i=0; i<entryCount; i++)
    {
        AllocationInfo newInfo = {};

        csvIn.readNextEntry();
        newInfo.requirementIndex = atoi(csvIn.field(1));
        newInfo.sourceIndex = atoi(csvIn.field(2));
        newInfo.balanceIndex = atoi(csvIn.field(3));

        int day, month, year;
        sscanf(csvIn.field(2), "%d/%d/%d", &day, &month, &year);
        newInfo.startDate = year*12 + month;

        newInfo.tenor = atoi(csvIn.field(5));
        newInfo.amount = atoi(csvIn.field(6));

        allocations.push_back(newInfo);
    }

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
