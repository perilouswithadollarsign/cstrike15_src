//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
//	Implementation of the CGameEventTransmitter class.
//
//
//===============================================================================


#include <tier0/vprof.h>
#include "gameeventtransmitter.h"
#include "net.h"
#include "tier1/convar.h"
#include "GameEventManager.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static CGameEventTransmitter s_GameEventTransmitter;
CGameEventTransmitter &g_GameEventTransmitter = s_GameEventTransmitter;

//-----------------------------------------------------------------------------
// Convar function to set the IP address and port.
//-----------------------------------------------------------------------------
void CC_TransmitEvents( const CCommand &args )
{
	if ( !g_GameEventTransmitter.SetIPAndPort( args.ArgS() ) )
	{
		if ( args.ArgC() > 1 )
		{
			Msg("Invalid address or port: %s\n", args.ArgS() );
		}
		else
		{
			Msg("No address and port passed in.\n" );
		}
	}
	else
	{
		Msg("SUCCESS! address and port is now set to: %s\n", args.ArgS() );
	}
}
static ConCommand TransmitEvents( "TransmitEvents", CC_TransmitEvents, "Transmits Game Events to <address:port>", FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Initializes the network address based on IP and port
//-----------------------------------------------------------------------------
bool CGameEventTransmitter::SetIPAndPort( const char *address )
{
	m_Adr.SetFromString( address );
	return m_Adr.IsValid();
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameEventTransmitter::CGameEventTransmitter()
{
	m_Adr.Clear();
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CGameEventTransmitter::~CGameEventTransmitter()
{

}

//-----------------------------------------------------------------------------
// Used to be able to set the IP address and port from the command line. 
// This will be the primary way to initialize the system, so that uses 
// won't have to enter the address/port during game play.
//-----------------------------------------------------------------------------
bool CGameEventTransmitter::Init()
{
	const char *address = CommandLine()->ParmValue( "-transmitevents", "" );
	
	if ( address[0] )
	{
		SetIPAndPort( address );
	}
	return true;
}

//-----------------------------------------------------------------------------
// This function actually serializes the event data and sends it out on the network
// to the address and port previously provided.
//-----------------------------------------------------------------------------
void CGameEventTransmitter::TransmitGameEvent( IGameEvent *event )
{
	// Don't bother doing anything if we don't have a transmit address, bad event, or a non-networked event
	if ( !m_Adr.IsValid() || NULL == event || ( event && event->IsLocal() ) )
	{
		return;
	}

	CSVCMsg_GameEvent_t eventData;

	// We send the event name instead of event ID becase the ID's can change depending on what the server sends to the clients
	eventData.set_event_name( event->GetName() );

	// create bitstream from KeyValues
	if ( g_GameEventManager.SerializeEvent( event, &eventData ) )
	{
		bf_write buffer;
		unsigned char buffer_data[MAX_EVENT_BYTES];

		buffer.StartWriting( buffer_data, sizeof(buffer_data) );

		eventData.WriteToBuffer( buffer );

		NET_SendPacket( NULL, NS_CLIENT, m_Adr, buffer_data, buffer.GetNumBytesWritten() );		
	}
	else
	{
		DevMsg("GameEventTransmitter: failed to serialize event '%s'.\n", event->GetName() );
	}
}