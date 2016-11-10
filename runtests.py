from subprocess import check_output
from os.path import isfile
import time
import argparse
import re

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

    for index, data_set in enumerate(args.data_set):
        if not isfile("./data/%s_requirements.csv" % data_set):
            print("Data set %s unrecognized, ignoring" % data_set)
            del args.data_set[index]

    for data_set in args.data_set:
        print("Running tests for dataset %s" % data_set)
        for f in plot_files:
            f.write(data_set)
        for method in args.method:
            print("Running %d iterations using %s" % (iterations, method))
            averageFitness = 0
            averageRuntime = 0
            averageAllocations = 0
            validResultCount = 0
            for i in range(iterations):
                startTime = time.time()
                output = check_output(["./build/%s" % method, data_set]).decode()
                endTime = time.time()
                regexMatch = re.search(r"fitness was (-?\d+\.\d+) from (\d+) allocations", output)
                fitnessStr = regexMatch.group(1)
                fitness = float(fitnessStr)
                allocCountStr = regexMatch.group(2)
                allocCount = int(allocCountStr)
                if(fitness > 0):
                    averageFitness += fitness
                    averageAllocations += allocCount
                    averageRuntime += endTime-startTime
                    validResultCount += 1
                print("\tIteration %d: %f" % (i, fitness))
            if(validResultCount > 0):
                averageFitness /= validResultCount
                averageAllocations /= validResultCount
                averageRuntime /= validResultCount
            else:
                averageFitness = 0
                averageAllocations = 0
                averageRuntime = 0
            fitness_plot_file.write(" %.2f" % averageFitness)
            alloc_count_plot_file.write(" %.2f" % averageAllocations)
            runtime_plot_file.write(" %.2f" % averageRuntime)
            print("\tAverage fitness: %.2f" % averageFitness)
            print("") # Get us an empty line
        for f in plot_files:
            f.write("\n")
        print("\n\n")
