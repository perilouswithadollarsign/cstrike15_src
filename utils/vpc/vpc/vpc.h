//========= Copyright � 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#pragma once

#if defined( WIN32 )

#include "tier0/wchartypes.h"

#include "winlite.h"
#include <io.h>

// Allow atlbase.h to access 'CreateEvent' in the windows API (see windows_undefines.h)
#undef  CreateEventA
#include <atlbase.h>
#define CreateEventA  CreateEvent

#endif // WIN32

#include "tier1/utlstring.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlvector.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstack.h"
#include "tier1/utldict.h"
#include "tier1/utlsortvector.h"
#include "tier1/checksum_crc.h"
#include "tier1/checksum_md5.h"
#include "tier1/fmtstr.h"
#include "tier1/exprevaluator.h"
#include "tier1/interface.h"
#include "p4lib/ip4.h"
#include "scriptsource.h"
#include "logging.h"
#ifdef STEAM
#include "vstdlib/strtools.h"
#else
#include "tier1/strtools.h"
#endif
#include "sys_utils.h"
#include "tier1/keyvalues.h"
#include "generatordefinition.h"
#include "environment_utils.h"
#include "tier1/UtlStringMap.h"

DECLARE_LOGGING_CHANNEL( LOG_VPC );

// We try to avoid any kind of fixed path buffers
// but we still have certain calls that fill
// in fixed buffers.  Make local defines so that
// we can control things separately from MAX_PATH.
#define MAX_FIXED_PATH 2000
// Maximum size of a bare filename with no path.
#define MAX_BASE_FILENAME 260

//#define DISALLOW_UNITY_FILE_EXCLUSION 1

enum VpcFileFlags_t
{
	VPC_FILE_FLAGS_NONE			= 0,
	VPC_FILE_FLAGS_SCHEMA		= 1<<0,
	VPC_FILE_FLAGS_DYNAMIC		= 1<<1,
	VPC_FILE_FLAGS_QT			= 1<<2,
    VPC_FILE_FLAGS_CREATE_PCH   = 1<<3,
    VPC_FILE_FLAGS_NO_PCH       = 1<<4,
	VPC_FILE_FLAGS_SCHEMA_INCLUDE	= 1<<5,
	VPC_FILE_FLAGS_STATIC_LIB		= 1<<6,
	VPC_FILE_FLAGS_IMPORT_LIB		= 1<<7,
	VPC_FILE_FLAGS_SHARED_LIB		= 1<<8,
};

enum VpcFolderFlags_t
{
	VPC_FOLDER_FLAGS_NONE		= 0,
	VPC_FOLDER_FLAGS_DYNAMIC	= 1<<0,
	VPC_FOLDER_FLAGS_UNITY		= 1<<1
};

struct KeywordName_t
{
	const char			*m_pName;
	configKeyword_e		m_Keyword;
};

typedef bool (*procptr_t)( const char *pPropertyName );
typedef bool (*GetSymbolProc_t)( const char *pKey );

#define INVALID_INDEX -1

struct property_t
{
	const char	*pName;
	procptr_t	handler;
	int			platformMask;
};

enum conditionalType_e
{
	CONDITIONAL_NULL,
	CONDITIONAL_PLATFORM,	// reserved for each known platform
	CONDITIONAL_GAME,		// reserved for each known game
	CONDITIONAL_SYSTEM,		// reserved for system features that permute global state that cannot be altered per script, not changeable by user scripts.
	CONDITIONAL_CUSTOM,		// created by user via command line, used for private or local testing
	CONDITIONAL_SCRIPT,		// created by scripts
};

#define k_bVPCForceLowerCase false

inline bool CharStrEq( const char *pStr, char ch )
{
    return pStr[0] == ch && pStr[1] == 0;
}

struct conditional_t
{
	conditional_t()
	{
		m_Type = CONDITIONAL_NULL;
		m_bDefined = false;
		m_bGameConditionActive = false;
	}

	conditional_t( const conditional_t &other )
	{
		m_Name = other.m_Name;
		m_UpperCaseName = other.m_UpperCaseName;
		m_Type = other.m_Type;
		m_bDefined = other.m_bDefined;
		m_bGameConditionActive = other.m_bGameConditionActive;
	}

	CUtlString			m_Name;
	CUtlString			m_UpperCaseName;
	conditionalType_e	m_Type;

	// a conditional can be present in the table but not defined
	// e.g. default conditionals that get set by command line args
	bool				m_bDefined;

	// only used during multiple game iterations for game conditionals as each 'defined' game becomes active
	bool				m_bGameConditionActive;
};

#define MAX_MACRO_NAME 200

struct CMacro
{
public:
	CMacro( const char *pMacroName, const char *pMacroValue, const char *pConfigurationName, bool bSystemMacro, bool bSetupDefine );
	CMacro( const char *pMacroName, void (*pFNResolveValue)( CMacro * ) );

	bool IsSystemMacro() { return m_bSystemMacro; }
	bool IsPropertyMacro() { return !m_ConfigurationName.IsEmpty(); }
	bool ShouldDefineInProjectFile()	{ return m_bSetupDefineInProjectFile; }

