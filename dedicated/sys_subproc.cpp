//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//


#ifdef _LINUX

// linux has a multi-processing forked server mode.
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>
#include <arpa/inet.h>
//#include <linux/tcp.h>
#include <netdb.h>
//#include <sys/param.h>
#include <sys/uio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "isys.h"
#include "dedicated.h"
#include "engine_hlds_api.h"
#include "filesystem.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "tier2/socketcreator.h"
#include "idedicatedexports.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include "mathlib/expressioncalculator.h"
#define closesocket close
#define WSAGetLastError() errno
#define ioctlsocket ioctl





// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static netadr_t	net_local_adr;
unsigned short NET_HostToNetShort( unsigned short us_in )
{
	return htons( us_in );
}

unsigned short NET_NetToHostShort( unsigned short us_in )
{
	return ntohs( us_in );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *s - 
//			*sadr - 
// Output : bool	NET_StringToSockaddr
//-----------------------------------------------------------------------------
bool NET_StringToSockaddr( const char *s, struct sockaddr *sadr )
{
	char	*colon;
	char	copy[128];
	
	Q_memset (sadr, 0, sizeof(*sadr));
	((struct sockaddr_in *)sadr)->sin_family = AF_INET;
	((struct sockaddr_in *)sadr)->sin_port = 0;

	Q_strncpy (copy, s, sizeof( copy ) );
	// strip off a trailing :port if present
	for (colon = copy ; *colon ; colon++)
	{
		if (*colon == ':')
		{
			*colon = 0;
			((struct sockaddr_in *)sadr)->sin_port = NET_HostToNetShort((short)atoi(colon+1));	
		}
	}
	
	if (copy[0] >= '0' && copy[0] <= '9' && Q_strstr( copy, "." ) )
	{
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = inet_addr(copy);
	}
	else
	{
		struct hostent	*h;
		if ( (h = gethostbyname(copy)) == NULL )
			return false;
		*(int *)&((struct sockaddr_in *)sadr)->sin_addr = *(int *)h->h_addr_list[0];
	}
	
	return true;
}

/*
=============
NET_StringToAdr

localhost
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
=============
*/
bool NET_StringToAdr ( const char *s, netadr_t *a)
{
	struct sockaddr saddr;

	char address[128];
	
	Q_strncpy( address, s, sizeof(address) );

	if ( !Q_strncmp( address, "localhost", 10 ) || !Q_strncmp( address, "localhost:", 10 ) )
	{
		// subsitute 'localhost' with '127.0.0.1", both have 9 chars
		// this way we can resolve 'localhost' without DNS and still keep the port
		Q_memcpy( address, "127.0.0.1", 9 );
	}

	
	if ( !NET_StringToSockaddr (address, &saddr) )
		return false;
		
	a->SetFromSockadr( &saddr );

	return true;
}


void NET_GetLocalAddress (void)
{
	net_local_adr.Clear();

	char	buff[512];

	gethostname( buff, sizeof(buff) );	// get own IP address
	buff[sizeof(buff)-1] = 0;			// Ensure that it doesn't overrun the buffer
	NET_StringToAdr (buff, &net_local_adr);

}


#define MAX_STATUS_STRING_LENGTH 1024
#define MAX_INPUT_FROM_CHILD 2048

class CConnectedNetConsoleData
{
public:
	int m_nCharsInCommandBuffer;
	char m_pszInputCommandBuffer[MAX_INPUT_FROM_CHILD];
	bool m_bAuthorized;										// for password protection
	CConnectedNetConsoleData( void )
	{
		m_nCharsInCommandBuffer = 0;
		m_bAuthorized = false;
	}
};

class CParentProcessNetConsoleMgr : public ISocketCreatorListener
{
public:
	CSocketCreator m_Socket;
	netadr_t m_Address;
	char m_pPassword[256];									// if set
	bool m_bPasswordProtected;
	bool m_bActive;

	bool ShouldAcceptSocket( SocketHandle_t hSocket, const netadr_t &netAdr )
	{
		return true;
	}

	void OnSocketAccepted( SocketHandle_t hSocket, const netadr_t &netAdr, void** ppData )
	{
		CConnectedNetConsoleData *pData = new CConnectedNetConsoleData;
		if ( ! m_bPasswordProtected )
			pData->m_bAuthorized = true;				// no password, auto-auth
		*ppData = pData;
	}

	void OnSocketClosed( SocketHandle_t hSocket, const netadr_t &netAdr, void* pData )
	{
		if ( pData )
			free( pData );
	}

	void RunFrame( void );
	
	CParentProcessNetConsoleMgr( void );

	void HandleInputChars( char const *pIn, int recvLen, CConnectedNetConsoleData *pData, int idx );

	void SendString( char const *pString, int idx = -1 );	// send a string to all sockets or just one

	void Execute( CConnectedNetConsoleData *pData, int idx );

};

void CParentProcessNetConsoleMgr::SendString( char const *pString, int nidx  )
{
	m_Socket.RunFrame();
	int nCount = m_Socket.GetAcceptedSocketCount();
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
			if ( ( nidx == -1 ) || ( i == nidx ) )
			{
				SocketHandle_t hSocket = m_Socket.GetAcceptedSocketHandle( i );
				//const netadr_t& socketAdr = m_Socket.GetAcceptedSocketAddress( i );
				CConnectedNetConsoleData *pData = ( CConnectedNetConsoleData * ) m_Socket.GetAcceptedSocketData( i );
				if ( pData->m_bAuthorized )						// no output to un-authed net consoles
				{
					send( hSocket, pTmp, oString - pTmp - 1, MSG_NOSIGNAL );
				}
			}
		}
	}
}


