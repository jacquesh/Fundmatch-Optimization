#ifndef _RAND_H
#define _RAND_H

#include <stdint.h>

typedef uint8_t uint8;
typedef uint32_t uint32;
typedef uint64_t uint64;

// Random Number Generation
// From: www.pcg-random.org
// ========================
struct RNGState
{
    uint64 state;
    uint64 inc;
};

uint32 randint(RNGState* rng);
uint32 randint(RNGState* rng, uint32 bound);

float randf(RNGState* rng);

void seedRNG(RNGState* rng, uint64 initialState, uint64 initialSequence);

#endif
