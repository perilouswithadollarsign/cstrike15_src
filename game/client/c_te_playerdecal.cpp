//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_basetempentity.h"
#include "iefx.h"
#include "fx.h"
#include "decals.h"
#include "materialsystem/imaterialsystem.h"
#include "filesystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialvar.h"
#include "functionproxy.h"
#include "imaterialproxydict.h"
#include "precache_register.h"
#include "econ/econ_item_schema.h"
#include "tier0/vprof.h"
#include "playerdecals_signature.h"
#include "tier1/callqueue.h"
#include "engine/decal_flags.h"
#include "cstrike15/Scaleform/HUD/sfhud_rosettaselector.h"
#include "c_cs_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecachePlayerDecal )
PRECACHE( MATERIAL, "decals/playerlogo01" )
PRECACHE_REGISTER_END()

#define PLAYERDECAL_VERBOSE_DEBUG 0

void QcCreatePreviewDecal( uint32 nStickerKitDefinition, uint32 nTintID, const trace_t& trace, const Vector* pRight );

//-----------------------------------------------------------------------------
// Purpose: Player Decal TE
//-----------------------------------------------------------------------------
class C_TEPlayerDecal : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerDecal, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEPlayerDecal( void );
	virtual			~C_TEPlayerDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	int				m_nPlayer;
	Vector			m_vecOrigin;
	Vector			m_vecStart;
	Vector			m_vecRight;
	int				m_nEntity;
	int				m_nHitbox;
};

int g_nPlayerLogoProxyForPreviewKey = ( 8 << 24 ) | 0x1FFFF; // used for preview (model panel and in-game)
// there's a rendering batching approach that uses only low 16-bits of the proxy data for splitting decals into batches, this is sufficient for now, but
// eventually we might want to find all places that use only 16-bits and upgrade them to full proxy data value comparison, e.g. patterns like this:
// ( unPlayerDecalStickerKitID && ( pDecal->flags & FDECAL_PLAYERSPRAY ) && ( uint16( reinterpret_cast< uintp >( pDecal->userdata ) ) != unPlayerDecalStickerKitID ) )
inline int MakeKey( int nUniqueId )
{
	return ( 1 << 24 ) | nUniqueId;
}

// This whole thing exists so we can shunt data from the main thread to the material thread. 
// We will queue calls to create decal data and DeleteDecalData so that the material thread doesn't
// have to grab a mutex to examine s_mapUniqueId2DecalData.
struct DecalData_t
{
	ITexture* m_pTex;
	int m_nRarity;
	int m_nTintID;
	float m_flCreationTime;
};
static CUtlMap< int, DecalData_t, int, CDefLess< int > > s_mapUniqueId2DecalData;

void CreateDecalData( int nKey, ITexture* pTex, int nRarity, int nTintID, float flCreateTime )
{
	Assert( pTex );
	if ( !pTex )
		return;

	int itExisting = s_mapUniqueId2DecalData.Find( nKey );
	if ( itExisting != s_mapUniqueId2DecalData.InvalidIndex() )
	{
		DecalData_t &ddata = s_mapUniqueId2DecalData[ itExisting ];

		// Release the old texture
		Assert( ddata.m_pTex );
		ddata.m_pTex->DecrementReferenceCount();
		ddata.m_pTex = NULL;

		// Overwrite the data for the new item.
		ddata.m_pTex = pTex;
		ddata.m_nRarity = nRarity;
		ddata.m_nTintID = nTintID;
		ddata.m_flCreationTime = flCreateTime;

		return;
	}

	// It wasn't already in the map, so add it.
	DecalData_t dd = { pTex, nRarity, nTintID, flCreateTime };
	s_mapUniqueId2DecalData.Insert( nKey, dd );
}

void DeleteDecalData( int nKey )
{
	int it = s_mapUniqueId2DecalData.Find( nKey );
	Assert ( ( nKey == g_nPlayerLogoProxyForPreviewKey )
		|| ( it != s_mapUniqueId2DecalData.InvalidIndex() ) );
	if ( it == s_mapUniqueId2DecalData.InvalidIndex() )
		return;

	Assert( s_mapUniqueId2DecalData[ it ].m_pTex );
	s_mapUniqueId2DecalData[ it ].m_pTex->DecrementReferenceCount();

	s_mapUniqueId2DecalData.RemoveAt( it );
}

bool ReadDecalData( int nKey, DecalData_t* pOutDD )
{
	if ( !pOutDD )
		return false;

	int it = s_mapUniqueId2DecalData.Find( nKey );
	if ( it == s_mapUniqueId2DecalData.InvalidIndex() )
		return false;

	( *pOutDD ) = s_mapUniqueId2DecalData[ it ];
	return true;
}

inline bool BShouldHaveDrips( const Vector& normal )
{
	return fabs( normal.z ) < 0.8;
}

class C_FEPlayerDecal;
typedef CUtlMap< int, C_FEPlayerDecal *, int, CDefLess< int > > PlayerDecalsByUniqueId_t;
static PlayerDecalsByUniqueId_t s_mapPlayerDecalsUniqueIDsAll;		// All client-side player decals entities
static PlayerDecalsByUniqueId_t s_mapPlayerDecalsUniqueIDsToApply;	// Player decals entities that should still be applied to the world
static CUtlMap< int, bool, int, CDefLess< int > > s_mapPlayerDecalsUniqueIDsRecreating;	// Player decals entities that were temporarily destroyed and getting recreated ID->bReapplyPending
void OnPlayerDecalsLevelShutdown()
{	// we purge and re-apply decals here
	// so add all client-side decals that we know about into the IDs to apply map
	FOR_EACH_MAP( s_mapPlayerDecalsUniqueIDsAll, i )
	{
#if PLAYERDECAL_VERBOSE_DEBUG
		DevMsg( "DECAL: schedule for reapply ( %d )\n", s_mapPlayerDecalsUniqueIDsAll.Key( i ) );
#endif
		s_mapPlayerDecalsUniqueIDsToApply.InsertOrReplace( s_mapPlayerDecalsUniqueIDsAll.Key( i ), s_mapPlayerDecalsUniqueIDsAll.Element( i ) );
	}
	s_mapPlayerDecalsUniqueIDsRecreating.RemoveAll();
}

