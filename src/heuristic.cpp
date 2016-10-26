#include <stdio.h>
#include <math.h>
#include <assert.h>

#include <random>
#include <algorithm>

#include "fundmatch.h"

using namespace std;

Vector computeAllocations(int allocationCount, AllocationPointer* allocations)
{
    // TODO: Balance pools
    int* requirementSources = new int[g_input.requirements.size()];
    for(int i=0; i<g_input.requirements.size(); i++)
        requirementSources[i] = -1;

    bool* sourcesUsed = new bool[g_input.sources.size()];
    for(int i=0; i<g_input.sources.size(); i++)
        sourcesUsed[i] = false;

    int* requirementBalancePools = new int[g_input.requirements.size()];
    for(int i=0; i<g_input.requirements.size(); i++)
        requirementBalancePools[i] = -1;

    int* balanceRemaining = new int[g_input.balancePools.size()];
    for(int i=0; i<g_input.balancePools.size(); i++)
        balanceRemaining[i] = g_input.balancePools[i].amount;

    for(int reqIndex=0; reqIndex<g_input.requirements.size(); reqIndex++)
    {
        RequirementInfo& req = g_input.requirements[reqIndex];

        // Find the source that best matches this requirement
        int bestSrcIndex = -1;
        for(int srcIndex=0; srcIndex<g_input.sources.size(); srcIndex++)
        {
            SourceInfo& src = g_input.sources[srcIndex];
            if(sourcesUsed[srcIndex] || (src.taxClass != req.taxClass) ||
                    (maxAllocationTenor(src,req) < 1))
                continue;

            if(bestSrcIndex == -1) // The first one is obviously the best so far
            {
                bestSrcIndex = srcIndex;
                continue;
            }
            SourceInfo& bestSrc = g_input.sources[bestSrcIndex];

            // Check how much better the new source would be in terms of:
            // Tenor (can you allocate for the entire requirement duration? Are we closer to that?)
            bool tenorImprovement = false;
            int maxTenor = maxAllocationTenor(src, req);
            int bestMaxTenor = maxAllocationTenor(bestSrc, req);
            if(((maxTenor < req.tenor) && (maxTenor > bestMaxTenor)) ||
                ((maxTenor >= req.tenor) && (maxTenor < bestMaxTenor)))
                tenorImprovement = true;

            // Amount (can you allocate the full value? Are we closer to that?)
            bool amountImprovement = false;
            if(((src.amount < req.amount) && (src.amount > bestSrc.amount)) ||
                    ((src.amount >= req.amount) && (src.amount < bestSrc.amount)))
                amountImprovement = true;

            if(tenorImprovement && amountImprovement)
            {
                bestSrcIndex = srcIndex;
            }
        }

        // Find the balance pool that best matches this requirement:
        int bestPoolIndex = -1;
        for(int poolIndex=0; poolIndex<g_input.balancePools.size(); poolIndex++)
        {
            if(bestPoolIndex == -1)
            {
                bestPoolIndex = poolIndex;
                continue;
            }

            bool isFullySatisfied = (balanceRemaining[poolIndex] >= req.amount);
            bool wasFullySatisfied = (balanceRemaining[bestPoolIndex] < req.amount);
            bool nowSmaller = (balanceRemaining[poolIndex] < balanceRemaining[bestPoolIndex]);

            if((isFullySatisfied && !wasFullySatisfied) ||
                    (isFullySatisfied && wasFullySatisfied && nowSmaller) ||
                    (!isFullySatisfied && !wasFullySatisfied && !nowSmaller))
            {
                bestPoolIndex = poolIndex;
            }
        }

        // NOTE: For this heuristic we only support using the balance pool if we can fully satisfy
        //       the requirement with it. This is to ensure that later when we set the values on the
        //       corresponding allocation, we will get a correct/feasible allocation amount.
        if(balanceRemaining[bestPoolIndex] < req.amount)
            bestPoolIndex = -1;

#if 1
        if((bestPoolIndex >= 0) &&
                (bestSrcIndex == -1))
#if 0
                ((bestSrcIndex == -1) ||
                (g_input.sources[bestSrcIndex].amount < (int)(req.amount*0.9f)) ||
                (g_input.sources[bestSrcIndex].tenor < (int)(req.tenor*0.9f))))
#endif
        {
            requirementBalancePools[reqIndex] = bestPoolIndex;

            // NOTE: It doesn't matter if this goes negative
            balanceRemaining[bestPoolIndex] -= req.amount;
        }
        else if(bestSrcIndex >= 0)
#else
        if(bestSrcIndex >= 0)
#endif
        {
            requirementSources[reqIndex] = bestSrcIndex;
            sourcesUsed[bestSrcIndex] = true;
        }
    }

    int dimensionCount = allocationCount * DIMENSIONS_PER_ALLOCATION;
    Vector solution(dimensionCount);
    for(int allocIndex=0; allocIndex<allocationCount; allocIndex++)
    {
        AllocationPointer& alloc = allocations[allocIndex];
        int reqIndex = allocations[allocIndex].requirementIndex;
        int srcIndex = allocations[allocIndex].sourceIndex;
        int poolIndex = allocations[allocIndex].balancePoolIndex;
        
        if((requirementSources[reqIndex] >= 0) && (requirementSources[reqIndex] == srcIndex))
        {
            SourceInfo& src = g_input.sources[srcIndex];
            RequirementInfo& req = g_input.requirements[reqIndex];
            int allocStart = max(src.startDate, req.startDate);
            int allocTenor = maxAllocationTenor(src, req);
            int allocAmount = min(src.amount, req.amount);
            assert(allocStart > 0);
            assert(allocTenor > 0);
            assert(allocAmount > 0);

            alloc.setStartDate(solution, (float)allocStart);
            alloc.setTenor(solution, (float)allocTenor);
            alloc.setAmount(solution, (float)allocAmount);
        }
        else if((requirementBalancePools[reqIndex] >= 0) &&
                (requirementBalancePools[reqIndex] == poolIndex))
        {
            RequirementInfo& req = g_input.requirements[reqIndex];

            alloc.setStartDate(solution, (float)req.startDate);
            alloc.setTenor(solution, (float)req.tenor);
            alloc.setAmount(solution, (float)req.amount); // NOTE: As above, we need the full amount
        }
        else
        {
            // Use the RCF
            alloc.setStartDate(solution, 0);
            alloc.setTenor(solution, 0);
            alloc.setAmount(solution, 0);
        }
    }

    delete[] requirementSources;
    delete[] sourcesUsed;
    delete[] requirementBalancePools;
    delete[] balanceRemaining;

    return solution;
}
