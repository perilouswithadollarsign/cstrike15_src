//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CS_CLIENTMODE_H
#define CS_CLIENTMODE_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include "counterstrikeviewport.h"
#include "matchmaking/imatchframework.h"
#include "c_cs_player.h"
#include "usermessages.h"

enum PostProcessEffect_t
{
	POST_EFFECT_DEFAULT = 0,
	POST_EFFECT_LOW_HEATH,			// 1st person	event
	POST_EFFECT_VERY_LOW_HEATH,
	POST_EFFECT_IN_BUY_MENU,		// 1st person	event
	POST_EFFECT_DEATH_CAM,				// 1st person	event
	POST_EFFECT_SPECTATING,					// 1st person	state
	POST_EFFECT_IN_FIRE,
	POST_EFFECT_ZOOMED_RIFLE,
	POST_EFFECT_ZOOMED_SNIPER,
	POST_EFFECT_ZOOMED_SNIPER_MOVING,
	POST_EFFECT_UNDER_WATER,
	POST_EFFECT_ROUND_END_VIA_BOMBING,
	POST_EFFECT_SPEC_CAMERA_LERPING,
	POST_EFFECT_MAP_CONTROLLED,
	POST_EFFECT_DEATH_CAM_BODYSHOT,				// 1st person	event
	POST_EFFECT_DEATH_CAM_HEADSHOT,				// 1st person	event
	NUM_POST_EFFECTS
};

enum RoundStatus_t
{
	ROUND_UNKNOWN = 0,
	ROUND_STARTED,
	ROUND_ENDED,
	ROUND_ENDED_VIA_BOMBING
};

struct PostProcessEffectParams_t
{
	float fLocalContrastStrength;
	float fLocalContrastEdgeStrength;
	float fVignetteStart;
	float fVignetteEnd;
	float fVignetteBlurStrength;
	float fFadeToBlackStrength;
};

enum SyncKeyBindingValueDirection_t
{
	KEYBINDING_WRITE_TO_TITLEDATA,
	KEYBINDING_READ_FROM_TITLEDATA
};

class SpecCameraPosition_t
{
public:
	Vector	vecPosition;
	Vector	vecAngles;

	float	flWeight;
};

class ClientModeCSNormal : public ClientModeShared, public IMatchEventsSink
{
DECLARE_CLASS( ClientModeCSNormal, ClientModeShared );

private:

// IClientMode overrides.
public:

					ClientModeCSNormal();
					~ClientModeCSNormal();

	virtual void	OnEvent( KeyValues *pEvent );

	virtual void	Init();
	virtual void	InitViewport();
	// dgoodenough - fix GCC shortcoming
	// PS3_BUILDFIX
	virtual void	InitViewport( bool bOnlyBaseClass = false );
	virtual void	Update();

	virtual void	LevelShutdown( void );

	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	int				GetDeathMessageStartHeight( void );

	virtual void	FireGameEvent( IGameEvent *event );
	virtual void	PostRenderVGui();
	virtual void	PreRender(CViewSetup *pSetup) OVERRIDE;

	virtual bool	ShouldDrawViewModel( void );

	virtual bool	CanRecordDemo( char *errorMsg, int length ) const;

	virtual wchar_t*	GetServerName( void ) { return m_pServerName; }
	virtual void		SetServerName( wchar_t *name );
	virtual wchar_t*	GetMapName( void ) { return m_pMapName; }
	virtual void		SetMapName( wchar_t *name );

	virtual void	SetBlurFade( float scale );
	virtual float	GetBlurFade( void ) { return 0; }
	virtual void	DoPostScreenSpaceEffects( const CViewSetup *pSetup );

	virtual void	UpdateColorCorrectionWeights( void );
	virtual void	OnColorCorrectionWeightsReset( void );
	virtual float	GetColorCorrectionScale( void ) const;

	void			SetupStaticCameras( void );
	bool			GetIdealCameraPosForPlayer( int playerindex );

