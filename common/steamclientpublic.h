//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef STEAMCLIENTPUBLIC_H
#define STEAMCLIENTPUBLIC_H
#ifdef _WIN32
#pragma once
#endif
//lint -save -e1931 -e1927 -e1924 -e613 -e726

// This header file defines the interface between the calling application and the code that
// knows how to communicate with the connection manager (CM) from the Steam service 

// This header file is intended to be portable; ideally this 1 header file plus a lib or dll
// is all you need to integrate the client library into some other tree.  So please avoid
// including or requiring other header files if possible.  This header should only describe the 
// interface layer, no need to include anything about the implementation.

#include "steamtypes.h"

#ifndef NETADR_H
class netadr_t;
#endif

class ICMInterface;		// forward declaration
class ICMCallback;		// forward declaration
class IVConnCallback;	// forward declaration

extern "C" {
	ICMInterface * CreateCMInterface( ICMCallback * pCMCallback );			// global function to create an interface object
	typedef ICMInterface * (* LPCREATECMINTERFACEPROC)( ICMCallback * );	// typedef for function pointer (for GetProcAddress)

	void DestroyCMInterface( ICMInterface * pCMInterface );					// global function to destroy and free an interface object
	typedef void (* LPDESTROYCMINTERFACEPROC)( ICMInterface * );			// typedef for function pointer (for GetProcAddress)
}


// General result codes
enum EResult
{
	k_EResultOK	= 1,							// success
	k_EResultFail = 2,							// generic failure 
	k_EResultNoConnection = 3,					// no/failed network connection
	k_EResultNoConnectionRetry = 4,				// no/failed network connection, will retry
	k_EResultInvalidPassword = 5,				// password/ticket is invalid
	k_EResultLoggedInElsewhere = 6,				// same user logged in elsewhere
	k_EResultInvalidProtocolVer = 7,			// protocol version is incorrect
	k_EResultInvalidParam = 8,					// a parameter is incorrect
	k_EResultFileNotFound = 9,					// file was not found
	k_EResultBusy = 10,							// called method busy - action not taken
	k_EResultInvalidState = 11,					// called object was in an invalid state
	k_EResultInvalidName = 12,					// name is invalid
	k_EResultInvalidEmail = 13,					// email is invalud
	k_EResultDuplicateName = 14,				// name is not unique
	k_EResultAccessDenied = 15,					// access is denied
	k_EResultTimeout = 16,						// operation timed out
};

// Result codes to GSHandleClientDeny/Kick
typedef enum
{
	k_EDenyInvalidVersion = 1,
	k_EDenyGeneric,
	k_EDenyNotLoggedOn,
	k_EDenyNoLicense,
	k_EDenyCheater,
} EDenyReason;

// Server flags.
const uint k_unServerFlagNone = 0;
const uint k_unServerFlagActive = 1;
const uint k_unServerFlagInsecure = 2;

// Steam universes.  Each universe is a self-contained Steam instance.
enum EUniverse
{
	k_EUniverseInvalid = 0,
	k_EUniversePublic = 1,
	k_EUniverseTestPublic = 2,
	k_EUniverseInternal = 3,

	k_EUniverseMax
};


EUniverse EUniverseFromName( const char * pchUniverseName );
const char * PchNameFromEUniverse( EUniverse eUniverse );

// Steam account types
enum EAccountType
{
	k_EAccountTypeInvalid = 0,			
	k_EAccountTypeIndividual = 1,		// single user account
	k_EAccountTypeMultiseat = 2,		// multiseat (e.g. cybercafe) account
	k_EAccountTypeGameServer = 3,		// game server account
	k_EAccountTypeAnonGameServer = 4,	// anonomous game server account
	k_EAccountTypePending = 5			// pending
};

#pragma pack( push, 1 )		

// Steam ID structure (64 bits total)
class CSteamID
{
public:

