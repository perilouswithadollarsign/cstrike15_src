//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//


#include "server_pch.h"
#include "ipuserfilter.h"
#include "sv_filter.h"
#include "sv_steamauth.h"
#include "GameEventManager.h"
#include "proto_oob.h"
#include "tier1/commandbuffer.h"
#include "net.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar sv_filterban( "sv_filterban", "1", 0, "Set packet filtering by IP mode" );

CUtlVector< ipfilter_t > g_IPFilters;
CUtlVector< userfilter_t > g_UserFilters;

#define BANNED_IP_FILENAME "banned_ip.cfg"
#define BANNED_USER_FILENAME "banned_user.cfg"
#define CONFIG_DIR "cfg/"
#define STEAM_PREFIX "STEAM_"

//-----------------------------------------------------------------------------
// Purpose: Sends a message to prospective clients letting them know they're banned
// Input  : *adr - 
//-----------------------------------------------------------------------------
void Filter_SendBan( const ns_address& adr )
{
	NET_OutOfBandPrintf( NS_SERVER, adr, "%cBanned by server\n", A2A_PRINT );
}

//-----------------------------------------------------------------------------
// Purpose: Checks an IP address to see if it is banned
// Input  : *adr - 
// Output : bool
//-----------------------------------------------------------------------------
bool Filter_ShouldDiscard( const ns_address& adr )
{
	if ( sv_filterban.GetInt() == 0 )
	{
		return false;
	}

	bool bNegativeFilter = sv_filterban.GetInt() == 1;

	if ( !adr.IsType<netadr_t>() )
		return false;

	unsigned in = adr.AsType<netadr_t>().GetIPNetworkByteOrder();

	// Handle timeouts 
	for ( int i = g_IPFilters.Count() - 1 ; i >= 0 ; i--)
	{
		if ( ( g_IPFilters[i].compare != 0xffffffff) &&
			 ( g_IPFilters[i].banEndTime != 0.0f ) &&
			 ( g_IPFilters[i].banEndTime <= realtime ) )
		{
			g_IPFilters.Remove(i);
			continue;
		}

		// Only get here if ban is still in effect.
		if ( (in & g_IPFilters[i].mask) == g_IPFilters[i].compare)
		{
			return bNegativeFilter;
		}
	}

	return !bNegativeFilter;
}

