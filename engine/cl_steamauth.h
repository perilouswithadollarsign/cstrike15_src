//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: steam state machine that handles authenticating steam users
//
//=============================================================================//

#ifndef CL_STEAMUAUTH_H
#define CL_STEAMUAUTH_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steam_api.h"

class CSteam3Client : public CSteamAPIContext
{
public:
	CSteam3Client();
	~CSteam3Client();

	void Activate();
	void Shutdown();
	
	bool IsInitialized()const { return m_bInitialized; }

	void GetAuthSessionTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket, uint64 unGSSteamID, bool bSecure );
	void CancelAuthTicket();
	bool BGSSecure() { return m_bGSSecure; }
	void RunFrame();
	bool IsGameOverlayActive()const { return m_bGameOverlayActive; }
#if !defined(NO_STEAM)
	STEAM_CALLBACK( CSteam3Client, OnClientGameServerDeny, ClientGameServerDeny_t, m_CallbackClientGameServerDeny );
	STEAM_CALLBACK( CSteam3Client, OnGameServerChangeRequested, GameServerChangeRequested_t, m_CallbackGameServerChangeRequested );
	STEAM_CALLBACK( CSteam3Client, OnGameOverlayActivated, GameOverlayActivated_t, m_CallbackGameOverlayActivated );
	STEAM_CALLBACK( CSteam3Client, OnPersonaUpdated, PersonaStateChange_t, m_CallbackPersonaStateChanged );
	STEAM_CALLBACK( CSteam3Client, OnLowBattery, LowBatteryPower_t, m_CallbackLowBattery );
	STEAM_CALLBACK( CSteam3Client, OnSteamSocketStatus, SocketStatusCallback_t, m_CallbackSteamSocketStatus );
#endif

private:
	HAuthTicket m_hAuthTicket;
	bool m_bActive;
	bool m_bGSSecure;
	bool m_bGameOverlayActive;
	bool m_bInitialized;
};

#ifndef DEDICATED
// singleton accessor
CSteam3Client &Steam3Client();
#endif

inline bool IsSteam3ClientGameOverlayActive()
{
#ifndef DEDICATED
	return Steam3Client().IsGameOverlayActive();
#else
	return false; // dedicated server has no overlays
#endif	
}

#endif // CL_STEAMUAUTH_H
