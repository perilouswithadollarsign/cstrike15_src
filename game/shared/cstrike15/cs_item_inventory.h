//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Container that allows client & server access to data in player inventories & loadouts
//
//=============================================================================

#ifndef CS_ITEM_INVENTORY_H
#define CS_ITEM_INVENTORY_H
#ifdef _WIN32
#pragma once
#endif

#include "item_inventory.h"
#include "cs_shareddefs.h"

#define LOADOUT_SLOT_USE_BASE_ITEM		0

//===============================================================================================================
// POSITION HANDLING
//===============================================================================================================
// Items at this position have been found, but not presented to the player
#define BACKEND_POSITION_FOR_NEW_ITEM	0

// We store a bag index in the highbits of the inventory position.
// The lowbit stores the position of the item within the bag.
enum inventory_bags_t
{
	BAG_ALL_ITEMS = 0,
	BAG_BACKPACK = 1,

	BAG_TEAM_BASE = 1000,
};

#define MAX_BACKPACK_SLOTS		50

inline uint32 GetBackendPositionFor( int iBag, int iSlot )
{
	uint32 iPos = iSlot | (iBag << 16);
	return iPos;
}
inline int ExtractBagFromBackendPosition( uint32 iBackendPosition )
{
	int iHighbits = iBackendPosition >> 16;
	return iHighbits;
}
inline int ExtractBagSlotFromBackendPosition( uint32 iBackendPosition )
{
	int iLowbits = iBackendPosition & 0xFFFF;
	return iLowbits;
}

/*
inline int GetClassFromLoadoutBag( int iBag )
{
	return (iBag - BAG_CLASS_LOADOUT_SCOUT + 1);
}
inline inventory_bags_t GetLoadoutBagForClass( int iClass )
{
	return (inventory_bags_t)(BAG_CLASS_LOADOUT_SCOUT + iClass - 1);
}
*/


//===============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: A single CS player's inventory. 
//		On the client, the inventory manager contains contains an instance of this for the local player.
//		On the server, each player contains an instance of this.
//-----------------------------------------------------------------------------
class CCSPlayerInventory : public CPlayerInventory
{
	DECLARE_CLASS( CCSPlayerInventory, CPlayerInventory );
public:
	CCSPlayerInventory();

		void				SetBag( int iBag ) { m_iBag = iBag; }
		int					GetBag( void ) { return m_iBag; }

	// Returns true if the specified item is allowed to be stored at the specified position
	// DOES NOT check to make sure the position is clear. It's just a class / slot validity check.
	virtual bool		IsItemAllowedInPosition( CScriptCreatedItem *pItem, int iPosition ) { return true; }

	// Used to reject items on the backend for inclusion into this inventory.
	// Mostly used for division of bags into different in-game inventories.
	virtual bool		ItemShouldBeIncluded( int iItemPosition );

protected:
	// Item movement functions that are called by the inventory manager
#ifdef CLIENT_DLL
	// Updates the backpack or loadout for the specified item
	// Returns false if the item isn't allowed to be placed in the specified position.
	virtual bool		MoveItemToPosition( globalindex_t iGlobalIndex, int iPosition );

	// Tries to move the specified item into the player's backpack. If this inventory isn't the
	// backpack, it'll find the matching player's backpack and move it. 
	// FAILS if the backpack is full. Returns false in that case.
	virtual bool		MoveItemToBackpack( globalindex_t iGlobalIndex );
#endif

	// Inventory updating, called by the Inventory Manager only. If you want an inventory updated,
	// use the SteamRequestX functions in CInventoryManager.
	virtual void		InventoryReceived( EItemRequestResult eResult, int iItems );

	// Debugging
	virtual void		DumpInventoryToConsole( bool bRoot );

protected:
	// The bag position of this inventory, if any (0 == contains all items)
	// If this inventory has a bag position, items inside other bags will be ignored.
	int			m_iBag;

	friend class CCSInventoryManager;
// 	friend class CTFPlayerInventoryLoadout;
};

