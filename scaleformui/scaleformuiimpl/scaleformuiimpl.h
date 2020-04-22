//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#if !defined( __SCALEFFORMUIIMPL_H__ )
#define __SCALEFFORMUIIMPL_H__

#include "scaleformuiintegration.h"
#include "movieslot.h"
#include "sfuiavatarimage.h"
#include "sfuiinventoryimage.h"
#include "sfuichromehtmlimage.h"
#include "tier1/utlmap.h"
#include "igameevents.h"
#include "cdll_int.h"

#if defined( USE_SDL ) || defined( OSX )
#include "appframework/ilaunchermgr.h"
#endif

#if defined( SF_ENABLE_IME ) && defined( SF_ENABLE_IME_WIN32 )
#include "GFx/IME/GFx_IMEManager.h"
#include "GFxIME/GFx_IMEManagerWin32.h"
#endif

#if ( defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION ) ) || defined( _X360 )
#include <d3d9.h>
#endif

class DeviceCallbacks;
class ScaleformUIAvatarImage;

class IShaderAPI;

#define MAX_VALUES_IN_ARRAY 20
#define NUM_VALUEARRAY_SLOTS ( MAX_VALUES_IN_ARRAY-1 )

#define MAX_BOUND_COMMAND_LENGTH 64
#define TEMPORARY_BUFFER_SIZE 4086

#define MAX_AXIS_PER_JOYSTICK 2
#define MAX_JOYSTICKS_PER_SLOT 2
#define MAX_SLOTS_WITH_JOYSTICKS 2

#define JOY_VALUE_INDEX( slot, stick, axis ) ( ( slot * MAX_JOYSTICKS_PER_SLOT + stick ) * MAX_AXIS_PER_JOYSTICK + axis )

// [HPE] Removing the extra splitscreen slot - we no longer support local multiplayer on CS:GO
#define MAX_SLOTS ( SF_SS_SLOT( 0 ) + 1 )

#if defined( _WIN32 )
#define ALLOCA _alloca
#elif defined( _PS3 )
#define ALLOCA alloca
#endif

#if defined( _PS3 )
struct IDirect3DDevice9;
struct D3DPRESENT_PARAMETERS;
#endif

enum CURSOR_IMAGE_TYPE
{
	CURSOR_IMAGE_NONE,
	CURSOR_IMAGE_MOVE_CROSSHAIR,
	CURSOR_IMAGE_MOUSE,
};

inline SFPARAMS ToSFPARAMS( SF::GFx::FunctionHandler::Params* ptr )
{
	return reinterpret_cast<SFPARAMS>( ptr );
}

inline SF::GFx::FunctionHandler::Params* FromSFPARAMS( SFPARAMS ptr )
{
	return reinterpret_cast< SF::GFx::FunctionHandler::Params* >( ptr );
}

inline SFVALUE ToSFVALUE( SF::GFx::Value* ptr )
{
	return reinterpret_cast<SFVALUE>( ptr );
}

inline SF::GFx::Value* FromSFVALUE( SFVALUE ptr )
{
	return reinterpret_cast<SF::GFx::Value*>( ptr );
}

inline SFMOVIE ToSFMOVIE( Scaleform::GFx::Movie *ptr )
{
	return reinterpret_cast< SFMOVIE >( ptr );
}

inline SF::GFx::Movie* FromSFMOVIE( SFMOVIE ptr )
{
	return reinterpret_cast< SF::GFx::Movie* >( ptr );
}

inline SFMOVIEDEF ToSFMOVIEDEF( Scaleform::GFx::MovieDef *ptr )
{
	return reinterpret_cast< SFMOVIEDEF >( ptr );
}

inline SF::GFx::MovieDef* FromSFMOVIEDEF( SFMOVIEDEF ptr )
{
	return reinterpret_cast< SF::GFx::MovieDef* >( ptr );
}

class CScaleFormThreadCommandQueue: public SF::Render::ThreadCommandQueue
{
public:

	virtual void GetRenderInterfaces( SF::Render::Interfaces* p ) 
	{
		p->pHAL = pHAL;
		p->pRenderer2D = pR2D;
		p->pTextureManager = pHAL->GetTextureManager();
		p->RenderThreadID = 0;
	}

	virtual void PushThreadCommand( SF::Render::ThreadCommand* command ) 
	{
		if (command) {
			MaterialLock_t hMaterialLock = materials->Lock();
			command->Execute();
			materials->Unlock( hMaterialLock );

		}
	}

	SF::Render::HAL* pHAL;
	SF::Render::Renderer2D* pR2D;

};

#if defined( SF_ENABLE_IME ) && defined( SF_ENABLE_IME_WIN32 )
class ScaleformeUIIMEManager : public SF::GFx::IME::GFxIMEManagerWin32
{
public:

