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
        if(uniformf(rng) > MUTATION_RATE)
            continue;

        AllocationPointer& alloc = allocations[allocID];
#if 1 // Single value mutation
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
            float newTenor = round(uniformf(rng)*maxTenor);
            alloc.setTenor(individual, newTenor);
        }
        else
        {
            // Amount
            float maxAmount = alloc.getMaxAmount(individual);
            float newAmount = round(uniformf(rng)*maxAmount);
            alloc.setAmount(individual, newAmount);
        }
#endif
#if 0   // Single allocation mutation
        float minStartDate = alloc.getMinStartDate();
        float maxStartDate = alloc.getMaxStartDate();
        float dateRange = maxStartDate - minStartDate;
        float newStartDate = minStartDate + uniformf(rng)*dateRange;
        float maxTenor = alloc.getMaxTenor(individual);
        float newTenor = round(uniformf(rng)*maxTenor);
        float maxAmount = alloc.getMaxAmount(individual);
        float newAmount = round(uniformf(rng)*maxAmount);

        alloc.setStartDate(individual, newStartDate);
        alloc.setTenor(individual, newTenor);
        alloc.setAmount(individual, newAmount);
#endif
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
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);

    if(uniformf(rng) > CROSSOVER_RATE)
        return;

#if 0
    // Standard crossover (swap one side of a single point)
    uniform_int_distribution<int> randomAllocation(0, allocationCount-1); // Endpoints are inclusive
    int middleAllocation = randomAllocation(rng);
    for(int allocID=0; allocID<middleAllocation; allocID++)
    {
        AllocationPointer& alloc = allocations[allocID];
        crossoverIndividualAllocation(individualA, individualB, alloc);
    }
#endif

#if 0
    // Interval crossover
    uniform_int_distribution<int> randomAllocation(0, allocationCount-1);
    int fromAlloc = randomAllocation(rng);
    int toAlloc = randomAllocation(rng);
    int currentAlloc = fromAlloc;
    while(currentAlloc != toAlloc)
    {
        AllocationPointer& alloc = allocations[currentAlloc];
        crossoverIndividualAllocation(individualA, individualB, alloc);

        currentAlloc = (currentAlloc+1)%allocationCount;
    }
#endif

#if 1
    // N-point crossover
    for(int allocID=0; allocID<allocationCount; allocID++)
    {
        if(uniformf(rng) > 0.5f)
            continue;

        AllocationPointer& alloc = allocations[allocID];
        crossoverIndividualAllocation(individualA, individualB, alloc);
    }
#endif

#if 0
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

Vector evolvePopulation(Vector* population, int dimensionCount,
                        int allocCount, AllocationPointer* allocations)
{
    int bestIndivIndex = 0;
    for(int indivID=1; indivID<POPULATION_SIZE; indivID++)
    {
        if(isPositionBetter(population[indivID], population[bestIndivIndex],
                            allocCount, allocations))
        {
            bestIndivIndex = indivID;
        }
    }

    Vector bestIndividual = population[bestIndivIndex];
    if(bestIndividual.fitness != FLT_MAX)
        plotLog.log("%d %.2f\n", -1, bestIndividual.fitness);

    mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);
    uniform_int_distribution<int> uniformIndiv(0, POPULATION_SIZE-1); // Inclusive
    uniform_int_distribution<int> uniformIndivOrBest(-1, POPULATION_SIZE-1);

    assert(POPULATION_SIZE % 2 == 0); // So we can do nice crossover
    vector<Vector> parentList(POPULATION_SIZE);

    for(int iteration=0; iteration<MAX_ITERATIONS; iteration++)
    {
        // Parent Selection
        for(int parentID=0; parentID<POPULATION_SIZE; parentID++)
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
        for(int parentID=0; parentID<POPULATION_SIZE; parentID+=2)
        {
            crossoverIndividuals(parentList[parentID], parentList[parentID+1], allocCount, allocations);
            // NOTE: These same Vectors will get updated again during mutation, and thats when
            //       we'll get their new violation/fitness
        }

        // Mutation
        for(int parentID=0; parentID<POPULATION_SIZE; parentID++)
        {
            mutateIndividual(parentList[parentID], allocCount, allocations);

            parentList[parentID].processPositionUpdate(allocCount, allocations);
        }

        // Child selection
        for(int i=0; i<POPULATION_SIZE; i++)
        {
            population[i] = parentList[i];
        }

        // Evaluation
        for(int indivID=0; indivID<POPULATION_SIZE; indivID++)
        {
            if(isPositionBetter(population[indivID], bestIndividual, allocCount, allocations))
            {
                bestIndividual = population[indivID];
            }
        }
        if(bestIndividual.fitness != FLT_MAX)
            plotLog.log("%d %.2f\n", iteration, bestIndividual.fitness);
    }

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
        // NOTE: We initialize the values here just so that our initial solution is feasible
        for(int allocID=0; allocID<allocationCount; allocID++)
        {
            float minStartDate = allocations[allocID].getMinStartDate();
            allocations[allocID].setStartDate(population[i], minStartDate);
            allocations[allocID].setTenor(population[i], 0.0f);
            allocations[allocID].setAmount(population[i], 0.0f);
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
                initializeAllocation(allocations[allocID], population[i], rng);
            }
        } while((retries++ < 5) &&
                !isFeasible(population[i], allocationCount, allocations));

        if(i == 0)
        {
            for(int allocID=0; allocID<allocationCount; allocID++)
                allocations[allocID].setAmount(population[0], 0.0f);
        }
        population[i].processPositionUpdate(allocationCount, allocations);
    }
    printf("Initialization complete\n");

    // Run the GA on our new population
    Vector bestSolution = evolvePopulation(population, dimensionCount, allocationCount, allocations);

    // Cleanup
    delete[] population;

    return bestSolution;
}
