@echo off
set libs=-lkernel32 -luser32 -lgdi32.lib -ldwmapi.lib 

clang play.c -g -o main2.exe %libs%

REM clang play.c -O3 -ffast-math -o main2.exe %libs%

REM -Wall -Wextra
if %ERRORLEVEL% EQU 0 (
    call main2.exe
) else (
    echo Compilation failed.
)
