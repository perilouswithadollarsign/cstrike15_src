//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======

//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "econ_entity_creation.h"
#include "vgui/ILocalize.h"
#include "tier3/tier3.h"

#if defined( CLIENT_DLL )
#define UTIL_VarArgs VarArgs

#include "econ_item_inventory.h"
#include "model_types.h"
#include "eventlist.h"
#include "networkstringtable_clientdll.h"

#if defined(CSTRIKE_CLIENT_DLL)
#include "cstrike15_item_inventory.h"
#endif

#if defined(TF_CLIENT_DLL)
#include "c_tf_player.h"
#include "tf_gamerules.h"
#include "c_playerresource.h"
#endif

#if defined(DOTA_DLL) && defined( GAME_DLL )
#include "dota_npc_base.h"
#endif
#if defined(DOTA_DLL) && defined( CLIENT_DLL )
#include "c_dota_npc_base.h"
#endif

extern INetworkStringTable *g_StringTableDynamicModels;
extern CUtlMap<int, int>	g_DynamicModelStringRemap;

#if defined(DOTA_DLL)
#include "dota_particle_manager.h"

#endif

extern INetworkStringTable *g_StringTableDynamicModels;
extern CUtlMap<int, int>	g_DynamicModelStringRemap;

#else // defined( CLIENT_DLL )

#include "activitylist.h"

#if defined(TF_DLL)
#include "tf_player.h"
#endif
#endif // defined( CLIENT_DLL )

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( EconEntity, DT_EconEntity )
IMPLEMENT_NETWORKCLASS_ALIASED( BaseAttributableItem, DT_BaseAttributableItem )

#if defined( CLIENT_DLL )
bool ParseItemKeyvalue( void *pObject, typedescription_t *pFields, int iNumFields, const char *szKeyName, const char *szValue );
#endif

#if defined(_DEBUG)
extern ConVar item_debug;
extern ConVar item_debug_validation;
#endif

#define NUM_FALLBACK_STATTRAK_BITS 20
#define INVALID_STATTRAK_VALUE int( uint32(~0) >> (sizeof(uint32)*8-NUM_FALLBACK_STATTRAK_BITS) )
inline bool IsValidStatTrakValue( int iValue )
{
	return ( iValue >= 0 ) && ( iValue < INVALID_STATTRAK_VALUE );
}

#if !defined( CLIENT_DLL )
	#define DEFINE_ECON_ENTITY_NETWORK_TABLE() \
		SendPropDataTable( SENDINFO_DT( m_AttributeManager ), &REFERENCE_SEND_TABLE(DT_AttributeContainer) ), \
		SendPropInt( SENDINFO( m_OriginalOwnerXuidLow ) ), \
		SendPropInt( SENDINFO( m_OriginalOwnerXuidHigh ) ), \
		SendPropInt( SENDINFO( m_nFallbackPaintKit ), 16, SPROP_UNSIGNED ), \
		SendPropInt( SENDINFO( m_nFallbackSeed ), 10, SPROP_UNSIGNED ), \
		SendPropFloat( SENDINFO( m_flFallbackWear ), 8,	SPROP_NOSCALE, 0.0f, 1.0f ), \
		SendPropInt( SENDINFO( m_nFallbackStatTrak ), NUM_FALLBACK_STATTRAK_BITS ),
#else
	#define DEFINE_ECON_ENTITY_NETWORK_TABLE() \
		RecvPropDataTable( RECVINFO_DT( m_AttributeManager ), 0, &REFERENCE_RECV_TABLE(DT_AttributeContainer) ), \
		RecvPropInt( RECVINFO( m_OriginalOwnerXuidLow ) ), \
		RecvPropInt( RECVINFO( m_OriginalOwnerXuidHigh ) ), \
		RecvPropInt( RECVINFO( m_nFallbackPaintKit ) ), \
		RecvPropInt( RECVINFO( m_nFallbackSeed ) ), \
		RecvPropFloat( RECVINFO( m_flFallbackWear ) ), \
		RecvPropInt( RECVINFO( m_nFallbackStatTrak ) ),
#endif // CLIENT_DLL

BEGIN_NETWORK_TABLE( CEconEntity , DT_EconEntity )
	DEFINE_ECON_ENTITY_NETWORK_TABLE()
END_NETWORK_TABLE()

BEGIN_DATADESC( CEconEntity )
END_DATADESC()

//
// Duplicating CEconEntity's network table and data description for backwards compat with demos.
// NOTE: NOTE_RENAMED_RECVTABLE() will not work with this class.
//
BEGIN_NETWORK_TABLE( CBaseAttributableItem, DT_BaseAttributableItem )
	DEFINE_ECON_ENTITY_NETWORK_TABLE()
END_NETWORK_TABLE()

BEGIN_DATADESC( CBaseAttributableItem )
END_DATADESC()

#ifdef TF_CLIENT_DLL
extern ConVar cl_flipviewmodels;
#endif