	//-----------------------------------------------------------------------------
	// Purpose: Constructor
	//-----------------------------------------------------------------------------
	CSteamID( )
	{
		m_unAccountID = 0;
		m_EAccountType = k_EAccountTypeInvalid;
		m_EUniverse = k_EUniverseInvalid;
		m_unAccountInstance = 0;
	}


	//-----------------------------------------------------------------------------
	// Purpose: Constructor
	// Input  : unAccountID -	32-bit account ID
	//			eUniverse -		Universe this account belongs to
	//			eAccountType -	Type of account
	//-----------------------------------------------------------------------------
	CSteamID( uint32 unAccountID, EUniverse eUniverse, EAccountType eAccountType )
	{
		Set( unAccountID, eUniverse, eAccountType );
	}


	//-----------------------------------------------------------------------------
	// Purpose: Constructor
	// Input  : unAccountID -	32-bit account ID
	//			unAccountInstance - instance 
	//			eUniverse -		Universe this account belongs to
	//			eAccountType -	Type of account
	//-----------------------------------------------------------------------------
	CSteamID( uint32 unAccountID, unsigned int unAccountInstance, EUniverse eUniverse, EAccountType eAccountType )
	{
		InstancedSet( unAccountID, unAccountInstance, eUniverse, eAccountType );
	}


	//-----------------------------------------------------------------------------
	// Purpose: Constructor
	// Input  : ulSteamID -		64-bit representation of a Steam ID
	//-----------------------------------------------------------------------------
	CSteamID( uint64 ulSteamID )
	{
		SetFromUint64( ulSteamID );
	}


	//-----------------------------------------------------------------------------
	// Purpose: Sets parameters for steam ID
	// Input  : unAccountID -	32-bit account ID
	//			eUniverse -		Universe this account belongs to
	//			eAccountType -	Type of account
	//-----------------------------------------------------------------------------
	void Set( uint32 unAccountID, EUniverse eUniverse, EAccountType eAccountType )
	{
		m_unAccountID = unAccountID;
		m_EUniverse = eUniverse;
		m_EAccountType = eAccountType;
		m_unAccountInstance = 1;
	}


	//-----------------------------------------------------------------------------
	// Purpose: Sets parameters for steam ID
	// Input  : unAccountID -	32-bit account ID
	//			eUniverse -		Universe this account belongs to
	//			eAccountType -	Type of account
	//-----------------------------------------------------------------------------
	void InstancedSet( uint32 unAccountID, uint32 unInstance, EUniverse eUniverse, EAccountType eAccountType )
	{
		m_unAccountID = unAccountID;
		m_EUniverse = eUniverse;
		m_EAccountType = eAccountType;
		m_unAccountInstance = unInstance;
	}


	//-----------------------------------------------------------------------------
	// Purpose: Initializes a steam ID from its 52 bit parts and universe/type
	// Input  : ulIdentifier - 52 bits of goodness
	//-----------------------------------------------------------------------------
	void FullSet( uint64 ulIdentifier, EUniverse eUniverse, EAccountType eAccountType )
	{
		m_unAccountID = ( ulIdentifier & 0xFFFFFFFF );						// account ID is low 32 bits
		m_unAccountInstance = ( ( ulIdentifier >> 32 ) & 0xFFFFF );			// account instance is next 20 bits
		m_EUniverse = eUniverse;
		m_EAccountType = eAccountType;
	}


	//-----------------------------------------------------------------------------
	// Purpose: Initializes a steam ID from its 64-bit representation
	// Input  : ulSteamID -		64-bit representation of a Steam ID
	//-----------------------------------------------------------------------------
	void SetFromUint64( uint64 ulSteamID )
	{
		m_unAccountID = ( ulSteamID & 0xFFFFFFFF );							// account ID is low 32 bits
		m_unAccountInstance = ( ( ulSteamID >> 32 ) & 0xFFFFF );			// account instance is next 20 bits

		m_EAccountType = ( EAccountType ) ( ( ulSteamID >> 52 ) & 0xF );	// type is next 4 bits
		m_EUniverse = ( EUniverse ) ( ( ulSteamID >> 56 ) & 0xFF );			// universe is next 8 bits
	}

