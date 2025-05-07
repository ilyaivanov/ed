@echo off
REM del main.exe
clang main.c -g -O3 -ffast-math -o build\main.exe -lkernel32 -luser32 -lgdi32.lib -ldwmapi.lib 

REM -Wall -Wextra
if %ERRORLEVEL% EQU 0 (
    pushd build
    call main.exe
    popd
) else (
    echo Compilation failed.
)
