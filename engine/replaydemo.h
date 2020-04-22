//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef REPLAYDEMO_H
#define REPLAYDEMO_H
#ifdef _WIN32
#pragma once
#endif

#include <filesystem.h>

#include "demo.h"
#include "demofile.h"
#include "netmessages_signon.h"

class CReplayFrame;
class CReplayServer;

class CReplayDemoRecorder : public IDemoRecorder
{
public:
	CReplayDemoRecorder( CReplayServer* pServer );
	virtual ~CReplayDemoRecorder();

	CDemoFile *GetDemoFile();
	int		GetRecordingTick();

	void	DumpToFile( char const *filename );

	void	StartRecording( const char *filename, bool bContinuously );
	void	StartAutoRecording();
	void	SetSignonState( SIGNONSTATE state ) {}; // not need by Replay recorder
	bool	IsRecording();
	void	PauseRecording() {}
	void	ResumeRecording() {}
	void	StopRecording( const CGameInfo *pGameInfo = NULL );
	
	void	RecordCommand( const char *cmdstring );
	void	RecordUserInput( int cmdnumber ) {} ;  // not need by Replay recorder
	void	RecordMessages( bf_read &data, int bits );
	void	RecordPacket(); 
	void	RecordServerClasses( ServerClass *pClasses );
	void	RecordStringTables(); 
	void	RecordCustomData( int iCallbackIndex, const void *pData, size_t iDataLength );

	void	ResetDemoInterpolation() {}

	void	WriteFrame( CReplayFrame *pFrame );
	void	CloseFile();
	void	Reset();

	void	WriteServerInfo();
	int		WriteSignonData();  // write all necessary signon data and returns written bytes
	void	WriteMessages( unsigned char cmd, bf_write &message );
	int		GetMaxAckTickCount();

	void	GetUniqueDemoFilename( char* pOut, int nLength );

public:
	CDemoFile		m_DemoFile;
	bool			m_bIsRecording;
	int				m_nFrameCount;
	int				m_nStartTick;
	int				m_SequenceInfo;
	int				m_nDeltaTick;	
	int				m_nSignonTick;
	bf_write		m_MessageData; // temp buffer for all network messages
	CReplayServer	*m_pReplayServer;
};



#endif // REPLAYDEMO_H
