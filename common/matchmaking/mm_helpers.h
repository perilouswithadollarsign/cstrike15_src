//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: common routines to operate on matchmaking sessions and members
// Assumptions: caller should include all required headers before including mm_helpers.h
//
//===========================================================================//

#ifndef __COMMON__MM_HELPERS_H_
#define __COMMON__MM_HELPERS_H_
#ifdef _WIN32
#pragma once
#endif

#include "tier1/keyvalues.h"
#include "tier1/fmtstr.h"


//
// Contains inline functions to deal with common tasks involving matchmaking and sessions
//

inline KeyValues * SessionMembersFindPlayer( KeyValues *pSessionSettings, XUID xuidPlayer, KeyValues **ppMachine = NULL )
{
	if ( ppMachine )
		*ppMachine = NULL;

	if ( !pSessionSettings )
		return NULL;

	KeyValues *pMembers = pSessionSettings->FindKey( "Members" );
	if ( !pMembers )
		return NULL;

	int numMachines = pMembers->GetInt( "numMachines" );
	for ( int k = 0; k < numMachines; ++ k )
	{
		KeyValues *pMachine = pMembers->FindKey( CFmtStr( "machine%d", k ) );
		if ( !pMachine )
			continue;

		int numPlayers = pMachine->GetInt( "numPlayers" );
		for ( int j = 0; j < numPlayers; ++ j )
		{
			KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", j ) );
			if ( !pPlayer )
				continue;

			if ( pPlayer->GetUint64( "xuid" ) == xuidPlayer )
			{
				if ( ppMachine )
					*ppMachine = pMachine;

				return pPlayer;
			}
		}
	}

	return NULL;
}

inline XUID SessionMembersFindNonGuestXuid( XUID xuid )
{
#ifdef _X360
	if ( !g_pMatchFramework )
		return xuid;

	if ( !g_pMatchFramework->GetMatchSession() )
		return xuid;

	KeyValues *pMachine = NULL;
	KeyValues *pPlayer = SessionMembersFindPlayer( g_pMatchFramework->GetMatchSession()->GetSessionSettings(), xuid, &pMachine );
	if ( !pPlayer || !pMachine )
		return xuid;

	if ( !strchr( pPlayer->GetString( "name" ), '(' ) )
		return xuid;

	int numPlayers = pMachine->GetInt( "numPlayers" );
	for ( int k = 0; k < numPlayers; ++ k )
	{
		XUID xuidOtherPlayer = pMachine->GetUint64( CFmtStr( "player%d/xuid", k ) );
		if ( xuidOtherPlayer && !strchr( pMachine->GetString( CFmtStr( "player%d/xuid", k ) ), '(' ) )
			return xuidOtherPlayer;	// found a replacement that is not guest
	}
#endif

	return xuid;
}

inline TitleDataFieldsDescription_t const * TitleDataFieldsDescriptionFindByString( TitleDataFieldsDescription_t const *fields, char const *szString )
{
	if ( !szString )
		return NULL;
	for ( ; fields && fields->m_szFieldName; ++ fields )
	{
		if ( !Q_stricmp( fields->m_szFieldName, szString ) )
			return fields;
	}
	return NULL;
}

inline bool TitleDataFieldsDescriptionGetBit( TitleDataFieldsDescription_t const *fdKey, IPlayerLocal *pPlayer )
{
	Assert( pPlayer );
	Assert( fdKey );
	Assert( fdKey->m_eDataType == TitleDataFieldsDescription_t::DT_BITFIELD );
	return !!( ( *( const uint8 * )( ( ( const char * )pPlayer->GetPlayerTitleData( fdKey->m_iTitleDataBlock ) ) + (fdKey->m_numBytesOffset/8) ) ) & ( 1 << ( fdKey->m_numBytesOffset%8 ) ) );
}

inline void TitleDataFieldsDescriptionSetBit( TitleDataFieldsDescription_t const *fdKey, IPlayerLocal *pPlayer, bool bBitValue )
{
	Assert( pPlayer );
	Assert( fdKey );
	Assert( fdKey->m_eDataType == TitleDataFieldsDescription_t::DT_BITFIELD );
	uint8 uiValue = static_cast< uint8 >( bBitValue ? (~0u) : 0u );
	pPlayer->UpdatePlayerTitleData( fdKey, &uiValue, sizeof( uiValue ) );
}

template < typename T >
inline T TitleDataFieldsDescriptionGetValue( TitleDataFieldsDescription_t const *fdKey, IPlayerLocal *pPlayer )
{
	Assert( pPlayer );
	Assert( fdKey );
	Assert( ( fdKey->m_eDataType != TitleDataFieldsDescription_t::DT_BITFIELD ) && ( fdKey->m_eDataType != TitleDataFieldsDescription_t::DT_0 ) );
	return *( T const * )(&( (char const*) pPlayer->GetPlayerTitleData( fdKey->m_iTitleDataBlock ) )[(fdKey->m_numBytesOffset)]);
}

template < typename T > // T is primitive type, passing by value instead of constref to avoid compiler troubles when passing temporaries
inline void TitleDataFieldsDescriptionSetValue( TitleDataFieldsDescription_t const *fdKey, IPlayerLocal *pPlayer, T val )
{
	Assert( pPlayer );
	Assert( fdKey );
	Assert( ( fdKey->m_eDataType != TitleDataFieldsDescription_t::DT_BITFIELD ) && ( fdKey->m_eDataType != TitleDataFieldsDescription_t::DT_0 ) );
	pPlayer->UpdatePlayerTitleData( fdKey, &val, sizeof( T ) );
}


#endif // __COMMON__MM_HELPERS_H_

