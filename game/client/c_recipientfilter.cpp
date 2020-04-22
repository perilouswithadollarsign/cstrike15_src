//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "prediction.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static IPredictionSystem g_RecipientFilterPredictionSystem;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_RecipientFilter::C_RecipientFilter()
{
	Reset();
}

C_RecipientFilter::~C_RecipientFilter()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : src - 
//-----------------------------------------------------------------------------
void C_RecipientFilter::CopyFrom( const C_RecipientFilter& src )
{
	m_bReliable = src.IsReliable();
	m_bInitMessage = src.IsInitMessage();

	m_bUsingPredictionRules = src.IsUsingPredictionRules();
	m_bIgnorePredictionCull = src.IgnorePredictionCull();

	int c = src.GetRecipientCount();
	for ( int i = 0; i < c; ++i )
	{
		m_Recipients.AddToTail( src.GetRecipientIndex( i ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_RecipientFilter::Reset( void )
{
	m_bReliable			= false;
	m_Recipients.RemoveAll();
	m_bUsingPredictionRules = false;
	m_bIgnorePredictionCull = false;
}

void C_RecipientFilter::MakeReliable( void )
{
	m_bReliable = true;
}

bool C_RecipientFilter::IsReliable( void ) const
{
	return m_bReliable;
}

int C_RecipientFilter::GetRecipientCount( void ) const
{
	return m_Recipients.Count();
}

int	C_RecipientFilter::GetRecipientIndex( int slot ) const
{
	if ( slot < 0 || slot >= GetRecipientCount() )
		return -1;

	return m_Recipients[ slot ];
}

void C_RecipientFilter::AddAllPlayers( void )
{
	if ( !C_BasePlayer::HasAnyLocalPlayer() )
		return;

	m_Recipients.RemoveAll();
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		AddRecipient( C_BasePlayer::GetLocalPlayer( hh ) );
	}
}

void C_RecipientFilter::AddRecipient( C_BasePlayer *player )
{
	if ( !player )
		return;

	int index = player->index;

	// If we're predicting and this is not the first time we've predicted this sound
	//  then don't send it to the local player again.
	if ( m_bUsingPredictionRules )
	{
		Assert( C_BasePlayer::IsLocalPlayer( player ) );
		Assert( prediction->InPrediction() );

		// Only add local player if this is the first time doing prediction
		if ( !g_RecipientFilterPredictionSystem.CanPredict() )
		{
			return;
		}
	}

	// Already in list
	if ( m_Recipients.Find( index ) != m_Recipients.InvalidIndex() )
		return;

	// this is a client side filter, only add the local player
	if ( !C_BasePlayer::IsLocalPlayer( player ) )
		return;

	m_Recipients.AddToTail( index );
}

void C_RecipientFilter::RemoveRecipient( C_BasePlayer *player )
{
	if ( !player )
		return;

	int index = player->index;

	// Remove it if it's in the list
	m_Recipients.FindAndRemove( index );
}

void C_RecipientFilter::AddRecipientsByTeam( C_Team *team )
{
	AddAllPlayers();
}

void C_RecipientFilter::RemoveRecipientsByTeam( C_Team *team )
{
	Assert ( 0 );
}


void C_RecipientFilter::AddPlayersFromBitMask( CPlayerBitVec& playerbits )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer( hh );
		if ( !pPlayer )
			continue;

		// only add the local player on client side
		if ( !playerbits[ pPlayer->index ] )
			continue;

		AddRecipient( pPlayer );
	}
}

void C_RecipientFilter::AddRecipientsByPVS( const Vector& origin )
{
	AddAllPlayers();
}

void C_RecipientFilter::AddRecipientsByPAS( const Vector& origin )
{
	AddAllPlayers();
}

void C_RecipientFilter::UsePredictionRules( void )
{
	if ( m_bUsingPredictionRules )
		return;

	if ( !prediction->InPrediction() )
	{
		Assert( 0 );
		return;
	}

	if ( !C_BasePlayer::HasAnyLocalPlayer() )
	{
		Assert( 0 );
		return;
	}

	m_bUsingPredictionRules = true;

	// Cull list now, if needed
	int c = GetRecipientCount();
	if ( c == 0 )
		return;

	if ( !g_RecipientFilterPredictionSystem.CanPredict() )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			RemoveRecipient( C_BasePlayer::GetLocalPlayer( hh ) );
		}
	}
}

void C_RecipientFilter::RemoveSplitScreenPlayers()
{
	for ( int i = GetRecipientCount() - 1; i >= 0; --i )
	{
		int idx = m_Recipients[ i ];
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( idx );
		if ( !pPlayer || !pPlayer->IsSplitScreenPlayer() )
			continue;

		m_Recipients.Remove( i );
	}
}

// THIS FUNCTION IS SUFFICIENT FOR PORTAL2 SPECIFIC CIRCUMSTANCES
// AND MAY OR MAY NOT FUNCTION AS EXPECTED WHEN USED WITH MULTIPLE
// SPLITSCREEN CLIENTS NETWORKED TOGETHER, ETC.
void C_RecipientFilter::ReplaceSplitScreenPlayersWithOwners()
{
	// coop
	if( gpGlobals->maxClients >= 2 && C_BasePlayer::HasAnyLocalPlayer() ) 
	{
		bool addedHostPlayer = false;
		const int count = m_Recipients.Count();
		for( int i = 0; i < count; ++i )
		{
			// If this is a split screen player
			CBasePlayer* pPlayer = UTIL_PlayerByIndex( m_Recipients[i] );
			if( pPlayer && pPlayer->IsSplitScreenPlayer() )
			{
				// Add the host player, if it exists, to the filter
				C_BasePlayer* pHostPlayer = C_BasePlayer::GetLocalPlayer( 0 );
				if( pHostPlayer )
				{
					m_Recipients.AddToTail( pHostPlayer->entindex() );
					addedHostPlayer = true;
				}
				break;
			}
		}

		if( addedHostPlayer )
		{
			// Remove all split screen players
			RemoveSplitScreenPlayers();

			// Remove duplicates
			RemoveDuplicateRecipients();
		}
	}
}
bool C_RecipientFilter::IsUsingPredictionRules( void ) const
{
	return m_bUsingPredictionRules;
}

bool C_RecipientFilter::IgnorePredictionCull( void ) const
{
	return m_bIgnorePredictionCull;
}

void C_RecipientFilter::SetIgnorePredictionCull( bool ignore )
{
	m_bIgnorePredictionCull = ignore;
}

void C_RecipientFilter::RemoveDuplicateRecipients()
{
	for( int i = 0; i < m_Recipients.Count(); ++i )
	{
		int currentElem = m_Recipients[i];
		for( int j = m_Recipients.Count() - 1; j > i ; --j )
		{
			if( m_Recipients[j] == currentElem )
				m_Recipients.FastRemove( j );
		}
	}
}

CLocalPlayerFilter::CLocalPlayerFilter()
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		if ( C_BasePlayer::GetLocalPlayer() )
		{
			AddRecipient( C_BasePlayer::GetLocalPlayer() );
		}
	}
}