class C_FEPlayerDecal : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FEPlayerDecal, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_FEPlayerDecal( void ) {
		m_nUniqueID = 0;
		m_unAccountID = 0;
		m_unTraceID = 0;
		m_rtGcTime = 0;
		m_vecEndPos.Init();
		m_vecStart.Init();
		m_vecRight.Init();
		m_vecNormal.Init();
		m_nPlayer = 0;
		m_nEntity = 0;
		m_nHitbox = 0;
		m_nTintID = 0;
		m_flCreationTime = 0;
		m_nVersion = 0;
		V_memset( m_ubSignature, 0, sizeof( m_ubSignature ) );

		m_bDecalReadyToApplyToWorld = false;
	}
	virtual			~C_FEPlayerDecal( void )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ICallQueue* pCQ = pRenderContext->GetCallQueue();
		if ( pCQ )
			pCQ->QueueCall( DeleteDecalData, MakeKey( m_nUniqueID ) );
		else
			DeleteDecalData( MakeKey( m_nUniqueID ) );

		if ( m_nUniqueID )
		{
#if PLAYERDECAL_VERBOSE_DEBUG
			DevMsg( "DECAL: entity removed ( %d ): %s %s %s\n",
				m_nUniqueID,
				( ( s_mapPlayerDecalsUniqueIDsToApply.Find( m_nUniqueID ) == s_mapPlayerDecalsUniqueIDsToApply.InvalidIndex() ) ? "" : "{was scheduled for application}" ),
				( ( s_mapPlayerDecalsUniqueIDsAll.Find( m_nUniqueID ) == s_mapPlayerDecalsUniqueIDsAll.InvalidIndex() ) ? "[NOT REGISTERED IN MAP]" : "" ),
				( ( s_mapPlayerDecalsUniqueIDsRecreating.Find( m_nUniqueID ) == s_mapPlayerDecalsUniqueIDsRecreating.InvalidIndex() ) ? "" : ( ( s_mapPlayerDecalsUniqueIDsRecreating.Element( s_mapPlayerDecalsUniqueIDsRecreating.Find( m_nUniqueID ) ) ? "{recreate and reapply pending}" : "{recreating, but already applied}" ) ) ) );
#endif

			Assert( ( s_mapPlayerDecalsUniqueIDsToApply.Find( m_nUniqueID ) == s_mapPlayerDecalsUniqueIDsToApply.InvalidIndex() ) ||
				( s_mapPlayerDecalsUniqueIDsToApply.Element( s_mapPlayerDecalsUniqueIDsToApply.Find( m_nUniqueID ) ) == this ) );
			s_mapPlayerDecalsUniqueIDsToApply.Remove( m_nUniqueID );

			Assert( ( s_mapPlayerDecalsUniqueIDsAll.Find( m_nUniqueID ) != s_mapPlayerDecalsUniqueIDsAll.InvalidIndex() ) &&
				( s_mapPlayerDecalsUniqueIDsAll.Element( s_mapPlayerDecalsUniqueIDsAll.Find( m_nUniqueID ) ) == this ) );
			s_mapPlayerDecalsUniqueIDsAll.Remove( m_nUniqueID );
		}
	}

	virtual void	PostDataUpdate( DataUpdateType_t updateType );
	virtual void SetDestroyedOnRecreateEntities( void ) OVERRIDE;
	
	bool BMakeDecalReadyToApplyToWorld();
	void ApplyDecalDataToWorld();
	void MakeDecalReady( int nKey );

public:
	int m_nUniqueID;
	uint32 m_unAccountID;
	uint32 m_unTraceID;
	uint32 m_rtGcTime;
	Vector m_vecEndPos;
	Vector m_vecStart;
	Vector m_vecRight;
	Vector m_vecNormal;
	int m_nPlayer;
	int m_nEntity;
	int m_nHitbox;
	int m_nTintID;
	float m_flCreationTime;
	uint8 m_nVersion;
	uint8 m_ubSignature[ PLAYERDECALS_SIGNATURE_BYTELEN ];

private:
	bool m_bDecalReadyToApplyToWorld;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEPlayerDecal::C_TEPlayerDecal( void )
{
	m_nPlayer = 0;
	m_vecOrigin.Init();
	m_vecStart.Init();
	m_vecRight.Init();
	m_nEntity = 0;
	m_nHitbox = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEPlayerDecal::~C_TEPlayerDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEPlayerDecal::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void TE_PlayerDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, const Vector* right, int nPlayerAndStickerKitID, int entity, int hitbox, int nAdditionalDecalFlags )
{
	// Special case for world entity with hitbox:
	trace_t tr;
	if ( ( entity == 0 ) && ( hitbox != 0 ) )
	{
		Ray_t ray;
		ray.Init( *start, *pos );
		staticpropmgr->AddDecalToStaticProp( *start, *pos, hitbox - 1, nPlayerAndStickerKitID, false, tr, ( void * ) nPlayerAndStickerKitID, right, EDF_PLAYERSPRAY | nAdditionalDecalFlags );
	}
	else
	{
		// Only decal the world + brush models
		// Here we deal with decals on entities.
		C_BaseEntity* ent;
		if ( ( ent = cl_entitylist->GetEnt( entity ) ) == NULL )
			return;

		ent->AddDecal( *start, *pos, *pos, hitbox,
			nPlayerAndStickerKitID, false, tr, ADDDECAL_TO_ALL_LODS, right, EDF_PLAYERSPRAY | nAdditionalDecalFlags );
	}
}

