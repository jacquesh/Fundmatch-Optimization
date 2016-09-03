#ifndef _PSO_H
#define _PSO_H

const int MAX_ITERATIONS = 100;
const int SWARM_SIZE = 100;
const int NEIGHBOUR_COUNT = 10;

const float DOMAIN_SIZE = 5.0f;
const float START_VELOCITY_SCALE = 2.0f;

const float TIMESTEP = 0.02f;
const float VELOCITY_UPDATE_FACTOR = 0.8f;
const float SELF_BEST_FACTOR = 0.55f;
const float NEIGHBOUR_BEST_FACTOR = 0.44f;
const float GLOBAL_BEST_FACTOR = 0.01f;

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
};

#endif
