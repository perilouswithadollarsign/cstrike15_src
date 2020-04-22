# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=rule2 - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to rule2 - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "rule2 - Win32 Release" && "$(CFG)" != "rule2 - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "rule2.mak" CFG="rule2 - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "rule2 - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "rule2 - Win32 Debug" (based on "Win32 (x86) Application")
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
# PROP Target_Last_Scanned "rule2 - Win32 Debug"
CPP=cl.exe
RSC=rc.exe
MTL=mktyplib.exe

!IF  "$(CFG)" == "rule2 - Win32 Release"

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

ALL : "$(OUTDIR)\rule2.exe"

CLEAN : 
	-@erase "$(INTDIR)\Rule2.obj"
	-@erase "$(INTDIR)\Rule2.res"
	-@erase "$(OUTDIR)\rule2.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\Include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "..\..\..\Include" /D "WIN32" /D "NDEBUG"\
 /D "_WINDOWS" /Fp"$(INTDIR)/rule2.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/Rule2.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/rule2.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows\
 /incremental:no /pdb:"$(OUTDIR)/rule2.pdb" /machine:I386\
 /out:"$(OUTDIR)/rule2.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Rule2.obj" \
	"$(INTDIR)\Rule2.res"

"$(OUTDIR)\rule2.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "rule2 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "rule2___"
# PROP BASE Intermediate_Dir "rule2___"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "rule2___"
# PROP Intermediate_Dir "rule2___"
# PROP Target_Dir ""
OUTDIR=.\rule2___
INTDIR=.\rule2___

ALL : "$(OUTDIR)\rule2.exe"

CLEAN : 
	-@erase "$(INTDIR)\Rule2.obj"
	-@erase "$(INTDIR)\Rule2.res"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\rule2.exe"
	-@erase "$(OUTDIR)\rule2.ilk"
	-@erase "$(OUTDIR)\rule2.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D\
 "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)/rule2.pch" /YX /Fo"$(INTDIR)/"\
 /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\rule2___/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/Rule2.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/rule2.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows\
 /incremental:yes /pdb:"$(OUTDIR)/rule2.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/rule2.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Rule2.obj" \
	"$(INTDIR)\Rule2.res"

"$(OUTDIR)\rule2.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

# Name "rule2 - Win32 Release"
# Name "rule2 - Win32 Debug"

!IF  "$(CFG)" == "rule2 - Win32 Release"

!ELSEIF  "$(CFG)" == "rule2 - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Rule2\Rule2.rc
DEP_RSC_RULE2=\
	"..\rule2.dlg"\
	"..\Rule2.h"\
	"..\Rule2.ico"\
	

!IF  "$(CFG)" == "rule2 - Win32 Release"


"$(INTDIR)\Rule2.res" : $(SOURCE) $(DEP_RSC_RULE2) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/Rule2.res" /i "\wtkit125\Examples\Rule2" /d\
 "NDEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "rule2 - Win32 Debug"


"$(INTDIR)\Rule2.res" : $(SOURCE) $(DEP_RSC_RULE2) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/Rule2.res" /i "\wtkit125\Examples\Rule2" /d\
 "_DEBUG" $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Rule2\Rule2.c
DEP_CPP_RULE2_=\
	"..\..\..\Include\pktdef.h"\
	"..\..\..\Include\wintab.h"\
	"..\Msgpack.h"\
	"..\Rule2.h"\
	

"$(INTDIR)\Rule2.obj" : $(SOURCE) $(DEP_CPP_RULE2_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Rule2\Rule2.h

!IF  "$(CFG)" == "rule2 - Win32 Release"

!ELSEIF  "$(CFG)" == "rule2 - Win32 Debug"

!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Rule2\Msgpack.h

!IF  "$(CFG)" == "rule2 - Win32 Release"

!ELSEIF  "$(CFG)" == "rule2 - Win32 Debug"

!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
