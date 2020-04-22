//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: This file provides Client.dll's wrapper around the PuzzleMaker LIB/DLL
//
//==========================================================================//

#include "cbase.h"

#if defined( PORTAL2_PUZZLEMAKER )

#include "puzzlemaker/puzzlemaker.h"

#include "vgui_int.h"
#include "vgui/ivgui.h"
#include "vgui/isurface.h"
#include "ienginevgui.h"
#include "view_shared.h"
#include "rendertexture.h"
#include "glow_outline_effect.h"
#include "viewpostprocess.h"
#include "iclientmode.h"
#include "engine/ienginesound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//This offset is needed to make sure that the entire puzzlemaker vgui::frame gets mouse input
#define PUZZLEMAKER_WINDOW_SIZE_OFFSET 30


using namespace vgui;


//-----------------------------------------------------------------------------
// CPuzzleMakerGameSystem
//-----------------------------------------------------------------------------

class CPuzzleMakerGameSystem : public CBaseGameSystem
{
public:
	CPuzzleMakerGameSystem() : m_pFrame( NULL ) {}

 	virtual char const *Name( void ) { return "CPuzzleMakerGameSystem"; }

	virtual void PostInit( void )
	{
		// Provide global interface pointers to the PuzzleMaker DLL (this is ignored if we're statically linked)
		g_pPuzzleMaker->Connect(	g_pFullFileSystem, engine, materials, gpGlobals, g_pMDLCache, modelinfo, modelrender,
									g_pStudioRender, g_pVGuiSurface, g_pVGuiInput, g_pVGuiLocalize, g_pProcessUtils, g_pCVar, gameeventmanager );

		// Create the VGui panel (*after* ClientDLL_Init() is called - engine's vgui hasn't started up until then)
		// NOTE: m_pFrame is auto-deleted by its parent panel
		m_pFrame = new CPuzzleMakerFrame();
		g_pPuzzleMaker->PostInit( m_pFrame );
	}

	virtual void LevelInitPreEntity( void )			{ g_pPuzzleMaker->LevelInitPreEntity(); }
	virtual void LevelInitPostEntity( void )		{ g_pPuzzleMaker->LevelInitPostEntity(); }
	virtual void LevelShutdownPostEntity( void )	{ g_pPuzzleMaker->LevelShutdownPostEntity(); }
	virtual void Shutdown( void )					{ g_pPuzzleMaker->Shutdown(); }

private:
	CPuzzleMakerFrame *m_pFrame;
};

// CPuzzleMakerGameSystem Singleton
static CPuzzleMakerGameSystem s_PuzzleMakerGameSystem;
CBaseGameSystem* g_pPuzzleMakerGameSystem = &s_PuzzleMakerGameSystem;


//-----------------------------------------------------------------------------
// CPuzzleMakerFrame
//-----------------------------------------------------------------------------
CPuzzleMakerFrame::CPuzzleMakerFrame( void )
 : Frame( NULL, "PuzzleMaker" )
{
	// CPuzzleMakerFrame is parented to a new special-purpose engine panel,
	// which draws over the game, but under the menu/console:
	// NOTE: the engine panel will auto-delete this class

	// TODO-WIP: this is all very hacky and fragile, need someone with real VGui knowledge to stabilize this code...
	//  - SetParent fails because 'modulename' doesn't match between this panel and the engine panel ('BaseUI' versus 'ClientDLL')
	//  - SetPopup( false ) seems to be necessary to get this panel drawn ( MakePopup( false ) does not work ),
	//    even though the panel still ends up being treated as a popup (see MovePopupToBack below!)
	SetParent( enginevgui->GetPanel( PANEL_PUZZLEMAKER ) );
	ipanel()->SetPopup( this->GetVPanel(), false );

	// TODO-WIP: setup code copied from CPortal2EconUI
	SetMoveable( false );
	SetSizeable( false );
	SetCloseButtonVisible( false );
	//HScheme scheme = scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/ClientScheme.res", "ClientScheme");
	//SetScheme(scheme);
	SetProportional( true );

	SetScheme( "basemodui_scheme" );

	// TODO-WIP: setup code copied from PreparePanel (vgui_baseui_interface.cpp)
	SetPaintBorderEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetPaintEnabled( true );
	SetVisible( true );

	SetCursor( dc_arrow );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( true );

	// We do our own transition effect, so the default blur-crossfade is not necessary
	DisableFadeEffect();
}

