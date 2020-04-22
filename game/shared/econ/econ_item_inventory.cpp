//========= Copyright � 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "econ_item_inventory.h"
#include "vgui/ILocalize.h"
#include "tier3/tier3.h"
#include "econ_item_system.h"
#include "econ_item.h"
#include "econ_gcmessages.h"
#include "shareddefs.h"
#include "filesystem.h"
#include "econ/econ_coupons.h"
#include "econ/econ_item_view_helpers.h"
#include "cdll_int.h"

#include "cs_econ_item_string_table.h"
#include "econ_game_account_client.h"

#ifdef CLIENT_DLL
#include <igameevents.h>
#include "ienginevgui.h"
#include "clientmode_csnormal.h"
#else
#include "props_shared.h"
#include "basemultiplayerplayer.h"
#endif

#if defined( CSTRIKE15 )
#include "cs_gamerules.h"
#endif

#if defined(TF_CLIENT_DLL) || defined(TF_DLL)
#include "tf_gcmessages.h"
#include "tf_duel_summary.h"
#include "econ_contribution.h"
#include "tf_player_info.h"
#include "econ/econ_claimcode.h"
#endif

#if defined(TF_DLL) && defined(GAME_DLL)
#include "tf_gc_api.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace GCSDK;

#ifdef _DEBUG
ConVar item_inventory_debug( "item_inventory_debug", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
#endif

#ifdef CLIENT_DLL
extern ConVar econ_debug_loadout_ui;
#endif

#ifdef USE_DYNAMIC_ASSET_LOADING
//extern ConVar item_dynamicload;
#endif

#ifdef _DEBUG
#ifdef CLIENT_DLL
ConVar item_debug_clientacks( "item_debug_clientacks", "0", FCVAR_CLIENTDLL );
#endif
#endif // _DEBUG

// Result codes strings for GC results.
const char* GCResultString[8] =
{
	"k_EGCMsgResponseOK",			// Request succeeded
	"k_EGCMsgResponseDenied",		// Request denied
	"k_EGCMsgResponseServerError",	// Request failed due to a temporary server error
	"k_EGCMsgResponseTimeout",		// Request timed out
	"k_EGCMsgResponseInvalid",		// Request was corrupt
	"k_EGCMsgResponseNoMatch",		// No item definition matched the request
	"k_EGCMsgResponseUnknownError",	// Request failed with an unknown error
	"k_EGCMsgResponseNotLoggedOn",	// Client not logged on to steam
};


#ifdef CLIENT_DLL

extern char g_szGeneratingTradingIconPath[ MAX_PATH ];
int g_nGeneratingTradingIconSpew = -1;

void CInventoryItemUpdateManager::Update( float frametime )
{
	// Update items that need fixup
	{
		m_fixupListMutex.Lock();

		int32 nTailIdx = m_fixupList.Count() - 1; 
		if ( nTailIdx >= 0 )
		{
			CEconItemView &item = *m_fixupList[nTailIdx];
			item.FinishLoadCachedInventoryImage( item.m_inventoryImageRgba.Base(), item.m_nNumAsyncReadBytes, item.m_asyncStatus );
			m_fixupList.Remove( nTailIdx );
		}

		m_fixupListMutex.Unlock();
	}

	int nTail = m_ManagedItems.Count() - 1;
	if ( nTail < 0 )
	{
		g_nGeneratingTradingIconSpew = -1;
		return;
	}

	CEconItemView *pItemView = m_ManagedItems[ nTail ];

	if ( g_szGeneratingTradingIconPath[ 0 ] )
	{
		const CPaintKit *pPaintKit = pItemView->GetCustomPaintKit();
		if ( pPaintKit )
		{
			if ( g_nGeneratingTradingIconSpew == -1 || g_nGeneratingTradingIconSpew > m_ManagedItems.Count() )
			{
				g_nGeneratingTradingIconSpew = m_ManagedItems.Count();
				DevMsg( "Generating icon [%s]%s, %i remaining!\n", pPaintKit->sName.Get(), pItemView->GetItemDefinition()->GetDefinitionName(), m_ManagedItems.Count() );
			}
		}
	}

	m_ManagedItems.Remove( nTail );

	pItemView->Update();
}

CInventoryItemUpdateManager g_InventoryItemUpdateManager;


#endif // #ifdef CLIENT_DLL

// Inventory Less function.
// Used to sort the inventory items into their positions.
bool CInventoryListLess::Less( CEconItemView* const &src1, CEconItemView *const &src2, void *pCtx )
{
	int iPos1 = src1->GetInventoryPosition();
	int iPos2 = src2->GetInventoryPosition();

	// Context can be specified to point to a func that extracts the position from the backend position.
	// Necessary if your inventory packs a bunch of info into the position instead of using it just as a position.
	if ( pCtx )
	{
		CPlayerInventory *pInv = (CPlayerInventory*)pCtx;
		iPos1 = pInv->ExtractInventorySortPosition( iPos1 );
		iPos2 = pInv->ExtractInventorySortPosition( iPos2 );
	}

	if ( iPos1 < iPos2 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CInventoryManager::CInventoryManager( void )
#ifdef CLIENT_DLL
	: m_mapPersonaNamesCache( DefLessFunc( uint32 ) ),
	  m_mapPredictedFilledSlots( DefLessFunc( uint32 ) )
#endif
{
#ifdef CLIENT_DLL
	m_iPredictedDiscards = 0;
	m_bIsBatchingPositions = false;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::RegisterPlayerInventory( CPlayerInventory *pInventory, IInventoryUpdateListener *pListener /* = NULL */, CSteamID* pSteamID )
{
	// If we haven't seen this inventory before, register it
	bool bFound = false;
	for ( int i = 0; i < m_pInventories.Count(); i++ )
	{
		if ( m_pInventories[i].pInventory == pInventory )
		{
			bFound = true;
			break;
		}
	}

	if ( !bFound )
	{
		int iIdx = m_pInventories.AddToTail();
		m_pInventories[iIdx].pInventory = pInventory;
		m_pInventories[iIdx].pListener = pListener;

		if ( pSteamID )
		{
			SOID_t ownerSOID = GetSOIDFromSteamID( *pSteamID );
			pInventory->SetOwner( ownerSOID );
		}

		if ( pListener )
		{
			pInventory->AddListener( pListener );
		}
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::SteamRequestInventory( CPlayerInventory *pInventory, CSteamID pSteamID, IInventoryUpdateListener *pListener )
{
	RegisterPlayerInventory( pInventory, pListener, &pSteamID );

	pInventory->RequestInventory( pSteamID );	
}


//-----------------------------------------------------------------------------
// Purpose: Called when a gameserver connects to steam.
//-----------------------------------------------------------------------------
void CInventoryManager::GameServerSteamAPIActivated()
{
#if defined(TF_DLL) && defined(GAME_DLL)
	GameCoordinator_NotifyGameState();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerInventory *CInventoryManager::GetInventoryForAccount( uint32 iAccountID )
{
	FOR_EACH_VEC( m_pInventories, i )
	{
		CPlayerInventory* pInventory = m_pInventories[i].pInventory;
		if ( GetSteamIDFromSOID( pInventory->GetOwner() ).GetAccountID() == iAccountID )
			return m_pInventories[i].pInventory;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::DeregisterInventory( CPlayerInventory *pInventory )
{
	int iCount = m_pInventories.Count();
	for ( int i = iCount-1; i >= 0; i-- )
	{
		if ( m_pInventories[i].pInventory == pInventory )
		{
			m_pInventories.Remove(i);
		}
	}
}


#ifdef CLIENT_DLL

void CInventoryManager::DestroyPlayerInventoryObject( CPlayerInventory *pPlayerInventory ) const
{
	if ( pPlayerInventory )
	{
		pPlayerInventory->Shutdown();
	}

	delete pPlayerInventory;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::UpdateLocalInventory( void )
{
	if ( steamapicontext->SteamUser() && GetLocalInventory() )
	{
		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
		SteamRequestInventory( GetLocalInventory(), steamID );
	}	
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CInventoryManager::Init( void )
{
	REG_SHARED_OBJECT_SUBCLASS( CEconItem );
	REG_SHARED_OBJECT_SUBCLASS( CEconDefaultEquippedDefinitionInstanceClient );

#if defined (CLIENT_DLL)
	REG_SHARED_OBJECT_SUBCLASS( CEconGameAccountClient );

	REG_SHARED_OBJECT_SUBCLASS( CEconCoupon );

	// Make sure the cache directory exists
	g_pFullFileSystem->CreateDirHierarchy( ECON_ITEM_GENERATED_ICON_DIR, "DEFAULT_WRITE_PATH" );
#endif

#if defined(GAME_DLL)
	REG_SHARED_OBJECT_SUBCLASS( CEconPersonaDataPublic );
#endif

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::PostInit( void )
{
	// Initialize the item system.
	ItemSystem()->Init();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::PreInitGC()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::PostInitGC()
{
#ifdef CLIENT_DLL
	// The client immediately loads the local player's inventory
	UpdateLocalInventory();
#endif
}


//-----------------------------------------------------------------------------
void CInventoryManager::Shutdown()
{
	int nInventoryCount = m_pInventories.Count();
	for ( int iInventory = 0; iInventory < nInventoryCount; ++iInventory )
	{
		CPlayerInventory *pInventory = m_pInventories[iInventory].pInventory;
		if ( pInventory )
		{
			pInventory->SOClear();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::LevelInitPreEntity()
{
	// Throw out any testitem definitions
	for ( int i = 0; i < TESTITEM_DEFINITIONS_COUNT; i++ )
	{
		int iNewDef = TESTITEM_DEFINITIONS_BEGIN_AT + i;
		ItemSystem()->GetItemSchema()->ItemTesting_DiscardTestDefinition( iNewDef );
	}

	// Precache all item models we've got
#ifdef GAME_DLL
	CUtlVector<const char *> vecPrecacheStrings;
#endif // GAME_DLL
	const CEconItemSchema::ItemDefinitionMap_t& mapItemDefs = ItemSystem()->GetItemSchema()->GetItemDefinitionMap();
	FOR_EACH_MAP_FAST( mapItemDefs, i )
	{
		CEconItemDefinition *pData = mapItemDefs[i];

		pData->SetHasBeenLoaded( true );

#ifdef GAME_DLL
		bool bDynamicLoad = false;

#ifdef USE_DYNAMIC_ASSET_LOADING
		bDynamicLoad = true;//item_dynamicload.GetBool();
#endif // USE_DYNAMIC_ASSET_LOADING

		// Robin: Don't precache all assets up front.
		bDynamicLoad = true;

		pData->GeneratePrecacheModelStrings( bDynamicLoad, &vecPrecacheStrings );

		// Precache the models and the gibs for everything the definition requested.
		FOR_EACH_VEC( vecPrecacheStrings, i )
		{
			// Ignore any objects which requested an empty precache string for whatever reason.
			if ( vecPrecacheStrings[i] && vecPrecacheStrings[i][0] )
			{
				int iModelIndex = CBaseEntity::PrecacheModel( vecPrecacheStrings[i] );
				PrecacheGibsForModel( iModelIndex );
			}
		}

		vecPrecacheStrings.RemoveAll();

		pData->GeneratePrecacheSoundStrings( &vecPrecacheStrings );

		// Precache the sounds for everything
		FOR_EACH_VEC( vecPrecacheStrings, i )
		{
			// Ignore any objects which requested an empty precache string for whatever reason.
			if ( vecPrecacheStrings[i] && vecPrecacheStrings[i][0] )
			{
				CBaseEntity::PrecacheScriptSound( vecPrecacheStrings[i] );
			}
		}

		vecPrecacheStrings.RemoveAll();

		pData->GeneratePrecacheEffectStrings( &vecPrecacheStrings );

		// Precache the sounds for everything
		FOR_EACH_VEC( vecPrecacheStrings, i )
		{
			// Ignore any objects which requested an empty precache string for whatever reason.
			if ( vecPrecacheStrings[i] && vecPrecacheStrings[i][0] )
			{
				PrecacheEffect( vecPrecacheStrings[i] );
			}
		}

		vecPrecacheStrings.RemoveAll();
#endif //#ifdef GAME_DLL

#if defined(CSTRIKE15) && ( defined( CLIENT_DLL ) || defined( GAME_DLL ) )
		const char *szSlot = pData->GetRawDefinition()->GetString("item_slot");
		if ( szSlot && (
			!V_stricmp( szSlot, "secondary" ) ||
			!V_stricmp( szSlot, "smg" ) ||
			!V_stricmp( szSlot, "heavy" ) ||
			!V_stricmp( szSlot, "rifle" )
			) )
		{
			extern void GenerateWeaponRecoilPatternForItemDefinition( item_definition_index_t idx );
			GenerateWeaponRecoilPatternForItemDefinition( pData->GetDefinitionIndex() );
		}
#endif
	}

	// We reset the cached attribute class strings, since it's invalidated by level changes
	ItemSystem()->ResetAttribStringCache();

#ifdef GAME_DLL
	ItemSystem()->ReloadWhitelist();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::LevelShutdownPostEntity()
{
	// We reset the cached attribute class strings, since it's invalidated by level changes
	ItemSystem()->ResetAttribStringCache();
}

#ifdef CLIENT_DLL

void CInventoryManager::ClearLocalInventoryImages( const CCommand &args )
{
	CPlayerInventory *pInventory = GetLocalInventory();
	if ( !pInventory )
		return;

	for ( int i = 0; i < pInventory->GetItemCount(); i++ )
	{
		CEconItemView *pItem = pInventory->GetItem(i);
		if ( pItem )
		{
			pItem->ClearInventoryImageRgba();
		}
	}
}

void CInventoryManager::InsertMaterialGenerationJob( CEconItemView *pItemView )
{
	g_InventoryItemUpdateManager.AddItemView( pItemView );
}

void CInventoryManager::OnDestroyEconItemView( CEconItemView *pItemView )
{
	g_InventoryItemUpdateManager.RemoveItemView( pItemView );
}

#endif


//-----------------------------------------------------------------------------
// Purpose: Lets the client know that we're now connected to the GC
//-----------------------------------------------------------------------------
#ifdef CLIENT_DLL
void CInventoryManager::SendGCConnectedEvent( void )
{
	IGameEvent *event = gameeventmanager->CreateEvent( "gc_connected" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}
#endif
 
#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::DeleteItem( itemid_t iItemID, bool bRecycle )
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: Delete any items we can't find static data for. This can happen when we're testing
//			internally, and then remove an item. Shouldn't ever happen in the wild.
//-----------------------------------------------------------------------------
int	CInventoryManager::DeleteUnknowns( CPlayerInventory *pInventory )
{
	// We need to manually walk the main inventory's SOC, because unknown items won't be in the inventory
	GCSDK::CGCClientSharedObjectCache *pSOC = pInventory->GetSOC();
	if ( pSOC )
	{
		int iBadItems = 0;
		CGCClientSharedObjectTypeCache *pTypeCache = pSOC->FindTypeCache( CEconItem::k_nTypeID );
		if( pTypeCache )
		{
			for( uint32 unItem = 0; unItem < pTypeCache->GetCount(); unItem++ )
			{
				CEconItem *pItem = (CEconItem *)pTypeCache->GetObject( unItem );
				if ( pItem )
				{
					const CEconItemDefinition *pData = ItemSystem()->GetStaticDataForItemByDefIndex( pItem->GetDefinitionIndex() );
					if ( !pData )
					{
						DeleteItem( pItem->GetItemID() );
						iBadItems++;
					}
				}
			}
		}
		return iBadItems;
	}		

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Tries to move the specified item into the player's backpack.
//			FAILS if the backpack is full. Returns false in that case.
//-----------------------------------------------------------------------------
bool CInventoryManager::SetItemBackpackPosition( CEconItemView *pItem, uint32 iPosition, bool bForceUnequip, bool bAllowOverflow )
{
	CPlayerInventory *pInventory = GetLocalInventory();
	if ( !pInventory )
		return false;

	const int iMaxItems = pInventory->GetMaxItemCount();
	if ( !iPosition )
	{
		// Build a list of empty slots. We track extra slots beyond the backpack for overflow.
		CUtlVector< bool > bFilledSlots;
		bFilledSlots.SetSize( iMaxItems * 2 );
		for ( int i = 0; i < bFilledSlots.Count(); ++i )
		{
			bFilledSlots[i] = false;
		}
		for ( int i = 0; i < pInventory->GetItemCount(); i++ )
		{
			CEconItemView *pTmpItem = pInventory->GetItem(i);
			// Ignore the item we're moving.
			if ( pTmpItem == pItem )
				continue;

			int iBackpackPos = GetBackpackPositionFromBackend( pTmpItem->GetInventoryPosition() );
			if ( iBackpackPos >= 0 && iBackpackPos < bFilledSlots.Count() )
			{
				bFilledSlots[iBackpackPos] = true;
			}
		}

		// Add predicted filled slots
		for ( int i = m_mapPredictedFilledSlots.FirstInorder(); i != m_mapPredictedFilledSlots.InvalidIndex(); i = m_mapPredictedFilledSlots.NextInorder( i ) )
		{
			uint32 iBackpackPos = m_mapPredictedFilledSlots.Key( i );
			uint64 ullItemID = m_mapPredictedFilledSlots.Element( i );

			if ( ullItemID == pItem->GetItemID() )
			{
				// We already predicted the location of this item.
				return !(iBackpackPos > (uint32)iMaxItems);
			}

			if ( iBackpackPos < (uint32) bFilledSlots.Count() )
			{
				bFilledSlots[iBackpackPos] = true;
			}
		}

		// Now find an empty slot
		for ( int i = 1; i < bFilledSlots.Count(); i++ )
		{
			if ( !bFilledSlots[i] )
			{
				iPosition = i;
				break;
			}
		}

		if ( !iPosition )
			return false;
	}

	if ( !bAllowOverflow && iPosition > (uint32)iMaxItems )
		return false;

	//Warning("Moved item %llu to backpack slot: %d\n", pItem->GetItemID(), iPosition );

	uint32 iBackendPosition = bForceUnequip ? 0 : pItem->GetInventoryPosition();
	SetBackpackPosition( &iBackendPosition, iPosition );
	UpdateInventoryPosition( pInventory, pItem->GetItemID(), iBackendPosition );

	m_mapPredictedFilledSlots.Insert( iPosition, pItem->GetItemID() );
	return true;
}

void CInventoryManager::PredictedBackpackPosFilled( int iBackpackPos )
{
	int idx = m_mapPredictedFilledSlots.Find( iBackpackPos );
	if ( m_mapPredictedFilledSlots.IsValidIndex( idx ) )
	{
		m_mapPredictedFilledSlots.Remove( iBackpackPos );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::MoveItemToBackpackPosition( CEconItemView *pItem, int iBackpackPosition )
{
	CEconItemView *pOldItem = GetItemByBackpackPosition( iBackpackPosition );
	if ( pOldItem )
	{
		// Move the item in the new spot to our current spot
		SetItemBackpackPosition( pOldItem, GetBackpackPositionFromBackend(pItem->GetInventoryPosition()) );

		//Warning("Moved OLD item %llu to backpack slot: %d\n", pOldItem->GetItemID(), GetBackpackPositionFromBackend(iBackendPosition) );
	}

	// Move the item to the new spot
	SetItemBackpackPosition( pItem, iBackpackPosition );

	//Warning("Moved item %llu to backpack slot: %d\n", pItem->GetItemID(), iBackpackPosition );
}

#ifdef TF_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CWaitForBackpackSortFinishDialog : public CGenericWaitingDialog
{
public:
	CWaitForBackpackSortFinishDialog( vgui::Panel *pParent ) : CGenericWaitingDialog( pParent )
	{
	}

protected:
	virtual void OnTimeout()
	{
		InventoryManager()->SortBackpackFinished();
	}
};
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::SortBackpackBy( uint32 iSortType )
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::SortBackpackFinished( void )
{
	m_bInBackpackSort = false;

	GetLocalInventory()->SendInventoryUpdateEvent();
}

//-----------------------------------------------------------------------------
void CInventoryManager::BeginBackpackPositionTransaction()
{
	Assert( !m_bIsBatchingPositions );

	m_itemPositions.Clear();
	m_bIsBatchingPositions = true;
}

//-----------------------------------------------------------------------------
void CInventoryManager::EndBackpackPositionTransaction()
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::UpdateInventoryPosition( CPlayerInventory *pInventory, uint64 ulItemID, uint32 unNewInventoryPos )
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::UpdateInventoryEquippedState( CPlayerInventory *pInventory, uint64 ulItemID, equipped_class_t unClass, equipped_slot_t unSlot, bool bSwap /*= false*/ )
{
	C_EconItemView *pItem = pInventory->GetInventoryItemByItemID( ulItemID );

	bool bIsBaseItem = false;

	const CEconItemDefinition *pItemDef = NULL;

	// The Item isn't in the inventory, test for base item.
	if ( !pItem )
	{
		//Warning("Attempt to update equipped state failure: %s.\n", "could not find matching item ID");
		if ( !CombinedItemIdIsDefIndexAndPaint( ulItemID ) )
			return;

		pItemDef = GetItemSchema()->GetItemDefinition( CombinedItemIdGetDefIndex( ulItemID ) ) ; 
		if ( !pItemDef )
			return;

		bIsBaseItem = pItemDef->IsBaseItem();
	}

	if ( !bIsBaseItem && !pInventory->GetSOCDataForItem( ulItemID ) )
	{
		//Warning("Attempt to update equipped state failure: %s\n", "could not find SOC data for item");
		return;
	}

#ifdef CLIENT_DLL
	// If this is a set item, other owned items in the set need their descriptions updated

	if ( pItem )
	{
		int nSetIndex = pItem->GetItemSetIndex();
		if ( nSetIndex >= 0 )
		{
			const CEconItemSetDefinition *pItemSetDef = GetItemSchema()->GetItemSetByIndex( nSetIndex );
			if ( pItemSetDef && !pItemSetDef->m_bIsCollection )
			{
				pInventory->MarkSetItemDescriptionsDirty( nSetIndex );
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CInventoryManager::ShowItemsPickedUp( bool bForce, bool bReturnToGame, bool bNoPanel )
{
	/** Removed for partner depot **/
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CInventoryManager::CheckForRoomAndForceDiscard( void )
{
	/** Removed for partner depot **/
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView *CInventoryManager::GetItemByBackpackPosition( int iBackpackPosition )
{
	CPlayerInventory *pInventory = GetLocalInventory();
	if ( !pInventory )
		return NULL;

	// Backpack positions start from 1
	if ( iBackpackPosition <= 0 || iBackpackPosition >= pInventory->GetMaxItemCount() )
	{
		return NULL;
	}

	for ( int i = 0; i < pInventory->GetItemCount(); i++ )
	{
		CEconItemView *pItem = pInventory->GetItem(i);
		if ( GetBackpackPositionFromBackend( pItem->GetInventoryPosition() ) == iBackpackPosition )
			return pItem;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CInventoryManager::HasBeenAckedByClient( CEconItemView *pItem )
{
	return ( m_rbItemIdsClientAck.Find( pItem->GetItemID() ) != m_rbItemIdsClientAck.InvalidIndex() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::SetAckedByClient( CEconItemView *pItem )
{
	m_rbItemIdsClientAck.InsertIfNotFound( pItem->GetItemID() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInventoryManager::SetAckedByGC( CEconItemView *pItem, bool bSave )
{
	m_rbItemIdsClientAck.Remove( pItem->GetItemID() );
}

bool CInventoryManager::BGetPlayerQuestIdPointsRemaining( uint16 unQuestID, uint32 &numPointsRemaining, uint32 &numPointsUncommitted )
{
	/** Removed for partner depot **/
	return true;
}

CEconItemView* CInventoryManager::CreateReferenceEconItem( item_definition_index_t iDefIndex, int iPaintIndex, uint8 ub1 )
{
	int iNewItem = m_pTempReferenceItems.AddToTail( new CEconItemView );
	CEconItemView *pItemView = m_pTempReferenceItems[ iNewItem ];

	pItemView->Init( iDefIndex, AE_UNIQUE, AE_USE_SCRIPT_VALUE, true );

	static CSchemaItemDefHandle hItemDefSticker( "sticker" );
	static CSchemaItemDefHandle hItemDefSpray( "spray" );
	static CSchemaItemDefHandle hItemDefSprayPaint( "spraypaint" );

	static CSchemaItemDefHandle hItemDefQuest( "quest" );
	static CSchemaAttributeDefHandle pAttr_QuestID( "quest id" );

	static CSchemaItemDefHandle hItemDefMusicKit( "musickit" );
	static CSchemaAttributeDefHandle pAttr_MusicID( "music id" );

	if ( !pItemView->IsValid() )
		return NULL;

	if ( iPaintIndex > 0 )
	{
		if ( ( hItemDefSticker && iDefIndex == hItemDefSticker->GetDefinitionIndex() )
			|| ( hItemDefSpray && iDefIndex == hItemDefSpray->GetDefinitionIndex() )
			|| ( hItemDefSprayPaint && iDefIndex == hItemDefSprayPaint->GetDefinitionIndex() ) )
		{
			float flStickerID = *((float*)&iPaintIndex);
			pItemView->SetOrAddAttributeValueByName( "sticker slot 0 id", flStickerID );

			if ( ub1 )
			{
				uint32 ub1_32 = ub1;
				float flub1_fl = *( ( float* ) &ub1_32 );
				pItemView->SetOrAddAttributeValueByName( "spray tint id", flub1_fl );
			}

			const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( iDefIndex );
			const CStickerKit *pStickerKit = GetItemSchema()->GetStickerKitDefinition( iPaintIndex );

			pItemView->SetItemRarityOverride( EconRarity_CombinedItemAndPaintRarity( pItemDef->GetRarity(), pStickerKit->nRarity ) );
		}
		else if ( hItemDefQuest && pAttr_QuestID && iDefIndex == hItemDefQuest->GetDefinitionIndex() )
		{
			int iQuestID = iPaintIndex;

			if ( iQuestID )
			{
				uint32 numPointsRemaining = 0;
				uint32 numPointsUncommitted = 0;
				if ( BGetPlayerQuestIdPointsRemaining( iQuestID, numPointsRemaining, numPointsUncommitted ) && numPointsRemaining )
				{
					float flPointsRemaining = *( ( float* ) &numPointsRemaining );
					pItemView->SetOrAddAttributeValueByName( "quest points remaining", flPointsRemaining );

					float flPointsUncommitted = *( ( float* ) &numPointsUncommitted );
					pItemView->SetOrAddAttributeValueByName( "operation points", flPointsUncommitted );
				}

				float flQuestID = *( ( float* )&iQuestID );
				pItemView->SetOrAddAttributeValueByName( "quest id", flQuestID );

			}


			pItemView->SetItemRarityOverride( 1 ); // TODO: different missions rarity?
		}
		else if ( hItemDefMusicKit && pAttr_MusicID && iDefIndex == hItemDefMusicKit->GetDefinitionIndex() )
		{
			if ( iPaintIndex )
			{
				float flMusicID = *( ( float* )&iPaintIndex );
				pItemView->SetOrAddAttributeValueByName( "music id", flMusicID );

				if ( ub1 )
				{	// This is how StatTrak Music Kits are made
					Assert( ub1 == AE_STRANGE );
					pItemView->SetItemQualityOverride( ub1 );
				}
			}
		}
		else
		{
			pItemView->SetOrAddAttributeValueByName( "set item texture prefab", iPaintIndex );

			const CEconItemDefinition *pItemDef = GetItemSchema()->GetItemDefinition( iDefIndex );

			AssertMsg1( pItemDef, "Failed to get item definition for index: %i\n", iDefIndex );
			if ( !pItemDef )
				return NULL;

			const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinition( iPaintIndex );

			AssertMsg1( pPaintKit, "Failed to get paintkit definition for index: %i\n", iPaintIndex );
			if ( !pPaintKit )
				return NULL;

			pItemView->SetItemRarityOverride( EconRarity_CombinedItemAndPaintRarity( pItemDef->GetRarity(), pPaintKit->nRarity ) );
		}

#ifdef CLIENT_DLL
		InsertMaterialGenerationJob( pItemView );
#endif // #ifdef CLIENT_DLL
	}
	else
	{
		if ( pItemView->GetItemDefinition() && !pItemView->GetItemDefinition()->IsBaseItem() )
		{
			pItemView->SetItemRarityOverride( pItemView->GetItemDefinition()->GetRarity() );

			// Genuine collectibles will force the item quality
			uint8 unDefinitionQuality = pItemView->GetItemDefinition()->GetQuality();
			if ( unDefinitionQuality == AE_GENUINE )
			{
				pItemView->SetItemQualityOverride( unDefinitionQuality );
			}
		}
		else
		{
			pItemView->SetItemRarityOverride( 0 /* default aka stock */ );
		}
	}

	return pItemView;
}

void CInventoryManager::RemoveReferenceEconItem( item_definition_index_t iDefIndex, int iPaintIndex, uint8 ub1 )
{
	static CSchemaItemDefHandle hItemDefSticker( "sticker" );
	static CSchemaItemDefHandle hItemDefSpray( "spray" );
	static CSchemaItemDefHandle hItemDefSprayPaint( "spraypaint" );
	static CSchemaAttributeDefHandle pAttr_SprayTintID( "spray tint id" );

	static CSchemaItemDefHandle hItemDefQuest( "quest" );
	static CSchemaAttributeDefHandle pAttr_QuestID( "quest id" );

	static CSchemaItemDefHandle hItemDefMusicKit( "musickit" );
	static CSchemaAttributeDefHandle pAttr_MusicID( "music id" );

	static CSchemaItemDefHandle hItemDefWearable( "wearable" );

	FOR_EACH_VEC( m_pTempReferenceItems, i )
	{
		CEconItemView *pItem = m_pTempReferenceItems[ i ];

		if ( pItem->GetItemIndex() == iDefIndex )
		{
			if ( ( hItemDefSticker && iDefIndex == hItemDefSticker->GetDefinitionIndex() )
				|| ( hItemDefSpray && iDefIndex == hItemDefSpray->GetDefinitionIndex() )
				|| ( hItemDefSprayPaint && iDefIndex == hItemDefSprayPaint->GetDefinitionIndex() ) )
			{
				uint32 unTintID = 0;
				if ( ( pItem->GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 ) == uint32( iPaintIndex ) ) &&
					pAttr_SprayTintID && ( pItem->FindAttribute( pAttr_SprayTintID, &unTintID ), ( unTintID == ub1 ) )
					)
				{
					m_pTempReferenceItems.Remove( i );
					return;
				}
			}
			else if ( hItemDefQuest && pAttr_QuestID && iDefIndex == hItemDefQuest->GetDefinitionIndex() )
			{
				uint32 unQuestID = 0;
				if ( pItem->FindAttribute( pAttr_QuestID, &unQuestID ) && ( unQuestID == uint32( iPaintIndex ) ) )
				{
					m_pTempReferenceItems.Remove( i );
					return;
				}
			}
			else if ( hItemDefMusicKit && pAttr_MusicID && iDefIndex == hItemDefMusicKit->GetDefinitionIndex() )
			{
				uint32 unMusicID = 0;
				if ( pItem->FindAttribute( pAttr_MusicID, &unMusicID ) && ( unMusicID == uint32( iPaintIndex ) ) )
				{	// Music Kits can be regular or StatTrak
					int iQualityNeeded = ub1 ? ub1 : AE_UNIQUE;
					if ( iQualityNeeded == pItem->GetQuality() )
					{
						m_pTempReferenceItems.Remove( i );
						return;
					}
				}
			}
			else // code is currently valid for both wearables and weapons with paint kits
			{
				if ( pItem->GetCustomPaintKitIndex() == iPaintIndex )
				{
					m_pTempReferenceItems.Remove( i );
					return;
				}
			}
		}
	}
}

CEconItemView* CInventoryManager::FindReferenceEconItem( item_definition_index_t iDefIndex, int iPaintIndex, uint8 ub1 )
{
	static CSchemaItemDefHandle hItemDefSticker( "sticker" );
	static CSchemaItemDefHandle hItemDefSpray( "spray" );
	static CSchemaItemDefHandle hItemDefSprayPaint( "spraypaint" );
	static CSchemaAttributeDefHandle pAttr_SprayTintID( "spray tint id" );

	static CSchemaItemDefHandle hItemDefQuest( "quest" );
	static CSchemaAttributeDefHandle pAttr_QuestID( "quest id" );

	static CSchemaItemDefHandle hItemDefMusicKit( "musickit" );
	static CSchemaAttributeDefHandle pAttr_MusicID( "music id" );

	FOR_EACH_VEC( m_pTempReferenceItems, i )
	{
		CEconItemView *pItem = m_pTempReferenceItems[ i ];

		if ( pItem->GetItemIndex() == iDefIndex )
		{
			if ( ( hItemDefSticker && iDefIndex == hItemDefSticker->GetDefinitionIndex() )
				|| ( hItemDefSpray && iDefIndex == hItemDefSpray->GetDefinitionIndex() )
				|| ( hItemDefSprayPaint && iDefIndex == hItemDefSprayPaint->GetDefinitionIndex() ) )
			{
				uint32 unTintID = 0;
				if ( ( pItem->GetStickerAttributeBySlotIndexInt( 0, k_EStickerAttribute_ID, 0 ) == uint32( iPaintIndex ) ) &&
					pAttr_SprayTintID && ( pItem->FindAttribute( pAttr_SprayTintID, &unTintID ), ( unTintID == ub1 ) )
					)
				{
					return pItem;
				}
			}
			else if ( hItemDefQuest && pAttr_QuestID && iDefIndex == hItemDefQuest->GetDefinitionIndex() )
			{
				uint32 unQuestID = 0;
				if ( pItem->FindAttribute( pAttr_QuestID, &unQuestID ) && ( unQuestID == uint32( iPaintIndex ) ) )
				{
					// Ensure that the points remaining attribute is correct, just always stomp it
					uint32 numPointsRemaining = 0;
					uint32 numPointsUncommitted = 0;
					if ( BGetPlayerQuestIdPointsRemaining( unQuestID, numPointsRemaining, numPointsUncommitted ) && numPointsRemaining )
					{
						float flPointsRemaining = *( ( float* ) &numPointsRemaining );
						pItem->SetOrAddAttributeValueByName( "quest points remaining", flPointsRemaining );

						float flPointsUncommitted = *( ( float* ) &numPointsUncommitted );
						pItem->SetOrAddAttributeValueByName( "operation points", flPointsUncommitted );
					}
					return pItem;
				}
			}
			else if ( hItemDefMusicKit && pAttr_MusicID && iDefIndex == hItemDefMusicKit->GetDefinitionIndex() )
			{
				uint32 unMusicID = 0;
				if ( pItem->FindAttribute( pAttr_MusicID, &unMusicID ) && ( unMusicID == uint32( iPaintIndex ) ) )
				{	// Music Kits can be regular or StatTrak
					int iQualityNeeded = ub1 ? ub1 : AE_UNIQUE;
					if ( iQualityNeeded == pItem->GetQuality() )
						return pItem;
				}
			}
			else   // code is currently valid for both wearables and weapons with paint kits
			{
				if ( pItem->GetCustomPaintKitIndex() == iPaintIndex )
				{
					return pItem;
				}
			}
		}
	}

	return NULL;
}

CEconItemView* CInventoryManager::FindOrCreateReferenceEconItem( item_definition_index_t iDefIndex, int iPaintIndex, uint8 ub1 )
{
	CEconItemView *pItem = FindReferenceEconItem( iDefIndex, iPaintIndex, ub1 );

	if ( !pItem )
	{
		pItem = CreateReferenceEconItem( iDefIndex, iPaintIndex, ub1 );
	}

	return pItem;
}


CEconItemView* CInventoryManager::FindOrCreateReferenceEconItem( itemid_t ullFauxItemId )
{
	if ( !CombinedItemIdIsDefIndexAndPaint( ullFauxItemId) )
	{
		Assert(0);
		return NULL;
	}

	item_definition_index_t nDefIndex = CombinedItemIdGetDefIndex( ullFauxItemId );
	uint16 nPaintIndex = CombinedItemIdGetPaint( ullFauxItemId );
	uint8 ub1 = CombinedItemIdGetUB1( ullFauxItemId );

	return FindOrCreateReferenceEconItem( nDefIndex, nPaintIndex, ub1 );
}

#endif // CLIENT_DLL


//=======================================================================================================================
// PLAYER INVENTORY
//=======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerInventory::CPlayerInventory( void )
{
	m_bGotItemsFromSteamAtLeastOnce = false;
	m_bCurrentlySubscribedToSteam = false;
	
	m_pSOCache = NULL;
	m_nTargetRecipe = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPlayerInventory::~CPlayerInventory()
{
}

void CPlayerInventory::Shutdown()
{
	SOClear();

	InventoryManager()->DeregisterInventory( this );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerInventory::SOClear()
{
	// Somebody registered as a listener through us, but now our Steam ID
	// is changing?  This is bad news.
	Assert( m_vecListeners.Count() == 0 );
	while ( m_vecListeners.Count() > 0 )
	{
		RemoveListener( m_vecListeners[0] );
	}

	// If we were subscribed, we should have gotten our unsubscribe message,
	// and that should have cleared the pointer
	// [ when we unsubscribe we no longer remove all references, so the assert is no longer valid ]
	// Assert( m_pSOCache == NULL);
	m_pSOCache = NULL;
	m_Items.Purge();
	m_aDefaultEquippedDefinitionItems.PurgeAndDeleteElements();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerInventory::RequestInventory( SOID_t ID )
{
	// Make sure we don't already have somebody else's stuff
	// on hand
	if ( m_OwnerID != ID )
		SOClear();

	// Remember whose inventory we're looking at
	m_OwnerID = ID; 
}

void CPlayerInventory::AddListener( GCSDK::ISharedObjectListener *pListener )
{
	Assert( m_OwnerID.IsValid() );
	if ( m_vecListeners.Find( pListener ) >= 0 )
	{
		Assert( !"CPlayerInventory::AddListener - added multiple times" );
	}
	else
	{
		m_vecListeners.AddToTail( pListener );
	}
}


void CPlayerInventory::RemoveListener( GCSDK::ISharedObjectListener *pListener )
{
	if ( m_OwnerID.IsValid() )
	{
		m_vecListeners.FindAndFastRemove( pListener );
	}
	else
	{
		Assert( m_vecListeners.Count() == 0 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Helper function to add a new item for a econ item
//-----------------------------------------------------------------------------
bool CPlayerInventory::AddEconItem( CEconItem * pItem, bool bUpdateAckFile, bool bWriteAckFile, bool bCheckForNewItems )
{
	CEconItemView *pNewItem = new CEconItemView;
	if( !FilloutItemFromEconItem( pNewItem, pItem ) )
	{
		return false;
	}

#ifndef CLIENT_DLL
	CEconItemView::s_mapLookupByID.InsertOrReplace( pItem->GetItemID(),
			CombinedItemIdMakeFromDefIndexAndPaint(
				pItem->GetDefinitionIndex(), pItem->GetCustomPaintKitIndex() )
		);
	// Also insert the fact that this account owns an item of this defindex
	CEconItemView::s_mapLookupByID.InsertOrReplace(
			(1ull << 63)
			| ( uint64( GetSteamIDFromSOID( GetOwner() ).GetAccountID() & 0x7FFFFFFF ) << 32 )
			| pItem->GetDefinitionIndex(),
			0ull
		);
#endif

	m_Items.Add( pNewItem );
	ItemHasBeenUpdated( pNewItem, bUpdateAckFile, bWriteAckFile, k_EInventoryItemCreated );

	int nItemSet = pItem->GetItemSetIndex();
	if ( nItemSet )
	{
		const CEconItemSetDefinition *pItemSetDef = GetItemSchema()->GetItemSetByIndex( nItemSet );
		if ( pItemSetDef && pItemSetDef->m_bIsCollection )
		{
			MarkSetItemDescriptionsDirty( nItemSet );
		}
	}

#ifdef CLIENT_DLL
	if ( bCheckForNewItems && InventoryManager()->GetLocalInventory() == this )
	{
		bool bNotify = IsUnacknowledged( pItem->GetInventoryToken() );
		// ignore Halloween drops
		bNotify &= pItem->GetOrigin() != kEconItemOrigin_HalloweenDrop;
		// only notify for specific reasons
		unacknowledged_item_inventory_positions_t reason = GetUnacknowledgedReason( pItem->GetInventoryToken() );
		bNotify &= 
			reason == UNACK_ITEM_DROPPED || reason == UNACK_ITEM_SUPPORT || 
			reason == UNACK_ITEM_EARNED || reason == UNACK_ITEM_REFUNDED || 
			reason == UNACK_ITEM_COLLECTION_REWARD || reason == UNACK_ITEM_TRADED ||
			reason == UNACK_ITEM_RECYCLING || reason == UNACK_ITEM_PURCHASED;
		if ( bNotify )
		{
			OnHasNewItems();
		}		
	}
#endif
	return true;
}

bool CPlayerInventory::AddEconDefaultEquippedDefinition( CEconDefaultEquippedDefinitionInstanceClient *pDefaultEquippedDefinition )
{
	int nItemDef = pDefaultEquippedDefinition->Obj().item_definition();
	int nClassID = pDefaultEquippedDefinition->Obj().class_id();
	int nSlotID = pDefaultEquippedDefinition->Obj().slot_id();

	SetDefaultEquippedDefinitionItemBySlot( nClassID, nSlotID, nItemDef );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a script item and associates it with this econ item
//-----------------------------------------------------------------------------
void CPlayerInventory::SOCreated( SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )
{
#ifdef _DEBUG
	Msg("CPlayerInventory::SOCreated %s [event = %u]\n", CSteamID( owner.ID() ).Render(), eEvent );
#endif

	if ( owner != m_OwnerID )
		return;

	// We shouldn't get these notifications unless we're subscribed, right?
	if ( m_pSOCache == NULL)
	{
		Assert( m_pSOCache );
		return;
	}

	// Don't bother unless it's an incremental notification.
	// For mass updates, we'll do everything more efficiently in one place
	if ( eEvent != GCSDK::eSOCacheEvent_Incremental )
	{
		Assert( eEvent == GCSDK::eSOCacheEvent_Subscribed || eEvent == GCSDK::eSOCacheEvent_Resubscribed || eEvent == GCSDK::eSOCacheEvent_ListenerAdded );
		return;
	}

	if ( pObject->GetTypeID() == CEconItem::k_nTypeID )
	{
		CEconItem *pItem = (CEconItem *)pObject;
		AddEconItem( pItem, true, true, true );
		
	}
	else if ( pObject->GetTypeID() == CEconDefaultEquippedDefinitionInstanceClient::k_nTypeID )
	{
		CEconDefaultEquippedDefinitionInstanceClient *pDefaultEquippedDefinitionInstance = (CEconDefaultEquippedDefinitionInstanceClient *)pObject;
		AddEconDefaultEquippedDefinition( pDefaultEquippedDefinitionInstance );
	}
	else
	{
		return;
	}

	SendInventoryUpdateEvent();
}


//-----------------------------------------------------------------------------
// Purpose: Updates the script item associated with this econ item
//-----------------------------------------------------------------------------
void CPlayerInventory::SOUpdated( SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )
{
#ifdef _DEBUG
	{
		CSteamID steamIDOwner( owner.ID() );

		static CSteamID spewSteamID;
		static GCSDK::ESOCacheEvent spewEvent;
		static double spewTime = 0;
		if ( spewSteamID.IsValid() && spewSteamID == steamIDOwner && spewEvent == eEvent && Plat_FloatTime() - spewTime < 3.0f )
		{
			; // same event
		}
		else
		{
			spewSteamID = steamIDOwner;
			spewEvent = eEvent;
			spewTime = Plat_FloatTime();
			Msg("CPlayerInventory::SOUpdated %s [event = %u]\n", steamIDOwner.Render(), eEvent );
		}
	}
#endif

	if ( owner != m_OwnerID )
		return;

	// We shouldn't get these notifications unless we're subscribed, right?
	if ( m_pSOCache == NULL)
	{
		Assert( m_pSOCache );
		return;
	}

	// Don't bother unless it's an incremental notification.
	// For mass updates, we'll do everything more efficiently in one place
	if ( eEvent != GCSDK::eSOCacheEvent_Incremental )
	{
		Assert( eEvent == GCSDK::eSOCacheEvent_Subscribed || eEvent == GCSDK::eSOCacheEvent_Resubscribed );
		return;
	}


	bool bChanged = false;

	switch ( pObject->GetTypeID() )
	{
	case CEconItem::k_nTypeID:
	{
		CEconItem *pEconItem = (CEconItem *)pObject;

		CEconItemView *pScriptItem = GetInventoryItemByItemID( pEconItem->GetItemID() );
		if ( pScriptItem )
		{
			if ( FilloutItemFromEconItem( pScriptItem, pEconItem ) )
			{
				ItemHasBeenUpdated( pScriptItem, false, false, k_EInventoryItemUpdated );
			}

			bChanged = true;
		}
		else
		{
			// The item isn't in this inventory right now. But it may need to be 
			// after the update, so try adding it and see if the inventory wants it.
			bChanged = AddEconItem( pEconItem, false, false, false );
		}
	}
	break;
	case CEconDefaultEquippedDefinitionInstanceClient::k_nTypeID:
	{
		CEconDefaultEquippedDefinitionInstanceClient *pDefaultEquippedDefinitionInstance = (CEconDefaultEquippedDefinitionInstanceClient *)pObject;
		SetDefaultEquippedDefinitionItemBySlot( pDefaultEquippedDefinitionInstance->Obj().class_id(), 
												pDefaultEquippedDefinitionInstance->Obj().slot_id(), 
												pDefaultEquippedDefinitionInstance->Obj().item_definition() );

		DefaultEquippedDefinitionHasBeenUpdated( pDefaultEquippedDefinitionInstance );

#ifdef CLIENT_DLL
		if ( econ_debug_loadout_ui.GetBool() )
		{
			const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinition( pDefaultEquippedDefinitionInstance->Obj().item_definition() );
			if ( pDef )
			{
				ConColorMsg( Color( 205, 0, 205, 255 ), "LOADOUT GC UPDATE BaseItem \"%s\" to slot: %i\n", pDef->GetDefinitionName(), pDefaultEquippedDefinitionInstance->Obj().slot_id() );
			}
		}
#endif
#ifdef GAME_DLL
		bChanged = true;
#endif
	}
	break;
	case CEconPersonaDataPublic::k_nTypeID:
	{
#ifdef GAME_DLL
		bChanged = true;
#endif
	}
	break;
	}

	if ( bChanged )
	{
#ifdef CLIENT_DLL
		// Client doesn't update inventory while items are moving in a backpack sort. Does it once at the sort end instead.
		if ( !InventoryManager()->IsInBackpackSort() )
#endif
		{
			SendInventoryUpdateEvent();
		}
#ifdef _DEBUG
		if ( item_inventory_debug.GetBool() )
		{
			DumpInventoryToConsole( true );
		}
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: Removes the script item associated with this econ item
//-----------------------------------------------------------------------------
void CPlayerInventory::SODestroyed( SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )
{
#ifdef _DEBUG
	Msg("CPlayerInventory::SODestroyed %s [event = %u]\n", CSteamID( owner.ID() ).Render(), eEvent );
#endif

	if( pObject->GetTypeID() != CEconItem::k_nTypeID )
		return;
	if ( owner != m_OwnerID )
		return;

	// We shouldn't get these notifications unless we're subscribed, right?
	if ( m_pSOCache == NULL)
	{
		Assert( m_pSOCache );
		return;
	}

	// Don't bother unless it's an incremental notification.
	// For mass updates, we'll do everything more efficiently in one place
	if ( eEvent != GCSDK::eSOCacheEvent_Incremental )
	{
		Assert( eEvent == GCSDK::eSOCacheEvent_Subscribed || eEvent == GCSDK::eSOCacheEvent_Resubscribed );
		return;
	}

	CEconItem *pEconItem = (CEconItem *)pObject;
	RemoveItem( pEconItem->GetItemID() );

#ifdef CLIENT_DLL
	InventoryManager()->OnItemDeleted( this );
#endif

	SendInventoryUpdateEvent();
}


//-----------------------------------------------------------------------------
// Purpose: This is our initial notification that this cache has been received
//			from the server.
//-----------------------------------------------------------------------------
void CPlayerInventory::SOCacheSubscribed( SOID_t owner, GCSDK::ESOCacheEvent eEvent )
{
	/** Removed for partner depot **/
	return;
}

bool CInventoryManager::IsValidPlayerClass( equipped_class_t unClass )
{
	const bool bResult = ItemSystem()->GetItemSchema()->IsValidClass( unClass );
	AssertMsg( bResult, "Invalid player class!" );
	return bResult;
}

bool CInventoryManager::IsValidSlot( equipped_slot_t unSlot )
{
	const bool bResult = ItemSystem()->GetItemSchema()->IsValidItemSlot( unSlot );
	AssertMsg( bResult, "Invalid item slot!" );
	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Removes the script item associated with this econ item
//-----------------------------------------------------------------------------
void CPlayerInventory::ValidateInventoryPositions( void )
{
#ifdef TF2
	if ( engine->GetAppID() == 520 )
	{
		TFInventoryManager()->DeleteUnknowns( this );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerInventory::ItemHasBeenUpdated( CEconItemView *pItem, bool bUpdateAckFile, bool bWriteAckFile, EInventoryItemEvent eEventType )
{
#ifdef CLIENT_DLL
	// Handle the clientside ack file
	if ( bUpdateAckFile && !IsUnacknowledged(pItem->GetInventoryPosition()) )
	{
		if ( InventoryManager()->GetLocalInventory() == this )
		{
			InventoryManager()->SetAckedByGC( pItem, bWriteAckFile );
		}
	}
#endif
}

void CPlayerInventory::DefaultEquippedDefinitionHasBeenUpdated( CEconDefaultEquippedDefinitionInstanceClient *pDefaultEquippedDefinition )
{
}

void CPlayerInventory::MarkSetItemDescriptionsDirty( int nItemSetIndex )
{
	int nInventoryCount = GetItemCount();
	for ( int i = 0; i < nInventoryCount; i++ )
	{
		CEconItemView *pPotentialSetItem = GetItem( i );
		if ( !pPotentialSetItem )
			continue;

		if ( pPotentialSetItem->GetItemSetIndex() == nItemSetIndex )
		{
			pPotentialSetItem->MarkDescriptionDirty();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPlayerInventory::SOCacheUnsubscribed( GCSDK::SOID_t ID, GCSDK::ESOCacheEvent eEvent )
{
#ifdef _DEBUG
	Msg("CPlayerInventory::SOCacheUnsubscribed %s [event = %u]\n", CSteamID( ID.ID() ).Render(), eEvent );
#endif

	if ( ID != m_OwnerID )
		return;

	m_bCurrentlySubscribedToSteam = false;

	/*
	[ Vitaliy / Oct-2015 ] --- no longer dropping all references to everything just because we got unsubscribed of our
	user's Steam SO cache. All the items are still valid to be used even though we might not be getting updates of new items.
	This can happen if user's GC session drops for some reason, but GS session is still active.
	m_pSOCache = NULL;
	m_pRecipeCache = NULL;
	m_Items.Purge();
	m_aDefaultEquippedDefinitionItems.PurgeAndDeleteElements();
	*/
}


//-----------------------------------------------------------------------------
// Purpose: On the client this sends the "inventory_updated" event. On the server
//			it does nothing.
//-----------------------------------------------------------------------------
void CPlayerInventory::SendInventoryUpdateEvent()
{
#ifdef CLIENT_DLL
	IGameEvent *event = gameeventmanager->CreateEvent( "inventory_updated" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
#endif
#if defined( GAME_DLL ) && !defined( NO_STEAM_GAMECOORDINATOR )
	extern void OnInventoryUpdatedForSteamID( CSteamID steamID );
	OnInventoryUpdatedForSteamID( GetSteamIDFromSOID( m_OwnerID ) );
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Fills out all the fields in the script item based on what's in the
//			econ item
//-----------------------------------------------------------------------------
bool CPlayerInventory::FilloutItemFromEconItem( CEconItemView *pScriptItem, CEconItem *pEconItem )
{
	// We need to detect the case where items have been updated & moved bags / positions.
	uint32 iOldPos = pScriptItem->GetInventoryPosition();
	bool bWasInThisBag = ItemShouldBeIncluded( iOldPos );

	// Ignore items that this inventory doesn't care about
	if ( !ItemShouldBeIncluded( pEconItem->GetInventoryToken() ) )
	{
		// The item has been moved out of this bag. Ensure our derived inventory classes know.
		if ( bWasInThisBag )
		{
			// We need to update it before it's removed.
			ItemHasBeenUpdated( pScriptItem, false, false, k_EInventoryItemDestroyed );

			RemoveItem( pEconItem->GetItemID() );
		}

		return false;
	}

	pScriptItem->Init( pEconItem->GetDefinitionIndex(), pEconItem->GetQuality(), pEconItem->GetItemLevel(), pEconItem->GetAccountID() );
	if ( !pScriptItem->IsValid() )
		return false;

	pScriptItem->SetItemID( pEconItem->GetItemID() );

	pScriptItem->SetInventoryPosition( pEconItem->GetInventoryToken() );
	OnItemChangedPosition( pScriptItem, iOldPos );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerInventory::DumpInventoryToConsole( bool bRoot )
{
	if ( bRoot )
	{
#ifdef CLIENT_DLL
		Msg("(CLIENT) Inventory:\n");
#else
		Msg("(SERVER) Inventory for account (%d):\n", GetSteamIDFromSOID( m_OwnerID ).GetAccountID() );
#endif
		Msg("  Version: %llu:\n", m_pSOCache ? m_pSOCache->GetVersion() : -1 );
	}

	int iCount = m_Items.GetItemVector().Count();
	Msg("   Num items: %d\n", iCount );
	for ( int i = 0; i < iCount; i++ )
	{
		Msg( "      %s (ID %llu)\n", m_Items.GetItemVector()[ i ]->GetStaticData()->GetDefinitionName(), m_Items.GetItemVector()[ i ]->GetItemID() );
	}

	iCount = m_aDefaultEquippedDefinitionItems.Count();
	Msg("   Default equipped definitions: %d\n", iCount );
	for ( int i = 0; i < iCount; i++ )
	{
		Msg( "      %s (ID %llu)\n", m_aDefaultEquippedDefinitionItems[ i ]->GetStaticData()->GetDefinitionName(), m_Items.GetItemVector()[ i ]->GetItemID() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPlayerInventory::RemoveItem( itemid_t iItemID )
{
	CEconItemView *pItem = GetInventoryItemByItemID( iItemID );
	if ( pItem )
	{
		int nItemSet = pItem->GetItemSetIndex();
		if ( nItemSet )
		{
			const CEconItemSetDefinition *pItemSetDef = GetItemSchema()->GetItemSetByIndex( nItemSet );
			if ( pItemSetDef && pItemSetDef->m_bIsCollection )
			{
				MarkSetItemDescriptionsDirty( nItemSet );
			}
		}

		m_Items.Remove( iItemID );

#ifdef _DEBUG
		if ( item_inventory_debug.GetBool() )
		{
			DumpInventoryToConsole( true );
		}
#endif
	}

	// Don't need to resort because items will still be in order
}

//-----------------------------------------------------------------------------
// Purpose: Finds the item in our inventory that matches the specified global index
//-----------------------------------------------------------------------------
CEconItemView *CPlayerInventory::GetInventoryItemByItemID( itemid_t iIndex ) const
{
	// Early out for weapon ID 0
	if ( iIndex == 0 )
		return NULL;

	ItemIdToItemMap_t::IndexType_t idx  = m_Items.GetItemMap().Find( iIndex );
	return m_Items.GetItemMap().IsValidIndex( idx ) ? m_Items.GetItemMap()[ idx ] : NULL;
	/*
	int iCount = m_aInventoryItems.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		if ( m_aInventoryItems[i]->GetItemID() == iIndex )
		{
			if ( pIndex )
			{
				*pIndex = i;
			}

			return m_aInventoryItems[i];
		}
	}

	return NULL;
	*/
}

void CPlayerInventory::FindItemsByType( EItemType eType, CUtlVector< CEconItemView* >& outVec )
{
	for ( int i = 0; i < m_Items.GetItemVector().Count(); ++i )
	{
		if ( m_Items.GetItemVector()[ i ]->GetItemDefinition()->GetItemType() == eType )
		{
			outVec.AddToTail( m_Items.GetItemVector()[ i ] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Finds the item in our inventory in the specified position
//-----------------------------------------------------------------------------
CEconItemView *CPlayerInventory::GetItemByPosition( int iPosition, int *pIndex )
{
	int iCount = m_Items.GetItemVector().Count();
	for ( int i = 0; i < iCount; i++ )
	{
		if ( m_Items.GetItemVector()[ i ]->GetInventoryPosition() == ( unsigned int ) iPosition )
		{
			if ( pIndex )
			{
				*pIndex = i;
			}

			return m_Items.GetItemVector()[ i ];
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get the index for the item in our inventory utlvector
//-----------------------------------------------------------------------------
int	CPlayerInventory::GetIndexForItem( CEconItemView *pItem )
{
	int iCount = m_Items.GetItemVector().Count();
	for ( int i = 0; i < iCount; i++ )
	{
		if ( m_Items.GetItemVector()[ i ]->GetItemID() == pItem->GetItemID() )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Get the item object cache data for the specified item
//-----------------------------------------------------------------------------
CEconItem *CPlayerInventory::GetSOCDataForItem( itemid_t iItemID ) 
{
	if ( iItemID == 0 )
		return NULL;

	CEconItem* pData = NULL;
	if ( !m_pSOCache )
	{
		pData = GetEconItemFromStringTable( iItemID );
	}

#ifdef CLIENT_DLL
	// Don't access the live inventory data when playing a demo
	bool bIsLiveBroadcast = engine->GetDemoPlaybackParameters() && engine->GetDemoPlaybackParameters()->m_bPlayingLiveRemoteBroadcast;
	if ( ( engine->IsPlayingDemo() && !bIsLiveBroadcast ) || ( engine->IsHLTV() && this != InventoryManager()->GetLocalInventory() ) )
		return pData;
#endif

	if ( m_pSOCache && !pData )
	{
		CEconItem soIndex;
		soIndex.SetItemID( iItemID );
		pData = (CEconItem*) m_pSOCache->FindSharedObject( soIndex );
	}

	return pData;
}

const CEconItem *CPlayerInventory::GetSOCDataForItem( itemid_t iItemID ) const
{ 
	if ( iItemID == 0 )
		return NULL;

	CEconItem* pData = NULL;
	if ( !m_pSOCache )
	{
		pData = GetEconItemFromStringTable( iItemID );
	}

	if ( m_pSOCache && !pData )
	{
		CEconItem soIndex;
		soIndex.SetItemID( iItemID );
		pData = (CEconItem*) m_pSOCache->FindSharedObject( soIndex );
	}

	return pData;
}

#if defined (_DEBUG) && defined(CLIENT_DLL)
CON_COMMAND_F( item_deleteall, "WARNING: Removes all of the items in your inventory.", FCVAR_CHEAT )
{
	CPlayerInventory *pInventory = InventoryManager()->GetLocalInventory();
	if ( !pInventory )
		return;

	int iCount = pInventory->GetItemCount();
	for ( int i = 0; i < iCount; i++ )
	{
		CEconItemView *pItem = pInventory->GetItem(i);
		if ( pItem )
		{
			InventoryManager()->DeleteItem( pItem->GetItemID() );
		}
	}

	InventoryManager()->UpdateLocalInventory();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint32 CPlayerInventory::GetItemCount( uint32 unItemDef )
{
	int iOwnedCount = 0;

	int iCount = GetItemCount();
	for ( int i = 0; i < iCount; i++ )
	{
		CEconItemView *pTmp = GetItem( i );
		if ( !pTmp )
			continue;

		if ( pTmp->GetItemDefinition()->GetDefinitionIndex() == unItemDef )
			iOwnedCount++;
	}

	return iOwnedCount;
}

CEconItemView* CPlayerInventory::FindDefaultEquippedDefinitionItemBySlot( int iClass, int iSlot ) const
{
	uint32 nSlotValue = iClass * LOADOUT_POSITION_COUNT + iSlot;

	FOR_EACH_VEC( m_aDefaultEquippedDefinitionItems, i )
	{
		if ( m_aDefaultEquippedDefinitionItems[ i ]->GetInventoryPosition() == nSlotValue )
		{
			return m_aDefaultEquippedDefinitionItems[ i ];
		}
	}

	return NULL;
}

bool CPlayerInventory::GetDefaultEquippedDefinitionItemSlotByDefinitionIndex( item_definition_index_t nDefIndex, int nClass, int &nSlot )
{
	FOR_EACH_VEC( m_aDefaultEquippedDefinitionItems, i )
	{
		if ( m_aDefaultEquippedDefinitionItems[ i ]->GetItemDefinition()->GetDefinitionIndex() == nDefIndex )
		{
			int nClassTemp = m_aDefaultEquippedDefinitionItems[ i ]->GetInventoryPosition() / LOADOUT_POSITION_COUNT;
			if ( nClassTemp == nClass )
			{
				nSlot = m_aDefaultEquippedDefinitionItems[ i ]->GetInventoryPosition() - nClass * LOADOUT_POSITION_COUNT;
				return true;
			}
		}
	}

	return false;
}

void CPlayerInventory::SetDefaultEquippedDefinitionItemBySlot( int iClass, int iSlot, item_definition_index_t iDefIndex )
{
	uint32 nInventoryPosition = iClass * LOADOUT_POSITION_COUNT + iSlot;

	FOR_EACH_VEC( m_aDefaultEquippedDefinitionItems, i )
	{
		CEconItemView *pItemView = m_aDefaultEquippedDefinitionItems[ i ];
		if ( pItemView->GetInventoryPosition() == nInventoryPosition )
		{
			if ( iDefIndex == 0 )
			{
				delete m_aDefaultEquippedDefinitionItems[ i ];
				m_aDefaultEquippedDefinitionItems.Remove( i );
				return;
			}
			else
			{
				pItemView->Init( iDefIndex, AE_UNIQUE, AE_USE_SCRIPT_VALUE, true );
				return;
			}
		}
	}

	if ( iDefIndex == 0 || iSlot == INVALID_EQUIPPED_SLOT )
		return;

	CEconItemView *pNewItem = new CEconItemView;
	pNewItem->Init( iDefIndex, AE_UNIQUE, AE_USE_SCRIPT_VALUE, true );

	if ( !pNewItem->IsValid() )
		return;

	int nIndex = m_aDefaultEquippedDefinitionItems.AddToTail( pNewItem );
	m_aDefaultEquippedDefinitionItems[ nIndex ]->SetInventoryPosition( nInventoryPosition );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CPlayerInventory::GetRecipeCount() const
{
	const CUtlMap<int, CEconCraftingRecipeDefinition *, int, CDefLess<int> >& mapRecipes = ItemSystem()->GetItemSchema()->GetRecipeDefinitionMap();

	return mapRecipes.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const CEconCraftingRecipeDefinition *CPlayerInventory::GetRecipeDef( int iIndex ) const
{
	if ( !m_pSOCache )
		return NULL;

	if ( iIndex < 0 || iIndex >= GetRecipeCount() )
		return NULL;

	const CUtlMap<int, CEconCraftingRecipeDefinition *, int, CDefLess<int> >& mapRecipes = ItemSystem()->GetItemSchema()->GetRecipeDefinitionMap();

	// Store off separate index for "number of items iterated over" in case something
	// deletes from the recipes map out from under us.
	int j = 0;
	FOR_EACH_MAP_FAST( mapRecipes, i )
	{
		if ( j == iIndex )
			return mapRecipes[i];

		j++;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const CEconCraftingRecipeDefinition *CPlayerInventory::GetRecipeDefByDefIndex( uint16 iDefIndex ) const
{
	if ( !m_pSOCache )
		return NULL;

	// check always-known recipes
	const CUtlMap<int, CEconCraftingRecipeDefinition *, int, CDefLess<int> >& mapRecipes = ItemSystem()->GetItemSchema()->GetRecipeDefinitionMap();
	int i = mapRecipes.Find( iDefIndex );
	if ( i != mapRecipes.InvalidIndex() )
		return mapRecipes[i];

	return NULL;
}

int CPlayerInventory::GetMaxCraftIngredientsNeeded( const CUtlVector< itemid_t >& vecCraftItems ) const
{
	int nMax = 0;
	int nPossible = GetPossibleCraftResultsCount( vecCraftItems );

	for ( int i = 0; i < nPossible; ++i )
	{
		const CEconCraftingRecipeDefinition *pRecipe = GetPossibleCraftResult( vecCraftItems, i );
		if ( nMax < pRecipe->GetTotalInputItemsRequired() )
		{
			nMax = pRecipe->GetTotalInputItemsRequired();
		}
	}

	return nMax;
}

void CPlayerInventory::GetMarketCraftCompletionLink( const CUtlVector< itemid_t >& vecCraftItems, char *pchLink, int nNumChars ) const
{
	int nCollection = -2;

	bool bWeapons = false;
	bool bNonWeapons = false;
	bool bStatTrak = false;
	FOR_EACH_VEC( vecCraftItems, nCraftItem )
	{
		const CEconItem *pEconItem = GetSOCDataForItem( vecCraftItems[ nCraftItem ] );
		if ( !pEconItem )
			continue;

		if ( ( StringHasPrefix( pEconItem->GetItemDefinition()->GetDefinitionName(), "weapon_" ) ) )
		{
			bWeapons = true;
		}
		else
		{
			bNonWeapons = true;
		}

		if ( !bStatTrak )
		{
			static CSchemaAttributeDefHandle pAttr_KillEater( "kill eater" );
			if ( pEconItem->FindAttribute( pAttr_KillEater ) )
				bStatTrak = true;
		}

		if ( nCollection )
		{
			// Found the first collection type
			nCollection = pEconItem->GetItemSetIndex();
		}
		else if ( nCollection != pEconItem->GetItemSetIndex() )
		{
			// Found more than 1 collection type
			nCollection = -1;
		}
	}

	int nRarity = -2;
	int nPossibleCount = GetPossibleCraftResultsCount( vecCraftItems );

	// We can only match a collection if all the items have a matching collection and there's at leas 1 possible craft outcome
	bool bMatchCollection = nCollection > 0 && nPossibleCount > 0;

	for ( int i = 0; i < nPossibleCount; ++i )
	{
		const CEconCraftingRecipeDefinition *pRecipe = GetPossibleCraftResult( vecCraftItems, i );

		// Can only do a rarity if everything is all weapons or all non-weapons since they localize differently
		if ( ( bWeapons && !bNonWeapons ) || ( !bWeapons && bNonWeapons ) )
		{
			const CUtlVector< CItemSelectionCriteria > *pInputItems = pRecipe->GetInputItems();
			FOR_EACH_VEC( *pInputItems, nInput )
			{
				const CItemSelectionCriteria *pInput = &((*pInputItems)[ nInput ]);

				// RARITY

				int nTempRarity = -1;

				if ( pInput->BRaritySet() )
				{
					nTempRarity = pInput->GetRarity();
				}
				else
				{
					const char *pchRarity = pInput->GetValueForFirstConditionOfFieldName( "*rarity" );
					if ( pchRarity )
					{
						const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinitionByName( pchRarity );
						if ( pRarity )
						{
							nTempRarity = pRarity->GetDBValue();
						}
					}
				}

				if ( nRarity == -2 )
				{
					nRarity = nTempRarity;
				}
				else if ( nRarity != nTempRarity )
				{
					nRarity = -1;
				}
			}
		}

		if ( bMatchCollection )
		{
			const CUtlVector< CItemSelectionCriteria > *pOutputItems = &(pRecipe->GetOutputItems());
			FOR_EACH_VEC( *pOutputItems, nOutput )
			{
				const CItemSelectionCriteria *pOutput = &((*pOutputItems)[ nOutput ]);

				if ( pOutput->GetValueForFirstConditionOfFieldName( "*match_set_rarity" ) == NULL )
				{
					bMatchCollection = false;
				}
			}
		}
	}

	V_snprintf( pchLink, nNumChars, "http://%ssteamcommunity.com/market/search?appid=730&lock_appid=730&q=",
		( steamapicontext->SteamUtils()->GetConnectedUniverse() == k_EUniversePublic ) ? "" : "beta." );

	if ( nRarity >= 0 || bMatchCollection || bStatTrak )
	{
		// Special search tags
		// http://steamcommunity.com/market/search?appid=730&q=%22The+Arms+Deal+Collection%22+%22Mil-Spec+Grade%22
		const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinition( nRarity );
		if ( pRarity )
		{	// see "econ_assetapi_context.cpp : Rarity Tag"
			// &category_730_Rarity%5B%5D=tag_Rarity_Ancient_Weapon
			V_strncat( pchLink, "&category_730_Rarity%5B%5D=tag_", nNumChars );
			const char *pchLocKey = pRarity->GetLocKey();
			if ( ( nRarity != 7 ) && bWeapons )
				pchLocKey = pRarity->GetWepLocKey();
			V_strncat( pchLink, pchLocKey, nNumChars );
		}

		if ( bMatchCollection )
		{
			// &category_730_ItemSet%5B%5D=tag_set_bravo_i
			const IEconItemSetDefinition *pCollection = GetItemSchema()->GetItemSet( nCollection );
			if ( pCollection )
			{	// see "econ_assetapi_context.cpp : ItemSet"
				char const *pchName = pCollection->GetName();
				V_strncat( pchLink, "&category_730_ItemSet%5B%5D=tag_", nNumChars );
				V_strncat( pchLink, pchName, nNumChars );
			}
		}

		if ( bStatTrak )
		{
			V_strncat( pchLink, "&category_730_Quality%5B%5D=tag_strange", nNumChars );
		}

		// Replace spaces with +
		char *pCurrentChar = pchLink;
		while ( *pCurrentChar != '\0' )
		{
			if ( *pCurrentChar == ' ' )
			{
				*pCurrentChar = '+';
			}

			pCurrentChar++;
		}
	}

	V_strncat( pchLink, "&", nNumChars ); // this allows us to defeat PHP redirects duplicating GETPARAMS!
}

int CPlayerInventory::GetPossibleCraftResultsCount( const CUtlVector< itemid_t >& vecCraftItems ) const
{
	if ( m_nTargetRecipe >= 0 )
		return 1;

	CUtlVector< CEconItem* > vecCraftEconItems;
	FOR_EACH_VEC( vecCraftItems, i )
	{
		const CEconItem *pEconItem = GetSOCDataForItem( vecCraftItems[ i ] );
		if ( !pEconItem )
			continue;

		vecCraftEconItems.AddToTail( const_cast< CEconItem* >( pEconItem ) );
	}

	return GetPossibleCraftResultsCount( vecCraftEconItems );
}

int CPlayerInventory::GetPossibleCraftResultsCount( const CUtlVector< CEconItem* >& vecCraftEconItems ) const
{
	if ( m_nTargetRecipe >= 0 )
		return 1;

	int nPossibleRecipes = 0;

	int nRecipeCount = GetRecipeCount();
	for ( int i = 0; i < nRecipeCount; ++i )
	{
		const CEconCraftingRecipeDefinition *pRecipe = GetRecipeDef( i );
		if ( !pRecipe || pRecipe->IsDisabled() )
			continue;

		if ( m_nTargetRecipe < -1 && m_nTargetRecipe != pRecipe->GetFilter() )
			continue;

		// Early out if it uses less items than we're crafting
		if ( pRecipe->GetTotalInputItemsRequired() < vecCraftEconItems.Count() )
			continue;

		// If it has any items do they conflict?
		if ( vecCraftEconItems.Count() > 0 && !pRecipe->ItemListMatchesInputs( vecCraftEconItems, true ) )
			continue;

		nPossibleRecipes++;
	}

	return nPossibleRecipes;
}

const CEconCraftingRecipeDefinition *CPlayerInventory::GetPossibleCraftResult( const CUtlVector< itemid_t >& vecCraftItems, int nIndex ) const
{
	if ( m_nTargetRecipe >= 0 )
	{
		if ( nIndex > 0 )
			return NULL;

		return GetRecipeDef( m_nTargetRecipe );
	}

	int nPossibleRecipes = 0;

	CUtlVector< CEconItem* > vecCraftEconItems;
	FOR_EACH_VEC( vecCraftItems, i )
	{
		const CEconItem *pEconItem = GetSOCDataForItem( vecCraftItems[ i ] );
		if ( !pEconItem )
			continue;

		vecCraftEconItems.AddToTail( const_cast< CEconItem* >( pEconItem ) );
	}

	int nRecipeCount = GetRecipeCount();
	for ( int i = 0; i < nRecipeCount; ++i )
	{
		const CEconCraftingRecipeDefinition *pRecipe = GetRecipeDef( i );
		if ( !pRecipe || pRecipe->IsDisabled() )
			continue;

		if ( m_nTargetRecipe < -1 && m_nTargetRecipe != pRecipe->GetFilter() )
			continue;

		// Early out if it uses less items than we're crafting
		if ( pRecipe->GetTotalInputItemsRequired() < vecCraftEconItems.Count() )
			continue;

		// If it has any items do they conflict?
		if ( vecCraftEconItems.Count() > 0 && !pRecipe->ItemListMatchesInputs( vecCraftEconItems, true ) )
			continue;

		if ( nPossibleRecipes == nIndex )
		{
			return pRecipe;
		}

		nPossibleRecipes++;
	}

	return NULL;
}

int CPlayerInventory::GetPossibleCraftResultID( const CUtlVector< itemid_t >& vecCraftItems, int nIndex ) const
{
	const CEconCraftingRecipeDefinition *pRecipe = GetPossibleCraftResult( vecCraftItems, nIndex );
	if ( !pRecipe )
		return -1;

	return pRecipe->GetDefinitionIndex();
}

const wchar_t* CPlayerInventory::GetPossibleCraftResultName( const CUtlVector< itemid_t >& vecCraftItems, int nIndex ) const
{
	const CEconCraftingRecipeDefinition *pRecipe = GetPossibleCraftResult( vecCraftItems, nIndex );
	if ( !pRecipe )
		return L"";

	return pRecipe->GetLocName();
}

const wchar_t* CPlayerInventory::GetPossibleCraftResultDescription( const CUtlVector< itemid_t >& vecCraftItems, int nIndex ) const
{
	const CEconCraftingRecipeDefinition *pRecipe = GetPossibleCraftResult( vecCraftItems, nIndex );
	if ( !pRecipe )
		return L"";

	return pRecipe->GetLocDescription();
}

bool CPlayerInventory::CanAddToCraftTarget( const CUtlVector< itemid_t >& vecCraftItems, itemid_t nNewItem ) const
{
	const CEconCraftingRecipeDefinition *pRecipe = NULL;

	if ( m_nTargetRecipe < 0 )
	{
		if ( m_nTargetRecipe != CRAFT_FILTER_COLLECT && m_nTargetRecipe != CRAFT_FILTER_TRADEUP )
		{
			return true;
		}
	}
	else
	{
		pRecipe = GetRecipeDef( m_nTargetRecipe );
		if ( !pRecipe || pRecipe->IsDisabled() )
			return false;
	}

	const CEconItem *pEconItem = GetSOCDataForItem( nNewItem );
	if ( !pEconItem )
		return false;

	int nSet = pEconItem->GetItemSetIndex();

	CUtlVector< CEconItem* > vecCraftEconItems;
	FOR_EACH_VEC( vecCraftItems, i )
	{
		const CEconItem *pTempEconItem = GetSOCDataForItem( vecCraftItems[ i ] );
		if ( !pTempEconItem )
			continue;

		// You can't put an item in twice!
		if ( vecCraftItems[ i ] == nNewItem )
			return false;

		if ( m_nTargetRecipe == CRAFT_FILTER_COLLECT )
		{
			// Collecting requires items from the same set!
			if ( nSet != pTempEconItem->GetItemSetIndex() )
			{
				return false;
			}
		}
		else if ( m_nTargetRecipe == CRAFT_FILTER_TRADEUP )
		{
			// Trading up requires items from the same rarity!
			if ( pEconItem->GetRarity() != pTempEconItem->GetRarity() )
			{
				return false;
			}
		}

		vecCraftEconItems.AddToTail( const_cast< CEconItem* >( pTempEconItem ) );
	}

	vecCraftEconItems.AddToTail( const_cast< CEconItem* >( pEconItem ) );

	if ( !pRecipe )
	{
		return GetPossibleCraftResultsCount( vecCraftEconItems ) > 0;
	}

	// Early out if we already have enough items in the recipe
	if ( pRecipe->GetTotalInputItemsRequired() < vecCraftEconItems.Count() )
		return false;
	
	// Does the items + the new one work for the target?
	return pRecipe->ItemListMatchesInputs( vecCraftEconItems, true );
}

bool CPlayerInventory::IsCraftReady( const CUtlVector< itemid_t >& vecCraftItems, int16 &nRecipe ) const
{
	CUtlVector< CEconItem* > vecCraftEconItems;
	FOR_EACH_VEC( vecCraftItems, i )
	{
		const CEconItem *pEconItem = GetSOCDataForItem( vecCraftItems[ i ] );
		if ( !pEconItem )
			continue;

		vecCraftEconItems.AddToTail( const_cast< CEconItem* >( pEconItem ) );
	}

	if ( m_nTargetRecipe >= 0 )
	{
		const CEconCraftingRecipeDefinition *pRecipe = GetRecipeDef( m_nTargetRecipe );
		if ( !pRecipe || pRecipe->IsDisabled() )
			return false;

		// Early out if it uses less items than we're crafting
		if ( pRecipe->GetTotalInputItemsRequired() < vecCraftEconItems.Count() )
			return false;

		// If it has any items do they conflict?
		if ( vecCraftEconItems.Count() > 0 && !pRecipe->ItemListMatchesInputs( vecCraftEconItems ) )
			return false;

		nRecipe = m_nTargetRecipe;
		return true;
	}

	bool bIsReady = false;

	int nRecipeCount = GetRecipeCount();
	for ( int i = 0; i < nRecipeCount; ++i )
	{
		const CEconCraftingRecipeDefinition *pRecipe = GetRecipeDef( i );
		if ( !pRecipe || pRecipe->IsDisabled() )
			continue;

		// Early out if it uses less items than we're crafting
		if ( pRecipe->GetTotalInputItemsRequired() < vecCraftEconItems.Count() )
			continue;

		// If it has any items do they conflict?
		if ( vecCraftEconItems.Count() > 0 && !pRecipe->ItemListMatchesInputs( vecCraftEconItems ) )
			continue;

		if ( !bIsReady )
		{
			bIsReady = true;
			nRecipe = pRecipe->GetDefinitionIndex();
		}
		else
		{
			// Found 2 valid recipes!
			return false;
		}
	}

	return bIsReady;
}

void CPlayerInventory::CraftIngredients( const CUtlVector< itemid_t >& vecCraftItems ) const
{
	/** Removed for partner depot **/
}

void CPlayerInventory::ClearItemCustomName( itemid_t iIndex )
{
	/** Removed for partner depot **/
}

void CPlayerInventory::WearItemSticker( itemid_t iIndex, int nSlot, float flStickerWearCurrent )
{
	/** Removed for partner depot **/
}


void CPlayerInventory::CItemContainers::Add( CEconItemView* pItem )
{
	Assert( DbgValidate() );
	m_vecInventoryItems.AddToTail( pItem );
	m_mapItemIDToItemView.Insert( pItem->GetItemID(), pItem );
	Assert( DbgValidate() );
}

void CPlayerInventory::CItemContainers::Remove( itemid_t ullItemID )
{
	Assert( DbgValidate() );
	CUtlVectorFixedGrowable< CEconItemView *, 2 > arrPurgeAndDeleteItems;
	for ( ItemIdToItemMap_t::IndexType_t uiMapIdx = m_mapItemIDToItemView.Find( ullItemID );
		( uiMapIdx != m_mapItemIDToItemView.InvalidIndex() ) &&
		( m_mapItemIDToItemView.Key( uiMapIdx ) == ullItemID );
	)
	{
		arrPurgeAndDeleteItems.AddToTail( m_mapItemIDToItemView.Element( uiMapIdx ) );
		ItemIdToItemMap_t::IndexType_t uiNextMapIdx = m_mapItemIDToItemView.NextInorder( uiMapIdx );
		m_vecInventoryItems.FindAndRemove( m_mapItemIDToItemView.Element( uiMapIdx ) );
		m_mapItemIDToItemView.RemoveAt( uiMapIdx );
		uiMapIdx = uiNextMapIdx;
	}
	arrPurgeAndDeleteItems.PurgeAndDeleteElements();
	Assert( DbgValidate() );
}

void CPlayerInventory::CItemContainers::Purge()
{
	m_mapItemIDToItemView.Purge();
	m_vecInventoryItems.PurgeAndDeleteElements();
}

// Make sure each element in vector can be found in map and the pointers match, vise versa
// If we add more containers this will need to be rewritten to require all are synced.
bool CPlayerInventory::CItemContainers::DbgValidate() const
{
	CUtlRBTree< CEconItemView *, int, CDefLess< CEconItemView * > > rbItems;
	FOR_EACH_VEC( m_vecInventoryItems, i )
	{
		int idx = rbItems.InsertIfNotFound( m_vecInventoryItems[ i ] );
		if ( idx == rbItems.InvalidIndex() )
		{
			DebuggerBreakIfDebugging();
			return false;
		}
	}
	FOR_EACH_MAP( m_mapItemIDToItemView, i )
	{
		CEconItemView *pElem = m_mapItemIDToItemView.Element( i );
		if ( pElem->GetItemID() != m_mapItemIDToItemView.Key( i ) )
		{
			DebuggerBreakIfDebugging();
			return false;
		}
		if ( !rbItems.Remove( pElem ) )
		{
			DebuggerBreakIfDebugging();
			return false;
		}
	}
	if ( rbItems.Count() )
	{
		DebuggerBreakIfDebugging();
		return false;
	}

	return true;
}
