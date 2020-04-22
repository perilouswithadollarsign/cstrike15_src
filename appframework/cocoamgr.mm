//========= Copyright  1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines a group of app systems that all have the same lifetime
// that need to be connected/initialized, etc. in a well-defined order
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//
#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <IOKit/IOKitLib.h>

#undef MIN
#undef MAX
#define DONT_DEFINE_BOOL	// Don't define BOOL!
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"
#include "tier0/fasttimer.h"
#include "tier0/dynfunction.h"
#include "tier1/interface.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "togl/rendermechanism.h"
#include "appframework/ilaunchermgr.h"	// gets pulled in from glmgr.h
#include "appframework/iappsystemgroup.h"
#include "inputsystem/ButtonCode.h"
#ifdef GLMPRINTF
	#undef GLMPRINTF
#endif
#if GLMDEBUG
	#define GLMPRINTF(args)	printf args
#else
	#define GLMPRINTF(args)
#endif
@class CocoaBridge;
@class CocoaViewBridge;
class CCocoaThreadMsg;

COpenGLEntryPoints *gGL = NULL;

// ------------------------------------------------------------------------------------ //
// some Carbon stuff we're using
// ------------------------------------------------------------------------------------ //
extern "C" uint GetCurrentKeyModifiers( void );
enum ECarbonModKeyIndex
{
  EcmdKeyBit                     = 8,    /* command key down?*/
  EshiftKeyBit                   = 9,    /* shift key down?*/
  EalphaLockBit                  = 10,   /* alpha lock down?*/
  EoptionKeyBit                  = 11,   /* option key down?*/
  EcontrolKeyBit                 = 12    /* control key down?*/
};
enum ECarbonModKeyMask
{
  EcmdKey                        = 1 << EcmdKeyBit,
  EshiftKey                      = 1 << EshiftKeyBit,
  EalphaLock                     = 1 << EalphaLockBit,
  EoptionKey                     = 1 << EoptionKeyBit,
  EcontrolKey                    = 1 << EcontrolKeyBit
};

enum {
   kUIModeNormal = 0,
   kUIModeContentSuppressed = 1,
   kUIModeContentHidden = 2,
   kUIModeAllSuppressed = 4,
   kUIModeAllHidden = 3,
};
typedef UInt32 SystemUIMode;
enum {
   kUIOptionAutoShowMenuBar = 1 << 0,
   kUIOptionDisableAppleMenu = 1 << 2,
   kUIOptionDisableProcessSwitch = 1 << 3,
   kUIOptionDisableForceQuit = 1 << 4,
   kUIOptionDisableSessionTerminate = 1 << 5,
   kUIOptionDisableHide = 1 << 6
};
typedef OptionBits SystemUIOptions;
extern "C" OSStatus SetSystemUIMode ( SystemUIMode inMode, SystemUIOptions inOptions );
void	SetFullscreenUIMode( bool on )
{
	if (on)
	{
		SetSystemUIMode( kUIModeAllHidden, (kUIOptionAutoShowMenuBar | /* kUIOptionDisableProcessSwitch |*/ kUIOptionDisableHide) );
	}
	else
	{
		SetSystemUIMode( kUIModeNormal, (SystemUIOptions)0);
	}
}

extern "C" uint GetCurrentEventButtonState( void );
int GetCurrentMouseButtonState( void )
{
	// see http://lists.apple.com/archives/cocoa-dev/2010/Feb/msg01193.html
	// and http://serenity.uncc.edu/web/ADC/2005/Developer_DVD_Series/April/ADC%20Reference%20Library/documentation/Carbon/Conceptual/Carbon_Event_Manager/Tasks/chapter_18_section_10.html
	
	if ([NSEvent respondsToSelector:@selector(pressedMouseButtons)])
		return (int)[NSEvent pressedMouseButtons];
	else
		return (int)GetCurrentEventButtonState();
}
// ------------------------------------------------------------------------------------ //
// Couple of functions for making and freeing AR pools
// ------------------------------------------------------------------------------------ //
void *macMakeAutoreleasePool(void)
{
	return [[NSAutoreleasePool alloc] init];
}
void macReleaseAutoreleasePool(void *_pool)
{
	NSAutoreleasePool *pool = (NSAutoreleasePool *) _pool;
	[pool release];
}
// NSWindow subclass
bool s_bBlockWarpCursor = false;
@interface AppWindow: NSWindow
{
	uint dummy;
}
-(BOOL)canBecomeKeyWindow;
-(BOOL)canBecomeMainWindow;
-(void)becomeKeyWindow;
-(void)sendEvent:(NSEvent *)event;
-(void)resignMainWindow;
@end
@implementation AppWindow
-(BOOL)canBecomeKeyWindow
{
	return YES;
}
-(BOOL)canBecomeMainWindow
{
	return YES;
}
-(void)resignMainWindow
{
	[super resignMainWindow];
	s_bBlockWarpCursor = true;
}
-(void)becomeKeyWindow
{
	NSEvent *event = [self currentEvent];
	if ( [event type] == NSLeftMouseDown && [event window] == self )
	{
		NSPoint pt = [self mouseLocationOutsideOfEventStream];
		NSRect windowFrame = [self frame];
		if ( pt.y > ( windowFrame.size.height - 21 ) && pt.y < windowFrame.size.height )
			s_bBlockWarpCursor = true;
		else
			s_bBlockWarpCursor = false;
	}
	else
		s_bBlockWarpCursor = false;
	[super becomeKeyWindow];
}
-(void)sendEvent:(NSEvent *)event
{
	[super sendEvent:event];
	if ( s_bBlockWarpCursor )
	{
		if ( [event type] == NSLeftMouseUp )
			s_bBlockWarpCursor = false;
		else if ( [event type] == NSLeftMouseDown && [event window] == self )
		{
			NSPoint pt = [self mouseLocationOutsideOfEventStream];
			NSRect windowFrame = [self frame];
			if ( pt.y > ( windowFrame.size.height - 21 ) && pt.y < windowFrame.size.height )
				s_bBlockWarpCursor = true;
			else
				s_bBlockWarpCursor = false;
		}
	}
}

@end
//===============================================================================
void __checkgl__( void )
{
#if GLMDEBUG
	GLenum errorcode = (GLenum)glGetError();
	if (errorcode != GL_NO_ERROR)
	{
		Debugger();
		printf("\nGL Error %d",errorcode);
	}
#endif
}
// some helper functions, relocated out of GLM since they are used here
// this one makes a new context
bool	GLMDetectSLGU( void )
{
	CGLError	cgl_error = (CGLError)0;
	bool		result = false;
	
	CGLContextObj oldctx = CGLGetCurrentContext();
	static CGLPixelFormatAttribute attribs[] = 
	{
		kCGLPFADoubleBuffer,
		kCGLPFANoRecovery,
		kCGLPFAAccelerated,
		kCGLPFADepthSize,
			(CGLPixelFormatAttribute)0,
		kCGLPFAColorSize,
			(CGLPixelFormatAttribute)32,
		(CGLPixelFormatAttribute)0	// list term
	};
	CGLPixelFormatObj	pixfmtobj = NULL;
	GLint				npix;
	
	CGLContextObj		ctxobj = NULL;
	
	cgl_error = CGLChoosePixelFormat( attribs, &pixfmtobj, &npix );
	if (!cgl_error)
	{
		// got pixel format, make a context
		
		cgl_error = CGLCreateContext( pixfmtobj, NULL, &ctxobj );
		if (!cgl_error)
		{
			CGLSetCurrentContext( ctxobj );
			// now do the test
			_CGLContextParameter	kCGLCPGCDMPEngine = ((_CGLContextParameter)1314);
			GLint dummyval = 0;
			cgl_error = CGLGetParameter( CGLGetCurrentContext(), kCGLCPGCDMPEngine, &dummyval );
			result = (!cgl_error);
			
			// all done, go back to old context, and destroy the temp one
			CGLSetCurrentContext( oldctx );
			CGLDestroyContext( ctxobj );
		}
		
		// destroy the pixel format obj
		CGLDestroyPixelFormat( pixfmtobj );
	}
	return result;
}

bool	GLMDetectScaledResolveMode( uint osComboVersion, bool hasSLGU )
{
	bool result = false;
	
	// note this function assumes a current context on the renderer in question
	// and that FB blit and SLGU are present..
	
	if (!hasSLGU)
		return false;
		
	if (osComboVersion <= 0x000A0604)	// we know no one has it before 10.6.5
		return false;
	// in 10.6.6 and later, just check for the ext string.
	char *gl_ext_string = (char*)glGetString(GL_EXTENSIONS);
	// if we failed to init and bind a GL context at all, glGetString can return null
	if ( !gl_ext_string )
		return false;
		
	result = strstr(gl_ext_string, "GL_EXT_framebuffer_multisample_blit_scaled") != NULL;
	
	// if we didn't find an explicity extension string try to sniff for it
	if ( !result )
	{
		// make two FBO's
		GLuint	fbos[2];
		GLuint	rbos[2];
		int extent = 64;
		
		// make two render buffers
		for( int fbi = 0; fbi < 2; fbi++ )
		{
			glGenFramebuffersEXT( 1, &fbos[fbi] );  __checkgl__();
			glBindFramebufferEXT( fbi ? GL_DRAW_FRAMEBUFFER_EXT : GL_READ_FRAMEBUFFER_EXT , fbos[fbi] );  __checkgl__();
			glGenRenderbuffersEXT( 1, &rbos[fbi] );  __checkgl__();
			glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, rbos[fbi] );  __checkgl__();
			// make it multisampled if 0
			if (!fbi)
			{
				glRenderbufferStorageMultisampleEXT( GL_RENDERBUFFER_EXT, 2, GL_RGBA8, extent,extent );	  __checkgl__();
			}
			else
			{
				glRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, GL_RGBA8, extent,extent );	  __checkgl__();			
			}
			// attach it 
			// #0 gets to be read and multisampled
			// #1 gets to be draw and multisampled
			glFramebufferRenderbufferEXT( fbi ? GL_DRAW_FRAMEBUFFER_EXT : GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, rbos[fbi] );  __checkgl__();			
		}
		// now test
		while( glGetError() )	// clear error queue
		{
			;
		}
		// now do the dummy blit
		glBlitFramebufferEXT(	0,0,extent,extent, 0,0,extent,extent, GL_COLOR_BUFFER_BIT, XGL_SCALED_RESOLVE_FASTEST_EXT );
		// type of error we get back lets us know what the outcome is.
		// invalid enum error								-> unsupported
		// no error or invalid op							-> supported
		GLenum errorcode = (GLenum)glGetError();
		switch(errorcode)
		{
			// expected outcomes.
			
			// positive
			case GL_NO_ERROR:
			case GL_INVALID_OPERATION:
				result = true;			// new scaled resolve detected
			break;
			
			default:
				result = false;			// no scaled resolve
			break;
		}
		
		// unbind and wipe stuff
		
		glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );  __checkgl__();
		
		for( int xfbi = 0; xfbi < 2; xfbi++ )
		{
			// unbind FBO
			glBindFramebufferEXT( xfbi ? GL_DRAW_FRAMEBUFFER_EXT : GL_READ_FRAMEBUFFER_EXT , 0 );  __checkgl__();
			// del FBO and RBO
			glDeleteFramebuffersEXT( 1, &fbos[xfbi] );  __checkgl__();
			glDeleteRenderbuffersEXT( 1, &rbos[xfbi] );  __checkgl__();
		}
	}
	return result; // no SLGU, no scaled resolve blit even possible
}



void *VoidFnPtrLookup_GlMgr(const char *libname, const char *fn, bool &okay, const bool bRequired, void *fallback)
{
	void *retval = NULL;
	if ((!okay) && (!bRequired))  // always look up if required (so we get a complete list of crucial missing symbols).
		return NULL;

	// The SDL path would work on all these platforms, if we were using SDL there, too...
#if defined( LINUX ) || defined( WIN32 )
	// SDL does the right thing, so we never need to use tier0 in this case.
	retval = SDL_GL_GetProcAddress(fn);
	//printf("CDynamicFunctionOpenGL: SDL_GL_GetProcAddress(\"%s\") returned %p\n", fn, retval);
	if ((retval == NULL) && (fallback != NULL))
	{
		//printf("CDynamicFunctionOpenGL: Using fallback %p for \"%s\"\n", fallback, fn);
		retval = fallback;
	}
#elif defined( OSX )
	// there's no glXGetProcAddress() equivalent for Mac OS X...it's just dlopen(), basically. Let tier0 handle that.
	retval = VoidFnPtrLookup_Tier0( libname, fn, (void *) fallback);
#else
	#error Unimplemented
#endif

	// Note that a non-NULL response doesn't mean it's safe to call the function!
	//  You always have to check that the extension is supported;
	//  an implementation MAY return NULL in this case, but it doesn't have to (and doesn't, with the DRI drivers).
	okay = (okay && (retval != NULL));
	if (bRequired && !okay)
	{
		fprintf( stderr, "Could not find required OpenGL entry point '%s'!\n", fn );
		// We can't continue execution, because one or more GL function pointers will be NULL.
		Error( "Could not find required OpenGL entry point '%s'!\n", fn);
	}

	return retval;
}

// ------------------------------------------------------------------------------------ //
// CCocoaMgr class.
// ------------------------------------------------------------------------------------ //
class CCocoaMgr : public ILauncherMgr
{
// Called by the ValveCocoaMain startup code.
public:
	CCocoaMgr();
	void FinishLaunchingApplication();
	// Create the NSApplication object. return false if it was already done.
	bool CreateApplicationObject();
	void WaitForApplicationToFinishLaunching();
	
	// Called from the Valve main thread. Stops the NSApplication's run loop.
	void StopRunLoop();
	// Called from the Cocoa thread.
	void ProcessMessageInCocoaThread( CCocoaThreadMsg *pMessage );
	// If we're in the Cocoa thread, this processes the message directly.
	// Otherwise, it sends it in to be processed.
	void SendMessageToCocoaThread( CCocoaThreadMsg *pMessage, bool bWaitUntilDone );
	// Post an event to the input event queue.
	// if debugEvent is true, post it to the debug event queue.
	void PostEvent( const CCocoaEvent &theEvent, bool debugEvent=false );
	// ask if an event is debug flavor or not.
	bool IsDebugEvent( CCocoaEvent& event );
	void AccumlateMouseDelta( int32 &x, int32 &y );
	
	void ShowMainWindow();
// ICocoaMgr overrides.
public:
	virtual bool CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height );
	virtual void DestroyGameWindow();
	virtual PseudoNSGLContextPtr GetNSGLContextForWindow( void* nswindow );
	virtual int GetEvents( CCocoaEvent *pEvents, int nMaxEventsToReturn, bool debugEvent = false );
	virtual void SetCursorPosition( int x, int y );
	virtual void* GetWindowRef();
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect() ;
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init() ;
	virtual void Shutdown();
	
	virtual void ShowPixels( CShowPixelsParams *params );
	virtual void MoveWindow( int x, int y );
	virtual void SizeWindow( int width, int tall );
	virtual void PumpWindowsMessageLoop();
	virtual void WaitUntilUserInput( int msSleepTime );
	virtual void GetStackCrawl( CStackCrawlParams *params );	
	virtual void SetApplicationIcon( const char *pchAppIconFile );
	virtual void GetMouseDelta( int &x, int &y, bool bIgnoreNextDelta );
	virtual void RenderedSize( uint &width, uint &height, bool set );	// either set or retrieve rendered size value
	virtual void DisplayedSize( uint &width, uint &height );			// query backbuffer size (window size whether FS or windowed)
		
	virtual void GetDesiredPixelFormatAttribsAndRendererInfo( uint **ptrOut, uint *countOut, GLMRendererInfoFields *rendInfoOut );
	virtual GLMDisplayDB *GetDisplayDB( void );
	// does this need to be virtual?  don't think so..
	NSPoint ScreenCoordsToSourceWindowCoords( NSView *pView, NSPoint screenCoord );

// new to the cocoa launch mgr
        virtual void SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight );
	virtual bool IsWindowFullScreen();
	virtual void SetForbidMouseGrab( bool bForbidMouseGrab );

	virtual double GetPrevGLSwapWindowTime() { return m_flPrevGLSwapWindowTime; }

        virtual PseudoGLContextPtr GetGLContextForWindow( void* windowref );
	virtual PseudoGLContextPtr	GetMainContext();
	virtual PseudoGLContextPtr CreateExtraContext();
	virtual void DeleteContext( PseudoGLContextPtr hContext );
	virtual bool MakeContextCurrent( PseudoGLContextPtr hContext );

	virtual void SetMouseVisible( bool bState );
	virtual void GetDisplaySize( uint *puiWidth, uint *puiHeight );  // Retrieve the size of the monitor (desktop)
	virtual void GetNativeDisplayInfo( int nDisplay, uint &nWidth, uint &nHeight, uint &nRefreshHz );


// Messages piped through from Objective-C objects.
public:
	void applicationDidFinishLaunching();
	void applicationWillTerminate();
	bool BFullScreen() { return m_fsEnable; }
	
