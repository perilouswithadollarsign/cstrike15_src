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

proj = CADTEST

all: $(proj).exe

# force a complete rebuild from source.
cleanall: clean
	-del *.exe

#clean up everything but the .EXEs.
clean:
	-del *.res
	-del *.?bj
	-del *.map
	
# Update the resource if necessary
$(proj).res: $(srcdir)\$(proj).rc $(srcdir)\$(proj).h
    $(rc) $(cinclude) $(rcvars) -r -fo $(proj).res $(cvars) $(srcdir)\$(proj).rc
!IF defined(CPUTYPE)
    cvtres -$(CPU) $(proj).res -o $(proj).rbj
!ENDIF


# Update the object file if necessary
$(proj).obj: $(srcdir)\$(proj).c $(srcdir)\$(proj).h
    $(cc) $(cflags) $(cinclude) $(cvars) $(cdebug) $(srcdir)\$(proj).c

# Since the link line has some severe differences depending on what
# platform we are running on, we need to special case this so that
# we execute the correct commands:

$(proj).exe: $(proj).obj $(proj).res $(proj).def
!if defined(CPU)
# This is for Windows NT:
    $(link) $(linkdebug) $(guiflags) $(proj).obj $(WINTAB)\lib\$(CPU)\wintab32.lib $(guilibs) VERSION.LIB -out:$(proj).exe
!ELSE
# This is for Windows DOS:
    $(link) $(guiflags) $(proj).obj ,,, $(WINTAB)\lib\wintab.lib $(guilibs) , $(proj).DEF
    rc $(proj).res
!ENDIF
