//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Player-driven Voting System for Multiplayer Source games (currently implemented for TF2)
//
// $NoKeywords: $
//=============================================================================//

#ifndef VOTE_CONTROLLER_H
#define VOTE_CONTROLLER_H

#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"

#define MAX_COMMAND_LENGTH 64
#define MAX_CREATE_ERROR_STRING 96

// TODO: look into doing enum instead of string compares here - mtw
#define	VOTEISSUE_NAME_KICK					"Kick"
#define	VOTEISSUE_NAME_CHANGELEVEL		"ChangeLevel"
#define	VOTEISSUE_NAME_NEXTLEVEL		"NextLevel"
#define	VOTEISSUE_NAME_SWAPTEAMS		"SwapTeams"
#define	VOTEISSUE_NAME_SCRAMBLE			"ScrambleTeams"
#define	VOTEISSUE_NAME_RESTARTGAME		"RestartGame"
#define	VOTEISSUE_NAME_SURRENDER		"Surrender"
#define	VOTEISSUE_NAME_REMATCH			"Rematch"
#define	VOTEISSUE_NAME_CONTINUE			"ContinueGame"
#define	VOTEISSUE_NAME_PAUSEMATCH		"PauseMatch"
#define VOTEISSUE_NAME_UNPAUSEMATCH		"UnpauseMatch"
#define VOTEISSUE_NAME_LOADBACKUP		"LoadBackup"
#define VOTEISSUE_NAME_READYFORMATCH	"ReadyForMatch"
#define VOTEISSUE_NAME_NOTREADYFORMATCH	"NotReadyForMatch"
#define VOTEISSUE_NAME_STARTTIMEOUT		"StartTimeOut"

class CVoteController;

extern CVoteController *g_voteControllerGlobal;
extern CVoteController *g_voteControllerCT;
extern CVoteController *g_voteControllerT;

class CBaseIssue	// Abstract base class for all things-that-can-be-voted-on.  
{
public:
	CBaseIssue( const char *typeString, CVoteController *pVoteController );
	virtual ~CBaseIssue();
	const char			*GetTypeString( void );						// Connection between console command and specific type of issue
	virtual const char	*GetDetailsString();
	virtual void		SetIssueDetails( const char *pszDetails );	// We need to know the details part of the con command for later
	virtual void		OnVoteFailed( void );						// The moment the vote fails, also has some time for feedback before the window goes away
	virtual void		OnVoteStarted( void ) {}					// Called as soon as the vote starts
	virtual bool		IsEnabled( void ) { return false; }			// Query the issue to see if it's enabled
	virtual bool		IsEnabledInQueuedMatchmaking( void ) { return false; } // Query if the issue is supported in queued matchmaking mode
	virtual bool		IsEnabledDuringWarmup( void )		{ return false; } // Can this vote be called during warmup?
	virtual float		GetCommandDelay( void );	
	virtual bool		ShouldIgnoreCreationTimer( void ) { return false; }	// should this issue ignore sv_vote_creation_timer that prevents spamming callvotes?
	virtual bool		CanTeamCallVote( int iTeam ) const;			// Can someone on the given team call this vote?
	virtual bool		CanCallVote( int iEntIndex, const char *pszCommand, const char *pszDetails, vote_create_failed_t &nFailCode, int &nTime ); // Can this guy hold a vote on this issue?
	virtual bool		IsAllyRestrictedVote( void );				// Can only members of the same team vote on this?
	virtual bool		IsUnanimousVoteToPass( void ) {return false; }	// Requires all potential voters to pass
	virtual bool		IsVoteCallExclusiveToSpectators( void ) { return false; }		// Whether only spectators can call the vote
	virtual int			GetVotesRequiredToPass( void );					// how many votes are required to pass
	virtual const char *GetDisplayString( void ) = 0;				// The string that will be passed to the client for display
	virtual const char *GetOtherTeamDisplayString( void ) = 0;				// The string that will be passed to the client for a vote being cast by the other team
	virtual void		ExecuteCommand( void ) = 0;					// Where the magic happens.  Do your thing.
	virtual void		ListIssueDetails( CBasePlayer *pForWhom ) = 0;	// Someone would like to know all your valid details
	virtual const char *GetVotePassedString( void );				// Get the string an issue would like to display when it passes.
	virtual int			CountPotentialVoters( void );
	virtual int			GetNumberVoteOptions( void );				// How many choices this vote will have.  i.e. Returns 2 on a Yes/No issue (the default).
	virtual bool		IsYesNoVote( void );
	virtual void		SetYesNoVoteCount( int iNumYesVotes, int iNumNoVotes, int iNumPotentialVotes );
	virtual bool		GetVoteOptions( CUtlVector <const char*> &vecNames );	// We use this to generate options for voting
	virtual int			GetVoteIssue( void ) { return VOTEISSUE_UNDEFINED; }
	virtual float		GetFailedVoteLockOutTime( void );			// How long to wait before a failed vote can be resubmitted.
	virtual vote_create_failed_t MakeVoteFailErrorCodeForClients( vote_create_failed_t eDefaultFailCode ) { return eDefaultFailCode; }

protected:
	static void			ListStandardNoArgCommand( CBasePlayer *forWhom, const char *issueString );		// List a Yes vote command

