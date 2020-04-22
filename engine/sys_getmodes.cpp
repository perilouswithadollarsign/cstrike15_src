//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#if defined( USE_SDL )
#undef PROTECTED_THINGS_ENABLE
#include "SDL.h"
#include "SDL_syswm.h"
#endif

#if defined( _WIN32 ) && !defined( _X360 )
#include "winlite.h"
#elif defined(POSIX)
	#ifdef OSX
		#include <Carbon/Carbon.h>
	#endif
typedef void *HDC;
#endif

#include "appframework/ilaunchermgr.h"

#include "basetypes.h"
#include "sysexternal.h"
#include "cmd.h"
#include "modelloader.h"
#include "gl_matsysiface.h"
#include "vmodes.h"
#include "modes.h"
#include "ivideomode.h"
#include "igame.h"
#include "iengine.h"
#include "engine_launcher_api.h"
#include "iregistry.h"
#include "common.h"
#include "tier0/icommandline.h"
#include "cl_main.h"
#include "filesystem_engine.h"
#include "host.h"
#include "gl_model_private.h"
#include "bitmap/tgawriter.h"
#include "vtf/vtf.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "jpeglib/jpeglib.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "vgui_controls/Controls.h"
#include "gl_shader.h"
#include "sys_dll.h"
#include "materialsystem/imaterial.h"
#include "IHammer.h"
#include "avi/iavi.h"
#include "tier2/tier2.h"
#include "tier2/renderutils.h"
#include "LoadScreenUpdate.h"
#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_launch.h"
#else
#include "xbox/xboxstubs.h"
#endif
#if !defined(NO_STEAM)
#include "cl_steamauth.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_savescreenshotstosteam( "cl_savescreenshotstosteam", "0", FCVAR_HIDDEN, "Saves screenshots to the Steam's screenshot library" );
ConVar cl_screenshotusertag( "cl_screenshotusertag", "", FCVAR_HIDDEN, "User to tag in the screenshot" );
ConVar cl_screenshotlocation( "cl_screenshotlocation", "", FCVAR_HIDDEN, "Location to tag the screenshot with" );

//-----------------------------------------------------------------------------
// HDRFIXME: move this somewhere else.
//-----------------------------------------------------------------------------
static void PFMWrite( float *pFloatImage, const char *pFilename, int width, int height )
{
    FileHandle_t fp;
    fp = g_pFileSystem->Open( pFilename, "wb" );
    g_pFileSystem->FPrintf( fp, "PF\n%d %d\n-1.000000\n", width, height );
    int i;
    for( i = height-1; i >= 0; i-- )
    {
        float *pRow = &pFloatImage[3 * width * i];
        g_pFileSystem->Write( pRow, width * sizeof( float ) * 3, fp );
    }
    g_pFileSystem->Close( fp );
}

//-----------------------------------------------------------------------------
// Purpose: Functionality shared by all video modes
//-----------------------------------------------------------------------------
class CVideoMode_Common : public IVideoMode
{
public:
						CVideoMode_Common( void );
	virtual 			~CVideoMode_Common( void );

	// Methods of IVideoMode
	virtual bool		Init( );
	virtual void		Shutdown( void );
	virtual vmode_t		*GetMode( int num );
	virtual int			GetModeCount( void );
	virtual bool		IsWindowedMode( void ) const;
	virtual	bool		NoWindowBorder() const;
	virtual void		UpdateWindowPosition( void );
	virtual void		RestoreVideo( void );
	virtual void		ReleaseVideo( void );
	virtual void		DrawNullBackground( void *hdc, int w, int h );
	virtual void		InvalidateWindow();
	virtual void		DrawStartupGraphic();
	virtual bool		CreateGameWindow( int nWidth, int nHeight, bool bWindowed, bool bNoWindowBorder );
	virtual int			GetModeWidth( void ) const;
	virtual int			GetModeHeight( void ) const;
	virtual const vrect_t &GetClientViewRect( ) const;
	virtual void		SetClientViewRect( const vrect_t &viewRect );
	virtual void		MarkClientViewRectDirty();
	virtual void		TakeSnapshotTGA( const char *pFileName );
	virtual void		TakeSnapshotTGARect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, bool bPFM, CubeMapFaceIndex_t faceIndex );
	virtual void		WriteMovieFrame( const MovieInfo_t& info );
	virtual void		TakeSnapshotJPEG( const char *pFileName, int quality );
	virtual bool		TakeSnapshotJPEGToBuffer( CUtlBuffer& buf, int quality );
protected:
	bool				GetInitialized( ) const;
	void				SetInitialized( bool init );
	void				AdjustWindow( int nWidth, int nHeight, int nBPP, bool bWindowed, bool bNoWindowBorder );
	void				ResetCurrentModeForNewResolution( int width, int height, bool bWindowed, bool bNoWindowBorder );
	int					GetModeBPP( ) const { return 32; }
	void				DrawStartupVideo();
	void				ComputeStartupGraphicName( char *pBuf, int nBufLen );
	void				WriteScreenshotToSteam( uint8 *pImage, int cubImage, int width, int height );
	void				AddScreenshotToSteam( const char *pchFilenameJpeg, int width, int height );
#if !defined(NO_STEAM)
	void				ApplySteamScreenshotTags( ScreenshotHandle hScreenshot );
#endif

	// Finds the video mode in the list of video modes 
	int					FindVideoMode( int nDesiredWidth, int nDesiredHeight, bool bWindowed );

	// Purpose: Returns the optimal refresh rate for the specified mode
	int					GetRefreshRateForMode( const vmode_t *pMode );

	// Inline accessors
	vmode_t&			DefaultVideoMode();
	vmode_t&			RequestedWindowVideoMode();

private:
    // Purpose: Loads the startup graphic
    bool                SetupStartupGraphic();
    void                CenterEngineWindow(void *hWndCenter, int width, int height);
    void                DrawStartupGraphic( HWND window );
    void                BlitGraphicToHDC(HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1);
    void                BlitGraphicToHDCWithAlpha(HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1);
    IVTFTexture         *LoadVTF( CUtlBuffer &temp, const char *szFileName );
    void                RecomputeClientViewRect();

    // Overridden by derived classes
    virtual void        ReleaseFullScreen( void );
    virtual void        ChangeDisplaySettingsToFullscreen( int nWidth, int nHeight, int nBPP, bool bDesktopFriendlyFullscreen );
    virtual void        ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format );

    // PFM screenshot methods
    ITexture *GetBuildCubemaps16BitTexture( void );
    ITexture *GetFullFrameFB0( void );

    void BlitHiLoScreenBuffersTo16Bit( void );
    void TakeSnapshotPFMRect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, CubeMapFaceIndex_t faceIndex );

protected:
	enum
	{
#if !defined( _X360 )
		MAX_MODE_LIST =	512
#else
		MAX_MODE_LIST =	2
#endif
	};

	enum
	{
		VIDEO_MODE_DEFAULT = -1,
		VIDEO_MODE_REQUESTED_WINDOW_SIZE = -2,
		CUSTOM_VIDEO_MODES = 2
	};

	// Master mode list
	int					m_nNumModes;
	vmode_t				m_rgModeList[MAX_MODE_LIST];
	vmode_t				m_nCustomModeList[CUSTOM_VIDEO_MODES];
	bool				m_bInitialized;
 	bool				m_bPlayedStartupVideo;

	// Renderable surface information
	int					m_nModeWidth;
	int					m_nModeHeight;
#if defined( USE_SDL )
	int					m_nRenderWidth;
	int					m_nRenderHeight;
#endif
 	bool				m_bWindowed;
	bool				m_bNoWindowBorder;
	bool				m_bSetModeOnce;

	// Client view rectangle
	vrect_t				m_ClientViewRect;
	bool				m_bClientViewRectDirty;

	// loading image
	IVTFTexture			*m_pBackgroundTexture;
	IVTFTexture			*m_pLoadingTexture;
	IVTFTexture			*m_pTitleTexture;
};


//-----------------------------------------------------------------------------
// Inline accessors
//-----------------------------------------------------------------------------
inline vmode_t& CVideoMode_Common::DefaultVideoMode()
{
    return m_nCustomModeList[ - VIDEO_MODE_DEFAULT - 1 ];
}

