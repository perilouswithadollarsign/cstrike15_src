//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef SND_DEV_XAUDIO_H
#define SND_DEV_XAUDIO_H
#pragma once
#include "audio_pch.h"
#include "inetmessage.h"
#include "netmessages.h"
#include "engine/ienginevoice.h"

class IAudioDevice;
IAudioDevice *Audio_CreateXAudioDevice( bool bInitVoice );

#if defined ( _X360 )

class CXboxVoice : public IEngineVoice
{
public:
	CXboxVoice();

	static const DWORD MAX_VOICE_BUFFER_TIME = 200;  // 200ms

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

private:
	PIXHV2ENGINE		m_pXHVEngine;

	// Local chat data
	static const WORD m_ChatBufferSize = XHV_VOICECHAT_MODE_PACKET_SIZE * XHV_MAX_VOICECHAT_PACKETS;
	BYTE        m_ChatBuffer[ XUSER_MAX_COUNT ][ m_ChatBufferSize ];
	WORD        m_wLocalDataSize[ XUSER_MAX_COUNT ];
	bool		m_bUserRegistered[ XUSER_MAX_COUNT ]; //This is to handle a bug in UnregisterLocalTalker where it crashes on 
													  //unregistering a local talker who has not been registered if the 
													  //controller id is non-0.
	// Last voice data sent
	DWORD       m_dwLastVoiceSend[ XUSER_MAX_COUNT ];
};

CXboxVoice *Audio_GetXVoice( void );
IXAudio2 *Audio_GetXAudio2( void );

#endif



#endif // SND_DEV_XAUDIO_H
