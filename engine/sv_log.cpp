//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "server_pch.h"
#include <time.h>
#include "server.h"
#include "sv_log.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "sv_main.h"
#include "tier0/icommandline.h"
#include <proto_oob.h>
#include "GameEventManager.h"
#include "netadr.h"
#include "sv_steamauth.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar sv_logsdir( "sv_logsdir", "logs", FCVAR_ARCHIVE, "Folder in the game directory where server logs will be stored." );
static ConVar sv_logfile( "sv_logfile", "1", FCVAR_ARCHIVE, "Log server information in the log file." );
static ConVar sv_logflush( "sv_logflush", "0", FCVAR_ARCHIVE, "Flush the log file to disk on each write (slow)." );
static ConVar sv_logecho( "sv_logecho", "1", FCVAR_ARCHIVE, "Echo log information to the console." );
static ConVar sv_log_onefile( "sv_log_onefile", "0", FCVAR_ARCHIVE, "Log server information to only one file." );
static ConVar sv_logbans( "sv_logbans", "0", FCVAR_ARCHIVE, "Log server bans in the server logs." ); // should sv_banid() calls be logged in the server logs?
static ConVar sv_logsecret( "sv_logsecret", "0", FCVAR_RELEASE, "If set then include this secret when doing UDP logging (will use 0x53 as packet type, not usual 0x52)" ); 
static ConVar sv_logsocket( "sv_logsocket", ( new CFmtStr( "%d", NS_SERVER ) )->Access(), FCVAR_RELEASE, "Uses a specific outgoing socket for sv udp logging" );
static ConVar sv_logsocket2( "sv_logsocket2", "1", FCVAR_RELEASE, "Uses a specific outgoing socket for second source of sv udp logging" );
static ConVar sv_logsocket2_substr( "sv_logsocket2_substr", "", FCVAR_RELEASE, "Uses a substring match for second source of sv udp logging" );

CLog g_Log;	// global Log object

CON_COMMAND( log, "Enables logging to file, console, and udp < on | off >." )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  log < on | off >\n" );

		if ( g_Log.IsActive() )
		{
			bool bHaveFirst = false;

			ConMsg( "currently logging to: " );

			if ( sv_logfile.GetInt() )
			{
				ConMsg( "file" );
				bHaveFirst = true;
			}

			if ( sv_logecho.GetInt() )
			{
				if ( bHaveFirst )
				{
					ConMsg( ", console" );
				}
				else
				{
					ConMsg( "console" );
					bHaveFirst = true;
				}
			}

			if ( g_Log.UsingLogAddress() )
			{
				if ( bHaveFirst )
				{
					ConMsg( ", udp" );
				}
				else
				{
					ConMsg( "udp" );
					bHaveFirst = true;
				}
			}
	
			if ( !bHaveFirst )
			{
				ConMsg( "no destinations! (file, console, or udp)\n" );	
				ConMsg( "check \"sv_logfile\", \"sv_logecho\", and \"logaddress_list\"" );	
			}

			ConMsg( "\n" );
		}
		else 
		{
			ConMsg( "not currently logging\n" );
		}
		return;
	}

	if ( !Q_stricmp( args[1], "off" ) || !Q_stricmp( args[1], "0" ) )
	{
		if ( g_Log.IsActive() )
		{
			g_Log.Close();
			g_Log.SetLoggingState( false );
			ConMsg( "Server logging disabled.\n" );
		}
	}
	else if ( !Q_stricmp( args[1], "on" ) || !Q_stricmp( args[1], "1" ) )
	{
		g_Log.SetLoggingState( true );
		ConMsg( "Server logging enabled.\n" );
		g_Log.Open();
	}
	else
	{
		ConMsg( "log:  unknown parameter %s, 'on' and 'off' are valid\n", args[1] );
	}
}

