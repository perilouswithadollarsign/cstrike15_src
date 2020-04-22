!IF "$(CPU)" != ""
OS=NT
ENV=WIN32
!ELSE
OS=DOS
ENV=WIN16
!ENDIF

# Environment variables LIB and INCLUDE should point to your Win SDK libraries
# and include files.

# Define WINTAB to point to your wintab development tree
# Example: WINTAB=c:\wintab
WINTAB=..\..\..

!include "$(OS)$(ENV).MAK"
cinclude=-I$(srcdir) -I$(WINTAB)\include

proj = MGRTEST
projdll = MGRDLL

all: $(proj).exe $(projdll).dll

# force a complete rebuild from source.
cleanall: clean
	-del *.exe
	-del *.dll

#clean up everything but the .EXEs.
clean:
	-del *.lib
	-del *.exp
	-del *.res
	-del *.?bj
	-del *.map
	
# Update the resource if necessary
$(proj).res: $(srcdir)\$(proj).rc $(srcdir)\mgrdlg.dlg $(srcdir)\mgrdlg.h $(srcdir)\$(proj).h
    $(rc) $(rcvars) -r -fo $(proj).res $(cvars) $(srcdir)\$(proj).rc
!if defined(CPU)
    cvtres -$(CPU) $(proj).res -o $(proj).rbj
!ENDIF

OBJS = $(proj).obj mgrdlg.obj bitbox.obj btnMap.obj Csrmask.obj Infodlg.obj MoveMask.obj tests.obj BtnMask.obj
# Update the object files if necessary
$(proj).obj: $(srcdir)\$(proj).c $(srcdir)\$(proj).h $(srcdir)\mgrdlg.h
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\$(proj).c $(srcdir)\$(proj).h $(srcdir)\mgrdlg.h

mgrdlg.obj: $(srcdir)\mgrdlg.c $(srcdir)\mgrdlg.h
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\mgrdlg.c $(srcdir)\mgrdlg.h

mgrdll.obj: $(srcdir)\mgrdll.c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\mgrdll.c

bitbox.obj: $(srcdir)\bitbox.c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\bitbox.c

btnMap.obj: $(srcdir)\btnMap.c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\btnMap.c

btnMask.obj: $(srcdir)\btnMask.c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\btnMask.c

Infodlg.obj: $(srcdir)\Infodlg.c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\Infodlg.c

MoveMask.obj: $(srcdir)\MoveMask.c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\MoveMask.c

tests.obj: $(srcdir)\tests.c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\tests.c

Csrmask.obj: $(srcdir)\Csrmask.c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\Csrmask.c


# Update the exp file if necessary.
$(projdll).exp $(projdll).lib: $(projdll).obj $(projdll).def
	$(implib) -machine:$(CPU) \
	-def:$(projdll).def \
	-out:$(projdll).lib $(projdll).obj -verbose


# Since the link line has some severe differences depending on what
# platform we are running on, we need to special case this so that
# we execute the correct commands:

!if defined(CPU)
# This is for Windows NT:
$(proj).exe: $(OBJS) $(proj).res $(proj).def
    $(link) $(linkdebug) $(guiflags) $(OBJS) \
	$(WINTAB)\lib\$(CPU)\wintab32.lib $(guilibs) VERSION.LIB \
	$(proj).rbj -out:$(proj).exe /machine:$(CPU) 
$(projdll).dll: $(projdll).obj $(projdll).def $(projdll).exp
    $(link) $(linkdebug) $(guiflags) $(projdll).obj $(projdll).exp \
	$(guilibsdll) VERSION.LIB -out:$(projdll).dll \
	/machine:$(CPU) -entry:_DllMainCRTStartup$(DLLENTRY) -dll
!ENDIF
!if !defined(CPU)
# This is for Windows DOS:
$(proj).exe: $(OBJS) $(proj).res $(proj).def
    $(link) $(guiflags) $(OBJS) ,,, $(WINTAB)\lib\wintab.lib $(guilibs) , $(proj).DEF
    rc $(proj).res
$(projdll).dll: $(projdll).obj $(projdll).def
    $(link) $(guiflags) libentry.obj $(projdll).obj ,$(projdll).dll,, \
		$(WINTAB)\lib\wintab.lib $(guilibsdll) , $(projdll).DEF
    rc $(projdll).dll
!ENDIF