	ScaleformeUIIMEManager( HWND hwnd, IGameEventManager2* pGameEventManager )
		: SF::GFx::IME::GFxIMEManagerWin32( hwnd ), m_pGameEventManager( pGameEventManager ) {}

	virtual void FinalizeComposition( const wchar_t* pstr, SF::UPInt len = SF_MAX_UPINT )
	{
		IGameEvent* pEvent = m_pGameEventManager->CreateEventA( "cs_handle_ime_event" );

		if ( pEvent )
		{
			pEvent->SetString( "eventtype", "addchars" );
			pEvent->SetWString( "eventdata", pstr );
			m_pGameEventManager->FireEventClientSide( pEvent );
		}
	}

	virtual void SetCompositionText( const wchar_t* pstr, SF::UPInt len = SF_MAX_UPINT )
	{
		IGameEvent* pEvent = m_pGameEventManager->CreateEventA( "cs_handle_ime_event" );

		if ( pEvent )
		{
			pEvent->SetString( "eventtype", "setcomposition" );
			pEvent->SetWString( "eventdata", pstr );
			m_pGameEventManager->FireEventClientSide( pEvent );
		}
	}

	virtual void HighlightText( SF::UPInt pos, SF::UPInt len, TextHighlightStyle style, bool clause )
	{}

	virtual void ClearComposition()
	{
		IGameEvent* pEvent = m_pGameEventManager->CreateEventA( "cs_handle_ime_event" );

		if ( pEvent )
		{
			pEvent->SetString( "eventtype", "cancelcomposition" );
			pEvent->SetWString( "eventdata", L"" );
			m_pGameEventManager->FireEventClientSide( pEvent );
		}
	}

private:

	IGameEventManager2* m_pGameEventManager;
};
#endif

class ScaleformUIImpl: public CTier3AppSystem<IScaleformUI>
{
	typedef CTier3AppSystem<IScaleformUI> BaseClass;
	/**********************
	 * These are the basic singleton support functions
	 * the are in ScaleformUIInitImpl.cpp
	 */

	/* singleton support */
public:
	static ScaleformUIImpl m_Instance;

	/* normal class */
protected:
	SF::GFx::System* m_pSystem;
	SF::GFx::Loader* m_pLoader;

#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	SF::Ptr<SF::Render::D3D9::HAL> m_pRenderHAL;
#else
	SF::Ptr<SF::Render::GL::HAL> m_pRenderHAL;
#endif
	SF::Ptr<SF::Render::Renderer2D> m_pRenderer2D;
	
	CScaleFormThreadCommandQueue *m_pThreadCommandQueue;

	IDirect3DDevice9*	  m_pDevice;
#if defined( _WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	IDirect3DStateBlock9* m_pD3D9Stateblock;
	DWORD				  m_srgbRenderState;
	DWORD				  m_pSavedSrgbSamplerStates[16];
#endif

#if defined( _WIN32 )
	SF::Ptr<ScaleformeUIIMEManager> m_pIMEManager;
#endif
	SF::Ptr<ScaleformTranslatorAdapter> m_pTranslatorAdapter;
	SF::Ptr<ScaleformFunctionHandlerAdapter> m_pFunctionAdapter;

	SF::SysAlloc* m_pAllocator;

#if defined( USE_SDL ) || defined( OSX ) 
	ILauncherMgr *m_pLauncherMgr;
#endif

	IShaderDeviceMgr* m_pShaderDeviceMgr;
	DeviceCallbacks* m_pDeviceCallbacks;
	IShaderAPI*		m_pShaderAPI;

	IGameUIFuncs* m_pGameUIFuncs;

	IVEngineClient* m_pEngine;

	IGameEventManager2* m_pGameEventManager;

	wchar_t m_wcControllerButtonToBindingTable[BUTTON_CODE_COUNT][MAX_BOUND_COMMAND_LENGTH];

	BaseSlot* m_SlotPtrs[MAX_SLOTS];
	
	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	// CThreadMutex m_SlotMutexes[MAX_SLOTS];
	
	int		m_SlotDeniesInputRefCount[MAX_SLOTS];

	CUtlVector<SF::GFx::Movie*> m_MovieViews;

	CUtlVector<const wchar_t*> m_LocalizableCommandNames;
	CUtlVector<const char*> m_LocalizableCommandKeys;

	CUtlVector<const char*> m_MovieDefNameCache;
	CUtlVector<SF::GFx::MovieDef*> m_MovieDefCache;

