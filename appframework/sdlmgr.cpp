//====== Copyright  1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: An application framework  
//
//=============================================================================//

#include "SDL.h"
#include "SDL_opengl.h"

#include "appframework/ilaunchermgr.h"
#include "tier1/keyvalues.h"
#include "filesystem.h"

#include "togl/rendermechanism.h"
#include "tier0/fasttimer.h"


// NOTE: This has to be the last file included! (turned off below, since this is included like a header)
#include "tier0/memdbgon.h"

#ifdef GLMPRINTF
#undef GLMPRINTF
#endif

#if GLMDEBUG
#define GLMPRINTF(args)	printf args
#else
#define GLMPRINTF(args)
#endif

ConVar gl_blit_halfx( "gl_blit_halfx", "0" );
ConVar gl_blit_halfy( "gl_blit_halfy", "0" );
ConVar gl_swapdebug( "gl_swapdebug", "0");
ConVar gl_swaplimit( "gl_swaplimit", "0");
ConVar gl_swapinterval( "gl_swapinterval", "0");
ConVar gl_swaplimit_mt( "gl_swaplimit_mt", "3");
ConVar gl_disable_forced_vsync( "gl_disable_forced_vsync", "0" );
ConVar gl_swaptear( "gl_swaptear", "1" );

ConVar sdl_double_click_size( "sdl_double_click_size", "2" );
ConVar sdl_double_click_time( "sdl_double_click_time", "400" );

#if defined( DX_TO_GL_ABSTRACTION )
COpenGLEntryPoints *gGL = NULL;
#endif

#if defined( WIN32 ) && defined( DX_TO_GL_ABSTRACTION )
#define PATH_MAX MAX_PATH
#endif

static void DebugPrintf( const char *pMsg, ... )
{
	va_list args;
	va_start( args, pMsg );
	char buf[2048];
	V_vsnprintf( buf, sizeof( buf ), pMsg, args );
	va_end( args );

	Plat_DebugString( buf );
}

// #define SDLAPP_DEBUG
#ifdef SDLAPP_DEBUG
class LinuxAppFuncLogger
{
	public:
		LinuxAppFuncLogger( const char *funcName ) : m_funcName( funcName )
		{
			printf( ">%s\n", m_funcName );
		};
		
		LinuxAppFuncLogger( const char *funcName, char *fmt, ... )
		{
			m_funcName = funcName;

			va_list	vargs;
			va_start(vargs, fmt);
			vprintf( fmt, vargs );
			va_end( vargs );
		}
		
		~LinuxAppFuncLogger( )
		{
			printf( "<%s\n", m_funcName );
		};
	
		const char *m_funcName;
};
#define	SDLAPP_FUNC			LinuxAppFuncLogger _logger_( __FUNCTION__ )
#else
#define SDLAPP_FUNC
#endif


#if defined( DX_TO_GL_ABSTRACTION )
void	CheckGLError( int line )
{
	SDLAPP_FUNC;

	// Don't check this in enabled! glGetError() is extremely slow with threaded drivers.
	return;
	//char errbuf[1024];

	//borrowed from GLMCheckError.. slightly different
	
	
	GLenum errorcode = (GLenum)gGL->glGetError();
	//GLenum errorcode2 = 0;
	if ( errorcode != GL_NO_ERROR )
	{
		const char	*decodedStr = GLMDecode( eGL_ERROR, errorcode );

		printf( "\n(%d) GL Error %08x = '%s'", line, errorcode, decodedStr );
	}
}
#endif

//-----------------------------------------------------------------------------
#if !defined( DEDICATED )

void *VoidFnPtrLookup_GlMgr( const char *libname, const char *fn, bool &okay, const bool bRequired, void *fallback)
{
	void *retval = NULL;
	if ((!okay) && (!bRequired))  // always look up if required (so we get a complete list of crucial missing symbols).
		return NULL;

	// The SDL path would work on all these platforms, if we were using SDL there, too...
#if defined( USE_SDL )
	// SDL does the right thing, so we never need to use tier0 in this case.
	retval = SDL_GL_GetProcAddress(fn);
	//printf("CDynamicFunctionOpenGL: SDL_GL_GetProcAddress(\"%s\") returned %p\n", fn, retval);
	if ((retval == NULL) && (fallback != NULL))
	{
		//printf("CDynamicFunctionOpenGL: Using fallback %p for \"%s\"\n", fallback, fn);
		retval = fallback;
	}
#else
    // there's no glXGetProcAddress() equivalent for Mac OS X...it's just dlopen(), basically. Let tier0 handle that.
    retval = VoidFnPtrLookup_Tier0( fn, (void *) fallback);
    #error Unimplemented
#endif

	// Note that a non-NULL response doesn't mean it's safe to call the function!
	//  You always have to check that the extension is supported;
	//  an implementation MAY return NULL in this case, but it doesn't have to (and doesn't, with the DRI drivers).
	okay = (okay && (retval != NULL));
	if (bRequired && !okay)
	{
		// We can't continue execution, because one or more GL function pointers will be NULL.
		Error( "Could not find required OpenGL entry point '%s'! Either your video card is unsupported, or your OpenGL driver needs to be updated.\n", fn);
	}
	
	return retval;
}

class CSDLMgr : public ILauncherMgr
{
public:

	CSDLMgr();

// ILauncherMgr impls.
public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	
	virtual void *QueryInterface( const char *pInterfaceName );
	
	// Init, shutdown
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	virtual bool CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height, bool bDesktopFriendlyFullscreen );

	// Get the next N events. The function returns the number of events that were filled into your array.
	virtual int GetEvents( CCocoaEvent *pEvents, int nMaxEventsToReturn, bool debugEvents = false );

	// Set the mouse cursor position.
	virtual void SetCursorPosition( int x, int y );
	virtual void GetCursorPosition( int *px, int *py );
	
	virtual void *GetWindowRef() { return (void *)m_Window; }

	virtual void SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight, bool bDesktopFriendlyFullscreen );
	virtual bool IsWindowFullScreen() { return m_bFullScreen; }
	virtual void MoveWindow( int x, int y );
	virtual void SizeWindow( int width, int tall );
	virtual void PumpWindowsMessageLoop();
		
	virtual void DestroyGameWindow();
	virtual void SetApplicationIcon( const char *pchAppIconFile );
	
	virtual void GetMouseDelta( int &x, int &y, bool bIgnoreNextMouseDelta = false );

	virtual int GetActiveDisplayIndex();
	virtual void GetNativeDisplayInfo( int nDisplay, uint &nWidth, uint &nHeight, uint &nRefreshHz ); // Retrieve the size of the monitor (desktop)
	virtual void RenderedSize( uint &width, uint &height, bool set );	// either set or retrieve rendered size value (from dxabstract)
	virtual void DisplayedSize( uint &width, uint &height );			// query backbuffer size (window size whether FS or windowed)

#if defined( DX_TO_GL_ABSTRACTION )
	virtual void GetDesiredPixelFormatAttribsAndRendererInfo( uint **ptrOut, uint *countOut, GLMRendererInfoFields *rendInfoOut );

	virtual PseudoGLContextPtr	GetMainContext();
	// Get the NSGLContext for a window's main view - note this is the carbon windowref as an argument
	virtual PseudoGLContextPtr GetGLContextForWindow( void* windowref ) { return (PseudoGLContextPtr)m_GLContext; }
	virtual PseudoGLContextPtr CreateExtraContext();
	virtual void DeleteContext( PseudoGLContextPtr hContext );
	virtual bool MakeContextCurrent( PseudoGLContextPtr hContext );
	virtual GLMDisplayDB *GetDisplayDB( void );

	virtual void ShowPixels( CShowPixelsParams *params );
#endif

	virtual void GetStackCrawl( CStackCrawlParams *params );

	virtual void WaitUntilUserInput( int msSleepTime );

  	virtual InputCursorHandle_t LoadCursorFromFile( const char *pchFileName );

	virtual void FreeCursor( const InputCursorHandle_t pchCursor );
	virtual void SetCursorIcon( const InputCursorHandle_t pchCursor );
	
	// Post an event to the input event queue.
	// if debugEvent is true, post it to the debug event queue.
	void PostEvent( const CCocoaEvent &theEvent, bool debugEvent=false );

	// ask if an event is debug flavor or not.
	bool IsDebugEvent( CCocoaEvent& event );

	virtual void SetMouseVisible( bool bState );
	virtual void SetMouseCursor( SDL_Cursor *hCursor );
	virtual void SetForbidMouseGrab( bool bForbidMouseGrab ) { m_bForbidMouseGrab = bForbidMouseGrab; }

	virtual void OnFrameRendered();

	virtual double GetPrevGLSwapWindowTime() { return m_flPrevGLSwapWindowTime; }
	
	// Returns all dependent libraries
	virtual const AppSystemInfo_t* GetDependencies() {return NULL;}

	virtual void SetGammaRamp( const uint16 *pRed, const uint16 *pGreen, const uint16 *pBlue );

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	virtual void ForceSystemCursorVisible();
	virtual void UnforceSystemCursorVisible();
