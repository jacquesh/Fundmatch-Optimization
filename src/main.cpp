#include <stdio.h>
#include <assert.h>

#include "fundmatch.h"
#include "platform.h"

bool isAllocationPairValid(SourceInfo& source, RequirementInfo& req)
{
    if(source.taxClass != req.taxClass)
        return false;

    int sourceStart = source.startDate;
    int requireStart = req.startDate;
    int sourceEnd = source.startDate + source.tenor;
    int requireEnd = req.startDate + req.tenor;

    if((sourceEnd < requireStart) || (sourceStart > requireEnd))
        return false;

    return true;
}


int main()
{
    int64_t clockFrequency = getClockFrequency();
    int64_t loadStartTime = getClockValue();

    InputData input = {};
    loadSourceData("data/DS1_sources.csv", input);
    printf("Loaded %d sources\n", input.sourceCount);

    loadBalancePoolData("data/DS1_balancepools.csv", input);
    printf("Loaded %d balance pools\n", input.balancePoolCount);

    loadRequirementData("data/DS1_requirements.csv", input);
    printf("Loaded %d requirements\n", input.requirementCount);

    AllocationInfo* manualAllocations;
    int manualAllocationCount;
    loadAllocationData("data/DS1_allocations.csv", &manualAllocations, manualAllocationCount);
    printf("Loaded %d allocations\n", manualAllocationCount);

    // NOTE: First allocations are from balance pools in our valid allocation list
    int validAllocationCount = input.balancePoolCount * input.requirementCount;
    for(int reqID=0; reqID<input.requirementCount; reqID++)
    {
        for(int sourceID=0; sourceID<input.sourceCount; sourceID++)
        {
            if(isAllocationPairValid(input.sources[sourceID], input.requirements[reqID]))
            {
                validAllocationCount++;
            }
        }
    }

    AllocationInfo* allocations = new AllocationInfo[validAllocationCount];
    memset(allocations, 0, validAllocationCount*sizeof(AllocationInfo));

    int currentAllocIndex = 0;
    for(int reqID=0; reqID<input.requirementCount; reqID++)
    {
        for(int balanceID=0; balanceID<input.balancePoolCount; balanceID++)
        {
            allocations[currentAllocIndex].sourceIndex = -1;
            allocations[currentAllocIndex].requirementIndex = reqID;
            allocations[currentAllocIndex].balanceIndex = balanceID;
            currentAllocIndex++;
        }
    }
    for(int reqID=0; reqID<input.requirementCount; reqID++)
    {
        for(int sourceID=0; sourceID<input.sourceCount; sourceID++)
        {
            if(isAllocationPairValid(input.sources[sourceID], input.requirements[reqID]))
            {
                allocations[currentAllocIndex].sourceIndex = sourceID;
                allocations[currentAllocIndex].requirementIndex = reqID;
                allocations[currentAllocIndex].balanceIndex = -1;
                currentAllocIndex++;
            }
        }
    }

    int64_t loadEndTime = getClockValue();
    float loadSeconds = (float)(loadEndTime-loadStartTime)/(float)clockFrequency;
    printf("Input data loaded in %.2fs\n", loadSeconds);

    printf("Computing values for %d allocations...\n", validAllocationCount);
    computeAllocations(input, validAllocationCount, allocations);

    int64_t computeEndTime = getClockValue();
    float computeSeconds = (float)(computeEndTime - loadEndTime)/(float)clockFrequency;
    printf("Optimization completed in %.2fs\n", computeSeconds);

    writeOutputData(input, validAllocationCount, allocations, "output.json");
}