inline vmode_t& CVideoMode_Common::RequestedWindowVideoMode()
{
    return m_nCustomModeList[ - VIDEO_MODE_REQUESTED_WINDOW_SIZE - 1 ];
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVideoMode_Common::CVideoMode_Common( void )
{
    m_nNumModes    = 0;
    m_bInitialized = false;

	if ( IsOSX() )
	{
		DefaultVideoMode().width  = 1024;
		DefaultVideoMode().height = 768;
	}
	else 
	{
		DefaultVideoMode().width  = 640;
		DefaultVideoMode().height = 480;
	}

    DefaultVideoMode().bpp    = 32;
    DefaultVideoMode().refreshRate = 0;

    RequestedWindowVideoMode().width  = -1;
    RequestedWindowVideoMode().height = -1;
    RequestedWindowVideoMode().bpp    = 32;
    RequestedWindowVideoMode().refreshRate = 0;
    
    m_bClientViewRectDirty = false;
    m_pBackgroundTexture   = NULL;
    m_pLoadingTexture      = NULL;
	m_pTitleTexture        = NULL;
    m_bWindowed            = false;
    m_nModeWidth           = IsPC() ? 1024 : 640;
    m_nModeHeight          = IsPC() ? 768 : 480;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CVideoMode_Common::~CVideoMode_Common( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVideoMode_Common::GetInitialized( void ) const
{
    return m_bInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : init - 
//-----------------------------------------------------------------------------
void CVideoMode_Common::SetInitialized( bool init )
{
    m_bInitialized = init;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVideoMode_Common::IsWindowedMode( void ) const
{
	return m_bWindowed;
}

bool CVideoMode_Common::NoWindowBorder() const
{
	return m_bNoWindowBorder;
}


//-----------------------------------------------------------------------------
// Returns the video mode width + height.
//-----------------------------------------------------------------------------
int CVideoMode_Common::GetModeWidth( void ) const
{
    return m_nModeWidth;
}

int CVideoMode_Common::GetModeHeight( void ) const
{
    return m_nModeHeight;
}


//-----------------------------------------------------------------------------
// Returns the enumerated video mode
//-----------------------------------------------------------------------------
vmode_t *CVideoMode_Common::GetMode( int num )
{
    if ( num < 0 )
        return &m_nCustomModeList[-num - 1];

    if ( num >= m_nNumModes )
        return &DefaultVideoMode();

    return &m_rgModeList[num];
}


//-----------------------------------------------------------------------------
// Returns the number of fullscreen video modes 
//-----------------------------------------------------------------------------
int CVideoMode_Common::GetModeCount( void )
{
    return m_nNumModes;
}


//-----------------------------------------------------------------------------
// Purpose: Compares video modes so we can sort the list
// Input  : *arg1 - 
//          *arg2 - 
// Output : static int
//-----------------------------------------------------------------------------
static int __cdecl VideoModeCompare( const void *arg1, const void *arg2 )
{
    vmode_t *m1, *m2;

    m1 = (vmode_t *)arg1;
    m2 = (vmode_t *)arg2;

    if ( m1->width < m2->width )
    {
        return -1;
    }

    if ( m1->width == m2->width )
    {
        if ( m1->height < m2->height )
        {
            return -1;
        }

        if ( m1->height > m2->height )
        {
            return 1;
        }

        return 0;
    }

    return 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CVideoMode_Common::Init( )
{   
    return true;
}


//-----------------------------------------------------------------------------
// Finds the video mode in the list of video modes 
//-----------------------------------------------------------------------------
int CVideoMode_Common::FindVideoMode( int nDesiredWidth, int nDesiredHeight, bool bWindowed )
{
#if defined( USE_SDL )

	// If we want to scale the 3D portion of the game and leave the UI at the same res, then
	//	re-enable this code. Not that on retina displays the UI will be super small and that
	//	should probably be fixed.
#if 0
	static ConVarRef mat_viewportscale( "mat_viewportscale" );

	if ( !bWindowed )
	{
		m_nRenderWidth = nDesiredWidth;
		m_nRenderHeight = nDesiredHeight;

		uint nWidth, nHeight, nRefreshHz;

		g_pLauncherMgr->GetNativeDisplayInfo( -1, nWidth, nHeight, nRefreshHz );

		for ( int i = 0; i < m_nNumModes; i++)
		{
			if ( m_rgModeList[i].width != ( int )nWidth )
			{
				continue;
			}

			if ( m_rgModeList[i].height != ( int )nHeight )
			{
				continue;
			}

			if ( m_rgModeList[i].refreshRate != ( int )nRefreshHz )
			{
				continue;
			}

			mat_viewportscale.SetValue( ( float )nDesiredWidth / ( float )nWidth );
			return i;
		}

		Assert( 0 );	// we should have found our native resolution, why not???
	}
	else
	{
		mat_viewportscale.SetValue( 1.0f );
	}
#endif // 0

#endif // USE_SDL

    // Check the default window size..
    if ( ( nDesiredWidth == DefaultVideoMode().width) && (nDesiredHeight == DefaultVideoMode().height) )
        return VIDEO_MODE_DEFAULT;

    // Check the requested window size, but only if we're running windowed
    if ( bWindowed )
    {
        if ( ( nDesiredWidth == RequestedWindowVideoMode().width) && (nDesiredHeight == RequestedWindowVideoMode().height) )
            return VIDEO_MODE_REQUESTED_WINDOW_SIZE;
    }

    int i;
    int iOK = VIDEO_MODE_DEFAULT;
    for ( i = 0; i < m_nNumModes; i++)
    {
        // Match width first
        if ( m_rgModeList[i].width != nDesiredWidth )
            continue;
        
        iOK = i;

        if ( m_rgModeList[i].height != nDesiredHeight )
            continue;

        // Found a decent match
        break;
    }

    // No match, use mode 0
    if ( i >= m_nNumModes )
    {
        if ( ( iOK != VIDEO_MODE_DEFAULT ) && !bWindowed )
        {
            i = iOK;
        }
        else
        {
            i = 0;
        }
    }

    return i;
}


//-----------------------------------------------------------------------------
// Choose the actual video mode based on the available modes
//-----------------------------------------------------------------------------
void CVideoMode_Common::ResetCurrentModeForNewResolution( int nWidth, int nHeight, bool bWindowed, bool bNoWindowBorder )
{
	// Fill in vid structure for the mode
	int nFoundMode = FindVideoMode( nWidth, nHeight, bWindowed );
	vmode_t videoMode = *GetMode( nFoundMode );

	if ( bWindowed && ( nFoundMode == 0 ) && ( videoMode.width != nWidth || videoMode.height != nHeight ) )
	{
		// Setting a custom windowed size
		videoMode.width = nWidth;
		videoMode.height = nHeight;
	}

	m_bWindowed = bWindowed;
	m_nModeWidth = videoMode.width;
	m_nModeHeight = videoMode.height;
	m_bNoWindowBorder = bNoWindowBorder;
}


//-----------------------------------------------------------------------------
// Creates the game window, plays the startup movie
//-----------------------------------------------------------------------------
bool CVideoMode_Common::CreateGameWindow( int nWidth, int nHeight, bool bWindowed, bool bNoWindowBorder )
{
    COM_TimestampedLog( "CVideoMode_Common::Init  CreateGameWindow" );

    // This allows you to have a window of any size.
    // Requires you to set both width and height for the window and
    // that you start in windowed mode
    if ( bWindowed && nWidth && nHeight )
    {
        // FIXME: There's some ordering issues related to the config record
        // and reading the command-line. Would be nice for just one place where this is done.
        RequestedWindowVideoMode().width = nWidth;
        RequestedWindowVideoMode().height = nHeight;
    }
    
    if ( !InEditMode() )
    {
        // Fill in vid structure for the mode.
        // Note: ModeWidth/Height may *not* match requested nWidth/nHeight
		ResetCurrentModeForNewResolution( nWidth, nHeight, bWindowed, bNoWindowBorder );

		COM_TimestampedLog( "CreateGameWindow - Start" );

		if( IsPS3QuitRequested() )
			return false;

        // When running in stand-alone mode, create your own window 
        if ( !game->CreateGameWindow() )
            return false;

		COM_TimestampedLog( "CreateGameWindow - Finish" );

		if( IsPS3QuitRequested() )
			return false;

        // Re-size and re-center the window
		AdjustWindow( GetModeWidth(), GetModeHeight(), GetModeBPP(), IsWindowedMode(), NoWindowBorder() );

		COM_TimestampedLog( "SetMode - Start" );

        // Set the mode and let the materialsystem take over
		if ( !SetMode( GetModeWidth(), GetModeHeight(), IsWindowedMode(), NoWindowBorder() ) )
            return false;

#if defined( USE_SDL ) && 0
		static ConVarRef mat_viewportscale( "mat_viewportscale" );

		if ( !bWindowed )
		{
			m_nRenderWidth = nWidth;
			m_nRenderHeight = nHeight;

			mat_viewportscale.SetValue(  ( float )nWidth / ( float )GetModeWidth() );
		}
#endif

		if( IsPS3QuitRequested() )
			return false;

		COM_TimestampedLog( "DrawStartupGraphic - Start" );

        // Display the image for the background during the remainder of startup
        DrawStartupGraphic();

		if( IsPS3QuitRequested() )
			return false;

		COM_TimestampedLog( "DrawStartupGraphic - Finish" );
    }

    return true;
}

//-----------------------------------------------------------------------------
// Purpose: loads a vtf, through the temporary buffer
//-----------------------------------------------------------------------------
IVTFTexture *CVideoMode_Common::LoadVTF( CUtlBuffer &temp, const char *szFileName )
{
    if ( !g_pFileSystem->ReadFile( szFileName, NULL, temp ) )
        return NULL;

    IVTFTexture *texture = CreateVTFTexture();
    if ( !texture->Unserialize( temp ) )
    {
        Error( "Invalid or corrupt background texture %s\n", szFileName );
        return NULL;
    }
    texture->ConvertImageFormat( IMAGE_FORMAT_RGBA8888, false );
    return texture;
}

//-----------------------------------------------------------------------------
// Computes the startup graphic name
//-----------------------------------------------------------------------------
void CVideoMode_Common::ComputeStartupGraphicName( char *pBuf, int nBufLen )
{
	// get the image to load
	char startupName[MAX_PATH];
	CL_GetStartupImage( startupName, sizeof( startupName ) );
	Q_snprintf( pBuf, nBufLen, "materials/%s.vtf", startupName );
}

bool CVideoMode_Common::SetupStartupGraphic()
{
	COM_TimestampedLog( "CVideoMode_Common::Init  SetupStartupGraphic" );

	char startupName[MAX_PATH];
	ComputeStartupGraphicName( startupName, sizeof( startupName ) );

	// load in the background vtf
	CUtlBuffer buf;
	m_pBackgroundTexture = LoadVTF( buf, startupName );
	if ( !m_pBackgroundTexture )
	{
		Error( "Can't find background image '%s'\n", startupName );
		return false;
	}

	// loading vtf
	// added this Clear() because we saw cases where LoadVTF was not emptying the buf fully in the above section
	buf.Clear();
	const char *pLoadingName = "materials/console/startup_loading.vtf";
	m_pLoadingTexture = LoadVTF( buf, pLoadingName );
	if ( !m_pLoadingTexture )
	{
		Error( "Can't find background image %s\n", pLoadingName );
		return false;
	}

	buf.Clear();
#if defined( PORTAL2 )
	const char *pTitleName = "materials/vgui/portal2logo.vtf";
#else
	const char *pTitleName = "materials/console/logo.vtf";
#endif
		
	m_pTitleTexture = LoadVTF( buf, pTitleName );
	if ( !m_pTitleTexture )
	{
		Error( "Can't find title image %s\n", pTitleName );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Writes a screenshot to Steam screenshot library given an RGB buffer
// and applies any tags that have been set for it
//-----------------------------------------------------------------------------
void CVideoMode_Common::WriteScreenshotToSteam( uint8 *pImage, int cubImage, int width, int height )
{
#if !defined(NO_STEAM)
	if ( cl_savescreenshotstosteam.GetBool() )
	{
		if ( Steam3Client().SteamScreenshots() )
		{
			ScreenshotHandle hScreenshot = Steam3Client().SteamScreenshots()->WriteScreenshot( pImage, cubImage, width, height );
			ApplySteamScreenshotTags( hScreenshot );
		}
	}
	cl_screenshotusertag.SetValue(0);
	cl_screenshotlocation.SetValue("");
#endif
}


//-----------------------------------------------------------------------------
// Adds a screenshot to the Steam screenshot library from disk
// and applies any tags that have been set for it
//-----------------------------------------------------------------------------
void CVideoMode_Common::AddScreenshotToSteam( const char *pchFilename, int width, int height )
{
#if !defined(NO_STEAM)
	if ( cl_savescreenshotstosteam.GetBool() )
	{
		if ( Steam3Client().SteamScreenshots() )
		{
			ScreenshotHandle hScreenshot = Steam3Client().SteamScreenshots()->AddScreenshotToLibrary( pchFilename, NULL, width, height );
			ApplySteamScreenshotTags( hScreenshot );
		}
	}
	cl_screenshotusertag.SetValue(0);
	cl_screenshotlocation.SetValue("");
#endif
}


//-----------------------------------------------------------------------------
// Applies tags to a screenshot for the Steam screenshot library, which are
// passed in through convars
//-----------------------------------------------------------------------------
#if !defined(NO_STEAM)
void CVideoMode_Common::ApplySteamScreenshotTags( ScreenshotHandle hScreenshot )
{
	if ( hScreenshot != INVALID_SCREENSHOT_HANDLE )
	{
		if ( cl_screenshotusertag.GetBool() )
		{
			if ( Steam3Client().SteamUtils() )
			{
				CSteamID steamID( cl_screenshotusertag.GetInt(), Steam3Client().SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
				Steam3Client().SteamScreenshots()->TagUser( hScreenshot, steamID );
			}
		}
		const char *pchLocation = cl_screenshotlocation.GetString();
		if ( pchLocation && pchLocation[0] )
		{
			Steam3Client().SteamScreenshots()->SetLocation( hScreenshot, pchLocation );
		}
	}	
}
#endif



//-----------------------------------------------------------------------------
// Purpose: Renders the startup video into the HWND
//-----------------------------------------------------------------------------
void CVideoMode_Common::DrawStartupVideo()
{
#if defined( _X360 )
	if ( ( XboxLaunch()->GetLaunchFlags() & LF_WARMRESTART ) )
	{
		// xbox does not play intro startup videos if it restarted itself
		return;
	}
#endif

    // render an avi, if we have one
    if ( !m_bPlayedStartupVideo && !InEditMode() )
    {
        game->PlayStartupVideos();
        m_bPlayedStartupVideo = true;
    }
}


//-----------------------------------------------------------------------------
// Purpose: Renders the startup graphic into the HWND
//-----------------------------------------------------------------------------
void CVideoMode_Common::DrawStartupGraphic()
{
    if ( IsGameConsole() )
	{
		// For TCRs the game consoles must maintain a steady refresh with the startup background.
		// This system already has already been supplied the components. This just starts it.
		BeginLoadingUpdates( MATERIAL_NON_INTERACTIVE_MODE_STARTUP );
		g_pMaterialSystem->RefreshFrontBufferNonInteractive();
        return;
	}

	if ( !SetupStartupGraphic() )
        return;

    CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

    char pStartupGraphicName[MAX_PATH];
    ComputeStartupGraphicName( pStartupGraphicName, sizeof(pStartupGraphicName) );

    // Allocate a white material
    KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
    pVMTKeyValues->SetString( "$basetexture", pStartupGraphicName + 10 );
    pVMTKeyValues->SetInt( "$ignorez", 1 );
    pVMTKeyValues->SetInt( "$nofog", 1 );
    pVMTKeyValues->SetInt( "$no_fullbright", 1 );
    pVMTKeyValues->SetInt( "$nocull", 1 );
    IMaterial *pMaterial = g_pMaterialSystem->CreateMaterial( "__background", pVMTKeyValues );

    pVMTKeyValues = new KeyValues( "UnlitGeneric" );
    pVMTKeyValues->SetString( "$basetexture", "console/startup_loading.vtf" );
    pVMTKeyValues->SetInt( "$translucent", 1 );
    pVMTKeyValues->SetInt( "$ignorez", 1 );
    pVMTKeyValues->SetInt( "$nofog", 1 );
    pVMTKeyValues->SetInt( "$no_fullbright", 1 );
    pVMTKeyValues->SetInt( "$nocull", 1 );
    IMaterial *pLoadingMaterial = g_pMaterialSystem->CreateMaterial( "__loading", pVMTKeyValues );

// Don't draw the title text for CSS15
#if !defined( CSTRIKE15 )

	pVMTKeyValues = new KeyValues( "UnlitGeneric" );
#if defined( PORTAL2 )
	pVMTKeyValues->SetString( "$basetexture", "vgui/portal2logo.vtf" );
#else
	pVMTKeyValues->SetString( "$basetexture", "console/logo.vtf" );
#endif // defined( PORTAL2 )
	pVMTKeyValues->SetInt( "$translucent", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetInt( "$nofog", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$nocull", 1 );
	IMaterial *pTitleMaterial = g_pMaterialSystem->CreateMaterial( "__title", pVMTKeyValues );

#endif // CSTRIKE15

    int w = GetModeWidth();
    int h = GetModeHeight();
    int tw = m_pBackgroundTexture->Width();
    int th = m_pBackgroundTexture->Height();
    int lw = m_pLoadingTexture->Width();
    int lh = m_pLoadingTexture->Height();

	char debugstartup = CommandLine()->FindParm("-debugstartupscreen");
	if (debugstartup)
	{
		for ( int repeat = 0; repeat<1000; repeat++)
		{
			pRenderContext->Viewport( 0, 0, w, h );
			pRenderContext->DepthRange( 0, 1 );
			pRenderContext->ClearColor3ub( 0, (repeat & 0x7) << 3, 0 );
			pRenderContext->ClearBuffers( true, true, true );
			pRenderContext->SetToneMappingScaleLinear( Vector(1,1,1) );

			if(1)	// draw normal BK
			{
				float depth = 0.55f;
				int slide = (repeat) % 200;	// 100 down and 100 up
				if (slide > 100)
				{
					slide = 200-slide;		// aka 100-(slide-100).
				}
				
				DrawScreenSpaceRectangle( pMaterial, 0, 0+slide, w, h-50, 0, 0, tw-1, th-1, tw, th, NULL,1,1,depth );
				DrawScreenSpaceRectangle( pLoadingMaterial, w-lw, h-lh+slide/2, lw, lh, 0, 0, lw-1, lh-1, lw, lh, NULL,1,1,depth-0.1 );
			}

			if(0)
			{
					// draw a grid too
				int grid_size = 8;
				float depthacc = 0.0;
				float depthinc = 1.0 / (float)((grid_size * grid_size)+1);
				
				for( int x = 0; x<grid_size; x++)
				{
					float cornerx = ((float)x) * 20.0f;
					
					for( int y=0; y<grid_size; y++)
					{
						float cornery = ((float)y) * 20.0f;

						//if (! ((x^y) & 1) )
						{
							DrawScreenSpaceRectangle( pMaterial, 10.0f+cornerx,10.0f+ cornery, 15, 15, 0, 0, tw-1, th-1, tw, th, NULL,1,1, depthacc );
						}
						
						depthacc += depthinc;
					}
				}
			}

			g_pMaterialSystem->SwapBuffers();			
		}
	}
	else
	{
		pRenderContext->Viewport( 0, 0, w, h );
		pRenderContext->DepthRange( 0, 1 );
		pRenderContext->ClearColor3ub( 0, 0, 0 );
		pRenderContext->ClearBuffers( true, true, true );
		pRenderContext->SetToneMappingScaleLinear( Vector(1,1,1) );
		
		float depth = 0.5f;

#if defined( CSTRIKE15 )
		// Apply a custom scaling that mirrors the way we draw the Scaleform background texture
		Assert( tw == th );

		// VTFs are forced to be square, even if the source texture is non 1:1.  Rescale the
		//		texture to assume its actually in 16:9, as it was authored
		float convertTH = th * 720.0f / 1280.0f;

		// Now, determine the scale between the texture and the viewport in height
		float heightScale = (float)h / convertTH;

		int scaledTH = (int)( heightScale * (float) convertTH );
		int scaledTW = (int)( heightScale * (float) tw );

		int halfH = h / 2;
		int halfW = w / 2;

		int halfTH = scaledTH / 2;
		int halfTW = scaledTW / 2;

		DrawScreenSpaceRectangle( pMaterial, halfW - halfTW, halfH - halfTH, scaledTW, scaledTH, 0, 0, tw-1, th-1, tw, th, NULL,1,1,depth );
#else
		
		DrawScreenSpaceRectangle( pMaterial, 0, 0, w, h, 0, 0, tw-1, th-1, tw, th, NULL,1,1,depth );
//		DrawScreenSpaceRectangle( pLoadingMaterial, w-lw, h-lh, lw, lh, 0, 0, lw-1, lh-1, lw, lh, NULL,1,1,depth );

#endif // CSTRIKE15

// Don't draw the title text for CSS15
#if !defined( CSTRIKE15 )
		// center align at bottom
		int title_y = vgui::scheme()->GetProportionalScaledValue( 390 );
		int title_w = vgui::scheme()->GetProportionalScaledValue( 240 );
		int title_h = vgui::scheme()->GetProportionalScaledValue( 60 );
		int title_x = (w/2 - title_w/2);

		DrawScreenSpaceRectangle( pTitleMaterial, title_x, title_y, title_w, title_h, 0, 0, title_w-1, title_h-1, title_w, title_h, NULL,1,1,depth );
#endif // CSTRIKE15

		g_pMaterialSystem->SwapBuffers();
	}

#if defined( DX_TO_GL_ABSTRACTION ) && !defined( _GAMECONSOLE )
	g_pMaterialSystem->DoStartupShaderPreloading();
#endif

    pMaterial->Release();
    pLoadingMaterial->Release();

    // release graphics
    DestroyVTFTexture( m_pBackgroundTexture );
    m_pBackgroundTexture = NULL;
    DestroyVTFTexture( m_pLoadingTexture );
    m_pLoadingTexture = NULL;
	DestroyVTFTexture( m_pTitleTexture );
	m_pTitleTexture = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Blits an image to the loading window hdc
//-----------------------------------------------------------------------------
void CVideoMode_Common::BlitGraphicToHDCWithAlpha(HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1)
{
#ifdef WIN32
    if ( IsX360() )
        return;

    int x = x0;
    int y = y0;
    int wide = x1 - x0;
    int tall = y1 - y0;

    Assert(imageWidth == wide && imageHeight == tall);

    int texwby4 = imageWidth << 2;

    for ( int v = 0; v < tall; v++ )
    {
        int *src = (int *)(rgba + (v * texwby4));
        int xaccum = 0;

        for ( int u = 0; u < wide; u++ )
        {
            byte *xsrc = (byte *)(src + xaccum);
            if (xsrc[3])
            {
                ::SetPixel(hdc, x + u, y + v, RGB(xsrc[0], xsrc[1], xsrc[2]));
            }
            xaccum += 1;
        }
    }
#else
    Assert( !"Impl me" );
#endif
}

void CVideoMode_Common::InvalidateWindow()
{
    if ( CommandLine()->FindParm( "-noshaderapi" ) )
    {
#if USE_SDL
        SDL_Event fake;
        memset(&fake, '\0', sizeof (SDL_Event));
        fake.type = SDL_WINDOWEVENT;
        fake.window.windowID = SDL_GetWindowID((SDL_Window *) game->GetMainWindow());
        fake.window.event = SDL_WINDOWEVENT_EXPOSED;
        SDL_PushEvent(&fake);
#elif defined( WIN32 ) 
        InvalidateRect( (HWND)game->GetMainWindow(), NULL, FALSE );
#elif defined( OSX ) && defined( PLATFORM_64BITS )
	// Do nothing, we'll move to SDL or we'll port the below.
	Assert( !"OSX-64 unimpl" );
#elif OSX
        int x,y,w,t;
        game->GetWindowRect( &x,&y,&w,&t);
        Rect bounds = { 0,0,w,t}; // inval is in local co-ords
        InvalWindowRect( (WindowRef)game->GetMainWindow(), &bounds );
#elif defined( _PS3 )
#else
#error
#endif
    }
}

void CVideoMode_Common::DrawNullBackground( void *hHDC, int w, int h )
{
	if ( IsX360() )
		return;

	HDC hdc = (HDC)hHDC;

	// Show a message if running without renderer..
	if ( CommandLine()->FindParm( "-noshaderapi" ) )
	{
#ifdef WIN32
		HFONT fnt = CreateFontA( -12, 
		 0,
		 0,
		 0,
		 FW_NORMAL,
		 FALSE,
		 FALSE,
		 FALSE,
		 ANSI_CHARSET,
		 OUT_TT_PRECIS,
		 CLIP_DEFAULT_PRECIS,
		 ANTIALIASED_QUALITY,
		 DEFAULT_PITCH,
		 "Arial" );

		HFONT oldFont = (HFONT)SelectObject( hdc, fnt );
		int oldBkMode = SetBkMode( hdc, TRANSPARENT );
		COLORREF oldFgColor = SetTextColor( hdc, RGB( 255, 255, 255 ) );

		HBRUSH br = CreateSolidBrush( RGB( 0, 0, 0  ) );
		HBRUSH oldBr = (HBRUSH)SelectObject( hdc, br );
		Rectangle( hdc, 0, 0, w, h );
		
		RECT rc;
		rc.left = 0;
		rc.top = 0;
		rc.right = w;
		rc.bottom = h;

		DrawText( hdc, "Running with -noshaderapi", -1, &rc, DT_NOPREFIX | DT_VCENTER | DT_CENTER | DT_SINGLELINE  );

		rc.top = rc.bottom - 30;

		if ( host_state.worldmodel != NULL )
		{
			rc.left += 10;
			DrawText( hdc, modelloader->GetName( host_state.worldmodel ), -1, &rc, DT_NOPREFIX | DT_VCENTER | DT_SINGLELINE  );
		}

		SetTextColor( hdc, oldFgColor );

		SelectObject( hdc, oldBr );
		SetBkMode( hdc, oldBkMode );
		SelectObject( hdc, oldFont );

		DeleteObject( br );
		DeleteObject( fnt );
#else
		printf ( "%s\n",  modelloader->GetName( host_state.worldmodel ) );
#endif
	}
}

#if !defined( _WIN32 ) && !defined( _PS3 )

typedef unsigned char BYTE;

#if !defined( OSX ) || defined( PLATFORM_64BITS )
        typedef unsigned int ULONG;
        typedef int LONG;
#else
        typedef unsigned long ULONG;
        typedef long LONG;
#endif

typedef char * LPSTR;

typedef struct tagBITMAPINFOHEADER{
    DWORD      biSize;
    LONG       biWidth;
    LONG       biHeight;
    WORD       biPlanes;
    WORD       biBitCount;
    DWORD      biCompression;
    DWORD      biSizeImage;
    LONG       biXPelsPerMeter;
    LONG       biYPelsPerMeter;
    DWORD      biClrUsed;
    DWORD      biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagBITMAPFILEHEADER {
    WORD    bfType;
    DWORD   bfSize;
    WORD    bfReserved1;
    WORD    bfReserved2;
    DWORD   bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagRGBQUAD {
    BYTE    rgbBlue;
    BYTE    rgbGreen;
    BYTE    rgbRed;
    BYTE    rgbReserved;
} RGBQUAD;

/* constants for the biCompression field */
#define BI_RGB        0L
#define BI_RLE8       1L
#define BI_RLE4       2L
#define BI_BITFIELDS  3L

typedef GUID UUID;

#endif //WIN32
//-----------------------------------------------------------------------------
// Purpose: Blits an image to the loading window hdc
//-----------------------------------------------------------------------------
void CVideoMode_Common::BlitGraphicToHDC(HDC hdc, byte *rgba, int imageWidth, int imageHeight, int x0, int y0, int x1, int y1)
{
    if ( IsX360() )
        return;

#ifdef WIN32
    int x = x0;
    int y = y0;
    int wide = x1 - x0;
    int tall = y1 - y0;

    // Needs to be a multiple of 4
    int dibwide = ( wide + 3 ) & ~3;

    Assert(rgba);
    int texwby4 = imageWidth << 2;

    double st = Plat_FloatTime();

    void *destBits = NULL;

    HBITMAP bm;
    BITMAPINFO bmi;
    Q_memset( &bmi, 0, sizeof( bmi ) );

    BITMAPINFOHEADER *hdr = &bmi.bmiHeader;

    hdr->biSize = sizeof( *hdr );
    hdr->biWidth = dibwide;
    hdr->biHeight = -tall;  // top down bitmap
    hdr->biBitCount = 24;
    hdr->biPlanes = 1;
    hdr->biCompression = BI_RGB;
    hdr->biSizeImage = dibwide * tall * 3;
    hdr->biXPelsPerMeter = 3780;
    hdr->biYPelsPerMeter = 3780;

    // Create a "source" DC
    HDC tempDC = CreateCompatibleDC( hdc );

    // Create the dibsection bitmap
    bm = CreateDIBSection
    (
        tempDC,                     // handle to DC
        &bmi,                       // bitmap data
        DIB_RGB_COLORS,             // data type indicator
        &destBits,                  // bit values
        NULL,                       // handle to file mapping object
        0                           // offset to bitmap bit values
    );
    
    // Select it into the source DC
    HBITMAP oldBitmap = (HBITMAP)SelectObject( tempDC, bm );

    // Setup for bilinaer filtering. If we don't do this filter here, there will be a big
    // annoying pop when it switches to the vguimatsurface version of the background.
    // We leave room for 14 bits of integer precision, so the image can be up to 16k x 16k.
    const int BILINEAR_FIX_SHIFT = 17;
    const int BILINEAR_FIX_MUL = (1 << BILINEAR_FIX_SHIFT);

    #define FIXED_BLEND( a, b, out, frac ) \
        out[0] = (a[0]*frac + b[0]*(BILINEAR_FIX_MUL-frac)) >> BILINEAR_FIX_SHIFT; \
        out[1] = (a[1]*frac + b[1]*(BILINEAR_FIX_MUL-frac)) >> BILINEAR_FIX_SHIFT; \
        out[2] = (a[2]*frac + b[2]*(BILINEAR_FIX_MUL-frac)) >> BILINEAR_FIX_SHIFT;

    float eps = 0.001f;
    float uMax = imageWidth - 1 - eps;
    float vMax = imageHeight - 1 - eps;

    int fixedBilinearV = 0;
    int bilinearUInc = (int)( (uMax / (dibwide-1)) * BILINEAR_FIX_MUL );
    int bilinearVInc = (int)( (vMax / (tall-1)) * BILINEAR_FIX_MUL );

    for ( int v = 0; v < tall; v++ )
    {
        int iBilinearV = fixedBilinearV >> BILINEAR_FIX_SHIFT;
        int fixedFractionV = fixedBilinearV & (BILINEAR_FIX_MUL-1);
        fixedBilinearV += bilinearVInc;

        int fixedBilinearU = 0;
        byte *dest = (byte *)destBits + ( ( y + v ) * dibwide + x ) * 3;

        for ( int u = 0; u < dibwide; u++, dest+=3 )
        {
            int iBilinearU = fixedBilinearU >> BILINEAR_FIX_SHIFT;
            int fixedFractionU = fixedBilinearU & (BILINEAR_FIX_MUL-1);
            fixedBilinearU += bilinearUInc;
        
            Assert( iBilinearU >= 0 && iBilinearU+1 < imageWidth );
            Assert( iBilinearV >= 0 && iBilinearV+1 < imageHeight );

            byte *srcTopLine    = rgba + iBilinearV * texwby4;
            byte *srcBottomLine = rgba + (iBilinearV+1) * texwby4;

            byte *xsrc[4] = {
                srcTopLine + (iBilinearU+0)*4,    srcTopLine + (iBilinearU+1)*4,
                srcBottomLine + (iBilinearU+0)*4, srcBottomLine + (iBilinearU+1)*4  };

            int topColor[3], bottomColor[3], finalColor[3];
            FIXED_BLEND( xsrc[1], xsrc[0], topColor, fixedFractionU );
            FIXED_BLEND( xsrc[3], xsrc[2], bottomColor, fixedFractionU );
            FIXED_BLEND( bottomColor, topColor, finalColor, fixedFractionV );

            // Windows wants the colors in reverse order.
            dest[0] = finalColor[2];
            dest[1] = finalColor[1];
            dest[2] = finalColor[0];
        }
    }
    
    // Now do the Blt
    BitBlt( hdc, 0, 0, dibwide, tall, tempDC, 0, 0, SRCCOPY );

    // This only draws if running -noshaderapi
    DrawNullBackground( hdc, dibwide, tall );

    // Restore the old Bitmap
    SelectObject( tempDC, oldBitmap );

    // Destroy the temporary DC
    DeleteDC( tempDC );

    // Destroy the DIBSection bitmap
    DeleteObject( bm );

    double elapsed = Plat_FloatTime() - st;

    COM_TimestampedLog( "BlitGraphicToHDC: new ver took %.4f", elapsed );
#else
    Assert( !"Impl me" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: This is called in response to a WM_MOVE message
//-----------------------------------------------------------------------------
void CVideoMode_Common::UpdateWindowPosition( void )
{
    int x, y, w, h;

    // Get the window from the game ( right place for it? )
    game->GetWindowRect( &x, &y, &w, &h );

#ifdef WIN32
    RECT window_rect;
    window_rect.left = x;
    window_rect.right = x + w;
    window_rect.top = y;
    window_rect.bottom = y + h;
#endif
    // NOTE: We need to feed this back into the video mode stuff
    // esp. in Resizing window mode.
}

void CVideoMode_Common::ChangeDisplaySettingsToFullscreen( int nWidth, int nHeight, int nBPP, bool bDesktopFriendlyFullscreen )
{
}

void CVideoMode_Common::ReleaseFullScreen( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: Returns the optimal refresh rate for the specified mode
//-----------------------------------------------------------------------------
int CVideoMode_Common::GetRefreshRateForMode( const vmode_t *pMode )
{
    int nRefreshRate = pMode->refreshRate;

    // FIXME: We should only read this once, at the beginning
    // override the refresh rate from the command-line maybe
    nRefreshRate = CommandLine()->ParmValue( "-freq", nRefreshRate );
    nRefreshRate = CommandLine()->ParmValue( "-refresh", nRefreshRate );
    nRefreshRate = CommandLine()->ParmValue( "-refreshrate", nRefreshRate );

    return nRefreshRate;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *mode - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
void CVideoMode_Common::AdjustWindow( int nWidth, int nHeight, int nBPP, bool bWindowed, bool bNoWindowBorder )
{
	if ( IsPS3() )
	{
		if ( game )
			game->SetWindowSize( nWidth, nHeight );
		return;
	}

    if ( g_bTextMode )
        return;
    
    // Use Change Display Settings to go full screen
    ChangeDisplaySettingsToFullscreen( nWidth, nHeight, nBPP, bNoWindowBorder );

    RECT WindowRect;
    WindowRect.top      = 0;
    WindowRect.left     = 0;
    WindowRect.right    = nWidth;
    WindowRect.bottom   = nHeight;

#if defined( WIN32 ) && !defined( USE_SDL )
#ifndef _X360
	// Get window style
    DWORD style = GetWindowLong( (HWND)game->GetMainWindow(), GWL_STYLE );
    DWORD exStyle = GetWindowLong( (HWND)game->GetMainWindow(), GWL_EXSTYLE );

    if ( bWindowed )
    {
        // Give it a frame (pretty much WS_OVERLAPPEDWINDOW except for we do not modify the
        // flags corresponding to resizing-frame and maximize-box)
		if( !CommandLine()->FindParm( "-noborder" ) && !bNoWindowBorder )
        {
            style |= WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        }
        else
        {
            style &= ~WS_OVERLAPPEDWINDOW;
        }

        // remove topmost flag
        exStyle &= ~WS_EX_TOPMOST;
        SetWindowLong( (HWND)game->GetMainWindow(), GWL_EXSTYLE, exStyle );
    }
    else
    {
        // Remove window border going into fullscreen mode to avoid Vista sizing issues when DWM is enabled
        style &= ~WS_OVERLAPPEDWINDOW;
    }

    SetWindowLong( (HWND)game->GetMainWindow(), GWL_STYLE, style );

    // Compute rect needed for that size client area based on window style
    AdjustWindowRectEx( &WindowRect, style, FALSE, exStyle );
#endif

    // Prepare to set window pos, which is required when toggling between topmost and not window flags
    HWND hWndAfter = NULL;
    DWORD dwSwpFlags = 0;
#ifndef _X360
    {
        if ( bWindowed )
        {
            hWndAfter = HWND_NOTOPMOST;
        }
        else
        {
            hWndAfter = HWND_TOPMOST;
        }
        dwSwpFlags = SWP_FRAMECHANGED;
    }
#else
    {
        dwSwpFlags = SWP_NOZORDER;
    }
#endif

    // Move the window to 0, 0 and the new true size
    SetWindowPos( (HWND)game->GetMainWindow(),
                 hWndAfter,
                 0, 0, WindowRect.right - WindowRect.left,
                 WindowRect.bottom - WindowRect.top,
                 SWP_NOREDRAW | dwSwpFlags );
#endif // WIN32 && !USE_SDL
	
	// Now center
	CenterEngineWindow( game->GetMainWindow(),
					   WindowRect.right - WindowRect.left,
					   WindowRect.bottom - WindowRect.top );
	


#if defined(USE_SDL)
	g_pLauncherMgr->SetWindowFullScreen( !bWindowed, nWidth, nHeight, bNoWindowBorder );

	MaterialVideoMode_t vidMode;

	// Mid-2015 MBP with Intel and AMD GPUs can fail to set the resolution correctly the first time when
	// transitioning to fullscreen. The low level driver seems to throw an error but moves past it. Terrifying.
	// In the meantime, we workaround this by trying again--because trying a second time generally seems to 
	// be successful. If it isn't, then we punt on exclusive fullscreen mode and do non-exclusive fs instead.
	// We can't fix this in sdlmgr.cpp (which would be less ugly) because SDL caches window and display
	// information, and so it thinks everything worked great. We need to get the resolution of the desktop
	// back from the firehose, and calling into the material system from sdlmgr would insert a dependency
	// on materialsystem into every dll we build (since sdlmgr lives in appframework).
	if ( IsOSX() && !bWindowed && !bNoWindowBorder )
	{
		// Did we set the size correctly?
		materials->GetDisplayMode( vidMode );
		if ( vidMode.m_Width != nWidth || vidMode.m_Height != nHeight )
		{
			Msg( "Requested full screen window of %dx%d, but got %dx%d. Trying again.\n", nWidth, nHeight, vidMode.m_Width, vidMode.m_Height );
			g_pLauncherMgr->SetWindowFullScreen( false, nWidth, nHeight, bNoWindowBorder );
			g_pLauncherMgr->SetWindowFullScreen( true, nWidth, nHeight, bNoWindowBorder );
		}

		// If still not, then force non-exclusive mode. 
		materials->GetDisplayMode( vidMode );
		if ( vidMode.m_Width != nWidth || vidMode.m_Height != nHeight )
		{
			Msg( "Requested full screen window of %dx%d, but got %dx%d. Disabling exclusive mode and trying again.\n", nWidth, nHeight, vidMode.m_Width, vidMode.m_Height );
			CommandLine()->RemoveParm( "-exclusivefs" );
			CommandLine()->AppendParm( "-noexclusivefs", nullptr );
			g_pLauncherMgr->SetWindowFullScreen( false, nWidth, nHeight, bNoWindowBorder );
			g_pLauncherMgr->SetWindowFullScreen( true, nWidth, nHeight, bNoWindowBorder );
		}
	}

	CenterEngineWindow( game->GetMainWindow(),
		WindowRect.right - WindowRect.left,
		WindowRect.bottom - WindowRect.top );

	g_pLauncherMgr->SizeWindow( WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top );

#elif defined( WIN32 )
#elif defined(OSX)

	g_pLauncherMgr->SizeWindow( WindowRect.right - WindowRect.left, WindowRect.bottom - WindowRect.top );

#ifdef LINUX
	if( bWindowed )
	{
		SDL_Window* win = (SDL_Window*)g_pLauncherMgr->GetWindowRef();
		SDL_SetWindowBordered( win, ( CommandLine()->FindParm( "-noborder" ) ) ? SDL_FALSE : SDL_TRUE );
	}
#endif

#else
    Assert( !"Impl me" );
#endif

    game->SetWindowSize( nWidth, nHeight );

    // Make sure we have updated window information
    UpdateWindowPosition();
    MarkClientViewRectDirty();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_Common::Shutdown( void )
{
    ReleaseFullScreen();
    game->DestroyGameWindow();

    if ( !GetInitialized() )
        return;

    SetInitialized( false );
}


//-----------------------------------------------------------------------------
// Gets/sets the client view rectangle
//-----------------------------------------------------------------------------
const vrect_t &CVideoMode_Common::GetClientViewRect( ) const
{
    const_cast<CVideoMode_Common*>(this)->RecomputeClientViewRect();
    return m_ClientViewRect;
}

void CVideoMode_Common::SetClientViewRect( const vrect_t &viewRect )
{
    m_ClientViewRect = viewRect;
}


//-----------------------------------------------------------------------------
// Marks the client view rect dirty
//-----------------------------------------------------------------------------
void CVideoMode_Common::MarkClientViewRectDirty()
{
    m_bClientViewRectDirty = true;
}

void CVideoMode_Common::RecomputeClientViewRect()
{
    if ( !InEditMode() )
    {
        if ( !m_bClientViewRectDirty )
            return;
    }

    m_bClientViewRectDirty = false;

    int nWidth, nHeight;
    CMatRenderContextPtr pRenderContext( materials );

    pRenderContext->GetRenderTargetDimensions( nWidth, nHeight );
    m_ClientViewRect.width  = nWidth;
    m_ClientViewRect.height = nHeight;
    m_ClientViewRect.x      = 0;
    m_ClientViewRect.y      = 0;

    if (!nWidth || !nHeight)
    {
        // didn't successfully get the screen size, try again next frame
        // window is probably minimized
        m_bClientViewRectDirty = true;
    }
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hWndCenter - 
//          width - 
//          height - 
// Output : static void
//-----------------------------------------------------------------------------
void CVideoMode_Common::CenterEngineWindow( void *hWndCenter, int width, int height)
{
    int     CenterX, CenterY;

#if defined(USE_SDL)
	// Get the displayindex, and center our window on that display.
	int displayindex = g_pLauncherMgr->GetActiveDisplayIndex();

	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode( displayindex, &mode );

	const int wide = mode.w;
	const int tall = mode.h;
	
	CenterX = (wide - width) / 2;
	CenterY = (tall - height) / 2;
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;

	// tweak the x and w positions if the user species them on the command-line
	CenterX = CommandLine()->ParmValue( "-x", CenterX );
	CenterY = CommandLine()->ParmValue( "-y", CenterY );
	
	// also check for the negated form (since it is hard to say "-x -1000")
	int negx = CommandLine()->ParmValue( "-negx", 0 ); 
	if (negx > 0)
	{
		CenterX = -negx;
	}
	int negy = CommandLine()->ParmValue( "-negy", 0 ); 
	if (negy > 0)
	{
		CenterY = -negy;
	}
	
	SDL_Rect rect = { 0, 0, 0, 0 };
	SDL_GetDisplayBounds( displayindex, &rect );

	CenterX += rect.x;
	CenterY += rect.y;

	game->SetWindowXY( CenterX, CenterY );
	g_pLauncherMgr->MoveWindow( CenterX, CenterY );
#elif defined( WIN32 ) 
   if ( IsPC() )
    {
        // In windowed mode go through game->GetDesktopInfo because system metrics change
        // when going fullscreen vs windowed.
        // Use system metrics for fullscreen or when game didn't have a chance to initialize.

        int cxScreen = 0, cyScreen = 0, refreshRate = 0;

        if ( !( WS_EX_TOPMOST & ::GetWindowLong( (HWND)hWndCenter, GWL_EXSTYLE ) ) )
        {
            game->GetDesktopInfo( cxScreen, cyScreen, refreshRate );
        }
		
        if ( !cxScreen || !cyScreen )
        {
            cxScreen = GetSystemMetrics(SM_CXSCREEN);
            cyScreen = GetSystemMetrics(SM_CYSCREEN);
        }

        // Compute top-left corner offset
        CenterX = (cxScreen - width) / 2;
        CenterY = (cyScreen - height) / 2;
        CenterX = (CenterX < 0) ? 0: CenterX;
        CenterY = (CenterY < 0) ? 0: CenterY;
    }
    else
    {
        CenterX = 0;
        CenterY = 0;
    }

    // tweak the x and w positions if the user species them on the command-line
    CenterX = CommandLine()->ParmValue( "-x", CenterX );
    CenterY = CommandLine()->ParmValue( "-y", CenterY );

    game->SetWindowXY( CenterX, CenterY );

    SetWindowPos ( (HWND)hWndCenter, NULL, CenterX, CenterY, 0, 0,
                  SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW | SWP_DRAWFRAME);
#elif defined(OSX)
	CGDisplayCount maxDisplays = 1;
	CGDirectDisplayID activeDspys[1];
	CGDisplayErr error;
	short i;
	CGDisplayCount newDspyCnt = 0;
	
	error = CGGetActiveDisplayList(maxDisplays, activeDspys, &newDspyCnt);
	if (error || newDspyCnt < 1) 
		return;
	
	CGRect displayRect = CGDisplayBounds (activeDspys[0]);
	int wide = displayRect.size.width;
	int tall = displayRect.size.height;
	
	CenterX = (wide - width) / 2;
	CenterY = (tall - height) / 2;
	CenterX = (CenterX < 0) ? 0: CenterX;
	CenterY = (CenterY < 0) ? 0: CenterY;

	// tweak the x and w positions if the user species them on the command-line
    CenterX = CommandLine()->ParmValue( "-x", CenterX );
    CenterY = CommandLine()->ParmValue( "-y", CenterY );
	
	// also check for the negated form (since it is hard to say "-x -1000")
	int negx = CommandLine()->ParmValue( "-negx", 0 ); 
	if (negx > 0)
	{
		CenterX = -negx;
	}
	int negy = CommandLine()->ParmValue( "-negy", 0 ); 
	if (negy > 0)
	{
		CenterY = -negy;
	}
	
	game->SetWindowXY( CenterX, CenterY );
	g_pLauncherMgr->MoveWindow( CenterX, CenterY );
#else
	Assert( !"Impl me" );
#endif

}


//-----------------------------------------------------------------------------
// Handle alt-tab
//-----------------------------------------------------------------------------
void CVideoMode_Common::RestoreVideo( void )
{
}

void CVideoMode_Common::ReleaseVideo( void )
{
}


//-----------------------------------------------------------------------------
// Read screen pixels
//-----------------------------------------------------------------------------
void CVideoMode_Common::ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format )
{
    int nBytes = ImageLoader::GetMemRequired( w, h, 1, format, false );
    memset( pBuffer, 0, nBytes );
}

//-----------------------------------------------------------------------------
// Purpose: Write vid.buffer out as a .tga file
//-----------------------------------------------------------------------------
void CVideoMode_Common::TakeSnapshotTGA( const char *pFilename )
{
    // bitmap bits
    uint8 *pImage = new uint8[ GetModeWidth() * 3 * GetModeHeight() ];

    // Get Bits from the material system
    ReadScreenPixels( 0, 0, GetModeWidth(), GetModeHeight(), pImage, IMAGE_FORMAT_RGB888 );

	// this xbox screenshot path is not meant as an exact framebuffer image
	// need to correct for srgb disparity for proper comparison with PC screenshots
	if ( IsX360() )
	{
		// process as 3 byte tuples
		for ( int i = 0; i < GetModeWidth() * GetModeHeight() * 3; i++ )
		{
			pImage[i] = ( unsigned char )( SrgbLinearToGamma( X360GammaToLinear( (float)pImage[i] / 255.0f ) ) * 255.0f );
		}
	}

    CUtlBuffer outBuf;
    if ( TGAWriter::WriteToBuffer( pImage, outBuf, GetModeWidth(), GetModeHeight(), IMAGE_FORMAT_RGB888,
        IMAGE_FORMAT_RGB888 ) )
    {
        if ( !g_pFileSystem->WriteFile( pFilename, NULL, outBuf ) )
        {
            Warning( "Couldn't write bitmap data snapshot to file %s.\n", pFilename );
		}
		else
		{
			char szPath[MAX_PATH];
			szPath[0] = 0;
			if ( g_pFileSystem->GetLocalPath( pFilename, szPath, sizeof(szPath) ) )
			{
				AddScreenshotToSteam( szPath, GetModeWidth(), GetModeHeight() );
			}
		}

    }

    delete[] pImage;
}

//-----------------------------------------------------------------------------
// PFM screenshot helpers
//-----------------------------------------------------------------------------
ITexture *CVideoMode_Common::GetBuildCubemaps16BitTexture( void )
{
    return materials->FindTexture( "_rt_BuildCubemaps16bit", TEXTURE_GROUP_RENDER_TARGET );
}

ITexture *CVideoMode_Common::GetFullFrameFB0( void )
{
    return materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
}

void CVideoMode_Common::BlitHiLoScreenBuffersTo16Bit( void )
{
    if ( IsX360() )
    {
        // FIXME: this breaks in 480p due to (at least) the multisampled depth buffer (need to cache, clear and restore the depth target)
        Assert( 0 );
        return;
    }
    
    IMaterial *pHDRCombineMaterial = materials->FindMaterial( "dev/hdrcombineto16bit", TEXTURE_GROUP_OTHER, true );
//  if( IsErrorMaterial( pHDRCombineMaterial ) )
//  {
//      Assert( 0 );
//      return;
//  }

    CMatRenderContextPtr pRenderContext( materials );
    ITexture *pSaveRenderTarget;
    pSaveRenderTarget = pRenderContext->GetRenderTarget();

    int oldX, oldY, oldW, oldH;
    pRenderContext->GetViewport( oldX, oldY, oldW, oldH );

    pRenderContext->SetRenderTarget( GetBuildCubemaps16BitTexture() );
    int width, height;
    pRenderContext->GetRenderTargetDimensions( width, height );
    pRenderContext->Viewport( 0, 0, width, height );
    pRenderContext->DrawScreenSpaceQuad( pHDRCombineMaterial );

    pRenderContext->SetRenderTarget( pSaveRenderTarget );
    pRenderContext->Viewport( oldX, oldY, oldW, oldH );
}

void GetCubemapOffset( CubeMapFaceIndex_t faceIndex, int &x, int &y, int &faceDim )
{
    int fbWidth, fbHeight;
    materials->GetBackBufferDimensions( fbWidth, fbHeight );

    if( fbWidth * 4 > fbHeight * 3 )
    {
        faceDim = fbHeight / 3;
    }
    else
    {
        faceDim = fbWidth / 4;
    }

    switch( faceIndex )
    {
    case CUBEMAP_FACE_RIGHT:
        x = 2;
        y = 1;
        break;
    case CUBEMAP_FACE_LEFT:
        x = 0;
        y = 1;
        break;
    case CUBEMAP_FACE_BACK:
        x = 1;
        y = 1;
        break;
    case CUBEMAP_FACE_FRONT:
        x = 3;
        y = 1;
        break;
    case CUBEMAP_FACE_UP:
        x = 2;
        y = 0;
        break;
    case CUBEMAP_FACE_DOWN:
        x = 2;
        y = 2;
        break;
    NO_DEFAULT
    }
    x *= faceDim;
    y *= faceDim;
}

//-----------------------------------------------------------------------------
// Takes a snapshot to PFM
//-----------------------------------------------------------------------------
void CVideoMode_Common::TakeSnapshotPFMRect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, CubeMapFaceIndex_t faceIndex )
{
    if ( IsX360() )
    {
        // FIXME: this breaks in 480p due to (at least) the multisampled depth buffer (need to cache, clear and restore the depth target)
        Assert( 0 );
        return;
    }

    if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE )
    {
        Warning( "Unable to take PFM screenshots if HDR isn't enabled!\n" );
        return;
    }

    // hack
//  resampleWidth = w;
//  resampleHeight = h;
    // bitmap bits
    float16 *pImage = ( float16 * )malloc( w * h * ImageLoader::SizeInBytes( IMAGE_FORMAT_RGBA16161616F ) );
    float *pImage1 = ( float * )malloc( w * h * ImageLoader::SizeInBytes( IMAGE_FORMAT_RGB323232F ) );

    CMatRenderContextPtr pRenderContext( materials );

    // Save the current render target.
    ITexture *pSaveRenderTarget = pRenderContext->GetRenderTarget();

    // Set this as the render target so that we can read it.
    pRenderContext->SetRenderTarget( GetFullFrameFB0() );

    // Get Bits from the material system
    ReadScreenPixels( x, y, w, h, pImage, IMAGE_FORMAT_RGBA16161616F );

    // Draw what we just grabbed to the screen
    pRenderContext->SetRenderTarget( NULL);

    int scrw, scrh;
    pRenderContext->GetRenderTargetDimensions( scrw, scrh );
    pRenderContext->Viewport( 0, 0, scrw,scrh );

    int offsetX, offsetY, faceDim;
    GetCubemapOffset( faceIndex, offsetX, offsetY, faceDim );
    pRenderContext->DrawScreenSpaceRectangle( materials->FindMaterial( "dev/copyfullframefb", "" ),
        offsetX, offsetY, faceDim, faceDim, 0, 0, w-1, h-1, scrw, scrh );

    // Restore the render target.
    pRenderContext->SetRenderTarget( pSaveRenderTarget );

    // convert from float16 to float32
    ImageLoader::ConvertImageFormat( ( unsigned char * )pImage, IMAGE_FORMAT_RGBA16161616F, 
        ( unsigned char * )pImage1, IMAGE_FORMAT_RGB323232F, 
        w, h );

    Assert( w == h ); // garymcthack - this only works for square images

    float *pFloatImage = ( float * )malloc( resampleWidth * resampleHeight * ImageLoader::SizeInBytes( IMAGE_FORMAT_RGB323232F ) );

    ImageLoader::ResampleInfo_t info;
    info.m_pSrc = ( unsigned char * )pImage1;
    info.m_pDest = ( unsigned char * )pFloatImage;
    info.m_nSrcWidth = w;
    info.m_nSrcHeight = h;
    info.m_nDestWidth = resampleWidth;
    info.m_nDestHeight = resampleHeight;
    info.m_flSrcGamma = 1.0f;
    info.m_flDestGamma = 1.0f;

    if( !ImageLoader::ResampleRGB323232F( info ) )
    {
        Sys_Error( "Can't resample\n" );
    }

    PFMWrite( pFloatImage, pFilename, resampleWidth, resampleHeight );

    free( pImage1 );
    free( pImage );
    free( pFloatImage );
}


//-----------------------------------------------------------------------------
// Takes a snapshot
//-----------------------------------------------------------------------------
void CVideoMode_Common::TakeSnapshotTGARect( const char *pFilename, int x, int y, int w, int h, int resampleWidth, int resampleHeight, bool bPFM, CubeMapFaceIndex_t faceIndex )
{
    if ( IsX360() )
    {
        Assert( 0 );
        return;
    }

    if ( bPFM )
    {
        TakeSnapshotPFMRect( pFilename, x, y, w, h, resampleWidth, resampleHeight, faceIndex );
        return;
    }

    // bitmap bits
    uint8 *pImage = new uint8[ w * h * 4 ];
    uint8 *pImage1 = new uint8[ resampleWidth * resampleHeight * 4 ];

    // Get Bits from the material system
    ReadScreenPixels( x, y, w, h, pImage, IMAGE_FORMAT_RGBA8888 );

    Assert( w == h ); // garymcthack - this only works for square images

    ImageLoader::ResampleInfo_t info;
    info.m_pSrc = pImage;
    info.m_pDest = pImage1;
    info.m_nSrcWidth = w;
    info.m_nSrcHeight = h;
    info.m_nDestWidth = resampleWidth;
    info.m_nDestHeight = resampleHeight;
    info.m_flSrcGamma = 1.0f;
    info.m_flDestGamma = 1.0f;

    if( !ImageLoader::ResampleRGBA8888( info ) )
    {
        Sys_Error( "Can't resample\n" );
    }
    
    CUtlBuffer outBuf;
    if ( TGAWriter::WriteToBuffer( pImage1, outBuf, resampleWidth, resampleHeight, IMAGE_FORMAT_RGBA8888, IMAGE_FORMAT_RGBA8888 ) )
    {
        if ( !g_pFileSystem->WriteFile( pFilename, NULL, outBuf ) )
        {
            Error( "Couldn't write bitmap data snapshot to file %s.\n", pFilename );
        }
		else
		{
			DevMsg( "Screenshot: %dx%d saved to '%s'.\n", w, h, pFilename );
		}
    }

    delete[] pImage1;
    delete[] pImage;
    materials->SwapBuffers();
}


//-----------------------------------------------------------------------------
// Purpose: Writes the data in *data to the sequentially number .bmp file filename
// Input  : *filename - 
//          width - 
//          height - 
//          depth - 
//          *data - 
// Output : static void
//-----------------------------------------------------------------------------
static void VID_ProcessMovieFrame( const MovieInfo_t& info, bool jpeg, const char *filename, int width, int height, byte *data )
{
    CUtlBuffer outBuf;
    bool bSuccess = false;
    if ( jpeg )
    {
        bSuccess = videomode->TakeSnapshotJPEGToBuffer( outBuf, info.jpeg_quality );
    }
    else
    {
        bSuccess = TGAWriter::WriteToBuffer( data, outBuf, width, height, IMAGE_FORMAT_BGR888, IMAGE_FORMAT_RGB888 );
    }

    if ( bSuccess )
    {
        if ( !g_pFileSystem->WriteFile( filename, NULL, outBuf ) )
        {
            Warning( "Couldn't write movie snapshot to file %s.\n", filename );
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), "endmovie\n" );
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Store current frame to numbered .bmp file
// Input  : *pFilename - 
//-----------------------------------------------------------------------------
void CVideoMode_Common::WriteMovieFrame( const MovieInfo_t& info )
{
	if ( IsX360() )
		return;
    char const *pMovieName = info.moviename;
    int nMovieFrame = info.movieframe;

    if ( g_LostVideoMemory )
        return;

    if ( !pMovieName[0] )
    {
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "endmovie\n" );
        ConMsg( "Tried to write movie buffer with no filename set!\n" );
        return;
    }

    int imagesize = GetModeWidth() * GetModeHeight();
    BGR888_t *hp = new BGR888_t[ imagesize ];
    if ( hp == NULL )
    {
        Sys_Error( "Couldn't allocate bitmap header to snapshot.\n" );
    }

    // Get Bits from material system
    ReadScreenPixels( 0, 0, GetModeWidth(), GetModeHeight(), hp, IMAGE_FORMAT_BGR888 );

    // Store frame to disk
    if ( info.DoTga() )
    {
        VID_ProcessMovieFrame( info, false, va( "%s%04d.tga", pMovieName, nMovieFrame ), 
            GetModeWidth(), GetModeHeight(), (unsigned char*)hp );
    }

    if ( info.DoJpg() )
    {
        VID_ProcessMovieFrame( info, true, va( "%s%04d.jpg", pMovieName, nMovieFrame ), 
            GetModeWidth(), GetModeHeight(), (unsigned char*)hp );
    }

    if ( info.DoAVI() )
    {
        avi->AppendMovieFrame( g_hCurrentAVI, hp );
    }

    delete[] hp;
}

//-----------------------------------------------------------------------------
// Purpose: Expanded data destination object for CUtlBuffer output
//-----------------------------------------------------------------------------
struct JPEGDestinationManager_t
{
    struct jpeg_destination_mgr pub; // public fields
    
    CUtlBuffer  *pBuffer;       // target/final buffer
    byte        *buffer;        // start of temp buffer
};

// choose an efficiently bufferaable size
#define OUTPUT_BUF_SIZE  4096   

//-----------------------------------------------------------------------------
// Purpose:  Initialize destination --- called by jpeg_start_compress
//  before any data is actually written.
//-----------------------------------------------------------------------------
METHODDEF(void) init_destination (j_compress_ptr cinfo)
{
    JPEGDestinationManager_t *dest = ( JPEGDestinationManager_t *) cinfo->dest;
    
    // Allocate the output buffer --- it will be released when done with image
    dest->buffer = (byte *)
        (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
        OUTPUT_BUF_SIZE * sizeof(byte));
    
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

//-----------------------------------------------------------------------------
// Purpose: Empty the output buffer --- called whenever buffer fills up.
// Input  : boolean - 
//-----------------------------------------------------------------------------
METHODDEF(boolean) empty_output_buffer (j_compress_ptr cinfo)
{
    JPEGDestinationManager_t *dest = ( JPEGDestinationManager_t * ) cinfo->dest;
    
    CUtlBuffer *buf = dest->pBuffer;

    // Add some data
    buf->Put( dest->buffer, OUTPUT_BUF_SIZE );
    
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
    
    return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Terminate destination --- called by jpeg_finish_compress
// after all data has been written.  Usually needs to flush buffer.
//
// NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
// application must deal with any cleanup that should happen even
// for error exit.
//-----------------------------------------------------------------------------
METHODDEF(void) term_destination (j_compress_ptr cinfo)
{
    JPEGDestinationManager_t *dest = (JPEGDestinationManager_t *) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;
    
    CUtlBuffer *buf = dest->pBuffer;

    /* Write any data remaining in the buffer */
    if (datacount > 0) 
    {
        buf->Put( dest->buffer, datacount );
    }
}

//-----------------------------------------------------------------------------
// Purpose: Set up functions for writing data to a CUtlBuffer instead of FILE *
//-----------------------------------------------------------------------------
GLOBAL(void) jpeg_UtlBuffer_dest (j_compress_ptr cinfo, CUtlBuffer *pBuffer )
{
    JPEGDestinationManager_t *dest;
    
    /* The destination object is made permanent so that multiple JPEG images
    * can be written to the same file without re-executing jpeg_stdio_dest.
    * This makes it dangerous to use this manager and a different destination
    * manager serially with the same JPEG object, because their private object
    * sizes may be different.  Caveat programmer.
    */
    if (cinfo->dest == NULL) {  /* first time for this JPEG object? */
        cinfo->dest = (struct jpeg_destination_mgr *)
            (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
            sizeof(JPEGDestinationManager_t));
    }
    
    dest = ( JPEGDestinationManager_t * ) cinfo->dest;

    dest->pub.init_destination      = init_destination;
    dest->pub.empty_output_buffer   = empty_output_buffer;
    dest->pub.term_destination      = term_destination;
    dest->pBuffer                   = pBuffer;
}

bool CVideoMode_Common::TakeSnapshotJPEGToBuffer( CUtlBuffer& buf, int quality )
{
#if !defined( _GAMECONSOLE )
    if ( g_LostVideoMemory )
        return false;

    // Validate quality level
    quality = clamp( quality, 1, 100 );

    // Allocate space for bits
    uint8 *pImage = new uint8[ GetModeWidth() * 3 * GetModeHeight() ];
    if ( !pImage )
    {
        Msg( "Unable to allocate %i bytes for image\n", GetModeWidth() * 3 * GetModeHeight() );
        return false;
    }

    // Get Bits from the material system
    ReadScreenPixels( 0, 0, GetModeWidth(), GetModeHeight(), pImage, IMAGE_FORMAT_RGB888 );

    JSAMPROW row_pointer[1];     // pointer to JSAMPLE row[s]
    int row_stride;              // physical row width in image buffer

    // stderr handler
    struct jpeg_error_mgr jerr;

    // compression data structure
    struct jpeg_compress_struct cinfo;

    row_stride = GetModeWidth() * 3; // JSAMPLEs per row in image_buffer

    // point at stderr
    cinfo.err = jpeg_std_error(&jerr);

    // create compressor
    jpeg_create_compress(&cinfo);

    // Hook CUtlBuffer to compression
    jpeg_UtlBuffer_dest(&cinfo, &buf );

    // image width and height, in pixels
    cinfo.image_width = GetModeWidth();
    cinfo.image_height = GetModeHeight();
    // RGB is 3 componnent
    cinfo.input_components = 3;
    // # of color components per pixel
    cinfo.in_color_space = JCS_RGB;

    // Apply settings
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE );

    // Start compressor
    jpeg_start_compress(&cinfo, TRUE);
    
    // Write scanlines
    while ( cinfo.next_scanline < cinfo.image_height ) 
    {
        row_pointer[ 0 ] = &pImage[ cinfo.next_scanline * row_stride ];
        jpeg_write_scanlines( &cinfo, row_pointer, 1 );
    }

    // Finalize image
    jpeg_finish_compress(&cinfo);

    // Cleanup
    jpeg_destroy_compress(&cinfo);
    
    delete[] pImage;

#else
    // not supporting
    Assert( 0 );
#endif
    return true;
}

//-----------------------------------------------------------------------------
// Purpose: Write vid.buffer out as a .jpg file
// Input  : *pFilename - 
//-----------------------------------------------------------------------------
void CVideoMode_Common::TakeSnapshotJPEG( const char *pFilename, int quality )
{
#if !defined( _X360 )
    Assert( pFilename );

    // Output buffer
    CUtlBuffer buf( 0, 0 );
    TakeSnapshotJPEGToBuffer( buf, quality );

    int finalSize = 0;
    FileHandle_t fh = g_pFileSystem->Open( pFilename, "wb" );
    if ( FILESYSTEM_INVALID_HANDLE != fh )
    {
        g_pFileSystem->Write( buf.Base(), buf.TellPut(), fh );
        finalSize = g_pFileSystem->Tell( fh );
        g_pFileSystem->Close( fh );
    }

// Show info to console.
    char orig[ 64 ];
    char final[ 64 ];
    Q_strncpy( orig, Q_pretifymem( GetModeWidth() * 3 * GetModeHeight(), 2 ), sizeof( orig ) );
    Q_strncpy( final, Q_pretifymem( finalSize, 2 ), sizeof( final ) );

    Msg( "Wrote '%s':  %s (%dx%d) compresssed (quality %i) to %s\n",
        pFilename, orig, GetModeWidth(), GetModeHeight(), quality, final );

	if ( finalSize > 0 )
	{
		char szPath[MAX_PATH];
		szPath[0] = 0;
		if ( g_pFileSystem->GetLocalPath( pFilename, szPath, sizeof(szPath) ) )
		{
			AddScreenshotToSteam( szPath, GetModeWidth(), GetModeHeight() );
		}
	}

#else
    Assert( 0 );
#endif
}

//-----------------------------------------------------------------------------
// The version of the VideoMode class for the material system 
//-----------------------------------------------------------------------------
class CVideoMode_MaterialSystem: public CVideoMode_Common
{
public:
    typedef CVideoMode_Common BaseClass;
    
    CVideoMode_MaterialSystem( );

    virtual bool        Init( );
    virtual void        Shutdown( void );
    virtual void        SetGameWindow( void *hWnd );
	virtual bool		SetMode( int nWidth, int nHeight, bool bWindowed, bool bNoWindowBorder );
    virtual void        ReleaseVideo( void );
    virtual void        RestoreVideo( void );
    virtual void        AdjustForModeChange( void );
    virtual void        ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format );

private:
    virtual void        ReleaseFullScreen( void );
    virtual void        ChangeDisplaySettingsToFullscreen( int nWidth, int nHeight, int nBPP, bool bDesktopFriendlyFullscreen );
};

static void VideoMode_AdjustForModeChange( void )
{
    ( ( CVideoMode_MaterialSystem * )videomode )->AdjustForModeChange();
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CVideoMode_MaterialSystem::CVideoMode_MaterialSystem( )
{
}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
bool CVideoMode_MaterialSystem::Init( )
{
    m_bSetModeOnce = false;
    m_bPlayedStartupVideo = false;

    // we only support 32-bit rendering.
    int bitsperpixel = 32;

    bool bAllowSmallModes = false;
    if ( CommandLine()->FindParm( "-small" ) )
    {
        bAllowSmallModes = true;
    }

    int nAdapter = materials->GetCurrentAdapter();
    int nModeCount = materials->GetModeCount( nAdapter );

    int nDesktopWidth, nDesktopHeight, nDesktopRefresh;
    game->GetDesktopInfo( nDesktopWidth, nDesktopHeight, nDesktopRefresh );

    for ( int i = 0; i < nModeCount; i++ )
    {
        MaterialVideoMode_t info;
        materials->GetModeInfo( nAdapter, i, info );

        if ( info.m_Width < 640 || info.m_Height < 480 )
        {
            if ( !bAllowSmallModes )
                continue;
        }

        // make sure we don't already have this mode listed
        bool bAlreadyInList = false;
        for ( int j = 0; j < m_nNumModes; j++ )
        {
            if ( info.m_Width == m_rgModeList[ j ].width && info.m_Height == m_rgModeList[ j ].height )
            {
                // choose the highest refresh rate available for each mode up to the desktop rate

                // if the new mode is valid and current is invalid or not as high, choose the new one
                if ( info.m_RefreshRate <= nDesktopRefresh && (m_rgModeList[j].refreshRate > nDesktopRefresh || m_rgModeList[j].refreshRate < info.m_RefreshRate) )
                {
                    m_rgModeList[j].refreshRate = info.m_RefreshRate;
                }
                bAlreadyInList = true;
                break;
            }
        }

        if ( bAlreadyInList )
            continue;

        m_rgModeList[ m_nNumModes ].width = info.m_Width;
        m_rgModeList[ m_nNumModes ].height = info.m_Height;
        m_rgModeList[ m_nNumModes ].bpp = bitsperpixel;
        // NOTE: Don't clamp this to the desktop rate because we want to be sure we've only added
        // modes that the adapter can do and maybe the desktop rate isn't available in this mode
        m_rgModeList[ m_nNumModes ].refreshRate = info.m_RefreshRate;

        if ( ++m_nNumModes >= MAX_MODE_LIST )
            break;
    }

    // Sort modes for easy searching later
    if ( m_nNumModes > 1 )
    {
        qsort( (void *)&m_rgModeList[0], m_nNumModes, sizeof(vmode_t), VideoModeCompare );
    }

    materials->AddModeChangeCallBack( &VideoMode_AdjustForModeChange );
    SetInitialized( true );
    return true;
}


void CVideoMode_MaterialSystem::Shutdown()
{
    materials->RemoveModeChangeCallBack( &VideoMode_AdjustForModeChange );
    BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Sets the video mode
//-----------------------------------------------------------------------------
bool CVideoMode_MaterialSystem::SetMode( int nWidth, int nHeight, bool bWindowed, bool bNoWindowBorder )
{
	extern void S_ClearBuffer();

	// if the sound engine is running, make it silent since this will take a while
	S_ClearBuffer();

    // Necessary for mode selection to work
    int nFoundMode = FindVideoMode( nWidth, nHeight, bWindowed );
	vmode_t videoMode = *GetMode( nFoundMode );
	
	if ( bWindowed && ( nFoundMode == 0 ) && ( videoMode.width != nWidth || videoMode.height != nHeight ) )
	{
		// Setting a custom windowed size
		videoMode.width = nWidth;
		videoMode.height = nHeight;
	}

    // update current video state
    MaterialSystem_Config_t config = *g_pMaterialSystemConfig;
	config.m_VideoMode.m_Width = videoMode.width;
	config.m_VideoMode.m_Height = videoMode.height;

#ifdef DEDICATED
    config.m_VideoMode.m_RefreshRate = 60;
#else
	config.m_VideoMode.m_RefreshRate = GetRefreshRateForMode( &videoMode );
#endif
    
    config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, bWindowed );
	config.SetFlag( MATSYS_VIDCFG_FLAGS_NO_WINDOW_BORDER, bNoWindowBorder );

#if defined( _X360 )
	XVIDEO_MODE xvideoMode;
	XGetVideoMode( &xvideoMode );
	config.SetFlag( MATSYS_VIDCFG_FLAGS_SCALE_TO_OUTPUT_RESOLUTION, (DWORD)nWidth != xvideoMode.dwDisplayWidth || (DWORD)nHeight != xvideoMode.dwDisplayHeight );
    if ( nHeight == 480 || nWidth == 576 )
    {
        // Use 2xMSAA for standard def (see mat_software_aa_strength for fake hi-def aa)
        // FIXME: shuffle the EDRAM surfaces to allow 4xMSAA for standard def
        //        (they would overlap & trash each other with the current arrangement)
        // NOTE: This should affect 640x480 and 848x480 (which is also used for 640x480 widescreen), and PAL 640x576
        config.m_nAASamples = 2;
    }
#endif

    // FIXME: This is trash. We have to do *different* things depending on how we're setting the mode!
    if ( !m_bSetModeOnce )
    {
		//Debugger();
		
        if ( !materials->SetMode( (void*)game->GetMainWindow(), config ) )
            return false;

        m_bSetModeOnce = true;

        InitStartupScreen();
        return true;
    }

    // update the config 
    OverrideMaterialSystemConfig( config );
    return true;
}


//-----------------------------------------------------------------------------
// Called by the material system when mode changes after a call to OverrideConfig
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::AdjustForModeChange( void )
{
    if ( InEditMode() )
        return;

    // get previous size
    int nOldWidth = GetModeWidth();
    int nOldHeight = GetModeHeight();

    // Get the new mode info from the config record
    int nNewWidth = g_pMaterialSystemConfig->m_VideoMode.m_Width;
    int nNewHeight = g_pMaterialSystemConfig->m_VideoMode.m_Height;
    bool bWindowed = g_pMaterialSystemConfig->Windowed();
	bool bNoWindowBorder = g_pMaterialSystemConfig->NoWindowBorder();

    // reset the window size
    CMatRenderContextPtr pRenderContext( materials );

#if ( !defined( _GAMECONSOLE ) && defined ( WIN32 ) )
	if ( !IsGameConsole() && !IsWindowedMode() && bWindowed )
	{
		// Release fullscreen before going from windowed to fullscreen to avoid the case on Vista where we go from 
		// fullscreen 640x480 to a higher reswindowed, but the rendertarget stays at 640x480 and the window is sized arbitrarily.
		ReleaseFullScreen( );
		ShowWindow( (HWND)game->GetMainWindow(), SW_SHOWNORMAL );
	}
#endif

	ResetCurrentModeForNewResolution( nNewWidth, nNewHeight, bWindowed, bNoWindowBorder );
	AdjustWindow( GetModeWidth(), GetModeHeight(), GetModeBPP(), IsWindowedMode(), NoWindowBorder() );
    MarkClientViewRectDirty();
    pRenderContext->Viewport( 0, 0, GetModeWidth(), GetModeHeight() );

    // fixup vgui
    vgui::surface()->OnScreenSizeChanged( nOldWidth, nOldHeight );
	
	game->OnScreenSizeChanged( nOldWidth, nOldHeight );
}


//-----------------------------------------------------------------------------
// Sets the game window in editor mode
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::SetGameWindow( void *hWnd )
{
    if ( hWnd == NULL )
    {
        // No longer confine rendering into this view
        materials->SetView( NULL );
        return;
    }

    // When running in edit mode, just use hammer's window
    game->SetGameWindow( (HWND)hWnd );

    // FIXME: Move this code into the _MaterialSystem version of CVideoMode
    // In editor mode, the mode width + height is equal to the desktop width + height
    MaterialVideoMode_t mode;
    materials->GetDisplayMode( mode );
    m_bWindowed = true;
    m_nModeWidth = mode.m_Width;
    m_nModeHeight = mode.m_Height;
	m_bNoWindowBorder = false;

    materials->SetView( game->GetMainWindow() );
}


//-----------------------------------------------------------------------------
// Called when we lose the video buffer (alt-tab)
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::ReleaseVideo( void )
{
    if ( IsX360() )
        return;

    if ( IsWindowedMode() )
        return;

    ReleaseFullScreen();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::RestoreVideo( void )
{
    if ( IsX360() )
        return;

    if ( IsWindowedMode() )
        return;

#if defined( WIN32 ) && !defined( USE_SDL )
    ShowWindow( (HWND)game->GetMainWindow(), SW_SHOWNORMAL );
#elif defined( OSX ) && defined( PLATFORM_64BITS )
    Assert( !"OSX-64 unimpl" );
#elif OSX
    ShowWindow( (WindowRef)game->GetMainWindow() );
    CollapseWindow( (WindowRef)game->GetMainWindow(), false );
#elif LINUX
// 	XMapWindow( g_pLauncherMgr->GetDisplay(), (Window)game->GetMainWindow() );
// !!! FIXME: Mapping isn't really what we want here.
#elif _WIN32
#else
#error
#endif
	AdjustWindow( GetModeWidth(), GetModeHeight(), GetModeBPP(), IsWindowedMode(), NoWindowBorder() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::ReleaseFullScreen( void )
{
    if ( IsGameConsole() )
        return;

    if ( IsWindowedMode() )
        return;

#if defined( WIN32 ) && !defined( USE_SDL )
    // Hide the main window
    ChangeDisplaySettings( NULL, 0 );
    ShowWindow( (HWND)game->GetMainWindow(), SW_MINIMIZE );
#elif defined( OSX ) && defined( PLATFORM_64BITS )
    Assert( !"OSX-64 unimpl" );
#elif OSX
    CollapseWindow( (WindowRef)game->GetMainWindow(), true );

	if (!CommandLine()->FindParm("-keepmousehooked"))
	{
		 //CGAssociateMouseAndMouseCursorPosition (TRUE);
	}
	CGDisplayShowCursor (kCGDirectMainDisplay);
#elif LINUX
//	XUnmapWindow( g_pLauncherMgr->GetDisplay(), (Window)game->GetMainWindow() );
// !!! FIXME: Unmapping isn't really what we want here.
#elif _WIN32
#else
#error
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Use Change Display Settings to go Full Screen
//-----------------------------------------------------------------------------
void CVideoMode_MaterialSystem::ChangeDisplaySettingsToFullscreen( int nWidth, int nHeight, int nBPP, bool bDesktopFriendlyFullscreen )
{
	if ( IsGameConsole() )
        return;

    if ( IsWindowedMode() )
        return;

#if defined( USE_SDL )
	g_pLauncherMgr->SetWindowFullScreen( true, nWidth, nHeight, bDesktopFriendlyFullscreen );
#elif defined( WIN32 )
    DEVMODE dm;
    memset(&dm, 0, sizeof(dm));

    dm.dmSize       = sizeof( dm );
    dm.dmPelsWidth  = nWidth;
    dm.dmPelsHeight = nHeight;
    dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    dm.dmBitsPerPel = nBPP;

    // FIXME: Fix direct reference of refresh rate from config record
    int freq = g_pMaterialSystemConfig->m_VideoMode.m_RefreshRate;
    if ( freq >= 60 )
    {
        dm.dmDisplayFrequency = freq;
        dm.dmFields |= DM_DISPLAYFREQUENCY;
    }

    ChangeDisplaySettings( &dm, CDS_FULLSCREEN );
#else
	if (!CommandLine()->FindParm("-hushasserts"))
	{
	Assert( !"Impl me" );
	}
#endif
}

void CVideoMode_MaterialSystem::ReadScreenPixels( int x, int y, int w, int h, void *pBuffer, ImageFormat format )
{
    if ( !g_LostVideoMemory )
    {
        bool bReadPixelsFromFrontBuffer = g_pMaterialSystemHardwareConfig->ReadPixelsFromFrontBuffer();
        if( IsPS3() || bReadPixelsFromFrontBuffer )
        {
            Shader_SwapBuffers();
        }

        CMatRenderContextPtr pRenderContext( materials );

        Rect_t rect;
        rect.x = x;
        rect.y = y;
        rect.width = w;
        rect.height = h;

        pRenderContext->ReadPixelsAndStretch( &rect, &rect, (unsigned char*)pBuffer, format, w * ImageLoader::SizeInBytes( format ) );

		if( IsPS3() || bReadPixelsFromFrontBuffer )
        {
            Shader_SwapBuffers();
        }
    }
    else
    {
        int nBytes = ImageLoader::GetMemRequired( w, h, 1, format, false );
        memset( pBuffer, 0, nBytes );
    }
}


//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------

IVideoMode *videomode = ( IVideoMode * )NULL;

void VideoMode_Create( )
{
    videomode = new CVideoMode_MaterialSystem;
    Assert( videomode );
}

void VideoMode_Destroy()
{
    if ( videomode )
    {
        CVideoMode_MaterialSystem *pVideoMode_MS = static_cast<CVideoMode_MaterialSystem*>(videomode);
        delete pVideoMode_MS;
        videomode = NULL;
    }
}
