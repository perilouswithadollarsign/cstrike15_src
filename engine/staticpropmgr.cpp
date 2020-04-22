//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Revision: $
// $NoKeywords: $
//
// This file contains code to allow us to associate client data with bsp leaves.
//
//===========================================================================//


#include "staticpropmgr.h"
#include "convar.h"
#include "vcollide_parse.h"
#include "engine/ICollideable.h"
#include "iclientunknown.h"
#include "iclientrenderable.h"
#include "gamebspfile.h"
#include "engine/ivmodelrender.h"
#include "engine/IClientLeafSystem.h"
#include "ispatialpartitioninternal.h"
#include "utlbuffer.h"
#include "utlvector.h"
#include "filesystem.h"
#include "gl_model_private.h"
#include "gl_matsysiface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/ivballoctracker.h"
#include "materialsystem/imesh.h"
#include "lightcache.h"
#include "tier0/vprof.h"
#include "render.h"
#include "cmodel_engine.h"
#include "datacache/imdlcache.h"
#include "ModelInfo.h"
#include "cdll_engine_int.h"
#include "tier0/dbg.h"
#include "debugoverlay.h"
#include "draw.h"
#include "client.h"
#include "server.h"
#include "l_studio.h"
#include "tier0/icommandline.h"
#include "sys_dll.h"
#include "generichash.h"
#include "tier2/renderutils.h"
#include "ipooledvballocator.h"
#include "tier3/tier3.h"
#include "gl_cvars.h"
#include "shaderapi/ishaderapi.h"
#include "iclientalphaproperty.h"
#include "vgui_baseui_interface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Convars!
//-----------------------------------------------------------------------------
static ConVar r_DrawSpecificStaticProp( "r_DrawSpecificStaticProp", "-1" );
static ConVar r_drawstaticprops( "r_drawstaticprops", "1", FCVAR_CHEAT, "0=Off, 1=Normal, 2=Wireframe" );
extern ConVar r_slowpathwireframe;
static ConVar r_colorstaticprops( "r_colorstaticprops", "0", FCVAR_CHEAT );
ConVar r_staticpropinfo( "r_staticpropinfo", "0" );
ConVar  r_drawmodeldecals( "r_drawmodeldecals", "1" );
ConVar disableStaticPropLoading( "disable_static_prop_loading", "0", FCVAR_CHEAT, "If non-zero when a map loads, static props won't be loaded" );
extern ConVar mat_fullbright;
static bool g_MakingDevShots = false;
ConVar r_shadow_deferred( "r_shadow_deferred", "0", FCVAR_CHEAT, "Toggle deferred shadow rendering");

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_StaticPropManager, "StaticPropManager", 0, LS_ERROR );

//-----------------------------------------------------------------------------
// Index into the fade list
//-----------------------------------------------------------------------------
enum
{
	INVALID_FADE_INDEX = (unsigned short)~0
};


//-----------------------------------------------------------------------------
// All static props have these bits set (to differentiate them from edict indices)
//-----------------------------------------------------------------------------
enum
{
	// This bit will be set in GetRefEHandle for all static props
	STATICPROP_EHANDLE_MASK = 0x40000000
};


//-----------------------------------------------------------------------------
// A default physics property for non-vphysics static props
//-----------------------------------------------------------------------------
static const objectparams_t g_PhysDefaultObjectParams =
{
	NULL,
		1.0, //mass
		1.0, // inertia
		0.1f, // damping
		0.1f, // rotdamping
		0.05f, // rotIntertiaLimit
		"DEFAULT",
		NULL,// game data
		0.f, // volume (leave 0 if you don't have one or call physcollision->CollideVolume() to compute it)
		1.0f, // drag coefficient
		true,// enable collisions?
};

// return true if the renderer should use the slow path that supports the various debug modes
inline bool IsUsingStaticPropDebugModes()
{
	if ( r_drawstaticprops.GetInt() != 1 ||
		r_DrawSpecificStaticProp.GetInt() >= 0 ||
		r_colorstaticprops.GetBool() ||
		r_staticpropinfo.GetInt() ||
		mat_fullbright.GetInt() ||
		r_drawmodellightorigin.GetBool() ||
		ShouldDrawInWireFrameMode() ||
		r_drawmodelstatsoverlay.GetBool() )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// A static prop
//-----------------------------------------------------------------------------
class CStaticProp : public IClientUnknown, public IClientRenderable, public ICollideable, public IClientModelRenderable
{
public:
	CStaticProp();
	~CStaticProp();

	// IHandleEntity overrides
public:
	virtual void SetRefEHandle( const CBaseHandle &handle );
	virtual const CBaseHandle& GetRefEHandle() const;

	// IClientUnknown overrides.
public:
	virtual IClientUnknown*		GetIClientUnknown()		{ return this; }
	virtual ICollideable*		GetCollideable()		{ return this; }
	virtual IClientNetworkable*	GetClientNetworkable()	{ return NULL; }
	virtual IClientRenderable*	GetClientRenderable()	{ return this; }
	virtual IClientEntity*		GetIClientEntity()		{ return NULL; }
	virtual C_BaseEntity*		GetBaseEntity()			{ return NULL; }
	virtual IClientThinkable*	GetClientThinkable()	{ return NULL; }
	virtual IClientAlphaProperty*	GetClientAlphaProperty()	{ return m_pClientAlphaProperty; }

public:
	// These methods return a box defined in the space of the entity
	virtual const Vector&	OBBMins( ) const;
	virtual const Vector&	OBBMaxs( ) const;

	// custom collision test
	virtual bool			TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	// Perform hitbox test, returns true *if hitboxes were tested at all*!!
	virtual bool			TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr );

	// Returns the BRUSH model index if this is a brush model. Otherwise, returns -1.
	virtual int				GetCollisionModelIndex();

	// Return the model, if it's a studio model.
	virtual const model_t*	GetCollisionModel();

	// Get angles and origin.
	virtual const Vector&	GetCollisionOrigin() const;
	virtual const QAngle&	GetCollisionAngles() const;
	virtual const matrix3x4_t&	CollisionToWorldTransform() const;

	// Return a SOLID_ define.
	virtual SolidType_t		GetSolid() const;
	virtual int				GetSolidFlags() const;

	// Gets at the entity handle associated with the collideable
	virtual IHandleEntity	*GetEntityHandle() { return this; }

	virtual int				GetCollisionGroup() const { return COLLISION_GROUP_NONE; }

	virtual void			WorldSpaceTriggerBounds( Vector* pVecWorldMins, Vector *pVecWorldMaxs ) const;
	virtual void			WorldSpaceSurroundingBounds( Vector* pVecWorldMins, Vector *pVecWorldMaxs );
	virtual uint			GetRequiredTriggerFlags() const { return 0; }
	virtual const matrix3x4_t	*GetRootParentToWorldTransform() const { return NULL; }
	virtual IPhysicsObject	*GetVPhysicsObject() const { return NULL; }


	// IClientRenderable overrides.
public:
	virtual int				GetBody() { return 0; }
	virtual int				GetSkin() { return m_Skin; }
	virtual const Vector&	GetRenderOrigin( );
	virtual const QAngle&	GetRenderAngles( );
	virtual bool			ShouldDraw();
	virtual uint8			OverrideAlphaModulation( uint8 nAlpha ) { return nAlpha; }
	virtual uint8			OverrideShadowAlphaModulation( uint8 nAlpha ) { return nAlpha; }
	virtual void			OnThreadedDrawSetup() {}
	virtual const model_t*	GetModel( ) const;
	virtual int				DrawModel( int flags, const RenderableInstance_t &instance );
	virtual void			GetColorModulation( float* color );
	virtual bool			LODTest() { return true; } // NOTE: UNUSED
	virtual bool			SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime );
	virtual void			SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );
	virtual bool			UsesFlexDelayedWeights() { return false; }
	virtual void			DoAnimationEvents( void );
	virtual IPVSNotify*		GetPVSNotifyInterface();
	virtual void			GetRenderBounds( Vector& mins, Vector& maxs );
	virtual void			GetRenderBoundsWorldspace( Vector& mins, Vector& maxs );
	virtual bool			ShouldCacheRenderInfo();
	virtual bool			ShouldReceiveProjectedTextures( int flags );
	virtual bool			GetShadowCastDistance( float *pDist, ShadowType_t shadowType ) const			{ return false; }
	virtual bool			GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const	{ return false; }
	virtual int			    GetRenderFlags( void );
	virtual ClientShadowHandle_t	GetShadowHandle() const { return CLIENTSHADOW_INVALID_HANDLE; }
	virtual ClientRenderHandle_t&	RenderHandle();
	virtual void	RecordToolMessage() {}
	virtual bool	ShouldDrawForSplitScreenUser( int nSlot ) { return true; }

	// These normally call through to GetRenderAngles/GetRenderBounds, but some entities custom implement them.
	virtual void			GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
	{
		GetRenderBounds( mins, maxs );
	}

	// Other methods related to shadow rendering
	virtual bool	IsShadowDirty( ) { return false; }
	virtual void	MarkShadowDirty( bool bDirty ) {}

	// Iteration over shadow hierarchy
	virtual IClientRenderable *GetShadowParent() { return NULL; }
	virtual IClientRenderable *FirstShadowChild() { return NULL; }
	virtual IClientRenderable *NextShadowPeer() { return NULL; }

	// Returns the shadow cast type
	virtual ShadowType_t ShadowCastType() { return SHADOWS_NONE; }

	// Create/get/destroy model instance
	virtual void CreateModelInstance() { Assert(0); }
	virtual ModelInstanceHandle_t GetModelInstance();

	// Attachments
	virtual int LookupAttachment( const char *pAttachmentName ) { return -1; }
	virtual	bool GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool ComputeLightingOrigin( int nAttachmentIndex, Vector modelLightingCenter, const matrix3x4_t &matrix, Vector &transformedLightingCenter );

	// Rendering clip plane, should be 4 floats, return value of NULL indicates a disabled render clip plane
	virtual float *GetRenderClipPlane( void ) { return NULL; }

	// Returns the transform from RenderOrigin/RenderAngles to world
	virtual const matrix3x4_t &RenderableToWorldTransform()
	{
		return m_ModelToWorld;
	}
	virtual IClientModelRenderable*	GetClientModelRenderable();

	// IClientModelRenderable overrides
public:
	virtual bool GetRenderData( void *pData, ModelDataCategory_t nCategory );

public:
	bool Init( int index, StaticPropLump_t &lump, model_t *pModel );
	// KD Tree
	void InsertPropIntoKDTree();
	void RemovePropFromKDTree();

	void PrecacheLighting();
	void RecomputeStaticLighting();

	int LeafCount() const;
	int FirstLeaf() const;
	LightCacheHandle_t GetLightCacheHandle() const;
	void SetModelInstance( ModelInstanceHandle_t handle );
	void SetRenderHandle( ClientRenderHandle_t handle );
	void CleanUpRenderHandle( );
	ClientRenderHandle_t GetRenderHandle() const;
	void CleanUpAlphaProperty();

	// Create VPhysics representation
	void CreateVPhysics( IPhysicsEnvironment *physenv, IVPhysicsKeyHandler *pDefaults, void *pGameData );

	float Radius() const { return m_flRadius; }
	int Flags() const { return m_Flags; }
	int FlagsEx() const { return m_FlagsEx; }
	void DisableCSMRendering( void ) { m_FlagsEx |= STATIC_PROP_FLAGS_EX_DISABLE_CSM; }

	int	DrawModelSlow( int flags, const RenderableInstance_t &instance );

