//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Holds the CAccountDetails class.
//
//=============================================================================

#ifndef ACCOUNTDETAILS_H
#define ACCOUNTDETAILS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/thash.h"
#include "tier1/utlhashmaplarge.h"

namespace GCSDK
{

class CAccountDetails
{
public:
	CAccountDetails();

	void Init( KeyValues *pkvDetails );
	void Reset();
	bool BIsExpired() const;
	bool BIsValid() const { return m_bValid; }
	void SetCacheTime( int cacheTime );

	const char *GetAccountName() const { return m_sAccountName.Get(); }
	const char *GetPersonaName() const { return m_sPersonaName.Get(); }
	bool BHasPublicProfile() const { return m_bPublicProfile; }
	bool BHasPublicInventory() const { return m_bPublicInventory; }
	bool BIsVacBanned() const { return m_bVacBanned; }
	bool BIsCyberCafe() const { return m_bCyberCafe; }
	bool BIsSchoolAccount() const { return m_bSchoolAccount; }
	bool BIsFreeTrialAccount() const { return m_bFreeTrialAccount; }
	bool BIsFreeTrialAccountOrDemo() const;
	bool BIsSubscribed() const { return m_bSubscribed; }
	bool BIsLowViolence() const { return m_bLowViolence; }
	bool BIsLimitedAccount() const { return m_bLimited; }
	bool BIsTrustedAccount() const { return m_bTrusted; }
	uint32  GetPackage() const { return m_unPackage; }
	RTime32 GetCacheTime() const { return m_rtimeCached; }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

private:
	RTime32 m_rtimeCached;
	CUtlConstString m_sAccountName;
	CUtlConstString m_sPersonaName;
	uint32 m_unPackage;
	bool 
		m_bValid:1,
		m_bPublicProfile:1,
		m_bPublicInventory:1,
		m_bVacBanned:1,
		m_bCyberCafe:1,
		m_bSchoolAccount:1,
		m_bFreeTrialAccount:1,
		m_bSubscribed:1,
		m_bLowViolence:1,
		m_bLimited:1,
		m_bTrusted:1;
};


//-----------------------------------------------------------------------------
// Purpose: Manages requests for CAccountDetails objects
//-----------------------------------------------------------------------------
class CAccountDetailsManager
{
public:
	CAccountDetailsManager();
	CAccountDetails *YieldingGetAccountDetails( const CSteamID &steamID );
	
	void MarkFrame() { m_hashAccountDetailsCache.StartFrameSchedule( true ); }
	bool BExpireAccountDetails( CLimitTimer &limitTimer );

	void Dump() const;

private:
	friend class CGCJobSendGetAccountDetailsRequest;

	bool BFindInLocalCache( const CSteamID &steamID, CAccountDetails **ppAccount );
	void WakeWaitingJobs( const CSteamID &steamID );

	CTHash<CAccountDetails, uint32> m_hashAccountDetailsCache;
	CUtlHashMapLarge<CSteamID, CCopyableUtlVector<JobID_t> > m_mapQueuedRequests;
};


} // namespace GCSDK
#endif // ACCOUNTDETAILS_H
