//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implementation of a material
//
//===========================================================================//

#include "imaterialinternal.h"
#include "bitmap/tgaloader.h"
#include "colorspace.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include <string.h>
#include "materialsystem_global.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/imaterialproxy.h"							   
#include "shadersystem.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "IHardwareConfigInternal.h"
#include "utlsymbol.h"
#if !defined( _PS3 )
#include <malloc.h>
#endif //!_PS3
#include "filesystem.h"
#include <keyvalues.h>
#include "mempool.h"
#include "shaderapi/ishaderutil.h"
#include "vtf/vtf.h"
#include "tier1/strtools.h"
#include <ctype.h>
#include "utlbuffer.h"
#include "mathlib/vmatrix.h"
#include "texturemanager.h"
#include "itextureinternal.h"
#include "mempool.h"
#include "tier1/callqueue.h"
#include "cmaterial_queuefriendly.h"
#include "ifilelist.h"
#include "tier0/icommandline.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
#endif

//static ConVar mat_debug_material_reference_name( "mat_debug_material_reference_name", "/eq_grenades/smoke_grenade", FCVAR_RELEASE, "Substring to debug material references" );

//-----------------------------------------------------------------------------
// Material implementation
//-----------------------------------------------------------------------------
class CMaterial : public IMaterialInternal
{
public:
	// Members of the IMaterial interface
	const char  *GetName() const;
	IMaterialVar** GetVars() { return m_pShaderParams; }
	const char  *GetTextureGroupName() const;
	inline bool	IsPrecached_Inline( ) const
	{
		return (m_Flags & MATERIAL_IS_PRECACHED) != 0;
	}
	bool	IsPrecached( ) const { return IsPrecached_Inline(); }



	inline bool	IsPrecachedVars_Inline( ) const
	{
		return (m_Flags & MATERIAL_VARS_IS_PRECACHED) != 0;
	}
	bool	IsPrecachedVars( ) const { return IsPrecachedVars_Inline(); }


	PreviewImageRetVal_t GetPreviewImageProperties( int *width, int *height, 
				 				ImageFormat *imageFormat, bool* isTranslucent ) const;
	PreviewImageRetVal_t GetPreviewImage( unsigned char *data, int width, int height,
								ImageFormat imageFormat ) const;

	int			GetMappingWidth( );
	int			GetMappingHeight( );
	int			GetNumAnimationFrames( );

	bool		InMaterialPage( void )						{ return false; }
	void		GetMaterialOffset( float *pOffset );
	void		GetMaterialScale( float *pOffset );
	IMaterial	*GetMaterialPage( void )					{ return NULL; }

	void		IncrementReferenceCount( );
	void		DecrementReferenceCount( );
	int 		GetEnumerationID( ) const;
	void		GetLowResColorSample( float s, float t, float *color ) const;
	
	IMaterialVar *		FindVar( char const *varName, bool *found, bool complain = true );
	IMaterialVar *		FindVarFast( char const *pVarName, unsigned int *pToken );

	// Sets new VMT shader parameters for the material
	virtual void		SetShaderAndParams( KeyValues *pKeyValues );

	bool				UsesEnvCubemap( void );
	bool				NeedsSoftwareSkinning( void );
	virtual bool		NeedsSoftwareLighting( void );
	bool				NeedsTangentSpace( void );
	bool				NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame = true );
	bool				NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame = true );
	virtual bool		IsUsingVertexID( ) const;

	// GR - Is lightmap alpha needed?
	bool	NeedsLightmapBlendAlpha( void );
	
	virtual void	AlphaModulate( float alpha );
	virtual void	ColorModulate( float r, float g, float b );
	virtual float	GetAlphaModulation();
	virtual void	GetColorModulation( float *r, float *g, float *b );

	void	SetMaterialVarFlag( MaterialVarFlags_t flag, bool on );
	bool	GetMaterialVarFlag( MaterialVarFlags_t flag ) const;

	bool	IsTranslucent();
	bool	IsTranslucentInternal( float fAlphaModulation ) const; //need to centralize the logic without relying on the *current* alpha modulation being that which is stored in m_pShaderParams[ALPHA].
	virtual bool	IsTranslucentUnderModulation( float fAlphaModulation ) const;
	bool	IsAlphaTested();
	bool	IsVertexLit();
	virtual bool IsSpriteCard();

	void	GetReflectivity( Vector& reflect );
	bool	GetPropertyFlag( MaterialPropertyTypes_t type );

	// Is the material visible from both sides?
	bool	IsTwoSided();

	int		GetNumPasses( void );
	int		GetTextureMemoryBytes( void );

public:
	// stuff that is visible only from within the material system

	// constructor, destructor
	CMaterial( char const* materialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues );
	virtual		~CMaterial();

	CBasePerMaterialContextData **GetContextData( int modulationFlags )
	{
		return &(m_ShaderRenderState.m_pSnapshots[modulationFlags].m_pContextData[0]);
	}

	StateSnapshot_t	GetSnapshotId( int modulation, int renderPass );
	unsigned char* GetInstanceCommandBuffer( int modulation );
	void		DrawMesh( VertexCompressionType_t vertexCompression, bool bIsAlphaModulating, bool bRenderingPreTessPatchMesh );
	void		Uncache( bool bPreserveVars = false );
	void		ReloadTextures( void );
private:
	void		Precache_Internal();
	bool		PrecacheVars_Internal( KeyValues *pKeyValues, KeyValues *pPatchKeyValues, CUtlVector<FileNameHandle_t> *pIncludes );
public:
	// If provided, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	inline bool	PrecacheVars_Inline( KeyValues *pKeyValues = NULL, KeyValues *pPatchKeyValues = NULL, CUtlVector<FileNameHandle_t> *pIncludes = NULL )
	{
		// We should get both parameters or neither
		Assert( !!pKeyValues == !!pPatchKeyValues );

		// Don't bother if we're already precached
		if( IsPrecachedVars_Inline() )
			return true;

		return PrecacheVars_Internal( pKeyValues, pPatchKeyValues, pIncludes );
	}
	inline void	Precache_Inline()
	{
		// Don't bother if we're already precached
		if ( IsPrecached_Inline() )
			return;

		// load data from the vmt file
		if ( !PrecacheVars_Inline() )
			return;

		Precache_Internal();
	}
	void	Precache()
	{
		Precache_Inline();
	}

	bool PrecacheVars( KeyValues *pKeyValues = NULL, KeyValues *pPatchKeyValues = NULL, CUtlVector<FileNameHandle_t> *pIncludes = NULL )
	{
		return PrecacheVars_Inline( pKeyValues, pPatchKeyValues, pIncludes );
	}

	void		SetMinLightmapPageID( int pageID );
	void		SetMaxLightmapPageID( int pageID );
	int			GetMinLightmapPageID( ) const;
	int			GetMaxLightmapPageID( ) const;
	void		SetNeedsWhiteLightmap( bool val );
	bool		GetNeedsWhiteLightmap( ) const;
	IShader *	GetShader() const;
	const char *GetShaderName() const;
	
	virtual void			DeleteIfUnreferenced();

	void		SetEnumerationID( int id );
	void		CallBindProxy( void *proxyData, ICallQueue *pCallQueue );
	bool		HasProxy( void ) const;

	// Sets the shader associated with the material
	void SetShader( const char *pShaderName );

	// Can we override this material in debug?
	bool NoDebugOverride() const;

	// Gets the vertex format
	VertexFormat_t GetVertexFormat() const;
	
	// diffuse bump lightmap?
	bool IsUsingDiffuseBumpedLighting() const;

	// lightmap?
	bool IsUsingLightmap() const;

	// Gets the vertex usage flags
	VertexFormat_t GetVertexUsage() const;

	// Debugs this material
	bool PerformDebugTrace() const;

	// Are we suppressed?
	bool IsSuppressed() const;

	// Do we use fog?
	bool UseFog( void ) const;
	
	// Should we draw?
	void ToggleSuppression();
	void ToggleDebugTrace();
	
	// Refresh material based on current var values
	void Refresh();
	void RefreshPreservingMaterialVars();

	// This computes the state snapshots for this material
	void RecomputeStateSnapshots();

	// Gets at the shader parameters
	virtual int ShaderParamCount() const;
	virtual IMaterialVar **GetShaderParams( void );

	virtual void AddMaterialVar( IMaterialVar *pMaterialVar );

	virtual bool IsErrorMaterial() const;

	// Was this manually created (not read from a file?)
	virtual bool IsManuallyCreated() const;

	virtual bool NeedsFixedFunctionFlashlight() const;

	virtual void MarkAsPreloaded( bool bSet );
	virtual bool IsPreloaded() const;

	virtual void ArtificialAddRef( void );
	virtual void ArtificialRelease( void );

	virtual void ReportVarChanged( IMaterialVar *pVar )
	{	
		m_ChangeID++;
	}
	virtual void ClearContextData( void );

	virtual uint32 GetChangeID() const { return m_ChangeID; }
	virtual uint32 GetChangeTimestamp() const { return m_ChangeID ^ g_nDebugVarsSignature; }

	virtual bool IsRealTimeVersion( void ) const { return true; }
	virtual IMaterialInternal *GetRealTimeVersion( void ) { return this; }
	virtual IMaterialInternal *GetQueueFriendlyVersion( void ) { return &m_QueueFriendlyVersion; }

	void DecideShouldReloadFromWhitelist( IFileList *pFilesToReload );
	void ReloadFromWhitelistIfMarked();
	bool WasReloadedFromWhitelist();

	virtual void CompactMaterialVars();

	// Are any of the proxies attached to this material callable from the queued thread?
	virtual bool HasQueueFriendlyProxies() const OVERRIDE;

	virtual bool SetTempExcluded( bool bSet, int nExcludedDimensionLimit );

	virtual int	GetReferenceCount() const;

private:
	// Initializes, cleans up the shader params
	void CleanUpShaderParams();

	// Sets up an error shader when we run into problems.
	void SetupErrorShader();

	// Does this material have a UNC-file name?
	bool UsesUNCFileName() const;

	// Prints material flags.
	void PrintMaterialFlags( int flags, int flagsDefined );

	// Parses material flags
	bool ParseMaterialFlag( KeyValues* pParseValue, IMaterialVar* pFlagVar,
		IMaterialVar* pFlagDefinedVar, bool parsingOverrides, int& flagMask, int& overrideMask );

	// Computes the material vars for the shader
	int ParseMaterialVars( IShader* pShader, KeyValues& keyValues, 
		KeyValues* pOverride, bool modelDefault, IMaterialVar** ppVars );

	// Figures out the preview image for worldcraft
	char const*	GetPreviewImageName( );
	char const* GetPreviewImageFileName( void ) const;

	// Hooks up the shader, returns keyvalues of fallback that was used
	KeyValues* InitializeShader( KeyValues &keyValues, KeyValues &patchKeyValues );

	// Finds the flag associated with a particular flag name
	int FindMaterialVarFlag( char const* pFlagName ) const;

	// Initializes, cleans up the state snapshots
	bool InitializeStateSnapshots();
	void CleanUpStateSnapshots();

	// Initializes, cleans up the material proxy
	void InitializeMaterialProxy( KeyValues* pFallbackKeyValues );
	void CleanUpMaterialProxy();

	// Creates, destroys snapshots
	RenderPassList_t *CreateRenderPassList();
	void DestroyRenderPassList( RenderPassList_t *pPassList );

	// Grabs the texture width and height from the var list for faster access
	void PrecacheMappingDimensions( );

	// Gets the renderstate
	virtual ShaderRenderState_t *GetRenderState();

	// Do we have a valid renderstate?
	bool IsValidRenderState() const;

	// Get the material var flags
	int	GetMaterialVarFlags() const;
	void SetMaterialVarFlags( int flags, bool on );
	int	GetMaterialVarFlags2() const;
	void SetMaterialVarFlags2( int flags, bool on );

	// Returns a dummy material variable
	IMaterialVar*	GetDummyVariable();

	IMaterialVar*	GetShaderParam( int id );

	void			FindRepresentativeTexture( void );

	bool ShouldSkipVar( KeyValues *pMaterialVar, bool * pWasConditional );


	// Fixed-size allocator
	DECLARE_FIXEDSIZE_ALLOCATOR( CMaterial );

private:
	enum
	{
		MATERIAL_NEEDS_WHITE_LIGHTMAP = 0x1,
		MATERIAL_IS_PRECACHED = 0x2,
		MATERIAL_VARS_IS_PRECACHED = 0x4,
		MATERIAL_VALID_RENDERSTATE = 0x8,
		MATERIAL_IS_MANUALLY_CREATED = 0x10,
		MATERIAL_USES_UNC_FILENAME = 0x20,
		MATERIAL_IS_PRELOADED = 0x40,
		MATERIAL_ARTIFICIAL_REFCOUNT = 0x80,
	};

	int					m_iEnumerationID;
	
	int					m_minLightmapPageID;
	int					m_maxLightmapPageID;

	unsigned short		m_MappingWidth;
	unsigned short		m_MappingHeight;
	
	IShader				*m_pShader;

	CUtlSymbol			m_Name;
	// Any textures created for this material go under this texture group.
	CUtlSymbol			m_TextureGroupName;

	CInterlockedInt		m_RefCount;
	unsigned short		m_Flags;

	unsigned char		m_VarCount;
	unsigned char		m_ProxyCount;

	IMaterialVar**		m_pShaderParams;
	IMaterialProxy**	m_ppProxies;
	ShaderRenderState_t m_ShaderRenderState;

	// This remembers filenames of VMTs that we included so we can sv_pure/flush ourselves if any of them need to be reloaded.
	CUtlVector<FileNameHandle_t> m_VMTIncludes;
	bool m_bShouldReloadFromWhitelist;	// Tells us if the material decided it should be reloaded due to sv_pure whitelist changes.

	ITextureInternal	*m_representativeTexture;
	Vector				m_Reflectivity;
	uint32				m_ChangeID;

	// Used only by procedural materials; it essentially is an in-memory .VMT file
	KeyValues			*m_pVMTKeyValues;

#ifdef _DEBUG
	// Makes it easier to see what's going on
	char				*m_pDebugName;
#endif

protected:
	CMaterial_QueueFriendly m_QueueFriendlyVersion;
};


// NOTE: This must be the last file included
// Has to exist *after* fixed size allocator declaration
#include "tier0/memdbgon.h"

static ConVar mat_processtoolvars( "mat_processtoolvars", "0" ); // 0 = Ignore tool material variables (e.g. %foo) at runtime (default)
                                                                 // 1 = Process tool material variables at runtime (required for dynamic level builds via VBSP2LIB)
// Forward decls of helper functions for dealing with patch vmts.
static void ApplyPatchKeyValues( KeyValues &keyValues, KeyValues &patchKeyValues );
static bool AccumulateRecursiveVmtPatches( KeyValues &patchKeyValuesOut, KeyValues **ppBaseKeyValuesOut,
										   const KeyValues& keyValues, const char *pPathID, CUtlVector<FileNameHandle_t> *pIncludes );


