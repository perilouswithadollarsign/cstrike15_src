//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Methods associated with the cursor
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//


#if !defined( _X360 )
#define OEMRESOURCE //for OCR_* cursor junk
#include "winlite.h"
#endif

#include "tier0/dbg.h"
#include "tier1/utldict.h"
#include "Cursor.h"
#include "vguimatsurface.h"
#include "filesystem.h"

#if defined( PLATFORM_OSX )
#include <Carbon/Carbon.h>
#endif

#include <appframework/ilaunchermgr.h>

#if (USE_SDL)
#include "SDL.h"
#endif

#if defined( DX_TO_GL_ABSTRACTION ) 
#include "materialsystem/imaterialsystem.h"
#endif

#include "inputsystem/iinputsystem.h"
#include "inputsystem/iinputstacksystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


#if defined( USE_SDL )
static SDL_Cursor *s_pDefaultCursor[ dc_last ];
static SDL_Cursor *s_hCurrentCursor = NULL;
static SDL_Cursor *s_hCurrentlySetCursor = NULL;
#elif defined( WIN32 )
static InputCursorHandle_t s_pDefaultCursor[20];
static InputCursorHandle_t s_hCurrentCursor = NULL;
#endif


static bool s_bCursorLocked = false; 
static bool s_bCursorVisible = true;


//-----------------------------------------------------------------------------
// Initializes cursors
//-----------------------------------------------------------------------------
void InitCursors()
{
	// load up all default cursors
#if defined( USE_SDL )

    s_pDefaultCursor[ dc_none ]     = NULL;
    s_pDefaultCursor[ dc_arrow ]    = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_ARROW );
    s_pDefaultCursor[ dc_ibeam ]    = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_IBEAM );
    s_pDefaultCursor[ dc_hourglass ]= SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_WAIT );
    s_pDefaultCursor[ dc_crosshair ]= SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_CROSSHAIR );
    s_pDefaultCursor[ dc_waitarrow ]= SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_WAITARROW );
    s_pDefaultCursor[ dc_sizenwse ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENWSE );
    s_pDefaultCursor[ dc_sizenesw ] = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENESW );
    s_pDefaultCursor[ dc_sizewe ]   = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEWE );
    s_pDefaultCursor[ dc_sizens ]   = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZENS );
    s_pDefaultCursor[ dc_sizeall ]  = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_SIZEALL );
    s_pDefaultCursor[ dc_no ]       = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_NO );
    s_pDefaultCursor[ dc_hand ]     = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_HAND );

	s_hCurrentCursor = s_pDefaultCursor[ dc_arrow ];

#elif defined( WIN32 )

	s_pDefaultCursor[dc_none]     = INPUT_CURSOR_HANDLE_INVALID;
	s_pDefaultCursor[dc_arrow]    = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_ARROW );
	s_pDefaultCursor[dc_ibeam]    = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_IBEAM );
	s_pDefaultCursor[dc_hourglass]= g_pInputSystem->GetStandardCursor( INPUT_CURSOR_HOURGLASS );
	s_pDefaultCursor[dc_crosshair]= g_pInputSystem->GetStandardCursor( INPUT_CURSOR_CROSSHAIR );
	s_pDefaultCursor[dc_waitarrow]= g_pInputSystem->GetStandardCursor( INPUT_CURSOR_WAITARROW );
	s_pDefaultCursor[dc_up]       = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_UP );
	s_pDefaultCursor[dc_sizenwse] = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_SIZE_NW_SE );
	s_pDefaultCursor[dc_sizenesw] = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_SIZE_NE_SW );
	s_pDefaultCursor[dc_sizewe]   = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_SIZE_W_E );
	s_pDefaultCursor[dc_sizens]   = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_SIZE_N_S );
	s_pDefaultCursor[dc_sizeall]  = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_SIZE_ALL );
	s_pDefaultCursor[dc_no]       = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_NO );
	s_pDefaultCursor[dc_hand]     = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_HAND );

	s_hCurrentCursor = s_pDefaultCursor[dc_arrow];

#endif

	s_bCursorLocked = false;
	s_bCursorVisible = true;
}



#define USER_CURSOR_MASK 0x80000000

