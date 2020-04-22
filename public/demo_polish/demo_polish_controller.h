//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef DEMO_POLISH_CONTROLLER_H
#define DEMO_POLISH_CONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

//------------------------------------------------------------------------------------------------------------------------

#include "demo_polish/demo_polish_consts.h"
//#include "locator.h"
#include "../game/shared/Multiplayer/multiplayer_animstate.h"

//------------------------------------------------------------------------------------------------------------------------

class CBonePolishData;
class CDemoPolishFile;
class C_BaseAnimating;

//------------------------------------------------------------------------------------------------------------------------

class CHeaderFile;
class CBoneBitList;
class CFinalDemoPolishFile;
class FinalPolishElementData;
class CBonePath;
class CPathManager;
class CDemoPolishEventQueue;
class CLowerBodyController;
class CDemoPolishConfig;
class CDemoPolishPanel;
class ISequenceAnalyzer;

class CDemoPolishController
{
public:
	static CDemoPolishController& Instance();

	bool Init( char const* pDemoBaseName );
	void Think( float flPlaybackTime );
	bool Shutdown();

	bool IsInitialized() const	{ return m_bLateInit; }
	bool IsEntityRegistered( int iEntIndex ) const;

	void RefreshEvents();

	int GetSequenceOverride( int iPlayerEntIndex, float flCurrentDemoTime );
	void MakeGlobalAdjustments( int iPlayerEntIndex, CStudioHdr const* pStudioHdr, int& iBoneMask, CBoneAccessor& boneAccessor );
	void MakeLocalAdjustments( int iPlayerEntIndex, CStudioHdr const* pStudioHdr, int& iBoneMask, Vector* pPositions,
							   Quaternion* pRotations, CBoneBitList& boneComputed );

	void DrawUnpolishedSkeleton( int iPlayerEntIndex, CStudioHdr const* pStudioHdr );
	void DrawUnpolishedSkeleton( int iPlayerLookupIndex, CStudioHdr const* pStudioHdr, CBonePath const* pBonePath, int iDrawFrame, float s, int r, int g, int b );

	float GetAdjustedPlaybackTime() const;
	float GetElapsed() const;

	QAngle& GetRenderAngles( int iPlayerEntIndex ) const;
	Vector& GetRenderOrigin( int iPlayerEntIndex ) const;

	void UpdatePlayerAnimState( int iPlayerEntIndex, PlayerAnimEvent_t iEvent );

public:
	bool m_bInit;

private:
	friend class CDemoPolishRecorder;

	CDemoPolishController();
	~CDemoPolishController();

	void MakeRootAdjustments( int iPlayerLookupIndex, Vector& pos, Quaternion& rot );
	void ReloadDemoPolishConfig();
	bool InitPanel();
	void DispatchEvents( int iPlayerLookupIndex, float flCurrentDemoTime );
	bool ReadEventData( int iPlayerLookupIndex );
	bool AnalyzeDataForEvents( int iPlayerLookupIndex, int iStartFrame = 0 );
	int GetLookupIndexFromEntIndex( int iPlayerEntIndex ) const;
	int GetEntIndexFromLookupIndex( int iPlayerLookupIndex ) const;
	void AddNewLeaningStartEvent( int iPlayerLookupIndex, float flStartTime, float flLeanDir, bool bStrafe );
	void AddNewLeaningStopEvent( int iPlayerLookupIndex, float flStopTime, float flLeanDir, bool bStrafe );

	CUtlVector< CDemoPolishFile* > m_vFiles;

	int m_iNumPlayerFiles;

public:
	CBonePath** m_pBonePaths;
	CPathManager* m_pPathManager;
	CHeaderFile* m_pHeader;
	CDemoPolishEventQueue** m_pEventQueues;					// TODO: One queue per player (as of now this is implemented as one queue for one player)
	CLowerBodyController** m_pLowerBodyControllers;
	CDemoPolishConfig* m_pDemoPolishConfig;
	ISequenceAnalyzer** m_pSequenceAnalyzers;

	mutable Vector m_renderOrigins[kMaxPlayersSupported];
	mutable QAngle m_renderAngles[kMaxPlayersSupported];

private:
	float m_flLastTime;
	float m_flElapsed;
	Vector m_noisePt;

	bool LateInit();
	bool m_bLateInit;

	CDemoPolishPanel* m_pDemoPolishPanel;

	CThreadFastMutex m_mutex;
};

//------------------------------------------------------------------------------------------------------------------------

#endif // DEMO_POLISH_CONTROLLER_H
