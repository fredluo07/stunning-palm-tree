@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"

echo === 暗影迷宫 / Dark Maze 一键编译 ===

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [错误] 未找到 vswhere.exe。请安装 Visual Studio（勾选"使用 C++ 的桌面开发"），或使用 CMake 构建。
    pause
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%i"
if "%VSPATH%"=="" (
    echo [错误] 未检测到 Visual Studio 安装。
    pause
    exit /b 1
)
echo 使用 Visual Studio: %VSPATH%
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [错误] 无法初始化 MSVC 编译环境。
    pause
    exit /b 1
)

cl /nologo /std:c++17 /EHsc /O2 /utf-8 /W3 /permissive- ^
   /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX ^
   src\main.cpp /Fe:DarkMaze.exe ^
   /link opengl32.lib gdi32.lib user32.lib winmm.lib gdiplus.lib shlwapi.lib /SUBSYSTEM:WINDOWS

if errorlevel 1 (
    echo [错误] 编译失败，请查看上方输出。
    pause
    exit /b 1
)
echo.
echo 编译完成：DarkMaze.exe
echo 运行：直接双击 DarkMaze.exe
pause