// changed log_addaddress back to logaddress_add to be consistent with GoldSrc
static void helper_logaddress_add( const CCommand &args, char const *szCommand, uint64 ullToken )
{
	netadr_t adr;
	const char *pszIP, *pszPort;

	if ( args.ArgC() != 4 && args.ArgC() != 2 )
	{
		ConMsg( "Usage:  %s ip:port\n", szCommand );
		return;
	}

	pszIP = args[1];

	if ( args.ArgC() == 4 )
	{
		pszPort	= args[3];
	}
	else
	{
		pszPort = Q_strstr( pszIP, ":" );

		// if we have "IP:port" as one argument inside quotes
		if ( pszPort )
		{
			// add one to remove the :
			pszPort++;
		}
		else
		{
			// default port
			pszPort = "27015";
		}
	}

	if ( !Q_atoi( pszPort ) )
	{
		ConMsg( "%s:  must specify a valid port\n", szCommand );
		return;
	}

	if ( !pszIP || !pszIP[0] )
	{
		ConMsg( "%s:  unparseable address\n", szCommand );
		return;
	}

	char szAdr[32];
	Q_snprintf( szAdr, sizeof( szAdr ), "%s:%s", pszIP, pszPort );

	if ( NET_StringToAdr( szAdr, &adr ) )
	{
		if ( g_Log.AddLogAddress( adr, ullToken ) )
		{
			if ( ullToken )
				ConMsg( "%s:  %s [%016llX]\n", szCommand, adr.ToString(), ullToken );
			else
				ConMsg( "%s:  %s\n", szCommand, adr.ToString() );
		}
		else
		{
			ConMsg( "%s:  %s is already in the list\n", szCommand, adr.ToString() );
		}
	}
	else
	{
		ConMsg( "%s:  unable to resolve %s\n", szCommand, szAdr );
	}
}

CON_COMMAND( logaddress_add, "Set address and port for remote host <ip:port>." )
{
	helper_logaddress_add( args, "logaddress_add", 0ull );
}

CON_COMMAND( logaddress_add_ex, "Set address and port for remote host <ip:port> and supplies a unique token in the UDP packets." )
{
	CRC64_t crcToken;
	CRC64_Init( &crcToken );
	
	const char *szCmdLine = CommandLine()->GetCmdLine();
	if ( szCmdLine && *szCmdLine )
		CRC64_ProcessBuffer( &crcToken, szCmdLine, V_strlen( szCmdLine ) );

	const char *szSteamToken = Steam3Server().GetAccountToken();
	if ( szSteamToken && *szSteamToken )
		CRC64_ProcessBuffer( &crcToken, szSteamToken, V_strlen( szSteamToken ) );

	const char *szLocalAdr = net_local_adr.ToString();
	if ( szLocalAdr && *szLocalAdr )
		CRC64_ProcessBuffer( &crcToken, szLocalAdr, V_strlen( szLocalAdr ) );

	CRC64_Final( &crcToken );
	if ( !crcToken )
		crcToken = 1;

	helper_logaddress_add( args, "logaddress_add_ex", crcToken );
}

static uint64 s_ullLogaddressTokenSecret = 0;
void FnChangeCallback_logaddress_token_secret( IConVar *var, const char *pOldValue, float flOldValue )
{
	ConVarRef cv( var );
	char const *szstring = cv.GetString();
	if ( !szstring || !*szstring )
	{
		ConMsg( "logaddress_token_secret:  must use a non-empty string for checksum\n" );
		return;
	}

	CRC64_ProcessBuffer( &s_ullLogaddressTokenSecret, szstring, V_strlen( szstring ) );
	if ( !s_ullLogaddressTokenSecret )
		s_ullLogaddressTokenSecret = 1;
	ConMsg( "logaddress_token_secret:  token checksum = %016llX\n", s_ullLogaddressTokenSecret );
}
ConVar logaddress_token_secret( "logaddress_token_secret", "", FCVAR_RELEASE, "Set a secret string that will be hashed when using logaddress with explicit token hash.", FnChangeCallback_logaddress_token_secret );
CON_COMMAND( logaddress_add_ts, "Set address and port for remote host <ip:port> and uses a unique checksum from logaddress_token_secret in the UDP packets." )
{
	if ( !s_ullLogaddressTokenSecret )
	{
		ConMsg( "logaddress_add_ts:  must set logaddress_token_secret before adding this log address\n" );
		return;
	}

	helper_logaddress_add( args, "logaddress_add_ts", s_ullLogaddressTokenSecret );
}