/*
//-----------------------------------------------------------------------------
// Purpose: Custom TF inventory that manages the player's loadout
//-----------------------------------------------------------------------------
class CTFPlayerInventoryLoadout : public CCSPlayerInventory
{
	DECLARE_CLASS( CTFPlayerInventoryLoadout, CCSPlayerInventory );
public:
	CTFPlayerInventoryLoadout( void );

	// Returns the item in the specified loadout slot for a given class
	CScriptCreatedItem	*GetItemInLoadout( int iSlot );

	// Get the class that this loadout inventory is for
	int					GetLoadoutClass( void ) { return GetClassFromLoadoutBag(m_iBag); }

	// Returns true if the specified item is allowed to be stored at the specified position
	// DOES NOT check to make sure the position is clear. It's just a class / slot validity check.
	bool				IsItemAllowedInPosition( CScriptCreatedItem *pItem, int iPosition );

	// Debugging
	virtual void		DumpInventoryToConsole( bool bRoot );

protected:
	// Item movement functions that are called by the inventory manager
#ifdef CLIENT_DLL
	// Updates the backpack or loadout for the specified item
	// Returns false if the item isn't allowed to be placed in the specified position.
	virtual bool		MoveItemToPosition( globalindex_t iGlobalIndex, int iPosition );

	// Removes any item in a loadout slot. If the slot has a base item,
	// the player essentially returns to using that item.
	// NOTE: This can fail if the player has no backpack space to contain the equipped item.
	bool				ClearLoadoutSlot( int iSlot );
#endif

protected:
	// Inventory updating, called by the Inventory Manager only. If you want an inventory updated,
	// use the SteamRequestX functions in CInventoryManager.
	virtual void		InventoryReceived( EItemRequestResult eResult, int iItems );

	// Derived inventory hooks
	virtual void		ItemHasBeenUpdated( CScriptCreatedItem *pItem );
	virtual void		ItemIsBeingRemoved( CScriptCreatedItem *pItem );

private:
	// Global indices of the items in our inventory in the loadout slots
	globalindex_t		m_LoadoutItems[ LOADOUT_POSITION_COUNT ];

	friend class CCSInventoryManager;
};

//-----------------------------------------------------------------------------
// Purpose: Custom TF inventory that manages the player's loadout
//-----------------------------------------------------------------------------
class CTFPlayerInventoryBackpack : public CCSPlayerInventory
{
	DECLARE_CLASS( CTFPlayerInventoryBackpack, CCSPlayerInventory );
public:
	CTFPlayerInventoryBackpack( void );

#ifdef CLIENT_DLL
	// Tries to move the specified item into the player's backpack. 
	// FAILS if the backpack is full. Returns false in that case.
	virtual bool		MoveItemToBackpack( globalindex_t iGlobalIndex );

protected:
	virtual void		ItemHasBeenUpdated( CScriptCreatedItem *pItem );
	void				CalculateNextEmptySlot( void );
#endif

private:
	int			m_iPredictedEmptySlot;
};
*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSInventoryManager : public CInventoryManager
{
	DECLARE_CLASS( CCSInventoryManager, CInventoryManager );
public:
	CCSInventoryManager();

	virtual void		PostInit( void );

#ifdef CLIENT_DLL
	// Show the player a pickup screen with any items they've collected recently, if any
	bool				ShowItemsPickedUp( void );
	// Force the player to discard an item to make room for a new item, if they have one
	bool				CheckForRoomAndForceDiscard( void );
#endif

	// Returns the item data for the base item in the loadout slot for a given class
	CScriptCreatedItem	*GetBaseItemForClass( int iSlot );
	void				GenerateBaseItems( void );

	// Gets the specified inventory for the steam ID
	CCSPlayerInventory	*GetBagForPlayer( CSteamID &playerID, inventory_bags_t iBag );

	// Returns the item in the specified loadout slot for a given class
	CScriptCreatedItem	*GetItemInLoadoutForClass( int iClass, int iSlot, CSteamID *pID = NULL );

private:
	// Base items, returned for slots that the player doesn't have anything in
	CScriptCreatedItem m_pBaseLoadoutItems[ LOADOUT_POSITION_COUNT ];

#ifdef CLIENT_DLL
	// On the client, we have a single inventory for the local player. Stored here, instead of in the
	// local player entity, because players need to access it while not being connected to a server.
public:
	virtual void		UpdateLocalInventory( void );
	CPlayerInventory	*GetLocalInventory( void ) { return &m_LocalInventory; }
	CCSPlayerInventory	*GetLocalTFInventory( void );

	// Try and equip the specified item in the specified class's loadout slot
	bool				EquipItemInLoadout( int iClass, int iSlot, globalindex_t iGlobalIndex );

	// Fills out pList with all inventory items that could fit into the specified loadout slot for a given class
	int					GetAllUsableItemsForSlot( int iClass, int iSlot, CUtlVector<CScriptCreatedItem*> *pList );
private:
	CCSPlayerInventory			m_LocalInventory;
/*
	CTFPlayerInventoryBackpack	m_LocalBackpack;
	CTFPlayerInventoryLoadout	m_LoadoutInventories[TF_CLASS_COUNT];
*/
#endif // CLIENT_DLL
};

CCSInventoryManager *CSInventoryManager( void );

#endif // CS_ITEM_INVENTORY_H
