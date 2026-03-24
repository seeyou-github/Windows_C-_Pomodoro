@echo off
setlocal

echo ===== Build Windows_C++_Pomodoro =====

if not exist build mkdir build

set CXX=g++
set RC=windres
set CXXFLAGS=-std=c++17 -DUNICODE -D_UNICODE -Isrc -Isrc\res
set LDFLAGS=-municode -mwindows -lgdi32 -lcomctl32 -lshell32 -lole32 -loleaut32 -luuid -ldwmapi -luxtheme

%CXX% -c src\main.cpp %CXXFLAGS% -o build\main.o
if errorlevel 1 goto error

%CXX% -c src\MainWindow.cpp %CXXFLAGS% -o build\MainWindow.o
if errorlevel 1 goto error

%CXX% -c src\PomodoroEngine.cpp %CXXFLAGS% -o build\PomodoroEngine.o
if errorlevel 1 goto error

%CXX% -c src\ResourceLoader.cpp %CXXFLAGS% -o build\ResourceLoader.o
if errorlevel 1 goto error

%CXX% -c src\OverlayWindows.cpp %CXXFLAGS% -o build\OverlayWindows.o
if errorlevel 1 goto error

%RC% src\res\resource.rc -O coff -o build\resource.o
if errorlevel 1 goto error

echo Linking...
%CXX% build\main.o build\MainWindow.o build\PomodoroEngine.o build\ResourceLoader.o build\OverlayWindows.o build\resource.o -o build\PomodoroTimer.exe %LDFLAGS%
if errorlevel 1 goto error

echo ===== Build succeeded =====
echo Output: build\PomodoroTimer.exe
goto end

:error
echo ===== Build failed =====
exit /b 1

:end
endlocal