#endif
	
	// Returns the tier
	virtual AppSystemTier_t GetTier()
	{
		return APP_SYSTEM_TIER2;
	}
	// Reconnect to a particular interface
	virtual void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName ) {}

	// Called to create a game window that will be hidden, designed for
	// getting an OpenGL context going so we can begin initializing things.
    bool CreateHiddenGameWindow( const char *pTitle, bool bWindowed, int width, int height );

	virtual bool IsSingleton() { return false; }
		
private:
	void handleKeyInput( const SDL_Event &event );

#if defined( DX_TO_GL_ABSTRACTION )
	SDL_GLContext m_GLContext;
	GLuint m_readFBO;
	GLMDisplayDB *m_displayDB;
#endif

#if defined( OSX )
	bool					m_force_vsync;				// true if 10.6.4 + bad NV driver
#endif

	SDL_Window *m_Window;


	bool m_bCursorVisible;
	bool m_bSetMouseVisibleCalled;
	int m_nFramesCursorInvisibleFor;
	SDL_Cursor *m_hCursor;
	bool m_bSetMouseCursorCalled;

	bool m_bHasFocus;
	bool m_bFullScreen;
	bool m_bForbidMouseGrab;  // temporary setting showing if the mouse should
	                          // grab if possible.

	int m_nMouseXDelta;
	int m_nMouseYDelta;

	int m_ScreenWidth;
	int m_ScreenHeight;

	int m_renderedWidth;
	int m_rendererHeight;

	int m_WindowWidth;
	int m_WindowHeight;

    bool m_bExpectSyntheticMouseMotion;
    int  m_nMouseTargetX;
    int  m_nMouseTargetY;
    int  m_nWarpDelta;
    bool m_bRawInput;

	int m_lastKnownSwapInterval;	//-2 if unknown, 0/1/-1 otherwise
	int m_lastKnownSwapLimit;		//-1 if unknown, 0/1 otherwise

	int m_pixelFormatAttribs[32];
	int m_pixelFormatAttribCount;

	float m_flMouseXScale;
	float m_flMouseYScale;

	// !!! FIXME: can we rename these from "Cocoa"?
	CThreadMutex m_CocoaEventsMutex;					// use for either queue below
	CUtlLinkedList<CCocoaEvent,int> m_CocoaEvents;
	CUtlLinkedList<CCocoaEvent,int> m_DebugEvents;		// intercepted keys which wil be passed over to GLM

	uint m_keyModifierMask;
	uint32_t m_keyModifiers;
	uint32_t m_mouseButtons;

	bool m_bGotMouseButtonDown;
	Uint32 m_MouseButtonDownTimeStamp;
	int m_MouseButtonDownX;
	int m_MouseButtonDownY;
			
#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	int m_nForceCursorVisible;
	int m_nForceCursorVisiblePrev;
	SDL_Cursor* m_hSystemArrowCursor;
#endif

	double m_flPrevGLSwapWindowTime;

	bool m_bTextMode;
};

ILauncherMgr *g_pLauncherMgr = NULL;

void* CreateSDLMgr()
{
	if ( g_pLauncherMgr == NULL )
	{
		g_pLauncherMgr = new CSDLMgr();
	}
	return (void *)g_pLauncherMgr;
}

// Display index to show window on.
static bool g_bSDLDisplayindexSet = false;
static void sdl_displayindex_changed( IConVar *pConVar, const char *pOldString, float flOldValue );
ConVar sdl_displayindex( "sdl_displayindex", "-1", FCVAR_ARCHIVE | FCVAR_HIDDEN, "SDL fullscreen display index.", sdl_displayindex_changed );
static void sdl_displayindex_changed( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	int NumVideoDisplays = SDL_GetNumVideoDisplays();

	if ( ( sdl_displayindex.GetInt() < 0 ) || ( sdl_displayindex.GetInt() >= NumVideoDisplays ) )
	{
		sdl_displayindex.SetValue( 0 );
	}

	g_bSDLDisplayindexSet = true;
}


// Return display index of largest SDL display ( plus width & height ).
static int GetLargestDisplaySize( int& Width, int& Height )
{
	int nDisplay = 0;

	Width = 640;
	Height = 480;

	for ( int i = 0; i < SDL_GetNumVideoDisplays(); i++ )
	{
		SDL_Rect rect = { 0, 0, 0, 0 };

		SDL_GetDisplayBounds( i, &rect );

		if ( ( rect.w > Width ) || ( ( rect.w == Width ) && ( rect.h > Height ) ) )
		{
			Width = rect.w;
			Height = rect.h;

			nDisplay = i;
		}
	}

	return nDisplay;
}

CON_COMMAND( grab_window, "grab/ungrab window." )
{
	if ( g_pLauncherMgr && g_pLauncherMgr->GetWindowRef() )
	{
		SDL_bool bGrab;
		SDL_Window *pWindow = ( SDL_Window * )g_pLauncherMgr->GetWindowRef();

		if ( args.ArgC() >= 2 )
		{
			bGrab = ( args[ 1 ][ 0 ] == '1' ) ? SDL_TRUE : SDL_FALSE;
		}
		else
		{
			bGrab = SDL_GetWindowGrab( pWindow ) ? SDL_FALSE : SDL_TRUE;
		}

		g_pLauncherMgr->SetForbidMouseGrab( !bGrab );

		if ( bGrab != SDL_GetWindowGrab( pWindow ) )
		{
			Msg( "SetWindowGrab( %s )\n", bGrab ? "true" : "false" );
			SDL_SetWindowGrab( pWindow, bGrab );

			// force non-fullscreen windows to the foreground if grabbed, so you can't
			//  get your mouse locked to something in the background.
			if ( bGrab && !g_pLauncherMgr->IsWindowFullScreen() )
			{
				SDL_RaiseWindow( pWindow );
			}
		}
	}
}

CSDLMgr::CSDLMgr()
{
	m_Window = NULL;
	Init();
}

InitReturnVal_t CSDLMgr::Init()
{
	SDLAPP_FUNC;

	if (m_Window != NULL)
		return INIT_OK;  // already initialized.

#if ALLOW_TEXT_MODE
	m_bTextMode = CommandLine()->FindParm( "-textmode" );
#else
	m_bTextMode = false;
#endif

#if defined( WIN32 ) && defined( DX_TO_GL_ABSTRACTION )
	if ( !m_bTextMode ) 
		SDL_SetMainReady();
#endif

	// Default to using Xinerama and XRandR if available.

	// If someone is running multimon, we want the individual
	// screen sizes (not combined), so default to xinerama on.
	SDL_SetHint( "SDL_VIDEO_X11_XINERAMA", "1" );
	// We don't need xrandr since we're not changing the desktop resolution.
	SDL_SetHint( "SDL_VIDEO_X11_XRANDR", "0" );
	// Default to no XVidMode.
	SDL_SetHint( "SDL_VIDEO_X11_XVIDMODE", "0" );

	if (!m_bTextMode && !SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
			Error( "SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError() );

#if defined( DX_TO_GL_ABSTRACTION )
		if ( CommandLine()->FindParm( "-gl_debug" ) )
		{
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG );
		}

		if (SDL_GL_LoadLibrary(NULL) == -1)
			Error( "SDL_GL_LoadLibrary(NULL) failed: %s", SDL_GetError() );
#endif
	}

	if ( !m_bTextMode )
		Msg("SDL video target is '%s'\n", SDL_GetCurrentVideoDriver());

	m_bForbidMouseGrab = true;
	if ( !CommandLine()->FindParm( "-nomousegrab" ) && CommandLine()->FindParm( "-mousegrab" ) )
	{
		m_bForbidMouseGrab = false;
	}

#if defined( DX_TO_GL_ABSTRACTION )
	m_GLContext = NULL;
	m_readFBO = 0;
	m_displayDB = NULL;
#endif

	m_bCursorVisible = true;
	m_bSetMouseVisibleCalled = false;
	m_nFramesCursorInvisibleFor = 0;
	m_hCursor = NULL;
	m_bSetMouseCursorCalled = false;


	m_bHasFocus = true;
	m_keyModifiers = 0;
	m_keyModifierMask = 0;
	m_mouseButtons = 0;

	m_Window = NULL;
	m_bFullScreen = false;
	m_nMouseXDelta = 0;
	m_nMouseYDelta = 0;
	m_ScreenWidth = 0;
	m_ScreenHeight = 0;
	m_renderedWidth = 0;
	m_rendererHeight = 0;
	m_WindowWidth = 0;
	m_WindowHeight = 0;
	m_pixelFormatAttribCount = 0;
	m_lastKnownSwapInterval = -2;
	m_lastKnownSwapLimit = -1;
	m_flMouseXScale = 1.0f;
	m_flMouseYScale = 1.0f;

	m_bGotMouseButtonDown = false;
	m_MouseButtonDownTimeStamp = 0;
	m_MouseButtonDownX = 0;
	m_MouseButtonDownY = 0;

	m_bExpectSyntheticMouseMotion = false;
	m_nMouseTargetX = 0;
	m_nMouseTargetY = 0;
	m_nWarpDelta = 0;
	m_bRawInput = false;

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	m_nForceCursorVisible = 0;
	m_nForceCursorVisiblePrev = 0;
	m_hSystemArrowCursor = SDL_CreateSystemCursor( SDL_SYSTEM_CURSOR_ARROW );
#endif
			
	m_flPrevGLSwapWindowTime = 0.0f;

	memset(m_pixelFormatAttribs, '\0', sizeof (m_pixelFormatAttribs));

	int *attCursor = m_pixelFormatAttribs;

#define SET_GL_ATTR(key,value) \
	*(attCursor++) = (int) (key); \
	*(attCursor++) = (int) (value);

	SET_GL_ATTR(SDL_GL_RED_SIZE, 8);
	SET_GL_ATTR(SDL_GL_GREEN_SIZE, 8);
	SET_GL_ATTR(SDL_GL_BLUE_SIZE, 8);
	SET_GL_ATTR(SDL_GL_ALPHA_SIZE, 8);
	SET_GL_ATTR(SDL_GL_DOUBLEBUFFER, 1);

#ifdef OSX
	// no explicit depth buffer is needed since FBO RT's are made for that
	SET_GL_ATTR(SDL_GL_DEPTH_SIZE, 0);
#else
	SET_GL_ATTR(SDL_GL_DEPTH_SIZE, 24);
	SET_GL_ATTR(SDL_GL_STENCIL_SIZE, 8);
#endif

	SET_GL_ATTR(SDL_GL_ACCELERATED_VISUAL, 1);

//	Disabled due to reports of failures on some Intel GPU's on Linux.
//	SET_GL_ATTR(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#undef SET_GL_ATTR


	m_pixelFormatAttribCount = (attCursor - &m_pixelFormatAttribs[0]) / 2;

	// we need a GL context before we dig down further, since we're calling
	//  GL entry points, but the game hasn't made a window yet. So it's time
	//  to make a window! We make a 640x480 one here, and later, when asked
	//  to really actually make a window, we just resize the one we built here.
    if ( !CreateHiddenGameWindow( "", true, 640, 480 ) )
		Error( "CreateGameWindow failed" );

	if ( !m_bTextMode )
		SDL_HideWindow( m_Window );

	return INIT_OK;
}

