//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: interface to steam.dll, for bridging steam2 data into steam3 client
//
//=============================================================================

#ifndef ISTEAM2BRIDGE_H
#define ISTEAM2BRIDGE_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CreateProcess
#undef CreateProcess
#endif

//-----------------------------------------------------------------------------
// Purpose: bridge functions to steam.dll
//-----------------------------------------------------------------------------
class ISteam2Bridge
{
public:
	virtual void SetSteam2Ticket( uint8 *pubTicket, int cubTicket ) = 0;
	virtual void SetAccountName( const char *pchAccountName ) = 0;
	virtual bool SetPassword( const char *pchPassword ) = 0;
	virtual void SetAccountCreationTime( RTime32 rt ) = 0;
	virtual bool CreateProcess( void *lpVACBlob, uint cbBlobSize, const char *lpApplicationName, char *lpCommandLine, uint32 dwCreationFlags, void *lpEnvironment, char *lpCurrentDirectory, uint32 nGameID ) = 0;
	virtual EUniverse GetConnectedUniverse() = 0;
	virtual const char *GetIPCountry() = 0;

	virtual uint32 GetNumLicenses() = 0;
	virtual int32 GetLicensePackageID( uint32 nLicenseIndex ) = 0;
	virtual uint32 GetLicenseTimeCreated( uint32 nLicenseIndex ) = 0;
	virtual uint32 GetLicenseTimeNextProcess( uint32 nLicenseIndex ) = 0;
	virtual int32 GetLicenseMinuteLimit( uint32 nLicenseIndex ) = 0;
	virtual int32 GetLicenseMinutesUsed( uint32 nLicenseIndex ) = 0;
	virtual EPaymentMethod GetLicensePaymentMethod( uint32 nLicenseIndex ) = 0;
	virtual uint32 GetLicenseFlags( uint32 nLicenseIndex ) = 0;
	virtual const char * GetLicensePurchaseCountryCode( uint32 nLicenseIndex ) = 0;

	virtual void SetOfflineMode( bool bOffline ) = 0;

	virtual uint64 GetCurrentSessionToken() = 0;

	virtual void SetCellID( CellID_t cellID ) = 0;
	virtual void SetSteam2FullASTicket( uint8 *pubTicket, int cubTicket ) = 0;

	virtual bool BUpdateAppOwnershipTicket( uint32 nAppID, bool bOnlyUpdateIfStale ) = 0;

	// Gets the length of the current ticket for the given appid, 0 means no ticket available
	virtual uint32 GetAppOwnershipTicketLength( uint32 nAppID ) = 0;

	// Gets the data for the app ownership ticket for a given appid.  Returns the length of the buffer
	// which was used, or 0 if the buffer was too small to contain the ticket (and signature which is always on the end).
	virtual uint32 GetAppOwnershipTicketData( uint32 nAppID, void *pvBuffer, uint32 cbBufferLength ) = 0;

	virtual bool GetAppDecryptionKey( uint32 nDepotID, void *pvBuffer, uint32 cbBufferLength ) = 0; // this is for depots
	
	virtual const char *GetPlatformName( bool *bIs64Bit ) = 0;

	virtual int32 GetSteam2FullASTicket( uint8 *pubTicket, int cubTicket ) = 0;
};

#define STEAM2BRIDGE_INTERFACE_VERSION "STEAM2BRIDGE_INTERFACE_VERSION002"

#ifndef ICLIENTUSER_H
//-----------------------------------------------------------------------------
// Purpose: Signaled whenever licenses change
//-----------------------------------------------------------------------------
struct LicensesUpdated_t
{
	enum { k_iCallback = k_iSteamUserCallbacks + 25 };
};

//-----------------------------------------------------------------------------
// Purpose: Status of a Steam-launched application lifetime
//-----------------------------------------------------------------------------
struct AppLifetimeNotice_t
{
	enum { k_iCallback = k_iSteamUserCallbacks + 30 };

	int32 m_nAppID;			// AppID - subset of gameid, left in for backcompat to steam2 listener.
	int32 m_nInstanceID;	// Instance ID of this App
	bool m_bExiting;		// launched if false, exiting if true
	CGameID	m_gameID;		// the full game id, Steam2 doesn't see this
};

#endif


#endif // ISTEAM2BRIDGE_H
