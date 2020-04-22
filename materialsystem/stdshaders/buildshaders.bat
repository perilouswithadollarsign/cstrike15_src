@echo off

set TTEXE=..\..\devtools\bin\timeprecise.exe
if not exist %TTEXE% goto no_ttexe
goto no_ttexe_end

:no_ttexe
set TTEXE=time /t
:no_ttexe_end

echo.
rem echo ==================== buildshaders %* ==================
%TTEXE% -cur-Q
set tt_start=%ERRORLEVEL%
set tt_chkpt=%tt_start%


REM ****************
REM usage: buildshaders <shaderProjectName> [-x360|-ps3]
REM ****************

setlocal
set arg_filename=%1
rem set shadercompilecommand=echo shadercompile.exe -mpi_graphics -mpi_TrackEvents
set shadercompilecommand=shadercompile.exe
set shadercompileworkers=140
set x360_args=
set ps3_args=
set ps3_additional_args=
set targetdir=..\..\..\game\platform\shaders
set SrcDirBase=..\..
set ChangeToDir=../../../game/bin
set shaderDir=shaders
set SDKArgs=
set SHADERINCPATH=vshtmp9/... fxctmp9/...


set DIRECTX_SDK_VER=pc09.00
set DIRECTX_SDK_BIN_DIR=dx9sdk\utilities

if /i "%2" == "-x360" goto dx_sdk_x360
if /i "%2" == "-ps3" goto dx_sdk_ps3
if /i "%2" == "-dx9_30" goto dx_sdk_dx9_30
if /i "%2" == "-dx10" goto dx_sdk_dx10
goto dx_sdk_end
:dx_sdk_x360
			set DIRECTX_SDK_VER=x360.00
			set DIRECTX_SDK_BIN_DIR=x360xdk\bin\win32
			goto dx_sdk_end
:dx_sdk_dx9_30
			set DIRECTX_SDK_VER=pc09.30
			set DIRECTX_SDK_BIN_DIR=dx10sdk\utilities\dx9_30
			goto dx_sdk_end
:dx_sdk_dx10
			set DIRECTX_SDK_VER=pc10.00
			set DIRECTX_SDK_BIN_DIR=dx10sdk\utilities\dx10_40
			goto dx_sdk_end
:dx_sdk_ps3
			set DIRECTX_SDK_VER=ps3
			set DIRECTX_SDK_BIN_DIR=ps3sdk\utilities
			goto dx_sdk_end
:dx_sdk_end

if "%1" == "" goto usage
set inputbase=%1

if /i "%3" == "-force30" goto set_force30_arg
goto set_force_end
:set_force30_arg
			set DIRECTX_FORCE_MODEL=30
			goto set_force_end
:set_force_end

if /i "%2" == "-x360" goto set_x360_args
if /i "%2" == "-ps3" goto set_ps3_args
if /i "%2" == "-game" goto set_mod_args
if /i "%2" == "-nompi" set SDKArgs=-nompi
goto build_shaders

REM ****************
REM USAGE
REM ****************
:usage
echo.
echo "usage: buildshaders <shaderProjectName> [-ps3 or -x360 or -dx10 or -game] [gameDir if -game was specified] [-nompi if -ps3 or -x360 was specified] [-source sourceDir]"
echo "       gameDir is where gameinfo.txt is (where it will store the compiled shaders)."
echo "       sourceDir is where the source code is (where it will find scripts and compilers)."
echo "ex   : buildshaders myshaders"
echo "ex   : buildshaders myshaders -game c:\steam\steamapps\sourcemods\mymod -source c:\mymod\src"
echo "ex   : buildshaders myshaders -x360 -nompi"
echo.
echo "PS3 specific parameters (mutually exclusive, and must directly follow the -ps3 parameter):"
echo "-ps3debug - Generate PS3 debug information (only do this if you have an SSD - see the wiki)"
echo "-ps3scheduleoptimization - Find optimal fragment shader compiler scheduler settings (this takes a LONG time!)"
goto :end

REM ****************
REM X360 ARGS
REM ****************
:set_x360_args
set x360_args=-x360
set SDKArgs=
if /i "%3" == "-nompi" set SDKArgs=-nompi
set SHADERINCPATH=vshtmp9_360/... fxctmp9_360/...
goto build_shaders

REM ****************
REM PS3 ARGS
REM ****************
:set_ps3_args
set ps3_args=-ps3
REM PS3 does not support MPI yet
set SDKArgs=
if /i "%3" == "-nompi" set SDKArgs=-nompi
if /i "%3" == "-ps3scheduleoptimization" set ps3_additional_args=-ps3optimizeschedules
if /i "%3" == "-ps3debug" set ps3_additional_args=-ps3debug
set SHADERINCPATH=vshtmp9_ps3/... fxctmp9_ps3/...
goto build_shaders

