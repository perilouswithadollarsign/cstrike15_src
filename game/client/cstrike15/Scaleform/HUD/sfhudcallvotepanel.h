//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SFHUDCALLVOTEPANEL_H
#define SFHUDCALLVOTEPANEL_H
#ifdef _WIN32
#pragma once
#endif //_WIN32

#include "sfhudflashinterface.h"

#define MAX_TARGET_COUNT 10
#define CALLVOTE_NAME_TRUNCATE_AT	16  // number of name character displayed before truncation

class SFHudCallVotePanel: public ScaleformFlashInterface
{
public:
	static SFHudCallVotePanel *m_pInstance;

	SFHudCallVotePanel();	

	static void LoadDialog( void );
	static void UnloadDialog( void );
	
	virtual void LevelShutdown( void );	
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );	
	virtual void PostUnloadFlash( void );

	void PopulatePlayerTargets( SCALEFORM_CALLBACK_ARGS_DECL );
	void PopulateMapTargets( SCALEFORM_CALLBACK_ARGS_DECL );	
	void PopulateBackupFilenames( SCALEFORM_CALLBACK_ARGS_DECL );
	void PopulateBackupFilenames_Callback( const CCSUsrMsg_RoundBackupFilenames &msg );
	void GetNumberOfValidMapsInGroup( SCALEFORM_CALLBACK_ARGS_DECL );	
	void GetNumberOfValidKickTargets( SCALEFORM_CALLBACK_ARGS_DECL );	
	void IsQueuedMatchmaking( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsEndMatchMapVoteEnabled( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsPlayingClassicCompetitive( SCALEFORM_CALLBACK_ARGS_DECL );

	void Show( void );
	void Hide( void );

	
};

#endif // SFHUDCALLVOTEPANEL_H
