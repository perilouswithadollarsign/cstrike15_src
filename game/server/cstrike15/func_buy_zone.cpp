//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "triggers.h"
#include "cs_player.h"
#include "cs_team.h"


//======================================
// Buy zone
//
//

extern ConVar mp_logdetail;

class CBuyZone : public CBaseTrigger
{
public:
	DECLARE_CLASS( CBuyZone, CBaseTrigger );
	DECLARE_DATADESC();

	CBuyZone();
	void Spawn();
	void EXPORT BuyZoneTouch( CBaseEntity* pOther );

	void InputSetTeam_TerroristOnly( inputdata_t& inputdata );
	void InputSetTeam_CTOnly( inputdata_t& inputdata );
	void InputSetTeam_AllTeams( inputdata_t& inputdata );
	void InputSetTeam_None( inputdata_t& inputdata );

	// CBaseTrigger override
	virtual void EndTouch( CBaseEntity *pOther ) OVERRIDE;

public:
	int m_LegacyTeamNum;
};


LINK_ENTITY_TO_CLASS( func_buyzone, CBuyZone );

BEGIN_DATADESC( CBuyZone )
	DEFINE_FUNCTION( BuyZoneTouch ),
	
	// This is here to support maps that haven't updated to using "teamnum" yet.
	DEFINE_INPUT( m_LegacyTeamNum, FIELD_INTEGER, "team" ),

	DEFINE_INPUTFUNC( FIELD_VOID,				"SetTeam_TerroristOnly",	InputSetTeam_TerroristOnly ),
	DEFINE_INPUTFUNC( FIELD_VOID,				"SetTeam_CTOnly",	InputSetTeam_CTOnly ),
	DEFINE_INPUTFUNC( FIELD_VOID,				"SetTeam_AllTeams",	InputSetTeam_AllTeams ),
	DEFINE_INPUTFUNC( FIELD_VOID,				"SetTeam_None",	InputSetTeam_None )
END_DATADESC()


CBuyZone::CBuyZone()
{
	m_LegacyTeamNum = -1;
}


void CBuyZone::Spawn()
{
	InitTrigger();
	SetTouch( &CBuyZone::BuyZoneTouch );

	// Support for legacy-style teamnums.
	if ( m_LegacyTeamNum == 1 )
	{
		ChangeTeam( TEAM_TERRORIST );
	}
	else if ( m_LegacyTeamNum == 2 )
	{
		ChangeTeam( TEAM_CT );
	}
}

	
void CBuyZone::BuyZoneTouch( CBaseEntity* pOther )
{
	CCSPlayer *p = dynamic_cast< CCSPlayer* >( pOther );
	if ( p )
	{
		int nZoneTeam = GetTeamNumber();
		if ( nZoneTeam == -1 )
		{
			// -1 mean no team can use it
			return;
		}
		// compare player team with buy zone team number
		else if ( nZoneTeam == 0 || p->GetTeamNumber() == GetTeamNumber() )
		{
			p->m_bInBuyZone = true;
			p->AutoBuyAmmo();
		}
	}
}


void CBuyZone::InputSetTeam_TerroristOnly( inputdata_t& inputdata )
{
	ChangeTeam( TEAM_TERRORIST );
}

void CBuyZone::InputSetTeam_CTOnly( inputdata_t& inputdata )
{
	ChangeTeam( TEAM_CT );
}

void CBuyZone::InputSetTeam_AllTeams( inputdata_t& inputdata )
{
	// team 0 in this case means that everyone can use it
	ChangeTeam( 0 );
}

void CBuyZone::InputSetTeam_None( inputdata_t& inputdata )
{
	// team -1 means that no one can use it
	ChangeTeam( -1 );
}

void CBuyZone::EndTouch( CBaseEntity *pOther )
{
	BaseClass::EndTouch( pOther );

	// Feature mostly for tournament organizers to aid match stat log parsing. 
	if ( mp_logdetail.GetInt() >= 3 && pOther->IsPlayer() )
	{
		if ( CCSPlayer *pCSPlayer = ToCSPlayer( pOther ) )
		{
			CUtlString strEquipment( "[ ");

			for ( int i = 0; i < MAX_WEAPONS; ++i )
			{
				if ( CBaseCombatWeapon* pWeapon = pCSPlayer->GetWeapon( i ) )
					strEquipment.Append( CFmtStr( "%s ", pWeapon->GetEconItemView() ? pWeapon->GetEconItemView()->GetItemDefinition()->GetDefinitionName() : pWeapon->GetName() ).Get() );
			}

			if ( pCSPlayer->HasDefuser() )
				strEquipment.Append( "defuser " );

			if ( pCSPlayer->ArmorValue() > 0 )
				strEquipment.Append( CFmtStr( "kevlar(%d) ", pCSPlayer->ArmorValue() ).Get() );

			if ( pCSPlayer->m_bHasHelmet )
				strEquipment.Append( "helmet " );

			if ( pCSPlayer->HasC4() )
				strEquipment.Append( "C4 " );

			strEquipment.Append( "]" );

			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" left buyzone with %s\n",
							pCSPlayer->GetPlayerName(),
							pCSPlayer->GetUserID(),
							pCSPlayer->GetNetworkIDString(),
							pCSPlayer->GetTeam()->GetName(),
							strEquipment.Get() );
		}
	}
}
