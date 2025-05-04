@echo off
REM del main.exe
clang main.c -g -o build\main.exe -lkernel32 -luser32 -lgdi32.lib -ldwmapi.lib 
pushd build

REM -Wall -Wextra
if %ERRORLEVEL% EQU 0 (
    call main.exe
) else (
    echo Compilation failed.
)
popd
