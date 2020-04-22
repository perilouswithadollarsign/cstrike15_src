//========= Copyright (c) 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "cstrike15_item_inventory.h"
#include "econ_entity_creation.h"
#include "cstrike15_item_system.h"
#include "vgui/ILocalize.h"
#include "tier3/tier3.h"
#ifdef CLIENT_DLL
#include "c_cs_player.h"
#include "itempickup_scaleform.h"
#include "keyvalues.h"
#include "filesystem.h"
#include "ienginevgui.h"
#include "cs_gamerules.h"
//#include "econ_notifications.h"
//#include "econ/econ_item_preset.h"
#else
#include "cs_player.h"
#endif
#include "econ_game_account_client.h"
#include "gcsdk/gcclientsdk.h"
#include "econ_gcmessages.h"
#include "cs_gamerules.h"
#include "econ_item.h"
#include "game_item_schema.h"

#ifdef CLIENT_DLL
#include "matchmaking/imatchframework.h"
#endif // CLIENT_DLL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace GCSDK;

#ifdef CLIENT_DLL

ConVar econ_debug_loadout_ui( "econ_debug_loadout_ui", "0", FCVAR_DEVELOPMENTONLY, "Show debug data when players change their loadout." );

//-----------------------------------------------------------------------------
//class CEconNotification_HasNewItems : public CEconNotification
//{
//public:
//	CEconNotification_HasNewItems() : CEconNotification()
//	{
//		m_bHasTriggered = false;
//	}
//
//	~CEconNotification_HasNewItems()
//	{
//		if ( !m_bHasTriggered )
//		{
//			m_bHasTriggered = true;
//			PortalInventoryManager()->ShowItemsPickedUp( true, true, true );
//		}
//	}
//
//	virtual void MarkForDeletion()
//	{
//		m_bHasTriggered = true;
//
//		CEconNotification::MarkForDeletion();
//	}
//
//	virtual bool CanBeTriggered() { return true; }
//	virtual void Trigger()
//	{
//		m_bHasTriggered = true;
//		PortalInventoryManager()->ShowItemsPickedUp( true );
//		MarkForDeletion();
//	}
//
//	static bool IsNotificationType( CEconNotification *pNotification ) { return dynamic_cast< CEconNotification_HasNewItems *>( pNotification ) != NULL; }
//
//private:
//
//	bool m_bHasTriggered;
//};
#endif
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool AreSlotsConsideredIdentical( int iBaseSlot, int iTestSlot )
{
	if ( iBaseSlot == LOADOUT_POSITION_SPRAY0 )
	{
		return ( iTestSlot >= LOADOUT_POSITION_SPRAY0 && iTestSlot <= LOADOUT_POSITION_SPRAY0 /*LOADOUT_POSITION_SPRAY3*/ );
	}

	if ( iBaseSlot == LOADOUT_POSITION_SECONDARY0 )
	{
		return ( iTestSlot >= LOADOUT_POSITION_SECONDARY0 && iTestSlot <= LOADOUT_POSITION_SECONDARY5 );
	}

	if ( iBaseSlot == LOADOUT_POSITION_SMG0 )
	{
		return ( iTestSlot >= LOADOUT_POSITION_SMG0 && iTestSlot <= LOADOUT_POSITION_SMG5 );
	}

	if ( iBaseSlot == LOADOUT_POSITION_RIFLE0 )
	{
		return ( iTestSlot >= LOADOUT_POSITION_RIFLE0 && iTestSlot <= LOADOUT_POSITION_RIFLE5 );
	}

	if ( iBaseSlot == LOADOUT_POSITION_HEAVY0 )
	{
		return ( iTestSlot >= LOADOUT_POSITION_HEAVY0 && iTestSlot <= LOADOUT_POSITION_HEAVY5 );
	}

	if ( iBaseSlot == LOADOUT_POSITION_GRENADE0 )
	{
		return ( iTestSlot >= LOADOUT_POSITION_GRENADE0 && iTestSlot <= LOADOUT_POSITION_GRENADE5 );
	}

	if ( iBaseSlot == LOADOUT_POSITION_EQUIPMENT0 )
	{
		return ( iTestSlot >= LOADOUT_POSITION_EQUIPMENT0 && iTestSlot <= LOADOUT_POSITION_EQUIPMENT5 );
	}

	if ( iBaseSlot == LOADOUT_POSITION_MISC0 )
	{
		return ( iTestSlot >= LOADOUT_POSITION_MISC0 && iTestSlot <= LOADOUT_POSITION_MISC5 );
	}

	return iBaseSlot == iTestSlot;
}

//-----------------------------------------------------------------------------
CCSInventoryManager g_CSInventoryManager;
CInventoryManager *InventoryManager( void )
{
	return &g_CSInventoryManager;
}
CCSInventoryManager *CSInventoryManager( void )
{
	return &g_CSInventoryManager;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSInventoryManager::CCSInventoryManager( void )
{
#ifdef CLIENT_DLL
	m_pLocalInventory = NULL;
#endif
	// Do not renumber these indices. Stored in database. 
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_MELEE == 0 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_C4 == 1 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SECONDARY0 == 2 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SECONDARY1 == 3 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SECONDARY2 == 4 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SECONDARY3 == 5 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SECONDARY4 == 6 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SMG0 == 8 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SMG1 == 9 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SMG2 == 10 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SMG3 == 11 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SMG4 == 12 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_RIFLE0 == 14 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_RIFLE1 == 15 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_RIFLE2 == 16 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_RIFLE3 == 17 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_RIFLE4 == 18 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_RIFLE5 == 19 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_HEAVY0 == 20 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_HEAVY1 == 21 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_HEAVY2 == 22 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_HEAVY3 == 23 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_HEAVY4 == 24 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_CLOTHING_HANDS == 41 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_MUSICKIT == 54 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_FLAIR0 == 55 );
	COMPILE_TIME_ASSERT( LOADOUT_POSITION_SPRAY0 == 56 );
}