private:
	void PostDummyEvent();
	bool InternalCreateWindow( const char *pTitle, bool bWindowed, NSRect *frame );
	void SetWindowedFrame( NSRect *frame, bool forEffect );			// *frame goes into m_winFrame unless null.  If forEffect = true, window adopts m_winFrame and windowed style mask.
	void SetFullscreenFrame( NSRect *frame, bool forEffect );		// *frame goes into m_fsFrame unless null.  If forEffect = true, window adopts m_fsFrame and adopts fullscreen style mask.
	
	void ClearGLView( void );							// blacken the m_view (NSGLView) and flush it out
	
private:
	CThreadMutex m_CocoaEventsMutex;					// use for either queue below
	CUtlLinkedList<CCocoaEvent,int> m_CocoaEvents;
	CUtlLinkedList<CCocoaEvent,int> m_DebugEvents;		// intercepted keys which wil be passed over to GLM
	void					*m_application;
	AppWindow				*m_window;
	CocoaViewBridge			*m_view;
	
	uint					m_renderedWidth,m_rendererHeight;	// latched from RenderedSize
	
	GLMDisplayDB			*m_displayDB;
	bool					m_leopard;					// true if <10.6.3 and we have to do extra work for fullscreen handling
	bool					m_force_vsync;				// true if 10.6.4 + bad NV driver
	
	uint					m_chosenRendererIndex;		// zero for now.. use this to drive the generation of the PFA next, and to answer the req for renderer info from GLM
	uint					m_pixelFormatAttribs[20];	// preferred attrib list for window view context and for drawing context in engine thread.
	uint					m_pixelFormatAttribCount;
	
	NSRect					m_winFrame;				// where window is when windowed mode (save it before switching to FS)
	NSRect					m_fsFrame;				// where window goes in full screen (set it before switching to FS)
	bool					m_fsEnable;				// are we in fullscreen now?
	
	ThreadId_t						m_nRunLoopThreadID;
	CThreadEvent			m_AppObjectInitialized;
	CocoaBridge				*m_pCocoaBridge;
	
	CShowPixelsParams		m_lastShownPixels;		// we may peek at this to infer what the video resolution of the engine is...
	NSOpenGLContext			*m_showPixelsCtx;
	int						m_lastKnownSwapInterval;	//-1 if unknown, 0/1 otherwise
	int						m_lastKnownSwapLimit;		//-1 if unknown, 0/1 otherwise
	
	int32			m_MouseDeltaX, m_MouseDeltaY;
	bool					m_bIgnoreNextMouseDelta;
	
	char					*m_windowTitle;
public:
	int						m_frontPushCounter;		// if non zero, bump window to front on ShowPixels

	double m_flPrevGLSwapWindowTime;
};

int		s_windowedStyleMask		= 	NSClosableWindowMask | /*NSResizableWindowMask |*/ NSTexturedBackgroundWindowMask | NSTitledWindowMask | NSMiniaturizableWindowMask;
int		s_fullscreenStyleMask	= 	NSBorderlessWindowMask;
CCocoaMgr g_CocoaMgr;
ILauncherMgr *g_pCocoaMgr = &g_CocoaMgr;

void* CreateCCocoaMgr()
{
		return g_pCocoaMgr;
}
//EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CCocoaMgr, ICocoaMgr, COCOAMGR_INTERFACE_VERSION, g_CocoaMgr );

// ------------------------------------------------------------------------------------ //
// 
// CCocoaThreadMsg is what the REAL code (C++ code) deals with when it passes
// messages between the Valve thread and the Cocoa thread.
//
// We use NSObject::performSelectorInMainThread to pass the messages back
// and forth, so we have to stuff the CCocoaThreadMsg into an Objective C++ object
// called CCocoaThreadMsgContainer
//
// ------------------------------------------------------------------------------------ //
class CCocoaThreadMsg
{
public:
	virtual ~CCocoaThreadMsg() {}
};
class CCocoaThreadMsg_CreateWindow : public CCocoaThreadMsg
{
public:
	char m_Title[256];
	bool m_bWindowed;
	int m_nWidth;
	int m_nHeight;
};
@interface CCocoaThreadMsgContainer : NSObject
{
@public
	CCocoaThreadMsg *m_pMessage;
}
@end
@implementation CCocoaThreadMsgContainer
@end

// ------------------------------------------------------------------------------------ //
// CocoaBridge Objective-C class. This is the bridge between NSApplication and
// the Valve code in CCocoaMgr. It just forwards Cocoa messages to CCocoaMgr.
// ------------------------------------------------------------------------------------ //
@interface CocoaBridge : NSObject<NSApplicationDelegate>
{
@public
	CCocoaMgr *m_pCocoaMgr;
}
/*
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
- (void)applicationWillTerminate:(NSNotification *)aNotification;
- (void)ProcessMessage:(id)pObj;
- (void)applicationDidBecomeActive:(NSNotification *)aNotification;
- (void)applicationDidResignActive:(NSNotification *)aNotification;
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication
					hasVisibleWindows:(BOOL)flag;
-(void) populateApplicationMenu:(NSMenu *)aMenu;
-(void) populateWindowMenu:(NSMenu *)aMenu;
-(void) applicationWillFinishLaunching:(NSNotification *)aNotification;
*/
@end
@implementation CocoaBridge
#pragma mark NSApplication delegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_AppQuit;
	m_pCocoaMgr->PostEvent( theEvent );
	return NSTerminateCancel;
}
- (void)applicationWillTerminate:(NSNotification *)aNotification
{
	m_pCocoaMgr->applicationWillTerminate();
}
- (void)ProcessMessage:(id)pObj
{
	CCocoaThreadMsgContainer *pDispatch = (CCocoaThreadMsgContainer*)pObj;
	m_pCocoaMgr->ProcessMessageInCocoaThread( pDispatch->m_pMessage );
	delete pDispatch->m_pMessage;
	[pDispatch release];
}
- (void)applicationDidBecomeActive:(NSNotification *)aNotification
{
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_AppActivate;
	theEvent.m_ModifierKeyMask = 1;
	m_pCocoaMgr->PostEvent( theEvent );
} 
- (void)applicationDidResignActive:(NSNotification *)aNotification
{
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_AppActivate;
	theEvent.m_ModifierKeyMask = 0;
	m_pCocoaMgr->PostEvent( theEvent );	
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication
					hasVisibleWindows:(BOOL)flag
{
	m_pCocoaMgr->ShowMainWindow();
	return YES;
}

-(void) populateApplicationMenu:(NSMenu *)aMenu
{
	NSString * applicationName =  NSLocalizedString(@"Source", nil);
	NSMenuItem * menuItem;
	
	menuItem = [aMenu addItemWithTitle:[NSString stringWithFormat:@"%@ %@", NSLocalizedString(@"About", nil), applicationName]
								action:@selector(openAboutDialog:)
						 keyEquivalent:@""];
	[menuItem setTarget:self];
	
	[aMenu addItem:[NSMenuItem separatorItem]];
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Preferences...", nil)
								action:@selector(openPreferencesDialog:)
						 keyEquivalent:@","];
	[menuItem setTarget:self];
	
	[aMenu addItem:[NSMenuItem separatorItem]];
	
	menuItem = [aMenu addItemWithTitle:[NSString stringWithFormat:@"%@ %@", NSLocalizedString(@"Hide", nil), applicationName]
								action:@selector(hide:)
						 keyEquivalent:@"h"];
	[menuItem setTarget:NSApp];
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Hide Others", nil)
								action:@selector(hideOtherApplications:)
						 keyEquivalent:@"h"];
	[menuItem setKeyEquivalentModifierMask:NSCommandKeyMask | NSAlternateKeyMask];
	[menuItem setTarget:NSApp];
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Show All", nil)
								action:@selector(unhideAllApplications:)
						 keyEquivalent:@""];
	[menuItem setTarget:NSApp];
	
	[aMenu addItem:[NSMenuItem separatorItem]];
	
	menuItem = [aMenu addItemWithTitle:[NSString stringWithFormat:@"%@ %@", NSLocalizedString(@"Quit", nil), applicationName]
								action:@selector(terminate:)
						 keyEquivalent:@"q"];
	[menuItem setTarget:NSApp];
}
-(void) populateWindowMenu:(NSMenu *)aMenu
{
	NSMenuItem * menuItem;
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Minimize", nil)
								action:@selector(performMinimize:)
						 keyEquivalent:@"m"];
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Zoom", nil)
								action:@selector(performZoom:)
						 keyEquivalent:@""];
	
	[aMenu addItem:[NSMenuItem separatorItem]];
	
	menuItem = [aMenu addItemWithTitle:NSLocalizedString(@"Bring All to Front", nil)
								action:@selector(arrangeInFront:)
						 keyEquivalent:@""];
}

//-----------------------------------------------------------------------------
// Purpose: called on startup, populates the OS level menus
//-----------------------------------------------------------------------------
-(void) applicationWillFinishLaunching:(NSNotification *)aNotification 
{
	// NSApplication might not have created an autorelease pool yet, so create our own.
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];	
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:@"NO" forKey:@"AppleMomentumScrollSupported"];
   [defaults registerDefaults:appDefaults];
	
	NSMenu * mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
	
	NSMenuItem * menuItem;
	NSMenu * submenu;
	
	// The titles of the menu items are for identification purposes only and shouldn't be localized.
	// The strings in the menu bar come from the submenu titles,
	// except for the application menu, whose title is ignored at runtime.
	menuItem = [mainMenu addItemWithTitle:@"Apple" action:NULL keyEquivalent:@""];
	submenu = [[NSMenu alloc] initWithTitle:@"Apple"];
	[NSApp performSelector:@selector(setAppleMenu:) withObject:submenu];
	[self populateApplicationMenu:submenu];
	[mainMenu setSubmenu:submenu forItem:menuItem];
	
	menuItem = [mainMenu addItemWithTitle:@"Window" action:NULL keyEquivalent:@""];
	submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Window", @"The Window menu")];
	[self populateWindowMenu:submenu];
	[mainMenu setSubmenu:submenu forItem:menuItem];
	[NSApp setWindowsMenu:submenu];
	/*
	 menuItem = [mainMenu addItemWithTitle:@"Help" action:NULL keyEquivalent:@""];
	 submenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Help", @"The Help menu")];
	 [self populateHelpMenu:submenu];
	 [mainMenu setSubmenu:submenu forItem:menuItem];
	 */
	[NSApp setMainMenu:mainMenu];
	
	/*	NSAppleEventManager *appleEventManager = [NSAppleEventManager
	 sharedAppleEventManager];
	 
	 // Get URL Apple Event ('GURL') is part of the internet AE suite not the standard AE suite and
	 // it isn't currently supported directly via a application delegate method so we have to register
	 // an AE event handler for it.
	 [appleEventManager setEventHandler:self
	 andSelector:@selector(handleGetURLEvent:withReplyEvent:)
	 forEventClass:'GURL'
	 andEventID:'GURL'];	
	 */
	[pool release];
}
@end

@interface NSValveApplication : NSApplication
{
}
- (void)sendEvent:(NSEvent *)anEvent;
@end
@implementation NSValveApplication

- (void)sendEvent:(NSEvent *)anEvent
{
	//This works around an AppKit bug, where key up events while holding
	//down the command key don't get sent to the key window.
	if( ([anEvent modifierFlags] & NSCommandKeyMask))
	{
		NSEventType type = [anEvent type];
		//printf( "Got event %d (%d)\n", [anEvent type], NSKeyUp );
		if ( type == NSKeyUp || type == NSKeyDown  
			|| type == NSLeftMouseDown  || type == NSLeftMouseUp
			|| type == NSRightMouseDown  || type == NSRightMouseUp
			|| type == NSOtherMouseDown  || type == NSOtherMouseUp
			|| type == NSLeftMouseDragged  || type == NSRightMouseDragged
			|| type == NSFlagsChanged || type == NSMouseMoved || type == NSScrollWheel )
		{
			if ( type == NSKeyUp || type == NSKeyDown )
			{
				// tell both the high level and us for CMD-Q,W,H,M
				int keyCode = [anEvent keyCode];
				if ( keyCode == 12 || keyCode == 13 || keyCode == 4 || keyCode == 46 )
					[super sendEvent:anEvent];
			}
			//printf( "%i\n", keyCode );
			[[self keyWindow] sendEvent:anEvent];
		}
		else
			[super sendEvent:anEvent];
	}
	else
	{
		[super sendEvent:anEvent];
	}
}
@end

@interface CocoaViewDelegate : NSObject<NSWindowDelegate>
{
}
@end
@implementation CocoaViewDelegate
- (void)windowDidDeminiaturize:(NSNotification *)notification
{

  CGDisplayHideCursor(kCGDirectMainDisplay);
}
@end
// ------------------------------------------------------------------------------------ //
// Our NSView bridge.
// ------------------------------------------------------------------------------------ //
@interface CocoaViewBridge : NSOpenGLView
{
@public
	CCocoaMgr *m_pCocoaMgr;
}
@end
@implementation CocoaViewBridge
- (void)drawRect:(NSRect)rect
{
	// letting this method do anything was having evil effects on clipping (black portal bug).
	// we need a better solution though, or we will get garbage in the window at launch
	
	return;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)mouseDown:(NSEvent*)pEvent
{
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseButtonDown;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	theEvent.m_nMouseClickCount = [pEvent clickCount];
	if ( [pEvent modifierFlags] & NSCommandKeyMask  ) // make ctrl-click be a right click
	{
		theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
		theEvent.m_MouseButtonFlags &= ~0x1; // turn off left button press
		theEvent.m_MouseButtonFlags |= 0x2; // and press right instead
		theEvent.m_MouseButton = COCOABUTTON_RIGHT; 
	}
	else
	{
		theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
		theEvent.m_MouseButton = COCOABUTTON_LEFT; 
	}
	
	m_pCocoaMgr->PostEvent( theEvent );
}

- (void)rightMouseDown:(NSEvent*)pEvent
{
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseButtonDown;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	theEvent.m_nMouseClickCount = [pEvent clickCount];
	theEvent.m_MouseButton = COCOABUTTON_RIGHT; 
	
	m_pCocoaMgr->PostEvent( theEvent );
}

- (void)otherMouseDown:(NSEvent*)pEvent
{
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseButtonDown;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	theEvent.m_nMouseClickCount = [pEvent clickCount];
	switch( [pEvent buttonNumber] )
	{
		case 2:
			theEvent.m_MouseButton = COCOABUTTON_MIDDLE;
			break;
		case 3:
			theEvent.m_MouseButton = COCOABUTTON_4; 
			break;
		case 4:
			theEvent.m_MouseButton = COCOABUTTON_5; 
			break;
	}
	m_pCocoaMgr->PostEvent( theEvent );
}

- (void)mouseMoved:(NSEvent*)pEvent
{
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseMove;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	NSPoint orig = [NSEvent mouseLocation];
	//printf( "MouseMove: %0.1f,%0.1f %0.1f,%0.1f\n", pt.x, pt.y, orig.x, orig.y );
	m_pCocoaMgr->PostEvent( theEvent );
		
	int32 deltaX, deltaY;
	CGGetLastMouseDelta( &deltaX, &deltaY );
	m_pCocoaMgr->AccumlateMouseDelta( deltaX, deltaY );
}

- (void)mouseDragged:(NSEvent*)pEvent
{	
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	int32 deltaX, deltaY;
	CGGetLastMouseDelta( &deltaX, &deltaY );
	m_pCocoaMgr->AccumlateMouseDelta( deltaX, deltaY );	
	if ( !CGCursorIsVisible() )
	{
		pt.x += deltaX;
		pt.y += deltaY;
	}
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseMove;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	
	m_pCocoaMgr->PostEvent( theEvent );
	
}
- (void)rightMouseDragged:(NSEvent*)pEvent
{
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	
	int32 deltaX, deltaY;
	CGGetLastMouseDelta( &deltaX, &deltaY );
	m_pCocoaMgr->AccumlateMouseDelta( deltaX, deltaY );
	if ( !CGCursorIsVisible() )
	{
		pt.x += deltaX;
		pt.y += deltaY;
	}
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseMove;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	
	m_pCocoaMgr->PostEvent( theEvent );
}
- (void)otherMouseDragged:(NSEvent*)pEvent
{
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	
	int32 deltaX, deltaY;
	CGGetLastMouseDelta( &deltaX, &deltaY );
	m_pCocoaMgr->AccumlateMouseDelta( deltaX, deltaY );
	if ( !CGCursorIsVisible() )
	{
		pt.x += deltaX;
		pt.y += deltaY;
	}
	
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseMove;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	
	m_pCocoaMgr->PostEvent( theEvent );
}

- (void)mouseUp:(NSEvent*)pEvent
{
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseButtonUp;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	m_pCocoaMgr->PostEvent( theEvent );
}
- (void)rightMouseUp:(NSEvent*)pEvent
{
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseButtonUp;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	
	m_pCocoaMgr->PostEvent( theEvent );
}