CON_COMMAND( logaddress_delall, "Remove all udp addresses being logged to" )
{
	g_Log.DelAllLogAddress();
}

CON_COMMAND( logaddress_del, "Remove address and port for remote host <ip:port>." )
{
	netadr_t adr;
	const char *pszIP, *pszPort;

	if ( args.ArgC() != 4 && args.ArgC() != 2 )
	{
		ConMsg( "Usage:  logaddress_del ip:port\n" );
		return;
	}

	pszIP = args[1];

	if ( args.ArgC() == 4 )
	{
		pszPort	= args[3];
	}
	else
	{
		pszPort = Q_strstr( pszIP, ":" );

		// if we have "IP:port" as one argument inside quotes
		if ( pszPort )
		{
			// add one to remove the :
			pszPort++;
		}
		else
		{
			// default port
			pszPort = "27015";
		}
	}

	if ( !Q_atoi( pszPort ) )
	{
		ConMsg( "logaddress_del:  must specify a valid port\n" );
		return;
	}

	if ( !pszIP || !pszIP[0] )
	{
		ConMsg( "logaddress_del:  unparseable address\n" );
		return;
	}

	char szAdr[32];
	Q_snprintf( szAdr, sizeof( szAdr ), "%s:%s", pszIP, pszPort );

	if ( NET_StringToAdr( szAdr, &adr ) )
	{
		if ( g_Log.DelLogAddress( adr ) )
		{
			ConMsg( "logaddress_del:  %s\n", adr.ToString() );
		}
		else
		{
			ConMsg( "logaddress_del:  address %s not found in the list\n", adr.ToString() );
		}
	}
	else
	{
		ConMsg( "logaddress_del:  unable to resolve %s\n", szAdr );
	}
}

CON_COMMAND( logaddress_list, "List all addresses currently being used by logaddress." )
{
	g_Log.ListLogAddress();
}

CLog::CLog()
{
	Reset();
	m_nDebugID = EVENT_DEBUG_ID_INIT;
}

CLog::~CLog()
{
	m_nDebugID = EVENT_DEBUG_ID_SHUTDOWN;
}

void CLog::Reset( void )	// reset all logging streams
{
	m_LogAddrDestinations.RemoveAll();
	m_hLogFile = FILESYSTEM_INVALID_HANDLE;
	m_bActive = false;
	m_flLastLogFlush = realtime;
	m_bFlushLog = false;

	if ( CommandLine()->CheckParm( "-flushlog" ) )
	{
		m_bFlushLog = true;
	}
}

void CLog::Init( void )
{
	Reset();

	// listen to these events
	g_GameEventManager.AddListener( this, "server_spawn", true );
	g_GameEventManager.AddListener( this, "server_shutdown", true );
	g_GameEventManager.AddListener( this, "server_cvar", true );
	g_GameEventManager.AddListener( this, "server_message", true );
	g_GameEventManager.AddListener( this, "server_addban", true );
	g_GameEventManager.AddListener( this, "server_removeban", true );
}

void CLog::Shutdown()
{
	Close();
	Reset();
	g_GameEventManager.RemoveListener( this );
}

void CLog::SetLoggingState( bool state )
{
	m_bActive = state;
}

void CLog::RunFrame() 
{
	if ( m_bFlushLog && m_hLogFile != FILESYSTEM_INVALID_HANDLE && ( realtime - m_flLastLogFlush ) > 1.0f )
	{
		m_flLastLogFlush = realtime;
		g_pFileSystem->Flush( m_hLogFile );
	}
}

bool CLog::AddLogAddress( netadr_t addr, uint64 ullToken )
{
	for ( int i = 0; i < m_LogAddrDestinations.Count(); ++i )
	{
		if ( m_LogAddrDestinations.Element(i).m_adr.CompareAdr(addr, false) )
		{
			if ( m_LogAddrDestinations.Element( i ).m_ullToken == ullToken )
				return false;

			m_LogAddrDestinations.Element( i ).m_ullToken = ullToken;
			return true;
		}
	}

	LogAddressDestination_t dest = { addr, ullToken };
	m_LogAddrDestinations.AddToTail( dest );
	return true;
}

