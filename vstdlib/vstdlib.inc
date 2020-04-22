//-----------------------------------------------------------------------------
//	VSTDLIB.INC
//
//	Project Script
//-----------------------------------------------------------------------------

$Project
{
	$Folder	"Source Files"
	{
		$File	"xbox\___FirstModule.cpp"	[$X360]
		$File	"concommandhash.h"
		$File	"coroutine.cpp"				[!$X360]
		{
			$Configuration
			{
				$Compiler [!$PS3]
				{
					$BasicRuntimeChecks						"Default" 
				}
			}
		}
		$File	"getstackptr64.masm"		[$WIN64]
		{
			$Configuration
			{
				$CustomBuildStep
				{
					// General
					$CommandLine			"$QUOTE$(VCInstallDir)bin\x86_amd64\ml64.exe$QUOTE /nologo /c /Fo$QUOTE$(IntDir)\$(InputName).obj$QUOTE $QUOTE$(InputPath)$QUOTE"
					$Description			"Compiling GetStackPtr64.masm"
					$Outputs			"$(IntDir)\$(InputName).obj"
				}
			}
		}
		$File	"cvar.cpp"
		$File	"jobthread.cpp"
		$File	"keyvaluessystem.cpp"
		$File	"processutils.cpp"			[$WINDOWS]
		$File	"random.cpp"
		$File	"vcover.cpp"
		$File	"eventsystem.cpp"
		$File	"prx.cpp"					[$PS3]
		$File	"prxexport.cpp"		[$PS3]
		$File   "vstrtools.cpp"
		$File   "vstdlib_strtools.cpp"
	}

	$Folder	"Public Header Files"
	{
		$File	"$SRCDIR\public\vstdlib\cvar.h"
		$File	"$SRCDIR\public\vstdlib\coroutine.h"
		$File	"$SRCDIR\public\vstdlib\jobthread.h"
		$File	"$SRCDIR\public\vstdlib\ikeyvaluessystem.h"
		$File	"$SRCDIR\public\vstdlib\iprocessutils.h"
		$File	"$SRCDIR\public\tier1\mempool.h"
		$File	"$SRCDIR\public\vstdlib\random.h"
		$File	"$SRCDIR\public\vstdlib\vcover.h"
		$File	"$SRCDIR\public\vstdlib\vstdlib.h"
		$File	"$SRCDIR\public\vstdlib\ieventsystem.h"
		$File	"$SRCDIR\public\vstdlib\vstrtools.h"
	}
}
