//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef SND_DEV_PS3AUDIO_H
#define SND_DEV_PS3AUDIO_H
#pragma once
#include "audio_pch.h"
#include "inetmessage.h"
#include "netmessages.h"
#include "engine/ienginevoice.h"

class IAudioDevice;
IAudioDevice *Audio_CreatePS3AudioDevice( bool bInitVoice );

#define PS3_SUPPORT_XVOICE
#ifdef PS3_SUPPORT_XVOICE

class CEngineVoicePs3 : public IEngineVoice
{
public:
	CEngineVoicePs3();

	static const float MAX_VOICE_BUFFER_TIME = 0.200f;  // 200ms

	void		VoiceInit( void );
	void		VoiceShutdown( void );
	void		AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags );
	void		RemovePlayerFromVoiceList( XUID xPlayer, int iController );
	bool		VoiceUpdateData( int iCtrlr );
	void		GetVoiceData( int iCtrlr, CCLCMsg_VoiceData_t *pData );
	void		GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes );
	void		VoiceSendData( int iCtrlr, INetChannel	*pChannel );
	void		VoiceResetLocalData( int iCtrlr );
	void		PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers = NULL );
	void		UpdateHUDVoiceStatus( void );
	void		GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers );
	void		SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback );
	bool		IsLocalPlayerTalking( int controlerID );
	bool		IsPlayerTalking( XUID uid );
	bool		IsHeadsetPresent( int id );
	void		RemoveAllTalkers();

	void		PlayPortInterruptHandler();

private:
	// Local chat data
	static const uint8 m_numVoiceUsers = 1;
	inline int GetVoiceUserIndex( int iController ) const { return 0; }
	inline int GetVoiceDeviceNumber( int iController ) const { return 0; }
	static const WORD m_ChatBufferSize = 16*1024;
	BYTE        m_ChatBuffer[ m_numVoiceUsers ][ m_ChatBufferSize ];
	WORD        m_wLocalDataSize[ m_numVoiceUsers ];
	enum VoiceState_t
	{
		kVoiceNotInitialized,
		kVoiceInit,
		kVoiceOpen
	};
	VoiceState_t m_bUserRegistered[ m_numVoiceUsers ]; //This is to handle a bug in UnregisterLocalTalker where it crashes on 
													  //unregistering a local talker who has not been registered if the 
													  //controller id is non-0.
	// Last voice data sent
	float       m_dwLastVoiceSend[ m_numVoiceUsers ];

	//
	// PS3 playback parameters
	//
	int CreateVoicePortsLocal( uint64 uiFlags );
	uint32		m_memContainer;
	uint32		m_portIMic, m_portOVoice, m_portIVoiceEcho, m_portOPcm, m_portOEarphone, m_portOSendForRemote;
	struct RemoteTalker_t
	{
		XUID m_xuid;
		uint32 m_portIRemoteVoice;
		uint64 m_uiFlags;
		float m_flLastTalkTimestamp;
	};
	int CreateVoicePortsRemote( RemoteTalker_t &rt );
	CUtlVector< RemoteTalker_t > m_arrRemoteTalkers;
};

CEngineVoicePs3 *Audio_GetXVoice( void );

#endif


#endif // SND_DEV_PS3AUDIO_H