//-----------------------------------------------------------------------------
// Parser utilities
//-----------------------------------------------------------------------------
static inline bool IsWhitespace( char c )
{
	return c == ' ' || c == '\t';
}

static inline bool IsEndline( char c )
{
	return c == '\n' || c == '\0';
}

static inline bool IsVector( char const* v )
{
	while (IsWhitespace(*v))
	{
		++v;
		if (IsEndline(*v))
			return false;
	}
	return *v == '[' || *v == '{';
}


//-----------------------------------------------------------------------------
// Methods to create state snapshots
//-----------------------------------------------------------------------------
#include "tier0/memdbgoff.h"

#ifndef _GAMECONSOLE
struct EditorRenderStateList_t
{
	// Store combo of alpha, color, fixed-function baked lighting, flashlight, editor mode
	RenderPassList_t m_Snapshots[SNAPSHOT_COUNT_GBUFFER];

	DECLARE_FIXEDSIZE_ALLOCATOR( EditorRenderStateList_t );
};
#endif

struct StandardRenderStateList_t
{
	// Store combo of alpha, color, fixed-function baked lighting, flashlight
	RenderPassList_t m_Snapshots[SNAPSHOT_COUNT_NORMAL];

	DECLARE_FIXEDSIZE_ALLOCATOR( StandardRenderStateList_t );
};

#include "tier0/memdbgon.h"

#ifndef _GAMECONSOLE
DEFINE_FIXEDSIZE_ALLOCATOR( EditorRenderStateList_t, 256, true );
#endif
DEFINE_FIXEDSIZE_ALLOCATOR( StandardRenderStateList_t, 256, true );


//-----------------------------------------------------------------------------
// class factory methods
//-----------------------------------------------------------------------------
DEFINE_FIXEDSIZE_ALLOCATOR( CMaterial, 256, true );

IMaterialInternal* IMaterialInternal::CreateMaterial( char const* pMaterialName, const char *pTextureGroupName, KeyValues *pVMTKeyValues )
{
	MaterialLock_t hMaterialLock = MaterialSystem()->Lock();
	IMaterialInternal *pResult = new CMaterial( pMaterialName, pTextureGroupName, pVMTKeyValues );
	MaterialSystem()->Unlock( hMaterialLock );
	return pResult;
}

void IMaterialInternal::DestroyMaterial( IMaterialInternal* pMaterial )
{
	MaterialLock_t hMaterialLock = MaterialSystem()->Lock();
	if (pMaterial)
	{
		Assert( pMaterial->IsRealTimeVersion() );
		CMaterial* pMatImp = static_cast<CMaterial*>(pMaterial);
		// deletion of the error material is deferred until after all other materials have been deleted.
		if ( !pMatImp->IsErrorMaterial() )
		{
			delete pMatImp;
		}
	}
	MaterialSystem()->Unlock( hMaterialLock );
}

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CMaterial::CMaterial( char const* materialName, const char *pTextureGroupName, KeyValues *pKeyValues )
{
	m_Reflectivity.Init( 0.2f, 0.2f, 0.2f );
	int len = Q_strlen(materialName);
	char* pTemp = (char*)stackalloc( len + 1 );

	// Strip off the extension
	Q_StripExtension( materialName, pTemp, len+1 );
	Q_strlower( pTemp );

#if defined( _X360 )
	// material names are expected to be forward slashed for correct sort and find behavior!
	// assert now to track alternate or regressed path that is source of inconsistency
	Assert( strchr( pTemp, '\\' ) == NULL );
#endif

	// Convert it to a symbol
	m_Name = pTemp;

#ifdef _DEBUG
	m_pDebugName = new char[strlen(pTemp) + 1];
	Q_strncpy( m_pDebugName, pTemp, strlen(pTemp) + 1 );
#endif

	m_bShouldReloadFromWhitelist = false;
	m_Flags = 0;
	m_pShader = NULL;
	m_pShaderParams = NULL;
	m_RefCount = 0;
	m_representativeTexture = NULL;
	m_ppProxies = NULL;
	m_ProxyCount = 0;
	m_VarCount = 0;
	m_MappingWidth = m_MappingHeight = 0;
	m_iEnumerationID = 0;
	m_minLightmapPageID = m_maxLightmapPageID = 0;
	m_TextureGroupName = pTextureGroupName;
	m_pVMTKeyValues = pKeyValues;
	if (m_pVMTKeyValues)
	{
		m_Flags |= MATERIAL_IS_MANUALLY_CREATED; 
	}

	if ( pTemp[0] == '/' && pTemp[1] == '/' && pTemp[2] != '/' )
	{
		m_Flags |= MATERIAL_USES_UNC_FILENAME;
	}

	// Initialize the renderstate to something indicating nothing should be drawn
	m_ShaderRenderState.m_Flags = 0;
	m_ShaderRenderState.m_VertexFormat = m_ShaderRenderState.m_VertexUsage = 0;
	m_ShaderRenderState.m_pSnapshots = CreateRenderPassList(); 
	m_ChangeID = 0;

	m_QueueFriendlyVersion.SetRealTimeVersion( this );
}

CMaterial::~CMaterial()
{
	MaterialSystem()->UnbindMaterial( this );

	Uncache();

#if defined( DEVELOPMENT_ONLY ) || defined( ALLOW_TEXT_MODE )
	static bool s_bTextMode = CommandLine()->HasParm( "-textmode" );
#else
	const bool s_bTextMode = false;
#endif

	if ( m_RefCount != 0 && !s_bTextMode )
	{
	    Warning( "Reference Count for Material %s (%d) != 0\n", GetName(), (int) m_RefCount );
	}

	if ( m_pVMTKeyValues )
	{
		m_pVMTKeyValues->deleteThis();
		m_pVMTKeyValues = NULL;
	}

	DestroyRenderPassList( m_ShaderRenderState.m_pSnapshots ); 

	m_QueueFriendlyVersion.SetRealTimeVersion( NULL );
	m_representativeTexture = NULL;

#ifdef _DEBUG
	delete[] m_pDebugName;
#endif

	// Deliberately stomp our VTable so that we can detect cases where code tries to access freed materials.
	int *p = (int *)this;
	*p = 0xc0dedbad;
}


