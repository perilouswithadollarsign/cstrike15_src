   ------------------------------- readme.txt -------------------------------   

     Wintab(tm) Programmer's Kit V. 1.25 for 16- and 32-bit Windows APIs.

     This file contains programmer's kit installation instructions and
     programming notes for the WTHOOK project.

     Please direct your programming questions and comments to the author:

	LCS/Telegraphics
	150 Rogers St.
	Cambridge, MA 02142
	voice: (617) 225-7970
	fax: (617) 225-7969
	email: wintab@pointing.com
	web site: http://www.pointing.com
	ftp site: ftp://ftp.pointing.com/pointing/Wintab/

     Questions or problems regarding specific devices or device drivers
     should be directed to the device manufacturer.

   --------------------------------------------------------------------------   
				MSVC Notes for WTHOOK Project
   --------------------------------------------------------------------------   

     MSVC 4.2 will not be able to compile MFC_DEMO and WTHOOK projects.  

     MSVC 5.0 will be able to compile all the projects. 
     Also, please note that the linker warning LNK4098 (to recompile
     with the /Nodefaultlib:LIBC option) during the compilationq of Debug
     WTHOOK project will not affect the operation of the resulting
     executable.  This bug concerns all projects linking the WNTAB32X.LIB
     and will be fixed in the next version of the Wintab Programmer's Kit.

   --------------------------------------------------------------------------   
				Project Settings for WTKIT
   --------------------------------------------------------------------------   
     All projects require that the LIB and INCLUDE variables point to
     the appropriate Windows SDK directories.  Also, the WINTAB variable
     must point to a tree containing the LIB and INCLUDE subtrees from the
     Wintab Programmer's Kit.  

     For example, in MSVC5.0, under the Tools menu, Options setting,
     the WINTAB\INCLUDE directory must be listed as a directory for
     Include files, and the WINTAB\LIB\I386 must be listed as a directory
     for library files.

     Please cleanup intermediate files between builds to different
     targets. If you have built the programs from source for one target
     environment, and wish to switch to another target, first run the
     command NMAKE CLEANALL in the source subdirectories.

   --------------------------------------------------------------------------   
				Keeping in Touch
   --------------------------------------------------------------------------   

     Here are three ways to show your support for the Wintab standard, and
     allow us to let people know that your product has Wintab support.

     1. When you complete Wintab support in your product, let us know. Be
     sure to include the product name, a description, and contact
     information for your company.

     2. If possible, send us an evaluation copy of your product, or the
     portions of it that contain Wintab support.  LCS/Telegraphics is
     building a library of Wintab-compliant applications for ongoing
     compatibility testing.

     3. Join the Committee for Advanced Pointing Standards (CAPS). CAPS
     formed in August of 1993 to promote and support future development of
     the specification.  The more than 40 corporate members of CAPS
     include most major pointing device manufacturers, and many leading
     CAD and graphics arts software developers.  To join CAPS, contact:

	LCS/Telegraphics
	150 Rogers St.
	Cambridge, MA 02142
	voice: (617) 225-7970
	fax: (617) 225-7969
	email: caps@pointing.com
	web site: http://www.pointing.com
