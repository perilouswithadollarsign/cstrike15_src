//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "cs_item_inventory.h"
#include "item_creation.h"
#include "vgui/ILocalize.h"
#include "tier3/tier3.h"
#ifdef CLIENT_DLL
// #include "item_pickup_panel.h"
#else
#include "cs_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


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
/*
	m_LocalBackpack.SetBag( BAG_BACKPACK );

	for ( int i = CS_FIRST_NORMAL_CLASS; i < CS_LAST_NORMAL_CLASS; i++ )
	{
		m_LoadoutInventories[i].SetBag( GetLoadoutBagForClass(i) );
	}
*/

#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSInventoryManager::PostInit( void )
{
	BaseClass::PostInit();
	GenerateBaseItems();
}

//-----------------------------------------------------------------------------
// Purpose: Generate & store the base item details for each class & loadout slot
//-----------------------------------------------------------------------------
void CCSInventoryManager::GenerateBaseItems( void )
{
/*
	for ( int slot = 0; slot < LOADOUT_POSITION_COUNT; slot++ )
	{
		baseitemcriteria_t pCriteria;
		pCriteria.iClass = 0;	// FIXME[pmf]: set up CS class identifiers?
		pCriteria.iSlot = slot;
		int iItemDef = ItemSystem()->GenerateBaseItem( &pCriteria );
		if ( iItemDef != INVALID_ITEM_INDEX )
		{
			/ *
			IHasAttributes *pItemInterface = dynamic_cast<IHasAttributes *>(pItem);
			if ( pItemInterface )
			{
				CScriptCreatedItem *pScriptItem = pItemInterface->GetAttributeManager()->GetItem();
				Msg("============================================\n");
				Msg("Inventory: Generating base item for %s, slot %s...\n", g_aPlayerClassNames_NonLocalized[iClass], g_szLoadoutStrings[slot] );
				Msg("Generated: %s \"%s\"\n", g_szQualityStrings[pScriptItem->GetItemQuality()], pEnt->GetClassname() );
				char tempstr[1024];
				g_pVGuiLocalize->ConvertUnicodeToANSI( pScriptItem->GetAttributeDescription(), tempstr, sizeof(tempstr) );
				Msg("%s", tempstr );
				Msg("\n============================================\n");
			}
			* /

			m_pBaseLoadoutItems[slot].Init( iItemDef, AE_USE_SCRIPT_VALUE, AE_USE_SCRIPT_VALUE, false );
		}
		else
		{
			m_pBaseLoadoutItems[slot].Invalidate();
		}
	}
*/
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSInventoryManager::UpdateLocalInventory( void )
{
	BaseClass::UpdateLocalInventory();

	if ( steamapicontext->SteamUser() )
	{
		CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();

/*
		CSInventoryManager()->SteamRequestInventory( &m_LocalBackpack, steamID );

		// Request our loadouts.
		for ( int i = CS_FIRST_NORMAL_CLASS; i < CS_LAST_NORMAL_CLASS; i++ )
		{
			CSInventoryManager()->SteamRequestInventory( &m_LoadoutInventories[i], steamID );
		}
*/
	}	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
/*
bool CCSInventoryManager::EquipItemInLoadout( int iClass, int iSlot, globalindex_t iGlobalIndex )
{
	CSteamID localSteamID = steamapicontext->SteamUser()->GetSteamID();
	inventory_bags_t iLoadoutBag = GetLoadoutBagForClass( iClass );
	CCSPlayerInventory *pInv = GetBagForPlayer( localSteamID, iLoadoutBag );
	if ( pInv )
	{
		uint32 iPosition = GetBackendPositionFor(iLoadoutBag,iSlot);
		if ( !iGlobalIndex )
		{
			CCSPlayerInventoryLoadout *pLoadoutInv = assert_cast<CCSPlayerInventoryLoadout*>( pInv );
			return pLoadoutInv->ClearLoadoutSlot( iPosition );
		}

		return pInv->MoveItemToPosition( iGlobalIndex, iPosition );
	}

	return false;
}
*/

//-----------------------------------------------------------------------------
// Purpose: Fills out pList with all inventory items that could fit into the specified loadout slot for a given class
//-----------------------------------------------------------------------------
int	CCSInventoryManager::GetAllUsableItemsForSlot( int iClass, int iSlot, CUtlVector<CScriptCreatedItem*> *pList )
{
// 	Assert( iClass >= CS_FIRST_NORMAL_CLASS && iClass < CS_CLASS_COUNT );
	Assert( iSlot >= -1 && iSlot < LOADOUT_POSITION_COUNT );

	int iCount = m_LocalInventory.GetItemCount();
	for ( int i = 0; i < iCount; i++ )
	{
		CScriptCreatedItem *pItem = m_LocalInventory.GetItem(i);
		CPersistentItemDefinition *pItemData = pItem->GetStaticData();

		if ( !pItemData->CanBeUsedByClass(iClass) )
			continue;

		// Passing in iSlot of -1 finds all items usable by the class
		if ( iSlot >= 0 && pItem->GetLoadoutSlot(iClass) != iSlot )
			continue;

		pList->AddToTail( m_LocalInventory.GetItem(i) );
	}

	return pList->Count();
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
/*
CScriptCreatedItem *CCSInventoryManager::GetItemInLoadoutForClass( int iClass, int iSlot, CSteamID *pID )
{
#ifdef CLIENT_DLL
	if ( !steamapicontext || !steamapicontext->SteamUser() )
		return NULL;

	CSteamID localSteamID = steamapicontext->SteamUser()->GetSteamID();
	pID = &localSteamID;
#endif

	inventory_bags_t iLoadoutBag = GetLoadoutBagForClass( iClass );
	CCSPlayerInventory *pInv = GetBagForPlayer( *pID, iLoadoutBag );
	if ( !pInv )
		return NULL;

	CCSPlayerInventoryLoadout *pLoadoutInv = assert_cast<CCSPlayerInventoryLoadout*>( pInv );
	return pLoadoutInv->GetItemInLoadout( iSlot );
}
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSPlayerInventory *CCSInventoryManager::GetBagForPlayer( CSteamID &playerID, inventory_bags_t iBag )
{
	for ( int i = 0; i < m_pInventories.Count(); i++ )
	{
		if ( m_pInventories[i].pInventory->GetOwner() != playerID )
			continue;

		CCSPlayerInventory *pCSInv = assert_cast<CCSPlayerInventory*>( m_pInventories[i].pInventory );
		if ( pCSInv->GetBag() == iBag )
			return pCSInv;
	}

	return NULL;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSInventoryManager::ShowItemsPickedUp( void )
{
	// Go through the root inventory and find any items that are in the "found" position
	CUtlVector<CScriptCreatedItem*> aItemsFound;
	int iCount = m_LocalInventory.GetItemCount();
	for ( int i = 0; i < iCount; i++ )
	{
		if ( m_LocalInventory.GetItem(i)->GetInventoryPosition() == BACKEND_POSITION_FOR_NEW_ITEM )
		{
			aItemsFound.AddToTail( m_LocalInventory.GetItem(i) );

			m_LocalInventory.MoveItemToBackpack( m_LocalInventory.GetItem(i)->GetGlobalIndex() );
		}
	}

	if ( !aItemsFound.Count() )
		return CheckForRoomAndForceDiscard();

	// We're not forcing the player to make room yet. Just show the pickup panel.
/*
	CItemPickupPanel *pItemPanel = OpenItemPickupPanel();
	for ( int i = 0; i < aItemsFound.Count(); i++ )
	{
		pItemPanel->AddItem( aItemsFound[i] );
	}
*/
	
	aItemsFound.Purge();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSInventoryManager::CheckForRoomAndForceDiscard( void )
{
	// Do we have room for the new item?
	int iCount = m_LocalInventory.GetItemCount();
	if ( iCount <= MAX_BACKPACK_SLOTS )
		return false;

	CScriptCreatedItem *pItem = m_LocalInventory.GetItem(MAX_BACKPACK_SLOTS);
	if ( !pItem )
		return false;

	// We're forcing the player to make room for items he's found. Bring up that panel with the first item over the limit.
/*
	CItemDiscardPanel *pDiscardPanel = OpenItemDiscardPanel();
	pDiscardPanel->SetItem( pItem );
*/
	return true;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Returns the item data for the base item in the loadout slot for a given class
//-----------------------------------------------------------------------------
CScriptCreatedItem *CCSInventoryManager::GetBaseItemForClass( int iSlot )
{
	Assert( iSlot >= 0 && iSlot < LOADOUT_POSITION_COUNT );
	return &m_pBaseLoadoutItems[iSlot];
}


//=======================================================================================================================
// CS PLAYER INVENTORY
//=======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSPlayerInventory::CCSPlayerInventory()
{
	m_iBag = BAG_ALL_ITEMS;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSPlayerInventory::MoveItemToPosition( globalindex_t iGlobalIndex, int iPosition )
{
	CCSPlayerInventory *pRootInv = CSInventoryManager()->GetBagForPlayer( m_OwnerID, BAG_ALL_ITEMS );
	if ( !pRootInv )
		return false;

	// Find the item in our root inventory (we don't know where it's moving from)
	CScriptCreatedItem *pMovingItem = pRootInv->GetInventoryItemByGlobalIndex( iGlobalIndex );
	if ( !pMovingItem )
		return false;

	// Is the item allowed to be in the new position?
	if ( !IsItemAllowedInPosition( pMovingItem, iPosition ) )
		return false;

	// Get the current item in the position
	CScriptCreatedItem *pItemAlreadyInPosition = GetItemByPosition(iPosition);
	if ( pItemAlreadyInPosition )
	{
		// See if that item is allowed to be where our new item is
		int iSwapPosition = pMovingItem->GetInventoryPosition();

		if ( !IsItemAllowedInPosition( pItemAlreadyInPosition, iSwapPosition ) )
		{
			// Try to move the swapped item to the backpack, and abort if that fails
			if ( !MoveItemToBackpack( pItemAlreadyInPosition->GetGlobalIndex() ) )
				return false;
		}
		else
		{
			// Move the swapped item to our current position
			SteamUserItems()->UpdateInventoryPos( pItemAlreadyInPosition->GetGlobalIndex(), iSwapPosition );
		}
	}

	SteamUserItems()->UpdateInventoryPos( iGlobalIndex, iPosition );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Tries to move the specified item into the player's backpack. If this inventory isn't the
//			backpack, it'll find the matching player's backpack and move it. 
//			FAILS if the backpack is full. Returns false in that case.
//-----------------------------------------------------------------------------
bool CCSPlayerInventory::MoveItemToBackpack( globalindex_t iGlobalIndex )
{
	if ( m_iBag != BAG_BACKPACK )
	{
		// We're not the backpack inventory. Find it and tell it to move the item.
		CCSPlayerInventory *pBackpackInv = CSInventoryManager()->GetBagForPlayer( m_OwnerID, BAG_BACKPACK );
		if ( pBackpackInv )
			return pBackpackInv->MoveItemToBackpack( iGlobalIndex );
	}
	return false;
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventory::InventoryReceived( EItemRequestResult eResult, int iItems )
{
	BaseClass::InventoryReceived( eResult, iItems );

#ifdef CLIENT_DLL
	if ( eResult == k_EItemRequestResultOK && m_iBag == BAG_ALL_ITEMS )
	{
		// The base bag checks for duplicate positions
		int iCount = m_aInventoryItems.Count();
		for ( int i = iCount-1; i >= 0; i-- )
		{
			uint32 iPosition = m_aInventoryItems[i].GetInventoryPosition();

			// Waiting to be acknowledged?
			if ( iPosition == BACKEND_POSITION_FOR_NEW_ITEM )
				continue;

			bool bInvalidSlot = false;

			// Inside the backpack?
			if ( i > 0 )
			{
				// We're not in an invalid slot yet. But if we're in the same position as another item, we should be moved too.
				if ( iPosition == m_aInventoryItems[i-1].GetInventoryPosition() )
				{
					Warning("WARNING: Found item in a duplicate position. Moving to the backpack.\n" );
					bInvalidSlot = true;
				}
			}

			if ( bInvalidSlot )
			{
				// The item is in an invalid slot. Move it back to the backpack.
				if ( !MoveItemToBackpack( m_aInventoryItems[i].GetGlobalIndex() ) )
				{
					// We failed to move it to the backpack, because the player has no room.
					// Force them to "refind" the item, which will make them throw something out.
					SteamUserItems()->UpdateInventoryPos( m_aInventoryItems[i].GetGlobalIndex(), BACKEND_POSITION_FOR_NEW_ITEM );
				}
			}
		}
	}
#endif
}

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
		Msg("(SERVER) Inventory for account (%d):\n", m_OwnerID.GetAccountID() );
#endif
	}

	Msg("Bag ID : %d\n", m_iBag );

	int iCount = m_aInventoryItems.Count();
	Msg("   Num items: %d\n", iCount );
	for ( int i = 0; i < iCount; i++ )
	{
		char tempstr[1024];
		g_pVGuiLocalize->ConvertUnicodeToANSI( m_aInventoryItems[i].GetItemName(), tempstr, sizeof(tempstr) );
		Msg("      %s (ID %I64d) at slot %d\n", tempstr, m_aInventoryItems[i].GetGlobalIndex(), ExtractBagSlotFromBackendPosition(m_aInventoryItems[i].GetInventoryPosition()) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used to reject items on the backend for inclusion into this inventory.
//			Mostly used for division of bags into different in-game inventories.
//-----------------------------------------------------------------------------
bool CCSPlayerInventory::ItemShouldBeIncluded( int iItemPosition )
{
	if ( m_iBag )
	{
		int iItemBagPosition = ExtractBagFromBackendPosition( iItemPosition );
		if ( iItemBagPosition != m_iBag )
			return false;
	}

	return BaseClass::ItemShouldBeIncluded( iItemPosition );
}

//=======================================================================================================================
// CS PLAYER INVENTORY
//=======================================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

#if 0
CCSPlayerInventoryLoadout::CCSPlayerInventoryLoadout( void )
{
	memset( m_LoadoutItems, LOADOUT_SLOT_USE_BASE_ITEM, LOADOUT_POSITION_COUNT * sizeof(int) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the item in the specified loadout slot for a given class
//-----------------------------------------------------------------------------
CScriptCreatedItem *CCSPlayerInventoryLoadout::GetItemInLoadout( int iSlot )
{
	if ( iSlot < 0 || iSlot >= LOADOUT_POSITION_COUNT )
		return NULL;

	// If we don't have an item in the loadout at that slot, we return the base item
	if ( m_LoadoutItems[iSlot] != LOADOUT_SLOT_USE_BASE_ITEM )
	{
		CScriptCreatedItem *pItem = GetInventoryItemByGlobalIndex( m_LoadoutItems[iSlot] );

#ifdef GAME_DLL
		// To protect against users lying to the backend about the position of their items,
		// we need to validate their position on the server when we retrieve them.
		if ( IsItemAllowedInPosition( pItem, GetBackendPositionFor(GetBag(),iSlot) ) )
#endif
		return pItem;
	}

	return CSInventoryManager()->GetBaseItemForClass( iSlot );
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSPlayerInventoryLoadout::MoveItemToPosition( globalindex_t iGlobalIndex, int iPosition )
{
	if ( !BaseClass::MoveItemToPosition( iGlobalIndex, iPosition ) )
		return false;

	// TODO: Prediction
	// Item has been moved, so update our loadout.
	//int iSlot = ExtractBagSlotFromBackendPosition( iPosition );
	//m_LoadoutItems[iSlot] = iGlobalIndex;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Removes any item in a loadout slot. If the slot hase a base item,
//			the player essentially returns to using that item.
//-----------------------------------------------------------------------------
bool CCSPlayerInventoryLoadout::ClearLoadoutSlot( int iPosition )
{
	CScriptCreatedItem *pItemInSlot = GetItemByPosition(iPosition);
	if ( !pItemInSlot )
		return false;

	if ( !MoveItemToBackpack( pItemInSlot->GetGlobalIndex() ) )
		return false;	

	/*
	// TODO: Prediction
	// It's been moved to the backpack, so clear out loadout entry
	int iSlot = ExtractBagSlotFromBackendPosition( iPosition );
	if ( iSlot >= 0 && iSlot < LOADOUT_POSITION_COUNT )
	{
		m_LoadoutItems[iSlot] = LOADOUT_SLOT_USE_BASE_ITEM;
	}
	*/
	return true;
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Returns true if the specified item is allowed to be stored at the specified position
//			DOES NOT check to make sure the position is clear. It's just a class / slot validity check.
//-----------------------------------------------------------------------------
bool CCSPlayerInventoryLoadout::IsItemAllowedInPosition( CScriptCreatedItem *pItem, int iPosition )
{
	int iSlot = ExtractBagSlotFromBackendPosition( iPosition );
	if ( pItem->GetStaticData()->m_vbClassUsability.IsBitSet( GetLoadoutClass() ) && pItem->GetLoadoutSlot() == iSlot )	
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventoryLoadout::InventoryReceived( EItemRequestResult eResult, int iItems )
{
	if ( eResult == k_EItemRequestResultOK )
	{
		memset( m_LoadoutItems, LOADOUT_SLOT_USE_BASE_ITEM, LOADOUT_POSITION_COUNT * sizeof(int) );
	}

#ifdef CLIENT_DLL
	if ( eResult == k_EItemRequestResultOK )
	{
		// Now that we have all our items, validate our loadout positions.
		int iCount = m_aInventoryItems.Count();
		for ( int i = iCount-1; i >= 0; i-- )
		{
			uint32 iPosition = m_aInventoryItems[i].GetInventoryPosition();

			// Waiting to be acknowledged?
			if ( iPosition == BACKEND_POSITION_FOR_NEW_ITEM )
				continue;

			Assert( ExtractBagFromBackendPosition( iPosition ) == m_iBag );

			int iBagSlot = ExtractBagSlotFromBackendPosition( iPosition );

			// Ensure it's in a valid loadout slot
			bool bInvalidSlot = (iBagSlot >= LOADOUT_POSITION_COUNT);
			if ( !bInvalidSlot )
			{
				bInvalidSlot = !IsItemAllowedInPosition( &m_aInventoryItems[i], iBagSlot );
			}

			if ( bInvalidSlot )
			{
				Warning("WARNING: Found item in an invalid loadout position. Moving to the backpack.\n" );

				// The item is in an invalid slot. Move it back to the backpack.
				if ( !MoveItemToBackpack( m_aInventoryItems[i].GetGlobalIndex() ) )
				{
					// We failed to move it to the backpack, because the player has no room.
					// Force them to "refind" the item, which will make them throw something out.
					SteamUserItems()->UpdateInventoryPos( m_aInventoryItems[i].GetGlobalIndex(), BACKEND_POSITION_FOR_NEW_ITEM );
				}
			}
		}
	}
#endif

	BaseClass::InventoryReceived( eResult, iItems );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventoryLoadout::ItemHasBeenUpdated( CScriptCreatedItem *pItem )
{
	// Now that we have all our items, set up out loadout pointers
	uint32 iPosition = pItem->GetInventoryPosition();
	int iSlot = ExtractBagSlotFromBackendPosition( iPosition );

	if ( iSlot >= 0 && iSlot < LOADOUT_POSITION_COUNT )
	{
		Assert( GetInventoryItemByGlobalIndex(pItem->GetGlobalIndex()) );

		// This may be an invalid slot for the item, but CCSPlayerInventory::InventoryReceived()
		// will have detected that and sent off a request already to move it. The response
		// to that will clear this loadout slot.
		m_LoadoutItems[iSlot] = pItem->GetGlobalIndex();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventoryLoadout::ItemIsBeingRemoved( CScriptCreatedItem *pItem )
{
	// Now that we have all our items, set up out loadout pointers
	uint32 iPosition = pItem->GetInventoryPosition();
	int iSlot = ExtractBagSlotFromBackendPosition( iPosition );

	if ( iSlot >= 0 && iSlot < LOADOUT_POSITION_COUNT )
	{
		m_LoadoutItems[iSlot] = LOADOUT_SLOT_USE_BASE_ITEM;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventoryLoadout::DumpInventoryToConsole( bool bRoot )
{
	if ( bRoot )
	{
		Msg("========================================\n");
#ifdef CLIENT_DLL
		Msg("(CLIENT) Inventory:\n");
#else
		Msg("(SERVER) Inventory for account (%d):\n", m_OwnerID.GetAccountID() );
#endif
	}

	Msg("Loadout for Class: %s (%d)\n", g_aPlayerClassNames_NonLocalized[GetLoadoutClass()], GetLoadoutClass() );

	BaseClass::DumpInventoryToConsole( false );

	if ( GetItemCount() > 0 )
	{
		Msg("   LOADOUT:\n");
		for ( int i = 0; i < LOADOUT_POSITION_COUNT; i++ )
		{
			Msg("      Slot %d: ", i );

			if ( m_LoadoutItems[i] != LOADOUT_SLOT_USE_BASE_ITEM )
			{
				CScriptCreatedItem *pItem = GetInventoryItemByGlobalIndex( m_LoadoutItems[i] );
				char tempstr[1024];
				g_pVGuiLocalize->ConvertUnicodeToANSI( pItem->GetItemName(), tempstr, sizeof(tempstr) );
				Msg("%s (ID %I64d)\n", tempstr, pItem->GetGlobalIndex() );
			}
			else
			{
				Msg("\n");
			}
		}
	}
}
#endif

#if 0
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CCSPlayerInventoryBackpack::CCSPlayerInventoryBackpack( void )
{
	SetBag( BAG_BACKPACK );
	m_iPredictedEmptySlot = 1;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventoryBackpack::ItemHasBeenUpdated( CScriptCreatedItem *pItem )
{
	BaseClass::ItemHasBeenUpdated( pItem );

	// Now that we have all our items, set up out loadout pointers
	uint32 iPosition = pItem->GetInventoryPosition();
	int iSlot = ExtractBagSlotFromBackendPosition( iPosition );

	if ( iSlot == m_iPredictedEmptySlot )
	{
		CalculateNextEmptySlot();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSPlayerInventoryBackpack::CalculateNextEmptySlot( void )
{
	// Move forward looking for the first slot that doesn't have an item in it.
	// We start from our current slot and only move forward, so we don't run into the problem of 
	// choosing a slot that's probably got another item in the process of being moved into it.
	do 
	{
		m_iPredictedEmptySlot++;

		if ( !GetItemByPosition( GetBackendPositionFor(BAG_BACKPACK, m_iPredictedEmptySlot)) )
			break;
	} while ( m_iPredictedEmptySlot < 1000 );
}

//-----------------------------------------------------------------------------
// Purpose: Tries to move the specified item into the player's backpack.
//			FAILS if the backpack is full. Returns false in that case.
//-----------------------------------------------------------------------------
bool CCSPlayerInventoryBackpack::MoveItemToBackpack( globalindex_t iGlobalIndex )
{
	Warning("Moved item %I64d to backpack slot: %d\n", iGlobalIndex, m_iPredictedEmptySlot );

	Assert( GetItemByPosition(GetBackendPositionFor(BAG_BACKPACK, m_iPredictedEmptySlot)) == NULL );

	// Move to our current predicted empty slot, and then try to find the next slot.
	SteamUserItems()->UpdateInventoryPos( iGlobalIndex, GetBackendPositionFor(BAG_BACKPACK, m_iPredictedEmptySlot) );
	CalculateNextEmptySlot();
	return true;
}
#endif


#ifdef CLIENT_DLL
// WTF: Declaring this inline caused a compiler bug.
CCSPlayerInventory	*CCSInventoryManager::GetLocalCSInventory( void ) 
{ 
	return &m_LocalInventory; 
}
#endif

#ifdef _DEBUG
#if defined(CLIENT_DLL)
CON_COMMAND_F( item_dumpinv, "Dumps the contents of a specified client inventory. Format: item_dumpinv <bag index>", FCVAR_CHEAT )
#else
CON_COMMAND_F( item_dumpinv_sv, "Dumps the contents of a specified server inventory. Format: item_dumpinv_sv <bag index>", FCVAR_CHEAT )
#endif
{
	int iBag = ( args.ArgC() > 1 ) ? atoi(args[1]) : BAG_ALL_ITEMS;
#if defined(CLIENT_DLL)
	CSteamID steamID = steamapicontext->SteamUser()->GetSteamID();
#else
	CSteamID steamID;
	CCSPlayer *pPlayer = ToCSPlayer( UTIL_GetCommandClient() ); 
	pPlayer->GetSteamID( &steamID );
#endif
	CPlayerInventory *pInventory = CSInventoryManager()->GetBagForPlayer( steamID, (inventory_bags_t)iBag );
	if ( !pInventory )
	{
		Msg("No inventory for bag %d\n", iBag);
		return;
	}

	pInventory->DumpInventoryToConsole( true );
}
#endif

#endif