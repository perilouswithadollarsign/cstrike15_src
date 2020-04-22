//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVDEMO_H
#define HLTVDEMO_H
#ifdef _WIN32
#pragma once
#endif

#include <filesystem.h>

#include "demo.h"
#include "demofile.h"
#include "netmessages_signon.h"

class CHLTVFrame;
class CGameInfo;
class CHLTVServer;
class CNETMsg_PlayerAvatarData_t;

class CHLTVDemoRecorder : private IDemoRecorder
{
public:
	CHLTVDemoRecorder( CHLTVServer *pHltvServer );
	virtual ~CHLTVDemoRecorder();

	// Convert to demo recorder
	IDemoRecorder* GetDemoRecorder() { return this; }

public: // For use by HLTVServer
	void	WriteFrame( CHLTVFrame *pFrame, bf_write *additionaldata = NULL );
	void	RecordPlayerAvatar( const CNETMsg_PlayerAvatarData_t* hltvPlayerAvatar );

	bool	IsRecording( void );			// True between StartRecording and StopRecording()
	void	StopRecording( const CGameInfo* pGameInfo = NULL );
	int		GetRecordingTick( void );
	const char* GetDemoFilename( void ) { return m_DemoFile.m_szFileName; }

	// These are somewhat misnamed; they queue up the recording to start on the next tick.
	// This is to guarantee that all game state is consistent during recording.
	void	StartAutoRecording();
	void	StartRecording( const char *filename, bool bContinuously );

private: // IDemoRecorder
	void	RecordCommand( const char *cmdstring );
	void	RecordMessages( bf_read &data, int bits );
	void	RecordPacket( void );
	void	RecordServerClasses( ServerClass *pClasses );
	void	RecordCustomData( int iCallbackIndex, const void *pData, size_t iDataLength );

	// These are not needed by HLTV demos, as they are for the local client; for HLTV/GOTV there
	// is no local client playing the game.
	void	SetSignonState( SIGNONSTATE state ) {}
	void	RecordUserInput( int cmdnumber ) {}
	void	ResetDemoInterpolation( void ) {}

private: // internal
	void	CloseFile();
	void	Reset();

	void	WriteServerInfo();
	int		WriteSignonData();  // write all necessary signon data and returns written bytes
	void	WriteMessages( unsigned char cmd, bf_write &message );
	void	RecordStringTables();

	// If we are recording and we have finished the 'sign-on' step of demo recording
	bool	HasRecordingActuallyStarted() { return m_bIsRecording && m_nStartTick >= 0; }

private:
	CDemoFile		m_DemoFile;
	bool			m_bIsRecording;
	int				m_nFrameCount;
	int				m_nStartTick;
	int				m_SequenceInfo;
	int				m_nDeltaTick;	
	bf_write		m_MessageData; // temp buffer for all network messages
	int				m_nLastWrittenTick;
	CHLTVServer		*hltv;
};



#endif // HLTVDEMO_H
