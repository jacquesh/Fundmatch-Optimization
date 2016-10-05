#include <stdio.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <float.h>

#include <random>
#include <vector>
#include <algorithm>

#include "pso.h"
#include "fundmatch.h"
#include "logging.h"

using namespace std;

static InputData g_input;

static FileLogger plotLog = FileLogger("pso_fitness.dat");

static vector<int> requirementsByStart;
static vector<int> requirementsByEnd;

static const int START_DATE_OFFSET = 0;
static const int TENOR_OFFSET = 1;
static const int AMOUNT_OFFSET = 2;

Vector::Vector(int dimCount)
    : dimensions(dimCount)
{
    this->coords = new float[dimensions];
}

Vector::Vector(const Vector& other)
    : dimensions(other.dimensions)
{
    this->coords = new float[this->dimensions];
    memcpy(this->coords, other.coords, this->dimensions*sizeof(float));
}

Vector::~Vector()
{
    if(coords)
    {
        delete[] coords;
    }
}

Vector& Vector::operator =(const Vector& other)
{
    this->~Vector();
    this->dimensions = other.dimensions;
    this->coords = new float[this->dimensions];
    memcpy(this->coords, other.coords, this->dimensions*sizeof(float));

    return *this;
}

Particle::Particle()
    : position(0), velocity(0), bestSeenLoc(0)
{
}

Particle::Particle(int dimCount)
    : position(dimCount), velocity(dimCount), bestSeenLoc(dimCount)
{
}

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

bool isFeasible(Vector position, int allocationCount, PSOAllocationPointer* allocations)
{
    for(int allocID=0; allocID<allocationCount; allocID++)
    {
        // TODO: Balance pools
        SourceInfo& source = g_input.sources[allocations[allocID].sourceIndex];
        RequirementInfo& req = g_input.requirements[allocations[allocID].requirementIndex];
        float allocStart = allocations[allocID].getStartDate(position);
        float allocEnd = allocations[allocID].getEndDate(position);
        float allocTenor = allocations[allocID].getTenor(position);
        float allocAmount = allocations[allocID].getAmount(position);

        float sourceStart = (float)source.startDate;
        float sourceEnd = (float)(source.startDate + source.tenor);
        float sourceAmount = (float)source.amount;

        if((allocStart < sourceStart) || (allocStart > sourceEnd))
            return false;
        if((allocTenor < 0.0f) || (allocEnd > sourceEnd))
            return false;
        if((allocAmount < 0.0f) || (allocAmount > sourceAmount))
            return false;
    }

    // Check that no sources ever get over-used (IE 2 allocations with value 300 from a source of 500 at the same time)
    vector<PSOAllocationPointer*> allocationsByStart;
    for(int i=0; i<allocationCount; i++)
        allocationsByStart.push_back(&allocations[i]);
    vector<PSOAllocationPointer*> allocationsByEnd(allocationsByStart);

    auto allocStartDateComparison = [&position](PSOAllocationPointer* a, PSOAllocationPointer* b)
    {
        return a->getStartDate(position) < b->getStartDate(position);
    };
    auto allocEndDateComparison = [&position](PSOAllocationPointer* a, PSOAllocationPointer* b)
    {
        return a->getEndDate(position) < b->getEndDate(position);
    };
    sort(allocationsByStart.begin(), allocationsByStart.end(), allocStartDateComparison);
    sort(allocationsByEnd.begin(), allocationsByEnd.end(), allocEndDateComparison);

    float* sourceValueRemaining = new float[g_input.sourceCount];
    for(int i=0; i<g_input.sourceCount; i++)
    {
        sourceValueRemaining[i] = (float)g_input.sources[i].amount;
    }

    int allocStartIndex = 0;
    int allocEndIndex = 0;
    vector<PSOAllocationPointer*> activeAllocations;
    while((allocStartIndex < allocationCount) || (allocEndIndex < allocationCount))
    {
        float nextAllocStartTime = FLT_MAX;
        float nextAllocEndTime = FLT_MAX;
        if(allocStartIndex < allocationCount)
            nextAllocStartTime = allocationsByStart[allocStartIndex]->getStartDate(position);
        if(allocEndIndex < allocationCount)
            nextAllocEndTime = allocationsByEnd[allocEndIndex]->getEndDate(position);

        // Handle the allocation event
        // NOTE: It is significant that this is a strict inequality, because for very small
        //       tenor, we still want to handle the allocation start first
        // TODO: Balance pools
        if(nextAllocEndTime < nextAllocStartTime)
        {
            if(allocationsByEnd[allocEndIndex]->sourceIndex >= 0)
            {
                // Handle the allocation-end event
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
                assert(alloc != nullptr);
                sourceValueRemaining[alloc->sourceIndex] += alloc->getAmount(position);
            }
            allocEndIndex++;
        }
        else
        {
            // Handle the allocation-start event
            PSOAllocationPointer* alloc = allocationsByStart[allocStartIndex];
            allocStartIndex++;
            if(alloc->sourceIndex >= 0)
            {
                activeAllocations.push_back(alloc);
                sourceValueRemaining[alloc->sourceIndex] -= alloc->getAmount(position);
                if(sourceValueRemaining[alloc->sourceIndex] < 0.0f)
                    return false;
            }
        }
    }
    delete[] sourceValueRemaining;

    return true;
}

