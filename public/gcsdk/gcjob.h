//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef GCJOB_H
#define GCJOB_H
#ifdef _WIN32
#pragma once
#endif

#include "gcwebapikey.h"

namespace GCSDK
{


//-----------------------------------------------------------------------------
// Purpose: handles a network message job from the client
//-----------------------------------------------------------------------------
class CGCJob : public CJob
{
public:
	CGCJob( CGCBase *pGC ) : CJob( pGC->GetJobMgr() ), m_pGC( pGC ), m_cHeartbeatsBeforeTimeout( k_cJobHeartbeatsBeforeTimeoutDefault ) {}

	// all GC jobs must implement one of these
	virtual bool BYieldingRunGCJob( IMsgNetPacket *pNetPacket )	{ return false; }
	virtual bool BYieldingRunGCJob()							{ return false; }

	virtual EServerType GetServerType() { return k_EServerTypeGC; }

	bool BYldSendMessageAndGetReply( CSteamID &steamIDTarget, CGCMsgBase &msgOut, uint nTimeoutSec, CGCMsgBase *pMsgIn, MsgType_t eMsg );
	bool BYldSendMessageAndGetReply( CSteamID &steamIDTarget, CGCMsgBase &msgOut, uint nTimeoutSec, IMsgNetPacket **ppNetPacket );
	bool BYldSendMessageAndGetReply( CSteamID &steamIDTarget, IProtoBufMsg &msgOut, uint nTimeoutSec, IProtoBufMsg *pMsgIn, MsgType_t eMsg );
	bool BYldSendMessageAndGetReply( CSteamID &steamIDTarget, IProtoBufMsg &msgOut, uint nTimeoutSec, IMsgNetPacket **ppNetPacket );

	virtual uint32 CHeartbeatsBeforeTimeout() { return m_cHeartbeatsBeforeTimeout; }
	void SetJobTimeout( uint nTimeoutSec ) {  m_cHeartbeatsBeforeTimeout = 1 + ((nTimeoutSec * k_nMillion) / k_cMicroSecJobHeartbeat); }

protected:
	CGCBase *m_pGC;

private:
	virtual bool BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
	{
		return BYieldingRunGCJob( pNetPacket );
	}

	virtual bool BYieldingRunJob( void *pvStartParam )
	{
		return BYieldingRunGCJob();
	}

	uint32 m_cHeartbeatsBeforeTimeout;
};

//-----------------------------------------------------------------------------
// CGCWGJob - A job invoked from forwarded WG messages
//-----------------------------------------------------------------------------
class CGCWGJob : public CGCJob
{
public:
	CGCWGJob( CGCBase *pGCBase );
	~CGCWGJob();
	bool BYieldingRunGCJob(  IMsgNetPacket * pNetPacket ); // invokes BYieldingRunJobFromRequest
	virtual bool BYieldingRunJobFromRequest( KeyValues *pkvRequest, KeyValues *pkvResponse ) = 0;
	virtual bool BVerifyParams( const CGCMsg<MsgGCWGRequest_t> & msg, KeyValues *pkvRequest, const WebApiFunc_t * pWebApiFunc );
	void SetWebApiFunc( const WebApiFunc_t * pWebApiFunc ) { m_pWebApiFunc = pWebApiFunc; }

	void SetErrorMessage( KeyValues *pkvErr, const char *pchErrorMsg, int32 nResult );

protected:
	KeyValues *m_pkvResponse;
	CUtlBuffer m_bufRequest; // packed binary form of the request
	const WebApiFunc_t * m_pWebApiFunc;