	CUtlVector<SF::GFx::Value*> m_ValueCache;
	CUtlVector<SF::GFx::Value*> m_ValueArrayCaches[NUM_VALUEARRAY_SLOTS];

#ifdef _DEBUG
	CUtlVector<SF::GFx::Value*> m_ValuesInUse;
	CUtlVector<SF::GFx::Value*> m_ValueArraysInUse[NUM_VALUEARRAY_SLOTS];
#endif

	float m_fJoyValues[MAX_AXIS_PER_JOYSTICK * MAX_JOYSTICKS_PER_SLOT * MAX_SLOTS_WITH_JOYSTICKS];
	int m_iJoyAxisButtonsDown[MAX_SLOTS_WITH_JOYSTICKS];

	int m_iScreenWidth;
	int m_iScreenHeight;

	int m_iLastMouseX;
	int m_iLastMouseY;

	float m_fCursorTimeUntilHide;

	char m_cTemporaryBuffer[TEMPORARY_BUFFER_SIZE];

	bool m_bTrySWFFirst;
	bool m_bPumpScaleformStats;
	bool m_bForcePS3;
	bool m_bDenyAllInputToGame;
	bool m_bShowActionNameIfUnbound;
	bool m_bEatPS3MouseEvent;
	int  m_iWantCursorShown;
	
	bool m_bIMEEnabled;
	int m_iIMEFocusSlot;

	CUtlMap< XUID, ScaleformUIAvatarImage * > m_mapUserXuidToAvatar;
	IVTFTexture* m_pDefaultAvatarTexture;
	ScaleformUIAvatarImage* m_pDefaultAvatarImage;

#if !defined( NO_STEAM )
	bool m_bSteamCallbacksConfigured;
	void EnsureSteamCallbacksConfigured();
	STEAM_CALLBACK_MANUAL( ScaleformUIImpl, Steam_OnPersonaStateChanged, PersonaStateChange_t, m_CallbackPersonaStateChanged );
	STEAM_CALLBACK_MANUAL( ScaleformUIImpl, Steam_OnAvatarImageLoaded, AvatarImageLoaded_t, m_CallbackAvatarImageLoaded );
#endif // NO_STEAM

	CUtlMap< uint64, ScaleformUIInventoryImage * > m_mapItemIdToImage;
	IVTFTexture* m_pDefaultInventoryTexture;
	ScaleformUIInventoryImage* m_pDefaultInventoryImage;
	struct DefaultInventoryIcon_t
	{
		IVTFTexture* m_pTexture;
		ScaleformUIInventoryImage* m_pImage;
	};
#ifdef USE_DEFAULT_INVENTORY_ICON_BACKGROUNDS
	CUtlHashFast< DefaultInventoryIcon_t > m_defaultInventoryIcons;
#endif
	CUtlMap< uint64, ScaleformUIChromeHTMLImage* > m_mapImageIdToChromeImage;
	ScaleformUIChromeHTMLImage* m_pDefaultChromeHTMLImage;

	int m_iKeyboardSlot;
	CURSOR_IMAGE_TYPE m_loadedCursorImage;
	bool m_isCursorForced;

	// Time updated in RunFrame and used in AdvanceSlot
	float m_fTime;

#if defined( _PS3 )
	InputDevice_t m_preForcedInputType;
#endif

	// Set to true if advance and render are running on the same thread
	bool m_bSingleThreaded;
	// ScaleformUI::ClearCache will queue the call to clear the scaleform mesh cache
	// to ensure the mesh cache being cleared on the render thread
	bool m_bClearMeshCacheQueued;

protected:
	ScaleformUIImpl( void );
	void ClearMembers( void );

	void InitMovieImpl( void );
	void ShutdownMovieImpl( void );

	void InitRendererImpl( void );
	void ShutdownRendererImpl( void );

	void InitValueImpl( void );
	void ShutdownValueImpl( void );

	void InitHighLevelImpl( void );
	void ShutdownHighLevelImpl( void );

	void InitMovieSlotImpl( void );
	void ShutdownMovieSlotImpl( void );

	void InitCursorImpl( void );
	void ShutdownCursorImpl( void );

	void InitTranslationImpl( void );
	void ShutdownTranslationImpl( void );

	void InitFonts( void );

	bool DistributeEvent( SF::GFx::Event& event, int slotNumber, bool toAllSlots, bool clearControllerUI = true );
	bool DistributeKeyEvent( bool keyDown, bool fromController, const char* binding, ButtonCode_t code, ButtonCode_t vkey, int slotNumber, bool toAllSlots );
	bool DistributeCharTyped( wchar_t code );
	bool TallyAxisButtonEvent( int slot, int code, bool down );

	bool HitTest( int x, int y );

	BaseSlot* LockSlotPtr( int slot );
	void UnlockSlotPtr( int slot );

	bool AnalogStickNavigationDisabled( int slot );

