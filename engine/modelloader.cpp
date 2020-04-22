//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Model loading / unloading interface
//
// $NoKeywords: $
//===========================================================================//

#include "render_pch.h"
#include "common.h"
#include "modelloader.h"
#include "sysexternal.h"
#include "cmd.h"
#include "istudiorender.h"
#include "draw.h"
#include "zone.h"
#include "edict.h"
#include "cmodel_engine.h"
#include "cdll_engine_int.h"
#include "iscratchpad3d.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/materialsystem_config.h"
#include "gl_rsurf.h"
#include "avi/iavi.h"
#include "avi/ibik.h"
#include "materialsystem/itexture.h"
#include "Overlay.h"
#include "utldict.h"
#include "mempool.h"
#include "r_decal.h"
#include "l_studio.h"
#include "optimize.h"
#include "gl_drawlights.h"
#include "tier0/icommandline.h"
#include "MapReslistGenerator.h"
#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#endif
#include "engine/ivmodelrender.h"
#include "host.h"
#include "datacache/idatacache.h"
#include "sys_dll.h"
#include "datacache/imdlcache.h"
#include "gl_cvars.h"
#include "vphysics_interface.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "lightcache.h"
#include "lumpfiles.h"
#include "tier2/fileutils.h"
#include "../utils/common/bsplib.h"
#include "ibsppack.h"
#include "utlsortvector.h"
#include "utlhashtable.h"
#include "UtlStringMap.h"
#include "callqueue.h"
#include "color.h"
#include "tier1/lzmaDecoder.h"
#include "eiface.h"
#include "server.h"
#include "ifilelist.h"
#include "LoadScreenUpdate.h"
#if defined( _X360 )
#include "xbox/xbox_console.h"
#elif defined( _PS3 )
#include "ps3/ps3_console.h"
#endif
#include "materialsystem/imesh.h"
#include "networkstringtable.h"
#include "fmtstr.h"
#include "engine_model_client.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Uncomment this line to break down Map_LoadModel into 
// smaller, individual scopes. 
// #define MEM_DETAILED_ACCOUNTING_MAP_LOADMODEL

ConVar mat_loadtextures( "mat_loadtextures", "1", FCVAR_CHEAT );
static ConVar mod_touchalldata( "mod_touchalldata", "1", 0, "Touch model data during level startup" );
static ConVar mod_forcetouchdata( "mod_forcetouchdata", "1", 0, "Forces all model file data into cache on model load." );
ConVar mat_excludetextures( "mat_excludetextures", "0", 0 );

ConVar r_unloadlightmaps( "r_unloadlightmaps", "0" );
ConVar r_hunkalloclightmaps( "r_hunkalloclightmaps", "1" );

// Not compatible for PC (due to ALT+TAB req's), mutually exclusive and similar to "unloadlightmaps", but keeps only
// the styled lightmaps for animated light updates and discards the static portion of the lightmaps
// (after lightmap page setup), so dlight support is severed when this is enabled.
ConVar r_keepstyledlightmapsonly( "r_keepstyledlightmapsonly", IsGameConsole() ? "1" : "0" ); 

// keep this many weapon view models resident, LRU purge others
// clamped to minimum player inventory to prevent LRU and needing to 
ConVar mod_weaponviewmodelcache( "mod_WeaponViewModelCache", "8", 0, "", true, 0, false, 0 );
// keep this many weapon world models resident, LRU purge others
ConVar mod_weaponworldmodelcache( "mod_WeaponWorldModelCache", "10", 0, "", true, 0, false, 0 );
ConVar mod_weaponworldmodelminage( "mod_WeaponWorldModelMinAge", "3000", 0, "", true, 0, false, 0 );

#if !defined( DEDICATED )
extern ConVar r_lightcache_zbuffercache;
#endif

bool g_bHunkAllocLightmaps;
bool g_bClearingClientState = false;

extern	CGlobalVars g_ServerGlobalVariables;
extern	IMaterial	*g_materialEmpty;
extern	ConVar		r_rootlod;

model_t *g_pSimpleWorldModel = NULL;
model_t *g_pSimpleWorldModelWater = NULL;

bool g_bLoadedMapHasBakedPropLighting = false;
bool g_bBakedPropLightingNoSeparateHDR = false;  // Some maps only have HDR lighting on props, contained in the file for non-hdr light data
bool g_bHasLightmapAlphaData = false;
bool g_bBakedPropLightingStreams3 = false;
bool g_bHasLightmapAlphaData3 = false;		// newer alpha data for CSM blending
bool g_bHasIndirectOnlyInLightingStreams = false;
bool g_bLightstylesWithCSM = false;

double g_flAccumulatedModelLoadTime;
double g_flAccumulatedModelLoadTimeStudio;
double g_flAccumulatedModelLoadTimeStaticMesh;
double g_flAccumulatedModelLoadTimeBrush;
double g_flAccumulatedModelLoadTimeSprite;
double g_flAccumulatedModelLoadTimeVCollideSync;
double g_flAccumulatedModelLoadTimeVCollideAsync;
double g_flAccumulatedModelLoadTimeVirtualModel;
double g_flAccumulatedModelLoadTimeMaterialNamesOnly;

static ConVar mod_dynamicunloadtime( "mod_dynamicunloadtime", "150", FCVAR_HIDDEN | FCVAR_DONTRECORD );
static ConVar mod_dynamicunloadtextures( "mod_dynamicunloadtex", "1", FCVAR_HIDDEN | FCVAR_DONTRECORD );
static ConVar mod_dynamicloadpause( "mod_dynamicloadpause", "0", FCVAR_CHEAT | FCVAR_HIDDEN | FCVAR_DONTRECORD );
static ConVar mod_dynamicloadthrottle( "mod_dynamicloadthrottle", "0", FCVAR_CHEAT | FCVAR_HIDDEN | FCVAR_DONTRECORD );
static ConVar mod_dynamicloadspew( "mod_dynamicloadspew", "0", FCVAR_HIDDEN | FCVAR_DONTRECORD );
#define DynamicModelDebugMsg(...) ( mod_dynamicloadspew.GetBool() ? Msg(__VA_ARGS__) : (void)0 )

//-----------------------------------------------------------------------------
// A dictionary used to store where to find game lump data in the .bsp file
//-----------------------------------------------------------------------------

// Extended from the on-disk struct to include uncompressed size and stop propagation of bogus signed values
struct dgamelump_internal_t
{
	dgamelump_internal_t( dgamelump_t &other, unsigned int nCompressedSize )
		: id( other.id )
		, flags( other.flags )
		, version( other.version )
		, offset( Max( other.fileofs, 0 ) )
		, uncompressedSize( Max( other.filelen, 0 ) )
		, compressedSize( nCompressedSize )
		{}
	GameLumpId_t	id;
	unsigned short	flags;
	unsigned short	version;
	unsigned int	offset;
	unsigned int	uncompressedSize;
	unsigned int	compressedSize;
};

static CUtlVector< dgamelump_internal_t > g_GameLumpDict;
static char g_GameLumpFilename[MAX_PATH];

//-----------------------------------------------------------------------------

void Con_ColorPrintf( const Color& clr, const char *fmt, ... );

void NotifyHunkBeginMapLoad( const char *pszMapName )
{
	// Set the estimated hunk size. For maps where there's versus versions, using the larger of the two
	struct EstimatedHunkSize_t
	{
		const char *pszMapRoot;
		int nBytes;
	};

	// These hunk sizes are used to set the initial commit amount.
	// They are an optimization and they don't need to be perfect. Setting
	// them a little bit low ensures that there is no wasted commit.
	static EstimatedHunkSize_t EstimatedHunkSizes[] =
	{
		// TODO: if Portal 2 map hunk sizes end up being highly variable, add entries here for maps
		//       requiring > HUNK_COMMIT_FLOOR (defined in zone.cpp), to avoid fragmentation issues
		//{ "hospital01", 9568256 },
		{ NULL, 1024*1024 },
	};

	for ( int i = 0; i < ARRAYSIZE(EstimatedHunkSizes); i++ )
	{
		if ( !EstimatedHunkSizes[i].pszMapRoot || V_stristr( pszMapName, EstimatedHunkSizes[i].pszMapRoot ) )
		{
			Hunk_OnMapStart( EstimatedHunkSizes[i].nBytes );
			break;
		}
	}
}

//-----------------------------------------------------------------------------


// FIXME/TODO:  Right now Host_FreeToLowMark unloads all models including studio
//  models that have Cache_Alloc data, too.  This needs to be fixed before shipping

BEGIN_BYTESWAP_DATADESC( lump_t )
	DEFINE_FIELD( fileofs, FIELD_INTEGER ),
	DEFINE_FIELD( filelen, FIELD_INTEGER ),
	DEFINE_FIELD( version, FIELD_INTEGER ),
	DEFINE_ARRAY( fourCC, FIELD_CHARACTER, 4 ),
END_BYTESWAP_DATADESC()

BEGIN_BYTESWAP_DATADESC( BSPHeader_t )
	DEFINE_FIELD( ident, FIELD_INTEGER ),
	DEFINE_FIELD( m_nVersion, FIELD_INTEGER ),
	DEFINE_EMBEDDED_ARRAY( lumps, HEADER_LUMPS ),
	DEFINE_FIELD( mapRevision, FIELD_INTEGER ),
END_BYTESWAP_DATADESC()

bool Model_LessFunc( FileNameHandle_t const &a, FileNameHandle_t const &b )
{
	return a < b;
}

struct ViewWeaponEntry_t
{
	ViewWeaponEntry_t( bool bIsViewModel )
	{
		m_nAgeTime = 0;
		m_bStudioHWDataResident = false;
		m_bViewModel = bIsViewModel;

		m_hAsyncVTXControl = NULL;
		m_hAsyncVVDControl = NULL;
	}

	CUtlVector< CUtlString >	m_Materials;
	unsigned int				m_nAgeTime;
	bool						m_bStudioHWDataResident;
	bool						m_bViewModel;

	FSAsyncControl_t			m_hAsyncVTXControl;
	FSAsyncControl_t			m_hAsyncVVDControl;
};

//-----------------------------------------------------------------------------
// Purpose: Implements IModelLoader
//-----------------------------------------------------------------------------
class CModelLoader : public IModelLoader
{
	friend class CMDLCacheNotify;

// Implement IModelLoader interface
public:
	CModelLoader() :
		m_ModelPool( sizeof( model_t ), MAX_KNOWN_MODELS, CUtlMemoryPool::GROW_FAST, "CModelLoader::m_ModelPool" ),
		m_Models( 0, 0, Model_LessFunc ),
		m_WeaponModelCache( 0, 0, DefLessFunc( model_t* ) )
	{
	}

	void			Init( void );
	void			Shutdown( void );

	int				GetCount( void );
	model_t			*GetModelForIndex( int i );

	// Look up name for model
	const char		*GetName( model_t const *model );

	// Check cache for data, reload model if needed
	void			*GetExtraData( model_t *model );

	int				GetModelFileSize( char const *name );

	// Finds the model, and loads it if it isn't already present.  Updates reference flags
	model_t			*GetModelForName( const char *name, REFERENCETYPE referencetype );
	// Mark as referenced by name
	model_t			*ReferenceModel( const char *name, REFERENCETYPE referencetype );

	// Unmasks the referencetype field for the model
	void			UnreferenceModel( model_t *model, REFERENCETYPE referencetype );
	// Unmasks the specified reference type across all models
	void			UnreferenceAllModels( REFERENCETYPE referencetype );

	// For any models with referencetype blank, frees all memory associated with the model
	//  and frees up the models slot
	void			UnloadUnreferencedModels( void );
	void			PurgeUnusedModels( void );

	void			UnMountCompatibilityPaths( void );
	void			AddCompatibilityPath( const char *szNewCompatibilityPath );

	bool			Map_GetRenderInfoAllocated( void );
	void			Map_SetRenderInfoAllocated( bool allocated );

	virtual void	Map_LoadDisplacements( model_t *pModel, bool bRestoring );

	// Validate version/header of a .bsp file
	bool			Map_IsValid( char const *mapname, bool bQuiet = false );

	virtual void	RecomputeSurfaceFlags( model_t *mod );

	virtual void	Studio_ReloadModels( ReloadType_t reloadType );

	void			Print( void );

	// Is a model loaded?
	virtual bool	IsLoaded( const model_t *mod );

	virtual bool	LastLoadedMapHasHDRLighting(void);
	virtual bool	LastLoadedMapHasLightmapAlphaData( void );
	
	void			DumpVCollideStats();

	// Returns the map model, otherwise NULL, no load or create
	model_t			*FindModelNoCreate( const char *pModelName );

	// Finds the model, builds a model entry if not present
	model_t			*FindModel( const char *name );

	modtype_t		GetTypeFromName( const char *pModelName );

	// start with -1, list terminates with -1
	int				FindNext( int iIndex, model_t **ppModel );

	void			UnloadModel( model_t *pModel );

	virtual void	ReloadFilesInList( IFileList *pFilesToReload );


	// Dynamic model loading
	virtual void	UpdateDynamicModels() { InternalUpdateDynamicModels(false); }
	virtual void	FlushDynamicModels() { InternalUpdateDynamicModels(true); }
	virtual void	ForceUnloadNonClientDynamicModels();
	virtual model_t *GetDynamicModel( const char *name, bool bClientOnly );
	virtual model_t	*GetDynamicCombinedModel( const char *name, bool bClientOnly );
	virtual void	UpdateDynamicCombinedModel( model_t *pModel, MDLHandle_t Handle, bool bClientSide );
	virtual bool	SetCombineModels( model_t* pModel, const CUtlVector< SCombinerModelInput_t > &vecModelsToCombine );
	virtual bool	FinishCombinedModel( model_t *pModel, CombinedModelLoadedCallback pFunc, void *pUserData );
	virtual bool	IsDynamicModelLoading( model_t *pModel );
	virtual void	AddRefDynamicModel( model_t *pModel, bool bClientSideRef );
	virtual void	ReleaseDynamicModel( model_t *pModel, bool bClientSideRef );
	virtual bool	RegisterModelLoadCallback( model_t *pModel, IModelLoadCallback *pCallback, bool bCallImmediatelyIfLoaded );
	virtual void	UnregisterModelLoadCallback( model_t *pModel, IModelLoadCallback *pCallback );
	virtual void	Client_OnServerModelStateChanged( model_t *pModel, bool bServerLoaded );
	void			DebugPrintDynamicModels();
	void			DebugCombinerInfo( );


	byte			*GetLightstyles( model_t *pModel );
	void			AllocateLightstyles( model_t *pModel, byte *pStyles, int nStyleCount );

	virtual void	UpdateViewWeaponModelCache( const char **ppWeaponModels, int nWeaponModels );
	virtual void	TouchWorldWeaponModelCache( const char **ppWeaponModels, int nWeaponModels );
	void			DumpWeaponModelCache( bool bViewModelsOnly );
	virtual bool	IsModelInWeaponCache( const model_t *pModel );
	virtual void	EvictAllWeaponsFromModelCache( bool bLoadingComplete );
	virtual bool	IsViewWeaponModelResident( const model_t *pModel );
	bool			ProcessWeaponModelCacheOperations();

// Internal types
private:
	// TODO, flag these and allow for UnloadUnreferencedModels to check for allocation type
	//  so we don't have to flush all of the studio models when we free the hunk
	enum
	{
		FALLOC_USESHUNKALLOC = (1<<31),
		FALLOC_USESCACHEALLOC = (1<<30),
	};

// Internal methods
private:
	// Set reference flags and load model if it's not present already
	model_t		*LoadModel( model_t	*model, REFERENCETYPE *referencetype );
	// Unload models ( won't unload referenced models if checkreferences is true )
	void		UnloadAllModels( bool checkreference );
	void		SetupSubModels( model_t	*model, CUtlVector<mmodel_t> &list );

	// World/map
	void		Map_LoadModel( model_t *mod );
	void		Map_LoadModelGuts( model_t *mod );
	void		Map_UnloadModel( model_t *mod );
	void		Map_UnloadCubemapSamples( model_t *mod );
	void		Map_UnloadSimpleWorldModel( model_t *mod );

	// World loading helper
	void		SetWorldModel( model_t *mod );
	void		ClearWorldModel( void );
	bool		IsWorldModelSet( void );
	int			GetNumWorldSubmodels( void );

	// Sprites
	void		Sprite_LoadModel( model_t *mod );
	void		Sprite_UnloadModel( model_t *mod );

	// Studio models
	void		Studio_LoadModel( model_t *mod, bool bTouchAllData );
	void		Studio_UnloadModel( model_t *mod );

	// Byteswap
	int			UpdateOrCreate( const char *pSourceName, char *pTargetName, int maxLen, bool bForce );

	// Dynamic model state
	void		InternalUpdateDynamicModels( bool bForceFlushUnreferenced );

	// Dynamic load queue
	class CDynamicModelInfo;
	void		QueueDynamicModelLoad( CDynamicModelInfo *dyn, model_t *mod );
	bool		CancelDynamicModelLoad( CDynamicModelInfo *dyn, model_t *mod );
	void		UpdateDynamicModelLoadQueue();
	void		FinishDynamicModelLoadIfReady( CDynamicModelInfo *dyn, model_t *mod );

	void		EvictWeaponModel( int CacheIndex, bool bForce );
	void		RestoreWeaponModel( int CacheIndex );
	
	// Internal data
private:
	enum 
	{
		MAX_KNOWN_MODELS = 1024,
	};

	struct ModelEntry_t
	{
		model_t *modelpointer;
	};

	CUtlMap< FileNameHandle_t, ModelEntry_t >	m_Models;

	CUtlMemoryPool		m_ModelPool;

	CUtlVector<model_t>	m_InlineModels;

	model_t				*m_pWorldModel;

public: // HACKHACK
	worldbrushdata_t	m_worldBrushData;

private:
	// local base name of current loading model
	// generally used for debugging spew where only the name is desired, not the disk path
	char				m_szBaseName[64];

	bool				m_bMapRenderInfoLoaded;
	bool				m_bMapHasHDRLighting;

	// Dynamic model support:
	class CDynamicModelInfo
	{
	public:
		enum 
		{ 
			QUEUED			= 0x01, 
			LOADING			= 0x02, 
			LOCKED			= 0x04, 
			SERVERLOADING	= 0x08, 
			READY			= 0x10, 
			INVALIDFLAG		= 0x20,
			COMBINED		= 0x40,
		}; // flags
		CDynamicModelInfo() : m_iRefCount(0), m_iClientRefCount(0), m_nLoadFlags(INVALIDFLAG), m_uLastTouchedMS_Div256(0) { }
		int16 m_iRefCount;
		int16 m_iClientRefCount; // also doublecounted in m_iRefCount
		uint32 m_nLoadFlags : 8;
		uint32 m_uLastTouchedMS_Div256 : 24;
		CUtlVector< IModelLoadCallback * > m_Callbacks;
	};
	CUtlHashtable< model_t * , CDynamicModelInfo > m_DynamicModels;
	CUtlHashtable< IModelLoadCallback * , int > m_RegisteredDynamicCallbacks;

	// Dynamic model load queue
	CUtlVector< model_t* > m_DynamicModelLoadQueue;
	bool m_bDynamicLoadQueueHeadActive;

	CUtlVector<byte>	m_LightStyleList;

	CMemoryStack		m_WorldLightingDataStack;

	bool										m_bAllowWeaponModelCache;
	bool										m_bAllowWeaponVertexEviction;
	bool										m_bAllowWorldWeaponEviction;
	CUtlMap< model_t *, ViewWeaponEntry_t * >	m_WeaponModelCache;
	int											m_nNumWeaponsPartialResident;

	struct compatibility_path_t
	{
		CUtlString mPath;
		CUtlString mPathId;
	};
	CUtlVector< compatibility_path_t >	m_vecSzCompatibilityPaths;

};

// Expose interface
static CModelLoader g_ModelLoader;
IModelLoader *modelloader = ( IModelLoader * )&g_ModelLoader;

//-----------------------------------------------------------------------------
// Globals used by the CMapLoadHelper
//-----------------------------------------------------------------------------
static BSPHeader_t		s_MapHeader;
static FileHandle_t		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
static char				s_szMapPathName[MAX_PATH];
static char				s_szMapPathNameOnDisk[MAX_PATH];
static worldbrushdata_t	*s_pMap = NULL;
static int				s_nMapLoadRecursion = 0;
CMemoryStack			s_MapBuffer;

// Lump files are patches for a shipped map
// List of lump files found when map was loaded. Each entry is the lump file index for that lump id.
struct lumpfiles_t
{
	FileHandle_t		file;
	int					lumpfileindex;
	lumpfileheader_t	header;
};
static lumpfiles_t s_MapLumpFiles[ HEADER_LUMPS ];

CON_COMMAND( mem_vcollide, "Dumps the memory used by vcollides" )
{
	g_ModelLoader.DumpVCollideStats();
}

CON_COMMAND( mod_DumpWeaponWiewModelCache, "Dumps the weapon view model cache contents" )
{
	g_ModelLoader.DumpWeaponModelCache( true );
}

CON_COMMAND( mod_DumpWeaponWorldModelCache, "Dumps the weapon world model cache contents" )
{
	g_ModelLoader.DumpWeaponModelCache( false );
}

//-----------------------------------------------------------------------------
// Get the map name with the appropriate platform extension. Used to hide
// the expected platform suffix, which is desired to be private. The code
// on all platforms generally expects the PC name and only changes it before i/o.
//-----------------------------------------------------------------------------
char *GetMapPathNameOnDisk( char *pDiskName, const char *pFullMapName, unsigned int nDiskNameSize )
{
	if ( !IsGameConsole() )
	{
		// pc names are as is
		if ( pFullMapName != pDiskName )
		{
			V_strncpy( pDiskName, pFullMapName, nDiskNameSize );
		}
	}
	else
	{
		// expecting the input name to be maps/foo.bsp
		V_StripExtension( pFullMapName, pDiskName, nDiskNameSize );
		V_strncat( pDiskName, PLATFORM_EXT ".bsp", nDiskNameSize );
	}
	return pDiskName;
}

//-----------------------------------------------------------------------------
// Returns the ref count for this bsp
//-----------------------------------------------------------------------------
int CMapLoadHelper::GetRefCount()
{
	return s_nMapLoadRecursion;
}

