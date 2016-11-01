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
//static minstd_rand randDevice(3);
static random_device randDevice;


void mutateIndividual(Vector& individual, int allocCount, AllocationPointer* allocations)
{
    static mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);

    for(int allocID=0; allocID<allocCount; allocID++)
    {
        float shouldMutate = uniformf(rng);
        if(!(shouldMutate < MUTATION_RATE))
            continue;

        AllocationPointer& alloc = allocations[allocID];
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
}

static void crossoverIndividualAllocation(Vector& indivA, Vector& indivB, AllocationPointer& alloc)
{
    float tempStartDate = alloc.getStartDate(indivA);
    float tempTenor = alloc.getTenor(indivA);
    float tempAmount = alloc.getAmount(indivA);

    alloc.setStartDate(indivA, alloc.getStartDate(indivB));
    alloc.setTenor(indivA, alloc.getTenor(indivB));
    alloc.setAmount(indivA, alloc.getAmount(indivB));

    alloc.setStartDate(indivB, tempStartDate);
    alloc.setTenor(indivB, tempTenor);
    alloc.setAmount(indivB, tempAmount);
}

void crossoverIndividuals(Vector& individualA, Vector& individualB,
                          int allocationCount, AllocationPointer* allocations)
{
    static mt19937 rng(randDevice());
#if 0
    // 1-point crossover
    uniform_int_distribution<int> randomIndividual(0, allocationCount-1); // Endpoints are inclusive
    AllocationPointer& alloc = allocations[randomIndividual(rng)];
    crossoverIndividualAllocation(individualA, individualB, alloc);
#endif

#if 0
    // N-point crossover
    uniform_real_distribution<float> shouldCrossover(0.0f, 1.0f);
    for(int allocID=0; allocID<allocationCount; allocID++)
    {
        if(!(shouldCrossover(rng) <= CROSSOVER_RATE))
            continue;

        AllocationPointer& alloc = allocations[allocID];
        crossoverIndividualAllocation(individualA, individualB, alloc);
    }
#endif

#if 1
    // Requirement crossover
    uniform_int_distribution<int> randomReq(0, (int)g_input.requirements.size()-1);
    int crossedReq = randomReq(rng);
    for(int allocID=0; allocID<allocationCount; allocID++)
    {
        AllocationPointer& alloc = allocations[allocID];
        if(alloc.requirementIndex != crossedReq)
            continue;

        crossoverIndividualAllocation(individualA, individualB, alloc);
    }
#endif
}

Vector evolvePopulation(Individual* population, int dimensionCount,
                        int allocCount, AllocationPointer* allocations)
{
    Individual bestIndividual = population[0];
    for(int indivID=1; indivID<POPULATION_SIZE; indivID++)
    {
        if(population[indivID].fitness < bestIndividual.fitness)
            bestIndividual = population[indivID];
    }
    plotLog.log("%d %.2f\n", -1, bestIndividual.fitness);

    mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);
    uniform_int_distribution<int> uniformIndiv(0, POPULATION_SIZE-1); // Inclusive

    assert(PARENT_COUNT % 2 == 0); // So we can do nice crossover
    vector<Individual> parentList(PARENT_COUNT);

    for(int iteration=0; iteration<MAX_ITERATIONS; iteration++)
    {
        // Parent Selection
        for(int parentID=0; parentID<PARENT_COUNT; parentID++)
        {
            Vector* tourneyWinner = nullptr;
            for(int i=0; i<TOURNAMENT_SIZE; i++)
            {
                Vector* contestant = nullptr;
                int contestantID = uniformIndivOrBest(rng);
                if(contestantID == -1)
                    contestant = &bestIndividual;
                else
                    contestant = &population[contestantID];

                assert(contestant != nullptr);
                if(tourneyWinner == nullptr)
                {
                    tourneyWinner = contestant;
                }
                else if(isPositionBetter(*contestant, *tourneyWinner, allocCount, allocations))
                {
                    tourneyWinner = contestant;
                }
            }

            parentList[parentID] = *tourneyWinner;
        }

        // Crossover
        for(int parentID=0; parentID<PARENT_COUNT; parentID+=2)
        {
            crossoverIndividuals(parentList[parentID].position, parentList[parentID+1].position, allocCount, allocations);
        }

        // Mutation
        for(int parentID=0; parentID<PARENT_COUNT; parentID++)
        {
            mutateIndividual(parentList[parentID].position, allocCount, allocations);
        }

        // Child selection
        for(int parentID=0; parentID<PARENT_COUNT; parentID++)
        {
            float maxFitness = population[0].fitness;
            int maxFitnessIndex = 0;
            for(int i=1; i<POPULATION_SIZE; i++)
            {
                if(population[i].fitness > maxFitness)
                {
                    maxFitness = population[i].fitness;
                    maxFitnessIndex = i;
                }
            }
            int replacedIndiv = uniformIndiv(rng);
            population[maxFitnessIndex] = parentList[parentID];
        }

        // Evaluation
        for(int indivID=0; indivID<POPULATION_SIZE; indivID++)
        {
            if(isFeasible(population[indivID].position, allocCount, allocations))
            {
                float fitness = computeFitness(population[indivID].position, allocCount, allocations);
                population[indivID].fitness = fitness;
                if(fitness < bestIndividual.fitness)
                {
                    bestIndividual = population[indivID];
                }
            }
            else
            {
                population[indivID].fitness = FLT_MAX;
            }
        }
        plotLog.log("%d %.2f\n", iteration, bestIndividual.fitness);
    }

    return bestIndividual.position;
}

Vector computeAllocations(int allocationCount, AllocationPointer* allocations)
{
    // Create the swarm
    int dimensionCount = allocationCount * DIMENSIONS_PER_ALLOCATION;
    Individual* population = new Individual[POPULATION_SIZE];
    for(int i=0; i<POPULATION_SIZE; i++)
    {
        population[i].position = Vector(dimensionCount);
        // NOTE: We initialize the values here just so that our initial solution is feasible
        for(int allocID=0; allocID<allocationCount; allocID++)
        {
            float minStartDate = allocations[allocID].getMinStartDate();
            allocations[allocID].setStartDate(population[i].position, minStartDate);
            allocations[allocID].setTenor(population[i].position, 0.0f);
            allocations[allocID].setAmount(population[i].position, 0.0f);
        }
    }

    // Initialize the swarm
    mt19937 rng(randDevice());
    for(int i=0; i<POPULATION_SIZE; i++)
    {
        int retries = 0;
        do
        {
            for(int allocID=0; allocID<allocationCount; allocID++)
            {
                initializeAllocation(allocations[allocID], population[i].position, rng);
                allocations[allocID].setAmount(population[i].position, 0); // TODO: See the TODO in pso.cpp
            }
        } while((retries++ < 5) &&
                !isFeasible(population[i].position, allocationCount, allocations));

        if(isFeasible(population[i].position, allocationCount, allocations))
            population[i].fitness = computeFitness(population[i].position, allocationCount, allocations);
        else
            population[i].fitness = FLT_MAX;

        if(retries > 1)
            printf("Alloc %d took %d retries to initialize\n", i, retries);
    }
    printf("Initialization complete\n");

    // Run the GA on our new population
    Vector bestSolution = evolvePopulation(population, dimensionCount, allocationCount, allocations);

    // Cleanup
    delete[] population;

    return bestSolution;
}