#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DrawEconEntityAttachedModels( CBaseAnimating *pEnt, CEconEntity *pAttachedModelSource, const ClientModelRenderInfo_t *pInfo, int iMatchDisplayFlags )
{
#ifndef DOTA_DLL
	if ( !pEnt || !pAttachedModelSource || !pInfo )
		return;

	// Draw our attached models as well
	for ( int i = 0; i < pAttachedModelSource->m_vecAttachedModels.Count(); i++ )
	{
		const CEconEntity::AttachedModelData_t& attachedModel = pAttachedModelSource->m_vecAttachedModels[i];

		if ( attachedModel.m_pModel && (attachedModel.m_iModelDisplayFlags & iMatchDisplayFlags) )
		{
			ClientModelRenderInfo_t infoAttached = *pInfo;
			
			infoAttached.pRenderable	= pEnt;
			infoAttached.instance		= MODEL_INSTANCE_INVALID;
			infoAttached.entity_index	= pEnt->index;
			infoAttached.pModel			= attachedModel.m_pModel;

			infoAttached.pModelToWorld  = &infoAttached.modelToWorld;

			// Turns the origin + angles into a matrix
			AngleMatrix( infoAttached.angles, infoAttached.origin, infoAttached.modelToWorld );

			//DrawModelState_t state;
			//matrix3x4_t *pBoneToWorld;
			//bool bMarkAsDrawn = modelrender->DrawModelSetup( infoAttached, &state, NULL, &pBoneToWorld );
			//pEnt->DoInternalDrawModel( &infoAttached, ( bMarkAsDrawn && ( infoAttached.flags & STUDIO_RENDER ) ) ? &state : NULL, pBoneToWorld );
		}
	}
#endif
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconEntity::CEconEntity()
{
	// Inform base entity system that we can deal with dynamic models
	// EnableDynamicModels();
#ifdef GAME_DLL
	m_iOldOwnerClass = 0;
#endif

#ifdef CLIENT_DLL
	m_flFlexDelayTime = 0.0f;
	m_flFlexDelayedWeight = NULL;
	m_iNumOwnerValidationRetries = 0;

	m_bAttributesInitialized = false;
#endif

	m_OriginalOwnerXuidLow = 0;
	m_OriginalOwnerXuidHigh = 0;

	m_nFallbackPaintKit = 0;
	m_nFallbackSeed = 0;
	m_flFallbackWear = 0;
	m_nFallbackStatTrak = -1;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconEntity::~CEconEntity()
{
#ifdef CLIENT_DLL
	SetParticleSystemsVisible( false );
	delete [] m_flFlexDelayedWeight;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CStudioHdr * CEconEntity::OnNewModel()
{
	CStudioHdr* hdr = BaseClass::OnNewModel();

#ifdef GAME_DLL
	// Adjust class-specific bodygroup after model load if we have a model and a class
	if ( hdr && m_iOldOwnerClass > 0 )
	{
		CEconItemView *pItem = GetAttributeContainer()->GetItem();
		if ( pItem && pItem->IsValid() && pItem->GetStaticData()->UsesPerClassBodygroups() )
		{
			// Classes start at 1, bodygroups at 0
			SetBodygroup( 1, m_iOldOwnerClass - 1 );
		}
	}
#endif

#ifdef TF_CLIENT_DLL
	// If we're carried by a player, let him know he should recalc his bodygroups.
	C_TFPlayer *pPlayer = ToTFPlayer( GetOwnerEntity() );
	if ( pPlayer )
	{
		pPlayer->SetBodygroupsDirty();
	}

	// allocate room for delayed flex weights
	delete [] m_flFlexDelayedWeight;
	if ( hdr && hdr->numflexcontrollers() )
	{
		m_flFlexDelayedWeight = new float[  hdr->numflexcontrollers() ];
		memset( m_flFlexDelayedWeight, 0, sizeof( float ) * hdr->numflexdesc() ); 

		C_BaseFlex::LinkToGlobalFlexControllers( hdr );
	}
#endif // TF_CLIENT_DLL

	return hdr;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::InitializeAttributes( void )
{
	m_AttributeManager.InitializeAttributes( this );
	m_AttributeManager.SetProviderType( PROVIDER_WEAPON );

	CEconItemView *pEconItemView = m_AttributeManager.GetItem();

#ifdef CLIENT_DLL

	m_bAttributesInitialized = true;

	bool bFoundSOData = false;
	CSteamID steamID = GetOriginalOwnerXuid();

	CPlayerInventory *pInventory = CSInventoryManager()->GetInventoryForPlayer( steamID );
	if ( pInventory )
	{
		CEconItem *pItem = pInventory->GetSOCDataForItem( pEconItemView->GetItemID() );
		if ( pItem )
		{
			bFoundSOData = true;

			// All econ entities for gameplay need to generate a material
			pEconItemView->UpdateGeneratedMaterial();

			// Generate sticker materials now
			pEconItemView->GenerateStickerMaterials();
		}
	}

	if ( !bFoundSOData && pEconItemView->GetItemID() > 0 )
	{
		// The SO Data for this item hasn't been retrieved yet!
		// This is the old obsolete fallback for demos
		if ( m_nFallbackPaintKit > 0 )
		{
			pEconItemView->SetOrAddAttributeValueByName( "set item texture prefab", m_nFallbackPaintKit );
		}

		if ( m_nFallbackSeed > 0 )
		{
			pEconItemView->SetOrAddAttributeValueByName( "set item texture seed", m_nFallbackSeed );
		}

		if ( m_flFallbackWear > 0.0f )
		{
			pEconItemView->SetOrAddAttributeValueByName( "set item texture wear", m_flFallbackWear );
		}

		if ( IsValidStatTrakValue( m_nFallbackStatTrak ) )
		{
			int nFallbackStatTrakInt = m_nFallbackStatTrak;
			float flFallbackStatTrakHack = *reinterpret_cast<float*>( ( char * ) &nFallbackStatTrakInt );
			pEconItemView->SetOrAddAttributeValueByName( "kill eater", flFallbackStatTrakHack );
			pEconItemView->SetOrAddAttributeValueByName( "kill eater score type", 0 );

			pEconItemView->m_bKillEaterTypesCached = false;
		}

		// Attributes could have also been networked through the new fallback system m_NetworkedDynamicAttributesForDemos
		if ( pEconItemView->GetCustomPaintKit() != 0 /*|| pEconItemView->GetCustomCharacterPaintKit() != 0*/ )
		{
			// All econ entities for gameplay need to generate a material
			pEconItemView->UpdateGeneratedMaterial();
			InventoryManager()->InsertMaterialGenerationJob( m_AttributeManager.GetItem() );
		}

		// Generate sticker materials now
		pEconItemView->GenerateStickerMaterials();

	}

#else

	pEconItemView->InitNetworkedDynamicAttributesForDemos();

	// Hack: String attributes don't fit in networked attribute lists. To get the custom name to clients
	// we use a separate network string for now... if this happens often we can rework it into a system.
	pEconItemView->UpdateNetworkedCustomName();

#endif	//#ifdef CLIENT_DLL



}


uint64 CEconEntity::GetOriginalOwnerXuid( void ) const
{
#ifdef CLIENT_DLL
	if ( CDemoPlaybackParameters_t const *pPlaybackParams = engine->GetDemoPlaybackParameters() )
	{
		if ( pPlaybackParams->m_bAnonymousPlayerIdentity )
			return 0ull; // force an anonymous weapon owner when watching Overwatch
	}
#endif
	return ( uint64( m_OriginalOwnerXuidHigh ) << 32 ) | uint64( m_OriginalOwnerXuidLow );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::DebugDescribe( void )
{
	CEconItemView *pScriptItem = GetAttributeContainer()->GetItem();

	Msg("============================================\n");
	char tempstr[1024];
// FIXME:	ILocalize::ConvertUnicodeToANSI( pScriptItem->GetItemName(), tempstr, sizeof(tempstr) );
	const char *pszQualityString = EconQuality_GetQualityString( (EEconItemQuality)pScriptItem->GetItemQuality() );
	Msg("%s \"%s\" (level %d)\n", pszQualityString ? pszQualityString : "[unknown]", tempstr, pScriptItem->GetItemLevel() );
	// FIXME: ILocalize::ConvertUnicodeToANSI( pScriptItem->GetAttributeDescription(), tempstr, sizeof(tempstr) );
	Msg("%s", tempstr );
	Msg("\n============================================\n");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::UpdateOnRemove( void )
{
	SetOwnerEntity( NULL );
	ReapplyProvision();
#ifdef CLIENT_DLL
	if ( m_hViewmodelAttachment )
	{
		m_hViewmodelAttachment->Remove();
	}
#endif

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::ReapplyProvision( void )
{
#ifdef GAME_DLL
	UpdateModelToClass();
#endif //#ifdef GAME_DLL

	CBaseEntity *pNewOwner = GetOwnerEntity();
	if ( pNewOwner == m_hOldProvidee.Get() )
		return;

	// Remove ourselves from the old providee's list
	if ( m_hOldProvidee.Get() && GetAttributeManager() )
	{
		GetAttributeManager()->StopProvidingTo( m_hOldProvidee.Get() );
	}

	// Add ourselves to our new owner's provider list
	if ( pNewOwner && GetAttributeManager() )
	{
		GetAttributeManager()->ProvideTo( pNewOwner );
	}

	m_hOldProvidee = pNewOwner;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CEconEntity::TranslateViewmodelHandActivity( Activity actBase )
{
	CEconItemView *pItem = GetAttributeContainer()->GetItem();
	if ( pItem && pItem->IsValid() )
	{
		const GameItemDefinition_t *pStaticData = pItem->GetStaticData();
		if ( pStaticData && pStaticData->ShouldAttachToHands() )
		{
			return TranslateViewmodelHandActivityInternal(actBase);
		}
	}

	return actBase;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static int GetCustomParticleEffectId( CEconEntity *pEconEntity )
{
	Assert( pEconEntity );
	Assert( pEconEntity->GetAttributeContainer() );
	Assert( pEconEntity->GetAttributeManager() == pEconEntity->GetAttributeContainer() );

	CEconItemView *pEconItemView = pEconEntity->GetAttributeContainer()->GetItem();

	int iCustomParticleEffect = 0;
	CALL_ATTRIB_HOOK_INT_ON_OTHER( pEconEntity, iCustomParticleEffect, set_attached_particle );

	if ( pEconItemView )
	{
		if ( iCustomParticleEffect == 0 )
		{
			iCustomParticleEffect = pEconItemView->GetQualityParticleType();
		}

		if ( iCustomParticleEffect == 0 )
		{
			static CSchemaAttributeDefHandle pAttrDef_SetAttachedParticle( "attach particle effect" );

			float flCustomParticleEffect;
			if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pEconItemView, pAttrDef_SetAttachedParticle, &flCustomParticleEffect ) )
			{
				iCustomParticleEffect = flCustomParticleEffect;
			}
		}
	}
	return iCustomParticleEffect;
}

#if !defined( CLIENT_DLL )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::OnOwnerClassChange( void )
{
#ifdef TF_DLL
	CTFPlayer *pPlayer = ToTFPlayer( GetOwnerEntity() );
	if ( pPlayer && pPlayer->GetPlayerClass()->GetClassIndex() != m_iOldOwnerClass )
	{
		UpdateModelToClass();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEconEntity::CalculateVisibleClassFor( CBaseCombatCharacter *pPlayer )
{
#ifdef TF_DLL
	CTFPlayer *pTFPlayer = ToTFPlayer( pPlayer );
	return (pTFPlayer ? pTFPlayer->GetPlayerClass()->GetClassIndex() : 0);
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
int CEconEntity::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	int iReturn = BaseClass::ShouldTransmit( pInfo );

// TODO
// 	CBaseEntity *pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );
// 
// 	if ( pRecipientEntity )
// 	{
// 		return ShouldBeSentToClient( pRecipientEntity->entindex(), iReturn );
// 	}

	return iReturn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::UpdateModelToClass( void )
{
#ifdef TF_DLL
	MDLCACHE_CRITICAL_SECTION();

	CTFPlayer *pPlayer = ToTFPlayer( GetOwnerEntity() );
	m_iOldOwnerClass = CalculateVisibleClassFor( pPlayer );
	if ( !pPlayer )
		return;

	CEconItemView *pItem = GetAttributeContainer()->GetItem();
	if ( !pItem->IsValid() )
		return;

	const char *pszModel = NULL;

	// If we attach to hands, we need to use the hand models
	if ( pItem->GetStaticData()->ShouldAttachToHands() )
	{
		pszModel = pPlayer->GetPlayerClass()->GetHandModelName( 0 );
	}
	else
	{
		pszModel = pItem->GetPlayerDisplayModel( m_iOldOwnerClass );
	}
	if ( pszModel && pszModel[0] )
	{
		if ( V_stricmp( STRING( GetModelName() ), pszModel ) != 0 )
		{
			if ( pItem->GetStaticData()->IsContentStreamable() )
			{
				modelinfo->RegisterDynamicModel( pszModel, IsClient() );
			}

			SetModel( pszModel );
		}
	}

	if ( GetModelPtr() && pItem->GetStaticData()->UsesPerClassBodygroups( GetTeamNumber() ) )
	{
		// Classes start at 1, bodygroups at 0, so we shift them all back 1.
		SetBodygroup( 1, (m_iOldOwnerClass-1) );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::PlayAnimForPlaybackEvent( wearableanimplayback_t iPlayback )
{
	CEconItemView *pItem = GetAttributeContainer()->GetItem();
	if ( !pItem->IsValid() || !GetOwnerEntity() )
		return;

	// Don't do this if we have no model.
	if ( !GetModel() || !GetModelPtr() )
		return;

	int iAnims = pItem->GetStaticData()->GetNumAnimations();
	for ( int i = 0; i < iAnims; i++ )
	{
		animation_on_wearable_t *pData = pItem->GetStaticData()->GetAnimationData( i );
		if ( pData && pData->iPlayback == iPlayback && pData->pszActivity )
		{
			// If this is the first time we've tried to use it, find the activity
			if ( pData->iActivity == -2 )
			{
				pData->iActivity = ActivityList_IndexForName( pData->pszActivity );
			}

			int sequence = SelectWeightedSequence( (Activity)pData->iActivity ); 
			if ( sequence != ACTIVITY_NOT_AVAILABLE )
			{
				ResetSequence( sequence );
				SetCycle( 0 );

				if ( IsUsingClientSideAnimation() )
				{
					ResetClientsideFrame();
				}
			}
			return;
		}
	}
}

#endif // #if !defined( CLIENT_DLL )

#if defined( CLIENT_DLL )
// It's okay to draw attached entities with these models.
const char* g_modelWhiteList[] =
{
	"models/weapons/w_models/w_toolbox.mdl",
	"models/weapons/w_models/w_sapper.mdl",

	// These are temporarily white-listed pending a proper fix to using model_player_per_class on weapons. 3/10/2011, BrandonR
	"models/weapons/c_models/c_shogun_katana/c_shogun_katana_soldier.mdl",
	"models/weapons/c_models/c_shogun_katana/c_shogun_katana.mdl"
};

//-----------------------------------------------------------------------------
// Purpose: TF prevents drawing of any entity attached to players that aren't items in the inventory of the player.
//			This is to prevent servers creating fake cosmetic items and attaching them to players.
//-----------------------------------------------------------------------------
bool CEconEntity::ValidateEntityAttachedToPlayer( bool &bShouldRetry )
{
	bShouldRetry = false;

	// We only use this variable in debug or on the client.
#if defined( _DEBUG ) || defined( TF_CLIENT_DLL )
	bool bItemDebugValidation = false;
#endif // defined( _DEBUG ) || defined( TF_CLIENT_DLL )

#ifdef _DEBUG
	bItemDebugValidation = item_debug_validation.GetBool();

	// Always valid in debug if item_debug_validation is disabled
	if ( !bItemDebugValidation )
		return true;
#endif // _DEBUG

#if defined( TF_CLIENT_DLL )
	// Always valid in item testing mode
	if ( TFGameRules()->IsInItemTestingMode() )
		return true;

	C_TFPlayer *pOwner = ToTFPlayer( GetOwnerEntity() );

	// If we're not carried by a player, we're not valid. This prevents them
	// parenting hats to ents that they then parent to the player.
	if ( !pOwner )
	{
		//Msg( "NO OWNER SET! %i\n", m_iNumOwnerValidationRetries );
		bShouldRetry = ( m_iNumOwnerValidationRetries < 500 );
		m_iNumOwnerValidationRetries++;
		return false;
	}

	// The owner entity must also be a move parent of this entity.
	bool bPlayerIsParented = false;
	C_BaseEntity *pEntity = this;
	while ( (pEntity = pEntity->GetMoveParent()) != NULL )
	{
		if ( pOwner == pEntity )
		{
			bPlayerIsParented = true;
			break;
		}
	}

	if ( !bPlayerIsParented )
	{
		//Msg( "NOT PARENTED! %i\n", m_iNumOwnerValidationRetries );
		bShouldRetry = ( m_iNumOwnerValidationRetries < 500 );
		m_iNumOwnerValidationRetries++;
		return false;
	}

	m_iNumOwnerValidationRetries = 0;

	bool bOwnerIsBot = false;
#if defined( _DEBUG ) || defined( TF_PVE_MODE )
	// We only need this in debug (for item_debug_validation) or PvE mode
	bOwnerIsBot = pOwner->IsABot(); // THIS IS INSECURE -- DO NOT USE THIS OUTSIDE OF DEBUG OR PVE MODE
#endif

#ifdef TF_PVE_MODE
	// Allow bots to use anything in PvE mode
	if ( bOwnerIsBot && TFGameRules()->IsPVEModeActive() )
		return true;
#endif // TF_PVE_MODE

	int iClass = pOwner->GetPlayerClass()->GetClassIndex();

	// Allow all weapons parented to the local player
	if ( pOwner == C_BasePlayer::GetLocalPlayer() )
	{
		// They can change the owner entity, so we have to keep checking.
		bShouldRetry = true;
		return true;
	}

	// HACK: For now, if our owner is a disguised spy, we assume everything is valid.
	if ( (pOwner->m_Shared.InCond( TF_COND_DISGUISED ) || pOwner->m_Shared.InCond( TF_COND_DISGUISING )) && iClass == TF_CLASS_SPY )
	{
		bShouldRetry = true; // Keep checking in case the player switches class or becomes no longer disguised
		return true;
	}

	// If our owner is a disguised spy, we validate everything based 
	// on the items carried by the person we're disguised as.
	/*if ( pOwner->m_Shared.InCond( TF_COND_DISGUISED ) )
	{
		// DAMN: This won't work. If our disguise target is a player we've never seen before,
		//		 we won't have a client entity, and hence we don't have their inventory.
		C_TFPlayer *pDisguiseTarget = ToTFPlayer( pOwner->m_Shared.GetDisguiseTarget() );
		if ( pDisguiseTarget && pDisguiseTarget != pOwner )
		{
			pOwner = pDisguiseTarget;
			iClass = pOwner->GetPlayerClass()->GetClassIndex();
		}
		else
		{
			// We're not disguised as a specific player. Make sure we lookup base weapons with the disguise class.
			iClass = pOwner->m_Shared.GetDisguiseClass();
		}
	}
	*/

	const char *pszClientModel = modelinfo->GetModelName( GetModel() );
	if ( pszClientModel && g_modelWhiteList[0] )
	{
		// Certain builder models are okay to have.
		for ( int i=0; i<ARRAYSIZE( g_modelWhiteList ); ++i )
		{
				if ( FStrEq( pszClientModel, g_modelWhiteList[i] ) )
					return true;
		}
	}

	// If our player doesn't have an inventory, we're not valid. 
	CTFPlayerInventory *pInv = pOwner->Inventory();
	if ( !pInv )
		return false;

	// If we've lost connection to the GC, let's just trust the server to avoid breaking the appearance for everyone.
	bool bSkipInventoryCheck = bItemDebugValidation && bOwnerIsBot; // will always be false in release builds
	if ( ( !pInv->GetSOC() || !pInv->GetSOC()->BIsInitialized() ) && !bSkipInventoryCheck )
	{
		bShouldRetry = true;
		return true;
	}

	CEconItemView *pScriptItem = GetAttributeContainer()->GetItem();

	// If the item isn't valid, we're probably an extra wearable for another item. See if our model is 
	// a model specified as the extra wearable for any of the items we have equipped.
	if ( !pScriptItem->IsValid() )
	{
		// Uninitialized client models return their model as '?'
		if ( pszClientModel && pszClientModel[0] != '?' )
		{
			CSteamID steamIDForPlayer;
			pOwner->GetSteamID( &steamIDForPlayer );

			for ( int i = 0; i < LOADOUT_POSITION_COUNT; i++ )
			{
				CEconItemView *pItem = TFInventoryManager()->GetItemInLoadoutForClass( iClass, i, &steamIDForPlayer );
				if ( pItem && pItem->IsValid() )
				{
					const char *pszAttached = pItem->GetExtraWearableModel();
					if ( pszAttached && pszAttached[0] )
					{
						if ( FStrEq( pszClientModel, pszAttached ) )
							return true;
					}
				}
			}
		}
		else if ( pszClientModel && pszClientModel[0] == '?' )
		{
			bShouldRetry = true;
		}

		return false;
	}

	// Skip this check for bots if item_debug_validation is enabled.
	if ( !pInv->GetInventoryItemByItemID( pScriptItem->GetItemID() ) && !bSkipInventoryCheck )
	{
		// If it's a base item, we allow it.
		CEconItemView *pBaseItem = TFInventoryManager()->GetBaseItemForClass( iClass, pScriptItem->GetStaticData()->GetLoadoutSlot(iClass) );
		if ( *pScriptItem != *pBaseItem )
		{
			const wchar_t *pwzItemName = pScriptItem->GetItemName();

			char szItemName[ MAX_ITEM_NAME_LENGTH ];
			ILocalize::ConvertUnicodeToANSI( pwzItemName, szItemName, sizeof( szItemName ) );

#ifdef _DEBUG
			Warning("Item '%s' attached to %s, but it's not in his inventory.\n", szItemName, pOwner->GetPlayerName() );
#endif
			return false;
		}
	}

	// Our model has to match the model in our script
	const char *pszScriptModel = pScriptItem->GetWorldDisplayModel();
	if ( !pszScriptModel )
	{
		pszScriptModel = pScriptItem->GetPlayerDisplayModel( iClass );
	}

	if ( pszClientModel && pszClientModel[0] && pszClientModel[0] != '?' )
	{
		// A model was set on the entity, let's make sure it matches the model in the script.
		if ( !pszScriptModel || !pszScriptModel[0] )
			return false;
		if ( FStrEq( pszClientModel, pszScriptModel ) == false )
			return false;
	}
	else
	{
		// The client model was not set, so check that there isn't a model set in the script either.
		if ( pszScriptModel && pszScriptModel[0] )
		{
			if ( pszClientModel[0] == '?' )
				bShouldRetry = true;

			return false;
		}
	}

	return true;
#else
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Set a material override for this entity via code
//-----------------------------------------------------------------------------
void CEconEntity::SetMaterialOverride( const char *pszMaterial )
{
	m_MaterialOverrides.Init( pszMaterial, TEXTURE_GROUP_CLIENT_EFFECTS );
}


//-----------------------------------------------------------------------------
void CEconEntity::SetMaterialOverride( CMaterialReference &ref )
{
	m_MaterialOverrides.Init( ref );
}

#ifndef DOTA_DLL


bool C_ViewmodelAttachmentModel::InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels )
{
	if ( !BaseClass::InitializeAsClientEntity( pszModelName, bRenderWithViewModels ) )
		return false;

	AddEffects( EF_BONEMERGE );
	AddEffects( EF_BONEMERGE_FASTCULL );

	// Invisible by default, and made visible->drawn->made invisible when the viewmodel is drawn
	AddEffects( EF_NODRAW );
	return true;
}

int C_ViewmodelAttachmentModel::InternalDrawModel( int flags, const RenderableInstance_t &instance )
{
#if defined(TF_CLIENT_DLL)
	CMatRenderContextPtr pRenderContext( materials );
	if ( cl_flipviewmodels.GetBool() != m_bAlwaysFlip )
	{
		pRenderContext->CullMode( MATERIAL_CULLMODE_CW );
	}
#endif
#if defined( CSTRIKE15 )
	CMatRenderContextPtr pRenderContext( materials );
	C_BaseViewModel *pViewmodel = m_hViewmodel;
	if ( pViewmodel && pViewmodel->ShouldFlipModel() )
		pRenderContext->CullMode( MATERIAL_CULLMODE_CW );
#endif

	int r = BaseClass::InternalDrawModel( flags, instance );

#if defined(TF_CLIENT_DLL) || defined( CSTRIKE15 )
	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
#endif

	return r;
}

void C_ViewmodelAttachmentModel::SetViewmodel( C_BaseViewModel *pVM )
{
	m_hViewmodel = pVM;
}

#endif // !defined( DOTA_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::SetDormant( bool bDormant )
{
	// If I'm burning, stop the burning sounds
	if ( !IsDormant() && bDormant && m_bParticleSystemsCreated )
	{
		SetParticleSystemsVisible( false );
	}

	BaseClass::SetDormant( bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::OnPreDataChanged( DataUpdateType_t type )
{
	BaseClass::OnPreDataChanged( type );

	m_iOldTeam = m_iTeamNum;
}

IMaterial *CreateTempMaterialForPlayerLogo( int iPlayerIndex, player_info_t *info, char *texname, int nchars );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::OnDataChanged( DataUpdateType_t updateType )
{
	// If we were just created, setup from the script files
	if ( updateType == DATA_UPDATE_CREATED )
	{
		InitializeAttributes();
		m_bParticleSystemsCreated = false;
		m_bAttachmentDirty = true;
	}

	BaseClass::OnDataChanged( updateType );

	GetAttributeContainer()->OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		CEconItemView *pItem = m_AttributeManager.GetItem();

#if defined(_DEBUG)
		if ( item_debug.GetBool() )
		{
			DebugDescribe();
		}
#endif

		// Find & cache for easy leaf code usage
		const char *pszMaterial = pItem->GetStaticData()->GetMaterialOverride();
		if ( pszMaterial )
		{
			m_MaterialOverrides.Init( pszMaterial, TEXTURE_GROUP_CLIENT_EFFECTS );
		}

#ifdef TF_CLIENT_DLL
		// If we're carried by a player, let him know he should recalc his bodygroups.
		C_TFPlayer *pPlayer = ToTFPlayer( GetOwnerEntity() );
		if ( pPlayer )
		{
			pPlayer->SetBodygroupsDirty();
		}

		//Warning("Forcing recalc of visiblity for %d\n", entindex().GetRaw());
		m_bValidatedOwner = false;
		m_iNumOwnerValidationRetries = 0;
		UpdateVisibility();
#endif // TF_CLIENT_DLL
	}

	UpdateAttachmentModels();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::UpdateAttachmentModels( void )
{
#ifndef DOTA_DLL
	CEconItemView *pItem = GetAttributeContainer()->GetItem();
	const GameItemDefinition_t *pItemDef = pItem && pItem->IsValid() ? pItem->GetStaticData() : NULL;

	// Update the state of additional model attachments
	m_vecAttachedModels.Purge();
	if ( pItemDef )
	{
		int iAttachedModels = pItemDef->GetNumAttachedModels();
		for ( int i = 0; i < iAttachedModels; i++ )
		{
			attachedmodel_t	*pModel = pItemDef->GetAttachedModelData( i );

			int iModelIndex = modelinfo->GetModelIndex( pModel->m_pszModelName );
			if ( iModelIndex >= 0 )
			{
				AttachedModelData_t attachedModelData;
				attachedModelData.m_pModel			   = modelinfo->GetModel( iModelIndex );
				attachedModelData.m_iModelDisplayFlags = pModel->m_iModelDisplayFlags;
				m_vecAttachedModels.AddToTail( attachedModelData );
			}
		}
	}

	// Update the state of attachment models for this item
 	bool bItemNeedsAttachment = pItemDef && (pItemDef->ShouldAttachToHands() || pItemDef->ShouldAttachToHandsVMOnly());
	if ( bItemNeedsAttachment )
	{
		bool bShouldShowAttachment = false;
		CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		CBasePlayer *pOwner = ToBasePlayer( GetOwnerEntity() );
		bShouldShowAttachment = ( pOwner && (pOwner == pLocalPlayer) || ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pLocalPlayer->GetObserverTarget() == pOwner ) );
		if ( bShouldShowAttachment && AttachmentModelsShouldBeVisible() )
		{
			if ( !m_hViewmodelAttachment )
			{
				C_BaseViewModel *vm = pOwner->GetViewModel( 0 );
				if ( vm )
				{
					C_ViewmodelAttachmentModel *pEnt = new class C_ViewmodelAttachmentModel;
					if ( !pEnt )
						return;

					if ( pEnt->InitializeAsClientEntity( pItem->GetPlayerDisplayModel(), true ) == false )
						return;

					m_hViewmodelAttachment = pEnt;
					m_hViewmodelAttachment->SetParent( vm );
					m_hViewmodelAttachment->SetLocalOrigin( vec3_origin );
					m_hViewmodelAttachment->UpdatePartitionListEntry();
					m_hViewmodelAttachment->CollisionProp()->UpdatePartition();
					m_hViewmodelAttachment->UpdateVisibility();

					m_bAttachmentDirty = true;
				}
			}
			else if ( m_hViewmodelAttachment )
			{
				// If a player changes team, we may need to update the skin on the attachment weapon model
				if ( m_iOldTeam != m_iTeamNum )
				{
					m_bAttachmentDirty = true;
				}
			}
			return;
		}
	}

	// If we get here we shouldn't have an attachment.
	if ( m_hViewmodelAttachment )
	{
		m_hViewmodelAttachment->Release();
	}

#endif // !defined( DOTA_DLL )
}

//-----------------------------------------------------------------z------------
// Purpose: Create / Destroy particle systems on this item as appropriate
//-----------------------------------------------------------------------------
void CEconEntity::UpdateParticleSystems( void )
{
	if ( !HasCustomParticleSystems() )
		return;

	bool bVisible = false;
	if ( IsEffectActive( EF_NODRAW ))
	{
		bVisible = false;
	}
	else if ( !GetOwnerEntity() && !IsDormant() )
	{
		bVisible = true;
	}
	else if ( GetOwnerEntity() && !GetOwnerEntity()->IsDormant() )
	{
		// Dota heroes turn off particle effects when they're dead
		if ( GetOwnerEntity()->IsAlive() )
		{
			bVisible = true;
		}
	}

	if ( bVisible )
	{
		bVisible = ShouldDrawParticleSystems();
	}

	SetParticleSystemsVisible( bVisible );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconEntity::ShouldDrawParticleSystems( void )
{
	if ( !ShouldDraw() )
		return false;

#if defined(TF_CLIENT_DLL) || defined(TF_DLL)
	C_TFPlayer *pPlayer = ToTFPlayer( GetOwnerEntity() );
	if ( pPlayer )
	{
		bool bStealthed = pPlayer->m_Shared.InCond( TF_COND_STEALTHED );
		if ( bStealthed )
			return false;
		bool bDisguised = pPlayer->m_Shared.InCond( TF_COND_DISGUISED );
		if ( bDisguised )
		{
			CTFWeaponBase *pWeapon = dynamic_cast<CTFWeaponBase*>( this );
			bool bDisguiseWeapon = pWeapon && pWeapon->m_bDisguiseWeapon;
			if ( !bDisguiseWeapon )
			{
				return false;
			}
		}
	}
#endif

	// Make sure the entity we're attaching to is being drawn
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		C_BaseEntity *pEffectOwner = this;
		if ( pLocalPlayer == GetOwnerEntity() && pLocalPlayer->GetViewModel() && !pLocalPlayer->ShouldDrawLocalPlayer() )
		{
			pEffectOwner = pLocalPlayer->GetViewModel();
		}

		if ( !pEffectOwner->ShouldDraw() )
		{
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconEntity::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if ( !InternalFireEvent( origin, angles, event, options ) )
	{
		BaseClass::FireEvent( origin, angles, event, options );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconEntity::OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options )
{
	return InternalFireEvent( origin, angles, event, options );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconEntity::InternalFireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	switch( event )
	{
	case AE_CL_BODYGROUP_SET_VALUE_CMODEL_WPN:
		if ( m_hViewmodelAttachment )
		{
			// Translate it into a set bodygroup event on our attached weapon
			m_hViewmodelAttachment->FireEvent( origin, angles, AE_CL_BODYGROUP_SET_VALUE, options );
		}
		return true;
		break;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Does this model use delayed flex weights?
//-----------------------------------------------------------------------------
bool CEconEntity::UsesFlexDelayedWeights()
{
	return m_flFlexDelayedWeight != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Rendering callback to allow the client to set up all the model specific flex weights
//-----------------------------------------------------------------------------
void CEconEntity::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	/*
	if ( GetModelPtr() && GetModelPtr()->numflexcontrollers() )
	{
		if ( IsEffectActive( EF_BONEMERGE ) && GetMoveParent() )
		{
			C_BaseFlex *pParentFlex = dynamic_cast<C_BaseFlex*>( GetMoveParent() );
			if ( pParentFlex )
			{
				if ( pParentFlex->SetupGlobalWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights ) )
				{
					// convert the flex controllers into actual flex values
					C_BaseFlex::RunFlexRules( GetModelPtr(), pFlexWeights );
					
					// aim the eyes
					// SetViewTarget( hdr ); // FIXME: Not enough info yet
					
					// process local versions of the delay weights
					if ( pFlexDelayedWeights )
					{
						C_BaseFlex::RunFlexDelay( nFlexWeightCount, pFlexWeights, m_flFlexDelayedWeight, m_flFlexDelayTime );
						memcpy( pFlexDelayedWeights, m_flFlexDelayedWeight, sizeof( float ) * nFlexWeightCount );
					}
					return;
				}
			}
		}
	}
	*/

	BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );
	return;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static void cc_dump_particlemanifest()
{
	Msg("Dumping particle list:\n");
	for ( int i = 0; i < g_pParticleSystemMgr->GetParticleSystemCount(); i++ )
	{
		const char *pParticleSystemName = g_pParticleSystemMgr->GetParticleSystemNameFromIndex(i);
		Msg(" %d: %s\n", i, pParticleSystemName );
	}
}
static ConCommand dump_particlemanifest( "dump_particlemanifest", cc_dump_particlemanifest, "Dump the list of particles loaded.", FCVAR_CHEAT );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::SetParticleSystemsVisible( bool bVisible )
{
	if ( bVisible == m_bParticleSystemsCreated )
		return;

	if ( !HasCustomParticleSystems() )
		return;

	int iSystems = m_AttributeManager.GetItem()->GetStaticData()->GetNumAttachedParticles();
	for ( int i = 0; i < iSystems; i++ )
	{
		attachedparticlesystem_t *pSystem = m_AttributeManager.GetItem()->GetStaticData()->GetAttachedParticleData( i );
		Assert( pSystem->pszSystemName && pSystem->pszSystemName[0] );

		// Ignore custom particles. Weapons handle them in custom fashions.
		if ( pSystem->iCustomType )
			continue;

		UpdateSingleParticleSystem( bVisible, pSystem );
	}

	// Now check for particle systems that controlled on by specific attributes
	int iCustomParticleEffect = GetCustomParticleEffectId( this );
	if ( iCustomParticleEffect > 0 )
	{
		attachedparticlesystem_t *pSystem = GetItemSchema()->GetAttributeControlledParticleSystem( iCustomParticleEffect );
		if ( pSystem )
		{
			iCustomParticleEffect--;

			if ( pSystem->iCount > 0 )
			{
				// Support X particles attached to X attachment points
				for ( int i = 0; i < pSystem->iCount; i++ )
				{
					UpdateSingleParticleSystem( bVisible, pSystem, UTIL_VarArgs("%s%d", pSystem->pszAttachmentName, i+1 ) );
				}
			}
			else
			{
				UpdateSingleParticleSystem( bVisible, pSystem );
			}
		}
	}

	m_bParticleSystemsCreated = bVisible;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconEntity::UpdateSingleParticleSystem( bool bVisible, attachedparticlesystem_t *pSystem, const char *pszAttachmentName )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return;

	if ( !pszAttachmentName )
	{
		pszAttachmentName = pSystem->pszAttachmentName;
	}

	C_BaseEntity *pEffectOwner = this;
	if ( pSystem->nAttachToEntity == ATTPART_TO_PARENT )
	{
		pEffectOwner = GetOwnerEntity();
	}

	if ( bVisible && pEffectOwner )
	{
		// We can't have fastcull on if we want particles attached to us
		if ( pEffectOwner == this )
		{
			RemoveEffects( EF_BONEMERGE_FASTCULL );
		}

#ifdef DOTA_DLL
		C_DOTA_BaseNPC *pNPC = ToDOTABaseNPC( pEffectOwner );
		if ( pNPC )
			pNPC->CopyParticlesToPortrait( true );

		bool bAttachTypeSet = (pSystem->nRootAttachType != 1);
		int iIndex = -1;

		bool bAttached = false;
		if ( pszAttachmentName && pszAttachmentName[0] && pEffectOwner->GetBaseAnimating() )
		{
			int iAttachment = pEffectOwner->GetBaseAnimating()->LookupAttachment( pszAttachmentName );
			if ( iAttachment != INVALID_PARTICLE_ATTACHMENT )
			{
				iIndex = GetParticleManager()->CreateParticle( GetParticleManager()->GetParticleReplacement( pSystem->pszSystemName, GetOwnerEntity() ), bAttachTypeSet ? pSystem->nRootAttachType : PATTACH_POINT_FOLLOW, pEffectOwner );
				m_vecAttachedParticles.AddToTail( iIndex );
				bAttached = true;
			}
		}

		// Attachments can fall back to following root bones if the attachment point wasn't found
		if ( !bAttached )
		{
			if ( bAttachTypeSet )
			{
				iIndex = GetParticleManager()->CreateParticle( GetParticleManager()->GetParticleReplacement( pSystem->pszSystemName, GetOwnerEntity() ), pSystem->nRootAttachType, pEffectOwner );
				m_vecAttachedParticles.AddToTail( iIndex );
			}
			else
			{
				if ( pSystem->bFollowRootBone )
				{
					iIndex = GetParticleManager()->CreateParticle( GetParticleManager()->GetParticleReplacement( pSystem->pszSystemName, GetOwnerEntity() ), PATTACH_ROOTBONE_FOLLOW, pEffectOwner );
				}
				else
				{
					iIndex = GetParticleManager()->CreateParticle( GetParticleManager()->GetParticleReplacement( pSystem->pszSystemName, GetOwnerEntity() ), PATTACH_ABSORIGIN_FOLLOW, pEffectOwner );
				}
				m_vecAttachedParticles.AddToTail( iIndex );
			}
		}
		// Now init any control points
		if ( iIndex != -1 )
		{
			FOR_EACH_VEC( pSystem->vecControlPoints, i )
			{
				attachedparticlecontrolpoint_t *pCP = &pSystem->vecControlPoints[i];
				if ( pCP->nAttachType == PATTACH_CUSTOMORIGIN || pCP->nAttachType == PATTACH_WORLDORIGIN )
					GetParticleManager()->SetParticleControl( iIndex, pCP->nControlPoint, pCP->vecPosition );
				else
					GetParticleManager()->SetParticleControlEnt( iIndex, pCP->nControlPoint, pEffectOwner, pCP->nAttachType, pCP->pszAttachmentName );
			}

			// If we're a Strange Type, set CP 13 to our current value
			CEconItemView *pItem = GetAttributeContainer()->GetItem();

			if ( pItem )
			{
				
				static CSchemaAttributeDefHandle pAttrDef_KillEaterAttribute( "kill eater" );
				uint32 unKillEater;
				if ( pItem->FindAttribute( pAttrDef_KillEaterAttribute, &unKillEater ) )
				{
					GetParticleManager()->SetParticleControl( iIndex, 13, Vector( unKillEater, 1, 1 ) );
				}
				const CDOTAItemDefinition *pDef = pItem->GetStaticData();

				for ( int nCPIndex = 0; nCPIndex < pDef->GetNumParticleControlPoints(); ++nCPIndex )
				{
					int nWhichCP;
					Vector vecCPValue;
					bool bHasCP = pDef->GetReplacementControlPoint( nCPIndex, GetParticleManager()->GetParticleReplacement( pSystem->pszSystemName, GetOwnerEntity() ), nWhichCP, vecCPValue );
					if ( bHasCP )
					{
						GetParticleManager()->SetParticleControl( iIndex, nWhichCP, vecCPValue );
					}
				}
			}
		}



		if ( pNPC )
			pNPC->CopyParticlesToPortrait( false );
			
#endif //#ifdef DOTA_DLL
	}
	else
	{
#ifdef DOTA_DLL
		FOR_EACH_VEC( m_vecAttachedParticles, i )
		{
			GetParticleManager()->DestroyParticleEffect( m_vecAttachedParticles[i], true );
		}
		m_vecAttachedParticles.RemoveAll();
#endif //#ifdef DOTA_DLL
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels )
{
	m_bClientside = true;
	return BaseClass::InitializeAsClientEntity( pszModelName, bRenderWithViewModels );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CEconEntity::InternalDrawModel( int flags, const RenderableInstance_t &instance )
{
	bool bUseOverride = m_MaterialOverrides.IsValid();
	if ( bUseOverride && (flags & STUDIO_RENDER) )
	{
		modelrender->ForcedMaterialOverride( m_MaterialOverrides );
	}

	int ret = BaseClass::InternalDrawModel( flags, instance );

	if ( bUseOverride && (flags & STUDIO_RENDER) )
	{
		modelrender->ForcedMaterialOverride( NULL );
	}

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::ViewModel_IsTransparent( void )
{
	if ( m_hViewmodelAttachment != NULL && m_hViewmodelAttachment->IsTransparent() )
	{
		return true;
	}
	return IsTransparent();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::ViewModel_IsUsingFBTexture( void )
{
// 	if ( m_hViewmodelAttachment != NULL && m_hViewmodelAttachment->UsesPowerOfTwoFrameBufferTexture() )
// 	{
// 		return true;
// 	}
// 	return UsesPowerOfTwoFrameBufferTexture();

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::IsOverridingViewmodel( void )
{
	bool bUseOverride = m_MaterialOverrides.IsValid();
	bUseOverride = bUseOverride || (m_hViewmodelAttachment != NULL) || ( m_AttributeManager.GetItem()->GetStaticData()->GetNumAttachedModels() > 0 );
	return bUseOverride;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CEconEntity::DrawOverriddenViewmodel( C_BaseViewModel *pViewmodel, int flags )
{
	int ret = 0;
#ifdef TF_DLL
	bool bIsAttachmentTranslucent = m_hViewmodelAttachment.Get() ? m_hViewmodelAttachment->IsTransparent() : false;
	bool bUseOverride = false;
	
	CEconItemView *pItem = GetAttributeContainer()->GetItem();
	bool bAttachesToHands = ( pItem->IsValid() && (pItem->GetStaticData()->ShouldAttachToHands() || pItem->GetStaticData()->ShouldAttachToHandsVMOnly()));

	// If the attachment is translucent, we need to render the viewmodel first
	if ( bIsAttachmentTranslucent )
	{
		ret = pViewmodel->DrawOverriddenViewmodel( flags );
	}

	if ( flags & STUDIO_RENDER )
	{
		bUseOverride = m_MaterialOverrides.IsValid();
		if ( bUseOverride )
		{
			modelrender->ForcedMaterialOverride( m_MaterialOverrides );
		}
	
		if ( m_hViewmodelAttachment )
		{
			m_hViewmodelAttachment->RemoveEffects( EF_NODRAW );
			m_hViewmodelAttachment->DrawModel( flags );
			m_hViewmodelAttachment->AddEffects( EF_NODRAW );
		}

		// if we are attached to the hands, then we DO NOT want have an override material when we draw our view model
		if ( bAttachesToHands && bUseOverride )
		{
			modelrender->ForcedMaterialOverride( NULL );
			bUseOverride = false;
		}
	}

	if ( !bIsAttachmentTranslucent )
	{
		ret = pViewmodel->DrawOverriddenViewmodel( flags );
	}

	if ( bUseOverride )
	{
		modelrender->ForcedMaterialOverride( NULL );
	}
#endif // #ifdef TF_DLL
	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::OnInternalDrawModel( ClientModelRenderInfo_t *pInfo )
{
	if ( !BaseClass::OnInternalDrawModel( pInfo ) )
		return false;

	DrawEconEntityAttachedModels( this, this, pInfo, kAttachedModelDisplayFlag_WorldModel );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CEconEntity::LookupAttachment( const char *pAttachmentName )
{
	if ( m_hViewmodelAttachment )
		return m_hViewmodelAttachment->LookupAttachment( pAttachmentName );

	return BaseClass::LookupAttachment( pAttachmentName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::GetAttachment( int number, matrix3x4_t &matrix )
{
	if ( m_hViewmodelAttachment )
		return m_hViewmodelAttachment->GetAttachment( number, matrix );

	return BaseClass::GetAttachment( number, matrix );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::GetAttachment( int number, Vector &origin )
{
	if ( m_hViewmodelAttachment )
		return m_hViewmodelAttachment->GetAttachment( number, origin );

	return BaseClass::GetAttachment( number, origin );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	if ( m_hViewmodelAttachment )
		return m_hViewmodelAttachment->GetAttachment( number, origin, angles );

	return BaseClass::GetAttachment( number, origin, angles );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
{
	if ( m_hViewmodelAttachment )
		return m_hViewmodelAttachment->GetAttachmentVelocity( number, originVel, angleVel );

	return BaseClass::GetAttachmentVelocity( number, originVel, angleVel );
}

#endif // #if defined( CLIENT_DLL )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconEntity::HasCustomParticleSystems( void )
{
	if ( m_AttributeManager.GetItem()->GetStaticData()->GetNumAttachedParticles() )
		return true;

	int iCustomParticleEffect = GetCustomParticleEffectId( this );

	return ( iCustomParticleEffect > 0 && GetItemSchema()->GetAttributeControlledParticleSystem( iCustomParticleEffect ) != NULL );
}

//-----------------------------------------------------------------------------
// Purpose: Hides or shows masked bodygroups associated with this item.
//-----------------------------------------------------------------------------
bool CEconEntity::UpdateBodygroups( CBaseCombatCharacter* pOwner, int iState )
{
	if ( !pOwner )
		return false;

	CAttributeContainer *pCont = GetAttributeContainer();
	if ( !pCont )
		return false;

	CEconItemView *pItem = pCont->GetItem();
	if ( !pItem )
		return false;

	const CEconItemDefinition *pItemDef = pItem->GetItemDefinition();
	if ( !pItemDef )
		return false;

	int iNumBodyGroups = pItemDef->GetNumModifiedBodyGroups();
	for ( int i=0; i<iNumBodyGroups; ++i )
	{
		int iBody = 0;
		const char *pszBodyGroup = pItemDef->GetModifiedBodyGroup( i, iBody );
		int iBodyGroup = pOwner->FindBodygroupByName( pszBodyGroup );

		if ( iBodyGroup == -1 )
			continue;

		pOwner->SetBodygroup( iBodyGroup, pItemDef->IsBaseItem() ? 0 : iState );
	}

	// Handle per-style bodygroup hiding
	const CEconStyleInfo *pStyle = pItemDef->GetStyleInfo( pItem->GetStyle() );
	if ( pStyle )
	{
		FOR_EACH_VEC( pStyle->GetAdditionalHideBodygroups(), i )
		{
			int iBodyGroup = pOwner->FindBodygroupByName( pStyle->GetAdditionalHideBodygroups()[i] );

			if ( iBodyGroup == -1 )
				continue;

			pOwner->SetBodygroup( iBodyGroup, iState );
		}
	}

	// Handle world model bodygroup overrides
	int iBodyOverride = pItemDef->GetWorldmodelBodygroupOverride();
	int iBodyStateOverride = pItemDef->GetWorldmodelBodygroupStateOverride();
	if ( iBodyOverride > -1 && iBodyStateOverride > -1 )
	{
		pOwner->SetBodygroup( iBodyOverride, iBodyStateOverride );
	}

	// Handle view model bodygroup overrides
	iBodyOverride = pItemDef->GetViewmodelBodygroupOverride();
	iBodyStateOverride = pItemDef->GetViewmodelBodygroupStateOverride();
	if ( iBodyOverride > -1 && iBodyStateOverride > -1 )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pOwner );
		if ( pPlayer )
		{
			CBaseViewModel *pVM = pPlayer->GetViewModel();
			if ( pVM )
			{
				pVM->SetBodygroup( iBodyOverride, iBodyStateOverride );
			}
		}
	}
	 
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseAttributableItem::CBaseAttributableItem()
{
}

loadout_positions_t CEconEntity::GetLoadoutPosition( int iTeam /*= 0 */ ) const
{
	return ( loadout_positions_t ) GetAttributeContainer()->GetItem()->GetStaticData()->GetLoadoutSlot( iTeam );
}

const CEconItemView* CEconEntity::GetEconItemView( void ) const
{
	return m_AttributeManager.GetItem();
}