    // GetName returns the name without a leading $.
	const char *GetName() { return m_FullName.Get() + 1; }
    const char *GetFullName() { return m_FullName.Get(); }
    int GetNameLength() { return m_nBaseNameLength; }
    int GetFullNameLength() { return m_nBaseNameLength + 1; }
	const char *GetValue() { ResolveValue(); return m_Value; }
    int GetValueLength() { ResolveValue(); return m_Value.Length(); }
    bool HasValue() { ResolveValue(); return !m_Value.IsEmpty(); }
	const char *GetConfigurationName() { return m_ConfigurationName; }
    bool HasConfigurationName() { return !m_ConfigurationName.IsEmpty(); }
	void SetValue( const char *pMacroValue ) { Assert( !m_pFNResolveDynamicMacro); m_Value = pMacroValue; }
	void SetResolveFunc( void (*pFNResolveValue)( CMacro *pThis ) ) { m_pFNResolveDynamicMacro = pFNResolveValue; }

private:
	void SetMacroName( const char *pMacroName );
	void ResolveValue( void ) { if ( m_pFNResolveDynamicMacro ) m_pFNResolveDynamicMacro( this ); }

	// m_FullName has the case-preserved macro name with a leading $.
	CUtlString	m_FullName;

protected:
	CUtlString	m_Value;

	// when set denotes the configuration this macro belongs to
	CUtlString	m_ConfigurationName;

    int			m_nBaseNameLength;
    
	// If set to true, then VPC will add this as a -Dname=value parameter to the compiler's command line.
	bool		m_bSetupDefineInProjectFile;
	
	// VPC created this macro itself rather than the macro being created from a script file.
	// System macros are Read-Only to scripts.
	bool		m_bSystemMacro;

	//when set, the macro calls the resolve function whenever HasValue() or GetValue() are called
	void (*m_pFNResolveDynamicMacro)( CMacro *pThis );
};

typedef int scriptIndex_t;
struct script_t
{
	CUtlString		name;
	CUtlString		m_condition;
};

typedef int	projectIndex_t;
struct project_t
{
	CUtlString				name;
	CUtlVector< script_t >	scripts;
};

typedef int groupIndex_t;
struct group_t
{
	CUtlVector< projectIndex_t >	projects;
};

typedef int groupTagIndex_t;
struct groupTag_t
{
	groupTag_t()
	{
		bSameAsProject = false;
	}

	CUtlString					name;
	CUtlVector< groupIndex_t >	groups;

	// this tag is an implicit definition of the project
	bool						bSameAsProject;
};

struct scriptList_t
{
	scriptList_t()
	{
		m_crc = 0;
	}

	CUtlString	m_scriptName;
	CRC32_t		m_crc;
	bool		m_bCRCCheck;
};

class IProjectIterator
{
public:
	// the base IProjectIterator::VisitProject() implementation performs the CRC check,
	// so derived classes should call it if they want to skip up-to-date projects
	// NOTE: iProject/projectIndex_t indexes CVPC::m_Projects/project_t
	virtual bool VisitProject( projectIndex_t iProject, const char *szScriptPath );

	// the base IProjectIterator::VisitProject() operates quietly, it's status is stored here
	CUtlString m_CRCCheckStatusSpew;
};

#include "ibasesolutiongenerator.h"
#include "ibaseprojectgenerator.h"
#include "baseprojectdatacollector.h"
#include "projectgenerator_vcproj.h"
#include "projectgenerator_win32.h"
#include "projectgenerator_win32_2010.h"
#include "projectgenerator_xbox360.h"
#include "projectgenerator_xbox360_2010.h"
#include "projectgenerator_ps3.h"
#include "projectgenerator_makefile.h"

class CUtlStringCaseLess
{
public:
	bool Less( const CUtlString &lhs, const CUtlString &rhs, void *pCtx )
	{
		return ( V_stricmp_fast( lhs.Get(), rhs.Get() ) < 0 ? true : false );
	}
};

class CDefCaselessCUtlStringLess
{
public:
	CDefCaselessCUtlStringLess() {}
	CDefCaselessCUtlStringLess( int i ) {}
	inline bool operator()( const CUtlString &lhs, const CUtlString &rhs ) const { return ( V_stricmp_fast( lhs.String(), rhs.String() ) < 0 ); }
	inline bool operator!() const { return false; }
};

class CVPC
{
public:
	CVPC();
	~CVPC();

	bool		Init( int argc, char **argv );
	void		Shutdown( bool bHasError = false );

	void		VPCError( PRINTF_FORMAT_STRING const char *pFormat, ... ) FMTFUNCTION( 2, 3 );
	void		VPCWarning( PRINTF_FORMAT_STRING const char *pFormat, ... ) FMTFUNCTION( 2, 3 );
	void		VPCStatus( bool bAlwaysSpew, PRINTF_FORMAT_STRING const char *pFormat, ... ) FMTFUNCTION( 3, 4 );
	void		VPCStatusWithColor( bool bAlwaysSpew, Color messageColor, PRINTF_FORMAT_STRING const char *pFormat, ... ) FMTFUNCTION( 4, 5 );
	void		VPCSyntaxError( PRINTF_FORMAT_STRING const char *pFormat = NULL, ... ) FMTFUNCTION( 2, 3 );

