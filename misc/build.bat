@echo off

set libs=-lkernel32 -luser32 -lgdi32.lib -ldwmapi.lib -ldbghelp.lib

clang main.c -g -o main.exe %libs%
rem clang main.c -O3 -ffast-math -o main.exe %libs%

REM -Wall -Wextra
if %ERRORLEVEL% EQU 0 (
    rem pushd build
    call main.exe
    rem popd
) else (
    echo Compilation failed.
)