//-----------------------------------------------------------------------------
// Setup a BSP loading context, maintains a ref count.	
//-----------------------------------------------------------------------------
void CMapLoadHelper::Init( model_t *pMapModel, const char *pPathName )
{
	if ( ++s_nMapLoadRecursion > 1 )
	{
		return;
	}

	s_pMap = NULL;
	s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
	V_memset( &s_MapHeader, 0, sizeof( s_MapHeader ) );
	V_memset( &s_MapLumpFiles, 0, sizeof( s_MapLumpFiles ) );

	if ( !pMapModel )
	{
		Q_strncpy( s_szMapPathName, pPathName, sizeof( s_szMapPathName ) );
	}
	else
	{
		Q_strncpy( s_szMapPathName, pMapModel->szPathName, sizeof( s_szMapPathName ) );
	}

	char szNameOnDisk[MAX_PATH];
	GetMapPathNameOnDisk( szNameOnDisk, s_szMapPathName, sizeof( szNameOnDisk ) );
	s_MapFileHandle = g_pFileSystem->OpenEx( szNameOnDisk, "rb", IsGameConsole() ? FSOPEN_NEVERINPACK : 0, IsGameConsole() ? "GAME" : NULL );
	if ( s_MapFileHandle == FILESYSTEM_INVALID_HANDLE )
	{
		if ( !g_bClearingClientState )
		{
			Host_Error( "CMapLoadHelper::Init, unable to open %s\n", szNameOnDisk );
		}
		return;
	}

	g_pFileSystem->Read( &s_MapHeader, sizeof( BSPHeader_t ), s_MapFileHandle );
	if ( s_MapHeader.ident != IDBSPHEADER )
	{
		g_pFileSystem->Close( s_MapFileHandle );
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
		Host_Error( "CMapLoadHelper::Init, map %s has wrong identifier\n", szNameOnDisk );
		return;
	}

	if ( s_MapHeader.m_nVersion < MINBSPVERSION || s_MapHeader.m_nVersion > BSPVERSION )
	{
		g_pFileSystem->Close( s_MapFileHandle );
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
		Host_Error( "CMapLoadHelper::Init, map %s has wrong version (%i when expecting %i)\n", szNameOnDisk,
			s_MapHeader.m_nVersion, BSPVERSION );
		return;
	}

	// Store map version, but only do it once so that the communication between the engine and Hammer isn't broken. The map version
	// is incremented whenever a Hammer to Engine session is established so resetting the global map version each time causes a problem.
	if ( 0 == g_ServerGlobalVariables.mapversion )
	{
		g_ServerGlobalVariables.mapversion = s_MapHeader.mapRevision;
	}

#ifndef DEDICATED
	InitDLightGlobals( s_MapHeader.m_nVersion );
#endif

	s_pMap = &g_ModelLoader.m_worldBrushData;

	if ( IsPC() )
	{
		// Now find and open our lump files, and create the master list of them.
		for ( int iIndex = 0; iIndex < MAX_LUMPFILES; iIndex++ )
		{
			lumpfileheader_t lumpHeader;
			char lumpfilename[MAX_PATH];

			GenerateLumpFileName( s_szMapPathName, lumpfilename, MAX_PATH, iIndex );
			if ( !g_pFileSystem->FileExists( lumpfilename ) )
				break;

			// Open the lump file
			FileHandle_t lumpFile = g_pFileSystem->Open( lumpfilename, "rb" );
			if ( lumpFile == FILESYSTEM_INVALID_HANDLE )
			{
				Host_Error( "CMapLoadHelper::Init, failed to load lump file %s\n", lumpfilename );
				return;
			}

			// Read the lump header
			memset( &lumpHeader, 0, sizeof( lumpHeader ) );
			g_pFileSystem->Read( &lumpHeader, sizeof( lumpfileheader_t ), lumpFile );

			if ( lumpHeader.lumpID >= 0 && lumpHeader.lumpID < HEADER_LUMPS )
			{
				// We may find multiple lump files for the same lump ID. If so,
				// close the earlier lump file, because the later one overwrites it.
				if ( s_MapLumpFiles[lumpHeader.lumpID].file != FILESYSTEM_INVALID_HANDLE )
				{
					g_pFileSystem->Close( s_MapLumpFiles[lumpHeader.lumpID].file );
				}

				s_MapLumpFiles[lumpHeader.lumpID].file = lumpFile;
				s_MapLumpFiles[lumpHeader.lumpID].lumpfileindex = iIndex;
				memcpy( &(s_MapLumpFiles[lumpHeader.lumpID].header), &lumpHeader, sizeof(lumpHeader) );
			}
			else
			{
				Warning( "Found invalid lump file '%s'. Lump Id: %d\n", lumpfilename, lumpHeader.lumpID );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Setup a BSP loading context from a supplied buffer
//-----------------------------------------------------------------------------
void CMapLoadHelper::InitFromMemory( model_t *pMapModel, const void *pData, int nDataSize )
{
	// valid for consoles only 
	// consoles have reorganized bsp format and no external lump files
	Assert( IsGameConsole() && pData && nDataSize );

	// the memory should be contained in s_MapBuffer
	Assert( ( pData == s_MapBuffer.GetBase() ) && ( nDataSize == s_MapBuffer.GetUsed() ) );

	if ( ++s_nMapLoadRecursion > 1 )
	{
		return;
	}

	s_pMap = NULL;
	s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
	V_memset( &s_MapHeader, 0, sizeof( s_MapHeader ) );
	V_memset( &s_MapLumpFiles, 0, sizeof( s_MapLumpFiles ) );

	// mimic the expected globals, as if loading from disk
	V_strncpy( s_szMapPathName, pMapModel->szPathName, sizeof( s_szMapPathName ) );

	char szNameOnDisk[MAX_PATH];
	GetMapPathNameOnDisk( szNameOnDisk, s_szMapPathName, sizeof( szNameOnDisk ) );

	g_ModelLoader.m_worldBrushData.m_nBSPFileSize = nDataSize;

	V_memcpy( &s_MapHeader, pData, sizeof( BSPHeader_t ) );

	if ( s_MapHeader.ident != IDBSPHEADER )
	{
		Host_Error( "CMapLoadHelper::Init, map %s has wrong identifier\n", szNameOnDisk );
		return;
	}

	if ( s_MapHeader.m_nVersion < MINBSPVERSION || s_MapHeader.m_nVersion > BSPVERSION )
	{
		Host_Error( "CMapLoadHelper::Init, map %s has wrong version (%i when expecting %i)\n", szNameOnDisk, s_MapHeader.m_nVersion, BSPVERSION );
		return;
	}

	// Store map version
	g_ServerGlobalVariables.mapversion = s_MapHeader.mapRevision;

#ifndef DEDICATED
	InitDLightGlobals( s_MapHeader.m_nVersion );
#endif

	s_pMap = &g_ModelLoader.m_worldBrushData;
}

//-----------------------------------------------------------------------------
// Shutdown a BSP loading context.
//-----------------------------------------------------------------------------
void CMapLoadHelper::Shutdown( void )
{
	if ( --s_nMapLoadRecursion > 0 )
	{
		return;
	}

	if ( s_MapFileHandle != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFileSystem->Close( s_MapFileHandle );
		s_MapFileHandle = FILESYSTEM_INVALID_HANDLE;
	}

	if ( IsPC() )
	{
		// Close our open lump files
		for ( int i = 0; i < HEADER_LUMPS; i++ )
		{
			if ( s_MapLumpFiles[i].file != FILESYSTEM_INVALID_HANDLE )
			{
				g_pFileSystem->Close( s_MapLumpFiles[i].file );
			}
		}
		V_memset( &s_MapLumpFiles, 0, sizeof( s_MapLumpFiles ) );
	}

	s_szMapPathName[ 0 ] = '\0';
	V_memset( &s_MapHeader, 0, sizeof( s_MapHeader ) );
	s_pMap = NULL;

	// discard from memory
	if ( s_MapBuffer.GetUsed() )
	{
		s_MapBuffer.FreeAll();
	}
}

//-----------------------------------------------------------------------------
// Free the lighting lump (increases free memory during loading on 360)
//-----------------------------------------------------------------------------
void CMapLoadHelper::FreeLightingLump( void )
{
	if ( IsGameConsole() && ( s_MapFileHandle == FILESYSTEM_INVALID_HANDLE ) && s_MapBuffer.GetUsed() )
	{
		int lightingLump = LumpSize( LUMP_LIGHTING_HDR ) ? LUMP_LIGHTING_HDR : LUMP_LIGHTING;
		// Should never have both lighting lumps on 360
		Assert( ( lightingLump == LUMP_LIGHTING ) || ( LumpSize( LUMP_LIGHTING ) == 0 ) );

		if ( LumpSize( lightingLump ) )
		{
			// Check that the lighting lump is next to the last one in the BSP
			// The pak file is expected to be last
			int lightingOffset = LumpOffset( lightingLump );
			for ( int i = 0; i < HEADER_LUMPS; i++ )
			{
				if ( ( LumpOffset( i ) > lightingOffset ) && ( i != LUMP_PAKFILE ) )
				{
					Warning( "CMapLoadHelper: Cannot free lighting lump (should be last before the PAK lump).\n" );
					Warning( "Lumps may be now be incorrectly ordered. Regenerate the " PLATFORM_EXT ".bsp file with MakeGameData.\n" );
					return;
				}
			}

			// Flag the lighting chunk as gone from the BSP (principally, this sets 'filelen' to 0)
			V_memset( &s_MapHeader.lumps[ lightingLump ], 0, sizeof( lump_t ) );

			// Shrink the buffer to free up the space that was used by the lighting lump
			// The pak file is not part of the original allocation
			s_MapBuffer.FreeToAllocPoint( (MemoryStackMark_t)lightingOffset );
		}
	}
}


//-----------------------------------------------------------------------------
// Returns the size of a particular lump without loading it...
//-----------------------------------------------------------------------------
int CMapLoadHelper::LumpSize( int lumpId )
{
	// If we have a lump file for this lump, return its length instead
	if ( IsPC() && s_MapLumpFiles[lumpId].file != FILESYSTEM_INVALID_HANDLE )
	{
		return s_MapLumpFiles[lumpId].header.lumpLength;
	}

	lump_t *pLump = &s_MapHeader.lumps[ lumpId ];
	Assert( pLump );

	if ( IsGameConsole() )
	{
		// a compressed lump hides the uncompressed size in the unused fourCC
		// otherwise, the data has to be loaded to determine original size
		// all knowledge of compression is private, they expect and get the original size
		int originalSize = BigLong( *((int *)s_MapHeader.lumps[lumpId].fourCC) );
		if ( originalSize )
		{
			return originalSize;
		}
	}

	return pLump->filelen;
}

//-----------------------------------------------------------------------------
// Returns the offset of a particular lump without loading it...
//-----------------------------------------------------------------------------
int CMapLoadHelper::LumpOffset( int lumpID  )
{
	// If we have a lump file for this lump, return 
	// the offset to move past the lump file header.
	if ( IsPC() && s_MapLumpFiles[lumpID].file != FILESYSTEM_INVALID_HANDLE )
	{
		return s_MapLumpFiles[lumpID].header.lumpOffset;
	}

	lump_t *pLump = &s_MapHeader.lumps[ lumpID ];
	Assert( pLump );

	return pLump->fileofs;
}

//-----------------------------------------------------------------------------
// Loads one element in a lump.
//-----------------------------------------------------------------------------
void CMapLoadHelper::LoadLumpElement( int nElemIndex, int nElemSize, void *pData )
{
	if ( !nElemSize || !m_nLumpSize )
	{
		return;
	}

	// supply from memory
	if ( nElemIndex * nElemSize + nElemSize <= m_nLumpSize )
	{
		V_memcpy( pData, m_pData + nElemIndex * nElemSize, nElemSize );
	}
	else
	{
		// out of range
		Assert( 0 );
	}
}


//-----------------------------------------------------------------------------
// Loads one element in a lump.
//-----------------------------------------------------------------------------
void CMapLoadHelper::LoadLumpData( int offset, int size, void *pData )
{
	if ( !size || !m_nLumpSize )
	{
		return;
	}

	if ( offset + size <= m_nLumpSize )
	{
		V_memcpy( pData, m_pData + offset, size );
	}
	else
	{
		// out of range
		Assert( 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mapfile - 
//			lumpToLoad - 
//-----------------------------------------------------------------------------
CMapLoadHelper::CMapLoadHelper( int lumpToLoad, bool bUncompress )
{
	if ( lumpToLoad < 0 || lumpToLoad >= HEADER_LUMPS )
	{
		Sys_Error( "Can't load lump %i, range is 0 to %i!!!", lumpToLoad, HEADER_LUMPS - 1 );
	}

	m_nLumpID = lumpToLoad;
	m_nLumpSize = 0;
	m_nLumpOffset = -1;
	m_pData = NULL;
	m_pRawData = NULL;
	m_pUncompressedData = NULL;
	m_nUncompressedLumpSize = 0;
	m_bUncompressedDataExternal = false;
	
	// Load raw lump from disk
	lump_t *lump = &s_MapHeader.lumps[ lumpToLoad ];
	Assert( lump );

	m_nLumpSize = lump->filelen;
	m_nLumpOffset = lump->fileofs;
	m_nLumpVersion = lump->version;	

	FileHandle_t fileToUse = s_MapFileHandle;

	// If we have a lump file for this lump, use it instead
	if ( IsPC() && s_MapLumpFiles[lumpToLoad].file != FILESYSTEM_INVALID_HANDLE )
	{
		fileToUse = s_MapLumpFiles[lumpToLoad].file;
		m_nLumpSize = s_MapLumpFiles[lumpToLoad].header.lumpLength;
		m_nLumpOffset = s_MapLumpFiles[lumpToLoad].header.lumpOffset;
		m_nLumpVersion = s_MapLumpFiles[lumpToLoad].header.lumpVersion;

		// Store off the lump file name
		GenerateLumpFileName( s_szMapPathName, m_szLumpFilename, sizeof( m_szLumpFilename ), s_MapLumpFiles[lumpToLoad].lumpfileindex );
	}

	if ( !m_nLumpSize )
	{
		// this lump has no data
		return;
	}

	if ( s_MapBuffer.GetUsed() )
	{
		// bsp is in memory
		m_pData = (unsigned char*)s_MapBuffer.GetBase() + m_nLumpOffset;
	}
	else
	{
		if ( s_MapFileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			Sys_Error( "Can't load map from invalid handle!!!" );
		}

		unsigned nOffsetAlign, nSizeAlign, nBufferAlign;
		bool bTryOptimal = g_pFileSystem->GetOptimalIOConstraints( fileToUse, &nOffsetAlign, &nSizeAlign, &nBufferAlign );

		if ( bTryOptimal )
		{
			bTryOptimal = ( m_nLumpOffset % 4 == 0 ); // Don't return badly aligned data
		}

		unsigned int alignedOffset = m_nLumpOffset;
		unsigned int alignedBytesToRead = ( ( m_nLumpSize ) ? m_nLumpSize : 1 );

		if ( bTryOptimal )
		{
			alignedOffset = AlignValue( ( alignedOffset - nOffsetAlign ) + 1, nOffsetAlign );
			alignedBytesToRead = AlignValue( ( m_nLumpOffset - alignedOffset ) + alignedBytesToRead, nSizeAlign );
		}

		m_pRawData = (byte *)g_pFileSystem->AllocOptimalReadBuffer( fileToUse, alignedBytesToRead, alignedOffset );
		if ( !m_pRawData && m_nLumpSize )
		{
			Sys_Error( "Can't load lump %i, allocation of %i bytes failed!!!", lumpToLoad, m_nLumpSize + 1 );
		}

		if ( m_nLumpSize )
		{
			g_pFileSystem->Seek( fileToUse, alignedOffset, FILESYSTEM_SEEK_HEAD );
			g_pFileSystem->ReadEx( m_pRawData, alignedBytesToRead, alignedBytesToRead, fileToUse );
			m_pData = m_pRawData + ( m_nLumpOffset - alignedOffset );
		}
	}

	m_nUncompressedLumpSize = m_nLumpSize;
	if ( IsGameConsole() )
	{
		CLZMA lzma;
		if ( lzma.IsCompressed( m_pData ) )
		{
			m_nUncompressedLumpSize = lzma.GetActualSize( m_pData );
			if ( bUncompress )
			{
				UncompressLump();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapLoadHelper::~CMapLoadHelper( void )
{
	if ( IsGameConsole() && m_pUncompressedData && !m_bUncompressedDataExternal )
	{
		free( m_pUncompressedData );
	}

	if ( m_pRawData )
	{
		g_pFileSystem->FreeOptimalReadBuffer( m_pRawData );
	}
}

//-----------------------------------------------------------------------------
// Purpose: decompress a compressed lump
//-----------------------------------------------------------------------------
void CMapLoadHelper::UncompressLump( void *pExternalBuffer )
{
	Assert( !m_pUncompressedData );
	if ( m_pUncompressedData )
		return; // Already uncompressed!


	if ( pExternalBuffer )
	{
		m_pUncompressedData = (unsigned char *)pExternalBuffer;
		m_bUncompressedDataExternal = true;
	}
	else
	{
		m_pUncompressedData = (unsigned char *)malloc( m_nUncompressedLumpSize );
		m_bUncompressedDataExternal = false;
	}
	CLZMA lzma;
	int decodedLength;
	if ( IsGameConsole() && lzma.IsCompressed( m_pData ) )
	{
		// Uncompress into the dest buffer
		decodedLength = lzma.Uncompress( m_pData, m_pUncompressedData );
		Assert( decodedLength == m_nUncompressedLumpSize );
	}
	else
	{
		// Copy into the dest buffer
		Assert( m_nLumpSize == m_nUncompressedLumpSize );
		memcpy( m_pUncompressedData, m_pData, m_nUncompressedLumpSize );
	}

	// a user of the class sees the uncompressed data
	m_pData     = m_pUncompressedData;
	m_nLumpSize = m_nUncompressedLumpSize;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : model_t
//-----------------------------------------------------------------------------
worldbrushdata_t *CMapLoadHelper::GetMap( void )
{
	Assert( s_pMap );
	return s_pMap;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CMapLoadHelper::GetMapPathName( void )
{
	return s_szMapPathName;
}

//-----------------------------------------------------------------------------
// Return the path for the lump, can be the map path or an atomic lump file
// override.
//-----------------------------------------------------------------------------
char *CMapLoadHelper::GetLoadName( void )
{
	// If we have a custom lump file for the lump this helper 
	// is loading, return it instead.
	if ( IsPC() && s_MapLumpFiles[m_nLumpID].file != FILESYSTEM_INVALID_HANDLE )
	{
		return m_szLumpFilename;
	}

	return s_szMapPathName;
}

//-----------------------------------------------------------------------------
// Hides possible platform extension.
//-----------------------------------------------------------------------------
char *CMapLoadHelper::GetDiskName( void )
{
	return GetMapPathNameOnDisk( s_szMapPathNameOnDisk, GetLoadName(), sizeof( s_szMapPathNameOnDisk ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : byte
//-----------------------------------------------------------------------------
byte *CMapLoadHelper::LumpBase( void )
{
	return m_pData;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CMapLoadHelper::LumpSize()
{
	return m_nLumpSize;
}

int CMapLoadHelper::UncompressedLumpSize()
{
	return m_nUncompressedLumpSize;
}

int CMapLoadHelper::LumpOffset()
{
	return m_nLumpOffset;
}

int	CMapLoadHelper::LumpVersion() const
{
	return m_nLumpVersion;
}

void EnableHDR( bool bEnable )
{
	if ( g_pMaterialSystemHardwareConfig->GetHDREnabled() == bEnable )
		return;

	g_pMaterialSystemHardwareConfig->SetHDREnabled( bEnable );

	if ( IsGameConsole() )
	{
		// cannot do what the pc does and ditch resources, we're loading!
		// can safely do the state update only, knowing that the state change won't affect 360 resources
		((MaterialSystem_Config_t *)g_pMaterialSystemConfig)->SetFlag( MATSYS_VIDCFG_FLAGS_ENABLE_HDR, bEnable );
		return;
	}

	// And this is okay here!
	materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();

	ShutdownWellKnownRenderTargets();
	InitWellKnownRenderTargets();

	// Grah. This is terrible. changin mat_hdr_enabled at the commandline
	// will by definition break because the release/restore methods don't call
	// ShutdownWellKnownRenderTargets/InitWellKnownRenderTargets.
	// Also, this forces two alt-tabs, one for InitWellKnownRenderTargets, one
	// for UpdateMaterialSystemConfig.
	UpdateMaterialSystemConfig();

	// Worse, since we need to init+shutdown render targets here, we can't
	// rely on UpdateMaterialSystemConfig to release + reacquire resources
	// because it could be called at any time. We have to precisely control
	// when hdr is changed since this is the only time the code can handle it.
	materials->ReleaseResources();
	materials->ReacquireResources();

	materials->FinishRenderTargetAllocation();
}

//-----------------------------------------------------------------------------
// Determine feature flags
//-----------------------------------------------------------------------------
void Map_CheckFeatureFlags()
{
	g_bLoadedMapHasBakedPropLighting = false;
	g_bBakedPropLightingNoSeparateHDR = false;
	g_bHasLightmapAlphaData = false;
	g_bBakedPropLightingStreams3 = false;
	g_bHasIndirectOnlyInLightingStreams = false;
	g_bLightstylesWithCSM = false;

	if ( CMapLoadHelper::LumpSize( LUMP_MAP_FLAGS ) > 0 )
	{
		CMapLoadHelper lh( LUMP_MAP_FLAGS );
		dflagslump_t flags_lump;
		flags_lump = *( (dflagslump_t *)( lh.LumpBase() ) );

		// check if loaded map has baked static prop lighting
		g_bLoadedMapHasBakedPropLighting = 
			( flags_lump.m_LevelFlags & LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_NONHDR ) != 0 ||
			( flags_lump.m_LevelFlags & LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_HDR ) != 0;
		g_bBakedPropLightingNoSeparateHDR = 
			( flags_lump.m_LevelFlags & LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_HDR ) == 0;
		g_bHasLightmapAlphaData = ( flags_lump.m_LevelFlags & LVLFLAGS_LIGHTMAP_ALPHA ) != 0;
		g_bBakedPropLightingStreams3 = ( flags_lump.m_LevelFlags & LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_3 ) != 0;
		g_bHasLightmapAlphaData3 = ( flags_lump.m_LevelFlags & LVLFLAGS_LIGHTMAP_ALPHA_3 ) != 0;
		g_bHasIndirectOnlyInLightingStreams = ( flags_lump.m_LevelFlags & LVLFLAGS_BAKED_STATIC_PROP_LIGHTING_3_NO_SUN ) != 0;
		g_bLightstylesWithCSM = ( flags_lump.m_LevelFlags & LVLFLAGS_LIGHTSTYLES_WITH_CSM ) != 0;

		// set num static light streams value
		ConVarRef r_staticlight_streams( "r_staticlight_streams" );
		r_staticlight_streams.SetValue( g_bBakedPropLightingStreams3 ? 3 : 1 );
		ConVarRef r_staticlight_streams_indirect_only( "r_staticlight_streams_indirect_only" );
		r_staticlight_streams_indirect_only.SetValue( g_bHasIndirectOnlyInLightingStreams );

		g_pMaterialSystemHardwareConfig->SetCSMAccurateBlending( g_bHasLightmapAlphaData3 );
	}
}

//-----------------------------------------------------------------------------
// Parse the map header for HDR ability. Returns the presence of HDR data only,
// not the HDR enable state.
//-----------------------------------------------------------------------------
bool Map_CheckForHDR( model_t *pModel, const char *pMapPathName )
{
	// parse the map header only
	CMapLoadHelper::Init( pModel, pMapPathName );

	bool bHasHDR = false;
	if ( IsGameConsole() )
	{
		// If this is true, the 360 MUST use HDR, because the LDR data gets stripped out.
		bHasHDR = CMapLoadHelper::LumpSize( LUMP_LIGHTING_HDR ) > 0;
	}
	else
	{
		// might want to also consider the game lumps GAMELUMP_DETAIL_PROP_LIGHTING_HDR
		bHasHDR = CMapLoadHelper::LumpSize( LUMP_LIGHTING_HDR ) > 0 &&
			CMapLoadHelper::LumpSize( LUMP_WORLDLIGHTS_HDR ) > 0;
		//			 Mod_GameLumpSize( GAMELUMP_DETAIL_PROP_LIGHTING_HDR ) > 0  // fixme
	}
	if ( s_MapHeader.m_nVersion >= 20 && CMapLoadHelper::LumpSize( LUMP_LEAF_AMBIENT_LIGHTING_HDR ) == 0 )
	{
		// This lump only exists in version 20 and greater, so don't bother checking for it on earlier versions.
		bHasHDR = false;
	}
	
	bool bEnableHDR = ( IsGameConsole() && bHasHDR ) ||
		bHasHDR && 
		( mat_hdr_level.GetInt() >= 2 ) && 
		( g_pMaterialSystemHardwareConfig->GetHardwareHDRType() != HDR_TYPE_NONE );
	EnableHDR( bEnableHDR );

	// this data really should have been in the header, but it isn't
	// establish the features now, before the real bsp load commences
	Map_CheckFeatureFlags();

	CMapLoadHelper::Shutdown();

	return bHasHDR;
}

//-----------------------------------------------------------------------------
// Allocates, frees lighting data
//-----------------------------------------------------------------------------
static void AllocateLightingData( worldbrushdata_t *pBrushData, int nSize )
{
	g_bHunkAllocLightmaps = ( !r_keepstyledlightmapsonly.GetBool() && !r_unloadlightmaps.GetBool() && r_hunkalloclightmaps.GetBool() );
	if ( g_bHunkAllocLightmaps )
	{
		pBrushData->lightdata = (ColorRGBExp32 *)Hunk_AllocName( nSize, "Lightmaps", false );
	}
	else if ( r_keepstyledlightmapsonly.GetBool() && !r_unloadlightmaps.GetBool() && pBrushData->m_pLightingDataStack )
	{
		// the lighting data is allocated from a memory stack
		// this is to facilitate the decommit after compaction due to discarding lightmaps
		pBrushData->m_pLightingDataStack->Term();
		pBrushData->m_pLightingDataStack->Init( "LightingData", nSize );
		pBrushData->lightdata = (ColorRGBExp32 *)pBrushData->m_pLightingDataStack->Alloc( nSize );
	}
	else
	{
		// Specifically *not* adding it to the hunk.
		// If this malloc changes, also change the free in CacheAndUnloadLightmapData()
		pBrushData->lightdata = (ColorRGBExp32 *)malloc( nSize );
	}

	pBrushData->m_nLightingDataSize = nSize;
	pBrushData->m_bUnloadedAllLightmaps = false;
}

void DeallocateLightingData( worldbrushdata_t *pBrushData )
{
	if ( pBrushData && pBrushData->lightdata )
	{
		if ( !g_bHunkAllocLightmaps )
		{
			if ( pBrushData->m_pLightingDataStack && pBrushData->m_pLightingDataStack->GetSize() )
			{
				// the lighting data was placed in the memory stack
				pBrushData->m_pLightingDataStack->Term();
			}
			else
			{
				free( pBrushData->lightdata );
			}
		}

		pBrushData->lightdata = NULL;
		pBrushData->m_nLightingDataSize = 0;
	}
}

static int ComputeLightmapSize( dface_t *pFace, mtexinfo_t *pTexInfo )
{
	bool bNeedsBumpmap = false;
	if( pTexInfo[pFace->texinfo].flags & SURF_BUMPLIGHT )
	{
		bNeedsBumpmap = true;
	}

    int lightstyles;
    for (lightstyles=0; lightstyles < MAXLIGHTMAPS; lightstyles++ )
    {
        if ( pFace->styles[lightstyles] == 255 )
            break;
    }

	int nLuxels = (pFace->m_LightmapTextureSizeInLuxels[0]+1) * (pFace->m_LightmapTextureSizeInLuxels[1]+1);
	if( bNeedsBumpmap )
	{
		return nLuxels * 4 * lightstyles * ( NUM_BUMP_VECTS + 1 );
	}

	return nLuxels * 4 * lightstyles;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadLighting( bool bLoadHDR )
{
	// NOTE: we tell CMapLoadHelper to NOT decompress the lighting lump, since we want to allocate the final buffer here:
	bool bDontDecompress = false;
	CMapLoadHelper lh( bLoadHDR ? LUMP_LIGHTING_HDR : LUMP_LIGHTING, bDontDecompress );

	if ( !lh.LumpSize() )
	{
		lh.GetMap()->lightdata = NULL;
		return;
	}
	Assert ( lh.LumpVersion() != 0 );

	AllocateLightingData( lh.GetMap(), lh.UncompressedLumpSize() );
	if ( !lh.GetMap()->lightdata )
	{
		// TERROR: If we fail this huge malloc, don't crash on a memcpy.  We'll rely on the fact that lightdata
		// can be NULL if the lump is 0 bytes above, and hope that we won't crash.  No worse than the
		// guaranteed crash below if we don't bail.
		return;
	}

	// Now pass in our buffer for decompression to occur (just a memcpy if the data is not compressed):
	lh.UncompressLump( lh.GetMap()->lightdata );

	if ( IsGameConsole() )
	{
		// Free the lighting lump, to increase the amount of memory free during the rest of loading
		CMapLoadHelper::FreeLightingLump();
	}
}

void Mod_LoadFaceBrushes()
{
	{
		CMapLoadHelper lh( LUMP_FACEBRUSHLIST );
		if ( !lh.LumpSize() )
		{
			lh.GetMap()->m_pSurfaceBrushes = NULL;
			lh.GetMap()->m_pSurfaceBrushList = NULL;
			return;
		}
		lh.GetMap()->m_pSurfaceBrushList = (dfacebrushlist_t *)Hunk_AllocName( lh.LumpSize(), "FaceBrushLists", false );
		memcpy( lh.GetMap()->m_pSurfaceBrushList, lh.LumpBase(), lh.LumpSize() );
	}
	{
		CMapLoadHelper lh( LUMP_FACEBRUSHES );
		lh.GetMap()->m_pSurfaceBrushes = (uint16 *)Hunk_AllocName( lh.LumpSize(), "FaceBrushes", false );
		memcpy( lh.GetMap()->m_pSurfaceBrushes, lh.LumpBase(), lh.LumpSize() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadWorldlights( CMapLoadHelper &lh, bool bIsHDR )
{
	lh.GetMap()->shadowzbuffers = NULL;
	if ( !lh.LumpSize() )
	{
		lh.GetMap()->numworldlights = 0;
		lh.GetMap()->worldlights = NULL;
		return;
	}

	switch ( lh.LumpVersion() )
	{
		case LUMP_WORLDLIGHTS_VERSION:
		{
			lh.GetMap()->numworldlights = lh.LumpSize() / sizeof( dworldlight_t );
			lh.GetMap()->worldlights = (dworldlight_t *)Hunk_AllocName( lh.LumpSize(), va( "%s [%s]", lh.GetLoadName(), "worldlights" ) );
			memcpy( lh.GetMap()->worldlights, lh.LumpBase(), lh.LumpSize() );
			break;
		}

		case 0:
		{
			int nNumWorldLights = lh.LumpSize() / sizeof( dworldlight_version0_t );
			lh.GetMap()->numworldlights = nNumWorldLights;
			lh.GetMap()->worldlights = (dworldlight_t *)Hunk_AllocName( nNumWorldLights * sizeof( dworldlight_t ), va( "%s [%s]", lh.GetLoadName(), "worldlights" ) );
			dworldlight_version0_t* RESTRICT pOldWorldLight = reinterpret_cast<dworldlight_version0_t*>( lh.LumpBase() );
			dworldlight_t* RESTRICT pNewWorldLight = lh.GetMap()->worldlights;

			for ( int i = 0; i < nNumWorldLights; i++ )
			{
				pNewWorldLight->origin			= pOldWorldLight->origin;
				pNewWorldLight->intensity		= pOldWorldLight->intensity;
				pNewWorldLight->normal			= pOldWorldLight->normal;
				pNewWorldLight->shadow_cast_offset.Init( 0.0f, 0.0f, 0.0f );
				pNewWorldLight->cluster			= pOldWorldLight->cluster;
				pNewWorldLight->type			= pOldWorldLight->type;
				pNewWorldLight->style			= pOldWorldLight->style;
				pNewWorldLight->stopdot			= pOldWorldLight->stopdot;
				pNewWorldLight->stopdot2		= pOldWorldLight->stopdot2;
				pNewWorldLight->exponent		= pOldWorldLight->exponent;
				pNewWorldLight->radius			= pOldWorldLight->radius;
				pNewWorldLight->constant_attn	= pOldWorldLight->constant_attn;	
				pNewWorldLight->linear_attn		= pOldWorldLight->linear_attn;
				pNewWorldLight->quadratic_attn	= pOldWorldLight->quadratic_attn;
				pNewWorldLight->flags			= pOldWorldLight->flags;
				pNewWorldLight->texinfo			= pOldWorldLight->texinfo;
				pNewWorldLight->owner			= pOldWorldLight->owner;
				pNewWorldLight++;
				pOldWorldLight++;
			}
			break;
		}

		default:
			Host_Error( "Invalid worldlight lump version!\n" );
			break;
	}

#if !defined( DEDICATED )
	if ( r_lightcache_zbuffercache.GetInt() )
	{
		size_t zbufSize = lh.GetMap()->numworldlights * sizeof( lightzbuffer_t );
		lh.GetMap()->shadowzbuffers = ( lightzbuffer_t *)Hunk_AllocName( zbufSize, va( "%s [%s]", lh.GetLoadName(), "shadowzbuffers" ) );
		memset( lh.GetMap()->shadowzbuffers, 0, zbufSize );		// mark empty
	}
#endif

	// Fixup for backward compatability
	for ( int i = 0; i < lh.GetMap()->numworldlights; i++ )
	{
		if ( lh.GetMap()->worldlights[i].type == emit_spotlight )
		{
			if ((lh.GetMap()->worldlights[i].constant_attn == 0.0) && 
				(lh.GetMap()->worldlights[i].linear_attn == 0.0) && 
				(lh.GetMap()->worldlights[i].quadratic_attn == 0.0))
			{
				lh.GetMap()->worldlights[i].quadratic_attn = 1.0;
			}

			if ( lh.GetMap()->worldlights[i].exponent == 0.0 )
				lh.GetMap()->worldlights[i].exponent = 1.0;
		}
		else if ( lh.GetMap()->worldlights[i].type == emit_point )
		{
			// To match earlier lighting, use quadratic...
			if ((lh.GetMap()->worldlights[i].constant_attn == 0.0) && 
				(lh.GetMap()->worldlights[i].linear_attn == 0.0) && 
				(lh.GetMap()->worldlights[i].quadratic_attn == 0.0))
			{
				lh.GetMap()->worldlights[i].quadratic_attn = 1.0;
			}
		}

   		// I replaced the cuttoff_dot field (which took a value from 0 to 1)
		// with a max light radius. Radius of less than 1 will never happen,
		// so I can get away with this. When I set radius to 0, it'll 
		// run the old code which computed a radius
		if ( lh.GetMap()->worldlights[i].radius < 1 )
		{
			lh.GetMap()->worldlights[i].radius = ComputeLightRadius( &lh.GetMap()->worldlights[i], bIsHDR );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadVertices( void )
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	CMapLoadHelper lh( LUMP_VERTEXES );

	in = (dvertex_t *)lh.LumpBase();
	if ( lh.LumpSize() % sizeof(*in) )
	{
		Host_Error( "Mod_LoadVertices: funny lump size in %s", lh.GetMapPathName() );
	}
	count = lh.LumpSize() / sizeof(*in);
	out = (mvertex_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "vertexes" ) );

	lh.GetMap()->vertexes = out;
	lh.GetMap()->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = in->point[0];
		out->position[1] = in->point[1];
		out->position[2] = in->point[2];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : mins - 
//			maxs - 
// Output : float
//-----------------------------------------------------------------------------
static float RadiusFromBounds (Vector& mins, Vector& maxs)
{
	int		i;
	Vector	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return VectorLength( corner );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadSubmodels( CUtlVector<mmodel_t> &submodelList )
{
	dmodel_t	*in;
	int			i, j, count;

	CMapLoadHelper lh( LUMP_MODELS );

	in = (dmodel_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error("Mod_LoadSubmodels: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);

	submodelList.SetCount( count );
	lh.GetMap()->numsubmodels = count;
	// first submodel is the world, copy out the face count
	lh.GetMap()->nWorldFaceCount = in->numfaces;

	for ( i=0 ; i<count ; i++, in++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			submodelList[i].mins[j] = in->mins[j] - 1;
			submodelList[i].maxs[j] = in->maxs[j] + 1;
			submodelList[i].origin[j] = in->origin[j];
		}
		submodelList[i].radius = RadiusFromBounds (submodelList[i].mins, submodelList[i].maxs);
		submodelList[i].headnode = in->headnode;
		submodelList[i].firstface = in->firstface;
		submodelList[i].numfaces = in->numfaces;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : medge_t *Mod_LoadEdges
//-----------------------------------------------------------------------------
medge_t *Mod_LoadEdges ( void )
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	CMapLoadHelper lh( LUMP_EDGES );

	in = (dedge_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadEdges: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	medge_t *pedges = new medge_t[count];

	out = pedges;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = in->v[0];
		out->v[1] = in->v[1];
	}

	// delete this in the loader
	return pedges;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadOcclusion( void )
{
	CMapLoadHelper lh( LUMP_OCCLUSION );

	worldbrushdata_t *b = lh.GetMap();
	b->numoccluders = 0;
	b->occluders = NULL;
	b->numoccluderpolys = 0;
	b->occluderpolys = NULL;
	b->numoccludervertindices = 0;
	b->occludervertindices = NULL;

	if ( !lh.LumpSize() )
	{
		return;
	}

	CUtlBuffer buf( lh.LumpBase(), lh.LumpSize(), CUtlBuffer::READ_ONLY );

	switch( lh.LumpVersion() )
	{
	case LUMP_OCCLUSION_VERSION:
		{
			b->numoccluders = buf.GetInt();
			if (b->numoccluders)
			{
				int nSize = b->numoccluders * sizeof(doccluderdata_t);
				b->occluders = (doccluderdata_t*)Hunk_AllocName( nSize, "occluder data" );
				buf.Get( b->occluders, nSize );
			}

			b->numoccluderpolys = buf.GetInt();
			if (b->numoccluderpolys)
			{
				int nSize = b->numoccluderpolys * sizeof(doccluderpolydata_t);
				b->occluderpolys = (doccluderpolydata_t*)Hunk_AllocName( nSize, "occluder poly data" );
				buf.Get( b->occluderpolys, nSize );
			}

			b->numoccludervertindices = buf.GetInt();
			if (b->numoccludervertindices)
			{
				int nSize = b->numoccludervertindices * sizeof(int);
				b->occludervertindices = (int*)Hunk_AllocName( nSize, "occluder vertices" );
				buf.Get( b->occludervertindices, nSize );
			}
		}
		break;

	case 1:
		{
			b->numoccluders = buf.GetInt();
			if (b->numoccluders)
			{
				int nSize = b->numoccluders * sizeof(doccluderdata_t);
				b->occluders = (doccluderdata_t*)Hunk_AllocName( nSize, "occluder data" );

				doccluderdataV1_t temp;
				for ( int i = 0; i < b->numoccluders; ++i )
				{
					buf.Get( &temp, sizeof(doccluderdataV1_t) );
					memcpy( &b->occluders[i], &temp, sizeof(doccluderdataV1_t) );
					b->occluders[i].area = 1;
				}
			}

			b->numoccluderpolys = buf.GetInt();
			if (b->numoccluderpolys)
			{
				int nSize = b->numoccluderpolys * sizeof(doccluderpolydata_t);
				b->occluderpolys = (doccluderpolydata_t*)Hunk_AllocName( nSize, "occluder poly data" );
				buf.Get( b->occluderpolys, nSize );
			}

			b->numoccludervertindices = buf.GetInt();
			if (b->numoccludervertindices)
			{
				int nSize = b->numoccludervertindices * sizeof(int);
				b->occludervertindices = (int*)Hunk_AllocName( nSize, "occluder vertices" );
				buf.Get( b->occludervertindices, nSize );
			}
		}
		break;

	case 0:
		break;

	default:
		Host_Error("Invalid occlusion lump version!\n");
		break;
	}
}



// UNDONE: Really, it's stored 2 times because the texture system keeps a 
// copy of the name too.  I guess we'll get rid of this when we have a material
// system that works without a graphics context.  At that point, everyone can
// reference the name in the material, or just the material itself.
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadTexdata( void )
{
	// Don't bother loading these again; they're already stored in the collision model
	// which is guaranteed to be loaded at this point
	s_pMap->numtexdata = GetCollisionBSPData()->numtextures;
	s_pMap->texdata = GetCollisionBSPData()->map_surfaces.Base();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadTexinfo( CMapLoadHelper &lh )
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	// UNDONE: Fix this

	in = (texinfo_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadTexinfo: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mtexinfo_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "texinfo" ) );

	s_pMap->texinfo = out;
	s_pMap->numtexinfo = count;

#if defined( DEVELOPMENT_ONLY ) || defined( ALLOW_TEXT_MODE )
	static bool s_bTextMode = CommandLine()->HasParm( "-textmode" );
#else
	const bool s_bTextMode = false;
#endif

	bool loadtextures = mat_loadtextures.GetBool() && !s_bTextMode;

	for ( i=0 ; i<count ; ++i, ++in, ++out )
	{
		for (j=0; j<2; ++j)
		{
			for (int k=0 ; k<4 ; ++k)
			{
				out->textureVecsTexelsPerWorldUnits[j][k] = in->textureVecsTexelsPerWorldUnits[j][k];
				out->lightmapVecsLuxelsPerWorldUnits[j][k] = in->lightmapVecsLuxelsPerWorldUnits[j][k] ;
			}
		}

		// assume that the scale is the same on both s and t.
		out->luxelsPerWorldUnit = VectorLength( out->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D() );
		// Protect against divide-by-zero
		if ( out->luxelsPerWorldUnit != 0 )
			out->worldUnitsPerLuxel = 1.0f / out->luxelsPerWorldUnit;

		out->flags = in->flags;
		out->texinfoFlags = 0;

		if ( loadtextures )
		{
			if ( in->texdata >= 0 )
			{
				out->material = GL_LoadMaterial( lh.GetMap()->texdata[ in->texdata ].name, TEXTURE_GROUP_WORLD );
				if ( out->material->IsErrorMaterial() == true )
				{
					Msg( "Missing map material: %s\n", lh.GetMap()->texdata[ in->texdata ].name );
				}
			}
			else
			{
				DevMsg( "Mod_LoadTexinfo: texdata < 0 (index==%i/%i)\n", i, count );
				out->material = NULL;
			}
			if ( !out->material )
			{
				out->material = g_materialEmpty;
				g_materialEmpty->IncrementReferenceCount();
			}
 		}
		else
		{
			out->material = g_materialEmpty;
			g_materialEmpty->IncrementReferenceCount();
		}
	}
}

// code to scan the lightmaps for empty lightstyles
static void LinearToGamma( unsigned char *pDstRGB, const float *pSrcRGB )
{
	pDstRGB[0] = LinearToScreenGamma( pSrcRGB[0] );
	pDstRGB[1] = LinearToScreenGamma( pSrcRGB[1] );
	pDstRGB[2] = LinearToScreenGamma( pSrcRGB[2] );
}

static void CheckSurfaceLighting( SurfaceHandle_t surfID, worldbrushdata_t *pBrushData )
{
#if !defined( DEDICATED )
	host_state.worldbrush = pBrushData;
	msurfacelighting_t *pLighting = SurfaceLighting( surfID, pBrushData );

	if ( !pLighting->m_pSamples )
		return;

	int smax = ( pLighting->m_LightmapExtents[0] ) + 1;
	int tmax = ( pLighting->m_LightmapExtents[1] ) + 1;
	int size = smax * tmax;
	int offset = size;
	if ( SurfHasBumpedLightmaps( surfID ) )
	{
		offset *= ( NUM_BUMP_VECTS + 1 );
	}
	// for old maps revert to data layout before lightstyles were fixed to work with CSM's
	// this is so as not to break modders who used lightstyles in way that worked for them (i.e. no env_cascade light)
	if ( g_bLightstylesWithCSM )
	{
		// extra CSM alpha data - for new maps
		offset += size;
	}

	// how many additional lightmaps does this surface have?
	int maxLightmapIndex = 0;
	for (int maps = 1 ; maps < MAXLIGHTMAPS && pLighting->m_nStyles[maps] != 255 ; ++maps)
	{
		maxLightmapIndex = maps;
	}

	if ( maxLightmapIndex < 1 )
	{
		// can't purge the base lightmap
		return;
	}

	// iterate and test each lightmap
	for ( int maps = maxLightmapIndex; maps != 0; maps-- )
	{
		ColorRGBExp32 *pLightmap = pLighting->m_pSamples + (maps * offset);
		float maxLen = -1;
		Vector maxLight;
		maxLight.Init();
		for ( int i = 0; i < offset; i++ )
		{
			Vector c;
			ColorRGBExp32ToVector( pLightmap[i], c );
			if ( c.Length() > maxLen )
			{
				maxLight = c;
				maxLen = c.Length();
			}
		}

		unsigned char color[4];
		LinearToGamma( color, maxLight.Base() );
		const int minLightVal = 1;
		if ( color[0] <= minLightVal && color[1] <= minLightVal && color[2] <= minLightVal )
		{
			// found a lightmap that is too dark, remove it and shift over the subsequent maps/styles
			for ( int i = maps; i < maxLightmapIndex; i++ )
			{
				ColorRGBExp32 *pLightmapOverwrite = pLighting->m_pSamples + ( i * offset );
				memcpy( pLightmapOverwrite, pLightmapOverwrite+offset, offset * sizeof( ColorRGBExp32 ) );
				pLighting->m_nStyles[i] = pLighting->m_nStyles[i+1];

				// shift 'up' avgcolor values
				// the '-' is correct, the avgcolors are stored behind the lightmaps and in reverse order
				pLighting->m_pSamples[-( i + 1 )] = pLighting->m_pSamples[-( i + 2 )];
			}

			// mark end lightstyle as removed, decrement max index
			pLighting->m_nStyles[maxLightmapIndex] = 255;
			maxLightmapIndex--;
		}
	}

	// we removed all of the lightstyle maps so clear the flag
	if ( maxLightmapIndex == 0 )
	{
		surfID->flags &= ~SURFDRAW_HASLIGHTSYTLES;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*s - 
// Output : void CalcSurfaceExtents
//-----------------------------------------------------------------------------
static void CalcSurfaceExtents( CMapLoadHelper& lh, SurfaceHandle_t surfID )
{
	float	textureMins[2], textureMaxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	textureMins[0] = textureMins[1] = 999999;
	textureMaxs[0] = textureMaxs[1] = -99999;

	worldbrushdata_t *pBrushData = lh.GetMap();
	tex = MSurf_TexInfo( surfID, pBrushData );
	
	for (i=0 ; i<MSurf_VertCount( surfID ); i++)
	{
		e = pBrushData->vertindices[MSurf_FirstVertIndex( surfID )+i];
		v = &pBrushData->vertexes[e];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->textureVecsTexelsPerWorldUnits[j][0] + 
				  v->position[1] * tex->textureVecsTexelsPerWorldUnits[j][1] +
				  v->position[2] * tex->textureVecsTexelsPerWorldUnits[j][2] +
				  tex->textureVecsTexelsPerWorldUnits[j][3];
			if (val < textureMins[j])
				textureMins[j] = val;
			if (val > textureMaxs[j])
				textureMaxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		if( MSurf_LightmapExtents( surfID, pBrushData )[i] == 0 && !MSurf_Samples( surfID, pBrushData ) )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NOLIGHT;
		}

		bmins[i] = Float2Int( textureMins[i] );
		bmaxs[i] = Ceil2Int( textureMaxs[i] );
		MSurf_TextureMins( surfID, pBrushData )[i] = bmins[i];
		MSurf_TextureExtents( surfID, pBrushData )[i] = ( bmaxs[i] - bmins[i] );

		if ( !(tex->flags & SURF_NOLIGHT) && MSurf_LightmapExtents( surfID, pBrushData )[i] > MSurf_MaxLightmapSizeWithBorder( surfID ) )
		{
			Sys_Error ("Bad surface extents on texture %s", tex->material->GetName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Input  : *pModel - 
//			*pLump - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadVertNormals( void )
{
	CMapLoadHelper lh( LUMP_VERTNORMALS );

    // get a pointer to the vertex normal data.
	Vector *pVertNormals = ( Vector * )lh.LumpBase();

    //
    // verify vertnormals data size
    //
    if( lh.LumpSize() % sizeof( *pVertNormals ) )
        Host_Error( "Mod_LoadVertNormals: funny lump size in %s!\n", lh.GetMapPathName() );

	int count = lh.LumpSize() / sizeof(*pVertNormals);
	Vector *out = (Vector *)Hunk_AllocName( lh.LumpSize(), va( "%s [%s]", lh.GetLoadName(), "vertnormals" ) );
	memcpy( out, pVertNormals, lh.LumpSize() );
	
	lh.GetMap()->vertnormals = out;
	lh.GetMap()->numvertnormals = count;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadVertNormalIndices( void )
{
	CMapLoadHelper lh( LUMP_VERTNORMALINDICES );

    // get a pointer to the vertex normal data.
	unsigned short *pIndices = ( unsigned short * )lh.LumpBase();

	int count = lh.LumpSize() / sizeof(*pIndices);
	unsigned short *out = (unsigned short *)Hunk_AllocName( lh.LumpSize(), va( "%s [%s]", lh.GetLoadName(), "vertnormalindices" ) );
	memcpy( out, pIndices, lh.LumpSize() );
	
	lh.GetMap()->vertnormalindices = out;
	lh.GetMap()->numvertnormalindices = count;

	// OPTIMIZE: Water surfaces don't need vertex normals?
	int normalIndex = 0;
	for( int i = 0; i < lh.GetMap()->numsurfaces; i++ )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( i, lh.GetMap() );
		MSurf_FirstVertNormal( surfID, lh.GetMap() ) = normalIndex;
		normalIndex += MSurf_VertCount( surfID );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadPrimitives( void )
{
	dprimitive_t	*in;
	mprimitive_t	*out;
	int				i, count;

	CMapLoadHelper lh( LUMP_PRIMITIVES );

	in = (dprimitive_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadPrimitives: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mprimitive_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "primitives" ) );
	memset( out, 0, count * sizeof( mprimitive_t ) );

	lh.GetMap()->primitives = out;
	lh.GetMap()->numprimitives = count;
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->firstIndex		= in->firstIndex;
		out->firstVert		= in->firstVert;
		out->indexCount		= in->indexCount;
		out->type			= in->type;
		out->vertCount		= in->vertCount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadPrimVerts( void )
{
	dprimvert_t		*in;
	mprimvert_t		*out;
	int				i, count;

	CMapLoadHelper lh( LUMP_PRIMVERTS );

	in = (dprimvert_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadPrimVerts: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mprimvert_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "primverts" ) );
	memset( out, 0, count * sizeof( mprimvert_t ) );

	lh.GetMap()->primverts = out;
	lh.GetMap()->numprimverts = count;
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->pos = in->pos;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadPrimIndices( void )
{
	unsigned short	*in;
	unsigned short	*out;
	int				count;

	CMapLoadHelper lh( LUMP_PRIMINDICES );

	in = (unsigned short *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadPrimIndices: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	out = (unsigned short *)Hunk_AllocName( count*sizeof(*out), va("%s [%s]", lh.GetLoadName(), "primindices" ) );
	memset( out, 0, count * sizeof( unsigned short ) );

	lh.GetMap()->primindices = out;
	lh.GetMap()->numprimindices = count;

	memcpy( out, in, count * sizeof( unsigned short ) );
}


// This allocates memory for a lump and copies the lump data in.
void Mod_LoadLump( 
	model_t *loadmodel, 
	int iLump,
	char *loadname, 
	int elementSize,
	void **ppData, 
	int *nElements )
{
	CMapLoadHelper lh( iLump );

	if ( lh.LumpSize() % elementSize )
	{
		Host_Error( "Mod_LoadLump: funny lump size in %s", loadmodel->szPathName );
	}

	// How many elements?
	*nElements = lh.LumpSize() / elementSize;

	// Make room for the data and copy the data in.
	*ppData = Hunk_AllocName( lh.LumpSize(), loadname );
	memcpy( *ppData, lh.LumpBase(), lh.LumpSize() );
}


//-----------------------------------------------------------------------------
// Sets up the msurfacelighting_t structure
//-----------------------------------------------------------------------------
bool Mod_LoadSurfaceLighting( msurfacelighting_t *pLighting, dface_t *in, ColorRGBExp32 *pBaseLightData )
{
	// Get lightmap extents from the file.
	pLighting->m_LightmapExtents[0] = in->m_LightmapTextureSizeInLuxels[0];
	pLighting->m_LightmapExtents[1] = in->m_LightmapTextureSizeInLuxels[1];
	pLighting->m_LightmapMins[0] = in->m_LightmapTextureMinsInLuxels[0];
	pLighting->m_LightmapMins[1] = in->m_LightmapTextureMinsInLuxels[1];

	int lightOffset = in->lightofs;
	if ( ( lightOffset == -1 ) || !pBaseLightData )
	{
		pLighting->m_pSamples = NULL;

		// Can't have *any* lightstyles if we have no samples
		for ( int i = 0; i < MAXLIGHTMAPS; ++i )
		{
			pLighting->m_nStyles[i] = 255;
		}
	}
	else
	{
		pLighting->m_pSamples = (ColorRGBExp32 *)( ((byte *)pBaseLightData) + lightOffset );

		for ( int i = 0; i<MAXLIGHTMAPS; ++i )
		{
			pLighting->m_nStyles[i] = in->styles[i];
		}
	}

	return ((pLighting->m_nStyles[0] != 0) && (pLighting->m_nStyles[0] != 255)) || (pLighting->m_nStyles[1] != 255);
}

void *Hunk_AllocNameAlignedClear_( int size, int alignment, const char *pHunkName )
{
	Assert(IsPowerOfTwo(alignment));
	void *pMem = Hunk_AllocName( alignment + size, pHunkName );
	memset( pMem, 0, size + alignment );
	pMem = (void *)( ( ( ( unsigned long )pMem ) + (alignment-1) ) & ~(alignment-1) );

	return pMem;
}

// Allocates a block of T from the hunk.  Aligns as specified and clears the memory
template< typename T > 
T *Hunk_AllocNameAlignedClear( int count, int alignment, const char *pHunkName )
{
	return (T *)Hunk_AllocNameAlignedClear_( alignment + count * sizeof(T), alignment, pHunkName );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadFaces( void )
{
	dface_t		*in;
	int			count, surfnum;
	int			planenum;
	int			ti, di;

	int face_lump_to_load = LUMP_FACES;
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE &&
		CMapLoadHelper::LumpSize( LUMP_FACES_HDR ) > 0 )
	{
		face_lump_to_load = LUMP_FACES_HDR;
	}
	CMapLoadHelper lh( face_lump_to_load );
	
	in = (dface_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadFaces: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);

	// align these allocations
	// If you trip one of these, you need to rethink the alignment of the struct
	Assert( sizeof(msurface1_t) == 16 );
	Assert( sizeof(msurface2_t) == 32 );
	Assert( sizeof(msurfacelighting_t) == 32 );

	msurface1_t *out1 = Hunk_AllocNameAlignedClear< msurface1_t >( count, 16, va( "%s [%s]", lh.GetLoadName(), "surface1" ) );
	msurface2_t *out2 = Hunk_AllocNameAlignedClear< msurface2_t >( count, 32, va( "%s [%s]", lh.GetLoadName(), "surface2" ) );

	msurfacelighting_t *pLighting = Hunk_AllocNameAlignedClear< msurfacelighting_t >( count, 32, va( "%s [%s]", lh.GetLoadName(), "surfacelighting" ) );

	lh.GetMap()->surfaces1 = out1;
	lh.GetMap()->surfaces2 = out2;
	lh.GetMap()->surfacelighting = pLighting;
	lh.GetMap()->surfacenormals = Hunk_AllocNameAlignedClear< msurfacenormal_t >( count, 2, va( "%s [%s]", lh.GetLoadName(), "surfacenormal" ) );
	lh.GetMap()->numsurfaces = count;

	worldbrushdata_t *pBrushData = lh.GetMap();

	for ( surfnum=0 ; surfnum<count ; ++surfnum, ++in, ++out1, ++out2, ++pLighting )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( surfnum, pBrushData );
		MSurf_FirstVertIndex( surfID )  = in->firstedge;
		
		int vertCount = in->numedges;
		MSurf_Flags( surfID ) = 0;
		Assert( vertCount <= 255 );
		MSurf_SetVertCount( surfID, vertCount );

		planenum = in->planenum;
		
		if ( in->onNode )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NODE;
		}
		if ( in->side )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_PLANEBACK;
		}

#ifndef _PS3
		out2->plane = lh.GetMap()->planes + planenum;
#else
		out2->m_plane = *(lh.GetMap()->planes + planenum);
#endif

		ti = in->texinfo;
		if (ti < 0 || ti >= lh.GetMap()->numtexinfo)
		{
			Host_Error( "Mod_LoadFaces: bad texinfo number" );
		}
		surfID->texinfo = ti;
		surfID->m_bDynamicShadowsEnabled = in->AreDynamicShadowsEnabled();
		mtexinfo_t *pTex = lh.GetMap()->texinfo + ti;

		// big hack!
		if ( !pTex->material )
		{
			pTex->material = g_materialEmpty;
			g_materialEmpty->IncrementReferenceCount();
		}

		// lighting info
		if ( Mod_LoadSurfaceLighting( pLighting, in, lh.GetMap()->lightdata ) )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_HASLIGHTSYTLES;
		}

		// set the drawing flags flag
		if ( pTex->flags & SURF_NOLIGHT )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NOLIGHT;
		}
		
		if ( pTex->flags & SURF_NOSHADOWS )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NOSHADOWS;
		}

		if ( pTex->flags & SURF_WARP )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_WATERSURFACE;
		}

		if ( pTex->flags & SURF_SKY )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_SKY;
		}

		if ( pTex->flags & SURF_NOPAINT )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NOPAINT;
		}

        di = in->dispinfo;
		out2->pDispInfo = NULL;
        if( di != -1 )
        {
//			out->origSurfaceID = in->origFace;
			MSurf_Flags( surfID ) |= SURFDRAW_HAS_DISP;
        }
		else
		{
			// non-displacement faces shouldn't come out of VBSP if they have nodraw.
			Assert( !(pTex->flags & SURF_NODRAW) );

			out1->prims.numPrims = in->GetNumPrims();
			out1->prims.firstPrimID = in->firstPrimID;
			if ( in->GetNumPrims() )
			{
				MSurf_Flags( surfID ) |= SURFDRAW_HAS_PRIMS;
				mprimitive_t *pPrim = &pBrushData->primitives[in->firstPrimID];
				if ( pPrim->vertCount > 0 )
				{
					MSurf_Flags( surfID ) |= SURFDRAW_DYNAMIC;
				}
			}
		}
		
		// No shadows on the surface to start with
		out2->m_ShadowDecals = SHADOW_DECAL_HANDLE_INVALID;
		out2->decals = WORLD_DECAL_HANDLE_INVALID;

		// No overlays on the surface to start with
		out2->m_nFirstOverlayFragment = OVERLAY_FRAGMENT_INVALID;

		CalcSurfaceExtents( lh, surfID );

		// check and purge needless lightmaps
		CheckSurfaceLighting( surfID, pBrushData );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *node - 
//			*parent - 
// Output : void Mod_SetParent
//-----------------------------------------------------------------------------
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents >= 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}


//-----------------------------------------------------------------------------
// Mark an entire subtree as being too small to bother with
//-----------------------------------------------------------------------------
static void MarkSmallNode( mnode_t *node )
{
	if (node->contents >= 0)
		return;
	node->contents = -2;
	MarkSmallNode (node->children[0]);
	MarkSmallNode (node->children[1]);
}

static void CheckSmallVolumeDifferences( mnode_t *pNode, const Vector &parentSize )
{
	if (pNode->contents >= 0)
		return;

	Vector delta;
	VectorSubtract( parentSize, pNode->m_vecHalfDiagonal, delta );

	if ((delta.x < 5) && (delta.y < 5) && (delta.z < 5))
	{
		pNode->contents = -3;
		CheckSmallVolumeDifferences( pNode->children[0], parentSize );
		CheckSmallVolumeDifferences( pNode->children[1], parentSize );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadNodes( void )
{
	Vector mins( 0, 0, 0 ), maxs( 0, 0, 0 );
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	CMapLoadHelper lh( LUMP_NODES );

	in = (dnode_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mnode_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "nodes" ) );

	lh.GetMap()->nodes = out;
	lh.GetMap()->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			mins[j] = in->mins[j];
			maxs[j] = in->maxs[j];
		}
	
		VectorAdd( mins, maxs, out->m_vecCenter );
		out->m_vecCenter *= 0.5f;
		VectorSubtract( maxs, out->m_vecCenter, out->m_vecHalfDiagonal );

		p = in->planenum;
		out->plane = lh.GetMap()->planes + p;

		out->firstsurface = in->firstface;
		out->numsurfaces = in->numfaces;
		out->area = in->area;
		out->contents = -1;	// differentiate from leafs

		for (j=0 ; j<2 ; j++)
		{
			p = in->children[j];
			if (p >= 0)
				out->children[j] = lh.GetMap()->nodes + p;
			else
				out->children[j] = (mnode_t *)(lh.GetMap()->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (lh.GetMap()->nodes, NULL);	// sets nodes and leafs

	// Check for small-area parents... no culling below them...
	mnode_t *pNode = lh.GetMap()->nodes;
	for ( i=0 ; i<count ; ++i, ++pNode)
	{
		if (pNode->contents == -1)
		{
			if ((pNode->m_vecHalfDiagonal.x <= 50) && (pNode->m_vecHalfDiagonal.y <= 50) && 
				(pNode->m_vecHalfDiagonal.z <= 50))
			{
				// Mark all children as being too small to bother with...
				MarkSmallNode( pNode->children[0] );
				MarkSmallNode( pNode->children[1] );
			}
			else
			{
				CheckSmallVolumeDifferences( pNode->children[0], pNode->m_vecHalfDiagonal );
				CheckSmallVolumeDifferences( pNode->children[1], pNode->m_vecHalfDiagonal );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadLeafs_Version_0( CMapLoadHelper &lh )
{
	Vector mins( 0, 0, 0 ), maxs( 0, 0, 0 );
	dleaf_version_0_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (dleaf_version_0_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mleaf_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "leafs" ) );

	lh.GetMap()->leafs = out;
	lh.GetMap()->numleafs = count;

	// one sample per leaf
	lh.GetMap()->m_pLeafAmbient = (mleafambientindex_t *)Hunk_AllocName( count * sizeof(*lh.GetMap()->m_pLeafAmbient), "LeafAmbient" );
	lh.GetMap()->m_pAmbientSamples = (mleafambientlighting_t *)Hunk_AllocName( count * sizeof(*lh.GetMap()->m_pAmbientSamples), "LeafAmbientSamples" );
	mleafambientindex_t *pTable = lh.GetMap()->m_pLeafAmbient;
	mleafambientlighting_t *pSamples = lh.GetMap()->m_pAmbientSamples;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			mins[j] = in->mins[j];
			maxs[j] = in->maxs[j];
		}

		VectorAdd( mins, maxs, out->m_vecCenter );
		out->m_vecCenter *= 0.5f;
		VectorSubtract( maxs, out->m_vecCenter, out->m_vecHalfDiagonal );

		pTable[i].ambientSampleCount = 1;
		pTable[i].firstAmbientSample = i;
		pSamples[i].x = pSamples[i].y = pSamples[i].z = 128;
		pSamples[i].pad = 0;
		Q_memcpy( &pSamples[i].cube, &in->m_AmbientLighting, sizeof(pSamples[i].cube) );


		p = in->contents;
		out->contents = p;

		out->cluster = in->cluster;
		out->area = in->area;
		out->flags = in->flags;
/*
		out->firstmarksurface = lh.GetMap()->marksurfaces + in->firstleafface;
*/
		out->firstmarksurface = in->firstleafface;
		out->nummarksurfaces = in->numleaffaces;
		out->parent = NULL;
		
		out->dispCount = 0;

		out->leafWaterDataID = in->leafWaterDataID;
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadLeafs_Version_1( CMapLoadHelper &lh, CMapLoadHelper &ambientLightingLump, CMapLoadHelper &ambientLightingTable )
{
	Vector mins( 0, 0, 0 ), maxs( 0, 0, 0 );
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (dleaf_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mleaf_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "leafs" ) );

	lh.GetMap()->leafs = out;
	lh.GetMap()->numleafs = count;

	if ( ambientLightingLump.LumpVersion() != LUMP_LEAF_AMBIENT_LIGHTING_VERSION || ambientLightingTable.LumpSize() == 0 )
	{
		// convert from previous version
		CompressedLightCube *inLightCubes = NULL;
		if ( ambientLightingLump.LumpSize() )
		{
			inLightCubes = ( CompressedLightCube * )ambientLightingLump.LumpBase();
			Assert( ambientLightingLump.LumpSize() % sizeof( CompressedLightCube ) == 0 );
			Assert( ambientLightingLump.LumpSize() / sizeof( CompressedLightCube ) == lh.LumpSize() / sizeof( dleaf_t ) );
		}
		lh.GetMap()->m_pLeafAmbient = (mleafambientindex_t *)Hunk_AllocName( count * sizeof(*lh.GetMap()->m_pLeafAmbient), "LeafAmbient" );
		lh.GetMap()->m_pAmbientSamples = (mleafambientlighting_t *)Hunk_AllocName( count * sizeof(*lh.GetMap()->m_pAmbientSamples), "LeafAmbientSamples" );
		mleafambientindex_t *pTable = lh.GetMap()->m_pLeafAmbient;
		mleafambientlighting_t *pSamples = lh.GetMap()->m_pAmbientSamples;
		Vector gray(0.5, 0.5, 0.5);
		ColorRGBExp32 grayColor;
		VectorToColorRGBExp32( gray, grayColor );
		for ( i = 0; i < count; i++ )
		{
			pTable[i].ambientSampleCount = 1;
			pTable[i].firstAmbientSample = i;
			pSamples[i].x = pSamples[i].y = pSamples[i].z = 128;
			pSamples[i].pad = 0;
			if ( inLightCubes )
			{
				Q_memcpy( &pSamples[i].cube, &inLightCubes[i], sizeof(pSamples[i].cube) );
			}
			else
			{
				for ( j = 0; j < 6; j++ )
				{
					pSamples[i].cube.m_Color[j] = grayColor;
				}
			}
		}
	}
	else
	{
		Assert( ambientLightingLump.LumpSize() % sizeof( dleafambientlighting_t ) == 0 );
		Assert( ambientLightingTable.LumpSize() % sizeof( dleafambientindex_t ) == 0 );
		Assert((ambientLightingTable.LumpSize() / sizeof(dleafambientindex_t)) == (unsigned)count);	// should have one of these per leaf
		lh.GetMap()->m_pLeafAmbient = (mleafambientindex_t *)Hunk_AllocName( ambientLightingTable.LumpSize(), "LeafAmbient" );
		lh.GetMap()->m_pAmbientSamples = (mleafambientlighting_t *)Hunk_AllocName( ambientLightingLump.LumpSize(), "LeafAmbientSamples" );
		Q_memcpy( lh.GetMap()->m_pLeafAmbient, ambientLightingTable.LumpBase(), ambientLightingTable.LumpSize() );
		Q_memcpy( lh.GetMap()->m_pAmbientSamples, ambientLightingLump.LumpBase(), ambientLightingLump.LumpSize() );
	}


	for ( i=0 ; i<count ; i++, in++, out++ )
	{
		for (j=0 ; j<3 ; j++)
		{
			mins[j] = in->mins[j];
			maxs[j] = in->maxs[j];
		}

		VectorAdd( mins, maxs, out->m_vecCenter );
		out->m_vecCenter *= 0.5f;
		VectorSubtract( maxs, out->m_vecCenter, out->m_vecHalfDiagonal );

		p = in->contents;
		out->contents = p;

		out->cluster = in->cluster;
		out->area = in->area;
		out->flags = in->flags;
/*
		out->firstmarksurface = lh.GetMap()->marksurfaces + in->firstleafface;
*/
		out->firstmarksurface = in->firstleafface;
		out->nummarksurfaces = in->numleaffaces;
		out->parent = NULL;
		
		out->dispCount = 0;

		out->leafWaterDataID = in->leafWaterDataID;
	}	
}

void Mod_LoadLeafs( void )
{
	CMapLoadHelper lh( LUMP_LEAFS );

	switch( lh.LumpVersion() )
	{
	case 0:
		Mod_LoadLeafs_Version_0( lh );
		break;
	case 1:
		if( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE &&
	  		  CMapLoadHelper::LumpSize( LUMP_LEAF_AMBIENT_LIGHTING_HDR ) > 0 )
		{
			CMapLoadHelper mlh( LUMP_LEAF_AMBIENT_LIGHTING_HDR );
			CMapLoadHelper mlhTable( LUMP_LEAF_AMBIENT_INDEX_HDR );
			Mod_LoadLeafs_Version_1( lh, mlh, mlhTable );
		}
		else
		{
			CMapLoadHelper mlh( LUMP_LEAF_AMBIENT_LIGHTING );
			CMapLoadHelper mlhTable( LUMP_LEAF_AMBIENT_INDEX );
			Mod_LoadLeafs_Version_1( lh, mlh, mlhTable ); 
		}
		break;
	default:
		Assert( 0 );
		Error( "Unknown LUMP_LEAFS version\n" );
		break;
	}

	worldbrushdata_t *pMap = lh.GetMap();
	cleaf_t *pCLeaf = GetCollisionBSPData()->map_leafs.Base();
	for ( int i = 0; i < pMap->numleafs; i++ )
	{
		pMap->leafs[i].dispCount = pCLeaf[i].dispCount;
		pMap->leafs[i].dispListStart = pCLeaf[i].dispListStart;
	}
	// HACKHACK: Copy over the shared global list here.  Hunk_Alloc a copy?
	pMap->m_pDispInfoReferences = GetCollisionBSPData()->map_dispList.Base();
	pMap->m_nDispInfoReferences = GetCollisionBSPData()->numdisplist;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadLeafWaterData( void )
{
	dleafwaterdata_t *in;
	mleafwaterdata_t *out;
	int count, i;

	CMapLoadHelper lh( LUMP_LEAFWATERDATA );

	in = (dleafwaterdata_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mleafwaterdata_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "leafwaterdata" ) );

	lh.GetMap()->leafwaterdata = out;
	lh.GetMap()->numleafwaterdata = count;
	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->minZ = in->minZ;
		out->surfaceTexInfoID = in->surfaceTexInfoID;
		out->surfaceZ = in->surfaceZ;
		out->firstLeafIndex = -1;
	}
	if ( count == 1 )
	{
		worldbrushdata_t *brush = lh.GetMap();
		for ( i = 0; i < brush->numleafs; i++ )
		{
			if ( brush->leafs[i].leafWaterDataID >= 0 )
			{
				brush->leafwaterdata[0].firstLeafIndex = i;
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadCubemapSamples( void )
{
	char textureName[512];
	char loadName[ MAX_PATH ];
	dcubemapsample_t *in;
	mcubemapsample_t *out;
	int count, i;

	CMapLoadHelper lh( LUMP_CUBEMAPS );

	V_StripExtension( lh.GetLoadName(), loadName, sizeof(loadName) );

	in = (dcubemapsample_t *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadCubemapSamples: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	out = (mcubemapsample_t *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "cubemapsample" ) );

	lh.GetMap()->m_pCubemapSamples = out;
	lh.GetMap()->m_nCubemapSamples = count;

	// We have separate HDR versions of the textures.  In order to deal with this,
	// we have blahenvmap.hdr.vtf and blahenvmap.vtf.
	char *pHDRExtension = "";
	if( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
	{
		pHDRExtension = ".hdr";
	}

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->origin.Init( ( float )in->origin[0], ( float )in->origin[1], ( float )in->origin[2] );
		out->size = in->size;
		Q_snprintf( textureName, sizeof( textureName ), "%s/c%d_%d_%d%s", loadName, ( int )in->origin[0], 
			( int )in->origin[1], ( int )in->origin[2], pHDRExtension );
		out->pTexture = materials->FindTexture( textureName, TEXTURE_GROUP_CUBE_MAP, true );
		if ( IsErrorTexture( out->pTexture ) )
		{
			if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE )
			{
				Warning( "Couldn't get HDR '%s' -- ", textureName );
				// try non hdr version
				Q_snprintf( textureName, sizeof( textureName ), "%s/c%d_%d_%d", loadName, ( int )in->origin[0], 
							( int )in->origin[1], ( int )in->origin[2]);
				Warning( "Trying non HDR '%s'\n", textureName);
				out->pTexture = materials->FindTexture( textureName, TEXTURE_GROUP_CUBE_MAP, true );
			}
			if ( IsErrorTexture( out->pTexture ) )
			{
				Q_snprintf( textureName, sizeof( textureName ), "%s/cubemapdefault", loadName );
				out->pTexture = materials->FindTexture( textureName, TEXTURE_GROUP_CUBE_MAP, true );
				if ( IsErrorTexture( out->pTexture ) )
				{
					out->pTexture = materials->FindTexture( "engine/defaultcubemap", TEXTURE_GROUP_CUBE_MAP, true );
				}
				Warning( "Failed, using default cubemap '%s'\n", out->pTexture->GetName() );
			}
		}
		out->pTexture->IncrementReferenceCount();
	}

	CMatRenderContextPtr pRenderContext( materials );

	if ( count )
	{
		pRenderContext->BindLocalCubemap( lh.GetMap()->m_pCubemapSamples[0].pTexture );
	}
	else
	{
		if ( CommandLine()->CheckParm( "-requirecubemaps" ) )
		{
			Sys_Error( "Map \"%s\" does not have cubemaps!", lh.GetMapPathName() );
		}

		ITexture *pTexture;
		Q_snprintf( textureName, sizeof( textureName ), "%s/cubemapdefault", loadName );
		pTexture = materials->FindTexture( textureName, TEXTURE_GROUP_CUBE_MAP, true );
		if ( IsErrorTexture( pTexture ) )
		{
			pTexture = materials->FindTexture( "engine/defaultcubemap", TEXTURE_GROUP_CUBE_MAP, true );
		}
		pTexture->IncrementReferenceCount();
		pRenderContext->BindLocalCubemap( pTexture );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadSimpleWorldModel( const char *pMapBaseName )
{
#if defined( CSTRIKE15 )
	// We only load the world imposter models for specific maps on cstrike15
	if( !( V_stristr( pMapBaseName, "de_lake" ) ||
		   V_stristr( pMapBaseName, "de_stmarc" ) ||
		   V_stristr( pMapBaseName, "de_aztec" ) ) )
	{
		return;
	}
#else
	// We only load the world imposter models for multiplayer maps on consoles
	// Note: This seems super-sketchy, but apparently we use the map name to decide if we are co-op or not in portal 2.
	if ( !V_stristr( pMapBaseName, "mp_coop_" ) && IsGameConsole() )
	{
		return;
	}
#endif

	char modelPath[MAX_PATH];
	V_snprintf( modelPath, MAX_PATH, "models/maps/%s/simpleworldmodel.mdl", pMapBaseName );
	char modelPathWater[MAX_PATH];
	V_snprintf( modelPathWater, MAX_PATH, "models/maps/%s/simpleworldmodel_water.mdl", pMapBaseName );

	IModelLoader::REFERENCETYPE referenceType = IModelLoader::FMODELLOADER_SIMPLEWORLD;
	g_pSimpleWorldModel = g_ModelLoader.GetModelForName( modelPath, referenceType );
	g_pSimpleWorldModelWater = g_ModelLoader.GetModelForName( modelPathWater, referenceType );

	if ( !g_pSimpleWorldModel )
	{
		// This is BAD: it implies an image-building failure, and will cause huge perf regressions!
		Warning("\n\n###########################################\n"
					"## !!FAILED TO LOAD SIMPLE WORLD MODEL!! ##\n"
					"##        (perf will be terrible)        ##\n"
					"##           (image is broken)           ##\n"
					"###########################################\n\n\n" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadLeafMinDistToWater( void )
{
	CMapLoadHelper lh( LUMP_LEAFMINDISTTOWATER );

	unsigned short *pTmp = ( unsigned short * )lh.LumpBase();

	int i;
	bool foundOne = false;
	for( i = 0; i < ( int )( lh.LumpSize() / sizeof( *pTmp ) ); i++ )
	{
		if( pTmp[i] != 65535 ) // FIXME: make a marcro for this.
		{
			foundOne = true;
			break;
		}
	}
	
	if( !foundOne || lh.LumpSize() == 0 || !g_pMaterialSystemHardwareConfig )
	{
		// We don't bother keeping this if:
		// 1) there is no water in the map
		// 2) we don't have this lump in the bsp file (old bsp file)
		// 3) we aren't going to use it because we are on old hardware.
		lh.GetMap()->m_LeafMinDistToWater = NULL;
	}
	else
	{
		int		count;
		unsigned short	*in;
		unsigned short	*out;

		in = (unsigned short *)lh.LumpBase();
		if (lh.LumpSize() % sizeof(*in))
			Host_Error ("Mod_LoadLeafMinDistToWater: funny lump size in %s", lh.GetMapPathName());
		count = lh.LumpSize() / sizeof(*in);
		out = (unsigned short *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "leafmindisttowater" ) );

		memcpy( out, in, sizeof( out[0] ) * count );
		lh.GetMap()->m_LeafMinDistToWater = out;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Mod_LoadMarksurfaces( void )
{	
	int		i, j, count;
	unsigned short	*in;

	CMapLoadHelper lh( LUMP_LEAFFACES );
	
	in = (unsigned short *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadMarksurfaces: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	SurfaceHandle_t	*tempDiskData = new SurfaceHandle_t[count];

	worldbrushdata_t *pBrushData = lh.GetMap();
	pBrushData->marksurfaces = tempDiskData;
	pBrushData->nummarksurfaces = count;

	// read in the mark surfaces, count out how many we'll actually need to store
	int realCount = 0;
	for ( i=0 ; i<count ; i++)
	{
		j = in[i];
		if (j >= lh.GetMap()->numsurfaces)
			Host_Error ("Mod_LoadMarksurfaces: bad surface number");
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( j, pBrushData );
		tempDiskData[i] = surfID;
		if ( !SurfaceHasDispInfo( surfID ) && !(MSurf_Flags(surfID) & SURFDRAW_NODRAW) )
		{
			realCount++;
		}
	}

	// now allocate the permanent list, and copy the non-terrain, non-nodraw surfs into it
	SurfaceHandle_t *surfList = (SurfaceHandle_t *)Hunk_AllocName( realCount*sizeof(SurfaceHandle_t), va( "%s [%s]", lh.GetLoadName(), "surfacehandle" ) );

	int outCount = 0;
	mleaf_t *pLeaf = pBrushData->leafs;
	for ( i = 0; i < pBrushData->numleafs; i++ )
	{
		int firstMark = outCount;
		int numMark = 0;
		bool foundDetail = false;
		int numMarkNode = 0;
		for ( j = 0; j < pLeaf[i].nummarksurfaces; j++ )
		{
			// write a new copy of the mark surfaces for this leaf, strip out the nodraw & terrain
			SurfaceHandle_t surfID = tempDiskData[pLeaf[i].firstmarksurface+j];
			if ( !SurfaceHasDispInfo( surfID ) && !(MSurf_Flags(surfID) & SURFDRAW_NODRAW) )
			{
				surfList[outCount++] = surfID;
				numMark++;
				Assert(outCount<=realCount);
				if ( MSurf_Flags(surfID) & SURFDRAW_NODE )
				{
					// this assert assures that all SURFDRAW_NODE surfs appear coherently
					Assert( !foundDetail );
					numMarkNode++;
				}
				else
				{
					foundDetail = true;
				}
			}
		}
		// update the leaf count
		pLeaf[i].nummarksurfaces = numMark;
		pLeaf[i].firstmarksurface = firstMark;
		pLeaf[i].nummarknodesurfaces = numMarkNode;
	}

	// write out the compacted array
	pBrushData->marksurfaces = surfList;
	pBrushData->nummarksurfaces = realCount;
	
	// remove the temp copy of the disk data
	delete[] tempDiskData;

	//Msg("Must check %d / %d faces\n", checkCount, pModel->brush.numsurfaces );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pedges - 
//			*loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadSurfedges( medge_t *pedges )
{	
	int		i, count;
	int		*in;
	unsigned short *out;
	
	CMapLoadHelper lh( LUMP_SURFEDGES );

	in = (int *)lh.LumpBase();
	if (lh.LumpSize() % sizeof(*in))
		Host_Error ("Mod_LoadSurfedges: funny lump size in %s", lh.GetMapPathName());
	count = lh.LumpSize() / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		Host_Error ("Mod_LoadSurfedges: bad surfedges count in %s: %i",
		lh.GetMapPathName(), count);
	out = (unsigned short *)Hunk_AllocName( count*sizeof(*out), va( "%s [%s]", lh.GetLoadName(), "surfedges" ) );

	lh.GetMap()->vertindices = out;
	lh.GetMap()->numvertindices = count;

	for ( i=0 ; i<count ; i++)
	{
		int edge = in[i];
		int index = 0;
		if ( edge < 0 )
		{
			edge = -edge;
			index = 1;
		}
		out[i] = pedges[edge].v[index];
	}

	delete[] pedges;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *loadmodel - 
//			*l - 
//			*loadname - 
//-----------------------------------------------------------------------------
void Mod_LoadPlanes( void )
{
	// Don't bother loading them, they're already stored
	s_pMap->planes = GetCollisionBSPData()->map_planes.Base();
	s_pMap->numplanes = GetCollisionBSPData()->numplanes;
}


//-----------------------------------------------------------------------------
// Returns game lump version
//-----------------------------------------------------------------------------
int Mod_GameLumpVersion( int lumpId )
{
	for ( int i = g_GameLumpDict.Count(); --i >= 0; )
	{
		if ( g_GameLumpDict[i].id == lumpId )
		{
			return g_GameLumpDict[i].version;
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Returns game lump size
//-----------------------------------------------------------------------------
int Mod_GameLumpSize( int lumpId )
{
	for ( int i = g_GameLumpDict.Count(); --i >= 0; )
	{
		if ( g_GameLumpDict[i].id == lumpId )
		{
			return g_GameLumpDict[i].uncompressedSize;
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Loads game lumps
//-----------------------------------------------------------------------------
bool Mod_LoadGameLump( int lumpId, void *pOutBuffer, int size )
{
	int i;
	for ( i = g_GameLumpDict.Count(); --i >= 0; )
	{
		if ( g_GameLumpDict[i].id == lumpId )
		{
			break;
		}
	}
	if ( i < 0 )
	{
		// unknown
		return false;
	}

	byte *pData;
	bool bIsCompressed = ( g_GameLumpDict[i].flags & GAMELUMPFLAG_COMPRESSED ) != 0;
	int dataLength;
	int outSize;
	if ( bIsCompressed )
	{
		// lump data length is always original uncompressed size
		// compressed lump data length is determined from next dictionary entry offset
		dataLength = g_GameLumpDict[i].compressedSize;
		outSize = g_GameLumpDict[i].uncompressedSize;
	}
	else
	{
		dataLength = outSize = g_GameLumpDict[i].uncompressedSize;
	}

	if ( size < 0 || size < outSize )
	{
		// caller must supply a buffer that is large enough to hold the data
		return false;
	}

	if ( s_MapBuffer.GetUsed() )
	{
		// data is in memory
		Assert( CMapLoadHelper::GetRefCount() );

		if ( g_GameLumpDict[i].offset + dataLength > (unsigned int)s_MapBuffer.GetUsed() )
		{
			// out of range
			Assert( 0 );
			return false;
		}

		pData = (unsigned char *)s_MapBuffer.GetBase() + g_GameLumpDict[i].offset;
		if ( !bIsCompressed )
		{
			V_memcpy( pOutBuffer, pData, outSize );
			return true;
		}
	}
	else
	{
		// Load file into buffer
		char szNameOnDisk[MAX_PATH];
		GetMapPathNameOnDisk( szNameOnDisk, g_GameLumpFilename, sizeof( szNameOnDisk ) );
		FileHandle_t fileHandle = g_pFileSystem->OpenEx( szNameOnDisk, "rb", IsGameConsole() ? FSOPEN_NEVERINPACK : 0, IsGameConsole() ? "GAME" : NULL );
		if ( fileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			return false;
		}

		g_pFileSystem->Seek( fileHandle, g_GameLumpDict[i].offset, FILESYSTEM_SEEK_HEAD );

		if ( !bIsCompressed )
		{
			// read directly into user's buffer
			bool bOK = ( g_pFileSystem->Read( pOutBuffer, outSize, fileHandle ) > 0 );
			g_pFileSystem->Close( fileHandle );
			return bOK;
		}
		else
		{
			// data is compressed, read into temporary
			pData = (byte *)malloc( dataLength );
			bool bOK = ( g_pFileSystem->Read( pData, dataLength, fileHandle ) > 0 );
			g_pFileSystem->Close( fileHandle );
			if ( !bOK )
			{
				free( pData );
				return false;
			}
		}
	}

	// NOTE: TF2 added support for compressed lumps on PC (see CL#2898466 & CL#2898683 in //Valve mainline), but we'll keep it disabled in CS:GO
#if COMPRESSED_GAMELUMPS_SUPPORTED_ON_PC
	// We'll fall though to here through here if we're compressed
	bool bResult = false;
	if ( !CLZMA::IsCompressed( pData ) || CLZMA::GetActualSize( (unsigned char *)pData ) != g_GameLumpDict[i].uncompressedSize )
	{
		Warning( "Failed loading game lump %i: lump claims to be compressed but metadata does not match\n", lumpId );
	}
	else
	{
		// uncompress directly into caller's buffer
		int outputLength = CLZMA::Uncompress( pData, ( unsigned char * ) pOutBuffer );
		bResult = ( outputLength > 0 && ( unsigned int ) outputLength == g_GameLumpDict[ i ].uncompressedSize );
	}

	if ( !s_MapBuffer.Base() )
	{
		// done with temporary buffer
		free( pData );
	}

	return bResult;
#endif

	// only 360 has compressed gamelumps
	Assert( 0 );
	return false;
}

//-----------------------------------------------------------------------------
// Loads game lump dictionary
//-----------------------------------------------------------------------------
void Mod_LoadGameLumpDict( void )
{
	CMapLoadHelper lh( LUMP_GAME_LUMP );

	// FIXME: This is brittle. If we ever try to load two game lumps
	// (say, in multiple BSP files), the dictionary info I store here will get whacked

	g_GameLumpDict.RemoveAll();
	Q_strncpy( g_GameLumpFilename, lh.GetMapPathName(), sizeof( g_GameLumpFilename ) );

	unsigned int lhSize = (unsigned int)Max( lh.LumpSize(), 0 );
	if ( lhSize >= sizeof( dgamelumpheader_t ) )
	{
		dgamelumpheader_t* pGameLumpHeader = (dgamelumpheader_t*)lh.LumpBase();

		// Ensure (lumpsize * numlumps + headersize) doesn't overflow
		const int nMaxGameLumps = ( INT_MAX - sizeof( dgamelumpheader_t ) ) / sizeof( dgamelump_t );
		if ( pGameLumpHeader->lumpCount < 0 ||
		     pGameLumpHeader->lumpCount > nMaxGameLumps ||
		     sizeof( dgamelumpheader_t ) + sizeof( dgamelump_t ) * pGameLumpHeader->lumpCount > lhSize )
		{
			Warning( "Bogus gamelump header in map, rejecting\n" );
		}
		else
		{
			// Load in lumps
			dgamelump_t* pGameLump = (dgamelump_t*)(pGameLumpHeader + 1);
			for (int i = 0; i < pGameLumpHeader->lumpCount; ++i )
			{
				if ( pGameLump[ i ].fileofs >= 0 &&
					( unsigned int ) pGameLump[ i ].fileofs >= ( unsigned int ) lh.LumpOffset() &&
					( unsigned int ) pGameLump[ i ].fileofs < ( unsigned int ) lh.LumpOffset() + lhSize &&
					pGameLump[ i ].filelen > 0 )
				{
					unsigned int compressedSize = 0;
					if ( i + 1 < pGameLumpHeader->lumpCount &&
						pGameLump[ i + 1 ].fileofs > pGameLump[ i ].fileofs &&
						pGameLump[ i + 1 ].fileofs >= 0 &&
						( unsigned int ) pGameLump[ i + 1 ].fileofs <= ( unsigned int ) lh.LumpOffset() + lhSize )
					{
						compressedSize = ( unsigned int ) pGameLump[ i + 1 ].fileofs - ( unsigned int ) pGameLump[ i ].fileofs;
					}
					else
					{
						compressedSize = ( unsigned int ) lh.LumpOffset() + lhSize - ( unsigned int ) pGameLump[ i ].fileofs;
					}
					g_GameLumpDict.AddToTail( { pGameLump[ i ], compressedSize } );
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Re-Loads all of a model's peer data
//-----------------------------------------------------------------------------
void Mod_TouchAllData( model_t *pModel, int nServerCount )
{
	double t1 = Plat_FloatTime();

	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	virtualmodel_t *pVirtualModel = g_pMDLCache->GetVirtualModel( pModel->studio );

	double t2 = Plat_FloatTime();
	g_flAccumulatedModelLoadTimeVirtualModel += ( t2 - t1 );

	if ( pVirtualModel && nServerCount >= 1 )
	{
		// ensure all sub models get current count to avoid purge
		// mark first to prevent re-entrant issues during possible reload
		// skip self, start at children
		for ( int i=1; i<pVirtualModel->m_group.Count(); ++i )
		{
			MDLHandle_t childHandle = VoidPtrToMDLHandle( pVirtualModel->m_group[i].cache );
			model_t *pChildModel = (model_t *)g_pMDLCache->GetUserData( childHandle );
			if ( pChildModel )
			{
				// child inherits parent reference
				pChildModel->nLoadFlags |= ( pModel->nLoadFlags & IModelLoader::FMODELLOADER_REFERENCEMASK );
				pChildModel->nLoadFlags |= IModelLoader::FMODELLOADER_LOADED;
				pChildModel->nLoadFlags &= ~IModelLoader::FMODELLOADER_LOADED_BY_PRELOAD;
				pChildModel->nServerCount = nServerCount;
			}
		}
	}

	// don't touch all the data
	if ( !mod_forcetouchdata.GetBool() )
		return;

	g_pMDLCache->TouchAllData( pModel->studio );
}

//-----------------------------------------------------------------------------
// Callbacks to get called when various data is loaded or unloaded 
//-----------------------------------------------------------------------------
class CMDLCacheNotify : public IMDLCacheNotify
{
public:
	virtual void OnDataLoaded( MDLCacheDataType_t type, MDLHandle_t handle );
	virtual void OnCombinerPreCache( MDLHandle_t OldHandle, MDLHandle_t NewHandle );
	virtual void OnDataUnloaded( MDLCacheDataType_t type, MDLHandle_t handle );
	virtual bool ShouldSupressLoadWarning( MDLHandle_t handle );

private:
	void ComputeModelFlags( model_t* mod, MDLHandle_t handle );

	// Sets the bounds from the studiohdr 
	void SetBoundsFromStudioHdr( model_t *pModel, MDLHandle_t handle );
};
static CMDLCacheNotify s_MDLCacheNotify;

//-----------------------------------------------------------------------------
// Computes model flags
//-----------------------------------------------------------------------------
void CMDLCacheNotify::ComputeModelFlags( model_t* pModel, MDLHandle_t handle )
{
	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( handle );

	// Clear out those flags we set...
	pModel->flags &= ~(MODELFLAG_TRANSLUCENT_TWOPASS | MODELFLAG_VERTEXLIT | 
		MODELFLAG_TRANSLUCENT | MODELFLAG_MATERIALPROXY | MODELFLAG_FRAMEBUFFER_TEXTURE |
		MODELFLAG_STUDIOHDR_USES_FB_TEXTURE | MODELFLAG_STUDIOHDR_USES_BUMPMAPPING | 
		MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP | MODELFLAG_STUDIOHDR_IS_STATIC_PROP | MODELFLAG_STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY );

	bool bForceOpaque = (pStudioHdr->flags & STUDIOHDR_FLAGS_FORCE_OPAQUE) != 0;

	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS )
	{
		pModel->flags |= MODELFLAG_TRANSLUCENT_TWOPASS;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_USES_FB_TEXTURE )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_USES_FB_TEXTURE;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_IS_STATIC_PROP;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_USES_BUMPMAPPING )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_USES_BUMPMAPPING;
	}
	if ( pStudioHdr->flags & STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_USES_ENV_CUBEMAP )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_AMBIENT_BOOST )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_AMBIENT_BOOST;
	}
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_DO_NOT_CAST_SHADOWS )
	{
		pModel->flags |= MODELFLAG_STUDIOHDR_DO_NOT_CAST_SHADOWS;
	}

	IMaterial *materials[ 128 ];
	int materialCount = Mod_GetModelMaterials( pModel, ARRAYSIZE( materials ), materials );

	for ( int i = 0; i < materialCount; ++i )
	{
		IMaterial *pMaterial = materials[ i ];
		if ( !pMaterial )
			continue;

		if ( pMaterial->IsVertexLit() )
		{
			pModel->flags |= MODELFLAG_VERTEXLIT;
		}

		if ( !bForceOpaque && pMaterial->IsTranslucent() )
		{
			//Msg("Translucent material %s for model %s\n", pLODData->ppMaterials[i]->GetName(), pModel->name );
			pModel->flags |= MODELFLAG_TRANSLUCENT;
		}

		if ( pMaterial->HasProxy() )
		{
			pModel->flags |= MODELFLAG_MATERIALPROXY;
		}

		if ( pMaterial->NeedsPowerOfTwoFrameBufferTexture( false ) ) // The false checks if it will ever need the frame buffer, not just this frame
		{
			pModel->flags |= MODELFLAG_FRAMEBUFFER_TEXTURE;
		}
	}
}

//-----------------------------------------------------------------------------
// Sets the bounds from the studiohdr 
//-----------------------------------------------------------------------------
void CMDLCacheNotify::SetBoundsFromStudioHdr( model_t *pModel, MDLHandle_t handle )
{
	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( handle );
	VectorCopy( pStudioHdr->hull_min, pModel->mins );
	VectorCopy( pStudioHdr->hull_max, pModel->maxs );
	pModel->radius = 0.0f;
	for ( int i = 0; i < 3; i++ )
	{
		if ( fabs(pModel->mins[i]) > pModel->radius )
		{
			pModel->radius = fabs(pModel->mins[i]);
		}

		if ( fabs(pModel->maxs[i]) > pModel->radius )
		{
			pModel->radius = fabs(pModel->maxs[i]);
		}
	}
}

//-----------------------------------------------------------------------------
// Callbacks to get called when various data is loaded or unloaded 
//-----------------------------------------------------------------------------
void CMDLCacheNotify::OnDataLoaded( MDLCacheDataType_t type, MDLHandle_t handle )
{
	model_t *pModel = (model_t*)g_pMDLCache->GetUserData( handle );

	// NOTE: A NULL model can occur for dependent MDLHandle_ts (like .ani files)
	if ( !pModel )
		return;

	switch( type )
	{
	case MDLCACHE_STUDIOHDR:
		{
			// FIXME: This code only works because it assumes StudioHdr
			// is loaded before VCollide.
			SetBoundsFromStudioHdr( pModel, handle );
		}
		break;

	case MDLCACHE_VCOLLIDE:
		{
			SetBoundsFromStudioHdr( pModel, handle );

			// Expand the model bounds to enclose the collision model (should be done in studiomdl)
			vcollide_t *pCollide = g_pMDLCache->GetVCollide( handle );
			if ( pCollide )
			{
				Vector mins, maxs;
				physcollision->CollideGetAABB( &mins, &maxs, pCollide->solids[0], vec3_origin, vec3_angle );
				AddPointToBounds( mins, pModel->mins, pModel->maxs );
				AddPointToBounds( maxs, pModel->mins, pModel->maxs );
			}
		}
		break;

	case MDLCACHE_STUDIOHWDATA:
		{
			ComputeModelFlags( pModel, handle );

#if !defined( DEDICATED )
			if ( g_ModelLoader.m_bAllowWeaponModelCache )
			{
				int nMapIndex = g_ModelLoader.m_WeaponModelCache.Find( pModel );
				if ( nMapIndex != g_ModelLoader.m_WeaponModelCache.InvalidIndex() )
				{
					g_ModelLoader.m_WeaponModelCache[nMapIndex]->m_bStudioHWDataResident = true;
				}
			}
#endif
		}
		break;
	}
}


void CMDLCacheNotify::OnCombinerPreCache( MDLHandle_t OldHandle, MDLHandle_t NewHandle )
{
	model_t *pModel = ( model_t * )g_pMDLCache->GetUserData( OldHandle );
	if ( !pModel )
	{
		Assert( 0 );
		return;
	}

	pModel->studio = NewHandle;
	g_pMDLCache->SetUserData( OldHandle, NULL );
	g_pMDLCache->SetUserData( NewHandle, pModel );
}


void CMDLCacheNotify::OnDataUnloaded( MDLCacheDataType_t type, MDLHandle_t handle )
{
#if defined( PLATFORM_WINDOWS_PC ) || defined( DEDICATED )
	// NOTE: This is because CMDLCache::UnloadQueuedHardwareData() FUNDAMENTALLY broke the modelcache due to its
	// need to break the "flush" dependency. I did not investigate the validity of WHY THAT needed to be done.
	// Since CMDLCache::ShutdownStudioData() breaks the dependency, the higher code does the m_MDLDict.RemoveAt( handle );
	// not realizing the flush has been deferred, along comes the flush later and all sorts of code that expected
	// the MDLHandle_t to be valid (now it's invalid) via the removal from underlying dictionary and code crashes.
	return;
#endif

#if !defined( DEDICATED )
	if ( g_ModelLoader.m_bAllowWeaponModelCache && type == MDLCACHE_STUDIOHWDATA )
	{
		model_t *pModel = (model_t*)g_pMDLCache->GetUserData( handle );
		if ( pModel )
		{
			int nMapIndex = g_ModelLoader.m_WeaponModelCache.Find( pModel );
			if ( nMapIndex != g_ModelLoader.m_WeaponModelCache.InvalidIndex() )
			{
				g_ModelLoader.m_WeaponModelCache[nMapIndex]->m_bStudioHWDataResident = false;
			}
		}
	}
#endif
}

bool CMDLCacheNotify::ShouldSupressLoadWarning( MDLHandle_t handle )
{
#if !defined( DEDICATED )
	if ( g_ModelLoader.m_bAllowWeaponModelCache )
	{
		// the QL wants to warn about loading data outside its awareness
		// weapon models are explicitly prevented from the QL path
		model_t *pModel = (model_t*)g_pMDLCache->GetUserData( handle );
		if ( pModel )
		{
			int nMapIndex = g_ModelLoader.m_WeaponModelCache.Find( pModel );
			if ( nMapIndex != g_ModelLoader.m_WeaponModelCache.InvalidIndex() )
			{
				// model is part of weapon model cache
				// any QL load warnings are benign and should be suppressed
				return true;
			}
		}
	}
#endif

	return false;
}

//-----------------------------------------------------------------------------
// Hooks the cache notify into the MDL cache system 
//-----------------------------------------------------------------------------
void ConnectMDLCacheNotify( )
{
	g_pMDLCache->SetCacheNotify( &s_MDLCacheNotify );
}

void DisconnectMDLCacheNotify( )
{
	g_pMDLCache->SetCacheNotify( NULL );
}

//-----------------------------------------------------------------------------
// Initialize studiomdl state
//-----------------------------------------------------------------------------
void InitStudioModelState( model_t *pModel )
{
	Assert( pModel->type == mod_studio );

	if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_STUDIOHDR ) )
	{
		s_MDLCacheNotify.OnDataLoaded( MDLCACHE_STUDIOHDR, pModel->studio );
	}
	if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_STUDIOHWDATA ) )
	{
		s_MDLCacheNotify.OnDataLoaded( MDLCACHE_STUDIOHWDATA, pModel->studio );
	}
	if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_VCOLLIDE ) )
	{
		s_MDLCacheNotify.OnDataLoaded( MDLCACHE_VCOLLIDE, pModel->studio );
	}
}

//-----------------------------------------------------------------------------
// Resource loading for models
//-----------------------------------------------------------------------------
class CResourcePreloadModel : public CResourcePreload
{
	static void QueuedLoaderMapCallback( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
	{
		if ( loaderError == LOADERERROR_NONE )
		{
			// 360 mounts its bsp entirely into memory
			// this data is discarded at the conclusion of the entire load process
			Assert( CMapLoadHelper::GetRefCount() == 0 );
			CMapLoadHelper::InitFromMemory( (model_t *)pContext, pData, nSize );
		}
	}

	virtual bool CreateResource( const char *pName )
	{
		modtype_t modType = g_ModelLoader.GetTypeFromName( pName );

		// each model type resource has entirely differnt schemes for loading/creating
		if ( modType == mod_brush )
		{
			// expect to be the map bsp model
			MEM_ALLOC_CREDIT_( "CResourcePreloadModel(BSP)" );
			model_t *pMapModel = g_ModelLoader.FindModelNoCreate( pName );
			if ( pMapModel )
			{
				Assert( CMapLoadHelper::GetRefCount() == 0 );

				// 360 reads its specialized bsp into memory up to the pack lump, guaranteed last
				// the real size of the i/o operation is up to pack lump
				CMapLoadHelper::Init( pMapModel, pMapModel->szPathName );
				int nBytesToRead = CMapLoadHelper::LumpOffset( LUMP_PAKFILE );
				CMapLoadHelper::Shutdown();

				void *pTargetData = NULL;
				Assert( ( s_MapBuffer.GetUsed() == 0 ) && ( s_MapBuffer.GetMaxSize() >= nBytesToRead ) );
				if ( ( ( s_MapBuffer.GetUsed() == 0 ) && ( s_MapBuffer.GetMaxSize() >= nBytesToRead ) ) )
					pTargetData = s_MapBuffer.Alloc( nBytesToRead );

				// create a loader job to perform i/o operation to mount the .bsp
				char szNameOnDisk[MAX_PATH];
				GetMapPathNameOnDisk( szNameOnDisk, pMapModel->szPathName, sizeof( szNameOnDisk ) );
				LoaderJob_t loaderJobBSP;
				loaderJobBSP.m_pFilename = szNameOnDisk;
				loaderJobBSP.m_pPathID = "GAME";
				loaderJobBSP.m_pCallback = QueuedLoaderMapCallback;
				loaderJobBSP.m_pContext = (void *)pMapModel;
				loaderJobBSP.m_pTargetData = pTargetData;
				loaderJobBSP.m_nBytesToRead = nBytesToRead;
				loaderJobBSP.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
				g_pQueuedLoader->AddJob( &loaderJobBSP );

				bool bPreventAIN = false;
				const char *pGame = V_UnqualifiedFileName( com_gamedir );
				bPreventAIN = StringHasPrefix( pGame, "csgo" ) || 
								StringHasPrefix( pGame, "cstrike" ) || 
								StringHasPrefix( pGame, "portal2" ) || 
								StringHasPrefix( pGame, "left4dead" );
				if ( !bPreventAIN )
				{
					// create an anonymous job to perform i/o operation to mount the .ain
					// the .ain gets claimed later
					char szLoadName[MAX_PATH];
					V_FileBase( pMapModel->szPathName, szLoadName, sizeof( szLoadName ) );
					V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "maps/graphs/%s" PLATFORM_EXT ".ain", szLoadName );
					LoaderJob_t loaderJobAIN;
					loaderJobAIN.m_pFilename = szNameOnDisk;
					loaderJobAIN.m_pPathID = "GAME";
					loaderJobAIN.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
					g_pQueuedLoader->AddJob( &loaderJobAIN );
				}

				return true;
			}
		}
		else if ( modType == mod_studio )
		{
			MEM_ALLOC_CREDIT_( "CResourcePreloadModel(MDL)" );

			char szFilename[MAX_PATH];
			V_ComposeFileName( "models", pName, szFilename, sizeof( szFilename ) );			
	
			// find model or create empty entry
			model_t *pModel = g_ModelLoader.FindModel( szFilename );

			if ( g_ModelLoader.IsModelInWeaponCache( pModel ) )
			{
				// ignore it, these cannot be loaded now
				return true;
			}

			// mark as touched
			pModel->nLoadFlags |= IModelLoader::FMODELLOADER_TOUCHED_BY_PRELOAD;

			if ( pModel->nLoadFlags & ( IModelLoader::FMODELLOADER_LOADED|IModelLoader::FMODELLOADER_LOADED_BY_PRELOAD ) )
			{
				// already loaded or preloaded
				return true;
			}
			
			// the model in not supposed to be in memory
			Assert( pModel->type == mod_bad );

			// set its type
			pModel->type = mod_studio;

			// mark the model so that the normal studio load path can perform a final fixup
			pModel->nLoadFlags |= IModelLoader::FMODELLOADER_LOADED_BY_PRELOAD;

			// setup the new entry for preload to operate
			pModel->studio = g_pMDLCache->FindMDL( pModel->szPathName );

			// the model is not supposed to be in memory
			// if this hits, the mdlcache is out of sync with the modelloder
			// if this hits, the mdlcache has the model, but the modelloader doesn't think so
			// if the refcounts go haywire, bad evil bugs will occur
			Assert( g_pMDLCache->GetRef( pModel->studio ) == 1 );

			g_pMDLCache->SetUserData( pModel->studio, pModel );

			// get it into the cache
			g_pMDLCache->PreloadModel( pModel->studio );
			
			return true;
		}

		// unknown
		return false;
	}

	void PurgeModels( bool bPurgeAll )
	{
		bool bSpew = ( g_pQueuedLoader->GetSpewDetail() & LOADER_DETAIL_PURGES ) != 0;

		// purge any model that was not touched by the preload process
		int iIndex = -1;
		CUtlVector< model_t* > firstList;
		CUtlVector< model_t* > otherList;
		for ( ;; )
		{
			model_t *pModel;
			iIndex = g_ModelLoader.FindNext( iIndex, &pModel );
			if ( iIndex == -1 || !pModel )
			{
				// end of list
				break;
			}
			if ( pModel->type == mod_studio )
			{
				// models that were touched during the preload stay, otherwise purged
				bool bDoPurge = bPurgeAll || !( pModel->nLoadFlags & IModelLoader::FMODELLOADER_TOUCHED_BY_PRELOAD );

				pModel->nLoadFlags &= ~IModelLoader::FMODELLOADER_TOUCHED_BY_PRELOAD;

				if ( bDoPurge )
				{
					if ( bSpew )
					{
						Msg( "CResourcePreloadModel: Purging: %s\n", pModel->szPathName );
					}

					// Models that have virtual models have to unload first to
					// ensure they properly unreference their virtual models.
					if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_VIRTUALMODEL ) )
					{
						firstList.AddToTail( pModel );
					}
					else
					{
						otherList.AddToTail( pModel );
					}
				}
			}
		}

		for ( int i=0; i<firstList.Count(); i++ )
		{
			g_ModelLoader.UnloadModel( firstList[i] );
		}
		for ( int i=0; i<otherList.Count(); i++ )
		{
			g_ModelLoader.UnloadModel( otherList[i] );
		}

		if ( bPurgeAll || !g_pQueuedLoader->IsSameMapLoading() )
		{
			g_pMDLCache->Flush( MDLCACHE_FLUSH_ANIMBLOCK );
		}
	}

	//-----------------------------------------------------------------------------
	// Called before queued loader i/o jobs are actually performed. Must free up memory
	// to ensure i/o requests have enough memory to succeed. The models that were
	// touched by the CreateResource() are the ones to keep, all others get purged.
	//-----------------------------------------------------------------------------
	virtual void PurgeUnreferencedResources()
	{
		PurgeModels( false );
	}

	virtual void PurgeAll()
	{
		PurgeModels( true );
	}

	virtual void OnEndMapLoading( bool bAbort )
	{
		// discard the memory mounted bsp
		CMapLoadHelper::Shutdown();
		Assert( CMapLoadHelper::GetRefCount() == 0 );
	}

#if defined( _PS3 )
	virtual bool RequiresRendererLock()
	{
		return true;
	}
#endif // _PS3
};
static CResourcePreloadModel s_ResourcePreloadModel;

bool ProcessWeaponModelCacheOperations()
{
	return g_ModelLoader.ProcessWeaponModelCacheOperations();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Init( void )
{
	m_Models.RemoveAll();
	m_InlineModels.Purge();

	m_pWorldModel = NULL;
	m_bMapRenderInfoLoaded = false;
	m_bMapHasHDRLighting = false;
	g_bLoadedMapHasBakedPropLighting = false;
	g_bBakedPropLightingStreams3 = false;
	g_bHasIndirectOnlyInLightingStreams = false;
	
	m_worldBrushData.m_pLightingDataStack = &m_WorldLightingDataStack;

	// Make sure we have physcollision and physprop interfaces
	CollisionBSPData_LinkPhysics();

	if ( IsGameConsole() && g_pQueuedLoader )
	{
		g_pQueuedLoader->InstallLoader( RESOURCEPRELOAD_MODEL, &s_ResourcePreloadModel );
	}
	if ( IsGameConsole() )
	{
		s_MapBuffer.Init( "s_MapBuffer", 32*1024*1024, 64*1024 );
	}

#if defined( PLATFORM_WINDOWS_PC ) || defined( DEDICATED )
	// not compatible for any platform but the game consoles due to at least CMDLCache::UnloadQueuedHardwareData() concepts
	m_bAllowWeaponModelCache = false;
	m_bAllowWeaponVertexEviction = false;
	m_bAllowWorldWeaponEviction = false;
#else
	// on for 360 by default
	m_bAllowWeaponModelCache = IsX360() || IsPS3() || CommandLine()->FindParm( "-weaponmodelcache" ) != 0;
	if ( CommandLine()->FindParm( "-noweaponmodelcache" ) != 0 )
	{
		// explicit opt-out
		m_bAllowWeaponModelCache = false;
	}

	m_bAllowWeaponVertexEviction = true;
	if ( CommandLine()->FindParm( "-keepweaponverts" ) != 0 )
	{
		// explicit opt-out
		m_bAllowWeaponVertexEviction = false;
	}

	m_bAllowWorldWeaponEviction = true;
#endif

	// now invalid due to m_Models purge
	m_nNumWeaponsPartialResident = 0;
	m_WeaponModelCache.PurgeAndDeleteElements();

#if !defined( DEDICATED )
	if ( IsGameConsole() && m_bAllowWorldWeaponEviction && g_pMaterialSystem )
	{
		g_pMaterialSystem->AddEndFramePriorToNextContextFunc( ::ProcessWeaponModelCacheOperations );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Shutdown( void )
{
	m_pWorldModel = NULL;

	UnloadAllModels( false );

	g_pMDLCache->UnloadQueuedHardwareData();

	m_ModelPool.Clear();

	if ( IsGameConsole() )
	{
		s_MapBuffer.Term();
	}

	if ( IsGameConsole() && m_bAllowWorldWeaponEviction && g_pMaterialSystem )
	{
		g_pMaterialSystem->RemoveEndFramePriorToNextContextFunc( ::ProcessWeaponModelCacheOperations );
	}
}

int CModelLoader::GetCount( void )
{
	return m_Models.Count();
}

model_t *CModelLoader::GetModelForIndex( int i )
{
	if ( i < 0 || (unsigned)i >= m_Models.Count() )
	{
		return NULL;
	}

	return m_Models[i].modelpointer;
}

//-----------------------------------------------------------------------------
// Purpose: Look up name for model
// Input  : *model - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CModelLoader::GetName( const model_t *pModel )
{
	if ( pModel )
	{
		return pModel->szPathName;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the model, builds entry if not present, always returns a model
// Input  : *name - 
//			referencetype - 
// Output : model_t
//-----------------------------------------------------------------------------
model_t *CModelLoader::FindModel( const char *pName )
{
	if ( !pName || !pName[0] )
	{
		Sys_Error( "CModelLoader::FindModel: NULL name" );
	}

	// inline models are grabbed only from worldmodel
	if ( pName[0] == '*' )
	{
		int modelNum = atoi( pName + 1 );
		if ( !IsWorldModelSet() )
		{
			Warning( "bad inline model number %i, worldmodel not yet setup\n", modelNum );
			return NULL;
		}

		if ( modelNum < 1 || modelNum >= GetNumWorldSubmodels() )
		{
			Warning( "bad inline model number %i\n", modelNum );
			return NULL;
		}
		return &m_InlineModels[modelNum];
	}

	model_t *pModel = NULL;

	// get a handle suitable to use as the model key
	// handles are insensitive to case and slashes
	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( pName );

	int i = m_Models.Find( fnHandle );
	if ( i == m_Models.InvalidIndex() )
	{
		pModel = (model_t *)m_ModelPool.Alloc();
		Assert( pModel );
		memset( pModel, 0, sizeof( model_t ) );

		pModel->fnHandle = fnHandle;

		// Mark that we should load from disk
		pModel->nLoadFlags = FMODELLOADER_NOTLOADEDORREFERENCED;

		// Copy in name and normalize!
		// Various other subsystems fetch this 'object' name to do dictionary lookups, 
		// which are usually case insensitive, but not to slashes or dotslashes.
		Q_strncpy( pModel->szPathName, pName, sizeof( pModel->szPathName ) );
		V_RemoveDotSlashes( pModel->szPathName, '/' );

		//
		// Model censoring for perfect world, banlist loads here
		//
		static struct BannedMDLs_t
		{
			BannedMDLs_t()
			{
				bool bLoadBannedWords = !!CommandLine()->FindParm( "-perfectworld" );
				bLoadBannedWords |= !!CommandLine()->FindParm( "-usebanlist" );
				if ( bLoadBannedWords )
				{
					CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
					if ( g_pFullFileSystem->ReadFile( "banmdls.res", "MOD", buf ) )
					{
						while ( buf.IsValid() )
						{
							char chModel[ 256 ] = {};
							buf.GetString( chModel, sizeof( chModel ) - 1 );
							if ( chModel[0] )
							{
								m_map.AddString( chModel );
#ifdef _DEBUG
								DevMsg( "Banned MDL: %s\n", chModel );
#endif
							}
						}
					}
				}
			}
			CUtlSymbolTable m_map;
		} s_BannedMDLs;

		//
		// Check the banlist and flag models as render disabled
		//
		if ( s_BannedMDLs.m_map.GetNumStrings() > 0 )
		{
			if ( s_BannedMDLs.m_map.Find( pModel->szPathName ).IsValid() )
			{
				COMPILE_TIME_ASSERT( MODELFLAG_RENDER_DISABLED == ENGINE_MODEL_CLIENT_MODELFLAG_RENDER_DISABLED );
				COMPILE_TIME_ASSERT( offsetof( model_t, flags ) == ENGINE_MODEL_CLIENT_MODELT_OFFSET_FLAGS );

				pModel->flags |= MODELFLAG_RENDER_DISABLED;
#ifdef _DEBUG
				DevMsg( "Render disabled for banned MDL: %s\n", pModel->szPathName );
#endif
			}
		}
		
		//
		// Proceed with inserting this model entry
		//
		ModelEntry_t entry;
		entry.modelpointer = pModel;
		m_Models.Insert( fnHandle, entry );

#if !defined( DEDICATED )
		if ( m_bAllowWeaponModelCache )
		{
			// setup for tracking weapon models BEFORE anything else happens
			// need the entries established before any data starts to arrive
			if ( V_stristr( pModel->szPathName, "weapons/v_" ) != NULL )
			{
				// track weapon view models
				int nMapIndex = m_WeaponModelCache.Find( pModel );
				if ( nMapIndex == m_WeaponModelCache.InvalidIndex() )
				{
					nMapIndex = m_WeaponModelCache.Insert( pModel );
					m_WeaponModelCache[nMapIndex] = new ViewWeaponEntry_t( true );
					pModel->flags |= MODELFLAG_VIEW_WEAPON_MODEL;
				}
			}
			else if ( V_stristr( pModel->szPathName, "weapons/w_" ) != NULL )
			{
				// track weapon world models
				int nMapIndex = m_WeaponModelCache.Find( pModel );
				if ( nMapIndex == m_WeaponModelCache.InvalidIndex() )
				{
					nMapIndex = m_WeaponModelCache.Insert( pModel );
					m_WeaponModelCache[nMapIndex] = new ViewWeaponEntry_t( false );
				}
			}
		}
#endif
	}
	else
	{
		pModel = m_Models[i].modelpointer;
	}

	// notify the reslist generator that this model may be referenced later in the level 
	// (does nothing if reslist generation is not enabled)
	MapReslistGenerator().OnModelPrecached( pName );

	Assert( pModel );

	return pModel;
}

//-----------------------------------------------------------------------------
// Purpose: Finds the model, and loads it if it isn't already present.  Updates reference flags
// Input  : *name - 
//			referencetype - 
// Output : model_t
//-----------------------------------------------------------------------------
model_t *CModelLoader::GetModelForName( const char *name, REFERENCETYPE referencetype )
{
	// find or build new entry
	model_t *model = FindModel( name );
	if ( !model )
		return NULL;

	// touch and load if not present
	model_t *retval = LoadModel( model, &referencetype );

	return retval;
}


//-----------------------------------------------------------------------------
// Purpose: Add a reference to the model in question
// Input  : *name - 
//			referencetype - 
//-----------------------------------------------------------------------------
model_t *CModelLoader::ReferenceModel( const char *name, REFERENCETYPE referencetype )
{
	model_t *model = FindModel( name );

	model->nLoadFlags |= referencetype;

	return model;
}

static void QueuedLoaderBeginMapLoadingCallback( int nStage )
{
	if ( IsGameConsole() )
	{
		// unload lightmap textures before loading the next map (PC does this in CMatLightmaps::BeginLightmapAllocation)
		g_pMaterialSystem->CleanupLightmaps();
	}

#ifdef _PS3
	// Reclaim the space from unloaded lightmaps and (if the queued loader ran) pre-purged assets not used by the next map
	g_pMaterialSystem->CompactRsxLocalMemory( "BEGIN MAP LOADING" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *entry - 
//			referencetype - 
//-----------------------------------------------------------------------------
model_t	*CModelLoader::LoadModel( model_t *mod, REFERENCETYPE *pReferencetype )
{
	if ( pReferencetype )
	{
		mod->nLoadFlags |= *pReferencetype;
	}

	// during initial load mark the model with an unique session ticket
	// at load end, models that have a mismatch count are considered candidates for purge
	// models that get marked, touch *all* their sub data to ensure the cache is pre-populated
	// and hitches less during gameplay
	bool bTouchAllData = false;
	int nServerCount = Host_GetServerCount();
	if ( mod->nServerCount != nServerCount )
	{
		// server has changed
		mod->nServerCount = nServerCount;
		bTouchAllData = true;
	}

	// Check if the studio model is in cache.
	// The model type will not be set for first time models that need to fall through to the load path.
	// A model that needs a post precache fixup will fall through to the load path.
	if ( mod->type == mod_studio && !( mod->nLoadFlags & FMODELLOADER_LOADED_BY_PRELOAD ) )
	{
		// in cache
		Verify( g_pMDLCache->GetStudioHdr( mod->studio ) != 0 );
		Assert( FMODELLOADER_LOADED & mod->nLoadFlags );

		if ( bTouchAllData )
		{
			// Touch all related .ani files and sub/dependent models
			// only touches once, when server changes
			Mod_TouchAllData( mod, nServerCount );
		}

		return mod;
	}

	// Check if brushes or sprites are loaded
	if ( FMODELLOADER_LOADED & mod->nLoadFlags ) 
	{
		return mod;
	}

	// model needs to be loaded
	double st = Plat_FloatTime();

	// Set the name of the current model we are loading
	V_FileBase( mod->szPathName, m_szBaseName, sizeof( m_szBaseName ) );

	// load the file
	if ( developer.GetInt() > 1 )
	{
		DevMsg( "Loading: %s\n", mod->szPathName );
	}

	mod->type = GetTypeFromName( mod->szPathName );
	if ( developer.GetInt() > 1 )
	{
		DevMsg( "Loading type: %d\n", mod->type );
	}
	if ( mod->type == mod_bad )
	{
		mod->type = mod_studio;
	}

	// finalize the model data
	switch ( mod->type )
	{
	case mod_sprite:
		{
			MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

			double t1 = Plat_FloatTime();
			Sprite_LoadModel( mod );
			double t2 = Plat_FloatTime();
			g_flAccumulatedModelLoadTimeSprite += ( t2 - t1 );
		}
		break;

	case mod_studio:
		{
			MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

			double t1 = Plat_FloatTime();
			Studio_LoadModel( mod, bTouchAllData );
			double t2 = Plat_FloatTime();
			g_flAccumulatedModelLoadTimeStudio += ( t2 - t1 );
		}
		break;

	case mod_brush:
		{
			double t1 = Plat_FloatTime();
			
			if ( developer.GetInt() > 1 )
			{
				DevMsg( "Loading brush, compacting heap...\n" );
			}
			g_pMemAlloc->CompactHeap();
		
			// the training map needs ALL the world weapons at high-res on a display wall
			m_bAllowWorldWeaponEviction = ( V_stristr( m_szBaseName, "training1" ) == NULL );

			// This is necessary on dedicated clients. On listen + dedicated servers, it's called twice.
			// The second invocation is harmless.
			// Add to file system before loading so referenced objects in map can use the filename.
			char szNameOnDisk[MAX_PATH];
			GetMapPathNameOnDisk( szNameOnDisk, mod->szPathName, sizeof( szNameOnDisk ) );
			if ( developer.GetInt() > 1 )
			{
				DevMsg( "Loading map: %s\n", szNameOnDisk );
			}
			g_pFileSystem->AddSearchPath( szNameOnDisk, "GAME", PATH_ADD_TO_HEAD );

			// the map may have explicit texture exclusion
			// the texture state needs to be established before any loading work
			if ( IsGameConsole() || mat_excludetextures.GetBool() )
			{
#if defined( PORTAL2 )
				char szExcludePath[MAX_PATH] = "";
				// PORTAL2: we aren't using per-map excludes, we just need a few textures excluded in SP
				if ( V_stristr( m_szBaseName, "sp_" ) )
				{
					v_snprintf( szExcludePath, sizeof( szExcludePath ), "//MOD/maps/sp_exclude.lst" );
				}
#else
				char szExcludePath[MAX_PATH];
				V_snprintf( szExcludePath, sizeof( szExcludePath ), "//MOD/maps/%s_exclude.lst", m_szBaseName );
#endif
				if ( developer.GetInt() > 1 )
				{
					DevMsg( "Setting excluded textures: %s\n", szExcludePath );
				}
				g_pMaterialSystem->SetExcludedTextures( szExcludePath, m_bAllowWeaponModelCache );
			}

			NotifyHunkBeginMapLoad( m_szBaseName );

			bool bQueuedLoader = false;
			if ( IsGameConsole() )
			{
				// must establish the bsp feature set first to ensure proper state during queued loading
				Map_CheckForHDR( mod, mod->szPathName );

				// Do not optimize map-to-same-map loading in TF
				// FIXME/HACK: this fixes a bug (when shipping Orange Box) where static props would sometimes
				//             disappear when a client disconnects and reconnects to the same map+server
				//             (static prop lighting data persists when loading map A after map A)
				bool bIsTF = !V_stricmp( COM_GetModDirectory(), "tf" );
				bool bIsCSGO = !V_stricmp( COM_GetModDirectory(), "csgo" );
				bool bOptimizeMapReload = !bIsTF && !bIsCSGO;

				// start the queued loading process
				if ( developer.GetInt() > 1 )
				{
					DevMsg( "Loading map: BeginMapLoading...\n" );
				}
				bQueuedLoader = g_pQueuedLoader && g_pQueuedLoader->BeginMapLoading( mod->szPathName, g_pMaterialSystemHardwareConfig->GetHDREnabled(), bOptimizeMapReload, QueuedLoaderBeginMapLoadingCallback );
			}

			if ( !bQueuedLoader )
			{
				if ( IsGameConsole() || mat_excludetextures.GetBool() )
				{
					// the queued loader process needs to own the actual texture update
					g_pMaterialSystem->UpdateExcludedTextures();
				}

				// This needs to get run if the queued loader did not call it:
				QueuedLoaderBeginMapLoadingCallback( 1 );
			}

			if ( developer.GetInt() > 1 )
			{
				DevMsg( "Loading map: BeginLoadingUpdates...\n" );
			}

			BeginLoadingUpdates( MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD );
			g_pFileSystem->BeginMapAccess();
			
			if ( developer.GetInt() > 1 )
			{
				DevMsg( "Loading map: Map_LoadModel...\n" );
			}

			Map_LoadModel( mod );
			g_pFileSystem->EndMapAccess();
	
			double t2 = Plat_FloatTime();
			g_flAccumulatedModelLoadTimeBrush += (t2 - t1);
		}
		break;

	default:
		Assert( 0 );
		break;
	};

	float dt = ( Plat_FloatTime() - st );
	COM_TimestampedLog( "Load of %s took %.3f msec", mod->szPathName, 1000.0f * dt );
	g_flAccumulatedModelLoadTime += dt;

	return mod;
}

//-----------------------------------------------------------------------------
// Purpose: Creates the name of the sprite
//-----------------------------------------------------------------------------
static void BuildSpriteLoadName( const char *pName, char *pOut, int outLen, bool &bIsAVI, bool &bIsBIK )
{
	// If it's a .vmt and they put a path in there, then use the path.
	// Otherwise, use the old method of prepending the sprites directory.
	const char *pExt = V_GetFileExtension( pName );
	bIsAVI = !Q_stricmp( pExt, "avi" );
	bIsBIK = !Q_stricmp( pExt, "bik" );
	bool bIsVMT = !Q_stricmp( pExt, "vmt" );
	if ( ( bIsAVI || bIsBIK || bIsVMT ) && ( strchr( pName, '/' ) || strchr( pName, '\\' ) ) )
	{
		// The material system cannot handle a prepended "materials" dir
		// Keep .avi extensions on the material to load avi-based materials
		if ( bIsVMT )
		{
			const char *pNameStart = pName;
			if ( Q_stristr( pName, "materials/" ) == pName ||
				Q_stristr( pName, "materials\\" ) == pName )
			{
				// skip past materials/
				pNameStart = &pName[10];
			}
			Q_StripExtension( pNameStart, pOut, outLen );
		}
		else
		{
			// name is good as is
			Q_strncpy( pOut, pName, outLen );
		}
	}
	else
	{
		char szBase[MAX_PATH];
		Q_FileBase( pName, szBase, sizeof( szBase ) );
		Q_snprintf( pOut, outLen, "sprites/%s", szBase );
	}
	return;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int
//-----------------------------------------------------------------------------
int CModelLoader::GetModelFileSize( char const *name )
{
	if ( !name || !name[ 0 ] )
		return -1;

	model_t *model = FindModel( name );

	int size = -1;
	if ( Q_stristr( model->szPathName, ".spr" ) || Q_stristr( model->szPathName, ".vmt" ) )
	{
		char spritename[ MAX_PATH ];
		Q_StripExtension( va( "materials/%s", model->szPathName ), spritename, MAX_PATH );
		Q_DefaultExtension( spritename, ".vmt", sizeof( spritename ) );

		size = COM_FileSize( spritename );
	}
	else
	{
		size = COM_FileSize( name );
	}

	return size;
}

//-----------------------------------------------------------------------------
// Purpose: Unmasks the referencetype field for the model
// Input  : *model - 
//			referencetype - 
//-----------------------------------------------------------------------------
void CModelLoader::UnreferenceModel( model_t *model, REFERENCETYPE referencetype )
{
	model->nLoadFlags &= ~referencetype;
}

//-----------------------------------------------------------------------------
// Purpose: Unmasks the specified reference type across all models
// Input  : referencetype - 
//-----------------------------------------------------------------------------
void CModelLoader::UnreferenceAllModels( REFERENCETYPE referencetype )
{
	int				i;
	model_t			*mod;

	// UNDONE: If we ever free a studio model, write code to free the collision data
	// UNDONE: Reference count collision data?

	int c = m_Models.Count();
	for ( i=0 ; i < c ; i++ )
	{
		mod = m_Models[ i ].modelpointer;
		UnreferenceModel( mod, referencetype );
	}
}

void CModelLoader::ReloadFilesInList( IFileList *pFilesToReload )
{
	int c = m_Models.Count();
	for ( int i=0 ; i < c ; i++ )
	{
		model_t	*pModel = m_Models[i].modelpointer;
		
		if ( pModel->type != mod_studio )
			continue;
		
		if ( !IsLoaded( pModel ) )
			continue;
		
		if ( pModel->type != mod_studio )
			continue;
		
		if ( pFilesToReload->IsFileInList( pModel->szPathName ) )
		{
			// Flush out the model cache
			// Don't flush vcollides since the vphysics system currently
			// has no way of indicating they refer to vcollides
			g_pMDLCache->Flush( pModel->studio, (int)(MDLCACHE_FLUSH_ALL & ~(MDLCACHE_FLUSH_IGNORELOCK|MDLCACHE_FLUSH_VCOLLIDE)) );

			MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

			// Get the studiohdr into the cache
			g_pMDLCache->GetStudioHdr( pModel->studio );

			// force the collision to load
			g_pMDLCache->GetVCollide( pModel->studio );
		}
		else
		{
			if ( g_pMDLCache->IsDataLoaded( pModel->studio, MDLCACHE_STUDIOHWDATA ) )
			{
				studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );
				if ( pStudioHdr )
				{
					// Ok, we didn't have to do a full reload, but if any of our materials changed, flush out the studiohwdata because the
					// vertex format may have changed.
					IMaterial *pMaterials[128];
					int nMaterials = g_pStudioRender->GetMaterialList( pStudioHdr, ARRAYSIZE( pMaterials ), &pMaterials[0] );

					for ( int j=0; j < nMaterials; j++ )
					{
						if ( pMaterials[j] && pMaterials[j]->WasReloadedFromWhitelist() )
						{
							g_pMDLCache->Flush( pModel->studio, MDLCACHE_FLUSH_STUDIOHWDATA );
							break;
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: For any models with referencetype blank (if checking), frees all memory associated with the model
// and frees up the models slot
//-----------------------------------------------------------------------------
void CModelLoader::UnloadAllModels( bool bCheckReference )
{
	int				i;
	model_t			*model;

	int c = m_Models.Count();
	for ( i=0 ; i < c ; i++ )
	{
		model = m_Models[ i ].modelpointer;
		if ( bCheckReference )
		{
			if ( model->nLoadFlags & FMODELLOADER_REFERENCEMASK )
			{
				if ( model->type == mod_studio && !IsModelInWeaponCache( model ) )
				{
					g_pMDLCache->MarkAsLoaded( model->studio );
				}
				continue;
			}
		}
		else
		{
			// Wipe current flags
			model->nLoadFlags &= ~FMODELLOADER_REFERENCEMASK;
		}

		if ( IsGameConsole() &&
			g_pQueuedLoader && g_pQueuedLoader->IsMapLoading() &&
			( model->nLoadFlags & FMODELLOADER_LOADED_BY_PRELOAD ) )
		{
			// models preloaded by the queued loader are not initially claimed and MUST remain until the end of the load process
			// unclaimed models get unloaded during the post load purge
			continue;
		}

		if ( model->nLoadFlags & ( FMODELLOADER_LOADED | FMODELLOADER_LOADED_BY_PRELOAD ) )
		{		
			UnloadModel( model );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: For any models with referencetype blank (if checking), frees all memory associated with the model
//  and frees up the models slot
//-----------------------------------------------------------------------------
void CModelLoader::UnloadUnreferencedModels( void )
{
	// unload all unreferenced models
	UnloadAllModels( true );
}


void CModelLoader::AddCompatibilityPath( const char* szNewCompatibilityPath )
{
	g_pFullFileSystem->AddSearchPath( szNewCompatibilityPath, "COMPAT:GAME", PATH_ADD_TO_HEAD );
	compatibility_path_t tNewCompatibilityPath;
	tNewCompatibilityPath.mPath = szNewCompatibilityPath;
	tNewCompatibilityPath.mPathId = "COMPAT:GAME";
	m_vecSzCompatibilityPaths.AddToTail( tNewCompatibilityPath );
}

void CModelLoader::UnMountCompatibilityPaths( void )
{
	FOR_EACH_VEC_BACK( m_vecSzCompatibilityPaths, i )
	{
		DevWarning( "UnMounting content compatibility path [%s] %s.\n", m_vecSzCompatibilityPaths[i].mPathId.Get(), m_vecSzCompatibilityPaths[i].mPath.Get() );
		g_pFullFileSystem->RemoveSearchPath( m_vecSzCompatibilityPaths[i].mPath.Get(), m_vecSzCompatibilityPaths[i].mPathId.Get() );
		m_vecSzCompatibilityPaths.Remove(i);
	}
}

//-----------------------------------------------------------------------------
// Called at the conclusion of loading.
// Frees all memory associated with models (and their materials) that are not
// marked with the current session.
//-----------------------------------------------------------------------------
void CModelLoader::PurgeUnusedModels( void )
{
	int nServerCount = Host_GetServerCount();
	int count = m_Models.Count();
	for ( int i = 0; i < count; i++ )
	{
		model_t *pModel = m_Models[i].modelpointer;
		// Don't purge the simple world model here; it's managed separately
		if ( ( pModel->nLoadFlags & FMODELLOADER_LOADED ) && ( pModel->nServerCount != nServerCount ) && !( pModel->nLoadFlags & FMODELLOADER_SIMPLEWORLD ) )
		{
			// mark as unreferenced
			pModel->nLoadFlags &= ~FMODELLOADER_REFERENCEMASK;
		}
	}

	// unload unreferenced models only
	UnloadAllModels( true );

	// now purge unreferenced materials
	materials->UncacheUnusedMaterials( true );
}


byte *CModelLoader::GetLightstyles( model_t *pModel )
{
	if ( pModel->brush.nLightstyleCount != 0 )
	{
		byte *pLightstyles = &m_LightStyleList[pModel->brush.nLightstyleIndex];
		return pLightstyles;
	}
	return NULL;
}

void CModelLoader::AllocateLightstyles( model_t *pModel, byte *pStyles, int nStyleCount )
{
	unsigned short nLast = m_LightStyleList.Count();
	for ( int i = 0; i < nStyleCount; i++ )
	{
		m_LightStyleList.AddToTail( pStyles[i] );
	}
	pModel->brush.nLightstyleCount = nStyleCount;
	pModel->brush.nLightstyleIndex = nLast;
}

bool Mod_NeedsLightstyleUpdate( model_t *pModel )
{
#ifndef SWDS
	if ( pModel->brush.nLightstyleCount != 0 )
	{
		byte *pLightstyles = g_ModelLoader.GetLightstyles( pModel );
		int nCount = pModel->brush.nLightstyleCount;
		int nLastComputed = pModel->brush.nLightstyleLastComputedFrame;
		for( int i = 0; i < nCount; i++ )
		{
			if( d_lightstyleframe[pLightstyles[i]] > nLastComputed )
				return true;
		}
	}
#endif
	return false;
}

//-----------------------------------------------------------------------------
// Compute whether this submodel uses material proxies or not
//-----------------------------------------------------------------------------
static void Mod_ComputeBrushModelFlags( model_t *mod )
{
	Assert( mod );

	worldbrushdata_t *pBrushData = mod->brush.pShared;
	// Clear out flags we're going to set
	mod->flags &= ~(MODELFLAG_MATERIALPROXY | MODELFLAG_TRANSLUCENT | MODELFLAG_FRAMEBUFFER_TEXTURE | MODELFLAG_TRANSLUCENT_TWOPASS );
	mod->flags = MODELFLAG_HAS_DLIGHT; // force this check the first time

	int i;
	int scount = mod->brush.nummodelsurfaces;
	bool bHasOpaqueSurfaces = false;
	bool bHasTranslucentSurfaces = false;
	mod->brush.nLightstyleIndex = 0;
	mod->brush.nLightstyleCount = 0;

	CUtlVector<byte> lightStyles;
	
	for ( i = 0; i < scount; ++i )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( mod->brush.firstmodelsurface + i, pBrushData );

		// Clear out flags we're going to set
		MSurf_Flags( surfID ) &= ~(SURFDRAW_NOCULL | SURFDRAW_TRANS | SURFDRAW_ALPHATEST | SURFDRAW_NODECALS);

		mtexinfo_t *pTex = MSurf_TexInfo( surfID, pBrushData );
		IMaterial* pMaterial = pTex->material;

		if ( pMaterial->HasProxy() )
		{
			mod->flags |= MODELFLAG_MATERIALPROXY;
		}

		if ( pMaterial->NeedsPowerOfTwoFrameBufferTexture( false ) ) // The false checks if it will ever need the frame buffer, not just this frame
		{
			mod->flags |= MODELFLAG_FRAMEBUFFER_TEXTURE;
		}

		// Deactivate culling if the material is two sided
		if ( pMaterial->IsTwoSided() )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NOCULL;
		}

		if ( (pTex->flags & SURF_TRANS) || pMaterial->IsTranslucent() )
		{
			mod->flags |= MODELFLAG_TRANSLUCENT;
			MSurf_Flags( surfID ) |= SURFDRAW_TRANS;
			bHasTranslucentSurfaces = true;
		}
		else
		{
			bHasOpaqueSurfaces = true;
		}

		// Certain surfaces don't want decals at all
		if ( (pTex->flags & SURF_NODECALS) || pMaterial->GetMaterialVarFlag( MATERIAL_VAR_SUPPRESS_DECALS ) || pMaterial->IsAlphaTested() )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_NODECALS;
		}

		if ( pMaterial->IsAlphaTested() )
		{
			MSurf_Flags( surfID ) |= SURFDRAW_ALPHATEST;
		}
		msurfacelighting_t *pLighting = SurfaceLighting( surfID, pBrushData );

		for (int maps = 0; maps < MAXLIGHTMAPS && pLighting->m_nStyles[maps] != 255 ; ++maps )
		{
			byte nStyle = pLighting->m_nStyles[maps];
			if ( lightStyles.Find(nStyle) == -1 )
			{
				lightStyles.AddToTail( nStyle );
			}
		}
	}

	if ( bHasOpaqueSurfaces && bHasTranslucentSurfaces )
	{
		mod->flags |= MODELFLAG_TRANSLUCENT_TWOPASS;
	}
	if ( lightStyles.Count() )
	{
		mod->brush.nLightstyleLastComputedFrame = 0;
		g_ModelLoader.AllocateLightstyles( mod, lightStyles.Base(), lightStyles.Count() );
	}
}


//-----------------------------------------------------------------------------
// Recomputes translucency for the model...
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t Mod_ComputeTranslucencyType( model_t* mod, int nSkin, int nBody )
{
	switch( mod->type )
	{
	case mod_brush:
		if ( ( mod->flags & MODELFLAG_TRANSLUCENT ) == 0 )
			return RENDERABLE_IS_OPAQUE;
		return ( mod->flags & MODELFLAG_TRANSLUCENT_TWOPASS ) ? RENDERABLE_IS_TWO_PASS : RENDERABLE_IS_TRANSLUCENT;

	case mod_studio:
		{
			studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( mod->studio );
			if ( pStudioHdr->flags & STUDIOHDR_FLAGS_FORCE_OPAQUE )
				return RENDERABLE_IS_OPAQUE;

			if ( IsGameConsole() && !g_ModelLoader.IsViewWeaponModelResident( mod ) )
			{
				// best guess
				// purposely preventing any request that would cause the hwmesh to load
				return RENDERABLE_IS_OPAQUE;
			}

			IMaterial *pMaterials[ 128 ];
			int materialCount = g_pStudioRender->GetMaterialListFromBodyAndSkin( mod->studio, nSkin, nBody, ARRAYSIZE( pMaterials ), pMaterials );
			for ( int i = 0; i < materialCount; i++ )
			{
				if ( pMaterials[i] == NULL )
					continue;

				bool bIsTranslucent = pMaterials[i]->IsTranslucent();
				if ( bIsTranslucent )
					return ( pStudioHdr->flags & STUDIOHDR_FLAGS_TRANSLUCENT_TWOPASS ) ? RENDERABLE_IS_TWO_PASS : RENDERABLE_IS_TRANSLUCENT;
			}

			return RENDERABLE_IS_OPAQUE;
		}
		break;

	case mod_sprite:
		return RENDERABLE_IS_TRANSLUCENT;

	default:
		return RENDERABLE_IS_OPAQUE;
	}
}


//-----------------------------------------------------------------------------
// returns the material count...
//-----------------------------------------------------------------------------
int Mod_GetMaterialCount( model_t* mod )
{
	switch( mod->type )
	{
	case mod_brush:
		{
			CUtlVector<IMaterial*> uniqueMaterials( 0, 32 );

			for (int i = 0; i < mod->brush.nummodelsurfaces; ++i)
			{
				SurfaceHandle_t surfID = SurfaceHandleFromIndex( mod->brush.firstmodelsurface + i, mod->brush.pShared );

				if ( MSurf_Flags( surfID ) & SURFDRAW_NODRAW )
					continue;

				IMaterial* pMaterial = MSurf_TexInfo( surfID, mod->brush.pShared )->material;

				// Try to find the material in the unique list of materials
				// if it's not there, then add it
				if (uniqueMaterials.Find(pMaterial) < 0)
					uniqueMaterials.AddToTail(pMaterial);
			}

			return uniqueMaterials.Count();
		}
		break;

	case mod_studio:
		{
			// FIXME: This should return the list of all materials
			// across all LODs if we every decide to implement this
			Assert(0);
		}
		break;

	default:
		// unimplemented
		Assert(0);
		break;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// returns the first n materials.
//-----------------------------------------------------------------------------
int Mod_GetModelMaterials( model_t* pModel, int count, IMaterial** ppMaterials )
{
	studiohdr_t *pStudioHdr;
	int found = 0; 
	int	i;

	switch( pModel->type )
	{
	case mod_brush:
		{
			for ( i = 0; i < pModel->brush.nummodelsurfaces; ++i)
			{
				SurfaceHandle_t surfID = SurfaceHandleFromIndex( pModel->brush.firstmodelsurface + i, pModel->brush.pShared );
				if ( MSurf_Flags( surfID ) & SURFDRAW_NODRAW )
					continue;

				IMaterial* pMaterial = MSurf_TexInfo( surfID, pModel->brush.pShared )->material;

				// Try to find the material in the unique list of materials
				// if it's not there, then add it
				int j = found;
				while ( --j >= 0 )
				{
					if ( ppMaterials[j] == pMaterial )
						break;
				}
				if (j < 0)
					ppMaterials[found++] = pMaterial;

				// Stop when we've gotten count materials
				if ( found >= count )
					return found;
			}
		}
		break;

	case mod_studio:
		// Get the studiohdr into the cache
		pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );
		// Get the list of materials
		found = g_pStudioRender->GetMaterialList( pStudioHdr, count, ppMaterials );
		break;

	default:
		// unimplemented
		Assert( 0 );
		break;
	}

	return found;
}

//-----------------------------------------------------------------------------
// Used to compute which surfaces are in water or not
//-----------------------------------------------------------------------------

static void MarkWaterSurfaces_ProcessLeafNode( mleaf_t *pLeaf )
{
	int i;

	int flags = ( pLeaf->leafWaterDataID == -1 ) ? SURFDRAW_ABOVEWATER : SURFDRAW_UNDERWATER;

	SurfaceHandle_t *pHandle = &host_state.worldbrush->marksurfaces[pLeaf->firstmarksurface];

	for( i = 0; i < pLeaf->nummarksurfaces; i++ )
	{
		SurfaceHandle_t surfID = pHandle[i];
		ASSERT_SURF_VALID( surfID );
		if( MSurf_Flags( surfID ) & SURFDRAW_WATERSURFACE )
			continue;

		if (SurfaceHasDispInfo( surfID ))
			continue;

		MSurf_Flags( surfID ) |= flags;
	}

	// FIXME: This is somewhat bogus, but I can do it quickly, and it's
	// not clear I need to solve the harder problem.

	// If any portion of a displacement surface hits a water surface,
	// I'm going to mark it as being in water, and vice versa.
	for ( i = 0; i < pLeaf->dispCount; i++ )
	{
		IDispInfo *pDispInfo = MLeaf_Disaplcement( pLeaf, i );

		SurfaceHandle_t parentSurfID = pDispInfo->GetParent();
		MSurf_Flags( parentSurfID ) |= flags;
	}
}


void MarkWaterSurfaces_r( mnode_t *node )
{
	// no polygons in solid nodes
	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	// if a leaf node, . .mark all the polys as to whether or not they are in water.
	if (node->contents >= 0)
	{
		MarkWaterSurfaces_ProcessLeafNode( (mleaf_t *)node );
		return;
	}

	MarkWaterSurfaces_r( node->children[0] );
	MarkWaterSurfaces_r( node->children[1] );
}


//-----------------------------------------------------------------------------
// Computes the sort group for a particular face
//-----------------------------------------------------------------------------
static int SurfFlagsToSortGroup( SurfaceHandle_t surfID, int flags )
{
	if( flags & SURFDRAW_WATERSURFACE )
		return MAT_SORT_GROUP_WATERSURFACE;

	if( ( flags & ( SURFDRAW_UNDERWATER | SURFDRAW_ABOVEWATER ) ) == ( SURFDRAW_UNDERWATER | SURFDRAW_ABOVEWATER ) )
		return MAT_SORT_GROUP_INTERSECTS_WATER_SURFACE;
	
	if( flags & SURFDRAW_UNDERWATER )
		return MAT_SORT_GROUP_STRICTLY_UNDERWATER;

	if( flags & SURFDRAW_ABOVEWATER )
		return MAT_SORT_GROUP_STRICTLY_ABOVEWATER;

	static int warningcount = 0;
	if ( ++warningcount < 10 )
	{
		Vector vecCenter;
		Surf_ComputeCentroid( surfID, &vecCenter );
		DevWarning( "SurfFlagsToSortGroup:  unhandled flags (%X) (%s)!\n", flags, MSurf_TexInfo(surfID)->material->GetName() );	
		DevWarning( "- This implies you have a surface (usually a displacement) embedded in solid.\n" );	
		DevWarning( "- Look near (%.1f, %.1f, %.1f)\n", vecCenter.x, vecCenter.y, vecCenter.z );	
	}
	//Assert( 0 );
	return MAT_SORT_GROUP_STRICTLY_ABOVEWATER;
}



//-----------------------------------------------------------------------------
// Computes sort group
//-----------------------------------------------------------------------------
bool Mod_MarkWaterSurfaces( model_t *pModel )
{
	bool bHasWaterSurfaces = false;
	model_t *pSaveModel = host_state.worldmodel;

	// garymcthack!!!!!!!!
	// host_state.worldmodel isn't set at this point, so. . . . 
	host_state.SetWorldModel( pModel );
	MarkWaterSurfaces_r( pModel->brush.pShared->nodes );
	for ( int i = 0; i < pModel->brush.pShared->numsurfaces; i++ )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( i, pModel->brush.pShared );
		
		int sortGroup = SurfFlagsToSortGroup( surfID, MSurf_Flags( surfID ) );
		if ( sortGroup == MAT_SORT_GROUP_WATERSURFACE )
		{
			bHasWaterSurfaces = true;
		}
		MSurf_SetSortGroup( surfID, sortGroup );
	}
	host_state.SetWorldModel( pSaveModel );

	return bHasWaterSurfaces;
}


//-----------------------------------------------------------------------------
// Marks identity brushes as being in fog volumes or not
//-----------------------------------------------------------------------------
class CBrushBSPIterator : public ISpatialLeafEnumerator
{
public:
	CBrushBSPIterator( model_t *pWorld, model_t *pBrush )
	{
		m_pWorld = pWorld;
		m_pBrush = pBrush;
		m_pShared = pBrush->brush.pShared;
		m_count = 0;
	}
	bool EnumerateLeaf( int leaf, intp )
	{
		// garymcthack - need to test identity brush models
		int flags = ( m_pShared->leafs[leaf].leafWaterDataID == -1 ) ? SURFDRAW_ABOVEWATER : SURFDRAW_UNDERWATER;
		MarkModelSurfaces( flags );
		m_count++;
		return true;
	}

	void MarkModelSurfaces( int flags )
	{
		// Iterate over all this models surfaces
		int surfaceCount = m_pBrush->brush.nummodelsurfaces;
		for (int i = 0; i < surfaceCount; ++i)
		{
			SurfaceHandle_t surfID = SurfaceHandleFromIndex( m_pBrush->brush.firstmodelsurface + i, m_pShared ); 
			MSurf_Flags( surfID ) &= ~(SURFDRAW_ABOVEWATER | SURFDRAW_UNDERWATER);
			MSurf_Flags( surfID ) |= flags;
		}
	}

	void CheckSurfaces()
	{
		if ( !m_count )
		{
			MarkModelSurfaces( SURFDRAW_ABOVEWATER );
		}
	}

	model_t* m_pWorld;
	model_t* m_pBrush;
	worldbrushdata_t *m_pShared;
	int m_count;
};

static void MarkBrushModelWaterSurfaces( model_t* world,
	Vector const& mins, Vector const& maxs, model_t* brush )
{
	// HACK: This is a totally brutal hack dealing with initialization order issues.
	// I want to use the same box enumeration code so I don't have multiple
	// copies, but I want to use it from modelloader. host_state.worldmodel isn't
	// set up at that time however, so I have to fly through these crazy hoops.
	// Massive suckage.

	model_t* pTemp = host_state.worldmodel;
	CBrushBSPIterator brushIterator( world, brush );
	host_state.SetWorldModel( world );
	g_pToolBSPTree->EnumerateLeavesInBox( mins, maxs, &brushIterator, (intp)brush );
	brushIterator.CheckSurfaces();
	host_state.SetWorldModel( pTemp );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//			*buffer - 
//-----------------------------------------------------------------------------
void CModelLoader::Map_LoadModel( model_t *mod )
{
	if ( sv.IsDedicated() )
		BeginWatchdogTimer( 4 * 60 );							// reset watchdog timer to allow 4 minutes for map load

#ifndef MEM_DETAILED_ACCOUNTING_MAP_LOADMODEL
	MEM_ALLOC_CREDIT();
#endif

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	Assert( !( mod->nLoadFlags & FMODELLOADER_LOADED ) );

	COM_TimestampedLog( "Map_LoadModel: Start" );

	double startTime = Plat_FloatTime();

	SetWorldModel( mod );

	// point at the shared world/brush data
	mod->brush.pShared = &m_worldBrushData;
	mod->brush.renderHandle = 0;
	mod->type = mod_brush;
	mod->nLoadFlags |= FMODELLOADER_LOADED;
	CMapLoadHelper::Init( mod, mod->szPathName );
	Map_LoadModelGuts( mod );
	// Close map file, etc.
	CMapLoadHelper::Shutdown();

	double elapsed = Plat_FloatTime() - startTime;
	COM_TimestampedLog( "Map_LoadModel: Finish - loading took %.4f seconds", elapsed );
	if ( sv.IsDedicated() )
		EndWatchdogTimer();
}

int g_nMapLoadCount = 0;

// do all of the I/O.  This is broken out to bracket the MapLoadHelper::Init/Shutdown
void CModelLoader::Map_LoadModelGuts( model_t *mod )
{
	++g_nMapLoadCount;
	// HDR and features must be established first
	COM_TimestampedLog( "  Map_CheckForHDR" );
	m_bMapHasHDRLighting = Map_CheckForHDR( mod, mod->szPathName );
	if ( IsGameConsole() && !m_bMapHasHDRLighting )
	{
		Warning( "Map '%s' lacks exepected HDR data! 360 does not support accurate LDR visuals.\n", mod->szPathName );
	}

	// load the texinfo lump (used by many subsequent lumps in raw form)
	CMapLoadHelper lhTexinfo( LUMP_TEXINFO );
	texinfo_t *pTexinfo = (texinfo_t *)lhTexinfo.LumpBase();
	if ( lhTexinfo.LumpSize() % sizeof( *pTexinfo ) )
		Host_Error( "Map_LoadModelGuts: bad LUMP_TEXINFO size in %s", mod->szPathName );
	int texinfoCount = lhTexinfo.LumpSize() / sizeof( *pTexinfo );
	if ( texinfoCount < 1 )
	{
		if ( !g_bClearingClientState )
		{
			Sys_Error( "Map_LoadModelGuts: Map with no texinfo, %s", mod->szPathName );
		}
		return;
	}
	if ( texinfoCount > MAX_MAP_TEXINFO )
		Sys_Error( "Map_LoadModelGuts: Map has too many surfaces, %s", mod->szPathName );

	// Load the collision model
	COM_TimestampedLog( "  CM_LoadMap" );
	unsigned int checksum;
	CM_LoadMap( mod->szPathName, false, pTexinfo, texinfoCount, &checksum );

	// The MEM_ALLOC_CREDITs here will be overridden and credited to Map_LoadModel()
	// unless you #define MEM_DETAILED_ACCOUNTING_MAP_LOADMODEL at the top of this file.
	{
		MEM_ALLOC_CREDIT_("Mod_LoadVertices");
		COM_TimestampedLog( "  Mod_LoadVertices" );
		Mod_LoadVertices();
	}
	
	{
		MEM_ALLOC_CREDIT_("Mod_LoadEdges");
		COM_TimestampedLog( "  Mod_LoadEdges" );
		medge_t *pedges = Mod_LoadEdges();
	
		COM_TimestampedLog( "  Mod_LoadSurfedges" );
		Mod_LoadSurfedges( pedges );
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadPlanes");
		COM_TimestampedLog( "  Mod_LoadPlanes" );
		Mod_LoadPlanes();
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadOcclusion");
		COM_TimestampedLog( "  Mod_LoadOcclusion" );
		Mod_LoadOcclusion();
	}

	{
		// texdata needs to load before texinfo
		MEM_ALLOC_CREDIT_("Mod_LoadTexdata");
		COM_TimestampedLog( "  Mod_LoadTexdata" );
		Mod_LoadTexdata();
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadTexinfo");
		COM_TimestampedLog( "  Mod_LoadTexinfo" );
		Mod_LoadTexinfo( lhTexinfo );
	}

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	// Until BSP version 19, this must occur after loading texinfo
	{
		MEM_ALLOC_CREDIT_("Mod_LoadLighting");
		COM_TimestampedLog( "  Mod_LoadLighting" );
		bool bLoadHDR = ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE ) &&
						( CMapLoadHelper::LumpSize( LUMP_LIGHTING_HDR ) > 0 );
		Mod_LoadLighting( bLoadHDR );
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadPrimitives");
		COM_TimestampedLog( "  Mod_LoadPrimitives" );
		Mod_LoadPrimitives();
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadPrimVerts");
		COM_TimestampedLog( "  Mod_LoadPrimVerts" );
		Mod_LoadPrimVerts();
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadPrimIndices");
		COM_TimestampedLog( "  Mod_LoadPrimIndices" );
		Mod_LoadPrimIndices();
	}

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	{
		// faces need to be loaded before vertnormals
		MEM_ALLOC_CREDIT_("Mod_LoadFaces");
		COM_TimestampedLog( "  Mod_LoadFaces" );
		Mod_LoadFaces();
		Mod_LoadFaceBrushes();
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadVertNormals");
		COM_TimestampedLog( "  Mod_LoadVertNormals" );
		Mod_LoadVertNormals();
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadVertNormalIndices");
		COM_TimestampedLog( "  Mod_LoadVertNormalIndices" );
		Mod_LoadVertNormalIndices();
	}

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	{
		// note leafs must load befor marksurfaces
		MEM_ALLOC_CREDIT_("Mod_LoadLeafs");
		COM_TimestampedLog( "  Mod_LoadLeafs" );
		Mod_LoadLeafs();
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadMarksurfaces");
		COM_TimestampedLog( "  Mod_LoadMarksurfaces" );
		Mod_LoadMarksurfaces();
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadNodes");
		COM_TimestampedLog( "  Mod_LoadNodes" );
		Mod_LoadNodes();
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadLeafWaterData");
		COM_TimestampedLog( "  Mod_LoadLeafWaterData" );
		Mod_LoadLeafWaterData();
	}

#ifndef DEDICATED
	{
		// UNDONE: Does the cmodel need worldlights?
		MEM_ALLOC_CREDIT_("OverlayMgr()->LoadOverlays");
		COM_TimestampedLog( "  OverlayMgr()->LoadOverlays" );
		OverlayMgr()->LoadOverlays();	
	}
#endif

	{
		MEM_ALLOC_CREDIT_("Mod_LoadLeafMinDistToWater");
		COM_TimestampedLog( "  Mod_LoadLeafMinDistToWater" );
		Mod_LoadLeafMinDistToWater();
	}

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	{
		MEM_ALLOC_CREDIT_("LUMP_CLIPPORTALVERTS");
		COM_TimestampedLog( "  LUMP_CLIPPORTALVERTS" );
		Mod_LoadLump( mod, 
			LUMP_CLIPPORTALVERTS, 
			va( "%s [%s]", m_szBaseName, "clipportalverts" ),
			sizeof(m_worldBrushData.m_pClipPortalVerts[0]), 
			(void**)&m_worldBrushData.m_pClipPortalVerts,
			&m_worldBrushData.m_nClipPortalVerts );
	}

	{
		MEM_ALLOC_CREDIT_("LUMP_AREAPORTALS");
		COM_TimestampedLog( "  LUMP_AREAPORTALS" );
		Mod_LoadLump( mod, 
			LUMP_AREAPORTALS, 
			va( "%s [%s]", m_szBaseName, "areaportals" ),
			sizeof(m_worldBrushData.m_pAreaPortals[0]), 
			(void**)&m_worldBrushData.m_pAreaPortals,
			&m_worldBrushData.m_nAreaPortals );
	}
	
	{
		MEM_ALLOC_CREDIT_("LUMP_AREAS");
		COM_TimestampedLog( "  LUMP_AREAS" );
		Mod_LoadLump( mod, 
			LUMP_AREAS, 
			va( "%s [%s]", m_szBaseName, "areas" ),
			sizeof(m_worldBrushData.m_pAreas[0]), 
			(void**)&m_worldBrushData.m_pAreas,
			&m_worldBrushData.m_nAreas );
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadWorldlights");
		COM_TimestampedLog( "  Mod_LoadWorldlights" );
		if ( g_pMaterialSystemHardwareConfig->GetHDRType() != HDR_TYPE_NONE &&
			CMapLoadHelper::LumpSize( LUMP_WORLDLIGHTS_HDR ) > 0 )
		{
			CMapLoadHelper mlh( LUMP_WORLDLIGHTS_HDR );
			Mod_LoadWorldlights( mlh, true );
		}
		else
		{
			CMapLoadHelper mlh( LUMP_WORLDLIGHTS );
			Mod_LoadWorldlights( mlh, false );
		}
	}

	{
		MEM_ALLOC_CREDIT_("Mod_LoadCubemapSamples");
		COM_TimestampedLog( "  Mod_LoadCubemapSamples" );
		Mod_LoadCubemapSamples();
	}

#if defined( PORTAL2 ) || defined( CSTRIKE15 )
	{
		MEM_ALLOC_CREDIT_("Mod_LoadSimpleWorldModel");
		COM_TimestampedLog( "  Mod_LoadSimpleWorldModel" );
		Mod_LoadSimpleWorldModel( m_szBaseName );
	}
#endif
	{
		MEM_ALLOC_CREDIT_("Mod_LoadGameLumpDict");
		COM_TimestampedLog( "  Mod_LoadGameLumpDict" );
		Mod_LoadGameLumpDict();
	}

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

	{
		MEM_ALLOC_CREDIT_("Mod_LoadSubmodels");
		COM_TimestampedLog( "  Mod_LoadSubmodels" );
		CUtlVector<mmodel_t> submodelList;
		Mod_LoadSubmodels( submodelList );

#ifndef DEDICATED
		EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif

		COM_TimestampedLog( "  SetupSubModels" );
		SetupSubModels( mod, submodelList );
	}

	{
		MEM_ALLOC_CREDIT_("RecomputeSurfaceFlags");
		COM_TimestampedLog( "  RecomputeSurfaceFlags" );
		RecomputeSurfaceFlags( mod );
	}

#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar(PROGRESS_LOADWORLDMODEL);
#endif
	{
		MEM_ALLOC_CREDIT_("Map_VisClear");
		COM_TimestampedLog( "  Map_VisClear" );
		Map_VisClear();
	}

	{
		MEM_ALLOC_CREDIT_("Map_SetRenderInfoAllocated");
		COM_TimestampedLog( "  Map_SetRenderInfoAllocated" );
		Map_SetRenderInfoAllocated( false );
	}
}

void CModelLoader::Map_UnloadCubemapSamples( model_t *mod )
{
	int i;
	for ( i=0 ; i < mod->brush.pShared->m_nCubemapSamples ; i++ )
	{
		mcubemapsample_t *pSample = &mod->brush.pShared->m_pCubemapSamples[i];
		pSample->pTexture->DecrementReferenceCount();
	}
}

void CModelLoader::Map_UnloadSimpleWorldModel( model_t *mod )
{
	if ( g_pSimpleWorldModel )
	{
		UnloadModel( g_pSimpleWorldModel );
	}
	if ( g_pSimpleWorldModelWater )
	{
		UnloadModel( g_pSimpleWorldModelWater );
	}
	g_pSimpleWorldModel = NULL;
	g_pSimpleWorldModelWater = NULL;
}

//-----------------------------------------------------------------------------
// Recomputes surface flags
//-----------------------------------------------------------------------------
void CModelLoader::RecomputeSurfaceFlags( model_t *mod )
{
	for (int i=0 ; i<mod->brush.pShared->numsubmodels ; i++)
	{
		model_t *pSubModel = &m_InlineModels[i];

		// Compute whether this submodel uses material proxies or not
		Mod_ComputeBrushModelFlags( pSubModel );

		// Mark if brush models are in water or not; we'll use this
		// for identity brushes. If the brush is not an identity brush,
		// then we'll not have to worry.
		if ( i != 0 )
		{
			MarkBrushModelWaterSurfaces( mod, pSubModel->mins, pSubModel->maxs, pSubModel );
		}
	}
}

//-----------------------------------------------------------------------------
// Setup sub models
//-----------------------------------------------------------------------------
void CModelLoader::SetupSubModels( model_t *mod, CUtlVector<mmodel_t> &list )
{
	int	i;

	m_InlineModels.SetCount( m_worldBrushData.numsubmodels );

	for (i=0 ; i<m_worldBrushData.numsubmodels ; i++)
	{
		model_t		*starmod;
		mmodel_t	*bm;

		bm = &list[i];
		starmod = &m_InlineModels[i];

		*starmod = *mod;
		
		starmod->brush.firstmodelsurface = bm->firstface;
		starmod->brush.nummodelsurfaces = bm->numfaces;
		starmod->brush.firstnode = bm->headnode;
		if ( starmod->brush.firstnode >= m_worldBrushData.numnodes )
		{
			Sys_Error( "Inline model %i has bad firstnode", i );
		}

		VectorCopy(bm->maxs, starmod->maxs);
		VectorCopy(bm->mins, starmod->mins);
		starmod->radius = bm->radius;
	
		if (i == 0)
		{
			*mod = *starmod;
		}
		else
		{
			Q_snprintf( starmod->szPathName, sizeof( starmod->szPathName ), "*%d", i );
			starmod->fnHandle = g_pFileSystem->FindOrAddFileName( starmod->szPathName );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//-----------------------------------------------------------------------------
void CModelLoader::Map_UnloadModel( model_t *mod )
{
	Assert( !( mod->nLoadFlags & FMODELLOADER_REFERENCEMASK ) );
	mod->nLoadFlags &= ~FMODELLOADER_LOADED;
	
#ifndef DEDICATED
	OverlayMgr()->UnloadOverlays();
#endif

	DeallocateLightingData( &m_worldBrushData );

#ifndef DEDICATED
	DispInfo_ReleaseMaterialSystemObjects( mod );
#endif

	Map_UnloadSimpleWorldModel( mod );

	Map_UnloadCubemapSamples( mod );
	
#ifndef DEDICATED
	// Free decals in displacements.
	R_DecalTerm( &m_worldBrushData, true, true );
#endif

	if ( m_worldBrushData.hDispInfos )
	{
		DispInfo_DeleteArray( m_worldBrushData.hDispInfos );
		m_worldBrushData.hDispInfos = NULL;
	}

	// Model loader loads world model materials, unload them here
	for( int texinfoID = 0; texinfoID < m_worldBrushData.numtexinfo; texinfoID++ )
	{
		mtexinfo_t *pTexinfo = &m_worldBrushData.texinfo[texinfoID];
		if ( pTexinfo )
		{
			GL_UnloadMaterial( pTexinfo->material );
		}
	}

	MaterialSystem_DestroySortinfo();

	// Don't store any reference to it here
	ClearWorldModel();
	Map_SetRenderInfoAllocated( false );
}


//-----------------------------------------------------------------------------
// Computes dimensions + frame count of a material 
//-----------------------------------------------------------------------------
static void GetSpriteInfo( const char *pName, bool bIsAVI, bool bIsBIK, int &nWidth, int &nHeight, int &nFrameCount )
{
	nFrameCount = 1;
	nWidth = nHeight = 1;

	// FIXME: The reason we are putting logic related to AVIs here,
	// logic which is duplicated in the client DLL related to loading sprites,
	// is that this code gets run on dedicated servers also.
	IMaterial *pMaterial = NULL;
	AVIMaterial_t hAVIMaterial = AVIMATERIAL_INVALID; 
#if !defined( _GAMECONSOLE ) || defined( BINK_ENABLED_FOR_CONSOLE )
	BIKMaterial_t hBIKMaterial = BIKMATERIAL_INVALID; 
#endif
	if ( bIsAVI )
	{
		hAVIMaterial = avi->CreateAVIMaterial( pName, pName, "GAME" );
		avi->GetFrameSize( hAVIMaterial, &nWidth, &nHeight );
		nFrameCount = avi->GetFrameCount( hAVIMaterial );
		if ( hAVIMaterial != AVIMATERIAL_INVALID )
		{
			pMaterial = avi->GetMaterial( hAVIMaterial );
		}
	}
#if !defined( _GAMECONSOLE ) || defined( BINK_ENABLED_FOR_CONSOLE )
	else if ( bIsBIK )
	{
		hBIKMaterial = bik->CreateMaterial( pName, pName, "GAME" );
		if (hBIKMaterial != BIKMATERIAL_INVALID )
		{
			bik->GetFrameSize( hBIKMaterial, &nWidth, &nHeight );
			nFrameCount = bik->GetFrameCount( hBIKMaterial );
			pMaterial = bik->GetMaterial( hBIKMaterial );
		}
	}
#endif
	else
	{
		pMaterial = GL_LoadMaterial( pName, TEXTURE_GROUP_OTHER );
		if ( pMaterial )
		{
			// Store off our source height, width, frame count
			nWidth = pMaterial->GetMappingWidth();
			nHeight = pMaterial->GetMappingHeight();
			nFrameCount = pMaterial->GetNumAnimationFrames();
		}
	}

	if ( pMaterial == g_materialEmpty )
	{
		DevMsg( "Missing sprite material %s\n", pName );
	}

	if ( hAVIMaterial != AVIMATERIAL_INVALID )
	{
		avi->DestroyAVIMaterial( hAVIMaterial );
	}

#if !defined( _GAMECONSOLE ) || defined( BINK_ENABLED_FOR_CONSOLE )
	if ( hBIKMaterial != BIKMATERIAL_INVALID )
	{
		bik->DestroyMaterial( hBIKMaterial );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Sprite_LoadModel( model_t *mod )
{
	Assert( !( mod->nLoadFlags & FMODELLOADER_LOADED ) );

	mod->nLoadFlags |= FMODELLOADER_LOADED;

	// The hunk data is not used on the server
	byte* pSprite = NULL;

#ifndef DEDICATED
	if ( g_ClientDLL )
	{
		int nSize = g_ClientDLL->GetSpriteSize();
		if ( nSize )
		{
			pSprite = ( byte * )new byte[ nSize ];
		}
	}
#endif

	mod->type = mod_sprite;
	mod->sprite.sprite = (CEngineSprite *)pSprite;

	// Fake the bounding box. We need it for PVS culling, and we don't
	// know the scale at which the sprite is going to be rendered at
	// when we load it
	mod->mins = mod->maxs = Vector(0,0,0);

	// Figure out the real load name..
	char loadName[MAX_PATH];
	bool bIsAVI, bIsBIK;
	BuildSpriteLoadName( mod->szPathName, loadName, MAX_PATH, bIsAVI, bIsBIK );
	GetSpriteInfo( loadName, bIsAVI, bIsBIK, mod->sprite.width, mod->sprite.height, mod->sprite.numframes );

#ifndef DEDICATED
	if ( g_ClientDLL && mod->sprite.sprite )
	{
		g_ClientDLL->InitSprite( mod->sprite.sprite, loadName );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Sprite_UnloadModel( model_t *mod )
{
	Assert( !( mod->nLoadFlags & FMODELLOADER_REFERENCEMASK ) );
	mod->nLoadFlags &= ~FMODELLOADER_LOADED;

	char loadName[MAX_PATH];
	bool bIsAVI, bIsBIK;
	BuildSpriteLoadName( mod->szPathName, loadName, sizeof( loadName ), bIsAVI, bIsBIK );

	IMaterial *mat = materials->FindMaterial( loadName, TEXTURE_GROUP_OTHER );
	if ( !IsErrorMaterial( mat ) )
	{
		GL_UnloadMaterial( mat );
	}

#ifndef DEDICATED
	if ( g_ClientDLL && mod->sprite.sprite )
	{
		g_ClientDLL->ShutdownSprite( mod->sprite.sprite );
	}
#endif

	delete[] (byte *)mod->sprite.sprite;
	mod->sprite.sprite = 0;
	mod->sprite.numframes = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Flush and reload models.  Intended for use when lod changes.
//-----------------------------------------------------------------------------
void CModelLoader::Studio_ReloadModels( CModelLoader::ReloadType_t reloadType )
{
#if !defined( DEDICATED )
	if ( g_ClientDLL )
		g_ClientDLL->InvalidateMdlCache();
#endif // DEDICATED
	if ( serverGameDLL )
		serverGameDLL->InvalidateMdlCache();

	// ensure decals have no stale references to invalid lods
	modelrender->RemoveAllDecalsFromAllModels( false );

	// ensure static props have no stale references to invalid lods
	modelrender->ReleaseAllStaticPropColorData();

	// Flush out the model cache
	// Don't flush vcollides since the vphysics system currently
	// has no way of indicating they refer to vcollides
	g_pMDLCache->Flush( (MDLCacheFlush_t) (MDLCACHE_FLUSH_ALL & ~(MDLCACHE_FLUSH_IGNORELOCK|MDLCACHE_FLUSH_VCOLLIDE)) );

	// Load the critical pieces now
	// The model cache will re-populate as models render
	int c = m_Models.Count();
	for ( int i=0 ; i < c ; i++ )
	{
		model_t *pModel = m_Models[ i ].modelpointer;
		if ( !IsLoaded( pModel ) )
			continue;

		if ( pModel->type != mod_studio )
			continue;

		MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

		// Get the studiohdr into the cache
		g_pMDLCache->GetStudioHdr( pModel->studio );

		// force the collision to load
		g_pMDLCache->GetVCollide( pModel->studio );
	}
}

struct modelsize_t
{
	const char *pName;
	int			size;
};

class CModelsize_Less
{
public:
	bool Less( const modelsize_t& src1, const modelsize_t& src2, void *pCtx )
	{
		return ( src1.size < src2.size );
	}
};

void CModelLoader::DumpVCollideStats()
{
	int i;
	CUtlSortVector< modelsize_t, CModelsize_Less > list;
	for ( i = m_Models.Count(); --i >= 0; )
	{
		model_t *pModel = m_Models[ i ].modelpointer;
		if ( pModel && pModel->type == mod_studio )
		{
			int size = 0;
			bool loaded = g_pMDLCache->GetVCollideSize( pModel->studio, &size );
			if ( loaded && size )
			{
				modelsize_t elem;
				elem.pName = pModel->szPathName;
				elem.size = size;
				list.Insert( elem );
			}
		}
	}
	for ( i = m_InlineModels.Count(); --i >= 0; )
	{
		vcollide_t *pCollide = CM_VCollideForModel( i+1, &m_InlineModels[i] );
		if ( pCollide )
		{
			int size = 0;
			for ( int j = 0; j < pCollide->solidCount; j++ )
			{
				size += physcollision->CollideSize( pCollide->solids[j] );
			}
			size += pCollide->descSize;
			if ( size )
			{
				modelsize_t elem;
				elem.pName = m_InlineModels[i].szPathName;
				elem.size = size;
				list.Insert( elem );
			}
		}
	}

	Msg("VCollides loaded: %d\n", list.Count() );
	int totalVCollideMemory = 0;
	for ( i = 0; i < list.Count(); i++ )
	{
		Msg("%8d bytes:%s\n", list[i].size, list[i].pName);
		totalVCollideMemory += list[i].size;
	}
	int bboxCount, bboxSize;
	physcollision->GetBBoxCacheSize( &bboxSize, &bboxCount );
	Msg( "%8d bytes BBox physics: %d boxes\n", bboxSize, bboxCount );
	totalVCollideMemory += bboxSize;
	Msg( "--------------\n%8d bytes total VCollide Memory\n", totalVCollideMemory );
}


//-----------------------------------------------------------------------------
// Is the model loaded?
//-----------------------------------------------------------------------------
bool CModelLoader::IsLoaded( const model_t *mod )
{
	return (mod->nLoadFlags & FMODELLOADER_LOADED) != 0;
}

bool CModelLoader::LastLoadedMapHasHDRLighting(void)
{
	return m_bMapHasHDRLighting;
}

bool CModelLoader::LastLoadedMapHasLightmapAlphaData( void )
{
	return g_bHasLightmapAlphaData;
}

//-----------------------------------------------------------------------------
// Loads a studio model
//-----------------------------------------------------------------------------
void CModelLoader::Studio_LoadModel( model_t *pModel, bool bTouchAllData )
{
	if ( !mod_touchalldata.GetBool() )
	{
		bTouchAllData = false;
	}

	// a preloaded model requires specific fixup behavior
	bool bPreLoaded = ( pModel->nLoadFlags & FMODELLOADER_LOADED_BY_PRELOAD ) != 0;

	bool bLoadPhysics = true;
	if ( pModel->nLoadFlags == FMODELLOADER_STATICPROP )
	{
		// this is the first call in loading as a static prop (load bit not set), don't load physics yet
		// the next call in causes the physics to load
		bLoadPhysics = false;
	}

	// mark as loaded and fixed up
	pModel->nLoadFlags |= FMODELLOADER_LOADED;
	pModel->nLoadFlags &= ~FMODELLOADER_LOADED_BY_PRELOAD;

	if ( !bPreLoaded )
	{
		pModel->studio = g_pMDLCache->FindMDL( pModel->szPathName );		
		g_pMDLCache->SetUserData( pModel->studio, pModel );

		InitStudioModelState( pModel );
	}

	// Get the studiohdr into the cache
	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );

	// a preloaded model already has its physics data resident
	if ( bLoadPhysics && !bPreLoaded )
	{
		// load the collision data now
		bool bSynchronous = bTouchAllData;
		double t1 = Plat_FloatTime();
		g_pMDLCache->GetVCollideEx( pModel->studio, bSynchronous );

		double t2 = Plat_FloatTime();
		if ( bSynchronous )
		{
			g_flAccumulatedModelLoadTimeVCollideSync += ( t2 - t1 );
		}
		else
		{
			g_flAccumulatedModelLoadTimeVCollideAsync += ( t2 - t1 );
		}
	}

	// querying materials forces sync setup operations (materials/shaders) to build out now during load and not at runtime
	double t1 = Plat_FloatTime();

	IMaterial *pMaterials[128];
	int nMaterials = g_pStudioRender->GetMaterialList( pStudioHdr, ARRAYSIZE( pMaterials ), &pMaterials[0] );
	if ( nMaterials > 0 )
	{
		for ( int i = 0; i < nMaterials; i++ )
		{
			// Up the reference to all of this model's materials (decremented during UnloadModel)
			// otherwise the post-load purge will discard materials whose meshes are not yet in the cache.
			pMaterials[i]->IncrementReferenceCount();
		}

		// track the refcount bump
		pModel->nLoadFlags |= FMODELLOADER_TOUCHED_MATERIALS;
	}

	double t2 = Plat_FloatTime();
	g_flAccumulatedModelLoadTimeMaterialNamesOnly += ( t2 - t1 );

	// a preloaded model must touch its children
	if ( bTouchAllData || bPreLoaded )
	{
		Mod_TouchAllData( pModel, Host_GetServerCount() );
	}

	// track weapon models and their materials
	int nMapIndex = m_WeaponModelCache.Find( pModel );
	if ( nMapIndex != m_WeaponModelCache.InvalidIndex() )
	{	
		if ( nMaterials > 0 && !m_WeaponModelCache[nMapIndex]->m_Materials.Count() )
		{
			m_WeaponModelCache[nMapIndex]->m_Materials.AddMultipleToTail( nMaterials );
			for ( int i = 0; i < nMaterials; i++ )
			{
				m_WeaponModelCache[nMapIndex]->m_Materials[i] = pMaterials[i]->GetName();
			}
		}

		// this is truly unfortunate, but the act of getting the material dependencies, built out all the model's resources
		// evict now because the aggregate memory for the view models has been allocated elsewhere
		EvictWeaponModel( nMapIndex, true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//-----------------------------------------------------------------------------
void CModelLoader::Studio_UnloadModel( model_t *pModel )
{
	if ( pModel->nLoadFlags & FMODELLOADER_TOUCHED_MATERIALS )
	{
		// remove the added reference to all of this model's materials
		IMaterial *pMaterials[128];
		int nMaterials = Mod_GetModelMaterials( pModel, ARRAYSIZE( pMaterials ), &pMaterials[0] );
		for ( int j=0; j<nMaterials; j++ )
		{
			pMaterials[j]->DecrementReferenceCount();
		}
		pModel->nLoadFlags &= ~FMODELLOADER_TOUCHED_MATERIALS;
	}

	// leave these flags alone since we are going to return from alt-tab at some point.
	//	Assert( !( mod->needload & FMODELLOADER_REFERENCEMASK ) );
	pModel->nLoadFlags &= ~( FMODELLOADER_LOADED | FMODELLOADER_LOADED_BY_PRELOAD );
	if ( IsGameConsole() )
	{
		// 360 doesn't need to keep the reference flags, but the PC does
		pModel->nLoadFlags &= ~FMODELLOADER_REFERENCEMASK;
	}

#ifdef DBGFLAG_ASSERT
	int nRef = 
#endif
		g_pMDLCache->Release( pModel->studio );

	// the refcounts must be as expected, or evil latent bugs will occur
	Assert( InEditMode() || ( nRef == 0 ) );

	pModel->studio = MDLHANDLE_INVALID;
	pModel->type = mod_bad;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//-----------------------------------------------------------------------------
void CModelLoader::SetWorldModel( model_t *mod )
{
	Assert( mod );
	m_pWorldModel = mod;
//	host_state.SetWorldModel( mod ); // garymcthack
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::ClearWorldModel( void )
{
	m_pWorldModel = NULL;

	m_InlineModels.Purge();

	// zero out the world brush data and restore any embedded pointers
	memset( &m_worldBrushData, 0, sizeof( m_worldBrushData ) );
	m_worldBrushData.m_pLightingDataStack = &m_WorldLightingDataStack;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CModelLoader::IsWorldModelSet( void )
{
	return m_pWorldModel ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CModelLoader::GetNumWorldSubmodels( void )
{
	if ( !IsWorldModelSet() )
		return 0;

	return m_worldBrushData.numsubmodels;
}

//-----------------------------------------------------------------------------
// Purpose: Check cache or union data for info, reload studio model if needed 
// Input  : *model - 
//-----------------------------------------------------------------------------
void *CModelLoader::GetExtraData( model_t *model )
{
	if ( !model )
	{
		return NULL;
	}

	switch ( model->type )
	{
	case mod_sprite:
		{
			// sprites don't use the real cache yet
			if ( model->type == mod_sprite )
			{
				// The sprite got unloaded.
				if ( !( FMODELLOADER_LOADED & model->nLoadFlags ) )
				{
					return NULL;
				}

				return model->sprite.sprite;
			}
		}
		break;

	case mod_studio:
		return g_pMDLCache->GetStudioHdr( model->studio );

	default:
	case mod_brush:
		// Should never happen
		Assert( 0 );
		break;
	};

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CModelLoader::Map_GetRenderInfoAllocated( void )
{
	return m_bMapRenderInfoLoaded;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelLoader::Map_SetRenderInfoAllocated( bool allocated )
{
	m_bMapRenderInfoLoaded = allocated;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mod - 
//-----------------------------------------------------------------------------
void CModelLoader::Map_LoadDisplacements( model_t *pModel, bool bRestoring )
{
	if ( !pModel )
	{
		Assert( false );
		return;
	}

	V_FileBase( pModel->szPathName, m_szBaseName, sizeof( m_szBaseName ) );
	CMapLoadHelper::Init( pModel, pModel->szPathName );

    DispInfo_LoadDisplacements( pModel, bRestoring );

	CMapLoadHelper::Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: List the model dictionary
//-----------------------------------------------------------------------------
void CModelLoader::Print( void )
{
	ConMsg( "Models:\n" );
	int c = m_Models.Count();
	for ( int i = 0; i < c; i++ )
	{
		model_t *pModel = m_Models[i].modelpointer;
		if ( pModel->type == mod_studio || pModel->type == mod_bad )
		{
			// studio models have ref counts
			// bad models are unloaded models which need to be listed
			int refCount = ( pModel->type == mod_studio ) ? g_pMDLCache->GetRef( pModel->studio ) : 0;
			ConMsg( "%4d: Flags:0x%8.8x RefCount:%2d %s\n", i, pModel->nLoadFlags, refCount, pModel->szPathName );
		}
		else
		{
			ConMsg( "%4d: Flags:0x%8.8x %s\n", i, pModel->nLoadFlags, pModel->szPathName );
		}
	}
}

//-----------------------------------------------------------------------------
// Calls utility function to create .360 version of a file.
//-----------------------------------------------------------------------------
int CModelLoader::UpdateOrCreate( const char *pSourceName, char *pTargetName, int targetLen, bool bForce )
{
#if defined( _GAMECONSOLE )
	return ::UpdateOrCreate( pSourceName, pTargetName, targetLen, NULL, NULL, bForce );
#else
	return UOC_NOT_CREATED;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Determine if specified .bsp is valid
// Input  : *mapname - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CModelLoader::Map_IsValid( char const *pBaseMapName, bool bQuiet /* = false */ )
{
	static char	s_szBaseMapName[MAX_PATH];

	if ( !pBaseMapName || !pBaseMapName[0] )
	{
		if ( !bQuiet )
		{
			ConMsg( "CModelLoader::Map_IsValid:  Empty mapname!!!\n" );
		}
		return false;
	}

	if ( ( IsGameConsole() || sv.IsDedicated() ) && !V_stricmp( pBaseMapName, s_szBaseMapName ) )
	{
		// already been checked, no reason to do multiple i/o validations
		return true;
	}

	FileHandle_t mapfile;
	char mapname[MAX_PATH];
	V_snprintf( mapname, sizeof( mapname ), "maps/%s.bsp", pBaseMapName );
	V_FixSlashes( mapname );

	if ( IsGameConsole() )
	{
		char szMapName360[MAX_PATH];
		UpdateOrCreate( mapname, szMapName360, sizeof( szMapName360 ), false );
		Q_strncpy( mapname, szMapName360, sizeof( mapname ) );
	}

	mapfile = g_pFileSystem->OpenEx( mapname, "rb", IsGameConsole() ? FSOPEN_NEVERINPACK : 0, "GAME" );
	if ( mapfile != FILESYSTEM_INVALID_HANDLE )
	{
		BSPHeader_t header;
		memset( &header, 0, sizeof( header ) );
		g_pFileSystem->Read( &header, sizeof( BSPHeader_t ), mapfile );
		g_pFileSystem->Close( mapfile );

		if ( header.ident == IDBSPHEADER )
		{
			if ( header.m_nVersion >= MINBSPVERSION && header.m_nVersion <= BSPVERSION )
			{
				V_strncpy( s_szBaseMapName, pBaseMapName, sizeof( s_szBaseMapName ) );
				return true;
			}
			else
			{
				if ( !bQuiet )
				{
					Warning( "CModelLoader::Map_IsValid:  Map '%s' bsp version %i, expecting %i\n", mapname, header.m_nVersion, BSPVERSION );
				}

			}
		}
		else
		{
			if ( !bQuiet )
			{
				Warning( "CModelLoader::Map_IsValid: '%s' is not a valid BSP file\n", mapname );
			}
		}
	}
	else
	{
		if ( !bQuiet )
		{
			Warning( "CModelLoader::Map_IsValid:  No such map '%s'\n", mapname );
		}
	}

	// Get outta here if we are checking vidmemstats.
	if ( CommandLine()->CheckParm( "-dumpvidmemstats" ) )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );
	}

	return false;
}

model_t *CModelLoader::FindModelNoCreate( const char *pModelName )
{
	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( pModelName );
	int i = m_Models.Find( fnHandle );
	if ( i != m_Models.InvalidIndex() )
	{
		return m_Models[i].modelpointer;
	}

	// not found
	return NULL;
}

modtype_t CModelLoader::GetTypeFromName( const char *pModelName )
{
	// HACK HACK, force sprites to correctly
	const char *pExt = V_GetFileExtension( pModelName );
	if ( pExt )
	{
		if ( !V_stricmp( pExt, "spr" ) || !V_stricmp( pExt, "vmt" ) || !V_stricmp( pExt, "avi" ) || !V_stricmp( pExt, "bik" ) )
		{
			return mod_sprite;
		}
		else if ( !V_stricmp( pExt, "bsp" ) )
		{
			return mod_brush;
		}
		else if ( !V_stricmp( pExt, "mdl" ) )
		{
			return mod_studio;
		}
	}

	return mod_bad;
}

int	CModelLoader::FindNext( int iIndex, model_t **ppModel )
{
	if ( iIndex == -1 && m_Models.Count() )
	{
		iIndex = m_Models.FirstInorder();
	}
	else if ( !m_Models.Count() || !m_Models.IsValidIndex( iIndex ) )
	{
		*ppModel = NULL;
		return -1;
	}

	*ppModel = m_Models[iIndex].modelpointer;
	
	iIndex = m_Models.NextInorder( iIndex );
	if ( iIndex == m_Models.InvalidIndex() )
	{
		// end of list
		iIndex = -1;
	}

	return iIndex;
}

void CModelLoader::UnloadModel( model_t *pModel )
{
	switch ( pModel->type )
	{
	case mod_brush:
		// Let it free data and call destructors.
		Map_UnloadModel( pModel );

		// Re-write .bsp if networkstringtable dictionary was updated.
		g_pStringTableDictionary->OnBSPFullyUnloaded();

		// Remove from file system
		char szNameOnDisk[MAX_PATH];
		GetMapPathNameOnDisk( szNameOnDisk, pModel->szPathName, sizeof( szNameOnDisk ) );
		g_pFileSystem->RemoveSearchPath( szNameOnDisk, "GAME" );
		break;

	case mod_studio:
		Studio_UnloadModel( pModel );
		break;

	case mod_sprite:
		Sprite_UnloadModel( pModel );
		break;
	}
	if ( pModel->m_pKeyValues )
	{
		pModel->m_pKeyValues->deleteThis();
		pModel->m_pKeyValues = NULL;
	}
}

bool CModelLoader::IsModelInWeaponCache( const model_t *pModel )
{
#if !defined( DEDICATED )
	if ( m_bAllowWeaponModelCache )
	{
		int nMapIndex = m_WeaponModelCache.Find( (model_t*)pModel );
		return ( nMapIndex != g_ModelLoader.m_WeaponModelCache.InvalidIndex() );
	}
#endif
	return false;
}

bool CModelLoader::IsViewWeaponModelResident( const model_t *pModel )
{
#if !defined( DEDICATED )
	if ( m_bAllowWeaponModelCache && ( pModel->flags & MODELFLAG_VIEW_WEAPON_MODEL ) )
	{
		// only view weapon models might have their verts evicted
		int nMapIndex = m_WeaponModelCache.Find( (model_t*)pModel );
		if ( nMapIndex != g_ModelLoader.m_WeaponModelCache.InvalidIndex() )
		{
			return g_ModelLoader.m_WeaponModelCache[nMapIndex]->m_bStudioHWDataResident;
		}
	}
#endif
	return true;
}

void CModelLoader::EvictAllWeaponsFromModelCache( bool bLoadingComplete )
{
#if !defined( DEDICATED )
	if ( !m_bAllowWeaponModelCache )
		return;

	// for reliability, ensure the studiohwdata presence is accurate
	// only need to do this once as the baseline eviction state is established
	for ( int i = m_WeaponModelCache.FirstInorder(); i != m_WeaponModelCache.InvalidIndex(); i = m_WeaponModelCache.NextInorder( i ) )
	{
		if ( !m_WeaponModelCache[i]->m_bViewModel )
			continue;

		m_WeaponModelCache[i]->m_bStudioHWDataResident = g_pMDLCache->IsDataLoaded( m_WeaponModelCache.Key( i )->studio, MDLCACHE_STUDIOHWDATA );
	}

	// forcefully evict all the known weapon models
	for ( int i = m_WeaponModelCache.FirstInorder(); i != m_WeaponModelCache.InvalidIndex(); i = m_WeaponModelCache.NextInorder( i ) )
	{
		EvictWeaponModel( i, true );
	}

	g_pMaterialSystem->ClearForceExcludes();

	if ( bLoadingComplete && !m_bAllowWorldWeaponEviction )
	{
		// loading is complete, put all the world weapons back
		for ( int i = m_WeaponModelCache.FirstInorder(); i != m_WeaponModelCache.InvalidIndex(); i = m_WeaponModelCache.NextInorder( i ) )
		{
			if ( m_WeaponModelCache[i]->m_bViewModel )
			{
				// ignore non-desired model type
				continue;
			}

			// ensure world model is restored
			RestoreWeaponModel( i );
		}
	}
#endif
}

void CModelLoader::UpdateViewWeaponModelCache( const char **ppWeaponModels, int nWeaponModels )
{
#if !defined( DEDICATED )
	if ( !m_bAllowWeaponModelCache )
		return;

	// age any orphaned view models
	// owned view models will have their age reset below
	int nNumResident = 0;
	m_nNumWeaponsPartialResident = 0;
	for ( int i = m_WeaponModelCache.FirstInorder(); i != m_WeaponModelCache.InvalidIndex(); i = m_WeaponModelCache.NextInorder( i ) )
	{
		if ( !m_WeaponModelCache[i]->m_bViewModel )
			continue;

		if ( m_WeaponModelCache[i]->m_nAgeTime == UINT_MAX )
		{
			// one time age tag
			// the oldest will be the smallest and identify a LRU (i.e. thrown out from the player's inventory)
			m_WeaponModelCache[i]->m_nAgeTime = Plat_MSTime();
		}

		if ( m_WeaponModelCache[i]->m_nAgeTime )
		{
			nNumResident++;
		}
		else if ( m_WeaponModelCache[i]->m_bStudioHWDataResident )
		{
			// not resident, but hw mesh is and it should not be
			// retry eviction after any weapons have been restored
			m_nNumWeaponsPartialResident++;
		}
	}

	for ( int i = 0; i < nWeaponModels; i++ )
	{
		// resolve from name to model
		model_t* pModel = FindModelNoCreate( ppWeaponModels[i] );
		if ( !pModel )
			continue;

		// find view model
		int nIndex = m_WeaponModelCache.Find( pModel );
		if ( nIndex != m_WeaponModelCache.InvalidIndex() )
		{
			if ( !m_WeaponModelCache[nIndex]->m_bViewModel )
				continue;

			RestoreWeaponModel( nIndex );

			// LRU bump any known player inventory view models
			// all view model's currently within primary player's inventory do not age AT ALL
			// marked uniquely to not be subject to any LRU eviction
			// this ensures that weapon switching avoids any restore latency
			m_WeaponModelCache[nIndex]->m_nAgeTime = UINT_MAX;
		}
	}

	// drive the view model cache down to its desired size
	while ( mod_weaponviewmodelcache.GetInt() && nNumResident > mod_weaponviewmodelcache.GetInt() )
	{
		// find suitable candidates for discard
		// will NOT consider view model's in the primary inventory (i.e at UINT_MAX)
		unsigned int nOldestTime = UINT_MAX;
		int nOldestIndex = m_WeaponModelCache.InvalidIndex();
		for ( int i = m_WeaponModelCache.FirstInorder(); i != m_WeaponModelCache.InvalidIndex(); i = m_WeaponModelCache.NextInorder( i ) )
		{
			if ( m_WeaponModelCache[i]->m_bViewModel && m_WeaponModelCache[i]->m_nAgeTime && nOldestTime > m_WeaponModelCache[i]->m_nAgeTime )
			{
				nOldestIndex = i;
				nOldestTime = m_WeaponModelCache[i]->m_nAgeTime;
			}
		}

		if ( nOldestIndex == m_WeaponModelCache.InvalidIndex() )
		{
			// there was no suitable oldest candidate this frame
			break;
		}

		EvictWeaponModel( nOldestIndex, false );
		nNumResident--;
	}
#endif
}

void CModelLoader::TouchWorldWeaponModelCache( const char **ppWeaponModels, int nWeaponModels )
{
#if !defined( DEDICATED )
	if ( !m_bAllowWeaponModelCache || !m_bAllowWorldWeaponEviction )
		return;

	// touch the world weapons
	unsigned int nCurrentTime = Plat_MSTime();
	for ( int i = 0; i < nWeaponModels; i++ )
	{
		// resolve from name to model
		model_t* pModel = FindModelNoCreate( ppWeaponModels[i] );
		if ( !pModel )
			continue;

		// touch world model
		int nIndex = m_WeaponModelCache.Find( pModel );
		if ( nIndex != m_WeaponModelCache.InvalidIndex() )
		{
			if ( m_WeaponModelCache[nIndex]->m_bViewModel )
				continue;

			RestoreWeaponModel( nIndex );

			m_WeaponModelCache[nIndex]->m_nAgeTime = nCurrentTime;
		}
	}

	int nNumResident = 0;
	for ( int i = m_WeaponModelCache.FirstInorder(); i != m_WeaponModelCache.InvalidIndex(); i = m_WeaponModelCache.NextInorder( i ) )
	{
		if ( m_WeaponModelCache[i]->m_bViewModel )
			continue;

		if ( m_WeaponModelCache[i]->m_nAgeTime )
		{
			nNumResident++;
		}
	}

	// drive the world model cache down to its desired size
	while ( mod_weaponworldmodelcache.GetInt() && nNumResident > mod_weaponworldmodelcache.GetInt() )
	{
		// find suitable candidates for discard
		unsigned int nOldestTime = UINT_MAX;
		int nOldestIndex = m_WeaponModelCache.InvalidIndex();
		for ( int i = m_WeaponModelCache.FirstInorder(); i != m_WeaponModelCache.InvalidIndex(); i = m_WeaponModelCache.NextInorder( i ) )
		{
			if ( m_WeaponModelCache[i]->m_bViewModel )
				continue;

			if ( m_WeaponModelCache[i]->m_nAgeTime && nOldestTime > m_WeaponModelCache[i]->m_nAgeTime )
			{
				// prevent world model thrashing by only considering for purging aged world models beyond a minimum age
				if ( nCurrentTime - m_WeaponModelCache[i]->m_nAgeTime >= (unsigned int)mod_weaponworldmodelminage.GetInt() )
				{
					nOldestIndex = i;
					nOldestTime = m_WeaponModelCache[i]->m_nAgeTime;
				}
			}
		}

		if ( nOldestIndex == m_WeaponModelCache.InvalidIndex() )
		{
			// there was no suitable oldest candidate this frame
			break;
		}

		EvictWeaponModel( nOldestIndex, false );
		nNumResident--;
	}
#endif
}

void CModelLoader::EvictWeaponModel( int nCacheIndex, bool bForce )
{
#if !defined( DEDICATED )
	if ( !m_bAllowWeaponModelCache || !m_WeaponModelCache.IsValidIndex( nCacheIndex ) )
		return;

	if ( !bForce && !m_WeaponModelCache[nCacheIndex]->m_nAgeTime )
	{
		// already evicted
		return;
	}

	if ( m_WeaponModelCache.Key( nCacheIndex )->studio == MDLHANDLE_INVALID )
	{
		// this model was explicitly unloaded
		// ignore this model until the model loader restores it
		return;
	}

	DevMsg( "Evicting: %s\n", m_WeaponModelCache.Key( nCacheIndex )->szPathName );

	for ( int i = 0; i < m_WeaponModelCache[nCacheIndex]->m_Materials.Count(); i++ )
	{
		IMaterial *pMaterial = materials->FindMaterial( m_WeaponModelCache[nCacheIndex]->m_Materials[i].Get(), TEXTURE_GROUP_OTHER, false );
		if ( !pMaterial || pMaterial->IsErrorMaterial() )
		{
			// cannot evict the material that should have been there
			AssertMsg( false, CFmtStr( "EvictWeaponModel( %s ): Could not find expected valid material %s\n", m_WeaponModelCache.Key( nCacheIndex )->szPathName, m_WeaponModelCache[nCacheIndex]->m_Materials[i].Get() ).Access() );
			continue;
		}

		// force material to temporarily consume less memory
		// force worldmodels and viewmodels to be very small versions
		// as viewmodels are restored they will render temporarily with their small versions, but the pop is not noticeable
		pMaterial->SetTempExcluded( true, 16 );
	}

	// mark as evicted, the actual mesh eviction is deferred until frame boundary in ProcessWeaponModelCacheOperations()
	m_WeaponModelCache[nCacheIndex]->m_nAgeTime = 0;

	if ( bForce )
	{
		// THIS IS NOT FOR GENERAL-PURPOSE ANYTIME USE - IT WILL DESTABILIZE THE CONSOLES
		// Force the model processing operations to do the evict (which can only be done when rendering is stopped (i.e. at least no queued model draws) ).
		m_nNumWeaponsPartialResident = 1;
		ProcessWeaponModelCacheOperations();
	}
#endif
}

void CModelLoader::RestoreWeaponModel( int nCacheIndex )
{
#if !defined( DEDICATED )
	if ( !m_bAllowWeaponModelCache || !m_WeaponModelCache.IsValidIndex( nCacheIndex ) )
		return;

	if ( m_WeaponModelCache[nCacheIndex]->m_nAgeTime )
	{
		// already restored
		return;
	}

	if ( m_WeaponModelCache.Key( nCacheIndex )->studio == MDLHANDLE_INVALID )
	{
		// this model was explicitly unloaded
		// ignore this model until the model loader restores it
		return;
	}

	DevMsg( "Restoring: %s\n", m_WeaponModelCache.Key( nCacheIndex )->szPathName );

	for ( int i = 0; i < m_WeaponModelCache[nCacheIndex]->m_Materials.Count(); i++ )
	{
		IMaterial *pMaterial = materials->FindMaterial( m_WeaponModelCache[nCacheIndex]->m_Materials[i].Get(), TEXTURE_GROUP_OTHER, false );
		if ( !pMaterial || pMaterial->IsErrorMaterial() )
		{
			// cannot restore the material that should have been there
			AssertMsg( false, CFmtStr( "RestoreWeaponModel( %s ): Could not find expected valid material %s\n", m_WeaponModelCache.Key( nCacheIndex )->szPathName, m_WeaponModelCache[nCacheIndex]->m_Materials[i].Get() ).Access() );
			continue;
		}

		// force material back to its original memory state
		pMaterial->SetTempExcluded( false );
	}

	// only view model weapons could have had their vertexes evicted
	if ( m_bAllowWeaponVertexEviction && m_WeaponModelCache[nCacheIndex]->m_bViewModel && !m_WeaponModelCache[nCacheIndex]->m_bStudioHWDataResident )
	{
		g_pMDLCache->RestoreHardwareData( m_WeaponModelCache.Key( nCacheIndex )->studio, &m_WeaponModelCache[nCacheIndex]->m_hAsyncVTXControl, &m_WeaponModelCache[nCacheIndex]->m_hAsyncVVDControl );
	}

	// mark as restored
	m_WeaponModelCache[nCacheIndex]->m_nAgeTime = Plat_MSTime();
#endif
}

bool CModelLoader::ProcessWeaponModelCacheOperations()
{
	bool bMeshedRestored = false;

#if !defined( DEDICATED )
	if ( !ThreadInMainThread() )
	{
		// must be on main thread to do any of the mesh eviction/restore
		return false;
	}

	if ( m_bAllowWeaponVertexEviction )
	{
		// restore all the pending meshes
		bMeshedRestored |= g_pMDLCache->ProcessPendingHardwareRestore();

		// avoid doing any work unless pre-qualified by UpdateViewModelCache() that evictions need to occur
		if ( m_nNumWeaponsPartialResident )
		{
			// It's possible that the hw mesh eviction could not occur due to frame lock or
			// it was accessed by code that was unaware of this violation and restored it.
			for ( int i = m_WeaponModelCache.FirstInorder(); i != m_WeaponModelCache.InvalidIndex(); i = m_WeaponModelCache.NextInorder( i ) )
			{
				if ( !m_WeaponModelCache[i]->m_bViewModel )
					continue;

				if ( !m_WeaponModelCache[i]->m_nAgeTime && m_WeaponModelCache[i]->m_bStudioHWDataResident )
				{
					// expect model to be evicted but hw mesh is still resident and it should not be
					DevMsg( "*** Evicting: %s\n", m_WeaponModelCache.Key( i )->szPathName );

					// ensure any outstanding prior async hwdata restore is complete
					// cannot have an outstanding async restore operation in-flight while trying to evict
					if ( m_WeaponModelCache[i]->m_hAsyncVTXControl || m_WeaponModelCache[i]->m_hAsyncVVDControl )
					{
						// do the sync finish in sequence to ensure the io callback completes
						// if the async operation have already completed, these will do nothing as expected
						g_pFullFileSystem->AsyncFinish( m_WeaponModelCache[i]->m_hAsyncVTXControl, true );
						g_pFullFileSystem->AsyncFinish( m_WeaponModelCache[i]->m_hAsyncVVDControl, true );

						if ( m_WeaponModelCache[i]->m_hAsyncVTXControl )
						{
							// further safety to ensure the async i/o operation completed as requested
							FSAsyncStatus_t status = g_pFullFileSystem->AsyncStatus( m_WeaponModelCache[i]->m_hAsyncVTXControl );
							if ( status != FSASYNC_STATUS_PENDING && status != FSASYNC_STATUS_INPROGRESS && status != FSASYNC_STATUS_UNSERVICED )
							{
								// operation was completed
								// release the handle to avoid leak
								g_pFullFileSystem->AsyncRelease( m_WeaponModelCache[i]->m_hAsyncVTXControl );
								m_WeaponModelCache[i]->m_hAsyncVTXControl = NULL;
							}
						}

						if ( m_WeaponModelCache[i]->m_hAsyncVVDControl )
						{
							// further safety to ensure the async i/o operation completed as requested
							FSAsyncStatus_t status = g_pFullFileSystem->AsyncStatus( m_WeaponModelCache[i]->m_hAsyncVVDControl );
							if ( status != FSASYNC_STATUS_PENDING && status != FSASYNC_STATUS_INPROGRESS && status != FSASYNC_STATUS_UNSERVICED )
							{
								// operation was completed
								// release the handle to avoid leak
								g_pFullFileSystem->AsyncRelease( m_WeaponModelCache[i]->m_hAsyncVVDControl );
								m_WeaponModelCache[i]->m_hAsyncVVDControl = NULL;
							}
						}
					}

					if ( !m_WeaponModelCache[i]->m_hAsyncVTXControl && !m_WeaponModelCache[i]->m_hAsyncVVDControl )
					{
						// This is unfortunate and ideally is a no-op. The AsyncFinish() only finished the i/o and not the actual mesh restore.
						// The mesh must be restored for the evict logic to work properly. ALL the pending restores must be drained to ensure this
						// possible mesh (whose i/o was just forced to complete) performs its build out. In practice this list should be nominally
						// empty or one, and not really more than that.
						bMeshedRestored |= g_pMDLCache->ProcessPendingHardwareRestore();

						// only an evicted view model can flush all their vertexes because we don't expect it to render or be queried
						// the expected contract is that an evicted viewmodel won't be rendering anytime soon
						// it's possible that the eviction does not occur due to frame locks or some other preventing condition
						// the next UpdateViewWeaponModelCache() will catch these for another retry
						g_pMDLCache->Flush( m_WeaponModelCache.Key( i )->studio, MDLCACHE_FLUSH_STUDIOHWDATA|MDLCACHE_FLUSH_ANIMBLOCK|MDLCACHE_FLUSH_VIRTUALMODEL|MDLCACHE_FLUSH_AUTOPLAY|MDLCACHE_FLUSH_VERTEXES );
					}
				}
			}

			// assume all evicted, the UpdateViewModelCache() will update
			m_nNumWeaponsPartialResident = 0;
		}
	}
#endif

	return bMeshedRestored;
}

void CModelLoader::DumpWeaponModelCache( bool bViewModelsOnly )
{
#if !defined( DEDICATED )
	if ( !m_bAllowWeaponModelCache )
		return;

	Color defaultColor( 0, 0, 0, 255 );
	Color anomalyColor( 255, 0, 0, 255 );

	Msg( "Weapon %s Model Cache:\n", bViewModelsOnly ? "View" : "World" );
	Msg( "------------------------\n" );
	for ( int nPass = 0; nPass < 2; nPass++ )
	{
		if ( !nPass )
		{
			Con_ColorPrintf( Color( 0, 255, 255, 255 ), "\nEvicted:--------------------------------\n" );
		}
		else
		{
			Con_ColorPrintf( Color( 0, 255, 255, 255 ), "\nResident:-------------------------------\n" );
		}

		for ( int i = m_WeaponModelCache.FirstInorder(); i != m_WeaponModelCache.InvalidIndex(); i = m_WeaponModelCache.NextInorder( i ) )
		{
			if ( bViewModelsOnly != m_WeaponModelCache[i]->m_bViewModel )
			{
				// ignore non-desired model type
				continue;
			}

			if ( !nPass && m_WeaponModelCache[i]->m_nAgeTime )
			{
				// ignore resident
				continue;
			}
			else if ( nPass && !m_WeaponModelCache[i]->m_nAgeTime )
			{
				// ignore evicted
				continue;
			}
			
			Color color = defaultColor;
			if ( bViewModelsOnly && !m_WeaponModelCache[i]->m_nAgeTime && m_WeaponModelCache[i]->m_bStudioHWDataResident )
			{
				color = anomalyColor;
			}
			Con_ColorPrintf( color, "\nModel: %s\n", m_WeaponModelCache.Key( i )->szPathName );
			
			Con_ColorPrintf( defaultColor, "  Age: %u\n", m_WeaponModelCache[i]->m_nAgeTime );
			if ( bViewModelsOnly )
			{
				Con_ColorPrintf( defaultColor, "  %sStudioHWData\n", m_WeaponModelCache[i]->m_bStudioHWDataResident ? "+" : "-");
			}

			for ( int j = 0; j < m_WeaponModelCache[i]->m_Materials.Count(); j++ )
			{
				IMaterial *pMaterial = materials->FindMaterial( m_WeaponModelCache[i]->m_Materials[j].Get(), TEXTURE_GROUP_OTHER, false );
				Con_ColorPrintf( defaultColor, "  Material: %s (Ref: %d)\n", m_WeaponModelCache[i]->m_Materials[j].Get(), pMaterial->GetReferenceCount() );

				int nMatParamCount = pMaterial->ShaderParamCount();
				IMaterialVar **ppMatVars = pMaterial->GetShaderParams();
				for ( int nParam = 0; nParam < nMatParamCount; nParam++ )
				{
					IMaterialVar *pVar = ppMatVars[nParam];
					if ( !pVar || 
						pVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE || 
						pVar->IsTextureValueInternalEnvCubemap() )
					{
						// not possible to temp exclude these, so not interested
						continue;
					}

					ITexture *pTex = pVar->GetTextureValue();
					if ( !pTex )
					{
						// not possible to temp exclude these, so not interested
						continue;
					}
					
					Con_ColorPrintf( 
						pTex->CanBeTempExcluded() ? Color( 0, 0, 0, 255 ) : Color( 255, 255, 0, 255 ), 
						"    %sTexture: %s (Ref: %d)\n", 
						pTex->IsTempExcluded() ? "-" : "+", 
						pTex->GetName(), 
						pTex->GetReferenceCount() );
				}
			}
		}
	}
#endif
}

model_t *CModelLoader::GetDynamicModel( const char *name, bool bClientOnly )
{
	model_t *pModel = FindModel( name );
	Assert( pModel );

	CDynamicModelInfo &dyn = m_DynamicModels[ m_DynamicModels.Insert( pModel ) ]; // Insert returns existing if key is already set
	if ( dyn.m_nLoadFlags == CDynamicModelInfo::INVALIDFLAG )
	{
		dyn.m_nLoadFlags = 0;
		DynamicModelDebugMsg( "model %p [%s] registered\n", pModel, pModel->szPathName );
	}
	dyn.m_uLastTouchedMS_Div256 = Plat_MSTime() >> 8;

	return pModel;
}


model_t *CModelLoader::GetDynamicCombinedModel( const char *name, bool bClientOnly )
{
	model_t *pModel = FindModel( name );
	Assert( pModel );

	CDynamicModelInfo &dyn = m_DynamicModels[ m_DynamicModels.Insert( pModel ) ]; // Insert returns existing if key is already set
	if ( dyn.m_nLoadFlags == CDynamicModelInfo::INVALIDFLAG )
	{
		dyn.m_nLoadFlags = CDynamicModelInfo::COMBINED | CDynamicModelInfo::READY;
		pModel->type = mod_studio;
		pModel->nLoadFlags = FMODELLOADER_COMBINED | ( bClientOnly ? FMODELLOADER_DYNCLIENT : FMODELLOADER_DYNSERVER );
		dyn.m_iRefCount = 1;
		dyn.m_iClientRefCount = ( bClientOnly ? 1 : 0 );
		DynamicModelDebugMsg( "model %p [%s] registered\n", pModel, pModel->szPathName );
	}
	else
	{
		pModel->nLoadFlags |= ( bClientOnly ? FMODELLOADER_DYNCLIENT : FMODELLOADER_DYNSERVER );
	}

	dyn.m_uLastTouchedMS_Div256 = Plat_MSTime() >> 8;

	return pModel;
}


void CModelLoader::UpdateDynamicCombinedModel( model_t *pModel, MDLHandle_t Handle, bool bClientSide )
{
	if ( pModel->studio != MDLHANDLE_INVALID && pModel->studio != Handle )
	{
		UnloadModel( pModel );
	}
	pModel->studio = Handle;
	pModel->type = mod_studio;
	pModel->nLoadFlags = IModelLoader::FMODELLOADER_LOADED | FMODELLOADER_COMBINED | ( bClientSide ? FMODELLOADER_DYNCLIENT : FMODELLOADER_DYNSERVER );
	pModel->nServerCount = Host_GetServerCount();
	g_pMDLCache->SetUserData( pModel->studio, pModel );
}


bool CModelLoader::SetCombineModels( model_t *pModel, const CUtlVector< SCombinerModelInput_t > &vecModelsToCombine )
{
	if ( !pModel )
		return false;

	return g_pMDLCache->SetCombineModels( pModel->studio, vecModelsToCombine );
}

bool CModelLoader::FinishCombinedModel( model_t *pModel, CombinedModelLoadedCallback pFunc, void *pUserData )
{
	return g_pMDLCache->FinishCombinedModel( pModel->studio, pFunc, pUserData );
}


void CModelLoader::UpdateDynamicModelLoadQueue()
{
	if ( mod_dynamicloadpause.GetBool() )
		return;

	static double s_LastDynamicLoadTime = 0.0;
	if ( mod_dynamicloadthrottle.GetFloat() > 0 && Plat_FloatTime() < s_LastDynamicLoadTime + mod_dynamicloadthrottle.GetFloat() )
		return;

	if ( m_bDynamicLoadQueueHeadActive )
	{
		Assert( m_DynamicModelLoadQueue.Count() >= 1 );
		MaterialLock_t matLock = g_pMaterialSystem->Lock(); // ASDFADFASFASEGAafliejsfjaslaslgsaigas
		bool bComplete = true;
		//bool bComplete = g_pQueuedLoader->CompleteDynamicLoad();
		g_pMaterialSystem->Unlock(matLock);

		if ( bComplete )
		{
			model_t *pModel = m_DynamicModelLoadQueue[0];
			m_DynamicModelLoadQueue.Remove(0);
			m_bDynamicLoadQueueHeadActive = false;

			Assert( pModel->nLoadFlags & FMODELLOADER_DYNAMIC );
			Assert( pModel->type == mod_bad || ( pModel->nLoadFlags & (FMODELLOADER_LOADED | FMODELLOADER_LOADED_BY_PRELOAD) ) );
			(void) LoadModel( pModel, NULL );
			Assert( pModel->type == mod_studio );

			UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
			Assert( hDyn != m_DynamicModels.InvalidHandle() );
			if ( hDyn != m_DynamicModels.InvalidHandle() )
			{
				CDynamicModelInfo &dyn = m_DynamicModels[hDyn];
				Assert( dyn.m_nLoadFlags & CDynamicModelInfo::QUEUED );
				Assert( dyn.m_nLoadFlags & CDynamicModelInfo::LOADING );

				dyn.m_nLoadFlags &= ~( CDynamicModelInfo::QUEUED | CDynamicModelInfo::LOADING );

				g_pMDLCache->LockStudioHdr( pModel->studio );
				dyn.m_nLoadFlags |= CDynamicModelInfo::LOCKED;

				dyn.m_uLastTouchedMS_Div256 = Plat_MSTime() >> 8;

				FinishDynamicModelLoadIfReady( &dyn, pModel );
			}

			s_LastDynamicLoadTime = Plat_FloatTime();
		}
	}

	// If we're not working, and we have work to do, and the queued loader is open for business...
	if ( !m_bDynamicLoadQueueHeadActive && m_DynamicModelLoadQueue.Count() > 0 && g_pQueuedLoader->IsFinished() )
	{
		model_t *pModel = m_DynamicModelLoadQueue[0];
		UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
		Assert( hDyn != m_DynamicModels.InvalidHandle() );
		if ( hDyn != m_DynamicModels.InvalidHandle() )
		{
			m_bDynamicLoadQueueHeadActive = true;

			CDynamicModelInfo &dyn = m_DynamicModels[hDyn];
			Assert( dyn.m_nLoadFlags & CDynamicModelInfo::QUEUED );
			Assert( !(dyn.m_nLoadFlags & CDynamicModelInfo::LOADING) );
			Assert( !(dyn.m_nLoadFlags & CDynamicModelInfo::LOCKED) );
			dyn.m_nLoadFlags |= CDynamicModelInfo::LOADING;

			// the queued loader is very ... particular about path names. it doesn't like leading "models/"
			const char* pName = pModel->szPathName;
			int nLen = V_strlen( "models" );
			if ( StringHasPrefix( pName, "models" ) && ( pName[nLen] == '/' || pName[nLen] == '\\' ) )
			{
				pName += ( nLen + 1 );
			}

			MaterialLock_t matLock = g_pMaterialSystem->Lock();
			//g_pQueuedLoader->DynamicLoadMapResource( pName );
			g_pMaterialSystem->Unlock(matLock);
		}
		else
		{
			m_DynamicModelLoadQueue.Remove(0);
		}
	}
}

void CModelLoader::FinishDynamicModelLoadIfReady( CDynamicModelInfo *pDyn, model_t *pModel )
{
	CDynamicModelInfo &dyn = *pDyn;
	if ( ( dyn.m_nLoadFlags & CDynamicModelInfo::LOCKED ) && !( dyn.m_nLoadFlags & CDynamicModelInfo::SERVERLOADING ) )
	{
		// There ought to be a better way to plumb this through, but this should be ok...
		if ( sv.m_pDynamicModelTable )
		{
			int netidx = sv.m_pDynamicModelTable->FindStringIndex( pModel->szPathName );
			if ( netidx != INVALID_STRING_INDEX )
			{
				char nIsLoaded = 1;
				sv.m_pDynamicModelTable->SetStringUserData( netidx, 1, &nIsLoaded );
			}
		}

		DynamicModelDebugMsg( "model %p [%s] loaded\n", pModel, pModel->szPathName );

		dyn.m_nLoadFlags |= CDynamicModelInfo::READY;
		while ( dyn.m_Callbacks.Count() > 0 )
		{
			IModelLoadCallback* pCallback = dyn.m_Callbacks.Tail();
			UnregisterModelLoadCallback( pModel, pCallback );
			pCallback->OnModelLoadComplete( pModel );
		}
	}
}

bool CModelLoader::RegisterModelLoadCallback( model_t *pModel, IModelLoadCallback *pCallback, bool bCallImmediatelyIfLoaded )
{
	UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
	Assert( hDyn != m_DynamicModels.InvalidHandle() );
	if ( hDyn == m_DynamicModels.InvalidHandle() )
		return false;

	CDynamicModelInfo &dyn = m_DynamicModels[ hDyn ];
	AssertMsg( dyn.m_iRefCount > 0, "RegisterModelLoadCallback requires non-zero model refcount" );
	if ( dyn.m_nLoadFlags & CDynamicModelInfo::READY )
	{
		if ( !bCallImmediatelyIfLoaded )
			return false;

		pCallback->OnModelLoadComplete( pModel );
	}
	else
	{
		if ( !dyn.m_Callbacks.HasElement( pCallback ) )
		{
			dyn.m_Callbacks.AddToTail( pCallback );
			// Set registration count for callback pointer
			m_RegisteredDynamicCallbacks[ m_RegisteredDynamicCallbacks.Insert( pCallback, 0 ) ]++;
		}
	}

	return true;
}

bool CModelLoader::IsDynamicModelLoading( model_t *pModel )
{
	Assert( pModel->nLoadFlags & FMODELLOADER_DYNAMIC );
	UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
	Assert( hDyn != m_DynamicModels.InvalidHandle() );
	if ( hDyn != m_DynamicModels.InvalidHandle() )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[ hDyn ];
		AssertMsg( dyn.m_iRefCount > 0, "dynamic model state cannot be queried with zero refcount" );
		if ( dyn.m_iRefCount > 0 )
		{
			return !( dyn.m_nLoadFlags & CDynamicModelInfo::READY );
		}
	}
	return false;
}

void CModelLoader::AddRefDynamicModel( model_t *pModel, bool bClientSideRef  )
{
	extern IVModelInfo* modelinfo;

	UtlHashHandle_t hDyn = m_DynamicModels.Insert( pModel );
	CDynamicModelInfo& dyn = m_DynamicModels[ hDyn ];
	dyn.m_iRefCount++;
	dyn.m_iClientRefCount += ( bClientSideRef ? 1 : 0 );
	Assert( dyn.m_iRefCount > 0 );

	DynamicModelDebugMsg( "model %p [%s] addref %d (%d)\n", pModel, pModel->szPathName, dyn.m_iRefCount, dyn.m_iClientRefCount );

	if ( !( dyn.m_nLoadFlags & ( CDynamicModelInfo::QUEUED | CDynamicModelInfo::LOCKED ) ) )
	{
		QueueDynamicModelLoad( &dyn, pModel );

		// Try to kick it off asap if we aren't already busy.
		if ( !m_bDynamicLoadQueueHeadActive )
		{
			UpdateDynamicModelLoadQueue();
		}
	}
}

void CModelLoader::ReleaseDynamicModel( model_t *pModel, bool bClientSideRef )
{
	Assert( pModel->nLoadFlags & FMODELLOADER_DYNAMIC );
	UtlHashHandle_t hDyn = m_DynamicModels.Find( pModel );
	if ( hDyn != m_DynamicModels.InvalidHandle() )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[ hDyn ];
		Assert( dyn.m_iRefCount > 0 );
		if ( dyn.m_iRefCount > 0 )
		{
			DynamicModelDebugMsg( "model %p [%s] release %d (%dc)\n", pModel, pModel->szPathName, dyn.m_iRefCount, dyn.m_iClientRefCount );
			dyn.m_iRefCount--;
			dyn.m_iClientRefCount -= ( bClientSideRef ? 1 : 0 );
			Assert( dyn.m_iClientRefCount >= 0 );
			if ( dyn.m_iClientRefCount < 0 )
				dyn.m_iClientRefCount = 0;
			dyn.m_uLastTouchedMS_Div256 = Plat_MSTime() >> 8;
		}
	}
}

void CModelLoader::UnregisterModelLoadCallback( model_t *pModel, IModelLoadCallback *pCallback )
{
	if ( int *pCallbackRegistrationCount = m_RegisteredDynamicCallbacks.GetPtr( pCallback ) )
	{
		if ( pModel )
		{
			UtlHashHandle_t i = m_DynamicModels.Find( pModel );
			if ( i != m_DynamicModels.InvalidHandle() )
			{
				CDynamicModelInfo &dyn = m_DynamicModels[ i ];
				if ( dyn.m_Callbacks.FindAndFastRemove( pCallback ) )
				{
					if ( dyn.m_Callbacks.Count() == 0 )
					{
						dyn.m_Callbacks.Purge();
					}
					if ( --(*pCallbackRegistrationCount) == 0 )
					{
						m_RegisteredDynamicCallbacks.Remove( pCallback );
						return;
					}
				}
			}
		}
		else
		{
			for ( UtlHashHandle_t i = m_DynamicModels.FirstHandle(); i != m_DynamicModels.InvalidHandle(); i = m_DynamicModels.NextHandle(i) )
			{
				CDynamicModelInfo &dyn = m_DynamicModels[ i ];
				if ( dyn.m_Callbacks.FindAndFastRemove( pCallback ) )
				{
					if ( dyn.m_Callbacks.Count() == 0 )
					{
						dyn.m_Callbacks.Purge();
					}
					if ( --(*pCallbackRegistrationCount) == 0 )
					{
						m_RegisteredDynamicCallbacks.Remove( pCallback );
						return;
					}
				}
			}
		}
	}
}

void CModelLoader::QueueDynamicModelLoad( CDynamicModelInfo *dyn, model_t *mod )
{
	Assert( !(dyn->m_nLoadFlags & CDynamicModelInfo::QUEUED) );
	// Client-side entities have priority over server-side entities
	// because they are more likely to be used in UI elements. --henryg
	if ( dyn->m_iClientRefCount > 0 && m_DynamicModelLoadQueue.Count() > 1 )
	{
		m_DynamicModelLoadQueue.InsertAfter( 0, mod );
	}
	else
	{
		m_DynamicModelLoadQueue.AddToTail( mod );
	}
	dyn->m_nLoadFlags |= CDynamicModelInfo::QUEUED;
	mod->nLoadFlags |= ( dyn->m_iClientRefCount > 0 ? FMODELLOADER_DYNCLIENT : FMODELLOADER_DYNSERVER );
}

bool CModelLoader::CancelDynamicModelLoad( CDynamicModelInfo *dyn, model_t *mod )
{
	int i = m_DynamicModelLoadQueue.Find( mod );
	Assert( (i < 0) == !(dyn->m_nLoadFlags & CDynamicModelInfo::QUEUED) );
	if ( i >= 0 )
	{
		if ( i == 0 && m_bDynamicLoadQueueHeadActive )
		{
			// can't remove head of queue
			return false;
		}
		else
		{
			Assert( dyn->m_nLoadFlags & CDynamicModelInfo::QUEUED );
			m_DynamicModelLoadQueue.Remove( i );
			dyn->m_nLoadFlags &= ~CDynamicModelInfo::QUEUED;
			mod->nLoadFlags &= ~FMODELLOADER_DYNAMIC;
			return true;
		}
	}
	return false;
}

void CModelLoader::InternalUpdateDynamicModels( bool bForceFlushUnreferenced )
{
	const uint now = Plat_MSTime();
	const uint delay = bForceFlushUnreferenced ? 0 : (int)( clamp( mod_dynamicunloadtime.GetFloat(), 1.f, 600.f ) * 1000 );

	UpdateDynamicModelLoadQueue();

#ifdef _DEBUG
	extern CNetworkStringTableContainer *networkStringTableContainerServer;
	bool bPrevStringTableLockState = networkStringTableContainerServer->Lock( false );
#endif

	// Scan for models to unload. TODO: accelerate with a "models to potentially unload" list?
	UtlHashHandle_t i = m_DynamicModels.FirstHandle();
	while ( i != m_DynamicModels.InvalidHandle() )
	{
		model_t *pModel = m_DynamicModels.Key( i );
		CDynamicModelInfo& dyn = m_DynamicModels[ i ];

		// UNLOAD THIS MODEL if zero refcount and not currently loading, and either timed out or never loaded
		if ( dyn.m_iRefCount <= 0 && !(dyn.m_nLoadFlags & CDynamicModelInfo::LOADING) &&
			( ( now - (dyn.m_uLastTouchedMS_Div256 << 8) ) >= delay || !( dyn.m_nLoadFlags & CDynamicModelInfo::LOCKED ) ) )
		{
			// Remove from load queue
			if ( dyn.m_nLoadFlags & CDynamicModelInfo::QUEUED )
			{
				if ( !CancelDynamicModelLoad( &dyn, pModel ) )
				{
					// Couldn't remove from queue, advance to next entry and do not remove
					i = m_DynamicModels.NextHandle(i);
					continue;
				}
			}

			// Unlock studiohdr_t
			if ( dyn.m_nLoadFlags & CDynamicModelInfo::LOCKED )
			{
				g_pMDLCache->UnlockStudioHdr( pModel->studio );
			}

			// There ought to be a better way to plumb this through, but this should be ok...
			if ( sv.m_pDynamicModelTable )
			{
				int netidx = sv.m_pDynamicModelTable->FindStringIndex( pModel->szPathName );
				if ( netidx != INVALID_STRING_INDEX )
				{
					char nIsLoaded = 0;
					sv.m_pDynamicModelTable->SetStringUserData( netidx, 1, &nIsLoaded );
				}
			}

			if ( pModel->nLoadFlags & FMODELLOADER_DYNAMIC )
			{
				pModel->nLoadFlags &= ~FMODELLOADER_DYNAMIC;
				// Actually unload the model if all system references are gone
				if ( pModel->nLoadFlags & FMODELLOADER_REFERENCEMASK )
				{
					DynamicModelDebugMsg( "model %p [%s] unload - deferred: non-dynamic reference\n", pModel, pModel->szPathName );
				}
				else
				{
					DynamicModelDebugMsg( "model %p [%s] unload\n", pModel, pModel->szPathName );

					Studio_UnloadModel( pModel );

					if ( mod_dynamicunloadtextures.GetBool() )
					{
						if ( ICallQueue* pCallQueue = materials->GetRenderContext()->GetCallQueue() )
						{
							pCallQueue->QueueCall( materials, &IMaterialSystem::UncacheUnusedMaterials, false );
						}
						else
						{
							materials->UncacheUnusedMaterials();
						}
					}
				}
			}

			// Remove from table, advance to next entry
			i = m_DynamicModels.RemoveAndAdvance(i);
			continue;
		}

		// Advance to next entry in table
		i = m_DynamicModels.NextHandle(i);
	}

#ifdef _DEBUG
	networkStringTableContainerServer->Lock( bPrevStringTableLockState );
#endif
}

void CModelLoader::Client_OnServerModelStateChanged( model_t *pModel, bool bServerLoaded )
{
#ifndef DEDICATED
	// Listen server don't distinguish between server and client ready, never use SERVERLOADING flag
	if ( sv.IsActive() ) 
		return;

	UtlHashHandle_t i = m_DynamicModels.Find( pModel );
	if ( i != m_DynamicModels.InvalidHandle() )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[i];
		if ( !bServerLoaded )
		{
			if ( dyn.m_nLoadFlags & CDynamicModelInfo::READY )
				DynamicModelDebugMsg( "dynamic model [%s] loaded on client but not server! is this bad? unknown...", pModel->szPathName );
			// XXX DESIGN WART - WHAT IF A CLIENT-SIDE MODEL IS SHARED WITH A SERVER-SIDE MODEL?
			// The client side model may still be in use while the server side model is unloaded.
			// We don't clear the READY flag for this reason. This means that new dynamic uses of
			// the model on the client will trigger READY before the server is ready to show the
			// model, and the client may show wrong animation state or body groups temporarily.
			// Is this a real problem? We would require the ability for a model to be marked both
			// as client-side AND networked in order to fix it, with separate refcounts...
			//dyn.m_nLoadFlags &= ~CDynamicModelInfo::READY;
			dyn.m_nLoadFlags |= CDynamicModelInfo::SERVERLOADING;
		}
		else
		{
			dyn.m_nLoadFlags &= ~CDynamicModelInfo::SERVERLOADING;
			FinishDynamicModelLoadIfReady( &dyn, pModel );
		}
	}
#endif
}

void CModelLoader::ForceUnloadNonClientDynamicModels()
{
	UtlHashHandle_t i = m_DynamicModels.FirstHandle();
	while ( i != m_DynamicModels.InvalidHandle() )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[i];
		dyn.m_iRefCount = dyn.m_iClientRefCount;
		i = m_DynamicModels.NextHandle( i );
	}

	// Flush everything
	InternalUpdateDynamicModels( true );
}

// reconstruct the ambient lighting for a leaf at the given position in worldspace
void Mod_LeafAmbientColorAtPos( Vector *pOut, const Vector &pos, int leafIndex )
{
	for ( int i = 0; i < 6; i++ )
	{
		pOut[i].Init();
	}
	mleafambientindex_t *pAmbient = &host_state.worldbrush->m_pLeafAmbient[leafIndex];
	if ( !pAmbient->ambientSampleCount && pAmbient->firstAmbientSample )
	{
		// this leaf references another leaf, move there (this leaf is a solid leaf so it borrows samples from a neighbor)
		leafIndex = pAmbient->firstAmbientSample;
		pAmbient = &host_state.worldbrush->m_pLeafAmbient[leafIndex];
	}
	int count = pAmbient->ambientSampleCount;
	if ( count > 0 )
	{
		int start = host_state.worldbrush->m_pLeafAmbient[leafIndex].firstAmbientSample;
		mleafambientlighting_t *pSamples = host_state.worldbrush->m_pAmbientSamples + start;
		mleaf_t *pLeaf = &host_state.worldbrush->leafs[leafIndex];
		float totalFactor = 0;
		for ( int i = 0; i < count; i++ )
		{
			// do an inverse squared distance weighted average of the samples to reconstruct 
			// the original function

			// the sample positions are packed as leaf bounds fractions, compute
			Vector samplePos = pLeaf->m_vecCenter - pLeaf->m_vecHalfDiagonal;
			samplePos.x += float(pSamples[i].x) * pLeaf->m_vecHalfDiagonal.x * (2.0f / 255.0f);
			samplePos.y += float(pSamples[i].y) * pLeaf->m_vecHalfDiagonal.y * (2.0f / 255.0f);
			samplePos.z += float(pSamples[i].z) * pLeaf->m_vecHalfDiagonal.z * (2.0f / 255.0f);

			float dist = (samplePos - pos).LengthSqr();
			float factor = 1.0f / (dist + 1.0f);
			totalFactor += factor;
			for ( int j = 0; j < 6; j++ )
			{
				Vector v;
				ColorRGBExp32ToVector( pSamples[i].cube.m_Color[j], v );
				pOut[j] += v * factor;
			}
		}
		for ( int i = 0; i < 6; i++ )
		{
			pOut[i] *= (1.0f / totalFactor);
		}
	}
}

#if defined( _X360 ) || defined( _PS3 ) || defined( PLATFORM_WINDOWS_PC )

#if defined( PLATFORM_WINDOWS_PC )

struct xModelList_t
{
	char		name[MAX_PATH];
	int			dataSize;
	int			numVertices;
	int			triCount;
	int			dataSizeLod0;
	int			numVerticesLod0;
	int			triCountLod0;
	int			numBones;
	int			numParts;
	int			numLODs;
	int			numMeshes;
};

#endif // PLATFORM_WINDOWS_PC

int ComputeSize( studiohwdata_t *hwData, int *numVerts, int *pTriCount, bool onlyTopLod = false )
{
	unsigned size = 0;
	Assert(hwData && numVerts);
	int max_lod = (onlyTopLod ? 1 : hwData->m_NumLODs);
	*pTriCount = 0;
	for ( int i=0; i < max_lod; i++ )
	{
		studioloddata_t *pLOD = &hwData->m_pLODs[i];
		for ( int j = 0; j < hwData->m_NumStudioMeshes; j++ )
		{
			studiomeshdata_t *pMeshData = &pLOD->m_pMeshData[j];
			for ( int k = 0; k < pMeshData->m_NumGroup; k++ )
			{
				studiomeshgroup_t *pMeshGroup = &pMeshData->m_pMeshGroup[k];
				IMesh* mesh = pMeshGroup->m_pMesh;
				size += mesh->ComputeMemoryUsed();

				// This doesn't seem relevant since it has no bearing on GPU memory, but keeping it here
				// on the PC, since the reason it's being aded back in is to look at differences between
				// main and portal2.
#if defined( PLATFORM_WINDOWS_PC )
				size += 2 * pMeshGroup->m_NumVertices;	// Size of m_pGroupIndexToMeshIndex[] array
#endif

				*numVerts += mesh->VertexCount();
				Assert( mesh->VertexCount() == pMeshGroup->m_NumVertices );
				for ( int l = 0; l < pMeshGroup->m_NumStrips; ++l )
				{
					OptimizedModel::StripHeader_t *pStripData = &pMeshGroup->m_pStripData[l];
					*pTriCount += pStripData->numIndices / 3;
				}
			}
		}
	}
	return size;
}

// APSFIXME: needs to only do models that are resident, sizes might be wrong, i.e lacking compressed vert state?
CON_COMMAND( vx_model_list, "Dump models to VXConsole" )
{
	CUtlVector< xModelList_t > modelList;
	modelList.SetCount( modelloader->GetCount() );

	int numActualModels = 0;
	for ( int i = 0; i < modelList.Count(); i++ )
	{
		const char* name = "Unknown";
		int dataSizeLod0 = 0;
		int dataSize = 0;
		int numParts = 0;
		int numBones = 0;
		int numVertsLod0 = 0;
		int numVerts = 0;
		int numLODs = 0;
		int numMeshes = 0;
		int nTriCount = 0;
		int nTriCountLod0 = 0;

		model_t* model = modelloader->GetModelForIndex( i );
		if ( model )
		{
			// other model types are not interesting
			if ( model->type != mod_studio )
				continue;

			name = model->szPathName;
			studiohwdata_t *hwData = g_pMDLCache->GetHardwareData( model->studio );
			if ( hwData )
			{
				numMeshes = hwData->m_NumStudioMeshes;
				numLODs = hwData->m_NumLODs;
				dataSize = ComputeSize( hwData, &numVerts, &nTriCount, false );
				dataSizeLod0 = ComputeSize( hwData, &numVertsLod0, &nTriCountLod0, true );
			}

			studiohdr_t *pStudioHdr = (studiohdr_t *)modelloader->GetExtraData( model );
			numBones = pStudioHdr->numbones;
			numParts = pStudioHdr->numbodyparts;
		}

		xModelList_t &modelInfo = modelList[numActualModels];
		++numActualModels;
		strcpy( modelInfo.name, name );
		modelInfo.dataSize = dataSize;
		modelInfo.numVertices = numVerts;
		modelInfo.triCount = nTriCount;
		modelInfo.dataSizeLod0 = dataSizeLod0;
		modelInfo.numVerticesLod0 = numVertsLod0;
		modelInfo.triCountLod0 = nTriCountLod0;
		modelInfo.numParts = numParts;
		modelInfo.numBones = numBones;
		modelInfo.numLODs = numLODs;
		modelInfo.numMeshes = numMeshes;
	}

#if defined( _X360 )
	XBX_rModelList( numActualModels, modelList.Base() );
#elif defined( _PS3 )
	g_pValvePS3Console->ModelList( numActualModels, modelList.Base() ); // super stupid, it just gets copied into yet another cutlvec on the other side, but that's the way the 360 ver does it.
#elif defined( PLATFORM_WINDOWS_PC )
	
	extern IVEngineClient *engineClient;
	char csvFileName[ MAX_PATH ];
	Q_snprintf( csvFileName, MAX_PATH, "modellist_%s.csv", engineClient->GetLevelNameShort() );
	Msg( "Writing model list to ""%s""...\n", csvFileName );
	FileHandle_t fileHandle = g_pFullFileSystem->Open( csvFileName, "w" );
	g_pFullFileSystem->FPrintf( fileHandle, "Model,DataSize,Tris,Verts,DataSize (LOD0),Tris (LOD0),Verts (LOD0),Parts,Bones,LODs,Meshes\n" );

	for ( int i = 0; i < numActualModels; ++ i )
	{
		g_pFullFileSystem->FPrintf( fileHandle, "%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", 
			modelList[ i ].name, modelList[ i ].dataSize, modelList[ i ].triCount, modelList[ i ].numVertices, modelList[ i ].dataSizeLod0, modelList[ i ].triCountLod0, modelList[ i ].numVerticesLod0,
			modelList[ i ].numParts, modelList[ i ].numBones, modelList[ i ].numLODs, modelList[ i ].numMeshes );
	}

	g_pFullFileSystem->Close( fileHandle );
#endif // PLATFORM_WINDOWS_PC
}
#endif // _X360 || PLATFORM_WINDOWS_PC


CON_COMMAND_F( mod_dynamicmodeldebug, "debug spew for dynamic model loading", FCVAR_HIDDEN | FCVAR_DONTRECORD )
{
	((CModelLoader*)modelloader)->DebugPrintDynamicModels();
}

#include "server.h"
#ifndef DEDICATED
#include "client.h"
#endif
void CModelLoader::DebugPrintDynamicModels()
{
	Msg( "network table (server):\n" );
	if ( sv.m_pDynamicModelTable )
	{
		for ( int i = 0; i < sv.m_pDynamicModelTable->GetNumStrings(); ++i )
		{
			int dummy = 0;
			char* data = (char*) sv.m_pDynamicModelTable->GetStringUserData( i, &dummy );
			bool bLoadedOnServer = !(data && dummy && data[0] == 0);
			Msg( "%3i: %c %s\n", i, bLoadedOnServer ? '*' : ' ', sv.m_pDynamicModelTable->GetString(i) );
		}
	}

#ifndef DEDICATED
	Msg( "\nnetwork table (client):\n" );
	if ( GetBaseLocalClient().m_pDynamicModelTable )
	{
		for ( int i = 0; i < GetBaseLocalClient().m_pDynamicModelTable->GetNumStrings(); ++i )
		{
			int dummy = 0;
			char* data = (char*) GetBaseLocalClient().m_pDynamicModelTable->GetStringUserData( i, &dummy );
			bool bLoadedOnServer = !(data && dummy && data[0] == 0);
			Msg( "%3i: %c %s\n", i, bLoadedOnServer ? '*' : ' ', GetBaseLocalClient().m_pDynamicModelTable->GetString(i) );
		}
	}
#endif

	extern IVModelInfo *modelinfo;
	extern IVModelInfoClient *modelinfoclient;
	Msg( "\ndynamic models:\n" );
	for ( UtlHashHandle_t h = m_DynamicModels.FirstHandle(); h != m_DynamicModels.InvalidHandle(); h = m_DynamicModels.NextHandle(h) )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[h];
		int idx = modelinfo->GetModelIndex( m_DynamicModels.Key(h)->szPathName );
#ifndef DEDICATED
		if ( idx == -1 ) idx = modelinfoclient->GetModelIndex( m_DynamicModels.Key(h)->szPathName );
#endif
		Msg( "%d (%d%c): %s [ref: %d (%dc)] %s%s%s%s%s\n", idx, ((-2 - idx) >> 1), (idx & 1) ? 'c' : 's',
			m_DynamicModels.Key(h)->szPathName, dyn.m_iRefCount, dyn.m_iClientRefCount,
			(dyn.m_nLoadFlags & CDynamicModelInfo::QUEUED) ? " QUEUED" : "",
			(dyn.m_nLoadFlags & CDynamicModelInfo::LOADING) ? " LOADING" : "",
			(dyn.m_nLoadFlags & CDynamicModelInfo::LOCKED) ? " LOCKED" : "",
			(dyn.m_nLoadFlags & CDynamicModelInfo::READY) ? " READY" : "" ,
			(dyn.m_nLoadFlags & CDynamicModelInfo::COMBINED) ? " COMBINED" : "" );
	}
}

#if !defined ( _CERT )
CON_COMMAND( mod_combiner_info, "debug spew for Combiner Info" )
{
	((CModelLoader*)modelloader)->DebugCombinerInfo();
}
#endif


void CModelLoader::DebugCombinerInfo()
{
	extern IVModelInfo			*modelinfo;
	extern IVModelInfoClient	*modelinfoclient;

	Msg( "Dynamic Combined Models:\n" );
	for ( UtlHashHandle_t h = m_DynamicModels.FirstHandle(); h != m_DynamicModels.InvalidHandle(); h = m_DynamicModels.NextHandle(h) )
	{
		CDynamicModelInfo &dyn = m_DynamicModels[h];
		if ( ( dyn.m_nLoadFlags & CDynamicModelInfo::COMBINED ) == 0 )
		{
			continue;
		}

		int idx = modelinfo->GetModelIndex( m_DynamicModels.Key( h )->szPathName );
#ifndef DEDICATED
		if ( idx == -1 ) idx = modelinfoclient->GetModelIndex( m_DynamicModels.Key( h )->szPathName );
#endif
		Msg( "%d ( %d : %s ): %s [ reference count: %d / %d client ] %s%s%s%s%s\n", idx, ((-2 - idx) >> 1), (idx & 1) ? "client" : "server",
			m_DynamicModels.Key(h)->szPathName, dyn.m_iRefCount, dyn.m_iClientRefCount,
			(dyn.m_nLoadFlags & CDynamicModelInfo::QUEUED) ? " QUEUED" : "",
			(dyn.m_nLoadFlags & CDynamicModelInfo::LOADING) ? " LOADING" : "",
			(dyn.m_nLoadFlags & CDynamicModelInfo::LOCKED) ? " LOCKED" : "",
			(dyn.m_nLoadFlags & CDynamicModelInfo::READY) ? " READY" : "" ,
			(dyn.m_nLoadFlags & CDynamicModelInfo::COMBINED) ? " COMBINED" : "" );
	}

	Msg( "\n" );

	g_pMDLCache->DebugCombinerInfo();
}