void CMaterial::ClearContextData( void )
{
	int nSnapshotCount = SnapshotTypeCount();
	for( int i = 0 ; i < nSnapshotCount ; i++ )
	{
		RenderPassList_t &renderPassList = m_ShaderRenderState.m_pSnapshots[i];
		for( int j = 0 ; j < renderPassList.m_nPassCount; j++ )
		{
			if ( renderPassList.m_pContextData[j] )
			{
				delete renderPassList.m_pContextData[j];
				renderPassList.m_pContextData[j] = NULL;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Sets new VMT shader parameters for the material
//-----------------------------------------------------------------------------
void CMaterial::SetShaderAndParams( KeyValues *pKeyValues )
{
	Uncache();

	if ( m_pVMTKeyValues )
	{
		m_pVMTKeyValues->deleteThis();
		m_pVMTKeyValues = NULL;
	}

	m_pVMTKeyValues = pKeyValues ? pKeyValues->MakeCopy() : NULL;
	if ( m_pVMTKeyValues )
	{
		m_Flags |= MATERIAL_IS_MANUALLY_CREATED; 
	}

	// Apply patches
	const char *pMaterialName = GetName();
	char pFileName[MAX_PATH];
	const char *pPathID = "GAME";
	if ( !UsesUNCFileName() )
	{
		Q_snprintf( pFileName, sizeof( pFileName ), "materials/%s.vmt", pMaterialName );
	}
	else
	{
		Q_snprintf( pFileName, sizeof( pFileName ), "%s.vmt", pMaterialName );
		if ( pMaterialName[0] == '/' && pMaterialName[1] == '/' && pMaterialName[2] != '/' )
		{
			// UNC, do full search
			pPathID = NULL;
		}
	}

	KeyValues *pLoadedKeyValues = new KeyValues( "vmt" );
	if ( pLoadedKeyValues->LoadFromFile( g_pFullFileSystem, pFileName, pPathID ) )
	{
		// Load succeeded, check if it's a patch file
		if ( V_stricmp( pLoadedKeyValues->GetName(), "patch" ) == 0 )
		{
			// it's a patch file, recursively build up patch keyvalues
			KeyValues *pPatchKeyValues = new KeyValues( "vmt_patch" );
			bool bSuccess = AccumulateRecursiveVmtPatches( *pPatchKeyValues, NULL, *pLoadedKeyValues, pPathID, NULL );
			if ( bSuccess )
			{
				// Apply accumulated patches to final vmt
				ApplyPatchKeyValues( *m_pVMTKeyValues, *pPatchKeyValues );
			}
			pPatchKeyValues->deleteThis();
		}
	}
	pLoadedKeyValues->deleteThis();

	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		Precache();
	}
}


//-----------------------------------------------------------------------------
// Creates, destroys snapshots
//-----------------------------------------------------------------------------
RenderPassList_t *CMaterial::CreateRenderPassList()
{
	RenderPassList_t *pRenderPassList;
	if ( IsGameConsole() || 
		 !( MaterialSystem()->GetConfigurationFlags() & 
			( MATCONFIG_FLAGS_SUPPORT_GBUFFER | MATCONFIG_FLAGS_SUPPORT_EDITOR ) ) )
	{
		StandardRenderStateList_t *pList = new StandardRenderStateList_t;
		pRenderPassList = (RenderPassList_t*)pList->m_Snapshots;
	}
#ifndef _GAMECONSOLE
	else
	{
		EditorRenderStateList_t *pList = new EditorRenderStateList_t;
		pRenderPassList = (RenderPassList_t*)pList->m_Snapshots;
	}
#endif

	int nSnapshotCount = SnapshotTypeCount();
	memset( pRenderPassList, 0, nSnapshotCount * sizeof(RenderPassList_t) );
	return pRenderPassList;
}

void CMaterial::DestroyRenderPassList( RenderPassList_t *pPassList )
{
	if ( !pPassList )
		return;

	int nSnapshotCount = SnapshotTypeCount();
	for( int i = 0 ; i < nSnapshotCount ; i++ )
	{
		for( int j = 0 ; j < pPassList[i].m_nPassCount; j++ )
		{
			if ( pPassList[i].m_pContextData[j] )
			{
				delete pPassList[i].m_pContextData[j];
				pPassList[i].m_pContextData[j] = NULL;
			}
			if ( pPassList[i].m_pInstanceData[j] )
			{
				delete pPassList[i].m_pInstanceData[j];
				pPassList[i].m_pInstanceData[j] = NULL;
			}
		}
	}
	if ( IsGameConsole() || 
		 !( MaterialSystem()->GetConfigurationFlags() & 
			( MATCONFIG_FLAGS_SUPPORT_GBUFFER | MATCONFIG_FLAGS_SUPPORT_EDITOR ) ) )
	{
		StandardRenderStateList_t *pList = (StandardRenderStateList_t*)pPassList;
		delete pList;
	}
#ifndef _GAMECONSOLE
	else
	{
		EditorRenderStateList_t *pList = (EditorRenderStateList_t*)pPassList;
		delete pList;
	}
#endif
}

	
//-----------------------------------------------------------------------------
// Gets the renderstate
//-----------------------------------------------------------------------------
ShaderRenderState_t *CMaterial::GetRenderState()
{
	Precache_Inline();
	return &m_ShaderRenderState;
}


//-----------------------------------------------------------------------------
// Returns a dummy material variable
//-----------------------------------------------------------------------------
IMaterialVar* CMaterial::GetDummyVariable()
{
	static IMaterialVar* pDummyVar = 0;
	if (!pDummyVar)
		pDummyVar = IMaterialVar::Create( 0, "$dummyVar", 0 );

	return pDummyVar;
}


//-----------------------------------------------------------------------------
// Cleans up shader parameters
//-----------------------------------------------------------------------------
void CMaterial::CleanUpShaderParams()
{
	if( m_pShaderParams )
	{
		for (int i = 0; i < m_VarCount; ++i)
		{
			IMaterialVar::Destroy( m_pShaderParams[i] );
		}

		free( m_pShaderParams );
		m_pShaderParams = 0;
	}
	m_VarCount = 0;
}


//-----------------------------------------------------------------------------
// Initializes the material proxy
//-----------------------------------------------------------------------------
void CMaterial::InitializeMaterialProxy( KeyValues* pFallbackKeyValues )
{
	IMaterialProxyFactory *pMaterialProxyFactory;
	pMaterialProxyFactory = MaterialSystem()->GetMaterialProxyFactory();	
	if( !pMaterialProxyFactory )
		return;

	// See if we've got a proxy section; obey fallbacks
	KeyValues* pProxySection = pFallbackKeyValues->FindKey("Proxies");
	if (!pProxySection)
		return;

	// Iterate through the section + create all of the proxies
	int proxyCount = 0;
	IMaterialProxy* ppProxies[256];
	KeyValues* pProxyKey = pProxySection->GetFirstSubKey();
	for ( ; pProxyKey; pProxyKey = pProxyKey->GetNextKey() )
	{
		// Each of the proxies should themselves be databases
		IMaterialProxy* pProxy = pMaterialProxyFactory->CreateProxy( pProxyKey->GetName() );
		if (!pProxy)
		{
			Warning( "Error: Material \"%s\" : proxy \"%s\" not found!\n", GetName(), pProxyKey->GetName() );
			continue;
		}

		if (!pProxy->Init( this->GetQueueFriendlyVersion(), pProxyKey ))
		{
			pMaterialProxyFactory->DeleteProxy( pProxy );
			Warning( "Error: Material \"%s\" : proxy \"%s\" unable to initialize!\n", GetName(), pProxyKey->GetName() );
		}
		else
		{
			ppProxies[proxyCount] = pProxy;
			++proxyCount;
			if ( proxyCount >= ARRAYSIZE( ppProxies ) )
			{
				Warning( "Error: Material \"%s\" has more than %lu proxies!\n", GetName(), ( unsigned long )ARRAYSIZE( ppProxies ) );
				break;
			}
		}
	}

	// Allocate space for the number of proxies we successfully made...
	m_ProxyCount = proxyCount;
	if (proxyCount)
	{
		m_ppProxies = (IMaterialProxy**)malloc( proxyCount * sizeof(IMaterialProxy*) );
		memcpy( m_ppProxies, ppProxies, proxyCount * sizeof(IMaterialProxy*) );
	}
	else
	{
		m_ppProxies = 0;
	}
}


//-----------------------------------------------------------------------------
// Cleans up the material proxy
//-----------------------------------------------------------------------------
void CMaterial::CleanUpMaterialProxy()
{
	if ( !m_ProxyCount )
		return;

	IMaterialProxyFactory *pMaterialProxyFactory;
	pMaterialProxyFactory = MaterialSystem()->GetMaterialProxyFactory();	
	if ( !pMaterialProxyFactory )
		return;

	// Clean up material proxies
	for ( int i = m_ProxyCount; --i >= 0; )
	{
		pMaterialProxyFactory->DeleteProxy( m_ppProxies[i] );
	}
	free( m_ppProxies );

	m_ppProxies = NULL;
	m_ProxyCount = 0;
}


static char const *GetVarName( KeyValues *pVar )
{
	char const *pVarName = pVar->GetName();
	char const *pQuestion = strchr( pVarName, '?' );
	if (! pQuestion )
		return pVarName;
	else
		return pQuestion + 1;
}

//-----------------------------------------------------------------------------
// Finds the index of the material var associated with a var
//-----------------------------------------------------------------------------
static int FindMaterialVar( IShader* pShader, char const* pVarName )
{
	if ( !pShader )
		return -1;

	// Strip preceeding spaces
	pVarName += strspn( pVarName, " \t" );

	for (int i = pShader->GetParamCount(); --i >= 0; )
	{
		// Makes the parser a little more lenient.. strips off bogus spaces in the var name.
		const char *pParamName = pShader->GetParamInfo(i).m_pName;
		const char *pFound = Q_stristr( pVarName, pParamName );

		// The found string had better start with the first non-whitespace character
		if ( pFound != pVarName )
			continue;

		// Strip spaces at the end
		int nLen = Q_strlen( pParamName );
		pFound += nLen;
		while ( true )
		{
			if ( !pFound[0] )
				return i;

			if ( !IsWhitespace( pFound[0] ) )
				break;

			++pFound;
		}
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Creates a vector material var
//-----------------------------------------------------------------------------
int ParseVectorFromKeyValueString( KeyValues *pKeyValue, const char *pMaterialName, float vecVal[4] )
{
	char const* pScan = pKeyValue->GetString();
	bool divideBy255 = false;

	// skip whitespace
	while( IsWhitespace(*pScan) )
	{
		++pScan;
	}

	if( *pScan == '{' )
	{
		divideBy255 = true;
	}
	else
	{
		Assert( *pScan == '[' );
	}
	
	// skip the '['
	++pScan;
	int i;
	for( i = 0; i < 4; i++ )
	{
		// skip whitespace
		while( IsWhitespace(*pScan) )
		{
			++pScan;
		}

		if( IsEndline(*pScan) || *pScan == ']' || *pScan == '}' )
		{
			if (*pScan != ']' && *pScan != '}')
			{
				Warning( "Warning in .VMT file (%s): no ']' or '}' found in vector key \"%s\".\n"
					"Did you forget to surround the vector with \"s?\n", pMaterialName, pKeyValue->GetName() );
			}

			// allow for vec2's, etc.
			vecVal[i] = 0.0f;
			break;
		}

		char* pEnd;

		vecVal[i] = strtod( pScan, &pEnd );
		if (pScan == pEnd)
		{
			Warning( "Error in .VMT file: error parsing vector element \"%s\" in \"%s\"\n", pKeyValue->GetName(), pMaterialName );
			return 0;
		}

		pScan = pEnd;
	}

	if( divideBy255 )
	{
		vecVal[0] *= ( 1.0f / 255.0f );
		vecVal[1] *= ( 1.0f / 255.0f );
		vecVal[2] *= ( 1.0f / 255.0f );
		vecVal[3] *= ( 1.0f / 255.0f );
	}

	return i;
}

static IMaterialVar* CreateVectorMaterialVarFromKeyValue( IMaterial* pMaterial, KeyValues* pKeyValue )
{
	char const *pszName = GetVarName( pKeyValue );
	float vecVal[4];
	int nDim = ParseVectorFromKeyValueString( pKeyValue, pszName, vecVal );
	if ( nDim == 0 )
		return NULL;

	// Create the variable!
	return IMaterialVar::Create( pMaterial, pszName, vecVal, nDim );
}

//-----------------------------------------------------------------------------
// Creates a vector material var
//-----------------------------------------------------------------------------
static IMaterialVar* CreateMatrixMaterialVarFromKeyValue( IMaterial* pMaterial, KeyValues* pKeyValue )
{
	char const* pScan = pKeyValue->GetString();
	char const *pszName = GetVarName( pKeyValue );

	// Matrices can be specified one of two ways:
	// [ # # # #  # # # #  # # # #  # # # # ]
	// or
	// center # # scale # # rotate # translate # #

	VMatrix mat;
	int count = sscanf( pScan, " [ %f %f %f %f  %f %f %f %f  %f %f %f %f  %f %f %f %f ]",
		&mat.m[0][0], &mat.m[0][1], &mat.m[0][2], &mat.m[0][3],
		&mat.m[1][0], &mat.m[1][1], &mat.m[1][2], &mat.m[1][3],
		&mat.m[2][0], &mat.m[2][1], &mat.m[2][2], &mat.m[2][3],
		&mat.m[3][0], &mat.m[3][1], &mat.m[3][2], &mat.m[3][3] );
	if (count == 16)
	{
		return IMaterialVar::Create( pMaterial, pszName, mat );
	}

	Vector2D scale, center;
	float angle;
	Vector2D translation;

	//scan for pre-rotation scale and translation with assumed center syntax
	count = sscanf( pScan, " scale %f %f translate %f %f rotate %f",
		&scale.x, &scale.y, &translation.x, &translation.y, &angle );
	if (count == 5)
	{
		VMatrix temp;

		MatrixBuildTranslation( mat, translation.x - 0.5, translation.y - 0.5, 0.0f );
		MatrixBuildScale( temp, scale.x, scale.y, 1.0f );
		MatrixMultiply( mat, temp, mat );
		MatrixBuildRotateZ( temp, angle );
		MatrixMultiply( mat, temp, mat );

		Vector2D vOffset;
		vOffset.Init( 0.5f / ( scale.x != 0 ? scale.x : 1.0 ), 0.5f / ( scale.y != 0 ? scale.y : 1.0 ) );
		Vector2DRotate( vOffset, -angle, vOffset );

		MatrixBuildTranslation( temp, vOffset.x, vOffset.y, 0.0f );
		MatrixMultiply( mat, temp, mat );

		return IMaterialVar::Create( pMaterial, pszName, mat );
	}

	count = sscanf( pScan, " center %f %f scale %f %f rotate %f translate %f %f",
		&center.x, &center.y, &scale.x, &scale.y, &angle, &translation.x, &translation.y );
	if (count != 7)
		return NULL;

	VMatrix temp;
	MatrixBuildTranslation( mat, -center.x, -center.y, 0.0f );
	MatrixBuildScale( temp, scale.x, scale.y, 1.0f );
	MatrixMultiply( temp, mat, mat );
	MatrixBuildRotateZ( temp, angle );
	MatrixMultiply( temp, mat, mat );
	MatrixBuildTranslation( temp, center.x + translation.x, center.y + translation.y, 0.0f );
	MatrixMultiply( temp, mat, mat );

	// Create the variable!
	return IMaterialVar::Create( pMaterial, pszName, mat );
}


//-----------------------------------------------------------------------------
// Creates a material var from a key value
//-----------------------------------------------------------------------------

static IMaterialVar* CreateMaterialVarFromKeyValue( IMaterial* pMaterial, KeyValues* pKeyValue )
{
	char const *pszName = GetVarName( pKeyValue );
	switch( pKeyValue->GetDataType() )
	{
		case KeyValues::TYPE_INT:
			return IMaterialVar::Create( pMaterial, pszName, pKeyValue->GetInt() );
			
		case KeyValues::TYPE_FLOAT:
			return IMaterialVar::Create( pMaterial, pszName, pKeyValue->GetFloat() );
			
		case KeyValues::TYPE_STRING:
		{
			char const* pString = pKeyValue->GetString();
			if (!pString || !pString[0])
				return 0;
			
			// Look for matrices
			IMaterialVar *pMatrixVar = CreateMatrixMaterialVarFromKeyValue( pMaterial, pKeyValue );
			if (pMatrixVar)
				return pMatrixVar;
			
			// Look for vectors
			if (!IsVector(pString))
				return IMaterialVar::Create( pMaterial, pszName, pString );
			
			// Parse the string as a vector...
			return CreateVectorMaterialVarFromKeyValue( pMaterial, pKeyValue );
		}
	}
	
	return 0;
}


//-----------------------------------------------------------------------------
// Reads out common flags, prevents them from becoming material vars
//-----------------------------------------------------------------------------
int CMaterial::FindMaterialVarFlag( char const* pFlagName ) const
{
	// Strip preceeding spaces
	while ( pFlagName[0] )
	{
		if ( !IsWhitespace( pFlagName[0] ) )
			break;

		++pFlagName;
	}

	for( int i = 0; *ShaderSystem()->ShaderStateString(i); ++i )
	{
		const char *pStateString = ShaderSystem()->ShaderStateString(i);
		const char *pFound = Q_stristr( pFlagName, pStateString );

		// The found string had better start with the first non-whitespace character
		if ( pFound != pFlagName )
			continue;

		// Strip spaces at the end
		int nLen = Q_strlen( pStateString );
		pFound += nLen;
		while ( true )
		{
			if ( !pFound[0] )
				return (1 << i);

			if ( !IsWhitespace( pFound[0] ) )
				break;

			++pFound;
		}
	}
	return 0;
}


//-----------------------------------------------------------------------------
// Print material flags
//-----------------------------------------------------------------------------
void CMaterial::PrintMaterialFlags( int flags, int flagsDefined )
{
	int i;
	for( i = 0; *ShaderSystem()->ShaderStateString(i); i++ )
	{
		if( flags & ( 1<<i ) )
		{
			Warning( "%s|", ShaderSystem()->ShaderStateString(i) );
		}
	}
	Warning( "\n" );
}


//-----------------------------------------------------------------------------
// Parses material flags
//-----------------------------------------------------------------------------
bool CMaterial::ParseMaterialFlag( KeyValues* pParseValue, IMaterialVar* pFlagVar,
	IMaterialVar* pFlagDefinedVar, bool parsingOverrides, int& flagMask, int& overrideMask )
{
	// See if the var is a flag...
	int flagbit = FindMaterialVarFlag( GetVarName( pParseValue ) );
	if (!flagbit)
		return false;

	// Allow for flag override
	int testMask = parsingOverrides ? overrideMask : flagMask;
	if (testMask & flagbit)
	{
		Warning("Error! Flag \"%s\" is multiply defined in material \"%s\"!\n", pParseValue->GetName(), GetName() );
		return true;
	}

	// Make sure overrides win
	if (overrideMask & flagbit)
		return true;

	if (parsingOverrides)
		overrideMask |= flagbit;
	else
		flagMask |= flagbit;

	// If so, then set the flag bit
	if (pParseValue->GetInt())
		pFlagVar->SetIntValue( pFlagVar->GetIntValue() | flagbit );
	else
		pFlagVar->SetIntValue( pFlagVar->GetIntValue() & (~flagbit) );

	// Mark the flag as being defined
	pFlagDefinedVar->SetIntValue( pFlagDefinedVar->GetIntValue() | flagbit );

/*
	if( stristr( m_pDebugName, "glasswindow064a" ) )
	{
		Warning( "flags\n" );
		PrintMaterialFlags( pFlagVar->GetIntValue(), pFlagDefinedVar->GetIntValue() );
	}
*/
	
	return true;
}


ConVar mat_reduceparticles( "mat_reduceparticles",  "0" );

extern ConVar gpu_level;
bool CMaterial::ShouldSkipVar( KeyValues *pVar, bool *pWasConditional )
{
	char const *pVarName = pVar->GetName();
	char const *pQuestion = strchr( pVarName, '?' );
	if ( ( ! pQuestion ) || (pQuestion == pVarName ) )
	{
		*pWasConditional = false;							// unconditional var
		return false;
	}
	else
	{
		bool bShouldSkip = true;
		*pWasConditional = true;
		// parse the conditional part
		char pszConditionName[256];
		V_strncpy( pszConditionName, pVarName, 1+pQuestion-pVarName );
		char const *pCond = pszConditionName;
		bool bToggle = false;
		if ( pCond[0] == '!' )
		{
			pCond++;
			bToggle = true;
		}

		if ( ! stricmp( pCond, "lowfill" ) )
		{
			bShouldSkip = !mat_reduceparticles.GetBool();
		}
		else if ( ! stricmp( pCond, "hdr" ) )
		{
			bShouldSkip = ( HardwareConfig()->GetHDRType() == HDR_TYPE_NONE );
		}
		else if ( ! stricmp( pCond, "srgb" ) )
		{
			bShouldSkip = ( !HardwareConfig()->UsesSRGBCorrectBlending() );
		}
		else if ( ! stricmp( pCond, "srgb_gameconsole" ) )
		{
			bShouldSkip = !( HardwareConfig()->UsesSRGBCorrectBlending() && IsGameConsole() );
		}
		else if ( ! stricmp( pCond, "srgb_pc" ) )
		{
			bShouldSkip = !( HardwareConfig()->UsesSRGBCorrectBlending() && IsPC() );
		}
		else if ( ! stricmp( pCond, "ldr" ) )
		{
			bShouldSkip = ( HardwareConfig()->GetHDRType() != HDR_TYPE_NONE );
		}
		else if ( ! stricmp( pCond, "GPU>=3" ) )
		{
			bShouldSkip = gpu_level.GetInt() < 3;
		}
		else if ( ! stricmp( pCond, "GPU>=2" ) )
		{
			bShouldSkip = gpu_level.GetInt() < 2;
		}
		else if ( ! stricmp( pCond, "GPU>=1" ) )
		{
			bShouldSkip = gpu_level.GetInt() < 1;
		}
		else if ( ! stricmp( pCond, "GPU<2" ) )
		{
			bShouldSkip = gpu_level.GetInt() >= 2;
		}
		else if ( ! stricmp( pCond, "GPU<3" ) )
		{
			bShouldSkip = gpu_level.GetInt() >= 3;
		}
		else if ( ! stricmp( pCond, "GPU<1" ) )
		{
			bShouldSkip = gpu_level.GetInt() >= 1;
		}
		else if ( ! stricmp( pCond, "360" ) )
		{
			bShouldSkip = !IsX360();
		}
		else if ( ! stricmp( pCond, "SonyPS3" ) )
		{
			bShouldSkip = !IsPS3();
		}
		else if ( ! stricmp( pCond, "gameconsole" ) )
		{
			bShouldSkip = !IsGameConsole();
		}
		else if ( ! stricmp( pCond, "LowQualityCSM" ) )
		{
			bShouldSkip = HardwareConfig()->SupportsBilinearPCFSampling() && ( materials->GetCurrentConfigForVideoCard().GetCSMQualityMode() > CSMQUALITY_LOW );
		}
		else if ( ! stricmp( pCond, "HighQualityCSM" ) )
		{
			bShouldSkip = !HardwareConfig()->SupportsBilinearPCFSampling() || ( materials->GetCurrentConfigForVideoCard().GetCSMQualityMode() < CSMQUALITY_HIGH );
		}
		else
		{
			Warning( "unrecognized conditional test %s in %s\n", pVarName, GetName() );
		}

		return bShouldSkip ^ bToggle;
	}
}


//-----------------------------------------------------------------------------
// Computes the material vars for the shader
//-----------------------------------------------------------------------------
int CMaterial::ParseMaterialVars( IShader* pShader, KeyValues& keyValues, 
			KeyValues* pOverrideKeyValues, bool modelDefault, IMaterialVar** ppVars )
{
	IMaterialVar* pNewVar;
	bool pOverride[256];
	bool bWasConditional[256];
	int overrideMask = 0;
	int flagMask = 0;

	memset( ppVars, 0, 256 * sizeof(IMaterialVar*) );
	memset( pOverride, 0, sizeof( pOverride ) );
	memset( bWasConditional, 0, sizeof( bWasConditional ) );

	// Create the flag var...
	// Set model mode if we fell back from a model mode shader
	int modelFlag = modelDefault ? MATERIAL_VAR_MODEL : 0;
	ppVars[FLAGS] = IMaterialVar::Create( this, "$flags", modelFlag );
	ppVars[FLAGS_DEFINED] = IMaterialVar::Create( this, "$flags_defined", modelFlag );
	ppVars[FLAGS2] = IMaterialVar::Create( this, "$flags2", 0 );
	ppVars[FLAGS_DEFINED2] = IMaterialVar::Create( this, "$flags_defined2", 0 );

	int numParams = pShader ? pShader->GetParamCount() : 0;
	int varCount = numParams;

	bool parsingOverrides = (pOverrideKeyValues != 0);
	KeyValues* pVar = pOverrideKeyValues ? pOverrideKeyValues->GetFirstSubKey() : keyValues.GetFirstSubKey();
	while( pVar )
	{
		bool bProcessThisOne = true;

		bool bIsConditionalVar;
		
		// See if the var is a flag...
		if ( 
			ShouldSkipVar( pVar, &bIsConditionalVar ) ||	// should skip?
			( ( pVar->GetName()[0] == '%' ) && ( g_pShaderDevice->IsUsingGraphics() ) && ( !MaterialSystem()->CanUseEditorMaterials() ) && !mat_processtoolvars.GetInt() ) || // is an editor var?
			ParseMaterialFlag( pVar, ppVars[FLAGS], ppVars[FLAGS_DEFINED], parsingOverrides, flagMask, overrideMask ) || // is a flag?
			ParseMaterialFlag( pVar, ppVars[FLAGS2], ppVars[FLAGS_DEFINED2], parsingOverrides, flagMask, overrideMask )
			)
			bProcessThisOne = false;

		if ( bProcessThisOne )
		{
			// See if the var is one of the shader params
			int varIdx = FindMaterialVar( pShader, GetVarName( pVar ) );
			
			// Check for multiply defined or overridden
			if (varIdx >= 0)
			{
				if (ppVars[varIdx] && (! bIsConditionalVar ) )
				{
					if ( !pOverride[varIdx] || parsingOverrides )
					{
						Warning("Error! Variable \"%s\" is multiply defined in material \"%s\"!\n", pVar->GetName(), GetName() );
					}
					goto nextVar;
				}
			}
			else
			{
				int i;
				for ( i = numParams; i < varCount; ++i)
				{
					Assert( ppVars[i] );
					if (!stricmp( ppVars[i]->GetName(), pVar->GetName() ))
						break;
				}
				if (i != varCount)
				{
					if ( !pOverride[i] || parsingOverrides )
					{
						Warning("Error! Variable \"%s\" is multiply defined in material \"%s\"!\n", pVar->GetName(), GetName() );
					}
					goto nextVar;
				}
			}

			// Create a material var for this dudely dude; could be zero...
			pNewVar = CreateMaterialVarFromKeyValue( this, pVar );
			if (!pNewVar) 
				goto nextVar;

			if (varIdx < 0)
			{
				varIdx = varCount++;
			}
			if ( ppVars[varIdx] )
			{
				IMaterialVar::Destroy( ppVars[varIdx] );
			}
			ppVars[varIdx] = pNewVar;
			if (parsingOverrides)
				pOverride[varIdx] = true;
			bWasConditional[varIdx] = bIsConditionalVar;

		}

nextVar:
		pVar = pVar->GetNextKey();
		if (!pVar && parsingOverrides)
		{
			pVar = keyValues.GetFirstSubKey();
			parsingOverrides = false;
		}
	}

	// Create undefined vars for all the actual material vars
	for (int i = 0; i < numParams; ++i)
	{
		if (!ppVars[i])
			ppVars[i] = IMaterialVar::Create( this, pShader->GetParamInfo(i).m_pName );
	}

	return varCount;
}


static KeyValues *CheckConditionalFakeShaderName( char const *pShaderName, char const *pSuffixName,
												  KeyValues *pKeyValues )
{
	KeyValues *pFallbackSection = pKeyValues->FindKey( pSuffixName );
	if (pFallbackSection)
		return pFallbackSection;

	char nameBuf[256];
	V_snprintf( nameBuf, sizeof(nameBuf), "%s_%s", pShaderName, pSuffixName );
	pFallbackSection = pKeyValues->FindKey( nameBuf );

	if (pFallbackSection)
		return pFallbackSection;

	return NULL;
}


static KeyValues *FindBuiltinFallbackBlock( char const *pShaderName, KeyValues *pKeyValues )
{
	// handle "fake" shader fallbacks which are conditional upon mode. like _hdr_dx9, etc
	int nGPULevel = gpu_level.GetInt();
	if ( nGPULevel < 1 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"GPU<1", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( nGPULevel < 2 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"GPU<2", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( nGPULevel >= 1 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"GPU>=1", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( nGPULevel >= 2 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"GPU>=2", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->GetDXSupportLevel() < 90 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"<DX90", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->GetDXSupportLevel() < 95 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"<DX95", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->GetDXSupportLevel() < 92 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"<DX90_20b", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->GetDXSupportLevel() >= 92 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,">=DX90_20b", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->GetDXSupportLevel() <= 90 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"<=DX90", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->GetDXSupportLevel() >= 90 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,">=DX90", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->GetDXSupportLevel() > 90 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,">DX90", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->GetHDRType() != HDR_TYPE_NONE )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"hdr_dx9", pKeyValues );
		if ( pRet )
			return pRet;
		pRet = CheckConditionalFakeShaderName( pShaderName,"hdr", pKeyValues );
		if ( pRet )
			return pRet;
	}
	else
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"ldr", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->UsesSRGBCorrectBlending() )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"srgb", pKeyValues );
		if ( pRet )
			return pRet;
	}
	if ( HardwareConfig()->GetDXSupportLevel() >= 90 )
	{
		KeyValues *pRet = CheckConditionalFakeShaderName( pShaderName,"dx9", pKeyValues );
		if ( pRet )
			return pRet;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Hooks up the shader
//-----------------------------------------------------------------------------
KeyValues* CMaterial::InitializeShader( KeyValues &keyValues, KeyValues &patchKeyValues )
{
	MaterialLock_t hMaterialLock = MaterialSystem()->Lock();

	KeyValues* pCurrentFallback = &keyValues;
	KeyValues* pFallbackSection = 0;

	char szShaderName[MAX_PATH];
	char const* pShaderName = pCurrentFallback->GetName();
	if ( !pShaderName )
	{
		// I'm not quite sure how this can happen, but we'll see... 
		Warning( "Shader not specified in material %s\nUsing wireframe instead...\n", GetName() );
		Assert( 0 );
		pShaderName = "Wireframe_DX9";
	}
	else
	{
		// can't pass a stable reference to the key values name around
		// naive leaf functions can cause KV system to re-alloc
		V_strncpy( szShaderName, pShaderName, sizeof( szShaderName ) );
		pShaderName = szShaderName;
	}

	IShader* pShader;
	IMaterialVar* ppVars[256];
	char pFallbackShaderNameBuf[256];
	char pFallbackMaterialNameBuf[256];
	int varCount = 0;
	bool modelDefault = false;

	// Keep going until there's no more fallbacks...
	while( true )
	{
		// Find the shader for this material. Note that this may not be
		// the actual shader we use due to fallbacks...
		pShader = ShaderSystem()->FindShader( pShaderName );
		if ( !pShader )
		{
			if ( g_pShaderDevice->IsUsingGraphics() )
			{
				Warning( "Error: Material \"%s\" uses unknown shader \"%s\"\n", GetName(), pShaderName );
				//hushed Assert( 0 );
			}

			pShaderName = "Wireframe_DX9";
			pShader = ShaderSystem()->FindShader( pShaderName );

			AssertOnce( pShader );
		}

#ifndef DEDICATED
		if ( !pShader ) 
		{
			MaterialSystem()->Unlock( hMaterialLock );
			return NULL;
		}
#endif

		bool bHasBuiltinFallbackBlock = false;
		if ( !pFallbackSection )
		{
			pFallbackSection = FindBuiltinFallbackBlock( pShaderName, &keyValues );
			if( pFallbackSection )
			{
				bHasBuiltinFallbackBlock = true;
				pFallbackSection->ChainKeyValue( &keyValues );
				pCurrentFallback = pFallbackSection;
			}
		}

		// Here we must set up all flags + material vars that the shader needs
		// because it may look at them when choosing shader fallback.
		varCount = ParseMaterialVars( pShader, keyValues, pFallbackSection, modelDefault, ppVars );

		if ( !pShader )
			break;

		// Make sure we set default values before the fallback is looked for
		ShaderSystem()->InitShaderParameters( pShader, ppVars, GetName() );

		// Now that the material vars are parsed, see if there's a fallback
		// But only if we're not in the tools
/*
		if (!g_pShaderAPI->IsUsingGraphics())
			break;
*/

		// Check for a fallback; if not, we're done
		pShaderName = pShader->GetFallbackShader( ppVars );
		if (!pShaderName)
		{
			break;
		}
		// Copy off the shader name, as it may be in a materialvar in the shader
		// because we're about to delete all materialvars
		Q_strncpy( pFallbackShaderNameBuf, pShaderName, 256 );
		pShaderName = pFallbackShaderNameBuf;

		// Remember the model flag if we're on dx7 or higher...
		modelDefault = ( ppVars[FLAGS]->GetIntValue() & MATERIAL_VAR_MODEL ) != 0;

		// Try to get the section associated with the fallback shader
		// Then chain it to the base data so it can override the 
		// values if it wants to
		if( !bHasBuiltinFallbackBlock )
		{
			pFallbackSection = keyValues.FindKey( pShaderName );
			if (pFallbackSection)
			{
				pFallbackSection->ChainKeyValue( &keyValues );
				pCurrentFallback = pFallbackSection;
			}
		}

		// Now, blow away all of the material vars + try again...
		for (int i = 0; i < varCount; ++i)
		{
			Assert( ppVars[i] );
			IMaterialVar::Destroy( ppVars[i] );
		}

		// Check the KeyValues for '$fallbackmaterial'
		// Note we have to do this *after* we chain the keyvalues 
		// based on the fallback shader	since the names of the fallback material
		// must lie within the shader-specific block usually.
		const char *pFallbackMaterial = pCurrentFallback->GetString( "$fallbackmaterial" );
		if ( pFallbackMaterial[0] )
		{
			// Don't fallback to ourselves
			if ( Q_stricmp( GetName(), pFallbackMaterial ) )
			{
				// Gotta copy it off; clearing the keyvalues will blow the string away
				Q_strncpy( pFallbackMaterialNameBuf, pFallbackMaterial, 256 );
				keyValues.Clear();
				if( !LoadVMTFile( keyValues, patchKeyValues, pFallbackMaterialNameBuf, UsesUNCFileName(), NULL ) )
				{
					Warning( "CMaterial::PrecacheVars: error loading vmt file %s for %s\n", pFallbackMaterialNameBuf, GetName() );
					keyValues = *(((CMaterial *)g_pErrorMaterial)->m_pVMTKeyValues);
				}
			}
			else
			{
				Warning( "CMaterial::PrecacheVars: fallback material for vmt file %s is itself!\n", GetName() );
				keyValues = *(((CMaterial *)g_pErrorMaterial)->m_pVMTKeyValues);
			}

			pCurrentFallback = &keyValues;
			pFallbackSection = NULL;

			// I'm not quite sure how this can happen, but we'll see... 
			pShaderName = pCurrentFallback->GetName();
			if (!pShaderName)
			{
				Warning("Shader not specified in material %s (fallback %s)\nUsing wireframe instead...\n", GetName(), pFallbackMaterialNameBuf );
				pShaderName = "Wireframe_DX9";
			}
		}
	}

	// Store off the shader
	m_pShader = pShader;

	// Store off the material vars + flags
	m_VarCount = varCount;
	m_pShaderParams = (IMaterialVar**)malloc( varCount * sizeof(IMaterialVar*) );
	memcpy( m_pShaderParams, ppVars, varCount * sizeof(IMaterialVar*) );

#ifdef _DEBUG
	for (int i = 0; i < varCount; ++i)
	{
		Assert( ppVars[i] );
	}
#endif

	MaterialSystem()->Unlock( hMaterialLock );
	return pCurrentFallback;
}

//-----------------------------------------------------------------------------
// Gets the texturemap size
//-----------------------------------------------------------------------------

void CMaterial::PrecacheMappingDimensions( )
{
	// Adding support for mapping width override.  To be used sparingly.
	bool bFound;
	IMaterialVar *pMappingWidthVar = FindVar( "$mappingwidth", &bFound, false );
	if ( bFound && pMappingWidthVar->GetIntValue() > 0 )
	{
		IMaterialVar *pMappingHeightVar = FindVar( "$mappingheight", &bFound, false );
		if ( bFound && pMappingHeightVar->GetIntValue() > 0 )
		{
			m_MappingWidth = pMappingWidthVar->GetIntValue();
			m_MappingHeight = pMappingHeightVar->GetIntValue();
			return;
		}
	}

	// Cache mapping width and mapping height
	if (!m_representativeTexture)
	{
#ifdef PARANOID
		Warning( "No representative texture on material: \"%s\"\n", GetName() );
#endif
		m_MappingWidth = 64;
		m_MappingHeight = 64;
	}
	else
	{
		m_MappingWidth = m_representativeTexture->GetMappingWidth();
		m_MappingHeight = m_representativeTexture->GetMappingHeight();
	}
}


//-----------------------------------------------------------------------------
// Initialize the state snapshot
//-----------------------------------------------------------------------------
bool CMaterial::InitializeStateSnapshots()
{
	if (IsPrecached())
	{
		// Default state
		CleanUpStateSnapshots();

		if ( m_pShader && !ShaderSystem()->InitRenderState( m_pShader, m_VarCount, m_pShaderParams, &m_ShaderRenderState, GetName() ))
		{
			m_Flags &= ~MATERIAL_VALID_RENDERSTATE;
			return false;
		}

		m_Flags |= MATERIAL_VALID_RENDERSTATE; 
	}

	return true;
}

void CMaterial::CleanUpStateSnapshots()
{
	if (IsValidRenderState())
	{
		ShaderSystem()->CleanupRenderState(&m_ShaderRenderState);
		// -- THIS CANNOT BE HERE: m_Flags &= ~MATERIAL_VALID_RENDERSTATE;
		// -- because it will cause a crash when main thread asks for material
		// -- sort group it can temporarily see material in invalid render state
		// -- and crash in DecalSurfaceAdd(msurface2_t*, int)
	}
}


//-----------------------------------------------------------------------------
// This sets up a debugging/error shader...
//-----------------------------------------------------------------------------
void CMaterial::SetupErrorShader()
{
	// Preserve the model flags
	int flags = 0;
	if ( m_pShaderParams && m_pShaderParams[FLAGS] )
	{
		flags = (m_pShaderParams[FLAGS]->GetIntValue() & MATERIAL_VAR_MODEL);
	}

	CleanUpShaderParams();
	CleanUpMaterialProxy();

	// We had a failure; replace it with a valid shader...

	m_pShader = ShaderSystem()->FindShader( "Wireframe_DX9" );
	Assert( m_pShader );

	// Create undefined vars for all the actual material vars
	m_VarCount = m_pShader->GetParamCount();
	m_pShaderParams = (IMaterialVar**)malloc( m_VarCount * sizeof(IMaterialVar*) );

	for (int i = 0; i < m_VarCount; ++i)
	{
		m_pShaderParams[i] = IMaterialVar::Create( this, m_pShader->GetParamInfo(i).m_pName );
	}

	// Store the model flags
	SetMaterialVarFlags( flags, true );

	// Set the default values
	ShaderSystem()->InitShaderParameters( m_pShader, m_pShaderParams, "Error" );

	// Invokes the SHADER_INIT block in the various shaders,
	ShaderSystem()->InitShaderInstance( m_pShader, m_pShaderParams, "Error", GetTextureGroupName() );

#ifdef DBGFLAG_ASSERT
	bool ok = 
#endif
		InitializeStateSnapshots();

	m_QueueFriendlyVersion.UpdateToRealTime();

	Assert(ok);
}


//-----------------------------------------------------------------------------
// This computes the state snapshots for this material
//-----------------------------------------------------------------------------
void CMaterial::RecomputeStateSnapshots()
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CMaterial::RecomputeStateSnapshots );
		return;
	}

	bool ok = InitializeStateSnapshots();

	// compute the state snapshots
	if (!ok)
	{
		SetupErrorShader();
	}
}


//-----------------------------------------------------------------------------
// Are we valid
//-----------------------------------------------------------------------------
inline bool CMaterial::IsValidRenderState() const
{
	return (m_Flags & MATERIAL_VALID_RENDERSTATE) != 0;
}


//-----------------------------------------------------------------------------
// Gets/sets material var flags
//-----------------------------------------------------------------------------
inline int CMaterial::GetMaterialVarFlags() const
{
	if ( m_pShaderParams && m_pShaderParams[FLAGS] )
	{
		return m_pShaderParams[FLAGS]->GetIntValueFast();
	}
	else
	{
		return 0;
	}
}

inline void CMaterial::SetMaterialVarFlags( int flags, bool on )
{
	if (on)
		m_pShaderParams[FLAGS]->SetIntValue( GetMaterialVarFlags() | flags );
	else
		m_pShaderParams[FLAGS]->SetIntValue( GetMaterialVarFlags() & (~flags) );

	// Mark it as being defined...
	m_pShaderParams[FLAGS_DEFINED]->SetIntValue( 
		m_pShaderParams[FLAGS_DEFINED]->GetIntValueFast() | flags );
}

inline int CMaterial::GetMaterialVarFlags2() const
{
	if ( m_pShaderParams && m_VarCount > FLAGS2 && m_pShaderParams[FLAGS2] )
	{
		return m_pShaderParams[FLAGS2]->GetIntValueFast();
	}
	else
	{
		return 0;
	}
}

inline void CMaterial::SetMaterialVarFlags2( int flags, bool on )
{
	if ( m_pShaderParams && m_VarCount > FLAGS2 && m_pShaderParams[FLAGS2] )
	{
		if (on)
			m_pShaderParams[FLAGS2]->SetIntValue( GetMaterialVarFlags2() | flags );
		else
			m_pShaderParams[FLAGS2]->SetIntValue( GetMaterialVarFlags2() & (~flags) );
	}

	if ( m_pShaderParams && m_VarCount > FLAGS_DEFINED2 && m_pShaderParams[FLAGS_DEFINED2] )
	{
		// Mark it as being defined...
		m_pShaderParams[FLAGS_DEFINED2]->SetIntValue( 
			m_pShaderParams[FLAGS_DEFINED2]->GetIntValueFast() | flags );
	}
}


//-----------------------------------------------------------------------------
// Gets the vertex format
//-----------------------------------------------------------------------------
VertexFormat_t CMaterial::GetVertexFormat() const
{
	Assert( IsValidRenderState() );
	return m_ShaderRenderState.m_VertexFormat;
}

VertexFormat_t CMaterial::GetVertexUsage() const
{
	Assert( IsValidRenderState() );
	return m_ShaderRenderState.m_VertexUsage;
}

bool CMaterial::PerformDebugTrace() const
{
	return IsValidRenderState() && ((GetMaterialVarFlags() & MATERIAL_VAR_DEBUG ) != 0);
}


//-----------------------------------------------------------------------------
// Are we suppressed?
//-----------------------------------------------------------------------------
bool  CMaterial::IsSuppressed() const
{
	if ( !IsValidRenderState() )
		return true;

	return ((GetMaterialVarFlags() & MATERIAL_VAR_NO_DRAW) != 0);
}

void CMaterial::ToggleSuppression()
{
	if (IsValidRenderState())
	{
		if ((GetMaterialVarFlags() & MATERIAL_VAR_NO_DEBUG_OVERRIDE) != 0)
			return;

		SetMaterialVarFlags( MATERIAL_VAR_NO_DRAW, 
			(GetMaterialVarFlags() & MATERIAL_VAR_NO_DRAW) == 0 );
	}
}

void CMaterial::ToggleDebugTrace()
{
	if (IsValidRenderState())
	{
		SetMaterialVarFlags( MATERIAL_VAR_DEBUG, 
			(GetMaterialVarFlags() & MATERIAL_VAR_DEBUG) == 0 );
	}
}

//-----------------------------------------------------------------------------
// Can we override this material in debug?
//-----------------------------------------------------------------------------

bool CMaterial::NoDebugOverride() const
{
	return IsValidRenderState() && (GetMaterialVarFlags() & MATERIAL_VAR_NO_DEBUG_OVERRIDE) != 0;
}


//-----------------------------------------------------------------------------
// Material Var flags
//-----------------------------------------------------------------------------
void CMaterial::SetMaterialVarFlag( MaterialVarFlags_t flag, bool on )
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue )
	{
		pCallQueue->QueueCall( this, &CMaterial::SetMaterialVarFlag, flag, on );
		return;
	}

	bool oldOn = (GetMaterialVarFlags( ) & flag) != 0;
	if (oldOn != on)
	{
		SetMaterialVarFlags( flag, on );

		// This is going to be called from client code; recompute snapshots!
		RecomputeStateSnapshots();
	}
}

