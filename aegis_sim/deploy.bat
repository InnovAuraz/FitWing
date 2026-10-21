@echo off
setlocal

rem ---- paths (edit if your layout differs) ------------------------------------
set PROJECT_DIR=%~dp0
set BUILD_DIR=%PROJECT_DIR%build_release\Release
set DIST_DIR=%PROJECT_DIR%dist
set EXE_NAME=FitWingSim.exe

rem Only needed if you link GLFW/GLEW dynamically (glfw3dll / glew32). With your
rem current CMake (glfw3 + glew32s + GLEW_STATIC) both are static -> leave empty.
set GLFW_DLL_DIR=
set GLEW_DLL_DIR=

rem ---- sanity check ------------------------------------------------------------
if not exist "%BUILD_DIR%\%EXE_NAME%" (
    echo ERROR: %BUILD_DIR%\%EXE_NAME% not found. Build the Release configuration first.
    exit /b 1
)

rem ---- fresh dist --------------------------------------------------------------
if exist "%DIST_DIR%" rmdir /S /Q "%DIST_DIR%"
mkdir "%DIST_DIR%"

rem ---- executable --------------------------------------------------------------
copy /Y "%BUILD_DIR%\%EXE_NAME%" "%DIST_DIR%\" >nul

rem ---- optional dynamic libraries ----------------------------------------------
if not "%GLFW_DLL_DIR%"=="" copy /Y "%GLFW_DLL_DIR%\glfw3.dll" "%DIST_DIR%\" >nul
if not "%GLEW_DLL_DIR%"=="" copy /Y "%GLEW_DLL_DIR%\glew32.dll" "%DIST_DIR%\" >nul

rem ---- runtime folders the simulator reads from next to the exe -----------------
xcopy /E /I /Y "%PROJECT_DIR%shaders" "%DIST_DIR%\shaders" >nul
xcopy /E /I /Y "%PROJECT_DIR%resources" "%DIST_DIR%\resources" >nul

echo Done: %DIST_DIR%
endlocal
