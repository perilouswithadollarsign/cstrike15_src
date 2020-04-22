//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef INETCHANNEL_H
#define INETCHANNEL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "inetchannelinfo.h"
#include "tier1/bitbuf.h"
#include "tier1/netadr.h"
#include "tier1/ns_address.h"

class	IDemoRecorder;
class	INetMessage;
class	INetChannelHandler;
class	INetChannelInfo;
class	INetMessageBinder;
typedef struct netpacket_s netpacket_t;

#ifndef NET_PACKET_ST_DEFINED
#define NET_PACKET_ST_DEFINED
typedef struct netpacket_s
{
	ns_address		from;		// sender address
	int				source;		// received source 
	double			received;	// received time
	unsigned char	*data;		// pointer to raw packet data
	bf_read			message;	// easy bitbuf data access
	int				size;		// size in bytes
	int				wiresize;   // size in bytes before decompression
	bool			stream;		// was send as stream
	struct netpacket_s *pNext;	// for internal use, should be NULL in public
} netpacket_t;
#endif // NET_PACKET_ST_DEFINED


abstract_class INetChannel : public INetChannelInfo
{
public:
	virtual	~INetChannel( void ) {};

	virtual void	SetDataRate(float rate) = 0;

	virtual bool	RegisterMessage(INetMessageBinder *msg) = 0;
	virtual bool	UnregisterMessage(INetMessageBinder *msg) = 0;

	virtual bool	StartStreaming( unsigned int challengeNr ) = 0;
	virtual void	ResetStreaming( void ) = 0;
	virtual void	SetTimeout(float seconds, bool bForceExact = false) = 0;
	virtual void	SetDemoRecorder(IDemoRecorder *recorder) = 0;
	virtual void	SetChallengeNr(unsigned int chnr) = 0;
	
	virtual void	Reset( void ) = 0;
	virtual void	Clear( void ) = 0;
	virtual void	Shutdown(const char *reason) = 0;
	
	virtual void	ProcessPlayback( void ) = 0;
	virtual bool	ProcessStream( void ) = 0;
	virtual void	ProcessPacket( struct netpacket_s* packet, bool bHasHeader ) = 0;
			
	virtual bool	SendNetMsg(INetMessage &msg, bool bForceReliable = false, bool bVoice = false ) = 0;
#ifdef POSIX
	FORCEINLINE bool SendNetMsg(INetMessage const &msg, bool bForceReliable = false, bool bVoice = false ) { return SendNetMsg( *( (INetMessage *) &msg ), bForceReliable, bVoice ); }
#endif
	virtual bool	SendData(bf_write &msg, bool bReliable = true) = 0;
	virtual bool	SendFile(const char *filename, unsigned int transferID, bool bIsReplayDemoFile ) = 0;
	virtual void	DenyFile(const char *filename, unsigned int transferID, bool bIsReplayDemoFile ) = 0;
	virtual void	RequestFile_OLD(const char *filename, unsigned int transferID) = 0;	// get rid of this function when we version the 
	virtual void	SetChoked( void ) = 0;
	virtual int		SendDatagram(bf_write *data) = 0;		
	virtual bool	Transmit(bool onlyReliable = false) = 0;

	virtual const ns_address &GetRemoteAddress( void ) const = 0;
	virtual INetChannelHandler *GetMsgHandler( void ) const = 0;
	virtual int				GetDropNumber( void ) const = 0;
	virtual int				GetSocket( void ) const = 0;
	virtual unsigned int	GetChallengeNr( void ) const = 0;
	virtual void			GetSequenceData( int &nOutSequenceNr, int &nInSequenceNr, int &nOutSequenceNrAck ) = 0;
	virtual void			SetSequenceData( int nOutSequenceNr, int nInSequenceNr, int nOutSequenceNrAck ) = 0;
		
	virtual void	UpdateMessageStats( int msggroup, int bits) = 0;
	virtual bool	CanPacket( void ) const = 0;
	virtual bool	IsOverflowed( void ) const = 0;
	virtual bool	IsTimedOut( void ) const  = 0;
	virtual bool	HasPendingReliableData( void ) = 0;

	virtual void	SetFileTransmissionMode(bool bBackgroundMode) = 0;
	virtual void	SetCompressionMode( bool bUseCompression ) = 0;
	virtual unsigned int RequestFile(const char *filename, bool bIsReplayDemoFile ) = 0;
	virtual float	GetTimeSinceLastReceived( void ) const = 0;	// get time since last received packet in seconds

	virtual void	SetMaxBufferSize(bool bReliable, int nBytes, bool bVoice = false ) = 0;

	virtual bool	IsNull() const = 0;
	virtual int		GetNumBitsWritten( bool bReliable ) = 0;
	virtual void	SetInterpolationAmount( float flInterpolationAmount ) = 0;
	virtual void	SetRemoteFramerate( float flFrameTime, float flFrameTimeStdDeviation, float flFrameStartTimeStdDeviation ) = 0;

	// Max # of payload bytes before we must split/fragment the packet
	virtual void	SetMaxRoutablePayloadSize( int nSplitSize ) = 0;
	virtual int		GetMaxRoutablePayloadSize() = 0;

	// For routing messages to a different handler
	virtual bool	SetActiveChannel( INetChannel *pNewChannel ) = 0;
	virtual void	AttachSplitPlayer( int nSplitPlayerSlot, INetChannel *pChannel ) = 0;
	virtual void	DetachSplitPlayer( int nSplitPlayerSlot ) = 0;

	virtual bool	IsRemoteDisconnected() const = 0;

	virtual bool	WasLastMessageReliable() const = 0; // True if the last (or currently processing) message was sent via the reliable channel

	virtual const unsigned char * GetChannelEncryptionKey() const = 0;	// Returns a buffer with channel encryption key data (network layer determines the buffer size)

	virtual bool	EnqueueVeryLargeAsyncTransfer( INetMessage &msg ) = 0;	// Enqueues a message for a large async transfer
};


#endif // INETCHANNEL_H