bool CMaterial::GetMaterialVarFlag( MaterialVarFlags_t flag ) const
{
	return (GetMaterialVarFlags() & flag) != 0;
}


//-----------------------------------------------------------------------------
// Do we use the env_cubemap entity to get cubemaps from the level?
//-----------------------------------------------------------------------------
bool CMaterial::UsesEnvCubemap( void )
{
	Precache_Inline();

	AssertOnce( m_pShader );	
	if( !m_pShader )
	{
		return false;
	}

	Assert( m_pShaderParams );
	return IsFlag2Set( m_pShaderParams, MATERIAL_VAR2_USES_ENV_CUBEMAP );
}


//-----------------------------------------------------------------------------
// Do we need a tangent space at the vertex level?
//-----------------------------------------------------------------------------
bool CMaterial::NeedsTangentSpace( void )
{
	Precache_Inline();
	AssertOnce( m_pShader );
	if( !m_pShader )
	{
		return false;
	}

	Assert( m_pShaderParams );
	return IsFlag2Set( m_pShaderParams, MATERIAL_VAR2_NEEDS_TANGENT_SPACES );
}

bool CMaterial::NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame )
{
	Precache_Inline();
	AssertOnce( m_pShader );
	if( !m_pShader )
	{
		return false;
	}

	Assert( m_pShaderParams );
	return m_pShader->NeedsPowerOfTwoFrameBufferTexture( m_pShaderParams, bCheckSpecificToThisFrame );
}

