//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//
#ifndef TRIGGER_TRACTORBEAM_SHARED_H
#define TRIGGER_TRACTORBEAM_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
#include "c_trigger_tractorbeam.h"
#else
#include "trigger_tractorbeam.h"
#endif

#include "utlvector.h"
#include "igamesystem.h"

class CTrigger_TractorBeam_Shared  //defined as a class to make intellisense more intelligent
{
public:

	static void RemoveDeadBlobsFromBeams();
	static void RemoveBlobsFromPreviousBeams();
};


struct BeamInfo_t
{
	BeamInfo_t() : m_hBeamHandle( NULL ), m_nLastFrameUpdate( 0 )
	{
	}

	BeamInfo_t( const BeamInfo_t& copy )
	{
		m_hBeamHandle = copy.m_hBeamHandle;
		m_nLastFrameUpdate = copy.m_nLastFrameUpdate;
	}

	EHANDLE m_hBeamHandle;
	int m_nLastFrameUpdate;
};


struct EntityBeamHistory_t
{
	EntityBeamHistory_t()
	{
		m_beams.Purge();
	}

	EntityBeamHistory_t( const EntityBeamHistory_t& copy )
	{
		m_beams.CopyArray( copy.m_beams.Base(), copy.m_beams.Count() );
	}

	~EntityBeamHistory_t()
	{
		m_beams.Purge();
	}

	bool IsDifferentBeam( CTrigger_TractorBeam* pNewBeam );

	void UpdateBeam( CTrigger_TractorBeam *pBeam )
	{
		// remove the existing beam in the list
		LeaveBeam( pBeam );

		// if we have full list, remove the last one before we add the new one.
		if ( m_beams.Count() == 6 )
		{
			m_beams.Remove( 5 );
		}
		
		// add new beam to head to be the current
		m_beams.AddToHead();
		m_beams.Head().m_hBeamHandle = pBeam;
		m_beams.Head().m_nLastFrameUpdate = pBeam->GetLastUpdateFrame();
	}

	void LeaveBeam( CTrigger_TractorBeam *pBeam )
	{
		// remove the existing beam in the list
		for ( int i=0; i<m_beams.Count(); ++i )
		{
			if ( m_beams[i].m_hBeamHandle == pBeam )
			{
				m_beams.Remove(i);
				break;
			}
		}
	}

	void ClearAllBeams()
	{
		m_beams.Purge();
	}

	CUtlVectorFixed< BeamInfo_t, 6 > m_beams;
};

class CTractorBeam_Manager : public CAutoGameSystemPerFrame
{
public:
	CTractorBeam_Manager() : m_flLastUpdateTime(0.0)
	{
		m_entityBeamHistories.SetLessFunc( DefLessFunc( EHANDLE ) );
	}

	~CTractorBeam_Manager()
	{
		m_entityBeamHistories.Purge();
	}

	virtual void LevelShutdownPostEntity()
	{
		m_entityBeamHistories.Purge();
	}

#ifdef GAME_DLL
	virtual void PreClientUpdate()
#else
	virtual void Update( float frametime )
#endif
	{
		if ( gpGlobals->curtime - m_flLastUpdateTime > 1.f )
		{
			m_flLastUpdateTime = gpGlobals->curtime;

			// remove NULL entities
			CUtlVector< EHANDLE > removingHandles;
			for ( unsigned int i=0; i<m_entityBeamHistories.Count(); ++i )
			{
				const EHANDLE& hHandle = m_entityBeamHistories.Key( i );
				if ( hHandle == NULL )
				{
					removingHandles.AddToTail( hHandle );
				}
			}

			for ( int i=0; i<removingHandles.Count(); ++i )
			{
				m_entityBeamHistories.Remove( removingHandles[i] );
			}
		}
	}

	EntityBeamHistory_t& GetHistoryFromEnt( const EHANDLE& hEntHandle )
	{
		unsigned short index = m_entityBeamHistories.Find( hEntHandle );
		if ( index == m_entityBeamHistories.InvalidIndex() )
		{
			index = m_entityBeamHistories.Insert( hEntHandle );
		}

		return m_entityBeamHistories.Element( index );
	}

	void RemoveHistoryFromEnt( const EHANDLE& hEntHandle )
	{
		m_entityBeamHistories.Remove( hEntHandle );
	}

private:

	CUtlMap< EHANDLE, EntityBeamHistory_t > m_entityBeamHistories;
	double m_flLastUpdateTime;
};

extern CTractorBeam_Manager g_TractorBeamManager;

#endif //TRIGGRE_TRACTORBEAM_SHARED_H
