//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// model loading and caching
//
//===========================================================================//

#ifndef _PS3
#include <memory.h>
#endif

#include "tier0/vprof.h"
#include "tier0/icommandline.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlmap.h"
#include "datacache/imdlcache.h"
#include "istudiorender.h"
#include "filesystem.h"
#include "optimize.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/imesh.h"
#include "datacache/idatacache.h"
#include "studio.h"
#include "vcollide.h"
#include "utldict.h"
#include "convar.h"
#include "datacache_common.h"
#include "mempool.h"
#include "vphysics_interface.h"
#include "phyfile.h"
#include "studiobyteswap.h"
#include "tier2/fileutils.h"
#include "filesystem/IQueuedLoader.h"
#include "tier1/lzmaDecoder.h"
#include "datacache/iresourceaccesscontrol.h"
#include "tier0/miniprofiler.h"
#include <algorithm>
#include "mdlcombine.h"
#include "vtfcombine.h"
#include "keyvalues.h"

#ifdef _CERT
#define NO_LOG_MDLCACHE 1
#endif

//#define DEBUG_ANIM_STALLS

#ifdef NO_LOG_MDLCACHE
#define LogMdlCache() 0
#else
#define LogMdlCache() mod_trace_load.GetBool()
#endif

#define MdlCacheMsg		if ( !LogMdlCache() ) ; else Msg
#define MdlCacheWarning if ( !LogMdlCache() ) ; else Warning

#if defined( _X360 )
#define AsyncMdlCache() 0	// Explicitly !!!OFF!!! for 360 (incompatible), specific compatible resources opt in individually.
#else
#define AsyncMdlCache() 0
#endif

#define ERROR_MODEL		"models/error.mdl"
#define IDSTUDIOHEADER	(('T'<<24)+('S'<<16)+('D'<<8)+'I')

#define MakeCacheID( handle, type )	( ( (uint)(handle) << 16 ) | (uint)(type) )
#define HandleFromCacheID( id)		( (MDLHandle_t)((id) >> 16) )
#define TypeFromCacheID( id )		( (MDLCacheDataType_t)((id) & 0xffff) )

enum
{
	STUDIODATA_FLAGS_STUDIOMESH_LOADED	= 0x0001,
	STUDIODATA_FLAGS_VCOLLISION_LOADED	= 0x0002,
	STUDIODATA_ERROR_MODEL				= 0x0004,
	STUDIODATA_FLAGS_NO_STUDIOMESH		= 0x0008,
	STUDIODATA_FLAGS_NO_VERTEX_DATA		= 0x0010,
//										= 0x0020,	// unused
	STUDIODATA_FLAGS_PHYSICS2COLLISION_LOADED = 0x0040,
	STUDIODATA_FLAGS_VCOLLISION_SCANNED	= 0x0080,

	STUDIODATA_FLAGS_COMBINED_PLACEHOLDER	= 0x0100,
	STUDIODATA_FLAGS_COMBINED				= 0x0200,
	STUDIODATA_FLAGS_COMBINED_UNAVAILABLE	= 0x0400,
	STUDIODATA_FLAGS_COMBINED_ASSET			= 0x0800,
};

static IPhysicsSurfaceProps *physprops = NULL;

class CStudioVCollide : public CRefCounted<>
{
public:
	~CStudioVCollide()
	{
		g_pPhysicsCollision->VCollideUnload( &m_vcollide );
	}
	vcollide_t *GetVCollide()
	{
		return &m_vcollide;
	}
private:
	vcollide_t	m_vcollide;
};


// #define DEBUG_COMBINER	1


enum
{
	COMBINED_REFERENCE_PLACEHOLDER	= 0x00000001,
	COMBINED_REFERENCE_PRIMARY		= 0x00000002,
	COMBINED_REFERENCE_COMBINER		= 0x00000004,
};


// only models with type "mod_studio" have this data
struct studiodata_t
{
	// The .mdl file
	DataCacheHandle_t	m_MDLCache;
	// Reference count
	unsigned short		m_nRefCount;
	// User data associated with handle
	void				*m_pUserData;
	// the VPhysics collision model
	CStudioVCollide		*m_pVCollide;
	// Hardware & LOD data
	studiohwdata_t		m_HardwareData;
	// STUDIODATA_FLAGS_STUDIOMESH_LOADED, etc. from above
	unsigned short		m_nFlags;
	// Pointer to the virtual version of the model
	virtualmodel_t		*m_pVirtualModel;
	// Array of handles to animation blocks
	CUtlVector< DataCacheHandle_t > m_vecAnimBlocks;
	CUtlVector< unsigned long > m_vecFakeAnimBlockStall;
#ifdef DEBUG_ANIM_STALLS
	CUtlVector< unsigned long > m_vecFirstRequest;
#endif

	// vertex data is usually compressed to save memory (model decal code only needs some data)
	DataCacheHandle_t	m_VertexCache;

	CUtlVector< unsigned short > m_vecAutoplaySequenceList;

	studiohdr_t			*m_pForceLockedStudioHdr;			// only non-null if mod_lock_mdls_on_load is set
	vertexFileHeader_t	*m_pForceLockedVertexFileHeader;	// only non-null if mod_lock_meshs_on_load is set and not async loading
	CInterlockedInt		m_iStudioHdrVirtualLock;			// keeps count while mdlcache lock is held, lock counts fixed up for transition
	CThreadFastMutex	m_ForceLockMutex;

	MDLHandle_t			m_Handle;

	TCombinedStudioData	*m_pCombinedStudioData;

	DECLARE_FIXEDSIZE_ALLOCATOR_MT( studiodata_t );
};

DEFINE_FIXEDSIZE_ALLOCATOR_MT( studiodata_t, 128, CUtlMemoryPool::GROW_SLOW );

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MODEL_SUBSTITUTION_FILENAME "cfg/model_substitution.txt"

static const char *s_ModelSwapperExtensions[] = 
{
	".mdl",
	".dx90.vtx",
	".vvd",
	".ani",
};

//
// Class to swap out models based on GPU level (PC-only)
//
class CModelSwapper
{
public:
	CModelSwapper() :
		// Hash with 256 buckets
		m_ModelLookup( 256, 0, 0, ModelSubstitution_t::AreEqual, ModelSubstitution_t::Hash ),
		m_nMaxExtensionLength( 0 ),
		m_nGPULevel( -1 )
	{	
		
		for ( int i = 0; i < ARRAYSIZE( s_ModelSwapperExtensions ); ++ i )
		{
			int nLen = Q_strlen( s_ModelSwapperExtensions[ i ] );
			if ( m_nMaxExtensionLength < nLen )
			{
				m_nMaxExtensionLength = nLen;
			}
		}
	}
	
	~CModelSwapper()
	{
		Cleanup();
	}

	void LoadSubstitutionFile( const char *pSubstitutionDefinitionFile )
	{
		Cleanup();

		KeyValues *pKV = new KeyValues( "ModelSubstitution" );
		if ( pKV->LoadFromFile( g_pFullFileSystem, pSubstitutionDefinitionFile ) )
		{
			for ( KeyValues *pSubKV = pKV->GetFirstSubKey(); pSubKV != NULL; pSubKV = pSubKV->GetNextKey() )
			{
				if ( Q_stricmp( pSubKV->GetName(), "sub" ) == 0 )
				{
					// max GPU level for which this substitution will be performed (default: 1)
					int nMaxGPULevel = pSubKV->GetInt( "maxgpulevel", 1 );
					const char *pOriginalModelName = pSubKV->GetString( "original", "" );
					const char *pSubstituteModelName = pSubKV->GetString( "substitute", "" );

					int nOriginalModelNameLength = Q_strlen( pOriginalModelName );
					int nSubstituteModelNameLength = Q_strlen( pSubstituteModelName );

					if ( nOriginalModelNameLength >= ( MAX_PATH - m_nMaxExtensionLength ) || 
						nSubstituteModelNameLength >= ( MAX_PATH - m_nMaxExtensionLength ) )
					{
						Warning( "PERF WARNING: error parsing " MODEL_SUBSTITUTION_FILENAME "\n" );
						continue;
					}
					
					// Create substitution entries for .mdl, .ani, .vvd, .dx90.vtx
					ModelSubstitution_t modelSubstitution;
					modelSubstitution.nMaxGPULevel = nMaxGPULevel;

					for ( int i = 0; i < ARRAYSIZE( s_ModelSwapperExtensions ); ++ i )
					{
						char *pName;
						
						pName = new char[ MAX_PATH ];
						modelSubstitution.pOriginalModelName = pName;
						Q_strncpy( pName, pOriginalModelName, MAX_PATH );
						Q_strncat( pName, s_ModelSwapperExtensions[ i ], MAX_PATH );
						V_FixSlashes( pName );
						m_Strings.AddToTail( modelSubstitution.pOriginalModelName );

						pName = new char[ MAX_PATH ];
						modelSubstitution.pSubstituteModelName = pName;
						Q_strncpy( pName, pSubstituteModelName, MAX_PATH );
						Q_strncat( pName, s_ModelSwapperExtensions[ i ], MAX_PATH );
						V_FixSlashes( pName );
						m_Strings.AddToTail( modelSubstitution.pSubstituteModelName );

						m_ModelLookup.Insert( modelSubstitution );
					}
				}
			}			
		}
		else
		{
			Warning( "PERF WARNING: Failed to open model substitution file, cannot swap models out based on gpu_level!\n" );
		}

		pKV->deleteThis();
	}
	
	void Cleanup()
	{
		for ( int i = 0; i < m_Strings.Count(); ++ i )
		{
			delete[] m_Strings[i];
		}
		m_Strings.RemoveAll();
		m_ModelLookup.RemoveAll();		
	}

	const char * TranslateModelName( const char *pOriginalModelName )
	{
		ModelSubstitution_t searchData;
		searchData.pOriginalModelName = pOriginalModelName;

		UtlHashHandle_t handle = m_ModelLookup.Find( searchData );
		if ( handle != m_ModelLookup.InvalidHandle() && GetEffectiveGPULevel() <= m_ModelLookup[ handle ].nMaxGPULevel )
		{
			DevMsg( "Substituting model %s for %s because gpu_level is %d\n", m_ModelLookup[ handle ].pSubstituteModelName, pOriginalModelName, GetEffectiveGPULevel() );
			return m_ModelLookup[ handle ].pSubstituteModelName;
		}
		return pOriginalModelName;		
	}
	
	// We explicitly set and get the cached GPU level because
	// if it changes mid-level, bad things can happen to the model swapping logic.
	// Specifically, we'll try and re-load mesh data from the wrong models and
	// likely crash and/or corrupt memory.
	void LatchEffectiveGPULevel()
	{
		static ConVarRef gpu_level( "gpu_level" );
		m_nGPULevel = gpu_level.GetInt();
	}

	int GetEffectiveGPULevel()
	{
		// Latch on first request if this has not yet been initialized
		if ( m_nGPULevel == -1 )
		{
			LatchEffectiveGPULevel();
		}

		return m_nGPULevel;
	}

private:
	
	struct ModelSubstitution_t
	{
		const char *pOriginalModelName;
		const char *pSubstituteModelName;
		int nMaxGPULevel;

		static bool AreEqual( const ModelSubstitution_t &lhs, const ModelSubstitution_t &rhs )
		{
			return Q_stricmp( lhs.pOriginalModelName, rhs.pOriginalModelName ) == 0;
		}

		static unsigned int Hash( const ModelSubstitution_t &value )
		{
			return HashStringCaseless( value.pOriginalModelName );
		}
	};

	CUtlVector< const char * > m_Strings;
	CUtlHash< ModelSubstitution_t > m_ModelLookup;
	int m_nMaxExtensionLength;
	int m_nGPULevel;
};

//-----------------------------------------------------------------------------
// AnimBlock allocator - Provides a fixed block pooling strategy for anim blocks
// to ease fragmentation due to streaming
//-----------------------------------------------------------------------------

#if defined( CSTRIKE15 )

// CS:GO currently loads 518 anim blocks (most of which are in the 24-32K size range), I've made this 530 to allow for some additional animations that are coming soon.
#define MAX_ANIMBLOCKS 530
#define ANIMBLOCK_SIZE 33*1024 // this is set to 33K for now, because one animation is over 32K by a smidge and spews a message and ends up allocating outside the pool, reduce back to 32K when that anim is fixed

#else

// Portal 2 has different requirements. There are a lot of big animations seeking, which creates some issues in term of DVD latencies.
// On the other hand, there are not a lot of different animations. So we use the 9 MB buffer differently.
#define MAX_ANIMBLOCKS 137
#define ANIMBLOCK_SIZE 64*1024

#endif

CFixedBudgetMemoryPool<ANIMBLOCK_SIZE, MAX_ANIMBLOCKS> g_AnimBlockAllocator;

void FreeAnimBlock( void *p )
{
	// anim blocks can be allocated from different providers
	if ( g_AnimBlockAllocator.Owns( p ) )
	{
		g_AnimBlockAllocator.Free( p );
	}
	else
	{
		g_pFullFileSystem->FreeOptimalReadBuffer( p );
	}
}


//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
static ConVar r_rootlod( "r_rootlod", "0" );
static ConVar mod_forcedata( "mod_forcedata", ( AsyncMdlCache() ) ? "0" : "1",	0, "Forces all model file data into cache on model load." );
static ConVar mod_test_not_available( "mod_test_not_available", "0" );
static ConVar mod_test_mesh_not_available( "mod_test_mesh_not_available", "0" );
static ConVar mod_test_verts_not_available( "mod_test_verts_not_available", "0" );
static ConVar mod_load_mesh_async( "mod_load_mesh_async", ( AsyncMdlCache() ) ? "1" : "0" );
static ConVar mod_load_anims_async( "mod_load_anims_async", ( IsGameConsole() || AsyncMdlCache() ) ? "1" : "0" );
static ConVar mod_load_vcollide_async( "mod_load_vcollide_async",  ( AsyncMdlCache() ) ? "1" : "0" );
static ConVar mod_trace_load( "mod_trace_load", "0" );
static ConVar mod_lock_mdls_on_load( "mod_lock_mdls_on_load", "1" );
static ConVar mod_lock_meshes_on_load( "mod_lock_meshes_on_load", "1" );
static ConVar mod_load_fakestall( "mod_load_fakestall", "0", 0, "Forces all ANI file loading to stall for specified ms\n");
static ConVar mod_check_vcollide("mod_check_vcollide","0", 0, "Check all vcollides on load");
#ifdef DEBUG_ANIM_STALLS
static ConVar mod_load_showasync( "mod_load_showasync", "0", 0, "Shows the time to load an async animblock\n");
#endif

#ifdef DEDICATED
static ConVar mod_dont_load_vertices("mod_dont_load_vertices", "1", 0, "For the dedicated server, don't load model vertex data" );
#else
static ConVar mod_dont_load_vertices("mod_dont_load_vertices", "0", 0, "For the dedicated server, supress loading model vertex data" );
#endif

//-----------------------------------------------------------------------------
// Utility functions
//-----------------------------------------------------------------------------

static void MakeFilename( char szFileName[MAX_PATH], studiohdr_t *pStudioHdr, const char *pszExtension )
{
	char szBaseModelName[MAX_PATH];

	Q_StripExtension( pStudioHdr->pszName(), szBaseModelName, MAX_PATH );
	Q_snprintf( szFileName, MAX_PATH, "models/%s%s", szBaseModelName, pszExtension );
	Q_FixSlashes( szFileName );
#ifdef POSIX
	Q_strlower( szFileName );
#endif
}

// cache off the surface prop indices for each bone or model prop
static void StudioHdrLookupSurfaceProps( studiohdr_t *pStudioHdrIn )
{
	pStudioHdrIn->surfacepropLookup = physprops->GetSurfaceIndex( pStudioHdrIn->pszSurfaceProp() );
	for ( int i = 0; i < pStudioHdrIn->numbones; i++ )
	{
		mstudiobone_t *pBone = (mstudiobone_t *)pStudioHdrIn->pBone(i);
		pBone->surfacepropLookup = physprops->GetSurfaceIndex( pBone->pszSurfaceProp() );
	}
}

static void StudioHdrSetAnimEventFlag( studiohdr_t *pStudioHdrIn )
{
	for ( int i = 0; i < pStudioHdrIn->numlocalseq; i++ )
	{
		if ( pStudioHdrIn->pLocalSeqdesc(i)->numevents )
			return;
	}
	pStudioHdrIn->flags |= STUDIOHDR_FLAGS_NO_ANIM_EVENTS;
}

//-----------------------------------------------------------------------------
// Async support
//-----------------------------------------------------------------------------

struct AsyncInfo_t
{
	AsyncInfo_t() : hControl( NULL ), hModel( MDLHANDLE_INVALID ), type( MDLCACHE_NONE ), iAnimBlock( 0 ) {}

	FSAsyncControl_t	hControl;
	MDLHandle_t			hModel;
	MDLCacheDataType_t	type;
	int					iAnimBlock;
};

const int NO_ASYNC = CUtlLinkedList< AsyncInfo_t >::InvalidIndex();


//-------------------------------------

CUtlMap<int, intp> g_AsyncInfoMap( DefLessFunc( int ) );
CThreadFastMutex g_AsyncInfoMapMutex;

inline int MakeAsyncInfoKey( MDLHandle_t hModel, MDLCacheDataType_t type, int iAnimBlock )
{
	Assert( type <= 7 && iAnimBlock < 8*1024 );
	return ( ( ( (int)hModel) << 16 ) | ( (int)type << 13 ) | iAnimBlock );
}

inline intp GetAsyncInfoIndex( MDLHandle_t hModel, MDLCacheDataType_t type, int iAnimBlock = 0 )
{
	AUTO_LOCK( g_AsyncInfoMapMutex );
	int key = MakeAsyncInfoKey( hModel, type, iAnimBlock );
	int i = g_AsyncInfoMap.Find( key );
	if ( i == g_AsyncInfoMap.InvalidIndex() )
	{
		return NO_ASYNC;
	}
	return g_AsyncInfoMap[i];
}

inline intp SetAsyncInfoIndex( MDLHandle_t hModel, MDLCacheDataType_t type, int iAnimBlock, intp index )
{
	AUTO_LOCK( g_AsyncInfoMapMutex );
	Assert( index == NO_ASYNC || GetAsyncInfoIndex( hModel, type, iAnimBlock ) == NO_ASYNC );
	int key = MakeAsyncInfoKey( hModel, type, iAnimBlock );
	if ( index == NO_ASYNC )
	{
		g_AsyncInfoMap.Remove( key );
	}
	else
	{
		g_AsyncInfoMap.Insert( key, index );
	}

	return index;
}

inline intp SetAsyncInfoIndex( MDLHandle_t hModel, MDLCacheDataType_t type, intp index )
{
	return SetAsyncInfoIndex( hModel, type, 0, index );
}

//-----------------------------------------------------------------------------
// QUEUED LOADING
// Populates the cache by pushing expected MDL's (and all of their data).
// The Model cache i/o behavior is unchanged during gameplay, ideally the cache
// should yield miss free behaviour.
//-----------------------------------------------------------------------------

struct ModelParts_t
{
	enum BufferType_t
	{
		BUFFER_MDL = 0,
		BUFFER_VTX = 1,
		BUFFER_VVD = 2,
		BUFFER_PHY = 3,
		BUFFER_MAXPARTS,
	};

	ModelParts_t()
	{
		nLoadedParts = 0;
		nExpectedParts = 0;
		hMDL = MDLHANDLE_INVALID;
	}

	// thread safe, only one thread will get a positive result
	bool DoFinalProcessing()
	{
		// indicates that all buffers have arrived
		// when all parts are present, returns true ( guaranteed once ), and marked as completed
		return nLoadedParts.AssignIf( nExpectedParts, nExpectedParts | 0x80000000 );
	}

	CUtlBuffer		Buffers[BUFFER_MAXPARTS];
	MDLHandle_t		hMDL;

	// bit flags
	CInterlockedInt	nLoadedParts;
	int				nExpectedParts;
};

struct AsyncHardwareLoad_t
{
	ModelParts_t	*m_pModelParts;
};

class CMDLCacheData;

//-----------------------------------------------------------------------------
// Implementation of the simple studio data cache (no caching)
//-----------------------------------------------------------------------------
class CMDLCache : public CTier3AppSystem< IMDLCache >, public IStudioDataCache, public CDefaultDataCacheClient
{
	typedef CTier3AppSystem< IMDLCache > BaseClass;

public:
	CMDLCache();

	// Inherited from IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();
	virtual const AppSystemInfo_t* GetDependencies() { return NULL; }
	virtual AppSystemTier_t GetTier() { return APP_SYSTEM_TIER3; }
	virtual void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) { BaseClass::Reconnect( factory, pInterfaceName ); }

	// Inherited from IStudioDataCache
	bool VerifyHeaders( studiohdr_t *pStudioHdr );
	vertexFileHeader_t *CacheVertexData( studiohdr_t *pStudioHdr );

	// Inherited from IMDLCache
	virtual MDLHandle_t FindMDL( const char *pMDLRelativePath );
	virtual int AddRef( MDLHandle_t handle );
	virtual int Release( MDLHandle_t handle );
	virtual int GetRef( MDLHandle_t handle );
	virtual void MarkAsLoaded(MDLHandle_t handle);

	virtual studiohdr_t *GetStudioHdr( MDLHandle_t handle );
	virtual studiohwdata_t *GetHardwareData( MDLHandle_t handle );
	virtual vcollide_t *GetVCollide( MDLHandle_t handle ) { return GetVCollideEx( handle, true); }
	virtual vcollide_t *GetVCollideEx( MDLHandle_t handle, bool synchronousLoad = true );
	virtual unsigned char *GetAnimBlock( MDLHandle_t handle, int nBlock, bool preloadIfMissing );
	virtual bool HasAnimBlockBeenPreloaded( MDLHandle_t handle, int nBlock );
	virtual virtualmodel_t *GetVirtualModel( MDLHandle_t handle );
	virtual virtualmodel_t *GetVirtualModelFast( const studiohdr_t *pStudioHdr, MDLHandle_t handle );
	virtual int GetAutoplayList( MDLHandle_t handle, unsigned short **pOut );
	virtual void TouchAllData( MDLHandle_t handle );
	virtual void SetUserData( MDLHandle_t handle, void* pData );
	virtual void *GetUserData( MDLHandle_t handle );
	virtual bool IsErrorModel( MDLHandle_t handle );
	virtual bool IsOverBudget( MDLHandle_t handle );
	virtual void SetCacheNotify( IMDLCacheNotify *pNotify );
	virtual vertexFileHeader_t *GetVertexData( MDLHandle_t handle );
	virtual void Flush( MDLCacheFlush_t nFlushFlags = MDLCACHE_FLUSH_ALL );
	virtual void Flush( MDLHandle_t handle, int nFlushFlags = MDLCACHE_FLUSH_ALL );
	virtual const char *GetModelName( MDLHandle_t handle );

	IDataCacheSection *GetCacheSection( MDLCacheDataType_t type )
	{
		switch ( type )
		{
		case MDLCACHE_STUDIOHWDATA:
		case MDLCACHE_VERTEXES:
			// meshes and vertexes are isolated to their own section
			return m_pMeshCacheSection;

		case MDLCACHE_ANIMBLOCK:
			// anim blocks have their own section
			return m_pAnimBlocksCacheSection;

		default:
			// everybody else
			return m_pModelCacheSection;
		}
	}

	void *AllocData( MDLCacheDataType_t type, int size );
	void FreeData( MDLCacheDataType_t type, void *pData );
	void CacheData( DataCacheHandle_t *c, void *pData, int size, const char *name, MDLCacheDataType_t type, DataCacheClientID_t id = (DataCacheClientID_t)-1 );
	void *CheckData( DataCacheHandle_t c, MDLCacheDataType_t type );
	void *CheckDataNoTouch( DataCacheHandle_t c, MDLCacheDataType_t type );
	void UncacheData( DataCacheHandle_t c, MDLCacheDataType_t type, bool bLockedOk = false );

	void DisableAsync() { mod_load_mesh_async.SetValue( 0 ); mod_load_anims_async.SetValue( 0 ); }

	virtual void BeginLock();
	virtual void EndLock();
	virtual void BeginCoarseLock();
	virtual void EndCoarseLock();
	virtual int *GetFrameUnlockCounterPtrOLD();
	virtual int *GetFrameUnlockCounterPtr( MDLCacheDataType_t type );

	virtual void FinishPendingLoads();

	// Task switch
	void ReleaseMaterialSystemObjects( int nChangeFlags );
	void RestoreMaterialSystemObjects( int nChangeFlags );
	virtual bool GetVCollideSize( MDLHandle_t handle, int *pVCollideSize );

	virtual void BeginMapLoad();
	virtual void EndMapLoad();

	virtual void InitPreloadData( bool rebuild );
	virtual void ShutdownPreloadData();

	virtual bool IsDataLoaded( MDLHandle_t handle, MDLCacheDataType_t type );

	virtual studiohdr_t *LockStudioHdr( MDLHandle_t handle );
	virtual void UnlockStudioHdr( MDLHandle_t handle );

	virtual void UnloadQueuedHardwareData( );

	virtual bool PreloadModel( MDLHandle_t handle );
	virtual void ResetErrorModelStatus( MDLHandle_t handle );

	virtual void MarkFrame();

	// Queued loading
	void ProcessQueuedData( ModelParts_t *pModelParts );
	static void	QueuedLoaderCallback_MDL( void *pContext, void  *pContext2, const void *pData, int nSize, LoaderError_t loaderError );

	// combined models
	virtual MDLHandle_t	CreateCombinedModel( const char *pszModelName );
	virtual bool		CreateCombinedModel( MDLHandle_t handle );
	virtual bool		SetCombineModels( MDLHandle_t handle, const CUtlVector< SCombinerModelInput_t > &vecModelsToCombine );
	virtual bool		FinishCombinedModel( MDLHandle_t handle, CombinedModelLoadedCallback pFunc, void *pUserData );
	virtual bool		IsCombinedPlaceholder( MDLHandle_t handle );
	virtual bool		IsCombinedModel( MDLHandle_t handle );
	virtual int			GetNumCombinedSubModels( MDLHandle_t handle );
	virtual void		GetCombinedSubModelFilename( MDLHandle_t handle, int nSubModelIndex, char *pszResult, int nResultSize );
	virtual KeyValues	*GetCombinedMaterialKV( MDLHandle_t handle, int nAtlasGroup = 0 );	
	virtual void		UpdateCombiner( );
	virtual void		*GetCombinedInternalAsset( ECombinedAsset AssetType, const char *pszAssetID, int *nSize );
	virtual void		SetCombinerFlags( unsigned nFlags );
	virtual void		ClearCombinerFlags( unsigned nFlags );
	virtual void		DebugCombinerInfo( );

	virtual bool ReleaseAnimBlockAllocator();

	virtual bool RestoreHardwareData( MDLHandle_t handle, FSAsyncControl_t *pAsyncVTXControl, FSAsyncControl_t *pAsyncVVDControl );
	virtual bool ProcessPendingHardwareRestore();

	void OnAsyncHardwareDataComplete( ModelParts_t::BufferType_t bufferType, ModelParts_t *pContext, void *pData, int nNumReadBytes, FSAsyncStatus_t asyncStatus );

	virtual void DumpDictionaryState();

