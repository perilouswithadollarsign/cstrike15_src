//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Container that allows client & server access to data in player inventories & loadouts
//
//=============================================================================

#ifndef ITEM_INVENTORY_H
#define ITEM_INVENTORY_H
#ifdef _WIN32
#pragma once
#endif

#include "igamesystem.h"
#include "econ_entity.h"
#include "econ_item_view.h"
#include "utlsortvector.h"

#if !defined(NO_STEAM)
#include "steam/steam_api.h"
#include "gcsdk/gcclientsdk.h"
#endif // NO_STEAM


class CPlayerInventory;
class CEconItem;
struct baseitemcriteria_t;


// Inventory Less function.
// Used to sort the inventory items into their positions.
class CInventoryListLess
{
public:
	bool Less( CEconItemView* const &src1, CEconItemView *const &src2, void *pCtx );
};

// A class that wants notifications when an inventory is updated
class IInventoryUpdateListener : public GCSDK::ISharedObjectListener
{
public:
	virtual void InventoryUpdated( CPlayerInventory *pInventory ) = 0;
	virtual void SOCreated( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) OVERRIDE { InventoryUpdated( NULL ); }
	virtual void SOUpdated( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) OVERRIDE { InventoryUpdated( NULL ); }
	virtual void SODestroyed( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) OVERRIDE { InventoryUpdated( NULL ); }
	virtual void SOCacheSubscribed( GCSDK::SOID_t owner, GCSDK::ESOCacheEvent eEvent ) OVERRIDE { InventoryUpdated( NULL ); }
	virtual void SOCacheUnsubscribed( GCSDK::SOID_t owner, GCSDK::ESOCacheEvent eEvent ) OVERRIDE { InventoryUpdated( NULL ); }
};

//-----------------------------------------------------------------------------
// Purpose: A single player's inventory. 
//		On the client, the inventory manager contains an instance of this for the local player.
//		On the server, each player contains an instance of this.
//-----------------------------------------------------------------------------
class CPlayerInventory : public GCSDK::ISharedObjectListener
{
	DECLARE_CLASS_NOBASE( CPlayerInventory );
public:
	CPlayerInventory();
	virtual ~CPlayerInventory();

	void Shutdown( void );

	// Returns true if this inventory has been filled out by Steam.
	bool	InventoryRetrievedFromSteamAtLeastOnce( void ) const { return m_bGotItemsFromSteamAtLeastOnce; }
	bool	InventorySubscribedToSteamCurrently( void ) const { return m_bCurrentlySubscribedToSteam; }

	// Inventory access
	GCSDK::SOID_t				GetOwner( void ) { return m_OwnerID; }
	void						SetOwner( GCSDK::SOID_t& ownerID ) { m_OwnerID = ownerID; }
	int					GetItemCount( void ) const { return m_Items.GetItemVector().Count(); }
	int					GetDefaultEquippedDefinitionItemsCount( void ) const { return m_aDefaultEquippedDefinitionItems.Count(); }
	virtual bool		CanPurchaseItems( int iItemCount ) const { return GetMaxItemCount() - GetItemCount() >= iItemCount; }
	virtual int			GetMaxItemCount( void ) const { return DEFAULT_NUM_BACKPACK_SLOTS; }
	CEconItemView		*GetItem( int i ) { return m_Items.GetItemVector()[i]; }
	CEconItemView		*GetDefaultEquippedDefinitionItem( int i ) { return m_aDefaultEquippedDefinitionItems[ i ]; }
	CEconItemView		*FindDefaultEquippedDefinitionItemBySlot( int iClass, int iSlot ) const;
	bool				GetDefaultEquippedDefinitionItemSlotByDefinitionIndex( item_definition_index_t nDefIndex, int nClass, int &nSlot );
	void				SetDefaultEquippedDefinitionItemBySlot( int iClass, int iSlot, item_definition_index_t iDefIndex );

	void FindItemsByType( EItemType eType, CUtlVector< CEconItemView* >& outVec );

	virtual CEconItemView	*GetItemInLoadout( int iClass, int iSlot ) const { AssertMsg( 0, "Implement me!" ); return NULL; }

	
	// Get the item object cache data for the specified item
	CEconItem			*GetSOCDataForItem( itemid_t iItemID );
	const CEconItem		*GetSOCDataForItem( itemid_t iItemID ) const;
	GCSDK::CGCClientSharedObjectCache	*GetSOC( void ) { return m_pSOCache; }

	// tells the GC systems to forget about this listener
	void				RemoveListener( GCSDK::ISharedObjectListener *pListener );

