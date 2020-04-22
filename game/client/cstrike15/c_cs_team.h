//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side CTFTeam class
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_CS_TEAM_H
#define C_CS_TEAM_H
#ifdef _WIN32
#pragma once
#endif

#include "c_team.h"
#include "shareddefs.h"

class C_BaseEntity;
class C_BaseObject;
class CBaseTechnology;

//-----------------------------------------------------------------------------
// Purpose: TF's Team manager
//-----------------------------------------------------------------------------
class C_CSTeam : public C_Team
{
	DECLARE_CLASS( C_CSTeam, C_Team );
public:
	DECLARE_CLIENTCLASS();

					C_CSTeam();
	virtual			~C_CSTeam();
};

const char* Helper_GetLocalPlayerAssassinationQuestLocToken( const CEconQuestDefinition *pQuest );
bool Helper_GetDecoratedAssassinationTargetName( const CEconQuestDefinition *pQuest, wchar_t* pszBuffer, size_t nBuffSizeInCharacters );

#endif // C_CS_TEAM_H