void C_FEPlayerDecal::SetDestroyedOnRecreateEntities()
{
	BaseClass::SetDestroyedOnRecreateEntities();

	if ( m_nUniqueID )
	{	// Remeber that we are going to be recreating this decal entity
		s_mapPlayerDecalsUniqueIDsRecreating.InsertOrReplace( m_nUniqueID,
			s_mapPlayerDecalsUniqueIDsToApply.Find( m_nUniqueID ) != s_mapPlayerDecalsUniqueIDsToApply.InvalidIndex() );
	}
}

void C_FEPlayerDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	if ( m_nUniqueID )
	{
		if ( s_mapPlayerDecalsUniqueIDsAll.Find( m_nUniqueID ) == s_mapPlayerDecalsUniqueIDsAll.InvalidIndex() )
		{
			int idxRecreate = s_mapPlayerDecalsUniqueIDsRecreating.Find( m_nUniqueID );
			bool bApplyImmediate = true;
			bool bRecreating = ( idxRecreate != s_mapPlayerDecalsUniqueIDsRecreating.InvalidIndex() );
			if ( bRecreating )
			{
				bApplyImmediate = s_mapPlayerDecalsUniqueIDsRecreating.Element( idxRecreate );
				s_mapPlayerDecalsUniqueIDsRecreating.RemoveAt( idxRecreate );
			}

#if PLAYERDECAL_VERBOSE_DEBUG
			DevMsg( "DECAL: PostDataUpdate %s decal ( %d ) %s\n", bRecreating ? "recreating" : "new", m_nUniqueID, bApplyImmediate ? "apply" : "already applied, skipping application" );
#endif

			s_mapPlayerDecalsUniqueIDsAll.InsertOrReplace( m_nUniqueID, this );
			if ( bApplyImmediate )
				s_mapPlayerDecalsUniqueIDsToApply.InsertOrReplace( m_nUniqueID, this );
		}
	}

	// Make sure that we push render data to QMS thread every time we get data update about the decal
	m_bDecalReadyToApplyToWorld = BMakeDecalReadyToApplyToWorld();
}

DEVELOPMENT_ONLY_CONVAR( cl_playerspray_debug_pulse_force, 0 );

// Checks if the local player has an equipped spray and is aiming in a sprayable area with the rosetta menu up and if cooldown is ready
// Note: rosetta menu code is using this check to determine if we're passing all the validity checks to spray. 
bool Helper_CanShowPreviewDecal( CEconItemView **ppOutEconItemView = NULL, trace_t* pOutSprayTrace = NULL, Vector *pOutVecPlayerRight = NULL, uint32* pOutUnStickerKitID = NULL )
{
	if ( !Helper_CanUseSprays() )
		return false;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return false;

	if ( !cl_playerspray_debug_pulse_force.GetInt() )
	{
		// Check if UI is visible
		SFHudRosettaSelector* pRosetta = ( SFHudRosettaSelector* ) ( GetHud( 0 ).FindElement( "SFHudRosettaSelector" ) );
		if ( !pRosetta || !pRosetta->Visible() || !pRosetta->ShouldDraw() )
			return false;

		// Check player spray cooldown
		if ( pLocalPlayer->GetNextDecalTime() > gpGlobals->curtime )
			return false;
	}

	Vector playerRight;
	trace_t sprayTrace;
	if ( pLocalPlayer->IsAbleToApplySpray( &sprayTrace, NULL, &playerRight ) )
		return false;

	if ( pOutSprayTrace )
		*pOutSprayTrace = sprayTrace;

	if ( pOutVecPlayerRight )
		*pOutVecPlayerRight = playerRight;

	CCSPlayerInventory* pPlayerInv = CSInventoryManager()->GetLocalCSInventory();
	if ( !pPlayerInv )
		return false;

	CEconItemView* pEconItem = pPlayerInv->GetItemInLoadout( 0, LOADOUT_POSITION_SPRAY0 );
	if ( !pEconItem || !pEconItem->IsValid() )
		return false;

	if ( ppOutEconItemView )
		*ppOutEconItemView = pEconItem;

	uint32 unStickerKitID = pEconItem->GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 );
	if ( !unStickerKitID )
		return false;

	if ( pOutUnStickerKitID )
		*pOutUnStickerKitID = unStickerKitID;

	return true;
}

void UpdatePreviewDecal()
{
	uint32 unStickerKitID;
	trace_t sprayTrace;
	Vector playerRight;
	CEconItemView *pEconItem;
	if ( !Helper_CanShowPreviewDecal( &pEconItem, &sprayTrace, &playerRight, &unStickerKitID ) )
		return;

	static CSchemaAttributeDefHandle hAttrSprayTintID( "spray tint id" );
	uint32 unTintID = 0;
	if ( !hAttrSprayTintID || !pEconItem->FindAttribute( hAttrSprayTintID, &unTintID ) )
		unTintID = 0;

	QcCreatePreviewDecal( unStickerKitID, unTintID, sprayTrace, &playerRight );
}

