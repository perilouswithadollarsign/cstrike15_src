//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef NET_WS_HEADERS_H
#define NET_WS_HEADERS_H
#ifdef _WIN32
#pragma once
#endif


#ifdef _WIN32
#if !defined( _X360 )
#include "winlite.h"
#endif
#endif

#include "vstdlib/random.h"
#include "convar.h"
#include "tier0/icommandline.h"
#include "filesystem_engine.h"
#include "proto_oob.h"
#include "net_chan.h"
#include "inetmsghandler.h"
#include "protocol.h" // CONNECTIONLESS_HEADER
#include "sv_filter.h"
#include "sys.h"
#include "tier0/tslist.h"
#include "tier1/mempool.h"
#include "../utils/bzip2/bzlib.h"

#if defined(_WIN32)

#if !defined( _X360 )
#include <winsock2.h>
#else
#include "winsockx.h"
#endif

// #include <process.h>
typedef int socklen_t;

#elif defined POSIX

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#ifdef _PS3
#include <netex/errno.h>
#include <netex/net.h>
#include <netex/libnetctl.h>
#include <sys/time.h>
#include <sys/select.h>
#define PF_INET AF_INET
#define WSA_SOCKET_ERROR_CODE_FIXUP( ecode ) SYS_NET_##ecode
#define select socketselect
#define WSAGetLastError() sys_net_errno
#else
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#define WSA_SOCKET_ERROR_CODE_FIXUP( ecode ) ecode
#define WSAGetLastError() errno
#endif
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef WSA_SOCKET_ERROR_CODE_FIXUP
#define WSAEWOULDBLOCK		WSA_SOCKET_ERROR_CODE_FIXUP( EWOULDBLOCK )
#define WSAEMSGSIZE			WSA_SOCKET_ERROR_CODE_FIXUP( EMSGSIZE )
#define WSAEADDRNOTAVAIL	WSA_SOCKET_ERROR_CODE_FIXUP( EADDRNOTAVAIL )
#define WSAEAFNOSUPPORT		WSA_SOCKET_ERROR_CODE_FIXUP( EAFNOSUPPORT )
#define WSAECONNRESET		WSA_SOCKET_ERROR_CODE_FIXUP( ECONNRESET )
#define WSAECONNREFUSED     WSA_SOCKET_ERROR_CODE_FIXUP( ECONNREFUSED )
#define WSAEADDRINUSE		WSA_SOCKET_ERROR_CODE_FIXUP( EADDRINUSE )
#define WSAENOTCONN			WSA_SOCKET_ERROR_CODE_FIXUP( ENOTCONN )
#endif

#define ioctlsocket ioctl
#define closesocket close

#undef SOCKET
typedef int SOCKET;
#define FAR

#ifdef _PS3
#define ioctl( s, cmd, pVal ) setsockopt( s, SOL_SOCKET, cmd, pVal, sizeof( *( pVal ) ) )
#define FIONBIO SO_NBIO
#endif

#endif

#include "sv_rcon.h"
#ifndef DEDICATED
#include "cl_rcon.h"
#endif

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

void Con_RunFrame( void );									// call to handle socket updates, etc.


#ifdef _PS3
//#define ONLY_USE_STEAM_SOCKETS 1
#endif

#ifdef ONLY_USE_STEAM_SOCKETS
#define OnlyUseSteamSockets() true
#else
#define OnlyUseSteamSockets() false
#endif

#endif // NET_WS_HEADERS_H
