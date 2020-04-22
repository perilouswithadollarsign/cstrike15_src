//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: Implementation of the VGUI ISurface interface using the 
// material system to implement it
//
//=============================================================================//

#define SUPPORT_CUSTOM_FONT_FORMAT

#ifdef SUPPORT_CUSTOM_FONT_FORMAT
	#define _WIN32_WINNT 0x0500
#endif

#if defined( WIN32) && !defined( _X360 )
#include <windows.h>
#endif
#ifdef OSX
#include <Carbon/Carbon.h>
#endif
#ifdef LINUX
#include <fontconfig/fontconfig.h>
#endif

#if defined( USE_SDL ) || defined(OSX) 
#include <appframework/ilaunchermgr.h>
ILauncherMgr *g_pLauncherMgr = NULL;
#endif

#include "tier1/strtools.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "filesystem.h"
#include <vgui/vgui.h>
#include <color.h>
#include "shaderapi/ishaderapi.h"
#include "utlvector.h"
#include "Clip2D.h"
#include <vgui_controls/Panel.h>
#include <vgui/IInput.h>
#include <vgui/Point.h>
#include "bitmap/imageformat.h"
#include "vgui_surfacelib/texturedictionary.h"
#include "Cursor.h"
#include "Input.h"
#include <vgui/IHTML.h>
#include <vgui/IVGui.h>
#include "vgui_surfacelib/fontmanager.h"
#include "vgui_surfacelib/fonttexturecache.h"
#include "MatSystemSurface.h"
#include "inputsystem/iinputsystem.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISystem.h>
#include "icvar.h"
#include "mathlib/mathlib.h"
#include <vgui/ILocalize.h>
#include "mathlib/vmatrix.h"
#include <tier0/vprof.h>
#include "materialsystem/itexture.h"
#ifndef _PS3
#include <malloc.h>
#else
#include <wctype.h>
#endif
#include "../vgui2/src/VPanel.h"
#include <vgui/IInputInternal.h>
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif
#include "xbox/xboxstubs.h"

#pragma warning( disable : 4706 )

#include <vgui/IVguiMatInfo.h>
#include <vgui/IVguiMatInfoVar.h>
#include "materialsystem/imaterialvar.h"
#include "memorybitmap.h"

#include "valvefont.h"

#pragma warning( default : 4706 )

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( _GAMECONSOLE )
#define MODEL_PANEL_RT_NAME	"_rt_SmallFB0"
#else // _GAMECONSOLE
#define MODEL_PANEL_RT_NAME	"_rt_FullScreen"
#endif // !_GAMECONSOLE

#define VPANEL_NORMAL	((vgui::SurfacePlat *) NULL)
#define VPANEL_MINIMIZED ((vgui::SurfacePlat *) 0x00000001)

using namespace vgui;

static bool g_bSpewFocus = false;

class CVguiMatInfoVar : public IVguiMatInfoVar
{
public:
	CVguiMatInfoVar( IMaterialVar *pMaterialVar )
	{
		m_pMaterialVar = pMaterialVar;
	}

	// from IVguiMatInfoVar
	virtual int GetIntValue ( void ) const
	{
		return m_pMaterialVar->GetIntValue();
	}

	virtual void SetIntValue ( int val )
	{
		m_pMaterialVar->SetIntValue( val );
	}

private:
	IMaterialVar *m_pMaterialVar;
};

class CVguiMatInfo : public IVguiMatInfo
{
public:
	CVguiMatInfo( IMaterial *pMaterial )
	{
		m_pMaterial = pMaterial;
	}

	// from IVguiMatInfo
	virtual IVguiMatInfoVar* FindVarFactory( const char *varName, bool *found )
	{
		IMaterialVar *pMaterialVar = m_pMaterial->FindVar( varName, found );

		if ( pMaterialVar == NULL )
			return NULL;
		return new CVguiMatInfoVar( pMaterialVar );
	}

	virtual int GetNumAnimationFrames( void )
	{
		return m_pMaterial->GetNumAnimationFrames();
	}

private:
	IMaterial *m_pMaterial;
};


//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
vgui::IInputInternal		*g_pIInput;
static bool					g_bInDrawing;
static CFontTextureCache	g_FontTextureCache;

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
CMatSystemSurface g_MatSystemSurface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMatSystemSurface, ISurface, 
						VGUI_SURFACE_INTERFACE_VERSION, g_MatSystemSurface );

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMatSystemSurface, ISchemeSurface, 
						SCHEME_SURFACE_INTERFACE_VERSION, g_MatSystemSurface );

#ifdef LINUX
CUtlDict< CMatSystemSurface::font_entry, unsigned short > CMatSystemSurface::m_FontData;
#endif


//-----------------------------------------------------------------------------
// Make sure the panel is the same size as the viewport
//-----------------------------------------------------------------------------
CMatEmbeddedPanel::CMatEmbeddedPanel() : BaseClass( NULL, "MatSystemTopPanel" )
{
	SetPaintBackgroundEnabled( false );

#if defined( _X360 )
	SetPos( 0, 0 );
	SetSize( GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ) );
#endif
}

void CMatEmbeddedPanel::OnThink()
{
	int x, y, width, height;
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->GetViewport( x, y, width, height );
	SetSize( width, height );
	SetPos( x, y );
	Repaint();
}

VPANEL CMatEmbeddedPanel::IsWithinTraverse(int x, int y, bool traversePopups)
{
	VPANEL retval = BaseClass::IsWithinTraverse( x, y, traversePopups );
	if ( retval == GetVPanel() )
		return 0;
	return retval;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CMatSystemSurface::CMatSystemSurface() : m_pEmbeddedPanel(NULL), m_pWhite(NULL), m_ContextAbsPos( 0, 0, ContextAbsPos_t::Less )
{
	m_iBoundTexture = -1; 
	m_nCurrReferenceValue = 0;
	m_bIn3DPaintMode = false;
	m_bDrawingIn3DWorld = false;
	m_PlaySoundFunc = NULL;
	m_bInThink = false;
	m_bAllowJavaScript = false;
	m_bAppDrivesInput = false;
	m_nLastInputPollCount = 0;
	m_flApparentDepth = STEREO_INVALID;

	m_hCurrentFont = NULL;
	m_pRestrictedPanel = NULL;
	m_bRestrictedPanelOverrodeAppModalPanel = false;
	m_bEnableInput = false;

	m_bNeedsKeyboard = false;
	m_bNeedsMouse = false;
	m_bUsingTempFullScreenBufferMaterial = false;
	m_nFullScreenBufferMaterialId = -1;
	m_nFullScreenBufferMaterialIgnoreAlphaId = -1;
	m_hInputContext = INPUT_CONTEXT_HANDLE_INVALID;

	memset( m_WorkSpaceInsets, 0, sizeof( m_WorkSpaceInsets ) );
	m_nBatchedCharVertCount = 0;

	g_FontTextureCache.SetPrefix( "vgui" );
}

CMatSystemSurface::~CMatSystemSurface()
{
}


//-----------------------------------------------------------------------------
// Connect, disconnect...
//-----------------------------------------------------------------------------
bool CMatSystemSurface::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !g_pFullFileSystem )
	{
		Warning( "MatSystemSurface requires the file system to run!\n" );
		return false;
	}

	if ( !g_pMaterialSystem )
	{
		Warning( "MatSystemSurface requires the material system to run!\n" );
		return false;
	}

	if ( !g_pVGuiPanel )
	{
		Warning( "MatSystemSurface requires the vgui::IPanel system to run!\n" );
		return false;
	}

	g_pIInput = (IInputInternal *)factory( VGUI_INPUTINTERNAL_INTERFACE_VERSION, NULL );
	if ( !g_pIInput )
	{
		Warning( "MatSystemSurface requires the vgui::IInput system to run!\n" );
		return false;
	}

	if ( !g_pVGui )
	{
		Warning( "MatSystemSurface requires the vgui::IVGUI system to run!\n" );
		return false;
	}

	Assert( g_pVGuiSurface == this );

	// initialize vgui_control interfaces
	if ( !vgui::VGui_InitInterfacesList( "MATSURFACE", &factory, 1 ) )
		return false;

#if defined( USE_SDL )
    g_pLauncherMgr = (ILauncherMgr *)factory(  SDLMGR_INTERFACE_VERSION, NULL );
#elif defined( OSX )
    g_pLauncherMgr = (ILauncherMgr *)factory(  COCOAMGR_INTERFACE_VERSION, NULL );
#endif

	return true;	
}

