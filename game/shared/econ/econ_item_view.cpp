//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "econ_item_view.h"
#include "econ_item_system.h"
#include "econ_item_description.h"
#include "econ_item_inventory.h"

#include "econ_gcmessages.h"

// For localization
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"
#include "tier2/p4helpers.h"
#include "p4lib/ip4.h"

#include "imageutils.h"

#if defined(CLIENT_DLL) || defined(GAME_DLL)
#include "isaverestore.h"
#include "dt_utlvector_send.h"
#include "dt_utlvector_recv.h"
#endif

#ifdef CLIENT_DLL
#ifndef DEDICATED
#include "vgui_controls/Panel.h"
#include "vgui/IScheme.h"
#endif
#endif

#if defined(TF_CLIENT_DLL)
#include "tf_duel_summary.h"
#include "econ_contribution.h"
#include "tf_player_info.h"
#include "tf_gcmessages.h"
#include "c_tf_freeaccount.h"
#endif

#if defined(DOTA_DLL)
#include "dota_sharedfuncs.h"
#endif

#include "activitylist.h"

#if defined(CSTRIKE_CLIENT_DLL)
#include "materialsystem/icompositetexturegenerator.h"
#include "materialsystem/icustommaterialmanager.h"
#include "mathlib/camera.h"
#include "tier3/mdlutils.h"
#include "irendertorthelperobject.h"

#include "cs_weapon_parse.h"
#include "cs_custom_weapon_visualsdata_processor.h"
#include "cs_custom_clothing_visualsdata_processor.h"
#include "cs_custom_epidermis_visualsdata_processor.h"
#include "cs_custom_embroider_visualsdata_processor.h"
#include "cs_custom_texture_saver.h"
#include "materialsystem/imaterialvar.h"
#endif //#if defined(CSTRIKE_CLIENT_DLL)

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ECON_ITEM_ICON_CACHE_VERSION 0x10

// Dynamic attributes in the the Econ_Item SOCache need to be networked for demo recording!
#define ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS


#if defined(CLIENT_DLL) || defined(GAME_DLL)
// Networking tables for attributes
BEGIN_NETWORK_TABLE_NOBASE( CEconItemAttribute, DT_ScriptCreatedAttribute )

	// Note: we are networking the value as an int, even though it's a "float", because really it isn't
	// a float.  It's 32 raw bits.

#ifndef CLIENT_DLL
	SendPropInt( SENDINFO(m_iAttributeDefinitionIndex), -1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO_NAME(m_flValue, m_iRawValue32), 32, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO_NAME(m_flInitialValue, m_iRawInitialValue32), 32, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_nRefundableCurrency), -1, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO(m_bSetBonus) ),
#else
	RecvPropInt( RECVINFO(m_iAttributeDefinitionIndex) ),
	RecvPropInt( RECVINFO_NAME(m_flValue, m_iRawValue32) ),
	RecvPropFloat( RECVINFO(m_flValue), SPROP_NOSCALE ), // for demo compatibility only
	RecvPropInt( RECVINFO_NAME(m_flInitialValue, m_iRawInitialValue32) ),
RecvPropInt( RECVINFO( m_nRefundableCurrency ) ),
RecvPropBool( RECVINFO( m_bSetBonus ) ),
#endif
END_NETWORK_TABLE()
#endif

