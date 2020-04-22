#include "client_pch.h"
#include "cl_splitscreen.h"

#if defined( _PS3 )
#include "tls_ps3.h"
#define m_SplitSlot reinterpret_cast< SplitSlot_t *& >(GetTLSGlobals()->pEngineSplitSlot)
#endif // _PS3

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CSplitScreen : public ISplitScreen
{
public:
	CSplitScreen();

	virtual bool			Init();
	virtual void			Shutdown();

	virtual bool			AddSplitScreenUser( int nSlot, int nPlayerIndex );
	virtual bool			AddBaseUser( int nSlot, int nPlayerIndex );
	virtual bool			RemoveSplitScreenUser( int nSlot, int nPlayerIndex );
	virtual int				GetActiveSplitScreenPlayerSlot();
	virtual int				SetActiveSplitScreenPlayerSlot( int slot );

	virtual bool			IsValidSplitScreenSlot( int nSlot );
	virtual int				FirstValidSplitScreenSlot(); // -1 == invalid
	virtual int				NextValidSplitScreenSlot( int nPreviousSlot ); // -1 == invalid

	virtual int				GetNumSplitScreenPlayers();
	virtual int				GetSplitScreenPlayerEntity( int nSlot );
	virtual INetChannel		*GetSplitScreenPlayerNetChan( int nSlot );

	virtual bool			IsDisconnecting( int nSlot );
	virtual void			SetDisconnecting( int nSlot, bool bState );

	virtual bool			SetLocalPlayerIsResolvable( char const *pchContext, int nLine, bool bResolvable );
	virtual bool			IsLocalPlayerResolvable();

	CClientState			&GetLocalPlayer( int nSlot = -1 );

	struct SplitSlot_t
	{
		SplitSlot_t() : m_nActiveSplitScreenPlayer( 0 ), m_bLocalPlayerResolvable( false ), m_bMainThread( false ) { }

		short			m_nActiveSplitScreenPlayer;
		// Can a call to C_BasePlayer::GetLocalPlayer be resolved in client .dll (inside a setactivesplitscreenuser scope?)
		unsigned short	m_bLocalPlayerResolvable : 1;
		unsigned short	m_bMainThread : 1;
		unsigned short	pad : 14;
	};

private:

	int FindSplitPlayerSlot( int nPlayerEntityIndex );

	struct SplitPlayer_t
	{
		SplitPlayer_t() : 
	m_bActive( false )
	{ 
	}

	bool			m_bActive;
	CClientState	m_Client;
	};

	SplitPlayer_t *m_SplitScreenPlayers[ MAX_SPLITSCREEN_CLIENTS ];
	int			m_nActiveSplitScreenUserCount;

#if defined( _PS3 )
#elif !defined( _X360 )
	// Each thread (mainly an issue in the client .dll) can have it's own "active" context.  The per thread data is allocated as needed
#else
	// xbox uses 12 bit thread id key to do direct lookup
	SplitSlot_t						m_SplitSlotTable[0x1000];
#endif

	SplitSlot_t *GetSplitSlot();

	bool		m_bInitialized;
};

static CTHREADLOCALPTR( CSplitScreen::SplitSlot_t )	s_SplitSlot;


static CSplitScreen g_SplitScreenMgr;
ISplitScreen *splitscreen = &g_SplitScreenMgr;

CSplitScreen::CSplitScreen()
{
	m_bInitialized = false;
}

#if defined( _X360 )
inline int BucketForThreadId()
{
	DWORD id = GetCurrentThreadId();
	// e.g.:  0xF9000028 -- or's the 9 and the 28 to give 12 bits (slot array is 0x1000 in size), the first nibble is(appears to be) always F so is masked off (0x0F00)
	return ( ( id >> 16 ) & 0x00000F00 ) | ( id & 0x000000FF );
}
#endif

CSplitScreen::SplitSlot_t *CSplitScreen::GetSplitSlot()
{
#if defined( _X360 )
	// pix shows this function to be enormously expensive due to high frequency of inner loop calls
	// avoid conditionals and TLS, use a direct lookup instead
	return &m_SplitSlotTable[ BucketForThreadId() ];
#else
	if ( !s_SplitSlot )
	{
		s_SplitSlot = new SplitSlot_t();
	}
	return s_SplitSlot;
#endif
}

bool CSplitScreen::Init()
{
	m_bInitialized = true;

	Assert( ThreadInMainThread() );

	SplitSlot_t *pSlot = GetSplitSlot();
	pSlot->m_bLocalPlayerResolvable = false;
	pSlot->m_nActiveSplitScreenPlayer = 0;
	pSlot->m_bMainThread = true;
	m_nActiveSplitScreenUserCount = 1;
	for ( int i = 0 ; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		MEM_ALLOC_CREDIT();
		m_SplitScreenPlayers[ i ] = new SplitPlayer_t();
		SplitPlayer_t *sp = m_SplitScreenPlayers[ i ];
		sp->m_bActive = ( i == 0 ) ? true : false;
		sp->m_Client.m_bSplitScreenUser = ( i != 0 ) ? true : false;
	}

	return true;
}

