//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"

#include "VLoadingProgress.h"
#include "EngineInterface.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ProgressBar.h"
#include "vgui/ISurface.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/Image.h"
#include "vgui_controls/ImagePanel.h"
#include "gameui_util.h"
#include "KeyValues.h"
#include "fmtstr.h"
#include "FileSystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

ConVar ui_loadingscreen_transition_time( "ui_loadingscreen_transition_time", "1.0", FCVAR_DEVELOPMENTONLY, "" );
ConVar ui_loadingscreen_fadein_time( "ui_loadingscreen_fadein_time", "1.0", FCVAR_DEVELOPMENTONLY, "" );
ConVar ui_loadingscreen_mintransition_time( "ui_loadingscreen_mintransition_time", "0.5", FCVAR_DEVELOPMENTONLY, "" );
ConVar ui_loadingscreen_autotransition_time( "ui_loadingscreen_autotransition_time", "5.0", FCVAR_DEVELOPMENTONLY, "" );

LoadingProgress::LoadingProgress(Panel *parent, const char *panelName ):
	BaseClass( parent, panelName, false, false, false )
{
	if ( IsPC() )
	{
		MakePopup( false );
	}

	SetDeleteSelfOnClose( true );
	SetProportional( true );

	m_pChapterInfo = NULL;

	m_flPeakProgress = 0.0f;

	m_pWorkingAnim = NULL;
	
	// purposely not pre-caching the poster images
	// as they do not appear in-game, and are 1MB each, we will demand load them and ALWAYS discard them
	m_textureID_LoadingDots = -1;

	m_bDrawBackground = false;
	m_bDrawProgress = false;
	m_bDrawSpinner = false;

	m_flLastEngineTime = 0;

	m_nCurrentImage = 0;
	m_nNextImage = 0;
	m_nTargetImage = 0;
	m_flTransitionStartTime = 0;
	m_flLastTransitionTime = 0;

	m_bUseAutoTransition = false;
	m_flAutoTransitionTime = 0;

	m_flFadeInStartTime = 0;

	m_nProgressX = 0;
	m_nProgressY = 0;
	m_nProgressNumDots = 0;
	m_nProgressDotGap = 0;
	m_nProgressDotWidth = 0;
	m_nProgressDotHeight = 0;

	// marked to indicate the controls exist
	m_bValid = false;

	MEM_ALLOC_CREDIT();	
}

LoadingProgress::~LoadingProgress()
{
	EvictImages();
}

void LoadingProgress::OnThink()
{
	// Need to call this periodically to collect sign in and sign out notifications,
	// do NOT dispatch events here in the middle of loading and rendering!
	if ( ThreadInMainThread() )
	{
		XBX_ProcessEvents();
	}

	UpdateWorkingAnim();
}

void LoadingProgress::ApplySchemeSettings( IScheme *pScheme )
{
	// will cause the controls to be instanced
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( true );
	SetPostChildPaintEnabled( true );
	
	// now have controls, can now do further initing
	m_bValid = true;

	m_textureID_LoadingDots = CBaseModPanel::GetSingleton().GetImageId( "vgui/loadbar_dots" );

	vgui::Label *pLoadingProgress = dynamic_cast< vgui::Label* >( FindChildByName( "LoadingProgess" ) );
	if ( pLoadingProgress )
	{
		pLoadingProgress->GetPos( m_nProgressX, m_nProgressY );
	}

	m_nProgressNumDots = atoi( pScheme->GetResourceString( "LoadingProgress.NumDots" ) );
	m_nProgressDotGap = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "LoadingProgress.DotGap" ) ) );
	m_nProgressDotWidth = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "LoadingProgress.DotWidth" ) ) );
	m_nProgressDotHeight = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "LoadingProgress.DotHeight" ) ) );

	m_nProgressX = m_nProgressX - ( m_nProgressNumDots * m_nProgressDotWidth ) - ( m_nProgressNumDots - 1 ) * m_nProgressDotGap;
	m_nProgressY = m_nProgressY - m_nProgressDotHeight;

	SetupControlStates();
}

void LoadingProgress::Close()
{
	EvictImages();

	BaseClass::Close();
}

void LoadingProgress::EvictImages()
{
	// hint to force any of the expensive presentation images out of memory 
	for ( int i = 0; i < m_BackgroundImages.Count(); i++ )
	{
		if ( m_BackgroundImages[i].m_bOwnedByPanel )
		{
			vgui::surface()->DestroyTextureID( m_BackgroundImages[i].m_nTextureID );
		}
	}
	m_BackgroundImages.Purge();
}