//-----------------------------------------------------------------------------
// Purpose: Takes an IP address string and fills in an ipfilter_t mask and compare (raw address)
// Input  : *s - 
//			*f - 
// Output : bool Filter_ConvertString
//-----------------------------------------------------------------------------
bool Filter_ConvertString( const char *s, ipfilter_t *f )
{
	char	num[128];
	int		i, j;
	byte	b[4];
	byte	m[4];
	
	for (i=0 ; i<4 ; i++)
	{
		b[i] = 0;
		m[i] = 0;
	}
	
	for (i=0 ; i<4 ; i++)
	{
		if (*s < '0' || *s > '9')
		{
			ConMsg("Bad filter address: %s\n", s);
			return false;
		}
		
		j = 0;
		while (*s >= '0' && *s <= '9')
		{
			num[j++] = *s++;
		}
		num[j] = 0;
		b[i] = atoi(num);
		if (b[i] != 0)
			m[i] = 255;

		if (!*s)
			break;
		s++;
	}
	
	f->mask = *(unsigned int *)m;
	f->compare = *(unsigned int *)b;
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Adds an IP ban
//-----------------------------------------------------------------------------
static void Filter_Add_f( const CCommand& args )
{
	int			i = 0;
	float		banTime;
	bool		bKick = true;
	bool		bFound = false;
	char		szDuration[256];
	CGameClient *client = NULL;

	if ( !Q_stricmp( args[0], "banip" ) )
	{
		ConMsg( "Note: should use \"addip\" instead of \"banip\".\n" );
	}

	if ( args.ArgC() != 3 )
	{
		ConMsg( "Usage:  addip < minutes > < ipaddress >\nUse 0 minutes for permanent\n" );
		return;
	}

	ipfilter_t	f;
	if ( !Filter_ConvertString( args[2], &f ) )
		return;

	for (i=0 ; i<g_IPFilters.Count(); i++)
	{
		if ( g_IPFilters[i].compare == 0xffffffff || ( g_IPFilters[i].compare == f.compare && g_IPFilters[i].mask == f.mask ) )
			break;		// free spot
	}
	
	if (i == g_IPFilters.Count())
	{
		if (g_IPFilters.Count() == MAX_IPFILTERS)
		{
			ConMsg( "addip:  IP filter list is full\n" );
			return;
		}

		i = g_IPFilters.AddToTail();
	}
	else
	{
		// updating in-place, so don't kick people
		bKick = false;
	}
	
	banTime = atof( args[1] );
	if (banTime < 0.01f)
	{
		banTime = 0.0f;
	}
	
	g_IPFilters[i].banTime = banTime;

	// Time to unban.
	g_IPFilters[i].banEndTime = ( banTime != 0.0 ) ? ( realtime + 60.0 * banTime ) : 0.0;

	if ( !Filter_ConvertString( args[2], &g_IPFilters[i]) )
	{
		g_IPFilters[i].compare = 0xffffffff;
	}

	if ( bKick )
	{
		// Kick him if he's on
		for ( i=0; i < sv.GetClientCount(); ++i )
		{
			client = sv.Client(i);
			if ( !client || !client->IsActive() || !client->IsConnected() || !client->IsSpawned() )
				continue;

			if ( client->IsFakeClient() )
				continue;

			if ( !Filter_ShouldDiscard( client->GetNetChannel()->GetRemoteAddress() ) )
				continue;

			bFound = true;
			break;
		}
	}

	// Build a duration string for the ban
	if ( banTime == 0.0 )
	{
		Q_snprintf( szDuration, sizeof( szDuration ), "permanently" );
	}
	else
	{
		Q_snprintf( szDuration, sizeof( szDuration ), "for %.2f minutes", banTime );
	}
	
	// fire the event

	IGameEvent *event = g_GameEventManager.CreateEvent( "server_addban" );

	if ( event )
	{
		if ( bFound && client )
		{
			event->SetString( "name", client->m_Name );
			event->SetInt( "userid", client->GetUserID() );
			event->SetString( "networkid", client->GetNetworkIDString() );
		}
		else
		{
			event->SetString( "name", "" );
			event->SetInt( "userid", 0 );
			event->SetString( "networkid", "" );
		}

		event->SetString( "ip", args[2] );
		event->SetString( "duration", szDuration );
		event->SetString( "by", ( args.Source() != kCommandSrcNetClient ) ? "Console" : host_client->m_Name );
		event->SetBool( "kicked", bKick && bFound && client  );

		g_GameEventManager.FireEvent( event );
	}

	if ( bKick && bFound && client )
	{
		client->ClientPrintf ( "The server operator has added you to the banned list.\n" );
		client->Disconnect( "Added to banned list" );
	}
}

// IP Address filtering ConCommands
static ConCommand addip( "addip", Filter_Add_f, "Add an IP address to the ban list." );
static ConCommand banip( "banip", Filter_Add_f, "Add an IP address to the ban list." );


//-----------------------------------------------------------------------------
// Purpose: Removes an IP ban
//-----------------------------------------------------------------------------
CON_COMMAND( removeip, "Remove an IP address from the ban list." )
{
	ipfilter_t	f;
	int			i;

	if ( args.ArgC() < 1 )
	{
		ConMsg( "Usage:  removeip < slot | ipaddress >\n" );
		return;
	}

	// if no "." in the string we'll assume it's a slot number
	if ( !Q_strstr( args[1], "." ) )
	{
		int slot = Q_atoi( args[1] );
		if ( slot > 0 && slot <= g_IPFilters.Count() )
		{
			byte b[4];
			char szIP[32];

			// array access is zero based
			slot--;

			*(unsigned *)b = g_IPFilters[slot].compare;
			Q_snprintf( szIP, sizeof( szIP ), "%3i.%3i.%3i.%3i", b[0], b[1], b[2], b[3] );

			g_IPFilters.Remove( slot );

			// Tell server operator
			ConMsg( "removeip:  filter removed for %s, IP %s\n", args[1], szIP );

			// send an event
			IGameEvent *event = g_GameEventManager.CreateEvent( "server_removeban" );
			if ( event )
			{
				event->SetString( "networkid", "" );
				event->SetString( "ip", szIP );
				event->SetString( "by", ( args.Source() != kCommandSrcNetClient ) ? "Console" : host_client->m_Name );

				g_GameEventManager.FireEvent( event );
			}
		}
		else
		{
			ConMsg( "removeip:  invalid slot %i\n", slot );
		}

		return;
	}

	if ( !Filter_ConvertString( args[1], &f ) )
		return;

	for ( i = 0 ; i < g_IPFilters.Count() ; i++ )
	{
		if ( ( g_IPFilters[i].mask == f.mask ) &&
			 ( g_IPFilters[i].compare == f.compare ) )
		{
			g_IPFilters.Remove(i);
			ConMsg( "removeip:  filter removed for %s\n", args[1] );

			// send an event
			IGameEvent *event = g_GameEventManager.CreateEvent( "server_removeban" );

			if ( event )
			{
				event->SetString( "networkid", "" );
				event->SetString( "ip", args[1] );
				event->SetString( "by", ( args.Source() != kCommandSrcNetClient ) ? "Console" : host_client->m_Name );
				g_GameEventManager.FireEvent( event );
			}

			return;
		}
	}
	ConMsg( "removeip:  couldn't find %s\n", args[1] );
}


//-----------------------------------------------------------------------------
// Purpose: Lists IP bans
//-----------------------------------------------------------------------------
CON_COMMAND( listip, "List IP addresses on the ban list." )
{
	int		i;
	byte	b[4];
	int		count = g_IPFilters.Count();

	if ( !count )
	{
		ConMsg( "IP filter list: empty\n" );
		return;
	}
	else
	{
		if ( count == 1 )
		{
			ConMsg( "IP filter list: %i entry\n", count );
		}
		else
		{
			ConMsg( "IP filter list: %i entries\n", count );
		}
	}

	for ( i = 0 ; i < count ; i++ )
	{
		*(unsigned *)b = g_IPFilters[i].compare;

		if ( g_IPFilters[i].banTime != 0.0f )
		{
			ConMsg( "%i %3i.%3i.%3i.%3i : %.3f min\n", i+1, b[0], b[1], b[2], b[3], g_IPFilters[i].banTime );
		}
		else
		{
			ConMsg( "%i %3i.%3i.%3i.%3i : permanent\n", i+1, b[0], b[1], b[2], b[3] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Saves IP bans to a file
//-----------------------------------------------------------------------------
CON_COMMAND( writeip, "Save the ban list to " BANNED_IP_FILENAME "." )
{
	FileHandle_t f;
	char	name[MAX_OSPATH];
	byte	b[4];
	int		i;
	float banTime;

	Q_strncpy( name, CONFIG_DIR BANNED_IP_FILENAME, sizeof( name ) );

	ConMsg( "Writing %s.\n", name );

	f = g_pFileSystem->Open ( name, "wb" );
	if ( !f )
	{
		ConMsg( "Couldn't open %s\n", name );
		return;
	}
	
	for ( i = 0 ; i < g_IPFilters.Count() ; i++ )
	{
		*(unsigned *)b = g_IPFilters[i].compare;

		// Only store out the permanent bad guys from this server.
		banTime = g_IPFilters[i].banTime;
		
		if ( banTime != 0.0f )
		{
			continue;
		}

		g_pFileSystem->FPrintf( f, "addip 0 %i.%i.%i.%i\r\n", b[0], b[1], b[2], b[3] );
	}
	
	g_pFileSystem->Close( f );
}



//-----------------------------------------------------------------------------
// Purpose: Checks a USERID_t to see if the Steam ID has been banned
//-----------------------------------------------------------------------------
bool Filter_IsUserBanned( const USERID_t& userid )
{
	if ( sv_filterban.GetInt() == 0 )
		return false;

	bool bNegativeFilter = sv_filterban.GetInt() == 1;
	
	// Handle timeouts 
	for ( int i =g_UserFilters.Count() - 1 ; i >= 0 ; i-- )
	{
		// Time out old filters
		if ( ( g_UserFilters[i].banEndTime != 0.0f ) &&
			 ( g_UserFilters[i].banEndTime <= realtime ) )
		{
			g_UserFilters.Remove( i );
			continue;
		}

		// Only get here if ban is still in effect.
		if ( Steam3Server().CompareUserID( userid, g_UserFilters[i].userid ) )
		{
			return bNegativeFilter;
		}
	}

	return !bNegativeFilter;
}

//-----------------------------------------------------------------------------
// Purpose: Converts a "STEAM_X:Y:Z" string into a USERID_t
//-----------------------------------------------------------------------------
USERID_t *Filter_StringToUserID( const char *str )
{
	static USERID_t id;
	Q_memset( &id, 0, sizeof( id ) );

	if ( str && str[ 0 ] )
	{
		char szTemp[128];
		if ( StringHasPrefix( str, STEAM_PREFIX ) )
		{
			Q_strncpy( szTemp, str + Q_strlen( STEAM_PREFIX ), sizeof( szTemp ) - 1 );
			id.idtype = IDTYPE_STEAM;
		} 

		szTemp[ sizeof( szTemp ) - 1 ] = '\0';

		CCommand args;
		args.Tokenize( szTemp );
		if ( args.ArgC() >= 5 )
		{
			id.uid.steamid.m_SteamInstanceID = ( SteamInstanceID_t )atoi( args[ 0 ] );
			id.uid.steamid.m_SteamLocalUserID.Split.High32bits = (int)atoi( args[ 2 ] );
			id.uid.steamid.m_SteamLocalUserID.Split.Low32bits = (int)atoi( args[ 4 ] );
		}
	}
	return &id;
}
USERID_t *Filter_Steam64bitIdToUserID( uint64 uiAccountID )
{
	static USERID_t id;
	Q_memset( &id, 0, sizeof( id ) );

	if ( uiAccountID )
	{
		CSteamID steamID( uiAccountID );
		steamID.ConvertToSteam2( &id.uid.steamid );
		
		id.idtype = IDTYPE_STEAM;
		id.uid.steamid.m_SteamInstanceID = 1;
	}
	return &id;
}


//-----------------------------------------------------------------------------
// Purpose: Saves Steam ID bans to a file
//-----------------------------------------------------------------------------
CON_COMMAND( writeid, "Writes a list of permanently-banned user IDs to " BANNED_USER_FILENAME "." )
{
	FileHandle_t f;
	char name[MAX_OSPATH];
	int i;
	float banTime;

	Q_strncpy( name, CONFIG_DIR BANNED_USER_FILENAME, sizeof( name ) );

	ConMsg( "Writing %s.\n", name );

	f = g_pFileSystem->Open ( name, "wb" );
	if ( !f )
	{
		ConMsg( "Couldn't open %s\n", name );
		return;
	}
	
	for ( i = 0 ; i < g_UserFilters.Count() ; i++ )
	{
		banTime = g_UserFilters[i].banTime;

		if ( banTime != 0.0f )
		{
			continue;
		}

		g_pFileSystem->FPrintf( f, "banid 0 %s\r\n", GetUserIDString( g_UserFilters[i].userid ) );
	}

	g_pFileSystem->Close( f );
}


//-----------------------------------------------------------------------------
// Purpose: Removes all Steam ID bans from the ban list
//-----------------------------------------------------------------------------
CON_COMMAND( removeallids, "Remove all user IDs from the ban list." )
{
	int nCount = g_UserFilters.Count();
	g_UserFilters.RemoveAll();
	ConMsg( "removeallids:  filter removed for %u user IDs\n", nCount );
}


//-----------------------------------------------------------------------------
// Purpose: Removes a Steam ID ban
//-----------------------------------------------------------------------------
CON_COMMAND( removeid, "Remove a user ID from the ban list." )
{
	int			i = 0;
	const char	*pszArg1 = NULL;
	char		szSearchString[64];

	if ( args.ArgC() != 2 && args.ArgC() != 6 )
	{
		ConMsg( "Usage:  removeid < slot | uniqueid >\n" );
		return;
	}

	// get the first argument
	pszArg1 = args[1];

	// don't need the # if they're using it
	if ( pszArg1[ 0 ] == '#' )
	{
		ConMsg( "Usage:  removeid < userid | uniqueid >\n" );
		ConMsg( "No # necessary\n");
		return;
	}

	// if the first letter is a charcter then
	// we're searching for a uniqueid ( e.g. STEAM_ )
	if ( *pszArg1 < '0' || *pszArg1 > '9' )
	{
		// SteamID (need to reassemble it)
		if ( StringHasPrefix( pszArg1, STEAM_PREFIX ) && Q_strstr( args[2], ":" ) )
		{
			Q_snprintf( szSearchString, sizeof( szSearchString ), "%s:%s:%s", pszArg1, args[3], args[5] );
		}
		// some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
		// NOTE: assumed to be one argument
		else
		{
			ConMsg( "removeid:  invalid ban ID \"%s\"\n", pszArg1 );
			return;
		}

		for ( i = 0 ; i < g_UserFilters.Count() ; i++ )
		{
			if ( Q_stricmp( GetUserIDString( g_UserFilters[i].userid ), szSearchString ) )
				continue;

			g_UserFilters.Remove( i );
			ConMsg( "removeid:  filter removed for %s\n", szSearchString );

			// send an event
			IGameEvent *event = g_GameEventManager.CreateEvent( "server_removeban" );

			if ( event )
			{
				event->SetString( "networkid", szSearchString );
				event->SetString( "ip", "" );
				event->SetString( "by", ( args.Source() != kCommandSrcNetClient ) ? "Console" : host_client->m_Name );
				g_GameEventManager.FireEvent( event );
			}

			return;
		}
		
		ConMsg( "removeid:  couldn't find %s\n", szSearchString );
	}
	// this is a userid
	else
	{
		int slot = Q_atoi( pszArg1 );
		if ( slot > 0 && slot <= g_UserFilters.Count() )
		{
			USERID_t id;

			// array access is zero based
			slot--;

			// Copy off slot
			id = g_UserFilters[slot].userid;

			g_UserFilters.Remove( slot );

			// Tell server operator
			ConMsg( "removeid:  filter removed for %s, ID %s\n", pszArg1, GetUserIDString( id ) );

			// send an event
			IGameEvent *event = g_GameEventManager.CreateEvent( "server_removeban" );

			if ( event )
			{
				event->SetString( "networkid", GetUserIDString( id ) );
				event->SetString( "ip", "" );
				event->SetString( "by", ( args.Source() != kCommandSrcNetClient ) ? "Console" : host_client->m_Name );
				g_GameEventManager.FireEvent( event );
			}
		}
		else
		{
			ConMsg( "removeid:  invalid slot %i\n", slot );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Prints Steam ID bans to the console
//-----------------------------------------------------------------------------
CON_COMMAND( listid, "Lists banned users." )
{
	int i;
	int count = g_UserFilters.Count();

	if ( !count )
	{
		ConMsg( "ID filter list: empty\n" );
		return;
	}
	else
	{
		if ( count == 1 )
		{
			ConMsg( "ID filter list: %i entry\n", count );
		}
		else
		{
			ConMsg( "ID filter list: %i entries\n", count );
		}
	}

	for ( i = 0 ; i < count ; i++ )
	{
		if ( g_UserFilters[i].banTime != 0.0 )
		{
			ConMsg( "%i %s : %.3f min\n", i+1, GetUserIDString( g_UserFilters[i].userid ), g_UserFilters[i].banTime );
		}
		else
		{
			ConMsg( "%i %s : permanent\n", i+1, GetUserIDString( g_UserFilters[i].userid ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Bans a Steam ID
//-----------------------------------------------------------------------------
CON_COMMAND( banid, "Add a user ID to the ban list." )
{
	int			i;
	float		banTime;
	USERID_t	localId;
	USERID_t *	id = NULL;
	int			iSearchIndex = -1;
	char		szDuration[256];
	uint64		uiSteam64bitID = 0;
	char		szSearchString[64];
	bool		bKick = false;
	bool		bPlaying = false;
	const char	*pszArg2 = NULL;
	CGameClient *client = NULL;

	if ( Steam3Server().BLanOnly() )
	{
		ConMsg( "Can't ban users on a LAN\n" );
		return;
	}

	if ( args.ArgC() < 3 || args.ArgC() > 8 )
	{
		ConMsg( "Usage:  banid < minutes > < userid | uniqueid > { kick }\n" );
		ConMsg( "Use 0 minutes for permanent\n");
		return;
	}

	banTime = Q_atof( args[1] );
	if ( banTime < 0.01 )
	{
		banTime = 0.0;
	}

	// get the first argument
	pszArg2 = args[2];

	// don't need the # if they're using it
	if ( pszArg2[ 0 ] == '#' )
	{
		ConMsg( "Usage:  banid < minutes > < userid | uniqueid > { kick }\n" );
		ConMsg( "No # necessary\n");
		return;
	}

	bKick = ( args.ArgC() >= 4 && Q_strcasecmp( args[ args.ArgC() - 1 ], "kick" ) == 0 );

	// if the first letter is a charcter then
	// we're searching for a uniqueid ( e.g. STEAM_ )
	if ( *pszArg2 < '0' || *pszArg2 > '9' )
	{
		if ( char const *sz64bitID = StringAfterPrefix( pszArg2, "STEAM64BITID_" ) )
		{
			uiSteam64bitID = Q_atoui64( sz64bitID );
		}
		// SteamID (need to reassemble it)
		else if ( StringHasPrefix( pszArg2, STEAM_PREFIX ) && Q_strstr( args[3], ":" ) )
		{
			Q_snprintf( szSearchString, sizeof( szSearchString ), "%s:%s:%s", pszArg2, args[4], args[6] );
		}
		// some other ID (e.g. "UNKNOWN", "STEAM_ID_PENDING", "STEAM_ID_LAN")
		// NOTE: assumed to be one argument
		else
		{
			ConMsg( "Can't ban users with ID \"%s\"\n", pszArg2 );
			return;
		}
	}
	// this is a userid
	else
	{
		iSearchIndex = Q_atoi( pszArg2 );
	}

	// find this client (if they're currently in the server)
	for ( i = 0; i < sv.GetClientCount(); i++ )
	{
		client = sv.Client(i);

		if ( !client || !client->IsActive() || !client->IsConnected() || !client->IsSpawned() )
		{
			continue;
		}

		if ( client->IsFakeClient() )
		{
			continue;
		}

		// searching by UserID
		if ( iSearchIndex != -1 )
		{
			if ( client->GetUserID() == iSearchIndex )
			{
				// found!
				localId = client->GetNetworkID();
				id = &localId;
				bPlaying = true;
				break;
			}
		}
		else if ( uiSteam64bitID )
		{
			if ( client->m_SteamID.IsValid() && client->m_SteamID.BIndividualAccount() && ( client->m_SteamID.ConvertToUint64() == uiSteam64bitID ) )
			{
				// found!
				localId = client->GetNetworkID();
				id = &localId;
				bPlaying = true;
				break;
			}
		}
		// searching by UniqueID
		else	
		{
			if ( Q_stricmp( client->GetNetworkIDString(), szSearchString ) == 0 ) 
			{
				// found!
				localId = client->GetNetworkID();
				id = &localId;
				bPlaying = true;
				break;
			}
		}
	}

	// if we were searching by userid and we didn't find the person, we're done
	if ( iSearchIndex != -1 && !id )
	{
		ConMsg( "banid:  couldn't find userid %d\n", iSearchIndex );
		return;
	}

	if ( !id )
	{
		if ( uiSteam64bitID )
			id = Filter_Steam64bitIdToUserID( uiSteam64bitID );
		else
			// we're searching by SteamID and we haven't found them actively playing
			id = Filter_StringToUserID( szSearchString );

		if ( !id )
		{
			if ( uiSteam64bitID )
				ConMsg( "banid:  Couldn't resolve 64bit id \"%llu\".\n", uiSteam64bitID );
			else
				ConMsg( "banid:  Couldn't resolve uniqueid \"%s\".\n", szSearchString );

			ConMsg( "Usage:  banid < minutes > < userid | uniqueid > { kick }\n" );
			ConMsg( "Use 0 minutes for permanent\n");
			return;
		}
	}

	if ( !id )
	{
		// Should never occur!!!
		ConMsg( "SV_BanId_f:  id == NULL\n" );
		return;
	}

	// See if it's in the list already
	for ( i = 0 ; i < g_UserFilters.Count() ; i++ )
	{
		// We're just updating an existing id
		if ( Steam3Server().CompareUserID( g_UserFilters[i].userid, *id ) )
			break;
	}

	// 
	// Adding a new one
	if ( i >= g_UserFilters.Count() )
	{
		// See if we have space for it
		if ( g_UserFilters.Count() >= MAX_USERFILTERS )
		{
			ConMsg( "banid:  user filter list is full\n" );
			return;
		}
		userfilter_t nullUser;
		memset( &nullUser, 0, sizeof(nullUser) );
		i = g_UserFilters.AddToTail( nullUser );
	}

	g_UserFilters[i].banTime = banTime;
	g_UserFilters[i].banEndTime = ( banTime != 0.0 ) ? ( realtime + 60.0 * banTime ) : 0.0;
	g_UserFilters[i].userid = *id;

	// Build a duration string for the ban
	if ( banTime == 0.0 )
	{
		Q_snprintf( szDuration, sizeof( szDuration ), "permanently" );
	}
	else
	{
		Q_snprintf( szDuration, sizeof( szDuration ), "for %.2f minutes", banTime );
	}

	// fire the event
	IGameEvent *event = g_GameEventManager.CreateEvent( "server_addban" );

	if ( event )
	{
		if ( bPlaying )
		{
			event->SetString( "name", client->m_Name );
			event->SetInt( "userid", client->GetUserID() );
			event->SetString( "networkid", client->GetNetworkIDString() );
		}
		else
		{
			event->SetString( "name", "" );
			event->SetInt( "userid", 0 );
			event->SetString( "networkid", GetUserIDString( *id ) );
		}

		event->SetString( "ip", "" );
		event->SetString( "duration", szDuration );
		event->SetString( "by", ( args.Source() != kCommandSrcNetClient ) ? "Console" : host_client->m_Name );
		event->SetInt( "kicked", ( bKick && bPlaying && client ) ? 1 : 0 );

		g_GameEventManager.FireEvent( event );
	}

	if ( bKick && bPlaying && client )
	{
		client->ClientPrintf ( "You have been kicked and banned %s by the server.\n", szDuration );
		client->Disconnect( "Kicked and banned" );
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Filter_Init( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Filter_Shutdown( void )
{
}