bool CLog::DelLogAddress(netadr_t addr)
{
	int i = 0;
	
	for ( i = 0; i < m_LogAddrDestinations.Count(); ++i )
	{
		if ( m_LogAddrDestinations.Element(i).m_adr.CompareAdr(addr, false) )
		{
			// found!
			break;
		}
	}

	if ( i < m_LogAddrDestinations.Count() )
	{
		m_LogAddrDestinations.Remove(i);
		return true;
	}

	return false;
}

void CLog::ListLogAddress( void )
{
	LogAddressDestination_t *pElement;
	int count = m_LogAddrDestinations.Count();

	if ( count <= 0 )
	{
		ConMsg( "logaddress_list:  no addresses in the list\n" );
	}
	else
	{
		if ( count == 1 )
		{
			ConMsg( "logaddress_list: %i entry\n", count );
		}
		else
		{
			ConMsg( "logaddress_list: %i entries\n", count );
		}

		for ( int i = 0 ; i < count ; ++i )
		{
			pElement = &m_LogAddrDestinations.Element(i);
			if ( pElement->m_ullToken )
				ConMsg( "%s [%016llX]\n", pElement->m_adr.ToString(), pElement->m_ullToken );
			else
				ConMsg( "%s\n", pElement->m_adr.ToString() );
		}
	}
}

bool CLog::UsingLogAddress( void )
{
	return ( m_LogAddrDestinations.Count() > 0 );
}

void CLog::DelAllLogAddress( void )
{
	if ( m_LogAddrDestinations.Count() > 0 )
	{
		ConMsg( "logaddress_delall:  all addresses cleared\n" );
		m_LogAddrDestinations.RemoveAll();
	}
	else
	{
		ConMsg( "logaddress_delall:  no addresses in the list\n" );
	}
}

/*
==================
Log_PrintServerVars

==================
*/
void CLog::PrintServerVars( void )
{
	if ( !IsActive() )
	{
		return;
	}

	Printf( "server cvars start\n" );
	// Loop through cvars...
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsCommand() )
			continue;

		if ( !( var->IsFlagSet( FCVAR_NOTIFY ) ) )
			continue;

		Printf( "\"%s\" = \"%s\"\n", var->GetName(), ((ConVar*)var)->GetString() );
	}

	Printf( "server cvars end\n" );
}

bool CLog::IsActive( void )
{
	return m_bActive;
}

/*
==================
Log_Printf

Prints a log message to the server's log file, console, and possible a UDP address
==================
*/
void CLog::Printf( const char *fmt, ... )
{
	va_list			argptr;
	static char		string[1024];
	
	if ( !IsActive() )
	{
		return;
	}

	va_start ( argptr, fmt );
	Q_vsnprintf ( string, sizeof( string ), fmt, argptr );
	va_end   ( argptr );

	Print( string );
}

