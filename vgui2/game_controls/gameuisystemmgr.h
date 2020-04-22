//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef GAMEUISYSTEMMANAGER_H
#define GAMEUISYSTEMMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "tier1/timeutils.h"

#include "rendersystem/vertexdata.h"
#include "rendersystem/indexdata.h"
#include "inputsystem/buttoncode.h"
#include "gameuischeme.h"
#include "igameuisystemmgr.h"

#include "appframework/iappsystem.h"
#include "tier3/tier3dm.h"

#include "tier1/utlstring.h"
#include "tier1/utlmap.h"

#include "gameuidynamictextures.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IGameUIScreenController;
class IRenderContext;
class CGameUISystem;
class CGameGraphic;
class CHitArea;
class CGameText;
class CGameUIDefinition;
FORWARD_DECLARE_HANDLE( InputContextHandle_t );



//-----------------------------------------------------------------------------
// Global interfaces...
//-----------------------------------------------------------------------------
class IScriptManager;
extern IScriptManager *g_pScriptManager;



abstract_class IGameUIGraphicClassFactory
{
public:
	// Returns an instance of a graphic interface (keyvalues owned by caller)
	virtual CGameGraphic * CreateNewGraphicClass( KeyValues *kvRequest, CGameUIDefinition *pMenu ) = 0;
};


//-----------------------------------------------------------------------------
//
// Game UI system manager
//
//-----------------------------------------------------------------------------
class CGameUISystemMgr :  public CTier3AppSystem< IGameUISystemMgr >
{
	typedef CTier3AppSystem< IGameUISystemMgr > BaseClass;
public:
	// Constructor, destructor
	CGameUISystemMgr();
	virtual ~CGameUISystemMgr();

	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual const AppSystemInfo_t* GetDependencies();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init( );
	virtual void Shutdown();

	virtual void SetGameUIVisible( bool bVisible );
	virtual bool GetGameUIVisible(){ return IsMenuVisible(); }

	// Load the game UI menu screen
	// key values are owned and released by caller
	virtual IGameUISystem * LoadGameUIScreen( KeyValues *kvScreenLoadSettings );
	virtual void ReleaseAllGameUIScreens();

	virtual void RunFrame();

	virtual void RegisterScreenControllerFactory( char const *szControllerName, IGameUIScreenControllerFactory *pFactory );
	virtual IGameUIScreenControllerFactory * GetScreenControllerFactory( char const *szControllerName );

	virtual IGameUISystemSurface * GetSurface();
	virtual IGameUISchemeMgr * GetSchemeMgr();
	virtual IGameUIMiscUtils * GetMiscUtils();

	virtual void Render( const Rect_t &viewport, DmeTime_t flCurrentTime );
	virtual void Render( IRenderContext *pRenderContext, PlatWindow_t hWnd, const Rect_t &viewport, DmeTime_t flCurrentTime );

	virtual void UseGameInputSystemEventQueue( bool bEnable );
	virtual void RegisterInputEvent( const InputEvent_t &iEvent );
	virtual void SendEventToAllScreens( KeyValues *kvGlobalEvent );
	virtual void PostEventToAllScreens( KeyValues *kvGlobalEvent );
	virtual void SetSoundPlayback( IGameUISoundPlayback *pPlayback );
	virtual bool IsMenuVisible() const;
	virtual void SetInputContext( InputContextHandle_t hInputContext );

	CHitArea *GetMouseFocus();
	CHitArea *GetMouseFocus( int x, int y );
	CHitArea *GetKeyFocus();
	CHitArea *GetRequestedKeyFocus();
	void RequestKeyFocus( CHitArea *pGraphic, KeyValues *args = NULL );

	void GetScreenHeightForFontLoading( int &nTall );
	void SetScheme( IGameUIScheme * scheme );
	IGameUIScheme * GetCurrentScheme();
	DmeTime_t GetTime();

