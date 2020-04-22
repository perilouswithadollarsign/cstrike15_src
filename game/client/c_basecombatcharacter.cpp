//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's C_BaseCombatCharacter entity
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_basecombatcharacter.h"
#include "c_cs_player.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( CBaseCombatCharacter )
#undef CBaseCombatCharacter	
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseCombatCharacter::C_BaseCombatCharacter()
{
	for ( int i=0; i < m_iAmmo.Count(); i++ )
		m_iAmmo.Set( i, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseCombatCharacter::~C_BaseCombatCharacter()
{
}

/*
//-----------------------------------------------------------------------------
// Purpose: Returns the amount of ammunition of the specified type the character's carrying
//-----------------------------------------------------------------------------
int	C_BaseCombatCharacter::GetAmmoCount( char *szName ) const
{
	return GetAmmoCount( g_pGameRules->GetAmmoDef()->Index(szName) );
}
*/

//-----------------------------------------------------------------------------
// Purpose: Overload our muzzle flash and send it to any actively held weapon
//-----------------------------------------------------------------------------
void C_BaseCombatCharacter::DoMuzzleFlash()
{
	// Our weapon takes our muzzle flash command
	C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->DoMuzzleFlash();
		//NOTENOTE: We do not chain to the base here
	}
	else
	{
		BaseClass::DoMuzzleFlash();
	}
}

void C_BaseCombatCharacter::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	// view weapon model cache monitoring
	// NOTE: expected to be updated ONLY once per frame for the primary player ONLY!
	// the expectation is that there is ONLY one customer that requires view models
	// otherwise the lower level code will thrash as it tries to maintain a single player's view model inventory
	{
		const char *viewWeapons[MAX_WEAPONS];
		int nNumViewWeapons = 0;

		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer == this && !pLocalPlayer->IsObserver() )
		{
			// want to know what this player's weapon inventory is to keep all of these in cache
			for ( int i = 0; i < MAX_WEAPONS; i++ )
			{
				C_BaseCombatWeapon *pWeapon = GetWeapon( i );
				if ( !pWeapon )
					continue;

				viewWeapons[nNumViewWeapons] = pWeapon->GetViewModel();
				nNumViewWeapons++;
			}
		}
		else if ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pLocalPlayer->GetObserverTarget() == this ) 
		{
			// once spectating, PURPOSELY only the active view weapon gets tracked
			// cycling through spectators is the more common pattern and tracking just the active weapon prevents massive cache thrashing 
			// otherwise maintaining the observer targets inventories would needlessly thrash the cache as the player rapidly cycles
			C_BaseCombatWeapon *pWeapon = pLocalPlayer->GetActiveWeapon();
			if ( pWeapon )
			{
				viewWeapons[nNumViewWeapons] = pWeapon->GetViewModel();
				nNumViewWeapons++;
			}
		}

		if ( nNumViewWeapons )
		{
			// view model weapons are subject to a cache policy that needs to be kept accurate for a SINGLE Player
			modelinfo->UpdateViewWeaponModelCache( viewWeapons, nNumViewWeapons );
		}
	}

	// world weapon model cache monitoring
	// world weapons have a much looser cache policy and just needs to monitor the important set of world weapons
	{
		const char *worldWeapons[MAX_WEAPONS];
		int nNumWorldWeapons = 0;

		// want to track any world models that are active weapons
		// the world weapons lying on the ground are the ones that become LRU purge candidates
		C_BasePlayer *pPlayer = ToBasePlayer( this );
		if ( pPlayer )
		{
			C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
			if ( pWeapon )
			{
				worldWeapons[nNumWorldWeapons] = pWeapon->GetWorldModel();
				nNumWorldWeapons++;
			}
		}

		C_CSPlayer *pCSPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pCSPlayer == this )
		{
			int weaponEntIndex = pCSPlayer->GetTargetedWeapon();
			if ( weaponEntIndex > 0 ) //0 is a valid entity index, but will never be used for a weapon
			{			
				C_BaseEntity *pEnt = cl_entitylist->GetEnt( weaponEntIndex );
				C_BaseCombatWeapon *pWeapon = dynamic_cast< C_BaseCombatWeapon * >( pEnt );
				if ( pWeapon )
				{
					worldWeapons[nNumWorldWeapons] = pWeapon->GetWorldModel();
					nNumWorldWeapons++;
				}
			}
		}

		if ( nNumWorldWeapons )
		{
			modelinfo->TouchWorldWeaponModelCache( worldWeapons, nNumWorldWeapons );
		}
	}
}

bool C_BaseCombatCharacter::HasEverBeenInjured( void ) const
{
	return ( m_flTimeOfLastInjury != 0.0f );
}

float C_BaseCombatCharacter::GetTimeSinceLastInjury( void ) const
{
	return gpGlobals->curtime - m_flTimeOfLastInjury;
}

IMPLEMENT_CLIENTCLASS(C_BaseCombatCharacter, DT_BaseCombatCharacter, CBaseCombatCharacter);

// Only send active weapon index to local player
BEGIN_RECV_TABLE_NOBASE( C_BaseCombatCharacter, DT_BCCLocalPlayerExclusive )
	RecvPropTime( RECVINFO( m_flNextAttack ) ),
END_RECV_TABLE();

BEGIN_RECV_TABLE_NOBASE( C_BaseCombatCharacter, DT_BCCNonLocalPlayerExclusive )
#if defined( CSTRIKE15 )
	// In CS:GO send active weapon index to all players except local
	RecvPropArray3( RECVINFO_ARRAY(m_hMyWeapons), RecvPropEHandle( RECVINFO( m_hMyWeapons[0] ) ) ),
#endif
END_RECV_TABLE();

BEGIN_RECV_TABLE(C_BaseCombatCharacter, DT_BaseCombatCharacter)
	RecvPropDataTable( "bcc_localdata", 0, 0, &REFERENCE_RECV_TABLE(DT_BCCLocalPlayerExclusive) ),
	RecvPropDataTable( "bcc_nonlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_BCCNonLocalPlayerExclusive) ),
	RecvPropInt( RECVINFO( m_LastHitGroup ) ),
	RecvPropEHandle( RECVINFO( m_hActiveWeapon ) ),
	RecvPropTime( RECVINFO( m_flTimeOfLastInjury ) ),
	RecvPropInt( RECVINFO( m_nRelativeDirectionOfLastInjury ) ),
	RecvPropArray3( RECVINFO_ARRAY(m_hMyWeapons), RecvPropEHandle( RECVINFO( m_hMyWeapons[0] ) ) ),

#ifdef INVASION_CLIENT_DLL
	RecvPropInt( RECVINFO( m_iPowerups ) ),
#endif

END_RECV_TABLE()


BEGIN_PREDICTION_DATA( C_BaseCombatCharacter )

	DEFINE_PRED_ARRAY( m_iAmmo, FIELD_INTEGER,  MAX_AMMO_TYPES, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flNextAttack, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hActiveWeapon, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_ARRAY( m_hMyWeapons, FIELD_EHANDLE, MAX_WEAPONS, FTYPEDESC_INSENDTABLE ),

END_PREDICTION_DATA()
