#include <stdio.h>
#include <time.h>
#include <math.h>
#include <random>


#include "pso.h"
#include "fundmatch.h"

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
    std::random_device randDevice;
    std::mt19937 rng(randDevice());
    std::uniform_real_distribution<float> uniformf(0.0f, 1.0f);

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
    std::random_device randDevice;
    std::mt19937 rng(randDevice());
    std::uniform_real_distribution<float> uniformf(0.0f, 1.0f);
    std::uniform_int_distribution<int> uniformi(0, NEIGHBOUR_COUNT);
    for(int i=0; i<SWARM_SIZE; i++)
    {
        for(int allocID=0; allocID<allocationCount; allocID++)
        {
            SourceInfo& allocSource = g_input.sources[swarmAllocations[allocID].sourceIndex];
            int startDateDim = swarmAllocations[allocID].allocStartDimension;
            int tenorDim = startDateDim + 1;
            int amountDim = startDateDim + 2;
            swarm[i].position.coords[startDateDim] = 0; // TODO
            swarm[i].velocity.coords[startDateDim] = 0; // TODO

            swarm[i].position.coords[tenorDim] = uniformf(rng) * allocSource.tenor;
            swarm[i].velocity.coords[tenorDim] = uniformf(rng) * sqrtf((float)allocSource.tenor);

            swarm[i].position.coords[amountDim] = uniformf(rng) * allocSource.amount;
            swarm[i].velocity.coords[amountDim] = uniformf(rng) * sqrtf((float)allocSource.amount);
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

        tm allocStarttm = {};
        allocStarttm.tm_mday = 1;
        allocStarttm.tm_mon = 0;
        allocStarttm.tm_year = 2011 - 1900;

        allocations[i].startDate = allocStarttm;
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
