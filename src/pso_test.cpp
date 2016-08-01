#include <stdio.h>
#include <time.h>
#include <math.h>

#include "rand.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

const float pi = 3.1415927f;
const float e  = 2.7182818f;

const int NUM_DIMENSIONS = 2;
const int MAX_ITERATIONS = 1000;
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
    float coords[NUM_DIMENSIONS];

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

void plotSwarm(const char* filename, Particle* swarm)
{
    const int pixelsPerUnit = 20;
    int size = 2*(int)(DOMAIN_SIZE*pixelsPerUnit);
    uint8* data = new uint8[3*size*size];
    memset(data, 0, 3*size*size);

    for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
    {
        float particleX = swarm[particleIndex].position[0];
        float particleY = swarm[particleIndex].position[1];

        int pixelX = (int)((particleX + DOMAIN_SIZE)*pixelsPerUnit);
        int pixelY = (int)((particleY + DOMAIN_SIZE)*pixelsPerUnit);
        int pixelIndex = 3*(pixelY*size + pixelX);
        data[pixelIndex + 0] = 255;
    }

    stbi_write_bmp(filename, size, size, 3, data);
    delete[] data;
}

float computeFitness(Vector position)
{
    float result = 0.0f;
#if 0
    // Sphere function
    for(int d=0; d<NUM_DIMENSIONS; d++)
    {
        result += position[d]*position[d];
    }
#endif

#if 1
    // Ackley function
    result -= 20.0f*expf(-0.2f * sqrtf(0.5f*(position[0]*position[0] + position[1]*position[1])));
    result -= expf(0.5f * (cosf(2.0f*pi*position[0]) + cosf(2.0f*pi*position[1])));
    result += e + 20.0f;
#endif

#if 0
    // Rosebrock function
    for(int d=0; d<NUM_DIMENSIONS-1; d++)
    {
        float coordDiff = position[d+1] - (position[d]*position[d]);
        result += 100*coordDiff*coordDiff + (position[d] - 1.0f)*(position[d] - 1.0f);
    }
#endif
    return result;
}



int main()
{
    RNGState rng = { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL };
    seedRNG(&rng, time(NULL), rng.inc);

    Particle swarm[SWARM_SIZE];

    for(int i=0; i<SWARM_SIZE; i++)
    {
        for(int dim=0; dim<NUM_DIMENSIONS; dim++)
        {
            swarm[i].position.coords[dim] = DOMAIN_SIZE*(2.0f*randf(&rng) - 1.0f);
            swarm[i].velocity.coords[dim] = START_VELOCITY_SCALE*(2.0f*randf(&rng) - 1.0f);

            swarm[i].bestSeenLoc = swarm[i].position;
            swarm[i].bestSeenFitness = computeFitness(swarm[i].position);
        }

        swarm[i].neighbours[0] = &swarm[i];
        for(int neighbourIndex=1; neighbourIndex<NEIGHBOUR_COUNT; neighbourIndex++)
        {
            swarm[i].neighbours[neighbourIndex] = &swarm[randint(&rng, NEIGHBOUR_COUNT)];
        }
    }

    Vector bestLoc = swarm[0].position; // TODO: Might be a reasonable idea to actually compute this correctly here
    float bestFitness = computeFitness(bestLoc);
    for(int iteration=0; iteration<MAX_ITERATIONS; iteration++)
    {
        int plotInterval = MAX_ITERATIONS/20;
        if(iteration % plotInterval == 0)
        {
            char file[64];
            sprintf(file, "swarm_%d.bmp", iteration);
            plotSwarm(file, swarm);
        }

        // TODO: Why is this a separate loop, surely it'd be equivalent (and faster)
        //       to do this at the beginning of the update loop?
        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            float fitness = computeFitness(swarm[particleIndex].position);
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

        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            Particle& currentParticle = swarm[particleIndex];
            Vector localBestLoc = currentParticle.bestSeenLoc;
            Vector globalBestLoc = bestLoc;

            Vector neighbourBestLoc = currentParticle.neighbours[0]->bestSeenLoc;
            float neighbourBestFitness = computeFitness(neighbourBestLoc);
            for(int neighbourIndex=1; neighbourIndex<NEIGHBOUR_COUNT; neighbourIndex++)
            {
                Vector tempNeighbourBestLoc = currentParticle.neighbours[neighbourIndex]->bestSeenLoc;
                float tempNeighbourBestFitness = computeFitness(tempNeighbourBestLoc);
                if(tempNeighbourBestFitness < neighbourBestFitness)
                {
                    neighbourBestFitness = tempNeighbourBestFitness;
                    neighbourBestLoc = tempNeighbourBestLoc;
                }
            }

            for(int dim=0; dim<NUM_DIMENSIONS; dim++)
            {
                float selfFactor = SELF_BEST_FACTOR * randf(&rng);
                float neighbourFactor = NEIGHBOUR_BEST_FACTOR * randf(&rng);
                float globalFactor = GLOBAL_BEST_FACTOR * randf(&rng);

                currentParticle.velocity.coords[dim] =
                    (VELOCITY_UPDATE_FACTOR * currentParticle.velocity[dim]) +
                    (selfFactor * (localBestLoc[dim] - currentParticle.position[dim])) +
                    (neighbourFactor * (neighbourBestLoc[dim] - currentParticle.position[dim])) +
                    (globalFactor * (globalBestLoc[dim] - currentParticle.position[dim]));
            }
        }

        for(int particleIndex=0; particleIndex<SWARM_SIZE; particleIndex++)
        {
            for(int dim=0; dim<NUM_DIMENSIONS; dim++)
            {
                swarm[particleIndex].position.coords[dim] +=
                    TIMESTEP * swarm[particleIndex].velocity[dim];
            }
        }
    }

    char file[64];
    sprintf(file, "swarm_%d.bmp", MAX_ITERATIONS);
    plotSwarm(file, swarm);

    printf("Final best solution was %f at (%f,%f)\n", bestFitness, bestLoc[0], bestLoc[1]);
    return 0;
}