	void		ClearPacifier();
	void		OutputPacifier();
	void		BreakPacifier();

	void		LoadVPCCache( const char *szScriptFileName, KeyValues &intoKV );
	void		SaveVPCCache( const char *szScriptFileName, KeyValues &cacheKV );

	bool		IsProjectCurrent( const char *szScriptFileName, CUtlString &statusString );
	void		UpdateCacheFile( const char *szScriptFileName );

	bool		HasCommandLineParameter( const char *pParamName );
	bool		HasP4SLNCommand();

	CScript		&GetScript()			{ return m_Script; }

	bool		IsVerbose()						{ return m_bVerbose; }
	bool		IsQuiet()						{ return m_bQuiet; }
	bool		IsQuietValidSpew()				{ return m_bQuietValidSpew; }
	void		SetQuietValidSpew( bool bQuiet ){ m_bQuietValidSpew = bQuiet; }
	bool		IsShowDependencies()			{ return m_bShowDeps; }
	bool		IsForceGenerate()				{ return m_bForceGenerate; }
	bool		IsForceIterate()				{ return m_bForceIterate || IsForceGenerate(); }
	bool		IsCheckFiles()					{ return m_bCheckFiles; }
	
	bool		IsShowFixedPaths()				{ return m_bShowFixedPaths; }
	bool		IsShowCaseIssues()				{ return m_bShowCaseIssues; }

	bool		IsSourceControlEnabled()		{ return m_bSourceControl; }
	bool		IsOSMacroEnabled()				{ return m_bAllowOSMacro; }
	bool		IsCRCCheckInProjectEnabled()	{ return m_bCRCCheckInProject; }
	bool		IsMissingFileAsErrorEnabled()	{ return m_bMissingFileIsError; }
	bool		IsFilePatternEnabled()			{ return m_bAllowFilePattern; }
	bool		AddExecuteableToCRCChecks()		{ return m_bAddExecuteableToCRC; }
	bool		IsP4AutoAddEnabled()			{ return m_bP4AutoAdd; }
    bool        IsPerFileCompileConfigEnabled() { return m_bPerFileCompileConfig; }
    bool		IsLibWithinLibEnabled()			{ return m_bAllowLibWithinLib; }
    
	bool		Is2005()						{ return !m_bUse2010 && !m_bUse2012 && !m_bUse2013 && !m_bUse2015; }
	// Note that internally to VPC this returns true when Is2012() or Is2013() or Is2015() return true because they use
	// the VS 2010 file format.
	bool		Is2010()						{ return m_bUse2010; }
	bool		Is2012()						{ return m_bUse2012; }
	bool		Is2013()						{ return m_bUse2013; }
	bool		Is2015()						{ return m_bUse2015; }

	bool		PrefersVS2010()					{ return m_bPreferVS2010; }
	bool		PrefersVS2012()					{ return m_bPreferVS2012; }
	bool		PrefersVS2013()					{ return m_bPreferVS2013; }
	bool		PrefersVS2015()					{ return m_bPreferVS2015; }
  
	bool		IsForceRebuildCache()			{ return m_bForceRebuildCache; }
	bool		IsDedicatedBuild()				{ return m_bDedicatedBuild; }
	bool		UseValveBinDir()				{ return m_bUseValveBinDir; }

	bool		IsQtEnabled();
	bool		IsSchemaEnabled();
	bool		IsUnityEnabled();
	bool		IsProjectUsingUnity( script_t *pProjectScript = NULL );
	bool		IsClangEnabled();
	bool		ShouldEmitClangProject();

	bool		RestrictProjectsToEverything()	{ return m_bRestrictProjects; }

	int			GetMissingFilesCount() const	{ return m_nFilesMissing; }
	int			GetTotalMissingFilesCount() const	{ return m_nTotalFilesMissing; }
	void		IncrementFileMissing()	{ ++m_nFilesMissing; ++m_nTotalFilesMissing; }
	void		ResetMissingFilesCount() { m_nFilesMissing = 0; }

	bool		IsIgnoreRedundancyWarning()					{ return m_bIgnoreRedundancyWarning; }
	void		SetIgnoreRedundancyWarning( bool bSet )		{ m_bIgnoreRedundancyWarning = bSet; }

	bool UsingShallowDependencies( void ) { return m_bShallowDepencies; }

	const char *GetStartDirectory()			{ return m_StartDirectory.Get(); }
	const char *GetSourcePath()				{ return m_SourcePath.Get(); }
	
	const char *GetCRCString()				{ return m_SupplementalCRCString.Get(); }
	const char *GetVSAddInMetadataString()	{ return m_VSAddinMetadata.Get(); }
	const char *GetSolutionItemsFilename()	{ return m_SolutionItemsFilename.Get(); }
	const CUtlVector< CUtlString > &GetSolutionFolderNames() { return m_SolutionFolderNames; }

