::
:: Makefile to build zlib as statically-linked in a way that works with Steam
:: and the games.
::
:: Note: run this batch file from the "Visual Studio 2005 Command Prompt"
::
@echo off

echo ---------------------------------------------------
echo Building Win32
echo ---------------------------------------------------
nmake -f win32\Makefile.msc clean zlib.lib LOC="-MT -DASMV -DASMINF" OBJA="inffas32.obj match686.obj"
IF ERRORLEVEL 1 GOTO END

echo ---------------------------------------------------
echo Publishing Win32
echo ---------------------------------------------------
p4 open ..\..\lib\win32\release\zlib.lib
p4 open ..\..\lib\win32\release\zlib.pdb
copy zlib.lib ..\..\lib\win32\release\
copy zlib.pdb ..\..\lib\win32\release\

:END