bool CSDLMgr::Connect( CreateInterfaceFn factory )
{
	SDLAPP_FUNC;

	return true;
}

void CSDLMgr::Disconnect()
{
	SDLAPP_FUNC;

}

void *CSDLMgr::QueryInterface( const char *pInterfaceName )
{
	SDLAPP_FUNC;
#if defined(USE_SDL)
	if ( !Q_stricmp( pInterfaceName, SDLMGR_INTERFACE_VERSION ) )
		return this;
#endif
	return NULL;
}

void CSDLMgr::Shutdown()
{
	SDLAPP_FUNC;

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	SDL_FreeCursor( m_hSystemArrowCursor );
#endif

	if (gGL && m_readFBO)
		gGL->glDeleteFramebuffersEXT(1, &m_readFBO);
	m_readFBO = 0;

	DestroyGameWindow();
	SDL_GL_UnloadLibrary();
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool CSDLMgr::CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height, bool bDesktopFriendlyFullscreen )
{
	SDLAPP_FUNC;

	if( ( width <= 0 ) || ( height <= 0 ) )
	{
		// Don't mess with current width, height - use current (or sane defaults).
		uint defaultWidth = 0;
		uint defaultHeight = 0;
		uint defaultRefreshHz = 0; // Not used

		int displayindex = sdl_displayindex.GetInt();
		this->GetNativeDisplayInfo( displayindex, defaultWidth, defaultHeight, defaultRefreshHz );

		if ( 0 == defaultWidth ) defaultWidth = 1024;
		if ( 0 == defaultHeight ) defaultHeight = 768;

		width = m_WindowWidth ? m_WindowWidth : defaultWidth;
		height = m_WindowHeight ? m_WindowHeight : defaultHeight;
	}

	if ( m_Window )
	{
		if ( pTitle )
		{
			SDL_SetWindowTitle( m_Window, pTitle );
		}

		if ( m_bFullScreen != !bWindowed )
		{
			SetWindowFullScreen( !bWindowed, width, height, bDesktopFriendlyFullscreen );
			MoveWindow( 0, 0 );
		}
		else
		{
			SizeWindow( width, height );

			int displayindex = sdl_displayindex.GetInt();
			displayindex = displayindex < 0 ? 0 : displayindex;
			MoveWindow( SDL_WINDOWPOS_CENTERED_DISPLAY( displayindex ), SDL_WINDOWPOS_CENTERED_DISPLAY( displayindex ) );
		}

		SDL_ShowWindow( m_Window );
		SDL_RaiseWindow( m_Window );

		return true;
	}

	if ( CreateHiddenGameWindow( pTitle, true, width, height ) )
	{
		SDL_ShowWindow( m_Window );
		return true;
	}
	else
	{
		return false;
	}
}

bool CSDLMgr::CreateHiddenGameWindow( const char *pTitle, bool bWindowed, int width, int height )
{
	if ( m_bTextMode )
		return true;

	m_bFullScreen = !bWindowed;

	// Always make the fullscreen dimensions match the current
	// state of the display. We'll render to a texture for whatever
	// resolution the user _actually_ wants and stretchblt it to the
	// real screen from there.
	SDL_DisplayMode mode;
	if (m_bFullScreen)
	{
		SDL_GetWindowDisplayMode(m_Window, &mode);
	}

#if defined( DX_TO_GL_ABSTRACTION )
	// Set up GL context...
	const int *attrib = m_pixelFormatAttribs;
	for (int i = 0; i < m_pixelFormatAttribCount; i++, attrib += 2)
		SDL_GL_SetAttribute((SDL_GLattr) attrib[0], attrib[1]);
#endif

	// no window yet? Create one now!
	int displayindex = sdl_displayindex.GetInt();
	displayindex = displayindex < 0 ? 0 : displayindex;
	int x = SDL_WINDOWPOS_CENTERED_DISPLAY( displayindex );
	int y = SDL_WINDOWPOS_CENTERED_DISPLAY( displayindex );
	int flags = SDL_WINDOW_HIDDEN;
#if defined( DX_TO_GL_ABSTRACTION )
	flags |= SDL_WINDOW_OPENGL;
#endif
	m_Window = SDL_CreateWindow( pTitle, x, y, width, height,  flags );

	if (m_Window == NULL)
		Error( "Failed to create SDL window: %s", SDL_GetError() );

#if defined( LINUX )
	SetAssertDialogParent( m_Window );
#endif

#ifdef OSX
	GLMRendererInfoFields rendererInfo;
	GetDisplayDB()->GetRendererInfo( 0, &rendererInfo );
	//-----------------------------------------------------------------------------------------
	//- enforce minimum system requirements : OS X 10.6.7, Snow Leopard Graphics Update, and no GMA950, X3100, ATI X1600/X1900, or NV G7x.
	
	if (!CommandLine()->FindParm("-glmnosystemcheck"))	// escape hatch
	{
		if ( rendererInfo.m_osComboVersion < 0x0A0607 )
		{
			Error( "This game requires OS X version 10.6.7 or higher" );
			exit(1);
		}
		
		// forbidden chipsets
		if ( rendererInfo.m_atiR5xx || rendererInfo.m_intel95x || rendererInfo.m_intel3100 || rendererInfo.m_nvG7x )
		{
			Error( "This game does not support this type of graphics processor" );
			exit(1);
		}
	}
#endif
	
	if (m_bFullScreen)
	{
		SDL_SetWindowDisplayMode(m_Window, &mode);
		SDL_SetWindowFullscreen(m_Window, SDL_TRUE);
	}

#if defined( DX_TO_GL_ABSTRACTION )
	m_GLContext = SDL_GL_CreateContext(m_Window);
	if (m_GLContext == NULL)
		Error( "Failed to create GL context: %s", SDL_GetError() );

	SDL_GL_MakeCurrent(m_Window, m_GLContext);

	// !!! FIXME: note for later...we never delete this context anywhere, I think.
	// !!! FIXME:  when we do get around to that, don't forget to delete/NULL gGL!

    static CDynamicFunctionOpenGL< true, const GLubyte *( APIENTRY *)(GLenum name), const GLubyte * > glGetString( NULL, "glGetString");
    static CDynamicFunctionOpenGL< true, GLvoid ( APIENTRY *)(GLenum pname, GLint *params), GLvoid > glGetIntegerv( NULL, "glGetIntegerv");

	const char *pszString = ( const char * )glGetString(GL_VENDOR);
	pszString = ( const char * )glGetString(GL_RENDERER);
	pszString = ( const char * )glGetString(GL_VERSION);
	pszString = ( const char * )glGetString(GL_EXTENSIONS);

	GLint whichProfile = 0;
	glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &whichProfile);

	// This comes up if SDL gets confused--this is just an early warning for what will be a hard failure later.
	if ((whichProfile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) == 0)
	{
		Warning( "SDL failed to create GL compatibility profile (whichProfile=%x!\n", whichProfile );
	}

	// If we specified -gl_debug, make sure the extension string is present now.
	if ( CommandLine()->FindParm( "-gl_debug" ) )
	{
		Assert( V_strstr(pszString, "GL_ARB_debug_output") );
	}

	gGL = GetOpenGLEntryPoints(VoidFnPtrLookup_GlMgr);

	// It is now safe to call any base GL entry point that's supplied by gGL.
	// You still need to explicitly test for extension entry points, though!

	if ( CommandLine()->FindParm( "-gl_dump_strings" ) )
	{
		DebugPrintf("GL_RENDERER: %s\n", (const char *) gGL->glGetString(GL_RENDERER));
		DebugPrintf("GL_VENDOR: %s\n", (const char *) gGL->glGetString(GL_VENDOR));
		DebugPrintf("GL_VERSION: %s\n", (const char *) gGL->glGetString(GL_VERSION));
		const char *exts = (const char *) gGL->glGetString(GL_EXTENSIONS);
		DebugPrintf("GL_EXTENSIONS:%s\n", exts ? "" : NULL);
		if (exts)
		{
			for (const char *ptr = exts; *ptr; ptr++)
				DebugPrintf("%c", *ptr == ' ' ? '\n' : *ptr);
			DebugPrintf("\n");
		}
		DebugPrintf("\n");
	}

	gGL->glGenFramebuffersEXT(1, &m_readFBO);

	gGL->glViewport(0, 0, width, height);    /* Reset The Current Viewport And Perspective Transformation */
	gGL->glScissor(0, 0, width, height);    /* Reset The Current Viewport And Perspective Transformation */

	// Blank out the initial window, so we're not looking at uninitialized
	//  video RAM trash until we start proper drawing.
	gGL->glClearColor(0,0,0,0);
	gGL->glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(m_Window);
	gGL->glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(m_Window);
	gGL->glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(m_Window);
