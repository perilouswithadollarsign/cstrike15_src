//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// 
//
//===============================================================================

#ifndef SOS_ENTRY_MATCH_SYSTEM_H
#define SOS_ENTRY_MATCH_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "bitvec.h"
//#include "snd_channels.h"

// Externs the logging channel
DECLARE_LOGGING_CHANNEL( LOG_SND_OPERATORS );

extern Color StackColor;

//-----------------------------------------------------------------------------
//
// Match Entry system
//
//-----------------------------------------------------------------------------

class CSosEntryMatch
{

public:
	CSosEntryMatch()
	{
		Reset();
	}

	void Reset()
	{
		m_nMatchString1[0] = '\n';
		m_bMatchString1 = false;
		m_nMatchString2[0] = '\n';
		m_bMatchString2 = false;
		m_bMatchSubString = false;
		m_nMatchInt1 = -1;
		m_bMatchInt1 = false;
		m_nMatchInt2 = -1;
		m_bMatchInt2 = false;
		m_nMatchInt3 = -1;
		m_bMatchInt3 = false;
	}

	bool IsAMatch( CSosEntryMatch *pEntryMatch );

public:
	char	m_nMatchString1[64];
	bool	m_bMatchString1;
	char	m_nMatchString2[64];
	bool	m_bMatchString2;
	bool	m_bMatchSubString;
	int		m_nMatchInt1;
	bool	m_bMatchInt1;
	int		m_nMatchInt2;
	bool	m_bMatchInt2;
	int		m_nMatchInt3;
	bool	m_bMatchInt3;

};

class CSosEntryMatchList;
class CSosManagedEntryMatch : public CSosEntryMatch
{
	friend class CSosEntryMatchList;

	void Reset()
	{
		CSosEntryMatch::Reset();
		m_bActive = false;
		m_bTimed = false;
		m_flStartTime = -1.0;
		m_flDuration = -1.0;
		m_bFree = true;
	}

public:
	void Start()
	{
		m_flStartTime = g_pSoundServices->GetClientTime();
	}
private:
	void Print()
	{
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match String 1: %s\n", m_nMatchString1 );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match String 1?: %s\n", m_bMatchString1 ? "true" : "false" );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match String 2: %s\n", m_nMatchString2 );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match String 2?: %s\n", m_bMatchString2 ? "true" : "false" );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match SubString ?: %s\n", m_bMatchSubString ? "true" : "false" );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match Int 1: %i\n", m_nMatchInt1 );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match Int 1?: %s\n", m_bMatchInt1 ? "true" : "false" );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match Int 2: %i\n", m_nMatchInt2 );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match Int 2?: %s\n", m_bMatchInt2 ? "true" : "false" );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match Int 3: %i\n", m_nMatchInt3 );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Match Int 3?: %s\n", m_bMatchInt3 ? "true" : "false" );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Timed ?: %s\n", m_bTimed ? "true" : "false" );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Duration: %f\n", m_flDuration );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "StartTime: %f\n", m_flStartTime );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "StopTime: %f\n", m_flStartTime + m_flDuration );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "CurTime: %f\n", g_pSoundServices->GetClientTime() );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Active ?: %s\n", m_bActive ? "true" : "false" );
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "Free ?: %s\n", m_bFree ? "true" : "false" );
	}

	bool	m_bFree;
public:
	bool	m_bActive;
	bool	m_bTimed;
	float	m_flStartTime;
	float	m_flDuration;

};



#define SOS_BLOCKSYS_MAX_ENTRIES 32

class CSosEntryMatchList
{

public:
	~CSosEntryMatchList()
	{
		for( int i = 0; i < SOS_BLOCKSYS_MAX_ENTRIES; i++ )
		{
			delete m_vEntryMatchList[i];
			m_vEntryMatchList[i] = NULL;
		}
	}
	CSosEntryMatchList()
	{
		uint32 bit = 1;
		for( int i = 0; i < SOS_BLOCKSYS_MAX_ENTRIES; i++, ( bit <<= 1 ) )
		{
			m_vEntryMatchList[i] = new CSosManagedEntryMatch;
			m_vEntryMatchList[i]->Reset();
			m_Free |= bit;
		}
	}
	void Update();
	inline void Start( int nIndex ) { m_vEntryMatchList[ nIndex ]->Start();	}
	inline bool IsValidIndex( int nIndex ) const { return nIndex > -1 && nIndex < SOS_BLOCKSYS_MAX_ENTRIES; }
	int GetFreeEntryIndex() const;
	CSosManagedEntryMatch *GetEntryFromIndex( int nIndex ) const ;
	CSosManagedEntryMatch *GetFreeEntry( int &nIndex ) const ;

	void FreeEntry( int nIndex, bool bForce = false );

	bool HasAMatch( CSosEntryMatch *pEntryMatch ) const;
	void Print() const;


protected:

	mutable uint32 m_Free;
	CSosManagedEntryMatch *m_vEntryMatchList[ SOS_BLOCKSYS_MAX_ENTRIES ];

};




#endif // SOS_ENTRY_MATCH_SYSTEM_H
