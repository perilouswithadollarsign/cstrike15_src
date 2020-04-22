//======= Copyright 1996-2016, Valve Corporation, All rights reserved. ========
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void MakeFriendlyProjectName( char *pchProject )
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
		CUtlPathStringHolder tmpSolutionFilename;
		if ( !V_GetFileExtension( pSolutionFilename ) )
		{
			tmpSolutionFilename.Set( pSolutionFilename, ".mak" );
			pSolutionFilename = tmpSolutionFilename.Get();
		}

		Msg( "\nWriting master makefile %s.\n\n", pSolutionFilename );

		// Write the file.
		FILE *fp = fopen( pSolutionFilename, "wt" );
		if ( !fp )
			g_pVPC->VPCError( "Can't open %s for writing.", pSolutionFilename );

		fprintf( fp, "# VPC MASTER MAKEFILE\n\n\n" );
/*
		fprintf( fp, "# Disable built-in rules/variables. We don't depend on them, and they slow down make processing.\n" );
		fprintf( fp, "MAKEFLAGS += --no-builtin-rules --no-builtin-variables\n" );
		fprintf( fp, "ifeq ($(MAKE_VERBOSE),)\n" );
		fprintf( fp, "MAKEFLAGS += --no-print-directory\n" );
		fprintf( fp, "endif\n\n");
*/
		fprintf( fp, "ifneq \"$(LINUX_TOOLS_PATH)\" \"\"\n" );
		fprintf( fp, "TOOL_PATH = $(LINUX_TOOLS_PATH)/\n" );
		fprintf( fp, "SHELL := $(TOOL_PATH)bash\n" );
		fprintf( fp, "else\n" );
		fprintf( fp, "SHELL := /bin/bash\n" );
		fprintf( fp, "endif\n" );
		fprintf( fp, "\n");
		fprintf( fp, "ifdef MAKE_CHROOT\n");
		fprintf( fp, "    ifneq (\"$(wildcard /etc/schroot/chroot.d/$(MAKE_CHROOT).conf)\",\"\")\n" );
		fprintf( fp, "        export CHROOT_NAME ?= $(MAKE_CHROOT)\n" );
		fprintf( fp, "    endif\n" );
		fprintf( fp, "    export CHROOT_NAME ?= $(subst /,_,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))\n");
		fprintf( fp, "    CHROOT_CONF := /etc/schroot/chroot.d/$(CHROOT_NAME).conf\n");
		fprintf( fp, "    ifeq \"$(CHROOT_NAME)\" \"steamrt_scout_amd64\"\n" );
		fprintf( fp, "        CHROOT_DIR := /var/chroots\n" );
		fprintf( fp, "    else\n" );
		fprintf( fp, "        CHROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/tools/runtime/linux)\n");
		fprintf( fp, "    endif\n" );
		fprintf( fp, "    RUNTIME_NAME ?= steamrt_scout_amd64\n");
		fprintf( fp, "    ifneq (\"$(SCHROOT_CHROOT_NAME)\", \"$(CHROOT_NAME)\")\n");
		fprintf( fp, "        SHELL:=schroot --chroot $(CHROOT_NAME) -- /bin/bash\n");
		fprintf( fp, "    endif\n");
		fprintf( fp, "\n" );
		fprintf( fp, "    CHROOT_TARBALL = $(CHROOT_DIR)/$(RUNTIME_NAME).tar.xz\n" );
		fprintf( fp, "    CHROOT_TIMESTAMP_FILE = $(CHROOT_DIR)/$(RUNTIME_NAME)/timestamp\n" );
		fprintf( fp, "\n");
		fprintf( fp, "    ifneq (\"$(wildcard $(CHROOT_TIMESTAMP_FILE))\",\"\")\n" );
		fprintf( fp, "        ifneq (\"$(wildcard $(CHROOT_TARBALL))\",\"\")\n" );
		fprintf( fp, "            CHROOT_DEPENDENCY = $(CHROOT_CONF)\n" );
		fprintf( fp, "        endif\n" );
		fprintf( fp, "    endif\n" );

		fprintf( fp, "endif\n");

		fprintf( fp, "ECHO = $(TOOL_PATH)echo\n" );
		fprintf( fp, "ETAGS = $(TOOL_PATH)etags\n" );
		fprintf( fp, "FIND = $(TOOL_PATH)find\n" );
		fprintf( fp, "UNAME = $(TOOL_PATH)uname\n" );
		fprintf( fp, "XARGS = $(TOOL_PATH)xargs\n" );
		fprintf( fp, "\n");
		
		fprintf( fp, "# to control parallelism, set the MAKE_JOBS environment variable\n" );
		fprintf( fp, "ifeq ($(strip $(MAKE_JOBS)),)\n");
		fprintf( fp, "    ifeq ($(shell $(UNAME)),Darwin)\n" );
		fprintf( fp, "        CPUS := $(shell /usr/sbin/sysctl -n hw.ncpu)\n" );
		fprintf( fp, "    endif\n" );
		fprintf( fp, "    ifeq ($(shell $(UNAME)),Linux)\n" );
		fprintf( fp, "        CPUS := $(shell $(TOOL_PATH)grep processor /proc/cpuinfo | $(TOOL_PATH)wc -l)\n" );
		fprintf( fp, "    endif\n" );
		fprintf( fp, "    MAKE_JOBS := $(CPUS)\n" );
		fprintf( fp, "endif\n\n" );

		fprintf( fp, "ifeq ($(strip $(MAKE_JOBS)),)\n");
		fprintf( fp, "    MAKE_JOBS := 8\n" );
		fprintf( fp, "endif\n\n" );
		
		// First, make a target with all the project names.
		fprintf( fp, "# All projects (default target)\n" );
		fprintf( fp, "all: $(CHROOT_DEPENDENCY)\n" );
		fprintf( fp, "\t$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets\n\n" );

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
			CUtlPathStringHolder fileDir( pCurProject->m_Filename );
			fileDir.StripFilename();
			fileDir.FixSlashes( '/' );
			
			const char *pFilename = pCurProject->GetProjectFileName();

			fprintf( fp, "\n\t@$(ECHO) \"Building: %s\"", projNames[i].String());
			fprintf( fp, "\n\t@+$(MAKE) -C %s -f %s $(CLEANPARAM) SHELL=$(SHELL)", fileDir.Get(), pFilename );

			fprintf( fp, "\n\n" );
		}

		fprintf( fp, "# this is a bit over-inclusive, but the alternative (actually adding each referenced c/cpp/h file to\n" );
		fprintf( fp, "# the tags file) seems like more work than it's worth.  feel free to fix that up if it bugs you. \n" );
		fprintf( fp, "TAGS:\n" );
		fprintf( fp, "\t@$(TOOL_PATH)rm -f TAGS\n" );
		for ( int i=0; i < projects.Count(); i++ )
		{
			CDependency_Project *pCurProject = projects[i];
			CUtlPathStringHolder fileDir( pCurProject->GetProjectFileName() );
			fileDir.StripFilename();
			fileDir.FixSlashes( '/' );
			fprintf( fp, "\t@$(FIND) %s -name \'*.cpp\' -print0 | $(XARGS) -r0 $(ETAGS) --declarations --ignore-indentation --append\n", fileDir.Get() );
			fprintf( fp, "\t@$(FIND) %s -name \'*.h\' -print0 | $(XARGS) -r0 $(ETAGS) --language=c++ --declarations --ignore-indentation --append\n", fileDir.Get() );
			fprintf( fp, "\t@$(FIND) %s -name \'*.c\' -print0 | $(XARGS) -r0 $(ETAGS) --declarations --ignore-indentation --append\n", fileDir.Get() );
		}
		fprintf( fp, "\n\n" );

		fprintf( fp, "\n# Mark all the projects as phony or else make will see the directories by the same name and think certain targets \n\n" );
		fprintf( fp, ".PHONY: TAGS all all-targets showtargets regen showregen clean cleantargets cleanandremove relink " );
		for ( int i=0; i < projects.Count(); i++ )
		{
			fprintf( fp, "%s ", projNames[i].String() );
		}
		fprintf( fp, "\n\n\n" );

		fprintf( fp, "\n# The standard clean command to clean it all out.\n" );
		fprintf( fp, "\nclean: \n" );
		fprintf( fp, "\t@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets CLEANPARAM=clean\n\n\n" );

		fprintf( fp, "\n# clean targets, so we re-link next time.\n" );
		fprintf( fp, "\ncleantargets: \n" );
		fprintf( fp, "\t@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets CLEANPARAM=cleantargets\n\n\n" );
	
		fprintf( fp, "\n# p4 edit and remove targets, so we get an entirely clean build.\n" );
		fprintf( fp, "\ncleanandremove: \n" );
		fprintf( fp, "\t@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets CLEANPARAM=cleanandremove\n\n\n" );
	
		fprintf( fp, "\n#relink\n" );
		fprintf( fp, "\nrelink: cleantargets \n" );
		fprintf( fp, "\t@$(MAKE) -f $(lastword $(MAKEFILE_LIST)) -j$(MAKE_JOBS) all-targets\n\n\n" );
	
		
		
		// Create the showtargets target.
		fprintf( fp, "\n# Here's a command to list out all the targets\n\n" );
		fprintf( fp, "\nshowtargets: \n" );
		fprintf( fp, "\t@$(ECHO) '-------------------' && \\\n" );
		fprintf( fp, "\t$(ECHO) '----- TARGETS -----' && \\\n" );
		fprintf( fp, "\t$(ECHO) '-------------------' && \\\n" );
		fprintf( fp, "\t$(ECHO) 'clean' && \\\n" );
		fprintf( fp, "\t$(ECHO) 'regen' && \\\n" );
		fprintf( fp, "\t$(ECHO) 'showregen' && \\\n" );
		for ( int i=0; i < projects.Count(); i++ )
		{
			fprintf( fp, "\t$(ECHO) '%s'", projNames[i].String() );
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
		fprintf( fp, "\t@$(ECHO) " );
		for ( int i=0; i < pCommandLine->ParmCount(); i++ )
		{
			fprintf( fp, "%s ", pCommandLine->GetParm( i ) );
		}
		fprintf( fp, "\n\n" );

		// Warn if the chroot is out of date.
		fprintf( fp, "ifdef CHROOT_DEPENDENCY\n"
				"$(CHROOT_DEPENDENCY): $(CHROOT_TIMESTAMP_FILE)\n"
				"$(CHROOT_TIMESTAMP_FILE): $(CHROOT_TARBALL)\n"
				"\t@echo \"chroot ${CHROOT_NAME} at $(CHROOT_DIR) is out of date\"\n"
				"\t@echo You need to re-run sudo src/tools/runtime/linux/configure_runtime.sh ${CHROOT_NAME}\n"
				"endif\n");


		fclose( fp );
	}
	
	virtual const char *GetSolutionFileExtension() { return "mak"; }

	virtual SolutionType_t GetSolutionType( void ) { return ST_MAKEFILE; }

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
				if ( V_stricmp_fast( projects[j]->m_ProjectName.String(), pLookingFor ) == 0 )
					break;
			}

			// If we can't resolve the dependency just ignore it.
			// This fits a common usage pattern where something
			// may depend on tier0 (or whatever) but you don't want
			// to have tier0 in the solution since you aren't
			// changing it and will just used the checked-in lib.
			if ( j < projects.Count() )
			{
				additionalProjectDependencies.AddToTail( projects[j] );
			}
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
IBaseSolutionGenerator* GetMakefileSolutionGenerator()
{
	return &g_SolutionGenerator_Makefile;
}



