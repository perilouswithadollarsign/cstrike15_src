//====== Copyright © Valve Corporation, All rights reserved. =======
//
// Purpose: Container that allows client & server access to data in player inventories & loadouts
//
//=============================================================================

#pragma once


#include "econ_item_inventory.h"
#include "shareddefs.h"
#include "cs_shareddefs.h"
#include "econ_item_constants.h"
#include "cstrike15_item_constants.h"


#define LOADOUT_SLOT_USE_BASE_ITEM		0

namespace vgui
{
	class Panel;
}

struct baseitemcriteria_t;

//===============================================================================================================
//-----------------------------------------------------------------------------
// Purpose: A single player's inventory. 
//		On the client, the inventory manager contains contains an instance of this for the local player.
//		On the server, each player contains an instance of this.
//-----------------------------------------------------------------------------
class CCSPlayerInventory : public CPlayerInventory
{
	DECLARE_CLASS( CCSPlayerInventory, CPlayerInventory );
public:
	CCSPlayerInventory();

	virtual CEconItemView	*GetItemInLoadout( int iClass, int iSlot ) const;

	CEconItemView* GetItemInLoadoutFilteredByProhibition( int iClass, int iSlot ) const;


#ifdef CLIENT_DLL
	// Removes any item in a loadout slot. If the slot has a base item,
	// the player essentially returns to using that item.
	// NOTE: This can fail if the player has no backpack space to contain the equipped item.
	bool				ClearLoadoutSlot( int iClass, int iSlot );

	int					GetLastCompletedNodeForCampaign( uint32 unCampaignID ) const;
#endif

	virtual int			GetMaxItemCount( void ) const;
	virtual bool		CanPurchaseItems( int iItemCount ) const;
	virtual int			GetPreviewItemDef( void ) const;
	
	bool IsMissionRefuseAllowed( void ) const;

	// Derived inventory hooks
	virtual void		ItemHasBeenUpdated( CEconItemView *pItem, bool bUpdateAckFile, bool bWriteAckFile, EInventoryItemEvent eEventType ) OVERRIDE;
	virtual void		ItemIsBeingRemoved( CEconItemView *pItem ) OVERRIDE;
	virtual void		DefaultEquippedDefinitionHasBeenUpdated( CEconDefaultEquippedDefinitionInstanceClient *pDefaultEquippedDefinition ) OVERRIDE;

	// Debugging
	virtual void		DumpInventoryToConsole( bool bRoot );

	virtual void		NotifyHasNewItems() { OnHasNewItems(); }

public:
	float				FindInventoryItemWithMaxAttributeValue( char const *szItemType, char const *szAttrClass, CEconItemView **ppItemFound = NULL );
	void				FindInventoryItemsWithAttribute( char const *szAttrClass, CUtlVector< CEconItemView* > &foundItems, bool bMatchValue = false, uint32 unValue = 0 );	// DEPRECATED. Use CSchemaAttributeDefHandle version instead and use static handles.
	void				FindInventoryItemsWithAttribute( CSchemaAttributeDefHandle pAttr, CUtlVector< CEconItemView* > &foundItems, CUtlVector< CEconItemView* > *searchSet = NULL, bool bMatchValue = false, uint32 unValue = 0 );
	itemid_t			GetActiveSeasonItemId( bool bCoin = true /* false is the Pass */ );
	uint32				GetActiveQuestID( void ) const;
	void				RefreshActiveQuestID( void );


	bool				IsEquipped( CEconItemView * pEconItem, int nTeam );
	bool				IsEquipped( CEconItemView * pEconItem );

protected:
#ifdef CLIENT_DLL
	// Converts an old format inventory to the new format.
	void				ConvertOldFormatInventoryToNew( void );
#endif

	int					m_nActiveQuestID;

	virtual void		OnHasNewItems();
	virtual void		ValidateInventoryPositions( void );
	
