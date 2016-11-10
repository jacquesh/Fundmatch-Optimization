CompileFlags="-std=c++11 -I ./src -O2"
HarnessSrcFiles="src/main.cpp src/fundmatch.cpp src/dataio.cpp src/logging.cpp src/Jzon.cpp"
HarnessObjFiles="main.o fundmatch.o dataio.o logging.o Jzon.o"

mkdir -p build
g++ -c $CompileFlags $HarnessSrcFiles
g++ $CompileFlags -o build/pso src/pso.cpp $HarnessObjFiles
g++ $CompileFlags -o build/ga src/ga.cpp $HarnessObjFiles
g++ $CompileFlags -o build/heuristic src/heuristic.cpp $HarnessObjFiles
rm *.o
