//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Ownership id for a shared object cache
//
//=============================================================================

#ifndef SOID_H
#define SOID_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steamclientpublic.h"

class CMsgSOIDOwner;

namespace GCSDK
{

//----------------------------------------------------------------------------
// Shared type for object caches. This can hold SteamIDs, LobbyIDs, PartyIds,
// etc.  Make sure they don't conflict!
//----------------------------------------------------------------------------

struct SOID_t;

const uint32 k_SOID_Type_SteamID = 1;
const uint32 k_SOID_Type_PartyGroupID = 2;
const uint32 k_SOID_Type_LobbyGroupID = 3;
const uint32 k_SOID_Type_PartyInvite = 4;
const uint32 k_SOID_Type_CheatReport = 5;
const uint32 k_SOID_Type_NqmmRating = 6;

struct SOIDRender_t
{
	explicit SOIDRender_t( const SOID_t id );
	const char *String() const;

	//the buffer that is formatted into (should be large enough to hold the string representation of the type and the value)
	static const uint32 k_cBufLen = 128;
	char m_buf[ k_cBufLen ];

	//a utility class that is intended to be defined in a source file that will handle registering
	//the provided name and whether or not it should be displayed as a steam ID with the lock
	class CAutoRegisterName
	{
	public:
		CAutoRegisterName( uint16 nType, const char* pszDefaultString, bool bDisplaySteamID = false );
	};

	static const char *GetName( uint32 nType );
};

struct SOID_t
{
	SOID_t()
	: m_type( 0 )
	, m_id( 0 )
	, m_padding( 0 )
	{
	}

	SOID_t( uint32 type, uint64 id )
	: m_type( type )
	, m_id( id )
	, m_padding( 0 )
	{
	}

	// Conversion from a SteamID
	SOID_t( CSteamID steamID )
	: m_type( k_SOID_Type_SteamID )
	, m_id( steamID.ConvertToUint64() )
	, m_padding( 0 )
	{
	}

	//initializes the soid fields
	void Init( uint32 type, uint64 id )
	{
		m_type = type;
		m_id = id;
	}

	// Conversion from a protobuf version
	SOID_t( const CMsgSOIDOwner &msgSOIDOwner );

	void ToMsgSOIDOwner( CMsgSOIDOwner *pMsgSOIDOwner ) const;

	uint64 ID() const
	{
		return m_id;
	}

	uint32 Type() const
	{
		return m_type;
	}

	bool IsValid()
	{
		return m_type != 0;
	}

	bool operator==( const SOID_t &rhs ) const
	{
		return m_type == rhs.m_type && m_id == rhs.m_id;
	}

	bool operator!=( const SOID_t &rhs ) const
	{
		return m_type != rhs.m_type || m_id != rhs.m_id;
	}

	bool operator<( const SOID_t &rhs ) const
	{
		if ( m_type == rhs.m_type )
		{
			return m_id < rhs.m_id;
		}
		return m_type < rhs.m_type;
	}

	SOIDRender_t GetRender() const
	{
		return SOIDRender_t( *this );
	}

	uint64 m_id;
	uint32 m_type;
	uint32 m_padding; // so structure is 16 bytes
};

inline const char *SOIDRender_t::String() const
{
	return m_buf;
}

inline SOID_t GetSOIDFromSteamID( CSteamID steamID )
{
	return SOID_t( k_SOID_Type_SteamID, steamID.ConvertToUint64() );
}

inline CSteamID GetSteamIDFromSOID( SOID_t ID )
{
	if ( ID.Type() == k_SOID_Type_SteamID )
	{
		return CSteamID( ID.ID() );
	}
	return k_steamIDNil;
}

} // namespace GCSDK


#endif //SOID_H