bool CMaterial::NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame )
{
	Precache_Inline();
	Assert( m_pShader );
	if( !m_pShader )
	{
		return false;
	}

	Assert( m_pShaderParams );
	return m_pShader->NeedsFullFrameBufferTexture( m_pShaderParams, bCheckSpecificToThisFrame );
}

// GR - Is lightmap alpha needed?
bool CMaterial::NeedsLightmapBlendAlpha( void )
{
	Precache_Inline();
	return (GetMaterialVarFlags2() & MATERIAL_VAR2_BLEND_WITH_LIGHTMAP_ALPHA ) != 0;
}

//-----------------------------------------------------------------------------
// Do we need software skinning?
//-----------------------------------------------------------------------------
bool CMaterial::NeedsSoftwareSkinning( void )
{
	Precache_Inline();
	Assert( m_pShader );
	if( !m_pShader )
	{
		return false;
	}

	Assert( m_pShaderParams );
	return IsFlagSet( m_pShaderParams, MATERIAL_VAR_NEEDS_SOFTWARE_SKINNING );
}


//-----------------------------------------------------------------------------
// Do we need software lighting?
//-----------------------------------------------------------------------------
bool CMaterial::NeedsSoftwareLighting( void )
{
	Precache_Inline();
	Assert( m_pShader );
	if( !m_pShader )
	{
		return false;
	}
	Assert( m_pShaderParams );
	return IsFlag2Set( m_pShaderParams, MATERIAL_VAR2_NEEDS_SOFTWARE_LIGHTING );
}

//-----------------------------------------------------------------------------
// Alpha/color modulation
//-----------------------------------------------------------------------------
void CMaterial::AlphaModulate( float alpha )
{
	PrecacheVars_Inline();
	if ( m_VarCount > ALPHA )
		m_pShaderParams[ALPHA]->SetFloatValue(alpha);
}

void CMaterial::ColorModulate( float r, float g, float b )
{
	PrecacheVars_Inline();
	if ( m_VarCount > COLOR )
		m_pShaderParams[COLOR]->SetVecValue( r, g, b );
}

float CMaterial::GetAlphaModulation()
{
	PrecacheVars_Inline();
	if ( m_VarCount > ALPHA )
		return m_pShaderParams[ALPHA]->GetFloatValue();
	return 0.0f;
}

void CMaterial::GetColorModulation( float *r, float *g, float *b )
{
	PrecacheVars_Inline();

	float pColor[3] = { 0.0f, 0.0f, 0.0f };
	if ( m_VarCount > COLOR )
		m_pShaderParams[COLOR]->GetVecValue( pColor, 3 );
	*r = pColor[0];
	*g = pColor[1];
	*b = pColor[2];
}


//-----------------------------------------------------------------------------
// Do we use fog?
//-----------------------------------------------------------------------------
bool CMaterial::UseFog() const
{
	Assert( m_VarCount > 0 );
	return IsValidRenderState() && ((GetMaterialVarFlags() & MATERIAL_VAR_NOFOG) == 0);
}


//-----------------------------------------------------------------------------
// diffuse bump?
//-----------------------------------------------------------------------------
bool CMaterial::IsUsingDiffuseBumpedLighting() const
{
	return (GetMaterialVarFlags2() & MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP ) != 0;
}


//-----------------------------------------------------------------------------
// lightmap?
//-----------------------------------------------------------------------------
bool CMaterial::IsUsingLightmap() const
{
	return (GetMaterialVarFlags2() & MATERIAL_VAR2_LIGHTING_LIGHTMAP ) != 0;
}

bool CMaterial::IsManuallyCreated() const
{
	return (m_Flags & MATERIAL_IS_MANUALLY_CREATED) != 0;
}

bool CMaterial::UsesUNCFileName() const
{
	return (m_Flags & MATERIAL_USES_UNC_FILENAME) != 0;
}


void CMaterial::DecideShouldReloadFromWhitelist( IFileList *pFilesToReload )
{
	m_bShouldReloadFromWhitelist = false;
	if ( IsManuallyCreated() || !IsPrecached() )
		return;

	// Materials loaded with an absolute pathname are usually debug materials.
	if ( V_IsAbsolutePath( GetName() ) )
		return;

	char vmtFilename[MAX_PATH];
	V_ComposeFileName( "materials", GetName(), vmtFilename, sizeof( vmtFilename ) );
	V_strncat( vmtFilename, ".vmt", sizeof( vmtFilename ) );

	// Check if either this file or any of the files it included need to be reloaded.
	bool bShouldReload = pFilesToReload->IsFileInList( vmtFilename );
	if ( !bShouldReload )
	{
		for ( int i=0; i < m_VMTIncludes.Count(); i++ )
		{
			g_pFullFileSystem->String( m_VMTIncludes[i], vmtFilename, sizeof( vmtFilename ) );
			bShouldReload = pFilesToReload->IsFileInList( vmtFilename );
			if ( bShouldReload )
				break;
		}
	}

	m_bShouldReloadFromWhitelist = bShouldReload;
}

void CMaterial::ReloadFromWhitelistIfMarked()
{
	if ( !m_bShouldReloadFromWhitelist )
		return;

	Uncache();
	Precache_Inline();
	if ( !GetShader() )
	{
		// We can get in here if we previously loaded this material off disk and now the whitelist 
		// says to get it out of Steam but it's not in Steam. So just setup a wireframe thingy
		// to draw the material with.
		m_Flags |= MATERIAL_IS_PRECACHED | MATERIAL_VARS_IS_PRECACHED;
		#if DEBUG
		if (IsOSX())
		{
			printf("\n ##### CMaterial::ReloadFromWhitelistIfMarked: GetShader failed on %s, calling SetupErrorShader", m_pDebugName );
		}
		#endif
		
		SetupErrorShader();
	}
}

