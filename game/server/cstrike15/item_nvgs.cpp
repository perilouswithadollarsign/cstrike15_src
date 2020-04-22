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


class CItemNvgs : public CItem
{
public:

	DECLARE_CLASS( CItemNvgs, CItem );

	void Spawn( void )
	{ 
		Precache( );
		BaseClass::Spawn( );
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

		pPlayer->m_bHasNightVision = true;

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

LINK_ENTITY_TO_CLASS( item_nvgs, CItemNvgs );


