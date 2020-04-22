//======= Copyright ( c ) 1996-2009, Valve Corporation, All rights reserved. ======
//
// Purpose: Definitions that are shared by the game DLL and the client DLL.
//
//===============================================================================

#if defined( INCLUDE_SCALEFORM )

#ifndef SPLITSCREENSIGNON_H
#define SPLITSCREENSIGNON_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"
#include "matchmaking/imatchframework.h"
#include "scaleformui/scaleformui.h"


class SplitScreenSignonWidget : public ScaleformFlashInterfaceMixin<CGameEventListener>, public IMatchEventsSink
{
public:
	SplitScreenSignonWidget();

	void FlashReady( void );
	bool PreUnloadFlash( void );


	void OnShow( void );
	void OnHide( void );

	void Show( bool showit );

	void UpdateState( void );
	void SplitScreenConditionsAreValid( bool value );
	void Update( void );

	void DropSecondPlayer( void );
	void RevertUIToOnePlayerMode( void );

	virtual void FireGameEvent( IGameEvent *event );

	void SetPlayer2Name( const char* name );
	virtual void OnEvent( KeyValues *pEvent );
	void SetPlayerSignedIn( void );

public:
	SFVALUE m_pPlayer2Name;
	int m_iSecondPlayerId;
	int m_iControllerThatPressedStart;
	bool m_bVisible;
	bool m_bLoading;

	bool m_bWantShown;
	bool m_bConditionsAreValid;
	bool m_bWaitingForSignon;
	bool m_bDropSecondPlayer;

	bool m_bCurrentlyProcessingSignin;

};




#endif // SPLITSCREENSIGNON_H

#endif // include scaleform