	void DecorateProjectName( CUtlString &undecoratedName );
	void UndecorateProjectName( CUtlString &decoratedName );

	const char *GetProjectName()							{ return m_ProjectName.Get(); }
	void SetProjectName( const char *pProjectName )			{ m_ProjectName = pProjectName; }

	const char *GetLoadAddressName()						{ return m_LoadAddressName.Get(); }
	void SetLoadAddressName( const char *pLoadAddressName )	{ m_LoadAddressName = pLoadAddressName; }

	const char *GetGameName()								{ return m_GameName.Get(); }
	void SetGameName( const char *pGameName )				{ m_GameName = pGameName; }

	const char *GetOutputMirrorPath()						{ return m_OutputMirrorString.Get(); }

	const char *GetProjectPath()							{ return m_ProjectPath.Get(); }
	void SetProjectPath( const char *pProjectPath )			{ m_ProjectPath = pProjectPath; }

    const char *GetSourceFileConfigFilter()                 { return m_sourceFileConfigFilter.Get(); }
    bool IsConfigAllowedBySourceFileConfigFilter( const char *pConfigName )
    {
        return m_sourceFileConfigFilter.IsEmpty() || !V_stricmp_fast( pConfigName, m_sourceFileConfigFilter );
    }
    
    CUtlStringBuilder* GetTempStringBuffer1()               { return &m_TempStringBuffer1; }
    CUtlStringBuilder* GetTempStringBuffer2()               { return &m_TempStringBuffer2; }
    CUtlStringBuilder* GetMacroReplaceBuffer()              { return &m_MacroReplaceBuffer; }
    CUtlStringBuilder* GetPropertyValueBuffer()             { return &m_PropertyValueBuffer; }
    
    const char *FormatTemp1( const char *pFormat, ... )
    {
        va_list args;
        va_start( args, pFormat );
        m_TempStringBuffer1.VFormat( pFormat, args );
        va_end( args );
        return m_TempStringBuffer1.Get();
    }

    const char *CreateGeneratedRootFilePath( CUtlPathStringHolder *pBuf, const char *pFile, const char *pSuffix = NULL );
    const char *CreateGeneratedSubdirPath( CUtlPathStringHolder *pBuf, const char *pTopLevelName );
    
	int			ProcessCommandLine();

	// Returns the mask identifying what platforms should be built
	bool					IsPlatformDefined( const char *pName );
	bool					IsPlatformName( const char *pName );
	const char				*GetTargetPlatformName();
	const char				*GetTargetCompilerName();

	IBaseProjectGenerator	*GetProjectGenerator()										{ return m_pProjectGenerator; }
	void					SetProjectGenerator( IBaseProjectGenerator *pGenerator )	{ m_pProjectGenerator = pGenerator; }

	IBaseSolutionGenerator	*GetSolutionGenerator()	{ return m_pSolutionGenerator; }

	// Conditionals
	conditional_t			*FindOrCreateConditional( const char *pName, bool bCreate, conditionalType_e type );
	bool					ResolveConditionalSymbol( const char *pSymbol );
	bool					EvaluateConditionalExpression( const char *pExpression );
	bool					ConditionHasDefinedType( const char* pCondition, conditionalType_e type );
	void					SetConditional( const char *pName, bool bSet, conditionalType_e type );
	bool					IsConditionalDefined( const char *pName );

	// Macros
	void					ResolveMacrosInString( char const *pString, CUtlStringBuilder *pOutBuff, CUtlVector< CUtlString > *pMacrosReplaced = NULL );
	int						GetMacrosMarkedForCompilerDefines( CUtlVector< CMacro* > &macroDefines );
	void					RemoveScriptCreatedMacros();
	const char				*GetMacroValue( const char *pMacroName, const char *pConfigurationName = NULL );
	CMacro					*FindMacro( const char *pMacroName, const char *pConfigurationName = NULL );
	CMacro					*SetSystemMacro( const char *pMacroName, const char *pMacroValue, bool bSetupDefineInProjectFile = false );
	CMacro					*SetDynamicMacro( const char *pMacroName, void (*pFNResolveValue)( CMacro *pThis ) );
	CMacro					*SetScriptMacro( const char *pMacroName, const char *pMacroValue, bool bSetupDefineInProjectFile = false );
	CMacro					*SetPropertyMacro( const char *pMacroName, const char *pMacroValue, const char *pConfigurationName );

	// Iterates all the projects in the specified list, checks their conditionals, and calls pIterator->VisitProject for
	// each one that passes the conditional tests.
	//
	// If bForce is false, then it does a CRC check before visiting any project to see if the target project file is
	// already up-to-date with its .vpc file.
	void					IterateTargetProjects( CUtlVector<projectIndex_t> &projectList, IProjectIterator *pIterator );

	bool					ParseProjectScript( const char *pScriptName, int depth, bool bQuiet, bool bWriteCRCCheckFile, CDependency_Project *pDependencyProject = nullptr );

	void					AddScriptToParsedList( const char *pScriptName, bool bAddToCRCCheck, CRC32_t crc = 0 );

	const char				*KeywordToName( configKeyword_e keyword );
	configKeyword_e			NameToKeyword( const char *pKeywordName );