void LoadingProgress::UpdateWorkingAnim()
{
	if ( m_pWorkingAnim && m_bDrawSpinner )
	{
		// clock the anim at 10hz
		float time = Plat_FloatTime();
		if ( ( m_flLastEngineTime + 0.1f ) < time )
		{
			m_flLastEngineTime = time;
			m_pWorkingAnim->SetFrame( m_pWorkingAnim->GetFrame() + 1 );
		}
	}
}

void LoadingProgress::SetProgress( float progress )
{
	if ( progress > m_flPeakProgress )
	{
		m_flPeakProgress = progress;
		if ( !m_bUseAutoTransition && m_BackgroundImages.Count() )
		{
			// sequence based on progress
			m_nTargetImage = m_flPeakProgress * m_BackgroundImages.Count();
			m_nTargetImage = clamp( m_nTargetImage, 0, m_BackgroundImages.Count() - 1 );
		}
	}
	
	UpdateWorkingAnim();
}

float LoadingProgress::GetProgress()
{
	return m_flPeakProgress;
}

void LoadingProgress::PaintBackground()
{
	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );

	if ( m_bDrawBackground && m_BackgroundImages.Count() )
	{
		surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		surface()->DrawSetTexture( m_BackgroundImages[m_nCurrentImage].m_nTextureID );
		surface()->DrawTexturedRect( 0, 0, screenWide, screenTall );
	
		if ( m_flTransitionStartTime )
		{
			float flLerp = RemapValClamped( Plat_FloatTime(), m_flTransitionStartTime, m_flTransitionStartTime + ui_loadingscreen_transition_time.GetFloat(), 0.0f, 1.0f );
			
			surface()->DrawSetColor( Color( 255, 255, 255, flLerp * 255.0f ) );
			surface()->DrawSetTexture( m_BackgroundImages[m_nNextImage].m_nTextureID );
			surface()->DrawTexturedRect( 0, 0, screenWide, screenTall );

			if ( flLerp >= 1.0f )
			{
				// terminate effect
				StopTransitionEffect();

				if ( !IsGameConsole() && !m_bUseAutoTransition && !m_flTransitionStartTime && m_nCurrentImage != m_nTargetImage )
				{
					// PC moves through loading too fast
					// hold on this image and restart the transitioning context until we catch up with the sequence
					m_nNextImage = m_nCurrentImage + 1;
					m_nNextImage = clamp( m_nNextImage, 0, m_BackgroundImages.Count() - 1 );
					m_flTransitionStartTime = Plat_FloatTime() + ui_loadingscreen_transition_time.GetFloat();
				}
			}
		}

		if ( m_bUseAutoTransition && !m_flTransitionStartTime )
		{
			// safe to auto transition to another image
			if ( m_flAutoTransitionTime && Plat_FloatTime() > m_flAutoTransitionTime + ui_loadingscreen_autotransition_time.GetFloat() )
			{
				m_nTargetImage = ( m_nTargetImage + 1 ) % m_BackgroundImages.Count();
				m_flAutoTransitionTime = Plat_FloatTime();
			}
		}
	}

	Panel *pWaitscreen = CBaseModPanel::GetSingleton().GetWindow( WT_GENERICWAITSCREEN );
	bool bRenderSpinner = m_bDrawSpinner && m_pWorkingAnim;
	if ( pWaitscreen && pWaitscreen->IsVisible() )
	{
		// Don't render spinner if the progress screen is displaying progress
		bRenderSpinner = false;
	}
	if ( bRenderSpinner )
	{
		int x, y, wide, tall;

		m_pWorkingAnim->GetBounds( x, y, wide, tall );
		m_pWorkingAnim->GetImage()->SetFrame( m_pWorkingAnim->GetFrame() );

		surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
		surface()->DrawSetTexture( m_pWorkingAnim->GetImage()->GetID() );
		surface()->DrawTexturedRect( x, y, x+wide, y+tall );
	}

	if ( m_bDrawProgress )
	{
		DrawLoadingBar();
	}
}

