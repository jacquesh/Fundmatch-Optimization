#ifndef _FUNDMATCH_H
#define _FUNDMATCH_H

#include <vector>

const float RCF_INTEREST_RATE = 0.13f;
const float BALANCEPOOL_INTEREST_RATE = 0.11f;

static const int DIMENSIONS_PER_ALLOCATION = 3;

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
    int startDate;
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
    int startDate;
    int tenor;
    int amount;
    char* tier;
    char* purpose;
    TaxClass taxClass;
};

struct Vector
{
    int dimensions;
    float* coords;

    float& operator [](int index)
    {
        return coords[index];
    }

    Vector(int dimCount);
    Vector(const Vector& other);
    ~Vector();

    Vector& operator =(const Vector& other);
};

struct AllocationPointer
{
    int sourceIndex;
    int requirementIndex;
    int balanceIndex;
    int allocStartDimension; // The index of the dimension where this allocation's data starts

    float getStartDate(Vector& data);
    float getTenor(Vector& data);
    float getAmount(Vector& data);
    float getEndDate(Vector& data);

    void setStartDate(Vector& data, float value);
    void setTenor(Vector& data, float value);
    void setAmount(Vector& data, float value);
};

// TODO: Convert to vector<infotype> instead of arrays
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

//bool loadAllocationData(const char* inputFilename, AllocationInfo** allocations, int& allocationCount);

Vector computeAllocations(InputData input, int allocationCount, AllocationPointer* allocations);

// Serialize the sources, requirements and allocations into a JSON string and writes it to file
void writeOutputData(InputData input, int allocCount, AllocationPointer* allocations,
                     Vector solution, const char* outFilename);

#endif