	void			LoadPostProcessParamsFromFile( const char* pFileName = NULL );
	void			UpdatePostProcessingEffects();
	void			PostProcessLerpTo( PostProcessEffect_t effectID, float fFadeDuration = 0.75f, const PostProcessParameters_t* pTargetParams = NULL );
	void			PostProcessLerpTo( PostProcessEffect_t effectID, const C_PostProcessController* pPPController );
	void			DoPostProcessParamLerp();
	void			LerpPostProcessParam(	float fAmount, PostProcessParameters_t& result, const PostProcessParameters_t& from,
		const PostProcessParameters_t& to ) const;
	PostProcessEffect_t PostProcessEffectFromName( const char* pName ) const;
	void			GetDefaultPostProcessingParams( C_CSPlayer* pPlayer, PostProcessEffectParams_t* pParams );

	void			SyncCurrentKeyBindingsToDeviceTitleData( int iController, int eDevice, const SyncKeyBindingValueDirection_t eOp );

	void			CheckTitleDataStorageConnected( void );

#if !defined(NO_STEAM) && !defined (_PS3)
	STEAM_CALLBACK_MANUAL( ClientModeCSNormal, OnScreenshotRequested, ScreenshotRequested_t, m_CallbackScreenshotRequested );
#endif

	CUserMessageBinder m_UMCMsgKillCam;
	CUserMessageBinder m_UMCMsgMatchEndConditions;
	CUserMessageBinder m_UMCMsgDisconnectToLobby;
	CUserMessageBinder m_UMCMsgDisplayInventory;
	CUserMessageBinder m_UMCMsgWarmupHasEnded;
	CUserMessageBinder m_UMCMsgServerRankUpdate;
	CUserMessageBinder m_UMCMsgServerRankRevealAll;
	CUserMessageBinder m_UMCMsgScoreLeaderboardData;
	CUserMessageBinder m_UMCMsgGlowPropTurnOff;
	CUserMessageBinder m_UMCMsgXpUpdate;
	CUserMessageBinder m_UMCMsgQuestProgress;
	CUserMessageBinder m_UMCMsgPlayerDecalDigitalSignature;

private:
	//int				CameraSortFunction( SpecCameraPosition_t* entry1, SpecCameraPosition_t* entry2 );

	wchar_t			m_pServerName[256];
	wchar_t			m_pMapName[256];

	CHandle<C_ColorCorrection>	m_hCurrentColorCorrection;
	//	void	UpdateSpectatorMode( void );

	ClientCCHandle_t	m_CCKillCamReplay;	// handle to death cc effect
	float				m_CCKillCamReplayPercent;
	ClientCCHandle_t	m_CCDeathHandle;	// handle to death cc effect
	float				m_CCDeathPercent;
	ClientCCHandle_t	m_CCFreezePeriodHandle_CT;
	float				m_CCFreezePeriodPercent_CT;
	ClientCCHandle_t	m_CCFreezePeriodHandle_T;
	float				m_CCFreezePeriodPercent_T;
	ClientCCHandle_t	m_CCPlayerFlashedHandle;
	float				m_CCPlayerFlashedPercent;

	RoundStatus_t		m_iRoundStatus;

	float				m_fDelayedCTWinTime;

	static PostProcessParameters_t ms_postProcessParams[];
	static const char* ms_postProcessEffectNames[];
	PostProcessEffect_t m_activePostProcessEffect;
	PostProcessEffect_t m_lastPostProcessEffect;
	const C_PostProcessController* m_pActivePostProcessController;
	CountdownTimer m_postProcessEffectCountdown;
	PostProcessParameters_t m_postProcessLerpStartParams;
	PostProcessParameters_t m_postProcessLerpEndParams;
	PostProcessParameters_t m_postProcessCurrentParams;

	CUtlVector< SpecCameraPosition_t >	m_SpecCameraPositions;
	int m_nRoundMVP;

public:
	struct CQuestUncommittedProgress_t
	{
		uint32 m_numNormalPoints;
		double m_dblNormalPointsProgressTime;
		uint32 m_numNormalPointsProgressBaseline;
		bool m_bIsEventQuest;
	};
	static CUtlMap< uint32, CQuestUncommittedProgress_t, uint32, CDefLess< uint32 > > sm_mapQuestProgressUncommitted;
	static ScoreLeaderboardData s_ScoreLeaderboardData;
	static uint32 s_numLevelTransitions;
};


extern IClientMode *GetClientModeNormal();
extern ClientModeCSNormal* GetClientModeCSNormal();


#endif // CS_CLIENTMODE_H
