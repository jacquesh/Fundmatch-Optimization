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
        if((allocTenor == 0.0f) || (allocAmount == 0.0f))
            continue;

        if((allocTenor < 0.0f) || (allocAmount < 0.0f))
            return false;

        RequirementInfo& req = g_input.requirements[alloc.requirementIndex];
        float reqStart = (float)req.startDate;
        float reqEnd = (float)(req.startDate + req.tenor);
        float overlapStart = max(allocStart, reqStart);
        float overlapEnd = min(allocEnd, reqEnd);
        float overlapDuration = overlapEnd - overlapStart;
        if(overlapDuration < 1.0f)
            return false;

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
            allocStartIndex++;
        }
    }
    delete[] balancePoolValueRemaining;
    delete[] sourceValueRemaining;

    return true;
}

float computeFitness(Vector& position, int allocationCount, AllocationPointer* allocations)
{
    float result = 0.0f;
    for(auto iter=g_input.requirements.begin(); iter!=g_input.requirements.end(); iter++)
    {
        float tenor = (float)iter->tenor;
        float amount = (float)iter->amount;
        result += tenor*amount*RCF_INTEREST_RATE;
    }

    for(int allocID=0; allocID<allocationCount; allocID++)
    {
        AllocationPointer& alloc = allocations[allocID];
        float allocStart = alloc.getStartDate(position);
        float allocTenor = alloc.getTenor(position);
        float allocAmount = alloc.getAmount(position);
        float allocEnd = allocStart + allocTenor;

        if((allocTenor == 0.0f) || (allocAmount == 0.0f))
            continue;

        RequirementInfo& requirement = g_input.requirements[alloc.requirementIndex];
        float requirementStart = (float)requirement.startDate;
        float requirementTenor = (float)requirement.tenor;
        float requirementAmount = (float)requirement.amount;
        float requirementEnd = requirementStart + requirementTenor;

        float interestRate = BALANCEPOOL_INTEREST_RATE;
        if(alloc.sourceIndex != -1)
            interestRate = g_input.sources[alloc.sourceIndex].interestRate;

        if(allocStart < requirementStart)
        {
            // Add the interest for the time before the requirement start
            float extraTime = requirementStart - allocStart;
            result += extraTime * allocAmount * interestRate;
        }
        if(allocEnd > requirementEnd)
        {
            // Add the interest for the time after the requirement end
            float extraTime = allocEnd - requirementEnd;
            result += extraTime * allocAmount * interestRate;
        }

        // Subtract the difference in interest for the duration of the allocation
        float overlapStart = max(allocStart, requirementStart);
        float overlapEnd = min(allocEnd, requirementEnd);
        float overlapTime = overlapEnd - overlapStart;
        assert(overlapTime >= 0.0f);
        float interestDifference = RCF_INTEREST_RATE - interestRate;
        result -= overlapTime * allocAmount * interestDifference;
    }

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
    }
    rootNode.add("sources", sourceNodeList);
    rootNode.add("requirements", reqNodeList);
    rootNode.add("allocations", allocNodeList);

    Jzon::Writer writer;
    std::ofstream outFile(outFilename, std::ofstream::out);
    writer.writeStream(rootNode, outFile);
    outFile.close();
}