CPuzzleMakerFrame::~CPuzzleMakerFrame()
{
	// NOTE: CPuzzleMakerFrame is auto-deleted by its parent engine panel
}

void CPuzzleMakerFrame::AdjustBounds( void )
{
	// The vgui::frame is surrounded by an invisible border that blocks all mouse input on it
	// We make the frame bigger than the actual screen so that the entire screen gets mouse input
	int nScreenWidth, nScreenHeight;
	g_pVGuiSurface->GetScreenSize( nScreenWidth, nScreenHeight );
	SetBounds( -PUZZLEMAKER_WINDOW_SIZE_OFFSET, -PUZZLEMAKER_WINDOW_SIZE_OFFSET, nScreenWidth + ( PUZZLEMAKER_WINDOW_SIZE_OFFSET * 2 ), nScreenHeight + ( PUZZLEMAKER_WINDOW_SIZE_OFFSET * 2 ) );
}

void CPuzzleMakerFrame::PerformLayout( void ) 
{
	AdjustBounds();
}

void CPuzzleMakerFrame::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	g_pPuzzleMaker->ApplySchemeSettings( pScheme );
}

void CPuzzleMakerFrame::OnCursorMoved( int x, int y )
{
	// Subtract the frame offset (convert from frame-relative to screen-relative coords):
	g_pPuzzleMaker->OnCursorMoved( x-PUZZLEMAKER_WINDOW_SIZE_OFFSET, y-PUZZLEMAKER_WINDOW_SIZE_OFFSET );
}

void CPuzzleMakerFrame::OnThink( void )
{
	if ( !IsVisible() )
		return;

	if ( !g_pPuzzleMaker->IsVisible() )
	{
		// Close the frame when the transition-out is complete (send a message to the parent frame to do this)
		ivgui()->PostMessage( enginevgui->GetPanel( PANEL_PUZZLEMAKER ), new KeyValues( "EndTransitionOut" ), GetVPanel() );
	}

	// Make sure the GameUI can get mouse/keyboard focus:
	if ( enginevgui->IsGameUIVisible() )
		surface()->MovePopupToBack( GetVPanel() );
}


bool CPuzzleMakerFrame::ShadowMapPreRender( FlashlightState_t &flashlightState, CTextureReference &shadowDepthTexture, CTextureReference &shadowColorTexture, VMatrix &worldToShadow )
{
	// Get the state of the shadow camera (our flashlight state) from the puzzlemaker:
	if ( !g_pPuzzleMaker->GetDepthShadowState( flashlightState ) )
	{
		// If the puzzlemaker says no shadows - there's no shadows.
		return false;
	}

	// Compute the projection matrix for ShaderAPI:
	g_pClientShadowMgr->ComputeFlashlightMatrix( flashlightState, &worldToShadow );

	// Lock a depth buffer and get the dummy colour buffer to go with it:
	if ( !g_pClientShadowMgr->LockShadowDepthTextureEx( flashlightState, &shadowDepthTexture, &shadowColorTexture ) || !shadowDepthTexture.IsValid() )
	{
		AssertMsg( false, "CPuzzleMakerFrame::UpdateDepthShadowMap - could not lock shadow depth buffer!\n" );
		return false;
	}

	// Set texture resolution, now that we've locked the shadowmap:
	flashlightState.m_flShadowMapResolution = shadowDepthTexture->GetActualWidth();
	Assert( shadowDepthTexture->GetActualWidth() == shadowDepthTexture->GetActualHeight() );

	return true;
}


