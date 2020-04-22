//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: Playback of broadcast (cst) files
//
//=============================================================================//
#ifndef ENGINE_CL_BROADCAST_HDR
#define ENGINE_CL_BROADCAST_HDR


#include "broadcast.h"
#include "demostreamhttp.h"
#include "tier1/utlbufferstrider.h"

class CBroadcastPlayer : public IDemoPlayer, public IDemoStreamClient
{
public:
	CBroadcastPlayer();
	~CBroadcastPlayer();

	virtual IDemoStream *GetDemoStream();
	virtual int		GetPlaybackStartTick( void ) OVERRIDE;
	virtual int		GetPlaybackTick( void ) OVERRIDE;
	virtual int		GetPlaybackDeltaTick( void ) ;
	virtual int		GetPacketTick( void ) ;

	void StartStreaming( const char *url, const char *options );
	bool StartBroadcastPlayback( int nStartingTick );
	bool OnEngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt );

	virtual bool	IsPlayingBack( void ) const OVERRIDE { return m_bPlayingBack; }
	virtual bool	IsPlaybackPaused( void ) const OVERRIDE;
	virtual bool	IsPlayingTimeDemo( void ) const OVERRIDE { return false; } // not supported
	virtual bool	IsSkipping( void ) const OVERRIDE { return m_bPlayingBack && m_nSkipToTick != -1; } // true, if demo player skipping trough packets
	virtual bool	CanSkipBackwards( void ) const OVERRIDE { return true;  } // true if demoplayer can skip backwards

	virtual void	SetPlaybackTimeScale( float timescale ) OVERRIDE; // sets playback timescale
	virtual float	GetPlaybackTimeScale( void ) OVERRIDE; // get playback timescale

	virtual void	PausePlayback( float seconds ) OVERRIDE; // pause playback n seconds, -1 = forever
	virtual void	SkipToTick( int tick, bool bRelative, bool bPause ) OVERRIDE { } // goto a specific tick, 0 = start, -1 = end
	virtual void	SkipToImportantTick( const DemoImportantTick_t *pTick ) OVERRIDE { }
	virtual void	ResumePlayback( void ) OVERRIDE; // resume playback
	virtual void	StopPlayback( void ) OVERRIDE;	// stop playback, close file
	virtual void	InterpolateViewpoint() OVERRIDE { } // override viewpoint
	virtual netpacket_t *ReadPacket( void ) OVERRIDE; // read packet from demo file

	void SetDemoBuffer( CDemoStreamHttp::Buffer_t * pBuffer );
	void StrideDemoPacket( int nLength );
	void StrideDemoPacket();
	uint GetReminingStrideLength();

	virtual void ResetDemoInterpolation() OVERRIDE { }

	virtual CDemoPlaybackParameters_t const * GetDemoPlaybackParameters() OVERRIDE;

	virtual void SetPacketReadSuspended( bool bSuspendPacketReading ) OVERRIDE;

	virtual void	SetImportantEventData( const KeyValues *pData ) OVERRIDE {  }
	virtual int		FindNextImportantTick( int nCurrentTick, const char *pEventName = NULL ) OVERRIDE { Assert( !"not implemented" ); return 0; } // -1 = no next important tick
	virtual int		FindPreviousImportantTick( int nCurrentTick, const char *pEventName = NULL ) OVERRIDE { Assert( !"not implemented" ); return 0; } // -1 = no previous important tick
	virtual const DemoImportantTick_t *GetImportantTick( int nIndex ) OVERRIDE { Assert( !"not implemented" );  return NULL; }
	virtual const DemoImportantGameEvent_t *GetImportantGameEvent( const char *pszEventName ) OVERRIDE { Assert( !"not implemented" ); return NULL; }
	virtual void	ListImportantTicks( void ) OVERRIDE { Assert( !"not implemented" ); }
	virtual void	ListHighlightData( void ) OVERRIDE { Assert( !"not implemented" ); }
	virtual void	SetHighlightXuid( uint64 xuid, bool bLowlights ) OVERRIDE { Assert( !"not implemented" ); }

	virtual bool	ScanDemo( const char *filename, const char* pszMode ) OVERRIDE { Assert( !"not implemented" ); return false; }

	virtual void OnDemoStreamStart( const DemoStreamReference_t &start, int nResync );
	virtual bool OnDemoStreamRestarting();
	virtual void OnDemoStreamStop() OVERRIDE;

protected:
	bool StartStreamingInternal();
	void ResyncDemoClock();
	bool CheckPausedPlayback( void );
	bool PreparePacket( void ); // read packet from demo file
	void ReadCmdHeader( unsigned char& cmd, int& tick, int &nPlayerSlot );
	void ResyncStream();
protected:

	int				m_nStartHostTick;	// For synchronizing playback during timedemo.
	int				m_nStreamStartTick;
	int				m_nPreviousTick;
	CDemoStreamHttp::BufferRef m_DemoBuffer;
	CBufferStrider m_DemoStrider;
	netpacket_t	m_DemoPacket;	// last read demo packet
	bool m_bPlayingBack;
	bool m_bPlaybackPaused;
	float m_flAutoResumeTime;
	float m_flPlaybackRateModifier;
	int m_nSkipToTick;	// skip to tick ASAP, -1 = off
	uint m_nFileSize;
	CUtlVector< BroadcastTocKeyframe_t > m_Keyframes;
	bool m_bInterpolateView;
	bool m_bResetInterpolation;
	bool m_bPacketReadSuspended;
	float			m_flTotalFPSVariability; // Frame rate variability
	int				m_nPacketTick;
	int				m_nPreparePacketLastFail;

	enum StreamStateEnum_t
	{
		STREAM_STOP,
		STREAM_SYNC,
		STREAM_START,
		STREAM_MAP_LOADED,
		STREAM_WAITING_FOR_KEYFRAME,
		STREAM_FULLFRAME,
		STREAM_BEFORE_DELTAFRAMES,
		STREAM_DELTAFRAMES
	};
	StreamStateEnum_t m_nStreamState; // the pieces of stream state that still need to be streamed
	int m_nStreamFragment; // the next fragment to stream
	double m_dDelayedPrecacheTimeStart;

	CDemoStreamHttp m_DemoStream;

	bool m_bIgnoreDemoStopCommand;
	bool m_bSkipSync;
	double m_dResyncTimerStart;
};

extern CBroadcastPlayer s_ClientBroadcastPlayer;

#endif