void CSplitScreen::Shutdown()
{
	Assert( ThreadInMainThread() );
	for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
	{
		delete m_SplitScreenPlayers[ i ];
		m_SplitScreenPlayers[ i ] = NULL;
	}
}

bool CSplitScreen::AddBaseUser( int nSlot, int nPlayerIndex )
{
	Assert( ThreadInMainThread() );
	SplitPlayer_t *sp = m_SplitScreenPlayers[ nSlot ];
	sp->m_bActive = true;
	sp->m_Client.m_nSplitScreenSlot = nSlot;
	return true;
}

bool CSplitScreen::AddSplitScreenUser( int nSlot, int nPlayerEntityIndex )
{
	Assert( ThreadInMainThread() );
	SplitPlayer_t *sp = m_SplitScreenPlayers[ nSlot ];
	if ( sp->m_bActive == true )
	{
		Assert( sp->m_Client.m_nSplitScreenSlot == nSlot );
		Assert( sp->m_Client.m_nPlayerSlot == nPlayerEntityIndex - 1 );
		return true;
	}

	// Msg( "Attached %d to slot %d\n", nPlayerEntityIndex, nSlot );

	// 0.0.0.0:0 signifies a bot. It'll plumb all the way down to winsock calls but it won't make them.
	ns_address adr;
	adr.SetAddrType( NSAT_NETADR );
	adr.m_adr.SetIPAndPort( 0, 0 );

	char szName[ 256 ];
	Q_snprintf( szName, sizeof( szName), "SPLIT%d", nSlot );

	sp->m_bActive = true;
	sp->m_Client.Clear();
	sp->m_Client.m_nPlayerSlot = nPlayerEntityIndex - 1;
	sp->m_Client.m_nSplitScreenSlot = nSlot;
	sp->m_Client.m_NetChannel = NET_CreateNetChannel( NS_CLIENT, &adr, szName, &sp->m_Client, NULL, true );
	GetBaseLocalClient().m_NetChannel->AttachSplitPlayer( nSlot, sp->m_Client.m_NetChannel );
	sp->m_Client.m_nViewEntity = nPlayerEntityIndex;
	++m_nActiveSplitScreenUserCount;
	SetDisconnecting( nSlot, false );

	ClientDLL_OnSplitScreenStateChanged();

	return true;
}

bool CSplitScreen::RemoveSplitScreenUser( int nSlot, int nPlayerIndex )
{
	Assert( ThreadInMainThread() );
	// Msg( "Detached %d from slot %d\n", nPlayerIndex, nSlot );

	int idx = FindSplitPlayerSlot( nPlayerIndex );
	if ( idx != -1 )
	{
		SplitPlayer_t *sp = m_SplitScreenPlayers[ idx ];
		if ( sp->m_Client.m_NetChannel )
		{
			GetBaseLocalClient().m_NetChannel->DetachSplitPlayer( idx );
			sp->m_Client.m_NetChannel->Shutdown( "RemoveSplitScreenUser" );
			sp->m_Client.m_NetChannel = NULL;
		}
		sp->m_Client.m_nPlayerSlot = -1;
		sp->m_bActive = false;
		SetDisconnecting( nSlot, true );
		--m_nActiveSplitScreenUserCount;
		ClientDLL_OnSplitScreenStateChanged();
	}
	return true;
}

int CSplitScreen::GetActiveSplitScreenPlayerSlot()
{
#if !defined( SPLIT_SCREEN_STUBS )
	SplitSlot_t *pSlot = GetSplitSlot();
	int nSlot = pSlot->m_nActiveSplitScreenPlayer;
#if defined( _DEBUG )
	if ( nSlot >= host_state.max_splitscreen_players_clientdll )
	{
		static bool warned = false;
		if ( !warned )
		{
			warned = true;
			Warning( "GetActiveSplitScreenPlayerSlot() returning bogus slot #%d\n", nSlot );
		}
	}
#endif
	return nSlot;
#else
	return 0;
#endif
}

int CSplitScreen::SetActiveSplitScreenPlayerSlot( int slot )
{
#if !defined( SPLIT_SCREEN_STUBS )
	Assert( m_bInitialized );

	slot = clamp( slot, 0, host_state.max_splitscreen_players_clientdll - 1 );

	SplitSlot_t *pSlot = GetSplitSlot();
	Assert( pSlot );
	int old = pSlot->m_nActiveSplitScreenPlayer;

	if ( slot == old )
		return slot;

	pSlot->m_nActiveSplitScreenPlayer = slot;

	// Only change netchannel in main thread and only change vgui message context id in main thread (for now)
	if ( pSlot->m_bMainThread )
	{
		if ( m_SplitScreenPlayers[ slot ] && m_SplitScreenPlayers[ 0 ] )
		{
			INetChannel *nc = m_SplitScreenPlayers[ slot ]->m_Client.m_NetChannel;
			CBaseClientState &bcs = m_SplitScreenPlayers[ 0 ]->m_Client;
			if ( bcs.m_NetChannel && nc )
			{
				bcs.m_NetChannel->SetActiveChannel( nc );
			}
		}
	}

	return old;
#else
	return 0;
#endif
}

