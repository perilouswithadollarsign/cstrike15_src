//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SFOVERWATCHRESOLUTIONPANEL_H
#define SFOVERWATCHRESOLUTIONPANEL_H
#ifdef _WIN32
#pragma once
#endif //_WIN32

#include "scaleformui/scaleformui.h"
#include "GameEventListener.h"
#include "game/client/iviewport.h"
#include "matchmaking/imatchframework.h"

class SFHudOverwatchResolutionPanel : public ScaleformFlashInterface, public IMatchEventsSink
{
protected:
	static SFHudOverwatchResolutionPanel *m_pInstance;

	SFHudOverwatchResolutionPanel();
	~SFHudOverwatchResolutionPanel();

	//
	// IMatchEventsSink
	//
public:
	virtual void OnEvent( KeyValues *pEvent ) OVERRIDE;

public:
	static void LoadDialog( void );
	static void UnloadDialog( void );
	static SFHudOverwatchResolutionPanel * GetInstance() { return m_pInstance; }

	virtual void FlashReady( void );
	virtual void PostUnloadFlash( void );

	void HideFromScript( SCALEFORM_CALLBACK_ARGS_DECL );

	void Show( void );
	void Hide( void );
};

#endif // SFOVERWATCHRESOLUTIONPANEL_H
