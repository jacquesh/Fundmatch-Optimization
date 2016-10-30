from subprocess import check_output
import argparse
import re

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--iterations", default=3, type=int)
    parser.add_argument("-ds", "--data-set", default=["DSg_1"], type=str, nargs="*")
    parser.add_argument("-m", "--method", default=["heuristic"], type=str, nargs="*")
    args = parser.parse_args()

    possible_methods = ("heuristic", "pso", "ga")
    iterations = args.iterations

    for method in args.method:
        if method not in possible_methods:
            print("Method %s unrecognized, ignoring", method)
            continue

        print("Running tests for %s" % method)
        for data_set in args.data_set:
            print("Running %d iterations on dataset: %s" % (iterations, data_set))
            result = 0
            for i in range(iterations):
                output = check_output(["./build/%s" % method, data_set]).decode()
                fitnessStr = re.search(r"final fitness was (\d+\.\d+)", output).group(1)
                fitness = float(fitnessStr)
                result += fitness/iterations
                print("\tIteration %d: %f" % (i, fitness))
            print("\tAverage fitness: %f" % result)
            print("") # Get us an empty line
        print("\n\n")