- (void)otherMouseUp:(NSEvent*)pEvent
{
	NSPoint pt = m_pCocoaMgr->ScreenCoordsToSourceWindowCoords( self, [NSEvent mouseLocation] );
	
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseButtonUp;
	theEvent.m_MousePos[0] = (int)pt.x;
	theEvent.m_MousePos[1] = (int)pt.y;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	
	m_pCocoaMgr->PostEvent( theEvent );
}

- (void)keyDown:(NSEvent *)pEvent
{
	// Store the event in our input event queue.
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_KeyDown;
	theEvent.m_VirtualKeyCode = pEvent.keyCode;
	theEvent.m_UnicodeKey = 0;
	if ( [pEvent.characters length] > 0 )
		theEvent.m_UnicodeKey = [pEvent.characters characterAtIndex:0];
	theEvent.m_UnicodeKeyUnmodified = 0;
	if ( [pEvent.charactersIgnoringModifiers length] > 0 )
		theEvent.m_UnicodeKeyUnmodified = [pEvent.charactersIgnoringModifiers characterAtIndex:0];
	if ( theEvent.m_UnicodeKey & 0x8000 ) // apple set the high bit for "special" characters like arrow keys, so ignore them
	{
		theEvent.m_UnicodeKey = theEvent.m_UnicodeKeyUnmodified = 0;
	}
	
	theEvent.m_ModifierKeyMask = 0;
	// pick apart modifier key mask
	uint modifiers = [pEvent modifierFlags];
	if (modifiers & NSAlphaShiftKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eCapsLockKey);
	}
	if (modifiers & NSShiftKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eShiftKey);
	}
	if (modifiers & NSAlternateKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eAltKey);
	}
	if (modifiers & NSCommandKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eCommandKey);
	}
	if (modifiers & NSControlKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eControlKey);
	}
	
	// make a decision about this event - does it go in the normal evt queue or into the debug queue.
	bool debug = m_pCocoaMgr->IsDebugEvent( theEvent );	
	//if ( [pEvent isARepeat] && (!debug) )		// dump non debug key repeats
	//	return;
	m_pCocoaMgr->PostEvent( theEvent, debug );
	// This was used to build a table of virtual key codes to ButtonCode_t's.
	//char str[512];
	//V_strncpy( str, [pEvent.characters UTF8String], sizeof( str ) );
	//V_strupr( str );
	//Msg( "{0x%x, KEY_%s},\n", pEvent.keyCode, str ); 
}

void CheckModifierDiff( UInt32 diff, UInt32 mask, UInt32 old, ButtonCode_t key, CCocoaMgr *m_pCocoaMgr )
{
	if( diff & mask )
	{
		// Store the event in our input event queue.
		CCocoaEvent theEvent;
		theEvent.m_VirtualKeyCode = -1 * key; // use a negative code value so the input system can translate it directly to a buttoncode
		theEvent.m_UnicodeKey = theEvent.m_UnicodeKeyUnmodified = 0;
		theEvent.m_ModifierKeyMask = 0;
		if ( old & mask ) // was pressed, now isnt
		{
			theEvent.m_EventType = CocoaEvent_KeyUp;			
		}
		else
		{
			theEvent.m_EventType = CocoaEvent_KeyDown;
		}	
		// make a decision about this event - does it go in the normal evt queue or into the debug queue.
		bool debug = m_pCocoaMgr->IsDebugEvent( theEvent );	
		
		m_pCocoaMgr->PostEvent( theEvent, debug );			
	}
}

- (void)flagsChanged:(NSEvent *)pEvent
{	
	// pick apart modifier key mask
	uint modifierCode = [pEvent modifierFlags];
	static uint s_lastModifierCode = 0;
	UInt32 diff = s_lastModifierCode ^ modifierCode;
	CheckModifierDiff( diff, NSAlphaShiftKeyMask, s_lastModifierCode, KEY_CAPSLOCK, m_pCocoaMgr );
	CheckModifierDiff( diff, NSShiftKeyMask, s_lastModifierCode, KEY_LSHIFT, m_pCocoaMgr );
	CheckModifierDiff( diff, NSAlternateKeyMask, s_lastModifierCode, KEY_LALT, m_pCocoaMgr );
	CheckModifierDiff( diff, NSCommandKeyMask, s_lastModifierCode, KEY_LWIN, m_pCocoaMgr );
	CheckModifierDiff( diff, NSControlKeyMask, s_lastModifierCode, KEY_LCONTROL, m_pCocoaMgr );	
	s_lastModifierCode = modifierCode;	
}
- (void)keyUp:(NSEvent *)pEvent
{
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_KeyUp;
	theEvent.m_VirtualKeyCode = pEvent.keyCode;
	theEvent.m_UnicodeKey = 0;
	if ( [pEvent.characters length] > 0 )
		theEvent.m_UnicodeKey = [pEvent.characters characterAtIndex:0];
	theEvent.m_UnicodeKeyUnmodified = 0;
	if ( [pEvent.charactersIgnoringModifiers length] > 0 )
		theEvent.m_UnicodeKeyUnmodified = [pEvent.charactersIgnoringModifiers characterAtIndex:0];
	theEvent.m_ModifierKeyMask = 0;
	// pick apart modifier key mask
	uint modifiers = [pEvent modifierFlags];
	if (modifiers & NSAlphaShiftKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eCapsLockKey);
	}
	if (modifiers & NSShiftKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eShiftKey);
	}
	if (modifiers & NSAlternateKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eAltKey);
	}
	if (modifiers & NSCommandKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eCommandKey);
	}
	if (modifiers & NSControlKeyMask)
	{
		theEvent.m_ModifierKeyMask |= (1<<eControlKey);
	}
	
	m_pCocoaMgr->PostEvent( theEvent );
}

- (BOOL)performKeyEquivalent:(NSEvent *)theEvent 
{
	if ( theEvent.keyCode == 48 )
	{
		if ([theEvent type] == NSKeyDown )
		{
			[self keyDown: theEvent ];
		}
		else
		{
			[self keyUp: theEvent ];
		}
	}
	return [super performKeyEquivalent:theEvent];
}
- (void)scrollWheel:(NSEvent *)pEvent
{
	CCocoaEvent theEvent;
	theEvent.m_EventType = CocoaEvent_MouseScroll;
	theEvent.m_MousePos[0] = [pEvent deltaX]*100;
	theEvent.m_MousePos[1] = [pEvent deltaY]*100;
	theEvent.m_MouseButtonFlags = GetCurrentMouseButtonState( );
	
	m_pCocoaMgr->PostEvent( theEvent );	
}
@end

// ------------------------------------------------------------------------------------ //
// CCocoaMgr implementation.
// ------------------------------------------------------------------------------------ //
CCocoaMgr::CCocoaMgr()
{
	m_application = NULL;
	m_window = NULL;
	m_view = NULL;
	// You cannot make the display DB here.. because it will want to query some CLI args...
	// and this constructor happens too early in the program lifetime.
	
	// so leave it NULL, and teach the GetDisplayDB function to do a one-time init.
	m_displayDB = NULL;
	
	m_renderedWidth = m_rendererHeight = 0;
	m_chosenRendererIndex = 0;	//FIXME
	memset( m_pixelFormatAttribs, 0, sizeof(m_pixelFormatAttribs) );
	m_pixelFormatAttribCount = 0;
	
	m_nRunLoopThreadID = (ThreadId_t)-1;
	m_pCocoaBridge = NULL;
	m_MouseDeltaX = 0;
	m_MouseDeltaY = 0;
	m_bIgnoreNextMouseDelta = false;

	m_flPrevGLSwapWindowTime = 0.0f;
	
	int32 deltaX, deltaY;
	CGGetLastMouseDelta( &deltaX, &deltaY );
	memset( &m_lastShownPixels, 0, sizeof(m_lastShownPixels) );
	
	// set some silly rectangles for initial win frame and fs frame
	m_winFrame.origin.x		= 50.0f;
	m_winFrame.origin.y		= 50.0f;
	m_winFrame.size.width	= 640.0f;
	m_winFrame.size.height	= 480.0f;
	
	m_fsFrame = [[NSScreen mainScreen] frame];
	
	m_showPixelsCtx = NULL;
	m_lastKnownSwapInterval = -1;
	m_lastKnownSwapLimit = -1;
	m_frontPushCounter = 3;
	
	m_windowTitle = "";
}
void CCocoaMgr::AccumlateMouseDelta( int32 &x, int32 &y )
{
	if ( !m_bIgnoreNextMouseDelta )
	{
		m_MouseDeltaX += x;
		m_MouseDeltaY += y;
	}
	m_bIgnoreNextMouseDelta = false;
}

void CCocoaMgr::GetMouseDelta( int &x, int &y, bool bIgnoreNextDelta )
{
	if ( bIgnoreNextDelta )
		m_bIgnoreNextMouseDelta = bIgnoreNextDelta;
	x = m_MouseDeltaX;
	y = m_MouseDeltaY;
	m_MouseDeltaX = m_MouseDeltaY = 0;
}
void CCocoaMgr::ShowMainWindow()
{
	[m_window makeKeyAndOrderFront:nil];
}
void CCocoaMgr::RenderedSize( uint& width, uint& height, bool set )
{
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
void CCocoaMgr::DisplayedSize( uint &width, uint &height )
{
	if (m_view)
	{
		NSRect rect = [m_view frame];
		width = rect.size.width;
		height = rect.size.height;
	}
	else
	{
		width = height = 1;
	}
}
void CCocoaMgr::GetDesiredPixelFormatAttribsAndRendererInfo( uint **ptrOut, uint *countOut, GLMRendererInfoFields *rendInfoOut )
{
	Assert( m_pixelFormatAttribCount > 0 );
	
	if (ptrOut)		*ptrOut = m_pixelFormatAttribs;
	if (countOut)	*countOut = m_pixelFormatAttribCount;
	if (rendInfoOut)
	{
		GLMDisplayDB *db = GetDisplayDB();
		*rendInfoOut = ((*db->m_renderers)[ m_chosenRendererIndex ])->m_info;
	}
}
GLMDisplayDB *CCocoaMgr::GetDisplayDB( void )
{
	if (!m_displayDB)
	{
		m_displayDB = new GLMDisplayDB();		// creating the DB object does not do much other than init it to a good state.	
		m_displayDB->Populate();				// populate the tree
		
		// side effect: we fill in m_leopard and m_force_vsync..
		{
			GLMRendererInfoFields	info;
			m_displayDB->GetRendererInfo( 0, &info );
			m_leopard = (info.m_osComboVersion < 0x000A0600);
			m_force_vsync = info.m_badDriver1064NV;		// just force it if it's the bum NV driver
		}
	}
	return m_displayDB;
}

void CCocoaMgr::ProcessMessageInCocoaThread( CCocoaThreadMsg *pInMessage )
{
	CCocoaThreadMsg_CreateWindow *pMsg1 = dynamic_cast< CCocoaThreadMsg_CreateWindow* >( pInMessage );
	if ( pMsg1 )
	{
		// we could improve this to let the caller pass in a specific frame instead of just centering on the main screen
		
		NSRect screenFrame = [[NSScreen mainScreen] frame];
		int sw = screenFrame.size.width;
		int sh = screenFrame.size.height;
		NSRect newWinFrame;
		
		if ( pMsg1->m_bWindowed )
		{
			newWinFrame.origin.x = ((sw - pMsg1->m_nWidth) / 2);	// int math to truncate prior to assignment
			newWinFrame.origin.y = ((sh - pMsg1->m_nHeight) / 2);	// int math to truncate prior to assignment
		
			newWinFrame.size.width = pMsg1->m_nWidth;
			newWinFrame.size.height = pMsg1->m_nHeight;
		}
		else
		{
			newWinFrame = screenFrame;
		}
		
		InternalCreateWindow( pMsg1->m_Title, pMsg1->m_bWindowed, &newWinFrame );
	}
}
void CCocoaMgr::SendMessageToCocoaThread( CCocoaThreadMsg *pMessage, bool bWaitUntilDone )
{
	if ( ThreadGetCurrentId() == m_nRunLoopThreadID )
	{
		// We're already in the Cocoa thread. Process it immediately.
		ProcessMessageInCocoaThread( pMessage );
		delete pMessage;
	}
	else
	{
		Assert( !"We shouldn't be running in a thread anymore" );
		CCocoaThreadMsgContainer *pContainer = [[CCocoaThreadMsgContainer alloc] init];
		pContainer->m_pMessage = pMessage;
		[m_pCocoaBridge performSelectorOnMainThread:@selector(ProcessMessage:) withObject:pContainer waitUntilDone:bWaitUntilDone];
	}
}
void CCocoaMgr::PostEvent( const CCocoaEvent &theEvent, bool debugEvent )
{
	m_CocoaEventsMutex.Lock();
	
	CUtlLinkedList<CCocoaEvent,int> &queue = debugEvent ? m_CocoaEvents : m_DebugEvents;
	queue.AddToTail( theEvent );
	
	m_CocoaEventsMutex.Unlock();
}

bool CCocoaMgr::CreateApplicationObject()	// return false if this is already done (i.e. a mod is trying to kick things off again)
{
	//Assert( !m_application );
	if (!m_application)
	{
		// just do this stuff once
		// tell OSX we are actually an app
		ProcessSerialNumber psn = { 0, kCurrentProcess };
		TransformProcessType(&psn, kProcessTransformToForegroundApplication);
		SetFrontProcess(&psn);
		
		m_application = [NSValveApplication sharedApplication];
		return true;
	}
	else
	{
		return false;
	}
}
void CCocoaMgr::DestroyGameWindow()
{
	/*
	NOP baby - you get one window and like it.
	[[m_window delegate] release];
	[m_window close];
	
	m_window = NULL;
	m_view = NULL;
	*/
}
bool CCocoaMgr::CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height )
{
	// if m_window is already set, we skip this.
	if (!m_window)
	{
		// Most things with Cocoa objects have to be done in the Cocoa thread
		CCocoaThreadMsg_CreateWindow *pMessage = new CCocoaThreadMsg_CreateWindow;
		V_strncpy( pMessage->m_Title, pTitle, sizeof( pMessage->m_Title ) );
		pMessage->m_bWindowed = bWindowed;
		pMessage->m_nWidth = width;
		pMessage->m_nHeight = height;
		SendMessageToCocoaThread( pMessage, true );
	}
	return true;
}
bool CCocoaMgr::InternalCreateWindow( const char *pTitle, bool bWindowed, NSRect *frame )
{
	//stash the window title..
	if (pTitle)
	{
		m_windowTitle = strdup( pTitle );
	}
	
	// Setup an autorelease pool.
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	//-----------------------------------------------------------------------------------------
	// pick our favorite renderer (presently we just grab 0 in renderer table)
	//FIXME allow for other renderers to be selected somehow
	
	GLMRendererInfoFields rendererInfo;
	GetDisplayDB()->GetRendererInfo( m_chosenRendererIndex, &rendererInfo );
	//-----------------------------------------------------------------------------------------
	//- enforce minimum system requirements : OS X 10.7, Lion, and no GMA950, X3100, ATI X1600/X1900, or NV G7x.
	if (!CommandLine()->FindParm("-glmnosystemcheck"))	// escape hatch
	{
		if ( rendererInfo.m_osComboVersion < 0x0A0700 )
		{
			Error( "This game requires OS X version 10.7 or higher" );
			exit(1);
		}
		// forbidden chipsets
		if ( rendererInfo.m_atiR5xx || rendererInfo.m_intel95x || rendererInfo.m_intel3100 || rendererInfo.m_nvG7x )
		{
			Error( "This game does not support this type of graphics processor" );
			exit(1);
		}
	}
	
	
	//-----------------------------------------------------------------------------------------
	// write the preferred attribs
	uint *attCursor = m_pixelFormatAttribs;
	
	*attCursor++ = kCGLPFADoubleBuffer;
	*attCursor++ = kCGLPFANoRecovery;
	*attCursor++ = kCGLPFAAccelerated;
	*attCursor++ = kCGLPFADepthSize;
		*attCursor++ =  0;		// no explicit depth buffer is needed since FBO RT's are made for that
	*attCursor++ = kCGLPFAColorSize;
		*attCursor++ = 32;
	
	*attCursor++ = kCGLPFARendererID;
		*attCursor++ = rendererInfo.m_rendererID;

	if (CommandLine()->FindParm("-glmnobackingstore"))
	{
		*attCursor++ = kCGLPFABackingStore;
		*attCursor++ = 0;		// "NO BACKING STORE PLEASE" so swaps are possible
	}
	
	*attCursor++ = 0;
	
	// log attrib count
	m_pixelFormatAttribCount = attCursor - &m_pixelFormatAttribs[0];
	//-----------------------------------------------------------------------------------------
	// Create the window.
	int style = bWindowed ? s_windowedStyleMask : s_fullscreenStyleMask; 
	AppWindow *newWin = [[AppWindow alloc] initWithContentRect:*frame  styleMask:style  backing:NSBackingStoreBuffered  defer:YES];
	[newWin retain];	
	CocoaViewDelegate *delegate = [[CocoaViewDelegate alloc] init];
	[delegate retain];
	[newWin setDelegate: delegate ];
	// Set the title.
	// if pTitle is NULL, use previously stashed title..
	if (pTitle)
	{
		[newWin setTitle:[NSString stringWithUTF8String:pTitle]];	
	}
	else
	{
		[newWin setTitle:[NSString stringWithUTF8String:m_windowTitle]];
	}
	//-----------------------------------------------------------------------------------------
	// Set the content view to be our own NSOpenGLView, using the preferred attribute list.
	// **unless** we have a gl view from an old window, in which case we slot that one in instead.
	
	if (!m_view)	// one time only...
	{
		CGLPixelFormatAttribute *selAttribs	=	NULL;
		uint					selWords	=	0;
		this->GetDesiredPixelFormatAttribsAndRendererInfo( (uint**)&selAttribs, &selWords, NULL );
		
		NSOpenGLPixelFormat* nsglFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:(NSOpenGLPixelFormatAttribute*)selAttribs];
		CocoaViewBridge *newView = [[CocoaViewBridge alloc] initWithFrame:[newWin frame] pixelFormat:nsglFormat ];
		[ newView retain ];
		newView->m_pCocoaMgr = this;
		
		m_view = newView;		
	}
		
	if (1)
	{
		[[m_view openGLContext] makeCurrentContext];
		
		NSRect rect = [m_view frame];
		glViewport(0, 0, (GLsizei) rect.size.width, (GLsizei) rect.size.height);
		glScissor( 0,0, (GLsizei) rect.size.width, (GLsizei) rect.size.height );
	}
	
	[newWin setContentView:m_view];
	[newWin makeFirstResponder:m_view];
	[newWin setAcceptsMouseMovedEvents:YES];
	[newWin makeKeyAndOrderFront:nil];			// -drawRect gets invoked here.
	// - there is now a big white window on the screen.
	// - black it out and flush it
		
	ClearGLView();
	
	[ newWin setViewsNeedDisplay:YES ];	
	[ m_view setNeedsDisplay: YES ];
	if (!bWindowed)
	{
		[ newWin setLevel:NSMainMenuWindowLevel+1 ];			// move to front of stack
	}
	ClearGLView();
	[ newWin setViewsNeedDisplay:YES ];	
	[ m_view setNeedsDisplay: YES ];

	gGL = GetOpenGLEntryPoints(VoidFnPtrLookup_GlMgr);
	
	// It is now safe to call any base GL entry point that's supplied by gGL.
	// You still need to explicitly test for extension entry points, though!

	// if you don't sleep, you don't ncessarily get everything flushed to screen before movie player jumps on it..
	sleep(1);
	
	//-----------------------------------------------------------------------------------------
	// If old window exists, scrub it
	
	if (m_window)
	{
		[ m_window close ];
		[ m_window release ];
		m_window = NULL;
	}
	// replace with newly made window
	m_window	= newWin;
	//-----------------------------------------------------------------------------------------
	// Get rid of the autorelease pool.
	[pool release];
	// clear current GL context...
	[ NSOpenGLContext clearCurrentContext ];
	
	//this->PumpWindowsMessageLoop();
	m_fsEnable = !bWindowed;
	
	return true;
}