	//-----------------------------------------------------------------------------
	// Purpose: Initializes a steam ID from a Steam2 ID structure
	// Input:	pTSteamGlobalUserID -	Steam2 ID to convert
	//			eUniverse -				universe this ID belongs to
	//-----------------------------------------------------------------------------
	void SetFromSteam2( TSteamGlobalUserID *pTSteamGlobalUserID, EUniverse eUniverse )
	{
		m_unAccountID = pTSteamGlobalUserID->m_SteamLocalUserID.Split.Low32bits * 2 + 
		pTSteamGlobalUserID->m_SteamLocalUserID.Split.High32bits;
		m_EUniverse = eUniverse;		// set the universe
		m_EAccountType = k_EAccountTypeIndividual; // Steam 2 accounts always map to account type of individual
		m_unAccountInstance = 1;	// individual accounts always have an account instance ID of 1
	}

	//-----------------------------------------------------------------------------
	// Purpose: Converts steam ID to its 64-bit representation
	// Output : 64-bit representation of a Steam ID
	//-----------------------------------------------------------------------------
	uint64 ConvertToUint64( ) const
	{
		return (uint64) ( ( ( (uint64) m_EUniverse ) << 56 ) + ( ( (uint64) m_EAccountType ) << 52 ) + 
			( ( (uint64) m_unAccountInstance ) << 32 ) + m_unAccountID );
	}

	//-----------------------------------------------------------------------------
	// Purpose: Fills out a Steam2 ID structure
	// Input:	pTSteamGlobalUserID -	Steam2 ID to write to
	//-----------------------------------------------------------------------------
	void ConvertToSteam2( TSteamGlobalUserID *pTSteamGlobalUserID ) const
	{
		// only individual accounts have any meaning in Steam 2, only they can be mapped
		// Assert( m_EAccountType == k_EAccountTypeIndividual );

		pTSteamGlobalUserID->m_SteamInstanceID = 0;
		pTSteamGlobalUserID->m_SteamLocalUserID.Split.High32bits = m_unAccountID % 2;
		pTSteamGlobalUserID->m_SteamLocalUserID.Split.Low32bits = m_unAccountID / 2;
	}


	//-----------------------------------------------------------------------------
	// Purpose: Converts the static parts of a steam ID to a 64-bit representation.
	//			For multiseat accounts, all instances of that account will have the
	//			same static account key, so they can be grouped together by the static
	//			account key.
	// Output : 64-bit static account key
	//-----------------------------------------------------------------------------
	uint64 GetStaticAccountKey( ) const
	{
		// note we do NOT include the account instance (which is a dynamic property) in the static account key
		return (uint64) ( ( ( (uint64) m_EUniverse ) << 56 ) + ((uint64) m_EAccountType << 52 ) + m_unAccountID );
	}


