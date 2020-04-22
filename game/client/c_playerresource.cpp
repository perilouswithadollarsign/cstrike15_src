//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_playerresource.h"
#include "c_team.h"
#include "gamestringpool.h"
#include "hltvreplaysystem.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const float PLAYER_RESOURCE_THINK_INTERVAL = 0.2f;
#define PLAYER_DEBUG_NAME "WWWWWWWWWWWWWWW"

ConVar cl_names_debug( "cl_names_debug", "0", FCVAR_DEVELOPMENTONLY );


void RecvProxy_ChangedTeam( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// Have the regular proxy store the data.
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	if ( g_PR )
	{
		g_PR->TeamChanged();
	}
}

IMPLEMENT_CLIENTCLASS_DT_NOBASE(C_PlayerResource, DT_PlayerResource, CPlayerResource)
	RecvPropArray3( RECVINFO_ARRAY(m_iPing), RecvPropInt( RECVINFO(m_iPing[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iKills), RecvPropInt( RECVINFO(m_iKills[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iAssists), RecvPropInt( RECVINFO(m_iAssists[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iDeaths), RecvPropInt( RECVINFO(m_iDeaths[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_bConnected), RecvPropInt( RECVINFO(m_bConnected[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iTeam), RecvPropInt( RECVINFO(m_iTeam[0]), 0, RecvProxy_ChangedTeam )),
	RecvPropArray3( RECVINFO_ARRAY(m_iPendingTeam), RecvPropInt( RECVINFO(m_iPendingTeam[0]), 0, RecvProxy_ChangedTeam )),
	RecvPropArray3( RECVINFO_ARRAY(m_bAlive), RecvPropInt( RECVINFO(m_bAlive[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iHealth), RecvPropInt( RECVINFO(m_iHealth[0]))),
	RecvPropArray3( RECVINFO_ARRAY(m_iCoachingTeam), RecvPropInt( RECVINFO(m_iCoachingTeam[0]))),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_PlayerResource )

	DEFINE_PRED_ARRAY( m_szName, FIELD_STRING, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iPing, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iKills, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iAssists, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iDeaths, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_bConnected, FIELD_BOOLEAN, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iTeam, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iPendingTeam, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_bAlive, FIELD_BOOLEAN, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iHealth, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),
	DEFINE_PRED_ARRAY( m_iCoachingTeam, FIELD_INTEGER, MAX_PLAYERS+1, FTYPEDESC_PRIVATE ),

END_PREDICTION_DATA()	

C_PlayerResource *g_PR;

IGameResources * GameResources( void ) { return g_PR; }

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PlayerResource::C_PlayerResource()
{
	for ( int i=0; i<ARRAYSIZE(m_szName); ++i )
	{
		m_szName[i] = AllocPooledString( "unconnected" );
	}
	memset( m_iPing, 0, sizeof( m_iPing ) );
//	memset( m_iPacketloss, 0, sizeof( m_iPacketloss ) );
	memset( m_iKills, 0, sizeof( m_iKills ) );
	memset( m_iAssists, 0, sizeof( m_iAssists ) );
	memset( m_iDeaths, 0, sizeof( m_iDeaths ) );
	memset( m_bConnected, 0, sizeof( m_bConnected ) );
	memset( m_iTeam, 0, sizeof( m_iTeam ) );
	memset( m_iPendingTeam, 0, sizeof( m_iTeam ) );
	memset( m_bAlive, 0, sizeof( m_bAlive ) );
	memset( m_iHealth, 0, sizeof( m_iHealth ) );
	memset( m_Xuids, 0, sizeof( m_Xuids ) );
	memset( m_iCoachingTeam, 0, sizeof( m_iCoachingTeam) );

	for ( int i=0; i<MAX_TEAMS; i++ )
	{
		m_Colors[i] = COLOR_GREY;
	}

	g_PR = this;

	g_pScaleformUI->AddDeviceDependentObject( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_PlayerResource::~C_PlayerResource()
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		if ( m_Xuids[i] != INVALID_XUID )
		{
			g_pScaleformUI->AvatarImageRelease( m_Xuids[i] );
		}
	}

	g_PR = NULL;

	g_pScaleformUI->RemoveDeviceDependentObject( this );
}

void C_PlayerResource::OnDataChanged(DataUpdateType_t updateType)
{
	UpdateXuids();

	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( gpGlobals->curtime + PLAYER_RESOURCE_THINK_INTERVAL );
	}
}

void C_PlayerResource::UpdateXuids( void )
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		XUID newXuid = INVALID_XUID;
		player_info_t sPlayerInfo;
		if ( m_bConnected[ i ] && engine->GetPlayerInfo( i, &sPlayerInfo ) )
		{
			newXuid = sPlayerInfo.xuid;

			// When running in anonymous mode wipe out all XUIDs
			if ( newXuid != INVALID_XUID )
			{
				if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
				{
					if ( pParameters->m_bAnonymousPlayerIdentity )
					{
// 						CSteamID steamid( newXuid );
// 						steamid.SetAccountID( ( ~uint32(0) ) -1 - i );
// 						newXuid = steamid.ConvertToUint64();
						newXuid = INVALID_XUID;
					}
				}
			}
		}

		if ( newXuid != m_Xuids[i] )
		{
			bool bAddRefSuccess = false;

			if ( m_Xuids[i] != INVALID_XUID )
			{
				g_pScaleformUI->AvatarImageRelease( m_Xuids[i] );
			}

			if ( newXuid != INVALID_XUID )
			{
				bAddRefSuccess = g_pScaleformUI->AvatarImageAddRef( newXuid );
			}

			if ( bAddRefSuccess || ( newXuid == INVALID_XUID ) )
			{
				m_Xuids[i] = newXuid;
			}
		}
	}
}

void C_PlayerResource::UpdateAsLocalizedFakePlayerName( int slot, char const *pchPlayerName )
{
	static CUtlStringMap< CUtlString > s_mapLocalizedNames;
	UtlSymId_t symName = s_mapLocalizedNames.Find( pchPlayerName );
	if ( symName == UTL_INVAL_SYMBOL )
	{
		// Need to localize it from scratch
		CUtlVector< char * > arrLocTokens;
		CFmtStr1024 strComposite;
		V_SplitString( pchPlayerName, " ", arrLocTokens );
		FOR_EACH_VEC( arrLocTokens, i )
		{
			if ( i ) strComposite.Append( " " );

			if ( wchar_t const * const kwszLocalizedToken = g_pVGuiLocalize->Find( CFmtStr( "#CSGO_FakePlayer_%s", arrLocTokens[ i ] ) ) )
			{
				char chUtf8token[ MAX_PLAYER_NAME_LENGTH ] = {};
				V_UnicodeToUTF8( kwszLocalizedToken, chUtf8token, sizeof( chUtf8token ) );
				strComposite.Append( chUtf8token );
			}
			else
			{
				strComposite.Append( arrLocTokens[ i ] );
				Warning( "Failed to localize fake player name '#CSGO_FakePlayer_%s'\n", arrLocTokens[ i ] );
			}
		}
		symName = s_mapLocalizedNames.Insert( pchPlayerName, strComposite.Access() );
		arrLocTokens.PurgeAndDeleteElements();
	}

	Assert( symName != UTL_INVAL_SYMBOL );

	m_szName[ slot ] = s_mapLocalizedNames[ symName ];
}

void C_PlayerResource::UpdatePlayerName( int slot )
{
	if ( slot < 1 || slot > MAX_PLAYERS )
	{
		Error( "UpdatePlayerName with bogus slot %d\n", slot );
		return;
	}
	player_info_t sPlayerInfo;
	char const *pchPlayerName = PLAYER_UNCONNECTED_NAME;
	if ( IsConnected( slot ) && 
		engine->GetPlayerInfo( slot, &sPlayerInfo ) )
	{
		pchPlayerName = sPlayerInfo.name;

		if ( sPlayerInfo.fakeplayer && *pchPlayerName )
		{
			UpdateAsLocalizedFakePlayerName( slot, pchPlayerName );
			return;
		}
	}

	if ( !m_szName[slot] || Q_stricmp( m_szName[slot], pchPlayerName ) )
	{
		m_szName[slot] = AllocPooledString( pchPlayerName );
	}
}

void C_PlayerResource::ClientThink()
{
	BaseClass::ClientThink();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		UpdatePlayerName( i );
	}

	SetNextClientThink( gpGlobals->curtime + PLAYER_RESOURCE_THINK_INTERVAL );
}

#define STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_chStaticArrayVarName, szLocToken, szDefaultText ) \
	static char s_chStaticArrayVarName[MAX_PLAYER_NAME_LENGTH] = {}; \
	if ( !s_chStaticArrayVarName[ 0 ] ) \
	{ \
		wchar_t const * const kwszTheSuspect = g_pVGuiLocalize->Find( szLocToken ); \
		Assert( kwszTheSuspect ); \
		V_UnicodeToUTF8( kwszTheSuspect, s_chStaticArrayVarName, sizeof( s_chStaticArrayVarName ) ); \
		Assert( s_chStaticArrayVarName[ 0 ] ); \
		if ( !s_chStaticArrayVarName[ 0 ] ) \
			V_strcpy_safe( s_chStaticArrayVarName, szDefaultText ); \
	}


class CStaticPlayerNamesSet
{
public:
	CStaticPlayerNamesSet()
	{
		m_bInitialized = false;
		Q_memset( m_szNames, 0, sizeof( m_szNames ) );
	}

	char const * GetName( int idx )
	{
		if ( ( idx < 0 ) || ( idx > MAX_PLAYERS ) )
		{
			STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_utf8LocalPlayer, "#SFUI_LocalPlayer", "Player" );
			return s_utf8LocalPlayer;
		}
		else
		{
			if ( !m_bInitialized )
				InitializeNames();

			return m_szNames[idx];
		}
	}

protected:
	bool m_bInitialized;
	char const * m_szNames[MAX_PLAYERS+1];

public:
	void InitializeNames()
	{
		CUtlVector< const char * > arrNamesOptions;
#define RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( englishName ) { \
	STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_utf8Name##englishName, "#CSGO_FakePlayer_" #englishName, #englishName ); \
	arrNamesOptions.AddToTail( s_utf8Name##englishName ); }
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Albatross );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Alpha );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Anchor );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Banjo );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Bell );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Beta );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Blackbird );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Bulldog );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Canary );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Cat );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Calf );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Cyclone );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Daisy );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Dalmatian );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Dart );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Delta );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Diamond );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Donkey );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Duck );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Emu );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Eclipse );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Flamingo );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Flute );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Frog );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Goose );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Hatchet );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Heron );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Husky );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Hurricane );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Iceberg );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Iguana );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Kiwi );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Kite );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Lamb );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Lily );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Macaw );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Manatee );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Maple );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Mask );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Nautilus );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Ostrich );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Octopus );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Pelican );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Puffin );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Pyramid );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Rattle );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Robin );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Rose );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Salmon );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Seal );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Shark );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Sheep );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Snake );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Sonar );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Stump );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Sparrow );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Toaster );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Toucan );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Torus );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Violet );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Vortex );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Vulture );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Wagon );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Whale );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Woodpecker );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Zebra );
		RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( Zigzag );
