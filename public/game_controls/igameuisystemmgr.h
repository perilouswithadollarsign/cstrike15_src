//===== Copyright © Valve Corporation, All rights reserved. ======//
#ifndef IGAMEUISYSTEMMGR_H
#define IGAMEUISYSTEMMGR_H

#ifdef _WIN32
#pragma once
#endif



// Forward declarations
class IGameUISystemMgr;
class IGameUIScreenController;
class IGameUIScreenControllerFactory;
class IGameUISystem;
class IGameUIMiscUtils;
class IGameUISystemSurface;
class IGameUISchemeMgr;
class IGameUIScheme;
class IGameUISoundPlayback;
class IMaterialProxy;

// External includes
#include "tier1/timeutils.h"
#include "tier1/utlsymbol.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui_surfacelib/ifontsurface.h" 
#include "rendersystem/irenderdevice.h"
#include "inputsystem/InputEnums.h"


// Game controls includes
#include "miscutils.h"



// Types
FORWARD_DECLARE_HANDLE( InputContextHandle_t );



//-----------------------------------------------------------------------------
// Interface used to drive gameui (tier3)
//-----------------------------------------------------------------------------
#define GAMEUISYSTEMMGR_INTERFACE_VERSION	"GameUISystemMgr001"
abstract_class IGameUISystemMgr : public IAppSystem
{
public:
	virtual void SetGameUIVisible( bool bVisible ) = 0;
	virtual bool GetGameUIVisible() = 0;

	// Load the game UI menu screen
	// key values are owned and released by caller
	virtual IGameUISystem * LoadGameUIScreen( KeyValues *kvScreenLoadSettings ) = 0;
	virtual void ReleaseAllGameUIScreens() = 0;

	virtual void SetSoundPlayback( IGameUISoundPlayback *pPlayback ) = 0;
	virtual void UseGameInputSystemEventQueue( bool bEnable ) = 0;
	virtual void SetInputContext( InputContextHandle_t hInputContext ) = 0;
	virtual void RegisterInputEvent( const InputEvent_t &iEvent ) = 0;

	virtual void RunFrame() = 0;
	virtual void Render( const Rect_t &viewport, DmeTime_t flCurrentTime ) = 0;
	virtual void Render( IRenderContext *pRenderContext, PlatWindow_t hWnd, const Rect_t &viewport, DmeTime_t flCurrentTime ) = 0;

	virtual void RegisterScreenControllerFactory( char const *szControllerName, IGameUIScreenControllerFactory *pFactory ) = 0;
	virtual IGameUIScreenControllerFactory * GetScreenControllerFactory( char const *szControllerName ) = 0;

	virtual void SendEventToAllScreens( KeyValues *kvGlobalEvent ) = 0;

	virtual IGameUISystemSurface * GetSurface() = 0;
	virtual IGameUISchemeMgr * GetSchemeMgr() = 0;
	virtual IGameUIMiscUtils * GetMiscUtils() = 0;

	// Init any render targets needed by the UI.
	virtual void InitRenderTargets() = 0;
	virtual IMaterialProxy *CreateProxy( const char *proxyName ) = 0;
};



//-----------------------------------------------------------------------------
// Used to allow clients to install code hooks to the gui screens
//-----------------------------------------------------------------------------
abstract_class IGameUIScreenController
{
public:
	// Connects a screen to the controller, returns number of
	// remaining connected screens (or 1 for the first connection)
	virtual int OnScreenConnected( IGameUISystem *pScreenView ) = 0;

	// Releases the screen from controller, returns number of
	// remaining connected screens (returns 0 if no screens are
	// connected - new object must be reacquired from factory
	// in this case)
	virtual int OnScreenDisconnected( IGameUISystem *pScreenView ) = 0;

	// Callback for screen events handling (caller retains ownership of keyvalues)
	virtual KeyValues * OnScreenEvent( IGameUISystem *pScreenView, KeyValues *kvEvent ) = 0;

	// Broadcast an event to all connected screens (caller retains ownership of keyvalues)
	virtual void BroadcastEventToScreens( KeyValues *kvEvent ) = 0;
};

abstract_class IGameUIScreenControllerFactory
{
public:
	// Returns an instance of a controller interface (keyvalues owned by caller)
	virtual IGameUIScreenController * GetController( KeyValues *kvRequest ) = 0;

	// Access controller instances
	virtual int GetControllerInstancesCount() = 0;
	virtual IGameUIScreenController * GetControllerInstance( int iIndex ) = 0;
};


//-----------------------------------------------------------------------------
// IGameUISystem represents a logical collection of UI screens
//-----------------------------------------------------------------------------
abstract_class IGameUISystem
{
public:
	virtual char const * GetName() = 0;
	virtual bool ExecuteScript( KeyValues *kvEvent, KeyValues **ppResult = NULL ) = 0;

	virtual int32 GetScriptHandle() = 0;

	virtual void SetStageSize( int nWide, int nTall ) = 0;
	virtual void GetStageSize( Vector2D &stageSize ) = 0;
};