void CPuzzleMakerFrame::Paint( void )
{
	VMatrix worldToShadow;
	FlashlightState_t flashlightState;
	CTextureReference shadowDepthTexture;
	CTextureReference shadowColorTexture;
	static ConVarRef puzzlemaker_shadows( "puzzlemaker_shadows", false );

	// Copy the main view to the full-frame texture:
	g_pPuzzleMaker->UpdateSnapshot( IsGameConsole() ? GetFullFrameFrameBufferTexture( 1 ) : GetFullscreenTexture() );

	// Only draw the puzzlemaker if FrameUpdate says it is visible:
	if ( g_pPuzzleMaker->FrameUpdate() )
	{
		// If we're doing shadows, set up the shadowmap for the puzzlemaker:
		if ( puzzlemaker_shadows.GetBool() && ShadowMapPreRender( flashlightState, shadowDepthTexture, shadowColorTexture, worldToShadow ) )
		{
			// Render with shadows
			g_pPuzzleMaker->RenderPuzzleMaker( &flashlightState, &shadowDepthTexture, &shadowColorTexture, &worldToShadow );

			// Unlock the shadowmap, now we're done with it
			g_pClientShadowMgr->UnlockShadowDepthTextureEx( shadowDepthTexture );
		}
		else
		{
			// Render without shadows
			g_pPuzzleMaker->RenderPuzzleMaker();
		}

		// Render the glow outline effect for selected items:
		// TODO-WIP: many elements of CViewSetup are not initialized by the constructor!
		CViewSetup viewSetup;
		viewSetup.x = viewSetup.y = viewSetup.width = viewSetup.height = 0;
		g_pVGuiSurface->GetScreenSize( viewSetup.width, viewSetup.height );
		g_GlowObjectManager.RenderGlowEffects( &viewSetup, GLOW_FOR_ALL_SPLIT_SCREEN_SLOTS, true );


		// Splitscreen management for GetClientMode() [this Paint code can be called in wildly different contexts]
		int nSaveIndex = GET_ACTIVE_SPLITSCREEN_SLOT();
		bool bSaveResolvable = engine->IsLocalPlayerResolvable();
		engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, true );
		engine->SetActiveSplitScreenPlayerSlot( 0 );
		{
			float flBlurFade = GetClientMode()->GetBlurFade();
			if ( flBlurFade <= 0.0f )
			{
				// Render the UI elements (including localized text), after the glow:
				g_pPuzzleMaker->RenderPuzzleMakerUI( PUZZLEMAKER_WINDOW_SIZE_OFFSET, PUZZLEMAKER_WINDOW_SIZE_OFFSET );
			}
 			else
			{
				// Finally, apply blur if the in-game menu is showing on top of the puzzlemaker:
				DoBlurFade( flBlurFade, 1.0f, 0, 0, viewSetup.width, viewSetup.height );
			}
		}
		engine->SetActiveSplitScreenPlayerSlot( nSaveIndex );
		engine->SetLocalPlayerIsResolvable( __FILE__, __LINE__, bSaveResolvable );
	}
}


void CPuzzleMakerFrame::PrecacheSound( const char *pszSoundName )
{
	extern HSOUNDSCRIPTHASH PrecacheScriptSoundPuzzleMaker( const char *soundname );
	PrecacheScriptSoundPuzzleMaker( pszSoundName );
}


int CPuzzleMakerFrame::PlaySoundEffect( const char *pszSoundName )
{
	CLocalPlayerFilter filter;
	return CBaseEntity::EmitSound( filter, -1, pszSoundName );
}


void CPuzzleMakerFrame::StopSoundByGUID( int nGUID )
{
	enginesound->StopSoundByGuid( nGUID, true );
}