void OnPlayerDecalsUpdate()
{
	FOR_EACH_MAP( s_mapPlayerDecalsUniqueIDsToApply, i )
	{
#if PLAYERDECAL_VERBOSE_DEBUG
		DevMsg( "DECAL: ApplyDecalDataToWorld( %d )\n", s_mapPlayerDecalsUniqueIDsToApply.Key( i ) );
#endif
		s_mapPlayerDecalsUniqueIDsToApply.Element( i )->ApplyDecalDataToWorld();
	}
	s_mapPlayerDecalsUniqueIDsToApply.RemoveAll();

#if PLAYERDECAL_VERBOSE_DEBUG
	if ( s_mapPlayerDecalsUniqueIDsRecreating.Count() )
	{
		DevMsg( "DECAL: Was recreating %d decals, cleared recreate cache\n", s_mapPlayerDecalsUniqueIDsRecreating.Count() );
	}
#endif
	s_mapPlayerDecalsUniqueIDsRecreating.RemoveAll();
}

bool C_FEPlayerDecal::BMakeDecalReadyToApplyToWorld()
{
	VPROF( "C_FEPlayerDecal::BMakeDecalReadyToApplyToWorld" );

	// Validate the signature before applying on the client
	PlayerDecalDigitalSignature data;
	data.set_accountid( m_unAccountID );
	data.set_trace_id( m_unTraceID );
	data.set_rtime( m_rtGcTime );
	for ( int k = 0; k < 3; ++ k ) data.add_endpos( m_vecEndPos[k] );
	for ( int k = 0; k < 3; ++ k ) data.add_startpos( m_vecStart[k] );
	for ( int k = 0; k < 3; ++ k ) data.add_right( m_vecRight[k] );
	for ( int k = 0; k < 3; ++ k ) data.add_normal( m_vecNormal[k] );
	data.set_tx_defidx( m_nPlayer );
	data.set_entindex( m_nEntity );
	data.set_hitbox( m_nHitbox );
	data.set_tint_id( m_nTintID );
	data.set_creationtime( m_flCreationTime );
	if ( m_nVersion == PLAYERDECALS_SIGNATURE_VERSION )
		data.set_signature( &m_ubSignature[0], PLAYERDECALS_SIGNATURE_BYTELEN );
#ifdef _DEBUG
	{
		float flendpos[ 3 ] = { data.endpos( 0 ), data.endpos( 1 ), data.endpos( 2 ) };
		float flstartpos[ 3 ] = { data.startpos( 0 ), data.startpos( 1 ), data.startpos( 2 ) };
		float flright[ 3 ] = { data.right( 0 ), data.right( 1 ), data.right( 2 ) };
		float flnormal[ 3 ] = { data.normal( 0 ), data.normal( 1 ), data.normal( 2 ) };
		DevMsg( "Client signature #%u e(%08X,%08X,%08X) s(%08X,%08X,%08X) r(%08X,%08X,%08X) n(%08X,%08X,%08X)\n", data.trace_id(),
			*reinterpret_cast< uint32 * >( &flendpos[ 0 ] ), *reinterpret_cast< uint32 * >( &flendpos[ 1 ] ), *reinterpret_cast< uint32 * >( &flendpos[ 2 ] ),
			*reinterpret_cast< uint32 * >( &flstartpos[ 0 ] ), *reinterpret_cast< uint32 * >( &flstartpos[ 1 ] ), *reinterpret_cast< uint32 * >( &flstartpos[ 2 ] ),
			*reinterpret_cast< uint32 * >( &flright[ 0 ] ), *reinterpret_cast< uint32 * >( &flright[ 1 ] ), *reinterpret_cast< uint32 * >( &flright[ 2 ] ),
			*reinterpret_cast< uint32 * >( &flnormal[ 0 ] ), *reinterpret_cast< uint32 * >( &flnormal[ 1 ] ), *reinterpret_cast< uint32 * >( &flnormal[ 2 ] )
			);
	}
#endif
	if ( !BValidateClientPlayerDecalSignature( data ) )
		return false;

	// Make the decal ready.
	const int nKey = MakeKey( m_nUniqueID );
	MakeDecalReady( nKey );

	return true;
}

void C_FEPlayerDecal::ApplyDecalDataToWorld()
{
	if ( !m_bDecalReadyToApplyToWorld )
		return;

	CLocalPlayerFilter filter;
	const int nKey = MakeKey( m_nUniqueID );
	TE_PlayerDecal( filter, 0.0f, &m_vecEndPos, &m_vecStart, &m_vecRight, nKey, m_nEntity, m_nHitbox, 0 );
}

void QcCreateDecalData( int nKey, int nStickerKitDefinition, int nTintID, bool bDrips, float flCreationTime )
{
	const CStickerKit *pStickerKit = GetItemSchema()->GetStickerKitDefinition( nStickerKitDefinition );
	if ( pStickerKit && !pStickerKit->sMaterialPath.IsEmpty() )
	{
		// TODO: We should convert this to be an async texture load once texture streaming is in.
		CFmtStr fmtTextureName( "decals/sprays/%s", bDrips ? pStickerKit->sMaterialPath.Get() : pStickerKit->sMaterialPathNoDrips.Get() );
		ITexture* texture = materials->FindTexture( fmtTextureName, TEXTURE_GROUP_DECAL, false );

		if ( !texture || texture->IsError() )
		{
			Assert( !"Failed to find a texture for this decal. We're never going to see it." );
			Warning( "Failed to find spray '%s', this is never going to render.\n", fmtTextureName.Access() );

			return;
		}

		texture->IncrementReferenceCount();

		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		ICallQueue* pCQ = pRenderContext->GetCallQueue();
		if ( pCQ )
			pCQ->QueueCall( CreateDecalData, nKey, texture, pStickerKit->nRarity, nTintID, flCreationTime );
		else
			CreateDecalData( nKey, texture, pStickerKit->nRarity, nTintID, flCreationTime );
	}
}

