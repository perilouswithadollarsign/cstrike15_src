:: //========== Copyright (c) Valve Corporation, All rights reserved. ========
:: //
:: // Script to run swig
:: //
:: // SYNTAX: swig_python.cmd <PYVER> <OUTBINDIR> <SWIGFILE> [AUTOSWIG]
:: //
:: //=========================================================================


@echo off
setlocal

:: // Make sure we have enough arguments
if .%1. == .. goto Usage
if .%2. == .. goto Usage
if .%3. == .. goto Usage

set AUTOSWIG=
if not .%4. == .. set AUTOSWIG=1

set PREFIX=[%~n0]
set SRCDIR=%~d0%~p0..
set PYVER=%1
set OUTBINDIR=%2
set SWIGFILE=%3
set SWIGOUTDIR=swig_python%PYVER%
set SWIGC=%SWIGFILE%_wrap_python%PYVER%.cpp
set SWIG=%SRCDIR%\devtools\swigwin-1.3.40\swig.exe
set AUTOSWIGSCRIPT=%~d0%~p0swig_auto_dme.pl
set PERL=%~d0

if NOT EXIST %SWIG% GOTO ErrorNoSwig

if EXIST %SWIGOUTDIR% GOTO SwigOutDirOk
MKDIR %SWIGOUTDIR%
if ERRORLEVEL 1 GOTO ErrorSwigOutDir

:SwigOutDirOk

if NOT DEFINED AUTOSWIG GOTO AutoSwigOk

set FOUNDPERL=
for %%P in ( wperl.exe ) do ( set FOUNDPERL=%%~$PATH:P)
if NOT DEFINED FOUNDPERL goto ErrorNoPerl

echo %PREFIX% Perl produces swigfile, swig_auto_dme.pl produces %SWIGOUTDIR%\auto_%SWIGFILE%.i
echo %PREFIX% "%FOUNDPERL%" "%AUTOSWIGSCRIPT%" "%SWIGOUTDIR%" "%SWIGFILE%"
"%FOUNDPERL%" "%AUTOSWIGSCRIPT%" "%SWIGOUTDIR%" "%SWIGFILE%"

:AutoSwigOk

if EXIST %OUTBINDIR% GOTO OutBinDirOk
MKDIR %OUTBINDIR%
if ERRORLEVEL 1 GOTO ErrorOutBinDir

:OutBinDirOk

if EXIST %SWIGOUTDIR%\%SWIGC% DEL %SWIGOUTDIR%\%SWIGC%

echo %PREFIX% %SWIG% -Fmicrosoft -small -ignoremissing -w312 -w325 -w383 -w503 -w509 -c++ -Iswig_python%PYVER% -I%SRCDIR%/public -outdir %SWIGOUTDIR% -o %SWIGOUTDIR%/%SWIGC% -python %SWIGFILE%.i"
%SWIG% -small -Fmicrosoft -ignoremissing -w312 -w325 -w383 -w503 -w509 -w401 -c++ -Iswig_python%PYVER% -I%SRCDIR%/public -outdir %SWIGOUTDIR% -o %SWIGOUTDIR%/%SWIGC% -python %SWIGFILE%.i"

if ERRORLEVEL 1 goto ErrorSwig

:EndOk
endlocal
exit /b 0

:Usage
echo.
echo NAME
echo     %~n0
echo.
echo SYNOPSIS
echo     %0 ^<SRCDIR^> ^<PYVER^> ^<SWIGFILE^> ^<SWIGBINDIR^>
echo.
echo DESCRIPTION
echo     Runs swig on the specified swig file for the specified python version
echo.
goto EndError

:ErrorNoSwig
echo %PREFIX% Error! No swig executable found here: %SWIG%
goto EndError

:ErrorSwigOutDir
echo %PREFIX% Error! Swig Output Dir doesn't exist and could not be created: %SWIGOUTDIR%
goto EndError

:ErrorOutBinDir
echo %PREFIX% Error! Swig Output Bin Dir doesn't exist and could not be created: %OUTBINDIR%
goto EndError

:ErrorSwig
echo %PREFIX% Error! Swig filed"
goto EndError

:ErrorNoPerl
echo %PREFIX% Error! No perl.exe executable found in PATH"

:EndError
endlocal
exit 1