bool CCSInventoryManager::Init( void )
{
#ifdef CLIENT_DLL
	Assert( !m_pLocalInventory );
	m_pLocalInventory = dynamic_cast< CCSPlayerInventory* >( GeneratePlayerInventoryObject() );
#endif

	return BaseClass::Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSInventoryManager::PostInit( void )
{
	BaseClass::PostInit();
	GenerateBaseItems();
}

void CCSInventoryManager::Shutdown( void )
{
#ifdef CLIENT_DLL
	Assert( m_pLocalInventory );
	DestroyPlayerInventoryObject( m_pLocalInventory );
	m_pLocalInventory = NULL;
#endif

	BaseClass::Shutdown();
}

void CCSInventoryManager::LevelInitPreEntity( void )
{
	BaseClass::LevelInitPreEntity();
}

//-----------------------------------------------------------------------------
// Purpose: Generate & store the base item details for each class & loadout slot
//-----------------------------------------------------------------------------
void CCSInventoryManager::GenerateBaseItems( void )
{
	for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
	{
		for ( int slot = 0; slot < LOADOUT_POSITION_COUNT; slot++ )
		{
			bool bEligibleForDefaultItem = false; // The reason these checks are here for "bEligibleForDefaultItem" is because SlotContainsBaseItems function is shitty, doesn't take team argument and by default returns "true" which is not the desired behavior for allteams :( [fixing it at 9PM before shipping, good for now]
			switch ( iTeam )
			{
			case TEAM_CT:
			case TEAM_TERRORIST:
				bEligibleForDefaultItem = true;
				break;
			}
			switch ( slot )
			{
			case LOADOUT_POSITION_MUSICKIT:
				bEligibleForDefaultItem = true;
				break;
			}
			if ( !bEligibleForDefaultItem )
			{
				m_pBaseLoadoutItems[iTeam][slot].Invalidate();
				continue;
			}

			baseitemcriteria_t pCriteria;
			pCriteria.iClass = iTeam;
			pCriteria.iSlot = slot;
			int iItemDef = CStrike15ItemSystem()->GenerateBaseItem( &pCriteria );
			if ( iItemDef != INVALID_ITEM_INDEX )
			{
				/*
 				IHasAttributes *pItemInterface = dynamic_cast<IHasAttributes *>(pItem);
				if ( pItemInterface )
				{
					CEconItemView *pScriptItem = pItemInterface->GetAttributeManager()->GetItem();
					Msg("============================================\n");
					Msg("Inventory: Generating base item for %s, slot %s...\n", g_aPlayerClassNames_NonLocalized[iTeam], g_szLoadoutStrings[slot] );
					Msg("Generated: %s \"%s\"\n", g_szQualityStrings[pScriptItem->GetItemQuality()], pEnt->GetClassname() );
					char tempstr[1024];
					g_pVGuiLocalize->ConvertUnicodeToANSI( pScriptItem->GetAttributeDescription(), tempstr, sizeof(tempstr) );
					Msg("%s", tempstr );
					Msg("\n============================================\n");
				}
				*/

				m_pBaseLoadoutItems[iTeam][slot].Init( iItemDef, AE_USE_SCRIPT_VALUE, AE_USE_SCRIPT_VALUE, false );
			}
			else
			{
				m_pBaseLoadoutItems[iTeam][slot].Invalidate();
			}
		}
	}
}

#ifdef CLIENT_DLL
int g_nLoadoutActionBatchNum = 0;
int g_nLoadoutActionBatchLastFrame = 0;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSInventoryManager::EquipItemInLoadout( int iTeam, int iSlot, itemid_t iItemID, bool bSwap /*= false*/ )
{
	if ( !steamapicontext || !steamapicontext->SteamUser() )
		return false;

	if ( econ_debug_loadout_ui.GetBool() && g_nLoadoutActionBatchLastFrame != gpGlobals->framecount )
	{
		ConColorMsg( Color( 255, 0, 255, 255 ), "\nLOADOUT ACTION BATCH #%i\n", g_nLoadoutActionBatchNum );

		g_nLoadoutActionBatchNum++;
		g_nLoadoutActionBatchLastFrame = gpGlobals->framecount;
	}

	Assert( m_pLocalInventory );

	// If they pass in a 0 item id, we're just clearing the loadout slot
	bool bIsBaseItem = CombinedItemIdIsDefIndexAndPaint( iItemID );
	uint16 nLowDWord = CombinedItemIdGetDefIndex( iItemID );

	if ( iItemID == 0 || ( bIsBaseItem && ( nLowDWord == 0 ) ) )
	{
		if ( econ_debug_loadout_ui.GetBool() ) ConColorMsg( Color( 255, 0, 255, 255 ), "LOADOUT Clear slot: %i\n", iSlot );

		m_pLocalInventory->SetDefaultEquippedDefinitionItemBySlot( iTeam, iSlot, 0 );
		return m_pLocalInventory->ClearLoadoutSlot( iTeam, iSlot );
	}

	CEconItemView *pBaseItem = CSInventoryManager()->GetBaseItemForTeam( iTeam, iSlot );
	bool bWillClearSlot = ( pBaseItem && pBaseItem->IsValid() && bIsBaseItem && pBaseItem->GetItemDefinition()->GetDefinitionIndex() == nLowDWord );
	bool bSwapClearedASlot = false;

	CEconItemView *pItem = NULL;
	const GameItemDefinition_t *pDef = NULL;
	if ( !bIsBaseItem )
	{
		pItem = m_pLocalInventory->GetInventoryItemByItemID( iItemID );
		if ( !pItem || !pItem->IsValid() )
			return false;

		pDef = pItem->GetStaticData();

	}
	else
	{
		pDef = dynamic_cast< const GameItemDefinition_t* >( GetItemSchema()->GetItemDefinition( nLowDWord ) );
	}

	if ( !pDef )
		return false;

	if ( bWillClearSlot && pDef->SharesSlot() )
		bWillClearSlot = false;

	// We check for validity on the GC when we equip items, but we can't really trust anyone
	// and so we check here as well.
	Assert( AreSlotsConsideredIdentical( pDef->GetLoadoutSlot( iTeam ), iSlot ) );

	if ( !AreSlotsConsideredIdentical( pDef->GetLoadoutSlot( iTeam ), iSlot ) ||
		 ( pDef->GetDefaultLoadoutSlot() > -1
			&& pDef->GetDefaultLoadoutSlot() != LOADOUT_POSITION_SPRAY0		// all spray slots are considered identical for equipping (unlike weapons categories)
			&& pDef->GetDefaultLoadoutSlot() != iSlot ) ||
		 !pDef->CanBeUsedByTeam( iTeam ) )
	{
		return false;
	}

	// Do we already have an item equipped in this slot?
	CEconItemView *pItemAlreadyInPosition = NULL;
	if ( m_pLocalInventory->m_LoadoutItems[ iTeam ][ iSlot ] != LOADOUT_SLOT_USE_BASE_ITEM )
	{
		pItemAlreadyInPosition = m_pLocalInventory->GetInventoryItemByItemID( m_pLocalInventory->m_LoadoutItems[ iTeam ][ iSlot ] );
	}

	if ( bSwap )
	{
		int nSwapSlot = INVALID_EQUIPPED_SLOT;
		if ( pItem )
		{
			nSwapSlot = pItem->GetEquippedPositionForClass( iTeam );
		}
		else
		{
			m_pLocalInventory->GetDefaultEquippedDefinitionItemSlotByDefinitionIndex( nLowDWord, iTeam, nSwapSlot );
		}

		if ( nSwapSlot != INVALID_EQUIPPED_SLOT )
		{
			if ( pItemAlreadyInPosition && pItemAlreadyInPosition->IsValid() )
			{
				// Are we trying to equip the item that's already there?
				if ( pItem && pItem->GetItemID() == pItemAlreadyInPosition->GetItemID() )
					return true;

				pItemAlreadyInPosition->UpdateEquippedState( iTeam, nSwapSlot );
				m_pLocalInventory->m_LoadoutItems[ iTeam ][ nSwapSlot ] = pItemAlreadyInPosition->GetItemID();
				m_pLocalInventory->m_LoadoutItems[ iTeam ][ iSlot ] = LOADOUT_SLOT_USE_BASE_ITEM;

				if ( bWillClearSlot )
				{
					if ( econ_debug_loadout_ui.GetBool() ) ConColorMsg( Color( 255, 0, 255, 255 ), "LOADOUT UniqueItem \"%s\" to slot: %i\n", pItemAlreadyInPosition->GetItemDefinition()->GetDefinitionName(), nSwapSlot );

					// Need to move this on the backend since a swap won't happen
					UpdateInventoryEquippedState( m_pLocalInventory, pItemAlreadyInPosition->GetItemID(), iTeam, nSwapSlot, false );
				}
				else
				{
					if ( econ_debug_loadout_ui.GetBool() ) ConColorMsg( Color( 255, 0, 255, 255 ), "//LOADOUT UniqueItem \"%s\" swapped to slot: %i\n", pItemAlreadyInPosition->GetItemDefinition()->GetDefinitionName(), nSwapSlot );
				}
			}
			else
			{
				CEconItemView *pSwapItem = m_pLocalInventory->FindDefaultEquippedDefinitionItemBySlot( iTeam, iSlot );
				if ( pSwapItem )
				{
					CEconItemView *pBaseItem = CSInventoryManager()->GetBaseItemForTeam( iTeam, nSwapSlot );
					if ( pBaseItem && pBaseItem->IsValid() && pBaseItem->GetItemDefinition()->GetDefinitionIndex() == pSwapItem->GetItemDefinition()->GetDefinitionIndex() )
					{
						// Putting a default in the slot where it belongs... just clear it out
						if ( econ_debug_loadout_ui.GetBool() ) ConColorMsg( Color( 255, 0, 255, 255 ), "LOADOUT Clear slot: %i\n", nSwapSlot );

						m_pLocalInventory->SetDefaultEquippedDefinitionItemBySlot( iTeam, nSwapSlot, 0 );
						m_pLocalInventory->ClearLoadoutSlot( iTeam, nSwapSlot );

						bSwapClearedASlot = true;
					}
					else
					{
						// There's a base item in this slot

						uint64 nSwapItemId = CombinedItemIdMakeFromDefIndexAndPaint( pSwapItem->GetItemDefinition()->GetDefinitionIndex(), 0 );
						m_pLocalInventory->SetDefaultEquippedDefinitionItemBySlot( iTeam, nSwapSlot, pSwapItem->GetItemDefinition()->GetDefinitionIndex() );

						if ( bWillClearSlot )
						{
							if ( econ_debug_loadout_ui.GetBool() ) ConColorMsg( Color( 255, 0, 255, 255 ), "LOADOUT BaseItem \"%s\" to slot: %i\n", pSwapItem->GetItemDefinition()->GetDefinitionName(), nSwapSlot );

							// Need to move this on the backend since a swap won't happen
							UpdateInventoryEquippedState( m_pLocalInventory, nSwapItemId, iTeam, nSwapSlot, false );
						}
						else
						{
							if ( econ_debug_loadout_ui.GetBool() ) ConColorMsg( Color( 255, 0, 255, 255 ), "//LOADOUT BaseItem \"%s\" swapped to slot: %i\n", pSwapItem->GetItemDefinition()->GetDefinitionName(), nSwapSlot );
						}
					}
				}

				m_pLocalInventory->m_LoadoutItems[ iTeam ][ nSwapSlot ] = LOADOUT_SLOT_USE_BASE_ITEM;
			}
		}
		else
		{
			if ( pItemAlreadyInPosition && pItemAlreadyInPosition->IsValid() )
			{
				pItemAlreadyInPosition->UpdateEquippedState( iTeam, INVALID_EQUIPPED_SLOT );
			}
			else
			{
				CEconItemView *pSwapItem = m_pLocalInventory->FindDefaultEquippedDefinitionItemBySlot( iTeam, iSlot );
				if ( pSwapItem )
				{
					m_pLocalInventory->SetDefaultEquippedDefinitionItemBySlot( iTeam, iSlot, 0 );
				}
			}
		}
	}
	else
	{
		if ( pItemAlreadyInPosition && pItemAlreadyInPosition->IsValid() )
		{
			UpdateInventoryEquippedState( m_pLocalInventory, pItemAlreadyInPosition->GetItemID(), iTeam, INVALID_EQUIPPED_SLOT );
		}
	}

	// Equip the new item
	if ( pItem )
	{
		if ( econ_debug_loadout_ui.GetBool() ) ConColorMsg( Color( 255, 0, 255, 255 ), "LOADOUT UniqueItem \"%s\" to slot: %i\n", pDef->GetDefinitionName(), iSlot );

		pItem->UpdateEquippedState( iTeam, iSlot );
		m_pLocalInventory->m_LoadoutItems[ iTeam ][ iSlot ] = pItem->GetItemID();
	}
	else
	{
		if ( bWillClearSlot )
		{
			// Putting a default in the slot where it belongs... just clear it out
			if ( econ_debug_loadout_ui.GetBool() ) ConColorMsg( Color( 255, 0, 255, 255 ), "LOADOUT Clear slot: %i\n", iSlot );

			m_pLocalInventory->SetDefaultEquippedDefinitionItemBySlot( iTeam, iSlot, 0 );
			return m_pLocalInventory->ClearLoadoutSlot( iTeam, iSlot );
		}

		if ( econ_debug_loadout_ui.GetBool() ) ConColorMsg( Color( 255, 0, 255, 255 ), "LOADOUT BaseItem \"%s\" to slot: %i\n", pDef->GetDefinitionName(), iSlot );

		m_pLocalInventory->SetDefaultEquippedDefinitionItemBySlot( iTeam, iSlot, nLowDWord );
		m_pLocalInventory->m_LoadoutItems[ iTeam ][ iSlot ] = LOADOUT_SLOT_USE_BASE_ITEM;
	}

	UpdateInventoryEquippedState( m_pLocalInventory, iItemID, iTeam, iSlot, bSwap && !bSwapClearedASlot );

	return true;
}

struct BaseItemToCheck_t
{
	item_definition_index_t nDefIndex;
	int nSlot;

	BaseItemToCheck_t( item_definition_index_t p_nDefIndex, int p_nSlot )
	{
		nDefIndex = p_nDefIndex;
		nSlot = p_nSlot;
	}
};

bool CCSInventoryManager::CleanupDuplicateBaseItems( int iClass )
{
	CUtlVector< BaseItemToCheck_t > vecBaseItemsToCheck;
	CUtlVector< item_definition_index_t > vecDefaultItems;

	for ( int nSlot = 0; nSlot < ARRAYSIZE( m_pLocalInventory->m_LoadoutItems[ iClass ] ); ++nSlot )
	{
		// Skip unique item slots
		if ( m_pLocalInventory->m_LoadoutItems[ iClass ][ nSlot ] != 0 )
			continue;

		CEconItemView *pBaseItem = m_pLocalInventory->FindDefaultEquippedDefinitionItemBySlot( iClass, nSlot );
		if ( pBaseItem && pBaseItem->IsValid() )
		{
			// Base item here
			vecBaseItemsToCheck.AddToTail( BaseItemToCheck_t( pBaseItem->GetItemDefinition()->GetDefinitionIndex(), nSlot ) );
			continue;
		}

		// This slot is using the default, check for duplicates
		CEconItemView *pDefault = GetBaseItemForTeam( iClass, nSlot );
		if ( pDefault && pDefault->IsValid() )
		{
			vecDefaultItems.AddToTail( pDefault->GetItemDefinition()->GetDefinitionIndex() );
		}
	}

	bool bFoundDuplicate = true;
	while ( bFoundDuplicate )
	{
		bFoundDuplicate = false;

		FOR_EACH_VEC( vecBaseItemsToCheck, i )
		{
			if ( vecDefaultItems.IsValidIndex( vecDefaultItems.Find( vecBaseItemsToCheck[ i ].nDefIndex ) ) )
			{
				// Found a matching default
				bFoundDuplicate = true;

				// Unequip the base
				EquipItemInLoadout( iClass, vecBaseItemsToCheck[ i ].nSlot, 0 );

				CEconItemView *pDefault = GetBaseItemForTeam( iClass, vecBaseItemsToCheck[ i ].nSlot );
				if ( pDefault && pDefault->IsValid() )
				{
					// Add the default to the list
					vecDefaultItems.AddToTail( pDefault->GetItemDefinition()->GetDefinitionIndex() );
				}

				// Remove this from the list
				vecBaseItemsToCheck.Remove( i );
				break;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Fills out pList with all inventory items that could fit into the specified loadout slot for a given class
//-----------------------------------------------------------------------------
int	CCSInventoryManager::GetAllUsableItemsForSlot( int iTeam, int iSlot, unsigned int unUsedEquipRegionMask, CUtlVector<CEconItemView*> *pList )
{
	Assert( iTeam >= 0 && iTeam < LOADOUT_COUNT );
	Assert( iSlot >= -1 && iSlot < LOADOUT_POSITION_COUNT );
	Assert( m_pLocalInventory );

	int iCount = m_pLocalInventory->GetItemCount();
	for ( int i = 0; i < iCount; i++ )
	{
		CEconItemView *pItem = m_pLocalInventory->GetItem(i);
		const CCStrike15ItemDefinition *pItemData = pItem->GetStaticData();

		int nLoadoutSlot = pItem->GetStaticData()->GetLoadoutSlot(iTeam);

		if ( nLoadoutSlot != LOADOUT_POSITION_MISC0 && !pItemData->CanBeUsedByTeam(iTeam) )
			continue;

		// Passing in iSlot of -1 finds all items usable by the class
		if ( iSlot >= 0 && !AreSlotsConsideredIdentical( nLoadoutSlot, iSlot ) )
			continue;

		// Ignore unpack'd items
		if ( IsUnacknowledged( pItem->GetInventoryPosition() ) )
			continue;

		// Skip if it conflicts with equipped
		if ( pItemData->GetEquipRegionMask() & unUsedEquipRegionMask )
			continue;

		pList->AddToTail( m_pLocalInventory->GetItem(i) );
	}

	return pList->Count();
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconItemView *CCSInventoryManager::GetItemInLoadoutForTeam( int iTeam, int iSlot, CSteamID *pID )
{
#ifdef CLIENT_DLL
	if ( !pID )
	{
		// If they didn't specify a steamID, use the local player
		if ( !steamapicontext || !steamapicontext->SteamUser() )
			return NULL;

		CSteamID localSteamID = steamapicontext->SteamUser()->GetSteamID();
		pID = &localSteamID;
	}
#endif

	if ( !pID )
		return NULL;

	CCSPlayerInventory *pInv = GetInventoryForPlayer( *pID );
	if ( !pInv )
		return GetBaseItemForTeam( iTeam, iSlot );

	return pInv->GetItemInLoadout( iTeam, iSlot );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSPlayerInventory *CCSInventoryManager::GetInventoryForPlayer( const CSteamID &playerID )
{
	for ( int i = 0; i < m_pInventories.Count(); i++ )
	{
		if ( m_pInventories[i].pInventory->GetOwner() != playerID )
			continue;

		return assert_cast<CCSPlayerInventory*>( m_pInventories[i].pInventory );
	}

	return NULL;
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSInventoryManager::GetNumItemPickedUpItems( void )
{
	Assert( m_pLocalInventory );

	int iResult = 0;
	int iCount = m_pLocalInventory->GetItemCount();
	for ( int i = 0; i < iCount; i++ )
	{
		CEconItemView *pItemView = m_pLocalInventory->GetItem(i);
		if ( pItemView->IsValid() && IsUnacknowledged( pItemView->GetInventoryPosition() ) )
		{
			// It also needs to be in our previously updated list of unacknowledged items or SF will fail at looking it up later
			if ( m_UnacknowledgedItems.Find( pItemView ) != m_UnacknowledgedItems.InvalidIndex() )
			{
				++iResult;
			}
		}
	}
	return iResult;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSInventoryManager::UpdateUnacknowledgedItems( void )
{
	CPlayerInventory *pLocalInv = GetLocalInventory();
	if ( !pLocalInv )
		return;

	CUtlVector< CEconItemView* > vecItemsAckedOnClientOnly;

	RebuildUnacknowledgedItemList( &vecItemsAckedOnClientOnly );

	// Acknowledge items on the GC that we've acked on the client but are still in the unacked position
	if ( vecItemsAckedOnClientOnly.Count() > 0 )
	{
		BeginBackpackPositionTransaction(); // Batch up the position information

		// We're not forcing the player to make room yet. Just show the pickup panel.
		for ( int i = 0; i < vecItemsAckedOnClientOnly.Count(); i++ )
		{
			// Then move it to the first empty backpack position
			SetItemBackpackPosition( vecItemsAckedOnClientOnly[i], 0, false, true );
		}

		EndBackpackPositionTransaction(); // Transmit the item update
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSInventoryManager::AcknowledgeUnacknowledgedItems( void )
{
	if ( !m_UnacknowledgedItems.Count() )
		return;

	BeginBackpackPositionTransaction(); // Batch up the position information

	// We're not forcing the player to make room yet. Just show the pickup panel.
	for ( int i = 0; i < m_UnacknowledgedItems.Count(); i++ )
	{
		SetAckedByClient( m_UnacknowledgedItems[i] );

// 		int iMethod = GetUnacknowledgedReason( m_UnacknowledgedItems[i]->GetInventoryPosition() ) - 1;
// 		if ( iMethod >= ARRAYSIZE( g_pszItemPickupMethodStringsUnloc ) || iMethod < 0 )
// 			iMethod = 0;
// 		EconUI()->Gamestats_ItemTransaction( IE_ITEM_RECEIVED, m_UnacknowledgedItems[i], g_pszItemPickupMethodStringsUnloc[iMethod] );

		// Then move it to the first empty backpack position
		SetItemBackpackPosition( m_UnacknowledgedItems[i], 0, false, true );
	}

	EndBackpackPositionTransaction(); // Transmit the item update
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSInventoryManager::AcknowledgeUnacknowledgedItem( itemid_t ui64ItemID )
{
	if ( !m_UnacknowledgedItems.Count() )
		return false;

	BeginBackpackPositionTransaction(); // Batch up the position information

	// We're not forcing the player to make room yet. Just show the pickup panel.
	FOR_EACH_VEC( m_UnacknowledgedItems, i )
	{
		if ( m_UnacknowledgedItems[i]->GetItemID() == ui64ItemID )
		{
			SetAckedByClient( m_UnacknowledgedItems[ i ] );

			SetItemBackpackPosition( m_UnacknowledgedItems[ i ], 0, false, true );

			EndBackpackPositionTransaction(); // Transmit the item update
			
			return true;	// success!
		}
	}

	return false;	// never found the item.


}




bool CCSInventoryManager::ShowItemsPickedUp( bool bForce, bool bReturnToGame, bool bNoPanel )
{
	UpdateUnacknowledgedItems();

	if ( !m_UnacknowledgedItems.Count() )
		return false;//CheckForRoomAndForceDiscard();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSInventoryManager::ShowItemsCrafted( CUtlVector<itemid_t> *vecCraftedIndices )
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSInventoryManager::CheckForRoomAndForceDiscard( void )
{
	/** Removed for partner depot **/
	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Does a mapping to a "class" index based on a class index and a preset index, so that presets can be stored in the GC.
//-----------------------------------------------------------------------------
equipped_class_t CCSInventoryManager::DoClassMapping( equipped_class_t unClass, equipped_preset_t unPreset )
{
	return ( unPreset << 8 ) | unClass;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the item data for the base item in the loadout slot for a given class
//-----------------------------------------------------------------------------
CEconItemView *CCSInventoryManager::GetBaseItemForTeam( int iTeam, int iSlot )
{
	if ( iSlot < 0 || iSlot >= LOADOUT_POSITION_COUNT )
		return NULL;
	if ( iTeam < 0 || iTeam >= LOADOUT_COUNT )
		return NULL;
	return &m_pBaseLoadoutItems[iTeam][iSlot];
}

//-----------------------------------------------------------------------------
// Purpose: Fills out the vector with the sets that are currently active on the specified player & class
//-----------------------------------------------------------------------------
void CCSInventoryManager::GetActiveSets( CUtlVector<const CEconItemSetDefinition *> *pItemSets, CSteamID steamIDForPlayer, int iTeam )
{
	pItemSets->Purge();

	CEconItemSchema *pSchema = ItemSystem()->GetItemSchema();
	if ( !pSchema )
		return;

	// Loop through all the items we have equipped, and build a list of only set items
	// Accumulate a list of our equipped set items.
	CUtlVector<CEconItemView*> equippedSetItems;
	for ( int i = 0; i < LOADOUT_POSITION_COUNT; i++ )
	{
		CEconItemView *pItem = NULL;

#ifdef GAME_DLL
		// On the server we need to look at what the player actually has equipped
		// because they might be on a tournament server that's using an item_whitelist
		CCSPlayer *pPlayer = ToCSPlayer( CBasePlayer::GetPlayerBySteamID( steamIDForPlayer ) );
		if ( pPlayer && pPlayer->Inventory() )
		{
			pItem = pPlayer->GetEquippedItemInLoadoutSlot( i );
		}
#else
		pItem = CSInventoryManager()->GetItemInLoadoutForTeam( iTeam, i, &steamIDForPlayer );
#endif		

		if ( !pItem )
			continue;

		if ( pItem->GetItemSetIndex() < 0 )
			continue; // Ignore items that don't have set bonuses.

		equippedSetItems.AddToTail( pItem );
	}

	// Find out which sets to apply.
	CUtlVector<const char*> testedSets;
	for ( int inv = 0; inv < equippedSetItems.Count(); inv++ )
	{
		CEconItemView *pItem = equippedSetItems[inv];
		if ( !pItem )
			continue;

		const CEconItemSetDefinition *pItemSet = pSchema->GetItemSetByIndex( pItem->GetItemSetIndex() );
		if ( !pItemSet )
			continue;

		if ( testedSets.HasElement( pItemSet->m_pszName ) )
			continue; // Don't try to apply set bonuses we have already tested.

		testedSets.AddToTail( pItemSet->m_pszName );

		// Count how much of this set we have equipped.
		int iSetItemsEquipped = 0;
		FOR_EACH_VEC( pItemSet->m_ItemEntries, i )
		{
			unsigned int iIndex = pItemSet->m_ItemEntries[i].m_nItemDef;

			for ( int j=0; j<equippedSetItems.Count(); j++ )
			{
				CEconItemView *pTestItem = equippedSetItems[j];
				if ( iIndex == pTestItem->GetItemIndex() )
				{
					iSetItemsEquipped++;
					break;
				}
			}
		}

		if ( iSetItemsEquipped == pItemSet->m_ItemEntries.Count() )
		{
			// The entire set is equipped. 
			pItemSets->AddToTail( pItemSet );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: We're generating a base item. We need to add the game-specific keys to the criteria so that it'll find the right base item.
//-----------------------------------------------------------------------------
void CCSInventoryManager::AddBaseItemCriteria( baseitemcriteria_t *pCriteria, CItemSelectionCriteria *pSelectionCriteria )
{
	pSelectionCriteria->BAddCondition( "used_by_classes", k_EOperator_Subkey_Contains, ItemSystem()->GetItemSchema()->GetClassUsabilityStrings()[pCriteria->iClass], true );
	pSelectionCriteria->BAddCondition( "item_slot", k_EOperator_String_EQ, ItemSystem()->GetItemSchema()->GetLoadoutStrings()[pCriteria->iSlot], true );
	pSelectionCriteria->BAddCondition( "item_sub_position", k_EOperator_String_EQ, ItemSystem()->GetItemSchema()->GetLoadoutStringsSubPositions()[pCriteria->iSlot], true );
}

int CCSInventoryManager::GetSlotForBaseOrDefaultEquipped( int iClass, item_definition_index_t iDefIndex)
{
	for ( int i = 0; i < LOADOUT_POSITION_COUNT; i++ )
	{
		CEconItemView* pItem = GetItemInLoadoutForTeam( iClass, i );
		if (	pItem && 
				pItem->IsValid() && 
				pItem->GetItemDefinition()->GetDefinitionIndex() == iDefIndex &&
				pItem->GetItemID() == 0 )
		{
//			Msg( " BASE OR DEFAULT ITEMS EQUIPPED: %s, %d\n", pItem->GetItemDefinition()->GetItemBaseName(), i);
			return i;
		}
	}

	return INVALID_EQUIPPED_SLOT;
}

#if defined ( CLIENT_DLL )
void CCSInventoryManager::RebuildUnacknowledgedItemList( CUtlVector< CEconItemView* > * vecItemsAckedOnClientOnly /*null*/ )
{
	CPlayerInventory *pLocalInv = GetLocalInventory();
	if ( !pLocalInv )
		return;

	m_UnacknowledgedItems.Purge();

	// Go through the root inventory and find any items that are in the "found" position
	int iCount = pLocalInv->GetItemCount();
	for ( int i = 0; i < iCount; i++ )
	{
		CEconItemView *pTmp = pLocalInv->GetItem( i );
		if ( !pTmp )
			continue;

		if ( pTmp->GetStaticData()->IsHidden() )
			continue;

		uint32 iPosition = pTmp->GetInventoryPosition();
		if ( IsUnacknowledged( iPosition ) == false )
			continue;
		if ( GetBackpackPositionFromBackend( iPosition ) != 0 )
			continue;

		if ( vecItemsAckedOnClientOnly )
		{
			// Now make sure we haven't got a clientside saved ack for this item.
			// This makes sure we don't show multiple pickups for items that we've found,
			// but haven't been able to move out of unack'd position due to the GC being unavailable.
			if ( HasBeenAckedByClient( pTmp ) )
			{
				vecItemsAckedOnClientOnly->AddToTail( pTmp );
				continue;
			}
		}
		m_UnacknowledgedItems.AddToTail( pTmp );
	}
}

CUtlVector<CEconItemView*> & CCSInventoryManager::GetUnacknowledgedItems( bool bRefreshList )
{
	if ( bRefreshList )
	{
		RebuildUnacknowledgedItemList();
	}
	return m_UnacknowledgedItems;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSPlayerInventory::CCSPlayerInventory()
{
	memset( m_LoadoutItems, LOADOUT_SLOT_USE_BASE_ITEM, sizeof(m_LoadoutItems) );

	m_nActiveQuestID = 0;
}


static void Helper_NotifyMyPersonaInventoryUpdated( const CSteamID &steamIDOwner )
{
	/** Removed for partner depot **/
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventory::SOCreated( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )
{
	BaseClass::SOCreated( owner, pObject, eEvent );

	CSteamID steamIDOwner( owner.ID() );
	Helper_NotifyMyPersonaInventoryUpdated( steamIDOwner );

	if ( pObject->GetTypeID() == CEconItem::k_nTypeID )
	{
		CEconItem *pEconItem = ( CEconItem * ) pObject;

		// Update current quest if this is the op coin!
		switch ( pEconItem->GetDefinitionIndex() )
		{
		case MEDAL_SEASON_COIN_BRONZE:
		case MEDAL_SEASON_COIN_SILVER:
		case MEDAL_SEASON_COIN_GOLD:
			RefreshActiveQuestID();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventory::SODestroyed( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )
{
	BaseClass::SODestroyed( owner, pObject, eEvent );

	CSteamID steamIDOwner( owner.ID() );
	Helper_NotifyMyPersonaInventoryUpdated( steamIDOwner );

	if ( pObject->GetTypeID() == CEconItem::k_nTypeID )
	{
		CEconItem *pEconItem = ( CEconItem * ) pObject;

		// Update current quest if this is the op coin!
		switch ( pEconItem->GetDefinitionIndex() )
		{
		case MEDAL_SEASON_COIN_BRONZE:
		case MEDAL_SEASON_COIN_SILVER:
		case MEDAL_SEASON_COIN_GOLD:
			RefreshActiveQuestID();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventory::SOUpdated( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )
{
	BaseClass::SOUpdated( owner, pObject, eEvent );

	CSteamID steamIDOwner( owner.ID() );
	Helper_NotifyMyPersonaInventoryUpdated( steamIDOwner );

	if ( pObject->GetTypeID() == CEconItem::k_nTypeID )
	{
		CEconItem *pEconItem = ( CEconItem * ) pObject;

#ifdef CLIENT_DLL
		// Clear out any predicted backpack slots when items move into them
		int iBackpackPos = CSInventoryManager()->GetBackpackPositionFromBackend( pEconItem->GetInventoryToken() );
		CSInventoryManager()->PredictedBackpackPosFilled( iBackpackPos );

		if ( eEvent == eSOCacheEvent_Incremental )
		{
			pEconItem->SetSOUpdateFrame( gpGlobals->framecount );
		}
#endif // CLIENT_DLL

		// Update current quest if this is the op coin!
		switch ( pEconItem->GetDefinitionIndex() )
		{
		case MEDAL_SEASON_COIN_BRONZE:
		case MEDAL_SEASON_COIN_SILVER:
		case MEDAL_SEASON_COIN_GOLD:
			RefreshActiveQuestID();
		}
	}
}



float CCSPlayerInventory::FindInventoryItemWithMaxAttributeValue( char const *szItemType, char const *szAttrClass, CEconItemView **ppItemFound /* = NULL */ )
{
	if ( !szAttrClass || szAttrClass[ 0 ] == '\0' )
		return -1.0f;

	CSchemaAttributeDefHandle pAttr( szAttrClass );
	if ( !pAttr )
		return -1.0f;

	CEconItemView *pItemFound = NULL;
	float flReturnValue = -1.0f; // Nothing found yet

	CSchemaItemDefHandle hRequiredItem( szItemType );
	if ( hRequiredItem )
	{
		for ( int i = 0 ; i < GetItemCount() ; ++i )
		{
			CEconItemView *pItem = GetItem( i );
			Assert( pItem );
			if ( pItem->GetItemDefinition() == hRequiredItem )
			{
				if ( !pItemFound )
				{
					pItemFound = pItem;
					flReturnValue = 0.0f; // We at least found something
				}

				if ( pAttr->IsStoredAsInteger() )
				{
					uint32 unValue;
					if ( pItem->FindAttribute( pAttr, &unValue ) )
					{
						if ( flReturnValue < float( unValue ) )
						{
							pItemFound = pItem;
							flReturnValue = float( unValue );
						}
					}
				}
				else
				{
					float flFoundValue;
					if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pItem, pAttr, &flFoundValue ) )
					{
						if ( flReturnValue < flFoundValue )
						{
							pItemFound = pItem;
							flReturnValue = flFoundValue;
						}
					}
				}
			}
		}
	}

	if ( ppItemFound )
	{
		*ppItemFound = pItemFound;
	}
	
	return flReturnValue;
}

// DEPRECATED. Use version that takes CSchemaAttributeDefHandle instead. For Perf.
void CCSPlayerInventory::FindInventoryItemsWithAttribute( char const *szAttrClass, CUtlVector< CEconItemView* > &foundItems, bool bMatchValue /*= false*/, uint32 unValue /*= 0*/ )
{
	Assert(0); // DEPRECATED FUNCTION.

	if ( !szAttrClass || szAttrClass[ 0 ] == '\0' )
		return;

	CSchemaAttributeDefHandle pAttr( szAttrClass );
	if ( !pAttr )
		return;

	FindInventoryItemsWithAttribute( pAttr, foundItems, NULL, bMatchValue, unValue );


}

void CCSPlayerInventory::FindInventoryItemsWithAttribute( CSchemaAttributeDefHandle pAttr, CUtlVector< CEconItemView* > &foundItems, CUtlVector< CEconItemView* > *pVecSearchSet /* = NULL */, bool bMatchValue /*= false*/, uint32 unValue /*= 0*/ )
{

	Assert( pAttr);
	if ( !pAttr )
		return;
	
	CUtlVector< CEconItemView* > itemSet;

	// if no search set was passed in, create a set of all inventory items
	if ( !pVecSearchSet )
	{
		for ( int i = 0; i < GetItemCount(); ++i )
		{
			CEconItemView *pItem = GetItem( i );
			Assert( pItem );

			itemSet.AddToTail( pItem );
		}

		pVecSearchSet = &itemSet;
	}


	FOR_EACH_VEC( (*pVecSearchSet), i )
	{
		if ( pAttr->IsStoredAsInteger() )
		{
			uint32 unItemAttrValue;
			if ( (*pVecSearchSet)[ i ]->FindAttribute( pAttr, &unItemAttrValue ) )
			{
				if ( !bMatchValue || ( unItemAttrValue == unValue ) )
				{
					foundItems.AddToTail( (*pVecSearchSet)[ i ] );
				}
			}
		}
		else
		{
			float flFoundValue;
			if ( FindAttribute_UnsafeBitwiseCast<attrib_value_t>( (*pVecSearchSet)[ i ], pAttr, &flFoundValue ) )
			{
				if ( !bMatchValue || ( *reinterpret_cast< uint32 const* >( &flFoundValue ) == unValue ) )
				{
					foundItems.AddToTail( (*pVecSearchSet)[ i ] );
				}
			}
		}
	}

}



itemid_t CCSPlayerInventory::GetActiveSeasonItemId( bool bCoin /* false is the Pass */ )
{
	if ( !MEDAL_SEASON_ACCESS_ENABLED )
		return 0;


	// Season 2 access
	CUtlVector< CEconItemView* > foundItems;

	static CSchemaAttributeDefHandle pAttr( "season access" );
	Assert( pAttr );
	FindInventoryItemsWithAttribute( pAttr, foundItems, NULL, true, MEDAL_SEASON_ACCESS_VALUE );

	FOR_EACH_VEC( foundItems, i )
	{
		CEconItemView *pItem = foundItems[ i ];
		bool bAccessItemIsCoin = !pItem->GetItemDefinition()->IsToolAndNotACrate();
		if ( bCoin == bAccessItemIsCoin )
		{
			return pItem->GetItemID();
		}
	}

	return 0;
}


uint32 CCSPlayerInventory::GetActiveQuestID( void ) const
{
	return m_nActiveQuestID;
}

void CCSPlayerInventory::RefreshActiveQuestID( void )
{
	static CSchemaAttributeDefHandle pAttr( "season access" );
	CUtlVector< CEconItemView* > foundItems;
	FindInventoryItemsWithAttribute( pAttr, foundItems, NULL, true, MEDAL_SEASON_ACCESS_VALUE );

	Assert( foundItems.Count() <= 1 );
	if ( foundItems.Count() > 0 )
	{
		uint32 unQuestID = 0;
		static CSchemaAttributeDefHandle pAttr_QuestID( "quest id" );
		foundItems[ 0 ]->FindAttribute( pAttr_QuestID, &unQuestID );
		m_nActiveQuestID = unQuestID;
	}
	else
	{
		m_nActiveQuestID = 0;
	}
}

bool CCSPlayerInventory::IsEquipped( CEconItemView * pEconItemView, int nTeam )
{
	if ( !pEconItemView || !pEconItemView->IsValid() )
		return false;

	bool bEquipped = false;

	// if it's a base item
	if ( !pEconItemView->GetItemID() )
	{
		const CCStrike15ItemDefinition *pItemDef = pEconItemView->GetItemDefinition();
		if ( !pItemDef )
			return false;

		int nSlotOfTestItem = pItemDef->GetDefaultLoadoutSlot();

		// Check to see if the defindex of the item we got matches the defindex of the 
		if ( nSlotOfTestItem > -1 )
		{
			CEconItemView *pItemInSlotOfTestItem = GetItemInLoadout( nTeam, nSlotOfTestItem );

			if ( pItemInSlotOfTestItem && pItemInSlotOfTestItem->IsValid() )
			{
				// if the item that's in the test item's slot is a base item and has the same defindex then it's a match.
				if ( ( pItemInSlotOfTestItem->GetItemID() == 0 ) &&
					( pItemInSlotOfTestItem->GetItemIndex() == pItemDef->GetDefinitionIndex() ) )
				{
					bEquipped = true;
				}
			}
		}
	}
	else // otherwise if it's a real inventory item then the check is simple
	{
		bEquipped = pEconItemView->IsEquippedForClass( nTeam );
	}

	return bEquipped;

}

// For any team
bool CCSPlayerInventory::IsEquipped( CEconItemView *pEconItemView )
{
	for ( int j = 0; j < TEAM_MAXCOUNT; j++ )
	{
		if ( IsEquipped( pEconItemView, j ) )
		{
			return true;
		}
	}

	return false;
}



void CCSPlayerInventory::OnHasNewItems()
{
	BaseClass::OnHasNewItems();
}

#ifdef _DEBUG
#ifdef CLIENT_DLL
//CON_COMMAND( cl_newitem_test, "Tests the new item ui notification." )
//{
//	if ( steamapicontext == NULL || steamapicontext->SteamUser() == NULL )
//		return;
//
//	CEconNotification_HasNewItems *pNotification = new CEconNotification_HasNewItems();
//	pNotification->SetText( "TF_HasNewItems" );
//	pNotification->SetLifetime( engine->IsInGame() ? 7.0f : 60.0f );
//	NotificationQueue_Add( pNotification );
//}
#endif
#endif // _DEBUG

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventory::ValidateInventoryPositions( void )
{
	BaseClass::ValidateInventoryPositions();

#ifdef CLIENT_DLL
	bool bHasNewItems = false;
	// First, check for duplicate positions
	const CUtlVector<CEconItemView*> &vecItems = m_Items.GetItemVector();
	FOR_EACH_VEC_BACK( vecItems, i )
	{
		uint32 iPosition = vecItems[ i ]->GetInventoryPosition();

		// Waiting to be acknowledged?
		if ( IsUnacknowledged(iPosition) )
		{
			bHasNewItems = true;
			continue;
		}

	}
	if ( bHasNewItems )
	{
		OnHasNewItems();
	}
#endif
}

void CCSPlayerInventory::SOCacheSubscribed( GCSDK::SOID_t owner, GCSDK::ESOCacheEvent eEvent )
{
	BaseClass::SOCacheSubscribed( owner, eEvent );
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventory::ConvertOldFormatInventoryToNew( void )
{
	uint32 iBackpackPos = 1;

	// Loop through all items in the inventory. Move them all to the backpack, and in order.
	int iCount = m_Items.GetItemVector().Count();
	for ( int i = 0; i < iCount; i++ )
	{
		uint32 iPosition = m_Items.GetItemVector()[ i ]->GetInventoryPosition();

		// Waiting to be acknowledged?
		if ( IsUnacknowledged(iPosition) )
			continue;

		CSInventoryManager()->SetItemBackpackPosition( m_Items.GetItemVector()[ i ], iBackpackPos, true );
		iBackpackPos++;
	}
}

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventory::DumpInventoryToConsole( bool bRoot )
{
	if ( bRoot )
	{
		Msg("========================================\n");
#ifdef CLIENT_DLL
		Msg("(CLIENT) Inventory:\n");
#else
		Msg("(SERVER) Inventory for account (%d):\n", CSteamID( m_OwnerID.ID() ).GetAccountID() );
#endif
		Msg("  Version: %llu:\n", m_pSOCache ? m_pSOCache->GetVersion() : -1 );
	}

	int iCount = m_Items.GetItemVector().Count();
	Msg("   Num items: %d\n", iCount );
	for ( int i = 0; i < iCount; i++ )
	{
		Msg("      %s (ID %llu) at backpack slot %d\n", 
			m_Items.GetItemVector()[ i ]->GetStaticData()->GetDefinitionName(),
			m_Items.GetItemVector()[ i ]->GetItemID(),
			CSInventoryManager()->GetBackpackPositionFromBackend( m_Items.GetItemVector()[ i ]->GetInventoryPosition() ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the item in the specified loadout slot for a given team
//-----------------------------------------------------------------------------
CEconItemView *CCSPlayerInventory::GetItemInLoadout( int iTeam, int iSlot ) const
{
	if ( iSlot < 0 || iSlot >= LOADOUT_POSITION_COUNT )
		return NULL;
	if ( iTeam < 0 || iTeam >= LOADOUT_COUNT )
		return NULL;

	// If we don't have an item in the loadout at that slot, we return the base item
	if ( m_LoadoutItems[iTeam][iSlot] != LOADOUT_SLOT_USE_BASE_ITEM )
	{
		CEconItemView *pItem = GetInventoryItemByItemID( m_LoadoutItems[iTeam][iSlot] );

		// To protect against users lying to the backend about the position of their items,
		// we need to validate their position on the server when we retrieve them.
		if ( pItem && AreSlotsConsideredIdentical( pItem->GetStaticData()->GetLoadoutSlot( iTeam ), iSlot ) )
			return pItem;
	}

	CEconItemView *pDefaultItem = CSInventoryManager()->GetBaseItemForTeam( iTeam, iSlot );
	CEconItemView *pItem = FindDefaultEquippedDefinitionItemBySlot( iTeam, iSlot );
	if ( pItem && pItem->IsValid() )
	{
		if ( pItem->GetItemDefinition()->GetDefinitionIndex() == pDefaultItem->GetItemDefinition()->GetDefinitionIndex() )
		{
			// Just use the default if it's the same definition index
			return pDefaultItem;
		}

		// Return the default that they've specifically equppied in that slot
		return pItem;
	}

	// Fall back to the default
	return pDefaultItem;
}

CEconItemView* CCSPlayerInventory::GetItemInLoadoutFilteredByProhibition( int iTeam, int iSlot ) const
{
	CEconItemView *pItem = GetItemInLoadout( iTeam, iSlot );

	if ( !pItem || !CSGameRules() )
		return pItem;

	/** Removed for partner depot **/

	return pItem;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEconGameAccountClient *GetSOCacheGameAccountClient( CGCClientSharedObjectCache *pSOCache )
{
	if ( !pSOCache )
		return NULL;

	CSharedObjectTypeCache *pTypeCache = pSOCache->FindTypeCache( CEconGameAccountClient::k_nTypeID );
	if ( !pTypeCache || pTypeCache->GetCount() != 1 )
		return NULL;

	CEconGameAccountClient *pGameAccountClient = (CEconGameAccountClient*)pTypeCache->GetObject( 0 );
	return pGameAccountClient;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCSPlayerInventory::GetPreviewItemDef( void ) const
{
#if ECON_JOB_RENTAL_PREVIEW_SUPPORTED
	CEconGameAccountClient *pGameAccountClient = GetSOCacheGameAccountClient( m_pSOCache );
	if ( !pGameAccountClient  )
		return 0;

	return pGameAccountClient->Obj().preview_item_def();
#else
	return 0;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSPlayerInventory::CanPurchaseItems( int iItemCount ) const
{
#if 1
	return BaseClass::CanPurchaseItems( iItemCount );
#else
	// If we're not a free trial account, we fall back to our default logic of "do
	// we have enough empty slots?".
	CEconGameAccountClient *pGameAccountClient = GetSOCacheGameAccountClient( m_pSOCache );
	if ( !pGameAccountClient || !pGameAccountClient->Obj().trial_account() )
		return BaseClass::CanPurchaseItems( iItemCount );

	// We're a free trial account, so when we purchase these items, our inventory
	// will actually expand. We check to make sure that we have room for these
	// items against what will be our new maximum backpack size, not our current
	// backpack limit.
	int iNewItemCount			   = GetItemCount() + iItemCount,
		iAfterPurchaseMaxItemCount = DEFAULT_NUM_BACKPACK_SLOTS
								   + (pGameAccountClient ? pGameAccountClient->Obj().additional_backpack_slots() : 0);

	return iNewItemCount <= iAfterPurchaseMaxItemCount;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CCSPlayerInventory::GetMaxItemCount( void ) const
{
	int iMaxItems = DEFAULT_NUM_BACKPACK_SLOTS;

	CEconGameAccountClient *pGameAccountClient = GetSOCacheGameAccountClient( m_pSOCache );
	if ( pGameAccountClient )
	{
#if 0
		if ( pGameAccountClient->Obj().trial_account() )
		{
			// Currently it thinks everyone is a trial account, so just give everyone the full space!
			//iMaxItems = DEFAULT_NUM_BACKPACK_SLOTS_FREE_TRIAL_ACCOUNT;
		}
#endif
		iMaxItems += pGameAccountClient->Obj().additional_backpack_slots();
	}
	return Min( iMaxItems, MAX_NUM_BACKPACK_SLOTS );
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Removes any item in a loadout slot. If the slot has a base item,
//			the player essentially returns to using that item.
//-----------------------------------------------------------------------------
bool CCSPlayerInventory::ClearLoadoutSlot( int iTeam, int iSlot )
{
	/** Removed for partner depot **/
	return true;
}
#endif // CLIENT_DLL


#ifdef CLIENT_DLL
//
// Inventory image provider for Scaleform
//
IScaleformInventoryImageProvider *g_pIScaleformInventoryImageProvider = NULL;
class CScaleformInventoryImageProviderImpl : public IScaleformInventoryImageProvider
{
public:
	CScaleformInventoryImageProviderImpl()
	{
		m_mapItems2Owners.SetLessFunc( DefLessFunc( itemid_t ) );
		
		Assert( !g_pIScaleformInventoryImageProvider );
		g_pIScaleformInventoryImageProvider = this;
	}
	~CScaleformInventoryImageProviderImpl()
	{
		Assert( g_pIScaleformInventoryImageProvider == this );
		g_pIScaleformInventoryImageProvider = NULL;
	}

public:
	// Scaleform low-level image needs rgba bits of the inventory image (if it's ready)
	virtual bool GetInventoryImageInfo( uint64 uiItemId, ImageInfo_t *pImageInfo )
	{
		CEconItemView *pItemView = FindOrCreateEconItemViewForItemID( uiItemId );
		if ( !pItemView )
			return false;

		if ( pImageInfo )
		{
			pImageInfo->m_pvEconItemView = reinterpret_cast< void *>( pItemView );
			pImageInfo->m_bufImageDataRGBA = pItemView->GetInventoryImageRgba( ECON_ITEM_GENERATED_ICON_WIDTH, ECON_ITEM_GENERATED_ICON_HEIGHT, InventoryImageReadyCallback );
			pImageInfo->m_pDefaultIconName = pItemView->GetStaticData() ? pItemView->GetStaticData()->GetIconDefaultImage() : NULL;
			pImageInfo->m_nWidth = ECON_ITEM_GENERATED_ICON_WIDTH;
			pImageInfo->m_nHeight = ECON_ITEM_GENERATED_ICON_HEIGHT;
		}

		return true;
	}

	static void InventoryImageReadyCallback( const CEconItemView *pItemView, CUtlBuffer &rawImageRgba, int nWidth, int nHeight, uint64 nItemID )
	{
		g_pScaleformUI->InventoryImageUpdate( nItemID, g_pIScaleformInventoryImageProvider );
	}

	CEconItemView * FindOrCreateEconItemViewForItemID( uint64 uiItemId )
	{
		CEconItemView *pItemView = NULL;

		// Reference def/paint items ID structure:
		if ( CombinedItemIdIsDefIndexAndPaint( uiItemId ) )
		{
			uint16 iPaintIndex = CombinedItemIdGetPaint( uiItemId );
			uint16 iDefIndex = CombinedItemIdGetDefIndex( uiItemId );
			uint8 ub1 = CombinedItemIdGetUB1( uiItemId );

			pItemView = InventoryManager()->FindOrCreateReferenceEconItem( iDefIndex, iPaintIndex, ub1 );
			Assert( pItemView && pItemView->IsValid() );
		}
		// Otherwise it's a real owned item
		else
		{
			CUtlMap< itemid_t, XUID >::IndexType_t idx = m_mapItems2Owners.Find( uiItemId );
			if ( idx == m_mapItems2Owners.InvalidIndex() )
				return NULL;

			XUID xuidOwner = m_mapItems2Owners.Element( idx );
			if ( !xuidOwner )
				return NULL;

			CCSPlayerInventory *pInv = CSInventoryManager()->GetInventoryForPlayer( CSteamID( xuidOwner ) );
			if ( !pInv )
				return NULL;

			pItemView = pInv->GetInventoryItemByItemID( uiItemId );
		}

		return pItemView;
	}

public:
	CUtlMap< itemid_t, XUID > m_mapItems2Owners;
}
g_ScaleformInventoryImageProviderImpl;

CEconItemView * CEconItemView::FindOrCreateEconItemViewForItemID( uint64 uiItemId )
{
	return g_ScaleformInventoryImageProviderImpl.FindOrCreateEconItemViewForItemID( uiItemId );
}

#else

CEconItemView::UtlMapLookupByID_t CEconItemView::s_mapLookupByID;

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventory::ItemHasBeenUpdated( CEconItemView *pItem, bool bUpdateAckFile, bool bWriteAckFile, EInventoryItemEvent eEventType )
{
	Assert( pItem );

	BaseClass::ItemHasBeenUpdated( pItem, bUpdateAckFile, bWriteAckFile, eEventType );

	CEconItem *pEconItem = pItem->GetSOCData();

#ifdef CLIENT_DLL
	bool bRequestedGenerateStickerMaterials = false;
#endif

	for ( equipped_class_t iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
	{
		equipped_slot_t unSlot = pEconItem ? pEconItem->GetEquippedPositionForClass( iTeam ) : INVALID_EQUIPPED_SLOT;

		// unequipped items or items reporting an invalid slot
		for ( int i = 0; i < LOADOUT_POSITION_COUNT; i++ )
		{
			if ( unSlot != i && m_LoadoutItems[iTeam][i] == pItem->GetItemID() )
			{
				m_LoadoutItems[iTeam][i] = LOADOUT_SLOT_USE_BASE_ITEM;
			}
		}

		if ( unSlot == INVALID_EQUIPPED_SLOT )
			continue;

		Assert( GetInventoryItemByItemID( pItem->GetItemID() ) );

		itemid_t& pLoadoutItemID = m_LoadoutItems[iTeam][unSlot];

#ifdef CLIENT_DLL
		/*
		Undoing this -- This causes a bug when trying to equip both slots for a gun. The GC unequips 
		the previous guns when handling the normal equip messages so this shouldn't be needed anyway.
		if ( pLoadoutItemID != pItem->GetItemID() ) 
		{
			// We've got another item that thinks it's in this loadout slot. Move it out.
			CEconItemView *pItemAlreadyInPosition = GetItemInLoadout( iTeam, unSlot );
			if ( pItemAlreadyInPosition )
			{
				// Unequip it.
				pItemAlreadyInPosition->UpdateEquippedState( iTeam, INVALID_EQUIPPED_SLOT );
				InventoryManager()->UpdateInventoryEquippedState( this, pItemAlreadyInPosition->GetItemID(), iTeam, INVALID_EQUIPPED_SLOT );
			}
		}
		*/

		if ( InventoryManager()->GetLocalInventory() == this )
		{
			// Any weapons in our loadout should update their materials now!
			// That way there's no hitch when we first buy the item in game!
			pItem->UpdateGeneratedMaterial();
			pItem->GenerateStickerMaterials();
			bRequestedGenerateStickerMaterials = true;
		}
#endif

		// This may be an invalid slot for the item, but CTFPlayerInventory::InventoryReceived()
		// will have detected that and sent off a request already to move it. The response
		// to that will clear this loadout slot.
		pLoadoutItemID = pItem->GetItemID();
	}

#ifdef CLIENT_DLL
	if ( pItem->ItemHasAnyStickersApplied() && !bRequestedGenerateStickerMaterials )
	{
		pItem->GenerateStickerMaterials();
		bRequestedGenerateStickerMaterials = true;
	}
#endif

#ifdef CLIENT_DLL
	// Assert that this item belongs to this inventory and store the mapping to owner
	CSteamID owner( this->GetOwner().ID() );
	Assert( pItem->GetAccountID() == owner.GetAccountID() );
	g_ScaleformInventoryImageProviderImpl.m_mapItems2Owners.InsertOrReplace( pItem->GetItemID(), owner.ConvertToUint64() );

	// Make sure that the item inventory image is updated no that the item can be found in the mapping
	g_pScaleformUI->InventoryImageUpdate( pItem->GetItemID(), g_pIScaleformInventoryImageProvider );



#endif
}

void CCSPlayerInventory::DefaultEquippedDefinitionHasBeenUpdated( CEconDefaultEquippedDefinitionInstanceClient *pDefaultEquippedDefinition )
{
	BaseClass::DefaultEquippedDefinitionHasBeenUpdated( pDefaultEquippedDefinition );

	if ( pDefaultEquippedDefinition->Obj().item_definition() == LOADOUT_SLOT_USE_BASE_ITEM )
		return;

	int unSlot = pDefaultEquippedDefinition->Obj().slot_id();
	if ( unSlot == INVALID_EQUIPPED_SLOT )
		return;

	int nTeam = pDefaultEquippedDefinition->Obj().class_id();
	if ( nTeam < 0 || nTeam >= LOADOUT_COUNT )
		return;

	m_LoadoutItems[ nTeam ][ unSlot ] = LOADOUT_SLOT_USE_BASE_ITEM;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventory::ItemIsBeingRemoved( CEconItemView *pItem )
{
	for ( int iTeam = 0; iTeam < LOADOUT_COUNT; iTeam++ )
	{
		if ( pItem->IsEquippedForClass( iTeam ) )
		{
			for ( int iSlot = 0; iSlot < LOADOUT_POSITION_COUNT; iSlot++ )
			{
				if ( m_LoadoutItems[iTeam][iSlot] == pItem->GetItemID() )
				{
					m_LoadoutItems[iTeam][iSlot] = LOADOUT_SLOT_USE_BASE_ITEM;
				}
			}
		}
	}
}

bool CCSPlayerInventory::IsMissionRefuseAllowed( void ) const
{
#if 1
	return false;
#else
	CEconGameAccountClient *pGameAccountClient = GetSOCacheGameAccountClient( m_pSOCache );
	if ( !pGameAccountClient  )
		return false;

	return ( 1 == pGameAccountClient->Obj().mission_refuse_allowed() );
#endif
}