CParentProcessNetConsoleMgr::CParentProcessNetConsoleMgr( void ) : m_Socket( this )
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
		NET_GetLocalAddress();
		char const *pPortNum = CommandLine()->GetParm( nPort + 1 );
		char newBuf[256];
		V_strncpy( newBuf, pPortNum, sizeof( newBuf ) );
		char *pReplace = V_strstr( newBuf, "##" );
		if ( pReplace )
		{
			pReplace[0] = '0';
			pReplace[1] = '0';
		}
		m_Address = net_local_adr;
		int nPortNumber = EvaluateExpression( newBuf, -1 );
		if ( nPortNumber > 0 )
		{
			m_Address.SetPort( nPortNumber );
			m_bActive = true;
			m_Socket.CreateListenSocket( m_Address, true );
		}
	}
}

void CParentProcessNetConsoleMgr::RunFrame( void )
{
	// check for incoming data
	if (! m_bActive )
		return;
	m_Socket.RunFrame();
	int nCount = m_Socket.GetAcceptedSocketCount();
	for ( int i = nCount - 1; i >= 0; i-- )
	{
		SocketHandle_t hSocket = m_Socket.GetAcceptedSocketHandle( i );
		// const netadr_t& socketAdr = m_Socket.GetAcceptedSocketAddress( i );
		CConnectedNetConsoleData *pData = ( CConnectedNetConsoleData * ) m_Socket.GetAcceptedSocketData( i );
		char ch;
		int pendingLen = recv( hSocket, &ch, sizeof(ch), MSG_PEEK );
		if ( pendingLen == -1 && SocketWouldBlock() )
			continue;
			
		if ( pendingLen <= 0 )							// eof or error
		{
			m_Socket.CloseAcceptedSocket( i );
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
				m_Socket.CloseAcceptedSocket( i );
				break;
			}
				
			if ( recvLen < 0 && !SocketWouldBlock() )
			{
				break;
			}
				
			readLen -= recvLen;
			// now, lets write what we've got into the command buffer
			HandleInputChars( recvBuf, recvLen, pData, i );
		}
	}
}


void CParentProcessNetConsoleMgr::HandleInputChars( char const *pIn, int recvLen, CConnectedNetConsoleData *pData, int idx )
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
					Execute( pData, idx );
				}
				pData->m_nCharsInCommandBuffer = 0;
				break;
			}
			default:
			{
				if ( pData->m_nCharsInCommandBuffer < MAX_INPUT_FROM_CHILD - 1 )
					pData->m_pszInputCommandBuffer[pData->m_nCharsInCommandBuffer++] = *pIn;
				break;
			}
		}
		pIn++;
		recvLen--;
	}
}