#undef RANDOM_FAKE_PLAYER_NAME_UTF8_ARRAY_LOCALIZED
		for ( int k = 0; k < MAX_PLAYERS + 1; ++ k )
		{
			if ( !arrNamesOptions.Count() )
				Error( "Insufficient random names pool!\n" );
			int iRandomChoice = RandomInt( 0, arrNamesOptions.Count() - 1 );
			m_szNames[k] = arrNamesOptions.Element( iRandomChoice );
			arrNamesOptions.Remove( iRandomChoice );
		}
		m_bInitialized = true;
	}
} g_staticPlayerNames;
void EnsureStaticPlayerNamesReinitialized()
{
	g_staticPlayerNames.InitializeNames();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *C_PlayerResource::GetPlayerName( int iIndex )
{
	if ( cl_names_debug.GetInt() )
		return PLAYER_DEBUG_NAME;

	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		Assert( false );
		return PLAYER_ERROR_NAME;
	}
	
	if ( !IsConnected( iIndex ) )
		return PLAYER_UNCONNECTED_NAME;

	// X360TBD: Network - figure out why the name isn't set
	if ( !m_szName[ iIndex ] || !Q_stricmp( m_szName[ iIndex ], PLAYER_UNCONNECTED_NAME ) )
	{
		// If you get a full "reset" uncompressed update from server, then you can have NULLNAME show up in the scoreboard
		UpdatePlayerName( iIndex );
	}

	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
		{
			player_info_t sPlayerInfo;
			if ( pParameters->m_uiLockFirstPersonAccountID && engine->GetPlayerInfo( iIndex, &sPlayerInfo ) &&
				( CSteamID( sPlayerInfo.xuid ).GetAccountID() == pParameters->m_uiLockFirstPersonAccountID ) )
			{
				STATIC_PLAYER_NAME_UTF8_ARRAY_LOCALIZED( s_utf8TheSuspect, "#CSGO_Overwatch_TheSuspect", "The Suspect" );
				return s_utf8TheSuspect;
			}
			else
			{
				return g_staticPlayerNames.GetName( iIndex );
			}
		}
	}

	// This gets updated in ClientThink, so it could be up to 1 second out of date, oh well.
	return m_szName[iIndex];
}

