//============ Copyright (c) Valve Corporation, All rights reserved. ==========++;
//
//=============================================================================


// Valve includes
#include "appframework/appframework.h"
#include "appframework/tier3app.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/idatamodel.h"
#include "fbxsystem/ifbxsystem.h"
#include "fbxutils/dmfbxserializer.h"
#include "filesystem.h"
#include "icommandline.h"
#include "mathlib/mathlib.h"
#include "movieobjects/dmeaxissystem.h"
#include "movieobjects/dmefaceset.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmobjserializer.h"
#include "dmserializers/idmserializers.h"
#include "istudiorender.h"
#ifdef SOURCE2
#include "resourcesystem/resourcehandletypes.h"
#endif
#include "tier1/tier1.h"
#include "tier2/tier2.h"
#include "tier2/tier2dm.h"
#include "tier3/tier3.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"


// Last include
#include "tier0/memdbgon.h"



class CStudioDataCache : public CBaseAppSystem < IStudioDataCache >
{
public:
	bool VerifyHeaders( studiohdr_t *pStudioHdr );
	vertexFileHeader_t *CacheVertexData( studiohdr_t *pStudioHdr );
};

static CStudioDataCache	g_StudioDataCache;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CStudioDataCache, IStudioDataCache, STUDIO_DATA_CACHE_INTERFACE_VERSION, g_StudioDataCache );


/*
=================
VerifyHeaders

Minimal presence and header validation, no data loads
Return true if successful, false otherwise.
=================
*/
bool CStudioDataCache::VerifyHeaders( studiohdr_t *pStudioHdr )
{
	// default valid
	return true;
}

/*
=================
CacheVertexData

Cache model's specified dynamic data
=================
*/
vertexFileHeader_t *CStudioDataCache::CacheVertexData( studiohdr_t *pStudioHdr )
{
	// minimal implementation - return persisted data
	return ( vertexFileHeader_t* )pStudioHdr->VertexBase();
}


