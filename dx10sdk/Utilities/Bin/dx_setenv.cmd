@echo off

REM ** Pick a target platform.
REM **
REM ** First see if they requested something specifically, 
REM ** next try the Platform SDK cpu var, then default to 
REM ** the processor architecture of the host.

if "%1" == "amd64" Goto x64_Target
if "%1" == "Amd64" Goto x64_Target
if "%1" == "AMD64" Goto x64_Target
if "%1" == "x86" Goto  x86_Target
if "%1" == "X86" Goto  x86_Target
if "%1" == "i386" Goto  x86_Target
if "%1" == "I386" Goto  x86_Target
if "%CPU%" == "AMD64" Goto x64_Target
if "%CPU%" == "i386" Goto x86_Target
if "%PROCESSOR_ARCHITECTURE%" == "AMD64" Goto x64_Target
if "%PROCESSOR_ARCHITECTURE%" == "x86" Goto x86_Target
echo Could not detect target from command-line, CPU, or PROCESSOR_ARCHITECTURE, exiting ...
Goto Exit

:x64_Target
Set Lib=%DXSDK_DIR%Lib\x64;%Lib%
Set Include=%DXSDK_DIR%Include;%Include%
echo Dx x64 target enviroment is now enabled.
Goto Host

:x86_Target
Set Lib=%DXSDK_DIR%Lib\x86;%Lib%
Set Include=%DXSDK_DIR%Include;%Include%
echo Dx x86 target enviroment is now enabled.
Goto Host


:Host
REM ** Pick a host platform based on processor architecture.

if "%PROCESSOR_ARCHITECTURE%" == "AMD64" Goto x64_Host
if "%PROCESSOR_ARCHITECTURE%" == "x86" Goto x86_Host
echo Could not detect host from PROCESSOR_ARCHITECTURE, exiting ...
goto Exit

:x86_Host
Set Path=%DXSDK_DIR%Utilities\Bin\x86;%Path%
echo Dx x86 host enviroment is now enabled.
Goto Exit


:x64_Host
Set Path=%DXSDK_DIR%Utilities\Bin\x64;%DXSDK_DIR%Utilities\Bin\x86;%Path%
echo Dx x64 host enviroment is now enabled.
Goto Exit


:Exit