bool C_PlayerResource::IsAlive(int iIndex )
{
	return m_bAlive[iIndex];
}

int C_PlayerResource::GetTeam(int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		Assert( false );
		return 0;
	}
	else
	{
		return m_iTeam[iIndex];
	}
}

int C_PlayerResource::GetPendingTeam(int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
	{
		Assert( false );
		return 0;
	}
	else
	{
		return m_iPendingTeam[iIndex];
	}
}

const char * C_PlayerResource::GetTeamName(int index)
{
	C_Team *team = GetGlobalTeam( index );

	if ( !team )
		return "Unknown";

	return team->Get_Name();
}

int C_PlayerResource::GetCoachingTeam(int index)
{
	if ( index < 1 || index > MAX_PLAYERS )
	{
		Assert( false );
		return 0;
	}
	else
	{
		return m_iCoachingTeam[index];
	}
}


int C_PlayerResource::GetTeamScore(int index)
{
	C_Team *team = GetGlobalTeam( index );

	if ( !team )
		return 0;

	return team->Get_Score();
}

int C_PlayerResource::GetFrags(int index )
{
	return 666;
}

bool C_PlayerResource::IsLocalPlayer(int index)
{
	C_BasePlayer *pPlayer =	C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return false;

	// HLTV replay will not set m_bLocalPlayer flag, in a sense there's no selected local player, we're observing everyone
	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
		return false;

	return ( index == pPlayer->entindex() );
}