PseudoNSGLContextPtr CCocoaMgr::GetNSGLContextForWindow( void* windowref )
{
	WindowRef win = (WindowRef)windowref;
	
	if (win==[m_window windowRef])
	{
		PseudoNSGLContextPtr nsctx = [ m_view openGLContext ];
		
		Assert( nsctx != NULL );
		
		return nsctx;
	}
	else
	{
		return NULL;	// sorry, no idea
	}
}
void* CCocoaMgr::GetWindowRef()
{
	NSWindow *pWindow = (NSWindow*)m_window;
	return [pWindow windowRef];
}

void CCocoaMgr::SetWindowFullScreen( bool bFullScreen, int nWidth, int nHeight )
{
}

bool CCocoaMgr::IsWindowFullScreen()
{
  return m_fsEnable;
}

void  CCocoaMgr::SetForbidMouseGrab( bool bForbidMouseGrab )
{
}

PseudoNSGLContextPtr CCocoaMgr::GetGLContextForWindow( void* windowref )
{
  return GetNSGLContextForWindow(windowref);
}


PseudoNSGLContextPtr	 CCocoaMgr::GetMainContext()
{
  PseudoNSGLContextPtr nsctx = [ m_view openGLContext ];

  return nsctx;
}

PseudoNSGLContextPtr  CCocoaMgr::CreateExtraContext()
{
	return NULL;
}

void  CCocoaMgr::DeleteContext( PseudoNSGLContextPtr hContext )
{
}

bool  CCocoaMgr::MakeContextCurrent( PseudoNSGLContextPtr hContext )
{
	return false;
}

void  CCocoaMgr::SetMouseVisible( bool bState )
{
}

void  CCocoaMgr::GetDisplaySize( uint *puiWidth, uint *puiHeight )  // Retrieve the size of the monitor (desktop)
{
}

void  CCocoaMgr::GetNativeDisplayInfo( int nDisplay, uint &nWidth, uint &nHeight, uint &nRefreshHz )
{
  GLMDisplayDB	*db = GetDisplayDB();
  GLMRendererInfo	*pRenderInfo = ( *db->m_renderers )[ 0 ];
  GLMDisplayInfo	*pDisplayInfo = ( *pRenderInfo->m_displays )[ nDisplay ];
  GLMDisplayMode *displayModeInfo = (*pDisplayInfo->m_modes)[ -1 ];

  nRefreshHz = displayModeInfo->m_info.m_modeRefreshHz;
  nWidth = displayModeInfo->m_info.m_modePixelWidth;
  nHeight = displayModeInfo->m_info.m_modePixelHeight;
}




ConVar gl_swapdebug( "gl_swapdebug", "0");
ConVar gl_swaplimit( "gl_swaplimit", "0");
ConVar gl_swapinterval( "gl_swapinterval", "0");
ConVar gl_swaplimit_mt( "gl_swaplimit_mt", "3");
ConVar mac_fsbackground( "mac_fsbackground", "0");
ConVar mac_cursorwarp( "mac_cursorwarp", "1");
ConVar gl_blit_halfx( "gl_blit_halfx", "0" );
ConVar gl_blit_halfy( "gl_blit_halfy", "0" );
ConVar gl_disable_forced_vsync( "gl_disable_forced_vsync", "0" );
void CCocoaMgr::ShowPixels( CShowPixelsParams *params )
{
	// this is (probably) not being called on the main thread
	// send a message over for processing.
	Assert( m_window != NULL );
	Assert( m_view != NULL );
	[ [ m_view openGLContext ] makeCurrentContext ];
	if (m_frontPushCounter>0)
	{
		// we force the window to front after a mode change, on the first blit that comes through
		[m_window makeFirstResponder:m_view];
		[m_window setAcceptsMouseMovedEvents:YES];
		[m_window makeKeyAndOrderFront:nil];
		[ m_view update ];
		
		// further, if now in fullscreen mode and SL, we set the frame again in case the user managed to scoot it..
		if (m_fsEnable && !m_leopard)
		{
			[ m_window setFrame:m_fsFrame display:true ];
		}
		
		// and, reset the swap interval and limit
		m_lastKnownSwapInterval		= -1;		
		m_lastKnownSwapLimit		= -1;
		
		m_frontPushCounter--;
	}
	else
	{
		if (m_fsEnable)
		{
			NSInteger targetLevel = 0;
			if (mac_fsbackground.GetInt() && !m_leopard)	// SL or better, and user asked for FS to be visible as background when non-front app
			{
				[ m_window setHidesOnDeactivate: NO ];
				// try to avoid resetting this every frame unless it needs some settin'
				NSApplication *pApplication = (NSApplication*)m_application;
				targetLevel = [pApplication isActive] ? NSMainMenuWindowLevel+1 : kCGDesktopWindowLevel;
			}
			else
			{
				if (!CommandLine()->FindParm("-glmdebugfullscreen"))
				{
					[ m_window setHidesOnDeactivate: YES ];
				}
				targetLevel = NSMainMenuWindowLevel+1;
			}
			if ( [m_window level] != targetLevel )
			{
				[ m_window setLevel:targetLevel ];			// move to front of stack
				[ m_view update ];
			}
		}
		else
		{
			[ m_window setHidesOnDeactivate: NO ];
		}
	}
	
	// in the 10.6 world we go with the assumption that the context that is sending us pixels, was shared off of the
	// view bridge's context.  So there should be no need at all to create a new context - we can just target the view context
	// in the window.  This should work whether that context is fullscreen or not.
	// about all we need to know is the size of the slab handed us, and the size of the backing store of the view context.
	// note that on 10.6, we have the option of the view-context's backing store staying small even though the window/view is
	// large.
	
	// first look at the FS state and see if we need to flip
	if ( (params->m_fsEnable!=0) != (m_fsEnable!=0) )
	{
		// flip it
		// honor res changes here by resizing the backing store of the gl view
		
		NSRect screenFrame = [[NSScreen mainScreen] frame];
		if (params->m_fsEnable)
		{
			// go to FS... save the old windowed frame first?
			NSRect oldWinFrame = [ m_window frame ];
			SetWindowedFrame( &oldWinFrame, false );		// latch but do not act
						
			SetFullscreenFrame( &screenFrame, true );				// use the latched rect from setup time (or consider grabbing a new one which could be more current)
		}
		else
		{
			// go to windowed.
			// see if the m_winFrame is the same size as this inbound blit.
			// if it's not, construct a new one.
			NSRect	winFrame = m_winFrame;
			if ( (winFrame.size.width != params->m_width) || (winFrame.size.height != params->m_height) )
			{
				int sw = screenFrame.size.width;
				int sh = screenFrame.size.height;
				winFrame.origin.x = ((sw - params->m_width) / 2);	// int math to truncate prior to assignment
				winFrame.origin.y = ((sh - params->m_height) / 2);	// int math to truncate prior to assignment
				
				winFrame.size.width = params->m_width;
				winFrame.size.height = params->m_height;
			}
			
			SetWindowedFrame( &winFrame, true );					// use the latched rect from last transition to FS (see above, we save it)
		}
	}
	if (!params->m_onlySyncView)
	{
		// save old context
		NSOpenGLContext *curr = [ NSOpenGLContext currentContext ];
		
		// get target context
		m_showPixelsCtx = [ m_view openGLContext ];
		
		// make it current
		[m_showPixelsCtx makeCurrentContext];
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
			// only do the funky forced vsync for NV on 10.6.4 and only if the bypass is not turned on
			if (m_force_vsync && (gl_disable_forced_vsync.GetInt()==0))
			{
				swapInterval	= 1;
				swapLimit		= 1;
			}
		}
		
		// only touch them on changes, or right after a change in windowed/FS state
		if ( (swapInterval!=m_lastKnownSwapInterval) || (swapLimit!=m_lastKnownSwapLimit) )
		{
			[m_showPixelsCtx setValues:&swapInterval forParameter:NSOpenGLCPSwapInterval];
			m_lastKnownSwapInterval = swapInterval;
			
			if (swapLimit)
			{
				CGLEnable( CGLGetCurrentContext(), kCGLCESwapLimit );
			}
			else
			{
				CGLDisable( CGLGetCurrentContext(), kCGLCESwapLimit );
			}
			m_lastKnownSwapLimit = swapLimit;
			if (gl_swapdebug.GetInt())	// only touch this with swap debug on for now
			{
				GLint maxMPswaps = gl_swaplimit_mt.GetInt();
				CGLSetParameter( CGLGetCurrentContext(), kCGLCPMPSwapsInFlight, &maxMPswaps  );
				printf("\n ----- MT swap limit = %d \n", maxMPswaps );
			}
			
			printf("\n ##### swap interval = %d     swap limit = %d #####\n", m_lastKnownSwapInterval, m_lastKnownSwapLimit );
			fflush(stdout);
		}
		if (!params->m_noBlit)
		{
			if ( params->m_useBlit ) // FBO blit path - which is what we *should* be using.  But if the params say no, then don't do it because the ext is not there.
			{
				// bind a quickie FBO to enclose the source texture
				GLint	myreadfb = 1000;
				
				glBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, myreadfb);
				__checkgl__();
				
				glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0);		// to the default FB/backbuffer
				__checkgl__();
				// attach source tex to source FB
				glFramebufferTexture2DEXT( GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, params->m_srcTexName, 0);
				__checkgl__();
				
				// blit
				
				int srcxmin = 0;
				int srcymin = 0;
				int srcxmax = params->m_width;
				int srcymax = params->m_height;
				NSRect dstframe = [m_view frame];
				// normal blit
				int dstxmin = 0;
				int dstymin = 0;
				int dstxmax = dstframe.size.width;
				int dstymax = dstframe.size.height;
				
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
				__checkgl__();
				// detach source tex
				glFramebufferTexture2DEXT( GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
				__checkgl__();
				glBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, 0);
				__checkgl__();
				glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0);		// to the default FB/backbuffer
				__checkgl__();
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
						__checkgl__();
					GLint width;
					glGetTexLevelParameteriv(	GL_TEXTURE_2D,			//target
												0,						//level,
												GL_TEXTURE_WIDTH,		//pname
												&width
											);
					__checkgl__();					
				}
				else
				{
					glBindTexture( GL_TEXTURE_2D, 0 );
						__checkgl__();
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
				__checkgl__();
				if (texing)
				{
					glBindTexture( GL_TEXTURE_2D, 0 );
					__checkgl__();
					glDisable(GL_TEXTURE_2D);
				}
				
			}
		}
		[m_showPixelsCtx flushBuffer];	
		m_lastShownPixels = *params;
	CFastTimer tm;
	tm.Start();

		[curr makeCurrentContext];

	m_flPrevGLSwapWindowTime = tm.GetDurationInProgress().GetMillisecondsF();

	}
}
void CCocoaMgr::SetWindowedFrame( NSRect *frame, bool forEffect )
{
	// this function has to do different things depending on whether it's Leopard or Snow Leopard.
	
	if (frame)
	{
		m_winFrame = *frame;
	}
	
	if (forEffect)
	{
		// ask for window to be bumped to front on next few blits (and scrubbed, if a -drawRect lands on us)
		m_frontPushCounter = 3;
		
		if (m_fsEnable)
		{
			// -- transitioning into windowed --
			if (m_leopard)
			{
				// re create window as non full screen
				// FIXME need to get proper title
				InternalCreateWindow( NULL, true, &m_winFrame );	// NULL means "use old title", bWindowed = true
				
				SetFullscreenUIMode( false );
			}
			else
			{
				// SL or later
				
				// change the style mask and layering, then change the frame
				[ m_window setStyleMask:s_windowedStyleMask ];
				[ m_window setLevel: NSNormalWindowLevel ];
				// fix the title back..
				[ m_window setTitle:[NSString stringWithUTF8String:m_windowTitle]];
 			}
		
			// also do these tweaks
			[ m_window setOpaque:YES ];
			[ m_window setHidesOnDeactivate:NO ];
			m_fsEnable = false;
		}
		// now set the frame
		NSRect frameRect = [ m_window frameRectForContentRect:m_winFrame];
		[ m_window setFrame:frameRect display:true ];

		// make sure it's in front
		[ m_window makeKeyAndOrderFront:nil ];		
		//ClearGLView();
	}
}
void CCocoaMgr::SetFullscreenFrame( NSRect *frame, bool forEffect )
{
	// this function has to do different things depending on whether it's Leopard or Snow Leopard.
	if (frame)
	{
		m_fsFrame = *frame;
	}
	
	if (forEffect)
	{
		// ask for window to be bumped to front on next few blits
		m_frontPushCounter = 3;
		
		if (!m_fsEnable)
		{
			// -- transitioning into fullscreen --
			if (m_leopard)
			{
				// re-create the window as fullscreen
				InternalCreateWindow( NULL, false, &m_fsFrame );	// NULL means use old title, bWindowed = false
				SetFullscreenUIMode( true );
			}
			else
			{
				[ m_window setFrame:m_fsFrame display:false ];
				
				// SL or better
				// change the style mask and layering, then change the frame
				
				[ m_window setStyleMask:NSBorderlessWindowMask ];		// slam the style mask
			}
			
			if (!CommandLine()->FindParm("-glmdebugfullscreen"))
			{
				[ m_window setLevel:NSMainMenuWindowLevel+1 ];			// move to front of stack
				[ m_window setHidesOnDeactivate: mac_fsbackground.GetInt() ? NO : YES ];
			}
			else
			{
				[ m_window setHidesOnDeactivate:NO ];
			}
			[ m_window setOpaque:YES ];
			// now set the frame
			[ m_window setFrame:m_fsFrame display:true ];
			m_fsEnable = true;
		}
		// make sure it's in front
		[ m_window makeKeyAndOrderFront:nil /*m_window*/];
		//ClearGLView();
	}
}
void CCocoaMgr::ClearGLView( void )
{
	if (m_view)
	{
		[[m_view openGLContext] makeCurrentContext];
			
		GLfloat clear_color[4] = { 0.05f, 0.05f, 0.05f, 1.0f };	// near black
		glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
		glClear(GL_COLOR_BUFFER_BIT+GL_DEPTH_BUFFER_BIT+GL_STENCIL_BUFFER_BIT);
			
		glFinish();		
		[[m_view openGLContext] flushBuffer];
	}
}