	void UpdateUIAvatarImages( void );

	void SetJoyValue( int slot, int stick, int axis, int value );

#if defined( _PS3 )
	void InitCursorPS3( bool usingMoveCrosshair );
#endif

public:
	// the following are for the system callbacks from flash
	void AddAPIFunctionToObject( SFVALUE pAPI, SFMOVIE pMovie, ScaleformUIFunctionHandlerObject* object, const ScaleformUIFunctionHandlerDefinition* pFunctionDef );

	int GetScreenWidth( void )
	{
		return m_iScreenWidth;
	}
	int GetScreenHeight( void )
	{
		return m_iScreenHeight;
	}

	bool OwnsAtLeastOneMutex( void );
	void DebugBreakIfNotLocked( void );

	bool GetVerbose( void );
	void SetVerbose( bool bVerbose );

	// IAppSystem implementation
public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect( void );

	// Here's where systems can access other interfaces implemented by this m_pObject
	// Returns NULL if it doesn't implement the requested interface
	virtual void *QueryInterface( const char *pInterfaceName );

	// Init, shutdown
	virtual InitReturnVal_t Init( void );
	virtual void Shutdown( void );

	// Returns all dependent libraries
	virtual const AppSystemInfo_t* GetDependencies( void );

	// Returns the tier
	virtual AppSystemTier_t GetTier( void )
	{
		return APP_SYSTEM_TIER3;
	}

	// Reconnect to a particular interface
	virtual void Reconnect( CreateInterfaceFn factory, const char *pInterfaceName )
	{
		BaseClass::Reconnect( factory, pInterfaceName );
	}

	void LogPrintf( const char *format, ... );

	IVEngineClient* GetEnginePtr()
	{
		return m_pEngine;
	}

	// IScaleformUI implementation
	/**************************
	 * common, high-level functions and are implemented in ScaleformUIHighLevelImpl.cpp
	 */

	bool GetForcePS3( void ) { return m_bForcePS3; }
#if defined( _PS3 )
	void PS3UseMoveCursor( void );
	void PS3UseStandardCursor( void );
	void PS3ForceCursorStart( void );
	void PS3ForceCursorEnd( void );
#endif

public:

	// called when the safezone convar is changed
	// tells all the slots to change their safe zone
	void UpdateSafeZone( void );

	// called when the UI tint convar is changed - applies the new tint to all slots
	void UpdateTint( void );

	//	virtual void Render();
	virtual void SetSingleThreadedMode( bool bSingleThreded );
	virtual void RunFrame( float time );
	void AdvanceSlot( int slot );
	virtual bool HandleInputEvent( const InputEvent_t &event );
	virtual bool HandleIMEEvent( size_t hwnd, unsigned int uMsg, unsigned int wParam, long lParam );
	virtual bool PreProcessKeyboardEvent( size_t hwnd, unsigned int uMsg, unsigned int wParam, long lParam );
	virtual void SetIMEEnabled( bool bEnabled );
	virtual void SetIMEFocus( int slot );
	virtual void ShutdownIME();

	virtual float GetJoyValue( int slot, int stickIndex, int axis );

	void SetScreenSize( int x, int y );
	const char* CorrectFlashFileName( const char * name );

	ScaleformUIAvatarImage* GetAvatarImage( XUID playerID );
	void UpdateAvatarImages( void );

	virtual bool AvatarImageAddRef( XUID playerID );
	virtual void AvatarImageRelease( XUID playerID );
	virtual void AvatarImageReload( XUID playerID, IScaleformAvatarImageProvider *pProvider );
	virtual void AddDeviceDependentObject( IShaderDeviceDependentObject * pObject );
	virtual void RemoveDeviceDependentObject( IShaderDeviceDependentObject * pObject );

	ScaleformUIInventoryImage* GetInventoryImage( uint64 iItemId );
	virtual bool InventoryImageAddRef( uint64 iItemId, IScaleformInventoryImageProvider *pGlobalInventoryImageProvider );
	virtual void InventoryImageUpdate( uint64 iItemId, IScaleformInventoryImageProvider *pGlobalInventoryImageProvider );
	virtual void InventoryImageRelease( uint64 iItemId );

#ifdef USE_DEFAULT_INVENTORY_ICON_BACKGROUNDS
	virtual void InitInventoryDefaultIcons( CUtlVector< const char * > *vecIconDefaultNames );
#endif

	ScaleformUIChromeHTMLImage* GetChromeHTMLImage( uint64 imageID );
	virtual bool ChromeHTMLImageAddRef( uint64 imageID );
	virtual void ChromeHTMLImageUpdate( uint64 imageID, const byte* rgba, int width, int height, ::ImageFormat format );
	virtual void ChromeHTMLImageRelease( uint64 imageID );

