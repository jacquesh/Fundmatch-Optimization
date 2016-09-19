#include <stdio.h>
#include <time.h>
#include <math.h>
#include <assert.h>

#include <random>
#include <vector>
#include <algorithm>

#include "pso.h"
#include "fundmatch.h"

using namespace std;

static InputData g_input;

static const int START_DATE_OFFSET = 0;
static const int TENOR_OFFSET = 1;
static const int AMOUNT_OFFSET = 2;

float PSOAllocationPointer::getStartDate(Vector& data)
{
    return data[this->allocStartDimension + START_DATE_OFFSET];
}
float PSOAllocationPointer::getTenor(Vector& data)
{
    return data[this->allocStartDimension + TENOR_OFFSET];
}
float PSOAllocationPointer::getAmount(Vector& data)
{
    return data[this->allocStartDimension + AMOUNT_OFFSET];
}
float PSOAllocationPointer::getEndDate(Vector& data)
{
    return getStartDate(data) + getTenor(data);
}


float computeFitness(Vector position, int allocationCount, PSOAllocationPointer* allocations)
{
    // TODO: Should we be passing these by ref into the vector? I don't think theres any reason to
    //       other than avoid the copying. But we may well want to do that
    vector<PSOAllocationPointer*> allocationsByStart(allocationCount);
    for(int i=0; i<allocationCount; i++)
        allocationsByStart.push_back(&allocations[i]);
    vector<PSOAllocationPointer*> allocationsByEnd(allocationsByStart);

    auto startDateComparison = [&position](PSOAllocationPointer* a, PSOAllocationPointer* b)
    {
        return a->getStartDate(position) < b->getStartDate(position);
    };
    auto endDateComparison = [&position](PSOAllocationPointer* a, PSOAllocationPointer* b)
    {
        return a->getEndDate(position) < b->getEndDate(position);
    };
    sort(allocationsByStart.begin(), allocationsByStart.end(), startDateComparison);
    sort(allocationsByEnd.begin(), allocationsByEnd.end(), endDateComparison);

    float* requirementValueRemaining = new float[g_input.requirementCount];
    for(int i=0; i<g_input.requirementCount; i++)
    {
        requirementValueRemaining[i] = (float)g_input.requirements[i].amount;
    }

    float result = 0.0f;
    float currentTime = 0.0f;
    int allocStartIndex = 0;
    int allocEndIndex = 0;
    vector<PSOAllocationPointer*> activeAllocations;
    while((allocStartIndex < allocationCount) || (allocEndIndex < allocationCount))
    {
        float nextStartTime = -1.0f;
        float nextEndTime = -1.0f;
        if(allocStartIndex < allocationCount)
            nextStartTime = allocationsByStart[allocStartIndex]->getStartDate(position);
        if(allocEndIndex < allocationCount)
            nextEndTime = allocationsByEnd[allocEndIndex]->getEndDate(position);

        float previousTime = currentTime;
        currentTime = min(nextStartTime, nextEndTime);
        float timeOffset = currentTime - previousTime;

        for(int j=0; j<activeAllocations.size(); j++)
        {
            PSOAllocationPointer* activeAlloc = activeAllocations[j];
            float interestRate = BALANCEPOOL_INTEREST_RATE;
            if(activeAlloc->sourceIndex != -1)
                interestRate = g_input.sources[activeAlloc->sourceIndex].interestRate;

            result += timeOffset * activeAlloc->getAmount(position) * interestRate;
        }

        if(nextEndTime <= nextStartTime)
        {
            PSOAllocationPointer* alloc = nullptr;
            for(auto iter = activeAllocations.begin(); iter != activeAllocations.end(); iter++)
            {
                if(*iter == allocationsByEnd[allocEndIndex])
                {
                    alloc = *iter;
                    activeAllocations.erase(iter);
                    break;
                }
            }
            allocEndIndex++;
            assert(alloc != nullptr);
            requirementValueRemaining[alloc->requirementIndex] += alloc->getAmount(position);
        }
        else
        {
            // Process the start event
            PSOAllocationPointer* alloc = allocationsByStart[allocStartIndex];
            activeAllocations.push_back(alloc);
            allocStartIndex++;
            requirementValueRemaining[alloc->requirementIndex] -= alloc->getAmount(position);
        }

        for(int reqID=0; reqID<g_input.requirementCount; reqID++)
        {
            result += timeOffset * requirementValueRemaining[reqID] * RCF_INTEREST_RATE;
        }
    }

    delete[] requirementValueRemaining;

    return result;
}

