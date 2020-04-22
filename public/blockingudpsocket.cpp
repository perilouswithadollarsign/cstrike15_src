//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#if defined(_WIN32) && !defined(_X360)
#include <winsock.h>
#elif defined( _PS3 )
#include "blockingudpsocket.h"
#elif POSIX
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#define closesocket close
#endif

#include "blockingudpsocket.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#ifdef _PS3

CBlockingUDPSocket::CBlockingUDPSocket() : m_pImpl( NULL ), m_Socket( 0 ) {}
CBlockingUDPSocket::~CBlockingUDPSocket() {}

bool CBlockingUDPSocket::WaitForMessage( float timeOutInSeconds ) { return false; }
unsigned int CBlockingUDPSocket::ReceiveSocketMessage( struct sockaddr_in *packet_from, unsigned char *buf, size_t bufsize ) { return 0; }
bool CBlockingUDPSocket::SendSocketMessage( const struct sockaddr_in& rRecipient, const unsigned char *buf, size_t bufsize ) { return false; }

bool CBlockingUDPSocket::CreateSocket (void) { return false; }

#else

class CBlockingUDPSocket::CImpl	
{
public:
	struct sockaddr_in	m_SocketIP;
	fd_set				m_FDSet;
};

CBlockingUDPSocket::CBlockingUDPSocket() :
	m_cserIP(),
	m_Socket( 0 ),
	m_pImpl( new CImpl )
{
	CreateSocket(); 
}

CBlockingUDPSocket::~CBlockingUDPSocket()
{
	delete m_pImpl;
	closesocket( static_cast<unsigned int>( m_Socket )); 
}

bool CBlockingUDPSocket::CreateSocket (void)
{
	struct sockaddr_in	address;

	m_Socket = socket( PF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if ( m_Socket == INVALID_SOCKET )
	{
		return false;
	}

	address = m_pImpl->m_SocketIP;

	if ( SOCKET_ERROR == bind( m_Socket, ( struct sockaddr * )&address, sizeof( address ) ) )
	{
		return false;
	}

#ifdef _WIN32
	if ( m_pImpl->m_SocketIP.sin_addr.S_un.S_addr == INADDR_ANY )
	{
		m_pImpl->m_SocketIP.sin_addr.S_un.S_addr = 0L;
	}		
#elif POSIX
	if ( m_pImpl->m_SocketIP.sin_addr.s_addr == INADDR_ANY )
	{
		m_pImpl->m_SocketIP.sin_addr.s_addr = 0L;
	}
#endif

	return true;
}

bool CBlockingUDPSocket::WaitForMessage( float timeOutInSeconds )
{
	struct timeval tv;

	FD_ZERO( &m_pImpl->m_FDSet );
	FD_SET( m_Socket, &m_pImpl->m_FDSet );//lint !e717
	
	tv.tv_sec = (int)timeOutInSeconds;
	float remainder = timeOutInSeconds - (int)timeOutInSeconds;
	tv.tv_usec = (int)( remainder * 1000000 + 0.5f );         /* micro seconds */
	
	if ( SOCKET_ERROR == select( ( int )m_Socket + 1, &m_pImpl->m_FDSet, NULL, NULL, &tv ) )
	{
		return false;
	}

	if ( FD_ISSET( m_Socket, &m_pImpl->m_FDSet) )
	{
		return true;
	}

	// Timed out
	return false;
}

unsigned int CBlockingUDPSocket::ReceiveSocketMessage( struct sockaddr_in *packet_from, unsigned char *buf, size_t bufsize )
{
	memset( packet_from, 0, sizeof( *packet_from ) );

	struct sockaddr fromaddress;
	int		fromlen = sizeof( fromaddress );

	int packet_length = recvfrom
		(
		m_Socket, 
		(char *)buf, 
		(int)bufsize, 
		0, 
		&fromaddress, 
		(socklen_t *)&fromlen 
		);

	if ( SOCKET_ERROR == packet_length )
	{
		return 0;
	}

	// In case it's parsed as a string
	buf[ packet_length ] = 0;		

	// Copy over the receive address
	*packet_from = *( struct sockaddr_in * )&fromaddress;

	return ( unsigned int )packet_length;
}

bool CBlockingUDPSocket::SendSocketMessage( const struct sockaddr_in & rRecipient, const unsigned char *buf, size_t bufsize )
{
	// Send data
	int bytesSent = sendto
		(
		m_Socket, 
		(const char *)buf, 
		(int)bufsize, 
		0, 
		reinterpret_cast< const sockaddr * >( &rRecipient ), 
		sizeof( rRecipient ) 
		);

	if ( SOCKET_ERROR == bytesSent )
	{
		return false;
	}

	return true;
}

#endif

