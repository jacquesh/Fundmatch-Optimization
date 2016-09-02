#include <stdio.h>

#include "fundmatch.h"
bool isAllocationPairValid(SourceInfo& source, RequirementInfo& req)
{
    if(source.taxClass != req.taxClass)
        return false;

    time_t sourceStart = mktime(&source.startDate);
    time_t requireStart = mktime(&req.startDate);
    tm sourceEndTm = source.startDate;
    sourceEndTm.tm_mon += source.tenor;
    tm reqEndTm = req.startDate;
    reqEndTm.tm_mon += req.tenor;
    time_t sourceEnd = mktime(&sourceEndTm);
    time_t requireEnd = mktime(&reqEndTm);

    if((sourceEnd < requireStart) || (sourceStart > requireEnd))
        return false;

    return true;
}


int main()
{
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
    // NOTE: day-of-the-month cannot be 0 (starts at 1) so we just set everything to 1 so we
    //       can forget about it from now on unless we actually set the value
    for(int i=0; i<validAllocationCount; i++)
        allocations[i].startDate.tm_mday = 1;

    int currentAllocIndex = 0;
    for(int reqID=0; reqID<input.requirementCount; reqID++)
    {
        for(int balanceID=0; balanceID<input.balancePoolCount; balanceID++)
        {
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
                currentAllocIndex++;
            }
        }
    }
    printf("Computing values for %d allocations...\n", validAllocationCount);
    computeAllocations(input, validAllocationCount, allocations);
    printf("Complete, writing to file...\n");

    writeOutputData(input, validAllocationCount, allocations, "output.json");
}