	// Finds the item in our inventory that matches the specified global index
	CEconItemView		*GetInventoryItemByItemID( itemid_t iIndex ) const;

	// Finds the item in our inventory in the specified position
	CEconItemView		*GetItemByPosition( int iPosition, int *pIndex = NULL );

	// Used to reject items on the backend for inclusion into this inventory.
	// Mostly used for division of bags into different in-game inventories.
	virtual bool		ItemShouldBeIncluded( int iItemPosition ) { return true; }

	// Debugging
	virtual void		DumpInventoryToConsole( bool bRoot );

	// Extracts the position that should be used to sort items in the inventory from the backend position.
	// Necessary if your inventory packs a bunch of info into the position instead of using it just as a position.
	virtual int			ExtractInventorySortPosition( uint32 iBackendPosition ) { return iBackendPosition; }

	// Recipe access
	int									 GetRecipeCount( void ) const;
	const CEconCraftingRecipeDefinition *GetRecipeDef( int iIndex ) const;
	const CEconCraftingRecipeDefinition *GetRecipeDefByDefIndex( uint16 iDefIndex ) const;

	int GetMaxCraftIngredientsNeeded( const CUtlVector< itemid_t >& vecCraftEconItems ) const;

	void GetMarketCraftCompletionLink( const CUtlVector< itemid_t >& vecCraftItems, char *pchLink, int nNumChars ) const;

	int GetPossibleCraftResultsCount( const CUtlVector< itemid_t >& vecCraftItems ) const;
	int GetPossibleCraftResultsCount( const CUtlVector< CEconItem* >& vecCraftEconItems ) const;
	const CEconCraftingRecipeDefinition *GetPossibleCraftResult( const CUtlVector< itemid_t >& vecCraftItems, int nIndex ) const;
	int GetPossibleCraftResultID( const CUtlVector< itemid_t >& vecCraftItems, int nIndex ) const;
	const wchar_t* GetPossibleCraftResultName( const CUtlVector< itemid_t >& vecCraftItems, int nIndex ) const;
	const wchar_t* GetPossibleCraftResultDescription( const CUtlVector< itemid_t >& vecCraftItems, int nIndex ) const;
	bool CanAddToCraftTarget( const CUtlVector< itemid_t >& vecCraftItems, itemid_t nNewItem ) const;
	bool IsCraftReady( const CUtlVector< itemid_t >& vecCraftItems, int16 &nRecipe ) const;
	void CraftIngredients( const CUtlVector< itemid_t >& vecCraftItems ) const;
	void SetTargetCraftRecipe( int nRecipe ) { m_nTargetRecipe = nRecipe; }
	int16 GetTargetCraftRecipe( void ) const { return m_nTargetRecipe; }

	// Finds the item in our inventory that matches the specified global index
	void ClearItemCustomName( itemid_t iIndex );

	void WearItemSticker( itemid_t iIndex, int nSlot, float flStickerWearCurrent );

	// Item access
	uint32				GetItemCount( uint32 unItemDef );

	// Item previews
	virtual int			GetPreviewItemDef( void ) const { return 0; };

	// Access helpers
	virtual void		SOClear();

	virtual void		NotifyHasNewItems() {}

	bool				AddEconItem( CEconItem * pItem, bool bUpdateAckFile, bool bWriteAckFile, bool bCheckForNewItems );

	enum EInventoryItemEvent
	{
		k_EInventoryItemCreated,
		k_EInventoryItemUpdated,
		k_EInventoryItemDestroyed,
	};

	// Made public for an inventory helper func
	int					m_nTargetRecipe;

protected:
	// Inventory updating, called by the Inventory Manager only. If you want an inventory updated,
	// use the SteamRequestX functions in CInventoryManager.
	void				RequestInventory( GCSDK::SOID_t ID );
	void				AddListener( GCSDK::ISharedObjectListener *pListener );
	virtual void		RemoveItem( itemid_t iItemID );
	bool				AddEconDefaultEquippedDefinition( CEconDefaultEquippedDefinitionInstanceClient *pDefaultEquippedDefinition );
	bool				FilloutItemFromEconItem( CEconItemView *pScriptItem, CEconItem *pEconItem );
	virtual void		SendInventoryUpdateEvent();
	virtual void		OnHasNewItems() {}
	virtual void		OnItemChangedPosition( CEconItemView *pItem, uint32 iOldPos ) { return; }

