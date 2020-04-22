//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef NETCONSOLE_H
#define NETCONSOLE_H

#ifdef _WIN32
#pragma once
#endif

#ifdef _GAMECONSOLE
#define SUPPORT_NET_CONSOLE 0
#else
#define SUPPORT_NET_CONSOLE 1
#endif

#if SUPPORT_NET_CONSOLE
#include "tier2/socketcreator.h"



#define MAX_NETCONSOLE_INPUT_LEN 2048

class CConnectedNetConsoleData
{
public:
	SocketHandle_t m_hSocket;
	int m_nCharsInCommandBuffer;
	char m_pszInputCommandBuffer[MAX_NETCONSOLE_INPUT_LEN];
	bool m_bAuthorized;										// for password protection
	bool m_bInputOnly;										// if set, don't send spew to this netconsole

	CConnectedNetConsoleData( SocketHandle_t hSocket = -1 )
	{
		m_nCharsInCommandBuffer = 0;
		m_bAuthorized = false;
		m_hSocket = hSocket;
		m_bInputOnly = false;
	}
};


class CNetConsoleMgr : public ISocketCreatorListener
{
	CSocketCreator m_Socket;
	char m_pPassword[256];									// if set
	netadr_t m_Address;
	bool m_bActive;
	bool m_bPasswordProtected;
	
	CConnectedNetConsoleData m_ParentConnection;

	bool ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t &netAdr )
	{
		return true;
	}

	void OnSocketAccepted( SocketHandle_t hSocket, const netadr_t &netAdr, void** ppData )
	{
		CConnectedNetConsoleData *pData = new CConnectedNetConsoleData( hSocket );
		if ( ! m_bPasswordProtected )
		{
			pData->m_bAuthorized = true;				// no password, auto-auth
		}
		*ppData = pData;
	}

	void OnSocketClosed( SocketHandle_t hSocket, const netadr_t &netAdr, void* pData )
	{
		if ( pData )
			delete (CConnectedNetConsoleData*)pData;
	}

	void Execute( CConnectedNetConsoleData *pData );

	void HandleInputChars( char const *pChars, int nNumChars, CConnectedNetConsoleData *pData );

	int NumConnectedSockets( void )
	{
		int nRet = m_Socket.GetAcceptedSocketCount();
		if ( m_ParentConnection.m_hSocket != -1 )
		{
			++nRet;
		}
		return nRet;
	}

	CConnectedNetConsoleData *GetConnection( int nIdx )
	{
		if ( m_ParentConnection.m_hSocket != -1 )
		{
			if ( nIdx == 0 )
			{
				return &m_ParentConnection;
			}
			nIdx--;
		}

		CConnectedNetConsoleData *pData = ( CConnectedNetConsoleData * ) m_Socket.GetAcceptedSocketData( nIdx );
		return pData;
	}

	void CloseConnection( int nIdx )
	{
		if ( m_ParentConnection.m_hSocket != -1 )
		{
			if ( nIdx == 0 )
			{
				return;										// don't really close
			}
			nIdx--;
		}
		m_Socket.CloseAcceptedSocket( nIdx );
	}

public:
	void RunFrame( void );

	void SendStringToNetConsoles( char const *pString );

	CNetConsoleMgr( void );									// initialize from command line arguments

	CNetConsoleMgr( int nPort );							// init from expicity port number

	bool IsActive() const { return m_bActive; }
	const netadr_t& GetAddress() const { return m_Address; }
};


extern CNetConsoleMgr *g_pNetConsoleMgr;
#endif // support_netconsole


FORCEINLINE void SendStringToNetConsoles( char const *pMsg )
{
#if SUPPORT_NET_CONSOLE
	if ( g_pNetConsoleMgr )
		g_pNetConsoleMgr->SendStringToNetConsoles( pMsg );
#endif
}

#if SUPPORT_NET_CONSOLE
void InitNetConsole( void );
#else
FORCEINLINE void InitNetConsole( void )
{
}
#endif



#endif // if NETCONSOLE
