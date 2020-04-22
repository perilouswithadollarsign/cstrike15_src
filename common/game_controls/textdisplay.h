//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================

#ifndef TEXTDISPLAY_H
#define TEXTDISPLAY_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"

// THIS CLASS IS A CHEAT AND SHOULD NOT REALLY BE
// USED OUTSIDE OF GAME_CONTROLS LIB, BUT IT IS

class CGameText;
class IGameUISystem;
class CGameUISystem;


//-----------------------------------------------------------------------------
// A class to display generic text on screen using the GameUI.
// Static text hangs out on the right hand side, while messages hang out on the left side.
// Not meant for real UI's, useful for spewing debugging info though!
//-----------------------------------------------------------------------------
class CTextDisplay
{
public:

	CTextDisplay();
	void Init( IGameUISystem *pMenu );
	
	// These hang around until you call ClearStaticText 
	// Use this fxn if you want this class to just use in the next row below (like a list )
	void AddStaticText( const char* pFmt, ... );
	// Use this fxn if you want to supply a pixel position
	void AddStaticText( int xPos, int yPos, const char* pFmt, ... );

	// These get wiped every frame by calling Finish Frame, meant to be erased every frame loop.
	// Good for when you are updating the info every frame.
	// Use this fxn if you want this class to just use in the next row below (like a list )
	void PrintMsg( const char* pFmt, ... );
	// Use this fxn if you want to supply a pixel position
	void PrintMsg( int xPos, int yPos, const char* pFmt, ... );

	void ClearStaticText();
	void FinishFrame();
	void Shutdown();

private:
	CUtlVector< CGameText * > m_pStaticText;
	CUtlVector< CGameText * > m_pStatsText;
	bool m_bIsInitialized;
	int m_nXStaticOffset;
	int m_nYStaticOffset;

	int m_nXOffset;
	int m_nYOffset;

	CGameUISystem *m_pMenu;
};

#endif // TEXTDISPLAY_H

