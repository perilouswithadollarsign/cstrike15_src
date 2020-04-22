//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Holds the CGCSession class
//
//=============================================================================

#ifndef GCSESSION_H
#define GCSESSION_H
#ifdef _WIN32
#pragma once
#endif

namespace GCSDK
{

class CGCGSSession;

//-----------------------------------------------------------------------------
// Purpose: Base class for sessions in the GC
//-----------------------------------------------------------------------------
class CGCSession
{
public:
	CGCSession( const CSteamID & steamID, CGCSharedObjectCache *pCache );
	virtual ~CGCSession();

	const CSteamID & GetSteamID() const { return m_steamID; }

	const CGCSharedObjectCache *GetSOCache() const { return m_pSOCache; }
	CGCSharedObjectCache *GetSOCache() { return m_pSOCache; }
	void RemoveSOCache() { m_pSOCache = NULL; }

	EOSType GetOSType() const;
	bool IsTestSession() const;
	uint32 GetIPPublic() const;

	bool BIsShuttingDown() const { return m_bIsShuttingDown; }
	void SetIsShuttingDown( bool bIsShuttingDown ) { m_bIsShuttingDown = bIsShuttingDown; }

	virtual void Dump( bool bFull = true ) const = 0;

	virtual void MarkAccess() { }
	virtual void Run();
	virtual void YieldingSOCacheReloaded() {}
#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif // DBGFLAG_VALIDATE

private:
	CSteamID m_steamID;
	CGCSharedObjectCache *m_pSOCache;
	uint32 m_unIPPublic; 
	EOSType m_osType : 16;
	bool m_bIsShuttingDown : 1;
	bool m_bIsTestSession : 1;

	friend class CGCBase;
};


//-----------------------------------------------------------------------------
// Purpose: Base class for user sessions in the GC
//-----------------------------------------------------------------------------
class CGCUserSession : public CGCSession
{
public:
	CGCUserSession( const CSteamID & steamID, CGCSharedObjectCache *pCache ) : CGCSession( steamID, pCache ) { }
	virtual ~CGCUserSession();

	virtual bool BInit();

	const CSteamID &GetSteamIDGS() const { return m_steamIDGS; }
	const CSteamID &GetSteamIDGSPrev() const { return m_steamIDGSPrev; }

	virtual bool BSetServer( const CSteamID &steamIDGS );
	virtual bool BLeaveServer();
	virtual void Dump( bool bFull = true ) const;

private:
	CSteamID m_steamIDGS;
	CSteamID m_steamIDGSPrev;
};


//-----------------------------------------------------------------------------
// Purpose: Base class for gameserver sessions in the GC
//-----------------------------------------------------------------------------
class CGCGSSession : public CGCSession
{
public:
	CGCGSSession( const CSteamID & steamID, CGCSharedObjectCache *pCache, uint32 unServerAddr, uint16 usServerPort ) ;
	virtual ~CGCGSSession();

	uint32 GetAddr() const { return m_unServerAddr; }
	uint16 GetPort() const { return m_usServerPort; }
	void SetIPAndPort( uint32 unServerAddr, uint16 usServerPort ) { m_unServerAddr = unServerAddr; m_usServerPort = usServerPort; }

	int GetUserCount() const { return m_vecUsers.Count(); }
	const CSteamID &GetUserID( int nIndex ) const { return m_vecUsers[nIndex]; }
	
	// Manages users on the server. It is very important that these are not
	// virtual and not yielding. For custom behavior override the Pre*() hooks below
	bool BAddUser( const CSteamID &steamIDUser );
	bool BRemoveUser( const CSteamID &steamIDUser );
	void RemoveAllUsers();

	virtual void Dump( bool bFull = true ) const;

protected:
	// Hooks to trigger custom behavior when users are added and removed. It is
	// very important that these do not yield. If you need to yield, start a job instead
	virtual void PreAddUser( const CSteamID &steamIDUser ) {}
	virtual void PostAddUser( const CSteamID &steamIDUser ) {}
	virtual void PreRemoveUser( const CSteamID &steamIDUser ) {}
	virtual void PostRemoveUser( const CSteamID &steamIDUser ) {}
	virtual void PreRemoveAllUsers() {}
	virtual void PostRemoveAllUsers() {}

public:
	// These are the addresses the server itself told us
	uint32 m_unServerPublicIPAddr;
	uint32 m_unServerPrivateIPAddr;
	uint32 m_unServerPort;
	uint16 m_unServerTVPort;
	CUtlString m_serverKey;
	bool m_bServerHibernating;
	int m_serverType;
	int m_region;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif // DBGFLAG_VALIDATE
protected:
	CUtlVector<CSteamID> m_vecUsers;

	// These are the address of the server as connected to Steam
	uint32 m_unServerAddr;
	uint16 m_usServerPort;
};

} // namespace GCSDK

#endif // GCSESSION_H