// we should probably ignore this if we're in fullscreen mode
void CCocoaMgr::MoveWindow( int x, int y )
{
	if (!m_fsEnable)
	{
		NSWindow *pWindow = (NSWindow*)m_window;
		[pWindow setFrameOrigin:NSMakePoint( x, y )];
	}
}
// we should probably ignore this if we're in fullscreen mode
void CCocoaMgr::SizeWindow( int width, int tall )
{
	if (!m_fsEnable)
	{
		NSWindow *pWindow = (NSWindow*)m_window;
		[pWindow setContentSize:NSMakeSize( width, tall )];
	}

	// We don't want to clear on every resize because if we are threaded rendering, this will collide.
	// This static just means we only ever clear when our window is created
	// This fixes ALT-TAB

	static bool firstClear = true;
	if (firstClear)
	{
		ClearGLView();
		firstClear = false;
	}
}

int CCocoaMgr::GetEvents( CCocoaEvent *pEvents, int nMaxEventsToReturn, bool debugEvent )
{
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
void CCocoaMgr::SetCursorPosition( int x, int y )
{
	// scale, because screen coords may not match engine's video mode resolution
	float modeWidth = m_lastShownPixels.m_width;
	float modeHeight = m_lastShownPixels.m_height;
	
	if ( (modeWidth != m_view.frame.size.width ) && (modeHeight != m_view.frame.size.height ) )
	{
		// apply scale such that our coords make sense back in the engine's world
		// multiply by engine extent, divide by window extent
		x *= m_view.frame.size.width;	x /= modeWidth;
		y *= m_view.frame.size.height;	y /= modeHeight;
	}
	// Source view coords (y down) -> NSView coordinates (Y up)
	NSPoint pt;
	pt.x = x;
	pt.y = m_view.frame.size.height - y;	// Make Y go up.
	// NSView coords -> window base coords
    NSPoint windowCoords = [m_view convertPoint:pt toView:nil];
    // window base coords -> screen coords (Y up)
    NSPoint screenCoords = [m_window convertBaseToScreen:windowCoords];
	// screen coords (Y up) -> CG screen coords (Y down)
	CGPoint cgpt;
	cgpt.x = screenCoords.x;
	cgpt.y = [m_window screen].frame.size.height - screenCoords.y;
	// if you warp the cursor while the titlebar is grabbed (after alt-tab) then the windows goes nuts
	// with its positioning, so only warp if we aren't in that state
	if ( !s_bBlockWarpCursor )
	{
		if( mac_cursorwarp.GetInt() )
		{
			CGWarpMouseCursorPosition( cgpt );
		}
	}
}
NSPoint CCocoaMgr::ScreenCoordsToSourceWindowCoords( NSView *pView, NSPoint screenCoord )
{	
    // Go to Window base coordinates.
    NSPoint windowBase = [[pView window] convertScreenToBase:screenCoord];
	
	// Convert from Window base to view coordinates.
    NSPoint viewCoords = [pView convertPoint:windowBase fromView:nil];
	// Flip Y (they're reporting Y going up whereas Source wants it going down).
    viewCoords.y = pView.frame.size.height - viewCoords.y;
		
	// scale, because screen coords may not match engine's video mode resolution
	float modeWidth = m_lastShownPixels.m_width;
	float modeHeight = m_lastShownPixels.m_height;
	
	if ( (modeWidth != m_view.frame.size.width ) && (modeHeight != m_view.frame.size.height ) )
	{
		// apply scale such that our coords make sense back in the engine's world
		// multiply by engine extent, divide by window extent
		viewCoords.x *= modeWidth;	viewCoords.x /= pView.frame.size.width;
		viewCoords.y *= modeHeight;	viewCoords.y /= pView.frame.size.height;
	}
	else if ( modeWidth == 0.0 || modeHeight == 0.0 )
	{
		// shrug.  an event got to us before the first blit, so we don't know how to scale it.
		// just slam it to 0,0.
		viewCoords.x = viewCoords.y = 0;
	}
  	
    return viewCoords;
}

void CCocoaMgr::StopRunLoop()
{
	// Always have an autorelease pool handy...
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSApplication *pApplication = (NSApplication*)m_application;
	[pApplication performSelectorOnMainThread:@selector(stop:) withObject:pApplication waitUntilDone:true];
	// NSApplication::stop only sets a flag telling it to exit the run loop AFTER the next event gets processed,
	// so let's generate a dummy event.
	PostDummyEvent();
	[pool release];
}
static CGImageSourceRef CreateCGImageSourceFromFile(const char* the_path)
{
    CFURLRef the_url = NULL;
    CGImageSourceRef source_ref = NULL;
	CFStringRef cf_string = NULL;
	
	cf_string = CFStringCreateWithCString(  NULL, the_path, kCFStringEncodingUTF8  );
	if(!cf_string)
	{
		return NULL;
	}
	
    the_url = CFURLCreateWithFileSystemPath(NULL, cf_string, kCFURLPOSIXPathStyle, false );
	CFRelease(cf_string);	
	if(!the_url)
	{
		return NULL;
	}
	
    source_ref = CGImageSourceCreateWithURL( the_url, NULL );
	CFRelease(the_url);
	
	return source_ref;
}

static CGImageRef CreateCGImageFromCGImageSource(CGImageSourceRef image_source)
{
	CGImageRef image_ref = NULL;
	
    if(NULL == image_source)
	{
		return NULL;
	}
	
	// Get the first item in the image source (some image formats may
	// contain multiple items).
	image_ref = CGImageSourceCreateImageAtIndex(image_source, 0, NULL);
	return image_ref;
}

void CCocoaMgr::SetApplicationIcon( const char *pchAppIconFile )
{
	CGImageRef imgRef = CreateCGImageFromCGImageSource( CreateCGImageSourceFromFile( pchAppIconFile ) );
	if ( imgRef )
	{
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		NSBitmapImageRep* bir = [[NSBitmapImageRep alloc] initWithCGImage:imgRef];
		NSImage* img = [[NSImage alloc] initWithData:[ bir TIFFRepresentation]];
		[NSApp setApplicationIconImage:img];
		[img release];
		[bir release];
		[pool release];
	}
}

void CCocoaMgr::FinishLaunchingApplication()
{
	ThreadSetDebugName( "CCocoaMgr" );
	m_nRunLoopThreadID = ThreadGetCurrentId();
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSApplication *pApplication = (NSApplication*)m_application;
	
	// Hookup our delegate.
	m_pCocoaBridge = [[CocoaBridge alloc] init];
	[m_pCocoaBridge retain];
	
	m_pCocoaBridge->m_pCocoaMgr = this;
	[pApplication setDelegate:m_pCocoaBridge];
	
	[pApplication finishLaunching];
	[pool release];
}

void CCocoaMgr::PumpWindowsMessageLoop()
{
//	NSOpenGLContext *curr = [ NSOpenGLContext currentContext ];
//	[ NSOpenGLContext clearCurrentContext ];
	bool bWorkToDo = true;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSApplication *pApplication = (NSApplication*)m_application;
	while ( bWorkToDo )
	{
		NSEvent *event = [pApplication nextEventMatchingMask: NSAnyEventMask
								untilDate: [NSDate distantPast]
							    inMode: NSDefaultRunLoopMode
								dequeue: YES];
		if (event != nil)
		{
			[pApplication sendEvent: event];
		}
		else 
		{
			bWorkToDo = false;
		}
		//[event release];	// 96.5K malloc
    }
	[pool release];
//	[ curr makeCurrentContext ];
}
void CCocoaMgr::WaitUntilUserInput( int msSleepTime )
{
	if ( !msSleepTime || msSleepTime < 0 )
		return;
//	NSOpenGLContext *curr = [ NSOpenGLContext currentContext ];
//	[ NSOpenGLContext clearCurrentContext ];
	NSApplication *pApplication = (NSApplication*)m_application;
	NSEvent *event = [pApplication nextEventMatchingMask: NSAnyEventMask
											   untilDate: [NSDate dateWithTimeIntervalSinceNow:(double)msSleepTime/1000 ]
													  inMode: NSDefaultRunLoopMode
													 dequeue: YES];
	if (event != nil)
	{
		[pApplication sendEvent: event];
	}
//	[ curr makeCurrentContext ];
}


// ------------------------------------------------------------------------------------ //
// Access to private Symbolication framework, for stack crawling
// ------------------------------------------------------------------------------------ //
// this is all pieced together using class-dump and some other references:
// http://seriot.ch/resources/dynamic_iPhone_headers/2_2_1/
// http://seriot.ch/resources/dynamic_iPhone_headers/2_2_1/VMUSymbolicator.h
// http://seriot.ch/resources/dynamic_iPhone_headers/2_2_1/VMUSymbol.h
// http://www.cocoadev.com/index.pl?StackTraces
struct _VMURange {
    unsigned long long location;
    unsigned long long length;
};
@interface VMUAddressRange : NSObject {
    struct _VMURange _addressRange;
}
@end
@interface VMUSymbol : VMUAddressRange {
    NSString *_name;
    NSString *_mangledName;
    void *_owner;
    unsigned int _flags;
}
- (id)name;
- (id)sourceInfoForAddress:(unsigned long long)address;
@end

@interface VMUSymbolicator : NSObject {
    NSMutableArray *_symbolOwners;
    NSArray *_symbolOwnerAddressRanges;
    NSString *_path;
    void *_machTaskContainer;
    BOOL _isProtected;
}
+ (id)symbolicatorForPid:(int)fp8;
+ (VMUSymbolicator*)symbolicatorForTask:(unsigned)task;
- (id)symbolForAddress:(unsigned long long)address;
@end
struct _NSZone { };
@interface VMUSourceInfo : VMUAddressRange <NSCopying>
{
	NSString *_path;
	NSUInteger _lineNumber;
	NSUInteger _fileOffset;
}
+ (id)sourceInfoWithPath:(id)arg1 addressRange:(_VMURange)arg2 lineNumber:(NSUInteger)arg3 fileOffset:(NSUInteger)arg4;
- (id)initWithPath:(id)arg1 addressRange:(_VMURange)arg2 lineNumber:(NSUInteger)arg3 fileOffset:(NSUInteger)arg4;
- (id)path;
- (id)fileName;
- (NSUInteger)lineNumber;
- (NSUInteger)fileOffset;
- (_VMURange)addressRange;
- (NSInteger)compare:(id)arg1;
- (BOOL)isEqualToSourceInfo:(id)arg1;
- (id)description;
- (void)dealloc;
- (id)copyWithZone:(_NSZone*)arg1;
@end

static id _symbolicator = nil;
#define SYMBOLICATION_FRAMEWORK @"/System/Library/PrivateFrameworks/Symbolication.framework"
																						/*  redundant ?
																						@interface NSObject (SymbolicatorAPIs) 
																						- (id)symbolicatorForTask:(mach_port_t)task;
																						- (id)symbolForAddress:(uint64_t)address;
																						- (id)sourceInfoForAddress:(unsigned long long)address;
																						- (void)forceFullSymbolExtraction;
																						- (_VMURange)addressRange;
																						@end
																						*/
static inline id rb_objc_symbolicator(void) 
{
    if (_symbolicator == nil)
	{
		NSError *error;
		if (![[NSBundle bundleWithPath:SYMBOLICATION_FRAMEWORK] loadAndReturnError:&error])
		{
			NSLog(@"Cannot load Symbolication.framework: %@", error);
			abort();    
		}
		Class VMUSymbolicator = NSClassFromString(@"VMUSymbolicator");
		_symbolicator = [VMUSymbolicator symbolicatorForTask:mach_task_self()];
		assert(_symbolicator != nil);
    }
    return _symbolicator;
}
void CCocoaMgr::GetStackCrawl( CStackCrawlParams *params )
{
	params->m_crawlText[0] = 0;
	char *cursor = params->m_crawlText;
	bool shortenFuncNames = true;
	
	VMUSymbolicator * symbolicator = rb_objc_symbolicator();
	NSArray * addresses = [NSThread callStackReturnAddresses];
	params->m_frameCount = 0;
	for (NSNumber * address in addresses)
	{
		if (params->m_frameCount < params->m_frameLimit)
		{
			VMUSymbol * symbol = [symbolicator symbolForAddress:[address unsignedLongLongValue]];
			VMUSourceInfo *sourceinfo = [symbol sourceInfoForAddress:[address unsignedLongLongValue]];
			
			NSString	*funcname = [symbol name];
			NSString	*funcpath = [sourceinfo path];
			NSUInteger	funcline = [sourceinfo lineNumber];
			
			char	tempname[1000];
			char	temppath[1000];
			tempname[0] = 0;
			temppath[0] = 0;
					
			[ funcname getCString:tempname maxLength:(sizeof(tempname)) encoding:[NSString defaultCStringEncoding ] ];
			if(shortenFuncNames)
			{
				// find the first '(' and term the string there
				char *leftparen = strchr( tempname, '(' );
				if(leftparen)
				{
					*leftparen = 0;
				}
			}
			[ funcpath getCString:temppath maxLength:(sizeof(temppath)) encoding:[NSString defaultCStringEncoding ] ];
			Q_snprintf( cursor, sizeof(params->m_crawlText)-(cursor-params->m_crawlText)-2, "%s   --   %s:%d", tempname, temppath, funcline );
			
			// log this entry
			params->m_crawl[ params->m_frameCount ] = (void *)[ address unsignedLongLongValue ];
			params->m_crawlNames[ params->m_frameCount ] = cursor;
			
			cursor += strlen( cursor );
			*cursor++ = 0;
			// unsure if these need to be freed or not.
			//[funcname release];
			//[funcpath release];
			//[sourceinfo release];
			//[symbol release];
			params->m_frameCount++;
		}
	}
}
// ------------------------------------------------------------------------------------ //

void CCocoaMgr::PostDummyEvent()
{
	NSPoint location = {0,0};
	NSEvent *pEvent = [NSEvent otherEventWithType:NSApplicationDefined
		location:location 
		modifierFlags:0 
		timestamp:0
		windowNumber:0
		context:nil
		subtype:0
		data1:0
		data2:0
		];
	NSApplication *pApplication = (NSApplication*)m_application;
	[pApplication postEvent:pEvent atStart:false];
}
bool CCocoaMgr::IsDebugEvent( CCocoaEvent& event )
{
	bool result = false;
	#if GLMDEBUG
		// simple rule for now, if the option key is involved, it's a debug key
		// but only if GLM debugging is builtin
		
		result |= ( (event.m_EventType == CocoaEvent_KeyDown) && ((event.m_ModifierKeyMask & (1<<eAltKey))!=0) );
	#endif
	
	return result;
}
void CCocoaMgr::WaitForApplicationToFinishLaunching()
{
	m_AppObjectInitialized.Wait();
}
void CCocoaMgr::applicationDidFinishLaunching()
{
	m_AppObjectInitialized.Set();
}
void CCocoaMgr::applicationWillTerminate()
{
	Msg( "\n** CCocoaMgr::applicationWillTerminate\n\n" );
}

bool CCocoaMgr::Connect( CreateInterfaceFn factory )
{
	return true;
}
void CCocoaMgr::Disconnect()
{
}
void *CCocoaMgr::QueryInterface( const char *pInterfaceName )
{
	if ( !Q_stricmp( pInterfaceName, COCOAMGR_INTERFACE_VERSION ) )
		return this;
	return NULL;
}
// Init, shutdown
InitReturnVal_t CCocoaMgr::Init()
{
	return INIT_OK;
}
void CCocoaMgr::Shutdown()
{
}

@interface SteamThreadSafetyObject : NSObject {}
- (SteamThreadSafetyObject *) initThreadSafety;
- (void) threadSafetyEntry;
@end
@implementation SteamThreadSafetyObject
- (SteamThreadSafetyObject *) initThreadSafety
{
	// forcing an NSThread to spawn, even for an empty method, will switch Cocoa to thread-safe mode.
	self = [super init];
	[NSThread detachNewThreadSelector:@selector(threadSafetyEntry:)
							 toTarget:self withObject:nil];
	return self;
}
- (void) threadSafetyEntry {}
@end
void macMakeCocoaThreadSafe(void)
{
	SteamThreadSafetyObject *obj = [[SteamThreadSafetyObject alloc] init];
	[obj release];
}

// ------------------------------------------------------------------------------------ //
// ValveCocoaMain implementation.
// ------------------------------------------------------------------------------------ //
CThreadEvent g_AppObjectTerminated;
CThreadEvent g_MainFunctionThreadExiting;
class CAppSystemGroup;
struct MainFunctionThreadArgs_t
{
	CAppSystemGroup *pApp;
	int m_nReturnValue;
};
uintp MainFunctionThread( void *pParam ) 
{
	DeclareCurrentThreadIsMainThread();
	void *pPool = macMakeAutoreleasePool();
	
	// Now run the Valve stuff.
	MainFunctionThreadArgs_t *pArgs = (MainFunctionThreadArgs_t*)pParam;
	
	pArgs->m_nReturnValue = pArgs->pApp->Run();
	
	// Stop the NSApplication run loop and wait for it to stop.
	g_CocoaMgr.StopRunLoop();
	// Synchronize the Valve thread and the main() thread exit.
	g_MainFunctionThreadExiting.Set();
	macReleaseAutoreleasePool( pPool );
	return 0;
}
extern "C" int ValveCocoaMain( CAppSystemGroup *pApp )
{
	// Spawn a thread for all the normal Valve stuff. 
	// This is Valve's main() thread as far as it's concerned.
	MainFunctionThreadArgs_t args;
	args.pApp = pApp;
	void *pPool = macMakeAutoreleasePool();
	
	macMakeCocoaThreadSafe();
	
	// Create the NSApplication object and run it. 
	// This will block until the application calls CCocoaMgr::StopRunLoop.
	// it will return false if we did this already, in which case we will skip the "finish launching" stuff.
	if (g_CocoaMgr.CreateApplicationObject())
	{	
		//only do this once..
		g_CocoaMgr.FinishLaunchingApplication();
	}
	MainFunctionThread( &args );
	
	// Synchronize the Valve thread and the main() thread exit.
	g_AppObjectTerminated.Set();
	
	macReleaseAutoreleasePool( pPool );
	return args.m_nReturnValue;
}

// ------------------------------------------------------------------------------------ //
// GLMDisplayDB stuff which is going to live in CocoaMgr's module from now on..
//===============================================================================
//  GLMDisplayMode, GLMDisplayInfo, GLMRendererInfo, GLMDisplayDB methods
GLMDisplayMode::GLMDisplayMode( uint width, uint height, uint refreshHz )
{
	m_info.m_modePixelWidth = width;
	m_info.m_modePixelHeight = height;
	m_info.m_modeRefreshHz = refreshHz;
}
GLMDisplayMode::~GLMDisplayMode()
{
	// empty
}
void	GLMDisplayMode::Dump( int which )
{
	GLMPRINTF(("\n             # %-2d  width=%-4d  height=%-4d  refreshHz=%-2d", which, m_info.m_modePixelWidth, m_info.m_modePixelHeight, m_info.m_modeRefreshHz ));
}

//===============================================================================
GLMDisplayInfo::GLMDisplayInfo( CGDirectDisplayID displayID, CGOpenGLDisplayMask displayMask )
{	
	m_info.m_cgDisplayID			= displayID;
	m_info.m_glDisplayMask			= displayMask;
	
	// extract info about this display such as pixel width and height
	m_info.m_displayPixelWidth		= (uint)CGDisplayPixelsWide( m_info.m_cgDisplayID );
	m_info.m_displayPixelHeight		= (uint)CGDisplayPixelsHigh( m_info.m_cgDisplayID );
	m_modes = NULL;
}
GLMDisplayInfo::~GLMDisplayInfo( void )
{
	if (m_modes)
	{
		// delete all the new'd display modes
		FOR_EACH_VEC( *m_modes, i )
		{
			delete (*this->m_modes)[i];
		}
		delete m_modes;
		m_modes = NULL;
	}
}
extern "C" int DisplayModeSortFunction( GLMDisplayMode * const *A, GLMDisplayMode * const *B )
{
	int bigger = -1;
	int smaller = 1;	// adjust these for desired ordering
	// check refreshrate - higher should win
	if ( (*A)->m_info.m_modeRefreshHz > (*B)->m_info.m_modeRefreshHz )
	{	
		return bigger;
	}
	else if ( (*A)->m_info.m_modeRefreshHz < (*B)->m_info.m_modeRefreshHz )
	{
		return smaller;
	}
	// check area - larger mode should win
	int areaa = (*A)->m_info.m_modePixelWidth * (*A)->m_info.m_modePixelHeight;
	int areab = (*B)->m_info.m_modePixelWidth * (*B)->m_info.m_modePixelHeight;
	if ( areaa > areab )
	{	
		return bigger;
	}
	else if ( areaa < areab )
	{
		return smaller;
	}
	
	return 0;	// equal rank
}

void	GLMDisplayInfo::PopulateModes( void )
{
	Assert( !m_modes );
	m_modes = new CUtlVector< GLMDisplayMode* >;
	
	CFArrayRef		modeList;
//	CGDisplayErr	cgderr;
	CFDictionaryRef cgvidmode;
	CFNumberRef		number;
	CFBooleanRef	boolean;
	
	modeList = CGDisplayAvailableModes( m_info.m_cgDisplayID );
	if ( modeList != NULL )
	{
		//  examine each mode
		CFIndex count = CFArrayGetCount( modeList );
		
		for (CFIndex i = 0; i < count; i++) 
		{
			long modeHeight = 0, modeWidth = 0;
			long depth = 0;
			long refreshrate = 0;
			Boolean usable, stretched = false;
			
			// grab the mode dictionary
			cgvidmode = (CFDictionaryRef)CFArrayGetValueAtIndex( modeList, i);
			
			// grab mode params we need
			number = (CFNumberRef)CFDictionaryGetValue(cgvidmode, kCGDisplayBitsPerPixel);
			CFNumberGetValue(number, kCFNumberLongType, &depth);
			
			boolean = (CFBooleanRef)CFDictionaryGetValue(cgvidmode, kCGDisplayModeUsableForDesktopGUI) ;
			usable = CFBooleanGetValue(boolean);
			
			boolean = (CFBooleanRef)CFDictionaryGetValue(cgvidmode, kCGDisplayModeIsStretched);
			if (NULL != boolean) 
			{
				stretched = CFBooleanGetValue(boolean);
			}
			
			if ( usable && (!stretched) && (depth==32) )
			{
				// we're going to log this mode to the mode table.
				
				// get height of mode
				number = (CFNumberRef)CFDictionaryGetValue( cgvidmode, kCGDisplayHeight );
				CFNumberGetValue(number, kCFNumberLongType, &modeHeight);
				
				// get width of mode
				number = (CFNumberRef)CFDictionaryGetValue( cgvidmode, kCGDisplayWidth );
				CFNumberGetValue(number, kCFNumberLongType, &modeWidth);
				
				// get refresh rate of mode
				number = (CFNumberRef)CFDictionaryGetValue( cgvidmode, kCGDisplayRefreshRate ); 
				double flrefreshrate = 0.0f;
				CFNumberGetValue( number, kCFNumberDoubleType, &flrefreshrate );
				refreshrate = (int)flrefreshrate;
				// exclude silly small modes
				if ( (modeHeight >= 384) && (modeWidth >= 512) )
				{
					GLMDisplayMode *newmode = new GLMDisplayMode( modeWidth, modeHeight, refreshrate );
					m_modes->AddToTail( newmode );
				}
			}
		}
	}
	
	// now sort the modes
	// primary key is refresh rate
	// secondary key is area
	m_modes->Sort( DisplayModeSortFunction );
}

void	GLMDisplayInfo::Dump( int which )
{
	GLMPRINTF(("\n         #%d: GLMDisplayInfo @ %p, cg-id=%08x  display-mask=%08x  pixwidth=%d  pixheight=%d", which, this, m_info.m_cgDisplayID, m_info.m_glDisplayMask, m_info.m_displayPixelWidth,  m_info.m_displayPixelHeight ));
	FOR_EACH_VEC( *m_modes, i )
	{
		(*m_modes)[i]->Dump(i);
	}
}

//===============================================================================
GLMRendererInfo::GLMRendererInfo( GLMRendererInfoFields *info )
{
	NSAutoreleasePool	*tempPool = [[NSAutoreleasePool alloc] init ];
	// absorb info obtained so far by caller
	m_info = *info;
	m_displays = NULL;
	// gather more info using a dummy context
	unsigned int attribs[] = 
	{
		kCGLPFADoubleBuffer, kCGLPFANoRecovery, kCGLPFAAccelerated,
		kCGLPFADepthSize, 0,
		kCGLPFAColorSize, 32,
		kCGLPFARendererID, info->m_rendererID,
		0
	};
	NSOpenGLPixelFormat	*pixFmt		=	[[NSOpenGLPixelFormat alloc] initWithAttributes:(NSOpenGLPixelFormatAttribute*)attribs]; 
	NSOpenGLContext		*nsglCtx	=	[[NSOpenGLContext alloc] initWithFormat: pixFmt shareContext: NULL ];
	[nsglCtx makeCurrentContext]; // this is a no-op if nsglCtx is null! no context bound.
		
	// run queries.
	char *gl_ext_string = (char*)glGetString(GL_EXTENSIONS);
	uint vers = m_info.m_osComboVersion;
	// avoid crashing due to strstr'ing NULL pointer returned from glGetString
	if (!gl_ext_string)
	  gl_ext_string = "";

	// effectively blacklist the renderer if it doesn't actually work; sort it to back of list
	if ( !nsglCtx )
	{
		m_info.m_vidMemory = 1;
		m_info.m_texMemory = 1;
	}
	
	//-------------------------------------------------------------------
	// booleans
	//-------------------------------------------------------------------
	// gamma writes.
	m_info.m_hasGammaWrites = true;
	if ( vers < 0x000A0600 )				// pre 10.6.0, no SRGB write - see http://developer.apple.com/graphicsimaging/opengl/capabilities/GLInfo_1058.html
	{
		m_info.m_hasGammaWrites = false;
	}
	
	if (m_info.m_atiR5xx)
	{
		m_info.m_hasGammaWrites = false;	// it just don't, even post 10.6.3
	}
	
	// if CLI option for fake SRGB mode is enabled, turn off this cap, act like we do not have EXT FB SRGB
	if (CommandLine()->FindParm("-glmenablefakesrgb"))
	{
		m_info.m_hasGammaWrites = false;
	}
	
	// extension string *could* be checked, but on 10.6.3 the ext string is not there, but the func *is*
	//-------------------------------------------------------------------
	// mixed attach sizes for FBO
	m_info.m_hasMixedAttachmentSizes = true;
	if ( vers < 0x000A0603 )	// pre 10.6.3, no mixed attach sizes
	{
		m_info.m_hasMixedAttachmentSizes = false;
	}
	else
	{
		if ( !strstr( gl_ext_string, "GL_ARB_framebuffer_object" ) )
		{
			// ARB_framebuffer_object not available
			m_info.m_hasMixedAttachmentSizes = false;
		}
	}
	// also check ext string
	

	//-------------------------------------------------------------------
	// BGRA vert attribs
	m_info.m_hasBGRA = true;
	if ( vers < 0x000A0603 )	// pre 10.6.3, no BGRA attribs
	{
		m_info.m_hasBGRA = false;
	}
	else
	{
		if ( !strstr( gl_ext_string, "EXT_vertex_array_bgra" ) )
		{
			// EXT_vertex_array_bgra not available
			m_info.m_hasBGRA = false;
		}
	}
	//-------------------------------------------------------------------
	m_info.m_hasNewFullscreenMode = true;
	if ( vers < 0x000A0600 )	// pre 10.6.0, no clever window server full screen mode
	{
		m_info.m_hasNewFullscreenMode = false;
	}
	
	//-------------------------------------------------------------------
	m_info.m_hasNativeClipVertexMode = true;
	// this one uses a heuristic, and allows overrides in case the heuristic is wrong
	// or someone wants to try a beta driver or something.
	// known bad combinations get turned off here..
	
	// any ATI hardware...
	// TURNED OFF OS CHECK if (m_info.m_osComboVersion <= 0x000A0603)
	// still believe to be broken in 10.6.4
	{
		if (m_info.m_ati)
		{
			m_info.m_hasNativeClipVertexMode = false;
		}
	}
	
	// R500, forever..
	if (m_info.m_atiR5xx)
	{
		m_info.m_hasNativeClipVertexMode = false;
	}
	// if user disabled them
	if (CommandLine()->FindParm("-glmdisableclipplanes"))
	{
		m_info.m_hasNativeClipVertexMode = false;
	}
	
	// or maybe enabled them..
	if (CommandLine()->FindParm("-glmenableclipplanes"))
	{
		m_info.m_hasNativeClipVertexMode = true;
	}
	
	//-------------------------------------------------------------------
	m_info.m_hasOcclusionQuery = true;
	if (!strstr(gl_ext_string, "ARB_occlusion_query"))
	{
		m_info.m_hasOcclusionQuery = false;		// you don't got it!
	}
	
	//-------------------------------------------------------------------
	m_info.m_hasFramebufferBlit = true;
	if (!strstr(gl_ext_string, "EXT_framebuffer_blit"))
	{
		m_info.m_hasFramebufferBlit = false;	// you know you don't got it!
	}
	
	//-------------------------------------------------------------------
	glGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &m_info.m_maxAniso );
	
	//-------------------------------------------------------------------
	m_info.m_hasBindableUniforms = true;
	if (!strstr(gl_ext_string, "EXT_bindable_uniform"))
	{
		m_info.m_hasBindableUniforms = false;
	}
	m_info.m_hasBindableUniforms = false;		// hardwiring this path to false until we see how to accelerate it properly
	
	//-------------------------------------------------------------------
	m_info.m_hasUniformBuffers = true;
	if (!strstr(gl_ext_string, "ARB_uniform_buffer"))
	{
		m_info.m_hasUniformBuffers = false;
	}
	//-------------------------------------------------------------------
	// test for performance pack (10.6.4+)
	bool perfPackageDetected = GLMDetectSLGU();
	
	if (perfPackageDetected)
	{
		m_info.m_hasPerfPackage1 = true;
	}	
	if (CommandLine()->FindParm("-glmenableperfpackage"))	// force it on
	{
		m_info.m_hasPerfPackage1 = true;
	}
	
	if (CommandLine()->FindParm("-glmdisableperfpackage"))	// force it off
	{
		m_info.m_hasPerfPackage1 = false;
	}

	//-------------------------------------------------------------------
	// runtime options that aren't negotiable once set
	m_info.m_hasDualShaders = CommandLine()->FindParm("-glmdualshaders");
	//-------------------------------------------------------------------
	// "can'ts "
	
	m_info.m_cantBlitReliably = (m_info.m_osComboVersion < 0x000A0606) && m_info.m_intel;		//don't trust FBO blit on Intel before 10.6.6
	if (CommandLine()->FindParm("-glmenabletrustblit"))
	{
		m_info.m_cantBlitReliably = false;			// we trust the blit, so set the cant-blit cap to false
	}
	if (CommandLine()->FindParm("-glmdisabletrustblit"))
	{
		m_info.m_cantBlitReliably = true;			// we do not trust the blit, so set the cant-blit cap to true
	}
	//m_info.m_cantAttachSRGB = (m_info.m_nv && m_info.m_osComboVersion < 0x000A0600);	//NV drivers won't accept SRGB tex on an FBO color target in 10.5.8
	//m_info.m_cantAttachSRGB = (m_info.m_ati && m_info.m_osComboVersion < 0x000A0600);	//... does ATI have the same problem?
	m_info.m_cantAttachSRGB = (m_info.m_osComboVersion < 0x000A0600);	// across the board on 10.5.x actually..
	// MSAA resolve issues
	m_info.m_cantResolveFlipped	= false;	// initial stance
	
	if (m_info.m_ati)
	{
		//Jan 2011 - ATI says it's better to do two step blit than to try and resolve upside down
		m_info.m_cantResolveFlipped = true;
	}
	
	if (m_info.m_nv)
	{
		// we're going to mark it 'broken' unless perf package 1 (10.6.4+) is present
		if (!m_info.m_hasPerfPackage1)
		{
			m_info.m_cantResolveFlipped = true;
		}
	}
	
	// this is just the private assessment of whather scaled resolve is available.
	// the activation of it will stay tied to the gl_minify_resolve_mode / gl_magnify_resolve_mode convars in glmgr
	bool scaledResolveDetected = nsglCtx && GLMDetectScaledResolveMode( m_info.m_osComboVersion, m_info.m_hasPerfPackage1 );
	m_info.m_cantResolveScaled = !scaledResolveDetected;

	// and you can force it to be "available" if you really want to..
	if ( CommandLine()->FindParm("-gl_force_enable_scaled_resolve") )
	{
		m_info.m_cantResolveScaled = false;
	}
	
	if ( CommandLine()->FindParm("-gl_force_disable_scaled_resolve") )
	{
		m_info.m_cantResolveScaled = true;
	}
	
	// gamma decode impacting shader codegen
	m_info.m_costlyGammaFlips = false;
	if (m_info.m_osComboVersion < 0x000A0600)		// if Leopard
		m_info.m_costlyGammaFlips = true;
		
	if (m_info.m_atiR5xx)							// or r5xx - always
		m_info.m_costlyGammaFlips = true;
	if ( (m_info.m_atiR6xx) && (m_info.m_osComboVersion < 0x000A0605) )	// or r6xx prior to 10.6.5
		m_info.m_costlyGammaFlips = true;
	[nsglCtx release];
	[pixFmt release];
	
	[tempPool release];
}
GLMRendererInfo::~GLMRendererInfo( void )
{
	if (m_displays)
	{
		// delete all the new'd renderer infos that the table tracks
		FOR_EACH_VEC( *m_displays, i )
		{
			delete (*this->m_displays)[i];
		}
		delete m_displays;
		m_displays = NULL;
	}
}
extern "C" int DisplayInfoSortFunction( GLMDisplayInfo* const *A, GLMDisplayInfo* const *B )
{
	int bigger = -1;
	int smaller = 1;	// adjust these to get the ordering you want

	// check main-ness - main should win
	int mainDisplayID = CGMainDisplayID();
	int mainscreena = (*A)->m_info.m_cgDisplayID == mainDisplayID;
	int mainscreenb = (*B)->m_info.m_cgDisplayID == mainDisplayID;
	if ( mainscreena > mainscreenb )
	{
		return bigger;
	}
	else if ( mainscreena < mainscreenb )
	{
		return smaller;
	}
	
	// check area - larger screen should win
	int areaa = (*A)->m_info.m_displayPixelWidth * (*A)->m_info.m_displayPixelHeight;
	int areab = (*B)->m_info.m_displayPixelWidth * (*B)->m_info.m_displayPixelHeight;
	if ( areaa > areab )
	{	
		return bigger;
	}
	else if ( areaa < areab )
	{
		return smaller;
	}
	
	return 0;	// equal rank
}