float computeFitness(Vector position, int allocationCount, PSOAllocationPointer* allocations)
{
    vector<PSOAllocationPointer*> allocationsByStart;
    for(int i=0; i<allocationCount; i++)
        allocationsByStart.push_back(&allocations[i]);
    vector<PSOAllocationPointer*> allocationsByEnd(allocationsByStart);

    auto allocStartDateComparison = [&position](PSOAllocationPointer* a, PSOAllocationPointer* b)
    {
        return a->getStartDate(position) < b->getStartDate(position);
    };
    auto allocEndDateComparison = [&position](PSOAllocationPointer* a, PSOAllocationPointer* b)
    {
        return a->getEndDate(position) < b->getEndDate(position);
    };
    sort(allocationsByStart.begin(), allocationsByStart.end(), allocStartDateComparison);
    sort(allocationsByEnd.begin(), allocationsByEnd.end(), allocEndDateComparison);

    float* requirementValueRemaining = new float[g_input.requirementCount];
    for(int i=0; i<g_input.requirementCount; i++)
    {
        requirementValueRemaining[i] = (float)g_input.requirements[i].amount;
    }
    bool* requirementActive = new bool[g_input.requirementCount];
    for(int i=0; i<g_input.requirementCount; i++)
        requirementActive[i] = false;

    float firstAllocationTime = allocationsByStart[0]->getStartDate(position);
    float firstRequirementTime = (float)g_input.requirements[requirementsByStart[0]].startDate;

    float result = 0.0f;
    float currentTime = min(firstAllocationTime, firstRequirementTime);
    int allocStartIndex = 0;
    int allocEndIndex = 0;
    int reqStartIndex = 0;
    int reqEndIndex = 0;
    vector<PSOAllocationPointer*> activeAllocations;
    while((allocStartIndex < allocationCount) || (allocEndIndex < allocationCount) ||
          (reqStartIndex < g_input.requirementCount) || (reqEndIndex < g_input.requirementCount))
    {
        float nextAllocStartTime = FLT_MAX;
        float nextAllocEndTime = FLT_MAX;
        float nextReqStartTime = FLT_MAX;
        float nextReqEndTime = FLT_MAX;
        if(allocStartIndex < allocationCount)
            nextAllocStartTime = allocationsByStart[allocStartIndex]->getStartDate(position);
        if(allocEndIndex < allocationCount)
            nextAllocEndTime = allocationsByEnd[allocEndIndex]->getEndDate(position);
        if(reqStartIndex < g_input.requirementCount)
            nextReqStartTime = (float)g_input.requirements[reqStartIndex].startDate;
        if(reqEndIndex < g_input.requirementCount)
            nextReqEndTime = (float)g_input.requirements[reqEndIndex].startDate +
                             (float)g_input.requirements[reqEndIndex].tenor;

        float nextAllocEventTime = min(nextAllocStartTime, nextAllocEndTime);
        float nextReqEventTime = min(nextReqStartTime, nextReqEndTime);

        float previousTime = currentTime;
        currentTime = min(nextAllocEventTime, nextReqEventTime);
        float timeElapsed = currentTime - previousTime;

        // Add up the costs of the allocations for this timestep
        for(int j=0; j<activeAllocations.size(); j++)
        {
            PSOAllocationPointer* activeAlloc = activeAllocations[j];
            float interestRate = BALANCEPOOL_INTEREST_RATE;
            if(activeAlloc->sourceIndex != -1)
                interestRate = g_input.sources[activeAlloc->sourceIndex].interestRate;

            result += timeElapsed * activeAlloc->getAmount(position) * interestRate;
        }

        // Add the cost of the unsatisfied requirements (IE the cost to satisfy them via RCF)
        for(int reqID=0; reqID<g_input.requirementCount; reqID++)
        {
            if(requirementActive[reqID] && (requirementValueRemaining[reqID] > 0.0f))
                result += timeElapsed * requirementValueRemaining[reqID] * RCF_INTEREST_RATE;
        }

        // Handle the event that we stopped on, depending on what type it is
        if(nextReqEventTime <= nextAllocEventTime)
        {
            // Handle the requirement event
            if(nextReqEndTime <= nextReqStartTime)
            {
                // Handle the requirement-end event
                int reqIndex = requirementsByEnd[reqEndIndex];
                requirementActive[reqIndex] = false;
                reqEndIndex++;
            }
            else
            {
                // Handle the requirement-start event
                int reqIndex = requirementsByStart[reqStartIndex];
                requirementActive[reqIndex] = true;
                reqStartIndex++;
            }
        }
        else
        {
            // Handle the allocation event
            // NOTE: It is significant that this is a strict inequality, because for very small
            //       tenor, we still want to handle the allocation start first
            // TODO: This will break if we every get a negative tenor, we currently don't
            //       check that solutions are feasible before computing cost, that needs to be done
            if(nextAllocEndTime < nextAllocStartTime)
            {
                // Handle the allocation-end event
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
                assert(alloc != nullptr);
                allocEndIndex++;
                requirementValueRemaining[alloc->requirementIndex] += alloc->getAmount(position);
            }
            else
            {
                // Handle the allocation-start event
                PSOAllocationPointer* alloc = allocationsByStart[allocStartIndex];
                activeAllocations.push_back(alloc);
                allocStartIndex++;
                requirementValueRemaining[alloc->requirementIndex] -= alloc->getAmount(position);
            }
        }
    }

    delete[] requirementValueRemaining;
    delete[] requirementActive;

    return result;
}

