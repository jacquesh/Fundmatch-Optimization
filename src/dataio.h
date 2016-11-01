#ifndef _DATA_IO_H
#define _DATA_IO_H

#include "fundmatch.h"

// Allocates an array of SourceInfo, and puts it into input.sources.
// Returns true iff the function succeeded, if false is returned then input will not be modified.
bool loadSourceData(const char* inputFilename, InputData& input);

// Allocates an array of BalancePoolInfo, and puts it into input.balancePools.
// Returns true iff the function succeeded, if false is returned then input will not be modified.
bool loadBalancePoolData(const char* inputFilename, InputData& input);

// Allocates an array of RequirementInfo and puts it into input.requirements
// Returns true iff the function succeeded, if false is returned then input will not be modified.
bool loadRequirementData(const char* inputFilename, InputData& input);

// Loads allocations from a csv file. Fills AllocationPointer vector with AllocationPointers for each
// allocation, and returns the Vector with the values from the file that correspond to those pointers
Vector loadAllocationData(const char* inputFilename, std::vector<AllocationPointer>& allocations);

// Serialize the sources, requirements and allocations into a JSON string and writes it to file
// Returns the number of non-empty requirements (>0 tenor and amount) that were written
int writeOutputData(InputData input, int allocCount, AllocationPointer* allocations,
                     Vector solution, const char* outFilename);

#endif
