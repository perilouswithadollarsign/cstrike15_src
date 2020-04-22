# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

!IF "$(CFG)" == ""
CFG=syspress - Win32 use wntab32x
!MESSAGE No configuration specified.  Defaulting to syspress - Win32 use\
 wntab32x.
!ENDIF 

!IF "$(CFG)" != "syspress - Win32 Release" && "$(CFG)" !=\
 "syspress - Win32 Debug" && "$(CFG)" != "syspress - Win32 use wntab32x"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "syspress.mak" CFG="syspress - Win32 use wntab32x"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "syspress - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "syspress - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "syspress - Win32 use wntab32x" (based on "Win32 (x86) Application")
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
# PROP Target_Last_Scanned "syspress - Win32 Debug"
CPP=cl.exe
RSC=rc.exe
MTL=mktyplib.exe

!IF  "$(CFG)" == "syspress - Win32 Release"

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

ALL : "$(OUTDIR)\syspress.exe"

CLEAN : 
	-@erase "$(INTDIR)\Syspress.obj"
	-@erase "$(INTDIR)\Syspress.res"
	-@erase "$(OUTDIR)\syspress.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /GX /O2 /I "..\..\..\Include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "..\..\..\Include" /D "WIN32" /D "NDEBUG"\
 /D "_WINDOWS" /Fp"$(INTDIR)/syspress.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/Syspress.res" /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/syspress.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows\
 /incremental:no /pdb:"$(OUTDIR)/syspress.pdb" /machine:I386\
 /out:"$(OUTDIR)/syspress.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Syspress.obj" \
	"$(INTDIR)\Syspress.res"

"$(OUTDIR)\syspress.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "syspress - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "syspress"
# PROP BASE Intermediate_Dir "syspress"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "syspress"
# PROP Intermediate_Dir "syspress"
# PROP Target_Dir ""
OUTDIR=.\syspress
INTDIR=.\syspress

ALL : "$(OUTDIR)\syspress.exe"

CLEAN : 
	-@erase "$(INTDIR)\Syspress.obj"
	-@erase "$(INTDIR)\Syspress.res"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\syspress.exe"
	-@erase "$(OUTDIR)\syspress.ilk"
	-@erase "$(OUTDIR)\syspress.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D\
 "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)/syspress.pch" /YX /Fo"$(INTDIR)/"\
 /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\syspress/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/Syspress.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/syspress.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows\
 /incremental:yes /pdb:"$(OUTDIR)/syspress.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/syspress.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Syspress.obj" \
	"$(INTDIR)\Syspress.res"

"$(OUTDIR)\syspress.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "syspress - Win32 use wntab32x"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "syspres0"
# PROP BASE Intermediate_Dir "syspres0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "syspres0"
# PROP Intermediate_Dir "syspres0"
# PROP Target_Dir ""
OUTDIR=.\syspres0
INTDIR=.\syspres0

ALL : "$(OUTDIR)\syspress.exe"

CLEAN : 
	-@erase "$(INTDIR)\Syspress.obj"
	-@erase "$(INTDIR)\Syspress.res"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\syspress.exe"
	-@erase "$(OUTDIR)\syspress.ilk"
	-@erase "$(OUTDIR)\syspress.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "USE_X_LIB" /YX /c
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /I "..\..\..\Include" /D "WIN32" /D\
 "_DEBUG" /D "_WINDOWS" /D "USE_X_LIB" /Fp"$(INTDIR)/syspress.pch" /YX\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\syspres0/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
RSC_PROJ=/l 0x409 /fo"$(INTDIR)/Syspress.res" /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/syspress.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wintab32.lib /nologo /subsystem:windows /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\..\..\lib\I386\wntab32x.lib /nologo /subsystem:windows /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ..\..\..\lib\I386\wntab32x.lib /nologo /subsystem:windows\
 /incremental:yes /pdb:"$(OUTDIR)/syspress.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/syspress.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Syspress.obj" \
	"$(INTDIR)\Syspress.res"

"$(OUTDIR)\syspress.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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

# Name "syspress - Win32 Release"
# Name "syspress - Win32 Debug"
# Name "syspress - Win32 use wntab32x"

!IF  "$(CFG)" == "syspress - Win32 Release"

!ELSEIF  "$(CFG)" == "syspress - Win32 Debug"

!ELSEIF  "$(CFG)" == "syspress - Win32 use wntab32x"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Syspress\Syspress.rc
DEP_RSC_SYSPR=\
	"..\Syspress.h"\
	

!IF  "$(CFG)" == "syspress - Win32 Release"


"$(INTDIR)\Syspress.res" : $(SOURCE) $(DEP_RSC_SYSPR) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/Syspress.res" /i "\wtkit125\Examples\Syspress"\
 /d "NDEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "syspress - Win32 Debug"


"$(INTDIR)\Syspress.res" : $(SOURCE) $(DEP_RSC_SYSPR) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/Syspress.res" /i "\wtkit125\Examples\Syspress"\
 /d "_DEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "syspress - Win32 use wntab32x"


"$(INTDIR)\Syspress.res" : $(SOURCE) $(DEP_RSC_SYSPR) "$(INTDIR)"
   $(RSC) /l 0x409 /fo"$(INTDIR)/Syspress.res" /i "\wtkit125\Examples\Syspress"\
 /d "_DEBUG" $(SOURCE)


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=\wtkit125\Examples\Syspress\Syspress.c
DEP_CPP_SYSPRE=\
	"..\Msgpack.h"\
	"..\Syspress.h"\
	

!IF  "$(CFG)" == "syspress - Win32 Release"


"$(INTDIR)\Syspress.obj" : $(SOURCE) $(DEP_CPP_SYSPRE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "syspress - Win32 Debug"


"$(INTDIR)\Syspress.obj" : $(SOURCE) $(DEP_CPP_SYSPRE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "syspress - Win32 use wntab32x"


"$(INTDIR)\Syspress.obj" : $(SOURCE) $(DEP_CPP_SYSPRE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

# End Source File
# End Target
# End Project
################################################################################