void CLog::Print( const char * text )
{
	static char	string[1100];

	if ( !IsActive() || !text || !text[0] )
	{
		return;
	}

	if ( Q_strlen( text ) > 1024 )
	{
		DevMsg( 1, "CLog::Print: string too long (>1024 bytes)." );
		return;
	}

	serverGameDLL->LogForHTTPListeners( text );

	tm today;
	Plat_GetLocalTime( &today );

	Q_snprintf( string, sizeof( string ), "L %02i/%02i/%04i - %02i:%02i:%02i: %s",
		today.tm_mon+1, today.tm_mday, 1900 + today.tm_year,
		today.tm_hour, today.tm_min, today.tm_sec, text );

	// Echo to server console
	if ( sv_logecho.GetInt() ) 
	{
		ConMsg( "%s", string );
	}

	// Echo to log file
	if ( sv_logfile.GetInt() && ( m_hLogFile != FILESYSTEM_INVALID_HANDLE ) )
	{
		g_pFileSystem->FPrintf( m_hLogFile, "%s", string );
		if ( sv_logflush.GetBool() )
		{
			g_pFileSystem->Flush( m_hLogFile );
		}
	}

	// Echo to UDP port
	if ( m_LogAddrDestinations.Count() > 0 )
	{
		// out of band sending
		for ( int i = 0 ; i < m_LogAddrDestinations.Count() ; i++ )
		{
			int nSocketFrom = sv_logsocket.GetInt();
			if ( sv_logsocket2_substr.GetString()[0] )
			{
				// Check if we should use secondary outgoing socket?
				if ( V_strstr( m_LogAddrDestinations.Element( i ).m_adr.ToString(), sv_logsocket2_substr.GetString() ) )
					nSocketFrom = sv_logsocket2.GetInt();
			}
			
			char chTokenBuffer[64] = {};
			if ( m_LogAddrDestinations.Element( i ).m_ullToken )
				V_sprintf_safe( chTokenBuffer, "T%016llX ", m_LogAddrDestinations.Element( i ).m_ullToken );
			else
				chTokenBuffer[0] = 0;

			if ( sv_logsecret.GetInt() != 0 )
				NET_OutOfBandPrintf( nSocketFrom, m_LogAddrDestinations.Element(i).m_adr, "%c%s%s%s", S2A_LOGSTRING2, sv_logsecret.GetString(), chTokenBuffer, string );
			else
				NET_OutOfBandPrintf( nSocketFrom, m_LogAddrDestinations.Element(i).m_adr, "%c%s%s", S2A_LOGSTRING, chTokenBuffer, string );
		}
	}
}

void CLog::FireGameEvent( IGameEvent *event )
{
	if ( !IsActive() )
	{
		return;
	}

	// log server events

	const char * name = event->GetName();

	if ( !name || !name[0])
	{
		return;
	}

	if ( Q_strcmp(name, "server_spawn") == 0 )
	{
		Printf( "Started map \"%s\" (CRC \"%i\")\n", sv.GetMapName(), ( int ) sv.worldmapCRC );
	}

	else if ( Q_strcmp(name, "server_shutdown") == 0 )
	{
		Printf( "server_message: \"%s\"\n", event->GetString("reason") );
	}

	else if ( Q_strcmp(name, "server_cvar") == 0 )
	{
		Printf( "server_cvar: \"%s\" \"%s\"\n", event->GetString("cvarname"), event->GetString("cvarvalue")  );
	}

	else if ( Q_strcmp(name, "server_message") == 0 )
	{
		Printf( "server_message: \"%s\"\n", event->GetString("text") );
	}
	
	else if ( Q_strcmp(name, "server_addban") == 0 )
	{
		if ( sv_logbans.GetInt() > 0 )
		{
			const int userid = event->GetInt( "userid" );
			const char *pszName = event->GetString( "name" );
			const char *pszNetworkid = event->GetString( "networkid" );	
			const char *pszIP = event->GetString( "ip" );	
			const char *pszDuration = event->GetString( "duration" );
			const char *pszCmdGiver = event->GetString( "by" );
			const char *pszResult = NULL;
			
			if ( Q_strlen( pszIP ) > 0 )
			{
				pszResult = event->GetInt( "kicked" ) > 0 ? "was kicked and banned by IP" : "was banned by IP";

				if ( userid > 0 )
				{
					Printf( "Addip: \"%s<%i><%s><>\" %s \"%s\" by \"%s\" (IP \"%s\")\n", 
										pszName,
										userid, 
										pszNetworkid, 
										pszResult,
										pszDuration,
										pszCmdGiver,
										pszIP );
				}
				else
				{
					Printf( "Addip: \"<><><>\" %s \"%s\" by \"%s\" (IP \"%s\")\n", 
										pszResult,
										pszDuration,
										pszCmdGiver,
										pszIP );
				}
			}
			else
			{
				pszResult = event->GetInt( "kicked" ) > 0 ? "was kicked and banned" : "was banned";

				if ( userid > 0 )
				{
					Printf( "Banid: \"%s<%i><%s><>\" %s \"%s\" by \"%s\"\n", 
										pszName,
										userid, 
										pszNetworkid, 
										pszResult,
										pszDuration,
										pszCmdGiver );
				}
				else
				{
					Printf( "Banid: \"<><%s><>\" %s \"%s\" by \"%s\"\n", 
										pszNetworkid, 
										pszResult,
										pszDuration,
										pszCmdGiver );
				}
			}
		}
	}

	else if ( Q_strcmp(name, "server_removeban") == 0 )
	{
		if ( sv_logbans.GetInt() > 0 )
		{
			const char *pszNetworkid = event->GetString( "networkid" );	
			const char *pszIP = event->GetString( "ip" );	
			const char *pszCmdGiver = event->GetString( "by" );
			
			if ( Q_strlen( pszIP ) > 0 )
			{
				Printf( "Removeip: \"<><><>\" was unbanned by \"%s\" (IP \"%s\")\n", 
								pszCmdGiver,
								pszIP );
			}
			else
			{
				Printf( "Removeid: \"<><%s><>\" was unbanned by \"%s\"\n", 
								pszNetworkid, 
								pszCmdGiver );
			}
		}		
	}
}

