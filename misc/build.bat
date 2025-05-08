@echo off
REM del main.exe
clang main.c -g -o build\main.exe -lkernel32 -luser32 -lgdi32.lib -ldwmapi.lib 

REM -O3 -ffast-math 
REM -Wall -Wextra
if %ERRORLEVEL% EQU 0 (
    pushd build
    call main.exe
    popd
) else (
    echo Compilation failed.
)
