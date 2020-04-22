//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "ivmodemanager.h"
#include "clientmode_hlnormal.h"
#include "panelmetaclassmgr.h"
#include "c_playerresource.h"
#include "c_portal_player.h"
#include "clientmode_portal.h"
#include "usermessages.h"
#include "prediction.h"
#include "portal2/vgui/portalclientscoreboard.h"
#include "portal2/vgui/surveypanel.h"
#include "hud_macros.h"
#include "radialmenu.h"
#include "glow_outline_effect.h"
#include "portal2/portal_grabcontroller_shared.h"
#include "viewpostprocess.h"
#include "radialmenu.h"
#include "fmtstr.h"
#include "ivieweffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// default FOV for HL2
ConVar default_fov( "default_fov", "75", FCVAR_CHEAT );
ConVar fov_desired( "fov_desired", "75", FCVAR_ARCHIVE | FCVAR_USERINFO, "Sets the base field-of-view.", true, 75.0, true, 90.0 );
extern ConVar hide_gun_when_holding;
extern ConVar v_viewmodel_fov;

ConVar cl_viewmodelfov( "cl_viewmodelfov", "50", FCVAR_DEVELOPMENTONLY );

ConVar cl_finale_completed( "cl_finale_completed", "0", FCVAR_HIDDEN );

#define DEATH_CC_LOOKUP_FILENAME "materials/correction/cc_death.raw"
#define DEATH_CC_FADE_SPEED 0.05f


// The current client mode. Always ClientModeNormal in HL.
static IClientMode *g_pClientMode[ MAX_SPLITSCREEN_PLAYERS ];
IClientMode *GetClientMode()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_pClientMode[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}
//extern EHANDLE g_eKillTarget1;
//extern EHANDLE g_eKillTarget2;

vgui::HScheme g_hVGuiCombineScheme = 0;

#define SCREEN_FILE		"scripts/vgui_screens.txt"

// Voice data
void VoxCallback( IConVar *var, const char *oldString, float oldFloat )
{
	if ( engine && engine->IsConnected() )
	{
		ConVarRef voice_vox( var->GetName() );
		if ( voice_vox.GetBool() /*&& voice_modenable.GetBool()*/ )
		{
			engine->ClientCmd( "voicerecord_toggle on\n" );
		}
		else
		{
			engine->ClientCmd( "voicerecord_toggle off\n" );
		}
	}
}
ConVar voice_vox( "voice_vox", "1", FCVAR_ARCHIVE, "Voice chat uses a vox-style always on", false, 0, true, 1, VoxCallback );

//--------------------------------------------------------------------------------------------------------
class CVoxManager : public CAutoGameSystem
{
public:
	CVoxManager() : CAutoGameSystem( "VoxManager" ) { }

	virtual void LevelInitPostEntity( void )
	{
		if ( voice_vox.GetBool() /*&& voice_modenable.GetBool()*/ )
		{
			engine->ClientCmd( "voicerecord_toggle on\n" );
		}
	}


	virtual void LevelShutdownPreEntity( void )
	{
		if ( voice_vox.GetBool() )
		{
			engine->ClientCmd( "voicerecord_toggle off\n" );
		}
	}
};


//--------------------------------------------------------------------------------------------------------
static CVoxManager s_VoxManager;

//-----------------------------------------------------------------------------
// Purpose: this is the viewport that contains all the hud elements
//-----------------------------------------------------------------------------
class CHudViewport : public CBaseViewport
{
private:
	DECLARE_CLASS_SIMPLE( CHudViewport, CBaseViewport );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		GetHud().InitColors( pScheme );

		SetPaintBackgroundEnabled( false );
	}

	virtual IViewPortPanel *CreatePanelByName( const char *szPanelName );
};

IViewPortPanel* CHudViewport::CreatePanelByName( const char *szPanelName )
{
	IViewPortPanel* newpanel = NULL;

	if ( Q_strcmp( PANEL_SCOREBOARD, szPanelName) == 0 )
	{
		newpanel = new CPortalClientScoreBoardDialog( this );
		return newpanel;
	}
	else if ( Q_strcmp( PANEL_SURVEY, szPanelName) == 0 )
	{
		newpanel = new CSurveyPanel( this );
		return newpanel;
	}
	/*else if ( Q_strcmp(PANEL_INFO, szPanelName) == 0 )
	{
		newpanel = new CHL2MPTextWindow( this );
		return newpanel;
	}*/

	return BaseClass::CreatePanelByName( szPanelName ); 
}


class CHLModeManager : public IVModeManager
{
public:
	CHLModeManager( void );
	virtual		~CHLModeManager( void );

	virtual void	Init( void );
	virtual void	SwitchMode( bool commander, bool force );
	virtual void	OverrideView( CViewSetup *pSetup );
	virtual void	CreateMove( float flInputSampleTime, CUserCmd *cmd );
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
};