	//-----------------------------------------------------------------------------
	// Purpose: create an anonomous game server login to be filled in by the AM
	//-----------------------------------------------------------------------------
	void CreateBlankAnonLogon( )
	{
		m_unAccountID = 0;
		m_EAccountType = k_EAccountTypeAnonGameServer;
		m_EUniverse = k_EUniversePublic;
		m_unAccountInstance = 0;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this an anonomous game server login that will be filled in?
	//-----------------------------------------------------------------------------
	bool BBlankAnonAccount( )
	{
		return m_unAccountID == 0 && 
			m_EAccountType == k_EAccountTypeAnonGameServer &&
			m_EUniverse == k_EUniversePublic &&
			m_unAccountInstance == 0;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Is this a game server account id?
	//-----------------------------------------------------------------------------
	bool BGameServerAccount( )
	{
		return m_EAccountType == k_EAccountTypeGameServer || m_EAccountType == k_EAccountTypeAnonGameServer;
	}

	// simple accessors
	void SetAccountID( uint32 unAccountID ) { m_unAccountID = unAccountID; }
	uint32 GetAccountID( ) const { return m_unAccountID; }
	uint32 GetUnAccountInstance( ) const { return m_unAccountInstance; }
	EAccountType GetEAccountType( ) const { return m_EAccountType; }
	EUniverse GetEUniverse( ) const { return m_EUniverse; }
	bool IsValid( ) const { return m_EAccountType != k_EAccountTypeInvalid; }

	// this set of functions is hidden, will be moved out of class
	CSteamID( char *pchSteamID );
	void SetFromString( char *pchSteamID );
	char * Render( ) const;				// renders this steam ID to string
	static char * Render( uint64 ulSteamID );	// static method to render a uint64 representation of a steam ID to a string

	// DEBUG function
	bool BValidExternalSteamID( void );

private:
	// 64 bits total
	uint32				m_unAccountID : 32;			// unique account identifier
	unsigned int		m_unAccountInstance : 20;	// dynamic instance ID (used for multiseat type accounts only)
	EAccountType		m_EAccountType : 4;			// type of account
	EUniverse			m_EUniverse : 8;			// universe this account belongs to
};

#pragma pack( pop )

// ICMCallback
// Anyone who owns a CMInterface object should implement this to receive callbacks.
class ICMCallback
{
public:
	ICMCallback( ) { };
	virtual ~ICMCallback( ) { };

	virtual void OnLogonSuccess( void ) = 0 ;
	virtual void OnLogonFailure( EResult eResult ) = 0;
	virtual void OnLoggedOff( EResult eResult ) = 0;

	// Called during logon to get encrypted user ticket
	virtual bool GetEncryptedUserTicket( unsigned int unIPPublic, unsigned int unIPPrivate, 
		const void * pubEncryptionKey, unsigned int cubEncryptionKey,
		void * pubEncryptedTicket, unsigned int cubEncryptedTicketBuf, unsigned int * pcubEncryptedTicketSize ) = 0;

	// Called when we receive a VAC challenge
	virtual void HandleVACChallenge( int nClientGameID, uint8 *pubChallenge, int cubChallenge ) = 0;

	// Called when we receive a VAC kick message
	virtual void HandleVACKick( int nClientGameID ) = 0;

	// Methods to read/write to owner's persistent storage (e.g. config file)
	virtual bool GetValue( const char * pwzName, char * pwzValue, const int cchValue ) = 0;
	virtual bool SetValue( const char * pwzName, const char * pwzValue ) = 0;

	// called when a GS is getting responses to requests or user status
	virtual void GSHandleClientApprove( CSteamID & steamID ) = 0;
	virtual void GSHandleClientDeny( CSteamID & steamID, EDenyReason eDenyReason ) = 0;
	virtual void GSHandleClientKick( CSteamID & steamID, EDenyReason eDenyReason ) = 0;

#ifdef _SERVER
#ifdef TEST_CODE_ENABLED
	virtual void TEST_ReportAuthKey( uint8 * pubAuthKey,	uint32 cubAuthKey ) = 0;
	virtual void TEST_ReportMsgRecieved( int32 m_EMsg ) = 0;
#endif // TEST_CODE_ENABLED
#endif // _SERVER
};


// CMInterface
// This connects the client to the CM.
class ICMInterface
{
public:
	ICMInterface() {};
	virtual ~ICMInterface() {};

	// Manage ClientGames
	virtual int NClientGameIDAdd( int nGameID ) = 0;
	virtual void RemoveClientGame( int nClientGameID )  = 0;
	virtual void SetClientGameServer( int nClientGameID, uint unIPServer, uint16 usPortServer ) = 0;

	// Set CM IP and port -- currently only for debugging!
	virtual void SetCMs( const netadr_t *pNetAdrCMs, int nNetAdrCMs ) = 0;

	// Log on and off
	virtual void LogOff( void ) = 0;
	virtual void LogOn( CSteamID & steamID ) = 0;

	// Our mainloop
	virtual bool BMainLoop( ) = 0;

	// Send a VAC response to the CM
	virtual bool SendVACResponse( int nClientGameID, uint8 *pubResponse, int cubResponse ) = 0;

	// Send an authentication packet to the CM
	virtual bool SendAuth( uint8 *pubAuth, int cubAuth ) = 0;

	virtual bool BLoggedOn( void ) = 0;

	virtual void SetSpewLevel( int nSpewLevel ) = 0;

	virtual EUniverse GetEUniverse( ) = 0;
	virtual void SetEUniverse( EUniverse eUniverse ) = 0;

	// Game Server methods
	virtual bool GSSendLogonRequest( CSteamID & steamID ) = 0;
	virtual bool GSSendDisconnect( CSteamID & steamID ) = 0;
	virtual bool GSSendStatusResponse( CSteamID & steamID, int nSecondsConnected, int nSecondsSinceLast ) = 0;
	virtual bool GSSetStatus( int32 uAppIdServed, uint uServerFlags, int cPlayers, int cPlayersMax ) = 0;

#ifdef TEST_CODE_ENABLED
	virtual void TEST_SetFakePrivateIP( uint unIPPrivate ) = 0;
	virtual void TEST_SendBigMessage( void ) = 0;					// send a big (multi-packet) message to the server to test packetization code
	virtual bool TEST_BBigMessageResponseReceived( void ) = 0;		// returns TRUE if a big test message response has been sent back from the server
	virtual void TEST_SetPktLossPct( int nPct ) = 0;				// sets a simulated packet loss percentage for testing
	virtual void TEST_SetForceTCP( bool bForceTCP ) = 0;			// forces client to use TCP connection
	virtual void TEST_SetTCPFallback( bool bTCPFallback ) = 0;		// causes client to connect over TCP, send a fallback message, and disconnect
	virtual int	TEST_GetOurConnectionID( ) = 0;						// returns our connection ID for this connection
	virtual void TEST_HeartBeat( ) = 0;								// send a heartbeat to the CM 
	virtual IVConnCallback * TEST_GetVConnCallback( ) =0;			// returns our vconn callback interface
	virtual void TEST_FakeDisconnect() = 0;
#endif // TEST_CODE_ENABLED

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, char *pchName ) = 0;		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

};

#ifdef VAC_ENABLED
// IVAC
// This is the wrapper class for all VAC functionaility in the client
class IVAC
{
public:

