//====== Copyright Valve Corporation, All rights reserved. ====================
//
// Types and utilities used in the Steam datagram transport library
//
//=============================================================================

#ifndef STEAMDATAGRAM_SHARED_H
#define STEAMDATAGRAM_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/dbg.h"
#include "steam/steamclientpublic.h"

#pragma pack(push)
#pragma pack(8)

// API-level datagram max size.  (Actual UDP packets will be slightly bigger,
// due to the framing.)
const int k_nMaxSteamDatagramTransportPayload = 1200;

/// Max length of diagnostic error message
const int k_nMaxSteamDatagramErrMsg = 1024;

/// Used to return English-language diagnostic error messages to caller.
/// (For debugging or spewing to a console, etc.  Not intended for UI.)
typedef char SteamDatagramErrMsg[ k_nMaxSteamDatagramErrMsg ];

/// Network-routable identifier for a service.  In general, clients should
/// treat this as an opaque structure.  The only thing that is important
/// is that this contains everything the system needs to route packets to a
/// service.
struct SteamDatagramServiceNetID
{
	// Just use the private LAN address to identify the service
	uint32 m_unIP;
	uint16 m_unPort;
	uint16 m__nPad1;

	void Clear() { *(uint64 *)this = 0; }

	uint64 ConvertToUint64() const { return ( uint64(m_unIP) << 16U ) | uint64(m_unPort); }
	void SetFromUint64( uint64 x )
	{
		m_unIP = uint32(x >> 16U);
		m_unPort = uint16(x);
		m__nPad1 = 0;
	}

	inline bool operator==( const SteamDatagramServiceNetID &x ) const { return m_unIP == x.m_unIP && m_unPort == x.m_unPort; }
};

/// Ticket used to communicate with a gameserver.  This structure should be
/// considered an opaque structure!
struct SteamDatagramGameserverAuthTicket
{
	/// Ticket version
	uint32 m_nTicketVersion;

	/// Steam ID of the gameserver we want to talk to
	CSteamID m_steamIDGameserver;

	/// Steam ID of the person who was authorized.
	CSteamID m_steamIDAuthorizedSender;

	/// SteamID is authorized to send from a particular public IP.  If this
	/// is 0, then the sender is not restricted to a particular IP.
	uint32 m_unPublicIP;

	/// Time when the ticket expires.
	RTime32 m_rtimeTicketExpiry;

	/// Routing information
	SteamDatagramServiceNetID m_routing;

	/// Max length of ticket signature
	enum { k_nMaxSignatureSize = 512 };

	/// Length of signature.
	int m_cbSignature;

	/// Signature data
	uint8 m_arbSignature[ k_nMaxSignatureSize ];
};

/// Size of socket read/write buffers.  Set this before create a client
/// or server interface
extern int g_nSteamDatagramSocketBufferSize;

/// Instantaneous statistics for a link between two hosts.
struct SteamDatagramLinkInstantaneousStats
{

	/// Data rates
	float m_flOutPacketsPerSec;
	float m_flOutBytesPerSec;
	float m_flInPacketsPerSec;
	float m_flInBytesPerSec;

	/// Smoothed ping.  This will be -1 if we don't have any idea!
	int m_nPingMS;

	/// 0...1, estimated number of packets that were sent to us, but we failed to receive.
	/// <0 if we haven't received any sequenced packets and so we don't have any way to estimate this.
	float m_flPacketsDroppedPct;

	/// Packets received with a sequence number abnormality, other than basic packet loss.  (Duplicated, out of order, lurch.)
	/// <0 if we haven't received any sequenced packets and so we don't have any way to estimate this.
	float m_flPacketsWeirdSequenceNumberPct;

	/// Peak jitter
	int m_usecMaxJitter;

	void Clear();
};

/// Stats for the lifetime of a connection.
/// Should match CMsgSteamDatagramLinkLifetimeStats
struct SteamDatagramLinkLifetimeStats
{
	//
	// Lifetime counters.
	// NOTE: Average packet loss, etc can be deduced from this.
	//
	int64 m_nPacketsSent;
	int64 m_nBytesSent;
	int64 m_nPacketsRecv; // total number of packets received, some of which might not have had a sequence number.  Don't use this number to try to estimate lifetime packet loss, use m_nPacketsRecvSequenced
	int64 m_nBytesRecv;
	int64 m_nPktsRecvSequenced; // packets that we received that had a sequence number.
	int64 m_nPktsRecvDropped;
	int64 m_nPktsRecvOutOfOrder;
	int64 m_nPktsRecvDuplicate;
	int64 m_nPktsRecvSequenceNumberLurch;

	//
	// Ping distribution
	//
	int m_nPingHistogram25; // 0..25
	int m_nPingHistogram50; // 26..50
	int m_nPingHistogram75; // 51..75
	int m_nPingHistogram100; // etc
	int m_nPingHistogram125;
	int m_nPingHistogram150;
	int m_nPingHistogram200;
	int m_nPingHistogram300;
	int m_nPingHistogramMax; // >300
	int PingHistogramTotalCount() const;

