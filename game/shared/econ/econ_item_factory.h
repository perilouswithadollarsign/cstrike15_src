//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: EconItemFactory: Manages rolling for items requested by the game server
//
//=============================================================================

#ifndef ECONITEMFACTORY_H
#define ECONITEMFACTORY_H
#ifdef _WIN32
#pragma once
#endif

class CEconItem;
class CEconGameAccount;

namespace GCSDK
{
	class CGCSharedObjectCache;
}

#include "game_item_schema.h"

//-----------------------------------------------------------------------------
// CEconItemFactory
// Factory responsible for rolling random items
//-----------------------------------------------------------------------------
class CEconItemFactory
{
public:
	CEconItemFactory( );

	enum ECreateItemPolicy_t
	{
		k_ECreateItemPolicy_Default = 0,				// Default creating item policy
		k_ECreateItemPolicy_NoSqlItemID = ( 1 << 0 ),	// Item will have no ItemID generated
	};
	
	// Gets a pointer to the underlying item schema the factory is using
	GameItemSchema_t &GetSchema() { return m_schema; }

	// Create a random item based on the incoming item selection criteria
	CEconItem *CreateRandomItem( const CEconGameAccount *pGameAccount, const CItemSelectionCriteria &criteria );

	// Create an item from a specific definition index
	CEconItem *CreateSpecificItem( const CEconGameAccount *pGameAccount, item_definition_index_t unDefinitionIndex, ECreateItemPolicy_t eCreateItemPolicy = k_ECreateItemPolicy_Default );

	CEconItem *CreateSpecificItem( GCSDK::CGCSharedObjectCache *pUserSOCache, item_definition_index_t unDefinitionIndex )
	{
		return CreateSpecificItem( pUserSOCache->GetSingleton<CEconGameAccount>(), unDefinitionIndex );
	}

	itemid_t GetNextID();

	bool BYieldingInit();
	bool BIsInitialized() { return m_bIsInitialized; }

	void							 ApplyStaticAttributeToItem( CEconItem *pItem, const static_attrib_t& staticAttrib, const CEconGameAccount *pGameAccount, const CCopyableUtlVector< uint32 > *m_vecValues = NULL, float flRangeMin = 0.0f, float flRangeMax = 0.0f ) const;

private:
	// Functions to create random items
	const CEconItemDefinition		*RollItemDefinition( const CItemSelectionCriteria &criteria ) const;
	void							 AddGCGeneratedAttributesToItem( const CEconGameAccount *pGameAccount, CEconItem *pItem ) const;

private:

	// The schema this factory uses to create items
	GameItemSchema_t m_schema;

	// the next item ID to give out
	itemid_t m_ulNextObjID;
	bool m_bIsInitialized;
};

#endif //ECONITEMFACTORY_H
