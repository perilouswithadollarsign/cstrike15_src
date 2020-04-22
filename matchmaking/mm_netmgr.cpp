//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_netmgr.h"

#include "proto_oob.h"
#include "protocol.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Convars
//

ConVar net_allow_multicast( "net_allow_multicast", "1", FCVAR_RELEASE | FCVAR_ARCHIVE );
ConVar net_allow_syslink( "net_allow_syslink", "1", FCVAR_DEVELOPMENTONLY );


//
// CConnectionlessLanMgr
//

CConnectionlessLanMgr::CConnectionlessLanMgr()
{
	;
}

CConnectionlessLanMgr::~CConnectionlessLanMgr()
{
	;
}

static CConnectionlessLanMgr g_ConnectionlessLanMgr;
CConnectionlessLanMgr *g_pConnectionlessLanMgr = &g_ConnectionlessLanMgr;

//
// Implementation of CConnectionlessLanMgr
//

KeyValues * CConnectionlessLanMgr::UnpackPacket( netpacket_t *packet )
{
	if ( !packet )
		return NULL;

	if ( !packet->size || !packet->data )
		return NULL;

	// Try to unpack the data
	if ( packet->message.ReadLong() != 0 )
		return NULL;
	if ( packet->message.ReadLong() != g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() )
		return NULL;

	MEM_ALLOC_CREDIT();
	int nDataLen = packet->message.ReadLong();

	m_buffer.Clear();
	m_buffer.EnsureCapacity( nDataLen );
	packet->message.ReadBytes( m_buffer.Base(), nDataLen );
	m_buffer.SeekPut( CUtlBuffer::SEEK_HEAD, nDataLen );
	m_buffer.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );

	// Unpack key values
	KeyValues *pMsg = new KeyValues( "" );
	if ( pMsg->ReadAsBinary( m_buffer ) )
	{
		// Read binary data as well
		if ( int nBinarySize = packet->message.ReadLong() )
		{
			m_buffer.Clear();
			m_buffer.EnsureCapacity( nBinarySize );
			packet->message.ReadBytes( m_buffer.Base(), nBinarySize );
			m_buffer.SeekPut( CUtlBuffer::SEEK_HEAD, nBinarySize );

			// set the ptr to point to our buffer
			pMsg->SetPtr( "binary/ptr", m_buffer.Base() );
		}

		return pMsg;	// "pMsg" is deleted by caller
	}
	
	pMsg->deleteThis();
	return NULL;
}

bool CConnectionlessLanMgr::ProcessConnectionlessPacket( netpacket_t *packet )
{
	// Unpack key values
	KeyValues *pMsg = UnpackPacket( packet );
	if ( !pMsg )
		return false;
		
	MEM_ALLOC_CREDIT();
	KeyValues *notify = new KeyValues( "OnNetLanConnectionlessPacket" );
	notify->SetString( "from", ns_address_render( packet->from ).String() );
	notify->AddSubKey( pMsg );
	
	g_pMatchEventsSubscription->BroadcastEvent( notify );
	return true;	// "pMsg" is deleted as child of "notify"
}

void CConnectionlessLanMgr::Update()
{
#ifdef _X360
	if ( !net_allow_syslink.GetBool() )
		return;

	g_pMatchExtensions->GetINetSupport()->ProcessSocket( INetSupport::NS_SOCK_SYSTEMLINK, this );
#endif
}

void CConnectionlessLanMgr::SendPacket( KeyValues *pMsg, char const *szAddress /*= NULL*/, INetSupport::NetworkSocket_t eSock )
{
	char buf[ INetSupport::NC_MAX_ROUTABLE_PAYLOAD ];
	bf_write msg( buf, sizeof( buf ) );

	msg.WriteLong( CONNECTIONLESS_HEADER );
	msg.WriteLong( 0 );
	msg.WriteLong( g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() );

	CUtlBuffer data;
	data.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
	pMsg->WriteAsBinary( data );

	msg.WriteLong( data.TellMaxPut() );
	msg.WriteBytes( data.Base(), data.TellMaxPut() );
	Assert( !msg.IsOverflowed() );

	// Special case when encoding binary data
	KeyValues *kvPtr = pMsg->FindKey( "binary/ptr" );
	KeyValues *kvSize = pMsg->FindKey( "binary/size" );
	if ( kvPtr && kvSize )
	{
		void *pvData = kvPtr->GetPtr();
		int nSize = kvSize->GetInt();

		if ( pvData && nSize )
		{
			msg.WriteLong( nSize );
			if ( !msg.WriteBytes( pvData, nSize ) )
			{
				Assert( 0 );
				return;
			}
		}
		else
		{
			msg.WriteLong( 0 );
		}
	}
	else
	{
		msg.WriteLong( 0 );
	}
	Assert( !msg.IsOverflowed() );

	// Prepare the address
	netadr_t inetAddr;
	if ( szAddress )
	{
		if ( szAddress[0] == '*' && szAddress[1] == ':' )
		{
			inetAddr.SetType( NA_BROADCAST );
			inetAddr.SetPort( atoi( szAddress + 2 ) );
		}
		else
		{
			inetAddr.SetFromString( szAddress );
		}
	}
	else
	{
		inetAddr.SetType( NA_BROADCAST );
		inetAddr.SetPort( 0 );
	}

	// Check if broadcasts are allowed
	if ( inetAddr.GetType() == NA_BROADCAST &&
		!net_allow_multicast.GetBool() )
		return;

	// Sending the connectionless packet
	g_pMatchExtensions->GetINetSupport()->SendPacket( NULL, eSock,
		inetAddr, msg.GetData(), msg.GetNumBytesWritten() );
}