	void 					SetupAllGames( bool bSet );

	int						GetProjectsInGroup( CUtlVector< projectIndex_t > &projectList, const char *pGroupHame );

	void					CreateVSAddinMetadataString( void );

	void					DetermineProjectGenerator();

	bool					IsTestMode( void ) const { return m_bTestMode; }
	bool					OutputName_ShouldAppendSrvToDedicated( void ) const { return m_bAppendSrvToDedicated; }
	bool					OutputName_ShouldAddUnitySuffix( void ) const { return m_bAddUnitySuffix; }
	const CUtlString &		OutputName_ProjectSuffixString( void ) const { return m_ProjectSuffixString; }

	const project_t *		GetProjectFromIndex( projectIndex_t nIndex ) const { return &m_Projects[nIndex]; }

	bool					BuildDependencyProjects( CUtlVector< CDependency_Project *> &projects );

private:
	void					SpewUsage( void );

	bool					LoadPerforceInterface();
	void					UnloadPerforceInterface();

	void					InProcessCRCCheck();
	void					CheckForInstalledXDK();

	void					DetermineSourcePath();
	void					SetDefaultSourcePath();

	void					DetermineSolutionGenerator();
	void					SetupDefaultConditionals();
	void					SetMacrosAndConditionals();

	void					SetVerbosityFromCommandLineArgs();
	void					HandleSingleCommandLineArg( const char *pArg );
	void					ParseBuildOptions( int argc, const char **argv );

	bool					CheckBinPath( char *pOutBinPath, int outBinPathSize );
	bool					RestartFromCorrectLocation( bool *pIsChild );

	void					GenerateOptionsCRCString();
	void					FindProjectFromVCPROJ( const char *pScriptNameVCProj, int nMainArgc, const char **pMainArgv );
	const char				*BuildTempGroupScript( const char *pScriptName );

	bool					HandleP4SLN( IBaseSolutionGenerator *pSolutionGenerator );
	void					HandleMKSLN( IBaseSolutionGenerator *pSolutionGenerator,
                                         IBaseSolutionGenerator *pSolutionGenerator2,
                                         CProjectDependencyGraph &dependencyGraph );

	void					GenerateBuildSet( CProjectDependencyGraph &dependencyGraph );
	bool					BuildTargetProjects();
	bool					BuildTargetProject( IProjectIterator *pIterator, projectIndex_t projectIndex, script_t *pProjectScript, const char *pGameName );

	void					SaveConditionals();
	void					RestoreConditionals();
	CUtlString				GetCRCStringFromConditionals();

	bool					m_bVerbose;
	bool					m_bQuiet;
	bool					m_bQuietValidSpew;
	bool					m_bUsageOnly;
	bool					m_bHelp;
	bool					m_bSpewPlatforms;
	bool					m_bSpewGames;
	bool					m_bSpewGroups;
	bool					m_bSpewProjects;
	bool					m_bIgnoreRedundancyWarning;
	bool					m_bSpewProperties;
	bool					m_bTestMode;
	bool					m_bForceGenerate;
	bool					m_bForceIterate;
	bool					m_bEnableVpcGameMacro;
	bool    				m_bCheckFiles;
	bool					m_bDecorateProject;
	bool					m_bShowDeps;
	bool					m_bP4AutoAdd;
	bool					m_bDedicatedBuild;
	bool					m_bAppendSrvToDedicated;	// concat "_srv" to dedicated server .so's.
	bool					m_bUseValveBinDir;			// On Linux, use gcc toolchain from /valve/bin/
	bool					m_bAnyProjectQualified;
	bool					m_bUse2010;
	bool					m_bPreferVS2010;
	bool					m_bUse2012;
	bool					m_bPreferVS2012;
	bool					m_bUse2013;
	bool					m_bPreferVS2013;
	bool					m_bUse2015;
	bool					m_bPreferVS2015;
	bool					m_bSourceControl;
	bool					m_bAllowOSMacro;
	bool					m_bCRCCheckInProject;
	bool					m_bMissingFileIsError; 
	bool					m_bAllowFilePattern;
	bool					m_bAddExecuteableToCRC;
	bool					m_bRestrictProjects;
	bool					m_bForceRebuildCache;
	bool    				m_bShowFixedPaths;
	bool					m_bShowCaseIssues;
    bool                    m_bGenMakeProj;
    bool                    m_bPerFileCompileConfig;
    bool					m_bAllowLibWithinLib;

	bool					m_bAllowQt;
	bool					m_bAllowSchema;
	bool					m_bAllowUnity;
	bool					m_bAllowClang;
	bool					m_bEmitClangProject;

	// when set, .libs are not treated as dependencies, causing only direct source code dependencies to count
	bool					m_bShallowDepencies;

	// How many of the files listed in the VPC files are missing?
	int						m_nFilesMissing;
	int						m_nTotalFilesMissing;

	int						m_nArgc;
	const char				**m_ppArgv;

	CColorizedLoggingListener	m_LoggingListener;

	PlatModule_t			m_pP4Module;
	PlatModule_t			m_pFilesystemModule;