struct CServerInstance
{
	pid_t m_nPid;
	int m_nSocketToChild;									// "our" side of the socket connection
	int m_nNumCharsInInputBuffer;
	int m_nNumPlayers;

	char m_pszStatus[MAX_STATUS_STRING_LENGTH];
	char m_pszMapName[MAX_PATH];
	char m_pszInputBuffer[MAX_INPUT_FROM_CHILD];
	bool m_bRunning;


	void ClearInputBuffer( void )
	{
		m_nNumCharsInInputBuffer = 0;
	}

	void ResetStatus( void )
	{
		m_pszMapName[0] = 0;
		m_nNumPlayers = 0;
	}
	CServerInstance( void )
	{
		m_pszStatus[0] = 0;									// clear status string
		m_bRunning = false;
		m_nSocketToChild = -1;
		ClearInputBuffer();
		ResetStatus();
	}

	void HandleSocketInput( void );

	void ProcessInputFromChild( void );

};

#define MAX_CHILD_PROCESSSES 100

CParentProcessNetConsoleMgr *g_pParentProcessNetConsole;
int g_nNumChildInstances;
CServerInstance *g_pChildProcesses;

static bool s_bQuit = false;
static bool s_bDelayedQuit = false;

typedef void (*CMDFN)( char const *pArgs, int nIdx );

struct CommandDescriptor
{
	char const *m_pCmdName;
	CMDFN m_pCmdFn;
	char const *m_pCmdHelp;
};

static char *va( char *format, ... )
{
	va_list		argptr;
	static char	string[8][512];
	static int	curstring = 0;
	
	curstring = ( curstring + 1 ) % 8;

	va_start (argptr, format);
	Q_vsnprintf( string[curstring], sizeof( string[curstring] ), format, argptr );
	va_end (argptr);

	return string[curstring];  
}

static void s_DoStatusCmd( char const *pArgs, int nConsoleIdx )
{
	// print status
	g_pParentProcessNetConsole->SendString( "#status\n", nConsoleIdx );
	for( int i = 0; i < g_nNumChildInstances; i++ )
	{
		CServerInstance *pChild = g_pChildProcesses + i;
		g_pParentProcessNetConsole->SendString( va( "child %d\n", i ), nConsoleIdx );
		if ( pChild && ( pChild->m_nSocketToChild != -1 ) )
		{
			g_pParentProcessNetConsole->SendString( va( " pid : %d\n", i, pChild->m_nPid ), nConsoleIdx );
			g_pParentProcessNetConsole->SendString( va( " map : %s\n", pChild->m_pszMapName ), nConsoleIdx );
			g_pParentProcessNetConsole->SendString( va( " numplayers : %d\n", pChild->m_nNumPlayers ), nConsoleIdx );
		}
	}
	g_pParentProcessNetConsole->SendString( "#end\n", nConsoleIdx );
}

static void s_DoQuit( char const *pArgs, int nConsoleIdx )
{
	g_pParentProcessNetConsole->SendString( "Killing all children and exiting\n", nConsoleIdx );
	for( int i = 0; i < g_nNumChildInstances; i++ )
	{
		CServerInstance *pChild = g_pChildProcesses + i;
		if ( pChild && ( pChild->m_nSocketToChild != -1 ) )
		{
			g_pParentProcessNetConsole->SendString( va( "killing child %d\n", i ), nConsoleIdx );
			kill( pChild->m_nPid, SIGKILL );
		}
	}
	s_bQuit = true;

}

