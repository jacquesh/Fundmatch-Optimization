#include <stdio.h>

#include "fundmatch.h"

int main()
{
    InputData input = {};
    loadSourceData("data/DS1_sources.csv", input);
    printf("Loaded %d sources\n", input.sourceCount);

    loadRequirementData("data/DS1_requirements.csv", input);
    printf("Loaded %d requirements\n", input.requirementCount);

    AllocationInfo* allocations;
    int allocationCount = computeAllocations(input, &allocations);
    printf("Computed %d allocations\n", allocationCount);

    writeOutputData(input, allocationCount, allocations, "output.json");
}
