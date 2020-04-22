#!IF "$(CPU)" != ""
#OS=NT
#ENV=WIN32
#!ELSE
OS=DOS
ENV=WIN16
#!ENDIF

# Environment variables LIB and INCLUDE should point to your Win SDK libraries
# and include files.

# Define WINTAB to point to your wintab development tree
# Example: WINTAB=c:\wintab
WINTAB=..\..\..

!include "$(OS)$(ENV).MAK"
cinclude=-I$(srcdir) -I$(WINTAB)\include

proj = wthook
projdll = wthkdll

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
#$(proj).res: $(srcdir)\$(proj).rc $(srcdir)\mgrdlg.dlg $(srcdir)\mgrdlg.h $(srcdir)\$(proj).h
#    $(rc) $(rcvars) -r -fo $(proj).res $(cvars) $(srcdir)\$(proj).rc
#!if defined(CPU)
#    cvtres -$(CPU) $(proj).res -o $(proj).rbj
#!ENDIF

OBJS = $(proj).obj
DLLS = $(projdll).dll

# Update the object files if necessary
$(proj).obj: $(srcdir)\$(proj).c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\$(proj).c

#$(proj).obj: $(srcdir)\$(proj).c
#    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\$(projdll).c

$(projdll).obj: $(srcdir)\$(projdll).c
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\$(projdll).c

# Update the exp file if necessary.
$(projdll).exp $(projdll).lib: $(projdll).obj $(projdll).def
        $(implib) -machine:$(CPU) \
        -def:..\$(projdll).def \
        -out:$(projdll).lib $(projdll).obj -verbose


# Since the link line has some severe differences depending on what
# platform we are running on, we need to special case this so that
# we execute the correct commands:

#!if defined(CPU)
# This is for Windows NT:
#$(proj).exe: $(OBJS) ..\$(proj).def
#    $(link) $(linkdebug) $(guiflags) $(OBJS) \
#        $(WINTAB)\lib\$(CPU)\wintab32.lib $(guilibs) VERSION.LIB \
#        $(proj).rbj -out:$(proj).exe /machine:$(CPU) 
#$(projdll).dll: $(projdll).obj $(projdll).def $(projdll).exp
#    $(link) $(linkdebug) $(guiflags) $(projdll).obj $(projdll).exp \
#        $(guilibsdll) VERSION.LIB -out:$(projdll).dll \
#        /machine:$(CPU) -entry:_DllMainCRTStartup$(DLLENTRY) -dll
#!ENDIF
#!if !defined(CPU)
# This is for Windows DOS:
$(proj).exe: $(OBJS) $(DLLS) $(proj).def
    $(link) $(guiflags) $(OBJS) ,,, $(WINTAB)\lib\wintab.lib $(guilibs) , $(proj).def
    rc $(proj).exe
$(projdll).dll: $(projdll).obj $(projdll).def
    $(link) $(guiflags) libentry.obj $(projdll).obj ,$(projdll).dll,, \
                $(WINTAB)\lib\wintab.lib $(guilibsdll) , $(projdll).DEF
    rc $(projdll).dll
#!ENDIF