	virtual bool BVACCreateProcess(  
		PVOID lpVACBlob,
		DWORD  cbBlobSize,
		LPCTSTR lpApplicationName,
		LPTSTR lpCommandLine,
		LPSECURITY_ATTRIBUTES lpProcessAttributes,
		LPSECURITY_ATTRIBUTES lpThreadAttributes,
		BOOL bInheritHandles,
		DWORD dwCreationFlags,
		LPVOID lpEnvironment,
		LPCTSTR lpCurrentDirectory,
		LPSTARTUPINFO lpStartupInfo,
		LPPROCESS_INFORMATION lpProcessInformation,
		DWORD nGameID
		) = 0;

	virtual void KillAllVAC( void ) = 0;
	virtual void ProcessVAC( ICMInterface *pCMInterface ) = 0;

	virtual void RealHandleVACChallenge( ICMInterface *pCMInterface, int nClientGameID, uint8 *pubChallenge, int cubChallenge) = 0;
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, char *pchName ) = 0;		// Validate our internal structures
#endif
};

// this is a bogus number picked to be beyond any real steam2 uAppID
const int k_nGameIDNotepad = 65535;
// this is the real steam2 uAppID for Counter-Strike Source
const int k_nGameIDCSS = 240;

IVAC * IVACGet( void );
void ReleaseIVAC( IVAC *pIVAC );
BYTE *PbLoadVacBlob( int *pcbVacBlob );
void FreeVacBlob( BYTE *pbVacBlob );
#ifdef _DEBUG
void SetUseDllForIVAC( void );
#endif // _DEBUG

#endif // VAC_ENABLED

//lint -restore

#endif // STEAMCLIENTPUBLIC_H
