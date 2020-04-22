//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PROJECTGENERATOR_WIN32_2010_H
#define PROJECTGENERATOR_WIN32_2010_H
#ifdef _WIN32
#pragma once
#endif

#define PROPERTYNAME( X, Y ) X##_##Y,
enum Win32_2010_Properties_e
{
	#include "projectgenerator_win32_2010.inc"
};

class CProjectGenerator_Win32_2010 : public IVCProjWriter
{
public:
	CProjectGenerator_Win32_2010();
	virtual CVCProjGenerator *GetProjectGenerator() OVERRIDE { return m_pVCProjGenerator; }
	
	virtual bool Save( const char *pOutputFilename );
	virtual const char *GetProjectFileExtension() { return "vcxproj"; }

private:
	// primary XML - foo.vcxproj
	bool		WritePrimaryXML( const char *pOutputFilename, const char *szPlatformName );
	bool		WriteFolder( CProjectFolder *pFolder, const char *pFileTypeName, int nDepth, const char *szPlatformName );
	bool		WriteFile( CProjectFile *pFile, const char *pFileTypeName, const char *szPlatformName );
	bool		WriteConfiguration( CProjectConfiguration *pConfig, const char *szPlatformName );
	bool		WriteTools( CProjectConfiguration *pConfig, const char *szPlatformName );
	bool		WriteProperty( const PropertyState_t *pPropertyState, const char *szPlatformName, bool bEmitConfiguration = false, const char *pConfigurationName = NULL, const char *pOutputName = NULL, const char *pValue = NULL );
	bool		WriteTool( const char *pToolName, const CProjectTool *pProjectTool, CProjectConfiguration *pConfig, const char *szPlatformName );
	bool		WriteNULLTool( const char *pToolName, const CProjectConfiguration *pConfig );
	bool		WritePropertyGroupTool( CProjectTool *pProjectTool, CProjectConfiguration *pConfiguration, const char *szPlatformName );
	bool		WritePropertyGroup();

	// secondary XML - foo.vcxproj.filters
	bool		WriteSecondaryXML( const char *pOutputFilename );
	bool		WriteFolderToSecondaryXML( CProjectFolder *pFolder, const char *pParentPath );
	bool		WriteFolderContentsToSecondaryXML( CProjectFolder *pFolder, const char *pParentPath, const char *pFileTypeName, int nDepth );
	bool		WriteFileToSecondaryXML( CProjectFile *pFile, const char *pParentPath, const char *pFileTypeName );

    // VisualGDB support files
    bool        WriteVisualGDBSettings( const char *pConfiguration );

	const char	*GetKeyNameForFile( CProjectFile *pFile );

	bool		GenerateToolProperty( const char *pOutputName, const char *pScriptValue, CUtlString &outputWrite, const PropertyState_t *pPropertyState, const char *szPlatformName, const char *pConfigName );

	CXMLWriter			m_XMLWriter;
	CXMLWriter			m_XMLFilterWriter;

	CVCProjGenerator	*m_pVCProjGenerator;

	bool		m_bGenerateMakefileVCXProj;
	bool		m_bVisualGDB;
};

#endif // PROJECTGENERATOR_WIN32_2010_H