REM ****************
REM MOD ARGS - look for -game or the vproject environment variable
REM ****************
:set_mod_args

if not exist %sourcesdk%\bin\shadercompile.exe goto NoShaderCompile
set ChangeToDir=%sourcesdk%\bin

if /i "%4" NEQ "-source" goto NoSourceDirSpecified
set SrcDirBase=%~5

REM ** use the -game parameter to tell us where to put the files
set targetdir=%~3\shaders
set SDKArgs=-nompi -game "%~3"

if not exist "%~3\gameinfo.txt" goto InvalidGameDirectory
goto build_shaders

REM ****************
REM ERRORS
REM ****************
:InvalidGameDirectory
echo -
echo Error: "%~3" is not a valid game directory.
echo (The -game directory must have a gameinfo.txt file)
echo -
goto end

:NoSourceDirSpecified
echo ERROR: If you specify -game on the command line, you must specify -source.
goto usage
goto end

:NoShaderCompile
echo -
echo - ERROR: shadercompile.exe doesn't exist in %sourcesdk%\bin
echo -
goto end

REM ****************
REM BUILD SHADERS
REM ****************
:build_shaders

rem echo --------------------------------
rem echo %inputbase%
rem echo --------------------------------
REM make sure that target dirs exist
REM files will be built in these targets and copied to their final destination
if not exist %shaderDir% mkdir %shaderDir%
if not exist %shaderDir%\fxc mkdir %shaderDir%\fxc
if not exist %shaderDir%\vsh mkdir %shaderDir%\vsh
if not exist %shaderDir%\psh mkdir %shaderDir%\psh
REM Nuke some files that we will add to later.
if exist filelist.txt del /f /q filelist.txt
if exist filestocopy.txt del /f /q filestocopy.txt
if exist filelistgen.txt del /f /q filelistgen.txt
if exist inclist.txt del /f /q inclist.txt
if exist vcslist.txt del /f /q vcslist.txt
if exist makefile.%inputbase%.copy del /f /q makefile.%inputbase%.copy

REM ****************
REM Revert any targets (vcs or inc) that are opened for integrate.
REM ****************
perl "%SrcDirBase%\devtools\bin\p4revertshadertargets.pl" %x360_args% %ps3_args% -source "%SrcDirBase%" %inputbase%

REM ****************
REM Generate a makefile for the shader project
REM ****************
perl "%SrcDirBase%\devtools\bin\updateshaders.pl" %x360_args% %ps3_args% -source "%SrcDirBase%" %inputbase%


REM ****************
REM Run the makefile, generating minimal work/build list for fxc files, go ahead and compile vsh and psh files.
REM ****************
rem nmake /S /C -f makefile.%inputbase% clean > clean.txt 2>&1
echo Building inc files, asm vcs files, and VMPI worklist for %inputbase%...
nmake /S /C -f makefile.%inputbase%

REM ****************
REM Copy the inc files to their target
REM ****************
if exist "inclist.txt" (
	echo Publishing shader inc files to target...
	perl %SrcDirBase%\devtools\bin\copyshaderincfiles.pl inclist.txt %x360_args% %ps3_args%
)

REM ****************
REM Deal with perforce operations for inc files
REM ****************
if exist inclist.txt if not "%VALVE_NO_AUTO_P4_SHADERS%" == "1" (
	echo Executing perforce operations on .inc files.
	perl ..\..\devtools\bin\p4autocheckout.pl inclist.txt "Shader Auto Checkout INC" . %SHADERINCPATH%
)