#ifdef WIN32
//-----------------------------------------------------------------------------
// Purpose: Simple manager for user loaded windows cursors in vgui
//-----------------------------------------------------------------------------
class CUserCursorManager
{
public:
	void Shutdown();
	vgui::HCursor CreateCursorFromFile( char const *curOrAniFile, char const *pPathID );
	bool LookupCursor( vgui::HCursor cursor, InputCursorHandle_t& handle );
private:
	CUtlDict< InputCursorHandle_t, int >	m_UserCursors;
};

void CUserCursorManager::Shutdown()
{
	m_UserCursors.RemoveAll();
}

vgui::HCursor CUserCursorManager::CreateCursorFromFile( char const *curOrAniFile, char const *pPathID )
{
	char fn[ 512 ];
	Q_strncpy( fn, curOrAniFile, sizeof( fn ) );
	Q_strlower( fn );
	Q_FixSlashes( fn );
	
	int cursorIndex = m_UserCursors.Find( fn );
	if ( cursorIndex != m_UserCursors.InvalidIndex() )
	{
		return cursorIndex | USER_CURSOR_MASK;
	}

	InputCursorHandle_t newCursor = g_pInputSystem->LoadCursorFromFile( fn, pPathID );
	cursorIndex = m_UserCursors.Insert( fn, newCursor );
	return cursorIndex | USER_CURSOR_MASK;
}

bool CUserCursorManager::LookupCursor( vgui::HCursor cursor, InputCursorHandle_t& handle )
{
	if ( !( (int)cursor & USER_CURSOR_MASK ) )
	{
		handle = 0;
		return false;
	}

	int cursorIndex = (int)cursor & ~USER_CURSOR_MASK;
	if ( !m_UserCursors.IsValidIndex( cursorIndex ) )
	{
		handle = 0;
		return false;
	}

	handle = m_UserCursors[ cursorIndex ];
	return true;
}

static CUserCursorManager g_UserCursors;
#endif

vgui::HCursor Cursor_CreateCursorFromFile( char const *curOrAniFile, char const *pPathID )
{
#ifdef WIN32 
	return g_UserCursors.CreateCursorFromFile( curOrAniFile, pPathID );
#else
	return dc_user;
#endif
}


void Cursor_ClearUserCursors()
{
}

#ifdef OSX
static HCursor s_hCursor = dc_arrow;

#if defined( PLATFORM_64BITS )

// MCCLEANUP
OSStatus SetThemeCursor(ThemeCursor inCursor) 
{ 
	return OSStatus(0); 
}

#endif

#endif

//-----------------------------------------------------------------------------
// Selects a cursor
//-----------------------------------------------------------------------------

