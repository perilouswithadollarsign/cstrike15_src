//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _BASEMODFACTORYBASEPANEL_H__
#define _BASEMODFACTORYBASEPANEL_H__

#include "vgui_controls/Panel.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/Button.h"
#include "tier1/utllinkedlist.h"
#include "avi/ibik.h"
#include "ixboxsystem.h"
#include "matchmaking/imatchframework.h"

namespace BaseModUI 
{

	//=============================================================================
	//
	//=============================================================================
	class CBaseModPanel : public vgui::EditablePanel, public IMatchEventsSink
	{
		DECLARE_CLASS_SIMPLE( CBaseModPanel, vgui::EditablePanel );

	public:
		CBaseModPanel();
		~CBaseModPanel();

		// IMatchEventSink implementation
	public:
		virtual void OnEvent( KeyValues *pEvent );

	public:
		static CBaseModPanel& GetSingleton();
		static CBaseModPanel* GetSingletonPtr();

		void ReloadScheme();

		bool IsLevelLoading();

		void OnGameUIActivated();
		void OnGameUIHidden();
		void OpenFrontScreen();
		void RunFrame();
		void OnLevelLoadingStarted( char const *levelName, bool bShowProgressDialog );
		void OnLevelLoadingFinished( KeyValues *kvEvent );
		bool UpdateProgressBar(float progress, const char *statusText);
		void OnCreditsFinished(void);

		bool IsReadyToWriteConfig( void );
		void StartExitingProcess( bool bWarmRestart );

		void SetLastActiveUserId( int userId );
		int GetLastActiveUserId();

		bool IsMenuBackgroundMovieValid( void );

		bool IsBackgroundMusicPlaying();
		bool StartBackgroundMusic( float fVol );
		void UpdateBackgroundMusicVolume( float fVol );
		void ReleaseBackgroundMusic();

		void SafeNavigateTo( Panel *pExpectedFrom, Panel *pDesiredTo, bool bAllowStealFocus );

#if defined( _GAMECONSOLE ) && defined( _DEMO )
		void OnDemoTimeout();
#endif

	protected:
		CBaseModPanel(const CBaseModPanel&);
		CBaseModPanel& operator=(const CBaseModPanel&);

		void ApplySchemeSettings(vgui::IScheme *pScheme);
		void PaintBackground();

		void OnCommand(const char *command);
		void OnSetFocus();
		virtual bool RequestInfo( KeyValues *data );

		MESSAGE_FUNC( OnMovedPopupToFront, "OnMovedPopupToFront" );

	private:
		void DrawColoredText( vgui::HFont hFont, int x, int y, unsigned int color, const char *pAnsiText );
		void DrawCopyStats();
		void OnEngineLevelLoadingSession( KeyValues *pEvent );
		bool ActivateBackgroundEffects();

		// Background movie playback
		bool			InitBackgroundMovie( void );
		void			CalculateMovieParameters( void );
		bool			RenderBackgroundMovie( float *pflFadeDelta );	// Render and update our BIK movie in the background
		void			ShutdownBackgroundMovie( void );

		BIKMaterial_t	m_BIKHandle;
		IMaterial		*m_pMovieMaterial;
		float			m_flU0, m_flV0, m_flU1, m_flV1;
		float			m_flMovieFadeInTime;					// Time to be fully faded in
		bool			m_bMovieFailed;

		static CBaseModPanel* m_CFactoryBasePanel;

		bool m_LevelLoading;
		vgui::HScheme m_UIScheme;
		int m_lastActiveUserId;

		vgui::HFont m_hDefaultFont;

		int	m_iBackgroundImageID;
		int	m_iFadeToBackgroundImageID;
		int m_iMovieTransitionImage;

		int m_DelayActivation;
		int m_ExitingFrameCount;
		bool m_bWarmRestartMode;
		bool m_bClosingAllWindows;

		float m_flBlurScale;
		float m_flLastBlurTime;

		CUtlString m_backgroundMusic;
		int m_nBackgroundMusicGUID;
		bool m_bFadeMusicUp;
	};
};

#endif
