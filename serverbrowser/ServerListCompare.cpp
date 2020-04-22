//========= Copyright � 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

//-----------------------------------------------------------------------------
// Purpose: List password column sort function
//-----------------------------------------------------------------------------
int __cdecl PasswordCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	if ( s1->m_bPassword < s2->m_bPassword )
		return 1;
	else if ( s1->m_bPassword > s2->m_bPassword )
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: list column sort function
//-----------------------------------------------------------------------------
int __cdecl BotsCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	if ( s1->m_nBotPlayers < s2->m_nBotPlayers )
		return 1;
	else if ( s1->m_nBotPlayers > s2->m_nBotPlayers )
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: list column sort function
//-----------------------------------------------------------------------------
int __cdecl SecureCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	if ( s1->m_bSecure < s2->m_bSecure )
		return 1;
	else if ( s1->m_bSecure > s2->m_bSecure )
		return -1;

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: list column sort function
//-----------------------------------------------------------------------------
extern CUtlVector< gametypes_t > g_GameTypes;
int __cdecl ModeCompare( ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2 )
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer( p1.userData );
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer( p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	// HACK: If we start looking at things other than prefix to determine mode (eg tags) this will break... 
	// should make a 'GetModeForServerItem' shared func in that case...
	// ALSOHACK: Compare icons, not prefixes. Some game modes have alternate names (ba_ and jb_ are both jailbreak).
	const char* szIcon1 = NULL;
	const char* szIcon2 = NULL;
	FOR_EACH_VEC( g_GameTypes, i )
	{
		const gametypes_t& type = g_GameTypes[ i ];

		if ( StringHasPrefix( V_UnqualifiedFileName( s1->m_szMap ), type.m_szPrefix ) )
			szIcon1 = V_UnqualifiedFileName( type.m_szGametypeIcon );

		if ( StringHasPrefix( V_UnqualifiedFileName( s2->m_szMap ), type.m_szPrefix ) )
			szIcon2 = V_UnqualifiedFileName( type.m_szGametypeIcon );
	}

	if ( !szIcon1 && szIcon2 )
		return 1;
	if ( !szIcon2 && szIcon1 )
		return -1;
	if ( !szIcon1 && !szIcon2 )
		return 0;

	return V_stricmp( szIcon1, szIcon2 );

}

extern uint64 GetMapIDFromMapPath( const char *pMapPath );
//-----------------------------------------------------------------------------
// Purpose: list column sort function
//-----------------------------------------------------------------------------
int __cdecl WorkshopCompare( ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2 )
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer( p1.userData );
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer( p2.userData );

	if ( !s1 && s2 )
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	uint64 wsid1 = GetMapIDFromMapPath( s1->m_szMap );
	uint64 wsid2 = GetMapIDFromMapPath( s2->m_szMap );

	// No need to actually sort by id number... we really just want to group them. It does have the effect of sorting by age though... 
	if ( wsid1 > wsid2 )
		return 1;
	else if ( wsid2 > wsid1 )
		return -1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: list column sort function
//-----------------------------------------------------------------------------
int __cdecl IPAddressCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	if ( s1->m_NetAdr < s2->m_NetAdr )
		return -1;
	else if ( s2->m_NetAdr < s1->m_NetAdr )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Ping comparison function
//-----------------------------------------------------------------------------
int __cdecl PingCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	int ping1 = s1->m_nPing;
	int ping2 = s2->m_nPing;

	if ( ping1 < ping2 )
		return -1;
	else if ( ping1 > ping2 )
		return 1;
	
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Map comparison function
//-----------------------------------------------------------------------------
int __cdecl MapCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	return Q_stricmp( V_UnqualifiedFileName( s1->m_szMap ), V_UnqualifiedFileName( s2->m_szMap ) );
}

//-----------------------------------------------------------------------------
// Purpose: Game dir comparison function
//-----------------------------------------------------------------------------
int __cdecl GameCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	// make sure we haven't added the same server to the list twice
	Assert( p1.userData != p2.userData );

	return Q_stricmp( s1->m_szGameDescription, s2->m_szGameDescription );
}

//-----------------------------------------------------------------------------
// Purpose: Server name comparison function
//-----------------------------------------------------------------------------
int __cdecl ServerNameCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	return Q_stricmp( s1->GetName(), s2->GetName() );
}

//-----------------------------------------------------------------------------
// Purpose: Player number comparison function
//-----------------------------------------------------------------------------
int __cdecl PlayersCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	int s1p = MAX( 0, s1->m_nPlayers - s1->m_nBotPlayers );
	int s1m = MAX( 0, s1->m_nMaxPlayers - s1->m_nBotPlayers );
	int s2p = MAX( 0, s2->m_nPlayers - s2->m_nBotPlayers );
	int s2m = MAX( 0, s2->m_nMaxPlayers - s2->m_nBotPlayers );

	// compare number of players
	if ( s1p > s2p )
		return -1;
	if ( s1p < s2p )
		return 1;

	// compare max players if number of current players is equal
	if ( s1m > s2m )
		return -1;
	if ( s1m < s2m )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Player number comparison function
//-----------------------------------------------------------------------------
int __cdecl LastPlayedCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer( p1.userData );
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer( p2.userData );

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	// compare number of players
	if ( s1->m_ulTimeLastPlayed > s2->m_ulTimeLastPlayed )
		return -1;
	if ( s1->m_ulTimeLastPlayed < s2->m_ulTimeLastPlayed )
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Tag comparison function
//-----------------------------------------------------------------------------
int __cdecl TagsCompare(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
	gameserveritem_t *s1 = ServerBrowserDialog().GetServer(p1.userData);
	gameserveritem_t *s2 = ServerBrowserDialog().GetServer(p2.userData);

	if ( !s1 && s2 ) 
		return -1;
	if ( !s2 && s1 )
		return 1;
	if ( !s1 && !s2 )
		return 0;

	return Q_stricmp( s1->m_szGameTags, s2->m_szGameTags );
}