// Expose puzzlemaker_export, because it's useful for hardcore map authors:
CON_COMMAND_F( puzzlemaker_export, "puzzlemaker_export <name>  -  export the current puzzle as 'name.vmf'", FCVAR_CLIENTDLL )
{
	if ( args.ArgC() == 2 && g_pPuzzleMaker->GetActive() )
		g_pPuzzleMaker->ExportPuzzle_Dev( args.Arg(1) );
}

// Expose the shadows toggle because it affects perf and is useful for machine config
ConVar puzzlemaker_shadows( "puzzlemaker_shadows", "0", FCVAR_ARCHIVE, "Enable shadows in the Portal 2 Puzzle Maker" );


// TODO-WIP: Hide development convars/commands at the end of beta (avoid confusion for now):
#define PUZZLEMAKER_DEV 0
//#define PUZZLEMAKER_DEV FCVAR_DEVELOPMENTONLY

// PuzzleMaker convars & concommands:
CON_COMMAND_F( puzzlemaker_load_dev, "puzzlemaker_load_dev <name>  -  load the puzzle called 'name.p2c'", PUZZLEMAKER_DEV )
{
	if ( args.ArgC() == 2 )
		g_pPuzzleMaker->LoadPuzzle_Dev( args.Arg(1) );
}
CON_COMMAND_F( puzzlemaker_save_dev, "puzzlemaker_save_dev <name>  -  save the current puzzle as 'name.p2c'", PUZZLEMAKER_DEV )
{
	if ( args.ArgC() == 2 )
		g_pPuzzleMaker->SavePuzzle_Dev( args.Arg(1) );
}

CON_COMMAND_F( puzzlemaker_publish_dev, "puzzlemaker_publish_dev  -  compile the current puzzle and publish it to the Steam Workshop (via the standlone publishing tool)", PUZZLEMAKER_DEV )
{
	g_pPuzzleMaker->PublishPuzzle_Dev();
}

ConVar puzzlemaker_drawselectionmeshes( "puzzlemaker_drawselectionmeshes", "0", FCVAR_ARCHIVE|PUZZLEMAKER_DEV, "draw wireframe item selection meshes in red" );


CON_COMMAND_F( puzzlemaker_autosave_dev, "puzzlemaker_autosave_dev  -  autosaves the current puzzle as 'autosave.p2c'", PUZZLEMAKER_DEV )
{
	g_pPuzzleMaker->AutoSave_Dev();
}

void PuzzleMakerPlaySoundsChanged( IConVar *var, const char *pOldValue, float flOldValue );
ConVar puzzlemaker_play_sounds( "puzzlemaker_play_sounds", "1", FCVAR_ARCHIVE|PUZZLEMAKER_DEV, "sets if the puzzlemaker can play sounds or not", PuzzleMakerPlaySoundsChanged );
void PuzzleMakerPlaySoundsChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( puzzlemaker_play_sounds.GetBool() )
	{
		g_pPuzzleMaker->RestartSounds();
	}
	else
	{
		g_pPuzzleMaker->StopSounds();
	}
}

ConVar puzzlemaker_zoom_to_mouse( "puzzlemaker_zoom_to_mouse", "1", FCVAR_ARCHIVE, "0-zoom to center of screen, 1-zoom to mouse cursor (smart), 2-zoom to mouse cursor (simple)" );
ConVar puzzlemaker_enable_budget_bar( "puzzlemaker_enable_budget_bar", "0", FCVAR_ARCHIVE, "Shows/Hides the budget bar" );
ConVar puzzlemaker_show_budget_numbers( "puzzlemaker_show_budget_numbers", "0", FCVAR_ARCHIVE, "Shows the current values for all the different map limits." );
ConVar puzzlemaker_active( "puzzlemaker_active", "0", FCVAR_DEVELOPMENTONLY | FCVAR_HIDDEN, "Active state of the puzzlemaker" );

#endif // PORTAL2_PUZZLEMAKER