	// Render system stuff
	RenderInputLayout_t m_hInputLayout;
	RenderShaderHandle_t m_hVertexShader;
	RenderShaderHandle_t m_hPixelShader;
	ConstantBufferHandle_t m_hConstBuffer;

	void OnMouseFocusGained( CHitArea *mouseFocus );
	void OnMouseFocusLost( CHitArea *mouseFocus );

	bool OnKeyCodeTyped( const ButtonCode_t &code );
	bool OnKeyTyped( const wchar_t &unichar );

	bool OnGameGraphicScriptEvent( CGameGraphic *pGraphic, KeyValues *kvEvent );

	// Force focus to update on the next frame
	void ForceFocusUpdate(){ m_bForceFocusUpdate = true; }

	void PlayMenuSound( const char *pSoundFileName );
	void StopMenuSound( const char *pSoundFileName );

	void SetWindowSize( int nWidth, int nHeight );
	void GetWindowSize( int &nWidth, int &nHeight );
	void SetViewportSize( int nWidth, int nHeight );
	void GetViewportSize( int &nWidth, int &nHeight );

	// Returns the input context to use to control the cursor
	InputContextHandle_t GetInputContext( ) const;

	void OnScreenReleased( CGameUISystem *pScreen );

	// Dynamic texture management.
	void InitImageAlias( const char *pAlias );
	void LoadImageAliasTexture( const char *pAlias, const char *pBaseTextureName );
	void ReleaseImageAlias( const char *pAlias);
	IMaterial *GetImageAliasMaterial( const char *pAlias );
	ImageAliasData_t *GetImageAliasData( const char *pAlias );
	void TexCoordsToSheetTexCoords( const char *pAlias, Vector2D texCoords, Vector2D &sheetTexCoords );
	void DrawDynamicTexture( const char *pAlias, int x, int y );

	void ShowCursorCoords();
	void ShowGraphicName();

	virtual void RegisterGraphicClassFactory( char const *szGraphicClassName, IGameUIGraphicClassFactory *pFactory );
	virtual IGameUIGraphicClassFactory *GetGraphicClassFactory( char const *szGraphicClassName );

	virtual void InitRenderTargets();
	virtual IMaterialProxy *CreateProxy( const char *proxyName );

private:
	void GetScreenSize( int &nWide, int &nTall );

	Vector2D CursorToStage( Vector2D cursorPos );


	// We may wind up with multiple viewports and multiple lists?
	Rect_t m_Viewport;
	int m_nWindowWidth, m_nWindowHeight;
	CUtlVector< CGameUISystem * > m_ActiveMenuList;
	CUtlVector< CGameUISystem * > m_ReleasedMenuList;

	DmeTime_t m_flCurrentTime;
	IGameUIScheme *m_Scheme;	

	CHitArea *m_RequestedKeyFocus;

	bool m_bForceFocusUpdate;
	bool m_bUseGameInputQueue;

	InputContextHandle_t m_hInputContext;

	IGameUISoundPlayback *m_pSoundPlayback;
	CUtlVector< InputEvent_t > m_InputQueue;
	CUtlVector< KeyValues * > m_GameUIEventMainQueue;
	CUtlVector< CUtlString > m_MenuFileNames;
	CUtlMap< CUtlString, void * > m_MenuSoundMap;

	typedef CUtlMap< CUtlString, IGameUIScreenControllerFactory * > ScreenControllerFactoryMap;
	ScreenControllerFactoryMap m_ScreenControllerMap;

	typedef CUtlMap< CUtlString, IGameUIGraphicClassFactory * > GameUIGraphicClassFactoryMap;
	GameUIGraphicClassFactoryMap m_GraphicClassMap;

	bool m_bVisible;

	bool m_bSetReleaseTimer;
	DmeTime_t m_ReleaseStartTime;
	DmeTime_t m_ReleaseTime;

	CGameText *m_pCursorText;
};

extern CGameUISystemMgr *g_pGameUISystemMgrImpl;

bool UtlStringLessFunc( const CUtlString &lhs, const CUtlString &rhs );

#endif // GAMEUISYSTEMMANAGER_H