	virtual void ForceUpdateImages();

	SF::Render::Image* CreateImageFromFile( const char *pszFileName, const SF::GFx::ImageCreateInfo& info, int width, int height );

	/**********************************
	 * slot stuff.  This is all in ScaleformUIImplMovieSlot.cpp
	 */

	virtual void SetSlotViewport( int slot, int x, int y, int width, int height );
	virtual void RenderSlot( int slot );
	virtual void ForkRenderSlot( int slot );
	virtual void JoinRenderSlot( int slot );

	virtual void InitSlot( int slotID, const char* rootMovie, IScaleformSlotInitController *pController );

	virtual void SlotAddRef( int slot );
	virtual void SlotRelease( int slotID );

	virtual void LockSlot( int slot );
	virtual void UnlockSlot( int slot );

	virtual void RequestElement( int slot, const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject );
	virtual void RemoveElement( int slot, SFVALUE element );

	virtual void InstallGlobalObject( int slot, const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject, SFVALUE *pInstalledGlobalObjectResult );
	virtual void RemoveGlobalObject( int slot, SFVALUE element );

	virtual bool SlotConsumesInputEvents( int slot );
	virtual bool ConsumesInputEvents( void );
	virtual bool SlotDeniesInputToGame( int slot );
	virtual void DenyInputToGame( bool value );
	virtual void DenyInputToGameFromFlash( int slot, bool value );

	virtual void SendUIEvent( const char* action, const char* eventData, int slot = 0 );

	virtual void LockInputToSlot( int slot );
	virtual void UnlockInput( void );

	virtual void ForceCollectGarbage( int slot );

	virtual void ClearCache( void );

	virtual void LockMostRecentInputDevice( int slot );

	/************************************
	 * Cursor stuff. in ScaleformUIImplCursor.cpp
	 */
protected:
	void UpdateCursorLazyHide( float time );
	void UpdateCursorWaitTime( float newTime );
	void InnerShowCursor( void );
	void InnerHideCursor( void );
	void CursorMoved( void );
	void ControllerMoved( void );

	bool IsSetToControllerUI( int slot );
	void SetToControllerUI( int slot, bool value );

public:
	void MaybeShowCursor( void );
	bool IsSlotKeyboardAccessible( int slot );

	virtual void InitCursor( const char* cursorMovie );
	virtual void ReleaseCursor( void );

	virtual bool IsCursorVisible( void );
	virtual void RenderCursor( void );
	virtual void AdvanceCursor( void );

	virtual void SetCursorViewport( int x, int y, int width, int height );

	virtual void ShowCursor( void );
	virtual void HideCursor( void );

	virtual void SetCursorShape( int shapeIndex );

	/*******
	 * Renderer stuff
	 * These are in ScaleformUIRendererImpl.cpp
	 */

	void FinishInitializingRenderer( void );

	void SetRenderingDevice( IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentParameters, HWND hWnd );
	void NotifyRenderingDeviceLost();
	
	void SaveRenderingState( void );
	void RestoreRenderingState( void );

	void SetRenderTargets( void );

	void DumpMeshCacheStats( void );

	/*********
	 * Movie Def related stuff
	 * These are in ScaleformMovieImpl.cpp
	 */

	virtual SFMOVIEDEF CreateMovieDef( const char* pfilename, unsigned int loadConstants = 0, size_t memoryArena = 0 );
	virtual void ReleaseMovieDef( SFMOVIEDEF movieDef );

	virtual SFMOVIE MovieDef_CreateInstance( SFMOVIEDEF movieDef, bool initFirstFrame = true, size_t memoryArena = 0 );
	virtual void ReleaseMovieView( SFMOVIE movieView );

	virtual void MovieView_Advance( SFMOVIE movieView, float time, unsigned int frameCatchUpCount = 2 );
	virtual void MovieView_SetBackgroundAlpha( SFMOVIE movieView, float alpha );
	virtual void MovieView_SetViewport( SFMOVIE movieView, int bufw, int bufh, int left, int top, int w, int h, unsigned int flags = 0 );
	virtual void MovieView_Display( SFMOVIE movieView );

	virtual void MovieView_SetViewScaleMode( SFMOVIE movieView, IScaleformUI::_ScaleModeType type );
	virtual IScaleformUI::_ScaleModeType MovieView_GetViewScaleMode( SFMOVIE movieView );

	virtual void MovieView_SetViewAlignment( SFMOVIE movieView, IScaleformUI::_AlignType type );
	virtual IScaleformUI::_AlignType MovieView_GetViewAlignment( SFMOVIE movieView );