	virtual void		SOCacheSubscribed( GCSDK::SOID_t owner, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;

	// Extracts the position that should be used to sort items in the inventory from the backend position.
	// Necessary if your inventory packs a bunch of info into the position instead of using it just as a position.
	virtual int			ExtractInventorySortPosition( uint32 iBackendPosition )
	{ 
		// Consider unack'd items as -1, so they get stacked up before the 0th slot item
		if ( IsUnacknowledged(iBackendPosition) )
			return -1;
		return ExtractBackpackPositionFromBackend(iBackendPosition); 
	}

	virtual void SOCreated( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )	OVERRIDE;
	virtual void SOUpdated( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )   OVERRIDE;
	virtual void SODestroyed( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent )	OVERRIDE;

protected:
	// Global indices of the items in our inventory in the loadout slots
	itemid_t		m_LoadoutItems[ LOADOUT_COUNT ][ LOADOUT_POSITION_COUNT ];

	friend class CCSInventoryManager;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCSInventoryManager : public CInventoryManager
{
	DECLARE_CLASS( CCSInventoryManager, CInventoryManager );
public:
	CCSInventoryManager();

	virtual bool		Init( void );
	virtual void		PostInit( void );
	virtual void		Shutdown( void );
	virtual void		LevelInitPreEntity( void ) OVERRIDE;

#ifdef CLIENT_DLL
	virtual CPlayerInventory *GeneratePlayerInventoryObject() const { return new CCSPlayerInventory; }

	// Get the number of items picked up
	virtual int			GetNumItemPickedUpItems( void );				

	// Show the player a pickup screen with any items they've collected recently, if any
	virtual bool		ShowItemsPickedUp( bool bForce = false, bool bReturnToGame = true, bool bNoPanel = false );
	void				UpdateUnacknowledgedItems( void );

	void RebuildUnacknowledgedItemList( CUtlVector< CEconItemView* > *pVecItemsAckedOnClientOnly = NULL );

	// Get list of unacknowledged items. Optionally will rebuild the list before returning.
	CUtlVector<CEconItemView*> &GetUnacknowledgedItems( bool bRefreshList ); 

	void				AcknowledgeUnacknowledgedItems( void );
	bool				AcknowledgeUnacknowledgedItem( itemid_t ui64ItemID );

	// Show the player a pickup screen with the items they've crafted
	virtual void		ShowItemsCrafted( CUtlVector<itemid_t> *vecCraftedIndices );

	// Force the player to discard an item to make room for a new item, if they have one
	virtual bool		CheckForRoomAndForceDiscard( void );

#endif
	// Does a mapping to a "class" index based on a class index and a preset index, so that presets can be stored in the GC.
	virtual equipped_class_t	DoClassMapping( equipped_class_t unClass, equipped_preset_t unPreset );

	// Returns the item data for the base item in the loadout slot for a given class
	CEconItemView		*GetBaseItemForTeam( int iClass, int iSlot );
	void				GenerateBaseItems( void );

	// Gets the specified inventory for the steam ID
	CCSPlayerInventory	*GetInventoryForPlayer( const CSteamID &playerID );

	// Returns the item in the specified loadout slot for a given class
	CEconItemView		*GetItemInLoadoutForTeam( int iClass, int iSlot, CSteamID *pID = NULL );

	int					GetSlotForBaseOrDefaultEquipped( int iClass, item_definition_index_t );

	// Fills out the vector with the sets that are currently active on the specified player & class
	void				GetActiveSets( CUtlVector<const CEconItemSetDefinition *> *pItemSets, CSteamID steamIDForPlayer, int iClass );

	// We're generating a base item. We need to add the game-specific keys to the criteria so that it'll find the right base item.
	virtual void		AddBaseItemCriteria( baseitemcriteria_t *pCriteria, CItemSelectionCriteria *pSelectionCriteria );

private:
	// Base items, returned for slots that the player doesn't have anything in
	CEconItemView m_pBaseLoadoutItems[ LOADOUT_COUNT ][ LOADOUT_POSITION_COUNT ];

#ifdef CLIENT_DLL
	// On the client, we have a single inventory for the local player. Stored here, instead of in the
	// local player entity, because players need to access it while not being connected to a server.
public:
	CPlayerInventory	*GetLocalInventory( void ) { Assert( m_pLocalInventory ); return m_pLocalInventory; }
	CCSPlayerInventory	*GetLocalCSInventory( void ) { Assert( m_pLocalInventory ); return m_pLocalInventory; }

	// Try and equip the specified item in the specified class's loadout slot
	bool				EquipItemInLoadout( int iClass, int iSlot, itemid_t iItemID, bool bSwap = false );

	bool				CleanupDuplicateBaseItems( int iClass );

	// Fills out pList with all inventory items that could fit into the specified loadout slot for a given class
	int					GetAllUsableItemsForSlot( int iClass, int iSlot, unsigned int unUsedEquipRegionMask, CUtlVector<CEconItemView*> *pList );

	virtual int			GetBackpackPositionFromBackend( uint32 iBackendPosition ) { return ExtractBackpackPositionFromBackend(iBackendPosition); }

private:

	CCSPlayerInventory	*m_pLocalInventory;
	CUtlVector<CEconItemView*>	m_UnacknowledgedItems;

#endif // CLIENT_DLL
};

CCSInventoryManager *CSInventoryManager( void );

bool AreSlotsConsideredIdentical( int iBaseSlot, int iTestSlot );