	struct FailedVote
	{
		char	szFailedVoteCommand[MAX_COMMAND_LENGTH];
		char	szFailedVoteParameter[MAX_VOTE_DETAILS_LENGTH];
		float	flLockoutTime;					
	};

	CUtlVector<FailedVote *> m_FailedVotes;

	char				m_szTypeString[MAX_COMMAND_LENGTH];
	char				m_szDetailsString[MAX_PATH];

	int m_iNumYesVotes;
	int m_iNumNoVotes;
	int m_iNumPotentialVotes;

	CVoteController *m_pVoteController;


};




class CVoteController : public CBaseEntity
{
	DECLARE_CLASS( CVoteController, CBaseEntity );
	
public:
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	virtual ~CVoteController();

	enum TryCastVoteResult
	{
		CAST_OK,
		CAST_FAIL_SERVER_DISABLE,
		CAST_FAIL_NO_ACTIVE_ISSUE,
		CAST_FAIL_TEAM_RESTRICTED,
		CAST_FAIL_NO_CHANGES,
		CAST_FAIL_DUPLICATE,
		CAST_FAIL_VOTE_CLOSED,
		CAST_FAIL_SYSTEM_ERROR
	};

	virtual void	Spawn( void );
	virtual int		UpdateTransmitState( void );

	bool			SetupVote( int iEntIndex );	// This creates a list of issues for the UI
	bool			CreateVote( int iEntIndex, const char *pszTypeString, const char *pszDetailString );	// This is what the UI passes in
	TryCastVoteResult TryCastVote( int iEntIndex, const char *pszVoteString );
	float			GetAcceptingVotesTimeLeft() { return m_acceptingVotesTimer.GetRemainingTime(); }
	void			RegisterIssue( CBaseIssue *pNewIssue );
	void			ListIssues( CBasePlayer *pForWhom );
	bool			IsValidVoter( CBasePlayer *pWhom );
	bool			CanTeamCastVote( int iTeam ) const;
	void			SendVoteFailedMessage( vote_create_failed_t nReason = VOTE_FAILED_GENERIC, CBasePlayer *pVoteCaller = NULL, int nTime = -1 );
	void			VoteChoice_Increment( int nVoteChoice );
	void			VoteChoice_Decrement( int nVoteChoice );
	int				GetWinningVoteOption( void );
	int				GetCallingEntity( void )  { return m_iEntityHoldingVote; }
	int				GetPotentialVotes( void ) { return m_nPotentialVotes.Get(); }
	bool			IsAVoteInProgress( void ) { return ( m_iActiveIssueIndex != INVALID_ISSUE ); }
	bool			HasIssue( const char *pszIssue );
	void			EndVoteImmediately( void );

protected:
	void			ResetData( void );
	void			VoteControllerThink( void );
	void			CheckForEarlyVoteClose( void ); // If everyone has voted (and changing votes is not allowed) then end early

	CNetworkVar( int, m_iActiveIssueIndex );					// Type of thing being voted on
	CNetworkVar( int, m_iOnlyTeamToVote );						// If an Ally restricted vote, the team number that is allowed to vote
	CNetworkArray( int, m_nVoteOptionCount, MAX_VOTE_OPTIONS );	// Vote options counter
	CNetworkVar( int, m_nPotentialVotes );						// How many votes could come in, so we can close ballot early
	CNetworkVar( bool, m_bIsYesNoVote );						// Is the current issue Yes/No?
	CountdownTimer	m_acceptingVotesTimer;						// How long from vote start until we count the ballots
	CountdownTimer	m_executeCommandTimer;						// How long after end of vote time until we execute a passed vote
	CountdownTimer	m_resetVoteTimer;							// when the current vote will end 
	CUtlVector< uint64 > m_arrVotedUsers;						// SteamIDs of users who voted already
	int				m_nVotesCast[MAX_PLAYERS];					// votes cast by each entity
	int				m_iEntityHoldingVote;
	int				m_nHighestCountIndex;
	
	CUtlVector <CBaseIssue *> m_potentialIssues;
	CUtlVector <const char *> m_VoteOptions;
};


#endif // VOTE_CONTROLLER_H