	virtual SFVALUE MovieView_CreateObject( SFMOVIE movieView, const char* className = NULL, SFVALUEARRAY args = SFVALUEARRAY(0, NULL), int numArgs = 0 );
	virtual SFVALUE MovieView_GetVariable( SFMOVIE movieView, const char* variablePath );

	virtual SFVALUE MovieView_CreateString( SFMOVIE movieView, const char *str );
	virtual SFVALUE MovieView_CreateStringW( SFMOVIE movieView, const wchar_t *str );

	virtual SFVALUE MovieView_CreateArray( SFMOVIE movieView, int size = -1 );


	virtual bool MovieView_HitTest( SFMOVIE movieView, float x, float y, IScaleformUI::_HitTestType testCond = IScaleformUI::HitTest_Shapes, unsigned int controllerIdx = 0 );

	/*************************
	 * keycode stuff.
	 * these are in ScaleformUIKeymapImpl.cpp
	 */

	void DecodeButtonandSlotFromButtonCode( ButtonCode_t inCode, ButtonCode_t &outcode, int &outSlot );


	/***************************
	 * translation and button glyph functionality.
	 * These are in ScaleformUITranslationImpl.cpp
	 */

protected:
	void RemoveKeyBindings( void );
	const wchar_t* LocalizeCommand( const wchar_t* command );
	ButtonCode_t LookupButtonFromBinding( const wchar_t* binding, bool bForceControllerLookup = false );
	void BindCommandToControllerButton( ButtonCode_t code, const char* binding );
	IScaleformUI::ControllerButton::Enum ValveButtonToControllerButton( ButtonCode_t b );


public:
	virtual void RefreshKeyBindings( void );
	virtual void ShowActionNameWhenActionIsNotBound( bool value );
	virtual void UpdateBindingForButton( ButtonCode_t bt, const char* pbinding );

	virtual const wchar_t* Translate( const char *key, bool* pIsHTML );
	virtual const wchar_t* ReplaceGlyphKeywordsWithHTML( const wchar_t* pin, int fontSize, bool bForceControllerGlyph = false );
	virtual const wchar_t* ReplaceGlyphKeywordsWithHTML( const char* text, int fontSize, bool bForceControllerGlyph = false );
	virtual void MakeStringSafe( const wchar_t* stringin, wchar_t* stringout, int outlength );

	/*************************
	 * value stuff
	 * these are in ScaleformUIValueImpl.cpp
	 */

protected:
	SFVALUE CreateGFxValue( SFVALUE pValue = NULL );
	void ReleaseGFxValue( SFVALUE pValue );

public:
	virtual SFVALUE CreateValue( SFVALUE value );
	virtual SFVALUE CreateValue( const char* value );
	virtual SFVALUE CreateValue( const wchar_t* value );
	virtual SFVALUE CreateValue( int value );
	virtual SFVALUE CreateValue( float value );
	virtual SFVALUE CreateValue( bool value );

	virtual SFVALUE CreateNewObject( int slot );
	virtual SFVALUE CreateNewString( int slot, const char* value );
	virtual SFVALUE CreateNewString( int slot, const wchar_t* value );
	virtual SFVALUE CreateNewArray( int slot, int size = -1 );

	virtual void Value_SetValue( SFVALUE obj, SFVALUE value );
	virtual void Value_SetValue( SFVALUE obj, int value );
	virtual void Value_SetValue( SFVALUE obj, float value );
	virtual void Value_SetValue( SFVALUE obj, bool value );
	virtual void Value_SetValue( SFVALUE obj, const char* value );
	virtual void Value_SetValue( SFVALUE obj, const wchar_t* value );

	virtual void Value_SetColor( SFVALUE obj, int color );
	virtual void Value_SetColor( SFVALUE obj, float r, float g, float b, float a );
	virtual void Value_SetTint( SFVALUE obj, int color );
	virtual void Value_SetTint( SFVALUE obj, float r, float g, float b, float a );

	virtual void Value_SetColorTransform( SFVALUE obj, int colorMultiply, int colorAdd );
	virtual void Value_SetColorTransform( SFVALUE obj, float r, float g, float b, float a, int colorAdd );

	virtual void Value_SetText( SFVALUE obj, const char* value );
	virtual void Value_SetText( SFVALUE obj, const wchar_t* value );
	virtual void Value_SetTextHTML( SFVALUE obj, const char* value );
	virtual void Value_SetTextHTML( SFVALUE obj, const wchar_t* value );
	virtual int  Value_SetFormattedText( SFVALUE obj, const char* pFormat, ... );

