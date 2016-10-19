#include <stdio.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <float.h>

#include <random>

#include "pso.h"
#include "fundmatch.h"
#include "logging.h"

using namespace std;

static FileLogger plotLog = FileLogger("pso_fitness.dat");

Particle::Particle()
    : position(0), velocity(0), bestSeenLoc(0)
{
}

Particle::Particle(int dimCount)
    : position(dimCount), velocity(dimCount), bestSeenLoc(dimCount)
{
}

Vector optimizeSwarm(Particle* swarm, int dimensionCount,
                  int allocCount, AllocationPointer* allocations)
{
    random_device randDevice;
    mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);

    // Compute the best position on the initial swarm positions
    int bestFitnessIndex = -1;
    float bestFitness = FLT_MAX;
    for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
    {
        if(!isFeasible(swarm[particleIndex].position, allocCount, allocations))
            continue;

        float fitness = computeFitness(swarm[particleIndex].position, allocCount, allocations);
        if(fitness < bestFitness)
        {
            bestFitness = fitness;
            bestFitnessIndex = particleIndex;
        }
    }
    assert(bestFitnessIndex >= 0);
    Vector bestLoc = swarm[bestFitnessIndex].position;
    plotLog.log("%d %.2f\n", -1, bestFitness);

    for(int iteration=0; iteration<MAX_ITERATIONS; iteration++)
    {
        // Compute the fitness of each particle, updating its best seen as necessary
        // NOTE: We need to do this in a separate loop here first to ensure that all particles
        //       can compare with the correct best at the start of the current iteration
        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            if(!isFeasible(swarm[particleIndex].position, allocCount, allocations))
                continue;

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
        plotLog.log("%d %.2f\n", iteration, bestFitness);

        // Update particle velocities based on known best positions
        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            Particle& currentParticle = swarm[particleIndex];
            Vector localBestLoc = currentParticle.bestSeenLoc;
            Vector globalBestLoc = bestLoc;

            Vector neighbourBestLoc = currentParticle.neighbours[0]->bestSeenLoc;
            float neighbourBestFitness = currentParticle.neighbours[0]->bestSeenFitness;
            for(int neighbourIndex=1; neighbourIndex<NEIGHBOUR_COUNT; neighbourIndex++)
            {
                Particle* currentNeighbour = currentParticle.neighbours[neighbourIndex];
                if(currentNeighbour->bestSeenFitness < neighbourBestFitness)
                {
                    neighbourBestFitness = currentNeighbour->bestSeenFitness;
                    neighbourBestLoc = currentNeighbour->bestSeenLoc;;
                }
            }

            for(int dim=0; dim<dimensionCount; dim++)
            {
                float selfFactor = SELF_BEST_FACTOR * uniformf(rng);
                float neighbourFactor = NEIGHBOUR_BEST_FACTOR * uniformf(rng);

                currentParticle.velocity.coords[dim] = CONSTRICTION_COEFFICIENT * (
                    currentParticle.velocity[dim] +
                    (selfFactor * (localBestLoc[dim] - currentParticle.position[dim])) +
                    (neighbourFactor * (neighbourBestLoc[dim] - currentParticle.position[dim]))
                    );
            }
        }

        // Do a timestep of particle movement
        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            for(int dim=0; dim<dimensionCount; dim++)
            {
                swarm[particleIndex].position.coords[dim] += swarm[particleIndex].velocity[dim];
            }
        }
    }
    return bestLoc;
}

Vector computeAllocations(int allocationCount, AllocationPointer* allocations)
{
    // Create the swarm
    int dimensionCount = allocationCount * DIMENSIONS_PER_ALLOCATION;
    Particle* swarm = new Particle[SWARM_SIZE];
    for(int i=0; i<SWARM_SIZE; i++)
    {
        swarm[i] = Particle(dimensionCount);
    }

    RequirementInfo& firstReq = g_input.requirements[g_input.requirementsByStart[0]];
    RequirementInfo& lastReq = g_input.requirements[g_input.requirementsByEnd[g_input.requirements.size()-1]];
    int minReqDate = firstReq.startDate;
    int maxReqDate = lastReq.startDate + lastReq.tenor;
    assert(maxReqDate > minReqDate);

    // Initialize the swarm
    random_device randDevice;
    mt19937 rng(randDevice());
    uniform_real_distribution<float> centredUniformf(-1.0f, 1.0f);
    uniform_int_distribution<int> uniformi(0, NEIGHBOUR_COUNT);
    for(int i=0; i<SWARM_SIZE; i++)
    {
        for(int allocID=0; allocID<allocationCount; allocID++)
        {
            int retries = 0;
            do
            {
                initializeAllocation(allocations[allocID], swarm[i].position, rng);
                allocations[allocID].setAmount(swarm[i].position, 0); // TODO: This prevents us from getting 0 valid particles, would like to find a better solution though
                // NOTE: The reason we're getting no valid particles is that checking individual
                //       allocations independently is fine, but when you consider them all at once
                //       it isn't valid because you over-allocate from some source.
                //
                //       It might be a good idea to change the isFeasible version with non-default
                //       checkAlloc to check all allocations up-to-and-including checkAlloc instead
                //       of just checkAlloc.
            } while((retries++ < 5) &&
                    !isFeasible(swarm[i].position, allocationCount, allocations));

            if(retries > 1)
                printf("Alloc %d-%d took %d retries to initialize\n", i, allocID, retries);

            // Initialize allocation velocity
            int requirementID = allocations[allocID].requirementIndex;
            int allocReqStartDate = g_input.requirements[requirementID].startDate;
            int allocReqTenor = g_input.requirements[requirementID].tenor;
            int allocReqAmount = g_input.requirements[requirementID].amount;

            float startDateVelocity = centredUniformf(rng) * 0.5f * (float)allocReqTenor;
            float tenorVelocity = centredUniformf(rng) * 0.5f * (float)allocReqTenor;
            float amountVelocity = centredUniformf(rng) * 0.5f * (float)allocReqAmount;
            allocations[allocID].setStartDate(swarm[i].velocity, startDateVelocity);
            allocations[allocID].setTenor(swarm[i].velocity, tenorVelocity);
            allocations[allocID].setAmount(swarm[i].velocity, amountVelocity);
        }

        swarm[i].bestSeenLoc = swarm[i].position;
        swarm[i].bestSeenFitness = computeFitness(swarm[i].position, allocationCount, allocations);

        swarm[i].neighbours[0] = &swarm[i];
        for(int neighbourIndex=1; neighbourIndex<NEIGHBOUR_COUNT; neighbourIndex++)
        {
            swarm[i].neighbours[neighbourIndex] = &swarm[uniformi(rng)];
        }
    }
    printf("Initialization complete\n");

    // Run PSO using our new swarm
    Vector bestSolution = optimizeSwarm(swarm, dimensionCount, allocationCount, allocations);

    // Cleanup
    delete[] swarm;

    return bestSolution;
}
