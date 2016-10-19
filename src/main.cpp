#include <stdio.h>
#include <assert.h>
#include <vector>
#include <algorithm>

#include "fundmatch.h"
#include "platform.h"

using namespace std;

bool isAllocationPairValid(SourceInfo& source, RequirementInfo& req)
{
    if(source.taxClass != req.taxClass)
        return false;

    int sourceStart = source.startDate;
    int requireStart = req.startDate;
    int sourceEnd = source.startDate + source.tenor;
    int requireEnd = req.startDate + req.tenor;

    int validStart = max(sourceStart, requireStart);
    int validEnd = min(sourceEnd, requireEnd);

    // NOTE: We need there to be an overlap of at least 1 month (0-length loans are irrelevant)
    if(validEnd >= validStart+1)
        return true;

    return false;
}

int min(int a, int b)
{
    if(a < b)
        return a;
    return b;
}

int main()
{
    int64_t clockFrequency = getClockFrequency();
    int64_t loadStartTime = getClockValue();

    loadSourceData("data/DSt_sources.csv", g_input);
    printf("Loaded %zd sources\n", g_input.sources.size());

    loadBalancePoolData("data/DSt_balancepools.csv", g_input);
    printf("Loaded %zd balance pools\n", g_input.balancePools.size());

    loadRequirementData("data/DSt_requirements.csv", g_input);
    printf("Loaded %zd requirements\n", g_input.requirements.size());

#if 0
    AllocationInfo* manualAllocations;
    int manualAllocationCount;
    loadAllocationData("data/DS1_allocations.csv", &manualAllocations, manualAllocationCount);
    printf("Loaded %d allocations\n", manualAllocationCount);
#endif

    // Count the number of valid allocations, so we know how many to construct below
    // NOTE: First allocations are from balance pools in our valid allocation list
    int validAllocationCount = g_input.balancePools.size() * g_input.requirements.size();
    for(int reqID=0; reqID<g_input.requirements.size(); reqID++)
    {
        for(int sourceID=0; sourceID<g_input.sources.size(); sourceID++)
        {
            if(isAllocationPairValid(g_input.sources[sourceID], g_input.requirements[reqID]))
            {
                validAllocationCount++;
            }
        }
    }

    // Create allocations and set the source/requirement/balancePool that they correspond to
    AllocationPointer* allocations = new AllocationPointer[validAllocationCount];
    memset(allocations, 0, validAllocationCount*sizeof(AllocationPointer));
    for(int i=0; i<validAllocationCount; i++)
    {
        allocations[i].allocStartDimension = i*DIMENSIONS_PER_ALLOCATION;
    }

    int currentAllocIndex = 0;
    for(int reqID=0; reqID<g_input.requirements.size(); reqID++)
    {
        for(int balanceID=0; balanceID<g_input.balancePools.size(); balanceID++)
        {
            allocations[currentAllocIndex].sourceIndex = -1;
            allocations[currentAllocIndex].requirementIndex = reqID;
            allocations[currentAllocIndex].balanceIndex = balanceID;
            currentAllocIndex++;
        }
    }
    for(int reqID=0; reqID<g_input.requirements.size(); reqID++)
    {
        for(int sourceID=0; sourceID<g_input.sources.size(); sourceID++)
        {
            if(isAllocationPairValid(g_input.sources[sourceID], g_input.requirements[reqID]))
            {
                allocations[currentAllocIndex].sourceIndex = sourceID;
                allocations[currentAllocIndex].requirementIndex = reqID;
                allocations[currentAllocIndex].balanceIndex = -1;
                currentAllocIndex++;
            }
        }
    }

    // Create the sorted requirements lists and sort them
    for(int i=0; i<g_input.requirements.size(); i++)
    {
        g_input.requirementsByStart.push_back(i);
        g_input.requirementsByEnd.push_back(i);
    }

    auto reqStartDateComparison = [](int reqIndexA, int reqIndexB)
    {
        int aStart = g_input.requirements[reqIndexA].startDate;
        int bStart = g_input.requirements[reqIndexB].startDate;
        return aStart < bStart;
    };
    auto reqEndDateComparison = [](int reqIndexA, int reqIndexB)
    {
        int aEnd = g_input.requirements[reqIndexA].startDate +
                    g_input.requirements[reqIndexA].tenor;
        int bEnd = g_input.requirements[reqIndexB].startDate +
                    g_input.requirements[reqIndexB].tenor;
        return aEnd < bEnd;
    };
    sort(g_input.requirementsByStart.begin(), g_input.requirementsByStart.end(),
            reqStartDateComparison);
    sort(g_input.requirementsByEnd.begin(), g_input.requirementsByEnd.end(),
            reqEndDateComparison);

    int64_t loadEndTime = getClockValue();
    float loadSeconds = (float)(loadEndTime-loadStartTime)/(float)clockFrequency;
    printf("Input data loaded in %.2fs\n", loadSeconds);

    printf("Computing values for %d allocations...\n", validAllocationCount);
    Vector solution = computeAllocations(validAllocationCount, allocations);
    assert(isFeasible(solution, validAllocationCount, allocations));
    float solutionFitness = computeFitness(solution, validAllocationCount, allocations);

    int64_t computeEndTime = getClockValue();
    float computeSeconds = (float)(computeEndTime - loadEndTime)/(float)clockFrequency;
    printf("Optimization completed in %.2fs - final fitness was %f\n",
            computeSeconds, solutionFitness);

    writeOutputData(g_input, validAllocationCount, allocations, solution, "output.json");
}
