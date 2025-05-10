@echo off

set libs=-lkernel32 -luser32 -lgdi32.lib -ldwmapi.lib 

clang main.c -g -o build\main.exe %libs%
REM clang main.c -O3 -ffast-math -o build\main.exe %libs%

REM -Wall -Wextra
if %ERRORLEVEL% EQU 0 (
    pushd build
    call main.exe
    popd
) else (
    echo Compilation failed.
)
