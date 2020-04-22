//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#include "cbase.h"
#include "networkstringtabledefs.h"
#include "gcsdk/protobufsharedobject.h"
#include "cs_econ_item_string_table.h"
#include "econ_item_view.h"
#include "econ_item_inventory.h"

#ifdef CLIENT_DLL
	#include "networkstringtable_clientdll.h"
#else
	#include "networkstringtable_gamedll.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
ConVar cs_econ_item_string_table_debug( "cs_econ_item_string_table_debug", "0", FCVAR_DEVELOPMENTONLY | FCVAR_CLIENTDLL);
#endif

CUtlMap< itemid_t, CEconItem* > g_EconItemMap( 0, 0, DefLessFunc(itemid_t) );

INetworkStringTable *g_pStringTableEconItems = NULL;

CStrikeEconItemIndex_t InvalidEconItemStringIndex()
{
	return PLAYER_ECON_ITEMS_INVALID_STRING;
}

void CreateEconItemStringTable( void )
{
#ifdef CLIENT_DLL
	INetworkStringTable *pNewStringTable = networkstringtable->FindTable( "EconItems" );
	// This Create is called twice, once before the callbacks and once after. Only clear the map the first time so it doesn't
	// get stomped.
	if ( pNewStringTable != g_pStringTableEconItems )
	{
		g_EconItemMap.PurgeAndDeleteElements();
		g_pStringTableEconItems = pNewStringTable;
		g_pStringTableEconItems->SetStringChangedCallback( NULL, OnStringTableEconItemsChanged );
	}	

#else // GAME_DLL
	g_pStringTableEconItems = networkstringtable->CreateStringTable( "EconItems", MAX_PLAYER_ECON_ITEMS_STRINGS, 0, 0, NSF_DICTIONARY_ENABLED );
#endif
}

CEconItem* GetEconItemFromStringTable( itemid_t itemID )
{
	int nIndex = g_EconItemMap.Find( itemID );
	if ( nIndex == g_EconItemMap.InvalidIndex() )
		return NULL;

	return g_EconItemMap.Element( nIndex );
}

#ifdef CLIENT_DLL

void OnStringTableEconItemsChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, void const *newData )
{
	// is it new?
	CStrikeEconItemIndex_t nIndex = g_EconItemMap.Find( stringNumber );
	if ( nIndex == g_EconItemMap.InvalidIndex() )
	{
		int dataLength = 0;
		const void *pEconItemData = g_pStringTableEconItems->GetStringUserData( stringNumber, &dataLength );
		if ( pEconItemData )
		{
			CSOEconItem msgItem;
			if ( !msgItem.ParseFromArray( pEconItemData, dataLength ) )
				return;

			CEconItem *pItem = (CEconItem *)GCSDK::CSharedObject::Create( CEconItem::k_nTypeID );
			if( !pItem )
				return;

			pItem->DeserializeFromProtoBufItem( msgItem );

			g_EconItemMap.InsertOrReplace( pItem->GetItemID(), pItem );

			// Add the item to the appropriate user's inventory.
			// Must be done after the econ item map is updated because this will use it.
			CPlayerInventory *pLocalInventory = InventoryManager()->GetLocalInventory();
			CPlayerInventory *pInventory = InventoryManager()->GetInventoryForAccount( pItem->GetAccountID() );
			if ( pInventory && (pInventory != pLocalInventory) )
			{
				pInventory->AddEconItem( pItem, false, false, false );
			}
		}		
	}
	else
	{
		// Add support for updating existing items.
		Assert( false );
		/*
		// update it
		CEconItemView *pItemView = g_EconItemMap.Element( nIndex );
		Assert( pItemView );
		if ( pItemView )
		{
			// todo re-init the item view
		}
		*/
	}
}

void RepopulateInventory( CPlayerInventory *pInventory, uint32 iAccountID )
{
	if ( pInventory->GetItemCount() > 0 )
		return;

	FOR_EACH_MAP( g_EconItemMap, i )
	{
		CEconItem* pItem = g_EconItemMap[i];
		if ( !pItem )
			continue;

		if ( pItem->GetAccountID() == iAccountID )
		{
			pInventory->AddEconItem( pItem, false, false, false );
		}
	}
}

#endif	// CLIENT_DLL

#ifdef GAME_DLL
CStrikeEconItemIndex_t AddEconItemToStringTable( CEconItem* pItem )
{
	CStrikeEconItemIndex_t stringNumber = InvalidEconItemStringIndex();

	if ( !pItem )
		return stringNumber;

	CSOEconItem msgItem;
	pItem->SerializeToProtoBufItem( msgItem );

	int entrySize = msgItem.ByteSize();
	void *serializeBuffer = stackalloc( entrySize );
	if ( !msgItem.SerializeWithCachedSizesToArray( ( google::protobuf::uint8 * )serializeBuffer ) )
	{
		Warning( "Unexpected issue serializing buff!\n" );
	}

	char idString[32];
	Q_snprintf( idString, sizeof( idString ), "%llu", pItem->GetItemID() );
	stringNumber = g_pStringTableEconItems->AddString( true, idString, entrySize, serializeBuffer );
	if ( stringNumber != InvalidEconItemStringIndex() )
	{
		g_EconItemMap.InsertOrReplace( pItem->GetItemID(), pItem );
	}

	return stringNumber;
}
#endif	// GAME_DLL

#ifdef GAME_DLL
CON_COMMAND_F( sv_cs_dump_econ_item_stringtable, "sv_cs_dump_econ_item_stringtable", FCVAR_NONE )
#else
CON_COMMAND_F( cl_cs_dump_econ_item_stringtable, "cl_cs_dump_econ_item_stringtable", FCVAR_NONE )
#endif
{
	if ( !g_pStringTableEconItems )
	{
		return;
	}

	Msg("index,length,id,name,desc\n");

	for ( int i = 0; i < g_pStringTableEconItems->GetNumStrings(); ++i )
	{
		int dataLength = 0;
		const void *pEconItemData = g_pStringTableEconItems->GetStringUserData( i, &dataLength );

		const char *pszString = g_pStringTableEconItems->GetString( i );

		if ( !pEconItemData )
		{
			Msg( "Item Def - %s\n", pszString );
		}
		else
		{
			CSOEconItem msgItem;
			if ( !msgItem.ParseFromArray( pEconItemData, dataLength ) )
			{
				Msg( "%d, Error parsing Econ Item!\n", i );
				continue;
			}

			Msg("%d,%d, (%s), id:%llu\n",
				i,
				dataLength,
				pszString,
				msgItem.id()
				);

			Msg( "custom name: %s\n", msgItem.custom_name().c_str() );
			Msg( "custom desc: %s\n", msgItem.custom_desc().c_str() );
			Msg( "style: %i\n", msgItem.style() );

			for ( int i=0; i<msgItem.attribute_size(); i++ )
			{
				Msg( "attribute %i: %i, %i\n", i, msgItem.attribute(i).def_index(), msgItem.attribute(i).value() );
			}
		}
	}
}