bool CMaterial::WasReloadedFromWhitelist()
{
	return m_bShouldReloadFromWhitelist;
}

//-----------------------------------------------------------------------------
// Loads the material vars
//-----------------------------------------------------------------------------
bool CMaterial::PrecacheVars_Internal( KeyValues *pVMTKeyValues, KeyValues *pPatchKeyValues, CUtlVector<FileNameHandle_t> *pIncludes )
{
	if ( pIncludes )
		m_VMTIncludes = *pIncludes;
	else
		m_VMTIncludes.Purge();

	MaterialLock_t hMaterialLock = MaterialSystem()->Lock();

	bool bOk = false;
	bool bError = false;
	KeyValues *vmtKeyValues = NULL;
	KeyValues *patchKeyValues = NULL;
	if ( m_pVMTKeyValues )
	{
		// Use the procedural KeyValues
		vmtKeyValues = m_pVMTKeyValues;
		patchKeyValues = new KeyValues( "vmt_patches" );

		// The caller should not be passing in KeyValues if we have procedural ones
		Assert( ( pVMTKeyValues == NULL ) && ( pPatchKeyValues == NULL ) );
	}
	else if ( pVMTKeyValues )
	{
		// Use the passed-in (already-loaded) KeyValues
		vmtKeyValues = pVMTKeyValues;
		patchKeyValues = pPatchKeyValues;
	}
	else
	{
		m_VMTIncludes.Purge();

		// load data from the vmt file
		vmtKeyValues = new KeyValues( "vmt" );
		patchKeyValues = new KeyValues( "vmt_patches" );
		if( !LoadVMTFile( *vmtKeyValues, *patchKeyValues, GetName(), UsesUNCFileName(), &m_VMTIncludes ) )
		{
			Warning( "CMaterial::PrecacheVars: error loading vmt file for %s\n", GetName() );
			bError = true;
		}
	}

	if ( ! bError )
	{
		// Needed to prevent re-entrancy
		m_Flags |= MATERIAL_VARS_IS_PRECACHED;

		// Create shader and the material vars...
		KeyValues *pFallbackKeyValues = InitializeShader( *vmtKeyValues, *patchKeyValues );
		if ( pFallbackKeyValues )
		{
			// Gotta initialize the proxies too, using the fallback proxies
			InitializeMaterialProxy(pFallbackKeyValues);
			bOk = true;
		}
	}

	// Clean up
	if ( ( vmtKeyValues != m_pVMTKeyValues ) && ( vmtKeyValues != pVMTKeyValues ) )
	{
		vmtKeyValues->deleteThis();
	}
	if ( patchKeyValues != pPatchKeyValues )
	{
		patchKeyValues->deleteThis();
	}

	if ( m_pShaderParams )
	{
		m_QueueFriendlyVersion.UpdateToRealTime();
	}

	MaterialSystem()->Unlock( hMaterialLock );

	return bOk;
}


//-----------------------------------------------------------------------------
// Loads the material info from the VMT file
//-----------------------------------------------------------------------------
void CMaterial::Precache_Internal()
{
	MaterialLock_t hMaterialLock = MaterialSystem()->Lock();

	m_Flags |= MATERIAL_IS_PRECACHED;

	// Invokes the SHADER_INIT block in the various shaders,
	if ( m_pShader ) 
	{
#ifdef _DEBUG
		if ( GetMaterialVarFlags() & MATERIAL_VAR_DEBUG )
		{
			// Putcher breakpoint here to catch the rendering of a material
			// marked for debugging ($debug = 1 in a .vmt file) SHADER_INIT version
			int x = 0;
			NOTE_UNUSED( x );
		}
#endif
		ShaderSystem()->InitShaderInstance( m_pShader, m_pShaderParams, GetName(), GetTextureGroupName() );
	}

	// compute the state snapshots
	RecomputeStateSnapshots();

	FindRepresentativeTexture();

	// Reads in the texture width and height from the material var
	PrecacheMappingDimensions();

	Assert( IsValidRenderState() );

	MaterialSystem()->Unlock( hMaterialLock );
}


//-----------------------------------------------------------------------------
// Unloads the material data from memory
//-----------------------------------------------------------------------------
void CMaterial::Uncache( bool bPreserveVars )
{
	MaterialLock_t hMaterialLock = MaterialSystem()->Lock();

	// Don't bother if we're not cached
	if ( IsPrecached() )
	{		
		// Clean up the state snapshots
		CleanUpStateSnapshots();
		m_Flags &= ~MATERIAL_VALID_RENDERSTATE;
		m_Flags &= ~MATERIAL_IS_PRECACHED;
	}

	if ( !bPreserveVars )
	{
		if ( IsPrecachedVars() )
		{
			// Clean up the shader + params
			CleanUpShaderParams();
			m_pShader = 0;

			// Clean up the material proxy
			CleanUpMaterialProxy();

			m_Flags &= ~MATERIAL_VARS_IS_PRECACHED;
		}
	}

	MaterialSystem()->Unlock( hMaterialLock );
}