Vector optimizeSwarm(Particle* swarm, int dimensionCount,
                  int allocCount, PSOAllocationPointer* allocations)
{
    random_device randDevice;
    mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);

    Vector bestLoc = swarm[0].position; // TODO: Maybe actually compute this correctly here
    float bestFitness = computeFitness(bestLoc, allocCount, allocations);
    for(int iteration=0; iteration<MAX_ITERATIONS; iteration++)
    {
        // NOTE: We need to do this in a separate loop here first to ensure that all particles
        //       can compare with the correct best at the start of the current iteration
        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            float fitness = computeFitness(swarm[particleIndex].position, allocCount, allocations);
            if(fitness < bestFitness)
            {
                bestFitness = fitness;
                bestLoc = swarm[particleIndex].position;
            }

            if(fitness < swarm[particleIndex].bestSeenFitness)
            {
                swarm[particleIndex].bestSeenFitness = fitness;
                swarm[particleIndex].bestSeenLoc = swarm[particleIndex].position;
            }
        }

        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            Particle& currentParticle = swarm[particleIndex];
            Vector localBestLoc = currentParticle.bestSeenLoc;
            Vector globalBestLoc = bestLoc;

            Vector neighbourBestLoc = currentParticle.neighbours[0]->bestSeenLoc;
            float neighbourBestFitness = computeFitness(neighbourBestLoc, allocCount, allocations);
            for(int neighbourIndex=1; neighbourIndex<NEIGHBOUR_COUNT; neighbourIndex++)
            {
                Vector tempNeighbourBestLoc = currentParticle.neighbours[neighbourIndex]->bestSeenLoc;
                float tempNeighbourBestFitness = computeFitness(tempNeighbourBestLoc, allocCount, allocations);
                if(tempNeighbourBestFitness < neighbourBestFitness)
                {
                    neighbourBestFitness = tempNeighbourBestFitness;
                    neighbourBestLoc = tempNeighbourBestLoc;
                }
            }

            for(int dim=0; dim<dimensionCount; dim++)
            {
                float selfFactor = SELF_BEST_FACTOR * uniformf(rng);
                float neighbourFactor = NEIGHBOUR_BEST_FACTOR * uniformf(rng);
                float globalFactor = GLOBAL_BEST_FACTOR * uniformf(rng);

                currentParticle.velocity.coords[dim] =
                    (VELOCITY_UPDATE_FACTOR * currentParticle.velocity[dim]) +
                    (selfFactor * (localBestLoc[dim] - currentParticle.position[dim])) +
                    (neighbourFactor * (neighbourBestLoc[dim] - currentParticle.position[dim])) +
                    (globalFactor * (globalBestLoc[dim] - currentParticle.position[dim]));
            }
        }

        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            for(int dim=0; dim<dimensionCount; dim++)
            {
                swarm[particleIndex].position.coords[dim] +=
                    TIMESTEP * swarm[particleIndex].velocity[dim];
            }
        }
    }

    printf("Final best solution was %f\n", bestFitness);
    return bestLoc;
}

