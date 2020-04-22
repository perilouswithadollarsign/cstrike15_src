//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side C_CSTeam class
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "engine/IEngineSound.h"
#include "hud.h"
#include "recvproxy.h"
#include "c_cs_team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


IMPLEMENT_CLIENTCLASS_DT(C_CSTeam, DT_CSTeam, CCSTeam)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CSTeam::C_CSTeam()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CSTeam::~C_CSTeam()
{
}

const char* Helper_GetLocalPlayerAssassinationQuestLocToken( const CEconQuestDefinition *pQuest )
{
	if ( !pQuest )
		return NULL;

	KeyValues *pTargetKV = pQuest->GetStringTokens()->FindKey( "target" );
	return pTargetKV ? pTargetKV->GetString() : NULL;
}

bool Helper_GetDecoratedAssassinationTargetName( const CEconQuestDefinition *pQuest, wchar_t* pszBuffer, size_t nBuffSizeInCharacters )
{
	const char* szToken = Helper_GetLocalPlayerAssassinationQuestLocToken( pQuest );
	if ( wchar_t *wszUndecoratedName = g_pVGuiLocalize->Find( szToken ) )
	{
		V_snwprintf( pszBuffer, nBuffSizeInCharacters, L"<font color = '#FF0000'><i>" PRI_WS_FOR_WS L"</i></font>", wszUndecoratedName );
		return true;
	}
	return false;
}
