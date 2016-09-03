#ifndef _FUNDMATCH_H
#define _FUNDMATCH_H

#include <vector>
#include <time.h>

const float RCF_INTEREST_RATE = 0.13f;

enum class TaxClass
{
    None,
    IPF,
    UPF,
    CF,
};

struct SourceInfo
{
    char* segment;
    tm startDate;
    int tenor;
    int amount;
    char* sourceType;
    char* sourceTypeCategory;
    TaxClass taxClass;
    float interestRate;
};

struct BalancePoolInfo
{
    // TODO: Decide what other values we need here (including for Tim)
    float amount;
};

struct RequirementInfo
{
    char* segment;
    tm startDate;
    int tenor;
    int amount;
    char* tier;
    char* purpose;
    TaxClass taxClass;
};

struct AllocationInfo
{
    int sourceIndex;
    int requirementIndex;
    int balanceIndex;
    tm startDate;
    int tenor;
    int amount;
};

struct InputData
{
    int sourceCount;
    SourceInfo* sources;
    int balancePoolCount;
    BalancePoolInfo* balancePools;
    int requirementCount;
    RequirementInfo* requirements;
};

// Allocates an array of SourceInfo, and puts it into input.sources.
// Returns true iff the function succeeded, if false is returned then input will not be modified.
bool loadSourceData(const char* inputFilename, InputData& input);

// Allocates an array of BalancePoolInfo, and puts it into input.balancePools.
// Returns true iff the function succeeded, if false is returned then input will not be modified.
bool loadBalancePoolData(const char* inputFilename, InputData& input);

// Allocates an array of RequirementInfo and puts it into input.requirements
// Returns true iff the function succeeded, if false is returned then input will not be modified.
bool loadRequirementData(const char* inputFilename, InputData& input);

bool loadAllocationData(const char* inputFilename, AllocationInfo** allocations, int& allocationCount);

void computeAllocations(InputData input, int allocationCount, AllocationInfo* allocations);

// Serialize the sources, requirements and allocations into a JSON string and writes it to file
void writeOutputData(InputData input, int allocCount, AllocationInfo* allocations,
                     const char* outFilename);

#endif
