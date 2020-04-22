//===== Copyright c 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IMATCHVOICE_H
#define IMATCHVOICE_H

#ifdef _WIN32
#pragma once
#endif

abstract_class IMatchVoice
{
public:
	// Whether remote player talking can be visualized / audible
	virtual bool CanPlaybackTalker( XUID xuidTalker ) = 0;

	// Whether we are explicitly muting a remote player
	virtual bool IsTalkerMuted( XUID xuidTalker ) = 0;

	// Whether we are muting any player on the player's machine
	virtual bool IsMachineMuted( XUID xuidPlayer ) = 0;

	// Whether voice recording mode is currently active
	virtual bool IsVoiceRecording() = 0;

	// Enable or disable voice recording
	virtual void SetVoiceRecording( bool bRecordingEnabled ) = 0;

	// Enable or disable voice mute for a given talker
	virtual void MuteTalker( XUID xuidTalker, bool bMute ) = 0;
};

#endif

