@echo off

rem -lopengl32.lib -lwinmm.lib -lopengl32.lib -lgdi32.lib 

set libs=-lkernel32 -luser32 -lgdi32.lib -ldwmapi.lib
set linker=-Xlinker /subsystem:windows
rem set linker=-Xlinker /NODEFAULTLIB -Xlinker /entry:WinMainCRTStartup -Xlinker /subsystem:windows

set common=-Wall -Wextra

rem set conf=-O3
set conf=-g

if not exist build mkdir build

clang -Wall -Wextra main.cpp %linker% %conf% -o build\main.exe %libs%


@REM -Wall -Wextra
if %ERRORLEVEL% EQU 0 (
rem    call build\main.exe
) else (
    echo Compilation failed.
)

set CommonCompilerOptions=/nologo /GR- /FC /GS- /Gs9999999
set CompilerOptionsDev=/Zi /Od
set CompilerOptionsProd=/O2 
set LinkerOptions=/nodefaultlib /subsystem:windows /STACK:0x100000,0x100000 /incremental:no
set Libs=user32.lib gdi32.lib kernel32.lib dwmapi.lib

rem cl %CommonCompilerOptions% %CompilerOptionsProd% m.cpp /link %LinkerOptions% %Libs% 
    
@rem set CommonCompilerOptions=/nologo /GR- /FC /GS- /Gs9999999 /Feshell.exe
@rem set CompilerOptionsDev=/Zi /Od /W4
@rem set CompilerOptionsProd=/O2
@rem set LinkerOptions=/nodefaultlib /subsystem:windows /STACK:0x100000,0x100000 /incremental:no
@rem set Libs=user32.lib kernel32.lib gdi32.lib dwmapi.lib winmm.lib shell32.lib
@REM set Libs=user32.lib kernel32.lib gdi32.lib opengl32.lib dwmapi.lib winmm.lib shell32.lib ole32.lib uuid.lib ucrt.lib

