# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=mgrtest - Win32 thunk Debug
!MESSAGE No configuration specified.  Defaulting to mgrtest - Win32 thunk\
 Debug.
!ENDIF 

!IF "$(CFG)" != "mgrtest - Win32 Release" && "$(CFG)" !=\
 "mgrtest - Win32 Debug" && "$(CFG)" != "mgrtest - Win32 thunk Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "mgrtest.mak" CFG="mgrtest - Win32 thunk Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mgrtest - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "mgrtest - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "mgrtest - Win32 thunk Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "mgrtest - Win32 Debug"
CPP=cl.exe
RSC=rc.exe
MTL=mktyplib.exe

!IF  "$(CFG)" == "mgrtest - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\mgrtest.exe"

CLEAN : 
	-@erase "$(INTDIR)\bitbox.obj"
	-@erase "$(INTDIR)\btnMap.obj"
	-@erase "$(INTDIR)\BtnMask.obj"
	-@erase "$(INTDIR)\Csrmask.obj"
	-@erase "$(INTDIR)\Infodlg.obj"
	-@erase "$(INTDIR)\Mgrdlg.obj"
	-@erase "$(INTDIR)\Mgrtest.obj"
	-@erase "$(INTDIR)\mgrtest.res"
	-@erase "$(INTDIR)\MoveMask.obj"
	-@erase "$(INTDIR)\tests.obj"
	-@erase "$(OUTDIR)\mgrtest.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\Include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "..\..\..\Include" /D "WIN32" /D "NDEBUG"\
 /D "_WINDOWS" /Fp"$(INTDIR)/mgrtest.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/mgrtest.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/mgrtest.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows\
 /incremental:no /pdb:"$(OUTDIR)/mgrtest.pdb" /machine:I386\
 /out:"$(OUTDIR)/mgrtest.exe" 
LINK32_OBJS= \
	"$(INTDIR)\bitbox.obj" \
	"$(INTDIR)\btnMap.obj" \
	"$(INTDIR)\BtnMask.obj" \
	"$(INTDIR)\Csrmask.obj" \
	"$(INTDIR)\Infodlg.obj" \
	"$(INTDIR)\Mgrdlg.obj" \
	"$(INTDIR)\Mgrtest.obj" \
	"$(INTDIR)\mgrtest.res" \
	"$(INTDIR)\MoveMask.obj" \
	"$(INTDIR)\tests.obj"

"$(OUTDIR)\mgrtest.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mgrtest_"
# PROP BASE Intermediate_Dir "mgrtest_"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "mgrtest_"
# PROP Intermediate_Dir "mgrtest_"
# PROP Target_Dir ""
OUTDIR=.\mgrtest_
INTDIR=.\mgrtest_

ALL : "$(OUTDIR)\mgrtest.exe"

CLEAN : 
	-@erase "$(INTDIR)\bitbox.obj"
	-@erase "$(INTDIR)\btnMap.obj"
	-@erase "$(INTDIR)\BtnMask.obj"
	-@erase "$(INTDIR)\Csrmask.obj"
	-@erase "$(INTDIR)\Infodlg.obj"
	-@erase "$(INTDIR)\Mgrdlg.obj"
	-@erase "$(INTDIR)\Mgrtest.obj"
	-@erase "$(INTDIR)\mgrtest.res"
	-@erase "$(INTDIR)\MoveMask.obj"
	-@erase "$(INTDIR)\tests.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\mgrtest.exe"
	-@erase "$(OUTDIR)\mgrtest.ilk"
	-@erase "$(OUTDIR)\mgrtest.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D\
 "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)/mgrtest.pch" /YX /Fo"$(INTDIR)/"\
 /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\mgrtest_/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/mgrtest.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/mgrtest.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows\
 /incremental:yes /pdb:"$(OUTDIR)/mgrtest.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/mgrtest.exe" 
LINK32_OBJS= \
	"$(INTDIR)\bitbox.obj" \
	"$(INTDIR)\btnMap.obj" \
	"$(INTDIR)\BtnMask.obj" \
	"$(INTDIR)\Csrmask.obj" \
	"$(INTDIR)\Infodlg.obj" \
	"$(INTDIR)\Mgrdlg.obj" \
	"$(INTDIR)\Mgrtest.obj" \
	"$(INTDIR)\mgrtest.res" \
	"$(INTDIR)\MoveMask.obj" \
	"$(INTDIR)\tests.obj"

"$(OUTDIR)\mgrtest.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mgrtest0"
# PROP BASE Intermediate_Dir "mgrtest0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "mgrtest0"
# PROP Intermediate_Dir "mgrtest0"
# PROP Target_Dir ""
OUTDIR=.\mgrtest0
INTDIR=.\mgrtest0

ALL : "$(OUTDIR)\mgrtest.exe"

CLEAN : 
	-@erase "$(INTDIR)\bitbox.obj"
	-@erase "$(INTDIR)\btnMap.obj"
	-@erase "$(INTDIR)\BtnMask.obj"
	-@erase "$(INTDIR)\Csrmask.obj"
	-@erase "$(INTDIR)\Infodlg.obj"
	-@erase "$(INTDIR)\Mgrdlg.obj"
	-@erase "$(INTDIR)\Mgrtest.obj"
	-@erase "$(INTDIR)\mgrtest.res"
	-@erase "$(INTDIR)\MoveMask.obj"
	-@erase "$(INTDIR)\tests.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\mgrtest.exe"
	-@erase "$(OUTDIR)\mgrtest.ilk"
	-@erase "$(OUTDIR)\mgrtest.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D\
 "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)/mgrtest.pch" /YX /Fo"$(INTDIR)/"\
 /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\mgrtest0/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/mgrtest.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/mgrtest.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows\
 /incremental:yes /pdb:"$(OUTDIR)/mgrtest.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/mgrtest.exe" 
