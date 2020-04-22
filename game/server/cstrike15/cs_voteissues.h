//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============
//
// Purpose:  CS-specific things to vote on
//
//=============================================================================

#ifndef CS_VOTEISSUES_H
#define CS_VOTEISSUES_H

#ifdef _WIN32
#pragma once
#endif

#include "vote_controller.h"

class CCSPlayer;


//=============================================================================

// do not re-order, stored in DB
enum
{
	kVoteKickBanPlayerReason_Other,
	kVoteKickBanPlayerReason_Cheating,
	kVoteKickBanPlayerReason_Idle,
	kVoteKickBanPlayerReason_Scamming,
};

uint32 GetKickBanPlayerReason( const char *pReasonString );

//=============

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseCSIssue : public CBaseIssue
{
	// Overrides to BaseIssue standard to this mod.
public:
	CBaseCSIssue( const char *typeString, CVoteController *pVoteController ) : CBaseIssue( typeString, pVoteController )
	{
	}

	virtual int			GetVoteIssue( void ) { return VOTEISSUE_UNDEFINED; }
	virtual const char *GetOtherTeamDisplayString() { return "#SFUI_otherteam_vote_unimplemented"; }
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CRestartGameIssue : public CBaseCSIssue
{
public:
	CRestartGameIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_RESTARTGAME, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString();
	virtual void		ListIssueDetails( CBasePlayer *forWhom );
	virtual bool		IsAllyRestrictedVote( void ){ return false; }
	virtual const char *GetVotePassedString();
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_RESTARTGAME; }
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CKickIssue : public CBaseCSIssue
{
public:
	CKickIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_KICK, pVoteController ), m_bPlayerCrashed( false )
	{
	}

	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual const char *GetVotePassedString( void );
	virtual bool		IsAllyRestrictedVote( void ) { return true; }
	virtual void		OnVoteFailed( void );
	virtual void		OnVoteStarted( void );
	virtual const char *GetDetailsString( void );
	virtual const char *GetOtherTeamDisplayString();
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_KICK; }
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode

private:
	void				ExtractDataFromDetails( const char *pszDetails, CCSPlayer **pSubject, uint32 *pReason = NULL );
	void				NotifyGC( CCSPlayer *pSubject, bool bKickedSuccessfully, uint32 unReason );

	CSteamID			m_steamIDVoteCaller;
	CSteamID			m_steamIDtoBan;
	bool				m_bPlayerCrashed;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CLoadBackupIssue : public CBaseCSIssue
{
public:
	CLoadBackupIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_LOADBACKUP, pVoteController )
	{
		m_szPrevDetailsString[ 0 ] = 0;
		m_szNiceName[ 0 ] = 0;
	}

	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual const char *GetVotePassedString( void );
	virtual bool		IsAllyRestrictedVote( void ) { return false; }
	virtual void		OnVoteFailed( void );
	virtual const char *GetDetailsString( void );
	virtual float		GetFailedVoteLockOutTime( void ) { return 1.0; }
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_LOADBACKUP; }

private:

	CUtlVector< char const * > m_arrStrings;

	char				m_szPrevDetailsString[MAX_PATH];
	char				m_szNiceName[MAX_PATH];

};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CChangeLevelIssue : public CBaseCSIssue
{
public:
	CChangeLevelIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_CHANGELEVEL, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsAllyRestrictedVote( void ){ return false; }
	virtual bool		IsEnabled( void );
	virtual bool		CanTeamCallVote( int iTeam ) const;		// Can someone on the given team call this vote?
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual const char *GetVotePassedString( void );
	virtual const char *GetDetailsString( void );
	virtual bool		IsYesNoVote( void );
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_CHANGELEVEL; }
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNextLevelIssue : public CBaseCSIssue
{
public:
	CNextLevelIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_NEXTLEVEL, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsAllyRestrictedVote( void ){ return false; }
	virtual bool		IsEnabled( void );
	virtual bool		CanTeamCallVote( int iTeam ) const;		// Can someone on the given team call this vote?
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual const char *GetVotePassedString( void );
	virtual const char *GetDetailsString( void );
	virtual bool		IsYesNoVote( void );
	virtual int			GetNumberVoteOptions( void );
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_NEXTLEVEL; }

