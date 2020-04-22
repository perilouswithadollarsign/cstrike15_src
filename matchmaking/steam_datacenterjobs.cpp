//========= Copyright © Valve Corporation, All rights reserved. =======================//
//
// Purpose: Jobs for communicating with the custom Steam backend (Game Coordinator)
//
//=====================================================================================//

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR ) && !defined( SWDS )

#include "mm_framework.h"
#include "steam_datacenterjobs.h"

#include "generic_gcmessages.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

using GCSDK::CGCMsg;

//-----------------------------------------------------------------------------
// Purpose: Constructor
// Inputs:	pKVStats - A pointer to a KV with the global stats to update
//-----------------------------------------------------------------------------
CGCClientJobUpdateStats::CGCClientJobUpdateStats( KeyValues *pKVStats )
	: CGCClientJob( GGCClient() ), m_pKVCmd( NULL )
{
	m_pKVCmd = pKVStats->MakeCopy();
	m_pKVCmd->SetName( "stat_agg" );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCClientJobUpdateStats::~CGCClientJobUpdateStats( )
{
	if ( m_pKVCmd )
		m_pKVCmd->deleteThis();
}


//-----------------------------------------------------------------------------
// Purpose: Runs the job
//-----------------------------------------------------------------------------
bool CGCClientJobUpdateStats::BYieldingRunGCJob()
{
	CGCMsg<MsgGCGenericKV_t> msg( k_EMsgGCKVCommand );
	CUtlBuffer bufDest;

	m_pKVCmd->WriteAsBinary( bufDest );
	msg.AddVariableLenData( bufDest.Base(), bufDest.TellPut() );
	m_pGCClient->BSendMessage( msg );	

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
// Inputs:	pKVStats - A pointer to a KV with the global stats to update
//-----------------------------------------------------------------------------
CGCClientJobDataRequest::CGCClientJobDataRequest( )
: CGCClientJob( GGCClient() ), 
  m_pKVRequest( NULL ),
  m_pKVResults( NULL ),
  m_bComplete( false ),
  m_bSuccess( false ),
  m_bWaitForRead( true )
{
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGCClientJobDataRequest::~CGCClientJobDataRequest( )
{
	if ( m_pKVRequest )
		m_pKVRequest->deleteThis();
	if ( m_pKVResults )
		m_pKVResults->deleteThis();
}


//-----------------------------------------------------------------------------
// Purpose: Runs the job
//-----------------------------------------------------------------------------
bool CGCClientJobDataRequest::BYieldingRunGCJob()
{
	CGCMsg<MsgGCGenericKV_t> msgOut( k_EMsgGCKVCommand );
	CGCMsg<MsgGCGenericKVResponse_t> msgIn;
	CUtlBuffer bufOut;

	m_pKVRequest = new KeyValues( "datarequest" );
	m_pKVRequest->WriteAsBinary( bufOut );
	msgOut.AddVariableLenData( bufOut.Base(), bufOut.TellPut() );
	
	m_bSuccess = BYldSendMessageAndGetReply( msgOut, 30, &msgIn, k_EMsgGCKVCommandResponse )
				 && msgIn.Body().m_bSuccess;

	if ( m_bSuccess )
	{
		CUtlBuffer bufIn( msgIn.PubVarData(), msgIn.CubVarData(), CUtlBuffer::READ_ONLY );
		m_pKVResults = new KeyValues( "results" );
		m_bSuccess &= m_pKVResults->ReadAsBinary( bufIn );
	}

	m_bComplete = true;

	// Once we're complete give the datacenter code a reasonable chance
	// to get the data back before we finish and delete ourselves.
	int nFramesToWait = 5;
	while ( m_bWaitForRead && nFramesToWait-- > 0 )
	{
		BYieldingWaitOneFrame();
	}

	return true;
}

#endif
