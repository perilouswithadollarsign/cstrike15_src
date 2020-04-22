# If no CPUTYPE variable is defined, then we are running on a DOS system
# so lets whack in some flags and switches to match the NTWIN32.MAK
# settings:
srcdir=..
model=S
rc=rc
hcopts = -n
cc = cl
cdebug = -Zipel -Od
cflags = -c -A$(model) -Gsw -W3 $(cdebug) -DUSE_X_LIB
cvars = -D$(ENV)
linkdebug = 
link = link $(linkdebug)
guiflags = /NOE /NOD /CO /align:16
guilibs = libw $(model)libcew ver commdlg
guilibsdll = libw $(model)dllcew ver commdlg
