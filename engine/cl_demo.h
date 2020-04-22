//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CL_DEMO_H
#define CL_DEMO_H
#ifdef _WIN32
#pragma once
#endif

#include "demofile.h"
#include "cl_demoactionmanager.h"
#include "netmessages_signon.h"

struct DemoCommandQueue
{
	DemoCommandQueue()
	{
		tick = 0;
	}
	int				tick;
	democmdinfo_t	info;
	int				filepos;
};

struct DemoCustomDataCallbackMapping_t
{
	pfnDemoCustomDataCallback pCallback;
	CUtlString name;
};

struct DemoHighlightEntry_t
{
	int nSeekToTick;
	int nFastForwardToTick;
	int nPlayToTick;
	int nActualFirstEventTick;
	int nActualLastEventTick;
	int nNumEvents;
	uint32 unAccountID;
};

class CDemoPlayer : public IDemoPlayer
{

public: // IDemoPlayer interface implementation:
	CDemoPlayer();
	~CDemoPlayer();

	bool	StartPlayback( const char *filename, bool bAsTimeDemo, CDemoPlaybackParameters_t const *pPlaybackParameters, int nStartingTick = -1 ) ;
	bool StartBroadcastPlayback( int nStartingTick );
	virtual bool	IsPlayingBack( void ) const OVERRIDE; // true if demo loaded and playing back
	virtual bool	IsPlaybackPaused( void ) const OVERRIDE; // true if playback paused
	virtual bool	IsPlayingTimeDemo( void ) const OVERRIDE; // true if playing back in timedemo mode
	CDemoPlaybackParameters_t const * GetDemoPlaybackParameters() OVERRIDE;
	void	PausePlayback( float seconds );
	void	SkipToTick( int tick, bool bRelative, bool bPause );
	void	SkipToImportantTick( const DemoImportantTick_t *pTick );

	void	ResumePlayback( void );
	void	StopPlayback( void );
	void	RestartPlayback( void );

	int		GetPlaybackStartTick( void );
	int		GetPlaybackTick( void );
	virtual int 	GetPlaybackDeltaTick( void ) ;
	virtual int		GetPacketTick( void ) ;
	float	GetPlaybackTimeScale( void );
	int		GetTotalTicks( void );

	virtual bool	IsSkipping( void ) const OVERRIDE; // true, if demo player skipping trough packets
	// true if demoplayer can skip backwards
	virtual bool	CanSkipBackwards( void ) const OVERRIDE { return false; }
	
	void	SetPlaybackTimeScale( float timescale );
	void	InterpolateViewpoint(); // override viewpoint
	netpacket_t *ReadPacket( void );
	void	ResetDemoInterpolation( void );

	void	SetPacketReadSuspended( bool bSuspendPacketReading );


	virtual IDemoStream* GetDemoStream() OVERRIDE { return &m_DemoFile; }
public:	// other public functions
	void	MarkFrame( float flFPSVariability );
	void	SetBenchframe( int tick, const char *filename );
	void	ResyncDemoClock( void );
	bool	CheckPausedPlayback( void );
	void	WriteTimeDemoResults( void );
	bool	ParseAheadForInterval( int curtick, int intervalticks );
	void	InterpolateDemoCommand( int nSlot, int targettick, DemoCommandQueue& prev, DemoCommandQueue& next );

	void	SetImportantEventData( const KeyValues *pData ) OVERRIDE;
	void	GetImportantGameEventIDs();
	void	ScanForImportantTicks( void );
	int		FindNextImportantTick( int nCurrentTick, const char *pEventName = NULL ) OVERRIDE; // -1 = no next important tick
	int		FindPreviousImportantTick( int nCurrentTick, const char *pEventName = NULL ) OVERRIDE; // -1 = no previous important tick
	int		FindNextImportantTickByXuidAndEvent( int nCurrentTick, const CSteamID &steamID, const char *pKeyWithXuid, const char *pEventName = NULL ); // -1 = no next important tick
	int		FindPreviousImportantTickByXuidAndEvent( int nCurrentTick, const CSteamID &steamID, const char *pKeyWithXuid, const char *pEventName = NULL ); // -1 = no next important tick
	int		FindNextImportantTickByXuid( int nCurrentTick, const CSteamID &steamID ); // -1 = no next important tick

	const DemoImportantTick_t *GetImportantTick( int nIndex ) OVERRIDE;
	const DemoImportantGameEvent_t *GetImportantGameEvent( const char *pszEventName ) OVERRIDE;
	void	ListImportantTicks( void ) OVERRIDE;
	void	SetHighlightXuid( uint64 xuid, bool bLowlights ) OVERRIDE;
	void	ListHighlightData( void ) OVERRIDE;

	bool	ScanDemo( const char *filename, const char* pszMode ) OVERRIDE;

protected:
	bool	OverrideView( democmdinfo_t& info );
	void	BuildHighlightList( void );

public:
	