#endif // DX_TO_GL_ABSTRACTION

	m_WindowWidth = width;
	m_WindowHeight = height;

	return true;
}

#if defined( DX_TO_GL_ABSTRACTION )

PseudoGLContextPtr	CSDLMgr::GetMainContext()
{
	SDLAPP_FUNC;

	return (PseudoGLContextPtr)m_GLContext;
}

PseudoGLContextPtr CSDLMgr::CreateExtraContext()
{
	SDLAPP_FUNC;

	return (PseudoGLContextPtr) SDL_GL_CreateContext(m_Window);
}

void CSDLMgr::DeleteContext( PseudoGLContextPtr hContext )
{
	SDLAPP_FUNC;

	// Don't delete the main one.
	if ( (SDL_GLContext)hContext != m_GLContext )
	{
		SDL_GL_MakeCurrent( m_Window, m_GLContext );
		SDL_GL_DeleteContext( (SDL_GLContext) hContext );
	}
}

bool CSDLMgr::MakeContextCurrent( PseudoGLContextPtr hContext )
{
	SDLAPP_FUNC;

	// We only ever have one GL context on Linux at the moment, so don't spam these calls.
	return SDL_GL_MakeCurrent(m_Window, (SDL_GLContext)hContext ) == 0;
}

#endif // DX_TO_GL_ABSTRACTION


int CSDLMgr::GetEvents( CCocoaEvent *pEvents, int nMaxEventsToReturn, bool debugEvent )
{
	SDLAPP_FUNC;

	m_CocoaEventsMutex.Lock();

	CUtlLinkedList<CCocoaEvent,int> &queue = debugEvent ? m_CocoaEvents : m_DebugEvents;

	int nAvailable = queue.Count();
	int nToWrite = MIN( nAvailable, nMaxEventsToReturn );

	CCocoaEvent *pCurEvent = pEvents;
	for ( int i=0; i < nToWrite; i++ )
	{
		int iHead = queue.Head();
		memcpy( pCurEvent, &queue[iHead], sizeof( CCocoaEvent ) );
		queue.Remove( iHead );
		++pCurEvent;
	}

	m_CocoaEventsMutex.Unlock();

	return nToWrite;
}

bool CSDLMgr::IsDebugEvent( CCocoaEvent& event )
{
	SDLAPP_FUNC;

	bool result = false;

	#if GLMDEBUG
		// simple rule for now, if the option key is involved, it's a debug key
		// but only if GLM debugging is builtin
		
		result |= ( (event.m_EventType == CocoaEvent_KeyDown) && ((event.m_ModifierKeyMask & (1<<eControlKey))!=0) );
	#endif
	
	return result;
}

// Set the mouse cursor position.
void CSDLMgr::SetCursorPosition( int x, int y )
{
	SDLAPP_FUNC;

	int windowHeight = 0;
	int windowWidth = 0;
	SDL_GetWindowSize((SDL_Window*)GetWindowRef(), &windowWidth, &windowHeight);

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int rx, ry, width, height;
	pRenderContext->GetViewport( rx, ry, width, height );
	if ( width != windowWidth || height != windowHeight  )
	{
		x = x * (float)windowWidth/width;
		y = y * (float)windowHeight/height;
	}

    m_bExpectSyntheticMouseMotion = true;
	m_nMouseTargetX = x;
	m_nMouseTargetY = y;

	SDL_WarpMouseInWindow(m_Window, x, y);
}

void CSDLMgr::GetCursorPosition( int *px, int *py )
{
	int x, y;
	SDL_GetMouseState(&x, &y);

	int windowHeight = 0;
	int windowWidth = 0;
	//unsigned int ignored;
	SDL_GetWindowSize((SDL_Window*)GetWindowRef(), &windowWidth, &windowHeight);

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int rx, ry, width, height;
	pRenderContext->GetViewport( rx, ry, width, height );
	
	if ( width != windowWidth || height != windowHeight  )
	{
		// scale the x/y back into the co-ords of the back buffer, not the scaled up window 
		//DevMsg( "Mouse x:%d y:%d %d %d %d %d\n", x, y, width, windowWidth, height, abs( height - windowHeight ) );
		x = x * (float)width/windowWidth;
		y = y * (float)height/windowHeight;
	}
	*px = x;
	*py = y;
}

void CSDLMgr::PostEvent( const CCocoaEvent &theEvent, bool debugEvent )
{
	SDLAPP_FUNC;

	m_CocoaEventsMutex.Lock();
	
	CUtlLinkedList<CCocoaEvent,int> &queue = debugEvent ? m_CocoaEvents : m_DebugEvents;
	queue.AddToTail( theEvent );
	
	m_CocoaEventsMutex.Unlock();
}

void CSDLMgr::SetMouseVisible( bool bState )
{
	SDLAPP_FUNC;

    m_bCursorVisible = bState;
    m_bSetMouseVisibleCalled = true;
}

void CSDLMgr::SetMouseCursor( SDL_Cursor *hCursor )
{
	SDLAPP_FUNC;

	if ( m_hCursor != hCursor )
	{
		if ( !hCursor )
		{
			// SDL_SetCursor( NULL ) just forces a cursor redraw, so don't ever bother doing that.
			SetMouseVisible( false );
		}
		else
		{
			SetMouseVisible( true );
		}

		m_hCursor = hCursor;
		m_bSetMouseCursorCalled = true;
	}
}

void CSDLMgr::OnFrameRendered()
{
	SDLAPP_FUNC;

	if ( !m_bHasFocus )
	{
		return;
	}

	if ( !m_bCursorVisible )
	{
		++m_nFramesCursorInvisibleFor;
	}

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
	if ( m_nForceCursorVisible > 0 )
	{
		// Edge case: We were just asked to force the cursor visible, so do it now.
		if ( m_nForceCursorVisiblePrev == 0 ) 
		{
			SDL_SetCursor( m_hSystemArrowCursor );
			SDL_SetWindowGrab( m_Window, SDL_FALSE );
			SDL_SetRelativeMouseMode( SDL_FALSE );
			SDL_ShowCursor( 1 );
		}

		// No further cursor processing.
		m_nForceCursorVisiblePrev = m_nForceCursorVisible;
		return;
	} 
	else if ( m_nForceCursorVisiblePrev > 0 )
	{
		Assert( m_nForceCursorVisible == 0 );

		// Make sure to give the normal processing a shot at putting things
		// back correctly.
		m_bSetMouseCursorCalled = true;
		m_bSetMouseVisibleCalled = true;
	}

	m_nForceCursorVisiblePrev = m_nForceCursorVisible;
#endif

	if ( m_bCursorVisible && m_bSetMouseCursorCalled )
	{
		SDL_SetCursor( m_hCursor );

		m_bSetMouseCursorCalled = false;
	}

	if ( m_bSetMouseVisibleCalled )
	{
		ConVarRef rawinput( "m_rawinput" );

		m_bRawInput = !m_bCursorVisible && rawinput.IsValid() && rawinput.GetBool();

		SDL_bool bWindowGrab = !m_bCursorVisible ? SDL_TRUE : SDL_FALSE;
		SDL_bool bRelativeMouseMode = bWindowGrab;

		if ( !m_bRawInput )
		{
			if ( m_bForbidMouseGrab )
				bWindowGrab = SDL_FALSE;

			bRelativeMouseMode = SDL_FALSE;
		}

		SDL_SetWindowGrab( m_Window, bWindowGrab );
		SDL_SetRelativeMouseMode( bRelativeMouseMode );

		SDL_ShowCursor( m_bCursorVisible && m_hCursor );

		m_bSetMouseVisibleCalled = false;

		if ( m_bCursorVisible )
		{
			//if we were invisible for any number of frames and are set back to visible, then center the cursor.
			if ( m_nFramesCursorInvisibleFor > 60 )
			{
				int windowHeight = 0, windowWidth = 0;
				SDL_GetWindowSize((SDL_Window*)GetWindowRef(), &windowWidth, &windowHeight);
				SDL_WarpMouseInWindow( m_Window, windowWidth/2, windowHeight/2 );
			}

			m_nFramesCursorInvisibleFor = 0;
		}
	}
}

