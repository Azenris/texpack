@echo OFF

cls

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=ALL"

if not defined VCPKG_ROOT (
	echo [WARN] Setup enviroment variable: VCPKG_ROOT
	exit /b
)

setlocal EnableDelayedExpansion

set "PROJ_ROOT=%~dp0"
pushd "%PROJ_ROOT%"

if not exist "build" mkdir "build"
cd "build"

cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake

if %ERRORLEVEL% NEQ 0 (
	exit /b %ERRORLEVEL%
)

if "%CONFIG%"=="ALL" (
	cmake --build . --config Debug
	cmake --build . --config Release
) else (
	cmake --build . --config %CONFIG%
)

popd