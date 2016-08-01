#include "rand.h"

// Returns a random, uniformly distributed, unsigned 32-bit integer
uint32 randint(RNGState* rng)
{
    uint64 oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    uint32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32 rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

// Returns a random, uniformly distributed, unsigned integer r in the range 0 <= r < bound
uint32 randint(RNGState* rng, uint32 bound)
{
    uint32 threshold = -bound % bound;
    for (;;) {
        uint32 r = randint(rng);
        if (r >= threshold)
        {
            return r % bound;
        }
    }
}

// Returns a random, uniformly distributed 32-bit float r in the range 0.0 <= r < 1.0
float randf(RNGState* rng)
{
    uint32 resultInt = randint(rng);
    float result = resultInt * (1.0f/(float)UINT32_MAX);

    return result;
}

// Seeds the given RNGState using the initialState and initialSequence parameters
void seedRNG(RNGState* rng, uint64 initialState, uint64 initialSequence)
{
    rng->state = 0u;
    rng->inc = (initialSequence << 1u) | 1u;
    randint(rng);
    rng->state += initialState;
    randint(rng);
}