//-----------------------------------------------------------------------------
//
// Search for a material by name by generating resource names and seeing if
// the resource exists on disk in content and then game and then on user
// specified material search paths
//
// If a material file cannot be found, the original name is copied and false
// is returned
//
//-----------------------------------------------------------------------------
template <size_t maxLenInChars> bool FindMaterialResource( OUT_Z_ARRAY char (&szMaterialResourceName)[maxLenInChars], const char *pszMaterialName, const CUtlVector< CUtlString > &materialSearchPathList )
{
#ifdef SOURCE2
	char szResourceName[MAX_PATH] = { 0 };
	char szResourceFullPath[MAX_PATH] = { 0 };
	char szMaterialPath[MAX_PATH] = { 0 };

	ResourcePathGenerationType_t pSearchPaths[] =
	{
		RESOURCE_PATH_CONTENT,
		RESOURCE_PATH_GAME
	};

	for ( int s = 0; s < ARRAYSIZE( pSearchPaths ); ++s )
	{
		// Check if current material is valid
		FixupResourceName( pszMaterialName, RESOURCE_TYPE_MATERIAL, szResourceName, ARRAYSIZE( szResourceName ) );
		if ( GenerateStandardFullPathForResourceName( szResourceName, pSearchPaths[s], szResourceFullPath, ARRAYSIZE( szResourceFullPath ) ) )
		{
			V_strcpy_safe( szMaterialResourceName, szResourceName );
			return true;
		}
	}

	// Loop through material search paths and try to find the material
	for ( int s = 0; s < ARRAYSIZE( pSearchPaths ); s++ )
	{
		for ( int i = 0; i < materialSearchPathList.Count(); ++i )
		{
			V_ComposeFileName( materialSearchPathList[i].Get(), pszMaterialName, szMaterialPath, ARRAYSIZE( szMaterialPath ) );
			FixupResourceName( szMaterialPath, RESOURCE_TYPE_MATERIAL, szResourceName, ARRAYSIZE( szResourceName ) );

			if ( GenerateStandardFullPathForResourceName( szResourceName, pSearchPaths[s], szResourceFullPath, ARRAYSIZE( szResourceFullPath ) ) )
			{
				V_strcpy_safe( szMaterialResourceName, szResourceName );
				return true;
			}
		}
	}

	V_strcpy_safe( szMaterialResourceName, pszMaterialName );
#endif
	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void RemapMaterials( const CUtlVector< CUtlString > &materialSearchPathList )
{
	char szResourceName[MAX_PATH] = {};
	char szMaterialName[MAX_PATH] = {};

	// Loop through all nodes in the 

	for ( DmElementHandle_t hElement = g_pDataModel->FirstAllocatedElement(); hElement != DMELEMENT_HANDLE_INVALID; hElement = g_pDataModel->NextAllocatedElement( hElement ) )
	{
		CDmeMesh *pDmeMesh = CastElement< CDmeMesh >( g_pDataModel->GetElement( hElement ) );
		if ( !pDmeMesh )
			continue;

		for ( int i = 0; i < pDmeMesh->FaceSetCount(); ++i )
		{
			CDmeFaceSet *pDmeFaceSet = pDmeMesh->GetFaceSet( i );
			CDmeMaterial *pDmeMaterial = pDmeFaceSet->GetMaterial();

			// Check if current material is valid, and just set the standard name found anyway
			if ( FindMaterialResource( szResourceName, pDmeMaterial->GetMaterialName(), materialSearchPathList ) )
			{
				V_FileBase( szResourceName, szMaterialName, ARRAYSIZE( szMaterialName ) );
				pDmeMaterial->SetName( szMaterialName );
				pDmeMaterial->SetMaterial( szResourceName );
				continue;
			}

			{
				bool bFound = false;

				const char *szSuffixes[] = { "_color." };

				FbxString sTmpResourceName = pDmeMaterial->GetMaterialName();

				for ( int i = 0; i < ARRAYSIZE( szSuffixes ); ++i )
				{
					if ( sTmpResourceName.FindAndReplace( szSuffixes[i], "." ) )
					{
						if ( FindMaterialResource( szResourceName, sTmpResourceName.Buffer(), materialSearchPathList ) )
						{
							V_FileBase( szResourceName, szMaterialName, ARRAYSIZE( szMaterialName ) );
							pDmeMaterial->SetName( szMaterialName );
							pDmeMaterial->SetMaterial( szResourceName );
							bFound = 1;
							break;
						}
					}
				}

				if ( bFound )
					continue;
			}

			Warning( "Warning! Cannot find a material resource for material \"%s\" on mesh \"%s\"\n", pDmeMaterial->GetMaterialName(), pDmeMesh->GetName() );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int DoIt( CDmElement *pDmRoot, const char *pszOutFilename, const CUtlVector< CUtlString > &materialSearchPathList )
{
	RemapMaterials( materialSearchPathList );

	CP4AutoAddFile outfile( pszOutFilename );

	char szDmxFilename[MAX_PATH];
	V_strncpy( szDmxFilename, pszOutFilename, ARRAYSIZE( szDmxFilename ) );
	V_SetExtension( szDmxFilename, ".dmx", ARRAYSIZE( szDmxFilename ) );

	CP4AutoEditAddFile dmxFile( szDmxFilename );

	CDmeModel *pDmeModel = pDmRoot->GetValueElement< CDmeModel >( "model" );
	if ( !pDmeModel )
	{
		pDmeModel = pDmRoot->GetValueElement< CDmeModel >( "skeleton" );
	}
	Assert( pDmeModel );
	CDmeAxisSystem *pDmeAxisSystem = pDmeModel->GetValueElement< CDmeAxisSystem >( "axisSystem" );
	Assert( pDmeAxisSystem );

	const bool bReturn = g_pDataModel->SaveToFile( szDmxFilename, NULL, "keyvalues2", "model", pDmRoot );

	g_pDataModel->UnloadFile( pDmRoot->GetFileId() );

	return bReturn ? 0 : -1;
}


bool ProcessAxisSystem( CDmFbxSerializer &dmFbxSerializer );
//-----------------------------------------------------------------------------
// DEFINE_CONSOLE_APPLICATION in Source2
//-----------------------------------------------------------------------------
class CFbx2DmxApp : public CDefaultAppSystemGroup < CSteamAppSystemGroup >
{
	typedef CDefaultAppSystemGroup< CSteamAppSystemGroup > BaseClass;
public:

	CFbx2DmxApp()
	{
		m_pszOptForceMod = NULL;
		m_bOptUFC = false;
		m_nOptVerbosity = 0;
		m_bOptAnimation = false;
		m_bOptPrintSearchPaths = false;
		m_pszOptFilename = NULL;
		m_pszOptOutFilename = NULL;
	}

	virtual bool Create() OVERRIDE;
	virtual int Main() OVERRIDE;

	virtual bool PreInit() OVERRIDE
	{
		CreateInterfaceFn factory = GetFactory();
		ConnectTier1Libraries( &factory, 1 );
		ConnectTier2Libraries( &factory, 1 );

		if ( !g_pFullFileSystem )
			return false;

		if ( !g_pCVar )
			return false;

		ConVar_Register();

		return true;
	}

	virtual void PostShutdown() OVERRIDE
	{
		ConVar_Unregister();
		DisconnectTier2Libraries();
		DisconnectTier1Libraries();
	}

protected:
	const char *m_pszOptFilename;
	int m_nOptVerbosity;
	bool m_bOptAnimation;
	bool m_bOptPrintSearchPaths;
	bool m_bOptUFC;
	const char *m_pszOptForceMod;
	CUtlVector< CUtlString > m_sOptMaterialSearchPaths;
	const char *m_pszOptOutFilename;
protected:
	bool ParseArguments();
};

static bool CStudioMDLApp_SuggestGameInfoDirFn( CFSSteamSetupInfo const *pFsSteamSetupInfo, char *pchPathBuffer, int nBufferLength, bool *pbBubbleDirectories )
{
	const char *pProcessFileName = NULL;
	int nParmCount = CommandLine()->ParmCount();
	if ( nParmCount > 1 )
	{
		pProcessFileName = CommandLine()->GetParm( nParmCount - 1 );
	}

	if ( pProcessFileName )
	{
		Q_MakeAbsolutePath( pchPathBuffer, nBufferLength, pProcessFileName );

		if ( pbBubbleDirectories )
			*pbBubbleDirectories = true;

		return true;
	}

	return false;
}

int main( int argc, char **argv )
{																	
	SetSuggestGameInfoDirFn( CStudioMDLApp_SuggestGameInfoDirFn );
	CFbx2DmxApp s_ApplicationObject;
	CSteamApplication s_SteamApplicationObject( &s_ApplicationObject );
	return AppMain( argc, argv, &s_SteamApplicationObject );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void PrintUsage()
{
	Msg( "\n" );
	Msg( "NAME\n" );
	Msg( "    fbx2dmx - Converts an FBX file to a DMX file\n" );
	Msg( "\n" );
	Msg( "SYNOPSIS\n" );
	Msg( "    fbx2dmx [ opts ... ] < filename.fbx >\n" );
	Msg( "\n" );
	Msg( "    -h | -help . . . . . . . . . . . Prints this information\n" );
	Msg( "    -nop4  . . . . . . . . . . . . . Turns off Perforce integration\n" );
	Msg( "    -i | -input <$>  . . . . . . . . Specifies the input filename\n" );
	Msg( "    -o | -output <$>   . . . . . . . Specifies the output filename\n" );
	Msg( "    -ufc . . . . . . . . . . . . . . _'s in delta names means they are\n"
		 "                                     corrective states\n" );
	Msg( "    -v . . . . . . . . . . . . . . . Each -v increases verbosity\n" );
	Msg( "    -a . . . . . . . . . . . . . . . Convert animation, normally models are\n"
		 "                                     converted\n" );
	Msg( "    -msp | -materialSearchPath <$> . Specify a material search path to remap\n"
		 "                                     materials to\n" );
	Msg( "    -psp . . . . . . . . . . . . . . Print the search paths that will be used\n" );
	Msg( "    -up  . . . . . . . . . . . . . . One of [ x, y, z, -x, -y, -z ],    Def: y\n" );
	Msg( "    -fp | -forwardParity  .  . . . . One of [ even, odd, -even, -odd ], Def: x\n" );
	Msg( "\n" );
	Msg( "DESCRIPTION\n" );
	Msg( "    Converts an FBX file to a DMX file.  File is saved in same location as FBX\n"
		 "    file with extension changed\n" );
	Msg( "\n" );
	Msg( "    If -i isn't specified then any argument that isn't associated with a command\n"
		"     line switch is considered to be the input.\n" );
	Msg( "\n" );
	Msg( "    If -o isn't specified then the extension of the input is changed to be .fbx\n"
		 "    and used as the output filename.\n" );
	Msg( "\n" );
	Msg( "AXIS SYSTEM\n" );
	Msg( "    To specify the axis system the resulting DMX file will contain, the\n"
		 "    combination of -up & -parity is used.  -up specifies which axis is to be\n"
		 "    the up axis, prefix the x, y, or z with a minus to specify the negative\n"
		 "    axis.  -forwardParity specifies the forward axis.  Of the remaining two\n"
		 "    axes left after up is specified, parity specifies which of them,\n"
		 "    alphabetically, will be the forward, even being the first odd being the\n"
		 "    second.  i.e. If Y is up, then even means X, odd means Z.  Again prefix\n"
		 "    with a minus for the negative axis.\n"
		 "\n"
		 "    e.g. To specify Maya Y Up which is +Y up and +Z forward: -up y -fp odd\n"
		 "         To specify Valve Engine which is +Z up and +X forward: -up z -fp even\n" );
	Msg( "\n" );
	Msg( "\n" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ProcessAxisSystem( CDmFbxSerializer &dmFbxSerializer )
{
	// Defaults
	dmFbxSerializer.m_eOptUpAxis = CDmeAxisSystem::AS_AXIS_Y;
	dmFbxSerializer.m_eOptForwardParity = CDmeAxisSystem::AS_PARITY_ODD;

	const int nParmCount = CommandLine()->ParmCount();
	const int nUpIndex = CommandLine()->FindParm( "-up" );

	if ( nUpIndex > 0 )
	{
		if ( nUpIndex < ( nParmCount - 1 ) )	// Ensure there's a parameter value after -up
		{
			const char *pszUp = CommandLine()->GetParm( nUpIndex + 1 );

			if ( pszUp )
			{
				if ( StringHasPrefix( pszUp, "x" ) )
				{
					dmFbxSerializer.m_eOptUpAxis = CDmeAxisSystem::AS_AXIS_X;
				}
				else if ( StringHasPrefix( pszUp, "y" ) )
				{
					dmFbxSerializer.m_eOptUpAxis = CDmeAxisSystem::AS_AXIS_Y;
				}
				else if ( StringHasPrefix( pszUp, "z" ) )
				{
					dmFbxSerializer.m_eOptUpAxis = CDmeAxisSystem::AS_AXIS_Z;
				}
				else if ( StringHasPrefix( pszUp, "-x" ) )
				{
					dmFbxSerializer.m_eOptUpAxis = CDmeAxisSystem::AS_AXIS_NX;
				}
				else if ( StringHasPrefix( pszUp, "-y" ) )
				{
					dmFbxSerializer.m_eOptUpAxis = CDmeAxisSystem::AS_AXIS_NY;
				}
				else if ( StringHasPrefix( pszUp, "-z" ) )
				{
					dmFbxSerializer.m_eOptUpAxis = CDmeAxisSystem::AS_AXIS_NZ;
				}
				else
				{
					Warning( "Error! Invalid -up value specified, must be one of -up [ x, y, z, -x, -y, -z ]: \"%s\"\n", pszUp );
					return false;
				}
			}
			else
			{
				Warning( "Error! No parameter specified after -up, must be one of -up [ x, y, z, -x, -y, -z ]\n" );
				return false;
			}
		}
		else
		{
			Warning( "Error! No parameter specified after -up, must be one of -up [ x, y, z, -x, -y, -z ]\n" );
			return false;
		}
	}

	int nFpIndex = 0;

	const char *szFp[] = { "-fp", "-forwardParity", "-forwardParity" };
	for ( int i = 0; i < ARRAYSIZE( szFp ); ++i )
	{
		const int nTmpFpIndex = CommandLine()->FindParm( szFp[i] );
		if ( nTmpFpIndex > 0 )
		{
			if ( nTmpFpIndex < nParmCount - 1 )
			{
				nFpIndex = nTmpFpIndex;
				break;
			}
			else
			{
				Warning( "Error! No parameter specified after %s, must be one of %s [ even, odd, -even, -odd ]\n", CommandLine()->GetParm( nTmpFpIndex ), CommandLine()->GetParm( nTmpFpIndex ) );
				return false;
			}
		}
	}

	if ( nFpIndex > 0 )
	{
		const char *pszFp = CommandLine()->GetParm( nFpIndex + 1 );

		if ( pszFp )
		{
			if ( StringHasPrefix( pszFp, "e" ) )
			{
				dmFbxSerializer.m_eOptForwardParity = CDmeAxisSystem::AS_PARITY_EVEN;
			}
			else if ( StringHasPrefix( pszFp, "o" ) )
			{
				dmFbxSerializer.m_eOptForwardParity = CDmeAxisSystem::AS_PARITY_ODD;
			}
			else if ( StringHasPrefix( pszFp, "-e" ) )
			{
				dmFbxSerializer.m_eOptForwardParity = CDmeAxisSystem::AS_PARITY_NEVEN;
			}
			else if ( StringHasPrefix( pszFp, "-o" ) )
			{
				dmFbxSerializer.m_eOptForwardParity = CDmeAxisSystem::AS_PARITY_NODD;
			}
			else
			{
				Warning( "Error! Invalid -forwardParity value specified, must be one of [ even, odd, -even, -odd ]: \"%s\"\n", pszFp );
				return false;
			}
		}
		else
		{
			Warning( "Error! No parameter specified after %s, must be one of %s [ even, odd, -even, -odd ]\n", CommandLine()->GetParm( nFpIndex ), CommandLine()->GetParm( nFpIndex ) );
			return false;
		}
	}

	return true;
}


bool CFbx2DmxApp::Create() 
{
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f, false, false, false, false );

	if ( !ParseArguments() )
	{
		return false;
	}

	AppSystemInfo_t appSystems[] =
	{
		{ "vstdlib.dll", PROCESS_UTILS_INTERFACE_VERSION },
		{ "materialsystem.dll", MATERIAL_SYSTEM_INTERFACE_VERSION },
		{ "studiorender.dll", STUDIO_RENDER_INTERFACE_VERSION },
		{ "mdllib.dll", MDLLIB_INTERFACE_VERSION },
		{ "filesystem_stdio.dll", FILESYSTEM_INTERFACE_VERSION },
		{ "p4lib.dll", P4_INTERFACE_VERSION },
		{ "", "" }	// Required to terminate the list
	};

	AddSystem( g_pDataModel, VDATAMODEL_INTERFACE_VERSION );
	AddSystem( g_pDmElementFramework, VDMELEMENTFRAMEWORK_VERSION );
	AddSystem( g_pDmSerializers, DMSERIALIZERS_INTERFACE_VERSION );

	// Add in the locally-defined studio data cache
	AppModule_t	studioDataCacheModule = LoadModule( Sys_GetFactoryThis() );
	AddSystem( studioDataCacheModule, STUDIO_DATA_CACHE_INTERFACE_VERSION );

	// Add the P4 module separately so that if it is absent (say in the SDK) then the other system will initialize properly
	if ( !CommandLine()->FindParm( "-nop4" ) )
	{
		AppModule_t p4Module = LoadModule( "p4lib.dll" );
		AddSystem( p4Module, P4_INTERFACE_VERSION );
	}
	AddSystem( g_pFbx, FBX_INTERFACE_VERSION );

	bool bOk = AddSystems( appSystems );
	if ( !bOk )
		return false;

	IMaterialSystem *pMaterialSystem = ( IMaterialSystem* )FindSystem( MATERIAL_SYSTEM_INTERFACE_VERSION );
	if ( !pMaterialSystem )
		return false;

	pMaterialSystem->SetShaderAPI( "shaderapiempty.dll" );

	return true;
}

int CFbx2DmxApp::Main()
{
	char szOptFilename[ MAX_PATH ];

	// Globally disable DMX undo
	g_pDataModel->SetUndoEnabled( false );

	// This bit of hackery allows us to access files on the harddrive

	if ( m_bOptPrintSearchPaths )
	{
		g_pFullFileSystem->PrintSearchPaths();
	}

	//p4->SetVerbose( false );
	p4->SetOpenFileChangeList( "fbx2dmx" );

	int nRetVal = -1;

	char szExt[ MAX_PATH ] = { };
	V_ExtractFileExtension( szOptFilename, szExt, ARRAYSIZE( szExt ) );

	CDisableUndoScopeGuard noUndo;

	CDmElement *pDmRoot = NULL;

	if ( !V_stricmp( szExt, "obj" ) )
	{
		CDmObjSerializer dmObjSerializer;
		pDmRoot = dmObjSerializer.ReadOBJ( szOptFilename );

		if ( !pDmRoot )
		{
			Warning( "Couldn't load OBJ file: %s\n", szOptFilename );
		}
	}
	else
	{
		CDmFbxSerializer dmFbxSerializer;

		dmFbxSerializer.m_bOptUnderscoreForCorrectors = m_bOptUFC;
		dmFbxSerializer.m_nOptVerbosity = m_nOptVerbosity;
		dmFbxSerializer.m_bAnimation = m_bOptAnimation;
		dmFbxSerializer.m_sOptMaterialSearchPathList = m_sOptMaterialSearchPaths;

		if ( ProcessAxisSystem( dmFbxSerializer ) )
		{
			pDmRoot = dmFbxSerializer.ReadFBX( m_pszOptFilename );

			if ( !pDmRoot )
			{
				Warning( "Couldn't load FBX file: %s\n", szOptFilename );
			}
		}
	}

	if ( pDmRoot )
	{
		if ( m_pszOptOutFilename )
		{
			nRetVal = DoIt( pDmRoot, m_pszOptOutFilename, m_sOptMaterialSearchPaths );
		}
		else
		{
			nRetVal = DoIt( pDmRoot, m_pszOptFilename, m_sOptMaterialSearchPaths );
		}
	}
	return nRetVal;
}

bool CFbx2DmxApp::ParseArguments()
{
	if ( CommandLine()->CheckParm( "-h" ) || CommandLine()->CheckParm( "--help" ) || CommandLine()->CheckParm( "-help" ) )
	{
		PrintUsage();
		return false;
	}

	for ( int i = 1; i < CommandLine()->ParmCount(); ++i )
	{
		const char *pszParam = CommandLine()->GetParm( i );

		if ( !V_stricmp( pszParam, "-game" ) )
		{
			m_pszOptForceMod = CommandLine()->GetParm( ++i );
			continue;
		}

		if ( !V_stricmp( pszParam, "-nop4" ) )
		{
			continue;
		}

		if ( !V_stricmp( pszParam, "-ufc" ) )
		{
			m_bOptUFC = true;
			continue;
		}

		if ( !V_stricmp( pszParam, "-v" ) )
		{
			++m_nOptVerbosity;
			continue;
		}

		if ( !V_stricmp( pszParam, "-a" ) )
		{
			m_bOptAnimation = true;
			continue;
		}

		if ( !V_stricmp( pszParam, "-msp" ) || !V_stricmp( pszParam, "-materialSearchPath" ) )
		{
			m_sOptMaterialSearchPaths.AddToTail( CommandLine()->GetParm( ++i ) );
			continue;
		}

		if ( !V_stricmp( pszParam, "-psp" ) )
		{
			m_bOptPrintSearchPaths = true;
			continue;
		}

		if ( !V_stricmp( pszParam, "-up" ) )
		{
			++i;
			continue;
		}

		if ( !V_stricmp( pszParam, "-fp" ) || !V_stricmp( pszParam, "-forwardParity" ) || !V_stricmp( pszParam, "-forwardparity" ) )
		{
			++i;
			continue;
		}

		if ( !V_stricmp( pszParam, "-i" ) || !V_stricmp( pszParam, "-input" ) )
		{
			++i;
			pszParam = CommandLine()->GetParm( i );
			if ( m_pszOptFilename )
			{
				Warning( "Warning! Filename was already specified as: \"%s\", using -i \"%s\"\n", m_pszOptFilename, pszParam );
			}

			m_pszOptFilename = pszParam;
			continue;
		}

		if ( !V_stricmp( pszParam, "-o" ) || !V_stricmp( pszParam, "-output" ) )
		{
			++i;
			pszParam = CommandLine()->GetParm( i );
			if ( m_pszOptOutFilename )
			{
				Warning( "Warning! Output filename was already specified as: \"%s\", using -o \"%s\"\n", m_pszOptOutFilename, pszParam );
			}

			m_pszOptOutFilename = pszParam;
			continue;
		}

		if ( StringHasPrefix( pszParam, "-" ) )
		{
			Warning( "Warning! Unknown command line switch \"%s\"\n", pszParam );
			continue;
		}

		if ( m_pszOptFilename )
		{
			Warning( "Warning! Filename already specified: \"%s\", ignoring \"%s\"\n", m_pszOptFilename, pszParam );
		}
		else
		{
			m_pszOptFilename = pszParam;
		}
	}

	if ( !m_pszOptFilename )
	{
		Warning( "Error! Cannot find any file to execute from passed command line arguments\n\n" );
		PrintUsage();
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: bind studiohdr_t support functions to fbx2dmx utility
// FIXME: This should be moved into studio.cpp?
//-----------------------------------------------------------------------------
const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const
{
	MDLHandle_t handle = g_pMDLCache->FindMDL( pModelName );
	*cache = ( void* )( uintp )handle;
	return g_pMDLCache->GetStudioHdr( handle );
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	if ( numincludemodels == 0 )
		return NULL;
	return g_pMDLCache->GetVirtualModelFast( this, VoidPtrToMDLHandle( VirtualModel() ) );
}

byte *studiohdr_t::GetAnimBlock( int i, bool preloadIfMissing ) const
{
	return g_pMDLCache->GetAnimBlock( VoidPtrToMDLHandle( VirtualModel() ), i, preloadIfMissing );
}

bool studiohdr_t::hasAnimBlockBeenPreloaded( int i ) const
{
	return g_pMDLCache->HasAnimBlockBeenPreloaded( VoidPtrToMDLHandle( VirtualModel() ), i );
}

int	studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return g_pMDLCache->GetAutoplayList( VoidPtrToMDLHandle( VirtualModel() ), pOut );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	return g_pMDLCache->GetStudioHdr( VoidPtrToMDLHandle( cache ) );
}
