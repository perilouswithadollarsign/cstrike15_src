//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "vpc.h"
#include "dependencies.h"
#include "baseprojectdatacollector.h"
#include "utlsortvector.h"

#ifdef WIN32
#include <direct.h>
#define mkdir(dir, mode) _mkdir(dir)
#define getcwd _getcwd
#define snprintf _snprintf
typedef unsigned __int64 uint64_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
typedef signed __int16 int16_t;
#else
#include <stdint.h>
#endif

#ifndef STEAM
bool Q_StrSubstInPlace( char *pchInOut, int cchInOut, const char *pMatch, const char *pReplaceWith, bool bCaseSensitive )
{
        char *pchT = (char *)stackalloc( cchInOut );
        if ( Q_StrSubst( pchInOut, pMatch, pReplaceWith, pchT, cchInOut, bCaseSensitive ) )
        {
                Q_strncpy( pchInOut, pchT, cchInOut );
                return true;
        }
        return false;
}
#endif

static const char *k_pchProjects = "Projects";
static const char *k_pchLegacyTarget = "build with make";
static const char *k_rgchConfigNames[] = { "Debug", "Release", "Profile", "Retail" };

static const char *g_pOption_GameOutputFile = "$GameOutputFile";
static const char *g_pOption_SystemLibraries = "$SystemLibraries";
static const char *g_pOption_SystemFrameworks = "$SystemFrameworks";
static const char *g_pOption_LocalFrameworks = "$LocalFrameworks";
static const char *g_pOption_CommandLine = "$CommandLine";
static const char *g_pOption_Outputs = "$Outputs";
static const char *g_pOption_ExtraCompilerFlags = "$GCC_ExtraCompilerFlags";
static const char *g_pOption_ExtraLinkerFlags = "$GCC_ExtraLinkerFlags";
// stupid and evil - remove when we can
static const char *g_pOption_ForceIncludes = "$ForceIncludes";

static const char *k_rgchXCConfigFiles[] = { "debug.xcconfig", "release.xcconfig", "base.xcconfig" };

static int k_oidBuildConfigList = 0xc0de;
static int k_rgOidBuildConfigs[] = { 0x1c0de, 0x1c0e0 };

// we assume (and assert) that the order of this vector is the same as the order of the projects vector
extern CUtlVector<CBaseProjectDataCollector*> g_vecPGenerators;  

class CSolutionGenerator_XCode : public IBaseSolutionGenerator
{
public:
	virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects );
private:
    void XcodeFileTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf );
    void XcodeProductTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf );
    void EmitBuildSettings( const char *pszProjectName, const char *pszProjectDir, CUtlDict<CFileConfig *,int> *pDictFiles, KeyValues *pKV, bool bIsDebug );
    void WriteFilesFolder( uint64_t oid, const char *pFolderName, const char *pExtensions, const CUtlDict<CFileConfig *,int> &files );

 	void Write( const char *pMsg, ... );
	FILE *m_fp;
	int m_nIndent;

    // deprecated
    void GenerateSolutionFileOld( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects );

};

enum EOIDType
{
    EOIDTypeProject = 0x00001d00,
    EOIDTypeGroup,
    EOIDTypeFileReference,
    EOIDTypeBuildFile,
    EOIDTypeSourcesBuildPhase,
    EOIDTypeFrameworksBuildPhase,
    EOIDTypeCopyFilesBuildPhase,
    EOIDTypeHeadersBuildPhase,
    EOIDTypeShellScriptBuildPhase,
    EOIDTypeNativeTarget,
    EOIDTypeAggregateTarget,
    EOIDTypeTargetDependency,
    EOIDTypeContainerItemProxy,
    EOIDTypeBuildConfiguration,
    EOIDTypeConfigurationList,
};

uint64_t makeoid( const void *obj, EOIDType type, int16_t ordinal = 0 )
{
    return (uint64_t)obj + ((uint64_t)type << 32) + ((uint64_t)(ordinal+1) <<  52);
}

static const char* UsePOSIXSlashes( const char *pStr )
{
    int len = Q_strlen( pStr ) + 2;
    static char str[MAX_PATH*2];
    V_strncpy( str, pStr, len );
    V_FixSlashes( str, '/' );
    return str;
}

static bool NeedsBuildFileEntry( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false; 
    else if ( ! Q_stricmp( pchExtension, "cpp" ) || ! Q_stricmp( pchExtension, "cxx" ) || ! Q_stricmp( pchExtension, "cc" ) || ! Q_stricmp( pchExtension, "c" ) || ! Q_stricmp( pchExtension, "m" ) || ! Q_stricmp( pchExtension, "mm" ) || ! Q_stricmp( pchExtension, "cc" )  )
        return true;
    else if ( ! Q_stricmp( pchExtension, "a" ) || ! Q_stricmp( pchExtension, "dylib" ) )
        return true;
    return false;
}


static bool IsSourceFile( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false; 
    else if ( ! Q_stricmp( pchExtension, "cpp" ) || ! Q_stricmp( pchExtension, "cc" ) || ! Q_stricmp( pchExtension, "cxx" ) || ! Q_stricmp( pchExtension, "c" ) || ! Q_stricmp( pchExtension, "m" ) || ! Q_stricmp( pchExtension, "mm" ) || ! Q_stricmp( pchExtension, "cc" ) ) 
        return true;
    return false;
}


static bool AppearsInSourcesBuildPhase( const char *pszFileName )
{
    return IsSourceFile( pszFileName );
}


static bool IsStaticLibrary( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false; 
    else if ( ! Q_stricmp( pchExtension, "a" ) )
        return true;
    return false;
}

             
static bool IsDynamicLibrary( const char *pszFileName )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        return false; 
    else if ( ! Q_stricmp( pchExtension, "dylib" ) )
        return true;
    return false;
}

             

void ResolveAdditionalProjectDependencies( CDependency_Project *pCurProject, CUtlVector<CDependency_Project*> &projects, CUtlVector<CDependency_Project*> &additionalProjectDependencies )
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
 

void CSolutionGenerator_XCode::WriteFilesFolder( uint64_t oid, const char *pFolderName, const char *pExtensions, const CUtlDict<CFileConfig *,int> &files )
{
	CUtlVector<char*> extensions;
	V_SplitString( pExtensions, ";", extensions );
    
	Write( "%024llX /* %s */ = {\n", oid, pFolderName );
	++m_nIndent;
	Write( "isa = PBXGroup;\n" );
	Write( "children = (\n" );
	++m_nIndent;
	for ( int i=files.First(); i != files.InvalidIndex(); i=files.Next( i ) )
	{
		const char *pFileName = files[i]->GetName();
        
		// Make sure this file's extension is one of the extensions they're asking for.
		bool bValidExt = false;
		const char *pFileExtension = V_GetFileExtension( V_UnqualifiedFileName( pFileName ) );
		if ( pFileExtension )
		{
			for ( int iExt=0; iExt < extensions.Count(); iExt++ )
			{
				const char *pTestExt = extensions[iExt];
                
				if ( pTestExt[0] == '*' && pTestExt[1] == '.' && V_stricmp( pTestExt+2, pFileExtension ) == 0 )
				{
					bValidExt = true;
					break;
				}
			}
		}
        
		if ( bValidExt )
		{
			Write( "%024llX /* %s */,\n", makeoid( files[i]->GetName(), EOIDTypeFileReference ), UsePOSIXSlashes( pFileName ) );
		}
	}
    
	--m_nIndent;
	Write( ");\n" );
	Write( "name = \"%s\";\n", pFolderName );
	Write( "sourceTree = \"<group>\";\n" );
	--m_nIndent;
	Write( "};\n" );
}

             
void CSolutionGenerator_XCode::XcodeFileTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        snprintf( pchOutBuf, cchOutBuf, "compiled.mach-o.executable" );  
    else if ( ! Q_stricmp( pchExtension, "cpp" ) || ! Q_stricmp( pchExtension, "cxx" ) || ! Q_stricmp( pchExtension, "cc" ) || ! Q_stricmp( pchExtension, "h" ) || ! Q_stricmp( pchExtension, "hxx" ) || ! Q_stricmp( pchExtension, "cc" ) )
        snprintf( pchOutBuf, cchOutBuf, "sourcecode.cpp.%s", pchExtension );
    else if ( ! Q_stricmp( pchExtension, "c" ) )
        snprintf( pchOutBuf, cchOutBuf, "sourcecode.cpp.cpp" );
    else if ( ! Q_stricmp( pchExtension, "m" ) || ! Q_stricmp( pchExtension, "mm" ) )
        snprintf( pchOutBuf, cchOutBuf, "sourcecode.objc.%s", pchExtension );
    else if ( ! Q_stricmp( pchExtension, "a" ) )
        snprintf( pchOutBuf, cchOutBuf, "archive.ar" );
    else if ( ! Q_stricmp( pchExtension, "dylib" ) )
    {
        const char *pszLibName = V_UnqualifiedFileName( pszFileName );
        if ( pszLibName[0] == 'l' && pszLibName[1] == 'i' && pszLibName[2] == 'b' )
            snprintf( pchOutBuf, cchOutBuf, "compiled.mach-o.dylib" );
        else
            snprintf( pchOutBuf, cchOutBuf, "compiled.mach-o.bundle" );
    }
    else if ( ! Q_stricmp( pchExtension, "pl" ) )
	snprintf( pchOutBuf, cchOutBuf, "text.script.perl" );
    else
        snprintf( pchOutBuf, cchOutBuf, "text.plain" );
}


void CSolutionGenerator_XCode::XcodeProductTypeFromFileName( const char *pszFileName, char *pchOutBuf, int cchOutBuf )
{
    const char *pchExtension = V_GetFileExtension( V_UnqualifiedFileName( pszFileName ) );
    if ( !pchExtension )
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.tool" );  
    else if ( ! Q_stricmp( pchExtension, "a" ) )
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.library.static" );
    else if ( ! Q_stricmp( pchExtension, "dylib" ) )
    {
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.library.dynamic" );
#if 0
        const char *pszLibName = V_UnqualifiedFileName( pszFileName );
        if ( pszLibName[0] != 'l' || pszLibName[1] != 'i' || pszLibName[2] != 'b' )
            snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.bundle" );     
#endif
    }
    else
        snprintf( pchOutBuf, cchOutBuf, "com.apple.product-type.unknown" );
}


