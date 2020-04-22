:: //========== Copyright (c) Valve Corporation, All rights reserved. ========
:: //
:: // Script to install the .py output of a swig build.  Handles p4 edit
:: // and diffs file so only actual changes get checked in
:: //
:: // Could actually be used to p4 edit & copy with a diff any file to
:: // a destination file
:: //
:: // NOTE: Expects to be installed in SRCDIR\vpc_scripts\swig_installcmd
:: //       as it derives the path to SRCDIR from the path to itself
:: //
:: // SYNTAX: swig_install.cmd <SRCFILE> <DSTFILE>
:: //
:: //=========================================================================


@echo off
setlocal

:: // Make sure we have enough arguments
if .%1. == .. goto Usage
if .%2. == .. goto Usage

set PREFIX=[%~n0]
set SRCDIR=%~d0%~p0..
set DIFF=%SRCDIR%\devtools\bin\diff.exe
set SED=%SRCDIR%\devtools\bin\sed.exe
set P4EDIT=%~d0%~p0valve_p4_edit.cmd
set SRC=%~f1
set DST=%~f2
set DSTDIR=%~d2%~p2

if NOT EXIST %DIFF% GOTO ErrorNoDiff

if NOT EXIST %DIFF% GOTO ErrorNoSed

if NOT EXIST %P4EDIT% GOTO ErrorNoP4Edit

if NOT EXIST %SRC% GOTO ErrorNoSrc

if EXIST %DSTDIR% GOTO DstDirOk
MKDIR %DSTDIR%
if ERRORLEVEL 1 GOTO ErrorDstDir

:DstDirOk

set bADD=0

if NOT EXIST %DST% GOTO DoAdd
echo %PREFIX% "%DIFF%" -q "%SRC%" "%DST%"
::
:: This is horrible but Incredibuild seems to intercept ERRORLEVEL and for sub processes launched from batch files
:: and flag them as RED which sucks... because they're not really errors.  So do a wacky way to parse the output
:: diff -q will return nothing if files are the same and 'Files A and B differ' if they are different
:: So if there's no output, they are the same, if there's any output they are different
::
:: The sed is there to simply clear ERRORLEVEL but echos all output untouched
::
set sDIFFOUT=
FOR /F "tokens=*" %%A in ( '%DIFF% -q %SRC% %DST% ^| %SED% -e ""' ) do SET sDIFFOUT=%%A
if defined sDIFFOUT goto DoEdit

:DoSame
echo %PREFIX% Ok
GOTO EndOk

:DoAdd
set bADD=1
goto DoCopy

:DoEdit
echo %PREFIX% "%P4EDIT%" %DST% %SRCDIR%
call "%P4EDIT%" %DST% %SRCDIR%
IF ERRORLEVEL 1 goto ErrorP4Edit

:DoCopy
echo %PREFIX% COPY /Y "%SRC%" "%DST%"
COPY /Y %SRC% %DST%
IF ERRORLEVEL 1 goto ErrorCopy
if %bADD%==0 goto EndOk

echo %PREFIX% "%P4EDIT%" %DST% %SRCDIR%
call "%P4EDIT%" %DST% %SRCDIR%
IF ERRORLEVEL 1 goto ErrorP4Add

:EndOk
endlocal
exit /b 0

:Usage
echo.
echo NAME
echo     %~n0
echo.
echo SYNOPSIS
echo     %0 ^<SRCFILE^> ^<DSTFILE^>
echo.
echo DESCRIPTION
echo     Copies ^<SRCFILE^> to ^<DSTFILE^> opening it for p4 edit if necessary
echo.
echo NOTES
echo     Files are not automatically added to perforce, only files already
echo     controlled by perforce will be edited but no add operations are done
echo.
goto EndError

:ErrorNoDiff
echo %PREFIX% Error! No diff executable found here: %DIFF%
goto EndError

:ErrorNoSed
echo %PREFIX% Error! No sed executable found here: %SED%
goto EndError

:ErrorNoP4Edit
echo %PREFIX% Error! No valve_p4_edit.cmd found here: %P4EDIT%
goto EndError

:ErrorNoSrc
echo %PREFIX% Error! No source file found: %SRC%
goto EndError

:ErrorDstDir
echo %PREFIX% Error! Destination directory doesn't exist and could not be created: %DSTDIR%
goto EndError

:ErrorCopy
echo %PREFIX% Error! Copy "%SRC%" "%DST%" failed
goto EndError

:ErrorP4Edit
echo %PREFIX% Error! "%P4EDIT%" %DST% %SRCDIR% Failed
goto EndError

:ErrorP4Add
echo %PREFIX% Error! "%P4EDIT%" %DST% %SRCDIR% Failed
goto EndError

:EndError
endlocal
exit 1
