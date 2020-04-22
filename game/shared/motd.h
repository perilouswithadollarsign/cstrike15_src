//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: motd: Handles a list of message of the day entries
//
//=============================================================================

#ifndef MOTD_H
#define MOTD_H
#ifdef _WIN32
#pragma once
#endif

#include "keyvalues.h"
#include "language.h"

//-----------------------------------------------------------------------------
// CMOTDEntryDefinition
//-----------------------------------------------------------------------------
class CMOTDEntryDefinition
{
public:
	CMOTDEntryDefinition( void );
	~CMOTDEntryDefinition( void ) { }

	bool	BInitFromKV( KeyValues *pKVMOTD, CUtlVector<CUtlString> *pVecErrors = NULL );

	const char	*GetName( void ) { return m_pKVMOTD ? m_pKVMOTD->GetName() : "Unknown Entry"; }
	const char	*GetTitle( ELanguage eLang );
	const char	*GetText( ELanguage eLang );
	const char	*GetURL( void ) { return m_pKVMOTD ? m_pKVMOTD->GetString("url","") : NULL; }
	const char	*GetPostTimeStr( void ) { return m_pKVMOTD ? m_pKVMOTD->GetString("time") : NULL; }

	// Post time is the time displayed on the client screen for this post
	const RTime32		GetPostTime( void ) const { return m_PostTime; }

	// Change time is the time at which we last changed this post. If we change wording on
	// a post, we need to know that the change should be sent to clients when they log on afterwards.
	const RTime32		GetChangedTime( void ) const { return m_ChangedTime; }

private:
	KeyValues	*m_pKVMOTD;
	RTime32		m_PostTime;
	RTime32		m_ChangedTime;
};



//-----------------------------------------------------------------------------
// CMOTDMgr
//-----------------------------------------------------------------------------
class CMOTDManager
{
public:
	// MOTD handling
	bool BInitMOTDEntries( KeyValues *pKVMOTDEntries, CUtlVector<CUtlString> *pVecErrors );
	int  GetNumMOTDAfter( RTime32 iTime );
	CMOTDEntryDefinition *GetNextMOTDAfter( RTime32 iTime );
	int  GetNumMOTDs( void ) { return m_vecMOTDEntries.Count(); }
	CMOTDEntryDefinition *GetMOTDByIndex( int iIndex );

private:
	// Contains the list of MOTD entries.
	CUtlVector< CMOTDEntryDefinition >	m_vecMOTDEntries;

};


#endif // MOTD_H