int CLog::GetEventDebugID( void )
{
	return m_nDebugID;
}

/*
====================
Log_Close

Close logging file
====================
*/
void CLog::Close( void )
{
	if ( m_hLogFile != FILESYSTEM_INVALID_HANDLE )
	{
		Printf( "Log file closed\n" );
		g_pFileSystem->Close( m_hLogFile );
	}

	m_hLogFile = FILESYSTEM_INVALID_HANDLE;
}

/*
====================
Log_Open

Open logging file
====================
*/
void CLog::Open( void )
{
	char szFileBase[ MAX_OSPATH ];
	char szTestFile[ MAX_OSPATH ];
	int i;
	FileHandle_t fp = 0;

	if ( !m_bActive || !sv_logfile.GetInt() )
	{
		return;
	}

	// do we already have a log file (and we only want one)?
	if ( m_hLogFile && sv_log_onefile.GetInt() )
	{
		return;		
	}

	Close();

	// Find a new log file slot
	tm today;
	Plat_GetLocalTime( &today );
	const char *pszLogsDir = sv_logsdir.GetString();

	// safety check for invalid paths
	if ( !COM_IsValidPath( pszLogsDir ) )
	{
		pszLogsDir = "logs";
	}
		
	// Build a filename that will remain unique even if all games everywhere piled in to the same directory
	ConVar *hostip = cvar->FindVar( "hostip" );
	int ip = hostip->GetInt();
	int ipparts[4];
	ipparts[0] = (ip >> 24) & 0xff;
	ipparts[1] = (ip >> 16) & 0xff;
	ipparts[2] = (ip >> 8) & 0xff;
	ipparts[3] = (ip) & 0xff;
	int port = NET_GetUDPPort( NS_SERVER );

	Q_snprintf( szFileBase, sizeof( szFileBase ), "%s/L%03i_%03i_%03i_%03i_%i_%04i%02i%02i%02i%02i_", 
		pszLogsDir, ipparts[0], ipparts[1], ipparts[2], ipparts[3], port, today.tm_year + 1900, today.tm_mon + 1, today.tm_mday, today.tm_hour, today.tm_min );

	for ( i = 0; i < 1000; i++ )
	{
		Q_snprintf( szTestFile, sizeof( szTestFile ), "%s%03i.log", szFileBase, i );

		Q_FixSlashes( szTestFile );
		COM_CreatePath( szTestFile );

		fp = g_pFileSystem->Open( szTestFile, "r", "LOGDIR" );
		if ( !fp )
		{
			COM_CreatePath( szTestFile );

			fp = g_pFileSystem->Open( szTestFile, "wt", "LOGDIR" );
			if ( !fp )
			{
				i = 1000;
			}
			else
			{
				ConMsg( "Server logging data to file %s\n", szTestFile );
			}
			break;
		}
		g_pFileSystem->Close( fp );
	}

	if ( i == 1000 )
	{
		ConMsg( "Unable to open logfiles under %s\nLogging disabled\n", szFileBase );
		return;
	}

	if ( fp )
	{
		m_hLogFile = fp;
	}
	Printf( "Log file started (file \"%s\") (game \"%s\") (version \"%i\")\n", szTestFile, com_gamedir, build_number() );
}