private:
	// Inits, shuts downs studiodata_t
	void InitStudioData( MDLHandle_t handle );
	void ShutdownStudioData( MDLHandle_t handle, bool bImmediate );

	// Returns the *actual* name of the model (could be an error model if the requested model didn't load)
	const char *GetActualModelName( MDLHandle_t handle );

	// Attempts to load a MDL file, validates that it's ok.
	bool ReadMDLFile( MDLHandle_t handle, const char *pMDLFileName, CMDLCacheData &cacheData );

	void FlushImmediate( studiodata_t *pStudioData, MDLCacheFlush_t nFlushFlags = MDLCACHE_FLUSH_ALL );
	void Flush( studiodata_t *pStudioData, MDLCacheFlush_t nFlushFlags = MDLCACHE_FLUSH_ALL );

	// Unserializes the VCollide file associated w/ models (the vphysics representation)
	void UnserializeVCollide( MDLHandle_t handle, bool bUseAsync, bool synchronousLoad );
	
	void LoadPhysics2Collision( MDLHandle_t handle, bool synchronousLoad );

	// Destroys the VCollide associated w/ models
	void DestroyVCollide( studiodata_t *pStudioData );

	// Unserializes the MDL
	studiohdr_t *UnserializeMDL( MDLHandle_t handle, CMDLCacheData &cacheData );

	// Unserializes an animation block from disk
	unsigned char *UnserializeAnimBlock( MDLHandle_t handle, bool bUseAsync, int nBlock );

	// Allocates/frees the anim blocks
	void AllocateAnimBlocks( studiodata_t *pStudioData, int nCount );
	void FreeAnimBlocks( studiodata_t *pStudioData );

	// Allocates/frees the virtual model
	void AllocateVirtualModel( MDLHandle_t handle );
	void FreeVirtualModel( studiodata_t *pStudioData );

	// Purpose: Pulls all submodels/.ani file models into the cache
	void UnserializeAllVirtualModelsAndAnimBlocks( MDLHandle_t handle );

	// Loads/unloads the static meshes
	bool UnserializeHardwareData( MDLHandle_t handle, bool bUseAsync ); // returns false if not ready
	void UnloadHardwareData( studiodata_t *pStudioData );

	// Allocates/frees autoplay sequence list
	void AllocateAutoplaySequences( studiodata_t *pStudioData, int nCount );
	void FreeAutoplaySequences( studiodata_t *pStudioData );

	FSAsyncStatus_t LoadData( const char *pszFilename, const char *pszPathID, bool bAsync, FSAsyncControl_t *pControl, MDLHandle_t hModel ) { return LoadData( pszFilename, pszPathID, NULL, 0, 0, bAsync, pControl, hModel ); }
	FSAsyncStatus_t LoadData( const char *pszFilename, const char *pszPathID, void *pDest, int nBytes, int nOffset, bool bAsync, FSAsyncControl_t *pControl, MDLHandle_t hModel );
	vertexFileHeader_t *LoadVertexData( studiohdr_t *pStudioHdr );
	vertexFileHeader_t *BuildAndCacheVertexData( studiohdr_t *pStudioHdr, CMDLCacheData &cacheData );
	bool BuildHardwareData( MDLHandle_t handle, studiodata_t *pStudioData, studiohdr_t *pStudioHdr, CMDLCacheData &cacheData );
	void ConvertFlexData( studiohdr_t *pStudioHdr );

	int ProcessPendingAsync( intp iAsync );
	void ProcessPendingAsyncs( MDLCacheDataType_t type = MDLCACHE_NONE );
	bool ClearAsync( MDLHandle_t handle, MDLCacheDataType_t type, int iAnimBlock, bool bAbort = false );

	const char *GetVTXExtension();

	virtual bool HandleCacheNotification( const DataCacheNotification_t &notification  );
	virtual bool GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen  );

	virtual bool GetAsyncLoad( MDLCacheDataType_t type );
	virtual bool SetAsyncLoad( MDLCacheDataType_t type, bool bAsync );

	// Creates the 360 file if it doesn't exist or is out of date
	int UpdateOrCreate( studiohdr_t *pHdr, const char *pFilename, char *pX360Filename, int maxLen, const char *pPathID, bool bForce = false );

	// Attempts to read the platform native file - on 360 it can read and swap Win32 file as a fallback
	bool ReadFileNative( char *pFileName, const char *pPath, CUtlBuffer &buf, int nMaxBytes = 0 );

	// Creates a thin cache entry (to be used for model decals) from fat vertex data
	vertexFileHeader_t * CreateThinVertexes( vertexFileHeader_t * originalData, const studiohdr_t * pStudioHdr, int * cacheLength );

	// Creates a null cache entry (showing that vertex data has been loaded, turned into VBs/IBs, and discarded)
	vertexFileHeader_t * CreateNullVertexes( vertexFileHeader_t * originalData, const studiohdr_t * pStudioHdr, int * cacheLength );

	// Processes raw data (from an I/O source) into the cache. Sets the cache state as expected for bad data.
	bool ProcessDataIntoCache( MDLHandle_t handle, CMDLCacheData &cacheData, int iAnimBlock = 0 );

	void BreakFrameLock( bool bModels = true, bool bMesh = true, bool bAnimBlock = true );
	void RestoreFrameLock();

	void ReloadVCollide( MDLHandle_t handle );

	virtual void DisableVCollideLoad( void ) {m_bDisableVCollideLoad = true;}
	virtual void EnableVCollideLoad( void )  {m_bDisableVCollideLoad = false;}

	virtual void DisableFileNotFoundWarnings( void ) {m_bFileNotFoundAllowed = true;}
	virtual void EnableFileNotFoundWarnings( void ) {m_bFileNotFoundAllowed = false;}

	// combined models
	TCombinedStudioData	*GetCombinedData( MDLHandle_t handle );
	void				CheckCombinerFlagChanges( int nNewFlags );
	void				InitCombiner( );
	void				ShutdownCombiner( );
	void				CombinerThread( );
	static uintp		StaticCombinerThread( void *pParam );
	bool				UnserializeCombinedHardwareData( MDLHandle_t handle );
	void				FreeCombinedGeneratedData( studiodata_t *pStudioData );

private:
	IDataCacheSection *m_pModelCacheSection;
	IDataCacheSection *m_pMeshCacheSection;
	IDataCacheSection *m_pAnimBlocksCacheSection;

	int m_nModelCacheFrameLocks;
	int m_nMeshCacheFrameLocks;
	int m_nAnimBlockCacheFrameLocks;

	CUtlDict< studiodata_t*, MDLHandle_t > m_MDLDict;

	IMDLCacheNotify *m_pCacheNotify;

	CUtlFixedLinkedList< AsyncInfo_t > m_PendingAsyncs;

	CThreadFastMutex m_QueuedLoadingMutex;
	CThreadFastMutex m_AsyncMutex;

	CTSQueue< studiodata_t * > m_UnloadHandles;

	// combined functionality
	CTSQueue< TCombinedStudioData * >		m_CombinerToBeCombined;
	bool									m_bCombinerReady;
	unsigned								m_nCombinerFlags;
	volatile bool							m_bCombinerShutdown;
	ThreadHandle_t							m_hCombinerThread;
	CThreadEvent							m_CombinerEvent;
	CThreadEvent							m_CombinerShutdownEvent;
	CInterlockedPtr< TCombinedStudioData >	m_pToBeCombined;
	CInterlockedPtr< TCombinedStudioData >	m_pCombinedCompleted;

	bool m_bLostVideoMemory : 1;
	bool m_bConnected : 1;
	bool m_bInitialized : 1;
	bool m_bDisableVCollideLoad : 1;
	bool m_bFileNotFoundAllowed : 1;

	CModelSwapper m_ModelSwapper;

	CTSQueue< AsyncHardwareLoad_t >	m_QueuedAsyncHardwareLoads;

	friend class CMDLCacheData; // Needs to access ReadFileNative
};

//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
static CMDLCache g_MDLCache;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMDLCache, IMDLCache, MDLCACHE_INTERFACE_VERSION, g_MDLCache );
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMDLCache, IStudioDataCache, STUDIO_DATA_CACHE_INTERFACE_VERSION, g_MDLCache );


//-----------------------------------------------------------------------------
// Task switch
//-----------------------------------------------------------------------------
static void ReleaseMaterialSystemObjects( int nChangeFlags )
{
	g_MDLCache.ReleaseMaterialSystemObjects( nChangeFlags );
}

static void RestoreMaterialSystemObjects( int nChangeFlags )
{
	g_MDLCache.RestoreMaterialSystemObjects( nChangeFlags );
}

static void CleanupMaterialSystemObjects( )
{
//	g_MDLCache.UpdateCombiner();
	g_MDLCache.UnloadQueuedHardwareData();
}


//-----------------------------------------------------------------------------
// CMDLCacheData manages data being processed into the cache:
//  - it handles LZMA decompression (minimizing concurrent memory allocations)
//  - it makes sure all original+intermediate data get allocated/freed in the appropriate fashion
//-----------------------------------------------------------------------------
class CMDLCacheData
{
public:

	enum AllocType_t // Specifies how 
	{
		ALLOC_MALLOC			= 0,	// The input data (and decompressed data) are allocated via malloc
		ALLOC_OPTIMALREADBUFFER	= 1,	// The input buffer uses external memory allocated via g_pFullFileSystem->AllocOptimalReadBuffer
		ALLOC_ANIMBLOCK			= 2,	// The input buffer uses external memory allocated via g_AnimBlockAllocator.Alloc()
	};

	// NOTE: On construction, the given CUtlBuffer has its memory detached, so the CMDLCacheData now owns it.
	CMDLCacheData( MDLCacheDataType_t dataType, AllocType_t allocType, CUtlBuffer *pDataBuffer = NULL )
	 :	m_DataType( dataType ), m_AllocType( allocType ), m_pData( NULL ), m_nDataSize( 0 )
	{
		Assert( ( m_AllocType != ALLOC_MALLOC ) == ( pDataBuffer ? pDataBuffer->IsExternallyAllocated() : false ) );
		if ( pDataBuffer && pDataBuffer->TellMaxPut() )
		{
			m_pData     = pDataBuffer->Base();
			m_nDataSize = pDataBuffer->TellMaxPut();
			if ( pDataBuffer->IsExternallyAllocated() )
				pDataBuffer->SetExternalBuffer( NULL, 0, 0 );
			else
				pDataBuffer->Detach();

			Decompress();
		}
	}

	~CMDLCacheData() { Purge(); }

	// Get the data (if the data is invalid or absent, this will return NULL)
	void *Data( void ) { return m_pData; }

	// Get the size of the data (if the data is invalid, this will return zero)
	int DataSize( void ) { return m_nDataSize; }

	MDLCacheDataType_t DataType( void ) { return m_DataType; }

	// The caller may ask to discard the data (i.e. don't use it)
	void Purge( void )
	{
		if ( m_pData )
		{
			Deallocate( m_pData );
			m_pData     = NULL;
			m_nDataSize = 0;
		}
	}

	// Transfer ownership of the memory to the caller
	void *Detach( void )
	{
		if ( !m_pData )
		{
			// Paranoid usage check
			Warning( "ERROR: CMDLCacheData::Detach used incorrectly (there is no data to return!)\n" );
			Assert( 0 );
			return NULL;
		}

		void *pResult = m_pData;
		m_pData       = NULL;
		m_nDataSize   = 0;
		return pResult;
	}

	// Read a file into the CMDLCacheData's internal buffer (replaces any existing data)
	bool ReadFileNative( char *pFileName, const char *pPath )
	{
		// Clear out any existing data
		Purge();

		// Read a file into memory
		bool bSuccess = false;
		if ( m_AllocType == ALLOC_MALLOC )
		{
			CUtlBuffer buf;
			bSuccess = g_MDLCache.ReadFileNative( pFileName, pPath, buf );
			if ( bSuccess )
			{
				if ( m_DataType == MDLCACHE_STUDIOHDR ) 
				{
					studiohdr_t* pStudioHdr = ( studiohdr_t* ) buf.Base();
					if ( pStudioHdr->studiohdr2index == 0 )
					{
						// We always need this now, so make room for it in the buffer now.
						int bufferContentsEnd = buf.TellMaxPut();
						int maskBits = VALIGNOF( studiohdr2_t ) - 1;
						int offsetStudiohdr2 = ( bufferContentsEnd + maskBits ) & ~maskBits;
						int sizeIncrease = ( offsetStudiohdr2 - bufferContentsEnd )  + sizeof( studiohdr2_t );
						buf.SeekPut( CUtlBuffer::SEEK_CURRENT, sizeIncrease );

						// Re-get the pointer after resizing, because it has probably moved.
						pStudioHdr = ( studiohdr_t* ) buf.Base();
						studiohdr2_t* pStudioHdr2 = ( studiohdr2_t* ) ( ( byte * ) pStudioHdr + offsetStudiohdr2 );
						memset( pStudioHdr2, 0, sizeof( studiohdr2_t ) );
						pStudioHdr2->flMaxEyeDeflection = 0.866f; // Matches studio.h.

						pStudioHdr->studiohdr2index = offsetStudiohdr2;
						// Also make sure the structure knows about the extra bytes 
						// we've added so they get copied around.
						pStudioHdr->length += sizeIncrease;
					}
				}

				m_nDataSize = buf.TellMaxPut();
				m_pData     = buf.Detach();
				Decompress();
			}
		}
		else
		{
			// We don't support this pattern (if we need to, we could pass an alloc callback to g_pFullFileSystem->ReadFile)
			Warning( "ERROR: CMDLCacheData::ReadFileNative is only supported when using ALLOC_MALLOC\n" );
			Assert( 0 );
		}

		return bSuccess;
	}

private:

	void *Allocate( int nDataSize )
	{
		switch( m_AllocType )
		{
		case ALLOC_MALLOC:
			return malloc( nDataSize );
		case ALLOC_OPTIMALREADBUFFER:
			return g_pFullFileSystem->AllocOptimalReadBuffer( FILESYSTEM_INVALID_HANDLE, nDataSize );
		case ALLOC_ANIMBLOCK:
			if ( nDataSize <= ANIMBLOCK_SIZE )
			{
				return g_AnimBlockAllocator.Alloc();
			}
			else
			{
				Warning( "%s(%d): MDL Cache allocation outside the pool. Size allocated: %d.\n", __FILE__, __LINE__, nDataSize );

				// If an animblock was compressed, its decompressed size could exceed ANIMBLOCK_SIZE,
				// so deal with that the same manner as CMDLCache::UnserializeAnimBlock():
				m_AllocType = ALLOC_OPTIMALREADBUFFER;
				return Allocate( nDataSize );
			}
		}
		Assert(0);
		return NULL;
	}

	void Deallocate( void *pData )
	{
		switch( m_AllocType )
		{
		case ALLOC_MALLOC:
			return free( pData );
		case ALLOC_OPTIMALREADBUFFER:
			return g_pFullFileSystem->FreeOptimalReadBuffer( pData );
		case ALLOC_ANIMBLOCK:
			return FreeAnimBlock( pData ); // NOTE: this handles large animblocks allocated by the caller via AllocOptimalReadBuffer
		}
		Assert(0);
	}

	bool Decompress( void )
	{
		CLZMA lzma;

		// Trivial early-outs - make sure we have valid data, and are on a game console (no LZMA on PC):
		if ( !IsGameConsole() || !m_pData )
			return true;

		// Some asset types have an uncompressed header before the compressed data starts:
		int nHeaderSize = 0;
		if ( m_DataType == MDLCACHE_VERTEXES )
			nHeaderSize = sizeof( vertexFileHeader_t );
		if ( m_DataType == MDLCACHE_STUDIOHWDATA )
			nHeaderSize = sizeof( OptimizedModel::FileHeader_t );

		// Check that the data is actually compressed (only happens for game consoles)
		if ( !lzma.IsCompressed( nHeaderSize + (unsigned char *)m_pData ) )
			return true;

		// Allocate a new buffer for the uncompressed data
		unsigned int nUncompressedSize = lzma.GetActualSize( nHeaderSize + (unsigned char *)m_pData );
		void *       pUncompressedData = Allocate( nUncompressedSize + nHeaderSize );
		// Copy the uncompressed header verbatim
		memcpy( pUncompressedData, m_pData, nHeaderSize );
		// Decompress the rest
		if ( lzma.Uncompress( nHeaderSize + (unsigned char *)m_pData, nHeaderSize + (unsigned char *)pUncompressedData ) != nUncompressedSize )
		{
			// Decompression failed!
			Msg( "ERROR: LZMA decompression failed - corrupt data!!\n" );
			Deallocate( pUncompressedData );
			Purge();
			return false;
		}

		// Free the original memory (to minimize concurrent memory allocations - important on game consoles!)
		Deallocate( m_pData );

		// Expose the decompressed data to the user from now on
		m_pData     = pUncompressedData;
		m_nDataSize = nUncompressedSize + nHeaderSize;

		return true;
	}

	void				*m_pData;				// The original source data, replaced with decompressed data by 'Decompress'
	int					m_nDataSize;			// Size of the data, updated by 'Decompress'
	MDLCacheDataType_t	m_DataType;				// The type of the data (determines how memory is decompressed)
	AllocType_t			m_AllocType;			// Determines how data is [de]allocated.
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMDLCache::CMDLCache()
{
	m_bLostVideoMemory = false;
	m_bConnected = false;
	m_bInitialized = false;
	m_bDisableVCollideLoad = false;
	m_bFileNotFoundAllowed = false;
	m_pCacheNotify = NULL;
	m_pModelCacheSection = NULL;
	m_pMeshCacheSection = NULL;
	m_pAnimBlocksCacheSection = NULL;
	m_nModelCacheFrameLocks = 0;
	m_nMeshCacheFrameLocks = 0;
	m_nAnimBlockCacheFrameLocks = 0;
	m_bCombinerReady = false;
	m_nCombinerFlags = COMBINER_FLAG_THREADING;
	m_bCombinerShutdown = false;
	m_hCombinerThread = NULL;
	m_pToBeCombined = NULL;
	m_pCombinedCompleted = NULL;
	m_CombinerEvent.Reset();
	m_CombinerShutdownEvent.Reset();
}


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CMDLCache::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	// Connect can be called twice, because this inherits from 2 appsystems.
	if ( m_bConnected )
		return true;

	physprops = (IPhysicsSurfaceProps *)factory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL  );
	//if ( !physprops )
	//	return false;

	//if ( !g_pMaterialSystemHardwareConfig || !g_pPhysicsCollision || !g_pStudioRender || !g_pMaterialSystem )
	//	return false;

	m_bConnected = true;
	if( g_pMaterialSystem )
	{
		g_pMaterialSystem->AddReleaseFunc( ::ReleaseMaterialSystemObjects );
		g_pMaterialSystem->AddRestoreFunc( ::RestoreMaterialSystemObjects );
#ifdef PLATFORM_WINDOWS_PC
		g_pMaterialSystem->AddEndFrameCleanupFunc( ::CleanupMaterialSystemObjects );
#endif
	}

	return true;
}

void CMDLCache::Disconnect()
{
	if ( g_pMaterialSystem && m_bConnected )
	{
		g_pMaterialSystem->RemoveReleaseFunc( ::ReleaseMaterialSystemObjects );
		g_pMaterialSystem->RemoveRestoreFunc( ::RestoreMaterialSystemObjects );
#ifdef PLATFORM_WINDOWS_PC
		g_pMaterialSystem->RemoveEndFrameCleanupFunc( ::CleanupMaterialSystemObjects );
#endif

		ShutdownCombiner();

		m_bConnected = false;
	}

	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Query Interface
//-----------------------------------------------------------------------------
void *CMDLCache::QueryInterface( const char *pInterfaceName )
{
	if (!Q_strncmp(	pInterfaceName, STUDIO_DATA_CACHE_INTERFACE_VERSION, Q_strlen(STUDIO_DATA_CACHE_INTERFACE_VERSION) + 1))
		return (IStudioDataCache*)this;

	if (!Q_strncmp(	pInterfaceName, MDLCACHE_INTERFACE_VERSION, Q_strlen(MDLCACHE_INTERFACE_VERSION) + 1))
		return (IMDLCache*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Init/Shutdown
//-----------------------------------------------------------------------------

#define MODEL_CACHE_MODEL_SECTION_NAME		"ModelData"
#define MODEL_CACHE_MESH_SECTION_NAME		"ModelMesh"
#define MODEL_CACHE_ANIMBLOCK_SECTION_NAME	"AnimBlock"

// #define ENABLE_CACHE_WATCH 1

#if defined( ENABLE_CACHE_WATCH )
static ConVar cache_watch( "cache_watch", "", 0 );

static void CacheLog( const char *fileName, const char *accessType )
{
	if ( Q_stristr( fileName, cache_watch.GetString() ) )
	{
		Msg( "%s access to %s\n", accessType, fileName );
	}
}
#endif

InitReturnVal_t CMDLCache::Init()
{
	// Can be called twice since it inherits from 2 appsystems
	if ( m_bInitialized )
		return INIT_OK;

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	if ( !m_pModelCacheSection )
	{
		m_pModelCacheSection = g_pDataCache->AddSection( this, MODEL_CACHE_MODEL_SECTION_NAME );
	}

	if ( !m_pMeshCacheSection )
	{
		unsigned int meshLimit = (unsigned)-1;
		DataCacheLimits_t limits( meshLimit, (unsigned)-1, 0, 0 );
		m_pMeshCacheSection = g_pDataCache->AddSection( this, MODEL_CACHE_MESH_SECTION_NAME, limits );
		
		// model meshes do not participate in LRU pruge due to -1 max bytes limit
		// not allowing console mem_force_flush to unexpectedly flush, which would otherwise destabilize model meshes
		m_pMeshCacheSection->SetOptions( m_pMeshCacheSection->GetOptions() | DC_NO_USER_FORCE_FLUSH );
	}

	if ( !m_pAnimBlocksCacheSection )
	{
		unsigned int animBlockLimit = (unsigned)-1;
		if ( IsGameConsole() )
		{
			// consoles limit the anim cache, tuned to worst case
			// Use the amount of memory allocated by g_AnimBlockAllocator
			animBlockLimit = ANIMBLOCK_SIZE*MAX_ANIMBLOCKS;
		}
		DataCacheLimits_t limits( animBlockLimit, (unsigned)-1, 0, 0 );
		m_pAnimBlocksCacheSection = g_pDataCache->AddSection( this, MODEL_CACHE_ANIMBLOCK_SECTION_NAME, limits );
	}

	if ( IsGameConsole() )
	{
		// By default, source data is assumed to be non-native to the 360.
		StudioByteSwap::ActivateByteSwapping( true );
		StudioByteSwap::SetCollisionInterface( g_pPhysicsCollision );
	}
	m_bLostVideoMemory = false;
	m_bInitialized = true;

#if defined( ENABLE_CACHE_WATCH )
	g_pFullFileSystem->AddLoggingFunc( &CacheLog );
#endif

	if ( IsPC() )
	{
		//UNDONE: This opens up a whole fun realm of cheating for multiplayer games!
		//m_ModelSwapper.LoadSubstitutionFile( MODEL_SUBSTITUTION_FILENAME );
	}

	return INIT_OK;
}

void CMDLCache::Shutdown()
{
	if ( !m_bInitialized )
		return;
#if defined( ENABLE_CACHE_WATCH )
	g_pFullFileSystem->RemoveLoggingFunc( CacheLog );
#endif
	m_bInitialized = false;

	if ( m_pModelCacheSection || m_pMeshCacheSection )
	{
		// Free all MDLs that haven't been cleaned up
		MDLHandle_t i = m_MDLDict.First();
		while ( i != m_MDLDict.InvalidIndex() )
		{
			ShutdownStudioData( i, true );
			i = m_MDLDict.Next( i );
		}

		m_MDLDict.Purge();

		if ( m_pModelCacheSection )
		{
			g_pDataCache->RemoveSection( MODEL_CACHE_MODEL_SECTION_NAME );
			m_pModelCacheSection = NULL;
		}
		if ( m_pMeshCacheSection )
		{
			g_pDataCache->RemoveSection( MODEL_CACHE_MESH_SECTION_NAME );
			m_pMeshCacheSection = NULL;
		}
	}

	if ( m_pAnimBlocksCacheSection )
	{
		g_pDataCache->RemoveSection( MODEL_CACHE_ANIMBLOCK_SECTION_NAME );
		m_pAnimBlocksCacheSection = NULL;
	}

	BaseClass::Shutdown();
}

void CMDLCache::FlushImmediate( studiodata_t *pStudioData, MDLCacheFlush_t nFlushFlags )
{
	if ( nFlushFlags & MDLCACHE_FLUSH_STUDIOHWDATA )
	{
		if ( ClearAsync( pStudioData->m_Handle, MDLCACHE_STUDIOHWDATA, 0, true ) )
		{
			m_pMeshCacheSection->Unlock( pStudioData->m_VertexCache );
		}
	}

	if ( nFlushFlags & MDLCACHE_FLUSH_VERTEXES )
	{
		ClearAsync( pStudioData->m_Handle, MDLCACHE_VERTEXES, 0, true );
	}
}

void CMDLCache::Flush( studiodata_t *pStudioData, MDLCacheFlush_t nFlushFlags )
{
	Assert( pStudioData != NULL );

	bool bIgnoreLock = ( nFlushFlags & MDLCACHE_FLUSH_IGNORELOCK ) != 0;

	// release the hardware portion
	if ( nFlushFlags & MDLCACHE_FLUSH_STUDIOHWDATA )
	{
		UnloadHardwareData( pStudioData );
	}

	// free collision
	if ( nFlushFlags & MDLCACHE_FLUSH_VCOLLIDE )
	{
		DestroyVCollide( pStudioData );
	}

	// Free animations
	if ( nFlushFlags & MDLCACHE_FLUSH_VIRTUALMODEL )
	{
		FreeVirtualModel( pStudioData );
	}

	if ( nFlushFlags & MDLCACHE_FLUSH_ANIMBLOCK )
	{
		FreeAnimBlocks( pStudioData );
	}

	if ( nFlushFlags & MDLCACHE_FLUSH_AUTOPLAY )
	{
		// Free autoplay sequences
		FreeAutoplaySequences( pStudioData );
	}

	if ( nFlushFlags & MDLCACHE_FLUSH_COMBINED_DATA )
	{
#ifdef DEBUG_COMBINER
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED_ASSET ) != 0 )
		{
			Msg( "%p Flush: pStudioData=%p Flags=%08x", pStudioData->m_pCombinedStudioData, pStudioData, pStudioData->m_nFlags );
			if ( pStudioData->m_pCombinedStudioData )
			{
				Msg( " Reference=%08x", pStudioData->m_pCombinedStudioData->m_nReferenceFlags );
			}
			Msg( "\n" );
		}
#endif
		if ( pStudioData->m_pCombinedStudioData != NULL )
		{
			if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED_PLACEHOLDER ) != 0 )
			{
				pStudioData->m_pCombinedStudioData->m_nReferenceFlags &= ~COMBINED_REFERENCE_PLACEHOLDER;
			}
			if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED ) != 0 )
			{
				pStudioData->m_pCombinedStudioData->m_nReferenceFlags &= ~COMBINED_REFERENCE_PRIMARY;
			}

			if ( pStudioData->m_pCombinedStudioData->m_nReferenceFlags == 0 )
			{
#if 0
				if ( pStudioData->m_pCombinedStudioData->m_pCombineData != NULL )
				{
					//				Assert( 0 ); // is this currently in flight in the combiner thread?
					delete pStudioData->m_pCombinedStudioData->m_pCombineData;
				}
#endif
#ifdef DEBUG_COMBINER
				Msg( "%p Free: pStudioData=%p\n", pStudioData->m_pCombinedStudioData, pStudioData );
#endif
				FreeCombinedGeneratedData( pStudioData );
				free( pStudioData->m_pCombinedStudioData );
			}
