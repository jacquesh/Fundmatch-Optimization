#include <stdio.h>
#include <time.h>
#include <math.h>

#include "rand.h"

#include "pso.h"
#include "fundmatch.h"

static InputData g_input;

float computeFitness(Vector position, int allocationCount, PSOAllocationPointer* allocations)
{
    // TODO: This is just a first approximation to how much money the source has available at the time
    float* sourceValRemaining = new float[g_input.sourceCount];
    for(int i=0; i<g_input.sourceCount; i++)
    {
        sourceValRemaining[i] = (float)g_input.sources[i].amount;
    }

    float* requirementValRemaining = new float[g_input.requirementCount];
    float* requirementRCFMoneyMonths = new float[g_input.requirementCount];
    for(int i=0; i<g_input.requirementCount; i++)
    {
        requirementValRemaining[i] = (float)g_input.requirements[i].amount;
        requirementRCFMoneyMonths[i] = (float)(g_input.requirements[i].amount * g_input.requirements[i].tenor);
    }

    // Compute the cost of each allocation
    float result = 0.0f;
    for(int allocID=0; allocID<allocationCount; allocID++)
    {
        int sourceID = allocations[allocID].sourceIndex;
        int reqID = allocations[allocID].requirementIndex;
        SourceInfo& source = g_input.sources[sourceID];
        RequirementInfo& requirement = g_input.requirements[reqID];

        int startDimension = allocations[allocID].allocStartDimension;
        float startDate = position[startDimension + 0];
        float tenor = position[startDimension + 1];
        float amount = position[startDimension + 2];

        sourceValRemaining[sourceID] -= amount;
        requirementValRemaining[reqID] -= amount;
        requirementRCFMoneyMonths[reqID] -= tenor*amount;
        result += amount*source.interestRate*tenor;
    }

    // Do some basic penalization for over-using a source
    // TODO: Remove/replace with better constraint handling
    for(int i=0; i<g_input.sourceCount; i++)
    {
        if(sourceValRemaining[i] < 0)
        {
            result += -sourceValRemaining[i] * 10000.0f;
        }
    }

    // Compute the cost of using the RCF for the outstanding requirement values
    float avgRemaining = 0.0f;
    for(int i=0; i<g_input.requirementCount; i++)
    {
        avgRemaining += requirementValRemaining[i];

        if(requirementValRemaining[i] <= 0)
        {
            result += -requirementValRemaining[i] * 10000.0f;
            continue;
        }

        int tenor = g_input.requirements[i].tenor;
        result += RCF_INTEREST_RATE * requirementRCFMoneyMonths[i];
    }
    avgRemaining /= g_input.requirementCount;
    //printf("Average of %f required still\n", avgRemaining);
    delete[] requirementValRemaining;
    delete[] requirementRCFMoneyMonths;

    return result;
}

Vector optimizeSwarm(Particle* swarm, int dimensionCount,
                  int allocCount, PSOAllocationPointer* allocations)
{
    // TODO: Surely theres way of getting a reasonable 2nd value for the state (other than time)
    RNGState rng = { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL };
    seedRNG(&rng, time(NULL), rng.inc);

    for(int i=0; i<SWARM_SIZE; i++)
    {
        for(int allocID=0; allocID<allocCount; allocID++)
        {
            SourceInfo& allocSource = g_input.sources[allocations[allocID].sourceIndex];
            int startDateDim = allocations[allocID].allocStartDimension;
            int tenorDim = startDateDim + 1;
            int amountDim = startDateDim + 2;
            swarm[i].position.coords[startDateDim] = 0; // TODO
            swarm[i].velocity.coords[startDateDim] = 0; // TODO

            swarm[i].position.coords[tenorDim] = randf(&rng) * allocSource.tenor;
            swarm[i].velocity.coords[tenorDim] = randf(&rng) * sqrtf((float)allocSource.tenor);

            swarm[i].position.coords[amountDim] = randf(&rng) * allocSource.amount;
            swarm[i].velocity.coords[amountDim] = randf(&rng) * sqrtf((float)allocSource.amount);
        }

        swarm[i].bestSeenLoc = swarm[i].position;
        swarm[i].bestSeenFitness = computeFitness(swarm[i].position, allocCount, allocations);

        swarm[i].neighbours[0] = &swarm[i];
        for(int neighbourIndex=1; neighbourIndex<NEIGHBOUR_COUNT; neighbourIndex++)
        {
            swarm[i].neighbours[neighbourIndex] = &swarm[randint(&rng, NEIGHBOUR_COUNT)];
        }
    }

    Vector bestLoc = swarm[0].position; // TODO: Maybe actually compute this correctly here
    float bestFitness = computeFitness(bestLoc, allocCount, allocations);
    for(int iteration=0; iteration<MAX_ITERATIONS; iteration++)
    {
        // TODO: Why is this a separate loop, surely it'd be equivalent (and faster)
        //       to do this at the beginning of the update loop?
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
                float selfFactor = SELF_BEST_FACTOR * randf(&rng);
                float neighbourFactor = NEIGHBOUR_BEST_FACTOR * randf(&rng);
                float globalFactor = GLOBAL_BEST_FACTOR * randf(&rng);

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

void computeAllocations(InputData inputData, int allocationCount, AllocationInfo* allocOutput)
{
    g_input = inputData;

    PSOAllocationPointer* swarmAllocations = new PSOAllocationPointer[allocationCount];
    for(int i=0; i<allocationCount; i++)
    {
        swarmAllocations[i].sourceIndex = allocOutput[i].sourceIndex;
        swarmAllocations[i].requirementIndex = allocOutput[i].requirementIndex;
        swarmAllocations[i].balanceIndex = allocOutput[i].balanceIndex;
        swarmAllocations[i].allocStartDimension = i*3;
    }

    int dimensionCount = allocationCount*3;
    Particle* swarm = new Particle[SWARM_SIZE];
    for(int i=0; i<SWARM_SIZE; i++)
    {
        swarm[i].position.coords = new float[dimensionCount];
        swarm[i].velocity.coords = new float[dimensionCount];
    }

    for(int i=0; i<allocationCount; i++)
    {
        int tenorVal = (int)bestSolution[allocations[i].allocStartDimension + 1];
        int amountVal = (int)bestSolution[allocations[i].allocStartDimension + 2];
        if((tenorVal > 0) && (amountVal > 0))
        {
            nondegenerateAllocationCount++;
        }
    }

    Vector bestSolution = optimizeSwarm(swarm, dimensionCount, allocationCount, swarmAllocations);

    for(int i=0; i<allocationCount; i++)
    {
        int allocStartDate = (int)bestSolution[swarmAllocations[i].allocStartDimension + 0];
        int allocTenor = (int)bestSolution[swarmAllocations[i].allocStartDimension + 1];
        int allocAmount = (int)bestSolution[swarmAllocations[i].allocStartDimension + 2];
        if((allocTenor <= 0) || (allocAmount <= 0))
            continue;

        tm allocStarttm = {};
        allocStarttm.tm_mday = 1;
        allocStarttm.tm_mon = 0;
        allocStarttm.tm_year = 2011 - 1900;

        allocOutput[i].startDate = allocStarttm;
        allocOutput[i].tenor = allocTenor;
        allocOutput[i].amount = allocAmount;
    }

    for(int i=0; i<SWARM_SIZE; i++)
    {
        delete[] swarm[i].position.coords;
        delete[] swarm[i].velocity.coords;
    }
    delete[] swarm;
    delete[] swarmAllocations;
}
