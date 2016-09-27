#ifndef _PSO_H
#define _PSO_H

#include <math.h>

const int MAX_ITERATIONS = 100;
const int SWARM_SIZE = 100;
const int NEIGHBOUR_COUNT = 10;

const float PHI = 4.1f;
const float SELF_BEST_FACTOR = PHI/2.0f;
const float NEIGHBOUR_BEST_FACTOR = PHI/2.0f;
const float CONSTRICTION_COEFFICIENT = 2.0f/(PHI - 2.0f + sqrtf(PHI*PHI - 4.0f*PHI));

struct Vector
{
    float* coords;

    float operator [](int index)
    {
        return coords[index];
    }
};

struct Particle
{
    Vector position;
    Vector velocity;

    Vector bestSeenLoc;
    float bestSeenFitness;

    Particle* neighbours[NEIGHBOUR_COUNT];
};

struct PSOAllocationPointer
{
    int sourceIndex;
    int requirementIndex;
    int balanceIndex;
    int allocStartDimension; // The index of the dimension where this allocation's data starts

    float getStartDate(Vector& data);
    float getTenor(Vector& data);
    float getAmount(Vector& data);

    float getEndDate(Vector& data);
};

#endif
