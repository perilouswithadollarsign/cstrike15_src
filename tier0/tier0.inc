//-----------------------------------------------------------------------------
//	TIER0.INC
//
//	Project Script
//-----------------------------------------------------------------------------

$Project
{
	$Folder	"Source Files" [$WINDOWS||$X360||$PS3||$POSIX]
	{
		-$File	"$SRCDIR\public\tier0\memoverride.cpp"  [!$PS3]
	}
}

$Project
{
	$Folder	"Source Files"
	{
		$File	"../unitlib/unitlib.cpp"
		$File	"assert_dialog.cpp"
		$File	"assert_dialog.rc"		[$WIN32||$WIN64]
		$File	"commandline.cpp"
		$File	"cpu.cpp"
		$File	"cpumonitoring.cpp"
		$File   "cputopology.cpp"
		$File	"cpu_posix.cpp"			[$POSIX]
		$File	"dbg.cpp"
		$File	"etwprof.cpp"			[$WINDOWS]
		$File	"fasttimer.cpp"
		$File	"logging.cpp"
		$File	"mem.cpp"
		$File	"mem_helpers.cpp"
		$File	"memdbg.cpp"
		$File	"memprocessheap.cpp"	[$WINDOWS]
		$File	"memvirt.cpp"
		$File	"memstd.cpp"
		$File	"memvalidate.cpp"
		$File	"dynfunction.cpp"
		$File	"minidump.cpp"
		$File	"tier0_strtools.cpp"			
		$File	"miniprofiler.cpp"			
		$File	"perfstats.cpp"
		$File	"platform.cpp" [$WINDOWS||$X360||$PS3]
		$File   "platform_posix.cpp" [$POSIX]
		$File   "platform_independent.cpp"
		$File	"platwindow.cpp"
		$File	"pmc360.cpp"		[$X360]
		$File	"pme.cpp"			[$WIN32||$WIN64]
		$File	"pme_posix.cpp"		[$PS3||$POSIX]
		$File	"PMELib.cpp"		[$WIN32||$WIN64||$PS3]
		$File	"progressbar.cpp"	
		$File	"prx.cpp"			[$PS3]
		$File	"prxexport.cpp"		[$PS3]
		$File	"security.cpp"
		$File	"stackstats.cpp"
		$File	"stacktools.cpp"
		$File	"systeminformation.cpp"
		$File	"threadtools.cpp"
		$File	"tslist.cpp"
		$File	"vatoms.cpp"
		$File	"vprof.cpp"
		$File	"vtuneinterface.cpp"
		$File	"win32consoleio.cpp"

		$Folder	"dlmalloc"
		{
			$File	"dlmalloc\malloc.cpp"	[$WINDOWS||$X360||$PS3]
			$File	"dlmalloc\malloc-2.8.3.h"
		}

		$File "xbox/xbox_system.cpp" [$X360||$PS3]
		$File	"../tier1/pathmatch.cpp" [$LINUXALL]

	}

	$folder	"Header Files"
	{
		$File	"$SRCDIR\public\tier0\basetypes.h"
		$File	"$SRCDIR\public\tier0\commonmacros.h"
		$File	"$SRCDIR\public\tier0\cache_hints.h"
		$File	"$SRCDIR\public\tier0\cpumonitoring.h"
		$File	"cputopology.h"
		$File	"$SRCDIR\public\tier0\dbg.h"
		$File	"$SRCDIR\public\tier0\dbgflag.h"
		$File	"$SRCDIR\public\tier0\etwprof.h"
		$File	"$SRCDIR\public\tier0\EventMasks.h"
		$File	"$SRCDIR\public\tier0\EventModes.h"
		$File	"$SRCDIR\public\tier0\fasttimer.h"
		$File	"$SRCDIR\public\tier0\ia32detect.h"
		$File	"$SRCDIR\public\tier0\icommandline.h"
		$File	"$SRCDIR\public\tier0\IOCTLCodes.h"
		$File	"$SRCDIR\public\tier0\K8PerformanceCounters.h"
		$File	"$SRCDIR\public\tier0\l2cache.h"
		$File	"$SRCDIR\public\tier0\pmc360.h"		[$X360]
		$File	"$SRCDIR\public\tier0\logging.h"
		$File	"$SRCDIR\public\tier0\mem.h"
		$File	"mem_helpers.h"
		$File	"mem_impl_type.h"
		$File	"$SRCDIR\public\tier0\memalloc.h"
		$File	"$SRCDIR\public\tier0\memdbgoff.h"
		$File	"$SRCDIR\public\tier0\memdbgon.h"
		$File	"$SRCDIR\public\tier0\memoverride_ps3.h"	[$PS3]
		$File	"memstd.h"
		$File	"$SRCDIR\public\tier0\memvirt.h"
		$File	"$SRCDIR\public\tier0\minidump.h"
		$File	"$SRCDIR\public\tier0\miniprofiler.h"
		$File	"$SRCDIR\public\tier0\microprofiler.h"
		$File	"$SRCDIR\public\tier0\P4PerformanceCounters.h"
		$File	"$SRCDIR\public\tier0\P5P6PerformanceCounters.h"
		$File	"pch_tier0.h"
		$File	"tier0_strtools.h"
		$File	"$SRCDIR\public\tier0\perfstats.h"
		$File	"$SRCDIR\public\tier0\platform.h"
		$File	"$SRCDIR\public\tier0\platform_override.h"	[$PS3]
		$File	"$SRCDIR\public\tier0\platwindow.h"
		$File	"$SRCDIR\public\tier0\PMELib.h"
		$File	"$SRCDIR\public\tier0\progressbar.h"
		$File	"$SRCDIR\public\tier0\protected_things.h"
		$File	"resource.h"
		$File	"$SRCDIR\public\tier0\stackstats.h"
		$File	"$SRCDIR\public\tier0\stacktools.h"
		$File	"$SRCDIR\public\tier0\systeminformation.h"
		$File	"$SRCDIR\public\tier0\threadtools.h"
		$File	"$SRCDIR\public\tier0\threadtools.inl"
		$File	"$SRCDIR\public\tier0\tslist.h"
		$File	"$SRCDIR\public\tier0\validator.h"
		$File	"$SRCDIR\public\tier0\valobject.h"
		$File	"$SRCDIR\public\tier0\valve_off.h"
		$File	"$SRCDIR\public\tier0\valve_on.h"
		$File	"$SRCDIR\public\tier0\vatoms.h"
		$File	"$SRCDIR\public\tier0\vprof.h"
		$File	"$SRCDIR\public\tier0\vtuneinterface.h"
		$File	"$SRCDIR\public\tier0\vprof_sn.h"
		$File	"$SRCDIR\public\tier0\vprof_telemetry.h"
		$File	"$SRCDIR\public\tier0\wchartypes.h"
		$File	"$SRCDIR\public\tier0\win32consoleio.h"
		$File	"$SRCDIR\public\tier0\xbox_codeline_defines.h"
		$File	"$SRCDIR\public\tier0\dynfunction.h"
	}

	$Folder	"DESKey" [$WIN32||$WIN64]
	{
		$File	"DESKey\ALGO.H"
		$File	"DESKey\ALGO32.LIB"
		$File	"DESKey\DK2WIN32.H"
		$File	"DESKey\DK2WIN32.LIB"
	}

	$Folder "Xbox" [$X360]
	{
		$folder "Source Files"
		{
 			$File	"xbox\xbox_console.cpp"
			$File	"xbox\xbox_system.cpp"
			$File	"xbox\xbox_win32stubs.cpp"
		}
		$folder "Header Files"
		{
			$File	"$SRCDIR\common\xbox\xbox_console.h"
			$File	"$SRCDIR\common\xbox\xbox_core.h"
			$File	"$SRCDIR\common\xbox\xbox_win32stubs.h"
		}
	}

	$Folder "PS3 debugger" [$PS3]
	{
		$folder "Source Files"
		{
 			$File	"ps3\ps3_console.cpp"
		}
		$folder "Header Files"
		{
			$File	"$SRCDIR\common\ps3\ps3_console.h"
			$File	"$SRCDIR\common\ps3\ps3_win32stubs.h"
		}
	}

	$Folder "Manifest Files" [$WINDOWS]
	{
		$File	"valveetwprovider.man"
		{
			$Configuration
			{
				$CustomBuildStep
				{
					$CommandLine	"mc.exe -um $(InputFilename) -z $(InputName)Events"
					$Description	"Compiling ETW manifest file"
					$Outputs		"$(InputName)Events.h;$(InputName)Events.rc"
				}
			}
		}
	}
}