static void s_DoBroadCastCmd( char const *pArgs, int nConsoleIdx )
{
	if ( ! pArgs )
	{
		g_pParentProcessNetConsole->SendString( "Format of command is \"broadcast <concommand>\"\n" );
	}
	else
	{
		for( int i = 0; i < g_nNumChildInstances; i++ )
		{
			CServerInstance *pChild = g_pChildProcesses + i;
			if ( pChild && ( pChild->m_nSocketToChild != -1 ) )
			{
				send( pChild->m_nSocketToChild, pArgs, strlen( pArgs ), MSG_NOSIGNAL );
				send( pChild->m_nSocketToChild, "\n", 1, MSG_NOSIGNAL );
			}
		}
	}
}

static void s_DoShutdown( char const *pArgs, int nConsoleIdx )
{
	s_bDelayedQuit = ! s_bDelayedQuit;
	if ( s_bDelayedQuit )
	{
		g_pParentProcessNetConsole->SendString( "Server will shutdown when all games are finished and children have exited.\n" );
	}
	else
	{
		g_pParentProcessNetConsole->SendString( "Server shutdown cancelled.\n" );
	}
	for( int i = 0; i < g_nNumChildInstances; i++ )
	{
		CServerInstance *pChild = g_pChildProcesses + i;
		if ( pChild && ( pChild->m_nSocketToChild != -1 ) )
		{
			if ( pChild->m_nNumPlayers == 0 )
			{
				kill( pChild->m_nPid, SIGKILL );
			}
		}
	}
}


static void s_DoFind( char const *pArgs, int nConsoleIdx );


static CommandDescriptor s_CmdTable[]={
	{ "status", s_DoStatusCmd, "List the status of all subprocesses." },
	{ "broadcast", s_DoBroadCastCmd, "Send a command to all subprocesses." },
	{ "find", s_DoFind, "find commands containing a string." },
	{ "shutdown", s_DoShutdown, "Tell the server shutdown once all players have left. This is a toggle." },
	{ "quit", s_DoQuit, "immediately shut down the server and all its child processes." },
};

static void s_DoFind( char const *pArgs, int nConsoleIdx )
{
	for( int i = 0; i < ARRAYSIZE( s_CmdTable ); i++ )
	{
		if ( ( pArgs[0] == 0 ) || ( V_stristr( s_CmdTable[i].m_pCmdName, pArgs ) ) )
		{
			g_pParentProcessNetConsole->SendString( va( "%s:\t%s\n", s_CmdTable[i].m_pCmdName, s_CmdTable[i].m_pCmdHelp) , nConsoleIdx );
		}
	}
}


void CParentProcessNetConsoleMgr::Execute( CConnectedNetConsoleData *pData, int idx )
{
	if ( memcmp( pData->m_pszInputCommandBuffer, "PASS ", 5 ) == 0 )
	{
		if ( V_strcmp( pData->m_pszInputCommandBuffer + 5, m_pPassword ) == 0 )
		{
			pData->m_bAuthorized = true;
		}

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
			char const *pCmd = pData->m_pszInputCommandBuffer;
			pCmd += strspn( pCmd, " \t" );
			char const *pArgs = strchr( pCmd, ' ' );
			int nCmdLen;
			if ( pArgs )
			{
				nCmdLen = pArgs - pCmd;
				pArgs += strspn( pArgs, " \t" );			// skip to first char of first word
			}
			else
			{
				nCmdLen = strlen( pCmd );
				pArgs = pCmd + strlen( pCmd );				// point at trailing 0 bytes
			}
			for( int i = 0; i < ARRAYSIZE( s_CmdTable ); i++ )
			{
				char const *pTblCmd = s_CmdTable[i].m_pCmdName;
				if ( ( strlen( pTblCmd ) == nCmdLen ) &&
					 ( memcmp( pTblCmd, pCmd, nCmdLen ) == 0 ) )
				{
					// found it
					( *s_CmdTable[i].m_pCmdFn )( pArgs, idx );
					break;
				}
			}
		}
		else
		{
			SendString( "This server is password protected. Enter PASS <passwd> for access\n", idx );
		}
	}
}