	// Distribution.
	// NOTE: Some of these might be -1 if we didn't have enough data to make a meaningful estimate!
	// It takes fewer samples to make an estimate of the median than the 98th percentile!
	short m_nPingNtile5th; // 5% of ping samples were <= Nms
	short m_nPingNtile50th; // 50% of ping samples were <= Nms
	short m_nPingNtile75th; // 70% of ping samples were <= Nms
	short m_nPingNtile95th; // 95% of ping samples were <= Nms
	short m_nPingNtile98th; // 98% of ping samples were <= Nms
	short m__pad1;


	//
	// Connection quality distribution
	//
	int m_nQualityHistogram100; // This means everything was perfect.  Even if we delivered over 100 packets in the interval and we should round up to 100, we will use 99% instead.
	int m_nQualityHistogram99; // 99%+
	int m_nQualityHistogram97;
	int m_nQualityHistogram95;
	int m_nQualityHistogram90;
	int m_nQualityHistogram75;
	int m_nQualityHistogram50;
	int m_nQualityHistogram1;
	int m_nQualityHistogramDead; // we received nothing during the interval; it looks like the connection dropped
	int QualityHistogramTotalCount() const;

	// Distribution.  Some might be -1, see above for why.
	short m_nQualityNtile2nd; // 2% of measurement intervals had quality <= N%
	short m_nQualityNtile5th; // 5% of measurement intervals had quality <= N%
	short m_nQualityNtile25th; // 25% of measurement intervals had quality <= N%
	short m_nQualityNtile50th; // 50% of measurement intervals had quality <= N%

	// Jitter histogram
	int m_nJitterHistogramNegligible;
	int m_nJitterHistogram1;
	int m_nJitterHistogram2;
	int m_nJitterHistogram5;
	int m_nJitterHistogram10;
	int m_nJitterHistogram20;
	int JitterHistogramTotalCount() const;

	void Clear();
};

/// Link stats.  Pretty much everything you might possibly want to know about the connection
struct SteamDatagramLinkStats
{

	/// Latest instantaneous stats, calculated locally
	SteamDatagramLinkInstantaneousStats m_latest;

	/// Peak values for each instantaneous stat
	//SteamDatagramLinkInstantaneousStats m_peak;

	/// Lifetime stats, calculated locally
	SteamDatagramLinkLifetimeStats m_lifetime;

	/// Latest instantaneous stats received from remote host.
	/// (E.g. "sent" means they are reporting what they sent.)
	SteamDatagramLinkInstantaneousStats m_latestRemote;

	/// How many seconds ago did we receive m_latestRemote?
	/// This will be <0 if the data is not valid!
	float m_flAgeLatestRemote;

	/// Latest lifetime stats received from remote host.
	SteamDatagramLinkLifetimeStats m_lifetimeRemote;

	/// How many seconds ago did we receive the lifetime stats?
	/// This will be <0 if the data is not valid!
	float m_flAgeLifetimeRemote;

	/// Reset everything to unknown/initial state.
	void Clear();

	/// Print into a buffer.  Returns the number of characters copied,
	/// or needed if you pass NULL.  (Includes the '\0' terminator in
	/// both cases.)
	int Print( char *pszBuf, int cbBuf ) const;
};

/// Identifier used for a datacenter.
typedef uint32 SteamDataCenterID;

// Convert 3-character string to ID
inline SteamDataCenterID CalculateSteamDataCenterIDFromCode( const char *pszCode )
{
	// OK I made a bad decision when I decided how to pack 3-character codes into a uint32.  I'd like to support 4-character
	// codes, but I don't want to break compatibility.  The migration path has some subtleties that make it nontrivial, and
	// we're already storing them in SQL.  Ug, so the 4 character code "abcd" will be encoded with the digits "dabc".
	return
		( uint32(pszCode[3]) << 24U ) 
		| ( uint32(pszCode[0]) << 16U ) 
		| ( uint32(pszCode[1]) << 8U )
		| uint32(pszCode[2]);
}

/// Data center code, as string
template <int N>
inline void GetSteamDataCenterStringFromID( SteamDataCenterID id, char (&szCode)[N] )
{
	COMPILE_TIME_ASSERT( N >= 5 );
	szCode[0] = ( id >> 16U );
	szCode[1] = ( id >> 8U );
	szCode[2] = ( id );
	szCode[3] = ( id >> 24U ); // See comment above about the deep regret and sadness I feel about this
	szCode[4] = 0;
}

/// Fetch current time
extern uint64 SteamDatagram_GetCurrentTime();

/// Sometimes we put "fake" relays into the network config, which are used purely
/// to measure ping.
enum ESteamDatagramRouterType
{
	k_ESteamDatagramRouterType_Normal,
	k_ESteamDatagramRouterType_PingOnly,
	//k_ESteamDatagramRouterType_FakeGameserver,
};

enum ESteamDatagramPartner
{
	k_ESteamDatagramPartner_None = -1,
	k_ESteamDatagramPartner_Steam = 0,
	k_ESteamDatagramPartner_China = 1,
};

#pragma pack(pop)

#endif // STEAMDATAGRAM_SHARED_H
