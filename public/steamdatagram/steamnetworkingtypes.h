//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Purpose: misc networking utilities
//
//=============================================================================

#ifndef STEAMNETWORKINGTYPES
#define STEAMNETWORKINGTYPES
#ifdef _WIN32
#pragma once
#endif

#include "steamtypes.h"
#include "steamclientpublic.h"

struct SteamNetworkPingLocation_t;
class ISteamNetworkingMessage;
class CMsgSteamDatagramGameServerAuthTicket; // Ug

typedef uint32 SNetSocket_t; // Duplicate from the sockets API.  That file should eventually include this one

/// Identifier used for a network location point of presence.
/// Typically you won't need to directly manipulate these.
typedef uint32 SteamNetworkingPOPID;

/// A local timestamp.  You can subtract two timestamps to get the number of elapsed
/// microseconds.  This is guaranteed to increase over time during the lifetime
/// of a process, but not globally across runs.  You don't need to worry about
/// the value wrapping around.  Note that the underlying clock might not actually have
/// microsecond *resolution*.
typedef int64 SteamNetworkingLocalTimestamp;

/// A message that has been received.
class ISteamNetworkingMessage
{
	/// Call this when you're done with the object, to free up memory,
	/// etc.
	virtual void Release() = 0;

	/// Get size of the payload.
	inline uint32 GetSize() const { return m_cbSize; }

	/// Get message payload
	inline const void *GetData() const { return m_pData; }

	/// Return the channel number the message was received on
	inline int GetChannel() const { return m_nChannel; }

	/// Return SteamID that sent this to us.
	inline CSteamID GetSenderSteamID() const { return m_steamIDSender; }

	/// The socket this came from.  (Not used when using the P2P calls)
	inline SNetSocket_t GetSocket() const { return m_sock; }

protected:
	CSteamID m_steamIDSender;
	void *m_pData;
	uint32 m_cbSize;
	int m_nChannel;
	SNetSocket_t m_sock;

private:
	virtual ~ISteamNetworkingMessage() {}
};

/// Object that describes a "location" on the Internet with sufficient
/// detail that we can reasonably estimate an upper bound on the ping between
/// the two hosts, even if a direct route between the hosts is not possible,
/// and the connection must be routed through the Steam Datagram Relay network.
/// This does not contain any information that identifies the host.  Indeed,
/// if two hosts are in the same building or otherwise has nearly identical
/// networking characteristics, then it's valid to use the same location
/// object for both of them.
///
/// NOTE: This object should only be used in memory.  If you need to persist
/// it or send it over the wire, convert it to a string representation using
/// the methods in ISteamNetworkingUtils()
struct SteamNetworkPingLocation_t
{
	uint8 m_data[ 64 ];
};

/// Special ping values that are returned by some values that return a ping.
const int k_nSteamNetworkingPing_Failed = -1;
const int k_nSteamNetworkingPing_Unknown = -2;

#endif // STEAMNETWORKINGUTILS