	CScript					m_Script;

	// Path where vpc was started from
	CUtlString				m_StartDirectory;

	// Root path to the sources (i.e. the directory where the vpc_scripts directory can be found in).
	CUtlString				m_SourcePath;

	// path to the project being processed (i.e. the directory where this project's .vpc can be found).
	CUtlString				m_ProjectPath;

	// strings derived from command-line commands which is checked alongside project CRCs:
	CUtlString				m_SupplementalCRCString;

	CUtlSortVector< CUtlString, CUtlStringCaseLess >	m_ExtraOptionsForCRC;

	CUtlString				m_VSAddinMetadata;

	CUtlString				m_MKSolutionFilename;

	CUtlString				m_SolutionItemsFilename;	// For /slnitems

	CUtlString				m_P4SolutionFilename;		// For /p4sln
	CUtlVector< int >		m_iP4Changelists;

	CUtlString				m_ProjectName;
	CUtlString				m_LoadAddressName;
	CUtlString				m_GameName;

	CUtlString				m_ProjectSuffixString;
	CUtlString				m_OutputMirrorString;

	CUtlPathStringHolder	m_TempGroupScriptFilename;

    CUtlString              m_sourceFileConfigFilter;
    
	CUtlVector< CUtlString > m_SolutionFolderNames;	// For /slnfolders

	// This abstracts the differences between different output methods.
	IBaseProjectGenerator			*m_pProjectGenerator;
	IBaseSolutionGenerator			*m_pSolutionGenerator;
	IBaseSolutionGenerator			*m_pSolutionGenerator2;

	CUtlVector< CUtlString >		m_BuildCommands;

	CUtlVector< conditional_t* >	m_SavedConditionals;

    CUtlStringBuilder               m_TempStringBuffer1;
    CUtlStringBuilder               m_TempStringBuffer2;
    CUtlStringBuilder               m_MacroReplaceBuffer;
    CUtlStringBuilder               m_PropertyValueBuffer;
    
public:
	CUtlVector< conditional_t* >	m_Conditionals;
	
	// using UtlMap to support duplicates
	CUtlMap< CUtlString, CMacro*, int, CDefCaselessCUtlStringLess >	m_Macros;

	CUtlVector< scriptList_t >		m_ScriptList;

	CUtlVector< project_t >			m_Projects;
	CUtlVector< projectIndex_t >	m_TargetProjects;

	CUtlVector< group_t >			m_Groups;
	CUtlVector< groupTag_t >		m_GroupTags;

	CUtlVector< CUtlString >		m_P4GroupRestrictions;

	// for script extensions
	struct CustomBuildStepForExtension_t
	{
		CUtlString m_BuildSteps;
		CUtlString m_DefinedInFile;
		int m_nDefinitionStartLine;
	};
	CUtlDict< CustomBuildStepForExtension_t, int > 	m_CustomBuildSteps;
	CUtlDict< CUtlString, int > 	m_CustomAutoScripts;

    CUtlDict< CCopyableUtlVector< CUtlString >, int >     m_LibraryDependencies;

    bool                            m_bInProjectSection;
	bool							m_bGeneratedProject;
	bool							m_bIsDependencyPass; // True inside CProjectDependencyGraph::BuildProjectDependencies()
	int								m_nPacifier;

	// Schema stuff
	CUtlVector< CUtlStringCI >		m_SchemaFiles;		// NOTE: case-insensitive comparisons (error-tolerant w.r.t filenames in .VPCs)
	CUtlStringMap< CUtlString >		m_SchemaOutputFileMap;

	// Qt stuff
	CUtlVector< CUtlStringCI >		m_QtFiles;
	CUtlStringMap< CUtlString >		m_QtOutputFileMap;

	// Unity file stuff
	CUtlDict< bool > 				m_UnityFilesSeen;
	CUtlStringMap< CUtlString >		m_UnityOutputFileMap;
	bool							m_bAddUnitySuffix;
	bool							m_bProjectUsesUnity;
	bool							m_bUnitySchemaHeadersOnly;
	// Should writable files (presumed to be checked out) be put in unity files? Needed for buildbot where all files are writable.
	bool							m_bUnityOnWritableFiles;
	bool							m_bDoneOnParseProjectEnd;


	CDependency_Project				*m_pDependencyProject;
};

extern CVPC *g_pVPC;

