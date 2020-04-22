//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifdef _GAMECONSOLE
#define SUPPORT_NET_CONSOLE 0
#else
#define SUPPORT_NET_CONSOLE 1
#endif

#if SUPPORT_NET_CONSOLE

#if defined(_WIN32)
#if !defined(_X360)
#include "winlite.h"
#include <winsock2.h>
#endif
#undef SetPort // winsock screws with the SetPort string... *sigh*
#define MSG_NOSIGNAL 0

#elif POSIX

#ifdef OSX
#define MSG_NOSIGNAL 0 // doesn't exist on OSX, use SO_NOSIGPIPE socket option instead
#endif

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <sys/ioctl.h>
#define closesocket close
#define WSAGetLastError() errno
#define ioctlsocket ioctl

#endif // SUPPORT_NET_CONSOLE
#include "mathlib/expressioncalculator.h"

#include "client_pch.h"
#include <time.h>
#include "console.h"
#include "netconsole.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IVEngineClient *engineClient;

#if SUPPORT_NET_CONSOLE
void InitNetConsole( void )
{
	if ( !g_pNetConsoleMgr )
		g_pNetConsoleMgr = new CNetConsoleMgr;
}
#endif

CNetConsoleMgr::CNetConsoleMgr( void ) : m_Socket( this )
{
	m_bActive = false;
	m_bPasswordProtected = false;
	int nPassword = CommandLine()->FindParm( "-netconpassword" );
	if ( nPassword )
	{
		char const *pPassword = CommandLine()->GetParm( nPassword + 1 );
		V_strncpy( m_pPassword, pPassword, sizeof( m_pPassword ) );
		m_bPasswordProtected = true;
	}
	int nPort = CommandLine()->FindParm( "-netconport" );
	if ( nPort )
	{
		char const *pPortNum = CommandLine()->GetParm( nPort + 1 );
		m_Address = net_local_adr;
		int nPortNumber = EvaluateExpression( pPortNum, -1 );
		if ( nPortNumber > 0 )
		{
			m_Address.SetPort( nPortNumber );
			m_bActive = true;
			m_Socket.CreateListenSocket( m_Address, true );
		}
	}
	// now, handle cmds from parent process
	if ( g_nForkID > 0 )
	{
		int opt = 1;
		// set this socket to non-blocking
		ioctlsocket( g_nSocketToParentProcess, FIONBIO, (unsigned long*)&opt ); // non-blocking
		m_ParentConnection.m_hSocket = g_nSocketToParentProcess;
		m_ParentConnection.m_bAuthorized = true;			// no password needed from parent
		m_ParentConnection.m_bInputOnly = true;				// we don't want to spew to here
	}

}

CNetConsoleMgr::CNetConsoleMgr( int nPort ) : m_Socket( this )
{
	m_bPasswordProtected = false;
	m_Address.SetPort( nPort );
	m_bActive = true;
	m_Socket.CreateListenSocket( m_Address, true );
}


static char const s_pszPasswordMessage[]="This server is password protected for console access. Must send PASS command\n\r";

void CNetConsoleMgr::Execute( CConnectedNetConsoleData *pData )
{
	if ( memcmp( pData->m_pszInputCommandBuffer, "PASS ", 5 ) == 0 )
	{
		if ( V_strcmp( pData->m_pszInputCommandBuffer + 5, m_pPassword ) == 0 )
			pData->m_bAuthorized = true;
		else
		{
			// bad password
			Warning( "Bad password attempt from net console\n" );
			pData->m_bAuthorized = false;
		}
	}
	else
	{
		if ( pData->m_bAuthorized )
		{
#ifdef DEDICATED
			Cbuf_AddText(CBUF_SERVER, pData->m_pszInputCommandBuffer, kCommandSrcUserInput );
			Cbuf_Execute();
#else
			engineClient->ClientCmd_Unrestricted( pData->m_pszInputCommandBuffer, true );
#endif
		}
		else
		{
			SocketHandle_t hSocket = pData->m_hSocket;
#ifdef OSX
			int val = 1;
			setsockopt( hSocket, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val));
#endif	
			send( hSocket, s_pszPasswordMessage, strlen( s_pszPasswordMessage ), MSG_NOSIGNAL );
		}
	}
}

void CNetConsoleMgr::SendStringToNetConsoles( char const *pString )
{
	m_Socket.RunFrame();
	int nCount = NumConnectedSockets();
	if ( nCount )
	{
		// lets add the lf to any cr's
		char *pTmp = (char * ) stackalloc( strlen( pString ) * 2 + 1 );
		char *oString = pTmp;
		char const *pIn = pString;
		while ( *pIn )
		{
			if ( *pIn  == '\n' )
				*( oString++ ) = '\r';
			*( oString++ ) = *( pIn++ );
		} 
		*( oString++ ) = 0;

		for ( int i = 0; i < nCount; i++ )
		{
			CConnectedNetConsoleData *pData = GetConnection( i );
			if ( pData->m_bAuthorized && ( ! pData->m_bInputOnly ) ) // no output to un-authed net consoles
			{
#ifdef OSX
				int val = 1;
				setsockopt( pData->m_hSocket, SOL_SOCKET, SO_NOSIGPIPE, &val, sizeof(val));
#endif			
				send( pData->m_hSocket, pTmp, oString - pTmp - 1, MSG_NOSIGNAL );
			}
		}
	}
}

void CNetConsoleMgr::RunFrame( void )
{
	// check for incoming data
	m_Socket.RunFrame();
	int nCount = NumConnectedSockets();
	for ( int i = nCount - 1; i >= 0; i-- )
	{
		CConnectedNetConsoleData *pData = GetConnection( i );
		SocketHandle_t hSocket = pData->m_hSocket;
		char ch;
		int pendingLen = recv( hSocket, &ch, sizeof(ch), MSG_PEEK );
		if ( pendingLen == -1 && SocketWouldBlock() )
			continue;
			
		if ( pendingLen <= 0 )							// eof or error
		{
			CloseConnection( i );
			continue;
		}

		// find out how much we have to read
		unsigned long readLen;
		ioctlsocket( hSocket, FIONREAD, &readLen );
		while( readLen > 0  )
		{
			char recvBuf[256];
			int recvLen = recv( hSocket, recvBuf , MIN( sizeof( recvBuf ) , readLen ), 0 );
			if ( recvLen == 0 ) // socket was closed
			{
				CloseConnection( i );
				break;
			}
				
			if ( recvLen < 0 && !SocketWouldBlock() )
			{
				break;
			}
				
			readLen -= recvLen;
			// now, lets write what we've got into the command buffer
			HandleInputChars( recvBuf, recvLen, pData );
		}
	}
}


void CNetConsoleMgr::HandleInputChars( char const *pIn, int recvLen, CConnectedNetConsoleData *pData )
{
	while( recvLen )
	{
		switch( *pIn )
		{
			case '\r':
			case '\n':
			{
				if ( pData->m_nCharsInCommandBuffer )
				{
					pData->m_pszInputCommandBuffer[pData->m_nCharsInCommandBuffer] = 0;
					Execute( pData );
				}
				pData->m_nCharsInCommandBuffer = 0;
				break;
			}
			default:
			{
				if ( pData->m_nCharsInCommandBuffer < MAX_NETCONSOLE_INPUT_LEN - 1 )
					pData->m_pszInputCommandBuffer[pData->m_nCharsInCommandBuffer++] = *pIn;
				break;
			}
		}
		pIn++;
		recvLen--;
	}
}


CNetConsoleMgr *g_pNetConsoleMgr;

#endif // support_netconsole