Vector optimizeSwarm(Particle* swarm, int dimensionCount,
                  int allocCount, PSOAllocationPointer* allocations)
{
    random_device randDevice;
    mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);

    // Compute the best position on the initial swarm positions
    int bestFitnessIndex = -1;
    float bestFitness = FLT_MAX;
    for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
    {
        assert(isFeasible(swarm[particleIndex].position, allocCount, allocations));

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
        swarm[i] = Particle(dimensionCount);
    }

    // Set up the sorted requirements lists so we don't have to do this in the fitness function
    for(int i=0; i<g_input.requirementCount; i++)
        requirementsByStart.push_back(i);
    requirementsByEnd = vector<int>(requirementsByStart);
    auto reqStartDateComparison = [](int reqIndexA, int reqIndexB)
    {
        int aStart = g_input.requirements[reqIndexA].startDate;
        int bStart = g_input.requirements[reqIndexB].startDate;
        return aStart < bStart;
    };
    auto reqEndDateComparison = [](int reqIndexA, int reqIndexB)
    {
        int aEnd = g_input.requirements[reqIndexA].startDate + g_input.requirements[reqIndexA].tenor;
        int bEnd = g_input.requirements[reqIndexB].startDate + g_input.requirements[reqIndexB].tenor;
        return aEnd < bEnd;
    };
    sort(requirementsByStart.begin(), requirementsByStart.end(), reqStartDateComparison);
    sort(requirementsByEnd.begin(), requirementsByEnd.end(), reqEndDateComparison);

    RequirementInfo& firstReq = g_input.requirements[requirementsByStart[0]];
    RequirementInfo& lastReq = g_input.requirements[requirementsByEnd[g_input.requirementCount-1]];
    int minReqDate = firstReq.startDate;
    int maxReqDate = lastReq.startDate + lastReq.tenor;
    assert(maxReqDate > minReqDate);

    // Initialize the swarm
    random_device randDevice;
    mt19937 rng(randDevice());
    uniform_real_distribution<float> uniformf(0.0f, 1.0f);
    uniform_real_distribution<float> centredUniformf(-1.0f, 1.0f);
    uniform_int_distribution<int> uniformi(0, NEIGHBOUR_COUNT);
    for(int i=0; i<SWARM_SIZE; i++)
    {
        for(int allocID=0; allocID<allocationCount; allocID++)
        {
            int allocSourceIndex = swarmAllocations[allocID].sourceIndex;
            int allocBalanceIndex = swarmAllocations[allocID].balanceIndex;
            assert(((allocSourceIndex == -1) && (allocBalanceIndex >= 0)) ||
                    ((allocSourceIndex >= 0) && (allocBalanceIndex == -1)));

            float minStartDate = (float)minReqDate;
            float maxStartDate = (float)maxReqDate;
            float maxTenor = -1;
            float maxAmount = -1;
            if(allocSourceIndex >= 0)
            {
                SourceInfo& allocSource = g_input.sources[allocSourceIndex];
                int sourceEndDate = allocSource.startDate + allocSource.tenor;
                if(allocSource.startDate > minReqDate)
                    minStartDate = (float)allocSource.startDate;
                if(sourceEndDate < maxReqDate)
                    maxStartDate = (float)sourceEndDate;
                maxTenor = (float)allocSource.tenor;
                maxAmount = (float)allocSource.amount;
            }
            else // NOTE: By the assert above, we necessarily have a balance pool here
            {
                BalancePoolInfo& allocBalance = g_input.balancePools[allocBalanceIndex];
                maxTenor = 1; // TODO
                maxAmount = (float)allocBalance.amount;
            }

            int allocStartDim = swarmAllocations[allocID].allocStartDimension;
            int startDateDim = allocStartDim + START_DATE_OFFSET;
            int tenorDim = allocStartDim + TENOR_OFFSET;
            int amountDim = allocStartDim + AMOUNT_OFFSET;
            float dateRange = maxStartDate - minStartDate;
            float startDate = minStartDate + (uniformf(rng) * dateRange);
            swarm[i].position.coords[startDateDim] = startDate;
            swarm[i].velocity.coords[startDateDim] = centredUniformf(rng) * dateRange;

            float maxValidStartingTenor = maxStartDate - startDate;
            // TODO: Generate a tenor that is based on the generated start date, so that we're
            //       always feasible just after initialization
            swarm[i].position.coords[tenorDim] = uniformf(rng) * maxValidStartingTenor;
            swarm[i].velocity.coords[tenorDim] = centredUniformf(rng) * maxTenor;

            swarm[i].position.coords[amountDim] = uniformf(rng) * maxAmount;
            swarm[i].velocity.coords[amountDim] = centredUniformf(rng) * maxAmount;
        }

        // NOTE: We just make sure to only generate random positions within the feasible region
        assert(isFeasible(swarm[i].position, allocationCount, swarmAllocations));
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
    delete[] swarm;
    delete[] swarmAllocations;
}