#if defined( DX_TO_GL_ABSTRACTION )
void CSDLMgr::ShowPixels( CShowPixelsParams *params )
{
	SDLAPP_FUNC;

	if (params->m_onlySyncView)
		return;

	int swapInterval	= 0;
	int swapLimit		= 0;

	if (gl_swapdebug.GetInt())
	{
		// just jam through these debug convars every frame
		// but they will be shock absorbed below
			
		swapInterval	= gl_swapinterval.GetInt();
		swapLimit		= gl_swaplimit.GetInt();
	}
	else
	{
		// jam through (sync&limit) = 1 or 0..
		swapInterval	= params->m_vsyncEnable ? 1 : 0;
		swapLimit		= 1; // params->m_vsyncEnable ? 1 : 0;	// no good reason to turn off swap limit in normal user mode

#ifdef OSX
		// only do the funky forced vsync for NV on 10.6.4 and only if the bypass is not turned on
		if (m_force_vsync && (gl_disable_forced_vsync.GetInt()==0))
		{
			swapInterval	= 1;
			swapLimit		= 1;
		}
#else
		if (gl_swaptear.GetInt() && gGL->HasSwapTearExtension())
		{
			// For 0, do nothing. For 1, make it -1.
			swapInterval = -swapInterval;
		}
#endif
	}
		
	// only touch them on changes, or right after a change in windowed/FS state
	if ( (swapInterval!=m_lastKnownSwapInterval) || (swapLimit!=m_lastKnownSwapLimit) )
	{
		
		if (swapInterval!=m_lastKnownSwapInterval)
		{
			SDL_GL_SetSwapInterval(swapInterval);
		}

		m_lastKnownSwapInterval = swapInterval;
		m_lastKnownSwapLimit = swapLimit;

		printf("\n ##### swap interval = %d     swap limit = %d #####\n", m_lastKnownSwapInterval, m_lastKnownSwapLimit );
		fflush(stdout);

	}

#ifdef OSX
	if (!params->m_noBlit)
	{
		if ( params->m_useBlit ) // FBO blit path - which is what we *should* be using.  But if the params say no, then don't do it because the ext is not there.
		{
			// bind a quickie FBO to enclose the source texture
			GLint	myreadfb = 1000;
			
			glBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, myreadfb);
			CheckGLError( __LINE__ );
			
			glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0);		// to the default FB/backbuffer
			CheckGLError( __LINE__ );
			
			// attach source tex to source FB
			glFramebufferTexture2DEXT( GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, params->m_srcTexName, 0);
			CheckGLError( __LINE__ );
			
			// blit
			
			int srcxmin = 0;
			int srcymin = 0;
			int srcxmax = params->m_width;
			int srcymax = params->m_height;
			
			// normal blit
			int dstxmin = 0;
			int dstymin = 0;
			int dstxmax = 0;
			int dstymax = 0;

			SDL_GetWindowSize(m_Window, &dstxmax, &dstymax);
			if (gl_blit_halfx.GetInt())
			{
				// blit right half
				srcxmin += srcxmax/2;
				dstxmin += dstxmax/2;
			}
			
			if (gl_blit_halfy.GetInt())
			{
				// blit top half
				// er, but top on screen is bottom of GL y coord range
				srcymax /= 2;
				dstymin += dstymax/2;
			}
			
			// go NEAREST if sizes match
			GLenum filter = ( ((srcxmax-srcxmin)==(dstxmax-dstxmin)) && ((srcymax-srcymin)==(dstymax-dstymin)) ) ? GL_NEAREST : GL_LINEAR;
			
			glBlitFramebufferEXT(
					     /* src min and maxes xy xy */ srcxmin, srcymin,				srcxmax,srcymax,
					     /* dst min and maxes xy xy */ dstxmin, dstymax,				dstxmax,dstymin,		// note yflip here
					     GL_COLOR_BUFFER_BIT, filter );
			CheckGLError( __LINE__ );
			
			// detach source tex
			glFramebufferTexture2DEXT( GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
			CheckGLError( __LINE__ );
			
			glBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, 0);
			CheckGLError( __LINE__ );
			
			glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0);		// to the default FB/backbuffer
			CheckGLError( __LINE__ );
			
		}
		else
		{
			// old blit - gets very dark output with sRGB sources... not good
			bool texing = true;
			
			glUseProgram(NULL);
			
			glDisable( GL_DEPTH_TEST );
			glDepthMask( GL_FALSE );
			
			glActiveTexture( GL_TEXTURE0 );
			
			if (texing)
			{
				Assert( glIsTexture (params->m_srcTexName) );
				
				glEnable(GL_TEXTURE_2D);
				glBindTexture( GL_TEXTURE_2D, params->m_srcTexName );
				CheckGLError( __LINE__ );
				
				GLint width;
				glGetTexLevelParameteriv(	GL_TEXTURE_2D,			//target
							 0,						//level,
							 GL_TEXTURE_WIDTH,		//pname
							 &width
							 );
				CheckGLError( __LINE__ );
			}
			else
			{
				glBindTexture( GL_TEXTURE_2D, 0 );
				CheckGLError( __LINE__ );
				
				glDisable( GL_TEXTURE_2D );
				glColor4f( 1.0, 0.0, 0.0, 1.0 );
			}
			
			
			// immediate mode is fine for a simple textured quad
			// later if we switch the Valve side to render into an RBO, then this would turn into an FBO blit
			// note, do not check glGetError in between glBegin/glEnd, lol
			
			// flipped
			float topv = 0.0;
			float botv = 1.0;
			
			glBegin(GL_QUADS);
			
			if (texing)
				glTexCoord2f( 0.0, botv );
			glVertex3f		( -1.0, -1.0, 0.0 );
			
			if (texing)
				glTexCoord2f( 1.0, botv );
			glVertex3f		( 1.0, -1.0, 0.0 );
			
			if (texing)
				glTexCoord2f( 1.0, topv );
			glVertex3f		( 1.0, 1.0, 0.0 );
			
			if (texing)
				glTexCoord2f( 0.0, topv );
			glVertex3f		( -1.0, 1.0, 0.0 );
			glEnd();
			CheckGLError( __LINE__ );
			
			if (texing)
			{
				glBindTexture( GL_TEXTURE_2D, 0 );
				CheckGLError( __LINE__ );
				
				glDisable(GL_TEXTURE_2D);
			}
			
		}
	}
#endif

	//glFlush();
	//glFinish();
	CheckGLError( __LINE__ );

	CFastTimer tm;
	tm.Start();

	SDL_GL_SwapWindow( m_Window );

	m_flPrevGLSwapWindowTime = tm.GetDurationInProgress().GetMillisecondsF();

	CheckGLError( __LINE__ );
}
#endif // DX_TO_GL_ABSTRACTION