static void HandleDeadChildProcesses( void )
{
	for(;;)
	{
		int nStatus;
		pid_t nWait = waitpid( -1, &nStatus, WNOHANG );
		if ( nWait > 0 )
		{
			// find the process that exited
			CServerInstance *pFound = NULL;
			int nFound = -1;
			for( int i = 0; i < g_nNumChildInstances; i++ )
			{
				if ( g_pChildProcesses[i].m_nPid == nWait )
				{
					pFound = g_pChildProcesses + i;
					nFound = i;
					break;
				}
			}
			if ( ! pFound )
			{
				Warning( "unknown child process %d exited?\n", nWait );
			}
			else
			{
				if ( WIFEXITED( nStatus ) )
				{
					Msg( "Child %d exited with status %d\n", nFound, WEXITSTATUS( nStatus ) );
				}
				if ( WIFSIGNALED( nStatus ) )
				{
					Msg( "Child %d aborted with signal %d\n", nFound, WTERMSIG( nStatus ) );
				}
				if ( WCOREDUMP( nStatus ) )
				{
					Msg( "Child wrote a core dump\n");
				}
				pFound->m_bRunning = false;
				if ( pFound->m_nSocketToChild != -1 )
				{
					close( pFound->m_nSocketToChild );
					pFound->m_nSocketToChild = -1;
				}
			}
		}
		else
		{
			break;											// no dead children
		}
	}
}

#define MAX_ACTIVE_PARENT_NETCONSOLE_SOCKETS 20

static void ServiceChildProcesses( void )
{
	// for any children that aren't running (or not running yet), start them
	pollfd pollFds[MAX_CHILD_PROCESSSES + 1 + MAX_ACTIVE_PARENT_NETCONSOLE_SOCKETS ];
	int nPoll = 0;
	int nNumRunning = 0;
	for( int i = 0; i < g_nNumChildInstances; i++ )
	{
		if ( g_pChildProcesses[i].m_bRunning == false )
		{
			if (! s_bDelayedQuit )
			{
				int nSockets[2];
				int nRslt = socketpair( AF_UNIX, SOCK_STREAM, 0, nSockets );
				if ( nRslt != 0 )
				{
					Error( "socket pair returned error errno = %d\n", errno );
				}
				pid_t nChild = fork();
				if ( nChild == 0 )							// are we the forked child?
				{
					//ResetBaseTime();							// start plat_float time over at 0 for precision
					PerformCommandLineSubstitutions( i + 1 );
					close( nSockets[1] );
					engine->SetSubProcessID( i + 1, nSockets[0] );
					g_nSubProcessId = i + 1;
					RunServer( true );
					syscall( SYS_exit, 0 );					// we are not going to perform a normal c++ exit. We _dont_ want to run destructors, etc.
				}
				else
				{
					g_pChildProcesses[i].m_nPid = nChild;
					g_pChildProcesses[i].m_pszStatus[0] = 0;
					g_pChildProcesses[i].m_bRunning = true;
					close( nSockets[0] );
					g_pChildProcesses[i].m_nSocketToChild = nSockets[1];

				}
			}
		}
		else
		{
			nNumRunning++;
		}
		if ( g_pChildProcesses[i].m_nSocketToChild != -1 )
		{
			pollFds[nPoll].fd = g_pChildProcesses[i].m_nSocketToChild;
			pollFds[nPoll].events = POLLIN | POLLERR | POLLHUP;
			pollFds[nPoll].revents = 0;
			nPoll++;
		}
	}
	if ( s_bDelayedQuit && ( nNumRunning == 0 ) )
	{
		_exit( 0 );
	}

	// now, wait for activity on any of our sockets or stdin
// 	pollFds[nPoll].fd = STDIN_FILENO;
// 	pollFds[nPoll].events = POLLIN;
// 	pollFds[nPoll].revents = 0;
// 	nPoll++;
	if ( g_pParentProcessNetConsole && ( g_pParentProcessNetConsole->m_bActive ) )
	{
		pollFds[nPoll].fd = g_pParentProcessNetConsole->m_Socket.m_hListenSocket;
		pollFds[nPoll].events = POLLIN;
		pollFds[nPoll].revents = 0;
		nPoll++;

		int nCount = g_pParentProcessNetConsole->m_Socket.GetAcceptedSocketCount();
		for( int i = 0; ( i < nCount ) && ( nPoll < ARRAYSIZE( pollFds ) ); i++ )
		{
			SocketHandle_t hSocket = g_pParentProcessNetConsole->m_Socket.GetAcceptedSocketHandle( i );
			pollFds[nPoll].fd = hSocket;
			pollFds[nPoll].events = POLLIN;
			pollFds[nPoll].revents = 0;
			nPoll++;
		}
	}

	int nPollResult = poll( pollFds, nPoll, 10 * 1000 );		// wait up to 10 seconds. Could wait forever, really

	// check for activity on the sockets from our children
	int np = 0;
	for( int i = 0; i < g_nNumChildInstances; i++ )
	{
		if ( g_pChildProcesses[i].m_nSocketToChild != -1 )
		{
			if ( pollFds[np].revents & POLLIN )				// data ready to read?
			{
				g_pChildProcesses[i].HandleSocketInput();
			}
			np++;
		}

	}
	// see if any children have exited
	HandleDeadChildProcesses();

	g_pParentProcessNetConsole->RunFrame();


}




