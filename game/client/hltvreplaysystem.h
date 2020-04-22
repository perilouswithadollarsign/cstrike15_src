//======= Copyright (c) Valve Corporation, All rights reserved. ======
#ifndef HLTV_REPLAY_SYSTEM
#define HLTV_REPLAY_SYSTEM

// this struct is followed by matrix3x4a_t bones
struct ALIGN16 CachedRagdollBones_t
{
	int nBones;
	int nBodyParts;
	bool bAllAsleep;

	matrix3x4a_t *GetBones() { return ( matrix3x4a_t * )( this + 1 ); }
	matrix3x4a_t *GetBodyParts() { return GetBones() + nBones; }

} ALIGN16_POST;

class CHltvReplaySystem: public CGameEventListener
{
public:
	struct ReplayParams_t
	{
		int nRequest; // member of enum ReplayEventType_t
		int nPrimaryTargetEntIndex;
		float flEventTime;

		float flTime; // when gpGlobal->curtime reaches this value, replay starts
		int nPrimaryVictimEntIndex;
		bool bPrimaryVictimIsLocalPlayer;
		bool bFinalKillOfRound;

		ReplayParams_t()
		{
			Reset();
		}
		void Stop()
		{
			nRequest = -1;
		}
		void Reset()
		{
			nRequest = -1;
			flTime = -1000;
			nPrimaryVictimEntIndex = nPrimaryVictimEntIndex = -1;
			bPrimaryVictimIsLocalPlayer = false;
			flEventTime = 0;
			bFinalKillOfRound = false;
		}

		bool IsValid() { return nRequest >= 0; }
		void Invalidate() { nRequest = -1; }
	};

	class CLocalPlayerProps
	{
	public:
		bool m_bLastSeenAlive;
		int m_nLastTickUpdated;
	public:
		CLocalPlayerProps();
		bool Update();
	};
public:
	CHltvReplaySystem();
	~CHltvReplaySystem();

	void OnHltvReplay( const CSVCMsg_HltvReplay  &msg );
	void OnHltvReplayTick();

	void EmitTimeJump();

	int GetHltvReplayDelay() { return m_nHltvReplayDelay; }
	void StopHltvReplay();
	float GetReplayVideoFadeAmount()const { m_flDebugLastRequestedReplayVideoFadeAmount = m_flReplayVideoFadeAmount;  return m_flReplayVideoFadeAmount; }
	float GetHltvReplayDistortAmount()const;

	bool IsHltvReplayButtonEnabled();

	bool IsHltvReplayButtonTimedOut();

	void RequestHltvReplayDeath();
	void RequestHltvReplay( const ReplayParams_t &replay );
	void RequestCancelHltvReplay( bool bSuppressFadeout = false );
	bool IsHltvReplayFeatureEnabled();

	bool IsFadeoutFinished();
	bool IsFadeoutActive();
	bool IsDelayedReplayRequestPending() { return m_DelayedReplay.IsValid(); }
	bool PrepareHltvReplayCountdown( );
	void CancelDelayedHltvReplay();

	void OnLevelInit();
	void OnPlayerDeath( IGameEvent *event );

	bool WantsReplayEffect()const;
	void OnDemoPlayback( bool bPlaying );
	void SetDemoPlaybackHighlightXuid( uint64 xuid, bool bLowlights );
	void SetDemoPlaybackFadeBrackets( int nCurrentPlaybackTick, int nStartAt, int nStopAt );
	bool IsDemoPlayback() const { return m_bDemoPlayback; }
	bool IsDemoPlaybackLowLights() const { return m_bDemoPlaybackLowLights; }
	void FireGameEvent( IGameEvent *event );
	void OnLevelShutdown();
	void OnLocalPlayerRespawning();

	int GetPrimaryVictimEntIndex()const { return m_DelayedReplay.nPrimaryVictimEntIndex; }
	bool IsPrimaryVictimLocalPlayer()const { return m_DelayedReplay.bPrimaryVictimIsLocalPlayer; }
	bool IsReplayingFinalKillOfRound()const { return m_nHltvReplayDelay && m_DelayedReplay.bFinalKillOfRound; }

	void StopFades();
	void StopFadeout();
	void StartFadeout(float flEndTime, float flDuration );
	void StartFadein( float flStartRealTime );
	void SetReplaySoundMixLayer( float flFade );

	C_BasePlayer *GetDemoPlaybackPlayer();
	bool IsDemoPlaybackXuidOther()const;

	void Update();

	void PurgeRagdollBoneCache();

	void CacheRagdollBones();
	CachedRagdollBones_t *GetCachedRagdollBones( int nEntIndex, bool bTake );
	void FreeCachedRagdollBones( CachedRagdollBones_t* );


	enum ExperimentalEvents_t
	{
		EE_REPLAY_OFFERED = 1,
		EE_REPLAY_REQUESTED = 2,
		EE_REPLAY_STARTED = 4,
		EE_REPLAY_CANCELLED = 8,
		EE_REPLAY_AUTOMATIC	= 16,
		EE_REPLAY_STUCK = 32
		//EE_REPLAY_VICTIM_POV = 64 // never shipped, removed after discussions with Vitaliy, BrianL, Gautam
	};

	uint GetExperimentalEvents() const { return m_nExperimentalEvents; }
	bool UpdateHltvReplayButtonTimeOutState();
protected:
	static float GetReplayMessageTime();
protected:
	int		m_nHltvReplayDelay;
	int		m_nHltvReplayStopAt;
	int		m_nHltvReplayStartAt;
	int		m_nHltvReplaySlowdownBegin;
	int		m_nHltvReplaySlowdownEnd;
	float	m_flHltvReplaySlowdownRate;
	float	m_flHltvLastRequestRealTime;
	float	m_flLastReplayStoppedRealTime;
	float	m_flLastPlayerDeath;
	bool	m_bHltvReplayButtonTimedOut;
	int		m_nHltvReplayPrimaryTarget;
	CLocalPlayerProps m_LocalPlayer;
	bool	m_bWaitingForHltvReplayTick;
	float	m_flStartedWaitingForHltvReplayTickRealTime;
	int		m_nHltvReplayBeginTick;

	ReplayParams_t m_DelayedReplay;

	bool	m_bDemoPlayback;
	bool	m_bListeningForGameEvents;

	int		m_nDemoPlaybackStartAt;
	int		m_nDemoPlaybackStopAt;
	uint64	m_nDemoPlaybackXuid;
	bool	m_bDemoPlaybackLowLights;
	CHandle< C_BasePlayer > m_DemoPlaybackPlayer;
	int		m_nCurrentPlaybackTick;
	float	m_flCurrentPlaybackTime;

	float m_flReplayVideoFadeAmount;
	float m_flReplaySoundFadeAmount;

	float m_flFadeinStartRealTime;
	float m_flFadeinDuration;
	float m_flFadeoutEndTime;
	float m_flFadeoutDuration;

	mutable float m_flDebugLastRequestedReplayVideoFadeAmount;

	uint32	m_nSteamSelfAccountId;

	uint32 m_nExperimentalEvents;

	float m_flFreeCachedRagdollBonesTime; // time (when not replaying) to destroy cached-off ragdoll bones
	CUtlHashtable< int, CachedRagdollBones_t* > m_mapCachedRagdollBones;
};

extern CHltvReplaySystem g_HltvReplaySystem;
extern int CL_GetHltvReplayDelay();

#endif // HLTV_REPLAY_SYSTEM