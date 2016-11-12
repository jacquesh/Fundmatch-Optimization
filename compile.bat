@echo off

set CompileFlags= -nologo -Zi -GR- -Gm- -EHsc- -W4 -I../include -I../src -wd4100 -wd4189 -D_CRT_SECURE_NO_WARNINGS -DEBUG -O2 -Zo
set LinkFlags= -INCREMENTAL:NO

set HarnessSrcFiles=..\src\main.cpp ..\src\fundmatch.cpp ..\src\dataio.cpp ..\src\logging.cpp ..\src\Jzon.cpp
set HarnessObjFiles=main.obj fundmatch.obj dataio.obj logging.obj Jzon.obj


IF NOT EXIST build mkdir build
pushd build

REM Harness files
ctime -begin pso.ctm
cl -c %CompileFlags% %HarnessSrcFiles%

REM PSO
cl %CompileFlags% ..\src\pso.cpp %HarnessObjFiles% -link %LinkFlags%
ctime -end pso.ctm %ERRORLEVEL%

REM GA
cl %CompileFlags% ..\src\ga.cpp %HarnessObjFiles% -link %LinkFlags%

REM Heuristic
cl %CompileFlags% ..\src\heuristic.cpp %HarnessObjFiles% -link %LinkFlags%

REM Worst-case
cl %CompileFlags% ..\src\worstcase.cpp %HarnessObjFiles% -link %LinkFlags%
popd
