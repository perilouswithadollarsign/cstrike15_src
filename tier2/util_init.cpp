//========= Copyright © 2005-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: perform initialization needed in most command line programs
//
//===================================================================================-=//

#include <tier0/platform.h>
#include <tier2/tier2.h>
#include <mathlib/mathlib.h>
#include <tier0/icommandline.h>
#include "tier0/memalloc.h"
#include "tier0/progressbar.h"
#include "tier1/strtools.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static void PrintFReportHandler(char const *job_name, int total_units_to_do, int n_units_completed)
{
	static bool work_in_progress=false;
	static char LastJobName[1024];
	if ( Q_strncmp( LastJobName, job_name, sizeof( LastJobName ) ) )
	{
		if ( work_in_progress )
			printf("..done\n");
		Q_strncpy( LastJobName, job_name, sizeof( LastJobName ) );
	}
 	if ( (total_units_to_do > 0 ) && (total_units_to_do >= n_units_completed) )
	{
		int percent_done=(100*n_units_completed)/total_units_to_do;
		printf("\r%s : %d%%",LastJobName, percent_done );
		work_in_progress = true;
	}
	else
	{
		printf("%s\n",LastJobName);
		work_in_progress = false;
	}
}

void InitCommandLineProgram( int &argc, char ** &argv )
{
	MathLib_Init( 1,1,1,0,false,true,true,true);
	CommandLine()->CreateCmdLine( argc, argv );
	InitDefaultFileSystem();
	InstallProgressReportHandler( PrintFReportHandler );
	// handle -allowdebug transparently
	if ( ( argc > 1 ) && ( !strcmp( argv[1], "-allowdebug" ) ) )
	{
		argv++;												// messes up argv[0]
		argc--;
	}

}
