
@echo off

set libs=-lkernel32 -luser32 -lgdi32.lib -ldwmapi.lib -lopengl32.lib -lwinmm.lib

clang main.cpp -g -o main.exe %libs%
rem clang main2.cpp -O3 -ffast-math -o main.exe %libs%

@REM -Wall -Wextra
if %ERRORLEVEL% EQU 0 (
    call main.exe
) else (
    echo Compilation failed.
)

@rem set CommonCompilerOptions=/nologo /GR- /FC /GS- /Gs9999999 /Feshell.exe
@rem set CompilerOptionsDev=/Zi /Od /W4
@rem set CompilerOptionsProd=/O2
@rem set LinkerOptions=/nodefaultlib /subsystem:windows /STACK:0x100000,0x100000 /incremental:no
@rem set Libs=user32.lib kernel32.lib gdi32.lib dwmapi.lib winmm.lib shell32.lib
@REM set Libs=user32.lib kernel32.lib gdi32.lib opengl32.lib dwmapi.lib winmm.lib shell32.lib ole32.lib uuid.lib ucrt.lib
