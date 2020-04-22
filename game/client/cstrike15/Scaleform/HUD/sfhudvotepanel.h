//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SFHUDVOTEPANEL_H
#define SFHUDVOTEPANEL_H
#ifdef _WIN32
#pragma once
#endif //_WIN32

#include "sfhudflashinterface.h"

#define VOTE_PANEL_NAME_TRUNCATE_AT	16  // number of name character displayed before truncation

class SFHudVotePanel: public SFHudFlashInterface
{
public:
	explicit SFHudVotePanel( const char *value );
	virtual void ProcessInput( void );
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );	
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );
	virtual void FireGameEvent( IGameEvent * event );
	virtual void SetActive( bool bActive );

	//void TimerCallback( SCALEFORM_CALLBACK_ARGS_DECL );
	void VoteYes( SCALEFORM_CALLBACK_ARGS_DECL );
	void VoteNo( SCALEFORM_CALLBACK_ARGS_DECL );
	void UpdateYesNoButtonText( bool bShowOtherTeam = false );

	void Hide( void );
	virtual bool ShouldDraw( void );

	bool MsgFunc_CallVoteFailed( const CCSUsrMsg_CallVoteFailed &msg );
	bool MsgFunc_VoteStart( const CCSUsrMsg_VoteStart &msg );
	bool MsgFunc_VotePass( const CCSUsrMsg_VotePass &msg );
	bool MsgFunc_VoteFailed( const CCSUsrMsg_VoteFailed &msg );
	bool MsgFunc_VoteSetup( const CCSUsrMsg_VoteSetup &msg );

	void ShowVoteUI( wchar_t* headerText, wchar_t* voteText, bool bShowingOtherTeam = false );	

	void OnThink( void );

	CUserMessageBinder m_UMCMsgCallVoteFailed;
	CUserMessageBinder m_UMCMsgVoteStart;
	CUserMessageBinder m_UMCMsgVotePass;
	CUserMessageBinder m_UMCMsgVoteFailed;
	CUserMessageBinder m_UMCMsgVoteSetup;

private:
	void SetVoteActive( bool bActive );

	SFVALUE			m_hVoteButtonBG;
	ISFTextObject*	m_hVoteLocalCast;
	ISFTextObject*	m_option1Text;
	ISFTextObject*	m_option2Text;
	ISFTextObject*	m_option1CountText;
	ISFTextObject*	m_option2CountText;
	
	CUtlStringList m_VoteSetupIssues;
	CUtlStringList m_VoteSetupMapCycle;
	CUtlStringList m_VoteSetupChoices;

	bool m_bVoteActive;
	float m_flVoteResultCycleTime;	// what time will we cycle to the result
	float m_flHideTime;				// what time will we hide
	bool m_bVotePassed;				// what mode are we going to cycle to
	int m_nVoteOptionCount[MAX_VOTE_OPTIONS];	// Vote options counter
	int m_nPotentialVotes;						// If set, draw a line at this point to show the required bar length
	bool m_bIsYesNoVote;
	int m_nVoteChoicesCount;
	bool m_bPlayerVoted;
	int m_bPlayerLocalVote;
	float m_flPostVotedHideTime;
	bool m_bVisible;
	bool m_bHasFocus;

	int m_nVoteYes;
	int m_nVoteNo;
};

#endif // SFHUDVOTEPANEL_H