void LoadingProgress::PostChildPaint()
{
	BaseClass::PostChildPaint();

	if ( m_flFadeInStartTime )
	{
		int screenWide, screenTall;
		surface()->GetScreenSize( screenWide, screenTall );

		float flLerp = RemapValClamped( Plat_FloatTime(), m_flFadeInStartTime, m_flFadeInStartTime + ui_loadingscreen_fadein_time.GetFloat(), 1.0f, 0.0f );
		if ( flLerp == 0 )
		{
			m_flFadeInStartTime = 0;
		}

		surface()->DrawSetColor( Color( 0, 0, 0, flLerp * 255.0f ) );
		surface()->DrawFilledRect( 0, 0, screenWide, screenTall );
	}
}

void LoadingProgress::SetPosterData( KeyValues *pChapterInfo, const char *pszGameMode )
{
	m_pChapterInfo = pChapterInfo;
}

void LoadingProgress::SetupControlStates()
{
	m_flPeakProgress = 0.0f;

	if ( !m_bValid )
	{
		// haven't been functionally initialized yet
		// can't set or query control states until they get established
		return;
	}

	m_bDrawBackground = true;
	m_bDrawSpinner = true;
	m_bDrawProgress = true;
	m_bUseAutoTransition = false;

	const char *pCoopNetworkString = "";

	// set the correct background image

	CUtlString filenamePrefix;
	if ( m_pChapterInfo )
	{
		// need to know if we are transitioning from the main menu or map-to-map
		bool bTransitionFromMainMenu = ( GameUI().IsInLevel() == false );

		const char *pMapNameToLoad = m_pChapterInfo->GetString( "map" );
		const char *pActPrefix = V_stristr( pMapNameToLoad, "sp_a" );
		//Get the name of the current map
		//For cases where pMapNameToLoad doesn't get the entire path of the map
		//We need the entire path of the map to detect if we're loading a workshop or puzzlemaker map
		const char *pszCurrentMapName = engine->GetLevelNameShort();
		char szFixedCurrentMapName[MAX_PATH];
		V_strncpy( szFixedCurrentMapName, pszCurrentMapName, ARRAYSIZE( szFixedCurrentMapName ) );
		V_FixSlashes( szFixedCurrentMapName );
		if ( pActPrefix )
		{
			// single player
			// use the act transition when going from menu into any sp map due to length of load
			// otherwise use the alternate map-to-map presentation
			bool bUseActTransition = bTransitionFromMainMenu;
			int nAct = atoi( pActPrefix + 4 );

			// the presentation chosen for act 3 turned out to be a spoiler
			if ( V_stristr( pActPrefix, "sp_a3_00" ) ||  V_stristr( pActPrefix, "sp_a3_01" ) )
			{
				// use the prior act presentation for the first two maps
				nAct = 2;
			}
			else if ( V_stristr( pActPrefix, "sp_a3_03" ) )
			{
				// treat the third map as the true first map of the act
				nAct = 3;
				bUseActTransition = true;
			}
		
			if ( nAct && !bUseActTransition )
			{
				const char *pFirstMapOfAct = CBaseModPanel::GetSingleton().ActToMapName( nAct );
				if ( !V_stricmp( pMapNameToLoad, pFirstMapOfAct ) )
				{
					// use the act transition when going into the first map of the act
					bUseActTransition = true;
				}
			}

			if ( !bTransitionFromMainMenu && bUseActTransition )
			{
				const char *pLastMapName = m_pChapterInfo->GetString( "lastmap" );
				if ( !V_stricmp( pLastMapName, pMapNameToLoad ) )
				{
					// on death or re-loading the same map, use the default transition
					bUseActTransition = false;
				}
			}

			if ( V_stristr( pActPrefix, "sp_a5_credits" ) )
			{
				// the credits changed late, no time to do a matched act presentation
				// use the default transition
				bUseActTransition = false;
			}

			if ( !nAct || !bUseActTransition )
			{
				int nSubChapter = CBaseModPanel::GetSingleton().MapNameToSubChapter( pMapNameToLoad );
				if ( !nSubChapter )
				{
					// map not known
					nSubChapter = 1;
				}
				filenamePrefix = CFmtStr( "vgui/loading_screens/loadingscreen_default_%c", 'a' + nSubChapter - 1 );
			}
			else
			{
				filenamePrefix = CFmtStr( "vgui/loading_screens/loadingscreen_a%d", nAct );
			}
		}
		else if ( pMapNameToLoad && V_stristr( pMapNameToLoad, "e1912" ) )
		{
			m_bDrawSpinner = false;
			filenamePrefix = "vgui/loading_screens/loadingscreen_e1912";
		}
		else if ( ( pMapNameToLoad && V_stristr( pMapNameToLoad, "puzzlemaker/preview" ) ) ||
				  !V_strnicmp( szFixedCurrentMapName, "puzzlemaker\\", V_strlen( "puzzlemaker\\" ) ) ||
				  !V_strnicmp( szFixedCurrentMapName, "workshop\\", V_strlen( "workshop\\" ) ) )
		{
			filenamePrefix = "vgui/loading_screens/loadingscreen_default_b";
		}
		else
		{
			// coop
			// the special coop movies allowed in commentary mode aren't supposed to transition, but exit to main menu
			if ( !bTransitionFromMainMenu && !engine->IsInCommentaryMode() )
			{
				// coop has a modal movie player, none of this presentation is desired
				// prevent all the data load and rendering
				m_bDrawBackground = false;
				m_bDrawSpinner = false;
				m_bDrawProgress = false;
			}
			else
			{
				const char *pCoopPrefix = V_stristr( pMapNameToLoad, "mp_coop" );
				if ( pCoopPrefix )
				{
					filenamePrefix = "vgui/loading_screens/loadingscreen_coop";

					IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
					if ( pSession )
					{
						pCoopNetworkString = pSession->GetSessionSettings()->GetString( "system/network" );
					}
				}
			}
		}
	}

	bool bIsOnlineCoop = !V_stricmp( pCoopNetworkString, "live" ) || !V_stricmp( pCoopNetworkString, "lan" );

#ifdef _GAMECONSOLE
	if ( XBX_GetNumGameUsers() > 1 )
	{
		// HACK: Splitscreen games are always offline! This was a fake session for challenge mode!
		bIsOnlineCoop = false;
	}
#endif

	if ( m_bDrawBackground )
	{
		CUtlVector< CUtlString > imageNames;
		if ( filenamePrefix.IsEmpty() )
		{
			// unrecognized portal2 map, default to single product screen
			char startupImage[MAX_PATH];
			engine->GetStartupImage( startupImage, sizeof( startupImage ) );
			imageNames.AddToTail( startupImage );
		}
		else
		{
			const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();
			// determine image sequence
			CUtlString filename;
			while ( 1 )
			{
				int nImageIndex = imageNames.Count();
				filename = CFmtStr( "%s_%d%s", filenamePrefix.Get(), nImageIndex + 1, ( aspectRatioInfo.m_bIsWidescreen ? "_widescreen" : "" ) );
				if ( !g_pFullFileSystem->FileExists( CFmtStr( "materials/%s.vmt", filename.Get() ), "GAME" ) )
				{
					// end of list
					break;
				}
				imageNames.AddToTail( filename );
			}
		}

		// get all the images
		for ( int i = 0; i < imageNames.Count(); i++ )
		{
			BackgroundImage_t &image = m_BackgroundImages[m_BackgroundImages.AddToTail()];

			const char *pImageName = imageNames[i].Get();
			int nTextureID = vgui::surface()->DrawGetTextureId( pImageName );
			if ( nTextureID == -1 )
			{
				image.m_bOwnedByPanel = true;
				image.m_nTextureID = vgui::surface()->CreateNewTextureID();
				vgui::surface()->DrawSetTextureFile( image.m_nTextureID, pImageName, true, false );	
			}
			else
			{
				image.m_bOwnedByPanel = false;
				image.m_nTextureID = nTextureID;
			}
		}
	}

	m_pWorkingAnim = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "WorkingAnim" ) );
	if ( m_pWorkingAnim )
	{
		// we will custom draw
		m_pWorkingAnim->SetVisible( false );
	}

	// Used for the initial coop transition where the end of loading
	// is unknown (due to network unplugged) and therefore needs to transition
	// based on time and cycle
	m_bUseAutoTransition = bIsOnlineCoop;

	// Hide the employee badge by default
	ShowEmployeeBadge( false );

	if ( bIsOnlineCoop )
	{
		BASEMODPANEL_SINGLETON.SetupPartnerInScience();
		SetupPartnerInScience();
	}
