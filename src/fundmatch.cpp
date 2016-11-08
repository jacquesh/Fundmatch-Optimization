#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <vector>
#include <random>
#include <algorithm>

#include "fundmatch.h"

using namespace std;

static const int START_DATE_OFFSET = 0;
static const int TENOR_OFFSET = 1;
static const int AMOUNT_OFFSET = 2;

InputData g_input;

Vector::Vector()
    : dimensions(0), coords(nullptr), constraintViolation(FLT_MAX), fitness(FLT_MAX)
{
}

Vector::Vector(int dimCount)
    : dimensions(dimCount), constraintViolation(FLT_MAX), fitness(FLT_MAX)
{
    if(this->dimensions > 0)
        this->coords = new float[this->dimensions];
    else
        this->coords = nullptr;
}

Vector::Vector(const Vector& other)
    : dimensions(other.dimensions), constraintViolation(other.constraintViolation),
        fitness(other.fitness)
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

void Vector::processPositionUpdate(int allocCount, AllocationPointer* allocations)
{
    this->constraintViolation = measureConstraintViolation(*this, allocCount, allocations);
    if(this->constraintViolation == 0.0f)
        this->fitness = computeFitness(*this, allocCount, allocations);
    else
        this->fitness = FLT_MAX;
}

Vector& Vector::operator =(const Vector& other)
{
    // NOTE: We only need to delete/reallocate memory if the size has changed
    if(this->dimensions != other.dimensions)
    {
        if(this->coords)
            delete[] this->coords;
        this->coords = new float[other.dimensions];
        this->dimensions = other.dimensions;
    }
    memcpy(this->coords, other.coords, this->dimensions*sizeof(float));
    this->fitness = other.fitness;
    this->constraintViolation = other.constraintViolation;

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
        SourceInfo& src = g_input.sources[this->sourceIndex];
        result = (float)maxAllocationTenor(src, req);
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
        result = min(result, (float)source.amount);
    }
    else
    {
        BalancePoolInfo& pool = g_input.balancePools[this->balancePoolIndex];
        result = min(result, (float)pool.amount);
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

float measureConstraintViolation(Vector& position, int allocationCount, AllocationPointer* allocations)
{
    float result = 0.0f;
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
            if(allocStart < sourceStart)
                result += (sourceStart - allocStart) * allocAmount;
            if((allocStart > sourceEnd) || (allocEnd > sourceEnd))
                result += (allocEnd - sourceEnd) * allocAmount;
            if(allocAmount > sourceAmount)
                result += allocTenor * (allocAmount*sourceAmount);
        }
        else
        {
            assert(alloc.balancePoolIndex >= 0);
            BalancePoolInfo& balancePool = g_input.balancePools[alloc.balancePoolIndex];
            // TODO: Other properties, if we have any
            float balanceAmount = (float)balancePool.amount;
            if(allocAmount > balanceAmount)
                result += allocTenor * (allocAmount - balanceAmount);
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
            float allocTenor = alloc->getTenor(position);
            float allocAmount = alloc->getAmount(position);
            if(alloc->sourceIndex >= 0)
            {
                sourceValueRemaining[alloc->sourceIndex] -= allocAmount;
                if(sourceValueRemaining[alloc->sourceIndex] < 0.0f)
                    result += allocTenor * (-1.0f * sourceValueRemaining[alloc->sourceIndex]);
            }
            else
            {
                assert(alloc->balancePoolIndex >= 0);
                balancePoolValueRemaining[alloc->balancePoolIndex] -= allocAmount;
                if(balancePoolValueRemaining[alloc->balancePoolIndex] < 0.0f)
                    result += allocTenor * (-1.0f * balancePoolValueRemaining[alloc->balancePoolIndex]);
            }
        }
    }
    delete[] balancePoolValueRemaining;
    delete[] sourceValueRemaining;

    assert(result >= 0.0f);
    return result;
}

bool isPositionBetter(Vector& newPosition, Vector& testPosition,
                      int allocationCount, AllocationPointer* allocations)
{
    float newViolation = newPosition.constraintViolation;
    float testViolation = testPosition.constraintViolation;
    assert(newViolation >= 0.0f);
    assert(testViolation >= 0.0f);

    if((newViolation == 0.0f) && (testViolation == 0.0f))
    {
        float newFitness = newPosition.fitness;
        float testFitness = testPosition.fitness;
        if(newFitness < testFitness)
            return true;
        return false;
    }
    else
    {
        // NOTE: If at least one of them is not 0, then it's positive, and we either want the one
        //       that is feasible (and therefore less than the non-feasible one), or we want the
        //       one that is closest to being feasible (and therefore has the smaller violation)
        //
        return (newViolation < testViolation);
    }
}

bool isFeasible(Vector& position, int allocationCount, AllocationPointer* allocations)
{
    float violation = measureConstraintViolation(position, allocationCount, allocations);
    return (violation == 0.0f);
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
        {
            RequirementInfo& req = g_input.requirements[g_input.requirementsByStart[reqStartIndex]];
            nextReqStartTime = (float)req.startDate;
        }
        if(reqEndIndex < g_input.requirements.size())
        {
            RequirementInfo& req = g_input.requirements[g_input.requirementsByEnd[reqEndIndex]];
            nextReqEndTime = (float)req.startDate + (float)req.tenor;
        }

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
