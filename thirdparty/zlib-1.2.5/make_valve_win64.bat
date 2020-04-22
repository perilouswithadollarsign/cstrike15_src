::
:: Makefile to build zlib as statically-linked in a way that works with Steam
:: and the games.
::
:: **IMPORTANT**: run this batch file from the "Visual Studio 2005 x64 Cross Tools Command Prompt"
::
@echo off

echo ---------------------------------------------------
echo Building Win64
echo ---------------------------------------------------
nmake -f win32\Makefile.msc clean zlib.lib AS=ml64 LOC="-I. -MT -DASMV -DASMINF" OBJA="inffasx64.obj gvmat64.obj inffas8664.obj"
IF ERRORLEVEL 1 GOTO END

echo ---------------------------------------------------
echo Publishing Win64
echo ---------------------------------------------------
p4 open ..\..\lib\win64\release\zlib.lib
p4 open ..\..\lib\win64\release\zlib.pdb
copy zlib.lib ..\..\lib\win64\release\
copy zlib.pdb ..\..\lib\win64\release\

:END