	virtual void Value_SetArraySize( SFVALUE obj, int size );
	virtual int  Value_GetArraySize( SFVALUE obj );
	virtual void Value_ClearArrayElements( SFVALUE obj );
	virtual void Value_RemoveArrayElement( SFVALUE obj, int index );
	virtual void Value_RemoveArrayElements( SFVALUE obj, int index, int count );
	virtual SFVALUE Value_GetArrayElement( SFVALUE obj, int index );

	virtual void Value_SetArrayElement( SFVALUE obj, int index, SFVALUE value );
	virtual void Value_SetArrayElement( SFVALUE obj, int index, int value );
	virtual void Value_SetArrayElement( SFVALUE obj, int index, float value );
	virtual void Value_SetArrayElement( SFVALUE obj, int index, bool value );
	virtual void Value_SetArrayElement( SFVALUE obj, int index, const char* value );
	virtual void Value_SetArrayElement( SFVALUE obj, int index, const wchar_t* value );

	void SetVisible( SFVALUE pgfx, bool visible );

	virtual void Value_SetVisible( SFVALUE obj, bool visible );
	virtual void Value_GetDisplayInfo( SFVALUE obj, ScaleformDisplayInfo* dinfo );
	virtual void Value_SetDisplayInfo( SFVALUE obj, const ScaleformDisplayInfo* dinfo );

	virtual void ReleaseValue( SFVALUE value );

	virtual void CreateValueArray( SFVALUEARRAY& valueArray, int length );
	virtual SFVALUEARRAY CreateValueArray( int length );
	virtual void ReleaseValueArray( SFVALUEARRAY& valueArray );
	virtual void ReleaseValueArray( SFVALUEARRAY& valueArray, int count );	// DEPRECATED
	virtual SFVALUE ValueArray_GetElement( SFVALUEARRAY, int index );
	virtual IScaleformUI::_ValueType ValueArray_GetType( SFVALUEARRAY array, int index );
	virtual double ValueArray_GetNumber( SFVALUEARRAY array, int index );
	virtual bool ValueArray_GetBool( SFVALUEARRAY array, int index );
	virtual const char* ValueArray_GetString( SFVALUEARRAY array, int index );
	virtual const wchar_t* ValueArray_GetStringW( SFVALUEARRAY array, int index );

	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, SFVALUE value );
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, int value );
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, float value );
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, bool value );
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, const char* value );
	virtual void ValueArray_SetElement( SFVALUEARRAY, int index, const wchar_t* value );

	virtual void ValueArray_SetElementText( SFVALUEARRAY, int index, const char* value );
	virtual void ValueArray_SetElementText( SFVALUEARRAY, int index, const wchar_t* value );
	virtual void ValueArray_SetElementTextHTML( SFVALUEARRAY, int index, const char* value );
	virtual void ValueArray_SetElementTextHTML( SFVALUEARRAY, int index, const wchar_t* value );

	virtual bool Value_HasMember( SFVALUE value, const char* name );
	virtual SFVALUE Value_GetMember( SFVALUE value, const char* name );

	virtual bool Value_SetMember( SFVALUE obj, const char *name, SFVALUE value );
	virtual bool Value_SetMember( SFVALUE obj, const char *name, int value );
	virtual bool Value_SetMember( SFVALUE obj, const char *name, float value );
	virtual bool Value_SetMember( SFVALUE obj, const char *name, bool value );
	virtual bool Value_SetMember( SFVALUE obj, const char *name, const char* value );
	virtual bool Value_SetMember( SFVALUE obj, const char *name, const wchar_t* value );

	virtual ISFTextObject* TextObject_MakeTextObject( SFVALUE value );
	virtual ISFTextObject* TextObject_MakeTextObjectFromMember( SFVALUE value, const char* pName );

	virtual bool Value_InvokeWithoutReturn( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args );
	virtual SFVALUE Value_Invoke( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args );
	virtual bool Value_InvokeWithoutReturn( SFVALUE obj, const char* methodName, SFVALUE args, int numArgs );				
	virtual SFVALUE Value_Invoke( SFVALUE obj, const char* methodName, SFVALUE args, int numArgs );						
	virtual bool Value_InvokeWithoutReturn( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args, int numArgs );	// DEPRECATED
	virtual SFVALUE Value_Invoke( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args, int numArgs );			// DEPRECATED

	virtual IScaleformUI::_ValueType Value_GetType( SFVALUE obj );
	virtual double Value_GetNumber( SFVALUE obj );
	virtual bool Value_GetBool( SFVALUE obj );
	virtual const char* Value_GetString( SFVALUE obj );
	virtual const wchar_t* Value_GetStringW( SFVALUE obj );

	virtual SFVALUE Value_GetText( SFVALUE obj );
	virtual SFVALUE Value_GetTextHTML( SFVALUE obj );

	/************************************
	 * m_Callback parameter stuff
	 * These are in ScaleformUIParamsImpl.cpp
	 */

	virtual SFVALUEARRAY Params_GetArgs( SFPARAMS params );
	virtual unsigned int Params_GetNumArgs( SFPARAMS params );
	virtual bool Params_ArgIs( SFPARAMS params, unsigned int index, IScaleformUI::_ValueType v );

	virtual SFVALUE Params_GetArg( SFPARAMS params, int index = 0 );
	virtual IScaleformUI::_ValueType Params_GetArgType( SFPARAMS params, int index = 0 );
	virtual double Params_GetArgAsNumber( SFPARAMS params, int index = 0 );
	virtual bool Params_GetArgAsBool( SFPARAMS params, int index = 0 );
	virtual const char* Params_GetArgAsString( SFPARAMS params, int index = 0 );
	virtual const wchar_t* Params_GetArgAsStringW( SFPARAMS params, int index = 0 );

	virtual void Params_DebugSpew( SFPARAMS params );

	virtual void Params_SetResult( SFPARAMS params, SFVALUE value );
	virtual void Params_SetResult( SFPARAMS params, int value );
	virtual void Params_SetResult( SFPARAMS params, float value );
	virtual void Params_SetResult( SFPARAMS params, bool value );
	virtual void Params_SetResult( SFPARAMS params, const char* value, bool bMakeNewValue = true );
	virtual void Params_SetResult( SFPARAMS params, const wchar_t* value, bool bMakeNewValue = true );

	virtual SFVALUE Params_CreateNewObject( SFPARAMS params );
	virtual SFVALUE Params_CreateNewString( SFPARAMS params, const char* value );
	virtual SFVALUE Params_CreateNewString( SFPARAMS params, const wchar_t* value );
	virtual SFVALUE Params_CreateNewArray( SFPARAMS params, int size = -1 );

	ButtonCode_t GetCurrentKey() { return m_CurrentKey; }


