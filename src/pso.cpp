#include <stdio.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <float.h>

#include <random>
#include <algorithm>

#include "pso.h"
#include "fundmatch.h"
#include "logging.h"

using namespace std;

static FileLogger plotLog = FileLogger("pso_fitness.dat");
static random_device randDevice;

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
    mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);

    // Compute the best position on the initial swarm positions
    int bestFitnessIndex = 0;
    for(int particleIndex=1; particleIndex<SWARM_SIZE; particleIndex++)
    {
        if(isPositionBetter(swarm[particleIndex].position, swarm[bestFitnessIndex].position, allocCount, allocations))
        {
            bestFitnessIndex = particleIndex;
        }
    }
    Vector bestLoc = swarm[bestFitnessIndex].position;
    plotLog.log("%d %.2f\n", -1, bestLoc.fitness);

    for(int iteration=0; iteration<MAX_ITERATIONS; iteration++)
    {
        // Compute the fitness of each particle, updating its best seen as necessary
        // NOTE: We need to do this in a separate loop here first to ensure that all particles
        //       can compare with the correct best at the start of the current iteration
        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            Particle& particle = swarm[particleIndex];
            if(isPositionBetter(particle.position, bestLoc, allocCount, allocations))
            {
                bestLoc = particle.position;
            }
            if(isPositionBetter(particle.position, particle.bestSeenLoc, allocCount, allocations))
            {
                particle.bestSeenLoc = particle.position;
            }
        }
        plotLog.log("%d %.2f\n", iteration, bestLoc.fitness);

        // Update particle velocities based on known best positions
        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            Particle& particle = swarm[particleIndex];
            Vector globalBestLoc = bestLoc;

            // TODO: Just use a pointer here, we're doing a boatload of copying as it stands
            Vector neighbourBestLoc = particle.neighbours[0]->bestSeenLoc;
            for(int neighbourIndex=1; neighbourIndex<NEIGHBOUR_COUNT; neighbourIndex++)
            {
                Particle* neighbour = particle.neighbours[neighbourIndex];
                if(isPositionBetter(neighbour->bestSeenLoc, neighbourBestLoc, allocCount, allocations))
                {
                    neighbourBestLoc = neighbour->bestSeenLoc;
                }
            }

            for(int dim=0; dim<dimensionCount; dim++)
            {
                float selfFactor = SELF_BEST_FACTOR * uniformf(rng);
                float neighbourFactor = NEIGHBOUR_BEST_FACTOR * uniformf(rng);

                float selfBestOffset = particle.bestSeenLoc[dim] - particle.position[dim];
                float neighbourBestOffset = neighbourBestLoc[dim] - particle.position[dim];

                particle.velocity.coords[dim] = CONSTRICTION_COEFFICIENT * (
                    particle.velocity[dim] +
                    (selfFactor * selfBestOffset) +
                    (neighbourFactor * neighbourBestOffset)
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

            swarm[particleIndex].position.processPositionUpdate(allocCount, allocations);
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
    mt19937 rng(randDevice());
    uniform_real_distribution<float> centredUniformf(-1.0f, 1.0f);
    uniform_int_distribution<int> uniformParticleIndex(0, SWARM_SIZE-1); // Endpoints are inclusive
    for(int i=0; i<SWARM_SIZE; i++)
    {
        int retries = 0;
        do
        {
            for(int allocID=0; allocID<allocationCount; allocID++)
            {
                initializeAllocation(allocations[allocID], swarm[i].position, rng);
            }
        } while((retries++ < 5) &&
                !isFeasible(swarm[i].position, allocationCount, allocations));

        if(i == 0)
        {
            for(int allocID=0; allocID<allocationCount; allocID++)
                allocations[allocID].setAmount(swarm[0].position, 0.0f);
        }
        swarm[i].position.processPositionUpdate(allocationCount, allocations);

        // Initialize allocation velocity
        for(int allocID=0; allocID<allocationCount; allocID++)
        {
            float startDateVelocity = centredUniformf(rng) * 0.1f;
            float tenorVelocity = centredUniformf(rng) * 0.1f;
            float amountVelocity = centredUniformf(rng) * 0.1f;
            allocations[allocID].setStartDate(swarm[i].velocity, startDateVelocity);
            allocations[allocID].setTenor(swarm[i].velocity, tenorVelocity);
            allocations[allocID].setAmount(swarm[i].velocity, amountVelocity);
        }

        swarm[i].bestSeenLoc = swarm[i].position;

        swarm[i].neighbours[0] = &swarm[i];
        for(int neighbourIndex=1; neighbourIndex<NEIGHBOUR_COUNT; neighbourIndex++)
        {
            swarm[i].neighbours[neighbourIndex] = &swarm[uniformParticleIndex(rng)];
        }
    }
    printf("Initialization complete\n");

    // Run PSO using our new swarm
    Vector bestSolution = optimizeSwarm(swarm, dimensionCount, allocationCount, allocations);

    // Cleanup
    delete[] swarm;

    return bestSolution;
}
