//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//==================================================================================================

#ifndef PROJECTGENERATOR_MAKEFILE_H
#define PROJECTGENERATOR_MAKEFILE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlsymbollarge.h"

class CProjectGenerator_Makefile : public IVCProjWriter
{
public:
	CProjectGenerator_Makefile();
	virtual CVCProjGenerator *GetProjectGenerator() OVERRIDE { return m_pVCProjGenerator; }
	
	virtual bool Save( const char *pOutputFilename );
	virtual const char *GetProjectFileExtension() { return "mak"; }

private:
	enum ScriptType_t
	{
		ST_SH,
		ST_BAT,

		ST_COUNT,
	};

	struct ScriptHelpers_t
	{
		const char *szExtension;
		const char *szContentsPreamble;
		void (*pFN_ScriptConversion)( CUtlStringBuilder * );
	};

	static ScriptHelpers_t s_ScriptHelpers[ST_COUNT];

	typedef CUtlSymbolTableLargeBase< CNonThreadsafeTree< false >, false, 16000 > DependencyTable_t;
	void WriteSourceFilesList( CUtlBuffer &outBuf, const char *pListName, const char **pExtensions, const char *pConfigName );
	void WriteNonConfigSpecificStuff( CUtlBuffer &outBuf );
	void CollectDependencies( const char *pDependencies, DependencyTable_t &dependenciesOut, const char *pFullFileNameForVisualStudioReplacements = nullptr );
    void WriteCustomDependencies( const char *pDependencies, CUtlBuffer &outBuf, DependencyTable_t *pDependenciesOut );
    bool WriteCustomBuildTool(	CProjectConfiguration *pConfig,
								CUtlBuffer &outBuf,
								ScriptType_t scriptType,
								CProjectFile *pProjectFile,
								const char *pFixedProjectFileName,
								const char *szVPCGeneratedScriptsBasePath,
								DependencyTable_t &dependenciesOut,
								DependencyTable_t &generatedScriptsOut,
								DependencyTable_t &outputsOut );
    void WriteDefines( CProjectConfiguration *pConfig,
                       CUtlBuffer &outBuf,
                       CProjectFile *pProjectFile );
    void WriteIncludes( CProjectConfiguration *pConfig,
                        CUtlBuffer &outBuf,
                        CProjectFile *pProjectFile );
	void WriteCompileRule( CUtlBuffer &outBuf, CProjectFile *pProjectFile, CProjectConfiguration *pConfig,
	                       const char *pCompileFunction, const char *pTarget, const char *pSource, const char *pAdditionalDeps, const char *pOrderOnlyDeps,
	                       const char *pPchInclude );
	void WriteConfigSpecificStuff( CProjectConfiguration *pConfig, CUtlBuffer &outBuf, ScriptType_t scriptType );
    void WriteDependencies( const char *szVarName, DependencyTable_t &otherDependencies, CUtlBuffer &outBuf );
	bool WriteMakefile( const char *pFilename );

	//returns true if the file changed
	bool WriteScriptFile( const char *pFileName, CUtlStringBuilder *pContents, ScriptType_t scriptType );

	void BuildFileSet();

    const char *UsePOSIXSlashes( const char *pStr );
    
	CVCProjGenerator *m_pVCProjGenerator;

	CUtlVector< CProjectFile * >			m_Files;
	CUtlVector< CProjectConfiguration * >	m_RootConfigurations;

	CUtlVector< CUtlString > m_CustomBuildTools;

    CUtlStringBuilder m_TempFixedPath;
};

#endif
