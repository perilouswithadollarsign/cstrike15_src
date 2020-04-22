//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_VOICE_H
#define MM_VOICE_H
#ifdef _WIN32
#pragma once
#endif

#include "mm_framework.h"
#include "utlvector.h"

class CMatchVoice : public IMatchVoice
{
	// Methods of IMatchVoice
public:
	// Whether remote player talking can be visualized / audible
	virtual bool CanPlaybackTalker( XUID xuidTalker );

	// Whether we are explicitly muting a remote player
	virtual bool IsTalkerMuted( XUID xuidTalker );

	// Whether we are muting any player on the player's machine
	virtual bool IsMachineMuted( XUID xuidPlayer );

	// Whether voice recording mode is currently active
	virtual bool IsVoiceRecording();

	// Enable or disable voice recording
	virtual void SetVoiceRecording( bool bRecordingEnabled );

	// Enable or disable voice mute for a given talker
	virtual void MuteTalker( XUID xuidTalker, bool bMute );

protected:
	// Remap XUID of a player to a valid LIVE-enabled XUID
	XUID RemapTalkerXuid( XUID xuidTalker );

	// Check player-player voice privileges for machine blocking purposes
	bool IsTalkerMutedWithPrivileges( int iCtrlr, XUID xuidTalker );

	// Check if player machine is muting any of local players
	bool IsMachineMutingLocalTalkers( XUID xuidPlayer );

public:
	CMatchVoice();
	~CMatchVoice();

protected:
	CUtlVector< XUID > m_arrMutedTalkers;
};

// Match events subscription singleton
extern CMatchVoice *g_pMatchVoice;

#endif // MM_VOICE_H
