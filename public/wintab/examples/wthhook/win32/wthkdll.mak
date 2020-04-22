# Microsoft Developer Studio Generated NMAKE File, Based on wthkdll.dsp
!IF "$(CFG)" == ""
CFG=wthkdll - Win32 Release
!MESSAGE No configuration specified. Defaulting to wthkdll - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "wthkdll - Win32 Release" && "$(CFG)" !=\
 "wthkdll - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "wthkdll.mak" CFG="wthkdll - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "wthkdll - Win32 Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "wthkdll - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "wthkdll - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\wthkdll.dll"

!ELSE 

ALL : "$(OUTDIR)\wthkdll.dll"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\wthkdll.obj"
	-@erase "$(OUTDIR)\wthkdll.dll"
	-@erase "$(OUTDIR)\wthkdll.exp"
	-@erase "$(OUTDIR)\wthkdll.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\wthkdll.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\wthkdll.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wintab32.lib ..\..\..\lib\i386\wntab32x.lib kernel32.lib\
 user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib\
 ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)\wthkdll.pdb" /machine:I386 /def:".\wthkdll.def"\
 /out:"$(OUTDIR)\wthkdll.dll" /implib:"$(OUTDIR)\wthkdll.lib" 
DEF_FILE= \
	".\wthkdll.def"
LINK32_OBJS= \
	"$(INTDIR)\wthkdll.obj"

"$(OUTDIR)\wthkdll.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "wthkdll - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\wthkdll.dll"

!ELSE 

ALL : "$(OUTDIR)\wthkdll.dll"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(INTDIR)\wthkdll.obj"
	-@erase "$(OUTDIR)\wthkdll.dll"
	-@erase "$(OUTDIR)\wthkdll.exp"
	-@erase "$(OUTDIR)\wthkdll.ilk"
	-@erase "$(OUTDIR)\wthkdll.lib"
	-@erase "$(OUTDIR)\wthkdll.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\wthkdll.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\wthkdll.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=..\..\..\lib\i386\wntab32x.lib kernel32.lib user32.lib gdi32.lib\
 winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib\
 uuid.lib /nologo /subsystem:windows /dll /incremental:yes\
 /pdb:"$(OUTDIR)\wthkdll.pdb" /debug /machine:I386 /def:".\wthkdll.def"\
 /out:"$(OUTDIR)\wthkdll.dll" /implib:"$(OUTDIR)\wthkdll.lib" 
DEF_FILE= \
	".\wthkdll.def"
LINK32_OBJS= \
	"$(INTDIR)\wthkdll.obj"

"$(OUTDIR)\wthkdll.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

OutDir=.\.\Debug
TargetName=wthkdll
InputPath=.\Debug\wthkdll.dll
SOURCE=$(InputPath)

"debug\$(TargetName).dll"	 : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(OutDir)\$(TargetName).dll debug

!ENDIF 


!IF "$(CFG)" == "wthkdll - Win32 Release" || "$(CFG)" ==\
 "wthkdll - Win32 Debug"
SOURCE=..\wthkdll.c
DEP_CPP_WTHKD=\
	{$(INCLUDE)}"pktdef.h"\
	{$(INCLUDE)}"wintab.h"\
	

"$(INTDIR)\wthkdll.obj" : $(SOURCE) $(DEP_CPP_WTHKD) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

