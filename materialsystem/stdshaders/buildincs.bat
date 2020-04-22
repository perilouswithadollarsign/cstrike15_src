@echo off

set x360_args=
set ps3_args=

if not exist %2.txt goto error1
if /i "%1" == "-x360" goto set_x360_args
if /i "%1" == "-ps3" goto set_ps3_args

echo.
echo No valid platform specified for first argument; specify either -ps3 or -x360
echo.

goto usage

:set_x360_args
set x360_args=-x360
set SHADERINCPATH=vshtmp9_360/... fxctmp9_360/...
goto build_incs

:set_ps3_args
set ps3_args=-ps3
set SHADERINCPATH=vshtmp9_ps3/... fxctmp9_ps3/...
goto build_incs

:build_incs

updateshaders.pl %1 -source ..\.. %2

nmake makefile.%2

copyshaderincfiles.pl inclist.txt %1

set SHADERINCPATH=vshtmp9/... fxctmp9/...
p4autocheckout.pl inclist.txt "Shader Auto Checkout INC" . %SHADERINCPATH%

goto end

:error1

echo.
echo File %2.txt does not exist
echo.

goto usage

:usage

echo.
echo "Usage: buildincs.bat [-x360|-ps3] [shader list]"
echo .
goto end

:end