#ifdef DEBUG_COMBINER
			else if ( pStudioData->m_pCombinedStudioData->m_nReferenceFlags == COMBINED_REFERENCE_COMBINER )
			{
				Msg( "%p Combiner Reference: pStudioData=%p\n", pStudioData->m_pCombinedStudioData, pStudioData );
				Assert( 0 );
				//				Error( "CMDLCache::UpdateCombiner - freeing model handles before combiner finishes" );
			}
#endif
			pStudioData->m_pCombinedStudioData = NULL;
		}
#ifdef DEBUG_COMBINER
		else if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED_ASSET ) != 0 )
		{
			Msg( "%p Nothing to Free: pStudioData=%p\n", pStudioData->m_pCombinedStudioData, pStudioData );
			Assert( 0 );
		}
#endif
	}
	
	if ( nFlushFlags & MDLCACHE_FLUSH_STUDIOHDR )
	{
		MdlCacheMsg( "MDLCache: Free studiohdr %s\n", GetModelName( pStudioData->m_Handle ) );

		if ( pStudioData->m_pForceLockedStudioHdr )
		{
			GetCacheSection( MDLCACHE_STUDIOHDR )->Unlock( pStudioData->m_MDLCache );
			pStudioData->m_pForceLockedStudioHdr = NULL;
		}
		UncacheData( pStudioData->m_MDLCache, MDLCACHE_STUDIOHDR, bIgnoreLock );
		pStudioData->m_MDLCache = NULL;
	}

	if ( nFlushFlags & MDLCACHE_FLUSH_VERTEXES )
	{
		MdlCacheMsg( "MDLCache: Free VVD %s\n", GetModelName( pStudioData->m_Handle ) );

		if ( pStudioData->m_pForceLockedVertexFileHeader )
		{
			GetCacheSection( MDLCACHE_VERTEXES )->Unlock( pStudioData->m_VertexCache );
			pStudioData->m_pForceLockedVertexFileHeader = NULL;
		}

		ClearAsync( pStudioData->m_Handle, MDLCACHE_VERTEXES, 0, true );

		UncacheData( pStudioData->m_VertexCache, MDLCACHE_VERTEXES, bIgnoreLock );
		pStudioData->m_VertexCache = NULL;
	}
}

//-----------------------------------------------------------------------------
// Flushes an MDLHandle_t
//-----------------------------------------------------------------------------
void CMDLCache::Flush( MDLHandle_t handle, int nFlushFlags )
{
	studiodata_t *pStudioData = m_MDLDict[handle];

	Flush( pStudioData, ( MDLCacheFlush_t )nFlushFlags );
}


//-----------------------------------------------------------------------------
// Inits, shuts downs studiodata_t
//-----------------------------------------------------------------------------
void CMDLCache::InitStudioData( MDLHandle_t handle )
{
	Assert( m_MDLDict[handle] == NULL );

	studiodata_t *pStudioData = new studiodata_t;
	m_MDLDict[handle] = pStudioData;
	memset( pStudioData, 0, sizeof( studiodata_t ) );
	pStudioData->m_Handle = handle;
}

void CMDLCache::ShutdownStudioData( MDLHandle_t handle, bool bImmediate )
{
#ifdef PLATFORM_WINDOWS_PC

	BeginLock();

	if ( bImmediate == false )
	{
#ifdef DEBUG_COMBINER
		studiodata_t *pStudioData = m_MDLDict[handle];

		if ( ( pStudioData->m_nFlags & ( STUDIODATA_FLAGS_COMBINED_ASSET ) ) == STUDIODATA_FLAGS_COMBINED_ASSET )
		{
			Msg( "%p ShutdownStudioData: pStudioData=%p\n", pStudioData->m_pCombinedStudioData, pStudioData );
		}
#endif

		m_UnloadHandles.PushItem( m_MDLDict[ handle ] );
		FlushImmediate( m_MDLDict[ handle ] );
		m_MDLDict[handle] = NULL;
	}
	else
	{
		FlushImmediate( m_MDLDict[ handle ] );
		Flush( handle );

		studiodata_t *pStudioData = m_MDLDict[handle];
		Assert( pStudioData != NULL );
		delete pStudioData;
		m_MDLDict[handle] = NULL;
	}

	EndLock();

#else

	FlushImmediate( m_MDLDict[ handle ] );
	Flush( handle );

	studiodata_t *pStudioData = m_MDLDict[handle];
	Assert( pStudioData != NULL );
	delete pStudioData;
	m_MDLDict[handle] = NULL;

#endif // PLATFORM_WINDOWS
}


//-----------------------------------------------------------------------------
// Sets the cache notify
//-----------------------------------------------------------------------------
void CMDLCache::SetCacheNotify( IMDLCacheNotify *pNotify )
{
	m_pCacheNotify = pNotify;
}


//-----------------------------------------------------------------------------
// Returns the name of the model
//-----------------------------------------------------------------------------
const char *CMDLCache::GetModelName( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID || !m_MDLDict.IsValidIndex( handle ) )
		return ERROR_MODEL;

	return m_MDLDict.GetElementName( handle );
}


//-----------------------------------------------------------------------------
// Returns the *actual* name of the model (could be an error model)
//-----------------------------------------------------------------------------
const char *CMDLCache::GetActualModelName( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return ERROR_MODEL;

	if ( m_MDLDict[handle]->m_nFlags & STUDIODATA_ERROR_MODEL )
		return ERROR_MODEL;

	return m_MDLDict.GetElementName( handle );
}


//-----------------------------------------------------------------------------
// Finds an MDL
//-----------------------------------------------------------------------------
MDLHandle_t CMDLCache::FindMDL( const char *pMDLRelativePath )
{
	// can't trust provided path
	// ensure provided path correctly resolves (Dictionary is case-insensitive)
	char szFixedName[MAX_PATH];
	V_strncpy( szFixedName, pMDLRelativePath, sizeof( szFixedName ) );
	V_RemoveDotSlashes( szFixedName, '/' );

	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_MODEL, szFixedName ) )
		{
			Q_strncpy( szFixedName, ERROR_MODEL, sizeof(szFixedName) );
		}
	}

	MDLHandle_t handle = m_MDLDict.Find( szFixedName );
	if ( handle == m_MDLDict.InvalidIndex() )
	{
		handle = m_MDLDict.Insert( szFixedName, NULL );
		InitStudioData( handle );
	}

	AddRef( handle );
	return handle;
}

//-----------------------------------------------------------------------------
// Reference counting
//-----------------------------------------------------------------------------
int CMDLCache::AddRef( MDLHandle_t handle )
{
	return ++m_MDLDict[handle]->m_nRefCount;
}

int CMDLCache::Release( MDLHandle_t handle )
{
	// Deal with shutdown order issues (i.e. datamodel shutting down after mdlcache)
	if ( !m_bInitialized )
		return 0;

	// NOTE: It can be null during shutdown because multiple studiomdls
	// could be referencing the same virtual model
	if ( !m_MDLDict[handle] )
		return 0;

	Assert( m_MDLDict[handle]->m_nRefCount > 0 );

	int nRefCount = --m_MDLDict[handle]->m_nRefCount;
	if ( nRefCount <= 0 )
	{
		ShutdownStudioData( handle, false );
		m_MDLDict.RemoveAt( handle );
	}

	return nRefCount;
}

int CMDLCache::GetRef( MDLHandle_t handle )
{
	if ( !m_bInitialized )
		return 0;

	if ( !m_MDLDict[handle] )
		return 0;

	return m_MDLDict[handle]->m_nRefCount;
}

//-----------------------------------------------------------------------------
// Unserializes the PHY file associated w/ models (the vphysics representation)
//-----------------------------------------------------------------------------
void CMDLCache::UnserializeVCollide( MDLHandle_t handle, bool bUseAsync, bool synchronousLoad )
{
	VPROF( "CMDLCache::UnserializeVCollide" );

	// FIXME: Should the vcollde be played into cacheable memory?
	studiodata_t *pStudioData = m_MDLDict[handle];

	intp iAsync = GetAsyncInfoIndex( handle, MDLCACHE_VCOLLIDE );

	if ( iAsync == NO_ASYNC )
	{
		// clear existing data
		pStudioData->m_nFlags &= ~STUDIODATA_FLAGS_VCOLLISION_LOADED;
		Assert( pStudioData->m_pVCollide == NULL);
		pStudioData->m_pVCollide = NULL;

#if 0
		// FIXME:  ywb
		// If we don't ask for the virtual model to load, then we can get a hitch later on after startup
		// Should we async load the sub .mdls during startup assuming they'll all be resident by the time the level can actually
		//  start drawing?
		if ( pStudioData->m_pVirtualModel || synchronousLoad )
#endif
		{
			pStudioData->m_nFlags |= STUDIODATA_FLAGS_VCOLLISION_SCANNED;
			virtualmodel_t *pVirtualModel = GetVirtualModel( handle );
			if ( pVirtualModel )
			{
				for ( int i = 1; i < pVirtualModel->m_group.Count(); i++ )
				{
					MDLHandle_t sharedHandle = VoidPtrToMDLHandle( pVirtualModel->m_group[i].cache );
					studiodata_t *pData = m_MDLDict[sharedHandle];
					if ( !(pData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED) )
					{
						UnserializeVCollide( sharedHandle, bUseAsync, synchronousLoad );
					}
					if ( pData->m_pVCollide != NULL )
					{
						pData->m_pVCollide->AddRef();
						pStudioData->m_pVCollide = pData->m_pVCollide;
						pStudioData->m_nFlags |= STUDIODATA_FLAGS_VCOLLISION_LOADED;
						return;
					}
				}
			}
		}

		char pFileName[MAX_PATH];
		Q_strncpy( pFileName, GetActualModelName( handle ), MAX_PATH );
		Q_SetExtension( pFileName, ".phy", sizeof( pFileName ) );
		Q_FixSlashes( pFileName );
#ifdef POSIX
		Q_strlower( pFileName );
#endif
		if ( IsGameConsole() )
		{
			char pX360Filename[MAX_PATH];
			UpdateOrCreate( NULL, pFileName, pX360Filename, sizeof( pX360Filename ), "GAME" );
			Q_strncpy( pFileName, pX360Filename, sizeof(pX360Filename) );
		}

		bool bAsyncLoad = bUseAsync && !synchronousLoad;

		MdlCacheMsg( "MDLCache: %s load vcollide %s\n", bAsyncLoad ? "Async" : "Sync", GetModelName( handle ) );

		AsyncInfo_t info;
		if ( IsDebug() )
		{
			memset( &info, 0xdd, sizeof( AsyncInfo_t ) );
		}
		info.hModel = handle;
		info.type = MDLCACHE_VCOLLIDE;
		info.iAnimBlock = 0;
		info.hControl = NULL;
		LoadData( pFileName, "GAME", bAsyncLoad, &info.hControl, handle );
		{
			AUTO_LOCK_FM( m_AsyncMutex );
			iAsync = SetAsyncInfoIndex( handle, MDLCACHE_VCOLLIDE, m_PendingAsyncs.AddToTail( info ) );
		}
	}
	else if ( synchronousLoad )
	{
		AsyncInfo_t *pInfo;
		{
			AUTO_LOCK_FM( m_AsyncMutex );
			pInfo = &m_PendingAsyncs[iAsync];
		}
		if ( pInfo->hControl )
		{
			g_pFullFileSystem->AsyncFinish( pInfo->hControl, true );
		}
	}

	ProcessPendingAsync( iAsync );
}

//-----------------------------------------------------------------------------
// Free model's collision data
//-----------------------------------------------------------------------------
void CMDLCache::DestroyVCollide( studiodata_t *pStudioData )
{
	if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED )
	{
		pStudioData->m_nFlags &= ~STUDIODATA_FLAGS_VCOLLISION_LOADED;
		if ( pStudioData->m_pVCollide )
		{
			if ( m_pCacheNotify )
			{
				m_pCacheNotify->OnDataUnloaded( MDLCACHE_VCOLLIDE, pStudioData->m_Handle );
			}

			MdlCacheMsg( "MDLCache: Unload vcollide %s\n", GetModelName( pStudioData->m_Handle ) );

			pStudioData->m_pVCollide->Release();
			pStudioData->m_pVCollide = NULL;
		}
	}
}


void CMDLCache::ReloadVCollide( MDLHandle_t handle )
{
	studiodata_t *pStudioData = m_MDLDict[handle];
	ExecuteNTimes(1, Warning( "ReloadVCollide invoked and will leak memory\n" ) );
	pStudioData->m_nFlags &= ~( STUDIODATA_FLAGS_VCOLLISION_LOADED | STUDIODATA_FLAGS_VCOLLISION_SCANNED );
	// this is where we leak the memory
	pStudioData->m_pVCollide = NULL;

	virtualmodel_t *pVirtualModel = GetVirtualModel( handle );
	if ( pVirtualModel )
	{
		for ( int i = 1; i < pVirtualModel->m_group.Count(); i++ )
		{
			MDLHandle_t sharedHandle = VoidPtrToMDLHandle( pVirtualModel->m_group[i].cache );
			ReloadVCollide( sharedHandle );
		}
	}

	// now, reload
	GetVCollideEx( handle, true );
}


//-----------------------------------------------------------------------------
// Unserializes the PHY file associated w/ models (the vphysics representation)
//-----------------------------------------------------------------------------
vcollide_t *CMDLCache::GetVCollideEx( MDLHandle_t handle, bool synchronousLoad /*= true*/ )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( handle == MDLHANDLE_INVALID )
		return NULL;

	if ( m_MDLDict[handle]->m_nFlags & STUDIODATA_ERROR_MODEL )
		return NULL;

	if ( m_bDisableVCollideLoad )
		return NULL;

	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED ) == 0 )
	{
		UnserializeVCollide( handle, mod_load_vcollide_async.GetBool(), synchronousLoad );
	}
	// in queued mode we need to scan the virtual model for shared vcollides
	if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_SCANNED ) == 0 )
	{
		pStudioData->m_nFlags |= STUDIODATA_FLAGS_VCOLLISION_SCANNED;
		if ( !pStudioData->m_pVCollide )
		{
			virtualmodel_t *pVirtualModel = GetVirtualModel( handle );
			if ( pVirtualModel )
			{
				for ( int i = 1; i < pVirtualModel->m_group.Count(); i++ )
				{
					MDLHandle_t sharedHandle = VoidPtrToMDLHandle( pVirtualModel->m_group[i].cache );
					studiodata_t *pData = m_MDLDict[sharedHandle];
					if ( pData->m_pVCollide )
					{
						pStudioData->m_pVCollide = pData->m_pVCollide;
						pData->m_pVCollide->AddRef();
						break;
					}
				}
			}
		}
	}

	// We've loaded an empty collision file or no file was found, so return NULL
	if ( !pStudioData->m_pVCollide )
		return NULL;

	// returned pointer to shared vcollide
	return pStudioData->m_pVCollide->GetVCollide();
}


bool CMDLCache::GetVCollideSize( MDLHandle_t handle, int *pVCollideSize )
{
	*pVCollideSize = 0;

	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( !pStudioData->m_pVCollide )
		return false;

	vcollide_t *pCollide = pStudioData->m_pVCollide->GetVCollide();
	for ( int j = 0; j < pCollide->solidCount; j++ )
	{
		*pVCollideSize += g_pPhysicsCollision->CollideSize( pCollide->solids[j] );
	}
	*pVCollideSize += pCollide->descSize;
	return true;
}

//-----------------------------------------------------------------------------
// Allocates/frees the anim blocks
//-----------------------------------------------------------------------------
void CMDLCache::AllocateAnimBlocks( studiodata_t *pStudioData, int nCount )
{
	Assert( pStudioData->m_vecAnimBlocks.Count() == 0 );

	pStudioData->m_vecAnimBlocks.EnsureCount( nCount );
	memset( pStudioData->m_vecAnimBlocks.Base(), 0, sizeof(DataCacheHandle_t) * nCount );

	pStudioData->m_vecFakeAnimBlockStall.EnsureCount( nCount );
	memset( pStudioData->m_vecFakeAnimBlockStall.Base(), 0, sizeof( unsigned long ) * nCount );

#ifdef DEBUG_ANIM_STALLS
	pStudioData->m_vecFirstRequest.EnsureCount( nCount );
	memset( pStudioData->m_vecFirstRequest.Base(), 0, sizeof( unsigned long ) * nCount );
#endif
}

void CMDLCache::FreeAnimBlocks( studiodata_t *pStudioData )
{
	for ( int i = 0; i < pStudioData->m_vecAnimBlocks.Count(); ++i )
	{
		MdlCacheMsg( "MDLCache: Free Anim block %s (block: %d)\n", GetModelName( pStudioData->m_Handle ), i );
		ClearAsync( pStudioData->m_Handle, MDLCACHE_ANIMBLOCK, i, true );
		if ( pStudioData->m_vecAnimBlocks[i] )
		{
			UncacheData( pStudioData->m_vecAnimBlocks[i], MDLCACHE_ANIMBLOCK, true );
		}
	}

	pStudioData->m_vecAnimBlocks.Purge();
	pStudioData->m_vecFakeAnimBlockStall.Purge();
#ifdef DEBUG_ANIM_STALLS
	pStudioData->m_vecFirstRequest.Purge();
#endif
}


//-----------------------------------------------------------------------------
// Unserializes an animation block from disk
//-----------------------------------------------------------------------------
unsigned char *CMDLCache::UnserializeAnimBlock( MDLHandle_t handle, bool bUseAsync, int nBlock )
{
	VPROF( "CMDLCache::UnserializeAnimBlock" );

	if ( IsGameConsole() && g_pQueuedLoader->IsMapLoading() )
	{
		// anim block i/o is not allowed at this stage
		return NULL;
	}

	// Block 0 is never used!!!
	Assert( nBlock > 0 );

	studiodata_t *pStudioData = m_MDLDict[handle];

	intp iAsync = GetAsyncInfoIndex( handle, MDLCACHE_ANIMBLOCK, nBlock );

	if ( iAsync == NO_ASYNC )
	{
		studiohdr_t *pStudioHdr = GetStudioHdr( handle );

		// FIXME: For consistency, the block name maybe shouldn't have 'model' in it.
		char const *pModelName = pStudioHdr->pszAnimBlockName();
		mstudioanimblock_t *pBlock = pStudioHdr->pAnimBlock( nBlock );
		int nSize = pBlock->dataend - pBlock->datastart;
		if ( nSize == 0 )
			return NULL;

		// allocate space in the cache
		pStudioData->m_vecAnimBlocks[nBlock] = NULL;

		char pFileName[MAX_PATH];
		Q_strncpy( pFileName, pModelName, sizeof(pFileName) );
		Q_FixSlashes( pFileName );
#ifdef POSIX
		Q_strlower( pFileName );
#endif
		if ( IsGameConsole() )
		{
			char pX360Filename[MAX_PATH];
			UpdateOrCreate( pStudioHdr, pFileName, pX360Filename, sizeof( pX360Filename ), "GAME" );
			Q_strncpy( pFileName, pX360Filename, sizeof(pX360Filename) );
		}

		MdlCacheMsg( "MDLCache: Begin load Anim Block %s (block %i, bytes %d)\n", GetModelName( handle ), nBlock, nSize );

		AsyncInfo_t info;
		if ( IsDebug() )
		{
			memset( &info, 0xdd, sizeof( AsyncInfo_t ) );
		}
		info.hModel = handle;
		info.type = MDLCACHE_ANIMBLOCK;
		info.iAnimBlock = nBlock;
		info.hControl = NULL;
		void *pData;
		if ( nSize <= ANIMBLOCK_SIZE )
		{
			// using the pool
			pData = g_AnimBlockAllocator.Alloc();
		}
		else
		{
			// Null will yield file system allocating an optimal read buffer
			pData = NULL;
			Warning( "%s(%d): MDL Cache allocation outside the pool. %s : %d.\n", __FILE__, __LINE__, pStudioHdr->pszName(), nSize );
		}

		LoadData( pFileName, "GAME", pData, nSize, pBlock->datastart, bUseAsync, &info.hControl, handle );

		Assert( m_AsyncMutex.GetOwnerId() == ThreadGetCurrentId() );
		iAsync = SetAsyncInfoIndex( handle, MDLCACHE_ANIMBLOCK, nBlock, m_PendingAsyncs.AddToTail( info ) );

#ifdef DEBUG_ANIM_STALLS
		// keep track of when it was requested
		pStudioData->m_vecFirstRequest[nBlock] = Plat_MSTime();
#endif

#ifdef DEBUG_ANIM_BLOCKS_LOADED
		mstudioanimdesc_t &animdesc = pStudioHdr->pAnimdesc( nBlock );
		Msg( "AnimBlock: %s (%s) %d \n", animdesc.pszName(), pStudioHdr->pszName(), nSize );
#endif
	}

	ProcessPendingAsync( iAsync );

	return ( unsigned char * )CheckData( pStudioData->m_vecAnimBlocks[nBlock], MDLCACHE_ANIMBLOCK );
}

//-----------------------------------------------------------------------------
// Gets at an animation block associated with an MDL
//-----------------------------------------------------------------------------
unsigned char *CMDLCache::GetAnimBlock( MDLHandle_t handle, int nBlock, bool preloadIfMissing )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( handle == MDLHANDLE_INVALID )
		return NULL;

	if ( m_MDLDict[handle]->m_nFlags & STUDIODATA_ERROR_MODEL )
		return NULL;

	// Allocate animation blocks if we don't have them yet
	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( pStudioData->m_vecAnimBlocks.Count() == 0 )
	{
		AUTO_LOCK_FM( m_AsyncMutex );
		if ( pStudioData->m_vecAnimBlocks.Count() == 0 )
		{
			studiohdr_t *pStudioHdr = GetStudioHdr( handle );
			AllocateAnimBlocks( pStudioData, pStudioHdr->numanimblocks );
		}
	}

	// check for request being in range
	if ( nBlock < 0 || nBlock >= pStudioData->m_vecAnimBlocks.Count())
		return NULL;

	// Check the cache to see if the animation is in memory
	unsigned char *pData = ( unsigned char * )CheckData( pStudioData->m_vecAnimBlocks[nBlock], MDLCACHE_ANIMBLOCK );
	if ( !pData )
	{
		AUTO_LOCK_FM( m_AsyncMutex );
		pData = ( unsigned char * )CheckData( pStudioData->m_vecAnimBlocks[nBlock], MDLCACHE_ANIMBLOCK );
		if ( !pData )
		{
			pStudioData->m_vecAnimBlocks[nBlock] = NULL;

			if ( preloadIfMissing )
			{
				// It's not in memory, read it off of disk
				pData = UnserializeAnimBlock( handle, mod_load_anims_async.GetBool(), nBlock );
			}
		}
	}

	if (mod_load_fakestall.GetInt())
	{
		unsigned long t = Plat_MSTime();
		if (pStudioData->m_vecFakeAnimBlockStall[nBlock] == 0 || pStudioData->m_vecFakeAnimBlockStall[nBlock] > t)
		{
			pStudioData->m_vecFakeAnimBlockStall[nBlock] = t;
		}

		if ((int)(t - pStudioData->m_vecFakeAnimBlockStall[nBlock]) < mod_load_fakestall.GetInt())
		{
			return NULL;
		}
	}
	return pData;
}

//-----------------------------------------------------------------------------
// Indicates if an anim block has been preloaded (either already in memory or in asynchronous loading).
//-----------------------------------------------------------------------------
bool CMDLCache::HasAnimBlockBeenPreloaded( MDLHandle_t handle, int nBlock )
{
	if ( mod_test_not_available.GetBool() )
		return false;

	if ( handle == MDLHANDLE_INVALID )
		return false;

	if ( m_MDLDict[handle]->m_nFlags & STUDIODATA_ERROR_MODEL )
		return false;

	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( nBlock <= 0 )
		return true;

	// check for request being in range
	if ( nBlock >= pStudioData->m_vecAnimBlocks.Count() )
		return false;

	// Check the cache to see if the animation is in memory
	// TODO: Investigate if the double testing is really necessary here.
	unsigned char *pData = ( unsigned char * )CheckData( pStudioData->m_vecAnimBlocks[nBlock], MDLCACHE_ANIMBLOCK );
	if ( pData == NULL )
	{
		AUTO_LOCK_FM( m_AsyncMutex );
		pData = ( unsigned char * )CheckData( pStudioData->m_vecAnimBlocks[nBlock], MDLCACHE_ANIMBLOCK );
		if ( pData != NULL )
		{
			return true;
		}
	}
	else
	{
		return true;
	}

	// Data was not cached already, but maybe in async loading
	int iAsync = GetAsyncInfoIndex( handle, MDLCACHE_ANIMBLOCK, nBlock );
	return ( iAsync != NO_ASYNC );
}