#if defined( PORTAL2_PUZZLEMAKER )
	else if ( BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID() != 0 && BASEMODPANEL_SINGLETON.GetCommunityMapQueueMode() != QUEUEMODE_INVALID )
	{
		BASEMODPANEL_SINGLETON.SetupCommunityMapLoad();
		SetupCommunityMapLoad();
	}
#endif // PORTAL2_PUZZLEMAKER

	// Hold on to start frame slightly
	m_flLastEngineTime = Plat_FloatTime() + 0.2f;
	m_flLastTransitionTime = Plat_FloatTime();

	if ( m_bUseAutoTransition )
	{
		m_flAutoTransitionTime = Plat_FloatTime();
	}

	if ( m_bDrawBackground )
	{
		// run a transition to fade in
		m_flFadeInStartTime = Plat_FloatTime();
	}
}

void LoadingProgress::SetupPartnerInScience()
{
	vgui::ImagePanel *pPnlGamerPic = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );
	if ( pPnlGamerPic )
	{
		vgui::IImage *pAvatarImage = CBaseModPanel::GetSingleton().GetPartnerImage();
		if ( pAvatarImage )
		{
			pPnlGamerPic->SetImage( pAvatarImage );
		}
		else
		{
			pPnlGamerPic->SetImage( "icon_lobby" );
		}		

		pPnlGamerPic->SetVisible( true );
	}

	vgui::Label *pLblGamerTag = dynamic_cast< vgui::Label* >( FindChildByName( "LblGamerTag" ) );
	if ( pLblGamerTag )
	{
		CUtlString partnerName = CBaseModPanel::GetSingleton().GetPartnerName();
		pLblGamerTag->SetText( partnerName.Get() );
		pLblGamerTag->SetVisible( true );
	}

	vgui::Label *pLblGamerTagStatus = dynamic_cast< vgui::Label* >( FindChildByName( "LblGamerTagStatus" ) );
	if ( pLblGamerTagStatus )
	{
		pLblGamerTagStatus->SetVisible( true );
		pLblGamerTagStatus->SetText( CBaseModPanel::GetSingleton().GetPartnerDescKey() );
	}
}

