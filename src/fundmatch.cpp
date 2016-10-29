#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fstream>
#include <vector>
#include <random>
#include <algorithm>

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
    if(this->coords)
    {
        delete[] this->coords;
    }
}

Vector& Vector::operator =(const Vector& other)
{
    // NOTE: We only need to delete/reallocate memory if the size has changed
    if(this->dimensions != other.dimensions)
    {
        if(this->coords)
            delete[] this->coords;
        this->coords = new float[other.dimensions];

        int copySize = min(this->dimensions, other.dimensions) * sizeof(float);
        memcpy(this->coords, other.coords, copySize);

        this->dimensions = other.dimensions;
    }
    else
    {
        memcpy(this->coords, other.coords, this->dimensions*sizeof(float));
    }

    return *this;
}

float& Vector::operator [](int index) const
{
    return coords[index];
}

float AllocationPointer::getStartDate(const Vector& data) const
{
    return data[this->allocStartDimension + START_DATE_OFFSET];
}
float AllocationPointer::getTenor(const Vector& data) const
{
    return data[this->allocStartDimension + TENOR_OFFSET];
}
float AllocationPointer::getAmount(const Vector& data) const
{
    return data[this->allocStartDimension + AMOUNT_OFFSET];
}
float AllocationPointer::getEndDate(const Vector& data) const
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

float AllocationPointer::getMinStartDate() const
{
    RequirementInfo& req = g_input.requirements[this->requirementIndex];
    float result = (float)req.startDate;
    if(this->sourceIndex >= 0)
    {
        SourceInfo& source = g_input.sources[this->sourceIndex];
        result = max(result, (float)source.startDate);
    }
    // NOTE: If this allocation comes from a balance pool then the start date is determined
    //       entirely by that of the requirement
    return result;
}

float AllocationPointer::getMaxStartDate() const
{
    RequirementInfo& req = g_input.requirements[this->requirementIndex];
    float result = (float)(req.startDate + req.tenor - 1);
    if(this->sourceIndex >= 0)
    {
        SourceInfo& source = g_input.sources[this->sourceIndex];
        result = min(result, (float)(source.startDate + source.tenor - 1));
    }
    // NOTE: If this allocation comes from a balance pool then the start date is determined
    //       entirely by that of the requirement
    return result;
}

float AllocationPointer::getMaxTenor(const Vector& data) const
{
    RequirementInfo& req = g_input.requirements[this->requirementIndex];
    float result = (float)req.tenor;
    if(this->sourceIndex >= 0)
    {
        SourceInfo& source = g_input.sources[this->sourceIndex];
        result = min(result, (float)source.tenor); // TODO: Should we take startDate into account?
    }
    // NOTE: If this allocation comes from a balance pool then the tenor is determined
    //       entirely by that of the requirement
    return result;
}

float AllocationPointer::getMaxAmount(const Vector& data) const
{
    RequirementInfo& req = g_input.requirements[this->requirementIndex];
    float result = (float)req.amount;
    if(this->sourceIndex >= 0)
    {
        SourceInfo& source = g_input.sources[this->sourceIndex];
        result = min(result, (float)source.amount); // TODO: Use getMaxAllocationAmount?
    }
    else
    {
        BalancePoolInfo& pool = g_input.balancePools[this->balancePoolIndex];
        result = min(result, (float)pool.amount); // TODO: Use a balancePool-equivalent of getMaxAllocationAmount?
    }
    return result;
}

void initializeAllocation(AllocationPointer& alloc, Vector& position,
         mt19937& rng)
{
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);

    int allocSourceIndex = alloc.sourceIndex;
    int allocBalanceIndex = alloc.balancePoolIndex;
    assert(((allocSourceIndex == -1) && (allocBalanceIndex >= 0)) ||
            ((allocSourceIndex >= 0) && (allocBalanceIndex == -1)));

    float minStartDate = alloc.getMinStartDate();
    float maxStartDate = alloc.getMaxStartDate();
    float maxTenor = alloc.getMaxTenor(position);
    float maxAmount = alloc.getMaxAmount(position);

    float dateRange = maxStartDate - minStartDate;
    assert(dateRange >= 0.0f);

    float startDate = minStartDate + (uniformf(rng) * dateRange);
    assert(startDate > 1.0f);
    alloc.setStartDate(position, startDate);

    float maxValidStartingTenor = maxStartDate - startDate;
    float tenor = uniformf(rng) * maxValidStartingTenor;
    alloc.setTenor(position, tenor);

    float amount = 0.5f * uniformf(rng) * maxAmount;
    alloc.setAmount(position, amount);
}