void	GLMRendererInfo::PopulateDisplays( void )
{
	Assert( !m_displays );
	m_displays = new CUtlVector< GLMDisplayInfo* >;
	
	for( int i=0; i<32; i++)
	{
		// check mask to see if the selected display intersects this renderer
		CGOpenGLDisplayMask dspMask = (CGOpenGLDisplayMask)(1<<i);
		
		if ( m_info.m_displayMask & dspMask )
		{
			// exclude teeny displays (they may represent offline displays)
			// exclude inactive displays
			
			CGDirectDisplayID cgid = CGOpenGLDisplayMaskToDisplayID ( dspMask );
			if ( (cgid != kCGNullDirectDisplay) && CGDisplayIsActive( cgid ) && (CGDisplayPixelsWide( cgid ) >= 512) && (CGDisplayPixelsHigh( cgid ) >= 384) )
			{
				GLMDisplayInfo *newdisp = new GLMDisplayInfo( cgid, dspMask );
				m_displays->AddToTail( newdisp );			
			}
		}
	}
	
	// now sort the table of displays.
	m_displays->Sort( DisplayInfoSortFunction );
	// then go back and ask each display to populate its display mode table.
	FOR_EACH_VEC( *m_displays, i )
	{
		(*this->m_displays)[i]->PopulateModes();
	}
}
const char *CheesyRendererDecode( uint value )
{
	switch(value)
	{
		case 0x00020200 :  return "Generic";
		case 0x00020400 :  return "GenericFloat";
		case 0x00020600 :  return "AppleSW";
		case 0x00021000 :  return "ATIRage128";
		case 0x00021200 :  return "ATIRadeon";
		case 0x00021400 :  return "ATIRagePro";
		case 0x00021600 :  return "ATIRadeon8500";
		case 0x00021800 :  return "ATIRadeon9700";
		case 0x00021900 :  return "ATIRadeonX1000";
		case 0x00021A00 :  return "ATIRadeonX2000";
		case 0x00022000 :  return "NVGeForce2MX";
		case 0x00022200 :  return "NVGeForce3";
		case 0x00022400 :  return "NVGeForceFX";
		case 0x00022600 :  return "NVGeForce8xxx";
		case 0x00023000 :  return "VTBladeXP2";
		case 0x00024000 :  return "Intel900";
		case 0x00024200 :  return "IntelX3100";
		case 0x00040000 :  return "Mesa3DFX";
		default: return "UNKNOWN";
	}
}
extern const char *GLMDecode( GLMThing_t thingtype, unsigned long value );
void	GLMRendererInfo::Dump( int which )
{
	GLMPRINTF(("\n     #%d: GLMRendererInfo @ %08x, renderer-id=%s(%08x)  display-mask=%08x  vram=%dMB",
		which, this,
		CheesyRendererDecode( m_info.m_rendererID & 0x00FFFF00 ), m_info.m_rendererID,
		m_info.m_displayMask,
		m_info.m_vidMemory >> 20
	));
	GLMPRINTF(("\n       VendorID=%04x  DeviceID=%04x  Model=%s",
		m_info.m_pciVendorID,
		m_info.m_pciDeviceID,
		m_info.m_pciModelString
	));
	FOR_EACH_VEC( *m_displays, i )
	{
		(*m_displays)[i]->Dump(i);
	}
}