REM ****************
REM Add the executables to the worklist.
REM ****************
if /i "%DIRECTX_SDK_VER%" == "pc09.00" (
	rem echo "Copy extra files for dx 9 std
	echo %SrcDirBase%\..\game\bin\d3dx9_33.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dx9_43.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dx10_33.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dx10_43.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dcompiler_41.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dcompiler_43.dll >> filestocopy.txt
)
if /i "%DIRECTX_SDK_VER%" == "pc09.30" (
	echo %SrcDirBase%\..\game\bin\d3dx9_33.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dx9_43.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dx10_33.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dx10_43.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dcompiler_41.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dcompiler_43.dll >> filestocopy.txt
)
if /i "%DIRECTX_SDK_VER%" == "pc10.00" (
	echo %SrcDirBase%\..\game\bin\d3dx9_33.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dx9_43.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dx10_33.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dx10_43.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dcompiler_41.dll >> filestocopy.txt
	echo %SrcDirBase%\..\game\bin\d3dcompiler_43.dll >> filestocopy.txt
)
if /i "%DIRECTX_SDK_VER%" == "x360.00" (
	rem echo "Copy extra files for xbox360
)
if /i "%DIRECTX_SDK_VER%" == "ps3" (
	echo %SrcDirBase%\ps3sdk\utilities\libcgc.dll >> filestocopy.txt
	echo %SrcDirBase%\ps3sdk\utilities\sceshaderperf.dll >> filestocopy.txt
	echo %SrcDirBase%\ps3sdk\utilities\nvshaderperf.dll >> filestocopy.txt
	echo %SrcDirBase%\ps3sdk\utilities\nvshaderperf_rsx.dll >> filestocopy.txt
	if exist "%SrcDirBase%\materialsystem\stdshaders\ps3optimalschedules.bin" echo %SrcDirBase%\materialsystem\stdshaders\ps3optimalschedules.bin >> filestocopy.txt
	if exist "%ChangeToDir%\ps3compilelog.txt" move /Y "%ChangeToDir%\ps3compilelog.txt" "%ChangeToDir%\ps3compilelog_prev.txt"
	echo ***** Start of Log >> %ChangeToDir%\ps3compilelog.txt
)

echo %SrcDirBase%\%DIRECTX_SDK_BIN_DIR%\dx_proxy.dll >> filestocopy.txt

echo %SrcDirBase%\..\game\bin\shadercompile.exe >> filestocopy.txt
echo %SrcDirBase%\..\game\bin\shadercompile_dll.dll >> filestocopy.txt
echo %SrcDirBase%\..\game\bin\vstdlib.dll >> filestocopy.txt
echo %SrcDirBase%\..\game\bin\tier0.dll >> filestocopy.txt

REM ****************
REM Cull duplicate entries in work/build list
REM ****************
if exist filestocopy.txt type filestocopy.txt | perl "%SrcDirBase%\devtools\bin\uniqifylist.pl" > uniquefilestocopy.txt
if exist filelistgen.txt if not "%dynamic_shaders%" == "1" (
    echo Generating action list...
    copy filelistgen.txt filelist.txt >nul
    rem %SrcDirBase%\devtools\bin\fxccombogen.exe <filelistgen.txt 1>nul 2>filelist.txt
)

REM ****************
REM Execute distributed process on work/build list
REM ****************

set shader_path_cd=%cd%
if exist "filelist.txt" if exist "uniquefilestocopy.txt" if not "%dynamic_shaders%" == "1" (
	echo Running distributed shader compilation...
	cd %ChangeToDir%
	echo %shadercompilecommand% -mpi_workercount %shadercompileworkers% -allowdebug -shaderpath "%shader_path_cd:/=\%" %x360_args% %ps3_args% %SDKArgs% %ps3_additional_args%
	%shadercompilecommand% -mpi_workercount %shadercompileworkers% -allowdebug -shaderpath "%shader_path_cd:/=\%" %x360_args% %ps3_args% %SDKArgs% %ps3_additional_args%
	
	if /i "%DIRECTX_SDK_VER%" == "ps3" (
		echo.
		echo // PS3 fragment shader statistics: =============================================
		echo // Log file location: "%ChangeToDir%\ps3compilelog.txt"
		ps3shaderoptimizer ps3compilelog.txt
	)
	
	cd %shader_path_cd%
)


REM ****************
REM PC, 360, PS3 Shader copy
REM Publish the generated files to the output dir using ROBOCOPY (smart copy) or XCOPY
REM This batch file may have been invoked standalone or slaved (master does final smart mirror copy)
REM ****************
if not "%dynamic_shaders%" == "1" (
	if exist makefile.%inputbase%.copy echo Publishing shaders to target...
	if exist makefile.%inputbase%.copy perl %SrcDirBase%\devtools\bin\copyshaders.pl makefile.%inputbase%.copy %x360_args% %ps3_args%
)

REM ****************
REM Deal with perforce operations for vcs files
REM ****************
if not "%dynamic_shaders%" == "1" if exist vcslist.txt if not "%VALVE_NO_AUTO_P4_SHADERS%" == "1" (
	echo Executing perforce operations on .vcs files.
	perl ..\..\devtools\bin\p4autocheckout.pl vcslist.txt "Shader Auto Checkout VCS" ../../../game/platform/shaders ../../../game/platform/shaders/...
)

REM ****************
REM END
REM ****************
:end


%TTEXE% -diff %tt_start%
echo.

