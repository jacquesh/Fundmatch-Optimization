#ifndef _PSO_H
#define _PSO_H

#include <math.h>

#include "fundmatch.h"

const int MAX_ITERATIONS = 500;
const int SWARM_SIZE = 200;
const int NEIGHBOUR_COUNT = 10;

const float PHI = 4.1f;
const float SELF_BEST_FACTOR = PHI/2.0f;
const float NEIGHBOUR_BEST_FACTOR = PHI/2.0f;
const float CONSTRICTION_COEFFICIENT = 2.0f/(PHI - 2.0f + sqrtf(PHI*PHI - 4.0f*PHI));

struct Particle
{
    Vector position;
    Vector velocity;

    Vector bestSeenLoc;

    Particle* neighbours[NEIGHBOUR_COUNT];

    Particle();
    Particle(int dimCount);
};

#endif
