//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "cstrike15_item_system.h"
#include "cstrike15_item_inventory.h"
#include "econ_item_inventory.h"

//-----------------------------------------------------------------------------
// Purpose: Generate the base item for a class's loadout slot 
//-----------------------------------------------------------------------------
int CCStrike15ItemSystem::GenerateBaseItem( baseitemcriteria_t *pCriteria )
{
	Assert( ( pCriteria->iClass != 0 ) || ( pCriteria->iSlot == LOADOUT_POSITION_MUSICKIT ) );
	Assert( pCriteria->iSlot != LOADOUT_POSITION_INVALID );

	// Some slots don't have base items (i.e. were added after launch)
	if ( !SlotContainsBaseItems( pCriteria->iSlot ) )
		return INVALID_ITEM_INDEX;

	CItemSelectionCriteria criteria;
	criteria.SetQuality( AE_NORMAL );
	criteria.SetRarity( 1 );
	criteria.SetItemLevel( 1 );
	criteria.BAddCondition( "baseitem", k_EOperator_String_EQ, "1", true );
	criteria.BAddCondition( "default_slot_item", k_EOperator_String_EQ, "1", true );
	criteria.SetExplicitQualityMatch( true );
	CSInventoryManager()->AddBaseItemCriteria( pCriteria, &criteria );
	int iChosenItem = GenerateRandomItem( &criteria, NULL );
	return iChosenItem;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CCStrike15ItemSystem *CStrike15ItemSystem( void )
{
	CEconItemSystem *pItemSystem = ItemSystem();
	Assert( dynamic_cast<CCStrike15ItemSystem *>( pItemSystem ) != NULL );
	return (CCStrike15ItemSystem *)pItemSystem;
}