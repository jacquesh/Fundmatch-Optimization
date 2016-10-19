@echo off

set CompileFlags= -nologo -Zi -GR- -Gm- -EHsc- -W4 -I../include -I../src -wd4100 -wd4189 -D_CRT_SECURE_NO_WARNINGS
set LinkFlags= -INCREMENTAL:NO

set CompileFiles=..\src\main.cpp ..\src\fundmatch.cpp ..\src\platform.cpp ..\src\logging.cpp ..\src\Jzon.cpp

IF NOT EXIST build mkdir build
pushd build

REM PSO
ctime -begin pso.ctm
cl %CompileFlags% ..\src\pso.cpp %CompileFiles% -link %LinkFlags%
ctime -end pso.ctm %ERRORLEVEL%

REM GA
cl %CompileFlags% ..\src\ga.cpp %CompileFiles% -link %LinkFlags%
popd
