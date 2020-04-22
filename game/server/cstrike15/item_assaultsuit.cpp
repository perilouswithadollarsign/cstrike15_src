//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "items.h"
#include "cs_player.h"


class CItemAssaultSuit : public CItem
{
	void Spawn( void )
	{ 
		Precache( );
		CItem::Spawn( );
	}
	
	void Precache( void )
	{
		PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
	}
	
	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			Assert( false );
			return false;
		}

		pPlayer->m_bHasHelmet = true;
		pPlayer->SetArmorValue( 100 );
		pPlayer->RecalculateCurrentEquipmentValue();

		if ( pPlayer->IsAlive() )
		{
			CBroadcastRecipientFilter filter;

			if (pPlayer->GetTeamNumber() == TEAM_CT)
			{
				// Play the CT Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_CT");

			}
			else
			{
				// Play the T Suit sound
				EmitSound(filter, entindex(), "Player.EquipArmor_T");
			}

			//EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
		}

		return true;		
	}
};

LINK_ENTITY_TO_CLASS( item_assaultsuit, CItemAssaultSuit );

class CItemHeavyAssaultSuit : public CItemAssaultSuit
{
// 	void Spawn( void )
// 	{
// 		Precache();
// 		CItem::Spawn();
// 	}

// 	void Precache( void )
// 	{
// 		PrecacheScriptSound( "BaseCombatCharacter.ItemPickup2" );
// 	}

	bool MyTouch( CBasePlayer *pBasePlayer )
	{
		CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pBasePlayer );
		if ( !pPlayer )
		{
			Assert( false );
			return false;
		}

		pPlayer->m_bHasHelmet = true;
		pPlayer->SetArmorValue( 200 );
		pPlayer->RecalculateCurrentEquipmentValue();
		pPlayer->m_bHasHeavyArmor = true;

		if ( pPlayer->IsAlive() )
		{
			CBroadcastRecipientFilter filter;

			if (pPlayer->GetTeamNumber() == TEAM_CT)
			{
				// Play the CT Suit sound
				pPlayer->EmitSound("Player.EquipArmor_CT");

			}
			else
			{
				// Play the T Suit sound
				pPlayer->EmitSound("Player.EquipArmor_T");
			}
			//EmitSound( filter, entindex(), "BaseCombatCharacter.ItemPickup2" );
		}

		return true;
	}
};

LINK_ENTITY_TO_CLASS( item_heavyassaultsuit, CItemHeavyAssaultSuit );