extern const char			*g_pOption_ImportLibrary;
extern const char			*g_pOption_OutputFile;
extern const char			*g_pOption_AdditionalDependencies;
extern const char			*g_pOption_OrderOnlyFileDependencies;
extern const char			*g_pOption_OrderOnlyProjectDependencies;
extern const char			*g_pOption_AdditionalDependencies_Proj;
extern const char			*g_pOption_AdditionalIncludeDirectories;
extern const char			*g_pOption_AdditionalProjectDependencies;
extern const char			*g_pOption_AdditionalOutputFiles;
extern const char			*g_pOption_PreprocessorDefinitions;
extern const char			*g_pOption_PrecompiledHeader;				
extern const char			*g_pOption_UsePCHThroughFile;				
extern const char			*g_pOption_PrecompiledHeaderFile;			
extern const char			*g_pOption_ForceInclude;					
extern const char			*g_pOption_ExcludedFromBuild;				
extern const char			*g_pOption_CommandLine;
extern const char			*g_pOption_ConfigurationType;
extern const char			*g_pOption_Description;
extern const char			*g_pOption_GCC_ExtraCompilerFlags;
extern const char			*g_pOption_GCC_ExtraCxxCompilerFlags;
extern const char			*g_pOption_GCC_ExtraLinkerFlags;
extern const char			*g_pOption_POSIX_RPaths;
extern const char			*g_pOption_GameOutputFile;
extern const char			*g_pOption_LocalFrameworks;
extern const char			*g_pOption_OptimizerLevel;
extern const char			*g_pOption_Outputs;
extern const char			*g_pOption_PotentialOutputs;
extern const char			*g_pOption_PostBuildEvent;
extern const char			*g_pOption_SymbolVisibility;
extern const char			*g_pOption_SystemFrameworks;
extern const char			*g_pOption_SystemLibraries;
extern const char			*g_pOption_BuildMultiArch;
extern const char           *g_pOption_TreatWarningsAsErrors;
extern const char			*g_pOption_DisableLinkerDeadCodeElimination;

// TODO: unify all generated files into one folder tree under '.\_VPC_' MOC (DONE: unity, clang)
extern const char			*g_VPCGeneratedFolderName;					
extern const char			*g_QtFolderName;							
extern const char			*g_SchemaFolderName;						
extern const char			*g_SchemaAnchorBase;						
extern const char			*g_IncludeSeparators[2];

extern bool					VPC_IsPlatformWindows( const char *pPlatformName );
extern bool					VPC_IsPlatformLinux( const char *pPlatformName );
extern bool					VPC_IsPlatformOSX( const char *pPlatformName );
extern bool					VPC_IsPlatformAndroid( const char *pPlatformName );
extern bool					VPC_IsPlatform32Bits( const char *pPlatformName );
extern bool					VPC_IsPlatform64Bits( const char *pPlatformName );

extern void					VPC_ParseGroupScript( const char *pScriptName );
extern void					VPC_ParseProjectScriptParameters( const char *szScriptName, int depth, bool bQuiet );
extern void					VPC_HandleProjectCommands( const char *pUnusedScriptName, int depth, bool bQuiet );
extern void					VPC_GenerateProjectDependencies( CBaseProjectDataCollector *pDataCollector );
extern bool					VPC_AreProjectDependenciesSupportedForThisTargetPlatform( void );

extern groupTagIndex_t		VPC_Group_FindOrCreateGroupTag( const char *pName, bool bCreate );
extern projectIndex_t		VPC_Group_FindOrCreateProject( const char *pName, bool bCreate );

extern void					VPC_Keyword_Folder( VpcFolderFlags_t iFolderFlags = VPC_FOLDER_FLAGS_NONE );
extern void					VPC_Keyword_Configuration();
extern void					VPC_Keyword_FileConfiguration();

extern void					VPC_Config_SpewProperties( configKeyword_e keyword );
extern bool					VPC_Config_IgnoreOption( const char *pPropertyName );


// Convenience helpers to extract properties from project/file configs:
//  - if the pFileConfig has the property, it trumps the pRootConfig
//  - at least one of pRootConfig and pFileConfig must be non-NULL
extern PropertyState_t *	VPC_GetProperty(       configKeyword_e tool, CProjectConfiguration *pRootConfig, CProjectConfiguration *pFileConfig, const char *pPropertyName );
extern bool					VPC_GetPropertyBool(   configKeyword_e tool, CProjectConfiguration *pRootConfig, CProjectConfiguration *pFileConfig, const char *pPropertyName, bool *pResult );
extern bool					VPC_GetPropertyString( configKeyword_e tool, CProjectConfiguration *pRootConfig, CProjectConfiguration *pFileConfig, const char *pPropertyName, CUtlString *pResult );

//resolves common MSVC properties found in a string $(IntDir), $(TargetFile), ...
// returns the input string if no replacements are made, returns outputScratchSpace.Get() if replacements are performed.
extern const char *			VPC_ResolveCompilerMacrosInString( const char *szSourceString, CUtlString &outputScratchSpace, CProjectConfiguration *pRootConfig, CProjectConfiguration *pFileConfig );

// These variants extract 'global' properties from the root config(s)
//  - it requires+checks that they match across all root configs
extern bool					VPC_GetGlobalPropertyString( configKeyword_e tool, CVCProjGenerator *pDataCollector, const char *pPropertyName, CUtlString *pResult );

extern void					VPC_SetProperty_ForFile(	CProjectFile *pFile, const char *pConfigName, configKeyword_e tool,
														const char *pPropertyName, const char *pPropertyValue, CVCProjGenerator *pDataCollector );
