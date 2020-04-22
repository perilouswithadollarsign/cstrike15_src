//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"


extern void V_MakeAbsoluteCygwinPath( char *pOut, int outLen, const char *pRelativePath );

extern void MakeFriendlyProjectName( char *pchProject )
{
	int strLen =  V_strlen( pchProject );
	for ( int j = 0; j < strLen; j++ )
	{
		if ( pchProject[j] == ' ' )
			pchProject[j] = '_';
		if ( pchProject[j] == '(' || pchProject[j] == ')' )
		{
			V_memmove( pchProject+j, pchProject+j+1, strLen - j );
			strLen--;
		}
	}	
}


class CSolutionGenerator_Makefile : public IBaseSolutionGenerator
{
private:
	void GenerateProjectNames( CUtlVector<CUtlString> &projNames, CUtlVector<CDependency_Project*> &projects )
	{
		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];
			char szFriendlyName[256];
			V_strncpy( szFriendlyName, pCurProject->m_ProjectName.String(), sizeof(szFriendlyName) );
			MakeFriendlyProjectName( szFriendlyName );
			projNames[ projNames.AddToTail() ] = szFriendlyName;
		}
	}
	
public:
	virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects )
	{
		// Default extension.
		char szTmpSolutionFilename[MAX_PATH];
		if ( !V_GetFileExtension( pSolutionFilename ) )
		{
			V_snprintf( szTmpSolutionFilename, sizeof( szTmpSolutionFilename ), "%s.mak", pSolutionFilename );
			pSolutionFilename = szTmpSolutionFilename;
		}

		Msg( "\nWriting master makefile %s.\n\n", pSolutionFilename );

		// Write the file.
		FILE *fp = fopen( pSolutionFilename, "wt" );
		if ( !fp )
			g_pVPC->VPCError( "Can't open %s for writing.", pSolutionFilename );

		fprintf( fp, "# VPC MASTER MAKEFILE\n\n\n" );

		fprintf( fp, "\nSHELL:=/bin/bash\n" );
		
		fprintf( fp, "# to control parallelism, set the MAKE_JOBS environment variable\n" );
		fprintf( fp, "ifeq ($(strip $(MAKE_JOBS)),)\n");
		fprintf( fp, "\tifeq ($(shell uname),Darwin)\n\t\tCPUS:=$(shell /usr/sbin/sysctl -n hw.ncpu)\n\tendif\n");
		fprintf( fp, "\tifeq ($(shell uname),Linux)\n\t\tCPUS:=$(shell grep processor /proc/cpuinfo | wc -l)\n\tendif\n");
		fprintf( fp, "\tMAKE_JOBS:=-j$(CPUS)\n" );
		fprintf( fp, "endif\n\n" );

		fprintf( fp, "ifeq ($(strip $(MAKE_JOBS)),)\n");
		fprintf( fp, "\tMAKE_JOBS:=-j8\n" );
		fprintf( fp, "endif\n\n" );
		
		// First, make a target with all the project names.
		fprintf( fp, "# All projects (default target)\n" );
		fprintf( fp, "all: \n" );
		fprintf( fp, "\t$(MAKE) -f $(lastword $(MAKEFILE_LIST)) $(MAKE_JOBS) all-targets\n\n" );

		fprintf( fp, "all-targets : " );
		
		CUtlVector<CUtlString> projNames;
		GenerateProjectNames( projNames, projects ); 
							 
		for ( int i=0; i < projects.Count(); i++ )
		{
			fprintf( fp, "%s ", projNames[i].String() );
		}

		fprintf( fp, "\n\n\n# Individual projects + dependencies\n\n" );
		
		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];
			
			CUtlVector<CDependency_Project*> additionalProjectDependencies;
			ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );

			fprintf( fp, "%s : ", projNames[i].String() );

			for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
			{
				if ( i == iTestProject )
					continue;

				CDependency_Project *pTestProject = projects[iTestProject];
				int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
				if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
				{
					fprintf( fp, "%s ", projNames[iTestProject].String() );
				}
			}

			// Now add the code to build this thing.
			char sDirTemp[MAX_PATH], sDir[MAX_PATH];
			V_strncpy( sDirTemp, pCurProject->m_ProjectFilename.String(), sizeof( sDirTemp ) );
			V_StripFilename( sDirTemp );
			V_MakeAbsoluteCygwinPath( sDir, sizeof( sDir ), sDirTemp );
			
			const char *pFilename = V_UnqualifiedFileName( pCurProject->m_ProjectFilename.String() );

			fprintf( fp, "\n\t+cd %s && $(MAKE) -f %s $(CLEANPARAM)", sDir, pFilename );

			fprintf( fp, "\n\n" );
		}

		fprintf( fp, "# this is a bit over-inclusive, but the alternative (actually adding each referenced c/cpp/h file to\n" );
		fprintf( fp, "# the tags file) seems like more work than it's worth.  feel free to fix that up if it bugs you. \n" );
		fprintf( fp, "TAGS:\n" );
		fprintf( fp, "\t@rm -f TAGS\n" );
		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];
			char sDirTemp[MAX_PATH], sDir[MAX_PATH];
			V_strncpy( sDirTemp, pCurProject->m_ProjectFilename.String(), sizeof( sDirTemp ) );
			V_StripFilename( sDirTemp );
			V_MakeAbsoluteCygwinPath( sDir, sizeof( sDir ), sDirTemp );
			fprintf( fp, "\t@find %s -name \'*.cpp\' -print0 | xargs -0 etags --declarations --ignore-indentation --append\n", sDir );
			fprintf( fp, "\t@find %s -name \'*.h\' -print0 | xargs -0 etags --language=c++ --declarations --ignore-indentation --append\n", sDir );
			fprintf( fp, "\t@find %s -name \'*.c\' -print0 | xargs -0 etags --declarations --ignore-indentation --append\n", sDir );
		}
		fprintf( fp, "\n\n" );

		fprintf( fp, "\n# Mark all the projects as phony or else make will see the directories by the same name and think certain targets \n\n" );
		fprintf( fp, ".PHONY: TAGS showtargets regen showregen clean cleantargets relink " );
		for ( int i=0; i < projects.Count(); i++ )
		{
			fprintf( fp, "%s ", projNames[i].String() );
		}
		fprintf( fp, "\n\n\n" );

		fprintf( fp, "\n# The standard clean command to clean it all out.\n" );
		fprintf( fp, "\nclean: \n" );
		fprintf( fp, "\t$(MAKE) -f $(lastword $(MAKEFILE_LIST)) $(MAKE_JOBS) all-targets CLEANPARAM=clean\n\n\n" );

        fprintf( fp, "\n# clean targets, so we re-link next time.\n" );
		fprintf( fp, "\ncleantargets: \n" );
		fprintf( fp, "\t$(MAKE) -f $(lastword $(MAKEFILE_LIST)) $(MAKE_JOBS) all-targets CLEANPARAM=cleantargets\n\n\n" );
	
		fprintf( fp, "\n#relink\n" );
		fprintf( fp, "\nrelink: cleantargets \n" );
		fprintf( fp, "\t$(MAKE) -f $(lastword $(MAKEFILE_LIST)) $(MAKE_JOBS) all-targets\n\n\n" );
	
        
		
		// Create the showtargets target.
		fprintf( fp, "\n# Here's a command to list out all the targets\n\n" );
		fprintf( fp, "\nshowtargets: \n" );
		fprintf( fp, "\t@echo '-------------------' && \\\n" );
		fprintf( fp, "\techo '----- TARGETS -----' && \\\n" );
		fprintf( fp, "\techo '-------------------' && \\\n" );
		fprintf( fp, "\techo 'clean' && \\\n" );
		fprintf( fp, "\techo 'regen' && \\\n" );
		fprintf( fp, "\techo 'showregen' && \\\n" );
		for ( int i=0; i < projects.Count(); i++ )
		{
			fprintf( fp, "\techo '%s'", projNames[i].String() );
			if ( i != projects.Count()-1 )
				fprintf( fp, " && \\" );
			fprintf( fp, "\n" );
		}
		fprintf( fp, "\n\n" );

		
		// Create the regen target.
		fprintf( fp, "\n# Here's a command to regenerate this makefile\n\n" );
		fprintf( fp, "\nregen: \n" );
		fprintf( fp, "\t" );
		ICommandLine *pCommandLine = CommandLine();
		for ( int i=0; i < pCommandLine->ParmCount(); i++ )
		{
			fprintf( fp, "%s ", pCommandLine->GetParm( i ) );
		}
		fprintf( fp, "\n\n" );

		
		// Create the showregen target.
		fprintf( fp, "\n# Here's a command to list out all the targets\n\n" );
		fprintf( fp, "\nshowregen: \n" );
		fprintf( fp, "\t@echo " );
		for ( int i=0; i < pCommandLine->ParmCount(); i++ )
		{
			fprintf( fp, "%s ", pCommandLine->GetParm( i ) );
		}
		fprintf( fp, "\n\n" );


		fclose( fp );
	}
	
	void ResolveAdditionalProjectDependencies( 
		CDependency_Project *pCurProject, 
		CUtlVector<CDependency_Project*> &projects, 
		CUtlVector<CDependency_Project*> &additionalProjectDependencies )
	{
		for ( int i=0; i < pCurProject->m_AdditionalProjectDependencies.Count(); i++ )
		{
			const char *pLookingFor = pCurProject->m_AdditionalProjectDependencies[i].String();
			
			int j;
			for ( j=0; j < projects.Count(); j++ )
			{
				if ( V_stricmp( projects[j]->m_ProjectName.String(), pLookingFor ) == 0 )
					break;
			}
			
			if ( j == projects.Count() )
				g_pVPC->VPCError( "Project %s lists '%s' in its $AdditionalProjectDependencies, but there is no project by that name.", pCurProject->GetName(), pLookingFor );
				
			additionalProjectDependencies.AddToTail( projects[j] );
		}
	}

	const char* FindInFile( const char *pFilename, const char *pFileData, const char *pSearchFor )
	{
		const char *pPos = V_stristr( pFileData, pSearchFor );
		if ( !pPos )
			g_pVPC->VPCError( "Can't find ProjectGUID in %s.", pFilename );

		return pPos + V_strlen( pSearchFor );
	}
};


static CSolutionGenerator_Makefile g_SolutionGenerator_Makefile;
IBaseSolutionGenerator* GetSolutionGenerator_Makefile()
{
	return &g_SolutionGenerator_Makefile;
}



