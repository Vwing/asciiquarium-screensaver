@echo off
setlocal

cd /d "%~dp0"

:: Detect Visual Studio via vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Install Visual Studio 2019 or 2022.
    exit /b 1
)

for /f "usebackq tokens=*" %%i in (
    `"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`
) do set "MSBUILD=%%i"

if not defined MSBUILD (
    echo ERROR: MSBuild not found.
    exit /b 1
)

:: Configure and build
if not exist build mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    cmake .. -G "Visual Studio 16 2019" -A x64
    if errorlevel 1 (
        echo ERROR: CMake configuration failed.
        exit /b 1
    )
)

"%MSBUILD%" AsciiquariumScreensaver.sln /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 (
    echo ERROR: Build failed.
    exit /b 1
)

echo.
echo ============================================================
echo  Build succeeded.
echo  Output: build\Release\Asciiquarium.scr
echo          build\Release\AsciiquariumApp.exe
echo.
echo  To install:
echo    Right-click Asciiquarium.scr ^> Install
echo    (Keep AsciiquariumApp.exe in the same folder)
echo ============================================================
endlocal