void CursorSelect( InputContextHandle_t hContext, HCursor hCursor )
{
	if ( s_bCursorLocked )
		return;

	static ConVarRef cv_vguipanel_active( "vgui_panel_active" );

#if defined( USE_SDL )
	switch (hCursor)
	{
		case dc_user:
		case dc_none:
		case dc_blank:
			// Make sure we have the latest blank cursor handle.
			//		s_pDefaultCursor[dc_none] = (Cursor) g_pLauncherMgr->GetBlankCursor();
			s_bCursorVisible = false;
			break;

		case dc_arrow:
		case dc_waitarrow:
		case dc_ibeam:
		case dc_hourglass:
		case dc_crosshair:
		case dc_up:
		case dc_sizenwse:
		case dc_sizenesw:
		case dc_sizewe:
		case dc_sizens:
		case dc_sizeall:
		case dc_no:
		case dc_hand:
			s_bCursorVisible = true;
			s_hCurrentCursor = s_pDefaultCursor[hCursor];
			break;

		default:
			s_bCursorVisible = false;  // we don't support custom cursors at the moment (but could, if necessary).
			Assert(0);
			break;
	}

	ActivateCurrentCursor( hContext );

#elif defined( WIN32 ) 

	// [jason] When the console window is raised, keep the cursor active even if the mouse focus is not on the console window.
	//	This makes it easier track where the cursor is on-screen when the user moves off of the console.
	if ( cv_vguipanel_active.GetBool() == true && hCursor == dc_none )
	{
		hCursor = dc_arrow;
	}

	s_bCursorVisible = true;
	switch ( hCursor )
	{
	case dc_user:
	case dc_none:
	case dc_blank:
		s_bCursorVisible = false;
		break;

	case dc_arrow:
	case dc_waitarrow:
	case dc_ibeam:
	case dc_hourglass:
	case dc_crosshair:
	case dc_up:
	case dc_sizenwse:
	case dc_sizenesw:
	case dc_sizewe:
	case dc_sizens:
	case dc_sizeall:
	case dc_no:
	case dc_hand:
		s_hCurrentCursor = s_pDefaultCursor[hCursor];
		break;

	default:
		{
			InputCursorHandle_t custom = 0;
#ifdef WIN32 
			if ( g_UserCursors.LookupCursor( hCursor, custom ) && custom != 0 )
			{
				s_hCurrentCursor = custom;
			}
			else
#endif // WIN32
			{
				s_bCursorVisible = false;
				Assert(0);
			}
		}
		break;
	}

	ActivateCurrentCursor( hContext );

	g_pInputSystem->SetMouseCursorVisible( s_bCursorVisible );
#elif defined( PLATFORM_OSX )
	// @wge: Copied from window's section above
	// [jason] When the console window is raised, keep the cursor active even if the mouse focus is not on the console window.
	//	This makes it easier track where the cursor is on-screen when the user moves off of the console.
	if ( cv_vguipanel_active.GetBool() == true && hCursor == dc_none )
	{
		if (!CommandLine()->FindParm("-keepmousehooked"))
		{
			CGAssociateMouseAndMouseCursorPosition( false );
			if ( CGCursorIsVisible() )
			{
				CGDisplayHideCursor(kCGDirectMainDisplay);

				CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
				int rx, ry, width, height;
				pRenderContext->GetViewport( rx, ry, width, height );
				CursorSetPos( NULL, width/2, height/2 ); // we are hiding the cursor so move it to the middle of our window
				
			}
		}
		s_bCursorVisible = false;
	}
	else
	{
		if (!CommandLine()->FindParm("-keepmousehooked"))
		{
			CGAssociateMouseAndMouseCursorPosition( true );
			if ( !CGCursorIsVisible() )
			{
			  CGDisplayShowCursor(kCGDirectMainDisplay);
			}
		}
		s_bCursorVisible = true;
	}
	
	s_hCursor = hCursor;
	s_bCursorVisible = true;
	switch (hCursor)
	{
		case dc_none:  
		case dc_user:
		case dc_blank: 
			s_bCursorVisible = false;

			break;
		case dc_arrow:

			SetThemeCursor( kThemeArrowCursor );
			break;
		case dc_ibeam:

			SetThemeCursor( kThemeIBeamCursor );
			break;
		case dc_hourglass:

			SetThemeCursor( kThemeSpinningCursor );
			break;
		case dc_waitarrow:

			SetThemeCursor( kThemeSpinningCursor );
			break;
		case dc_crosshair:

			SetThemeCursor( kThemeCrossCursor );
			break;
		case dc_up:

			SetThemeCursor( kThemeResizeUpCursor );
			break;
		case dc_sizenwse:

			SetThemeCursor( kThemeCountingUpAndDownHandCursor );
			break;
		case dc_sizenesw:

			SetThemeCursor( kThemeResizeUpDownCursor );
			break;
		case dc_sizewe:

			SetThemeCursor( kThemeResizeLeftRightCursor );
			break;
		case dc_sizens:

			SetThemeCursor( kThemeResizeUpDownCursor );
			break;
		case dc_sizeall:

			SetThemeCursor( kThemeContextualMenuArrowCursor );
			break;
		case dc_no:

			SetThemeCursor( kThemeNotAllowedCursor );
			break;
		case dc_hand:

			SetThemeCursor( kThemePointingHandCursor );
			break;
	};
	
	g_pInputSystem->SetMouseCursorVisible( s_bCursorVisible );
#elif defined( _PS3 )
#elif defined( LINUX )
#error
#else
#error
#endif
}