	// requester auth info, like WGRequestContext
	CSteamID m_steamID;
};


//-----------------------------------------------------------------------------
// CGCJobVerifySession - A job that asks steam if a given user is still connected
//  and cleans up the session if the user is gone
//-----------------------------------------------------------------------------
class CGCJobVerifySession : public CGCJob
{
public:
	CGCJobVerifySession( CGCBase *pGC, const CSteamID &steamID ) : CGCJob( pGC ), m_steamID( steamID ) { }
	virtual bool BYieldingRunGCJob();

private:
	CSteamID m_steamID;
};



//-----------------------------------------------------------------------------
// CWebAPIJob - A job invoked from a forwarded WebAPI request
//-----------------------------------------------------------------------------
class CWebAPIJob : public CGCJob
{
public:
	CWebAPIJob( CGCBase *pGC, EWebAPIOutputFormat eDefaultOutputFormat = k_EWebAPIOutputFormat_JSON );
	~CWebAPIJob();

	// Called by jobmgr, and then invokes appropriate run function for specific API version requested
	bool BYieldingRunJobFromMsg( IMsgNetPacket * pNetPacket ); 

	// Implemented by each individual WebAPI job, should be declared using BEGIN_WEBAPI_JOB_VERSION_ADAPTERS and related macros.
	virtual bool BYieldingRunJobFromAPIRequest( const char *pchInterface, const char *pchMethod, uint32 unVersion, CHTTPRequest *pRequest, CHTTPResponse *pResponse, CWebAPIResponse *pWebAPIResponse  ) = 0;

protected:
	static void AddLocalizedString( CWebAPIValues *pOutDefn, const char *pchFieldName, const char *pchKeyName, ELanguage eLang, bool bReturnTokenIfNotFound = true );

	CHTTPRequest *m_pRequest;
	CHTTPResponse *m_pResponse;
	CWebAPIKey m_webAPIKey;
	EWebAPIOutputFormat m_eDefaultOutputFormat; 
};


#define BEGIN_WEBAPI_JOB_VERSION_ADAPTERS( interface, method ) \
	bool BYieldingRunJobFromAPIRequest( const char *pchInterface, const char *pchMethod, uint32 unVersion, CHTTPRequest *pRequest, CHTTPResponse *pResponse, CWebAPIResponse *pWebAPIResponse  ) \
{ \
	if ( Q_strnicmp( pchInterface, interface, Q_strlen( interface ) ) != 0 ) \
{ \
	AssertMsg2( false, "WebAPIJob recieved request for unexpected interface (got %s, expected %s)!", pchInterface, interface ); \
	pResponse->SetStatusCode( k_EHTTPStatusCode500InternalServerError ); \
	return false; \
} \
	\
	if ( Q_stricmp( pchMethod, method ) != 0 ) \
{ \
	AssertMsg2( false, "WebAPIJob received request fo unexpected method (got %s, expected %s)!", pchMethod, method ); \
	pResponse->SetStatusCode( k_EHTTPStatusCode500InternalServerError ); \
	return false; \
} \
	\
	\
	bool bFoundVersion = false; \
	bool bResult = false; \
	switch( unVersion ) \
{ 

#define WEBAPI_JOB_VERSION_ADAPTER( version, funcname ) \
	case version: \
	bFoundVersion = true; \
	{ \
	VPROF_BUDGET( #funcname, VPROF_BUDGETGROUP_JOBS_COROUTINES ); \
	bResult = this->##funcname( pRequest, pWebAPIResponse ); \
	} \
	break; 

#define WEBAPI_JOB_VERSION_ADAPTER_EXTENDED_ARRAYS( version, funcname ) \
	case version: \
	bFoundVersion = true; \
	pWebAPIResponse->SetExtendedArrays( true ); \
	{ \
	VPROF_BUDGET( #funcname, VPROF_BUDGETGROUP_JOBS_COROUTINES ); \
	bResult = this->##funcname( pRequest, pWebAPIResponse ); \
	} \
	break; 

#define END_WEBAPI_JOB_VERSION_ADAPTERS() \
	default: \
	break; \
} \
	AssertMsg3( bFoundVersion, "WebAPIJob for %s/%s received unhandled version %d", pchInterface, pchMethod, unVersion ); \
	return bResult; \
}




} // namespace GCSDK


#endif // GCJOB_H