	CDemoFile		m_DemoFile;
	int				m_nStartTick;	// For synchronizing playback during timedemo.
	int				m_nPreviousTick;
	netpacket_t		m_DemoPacket;	// last read demo packet
	bool			m_bPlayingBack; // true if demo playback
	bool			m_bPlaybackPaused; // true if demo is paused right now
	float			m_flAutoResumeTime; // how long do we pause demo playback
	float			m_flPlaybackRateModifier;
	int				m_nSkipToTick;	// skip to tick ASAP, -1 = off
	bool			m_bPacketReadSuspended;
	int				m_nTickToPauseOn;
	

	CDemoPlaybackParameters_t const *m_pPlaybackParameters;

	// view origin/angle interpolation:
	CUtlVector< DemoCommandQueue >	m_DestCmdInfo;
	democmdinfo_t					m_LastCmdInfo;
	bool							m_bInterpolateView;
	bool							m_bResetInterpolation;


	// timedemo stuff:
	bool			m_bTimeDemo;	// ture if in timedemo mode
	int				m_nTimeDemoStartFrame;	// host_tickcount at start
	double			m_flTimeDemoStartTime;	// Sys_FloatTime() at second frame of timedemo
	float			m_flTotalFPSVariability; // Frame rate variability
	int				m_nTimeDemoCurrentFrame; // last frame we read a packet
	int				m_nPacketTick;

	// benchframe stuff
	int				m_nSnapshotTick;
	char			m_SnapshotFilename[MAX_OSPATH];

	CUtlVector< DemoCustomDataCallbackMapping_t >	m_CustomDataCallbackMap; //maps callbacks in the file to callbacks in the dll when reading

	// important tick stuff
	CUtlVector< DemoImportantGameEvent_t >			m_ImportantGameEvents;
	CUtlVector< DemoImportantTick_t >				m_ImportantTicks;
	KeyValues										*m_pImportantEventData;

private:
	int				m_nRestartFilePos;
	bool			m_bSavedInterpolateState;
	CSteamID		m_highlightSteamID;
	int				m_nHighlightPlayerIndex;
	bool			m_bDoHighlightScan;
	CUtlVector< DemoHighlightEntry_t > m_highlights;
	int				m_nCurrentHighlight;
	bool			m_bLowlightsMode;
	bool			m_bScanMode;
	char			m_szScanMode[ 64 ];
};

class CDemoRecorder : public IDemoRecorder 
{
public:
	~CDemoRecorder();
	CDemoRecorder();

	CDemoFile *GetDemoFile( void );
	int		GetRecordingTick( void );

	void	StartRecording( const char *filename, bool bContinuously );
	void	SetSignonState( SIGNONSTATE state );
	bool	IsRecording( void );
	void	PauseRecording( void );
	void	ResumeRecording( void );
	void	StopRecording( const CGameInfo *pGameInfo = NULL );
	
	void	RecordCommand( const char *cmdstring );  // record a console command
	void	RecordUserInput( int cmdnumber );  // record a user input command
	void	RecordMessages( bf_read &data, int bits ); // add messages to current packet
	void	RecordPacket( void ); // packet finished, write all recorded stuff to file
	void	RecordServerClasses( ServerClass *pClasses ); // packet finished, write all recorded stuff to file
	void	RecordStringTables(); 
	void	RecordCustomData( int iCallbackIndex, const void *pData, size_t iDataLength ); //record a chunk of custom data

	void	ResetDemoInterpolation( void );

protected:

	void	ResyncDemoClock( void );
	void	StartupDemoFile( void );
	void	StartupDemoHeader( void );
	void	CloseDemoFile( void );
	void	GetClientCmdInfo( democmdinfo_t& info );
	void	WriteDemoCvars( void );
	void	WriteBSPDecals( void );
	void	WriteSplitScreenPlayers( void );
	void	WriteMessages( bf_write &message );
	bool	ComputeNextIncrementalDemoFilename( char *name, int namesize );

public:
	CDemoFile		m_DemoFile;

	// For synchronizing playback during timedemo.
	int				m_nStartTick; // host_tickcount when starting recoring

	// Name of demo file we are appending onto.
	char			m_szDemoBaseName[ MAX_OSPATH ];  

	// For demo file handle
	bool			m_bIsDemoHeader;	// true, if m_hDemoFile is the header file
	bool			m_bCloseDemoFile;	// if true, demo file will be closed ASAP

	bool			m_bRecording;	  // true if recording
	bool			m_bContinuously; // start new record after each
	int				m_nDemoNumber;	// demo count, increases each changelevel
	int				m_nFrameCount;	// # of demo frames in this segment.

	bf_write		m_MessageData; // temp buffer for all network messages

	bool			m_bResetInterpolation;
};

extern CDemoPlayer *g_pClientDemoPlayer;
extern CDemoRecorder *g_pClientDemoRecorder;

struct RegisteredDemoCustomDataCallbackPair_t
{
	pfnDemoCustomDataCallback pCallback;
	string_t szSaveID;
};
extern CUtlVector<RegisteredDemoCustomDataCallbackPair_t> g_RegisteredDemoCustomDataCallbacks;

#endif // CL_DEMO_H
