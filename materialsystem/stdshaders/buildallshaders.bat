@echo off
setlocal

set TTEXE=..\..\devtools\bin\timeprecise.exe
if not exist %TTEXE% goto no_ttexe
goto no_ttexe_end

:no_ttexe
set TTEXE=time /t
:no_ttexe_end


rem echo.
rem echo ~~~~~~ buildallshaders %* ~~~~~~
%TTEXE% -cur-Q
set tt_all_start=%ERRORLEVEL%
set tt_all_chkpt=%tt_start%



set sourcedir="shaders"
set targetdir="..\..\..\game\platform\shaders"

set BUILD_SHADER=call buildshaders.bat

set ARG_X360=-x360
set ARG_PS3=-ps3
set ARG_EXTRA=



REM ****************
REM usage: buildallshaders [-pc | -x360 | -ps3]
REM ****************
set ALLSHADERS_CONFIG=pc
if /i "%1" == "-pc" goto shcfg_pc
if /i "%1" == "-ps3" goto shcfg_ps3
if /i "%1" == "-360" goto shcfg_x360
if /i "%1" == "-x360" goto shcfg_x360
if /i "%1" == "-xbox" goto shcfg_x360

REM // The default is to build shaders for all platforms when not specifying -pc, -x360, or -ps3
call buildallshaders.bat -pc %*
call buildallshaders.bat -x360 %*
call buildallshaders.bat -ps3 %*
goto end_of_file

:shcfg_pc
	echo.
	echo // PC (-pc command line option) ===============================================
	if /i "%2" == "-nompi" set ARG_EXTRA=-nompi
	set ALLSHADERS_CONFIG=pc
	goto shcfg_end
:shcfg_ps3
	echo.
	echo // PS3 (-ps3 command line option) =============================================
	if /i "%2" == "-nompi" set ARG_EXTRA=-nompi
	if /i "%2" == "-ps3debug" set ARG_EXTRA=-ps3debug
	if /i "%2" == "-ps3scheduleoptimization" set ARG_EXTRA=-ps3scheduleoptimization
	set ALLSHADERS_CONFIG=ps3
	goto shcfg_end
:shcfg_x360
	echo.
	echo // 360 (-360 command line option) =============================================
	if /i "%2" == "-nompi" set ARG_EXTRA=-nompi
	set ALLSHADERS_CONFIG=x360
	goto shcfg_end
:shcfg_end


REM ****************
REM PC SHADERS
REM ****************
if /i "%ALLSHADERS_CONFIG%" == "pc" (
  %BUILD_SHADER% stdshader_dx9_20b %ARG_EXTRA%
  %BUILD_SHADER% stdshader_dx9_20b_new	-dx9_30 %ARG_EXTRA%
  %BUILD_SHADER% stdshader_dx9_30		-dx9_30	-force30 %ARG_EXTRA%
  rem %BUILD_SHADER% stdshader_dx10     -dx10 %ARG_EXTRA%
)

REM ****************
REM X360 SHADERS
REM ****************
if /i "%ALLSHADERS_CONFIG%" == "x360" (
  %BUILD_SHADER% stdshader_dx9_20b      %ARG_X360% %ARG_EXTRA%
  %BUILD_SHADER% stdshader_dx9_20b_new	%ARG_X360% %ARG_EXTRA%
  rem %BUILD_SHADER% stdshader_dx9_30   %ARG_X360% %ARG_EXTRA%
  rem %BUILD_SHADER% stdshader_dx10     %ARG_X360% %ARG_EXTRA%
)

REM ****************
REM PS3 SHADERS
REM ****************
if /i "%ALLSHADERS_CONFIG%" == "ps3" (
  %BUILD_SHADER% stdshader_ps3			%ARG_PS3% %ARG_EXTRA%
)

REM ****************
REM END
REM ****************
:end



rem echo.
if not "%dynamic_shaders%" == "1" (
  rem echo Finished full buildallshaders %*
) else (
  rem echo Finished dynamic buildallshaders %*
)

rem %TTEXE% -diff %tt_all_start% -cur
rem echo.

:end_of_file