CHLModeManager::CHLModeManager( void )
{
}

CHLModeManager::~CHLModeManager( void )
{
}

void CHLModeManager::Init( void )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		g_pClientMode[ i ] = GetClientModeNormal();
	}
	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( SCREEN_FILE );
}

void CHLModeManager::SwitchMode( bool commander, bool force )
{
}

void CHLModeManager::OverrideView( CViewSetup *pSetup )
{
}

void CHLModeManager::CreateMove( float flInputSampleTime, CUserCmd *cmd )
{
}

void CHLModeManager::LevelInit( const char *newmap )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetClientMode()->LevelInit( newmap );
	}

	if ( g_nKillCamMode > OBS_MODE_NONE )
	{
		g_bForceCLPredictOff = false;
	}

	g_nKillCamMode		= OBS_MODE_NONE;
}

void CHLModeManager::LevelShutdown( void )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetClientMode()->LevelShutdown();
	}
}

//--------------------------------------------------------------------------------------------------------
static void __MsgFunc_TransitionFade( bf_read &msg )
{
	float flFadeTime = msg.ReadFloat();
	GetClientModePortalNormal()->StartTransitionFade( flFadeTime );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModePortalNormal::ClientModePortalNormal()
{
	m_BlurFadeScale = 0;
	m_pRadialMenu = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: If you don't know what a destructor is by now, you are probably going to get fired
//-----------------------------------------------------------------------------
ClientModePortalNormal::~ClientModePortalNormal()
{
}

void ClientModePortalNormal::Init()
{
	BaseClass::Init();

	HOOK_MESSAGE( TransitionFade );

	InitRadialMenuHudElement();
}

void ClientModePortalNormal::InitViewport()
{
	m_pViewport = new CHudViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

CEG_NOINLINE void ClientModePortalNormal::LevelInit( const char *newmap )
{
	CEG_PROTECT_VIRTUAL_FUNCTION( ClientModePortalNormal_LevelInit );

	// Load color correction lookup for the death effect
	/*
	m_CCDeathHandle = g_pColorCorrectionMgr->AddColorCorrection( DEATH_CC_LOOKUP_FILENAME );
	if ( m_CCDeathHandle != INVALID_CLIENT_CCHANDLE )
	{
		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCDeathHandle, 0.0f );
	}
	m_flDeathCCWeight = 0.0f;
	*/

	m_hCurrentColorCorrection = NULL;
		
	m_BlurFadeScale = 0;

	return BaseClass::LevelInit( newmap );
}

void ClientModePortalNormal::LevelShutdown( void )
{
	// g_pColorCorrectionMgr->RemoveColorCorrection( m_CCDeathHandle );
	// m_CCDeathHandle = INVALID_CLIENT_CCHANDLE;
	return BaseClass::LevelShutdown();
}

void ClientModePortalNormal::OnColorCorrectionWeightsReset( void )
{
	BaseClass::OnColorCorrectionWeightsReset();

	C_Portal_Player *pPlayer = C_Portal_Player::GetLocalPortalPlayer();
	
	/*
	// if the player is dead, fade in the death color correction
	if ( m_CCDeathHandle != INVALID_CLIENT_CCHANDLE && ( pPlayer != NULL ) )
	{
		if ( pPlayer->m_lifeState != LIFE_ALIVE )
		{
			if ( m_flDeathCCWeight < 1.0f )
			{
				m_flDeathCCWeight += DEATH_CC_FADE_SPEED;
				clamp( m_flDeathCCWeight, 0.0f, 1.0f );
			}

			// Player is dead, fade out the environmental color correction
			if ( m_hCurrentColorCorrection )
			{
				m_hCurrentColorCorrection->EnableOnClient( false );
				m_hCurrentColorCorrection = NULL;
			}
		}
		else 
		{
			m_flDeathCCWeight = 0.0f;
		}

		g_pColorCorrectionMgr->SetColorCorrectionWeight( m_CCDeathHandle, m_flDeathCCWeight );
	}

	// Only blend between environmental color corrections if there is no death-induced color correction
	if ( m_flDeathCCWeight == 0.0f )
	*/
	{
		C_ColorCorrection *pNewColorCorrection = pPlayer ? pPlayer->GetActiveColorCorrection() : NULL;
		C_ColorCorrection *pOldColorCorrection = m_hCurrentColorCorrection;

		if ( pNewColorCorrection != pOldColorCorrection )
		{
			if ( pOldColorCorrection )
			{
				pOldColorCorrection->EnableOnClient( false );
			}
			if ( pNewColorCorrection )
			{
				pNewColorCorrection->EnableOnClient( true, pOldColorCorrection == NULL );
			}
			m_hCurrentColorCorrection = pNewColorCorrection;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModePortalNormal::ShouldDrawCrosshair( void )
{
	// If we're using the radial menu, don't show the crosshair
	// if ( IsRadialMenuOpen() )
	//	return false;

	// See if a player is holding 
	C_Portal_Player *pPlayer = C_Portal_Player::GetLocalPortalPlayer();
	if ( pPlayer && pPlayer->IsHoldingSomething() )
		return false;

	return BaseClass::ShouldDrawCrosshair();
}

void ClientModePortalNormal::SetBlurFade( float scale )
{
	m_BlurFadeScale = scale;
}

//--------------------------------------------------------------------------------------------------------
void ClientModePortalNormal::StartTransitionFade( float flFadeTime )
{
	STEAMWORKS_SELFCHECK(); 

	// Screen fade to black
	ScreenFade_t sf;
	memset( &sf, 0, sizeof( sf ) );
	sf.a = 255;
	sf.r = 0;
	sf.g = 0;
	sf.b = 0;
	sf.duration = (float)(1<<SCREENFADE_FRACBITS) * flFadeTime;
	sf.holdTime = 0.f;
	sf.fadeFlags = FFADE_OUT|FFADE_STAYOUT;
	GetViewEffects()->Fade( sf );

	// Sound fade to zero
	engine->ClientCmd( CFmtStr( "soundfade 100 5 %f %f\n", flFadeTime, flFadeTime ) );
}

extern ConVar building_cubemaps;
void ClientModePortalNormal::DoPostScreenSpaceEffects( const CViewSetup *pSetup )
{
	if ( building_cubemaps.GetBool() )
		return;

	MDLCACHE_CRITICAL_SECTION();

	g_GlowObjectManager.RenderGlowEffects( pSetup, GetSplitScreenPlayerSlot() );

	if ( m_BlurFadeScale )
	{
		CMatRenderContextPtr pRenderContext( materials );

		int xl, yl, dest_width, dest_height;
		pRenderContext->GetViewport( xl, yl, dest_width, dest_height );

		DoBlurFade( m_BlurFadeScale, 1.0f, xl, yl, dest_width, dest_height );
	}
}


void ClientModePortalNormal::InitRadialMenuHudElement( void )
{
	m_pRadialMenu = GET_HUDELEMENT( CRadialMenu );
	Assert( m_pRadialMenu );
}


int	ClientModePortalNormal::HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	/*if ( GetFullscreenClientMode() && GetFullscreenClientMode() != this &&
		!GetFullscreenClientMode()->HudElementKeyInput( down, keynum, pszCurrentBinding ) )
		return 0;*/

	if ( m_pRadialMenu )
	{
		if ( !m_pRadialMenu->KeyInput( down, keynum, pszCurrentBinding ) )
		{
			return 0;
		}
	}

	return 1;
}

// Override the viewmodelfov for splitscreen support.  This variable is set via the splitscreen_config.txt file.
// FIXME: Use this method for all games?  Are there other features that we need to support to use this?
float ClientModePortalNormal::GetViewModelFOV( void )
{
	return cl_viewmodelfov.GetFloat();
}

ClientModePortalNormal g_ClientModeNormal[ MAX_SPLITSCREEN_PLAYERS ];

IClientMode *GetClientModeNormal()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_ClientModeNormal[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

//--------------------------------------------------------------------------------------------------------
class FullscreenPortalViewport : public CHudViewport
{
private:
	DECLARE_CLASS_SIMPLE( FullscreenPortalViewport, CHudViewport );

private:
	virtual void InitViewportSingletons( void )
	{
		SetAsFullscreenViewportInterface();
	}
};

class ClientModePortalNormalFullscreen : public	ClientModePortalNormal
{
	DECLARE_CLASS_SIMPLE( ClientModePortalNormalFullscreen, ClientModePortalNormal );
public:
	virtual void InitViewport()
	{
		// Skip over BaseClass!!!
		BaseClass::ClientModeShared::InitViewport();
		m_pViewport = new FullscreenPortalViewport();
		m_pViewport->Start( gameuifuncs, gameeventmanager );
	}
};

//--------------------------------------------------------------------------------------------------------
static ClientModePortalNormalFullscreen g_FullscreenClientMode;
IClientMode *GetFullscreenClientMode( void )
{
	return &g_FullscreenClientMode;
}

ClientModePortalNormal* GetClientModePortalNormal()
{
	Assert( dynamic_cast< ClientModePortalNormal* >( GetClientModeNormal() ) );

	return static_cast< ClientModePortalNormal* >( GetClientModeNormal() );
}


static CHLModeManager g_HLModeManager;
IVModeManager *modemanager = &g_HLModeManager;