LINK32_OBJS= \
	"$(INTDIR)\bitbox.obj" \
	"$(INTDIR)\btnMap.obj" \
	"$(INTDIR)\BtnMask.obj" \
	"$(INTDIR)\Csrmask.obj" \
	"$(INTDIR)\Infodlg.obj" \
	"$(INTDIR)\Mgrdlg.obj" \
	"$(INTDIR)\Mgrtest.obj" \
	"$(INTDIR)\mgrtest.res" \
	"$(INTDIR)\MoveMask.obj" \
	"$(INTDIR)\tests.obj"

"$(OUTDIR)\mgrtest.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "mgrtest - Win32 Release"
# Name "mgrtest - Win32 Debug"
# Name "mgrtest - Win32 thunk Debug"

!IF  "$(CFG)" == "mgrtest - Win32 Release"

!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"

!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\tests.c
NODEP_CPP_TESTS=\
	"..\wintab.h"\
	

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\tests.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\tests.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\tests.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\btnMap.c
DEP_CPP_BTNMA=\
	"..\Mgrtest.h"\
	
NODEP_CPP_BTNMA=\
	"..\wintab.h"\
	

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\btnMap.obj" : $(SOURCE) $(DEP_CPP_BTNMA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\btnMap.obj" : $(SOURCE) $(DEP_CPP_BTNMA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\btnMap.obj" : $(SOURCE) $(DEP_CPP_BTNMA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\BtnMask.c
DEP_CPP_BTNMAS=\
	"..\Mgrdlg.h"\
	"..\Mgrtest.h"\
	
NODEP_CPP_BTNMAS=\
	"..\wintab.h"\
	

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\BtnMask.obj" : $(SOURCE) $(DEP_CPP_BTNMAS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\BtnMask.obj" : $(SOURCE) $(DEP_CPP_BTNMAS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\BtnMask.obj" : $(SOURCE) $(DEP_CPP_BTNMAS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\Csrmask.c
DEP_CPP_CSRMA=\
	"..\Mgrtest.h"\
	
NODEP_CPP_CSRMA=\
	"..\wintab.h"\
	

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\Csrmask.obj" : $(SOURCE) $(DEP_CPP_CSRMA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\Csrmask.obj" : $(SOURCE) $(DEP_CPP_CSRMA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\Csrmask.obj" : $(SOURCE) $(DEP_CPP_CSRMA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\Infodlg.c
DEP_CPP_INFOD=\
	"..\Mgrtest.h"\
	
NODEP_CPP_INFOD=\
	"..\wintab.h"\
	

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\Infodlg.obj" : $(SOURCE) $(DEP_CPP_INFOD) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\Infodlg.obj" : $(SOURCE) $(DEP_CPP_INFOD) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\Infodlg.obj" : $(SOURCE) $(DEP_CPP_INFOD) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\Mgrdlg.c
DEP_CPP_MGRDL=\
	"..\Mgrdlg.h"\
	"..\Msgpack.h"\
	

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\Mgrdlg.obj" : $(SOURCE) $(DEP_CPP_MGRDL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\Mgrdlg.obj" : $(SOURCE) $(DEP_CPP_MGRDL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\Mgrdlg.obj" : $(SOURCE) $(DEP_CPP_MGRDL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\Mgrtest.c
DEP_CPP_MGRTE=\
	"..\Mgrdlg.h"\
	"..\Mgrtest.h"\
	"..\Msgpack.h"\
	

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\Mgrtest.obj" : $(SOURCE) $(DEP_CPP_MGRTE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\Mgrtest.obj" : $(SOURCE) $(DEP_CPP_MGRTE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\Mgrtest.obj" : $(SOURCE) $(DEP_CPP_MGRTE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\mgrtest.rc
DEP_RSC_MGRTES=\
	"..\Mgrdlg.h"\
	"..\Mgrtest.h"\
	

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\mgrtest.res" : $(SOURCE) $(DEP_RSC_MGRTES) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/mgrtest.res" /i "\wtkit125\Examples\Mgrtest"\
 /d "NDEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\mgrtest.res" : $(SOURCE) $(DEP_RSC_MGRTES) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/mgrtest.res" /i "\wtkit125\Examples\Mgrtest"\
 /d "_DEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\mgrtest.res" : $(SOURCE) $(DEP_RSC_MGRTES) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/mgrtest.res" /i "\wtkit125\Examples\Mgrtest"\
 /d "_DEBUG" $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\MoveMask.c
DEP_CPP_MOVEM=\
	"..\Mgrtest.h"\
	
NODEP_CPP_MOVEM=\
	"..\wintab.h"\
	

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\MoveMask.obj" : $(SOURCE) $(DEP_CPP_MOVEM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\MoveMask.obj" : $(SOURCE) $(DEP_CPP_MOVEM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\MoveMask.obj" : $(SOURCE) $(DEP_CPP_MOVEM) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Mgrtest\bitbox.c

!IF  "$(CFG)" == "mgrtest - Win32 Release"


"$(INTDIR)\bitbox.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 Debug"


"$(INTDIR)\bitbox.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "mgrtest - Win32 thunk Debug"


"$(INTDIR)\bitbox.obj" : $(SOURCE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