	virtual void		SOCreated( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;
	virtual void		SOUpdated( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;
	virtual void		SODestroyed( GCSDK::SOID_t owner, const GCSDK::CSharedObject *pObject, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;
	virtual void		SOCacheSubscribed( GCSDK::SOID_t owner, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;
	virtual void		SOCacheUnsubscribed( GCSDK::SOID_t owner, GCSDK::ESOCacheEvent eEvent ) OVERRIDE;

	virtual void		ValidateInventoryPositions( void );




	// Derivable Inventory hooks
	virtual void		ItemHasBeenUpdated( CEconItemView *pItem, bool bUpdateAckFile, bool bWriteAckFile, EInventoryItemEvent eEventType );
	virtual void		ItemIsBeingRemoved( CEconItemView *pItem ) { return; }
	virtual void		DefaultEquippedDefinitionHasBeenUpdated( CEconDefaultEquippedDefinitionInstanceClient *pDefaultEquippedDefinition );
	virtual void		MarkSetItemDescriptionsDirty( int nItemSetIndex );

	// Get the index for the item in our inventory utlvector
	int					GetIndexForItem( CEconItemView *pItem );



protected:
	// The SharedObject Id of the player who owns this inventory
	GCSDK::SOID_t m_OwnerID;

	CUtlVector< CEconItemView*> m_aDefaultEquippedDefinitionItems;

	typedef CUtlMap< itemid_t, CEconItemView*, int, CDefLess< itemid_t > > ItemIdToItemMap_t;
	// Wrap different container types so they all stay in sync with inventory state
	class CItemContainers
	{
	public:
		void Add( CEconItemView* pItem );
		void Remove( itemid_t ullItemID );
		void Purge();
		bool DbgValidate() const; // Walk all containers and confirm content matches

		const CUtlVector< CEconItemView*>& GetItemVector( void ) const { return m_vecInventoryItems; }
		const ItemIdToItemMap_t& GetItemMap( void ) const { return m_mapItemIDToItemView; }

	private:
		// The items the player has in his inventory, received from steam.
		CUtlVector< CEconItemView*> m_vecInventoryItems;

		// Copies of pointers in vector for faster lookups
		ItemIdToItemMap_t m_mapItemIDToItemView;
	};

	CItemContainers	m_Items;

	int			m_iPendingRequests;
	bool		m_bGotItemsFromSteamAtLeastOnce, m_bCurrentlySubscribedToSteam;

	GCSDK::CGCClientSharedObjectCache	  *m_pSOCache;

	CUtlVector<GCSDK::ISharedObjectListener *> m_vecListeners;

	friend class CInventoryManager;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CInventoryManager : public CAutoGameSystem
{
	DECLARE_CLASS_GAMEROOT( CInventoryManager, CAutoGameSystem );
public:
	CInventoryManager( void );
	virtual ~CInventoryManager() {}

	// Adds the inventory to the list of inventories that should be maintained.
	// This causes the game to load the items for the SteamID into this inventory.
	// NOTE: This fires off a request to Steam. The data will not be filled out immediately.
	void SteamRequestInventory( CPlayerInventory *pInventory, CSteamID pSteamID, IInventoryUpdateListener *pListener = NULL );
	void RegisterPlayerInventory( CPlayerInventory *pInventory, IInventoryUpdateListener *pListener = NULL, CSteamID* pSteamID = NULL );

	void PreInitGC();
	void PostInitGC();

#ifdef CLIENT_DLL
	void	DeleteItem( itemid_t iItemID, bool bRecycle=false );
	int		DeleteUnknowns( CPlayerInventory *pInventory );
#endif

public:
	//-----------------------------------------------------------------------
	// IAutoServerSystem
	//-----------------------------------------------------------------------
	virtual bool Init( void );
	virtual void PostInit( void );
	virtual void Shutdown();
	virtual void LevelInitPreEntity( void );
	virtual void LevelShutdownPostEntity( void );
#if defined(CSTRIKE15) && defined(CLIENT_DLL)
	virtual void InsertMaterialGenerationJob( CEconItemView *pItemView );
	virtual void OnDestroyEconItemView( CEconItemView *pItemView );
#endif

	void GameServerSteamAPIActivated();

	virtual CPlayerInventory *GetInventoryForAccount( uint32 iAccountID );

	// We're generating a base item. We need to add the game-specific keys to the criteria so that it'll find the right base item.
	virtual void		AddBaseItemCriteria( baseitemcriteria_t *pCriteria, CItemSelectionCriteria *pSelectionCriteria ) { return; }

#ifdef CLIENT_DLL
	// Must be implemented by derived class
	virtual bool		EquipItemInLoadout( int iClass, int iSlot, itemid_t iItemID, bool bSwap = false ) = 0;

	virtual CPlayerInventory *GeneratePlayerInventoryObject() const { return new CPlayerInventory; }
	void DestroyPlayerInventoryObject( CPlayerInventory *pPlayerInventory ) const;

	//-----------------------------------------------------------------------
	// LOCAL INVENTORY
	//
	// On the client, we have a single inventory for the local player. Stored here, instead of in the
	// local player entity, because players need to access it while not being connected to a server.
	// Override GetLocalInventory() in your inventory manager and return your custom local inventory.
	//-----------------------------------------------------------------------
	virtual void				UpdateLocalInventory( void );
	virtual CPlayerInventory	*GetLocalInventory( void ) { return NULL; }

	// The local inventory is used to track discards & responses to. We need to
	// make a decision about inventory space right after sending a delete request,
	// so we predict the request will work.
	void				OnItemDeleted( CPlayerInventory *pInventory ) { if ( pInventory == GetLocalInventory() ) m_iPredictedDiscards--; }

	static void			SendGCConnectedEvent( void );

	// Returns the item at the specified backpack position
	virtual CEconItemView	*GetItemByBackpackPosition( int iBackpackPosition );

	// Moves the item to the specified backpack position. If there's another item as that spot, it swaps positions with it.
	virtual void		MoveItemToBackpackPosition( CEconItemView *pItem, int iBackpackPosition );

	// Tries to set the item to the specified backpack position. Passing in 0 will find the first empty position.
	// FAILS if the backpack is full, or if that spot isn't clear. Returns false in that case.
	virtual bool		SetItemBackpackPosition( CEconItemView *pItem, uint32 iPosition = 0, bool bForceUnequip = false, bool bAllowOverflow = false );

	// Sort the backpack items by the specified type
	virtual void		SortBackpackBy( uint32 iSortType );
	void				SortBackpackFinished( void );
	bool				IsInBackpackSort( void ) { return m_bInBackpackSort; }

	void				PredictedBackpackPosFilled( int iBackpackPos );

	// Tell the backend to move an item to a specified backend position
	virtual void		UpdateInventoryPosition( CPlayerInventory *pInventory, uint64 ulItemID, uint32 unNewInventoryPos );

	virtual void		UpdateInventoryEquippedState( CPlayerInventory *pInventory, uint64 ulItemID, equipped_class_t unClass, equipped_slot_t unSlot, bool bSwap = false );

	void				BeginBackpackPositionTransaction();
	void				EndBackpackPositionTransaction();

	//-----------------------------------------------------------------------
	// CLIENT PICKUP UI HANDLING
	//-----------------------------------------------------------------------

	// Get the number of items picked up
	virtual int			GetNumItemPickedUpItems( void ) { return 0; }

	// Show the player a pickup screen with any items they've collected recently, if any
	virtual bool		ShowItemsPickedUp( bool bForce = false, bool bReturnToGame = true, bool bNoPanel = false );

	// Show the player a pickup screen with the items they've crafted
	virtual void		ShowItemsCrafted( CUtlVector<itemid_t> *vecCraftedIndices ) { return; }

	// Force the player to discard an item to make room for a new item, if they have one.
	// Returns true if the discard panel has been brought up, and the player will be forced to discard an item.
	virtual bool		CheckForRoomAndForceDiscard( void );
	
	//-----------------------------------------------------------------------
	// CLIENT ITEM PICKUP ACKNOWLEDGEMENT FILES
	//
	// This system avoids showing multiple pickups for items that we've found, but haven't been 
	// able to move out of unack'd position due to the GC being unavailable. We keep a list of
	// items we've ack'd in a client file, and don't re-show pickups for them. When a GC item
	// update tells us the item has moved out of the unack'd position, we remove it from our file.
	//-----------------------------------------------------------------------

	bool				HasBeenAckedByClient( CEconItemView *pItem );		// Returns true if it's in our client file
	void				SetAckedByClient( CEconItemView *pItem );			// Adds it to our client file
	void				SetAckedByGC( CEconItemView *pItem, bool bSave );	// Removes it from our client file

private:
	CUtlRBTree< itemid_t, int32, CDefLess< itemid_t > > m_rbItemIdsClientAck;

	// As we move items around in batches (on pickups usually) we need to know what slots will be filled
	// by items we've moved, and haven't received a response from Steam.
	typedef CUtlMap< uint32, uint64, int > tPredictedSlots;
	tPredictedSlots			m_mapPredictedFilledSlots;

public:
	// TODO: Does this belong here?
	bool BGetPlayerQuestIdPointsRemaining( uint16 unQuestID, uint32 &numPointsRemaining, uint32 &numPointsUncommitted );

public:
	CEconItemView *CreateReferenceEconItem( item_definition_index_t iDefIndex, int iPaintIndex, uint8 ub1 = 0 );
	void RemoveReferenceEconItem( item_definition_index_t iDefIndex, int iPaintIndex, uint8 ub1 );
	CEconItemView *FindReferenceEconItem( item_definition_index_t iDefIndex, int iPaintIndex, uint8 ub1 );
	CEconItemView *FindOrCreateReferenceEconItem( item_definition_index_t iDefIndex, int iPaintIndex, uint8 ub1 = 0 );
	CEconItemView *FindOrCreateReferenceEconItem( itemid_t ullFauxItemId );

	CUtlVector< CEconItemView* >	m_pTempReferenceItems;		// A collection of econitemviews that are created temporarily just to pull inventory data from.
#endif //CLIENT_DLL
	
public:
	virtual int			GetBackpackPositionFromBackend( uint32 iBackendPosition ) { return ExtractBackpackPositionFromBackend(iBackendPosition); }

protected:
	//-----------------------------------------------------------------------
	// Inventory registry
	void DeregisterInventory( CPlayerInventory *pInventory );
	struct inventories_t
	{
		CPlayerInventory			*pInventory;
		IInventoryUpdateListener	*pListener;
	};
	CUtlVector<inventories_t>	m_pInventories;

	friend class CPlayerInventory;

	inline bool			IsValidPlayerClass( equipped_class_t unClass );
	inline bool			IsValidSlot( equipped_slot_t unSlot );

#ifdef CLIENT_DLL
	// Keep track of the number of items we've tried to discard, but haven't recieved responses on
	int			m_iPredictedDiscards;

	typedef CUtlMap< uint32, CUtlString, int > tPersonaNamesByAccountID;
	tPersonaNamesByAccountID m_mapPersonaNamesCache;

	bool		m_bInBackpackSort;

	// Allows transaction item updating when moving many items
	CMsgSetItemPositions m_itemPositions;
	bool m_bIsBatchingPositions;

private:
	CON_COMMAND_MEMBER_F( CInventoryManager, "econ_clear_inventory_images", ClearLocalInventoryImages, "clear the local inventory images (they will regenerate)", FCVAR_CLIENTDLL );

#endif //CLIENT_DLL
};

//=================================================================================
// Implement these functions in your game code to create custom derived versions
CInventoryManager *InventoryManager( void );

#ifdef CLIENT_DLL

// Inventory Less function.
// Used to sort the inventory items into their positions.
class CManagedItemViewsLess
{
public:
	bool Less( CEconItemView * const & src1, CEconItemView * const & src2, void *pCtx )
	{
		uint64 unIndex1 = src1->GetItemID();
		uint64 unIndex2 = src2->GetItemID();

		if ( unIndex1 == unIndex2 )
		{
			return EconRarity_CombinedItemAndPaintRarity( src1->GetStaticData()->GetRarity(), src1->GetCustomPaintKitIndex() ) >
				EconRarity_CombinedItemAndPaintRarity( src2->GetStaticData()->GetRarity(), src2->GetCustomPaintKitIndex() );
		}

		return unIndex1 == 0ull || ( unIndex1 < unIndex2 );
	}
};

class CInventoryItemUpdateManager: public CAutoGameSystemPerFrame
{
public:

	virtual void Update( float frametime );
	
	void AddItemView( CEconItemView *pItemView )
	{
		FOR_EACH_VEC( m_ManagedItems, i )
		{
			if ( m_ManagedItems[i] == pItemView )
				return;
		}

		m_ManagedItems.Insert( pItemView );
	}

	void RemoveItemView( CEconItemView *pItemView )
	{
		m_ManagedItems.FindAndRemove( pItemView );
	}

	void AddItemViewToFixupList( CEconItemView *pItemView )
	{
		m_fixupListMutex.Lock();
		m_fixupList.AddToHead( pItemView );
		m_fixupListMutex.Unlock();
	}

	void RemoveItemViewFromFixupList( CEconItemView *pItemView )
	{
		m_fixupListMutex.Lock();
		m_fixupList.FindAndRemove( pItemView );
		m_fixupListMutex.Unlock();
	}

private:

	CUtlSortVector< CEconItemView*, CManagedItemViewsLess > m_ManagedItems;

	CUtlVector< CEconItemView* > m_fixupList;
	CThreadFastMutex			 m_fixupListMutex;
};

extern CInventoryItemUpdateManager g_InventoryItemUpdateManager;

#endif

#endif // ITEM_INVENTORY_H