//-----------------------------------------------------------------------------
// Allocates/frees autoplay sequence list
//-----------------------------------------------------------------------------
void CMDLCache::AllocateAutoplaySequences( studiodata_t *pStudioData, int nCount )
{
	FreeAutoplaySequences( pStudioData );

	pStudioData->m_vecAutoplaySequenceList.EnsureCount( nCount );
}

void CMDLCache::FreeAutoplaySequences( studiodata_t *pStudioData )
{
	pStudioData->m_vecAutoplaySequenceList.Purge();
}


//-----------------------------------------------------------------------------
// Gets the autoplay list
//-----------------------------------------------------------------------------
int CMDLCache::GetAutoplayList( MDLHandle_t handle, unsigned short **pAutoplayList )
{
	if ( pAutoplayList )
	{
		*pAutoplayList = NULL;
	}

	if ( handle == MDLHANDLE_INVALID )
		return 0;

	virtualmodel_t *pVirtualModel = GetVirtualModel( handle );
	if ( pVirtualModel )
	{
		if ( pAutoplayList && pVirtualModel->m_autoplaySequences.Count() )
		{
			*pAutoplayList = pVirtualModel->m_autoplaySequences.Base();
		}
		return pVirtualModel->m_autoplaySequences.Count();
	}

	// FIXME: Should we cache autoplay info here on demand instead of in unserializeMDL?
	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( pAutoplayList )
	{
		*pAutoplayList = pStudioData->m_vecAutoplaySequenceList.Base();
	}

	return pStudioData->m_vecAutoplaySequenceList.Count();
}


//-----------------------------------------------------------------------------
// Allocates/frees the virtual model
//-----------------------------------------------------------------------------
void CMDLCache::AllocateVirtualModel( MDLHandle_t handle )
{
	studiodata_t *pStudioData = m_MDLDict[handle];
	Assert( pStudioData->m_pVirtualModel == NULL );
	pStudioData->m_pVirtualModel = new virtualmodel_t;

	// FIXME: The old code slammed these; could have leaked memory?
	Assert( pStudioData->m_vecAnimBlocks.Count() == 0 );
	Assert( pStudioData->m_vecAnimBlocks.Count() == 0 );
}

void CMDLCache::FreeVirtualModel( studiodata_t *pStudioData )
{
	if ( pStudioData && pStudioData->m_pVirtualModel )
	{
		int nGroupCount = pStudioData->m_pVirtualModel->m_group.Count();
		Assert( (nGroupCount >= 1) && pStudioData->m_pVirtualModel->m_group[0].cache == (void*)(uintp)pStudioData->m_Handle );

		// NOTE: Start at *1* here because the 0th element contains a reference to *this* handle
		for ( int i = 1; i < nGroupCount; ++i )
		{
			MDLHandle_t h = VoidPtrToMDLHandle( pStudioData->m_pVirtualModel->m_group[i].cache );
			FreeVirtualModel( m_MDLDict[ h ] );
			Release( h );
		}

		delete pStudioData->m_pVirtualModel;
		pStudioData->m_pVirtualModel = NULL;
	}
}


//-----------------------------------------------------------------------------
// Returns the virtual model
//-----------------------------------------------------------------------------
virtualmodel_t *CMDLCache::GetVirtualModel( MDLHandle_t handle )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( handle == MDLHANDLE_INVALID )
		return NULL;

	studiohdr_t *pStudioHdr = GetStudioHdr( handle );

	if ( pStudioHdr == NULL )
		return NULL;

	return GetVirtualModelFast( pStudioHdr, handle );
}

virtualmodel_t *CMDLCache::GetVirtualModelFast( const studiohdr_t *pStudioHdr, MDLHandle_t handle )
{
	if (pStudioHdr->numincludemodels == 0)
		return NULL;

	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( !pStudioData )
		return NULL;

	// These exist just so we can get some valid pointers when we're trying to catch a crash here
	static const studiohdr_t *pDebugStudioHdr = pStudioHdr;
	static const MDLHandle_t pDebugHandle = handle;
	static const studiodata_t *pDebugStudioData = m_MDLDict[handle];

	if ( !pStudioData->m_pVirtualModel )
	{
		DevMsg( 2, "Loading virtual model for %s\n", pStudioHdr->pszName() );

		CMDLCacheCriticalSection criticalSection( this );

		AllocateVirtualModel( handle );

		// Group has to be zero to ensure refcounting is correct
		int nGroup = pStudioData->m_pVirtualModel->m_group.AddToTail( );
		Assert( nGroup == 0 );
		pStudioData->m_pVirtualModel->m_group[nGroup].cache = (void *)(uintp)handle;

		// Add all dependent data
		pStudioData->m_pVirtualModel->AppendModels( 0, pStudioHdr );
	}

	return pStudioData->m_pVirtualModel;
}

//-----------------------------------------------------------------------------
// Purpose: Pulls all submodels/.ani file models into the cache
// to avoid runtime hitches and load animations at load time, set mod_forcedata to be 1
//-----------------------------------------------------------------------------
void CMDLCache::UnserializeAllVirtualModelsAndAnimBlocks( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return;

	// might be re-loading, discard old virtualmodel to force rebuild
	// unfortunately, the virtualmodel does build data into the cacheable studiohdr
	FreeVirtualModel( m_MDLDict[handle] );

	if ( IsGameConsole() && g_pQueuedLoader->IsMapLoading() )
	{
		// queued loading has to do it
		return;
	}

	// don't load the submodel data
	if ( !mod_forcedata.GetBool() )
		return;

	// if not present, will instance and load the submodels
	GetVirtualModel( handle );

	if ( IsGameConsole() )
	{
		// 360 does not drive the anims into its small cache section
		return;
	}

	// Note that the animblocks start at 1!!!
	studiohdr_t *pStudioHdr = GetStudioHdr( handle );
	for ( int i = 1 ; i < (int)pStudioHdr->numanimblocks; ++i )
	{
		GetAnimBlock( handle, i, true );
	}

	ProcessPendingAsyncs( MDLCACHE_ANIMBLOCK );
}


//-----------------------------------------------------------------------------
// Loads the static meshes
//-----------------------------------------------------------------------------
bool CMDLCache::UnserializeHardwareData( MDLHandle_t handle, bool bUseAsync )
{
	Assert( handle != MDLHANDLE_INVALID );

	// Don't try to load VTX files if we don't have focus...
	if ( m_bLostVideoMemory )
		return false;

	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( pStudioData->m_pCombinedStudioData && pStudioData->m_pCombinedStudioData->m_FinalHandle == handle )
	{
		return UnserializeCombinedHardwareData( handle );
	}

	CMDLCacheCriticalSection criticalSection( this );

	// Load up the model
	studiohdr_t *pStudioHdr = GetStudioHdr( handle );
	if ( !pStudioHdr || !pStudioHdr->numbodyparts )
	{
		pStudioData->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
		return true;
	}

	if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_STUDIOMESH )
	{
		return false;
	}

	if ( LogMdlCache() &&
		 GetAsyncInfoIndex( handle, MDLCACHE_STUDIOHWDATA ) == NO_ASYNC &&
		 GetAsyncInfoIndex( handle, MDLCACHE_VERTEXES ) == NO_ASYNC )
	{
		MdlCacheMsg( "MDLCache: Begin load studiomdl %s\n", GetModelName( handle ) );
	}

	// Vertex data is required to call LoadModel(), so make sure that's ready
	if ( !GetVertexData( handle ) )
	{
		if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_VERTEX_DATA )
		{
			pStudioData->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
		}
		return false;
	}

	intp iAsync = GetAsyncInfoIndex( handle, MDLCACHE_STUDIOHWDATA );

	if ( iAsync == NO_ASYNC )
	{
		m_pMeshCacheSection->Lock( pStudioData->m_VertexCache );

		// load and persist the vtx file
		// use model name for correct path
		char pFileName[MAX_PATH];
		MakeFilename( pFileName, pStudioHdr, GetVTXExtension() );
		if ( IsGameConsole() )
		{
			char pX360Filename[MAX_PATH];
			UpdateOrCreate( pStudioHdr, pFileName, pX360Filename, sizeof( pX360Filename ), "GAME" );
			Q_strncpy( pFileName, pX360Filename, sizeof(pX360Filename) );
		}

		MdlCacheMsg( "MDLCache: Begin load VTX %s\n", GetModelName( handle ) );

		AsyncInfo_t info;
		if ( IsDebug() )
		{
			memset( &info, 0xdd, sizeof( AsyncInfo_t ) );
		}
		info.hModel = handle;
		info.type = MDLCACHE_STUDIOHWDATA;
		info.iAnimBlock = 0;
		info.hControl = NULL;
		LoadData( pFileName, "GAME", bUseAsync, &info.hControl, handle );
		{
			AUTO_LOCK_FM( m_AsyncMutex );
			iAsync = SetAsyncInfoIndex( handle, MDLCACHE_STUDIOHWDATA, m_PendingAsyncs.AddToTail( info ) );
		}
	}

	if ( ProcessPendingAsync( iAsync ) > 0 )
	{
		if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_STUDIOMESH )
		{
			return false;
		}

		return ( pStudioData->m_HardwareData.m_NumStudioMeshes != 0 );
	}

	return false;
}



static bool SortLessFunc(const mstudiovertanim_t &left, const mstudiovertanim_t & right)
{
	return left.index < right.index;
}


static bool SortLessFuncWrinkle(const mstudiovertanim_wrinkle_t &left, const mstudiovertanim_wrinkle_t & right)
{
	return left.index < right.index;
}

CMiniProfiler s_mp_SortFlexData;