int	CSplitScreen::GetNumSplitScreenPlayers()
{
	return m_nActiveSplitScreenUserCount;
}

int CSplitScreen::GetSplitScreenPlayerEntity( int nSlot )
{
	Assert( nSlot >= 0 && nSlot < host_state.max_splitscreen_players );
	Assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return -1;
	if ( !m_SplitScreenPlayers[ nSlot ]->m_bActive )
		return -1;
	return m_SplitScreenPlayers[ nSlot ]->m_Client.m_nPlayerSlot + 1;
}

INetChannel *CSplitScreen::GetSplitScreenPlayerNetChan( int nSlot )
{
	Assert( nSlot >= 0 && nSlot < host_state.max_splitscreen_players );
	Assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return NULL;
	if ( !m_SplitScreenPlayers[ nSlot ]->m_bActive )
		return NULL;
	return m_SplitScreenPlayers[ nSlot ]->m_Client.m_NetChannel;
}

bool CSplitScreen::IsValidSplitScreenSlot( int nSlot )
{
	Assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return false;
	return m_SplitScreenPlayers[ nSlot ]->m_bActive;
}

int CSplitScreen::FirstValidSplitScreenSlot()
{
	return 0;
}

int CSplitScreen::NextValidSplitScreenSlot( int nPreviousSlot )
{
	for ( ;; )
	{
		++nPreviousSlot;
		if ( nPreviousSlot >= host_state.max_splitscreen_players )
		{
			return -1;
		}

		if ( m_SplitScreenPlayers[ nPreviousSlot ]->m_bActive )
		{
			break;
		}

	}
	return nPreviousSlot;
}

int CSplitScreen::FindSplitPlayerSlot( int nPlayerEntityIndex )
{
	int nPlayerSlot = nPlayerEntityIndex - 1;

	Assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	for ( int i = 1 ; i < host_state.max_splitscreen_players ; ++i )
	{
		if ( m_SplitScreenPlayers[ i ]->m_Client.m_nPlayerSlot == nPlayerSlot )
		{
			return i;
		}
	}
	return -1;
}

bool CSplitScreen::IsDisconnecting( int nSlot )
{
	Assert( nSlot >= 0 && nSlot < host_state.max_splitscreen_players );
	Assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return false;

	return ( m_SplitScreenPlayers[ nSlot ]->m_Client.m_nSignonState == SIGNONSTATE_NONE ) ? true : false;
}

void CSplitScreen::SetDisconnecting( int nSlot, bool bState )
{
	Assert( nSlot >= 0 && nSlot < host_state.max_splitscreen_players );
	Assert( host_state.max_splitscreen_players <= MAX_SPLITSCREEN_CLIENTS );
	if ( nSlot < 0 || nSlot >= host_state.max_splitscreen_players )
		return;

	Assert( nSlot != 0 );
	m_SplitScreenPlayers[ nSlot ]->m_Client.m_nSignonState = bState ? SIGNONSTATE_NONE : SIGNONSTATE_FULL;
}

CClientState &CSplitScreen::GetLocalPlayer( int nSlot /*= -1*/ )
{
	if ( nSlot == -1 )
	{
		Assert( IsLocalPlayerResolvable() );
		return m_SplitScreenPlayers[ GetActiveSplitScreenPlayerSlot() ]->m_Client;
	}
	return m_SplitScreenPlayers[ nSlot ]->m_Client;
}

bool CSplitScreen::SetLocalPlayerIsResolvable( char const *pchContext, int line, bool bResolvable )
{
	SplitSlot_t *pSlot = GetSplitSlot();
	Assert( pSlot );
	bool bPrev = pSlot->m_bLocalPlayerResolvable;
	pSlot->m_bLocalPlayerResolvable = bResolvable;
	return bPrev;
}

bool CSplitScreen::IsLocalPlayerResolvable()
{

#if defined( SPLIT_SCREEN_STUBS )

	return true;

#else

	SplitSlot_t *pSlot = GetSplitSlot();
	return pSlot->m_bLocalPlayerResolvable;

#endif

}

// Singleton client state
CClientState &GetLocalClient( int nSlot /*= -1*/ )
{
	return g_SplitScreenMgr.GetLocalPlayer( nSlot );
}

CClientState &GetBaseLocalClient()
{
	return g_SplitScreenMgr.GetLocalPlayer( 0 );
}
