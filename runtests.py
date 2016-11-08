from subprocess import check_output
from os.path import isfile
import argparse
import re

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--iterations", default=3, type=int)
    parser.add_argument("-ds", "--data-set", default=["DSg_1"], type=str, nargs="*")
    parser.add_argument("-m", "--method", default=["heuristic"], type=str, nargs="*")
    args = parser.parse_args()

    plot_file = open("comparison_test.dat", "w")
    plot_file.write("Dataset")

    iterations = args.iterations
    for index, method in enumerate(args.method):
        if not isfile("./build/%s.exe" % method):
            print("Method %s unrecognized, ignoring" % method)
            del args.method[index]
        else:
            plot_file.write(" %s" % method)
    plot_file.write("\n")

    for index, data_set in enumerate(args.data_set):
        if not isfile("./data/%s_requirements.csv" % data_set):
            print("Data set %s unrecognized, ignoring" % data_set)
            del args.data_set[index]

    for data_set in args.data_set:
        print("Running tests for dataset %s" % data_set)
        plot_file.write(data_set)
        for method in args.method:
            print("Running %d iterations using %s" % (iterations, method))
            result = 0
            validResultCount = 0
            for i in range(iterations):
                output = check_output(["./build/%s" % method, data_set]).decode()
                fitnessStr = re.search(r"final fitness was (-?\d+\.\d+)", output).group(1)
                fitness = float(fitnessStr)
                if(fitness > 0):
                    result += fitness
                    validResultCount += 1
                print("\tIteration %d: %f" % (i, fitness))
            if(validResultCount > 0):
                result /= validResultCount
            else:
                result = -1
            plot_file.write(" %.2f" % result)
            print("\tAverage fitness: %f" % result)
            print("") # Get us an empty line
        plot_file.write("\n")
        print("\n\n")