private:
	// Diagnostic information for static props
	void DisplayStaticPropInfo( int nInfoType );
	inline void InitModelRenderInfo( ModelRenderInfo_t &sInfo, int flags )
	{
		sInfo.origin = m_Origin;
		sInfo.angles = m_Angles;
		sInfo.pRenderable = this;
		sInfo.pModel = m_pModel;
		sInfo.pModelToWorld = &m_ModelToWorld;
		sInfo.pLightingOffset = NULL;
		sInfo.pLightingOrigin = &m_LightingOrigin;
		sInfo.flags = flags;
		sInfo.entity_index = -1;
		sInfo.skin = m_Skin;
		sInfo.body = 0;
		sInfo.hitboxset = 0;
		sInfo.instance = m_ModelInstance;
	}

private:
	friend class CStaticPropMgr;
	Vector					m_Origin;
	QAngle					m_Angles;
	model_t*				m_pModel;
	SpatialPartitionHandle_t	m_Partition;
	ModelInstanceHandle_t	m_ModelInstance;

	unsigned char			m_Alpha;
	unsigned char			m_nSolidType;
	unsigned char			m_Skin;
	unsigned char			m_Flags;
	unsigned int			m_FlagsEx;

	unsigned char			m_nMinCPULevel;
	unsigned char			m_nMaxCPULevel;
	unsigned char			m_nMinGPULevel;
	unsigned char			m_nMaxGPULevel;

	unsigned short			m_FirstLeaf;
	unsigned short			m_LeafCount;

	CBaseHandle				m_EntHandle;	// FIXME: Do I need client + server handles?

	ClientRenderHandle_t	m_RenderHandle;
	unsigned short			m_nReserved;

	IClientAlphaProperty*	m_pClientAlphaProperty;

	// bbox is the same for both GetBounds and GetRenderBounds since static props never move.
	// GetRenderBounds is interpolated data, and GetBounds is last networked.
	Vector					m_RenderBBoxMin;
	Vector					m_RenderBBoxMax;
	matrix3x4_t				m_ModelToWorld;
	float					m_flRadius;

	Vector					m_WorldRenderBBoxMin;
	Vector					m_WorldRenderBBoxMax;

	// FIXME: This sucks. Need to store the lighting origin off
	// because the time at which the static props are unserialized
	// doesn't necessarily match the time at which we can initialize the light cache
	Vector					m_LightingOrigin;
	Vector4D				m_DiffuseModulation;
};


//-----------------------------------------------------------------------------
// The engine's static prop manager
//-----------------------------------------------------------------------------
class CStaticPropMgr : public IStaticPropMgrEngine, public IStaticPropMgrClient, public IStaticPropMgrServer
{
public:
	// constructor, destructor
	CStaticPropMgr();
	virtual ~CStaticPropMgr();

	// methods of IStaticPropMgrEngine
	virtual bool Init();
	virtual void Shutdown();
	virtual void LevelInit();
	virtual void LevelInitClient();
	virtual void LevelShutdown();
	virtual void LevelShutdownClient();
	virtual bool IsPropInPVS( IHandleEntity *pHandleEntity, const byte *pVis ) const;
	virtual void DisableCSMRenderingForStaticProp( int staticPropIndex );
	virtual ICollideable *GetStaticProp( IHandleEntity *pHandleEntity );
	virtual void RecomputeStaticLighting( );
	virtual LightCacheHandle_t GetLightCacheHandleForStaticProp( IHandleEntity *pHandleEntity );
	inline bool IsStaticProp_Inline( IHandleEntity *pHandleEntity ) const
	{
#if defined( _GAMECONSOLE )
		return (!pHandleEntity) || pHandleEntity->m_bIsStaticProp;
#else
		return (!pHandleEntity) || ( (pHandleEntity->GetRefEHandle().GetSerialNumber() == (STATICPROP_EHANDLE_MASK >> NUM_SERIAL_NUM_SHIFT_BITS) ) != 0 );
#endif
	}
	virtual bool IsStaticProp( IHandleEntity *pHandleEntity ) const;
	virtual bool IsStaticProp( CBaseHandle handle ) const;
	virtual int GetStaticPropIndex( IHandleEntity *pHandleEntity ) const;
	virtual ICollideable *GetStaticPropByIndex( int propIndex );

	// methods of IStaticPropMgrClient
	virtual void TraceRayAgainstStaticProp( const Ray_t& ray, int staticPropIndex, trace_t& tr );
	virtual void AddDecalToStaticProp( Vector const& rayStart, Vector const& rayEnd,
		int staticPropIndex, int decalIndex, bool doTrace, trace_t& tr, void *pvProxyUserData = NULL, const Vector* saxis = NULL, int32 nAdditionalDecalFlags = 0 ) OVERRIDE;
	virtual void AddShadowToStaticProp( unsigned short shadowHandle, IClientRenderable* pRenderable );
	virtual void RemoveAllShadowsFromStaticProp( IClientRenderable* pRenderable );
	virtual void GetStaticPropMaterialColorAndLighting( trace_t* pTrace,
		int staticPropIndex, Vector& lighting, Vector& matColor );
	virtual void CreateVPhysicsRepresentations( IPhysicsEnvironment	*physenv, IVPhysicsKeyHandler *pDefaults, void *pGameData );

	// methods of IStaticPropMgrServer

	//Changes made specifically to support the Portal mod (smack Dave Kircher if something breaks)
	//===================================================================
	virtual void GetAllStaticProps( CUtlVector<ICollideable *> *pOutput );
	virtual void GetAllStaticPropsInAABB( const Vector &vMins, const Vector &vMaxs, CUtlVector<ICollideable *> *pOutput );
	virtual void GetAllStaticPropsInOBB( const Vector &ptOrigin, const Vector &vExtent1, const Vector &vExtent2, const Vector &vExtent3, CUtlVector<ICollideable *> *pOutput );
	//===================================================================

	virtual void GetLightingOrigins( Vector *pLightingOrigins, int nOriginStride, int nCount, IClientRenderable **ppRenderable, int nRenderableStride ); 
	virtual void ConfigureSystemLevel( int nCPULevel, int nGPULevel );
	virtual void RestoreStaticProps();

	// Internal methods
	const Vector &ViewOrigin() const { return m_vecLastViewOrigin; }

	// Computes the opacity for a single static prop
	void DrawStaticProps( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth, bool drawVCollideWireframe );
	void DrawStaticProps_Slow( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth, bool drawVCollideWireframe );
	void DrawStaticProps_Fast( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth );
	void DrawStaticProps_FastPipeline( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth );

private:
	void OutputLevelStats( void );
	void PrecacheLighting();
	void UpdatePropVisibility( int nCPULevel, int nGPULevel );

	// Methods associated with unserializing static props
	void UnserializeModelDict( CUtlBuffer& buf );
	void UnserializeLeafList( CUtlBuffer& buf );
	void UnserializeModels( CUtlBuffer& buf );
	void UnserializeStaticProps();

	int HandleEntityToIndex( IHandleEntity *pHandleEntity ) const;

private:
	// Unique static prop models

	// The list of all static props
	CUtlVector <model_t *>			m_StaticPropDict;
	CUtlVector <CStaticProp>		m_StaticProps;
	CUtlVector <StaticPropLeafLump_t> m_StaticPropLeaves;

	bool							m_bLevelInitialized;
	bool							m_bClientInitialized;
	Vector							m_vecLastViewOrigin;
	float							m_flLastViewFactor;

	int								m_nLastCPULevel;
	int								m_nLastGPULevel;
};


//-----------------------------------------------------------------------------
// Expose Interface to the game + client DLLs.
//-----------------------------------------------------------------------------
static CStaticPropMgr	s_StaticPropMgr;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CStaticPropMgr, IStaticPropMgrClient, INTERFACEVERSION_STATICPROPMGR_CLIENT, s_StaticPropMgr);
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CStaticPropMgr, IStaticPropMgrServer, INTERFACEVERSION_STATICPROPMGR_SERVER, s_StaticPropMgr);


//-----------------------------------------------------------------------------
//
// Static prop
//
//-----------------------------------------------------------------------------
CStaticProp::CStaticProp() : m_pModel(0), m_Alpha(255)
{
#ifdef _GAMECONSOLE
	m_bIsStaticProp = true;
#endif
	m_ModelInstance = MODEL_INSTANCE_INVALID;
	m_Partition = PARTITION_INVALID_HANDLE;
	m_EntHandle = INVALID_EHANDLE;
	m_RenderHandle = INVALID_CLIENT_RENDER_HANDLE;
	m_pClientAlphaProperty = NULL;
}

