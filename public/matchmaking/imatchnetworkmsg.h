//===== Copyright c 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IMATCHNETWORKMSG_H
#define IMATCHNETWORKMSG_H

#ifdef _WIN32
#pragma once
#endif


struct MM_QOS_t
{
	int nPingMsMin;		// Minimum round-trip time in ms
	int nPingMsMed;		// Median round-trip time in ms
	float flBwUpKbs;	// Bandwidth upstream in kilobytes/s
	float flBwDnKbs;	// Bandwidth downstream in kilobytes/s
	float flLoss;		// Average packet loss in percents
};

struct MM_GameDetails_QOS_t
{
	void *m_pvData;	// Encoded game details
	int m_numDataBytes; // Length of game details

	int m_nPing;	// Average ping in ms
};

abstract_class IMatchNetworkMsgController
{
public:
	// To determine host Quality-of-Service
	virtual MM_QOS_t GetQOS() = 0;

	virtual KeyValues * GetActiveServerGameDetails( KeyValues *pRequest ) = 0;

	virtual KeyValues * UnpackGameDetailsFromQOS( MM_GameDetails_QOS_t const *pvQosReply ) = 0;
	virtual KeyValues * UnpackGameDetailsFromSteamLobby( uint64 uiLobbyID ) = 0;

	virtual void PackageGameDetailsForQOS( KeyValues *pSettings, CUtlBuffer &buf ) = 0;

	virtual KeyValues * PackageGameDetailsForReservation( KeyValues *pSettings ) = 0;
};

#endif // IMATCHNETWORKMSG_H