// Get a list of preprocessor defines or include directories
// [NOTE: quotes are stripped before returning - the caller may need to add quotes depending on usage]
extern void					VPC_GetPreprocessorDefines( CProjectFile *pFile, CProjectConfiguration *pRootConfig, CUtlVector< CUtlString > &defines );
extern void					VPC_GetIncludeDirectories(  CProjectFile *pFile, CProjectConfiguration *pRootConfig, CUtlVector< CUtlString > &includes );


// ---------------- Qt feature --------------------------
extern void					VPC_Qt_OnParseProjectStart( void );
extern void					VPC_Qt_OnParseProjectEnd( class CVCProjGenerator *pDataCollector );
extern void					VPC_Qt_TrackFile( const char *pName, bool bRemove, VpcFileFlags_t iFileFlags );
extern CProjectFile *		VPC_Qt_GetGeneratedFile( CProjectFile *pInputFile, const char *pConfigName, CVCProjGenerator *pDataCollector );
// ------------------------------------------------------


// ---------------- Schema feature ----------------------
extern void					VPC_Schema_OnParseProjectStart( void );
extern void					VPC_Schema_OnParseProjectEnd( CVCProjGenerator *pDataCollector );
extern void					VPC_Schema_TrackFile( const char *pName, bool bRemove, VpcFileFlags_t iFileFlags );
extern void					VPC_Schema_ForceAdditionalDependencies( const char *pProjectName );
extern CProjectFile *		VPC_Schema_GetGeneratedFile( CProjectFile *pInputFile, const char *pConfigName, CVCProjGenerator *pDataCollector );
// ------------------------------------------------------


// ---------------- Unity files feature -----------------
extern void					VPC_Unity_OnParseProjectStart( void );
extern void					VPC_Unity_OnParseProjectEnd( CVCProjGenerator *pDataCollector );
extern bool					VPC_Unity_UpdateUnityFiles( const char **ppArgs, int nArgs );
extern CProjectFile *		VPC_Unity_GetContainingUnityFile( CProjectFile *pInputFile, const char *pConfigName, CVCProjGenerator *pDataCollector );
// ------------------------------------------------------


// Get the included PCH file (returns "" if none)
//  - sets 'bCreatesPCH'  to true if this file *creates* the PCH file
//  - sets 'bExcludesPCH' to true if this file is specifically configured to *not* use a PCH file
extern void					VPC_GetPCHInclude( CProjectFile *pFile, CProjectConfiguration *pRootConfig, CUtlString &pchFile, bool &bCreatesPCH, bool &bExcludesPCH );
// Get info describing all PCH files created by a project
//  - pchIncludeNames:	  returns the list of PCH header files (e.g cbase.h)
//    pchCreatorNames:    returns the list of files used to *create* the PCHs (e.g stdafx.cpp)
//    [NOTE: these two lists are parallel; always the same length, with no empty string entries]
//  - pRequiredPCHs:      (optional) can be used to filter the returned lists down to a subset of required PCHs
//  - pFilesExcludingPCH: (optional) receives a list of all files specifically configured to NOT use PCHs
extern void					VPC_GeneratePCHInfo(	CVCProjGenerator *pDataCollector, CProjectConfiguration *pRootConfig,
													CUtlVector< CUtlString > &pchIncludeNames, CUtlVector< CUtlString > &pchCreatorNames,
													CUtlVector< CUtlString > const *pRequiredPCHs = NULL,
													CUtlVector< CProjectFile * > *pFilesExcludingPCH = NULL );

// ---------------- Clang feature -----------------------
extern void					VPC_Clang_OnParseProjectEnd( CVCProjGenerator *pDataCollector );
// ------------------------------------------------------


// -------------- Build-generated files -----------------
struct CSourceFileInfo
{
	CSourceFileInfo( int folderIndex = -1 );
	CProjectFile *	m_pSourceFile;			// The input source file
	CProjectFile *	m_pDebugCompiledFile;	// The output file, compiled for debug   (may be different to m_pSourceFile for schema/qt/protobuf files)
	CProjectFile *	m_pReleaseCompiledFile;	// The output file, compiled for release (may be different to m_pSourceFile for schema/qt/protobuf files)
	CProjectFile *	m_pContainingUnityFile;	// The containing unity file for the output file(s), if any
	CUtlString		m_ConfigString;			// Constructed from the configuration properties of the output files
	CRC32_t			m_ConfigStringCRC;		// CRC of m_ConfigString, used for sorting in CUnityInputFileInfoLessFunc
	CUtlString		m_PCHName;				// The PCH file (if any) used by the input file
	bool			m_bCreatesPCH;			// Whether this file is used to create a PCH file
	int				m_iFolderIndex;			// Index of the input file in its folder, used as a tie-breaker during sorting
};

// Generates a CSourceFileInfo for a given source file
//  - return false on error, or for non-source files (dynamic files or non-compiled files)
extern bool					VPC_GeneratedFiles_GetSourceFileInfo(	CSourceFileInfo &result, CProjectFile *pFile, bool bGenerateConfigString,
																	CVCProjGenerator *pDataCollector, const CUtlVector< CProjectConfiguration * > &rootConfigs );
extern void					VPC_GeneratedFiles_OnParseProjectEnd(	CVCProjGenerator *pDataCollector );
// ------------------------------------------------------