CStaticProp::~CStaticProp()
{
	CleanUpAlphaProperty();
	RemovePropFromKDTree( );
	if (m_ModelInstance != MODEL_INSTANCE_INVALID)
	{
		modelrender->DestroyInstance( m_ModelInstance );
	}
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
bool CStaticProp::Init( int index, StaticPropLump_t &lump, model_t *pModel )
{
	m_EntHandle.Init(index, STATICPROP_EHANDLE_MASK >> NUM_SERIAL_NUM_SHIFT_BITS);
	m_Partition = PARTITION_INVALID_HANDLE;
	VectorCopy( lump.m_Origin, m_Origin );
	VectorCopy( lump.m_Angles, m_Angles );
	m_pModel = pModel;
	m_FirstLeaf = lump.m_FirstLeaf;
	m_LeafCount = lump.m_LeafCount;
	m_nSolidType = lump.m_Solid;
	m_DiffuseModulation[0] = lump.m_DiffuseModulation.r * ( 1.0f / 255.0f );
	m_DiffuseModulation[1] = lump.m_DiffuseModulation.g * ( 1.0f / 255.0f );
	m_DiffuseModulation[2] = lump.m_DiffuseModulation.b * ( 1.0f / 255.0f );
	m_DiffuseModulation[3] = lump.m_DiffuseModulation.a * ( 1.0f / 255.0f );
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( m_pModel );

	if ( pStudioHdr )
	{
		if ( !( pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) )
		{
			static int nBitchCount = 0;
			if( nBitchCount < 100 )
			{
				Log_Warning( LOG_StaticPropManager, "model %s used as a static prop, but not compiled as a static prop\n", pStudioHdr->pszName() );
				nBitchCount++;
			}
		}
	}

#ifndef DEDICATED
	// Initialize the alpha property
	if ( !sv.IsDedicated() ) 
	{
		CleanUpAlphaProperty();
		m_pClientAlphaProperty = g_pClientAlphaPropertyMgr->CreateClientAlphaProperty( this );
		m_pClientAlphaProperty->SetAlphaModulation( lump.m_DiffuseModulation.a );
		m_pClientAlphaProperty->SetDesyncOffset( index );
		m_pClientAlphaProperty->SetRenderFX( kRenderFxNone, kRenderTransTexture );

		float flForcedFadeScale = lump.m_flForcedFadeScale;
		if ( pStudioHdr->flags & STUDIOHDR_FLAGS_NO_FORCED_FADE )
		{
			flForcedFadeScale = 0.0f;
		}

		m_pClientAlphaProperty->SetFade( flForcedFadeScale, 0.0f, 0.0f );
		if ( lump.m_Flags & STATIC_PROP_FLAG_FADES )
		{
			m_pClientAlphaProperty->SetFade( flForcedFadeScale, lump.m_FadeMinDist, lump.m_FadeMaxDist );
		}
	}
#endif


	switch ( m_nSolidType )
	{
		// These are valid
	case SOLID_VPHYSICS:
	case SOLID_BBOX:
	case SOLID_NONE:
		break;

	default:
		{
			char szModel[MAX_PATH];
			Q_strncpy( szModel, m_pModel ? modelloader->GetName( m_pModel ) : "unknown model", sizeof( szModel ) );
			Log_Warning( LOG_StaticPropManager, "CStaticProp::Init:  Map error, static_prop with bogus SOLID_ flag (%d)! (%s)\n", m_nSolidType, szModel );
			m_nSolidType = SOLID_NONE;
		}
		break;
	}

	m_Alpha = 255;
	m_Skin = (unsigned char)lump.m_Skin;
	m_Flags = ( lump.m_Flags & ( STATIC_PROP_MARKED_FOR_FAST_REFLECTION | STATIC_PROP_NO_FLASHLIGHT | STATIC_PROP_NO_PER_VERTEX_LIGHTING ) );
	m_FlagsEx = (unsigned int)lump.m_FlagsEx;
		
	m_nMinCPULevel = lump.m_nMinCPULevel;
	m_nMaxCPULevel = lump.m_nMaxCPULevel;
	m_nMinGPULevel = lump.m_nMinGPULevel;
	m_nMaxGPULevel = lump.m_nMaxGPULevel;

	// Cache the model to world matrix since it never changes.
	AngleMatrix( lump.m_Angles, lump.m_Origin, m_ModelToWorld );

	// Cache the collision bounding box since it'll never change.
	modelinfo->GetModelRenderBounds( m_pModel, m_RenderBBoxMin, m_RenderBBoxMax );
	m_flRadius = m_RenderBBoxMin.DistTo( m_RenderBBoxMax ) * 0.5f;
	TransformAABB( m_ModelToWorld, m_RenderBBoxMin, m_RenderBBoxMax, m_WorldRenderBBoxMin, m_WorldRenderBBoxMax );

	// FIXME: Sucky, but unless we want to re-read the static prop lump when the client is
	// initialized (possible, but also gross), we need to cache off the illum center now
	if (lump.m_Flags & STATIC_PROP_USE_LIGHTING_ORIGIN)
	{
		m_LightingOrigin = lump.m_LightingOrigin;
	}
	else
	{
		modelinfo->GetIlluminationPoint( m_pModel, this, m_Origin, m_Angles, &m_LightingOrigin );
	}
	g_MakingDevShots = CommandLine()->FindParm( "-makedevshots" ) ? true : false;

	return true;
}


//-----------------------------------------------------------------------------
// EHandle
//-----------------------------------------------------------------------------
void CStaticProp::SetRefEHandle( const CBaseHandle &handle )
{
	// Only the static prop mgr should be setting this...
	Assert( 0 );
}


const CBaseHandle& CStaticProp::GetRefEHandle() const
{
	return m_EntHandle;
}


//-----------------------------------------------------------------------------
// These methods return a box defined in the space of the entity
//-----------------------------------------------------------------------------
const Vector& CStaticProp::OBBMins( ) const
{
	if ( GetSolid() == SOLID_VPHYSICS )
	{
		return m_pModel->mins;
	}
	Vector& tv = AllocTempVector();
	// FIXME: why doesn't this just return m_RenderBBoxMin?
	VectorSubtract( m_WorldRenderBBoxMin, GetCollisionOrigin(), tv );
	return tv;
}

const Vector& CStaticProp::OBBMaxs( ) const
{
	if ( GetSolid() == SOLID_VPHYSICS )
	{
		return m_pModel->maxs;
	}
	Vector& tv = AllocTempVector();
	// FIXME: why doesn't this just return m_RenderBBoxMax?
	VectorSubtract( m_WorldRenderBBoxMax, GetCollisionOrigin(), tv );
	return tv;
}

void CStaticProp::WorldSpaceTriggerBounds( Vector* pVecWorldMins, Vector *pVecWorldMaxs ) const
{
	// This should never be called..
	Assert(0);
}


//-----------------------------------------------------------------------------
// Surrounding box
//-----------------------------------------------------------------------------
void CStaticProp::WorldSpaceSurroundingBounds( Vector* pVecWorldMins, Vector *pVecWorldMaxs )
{
	*pVecWorldMins = m_WorldRenderBBoxMin;
	*pVecWorldMaxs = m_WorldRenderBBoxMax;
}


//-----------------------------------------------------------------------------
// Data accessors
//-----------------------------------------------------------------------------
const Vector& CStaticProp::GetRenderOrigin( void )
{
	return m_Origin;
}

const QAngle& CStaticProp::GetRenderAngles( void )
{
	return m_Angles;
}

bool CStaticProp::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	origin = m_Origin;
	angles = m_Angles;
	return true;
}

bool CStaticProp::GetAttachment( int number, matrix3x4_t &matrix )
{
	MatrixCopy( RenderableToWorldTransform(), matrix );
	return true;
}

bool CStaticProp::ComputeLightingOrigin( int nAttachmentIndex, Vector modelLightingCenter, const matrix3x4_t &matrix, Vector &transformedLightingCenter )
{
	if ( nAttachmentIndex <= 0 )
	{
		VectorTransform( modelLightingCenter, matrix, transformedLightingCenter );
	}
	else
	{
		matrix3x4_t attachmentTransform;
		GetAttachment( nAttachmentIndex, attachmentTransform );
		VectorTransform( modelLightingCenter, attachmentTransform, transformedLightingCenter );
	}
	return true;
}

bool CStaticProp::ShouldDraw()
{
	return ( m_RenderHandle != INVALID_CLIENT_RENDER_HANDLE );
}


//-----------------------------------------------------------------------------
// Render setup
//-----------------------------------------------------------------------------
bool CStaticProp::SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
{
	if (!m_pModel)
		return false;

	MatrixCopy( m_ModelToWorld, pBoneToWorldOut[0] );
	return true;
}

void	CStaticProp::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
}

void	CStaticProp::DoAnimationEvents( void )
{
}


//-----------------------------------------------------------------------------
// Render baby!
//-----------------------------------------------------------------------------
const model_t* CStaticProp::GetModel( ) const
{
	return m_pModel;
}


//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
IClientModelRenderable* CStaticProp::GetClientModelRenderable()
{
	// FIXME: Can I cache off ModelHasMaterialProxy?
	// Don't bother using fast path if proxies are happening
#ifndef DEDICATED
	if ( !m_pModel || modelinfoclient->ModelHasMaterialProxy( m_pModel ) )
		return NULL;
#endif

	if ( IsUsingStaticPropDebugModes() )
		return NULL;

	return this;
}

bool CStaticProp::GetRenderData( void *pData, ModelDataCategory_t nCategory )
{
	switch ( nCategory )
	{
	case MODEL_DATA_LIGHTING_MODEL:
		*(RenderableLightingModel_t*)pData = LIGHTING_MODEL_STATIC_PROP;
		return true;

#if defined( _X360 )
	// Deferred shadow rendering:
	case MODEL_DATA_STENCIL:
		{
			if ( r_shadow_deferred.GetBool() )
			{
				// clear stencil and hi-stencil because static props don't cast shadows
				ShaderStencilState_t* pStencilState = reinterpret_cast<ShaderStencilState_t*>( pData );

				uint32 mask = 1 << 2;
				uint32 nRef = 0;

				pStencilState->m_bEnable = true;
				pStencilState->m_nTestMask = 0xFFFFFFFF;
				pStencilState->m_nWriteMask = mask;
				pStencilState->m_nReferenceValue = nRef;
				pStencilState->m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
				pStencilState->m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
				pStencilState->m_FailOp = SHADER_STENCILOP_KEEP;
				pStencilState->m_ZFailOp = SHADER_STENCILOP_KEEP;//SHADER_STENCILOP_SET_TO_REFERENCE;


				pStencilState->m_bHiStencilEnable = false;
				pStencilState->m_bHiStencilWriteEnable = true;
				pStencilState->m_HiStencilCompareFunc = SHADER_HI_STENCILFUNC_NOTEQUAL;
				pStencilState->m_nHiStencilReferenceValue = 0;
				return true;
			}
		}
		return false;
#endif

	default:
		return false;
	}
}



//-----------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------
inline int CStaticProp::LeafCount() const 
{ 
	return m_LeafCount; 
}

inline int CStaticProp::FirstLeaf() const 
{ 
	return m_FirstLeaf; 
}

inline ModelInstanceHandle_t CStaticProp::GetModelInstance()
{
	return m_ModelInstance;
}

inline void CStaticProp::SetModelInstance( ModelInstanceHandle_t handle )
{
	m_ModelInstance = handle;
}

inline void CStaticProp::SetRenderHandle( ClientRenderHandle_t handle )
{
	m_RenderHandle = handle;
}

inline ClientRenderHandle_t CStaticProp::GetRenderHandle() const
{
	return m_RenderHandle;
}

void CStaticProp::CleanUpRenderHandle( )
{
	if ( m_RenderHandle != INVALID_CLIENT_RENDER_HANDLE )
	{
#ifndef DEDICATED
		clientleafsystem->RemoveRenderable( m_RenderHandle );
#endif
		m_RenderHandle = INVALID_CLIENT_RENDER_HANDLE;
	}
}

void CStaticProp::CleanUpAlphaProperty()
{
	if ( m_pClientAlphaProperty )
	{
#ifndef DEDICATED
		g_pClientAlphaPropertyMgr->DestroyClientAlphaProperty( m_pClientAlphaProperty );
#endif
		m_pClientAlphaProperty = NULL;
	}
}


//-----------------------------------------------------------------------------
// Determine alpha and blend amount for transparent objects based on render state info
//-----------------------------------------------------------------------------
void CStaticProp::GetColorModulation( float* color )
{
	memcpy( color, m_DiffuseModulation.Base(), sizeof( float ) * 3 );
}


//-----------------------------------------------------------------------------
// custom collision test
//-----------------------------------------------------------------------------
bool CStaticProp::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	Assert(0);
	return false;
}


//-----------------------------------------------------------------------------
// Perform hitbox test, returns true *if hitboxes were tested at all*!!
//-----------------------------------------------------------------------------
bool CStaticProp::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	return false;
}


//-----------------------------------------------------------------------------
// Returns the BRUSH model index if this is a brush model. Otherwise, returns -1.
//-----------------------------------------------------------------------------
int	CStaticProp::GetCollisionModelIndex()
{
	return -1;
}


//-----------------------------------------------------------------------------
// Return the model, if it's a studio model.
//-----------------------------------------------------------------------------
const model_t* CStaticProp::GetCollisionModel()
{
	return m_pModel;
}


//-----------------------------------------------------------------------------
// Get angles and origin.
//-----------------------------------------------------------------------------
const Vector& CStaticProp::GetCollisionOrigin() const
{
	return m_Origin;
}