//===============================================================================

GLMDisplayDB::GLMDisplayDB	( void )
{
	m_renderers = NULL;	
}
GLMDisplayDB::~GLMDisplayDB	( void )
{
	if (m_renderers)
	{
		// delete all the new'd renderer infos that the table tracks
		FOR_EACH_VEC( *m_renderers, i )
		{
			delete (*this->m_renderers)[i];
		}
		delete m_renderers;
		m_renderers = NULL;
	}
}
extern "C" int RendererInfoSortFunction( GLMRendererInfo * const *A, GLMRendererInfo* const *B )
{
	int bigger = -1;
	int smaller = 1;
	
	// check VRAM
	if ( (*A)->m_info.m_vidMemory > (*B)->m_info.m_vidMemory )
	{	
		return bigger;
	}
	else if ( (*A)->m_info.m_vidMemory < (*B)->m_info.m_vidMemory )
	{
		return smaller;
	}
	
	// check MSAA limit
	if ( (*A)->m_info.m_maxSamples > (*B)->m_info.m_maxSamples )
	{	
		return bigger;
	}
	else if ( (*A)->m_info.m_maxSamples < (*B)->m_info.m_maxSamples )
	{
		return smaller;
	}

	// prefer discrete devices over Intel integrated
	if ( !(*A)->m_info.m_intel && (*B)->m_info.m_intel )
	{
		return bigger;
	}
	else if ( (*A)->m_info.m_intel && !(*B)->m_info.m_intel )
	{
		return smaller;
	}
	
	/*
		// this was not a great idea here..
		
		// check if one has the main screen - is that index 0 in all cases?
		uint maskOfMainDisplay = CGDisplayIDToOpenGLDisplayMask( CGMainDisplayID() );
		Assert( maskOfMainDisplay==1 );	// just curious
		
		int mainscreena = (*A)->m_info.m_displayMask & maskOfMainDisplay;
		int mainscreenb = (*B)->m_info.m_displayMask & maskOfMainDisplay;
		
		if ( mainscreena > mainscreenb )
		{
			return bigger;
		}
		else if ( mainscreena < mainscreenb )
		{
			return smaller;
		}
	*/
	
	return 0;	// equal rank
}
/** some code that NV gave us.  more generalized approach below..
		static io_registry_entry_t lookup_dev_NV(char *name)
		{
			mach_port_t master_port = 0;
			io_iterator_t iterator;
			io_registry_entry_t nub = 0;
			kern_return_t ret;
			IOMasterPort(MACH_PORT_NULL, &master_port);
			ret = IOServiceGetMatchingServices(master_port, IOServiceMatching(name), &iterator);
			if (iterator) {
				nub = IOIteratorNext(iterator);
				if (IOIteratorNext(iterator)) {
					printf("warning: more than one card?\n");
				}
				IOObjectRelease(iterator);
			}
			IOObjectRelease(master_port);
			return nub;
		}

		void	GetDriverInfoString_NV( char *driverNameBuf, int driverNameBufLen )
		{
			// courtesy NVIDIA dev rel
			
			io_registry_entry_t registry;
			kern_return_t ret;
			//
			// Get NVKernel / IOGLBundleName
			//
			registry = lookup_dev_NV("NVKernel");
			if (!registry) {
				fprintf(stderr, "error: could not find NVKernel IORegistry entry!\n");
				return;
			}
			CFMutableDictionaryRef entry;
			ret = IORegistryEntryCreateCFProperties(registry, &entry, kCFAllocatorDefault, 0);
			if (ret != kIOReturnSuccess) {
				fprintf(stderr, "error: could not create CFProperties dictionary!\n");
				return;
			}
			CFStringRef bundle_name_ref = (CFStringRef) CFDictionaryGetValue(entry, CFSTR("IOGLBundleName"));
			if (!bundle_name_ref) {
				fprintf(stderr, "error: could not get IOGLBundleName reference!\n");
				return;
			}
			const char *bundle_name = CFStringGetCStringPtr(bundle_name_ref, CFStringGetSystemEncoding());
			if (!bundle_name) {
				fprintf(stderr, "error: could not get IOGLBundleName!\n");
				return;
			}
			CFStringRef identifier = CFStringCreateWithFormat(NULL, NULL, CFSTR("com.apple.%s"), bundle_name);
			//
			// Get bundle information
			//
			CFBundleRef bundle;
			bundle = CFBundleGetBundleWithIdentifier(identifier);
			if (!bundle) {
				fprintf(stderr, "error: could not get GL driver bundle!\n");
				return;
			}
			CFDictionaryRef dict;
			CFStringRef info;
			dict = CFBundleGetInfoDictionary(bundle);
			if (!dict) {
				fprintf(stderr, "error: could not get bundle info dictionary!\n");
				return;
			}
			info = (CFStringRef) CFDictionaryGetValue(dict, CFSTR("CFBundleGetInfoString"));
			if (!info) {
				fprintf(stderr, "error: could not get CFBundleGetInfoString!\n");
				return;
			}
			CFStringGetCString(info, driverNameBuf, driverNameBufLen, CFStringGetSystemEncoding());
			IOObjectRelease(registry);
		}
**/
void	GLMDisplayDB::PopulateRenderers( void )
{
	Assert( !m_renderers );
	m_renderers = new CUtlVector< GLMRendererInfo* >;
	
	// now walk the renderer list
	// find the eligible ones and insert them into vector
	// if more than one, sort the vector by desirability with favorite at 0
	// then ask each renderer object to populate its displays
	// turns out how you have to do this is to walk the display mask 1<<n..
	// and query at each one, what renderers can hit that one.
	
	// when you find one, see if it's already in the vector above. if not, add it.
	// later, we sort them.
	
	for( int i=0; i<32; i++ )
	{
		CGLError			cgl_err		= (CGLError)0;
		CGLRendererInfoObj	cgl_rend	= NULL;
		GLint				nrend;
	
		CGOpenGLDisplayMask	dspMask		= (CGOpenGLDisplayMask)(1<<i);	
		CGDirectDisplayID	cgid		= CGOpenGLDisplayMaskToDisplayID( dspMask );
		bool selected = true;	// assume the best		
		if (selected)
		{
			if ( (cgid == kCGNullDirectDisplay)  || (!CGDisplayIsActive( cgid )) )
			{
				selected = false;
			}
		}
		if (selected)
		{
			cgl_err = CGLQueryRendererInfo( dspMask, &cgl_rend, &nrend );	// FIXME this call spams the console if you ask about an out of bounds display mask
																			// "<Error>: unknown error code: invalid display"
																			// we can fix that by getting the active display mask first.
			if (!cgl_err)
			{
				// walk the renderers that can hit this display
				// add to table if not already in table, and minimums met
				for( int j=0; j<nrend; j++)
				{
					int problems = 0;
					
					GLMRendererInfoFields	fields;
					memset( &fields, 0, sizeof(fields) );
					// early out if renderer ID already in the table
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPRendererID, &fields.m_rendererID );			problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPDisplayMask, &fields.m_displayMask );		problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPFullScreen, &fields.m_fullscreen );			problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPAccelerated, &fields.m_accelerated );		problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPWindow, &fields.m_windowed );				problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPBufferModes, &fields.m_bufferModes );		problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPColorModes, &fields.m_colorModes );			problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPDepthModes, &fields.m_depthModes );			problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPStencilModes, &fields.m_stencilModes );		problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPMaxAuxBuffers, &fields.m_maxAuxBuffers );	problems += (cgl_err != 0);				
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPMaxSampleBuffers, &fields.m_maxSampleBuffers );	problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPMaxSamples, &fields.m_maxSamples );			problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPSampleModes, &fields.m_sampleModes );		problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPSampleAlpha, &fields.m_sampleAlpha );		problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPVideoMemory, &fields.m_vidMemory );			problems += (cgl_err != 0);
					cgl_err = CGLDescribeRenderer( cgl_rend, j,   kCGLRPTextureMemory, &fields.m_texMemory );		problems += (cgl_err != 0);
					// decide if this renderer goes in the table.
					// only insert renderers with at least one active display.
					
					bool	selected = !problems;
					
					if (selected)
					{
						// grab the OS version
						SInt32 vMajor = 0,
						       vMinor = 0,
						       vMinorMinor = 0;
						
						OSStatus gestalt_err = 0;
						gestalt_err = Gestalt(gestaltSystemVersionMajor, &vMajor);
						Assert(!gestalt_err);
						
						gestalt_err = Gestalt(gestaltSystemVersionMinor, &vMinor);
						Assert(!gestalt_err);
						gestalt_err = Gestalt(gestaltSystemVersionBugFix, &vMinorMinor);
						Assert(!gestalt_err);
						//encode into one quantity - 10.6.3 becomes 0x000A0603
						fields.m_osComboVersion = (vMajor << 16) | (vMinor << 8) | (vMinorMinor);
						if (CommandLine()->FindParm("-fakeleopard"))
						{
							// lie
							fields.m_osComboVersion = 0x000A0508;
						}
						
						if (fields.m_osComboVersion < 0x000A0508)
						{
							// no support below 10.5.8
							// we'll wind up with no valid renderers and give up
							selected = false;
						}
					}
					
					if (selected)
					{
						// gather more info from IOKit
						// cribbed from http://developer.apple.com/mac/library/samplecode/VideoHardwareInfo/listing3.html
						
						CFTypeRef typeCode;
						CFDataRef vendorID, deviceID, model;
						io_registry_entry_t dspPort;
							
						// Get the I/O Kit service port for the display
						dspPort = CGDisplayIOServicePort( cgid );
						// Get the information for the device
						// The vendor ID, device ID, and model are all available as properties of the hardware's I/O Kit service port
						
						vendorID	= (CFDataRef)IORegistryEntrySearchCFProperty(dspPort,kIOServicePlane,CFSTR("vendor-id"),	kCFAllocatorDefault,kIORegistryIterateRecursively | kIORegistryIterateParents);
						deviceID	= (CFDataRef)IORegistryEntrySearchCFProperty(dspPort,kIOServicePlane,CFSTR("device-id"),	kCFAllocatorDefault,kIORegistryIterateRecursively | kIORegistryIterateParents);
						model		= (CFDataRef)IORegistryEntrySearchCFProperty(dspPort,kIOServicePlane,CFSTR("model"),		kCFAllocatorDefault,kIORegistryIterateRecursively | kIORegistryIterateParents);
						
						// Send the appropriate data to the outputs checking to validate the data
						if(vendorID)
						{
							fields.m_pciVendorID = *((UInt32*)CFDataGetBytePtr(vendorID));
						}
						else
						{
							fields.m_pciVendorID = 0;
						}
						
						if(deviceID)
						{
							fields.m_pciDeviceID = *((UInt32*)CFDataGetBytePtr(deviceID));
						}
						else
						{
							fields.m_pciDeviceID = 0;
						}
						
						if(model)
						{
							int length = CFDataGetLength(model);
							char *data = (char*)CFDataGetBytePtr(model);
							Q_strncpy( fields.m_pciModelString, data, sizeof(fields.m_pciModelString) );
						}
						else
						{
							Q_strncpy( fields.m_pciModelString, "UnknownModel", sizeof(fields.m_pciModelString) );
						}
						
						// iterate through IOAccelerators til we find one that matches the vendorid and deviceid of this renderer (ugh!)
						// this provides the driver version string which can in turn be used to uniquely identify bad drivers and special case for them
						// first example to date - forcing vsync on 10.6.4 + NV
						
						{
							io_iterator_t	ioIterator		= (io_iterator_t)0;
							io_service_t	ioAccelerator;
							kern_return_t	ioResult		= 0;
							bool			ioDone			= false;
														
							ioResult = IOServiceGetMatchingServices( kIOMasterPortDefault, IOServiceMatching("IOAccelerator"), &ioIterator );
							if( ioResult == KERN_SUCCESS )
							{
								ioAccelerator = 0;
								while( ( !ioDone ) && ( ioAccelerator = IOIteratorNext( ioIterator ) )  )
								{
									io_service_t ioDevice;
									
									ioDevice = 0;
									ioResult = IORegistryEntryGetParentEntry( ioAccelerator, kIOServicePlane, &ioDevice);
									
									CFDataRef this_vendorID, this_deviceID;
									if(ioResult == KERN_SUCCESS)
									{
										this_vendorID	=	(CFDataRef)IORegistryEntryCreateCFProperty(ioDevice, CFSTR("vendor-id"), kCFAllocatorDefault, kNilOptions );
										this_deviceID	=	(CFDataRef)IORegistryEntryCreateCFProperty(ioDevice, CFSTR("device-id"), kCFAllocatorDefault, kNilOptions );
										
										if (this_vendorID && this_deviceID)	// null check..
										{
											// see if it matches. if so, do our business (get the extended version string), set ioDone, call it a day
											unsigned short this_vendorIDValue = *(unsigned short*)CFDataGetBytePtr(this_vendorID);
											unsigned short this_deviceIDValue = *(unsigned short*)CFDataGetBytePtr(this_deviceID);
											
											if ( (fields.m_pciVendorID == this_vendorIDValue) && (fields.m_pciDeviceID == this_deviceIDValue) )
											{
												// see if it matches. if so, do our business (get the extended version string), set ioDone, call it a day
												unsigned short* this_vendorIDBytes = (unsigned short*)CFDataGetBytePtr( this_vendorID );
												unsigned short* this_deviceIDBytes = (unsigned short*)CFDataGetBytePtr( this_deviceID );
												
												if (this_vendorIDBytes && this_deviceIDBytes)	// null check...
												{
													unsigned short this_vendorIDValue = *this_vendorIDBytes;
													unsigned short this_deviceIDValue = *this_deviceIDBytes;
													
													if ( (fields.m_pciVendorID == this_vendorIDValue) && (fields.m_pciDeviceID == this_deviceIDValue) )
													{
														// match, stop looking
														ioDone = true;
														
														// get extended info
														CFStringRef this_ioglName = (CFStringRef)IORegistryEntryCreateCFProperty( ioAccelerator, CFSTR("IOGLBundleName"), kCFAllocatorDefault, kNilOptions );
	
														NSString *bundlePath = [ NSString stringWithFormat:@"/System/Library/Extensions/%@.bundle", this_ioglName ];
														
														NSDictionary* this_driverDict = [ [NSBundle bundleWithPath: bundlePath] infoDictionary ];
														if (this_driverDict)
														{
															NSString* this_driverInfo = [ this_driverDict objectForKey:@"CFBundleGetInfoString" ];
															if ( this_driverInfo )
															{
																const char* theString = [ this_driverInfo UTF8String ];
																
																strncpy(fields.m_driverInfoString, theString, sizeof( fields.m_driverInfoString )  );
															}												
														}
														
														// [bundlePath release];
														
														CFRelease(this_ioglName);
													}
												}
	
												CFRelease(this_vendorID);
												CFRelease(this_deviceID);
											}
										}
									}
								}
							}
							IOObjectRelease(ioAccelerator);
							IOObjectRelease(ioIterator);
						}
						// Release vendorID, deviceID, and model as appropriate
						if(vendorID)
							CFRelease(vendorID);
						if(deviceID)
							CFRelease(deviceID);
						if(model)
							CFRelease(model);
						// generate shorthand bools
						switch( fields.m_pciVendorID )
						{
							case	0x1002:	//ATI
							{
								fields.m_ati = true;
								// http://www.pcidatabase.com/search.php?device_search_str=radeon&device_search.x=0&device_search.y=0&device_search=search+devices
								
								// Mac-relevant ATI R5xx PCI device ID's lie in this range: 0x7100 - 0x72FF
								// X1600, X1900, X1950
								if ( (fields.m_pciDeviceID >= 0x7100) && (fields.m_pciDeviceID <= 0x72ff) )
								{
									fields.m_atiR5xx = true;
								}
								// R6xx PCI device ID's lie in these ranges:
									// 0x94C1 - 0x9515 ... also 0x9581 - 0x9713
									// 2400HD, 2600HD, 3870, et al
								if	( 
										( (fields.m_pciDeviceID >= 0x94C1) && (fields.m_pciDeviceID <= 0x9515) )
									||	( (fields.m_pciDeviceID >= 0x9581) && (fields.m_pciDeviceID <= 0x9713) )
									)
								{
									fields.m_atiR6xx = true;
								}
								// R7xx PCI device ID's lie in: 0x9440 - 0x9460, also 9480-94b5.
								// why there is an HD5000 at 9462, I dunno.  Don't think that's an R8xx part.
								if	( 
										( (fields.m_pciDeviceID >= 0x9440) && (fields.m_pciDeviceID <= 0x9460) )
									||	( (fields.m_pciDeviceID >= 0x9480) && (fields.m_pciDeviceID <= 0x94B5) )
									)
								{
									fields.m_atiR7xx = true;
								}
								
								// R8xx: 0x6898-0x68BE
								if ( (fields.m_pciDeviceID >= 0x6898) && (fields.m_pciDeviceID <= 0x68Be) )
								{
									fields.m_atiR8xx = true;
								}
								#if 0
										// turned off, but we could use this for cross check.
										// we could also use the bit encoding of the renderer ID to ferret out a geberation clue.
										
										// string-scan for each generation
										// this could be a lot better if we got the precise PCI ID's used and/or cross-ref'd that against the driver name
										if (strstr("X1600", fields.m_pciModelString) || strstr("X1900", fields.m_pciModelString) || strstr("X1950", fields.m_pciModelString) )
										{
											fields.m_atiR5xx = true;
										}
										if (strstr("2600", fields.m_pciModelString) || strstr("3870", fields.m_pciModelString) || strstr("X2000", fields.m_pciModelString) )
										{
											fields.m_atiR6xx = true;
										}
										if (strstr("4670", fields.m_pciModelString) || strstr("4650", fields.m_pciModelString) || strstr("4850", fields.m_pciModelString)|| strstr("4870", fields.m_pciModelString) )
										{
											fields.m_atiR7xx = true;
										}
								#endif
							}
							break;
							
							case	0x8086:	//INTC
							{
								fields.m_intel = true;
								
								switch( fields.m_pciDeviceID )
								{
									case	0x27A6:	fields.m_intel95x = true;	break;	// GMA 950
									case	0x2A02:	fields.m_intel3100 = true;	break;	// X3100
									
									default:
									{
										if (fields.m_pciDeviceID > 0x2A02)		// assume ascending ID's for newer devices
										{
											fields.m_intelHD4000 = true;
										}
									}
								}
							}
							break;
							
							case	0x10DE:	//NV
							{
								fields.m_nv = true;
								// G7x: 0x0391 0x393 0x0395 (7300/7600 GT)  0x009D (Quadro FX)
								if	( (fields.m_pciDeviceID == 0x0391) || (fields.m_pciDeviceID == 0x0393) || (fields.m_pciDeviceID == 0x0395) || (fields.m_pciDeviceID == 0x009D) )
								{
									fields.m_nvG7x = true;
								}
								
								// G8x: 0400-04ff, also 0x5E1 (GTX280) through 0x08FF
								if	(
										( (fields.m_pciDeviceID >= 0x0400) && (fields.m_pciDeviceID <= 0x04ff) )
									||	( (fields.m_pciDeviceID >= 0x05E1) && (fields.m_pciDeviceID <= 0x08ff) )
									)
								{
									fields.m_nvG8x = true;
								}
								if ( fields.m_pciDeviceID > 0x0900 )
								{
									fields.m_nvNewer = true;
								}
								
								// detect the specific revision of NV driver in 10.6.4 that caused all the grief
								if (strstr(fields.m_driverInfoString, "1.6.16.11 (19.5.8f01)"))
								{
									fields.m_badDriver1064NV = true;
								}
							}
							break;
						}						
					}
					
					if (selected)
					{
						// dupe check
						FOR_EACH_VEC( *m_renderers, i )
						{
							uint rendid = (*m_renderers)[i]->m_info.m_rendererID;
							
							if ( rendid == fields.m_rendererID )
							{
								// don't add to table, it's a dupe
								selected = false;
							}
						}
					}
					
					if (selected)
					{
						// criteria check
						if (fields.m_fullscreen==0)
							selected = false;
						if (fields.m_accelerated==0)
							selected = false;
						if (fields.m_windowed==0)
							selected = false;
					}
					
					// we need something here that will exclude the renderer if it does not have any good displays attached.
					
					Assert( fields.m_displayMask != 0 );
					
					if (selected)
					{
						// add to table
						// note this constructor makes a dummy context just long enough to query remaining fields in the m_info.
						GLMRendererInfo *newinfo = new GLMRendererInfo( &fields );
						m_renderers->AddToTail( newinfo );
					}
				}
				if (cgl_rend)
				{
					CGLDestroyRendererInfo( cgl_rend );
				}
			}
		}
	}
	
	// now sort the table.
	m_renderers->Sort( RendererInfoSortFunction );
	// then go back and ask each renderer to populate its display info table.
	FOR_EACH_VEC( *m_renderers, i )
	{
		(*m_renderers)[i]->PopulateDisplays();
	}
}
void	GLMDisplayDB::PopulateFakeAdapters( uint realRendererIndex )		// fake adapters = one real adapter times however many displays are on it
{
	// presumption is that renderers have been populated.
	Assert( GetRendererCount() > 0 );
	Assert( realRendererIndex < GetRendererCount() );
	
	m_fakeAdapters.RemoveAll();
	
	// for( int r = 0; r < GetRendererCount(); r++ )
	int r = realRendererIndex;
	{
		for( int d = 0; d < GetDisplayCount( r ); d++ )
		{
			GLMFakeAdapter temp;
			
			temp.m_rendererIndex = r;
			temp.m_displayIndex = d;
			
			m_fakeAdapters.AddToTail( temp );
		}
	}
}
void	GLMDisplayDB::Populate(void)
{
	this->PopulateRenderers();
	
	// passing in zero here, constrains the set of fake adapters (GL renderer + a display) to the ones using the highest ranked renderer.
	//FIXME introduce some kind of convar allowing selection of other GPU's in the system.
	
	int realRendererIndex = 0;
	if (CommandLine()->FindParm("-glmrenderer0"))
		realRendererIndex = 0;
	if (CommandLine()->FindParm("-glmrenderer1"))
		realRendererIndex = 1;
	if (CommandLine()->FindParm("-glmrenderer2"))
		realRendererIndex = 2;
	if (CommandLine()->FindParm("-glmrenderer3"))
		realRendererIndex = 3;
		
	if (realRendererIndex >= GetRendererCount())
	{
		// fall back to 0
		realRendererIndex = 0;
	}
	
	this->PopulateFakeAdapters( 0 );
	#if GLMDEBUG
		this->Dump();
	#endif
}
	

