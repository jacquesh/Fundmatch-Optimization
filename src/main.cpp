#include <stdio.h>
#include <assert.h>
#include <vector>
#include <algorithm>

#include "fundmatch.h"
#include "platform.h"
#include "dataio.h"

using namespace std;

const int MAX_FILEPATH_LENGTH = 512;

int main(int argc, char** argv)
{
    int64_t clockFrequency = getClockFrequency();
    int64_t loadStartTime = getClockValue();

    const char* dataName = (argc > 1) ? argv[1] : "DS1";

    char sourceFilename[MAX_FILEPATH_LENGTH];
    snprintf(sourceFilename, MAX_FILEPATH_LENGTH, "data/%s_sources.csv", dataName);
    if(!loadSourceData(sourceFilename, g_input))
    {
        printf("Error: Unable to load source data from %s\n", sourceFilename);
        return -1;
    }
    printf("Loaded %zd sources\n", g_input.sources.size());

    char balancePoolFilename[MAX_FILEPATH_LENGTH];
    snprintf(balancePoolFilename, MAX_FILEPATH_LENGTH, "data/%s_balancepools.csv", dataName);
    if(!loadBalancePoolData(balancePoolFilename, g_input))
    {
        printf("Error: Unable to load balance pool data from %s\n", balancePoolFilename);
        return -1;
    }
    printf("Loaded %zd balance pools\n", g_input.balancePools.size());

    char requirementFilename[MAX_FILEPATH_LENGTH];
    snprintf(requirementFilename, MAX_FILEPATH_LENGTH, "data/%s_requirements.csv", dataName);
    if(!loadRequirementData(requirementFilename, g_input))
    {
        printf("Error: Unable to load requirement data from %s\n", requirementFilename);
        return -1;
    }
    printf("Loaded %zd requirements\n", g_input.requirements.size());

    // Count the number of valid allocations, so we know how many to construct below
    // NOTE: First allocations are from balance pools in our valid allocation list
    int validAllocationCount = (int)(g_input.balancePools.size() * g_input.requirements.size());
    for(int reqID=0; reqID<g_input.requirements.size(); reqID++)
    {
        for(int sourceID=0; sourceID<g_input.sources.size(); sourceID++)
        {
            SourceInfo& src = g_input.sources[sourceID];
            RequirementInfo& req = g_input.requirements[reqID];
            if((src.taxClass == req.taxClass) && (maxAllocationTenor(src, req) >= 1))
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
            allocations[currentAllocIndex].balancePoolIndex = balanceID;
            currentAllocIndex++;
        }
    }
    for(int reqID=0; reqID<g_input.requirements.size(); reqID++)
    {
        for(int sourceID=0; sourceID<g_input.sources.size(); sourceID++)
        {
            RequirementInfo& req = g_input.requirements[reqID];
            SourceInfo& src = g_input.sources[sourceID];
            if((src.taxClass == req.taxClass) && (maxAllocationTenor(src, req) >= 1))
            {
                allocations[currentAllocIndex].sourceIndex = sourceID;
                allocations[currentAllocIndex].requirementIndex = reqID;
                allocations[currentAllocIndex].balancePoolIndex = -1;
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
    float solutionFitness = -1.0f;
    if(isFeasible(solution, validAllocationCount, allocations))
        solutionFitness = computeFitness(solution, validAllocationCount, allocations);

    vector<AllocationPointer> manAllocs;
    char allocationFilename[MAX_FILEPATH_LENGTH];
    snprintf(allocationFilename, MAX_FILEPATH_LENGTH, "data/%s_allocations.csv", dataName);
    Vector manualSolution = loadAllocationData(allocationFilename, manAllocs);
    if(manualSolution.dimensions > 0)
    {
        //assert(isFeasible(manualSolution, validAllocationCount, allocations));
        float manualFitness = computeFitness(manualSolution, (int)manAllocs.size(), &manAllocs[0]);
        printf("Manual solution has %zd allocations and costs %f\n",
                manAllocs.size(), manualFitness);
    }

    int64_t computeEndTime = getClockValue();
    float computeSeconds = (float)(computeEndTime - loadEndTime)/(float)clockFrequency;

    int generatedAllocs = writeOutputData(g_input, validAllocationCount, allocations,
                                          solution, "output.json");
    printf("Optimization completed in %.2fs - final fitness was %.2f from %d allocations\n",
            computeSeconds, solutionFitness, generatedAllocs);
}