const QAngle& CStaticProp::GetCollisionAngles() const
{
	if ( GetSolid() == SOLID_VPHYSICS )
	{
		return m_Angles;
	}

	return vec3_angle;
}

const matrix3x4_t& CStaticProp::CollisionToWorldTransform() const
{
	return m_ModelToWorld;
}


//-----------------------------------------------------------------------------
// Return a SOLID_ define.
//-----------------------------------------------------------------------------
SolidType_t CStaticProp::GetSolid() const
{
	return (SolidType_t)m_nSolidType;
}

int	CStaticProp::GetSolidFlags() const
{
	return 0;
}

int CStaticProp::GetRenderFlags( void )
{
	int nRet = 0;
	if ( m_pModel )
	{
		if ( m_pModel->flags & MODELFLAG_STUDIOHDR_USES_FB_TEXTURE )
		{
			nRet |= ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB;
		}
	}
	return nRet;
}


ClientRenderHandle_t& CStaticProp::RenderHandle()
{
	return m_RenderHandle;
}

IPVSNotify* CStaticProp::GetPVSNotifyInterface()
{
	return NULL;
}


void CStaticProp::GetRenderBounds( Vector& mins, Vector& maxs )
{
	mins = m_RenderBBoxMin;
	maxs = m_RenderBBoxMax;
}

void CStaticProp::GetRenderBoundsWorldspace( Vector& mins, Vector& maxs )
{
	mins = m_WorldRenderBBoxMin;
	maxs = m_WorldRenderBBoxMax;
}

