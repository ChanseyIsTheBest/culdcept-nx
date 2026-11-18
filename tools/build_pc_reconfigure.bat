@echo off
rem culdcept-nx: configure the PC build folder again (a new shader dump folder, a changed CMake option), then build it.
rem Usage: tools\build_pc_reconfigure.bat [-DNAME=VALUE ...]   (BUILD_DIR defaults to app\out\pc)
call "%~dp0pc_env.bat" || exit /b 1
set "ROOT=%~dp0.."
if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT%\app\out\pc"
cmake %* "%BUILD_DIR%" || exit /b 1
cmake --build "%BUILD_DIR%" -j 14
