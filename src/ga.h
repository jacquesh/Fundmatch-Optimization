#ifndef _GA_H
#define _GA_H

#include <math.h>

#include "fundmatch.h"

const int MAX_ITERATIONS = 500;
const int POPULATION_SIZE = 200;

const float MUTATION_RATE = 1.0f/POPULATION_SIZE;
const float CROSSOVER_RATE = 0.60f;
const int TOURNAMENT_SIZE = 75;

#endif