bool CStaticProp::ShouldReceiveProjectedTextures( int flags )
{ 
	if ( m_Flags & STATIC_PROP_NO_FLASHLIGHT )
		return false;

	if( flags & ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SIMPLE_PROJECTION ) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CStaticProp::ShouldCacheRenderInfo()
{
	return true;
}


void CStaticProp::PrecacheLighting()
{
#ifndef DEDICATED
	if ( m_ModelInstance == MODEL_INSTANCE_INVALID )
	{
		LightCacheHandle_t lightCacheHandle = CreateStaticLightingCache( m_LightingOrigin, m_WorldRenderBBoxMin, m_WorldRenderBBoxMax );
		m_ModelInstance = modelrender->CreateInstance( this, &lightCacheHandle );
	}
#endif
}


void CStaticProp::RecomputeStaticLighting( void )
{
#ifndef DEDICATED
	modelrender->RecomputeStaticLighting( m_ModelInstance );
#endif
}


//-----------------------------------------------------------------------------
// Diagnostic information for static props
//-----------------------------------------------------------------------------
void CStaticProp::DisplayStaticPropInfo( int nInfoType )
{
#ifndef DEDICATED
	char buf[512];
	switch( nInfoType )
	{
	case 1:
		Q_snprintf( buf, sizeof( buf ), "%s", modelloader->GetName( m_pModel ) );
		break;

	case 2:
		Q_snprintf(buf, sizeof( buf ), "%d", (m_EntHandle.ToInt() & (~STATICPROP_EHANDLE_MASK)) );
		break;

	case 3:
		{
			float flDist = GetRenderOrigin().DistTo( s_StaticPropMgr.ViewOrigin() );
			Q_snprintf(buf, sizeof( buf ), "%.1f", flDist );
		}
		break;

	case 4:
		{
			CMatRenderContextPtr pRenderContext( materials );
			float flPixelWidth = pRenderContext->ComputePixelWidthOfSphere( GetRenderOrigin(), Radius() );
			Q_snprintf(buf, sizeof( buf ), "%.1f", flPixelWidth );
		}
		break;
	}

	Vector vecTextBox = ( m_WorldRenderBBoxMax + m_WorldRenderBBoxMin ) * 0.5f;
	vecTextBox.z = m_WorldRenderBBoxMax.z + 10;
	CDebugOverlay::AddTextOverlay( vecTextBox, 0.0f, buf );
#endif
}


//-----------------------------------------------------------------------------
// Draws the model
//-----------------------------------------------------------------------------
int	CStaticProp::DrawModelSlow( int flags, const RenderableInstance_t &instance )
{
#ifndef DEDICATED
	VPROF_BUDGET( "CStaticProp::DrawModel", VPROF_BUDGETGROUP_STATICPROP_RENDERING );

	if ( !r_drawstaticprops.GetBool() )
		return 0;

	if ( r_drawstaticprops.GetInt() == 2 || r_slowpathwireframe.GetInt() )
	{
		flags |= STUDIO_WIREFRAME;
	}

#ifdef _DEBUG
	if ( r_DrawSpecificStaticProp.GetInt() >= 0 )
	{
		if ( (m_EntHandle.ToInt() & (~STATICPROP_EHANDLE_MASK) ) != r_DrawSpecificStaticProp.GetInt() )
			return 0;
	}
#endif

	if ( ( instance.m_nAlpha == 0 ) || !m_pModel )
		return 0;

#ifdef _DEBUG
	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( m_pModel );
	Assert( pStudioHdr );
	if ( !( pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) )
	{
		return 0;
	}
#endif

	if ( r_colorstaticprops.GetBool() )
	{
		// deterministic random sequence
		unsigned short hash[3];
		hash[0] = HashItem( m_ModelInstance );
		hash[1] = HashItem( hash[0] );
		hash[2] = HashItem( hash[1] );
		r_colormod[0] = (float)hash[0] * 1.0f/65535.0f;
		r_colormod[1] = (float)hash[1] * 1.0f/65535.0f;
		r_colormod[2] = (float)hash[2] * 1.0f/65535.0f;
		VectorNormalize( r_colormod );
	}

	flags |= STUDIO_STATIC_LIGHTING;

	int nInfoType = r_staticpropinfo.GetInt();
	if ( nInfoType )
	{
		DisplayStaticPropInfo( nInfoType );
	}

	//	CDebugOverlay::AddBoxOverlay( vec3_origin, m_WorldRenderBBoxMin, m_WorldRenderBBoxMax, vec3_angle, 255, 0, 0, 32, 0.01 );
	//	CDebugOverlay::AddBoxOverlay( GetRenderOrigin(), m_RenderBBoxMin, m_RenderBBoxMax, GetRenderAngles(), 0, 255, 0, 32, 0.01 );

	ModelRenderInfo_t sInfo;
	InitModelRenderInfo( sInfo, flags );
	g_pStudioRender->SetColorModulation( r_colormod );
	g_pStudioRender->SetAlphaModulation( instance.m_nAlpha / 255.0f );
	// Restore the matrices if we're skinning
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	int drawn = modelrender->DrawModelEx( sInfo );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	if ( m_pModel && (flags & STUDIO_WIREFRAME_VCOLLIDE) )
	{
		if ( m_nSolidType == SOLID_VPHYSICS )
		{
			// This works because VCollideForModel only uses modelindex for mod_brush
			// and props are always mod_Studio.
			vcollide_t * pCollide = CM_VCollideForModel( -1, m_pModel ); 
			if ( pCollide && pCollide->solidCount == 1 )
			{
				static color32 debugColor = {0,255,255,0};
				DebugDrawPhysCollide( pCollide->solids[0], NULL, m_ModelToWorld, debugColor, false );
			}
		}
		else if ( m_nSolidType == SOLID_BBOX )
		{
			static Color debugColor( 0, 255, 255, 255 );
			RenderWireframeBox( m_Origin, vec3_angle, m_pModel->mins, m_pModel->maxs, debugColor, true );
		}
	}

	return drawn;
#else
	return 0;
#endif
}

int CStaticProp::DrawModel( int flags, const RenderableInstance_t &instance )
{
#ifndef DEDICATED
	VPROF_BUDGET( "CStaticProp::DrawModel", VPROF_BUDGETGROUP_STATICPROP_RENDERING );

	if ( ( instance.m_nAlpha == 0 ) || !m_pModel )
		return 0;

	if ( IsUsingStaticPropDebugModes() || (flags & (STUDIO_WIREFRAME_VCOLLIDE|STUDIO_WIREFRAME)) )
		return DrawModelSlow( flags, instance );

	flags |= STUDIO_STATIC_LIGHTING;

	ModelRenderInfo_t sInfo;
	InitModelRenderInfo( sInfo, flags );
	g_pStudioRender->SetColorModulation( r_colormod );
	g_pStudioRender->SetAlphaModulation( instance.m_nAlpha / 255.0f );
	// Restore the matrices if we're skinning
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

#ifdef _X360
	ShaderStencilState_t stencilState;
	bool bDeferredShadows = r_shadow_deferred.GetBool();
	if ( bDeferredShadows )
	{
		// Deferred shadow rendering:
		// clear stencil and hi-stencil because static props don't cast shadows
		uint32 mask = 1 << 2;
		uint32 nRef = 0;

		stencilState.m_bEnable = true;
		stencilState.m_nTestMask = 0xFFFFFFFF;
		stencilState.m_nWriteMask = mask;
		stencilState.m_nReferenceValue = nRef;
		stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
		stencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
		stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
		stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;

		stencilState.m_bHiStencilEnable = false;
		stencilState.m_bHiStencilWriteEnable = true;
		stencilState.m_HiStencilCompareFunc = SHADER_HI_STENCILFUNC_NOTEQUAL;
		stencilState.m_nHiStencilReferenceValue = 0;
		pRenderContext->SetStencilState( stencilState );
	}
#endif

	int drawn = modelrender->DrawModelExStaticProp( pRenderContext, sInfo );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

#ifdef _X360
	if ( bDeferredShadows )
	{
		ShaderStencilState_t stencilState;
		stencilState.m_bEnable = false;
		stencilState.m_bHiStencilWriteEnable = false;
		pRenderContext->SetStencilState( stencilState );
	}
#endif

	return drawn;
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
// KD Tree
//-----------------------------------------------------------------------------
void CStaticProp::InsertPropIntoKDTree()
{
	Assert( m_Partition == PARTITION_INVALID_HANDLE );
	if ( m_nSolidType == SOLID_NONE )
		return;

	// Compute the bbox of the prop
	Vector mins, maxs;
	matrix3x4_t propToWorld;
	AngleMatrix( m_Angles, m_Origin, propToWorld );
	TransformAABB( propToWorld, m_pModel->mins, m_pModel->maxs, mins, maxs ); 

	// If it's using vphysics, get a good AABB
	if ( m_nSolidType == SOLID_VPHYSICS )
	{
		vcollide_t *pCollide = CM_VCollideForModel( -1, m_pModel );
		if ( pCollide && pCollide->solidCount )
		{
			physcollision->CollideGetAABB( &mins, &maxs, pCollide->solids[0], m_Origin, m_Angles );
		}
		else
		{
			m_nSolidType = SOLID_NONE;
			return;
		}
	}

	// add the entity to the KD tree so we will collide against it
	m_Partition = SpatialPartition()->CreateHandle( this, 
		PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_STATIC_PROPS | 
		PARTITION_ENGINE_SOLID_EDICTS | PARTITION_ENGINE_STATIC_PROPS, 
		mins, maxs );

	Assert( m_Partition != PARTITION_INVALID_HANDLE );
}

void CStaticProp::RemovePropFromKDTree()
{
	// Release the spatial partition handle
	if ( m_Partition != PARTITION_INVALID_HANDLE )
	{
		SpatialPartition()->DestroyHandle( m_Partition );
		m_Partition = PARTITION_INVALID_HANDLE;
	}
}


//-----------------------------------------------------------------------------
// Create VPhysics representation
//-----------------------------------------------------------------------------
void CStaticProp::CreateVPhysics( IPhysicsEnvironment *pPhysEnv, IVPhysicsKeyHandler *pDefaults, void *pGameData )
{
	if ( m_nSolidType == SOLID_NONE )
		return;

	vcollide_t *pVCollide = NULL;
	solid_t solid;
	CPhysCollide* pPhysCollide = NULL;
	unsigned int iContents = MASK_ALL;

	if ( m_pModel )
	{		
		if ( m_nSolidType == SOLID_VPHYSICS )
		{
			// This works because VCollideForModel only uses modelindex for mod_brush
			// and props are always mod_Studio.
			pVCollide = CM_VCollideForModel( -1, m_pModel );
		}

		int nModelType = m_pModel->type;
		if ( nModelType == mod_studio )
		{
			MDLHandle_t hStudioHdr = m_pModel->studio;
			if ( hStudioHdr != MDLHANDLE_INVALID )
			{
				const studiohdr_t *pStudioHdr = mdlcache->GetStudioHdr( hStudioHdr );
				iContents = pStudioHdr->contents;
			}
		}
		else if ( nModelType == mod_brush )
		{
			iContents = m_pModel->brush.pShared->nodes[ m_pModel->brush.firstnode ].contents;
		}
	}

	if (pVCollide)
	{
		pPhysCollide = pVCollide->solids[0];

		IVPhysicsKeyParser *pParse = physcollision->VPhysicsKeyParserCreate( pVCollide );
		while ( !pParse->Finished() )
		{
			const char *pBlock = pParse->GetCurrentBlockName();
			if ( !strcmpi( pBlock, "solid" ) )
			{
				pParse->ParseSolid( &solid, pDefaults );
				break;
			}
			else
			{
				pParse->SkipBlock();
			}
		}
		physcollision->VPhysicsKeyParserDestroy( pParse );
	}
	else
	{
		if ( m_nSolidType != SOLID_BBOX  )
		{
			char szModel[MAX_PATH];
			Q_strncpy( szModel, m_pModel ? modelloader->GetName( m_pModel ) : "unknown model", sizeof( szModel ) );
			Log_Warning( LOG_StaticPropManager, "Map Error:  Static prop with bogus solid type %d! (%s)\n", m_nSolidType, szModel );
			m_nSolidType = SOLID_NONE;
			return;
		}

		// If there's no collide, we need a bbox...
		pPhysCollide = physcollision->BBoxToCollide( m_pModel->mins, m_pModel->maxs );
		solid.params = g_PhysDefaultObjectParams;
        Q_strncpy( solid.surfaceprop, "default", sizeof( solid.surfaceprop ) );
	}

	Assert(pPhysCollide);
	solid.params.enableCollisions = true;
	solid.params.pGameData = pGameData;
	solid.params.pName = "prop_static";

	int surfaceData = physprops->GetSurfaceIndex( solid.surfaceprop );
	IPhysicsObject *pPhysObject = pPhysEnv->CreatePolyObjectStatic( pPhysCollide, 
		surfaceData, m_Origin, m_Angles, &solid.params );

	pPhysObject->SetContents( iContents );
	if ( iContents & CONTENTS_SOLID )
	{
		pPhysObject->SetCollisionHints(COLLISION_HINT_STATICSOLID);
	}
	//PhysCheckAdd( pPhys, "Static" );
}


//-----------------------------------------------------------------------------
// Expose IStaticPropMgr to the engine
//-----------------------------------------------------------------------------
IStaticPropMgrEngine* StaticPropMgr()
{
	return &s_StaticPropMgr;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CStaticPropMgr::CStaticPropMgr()
{
	m_bLevelInitialized = false;
	m_bClientInitialized = false;
	m_nLastCPULevel = m_nLastGPULevel = -1;
}

CStaticPropMgr::~CStaticPropMgr()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CStaticPropMgr::Init()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CStaticPropMgr::Shutdown()
{
	if ( !m_bLevelInitialized )
		return;

	LevelShutdown();
}


//-----------------------------------------------------------------------------
// Configures the system level for static props
//-----------------------------------------------------------------------------
void CStaticPropMgr::ConfigureSystemLevel( int nCPULevel, int nGPULevel )
{
	if ( nCPULevel == m_nLastCPULevel && nGPULevel == m_nLastGPULevel )
		return;

	m_nLastCPULevel = nCPULevel;
	m_nLastGPULevel = nGPULevel;
	if ( m_bClientInitialized )
	{
		UpdatePropVisibility( nCPULevel, nGPULevel );
	}
}


void CStaticPropMgr::RestoreStaticProps( )
{
	UpdatePropVisibility( m_nLastCPULevel, m_nLastGPULevel );

	// It's possible in certain alt+tab situations that the mdl flags will not be set the first time we compute static lighting.
	// Ensure that we recompute it here to catch any mdl flag changes after the alt+tab.
	RecomputeStaticLighting();
}

void CStaticPropMgr::UpdatePropVisibility( int nCPULevel, int nGPULevel )
{
#ifdef DEDICATED
	return;
#endif

	if ( !m_bClientInitialized )
		return;

	int nCount = m_StaticProps.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CStaticProp &prop = m_StaticProps[i];

		bool bNoDraw = false;
		if ( !IsGameConsole() )
		{
			if ( nCPULevel >= 0 )
			{
				bNoDraw =            ( prop.m_nMinCPULevel && prop.m_nMinCPULevel-1 > nCPULevel );
				bNoDraw = bNoDraw || ( prop.m_nMaxCPULevel && prop.m_nMaxCPULevel-1 < nCPULevel );
			}
			if ( nGPULevel >= 0 )
			{
				bNoDraw = bNoDraw || ( prop.m_nMinGPULevel && prop.m_nMinGPULevel-1 > nGPULevel );
				bNoDraw = bNoDraw || ( prop.m_nMaxGPULevel && prop.m_nMaxGPULevel-1 < nGPULevel );
			}
		}

		// Add the prop to all the leaves it lies in
		if ( bNoDraw || !prop.LeafCount())
		{
			prop.CleanUpRenderHandle();
			continue;
		}

		RenderableTranslucencyType_t nType = modelinfo->ComputeTranslucencyType( prop.m_pModel, prop.GetSkin(), prop.GetBody() );
		if ( prop.RenderHandle() != INVALID_CLIENT_RENDER_HANDLE )
		{
			clientleafsystem->SetTranslucencyType( prop.RenderHandle(), nType );
			clientleafsystem->RenderInFastReflections( prop.RenderHandle(), ( prop.m_Flags & STATIC_PROP_MARKED_FOR_FAST_REFLECTION ) != 0 );
			clientleafsystem->DisableShadowDepthRendering( prop.RenderHandle(), ( prop.m_FlagsEx & STATIC_PROP_FLAGS_EX_DISABLE_SHADOW_DEPTH ) != 0 );
			clientleafsystem->DisableCSMRendering( prop.RenderHandle(), (prop.m_FlagsEx & STATIC_PROP_FLAGS_EX_DISABLE_CSM) != 0 );
			continue;
		}

		clientleafsystem->CreateRenderableHandle( &prop, false, nType, RENDERABLE_MODEL_STATIC_PROP );
		ClientRenderHandle_t handle = prop.RenderHandle();
		clientleafsystem->AddRenderableToLeaves( handle, prop.LeafCount(), (unsigned short*)&m_StaticPropLeaves[prop.FirstLeaf()] ); 
		clientleafsystem->RenderInFastReflections( prop.RenderHandle(), ( prop.m_Flags & STATIC_PROP_MARKED_FOR_FAST_REFLECTION ) != 0 );
		clientleafsystem->DisableShadowDepthRendering( prop.RenderHandle(), ( prop.m_FlagsEx & STATIC_PROP_FLAGS_EX_DISABLE_SHADOW_DEPTH ) != 0 );
		clientleafsystem->DisableCSMRendering( prop.RenderHandle(), (prop.m_FlagsEx & STATIC_PROP_FLAGS_EX_DISABLE_CSM) != 0 );
	}
}


//-----------------------------------------------------------------------------
// Unserialize static prop model dictionary
//-----------------------------------------------------------------------------
void CStaticPropMgr::UnserializeModelDict( CUtlBuffer& buf )
{
	int count = buf.GetInt();
	m_StaticPropDict.AddMultipleToTail( count );

	COM_TimestampedLog( "Starting UnserializeModelDict for %d models\n", count );
	for ( int i=0; i < count; i++ )
	{
#ifndef DEDICATED
		if ( !(i % 10 ) )
		{
			EngineVGui()->UpdateProgressBar(PROGRESS_DEFAULT);
		}
#endif

		StaticPropDictLump_t lump;
		buf.Get( &lump, sizeof(StaticPropDictLump_t) );

		m_StaticPropDict[i] = modelloader->GetModelForName( lump.m_Name, IModelLoader::FMODELLOADER_STATICPROP );
	}
	COM_TimestampedLog( "Finished UnserializeModelDict\n" );
}

void CStaticPropMgr::UnserializeLeafList( CUtlBuffer& buf )
{
	int nCount = buf.GetInt();
	m_StaticPropLeaves.Purge();
	if ( nCount > 0 )
	{
		m_StaticPropLeaves.AddMultipleToTail( nCount );
		buf.Get( m_StaticPropLeaves.Base(), nCount * sizeof(StaticPropLeafLump_t) );
	}
}

void CStaticPropMgr::UnserializeModels( CUtlBuffer& buf )
{
	// Version check
	int nLumpVersion = Mod_GameLumpVersion( GAMELUMP_STATIC_PROPS );
	if ( nLumpVersion < 4 )
	{
		Log_Warning( LOG_StaticPropManager, "Really old map format! Static props can't be loaded...\n" );
		return;
	}

	int count = buf.GetInt();

	// Gotta preallocate the static props here so no rellocations take place
	// the leaf list stores pointers to these tricky little guys.
	bool bSkip = false;
	m_StaticProps.EnsureCapacity(count);
	for ( int i = 0; i < count; ++i )
	{
		// Reset every loop.
		bSkip = false;

		StaticPropLump_t lump;
		switch ( nLumpVersion )
		{
		case 4:
			buf.Get( &lump, sizeof(StaticPropLumpV4_t) );
			lump.m_flForcedFadeScale = 1.0f;
			lump.m_nMinCPULevel = lump.m_nMaxCPULevel = lump.m_nMinGPULevel = lump.m_nMaxGPULevel = 0;
			lump.m_DiffuseModulation.r = lump.m_DiffuseModulation.g = lump.m_DiffuseModulation.b = lump.m_DiffuseModulation.a = 255; // default color/alpha modulation to identity
			lump.m_bDisableX360 = false;
			lump.m_FlagsEx = 0;
			break;

		case 5:
			buf.Get( &lump, sizeof(StaticPropLumpV5_t) );
			lump.m_nMinCPULevel = lump.m_nMaxCPULevel = lump.m_nMinGPULevel = lump.m_nMaxGPULevel = 0;
			lump.m_DiffuseModulation.r = lump.m_DiffuseModulation.g = lump.m_DiffuseModulation.b = lump.m_DiffuseModulation.a = 255; // default color/alpha modulation to identity
			lump.m_bDisableX360 = false;
			lump.m_FlagsEx = 0;
			break;

		case 6:
			buf.Get( &lump, sizeof( StaticPropLumpV6_t ) );
			lump.m_nMinCPULevel = lump.m_nMaxCPULevel = lump.m_nMinGPULevel = lump.m_nMaxGPULevel = 0;
			lump.m_DiffuseModulation.r = lump.m_DiffuseModulation.g = lump.m_DiffuseModulation.b = lump.m_DiffuseModulation.a = 255; // default color/alpha modulation to identity
			lump.m_bDisableX360 = false;
			lump.m_FlagsEx = 0;
			break;

		case 7:
			buf.Get( &lump, sizeof( StaticPropLumpV7_t ) );
			lump.m_nMinCPULevel = lump.m_nMaxCPULevel = lump.m_nMinGPULevel = lump.m_nMaxGPULevel = 0;
			lump.m_bDisableX360 = false;
			lump.m_FlagsEx = 0;
			break;

		case 8:
			buf.Get( &lump, sizeof( StaticPropLumpV8_t ) );
			lump.m_bDisableX360 = false;
			lump.m_FlagsEx = 0;
			break;

		case 9:
			buf.Get( &lump, sizeof( StaticPropLumpV9_t ) );
			lump.m_FlagsEx = 0;
			break;
		
		case 10:
			buf.Get( &lump, sizeof( StaticPropLump_t ) );
			break;
		}

		int j = m_StaticProps.AddToTail();
		m_StaticProps[j].Init( j, lump, m_StaticPropDict[lump.m_PropType] );

		// Add the prop to the K-D tree for collision
		m_StaticProps[j].InsertPropIntoKDTree( );

#ifndef DEDICATED
		if ( !(i % 10 ) )
		{
			EngineVGui()->UpdateProgressBar(PROGRESS_DEFAULT);
		}
#endif
	}
}

void CStaticPropMgr::OutputLevelStats( void )
{
	// STATS
	int i;
	int totalVerts = 0;
	for( i = 0; i < m_StaticProps.Count(); i++ )
	{
		CStaticProp *pStaticProp = &m_StaticProps[i];
		model_t *pModel = (model_t*)pStaticProp->GetModel();
		if( !pModel )
		{
			continue;
		}
		Assert( pModel->type == mod_studio );
		studiohdr_t *pStudioHdr = ( studiohdr_t * )modelloader->GetExtraData( pModel );
		int bodyPart;
		for( bodyPart = 0; bodyPart < pStudioHdr->numbodyparts; bodyPart++ )
		{
			mstudiobodyparts_t *pBodyPart = pStudioHdr->pBodypart( bodyPart );
			int model;
			for( model = 0; model < pBodyPart->nummodels; model++ )
			{
				mstudiomodel_t *pModel = pBodyPart->pModel( model );
				totalVerts += pModel->numvertices;
			}
		}
	}
	Log_Warning( LOG_StaticPropManager, "%d static prop instances in map\n", ( int )m_StaticProps.Count() );
	Log_Warning( LOG_StaticPropManager, "%d static prop models in map\n", ( int )m_StaticPropDict.Count() );
	Log_Warning( LOG_StaticPropManager, "%d static prop verts in map\n", ( int )totalVerts );
}


//-----------------------------------------------------------------------------
// Unserialize static props
//-----------------------------------------------------------------------------
void CStaticPropMgr::UnserializeStaticProps()
{
	// Unserialize static props, insert them into the appropriate leaves
	int size = Mod_GameLumpSize( GAMELUMP_STATIC_PROPS );
	if (!size)
		return;

	COM_TimestampedLog( "UnserializeStaticProps - start");

	MEM_ALLOC_CREDIT();
	CUtlBuffer buf( 0, size );
	if ( Mod_LoadGameLump( GAMELUMP_STATIC_PROPS, buf.PeekPut(), size ))
	{
		buf.SeekPut( CUtlBuffer::SEEK_HEAD, size );
		COM_TimestampedLog( "UnserializeModelDict" );
		UnserializeModelDict( buf );
		COM_TimestampedLog( "UnserializeLeafList" );
		UnserializeLeafList( buf );
		COM_TimestampedLog( "UnserializeModels" );
		UnserializeModels( buf );
	}

	COM_TimestampedLog( "UnserializeStaticProps - end");
}


//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------
void CStaticPropMgr::LevelInit()
{
	if ( m_bLevelInitialized )
		return;

	Assert( !m_bClientInitialized );
	m_bLevelInitialized = true;

	// See if we should skip loading static props
	if( disableStaticPropLoading.GetBool() == false )
	{
		// Read in static props that have been compiled into the bsp file
		UnserializeStaticProps();
	}

	//	OutputLevelStats();
}

void CStaticPropMgr::LevelShutdown()
{
	if ( !m_bLevelInitialized )
		return;

	// Deal with client-side stuff, if appropriate
	if ( m_bClientInitialized )
	{
		LevelShutdownClient();
	}

	m_bLevelInitialized = false;

	m_StaticProps.Purge();

	FOR_EACH_VEC( m_StaticPropDict, i )
	{
		modelloader->UnreferenceModel( m_StaticPropDict[i], IModelLoader::FMODELLOADER_STATICPROP );
	}
	m_StaticPropDict.Purge();
}

void CStaticPropMgr::LevelInitClient()
{
#ifndef DEDICATED
	if ( sv.IsDedicated() ) 
		return;

	extern ConVar r_proplightingfromdisk;

	bool bNeedsMapAccess = r_proplightingfromdisk.GetBool();
	if ( bNeedsMapAccess )
	{
		g_pFileSystem->BeginMapAccess();
	}

	Assert( m_bLevelInitialized );
	Assert( !m_bClientInitialized );

	// Since the client will be ready at a later time than the server
	// to set up its data, we need a separate call to handle that
	int nCount = m_StaticProps.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CStaticProp &prop = m_StaticProps[i];

		if ( prop.LeafCount() <= 0 )
		{
			Vector origin = prop.GetCollisionOrigin();
			Vector mins = prop.OBBMins();
			Vector maxs = prop.OBBMaxs();
			DevMsg( 1, "Static prop in 0 leaves! %s, @ %.1f, %.1f, %.1f\n", modelloader->GetName( prop.GetModel() ), origin.x, origin.y, origin.z );
			continue;
		}
	}

	m_bClientInitialized = true;

	// swapping order of UpdatePropVisibility and PrecacheLighting - Precache now first since it updates the flag to determine if prop contributes to CSM
	PrecacheLighting();
	UpdatePropVisibility( m_nLastCPULevel, m_nLastGPULevel );

	if ( bNeedsMapAccess )
	{
		g_pFileSystem->EndMapAccess();
	}

#endif
}

void CStaticPropMgr::LevelShutdownClient()
{
	if ( !m_bClientInitialized )
		return;

	Assert( m_bLevelInitialized );

	for (int i = m_StaticProps.Count(); --i >= 0; )
	{
		m_StaticProps[i].CleanUpAlphaProperty();
		m_StaticProps[i].CleanUpRenderHandle( );
		modelrender->SetStaticLighting( m_StaticProps[i].GetModelInstance(), NULL );
	}

#ifndef DEDICATED
	// Make sure static prop lightcache is reset
	ClearStaticLightingCache();
#endif

	m_bClientInitialized = false;
}

//-----------------------------------------------------------------------------
// Create physics representations of props
//-----------------------------------------------------------------------------
void CStaticPropMgr::CreateVPhysicsRepresentations( IPhysicsEnvironment	*pPhysEnv, IVPhysicsKeyHandler *pDefaults, void *pGameData )
{
	// Walk through the static props + make collideable thingies for them.
	int nCount = m_StaticProps.Count();
	for ( int i = nCount; --i >= 0; )
	{
		m_StaticProps[i].CreateVPhysics( pPhysEnv, pDefaults, pGameData );
	}
}


//-----------------------------------------------------------------------------
// Handles to props
//-----------------------------------------------------------------------------
inline int CStaticPropMgr::HandleEntityToIndex( IHandleEntity *pHandleEntity ) const
{
	Assert( IsStaticProp_Inline( pHandleEntity ) );
	return pHandleEntity->GetRefEHandle().GetEntryIndex();
}

ICollideable *CStaticPropMgr::GetStaticProp( IHandleEntity *pHandleEntity )
{
	if ( !IsStaticProp_Inline( pHandleEntity ) )
	{
		return NULL;
	}

	int nIndex = pHandleEntity ? pHandleEntity->GetRefEHandle().GetEntryIndex() : -1;
	if ( nIndex < 0 || nIndex > m_StaticProps.Count() )
	{
		return NULL;
	}
	return &m_StaticProps[nIndex];
}


ICollideable *CStaticPropMgr::GetStaticPropByIndex( int propIndex )
{
	if ( propIndex < m_StaticProps.Count() )
	{
		return &m_StaticProps[propIndex];
	}
	Assert(0);
	return NULL;
}



//-----------------------------------------------------------------------------
// Get large amounts of handles to static props
//-----------------------------------------------------------------------------
void CStaticPropMgr::GetAllStaticProps( CUtlVector<ICollideable *> *pOutput )
{
	if ( pOutput == NULL ) return;
	int iPropVectorSize = m_StaticProps.Count();

	int counter;
	for ( counter = 0; counter != iPropVectorSize; ++counter )
	{
		pOutput->AddToTail( &m_StaticProps[counter] );
	}
}

void CStaticPropMgr::GetAllStaticPropsInAABB( const Vector &vMins, const Vector &vMaxs, CUtlVector<ICollideable *> *pOutput )
{
	if ( pOutput == NULL ) return;
	int iPropVectorSize = m_StaticProps.Count();

	int counter;
	for ( counter = 0; counter != iPropVectorSize; ++counter )
	{
		CStaticProp *pProp = &m_StaticProps[counter];

		Vector vPropMins, vPropMaxs;
		pProp->WorldSpaceSurroundingBounds( &vPropMins, &vPropMaxs );

		if( vPropMaxs.x < vMins.x ) continue;
		if( vPropMaxs.y < vMins.y ) continue;
		if( vPropMaxs.z < vMins.z ) continue;

		if( vPropMins.x > vMaxs.x ) continue;
		if( vPropMins.y > vMaxs.y ) continue;
		if( vPropMins.z > vMaxs.z ) continue;

		pOutput->AddToTail( pProp );
	}
}

void CStaticPropMgr::GetAllStaticPropsInOBB( const Vector &ptOrigin, const Vector &vExtent1, const Vector &vExtent2, const Vector &vExtent3, CUtlVector<ICollideable *> *pOutput )
{
	if ( pOutput == NULL ) 
		return;
	int counter;

	Vector vAABBMins, vAABBMaxs;
	vAABBMins = ptOrigin;
	vAABBMaxs = ptOrigin;
	Vector ptAABBExtents[8];

	Vector ptOBBExtents[8];
	for( counter = 0; counter != 8; ++counter )
	{
		ptOBBExtents[counter] = ptOrigin;
		if( counter & (1<<0) ) ptOBBExtents[counter] += vExtent1;
		if( counter & (1<<1) ) ptOBBExtents[counter] += vExtent2;
		if( counter & (1<<2) ) ptOBBExtents[counter] += vExtent3;

		//expand AABB extents
		if( ptOBBExtents[counter].x < vAABBMins.x ) vAABBMins.x = ptOBBExtents[counter].x;
		if( ptOBBExtents[counter].x > vAABBMaxs.x ) vAABBMaxs.x = ptOBBExtents[counter].x;
		if( ptOBBExtents[counter].y < vAABBMins.y ) vAABBMins.y = ptOBBExtents[counter].y;
		if( ptOBBExtents[counter].y > vAABBMaxs.y ) vAABBMaxs.y = ptOBBExtents[counter].y;
		if( ptOBBExtents[counter].z < vAABBMins.z ) vAABBMins.z = ptOBBExtents[counter].z;
		if( ptOBBExtents[counter].z > vAABBMaxs.z ) vAABBMaxs.z = ptOBBExtents[counter].z;
	}

	//generate planes for the obb so we can use halfspace elimination
	Vector vOBBPlaneNormals[6];
	float fOBBPlaneDists[6];

	vOBBPlaneNormals[0] = vExtent1;
	vOBBPlaneNormals[0].NormalizeInPlace();
	fOBBPlaneDists[0] = vOBBPlaneNormals[0].Dot( ptOrigin + vExtent1 );
	vOBBPlaneNormals[1] = -vOBBPlaneNormals[0];
	fOBBPlaneDists[1] = vOBBPlaneNormals[1].Dot( ptOrigin );

	vOBBPlaneNormals[2] = vExtent2;
	vOBBPlaneNormals[2].NormalizeInPlace();
	fOBBPlaneDists[2] = vOBBPlaneNormals[2].Dot( ptOrigin + vExtent2 );
	vOBBPlaneNormals[3] = -vOBBPlaneNormals[2];
	fOBBPlaneDists[3] = vOBBPlaneNormals[3].Dot( ptOrigin );

	vOBBPlaneNormals[4] = vExtent3;
	vOBBPlaneNormals[4].NormalizeInPlace();
	fOBBPlaneDists[4] = vOBBPlaneNormals[4].Dot( ptOrigin + vExtent3 );
	vOBBPlaneNormals[5] = -vOBBPlaneNormals[4];
	fOBBPlaneDists[5] = vOBBPlaneNormals[5].Dot( ptOrigin );

	int iPropVectorSize = m_StaticProps.Count();

	for ( counter = 0; counter != iPropVectorSize; ++counter )
	{
		CStaticProp *pProp = &m_StaticProps[counter];

		Vector vPropMins, vPropMaxs;
		pProp->WorldSpaceSurroundingBounds( &vPropMins, &vPropMaxs );

		if( vPropMaxs.x < vAABBMins.x ) continue;
		if( vPropMaxs.y < vAABBMins.y ) continue;
		if( vPropMaxs.z < vAABBMins.z ) continue;

		if( vPropMins.x > vAABBMaxs.x ) continue;
		if( vPropMins.y > vAABBMaxs.y ) continue;
		if( vPropMins.z > vAABBMaxs.z ) continue;



		//static prop AABB and desired AABB intersect, do OBB tests

		Vector vPropOBBMins = pProp->OBBMins();
		Vector vPropOBBMaxs = pProp->OBBMaxs();

		Vector ptPropExtents[8];

		matrix3x4_t matPropWorld;
		AngleMatrix( pProp->GetCollisionAngles(), pProp->GetCollisionOrigin(), matPropWorld );

		int counter2, counter3;
		//generate prop extents, TODO: update these to handle props with OBB's since it should be nearly trivial
		for( counter2 = 0; counter2 != 8; ++counter2 )
		{
			/*ptPropExtents[counter2].x = (counter2 & (1<<0))?(vPropMaxs.x):(vPropMins.x);
			ptPropExtents[counter2].y = (counter2 & (1<<1))?(vPropMaxs.y):(vPropMins.y);
			ptPropExtents[counter2].z = (counter2 & (1<<2))?(vPropMaxs.z):(vPropMins.z);*/

			Vector ptTemp;
			ptTemp.x = (counter2 & (1<<0))?(vPropOBBMaxs.x):(vPropOBBMins.x);
			ptTemp.y = (counter2 & (1<<1))?(vPropOBBMaxs.y):(vPropOBBMins.y);
			ptTemp.z = (counter2 & (1<<2))?(vPropOBBMaxs.z):(vPropOBBMins.z);

			VectorTransform( ptTemp, matPropWorld, ptPropExtents[counter2] );
		}



		for( counter2 = 0; counter2 != 6; ++counter2 ) //loop over OBB planes
		{
			for( counter3 = 0; counter3 != 8; ++counter3 ) //loop over prop extents
			{
				if( (ptPropExtents[counter3].Dot( vOBBPlaneNormals[counter2] ) - fOBBPlaneDists[counter2]) < 0.0f )
				{
					//an extent of the prop is within the OBB halfspace, this halfspace does not eliminate our prop, move to the next halfspace
					break;
				}				
			}
			if( counter3 == 8 ) break; //if all 8 extents lie outside the halfspace, then the prop is not in the OBB
		}
		if( counter2 == 6 )
		{
			//if all 6 planes failed to eliminate the extents, the OBB and prop intersect

			//FIXME: Halfspace elimination will never remove props that do intersect, but leaves some false positives in some cases.

			pOutput->AddToTail( pProp );
		}
	}
}

//-----------------------------------------------------------------------------
// Are we a static prop?
//-----------------------------------------------------------------------------
bool CStaticPropMgr::IsStaticProp( IHandleEntity *pHandleEntity ) const
{
	return IsStaticProp_Inline( pHandleEntity );
}

bool CStaticPropMgr::IsStaticProp( CBaseHandle handle ) const
{
	return (handle.GetSerialNumber() == (STATICPROP_EHANDLE_MASK >> NUM_SERIAL_NUM_SHIFT_BITS));
}

int CStaticPropMgr::GetStaticPropIndex( IHandleEntity *pHandleEntity ) const
{
	return HandleEntityToIndex( pHandleEntity );
}


//-----------------------------------------------------------------------------
// Returns lighting origins
//-----------------------------------------------------------------------------
void CStaticPropMgr::GetLightingOrigins( Vector *pLightingOrigins, int nOriginStride, int nCount, IClientRenderable **ppRenderable, int nRenderableStride )
{											    
	for ( int i = 0; i < nCount; ++i, 
		ppRenderable = (IClientRenderable**)( (unsigned char*)ppRenderable + nRenderableStride ),
		pLightingOrigins = (Vector*)( (unsigned char*)pLightingOrigins + nOriginStride ))
	{
		CStaticProp *pProp = assert_cast< CStaticProp* >( (*ppRenderable)->GetIClientUnknown() );
		*pLightingOrigins = pProp->m_LightingOrigin;
	}
}


//-----------------------------------------------------------------------------
// Compute static lighting
//-----------------------------------------------------------------------------
void CStaticPropMgr::PrecacheLighting()
{
	COM_TimestampedLog( "CStaticPropMgr::PrecacheLighting - start");

	int numVerts = 0;
	if ( IsGameConsole() )
	{
		if ( g_bLoadedMapHasBakedPropLighting && g_pMaterialSystemHardwareConfig->SupportsStreamOffset() )
		{
			// total the static prop verts
			int i = m_StaticProps.Count();
			while ( --i >= 0 )
			{
				studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( ( (model_t*)m_StaticProps[i].GetModel() )->studio );
				for ( int lodID = pStudioHWData->m_RootLOD; lodID < pStudioHWData->m_NumLODs; lodID++ )
				{
					studioloddata_t *pLOD = &pStudioHWData->m_pLODs[lodID];
					for ( int meshID = 0; meshID < pStudioHWData->m_NumStudioMeshes; meshID++ )
					{
						studiomeshdata_t *pMesh = &pLOD->m_pMeshData[meshID];
						for ( int groupID = 0; groupID < pMesh->m_NumGroup; groupID++ )
						{
							numVerts += pMesh->m_pMeshGroup[groupID].m_NumVertices;
						}
					}
				}
			}
		}
		modelrender->SetupColorMeshes( numVerts );
	}

	int total = m_StaticProps.Count();
	for ( int i = 0; i < total; i++ )
	{
		MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
		m_StaticProps[i].PrecacheLighting();
#ifndef DEDICATED
		EngineVGui()->UpdateProgressBar( PROGRESS_PRECACHELIGHTING );
#endif
	}

	COM_TimestampedLog( "CStaticPropMgr::PrecacheLighting - end" );
}

void CStaticPropMgr::RecomputeStaticLighting( )
{
	int i = m_StaticProps.Count();
	while ( --i >= 0 )
	{
		if ( m_StaticProps[i].RenderHandle() == INVALID_CLIENT_RENDER_HANDLE )
			continue;
		m_StaticProps[i].RecomputeStaticLighting();
	}
}


//-----------------------------------------------------------------------------
// Is the prop in the PVS?
//-----------------------------------------------------------------------------
bool CStaticPropMgr::IsPropInPVS( IHandleEntity *pHandleEntity, const byte *pVis ) const
{
	// Strip off the bits
	int nIndex = HandleEntityToIndex( pHandleEntity );

	// Get the prop
	const CStaticProp &prop = m_StaticProps[nIndex];

	int i;
	int end = prop.FirstLeaf() + prop.LeafCount();
	for( i = prop.FirstLeaf(); i < end; i++ )
	{
		Assert( i >= 0 && i < m_StaticPropLeaves.Count() );
		int clusterID = CM_LeafCluster( m_StaticPropLeaves[i].m_Leaf );
		if( pVis[ clusterID >> 3 ] & ( 1 << ( clusterID & 7 ) ) )
		{
			return true;
		}
	}
	return false;
}


void CStaticPropMgr::DrawStaticProps_Slow( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth, bool drawVCollideWireframe )
{
	VPROF("CStaticPropMgr::DrawStaticProps_Slow");
	// slow mode
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
	int flags = STUDIO_RENDER;
	if (bShadowDepth)
		flags |= STUDIO_SHADOWDEPTHTEXTURE;
	if ( drawVCollideWireframe )
		flags |= STUDIO_WIREFRAME_VCOLLIDE;

	for ( int i = 0; i < count; i++ )
	{
		CStaticProp *pProp = (CStaticProp *)(pProps[i]);
		pProp->DrawModelSlow( flags, pInstances[i] );
	}
}

void CStaticPropMgr::DrawStaticProps_Fast( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth )
{
	VPROF("CStaticPropMgr::DrawStaticProps_Fast");
	float color[3];
	color[0] = color[1] = color[2] = 1.0f;
	g_pStudioRender->SetColorModulation(color);
	g_pStudioRender->SetAlphaModulation(1.0f);
	g_pStudioRender->SetViewState( CurrentViewOrigin(), CurrentViewRight(), CurrentViewUp(), CurrentViewForward() );
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	ModelRenderInfo_t sInfo;
	sInfo.flags = STUDIO_RENDER | STUDIO_STATIC_LIGHTING;
	if (bShadowDepth)
		sInfo.flags |= STUDIO_SHADOWDEPTHTEXTURE;

	sInfo.entity_index = -1;
	sInfo.body = 0;
	sInfo.hitboxset = 0;
	sInfo.pLightingOffset = NULL;
	for ( int i = 0; i < count; i++ )
	{
		MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
		CStaticProp *pProp = (CStaticProp *)(pProps[i]);
		if ( !pProp->m_pModel )
			continue;
		sInfo.instance = pProp->m_ModelInstance;
		sInfo.pModel = pProp->m_pModel;
		sInfo.origin = pProp->m_Origin;
		sInfo.angles = pProp->m_Angles;
		sInfo.skin = pProp->m_Skin;
		sInfo.pLightingOrigin = &pProp->m_LightingOrigin;
		sInfo.pModelToWorld = &pProp->m_ModelToWorld;
		sInfo.pRenderable = pProps[i];
		modelrender->DrawModelExStaticProp( pRenderContext, sInfo );
	}
	// Restore the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}

// NOTE: This is a work in progress for a new static prop (eventually new model) rendering pipeline
void CStaticPropMgr::DrawStaticProps_FastPipeline( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth )
{
	VPROF("CStaticPropMgr::DrawStaticProps_FastPipeline");
	const int MAX_OBJECTS = 2048;
	StaticPropRenderInfo_t propList[MAX_OBJECTS];
	int listCount = 0;
	if ( count > MAX_OBJECTS )
	{
		DrawStaticProps_FastPipeline( pProps + MAX_OBJECTS, pInstances, count - MAX_OBJECTS, bShadowDepth );
		count = MAX_OBJECTS;
	}

	for ( int i = 0; i < count; i++ )
	{
		CStaticProp *pProp = static_cast< CStaticProp * >(pProps[i]);
		if ( !pProp->m_pModel )
			continue;
		propList[listCount].pModelToWorld = &pProp->m_ModelToWorld;
		propList[listCount].pModel = pProp->m_pModel;
		propList[listCount].instance = pProp->m_ModelInstance;
		propList[listCount].skin = pProp->m_Skin;
		propList[listCount].pRenderable = pProp;
		propList[listCount].pLightingOrigin = &pProp->m_LightingOrigin;
		propList[listCount].alpha = pInstances[i].m_nAlpha;
		listCount++;
	}
	modelrender->DrawStaticPropArrayFast( propList, listCount, bShadowDepth );
}

// NOTE: Set this to zero to revert to the previous static prop lighting behavior
ConVar pipeline_static_props("pipeline_static_props", "1");
ConVar cl_skipslowpath( "cl_skipslowpath", "0", FCVAR_CHEAT, "Set to 1 to skip any models that don't go through the model fast path" );
void CStaticPropMgr::DrawStaticProps( IClientRenderable **pProps, const RenderableInstance_t *pInstances, int count, bool bShadowDepth, bool drawVCollideWireframe )
{
	VPROF_BUDGET( "CStaticPropMgr::DrawStaticProps", VPROF_BUDGETGROUP_STATICPROP_RENDERING );

	if ( !r_drawstaticprops.GetBool() || cl_skipslowpath.GetBool() )
		return;

	if ( IsUsingStaticPropDebugModes() || drawVCollideWireframe || r_slowpathwireframe.GetInt() )
	{
		DrawStaticProps_Slow( pProps, pInstances, count, bShadowDepth, drawVCollideWireframe );
	}
	else
	{
		// the fast pipeline is only supported on dx8+
		if ( pipeline_static_props.GetBool() )
		{
			DrawStaticProps_FastPipeline( pProps, pInstances, count, bShadowDepth );
		}
		else
		{
			DrawStaticProps_Fast( pProps, pInstances, count, bShadowDepth );
		}
	}
}


//-----------------------------------------------------------------------------
// Returns the lightcache handle
//-----------------------------------------------------------------------------
LightCacheHandle_t CStaticPropMgr::GetLightCacheHandleForStaticProp( IHandleEntity *pHandleEntity )
{
	int nIndex = HandleEntityToIndex(pHandleEntity);
	return modelrender->GetStaticLighting( m_StaticProps[ nIndex ].GetModelInstance() );
}


//-----------------------------------------------------------------------------
// Purpose: Trace a ray against the specified static Prop. Returns point of intersection in trace_t
//-----------------------------------------------------------------------------
void CStaticPropMgr::TraceRayAgainstStaticProp( const Ray_t& ray, int staticPropIndex, trace_t& tr )
{
#ifndef DEDICATED
	// Get the prop
	CStaticProp& prop = m_StaticProps[staticPropIndex];

	if (prop.GetSolid() != SOLID_NONE)
	{
		// FIXME: Better bloat?
		// Bloat a little bit so we get the intersection
		Ray_t temp = ray;
		temp.m_Delta *= 1.1f;
		g_pEngineTraceClient->ClipRayToEntity( temp, MASK_ALL, &prop, &tr );
	}
	else
	{
		// no collision
		tr.fraction = 1.0f;
	}
#endif
}


//-----------------------------------------------------------------------------
// Adds decals to static props, returns point of decal in trace_t
//-----------------------------------------------------------------------------
void CStaticPropMgr::AddDecalToStaticProp( Vector const& rayStart, Vector const& rayEnd,
										  int staticPropIndex, int decalIndex, bool doTrace, trace_t& tr, void *pvProxyUserData, const Vector* saxis, int32 nAdditionalDecalFlags )
{
#ifndef DEDICATED
	// Invalid static prop? Blow it off! 
	if (staticPropIndex >= m_StaticProps.Count())
	{
		memset( &tr, 0, sizeof(trace_t) );
		tr.fraction = 1.0f;
		return;
	}

	Ray_t ray;
	ray.Init( rayStart, rayEnd );
	if (doTrace)
	{
		// Trace the ray against the prop
		TraceRayAgainstStaticProp( ray, staticPropIndex, tr );
		if (tr.fraction == 1.0f)
			return;
	}

	if ( !r_drawmodeldecals.GetInt() )
		return;

	// Get the prop
	CStaticProp& prop = m_StaticProps[staticPropIndex];

	// Found the point, now lets apply the decals
	Assert( prop.GetModelInstance() != MODEL_INSTANCE_INVALID );

	// Choose a new ray along which to project the decal based on
	// surface normal. This prevents decal skewing
	bool noPokethru = false;
	if (doTrace && (prop.GetSolid() == SOLID_VPHYSICS) && !tr.startsolid && !tr.allsolid)
	{
		Vector temp;
		VectorSubtract( tr.endpos, tr.plane.normal, temp );
		ray.Init( tr.endpos, temp );
		noPokethru = true;
	}

	// Player sprays attempt to limit the smearing and backfaces application?
	// but this code doesn't seem to work
	/*
	if ( !doTrace && !noPokethru && pvProxyUserData )
	{
		if ( (prop.GetSolid() == SOLID_VPHYSICS) && !tr.startsolid && !tr.allsolid )
		{
			Vector temp;
			VectorSubtract( tr.endpos, tr.plane.normal, temp );
			ray.Init( tr.endpos, temp );
			noPokethru = true;
		}
	}
	*/

	// FIXME: What to do about the body parameter?
	Vector up(0, 0, 1);
	if ( saxis )
	{
		up = (*saxis).Cross( rayEnd - rayStart );
	}

	modelrender->AddDecal( prop.GetModelInstance(), ray, up, decalIndex, 0, noPokethru, ADDDECAL_TO_ALL_LODS, NULL, 1.0f, 1.0f, pvProxyUserData, nAdditionalDecalFlags );
#endif
}


//-----------------------------------------------------------------------------
// Adds/removes shadows from static props
//-----------------------------------------------------------------------------
void CStaticPropMgr::AddShadowToStaticProp( unsigned short shadowHandle, IClientRenderable* pRenderable )
{
#ifndef DEDICATED
	Assert( dynamic_cast<CStaticProp*>(pRenderable) != 0 );

	CStaticProp* pProp = static_cast<CStaticProp*>(pRenderable);

	g_pShadowMgr->AddShadowToModel( shadowHandle, pProp->GetModelInstance() );
#endif
}

void CStaticPropMgr::RemoveAllShadowsFromStaticProp( IClientRenderable* pRenderable )
{
#ifndef DEDICATED
	Assert( dynamic_cast<CStaticProp*>(pRenderable) != 0 );
	CStaticProp* pProp = static_cast<CStaticProp*>(pRenderable);
	if (pProp->GetModelInstance() != MODEL_INSTANCE_INVALID)
	{
		g_pShadowMgr->RemoveAllShadowsFromModel( pProp->GetModelInstance() );
	}
#endif
}

//-----------------------------------------------------------------------------
// disable rendering into cascade shadow maps
// TODO: flag could/should be set in vrad
//-----------------------------------------------------------------------------
void CStaticPropMgr::DisableCSMRenderingForStaticProp( int staticPropIndex )
{
	// Invalid static prop? Blow it off! 
	if ( staticPropIndex >= m_StaticProps.Count() )
	{
		return;
	}

	// Get the prop
	CStaticProp& prop = m_StaticProps[staticPropIndex];

	if ( !( prop.Flags() & STATIC_PROP_NO_PER_VERTEX_LIGHTING ) )
	{
		prop.DisableCSMRendering();
	}
}


//-----------------------------------------------------------------------------
// Gets the lighting + material color of a static prop
//-----------------------------------------------------------------------------
void CStaticPropMgr::GetStaticPropMaterialColorAndLighting( trace_t* pTrace,
														   int staticPropIndex, Vector& lighting, Vector& matColor )
{
#ifndef DEDICATED
	// Invalid static prop? Blow it off! 
	if (staticPropIndex >= m_StaticProps.Count())
	{
		lighting.Init( 0, 0, 0 );
		matColor.Init( 1, 1, 1 );
		return;
	}

	// Get the prop
	CStaticProp& prop = m_StaticProps[staticPropIndex];

	// Ask the model info about what we need to know
	modelinfoclient->GetModelMaterialColorAndLighting( (model_t*)prop.GetModel(), 
		prop.GetRenderOrigin(), prop.GetRenderAngles(), pTrace, lighting, matColor );
#endif
}

//-----------------------------------------------------------------------------
// Little debugger tool to report which prop we're looking at
//-----------------------------------------------------------------------------
void Cmd_PropCrosshair_f (void)
{
	Vector endPoint;
	VectorMA( MainViewOrigin(), COORD_EXTENT * 1.74f, MainViewForward(), endPoint );

	Ray_t ray;
	ray.Init( MainViewOrigin(), endPoint );

	trace_t tr;
	CTraceFilterWorldAndPropsOnly traceFilter;
	g_pEngineTraceServer->TraceRay( ray, MASK_ALL, &traceFilter, &tr );

	if ( tr.hitbox > 0 ) 
		Msg( "hit prop %d\n", tr.hitbox - 1 );
	else
		Msg( "didn't hit a prop\n" );
}

static ConCommand prop_crosshair( "prop_crosshair", Cmd_PropCrosshair_f, "Shows name for prop looking at", FCVAR_CHEAT );

