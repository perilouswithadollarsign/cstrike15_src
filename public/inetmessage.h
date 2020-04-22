//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: INetMessage interface
//
// $NoKeywords: $
//=============================================================================//

#ifndef INETMESSAGE_H
#define INETMESSAGE_H

#include "tier1/bitbuf.h"

class INetMsgHandler;
class INetMessage;
class INetChannel;

class INetMessage
{
public:
	virtual	~INetMessage() {};

	// Use these to setup who can hear whose voice.
	// Pass in client indices (which are their ent indices - 1).
	
	virtual void	SetNetChannel(INetChannel * netchan) { DebuggerBreak(); return; }
	virtual void	SetReliable( bool state ) = 0;	// set to true if it's a reliable message
	
	virtual bool	Process( void ) { DebuggerBreak(); return false; }
	
	virtual	bool	ReadFromBuffer( bf_read &buffer ) = 0; // returns true if parsing was OK
	virtual	bool	WriteToBuffer( bf_write &buffer ) const = 0;	// returns true if writing was OK
		
	virtual bool	IsReliable( void ) const = 0;  // true, if message needs reliable handling
	
	virtual int				GetType( void ) const = 0; // returns module specific header tag eg svc_serverinfo
	virtual int				GetGroup( void ) const = 0;	// returns net message group of this message
	virtual const char		*GetName( void ) const = 0;	// returns network message name, eg "svc_serverinfo"

	virtual INetChannel		*GetNetChannel( void ) const { DebuggerBreak(); return NULL; }

	virtual const char		*ToString( void ) const = 0; // returns a human readable string about message content
	virtual size_t			GetSize() const = 0;
};

class INetMessageBinder
{
public:
	virtual	~INetMessageBinder() {};

	virtual int	GetType( void ) const = 0; // returns module specific header tag eg svc_serverinfo
	virtual void SetNetChannel(INetChannel * netchan) = 0; // netchannel this message is from/for
	virtual INetMessage *CreateFromBuffer( bf_read &buffer ) = 0;
	virtual bool Process( const INetMessage &src ) = 0;
};

class INetChannelHandler
{
public:
	virtual	~INetChannelHandler( void ) {};

	virtual void ConnectionStart(INetChannel *chan) = 0;	// called first time network channel is established
	virtual void ConnectionStop( ) = 0;	// called first time network channel is established

	virtual void ConnectionClosing(const char *reason) = 0; // network channel is being closed by remote site

	virtual void ConnectionCrashed(const char *reason) = 0; // network error occured

	virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) = 0;	// called each time a new packet arrived

	virtual void PacketEnd( void ) = 0; // all messages has been parsed

	virtual void FileRequested(const char *fileName, unsigned int transferID, bool isReplayDemoFile) = 0; // other side request a file for download

	virtual void FileReceived(const char *fileName, unsigned int transferID, bool isReplayDemoFile) = 0; // we received a file

	virtual void FileDenied(const char *fileName, unsigned int transferID, bool isReplayDemoFile) = 0;	// a file request was denied by other side

	virtual void FileSent(const char *fileName, unsigned int transferID, bool isReplayDemoFile) = 0;	// we sent a file

	virtual bool ChangeSplitscreenUser( int nSplitScreenUserSlot ) = 0; // interleaved networking used by SS system is changing the SS player slot that the subsequent messages pertain to
};

#endif