void QcCreatePreviewDecal( uint32 nStickerKitDefinition, uint32 nTintID, const trace_t& trace, const Vector* pRight )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ICallQueue* pCQ = pRenderContext->GetCallQueue();
	if ( pCQ )
		pCQ->QueueCall( DeleteDecalData, g_nPlayerLogoProxyForPreviewKey );
	else
		DeleteDecalData( g_nPlayerLogoProxyForPreviewKey );

	bool bHasDrips = BShouldHaveDrips( trace.plane.normal );

	// Matches code in CCSPlayer::SprayPaint. Update that if you touch this.
	Vector startPos = trace.endpos + trace.plane.normal;

	CLocalPlayerFilter filter;
	QcCreateDecalData( g_nPlayerLogoProxyForPreviewKey, nStickerKitDefinition, nTintID, bHasDrips, gpGlobals->curtime - PLAYERDECALS_DURATION_APPLY );
	TE_PlayerDecal( filter, 0.0f, &trace.endpos, &startPos, pRight, g_nPlayerLogoProxyForPreviewKey, trace.GetEntityIndex(), trace.hitbox, EDF_IMMEDIATECLEANUP );
}

IMaterial * QcCreateDecalDataForModelPreviewPanel( int nStickerKitDefinition, int nTintID )
{
	IMaterial *pMatStickerOverride = NULL;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	ICallQueue* pCQ = pRenderContext->GetCallQueue();
	if ( pCQ )
		pCQ->QueueCall( DeleteDecalData, g_nPlayerLogoProxyForPreviewKey );
	else
		DeleteDecalData( g_nPlayerLogoProxyForPreviewKey );

	if ( nStickerKitDefinition > 0 && GetItemSchema() && GetItemSchema()->GetStickerKitDefinition( nStickerKitDefinition ) )
	{
		char const *szDesiredPreviewMaterialPath = "decals/playerlogo01_modelpreview.vmt";
		pMatStickerOverride = materials->FindMaterial( szDesiredPreviewMaterialPath, TEXTURE_GROUP_OTHER );
		if ( pMatStickerOverride->IsErrorMaterial() )
		{
			KeyValues *pSpecificStickerMaterialKeyValues = new KeyValues( "vmt" );
			KeyValues::AutoDelete autodelete_pSpecificStickerMaterialKeyValues( pSpecificStickerMaterialKeyValues );

			if ( pSpecificStickerMaterialKeyValues->LoadFromFile( g_pFullFileSystem, szDesiredPreviewMaterialPath, "GAME" ) )
			{
				pMatStickerOverride = materials->CreateMaterial( szDesiredPreviewMaterialPath, pSpecificStickerMaterialKeyValues );
			}

			autodelete_pSpecificStickerMaterialKeyValues.Detach();
		}
	}

	if ( !pMatStickerOverride || pMatStickerOverride->IsErrorMaterial() )
		return NULL;

	QcCreateDecalData( g_nPlayerLogoProxyForPreviewKey, nStickerKitDefinition, nTintID, true, gpGlobals->curtime - PLAYERDECALS_DURATION_APPLY );

	return pMatStickerOverride;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_FEPlayerDecal::MakeDecalReady( int nKey )
{
	QcCreateDecalData( nKey, m_nPlayer, m_nTintID, BShouldHaveDrips( m_vecNormal ), m_flCreationTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEPlayerDecal::PostDataUpdate( DataUpdateType_t updateType )
{
	VPROF( "C_TEPlayerDecal::PostDataUpdate" );

	// Decals disabled?
	if ( !r_decals.GetBool() )
		return;

	CLocalPlayerFilter filter;
	TE_PlayerDecal(  filter, 0.0f, &m_vecOrigin, &m_vecStart, &m_vecRight, m_nPlayer, m_nEntity, m_nHitbox, 0 );
}

IMPLEMENT_CLIENTCLASS_EVENT_DT(C_TEPlayerDecal, DT_TEPlayerDecal, CTEPlayerDecal)
	RecvPropVector( RECVINFO(m_vecOrigin)),
	RecvPropVector( RECVINFO(m_vecStart)),
	RecvPropVector( RECVINFO(m_vecRight)),
	RecvPropInt( RECVINFO(m_nEntity)),
	RecvPropInt( RECVINFO(m_nPlayer)),
	RecvPropInt( RECVINFO(m_nHitbox)),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT(C_FEPlayerDecal, DT_FEPlayerDecal, CFEPlayerDecal)
	RecvPropInt( RECVINFO( m_nUniqueID ) ),
	RecvPropInt( RECVINFO( m_unAccountID ) ),
	RecvPropInt( RECVINFO( m_unTraceID ) ),
	RecvPropInt( RECVINFO( m_rtGcTime ) ),
	RecvPropVector( RECVINFO(m_vecEndPos)),
	RecvPropVector( RECVINFO(m_vecStart)),
	RecvPropVector( RECVINFO(m_vecRight)),
	RecvPropVector( RECVINFO(m_vecNormal)),
	RecvPropInt( RECVINFO(m_nEntity)),
	RecvPropInt( RECVINFO(m_nPlayer)),
	RecvPropInt( RECVINFO(m_nHitbox)),
	RecvPropInt( RECVINFO(m_nTintID)),
	RecvPropFloat( RECVINFO( m_flCreationTime ) ),
	RecvPropInt( RECVINFO(m_nVersion)),
	RecvPropArray3( RECVINFO_ARRAY( m_ubSignature ), RecvPropInt( RECVINFO( m_ubSignature[0] ) ) ),
END_RECV_TABLE()




//-----------------------------------------------------------------------------
// Purpose: material proxy
//-----------------------------------------------------------------------------
class CPlayerLogoProxy : public IMaterialProxy
{
public:
	CPlayerLogoProxy();

	virtual bool Init( IMaterial* pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release()
	{
		if ( m_pDefaultTexture )
		{
			m_pDefaultTexture->DecrementReferenceCount();
		}

		delete this;
	}

	virtual IMaterial *GetMaterial();

	virtual bool CanBeCalledAsync() const { return true; }

protected:
	virtual void OnLogoBindInternal( const DecalData_t& decalData, bool bPreviewMaterial );
	bool m_bInspectInModelPreviewWindow;
	bool m_bVertexLitMaterial;

private:
	IMaterialVar *m_pAlphaVar;
	IMaterialVar *m_pColorVar;
	IMaterialVar *m_pDetailBlendFactorVar;
	IMaterialVar *m_pBaseTextureVar;
	ITexture *m_pDefaultTexture;
};

CPlayerLogoProxy::CPlayerLogoProxy()
{
	m_bInspectInModelPreviewWindow = false;
	m_bVertexLitMaterial = false;
	m_pAlphaVar = NULL;
	m_pColorVar = NULL;
	m_pDetailBlendFactorVar = NULL;
	m_pBaseTextureVar = NULL;
	m_pDefaultTexture = NULL;
}

bool CPlayerLogoProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool found = false;

	m_pAlphaVar = pMaterial->FindVar( "$alpha", &found );
	if ( !found )
		return false;
	if ( !m_pAlphaVar )
		return false;

	m_bVertexLitMaterial = pMaterial->IsVertexLit();
	// m_pColorVar = pMaterial->FindVar( pMaterial->IsVertexLit() ? "$color2" : "$color", &found );
	m_pColorVar = pMaterial->FindVar( "$color", &found );
	if ( !found )
		return false;
	if ( !m_pColorVar )
		return false;

	m_pDetailBlendFactorVar = pMaterial->FindVar( "$detailblendfactor", &found );
	if ( !found )
		return false;
	if ( !m_pDetailBlendFactorVar )
		return false;

	m_pBaseTextureVar = pMaterial->FindVar( "$basetexture", &found );
	if ( !found )
		return false;
	if ( !m_pBaseTextureVar )
		return false;

	m_pDefaultTexture = m_pBaseTextureVar->GetTextureValue();
	if ( !m_pDefaultTexture )
		return false;
	m_pDefaultTexture->IncrementReferenceCount();

	return true;
}

void CPlayerLogoProxy::OnBind( void *pC_BaseEntity )
{
	if ( !pC_BaseEntity )
		return; // dummy bind from mesh init

	if ( !m_pBaseTextureVar )
		return;

	DecalData_t dd;
	if ( ReadDecalData( ( int )( intp ) pC_BaseEntity, &dd ) )
	{
		bool bPreviewMaterial = ( g_nPlayerLogoProxyForPreviewKey == ( int )( intp ) pC_BaseEntity );
		OnLogoBindInternal( dd, bPreviewMaterial );
		return;
	}

	m_pBaseTextureVar->SetTextureValue( m_pDefaultTexture );
	m_pAlphaVar->SetFloatValue( 0.0f );
	m_pColorVar->SetVecValue( 1.0f, 1.0f, 1.0f, 1.0f );
	m_pDetailBlendFactorVar->SetFloatValue( 0.0f );
}


DEVELOPMENT_ONLY_CONVAR( cl_playerspray_debug_pulse_timescale, 0.6 );
DEVELOPMENT_ONLY_CONVAR( cl_playerspray_debug_pulse_alpha_low, 0.125 );
DEVELOPMENT_ONLY_CONVAR( cl_playerspray_debug_pulse_alpha_fraction, 1.0 );


#if ECON_SPRAY_TINT_IDS_FLOAT_COLORSPACE
void Helper_TintColorSpaceToRGBf( uint8 unHSVID, float arrFlcs[3], float flRenderRGB[3] )
{
	//
	// Tint processing
	// see: csgo_english.txt and econ_item_schema.cpp Helper_ExtractIntegerFromValueStringEntry for tint color names
	//
	struct CHSVSprayTintDefinition_t
	{
		// Based on the hue value the correct lerped bucket will be used
		int m_hue[3]; // HUE      low-mid-hi
		int m_slo[3]; // SAT low  low-mid-hi
		int m_shi[3]; // SAT high low-mid-hi
		int m_vlo[3]; // VAL low  low-mid-hi
		int m_vhi[3]; // VAL high low-mid-hi
	}
	const arrSprayTintDefinitions[] =
	{
		{	// zero-index: all white
			{	180,	180,	180 },	// hue
			{	0,		0,		0 },	// sat low
			{	0,		0,		0 },	// sat high
			{	100,	100,	100 },	// val low
			{	100,	100,	100 },	// val high
		},
		{	// index #1 - Aqua
			{ 158, 174, 190 }, // hue
			{ 50, 60, 40 },  // sat low
			{ 90, 90, 90 },  // sat high
			{ 60, 60, 60 },  // val low
			{ 90, 90, 90 }, // val high
		},
		{	// index #2 - Red
			{ 348, 358, 368 }, // hue
			{ 78, 80, 70 },  // sat low
			{ 90, 90, 90 },  // sat high
			{ 48, 50, 50 },  // val low
			{ 82, 80, 80 }, // val high
		},
		{	// index #3 - Orange
			{ 18, 28, 38 }, // hue
			{ 74, 72, 70 },  // sat low
			{ 94, 92, 90 },  // sat high
			{ 58, 78, 74 },  // val low
			{ 84, 92, 98 }, // val high
		},
		{	// index #4 - Yellow
			{ 48, 54, 61 }, // hue
			{ 40, 40, 40 },  // sat low
			{ 92, 92, 92 },  // sat high
			{ 82, 82, 82 },  // val low
			{ 98, 98, 98 }, // val high
		},
		{	// index #5 - Green
			{ 108, 129, 150 }, // hue
			{ 40, 50, 54 },  // sat low
			{ 90, 90, 90 },  // sat high
			{ 60, 60, 58 },  // val low
			{ 90, 90, 90 }, // val high
		},
		{	// index #6 - Blue
			{ 207, 223, 238 }, // hue
			{ 70, 40, 50 },  // sat low
			{ 90, 90, 90 },  // sat high
			{ 60, 50, 70 },  // val low
			{ 100, 80, 100 }, // val high
		},
		{	// index #7 - Purple
			{ 258, 272, 289 }, // hue
			{ 50, 40, 40 },  // sat low
			{ 90, 80, 80 },  // sat high
			{ 60, 60, 60 },  // val low
			{ 100, 90, 80 }, // val high
		},
		{	// index #8 - Pink
			{ 308, 340, 372 }, // hue
			{ 20, 20, 20 },  // sat low
			{ 67, 50, 64 },  // sat high
			{ 84, 80, 86 },  // val low
			{ 98, 98, 98 }, // val high
		},
		{	// index #9 - Lime
			{ 68, 78, 94 }, // hue
			{ 50, 50, 50 },  // sat low
			{ 92, 90, 90 },  // sat high
			{ 62, 70, 80 },  // val low
			{ 98, 98, 98 }, // val high
		},
		{	// index #10 - White
			{ 0, 180, 360 }, // hue
			{ 2, 2, 2 },  // sat low
			{ 7, 7, 7 },  // sat high
			{ 90, 90, 90 },  // val low
			{ 97, 97, 97 }, // val high
		},
	};
	COMPILE_TIME_ASSERT( 1 + NUM_SPRAY_TINT_IDS_SUPPORTED == Q_ARRAYSIZE( arrSprayTintDefinitions ) );
	if ( ( unHSVID > 0 ) && ( unHSVID <= NUM_SPRAY_TINT_IDS_SUPPORTED ) )
	{
		CHSVSprayTintDefinition_t const &tintdef = arrSprayTintDefinitions[unHSVID];
		
		//
		// Hue percentage value
		//
		float flHuePct = arrFlcs[0]*2.0f;
		int nHueIdx = 0; // 0 when in first half, 1 when in second half
		if ( flHuePct > 1.0f )
		{
			nHueIdx = 1;
			flHuePct -= 1.0f;
		}

		//
		// Actual HSV interpolated value
		//
		float flHue = Lerp<float>( flHuePct, tintdef.m_hue[nHueIdx], tintdef.m_hue[nHueIdx+1] );
		float flSat = Lerp<float>( arrFlcs[1],
			Lerp<float>( flHuePct, tintdef.m_slo[nHueIdx], tintdef.m_slo[nHueIdx+1] ),
			Lerp<float>( flHuePct, tintdef.m_shi[nHueIdx], tintdef.m_shi[nHueIdx+1] ) ) / 100.0f;
		float flVal = Lerp<float>( arrFlcs[2],
			Lerp<float>( flHuePct, tintdef.m_vlo[nHueIdx], tintdef.m_vlo[nHueIdx+1] ),
			Lerp<float>( flHuePct, tintdef.m_vhi[nHueIdx], tintdef.m_vhi[nHueIdx+1] ) ) / 100.0f;
		if ( flHue >= 360.0f )
			flHue -= 360.0f; // rotate into [0:360) range if needed overlapping area for lerps

		//
		// Now we just need to convert HSV->RGB
		//
		float hh = flHue/60.0f;
		int numhh = int( hh );
		float ff = hh - numhh;
		float xp = flVal * ( 1.0f - flSat );
		float xq = flVal * ( 1.0f - ( flSat * ff ) );
		float xt = flVal * ( 1.0f - ( flSat * ( 1.0f - ff ) ) );
		switch ( numhh )
		{
		case 0:
			flRenderRGB[ 0 ] = flVal; flRenderRGB[ 1 ] = xt; flRenderRGB[ 2 ] = xp;
			break;
		case 1:
			flRenderRGB[ 0 ] = xq; flRenderRGB[ 1 ] = flVal; flRenderRGB[ 2 ] = xp;
			break;
		case 2:
			flRenderRGB[ 0 ] = xp; flRenderRGB[ 1 ] = flVal; flRenderRGB[ 2 ] = xt;
			break;
		case 3:
			flRenderRGB[ 0 ] = xp; flRenderRGB[ 1 ] = xq; flRenderRGB[ 2 ] = flVal;
			break;
		case 4:
			flRenderRGB[ 0 ] = xt; flRenderRGB[ 1 ] = xp; flRenderRGB[ 2 ] = flVal;
			break;
		case 5:
		default:
			flRenderRGB[ 0 ] = flVal; flRenderRGB[ 1 ] = xp; flRenderRGB[ 2 ] = xq;
			break;
		}
	}
}
#endif


void CPlayerLogoProxy::OnLogoBindInternal( const DecalData_t& decalData, bool bPreviewMaterial )
{
	Assert( decalData.m_pTex );

	m_pBaseTextureVar->SetTextureValue( decalData.m_pTex );

	// Drive alpha to expiration (but not outside of creation time)
	float flRenderAlpha = 0;
	float flDetailBlendFactor = 0;
	if ( gpGlobals->curtime >= decalData.m_flCreationTime )
	{
		float flDecalTime = gpGlobals->curtime;
#if 0 // quick decals fading code
		flDecalTime = decalData.m_flCreationTime + PLAYERDECALS_DURATION_SOLID - 5;
		flDecalTime = flDecalTime + ( gpGlobals->curtime - decalData.m_flCreationTime ) * 4;
#endif
		if ( !m_bInspectInModelPreviewWindow && bPreviewMaterial )
		{
			flRenderAlpha = flDecalTime * cl_playerspray_debug_pulse_timescale.GetFloat(); // make the full cycle longer than 1 second
			flRenderAlpha -= floorf( flRenderAlpha ); // [0..1)[0..1)
			flRenderAlpha = fabsf( flRenderAlpha - 0.5f ); // 0.5..0..0.5..0..0.5
			flRenderAlpha = cl_playerspray_debug_pulse_alpha_low.GetFloat() + flRenderAlpha/cl_playerspray_debug_pulse_alpha_fraction.GetFloat(); // 12.5% ... 63% opacity cycle over 0.85 sec
		}
		else if ( gpGlobals->curtime >= decalData.m_flCreationTime + PLAYERDECALS_DURATION_APPLY )
		{
			float flFadeStart = decalData.m_flCreationTime + PLAYERDECALS_DURATION_SOLID;
			float flFadeEnd = flFadeStart + PLAYERDECALS_DURATION_FADE2;
			float flFadeAlphaStart = flFadeEnd - PLAYERDECALS_DURATION_FADE1;
			flDetailBlendFactor = RemapValClamped( flDecalTime, flFadeStart, flFadeEnd, 0.0f, 1.0f );
			flRenderAlpha = RemapValClamped( flDecalTime, flFadeAlphaStart, flFadeEnd, 1.0f, 0.0f );
		}
		else
		{
			float flFadeStart = decalData.m_flCreationTime;
			float flFadeEnd = flFadeStart + PLAYERDECALS_DURATION_APPLY;
			float flFadeAlphaStart = flFadeStart + PLAYERDECALS_DURATION_APPLY/2;
			flDetailBlendFactor = RemapValClamped( flDecalTime, flFadeStart, flFadeEnd, 1.0f, 0.0f );
			flRenderAlpha = RemapValClamped( flDecalTime, flFadeStart, flFadeAlphaStart, 0.0f, 1.0f );
		}
	}

	//
	// Convert tint HSV to RGB
	//
	float flRenderRGB[ 3 ] = { 1.0f, 1.0f, 1.0f };
	uint8 unRenderHSVID = CombinedTintIDGetHSVID( decalData.m_nTintID );
	if ( unRenderHSVID )
	{
#if ECON_SPRAY_TINT_IDS_FLOAT_COLORSPACE
		float arrFlcs[ 3 ] = { CombinedTintIDGetHSVc( decalData.m_nTintID, 0 ), CombinedTintIDGetHSVc( decalData.m_nTintID, 1 ), CombinedTintIDGetHSVc( decalData.m_nTintID, 2 ) };
		Helper_TintColorSpaceToRGBf( unRenderHSVID, arrFlcs, flRenderRGB );
#else
		if ( const CEconGraffitiTintDefinition *pDef = GEconItemSchema().GetGraffitiTintDefinitionByID( unRenderHSVID ) )
		{
			uint32 uiRGB = pDef->GetHexColorRGB();
			flRenderRGB[ 0 ] = float( ( uiRGB >> 16 ) & 0xFF ) / float( 0xFF );
			flRenderRGB[ 1 ] = float( ( uiRGB >> 8 ) & 0xFF ) / float( 0xFF );
			flRenderRGB[ 2 ] = float( ( uiRGB ) & 0xFF ) / float( 0xFF );
		}
#endif
	}

	// Do srgb color transform when applied to brush models via lightmapped generic
	if ( !m_bVertexLitMaterial && !m_bInspectInModelPreviewWindow )
	{
		for ( int j = 0; j < 3; ++j )
		{
			flRenderRGB[ j ] = SrgbGammaToLinear( flRenderRGB[ j ] );
		}
	}
	m_pColorVar->SetVecValue( flRenderRGB[ 0 ], flRenderRGB[ 1 ], flRenderRGB[ 2 ], 1.0f );

	m_pAlphaVar->SetFloatValue( flRenderAlpha );
	m_pDetailBlendFactorVar->SetFloatValue( flDetailBlendFactor );
}

IMaterial *CPlayerLogoProxy::GetMaterial()
{
	return m_pBaseTextureVar->GetOwningMaterial();
}

EXPOSE_MATERIAL_PROXY( CPlayerLogoProxy, PlayerLogo );

class CPlayerLogoProxyForModelWeaponPreviewPanel : public CPlayerLogoProxy
{
public:
	CPlayerLogoProxyForModelWeaponPreviewPanel()
	{
		m_bInspectInModelPreviewWindow = true;
	}
	virtual void OnBind( void *pC_BaseEntity ) OVERRIDE
	{
		CPlayerLogoProxy::OnBind( ( void * ) ( intp ) g_nPlayerLogoProxyForPreviewKey );
	}
};

EXPOSE_MATERIAL_PROXY( CPlayerLogoProxyForModelWeaponPreviewPanel, PlayerLogoModelPreview );