protected:
	ButtonCode_t m_CurrentKey;

protected:

	/************************************
	 * helper functions to map to and from SFUI and SDK enums
	 */
	SF::GFx::Movie::ScaleModeType ScaleModeType_SFUI_to_SDK( IScaleformUI::_ScaleModeType scaleModeType );
	IScaleformUI::_ScaleModeType ScaleModeType_SDK_to_SFUI( SF::GFx::Movie::ScaleModeType scaleModeType );

	SF::GFx::Value::ValueType ValueType_SFUI_to_SDK( IScaleformUI::_ValueType valueType );
	IScaleformUI::_ValueType ValueType_SDK_to_SFUI( SF::GFx::Value::ValueType ValueType );

	SF::GFx::Movie::AlignType AlignType_SFUI_to_SDK( IScaleformUI::_AlignType alignType );
	IScaleformUI::_AlignType AlignType_SDK_to_SFUI( SF::GFx::Movie::AlignType alignType );

	SF::GFx::Movie::HitTestType HitTestType_SFUI_to_SDK( IScaleformUI::_HitTestType hitTestType );
	IScaleformUI::_HitTestType HitTestType_SDK_to_SFUI( SF::GFx::Movie::HitTestType hitTestType );
};

#define SFINST ( ScaleformUIImpl::m_Instance )

class DeviceCallbacks: public IShaderDeviceDependentObject
{
public:
	int m_iRefCount;
	ScaleformUIImpl* m_pScaleform;

	DeviceCallbacks( void ) :
	m_iRefCount( 1 ), m_pScaleform( NULL )
	{
	}

	virtual void DeviceLost( void )
	{
		m_pScaleform->NotifyRenderingDeviceLost();
	}

	virtual void DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd )
	{
		m_pScaleform->SetRenderingDevice( ( IDirect3DDevice9* )pDevice, ( D3DPRESENT_PARAMETERS* )pPresentParameters, ( HWND )pHWnd );
	}

	virtual void ScreenSizeChanged( int width, int height )
	{
		m_pScaleform->SetScreenSize( width, height );
	}
};

class ScaleformCallbackHolder: public SF::GFx::ASUserData
{
public:
	ScaleformUIFunctionHandlerObject* m_pObject;
	ScaleformUIFunctionHandler m_Callback;

	inline ScaleformCallbackHolder( ScaleformUIFunctionHandlerObject* object, ScaleformUIFunctionHandler callback ) :
		SF::GFx::ASUserData(), m_pObject( object ), m_Callback( callback )
	{
	}

	inline void Execute( SF::GFx::FunctionHandler::Params *params )
	{
		( m_pObject->*m_Callback )( &SFINST, ToSFPARAMS( params ) );
	}

	virtual void OnDestroy( SF::GFx::Movie* pmovie, void* pobject );

};

#endif
