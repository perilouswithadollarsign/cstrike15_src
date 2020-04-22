   ------------------------------- readme.txt -------------------------------   

     Wintab(tm) Programmer's Kit V. 1.26 for 16- and 32-bit Windows APIs.

     This file contains programmer's kit installation instructions and
     programming notes.

     Please direct your programming questions and comments to:

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
				Legal/Disclaimer
   --------------------------------------------------------------------------   

     The Wintab specification is intended to be an open standard. The
     Wintab Programmer's Kit is an aid to understanding the Wintab
     standard and implementing Wintab-compliant applications.  As such,
     the text and information contained herein may be freely used, copied,
     or distributed without compensation or licensing restrictions.

     The Wintab Programmer's Kit is copyright 1991-1998 by
     LCS/Telegraphics.

     LCS/Telegraphics does not assume any liability for damages resulting
     from the use of the information contained herein.

   --------------------------------------------------------------------------   
				  Installation
   --------------------------------------------------------------------------   

     * Getting the latest Wintab driver files

     To get the latest Wintab driver files, including WINTAB.DLL and
     WINTAB32.DLL, contact your device manufacturer. Follow the
     manufacturer's installation directions.

     * Installing the development files

     The files are in several subdirectories on this disk. To copy the
     whole tree to your hard disk, use XCOPY with the /S flag. 

     Example:
     	xcopy a:\*.* c:\wintab\ /s

     * Installing the sample programs and testing your configuration

     To test your Wintab driver configuration, build and run the
     sample programs. (Previous versions of this kit included executable
     of all the samples, but as Windows platforms proliferate, these have	
     been dropped to control the size of the kit.)  The source code for
     the programs is in stored in subdirectories by program name. 

     * Project settings for WTKIT
     All projects require that the LIB and INCLUDE variables point to
     the appropriate Windows SDK directories.  Also, the WINTAB variable
     must point to a tree containing the LIB and INCLUDE subtrees from the
     Wintab Programmer's Kit.  

     For example, in MSVC 5.0, under the Tools menu, Options setting,
     the WINTAB\INCLUDE directory must be listed as a directory for
     Include files, and the WINTAB\LIB\I386 must be listed as a directory
     for library files.  Also, please see the note about linker warnings
     during the debug build under the WNTAB32X.LIB section of this document.

     * Compiler notes
     Makefiles compatible with Microsoft NMAKE are in the win16
     subdirectories.  Project files for Microsoft Visual C versions 4.2
     and 5.0 are in the win32 subdirectories. 
	
     MSVC 5.0 will be able to compile all the projects. 

     MSVC 4.2 will not be able to compile MFC_DEMO and WTHOOK
     projects.  

     * Cleaning intermediate files before switching targets 
     Please cleanup intermediate files between builds to different targets.
     If you have built the programs from source for one target environment,
     and wish to switch to another target, first run the command NMAKE
     CLEANALL in the source subdirectories.

   --------------------------------------------------------------------------   
		       Using WINTABX.LIB and WNTAB32X.LIB
   --------------------------------------------------------------------------   

     Two types of import libraries are provided. WINTAB.LIB and
     WINTAB32.LIB are regular import libraries created with the SDK
     utilities. Any module linked with WINTAB.LIB or WINTAB32.LIB will not
     load unless Wintab API support is present. WINTABX.LIB and
     WNTAB32X.LIB are more flexible.  They link each function as it is
     invoked, using the Windows functions LoadLibrary and GetProcAddress. 
     If the link fails, the user will see a message box saying "cannot
     find WINTAB.DLL", and the function will return 0 (the failure code
     for all WINTAB functions) to its caller.  Thus, WINTABX and WNTAB32X
     provide the flexibility of run-time linking, while reducing the code
     burden.

     WINTABX and WNTAB32X do add one new requirement, though.  Since Wintab
     was explicitly linked using LoadLibrary, it must be explicitly freed
     using FreeLibrary when shutting down.  WINTABX and WNTAB32X provide a
     new function, _UnlinkWintab, that calls FreeLibrary and resets the
     automatic function-linking mechanism.  The C declaration is included 
     in the header file WINTABX.H.

     If you want to avoid the "cannot find WINTAB.DLL" message when Wintab
     is not present, you can use the Windows function SetErrorMode() to
     suppress the error box while issuing a Wintab function call. For
     example:

	BOOL WintabHere(void)
	{
		/*-----
		Call this function before any Wintab API
		function when using WINTABX.LIB.
	
		This function returns TRUE if Wintab is
		present and functioning; FALSE otherwise.
		No warning message will be displayed if
		the WINTAB.DLL file is not found.
		-----*/
	
		WORD errmode;
		BOOL fResult;
	
		/* suppress error box */
		errmode = SetErrorMode(SEM_NOOPENFILEERRORBOX
	#ifdef WIN32
								| SEM_FAILCRITICALERRORS
	#endif
								);
	
		/* try wintab */
		fResult = WTInfo(0,0,NULL);
	
		/* restore previous error mode */
		SetErrorMode(errmode);
	
		/* return wintab result */
		return fResult;
	}

     *Note on MSVC 5.0 projects using WNTAB32X.LIB
     Note that the linker warning LNK4098 (to recompile with the
     /Nodefaultlib:LIBC option) during the compilation of Debug projects
     will not affect the operation of the resulting executable.  This bug
     will be fixed in the next version of the Wintab Programmer's Kit.
     For example, the WTHOOK project will give this linker warning during
     debug compilation, but the resulting executable is functional.

   --------------------------------------------------------------------------   
		Determining whether 1.1 features are supported
   --------------------------------------------------------------------------   

     To determine which version of the Wintab specification your device
     supports, use the result of the following function call:
     	WTINFO(WTI_INTERFACE,IFC_SPECVERSION)
     Please see MGRTEST for an example.
     The specification version number (either 1.0 or 1.1) will be
     returned.  If you need 1.1 features for your application, and you want to
     make sure they are supported in a tablet, please email us at
     wintab@pointing.com

   --------------------------------------------------------------------------   
			Using Non-Microsoft C Compilers
   --------------------------------------------------------------------------   

     WINTAB.LIB and WINTAB32.LIB are implicit import libraries created and
     tested with Microsoft C tools.  Some linkers from other vendors may
     not work with these versions of the libraries. This Programmer's Kit
     now includes the corresponding WINTAB.DEF and WINTAB32.DEF files used
     to create WINTAB.LIB and WINTAB32.LIB.  Use the .DEF files with your
     vendor's IMPLIB or librarian tools to create compatible import
     libraries.

     WINTABX.LIB and WNTAB32X.LIB have been tested with Microsoft C, but
     not with other C or C++ compilers, or compilers for other languages.
     Source code for these libraries are in the directories WINTABX and
     WNTAB32X.

   --------------------------------------------------------------------------  
				 RULE and RULE2
   --------------------------------------------------------------------------    

     Rule and Rule2 are versions of the same program. Rule demonstrates
     the limitations of a polled input approach in Windows; Rule2
     demonstrates a more robust, message-based approach, and adds a few
     other enhancements.

     To see what's bad about Rule, run it, click on it but don't start
     measuring.  Now turn off your Wintab device. Windows is now hung.

     If you try the same experiment with Rule2, you can still use the
     keyboard.

   --------------------------------------------------------------------------   
			   MGRTEST as a Debugging Aid
   --------------------------------------------------------------------------   

     Besides demonstrating the WTMgr* APIs, MGRTEST is useful in itself.
     You can use MGRTEST to see what contexts are active, their modes,
     status, and active areas, as well as the current overlap order. It
     can be very useful in understanding how various Wintab-aware programs
     use contexts.

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
