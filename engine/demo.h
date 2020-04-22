//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#ifndef DEMO_H
#define DEMO_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

#include "net.h"
#include "demofile/demoformat.h"
#include "netmessages_signon.h"

class CUtlBuffer;
class CDemoFile;
class ServerClass;
class CGameInfo;
struct CDemoPlaybackParameters_t;
struct DemoImportantTick_t;
struct DemoImportantGameEvent_t;
class IDemoStream;


abstract_class IDemoRecorder 
{
public:
	~IDemoRecorder() {} // TODO: Should be virtual?

	// Notify the demo recorder of the client sign-on state, so it knows whether the
	// string tables and entity data are fully populated.
	virtual void	SetSignonState( SIGNONSTATE state ) = 0;

	// Write entity send-tables so we can fix up deltas on later versions of the client
	// Note that this usually gets done as part of StartRecording, so it's a bit odd that it's in
	// the interface here.
	virtual void	RecordServerClasses( ServerClass *pClasses ) = 0;
	
	// Recording commands
	virtual void	RecordMessages( bf_read &data, int bits ) = 0; // add messages to current packet
	virtual void	RecordPacket( void ) = 0; // packet finished, write all recorded stuff to file
	virtual void	RecordCommand( const char *cmdstring ) = 0;  // record a console command
	virtual void	RecordUserInput( int cmdnumber ) = 0;  // record a user input command

	// This function is pretty scary.  It relies on the client code explicitly
	// versioning the embedded data (which the only client, blob particles, doesn't
	// seem to do!)
	//
	// Also, I am not sure how the callbacks are referenced in cross-version playback.
	//
	// You should use protocol buffers instead, with RecordMessages().  This way all
	// cross-version stuff is handled for you and backwards compatibility is a lot
	// simpler.
	virtual void	RecordCustomData( int iCallbackIndex, const void *pData, size_t iDataLength ) = 0; //record a chunk of custom data

	// Notify the demo recorder that an entity is about to teleport, and
	// stop it from being interpolated between its old and new positions.
	//
	// TODO: Shouldn't this specify which entity is being teleported? Currently it seems
	// to only do anything in client-recorded demos, and notifies that the current client
	// is being teleported.  (Also, nobody seems to ever call this function in CSGO, so
	// it's hard to verify my assumptions about it!)
	virtual void	ResetDemoInterpolation() = 0;

	// TODO: This doesn't really belong here; different demo objects have different processes
	// for handling start/stop recording.
	virtual void	StartRecording( const char *filename, bool bContinuously ) = 0;

	// Called during client disconnect.  Should probably be named 'Shutdown' or 'Detach'.
	//
	// NOTE: There is an unused CGameInfo* parameter here that probably came from DotA2.
	// We never define that structure so it is impossible to pass anything aside from NULL.
	//
	// It seems its purpose is to attach some extra metadata about the game that wasn't
	// known at the start of recording to the header of the demo.  This lets you populate
	// the playback interface with information about when kills happened, towers died, etc.
	virtual void	StopRecording( const CGameInfo* pGameInfo = NULL ) = 0;

	// True between StartRecording and StopRecording
	virtual bool	IsRecording( void ) = 0;

	// This is used by cdll_engine_int to provide an interface to the client's demo
	// recorder.  However, they don't seem to have any callers through that wrapper.
	virtual int		GetRecordingTick( void ) = 0;

	// These don't seem to be used anywhere, and are probably specific to the type
	// of demo being recorded, as opposed to belonging in IDemoRecorder.
	//
	// TODO: Delete these for real instead of just commenting them.
	//
	// virtual CDemoFile *GetDemoFile() = 0;
	//
	// virtual void	PauseRecording( void ) = 0;
	// virtual void	ResumeRecording( void ) = 0;

	// This function is especially odd.  This is generally handled by StartRecording/
	// SetSignonState or something similar to write the initial string tables, and not
	// called externally.  Updates are handled via the usual string table update
	// messages via RecordMessages().  Not sure where it would be used externally!
	//
	// virtual void	RecordStringTables() = 0; 
};


abstract_class IDemoPlayer
{
public:
	virtual ~IDemoPlayer() {};

	virtual IDemoStream *GetDemoStream() = 0;
	virtual int		GetPlaybackStartTick( void ) = 0;
	virtual int		GetPlaybackTick( void ) = 0;
//	virtual int		GetPlaybackDeltaTick( void ) = 0;
//	virtual int		GetPacketTick( void ) = 0;
	
	virtual bool	IsPlayingBack( void ) const = 0; // true if demo loaded and playing back
	virtual bool	IsPlaybackPaused( void ) const = 0; // true if playback paused
	virtual bool	IsPlayingTimeDemo( void ) const = 0; // true if playing back in timedemo mode
	virtual bool	IsSkipping( void ) const = 0; // true, if demo player skipping trough packets
	virtual bool	CanSkipBackwards( void ) const = 0; // true if demoplayer can skip backwards

	virtual void	SetPlaybackTimeScale( float timescale ) = 0; // sets playback timescale
	virtual float	GetPlaybackTimeScale( void ) = 0; // get playback timescale

	virtual void	PausePlayback( float seconds ) = 0; // pause playback n seconds, -1 = forever
	virtual void	SkipToTick( int tick, bool bRelative, bool bPause ) = 0; // goto a specific tick, 0 = start, -1 = end
	virtual void	SkipToImportantTick( const DemoImportantTick_t *pTick ) = 0;
	virtual void	ResumePlayback( void ) = 0; // resume playback
	virtual void	StopPlayback( void ) = 0;	// stop playback, close file
	virtual void	InterpolateViewpoint() = 0; // override viewpoint
	virtual netpacket_t *ReadPacket( void ) = 0; // read packet from demo file

	virtual void ResetDemoInterpolation() = 0;

	virtual CDemoPlaybackParameters_t const * GetDemoPlaybackParameters() = 0;

	virtual void SetPacketReadSuspended( bool bSuspendPacketReading ) = 0;

	virtual void	SetImportantEventData( const KeyValues *pData ) = 0;
	virtual int		FindNextImportantTick( int nCurrentTick, const char *pEventName = NULL ) = 0; // -1 = no next important tick
	virtual int		FindPreviousImportantTick( int nCurrentTick, const char *pEventName = NULL ) = 0; // -1 = no previous important tick
	virtual const DemoImportantTick_t *GetImportantTick( int nIndex ) = 0;
	virtual const DemoImportantGameEvent_t *GetImportantGameEvent( const char *pszEventName ) = 0;
	virtual void	ListImportantTicks( void ) = 0;
	virtual void	ListHighlightData( void ) = 0;
	virtual void	SetHighlightXuid( uint64 xuid, bool bLowlights ) = 0;

	virtual bool	ScanDemo( const char *filename, const char* pszMode ) = 0;
};

#if !defined( DEDICATED )
extern IDemoPlayer *demoplayer;	// reference to current demo player
extern IDemoRecorder *demorecorder; // reference to current demo recorder
#endif

#endif // DEMO_H
