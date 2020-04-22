@echo off

setlocal

set valveTmpFileFromVS2005=%1%
set valveTmpFileFromVS2005=%valveTmpFileFromVS2005:\\=\%

set valveChangelistName="Visual Studio Auto Checkout"

:: // Make sure we have 2 args
if .%2.==.. (
	echo  *** [valve_p4_edit] Error calling command! No file specified for checkout! 
	echo  *** Usage: valve_p4_edit.cmd file srcdir ["changelist name"]
	echo  *** The changelist name is optional. If present, must be quoted. By default it's %valveChangelistName%.
	endlocal
	exit /b 1
)

if [%3] NEQ [] (
	set valveChangelistName=%3
)

:: // Fallback to old functionality and just change file attribs
if NOT "%VALVE_NO_AUTO_P4%"=="" (
	echo [valve_p4_edit] VALVE_NO_AUTO_P4 defined. Changing file attribs only!
	if EXIST %valveTmpFileFromVS2005% (
		attrib -r %valveTmpFileFromVS2005%
	)
	endlocal
	exit /b 0
)

rem // :: // First check if file exists
rem // if NOT EXIST %valveTmpFileFromVS2005% (
rem // 	:: // File does not exist
rem // 	rem // echo [valve_p4_edit] %valveTmpFileFromVS2005% does not exist! Skipping p4 edit command.
rem // 	endlocal
rem // 	exit /b 0
rem // )
rem // 
rem // :: // Check if file is read-only (NOTE: If the file is writable, we don't call p4!)
rem // dir /b /ar %valveTmpFileFromVS2005% >nul 2>&1
rem // if NOT %ERRORLEVEL%==0 (
rem // 	rem // echo [valve_p4_edit] %valveTmpFileFromVS2005% is already writable. Not calling p4 edit.
rem // 	endlocal
rem // 	exit /b 0
rem // )

:: // Put 360 bins into their own changelist
for /f %%A in ('echo %valveTmpFileFromVS2005% ^| find /i "360"') do set valveTmpIs360=%%A
if NOT "%valveTmpIs360%"=="" (
	set valveChangelistName="360 Visual Studio Auto Checkout"
)

for /f %%A in ('echo %valveTmpFileFromVS2005% ^| find /i "launcher_main"') do set valveTmpIs360=%%A
if NOT "%valveTmpIs360%"=="" (
	set valveChangelistName="360 Visual Studio Auto Checkout"
)

set valveP4EditCall=%2\devtools\bin\valve_p4_edit_to_changelist.cmd %valveTmpFileFromVS2005% %valveChangelistName%

if "%VALVE_WAIT_ON_P4%"=="" (
	:: // The file exists and is read-only, so change the file attrib and spawn the Perforce checkout asynchronously
	if EXIST %valveTmpFileFromVS2005% attrib -r %valveTmpFileFromVS2005%
	start /b cmd /c %valveP4EditCall% EXIT
	rem // (NOTE: The EXIT arg is needed because we're using the start command to launch the cmd!)
) ELSE (
	:: // Do not use attrib -r if we're doing synchronous p4 edit.  If this fails, we want to throw an error below
	call %valveP4EditCall%
)

:: // Make sure file is writable if it exists
if NOT EXIST %valveTmpFileFromVS2005% goto End
dir /b /a-r %valveTmpFileFromVS2005% >nul 2>&1
if NOT %ERRORLEVEL%==0 (
	:: // Return with an error to stop the build!
	echo  *** [valve_p4_edit] Error! %valveTmpFileFromVS2005% is not writable!
	endlocal
	exit /b 1
)

:End
endlocal
exit /b 0
