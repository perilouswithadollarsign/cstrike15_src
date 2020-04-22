//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Entity that propagates general data needed by clients for every player.
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_PLAYERRESOURCE_H
#define C_PLAYERRESOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "const.h"
#include "c_baseentity.h"
#include <igameresources.h>

#define PLAYER_UNCONNECTED_NAME	"unconnected"
#define PLAYER_ERROR_NAME		"ERRORNAME"

class C_PlayerResource : public C_BaseEntity, public IGameResources, public IShaderDeviceDependentObject
{
	DECLARE_CLASS( C_PlayerResource, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

					C_PlayerResource();
	virtual			~C_PlayerResource();

public : // IGameResources intreface

	// Team data access 
	virtual int		GetTeamScore( int index );
	virtual const char *GetTeamName( int index );
	virtual const Color&GetTeamColor( int index );

	// Player data access
	virtual bool	IsConnected( int index );
	virtual bool	IsAlive( int index );
	virtual bool	IsFakePlayer( int index );
	virtual bool	IsLocalPlayer( int index  );
	virtual bool	IsHLTV(int index);
#if defined( REPLAY_ENABLED )
	virtual bool	IsReplay(int index);
#endif

	virtual const char *GetPlayerName( int index );
	virtual int		GetPing( int index );
//	virtual int		GetPacketloss( int index );
	virtual int		GetKills( int index );
	virtual int		GetAssists( int index );
	virtual int		GetDeaths( int index );
	virtual int		GetTeam( int index );
	virtual int		GetPendingTeam( int index );
	virtual int		GetFrags( int index );
	virtual int		GetHealth( int index );
	virtual int		GetCoachingTeam( int index );

	virtual void	ClientThink();
	virtual	void	OnDataChanged(DataUpdateType_t updateType);
	virtual void	DeviceLost( void );
	virtual void	DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd );
	virtual void	ScreenSizeChanged( int width, int height ) { }
	virtual void	TeamChanged( void ){ }

	// Returns the xuid for the given player, but only if they are connected
	XUID			GetXuid( int index );
	void			FillXuidText( int iIndex, char *buf, int bufSize );

protected:
	virtual void	UpdatePlayerName( int slot );
	void	UpdateAsLocalizedFakePlayerName( int slot, char const *pchRawPlayerName );
	void	UpdateXuids( void );

	// Data for each player that's propagated to all clients
	// Stored in individual arrays so they can be sent down via datatables
	string_t	m_szName[MAX_PLAYERS+1];
	int		m_iPing[MAX_PLAYERS+1];
	int		m_iKills[MAX_PLAYERS+1];
	int		m_iAssists[MAX_PLAYERS+1];
	int		m_iDeaths[MAX_PLAYERS+1];
	bool	m_bConnected[MAX_PLAYERS+1];
	int		m_iTeam[MAX_PLAYERS+1];
	int		m_iPendingTeam[MAX_PLAYERS+1];
	bool	m_bAlive[MAX_PLAYERS+1];
	int		m_iHealth[MAX_PLAYERS+1];
	Color	m_Colors[MAX_TEAMS];
	int		m_iCoachingTeam[MAX_PLAYERS+1];
	
	XUID	m_Xuids[MAX_PLAYERS+1];
};

extern C_PlayerResource *g_PR;

#endif // C_PLAYERRESOURCE_H