void CMatSystemSurface::Disconnect()
{
	g_pIInput = NULL;
	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Access to other interfaces...
//-----------------------------------------------------------------------------
void *CMatSystemSurface::QueryInterface( const char *pInterfaceName )
{
	// We also implement the IMatSystemSurface interface
	if (!Q_strncmp(	pInterfaceName, MAT_SYSTEM_SURFACE_INTERFACE_VERSION, Q_strlen(MAT_SYSTEM_SURFACE_INTERFACE_VERSION) + 1))
		return (IMatSystemSurface*)this;

	// We also implement the IMatSystemSurface interface
	if (!Q_strncmp(	pInterfaceName, VGUI_SURFACE_INTERFACE_VERSION, Q_strlen(VGUI_SURFACE_INTERFACE_VERSION) + 1))
		return (vgui::ISurface*)this;

	// We also implement the ISchemeSurface interface
	if (!Q_strncmp(	pInterfaceName, SCHEME_SURFACE_INTERFACE_VERSION, Q_strlen(SCHEME_SURFACE_INTERFACE_VERSION) + 1))
		return (ISchemeSurface*)this;

	return BaseClass::QueryInterface( pInterfaceName );
}


//-----------------------------------------------------------------------------
// Get dependencies
//-----------------------------------------------------------------------------
static AppSystemInfo_t s_Dependencies[] =
{
	{ "localize" DLL_EXT_STRING,		LOCALIZE_INTERFACE_VERSION },
	{ "inputsystem" DLL_EXT_STRING,		INPUTSTACKSYSTEM_INTERFACE_VERSION },
	{ "materialsystem" DLL_EXT_STRING,	MATERIAL_SYSTEM_INTERFACE_VERSION },
	{ NULL, NULL }
};

const AppSystemInfo_t* CMatSystemSurface::GetDependencies()
{
	return s_Dependencies;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::InitFullScreenBuffer( const char *pszRenderTargetName )
{
	char pTemp[512];

	m_FullScreenBufferMaterial.Shutdown();
	m_FullScreenBufferMaterialIgnoreAlpha.Shutdown();

	// Set up a material with which to reference the final image for subsequent display using vgui
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", pszRenderTargetName );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$nofog", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	Q_snprintf( pTemp, sizeof(pTemp), "VGUI_3DPaint_FullScreen_%s", pszRenderTargetName );
	m_FullScreenBufferMaterial.Init( pTemp, TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_FullScreenBufferMaterial->Refresh();

	pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetString( "$basetexture", pszRenderTargetName );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	pVMTKeyValues->SetInt( "$nofog", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	Q_snprintf( pTemp, sizeof(pTemp), "VGUI_3DPaint_FullScreen_IgnoreAlpha_%s", pszRenderTargetName );
	m_FullScreenBufferMaterialIgnoreAlpha.Init( pTemp, TEXTURE_GROUP_OTHER, pVMTKeyValues );
	m_FullScreenBufferMaterialIgnoreAlpha->Refresh();

	if ( m_nFullScreenBufferMaterialId != -1 )
	{
		DestroyTextureID( m_nFullScreenBufferMaterialId );
	}
	m_nFullScreenBufferMaterialId = -1;

	if ( m_nFullScreenBufferMaterialIgnoreAlphaId != -1 )
	{
		DestroyTextureID( m_nFullScreenBufferMaterialIgnoreAlphaId );
	}
	m_nFullScreenBufferMaterialIgnoreAlphaId = -1;

	m_FullScreenBuffer.Shutdown();

	m_FullScreenBufferName = pszRenderTargetName;
}

//-----------------------------------------------------------------------------
// Initialization and shutdown...
//-----------------------------------------------------------------------------
InitReturnVal_t CMatSystemSurface::Init( void )
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;
	MathLib_Init( 2.2f,  2.2f, 0.0f, 2.0f, true, true, true, true );

	g_pLocalize->SetTextQuery( this );

	// Allocate a white material
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	m_pWhite.Init( "VGUI_White", TEXTURE_GROUP_OTHER, pVMTKeyValues );

	InitFullScreenBuffer( MODEL_PANEL_RT_NAME );

	m_DrawColor[0] = m_DrawColor[1] = m_DrawColor[2] = m_DrawColor[3] = 255;
	m_nTranslateX = m_nTranslateY = 0;
	EnableScissor( false );
	SetScissorRect( 0, 0, 100000, 100000 );
	m_flAlphaMultiplier = 1.0f;

	// By default, use the default embedded panel
	m_pDefaultEmbeddedPanel = new CMatEmbeddedPanel;
	SetEmbeddedPanel( m_pDefaultEmbeddedPanel->GetVPanel() );

	m_iBoundTexture = -1;

	// Initialize font info..
	m_pDrawTextPos[0] = m_pDrawTextPos[1] = 0;
	m_DrawTextColor[0] = m_DrawTextColor[1] = m_DrawTextColor[2] = m_DrawTextColor[3] = 255;

	m_bIn3DPaintMode = false;
	m_bDrawingIn3DWorld = false;
	m_PlaySoundFunc = NULL;

	// Input system
	EnableWindowsMessages( true );

	// Initialize cursors
	InitCursors();

	// fonts initialization
	char language[64];
	bool bValid = false;
	if ( IsPC() )
	{
		memset( language, 0, sizeof( language ) );
		if ( CommandLine()->CheckParm( "-language" ) )
		{
			Q_strncpy( language, CommandLine()->ParmValue( "-language", "english"), sizeof( language ) );
            bValid = true;
		}
		else
		{    
#ifdef PLATFORM_WINDOWS
          bValid = system()->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", language, sizeof(language)-1 );
#elif defined(OSX)
          static ConVarRef cl_language("cl_language");
          Q_strncpy( language, cl_language.GetString(), sizeof( language ) );
          bValid = true;
#endif
        }
	}
	else
	{
		Q_strncpy( language, XBX_GetLanguageString(), sizeof( language ) );
		bValid = true;
	}

	if ( bValid )
	{
		FontManager().SetLanguage( language );
	}
	else
	{
		FontManager().SetLanguage( "english" );
	}
#ifdef LINUX
	FontManager().SetFontDataHelper( &CMatSystemSurface::FontDataHelper );
#endif

	// font manager needs the file system and material system for bitmap fonts
	FontManager().SetInterfaces( g_pFullFileSystem, g_pMaterialSystem );

	g_bSpewFocus = CommandLine()->FindParm( "-vguifocus" ) ? true : false;

	return INIT_OK;
}


#if defined( ENABLE_HTMLWINDOW )
void CMatSystemSurface::PurgeHTMLWindows( void ) 
{
	// we need to delete these BEFORE we close our window down, as the browser is using it
	// if this DOESN'T run then it will crash when we close the main window
	for ( int i=0; i<GetHTMLWindowCount(); i++ )
	{
		HtmlWindow * RESTRICT htmlwindow = GetHTMLWindow(i);
		AssertMsg1( htmlwindow , "Tried to delete NULL HTMLWindow %d in CMatSystemSurface::Shutdown. This is probably important.", i );
		delete htmlwindow;
	}

	_htmlWindows.Purge();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::Shutdown( void )
{
	for ( int i = m_FileTypeImages.First(); i != m_FileTypeImages.InvalidIndex(); i = m_FileTypeImages.Next( i ) )
	{
		delete m_FileTypeImages[ i ];
	}
	m_FileTypeImages.RemoveAll();

	// Release all textures
	TextureDictionary()->DestroyAllTextures();
	m_iBoundTexture = -1;

	// Release the standard materials
	m_pWhite.Shutdown();
	m_FullScreenBufferMaterial.Shutdown();
	m_FullScreenBufferMaterialIgnoreAlpha.Shutdown();
	m_FullScreenBuffer.Shutdown();

#if defined( ENABLE_HTMLWINDOW )
	// we need to delete these BEFORE we close our window down, as the browser is using it
	// if this DOESN'T run then it will crash when we close the main window
	PurgeHTMLWindows();
#endif

	m_Titles.Purge();
	m_PaintStateStack.Purge();

#if defined( WIN32 ) && !defined( _X360 )

	HMODULE gdiModule = NULL;

#ifdef SUPPORT_CUSTOM_FONT_FORMAT
	// On custom font format Windows takes care of cleaning up the font when the process quits.
	// 4/4/2011, mikesart: Except we seem to occasionally bsod in Windows' cleanup code.
	//  Googling for vCleanupPrivateFonts and left4dead results in several hits, and I'm hitting
	//	it several times a day during process exit on my Win7 x64 machine. After talking to a
	//  developer in GDI at Microsoft, this sounds like a race condition bug that was fixed in Windows 7 SP1.
	//  and this workaround would fix the bug on !Win7 SP1 machines. Repro for me was to run
	//	rendersystemtest.exe multiple times in a row.
	for (int i = 0; i < m_CustomFontHandles.Count(); i++)
	{
		::RemoveFontMemResourceEx( m_CustomFontHandles[i] );
	}
	m_CustomFontHandles.RemoveAll();
#else
 	// release any custom font files
	// use newer function if possible
	gdiModule = ::LoadLibrary( "gdi32.dll" );
	typedef int (WINAPI *RemoveFontResourceExProc)(LPCTSTR, DWORD, PVOID);
	RemoveFontResourceExProc pRemoveFontResourceEx = NULL;
	if ( gdiModule )
	{
		pRemoveFontResourceEx = (RemoveFontResourceExProc)::GetProcAddress(gdiModule, "RemoveFontResourceExA");
	}

	for (int i = 0; i < m_CustomFontFileNames.Count(); i++)
 	{
		if (pRemoveFontResourceEx)
		{
			// dvs: Keep removing the font until we get an error back. After consulting with Microsoft, it appears
			// that RemoveFontResourceEx must sometimes be called multiple times to work. Doing this insures that
			// when we load the font next time we get the real font instead of Ariel.
			int nRetries = 0;
			while ( (*pRemoveFontResourceEx)(m_CustomFontFileNames[i].String(), 0x10, NULL) && ( nRetries < 10 ) )
			{
				nRetries++;
				Msg( "Removed font resource %s on attempt %d.\n", m_CustomFontFileNames[i].String(), nRetries );
			}
		}
		else
		{
			// dvs: Keep removing the font until we get an error back. After consulting with Microsoft, it appears
			// that RemoveFontResourceEx must sometimes be called multiple times to work. Doing this insures that
			// when we load the font next time we get the real font instead of Ariel.
			int nRetries = 0;
			while ( ::RemoveFontResource(m_CustomFontFileNames[i].String()) && ( nRetries < 10 ) )
			{
				nRetries++;
				Msg( "Removed font resource %s on attempt %d.\n", m_CustomFontFileNames[i].String(), nRetries );
			}
		}
 	}
#endif // SUPPORT_CUSTOM_FONT_FORMAT

#endif

 	m_CustomFontFileNames.RemoveAll();
	m_BitmapFontFileNames.RemoveAll();
	m_BitmapFontFileMapping.RemoveAll();

	Cursor_ClearUserCursors();

#if defined( WIN32 ) && !defined( _X360 )
	if ( gdiModule )
	{
		::FreeLibrary(gdiModule);
	}
#endif

	g_pLocalize->SetTextQuery( NULL );

	BaseClass::Shutdown();
}

void CMatSystemSurface::SetEmbeddedPanel(VPANEL pEmbeddedPanel)
{
	m_pEmbeddedPanel = pEmbeddedPanel;
	((VPanel *)pEmbeddedPanel)->Client()->RequestFocus(0);
}

//-----------------------------------------------------------------------------
// hierarchy root
//-----------------------------------------------------------------------------
VPANEL CMatSystemSurface::GetEmbeddedPanel()
{
	return m_pEmbeddedPanel;
}

void CMatSystemSurface::SetInputContext( InputContextHandle_t hContext )
{
	m_hInputContext = hContext;
	if ( m_hInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pInputStackSystem->EnableInputContext( m_hInputContext, m_bNeedsMouse );
	}
}


//-----------------------------------------------------------------------------
// Purpose: cap bits
// Warning: if you change this, make sure the SurfaceV28 wrapper above reports
//          the correct capabilities.
//-----------------------------------------------------------------------------
bool CMatSystemSurface::SupportsFontFeature( FontFeature_t feature )
{
	switch (feature)
	{
	case FONT_FEATURE_ANTIALIASED_FONTS:
	case FONT_FEATURE_DROPSHADOW_FONTS:
		return true;

	case FONT_FEATURE_OUTLINE_FONTS:
		if ( IsX360() )
			return false;
		return true;

	default:
		return false;
	};
}

//-----------------------------------------------------------------------------
// Purpose: cap bits
// Warning: if you change this, make sure the SurfaceV28 wrapper above reports
//          the correct capabilities.
//-----------------------------------------------------------------------------
bool CMatSystemSurface::SupportsFeature( SurfaceFeature_t feature )
{
	switch (feature)
	{
	case ISurface::ESCAPE_KEY:
		return true;

	case ISurface::ANTIALIASED_FONTS:
	case ISurface::DROPSHADOW_FONTS:
	case ISurface::OUTLINE_FONTS:
		return SupportsFontFeature( ( FontFeature_t )feature );

	case ISurface::OPENING_NEW_HTML_WINDOWS:
	case ISurface::FRAME_MINIMIZE_MAXIMIZE:
	default:
		return false;
	};
}

//-----------------------------------------------------------------------------
// Hook needed to Get input to work
//-----------------------------------------------------------------------------
void CMatSystemSurface::SetAppDrivesInput( bool bLetAppDriveInput )
{
	m_bAppDrivesInput = bLetAppDriveInput;
}

bool CMatSystemSurface::HandleInputEvent( const InputEvent_t &event )
{
	if ( !m_bEnableInput )
		return false;

	if ( !m_bAppDrivesInput )
	{
		g_pIInput->UpdateButtonState( event );
	}

	return InputHandleInputEvent( GetInputContext(), event );
}


//-----------------------------------------------------------------------------
// Draws a panel in 3D space. Assumes view + projection are already set up
// Also assumes the (x,y) coordinates of the panels are defined in 640xN coords
// (N isn't necessary 480 because the panel may not be 4x3)
// The width + height specified are the size of the panel in world coordinates
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawPanelIn3DSpace( vgui::VPANEL pRootPanel, const VMatrix &panelCenterToWorld, int pw, int ph, float sw, float sh )
{
	Assert( pRootPanel );

	// FIXME: When should such panels be solved?!?
	SolveTraverse( pRootPanel, false );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	// Force Z buffering to be on for all panels drawn...
	pRenderContext->OverrideDepthEnable( true, false );

	Assert(!m_bDrawingIn3DWorld);
	m_bDrawingIn3DWorld = true;

	StartDrawingIn3DSpace( panelCenterToWorld, pw, ph, sw, sh );

	((VPanel *)pRootPanel)->Client()->Repaint();
	((VPanel *)pRootPanel)->Client()->PaintTraverse(true, false);

	FinishDrawing();

	// Reset z buffering to normal state
	pRenderContext->OverrideDepthEnable( false, true ); 

	m_bDrawingIn3DWorld = false;
}


//-----------------------------------------------------------------------------
// Purpose: Setup rendering for vgui on a panel existing in 3D space
//-----------------------------------------------------------------------------
void CMatSystemSurface::StartDrawingIn3DSpace( const VMatrix &screenToWorld, int pw, int ph, float sw, float sh )
{
	g_bInDrawing = true;
	m_iBoundTexture = -1; 

	int px = 0;
	int py = 0;

	m_pSurfaceExtents[0] = px;
	m_pSurfaceExtents[1] = py;
	m_pSurfaceExtents[2] = px + pw;
	m_pSurfaceExtents[3] = py + ph;

	// In order for this to work, the model matrix must have its origin
	// at the upper left corner of the screen. We must also scale down the
	// rendering from pixel space to screen space. Let's construct a matrix
	// transforming from pixel coordinates (640xN) to screen coordinates
	// (wxh, with the origin at the upper left of the screen). Then we'll
	// concatenate it with the panelCenterToWorld to produce pixelToWorld transform
	VMatrix pixelToScreen;

	// First, scale it so that 0->pw transforms to 0->sw
	MatrixBuildScale( pixelToScreen, sw / pw, -sh / ph, 1.0f );

	// Construct pixelToWorld
	VMatrix pixelToWorld;
	MatrixMultiply( screenToWorld, pixelToScreen, pixelToWorld );

	// make sure there is no translation and rotation laying around
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( pixelToWorld );

	// These are only here so that FinishDrawing works...
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();

	// Always enable scissoring (translate to origin because of the glTranslatef call above..)
	EnableScissor( true );

	m_nTranslateX = 0;
	m_nTranslateY = 0;
	m_flAlphaMultiplier = 1.0f;
}


//-----------------------------------------------------------------------------
// Purpose: Setup ortho for vgui
//-----------------------------------------------------------------------------

// we may need to offset by 0.5 texels to account for the different in pixel vs. texel centers in dx7-9
// however, we do this fixup already when we set up the texture coordinates for all materials/fonts
// so in theory we shouldn't need to do any adjustments for setting up the screen
// HOWEVER, we must do the offset, else the driver will think the text is something that should
// be antialiased, so the text will look broken if antialiasing is turned on (usually forced on in the driver)
// TOGL Linux/Win now automatically accounts for the half pixel offset between D3D9 vs. GL, if we are using the old OSX togl lib then we need pixel offsets to be 0.0f
float g_flPixelOffsetX = 0.5f;
float g_flPixelOffsetY = 0.5f;

bool g_bCheckedCommandLine = false;

extern void ___stop___( void );
void CMatSystemSurface::StartDrawing( void )
{
	MAT_FUNC;

	if ( !g_bCheckedCommandLine )
	{
		g_bCheckedCommandLine = true;
		
		const char *pX = CommandLine()->ParmValue( "-pixel_offset_x", (const char*)NULL );
		if ( pX )
			g_flPixelOffsetX = atof( pX );

		const char *pY = CommandLine()->ParmValue( "-pixel_offset_y", (const char*)NULL );
		if ( pY )
			g_flPixelOffsetY = atof( pY );
	}

	g_bInDrawing = true;
	m_iBoundTexture = -1; 

	int x, y, width, height;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->GetViewport( x, y, width, height);

	m_pSurfaceExtents[0] = x;
	m_pSurfaceExtents[1] = y;
	m_pSurfaceExtents[2] = x + width;
	m_pSurfaceExtents[3] = y + height;

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->Scale( 1, -1, 1 );
	
	//___stop___();
	pRenderContext->Ortho( g_flPixelOffsetX, g_flPixelOffsetY, width + g_flPixelOffsetX, height + g_flPixelOffsetY, -1.0f, 1.0f ); 

	// make sure there is no translation and rotation laying around
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	// Always enable scissoring (translate to origin because of the glTranslatef call above..)
	EnableScissor( true );

	m_nTranslateX = 0;
	m_nTranslateY = 0;

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::FinishDrawing( void )
{
	MAT_FUNC;

	// We're done with scissoring
	EnableScissor( false );

	// Restore the matrices
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

  	Assert( g_bInDrawing );
	g_bInDrawing = false;
}


//-----------------------------------------------------------------------------
// frame
//-----------------------------------------------------------------------------
void CMatSystemSurface::RunFrame()
{
#ifdef OSX
	void CursorRunFrame();
	CursorRunFrame();
#endif

	int nPollCount = g_pInputSystem->GetPollCount();
	if ( m_nLastInputPollCount == nPollCount )
		return;

	// If this isn't true, we've lost input!
	if ( !m_bAppDrivesInput && ( m_nLastInputPollCount != nPollCount - 1 ) )
	{
		Assert( 0 );
		Warning( "Vgui is losing input messages! Call brian!\n" );
	}

	m_nLastInputPollCount = nPollCount;

	if ( m_bAppDrivesInput )
		return;

	// Generate all input messages
	int nEventCount = g_pInputSystem->GetEventCount();
	const InputEvent_t* pEvents = g_pInputSystem->GetEventData( );
	for ( int i = 0; i < nEventCount; ++i )
	{
		HandleInputEvent( pEvents[i] );
	}
}


//-----------------------------------------------------------------------------
// Sets up a particular painting state...
//-----------------------------------------------------------------------------
void CMatSystemSurface::SetupPaintState( const PaintState_t &paintState )
{
	m_nTranslateX = paintState.m_iTranslateX;
	m_nTranslateY = paintState.m_iTranslateY;
	SetScissorRect( paintState.m_iScissorLeft, paintState.m_iScissorTop, 
		paintState.m_iScissorRight, paintState.m_iScissorBottom );
}

//-----------------------------------------------------------------------------
// Indicates a particular panel is about to be rendered 
//-----------------------------------------------------------------------------
void CMatSystemSurface::PushMakeCurrent(VPANEL pPanel, bool useInSets)
{
	int inSets[4] = {0, 0, 0, 0};
	int absExtents[4];
	int clipRect[4];

	if (useInSets)
	{
		g_pVGuiPanel->GetInset(pPanel, inSets[0], inSets[1], inSets[2], inSets[3]);
	}

	g_pVGuiPanel->GetAbsPos(pPanel, absExtents[0], absExtents[1]);
	int wide, tall;
	g_pVGuiPanel->GetSize(pPanel, wide, tall);
	absExtents[2] = absExtents[0] + wide;
	absExtents[3] = absExtents[1] + tall;

	g_pVGuiPanel->GetClipRect(pPanel, clipRect[0], clipRect[1], clipRect[2], clipRect[3]);

	int i = m_PaintStateStack.AddToTail();
	PaintState_t &paintState = m_PaintStateStack[i];
	paintState.m_pPanel = pPanel;

	// Determine corrected top left origin
	paintState.m_iTranslateX = inSets[0] + absExtents[0] - m_pSurfaceExtents[0];	
	paintState.m_iTranslateY = inSets[1] + absExtents[1] - m_pSurfaceExtents[1];

	// Setup clipping rectangle for scissoring
	paintState.m_iScissorLeft	= clipRect[0] - m_pSurfaceExtents[0];
	paintState.m_iScissorTop	= clipRect[1] - m_pSurfaceExtents[1];
	paintState.m_iScissorRight	= clipRect[2] - m_pSurfaceExtents[0];
	paintState.m_iScissorBottom	= clipRect[3] - m_pSurfaceExtents[1];
	
	SetupPaintState( paintState );
}

void CMatSystemSurface::PopMakeCurrent(VPANEL pPanel)
{
	// draw any remaining text
	if ( m_nBatchedCharVertCount > 0 )
	{
		DrawFlushText();
	}

	int top = m_PaintStateStack.Count() - 1;

	// More pops that pushes?
	Assert( top >= 0 );

	// Didn't pop in reverse order of push?
	Assert( m_PaintStateStack[top].m_pPanel == pPanel );

	m_PaintStateStack.Remove(top);

	if (top > 0)
		SetupPaintState( m_PaintStateStack[top-1] );

//	m_iBoundTexture = -1; 
}


//-----------------------------------------------------------------------------
// Color Setting methods
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetColor(int r, int g, int b, int a)
{
  	Assert( g_bInDrawing );
	m_DrawColor[0]=(unsigned char)r;
	m_DrawColor[1]=(unsigned char)g;
	m_DrawColor[2]=(unsigned char)b;
	m_DrawColor[3]=(unsigned char)(a * m_flAlphaMultiplier);
}

void CMatSystemSurface::DrawSetColor(Color col)
{
  	Assert( g_bInDrawing );
	DrawSetColor(col[0], col[1], col[2], col[3]);
}

//-----------------------------------------------------------------------------
// nVidia Stereoscopic Support methods
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetApparentDepth( float flDepth )
{
	Assert( g_bInDrawing );

	if ( !materials->IsStereoSupported() )
	{
		return;
	}

	// Can only skip the DrawFlushText because we'll expect to pop the stack in a bit and we need to have pushed. Otherwise we'd
	// have to have a separate stack for whether we pushed here, which would be ugly.
	if ( flDepth != m_flApparentDepth )
	{
		// Have to flush text, otherwise it's drawn incorrectly.
		DrawFlushText();
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	if ( flDepth <= STEREO_INVALID )
	{
		flDepth = STEREO_NOOP;
	}

	// Scaling by all four coordinates will cause stereo objects to be drawn at the depth specified
	// but will not otherwise affect the location of the object because of the eventual perspective divide.
	VMatrix depthMatrix( flDepth, 0, 0, 0, 
						 0, flDepth, 0, 0,
						 0, 0, flDepth, 0,
						 0, 0, 0, flDepth );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( depthMatrix );

	m_flApparentDepth = flDepth;
}

void CMatSystemSurface::DrawClearApparentDepth()
{
	if ( !materials->IsStereoSupported() )
	{
		return;
	}

	// Have to flush text, otherwise it's drawn incorrectly.
	DrawFlushText();

	m_flApparentDepth = STEREO_NOOP;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
}


//-----------------------------------------------------------------------------
// material Setting methods 
//-----------------------------------------------------------------------------
void CMatSystemSurface::InternalSetMaterial( IMaterial *pMaterial )
{
	if (!pMaterial)
	{
		pMaterial = m_pWhite;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	m_pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );
}


//-----------------------------------------------------------------------------
// Helper method to initialize vertices (transforms them into screen space too)
//-----------------------------------------------------------------------------
void CMatSystemSurface::InitVertex( Vertex_t &vertex, int x, int y, float u, float v )
{
	vertex.m_Position.Init( x + m_nTranslateX, y + m_nTranslateY );
	vertex.m_TexCoord.Init( u, v );
}


//-----------------------------------------------------------------------------
// Draws a line!
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawTexturedLineInternal( const Vertex_t &a, const Vertex_t &b )
{
	MAT_FUNC;

	Assert( !m_bIn3DPaintMode );

	// Don't bother drawing fully transparent lines
	if( m_DrawColor[3] == 0 )
		return;

	Vertex_t verts[2] = { a, b };
	
	verts[0].m_Position.x += m_nTranslateX + g_flPixelOffsetX;
	verts[0].m_Position.y += m_nTranslateY + g_flPixelOffsetY;
	
	verts[1].m_Position.x += m_nTranslateX + g_flPixelOffsetX;
	verts[1].m_Position.y += m_nTranslateY + g_flPixelOffsetY;

	Vertex_t clippedVerts[2];

	if (!ClipLine( verts, clippedVerts ))
		return;

	meshBuilder.Begin( m_pMesh, MATERIAL_LINES, 1 );

	meshBuilder.Position3f( clippedVerts[0].m_Position.x, clippedVerts[0].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( m_DrawColor );
	meshBuilder.TexCoord2fv( 0, clippedVerts[0].m_TexCoord.Base() );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( clippedVerts[1].m_Position.x, clippedVerts[1].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( m_DrawColor );
	meshBuilder.TexCoord2fv( 0, clippedVerts[1].m_TexCoord.Base() );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.End();
	m_pMesh->Draw();
}

void CMatSystemSurface::DrawLine( int x0, int y0, int x1, int y1 )
{
	MAT_FUNC;

	Assert( g_bInDrawing );

	// Don't bother drawing fully transparent lines
	if( m_DrawColor[3] == 0 )
		return;

	Vertex_t verts[2];
	verts[0].Init( Vector2D( x0, y0 ), Vector2D( 0, 0 ) );
	verts[1].Init( Vector2D( x1, y1 ), Vector2D( 1, 1 ) );
	
	InternalSetMaterial( );
	DrawTexturedLineInternal( verts[0], verts[1] );
}


void CMatSystemSurface::DrawTexturedLine( const Vertex_t &a, const Vertex_t &b )
{
	IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial(m_iBoundTexture);
	InternalSetMaterial( pMaterial );
	DrawTexturedLineInternal( a, b );
}


//-----------------------------------------------------------------------------
// Draws a line!
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawPolyLine( int *px, int *py ,int n )
{
	MAT_FUNC;

	Assert( g_bInDrawing );

	Assert( !m_bIn3DPaintMode );

	// Don't bother drawing fully transparent lines
	if( m_DrawColor[3] == 0 )
		return;

	InternalSetMaterial( );
	meshBuilder.Begin( m_pMesh, MATERIAL_LINES, n );

	for ( int i = 0; i < n ; i++ )
	{
		int inext = ( i + 1 ) % n;

		Vertex_t verts[2];
		Vertex_t clippedVerts[2];
		
		int x0, y0, x1, y1;

		x0 = px[ i ];
		x1 = px[ inext ];
		y0 = py[ i ];
		y1 = py[ inext ];

		InitVertex( verts[0], x0, y0, 0, 0 );
		InitVertex( verts[1], x1, y1, 1, 1 );

		if (!ClipLine( verts, clippedVerts ))
			continue;

		meshBuilder.Position3f( clippedVerts[0].m_Position.x+ g_flPixelOffsetX, clippedVerts[0].m_Position.y + g_flPixelOffsetY, m_flZPos );
		meshBuilder.Color4ubv( m_DrawColor );
		meshBuilder.TexCoord2fv( 0, clippedVerts[0].m_TexCoord.Base() );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

		meshBuilder.Position3f( clippedVerts[1].m_Position.x+ g_flPixelOffsetX, clippedVerts[1].m_Position.y + g_flPixelOffsetY, m_flZPos );
		meshBuilder.Color4ubv( m_DrawColor );
		meshBuilder.TexCoord2fv( 0, clippedVerts[1].m_TexCoord.Base() );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
	}

	meshBuilder.End();
	m_pMesh->Draw();
}


void CMatSystemSurface::DrawTexturedPolyLine( const Vertex_t *p,int n )
{
	int iPrev = n - 1;
	for ( int i=0; i < n; i++ )
	{
		DrawTexturedLine( p[iPrev], p[i] );
		iPrev = i;
	}
}


//-----------------------------------------------------------------------------
// Draws a quad: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawQuad( const Vertex_t &ul, const Vertex_t &lr, unsigned char *pColor )
{
	Assert( !m_bIn3DPaintMode );

	if ( !m_pMesh )
		return;

	meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( ul.m_Position.x, ul.m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( pColor );
	meshBuilder.TexCoord2f( 0, ul.m_TexCoord.x, ul.m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( lr.m_Position.x, ul.m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( pColor );
	meshBuilder.TexCoord2f( 0, lr.m_TexCoord.x, ul.m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( lr.m_Position.x, lr.m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( pColor );
	meshBuilder.TexCoord2f( 0, lr.m_TexCoord.x, lr.m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( ul.m_Position.x, lr.m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( pColor );
	meshBuilder.TexCoord2f( 0, ul.m_TexCoord.x, lr.m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.End();
	m_pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Purpose: Draws an array of quads
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawQuadArray( int quadCount, Vertex_t *pVerts, unsigned char *pColor, bool bShouldClip )
{
	Assert( !m_bIn3DPaintMode );

	if ( !m_pMesh )
		return;

	vgui::Vertex_t ulc;
	vgui::Vertex_t lrc;
	vgui::Vertex_t *pulc;
	vgui::Vertex_t *plrc;

	int nMaxVertices, nMaxIndices;	
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->GetMaxToRender( m_pMesh, false, &nMaxVertices, &nMaxIndices );
	if ( !nMaxVertices || !nMaxIndices )
		return; // probably in alt-tab

	int nMaxQuads = nMaxVertices / 4;
	nMaxQuads = MIN( nMaxQuads, nMaxIndices / 6 );

	int nFirstQuad = 0; 
	int nQuadsRemaining = quadCount;

	while ( nQuadsRemaining > 0 )
	{
		quadCount = MIN( nQuadsRemaining, nMaxQuads );

		meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, quadCount );
		if ( bShouldClip )
		{
			for ( int q = 0; q < quadCount; ++q )
			{
				int i = q + nFirstQuad;
				PREFETCH360( &pVerts[ 2 * ( i + 1 ) ], 0 );

				if ( !ClipRect( pVerts[2*i], pVerts[2*i + 1], &ulc, &lrc ) )
				{
					continue;	
				}
				pulc = &ulc;
				plrc = &lrc;

				meshBuilder.Position3f( pulc->m_Position.x, pulc->m_Position.y, m_flZPos );
				meshBuilder.Color4ubv( pColor );
				meshBuilder.TexCoord2f( 0, pulc->m_TexCoord.x, pulc->m_TexCoord.y );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.Position3f( plrc->m_Position.x, pulc->m_Position.y, m_flZPos );
				meshBuilder.Color4ubv( pColor );
				meshBuilder.TexCoord2f( 0, plrc->m_TexCoord.x, pulc->m_TexCoord.y );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.Position3f( plrc->m_Position.x, plrc->m_Position.y, m_flZPos );
				meshBuilder.Color4ubv( pColor );
				meshBuilder.TexCoord2f( 0, plrc->m_TexCoord.x, plrc->m_TexCoord.y );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.Position3f( pulc->m_Position.x, plrc->m_Position.y, m_flZPos );
				meshBuilder.Color4ubv( pColor );
				meshBuilder.TexCoord2f( 0, pulc->m_TexCoord.x, plrc->m_TexCoord.y );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
			}
		}
		else
		{
			for ( int q = 0; q < quadCount; ++q )
			{
				int i = q + nFirstQuad;
				PREFETCH360( &pVerts[ 2 * ( i + 1 ) ], 0 );

				pulc = &pVerts[2*i];
				plrc = &pVerts[2*i + 1];

				meshBuilder.Position3f( pulc->m_Position.x, pulc->m_Position.y, m_flZPos );
				meshBuilder.Color4ubv( pColor );
				meshBuilder.TexCoord2f( 0, pulc->m_TexCoord.x, pulc->m_TexCoord.y );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.Position3f( plrc->m_Position.x, pulc->m_Position.y, m_flZPos );
				meshBuilder.Color4ubv( pColor );
				meshBuilder.TexCoord2f( 0, plrc->m_TexCoord.x, pulc->m_TexCoord.y );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.Position3f( plrc->m_Position.x, plrc->m_Position.y, m_flZPos );
				meshBuilder.Color4ubv( pColor );
				meshBuilder.TexCoord2f( 0, plrc->m_TexCoord.x, plrc->m_TexCoord.y );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.Position3f( pulc->m_Position.x, plrc->m_Position.y, m_flZPos );
				meshBuilder.Color4ubv( pColor );
				meshBuilder.TexCoord2f( 0, pulc->m_TexCoord.x, plrc->m_TexCoord.y );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
			}
		}

		meshBuilder.End();
		m_pMesh->Draw();

		nFirstQuad += quadCount;
		nQuadsRemaining -= quadCount;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draws a rectangle colored with the current drawcolor
//		using the white material
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawFilledRect( int x0, int y0, int x1, int y1 )
{
	MAT_FUNC;

	CMatRenderContextPtr prc( g_pMaterialSystem );

	Assert( g_bInDrawing );

	// Don't even bother drawing fully transparent junk
	if( m_DrawColor[3]==0 )
		return;

	Vertex_t rect[2];
	Vertex_t clippedRect[2];
	InitVertex( rect[0], x0, y0, 0, 0 );
	InitVertex( rect[1], x1, y1, 0, 0 );

	// Fully clipped?
	if ( !ClipRect(rect[0], rect[1], &clippedRect[0], &clippedRect[1]) )
		return;	
	
	InternalSetMaterial();
	DrawQuad( clippedRect[0], clippedRect[1], m_DrawColor );
}

//-----------------------------------------------------------------------------
// Purpose: Draws an array of rectangles colored with the current drawcolor
//		using the white material
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawFilledRectArray( IntRect *pRects, int numRects )
{
	MAT_FUNC;

	Assert( g_bInDrawing );

	// Don't even bother drawing fully transparent junk
	if( m_DrawColor[3]==0 )
		return;

	if ( !m_pMesh )
		return;

	InternalSetMaterial( );

	meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, numRects );

	for (int i = 0; i < numRects; ++i )
	{
		Vertex_t rect[2];
		Vertex_t clippedRect[2];
		InitVertex( rect[0], pRects[i].x0, pRects[i].y0, 0, 0 );
		InitVertex( rect[1], pRects[i].x1, pRects[i].y1, 0, 0 );
		
		ClipRect( rect[0], rect[1], &clippedRect[0], &clippedRect[1] );
	
		Vertex_t &ul = clippedRect[0];
		Vertex_t &lr = clippedRect[1];

		meshBuilder.Position3f( ul.m_Position.x, ul.m_Position.y, m_flZPos );
		meshBuilder.Color4ubv( m_DrawColor );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();

		meshBuilder.Position3f( lr.m_Position.x, ul.m_Position.y, m_flZPos );
		meshBuilder.Color4ubv( m_DrawColor );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();

		meshBuilder.Position3f( lr.m_Position.x, lr.m_Position.y, m_flZPos );
		meshBuilder.Color4ubv( m_DrawColor );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();

		meshBuilder.Position3f( ul.m_Position.x, lr.m_Position.y, m_flZPos );
		meshBuilder.Color4ubv( m_DrawColor );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();
	}

	meshBuilder.End();
	m_pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Draws a fade between the fadeStartPt and fadeEndPT with the current draw color oriented according to argument
//   Example: DrawFilledRectFastFade( 10, 10, 100, 20, 50, 60, 255, 128, true );  
//			  -this will draw 
//					a solid rect (10,10,50,20) //alpha 255
//					a solid rect (50,10,60,20) //alpha faded from 255 to 128
//					a solid rect (60,10,100,20) //alpha 128
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawFilledRectFastFade( int x0, int y0, int x1, int y1, int fadeStartPt, int fadeEndPt, unsigned int alpha0, unsigned int alpha1, bool bHorizontal )
{
	if( bHorizontal )
	{
		if( alpha0 )
		{
			DrawSetColor( m_DrawColor[0], m_DrawColor[1], m_DrawColor[2], alpha0 );
			DrawFilledRect( x0, y0, fadeStartPt, y1 );
		}
		DrawFilledRectFade( fadeStartPt, y0, fadeEndPt, y1, alpha0, alpha1, true );
		if( alpha1 )
		{
			DrawSetColor( m_DrawColor[0], m_DrawColor[1], m_DrawColor[2], alpha1 );
			DrawFilledRect( fadeEndPt, y0, x1, y1 );
		}
	}
	else
	{
		if( alpha0 )
		{
			DrawSetColor( m_DrawColor[0], m_DrawColor[1], m_DrawColor[2], alpha0 );
			DrawFilledRect( x0, y0, x1, fadeStartPt );
		}
		DrawFilledRectFade( x0, fadeStartPt, x1, fadeEndPt, alpha0, alpha1, false );
		if( alpha1 )
		{
			DrawSetColor( m_DrawColor[0], m_DrawColor[1], m_DrawColor[2], alpha1 );
			DrawFilledRect( x0, fadeEndPt, x1, y1 );
		}
	}
}

//-----------------------------------------------------------------------------
// Draws a fade with the current draw color oriented according to argument
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawFilledRectFade( int x0, int y0, int x1, int y1, unsigned int alpha0, unsigned int alpha1, bool bHorizontal )
{
	MAT_FUNC;

	Assert( g_bInDrawing );

	// Scale the desired alphas by the surface alpha
	float alphaScale = m_DrawColor[3] / 255.f;
	alpha0 *= alphaScale;
	alpha1 *= alphaScale;

	// Don't even bother drawing fully transparent junk
	if ( alpha0 == 0 && alpha1 == 0 )
		return;

	Vertex_t rect[2];
	Vertex_t clippedRect[2];
	InitVertex( rect[0], x0, y0, 0, 0 );
	InitVertex( rect[1], x1, y1, 0, 0 );

	// Fully clipped?
	if ( !ClipRect(rect[0], rect[1], &clippedRect[0], &clippedRect[1]) )
		return;	
	
	InternalSetMaterial();

	unsigned char colors[4][4] = {0};
	for ( int i=0; i<4; i++ )
	{
		// copy the rgb and leave the alpha at zero
		Q_memcpy( colors[i], m_DrawColor, 3 );
	}

	unsigned char nAlpha0 = (alpha0 & 0xFF);
	unsigned char nAlpha1 = (alpha1 & 0xFF);

	if ( bHorizontal )
	{
		// horizontal fade
		colors[0][3] = nAlpha0;
		colors[1][3] = nAlpha1;
		colors[2][3] = nAlpha1;
		colors[3][3] = nAlpha0;
	}
	else
	{
		// vertical fade
		colors[0][3] = nAlpha0;
		colors[1][3] = nAlpha0;
		colors[2][3] = nAlpha1;
		colors[3][3] = nAlpha1;
	}

	meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( clippedRect[0].m_Position.x, clippedRect[0].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( colors[0] );
	meshBuilder.TexCoord2f( 0, clippedRect[0].m_TexCoord.x, clippedRect[0].m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( clippedRect[1].m_Position.x, clippedRect[0].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( colors[1] );
	meshBuilder.TexCoord2f( 0, clippedRect[1].m_TexCoord.x, clippedRect[0].m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( clippedRect[1].m_Position.x, clippedRect[1].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( colors[2] );
	meshBuilder.TexCoord2f( 0, clippedRect[1].m_TexCoord.x, clippedRect[1].m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( clippedRect[0].m_Position.x, clippedRect[1].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( colors[3] );
	meshBuilder.TexCoord2f( 0, clippedRect[0].m_TexCoord.x, clippedRect[1].m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.End();
	m_pMesh->Draw();
}


void CMatSystemSurface::DrawTexturedSubRectGradient( int x0, int y0, int x1, int y1, float texs0, float text0, float texs1, float text1, Color colStart, Color colEnd, bool bHorizontal )
{
	MAT_FUNC;

	Assert( g_bInDrawing );

	// Scale the desired alphas by the surface alpha
	colStart[3] *= m_flAlphaMultiplier;
	colEnd[3] *= m_flAlphaMultiplier;

	// Don't even bother drawing fully transparent junk
	if ( colStart.a() == 0 && colEnd.a() == 0 )
		return;

	float s0, t0, s1, t1;
	TextureDictionary()->GetTextureTexCoords( m_iBoundTexture, s0, t0, s1, t1 );

	float ssize = s1 - s0;
	float tsize = t1 - t0;

	// Rescale tex values into range of s0 to s1 ,etc.
	texs0 = s0 + texs0 * ( ssize );
	texs1 = s0 + texs1 * ( ssize );
	text0 = t0 + text0 * ( tsize );
	text1 = t0 + text1 * ( tsize );

	Vertex_t rect[2];
	Vertex_t clippedRect[2];
	InitVertex( rect[0], x0, y0, texs0, text0 );
	InitVertex( rect[1], x1, y1, texs1, text1 );

	// Fully clipped?
	if ( !ClipRect(rect[0], rect[1], &clippedRect[0], &clippedRect[1]) )
		return;	

	IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial(m_iBoundTexture);
	InternalSetMaterial( pMaterial );
	
	unsigned char colors[4][4];
	if ( bHorizontal )
	{
		// horizontal fade
		Q_memcpy( colors[0], &colStart[0], sizeof( colors[0] ) );
		Q_memcpy( colors[3], &colStart[0], sizeof( colors[3] ) );

		Q_memcpy( colors[1], &colEnd[0], sizeof( colors[1] ) );
		Q_memcpy( colors[2], &colEnd[0], sizeof( colors[2] ) );
	}
	else
	{
		// vertical fade
		Q_memcpy( colors[0], &colStart[0], sizeof( colors[0] ) );
		Q_memcpy( colors[1], &colStart[0], sizeof( colors[1] ) );

		Q_memcpy( colors[2], &colEnd[0], sizeof( colors[2] ) );
		Q_memcpy( colors[3], &colEnd[0], sizeof( colors[3] ) );
	}

	meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( clippedRect[0].m_Position.x, clippedRect[0].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( colors[0] );
	meshBuilder.TexCoord2f( 0, clippedRect[0].m_TexCoord.x, clippedRect[0].m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( clippedRect[1].m_Position.x, clippedRect[0].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( colors[1] );
	meshBuilder.TexCoord2f( 0, clippedRect[1].m_TexCoord.x, clippedRect[0].m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( clippedRect[1].m_Position.x, clippedRect[1].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( colors[2] );
	meshBuilder.TexCoord2f( 0, clippedRect[1].m_TexCoord.x, clippedRect[1].m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( clippedRect[0].m_Position.x, clippedRect[1].m_Position.y, m_flZPos );
	meshBuilder.Color4ubv( colors[3] );
	meshBuilder.TexCoord2f( 0, clippedRect[0].m_TexCoord.x, clippedRect[1].m_TexCoord.y );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.End();
	m_pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Purpose: Draws an unfilled rectangle in the current drawcolor
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawOutlinedRect(int x0,int y0,int x1,int y1)
{		
	MAT_FUNC;

	// Don't even bother drawing fully transparent junk
	if ( m_DrawColor[3] == 0 )
		return;

	DrawFilledRect(x0,y0,x1,y0+1);     //top
	DrawFilledRect(x0,y1-1,x1,y1);	   //bottom
	DrawFilledRect(x0,y0+1,x0+1,y1-1); //left
	DrawFilledRect(x1-1,y0+1,x1,y1-1); //right
}


//-----------------------------------------------------------------------------
// Purpose: Draws an outlined circle in the current drawcolor
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawOutlinedCircle(int x, int y, int radius, int segments)
{
	MAT_FUNC;

	Assert( g_bInDrawing );

	Assert( !m_bIn3DPaintMode );

	// Don't even bother drawing fully transparent junk
	if( m_DrawColor[3]==0 )
		return;

	// NOTE: Gotta use lines instead of linelist or lineloop due to clipping
	InternalSetMaterial( );
	meshBuilder.Begin( m_pMesh, MATERIAL_LINES, segments );

	Vertex_t renderVertex[2];
	Vertex_t vertex[2];
	vertex[0].m_Position.Init( m_nTranslateX + x + radius, m_nTranslateY + y );
	vertex[0].m_TexCoord.Init( 1.0f, 0.5f );

	float invDelta = 2.0f * M_PI / segments;
	for ( int i = 1; i <= segments; ++i )
	{
		float flRadians = i * invDelta;
		float ca = cos( flRadians );
		float sa = sin( flRadians );
					 
		// Rotate it around the circle
		vertex[1].m_Position.x = m_nTranslateX + x + (radius * ca);
		vertex[1].m_Position.y = m_nTranslateY + y + (radius * sa);
		vertex[1].m_TexCoord.x = 0.5f * (ca + 1.0f);
		vertex[1].m_TexCoord.y = 0.5f * (sa + 1.0f);

		if (ClipLine( vertex, renderVertex ))
		{
			meshBuilder.Position3f( renderVertex[0].m_Position.x, renderVertex[0].m_Position.y, m_flZPos );
			meshBuilder.Color4ubv( m_DrawColor );
			meshBuilder.TexCoord2fv( 0, renderVertex[0].m_TexCoord.Base() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( renderVertex[1].m_Position.x, renderVertex[1].m_Position.y, m_flZPos );
			meshBuilder.Color4ubv( m_DrawColor );
			meshBuilder.TexCoord2fv( 0, renderVertex[1].m_TexCoord.Base() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
		}

		vertex[0].m_Position = vertex[1].m_Position;
		vertex[0].m_TexCoord = vertex[1].m_TexCoord;
	}

	meshBuilder.End();
	m_pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Loads a particular texture (material)
//-----------------------------------------------------------------------------
int CMatSystemSurface::CreateNewTextureID( bool procedural /*=false*/ )
{
	return TextureDictionary()->CreateTexture( procedural );
}

void CMatSystemSurface::DestroyTextureID( int id )
{
	TextureDictionary()->DestroyTexture( id );
}

bool CMatSystemSurface::DeleteTextureByID(int id)
{
	TextureDictionary()->DestroyTexture( id );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
//			*filename - 
//			maxlen - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMatSystemSurface::DrawGetTextureFile(int id, char *filename, int maxlen )
{
	if ( !TextureDictionary()->IsValidId( id ) )
		return false;

	IMaterial *texture = TextureDictionary()->GetTextureMaterial(id);
	if ( !texture )
		return false;

	Q_strncpy( filename, texture->GetName(), maxlen );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - texture id
// Output : returns IMaterial for the referenced texture
//-----------------------------------------------------------------------------
IVguiMatInfo *CMatSystemSurface::DrawGetTextureMatInfoFactory(int id)
{
	if ( !TextureDictionary()->IsValidId( id ) )
		return NULL;

	IMaterial *texture = TextureDictionary()->GetTextureMaterial(id);

	if ( texture == NULL )
		return NULL;

	return new CVguiMatInfo(texture);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
// Output : int
//-----------------------------------------------------------------------------
int CMatSystemSurface::DrawGetTextureId( char const *filename )
{
	return TextureDictionary()->FindTextureIdForTextureFile( filename );
}

//-----------------------------------------------------------------------------
// Associates a texture with a material file (also binds it)
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetTextureFile(int id, const char *pFileName, int hardwareFilter, bool forceReload /*= false*/)
{
	TextureDictionary()->BindTextureToFile( id, pFileName );
	DrawSetTexture( id );
}


//-----------------------------------------------------------------------------
// Associates a texture with a material file (also binds it)
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetTextureMaterial(int id, IMaterial *pMaterial)
{
	TextureDictionary()->BindTextureToMaterial( id, pMaterial );
	DrawSetTexture( id );
}

IMaterial *CMatSystemSurface::DrawGetTextureMaterial( int id )
{
	return TextureDictionary()->GetTextureMaterial( id );
}


void CMatSystemSurface::ReferenceProceduralMaterial( int id, int referenceId, IMaterial *pMaterial )
{
	TextureDictionary()->BindTextureToMaterialReference( id, referenceId, pMaterial );
}


//-----------------------------------------------------------------------------
// Binds a texture
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetTexture( int id )
{
	// if we're switching textures, flush any batched text
	if ( id != m_iBoundTexture )
	{
		DrawFlushText();
		m_iBoundTexture = id;

		if ( id == -1 )
		{
			// ensure we unbind current material that may go away
			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
			pRenderContext->Bind( m_pWhite );
		}
	}
}


//-----------------------------------------------------------------------------
// Returns texture size
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawGetTextureSize(int id, int &iWide, int &iTall)
{
	TextureDictionary()->GetTextureSize( id, iWide, iTall );
}


//-----------------------------------------------------------------------------
// Draws a textured rectangle
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawTexturedRect( int x0, int y0, int x1, int y1 )
{
	MAT_FUNC;

	Assert( g_bInDrawing );

	// Don't even bother drawing fully transparent junk
	if( m_DrawColor[3] == 0 )
		return;

	float s0, t0, s1, t1;
	TextureDictionary()->GetTextureTexCoords( m_iBoundTexture, s0, t0, s1, t1 );

	Vertex_t rect[2];
	Vertex_t clippedRect[2];
	InitVertex( rect[0], x0, y0, s0, t0 );
	InitVertex( rect[1], x1, y1, s1, t1 );

	// Fully clipped?
	if ( !ClipRect(rect[0], rect[1], &clippedRect[0], &clippedRect[1]) )
		return;	

	IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial(m_iBoundTexture);
	InternalSetMaterial( pMaterial );
	DrawQuad( clippedRect[0], clippedRect[1], m_DrawColor );
}

//-----------------------------------------------------------------------------
// Draws a textured rectangle
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawTexturedSubRect( int x0, int y0, int x1, int y1, float texs0, float text0, float texs1, float text1 )
{
	MAT_FUNC;

	Assert( g_bInDrawing );

	// Don't even bother drawing fully transparent junk
	if( m_DrawColor[3] == 0 )
		return;

	float s0, t0, s1, t1;
	TextureDictionary()->GetTextureTexCoords( m_iBoundTexture, s0, t0, s1, t1 );

	float ssize = s1 - s0;
	float tsize = t1 - t0;

	// Rescale tex values into range of s0 to s1 ,etc.
	texs0 = s0 + texs0 * ( ssize );
	texs1 = s0 + texs1 * ( ssize );
	text0 = t0 + text0 * ( tsize );
	text1 = t0 + text1 * ( tsize );

	Vertex_t rect[2];
	Vertex_t clippedRect[2];
	InitVertex( rect[0], x0, y0, texs0, text0 );
	InitVertex( rect[1], x1, y1, texs1, text1 );

	// Fully clipped?
	if ( !ClipRect(rect[0], rect[1], &clippedRect[0], &clippedRect[1]) )
		return;	

	IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial(m_iBoundTexture);
	InternalSetMaterial( pMaterial );
	DrawQuad( clippedRect[0], clippedRect[1], m_DrawColor );
}

//-----------------------------------------------------------------------------
// Draws a textured polygon
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawTexturedPolygon(int n, Vertex_t *pVertices, bool bClipVertices /*= true*/ )
{
	Assert( !m_bIn3DPaintMode );

	Assert( g_bInDrawing );

	// Don't even bother drawing fully transparent junk
	if( (n == 0) || (m_DrawColor[3]==0) )
		return;

	if ( bClipVertices )
	{
		int iCount;
		Vertex_t **ppClippedVerts = NULL;
		iCount = ClipPolygon( n, pVertices, m_nTranslateX, m_nTranslateY, &ppClippedVerts );
		if (iCount <= 0)
			return;

		IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial(m_iBoundTexture);
		InternalSetMaterial( pMaterial );

		meshBuilder.Begin( m_pMesh, MATERIAL_POLYGON, iCount );

		for (int i = 0; i < iCount; ++i)
		{
			meshBuilder.Position3f( ppClippedVerts[i]->m_Position.x, ppClippedVerts[i]->m_Position.y, m_flZPos );
			meshBuilder.Color4ubv( m_DrawColor );
			meshBuilder.TexCoord2fv( 0, ppClippedVerts[i]->m_TexCoord.Base() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
		}

		meshBuilder.End();
		m_pMesh->Draw();
	}
	else
	{
		IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial(m_iBoundTexture);
		InternalSetMaterial( pMaterial );

		meshBuilder.Begin( m_pMesh, MATERIAL_POLYGON, n );

		for (int i = 0; i < n; ++i)
		{
			meshBuilder.Position3f( pVertices[i].m_Position.x + m_nTranslateX, pVertices[i].m_Position.y + m_nTranslateY, m_flZPos );
			meshBuilder.Color4ubv( m_DrawColor );
			meshBuilder.TexCoord2fv( 0, pVertices[i].m_TexCoord.Base() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
		}

		meshBuilder.End();
		m_pMesh->Draw();
	}
}

void CMatSystemSurface::DrawWordBubble( int x0, int y0, int x1, int y1, int nBorderThickness, Color rgbaBackground, Color rgbaBorder, bool bPointer, int nPointerX, int nPointerY, int nPointerBaseThickness )
{
	int nOldClipX0, nOldClipY0, nOldClipX1, nOldClipY1;
	GetClipRect( nOldClipX0, nOldClipY0, nOldClipX1, nOldClipY1 );
	SetClipRect( INT16_MIN, INT16_MIN, INT16_MAX, INT16_MAX );

	int nBackgroundWide = x1 - x0;
	int nBackgroundTall = y1 - y0;

	DrawSetColor( rgbaBackground );
	DrawFilledRect( x0, y0, x1, y1 );

	DrawSetTexture( -1 );
	Vector2D vecZero = Vector2D( 0.0f, 0.0f );

	// Figure out the relative position of the thing we're pointing at
	if ( nPointerY >= y0 && nPointerY < y0 + nBackgroundTall )
	{
		// Pointer is pointing inside the bubble!
		bPointer = false;
	}

	int nHalfPointerBaseTopWide, nHalfPointerBaseBottomWide;

	if ( bPointer )
	{
		if ( nPointerY < y0 )
		{
			// Pointing at something above bubble!
			nHalfPointerBaseTopWide = nPointerBaseThickness / 2;
			nHalfPointerBaseBottomWide = nPointerBaseThickness;

			// Draw the up pointer from polygons
			vgui::Vertex_t pointerVerts[ 3 ] = 
			{
				vgui::Vertex_t( Vector2D( x0 + nHalfPointerBaseTopWide, y0 ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY ), vecZero ),
				vgui::Vertex_t( Vector2D( x0 + nPointerBaseThickness, y0 ), vecZero )
			};
			DrawTexturedPolygon( 3, pointerVerts );
		}
		else
		{
			// Pointing at something below bubble!
			nHalfPointerBaseTopWide = nPointerBaseThickness;
			nHalfPointerBaseBottomWide = nPointerBaseThickness / 2;

			// Draw the down pointer from polygons
			vgui::Vertex_t pointerVerts[ 3 ] = 
			{
				vgui::Vertex_t( Vector2D( x0 + nPointerBaseThickness, y0 + nBackgroundTall ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY ), vecZero ),
				vgui::Vertex_t( Vector2D( x0 + nHalfPointerBaseBottomWide, y0 + nBackgroundTall ), vecZero )
			};
			DrawTexturedPolygon( 3, pointerVerts );
		}
	}
	else
	{
		// No pointer so the top and bottom separations are both closed
		nHalfPointerBaseTopWide = nPointerBaseThickness;
		nHalfPointerBaseBottomWide = nPointerBaseThickness;
	}

	// Build a border out of polygons!
	DrawSetColor( rgbaBorder );

	DrawFilledRect( x0, y0 - nBorderThickness, x0 + nHalfPointerBaseTopWide, y0 );
	DrawFilledRect( x0 + nPointerBaseThickness, y0 - nBorderThickness, x0 + nBackgroundWide, y0 );
	DrawFilledRect( x0 - nBorderThickness, y0, x0, y0 + nBackgroundTall );
	DrawFilledRect( x0 + nBackgroundWide, y0, x0 + nBackgroundWide + nBorderThickness, y0 + nBackgroundTall );
	DrawFilledRect( x0, y0 + nBackgroundTall, x0 + nHalfPointerBaseBottomWide, y0 + nBackgroundTall + nBorderThickness );
	DrawFilledRect( x0 + nPointerBaseThickness, y0 + nBackgroundTall, x0 + nBackgroundWide, y0 + nBackgroundTall + nBorderThickness );

	const int nNumCornerTris = 4;
	vgui::Vertex_t cornerVerts[ nNumCornerTris * 3 ] = 
	{
		// Corner TL
		vgui::Vertex_t( Vector2D( x0, y0 - nBorderThickness ), vecZero ),
		vgui::Vertex_t( Vector2D( x0, y0 ), vecZero ),
		vgui::Vertex_t( Vector2D( x0 - nBorderThickness, y0 ), vecZero ),

		// Corner TR
		vgui::Vertex_t( Vector2D( x0 + nBackgroundWide, y0 - nBorderThickness ), vecZero ),
		vgui::Vertex_t( Vector2D( x0 + nBackgroundWide + nBorderThickness, y0 ), vecZero ),
		vgui::Vertex_t( Vector2D( x0 + nBackgroundWide, y0 ), vecZero ),

		// Corner BL
		vgui::Vertex_t( Vector2D( x0 - nBorderThickness, y0 + nBackgroundTall ), vecZero ),
		vgui::Vertex_t( Vector2D( x0, y0 + nBackgroundTall ), vecZero ),
		vgui::Vertex_t( Vector2D( x0, y0 + nBackgroundTall + nBorderThickness ), vecZero ),

		// Corner BR
		vgui::Vertex_t( Vector2D( x0 + nBackgroundWide, y0 + nBackgroundTall ), vecZero ),
		vgui::Vertex_t( Vector2D( x0 + nBackgroundWide + nBorderThickness, y0 + nBackgroundTall ), vecZero ),
		vgui::Vertex_t( Vector2D( x0 + nBackgroundWide, y0 + nBackgroundTall + nBorderThickness ), vecZero )
	};

	for ( int nTri = 0; nTri < nNumCornerTris; ++nTri )
	{
		DrawTexturedPolygon( 3, cornerVerts + nTri * 3 );
	}

	if ( bPointer )
	{
		if ( nPointerY < y0 )
		{
			// Draw the up pointer border from polygons
			const int nNumPointerQuads = 3;
			vgui::Vertex_t pointerVerts[ nNumPointerQuads * 4 ] = 
			{
				// Pointer left
				vgui::Vertex_t( Vector2D( x0 + nHalfPointerBaseTopWide, y0 ), vecZero ),
				vgui::Vertex_t( Vector2D( x0 + nHalfPointerBaseTopWide, y0 - nBorderThickness ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX - nBorderThickness, nPointerY - nBorderThickness ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY ), vecZero ),

				// Pointer right
				vgui::Vertex_t( Vector2D( x0 + nPointerBaseThickness, y0 - nBorderThickness ), vecZero ),
				vgui::Vertex_t( Vector2D( x0 + nPointerBaseThickness, y0 ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX + nBorderThickness, nPointerY - nBorderThickness ), vecZero ),

				// Pointer bottom
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY - nBorderThickness * 2 ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX + nBorderThickness, nPointerY - nBorderThickness ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX - nBorderThickness, nPointerY - nBorderThickness ), vecZero )
			};

			for ( int nQuad = 0; nQuad < nNumPointerQuads; ++nQuad )
			{
				DrawTexturedPolygon( 4, pointerVerts + nQuad * 4 );
			}
		}
		else
		{
			// Draw the down pointer from polygons
			const int nNumPointerQuads = 3;
			vgui::Vertex_t pointerVerts[ nNumPointerQuads * 4 ] = 
			{
				// Pointer left
				vgui::Vertex_t( Vector2D( x0 + nHalfPointerBaseBottomWide, y0 + nBackgroundTall + nBorderThickness ), vecZero ),
				vgui::Vertex_t( Vector2D( x0 + nHalfPointerBaseBottomWide, y0 + nBackgroundTall ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX - nBorderThickness, nPointerY + nBorderThickness ), vecZero ),

				// Pointer right
				vgui::Vertex_t( Vector2D( x0 + nPointerBaseThickness, y0 + nBackgroundTall + nBorderThickness ), vecZero ),
				vgui::Vertex_t( Vector2D( x0 + nPointerBaseThickness, y0 + nBackgroundTall ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX + nBorderThickness, nPointerY + nBorderThickness ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY ), vecZero ),

				// Pointer bottom
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX + nBorderThickness, nPointerY + nBorderThickness ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX, nPointerY + nBorderThickness * 2 ), vecZero ),
				vgui::Vertex_t( Vector2D( nPointerX - nBorderThickness, nPointerY + nBorderThickness ), vecZero )
			};

			for ( int nQuad = 0; nQuad < nNumPointerQuads; ++nQuad )
			{
				DrawTexturedPolygon( 4, pointerVerts + nQuad * 4 );
			}
		}
	}

	SetClipRect( nOldClipX0, nOldClipY0, nOldClipX1, nOldClipY1 );
}



//-----------------------------------------------------------------------------
//
// Font-related methods begin here
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: creates a new empty font
//-----------------------------------------------------------------------------
HFont CMatSystemSurface::CreateFont()
{
	MAT_FUNC;

	return FontManager().CreateFont();
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CMatSystemSurface::SetFontGlyphSet(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin, int nRangeMax)
{
	return FontManager().SetFontGlyphSet(font, windowsFontName, tall, weight, blur, scanlines, flags, nRangeMin, nRangeMax);
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CMatSystemSurface::SetBitmapFontGlyphSet(HFont font, const char *windowsFontName, float scalex, float scaley, int flags)
{
	return FontManager().SetBitmapFontGlyphSet(font, windowsFontName, scalex, scaley, flags);
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CMatSystemSurface::GetFontTall(HFont font)
{
	return FontManager().GetFontTall(font);
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CMatSystemSurface::GetFontAscent(HFont font, wchar_t wch)
{
	return FontManager().GetFontAscent(font,wch);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMatSystemSurface::IsFontAdditive(HFont font)
{
	return FontManager().IsFontAdditive(font);
}

//-----------------------------------------------------------------------------
// Purpose: returns the abc widths of a single character
//-----------------------------------------------------------------------------
void CMatSystemSurface::GetCharABCwide(HFont font, int ch, int &a, int &b, int &c)
{
	FontManager().GetCharABCwide(font, ch, a, b, c);
}

//-----------------------------------------------------------------------------
// Purpose: returns the pixel width of a single character
//-----------------------------------------------------------------------------
int CMatSystemSurface::GetCharacterWidth(HFont font, int ch)
{
	return FontManager().GetCharacterWidth(font, ch);
}

//-----------------------------------------------------------------------------
// Purpose: returns the kerned width of this char
//-----------------------------------------------------------------------------
void CMatSystemSurface::GetKernedCharWidth( HFont font, wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC )
{
	FontManager().GetKernedCharWidth(font, ch, chBefore, chAfter, wide, abcA, abcC );
}


//-----------------------------------------------------------------------------
// Purpose: returns the area of a text string, including newlines
//-----------------------------------------------------------------------------
void CMatSystemSurface::GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall)
{
	FontManager().GetTextSize(font, text, wide, tall);
}

//-----------------------------------------------------------------------------
// Used by the localization library
//-----------------------------------------------------------------------------
int CMatSystemSurface::ComputeTextWidth( const wchar_t *pString )
{
	int nWide, nTall;
	GetTextSize( 1, pString, nWide, nTall );
	return nWide;
}

//-----------------------------------------------------------------------------
// Purpose: adds a custom font file (supports valve .vfont files)
//-----------------------------------------------------------------------------
bool CMatSystemSurface::AddCustomFontFile( const char *fontFileName )
{
	if ( IsGameConsole() )
	{
		// custom fonts are not supported (not needed) on xbox, all .vfonts are offline converted to ttfs
		// ttfs are mounted/handled elsewhere
		return true;
	}

	char fullPath[MAX_PATH];
	bool bFound = false;
	// windows needs an absolute path for ttf
	bFound = g_pFullFileSystem->GetLocalPath( fontFileName, fullPath, sizeof( fullPath ) ) ? true : false;
	if ( !bFound )
	{
		Warning( "Couldn't find custom font file '%s'\n", fontFileName );
		return false;
	}

	// only add if it's not already in the list
	Q_strlower( fullPath );
	CUtlSymbol sym(fullPath);
	int i;
	for ( i = 0; i < m_CustomFontFileNames.Count(); i++ )
	{
		if ( m_CustomFontFileNames[i] == sym )
			break;
	}
	if ( !m_CustomFontFileNames.IsValidIndex( i ) )
	{
	 	m_CustomFontFileNames.AddToTail( fullPath );

		if ( IsPC() )
		{
#ifdef SUPPORT_CUSTOM_FONT_FORMAT
			// We don't need the actual file on disk
#else
			// make sure it's on disk
			// only do this once for each font since in steam it will overwrite the
			// registered font file, causing windows to invalidate the font
			g_pFullFileSystem->GetLocalCopy( fullPath );
#endif
		}
	}

#if defined(WIN32)
#if !defined( _X360 )

#ifdef SUPPORT_CUSTOM_FONT_FORMAT
	// Just load the font data, decrypt in memory and register for this process
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( fontFileName, NULL, buf ) )
	{
		Msg( "Failed to load custom font file '%s'\n", fontFileName );
		return false;
	}

	if ( !ValveFont::DecodeFont( buf ) )
	{
		Msg( "Failed to parse custom font file '%s'\n", fontFileName );
		return false;
	}

	DWORD dwNumFontsRegistered = 0;
	HANDLE hRegistered = NULL;
	hRegistered = ::AddFontMemResourceEx( buf.Base(), buf.TellPut(), NULL, &dwNumFontsRegistered );

	if ( !hRegistered )
	{
		Msg( "Failed to register custom font file '%s'\n", fontFileName );
		return false;
	}
	
	m_CustomFontHandles.AddToTail( hRegistered );
	return hRegistered != NULL;
#else
	// try and use the optimal custom font loader, will makes sure fonts are unloaded properly
	// this function is in a newer version of the gdi library (win2k+), so need to try get it directly
	bool successfullyAdded = false;
	HMODULE gdiModule = ::LoadLibrary("gdi32.dll");
	if (gdiModule)
	{
		typedef int (WINAPI *AddFontResourceExProc)(LPCTSTR, DWORD, PVOID);
		AddFontResourceExProc pAddFontResourceEx = (AddFontResourceExProc)::GetProcAddress(gdiModule, "AddFontResourceExA");
		if (pAddFontResourceEx)
		{
			int result = (*pAddFontResourceEx)(fullPath, 0x10, NULL);
			if (result > 0)
			{
				successfullyAdded = true;
			}
		}
		::FreeLibrary(gdiModule);
	}

	// add to windows
	bool success = successfullyAdded || (::AddFontResource(fullPath) > 0);
	if ( !success )
	{
		Msg( "Failed to load custom font file '%s'\n", fullPath );
	}
	Assert( success );
	return success;
#endif
#endif // X360
#elif defined( _PS3 )
	return true;
#elif defined( OSX )
	// Just load the font data, decrypt in memory and register for this process
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( fontFileName, NULL, buf ) )
	{
		Msg( "Failed to load custom font file '%s'\n", fontFileName );
		return false;
	}
	
  OSStatus err;
	ATSFontContainerRef container;
  err = ATSFontActivateFromMemory( buf.Base(), buf.TellPut(), kATSFontContextLocal, kATSFontFormatUnspecified, NULL, kATSOptionFlagsDefault, &container );
  if ( err != noErr && ValveFont::DecodeFont( buf ) )
	{
    err = ATSFontActivateFromMemory( buf.Base(), buf.TellPut(), kATSFontContextLocal, kATSFontFormatUnspecified, NULL, kATSOptionFlagsDefault, &container );
  }
	
#if 0
  if ( err == noErr )
  {
	 // Debug code to let you find out the name of a font we pull in from a memory buffer
	 // Count the number of fonts that were loaded.
	 ItemCount fontCount = 0;
	 err = ATSFontFindFromContainer(container, kATSOptionFlagsDefault, 0,
	 NULL, &fontCount);
	 
	 if (err != noErr || fontCount < 1) {
	 return false;
	 }
	 
	 // Load font from container.
	 ATSFontRef font_ref_ats = 0;
	 ATSFontFindFromContainer(container, kATSOptionFlagsDefault, 1,
	 &font_ref_ats, NULL);
	 
	 if (!font_ref_ats) {
	 return false;
	 }
	 
	 CFStringRef name;
	 ATSFontGetPostScriptName( font_ref_ats, kATSOptionFlagsDefault, &name );
	 
	 const char *font_name = CFStringGetCStringPtr( name, CFStringGetSystemEncoding());
   printf( "loaded %s\n", font_name );
  }
#endif
	return err == noErr;
#elif defined(LINUX)
	// Just load the font data, decrypt in memory and register for this process
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( fontFileName, NULL, buf ) )
	{
		Msg( "Failed to load custom font file '%s'\n", fontFileName );
		return false;
	}
	
	FT_Error error;
	FT_Face face;
	if ( !ValveFont::DecodeFont( buf ) )
		error = FT_New_Face( FontManager().GetFontLibraryHandle(), (const char *)fullPath, 0, &face );
	else
		error = FT_New_Memory_Face( FontManager().GetFontLibraryHandle(), (FT_Byte *)buf.Base(), buf.TellPut(), 0, &face );

	if ( error == FT_Err_Unknown_File_Format ) 
	{
		return false;
	} 
	else if ( error ) 
	{ 
		return false;
	} 
	const char *pchFontName = FT_Get_Postscript_Name( face );
	if ( !V_stricmp( pchFontName, "TradeGothic" ) )
		pchFontName = "Trade Gothic";
	if ( !V_stricmp( pchFontName, "TradeGothicBold" ) )
		pchFontName = "Trade Gothic Bold";
	if ( !V_stricmp( pchFontName, "Stubble-bold" ) )
		pchFontName = "Stubble bold";

	font_entry entry;
	entry.size = buf.TellPut();
	entry.data = buf.Detach();
	m_FontData.Insert( pchFontName, entry );
	FT_Done_Face ( face );
	return true;	
#else
#error	
#endif
}

#ifdef LINUX
void *CMatSystemSurface::FontDataHelper( const char *pchFontName, int &size )
{
	int iIndex = m_FontData.Find( pchFontName );
	if ( iIndex != m_FontData.InvalidIndex() )
	{
		size = m_FontData[ iIndex ].size;
		return m_FontData[ iIndex ].data;
	}
	size = 0;
	return NULL;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: adds a bitmap font file
//-----------------------------------------------------------------------------
bool CMatSystemSurface::AddBitmapFontFile( const char *fontFileName )
{
	MAT_FUNC;

	bool bFound = false;
	bFound = ( ( g_pFullFileSystem->GetDVDMode() == DVDMODE_STRICT ) || g_pFullFileSystem->FileExists( fontFileName, IsGameConsole() ? "GAME" : NULL ) );
	if ( !bFound )
	{
		Msg( "Couldn't find bitmap font file '%s'\n", fontFileName );
		return false;
	}
	char path[MAX_PATH];
	Q_strncpy( path, fontFileName, MAX_PATH );

	// only add if it's not already in the list
	Q_strlower( path );
	CUtlSymbol sym( path );
	int i;
	for ( i = 0; i < m_BitmapFontFileNames.Count(); i++ )
	{
		if ( m_BitmapFontFileNames[i] == sym )
			break;
	}
	if ( !m_BitmapFontFileNames.IsValidIndex( i ) )
	{
	 	m_BitmapFontFileNames.AddToTail( path );

		if ( IsPC() )
		{
			// make sure it's on disk
			// only do this once for each font since in steam it will overwrite the
			// registered font file, causing windows to invalidate the font
			g_pFullFileSystem->GetLocalCopy( path );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::SetBitmapFontName( const char *pName, const char *pFontFilename )
{
	char fontPath[MAX_PATH];
	Q_strncpy( fontPath, pFontFilename, MAX_PATH );
	Q_strlower( fontPath );

	CUtlSymbol sym( fontPath );
	int i;
	for (i = 0; i < m_BitmapFontFileNames.Count(); i++)
	{
		if ( m_BitmapFontFileNames[i] == sym )
		{
			// found it, update the mapping
			int index = m_BitmapFontFileMapping.Find( pName );
			if ( !m_BitmapFontFileMapping.IsValidIndex( index ) )
			{
				index = m_BitmapFontFileMapping.Insert( pName );	
			}
			m_BitmapFontFileMapping.Element( index ) = i;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CMatSystemSurface::GetBitmapFontName( const char *pName )
{
	// find it in the mapping symbol table
	int index = m_BitmapFontFileMapping.Find( pName );
	if ( index == m_BitmapFontFileMapping.InvalidIndex() )
	{
		return "";
	}

	return m_BitmapFontFileNames[m_BitmapFontFileMapping.Element( index )].String();
}

void CMatSystemSurface::ClearTemporaryFontCache( void )
{
	FontManager().ClearTemporaryFontCache();
}

//-----------------------------------------------------------------------------
// Purpose: Force a set of characters to be rendered into the font page.
//-----------------------------------------------------------------------------
void CMatSystemSurface::PrecacheFontCharacters( HFont font, wchar_t *pCharacterString )
{
	if ( !pCharacterString || !pCharacterString[0] )
	{
		return;
	}

	StartDrawing();
	DrawSetTextFont( font );

	int numChars = 0;
	while( pCharacterString[ numChars ] )
	{
		numChars++;
	}
	int *pTextureIDs_ignored = (int *)stackalloc( numChars*sizeof( int ) );
	float **pTexCoords_ignored = (float **)stackalloc( numChars*sizeof( float * ) );
	g_FontTextureCache.GetTextureForChars( m_hCurrentFont, FONT_DRAW_DEFAULT, pCharacterString, pTextureIDs_ignored, pTexCoords_ignored, numChars );

	FinishDrawing();
}

const char *CMatSystemSurface::GetFontName( HFont font )
{
	return FontManager().GetFontName( font );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetTextFont(HFont font)
{
	Assert( g_bInDrawing );

	m_hCurrentFont = font;
}

//-----------------------------------------------------------------------------
// Purpose: Renders any batched up text
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawFlushText()
{
	if ( !m_nBatchedCharVertCount )
		return;

	IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial(m_iBoundTexture);
	InternalSetMaterial( pMaterial );
	DrawQuadArray( m_nBatchedCharVertCount / 2, m_BatchedCharVerts, m_DrawTextColor );
	m_nBatchedCharVertCount = 0;
}

//-----------------------------------------------------------------------------
// Sets the text color
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetTextColor(int r, int g, int b, int a)
{
	int adjustedAlpha = (a * m_flAlphaMultiplier);

	if ( r != m_DrawTextColor[0] || g != m_DrawTextColor[1] || b != m_DrawTextColor[2] || adjustedAlpha != m_DrawTextColor[3] )
	{
		// text color changed, flush any existing text
		DrawFlushText();

		m_DrawTextColor[0] = (unsigned char)r;
		m_DrawTextColor[1] = (unsigned char)g;
		m_DrawTextColor[2] = (unsigned char)b;
		m_DrawTextColor[3] = (unsigned char)adjustedAlpha;
	}
}

//-----------------------------------------------------------------------------
// Purpose: alternate color set
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetTextColor(Color col)
{
	DrawSetTextColor(col[0], col[1], col[2], col[3]);
}

//-----------------------------------------------------------------------------
// Purpose: change the scale of a bitmap font
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetTextScale(float sx, float sy)
{
	FontManager().SetFontScale( m_hCurrentFont, sx, sy );
}

//-----------------------------------------------------------------------------
// Text rendering location
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetTextPos(int x, int y)
{
	Assert( g_bInDrawing );

	m_pDrawTextPos[0] = x;
	m_pDrawTextPos[1] = y;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawGetTextPos(int& x,int& y)
{
	Assert( g_bInDrawing );

	x = m_pDrawTextPos[0];
	y = m_pDrawTextPos[1];
}

#pragma warning( disable : 4706 )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawUnicodeString( const wchar_t *pString, FontDrawType_t drawType /*= FONT_DRAW_DEFAULT */ )
{
	// skip fully transparent characters
	if ( m_DrawTextColor[3] == 0 )
		return;

	//hushed MAT_FUNC;
#ifdef POSIX
	DrawPrintText( pString, V_wcslen( pString ) , drawType );
#else
	wchar_t	ch;

	while ( ( ch = *pString++ ) )
	{
		DrawUnicodeChar( ch );	
	}
#endif
}
#pragma warning( default : 4706 )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawUnicodeChar(wchar_t ch, FontDrawType_t drawType /*= FONT_DRAW_DEFAULT */ )
{
	// skip fully transparent characters
	if ( m_DrawTextColor[3] == 0 )
		return;

	FontCharRenderInfo info;
	info.drawType = drawType;
	if ( DrawGetUnicodeCharRenderInfo( ch, info ) )
	{
		DrawRenderCharFromInfo( info );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMatSystemSurface::DrawGetUnicodeCharRenderInfo( wchar_t ch, FontCharRenderInfo& info )
{
	Assert( g_bInDrawing );
	info.valid = false;

	if ( !m_hCurrentFont )
	{
		return info.valid;
	}

	PREFETCH360( &m_BatchedCharVerts[ m_nBatchedCharVertCount ], 0 );

	info.valid = true;
	info.ch = ch;
	DrawGetTextPos(info.x, info.y);

	info.currentFont = m_hCurrentFont;
	info.fontTall = GetFontTall(m_hCurrentFont);

	GetCharABCwide(m_hCurrentFont, ch, info.abcA, info.abcB, info.abcC);
	bool bUnderlined = FontManager().GetFontUnderlined( m_hCurrentFont );
	
	// Do prestep before generating texture coordinates, etc.
	if ( !bUnderlined )
	{
		info.x += info.abcA;
	}

	// get the character texture from the cache
	info.textureId = 0;
	float *texCoords = NULL;
	if (!g_FontTextureCache.GetTextureForChar(m_hCurrentFont, info.drawType, ch, &info.textureId, &texCoords))
	{
		info.valid = false;
		return info.valid;
	}

	// Text will get flushed here if it needs to be.
	DrawSetTexture( info.textureId );


	int fontWide = info.abcB;
	if ( bUnderlined )
	{
		fontWide += ( info.abcA + info.abcC );
		info.x-= info.abcA;
	}

	// This avoid copying the data in the nonclipped case!!! (X360)
	info.verts = &m_BatchedCharVerts[ m_nBatchedCharVertCount ];
	InitVertex( info.verts[0], info.x, info.y, texCoords[0], texCoords[1] );
	InitVertex( info.verts[1], info.x + fontWide, info.y + info.fontTall, texCoords[2], texCoords[3] );

	info.shouldclip = true;

	return info.valid;
}

//-----------------------------------------------------------------------------
// Purpose: batches up characters for rendering
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawRenderCharInternal( const FontCharRenderInfo& info )
{
	Assert( g_bInDrawing );
	
	// xbox opts out of pricey/pointless text clipping
	if ( IsPC() && info.shouldclip )
	{
		Vertex_t clip[ 2 ];
		clip[ 0 ] = info.verts[ 0 ];
		clip[ 1 ] = info.verts[ 1 ];
		if ( !ClipRect( clip[0], clip[1], &info.verts[0], &info.verts[1] ) )
		{
			// Fully clipped
			return;	
		}
	}

	m_nBatchedCharVertCount += 2;

	if ( m_nBatchedCharVertCount >= MAX_BATCHED_CHAR_VERTS - 2 )
	{
		DrawFlushText();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawRenderCharFromInfo( const FontCharRenderInfo& info )
{
	if ( !info.valid )
		return;

	int x = info.x;

	// get the character texture from the cache
	DrawSetTexture( info.textureId );
	
	DrawRenderCharInternal( info );

	// Only do post step
	x += ( info.abcB + info.abcC );

	// Update cursor pos
	DrawSetTextPos(x, info.y);
}

//-----------------------------------------------------------------------------
// Renders a text buffer
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawPrintText(const wchar_t *text, int iTextLen, FontDrawType_t drawType /*= FONT_DRAW_DEFAULT */ )
{
	MAT_FUNC;

	CMatRenderContextPtr prc( g_pMaterialSystem );
	Assert( g_bInDrawing );
	if (!text)
		return;

	if (!m_hCurrentFont)
		return;

	int x = m_pDrawTextPos[0] + m_nTranslateX;
	int y = m_pDrawTextPos[1] + m_nTranslateY;

	int iTall = GetFontTall(m_hCurrentFont);
	int iLastTexId = -1;

	int iCount = 0;
	Vertex_t *pQuads = (Vertex_t*)stackalloc((2 * iTextLen) * sizeof(Vertex_t) );
	bool bUnderlined = FontManager().GetFontUnderlined( m_hCurrentFont );

	int iTotalWidth = 0;
	for (int i=0; i<iTextLen; ++i)
	{
		wchar_t ch = text[i];

		int abcA,abcB,abcC;
		GetCharABCwide(m_hCurrentFont, ch, abcA, abcB, abcC);
		
		//iTotalWidth += abcA;
		float flWide;
		float flabcA;
		float flabcC;
		wchar_t chBefore = 0;
		wchar_t chAfter = 0;
		if ( i > 0 )
			chBefore = text[i-1];
		if ( i < iTextLen )
			chAfter = text[i+1];
		FontManager().GetKernedCharWidth( m_hCurrentFont, ch, chBefore, chAfter, flWide, flabcA, flabcC );
		
		int textureWide = abcB;
		if ( bUnderlined )
		{
			textureWide += ( abcA + abcC );
			x-= abcA;
		}

		if ( !iswspace( ch ) || bUnderlined )
		{
			// get the character texture from the cache
			int iTexId = 0;
			float *texCoords = NULL;
			if (!g_FontTextureCache.GetTextureForChar(m_hCurrentFont, drawType, ch, &iTexId, &texCoords))
				continue;

			Assert( texCoords );

			if (iTexId != iLastTexId)
			{
				// FIXME: At the moment, we just draw all the batched up
				// text when the font changes. We Should batch up per material
				// and *then* draw
				if (iCount)
				{
					IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial(iLastTexId);
					InternalSetMaterial( pMaterial );
					DrawQuadArray( iCount, pQuads, m_DrawTextColor );
					iCount = 0;
				}

				iLastTexId = iTexId;
			}

 			Vertex_t &ul = pQuads[2*iCount];
 			Vertex_t &lr = pQuads[2*iCount + 1];
			++iCount;

			ul.m_Position.x = x + iTotalWidth + floor(abcA + 0.6);
			ul.m_Position.y = y;
			lr.m_Position.x = ul.m_Position.x +  textureWide;
			lr.m_Position.y = ul.m_Position.y + iTall;

			// Gets at the texture coords for this character in its texture page
			/*
			float tex_U0_bias = prc->Knob("tex-U0-bias");
			float tex_V0_bias = prc->Knob("tex-V0-bias");
			float tex_U1_bias = prc->Knob("tex-U1-bias");
			float tex_V1_bias = prc->Knob("tex-V1-bias");

			ul.m_TexCoord[0] = texCoords[0] + tex_U0_bias;
			ul.m_TexCoord[1] = texCoords[1] + tex_V0_bias;
			lr.m_TexCoord[0] = texCoords[2] + tex_U1_bias;
			lr.m_TexCoord[1] = texCoords[3] + tex_V1_bias;
			*/

			ul.m_TexCoord[0] = texCoords[0];
			ul.m_TexCoord[1] = texCoords[1];
			lr.m_TexCoord[0] = texCoords[2];
			lr.m_TexCoord[1] = texCoords[3];
		}

		iTotalWidth += floor(flWide+0.6);
	}

	// Draw any left-over characters
	if (iCount)
	{
		IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial(iLastTexId);
		InternalSetMaterial( pMaterial );
		DrawQuadArray( iCount, pQuads, m_DrawTextColor );
	}

	m_pDrawTextPos[0] += iTotalWidth;

	stackfree(pQuads);
}


//-----------------------------------------------------------------------------
// Returns the screen size
//-----------------------------------------------------------------------------
void CMatSystemSurface::GetScreenSize(int &iWide, int &iTall)
{
	if ( m_ScreenSizeOverride.m_bActive )
	{
		iWide = m_ScreenSizeOverride.m_nValue[ 0 ];
		iTall = m_ScreenSizeOverride.m_nValue[ 1 ];
		return;
	}

	int x, y;
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->GetViewport( x, y, iWide, iTall );
}

bool CMatSystemSurface::ForceScreenSizeOverride( bool bState, int wide, int tall )
{
	bool bWasSet = m_ScreenSizeOverride.m_bActive;
	m_ScreenSizeOverride.m_bActive = bState;
	m_ScreenSizeOverride.m_nValue[ 0 ] = wide;
	m_ScreenSizeOverride.m_nValue[ 1 ] = tall;
	return bWasSet;
}

// LocalToScreen, ParentLocalToScreen fixups for explicit PaintTraverse calls on Panels not at 0, 0 position
bool CMatSystemSurface::ForceScreenPosOffset( bool bState, int x, int y )
{
	bool bWasSet = m_ScreenPosOverride.m_bActive;
	m_ScreenPosOverride.m_bActive = bState;
	m_ScreenPosOverride.m_nValue[ 0 ] = x;
	m_ScreenPosOverride.m_nValue[ 1 ] = y;
	return bWasSet;
}

void CMatSystemSurface::OffsetAbsPos( int &x, int &y )
{
	if ( !m_ScreenPosOverride.m_bActive )
		return;

	x += m_ScreenPosOverride.m_nValue[ 0 ];
	y += m_ScreenPosOverride.m_nValue[ 1 ];
}


bool CMatSystemSurface::IsScreenSizeOverrideActive( void )
{
	return ( m_ScreenSizeOverride.m_bActive );
}

bool CMatSystemSurface::IsScreenPosOverrideActive( void )
{
	return ( m_ScreenPosOverride.m_bActive );
}

//-----------------------------------------------------------------------------
// Purpose: Notification of a new screen size
//-----------------------------------------------------------------------------
void CMatSystemSurface::OnScreenSizeChanged( int nOldWidth, int nOldHeight )
{
	int iNewWidth, iNewHeight;
	GetScreenSize( iNewWidth, iNewHeight );

	Msg( "Changing resolutions from (%d, %d) -> (%d, %d)\n", nOldWidth, nOldHeight, iNewWidth, iNewHeight );

	// update the root panel size
	ipanel()->SetSize(m_pEmbeddedPanel, iNewWidth, iNewHeight);

	// notify every panel
	VPANEL panel = GetEmbeddedPanel();
	ivgui()->PostMessage(panel, new KeyValues("OnScreenSizeChanged", "oldwide", nOldWidth, "oldtall", nOldHeight), NULL);

	// only the pc can support a resolution change
	// the schemes/fonts are size based, these need to be redone before the panels make font queries
	// scheme manager will early out if video size change not truly detected (need to match that logic, otherwise fonts won't get rebuilt)
	if ( IsPC() && ( iNewWidth != nOldWidth || iNewHeight != nOldHeight ) )
	{
		// schemes are size based and need to be redone
		ReloadSchemes();
	}

	// Run a frame of the GUI to notify all subwindows of the message size change
	ivgui()->RunFrame();
}

// Causes schemes to get reloaded which then causes fonts to get reloaded
void CMatSystemSurface::ReloadSchemes()
{
	// Don't do this on game consoles!!!
	// This can't be supported as font work is enormously expensive in terms of memory and i/o
	if ( IsGameConsole() )
		return;

	// clear font texture cache
	g_FontTextureCache.Clear();
	m_iBoundTexture = -1;

	// about to reload schemes, which will reload fonts
	// this wipes away the existing set of underlying resources (otherwise leak)
	FontManager().ClearAllFonts();
	// can now reload schemes which will repopulate the font tables
	scheme()->ReloadSchemes();
}

// Causes fonts to get reloaded, etc.
void CMatSystemSurface::ResetFontCaches()
{
	// Don't do this on game consoles!!!
	// This can't be supported as font work is enormously expensive in terms of memory and i/o
	if ( IsGameConsole() )
		return;

	// clear font texture cache
	g_FontTextureCache.Clear();
	m_iBoundTexture = -1;

	// reload fonts
	FontManager().ClearAllFonts();
	scheme()->ReloadFonts();
}

//-----------------------------------------------------------------------------
// Returns the size of the embedded panel
//-----------------------------------------------------------------------------
void CMatSystemSurface::GetWorkspaceBounds(int &x, int &y, int &iWide, int &iTall)
{
	if ( m_ScreenSizeOverride.m_bActive )
	{
		x = y = 0;
		iWide = m_ScreenSizeOverride.m_nValue[ 0 ];
		iTall = m_ScreenSizeOverride.m_nValue[ 1 ];
		return;
	}
	// NOTE: This is equal to the viewport size by default,
	// but other embedded panels can be used
	x = m_WorkSpaceInsets[0];
	y = m_WorkSpaceInsets[1];
	g_pVGuiPanel->GetSize(m_pEmbeddedPanel, iWide, iTall);

	iWide -= m_WorkSpaceInsets[2];
	iTall -= m_WorkSpaceInsets[3];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::SetWorkspaceInsets( int left, int top, int right, int bottom )
{
	m_WorkSpaceInsets[0] = left;
	m_WorkSpaceInsets[1] = top;
	m_WorkSpaceInsets[2] = right;
	m_WorkSpaceInsets[3] = bottom;
}

//-----------------------------------------------------------------------------
// A bunch of methods needed for the windows version only
//-----------------------------------------------------------------------------
void CMatSystemSurface::SetAsTopMost(VPANEL panel, bool state)
{
}

void CMatSystemSurface::SetAsToolBar(VPANEL panel, bool state)		// removes the window's task bar entry (for context menu's, etc.)
{
}

void CMatSystemSurface::SetForegroundWindow (VPANEL panel)
{
	BringToFront(panel);
}

void CMatSystemSurface::SetPanelVisible(VPANEL panel, bool state)
{
}

void CMatSystemSurface::SetMinimized(VPANEL panel, bool state)
{
	if (state)
	{
		g_pVGuiPanel->SetPlat(panel, VPANEL_MINIMIZED);
		g_pVGuiPanel->SetVisible(panel, false);
	}
	else
	{
		g_pVGuiPanel->SetPlat(panel, VPANEL_NORMAL);
	}
}

bool CMatSystemSurface::IsMinimized(vgui::VPANEL panel)
{
	return (g_pVGuiPanel->Plat(panel) == VPANEL_MINIMIZED);

}

void CMatSystemSurface::FlashWindow(VPANEL panel, bool state)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::SetTitle(VPANEL panel, const wchar_t *title)
{
	int entry = GetTitleEntry( panel );
	if ( entry == -1 )
	{
		entry = m_Titles.AddToTail();
	}

	TitleEntry *e = &m_Titles[ entry ];
	Assert( e );
	wcsncpy( e->title, title, sizeof( e->title )/ sizeof( wchar_t ) );
	e->panel = panel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
wchar_t const *CMatSystemSurface::GetTitle( VPANEL panel )
{
	int entry = GetTitleEntry( panel );
	if ( entry != -1 )
	{
		TitleEntry *e = &m_Titles[ entry ];
		return e->title;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Private lookup method
// Input  : *panel - 
// Output : TitleEntry
//-----------------------------------------------------------------------------
int CMatSystemSurface::GetTitleEntry( vgui::VPANEL panel )
{
	for ( int i = 0; i < m_Titles.Count(); i++ )
	{
		TitleEntry* entry = &m_Titles[ i ];
		if ( entry->panel == panel )
			return i;
	}
	return -1;
}

void CMatSystemSurface::SwapBuffers(VPANEL panel)
{
}

void CMatSystemSurface::Invalidate(VPANEL panel)
{
}

void CMatSystemSurface::ApplyChanges()
{
}

// notify icons?!?
VPANEL CMatSystemSurface::GetNotifyPanel()
{
	return NULL;
}

void CMatSystemSurface::SetNotifyIcon(VPANEL context, HTexture icon, VPANEL panelToReceiveMessages, const char *text)
{
}

bool CMatSystemSurface::IsWithin(int x, int y)
{
	return true;
}

bool CMatSystemSurface::ShouldPaintChildPanel(VPANEL childPanel)
{
	if ( m_pRestrictedPanel && ( m_pRestrictedPanel != childPanel ) && 
		 !g_pVGuiPanel->HasParent( childPanel, m_pRestrictedPanel ) )
	{
		return false;
	}

	bool isPopup = ipanel()->IsPopup(childPanel);
	return !isPopup;
}

bool CMatSystemSurface::RecreateContext(VPANEL panel)
{
	return false;
}

//-----------------------------------------------------------------------------
// Focus-related methods
//-----------------------------------------------------------------------------
bool CMatSystemSurface::HasFocus()
{
	return true;
}

void CMatSystemSurface::BringToFront(VPANEL panel)
{
	// move panel to top of list
	g_pVGuiPanel->MoveToFront(panel);

	// move panel to top of popup list
	if ( g_pVGuiPanel->IsPopup( panel ) )
	{
		MovePopupToFront( panel );
	}
}


// engine-only focus handling (replacing WM_FOCUS windows handling)
void CMatSystemSurface::SetTopLevelFocus(VPANEL pSubFocus)
{
	// walk up the hierarchy until we find what popup panel belongs to
	while (pSubFocus)
	{
		if (ipanel()->IsPopup(pSubFocus) && ipanel()->IsMouseInputEnabled(pSubFocus))
		{
			BringToFront(pSubFocus);
			break;
		}
		
		pSubFocus = ipanel()->GetParent(pSubFocus);
	}
}


//-----------------------------------------------------------------------------
// Installs a function to play sounds
//-----------------------------------------------------------------------------
void CMatSystemSurface::InstallPlaySoundFunc( PlaySoundFunc_t soundFunc )
{
	m_PlaySoundFunc = soundFunc;
}


//-----------------------------------------------------------------------------
// plays a sound
//-----------------------------------------------------------------------------
void CMatSystemSurface::PlaySound(const char *pFileName)
{
	if (m_PlaySoundFunc)
		m_PlaySoundFunc( pFileName );
}


//-----------------------------------------------------------------------------
// handles mouse movement
//-----------------------------------------------------------------------------
void CMatSystemSurface::SetCursorPos(int x, int y)
{
	CursorSetPos( GetInputContext(), x, y );
}

void CMatSystemSurface::GetCursorPos(int &x, int &y)
{
	CursorGetPos( GetInputContext(), x, y );
}

void CMatSystemSurface::SetCursor(HCursor hCursor)
{
	if ( IsCursorLocked() )
		return;

	if ( _currentCursor != hCursor )
	{
		_currentCursor = hCursor;
		CursorSelect( GetInputContext(), hCursor );
	}
}

void CMatSystemSurface::EnableMouseCapture( VPANEL panel, bool state )
{
	g_pInputStackSystem->SetMouseCapture( GetInputContext(), state );
}


//-----------------------------------------------------------------------------
// Purpose: Turns the panel into a standalone window
//-----------------------------------------------------------------------------
void CMatSystemSurface::CreatePopup(VPANEL panel, bool minimized,  bool showTaskbarIcon, bool disabled , bool mouseInput , bool kbInput)
{
	if (!g_pVGuiPanel->GetParent(panel))
	{
		g_pVGuiPanel->SetParent(panel, GetEmbeddedPanel());
	}
	((VPanel *)panel)->SetPopup(true);
	((VPanel *)panel)->SetKeyBoardInputEnabled(kbInput);
	((VPanel *)panel)->SetMouseInputEnabled(mouseInput);

	if ( m_PopupList.Find( panel ) == m_PopupList.InvalidIndex() )
	{
		m_PopupList.AddToTail( panel );
	}
	else
	{
		MovePopupToFront( panel );
	}
}


//-----------------------------------------------------------------------------
// Create/destroy panels..
//-----------------------------------------------------------------------------
void CMatSystemSurface::AddPanel(VPANEL panel)
{
	if (g_pVGuiPanel->IsPopup(panel))
	{
		// turn it into a popup menu
		CreatePopup(panel, false);
	}
}

void CMatSystemSurface::ReleasePanel(VPANEL panel)
{
	// Remove from popup list if needed and remove any dead popups while we're at it
	RemovePopup( panel );

	int entry = GetTitleEntry( panel );
	if ( entry != -1 )
	{
		m_Titles.Remove( entry );
	}
}

void CMatSystemSurface::ResetPopupList()
{
	m_PopupList.RemoveAll();
}

void CMatSystemSurface::AddPopup( VPANEL panel )
{
	if ( m_PopupList.Find( panel ) == m_PopupList.InvalidIndex() )
	{
		m_PopupList.AddToTail( panel );
	}
}


void CMatSystemSurface::RemovePopup( vgui::VPANEL panel )
{
	// Remove from popup list if needed and remove any dead popups while we're at it
	int c = GetPopupCount();

	for ( int i = c -  1; i >= 0 ; i-- )
	{
		VPANEL popup = GetPopup(i );
		if ( popup != panel )
			continue;

		m_PopupList.Remove( i );
		break;
	}
}

//-----------------------------------------------------------------------------
// Methods associated with iterating + drawing the panel tree
//-----------------------------------------------------------------------------
void CMatSystemSurface::AddPopupsToList( VPANEL panel )
{
	if (!g_pVGuiPanel->IsVisible(panel))
		return;

	// Add to popup list as we visit popups
	// Note:  popup list is cleared in RunFrame which occurs before this call!!!
	if ( g_pVGuiPanel->IsPopup( panel ) )
	{
		AddPopup( panel );
	}

	int count = g_pVGuiPanel->GetChildCount(panel);
	for (int i = 0; i < count; ++i)
	{
		VPANEL child = g_pVGuiPanel->GetChild(panel, i);
		AddPopupsToList( child );
	}
}


//-----------------------------------------------------------------------------
// Purpose: recurses the panels calculating absolute positions
//			parents must be solved before children
//-----------------------------------------------------------------------------
void CMatSystemSurface::InternalSolveTraverse(VPANEL panel)
{
	VPanel * RESTRICT vp = (VPanel *)panel;

	// solve the parent
	vp->Solve();
	
	CUtlVector< VPanel * > &children = vp->GetChildren();

	// now we can solve the children
	int c = children.Count();
	for (int i = 0; i < c; ++i)
	{
		VPanel *child = children[ i ];
		if (child->IsVisible())
		{
			InternalSolveTraverse((VPANEL)child);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: recurses the panels giving them a chance to do a user-defined think,
//			PerformLayout and ApplySchemeSettings
//			must be done child before parent
//-----------------------------------------------------------------------------
void CMatSystemSurface::InternalThinkTraverse(VPANEL panel)
{
	VPanel * RESTRICT vp = (VPanel *)panel;

	// think the parent
	vp->Client()->Think();

	CUtlVector< VPanel * > &children = vp->GetChildren();

	// and then the children...
	int c = children.Count();
	for (int i = 0; i < c; ++i)
	{
		VPanel *child = children[ i ];
		if ( child->IsVisible() )
		{
			InternalThinkTraverse((VPANEL)child);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: recurses the panels giving them a chance to do apply settings,
//-----------------------------------------------------------------------------
void CMatSystemSurface::InternalSchemeSettingsTraverse(VPANEL panel, bool forceApplySchemeSettings)
{
	VPanel * RESTRICT vp = (VPanel *)panel;

	CUtlVector< VPanel * > &children = vp->GetChildren();

	// apply to the children...
	int c = children.Count();
	for (int i = 0; i < c; ++i)
	{
		VPanel *child = children[ i ];
		if ( forceApplySchemeSettings || child->IsVisible() )
		{	
			InternalSchemeSettingsTraverse((VPANEL)child, forceApplySchemeSettings);
		}
	}
	// and then the parent
	vp->Client()->PerformApplySchemeSettings();
}

//-----------------------------------------------------------------------------
// Purpose: Walks through the panel tree calling Solve() on them all, in order
//-----------------------------------------------------------------------------
void CMatSystemSurface::SolveTraverse(VPANEL panel, bool forceApplySchemeSettings)
{
	{
		VPROF( "InternalSchemeSettingsTraverse" );
		InternalSchemeSettingsTraverse(panel, forceApplySchemeSettings);
	}

	{
		VPROF( "InternalThinkTraverse" );
		InternalThinkTraverse(panel);
	}

	{
		VPROF( "InternalSolveTraverse" );
		InternalSolveTraverse(panel);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Restricts rendering to a single panel
//-----------------------------------------------------------------------------
void CMatSystemSurface::RestrictPaintToSinglePanel( VPANEL panel, bool bForceAllowNonModalSurface )
{
	if ( !bForceAllowNonModalSurface && panel && m_pRestrictedPanel && m_pRestrictedPanel == input()->GetAppModalSurface() )
	{
		return;	// don't restrict drawing to a panel other than the modal one - that's a good way to hang the game.
	}

	m_pRestrictedPanel = panel;
	
	if ( !panel && m_bRestrictedPanelOverrodeAppModalPanel )
	{
		// Unrestricting after previously restricting to a non-app modal panel.
		input()->SetAppModalSurface( NULL );
		m_bRestrictedPanelOverrodeAppModalPanel = false;
		return;
	}
	
	VPANEL pAppModal = input()->GetAppModalSurface();
	if ( !pAppModal )
	{
		input()->SetAppModalSurface( panel );	// if painting is restricted to this panel, it had better be modal, or else you can get in some bad state...
		m_bRestrictedPanelOverrodeAppModalPanel = true;
	}
}


//-----------------------------------------------------------------------------
// Is a panel under the restricted panel?
//-----------------------------------------------------------------------------
bool CMatSystemSurface::IsPanelUnderRestrictedPanel( VPANEL panel )
{
	if ( !m_pRestrictedPanel )
		return true;

	while ( panel )
	{
		if ( panel == m_pRestrictedPanel )
			return true;

		panel = ipanel()->GetParent( panel );
	}
	return false;
}


//-----------------------------------------------------------------------------
// Main entry point for painting
//-----------------------------------------------------------------------------
void CMatSystemSurface::PaintTraverseEx(VPANEL panel, bool paintPopups /*= false*/ )
{
	MAT_FUNC;

	if ( !ipanel()->IsVisible( panel ) )
		return;

	VPROF( "CMatSystemSurface::PaintTraverse" );
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	bool bTopLevelDraw = false;

	ShaderStencilState_t state;
	if ( g_bInDrawing == false )
	{
		// only set the 2d ortho mode once
		bTopLevelDraw = true;
		StartDrawing();

		// clear z + stencil buffer
		// NOTE: Stencil is used to get 3D painting in vgui panels working correctly 
		pRenderContext->ClearBuffers( false, true, true );

		state.m_bEnable = true;
		state.m_FailOp = SHADER_STENCILOP_KEEP;
		state.m_ZFailOp = SHADER_STENCILOP_KEEP;
		state.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
		state.m_CompareFunc = SHADER_STENCILFUNC_GEQUAL;
		state.m_nReferenceValue = 0;
		state.m_nTestMask = 0xFFFFFFFF;
		state.m_nWriteMask = 0xFFFFFFFF;
		m_nCurrReferenceValue = 0;
		pRenderContext->SetStencilState( state );
	}

	float flOldZPos = m_flZPos;

	// NOTE You might expect we'd have to draw these under the popups so they would occlude
	// them, but there are a few things we do have to draw on top of, esp. the black
	// panel that draws over the top of the engine to darken everything.
	m_flZPos = 0.0f;								
	if ( panel == GetEmbeddedPanel() )
	{
		if ( m_pRestrictedPanel )
		{
			// Paint the restricted panel, and its parent.
			// NOTE: This call has guards to not draw popups. If the restricted panel
			// is a popup, it won't draw here.
			ipanel()->PaintTraverse( ipanel()->GetParent( m_pRestrictedPanel ), true );
		}
		else
		{
			// paint traverse the root panel, painting all children
			VPROF( "ipanel()->PaintTraverse" );
			ipanel()->PaintTraverse( panel, true );
		}
	}
	else
	{
		// If it's a popup, it should already have been painted above
		VPROF( "ipanel()->PaintTraverse" );
		if ( !paintPopups || !ipanel()->IsPopup( panel ) )
		{
			ipanel()->PaintTraverse( panel, true );
		}
	}

	// draw the popups
	if ( paintPopups )
	{
		// now draw the popups front to back
		// since depth-test and depth-write are on, the front panels will occlude the underlying ones
		{
			VPROF( "CMatSystemSurface::PaintTraverse popups loop" );
			int popups = m_PopupList.Count();

			// HACK! Using stencil ref 254 so drag/drop helper can use 255.
			int nStencilRef = 254;
			for ( int i = popups - 1; i >= 0; --i )
			{
				VPANEL popupPanel = m_PopupList[ i ];
				if ( !ipanel()->IsFullyVisible( popupPanel ) )
					continue;

				if ( !IsPanelUnderRestrictedPanel( popupPanel ) )
					continue;

				// This makes sure the drag/drop helper is always the first thing drawn
				bool bIsTopmostPopup = ( (VPanel *)popupPanel )->IsTopmostPopup();

				// set our z position
				m_nCurrReferenceValue = bIsTopmostPopup ? 255 : nStencilRef;
				state.m_nReferenceValue = m_nCurrReferenceValue;
				pRenderContext->SetStencilState( state );
				if ( nStencilRef < 1 )
				{
					Warning( "Too many popups! Rendering will be bad!\n" );
				}
				--nStencilRef;

				m_flZPos = ((float)(i) / (float)popups);
				ipanel()->PaintTraverse( popupPanel, true );
			}
		}
	}

	// Restore the old Z Pos
	m_flZPos = flOldZPos;

	if ( bTopLevelDraw )
	{
		// only undo the 2d ortho mode once
		VPROF( "FinishDrawing" );

		// Reset stencil to normal state
		state.m_bEnable = false;
		pRenderContext->SetStencilState( state );

		FinishDrawing();
	}
}

//-----------------------------------------------------------------------------
// Draw a panel
//-----------------------------------------------------------------------------
void CMatSystemSurface::PaintTraverse(VPANEL panel)
{
	PaintTraverseEx( panel, false );
}


//-----------------------------------------------------------------------------
// Begins, ends 3D painting from within a panel paint() method
//-----------------------------------------------------------------------------
void CMatSystemSurface::Begin3DPaint( int iLeft, int iTop, int iRight, int iBottom, bool bSupersampleRT )
{
	MAT_FUNC;

	Assert( iRight > iLeft );
	Assert( iBottom > iTop );

	// Can't use this while drawing in the 3D world since it relies on
	// whacking the shared depth buffer
	Assert( !m_bDrawingIn3DWorld );
	if ( m_bDrawingIn3DWorld )
		return;

	m_n3DLeft = iLeft;
	m_n3DRight = iRight;
	m_n3DTop = iTop;
	m_n3DBottom = iBottom;

	// Can't use this feature when drawing into the 3D world
	Assert( !m_bDrawingIn3DWorld );
	Assert( !m_bIn3DPaintMode );
	m_bIn3DPaintMode = true;

	// Save off the matrices in case the painting method changes them.
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();

	// For 3d painting, use the off-screen render target the material system allocates
	// NOTE: We have to grab it here, as opposed to during init,
	// because the mode hasn't been set by now.
	if ( !m_FullScreenBuffer )
	{
		m_FullScreenBuffer.Init( materials->FindTexture( m_FullScreenBufferName, "render targets" ) );
	}

	if ( !bSupersampleRT || IsX360() )
	{
		m_n3DViewportWidth = iRight - iLeft;
		m_n3DViewportHeight = iBottom - iTop;
	}
	else
	{
		m_n3DViewportWidth = m_FullScreenBuffer->GetActualWidth();
		m_n3DViewportHeight = m_FullScreenBuffer->GetActualHeight();

		// See if we can safely double the resolution
		if ( ( m_n3DViewportWidth >= ( iRight - iLeft )*2 ) && ( m_n3DViewportHeight >= ( iBottom - iTop )*2 ) )
		{
			m_n3DViewportWidth = ( iRight - iLeft )*2;
			m_n3DViewportHeight = ( iBottom - iTop )*2;
		}
		// See if we have more RT space to enlarge the buffer for supersampling
		else if ( ( m_n3DViewportWidth > iRight - iLeft ) && ( m_n3DViewportHeight > iBottom - iTop ) && ( iRight - iLeft > 0 ) && ( iBottom - iTop > 0 ) )
		{
			double dblWidthFactor = double( m_n3DViewportWidth ) / double( iRight - iLeft );
			double dblHeightFactor = double( m_n3DViewportHeight ) / double( iBottom - iTop );
			if ( dblWidthFactor < dblHeightFactor )
			{
				m_n3DViewportHeight = MIN( int( ( iBottom - iTop ) * dblWidthFactor ), m_n3DViewportHeight );
			}
			else
			{
				m_n3DViewportWidth = MIN( int( ( iRight - iLeft ) * dblHeightFactor ), m_n3DViewportWidth );
			}
		}
		else
		{
			// We have to stick with the non-supersampled dimensions because we failed
			// to fit a supersampled size into our render target
			m_n3DViewportWidth = iRight - iLeft;
			m_n3DViewportHeight = iBottom - iTop;
		}
	}

	// FIXME: Set the viewport to match the clip rectangle?
	// Set the viewport to match the scissor rectangle
	pRenderContext->PushRenderTargetAndViewport( m_FullScreenBuffer, 
		0, 0, m_n3DViewportWidth, m_n3DViewportHeight );

	pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	// Don't draw the 3D scene w/ stencil
	ShaderStencilState_t state;
	state.m_bEnable = false;
	pRenderContext->SetStencilState( state );
}

void CMatSystemSurface::End3DPaint( bool bIgnoreAlphaWhenCompositing )
{
	MAT_FUNC;

	// Can't use this feature when drawing into the 3D world
	Assert( !m_bDrawingIn3DWorld );
	Assert( m_bIn3DPaintMode );
	m_bIn3DPaintMode = false;

	// Reset stencil to set stencil everywhere we draw 
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	ShaderStencilState_t state;
	state.m_bEnable = true;
	state.m_FailOp = SHADER_STENCILOP_KEEP;
	state.m_ZFailOp = SHADER_STENCILOP_KEEP;
	state.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
	state.m_CompareFunc = SHADER_STENCILFUNC_GEQUAL;
	state.m_nReferenceValue = m_nCurrReferenceValue;
	state.m_nTestMask = 0xFFFFFFFF;
	state.m_nWriteMask = 0xFFFFFFFF;
	pRenderContext->SetStencilState( state );

	// Restore the matrices
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	if ( IsX360() )
	{
		Rect_t rect;
		rect.x = rect.y = 0;
		rect.width = m_n3DRight - m_n3DLeft;
		rect.height = m_n3DBottom - m_n3DTop;

		ITexture *pRtModelPanelTexture = g_pMaterialSystem->FindTexture( MODEL_PANEL_RT_NAME, TEXTURE_GROUP_RENDER_TARGET );
		int nMaxWidth = pRtModelPanelTexture->GetActualWidth();
		int nMaxHeight = pRtModelPanelTexture->GetActualHeight();
		rect.width = MIN( rect.width, nMaxWidth );
		rect.height = MIN( rect.height, nMaxHeight );

		pRenderContext->CopyRenderTargetToTextureEx( pRtModelPanelTexture, 0, &rect, &rect );
	}

	// Restore the viewport (it was stored off in StartDrawing)
	pRenderContext->PopRenderTargetAndViewport();
	pRenderContext->CullMode(MATERIAL_CULLMODE_CCW);

	// Draw the full-screen buffer into the panel
	DrawFullScreenBuffer( m_n3DLeft, m_n3DTop, m_n3DRight, m_n3DBottom, m_n3DViewportWidth, m_n3DViewportHeight, bIgnoreAlphaWhenCompositing );

	// ReSet the material state
	InternalSetMaterial( NULL );
}


//-----------------------------------------------------------------------------
// Gets texture coordinates for drawing the full screen buffer
//-----------------------------------------------------------------------------
void CMatSystemSurface::GetFullScreenTexCoords( int x, int y, int w, int h, float *pMinU, float *pMinV, float *pMaxU, float *pMaxV )
{
	int nTexWidth = m_FullScreenBuffer->GetActualWidth();
	int nTexHeight = m_FullScreenBuffer->GetActualHeight();
	float flOOWidth = 1.0f / nTexWidth;
	float flOOHeight = 1.0f / nTexHeight;

	*pMinU = ( (float)x + 0.5f ) * flOOWidth;
	*pMinV = ( (float)y + 0.5f ) * flOOHeight;
	*pMaxU = ( (float)(x+w) - 0.5f ) * flOOWidth;
	*pMaxV = ( (float)(y+h) - 0.5f ) * flOOHeight;	
}


//-----------------------------------------------------------------------------
// Draws the fullscreen buffer into the panel
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawFullScreenBuffer( int nLeft, int nTop, int nRight, int nBottom, int nOffscreenWidth, int nOffscreenHeight, bool bIgnoreAlphaWhenCompositing )
{
	MAT_FUNC;

	// Draw a textured rectangle over the area
	if ( bIgnoreAlphaWhenCompositing )
	{
		if ( m_nFullScreenBufferMaterialIgnoreAlphaId == -1 )
		{
			m_nFullScreenBufferMaterialIgnoreAlphaId = CreateNewTextureID();
			DrawSetTextureMaterial( m_nFullScreenBufferMaterialIgnoreAlphaId, m_FullScreenBufferMaterialIgnoreAlpha );
		}
	}
	else
	{
		if ( m_nFullScreenBufferMaterialId == -1 )
		{
			m_nFullScreenBufferMaterialId = CreateNewTextureID();
			DrawSetTextureMaterial( m_nFullScreenBufferMaterialId, m_FullScreenBufferMaterial );
		}
	}	

	unsigned char oldColor[4];
	oldColor[0] = m_DrawColor[0];
	oldColor[1] = m_DrawColor[1];
	oldColor[2] = m_DrawColor[2];
	oldColor[3] = m_DrawColor[3];

	DrawSetColor( 255, 255, 255, 255 );

	DrawSetTexture( bIgnoreAlphaWhenCompositing ? m_nFullScreenBufferMaterialIgnoreAlphaId : m_nFullScreenBufferMaterialId );

	float u0, u1, v0, v1;
	GetFullScreenTexCoords( 0, 0, nOffscreenWidth, nOffscreenHeight, &u0, &v0, &u1, &v1 );
	DrawTexturedSubRect( nLeft, nTop, nRight, nBottom, u0, v0, u1, v1 );

	m_DrawColor[0] = oldColor[0];
	m_DrawColor[1] = oldColor[1];
	m_DrawColor[2] = oldColor[2];
	m_DrawColor[3] = oldColor[3];
}


//-----------------------------------------------------------------------------
// Draws a rectangle, setting z to the current value
//-----------------------------------------------------------------------------
float CMatSystemSurface::GetZPos() const
{
	return m_flZPos;
}


//-----------------------------------------------------------------------------
// Some drawing methods that cannot be accomplished under Win32
//-----------------------------------------------------------------------------
#define CIRCLE_POINTS		360

void CMatSystemSurface::DrawColoredCircle( int centerx, int centery, float radius, int r, int g, int b, int a )
{
	MAT_FUNC;

	Assert( g_bInDrawing );
	// Draw a circle
	int iDegrees = 0;
	Vector vecPoint, vecLastPoint(0,0,0);
	vecPoint.z = 0.0f;
	Color clr;
	clr.SetColor( r, g, b, a );
	DrawSetColor( clr );

	for ( int i = 0; i < CIRCLE_POINTS; i++ )
	{
		float flRadians = DEG2RAD( iDegrees );
		iDegrees += (360 / CIRCLE_POINTS);

		float ca = cos( flRadians );
		float sa = sin( flRadians );
					 
		// Rotate it around the circle
		vecPoint.x = centerx + (radius * sa);
		vecPoint.y = centery - (radius * ca);

		// Draw the point, if it's not on the previous point, to avoid smaller circles being brighter
		if ( vecLastPoint != vecPoint )
		{
			DrawFilledRect( vecPoint.x, vecPoint.y,  vecPoint.x + 1, vecPoint.y + 1 );
		}

		vecLastPoint = vecPoint;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draws colored text to a vgui panel
// Input  : *font - font to use
//			x - position of text
//			y - 
//			r - color of text
//			g - 
//			b - 
//			a - alpha ( 255 = opaque, 0 = transparent )
//			*fmt - va_* text string
//			... - 
// Output : int - horizontal # of pixels drawn
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawColoredText( vgui::HFont font, int x, int y, int r, int g, int b, int a, const char *fmt, va_list argptr )
{
	Assert( g_bInDrawing );
	int len;
	char data[1024];

	DrawSetTextPos( x, y );
	DrawSetTextColor( r, g, b, a );

	len = Q_vsnprintf(data, sizeof( data ), fmt, argptr);

	CMatRenderContextPtr prc( g_pMaterialSystem );

	DrawSetTextFont( font );

	wchar_t szconverted[ 1024 ];
	g_pVGuiLocalize->ConvertANSIToUnicode( data, szconverted, 1024 );
	DrawPrintText( szconverted, wcslen(szconverted ) );
}

void CMatSystemSurface::DrawColoredText( vgui::HFont font, int x, int y, int r, int g, int b, int a, const char *fmt, ... )
{
	va_list argptr;
	va_start( argptr, fmt );
	DrawColoredText( font, x, y, r, g, b, a, fmt, argptr );
	va_end(argptr);
}


//-----------------------------------------------------------------------------
// Draws text with current font at position and wordwrapped to the rect using color values specified
//-----------------------------------------------------------------------------
void CMatSystemSurface::SearchForWordBreak( vgui::HFont font, char *text, int& chars, int& pixels )
{
	chars = pixels = 0;
	while ( 1 )
	{
		char ch = text[ chars ];
		int a, b, c;
		GetCharABCwide( font, ch, a, b, c );

		if ( ch == 0 || ch <= 32 )
		{
			if ( ch == 32 && chars == 0 )
			{
				pixels += ( b + c );
				chars++;
			}
			break;
		}

		pixels += ( b + c );
		chars++;
	}
}

//-----------------------------------------------------------------------------
// Purpose: If text width is specified, reterns height of text at that width
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawTextHeight( vgui::HFont font, int w, int& h, char *fmt, ... )
{
	if ( !font )
		return;

	int len;
	char data[8192];

	va_list argptr;
	va_start( argptr, fmt );
	len = Q_vsnprintf( data, sizeof( data ), fmt, argptr );
	va_end( argptr );

	int x = 0;
	int y = 0;

	int ystep = GetFontTall( font );
	int startx = x;
	int endx = x + w;
	//int endy = y + h;
	int endy = 0;

	int chars = 0;
	int pixels = 0;
	for ( int i = 0 ; i < len; i += chars )
	{
		SearchForWordBreak( font, &data[ i ], chars, pixels );

		if ( data[ i ] == '\n' )
		{
			x = startx;
			y += ystep;
			chars = 1;
			continue;
		}

		if ( x + ( pixels ) >= endx )
		{
			x = startx;
			// No room even on new line!!!
			if ( x + pixels >= endx )
				break;

			y += ystep;
		}

		for ( int j = 0 ; j < chars; j++ )
		{
			int a, b, c;
			char ch = data[ i + j ];

			GetCharABCwide( font, ch, a, b, c );
	
			x += a + b + c;
		}
	}

	endy = y+ystep;

	h = endy;
}


//-----------------------------------------------------------------------------
// Draws text with current font at position and wordwrapped to the rect using color values specified
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawColoredTextRect( vgui::HFont font, int x, int y, int w, int h, int r, int g, int b, int a, const char *fmt, ... )
{
	MAT_FUNC;

	Assert( g_bInDrawing );
	if ( !font )
		return;

	int len;
	char data[8192];

	va_list argptr;
	va_start( argptr, fmt );
	len = Q_vsnprintf( data, sizeof( data ), fmt, argptr );
	va_end( argptr );

	DrawSetTextPos( x, y );
	DrawSetTextColor( r, g, b, a );
	DrawSetTextFont( font );

	int ystep = GetFontTall( font );
	int startx = x;
	int endx = x + w;
	int endy = y + h;

	int chars = 0;
	int pixels = 0;

	char word[ 512 ];
	char space[ 2 ];
	space[1] = 0;
	space[0] = ' ';

	for ( int i = 0 ; i < len; i += chars )
	{
		SearchForWordBreak( font, &data[ i ], chars, pixels );

		if ( data[ i ] == '\n' )
		{
			x = startx;
			y += ystep;
			chars = 1;
			continue;
		}

		if ( x + ( pixels ) >= endx )
		{
			x = startx;
			// No room even on new line!!!
			if ( x + pixels >= endx )
				break;

			y += ystep;
		}

		if ( y + ystep >= endy )
			break;


		if ( chars <= 0 )
			continue;

		Q_strncpy( word, &data[ i ], chars + 1 );

		DrawSetTextPos( x, y );

		wchar_t szconverted[ 1024 ];
		g_pVGuiLocalize->ConvertANSIToUnicode( word, szconverted, 1024 );
		DrawPrintText( szconverted, wcslen(szconverted ) );

		// Leave room for space, too
		x += DrawTextLen( font, word );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Determine length of text string
//-----------------------------------------------------------------------------
int	CMatSystemSurface::DrawTextLen( vgui::HFont font, const char *fmt, ... )
{
	va_list argptr;
	char data[1024];
	int len;

	va_start(argptr, fmt);
	len = Q_vsnprintf(data, sizeof( data ), fmt, argptr);
	va_end(argptr);

	int i;
	int x = 0;

	int a = 0, b = 0, c = 0;
	for ( i = 0 ; i < len; i++ )
	{
		GetCharABCwide( font, data[i], a, b, c );

		x += a + b + c;
	}

	return x;
}

//-----------------------------------------------------------------------------
// Disable clipping during rendering
//-----------------------------------------------------------------------------
void CMatSystemSurface::DisableClipping( bool bDisable )
{
	EnableScissor( !bDisable );
}


//-----------------------------------------------------------------------------
// Purpose: unlocks the cursor state
//-----------------------------------------------------------------------------
bool CMatSystemSurface::IsCursorLocked() const
{
	return ::IsCursorLocked();
}


//-----------------------------------------------------------------------------
// Sets the mouse Get + Set callbacks
//-----------------------------------------------------------------------------
void CMatSystemSurface::SetMouseCallbacks( GetMouseCallback_t GetFunc, SetMouseCallback_t SetFunc )
{
	// FIXME: Remove! This is obsolete
	Assert(0);
}


//-----------------------------------------------------------------------------
// Tells the surface to ignore windows messages
//-----------------------------------------------------------------------------
void CMatSystemSurface::EnableWindowsMessages( bool bEnable )
{
	if ( m_bEnableInput == bEnable )
		return;

	if ( bEnable )
	{
		g_pInputSystem->AddUIEventListener();
	}
	else
	{
		g_pInputSystem->RemoveUIEventListener();
	}

	m_bEnableInput = bEnable;
}

void CMatSystemSurface::MovePopupToFront(VPANEL panel)
{
	int index = m_PopupList.Find( panel );
	if ( index == m_PopupList.InvalidIndex() )
		return;

	if ( index != m_PopupList.Count() - 1 )
	{
		m_PopupList.Remove( index );
		m_PopupList.AddToTail( panel );

		if ( g_bSpewFocus )
		{
			char const *pName;
			pName = ipanel()->GetName( panel );
			Msg( "%s moved to front\n", pName ? pName : "(no name)" ); 
		}
	}

	// If the modal panel isn't a parent, restore it to the top, to prevent a hard lock
	if ( input()->GetAppModalSurface() )
	{
		if ( !g_pVGuiPanel->HasParent(panel, input()->GetAppModalSurface()) )
		{
			VPANEL p = input()->GetAppModalSurface();
			index = m_PopupList.Find( p );
			if ( index != m_PopupList.InvalidIndex() )
			{
				m_PopupList.Remove( index );
				m_PopupList.AddToTail( p );
			}
		}
	}

	ivgui()->PostMessage(panel, new KeyValues("OnMovedPopupToFront"), NULL);
}

void CMatSystemSurface::MovePopupToBack(VPANEL panel)
{
	int index = m_PopupList.Find( panel );
	if ( index == m_PopupList.InvalidIndex() )
	{
		return;
	}

	m_PopupList.Remove( index );
	m_PopupList.AddToHead( panel );
}


bool CMatSystemSurface::IsInThink( VPANEL panel)
{
	if ( m_bInThink )
	{
		if ( panel == m_CurrentThinkPanel ) // HasParent() returns true if you pass yourself in
		{
			return false;
		}

		return ipanel()->HasParent( panel, m_CurrentThinkPanel);
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CMatSystemSurface::IsCursorVisible()
{
	return (_currentCursor != dc_none);
}


bool CMatSystemSurface::IsTextureIDValid(int id)
{
	// FIXME:
	return true;
}

void CMatSystemSurface::SetAllowHTMLJavaScript( bool state )
{ 
	m_bAllowJavaScript = state; 
}

IHTML *CMatSystemSurface::CreateHTMLWindow(vgui::IHTMLEvents *events,VPANEL context)
{
#if defined( ENABLE_HTMLWINDOW )
	HtmlWindow *IE = new HtmlWindow(events,context,reinterpret_cast<HWND>( GetAttachedWindow() ), m_bAllowJavaScript, false);
	IE->Show(false);
	_htmlWindows.AddToTail(IE);
	return dynamic_cast<IHTML *>(IE);
#else
	Assert( 0 );
	return NULL;
#endif
}


void CMatSystemSurface::DeleteHTMLWindow(IHTML *htmlwin)
{
#if defined( ENABLE_HTMLWINDOW )
	HtmlWindow *IE =static_cast<HtmlWindow *>(htmlwin);

	if(IE)
	{
		_htmlWindows.FindAndRemove( IE );
		delete IE;
	}
#elif !defined( _X360 ) && !defined( _PS3 )
//#error "GameUI now NEEDS the HTML component!!"
#endif
}



void CMatSystemSurface::PaintHTMLWindow(IHTML *htmlwin)
{
#if defined( ENABLE_HTMLWINDOW )
	HtmlWindow *IE = static_cast<HtmlWindow *>(htmlwin);
	if(IE)
	{
		//HBITMAP bits;
		HDC hdc = ::GetDC(reinterpret_cast<HWND>( GetAttachedWindow() ));
		IE->OnPaint(hdc);
		::ReleaseDC( reinterpret_cast<HWND>( GetAttachedWindow() ), hdc );
	}
#endif
}

bool CMatSystemSurface::BHTMLWindowNeedsPaint(IHTML *htmlwin)
{
	return false;
}

//-----------------------------------------------------------------------------
/*void CMatSystemSurface::DrawSetTextureRGBA( int id, const unsigned char* rgba, int wide, int tall )
{
	TextureDictionary()->SetTextureRGBAEx( id, (const char *)rgba, wide, tall, IMAGE_FORMAT_RGBA8888 );
}*/

void CMatSystemSurface::DrawSetTextureRGBA( int id, const unsigned char* rgba, int wide, int tall )
{
	TextureDictionary()->SetTextureRGBAEx( id, (const char *)rgba, wide, tall, IMAGE_FORMAT_RGBA8888, k_ETextureScalingPointSample );
}

void CMatSystemSurface::DrawSetTextureRGBALinear( int id, const unsigned char *rgba, int wide, int tall )
{
	TextureDictionary()->SetTextureRGBAEx( id, (const char *)rgba, wide, tall, IMAGE_FORMAT_RGBA8888, k_ETextureScalingLinear );
}

void CMatSystemSurface::DrawSetTextureRGBAEx( int id, const unsigned char* rgba, int wide, int tall, ImageFormat format )
{
	TextureDictionary()->SetTextureRGBAEx( id, (const char *)rgba, wide, tall, format, k_ETextureScalingPointSample );
}

void CMatSystemSurface::DrawSetSubTextureRGBA( int textureID, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall )
{
	TextureDictionary()->SetSubTextureRGBA( textureID, drawX, drawY, rgba, subTextureWide, subTextureTall );
}

#if defined( _X360 )

//-----------------------------------------------------------------------------
// Purpose: Get the texture id for the local gamerpic.
//-----------------------------------------------------------------------------
int CMatSystemSurface::GetLocalGamerpicTextureID( void )
{
	return TextureDictionary()->GetLocalGamerpicTextureID();
}

//-----------------------------------------------------------------------------
// Purpose: Update the local gamerpic texture. Use the given texture if a gamerpic cannot be loaded.
//-----------------------------------------------------------------------------
bool CMatSystemSurface::SetLocalGamerpicTexture( DWORD userIndex, const char *pDefaultGamerpicFileName )
{
	return TextureDictionary()->SetLocalGamerpicTexture( userIndex, pDefaultGamerpicFileName );
}

//-----------------------------------------------------------------------------
// Purpose: Set the current texture to be the local gamerpic.
//-----------------------------------------------------------------------------
bool CMatSystemSurface::DrawSetTextureLocalGamerpic( void )
{
	int id = TextureDictionary()->GetLocalGamerpicTextureID();
	if ( id != INVALID_TEXTURE_ID )
	{
		DrawSetTexture( id );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Get the texture id for a remote gamerpic with the given xuid.
//-----------------------------------------------------------------------------
int CMatSystemSurface::GetRemoteGamerpicTextureID( XUID xuid )
{
	return TextureDictionary()->GetRemoteGamerpicTextureID( xuid );
}

//-----------------------------------------------------------------------------
// Purpose: Update the remote gamerpic texture for the given xuid. 
// Use the given texture if a gamerpic cannot be loaded.
//-----------------------------------------------------------------------------
bool CMatSystemSurface::SetRemoteGamerpicTextureID( XUID xuid, const char *pDefaultGamerpicFileName )
{
	return TextureDictionary()->SetRemoteGamerpicTextureID( xuid, pDefaultGamerpicFileName );
}

//-----------------------------------------------------------------------------
// Purpose: Set the current texture to be the remote player's gamerpic.
// Returns false if the remote gamerpic texture has not been set for the given xuid.
//-----------------------------------------------------------------------------
bool CMatSystemSurface::DrawSetTextureRemoteGamerpic( XUID xuid )
{
	int id = TextureDictionary()->GetRemoteGamerpicTextureID( xuid );
	if ( id != INVALID_TEXTURE_ID )
	{
		DrawSetTexture( id );
		return true;
	}

	return false;
}

#endif // _X360

void CMatSystemSurface::DrawUpdateRegionTextureRGBA( int nTextureID, int x, int y, const unsigned char *pchData, int wide, int tall, ImageFormat imageFormat )
{
	TextureDictionary()->UpdateSubTextureRGBA( nTextureID, x, y, pchData, wide, tall, imageFormat );
}

void CMatSystemSurface::SetModalPanel(VPANEL )
{
}

VPANEL CMatSystemSurface::GetModalPanel()
{
	return 0;
}

void CMatSystemSurface::UnlockCursor()
{
	::LockCursor( GetInputContext(), false );
}

void CMatSystemSurface::LockCursor()
{
	::LockCursor( GetInputContext(), true );
}

void CMatSystemSurface::SetTranslateExtendedKeys(bool state)
{
}

VPANEL CMatSystemSurface::GetTopmostPopup()
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: gets the absolute coordinates of the screen (in screen space)
//-----------------------------------------------------------------------------
void CMatSystemSurface::GetAbsoluteWindowBounds(int &x, int &y, int &wide, int &tall)
{
	// always work in full window screen space
	x = 0;
	y = 0;
	GetScreenSize(wide, tall);
}

// returns true if the specified panel is a child of the current modal panel
// if no modal panel is set, then this always returns TRUE
static bool IsChildOfModalSubTree(VPANEL panel)
{
	if ( !panel )
		return true;

	VPANEL modalSubTree = input()->GetModalSubTree();

	if ( modalSubTree )
	{
		bool restrictMessages = input()->ShouldModalSubTreeReceiveMessages();

		// If panel is child of modal subtree, the allow messages to route to it if restrict messages is set
		bool isChildOfModal = ipanel()->HasParent( panel, modalSubTree );
		if ( isChildOfModal )
		{
			return restrictMessages;
		}
		// If panel is not a child of modal subtree, then only allow messages if we're not restricting them to the modal subtree
		else
		{
			return !restrictMessages;
		}
	}

	return true;
}

void CMatSystemSurface::CalculateMouseVisible()
{
	int i;
	m_bNeedsMouse = false;
	m_bNeedsKeyboard = false;

	if ( input()->GetMouseCapture() != 0 )
		return;

	int c = surface()->GetPopupCount();

	VPANEL modalSubTree = input()->GetModalSubTree();
	if ( modalSubTree )
	{
		m_bNeedsMouse = input()->ShouldModalSubTreeShowMouse();

		for (i = 0 ; i < c ; i++ )
		{
			VPanel *pop = (VPanel *)surface()->GetPopup(i) ;
			bool isChildOfModalSubPanel = IsChildOfModalSubTree( (VPANEL)pop );
			if ( !isChildOfModalSubPanel )
				continue;

			bool isVisible=pop->IsVisible();
			VPanel *p= pop->GetParent();

			while (p && isVisible)
			{
				if( p->IsVisible()==false)
				{
					isVisible=false;
					break;
				}
				p=p->GetParent();
			}

			if ( isVisible )
			{
				m_bNeedsMouse = m_bNeedsMouse || pop->IsMouseInputEnabled();
				m_bNeedsKeyboard = m_bNeedsKeyboard || pop->IsKeyBoardInputEnabled();

				// Seen enough!!!
				if ( m_bNeedsMouse && m_bNeedsKeyboard )
					break;
			}
		}
	}
	else
	{
		for (i = 0 ; i < c ; i++ )
		{
			VPanel *pop = (VPanel *)surface()->GetPopup(i) ;
			
			bool isVisible=pop->IsVisible();
			VPanel *p= pop->GetParent();

			while (p && isVisible)
			{
				if( p->IsVisible()==false)
				{
					isVisible=false;
					break;
				}
				p=p->GetParent();
			}
		
			if ( isVisible )
			{
				m_bNeedsMouse = m_bNeedsMouse || pop->IsMouseInputEnabled();
				m_bNeedsKeyboard = m_bNeedsKeyboard || pop->IsKeyBoardInputEnabled();

				// Seen enough!!!
				if ( m_bNeedsMouse && m_bNeedsKeyboard )
					break;
			}
		}
	}
	
	// [jason] If we don't handle windows input event messages, ensure that we UnlockCursor:
	//	this is used so that in-game Scaleform can claim mouse focus on PC and have the mouse move around
#if defined( CSTRIKE15 )
	if ( IsPC() && !m_bNeedsMouse )
	{
		m_bNeedsMouse = !m_bEnableInput;
	}
#endif // defined( CSTRIKE15 )

	if (m_bNeedsMouse)
	{
		g_pInputStackSystem->EnableInputContext( GetInputContext(), true );

		// NOTE: We must unlock the cursor *before* the set call here.
		// Failing to do this causes s_bCursorVisible to not be set correctly
		// (UnlockCursor fails to set it correctly)
		UnlockCursor();
		if ( _currentCursor == vgui::dc_none )
		{
			SetCursor(vgui::dc_arrow);
		}
	}
	else
	{
		g_pInputStackSystem->EnableInputContext( GetInputContext(), false );

		SetCursor(vgui::dc_none);
		LockCursor();
	}
}

bool CMatSystemSurface::NeedKBInput()
{
	return m_bNeedsKeyboard;
}

void CMatSystemSurface::SurfaceGetCursorPos(int &x, int &y)
{
	GetCursorPos( x, y );
}
void CMatSystemSurface::SurfaceSetCursorPos(int x, int y)
{
	SetCursorPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose: global alpha setting functions
//-----------------------------------------------------------------------------
void CMatSystemSurface::DrawSetAlphaMultiplier( float alpha /* [0..1] */ )
{
	m_flAlphaMultiplier = clamp(alpha, 0.0f, 1.0f);
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
float CMatSystemSurface::DrawGetAlphaMultiplier()
{
	return m_flAlphaMultiplier;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *curOrAniFile - 
// Output : vgui::HCursor
//-----------------------------------------------------------------------------
vgui::HCursor CMatSystemSurface::CreateCursorFromFile( char const *curOrAniFile, char const *pPathID )
{
	return Cursor_CreateCursorFromFile( curOrAniFile, pPathID );
}

void CMatSystemSurface::SetPanelForInput( VPANEL vpanel )
{
	g_pIInput->AssociatePanelWithInputContext( DEFAULT_INPUT_CONTEXT, vpanel );
	if ( vpanel )
	{
		m_bNeedsKeyboard = true;
	}
	else
	{
		m_bNeedsKeyboard = false;
	}
}

#if defined( WIN32 ) && !defined( _X360 )
static bool GetIconSize( ICONINFO& iconInfo, int& w, int& h )
{
	w = h = 0;

	HBITMAP bitmap = iconInfo.hbmColor;
	BITMAP bm;
	if ( 0 == GetObject((HGDIOBJ)bitmap, sizeof(BITMAP), (LPVOID)&bm) ) 
	{
        return false; 
	}

	w = bm.bmWidth;
	h = bm.bmHeight;

	return true;
}

// If rgba is NULL, bufsize gets filled in w/ # of bytes required
static bool GetIconBits( HDC hdc, ICONINFO& iconInfo, int& w, int& h, unsigned char *rgba, size_t& bufsize )
{
	if ( !iconInfo.hbmColor || !iconInfo.hbmMask )
		return false;

	if ( !rgba )
	{
		if ( !GetIconSize( iconInfo, w, h ) )
			return false;
		
		bufsize = (size_t)( ( w * h ) << 2 );
		return true;
	}

	bool bret = false;

	Assert( w > 0 );
	Assert( h > 0 );
	Assert( bufsize == (size_t)( ( w * h ) << 2 ) );

	DWORD *maskData = new DWORD[ w * h ];
	DWORD *colorData =  new DWORD[ w * h ];
	DWORD *output = (DWORD *)rgba;

	BITMAPINFO bmInfo;

	memset( &bmInfo, 0, sizeof( bmInfo ) );
	bmInfo.bmiHeader.biSize = sizeof( bmInfo.bmiHeader );
	bmInfo.bmiHeader.biWidth = w; 
    bmInfo.bmiHeader.biHeight = h; 
    bmInfo.bmiHeader.biPlanes = 1; 
    bmInfo.bmiHeader.biBitCount = 32; 
    bmInfo.bmiHeader.biCompression = BI_RGB; 

	// Get the info about the bits
	if ( GetDIBits( hdc, iconInfo.hbmMask, 0, h, maskData, &bmInfo, DIB_RGB_COLORS ) == h &&
         GetDIBits( hdc, iconInfo.hbmColor, 0, h, colorData, &bmInfo, DIB_RGB_COLORS ) == h )
	{
		bret = true;

		for ( int row = 0; row < h; ++row )
		{
			// Invert
			int r = ( h - row - 1 );
			int rowstart = r * w;

			DWORD *color = &colorData[ rowstart ];
			DWORD *mask = &maskData[ rowstart ];
			DWORD *outdata = &output[ row * w ];

			for ( int col = 0; col < w; ++col )
			{
				unsigned char *cr = ( unsigned char * )&color[ col ];

				// Set alpha
				cr[ 3 ] =  mask[ col ] == 0 ? 0xff : 0x00;
				
				// Swap blue and red
				unsigned char t = cr[ 2 ];
				cr[ 2 ] = cr[ 0 ];
				cr[ 0 ] = t;

				*( unsigned int *)&outdata[ col ] = *( unsigned int * )cr;
			}
		}
	}

	delete[] colorData;
	delete[] maskData;

	return bret;
}

static char const *g_pUniqueExtensions[]=
{
	"exe",
	"cur",
	"ani",
};

static bool ShouldMakeUnique( char const *extension )
{
	for ( int i = 0; i < ARRAYSIZE( g_pUniqueExtensions ); ++i )
	{
		if ( !Q_stricmp( extension, g_pUniqueExtensions[ i ] ) )
			return true;
	}

	return false;
}
#endif // !_X360

vgui::IImage *CMatSystemSurface::GetIconImageForFullPath( char const *pFullPath )
{
	vgui::IImage *newIcon = NULL;

#if defined( WIN32 ) && !defined( _X360 )
	SHFILEINFO info = { 0 };
	DWORD_PTR dwResult = SHGetFileInfo( 
		pFullPath,
		0,
		&info,
		sizeof( info ),
		SHGFI_TYPENAME | SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE 
	);
	if ( dwResult )
	{
		if ( info.szTypeName[ 0 ] != 0 )
		{
			char ext[ 32 ];
			Q_ExtractFileExtension( pFullPath, ext, sizeof( ext ) );

			char lookup[ 512 ];
			Q_snprintf( lookup, sizeof( lookup ), "%s", ShouldMakeUnique( ext ) ? pFullPath : info.szTypeName );
			
			// Now check the dictionary
			unsigned short idx = m_FileTypeImages.Find( lookup );
			if ( idx == m_FileTypeImages.InvalidIndex() )
			{
				ICONINFO iconInfo;
				if ( 0 != GetIconInfo( info.hIcon, &iconInfo ) )
				{
					int w, h;
					size_t bufsize = 0;
					
					HDC hdc = ::GetDC(reinterpret_cast<HWND>( GetAttachedWindow() ));

					if ( GetIconBits( hdc, iconInfo, w, h, NULL, bufsize ) )
					{
						byte *bits = new byte[ bufsize ];
						if ( bits && GetIconBits( hdc, iconInfo, w, h, bits, bufsize ) )
						{
							newIcon = new MemoryBitmap( bits, w, h );
						}
						delete[] bits;
					}

					::ReleaseDC( reinterpret_cast<HWND>( GetAttachedWindow() ), hdc );
				}

				idx = m_FileTypeImages.Insert( lookup, newIcon );
			}

			newIcon = m_FileTypeImages[ idx ];
		}

		DestroyIcon( info.hIcon );
	}
#endif
	return newIcon;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::Set3DPaintTempRenderTarget( const char *pRenderTargetName )
{
	MAT_FUNC;

	Assert( !m_bUsingTempFullScreenBufferMaterial );
	m_bUsingTempFullScreenBufferMaterial = true;

	InitFullScreenBuffer( pRenderTargetName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMatSystemSurface::Reset3DPaintTempRenderTarget( void )
{
	MAT_FUNC;

	Assert( m_bUsingTempFullScreenBufferMaterial );
	m_bUsingTempFullScreenBufferMaterial = false;

	InitFullScreenBuffer( MODEL_PANEL_RT_NAME );
}

void CMatSystemSurface::SetAbsPosForContext( int id, int x, int y )
{
	ContextAbsPos_t search;
	search.id = id;

	int idx = m_ContextAbsPos.Find( search );
	if ( idx == m_ContextAbsPos.InvalidIndex() )
	{
		idx = m_ContextAbsPos.Insert( search );
	}

	ContextAbsPos_t &entry = m_ContextAbsPos[ idx ];
	entry.m_nPos[ 0 ] = x;
	entry.m_nPos[ 1 ] = y;
}

void CMatSystemSurface::GetAbsPosForContext( int id, int &x, int& y )
{
	ContextAbsPos_t search;
	search.id = id;

	int idx = m_ContextAbsPos.Find( search );
	if ( idx == m_ContextAbsPos.InvalidIndex() )
	{
		x = y = 0;
		return;
	}
	const ContextAbsPos_t &entry = m_ContextAbsPos[ idx ];
	x = entry.m_nPos[ 0 ];
	y = entry.m_nPos[ 1 ];
}

int CMatSystemSurface::GetTextureNumFrames( int id )
{
	IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial( id );
	if ( !pMaterial )
	{
		return 0;
	}
	return pMaterial->GetNumAnimationFrames();
}

void CMatSystemSurface::DrawSetTextureFrame( int id, int nFrame, unsigned int *pFrameCache )
{
	IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial( id );
	if ( !pMaterial )
	{
		return;
	}

	int nTotalFrames = pMaterial->GetNumAnimationFrames();
	if ( !nTotalFrames )
	{
		return;
	}

	IMaterialVar *pFrameVar = pMaterial->FindVarFast( "$frame", pFrameCache );
	if ( pFrameVar )
	{
		pFrameVar->SetIntValue( nFrame % nTotalFrames );
	}
}

void CMatSystemSurface::GetClipRect( int &x0, int &y0, int &x1, int &y1 )
{
	if ( !m_PaintStateStack.Count() )
		return;

	PaintState_t &paintState = m_PaintStateStack.Tail();

	x0 = paintState.m_iScissorLeft;
	y0 = paintState.m_iScissorTop;
	x1 = paintState.m_iScissorRight;
	y1 = paintState.m_iScissorBottom;
}

void CMatSystemSurface::SetClipRect( int x0, int y0, int x1, int y1 )
{
	if ( !m_PaintStateStack.Count() )
		return;

	PaintState_t &paintState = m_PaintStateStack.Tail();

	paintState.m_iScissorLeft	= x0;
	paintState.m_iScissorTop	= y0;
	paintState.m_iScissorRight	= x1;
	paintState.m_iScissorBottom	= y1;

	SetupPaintState( paintState );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CMatSystemSurface::SetLanguage( const char *pLanguage )
{ 
	FontManager().SetLanguage( pLanguage );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CMatSystemSurface::GetLanguage()
{ 
	return FontManager().GetLanguage();
}

void CMatSystemSurface::DrawTexturedRectEx( DrawTexturedRectParms_t *pDrawParms )
{
	Assert( g_bInDrawing );
	Assert( !m_bIn3DPaintMode );

	// Don't even bother drawing fully transparent junk
	if ( m_DrawColor[3] == 0 )
		return;

	// scale incoming alpha
	int alpha_ul =  255.0f * ( (float)m_DrawColor[3]/255.0f * (float)pDrawParms->alpha_ul/255.0f );
	int alpha_ur =  255.0f * ( (float)m_DrawColor[3]/255.0f * (float)pDrawParms->alpha_ur/255.0f );
	int alpha_lr =  255.0f * ( (float)m_DrawColor[3]/255.0f * (float)pDrawParms->alpha_lr/255.0f );
	int alpha_ll =  255.0f * ( (float)m_DrawColor[3]/255.0f * (float)pDrawParms->alpha_ll/255.0f );

	// Don't even bother drawing fully transparent junk
	if ( alpha_ul == 0 && alpha_ur == 0 && alpha_lr == 0 && alpha_ll == 0 )
		return;

	float s0, t0, s1, t1;
	TextureDictionary()->GetTextureTexCoords( m_iBoundTexture, s0, t0, s1, t1 );

	float ssize = s1 - s0;
	float tsize = t1 - t0;

	// Rescale tex values into range of s0 to s1 ,etc.
	float texs0 = s0 + pDrawParms->s0 * ( ssize );
	float texs1 = s0 + pDrawParms->s1 * ( ssize );
	float text0 = t0 + pDrawParms->t0 * ( tsize );
	float text1 = t0 + pDrawParms->t1 * ( tsize );

	// rotate about center
	float cx = ( pDrawParms->x0 + pDrawParms->x1 ) / 2.0f;
	float cy = ( pDrawParms->y0 + pDrawParms->y1 ) / 2.0f;

	IMaterial *pMaterial = TextureDictionary()->GetTextureMaterial( m_iBoundTexture );
	InternalSetMaterial( pMaterial );
	if ( !m_pMesh )
		return;

	meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, 1 );

	// ul
	Vector in, out;
	in.x = pDrawParms->x0 - cx;
	in.y = pDrawParms->y0 - cy;
	in.z = 0;
	VectorYawRotate( in, pDrawParms->angle, out );
	out.x += cx + m_nTranslateX;
	out.y += cy + m_nTranslateY;
	meshBuilder.Position3f( out.x, out.y, m_flZPos );
	meshBuilder.Color4ub( m_DrawColor[0], m_DrawColor[1], m_DrawColor[2], alpha_ul );
	meshBuilder.TexCoord2f( 0, texs0, text0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	// ur
	in.x = pDrawParms->x1 - cx;
	in.y = pDrawParms->y0 - cy;
	VectorYawRotate( in, pDrawParms->angle, out );
	out.x += cx + m_nTranslateX;
	out.y += cy + m_nTranslateY;
	meshBuilder.Position3f( out.x, out.y, m_flZPos );
	meshBuilder.Color4ub( m_DrawColor[0], m_DrawColor[1], m_DrawColor[2], alpha_ur );
	meshBuilder.TexCoord2f( 0, texs1, text0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	// lr
	in.x = pDrawParms->x1 - cx;
	in.y = pDrawParms->y1 - cy;
	VectorYawRotate( in, pDrawParms->angle, out );
	out.x += cx + m_nTranslateX;
	out.y += cy + m_nTranslateY;
	meshBuilder.Position3f( out.x, out.y, m_flZPos );
	meshBuilder.Color4ub( m_DrawColor[0], m_DrawColor[1], m_DrawColor[2], alpha_lr );
	meshBuilder.TexCoord2f( 0, texs1, text1 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	// ll
	in.x = pDrawParms->x0 - cx;
	in.y = pDrawParms->y1 - cy;
	VectorYawRotate( in, pDrawParms->angle, out );
	out.x += cx + m_nTranslateX;
	out.y += cy + m_nTranslateY;
	meshBuilder.Position3f( out.x, out.y, m_flZPos );
	meshBuilder.Color4ub( m_DrawColor[0], m_DrawColor[1], m_DrawColor[2], alpha_ll );
	meshBuilder.TexCoord2f( 0, texs0, text1 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
	meshBuilder.End();
	m_pMesh->Draw();
}
