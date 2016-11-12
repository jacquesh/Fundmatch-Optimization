#include <stdio.h>
#include <math.h>
#include <assert.h>

#include <random>
#include <algorithm>

#include "fundmatch.h"

using namespace std;

Vector computeAllocations(int allocationCount, AllocationPointer* allocations)
{

    int dimensionCount = allocationCount * DIMENSIONS_PER_ALLOCATION;
    Vector solution(dimensionCount);
    for(int allocIndex=0; allocIndex<allocationCount; allocIndex++)
    {
        AllocationPointer& alloc = allocations[allocIndex];
        int reqIndex = allocations[allocIndex].requirementIndex;
        int srcIndex = allocations[allocIndex].sourceIndex;
        int poolIndex = allocations[allocIndex].balancePoolIndex;

        // Use the RCF
        alloc.setStartDate(solution, 0.0f);
        alloc.setTenor(solution, 0.0f);
        alloc.setAmount(solution, 0.0f);
    }

    assert(isFeasible(solution, allocationCount, allocations));
    float fitness = computeFitness(solution, allocationCount, allocations);

    return solution;
}
