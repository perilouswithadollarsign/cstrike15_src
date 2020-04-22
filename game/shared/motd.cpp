//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: motd: Handles a list of message of the day entries
//
//=============================================================================

#include "cbase.h"
#include "motd.h"
#include "schemainitutils.h"
#include "rtime.h"

using namespace GCSDK;

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CMOTDEntryDefinition::CMOTDEntryDefinition( void )
{
	m_pKVMOTD = NULL;
	m_PostTime = 0;
	m_ChangedTime = 0;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CMOTDEntryDefinition::BInitFromKV( KeyValues *pKVMOTD, CUtlVector<CUtlString> *pVecErrors )
{
	m_pKVMOTD = pKVMOTD->MakeCopy();

	const char *pszTime = m_pKVMOTD->GetString( "post_time", NULL );
	m_PostTime = (pszTime && pszTime[0]) ? CRTime::RTime32FromString(pszTime) : 0;

	pszTime = m_pKVMOTD->GetString( "last_changed_time", NULL );
	m_ChangedTime = (pszTime && pszTime[0]) ? CRTime::RTime32FromString(pszTime) : 0;

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
const char *CMOTDEntryDefinition::GetTitle( ELanguage eLang ) 
{ 
	if ( m_pKVMOTD )
	{
		// See if we have a localised block for the specified language.
		const char *pszLanguage = GetLanguageShortName( eLang );
		if ( pszLanguage && pszLanguage[0] )
		{
			const char *pszText = m_pKVMOTD->GetString( CFmtStr( "title_%s", pszLanguage ), NULL );
			if ( pszText && pszText[0] )
				return pszText;
		}

		// Fall back to english
		return m_pKVMOTD->GetString( "title_english", "No Title" );
	}

	return "No Title";
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
const char *CMOTDEntryDefinition::GetText( ELanguage eLang ) 
{ 
	if ( m_pKVMOTD )
	{
		// See if we have a localised block for the specified language.
		const char *pszLanguage = GetLanguageShortName( eLang );
		if ( pszLanguage && pszLanguage[0] )
		{
			const char *pszText = m_pKVMOTD->GetString( CFmtStr( "text_%s", pszLanguage ), NULL );
			if ( pszText && pszText[0] )
				return pszText;
		}

		// Fall back to english
		return m_pKVMOTD->GetString( "text_english", "No text" );
	}

	return "No text";
}

// Sorts the MOTD entries in order of the time they last changed
int	MOTDEntriesListLess( const CMOTDEntryDefinition *pLhs, const CMOTDEntryDefinition *pRhs )
{
#ifdef TF_GC_DLL
	// The GC sorts by changetime
	return ( pLhs->GetChangedTime() > pRhs->GetChangedTime() );
#else
	// The client sorts by post time
	return ( pLhs->GetPostTime() > pRhs->GetPostTime() );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the loot lists section of the schema
//-----------------------------------------------------------------------------
bool CMOTDManager::BInitMOTDEntries( KeyValues *pKVMOTDEntries, CUtlVector<CUtlString> *pVecErrors )
{
	m_vecMOTDEntries.RemoveAll();

	RTime32 iPrevTime = 0;

	if ( NULL != pKVMOTDEntries )
	{
		FOR_EACH_TRUE_SUBKEY( pKVMOTDEntries, pKVEntry )
		{
			const char *listName = pKVEntry->GetName();

			SCHEMA_INIT_CHECK( listName != NULL, CFmtStr( "All MOTD entries must have titles.") );

			int idx = m_vecMOTDEntries.AddToTail();
			SCHEMA_INIT_SUBSTEP( m_vecMOTDEntries[idx].BInitFromKV( pKVEntry, pVecErrors ) );

			// Make sure the dates all move forward
			SCHEMA_INIT_CHECK( m_vecMOTDEntries[idx].GetPostTime() > iPrevTime , CFmtStr( "MOTD entry '%s' occurs prior to the previous entry.", m_vecMOTDEntries[idx].GetName() ) );
			iPrevTime = m_vecMOTDEntries[idx].GetPostTime();
		}
	}

	// Then sort all the MOTDs in order of their changed times, so we can easily send them
	m_vecMOTDEntries.Sort( MOTDEntriesListLess );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Returns the number of MOTD entries we've got after the specified time
//-----------------------------------------------------------------------------
int CMOTDManager::GetNumMOTDAfter( RTime32 iTime )
{
	FOR_EACH_VEC( m_vecMOTDEntries, i )
	{
		if ( m_vecMOTDEntries[i].GetChangedTime() > iTime )
		{
			// We've hit the first MOTD entry after this time. All following posts are assumed after.
			return (m_vecMOTDEntries.Count() - i);
		}
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:	Returns the definition for the next blog post after the specified time
//-----------------------------------------------------------------------------
CMOTDEntryDefinition *CMOTDManager::GetNextMOTDAfter( RTime32 iTime )
{
	FOR_EACH_VEC( m_vecMOTDEntries, i )
	{
		if ( m_vecMOTDEntries[i].GetChangedTime() > iTime )
			return &m_vecMOTDEntries[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
CMOTDEntryDefinition *CMOTDManager::GetMOTDByIndex( int iIndex )
{
	if ( iIndex < 0 || iIndex > m_vecMOTDEntries.Count() )
		return NULL;
	return &m_vecMOTDEntries[iIndex];
}


#ifdef TF_GC_DLL

//-----------------------------------------------------------------------------
// Handle MOTD requests job.
//-----------------------------------------------------------------------------
class CGCMOTDRequest : public CGCGameBaseJob
{
public:
	CGCMOTDRequest( CGCGameBase *pGC ) : CGCGameBaseJob( pGC ) { }
	bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket );
};


//-----------------------------------------------------------------------------
// Purpose: Responds to requests from the client for the current MOTD list
//-----------------------------------------------------------------------------
bool CGCMOTDRequest::BYieldingRunJobFromMsg( IMsgNetPacket *pNetPacket )
{
	CGCMsg< MsgGCMOTDRequest_t > msg( pNetPacket );
	ELanguage eLang = (ELanguage)msg.Body().m_eLanguage;
	RTime32 iMOTDTime = msg.Body().m_nLastMOTDRequest;

	// Send the response to the client
	GCSDK::CGCMsg<MsgGCMOTDRequestResponse_t> msg_response( k_EMsgGCMOTDRequestResponse );

	int iEntries = 0;
	CMOTDEntryDefinition *pMOTD = m_pGCGameBase->GetMOTDManager().GetNextMOTDAfter( iMOTDTime );
	while ( pMOTD )
	{
		// Stuff this MOTD into the message.
		msg_response.AddStrData( pMOTD->GetName() );
		msg_response.AddUintData( pMOTD->GetPostTime() );
		msg_response.AddStrData( pMOTD->GetTitle( eLang ) );
		msg_response.AddStrData( pMOTD->GetText( eLang ) );
		msg_response.AddStrData( pMOTD->GetURL() );
		iEntries++;

		// Move on to the next message.
		iMOTDTime = pMOTD->GetChangedTime();
		pMOTD = m_pGCGameBase->GetMOTDManager().GetNextMOTDAfter( iMOTDTime );
	} 
	msg_response.Body().m_nEntries = iEntries;

	GGCTF()->BSendGCMsgToClient( msg.Hdr().m_ulSteamID, msg_response );

	return true;
}

GC_REG_JOB( CGCGameBase, CGCMOTDRequest, "CGCMOTDRequest", k_EMsgGCMOTDRequest, k_EServerTypeGC );

#endif // TF_GC_DLL
