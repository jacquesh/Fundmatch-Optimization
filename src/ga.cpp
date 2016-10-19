#include <stdio.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <float.h>

#include <random>
#include <vector>
#include <algorithm>

#include "ga.h"
#include "fundmatch.h"
#include "logging.h"

using namespace std;

static FileLogger plotLog = FileLogger("ga_fitness.dat");

void mutateIndividualAllocation(AllocationPointer& alloc, Vector& individual)
{
    static random_device randDevice;
    static mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);

    float mutationType = uniformf(rng);
    if(mutationType < 0.333f)
    {
        // Start Date
        float minStartDate = alloc.getMinStartDate();
        float maxStartDate = alloc.getMaxStartDate();
        float dateRange = maxStartDate - minStartDate;
        float newStartDate = minStartDate + uniformf(rng)*dateRange;
        alloc.setStartDate(individual, newStartDate);
    }
    else if(mutationType < 0.666f)
    {
        // Tenor
        float maxTenor = alloc.getMaxTenor(individual);
        float newTenor = uniformf(rng)*maxTenor;
        alloc.setTenor(individual, newTenor);
    }
    else
    {
        // Amount
        float maxAmount = alloc.getMaxAmount(individual);
        float newAmount = uniformf(rng)*maxAmount;
        alloc.setAmount(individual, newAmount);
    }
}

void crossoverAllocations(AllocationPointer& allocA, AllocationPointer& allocB, Vector* population)
{
    static random_device randDevice;
    static mt19937 rng(randDevice());
    uniform_int_distribution<int> randomIndividual(0, POPULATION_SIZE);
    // 1-point crossover
    int indivID = randomIndividual(rng);

    float tempStartDate = allocA.getStartDate(population[indivID]);
    float tempTenor = allocA.getTenor(population[indivID]);
    float tempAmount = allocA.getAmount(population[indivID]);

    allocA.setStartDate(population[indivID], allocB.getStartDate(population[indivID]));
    allocA.setTenor(population[indivID], allocB.getTenor(population[indivID]));
    allocA.setAmount(population[indivID], allocB.getAmount(population[indivID]));

    allocB.setStartDate(population[indivID], tempStartDate);
    allocB.setTenor(population[indivID], tempTenor);
    allocB.setAmount(population[indivID], tempAmount);
}

Vector evolvePopulation(Vector* population, int dimensionCount,
                        int allocCount, AllocationPointer* allocations)
{
    Vector bestIndividual = population[0];
    float bestFitness = FLT_MAX;

    random_device randDevice;
    mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);

    for(int iteration=0; iteration<MAX_ITERATIONS; iteration++)
    {
        for(int indivID=0; indivID<POPULATION_SIZE; indivID++)
        {
            if(isFeasible(population[indivID], allocCount, allocations))
            {
                float fitness = computeFitness(population[indivID], allocCount, allocations);
                if(fitness < bestFitness)
                {
                    bestFitness = fitness;
                    bestIndividual = population[indivID];
                }
            }

            // Crossover
            vector<int> allocIDs(allocCount);
            for(int i=0; i<allocCount; i++)
                allocIDs[i] = i;
            random_shuffle(allocIDs.begin(), allocIDs.end());
            for(int i=0; i<allocCount; i+=2)
            {
                AllocationPointer& allocA = allocations[allocIDs[i]];
                AllocationPointer& allocB = allocations[allocIDs[i+1]];
                crossoverAllocations(allocA, allocB, population);
            }

            // Mutation
            for(int allocID=0; allocID<allocCount; allocID++)
            {
                float shouldMutate = uniformf(rng);
                if(shouldMutate < MUTATION_RATE)
                {
                    mutateIndividualAllocation(allocations[allocID], population[indivID]);
                }
            }
        }
    }

    assert(isFeasible(bestIndividual, allocCount, allocations));
    return bestIndividual;
}

Vector computeAllocations(int allocationCount, AllocationPointer* allocations)
{
    // Create the swarm
    int dimensionCount = allocationCount * DIMENSIONS_PER_ALLOCATION;
    Vector* population = new Vector[POPULATION_SIZE];
    for(int i=0; i<POPULATION_SIZE; i++)
    {
        population[i] = Vector(dimensionCount);
    }

    // Initialize the swarm
    random_device randDevice;
    mt19937 rng(randDevice());
    for(int i=0; i<POPULATION_SIZE; i++)
    {
        for(int allocID=0; allocID<allocationCount; allocID++)
        {
            int retries = 0;
            do
            {
                initializeAllocation(allocations[allocID], population[i], rng);
                allocations[allocID].setAmount(population[i], 0); // TODO: See the TODO in pso.cpp
            } while((retries++ < 5) &&
                    !isFeasible(population[i], allocationCount, allocations));

            if(retries > 1)
                printf("Alloc %d-%d took %d retries to initialize\n", i, allocID, retries);
        }

        // NOTE: We just make sure to only generate random positions within the feasible region
        //assert(isFeasible(population[i], allocationCount, allocations));
    }
    printf("Initialization complete\n");

    // Run the GA on our new population
    Vector bestSolution = population[0];//optimizeSwarm(swarm, dimensionCount, allocationCount, allocations);

    // Cleanup
    delete[] population;

    return bestSolution;
}