//-----------------------------------------------------------------------------
// Activates the current cursor
//-----------------------------------------------------------------------------
void ActivateCurrentCursor( InputContextHandle_t hContext )
{
	if (s_bCursorVisible)
	{
#if defined( USE_SDL )
		if (s_hCurrentlySetCursor != s_hCurrentCursor)
		{
			s_hCurrentlySetCursor = s_hCurrentCursor;
			g_pLauncherMgr->SetMouseCursor( s_hCurrentlySetCursor );
			g_pLauncherMgr->SetMouseVisible( true );
		}

#elif defined( WIN32 )
		g_pInputStackSystem->SetCursorIcon( hContext, s_hCurrentCursor );
#elif defined( OSX )
		if ( !CGCursorIsVisible() && !CommandLine()->FindParm("-keepmousehooked") )
		{
			CGDisplayShowCursor(kCGDirectMainDisplay);
		}
#else
#error
#endif
	}
	else
	{
#if defined( USE_SDL )
		if (s_hCurrentlySetCursor != s_pDefaultCursor[dc_none])
		{
			s_hCurrentlySetCursor = s_pDefaultCursor[dc_none];
			g_pLauncherMgr->SetMouseCursor( s_hCurrentlySetCursor );
			g_pLauncherMgr->SetMouseVisible( false );
		}
#elif defined( WIN32 )
		g_pInputStackSystem->SetCursorIcon( hContext, INPUT_CURSOR_HANDLE_INVALID );
#elif defined( OSX )
		if ( CGCursorIsVisible() && !CommandLine()->FindParm("-keepmousehooked") )
		{
			CGDisplayHideCursor(kCGDirectMainDisplay);
		}
#else
#error
#endif
	}
}


//-----------------------------------------------------------------------------
// Purpose: prevents vgui from changing the cursor
//-----------------------------------------------------------------------------
void LockCursor( InputContextHandle_t hContext, bool bEnable )
{
	s_bCursorLocked = bEnable;
	ActivateCurrentCursor( hContext );
}


//-----------------------------------------------------------------------------
// Purpose: unlocks the cursor state
//-----------------------------------------------------------------------------
bool IsCursorLocked()
{
	return s_bCursorLocked;
}


//-----------------------------------------------------------------------------
// handles mouse movement
//-----------------------------------------------------------------------------
void CursorSetPos( InputContextHandle_t hContext, int x, int y )
{
	Assert( hContext != INPUT_CONTEXT_HANDLE_INVALID );
#if defined( DX_TO_GL_ABSTRACTION )
	if ( s_bCursorVisible )
#endif
		g_pInputStackSystem->SetCursorPosition( hContext, x, y );
}

void CursorGetPos( InputContextHandle_t hContext, int &x, int &y )
{
	// Should I add GetCursorPosition to InputStackSystem?

	Assert( hContext != INPUT_CONTEXT_HANDLE_INVALID );
	g_pInputSystem->GetCursorPosition( &x, &y );
}


#ifdef OSX
void CursorRunFrame()
{
	static HCursor hCursorLast = dc_none;
	
	if ( hCursorLast == s_hCursor )
		return;
	
	hCursorLast = s_hCursor;
	
	if ( s_hCursor == dc_none || s_hCursor == dc_user || s_hCursor == dc_blank )
	{
		if (!CommandLine()->FindParm("-keepmousehooked"))
		{
			// @wge Removed. After this is called, all mouse coordinates will be locked (returning only delta). We need coordinates for Scaleform.
			//CGAssociateMouseAndMouseCursorPosition( false );
			if ( CGCursorIsVisible() )
				CGDisplayHideCursor(kCGDirectMainDisplay);
			
			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
			int rx, ry, width, height;
			pRenderContext->GetViewport( rx, ry, width, height );
			// we are hiding the cursor so move it to the middle of our window
			g_pInputSystem->SetCursorPosition( width/2, height/2 );
		}
		s_bCursorVisible = false;
	}
	else
	{
		if (!CommandLine()->FindParm("-keepmousehooked"))
		{
			// @wge Removed, see above comment.
			//CGAssociateMouseAndMouseCursorPosition( true );
			if ( !CGCursorIsVisible() )
			{
				CGDisplayShowCursor( kCGDirectMainDisplay );
			}
		}
		s_bCursorVisible = true;
	}	
}
#endif