private:
	CUtlVector <const char *> m_IssueOptions;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CScrambleTeams : public CBaseCSIssue
{
public:
	CScrambleTeams( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_SCRAMBLE, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual bool		IsAllyRestrictedVote( void ){ return false; }
	virtual const char *GetVotePassedString( void );
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_SCRAMBLE; }
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSwapTeams : public CBaseCSIssue
{
public:
	CSwapTeams( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_SWAPTEAMS, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual bool		IsAllyRestrictedVote( void ){ return false; }
	virtual const char *GetVotePassedString( void );
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_SWAPTEAMS; }
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CPauseMatchIssue : public CBaseCSIssue
{
public:
	CPauseMatchIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_PAUSEMATCH, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual bool		ShouldIgnoreCreationTimer( void ) { return true; }
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual int			GetVotesRequiredToPass( void ){ return 1; }
	virtual const char *GetVotePassedString( void );
	virtual float		GetCommandDelay( void ) { return 0.0; }
	virtual bool		IsEnabledDuringWarmup( void )	{ return true; } // Can this vote be called during warmup?
	virtual float		GetFailedVoteLockOutTime( void ) { return 1.0; }
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_PAUSEMATCH; }

};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CUnpauseMatchIssue : public CBaseCSIssue
{
public:
	CUnpauseMatchIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_UNPAUSEMATCH, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual bool		ShouldIgnoreCreationTimer( void ) { return true; }
	virtual bool		IsUnanimousVoteToPass( void ) { return true; }	// Requires all potential voters to pass
	virtual bool		IsEnabledDuringWarmup( void )	{ return true; } // Can this vote be called during warmup?
	virtual bool		IsVoteCallExclusiveToSpectators( void ) { return true; }		// Whether only spectators can call the vote
	virtual const char *GetVotePassedString( void );
	virtual float		GetFailedVoteLockOutTime( void ) { return 1.0; }
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_UNPAUSEMATCH; }
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CReadyForMatchIssue : public CBaseCSIssue
{
public:
	CReadyForMatchIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_READYFORMATCH, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual bool		ShouldIgnoreCreationTimer( void ) { return true; }
	virtual bool		IsUnanimousVoteToPass( void )	{ return true; }	// Requires all potential voters to pass
	virtual bool		IsEnabledDuringWarmup( void )	{ return true; } // Can this vote be called during warmup?
	virtual bool		IsVoteCallExclusiveToSpectators( void ) { return true; }		// Whether only spectators can call the vote
	virtual const char *GetVotePassedString( void );
	virtual float		GetFailedVoteLockOutTime( void ) { return 1.0; }
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_READYFORMATCH; }
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CNotReadyForMatchIssue : public CBaseCSIssue
{
public:
	CNotReadyForMatchIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_NOTREADYFORMATCH, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual int			GetVotesRequiredToPass( void ){ return 1; }
	virtual float		GetCommandDelay( void ) { return 0.0; }
	virtual bool		ShouldIgnoreCreationTimer( void ) { return true; }
	virtual bool		IsEnabledDuringWarmup( void )	{ return true; } // Can this vote be called during warmup?
	virtual const char *GetVotePassedString( void );
	virtual float		GetFailedVoteLockOutTime( void ) { return 1.0; }
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_NOTREADYFORMATCH; }
};
class CStartTimeOutIssue : public CBaseCSIssue
{
public:
	CStartTimeOutIssue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_STARTTIMEOUT, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual bool		CanTeamCallVote( int iTeam ) const;		// Can someone on the given team call this vote?
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual bool		IsAllyRestrictedVote( void ) { return true; }
	virtual const char *GetVotePassedString( void );
	virtual const char *GetOtherTeamDisplayString( ) { return "#SFUI_otherteam_vote_timeout"; }
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_STARTTIMEOUT; }
	virtual vote_create_failed_t MakeVoteFailErrorCodeForClients( vote_create_failed_t eDefaultFailCode );
};


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSurrender : public CBaseCSIssue
{
public:
	CSurrender( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_SURRENDER, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual bool		CanTeamCallVote( int iTeam ) const;
	virtual const char *GetDisplayString( void );
	virtual const char *GetOtherTeamDisplayString();
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual bool		IsAllyRestrictedVote( void ){ return true; }
	virtual const char *GetVotePassedString( void );
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_SURRENDER; }
	virtual bool		IsUnanimousVoteToPass( void ) {return true; }	// Requires all potential voters to pass
	virtual vote_create_failed_t MakeVoteFailErrorCodeForClients( vote_create_failed_t eDefaultFailCode )
	{
		switch ( eDefaultFailCode )
		{
		case VOTE_FAILED_WAITINGFORPLAYERS:
		case VOTE_FAILED_TEAM_CANT_CALL:
			return VOTE_FAILED_TOO_EARLY_SURRENDER;
		default:
			return eDefaultFailCode;
		}
	}
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CQueuedMatchmakingRematch : public CBaseCSIssue
{
public:
	CQueuedMatchmakingRematch( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_REMATCH, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual void		OnVoteFailed( void );						// The moment the vote fails, also has some time for feedback before the window goes away
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual bool		IsAllyRestrictedVote( void ){ return false; }
	virtual const char *GetVotePassedString( void );
	virtual int GetVoteIssue( void ) { return VOTEISSUE_REMATCH; }
	virtual void		OnVoteStarted( void );
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode
	virtual vote_create_failed_t MakeVoteFailErrorCodeForClients( vote_create_failed_t eDefaultFailCode ) { return VOTE_FAILED_REMATCH; }

public:
	static bool IsTimeForRematchVote();
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CQueuedMatchmakingContinue : public CBaseCSIssue
{
public:
	CQueuedMatchmakingContinue( CVoteController *pVoteController ) : CBaseCSIssue( VOTEISSUE_NAME_CONTINUE, pVoteController )
	{
	}
	virtual void		ExecuteCommand( void );
	virtual void		OnVoteFailed( void );						// The moment the vote fails, also has some time for feedback before the window goes away
	virtual bool		IsEnabled( void );
	virtual bool		CanCallVote( int iEntIndex, const char *pszTypeString, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime );
	virtual const char *GetDisplayString( void );
	virtual void		ListIssueDetails( CBasePlayer *pForWhom );
	virtual bool		IsAllyRestrictedVote( void ){ return false; }
	virtual const char *GetVotePassedString( void );
	virtual int GetVoteIssue( void ) { return VOTEISSUE_CONTINUE; }
	virtual void		OnVoteStarted( void );
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return true; } // Query if the issue is supported in queued matchmaking mode
	virtual bool		IsUnanimousVoteToPass() { return true; } // Require all attending humans to vote
	virtual vote_create_failed_t MakeVoteFailErrorCodeForClients( vote_create_failed_t eDefaultFailCode ) { return VOTE_FAILED_CONTINUE; }
};

#endif // CS_VOTEISSUES_H