void RunServerSubProcesses( int nNumChildren )
{
	g_nNumChildInstances = nNumChildren;
	g_pChildProcesses = new CServerInstance[g_nNumChildInstances];
	g_pParentProcessNetConsole = new CParentProcessNetConsoleMgr;

	while( ! s_bQuit )
	{
		ServiceChildProcesses();
	}
	_exit( 0 );
}

static bool DecodeParam( char const *pParamName, char const *pInput, char const **pOutPtr )
{
	// if the left of the string matches pParamName, return the right of the string else return null
	int nPLen = strlen( pParamName );
	if ( memcmp( pParamName, pInput, nPLen ) == 0 )
	{
		*pOutPtr= pInput + nPLen;
	}
	else
	{
		*pOutPtr = NULL;
	}
	return ( *pOutPtr );
}

void CServerInstance::ProcessInputFromChild( void )
{
	if ( m_pszInputBuffer[0] == '#' )					// spew?
	{
		puts( m_pszInputBuffer );
	}
	else
	{
		char *pSpace = strchr( m_pszInputBuffer, ' ' );
		if ( pSpace )
		{
			*( pSpace++ ) = 0;
			pSpace += strspn( pSpace, " \t" );
		}
		else
		{
			pSpace = m_pszInputBuffer + strlen( m_pszInputBuffer );
		}
		if ( !strcmp( m_pszInputBuffer, "status" ) )
		{
			CUtlStringList statusRecords;
			V_SplitString( pSpace, ";", statusRecords );
			for( int i = 0; i < statusRecords.Count(); i++ )
			{
				char const *pRecord = statusRecords[i];
				char const *pParm;
				if ( DecodeParam( "map=", pRecord, &pParm ) )
				{
					V_strncpy( m_pszMapName, pParm, sizeof( m_pszMapName ) );
				}
				else if ( DecodeParam( "players=", pRecord, &pParm ) )
				{
					m_nNumPlayers = atoi( pParm );
				}
			}
		}
		else
		{
			Warning("got unknown cmd %s args %s\n", m_pszInputBuffer, pSpace  );
		}
	}
}



void CServerInstance::HandleSocketInput( void )
{
	char *pDest = m_pszInputBuffer + m_nNumCharsInInputBuffer;
	int nRead = recv( m_nSocketToChild, pDest, sizeof( m_pszInputBuffer ) - m_nNumCharsInInputBuffer, MSG_DONTWAIT );
	if ( nRead > 0 )
	{
		m_nNumCharsInInputBuffer += nRead;
		if ( m_pszInputBuffer[m_nNumCharsInInputBuffer - 1] == 0 )
		{
			ProcessInputFromChild();
			m_nNumCharsInInputBuffer = 0;
		}
		if ( m_nNumCharsInInputBuffer == MAX_INPUT_FROM_CHILD )
			m_nNumCharsInInputBuffer = 0;

	}


}



#endif //linux