#if defined(CSTRIKE_CLIENT_DLL)
CCSCustomTextureSaver g_Generated_Texture_Saver;
#endif //#if defined(CSTRIKE_CLIENT_DLL) 

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemAttribute::CEconItemAttribute( void )
{
 	Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemAttribute::Init( void )
{
	m_iAttributeDefinitionIndex = (attrib_definition_index_t)-1;
	m_flValue = 0.0f;

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	m_flInitialValue = 0;
	m_nRefundableCurrency = 0;

	m_bSetBonus = false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemAttribute & CEconItemAttribute::operator=( const CEconItemAttribute &val )
{
	m_iAttributeDefinitionIndex = val.m_iAttributeDefinitionIndex;
	m_flValue = val.m_flValue;

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	m_flInitialValue = m_flValue;

	m_bSetBonus = val.m_bSetBonus;
#endif
	return *this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemAttribute::CEconItemAttribute( const attrib_definition_index_t iAttributeIndex, float flValue )
{
	Init();

	m_iAttributeDefinitionIndex = iAttributeIndex;
	SetValue( flValue );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	m_flInitialValue = m_flValue;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemAttribute::CEconItemAttribute( const attrib_definition_index_t iAttributeIndex, uint32 unValue )
{
	Init();

	m_iAttributeDefinitionIndex = iAttributeIndex;
	SetIntValue( unValue );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	m_flInitialValue = m_flValue;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemAttribute::SetValue( float flValue )
{
//	Assert( GetStaticData() && GetStaticData()->IsStoredAsFloat() );
	m_flValue = flValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CEconItemAttribute::GetValue( void ) const
{
//	Assert( GetStaticData() && GetStaticData()->IsStoredAsFloat() );
	return m_flValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemAttribute::SetIntValue( uint32 unValue )
{
	// @note we don't check the storage type here, because this is how it is set from the data file
	// Note that numbers approaching two billion cannot be stored in a float
	// representation because they will map to NaNs. Numbers below 16 million
	// will fail if denormals are disabled.
	m_flValue = *(float*)&unValue;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint32 CEconItemAttribute::GetIntValue( void ) const
{
//	Assert( GetStaticData() && GetStaticData()->IsStoredAsInteger() );
	return *(uint32*)&m_flValue;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const CEconItemAttributeDefinition *CEconItemAttribute::GetStaticData( void ) const
{ 
	return GetItemSchema()->GetAttributeDefinition( m_iAttributeDefinitionIndex ); 
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)

#if defined( ECON_NETWORK_ATTRIBUTES ) || defined( ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS )
BEGIN_NETWORK_TABLE_NOBASE( CAttributeList, DT_AttributeList )
#if !defined( CLIENT_DLL )
SendPropUtlVectorDataTable( m_Attributes, MAX_ATTRIBUTES_PER_ITEM, DT_ScriptCreatedAttribute ),
#else
RecvPropUtlVectorDataTable( m_Attributes, MAX_ATTRIBUTES_PER_ITEM, DT_ScriptCreatedAttribute ),
#endif // CLIENT_DLL
END_NETWORK_TABLE()
#endif // #ifdef defined( ECON_NETWORK_ATTRIBUTES ) || defined( ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS )

BEGIN_DATADESC_NO_BASE( CAttributeList )
END_DATADESC()
#endif // #if defined(CLIENT_DLL) || defined(GAME_DLL)

#if defined(CLIENT_DLL)
bool CEconItemView::m_sbHasCleanedInventoryImageCacheDir = false;
#endif //#if defined(CLIENT_DLL)

//===========================================================================================================================
// SCRIPT CREATED ITEMS
//===========================================================================================================================
#if defined(CLIENT_DLL) || defined(GAME_DLL)
BEGIN_NETWORK_TABLE_NOBASE( CEconItemView, DT_ScriptCreatedItem )
#if !defined( CLIENT_DLL )
	SendPropInt( SENDINFO( m_iItemDefinitionIndex ), 20, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iEntityLevel ), 8 ),
	//SendPropInt( SENDINFO( m_iItemID ), 64, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iItemIDHigh ), 32, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iItemIDLow ), 32, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iAccountID ), 32, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_iEntityQuality ), 5 ),
	SendPropBool( SENDINFO( m_bInitialized ) ),
	SendPropString( SENDINFO( m_szCustomName ) ),
#ifdef ECON_NETWORK_ATTRIBUTES
	SendPropDataTable(SENDINFO_DT(m_AttributeList), &REFERENCE_SEND_TABLE(DT_AttributeList)),
#endif // #ifdef ECON_NETWORK_ATTRIBUTES
#ifdef ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS
	SendPropDataTable(SENDINFO_DT(m_NetworkedDynamicAttributesForDemos), &REFERENCE_SEND_TABLE(DT_AttributeList)),
#endif // #ifdef ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS
#else
	RecvPropInt( RECVINFO( m_iItemDefinitionIndex ) ),
	RecvPropInt( RECVINFO( m_iEntityLevel ) ),
	//RecvPropInt( RECVINFO( m_iItemID ) ),
	RecvPropInt( RECVINFO( m_iItemIDHigh ) ),
	RecvPropInt( RECVINFO( m_iItemIDLow ) ),
	RecvPropInt( RECVINFO( m_iAccountID ) ),
	RecvPropInt( RECVINFO( m_iEntityQuality ) ),
	RecvPropBool( RECVINFO( m_bInitialized ) ),
	RecvPropString( RECVINFO( m_szCustomName ) ),
#ifdef ECON_NETWORK_ATTRIBUTES
	RecvPropDataTable(RECVINFO_DT(m_AttributeList), 0, &REFERENCE_RECV_TABLE(DT_AttributeList)),
#endif // #ifdef ECON_NETWORK_ATTRIBUTES
#ifdef ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS
	RecvPropDataTable(RECVINFO_DT(m_NetworkedDynamicAttributesForDemos), 0, &REFERENCE_RECV_TABLE(DT_AttributeList)),
#endif // #ifdef ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS
#endif // CLIENT_DLL
END_NETWORK_TABLE()

BEGIN_DATADESC_NO_BASE( CEconItemView )
	DEFINE_FIELD( m_iItemDefinitionIndex, FIELD_SHORT ),
	DEFINE_FIELD( m_iEntityQuality, FIELD_INTEGER ),
	DEFINE_FIELD( m_iEntityLevel, FIELD_INTEGER ),
	DEFINE_FIELD( m_iItemID, FIELD_INTEGER ),
	//	DEFINE_FIELD( m_wszItemName, FIELD_STRING ),		Regenerated post-save
	//	DEFINE_FIELD( m_szItemName, FIELD_STRING ),		Regenerated post-save
	//	DEFINE_FIELD( m_szAttributeDescription, FIELD_STRING ),		Regenerated post-save
	//  m_AttributeLineColors	// Regenerated post-save
	//  m_Attributes			// Custom handling in Save()/Restore()
	DEFINE_FIELD( m_bInitialized, FIELD_BOOLEAN ),
	DEFINE_EMBEDDED( m_AttributeList ),
END_DATADESC()
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView::CEconItemView( void )
{
	m_iItemDefinitionIndex = 0;
	m_iEntityQuality = (int)AE_UNDEFINED;
	m_iEntityLevel = 0;
	SetItemID(0);
	m_iInventoryPosition = 0;
	m_bInitialized = false;
	m_iAccountID = 0;
	m_pNonSOEconItem = NULL;
#if defined( CLIENT_DLL )
	m_bIsTradeItem = false;
	m_iEntityQuantity = 1;
	m_iRarityOverride = -1;
	m_iQualityOverride = 0;
	m_unClientFlags = 0;
	m_unOverrideStyle = INVALID_STYLE_INDEX;
	m_bInventoryImageRgbaRequested = false;
	m_bInventoryImageTriedCache = false;
	m_nInventoryImageRgbaWidth = 0;
	m_nInventoryImageRgbaHeight = 0;
	m_pImageReadyCallback = NULL;
	m_pRenderToRTData = NULL;
	m_pScratchVTF = NULL;
	m_pRenderToRTMDL = NULL;
	m_hAsyncControl = NULL;
	m_asyncFixupState = AFS_Init;
#endif
	m_szCustomNameOverride[ 0 ] = '\0';

	m_bKillEaterTypesCached = false;
	m_nKillEaterValuesCacheFrame = -2;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView::~CEconItemView( void )
{
#ifdef CLIENT_DLL
	// don't want to call this during destruction
	m_pImageReadyCallback = NULL;

	g_InventoryItemUpdateManager.RemoveItemViewFromFixupList( this );

	if ( InventoryManager() )
	{
		InventoryManager()->OnDestroyEconItemView( this );
	}

	if ( m_asyncFixupState == AFS_LoadingInProgress )
	{
		if ( filesystem && m_hAsyncControl )
		{
			filesystem->AsyncAbort( m_hAsyncControl );
			filesystem->AsyncRelease( m_hAsyncControl );
		}
		m_hAsyncControl = NULL;
		m_asyncFixupState = AFS_Init;
	}
#endif

	DestroyAllAttributes();

	Cleanup();
}

void CEconItemView::Cleanup( void )
{
	MarkDescriptionDirty();

#if defined( CSTRIKE_CLIENT_DLL )
	for ( int i = 0; i < m_ppVisualsDataProcessors.Count(); i++ )
	{
		if ( m_ppVisualsDataProcessors[ i ]  != NULL )
		{
			m_ppVisualsDataProcessors[ i ]->Release();
			m_ppVisualsDataProcessors[ i ] = NULL;
		}
	}
	m_ppVisualsDataProcessors.RemoveAll();

	if ( m_pRenderToRTData != NULL )
	{
		g_pRenderToRTHelper->DestroyRenderToRTData( m_pRenderToRTData );
		m_pRenderToRTData = NULL;
	}
	if ( m_pRenderToRTMDL != NULL )
	{
		delete m_pRenderToRTMDL;
		m_pRenderToRTMDL = NULL;
	}
	if ( m_pScratchVTF != NULL )
	{
		DestroyVTFTexture( m_pScratchVTF );
		m_pScratchVTF = NULL;
	}

	m_pStickerMaterials.RemoveAll();

#endif //#if defined( CSTRIKE_CLIENT_DLL )

	if ( m_pNonSOEconItem )
	{
		delete m_pNonSOEconItem;
		m_pNonSOEconItem = NULL;		
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView::CEconItemView( const CEconItemView &src )
{
	m_pNonSOEconItem = NULL;
#if defined( CSTRIKE_CLIENT_DLL )
	m_bInventoryImageRgbaRequested = false;
	m_bInventoryImageTriedCache = false;
	m_pImageReadyCallback = NULL;
	m_pRenderToRTData = NULL;
	m_pScratchVTF = NULL;
	m_pRenderToRTMDL = NULL;
	m_hAsyncControl = NULL;
	m_asyncFixupState = AFS_Init;
#endif

	*this = src;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::Init( int iDefIndex, int iQuality, int iLevel, uint32 iAccountID )
{
	m_AttributeList.Init();
	m_NetworkedDynamicAttributesForDemos.Init();

	m_szCustomName.GetForModify()[0] = '\0';

	m_iItemDefinitionIndex = iDefIndex;
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
	{
		// We've got an item that we don't have static data for.
		return;
	}

	SetItemID(0);
	m_bInitialized = true;
	m_iAccountID = iAccountID;

	if ( iQuality == AE_USE_SCRIPT_VALUE )
	{
		m_iEntityQuality = pData->GetQuality();

		// Kyle says: this is a horrible hack because AE_UNDEFINED will get stuffed into a uint8 when
		// loaded into the item definition, but then read back out into a regular int here.
		if ( m_iEntityQuality == (uint8)AE_UNDEFINED )
		{
			m_iEntityQuality = (int)AE_NORMAL;
		}
	}
	else if ( iQuality == k_unItemQuality_Any )
	{
		m_iEntityQuality = (int) AE_UNIQUE;
	}
	else
	{
		m_iEntityQuality = iQuality;
	}

	if ( iLevel == AE_USE_SCRIPT_VALUE )
	{
		m_iEntityLevel = pData->RollItemLevel();
	}
	else
	{
		m_iEntityLevel = iLevel;
	}

	// We made changes to quality, level, etc. so mark the description as dirty.
	MarkDescriptionDirty();
}

bool CEconItemView::Init( CEconItem* pItem )
{
	Init( pItem->GetDefinitionIndex(), pItem->GetQuality(), pItem->GetItemLevel(), pItem->GetAccountID() );
	if ( !IsValid() )
	{
		return false;
	}

	SetItemID( pItem->GetItemID() );
	SetInventoryPosition( pItem->GetInventoryToken() );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView& CEconItemView::operator=( const CEconItemView& src )
{
	Cleanup();

	m_iItemDefinitionIndex = src.m_iItemDefinitionIndex;
	m_iEntityQuality = src.m_iEntityQuality;
	m_iEntityLevel = src.m_iEntityLevel;
	SetItemID( src.GetItemID() );
	m_iInventoryPosition = src.m_iInventoryPosition;
	m_bInitialized = src.m_bInitialized;
	m_iAccountID = src.m_iAccountID;
	m_pNonSOEconItem = src.m_pNonSOEconItem;
#if defined( CLIENT_DLL )
	m_bIsTradeItem = src.m_bIsTradeItem;
	m_iEntityQuantity = src.m_iEntityQuantity;
	m_iRarityOverride = src.m_iRarityOverride;
	m_iQualityOverride = src.m_iQualityOverride;
	m_unClientFlags = src.m_unClientFlags;
	m_unOverrideStyle = src.m_unOverrideStyle;
#endif //#if defined( CLIENT_DLL )

#if defined( CSTRIKE_CLIENT_DLL )
	if ( src.m_bInventoryImageRgbaRequested )
	{
		m_bInventoryImageRgbaRequested = src.m_bInventoryImageRgbaRequested;
		m_bInventoryImageTriedCache = src.m_bInventoryImageTriedCache;
		m_nInventoryImageRgbaWidth = src.m_nInventoryImageRgbaWidth;
		m_nInventoryImageRgbaHeight = src.m_nInventoryImageRgbaHeight;
		m_pImageReadyCallback = src.m_pImageReadyCallback;
		if ( src.m_inventoryImageRgba.TellPut() > 0 )
		{
			m_inventoryImageRgba.EnsureCapacity( src.m_inventoryImageRgba.TellPut() );
			m_inventoryImageRgba.CopyBuffer( src.m_inventoryImageRgba );
		}
		else
		{
			m_inventoryImageRgba.Purge();
		}
	}
	else
	{
		m_nInventoryImageRgbaWidth = 0;
		m_nInventoryImageRgbaHeight = 0;
		m_bInventoryImageRgbaRequested = false;
		m_bInventoryImageTriedCache = false;
		m_inventoryImageRgba.Purge();
		m_pImageReadyCallback = NULL;
	}

	src.DuplicateCustomMaterialsToOther( this );

	// copy and addref processors from src
	for ( int i = 0; i < src.m_ppVisualsDataProcessors.Count(); i++ )
	{
		if ( src.m_ppVisualsDataProcessors[ i ]  != NULL )
		{
			m_ppVisualsDataProcessors.AddToTail( src.m_ppVisualsDataProcessors[ i ] );
			src.m_ppVisualsDataProcessors[ i ]->AddRef();
		}
	}
#endif //#if defined( CSTRIKE_CLIENT_DLL )

	DestroyAllAttributes();

	m_AttributeList = src.m_AttributeList;
	m_NetworkedDynamicAttributesForDemos = src.m_NetworkedDynamicAttributesForDemos;

	if ( src.m_szCustomNameOverride[0] != '\0' )
	{
		V_strncpy(m_szCustomNameOverride, src.m_szCustomNameOverride, sizeof( m_szCustomNameOverride ) );
	}

	V_strncpy( m_szCustomName.GetForModify(), src.m_szCustomName.Get(), MAX_ITEM_CUSTOM_NAME_DATABASE_SIZE );

	return *this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CEconItemView::operator==( const CEconItemView &other ) const
{
	if ( IsValid() != other.IsValid() )
		return false;
	if ( (GetItemID() > 0 || other.GetItemID() > 0) && GetItemID() != other.GetItemID() )
		return false;
	if ( GetItemIndex() != other.GetItemIndex() )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const GameItemDefinition_t *CEconItemView::GetStaticData( void ) const
{ 
	const CEconItemDefinition	 *pRet		= GetItemSchema()->GetItemDefinition( m_iItemDefinitionIndex );
	const GameItemDefinition_t *pTypedRet = dynamic_cast<const GameItemDefinition_t *>( pRet );

	AssertMsg( pRet == pTypedRet, "Item definition of inappropriate type." );

	return pTypedRet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int32 CEconItemView::GetQuality() const
{
#if defined( CLIENT_DLL )
	if ( m_iQualityOverride )
		return m_iQualityOverride;
#endif
	return GetSOCData()
		 ? GetSOCData()->GetQuality()
#ifdef TF_CLIENT_DLL
		 : GetFlags() & kEconItemFlagClient_StoreItem
		 ? AE_UNIQUE
#endif
		 : ( ( m_iEntityQuality.Get() > int( AE_NORMAL ) ) ? m_iEntityQuality.Get() : int( AE_NORMAL ) );
}

int32 CEconItemView::GetRarity() const
{
#if defined( CLIENT_DLL )
	if ( m_iRarityOverride > -1 )
		return m_iRarityOverride;
#endif
	if ( GetSOCData() && ( GetSOCData()->GetRarity() != k_unItemRarity_Any ) )
	{
		return GetSOCData()->GetRarity();
	}
	else
	{
		int nRarity = GetItemDefinition()->GetRarity();

		const CPaintKit *pPaintKit = GetCustomPaintKit();
		if ( pPaintKit && pPaintKit->nID != 0 )
		{
			nRarity = EconRarity_CombinedItemAndPaintRarity( nRarity, pPaintKit->nRarity );
		}
		else if ( GetItemDefinition()->GetDefinitionIndex() < SCHEMA_BASE_ITEM_MAX )
		{
			nRarity = 0; // Stock
		}

		return nRarity;
	}
	// TODO: This should probably return EconRarity_CombinedItemAndPaintRarity( pItemDef->GetRarity(), pPaintKit->nRarity )
	// but there is no known case where the absence of paint consideration is a problem.
	// If it ain't broken, doth one fixeth?
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
style_index_t CEconItemView::GetStyle() const
{
	return GetItemStyle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint8 CEconItemView::GetFlags() const
{
	uint8 unSOCFlags = GetSOCData() ? GetSOCData()->GetFlags() : 0;

#if !defined( GAME_DLL )
	return unSOCFlags | m_unClientFlags;
#else // defined( GAME_DLL )
	return unSOCFlags;
#endif // !defined( GAME_DLL )
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
eEconItemOrigin CEconItemView::GetOrigin() const
{
	return GetSOCData() ? GetSOCData()->GetOrigin() : kEconItemOrigin_Invalid;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint16 CEconItemView::GetQuantity() const
{
	return GetItemQuantity();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetCustomName() const
{
	if ( m_szCustomNameOverride[ 0 ] != '\0' )
	{
		return m_szCustomNameOverride;
	}

	// If we have an so cache for this item, use the attribute in it
	if ( GetSOCData() )
		return GetSOCData()->GetCustomName();
	
	extern ConVar sv_spec_use_tournament_content_standards;

#if defined (CLIENT_DLL)
	// If no SO cache, use anything networked from the server or 
	// null if that string is empty (to support legacy behavior of this function)
	if ( *m_szCustomName )
	{
		if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
		{
			if ( pParameters->m_bAnonymousPlayerIdentity )
				return NULL;
		}

		extern ConVar cl_spec_use_tournament_content_standards;

		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( );

		if ( ( pLocalPlayer->IsSpectator( ) || pLocalPlayer->IsHLTV( ) ) &&
			( sv_spec_use_tournament_content_standards.GetBool( ) || cl_spec_use_tournament_content_standards.GetBool( ) ) )
			return NULL;

		return m_szCustomName;
	}
#endif

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetCustomDesc() const
{
	return GetSOCData() ? GetSOCData()->GetCustomDesc() : NULL;
}

int CEconItemView::GetItemSetIndex() const
{
	return GetSOCData() ? GetSOCData()->GetItemSetIndex() : -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::Invalidate( void )
{
	 m_bInitialized = false;
	 m_iItemDefinitionIndex = m_iItemID = 0;
	 if ( m_pNonSOEconItem )
	 {
		 delete m_pNonSOEconItem;
		 m_pNonSOEconItem = NULL;
	 }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::IterateAttributes( class IEconItemAttributeIterator *pIterator ) const
{
	Assert( pIterator );

	// First, we iterate over the attributes we have local copies of. If we have any attribute
	// values here they'll override whatever values we would otherwise have pulled from our
	// definition/CEconItem.	
	for ( int i = 0; i < GetNumAttributes(); i++ )
	{
		const CEconItemAttribute *pAttrInst = GetAttribute(i);
		Assert( pAttrInst );

		const CEconItemAttributeDefinition *pAttrDef = pAttrInst->GetStaticData();
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );
		Assert( pAttrType->BSupportsGameplayModificationAndNetworking() );
		
		attribute_data_union_t value;
		value.asFloat = pAttrInst->GetValue();

		if ( !pAttrType->OnIterateAttributeValue( pIterator, pAttrDef, value ) )
			return;
	}

	// This wraps any other iterator class and will prevent double iteration of any attributes
	// that exist on us.
	class CEconItemAttributeIterator_EconItemViewWrapper : public IEconItemAttributeIterator
	{
	public:
		CEconItemAttributeIterator_EconItemViewWrapper( const CEconItemView *pEconItemView, IEconItemAttributeIterator *pIterator )
			: m_pEconItemView( pEconItemView )
			, m_pIterator( pIterator )
		{
			Assert( m_pEconItemView );
			Assert( m_pIterator );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, attrib_value_t value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeByDefIndex( pAttrDef->GetDefinitionIndex() )
				 ? true
				 : m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, float value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeByDefIndex( pAttrDef->GetDefinitionIndex() )
				? true
				: m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const CAttribute_String& value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeByDefIndex( pAttrDef->GetDefinitionIndex() )
				 ? true
				 : m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

		virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const Vector& value )
		{
			Assert( pAttrDef );

			return m_pEconItemView->GetAttributeByDefIndex( pAttrDef->GetDefinitionIndex() )
				? true
				: m_pIterator->OnIterateAttributeValue( pAttrDef, value );
		}

	private:
		const CEconItemView *m_pEconItemView;
		IEconItemAttributeIterator *m_pIterator;
	};

	CEconItemAttributeIterator_EconItemViewWrapper iteratorWrapper( this, pIterator );

	// Next, iterate over our database-backed item if we have one... if we do have a DB
	// backing for our item here, that will also feed in the definition attributes.
	if ( GetSOCData() )
	{
		GetSOCData()->IterateAttributes( &iteratorWrapper );
	}
#ifdef ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS
	else if ( GetItemID() > 0 )
	{
		// Since there's no persistent data available, try the networked values!
		for ( int i = 0; i < m_NetworkedDynamicAttributesForDemos.GetNumAttributes(); i++ )
		{
			const CEconItemAttribute *pAttrInst = m_NetworkedDynamicAttributesForDemos.GetAttribute(i);
			Assert( pAttrInst );

			const CEconItemAttributeDefinition *pAttrDef = pAttrInst->GetStaticData();
			if ( !pAttrDef )
				continue;

			const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
			Assert( pAttrType );

			attribute_data_union_t value;
			value.asFloat = pAttrInst->GetValue();

			if ( !pAttrType->OnIterateAttributeValue( pIterator, pAttrDef, value ) )
				return;
		}

		if ( GetStaticData() )
		{
			GetStaticData()->IterateAttributes( &iteratorWrapper );
		}
	}
#endif
	// If we didn't have a DB backing, we can still iterate over our item definition
	// attributes ourselves. This can happen if we're previewing an item in the store, etc.
	else if ( GetStaticData() )
	{
		GetStaticData()->IterateAttributes( &iteratorWrapper );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::EnsureDescriptionIsBuilt() const
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::MarkDescriptionDirty()
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::SetGrayedOutReason( const char *pszGrayedOutReason )
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEconItemView::GetItemQuantity() const
{
	CEconItem *pSOCData = GetSOCData();
	if ( pSOCData )
	{
		return pSOCData->GetQuantity();
	}
#ifdef CLIENT_DLL
	return m_iEntityQuantity;
#else
	return 1;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
style_index_t CEconItemView::GetItemStyle() const
{
#ifdef CLIENT_DLL
	// Are we overriding the backing store style?
	if ( m_unOverrideStyle != INVALID_STYLE_INDEX )
		return m_unOverrideStyle;
#endif // CLIENT_DLL

// 	CEconItem *pSOCData = GetSOCData();
// 	if ( pSOCData )
// 		return pSOCData->GetStyle();

	return INVALID_STYLE_INDEX;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::SetClientItemFlags( uint8 unFlags )
{
	// Generally speaking, we have two uses for client flags:
	//
	//		- we don't have a backing store (a real CEconItem) but want to pretend we do
	//		  for purposes of generating tooltips, graying out icons, etc.
	//
	//		- we may or may not have a backing store but want to shove client-specific
	//		  information into the structure -- things like "this is the item being
	//		  actively previewed", etc.
	//
	// If neither of these two cases is true, then we're going to get unexpected
	// behavior where the GC and the client disagree about the item flags and then
	// Terrible Things happen. We assert to make sure we're in one of the above cases.
	Assert( !GetSOCData() || (unFlags & kEconItemFlags_CheckFlags_AllGCFlags) == 0 );

	m_unClientFlags = unFlags;
	MarkDescriptionDirty();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::SetItemStyleOverride( style_index_t unNewStyleOverride )
{
	// We should only ever override the style on items that don't have a real
	// backing store or we'll start getting disagreements about what the client
	// wants to happen and what's being stored on the GC. Unfortunately we can't
	// assert on this because we do it sometimes when previewing items.
	//Assert( !GetSOCData() );

	m_unOverrideStyle = unNewStyleOverride;
	MarkDescriptionDirty();
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItem *CEconItemView::GetSOCData( void ) const
{
	if ( m_pNonSOEconItem )
		return m_pNonSOEconItem;

#ifdef CLIENT_DLL
	// We need to find the inventory that contains this item. If we're not connected 
	// to a server, and the owner is the same as the local player, use the local inventory.
	// We need to do this for trading, since we are subscribed to the other person's cache.
	if ( !engine->IsInGame() && GetSteamIDFromSOID( InventoryManager()->GetLocalInventory()->GetOwner() ).GetAccountID() == m_iAccountID )
		return InventoryManager()->GetLocalInventory()->GetSOCDataForItem( GetItemID() );
#endif // CLIENT_DLL

	// We're in-game. Find the inventory with our account ID.
	CPlayerInventory *pInventory = InventoryManager()->GetInventoryForAccount( m_iAccountID );
#if defined ( CLIENT_DLL )
	Assert( !pInventory || pInventory == InventoryManager()->GetLocalInventory() );
#endif
	if ( pInventory )
		return pInventory->GetSOCDataForItem( GetItemID() );

	return NULL;
}

itemid_t CEconItemView::GetFauxItemIDFromDefinitionIndex( void ) const
{
	return m_iItemDefinitionIndex
		? CombinedItemIdMakeFromDefIndexAndPaint( m_iItemDefinitionIndex, 0 )
		: 0ull; // invalid items cannot represent a faux ItemID
}

// Purpose: Return the model to use for model panels containing this item
//-----------------------------------------------------------------------------
const char *CEconItemView::GetInventoryModel( void )
{
	if ( !GetStaticData() )
		return NULL;
	return GetStaticData()->GetInventoryModel();
}

bool CEconItemView::IsStickerTool( void )
{
	const GameItemDefinition_t *pStaticData = GetStaticData();
	if ( !pStaticData )
		return false;

	if ( pStaticData->IsTool() && pStaticData->GetEconTool() && ( pStaticData->GetEconTool()->GetCapabilities() & ITEM_CAP_CAN_STICKER ) != 0 )
		return true;

	return false;
}

IMaterial *CEconItemView::GetToolStickerMaterial( void )
{

	if ( !IsStickerTool() )
		return NULL;

	uint32 nStickerId = GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 );
	if ( nStickerId > 0 && GetItemSchema() && GetItemSchema()->GetStickerKitDefinition( nStickerId ) )
	{
		char szDesiredStickerMaterialPath[128];
		V_snprintf( szDesiredStickerMaterialPath, sizeof(szDesiredStickerMaterialPath), 
			"materials/models/weapons/customization/stickers/%s.vmt", 
			GetItemSchema()->GetStickerKitDefinition( nStickerId )->sMaterialPath.String() );

		IMaterial *pMatStickerOverride = materials->FindMaterial( szDesiredStickerMaterialPath, TEXTURE_GROUP_OTHER, false );
		if ( pMatStickerOverride->IsErrorMaterial() )
		{

			KeyValues *pSpecificStickerMaterialKeyValues = new KeyValues( "vmt" );
			KeyValues::AutoDelete autodelete_pSpecificStickerMaterialKeyValues( pSpecificStickerMaterialKeyValues );

			if ( pSpecificStickerMaterialKeyValues->LoadFromFile( g_pFullFileSystem, szDesiredStickerMaterialPath, "GAME" ) )
			{
				//pSpecificStickerMaterialKeyValues->SetString( "$envmap", "Editor/cube_vertigo" );
				//pSpecificStickerMaterialKeyValues->SetString( "$aotexture", "models/weapons/customization/rif_famas/rif_famas_decal_A" );
				//KeyValuesDumpAsDevMsg( pSpecificStickerMaterialKeyValues, 2 );

				pMatStickerOverride = materials->CreateMaterial( szDesiredStickerMaterialPath, pSpecificStickerMaterialKeyValues );
			}

			autodelete_pSpecificStickerMaterialKeyValues.Detach();

			if ( !pMatStickerOverride->IsErrorMaterial() )
				return pMatStickerOverride;

		}

	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Return the image to use for model panels containing this item
//-----------------------------------------------------------------------------
const char *CEconItemView::GetInventoryImage( void ) const
{
	const GameItemDefinition_t *pStaticData = GetStaticData();
	if ( !pStaticData )
		return NULL;

	const CEconStyleInfo *pStyle = GetStaticData()->GetStyleInfo( GetItemStyle() );

	static CSchemaAttributeDefHandle pAttr_AlternateIcon( "alternate icon" );
	uint32 unAlternateIcon = 0;

	extern bool Helper_IsGraphicTool( const CEconItemDefinition * pEconItemDefinition );
	static CSchemaItemDefHandle hItemDefMusicKit( "musickit" );
	static CSchemaItemDefHandle hItemDefMusicKitDefault( "musickit_default" );

#ifdef DOTA_DLL
	if ( pStaticData->GetCapabilities() & ITEM_CAP_USES_ESSENCE )
	{
		// Backwards compatibility to eggs, which were implemented before this feature.
		return pStaticData->GetAlternateIcon( GetEggColor( this ) );
	}
	else if ( FindAttribute( pAttr_AlternateIcon, &unAlternateIcon ) )
	{
		return pStaticData->GetAlternateIcon( unAlternateIcon );
	}
	else if ( pStyle && pStyle->GetIcon() )
	{
		return pStaticData->GetAlternateIcon( pStyle->GetIcon() );
	}
#else
	if ( FindAttribute( pAttr_AlternateIcon, &unAlternateIcon ) )
	{
		return GetItemSchema()->GetAlternateIcon( unAlternateIcon )->GetInventoryImage();
	}
	else if ( pStyle && pStyle->GetIcon() )
	{
		return GetItemSchema()->GetAlternateIcon( pStyle->GetIcon() )->GetInventoryImage();
	}
	else if ( Helper_IsGraphicTool( GetItemDefinition() ) )
	{
		uint32 nStickerId = GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 );
		if ( const CStickerKit *pStickerKitDef = ( ( nStickerId > 0 ) ? GetItemSchema()->GetStickerKitDefinition( nStickerId ) : NULL ) )
		{
			char const *szResult = pStickerKitDef->GetIconURLLarge();

			// exclude code providing a different image for client UI, client will color-multiply on its own
#if SPRAY_TINT_IMAGE_FOR_CLIENT_UI
			static CSchemaAttributeDefHandle pAttr_SprayTintID( "spray tint id" );
			uint32 unSprayTintID = 0;
			if ( pAttr_SprayTintID && FindAttribute( pAttr_SprayTintID, &unSprayTintID ) && unSprayTintID )
			{
				unSprayTintID = CombinedTintIDGetHSVID( unSprayTintID );
				if ( ! ( char * ) m_autoptrInventoryImageGeneratedPath )
					m_autoptrInventoryImageGeneratedPath.reset( new char[ MAX_PATH ] );

				int nImagePathLen = V_strlen( szResult );
				Assert( nImagePathLen + 4 < MAX_PATH );
				if ( ( nImagePathLen > 6 ) && !V_stricmp( szResult + nImagePathLen - 6, "_large" ) )
				{
					V_snprintf( m_autoptrInventoryImageGeneratedPath, MAX_PATH, "%.*s_%u_large", nImagePathLen - 6, szResult, unSprayTintID );
				}
				else
				{
					V_snprintf( m_autoptrInventoryImageGeneratedPath, MAX_PATH, "%s_%u", szResult, unSprayTintID );
				}
				szResult = m_autoptrInventoryImageGeneratedPath;
			}
#endif

			return szResult;
		}
	}
	else if (
		( hItemDefMusicKit && GetItemDefinition()->GetDefinitionIndex() == hItemDefMusicKit->GetDefinitionIndex() ) ||
		( hItemDefMusicKitDefault && GetItemDefinition()->GetDefinitionIndex() == hItemDefMusicKitDefault->GetDefinitionIndex() )
		)
	{
		static const CEconItemAttributeDefinition *pAttr_MusicID = GetItemSchema()->GetAttributeDefinitionByName( "music id" );
		uint32 unMusicID;
		if ( FindAttribute( pAttr_MusicID, &unMusicID ) )
		{
			const CEconMusicDefinition *pMusicDef = GetItemSchema()->GetMusicDefinition( unMusicID );

			if ( pMusicDef )
			{
				return pMusicDef->GetInventoryImage();
			}
		}
	}
#endif

	return GetStaticData()->GetInventoryImage();
}

// Does this item use a custom generated image due to a custom paint and wear
bool CEconItemView::HasGeneratedInventoryImage( void ) const
{
	return GetCustomPaintKitIndex() != 0 && ( GetCustomPaintKitWear() != 0.0f || GetCustomPaintKitSeed() != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Return the drawing data for the image to use for model panels containing this item
//-----------------------------------------------------------------------------
bool CEconItemView::GetInventoryImageData( int *iPosition, int *iSize )
{
	if ( !GetStaticData() )
		return false;
	for ( int i = 0; i < 2; i++ )
	{
		iPosition[i] = GetStaticData()->GetInventoryImagePosition(i);
		iSize[i] = GetStaticData()->GetInventoryImageSize(i);
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return the image to use for model panels containing this item
//-----------------------------------------------------------------------------
const char *CEconItemView::GetInventoryOverlayImage( int idx )
{
	if ( !GetStaticData() )
		return NULL;
	return GetStaticData()->GetInventoryOverlayImage( idx );
}

int	CEconItemView::GetInventoryOverlayImageCount( void )
{
	if ( !GetStaticData() )
		return 0;
	return GetStaticData()->GetInventoryOverlayImageCount();
}

//-----------------------------------------------------------------------------
// Purpose: Return the model to use when displaying this model on the player character model, if any
//-----------------------------------------------------------------------------
const char *CEconItemView::GetPlayerDisplayModel( int iClass ) const
{
	const CEconItemDefinition *pDef = GetStaticData();
	if ( !pDef )
		return NULL;

	// If we have styles, give the style system a chance to change the mesh used for this
	// player class.
	if ( pDef->GetNumStyles() )
	{
		const CEconStyleInfo *pStyle = pDef->GetStyleInfo( GetItemStyle() );

		// It's possible to get back a NULL pStyle if GetItemStyle() returns INVALID_STYLE_INDEX.
		if ( pStyle )
		{
#if defined( TF_DLL ) || defined( TF_CLIENT_DLL )
			// TF styles support per-class models.
			const CTFStyleInfo *pTFStyle = assert_cast<const CTFStyleInfo *>( pStyle );
			if ( pTFStyle->GetPlayerDisplayModel( iClass ) )
				return pTFStyle->GetPlayerDisplayModel( iClass );
#endif // defined( TF_DLL ) || defined( TF_CLIENT_DLL )

			if ( pStyle->GetBasePlayerDisplayModel() )
				return pStyle->GetBasePlayerDisplayModel();
		}
	}

#if defined( TF_DLL ) || defined( TF_CLIENT_DLL )
	// If we don't have a style, we still a couple potential overrides.
	if ( iClass >= 0 && iClass < LOADOUT_COUNT )
	{
		// We don't support overriding meshes in the visuals section, but we might still be overriding 
		// the model for each class at the schema level.
		const CTFItemDefinition *pTFDef = dynamic_cast<const CTFItemDefinition *>( pDef );
		if ( pTFDef )	
		{
			const char *pszModel = pTFDef->GetPlayerDisplayModel(iClass);
			if ( pszModel && pszModel[0] )
				return pszModel;
		}
	}
#endif // defined( TF_DLL ) || defined( TF_CLIENT_DLL )

	return pDef->GetBasePlayerDisplayModel();
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEconItemView::GetSkin() const
{
	// Do we have a style set?
	if ( GetStaticData()->GetNumStyles() )
		return GetStaticData()->GetStyleSkin( GetItemStyle() );
		
	// Do we have per-team skins set?
	const AssetInfo *pVisData = GetStaticData()->GetAssetInfo();
	if ( pVisData )
		return pVisData->iSkin;

	return 0;
}
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetIconDisplayModel()
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetIconDisplayModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetPedestalDisplayModel()
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	static CSchemaItemDefHandle hItemDefMusicKit( "musickit" );
	static CSchemaItemDefHandle hItemDefMusicKitDefault( "musickit_default" );

	if (
		( hItemDefMusicKit && GetItemDefinition()->GetDefinitionIndex() == hItemDefMusicKit->GetDefinitionIndex() ) ||
		( hItemDefMusicKitDefault && GetItemDefinition()->GetDefinitionIndex() == hItemDefMusicKitDefault->GetDefinitionIndex() )
		)
	{
		static const CEconItemAttributeDefinition *pAttr_MusicID = GetItemSchema()->GetAttributeDefinitionByName( "music id" );
		uint32 unMusicID;
		if ( FindAttribute( pAttr_MusicID, &unMusicID ) )
		{
			const CEconMusicDefinition *pMusicDef = GetItemSchema()->GetMusicDefinition( unMusicID );

			if ( pMusicDef )
			{
				return pMusicDef->GetPedestalDisplayModel();
			}
		}
	}

	return pData->GetPedestalDisplayModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetBuyMenuDisplayModel()
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetBuyMenuDisplayModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetWorldDroppedModel()
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetWorldDroppedModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetMagazineModel()
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetMagazineModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetScopeLensMaskModel()
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetScopeLensMaskModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetStatTrakModelByType( int nStatTrakType )
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetStatTrakModelByType( nStatTrakType );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetUidModel()
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetUidModel();
}

// Purpose: 
#if defined(CSTRIKE_CLIENT_DLL)

const char *CEconItemView::GetStickerSlotModelBySlotIndex( int nIndex )
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetStickerSlotModelBySlotIndex( nIndex );
}
int CEconItemView::GetNumSupportedStickerSlots( void )
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return 0;
	return pData->GetNumSupportedStickerSlots();
}
Vector CEconItemView::GetStickerSlotWorldProjectionStartBySlotIndex( int nIndex )
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return Vector(0,0,0);
	return pData->GetStickerSlotWorldProjectionStartBySlotIndex( nIndex );
}
Vector CEconItemView::GetStickerSlotWorldProjectionEndBySlotIndex( int nIndex )
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return Vector(0,0,0);
	return pData->GetStickerSlotWorldProjectionEndBySlotIndex( nIndex );
}
const char *CEconItemView::GetStickerWorldModelBoneParentNameBySlotIndex( int nIndex )
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetStickerWorldModelBoneParentNameBySlotIndex( nIndex );
}
const char *CEconItemView::GetStickerSlotMaterialBySlotIndex( int nIndex )
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetStickerSlotMaterialBySlotIndex( nIndex );
}
IMaterial *CEconItemView::GetStickerIMaterialBySlotIndex( int nIndex, bool bThirdPerson )
{
	FOR_EACH_VEC( m_pStickerMaterials, i )
	{
		if (m_pStickerMaterials[i].m_nSlotIndex == nIndex)
		{
			if ( bThirdPerson == true )
			{
				return m_pStickerMaterials[i].m_pMaterialReferenceThirdPerson;
			}
			else
			{
				return m_pStickerMaterials[i].m_pMaterialReferenceFirstPerson;
			}
		}
	}
	return NULL;
}

#endif //(CSTRIKE_CLIENT_DLL)

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetWorldDisplayModel()
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetWorldDisplayModel();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CEconItemView::GetExtraWearableModel()
{
	const CEconItemDefinition *pData = GetStaticData();
	if ( !pData )
		return NULL;

	return pData->GetExtraWearableModel();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEconItemView::GetQualityParticleType()
{
	static attachedparticlesystem_t *pSparkleSystem = GetItemSchema()->FindAttributeControlledParticleSystem( "community_sparkle" );

	CEconItem* pItem = GetSOCData();
	if ( !pItem )
		return 0;

	if( GetSOCData()->GetQuality() == AE_SELFMADE || GetSOCData()->GetQuality() == AE_COMMUNITY )
		return pSparkleSystem ? pSparkleSystem->nSystemID : 0;
	else
		return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Return the animation set that this item wants the player to use (ie., melee, item1, pda)
//-----------------------------------------------------------------------------
int CEconItemView::GetAnimationSlot( void )
{
	if ( !GetStaticData() )
		return -1;

#if defined( CSTRIKE_DLL ) || defined( CSTRIKE_GC_DLL )
	return -1;
#else
	return GetStaticData()->GetAnimSlot();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Return an int that indicates whether the item should be dropped from a dead owner.
//-----------------------------------------------------------------------------
int CEconItemView::GetDropType( void )
{
	if ( !GetStaticData() )
		return 0;
	return GetStaticData()->GetDropType();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::DestroyAllAttributes( void )
{
	m_AttributeList.DestroyAllAttributes();
	m_NetworkedDynamicAttributesForDemos.DestroyAllAttributes();
	NetworkStateChanged();
	MarkDescriptionDirty();
}


#if defined ( GAME_DLL )
void CEconItemView::UpdateNetworkedCustomName()
{
	const char* szName = GetSOCData() ? GetSOCData()->GetCustomName() : NULL;
	if ( szName )
	{
		V_strncpy( m_szCustomName.GetForModify(), szName, MAX_ITEM_CUSTOM_NAME_DATABASE_SIZE );
	}
}
#endif

// Done once, the first time need these values, since new KillEater attributes cannot be added during play
//
void CEconItemView::GenerateKillEaterTypeVector()
{
	m_vCachedKillEaterTypes.RemoveAll();
	m_vCachedKillEaterValues.RemoveAll();

	for ( int i = 0; i < GetKillEaterAttrPairCount(); i++ )
	{
		const CEconItemAttributeDefinition *pScoreTypeAttribDef = GetKillEaterAttrPair_Type(i);
		if ( !pScoreTypeAttribDef )
			continue;

		attrib_value_t nValue;
		if ( FindAttribute( pScoreTypeAttribDef, &nValue ) )
		{
			m_vCachedKillEaterTypes.Insert( nValue );
		}
	}
}

void CEconItemView::GetKillEaterTypes( CUtlSortVector<uint32> &types )
{
   if ( !m_bKillEaterTypesCached )
   {
	  GenerateKillEaterTypeVector();
	  m_bKillEaterTypesCached = true;
   }

   types = m_vCachedKillEaterTypes;
   return;
}

int32 CEconItemView::GetKillEaterValueByType( uint32 uKillEaterType )
{
uint32 uiThisSectionOfCodeMightHaveBugs = 1;
/*

BUG - BUG - BUG - BUG - BUG - BUG

			There are a bunch of bugs with the way StatTrak values are networked.
			I fixed it in shipped code, but cannot back merge the fix due to how
			concepts changed in trunk. Together with additional StatTrak module
			and new attribute networking it's highly likely that this system needs
			to be rewritten. If somebody integrates this to rel and sees this huge
			diff then make sure you test all combinations of following:
			Weapon Type											Test Cases
			-------------------									-----------------
			non-StatTrak weapon									live match, you playing with your own weapon
			StatTrak weapon with 0 kills						live match, somebody else on TEAM_SPECTATOR watching you
			StatTrak weapon with >0 kills						live match, watching via GOTV master connection on TV port of active game server
			StatTrak weapon getting a kill from 0				live match, watching via GOTV relay, relay connected to GOTV master
			StatTrak weapon with >0 kills getting a kill		playing back GOTV demo while logged in to Steam as your own account
			                                                    playing back GOTV demo while logged in to Steam as a different account
			all the above cases, but StatTrak weapon
			wielded by non-owner




Change: 1958395

Date: 10/18/2013 2:52:36 PM

Client: vitaliy_valve_csgo_work_2012

User: vitaliy

Status: submitted

Type: public

Description:
[ CSGO - StatTrak's GOTV and Demos ]
 Fixed different code paths that were incorrectly
 handling bitpatterns of stattrak values.

 When networking -1 value as only 20 bits on the
 recieving side we would not get -1, but rather some
 large 20-bit number and that was causing bad display.

 Also on GOTV relays we wouldn't have the SO caches
 of players and we were incorrectly sending bitpattern
 for the fallback value through a bunch of broken attrlist
 code that would on the display end get float bit pattern
 into an integer pointer resulting in a garbage number.

 Now StatTrak guns seem to display all correct values
 in game, in GOTV demos, on GOTV masters and on GOTV
 relays.

 Also increased streams count to 6 for new UI.


JobStatus: 

Jobs:


Files:
//depot/Valve/branch/cstrike15_pcbeta/source/cstrike15/src/game/client/cstrike15/Scaleform/HUD/sfhudweaponpanel.cpp#22
//depot/Valve/branch/cstrike15_pcbeta/source/cstrike15/src/game/client/cstrike15/Scaleform/components/scaleformcomponent_streams.cpp#6
//depot/Valve/branch/cstrike15_pcbeta/source/cstrike15/src/game/shared/econ/econ_entity.cpp#15

BUG - BUG - BUG - BUG - BUG - BUG

BUG - BUG - BUG - BUG - BUG - BUG

			There are a bunch of bugs with the way StatTrak values are networked.
			I fixed it in shipped code, but cannot back merge the fix due to how
			concepts changed in trunk. Together with additional StatTrak module
			and new attribute networking it's highly likely that this system needs
			to be rewritten. If somebody integrates this to rel and sees this huge
			diff then make sure you test all combinations of following:
			Weapon Type											Test Cases
			-------------------									-----------------
			non-StatTrak weapon									live match, you playing with your own weapon
			StatTrak weapon with 0 kills						live match, somebody else on TEAM_SPECTATOR watching you
			StatTrak weapon with >0 kills						live match, watching via GOTV master connection on TV port of active game server
			StatTrak weapon getting a kill from 0				live match, watching via GOTV relay, relay connected to GOTV master
			StatTrak weapon with >0 kills getting a kill		playing back GOTV demo while logged in to Steam as your own account
			                                                    playing back GOTV demo while logged in to Steam as a different account
			all the above cases, but StatTrak weapon
			wielded by non-owner




Change: 1958395

Date: 10/18/2013 2:52:36 PM

Client: vitaliy_valve_csgo_work_2012

User: vitaliy

Status: submitted

Type: public

Description:
[ CSGO - StatTrak's GOTV and Demos ]
 Fixed different code paths that were incorrectly
 handling bitpatterns of stattrak values.

 When networking -1 value as only 20 bits on the
 recieving side we would not get -1, but rather some
 large 20-bit number and that was causing bad display.

 Also on GOTV relays we wouldn't have the SO caches
 of players and we were incorrectly sending bitpattern
 for the fallback value through a bunch of broken attrlist
 code that would on the display end get float bit pattern
 into an integer pointer resulting in a garbage number.

 Now StatTrak guns seem to display all correct values
 in game, in GOTV demos, on GOTV masters and on GOTV
 relays.

 Also increased streams count to 6 for new UI.


JobStatus: 

Jobs:


Files:
//depot/Valve/branch/cstrike15_pcbeta/source/cstrike15/src/game/client/cstrike15/Scaleform/HUD/sfhudweaponpanel.cpp#22
//depot/Valve/branch/cstrike15_pcbeta/source/cstrike15/src/game/client/cstrike15/Scaleform/components/scaleformcomponent_streams.cpp#6
//depot/Valve/branch/cstrike15_pcbeta/source/cstrike15/src/game/shared/econ/econ_entity.cpp#15

BUG - BUG - BUG - BUG - BUG - BUG

*/
++ uiThisSectionOfCodeMightHaveBugs;



	CEconItem *pSOItem = GetSOCData();
	if ( pSOItem && m_nKillEaterValuesCacheFrame >= pSOItem->GetSOUpdateFrame() ) // The SO cache has not been recently updated
	{
	   // ..so we return our cached values
	   CUtlHashtable<uint32, int32>::handle_t hh =  m_vCachedKillEaterValues.Find( uKillEaterType );
	   if ( m_vCachedKillEaterValues.IsValidHandle( hh ) )
		  return m_vCachedKillEaterValues.Element( hh );
	}

#ifdef CLIENT_DLL
	pSOItem = 0;
#endif

	// Only use fallback if there's no SO cache!
	int32 nKillEater = -1;
	
	// TODO: This function does not update the killeater count after each kill; rather it only updates when the weapon
	// is instantiated in the world (buying)
	for ( int32 i = 0; i < GetKillEaterAttrPairCount(); i++ )
	{
	   const CEconItemAttributeDefinition *pKillEaterAltScoreTypeAttrDef = GetKillEaterAttrPair_Type( i );
	   if ( !pKillEaterAltScoreTypeAttrDef )
		  continue;
	   
	   attrib_value_t unValue;
	   if ( !FindAttribute( pKillEaterAltScoreTypeAttrDef, &unValue ) )
		  continue;
	   
	   // If this isn't the type we're tracking, ignore it
	   if ( unValue != uKillEaterType )
		  continue;
	   
	   const CEconItemAttributeDefinition *pKillEaterAltAttrDef = GetKillEaterAttrPair_Score( i );
	   if ( pKillEaterAltAttrDef )
	   {
		  // To get live attribute changes we need to get them from the SOCache because the instance of EconItemView won't have them until we spawn a new copy.
		  attrib_value_t unKillEaterAltScore = 0;
		  if ( pSOItem )
		  {
			 if ( pSOItem->FindAttribute( pKillEaterAltAttrDef, &unKillEaterAltScore ) )
			 {
				nKillEater = unKillEaterAltScore;
			 }
		  }
		  else
		  {
			 if ( FindAttribute( pKillEaterAltAttrDef, &unKillEaterAltScore ) )
			 {
				// The fallback might be larger
				nKillEater = Max( nKillEater, (int32)unKillEaterAltScore );
			 }
		  }
		  break;
	   }
	}
	
	if ( !pSOItem )
	{
		// Don't cache when playing back from networked dynamic attributes
		m_vCachedKillEaterValues.Remove( uKillEaterType );
		m_vCachedKillEaterValues.Insert( uKillEaterType, nKillEater );
	}
	
    m_nKillEaterValuesCacheFrame = gpGlobals->framecount;
	
	return nKillEater;
}


#if defined(CSTRIKE_CLIENT_DLL)
IVisualsDataProcessor *CEconItemView::GetVisualsDataProcessorByName( const char* szName ) const 
{
	FOR_EACH_VEC( m_ppVisualsDataProcessors, i )
	{
		IVisualsDataProcessor* pVDP = m_ppVisualsDataProcessors[ i ];
		if ( V_strcmp( pVDP->GetOriginalMaterialBaseName(), szName ) == 0 )
			return pVDP;
	}

	return NULL;
}


void CEconItemView::CreateCustomClothingMaterials( const char *pchSkinIdent, int nSlotId, int nTeam, bool bIgnorePicMip, CompositeTextureSize_t diffuseTextureSize )
{
	int nPaintKit = GetCustomPaintKitIndex();
	if ( nPaintKit == 0 ) // default paintkit, no need to composite
		return;

	if ( !GetPlayerDisplayModel( 0 ) )
	{
		AssertMsg1( false, "Failed to GetPlayerDisplayModel for %s.", pchSkinIdent );
		return;
	}

	//read in paint kit
	const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinition( nPaintKit );
	if ( !pPaintKit )
	{
		AssertMsg2( false, "Failed to find paintkit index %i for %s.", nPaintKit, pchSkinIdent);
		return;
	}

	
	MDLHandle_t modelHandle = mdlcache->FindMDL( GetPlayerDisplayModel( 0 ) );
	//MDLHandle_t modelHandle = mdlcache->FindMDL( GetWorldDisplayModel() );
	if ( modelHandle == MDLHANDLE_INVALID )
	{
		AssertMsg1( false, "Failed to find player model handle for %s.", pchSkinIdent );
		return;
	}
	
	studiohdr_t	*pStudioHdr = g_pMDLCache->GetStudioHdr( modelHandle );
	
	int nSeed = GetCustomPaintKitSeed();
	float flWear = GetCustomPaintKitWear( 0 );

	const CUtlVector< WeaponPaintableMaterial_t > *pPaintData = GetStaticData()->GetPaintData();

	int nNumMaterialsToPaint = pPaintData->Count();

	for ( int nCustomMaterialIndex = 0; nCustomMaterialIndex < nNumMaterialsToPaint; nCustomMaterialIndex++ )
	{
		CCSClothingVisualsDataCompare clothingCompareObject;
		clothingCompareObject.m_nIndex = nPaintKit;
		clothingCompareObject.m_nSeed = nSeed;
		clothingCompareObject.m_flWear = flWear;
		clothingCompareObject.m_nModelID = GetItemIndex();
		clothingCompareObject.m_nLOD = 0;
		clothingCompareObject.m_nTeamId = -1; // may need this later to differentiate teams
		clothingCompareObject.m_bMirrorPattern = (*pPaintData)[ nCustomMaterialIndex ].m_bMirrorPattern;
		clothingCompareObject.m_nMaterialId = nCustomMaterialIndex;
		
		CCSClothingVisualsDataProcessor* pVisualsDataProcessor = new CCSClothingVisualsDataProcessor( Move(clothingCompareObject), &( (*pPaintData)[ nCustomMaterialIndex ] ), "CustomCharacter" );
		bool bCreatedMaterial = false;

		if ( pVisualsDataProcessor->HasCustomMaterial() )
		{
			// find the material in the models materials to get its index (used for overriding)
			bool bFound = false;
			int nModelMaterialIndex;
			char szBaseName[ MAX_PATH ];
			for ( nModelMaterialIndex = 0; nModelMaterialIndex < pStudioHdr->numtextures; nModelMaterialIndex++ )
			{
				mstudiotexture_t *pTexture = pStudioHdr->pTexture( nModelMaterialIndex );
				V_FileBase( pTexture->pszName(), szBaseName, sizeof( szBaseName ) );
				if ( V_stricmp( szBaseName, pVisualsDataProcessor->GetOriginalMaterialBaseName() ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			if ( !bFound )
			{
				DevMsg( "Original material not found! Name: %s \n", pVisualsDataProcessor->GetOriginalMaterialBaseName() );
			}
			else
			{
				m_ppVisualsDataProcessors.AddToTail(  pVisualsDataProcessor );
				pVisualsDataProcessor->AddRef();

				KeyValues *pVMTKeyValues = pVisualsDataProcessor->GenerateCustomMaterialKeyValues();
				Assert( pVMTKeyValues );

				CUtlVector< SCompositeTextureInfo > vecTextureInfo;

				int nIndex = vecTextureInfo.AddToTail();
				vecTextureInfo[nIndex].m_size = diffuseTextureSize;
				vecTextureInfo[nIndex].m_format = COMPOSITE_TEXTURE_FORMAT_DXT5;
				vecTextureInfo[nIndex].m_nMaterialParamID = MATERIAL_PARAM_ID_BASE_DIFFUSE_TEXTURE;
				vecTextureInfo[nIndex].m_bSRGB = true;
				vecTextureInfo[nIndex].m_pVisualsDataProcessor = pVisualsDataProcessor;

				if (pVMTKeyValues->FindKey("$bumpmap", false))
				{
					nIndex = vecTextureInfo.AddToTail();
					vecTextureInfo[nIndex].m_size = diffuseTextureSize;
					vecTextureInfo[nIndex].m_format = COMPOSITE_TEXTURE_FORMAT_DXT5;
					vecTextureInfo[nIndex].m_nMaterialParamID = MATERIAL_PARAM_ID_BUMP_MAP;
					vecTextureInfo[nIndex].m_bSRGB = false;
					vecTextureInfo[nIndex].m_pVisualsDataProcessor = pVisualsDataProcessor;
				}

				if (pVMTKeyValues->FindKey("$masks1", false))
				{
					nIndex = vecTextureInfo.AddToTail();
					vecTextureInfo[nIndex].m_size = diffuseTextureSize;
					vecTextureInfo[nIndex].m_format = COMPOSITE_TEXTURE_FORMAT_DXT5;
					vecTextureInfo[nIndex].m_nMaterialParamID = MATERIAL_PARAM_ID_MASKS1_MAP;
					vecTextureInfo[nIndex].m_bSRGB = false;
					vecTextureInfo[nIndex].m_pVisualsDataProcessor = pVisualsDataProcessor;
				}

				ICustomMaterial *pCustomMaterial = g_pMaterialSystem->GetCustomMaterialManager()->GetOrCreateCustomMaterial( pVMTKeyValues, vecTextureInfo, bIgnorePicMip );
				delete pVMTKeyValues; // copied inside GetCustomMaterial, no longer needed

				if ( pCustomMaterial )
				{
					SetCustomMaterial( pCustomMaterial, nModelMaterialIndex );
					bCreatedMaterial = true;
				}
			}
		}

		pVisualsDataProcessor->Release();
	}
	
	mdlcache->Release( modelHandle );
}

void CEconItemView::SetCustomNameOverride( const char *pszCustomName )
{
	V_strncpy( m_szCustomNameOverride, pszCustomName, sizeof( m_szCustomNameOverride ) );
}


bool CEconItemView::ItemHasAnyStickersApplied( void )
{
	return ( GetNumSupportedStickerSlots() > 0 && m_pStickerMaterials.Count() > 0 );
}

bool CEconItemView::ItemHasAnyFreeStickerSlots( void )
{
	return ( GetNumSupportedStickerSlots() != m_pStickerMaterials.Count() );
}

int CEconItemView::GetStickerSlotFirstFreeFromIndex( int nIndex )
{
	if ( !ItemHasAnyFreeStickerSlots() )
		return -1;
	
	for ( int i=0; i<=GetNumSupportedStickerSlots(); i++ )
	{
		if ( nIndex >= GetNumSupportedStickerSlots() )
			nIndex = 0;

		if ( !GetStickerAttributeBySlotIndexInt( nIndex, k_EStickerAttribute_ID, 0 ) )
			return nIndex;

		nIndex++;
	}
	
	return -1;
}


#ifdef _DEBUG
ConVar stickers_debug_material_spew( "stickers_debug_material_spew", "0", FCVAR_DEVELOPMENTONLY, "Spew generated sticker materials to the console." );
extern ConVar stickers_debug_randomize;
ConVar stickers_debug_randomize_min_index( "stickers_debug_randomize_min_index", "28", FCVAR_DEVELOPMENTONLY, "" );
ConVar stickers_debug_randomize_max_index( "stickers_debug_randomize_max_index", "47", FCVAR_DEVELOPMENTONLY, "" );

ConVar stickers_always_regenerate( "stickers_always_regenerate", "0", FCVAR_DEVELOPMENTONLY, "Don't attempt to load cached sticker materials" );

char g_szStickersOverridePath[256];
ConVar stickers_override_path_enabled(	"stickers_override_path_enabled",	"0",	FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "" );
ConVar stickers_override_wear(			"stickers_override_wear",			"0.0",	FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "" );
ConVar stickers_override_scale(			"stickers_override_scale",			"1.0",	FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "" );
ConVar stickers_override_rotation(		"stickers_override_rotation",		"0.0",	FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT, "" );
void cc_StickersOverridePath(const CCommand& args)
{
	//e.g.: "stickers2/banana.vmt"
	V_StripExtension( args[1], g_szStickersOverridePath, sizeof(g_szStickersOverridePath) );
	stickers_always_regenerate.SetValue(1);
	stickers_override_path_enabled.SetValue(1);
}
static ConCommand stickers_override_path("stickers_override_path", cc_StickersOverridePath, "", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
#endif

extern ConVar cl_righthand;
void CEconItemView::GenerateStickerMaterials( void )
{
	//create sticker materials per item using the base sticker material for a particular slot + the item's sticker attributes for that slot

	if ( !GetItemSchema() )
		return;

	int nSupportedStickerSlots = GetNumSupportedStickerSlots();

	m_pStickerMaterials.RemoveAll();

	for ( int i=0; i<nSupportedStickerSlots; i++ )
	{
		
		uint32 nStickerId = GetStickerAttributeBySlotIndexInt( i, k_EStickerAttribute_ID, 0 );
#ifdef _DEBUG
		if ( stickers_debug_randomize.GetBool() )
		{
			if ( stickers_debug_randomize_min_index.GetInt() != -1 && stickers_debug_randomize_max_index.GetInt() != -1 )
			{
				stickers_debug_randomize_min_index.SetValue(  MAX( stickers_debug_randomize_min_index.GetInt(), 0 ) );
				stickers_debug_randomize_max_index.SetValue(  MIN( stickers_debug_randomize_max_index.GetInt(), GetItemSchema()->GetStickerKitDefinitionCount()-1 ) );
				nStickerId = RandomInt( stickers_debug_randomize_min_index.GetInt(), stickers_debug_randomize_max_index.GetInt() );
			}
			else
			{
				nStickerId = RandomInt( 1, GetItemSchema()->GetStickerKitDefinitionCount()-1 );
			}
		}
#endif

		if ( nStickerId <= 0 
#ifdef _DEBUG
			&& !stickers_override_path_enabled.GetBool() 
#endif
			)
			continue; // the sticker id for this slot is invalid, no need to build a sticker material

		//get additional sticker attributes
		float flStickerWear = GetStickerAttributeBySlotIndexFloat( i, k_EStickerAttribute_Wear, 0.0f );
		float flStickerScale = GetStickerAttributeBySlotIndexFloat( i, k_EStickerAttribute_Scale, 1.0f );
		float flStickerRotation = GetStickerAttributeBySlotIndexFloat( i, k_EStickerAttribute_Rotation, 0.0f );

#ifdef _DEBUG
		if ( stickers_debug_randomize.GetBool() )
		{
			//get additional sticker attributes
			flStickerWear = RandomFloat( 0.2f, 0.8f );
			flStickerScale = RandomFloat( 0.95f, 1.05f );
			flStickerRotation = RandomFloat( -10.0f, 10.0f );
		}

		if ( stickers_override_path_enabled.GetBool() )
		{
			//get additional sticker attributes
			flStickerWear = stickers_override_wear.GetFloat();
			flStickerScale = stickers_override_scale.GetFloat();
			flStickerRotation = stickers_override_rotation.GetFloat();
		}
#endif

		//get the sticker base material path
		const char *szStickerBaseMaterialPath = GetStickerSlotMaterialBySlotIndex(i);
		if ( !szStickerBaseMaterialPath || szStickerBaseMaterialPath[0] == 0 )
		{
#ifdef _DEBUG
			ConColorMsg( Color(240,170,10,255), "Failed to get the base material path for sticker ID: %i\n", nStickerId );
#endif
			continue; //failed to get the base material path
		}

		char szStickerShortName[32]; //this is the base name of the material AND model, by convention. e.g. "pist_deagle_decal_A"
		V_FileBase( szStickerBaseMaterialPath, szStickerShortName, sizeof(szStickerShortName) );

		const CStickerKit* pStickerKit = GetItemSchema()->GetStickerKitDefinition(nStickerId);

		const char *szStickerMaterialSchemaName;
		//get the sticker material schema name, e.g.: "navi"
#ifdef _DEBUG
		if ( stickers_override_path_enabled.GetBool() )
		{
			szStickerMaterialSchemaName = g_szStickersOverridePath;
		}
		else
#endif
		if (pStickerKit)
		{
			szStickerMaterialSchemaName = pStickerKit->sMaterialPath.String();
		}
		else
		{
#ifdef _DEBUG
			ConColorMsg(Color(240, 170, 10, 255), "Sticker kit definition is invalid for sticker ID: %i\n", nStickerId );
#endif
			continue; //sticker kit definition is invalid
		}
		
		if ( !szStickerMaterialSchemaName || szStickerMaterialSchemaName[0] == 0 )
		{
#ifdef _DEBUG
			ConColorMsg(Color(240, 170, 10, 255), "Failed to get material schema name: %s\n", szStickerBaseMaterialPath);
#endif
			continue; //failed to get material schema name
		}

		//build the full material vmt path from the schema name
		char szStickerMaterialPath[MAX_PATH];
		if ( pStickerKit && pStickerKit->bMaterialPathIsAbsolute )
		{
			V_snprintf( szStickerMaterialPath, sizeof(szStickerMaterialPath), "%s", szStickerMaterialSchemaName	);
		}
		else
		{
			V_snprintf( szStickerMaterialPath, sizeof(szStickerMaterialPath), "materials/models/weapons/customization/stickers/%s.vmt", szStickerMaterialSchemaName	);
		}

		//generate a unique but deterministic char sequence using rotation, scale, wear
		char szStickerUniqueParamStr[32];
		V_snprintf( szStickerUniqueParamStr, sizeof(szStickerUniqueParamStr), "%f_%f_%f", flStickerRotation, flStickerScale, flStickerWear );
		int nParamHash = HashStringCaseless( szStickerUniqueParamStr );

		//generate the desired material names
		char szDesiredStickerMaterialNameFirstPerson[128];
		V_snprintf( szDesiredStickerMaterialNameFirstPerson, sizeof(szDesiredStickerMaterialNameFirstPerson), "sticker_%s_%s_%i_1st", szStickerShortName, szStickerMaterialSchemaName, nParamHash );

		char szDesiredStickerMaterialNameThirdPerson[128];
		V_snprintf( szDesiredStickerMaterialNameThirdPerson, sizeof(szDesiredStickerMaterialNameThirdPerson), "sticker_%s_%s_%i_3rd", szStickerShortName, szStickerMaterialSchemaName, nParamHash );

		//these materials may already exist, try to find them
		IMaterial *stickerMaterialFirstPerson = materials->FindProceduralMaterial( szDesiredStickerMaterialNameFirstPerson, TEXTURE_GROUP_OTHER );
		IMaterial *stickerMaterialThirdPerson = materials->FindProceduralMaterial( szDesiredStickerMaterialNameThirdPerson, TEXTURE_GROUP_OTHER );

#ifdef _DEBUG
		if ( stickers_debug_material_spew.GetBool() )
		{
			if ( !stickerMaterialFirstPerson->IsErrorMaterial() )
			{
				ConColorMsg( Color(240,170,10,255), "Found pre-existing 1st-person sticker material: %s\n", szDesiredStickerMaterialNameFirstPerson );
			}
			if ( !stickerMaterialThirdPerson->IsErrorMaterial() )
			{
				ConColorMsg( Color(240,170,10,255), "Found pre-existing 3rd-person sticker material: %s\n", szDesiredStickerMaterialNameThirdPerson );
			}
		}
#endif

		//if it exists already, great - otherwise we need to create it
		if ( stickerMaterialFirstPerson->IsErrorMaterial() || stickerMaterialThirdPerson->IsErrorMaterial()
#ifdef _DEBUG
			|| stickers_always_regenerate.GetBool()
#endif
			)
		{
			//load the specific sticker material params, these are needed for both first and third person versions
			KeyValues *pSpecificStickerMaterialKeyValues = new KeyValues( "vmt" );
			KeyValues::AutoDelete autodelete_pSpecificStickerMaterialKeyValues( pSpecificStickerMaterialKeyValues );
			if ( !pSpecificStickerMaterialKeyValues->LoadFromFile( g_pFullFileSystem, szStickerMaterialPath, "GAME" ) )
			{
#ifdef _DEBUG
				ConColorMsg(Color(240, 170, 10, 255), "Failed to load specific sticker material keyvalues: %s\n", szStickerMaterialPath);
#endif
				continue; //failed to load specific sticker material keyvalues
			}

			//set wear, scale and rotation params in the specific sticker keyvalues
			pSpecificStickerMaterialKeyValues->SetFloat( "$wearprogress", flStickerWear );
			pSpecificStickerMaterialKeyValues->SetFloat( "$patternrotation", flStickerRotation );
			pSpecificStickerMaterialKeyValues->SetFloat( "$patternscale", flStickerScale );

			if ( stickerMaterialFirstPerson->IsErrorMaterial()
#ifdef _DEBUG
				|| stickers_always_regenerate.GetBool()
#endif
				)
			{
				//first person stickers need the base material params for custom per-sticker AO and wear remapping
				KeyValues *pFirstPersonKeyValues = new KeyValues( "vmt" );
				KeyValues::AutoDelete autodelete_pFirstPersonKeyValues( pFirstPersonKeyValues );
				
				if ( !pFirstPersonKeyValues->LoadFromFile( g_pFullFileSystem, szStickerBaseMaterialPath, "GAME" ) )
				{
#ifdef _DEBUG
					ConColorMsg(Color(240, 170, 10, 255), "Failed to load base material keyvalues: %s\n", szStickerBaseMaterialPath);
#endif
					continue; //failed to load base material keyvalues
				}
				
				if ( cl_righthand.GetBool() == false )
				{
					// left-handed viewmodels flip their stickers so they render correctly
					pFirstPersonKeyValues->SetInt( "$mirrorhorizontal", 1 );
				}

				//now combine the keyvalues - this will produce the final sticker material params
				pFirstPersonKeyValues->MergeFrom( pSpecificStickerMaterialKeyValues, KeyValues::MERGE_KV_UPDATE );


				//now add the relevant sticker slot proxy list
				KeyValues *pFirstPersonStickerProxyKeyValues = new KeyValues("vmt");
				KeyValues::AutoDelete autodelete_pFirstPersonStickerProxyKeyValues(pFirstPersonStickerProxyKeyValues);
				char szStickerMaterialProxyListPath[MAX_PATH];
				V_snprintf(szStickerMaterialProxyListPath, sizeof(szStickerMaterialProxyListPath), 
					"materials/models/weapons/customization/stickers/default/sticker_proxies_%i.vmt", i);
				if ( pFirstPersonStickerProxyKeyValues->LoadFromFile(g_pFullFileSystem, szStickerMaterialProxyListPath, "GAME"))
				{
					pFirstPersonKeyValues->MergeFrom( pFirstPersonStickerProxyKeyValues, KeyValues::MERGE_KV_UPDATE );
				}
				autodelete_pFirstPersonStickerProxyKeyValues.Detach();

#ifdef _DEBUG
				if ( !stickerMaterialFirstPerson->IsErrorMaterial() && stickers_always_regenerate.GetBool())
				{
					ConColorMsg( Color(240,170,10,255), "WARNING: stickers_always_regenerate is TRUE, regenerating: %s\n", szDesiredStickerMaterialNameFirstPerson );
					stickerMaterialFirstPerson->SetShaderAndParams( pFirstPersonKeyValues );
					stickerMaterialFirstPerson->RefreshPreservingMaterialVars();
				}
				else
#endif
				{
					//create a new material with the final sticker material params
					stickerMaterialFirstPerson = materials->CreateMaterial(szDesiredStickerMaterialNameFirstPerson, pFirstPersonKeyValues);
					stickerMaterialFirstPerson->Refresh();
				}
				
#ifdef _DEBUG
				if ( stickers_debug_material_spew.GetBool() )
				{
					ConColorMsg( Color(240,170,10,255), "Generated first person sticker material: %s\n", szDesiredStickerMaterialNameFirstPerson );
					KeyValuesDumpAsDevMsg( pFirstPersonKeyValues, 2 );
				}
#endif
				autodelete_pFirstPersonKeyValues.Detach();
			}

			if ( stickerMaterialThirdPerson->IsErrorMaterial()
#ifdef _DEBUG
				|| stickers_always_regenerate.GetBool()
#endif
				)
			{
				//create a 'thirdperson' material that only needs the specific sticker material params and a special $thirdperson param set

				KeyValues *pThirdPersonKeyValues = new KeyValues("WeaponDecal");
				KeyValues::AutoDelete autodelete_pThirdPersonKeyValues(pThirdPersonKeyValues);

				pThirdPersonKeyValues->MergeFrom( pSpecificStickerMaterialKeyValues, KeyValues::MERGE_KV_UPDATE );
				pThirdPersonKeyValues->SetInt( "$thirdperson", 1 );

#ifdef _DEBUG
				if ( !stickerMaterialThirdPerson->IsErrorMaterial() && stickers_always_regenerate.GetBool())
				{
					ConColorMsg( Color(240,170,10,255), "WARNING: stickers_always_regenerate is TRUE, regenerating: %s\n", szDesiredStickerMaterialNameThirdPerson );
					stickerMaterialThirdPerson->SetShaderAndParams( pThirdPersonKeyValues );
					stickerMaterialThirdPerson->RefreshPreservingMaterialVars();
				}
				else
#endif
				{
					stickerMaterialThirdPerson = materials->CreateMaterial( szDesiredStickerMaterialNameThirdPerson, pThirdPersonKeyValues );
					stickerMaterialThirdPerson->Refresh();
				}

#ifdef _DEBUG
				if ( stickers_debug_material_spew.GetBool() )
				{
					ConColorMsg( Color(240,170,10,255), "Generated third person sticker material: %s\n", szDesiredStickerMaterialNameThirdPerson );
					KeyValuesDumpAsDevMsg( pThirdPersonKeyValues, 2 );
				}
#endif
				autodelete_pThirdPersonKeyValues.Detach();
			}

		}

		//add material references for the materials to the econ item sticker material list
		int nNewStickerIndex = m_pStickerMaterials.AddToTail();
		m_pStickerMaterials[ nNewStickerIndex ].m_pMaterialReferenceFirstPerson.Init(stickerMaterialFirstPerson);
		m_pStickerMaterials[ nNewStickerIndex ].m_pMaterialReferenceThirdPerson.Init(stickerMaterialThirdPerson);
		m_pStickerMaterials[ nNewStickerIndex ].m_nSlotIndex = i;

	}

}

void CEconItemView::UpdateGeneratedMaterial( bool bIgnorePicMip, CompositeTextureSize_t diffuseTextureSize )
{
	if ( diffuseTextureSize != COMPOSITE_TEXTURE_SIZE_512 )
	{
		ClearCustomMaterials();
	}

	if ( ( GetCustomMaterialCount() == 0 ) && GetStaticData() )
	{
		const char *pItemClass = GetStaticData()->GetItemClass(); 
		if ( pItemClass )
		{
			if ( V_strcmp( pItemClass, "tool" ) == 0 )
			{
				// paint
			}
			else if ( V_strcmp( pItemClass, "wearable_item" ) == 0 )
			{
				// clothing items
				int nTeam = -1; // should remove this?  Will we do anything team specific?
				int nSlotId = GetStaticData()->GetLoadoutSlot( nTeam );
				if ( GetStaticData()->GetPaintData()->Count() > 0 )
				{
					CreateCustomClothingMaterials( GetStaticData()->GetDefinitionName(), nSlotId, nTeam, bIgnorePicMip, diffuseTextureSize );
				}
			}
			else
			{
				WEAPON_FILE_INFO_HANDLE handle = LookupWeaponInfoSlot( pItemClass );
				if ( handle != GetInvalidWeaponInfoHandle() )
				{
					int nWeaponId =  WeaponIdFromString( pItemClass );
					if ( GetStaticData()->GetPaintData()->Count() > 0 )
					{
						CreateCustomWeaponMaterials( nWeaponId, bIgnorePicMip, diffuseTextureSize );
					}
				}
			}
		}
	}
}

void CEconItemView::CreateCustomWeaponMaterials( int nWeaponId, bool bIgnorePicMip, CompositeTextureSize_t diffuseTextureSize )
{
	if ( !GetWorldDisplayModel() )
	{
		AssertMsg1( false, "Failed to GetWorldDisplayModel for %s.", WeaponIdAsString((CSWeaponID)nWeaponId) );
		return;
	}

	//read in paint kit
	int nPaintKit = GetCustomPaintKitIndex();
	const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinition( nPaintKit );
	if ( !pPaintKit )
	{
		AssertMsg2( false, "Failed to find paintkit index %i for %s.", nPaintKit, WeaponIdAsString((CSWeaponID)nWeaponId) );
		return;
	}

	MDLHandle_t modelHandle = mdlcache->FindMDL( GetWorldDisplayModel() );
	if ( modelHandle == MDLHANDLE_INVALID )
	{
		AssertMsg1( false, "Failed to find world display model handle for %s.", WeaponIdAsString((CSWeaponID)nWeaponId) );
		return;
	}

	studiohdr_t	*pStudioHdr = g_pMDLCache->GetStudioHdr( modelHandle );

	//read in random seed
	int nSeed = GetCustomPaintKitSeed();
	float flWear = GetCustomPaintKitWear( pPaintKit->flWearDefault );

	const CUtlVector< WeaponPaintableMaterial_t > *pPaintData = GetStaticData()->GetPaintData();

	int nNumMaterialsToPaint = pPaintKit->bOnlyFirstMaterial ? 1 : pPaintData->Count();

 	for ( int nCustomMaterialIndex = 0; nCustomMaterialIndex < nNumMaterialsToPaint; nCustomMaterialIndex++ )
	{
		CCSWeaponVisualsDataCompare compareObject;
		compareObject.m_nIndex = nPaintKit;
		compareObject.m_nSeed = nSeed;
		compareObject.m_flWear = flWear;
		compareObject.m_nModelID = nWeaponId;
		compareObject.m_nLOD = 1;
		compareObject.m_flWeaponLength = (*pPaintData)[ nCustomMaterialIndex ].m_flWeaponLength;
		compareObject.m_flUVScale = (*pPaintData)[ nCustomMaterialIndex ].m_flUVScale;

		CCSWeaponVisualsDataProcessor* pVisualsDataProcessor = new CCSWeaponVisualsDataProcessor( Move(compareObject), &( (*pPaintData)[ nCustomMaterialIndex ] ), "CustomWeapon" );
		bool bCreatedMaterial = false;

		if ( pVisualsDataProcessor->HasCustomMaterial() )
		{
			// find the material in the models materials to get it's index (used for overriding)
			bool bFound = false;
			int nModelMaterialIndex;
			char szBaseName[ MAX_PATH ];
			for ( nModelMaterialIndex = 0; nModelMaterialIndex < pStudioHdr->numtextures; nModelMaterialIndex++ )
			{
				mstudiotexture_t *pTexture = pStudioHdr->pTexture( nModelMaterialIndex );
				V_FileBase( pTexture->pszName(), szBaseName, sizeof( szBaseName ) );
				if ( V_stricmp( szBaseName, pVisualsDataProcessor->GetOriginalMaterialBaseName() ) == 0 )
				{
					bFound = true;
					break;
				}
			}

			if ( !bFound )
			{
				DevMsg( "Original material not found! Name: %s \n \n", pVisualsDataProcessor->GetOriginalMaterialBaseName() );
			}
			else
			{
				m_ppVisualsDataProcessors.AddToTail( pVisualsDataProcessor );
				pVisualsDataProcessor->AddRef();

				KeyValues *pVMTKeyValues = pVisualsDataProcessor->GenerateCustomMaterialKeyValues();
				Assert( pVMTKeyValues );

				CUtlVector< SCompositeTextureInfo > vecTextureInfo;

				int nIndex = vecTextureInfo.AddToTail();
				vecTextureInfo[ nIndex ].m_size = diffuseTextureSize;
				vecTextureInfo[ nIndex ].m_format = COMPOSITE_TEXTURE_FORMAT_DXT5;
				vecTextureInfo[ nIndex ].m_nMaterialParamID = MATERIAL_PARAM_ID_BASE_DIFFUSE_TEXTURE;
				vecTextureInfo[ nIndex ].m_bSRGB = true;
				vecTextureInfo[ nIndex ].m_pVisualsDataProcessor = pVisualsDataProcessor;
				nIndex = vecTextureInfo.AddToTail();
				vecTextureInfo[ nIndex ].m_size = ( diffuseTextureSize != COMPOSITE_TEXTURE_SIZE_512 ) ? static_cast< CompositeTextureSize_t > ( Q_log2( pPaintKit->nViewModelExponentOverrideSize ) ) : COMPOSITE_TEXTURE_SIZE_256;
				vecTextureInfo[ nIndex ].m_format = COMPOSITE_TEXTURE_FORMAT_DXT1;
				vecTextureInfo[ nIndex ].m_nMaterialParamID = MATERIAL_PARAM_ID_PHONG_EXPONENT_TEXTURE;
				vecTextureInfo[ nIndex ].m_bSRGB = false;
				vecTextureInfo[ nIndex ].m_pVisualsDataProcessor = pVisualsDataProcessor;

				ICustomMaterial *pCustomMaterial = g_pMaterialSystem->GetCustomMaterialManager()->GetOrCreateCustomMaterial( pVMTKeyValues, vecTextureInfo, bIgnorePicMip );
				delete pVMTKeyValues; // copied inside GetCustomMaterial, no longer needed

				if ( pCustomMaterial )
				{
					SetCustomMaterial( pCustomMaterial, nModelMaterialIndex );
					bCreatedMaterial = true;
				}
			}
		}

		pVisualsDataProcessor->Release();
	}

	mdlcache->Release( modelHandle );
}
#endif

#ifdef CLIENT_DLL

ConVar econ_enable_inventory_images( "econ_enable_inventory_images", "1", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY, "allow inventory image rendering for use by scaleform" );
ConVar econ_inventory_image_pinboard( "econ_inventory_image_pinboard", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY );

void CEconItemView::Update( void )
{
	if ( !econ_enable_inventory_images.GetBool() || g_pRenderToRTHelper == NULL )
		return;

	// if an async load from cache is in progress, just return
	if ( m_asyncFixupState == AFS_LoadingInProgress )
		return;

	// if we need to render our inventory image rgba, then attempt to now
	if ( m_bInventoryImageRgbaRequested && m_pRenderToRTData == NULL && m_inventoryImageRgba.TellPut() == 0 )
	{
		if ( !m_bInventoryImageTriedCache )
		{
			// See if we've already got it on disc
			if ( LoadCachedInventoryImage() )
			{
				return;
			}
			m_bInventoryImageTriedCache = true;
		}

		UpdateGeneratedMaterial( true );

		// check to see if we can render now (custom materials ready?)
		for ( int i = 0; i < GetCustomMaterialCount(); i++ )
		{
			if ( GetCustomMaterial( i ) && !GetCustomMaterial( i )->IsValid() )
			{
				InventoryManager()->InsertMaterialGenerationJob( this );
				return;
			}
		}

		m_pScratchVTF = CreateVTFTexture();
		
		m_pRenderToRTMDL = new CMergedMDL;
		m_pRenderToRTMDL->SetMDL( GetIconDisplayModel(), this );

		m_pRenderToRTData = g_pRenderToRTHelper->CreateRenderToRTData( m_pRenderToRTMDL, m_pScratchVTF );

		m_pRenderToRTData->m_pszIconNameSuffix = GetStaticData()->GetDefinitionName();

		const InventoryImageData_t *pInventoryImageData = GetStaticData()->GetInventoryImageData();
		if ( pInventoryImageData )
		{
			if ( econ_inventory_image_pinboard.GetBool() )
			{
				// Force the camera to a nice position for the pinboard
				m_pRenderToRTData->m_cameraAngles = QAngle( 0.0f, -90.0f, 0.0f );
				m_pRenderToRTData->m_cameraOffset = Vector( -25.0f, 0.0f, 0.0f );
			}
			else
			{
				matrix3x4_t matCamera;
				m_pRenderToRTMDL->SetupBonesForAttachmentQueries();
				if ( m_pRenderToRTMDL->GetAttachment( "camera_inventory", matCamera ) )
				{
					MatrixAngles( matCamera, m_pRenderToRTData->m_cameraAngles, m_pRenderToRTData->m_cameraOffset );
					m_pRenderToRTData->m_bUsingExplicitModelCameraPosAnglesFromAttachment = true;
				}
				else
				{
					if ( pInventoryImageData->m_pCameraAngles )
					{
						m_pRenderToRTData->m_cameraAngles = *( pInventoryImageData->m_pCameraAngles );
					}
					if ( pInventoryImageData->m_pCameraOffset )
					{
						m_pRenderToRTData->m_cameraOffset = *( pInventoryImageData->m_pCameraOffset );
					}
				}
			}

			if ( pInventoryImageData->m_cameraFOV != -1.0f )
			{
				m_pRenderToRTData->m_cameraFOV = pInventoryImageData->m_cameraFOV;
			}
			int nLightDescIndex = ( pInventoryImageData->m_bOverrideDefaultLight ) ? 0 : m_pRenderToRTData->m_LightingState.m_nLocalLightCount;
			for ( int i = 0; i < MATERIAL_MAX_LIGHT_COUNT; i++ )
			{
				if ( pInventoryImageData->m_pLightDesc[i] && nLightDescIndex < MATERIAL_MAX_LIGHT_COUNT )
				{
					m_pRenderToRTData->m_LightingState.m_pLocalLightDesc[ nLightDescIndex ] = *( pInventoryImageData->m_pLightDesc[ i ] );
					nLightDescIndex++;
				}
			}
			m_pRenderToRTData->m_LightingState.m_nLocalLightCount = nLightDescIndex;
		}
		g_pRenderToRTHelper->StartRenderToRT( m_pRenderToRTData );
	}
	
	if ( m_pRenderToRTData && m_pRenderToRTData->m_stage == RENDER_TO_RT_STAGE_DONE )
	{
		unsigned char *pSourceImage = m_pScratchVTF->ImageData( 0, 0, 0 );
		
		// copy out the result into our buffer, allowing less than the full height (we use 4:3 icon images in CS:GO)
		m_inventoryImageRgba.Purge();
		if ( m_nInventoryImageRgbaWidth == m_pScratchVTF->Width() && m_nInventoryImageRgbaHeight <= m_pScratchVTF->Height() )
		{
			m_inventoryImageRgba.EnsureCapacity( m_nInventoryImageRgbaWidth * m_nInventoryImageRgbaHeight * 4 );
			m_inventoryImageRgba.Put( pSourceImage, m_nInventoryImageRgbaWidth * m_nInventoryImageRgbaHeight * 4 );
		}
		else
		{
			// this normally shouldn't occur, but if it did then it's likely the requested width doesn't match the RenderToRtHelper RT width (currently that's 256)
			Assert(0);
		}

		// call back to update the image in the UI
		if ( m_pImageReadyCallback )
		{
			uint64 nItemID = GetItemID();
			if ( !nItemID )
				nItemID = CombinedItemIdMakeFromDefIndexAndPaint( GetItemDefinition()->GetDefinitionIndex(), GetCustomPaintKitIndex() );

			// Request to save the buffer before calling the callback - that way the callback 
			// can modify m_inventoryImageRgba, such as swapping channels (ef when wrting the PNG file),
			// without affecting the cached image
			SaveInventoryImage( m_inventoryImageRgba );

			m_pImageReadyCallback( this, m_inventoryImageRgba, m_nInventoryImageRgbaWidth, m_nInventoryImageRgbaHeight, nItemID );
			m_inventoryImageRgba.Purge(); // no longer need to keep this buffer around
			m_bInventoryImageRgbaRequested = false; // clear the requested flag, so that it can be requested again
			m_bInventoryImageTriedCache = false; // clear tried cache flag, since it's now in the cache and we want it to get from there next time.
		}

		g_pRenderToRTHelper->DestroyRenderToRTData( m_pRenderToRTData );
		m_pRenderToRTData = NULL;
		delete m_pRenderToRTMDL;
		m_pRenderToRTMDL = NULL;
		DestroyVTFTexture( m_pScratchVTF );
		m_pScratchVTF = NULL;

		// No longer need the custom material - inventory image has been generated !
		ClearCustomMaterials();

		return;
	}

	if ( m_inventoryImageRgba.TellPut() == 0 && m_bInventoryImageRgbaRequested )
	{
		InventoryManager()->InsertMaterialGenerationJob( this );
	}
}

static float g_pAttribWearMins[ 3 ] = { 0.00f, 0.20f, 0.56f };
static float g_pAttribWearMaxs[ 3 ] = { 0.07f, 0.48f, 1.00f };
char g_szGeneratingTradingIconPath[ MAX_PATH ] = "";

void BuildInventoryImagePath( char *pchOutfile, int nMaxPath, const char *pchDefName, const char *pchPaintKitName, bool bWear, float flWear, bool bLarge )
{
	const char *pchOutputFolder = "resource/Flash/econ/default_generated/";
	if ( g_szGeneratingTradingIconPath[ 0 ] )
	{
		pchOutputFolder = g_szGeneratingTradingIconPath;
	}

	if ( bWear )
	{
		V_snprintf( pchOutfile, nMaxPath, "%s%s_%s_%s%s.png", 
			pchOutputFolder, 
			pchDefName, 
			pchPaintKitName, 
			( flWear >= g_pAttribWearMins[ 2 ] ? "heavy" : ( flWear <= g_pAttribWearMaxs[ 0 ] ? "light" : "medium" ) ),
			bLarge ? "_large" : "" );
	}
	else
	{
		//if reading the wear attribute fails, don't try to pack any attributes into the output image name
		if ( !pchPaintKitName )
		{
			V_snprintf( pchOutfile, nMaxPath, "%s%s%s.png", 
				pchOutputFolder, pchDefName, bLarge ? "_large" : "" );
		}
		else
		{
			V_snprintf( pchOutfile, nMaxPath, "%s%s_%s%s.png", 
				pchOutputFolder, pchDefName, pchPaintKitName, bLarge ? "_large" : "" );
		}
	}
}

void SaveAndAddToP4( char *outfile, CUtlBuffer &outBuffer, bool bSkipAdd )
{
	bool bNewFile = false;

	FileHandle_t hFileOut = g_pFullFileSystem->Open( outfile, "rb" );
	if ( hFileOut != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Close( hFileOut );
	}
	else
	{
		bNewFile = true;
	}

	hFileOut = g_pFullFileSystem->Open( outfile, "wb" );
	if ( hFileOut != FILESYSTEM_INVALID_HANDLE )
	{
		g_pFullFileSystem->Write( outBuffer.Base(), outBuffer.TellPut(), hFileOut );
		g_pFullFileSystem->Close( hFileOut );
	}

	if ( bNewFile && !bSkipAdd )
	{
		char szFullIconPath[ MAX_PATH ];
		g_pFullFileSystem->RelativePathToFullPath( outfile, "MOD", szFullIconPath, sizeof( szFullIconPath ) );

		if ( !p4->IsFileInPerforce( szFullIconPath ) )
		{
			DevMsg( "P4 Add: %s\n", outfile );

			g_p4factory->SetDummyMode( p4 == NULL );
			g_p4factory->SetOpenFileChangeList( "Item Icons Auto Checkout" );
			CP4AutoAddFile autop4_add( szFullIconPath );
		}
		else
		{
			DevMsg( "File Already in P4: %s\n", szFullIconPath );
		}
	}
}

void InventoryImageReadyCallback( const CEconItemView *pItemView, CUtlBuffer &rawImageRgba, int nWidth, int nHeight, uint64 nItemID )
{
	const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( pItemView->GetStaticData()->GetDefinitionIndex() );
	if ( !pItemDef )
		return;

	// It's actually BGRA! Swap B and R to get RGBA!
	for ( int i = 0; i < rawImageRgba.Size(); i += 4 )
	{
		if ( i + 3 > rawImageRgba.Size() )
			break;

		unsigned char *pCurrent = reinterpret_cast< unsigned char* >( rawImageRgba.Base() ) + i;
		unsigned char chTemp = *pCurrent;
		*pCurrent = *( pCurrent + 2 );
		*( pCurrent + 2 ) = chTemp;
	}

	CUtlBuffer outBuffer;
	ImgUtl_WriteRGBAAsPNGToBuffer( reinterpret_cast< const unsigned char *>( rawImageRgba.Base() ), nWidth, nHeight, outBuffer );

	static CSchemaAttributeDefHandle pAttr_PaintKitWear( "set item texture wear" );

	float flWear = 0.0f;
	bool bHasWear = FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pItemView, pAttr_PaintKitWear, &flWear );

	char outfile[ MAX_PATH ];
	BuildInventoryImagePath( outfile, sizeof( outfile ), 
							 pItemView->GetStaticData()->GetDefinitionName(), 
							 pItemView->GetCustomPaintKitIndex() == 0 ? NULL : pItemView->GetCustomPaintKit()->sName.String(),
							 bHasWear, flWear,
							 false );

	SaveAndAddToP4( outfile, outBuffer, econ_inventory_image_pinboard.GetBool() );

	if ( g_szGeneratingTradingIconPath[ 0 ] && !econ_inventory_image_pinboard.GetBool() )
	{
		BuildInventoryImagePath( outfile, sizeof( outfile ), 
			pItemView->GetStaticData()->GetDefinitionName(), 
			pItemView->GetCustomPaintKitIndex() == 0 ? NULL : pItemView->GetCustomPaintKit()->sName.String(),
			bHasWear, flWear,
			true );

		SaveAndAddToP4( outfile, outBuffer, false );
	}
}

void CEconItemView::SaveInventoryImageAsPNG( int nWidth, int nHeight )
{
	GetInventoryImageRgba( nWidth, nHeight, InventoryImageReadyCallback );

	InventoryManager()->InsertMaterialGenerationJob( this );
}

struct KeyValueWith64BitID_t
{
	uint64 nID;
	KeyValues *pKV;
};

class CIconKeyValuesLess
{
public:
	bool Less( KeyValueWith64BitID_t * const & src1, KeyValueWith64BitID_t * const & src2, void *pCtx )
	{
		return ( src1->nID < src2->nID );
	}
};

const CUtlBuffer* CEconItemView::GetInventoryImageRgba( int nWidth, int nHeight, ImageReadyCallback_t pImageReadyCallback )
{
	if ( !econ_enable_inventory_images.GetBool() )
	{
		m_inventoryImageRgba.EnsureCapacity( nWidth * nHeight * sizeof( int ) );
		// just make a blank image of the size requested
		for ( int i = 0; i < nWidth * nHeight; i++ )
		{
			m_inventoryImageRgba.PutInt( 0 );
		}
		return &m_inventoryImageRgba;
	}

	if ( !m_bInventoryImageRgbaRequested )
	{
		InventoryManager()->InsertMaterialGenerationJob( this );
	}

	m_bInventoryImageRgbaRequested = true;

	if ( ( nWidth != m_nInventoryImageRgbaWidth ) || ( nHeight != m_nInventoryImageRgbaHeight ) )
	{
		m_inventoryImageRgba.Purge();
		m_nInventoryImageRgbaWidth = nWidth;
		m_nInventoryImageRgbaHeight = nHeight;
	}

	if ( m_inventoryImageRgba.TellPut() == 0 )
	{
		Assert( !m_pImageReadyCallback || ( m_pImageReadyCallback == pImageReadyCallback ) ); // only support generating one image at a time
		m_pImageReadyCallback = pImageReadyCallback;
	}	

	return &m_inventoryImageRgba;
}

bool CEconItemView::CanGenerateInventoryImageRgba()
{
	// we can currently only generate images for weapons and gloves
	// when that changes, we need to update this code.
	if ( econ_enable_inventory_images.GetBool() )
	{
		if ( GetStaticData() )
		{
			const char *pItemClass = GetStaticData()->GetItemClass(); 
			if ( pItemClass && V_strnicmp( pItemClass, "weapon_", 7 ) == 0 )
			{
				return true;
			}

			const char* szSubPos = GetStaticData()->GetRawDefinition()->GetString( "item_sub_position", NULL );
			if ( szSubPos && V_stricmp( "clothing_hands", szSubPos ) == 0 )
			{
				return !GetItemDefinition()->IsDefaultSlotItem();
			}
		}
#if 0 // Relies on prebuilt maps of item tags which isn't in staging yet
		if ( HasTag( "Weapon" ) )
		{
			return true;
		}
		if ( HasTag( "Hands" ) )
		{
			return true;
		}
#endif
	}

	return false;
}

void CEconItemView::ClearInventoryImageRgba()
{
	m_inventoryImageRgba.Purge();

	int nItemIdHigh = m_iItemIDHigh.Get();
	int nItemIdLow = m_iItemIDLow.Get();

	if ( m_iItemID == 0 )
	{
		nItemIdHigh = 0xFFFFFFFF;
		nItemIdLow = ( GetItemDefinition()->GetDefinitionIndex() << 16 );

		nItemIdLow += GetCustomPaintKitIndex();
	}

	char szFileName[ MAX_PATH ];
	V_snprintf( szFileName, sizeof( szFileName ), ECON_ITEM_GENERATED_ICON_DIR "%X-%X-%X.iic", ECON_ITEM_ICON_CACHE_VERSION, nItemIdHigh, nItemIdLow );
	if ( g_pFullFileSystem->FileExists( szFileName ) )
		g_pFullFileSystem->RemoveFile( szFileName );

	InventoryManager()->InsertMaterialGenerationJob( this );
}

static void AsyncLoadCachedIntevtoryImageCallback( const FileAsyncRequest_t &request, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	CEconItemView *pEconItemView = reinterpret_cast< CEconItemView * >( request.pContext );
	if ( pEconItemView )
	{
		pEconItemView->m_nNumAsyncReadBytes = numReadBytes;
		pEconItemView->m_asyncStatus = asyncStatus;
		pEconItemView->m_asyncFixupState = CEconItemView::AFS_LoadingDone;
		g_InventoryItemUpdateManager.AddItemViewToFixupList( pEconItemView );
	}
}

void CEconItemView::FinishLoadCachedInventoryImage( void* pData, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	if ( asyncStatus == FSASYNC_OK )
	{
		if ( m_pImageReadyCallback )
		{
			uint64 nItemID = GetItemID();
			if ( !nItemID )
			{
				nItemID = CombinedItemIdMakeFromDefIndexAndPaint( GetItemDefinition()->GetDefinitionIndex(), GetCustomPaintKitIndex() );
			}

			m_pImageReadyCallback( this, m_inventoryImageRgba, m_nInventoryImageRgbaWidth, m_nInventoryImageRgbaHeight, nItemID );
		}
		m_bInventoryImageRgbaRequested = false;	// clear the requested flag, so that it can be requested again
	}
	else
	{
		m_bInventoryImageTriedCache = true;
		InventoryManager()->InsertMaterialGenerationJob( this );
	}

	m_inventoryImageRgba.Purge(); // no longer need to keep this buffer around

	m_asyncFixupState = AFS_FixupDone;

	// it's possible to get here without m_hAsyncControl being set when async mode is not on.
	if ( m_hAsyncControl )
	{
		filesystem->AsyncRelease( m_hAsyncControl );
		m_hAsyncControl = NULL;
	}
}

bool CEconItemView::LoadCachedInventoryImage( void )
{
	if ( !m_pImageReadyCallback )
		return false;

	// already Async Loading
	if ( m_asyncFixupState == AFS_LoadingInProgress )
		return true;

	CleanInventoryImageCacheDir();

	GenerateCachedInventoryImageName();

	const int nBuffSize = ECON_ITEM_GENERATED_ICON_WIDTH * ECON_ITEM_GENERATED_ICON_HEIGHT * 4;

	m_inventoryImageRgba.EnsureCapacity( nBuffSize );

	// Already pending
	Assert ( !m_hAsyncControl );

	m_inventoryImageRgba.SeekPut( CUtlBuffer::SEEK_HEAD, nBuffSize );

	// async load the file	
	FileAsyncRequest_t fileRequest;
	fileRequest.pContext    = (void *)this;
	fileRequest.pfnCallback = ::AsyncLoadCachedIntevtoryImageCallback;
	fileRequest.pData       = m_inventoryImageRgba.Base();
	fileRequest.pszFilename = m_szCurrentLoadCachedFileName;
	fileRequest.priority    = -1;
		
	// queue for async load
	filesystem->AsyncRead( fileRequest, &m_hAsyncControl );
	// it's possible to get here without m_hAsyncControl being set when async mode is not on.
	if ( m_hAsyncControl )
	{
		m_asyncFixupState = AFS_LoadingInProgress;
	}
	return true;
}

void CEconItemView::SaveInventoryImage( CUtlBuffer &rawImageRgba )
{
	GenerateCachedInventoryImageName();

	CUtlBuffer *pBuffer = new CUtlBuffer();
	pBuffer->EnsureCapacity( rawImageRgba.TellPut() );
	pBuffer->CopyBuffer( rawImageRgba );
	FSAsyncStatus_t status = g_pFullFileSystem->AsyncWriteFile( m_szCurrentLoadCachedFileName, pBuffer, pBuffer->TellPut(), true );
	if ( status < FSASYNC_OK )
	{
		Msg( "EconItemView: Failed to write cache file %s\n", m_szCurrentLoadCachedFileName );
	}
}

void CEconItemView::GenerateCachedInventoryImageName()
{
	int nItemIdHigh = m_iItemIDHigh.Get();
	int nItemIdLow = m_iItemIDLow.Get();

	if (m_iItemID == 0)
	{
		nItemIdHigh = 0xFFFFFFFF;
		nItemIdLow = (GetItemDefinition()->GetDefinitionIndex() << 16);
		nItemIdLow += GetCustomPaintKitIndex();

		// Get the wear attribute
		static CSchemaAttributeDefHandle pAttr_PaintKitWear( "set item texture wear" );

		float flWear = 0.0f;
		bool bHasWear = FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttr_PaintKitWear, &flWear );

		if ( bHasWear )
		{
			int nItemWear = ( flWear >= g_pAttribWearMins[ 2 ] ) ? 2 : ( flWear <= g_pAttribWearMaxs[ 0 ] ? 0 : 1 );
			V_snprintf( m_szCurrentLoadCachedFileName, sizeof( m_szCurrentLoadCachedFileName ), ECON_ITEM_GENERATED_ICON_DIR "%X-%X-%X-%X.iic", ECON_ITEM_ICON_CACHE_VERSION, nItemIdHigh, nItemIdLow, nItemWear );
		}
		else
		{
			V_snprintf( m_szCurrentLoadCachedFileName, sizeof( m_szCurrentLoadCachedFileName ), ECON_ITEM_GENERATED_ICON_DIR "%X-%X-%X.iic", ECON_ITEM_ICON_CACHE_VERSION, nItemIdHigh, nItemIdLow );
		}
	}
	else
	{
		V_snprintf( m_szCurrentLoadCachedFileName, sizeof( m_szCurrentLoadCachedFileName ), ECON_ITEM_GENERATED_ICON_DIR "%X-%X-%X.iic", ECON_ITEM_ICON_CACHE_VERSION, nItemIdHigh, nItemIdLow );
	}
}

void CEconItemView::CleanInventoryImageCacheDir( void )
{
	// Only do this check once per session
	// and ensure that we've loaded at least one item into the inventory before doing the check
	if ( m_sbHasCleanedInventoryImageCacheDir || InventoryManager()->GetLocalInventory()->GetItemCount() == 0 )
		return;

	// Only do this once per session!
	m_sbHasCleanedInventoryImageCacheDir = true;

	// Loop through all the cache files in the cache dir and delete ones that are an older version
	FileFindHandle_t hSearchFile;

	char szPath[ 512 ];
	Q_strncpy( szPath, ECON_ITEM_GENERATED_ICON_DIR "*.iic", sizeof( szPath ) );

	//char szCorrectPrefix[ MAX_PATH ];
	//V_snprintf( szCorrectPrefix, sizeof( szCorrectPrefix ), "%X-", ECON_ITEM_ICON_CACHE_VERSION );

	const char *pchFileName = g_pFullFileSystem->FindFirstEx( szPath, "MOD", &hSearchFile );
	if ( pchFileName )
	{
		while ( pchFileName )
		{
			// read in exactly 3 numbers from the name
			uint32 nVersion = 0;
			uint32 nItemIdHigh = 0;
			uint32 nItemIdLow = 0;
			uint32 nItemWear = 0;
			int nScanResult = sscanf( pchFileName, "%X-%X-%X-%X.iic", &nVersion, &nItemIdHigh, &nItemIdLow, &nItemWear );

			bool bWipeIt = ( nScanResult != 3 ) && ( nScanResult != 4 );

			if ( !bWipeIt )
			{
				if ( nVersion != ECON_ITEM_ICON_CACHE_VERSION )
				{
					bWipeIt = true;
				}
				else if ( nItemIdHigh != 0xFFFFFFFF )
				{
					bWipeIt = ( nScanResult != 3 ); // don't expect to have the wear in this case (item coming from the inventory)
					if ( !bWipeIt )
					{
						// If we don't still have that item wipe it!
						itemid_t ullItem = ( static_cast< uint64 >( nItemIdHigh ) << 32 ) + static_cast< uint64 >( nItemIdLow ) ;
						bWipeIt = !InventoryManager()->GetLocalInventory()->GetInventoryItemByItemID( ullItem );
					}
				}
			}

			if ( bWipeIt )
			{
				char szFileName[ MAX_PATH ];
				V_snprintf( szFileName, sizeof( szFileName ), ECON_ITEM_GENERATED_ICON_DIR "%s", pchFileName );
				g_pFullFileSystem->RemoveFile( szFileName );
			}

			pchFileName = g_pFullFileSystem->FindNext( hSearchFile );
		}

		g_pFullFileSystem->FindClose( hSearchFile );
	}
}

#endif

void CEconItemView::InitNetworkedDynamicAttributesForDemos( void )
{
#if defined( ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS )
	if ( !GetSOCData() )
		return;

	m_NetworkedDynamicAttributesForDemos.DestroyAllAttributes();

	bool bAdded = false;

	const uint32 nNumAttributes = GetSOCData()->m_pCustomDataOptimizedObject ? GetSOCData()->m_pCustomDataOptimizedObject->m_numAttributes : 0;
	for ( uint32 nAttr = 0; nAttr < nNumAttributes; nAttr++ )
	{
		const CEconItem::attribute_t &attr = * GetSOCData()->m_pCustomDataOptimizedObject->GetAttribute( nAttr );
		const CEconItemAttributeDefinition *pDef = GetItemSchema()->GetAttributeDefinition( attr.m_unDefinitionIndex );
		if ( !pDef )
			continue;

		const ISchemaAttributeType *pAttributeType = pDef->GetAttributeType();
		if ( pAttributeType && !pAttributeType->BSupportsGameplayModificationAndNetworking() )
			continue;

		float flValue = 0.0f;
		if ( pDef->IsStoredAsFloat() )
		{
			flValue = attr.m_value.asFloat;
		}
		else
		{
			flValue = *reinterpret_cast< const float* >( &attr.m_value.asUint32 );
		}

		CEconItemAttribute attribute( attr.m_unDefinitionIndex, flValue );
		m_NetworkedDynamicAttributesForDemos.AddAttribute( &attribute );

		bAdded = true;
	}
	
	if ( bAdded )
	{
		NetworkStateChanged();
	}
#endif // #ifdef ECON_NETWORK_ATTRIBUTES
}

void CEconItemView::UpdateNetworkedDynamicAttributesForDemos( attrib_definition_index_t nDef, float flNewValue )
{
	for( int nAttr = 0; nAttr < m_NetworkedDynamicAttributesForDemos.GetNumAttributes(); nAttr++ )
	{
		CEconItemAttribute *pAttribute = m_NetworkedDynamicAttributesForDemos.GetAttribute( nAttr );
		if ( !pAttribute || pAttribute->GetAttribIndex() != nDef )
			continue;

		CEconItemAttribute attribute( nDef, flNewValue );
		*pAttribute = attribute;

		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemView::AddAttribute( CEconItemAttribute *pAttribute )
{
	m_AttributeList.AddAttribute( pAttribute );
#ifdef ECON_NETWORK_ATTRIBUTES
	NetworkStateChanged();
#endif // #ifdef ECON_NETWORK_ATTRIBUTES
	MarkDescriptionDirty();
}

//-----------------------------------------------------------------------------
// Purpose: Remove an attribute by name
//-----------------------------------------------------------------------------
void CEconItemView::SetOrAddAttributeValueByName( const char *pszAttribDefName, float flValue )
{
	m_AttributeList.SetOrAddAttributeValueByName( pszAttribDefName, flValue );
	MarkDescriptionDirty();
}

//-----------------------------------------------------------------------------
// Purpose: Remove an attribute by name
//-----------------------------------------------------------------------------
void CEconItemView::RemoveAttribute( const char *pszAttribDefName )
{
	m_AttributeList.RemoveAttribute( pszAttribDefName );
	MarkDescriptionDirty();
}

const bool CEconItemView::GetCombinedAttributeClassValue( float &flValue, string_t iszAttribClass ) const
{
	for( int i = 0; i < m_AttributeList.m_Attributes.Count(); i++ )
	{
		const CEconItemAttribute *pAttrib = &(m_AttributeList.m_Attributes[ i ]);
		if ( pAttrib )
		{
			const CEconItemAttributeDefinition *pStaticData = pAttrib->GetStaticData();

			if ( pStaticData->GetCachedClass() == iszAttribClass )
			{
				switch ( pStaticData->GetDescriptionFormat() )
				{
				case ATTDESCFORM_VALUE_IS_PERCENTAGE:
				case ATTDESCFORM_VALUE_IS_INVERTED_PERCENTAGE:
					{
						flValue *= pAttrib->GetValue();
					}
					return true;

				case ATTDESCFORM_VALUE_IS_ADDITIVE:
				case ATTDESCFORM_VALUE_IS_ADDITIVE_PERCENTAGE:
					{
						flValue += pAttrib->GetValue();
					}
					return true;

				case ATTDESCFORM_VALUE_IS_REPLACE:
					{
						flValue = pAttrib->GetValue();
					}
					return true;

				case ATTDESCFORM_VALUE_IS_OR:
					{
						int iTmp = flValue;
						iTmp |= (int)pAttrib->GetValue();
						flValue = iTmp;
					}
					return true;

				default:
					// Unknown value format. 
					Assert( 0 );
					return true;
				}
			}
		}
	}

	return false;

}

//-----------------------------------------------------------------------------
// Purpose: Returns the attribute matching the specified class, carried by this entity, if it exists.
//-----------------------------------------------------------------------------
CEconItemAttribute *CEconItemView::GetAttributeByClass( const char *szAttribClass )
{
	return m_AttributeList.GetAttributeByClass( szAttribClass );
}
const CEconItemAttribute *CEconItemView::GetAttributeByClass( const char *szAttribClass ) const
{
	return m_AttributeList.GetAttributeByClass( szAttribClass );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the attribute that matches the attribute def index, if it exists
//-----------------------------------------------------------------------------
CEconItemAttribute *CEconItemView::GetAttributeByDefIndex( int iAttributeDefIndex )
{
	return m_AttributeList.GetAttributeByDefIndex( iAttributeDefIndex );
}

const CEconItemAttribute *CEconItemView::GetAttributeByDefIndex( int iAttributeDefIndex ) const
{
	return m_AttributeList.GetAttributeByDefIndex( iAttributeDefIndex );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the attribute that matches the def name, if it exists
//-----------------------------------------------------------------------------
CEconItemAttribute *CEconItemView::GetAttributeByName( const char *pszAttribDefName )
{
	return m_AttributeList.GetAttributeByName( pszAttribDefName );
}

const CEconItemAttribute *CEconItemView::GetAttributeByName( const char *pszAttribDefName ) const
{
	return m_AttributeList.GetAttributeByName( pszAttribDefName );
}

extern const char *g_EffectTypes[NUM_EFFECT_TYPES];

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
const wchar_t *CEconItemView::GetItemName( bool bUncustomized /*= false*/ ) const
{
	static const wchar_t *pwzDefaultName = L"";
	/** Removed for partner depot **/
	return pwzDefaultName;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Get RGB modifying attribute value
//-----------------------------------------------------------------------------
int CEconItemView::GetModifiedRGBValue( bool bAltColor )
{
	enum
	{
		kPaintConstant_Default = 0,
		kPaintConstant_OldTeamColor = 1,
	};

	static CSchemaAttributeDefHandle pAttr_Paint( "set item tint rgb" );
	static CSchemaAttributeDefHandle pAttr_Paint2( "set item tint rgb 2" );

	// If we have no base paint color we don't do anything special.
	float fRGB;
	if ( !FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttr_Paint, &fRGB ) )
		return kPaintConstant_Default;

	// See if we also have a secondary paint color.
	uint32 unRGB = (uint32)fRGB,
		   unRGBAlt;
	float fRGBAlt;

	// Backwards compatibility for old team colored items.
	if ( unRGB == kPaintConstant_OldTeamColor )
	{
		unRGB = RGB_INT_RED;
		unRGBAlt = RGB_INT_BLUE;
	}
	else if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttr_Paint2, &fRGBAlt ) )
	{
		unRGBAlt = (uint32)fRGBAlt;
	}
	else
	{
		// By default our secondary color will match our primary if we can't find a replacement.
		unRGBAlt = unRGB;
	}

	return bAltColor ? unRGBAlt : unRGB;
}

int CEconItemView::GetCustomPaintKitIndex( void ) const
{
	static CSchemaAttributeDefHandle pAttr_PaintKit( "set item texture prefab" );

	// If we have no base paint kit color we don't do anything special.
	float flIndex;
	if ( !FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttr_PaintKit, &flIndex ) )
	{
		return 0;
	}

	return flIndex;
}

const char* CEconItemView::GetCustomPaintKitDbgName( void ) const
{
	int idx = GetCustomPaintKitIndex();
	const CPaintKit* pKit = GetItemSchema()->GetPaintKitDefinition( idx );
	return pKit ? pKit->sName.Get() : "<UNPAINTED>";
}

bool CEconItemView::IsStyleUnlocked( int iStyle ) const
{
	const CEconStyleInfo* pStyle = GetItemDefinition()->GetStyleInfo( iStyle );
	if ( pStyle && !pStyle->GetUnlockInfo()->IsLockable() )
		return true;

	// Do we have the style unlocked?
	static CSchemaAttributeDefHandle pAttr_UnlockedStyles( "unlocked styles" );

	uint32 nUnlockedStyles;
	if ( FindAttribute( pAttr_UnlockedStyles, &nUnlockedStyles ) )
	{
		if ( nUnlockedStyles & (1 << iStyle) )
		{
			return true;
		}
	}

	return false;
}

bool CEconItemView::CanCollect( CEconItemView &subject )
{
#if 0
	int nItemSet = GetItemSetIndex();
	if ( nItemSet < 0 )
		return false;

	CEconItemSchema *pSchema = GetItemSchema();
	if ( !pSchema )
		return false;

	const CEconItemSetDefinition *pItemSet = pSchema->GetItemSetByIndex( nItemSet );
	if ( !pItemSet )
		return false;

	if ( !pItemSet->m_bIsCollection )
		return false;

	// Check and see if subject is in our item set.
	FOR_EACH_VEC( pItemSet->m_ItemEntries, i )
	{
		unsigned int iIndex = pItemSet->m_ItemEntries[i].m_nItemDef;

		const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( iIndex );
		if ( !pItemDef )
			continue;

		if ( subject.GetStaticData()->GetDefinitionIndex() != pItemDef->GetDefinitionIndex() )
			continue;

		// Check and see if this item is already collected.
		CEconItem* pItem = GetSOCData();
		if ( !pItem )
			continue;

		const CEconItemAttributeDefinition* pCollectionAttrib = GetItemSchema()->GetAttributeDefinitionByName( "collection bits" );
		if ( !pCollectionAttrib )
			continue;

		float flValue = 0;
		pItem->HasCustomAttribute( pCollectionAttrib->GetDefinitionIndex(), &flValue );
		uint32 iCollectionBits = *(int *) &flValue;

		if ( iCollectionBits & (1 << i) )
		{
			return false;
		}
		else
		{
			return true;
		}
	}
#endif

	return false;
}

uint64 CEconItemView::GetCustomUserTextureID()
{
	static CSchemaAttributeDefHandle pAttr_CustomTextureLo( "custom texture lo" );
	static CSchemaAttributeDefHandle pAttr_CustomTextureHi( "custom texture hi" );

	uint32 unLowVal, unHighVal;
	const bool bHasLowVal = FindAttribute( pAttr_CustomTextureLo, &unLowVal ),
			   bHasHighVal = FindAttribute( pAttr_CustomTextureHi, &unHighVal );

	// We should have both, or neither.  We should never have just one
	Assert( bHasLowVal == bHasHighVal );

	if ( bHasLowVal && bHasHighVal )
	{
		return ((uint64)unHighVal << 32) | (uint64)unLowVal;
	}

	// No attribute set
	return 0;
}

const CPaintKit *CEconItemView::GetCustomPaintKit( void ) const
{
	int nPaintKit = GetCustomPaintKitIndex();
	return GetItemSchema()->GetPaintKitDefinition( nPaintKit );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAttributeList::CAttributeList()
{
#if defined(CLIENT_DLL) || defined(GAME_DLL)
	m_pManager = NULL;
#endif
	m_Attributes.Purge();
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::SetManager( CAttributeManager *pManager )
{
	m_pManager = pManager;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::Init()
{
	m_Attributes.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::IterateAttributes( class IEconItemAttributeIterator *pIterator )
{
	Assert( pIterator );

	FOR_EACH_VEC( m_Attributes, i )
	{
		CEconItemAttribute *pAttrInst = &m_Attributes[i];

		const CEconItemAttributeDefinition *pAttrDef = pAttrInst->GetStaticData();
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );
		Assert( pAttrType->BSupportsGameplayModificationAndNetworking() );

		attribute_data_union_t value;
		value.asFloat = pAttrInst->GetValue();

		if ( !pAttrType->OnIterateAttributeValue( pIterator, pAttrDef, value ) )
			return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::DestroyAllAttributes( void )
{
	if ( m_Attributes.Count() )
	{
		m_Attributes.Purge();
		UpdateManagerCache();
#if defined( ECON_NETWORK_ATTRIBUTES ) || defined( ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS ) 
		NetworkStateChanged();
#endif // #if defined( ECON_NETWORK_ATTRIBUTES ) || defined( ECON_NETWORK_DYNAMIC_ATTRIBUTES_FOR_DEMOS ) 
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::AddAttribute( CEconItemAttribute *pAttribute )
{
	Assert( pAttribute );

	// Only add attributes to the attribute list if they have a definition we can
	// pull data from.
	if ( !pAttribute->GetStaticData() )
		return;

	m_Attributes.AddToTail( *pAttribute );
#ifdef ECON_NETWORK_ATTRIBUTES
	NetworkStateChanged();
#endif // #ifdef ECON_NETWORK_ATTRIBUTES

	UpdateManagerCache();
}

//-----------------------------------------------------------------------------
// Purpose: Remove an attribute by name
//-----------------------------------------------------------------------------
void CAttributeList::SetOrAddAttributeValueByName( const char *pszAttribDefName, float flValue )
{
	// This should already be in the string table
	const CEconItemAttributeDefinition *pDef = GetItemSchema()->GetAttributeDefinitionByName( pszAttribDefName );
	if ( !pDef )
		return;

	int iAttributes = GetNumAttributes();
	for ( int i = 0; i < iAttributes; i++ )
	{
		CEconItemAttribute *pAttribute = GetAttribute(i);

#if defined(CLIENT_DLL) || defined(GAME_DLL)
		// Never combine into set bonus attributes, they're recreated regularly
		if ( pAttribute->m_bSetBonus )
			continue;
#endif

		if ( pAttribute->GetAttribIndex() == pDef->GetDefinitionIndex() )
		{
			pAttribute->SetValue( flValue );
			return;
		}
	}

	// Didn't find it. Add a new one.
	CEconItemAttribute attribute( pDef->GetDefinitionIndex(), flValue );
	AddAttribute( &attribute );
}

//-----------------------------------------------------------------------------
// Purpose: Remove an attribute by name
//-----------------------------------------------------------------------------
void CAttributeList::RemoveAttribute( const char *pszAttribDefName )
{
	int iAttributes = m_Attributes.Count();
	for ( int i = 0; i < iAttributes; i++ )
	{
		if ( !strcmp( m_Attributes[i].GetStaticData()->GetDefinitionName(), pszAttribDefName ) )
		{
			m_Attributes.Remove( i );
			UpdateManagerCache();
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove an attribute by index
//-----------------------------------------------------------------------------
void CAttributeList::RemoveAttributeByIndex( int iIndex )
{
	if ( iIndex < 0 || iIndex >= GetNumAttributes() )
		return;

	m_Attributes.Remove( iIndex );
	UpdateManagerCache();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the attribute matching the specified class, carried by this entity, if it exists.
//-----------------------------------------------------------------------------
CEconItemAttribute *CAttributeList::GetAttributeByClass( const char *szAttribClass )
{
	int iAttributes = GetNumAttributes();
	for ( int i = 0; i < iAttributes; i++ )
	{
		CEconItemAttribute *pAttr = GetAttribute( i );
		Assert( pAttr && pAttr->GetStaticData() );
		if ( pAttr && pAttr->GetStaticData() && !V_strcmp( pAttr->GetStaticData()->GetAttributeClass(), szAttribClass ) )
			return pAttr;
	}

	return NULL;
}

const CEconItemAttribute *CAttributeList::GetAttributeByClass( const char *szAttribClass ) const
{
	int iAttributes = GetNumAttributes();
	for ( int i = 0; i < iAttributes; i++ )
	{
		const CEconItemAttribute *pAttr = GetAttribute( i );
		Assert( pAttr && pAttr->GetStaticData() );
		if ( pAttr && pAttr->GetStaticData() && !V_strcmp( pAttr->GetStaticData()->GetAttributeClass(), szAttribClass ) )
			return pAttr;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEconItemAttribute *CAttributeList::GetAttributeByDefIndex( uint16 unAttrDefIndex )
{
	int iAttributes = GetNumAttributes();
	for ( int i = 0; i < iAttributes; i++ )
	{
		CEconItemAttribute *pAttr = GetAttribute( i );
		Assert( pAttr && pAttr->GetStaticData() );
		if ( pAttr && pAttr->GetStaticData() && pAttr->GetStaticData()->GetDefinitionIndex() == unAttrDefIndex )
			return pAttr;
	}

	return NULL;
}

const CEconItemAttribute *CAttributeList::GetAttributeByDefIndex( uint16 unAttrDefIndex ) const
{
	int iAttributes = GetNumAttributes();
	for ( int i = 0; i < iAttributes; i++ )
	{
		const CEconItemAttribute *pAttr = GetAttribute( i );
		Assert( pAttr && pAttr->GetStaticData() );
		if ( pAttr && pAttr->GetStaticData() && pAttr->GetStaticData()->GetDefinitionIndex() == unAttrDefIndex )
			return pAttr;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEconItemAttribute *CAttributeList::GetAttributeByName( const char *pszAttribDefName )
{
	const CEconItemAttributeDefinition *pDef = GetItemSchema()->GetAttributeDefinitionByName( pszAttribDefName );
	if ( !pDef )
		return NULL;

	int iAttributes = m_Attributes.Count();
	for ( int i = 0; i < iAttributes; i++ )
	{
		if ( m_Attributes[i].GetStaticData()->GetDefinitionIndex() == pDef->GetDefinitionIndex() )
			return &m_Attributes[i];
	}

	return NULL;
}

const CEconItemAttribute *CAttributeList::GetAttributeByName( const char *pszAttribDefName ) const
{
	const CEconItemAttributeDefinition *pDef = GetItemSchema()->GetAttributeDefinitionByName( pszAttribDefName );
	if ( !pDef )
		return NULL;

	int iAttributes = m_Attributes.Count();
	for ( int i = 0; i < iAttributes; i++ )
	{
		if ( m_Attributes[i].GetStaticData()->GetDefinitionIndex() == pDef->GetDefinitionIndex() )
			return &m_Attributes[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAttributeList&	CAttributeList::operator=( const CAttributeList& src )
{
	m_Attributes = src.m_Attributes;

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	// HACK: We deliberately don't copy managers, because attributelists are contained inside 
	// CEconItemViews, which we duplicate inside CItemModelPanels all the time. If the manager
	// is copied, copies will mess with the attribute caches of the copied item.
	// Our manager will be setup properly by the CAttributeManager itself if we have an associated entity.
	m_pManager = NULL;
#endif

	return *this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::SetValue( CEconItemAttribute *pAttrib, float flValue )
{
#ifdef DEBUG
	FOR_EACH_VEC( m_Attributes, i )
	{
		if ( &m_Attributes[i] == pAttrib )
		{
			pAttrib->SetValue( flValue );
			return;
		}
	}
	// Someone's not being honest, and setting an attribute's value via a CAttributeList that doesn't contain it.
	Assert( 0 );
#else
	pAttrib->SetValue( flValue );
	UpdateManagerCache();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeList::UpdateManagerCache( void ) 
{ 
#if defined(CLIENT_DLL) || defined(GAME_DLL)
	if ( m_pManager ) 
	{
		m_pManager->ClearCache();
	}
#endif
}