void computeAllocations(InputData inputData, int allocationCount, AllocationInfo* allocations)
{
    g_input = inputData;

    // Create the PSO-specific allocation data
    PSOAllocationPointer* swarmAllocations = new PSOAllocationPointer[allocationCount];
    for(int i=0; i<allocationCount; i++)
    {
        swarmAllocations[i].sourceIndex = allocations[i].sourceIndex;
        swarmAllocations[i].requirementIndex = allocations[i].requirementIndex;
        swarmAllocations[i].balanceIndex = allocations[i].balanceIndex;
        swarmAllocations[i].allocStartDimension = i*3;
    }

    // Create the swarm
    int dimensionCount = allocationCount*3;
    Particle* swarm = new Particle[SWARM_SIZE];
    for(int i=0; i<SWARM_SIZE; i++)
    {
        swarm[i].position.coords = new float[dimensionCount];
        swarm[i].velocity.coords = new float[dimensionCount];
    }

    // Initialize the swarm
    random_device randDevice;
    mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);
    uniform_int_distribution<int> uniformi(0, NEIGHBOUR_COUNT);
    for(int i=0; i<SWARM_SIZE; i++)
    {
        for(int allocID=0; allocID<allocationCount; allocID++)
        {
            int allocSourceIndex = swarmAllocations[allocID].sourceIndex;
            int allocBalanceIndex = swarmAllocations[allocID].balanceIndex;
            assert(((allocSourceIndex == -1) && (allocBalanceIndex >= 0)) ||
                    ((allocSourceIndex >= 0) && (allocBalanceIndex == -1)));

            float maxStartDate = -1; // TODO
            float maxTenor = -1;
            float maxAmount = -1;
            if(allocSourceIndex >= 0)
            {
                SourceInfo& allocSource = g_input.sources[allocSourceIndex];
                maxStartDate = -1; // TODO
                maxTenor = (float)allocSource.tenor;
                maxAmount = (float)allocSource.amount;
            }
            else // NOTE: By the assert above, we necessarily have a balance pool here
            {
                BalancePoolInfo& allocBalance = g_input.balancePools[allocBalanceIndex];
                maxStartDate = -1; // TODO
                maxTenor = 1; // TODO
                maxAmount = (float)allocBalance.amount;
            }

            int allocStartDim = swarmAllocations[allocID].allocStartDimension;
            int startDateDim = allocStartDim + START_DATE_OFFSET;
            int tenorDim = allocStartDim + TENOR_OFFSET;
            int amountDim = allocStartDim + AMOUNT_OFFSET;
            swarm[i].position.coords[startDateDim] = uniformf(rng); // TODO
            swarm[i].velocity.coords[startDateDim] = uniformf(rng); // TODO

            swarm[i].position.coords[tenorDim] = uniformf(rng) * maxTenor;
            swarm[i].velocity.coords[tenorDim] = uniformf(rng) * sqrtf(maxTenor);

            swarm[i].position.coords[amountDim] = uniformf(rng) * maxAmount;
            swarm[i].velocity.coords[amountDim] = uniformf(rng) * sqrtf(maxAmount);
        }

        swarm[i].bestSeenLoc = swarm[i].position;
        swarm[i].bestSeenFitness = computeFitness(swarm[i].position, allocationCount, swarmAllocations);

        swarm[i].neighbours[0] = &swarm[i];
        for(int neighbourIndex=1; neighbourIndex<NEIGHBOUR_COUNT; neighbourIndex++)
        {
            swarm[i].neighbours[neighbourIndex] = &swarm[uniformi(rng)];
        }
    }

    // Run PSO using our new swarm
    Vector bestSolution = optimizeSwarm(swarm, dimensionCount, allocationCount, swarmAllocations);

    // Convert our swarm info back into allocation data as expected for output
    for(int i=0; i<allocationCount; i++)
    {
        int allocStartDate = (int)bestSolution[swarmAllocations[i].allocStartDimension + 0];
        int allocTenor = (int)bestSolution[swarmAllocations[i].allocStartDimension + 1];
        int allocAmount = (int)bestSolution[swarmAllocations[i].allocStartDimension + 2];
        if((allocTenor <= 0) || (allocAmount <= 0))
            continue;

        allocations[i].startDate = allocStartDate;
        allocations[i].tenor = allocTenor;
        allocations[i].amount = allocAmount;
    }

    // Cleanup
    for(int i=0; i<SWARM_SIZE; i++)
    {
        delete[] swarm[i].position.coords;
        delete[] swarm[i].velocity.coords;
    }
    delete[] swarm;
    delete[] swarmAllocations;
}
