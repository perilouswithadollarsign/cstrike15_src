//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#pragma once

#include "tier1/utlstring.h"

//
// Usage:
// 
//	StartProject
//		StartConfigurationBlock	
//			StartPropertySection
//				HandleProperty...
//			EndPropertySection
//		EndConfigurationBlock
//
//		AddFile...
//			[inside each file it can do another configuration block as above]
//			[also, files can be put in folders with StartFolder/AddFolder]
//	EndProject
//
class IBaseProjectGenerator
{
public:
	// What file extension does this use? (vcproj, mak, vpj).
	virtual const char* GetProjectFileExtension() = 0;

	// Called before doing anything in a project (in g_pVPC->GetOutputFilename()).
	virtual void StartProject() = 0;
	virtual void EndProject( bool bSaveData ) = 0;

	// Access the project name.
	virtual const char *GetProjectName() = 0;
	virtual void SetProjectName( const char *pProjectName ) = 0;

	// Get a list of all configurations.
	virtual void GetAllConfigurationNames( CUtlVector< CUtlString > &configurationNames ) = 0;

	// Configuration data is specified in between these calls and inside BeginPropertySection/EndPropertySection.
	// If bFileSpecific is set, then the configuration data only applies to the last file added.
	virtual void StartConfigurationBlock( const char *pConfigName, bool bFileSpecific ) = 0;
	virtual void EndConfigurationBlock() = 0;

	// Get the current configuration name. Only valid between StartConfigurationBlock()/EndConfigurationBlock()
	virtual const char *GetCurrentConfigurationName() = 0;

	// These functions are called when it enters a section like $Compiler, $Linker, etc.
	// In between the BeginPropertySection/EndPropertySection, it'll call HandleProperty for any properties inside that section.
	virtual bool StartPropertySection( configKeyword_e keyword, bool *pbShouldSkip = NULL ) = 0;
	virtual void HandleProperty( const char *pProperty, const char *pCustomScriptData=NULL ) = 0;
	virtual const char *GetPropertyValue( const char *pProperty ) = 0;
	virtual void EndPropertySection( configKeyword_e keyword ) = 0;

	// Files go in folders. The generator should maintain a stack of folders as they're added.
	virtual void StartFolder( const char *pFolderName, VpcFolderFlags_t iFlags ) = 0;
	virtual void EndFolder() = 0;
	
	// Add files. Any config blocks/properties between StartFile/EndFile apply to this file only.
	// It will only ever have one active file.
	virtual bool StartFile( const char *pFilename, VpcFileFlags_t iFlags, bool bWarnIfAlreadyExists ) = 0;
	virtual void EndFile() = 0;

    virtual bool HasFile( const char *pFilename ) = 0;
        
	// Only valid between StartFile and EndFile
	virtual const char *GetCurrentFileName() = 0;

	// This is actually just per-file configuration data.
	virtual void FileExcludedFromBuild( bool bExcluded ) = 0;

	// Remove the specified file. return true if success
	virtual bool RemoveFile( const char *pFilename ) = 0;

	const char *GetOutputFileName( void )
	{
		if ( m_OutputFileName.IsEmpty() )
		{
			SetOutputFileName();
		}

		return m_OutputFileName;
	}

	const char *GetGUIDString()
	{ 
		if ( m_GUIDString.IsEmpty() )
		{
			SetGUID();
		}
		
		return m_GUIDString;
	}

	
	virtual void SetGUID( void );
	virtual void SetOutputFileName( void );

	virtual void EnumerateSupportedVPCTargetPlatforms( CUtlVector<CUtlString> &output ) = 0;
	virtual bool BuildsForTargetPlatform( const char *szVPCTargetPlatform ) = 0;
	virtual bool DeploysForVPCTargetPlatform( const char *szVPCTargetPlatform ) = 0;
	virtual CUtlString GetSolutionPlatformAlias( const char *szVPCTargetPlatform, IBaseSolutionGenerator *pSolutionGenerator ) = 0;

protected:
	CUtlString		m_OutputFileName;
	CUtlString		m_GUIDString;
};
