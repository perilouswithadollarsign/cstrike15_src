 //================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
//
//
//==================================================================================================

#ifndef ICOCOAMGR_H
#define ICOCOAMGR_H
#ifdef _WIN32
#pragma once
#endif


#include "tier0/threadtools.h"
#include "appframework/IAppSystem.h"
#include "glmgr/glmgr.h"

// if you rev this version also update materialsystem/cmaterialsystem.cpp CMaterialSystem::Connect as it defines the string directly
#define  COCOAMGR_INTERFACE_VERSION "CocoaMgrInterface006"


enum CocoaEventType_t
{
	CocoaEvent_KeyDown,
	CocoaEvent_KeyUp,
	CocoaEvent_MouseButtonDown,
	CocoaEvent_MouseMove,
	CocoaEvent_MouseButtonUp,
	CocoaEvent_AppActivate,
	CocoaEvent_MouseScroll,
	CocoaEvent_AppQuit
};

// enum values need to match bit-shifting logic in CInputSystem::UpdateMouseButtonState and 
// the codes from NSEvent pressedMouseButtons, turns out the two are in agreement right now
enum CocoaMouseButton_t
{
	COCOABUTTON_LEFT = 1 << 0,
	COCOABUTTON_RIGHT = 1 << 1,
	COCOABUTTON_MIDDLE = 1 << 2,
	COCOABUTTON_4 = 1 << 3,	
	COCOABUTTON_5 = 1 << 4,	
};

enum ECocoaKeyModifier
{
	eCapsLockKey,
	eShiftKey,
	eControlKey,
	eAltKey,		// aka option
	eCommandKey
};

class CCocoaEvent
{
public:
	CocoaEventType_t m_EventType;
	int m_VirtualKeyCode;
	wchar_t m_UnicodeKey;
	wchar_t m_UnicodeKeyUnmodified;
	uint m_ModifierKeyMask;		// 
	int m_MousePos[2];
	int m_MouseButtonFlags;	// Current state of the mouse buttons. See COCOABUTTON_xxxx.
	uint m_nMouseClickCount;
	int m_MouseButton; // which of the CocoaMouseButton_t buttons this is for from above
};

class CShowPixelsParams
{
public:
	GLuint					m_srcTexName;
	int						m_width,m_height;
	bool					m_vsyncEnable;
	bool					m_fsEnable;		// want receiving view to be full screen.  for now, just target the main screen.  extend later.
	bool					m_useBlit;		// use FBO blit - sending context says it is available.
	bool					m_noBlit;		// the back buffer has already been populated by the caller (perhaps via direct MSAA resolve from multisampled RT tex)
	bool					m_onlySyncView;	// react to full/windowed state change only, do not present bits
};

#define	kMaxCrawlFrames	100
#define	kMaxCrawlText		(kMaxCrawlFrames * 256)
class CStackCrawlParams
{
	public:
	uint					m_frameLimit;							// input: max frames to retrieve
	uint					m_frameCount;							// output: frames found
	void					*m_crawl[kMaxCrawlFrames];				// call site addresses
	char					*m_crawlNames[kMaxCrawlFrames];			// pointers into text following, one per decoded name
	char					m_crawlText[kMaxCrawlText];
};

struct GLMRendererInfoFields;
class GLMDisplayDB;

class ICocoaMgr : public IAppSystem
{
public:
	virtual bool Connect( CreateInterfaceFn factory ) = 0;
	virtual void Disconnect() = 0;
	
	virtual void *QueryInterface( const char *pInterfaceName ) = 0;
	
	// Init, shutdown
	virtual InitReturnVal_t Init() = 0;
	virtual void Shutdown() = 0;
	
	// Create the window.
	virtual bool CreateGameWindow( const char *pTitle, bool bWindowed, int width, int height ) = 0;
	
	// Get the NSWindow*.
//	virtual void* GetNSWindow() = 0;

	// Get the NSGLContext for a window's main view - note this is the carbon windowref as an argument
	virtual PseudoNSGLContextPtr GetNSGLContextForWindow( void* windowref ) = 0;
	
	// Get the next N events. The function returns the number of events that were filled into your array.
	virtual int GetEvents( CCocoaEvent *pEvents, int nMaxEventsToReturn, bool debugEvents = false ) = 0;

	// Set the mouse cursor position.
	virtual void SetCursorPosition( int x, int y ) = 0;
	
	virtual void *GetWindowRef() = 0;
	
	virtual void ShowPixels( CShowPixelsParams *params ) = 0;
	
	virtual void MoveWindow( int x, int y ) = 0;
	virtual void SizeWindow( int width, int tall ) = 0;
	virtual void PumpWindowsMessageLoop() = 0;
	
