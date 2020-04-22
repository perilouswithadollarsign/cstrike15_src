//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Client handler for instruction players how to play
//
//=============================================================================//

#ifndef _C_GAMEINSTRUCTOR_H_
#define _C_GAMEINSTRUCTOR_H_


#include "GameEventListener.h"
#include "vgui_controls/PHandle.h"

class CBaseLesson;


struct LessonGroupConVarToggle_t
{
	ConVarRef var;
	char szLessonGroupName[ 64 ];

	explicit LessonGroupConVarToggle_t( const char *pchConVarName ) :
		var( pchConVarName )
	{
	}
};


class C_GameInstructor : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	C_GameInstructor() : CAutoGameSystemPerFrame( "C_GameInstructor" )
	{
		m_nSplitScreenSlot = -1;

		m_bHasLoadedSaveData = false;
		m_bEnsureThatInitIsNotCalledMultipleTimes = false;
	}

	void SetSlot( int nSlot ) { m_nSplitScreenSlot = nSlot; }

	// Methods of IGameSystem
	virtual bool Init( void );
	virtual void Shutdown( void );
	virtual void Update( float frametime );

	void UpdateHiddenByOtherElements( void );
	bool Mod_HiddenByOtherElements( void );

	virtual void FireGameEvent( IGameEvent *event );

	void DefineLesson( CBaseLesson *pLesson );

	const CBaseLesson * GetLesson( const char *pchLessonName );
	bool IsLessonOfSameTypeOpen( const CBaseLesson *pLesson ) const;

	// Save / Restore
	void SaveGameBlock( ISave *pSave );
	void RestoreGameBlock( IRestore *pRestore, bool );

	bool ReadSaveData( void );
	bool WriteSaveData( void );
	void KeyValueBuilder( KeyValues *pKeyValues );
	void RefreshDisplaysAndSuccesses( void );
	void ResetDisplaysAndSuccesses( void );
	void MarkDisplayed( const char *pchLessonName );
	void MarkSucceeded( const char *pchLessonName );

	void PlaySound( const char *pchSoundName );

	bool OpenOpportunity( CBaseLesson *pLesson );

	void DumpOpenOpportunities( void );

	KeyValues * GetScriptKeys( void );
	C_BasePlayer * GetLocalPlayer( void );

	void EvaluateLessonsForGameRules( void );
	void SetLessonGroupEnabled( const char *pszGroup, bool bEnabled );

private:
	void FindErrors( void );

	bool UpdateActiveLesson( CBaseLesson *pLesson, const CBaseLesson *pRootLesson );
	void UpdateInactiveLesson( CBaseLesson *pLesson );

	CBaseLesson * GetLesson_Internal( const char *pchLessonName );

	void StopAllLessons( void );

	void CloseAllOpenOpportunities( void );
	void CloseOpportunity( CBaseLesson *pLesson );

	void ReadLessonsFromFile( const char *pchFileName );
	void InitLessonPrerequisites( void );

private:
	CUtlVector < CBaseLesson* >	m_Lessons;
	CUtlVector < CBaseLesson* >	m_OpenOpportunities;

	CUtlVector < LessonGroupConVarToggle_t > m_LessonGroupConVarToggles;

	KeyValues	*m_pScriptKeys;

	bool	m_bNoDraw;
	bool	m_bHiddenDueToOtherElements;

	int		m_iCurrentPriority;
	EHANDLE	m_hLastSpectatedPlayer;
	bool	m_bSpectatedPlayerChanged;

	char	m_szPreviousStartSound[ 128 ];
	float	m_fNextStartSoundTime;
	int		m_nSplitScreenSlot;

	bool	m_bHasLoadedSaveData;
	bool	m_bEnsureThatInitIsNotCalledMultipleTimes;
};


C_GameInstructor &GetGameInstructor();


#endif // _C_GAMEINSTRUCTOR_H_