int		GLMDisplayDB::GetFakeAdapterCount( void )
{
	return m_fakeAdapters.Count();
}
bool	GLMDisplayDB::GetFakeAdapterInfo( int fakeAdapterIndex, int *rendererOut, int *displayOut, GLMRendererInfoFields *rendererInfoOut, GLMDisplayInfoFields *displayInfoOut )
{
	if (fakeAdapterIndex >= GetFakeAdapterCount() )
	{
		*rendererOut = 0;
		*displayOut = 0;
		return true;		// fail
	}
	*rendererOut = m_fakeAdapters[fakeAdapterIndex].m_rendererIndex;
	*displayOut = m_fakeAdapters[fakeAdapterIndex].m_displayIndex;
	bool rendResult = GetRendererInfo( *rendererOut, rendererInfoOut );
	bool dispResult = GetDisplayInfo( *rendererOut, *displayOut, displayInfoOut );
	
	return rendResult || dispResult;
}
	
int		GLMDisplayDB::GetRendererCount( void )
{
	return	m_renderers->Count();
}
bool	GLMDisplayDB::GetRendererInfo( int rendererIndex, GLMRendererInfoFields *infoOut )
{
	memset( infoOut, 0, sizeof( GLMRendererInfoFields ) );
	if (rendererIndex >= GetRendererCount())
		return true; // fail
	
	GLMRendererInfo *rendInfo = (*m_renderers)[rendererIndex];		
	*infoOut = rendInfo->m_info;
	return false;
}
int		GLMDisplayDB::GetDisplayCount( int rendererIndex )
{
	if (rendererIndex >= GetRendererCount())
		return 0; // fail
	
	GLMRendererInfo *rendInfo = (*m_renderers)[rendererIndex];
		
	return	rendInfo->m_displays->Count();
}
bool	GLMDisplayDB::GetDisplayInfo( int rendererIndex, int displayIndex, GLMDisplayInfoFields *infoOut )
{
	memset( infoOut, 0, sizeof( GLMDisplayInfoFields ) );
	
	if (rendererIndex >= GetRendererCount())
		return true; // fail
	
	if (displayIndex >= GetDisplayCount(rendererIndex))
		return true; // fail
	
	GLMDisplayInfo *displayInfo = (*(*m_renderers)[rendererIndex]->m_displays)[displayIndex];
	*infoOut = displayInfo->m_info;
	return false;
}
int		GLMDisplayDB::GetModeCount( int rendererIndex, int displayIndex )
{
	if (rendererIndex >= GetRendererCount())
		return 0; // fail
	
	if (displayIndex >= GetDisplayCount(rendererIndex))
		return 0; // fail
		
	GLMDisplayInfo *displayInfo = (*(*m_renderers)[rendererIndex]->m_displays)[displayIndex];
	return displayInfo->m_modes->Count();
}
bool	GLMDisplayDB::GetModeInfo( int rendererIndex, int displayIndex, int modeIndex, GLMDisplayModeInfoFields *infoOut )
{
	memset( infoOut, 0, sizeof( GLMDisplayModeInfoFields ) );
	
	if (rendererIndex >= GetRendererCount())
		return true; // fail
	
	if (displayIndex >= GetDisplayCount(rendererIndex))
		return true; // fail
	
	if (modeIndex >= GetModeCount(rendererIndex,displayIndex))
		return true; // fail
	
	if (modeIndex>=0)
	{
		GLMDisplayMode *displayModeInfo = (*(*(*m_renderers)[rendererIndex]->m_displays)[displayIndex]->m_modes)[ modeIndex ];
		*infoOut = displayModeInfo->m_info;
	}
	else
	{
		// passing modeIndex = -1 means "tell me about current mode"..
		GLMRendererInfo		*rendInfo = (*m_renderers)[ rendererIndex ];
		GLMDisplayInfo		*dispinfo = (*rendInfo ->m_displays)[displayIndex];	
		CGDirectDisplayID	cgid = dispinfo->m_info.m_cgDisplayID;
		
		CFDictionaryRef		curModeDict = CGDisplayCurrentMode( cgid );
		CFNumberRef			number;
		CFBooleanRef		boolean;
		CFArrayRef			modeList;
		CGDisplayErr		cgderr;
		
		// get the mode number from the mode dict (using system mode numbering, not our sorted numbering)
		if (curModeDict)
		{
			int modeIndex=0;
			number = (CFNumberRef)CFDictionaryGetValue(curModeDict, kCGDisplayMode);
			CFNumberGetValue(number, kCFNumberLongType, &modeIndex);
			// grab the width and height, I am unclear on whether this is the displayed FB width or the display device width.
			int screenWidth=0;
			int screenHeight=0;
			int refreshHz=0;
			
			number = (CFNumberRef)CFDictionaryGetValue(curModeDict, kCGDisplayWidth);
			CFNumberGetValue(number, kCFNumberIntType, &screenWidth);
			number = (CFNumberRef)CFDictionaryGetValue(curModeDict, kCGDisplayHeight);
			CFNumberGetValue(number, kCFNumberIntType, &screenHeight);
			number = (CFNumberRef)CFDictionaryGetValue(curModeDict, kCGDisplayRefreshRate);
			CFNumberGetValue(number, kCFNumberIntType, &refreshHz);
			
			GLMPRINTF(( "-D- GLMDisplayDB::GetModeInfo sees mode-index=%d, width=%d, height=%d on CGID %08x (display index %d on rendererindex %d)", 
				modeIndex,
				screenWidth,
				screenHeight,
				cgid,
				displayIndex,
				rendererIndex ));
			// now match
			int foundIndex = -1;
			FOR_EACH_VEC( (*dispinfo->m_modes), i )
			{
				GLMDisplayMode *mode = (*dispinfo->m_modes)[i];
				
				if (mode->m_info.m_modePixelWidth == screenWidth)
				{
					if (mode->m_info.m_modePixelHeight == screenHeight)
					{
						if (mode->m_info.m_modeRefreshHz == refreshHz)
						{
							foundIndex = i;
							*infoOut = mode->m_info;
							return false;
						}
					}
				}
			}
		}
		// if we get here, we could not find the mode
		memset( infoOut, 0, sizeof( *infoOut ) );
		return true; // fail
	}
	return false;
}

void	GLMDisplayDB::Dump( void )
{
	GLMPRINTF(("\n GLMDisplayDB @ %08x ",this ));
	FOR_EACH_VEC( *m_renderers, i )
	{
		(*m_renderers)[i]->Dump(i);
	}
}