void CSolutionGenerator_XCode::EmitBuildSettings( const char *pszProjectName, const char *pszProjectDir, CUtlDict<CFileConfig *,int> *pDictFiles, KeyValues *pKV, bool bIsDebug )
{
    if ( !pKV )
    {
        Write( "PRODUCT_NAME = \"%s\";\n", pszProjectName );
        return;
    }

    CUtlString sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pKV->GetString( g_pOption_GameOutputFile, "" ) ) );
    if ( !sOutputFile.Length() )
        sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pKV->GetString( g_pOption_OutputFile, "" ) ) );
    if ( sOutputFile.Length() )
    {
        char sFilename[MAX_PATH];
		V_StripExtension( sOutputFile, sFilename, sizeof(sFilename) );
        Write( "PRODUCT_NAME = \"%s\";\n", sFilename );
        
        if ( Q_strlen( pKV->GetString( g_pOption_ExtraLinkerFlags, "" ) ) )
            Write( "OTHER_LDFLAGS = \"%s\";\n", pKV->GetString( g_pOption_ExtraLinkerFlags ) );


		CUtlString sOtherCompilerCFlags = "OTHER_CFLAGS = \"$(OTHER_CFLAGS) ";
		CUtlString sOtherCompilerCPlusFlags = "OTHER_CPLUSPLUSFLAGS = \"$(OTHER_CPLUSPLUSFLAGS) ";
		
		if ( Q_strlen( pKV->GetString( g_pOption_ExtraCompilerFlags, "" ) ) )
        {
			sOtherCompilerCFlags += pKV->GetString( g_pOption_ExtraCompilerFlags );
			sOtherCompilerCPlusFlags += pKV->GetString( g_pOption_ExtraCompilerFlags );
        }

        if ( Q_strlen( pKV->GetString( g_pOption_ForceIncludes, "" ) ) )
        {
			
			CSplitString outStrings( pKV->GetString( g_pOption_ForceIncludes ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
			for ( int i=0; i < outStrings.Count(); i++ )
			{
				if ( V_strlen( outStrings[i] ) > 2 )
				{	
					//char sIncludeDir[ MAX_PATH ];
					char szIncludeLine[ MAX_PATH ];
					/*Q_snprintf( sIncludeDir, sizeof( sIncludeDir ), "%s/%s", pszProjectDir, outStrings[i] );
					V_FixSlashes( sIncludeDir, '/' );
					Q_RemoveDotSlashes( sIncludeDir );
#ifdef STEAM
					Q_StripPrecedingAndTrailingWhitespace( sIncludeDir );
#endif				*/	
					V_snprintf( szIncludeLine, sizeof(szIncludeLine), " -include %s", UsePOSIXSlashes( outStrings[i] ) );
					sOtherCompilerCFlags += szIncludeLine;
					sOtherCompilerCPlusFlags += szIncludeLine;
				}
			}			
        }		
		
		sOtherCompilerCFlags += "\";\n" ;
		sOtherCompilerCPlusFlags += "\";\n" ;
		
		Write( sOtherCompilerCFlags );
		Write( sOtherCompilerCPlusFlags );
        
        if ( IsDynamicLibrary( sOutputFile ) )
        {
            if ( sFilename[0] != 'l' || sFilename[1] != 'i' || sFilename[2] == 'b' )
            {
                //if ( !pKV->GetString( g_pOption_LocalFrameworks, NULL ) )
                //    Write( "OTHER_LDFLAGS = \"-flat_namespace\";\n" );
                // Write( "MACH_O_TYPE = mh_bundle;\n" );
                // Write( "EXECUTABLE_EXTENSION = dylib;\n" );
                // Write( "OTHER_LDFLAGS = \"-flat_namespace -undefined suppress\";\n" );
            }
            Write( "MACH_O_TYPE = mh_dylib;\n" );
            Write( "LD_DYLIB_INSTALL_NAME = \"@loader_path/%s.dylib\";\n", sFilename );
        }
	if ( IsStaticLibrary( sOutputFile ) )
	{
		Write( "DEBUG_INFORMATION_FORMAT = stabs;\n" );
	}
    }
    else
        Write( "PRODUCT_NAME = \"%s\";\n", pszProjectName );

    // add our header search paths
    CSplitString outStrings( pKV->GetString( g_pOption_AdditionalIncludeDirectories ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
    if ( outStrings.Count() )
    {
        char sIncludeDir[MAX_PATH];
        
        // start the iquote list with the project directory
        Q_snprintf( sIncludeDir, sizeof( sIncludeDir ), "%s", pszProjectDir );
        V_FixSlashes( sIncludeDir, '/' );
        Q_RemoveDotSlashes( sIncludeDir );
#ifdef STEAM
        Q_StripPrecedingAndTrailingWhitespace( sIncludeDir );
#endif

        Write( "USER_HEADER_SEARCH_PATHS = (\n" );
        ++m_nIndent;
#ifdef STEAM
        Write( "\"%s\",\n", sIncludeDir );
#endif
        for ( int i=0; i < outStrings.Count(); i++ )
        {
            Q_snprintf( sIncludeDir, sizeof( sIncludeDir ), "%s/%s", pszProjectDir, outStrings[i] );
            V_FixSlashes( sIncludeDir, '/' );
            Q_RemoveDotSlashes( sIncludeDir );
#ifdef STEAM
            Q_StripPrecedingAndTrailingWhitespace( sIncludeDir );
#endif
            Write( "\"%s\",\n", sIncludeDir );
        }
        --m_nIndent;
        Write( ");\n" );
    }

    
    // add local frameworks we link against to the compiler framework search paths
    CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
    if ( localFrameworks.Count() )
    {
        Write( "FRAMEWORK_SEARCH_PATHS = (\n" );
        ++m_nIndent;
        {
            Write( "\"$(inherited)\",\n" );
            for ( int i=0; i < localFrameworks.Count(); i++ )
            {
                char rgchFrameworkPath[MAX_PATH];
                Q_snprintf( rgchFrameworkPath, sizeof( rgchFrameworkPath ), "%s/%s", pszProjectDir, localFrameworks[i] );
                rgchFrameworkPath[ Q_strlen( rgchFrameworkPath ) - Q_strlen( V_UnqualifiedFileName( localFrameworks[i] ) ) ] = '\0';
                Q_RemoveDotSlashes( rgchFrameworkPath );
                
                Write( "\"%s\",\n", rgchFrameworkPath );
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }

    // add our needed preprocessor definitions
    CSplitString preprocessorDefines( pKV->GetString( g_pOption_PreprocessorDefinitions ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
    if ( preprocessorDefines.Count() )
    {
        Write( "GCC_PREPROCESSOR_DEFINITIONS = (\n" );
        ++m_nIndent;
        {
            Write( "\"$(GCC_PREPROCESSOR_DEFINITIONS)\",\n" );
            for ( int i=0; i < preprocessorDefines.Count(); i++ )
            {
                Write( "\"%s\",\n", preprocessorDefines[i] );
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }

    if ( pDictFiles )
    {
        CUtlMap<const char *, bool> searchPaths;
        searchPaths.SetLessFunc( StringLessThan );
        // libraries we consume (specified in our files list)
        for ( int i=pDictFiles->First(); i != pDictFiles->InvalidIndex(); i=pDictFiles->Next(i) )
        {
            const char *pFileName = (*pDictFiles)[i]->m_Filename.String();
            if ( IsStaticLibrary( pFileName ) || IsDynamicLibrary( pFileName ) )
            {
                char rgchLibPath[MAX_PATH];
                Q_snprintf( rgchLibPath, sizeof( rgchLibPath ), "%s/%s", pszProjectDir, pFileName );
                Q_RemoveDotSlashes( rgchLibPath );
                V_StripFilename( rgchLibPath );
                int nIndex = searchPaths.Find( rgchLibPath );
                if ( nIndex == searchPaths.InvalidIndex() )
                {
                    char *pszLibPath = new char[Q_strlen( rgchLibPath )+1];
                    Q_strncpy( pszLibPath, rgchLibPath, Q_strlen( rgchLibPath )+1 ); 
                    nIndex = searchPaths.Insert( pszLibPath );
                }
            }
        }
        
        // add the library path we know we need to reference
        Write( "LIBRARY_SEARCH_PATHS = (\n" );
        ++m_nIndent;
        {
            Write( "\"$(inherited)\",\n" );
            FOR_EACH_DICT_FAST( searchPaths, iSearchPath )
            {
                Write( "\"%s\",\n", searchPaths.Key(iSearchPath) );
                delete[] searchPaths.Key(iSearchPath);
            }
        }
        --m_nIndent;
        Write( ");\n" );
    }    
}

class CStringLess
{
public:
	bool Less( const char *lhs, const char *rhs, void *pCtx )
	{
		return ( strcmp( lhs, rhs ) < 0 ? true : false );
	}
};

void CSolutionGenerator_XCode::GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects )
{
    
    Assert( projects.Count() == g_vecPGenerators.Count() );

    FOR_EACH_VEC( projects, iProject )
    {
        AssertFatal( !Q_strcmp( projects[iProject]->m_ProjectName, g_vecPGenerators[iProject]->GetProjectName() ) );
    }
    char sPbxProjFile[MAX_PATH];
	sprintf( sPbxProjFile, "%s.xcodeproj", pSolutionFilename );
	mkdir( sPbxProjFile, 0777 );
	sprintf( sPbxProjFile, "%s.xcodeproj/project.pbxproj", pSolutionFilename );
	m_fp = fopen( sPbxProjFile, "wt" );
	m_nIndent = 0;
    
	Msg( "\nWriting master XCode project %s.xcodeproj.\n\n", pSolutionFilename );
    
    /** header **/
	Write( "// !$*UTF8*$!\n{\n" );
    ++m_nIndent;
    {
        /** 
         **
         ** preamble
         **
         **/
        Write( "archiveVersion = 1;\n" );
        Write( "classes = {\n" );
        Write( "};\n" );
        Write( "objectVersion = 44;\n" );
        Write( "objects = {\n" );
        {
            /** 
             **
             ** buildfiles - any file that's involved in, or the output of, a build phase
             **
             **/
            Write( "\n/* Begin PBXBuildFile section */" );
            ++m_nIndent;
            {                
                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                {                  
                    // poke into the project we're looking @ in the dependency projects vector to figure out it's location on disk
                    char rgchProjectDir[MAX_PATH]; rgchProjectDir[0] = '\0';
                    V_strncpy( rgchProjectDir, projects[iGenerator]->m_ProjectFilename.String(), sizeof( rgchProjectDir ) );
                    V_StripFilename( rgchProjectDir );

                    // the files this project references
                    for ( int i=g_vecPGenerators[iGenerator]->m_Files.First(); i != g_vecPGenerators[iGenerator]->m_Files.InvalidIndex(); i=g_vecPGenerators[iGenerator]->m_Files.Next(i) )
                    {        
                        char rgchFilePath[MAX_PATH];
                        Q_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s/%s", rgchProjectDir, g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() );
                        Q_RemoveDotSlashes( rgchFilePath );
                        
                        CFileConfig *pFileConfig = g_vecPGenerators[iGenerator]->m_Files[i];
                        const char *pFileName = pFileConfig->m_Filename.String();
                        
                        bool bExcluded = true;
                        for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                        {
                            bExcluded &= ( pFileConfig->IsExcludedFrom( k_rgchConfigNames[iConfig] ) );
                        }
                        
                        if ( bExcluded )
                            continue;
                                          
                        if ( AppearsInSourcesBuildPhase( g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() ) )
                        {
                            Write( "\n" );
                            Write( "%024llX /* %s in Sources */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };", makeoid( pFileName, EOIDTypeBuildFile ), V_UnqualifiedFileName( pFileName ), makeoid( pFileName, EOIDTypeFileReference ), V_UnqualifiedFileName( pFileName ) );
                        }
                        
                        if ( IsDynamicLibrary( pFileName ) )
                        {
                            Write( "\n" );
                            Write( "%024llX /* %s in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };", makeoid( pFileName, EOIDTypeBuildFile ), V_UnqualifiedFileName( pFileName ), makeoid( pFileName, EOIDTypeFileReference ), V_UnqualifiedFileName( pFileName ) );
                        }

                        if ( IsStaticLibrary( pFileName ) )
                        {
                            const char *pszLibPfx = "lib";
                            const char *pszLibName = V_UnqualifiedFileName( pFileName );
                            if ( pszLibName[0] == 'l' && pszLibName[1] == 'i' && pszLibName[2] == 'b' )
                                pszLibPfx = "";
                            Write( "\n" );
                            Write( "%024llX /* %s%s in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s%s */; };", makeoid( pFileName, EOIDTypeBuildFile ), pszLibPfx, V_UnqualifiedFileName( pFileName ), makeoid( pFileName, EOIDTypeFileReference ), pszLibPfx, V_UnqualifiedFileName( pFileName ) );
                        }
                    }
                    
                    // system libraries we link against
                    KeyValues *pKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                    CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < libs.Count(); i++ )
                    {
                        Write( "\n" );
                        Write( "%024llX /* lib%s.dylib in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* lib%s.dylib */; };", 
                              makeoid( pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeBuildFile, i ), libs[i], 
                              makeoid( pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeFileReference, i ), libs[i] );
                    }

                    // system frameworks we link against
                    CSplitString sysFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < sysFrameworks.Count(); i++ )
                    {
                        Write( "\n" );
                        Write( "%024llX /* %s.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s.framework */; };", 
                              makeoid( pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeBuildFile, i ), sysFrameworks[i], 
                              makeoid( pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeFileReference, i ), sysFrameworks[i] );
                    }

                    // local frameworks we link against
                    CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < localFrameworks.Count(); i++ )
                    {
                        char rgchFrameworkName[MAX_PATH];
                        V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );

                        Write( "\n" );
                        Write( "%024llX /* %s.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = %024llX /* %s.framework */; };", 
                              makeoid( pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeBuildFile, i ), rgchFrameworkName, 
                              makeoid( pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeFileReference, i ), rgchFrameworkName );
                    }

                    
                    // look at everyone who depends on us, and emit a build file pointing at our output file for each of them
                    CDependency_Project *pCurProject = projects[iGenerator];
					const char *pszOutputFile = pKV->GetString( g_pOption_GameOutputFile, "" );
                    CUtlString sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pszOutputFile ) );
                    if ( !sOutputFile.Length() || IsStaticLibrary( sOutputFile ) )
					{
						pszOutputFile = pKV->GetString( g_pOption_OutputFile, "" );
                        sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pszOutputFile ) );
					} 
                    if ( sOutputFile.Length() && ( IsStaticLibrary( sOutputFile ) || IsDynamicLibrary( sOutputFile ) ) )
                    {
                        for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                        {
                            if ( iGenerator == iTestProject )
                                continue;
                            
                            CDependency_Project *pTestProject = projects[iTestProject];
                            
                            CUtlVector<CDependency_Project*> additionalProjectDependencies;
                            ResolveAdditionalProjectDependencies( pTestProject, projects, additionalProjectDependencies );
                            
                            int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                            if ( pTestProject->DependsOn( pCurProject, dependsOnFlags ) || additionalProjectDependencies.Find( pCurProject ) != additionalProjectDependencies.InvalidIndex() )
                            {
								Write( "\n" );
								Write( "%024llX /* (lib)%s */ = {isa = PBXBuildFile; fileRef = %024llX /* (lib)%s - depended on by %s */; };", 
									makeoid( pszOutputFile, EOIDTypeBuildFile, iTestProject ), 
									sOutputFile.String(), 
									makeoid( pszOutputFile, EOIDTypeFileReference ), 
									sOutputFile.String(),
									pTestProject->m_ProjectName.String() );
                            }
                        }
                    }

                    // and our output file
                    if ( sOutputFile.Length() ) 
                    {
                        Write( "\n" );
                        Write( "%024llX /* %s in Products */ = {isa = PBXBuildFile; fileRef = %024llX /* %s */; };", 
                              makeoid( pKV->GetString( g_pOption_GameOutputFile, NULL ) ? pKV->GetString( g_pOption_GameOutputFile, NULL ) : pKV->GetString( g_pOption_OutputFile, NULL ), EOIDTypeBuildFile, -1 ), 
                              sOutputFile.String(), 
                              makeoid( pKV->GetString( g_pOption_GameOutputFile, NULL ) ? pKV->GetString( g_pOption_GameOutputFile, NULL ) : pKV->GetString( g_pOption_OutputFile, NULL ), EOIDTypeFileReference ), 
                              sOutputFile.String() );
                    }
                }                
            }
            --m_nIndent;
            Write( "\n/* End PBXBuildFile section */\n" );
            
            /** 
             **
             ** file references - any file that appears in the project browser
             **
             **/
            Write( "\n/* Begin PBXFileReference section */" );
            ++m_nIndent;
            {
                // include the xcconfig files
                for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchXCConfigFiles); iConfig++ )
                {
                    char rgchFilePath[MAX_PATH];
                    Q_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s.xcodeproj/../devtools/%s", pSolutionFilename, k_rgchXCConfigFiles[iConfig] );
                    Q_RemoveDotSlashes( rgchFilePath );
                    
                    Write( "\n" );
                    Write( "%024llX /* %s */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = text.xcconfig; name = \"%s\"; path = \"%s\"; sourceTree = \"<absolute>\"; };",
                          makeoid( k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ),
                          k_rgchXCConfigFiles[iConfig], 
                          k_rgchXCConfigFiles[iConfig],
                          rgchFilePath
                          );
                }
                
                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                {                    
                    // find the project we're looking @ in the dependency projects vector to figure out it's location on disk
                    char rgchProjectDir[MAX_PATH]; rgchProjectDir[0] = '\0';
                    V_strncpy( rgchProjectDir, projects[iGenerator]->m_ProjectFilename.String(), sizeof( rgchProjectDir ) );
                    V_StripFilename( rgchProjectDir );
                    
                    
                    for ( int i=g_vecPGenerators[iGenerator]->m_Files.First(); i != g_vecPGenerators[iGenerator]->m_Files.InvalidIndex(); i=g_vecPGenerators[iGenerator]->m_Files.Next(i) )
                    {
                        char rgchFilePath[MAX_PATH];
                        Q_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s/%s", rgchProjectDir, g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() );
                        Q_RemoveDotSlashes( rgchFilePath );
                        
                        const char *pFileName = V_UnqualifiedFileName( g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() );
                        
                        char rgchFileType[MAX_PATH];
                        XcodeFileTypeFromFileName( pFileName, rgchFileType, sizeof( rgchFileType ) );
                        
                        Write( "\n" );
                        Write( "%024llX /* %s */ = {isa = PBXFileReference; fileEncoding = 4; explicitFileType = \"%s\"; name = \"%s\"; path = \"%s\"; sourceTree = \"<absolute>\"; };",
                              makeoid( g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String(), EOIDTypeFileReference ),
                              pFileName, 
                              rgchFileType,
                              pFileName,
                              rgchFilePath );
                    }
                    KeyValues *pKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                    
                    // system libraries we link against
                    CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < libs.Count(); i++ )
                    {
                        Write( "\n" );
                        Write( "%024llX /* lib%s.dylib */ = {isa = PBXFileReference; lastKnownFileType = \"compiled.mach-o.dylib\"; name = \"lib%s.dylib\"; path = \"usr/lib/lib%s.dylib\"; sourceTree = SDKROOT; };",
                              makeoid( pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeFileReference, i ), libs[i], libs[i], libs[i] );
                    }
                    
                    // system frameworks we link against
                    CSplitString sysFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < sysFrameworks.Count(); i++ )
                    {
                        Write( "\n" );
                        Write( "%024llX /* %s.framework */ = {isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = %s.framework; path = System/Library/Frameworks/%s.framework; sourceTree = SDKROOT; };",
                              makeoid( pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeFileReference, i ), sysFrameworks[i], sysFrameworks[i], sysFrameworks[i] );
                    }

                    // local frameworks we link against
                    CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                    for ( int i=0; i < localFrameworks.Count(); i++ )
                    {
                        char rgchFrameworkName[MAX_PATH];
                        V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );

                        char rgchFrameworkPath[MAX_PATH];
                        Q_snprintf( rgchFrameworkPath, sizeof( rgchFrameworkPath ), "%s/%s", rgchProjectDir, localFrameworks[i] );
                        Q_RemoveDotSlashes( rgchFrameworkPath );

                        Write( "\n" );
                        Write( "%024llX /* %s.framework */ = {isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = %s.framework; path = \"%s\"; sourceTree = \"<absolute>\"; };",                        
                              makeoid( pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeFileReference, i ), rgchFrameworkName, rgchFrameworkName, rgchFrameworkPath ); 
                    }

                    // include the output files (build products)                  
		            const char *pszOutputFile = pKV->GetString( g_pOption_GameOutputFile, "" );
                    CUtlString sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pszOutputFile ) );
                    if ( !sOutputFile.Length() || IsStaticLibrary( sOutputFile ) )
		            {
			            pszOutputFile = pKV->GetString( g_pOption_OutputFile, "" );
                        sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pszOutputFile ) );
		            }
                    if ( sOutputFile.Length() )
                    {
                        char rgchFileType[MAX_PATH];
                        XcodeFileTypeFromFileName( sOutputFile, rgchFileType, sizeof( rgchFileType ) );
                        
                        Write( "\n" );
                        if ( IsStaticLibrary( sOutputFile ) )
                        {
                            const char *pszLibPfx = "lib";
                            const char *pszLibName = sOutputFile;
                            if ( pszLibName[0] == 'l' && pszLibName[0] == 'i' && pszLibName[0] == 'b' )
                                pszLibPfx = "";
                            Write( "%024llX /* %s%s */ = {isa = PBXFileReference; explicitFileType = \"%s\"; includeInIndex = 0; path = \"%s%s\"; sourceTree = BUILT_PRODUCTS_DIR; };", 
                                  makeoid( pszOutputFile, EOIDTypeFileReference ), pszLibPfx, sOutputFile.String(), rgchFileType, pszLibPfx, sOutputFile.String() );
                        }
                        else
                            Write( "%024llX /* %s */ = {isa = PBXFileReference; explicitFileType = \"%s\"; includeInIndex = 0; path = \"%s\"; sourceTree = BUILT_PRODUCTS_DIR; };", 
                                  makeoid( pszOutputFile, EOIDTypeFileReference ), sOutputFile.String(), rgchFileType, sOutputFile.String() );
                    }

                    
                    // and the gameoutputfile
                    CUtlString sGameOutputFile = UsePOSIXSlashes( pKV->GetString( g_pOption_GameOutputFile, "" ) );
                    if ( sGameOutputFile.Length() )
                    {
                        char rgchFilePath[MAX_PATH];
                        Q_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s/%s", rgchProjectDir, sGameOutputFile.String() );
                        Q_RemoveDotSlashes( rgchFilePath );
                        
                        char rgchFileType[MAX_PATH];
                        XcodeFileTypeFromFileName( sGameOutputFile, rgchFileType, sizeof( rgchFileType ) );
                        
                        Write( "\n" );
                        Write( "%024llX /* %s */ = {isa = PBXFileReference; explicitFileType = \"%s\"; includeInIndex = 0; path = \"%s\"; sourceTree = \"<absolute>\"; };", 
                              makeoid( pKV->GetString( g_pOption_GameOutputFile, "" ), EOIDTypeFileReference ), V_UnqualifiedFileName( sGameOutputFile ), rgchFileType, rgchFilePath );
                    }
                    
                }
                
            }
            --m_nIndent;
            Write( "\n/* End PBXFileReference section */\n" );
            
            /** 
             **
             ** groups - the file hierarchy displayed in the project
             **
             **/
            Write( "\n/* Begin PBXGroup section */\n" );
            ++m_nIndent;
            {
                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                {
                    CUtlVector<char*> folderNames;
                    V_SplitString( "Source Files;Header Files;Resources;VPC Files", ";", folderNames );
                    
                    static const char* folderExtensions[] = 
                    {
                        "*.c;*.C;*.cc;*.cpp;*.cp;*.cxx;*.c++;*.prg;*.pas;*.dpr;*.asm;*.s;*.bas;*.java;*.cs;*.sc;*.e;*.cob;*.html;*.tcl;*.py;*.pl;*.m;*.mm",
                        "*.h;*.H;*.hh;*.hpp;*.hxx;*.inc;*.sh;*.cpy;*.if",
                        "*.plist;*.strings;*.xib;*.rc;*.proto",
                        "*.vpc"
                    };
                    
                    FOR_EACH_VEC( folderNames, iFolder )
                    {
                        WriteFilesFolder( makeoid( g_vecPGenerators[iGenerator]->m_ProjectName, EOIDTypeGroup, iFolder+1 ), folderNames[iFolder], folderExtensions[iFolder], g_vecPGenerators[iGenerator]->m_Files );
                    }
                    
                    Write( "%024llX /* %s */ = {\n", makeoid( g_vecPGenerators[iGenerator], EOIDTypeGroup ), g_vecPGenerators[iGenerator]->GetProjectName().String() );
                    ++m_nIndent;
                    {
                        Write( "isa = PBXGroup;\n" );
                        Write( "children = (\n" );        
                        
                        ++m_nIndent;
                        {
                            FOR_EACH_VEC( folderNames, iFolder )
                            {
                                Write( "%024llX /* %s */,\n", makeoid( g_vecPGenerators[iGenerator]->m_ProjectName, EOIDTypeGroup, iFolder+1 ), folderNames[iFolder] );        
                            }

                            KeyValues *pKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;

                            // system libraries we link against
                            CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                            for ( int i=0; i < libs.Count(); i++ )
                            {
                                Write( "%024llX /* lib%s.dylib (system library) */,\n", makeoid( pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeFileReference, i ), libs[i] );
                            }
                            
                            // system frameworks we link against
                            CSplitString sysFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                            for ( int i=0; i < sysFrameworks.Count(); i++ )
                            {
                                Write( "%024llX /* %s.framework (system framework) */,\n", makeoid( pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeFileReference, i ), sysFrameworks[i] );
                            }
                            
                            // local frameworks we link against
                            CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                            for ( int i=0; i < localFrameworks.Count(); i++ )
                            {
                                char rgchFrameworkName[MAX_PATH];
                                V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );

                                Write( "%024llX /* %s.framework (local framework) */,\n", makeoid( pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeFileReference, i ), rgchFrameworkName ); 
                            }

                            // libraries we consume (specified in our files list)
                            for ( int i=g_vecPGenerators[iGenerator]->m_Files.First(); i != g_vecPGenerators[iGenerator]->m_Files.InvalidIndex(); i=g_vecPGenerators[iGenerator]->m_Files.Next(i) )
                            {
                                CUtlString sFileName = V_UnqualifiedFileName( UsePOSIXSlashes( g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String() ) );
								bool bInclude = IsDynamicLibrary( sFileName );
                                if ( IsStaticLibrary( sFileName ) )
                                {
                                    bInclude = true;
                                    FOR_EACH_VEC( g_vecPGenerators, iGenerator2 )
                                    {
                                        // don't include static libs generated by other projects - we'll pull them out of the built products tree
                                        KeyValues *pKV = g_vecPGenerators[iGenerator2]->m_BaseConfigData.m_Configurations[0]->m_pKV;                                        
                                        CUtlString sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pKV->GetString( g_pOption_OutputFile, "" ) ) );
                                        if ( !Q_stricmp( sFileName, sOutputFile ) )
                                        {
                                            bInclude = false;
                                            break;
                                        }
                                    }
								}
                                if ( bInclude )
                                    Write( "%024llX /* %s in Frameworks (explicit) */,\n", makeoid( g_vecPGenerators[iGenerator]->m_Files[i]->m_Filename.String(), EOIDTypeFileReference ), sFileName.String() );
                            }
                            
                            CUtlString sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pKV->GetString( g_pOption_OutputFile, "" ) ) );
                            if ( sOutputFile.Length() )
                                Write( "%024llX /* %s */,\n", makeoid( pKV->GetString( g_pOption_OutputFile, "" ), EOIDTypeFileReference ), sOutputFile.String() );

                        }
                        --m_nIndent;
                        
                        Write( ");\n" );
                        Write( "name = \"%s\";\n", g_vecPGenerators[iGenerator]->GetProjectName().String() );
                        Write( "sourceTree = \"<group>\";\n" );
                    }
                    --m_nIndent;
                    Write( "};\n" );
                    
                }
                                
                // root group - the top of the displayed hierarchy
                Write( "%024llX = {\n", makeoid( &projects, EOIDTypeGroup ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXGroup;\n" );
                    Write( "children = (\n" );
					
					// sort the projects by name before we emit the list
					CUtlSortVector< CUtlString, CStringLess > vecSortedProjectNames;
					FOR_EACH_VEC( g_vecPGenerators, iGen )
					{
						// fprintf( stderr, "inserting %s (%p)\n",  g_vecPGenerators[iGen]->GetProjectName().String(), &g_vecPGenerators[iGen]->GetProjectName() );
						vecSortedProjectNames.Insert( g_vecPGenerators[iGen]->GetProjectName() );
					}

                    ++m_nIndent;
                    {
						FOR_EACH_VEC( vecSortedProjectNames, iProjectName ) 
						{
							// fprintf( stderr, "looking for %s\n", vecSortedProjectNames[iProjectName].String() );
                        // and each project's group (of groups)
                        FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                        {
								if ( strcmp( g_vecPGenerators[iGenerator]->m_ProjectName.String(), vecSortedProjectNames[iProjectName] ) )
								{
									// fprintf( stderr, "   skipping '%s' (%p) != '%s' (%p) (%d, %d)\n", g_vecPGenerators[iGenerator]->GetProjectName().String(), g_vecPGenerators[iGenerator]->GetProjectName().String(), vecSortedProjectNames[iProjectName].String(), vecSortedProjectNames[iProjectName].String(), iGenerator, iProjectName );
									continue;
								}
								// fprintf( stderr, "emitting %s (%d, %d)\n", g_vecPGenerators[iGenerator]->GetProjectName().String(), iGenerator, iProjectName );

                            Write( "%024llX /* %s */,\n", makeoid( g_vecPGenerators[iGenerator], EOIDTypeGroup ), g_vecPGenerators[iGenerator]->GetProjectName().String() );
								break;
							}
                        }

                        // add the build config (.xcconfig) files
                        for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchXCConfigFiles); iConfig++ )
                        {
                            Write( "%024llX /* %s */, \n", makeoid( k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
                        }
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "sourceTree = \"<group>\";\n" );
					
                }
                --m_nIndent;
                Write( "};" );
            }
            m_nIndent--;
            Write( "\n/* End PBXGroup section */\n" );
            
            
            /** 
             **
             ** the sources build phases - each target that compiles source references on of these, it in turn references the source files to be compiled
             **
             **/
            Write( "\n/* Begin PBXSourcesBuildPhase section */" );
            ++m_nIndent;
            FOR_EACH_VEC( projects, iProject )
            {
                Write( "\n" );
                Write( "%024llX /* Sources */ = {\n", makeoid( projects[iProject], EOIDTypeSourcesBuildPhase ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXSourcesBuildPhase;\n" );
                    Write( "buildActionMask = 2147483647;\n" );
                    Write( "files = (\n" );
                    ++m_nIndent;
                    {
                        for ( int i=g_vecPGenerators[iProject]->m_Files.First(); i != g_vecPGenerators[iProject]->m_Files.InvalidIndex(); i=g_vecPGenerators[iProject]->m_Files.Next(i) )
                        {
                            const char *pFileName = g_vecPGenerators[iProject]->m_Files[i]->m_Filename.String();
                            if ( AppearsInSourcesBuildPhase( pFileName ) )
                            {
                                Write( "%024llX /* %s in Sources */,\n", makeoid( pFileName, EOIDTypeBuildFile ), V_UnqualifiedFileName( UsePOSIXSlashes( pFileName ) ) );
                            }
                        }
                    }
                    --m_nIndent;
                    Write( ");\n");
                    Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
                }
                --m_nIndent;
                Write( "};" );
            }
            --m_nIndent;
            Write( "\n/* End PBXSourcesBuildPhase section */\n" );
            
            
            /** 
             **
             ** the frameworks build phases - each target that links libraries (static, dyamic, framework) has one of these, it references the linked thing
             **
             **/
            Write( "\n/* Begin PBXFrameworksBuildPhase section */" );
            ++m_nIndent;
            FOR_EACH_VEC( projects, iProject )
            {
                Write( "\n" );
                Write( "%024llX /* Frameworks */ = {\n", makeoid( projects[iProject], EOIDTypeFrameworksBuildPhase ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXFrameworksBuildPhase;\n" );
                    Write( "buildActionMask = 2147483647;\n" );
                    Write( "files = (\n" );
                    ++m_nIndent;
                    {
                        // libraries we consume (specified in our files list)
                        for ( int i=g_vecPGenerators[iProject]->m_Files.First(); i != g_vecPGenerators[iProject]->m_Files.InvalidIndex(); i=g_vecPGenerators[iProject]->m_Files.Next(i) )
                        {
                            const char *pFileName = g_vecPGenerators[iProject]->m_Files[i]->m_Filename.String();
                            if ( IsStaticLibrary( UsePOSIXSlashes( pFileName ) ) || IsDynamicLibrary( UsePOSIXSlashes( pFileName ) ) )
                            {
                                CUtlString sFileName = V_UnqualifiedFileName( UsePOSIXSlashes( g_vecPGenerators[iProject]->m_Files[i]->m_Filename.String() ) );
								bool bInclude = true;
								FOR_EACH_VEC( g_vecPGenerators, iGenerator )
								{
									// don't include libs generated by other projects - we'll pull them out of the built products tree
									KeyValues *pKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;                                        
									CUtlString sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pKV->GetString( g_pOption_OutputFile, "" ) ) );
									if ( !Q_stricmp( sFileName, sOutputFile ) )
									{
										bInclude = false;
										break;
									}
								}
                                if ( bInclude )
									Write( "%024llX /* %s in Frameworks (explicit) */,\n", makeoid( pFileName, EOIDTypeBuildFile ), V_UnqualifiedFileName( UsePOSIXSlashes( pFileName ) ) );

                            }
                        }

                        // libraries from projects we depend on
                        CDependency_Project *pCurProject = projects[iProject];
                        
                        CUtlVector<CDependency_Project*> additionalProjectDependencies;
                        ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );
                        
                        for ( int iTestProject=projects.Count()-1; iTestProject >= 0; --iTestProject )
                        {
                            if ( iProject == iTestProject )
                                continue;
                            
                            CDependency_Project *pTestProject = projects[iTestProject];
                            int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                            if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                            {
                                KeyValues *pKV = g_vecPGenerators[iTestProject]->m_BaseConfigData.m_Configurations[0]->m_pKV;
								const char *pszOutputFile = pKV->GetString( g_pOption_GameOutputFile, "" );
                                CUtlString sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pszOutputFile ) );
                                if ( !sOutputFile.Length() || IsStaticLibrary( sOutputFile ) )
								{ 
									pszOutputFile = pKV->GetString( g_pOption_OutputFile, "" );
									sOutputFile = V_UnqualifiedFileName( UsePOSIXSlashes( pszOutputFile ) );
								}
								if ( sOutputFile.Length() && ( IsStaticLibrary( sOutputFile ) || IsDynamicLibrary( sOutputFile ) ) )
                                {
                                    Write( "%024llX /* (lib)%s (dependency) */,\n", makeoid( pszOutputFile, EOIDTypeBuildFile, iProject ), sOutputFile.String() );
                                }
                            }
                        }

						KeyValues *pKV = g_vecPGenerators[iProject]->m_BaseConfigData.m_Configurations[0]->m_pKV;

                        // local frameworks we link against
                        CSplitString localFrameworks( pKV->GetString( g_pOption_LocalFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                        for ( int i=0; i < localFrameworks.Count(); i++ )
                        {
                            char rgchFrameworkName[MAX_PATH];
                            V_StripExtension( V_UnqualifiedFileName( localFrameworks[i] ), rgchFrameworkName, sizeof( rgchFrameworkName ) );
                            
                            Write( "%024llX /* %s in Frameworks (local framework) */,\n", makeoid( pKV->GetString( g_pOption_LocalFrameworks ), EOIDTypeBuildFile, i ), rgchFrameworkName ); 
                        }
                       
                        // system frameworks we link against
                        CSplitString sysFrameworks( pKV->GetString( g_pOption_SystemFrameworks ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                        for ( int i=0; i < sysFrameworks.Count(); i++ )
                        {
                            Write( "%024llX /* %s in Frameworks (system framework) */,\n", makeoid( pKV->GetString( g_pOption_SystemFrameworks ), EOIDTypeBuildFile, i ), sysFrameworks[i] );
                        }
                        
                        // system libraries we link against
                        CSplitString libs( pKV->GetString( g_pOption_SystemLibraries ), (const char**)g_IncludeSeparators, Q_ARRAYSIZE(g_IncludeSeparators) );
                        for ( int i=0; i < libs.Count(); i++ )
                        {
                            Write( "%024llX /* %s in Frameworks (system library) */,\n", makeoid( pKV->GetString( g_pOption_SystemLibraries ), EOIDTypeBuildFile, i ), libs[i] );
                        }
                                             
                    }
                    --m_nIndent;
                    Write( ");\n");
                    Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
                }
                --m_nIndent;
                Write( "};" );
            }
            --m_nIndent;
            Write( "\n/* End PBXFrameworksBuildPhase section */\n" );
            
            
            /** 
             **
             ** the shell script (pre/post build step) build phases - each target that generates a "gameoutputfile" has one of these, 
			 ** to p4 edit the target and copy the build result there.  
             **
             **/
            Write( "\n/* Begin PBXShellScriptBuildPhase section */" );
            ++m_nIndent;
            {
                FOR_EACH_VEC( g_vecPGenerators, iGenerator )
                {
                    char rgchProjectDir[MAX_PATH]; rgchProjectDir[0] = '\0';
                    V_strncpy( rgchProjectDir, projects[iGenerator]->m_ProjectFilename.String(), sizeof( rgchProjectDir ) );
                    V_StripFilename( rgchProjectDir );

                    char sInputFiles[8192]; sInputFiles[0] = '\0';
                    char sCommands[8192]; sCommands[0] = '\0';
                    char sOutputFiles[8192]; sOutputFiles[0] = '\0';
					// we don't have an output file - wander the list of files, looking for custom build steps
					// if we find any, magic up a shell script to run them
					for ( int i=g_vecPGenerators[iGenerator]->m_Files.First(); i != g_vecPGenerators[iGenerator]->m_Files.InvalidIndex(); i=g_vecPGenerators[iGenerator]->m_Files.Next(i) )
					{
						
						CFileConfig *pFileConfig = g_vecPGenerators[iGenerator]->m_Files[i];
						CSpecificConfig *pFileSpecificData = pFileConfig->GetOrCreateConfig( g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1]->GetConfigName(), 
																							 g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1] );
						
						CUtlString sCustomBuildCommandLine = pFileSpecificData->GetOption( g_pOption_CommandLine );
						CUtlString sOutputFile = pFileSpecificData->GetOption( g_pOption_Outputs );
                        
						if ( sOutputFile.Length() && sCustomBuildCommandLine.Length() )
						{
							char sInFile[MAX_PATH];
							Q_snprintf( sInFile, sizeof( sInFile ), "%s/%s", rgchProjectDir, UsePOSIXSlashes( pFileConfig->m_Filename.String() ) );

							Q_snprintf( sInputFiles, sizeof( sInputFiles ), "%s%s%s", sInputFiles, sInputFiles[0] ? ";" : "", UsePOSIXSlashes( pFileConfig->m_Filename.String() ) );
							char sCommand[8192];
							CBaseProjectDataCollector::DoStandardVisualStudioReplacements( sCustomBuildCommandLine, sInFile, sCommand, sizeof( sCommand ) );
							Q_StrSubstInPlace( sCommand, sizeof( sCommand ), "$(OBJ_DIR)", rgchProjectDir, false );
							Q_snprintf( sCommands, sizeof( sCommands ), "%s%s;\\n", sCommands, sCommand );

							char sOutFile[MAX_PATH];
							CBaseProjectDataCollector::DoStandardVisualStudioReplacements( sOutputFile, sInFile, sOutFile, sizeof( sOutFile ) );
							Q_StrSubstInPlace( sOutFile, sizeof( sOutFile ), "$(OBJ_DIR)", rgchProjectDir, false );
							Q_snprintf( sOutputFiles, sizeof( sOutputFiles ), "%s%s%s", sOutputFiles, sOutFile[0] ? ";" : "", sOutFile );
						}
					}
					if ( Q_strlen( sCommands ) )
					{
						Write( "\n" );
						Write( "%024llX /* ShellScript */ = {\n", makeoid( projects[iGenerator], EOIDTypeShellScriptBuildPhase, 0 ) );
						++m_nIndent;
						{
							Write( "isa = PBXShellScriptBuildPhase;\n" );
							Write( "buildActionMask = 2147483647;\n" );
							Write( "files = (\n" );
							Write( ");\n" );
							Write( "inputPaths = (\n" );
							++m_nIndent;
							{
								CSplitString inFiles( sInputFiles, ";" );
								for ( int i = 0; i < inFiles.Count(); i ++ )
								{
									char rgchFilePath[MAX_PATH];
									Q_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s/%s", rgchProjectDir, inFiles[i] );
									Q_RemoveDotSlashes( rgchFilePath ); 
									Write( "\"%s\",\n", rgchFilePath );
								}
							}
							--m_nIndent;
							Write( ");\n" );
							Write( "outputPaths = (\n" );
							++m_nIndent;
							{
								CSplitString outFiles( sOutputFiles, ";" );
								for ( int i = 0; i < outFiles.Count(); i ++ )
								{
									char rgchFilePath[MAX_PATH];
									Q_snprintf( rgchFilePath, sizeof( rgchFilePath ), "%s/%s", rgchProjectDir, outFiles[i] );
									Q_RemoveDotSlashes( rgchFilePath ); 
									Write( "\"%s\",\n", rgchFilePath );
								}
							}
							--m_nIndent;
							Write( ");\n");
							Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
							Write( "shellPath = /bin/bash;\n" );
							Write( "shellScript = \"cd %s;\\n"
								   "%s\";\n", rgchProjectDir, sCommands );
						}
						--m_nIndent;
						Write( "};" );
						
					}
                    
                    KeyValues *pDebugKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                    CUtlString sDebugOutputFile = UsePOSIXSlashes( pDebugKV->GetString( g_pOption_GameOutputFile, "" ) );
                    
                    KeyValues *pReleaseKV = g_vecPGenerators[iGenerator]->m_BaseConfigData.m_Configurations[1]->m_pKV;
                    CUtlString sReleaseOutputFile = UsePOSIXSlashes( pReleaseKV->GetString( g_pOption_GameOutputFile, "" ) );
                    
                    if ( !sDebugOutputFile.Length() || !sReleaseOutputFile.Length() )
                    {
                        sDebugOutputFile = UsePOSIXSlashes( pDebugKV->GetString( g_pOption_OutputFile, "" ) );
                        sReleaseOutputFile = UsePOSIXSlashes( pReleaseKV->GetString( g_pOption_OutputFile, "" ) );
                    }
                    
                    if ( sDebugOutputFile.Length() || sReleaseOutputFile.Length() )
                    {                        
                        char rgchDebugFilePath[MAX_PATH];
                        Q_snprintf( rgchDebugFilePath, sizeof( rgchDebugFilePath ), "%s/%s", rgchProjectDir, sDebugOutputFile.String() );
                        Q_RemoveDotSlashes( rgchDebugFilePath );
                        
                        char rgchReleaseFilePath[MAX_PATH];
                        Q_snprintf( rgchReleaseFilePath, sizeof( rgchReleaseFilePath ), "%s/%s", rgchProjectDir, sReleaseOutputFile.String() );
                        Q_RemoveDotSlashes( rgchReleaseFilePath );
                        
                        Write( "\n" );
                        Write( "%024llX /* ShellScript */ = {\n", makeoid( projects[iGenerator], EOIDTypeShellScriptBuildPhase, 1 ) );
                        ++m_nIndent;
                        {
                            Write( "isa = PBXShellScriptBuildPhase;\n" );
                            Write( "buildActionMask = 2147483647;\n" );
                            Write( "files = (\n" );
                            Write( ");\n" );
                            Write( "inputPaths = (\n" );
                            ++m_nIndent;
                            {
                                Write( "\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}\",\n" );
                            }
                            --m_nIndent;
                            Write( ");\n" );
                            Write( "outputPaths = (\n" );
                            ++m_nIndent;
                            {
                                Q_StrSubstInPlace( rgchDebugFilePath, sizeof( rgchDebugFilePath ), "/lib/osx32/debug/", "/lib/osx32/${BUILD_STYLE}/", false );
                                Q_StrSubstInPlace( rgchReleaseFilePath, sizeof( rgchReleaseFilePath ), "/lib/osx32/release/", "/lib/osx32/${BUILD_STYLE}/", false );

                                Write( "\"%s\",\n", rgchDebugFilePath );
                                if ( Q_strcmp( rgchDebugFilePath, rgchReleaseFilePath ) )
                                    Write( "\"%s\",\n", rgchReleaseFilePath );
                            }
                            --m_nIndent;
                            Write( ");\n");
                            Write( "runOnlyForDeploymentPostprocessing = 0;\n" );
                            Write( "shellPath = /bin/bash;\n" );
                            Write( "shellScript = "
                                  "\"if [ \\\"$BUILD_STYLE\\\" == \\\"Release\\\" ]; then\\n"
                                  "  OUTPUTFILE=%s\\n"
                                  "  POST_BUILD_COMMAND=%s\\n"
                                  "else\\n"
                                  "  OUTPUTFILE=%s\\n"
                                  "  POST_BUILD_COMMAND=%s\\n"
                                  "fi\\n"
                                  "if [ \\\"$POST_BUILD_COMMAND\\\" != \\\"\\\" ]; then\\n"
                                  "  ${POST_BUILD_COMMAND}\\n"
                                  "fi\\n"
								  "if [ -x \\\"$OUTPUTFILE\\\" -o -x \\\"$OUTPUTFILE.dSYM\\\" ]; then\\n"
                                  "  p4 edit \\\"$OUTPUTFILE...\\\" | grep -v \\\"also opened\\\"\\n"
								  "fi\\n"
                                  "if [ -d \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}.dSYM\\\" ]; then\\n"
                                  "  cp -R \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}.dSYM\\\" \\\"${OUTPUTFILE}.dSYM\\\"\\n"
                                  "fi\\n"
                                  "cp -v \\\"${TARGET_BUILD_DIR}/${FULL_PRODUCT_NAME}\\\" \\\"$OUTPUTFILE\\\"\";\n", 
                                  rgchReleaseFilePath, pReleaseKV->GetString( g_pOption_CommandLine, "" ), 
                                  rgchDebugFilePath, pDebugKV->GetString( g_pOption_CommandLine, "" ) );
                        }
                        --m_nIndent;
                        Write( "};" );
                    }            
                }
            }
            --m_nIndent;
            Write( "\n/* End PBXShellScriptBuildPhase section */\n" );

            /** 
             **
             ** nativetargets section - build targets, which ultimately reference build phases
             **
             **/
            Write( "\n/* Begin PBXNativeTarget section */" );
            ++m_nIndent;
            FOR_EACH_VEC( projects, iProject )
            {
                KeyValues *pKV = g_vecPGenerators[iProject]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                CUtlString sOutputFile = UsePOSIXSlashes( pKV->GetString( g_pOption_GameOutputFile, "" ) );
                if ( !sOutputFile.Length() )
                    sOutputFile = UsePOSIXSlashes( pKV->GetString( g_pOption_OutputFile, "" ) );
                if ( !sOutputFile.Length() )
                    continue;
                
                Write( "\n" );
                Write( "%024llX /* %s */ = {\n", makeoid( projects[iProject], EOIDTypeNativeTarget ), projects[iProject]->m_ProjectName.String() );
                ++m_nIndent;
                {
                    Write( "isa = PBXNativeTarget;\n" );
                    
                    Write( "buildConfigurationList = %024llX /* Build configuration list for PBXNativeTarget \"%s\" */;\n", makeoid( projects[iProject], EOIDTypeConfigurationList ), projects[iProject]->m_ProjectName.String() );
                    Write( "buildPhases = (\n" );
                    ++m_nIndent;
                    {
                        Write( "%024llX /* ShellScript */,\n",  makeoid( projects[iProject], EOIDTypeShellScriptBuildPhase, 0 ) );
                        Write( "%024llX /* Sources */,\n",  makeoid( projects[iProject], EOIDTypeSourcesBuildPhase ) );
                        Write( "%024llX /* Frameworks */,\n",  makeoid( projects[iProject], EOIDTypeFrameworksBuildPhase ) );                        
                        Write( "%024llX /* ShellScript */,\n",  makeoid( projects[iProject], EOIDTypeShellScriptBuildPhase, 1 ) );
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "buildRules = (\n" );
                    ++m_nIndent;
                    {
                        
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "dependencies = (\n" );
                    ++m_nIndent;
                    {
                        // these dependencies point to the dependency objects, which reference other projects through the container item proxy objects
                        CDependency_Project *pCurProject = projects[iProject];
                        
                        CUtlVector<CDependency_Project*> additionalProjectDependencies;
                        ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );
                        
                        for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                        {
                            if ( iProject == iTestProject )
                                continue;
                            
                            CDependency_Project *pTestProject = projects[iTestProject];
                            int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                            if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                            {
                                Write( "%024llX /* %s */,\n", makeoid( projects[iProject], EOIDTypeTargetDependency, (uint16_t)iTestProject ), pTestProject->GetName() );
                            }
                        }
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "productName = \"%s\";\n", projects[iProject]->m_ProjectName.String() );
                    Write( "name = \"%s\";\n", projects[iProject]->m_ProjectName.String() );
                    
                    if ( sOutputFile.Length() )
                        Write( "productReference = %024llX /* %s */;\n", makeoid( pKV->GetString( g_pOption_GameOutputFile, NULL ) ? pKV->GetString( g_pOption_GameOutputFile, NULL ) : pKV->GetString( g_pOption_OutputFile, "" ), EOIDTypeFileReference ), V_UnqualifiedFileName( sOutputFile ) );
                    
                    char rgchProductType[MAX_PATH];
                    XcodeProductTypeFromFileName( V_UnqualifiedFileName( sOutputFile ), rgchProductType, sizeof( rgchProductType ) );
                    Write( "productType = \"%s\";\n", rgchProductType );                
                }
                --m_nIndent;
                Write( "};" );
            }
            --m_nIndent;
            Write( "\n/* End PBXNativeTarget section */\n" );
            
            /** 
             **
             ** aggregate targets - for targets that have no output files (i.e. are scripts)
             ** and the "all" target
             **
             **/
            Write( "\n/* Begin PBXAggregateTarget section */\n" );
            ++m_nIndent;
            {
                Write( "%024llX /* All */ = {\n", makeoid( this, EOIDTypeAggregateTarget ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXAggregateTarget;\n" );
                    Write( "buildConfigurationList = %024llX /* Build configuration list for PBXAggregateTarget \"All\" */;\n", makeoid( this, EOIDTypeConfigurationList, 1 ) );
                    Write( "buildPhases = (\n" );
                    Write( ");\n" );
                    Write( "dependencies = (\n" );
                    ++m_nIndent;
                    {
                        FOR_EACH_VEC( projects, iProject )
                        {
                            // note the sneaky -1 ordinal here, is we can later generate a dependency block for the target thats not tied to any other targets dependency.
                            Write( "%024llX /* PBXProjectDependency */,\n", makeoid( projects[iProject], EOIDTypeTargetDependency, -1 ) );
                        }   
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "name = All;\n" );
                    Write( "productName = All;\n" );
                }
                --m_nIndent;
                Write( "};\n" );
                
                FOR_EACH_VEC( projects, iProject )
                {
                    KeyValues *pKV = g_vecPGenerators[iProject]->m_BaseConfigData.m_Configurations[0]->m_pKV;
                    CUtlString sOutputFile = UsePOSIXSlashes( pKV->GetString( g_pOption_GameOutputFile, "" ) );
                    if ( !sOutputFile.Length() )
                        sOutputFile = UsePOSIXSlashes( pKV->GetString( g_pOption_OutputFile, "" ) );
                    if ( sOutputFile.Length() )
                        continue;
                    
                    // NOTE: the use of EOIDTypeNativeTarget here is intentional - a project will never appear as both, and this makes things link up without 
                    // having to special case in dependencies and aggregate targets
                    Write( "%024llX /* %s */ = {\n", makeoid( projects[iProject], EOIDTypeNativeTarget ), projects[iProject]->m_ProjectName.String() );
                    ++m_nIndent;
                    {
                        Write( "isa = PBXAggregateTarget;\n" );
                        
                        Write( "buildConfigurationList = %024llX /* Build configuration list for PBXAggregateTarget \"%s\" */;\n", makeoid( projects[iProject], EOIDTypeConfigurationList ), projects[iProject]->m_ProjectName.String() );
                        Write( "buildPhases = (\n" );
                        ++m_nIndent;
                        {
                            Write( "%024llX /* ShellScript */,\n",  makeoid( projects[iProject], EOIDTypeShellScriptBuildPhase ) );
                        }
                        --m_nIndent;
                        Write( ");\n" );
                        Write( "buildRules = (\n" );
                        ++m_nIndent;
                        {
                            
                        }
                        --m_nIndent;
                        Write( ");\n" );
                        Write( "dependencies = (\n" );
                        ++m_nIndent;
                        {
                            // these dependencies point to the dependency objects, which reference other projects through the container item proxy objects
                            CDependency_Project *pCurProject = projects[iProject];
                            
                            CUtlVector<CDependency_Project*> additionalProjectDependencies;
                            ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );
                            
                            for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                            {
                                if ( iProject == iTestProject )
                                    continue;
                                
                                CDependency_Project *pTestProject = projects[iTestProject];
                                int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                                if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                                {
                                    Write( "%024llX /* %s */,\n", makeoid( projects[iProject], EOIDTypeTargetDependency, (uint16_t)iTestProject ), pTestProject->GetName() );
                                }
                            }
                        }
                        --m_nIndent;
                        Write( ");\n" );
                        Write( "name = \"%s\";\n", projects[iProject]->m_ProjectName.String() );
                        Write( "productName = \"%s\";\n", projects[iProject]->m_ProjectName.String() );
                    }
                    --m_nIndent;
                    Write( "};" );
                }
            }
            --m_nIndent;
            Write( "\n/* End PBXAggregateTarget section */\n" );
            
            /** 
             **
             ** project section - the top-level object that ties all the bits (targets, groups, ...) together
             **
             **/
            Write( "\n/* Begin PBXProject section */\n" );
            ++m_nIndent;
            Write( "%024llX /* project object */ = {\n", makeoid( this, EOIDTypeProject ) );
            ++m_nIndent;
            {
                Write( "isa = PBXProject;\n" );
				Write( "attributes = {\n" );
				++m_nIndent;
				{
					Write( "BuildIndependentTargetsInParallel = YES;\n" );
				}
				--m_nIndent;
			    Write( "};\n" );
                Write( "buildConfigurationList = %024llX /* Build configuration list for PBXProject \"%s\" */;\n", makeoid( this, EOIDTypeConfigurationList ), V_UnqualifiedFileName( UsePOSIXSlashes( pSolutionFilename ) ) ); 
                Write( "compatibilityVersion = \"Xcode 3.0\";\n" );
                Write( "hasScannedForEncodings = 0;\n" );
                Write( "mainGroup = %024llX;\n", makeoid( &projects, EOIDTypeGroup ) );
                Write( "productRefGroup = %024llX /* Products */;\n", makeoid( this, EOIDTypeGroup ) );
                Write( "projectDirPath = \"\";\n" );
                Write( "projectRoot = \"\";\n" );
                Write( "targets = (\n" );
                ++m_nIndent;
                {
                    Write( "%024llX /* All */,\n", makeoid( this, EOIDTypeAggregateTarget ) );
					// sort the projects by name before we emit the list
					CUtlSortVector< CUtlString, CStringLess > vecSortedProjectNames;
					FOR_EACH_VEC( g_vecPGenerators, iGen )
					{
						vecSortedProjectNames.Insert( g_vecPGenerators[iGen]->GetProjectName() );
					}
					FOR_EACH_VEC( vecSortedProjectNames, iProjectName ) 
					{

                    FOR_EACH_VEC( projects, iProject )
                    {
							if ( strcmp( projects[iProject]->m_ProjectName.String(), vecSortedProjectNames[iProjectName] ) )
							{
								continue;
							}
                        Write( "%024llX /* %s */,\n", makeoid( projects[iProject], EOIDTypeNativeTarget ), projects[iProject]->m_ProjectName.String() );
                    }
                }
				}
                --m_nIndent;
                Write( ");\n" );
            }      
            --m_nIndent;
            Write( "};" );
            Write( "\n/* End PBXProject section */\n" );
            
            /** 
             **
             ** container item proxies (no clue, I just work here...) - they sit between projects when expressing dependencies
             **
             **/
            Write( "\n/* Begin PBXContainerItemProxy section */" );
            {
                FOR_EACH_VEC( projects, iProject )
                {

                    // for the aggregate target
                    Write( "\n" );
                    Write( "%024llX /* PBXContainerItemProxy */ = {\n", makeoid( projects[iProject], EOIDTypeContainerItemProxy, -1 ) );
                    ++m_nIndent;
                    {
                        Write( "isa = PBXContainerItemProxy;\n" );
                        // it looks like if you cross ref between xcodeprojs, this is the oid for the other xcode proj
                        Write( "containerPortal = %024llX; /* Project object */\n", makeoid( this, EOIDTypeProject ) );
                        Write( "proxyType = 1;\n" );
                        Write( "remoteGlobalIDString = %024llX;\n", makeoid( projects[iProject], EOIDTypeNativeTarget ) );
                        Write( "remoteInfo = \"%s\";\n", projects[iProject]->m_ProjectName.String() );
                    }
                    --m_nIndent;
                    Write( "};" );          

                    // for each project, figure out what projects it depends on, and spit out a containeritemproxy for that dependency
                    // of particular note is that there are many item proxies for a given project, so we make their oids with the ordinal 
                    // of the project they depend on - this must be consistent within the generated solution
                    CDependency_Project *pCurProject = projects[iProject];
                    
                    CUtlVector<CDependency_Project*> additionalProjectDependencies;
                    ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );
                    
                    for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                    {
                        if ( iProject == iTestProject )
                            continue;
                        
                        CDependency_Project *pTestProject = projects[iTestProject];
                        int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                        if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                        {
                            Write( "\n" );
                            Write( "%024llX /* PBXContainerItemProxy */ = {\n", makeoid( projects[iProject], EOIDTypeContainerItemProxy, (uint16_t)iTestProject ) );
                            ++m_nIndent;
                            {
                                Write( "isa = PBXContainerItemProxy;\n" );
                                // it looks like if you cross ref between xcodeprojs, this is the oid for the other xcode proj
                                Write( "containerPortal = %024llX; /* Project object */\n", makeoid( this, EOIDTypeProject ) );
                                Write( "proxyType = 1;\n" );
                                Write( "remoteGlobalIDString = %024llX;\n", makeoid( projects[iTestProject], EOIDTypeNativeTarget ) );
                                Write( "remoteInfo = \"%s\";\n", projects[iTestProject]->m_ProjectName.String() );
                            }
                            --m_nIndent;
                            Write( "};" );          
                        }
                    }
                }                    
            }
            Write( "\n/* End PBXContainerItemProxy section */\n" );

            /** 
             **
             ** target dependencies - referenced by each project, in turn references the proxy container objects to express dependencies between targets
             **
             **/
            Write( "\n/* Begin PBXTargetDependency section */" );
            FOR_EACH_VEC( projects, iProject )
            {
                Write( "\n" );
                Write( "%024llX /* PBXTargetDependency */ = {\n", makeoid( projects[iProject], EOIDTypeTargetDependency, -1 ) );
                ++m_nIndent;
                {
                    Write( "isa = PBXTargetDependency;\n" );
                    Write( "target = %024llX /* %s */;\n", makeoid( projects[iProject], EOIDTypeNativeTarget ), projects[iProject]->m_ProjectName.String() );
                    Write( "targetProxy = %024llX /* PBXContainerItemProxy */;\n", makeoid( projects[iProject], EOIDTypeContainerItemProxy, -1 ) );
                }
                --m_nIndent;
                Write( "};" );

                CDependency_Project *pCurProject = projects[iProject];
                
                CUtlVector<CDependency_Project*> additionalProjectDependencies;
                ResolveAdditionalProjectDependencies( pCurProject, projects, additionalProjectDependencies );
                
                for ( int iTestProject=0; iTestProject < projects.Count(); iTestProject++ )
                {
                    if ( iProject == iTestProject )
                        continue;
                    
                    CDependency_Project *pTestProject = projects[iTestProject];
                    int dependsOnFlags = k_EDependsOnFlagTraversePastLibs | k_EDependsOnFlagCheckNormalDependencies | k_EDependsOnFlagRecurse;
                    if ( pCurProject->DependsOn( pTestProject, dependsOnFlags ) || additionalProjectDependencies.Find( pTestProject ) != additionalProjectDependencies.InvalidIndex() )
                    {
                        // project_t *pTestProjectT = &g_projects[ pTestProject->m_iProjectIndex ];
                        Write( "\n" );
                        Write( "%024llX /* PBXTargetDependency */ = {\n", makeoid( projects[iProject], EOIDTypeTargetDependency, (uint16_t)iTestProject ) );
                        ++m_nIndent;
                        {
                            Write( "isa = PBXTargetDependency;\n" );
                            Write( "target = %024llX /* %s */;\n", makeoid( projects[iProject], EOIDTypeNativeTarget ), projects[iProject]->m_ProjectName.String() );
                            Write( "targetProxy = %024llX /* PBXContainerItemProxy */;\n", makeoid( projects[iProject], EOIDTypeContainerItemProxy, (uint16_t)iTestProject ) );
                        }
                        --m_nIndent;
                        Write( "};" );
                    }
                }
            }
            --m_nIndent;
            Write( "\n/* End PBXTargetDependency section */\n" );
            
            
            /** 
             **
             ** build configurations - each target (and the project) has a set of build configurations (one release, one debug), each with their own set of build settings 
             ** the "baseConfigurationReference" points back to the appropriate .xcconfig file that gets referenced by the project and has all the non-target specific settings
             **
             **/
            Write( "\n/* Begin XCBuildConfiguration section */" );	
            ++m_nIndent;
            {
                // project and aggregate "all" target
                for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                {
                    bool bIsDebug = V_stristr( k_rgchConfigNames[iConfig], "debug" ) != NULL;
                    
                    Write( "\n" );
                    Write( "%024llX /* %s */ = {\n", makeoid( (uint8_t*)this + (uint64_t)&k_rgOidBuildConfigs[iConfig], EOIDTypeBuildConfiguration ), k_rgchConfigNames[iConfig] );
                    ++m_nIndent;
                    {
                        Write( "isa = XCBuildConfiguration;\n" );
                        Write( "baseConfigurationReference = %024llX /* %s */;\n", makeoid( k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
                        Write( "buildSettings = {\n" );
                        ++m_nIndent;
                        {
                            EmitBuildSettings( "All", NULL, NULL, NULL, bIsDebug );            
                        }
                        --m_nIndent;
                        Write( "};\n" );
                        Write( "name = \"%s\";\n", k_rgchConfigNames[iConfig] );            
                    }
                    --m_nIndent;
                    Write( "};" );

                    Write( "\n" );
                    Write( "%024llX /* %s */ = {\n", makeoid( (uint8_t*)this + (uint64_t)&k_rgOidBuildConfigs[iConfig], EOIDTypeBuildConfiguration, 1 ), k_rgchConfigNames[iConfig] );
                    ++m_nIndent;
                    {
                        Write( "isa = XCBuildConfiguration;\n" );
                        Write( "baseConfigurationReference = %024llX /* %s */;\n", makeoid( k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
                        Write( "buildSettings = {\n" );
                        ++m_nIndent;
                        {
                            EmitBuildSettings( "All", NULL, NULL, NULL, bIsDebug );            
                        }
                        --m_nIndent;
                        Write( "};\n" );
                        Write( "name = \"%s\";\n", k_rgchConfigNames[iConfig] );            
                    }
                    --m_nIndent;
                    Write( "};" );
                }
                
                
                FOR_EACH_VEC( projects, iProject )
                {
                    CFileConfig& pcurrentConfigData = g_vecPGenerators[iProject]->m_BaseConfigData;

                    for ( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                    {
                        if ( pcurrentConfigData.m_Configurations.IsValidIndex( iConfig ) )
                        {
                            bool bIsDebug = V_stristr( k_rgchConfigNames[iConfig], "debug" ) != NULL;

                            Write( "\n" );
                            Write( "%024llX /* %s */ = {\n", makeoid( ( uint8_t* ) projects[iProject] + ( uint64_t ) & k_rgchConfigNames[iConfig], EOIDTypeBuildConfiguration ),
                                    k_rgchConfigNames[iConfig] );
                            ++m_nIndent;
                            {
                                Write( "isa = XCBuildConfiguration;\n" );
                                Write( "baseConfigurationReference = %024llX /* %s */;\n", makeoid( k_rgchXCConfigFiles[iConfig], EOIDTypeFileReference ), k_rgchXCConfigFiles[iConfig] );
                                Write( "buildSettings = {\n" );
                                ++m_nIndent;
                                {
                                    KeyValues *pKV = pcurrentConfigData.m_Configurations[iConfig]->m_pKV;
                                    char rgchProjectDir[MAX_PATH];
                                    V_strncpy( rgchProjectDir, projects[iProject]->m_ProjectFilename.String(), sizeof( rgchProjectDir ) );
                                    V_StripFilename( rgchProjectDir );

                                    EmitBuildSettings( projects[iProject]->m_ProjectName, rgchProjectDir, &( g_vecPGenerators[iProject]->m_Files ), pKV, bIsDebug );
                                }
                                --m_nIndent;
                                Write( "};\n" );
                                Write( "name = \"%s\";\n", k_rgchConfigNames[iConfig] );
                            }
                            --m_nIndent;
                            Write( "};" );
                        }
                    }
                }
            }
            --m_nIndent;
            Write( "\n/* End XCBuildConfiguration section */\n" );	
            
            /** 
             **
             ** configuration lists - aggregates the build configurations above into sets, which are referenced by the individual targets.
             **
             **/
            Write( "\n/* Begin XCConfigurationList section */\n" );
            ++m_nIndent;
            {
                Write( "%024llX /* Build configuration list for PBXProject \"%s\" */ = {\n", makeoid( this, EOIDTypeConfigurationList ), V_UnqualifiedFileName( UsePOSIXSlashes( pSolutionFilename ) ) );
                ++m_nIndent;
                {
                    Write( "isa = XCConfigurationList;\n" );
                    Write( "buildConfigurations = (\n" );
                    ++m_nIndent;
                    for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                    {
                        Write( "%024llX /* %s */,\n", makeoid( (uint8_t*)this + (uint64_t)&k_rgOidBuildConfigs[iConfig], EOIDTypeBuildConfiguration ), k_rgchConfigNames[iConfig] );
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "defaultConfigurationIsVisible = 0;\n" );
                    Write( "defaultConfigurationName = \"%s\";\n", k_rgchConfigNames[0] );                
                }
                --m_nIndent;
                Write( "};\n" );

                Write( "%024llX /* Build configuration list for PBXAggregateTarget \"All\" */ = {\n", makeoid( this, EOIDTypeConfigurationList, 1 ) );
                ++m_nIndent;
                {
                    Write( "isa = XCConfigurationList;\n" );
                    Write( "buildConfigurations = (\n" );
                    ++m_nIndent;
                    for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                    {
                        Write( "%024llX /* %s */,\n", makeoid( (uint8_t*)this + (uint64_t)&k_rgOidBuildConfigs[iConfig], EOIDTypeBuildConfiguration, 1 ), k_rgchConfigNames[iConfig] );
                    }
                    --m_nIndent;
                    Write( ");\n" );
                    Write( "defaultConfigurationIsVisible = 0;\n" );
                    Write( "defaultConfigurationName = \"%s\";\n", k_rgchConfigNames[0] );                
                }
                --m_nIndent;
                Write( "};" );

                FOR_EACH_VEC( projects, iProject )
                {
                    Write( "\n" );
                    Write( "%024llX /* Build configuration list for PBXNativeTarget \"%s\" */ = {\n", makeoid( projects[iProject], EOIDTypeConfigurationList ), projects[iProject]->m_ProjectName.String() );
                    ++m_nIndent;
                    {
                        Write( "isa = XCConfigurationList;\n" );
                        Write( "buildConfigurations = (\n" );
                        ++m_nIndent;
                        for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
                        {
                        	if (g_vecPGenerators[iProject]->m_BaseConfigData.m_Configurations.IsValidIndex(iConfig))
                        	{
                        		Write( "%024llX /* %s */,\n", makeoid( (uint8_t*)projects[iProject] + (uint64_t)&k_rgchConfigNames[iConfig], EOIDTypeBuildConfiguration ), k_rgchConfigNames[iConfig] );
                        	}
                        }
                        --m_nIndent;
                        Write( ");\n" );
                        Write( "defaultConfigurationIsVisible = 0;\n" );
                        Write( "defaultConfigurationName = \"%s\";\n", k_rgchConfigNames[0] );                
                    }
                    --m_nIndent;
                    Write( "};" );
                }            
            }        
            --m_nIndent;
            Write( "\n/* End XCConfigurationList section */\n" );
        }
        Write( "};\n" ); // objects = { ...

        /** 
         **
         ** root object in the graph
         **
         **/
        Write( "rootObject = %024llX /* Project object */;\n", makeoid( this, EOIDTypeProject ) );
    }
    --m_nIndent;
      
    Write( "}\n" );
    fclose( m_fp );
}

void CSolutionGenerator_XCode::GenerateSolutionFileOld( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects )
{
	char sPbxProjFile[MAX_PATH];
	sprintf( sPbxProjFile, "%s.xcodeproj", pSolutionFilename );
	mkdir( sPbxProjFile, 0777 );
	sprintf( sPbxProjFile, "%s.xcodeproj/project.pbxproj", pSolutionFilename );
	m_fp = fopen( sPbxProjFile, "wt" );
	m_nIndent = 0;

	Msg( "\nWriting master XCode project %s.xcodeproj.\n\n", pSolutionFilename );

	/** preamble **/
	Write( "// !$*UTF8*$!\n{\n" );
	++m_nIndent;
	Write( "archiveVersion = 1;\n" );
	Write( "classes = {\n" );
    Write( "};\n" );
	Write( "objectVersion = 42;\n" );
	Write( "objects = {\n" );

	/** file references **/
	Write( "\n/* Begin PBXFileReference section */\n" );
	++m_nIndent;
	FOR_EACH_VEC( projects, iProject )
	{
		char sDir[MAX_PATH];
		V_strncpy( sDir, projects[iProject]->m_ProjectFilename.String(), sizeof( sDir ) );
		V_StripFilename( sDir );

		char sFilename[MAX_PATH];
		V_StripExtension( V_UnqualifiedFileName( projects[iProject]->m_ProjectFilename.String() ), sFilename, sizeof(sFilename) );

		Write( "%024X /* %s */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = \"wrapper.pb-project\"; name = \"%s\"; path = %s/%s.xcodeproj; sourceTree = \"<group>\"; };\n",
			   iProject,
			   sFilename,
			   sFilename,
			   sDir, sFilename
			);
	}
	--m_nIndent;
	Write( "/* End PBXFileReference section */\n" );

	/** groups **/ 
	Write( "\n/* Begin PBXGroup section */\n" );
	++m_nIndent;
	Write( "%024X /* %s */ = {\n", k_pchProjects, k_pchProjects );
	++m_nIndent;
	Write( "isa = PBXGroup;\n" );
	Write( "children = (\n" );
	++m_nIndent;
	FOR_EACH_VEC( projects, iProject )
	{
		char sFilename[MAX_PATH];
		V_StripExtension( V_UnqualifiedFileName( projects[iProject]->m_ProjectFilename.String() ), sFilename, sizeof(sFilename) );

		Write( "%024X /* %s */,\n", iProject, sFilename );
			
	}
	--m_nIndent;
	Write( ");\n" );
	Write( "name = %s;\n", k_pchProjects );
	Write( "sourceTree = \"<group>\";\n" );
	--m_nIndent;
	Write( "};\n" );

	Write( "%024X = {\n", &projects );
	++m_nIndent;
	Write( "isa = PBXGroup;\n" );
	Write( "children = (\n" );
	++m_nIndent;
	Write( "%024X /* %s */,\n", k_pchProjects, k_pchProjects );
	--m_nIndent;
	Write( ");\n" );
	Write( "sourceTree = \"<group>\";\n" );
	--m_nIndent;
	Write( "};" );

	Write( "\n/* End PBXGroup section */\n" );
	
	/** write legacy project section **/
	Write( "\n/* Begin PBXLegacyTarget section */\n" );	
	++m_nIndent;
	Write( "%024X /* build with make */ = {\n", k_pchLegacyTarget );
	++m_nIndent;
	Write( "isa = PBXLegacyTarget;\n");
	Write( "buildArgumentsString = \"-f %s.mak $(ACTION) MAKE_VERBOSE=1\";\n", V_UnqualifiedFileName( pSolutionFilename ) );
	Write( "buildConfigurationList = %024X /* Build configuration list for PBXLegacyTarget \"build with make\" */;\n",  k_oidBuildConfigList );
	Write( "buildPhases = (\n" );
	Write( ");\n" );
	Write( "buildToolPath = /usr/bin/make;\n" );
	char rgchSolutionDir[MAX_PATH];
	V_ExtractFilePath( pSolutionFilename, rgchSolutionDir, sizeof( rgchSolutionDir ) );
	Write( "buildWorkingDirectory = %s;\n", rgchSolutionDir );
	Write( "dependencies = (\n" );
	Write( ");\n");
	Write( "name = \"build with make\";\n" );
	Write( "passBuildSettingsInEnvironment = 1;\n" );
	Write( "productName = \"build with make\";\n" );
	--m_nIndent;
	Write( "};" );
	--m_nIndent;
	Write( "\n/* End PBXLegacyTarget section */\n" );	

	/** project section **/
	Write( "\n/* Begin PBXProject section */\n" );
	Write( "%024X /* Project object */ = {\n", this );
	++m_nIndent;
	Write( "isa = PBXProject;\n" );
	Write( "buildConfigurationList = %024X /* Build configuration list for PBXProject \"%s\" */;\n", k_oidBuildConfigList+1, V_UnqualifiedFileName( pSolutionFilename ) ); 
	Write( "compatibilityVersion = \"Xcode 2.4\";\n" );
	Write( "hasScannedForEncodings = 0;\n" );
	Write( "mainGroup = %024X;\n", &projects );
	Write( "projectDirPath = \"\";\n" );
	Write( "projectRoot = \"\";\n" );
	Write( "targets = (\n" );
	++m_nIndent;
	Write( "%024X /* build with make */, \n", k_pchLegacyTarget );
	--m_nIndent;
	Write( ");\n" );
	--m_nIndent;
	Write( "};" );
	Write( "\n/* End PBXProject section */\n" );


	/** build configurations **/
	++m_nIndent;

	Write( "\n/* Begin XCBuildConfiguration section */" );	
	for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
	{
		Write( "\n" );
		Write( "%024X /* %s */ = {\n", k_rgOidBuildConfigs[iConfig], k_rgchConfigNames[iConfig] );
		++m_nIndent;
		Write( "isa = XCBuildConfiguration;\n" );
		Write( "buildSettings = {\n" );
		++m_nIndent;
		Write( "COPY_PHASE_STRIP = %s;\n", ( V_strstr( k_rgchConfigNames[iConfig], "release" ) ? "YES" : "NO" ) );
		--m_nIndent;
		Write( "};\n" );
		Write( "name = %s;\n", k_rgchConfigNames[iConfig] );
		--m_nIndent;
		Write( "};" );
	}

	for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
	{
		Write( "\n" );
		Write( "%024X /* %s */ = {\n", k_rgOidBuildConfigs[iConfig]+Q_ARRAYSIZE(k_rgOidBuildConfigs)+1, k_rgchConfigNames[iConfig] );
		++m_nIndent;
		Write( "isa = XCBuildConfiguration;\n" );
		Write( "buildSettings = {\n" );
		++m_nIndent;
		Write( "CFG = %s;\n", k_rgchConfigNames[iConfig] );
		Write( "PRODUCT_NAME = \"build with make\";\n" );
		--m_nIndent;
		Write( "};\n" );
		Write( "name = %s;\n", k_rgchConfigNames[iConfig] );
		--m_nIndent;
		Write( "};" );
	}
	Write( "\n/* End XCBuildConfiguration section */\n" );	

	Write( "%024X /* Build configuration list for PBXLegacyTarget \"build with make\" */ = {\n", k_oidBuildConfigList );
	++m_nIndent;
	Write( "isa = XCConfigurationList;\n" );
	Write( "buildConfigurations = (\n" );
	++m_nIndent;
	for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
	{
		Write( "%024X /* %s */,\n", k_rgOidBuildConfigs[iConfig]+Q_ARRAYSIZE(k_rgOidBuildConfigs)+1, k_rgchConfigNames[iConfig] );
	}
	--m_nIndent;
	Write( ");\n" );
	Write( "defaultConfigurationIsVisible = 0;\n" );
	Write( "defaultConfigurationName = %s;\n", k_rgchConfigNames[0] );
	--m_nIndent;
	Write( "};" );

	Write( "\n/* Begin XCBuildConfigurationList section */\n" );	
	Write( "%024X /* Build configuration list for PBXProject \"%s\" */ = {\n", k_oidBuildConfigList+1, V_UnqualifiedFileName( pSolutionFilename ) );
	++m_nIndent;
	Write( "isa = XCConfigurationList;\n" );
	Write( "buildConfigurations = (\n" );
	++m_nIndent;
	for( int iConfig = 0; iConfig < Q_ARRAYSIZE(k_rgchConfigNames); iConfig++ )
	{
		Write( "%024X /* %s */,\n", k_rgOidBuildConfigs[iConfig], k_rgchConfigNames[iConfig] );
	}
	--m_nIndent;
	Write( ");\n" );
	Write( "defaultConfigurationIsVisible = 0;\n" );
	Write( "defaultConfigurationName = %s;\n", k_rgchConfigNames[0] );
	--m_nIndent;
	Write( "};\n" );
	
	Write( "\n/* End XCBuildConfigurationList section */\n" );	

	--m_nIndent;

	/** postamble **/
	--m_nIndent;
	Write( "};\n" );
	Write( "rootObject = %024X /* Project object */;\n", this );
	--m_nIndent;
	Write( "}\n" );

	fclose( m_fp );
}

void CSolutionGenerator_XCode::Write( const char *pMsg, ... )
{
	char sOut[8192];

	va_list marker;
	va_start( marker, pMsg );
	V_vsnprintf( sOut, sizeof( sOut ), pMsg, marker );
	va_end( marker );

	for ( int i=0; i < m_nIndent; i++ )
		fprintf( m_fp, "\t" );

	fprintf( m_fp, "%s", sOut );
}

static CSolutionGenerator_XCode g_SolutionGenerator_XCode;
IBaseSolutionGenerator* GetSolutionGenerator_XCode()
{
	return &g_SolutionGenerator_XCode;
}