void LoadingProgress::ShowEmployeeBadge( bool bState )
{
	vgui::ImagePanel *pBadgeBackground = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgEmployeeBadge" ) );
	if ( pBadgeBackground )
	{
		pBadgeBackground->SetVisible( bState );
	}
	
	vgui::ImagePanel *pBadgeOverlay = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgBadgeOverlay" ) );
	if ( pBadgeOverlay )
	{
		pBadgeOverlay->SetVisible( bState );
	}

	/*
	vgui::ImagePanel *pBadgeUpgrade = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgBadgeUpgrade" ) );
	if ( pBadgeUpgrade )
	{
		pBadgeUpgrade->SetVisible( bState );
	}
	*/
	
	vgui::ImagePanel *pBadgeLogo = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "ImgBadgeLogo" ) );
	if ( pBadgeLogo )
	{
		pBadgeLogo->SetVisible( bState );
	}
}

void LoadingProgress::SetupCommunityMapLoad()
{
	vgui::ImagePanel *pPnlGamerPic = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "PnlGamerPic" ) );
	if ( pPnlGamerPic )
	{
		vgui::IImage *pAvatarImage = BASEMODPANEL_SINGLETON.GetPartnerImage();
		if ( pAvatarImage )
		{
			pPnlGamerPic->SetImage( pAvatarImage );
			pPnlGamerPic->SetVisible( true );
		}
	}

#if defined( PORTAL2_PUZZLEMAKER )
	// Get the avatar for the author of this map
	const PublishedFileInfo_t *pInfo = WorkshopManager().GetPublishedFileInfoByID( BASEMODPANEL_SINGLETON.GetCurrentCommunityMapID() );
	if ( pInfo )
	{	
		// Setup the name of the author
		vgui::Label *pLblAuthorName = dynamic_cast< vgui::Label* >( FindChildByName( "LblMapTitleDesc" ) );
		if ( pLblAuthorName )
		{
			CSteamID steamID( pInfo->m_ulSteamIDOwner, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
			wchar_t convertedString[MAX_PATH] = L"";

			const wchar_t *authorFormat = g_pVGuiLocalize->Find( "#PORTAL2_CommunityPuzzle_Author" );
			g_pVGuiLocalize->ConvertANSIToUnicode( steamapicontext->SteamFriends()->GetFriendPersonaName( steamID ), convertedString, sizeof( convertedString ) );
			if ( authorFormat )
			{
				wchar_t	authorSteamNameString[256];
				g_pVGuiLocalize->ConstructString( authorSteamNameString, ARRAYSIZE( authorSteamNameString ), authorFormat, 1, convertedString );
				pLblAuthorName->SetText( authorSteamNameString );
				pLblAuthorName->SetVisible( true );
			}
			
		}
	
		// Setup the name of the map
		vgui::Label *pLblMapName = dynamic_cast< vgui::Label* >( FindChildByName( "LblMapTitle" ) );
		if ( pLblMapName )
		{
			wchar_t mapName[k_cchPublishedDocumentTitleMax];
			g_pVGuiLocalize->ConvertANSIToUnicode( pInfo->m_rgchTitle, mapName, sizeof( mapName ) );

			pLblMapName->SetVisible( true );
			pLblMapName->SetText( mapName );
		}

		ShowEmployeeBadge( true );
	}
#endif // PORTAL2_PUZZLEMAKER
}

