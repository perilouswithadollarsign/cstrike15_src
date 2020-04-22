# Microsoft Developer Studio Generated NMAKE File, Based on Wthook.dsp
!IF "$(CFG)" == ""
CFG=wthook - Win32 Release
!MESSAGE No configuration specified. Defaulting to wthook - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "wthook - Win32 Release" && "$(CFG)" != "wthook - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Wthook.mak" CFG="wthook - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "wthook - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "wthook - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "wthook - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\Wthook.exe"

!ELSE 

ALL : "wthkdll - Win32 Release" "$(OUTDIR)\Wthook.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"wthkdll - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\wthook.obj"
	-@erase "$(OUTDIR)\Wthook.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\Wthook.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\Wthook.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wintab32.lib ..\..\..\lib\i386\wntab32x.lib kernel32.lib\
 user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib\
 ole32.lib oleaut32.lib uuid.lib /nologo /subsystem:windows /incremental:no\
 /pdb:"$(OUTDIR)\Wthook.pdb" /machine:I386 /out:"$(OUTDIR)\Wthook.exe" 
LINK32_OBJS= \
	"$(INTDIR)\wthook.obj" \
	"$(OUTDIR)\wthkdll.lib"

"$(OUTDIR)\Wthook.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "wthook - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\Wthook.exe"

!ELSE 

ALL : "wthkdll - Win32 Debug" "$(OUTDIR)\Wthook.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"wthkdll - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(INTDIR)\wthook.obj"
	-@erase "$(OUTDIR)\Wthook.exe"
	-@erase "$(OUTDIR)\Wthook.ilk"
	-@erase "$(OUTDIR)\Wthook.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS"\
 /Fp"$(INTDIR)\Wthook.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\Wthook.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=..\..\..\lib\i386\wntab32x.lib kernel32.lib user32.lib gdi32.lib\
 winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib\
 uuid.lib /nologo /subsystem:windows /incremental:yes\
 /pdb:"$(OUTDIR)\Wthook.pdb" /debug /machine:I386 /out:"$(OUTDIR)\Wthook.exe" 
LINK32_OBJS= \
	"$(INTDIR)\wthook.obj" \
	"$(OUTDIR)\wthkdll.lib"

"$(OUTDIR)\Wthook.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "wthook - Win32 Release" || "$(CFG)" == "wthook - Win32 Debug"
SOURCE=..\wthook.c
DEP_CPP_WTHOO=\
	{$(INCLUDE)}"wintab.h"\
	

"$(INTDIR)\wthook.obj" : $(SOURCE) $(DEP_CPP_WTHOO) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "wthook - Win32 Release"

"wthkdll - Win32 Release" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\wthkdll.mak CFG="wthkdll - Win32 Release" 
   cd "."

"wthkdll - Win32 ReleaseCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\wthkdll.mak CFG="wthkdll - Win32 Release"\
 RECURSE=1 
   cd "."

!ELSEIF  "$(CFG)" == "wthook - Win32 Debug"

"wthkdll - Win32 Debug" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) /F .\wthkdll.mak CFG="wthkdll - Win32 Debug" 
   cd "."

"wthkdll - Win32 DebugCLEAN" : 
   cd "."
   $(MAKE) /$(MAKEFLAGS) CLEAN /F .\wthkdll.mak CFG="wthkdll - Win32 Debug"\
 RECURSE=1 
   cd "."

!ENDIF 


!ENDIF 