bool C_PlayerResource::IsHLTV(int index)
{
	if ( !IsConnected( index ) )
		return false;

	// HLTV replay will not set m_bLocalPlayer flag, in a sense there's no selected local player, we're observing everyone
	if ( g_HltvReplaySystem.GetHltvReplayDelay() && C_BasePlayer::GetLocalPlayer()->index == index )
		return true;  // local player is always HLTV in HLTV replay mode, even though the hltv property isn't set because we are in the past and replaying everything as it was (including no hltv flag set)

	player_info_t sPlayerInfo;
	
	if ( engine->GetPlayerInfo( index, &sPlayerInfo ) )
	{
		return sPlayerInfo.ishltv;
	}
	
	return false;
}

#if defined( REPLAY_ENABLED )
bool C_PlayerResource::IsReplay(int index)
{
	if ( !IsConnected( index ) )
		return false;

#if defined( REPLAY_ENABLED )
	player_info_t sPlayerInfo;

	if ( engine->GetPlayerInfo( index, &sPlayerInfo ) )
	{
		return sPlayerInfo.isreplay;
	}
#endif

	return false;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_PlayerResource::IsFakePlayer( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return false;

	// Yuck, make sure it's up to date
	player_info_t sPlayerInfo;
	if ( engine->GetPlayerInfo( iIndex, &sPlayerInfo ) )
	{
		return sPlayerInfo.fakeplayer;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetPing( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iPing[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
/*-----------------------------------------------------------------------------
int	C_PlayerResource::GetPacketloss( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iPacketloss[iIndex];
}*/

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetKills( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iKills[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetAssists( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iAssists[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetDeaths( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iDeaths[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	C_PlayerResource::GetHealth( int iIndex )
{
	if ( !IsConnected( iIndex ) )
		return 0;

	return m_iHealth[iIndex];
}

const Color &C_PlayerResource::GetTeamColor(int index )
{
	if ( index < 0 || index >= MAX_TEAMS )
	{
		Assert( false );
		static Color blah;
		return blah;
	}
	else
	{
		return m_Colors[index];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_PlayerResource::IsConnected( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
		return false;
	else
		return m_bConnected[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
XUID C_PlayerResource::GetXuid( int iIndex )
{
	if ( iIndex < 1 || iIndex > MAX_PLAYERS )
		return INVALID_XUID;
	else
		return m_Xuids[iIndex];
}

//-----------------------------------------------------------------------------
// Purpose: Fills the given string with the player xuid.
//-----------------------------------------------------------------------------
void C_PlayerResource::FillXuidText( int iIndex, char *buf, int bufSize )
{
	Assert( buf && bufSize );
	if ( buf && bufSize )
	{
		XUID xuid = GetXuid( iIndex );

		buf[0] = '\0';
		V_snprintf( buf, bufSize, "%llu", xuid );
	}
}

void C_PlayerResource::DeviceLost( void )
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		if ( m_Xuids[i] != INVALID_XUID )
		{
			g_pScaleformUI->AvatarImageRelease( m_Xuids[i] );
			m_Xuids[i] = INVALID_XUID;
		}
	}
}

void C_PlayerResource::DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd )
{
	UpdateXuids();
}
