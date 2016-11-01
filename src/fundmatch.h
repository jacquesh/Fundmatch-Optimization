#ifndef _FUNDMATCH_H
#define _FUNDMATCH_H

#include <vector>
#include <random>

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
    int amount;
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

struct AllocationPointer; // Forward-declare so we can use AllocationPointer*'s
struct Vector
{
    int dimensions;
    float* coords;

    float constraintViolation;
    float fitness;

    Vector();
    explicit Vector(int dimCount);
    Vector(const Vector& other);
    ~Vector();

    void processPositionUpdate(int allocCount, AllocationPointer* allocations);

    Vector& operator =(const Vector& other);
    float& operator [](int index) const;
};

struct AllocationPointer
{
    int sourceIndex;
    int requirementIndex;
    int balancePoolIndex;
    int allocStartDimension; // The index of the dimension where this allocation's data starts

    float getStartDate(const Vector& data) const;
    float getTenor(const Vector& data) const;
    float getAmount(const Vector& data) const;
    float getEndDate(const Vector& data) const;

    void setStartDate(Vector& data, float value);
    void setTenor(Vector& data, float value);
    void setAmount(Vector& data, float value);

    float getMinStartDate() const;
    float getMaxStartDate() const;
    float getMaxTenor(const Vector& data) const;
    float getMaxAmount(const Vector& data) const;
};

struct InputData
{
    std::vector<SourceInfo> sources;
    std::vector<BalancePoolInfo> balancePools;
    std::vector<RequirementInfo> requirements;

    std::vector<int> requirementsByStart;
    std::vector<int> requirementsByEnd;
};

extern InputData g_input;

// Allocates an array of SourceInfo, and puts it into input.sources.
// Returns true iff the function succeeded, if false is returned then input will not be modified.
bool loadSourceData(const char* inputFilename, InputData& input);

// Allocates an array of BalancePoolInfo, and puts it into input.balancePools.
// Returns true iff the function succeeded, if false is returned then input will not be modified.
bool loadBalancePoolData(const char* inputFilename, InputData& input);

// Allocates an array of RequirementInfo and puts it into input.requirements
// Returns true iff the function succeeded, if false is returned then input will not be modified.
bool loadRequirementData(const char* inputFilename, InputData& input);

Vector loadAllocationData(const char* inputFilename, std::vector<AllocationPointer>& allocations);

// Gives valid initial values to the given position vector, using the given random generators
void initializeAllocation(AllocationPointer& alloc, Vector& position, std::mt19937& rng);

// Returns the maximum sensible (and feasible) number of months to allocate from source to req
int maxAllocationTenor(SourceInfo& source, RequirementInfo& req);

// Returns the maximum feasible amount that be loaned as part of the given allocation
float maxAllocationAmount(Vector& position, int allocationCount, AllocationPointer* allocations, int allocID);

// Returns a Vector containing the final best solution for the parameters to be optimized
Vector computeAllocations(int allocationCount, AllocationPointer* allocations);

// Returns true iff the given position vector and allocation set is feasible
bool isFeasible(Vector& position, int allocationCount, AllocationPointer* allocations);

bool isPositionBetter(Vector& newPosition, Vector& testPosition, int allocationCount, AllocationPointer* allocations);
float measureConstraintViolation(Vector& position, int allocationCount, AllocationPointer* allocations);

// Returns the fitness (total interest cost) of the given position vector and allocation set
float computeFitness(Vector& position, int allocationCount, AllocationPointer* allocations);

// Serialize the sources, requirements and allocations into a JSON string and writes it to file
// Returns the number of non-empty requirements (>0 tenor and amount) that were written
int writeOutputData(InputData input, int allocCount, AllocationPointer* allocations,
                     Vector solution, const char* outFilename);

#endif
