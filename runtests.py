from subprocess import check_output
from os.path import isfile
import time
import argparse
import re

def avg(lst):
    return sum(lst)/len(lst)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--iterations", default=3, type=int)
    parser.add_argument("-ds", "--data-set", default=["DSg_1"], type=str, nargs="*")
    parser.add_argument("-m", "--method", default=["heuristic"], type=str, nargs="*")
    args = parser.parse_args()

    fitness_plot_file = open("fitness_comparison.dat", "w")
    runtime_plot_file = open("runtime_comparison.dat", "w")
    alloc_count_plot_file = open("alloc_count_comparison.dat", "w")

    plot_files = [fitness_plot_file, alloc_count_plot_file, runtime_plot_file]
    for f in plot_files:
        f.write("Dataset")

    iterations = args.iterations
    for index, method in enumerate(args.method):
        if not isfile("./build/%s.exe" % method) and not isfile("./build/%s" % method):
            print("Method %s unrecognized, ignoring" % method)
            del args.method[index]
        else:
            for f in plot_files:
                f.write(" %s" % method)
    for f in plot_files:
        f.write("\n")

    ds_requirement_count = [-1 for ds in args.data_set]
    for index, data_set in enumerate(args.data_set):
        require_filepath = "./data/%s_requirements.csv" % data_set
        if not isfile(require_filepath):
            print("Data set %s unrecognized, ignoring" % data_set)
            del args.data_set[index]
        else:
            with open(require_filepath) as req_file:
                require_text = req_file.read()
                ds_requirement_count[index] = require_text.count("\n")-1

    for req_count, data_set in zip(ds_requirement_count, args.data_set):
        worstcaseOutput = check_output(["./build/worstcase", data_set]).decode()
        regexMatch = re.search(r"fitness was (-?\d+\.\d+) from (\d+) allocations", worstcaseOutput)
        worstFitness = float(regexMatch.group(1))
        print("Running tests for dataset %s, normalized to %.2f" % (data_set, worstFitness))
        for f in plot_files:
            f.write(data_set)
        for method in args.method:
            print("Running %d iterations using %s" % (iterations, method))
            fitnesses = []
            runtimes = []
            allocCounts = []
            for i in range(iterations):
                startTime = time.time()
                output = check_output(["./build/%s" % method, data_set]).decode()
                endTime = time.time()
                regexMatch = re.search(r"fitness was (-?\d+\.\d+) from (\d+) allocations", output)
                fitness = float(regexMatch.group(1))
                allocCount = int(regexMatch.group(2))
                if(fitness > 0):
                    fitnesses.append(fitness)
                    runtimes.append(endTime-startTime)
                    allocCounts.append(allocCount)
                print("\tIteration %d: %f" % (i, fitness))

            minFitness = maxFitness = avgFitness = 0
            minAllocations = maxAllocations = avgAllocations = 0
            minRuntime = maxRuntime = avgRuntime = 0
            if len(fitnesses) > 0:
                minFitness = min(fitnesses)/worstFitness
                maxFitness = max(fitnesses)/worstFitness
                avgFitness = avg(fitnesses)/worstFitness
                minAllocs = min(allocCounts)/req_count
                maxAllocs = max(allocCounts)/req_count
                avgAllocs = avg(allocCounts)/req_count
                minRuntime = min(runtimes)
                maxRuntime = max(runtimes)
                avgRuntime = avg(runtimes)
            fitness_plot_file.write(" %.2f %.2f %.2f" % (avgFitness, minFitness, maxFitness))
            alloc_count_plot_file.write(" %.2f %.2f %.2f" % (avgAllocs, minAllocs, maxAllocs))
            runtime_plot_file.write(" %.2f %.2f %.2f" % (avgRuntime, minRuntime, maxRuntime))
            print("\tAverage fitness: %.2f" % avg(fitnesses))
            print("") # Get us an empty line
        for f in plot_files:
            f.write("\n")
        print("\n\n")