	virtual void GetStackCrawl( CStackCrawlParams *params ) = 0;	
	
	virtual void DestroyGameWindow() = 0;
	virtual void SetApplicationIcon( const char *pchAppIconFile ) = 0;
	
	virtual void GetMouseDelta( int &x, int &y, bool bIgnoreNextMouseDelta = false ) = 0;

	virtual void RenderedSize( uint &width, uint &height, bool set ) = 0;	// either set or retrieve rendered size value (from dxabstract)
	virtual void DisplayedSize( uint &width, uint &height ) = 0;			// query backbuffer size (window size whether FS or windowed)
	
	virtual void GetDesiredPixelFormatAttribsAndRendererInfo( uint **ptrOut, uint *countOut, GLMRendererInfoFields *rendInfoOut ) = 0;
	
	virtual GLMDisplayDB *GetDisplayDB( void ) = 0;
	
	virtual void WaitUntilUserInput( int msSleepTime ) = 0;
};




//===============================================================================

// modes, displays, and renderers
// think of renderers as being at the top of a tree.
// each renderer has displays hanging off of it.
// each display has modes hanging off of it.
// the tree is populated on demand and then queried as needed.

//===============================================================================

// GLMDisplayModeInfoFields is in glmdisplay.h

class GLMDisplayMode
{
public:
	GLMDisplayModeInfoFields	m_info;
	
	GLMDisplayMode( uint width, uint height, uint refreshHz );
	~GLMDisplayMode( void );

	void	Dump( int which );
};

//===============================================================================

// GLMDisplayInfoFields is in glmdisplay.h

class GLMDisplayInfo
{
public:
	GLMDisplayInfoFields			m_info;
	CUtlVector< GLMDisplayMode* >	*m_modes;				// starts out NULL, set by PopulateModes

	GLMDisplayInfo( CGDirectDisplayID displayID, CGOpenGLDisplayMask displayMask );
	~GLMDisplayInfo( void );
	
	void	PopulateModes( void );

	void	Dump( int which );
};

//===============================================================================

// GLMRendererInfoFields is in glmdisplay.h

class GLMRendererInfo
{
public:
	GLMRendererInfoFields			m_info;
	CUtlVector< GLMDisplayInfo* >	*m_displays;			// starts out NULL, set by PopulateDisplays

	GLMRendererInfo			( GLMRendererInfoFields *info );
	~GLMRendererInfo		( void );

	void	PopulateDisplays( void );
	void	Dump( int which );
};

//===============================================================================

// this is just a tuple describing fake adapters which are really renderer/display pairings.
// dxabstract bridges the gap between the d3d adapter-centric world and the GL renderer+display world.
// this makes it straightforward to handle cases like two video cards with two displays on one, and one on the other -
// you get three fake adapters which represent each useful screen.

// the constraint that dxa will have to follow though, is that if the user wants to change their 
// display selection for full screen, they would only be able to pick on that has the same underlying renderer.
// can't change fakeAdapter from one to another with different GL renderer under it.  Screen hop but no card hop.

struct GLMFakeAdapter
{
	int		m_rendererIndex;
	int		m_displayIndex;
};

class GLMDisplayDB
{
public:
	CUtlVector< GLMRendererInfo* >		*m_renderers;			// starts out NULL, set by PopulateRenderers

	CUtlVector< GLMFakeAdapter >		m_fakeAdapters;
	
	GLMDisplayDB	( void );
	~GLMDisplayDB	( void );	

	virtual void	PopulateRenderers( void );
	virtual void	PopulateFakeAdapters( uint realRendererIndex );		// fake adapters = one real adapter times however many displays are on it
	virtual void	Populate( void );
	
	// The info-get functions return false on success.
	virtual	int		GetFakeAdapterCount( void );
	virtual	bool	GetFakeAdapterInfo( int fakeAdapterIndex, int *rendererOut, int *displayOut, GLMRendererInfoFields *rendererInfoOut, GLMDisplayInfoFields *displayInfoOut );
	
	virtual	int		GetRendererCount( void );
	virtual	bool	GetRendererInfo( int rendererIndex, GLMRendererInfoFields *infoOut );
	
	virtual	int		GetDisplayCount( int rendererIndex );
	virtual	bool	GetDisplayInfo( int rendererIndex, int displayIndex, GLMDisplayInfoFields *infoOut );

	virtual	int		GetModeCount( int rendererIndex, int displayIndex );
	virtual	bool	GetModeInfo( int rendererIndex, int displayIndex, int modeIndex, GLMDisplayModeInfoFields *infoOut );
	
	virtual	void	Dump( void );
};



#endif // ICOCOAMGR_H



