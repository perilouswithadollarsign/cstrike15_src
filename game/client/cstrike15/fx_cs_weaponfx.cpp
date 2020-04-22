//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//

#include "cbase.h"
#include "fx_impact.h"
#include "tempent.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Handle weapon effect callbacks
//-----------------------------------------------------------------------------
void CStrike_EjectBrass( int shell, const CEffectData &data )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if( !pPlayer )
		return;

	DevWarning( "Unhandled CStrike EjectBrass effect\n" );
}

void CStrike_FX_EjectBrass_9mm_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_9MM, data );
}

void CStrike_FX_EjectBrass_57_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_57, data );
}

void CStrike_FX_EjectBrass_12Gauge_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_12GAUGE, data );
}

void CStrike_FX_EjectBrass_556_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_556, data );
}

void CStrike_FX_EjectBrass_762Nato_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_762NATO, data );
}

void CStrike_FX_EjectBrass_338Mag_Callback( const CEffectData &data )
{
	CStrike_EjectBrass( CS_SHELL_338MAG, data );
}

DECLARE_CLIENT_EFFECT( EjectBrass_9mm,		CStrike_FX_EjectBrass_9mm_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_12Gauge,	CStrike_FX_EjectBrass_12Gauge_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_57,		CStrike_FX_EjectBrass_57_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_556,		CStrike_FX_EjectBrass_556_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_762Nato,	CStrike_FX_EjectBrass_762Nato_Callback );
DECLARE_CLIENT_EFFECT( EjectBrass_338Mag,	CStrike_FX_EjectBrass_338Mag_Callback );