//-----------------------------------------------------------------------------
// This class is the interface to the font and font texture, systems.
// Load fonts given by schemes into the systems using this class.
//-----------------------------------------------------------------------------
class IGameUISystemSurface
{
public:
	virtual InitReturnVal_t Init() = 0;
	virtual void Shutdown() = 0;

	virtual void PrecacheFontCharacters( FontHandle_t font, wchar_t *pCharacterString = NULL ) = 0;

	virtual FontHandle_t CreateFont() = 0;
	virtual bool SetFontGlyphSet( FontHandle_t font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin = 0, int nRangeMax = 0 ) = 0;
	virtual int GetFontTall( FontHandle_t font ) = 0;
	virtual void GetCharABCwide( FontHandle_t font, int ch, int &a, int &b, int &c ) = 0;
	virtual int GetCharacterWidth( FontHandle_t font, int ch ) = 0;
	virtual const char *GetFontName( FontHandle_t font ) = 0;

	virtual bool AddCustomFontFile( const char *fontFileName ) = 0;

	// Helper fxns for loading bitmap fonts
	virtual bool AddBitmapFontFile( const char *fontFileName ) = 0;
	virtual void SetBitmapFontName( const char *pName, const char *pFontFilename ) = 0;
	virtual const char *GetBitmapFontName( const char *pName ) = 0;
	virtual bool SetBitmapFontGlyphSet( FontHandle_t font, const char *windowsFontName, float scalex, float scaley, int flags) = 0;

	virtual void ClearTemporaryFontCache( void ) = 0;

	// Causes fonts to get reloaded, etc.
	virtual void ResetFontCaches() = 0;

	virtual bool SupportsFontFeature( FontFeature_t feature ) = 0;

	virtual bool GetUnicodeCharRenderPositions( FontCharRenderInfo& info, Vector2D *pPositions ) = 0;
 	virtual IMaterial *GetTextureForChar( FontCharRenderInfo &info, float **texCoords ) = 0;
	virtual IMaterial *GetTextureAndCoordsForChar( FontCharRenderInfo &info, float *texCoords ) = 0;

	// Used for debugging.
	virtual void DrawFontTexture( int textureId, int xPos, int yPos ) = 0;
	virtual void DrawFontTexture( IRenderContext *pRenderContext, int textureId, int xPos, int yPos ) = 0;
	
	virtual IMaterial *GetMaterial( int textureId ) = 0;
	virtual HRenderTexture GetTextureHandle( int textureId ) = 0;

	virtual void SetLanguage( const char *pLanguage ) = 0;
	virtual const char *GetLanguage() = 0;
};


//-----------------------------------------------------------------------------
// Game UI scheme manager
//-----------------------------------------------------------------------------
abstract_class IGameUISchemeMgr
{
public:
	// loads a scheme from a file
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	// tag is friendly string representing the name of the loaded scheme
	virtual IGameUIScheme * LoadSchemeFromFile( const char *fileName, const char *tag ) = 0;

	// reloads the schemes from the file
	virtual void ReloadSchemes() = 0;

	// reloads scheme fonts
	virtual void ReloadFonts( int inScreenTall = -1 ) = 0;

	// returns a handle to the default (first loaded) scheme
	virtual IGameUIScheme * GetDefaultScheme() = 0;

	// returns a handle to the scheme identified by "tag"
	virtual IGameUIScheme * GetScheme( const char *tag ) = 0;

	virtual void SetLanguage( const char *pLanguage ) = 0;
	virtual char const * GetLanguage() = 0;
};


//-----------------------------------------------------------------------------
// Game UI version of a vgui scheme
//-----------------------------------------------------------------------------
abstract_class IGameUIScheme
{
public:
	// Gets at the scheme's name
	virtual const char *GetName() = 0;
	virtual const char *GetFileName() = 0;

	virtual FontHandle_t GetFont( const char *fontName, bool proportional = false ) = 0;
	virtual FontHandle_t GetFontNextSize( bool bUp, const char *fontName, bool proportional = false ) = 0;
	virtual char const * GetFontName( const FontHandle_t &font ) = 0;
};


//-----------------------------------------------------------------------------
// Used to allow clients to install different sound playback systems
//-----------------------------------------------------------------------------
abstract_class IGameUISoundPlayback
{
public:
	// EmitSound will return a handle to the sound being played.
	// StopSound stops the sound given the handle to the sound
	virtual void *EmitSound( const char *pSoundName ) = 0;
	virtual void StopSound( void *pSoundHandle ) = 0; 
};





//
// LINK_GAME_CONTROLS_LIB() macro must be included in the outer .dll code
// to force all required lib objects linked into the DLL.
//
extern void LinkGameControlsLib();
#define LINK_GAME_CONTROLS_LIB() \
namespace { \
	static class CLinkGameControlsLib { \
	public: \
		CLinkGameControlsLib() { \
			LinkGameControlsLib(); \
		} \
	} s_LinkHelper; \
};


#endif // IGAMEUISYSTEMMGR_H