//-----------------------------------------------------------------------------
// reload all textures used by this materals
//-----------------------------------------------------------------------------
void CMaterial::ReloadTextures( void )
{
	Precache_Inline();
	int i;
	int nParams = ShaderParamCount();
	IMaterialVar **ppVars = GetShaderParams();
	for( i = 0; i < nParams; i++ )
	{
		if( ppVars[i] )		
		{
			if( ppVars[i]->IsTexture() )
			{
				ITextureInternal *pTexture = ( ITextureInternal * )ppVars[i]->GetTextureValue();
				if( !IsTextureInternalEnvCubemap( pTexture ) )
				{
					pTexture->Download();
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Meant to be used with materials created using CreateMaterial
// It updates the materials to reflect the current values stored in the material vars
//-----------------------------------------------------------------------------
void CMaterial::Refresh()
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		Uncache();
		Precache();
	}
}

void CMaterial::RefreshPreservingMaterialVars()
{
	if ( g_pShaderDevice->IsUsingGraphics() )
	{
		Uncache( true );
		Precache();
	}
}

//-----------------------------------------------------------------------------
// Gets the material name
//-----------------------------------------------------------------------------
char const* CMaterial::GetName() const
{
	return m_Name.String();
}


char const* CMaterial::GetTextureGroupName() const
{
	return m_TextureGroupName.String();
}


//-----------------------------------------------------------------------------
// Material dimensions
//-----------------------------------------------------------------------------
int	CMaterial::GetMappingWidth( )
{
	Precache_Inline();
	return m_MappingWidth;
}

int	CMaterial::GetMappingHeight( )
{
	Precache_Inline();
	return m_MappingHeight;
}


//-----------------------------------------------------------------------------
// Animated material info
//-----------------------------------------------------------------------------

int CMaterial::GetNumAnimationFrames( )
{
	Precache_Inline();
	if( m_representativeTexture )
	{
		return m_representativeTexture->GetNumAnimationFrames();
	}
	else
	{
#ifndef POSIX
		Warning( "CMaterial::GetNumAnimationFrames:\nno representative texture for material %s\n", GetName() );
#endif
		return 1;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterial::GetMaterialOffset( float *pOffset )
{
	// Identity.
	pOffset[0] = 0.0f;
	pOffset[1] = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMaterial::GetMaterialScale( float *pScale )
{
	// Identity.
	pScale[0] = 1.0f;
	pScale[1] = 1.0f;
}

//-----------------------------------------------------------------------------
// Reference count
//-----------------------------------------------------------------------------
void CMaterial::IncrementReferenceCount()
{
//	if ( *mat_debug_material_reference_name.GetString() && Q_stristr( m_Name.String(), mat_debug_material_reference_name.GetString() ) )
//	{
//		Msg("%s ++REF(%d)\n", m_Name.String(), m_RefCount );
//	}
	
	++m_RefCount;
}

void CMaterial::DecrementReferenceCount()
{
//	if ( *mat_debug_material_reference_name.GetString() && Q_stristr( m_Name.String(), mat_debug_material_reference_name.GetString() ) )
//	{
//		Msg("%s --REF(%d)\n", m_Name.String(), m_RefCount );
//	}

	--m_RefCount;
}

int CMaterial::GetReferenceCount( )	const
{
	return m_RefCount;
}

//-----------------------------------------------------------------------------
// Sets the shader associated with the material
//-----------------------------------------------------------------------------
void CMaterial::SetShader( const char *pShaderName )
{
	Assert( pShaderName );

	int i;
	IShader* pShader;
	IMaterialVar* ppVars[256];
	int iVarCount = 0;

	// Clean up existing state
	Uncache();

	// Keep going until there's no more fallbacks...
	while( true )
	{
		// Find the shader for this material. Note that this may not be
		// the actual shader we use due to fallbacks...
		pShader = ShaderSystem()->FindShader( pShaderName );
		if (!pShader)
		{
			// Couldn't find the shader we wanted to use; it's not defined...
			Warning( "SetShader: Couldn't find shader %s for material %s!\n", pShaderName, GetName() );
			pShaderName = "Wireframe_DX9";
			pShader = ShaderSystem()->FindShader( pShaderName );
			Assert( pShader );
		}

		// Create undefined vars for all the actual material vars
		iVarCount = pShader->GetParamCount();
		for (i = 0; i < iVarCount; ++i)
		{
			ppVars[i] = IMaterialVar::Create( this, pShader->GetParamInfo(i).m_pName );
		}

		// Make sure we set default values before the fallback is looked for
		ShaderSystem()->InitShaderParameters( pShader, ppVars, pShaderName );

		// Now that the material vars are parsed, see if there's a fallback
		// But only if we're not in the tools
		if (!g_pShaderDevice->IsUsingGraphics())
			break;

		// Check for a fallback; if not, we're done
		pShaderName = pShader->GetFallbackShader( ppVars );
		if (!pShaderName)
			break;

		// Now, blow away all of the material vars + try again...
		for (i = 0; i < iVarCount; ++i)
		{
			Assert( ppVars[i] );
			IMaterialVar::Destroy( ppVars[i] );
		}
	}

	// Store off the shader
	m_pShader = pShader;

	// Store off the material vars + flags
	m_VarCount = iVarCount;
	m_pShaderParams = (IMaterialVar**)malloc( iVarCount * sizeof(IMaterialVar*) );
	memcpy( m_pShaderParams, ppVars, iVarCount * sizeof(IMaterialVar*) );

	// Invokes the SHADER_INIT block in the various shaders,
	ShaderSystem()->InitShaderInstance( m_pShader, m_pShaderParams, GetName(), GetTextureGroupName() );

	// Precache our initial state...
	// NOTE: What happens here for textures???

	// Pretend that we precached our material vars; we certainly don't have any!
	m_Flags |= MATERIAL_VARS_IS_PRECACHED;

	// NOTE: The caller has to call 'Refresh' for the shader to be ready...
}

const char *CMaterial::GetShaderName() const
{
	return (m_pShader) ? m_pShader->GetName() : "";
}


//-----------------------------------------------------------------------------
// Enumeration ID
//-----------------------------------------------------------------------------
int CMaterial::GetEnumerationID( ) const
{
	return m_iEnumerationID;
}

void CMaterial::SetEnumerationID( int id )
{
	m_iEnumerationID = id;
}

//-----------------------------------------------------------------------------
// Preview image
//-----------------------------------------------------------------------------
char const* CMaterial::GetPreviewImageName( void )
{
	if ( IsGameConsole() )
	{
		// not supporting
		return NULL;
	}

	PrecacheVars_Inline();

	bool found;
	IMaterialVar *pRepresentativeTextureVar;
	
	FindVar( "%noToolTexture", &found, false );
	if (found)
		return NULL;

	pRepresentativeTextureVar = FindVar( "%toolTexture", &found, false );
	if( found )
	{
		if (pRepresentativeTextureVar->GetType() == MATERIAL_VAR_TYPE_STRING )
			return pRepresentativeTextureVar->GetStringValue();
		if (pRepresentativeTextureVar->GetType() == MATERIAL_VAR_TYPE_TEXTURE )
			return pRepresentativeTextureVar->GetTextureValue()->GetName();
	}
	pRepresentativeTextureVar = FindVar( "$baseTexture", &found, false );
	if( found )
	{
		if (pRepresentativeTextureVar->GetType() == MATERIAL_VAR_TYPE_STRING )
			return pRepresentativeTextureVar->GetStringValue();
		if (pRepresentativeTextureVar->GetType() == MATERIAL_VAR_TYPE_TEXTURE )
			return pRepresentativeTextureVar->GetTextureValue()->GetName();
	}
	return GetName();
}

char const* CMaterial::GetPreviewImageFileName( void ) const
{
	char const* pName = const_cast<CMaterial*>(this)->GetPreviewImageName();
	if( !pName )
		return NULL;

	static char vtfFilename[MATERIAL_MAX_PATH];
	if( Q_strlen( pName ) >= MATERIAL_MAX_PATH - 5 )
	{
		Warning( "MATERIAL_MAX_PATH to short for %s.vtf\n", pName );
		return NULL;
	}

	if ( !UsesUNCFileName() )
	{
		Q_snprintf( vtfFilename, sizeof( vtfFilename ), "materials/%s.vtf", pName );
	}
	else
	{
		Q_snprintf( vtfFilename, sizeof( vtfFilename ), "%s.vtf", pName );
	}

	return vtfFilename;
}

PreviewImageRetVal_t CMaterial::GetPreviewImageProperties( int *width, int *height, 
				 		ImageFormat *imageFormat, bool* isTranslucent ) const
{	
	char const* pFileName = GetPreviewImageFileName();
	if ( IsGameConsole() || !pFileName )
	{
		*width = *height = 0;
		*imageFormat = IMAGE_FORMAT_RGBA8888;
		*isTranslucent = false;
		return MATERIAL_NO_PREVIEW_IMAGE;
	}

	int nHeaderSize = VTFFileHeaderSize( VTF_MAJOR_VERSION );
	unsigned char *pMem = (unsigned char *)stackalloc( nHeaderSize );
	CUtlBuffer buf( pMem, nHeaderSize );
	if( !g_pFullFileSystem->ReadFile( pFileName, NULL, buf, nHeaderSize ) )
	{
		Warning( "\"%s\" - \"%s\": cached version doesn't exist\n", GetName(), pFileName );
		return MATERIAL_PREVIEW_IMAGE_BAD;
	}
	
	IVTFTexture *pVTFTexture = CreateVTFTexture();
	if (!pVTFTexture->Unserialize( buf, true ))
	{
		Warning( "Error reading material \"%s\"\n", pFileName );
		DestroyVTFTexture( pVTFTexture );
		return MATERIAL_PREVIEW_IMAGE_BAD;
	}

	*width = pVTFTexture->Width();
	*height = pVTFTexture->Height();
	*imageFormat = pVTFTexture->Format();
	*isTranslucent = (pVTFTexture->Flags() & (TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA)) != 0;
	DestroyVTFTexture( pVTFTexture );
	return MATERIAL_PREVIEW_IMAGE_OK;
}

PreviewImageRetVal_t CMaterial::GetPreviewImage( unsigned char *pData, int width, int height,
					             ImageFormat imageFormat ) const
{
	CUtlBuffer buf;
	int nHeaderSize;
	int nImageOffset, nImageSize;

	char const* pFileName = GetPreviewImageFileName();
	if ( IsGameConsole() || !pFileName )
	{
		return MATERIAL_NO_PREVIEW_IMAGE;
	}

	IVTFTexture *pVTFTexture = CreateVTFTexture();
	FileHandle_t fileHandle = g_pFullFileSystem->Open( pFileName, "rb" );
	if( !fileHandle )
	{
		Warning( "\"%s\": cached version doesn't exist\n", pFileName );
		goto fail;
	}

	nHeaderSize = VTFFileHeaderSize( VTF_MAJOR_VERSION );
	buf.EnsureCapacity( nHeaderSize );
	
	// read the header first.. it's faster!!
	int nBytesRead; // GCC won't let this be initialized right away
	nBytesRead = g_pFullFileSystem->Read( buf.Base(), nHeaderSize, fileHandle );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );
		
	// Unserialize the header
	if (!pVTFTexture->Unserialize( buf, true ))
	{
		Warning( "Error reading material \"%s\"\n", pFileName );
		goto fail;
	}
		
	// FIXME: Make sure the preview image size requested is the same
	// size as mip level 0 of the texture
	Assert( (width == pVTFTexture->Width()) && (height == pVTFTexture->Height()) );
		
	// Determine where in the file to start reading (frame 0, face 0, mip 0)
	pVTFTexture->ImageFileInfo( 0, 0, 0, &nImageOffset, &nImageSize );

	if ( nImageSize == 0 )
	{
		Warning( "Couldn't determine offset and size of material \"%s\"\n", pFileName );
		goto fail;
	}
		
	// Prep the utlbuffer for reading
	buf.EnsureCapacity( nImageSize );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
		
	// Read in the bits at the specified location
	g_pFullFileSystem->Seek( fileHandle, nImageOffset, FILESYSTEM_SEEK_HEAD );
	g_pFullFileSystem->Read( buf.Base(), nImageSize, fileHandle );
	g_pFullFileSystem->Close( fileHandle );
		
	// Convert from the format read in to the requested format
	ImageLoader::ConvertImageFormat( (unsigned char*)buf.Base(), pVTFTexture->Format(), 
		pData, imageFormat, width, height );

	DestroyVTFTexture( pVTFTexture );
	return MATERIAL_PREVIEW_IMAGE_OK;

fail:
	if( fileHandle )
	{
		g_pFullFileSystem->Close( fileHandle );
	}
	int nSize = ImageLoader::GetMemRequired( width, height, 1, imageFormat, false );
	memset( pData, 0xff, nSize );
	DestroyVTFTexture( pVTFTexture );
	return MATERIAL_PREVIEW_IMAGE_BAD;
}

//-----------------------------------------------------------------------------
// Material variables
//-----------------------------------------------------------------------------
IMaterialVar *CMaterial::FindVar( char const *pVarName, bool *pFound, bool complain )
{
	PrecacheVars_Inline();

	// FIXME: Could look for flags here too...

	MaterialVarSym_t sym = IMaterialVar::FindSymbol(pVarName);
	if ( sym != UTL_INVAL_SYMBOL )
	{
		for (int i = m_VarCount; --i >= 0; )
		{
			if (m_pShaderParams[i]->GetNameAsSymbol() == sym)
			{
				if( pFound )
					*pFound = true;					  
				return m_pShaderParams[i];
			}
		}
	}

	if( pFound )
		*pFound = false;

	if( complain )
	{
		static int complainCount = 0;
		if( complainCount < 100 )
		{
			Warning( "No such variable \"%s\" for material \"%s\"\n", pVarName, GetName() );
			complainCount++;
		}
	}
	return GetDummyVariable();
}

IMaterialVar *CMaterial::FindVarFast( char const *pVarName, unsigned int *pCacheData )
{
	tokencache_t *pToken = reinterpret_cast<tokencache_t *>(pCacheData);
	PrecacheVars_Inline();

	if ( pToken->cached )
	{
		if ( !pToken->subrect && ( pToken->varIndex < m_VarCount ) && ( m_pShaderParams[pToken->varIndex]->GetNameAsSymbol() == pToken->symbol ) )
			return m_pShaderParams[pToken->varIndex];
		// FIXME: Could look for flags here too...
		if ( !IMaterialVar::SymbolMatches(pVarName, pToken->symbol) )
		{
			pToken->symbol = IMaterialVar::FindSymbol(pVarName);
		}
	}
	else
	{
		pToken->cached = true;
		pToken->symbol = IMaterialVar::FindSymbol(pVarName);
	}

	if ( pToken->symbol != UTL_INVAL_SYMBOL )
	{
		for (int i = m_VarCount; --i >= 0; )
		{
			if (m_pShaderParams[i]->GetNameAsSymbol() == pToken->symbol)
			{
				pToken->varIndex = i;
				pToken->subrect = false;
				return m_pShaderParams[i];
			}
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Lovely material properties
//-----------------------------------------------------------------------------
void CMaterial::GetReflectivity( Vector& reflect )
{
	Precache_Inline();

	reflect = m_Reflectivity;
}


bool CMaterial::GetPropertyFlag( MaterialPropertyTypes_t type )
{
	Precache_Inline();

	if (!IsValidRenderState())
		return false;

	switch( type )
	{
	case MATERIAL_PROPERTY_NEEDS_LIGHTMAP:
		return IsUsingLightmap();

	case MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS:
		return IsUsingDiffuseBumpedLighting();
	}

	return false;
}


//-----------------------------------------------------------------------------
// Is the material visible from both sides?
//-----------------------------------------------------------------------------
bool CMaterial::IsTwoSided()
{
	PrecacheVars_Inline();
	return GetMaterialVarFlag(MATERIAL_VAR_NOCULL);
}


//-----------------------------------------------------------------------------
// Are we translucent?
//-----------------------------------------------------------------------------
bool CMaterial::IsTranslucent()
{
	Precache_Inline();
	if ( m_VarCount > ALPHA )
		return IsTranslucentInternal( m_pShaderParams? m_pShaderParams[ALPHA]->GetFloatValue() : 0.0  );
	return false;
}


bool CMaterial::IsTranslucentInternal( float fAlphaModulation ) const
{
	if (m_pShader && IsValidRenderState())
	{
		// If alpha is modified by the proxy, then we are translucent
		// Dramatically simplifies game code to make this assumption
		int nMyMaterialVarFlags = GetMaterialVarFlags();
		if ( nMyMaterialVarFlags & MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY )
			return true;

		const int nPseudoTranslucentMaterialRenderingInOpaquePass = MATERIAL_VAR_TRANSLUCENT | MATERIAL_VAR_PSEUDO_TRANSLUCENT;
		if ( (nMyMaterialVarFlags & nPseudoTranslucentMaterialRenderingInOpaquePass) == nPseudoTranslucentMaterialRenderingInOpaquePass )
			return false;

		// I have to check for alpha modulation here because it isn't
		// factored into the shader's notion of whether or not it's transparent
		return ::IsTranslucent(&m_ShaderRenderState) || 
			(fAlphaModulation < 1.0f) ||
			m_pShader->IsTranslucent( m_pShaderParams );
	}
	return false;
}

// Is this translucent even without alpha modulation?
bool CMaterial::IsTranslucentUnderModulation( float fAlphaModulation ) const
{
	const_cast< CMaterial* >( this )->Precache_Inline();
	return IsTranslucentInternal( fAlphaModulation );
}


//-----------------------------------------------------------------------------
// Are we alphatested?
//-----------------------------------------------------------------------------
bool CMaterial::IsAlphaTested()
{
	Precache_Inline();
	if (m_pShader && IsValidRenderState())
	{
		return ::IsAlphaTested(&m_ShaderRenderState) || 
				GetMaterialVarFlag( MATERIAL_VAR_ALPHATEST );
	}
	return false;
}


//-----------------------------------------------------------------------------
// Are we vertex lit?
//-----------------------------------------------------------------------------
bool CMaterial::IsVertexLit()
{
	Precache_Inline();
	if (IsValidRenderState())
	{
		return ( GetMaterialVarFlags2() & MATERIAL_VAR2_LIGHTING_VERTEX_LIT ) != 0;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Is the shader a sprite card shader?
//-----------------------------------------------------------------------------
bool CMaterial::IsSpriteCard()
{
	Precache_Inline();
	if (IsValidRenderState())
	{
		return ( GetMaterialVarFlags2() & MATERIAL_VAR2_IS_SPRITECARD ) != 0;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Proxies
//-----------------------------------------------------------------------------
void CMaterial::CallBindProxy( void *proxyData, ICallQueue *pCallQueue )
{
	bool bIsThreaded = ( pCallQueue != NULL );
	switch (g_config.proxiesTestMode)
	{
	case 0:
		{
			// Make sure we call the proxies in the order in which they show up
			// in the .vmt file
			if ( m_ProxyCount )
			{
				if ( bIsThreaded )
				{
					EnableThreadedMaterialVarAccess( true, m_pShaderParams, m_VarCount );
				}

				IClientMaterialSystem *pClientMaterialSystem = MaterialSystem()->GetClientMaterialSystemInterface();
				if( pClientMaterialSystem )
				{
					pClientMaterialSystem->SetMaterialProxyData( proxyData );
				}

				for( int i = 0; i < m_ProxyCount; ++i )
				{
					m_ppProxies[i]->OnBind( proxyData );
				}

				if( pClientMaterialSystem )
				{
					pClientMaterialSystem->SetMaterialProxyData( NULL );
				}

				if ( bIsThreaded )
				{
					EnableThreadedMaterialVarAccess( false, m_pShaderParams, m_VarCount );
				}
			}
		}
		break;

	case 2:
		// alpha mod all....
		{
			float value = ( sin( 2.0f * M_PI * Plat_FloatTime() / 10.0f ) * 0.5f ) + 0.5f;
			m_pShaderParams[ALPHA]->SetFloatValue( value );
		}
		break;

	case 3:
		// color mod all...
		{
			float value = ( sin( 2.0f * M_PI * Plat_FloatTime() / 10.0f ) * 0.5f ) + 0.5f;
			m_pShaderParams[COLOR]->SetVecValue( value, 1.0f, 1.0f );
		}
		break;
	}
}

bool CMaterial::HasProxy( )	const
{
	const_cast< CMaterial* >( this )->PrecacheVars_Inline();
	return (m_ProxyCount > 0);
}


//-----------------------------------------------------------------------------
// Main draw method
//-----------------------------------------------------------------------------

#ifdef _WIN32
#pragma warning (disable: 4189)
#endif

StateSnapshot_t CMaterial::GetSnapshotId( int modulation, int renderPass )
{
	return m_ShaderRenderState.m_pSnapshots[modulation].m_Snapshot[renderPass];
}

unsigned char* CMaterial::GetInstanceCommandBuffer( int modulation )
{
	CBasePerInstanceContextData *pInstanceData = m_ShaderRenderState.m_pSnapshots[modulation].m_pInstanceData[0];
	return pInstanceData->GetInstanceCommandBuffer();
}

void CMaterial::DrawMesh( VertexCompressionType_t vertexCompression, bool bIsAlphaModulating, bool bRenderingPreTessPatchMesh )
{
	if ( m_pShader )
	{
#ifdef _DEBUG
		if ( GetMaterialVarFlags() & MATERIAL_VAR_DEBUG )
		{
			// Putcher breakpoint here to catch the rendering of a material
			// marked for debugging ($debug = 1 in a .vmt file) dynamic state version
			int x = 0;
		}
#endif
		if ((GetMaterialVarFlags() & MATERIAL_VAR_NO_DRAW) == 0)
		{
			ShaderSystem()->DrawElements( m_pShader, m_pShaderParams, &m_ShaderRenderState, vertexCompression,
				m_ChangeID ^ g_nDebugVarsSignature, bIsAlphaModulating ? SHADER_USING_ALPHA_MODULATION : 0, bRenderingPreTessPatchMesh );
		}
	}
	else
	{
		Warning( "CMaterial::DrawElements: No bound shader\n" );
	}
}

#ifdef _WIN32
#pragma warning (default: 4189)
#endif

IShader *CMaterial::GetShader( ) const
{
	return m_pShader;
}

IMaterialVar *CMaterial::GetShaderParam( int id )
{
	return m_pShaderParams[id];
}


//-----------------------------------------------------------------------------
// Adds a material variable to the material
//-----------------------------------------------------------------------------
void CMaterial::AddMaterialVar( IMaterialVar *pMaterialVar )
{
	++m_VarCount;
	m_pShaderParams = (IMaterialVar**)realloc( m_pShaderParams, m_VarCount * sizeof( IMaterialVar*) );
	m_pShaderParams[m_VarCount-1] = pMaterialVar;
}


bool CMaterial::IsErrorMaterial() const
{
	extern IMaterialInternal *g_pErrorMaterial;
	const IMaterialInternal *pThis = this;
	return g_pErrorMaterial == pThis;
}


void CMaterial::FindRepresentativeTexture( void )
{
	Precache_Inline();
	
	// First try to find the base texture...
	bool found;
	IMaterialVar *textureVar = FindVar( "$baseTexture", &found, false );
	if( found && textureVar->GetType() == MATERIAL_VAR_TYPE_TEXTURE )
	{
		ITextureInternal *pTexture = ( ITextureInternal * )textureVar->GetTextureValue();
		if( pTexture )
		{
			pTexture->GetReflectivity( m_Reflectivity );
		}
	}
	if( !found || textureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
	{
		// Try the flowmap for water
		textureVar = FindVar( "$flowmap", &found, false );
		if( !found || textureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
		{
			// Try the env map mask if the base texture doesn't work...
			// this is needed for specular decals
			textureVar = FindVar( "$envmapmask", &found, false );
			if( !found || textureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
			{
				// Try the bumpmap
				textureVar = FindVar( "$bumpmap", &found, false );
				if( !found || textureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
				{
					textureVar = FindVar( "$dudvmap", &found, false );
					if( !found || textureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
					{
						textureVar = FindVar( "$normalmap", &found, false );
						if( !found || textureVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE )
						{
							//				Warning( "Can't find representative texture for material \"%s\"\n",	GetName() );
							m_representativeTexture = TextureManager()->ErrorTexture();
							return;
						}
					}
				}
			}
		}
	}

	m_representativeTexture = static_cast<ITextureInternal *>( textureVar->GetTextureValue() );
	if (m_representativeTexture)
	{
		m_representativeTexture->Precache();
	}
	else
	{
		m_representativeTexture = TextureManager()->ErrorTexture();
		Assert( m_representativeTexture );
	}
}


void CMaterial::GetLowResColorSample( float s, float t, float *color ) const
{
	if( !m_representativeTexture )
	{
		return;
	}
	m_representativeTexture->GetLowResColorSample( s, t, color);
}


//-----------------------------------------------------------------------------
// Lightmap-related methods
//-----------------------------------------------------------------------------

void CMaterial::SetMinLightmapPageID( int pageID )
{
	m_minLightmapPageID = pageID;
}

void CMaterial::SetMaxLightmapPageID( int pageID )
{
	m_maxLightmapPageID = pageID;
}

int CMaterial::GetMinLightmapPageID( ) const
{
	return m_minLightmapPageID;
}

int	CMaterial::GetMaxLightmapPageID( ) const
{
	return m_maxLightmapPageID;
}

void CMaterial::SetNeedsWhiteLightmap( bool val )
{
	if ( val )
		m_Flags |= MATERIAL_NEEDS_WHITE_LIGHTMAP;
	else
		m_Flags &= ~MATERIAL_NEEDS_WHITE_LIGHTMAP;
}

bool CMaterial::GetNeedsWhiteLightmap( ) const
{
	return (m_Flags & MATERIAL_NEEDS_WHITE_LIGHTMAP) != 0;
}

void CMaterial::MarkAsPreloaded( bool bSet )
{
	if ( bSet )
	{
		m_Flags |= MATERIAL_IS_PRELOADED;
	}
	else
	{
		m_Flags &= ~MATERIAL_IS_PRELOADED;
	}
}

bool CMaterial::IsPreloaded() const
{
	return ( m_Flags & MATERIAL_IS_PRELOADED ) != 0;
}

void CMaterial::ArtificialAddRef( void )
{
	if ( m_Flags & MATERIAL_ARTIFICIAL_REFCOUNT )
	{
		// already done
		return;
	}

	m_Flags |= MATERIAL_ARTIFICIAL_REFCOUNT;
	m_RefCount++;
}

void CMaterial::ArtificialRelease( void )
{
	if ( !( m_Flags & MATERIAL_ARTIFICIAL_REFCOUNT ) )
	{
		return;
	}

	m_Flags &= ~MATERIAL_ARTIFICIAL_REFCOUNT;
	m_RefCount--;
}

//-----------------------------------------------------------------------------
// Return the shader params
//-----------------------------------------------------------------------------
IMaterialVar **CMaterial::GetShaderParams( void )
{
	return m_pShaderParams;
}

int CMaterial::ShaderParamCount() const
{
	return m_VarCount;
}


//-----------------------------------------------------------------------------
// VMT parser
//-----------------------------------------------------------------------------
void InsertKeyValues( KeyValues& dst, KeyValues& src, bool bCheckForExistence )
{
	KeyValues *pSrcVar = src.GetFirstSubKey();
	while( pSrcVar )
	{
		if ( !bCheckForExistence || dst.FindKey( pSrcVar->GetName() ) )
		{
			switch( pSrcVar->GetDataType() )
			{
			case KeyValues::TYPE_STRING:
				dst.SetString( pSrcVar->GetName(), pSrcVar->GetString() );
				break;
			case KeyValues::TYPE_INT:
				dst.SetInt( pSrcVar->GetName(), pSrcVar->GetInt() );
				break;
			case KeyValues::TYPE_FLOAT:
				dst.SetFloat( pSrcVar->GetName(), pSrcVar->GetFloat() );
				break;
			case KeyValues::TYPE_PTR:
				dst.SetPtr( pSrcVar->GetName(), pSrcVar->GetPtr() );
				break;
			}
		}
		pSrcVar = pSrcVar->GetNextKey();
	}

	if( bCheckForExistence )
	{
		for( KeyValues *pScan = dst.GetFirstTrueSubKey(); pScan; pScan = pScan->GetNextTrueSubKey() )
		{
			KeyValues *pTmp = src.FindKey( pScan->GetName() );
			if( !pTmp )
				continue;
			// make sure that this is a subkey.
			if( pTmp->GetDataType() != KeyValues::TYPE_NONE )
				continue;
			InsertKeyValues( *pScan, *pTmp, bCheckForExistence );
		}
	}
}

void WriteKeyValuesToFile( const char *pFileName, KeyValues& keyValues )
{
	keyValues.SaveToFile( g_pFullFileSystem, pFileName );
}

void ApplyPatchKeyValues( KeyValues &keyValues, KeyValues &patchKeyValues )
{
	KeyValues *pInsertSection = patchKeyValues.FindKey( "insert" );
	KeyValues *pReplaceSection = patchKeyValues.FindKey( "replace" );

	if ( pInsertSection )
	{
		InsertKeyValues( keyValues, *pInsertSection, false );
	}

	if ( pReplaceSection )
	{
		InsertKeyValues( keyValues, *pReplaceSection, true );
	}

	// Could add other commands here, like "delete", "rename", etc.
}

//-----------------------------------------------------------------------------
// Adds keys from srcKeys to destKeys, overwriting any keys that are already
// there. Doesn't work recursively.
//-----------------------------------------------------------------------------
void MergeKeyValues( KeyValues &srcKeys, KeyValues &destKeys )
{
	for( KeyValues *pKV = srcKeys.GetFirstValue(); pKV; pKV = pKV->GetNextValue() )
	{
		switch( pKV->GetDataType() )
		{
		case KeyValues::TYPE_STRING:
			destKeys.SetString( pKV->GetName(), pKV->GetString() );
			break;
		case KeyValues::TYPE_INT:
			destKeys.SetInt( pKV->GetName(), pKV->GetInt() );
			break;
		case KeyValues::TYPE_FLOAT:
			destKeys.SetFloat( pKV->GetName(), pKV->GetFloat() );
			break;
		case KeyValues::TYPE_PTR:
			destKeys.SetPtr( pKV->GetName(), pKV->GetPtr() );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void AccumulatePatchKeyValues( KeyValues &srcKeyValues, KeyValues &patchKeyValues )
{
	KeyValues *pDestInsertSection = patchKeyValues.FindKey( "insert" );
	if ( pDestInsertSection == NULL )
	{
		pDestInsertSection = new KeyValues( "insert" );
		patchKeyValues.AddSubKey( pDestInsertSection );
	}

	KeyValues *pDestReplaceSection = patchKeyValues.FindKey( "replace" );
	if ( pDestReplaceSection == NULL )
	{
		pDestReplaceSection = new KeyValues( "replace" );
		patchKeyValues.AddSubKey( pDestReplaceSection );
	}

	KeyValues *pSrcInsertSection = srcKeyValues.FindKey( "insert" );
	if ( pSrcInsertSection )
	{
		MergeKeyValues( *pSrcInsertSection, *pDestInsertSection );
	}

	KeyValues *pSrcReplaceSection = srcKeyValues.FindKey( "replace" );
	if ( pSrcReplaceSection )
	{
		MergeKeyValues( *pSrcReplaceSection, *pDestReplaceSection );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool AccumulateRecursiveVmtPatches( KeyValues &patchKeyValuesOut, KeyValues **ppBaseKeyValuesOut, const KeyValues& keyValues, const char *pPathID, CUtlVector<FileNameHandle_t> *pIncludes )
{
	if ( pIncludes )
	{
		pIncludes->Purge();
	}

	patchKeyValuesOut.Clear();

	if ( V_stricmp( keyValues.GetName(), "patch" ) != 0 )
	{
		// Not a patch file, nothing to do
		if ( ppBaseKeyValuesOut )
		{
			// flag to the caller that the passed in keyValues are in fact final non-patch values
			*ppBaseKeyValuesOut = NULL;
		}
		return true;
	}

	KeyValues *pCurrentKeyValues = keyValues.MakeCopy();

	// Recurse down through all patch files:
	int nCount = 0;
	while( ( nCount < 10 ) && ( V_stricmp( pCurrentKeyValues->GetName(), "patch" ) == 0 ) )
	{
		// Accumulate the new patch keys from this file
		AccumulatePatchKeyValues( *pCurrentKeyValues, patchKeyValuesOut );
		
		// Load the included file
		const char *pIncludeFileName = pCurrentKeyValues->GetString( "include" );

		if ( pIncludeFileName == NULL )
		{
			// A patch file without an $include key? Not good...
			Warning( "VMT patch file has no $include key - invalid!\n" );
			Assert( pIncludeFileName );
			break;
		}

		CUtlString includeFileName( pIncludeFileName ); // copy off the string before we clear the keyvalues it lives in
		pCurrentKeyValues->Clear();
		bool bSuccess = pCurrentKeyValues->LoadFromFile( g_pFullFileSystem, includeFileName, pPathID );
		if( bSuccess )
		{
			if ( pIncludes )
			{
				// Remember that we included this file for the pure server stuff.
				pIncludes->AddToTail( g_pFullFileSystem->FindOrAddFileName( includeFileName ) );
			}
		}
		else
		{
			pCurrentKeyValues->deleteThis();
			#ifndef DEDICATED
			{
				Warning( "Failed to load $include VMT file (%s)\n", includeFileName.Get() );
			}
			#endif
			Assert( bSuccess );
			return false;
		}

		nCount++;
	}

	if ( ppBaseKeyValuesOut )
	{
		*ppBaseKeyValuesOut = pCurrentKeyValues;
	}
	else
	{
		pCurrentKeyValues->deleteThis();
	}

	if( nCount >= 10 )
	{
		Warning( "Infinite recursion in patch file?\n" );
	}
	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void ExpandPatchFile( KeyValues& keyValues, KeyValues &patchKeyValues, const char *pPathID, CUtlVector<FileNameHandle_t> *pIncludes )
{
	KeyValues *pNonPatchKeyValues = NULL;
	bool bSuccess = AccumulateRecursiveVmtPatches( patchKeyValues, &pNonPatchKeyValues, keyValues, pPathID, pIncludes );
	if ( !bSuccess )
	{
		return;
	}

	if ( pNonPatchKeyValues != NULL )
	{
		// We're dealing with a patch file. Apply accumulated patches to final vmt
		ApplyPatchKeyValues( *pNonPatchKeyValues, patchKeyValues );
		keyValues = *pNonPatchKeyValues;
		pNonPatchKeyValues->deleteThis();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool LoadVMTFile( KeyValues &vmtKeyValues, KeyValues &patchKeyValues, const char *pMaterialName, bool bAbsolutePath, CUtlVector<FileNameHandle_t> *pIncludes )
{
	MEM_ALLOC_CREDIT();
	char pFileName[MAX_PATH];
	const char *pPathID = "GAME";
	if ( !bAbsolutePath )
	{
		Q_snprintf( pFileName, sizeof( pFileName ), "materials/%s.vmt", pMaterialName );
	}
	else
	{
		Q_snprintf( pFileName, sizeof( pFileName ), "%s.vmt", pMaterialName );
		if ( pMaterialName[0] == '/' && pMaterialName[1] == '/' && pMaterialName[2] != '/' )
		{
			// UNC, do full search
			pPathID = NULL;
		}
	}

	if ( !vmtKeyValues.LoadFromFile( g_pFullFileSystem, pFileName, pPathID ) )
	{
		return false;
	}
	ExpandPatchFile( vmtKeyValues, patchKeyValues, pPathID, pIncludes );

	return true;
}

int CMaterial::GetNumPasses( void )
{
	Precache_Inline();
//	int mod = m_ShaderRenderState.m_Modulation;
	int mod = 0;
	return m_ShaderRenderState.m_pSnapshots[mod].m_nPassCount;
}

int CMaterial::GetTextureMemoryBytes( void )
{
	Precache_Inline();
	int bytes = 0;
	int i;
	for( i = 0; i < m_VarCount; i++ )
	{
		IMaterialVar *pVar = m_pShaderParams[i];
		if( pVar->GetType() == MATERIAL_VAR_TYPE_TEXTURE )
		{
			ITexture *pTexture = pVar->GetTextureValue();
			if( pTexture && pTexture != ( ITexture * )0xffffffff )
			{
				bytes += pTexture->GetApproximateVidMemBytes();
			}
		}
	}
	return bytes;
}

bool CMaterial::NeedsFixedFunctionFlashlight() const
{
	return ( GetMaterialVarFlags2() & MATERIAL_VAR2_NEEDS_FIXED_FUNCTION_FLASHLIGHT ) &&
		MaterialSystem()->InFlashlightMode();
}

bool CMaterial::IsUsingVertexID( ) const
{
	return ( GetMaterialVarFlags2() & MATERIAL_VAR2_USES_VERTEXID ) != 0;
}

void CMaterial::DeleteIfUnreferenced()
{
	if ( m_RefCount > 0 )
		return;
	IMaterialVar::DeleteUnreferencedTextures( true );
	MaterialSystem()->RemoveMaterial( this );
	IMaterialVar::DeleteUnreferencedTextures( false );
}

void CMaterial::CompactMaterialVars()
{
	if ( m_ProxyCount == 0)
	{
		::CompactMaterialVars( GetShaderParams(), ShaderParamCount() );
	}
}

bool CMaterial::HasQueueFriendlyProxies() const
{
	if ( !HasProxy() )
		return false;
	
	bool anyAsync = false;
	bool allAsync = true;
	for  ( int i = 0; i < m_ProxyCount; ++i )
	{
		const bool isAsync = m_ppProxies[ i ]->CanBeCalledAsync();
		anyAsync = anyAsync || isAsync;
		allAsync = allAsync && isAsync;
	}
	
	// The code doesn't currently handle this, it would need to be updated.
	Assert( anyAsync == allAsync );

	return anyAsync;
}


bool CMaterial::SetTempExcluded( bool bSet, int nExcludedDimensionLimit )
{
	// iterate all texture variables
	for ( int i = 0; i < m_VarCount; i++ )
	{
		// there are some texture which we will absolutely not temp exclude
		if ( i == FLASHLIGHTTEXTURE )
			continue;

		IMaterialVar *pVar = m_pShaderParams[i];
		if ( !pVar || 
			pVar->GetType() != MATERIAL_VAR_TYPE_TEXTURE || 
			pVar->IsTextureValueInternalEnvCubemap() )
		{
			// not possible to temp exclude these
			continue;
		}

		ITexture *pTexture = pVar->GetTextureValue();
		if ( pTexture && ((ITextureInternal*)pTexture)->MarkAsTempExcluded( bSet, nExcludedDimensionLimit ) )
		{
			DevMsg( "SetTempExcluded: %s: %s (%d)\n", bSet ? "Excluding" : "Restoring", pTexture->GetName(), ((ITextureInternal*)pTexture)->GetReferenceCount() );
			((ITextureInternal*)pTexture)->UpdateExcludedState();
		}
	}

	return true;
}
