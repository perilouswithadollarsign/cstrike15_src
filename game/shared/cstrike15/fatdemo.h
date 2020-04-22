//=========== Copyright  Valve Corporation, All rights reserved. ==============//

#pragma once

#include "GameEventListener.h"
#include "igamesystem.h"

// This is completely disabled in release builds. We overwrite client Rel DLLs with client Staging DLLs on our cluster.
#if !defined( CSTRIKE_REL_BUILD )

// Records game state to a fat demo format.
class CCSFatDemoRecorder : public CAutoGameSystem, public CGameEventListener
{
public:
	CCSFatDemoRecorder();
	virtual ~CCSFatDemoRecorder();

	void Reset();

	virtual void FireGameEvent( IGameEvent *event ) OVERRIDE;

	virtual void PostInit() OVERRIDE;
	virtual void LevelInitPreEntity() OVERRIDE;
	virtual void LevelShutdownPostEntity() OVERRIDE;

	void OnTickPre( int tickcount );

private:
	void OutputProtobuf( ::google::protobuf::Message* pTick );

	void BeginFile();
	void FinalizeFile();

private:
	CUtlString m_currentMap;
	int m_tickcount;
	bool m_bInLevel;

	MLTick* m_pCurrentTick;
	FileHandle_t m_outFile;

	CUtlBuffer m_tempPacketStorage;
};

// The global to get at the instance of this class.
extern CCSFatDemoRecorder* g_pFatDemoRecorder;

#endif