bool LoadingProgress::LoadingProgressWantsIsolatedRender( bool bContextValid )
{
	bool bWantsTransition = ( m_nTargetImage != m_nCurrentImage );
	if ( IsGameConsole() && bWantsTransition && Plat_FloatTime() < m_flLastTransitionTime + ui_loadingscreen_mintransition_time.GetFloat() )
	{
		// transitions were meant for slow loading only,
		// post shipping consoles, now decided the PC will always do them, regardless of loading speed
		bWantsTransition = false;
	}

	if ( bWantsTransition && bContextValid && !m_flTransitionStartTime )
	{
		// caller has given us an isolated rendering context
		// run the effect
		m_flTransitionStartTime = Plat_FloatTime();

		if ( m_bUseAutoTransition )
		{
			// forever cycles
			m_nNextImage = m_nTargetImage;
		}
		else
		{
			// we want to move through the images regardless of how fast the progress is
			// and hold on the last image
			m_nNextImage = m_nCurrentImage + 1;
			m_nNextImage = clamp( m_nNextImage, 0, m_BackgroundImages.Count() - 1 );
		}
	}
	else if ( !bContextValid && m_flTransitionStartTime )
	{
		// caller has ended isolated rendering
		// abort the effect
		StopTransitionEffect();
		bWantsTransition = false;
	}

	if ( m_flFadeInStartTime )
	{
		// allow the isolated render so fade up is smooth
		bWantsTransition = true;
	}

	return bWantsTransition;
}

void LoadingProgress::StopTransitionEffect()
{
	m_nCurrentImage = m_nNextImage;
	m_flTransitionStartTime = 0;
	m_flLastTransitionTime = Plat_FloatTime();
}

void LoadingProgress::DrawLoadingBar()
{
	float flProgress = clamp( m_flPeakProgress, 0, 1.0f );
	if ( flProgress >= 0.97f )
	{
		// ensure we show the full row
		flProgress = 1.0;
	}

	surface()->DrawSetTexture( m_textureID_LoadingDots );

	float flWhiteDotS0 = 0.0f/64.0f;
	float flBlueDotS0 = 16.0f/64.0f;
	float flYellowDotS0 = 32.0f/64.0f;
	float flDotWidth = 16.0f/64.0f;

	DrawTexturedRectParms_t params;
	params.y0 = m_nProgressY;
	params.y1 = params.y0 + m_nProgressDotHeight;

	// background
	surface()->DrawSetColor( Color( 255, 255, 255, IsX360() ? 20 : 80 ) );
	int dotX = m_nProgressX;
	for ( int i = 0; i < m_nProgressNumDots; i++ )
	{
		params.x0 = dotX;
		params.x1 = params.x0 + m_nProgressDotWidth;
		params.s0 = flWhiteDotS0;
		params.s1 = params.s0 + flDotWidth;
		surface()->DrawTexturedRectEx( &params );
		dotX += m_nProgressDotWidth + m_nProgressDotGap;
	}

	// foreground
	int nProgressDots = flProgress * m_nProgressNumDots;
	surface()->DrawSetColor( Color( 255, 255, 255, 255 ) );
	dotX = m_nProgressX;
	for ( int i = 0; i < nProgressDots; i++ )
	{
		params.x0 = dotX;
		params.x1 = params.x0 + m_nProgressDotWidth;
		params.s0 = ( nProgressDots == m_nProgressNumDots ) ? flYellowDotS0 : flBlueDotS0;
		params.s1 = params.s0 + flDotWidth;
		surface()->DrawTexturedRectEx( &params );
		dotX += m_nProgressDotWidth + m_nProgressDotGap;
	}
}