void CSDLMgr::SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight, bool bDesktopFriendlyFullscreen )
{
	SDLAPP_FUNC;

	int displayIndex = GetActiveDisplayIndex();

	Uint32 iFullscreenMode = bDesktopFriendlyFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;

	if ( CommandLine()->FindParm( "-exclusivefs" ) )
	{
		iFullscreenMode = SDL_WINDOW_FULLSCREEN;
	}
	else if ( CommandLine()->FindParm( "-noexclusivefs" ) )
	{
		iFullscreenMode = SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	SDL_DisplayMode mode;
	if ( SDL_GetDesktopDisplayMode( displayIndex, &mode ) != 0 )
	{
		Assert( 0 );
		SDL_GetDesktopDisplayMode( 0, &mode );
	}

	if ( bFullScreen )
	{

		mode.format = (Uint32)SDL_PIXELFORMAT_RGBX8888;

#ifdef OSX
		if ( iFullscreenMode == SDL_WINDOW_FULLSCREEN )
		{
			mode.w = nWidth;
			mode.h = nHeight;
		}
#endif

		m_flMouseXScale = ( float )nWidth / ( float )mode.w;
		m_flMouseYScale = ( float )nHeight / ( float )mode.h;
	}
	else
	{
		mode.format = (Uint32)SDL_PIXELFORMAT_RGBX8888;
		mode.refresh_rate = 0;
		mode.w = nWidth;
		mode.h = nHeight;
		mode.driverdata = 0;
		m_flMouseXScale = 1.0f;
		m_flMouseYScale = 1.0f;
	}

	SDL_SetWindowDisplayMode( m_Window, &mode );

	if ( m_bFullScreen != bFullScreen )
	{
		if ( bFullScreen )
		{
			int x = 0;
			int y = 0;

			// If we have more than one display, center the window in the one we've been assigned to.
			if ( SDL_GetNumVideoDisplays() > 1 )
			{
				SDL_Rect rect = { 0, 0, 0, 0 };

				SDL_GetDisplayBounds( displayIndex, &rect );

				x = rect.x;
				y = rect.y;
			}

			if ( m_bFullScreen == bFullScreen )
			{
				// TODO: Temporary workaround. SDL doesn't support going fullscreen on one monitor to fullscreen
				// on another. So we switch to windowed here, move our window, then go back fullscreen.
				SDL_SetWindowFullscreen( m_Window, SDL_FALSE );
				ThreadSleep( 15 );
			}

			// Move the window to the upper left of whatever display we're on, then size to fullscreen.
			SDL_SetWindowPosition( m_Window, x, y );
			SizeWindow( nWidth, nHeight );
		}


		SDL_SetWindowFullscreen( m_Window, bFullScreen ? iFullscreenMode : SDL_FALSE );

		m_bFullScreen = bFullScreen;
	}
}


void CSDLMgr::MoveWindow( int x, int y )
{
	SDLAPP_FUNC;

	SDL_SetWindowPosition(m_Window, x, y);
}

void CSDLMgr::SizeWindow( int width, int tall )
{
	SDLAPP_FUNC;

	m_WindowWidth = width;
	m_WindowHeight = tall;

	SDL_SetWindowSize(m_Window, width, tall);

#if defined( DX_TO_GL_ABSTRACTION )
	gGL->glViewport(0, 0, (GLsizei) width, (GLsizei) tall);
	gGL->glScissor( 0,0, (GLsizei) width, (GLsizei) tall );
#endif
}


// key input handler
void CSDLMgr::handleKeyInput( const SDL_Event &event )
{
	SDLAPP_FUNC;

	Assert( ( event.type == SDL_KEYDOWN ) || ( event.type == SDL_KEYUP ) );

#ifdef OSX
	if ( event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB &&
	     SDL_GetModState()&KMOD_GUI && !CommandLine()->FindParm( "-noexclusivefs" ) )
	{
		// If we're in exclusive fullscreen mode, and they command-tab, handle
		// that by forcing minimization of the window.
		SDL_MinimizeWindow( m_Window );
	}
#endif

	const bool bPressed = ( event.type == SDL_KEYDOWN );

	// !!! FIXME: we should be getting text input from a different event...
	CCocoaEvent theEvent;
	theEvent.m_EventType = ( bPressed ) ? CocoaEvent_KeyDown : CocoaEvent_KeyUp;
	theEvent.m_VirtualKeyCode = event.key.keysym.scancode;
	theEvent.m_UnicodeKey = 0;
	theEvent.m_UnicodeKeyUnmodified = 0;

	// key modifiers aren't necessarily reliable in all the cases we'd want, so track it ourselves.
	const uint32_t ModCAPSLOCK = (1 << 0);
	const uint32_t ModSHIFTR   = (1 << 1);
	const uint32_t ModSHIFTL   = (1 << 2);
	const uint32_t ModCTRLR    = (1 << 3);
	const uint32_t ModCTRLL    = (1 << 4);
	const uint32_t ModALTR     = (1 << 5);
	const uint32_t ModALTL     = (1 << 6);
	const uint32_t ModGUIR     = (1 << 7);
	const uint32_t ModGUIL     = (1 << 8);

	#define KEYSYMCASE(mod,side,op,key) \
		case SDLK_##side##mod: \
			m_keyModifiers op Mod##mod##side; \
			theEvent.m_VirtualKeyCode = -key; \
			break

	//bool bDropKey = false;
	if (bPressed)
	{
		switch (event.key.keysym.sym)
		{
			KEYSYMCASE(CAPSLOCK,,|=,KEY_CAPSLOCK);
			KEYSYMCASE(SHIFT,R,|=,KEY_RSHIFT);
			KEYSYMCASE(SHIFT,L,|=,KEY_LSHIFT);
			KEYSYMCASE(CTRL,R,|=,KEY_RCONTROL);
			KEYSYMCASE(CTRL,L,|=,KEY_LCONTROL);
			KEYSYMCASE(GUI,R,|=,KEY_RWIN);
			KEYSYMCASE(GUI,L,|=,KEY_LWIN);
			KEYSYMCASE(ALT,R,|=,KEY_RALT);
			KEYSYMCASE(ALT,L,|=,KEY_LALT);
			default: break;  // don't care.
		}
	}
	else
	{
		switch (event.key.keysym.sym)
		{
			KEYSYMCASE(CAPSLOCK,,&= ~,KEY_CAPSLOCK);
			KEYSYMCASE(SHIFT,R,&= ~,KEY_RSHIFT);
			KEYSYMCASE(SHIFT,L,&= ~,KEY_LSHIFT);
			KEYSYMCASE(CTRL,R,&= ~,KEY_RCONTROL);
			KEYSYMCASE(CTRL,L,&= ~,KEY_LCONTROL);
			KEYSYMCASE(GUI,R,&= ~,KEY_RWIN);
			KEYSYMCASE(GUI,L,&= ~,KEY_LWIN);
			KEYSYMCASE(ALT,R,&= ~,KEY_RALT);
			KEYSYMCASE(ALT,L,&= ~,KEY_LALT);
			default: break;  // don't care.
		}
	}

	#undef KEYSYMCASE

	m_keyModifierMask = 0;
	if (m_keyModifiers & ModCAPSLOCK)
		m_keyModifierMask |= (1<<eCapsLockKey);
	if (m_keyModifiers & (ModSHIFTR | ModSHIFTL))
		m_keyModifierMask |= (1<<eShiftKey);
	if (m_keyModifiers & (ModCTRLR | ModCTRLL))
		m_keyModifierMask |= (1<<eControlKey);
	if (m_keyModifiers & (ModALTR | ModALTL))
		m_keyModifierMask |= (1<<eAltKey);
	if (m_keyModifiers & (ModGUIR | ModGUIL))
		m_keyModifierMask |= (1<<eCommandKey);

	theEvent.m_ModifierKeyMask = m_keyModifierMask;

	// make a decision about this event - does it go in the normal evt queue or into the debug queue.
	bool debug = IsDebugEvent( theEvent );

#if GLMDEBUG
	bool bIsShifted = ( ((theEvent.m_ModifierKeyMask & (1<<eCapsLockKey))!=0) || ((theEvent.m_ModifierKeyMask & (1<<eShiftKey))!=0) );
	theEvent.m_UnicodeKeyUnmodified = event.key.keysym.sym;
	if ( bIsShifted )
	{
		switch ( event.key.keysym.sym )
		{
			case '[':
				theEvent.m_UnicodeKeyUnmodified = '{';
				break;
			case ']':
				theEvent.m_UnicodeKeyUnmodified = '}';
				break;
			case 'h':
				theEvent.m_UnicodeKeyUnmodified = 'H';
				break;
			case ',':
				theEvent.m_UnicodeKeyUnmodified = '<';
				break;
			case '.':
				theEvent.m_UnicodeKeyUnmodified = '>';
				break;
		}		
	}
#endif

	PostEvent( theEvent, debug );
}

void CSDLMgr::PumpWindowsMessageLoop()
{
	SDLAPP_FUNC;

	SDL_Event event;
	int nEventsProcessed = 0;
	while ( SDL_PollEvent(&event) && nEventsProcessed < 100 )
	{
		nEventsProcessed++;

		switch ( event.type )
		{
			case SDL_MOUSEMOTION:
			{
                if( m_bHasFocus == false )
					{
					break;
					}

				// When SDL_WarpMouseInWindow is called, an SDL_MOUSEMOTION
				// event is sent. We want to ignore such 'synthetic'
				// mouse motion events.
				if ( m_bExpectSyntheticMouseMotion &&
					 event.motion.x == m_nMouseTargetX &&
					 event.motion.y == m_nMouseTargetY )
				{
					m_bExpectSyntheticMouseMotion = false;
					break;
				}

                m_nMouseXDelta += event.motion.xrel;
                m_nMouseYDelta += event.motion.yrel;

				CCocoaEvent theEvent;
				theEvent.m_EventType = CocoaEvent_MouseMove;
				theEvent.m_MousePos[0] = event.motion.x * (m_bCursorVisible ? m_flMouseXScale : 1.0);
				theEvent.m_MousePos[1] = event.motion.y * (m_bCursorVisible ? m_flMouseYScale : 1.0);
				theEvent.m_MouseButtonFlags = m_mouseButtons;
                PostEvent( theEvent );
				break;
			}
			
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
			{
				// SDL buttons:
				//  1 = Left button
				//  2 = Middle button
				//  3 = Right button
				//  4 = Wheel Forward		; These events are handled by SDL_MOUSEWHEEL and don't come through here.
				//  5 = Wheel Back			;
				//  6 = Wheel Tilt Left		;
				//  7 = Wheel Tilt Right	;
				//  8 = Browser Back
				//  9 = Browser Forward
				//  10 = Task button (probably similar to Alt-Tab)

				// every other platform does left(1), right(2), middle(3)...
				int button;

				switch( event.button.button )
				{
				case 1:	button = 1; break;
				case 2: button = 3; break;
				case 3: button = 2; break;
				default:
					// For all buttons above 4, map them to 4 & 5 forever. This is because different mice
					//	will use different mappings. Ie, mousewheel mice can do this:
					//    4 = Wheel Forward		; These events are handled by SDL_MOUSEWHEEL and don't come through here.
					//    5 = Wheel Back		;
					//    6 = Wheel Tilt Left	;
					//    7 = Wheel Tilt Right	;
					//    8 = Browser Back
					//    9 = Browser Forward
					//    10 = Task button (probably similar to Alt-Tab)
					// Mice without wheels can do 4/5 as regular 4/5, etc.
					button = 4 + ( event.button.button & 0x1 );
					break;
				}

				#if defined(WIN32)
					const bool bPressed = (event.type == SDL_MOUSEBUTTONDOWN);
				#else
					const bool bPressed = (event.button.state == SDL_PRESSED);
				#endif
				const CocoaMouseButton_t cocoaButton = ( CocoaMouseButton_t )( 1 << (button - 1 ) );

				if (bPressed)
					m_mouseButtons |= cocoaButton;
				else
					m_mouseButtons &= ~cocoaButton;

				bool bDoublePress = false;

				if (bPressed)  
				{
					if ( m_bGotMouseButtonDown &&
						 ( (int)( event.button.timestamp - m_MouseButtonDownTimeStamp ) <= sdl_double_click_time.GetInt() ) &&
						 ( abs( event.button.x - m_MouseButtonDownX ) <= sdl_double_click_size.GetInt() ) &&
						 ( abs( event.button.y - m_MouseButtonDownY ) <= sdl_double_click_size.GetInt() ) )
					{
						bDoublePress = true;
						m_bGotMouseButtonDown = false;
					}
					else
					{
						m_MouseButtonDownTimeStamp = event.button.timestamp;
						m_MouseButtonDownX = event.button.x;
						m_MouseButtonDownY = event.button.y;
						m_bGotMouseButtonDown = true;
					}
				}

				CCocoaEvent theEvent;
				theEvent.m_EventType = (bPressed) ? CocoaEvent_MouseButtonDown : CocoaEvent_MouseButtonUp;
				theEvent.m_MousePos[0] = event.button.x * (m_bCursorVisible ? m_flMouseXScale : 1.0);
				theEvent.m_MousePos[1] = event.button.y * (m_bCursorVisible ? m_flMouseYScale : 1.0);
				theEvent.m_MouseButtonFlags = m_mouseButtons;
				theEvent.m_nMouseClickCount = bDoublePress ? 2 : 1;
				theEvent.m_MouseButton = cocoaButton;
				PostEvent( theEvent );

				break;
			}

			case SDL_MOUSEWHEEL:
			{
				int scroll = event.wheel.y;

#ifdef OSX
				if ( scroll == 0 && ( SDL_GetModState()&KMOD_SHIFT ) )
				{
					scroll = -event.wheel.x;
				}
#endif

				if ( scroll )
				{
					CCocoaEvent theEvent;
					theEvent.m_EventType = CocoaEvent_MouseScroll;
					theEvent.m_MousePos[0] = scroll;
					theEvent.m_MousePos[1] = scroll;
					PostEvent( theEvent );
				}
				break;
			}

			case SDL_WINDOWEVENT:
				switch (event.window.event)
				{
					case SDL_WINDOWEVENT_EXPOSED:
					{
						/*if ( ev.xexpose.count > 0 )
							break; // multiple expose events queued
						EVENT_LOG( "Got event Expose\n" );
						int iPanel = m_mapWindowToVPanel.Find( ev.xexpose.window );
						if ( iPanel != m_mapWindowToVPanel.InvalidIndex() )
							drawVGUI( m_pXDisplay, ev.xexpose.window, m_mapWindowToVPanel[ iPanel ], m_GLContext );
						m_mapSentInvalidate.RemoveAll();*/
						break;
					}
					case SDL_WINDOWEVENT_FOCUS_GAINED:
					{
						m_bHasFocus = true;
						m_bSetMouseVisibleCalled = true;

						CCocoaEvent theEvent;
						theEvent.m_EventType = CocoaEvent_AppActivate;
						theEvent.m_ModifierKeyMask = 1;
						PostEvent( theEvent );
						break;
					}
					case SDL_WINDOWEVENT_FOCUS_LOST:
					{
						m_bHasFocus = false;

						SDL_SetWindowGrab( m_Window, SDL_FALSE );
						SDL_SetRelativeMouseMode( SDL_FALSE );
						SDL_ShowCursor( SDL_TRUE );

						CCocoaEvent theEvent;
						theEvent.m_EventType = CocoaEvent_AppActivate;
						theEvent.m_ModifierKeyMask = 0;
						PostEvent( theEvent );

						// Reset our key modifiers. This also happens in CocoaEvent_AppActivate in inputsystem.cpp for
						//	the g_pInputSystem, and WM_ACTIVATEAPP on Windows in that file.
						m_keyModifiers = 0;
						// Reset SDL state as well. SDL_keyboard.modstate in SDL_keyboard.c gets waaay out of alignment.
						SDL_SetModState( KMOD_NONE );
						break;
					}
				}
				break;

			case SDL_KEYUP:
			case SDL_KEYDOWN:
				if(event.type == SDL_KEYDOWN && event.key.repeat &&
				   (event.key.keysym.sym == SDLK_BACKSPACE ||
				    event.key.keysym.sym == SDLK_DELETE))
				{
					// If we have repeated keydown events, we want to
					// generate a synthetic keyup event, since Scaleform
					// doesn't behave well getting multiple keydown events
					// without corresponding keyups.
					event.type = SDL_KEYUP;
					handleKeyInput(event);
					event.type = SDL_KEYDOWN;
				}

				handleKeyInput(event);
				break;

			case SDL_TEXTINPUT:
			{
				char *text = event.text.text;

				if ( text && text[ 0 ] )
				{
					wchar_t WBuf[ SDL_TEXTINPUTEVENT_TEXT_SIZE + 1 ];
					WBuf[ 0 ] = 0;
					V_UTF8ToUnicode( text, WBuf, sizeof( WBuf ) );

					for ( int i = 0; i < SDL_TEXTINPUTEVENT_TEXT_SIZE; i++ )
					{
						wchar_t ch = WBuf[ i ];
						if ( ch == '\0' )
							break;

						CCocoaEvent theEvent;
						theEvent.m_EventType = CocoaEvent_KeyDown;
						theEvent.m_VirtualKeyCode = 0;
						theEvent.m_UnicodeKey = ch;
						theEvent.m_UnicodeKeyUnmodified = ch;
						theEvent.m_ModifierKeyMask = m_keyModifierMask;
						PostEvent( theEvent, false );

						theEvent.m_EventType = CocoaEvent_KeyUp;
						theEvent.m_VirtualKeyCode = 0;
						theEvent.m_UnicodeKey = 0;
						theEvent.m_UnicodeKeyUnmodified = 0;
						theEvent.m_ModifierKeyMask = m_keyModifierMask;
						PostEvent( theEvent, false );
					}
				}
				break;
			}

			case SDL_QUIT:
			{
				CCocoaEvent theEvent;
				theEvent.m_EventType = CocoaEvent_AppQuit;
				PostEvent( theEvent );
				break;
			}

			default:
				break;
		}
	}
}


void CSDLMgr::DestroyGameWindow()
{
	SDLAPP_FUNC;

	if ( m_Window )
	{
		if (m_GLContext)
		{
			SDL_GL_MakeCurrent(m_Window, NULL);
			SDL_GL_DeleteContext(m_GLContext);
			m_GLContext = NULL;
		}
		SDL_SetWindowFullscreen(m_Window, SDL_FALSE);  // just in case.
		SDL_SetWindowGrab(m_Window, SDL_FALSE);  // just in case.
		SDL_DestroyWindow(m_Window);
		m_Window = NULL;
	}
}


void CSDLMgr::SetApplicationIcon( const char *pchAppIconFile )
{
	SDLAPP_FUNC;

	SDL_Surface *icon = SDL_LoadBMP(pchAppIconFile);
	if (icon)
	{
		SDL_SetWindowIcon(m_Window, icon);
		SDL_FreeSurface(icon);
	}
}

void CSDLMgr::GetMouseDelta( int &x, int &y, bool bIgnoreNextMouseDelta )
{
	SDLAPP_FUNC;

    x = m_nMouseXDelta * (m_bCursorVisible ? m_flMouseXScale : 1.0);
    y = m_nMouseYDelta * (m_bCursorVisible ? m_flMouseYScale : 1.0);

	m_nMouseXDelta = m_nMouseYDelta = 0;
}

//  Returns the current active display index
//
int CSDLMgr::GetActiveDisplayIndex()
{
	// our preferred/remembered displayindex
	int displayindex = sdl_displayindex.GetInt();

	// current active display index (i.e. where the window actually is)
	int activeDisplayindex = SDL_GetWindowDisplayIndex( m_Window );

	if ( ( displayindex == -1 ) || ( displayindex != activeDisplayindex ) )
	{
		// we haven't configured sdl_displayindex, or we've moved the window to a different display since init
		displayindex = activeDisplayindex;

		if ( displayindex < 0 )
		{
			displayindex = 0;
		}

		// reflect any change to convar
		sdl_displayindex.SetValue( displayindex );
	}

	return displayindex;
}

//  Returns the resolution of the nth display. 0 is the default display.
//
void CSDLMgr::GetNativeDisplayInfo( int nDisplay, uint &nWidth, uint &nHeight, uint &nRefreshHz )
{
	SDL_DisplayMode mode;

	if ( nDisplay == -1 )
	{
		if ( g_bSDLDisplayindexSet )
		{
			nDisplay = sdl_displayindex.GetInt();
		}
		else
		{
			// sdl_displayindex hasn't been parsed yet. This can happen in CMaterialSystem::ModInit()
			//	before the config files have been read, etc. So in this case, just grab the largest
			//	display we can find and return with that.
			int Width, Height;
			nDisplay = GetLargestDisplaySize( Width, Height );
		}
	}

	if ( SDL_GetDesktopDisplayMode( nDisplay, &mode ) != 0 )
	{
		Assert( 0 );
		SDL_GetDesktopDisplayMode( 0, &mode );
	}

	nRefreshHz = mode.refresh_rate;
	nWidth = mode.w;
	nHeight = mode.h;
}


void CSDLMgr::RenderedSize( uint &width, uint &height, bool set )
{
	SDLAPP_FUNC;

	if (set)
	{
		m_renderedWidth = width;
		m_rendererHeight = height;	// latched from NotifyRenderedSize
	}
	else
	{
		width = m_renderedWidth;
		height = m_rendererHeight;
	}
}

void CSDLMgr::DisplayedSize( uint &width, uint &height ) 
{
	SDLAPP_FUNC;

	int w, h;
	SDL_GetWindowSize(m_Window, &w, &h);
	width = (uint) w;
	height = (uint) h;
}

void CSDLMgr::GetStackCrawl( CStackCrawlParams *params )
{
	SDLAPP_FUNC;
}

void CSDLMgr::WaitUntilUserInput( int msSleepTime )
{
	SDLAPP_FUNC;
	if ( m_bTextMode )
		return;

	SDL_WaitEventTimeout(NULL, msSleepTime);
}

static KeyValues *LoadCursorResource()
{
	static const char *pPath = "resource/cursor/cursor.res";
	KeyValues *pKeyValues = new KeyValues( pPath );
	const bool bLoadedCursorResource = pKeyValues->LoadFromFile( g_pFullFileSystem, pPath );
	Assert( bLoadedCursorResource );
	return pKeyValues;
}

InputCursorHandle_t CSDLMgr::LoadCursorFromFile( const char *pchFileName )
{
	// On SDL we don't support .ani files, like are used on Windows. Instead,
	// we expect there to be a .bmp file in the same location which will
	// contain the image for the cursor. (This means we don't support
	// animated or scaling cursors).
	char path[PATH_MAX];
	V_StripExtension( pchFileName, path, sizeof( path ) );
	V_strcat( path, ".bmp", sizeof( path ) );

	SDL_Surface *surface = SDL_LoadBMP( path );
	if ( surface == NULL )
	{
		Warning( "Failed to load image for cursor from %s: %s\n", path, SDL_GetError() );
		return NULL;
	}

	// The cursor resource file contains information on the cursor's
	// x,y hotspot. Load it and find the x,y hotspot.
	static KeyValues *pCursorResource = LoadCursorResource();

	char pchCursorName[PATH_MAX];
	V_FileBase( path, pchCursorName, sizeof( pchCursorName ) );

	int nHotX = 0, nHotY = 0;
	
	KeyValues *pRes = pCursorResource->FindKey( pchCursorName );
	if ( pRes != NULL )
	{
		nHotX = pRes->GetInt( "hotx" );
		nHotY = pRes->GetInt( "hoty" );
	}

	SDL_Cursor *cursor = SDL_CreateColorCursor( surface, nHotX, nHotY );
	if( cursor == NULL )
	{
		Warning( "Failed to load cursor from %s: %s\n", path, SDL_GetError() );
		return NULL;
	}
	return reinterpret_cast< InputCursorHandle_t >( cursor );
}

void CSDLMgr::FreeCursor( const InputCursorHandle_t pchCursor )
{
	SDL_FreeCursor( reinterpret_cast< SDL_Cursor* >( pchCursor ) );
}

void CSDLMgr::SetCursorIcon( const InputCursorHandle_t pchCursor )
{
	SDL_Cursor *cursor = reinterpret_cast< SDL_Cursor* >( pchCursor );
	SDL_SetCursor( cursor );
}


//===============================================================================

void CSDLMgr::SetGammaRamp( const uint16 *pRed, const uint16 *pGreen, const uint16 *pBlue )
{
	if ( m_Window )
	{
		int nResult = SDL_SetWindowGammaRamp( m_Window, pRed, pGreen, pBlue );
		
		if ( nResult != 0 )
		{
			ConMsg( "SDL_SetWindowGammaRamp failed: %d\n", nResult );
		}
	}
}

#if WITH_OVERLAY_CURSOR_VISIBILITY_WORKAROUND
//===============================================================================
void CSDLMgr::ForceSystemCursorVisible()
{
	Assert( m_nForceCursorVisible >= 0 );
	m_nForceCursorVisible += 1;
}

//===============================================================================
void CSDLMgr::UnforceSystemCursorVisible()
{
	Assert( m_nForceCursorVisible >= 1 );
	m_nForceCursorVisible -= 1;
}

#endif


//===============================================================================

#if defined( DX_TO_GL_ABSTRACTION )
void CSDLMgr::GetDesiredPixelFormatAttribsAndRendererInfo( uint **ptrOut, uint *countOut, GLMRendererInfoFields *rendInfoOut )
{
	SDLAPP_FUNC;

	Assert( m_pixelFormatAttribCount > 0 );

	if (ptrOut) *ptrOut = (uint *) m_pixelFormatAttribs;
	if (countOut) *countOut = m_pixelFormatAttribCount;
	if (rendInfoOut)
	{
		GLMDisplayDB *db = GetDisplayDB();
#ifdef OSX
		*rendInfoOut = db->m_renderers->Head()->m_info;
#else
		*rendInfoOut = db->m_renderer.m_info;
#endif
	}
}



GLMDisplayMode::GLMDisplayMode( uint width, uint height, uint refreshHz )
{
	SDLAPP_FUNC;

	Init( width, height, refreshHz );
}

GLMDisplayMode::~GLMDisplayMode()
{
	SDLAPP_FUNC;
	// empty
}

void GLMDisplayMode::Init( uint width, uint height, uint refreshHz )
{
	SDLAPP_FUNC;

	m_info.m_modePixelWidth = width;
	m_info.m_modePixelHeight = height;
	m_info.m_modeRefreshHz = refreshHz;
}

void GLMDisplayMode::Dump( int which )
{
	SDLAPP_FUNC;

	GLMPRINTF(("\n             # %-2d  width=%-4d  height=%-4d  refreshHz=%-2d",
			   which, m_info.m_modePixelWidth, m_info.m_modePixelHeight, m_info.m_modeRefreshHz ));
}

GLMDisplayDB *CSDLMgr::GetDisplayDB( void )
{
	SDLAPP_FUNC;

	if ( !m_displayDB )
	{
		m_displayDB = new GLMDisplayDB;		// creating the DB object does not do much other than init it to a good state.
		m_displayDB->Populate();			// populate the tree
#if defined( OSX )
		// side effect: we fill in m_force_vsync..
		{
			GLMRendererInfoFields	info;
			m_displayDB->GetRendererInfo( 0, &info );

			m_force_vsync = info.m_badDriver1064NV;		// just force it if it's the bum NV driver
		}
#endif
	}
	return m_displayDB;
}

#ifndef OSX
#include "glmdisplaydb_linuxwin.inl"
#endif


#endif // DX_TO_GL_ABSTRACTION

#endif  // !DEDICATED

// Turn off memdbg macros (turned on up top) since this is included like a header
#include "tier0/memdbgoff.h"