int maxAllocationTenor(SourceInfo& source, RequirementInfo& req)
{
    int sourceStart = source.startDate;
    int requireStart = req.startDate;
    int sourceEnd = source.startDate + source.tenor;
    int requireEnd = req.startDate + req.tenor;

    int validStart = max(sourceStart, requireStart);
    int validEnd = min(sourceEnd, requireEnd);

    // NOTE: We need there to be an overlap of at least 1 month (0-length loans are irrelevant)
    return validEnd - validStart;
}

float maxAllocationAmount(Vector& position, int allocationCount, AllocationPointer* allocations, int allocID)
{
    float result;
    float sourceValueRemaining;

    if(allocations[allocID].sourceIndex >= 0)
    {
        SourceInfo& source = g_input.sources[allocations[allocID].sourceIndex];
        result = (float)source.amount;
        sourceValueRemaining = (float)source.amount;
    }
    else
    {
        assert(allocations[allocID].balancePoolIndex >= 0);
        BalancePoolInfo& balancePool = g_input.balancePools[allocations[allocID].balancePoolIndex];
        result = (float)balancePool.amount;
        sourceValueRemaining = (float)balancePool.amount;
    }

    // Check that no sources ever get over-used (IE 2 allocations with value 300 from a source of 500 at the same time)
    vector<AllocationPointer*> allocationsByStart;
    for(int i=0; i<allocationCount; i++)
    {
        // NOTE: We don't want to consider the amount used by the allocation that we' trying to
        //       find the maximal feasible value for
        if(i != allocID)
            allocationsByStart.push_back(&allocations[i]);
    }
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

    int allocStartIndex = 0;
    int allocEndIndex = 0;
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
        if(nextAllocEndTime < nextAllocStartTime)
        {
            AllocationPointer* alloc = allocationsByEnd[allocEndIndex];
            sourceValueRemaining += alloc->getAmount(position);
            allocEndIndex++;
        }
        else
        {
            // Handle the allocation-start event
            AllocationPointer* alloc = allocationsByStart[allocStartIndex];
            sourceValueRemaining -= alloc->getAmount(position);
            allocStartIndex++;

            result = min(result, sourceValueRemaining);
        }
    }

    // NOTE: We can get a negative value if the solution is infeasible, but in that case we still
    //       only want to return 0 because that's how much money we can still allocate
    return max(0.0f, result);
}