void CMDLCache::ConvertFlexData( studiohdr_t *pStudioHdr )
{
	float flVertAnimFixedPointScale = pStudioHdr->VertAnimFixedPointScale();

	for ( int i = 0; i < pStudioHdr->numbodyparts; i++ )
	{
		mstudiobodyparts_t *pBody = pStudioHdr->pBodypart( i );
		for ( int j = 0; j < pBody->nummodels; j++ )
		{
			mstudiomodel_t *pModel = pBody->pModel( j );
			for ( int k = 0; k < pModel->nummeshes; k++ )
			{
				mstudiomesh_t *pMesh = pModel->pMesh( k );
				for ( int l = 0; l < pMesh->numflexes; l++ )
				{
					mstudioflex_t *pFlex = pMesh->pFlex( l );
					bool bIsWrinkleAnim = ( pFlex->vertanimtype == STUDIO_VERT_ANIM_WRINKLE );
					for ( int m = 0; m < pFlex->numverts; m++ )
					{
						mstudiovertanim_t *pVAnim = bIsWrinkleAnim ?
							pFlex->pVertanimWrinkle( m ) : pFlex->pVertanim( m );
						pVAnim->ConvertToFixed( flVertAnimFixedPointScale );
					}

					CMiniProfilerGuard mpguard(&s_mp_SortFlexData);
					switch ( pFlex->vertanimtype )
					{
						case STUDIO_VERT_ANIM_NORMAL:
						{
							mstudiovertanim_t *pvanim = pFlex->pVertanim( 0 );
							mstudiovertanim_t *pvanimEnd = pvanim + pFlex->numverts;
							std::make_heap( pvanim, pvanimEnd, SortLessFunc ); 
							std::sort_heap( pvanim, pvanimEnd, SortLessFunc ); 
						}
						break;

						case STUDIO_VERT_ANIM_WRINKLE:
						{
							mstudiovertanim_wrinkle_t *pvanim = pFlex->pVertanimWrinkle( 0 );
							mstudiovertanim_wrinkle_t *pvanimEnd = pvanim + pFlex->numverts;
							std::make_heap( pvanim, pvanimEnd, SortLessFuncWrinkle ); 
							std::sort_heap( pvanim, pvanimEnd, SortLessFuncWrinkle ); 
						}
						break;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMDLCache::BuildHardwareData( MDLHandle_t handle, studiodata_t *pStudioData, studiohdr_t *pStudioHdr, CMDLCacheData &cacheData )
{
	OptimizedModel::FileHeader_t *pVtxHdr = (OptimizedModel::FileHeader_t *)cacheData.Data();

	if ( pVtxHdr )
	{
		MdlCacheMsg("MDLCache: Alloc VTX %s\n", pStudioHdr->pszName() );

		// check header
		if ( pVtxHdr->version != OPTIMIZED_MODEL_FILE_VERSION )
		{
			Warning( "Error Index File for '%s' version %d should be %d\n", pStudioHdr->pszName(), pVtxHdr->version, OPTIMIZED_MODEL_FILE_VERSION );
			pVtxHdr = NULL;
		}
		else if ( pVtxHdr->checkSum != pStudioHdr->checksum )
		{
			Warning( "Error Index File for '%s' checksum %ld should be %ld\n", pStudioHdr->pszName(), pVtxHdr->checkSum, pStudioHdr->checksum );
			pVtxHdr = NULL;
		}
	}

	if ( !pVtxHdr )
	{
		pStudioData->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
		return false;
	}

	MdlCacheMsg( "MDLCache: Load studiohwdata %s\n", pStudioHdr->pszName() );

	Assert( GetVertexData( handle ) );

	BeginCoarseLock();
	BeginLock();
	bool bLoaded = g_pStudioRender->LoadModel( pStudioHdr, pVtxHdr, &pStudioData->m_HardwareData );
	EndLock();
	EndCoarseLock();

	if ( bLoaded )
	{
		pStudioData->m_nFlags |= STUDIODATA_FLAGS_STUDIOMESH_LOADED;
	}
	else
	{
		pStudioData->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
	}

	if ( m_pCacheNotify )
	{
		m_pCacheNotify->OnDataLoaded( MDLCACHE_STUDIOHWDATA, handle );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Loads the static meshes
//-----------------------------------------------------------------------------
void CMDLCache::UnloadHardwareData( studiodata_t *pStudioData )
{
	// Don't load it if it's loaded
	if ( pStudioData && pStudioData->m_nFlags & STUDIODATA_FLAGS_STUDIOMESH_LOADED )
	{
		if ( m_pCacheNotify )
		{
			m_pCacheNotify->OnDataUnloaded( MDLCACHE_STUDIOHWDATA, pStudioData->m_Handle );
		}

		MdlCacheMsg( "MDLCache: Unload studiohwdata %s\n", GetModelName( pStudioData->m_Handle ) );

		g_pStudioRender->UnloadModel( &pStudioData->m_HardwareData );
		memset( &pStudioData->m_HardwareData, 0, sizeof( pStudioData->m_HardwareData ) );
		pStudioData->m_nFlags &= ~STUDIODATA_FLAGS_STUDIOMESH_LOADED;

		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED ) != 0 )
		{
			g_pMaterialSystem->UncacheUnusedMaterials();
		}
	}
}


void CMDLCache::UnloadQueuedHardwareData( )
{
	studiodata_t *handle = NULL;

	BeginLock();

	while( m_UnloadHandles.PopItem( &handle ) == true )
	{
		Flush( handle );
		delete handle;
	}

	EndLock();
}


//-----------------------------------------------------------------------------
// Returns the hardware data associated with an MDL
//-----------------------------------------------------------------------------
studiohwdata_t *CMDLCache::GetHardwareData( MDLHandle_t handle )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( mod_test_mesh_not_available.GetBool() )
		return NULL;

	studiodata_t *pStudioData = m_MDLDict[handle];
	if ( ( pStudioData->m_nFlags & (STUDIODATA_FLAGS_STUDIOMESH_LOADED | STUDIODATA_FLAGS_NO_STUDIOMESH) ) == 0 )
	{
		m_pMeshCacheSection->LockMutex();
		if ( ( pStudioData->m_nFlags & (STUDIODATA_FLAGS_STUDIOMESH_LOADED | STUDIODATA_FLAGS_NO_STUDIOMESH) ) == 0 )
		{
			m_pMeshCacheSection->UnlockMutex();
			if ( !UnserializeHardwareData( handle, mod_load_mesh_async.GetBool() ) )
			{
				return NULL;
			}
		}
		else
		{
			m_pMeshCacheSection->UnlockMutex();
		}
	}

	return &pStudioData->m_HardwareData;
}


//-----------------------------------------------------------------------------
// Task switch
//-----------------------------------------------------------------------------
void CMDLCache::ReleaseMaterialSystemObjects( int nChangeFlags )
{
	Assert( !m_bLostVideoMemory );
	m_bLostVideoMemory = true;

	ShutdownCombiner();

	BreakFrameLock( false );

	// Free all hardware data
	MDLHandle_t i = m_MDLDict.First();
	while ( i != m_MDLDict.InvalidIndex() )
	{
		UnloadHardwareData( m_MDLDict[ i ] );
		i = m_MDLDict.Next( i );
	}

	RestoreFrameLock();
}

void CMDLCache::RestoreMaterialSystemObjects( int nChangeFlags )
{
	Assert( m_bLostVideoMemory );
	m_bLostVideoMemory = false;

	BreakFrameLock( false );

	// Restore all hardware data
	MDLHandle_t i = m_MDLDict.First();
	while ( i != m_MDLDict.InvalidIndex() )
	{
		studiodata_t *pStudioData = m_MDLDict[i];

		bool bIsMDLInMemory = GetCacheSection( MDLCACHE_STUDIOHDR )->IsPresent( pStudioData->m_MDLCache );

		// If the vertex format changed, we have to free the data because we may be using different .vtx files.
		if ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED )
		{
			MdlCacheMsg( "MDLCache: Free studiohdr\n" );
			MdlCacheMsg( "MDLCache: Free VVD\n" );
			MdlCacheMsg( "MDLCache: Free VTX\n" );

			// FIXME: Do we have to free m_MDLCache + m_VertexCache?
			// Certainly we have to free m_IndexCache, cause that's a dx-level specific vtx file.
			ClearAsync( i, MDLCACHE_STUDIOHWDATA, 0, true );

			Flush( i, MDLCACHE_FLUSH_VERTEXES );
		}

		// Only restore the hardware data of those studiohdrs which are currently in memory
		if ( bIsMDLInMemory )
		{
			GetHardwareData( i );
		}

		i = m_MDLDict.Next( i );
	}

	RestoreFrameLock();
}


//-----------------------------------------------------------------------------
// Finalization step of loading. Part of an intricate loading control flow.
//-----------------------------------------------------------------------------
void CMDLCache::MarkAsLoaded( MDLHandle_t handle )
{
	// For the 360...
	// This should only be occurring at the very end of loading. The queued loader will have
	// either left the data in the datacache or populated it. This just re-hooks the
	// aliased pointers to data that should be in the cache. If the queued loader is
	// working properly, no i/o should be evented.

	if ( mod_lock_mdls_on_load.GetBool() )
	{
		// re-establish the cached model header, if the cache doesn't have it, it will cause i/o and get loaded now
		g_MDLCache.GetStudioHdr( handle );
		if ( !m_MDLDict[handle]->m_pForceLockedStudioHdr )
		{
			m_MDLDict[handle]->m_pForceLockedStudioHdr = (studiohdr_t *)GetCacheSection( MDLCACHE_STUDIOHDR )->Lock( m_MDLDict[handle]->m_MDLCache );
		}
	}

	if ( !mod_dont_load_vertices.GetInt() )
	{
		// re-establish the cached vertex header, if the cache doesn't have it, it will cause i/o and get loaded now
		if ( !mod_load_mesh_async.GetBool() && mod_lock_meshes_on_load.GetBool() && !( m_MDLDict[handle]->m_nFlags & STUDIODATA_FLAGS_NO_VERTEX_DATA ) )
		{
			GetVertexData( handle );
			if ( !m_MDLDict[handle]->m_pForceLockedVertexFileHeader )
			{
				m_MDLDict[handle]->m_pForceLockedVertexFileHeader = (vertexFileHeader_t *)GetCacheSection( MDLCACHE_VERTEXES )->Lock( m_MDLDict[handle]->m_VertexCache );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Callback for UpdateOrCreate utility function - swaps any studiomdl file type.
//-----------------------------------------------------------------------------
static bool MdlcacheCreateCallback( const char *pSourceName, const char *pTargetName, const char *pPathID, void *pHdr )
{
	// Missing studio files are permissible and not spewed as errors
	bool retval = false;
	CUtlBuffer sourceBuf;
	bool bOk = g_pFullFileSystem->ReadFile( pSourceName, NULL, sourceBuf );
	if ( bOk )
	{
		CUtlBuffer targetBuf;
		targetBuf.EnsureCapacity( sourceBuf.TellPut() + BYTESWAP_ALIGNMENT_PADDING );

		int bytes = StudioByteSwap::ByteswapStudioFile( pTargetName, targetBuf.Base(), targetBuf.Size(), sourceBuf.Base(), sourceBuf.TellPut(), (studiohdr_t*)pHdr );
		if ( bytes )
		{
			// If the file was an .mdl, attempt to swap the .ani as well
			if ( Q_stristr( pSourceName, ".mdl" ) )
			{
				char szANISourceName[ MAX_PATH ];
				Q_StripExtension( pSourceName, szANISourceName, sizeof( szANISourceName ) );
				Q_strncat( szANISourceName, ".ani", sizeof( szANISourceName ), COPY_ALL_CHARACTERS );
				UpdateOrCreate( szANISourceName, NULL, 0, pPathID, MdlcacheCreateCallback, true, targetBuf.Base() );
			}

			targetBuf.SeekPut( CUtlBuffer::SEEK_HEAD, bytes );
			g_pFullFileSystem->WriteFile( pTargetName, pPathID, targetBuf );
			retval = true;
		}
		else
		{
			Warning( "Failed to create %s\n", pTargetName );
		}
	}
	return retval;
}

//-----------------------------------------------------------------------------
// Calls utility function to create .360 version of a file.
//-----------------------------------------------------------------------------
int CMDLCache::UpdateOrCreate( studiohdr_t *pHdr, const char *pSourceName, char *pTargetName, int targetLen, const char *pPathID, bool bForce )
{
	return ::UpdateOrCreate( pSourceName, pTargetName, targetLen, pPathID, MdlcacheCreateCallback, bForce, pHdr );
}

//-----------------------------------------------------------------------------
// Purpose: Attempts to read a file native to the current platform
//-----------------------------------------------------------------------------
bool CMDLCache::ReadFileNative( char *pFileName, const char *pPath, CUtlBuffer &buf, int nMaxBytes )
{
	bool bOk = false;

	if ( IsGameConsole() )
	{
		// Read the 360 version
		char pX360Filename[ MAX_PATH ];
		UpdateOrCreate( NULL, pFileName, pX360Filename, sizeof( pX360Filename ), pPath );
		bOk = g_pFullFileSystem->ReadFile( pX360Filename, pPath, buf, nMaxBytes );
	}
	else
	{
		const char *pActualFilename = pFileName;
		if ( IsPC() )
		{
			pActualFilename = m_ModelSwapper.TranslateModelName( pFileName );
		}

		// Read the PC version
		bOk = g_pFullFileSystem->ReadFile( pActualFilename, pPath, buf, nMaxBytes );
	}

	return bOk;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
studiohdr_t *CMDLCache::UnserializeMDL( MDLHandle_t handle, CMDLCacheData &cacheData )
{
	studiohdr_t	*pStudioHdrIn = (studiohdr_t *)cacheData.Data();
	Assert( pStudioHdrIn );
	if ( !pStudioHdrIn )
		return NULL;
			
#ifdef CSTRIKE15
	// Slamp root LOD to 0 for CS:GO
	int nRootLOD = 0;
#else
	int nRootLOD = r_rootlod.GetInt();
#endif	

	if ( nRootLOD > 0 )
	{
		// raw data is already setup for lod 0, override otherwise
		static bool s_bTempRootLodEnable = !!CommandLine()->FindParm( "-r_rootlod_enable" );
		if ( s_bTempRootLodEnable )
		{
			Studio_SetRootLOD( pStudioHdrIn, nRootLOD );
		}
		else
		{
			ExecuteNTimes( 5, Warning( "r_rootlod is temporarily unsupported: bugbait#70052" ) );
		}
	}
	StudioHdrLookupSurfaceProps( pStudioHdrIn );
	if ( pStudioHdrIn->numincludemodels == 0 )
	{
		StudioHdrSetAnimEventFlag( pStudioHdrIn );
	}

	// critical! store a back link to our data
	// this is fetched when re-establishing dependent cached data (vtx/vvd)
	pStudioHdrIn->SetVirtualModel( MDLHandleToVirtual( handle ) );

	MdlCacheMsg( "MDLCache: Alloc studiohdr %s\n", GetModelName( handle ) );

	// allocate cache space
	MemAlloc_PushAllocDbgInfo( "Models:StudioHdr", 0);
	studiohdr_t *pHdr = (studiohdr_t *)AllocData( MDLCACHE_STUDIOHDR, pStudioHdrIn->length );
	MemAlloc_PopAllocDbgInfo();
	if ( !pHdr )
		return NULL;

	CacheData( &m_MDLDict[handle]->m_MDLCache, pHdr, pStudioHdrIn->length, GetModelName( handle ), MDLCACHE_STUDIOHDR, MakeCacheID( handle, MDLCACHE_STUDIOHDR) );

	if ( mod_lock_mdls_on_load.GetBool() )
	{
		m_MDLDict[handle]->m_pForceLockedStudioHdr = (studiohdr_t *)GetCacheSection( MDLCACHE_STUDIOHDR )->Lock( m_MDLDict[handle]->m_MDLCache );
	}

	// FIXME: Is there any way we can compute the size to load *before* loading in
	// and read directly into cache memory? It would be nice to reduce cache overhead here.
	// move the complete, relocatable model to the cache
	memcpy( pHdr, pStudioHdrIn, pStudioHdrIn->length );

	// On first load, convert the flex deltas from fp16 to 16-bit fixed-point
	if ( (pHdr->flags & STUDIOHDR_FLAGS_FLEXES_CONVERTED) == 0 )
	{
		ConvertFlexData( pHdr );

		// Mark as converted so it only happens once
		pHdr->flags |= STUDIOHDR_FLAGS_FLEXES_CONVERTED;
	}

	if ( m_pCacheNotify )
	{
		m_pCacheNotify->OnDataLoaded( MDLCACHE_STUDIOHDR, handle );
	}

	return pHdr;
}


//-----------------------------------------------------------------------------
// Attempts to load a MDL file, validates that it's ok.
//-----------------------------------------------------------------------------
bool CMDLCache::ReadMDLFile( MDLHandle_t handle, const char *pMDLFileName, CMDLCacheData &cacheData )
{
	VPROF( "CMDLCache::ReadMDLFile" );

	char pFileName[ MAX_PATH ];
	Q_strncpy( pFileName, pMDLFileName, sizeof( pFileName ) );
	Q_FixSlashes( pFileName );
#ifdef POSIX
	Q_strlower( pFileName );
#endif

	MdlCacheMsg( "MDLCache: Load studiohdr %s\n", pFileName );

	MEM_ALLOC_CREDIT();

	bool bOk = cacheData.ReadFileNative( pFileName, "GAME" );
	if ( !bOk || !cacheData.Data() )
	{
		DevWarning( "Failed to load %s!\n", pMDLFileName );
		return false;
	}

	if ( cacheData.DataSize() < sizeof(studiohdr_t) )
	{
		DevWarning( "Empty model %s\n", pMDLFileName );
		return false;
	}

	studiohdr_t *pStudioHdr = (studiohdr_t*)cacheData.Data();
	if ( pStudioHdr->id != IDSTUDIOHEADER )
	{
		DevWarning( "Model %s not a .MDL format file!\n", pMDLFileName );
		return false;
	}

	// We need this, but older file formats don't have it.

	if ( pStudioHdr->studiohdr2index == 0 )
	{
		DevWarning( "Model %s doesn't have a studiohdr2, which should've been fixed. This is required of all models now.\n", pMDLFileName );
		return false;
	}

	// critical! store a back link to our data
	// this is fetched when re-establishing dependent cached data (vtx/vvd)
	pStudioHdr->SetVirtualModel( MDLHandleToVirtual( handle ) );

	static ConVarRef developer( "developer" );

	// Make sure all dependent files are valid (blocking loads and checks them in developer mode 2)
	if ( ( developer.IsValid() && developer.GetInt() >= 2 ) && 
		!VerifyHeaders( pStudioHdr ) )
	{
		DevWarning( "Model %s has mismatched .vvd + .vtx files!\n", pMDLFileName );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
studiohdr_t *CMDLCache::LockStudioHdr( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
	{
		return NULL;
	}

	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( pStudioData->m_pForceLockedStudioHdr )
	{
		++pStudioData->m_iStudioHdrVirtualLock;
		return pStudioData->m_pForceLockedStudioHdr;
	}

	CMDLCacheCriticalSection cacheCriticalSection( this );
	studiohdr_t *pStdioHdr = GetStudioHdr( handle );
	// @TODO (toml 9/12/2006) need this?: AddRef( handle );
	if ( !pStdioHdr )
	{
		return NULL;
	}

	if ( pStudioData->m_pForceLockedStudioHdr )
	{
		++pStudioData->m_iStudioHdrVirtualLock;
		return pStudioData->m_pForceLockedStudioHdr;
	}

	GetCacheSection( MDLCACHE_STUDIOHDR )->Lock( m_MDLDict[handle]->m_MDLCache );

	return pStdioHdr;
}

void CMDLCache::UnlockStudioHdr( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
	{
		return;
	}

	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( pStudioData->m_pForceLockedStudioHdr )
	{
		--pStudioData->m_iStudioHdrVirtualLock;
		return;
	}

	if ( pStudioData->m_MDLCache != DC_INVALID_HANDLE )
	{
		GetCacheSection( MDLCACHE_STUDIOHDR )->Unlock( m_MDLDict[handle]->m_MDLCache );
	}
	// @TODO (toml 9/12/2006) need this?: Release( handle );
}

//-----------------------------------------------------------------------------
// Loading the data in
//-----------------------------------------------------------------------------
studiohdr_t *CMDLCache::GetStudioHdr( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return NULL;

	studiodata_t *pStudioData = m_MDLDict[handle];

	if( !pStudioData )
	{
		// <sergiy> this happens on quit during map load. Safeguarding to prevent immediate crash, as the cause is unknown.
		Warning(
			"-------------------------------------------------------------------------------\n"
			"CMDLCache::GetStudioHdr(0x%X) : invalid handle, not in dictionary (of size %u).\n"
			"-------------------------------------------------------------------------------\n",
			(int)handle, m_MDLDict.Count() );
		return NULL;
	}

	if ( pStudioData->m_pForceLockedStudioHdr )
	{
		return pStudioData->m_pForceLockedStudioHdr;
	}

	if ( mod_lock_mdls_on_load.GetBool() )
	{
		pStudioData->m_ForceLockMutex.Lock();

		if ( pStudioData->m_pForceLockedStudioHdr )
		{
			pStudioData->m_ForceLockMutex.Unlock();
			return pStudioData->m_pForceLockedStudioHdr;
		}
	}

	// Returning a pointer to data inside the cache when it's unlocked is just a bad idea.
	// It's technically legal, but the pointer can get invalidated if anything else looks at the cache.
	// Don't do that.
	// Assert( m_pModelCacheSection->IsFrameLocking() );
	// Assert( m_pMeshCacheSection->IsFrameLocking() );

#if _DEBUG
	VPROF_INCREMENT_COUNTER( "GetStudioHdr", 1 );
#endif
	studiohdr_t *pHdr = (studiohdr_t*)CheckData( m_MDLDict[handle]->m_MDLCache, MDLCACHE_STUDIOHDR );
	if ( !pHdr )
	{
		m_MDLDict[handle]->m_MDLCache = NULL;

		CMDLCacheCriticalSection cacheCriticalSection( this );

		static ConVarRef developer( "developer" );

		// load the file
		const char *pModelName = GetActualModelName( handle );
		if ( developer.IsValid() && developer.GetInt() > 1 )
		{
			DevMsg( "Loading %s\n", pModelName );
		}

		// Load file to temporary space
		CMDLCacheData cacheData( MDLCACHE_STUDIOHDR, CMDLCacheData::ALLOC_MALLOC );
		if ( !ReadMDLFile( handle, pModelName, cacheData ) )
		{
			bool bOk = false;
			if ( ( m_MDLDict[handle]->m_nFlags & STUDIODATA_ERROR_MODEL ) == 0 )
			{
				m_MDLDict[handle]->m_nFlags |= STUDIODATA_ERROR_MODEL;
				bOk = ReadMDLFile( handle, ERROR_MODEL, cacheData );
			}

			if ( !bOk )
			{
				if (IsOSX())
				{
					// rbarris wants this to go somewhere like the console.log prior to crashing, which is what the Error call will do next
					printf("\n ##### Model %s not found and %s couldn't be loaded", pModelName, ERROR_MODEL );
					fflush( stdout );
				}
				
				Error( "Model %s not found and %s couldn't be loaded", pModelName, ERROR_MODEL );
				if ( mod_lock_mdls_on_load.GetBool() )
				{
					pStudioData->m_ForceLockMutex.Unlock();
				}
				return NULL;
			}
		}

		// put it in the cache
		if ( ProcessDataIntoCache( handle, cacheData ) )
		{
			pHdr = (studiohdr_t*)CheckData( m_MDLDict[handle]->m_MDLCache, MDLCACHE_STUDIOHDR );
		}
	}
	else
	{
		if ( mod_lock_mdls_on_load.GetBool() )
		{
			GetCacheSection( MDLCACHE_STUDIOHDR )->Lock( m_MDLDict[handle]->m_MDLCache ); // add an explicit lock
			m_MDLDict[handle]->m_pForceLockedStudioHdr = pHdr;
		}
	}


	if ( mod_lock_mdls_on_load.GetBool() )
	{
		pStudioData->m_ForceLockMutex.Unlock();
	}

	return pHdr;
}


//-----------------------------------------------------------------------------
// Gets/sets user data associated with the MDL
//-----------------------------------------------------------------------------
void CMDLCache::SetUserData( MDLHandle_t handle, void* pData )
{
	if ( handle == MDLHANDLE_INVALID )
		return;

	m_MDLDict[handle]->m_pUserData = pData;
}

void *CMDLCache::GetUserData( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return NULL;
	return m_MDLDict[handle]->m_pUserData;
}


//-----------------------------------------------------------------------------
// Polls information about a particular mdl
//-----------------------------------------------------------------------------
bool CMDLCache::IsErrorModel( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return false;

	return (m_MDLDict[handle]->m_nFlags & STUDIODATA_ERROR_MODEL) != 0;
}


bool CMDLCache::IsOverBudget( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
	{
		return false;
	}

	studiohdr_t *pStdioHdr = GetStudioHdr( handle );

	return ( ( pStdioHdr->flags & STUDIOHDR_FLAGS_OVER_BUDGET ) != 0 );
}


//-----------------------------------------------------------------------------
// Brings all data associated with an MDL into memory
//-----------------------------------------------------------------------------
void CMDLCache::TouchAllData( MDLHandle_t handle )
{
	studiohdr_t *pStudioHdr = GetStudioHdr( handle );
	virtualmodel_t *pVModel = GetVirtualModel( handle );
	if ( pVModel )
	{
		// skip self, start at children
		// ensure all sub models are cached
		for ( int i=1; i<pVModel->m_group.Count(); ++i )
		{
			MDLHandle_t childHandle = VoidPtrToMDLHandle( pVModel->m_group[i].cache );
			if ( childHandle != MDLHANDLE_INVALID )
			{
				// FIXME: Should this be calling TouchAllData on the child?
				GetStudioHdr( childHandle );
			}
		}
	}

	if ( !IsGameConsole() )
	{
		// cache the anims
		// Note that the animblocks start at 1!!!
		for ( int i=1; i< (int)pStudioHdr->numanimblocks; ++i )
		{
			pStudioHdr->GetAnimBlock( i );
		}
	}

	// cache the vertexes
	if ( pStudioHdr->numbodyparts )
	{
		if ( !mod_dont_load_vertices.GetInt() )
		{
			CacheVertexData( pStudioHdr );
			GetHardwareData( handle );
		}
	}
}


//-----------------------------------------------------------------------------
// Flushes all data
//-----------------------------------------------------------------------------
void CMDLCache::Flush( MDLCacheFlush_t nFlushFlags )
{
	// Free all MDLs that haven't been cleaned up
	MDLHandle_t i = m_MDLDict.First();
	while ( i != m_MDLDict.InvalidIndex() )
	{
		Flush( i, nFlushFlags );
		i = m_MDLDict.Next( i );
	}
}

//-----------------------------------------------------------------------------
// Cache handlers
//-----------------------------------------------------------------------------
static const char *g_ppszTypes[] =
{
	"studiohdr",		// MDLCACHE_STUDIOHDR
	"studiohwdata",		// MDLCACHE_STUDIOHWDATA
	"vcollide",			// MDLCACHE_VCOLLIDE
	"animblock",		// MDLCACHE_ANIMBLOCK
	"virtualmodel",		// MDLCACHE_VIRTUALMODEL
	"vertexes",			// MDLCACHE_VERTEXES
	"decodedanim",		// MDLCACHE_DECODEDANIMBLOCK
};

bool CMDLCache::HandleCacheNotification( const DataCacheNotification_t &notification  )
{
	switch ( notification.type )
	{
	case DC_AGE_DISCARD:
	case DC_FLUSH_DISCARD:
	case DC_REMOVED:
		{
			// This message can cause a crash on debug builds with "mod_trace_load 1"
			MdlCacheMsg( "MDLCache: Data cache discard %s %s\n", g_ppszTypes[TypeFromCacheID( notification.clientId )], GetModelName( HandleFromCacheID( notification.clientId ) ) );

			if ( (DataCacheClientID_t)notification.pItemData == notification.clientId ||
				 TypeFromCacheID(notification.clientId) != MDLCACHE_STUDIOHWDATA )
			{
				Assert( notification.pItemData );
				FreeData( TypeFromCacheID(notification.clientId), (void *)notification.pItemData );
			}
			else
			{
				UnloadHardwareData( m_MDLDict[ HandleFromCacheID( notification.clientId ) ] );
			}
			return true;
		}
	}

	return CDefaultDataCacheClient::HandleCacheNotification( notification );
}

bool CMDLCache::GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen  )
{
	if ( (DataCacheClientID_t)pItem == clientId )
	{
		return false;
	}

	MDLHandle_t handle = HandleFromCacheID( clientId );
	MDLCacheDataType_t type = TypeFromCacheID( clientId );

	Assert( type >= 0 && type < ARRAYSIZE(g_ppszTypes) );
	Q_snprintf( pDest, nMaxLen, "%s - %s", g_ppszTypes[type], GetModelName( handle ) );

	return false;
}


//-----------------------------------------------------------------------------
// Flushes all data
//-----------------------------------------------------------------------------
void CMDLCache::BeginCoarseLock()
{
	if ( IsGameConsole() )
	{
		m_pModelCacheSection->BeginFrameLocking();
		m_pMeshCacheSection->BeginFrameLocking();
	}
}

//-----------------------------------------------------------------------------
// Flushes all data
//-----------------------------------------------------------------------------
void CMDLCache::EndCoarseLock()
{
	if ( IsGameConsole() )
	{
		m_pModelCacheSection->EndFrameLocking();
		m_pMeshCacheSection->EndFrameLocking();
	}
}


//-----------------------------------------------------------------------------
// Flushes all data
//-----------------------------------------------------------------------------
void CMDLCache::BeginLock()
{
	if ( !IsGameConsole() )
	{
		m_pModelCacheSection->BeginFrameLocking();
		m_pMeshCacheSection->BeginFrameLocking();
	}
	m_pAnimBlocksCacheSection->BeginFrameLocking();
}

//-----------------------------------------------------------------------------
// Flushes all data
//-----------------------------------------------------------------------------
void CMDLCache::EndLock()
{
	if ( !IsGameConsole() )
	{
		m_pModelCacheSection->EndFrameLocking();
		m_pMeshCacheSection->EndFrameLocking();
	}
	m_pAnimBlocksCacheSection->EndFrameLocking();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMDLCache::BreakFrameLock( bool bModels, bool bMesh, bool bAnimBlock )
{
	if ( bModels )
	{
		if ( m_pModelCacheSection->IsFrameLocking() )
		{
			Assert( !m_nModelCacheFrameLocks );
			m_nModelCacheFrameLocks = 0;
			do
			{
				m_nModelCacheFrameLocks++;
			} while ( m_pModelCacheSection->EndFrameLocking() );
		}

	}

	if ( bMesh )
	{
		if ( m_pMeshCacheSection->IsFrameLocking() )
		{
			Assert( !m_nMeshCacheFrameLocks );
			m_nMeshCacheFrameLocks = 0;
			do
			{
				m_nMeshCacheFrameLocks++;
			} while ( m_pMeshCacheSection->EndFrameLocking() );
		}
	}

	if ( bAnimBlock )
	{
		if ( m_pAnimBlocksCacheSection->IsFrameLocking() )
		{
			Assert( !m_nAnimBlockCacheFrameLocks );
			m_nAnimBlockCacheFrameLocks = 0;
			do
			{
				m_nAnimBlockCacheFrameLocks++;
			} while ( m_pAnimBlocksCacheSection->EndFrameLocking() );
		}
	}
}

void CMDLCache::RestoreFrameLock()
{
	while ( m_nModelCacheFrameLocks )
	{
		m_pModelCacheSection->BeginFrameLocking();
		m_nModelCacheFrameLocks--;
	}
	while ( m_nMeshCacheFrameLocks )
	{
		m_pMeshCacheSection->BeginFrameLocking();
		m_nMeshCacheFrameLocks--;
	}
	while ( m_nAnimBlockCacheFrameLocks )
	{
		m_pAnimBlocksCacheSection->BeginFrameLocking();
		m_nAnimBlockCacheFrameLocks--;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int *CMDLCache::GetFrameUnlockCounterPtrOLD()
{
	return GetCacheSection( MDLCACHE_STUDIOHDR )->GetFrameUnlockCounterPtr();
}

int *CMDLCache::GetFrameUnlockCounterPtr( MDLCacheDataType_t type )
{
	return GetCacheSection( type )->GetFrameUnlockCounterPtr();
}

//-----------------------------------------------------------------------------
// Completes all pending async operations
//-----------------------------------------------------------------------------
void CMDLCache::FinishPendingLoads()
{
	if ( !ThreadInMainThread() )
	{
		return;
	}

	AUTO_LOCK_FM( m_AsyncMutex );

	// finish just our known jobs
	intp iAsync = m_PendingAsyncs.Head();
	while ( iAsync != m_PendingAsyncs.InvalidIndex() )
	{
		AsyncInfo_t &info = m_PendingAsyncs[iAsync];
		if ( info.hControl )
		{
			g_pFullFileSystem->AsyncFinish( info.hControl, true );
		}
		iAsync = m_PendingAsyncs.Next( iAsync );
	}

	ProcessPendingAsyncs();
}

//-----------------------------------------------------------------------------
// Notify map load has started
//-----------------------------------------------------------------------------
void CMDLCache::BeginMapLoad()
{
	BreakFrameLock();

	studiodata_t *pStudioData;

	m_ModelSwapper.LatchEffectiveGPULevel();

	// Unlock prior map MDLs prior to load
	MDLHandle_t i = m_MDLDict.First();
	while ( i != m_MDLDict.InvalidIndex() )
	{
		pStudioData = m_MDLDict[i];
		if ( pStudioData->m_pForceLockedStudioHdr )
		{
			// Reset the lock counts to where they need to be while the mdlcache lock is not active
			while ( pStudioData->m_iStudioHdrVirtualLock > 0 )
			{
				--pStudioData->m_iStudioHdrVirtualLock;
				GetCacheSection( MDLCACHE_STUDIOHDR )->Lock( pStudioData->m_MDLCache );
			}

			while ( pStudioData->m_iStudioHdrVirtualLock < 0 )
			{
				++pStudioData->m_iStudioHdrVirtualLock;
				GetCacheSection( MDLCACHE_STUDIOHDR )->Unlock( pStudioData->m_MDLCache );
			}

			GetCacheSection( MDLCACHE_STUDIOHDR )->Unlock( pStudioData->m_MDLCache );
			pStudioData->m_pForceLockedStudioHdr = NULL;
		}
		if ( pStudioData->m_pForceLockedVertexFileHeader )
		{
			GetCacheSection( MDLCACHE_VERTEXES )->Unlock( pStudioData->m_VertexCache );
			pStudioData->m_pForceLockedVertexFileHeader = NULL;
		}
		i = m_MDLDict.Next( i );
	}
}

//-----------------------------------------------------------------------------
// Notify map load is complete
//-----------------------------------------------------------------------------
void CMDLCache::EndMapLoad()
{
	FinishPendingLoads();

	bool bLockMdls = mod_lock_mdls_on_load.GetBool();
	bool bLockMeshes = ( !mod_load_mesh_async.GetBool() && mod_lock_meshes_on_load.GetBool() );

	// Remove all stray MDLs not referenced during load
	if ( bLockMdls || bLockMeshes )
	{
		studiodata_t *pStudioData;
		MDLHandle_t i = m_MDLDict.First();
		while ( i != m_MDLDict.InvalidIndex() )
		{
			pStudioData = m_MDLDict[i];
			if ( bLockMdls && !m_MDLDict[i]->m_pForceLockedStudioHdr )
			{
				Flush( i, MDLCACHE_FLUSH_STUDIOHDR | MDLCACHE_FLUSH_COMBINED_DATA );
			}
			if ( bLockMeshes && !( m_MDLDict[i]->m_nFlags & STUDIODATA_FLAGS_NO_VERTEX_DATA ) && !m_MDLDict[i]->m_pForceLockedVertexFileHeader )
			{
				Flush( i, MDLCACHE_FLUSH_VERTEXES );
			}
			i = m_MDLDict.Next( i );
		}
	}

	RestoreFrameLock();
}


//-----------------------------------------------------------------------------
// Is a particular part of the model data loaded?
//-----------------------------------------------------------------------------
bool CMDLCache::IsDataLoaded( MDLHandle_t handle, MDLCacheDataType_t type )
{
	if ( handle == MDLHANDLE_INVALID || !m_MDLDict.IsValidIndex( handle ) )
		return false;

	studiodata_t *pData = m_MDLDict[ handle ];
	switch( type )
	{
	case MDLCACHE_STUDIOHDR:
		return GetCacheSection( MDLCACHE_STUDIOHDR )->IsPresent( pData->m_MDLCache );

	case MDLCACHE_STUDIOHWDATA:
		return ( pData->m_nFlags & STUDIODATA_FLAGS_STUDIOMESH_LOADED ) != 0;

	case MDLCACHE_VCOLLIDE:
		return ( pData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED ) != 0;

	case MDLCACHE_ANIMBLOCK:
		{
			if ( !pData->m_vecAnimBlocks.Count() )
				return false;

			for (int i = 0; i < pData->m_vecAnimBlocks.Count(); ++i )
			{
				if ( !pData->m_vecAnimBlocks[i] )
					return false;

				if ( !GetCacheSection( type )->IsPresent( pData->m_vecAnimBlocks[i] ) )
					return false;
			}
			return true;
		}

	case MDLCACHE_VIRTUALMODEL:
		return ( pData->m_pVirtualModel != 0 );

	case MDLCACHE_VERTEXES:
		return m_pMeshCacheSection->IsPresent( pData->m_VertexCache );
	}
	return false;
}


//-----------------------------------------------------------------------------
// Get the correct extension for our dx
//-----------------------------------------------------------------------------
const char *CMDLCache::GetVTXExtension()
{
	return ".dx90.vtx";
}

//-----------------------------------------------------------------------------
// Minimal presence and header validation, no data loads
// Return true if successful, false otherwise.
//-----------------------------------------------------------------------------
bool CMDLCache::VerifyHeaders( studiohdr_t *pStudioHdr )
{
	VPROF( "CMDLCache::VerifyHeaders" );

	// model has no vertex data
	if ( !pStudioHdr->numbodyparts )
	{
		// valid
		return true;
	}

	char pFileName[ MAX_PATH ];

	MakeFilename( pFileName, pStudioHdr, ".vvd" );

	MdlCacheMsg("MDLCache: Load VVD (verify) %s\n", pFileName );

	// vvd header only
	CUtlBuffer vvdHeader( 0, sizeof(vertexFileHeader_t) );
	if ( !ReadFileNative( pFileName, "GAME", vvdHeader, sizeof(vertexFileHeader_t) ) )
	{
		return false;
	}

	vertexFileHeader_t *pVertexHdr = (vertexFileHeader_t*)vvdHeader.PeekGet();

	// check
	if (( pVertexHdr->id != MODEL_VERTEX_FILE_ID ) ||
		( pVertexHdr->version != MODEL_VERTEX_FILE_VERSION ) ||
		( pVertexHdr->checksum != pStudioHdr->checksum ))
	{
		return false;
	}

	// load the VTX file
	// use model name for correct path
	MakeFilename( pFileName, pStudioHdr, GetVTXExtension() );

	MdlCacheMsg("MDLCache: Load VTX (verify) %s\n", pFileName );

	// vtx header only
	CUtlBuffer vtxHeader( 0, sizeof(OptimizedModel::FileHeader_t) );
	if ( !ReadFileNative( pFileName, "GAME", vtxHeader, sizeof(OptimizedModel::FileHeader_t) ) )
	{
		return false;
	}

	// check
	OptimizedModel::FileHeader_t *pVtxHdr = (OptimizedModel::FileHeader_t*)vtxHeader.PeekGet();
	if (( pVtxHdr->version != OPTIMIZED_MODEL_FILE_VERSION ) ||
		( pVtxHdr->checkSum != pStudioHdr->checksum ))
	{
		return false;
	}

	// valid
	return true;
}


//-----------------------------------------------------------------------------
// Cache model's specified dynamic data
//-----------------------------------------------------------------------------
vertexFileHeader_t *CMDLCache::CacheVertexData( studiohdr_t *pStudioHdr )
{
	VPROF( "CMDLCache::CacheVertexData" );

	vertexFileHeader_t	*pVvdHdr;
	MDLHandle_t			handle;

	Assert( pStudioHdr );

	handle = VoidPtrToMDLHandle( pStudioHdr->VirtualModel() );
	Assert( handle != MDLHANDLE_INVALID );

	if ( m_MDLDict[handle]->m_pForceLockedVertexFileHeader )
	{
		return m_MDLDict[handle]->m_pForceLockedVertexFileHeader;
	}

	pVvdHdr = (vertexFileHeader_t *)CheckData( m_MDLDict[handle]->m_VertexCache, MDLCACHE_VERTEXES );
	if ( pVvdHdr )
	{
		return pVvdHdr;
	}

	m_MDLDict[handle]->m_VertexCache = NULL;

	return LoadVertexData( pStudioHdr );
}

//-----------------------------------------------------------------------------
// Start an async transfer
//-----------------------------------------------------------------------------
FSAsyncStatus_t CMDLCache::LoadData( const char *pszFilename, const char *pszPathID, void *pDest, int nBytes, int nOffset, bool bAsync, FSAsyncControl_t *pControl, MDLHandle_t hModel )
{
	if ( !*pControl )
	{
		if ( IsGameConsole() && g_pQueuedLoader->IsMapLoading() )
		{
			// the weapon model cache explicitly bypasses the QL causing beingin warnings
			// per request, these need to get suppressed from the log which is causing undesired noise
			if ( !m_pCacheNotify || !m_pCacheNotify->ShouldSupressLoadWarning( hModel ) )
			{
				DevWarning( "CMDLCache: Non-Optimal loading path for %s\n", pszFilename );
			}
		}

		const char *pActualFilename = pszFilename;
		if ( IsPC() )
		{
			pActualFilename = m_ModelSwapper.TranslateModelName( pszFilename );
		}
		
		FileAsyncRequest_t asyncRequest;
		asyncRequest.pszFilename = pActualFilename;
		asyncRequest.pszPathID = pszPathID;
		asyncRequest.pData = pDest;
		asyncRequest.nBytes = nBytes;
		asyncRequest.nOffset = nOffset;

		if ( !pDest )
		{
			asyncRequest.flags = FSASYNC_FLAGS_ALLOCNOFREE;
		}

		if ( !bAsync )
		{
			asyncRequest.flags |= FSASYNC_FLAGS_SYNC;
		}

		MEM_ALLOC_CREDIT();
		return g_pFullFileSystem->AsyncRead( asyncRequest, pControl );
	}

	return FSASYNC_ERR_FAILURE;
}

//-----------------------------------------------------------------------------
// Determine the maximum number of 'real' bone influences used by any vertex in a model
// (100% binding to bone zero doesn't count)
//-----------------------------------------------------------------------------
int ComputeMaxRealBoneInfluences( vertexFileHeader_t * vertexFile, int lod )
{
	const mstudiovertex_t * verts = vertexFile->GetVertexData();
	int numVerts = vertexFile->numLODVertexes[ lod ];
	Assert(verts);

	int maxWeights = 0;
	for (int i = 0;i < numVerts;i++)
	{
		if ( verts[i].m_BoneWeights.numbones > 0 )
		{
			int numWeights = 0;
			for (int j = 0;j < MAX_NUM_BONES_PER_VERT;j++)
			{
				if ( verts[i].m_BoneWeights.weight[j] > 0 )
					numWeights = j + 1;
			}
			if ( ( numWeights == 1 ) && ( verts[i].m_BoneWeights.bone[0] == 0 ) )
			{
				// 100% binding to first bone - not really skinned (the first bone is just the model transform)
				numWeights = 0;
			}
			maxWeights = MAX( numWeights, maxWeights );
		}
	}
	return maxWeights;
}

//-----------------------------------------------------------------------------
// Generate thin vertices (containing just the data needed to do model decals)
//-----------------------------------------------------------------------------
vertexFileHeader_t * CMDLCache::CreateThinVertexes( vertexFileHeader_t * originalData, const studiohdr_t * pStudioHdr, int * cacheLength )
{
	int rootLod = MIN( pStudioHdr->rootLOD, ( originalData->numLODs - 1 ) );
	Assert( rootLod >= 0 && rootLod < ARRAYSIZE(originalData->numLODVertexes) );
	int numVerts = originalData->numLODVertexes[ rootLod ] + 1; // Add 1 vert to support prefetch during array access

	int numBoneInfluences = ComputeMaxRealBoneInfluences( originalData, rootLod );
	// Only store (N-1) weights (all N weights sum to 1, so we can re-compute the Nth weight later)
	int numStoredWeights = MAX( 0, ( numBoneInfluences - 1 ) );

	int vertexSize = 2*sizeof( Vector ) + numBoneInfluences*sizeof( unsigned char ) + numStoredWeights*sizeof( float );
	*cacheLength = sizeof( vertexFileHeader_t ) + sizeof( thinModelVertices_t ) + numVerts*vertexSize;

	// Allocate cache space for the thin data
	MemAlloc_PushAllocDbgInfo( "Models:Vertex data", 0);
	vertexFileHeader_t * pNewVvdHdr = (vertexFileHeader_t *)AllocData( MDLCACHE_VERTEXES, *cacheLength );
	MemAlloc_PopAllocDbgInfo();

	Assert( pNewVvdHdr );
	if ( pNewVvdHdr )
	{
		// Copy the header and set it up to hold thin vertex data
		memcpy( (void *)pNewVvdHdr, (void *)originalData, sizeof( vertexFileHeader_t ) );
		pNewVvdHdr->id					= MODEL_VERTEX_FILE_THIN_ID;
		pNewVvdHdr->numFixups			= 0;
		pNewVvdHdr->fixupTableStart		= 0;
		pNewVvdHdr->tangentDataStart	= 0;
		pNewVvdHdr->vertexDataStart		= sizeof( vertexFileHeader_t );

		// Set up the thin vertex structure
 		thinModelVertices_t	* pNewThinVerts = (thinModelVertices_t	*)( pNewVvdHdr		+ 1 );
		Vector				* pPositions	= (Vector				*)( pNewThinVerts	+ 1 );
		float				* pBoneWeights	= (float				*)( pPositions		+ numVerts );
		// Alloc the (short) normals here to avoid mis-aligning the float data
		unsigned short		* pNormals		= (unsigned short		*)( pBoneWeights	+ numVerts*numStoredWeights );
		// Alloc the (char) indices here to avoid mis-aligning the float/short data
		byte				* pBoneIndices	= (byte					*)( pNormals		+ numVerts );
		if ( numStoredWeights == 0 )
			pBoneWeights = NULL;
		if ( numBoneInfluences == 0 )
			pBoneIndices = NULL;
		pNewThinVerts->Init( numBoneInfluences, pPositions, pNormals, pBoneWeights, pBoneIndices );

		// Copy over the original data
		const mstudiovertex_t * srcVertexData = originalData->GetVertexData();
		for ( int i = 0; i < numVerts; i++ )
		{
			pNewThinVerts->SetPosition( i, srcVertexData[ i ].m_vecPosition );
			pNewThinVerts->SetNormal(   i, srcVertexData[ i ].m_vecNormal );
			if ( numBoneInfluences > 0 )
			{
				mstudioboneweight_t boneWeights;
				boneWeights.numbones = numBoneInfluences;
				for ( int j = 0; j < numStoredWeights; j++ )
				{
					boneWeights.weight[ j ] = srcVertexData[ i ].m_BoneWeights.weight[ j ];
				}
				for ( int j = 0; j < numBoneInfluences; j++ )
				{
					boneWeights.bone[ j ] = srcVertexData[ i ].m_BoneWeights.bone[ j ];
				}
				pNewThinVerts->SetBoneWeights( i, boneWeights );
			}
		}
	}

	return pNewVvdHdr;
}

//-----------------------------------------------------------------------------
// Generate null vertices (containing no data - just a header to say verts have been loaded, converted into VBs/IBs and discarded)
//-----------------------------------------------------------------------------
vertexFileHeader_t * CMDLCache::CreateNullVertexes( vertexFileHeader_t * originalData, const studiohdr_t * pStudioHdr, int * cacheLength )
{
	// Allocate cache space for the thin data
	*cacheLength = sizeof( vertexFileHeader_t );
	MemAlloc_PushAllocDbgInfo( "Models:Vertex data", 0);
	vertexFileHeader_t * pNewVvdHdr = (vertexFileHeader_t *)AllocData( MDLCACHE_VERTEXES, *cacheLength );
	MemAlloc_PopAllocDbgInfo();

	Assert( pNewVvdHdr );
	if ( pNewVvdHdr )
	{
		// Copy the header and blank out any references to data - which will now be discarded
		memcpy( (void *)pNewVvdHdr, (void *)originalData, sizeof( vertexFileHeader_t ) );
		pNewVvdHdr->id					= MODEL_VERTEX_FILE_NULL_ID;
		pNewVvdHdr->numFixups			= 0;
		pNewVvdHdr->fixupTableStart		= 0;
		pNewVvdHdr->tangentDataStart	= 0;
		pNewVvdHdr->vertexDataStart		= 0;
	}

	return pNewVvdHdr;
}

//-----------------------------------------------------------------------------
// Process the provided raw data into the cache. Distributes to low level
// unserialization or build methods.
//-----------------------------------------------------------------------------
bool CMDLCache::ProcessDataIntoCache( MDLHandle_t handle, CMDLCacheData &cacheData, int iAnimBlock )
{
	studiohdr_t *pStudioHdrCurrent = NULL;
	if ( cacheData.DataType() != MDLCACHE_STUDIOHDR )
	{
		// can only get the studiohdr once the header has been processed successfully into the cache
		// causes a ProcessDataIntoCache() with the studiohdr data
		pStudioHdrCurrent = GetStudioHdr( handle );
		if ( !pStudioHdrCurrent )
		{
			return false;
		}
	}

	studiodata_t *pStudioDataCurrent = m_MDLDict[handle];

	if ( !pStudioDataCurrent )
	{
		return false;
	}

	switch ( cacheData.DataType() )
	{
	case MDLCACHE_STUDIOHDR:
		{
			pStudioHdrCurrent = UnserializeMDL( handle, cacheData );
			if ( !pStudioHdrCurrent )
			{
				return false;
			}

			if (!Studio_ConvertStudioHdrToNewVersion( pStudioHdrCurrent ))
			{
				Warning( "MDLCache: %s needs to be recompiled\n", pStudioHdrCurrent->pszName() );
			}

			if ( pStudioHdrCurrent->numincludemodels == 0 )
			{
				// perf optimization, calculate once and cache off the autoplay sequences
				int nCount = pStudioHdrCurrent->CountAutoplaySequences();
				if ( nCount )
				{
					AllocateAutoplaySequences( m_MDLDict[handle], nCount );
					pStudioHdrCurrent->CopyAutoplaySequences( m_MDLDict[handle]->m_vecAutoplaySequenceList.Base(), nCount );
				}
			}

			// Load animations
			UnserializeAllVirtualModelsAndAnimBlocks( handle );
			break;
		}

	case MDLCACHE_VERTEXES:
		{
			if ( cacheData.Data() )
			{
				BuildAndCacheVertexData( pStudioHdrCurrent, cacheData );
			}
			else
			{
				pStudioDataCurrent->m_nFlags |= STUDIODATA_FLAGS_NO_VERTEX_DATA;
				if ( pStudioHdrCurrent->numbodyparts )
				{
					// expected data not valid
					Warning( "MDLCache: Failed load of .VVD data for %s\n", pStudioHdrCurrent->pszName() );
					return false;
				}
			}
			break;
		}

	case MDLCACHE_STUDIOHWDATA:
		{
			if ( cacheData.Data() )
			{
				BuildHardwareData( handle, pStudioDataCurrent, pStudioHdrCurrent, cacheData );
			}
			else
			{
				pStudioDataCurrent->m_nFlags |= STUDIODATA_FLAGS_NO_STUDIOMESH;
				if ( pStudioHdrCurrent->numbodyparts )
				{
					// expected data not valid
					Warning( "MDLCache: Failed load of .VTX data for %s\n", pStudioHdrCurrent->pszName() );
					return false;
				}
			}

			m_pMeshCacheSection->Unlock( pStudioDataCurrent->m_VertexCache );
			m_pMeshCacheSection->Age( pStudioDataCurrent->m_VertexCache );

			if ( !( pStudioDataCurrent->m_nFlags & STUDIODATA_FLAGS_NO_STUDIOMESH ) )
			{
				vertexFileHeader_t *originalVertexData = GetVertexData( handle );
				Assert( originalVertexData );
				if ( originalVertexData && IsGameConsole() )
				{
					// PORTAL2 CONSOLE: Vertex/Index data will never be read again (no model decals or load-time lighting), so discard the VVD data and create a new header
					int nullVertexDataSize = 0;
					vertexFileHeader_t *nullVertexData = CreateNullVertexes( originalVertexData, pStudioHdrCurrent, &nullVertexDataSize );
					Assert( nullVertexData && ( nullVertexDataSize > 0 ) );
					if ( nullVertexData && ( nullVertexDataSize > 0 ) )
					{
						// Remove and free the original cache entry, and add the new one
						// This causes the aliased "forced" locked vertex pointer to be nulled
						// which trips MarkAsLoaded() to re-establish it during CL_FullyConnected(), thus the alias is maintained.
						Flush( handle, MDLCACHE_FLUSH_VERTEXES | MDLCACHE_FLUSH_IGNORELOCK );
						CacheData( &pStudioDataCurrent->m_VertexCache, nullVertexData, nullVertexDataSize, pStudioHdrCurrent->pszName(), MDLCACHE_VERTEXES, MakeCacheID( handle, MDLCACHE_VERTEXES) );
					}
				}
			}

			break;
		}

	case MDLCACHE_ANIMBLOCK:
		{
			MEM_ALLOC_CREDIT_( __FILE__ ": Anim Blocks" );

			if ( cacheData.Data() )
			{
				MdlCacheMsg( "MDLCache: Finish load anim block %s (block %i)\n", pStudioHdrCurrent->pszName(), iAnimBlock );

				char pCacheName[MAX_PATH];
				Q_snprintf( pCacheName, MAX_PATH, "%s (block %i)", pStudioHdrCurrent->pszName(), iAnimBlock );

				CacheData( &pStudioDataCurrent->m_vecAnimBlocks[iAnimBlock], cacheData.Data(), cacheData.DataSize(), pCacheName, MDLCACHE_ANIMBLOCK, MakeCacheID( handle, MDLCACHE_ANIMBLOCK) );

				// The cache now owns the data, so detach it from 'cacheData':
				cacheData.Detach();

#ifdef DEBUG_ANIM_STALLS
				if ( mod_load_showasync.GetBool() && pStudioDataCurrent->m_vecFirstRequest && pStudioHdrCurrent )
				{
					Msg("[%5.3f] async model load %s:%d\n", (Plat_MSTime() - pStudioDataCurrent->m_vecFirstRequest[iAnimBlock]) / 1000.0f, pStudioHdrCurrent->pszName(), iAnimBlock );
				}
#endif
			}
			else
			{
				MdlCacheMsg( "MDLCache: Failed load anim block %s (block %i)\n", pStudioHdrCurrent->pszName(), iAnimBlock );
				if ( pStudioDataCurrent->m_vecAnimBlocks.Count() > iAnimBlock )
				{
					pStudioDataCurrent->m_vecAnimBlocks[iAnimBlock] = NULL;
				}
				return false;
			}
			break;
		}

	case MDLCACHE_VCOLLIDE:
		{
			// always marked as loaded, vcollides are not present for every model
			pStudioDataCurrent->m_nFlags |= STUDIODATA_FLAGS_VCOLLISION_LOADED;

			if ( cacheData.Data() )
			{
				MdlCacheMsg( "MDLCache: Finish load vcollide for %s\n", pStudioHdrCurrent->pszName() );

				CUtlBuffer buf( cacheData.Data(), cacheData.DataSize(), CUtlBuffer::READ_ONLY );
				buf.SeekPut( CUtlBuffer::SEEK_HEAD, cacheData.DataSize() );

				phyheader_t header;
				buf.Get( &header, sizeof( phyheader_t ) );
				if ( ( header.size == sizeof( header ) ) && header.solidCount > 0 )
				{
					int nBufSize = buf.TellMaxPut() - buf.TellGet();
					pStudioDataCurrent->m_pVCollide = new CStudioVCollide;
					vcollide_t *pCollide = pStudioDataCurrent->m_pVCollide->GetVCollide();

					g_pPhysicsCollision->VCollideLoad( pCollide, header.solidCount, (const char*)buf.PeekGet(), nBufSize );
					if ( mod_check_vcollide.GetBool() )
					{
						g_pPhysicsCollision->VCollideCheck( pCollide, pStudioHdrCurrent->pszName() );
					}
					if ( m_pCacheNotify )
					{
						m_pCacheNotify->OnDataLoaded( MDLCACHE_VCOLLIDE, handle );
					}
				}
			}
			else
			{
				// Since it is legitimate to not have PHY data for a model, we only note this as a message and not a warning.
				MdlCacheMsg( "MDLCache: Failed load of .PHY data for %s\n", pStudioHdrCurrent->pszName() );
				return false;
			}
			break;
		}

	default:
		Assert( 0 );
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Returns:
//	<0: indeterminate at this time
//	=0: pending
//	>0:	completed
//-----------------------------------------------------------------------------
int CMDLCache::ProcessPendingAsync( intp iAsync )
{
	if ( !ThreadInMainThread() || iAsync == NO_ASYNC )
	{
		return -1;
	}

	ASSERT_NO_REENTRY();

	void *pData = NULL;
	int nBytesRead = 0;

	AsyncInfo_t *pInfo;
	{
		AUTO_LOCK_FM( m_AsyncMutex );
		pInfo = &m_PendingAsyncs[iAsync];
	}
	Assert( pInfo->hControl );

	FSAsyncStatus_t status = g_pFullFileSystem->AsyncGetResult( pInfo->hControl, &pData, &nBytesRead );
	if ( status == FSASYNC_STATUS_PENDING )
	{
		return 0;
	}

	AsyncInfo_t info = *pInfo;
	pInfo = &info;
	ClearAsync( pInfo->hModel, pInfo->type, pInfo->iAnimBlock );

	if( m_bFileNotFoundAllowed && status == FSASYNC_ERR_FILEOPEN )
	{
		// file not found here is valid so return complete
		return 1;
	}

	//Assert( nBytesRead > 0 );
	if ( nBytesRead <= 0 )
	{
		return 1;
	}

	switch ( pInfo->type )
	{
	case MDLCACHE_VERTEXES:
	case MDLCACHE_STUDIOHWDATA:
	case MDLCACHE_VCOLLIDE:
		{
			// NOTE: 'cacheData' deals with decompression/freeing of the incoming data
			CUtlBuffer buf( pData, nBytesRead, CUtlBuffer::READ_ONLY );
			CMDLCacheData cacheData( pInfo->type, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &buf );
			if ( status != FSASYNC_OK )
				cacheData.Purge();
			ProcessDataIntoCache( pInfo->hModel, cacheData );
		}
		break;

	case MDLCACHE_ANIMBLOCK:
		{
			// NOTES: - 'cacheData' deals with decompression/freeing of the incoming data
			//        - the cache assumes ownership of valid async'd data (invalid data gets freed)
			//        - see CMDLCache::UnserializeAnimBlock for how the incoming data was allocated (FreeAnimBlock will work in either case)
			CUtlBuffer buf( pData, nBytesRead, CUtlBuffer::READ_ONLY );
			CMDLCacheData cacheData( pInfo->type, CMDLCacheData::ALLOC_ANIMBLOCK, &buf );
			if ( status != FSASYNC_OK )
				cacheData.Purge();
			ProcessDataIntoCache( pInfo->hModel, cacheData, pInfo->iAnimBlock );
		}
		break;

	default:
		{
			Assert( 0 );
		}
		break;
	}

	return 1;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMDLCache::ProcessPendingAsyncs( MDLCacheDataType_t type )
{
	if ( !ThreadInMainThread() )
	{
		return;
	}

	if ( !m_PendingAsyncs.Count() )
	{
		return;
	}

	static bool bReentering;
	if ( bReentering )
	{
		return;
	}
	bReentering = true;

	AUTO_LOCK_FM( m_AsyncMutex );

	// Process all of the completed loads that were requested before a new one. This ensures two
	// things -- the LRU is in correct order, and it catches precached items lurking
	// in the async queue that have only been requested once (thus aren't being cached
	// and might lurk forever, e.g., wood gibs in the citadel)
	intp current = m_PendingAsyncs.Head();
	while ( current != m_PendingAsyncs.InvalidIndex() )
	{
		intp next = m_PendingAsyncs.Next( current );

		if ( type == MDLCACHE_NONE || m_PendingAsyncs[current].type == type )
		{
			// process, also removes from list
			if ( ProcessPendingAsync( current ) <= 0 )
			{
				// indeterminate or pending
				break;
			}
		}

		current = next;
	}

	bReentering = false;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CMDLCache::ClearAsync( MDLHandle_t handle, MDLCacheDataType_t type, int iAnimBlock, bool bAbort )
{
	intp iAsyncInfo = GetAsyncInfoIndex( handle, type, iAnimBlock );
	if ( iAsyncInfo != NO_ASYNC )
	{
		AsyncInfo_t *pInfo;
		{
			AUTO_LOCK_FM( m_AsyncMutex );
			pInfo = &m_PendingAsyncs[iAsyncInfo];
		}
		if ( pInfo->hControl )
		{
			if ( bAbort )
			{
				g_pFullFileSystem->AsyncAbort(  pInfo->hControl );
				void *pData;
				int ignored;
				if ( g_pFullFileSystem->AsyncGetResult(  pInfo->hControl, &pData, &ignored ) == FSASYNC_OK )
				{
					if ( type != MDLCACHE_ANIMBLOCK )
					{
						g_pFullFileSystem->FreeOptimalReadBuffer( pData );
					}
					else
					{
						FreeAnimBlock( pData );
					}
				}
			}
			g_pFullFileSystem->AsyncRelease(  pInfo->hControl );
			pInfo->hControl = NULL;
		}

		SetAsyncInfoIndex( handle, type, iAnimBlock, NO_ASYNC );
		{
			AUTO_LOCK_FM( m_AsyncMutex );
			m_PendingAsyncs.Remove( iAsyncInfo );
		}

		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMDLCache::GetAsyncLoad( MDLCacheDataType_t type )
{
	switch ( type )
	{
	case MDLCACHE_STUDIOHDR:
		return false;
	case MDLCACHE_STUDIOHWDATA:
		return mod_load_mesh_async.GetBool();
	case MDLCACHE_VCOLLIDE:
		return mod_load_vcollide_async.GetBool();
	case MDLCACHE_ANIMBLOCK:
		return mod_load_anims_async.GetBool();
	case MDLCACHE_VIRTUALMODEL:
		return false;
	case MDLCACHE_VERTEXES:
		return mod_load_mesh_async.GetBool();
	}
	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CMDLCache::SetAsyncLoad( MDLCacheDataType_t type, bool bAsync )
{
	bool bRetVal = false;
	switch ( type )
	{
	case MDLCACHE_STUDIOHDR:
		break;
	case MDLCACHE_STUDIOHWDATA:
		bRetVal = mod_load_mesh_async.GetBool();
		mod_load_mesh_async.SetValue( bAsync );
		break;
	case MDLCACHE_VCOLLIDE:
		bRetVal = mod_load_vcollide_async.GetBool();
		mod_load_vcollide_async.SetValue( bAsync );
		break;
	case MDLCACHE_ANIMBLOCK:
		bRetVal = mod_load_anims_async.GetBool();
		mod_load_anims_async.SetValue( bAsync );
		break;
	case MDLCACHE_VIRTUALMODEL:
		return false;
		break;
	case MDLCACHE_VERTEXES:
		bRetVal = mod_load_mesh_async.GetBool();
		mod_load_mesh_async.SetValue( bAsync );
		break;
	}
	return bRetVal;
}

//-----------------------------------------------------------------------------
// Cache model's specified dynamic data
//-----------------------------------------------------------------------------
vertexFileHeader_t *CMDLCache::BuildAndCacheVertexData( studiohdr_t *pStudioHdr, CMDLCacheData &cacheData )
{
	MDLHandle_t	handle = VoidPtrToMDLHandle( pStudioHdr->VirtualModel() );
	vertexFileHeader_t *pRawVvdHdr, *pVvdHdr;

	MdlCacheMsg( "MDLCache: Load VVD for %s\n", pStudioHdr->pszName() );

	pRawVvdHdr = (vertexFileHeader_t *)cacheData.Data();
	Assert( pRawVvdHdr );

	// check header
	if ( pRawVvdHdr->id != MODEL_VERTEX_FILE_ID )
	{
		Warning( "Error Vertex File for '%s' id %d should be %d\n", pStudioHdr->pszName(), pRawVvdHdr->id, MODEL_VERTEX_FILE_ID );
		return NULL;
	}
	if ( pRawVvdHdr->version != MODEL_VERTEX_FILE_VERSION )
	{
		Warning( "Error Vertex File for '%s' version %d should be %d\n", pStudioHdr->pszName(), pRawVvdHdr->version, MODEL_VERTEX_FILE_VERSION );
		return NULL;
	}
	if ( pRawVvdHdr->checksum != pStudioHdr->checksum )
	{
		Warning( "Error Vertex File for '%s' checksum %ld should be %ld\n", pStudioHdr->pszName(), pRawVvdHdr->checksum, pStudioHdr->checksum );
		return NULL;
	}

	Assert( pRawVvdHdr->numLODs );
	if ( !pRawVvdHdr->numLODs )
	{
		return NULL;
	}

	bool bNeedsTangentS = true;
	int rootLOD = MIN( pStudioHdr->rootLOD, pRawVvdHdr->numLODs - 1 );

	bool bHasExtraData = (pStudioHdr->flags & STUDIOHDR_FLAGS_EXTRA_VERTEX_DATA) != 0;

	// determine final cache footprint, possibly truncated due to lod
	int cacheLength = Studio_VertexDataSize( pRawVvdHdr, rootLOD, bNeedsTangentS, bHasExtraData );

	MdlCacheMsg( "MDLCache: Alloc VVD %s\n", GetModelName( handle ) );

	// allocate cache space
	MemAlloc_PushAllocDbgInfo( "Models:Vertex data", 0);
	pVvdHdr = (vertexFileHeader_t *)AllocData( MDLCACHE_VERTEXES, cacheLength );
	MemAlloc_PopAllocDbgInfo();

	GetCacheSection( MDLCACHE_VERTEXES )->BeginFrameLocking();

	CacheData( &m_MDLDict[handle]->m_VertexCache, pVvdHdr, cacheLength, pStudioHdr->pszName(), MDLCACHE_VERTEXES, MakeCacheID( handle, MDLCACHE_VERTEXES) );

	// expected 32 byte alignment
	Assert( ((uintp)pVvdHdr & 0x1F) == 0 );

	// load minimum vertexes and fixup
	Studio_LoadVertexes( pRawVvdHdr, pVvdHdr, rootLOD, bNeedsTangentS, bHasExtraData );

	GetCacheSection( MDLCACHE_VERTEXES )->EndFrameLocking();

	return pVvdHdr;
}

//-----------------------------------------------------------------------------
// Load and cache model's specified dynamic data
//-----------------------------------------------------------------------------
vertexFileHeader_t *CMDLCache::LoadVertexData( studiohdr_t *pStudioHdr )
{
	char				pFileName[MAX_PATH];
	MDLHandle_t			handle;

	Assert( pStudioHdr );
	handle = VoidPtrToMDLHandle( pStudioHdr->VirtualModel() );
	Assert( !m_MDLDict[handle]->m_VertexCache );

	studiodata_t *pStudioData = m_MDLDict[handle];

	if ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_VERTEX_DATA )
	{
		return NULL;
	}

	intp iAsync = GetAsyncInfoIndex( handle, MDLCACHE_VERTEXES );

	if ( iAsync == NO_ASYNC )
	{
		// load the VVD file
		// use model name for correct path
		MakeFilename( pFileName, pStudioHdr, ".vvd" );
		if ( IsGameConsole() )
		{
			char pX360Filename[MAX_PATH];
			UpdateOrCreate( pStudioHdr, pFileName, pX360Filename, sizeof( pX360Filename ), "GAME" );
			Q_strncpy( pFileName, pX360Filename, sizeof(pX360Filename) );
		}

		MdlCacheMsg( "MDLCache: Begin load VVD %s\n", pFileName );

		AsyncInfo_t info;
		if ( IsDebug() )
		{
			memset( &info, 0xdd, sizeof( AsyncInfo_t ) );
		}
		info.hModel = handle;
		info.type = MDLCACHE_VERTEXES;
		info.iAnimBlock = 0;
		info.hControl = NULL;
		LoadData( pFileName, "GAME", mod_load_mesh_async.GetBool(), &info.hControl, handle );
		{
			AUTO_LOCK_FM( m_AsyncMutex );
			iAsync = SetAsyncInfoIndex( handle, MDLCACHE_VERTEXES, m_PendingAsyncs.AddToTail( info ) );
		}
	}

	ProcessPendingAsync( iAsync );

	if ( !mod_load_mesh_async.GetBool() && mod_lock_meshes_on_load.GetBool() )
	{
		m_MDLDict[handle]->m_pForceLockedVertexFileHeader = (vertexFileHeader_t *)GetCacheSection( MDLCACHE_VERTEXES )->Lock( m_MDLDict[handle]->m_VertexCache );
	}
	return (vertexFileHeader_t *)CheckData( m_MDLDict[handle]->m_VertexCache, MDLCACHE_VERTEXES );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
vertexFileHeader_t *CMDLCache::GetVertexData( MDLHandle_t handle )
{
	if ( mod_test_not_available.GetBool() )
		return NULL;

	if ( mod_test_verts_not_available.GetBool() )
		return NULL;

	if ( m_MDLDict[handle]->m_pForceLockedVertexFileHeader )
	{
		return m_MDLDict[handle]->m_pForceLockedVertexFileHeader;
	}

	return CacheVertexData( GetStudioHdr( handle ) );
}


//-----------------------------------------------------------------------------
// Allocates a cacheable item
//-----------------------------------------------------------------------------
void *CMDLCache::AllocData( MDLCacheDataType_t type, int size )
{
	void *pData = _aligned_malloc( size, 32 );

	if ( !pData )
	{
		Error( "CMDLCache:: Out of memory" );
		return NULL;
	}

	return pData;
}


//-----------------------------------------------------------------------------
// Caches an item
//-----------------------------------------------------------------------------
void CMDLCache::CacheData( DataCacheHandle_t *c, void *pData, int size, const char *name, MDLCacheDataType_t type, DataCacheClientID_t id )
{
	if ( !pData )
	{
		return;
	}

	if ( id == (DataCacheClientID_t)-1 )
		id = (DataCacheClientID_t)pData;

	GetCacheSection( type )->Add(id, pData, size, c );
}

//-----------------------------------------------------------------------------
// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns NULL
//-----------------------------------------------------------------------------
void *CMDLCache::CheckData( DataCacheHandle_t c, MDLCacheDataType_t type )
{
	return GetCacheSection( type )->Get( c, true );
}

//-----------------------------------------------------------------------------
// returns the cached data, if present, otherwise returns NULL
//-----------------------------------------------------------------------------
void *CMDLCache::CheckDataNoTouch( DataCacheHandle_t c, MDLCacheDataType_t type )
{
	return GetCacheSection( type )->GetNoTouch( c, true );
}

//-----------------------------------------------------------------------------
// Frees a cache item
//-----------------------------------------------------------------------------
void CMDLCache::UncacheData( DataCacheHandle_t c, MDLCacheDataType_t type, bool bLockedOk )
{
	if ( c == DC_INVALID_HANDLE )
		return;

	IDataCacheSection *pSection = GetCacheSection( type );
	if ( !pSection->IsPresent( c ) )
		return;

	if ( !bLockedOk )
	{
		if ( pSection->GetLockCount( c ) > 0 )
		{
			return;
		}
	}

	pSection->BreakLock( c );

	const void *pItemData;
	pSection->Remove( c, &pItemData );

	FreeData( type, (void *)pItemData );
}


//-----------------------------------------------------------------------------
// Frees memory for an item
//-----------------------------------------------------------------------------
void CMDLCache::FreeData( MDLCacheDataType_t type, void *pData )
{
	if ( type != MDLCACHE_ANIMBLOCK )
	{
		_aligned_free( (void *)pData );
	}
	else
	{
		FreeAnimBlock( pData );
	}
}


void CMDLCache::InitPreloadData( bool rebuild )
{
}

void CMDLCache::ShutdownPreloadData()
{
}

//-----------------------------------------------------------------------------
// Work function for processing a model delivered by the queued loader.
// ProcessDataIntoCache() is invoked for each MDL datum.
//-----------------------------------------------------------------------------
void CMDLCache::ProcessQueuedData( ModelParts_t *pModelParts )
{
	// the studiohdr is critical, ensure it's setup as expected
	MDLHandle_t handle = pModelParts->hMDL;
	studiohdr_t *pStudioHdr = NULL;
	if ( pModelParts->nLoadedParts & ( 1 << ModelParts_t::BUFFER_MDL ) )
	{
		CMDLCacheData cacheData( MDLCACHE_STUDIOHDR, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &pModelParts->Buffers[ModelParts_t::BUFFER_MDL] );
		if ( cacheData.Data() )
		{
			ProcessDataIntoCache( handle, cacheData );
		}
	}

	bool bAbort = false;
	pStudioHdr = (studiohdr_t *)CheckDataNoTouch( m_MDLDict[handle]->m_MDLCache, MDLCACHE_STUDIOHDR );
	if ( !pStudioHdr )
	{
		// huh?, the header is expected to be loaded and locked, everything depends on it!
		Assert( 0 );
		DevWarning( "CMDLCache:: Error MDLCACHE_STUDIOHDR not present for '%s'\n", GetModelName( handle ) );

		// cannot unravel any of this model's dependant data, abort any further processing
		bAbort = true;
	}

	if ( pModelParts->nLoadedParts & ( 1 << ModelParts_t::BUFFER_PHY ) )
	{
		CMDLCacheData cacheData( MDLCACHE_VCOLLIDE, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &pModelParts->Buffers[ModelParts_t::BUFFER_PHY] );
		if ( bAbort )
			cacheData.Purge();
		ProcessDataIntoCache( handle, cacheData );
	}

	// vvd vertexes before vtx
	if ( pModelParts->nLoadedParts & ( 1 << ModelParts_t::BUFFER_VVD ) )
	{
		CMDLCacheData cacheData( MDLCACHE_VERTEXES, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &pModelParts->Buffers[ModelParts_t::BUFFER_VVD] );
		if ( bAbort )
			cacheData.Purge();
		ProcessDataIntoCache( handle, cacheData );
	}

	// can construct meshes after vvd and vtx vertexes arrive
	if ( pModelParts->nLoadedParts & ( 1 << ModelParts_t::BUFFER_VTX ) )
	{
		CMDLCacheData cacheData( MDLCACHE_STUDIOHWDATA, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &pModelParts->Buffers[ModelParts_t::BUFFER_VTX] );
		if ( bAbort )
			cacheData.Purge();

		// ProcessDataIntoCache() will do an unlock, so lock
		studiodata_t *pStudioData = m_MDLDict[handle];
		GetCacheSection( MDLCACHE_STUDIOHWDATA )->Lock( pStudioData->m_VertexCache );
		{
			// constructing the static meshes isn't thread safe
			AUTO_LOCK_FM( m_QueuedLoadingMutex );
			ProcessDataIntoCache( handle, cacheData );
		}
	}

	delete pModelParts;
}

//-----------------------------------------------------------------------------
// Journals each of the incoming MDL components until all arrive (or error).
// Not all components exist, but that information is not known at job submission.
//-----------------------------------------------------------------------------
void CMDLCache::QueuedLoaderCallback_MDL( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
{
	// validity is denoted by a nonzero buffer
	nSize = ( loaderError == LOADERERROR_NONE ) ? nSize : 0;

	// journal each incoming buffer
	ModelParts_t *pModelParts = (ModelParts_t *)pContext;
	ModelParts_t::BufferType_t bufferType = static_cast< ModelParts_t::BufferType_t >(size_cast<int>( (intp) pContext2 ) );
	pModelParts->Buffers[bufferType].SetExternalBuffer( (void *)pData, nSize, nSize, CUtlBuffer::READ_ONLY );
	pModelParts->nLoadedParts += (1 << bufferType);

	// wait for all components
	if ( pModelParts->DoFinalProcessing() )
	{
		// now have all components, process the raw data into the cache
		g_MDLCache.ProcessQueuedData( pModelParts );
	}
}

//-----------------------------------------------------------------------------
// Build a queued loader job to get the MDL ant all of its components into the cache.
//-----------------------------------------------------------------------------
bool CMDLCache::PreloadModel( MDLHandle_t handle )
{
	if ( !IsGameConsole() )
	{
		return false;
	}

	if ( !g_pQueuedLoader->IsMapLoading() || handle == MDLHANDLE_INVALID )
	{
		return false;
	}

	if ( !g_pQueuedLoader->IsBatching() )
	{
		// batching must be active, following code depends on its behavior
		DevWarning( "CMDLCache:: Late preload of model '%s'\n", GetModelName( handle ) );
		return false;
	}

	// determine existing presence
	// actual necessity is not established here, allowable absent files need their i/o error to occur
	// queued loader has additional info and may inhibit some specific model's types
	bool bNeedsMDL = !IsDataLoaded( handle, MDLCACHE_STUDIOHDR );
	bool bNeedsVTX = !IsDataLoaded( handle, MDLCACHE_STUDIOHWDATA );
	bool bNeedsVVD = !IsDataLoaded( handle, MDLCACHE_VERTEXES );
	bool bNeedsPHY = !IsDataLoaded( handle, MDLCACHE_VCOLLIDE );
	if ( !bNeedsMDL && !bNeedsVTX && !bNeedsVVD && !bNeedsPHY )
	{
		// already in cache, nothing to do
		return true;
	}

	char szFilename[MAX_PATH];
	char szNameOnDisk[MAX_PATH];
	V_strncpy( szFilename, GetActualModelName( handle ), sizeof( szFilename ) );
	V_StripExtension( szFilename, szFilename, sizeof( szFilename ) );

	// need to gather all model parts (mdl, vtx, vvd, phy, ani)
	ModelParts_t *pModelParts = new ModelParts_t;
	pModelParts->hMDL = handle;

	// create multiple loader jobs to perform gathering i/o operations
	LoaderJob_t loaderJob;
	loaderJob.m_pPathID = "GAME";
	loaderJob.m_pCallback = QueuedLoaderCallback_MDL;
	loaderJob.m_pContext = (void *)pModelParts;
	loaderJob.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
	loaderJob.m_bPersistTargetData = true;

	if ( bNeedsMDL )
	{
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.mdl", szFilename, GetPlatformExt() );
		loaderJob.m_pFilename = szNameOnDisk;
		loaderJob.m_pContext2 = (void *)ModelParts_t::BUFFER_MDL;
		if ( g_pQueuedLoader->AddJob( &loaderJob ) )
		{
			pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_MDL;
		}
	}

	if ( bNeedsVTX )
	{
		// vtx extensions are .xxx.vtx, need to re-form as, ???.xxx.yyy.vtx
		char szTempName[MAX_PATH];
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s", szFilename, GetVTXExtension() );
		V_StripExtension( szNameOnDisk, szTempName, sizeof( szTempName ) );
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.vtx", szTempName, GetPlatformExt() );
		loaderJob.m_pFilename = szNameOnDisk;
		loaderJob.m_pContext2 = (void *)ModelParts_t::BUFFER_VTX;
		if ( g_pQueuedLoader->AddJob( &loaderJob ) )
		{
			pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_VTX;
		}
	}

	if ( bNeedsVVD )
	{
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.vvd", szFilename, GetPlatformExt() );
		loaderJob.m_pFilename = szNameOnDisk;
		loaderJob.m_pContext2 = (void *)ModelParts_t::BUFFER_VVD;
		if ( g_pQueuedLoader->AddJob( &loaderJob ) )
		{
			pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_VVD;
		}
	}

	if ( bNeedsPHY )
	{
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.phy", szFilename, GetPlatformExt() );
		loaderJob.m_pFilename = szNameOnDisk;
		loaderJob.m_pContext2 = (void *)ModelParts_t::BUFFER_PHY;
		if ( g_pQueuedLoader->AddJob( &loaderJob ) )
		{
			pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_PHY;
		}
	}

	if ( !pModelParts->nExpectedParts )
	{
		// further logic showed that no components are actually needed
		delete pModelParts;
	}

	return true;
}

bool CMDLCache::ProcessPendingHardwareRestore()
{
	if ( !m_QueuedAsyncHardwareLoads.Count() )
	{
		// nothing to do
		return false;
	}

	bool bDataProcessed = false;
	AsyncHardwareLoad_t asyncHardwareLoad;
	while ( m_QueuedAsyncHardwareLoads.PopItem( &asyncHardwareLoad ) )
	{
		ModelParts_t *pModelParts = asyncHardwareLoad.m_pModelParts;

		// the studiohdr should still be there, otherwise the restoration has become invalid
		bool bError = !IsDataLoaded( pModelParts->hMDL, MDLCACHE_STUDIOHDR );

		// all i/o for data components must have succeeded or they all get purged
		bError |= ( ( pModelParts->nExpectedParts & ( 1 << ModelParts_t::BUFFER_VTX ) ) && ( pModelParts->Buffers[ModelParts_t::BUFFER_VTX].Size() == 0 ) );
		bError |= ( ( pModelParts->nExpectedParts & ( 1 << ModelParts_t::BUFFER_VVD ) ) && ( pModelParts->Buffers[ModelParts_t::BUFFER_VVD].Size() == 0 ) );
		
		if ( bError )
		{
			// unexpected error, purge all
			if ( pModelParts->nExpectedParts & ( 1 << ModelParts_t::BUFFER_VTX ) ) 
			{
				void *pVTXData = asyncHardwareLoad.m_pModelParts->Buffers[ModelParts_t::BUFFER_VTX].Detach();
				if ( pVTXData )
				{
					g_pFullFileSystem->FreeOptimalReadBuffer( pVTXData );
				}
			}

			if ( pModelParts->nExpectedParts & ( 1 << ModelParts_t::BUFFER_VVD ) ) 
			{
				void *pVVDData = asyncHardwareLoad.m_pModelParts->Buffers[ModelParts_t::BUFFER_VVD].Detach();
				if ( pVVDData )
				{
					g_pFullFileSystem->FreeOptimalReadBuffer( pVVDData );
				}
			}

			delete pModelParts;
		}
		else
		{
			DevMsg( "*** Restoring: %s\n", GetModelName( pModelParts->hMDL ) );

			// now have all components, process the raw data into the cache
			g_MDLCache.ProcessQueuedData( pModelParts );

			// a non trivial operation (static mesh buildout) occurred
			bDataProcessed = true;
		}
	}

	return bDataProcessed;
}

void CMDLCache::OnAsyncHardwareDataComplete( ModelParts_t::BufferType_t bufferType, ModelParts_t *pModelParts, void *pData, int nNumReadBytes, FSAsyncStatus_t asyncStatus )
{
	// validity is denoted by a nonzero buffer
	// error handling is deferred until the processing stage drains the queue
	int nSize = ( asyncStatus == FSASYNC_OK ) ? nNumReadBytes : 0;

	// journal each incoming buffer
	pModelParts->Buffers[bufferType].SetExternalBuffer( pData, nSize, nSize, CUtlBuffer::READ_ONLY );
	pModelParts->nLoadedParts += (1 << bufferType);

	// wait for all components
	if ( pModelParts->DoFinalProcessing() )
	{
		// queue the async loaded hardware data, cannot deal with updating the hardware data until the main thread
		AsyncHardwareLoad_t asyncHardwareLoad;
		asyncHardwareLoad.m_pModelParts = pModelParts;
		m_QueuedAsyncHardwareLoads.PushItem( asyncHardwareLoad ); 
	}
}

static void IOAsyncVTXCallback( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	g_MDLCache.OnAsyncHardwareDataComplete( ModelParts_t::BUFFER_VTX, (ModelParts_t *)asyncRequest.pContext, asyncRequest.pData, numReadBytes, asyncStatus );
}

static void IOAsyncVVDCallback( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	g_MDLCache.OnAsyncHardwareDataComplete( ModelParts_t::BUFFER_VVD, (ModelParts_t *)asyncRequest.pContext, asyncRequest.pData, numReadBytes, asyncStatus );
}

//-----------------------------------------------------------------------------
// Very specialized back door for the weapon model cache to restore the HW data  it evicted.
//-----------------------------------------------------------------------------
bool CMDLCache::RestoreHardwareData( MDLHandle_t handle, FSAsyncControl_t *pAsyncVTXControl, FSAsyncControl_t *pAsyncVVDControl )
{
	if ( !IsGameConsole() )
	{
		return false;
	}

	if ( *pAsyncVTXControl || *pAsyncVVDControl )
	{
		// already scheduled
		return false;
	}

	bool bNeedsVTX = !IsDataLoaded( handle, MDLCACHE_STUDIOHWDATA );
	if ( !bNeedsVTX )
	{
		// already in cache, nothing to do
		return false;
	}

	bool bNeedsVVD = !IsDataLoaded( handle, MDLCACHE_VERTEXES );

	char szFilename[MAX_PATH];
	char szNameOnDisk[MAX_PATH];
	V_strncpy( szFilename, GetActualModelName( handle ), sizeof( szFilename ) );
	V_StripExtension( szFilename, szFilename, sizeof( szFilename ) );

	// need to gather all model parts (vtx, vvd)
	ModelParts_t *pModelParts = new ModelParts_t;
	pModelParts->hMDL = handle;

	if ( bNeedsVTX )
	{
		// vtx extensions are .xxx.vtx, need to re-form as, ???.xxx.yyy.vtx
		char szTempName[MAX_PATH];
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s", szFilename, GetVTXExtension() );
		V_StripExtension( szNameOnDisk, szTempName, sizeof( szTempName ) );
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.vtx", szTempName, GetPlatformExt() );
	
		// schedule the async
		FileAsyncRequest_t asyncRequest;
		asyncRequest.pszFilename = szNameOnDisk;
		asyncRequest.pszPathID = "GAME";
		asyncRequest.priority = -1;
		asyncRequest.flags = FSASYNC_FLAGS_ALLOCNOFREE;
		asyncRequest.pContext = (void *)pModelParts;
		asyncRequest.pfnCallback = IOAsyncVTXCallback;
		g_pFullFileSystem->AsyncRead( asyncRequest, pAsyncVTXControl );

		pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_VTX;
	}

	if ( bNeedsVVD )
	{
		V_snprintf( szNameOnDisk, sizeof( szNameOnDisk ), "%s%s.vvd", szFilename, GetPlatformExt() );

		// schedule the async
		FileAsyncRequest_t asyncRequest;
		asyncRequest.pszFilename = szNameOnDisk;
		asyncRequest.pszPathID = "GAME";
		asyncRequest.priority = -1;
		asyncRequest.flags = FSASYNC_FLAGS_ALLOCNOFREE;
		asyncRequest.pContext = (void *)pModelParts;
		asyncRequest.pfnCallback = IOAsyncVVDCallback;
		g_pFullFileSystem->AsyncRead( asyncRequest, pAsyncVVDControl );

		pModelParts->nExpectedParts |= 1 << ModelParts_t::BUFFER_VVD;
	}

	if ( !pModelParts->nExpectedParts )
	{
		// further logic showed that no components are actually needed
		delete pModelParts;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Clear the STUDIODATA_ERROR_MODEL flag.
//-----------------------------------------------------------------------------
void CMDLCache::ResetErrorModelStatus( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
		return;

	// added STUDIODATA_FLAGS_NO_STUDIOMESH for hammer when the dir watching catches a file in mid-processing.
	// otherwise, this flag is permanently set and no future loading will happen, even once the model is valid.
	m_MDLDict[handle]->m_nFlags &= ~( STUDIODATA_ERROR_MODEL | STUDIODATA_FLAGS_NO_STUDIOMESH );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMDLCache::MarkFrame()
{
	ProcessPendingAsyncs();
}

bool CMDLCache::ReleaseAnimBlockAllocator()
{
	if ( !g_AnimBlockAllocator.IsEmpty() )
	{
		Warning( "Failure to release anim block allocator, unexpected remaining allocations\n" );
		return false;
	}

	g_AnimBlockAllocator.Clear();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: bind studiohdr_t support functions to the mdlcacher
//-----------------------------------------------------------------------------
const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const
{
	MDLHandle_t handle = g_MDLCache.FindMDL( pModelName );
	*cache = (void*)(uintp)handle;
	return g_MDLCache.GetStudioHdr( handle );
}

virtualmodel_t *studiohdr_t::GetVirtualModel( void ) const
{
	if (numincludemodels == 0)
		return NULL;

	return g_MDLCache.GetVirtualModelFast( this, VoidPtrToMDLHandle( VirtualModel() ) );
}

byte *studiohdr_t::GetAnimBlock( int i, bool preloadIfMissing ) const
{
	return g_MDLCache.GetAnimBlock( VoidPtrToMDLHandle( VirtualModel() ), i, preloadIfMissing );
}

// Shame that this code is duplicated... :(
bool studiohdr_t::hasAnimBlockBeenPreloaded( int i ) const
{
	return g_pMDLCache->HasAnimBlockBeenPreloaded( VoidPtrToMDLHandle( VirtualModel() ), i );
}

int studiohdr_t::GetAutoplayList( unsigned short **pOut ) const
{
	return g_MDLCache.GetAutoplayList( VoidPtrToMDLHandle( VirtualModel() ), pOut );
}

const studiohdr_t *virtualgroup_t::GetStudioHdr( void ) const
{
	return g_MDLCache.GetStudioHdr( VoidPtrToMDLHandle( cache ) );
}


// combined models
MDLHandle_t CMDLCache::CreateCombinedModel( const char *pszModelName )
{
	char szPlaceholderName[ MAX_PATH ];
	char szFinalName[ MAX_PATH ];

	V_strcpy_safe( szFinalName, pszModelName );
	V_RemoveDotSlashes( szFinalName, '/' );

	V_sprintf_safe( szPlaceholderName, "combined_placeholder_%s", pszModelName );
	V_RemoveDotSlashes( szPlaceholderName, '/' );

	MDLHandle_t FinalHandle = m_MDLDict.Find( szFinalName );
	MDLHandle_t PlaceholderHandle = m_MDLDict.Find( szPlaceholderName );
	if ( FinalHandle == m_MDLDict.InvalidIndex() && PlaceholderHandle == m_MDLDict.InvalidIndex() )
	{
		TCombinedStudioData	*pCombinedStudioData = ( TCombinedStudioData * )malloc( sizeof( TCombinedStudioData ) );
		memset( pCombinedStudioData, 0, sizeof( *pCombinedStudioData ) );
		pCombinedStudioData->m_nReferenceFlags = COMBINED_REFERENCE_PLACEHOLDER | COMBINED_REFERENCE_PRIMARY | COMBINED_REFERENCE_COMBINER;

		FinalHandle = m_MDLDict.Insert( szFinalName, NULL );
		InitStudioData( FinalHandle );
		studiodata_t *pFinalStudioData = m_MDLDict[ FinalHandle ];

		PlaceholderHandle = m_MDLDict.Insert( szPlaceholderName, NULL );
		InitStudioData( PlaceholderHandle );
		studiodata_t *pPlaceholderStudioData = m_MDLDict[ PlaceholderHandle ];

		V_strcpy_safe( pCombinedStudioData->m_szCombinedModelName, pszModelName );
		pCombinedStudioData->m_pPlaceholderStudioData = pPlaceholderStudioData;
		pCombinedStudioData->m_PlaceholderHandle = PlaceholderHandle;
		pCombinedStudioData->m_pFinalStudioData = pFinalStudioData;
		pCombinedStudioData->m_FinalHandle = FinalHandle;
		pCombinedStudioData->m_pCombinedUserData = 0;
		pCombinedStudioData->m_CallbackFunc = 0;

		pPlaceholderStudioData->m_pCombinedStudioData = pCombinedStudioData;
		pPlaceholderStudioData->m_nFlags |= STUDIODATA_FLAGS_COMBINED_PLACEHOLDER | STUDIODATA_FLAGS_COMBINED_ASSET;

		pFinalStudioData->m_pCombinedStudioData = pCombinedStudioData;
		pFinalStudioData->m_nFlags |= STUDIODATA_FLAGS_COMBINED | STUDIODATA_FLAGS_COMBINED_UNAVAILABLE | STUDIODATA_FLAGS_COMBINED_ASSET;
		
#ifdef DEBUG_COMBINER
		Msg( "%p Alloc: pPlaceholderStudioData=%p, pFinalStudioData=%p\n", pCombinedStudioData, pPlaceholderStudioData, pFinalStudioData );
#endif
	}
	else
	{
		AssertMsg1( false, "Asking to combine model '%s' when it already has a placeholder or final model handle", pszModelName );
		return MDLHANDLE_INVALID;
	}

	InitCombiner();

	AddRef( FinalHandle );
	AddRef( PlaceholderHandle );

	return PlaceholderHandle;
}


bool CMDLCache::CreateCombinedModel( MDLHandle_t handle )
{
	studiodata_t		*pFinalStudioData = m_MDLDict[ handle ];

	if ( pFinalStudioData == NULL )
	{
		return false;
	}

	TCombinedStudioData	*pCombinedStudioData = pFinalStudioData->m_pCombinedStudioData;

	if ( pCombinedStudioData == NULL )
	{
		pCombinedStudioData = ( TCombinedStudioData * )malloc( sizeof( TCombinedStudioData ) );
	}
	else
	{
/*
		if ( ( pCombinedStudioData->m_nReferenceFlags & COMBINED_REFERENCE_PLACEHOLDER ) != 0 )
		{	// are we trying to replace a combined model too quickly
			Assert( 0 );
			return false;
		}
*/
		if ( ( pFinalStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED_UNAVAILABLE ) != 0 )
		{	// are we trying to replace a combined model too quickly
			Assert( 0 );
			return false;
		}

		FreeCombinedGeneratedData( pFinalStudioData );
	}
	
	InitCombiner();

	memset( pCombinedStudioData, 0, sizeof( *pCombinedStudioData ) );
	pCombinedStudioData->m_nReferenceFlags |= COMBINED_REFERENCE_PLACEHOLDER;	// the replace will nuke away a ref count

	V_strcpy_safe( pCombinedStudioData->m_szCombinedModelName, GetModelName( handle ) );
	pCombinedStudioData->m_pPlaceholderStudioData = NULL;
	pCombinedStudioData->m_PlaceholderHandle = MDLHANDLE_INVALID;
	pCombinedStudioData->m_pFinalStudioData = pFinalStudioData;
	pCombinedStudioData->m_FinalHandle = handle;

	pFinalStudioData->m_pCombinedStudioData = pCombinedStudioData;
	pFinalStudioData->m_nFlags |= STUDIODATA_FLAGS_COMBINED | STUDIODATA_FLAGS_COMBINED_UNAVAILABLE;

	return true;
}


bool CMDLCache::SetCombineModels( MDLHandle_t handle, const CUtlVector< SCombinerModelInput_t > &vecModelsToCombine )
{
	if ( handle == MDLHANDLE_INVALID || vecModelsToCombine[ 0 ].m_iszModelName == NULL_STRING || STRING( vecModelsToCombine[ 0 ].m_iszModelName )[ 0 ] == '\0' )
	{
		return false;
	}

	studiodata_t		*pTempStudioData = m_MDLDict[ handle ];
	TCombinedStudioData	*pCombinedStudioData = pTempStudioData->m_pCombinedStudioData;

	FOR_EACH_VEC( vecModelsToCombine, i )
	{
		pCombinedStudioData->m_ModelInputData[ i ] = vecModelsToCombine.Element( i );
	}
	pCombinedStudioData->m_nNumModels = vecModelsToCombine.Count();

	return true;
}


bool CMDLCache::FinishCombinedModel( MDLHandle_t handle, CombinedModelLoadedCallback pFunc, void *pUserData )
{
	if ( handle == MDLHANDLE_INVALID )
	{
		return false;
	}

	studiodata_t		*pTempStudioData = m_MDLDict[ handle ];
	TCombinedStudioData	*pCombinedStudioData = pTempStudioData->m_pCombinedStudioData;

	pCombinedStudioData->m_pCombinedUserData = pUserData;
	pCombinedStudioData->m_CallbackFunc = pFunc;

	m_CombinerToBeCombined.PushItem( pCombinedStudioData );

	return true;
}


bool CMDLCache::IsCombinedPlaceholder( MDLHandle_t handle )
{
	studiodata_t *pTempStudioData = m_MDLDict[ handle ];

	return ( ( pTempStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED_PLACEHOLDER ) != 0 );
}


bool CMDLCache::IsCombinedModel( MDLHandle_t handle )
{
	studiodata_t *pTempStudioData = m_MDLDict[ handle ];

	return ( ( pTempStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED ) != 0 );
}


TCombinedStudioData	*CMDLCache::GetCombinedData( MDLHandle_t handle )
{
	if ( handle == MDLHANDLE_INVALID )
	{
		return NULL;
	}

	studiodata_t *pTempStudioData = m_MDLDict[ handle ];

	if ( ( pTempStudioData->m_nFlags & ( STUDIODATA_FLAGS_COMBINED_PLACEHOLDER | STUDIODATA_FLAGS_COMBINED ) ) == 0 )
	{
		return NULL;
	}

	return pTempStudioData->m_pCombinedStudioData;
}


int CMDLCache::GetNumCombinedSubModels( MDLHandle_t handle )
{
	TCombinedStudioData	*pCombinedStudioData = GetCombinedData( handle );
	if ( !pCombinedStudioData )
	{
		return 0;
	}

	return pCombinedStudioData->m_nNumModels;
}


void CMDLCache::GetCombinedSubModelFilename( MDLHandle_t handle, int nSubModelIndex, char *pszResult, int nResultSize )
{
	pszResult[ 0 ] = 0;

	TCombinedStudioData	*pCombinedStudioData = GetCombinedData( handle );
	if ( !pCombinedStudioData )
	{
		return;
	}

	if ( nSubModelIndex < 0 || 	nSubModelIndex > pCombinedStudioData->m_nNumModels )
	{
		return;
	}

	V_strncpy( pszResult, STRING( pCombinedStudioData->m_ModelInputData[ nSubModelIndex ].m_iszModelName ), nResultSize );
}


KeyValues *CMDLCache::GetCombinedMaterialKV( MDLHandle_t handle, int nAtlasGroup )
{
	TCombinedStudioData	*pCombinedStudioData = GetCombinedData( handle );
	if ( !pCombinedStudioData )
	{
		return NULL;
	}

	if ( !pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_pCombinedMaterial )
	{
		return NULL;
	}

	return pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_pCombinedMaterial->MakeCopy();
}


void CMDLCache::CheckCombinerFlagChanges( int nNewFlags )
{
	unsigned nFlagsChanged = m_nCombinerFlags ^ nNewFlags;

	if ( ( nFlagsChanged & COMBINER_FLAG_THREADING ) != 0 )
	{
		ShutdownCombiner();
	}

	m_nCombinerFlags = nNewFlags;

	if ( ( nFlagsChanged & COMBINER_FLAG_THREADING ) != 0 )
	{
		InitCombiner();
	}
}


void CMDLCache::SetCombinerFlags( unsigned nFlags )
{
	CheckCombinerFlagChanges( m_nCombinerFlags | nFlags );
}


void CMDLCache::ClearCombinerFlags( unsigned nFlags )
{
	CheckCombinerFlagChanges( m_nCombinerFlags & ( ~nFlags ) );
}


void CMDLCache::UpdateCombiner( )
{
	// non-threaded approach
	BeginLock();

	TCombinedStudioData	*pCombinedStudioData;

	if ( ( m_nCombinerFlags & COMBINER_FLAG_THREADING ) == 0 )
	{
		if ( m_CombinerToBeCombined.PopItem( &pCombinedStudioData ) )
		{
			pCombinedStudioData->m_pCombineData = &g_ModelCombiner;
			pCombinedStudioData->m_pCombineData->Init( pCombinedStudioData );
			pCombinedStudioData->m_pCombineData->Resolve();
			m_pCombinedCompleted = pCombinedStudioData;
		}
	}

	if ( m_pCombinedCompleted )
	{
		pCombinedStudioData = ( TCombinedStudioData * )m_pCombinedCompleted;

		if ( pCombinedStudioData->m_Results.m_nCombinedResults == COMBINE_RESULT_FLAG_OK )
		{
			double flStartEngineTime = Plat_FloatTime();

			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

			if ( pCombinedStudioData->m_PlaceholderHandle == MDLHANDLE_INVALID )
			{	// we are doing a replace, this will reduce the ref count that we artificially inflated for this pathway
				FlushImmediate( m_MDLDict[ pCombinedStudioData->m_FinalHandle ] );
				Flush( pCombinedStudioData->m_FinalHandle );
			}
			else
			{	// user is responsible for cleanup of the placeholder
//				Release( pCombinedStudioData->m_PlaceholderHandle );
			}
			if ( ( m_nCombinerFlags & COMBINER_FLAG_NO_DATA_PROCESSING ) == 0 )
			{
				Flush( pCombinedStudioData->m_FinalHandle, MDLCACHE_FLUSH_STUDIOHDR | MDLCACHE_FLUSH_STUDIOHWDATA | MDLCACHE_FLUSH_VERTEXES );

				if ( m_pCacheNotify && pCombinedStudioData->m_PlaceholderHandle != MDLHANDLE_INVALID )
				{
					m_pCacheNotify->OnCombinerPreCache( pCombinedStudioData->m_PlaceholderHandle, pCombinedStudioData->m_FinalHandle );
				}

				{
					CUtlBuffer buf( pCombinedStudioData->m_pCombineData->GetCombinedMDLPtr(), pCombinedStudioData->m_pCombineData->GetCombinedMDLSize(), CUtlBuffer::READ_ONLY );
					CMDLCacheData cacheData( MDLCACHE_STUDIOHDR, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &buf );
					ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, cacheData );
				}
				{
					CUtlBuffer buf( pCombinedStudioData->m_pCombineData->GetCombinedVVDPtr(), pCombinedStudioData->m_pCombineData->GetCombinedVVDSize(), CUtlBuffer::READ_ONLY );
					CMDLCacheData cacheData( MDLCACHE_VERTEXES, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &buf );
					ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, cacheData );
				}
				{
					CUtlBuffer buf( pCombinedStudioData->m_pCombineData->GetCombinedVTXPtr(), pCombinedStudioData->m_pCombineData->GetCombinedVTXSize(), CUtlBuffer::READ_ONLY );
					CMDLCacheData cacheData( MDLCACHE_STUDIOHWDATA, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &buf );
					ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, cacheData );
				}
				//ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, MDLCACHE_STUDIOHDR, 0, pCombinedStudioData->m_pCombineData->GetCombinedMDLPtr(), pCombinedStudioData->m_pCombineData->GetCombinedMDLSize(),
				//	pCombinedStudioData->m_pCombineData->GetCombinedMDLAvailability() );
				//ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, MDLCACHE_VERTEXES, 0, pCombinedStudioData->m_pCombineData->GetCombinedVVDPtr(), pCombinedStudioData->m_pCombineData->GetCombinedVVDSize(),
				//	pCombinedStudioData->m_pCombineData->GetCombinedVVDAvailability() );
				//ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, MDLCACHE_STUDIOHWDATA, 0, pCombinedStudioData->m_pCombineData->GetCombinedVTXPtr(), pCombinedStudioData->m_pCombineData->GetCombinedVTXSize(),
				//	pCombinedStudioData->m_pCombineData->GetCombinedVTXAvailability() );
			}			
			else
			{
				FreeCombinedGeneratedData( m_MDLDict[ pCombinedStudioData->m_FinalHandle ] );
			}
			studiodata_t *pStudioDataCurrent = m_MDLDict[ pCombinedStudioData->m_FinalHandle ];
			pStudioDataCurrent->m_nFlags &= ~STUDIODATA_FLAGS_COMBINED_UNAVAILABLE;

			pCombinedStudioData->m_Results.m_flEngineProcessingDuration = ( float )( Plat_FloatTime() - flStartEngineTime );
			
			pCombinedStudioData->m_CallbackFunc( pCombinedStudioData->m_pCombinedUserData, pCombinedStudioData->m_PlaceholderHandle, pCombinedStudioData->m_FinalHandle, pCombinedStudioData->m_Results );
		}
		else
		{
			FreeCombinedGeneratedData( m_MDLDict[ pCombinedStudioData->m_FinalHandle ] );
			Release( pCombinedStudioData->m_FinalHandle );
			pCombinedStudioData->m_CallbackFunc( pCombinedStudioData->m_pCombinedUserData, pCombinedStudioData->m_PlaceholderHandle, MDLHANDLE_INVALID, pCombinedStudioData->m_Results );
//			user is responsible for cleanup of the placeholder
//			Release( pCombinedStudioData->m_PlaceholderHandle );
		}

		if ( ( m_nCombinerFlags & COMBINER_FLAG_NO_DATA_PROCESSING ) != 0 )
		{
			GetTextureCombiner().FreeCombinedMaterials();
		}

		pCombinedStudioData->m_nReferenceFlags &= ~COMBINED_REFERENCE_COMBINER;
		if ( pCombinedStudioData->m_nReferenceFlags == 0 )
		{
			Assert( 0 );
			Error( "CMDLCache::UpdateCombiner - model handles have been freed" );
		}

		m_pCombinedCompleted = NULL;

//		delete pCombinedStudioData->m_pCombineData;
//		pCombinedStudioData->m_pCombineData = NULL;
	}

	// do the scheduling of the next item here, so that we have a full frame to process it
	if ( ( m_nCombinerFlags & COMBINER_FLAG_THREADING ) != 0 )
	{
		if ( m_pToBeCombined == NULL && m_pCombinedCompleted == NULL )
		{
			if ( m_CombinerToBeCombined.PopItem( &pCombinedStudioData ) )
			{
				m_pToBeCombined = pCombinedStudioData;
				m_CombinerEvent.Set();
			}
		}
	}

	EndLock();
}


void *CMDLCache::GetCombinedInternalAsset( ECombinedAsset AssetType, const char *pszAssetID, int *nSize )
{
	if ( nSize != NULL )
	{
		*nSize = 0;
	}

	switch( AssetType )
	{
		case COMBINED_ASSET_MATERIAL:
			{
				char		szAssetName[ MAX_PATH ];
				MDLHandle_t	nHandleID;
				int			nAssetID;
				int			nAtlasGroup;

				// expecting "!%s|%hu|%d!"
				const char *pStartPos = pszAssetID;
				if ( *pStartPos != '!' )
				{	
					Assert( 0 ); 
					return NULL;
				}

				pStartPos++;
				const char *pEndPos = strchr( pStartPos, '|' );
				if ( pEndPos == NULL )
				{	
					Assert( 0 ); 
					return NULL;
				}
				int nLength = pEndPos - pStartPos;
				if ( ( nLength + 1 ) > sizeof( szAssetName ) ) 
				{	
					Assert( 0 ); 
					return NULL;
				}

				strncpy( szAssetName, pStartPos, nLength );
				szAssetName[ nLength ] = 0;

				pStartPos = pEndPos + 1;
				nAtlasGroup = atoi( pStartPos );

				if ( nAtlasGroup < 0 || nAtlasGroup >= COMBINER_MAX_ATLAS_GROUPS )
				{
					Assert( 0 );
					return NULL;
				}

				pEndPos = strchr( pStartPos, '|' );
				if ( pEndPos == NULL )
				{	
					Assert( 0 ); 
					return NULL;
				}

				pStartPos = pEndPos + 1;
				nHandleID = atol( pStartPos );

				pEndPos = strchr( pStartPos, '|' );
				if ( pEndPos == NULL )
				{	
					Assert( 0 ); 
					return NULL;
				}

				pStartPos = pEndPos + 1;
				nAssetID = atoi( pStartPos );

				pEndPos = strchr( pStartPos, '!' );
				if ( pEndPos == NULL )
				{	
					Assert( 0 ); 
					return NULL;
				}

				studiodata_t *pFinalStudioData = m_MDLDict[ nHandleID ];
				if ( pFinalStudioData == NULL )
				{
					Assert( 0 );
					return NULL;
				}

				TCombinedStudioData	*pCombinedStudioData = pFinalStudioData->m_pCombinedStudioData;
				if ( pCombinedStudioData == NULL )
				{
					Assert( 0 );
					return NULL;
				}

				if ( pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_pCombinedMaterial == NULL )
				{
					Assert( 0 );
					return NULL;
				}

				return pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_pCombinedMaterial;
			}
			break;

		case COMBINED_ASSET_TEXTURE:
			{
				char		szAssetName[ MAX_PATH ];
				int			nAtlasGroup;
				int			nTexture;
				MDLHandle_t	nHandleID;
				int			nAssetID;

				// expecting "!%s|%d|%hu|%d!"
				const char *pStartPos = pszAssetID;
				if ( *pStartPos != '!' )
				{	
					Assert( 0 ); 
					return NULL;
				}

				pStartPos++;
				const char *pEndPos = strchr( pStartPos, '|' );
				if ( pEndPos == NULL )
				{	
					Assert( 0 ); 
					return NULL;
				}
				int nLength = pEndPos - pStartPos;
				if ( ( nLength + 1 ) > sizeof( szAssetName ) ) 
				{	
					Assert( 0 ); 
					return NULL;
				}

				strncpy( szAssetName, pStartPos, nLength );
				szAssetName[ nLength ] = 0;

				pStartPos = pEndPos + 1;
				nAtlasGroup = atoi( pStartPos );

				if ( nAtlasGroup < 0 || nAtlasGroup >= COMBINER_MAX_ATLAS_GROUPS )
				{
					Assert( 0 );
					return NULL;
				}

				pEndPos = strchr( pStartPos, '|' );
				if ( pEndPos == NULL )
				{	
					Assert( 0 ); 
					return NULL;
				}

				pStartPos = pEndPos + 1;
				nTexture = atoi( pStartPos );

				if ( nTexture < 0 || nTexture >= COMBINER_MAX_TEXTURES_PER_MATERIAL )
				{
					Assert( 0 );
					return NULL;
				}

				pEndPos = strchr( pStartPos, '|' );
				if ( pEndPos == NULL )
				{	
					Assert( 0 ); 
					return NULL;
				}

				pStartPos = pEndPos + 1;
				nHandleID = atol( pStartPos );

				pEndPos = strchr( pStartPos, '|' );
				if ( pEndPos == NULL )
				{	
					Assert( 0 ); 
					return NULL;
				}

				pStartPos = pEndPos + 1;
				nAssetID = atoi( pStartPos );

				pEndPos = strchr( pStartPos, '!' );
				if ( pEndPos == NULL )
				{	
					Assert( 0 ); 
					return NULL;
				}

				studiodata_t *pFinalStudioData = m_MDLDict[ nHandleID ];
				if ( pFinalStudioData == NULL )
				{
					Assert( 0 );
					return NULL;
				}

				TCombinedStudioData	*pCombinedStudioData = pFinalStudioData->m_pCombinedStudioData;
				if ( pCombinedStudioData == NULL )
				{
					Assert( 0 );
					return NULL;
				}

				if ( pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_pCombinedTextures[ nTexture ] == NULL )
				{
					Assert( 0 );
					return NULL;
				}

				*nSize = pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_nCombinedTextureSizes[ nTexture ];
				return pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_pCombinedTextures[ nTexture ];
			}
			break;
	}

	return NULL;
}


void CMDLCache::CombinerThread( )
{
	while( !m_bCombinerShutdown )
	{
		m_CombinerEvent.Wait();
		m_CombinerEvent.Reset();

		if ( m_pToBeCombined )
		{
			TCombinedStudioData	*pCombinedStudioData = ( TCombinedStudioData * )m_pToBeCombined;

			pCombinedStudioData->m_pCombineData = &g_ModelCombiner;
			pCombinedStudioData->m_pCombineData->Init( pCombinedStudioData );
			pCombinedStudioData->m_pCombineData->Resolve();

			m_pCombinedCompleted = m_pToBeCombined;
			m_pToBeCombined = NULL;
		}
	}

	m_CombinerShutdownEvent.Set();
}


uintp CMDLCache::StaticCombinerThread( void *pParam )
{
	g_MDLCache.CombinerThread();

	return 0;
}


void CMDLCache::InitCombiner( )
{
	if ( m_bCombinerReady )
	{
		return;
	}

	if ( ( m_nCombinerFlags & COMBINER_FLAG_THREADING ) != 0 )
	{
		m_bCombinerShutdown = false;
		m_CombinerShutdownEvent.Reset();

		m_hCombinerThread = CreateSimpleThread( CMDLCache::StaticCombinerThread, NULL, 10240 );
		ThreadSetDebugName( m_hCombinerThread, "Combiner" );
	}

	m_bCombinerReady = true;
}


void CMDLCache::ShutdownCombiner( )
{
	if ( !m_bCombinerReady )
	{
		return;
	}

	if ( ( m_nCombinerFlags & COMBINER_FLAG_THREADING ) != 0 && m_hCombinerThread != NULL )
	{
		m_bCombinerShutdown = true;
		m_CombinerEvent.Set();
		m_CombinerShutdownEvent.Wait();
#if 0
		// how to kill this guy off?
		ReleaseThreadHandle( m_hCombinerThread );
#endif
		m_hCombinerThread = NULL;
	}

	m_bCombinerReady = false;
}


void CMDLCache::FreeCombinedGeneratedData( studiodata_t *pStudioData )
{
	for ( int nGroup = 0; nGroup < pStudioData->m_pCombinedStudioData->m_nNumAtlasGroups; nGroup++ )
	{
		for( int nTexture = 0; nTexture < COMBINER_MAX_TEXTURES_PER_MATERIAL; nTexture++ )
		{
			if ( pStudioData->m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_pCombinedTextures[ nTexture ] )
			{
				free( pStudioData->m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_pCombinedTextures[ nTexture ] );
				pStudioData->m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_pCombinedTextures[ nTexture ] = NULL;
				pStudioData->m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_nCombinedTextureSizes[ nTexture ] = 0;
				pStudioData->m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_szCombinedMaterialName[ 0 ] = 0;
			}
		}

		if ( pStudioData->m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_pCombinedMaterial )
		{
			pStudioData->m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_pCombinedMaterial->deleteThis();
			pStudioData->m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_pCombinedMaterial = NULL;
		}
	}
}


bool CMDLCache::UnserializeCombinedHardwareData( MDLHandle_t handle )
{
	if ( m_bCombinerReady )
	{	// we should be doing this only when the combiner is shut down
		Assert( 0 );
		return false;
	}

	studiodata_t *pStudioData = m_MDLDict[ handle ];

	if ( !pStudioData )
	{
		return false;
	}

	// we should have the lock already if we are in here!

	TCombinedStudioData	*pCombinedStudioData = pStudioData->m_pCombinedStudioData;

	if ( !pCombinedStudioData )
	{
		return false;
	}

	FreeCombinedGeneratedData( pStudioData );

	pCombinedStudioData->m_pCombineData = &g_ModelCombiner;
	pCombinedStudioData->m_pCombineData->Init( pCombinedStudioData );
	pCombinedStudioData->m_pCombineData->Resolve();
	m_pCombinedCompleted = pCombinedStudioData;

	if ( pCombinedStudioData->m_Results.m_nCombinedResults == COMBINE_RESULT_FLAG_OK )
	{	
		Flush( handle, MDLCACHE_FLUSH_STUDIOHDR | MDLCACHE_FLUSH_STUDIOHWDATA | MDLCACHE_FLUSH_VERTEXES );

		// we need to process all of the data as the internal asset id's have been updated ( for material and texture references )

		{
			CUtlBuffer buf( pCombinedStudioData->m_pCombineData->GetCombinedMDLPtr(), pCombinedStudioData->m_pCombineData->GetCombinedMDLSize(), CUtlBuffer::READ_ONLY );
			CMDLCacheData cacheData( MDLCACHE_STUDIOHDR, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &buf );
			ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, cacheData );
		}
		{
			CUtlBuffer buf( pCombinedStudioData->m_pCombineData->GetCombinedVVDPtr(), pCombinedStudioData->m_pCombineData->GetCombinedVVDSize(), CUtlBuffer::READ_ONLY );
			CMDLCacheData cacheData( MDLCACHE_VERTEXES, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &buf );
			ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, cacheData );
		}
		{
			CUtlBuffer buf( pCombinedStudioData->m_pCombineData->GetCombinedVTXPtr(), pCombinedStudioData->m_pCombineData->GetCombinedVTXSize(), CUtlBuffer::READ_ONLY );
			CMDLCacheData cacheData( MDLCACHE_STUDIOHWDATA, CMDLCacheData::ALLOC_OPTIMALREADBUFFER, &buf );
			ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, cacheData );
		}

		//ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, MDLCACHE_STUDIOHDR, 0, pCombinedStudioData->m_pCombineData->GetCombinedMDLPtr(), pCombinedStudioData->m_pCombineData->GetCombinedMDLSize(),
		//	pCombinedStudioData->m_pCombineData->GetCombinedMDLAvailability() );
		//ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, MDLCACHE_VERTEXES, 0, pCombinedStudioData->m_pCombineData->GetCombinedVVDPtr(), pCombinedStudioData->m_pCombineData->GetCombinedVVDSize(),
		//	pCombinedStudioData->m_pCombineData->GetCombinedVVDAvailability() );
		//ProcessDataIntoCache( pCombinedStudioData->m_FinalHandle, MDLCACHE_STUDIOHWDATA, 0, pCombinedStudioData->m_pCombineData->GetCombinedVTXPtr(), pCombinedStudioData->m_pCombineData->GetCombinedVTXSize(),
		//	pCombinedStudioData->m_pCombineData->GetCombinedVTXAvailability() );

		return true;
	}

	return false;
}


void CMDLCache::DebugCombinerInfo( )
{
	Msg( "MDLCache:\n" );

	for( MDLHandle_t nHandle = m_MDLDict.First(); nHandle != m_MDLDict.InvalidIndex(); nHandle = m_MDLDict.Next( nHandle ) )
	{
		studiodata_t *pStudioData = m_MDLDict[ nHandle ];

		if ( pStudioData == NULL || ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED_ASSET ) == 0 )
		{
			continue;
		}

		Msg( "  %d: ", nHandle );


		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_STUDIOMESH_LOADED ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_STUDIOMESH_LOADED " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_LOADED ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_VCOLLISION_LOADED " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_ERROR_MODEL ) != 0 )
		{
			Msg( "STUDIODATA_ERROR_MODEL " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_STUDIOMESH ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_NO_STUDIOMESH " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_NO_VERTEX_DATA ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_NO_VERTEX_DATA " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_PHYSICS2COLLISION_LOADED ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_PHYSICS2COLLISION_LOADED " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_VCOLLISION_SCANNED ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_VCOLLISION_SCANNED " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED_PLACEHOLDER ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_COMBINED_PLACEHOLDER " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_COMBINED " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED_UNAVAILABLE ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_COMBINED_UNAVAILABLE " );
		}
		if ( ( pStudioData->m_nFlags & STUDIODATA_FLAGS_COMBINED_ASSET ) != 0 )
		{
			Msg( "STUDIODATA_FLAGS_COMBINED_ASSET " );
		}
		Msg( "\n" );

		if ( pStudioData->m_pCombinedStudioData )
		{
			Msg( "   Combined Name: %s\n", pStudioData->m_pCombinedStudioData->m_szCombinedModelName );
			Msg( "   Primary Model: %s\n", STRING( pStudioData->m_pCombinedStudioData->m_ModelInputData[ 0 ].m_iszModelName ) );
			for( int i = 1; i < pStudioData->m_pCombinedStudioData->m_nNumModels; i++ )
			{
				Msg( "   Secondary Model %d: %s\n", i, STRING( pStudioData->m_pCombinedStudioData->m_ModelInputData[ i ].m_iszModelName ) );
			}
		}
	}
}

void CMDLCache::DumpDictionaryState()
{
	int nDataCount = 0;
	int nMeshCount = 0;
	int nAnimCount = 0;
	MDLHandle_t i = m_MDLDict.First();
	while ( i != m_MDLDict.InvalidIndex() )
	{
		Msg("0x%08x : %p : %s \n",
			m_MDLDict.Element( i )->m_Handle, // MDLHandle_t   (should = i)
			m_MDLDict.Element( i )->m_MDLCache, // DataCacheHandle_t
			m_MDLDict.GetElementName( i ) );

		if ( m_MDLDict.Element( i )->m_VertexCache != NULL )
		{
			Msg("0x%08x : %p : %s \n", 
				m_MDLDict.Element( i )->m_Handle, 
				m_MDLDict.Element( i )->m_VertexCache, 
				"MeshData");
			nMeshCount++;
		}

		for ( int j = 0; j < m_MDLDict.Element( i )->m_vecAnimBlocks.Count(); j++ )
		{
			if ( m_MDLDict.Element( i )->m_vecAnimBlocks.Element( j ) != NULL )
			{
				Msg("0x%08x : %p : %s \n", 
					m_MDLDict.Element( i )->m_Handle, 
					m_MDLDict.Element( i )->m_vecAnimBlocks.Element( j ), 
					"AnimBlock");
				nAnimCount++;
			}
		}

		i = m_MDLDict.Next( i );
		nDataCount++;
	}

	Msg( "DataCount: %d  MeshCount: %d  AnimCount: %d \n", nDataCount, nMeshCount, nAnimCount );
	Msg( "Total: %d \n", nDataCount + nMeshCount + nAnimCount );
}

CON_COMMAND( mdlcache_dump_dictionary_state, "Dump the state of the MDLCache Dictionary." )
{
	g_pMDLCache->DumpDictionaryState();
}

//-----------------------------------------------------------------------------
// Clears the anim cache, freeing its memory
//
// NOTE: this is only useful if no more entities will animate after this point in the map,
//       since any further ANIMBLOCK load requests will cause the cache to be recreated.
//       Entirely resident (non-streaming) animations will not have this effect.
//-----------------------------------------------------------------------------
CON_COMMAND( clear_anim_cache, "Clears the animation cache, freeing the memory (until the next time a streaming animblock is requested)." )
{
	IDataCacheSection* pSection = g_MDLCache.GetCacheSection( MDLCACHE_ANIMBLOCK );
	pSection->Purge( 512*1024*1024 ); // "purge everything"
	if ( g_AnimBlockAllocator.IsEmpty() )
	{
		Msg( "Animblock cache successfully cleared\n" );
		g_AnimBlockAllocator.Clear();
	}
	else
	{
		Warning( "Cannot clear animblock cache - %d blocks still in use!\n", MAX_ANIMBLOCKS - g_AnimBlockAllocator.m_freeList.Count() );
	}
}
