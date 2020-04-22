//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
//  hud_msg.cpp
//
#include "cbase.h"
#include "clientmode.h"
#include "hudelement.h"
#include "keyvalues.h"
#include "vgui_controls/AnimationController.h"
#include "engine/IEngineSound.h"
#include <bitbuf.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/// USER-DEFINED SERVER MESSAGE HANDLERS

bool CHud::MsgFunc_ResetHUD( const CCSUsrMsg_ResetHud& msg )
{
	ResetHUD();
	return true;
}

void CHud::ResetHUD()
{
	// clear all hud data
	GetClientMode()->GetViewportAnimationController()->CancelAllAnimations();

	for ( int i = 0; i < GetHudList().Count(); i++ )
	{
#if defined ( PORTAL2 )
		// 78342: We don't want to clear hud chat every spawn as our 
		// MP re-spawns players every death... losing chat is a big deal in p2 coop.
		if ( V_strcmp( GetHudList()[i]->GetName(), "CHudChat" ) == 0 )
			continue;
#endif
		GetHudList()[i]->Reset();
	}

	GetClientMode()->GetViewportAnimationController()->RunAllAnimationsToCompletion();

	// reset sensitivity
	m_flMouseSensitivity = 0;
	m_flMouseSensitivityFactor = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

bool CHud::MsgFunc_SendAudio(const CCSUsrMsg_SendAudio& msg )
{
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, msg.radio_sound().c_str() );
	return true;
}