bool isFeasible(Vector& position, int allocationCount, AllocationPointer* allocations)
{
    for(int allocID=0; allocID<allocationCount; allocID++)
    {
        AllocationPointer& alloc = allocations[allocID];
        float allocStart = alloc.getStartDate(position);
        float allocEnd = alloc.getEndDate(position);
        float allocTenor = alloc.getTenor(position);
        float allocAmount = alloc.getAmount(position);

        // NOTE: We don't care what happens in empty allocations
        if((allocTenor <= 0.0f) || (allocAmount <= 0.0f))
            continue;

        RequirementInfo& req = g_input.requirements[alloc.requirementIndex];
        float reqStart = (float)req.startDate;
        float reqEnd = (float)(req.startDate + req.tenor);
        float overlapStart = max(allocStart, reqStart);
        float overlapEnd = min(allocEnd, reqEnd);
        float overlapDuration = overlapEnd - overlapStart;
        if(overlapDuration < 1.0f)
            continue;

        if(alloc.sourceIndex >= 0)
        {
            SourceInfo& source = g_input.sources[alloc.sourceIndex];
            float sourceStart = (float)source.startDate;
            float sourceEnd = (float)(source.startDate + source.tenor);
            float sourceAmount = (float)source.amount;
            if((allocStart < sourceStart) || (allocStart > sourceEnd))
                return false;
            if(allocEnd > sourceEnd)
                return false;
            if(allocAmount > sourceAmount)
                return false;
        }
        else
        {
            assert(alloc.balancePoolIndex >= 0);
            BalancePoolInfo& balancePool = g_input.balancePools[alloc.balancePoolIndex];
            // TODO: Other properties, if we have any
            float balanceAmount = (float)balancePool.amount;
            if(allocAmount > balanceAmount)
                return false;
        }
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
    float* balancePoolValueRemaining = new float[g_input.balancePools.size()];
    for(int i=0; i<g_input.balancePools.size(); i++)
    {
        balancePoolValueRemaining[i] = (float)g_input.balancePools[i].amount;
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
        if(nextAllocEndTime < nextAllocStartTime)
        {
            AllocationPointer& oldAlloc = *allocationsByEnd[allocEndIndex];
            if(oldAlloc.getTenor(position) < 0.0f)
            {
                allocEndIndex++;
                continue;
            }
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
                float allocAmount = alloc->getAmount(position);
                if(alloc->sourceIndex >= 0)
                {
                    sourceValueRemaining[alloc->sourceIndex] += allocAmount;
                }
                else
                {
                    assert(alloc->balancePoolIndex >= 0);
                    balancePoolValueRemaining[alloc->balancePoolIndex] += allocAmount;
                }
            }
            allocEndIndex++;
        }
        else
        {
            // Handle the allocation-start event
            AllocationPointer* alloc = allocationsByStart[allocStartIndex];
            allocStartIndex++;
            if(alloc->getTenor(position) < 0.0f)
                continue;
            activeAllocations.push_back(alloc);
            float allocAmount = alloc->getAmount(position);
            if(alloc->sourceIndex >= 0)
            {
                sourceValueRemaining[alloc->sourceIndex] -= allocAmount;
                if(sourceValueRemaining[alloc->sourceIndex] < 0.0f)
                    return false;
            }
            else
            {
                assert(alloc->balancePoolIndex >= 0);
                balancePoolValueRemaining[alloc->balancePoolIndex] -= allocAmount;
                if(balancePoolValueRemaining[alloc->balancePoolIndex] < 0.0f)
                    return false;
            }
        }
    }
    delete[] balancePoolValueRemaining;
    delete[] sourceValueRemaining;

    return true;
}

float computeFitness(Vector& position, int allocationCount, AllocationPointer* allocations)
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
            nextReqStartTime = (float)g_input.requirements[g_input.requirementsByStart[reqStartIndex]].startDate;
        if(reqEndIndex < g_input.requirements.size())
            nextReqEndTime = (float)g_input.requirements[g_input.requirementsByEnd[reqEndIndex]].startDate +
                             (float)g_input.requirements[g_input.requirementsByEnd[reqEndIndex]].tenor;

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
            if(nextAllocEndTime < nextAllocStartTime)
            {
                // Handle the allocation-end event
                float allocTenor = allocationsByEnd[allocEndIndex]->getTenor(position);
                float allocAmount = allocationsByEnd[allocEndIndex]->getAmount(position);
                if((allocTenor <= 0) || (allocAmount <= 0.0f))
                {
                    allocEndIndex++;
                    continue;
                }

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
                allocStartIndex++;

                float allocTenor = alloc->getTenor(position);
                float allocAmount = alloc->getAmount(position);
                if((allocTenor <= 0) || (allocAmount <= 0.0f))
                    continue;

                activeAllocations.push_back(alloc);
                requirementValueRemaining[alloc->requirementIndex] -= allocAmount;
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
    rootNode.add("allocations", allocNodeList);

    Jzon::Writer writer;
    std::ofstream outFile(outFilename, std::ofstream::out);
    writer.writeStream(rootNode, outFile);
    outFile.close();

    return nonEmptyAllocationCount;
}
