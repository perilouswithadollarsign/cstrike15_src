//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef KEYREPEAT_H
#define KEYREPEAT_H

#ifdef _WIN32
#pragma once
#endif

#include "inputsystem/buttoncode.h"

enum KEYREPEAT_ALIASES
{
	KR_ALIAS_UP,
	KR_ALIAS_DOWN,
	KR_ALIAS_LEFT,
	KR_ALIAS_RIGHT,

	FM_NUM_KEYREPEAT_ALIASES,
};

class CKeyRepeatHandler
{
public:
	CKeyRepeatHandler();

	void			Reset();
	void			KeyDown( ButtonCode_t code );
	void			KeyUp( ButtonCode_t code );
	ButtonCode_t	KeyRepeated();
	void			SetKeyRepeatTime( ButtonCode_t code, float flRepeat );

private:
	bool			m_bAliasDown[MAX_JOYSTICKS][FM_NUM_KEYREPEAT_ALIASES];
	float			m_flRepeatTimes[FM_NUM_KEYREPEAT_ALIASES];
	float			m_flNextKeyRepeat[MAX_JOYSTICKS];
	bool			m_bHaveKeyDown;
};


#endif // KEYREPEAT_H
