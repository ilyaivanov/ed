@echo off

if not exist build mkdir build

set libs=-lkernel32 -luser32 -lgdi32.lib -ldwmapi.lib

clang play.c -Xlinker /NODEFAULTLIB -shared -g -o build\play.dll %libs%
