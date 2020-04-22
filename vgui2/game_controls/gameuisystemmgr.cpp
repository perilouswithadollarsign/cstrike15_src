//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
		 
#include "gameuisystemmgr.h"
#include "soundsystem/isoundsystem.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "gameuisystem.h"
#include "tier2/fileutils.h"
#include "rendersystem/irenderdevice.h"
#include "rendersystem/irendercontext.h"
#include "inputgameui.h"
#include "gameuisystemsurface.h"
#include "inputsystem/iinputsystem.h"
#include "gamelayer.h"
#include "gameuiscriptsystem.h"
#include "inputsystem/iinputstacksystem.h"
#include "fmtstr.h"
#include "gametext.h"

#undef PlaySound

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static CGameUIDynamicTextures *s_pDynamicTextures = NULL;
void OnRestore( int nChangeFlags )
{
	if ( s_pDynamicTextures != NULL )
	{
		s_pDynamicTextures->OnRestore( nChangeFlags );
	}
}

//
// MISC UTILS
//

class CGameUIMiscUtils : public IGameUIMiscUtils
{
public:
	virtual bool PointTriangleHitTest( Vector2D tringleVert0, Vector2D tringleVert1, Vector2D tringleVert2, Vector2D point )
	{
		return ::PointTriangleHitTest( tringleVert0, tringleVert1, tringleVert2, point );
	}
}
g_GameUIMiscUtils;
IGameUIMiscUtils *g_pGameUIMiscUtils = &g_GameUIMiscUtils;


//-----------------------------------------------------------------------------
// Default game ui sound playback implementation
//-----------------------------------------------------------------------------
class CDefaultGameUISoundPlayback : public IGameUISoundPlayback
{
public:
	void *EmitSound( const char *pSoundName )
	{
		if ( !g_pSoundSystem )
			return NULL;

		// Ensure the sound is valid
		int nSoundIndex = g_pSoundEmitterSystem->GetSoundIndex( pSoundName );
		if ( !g_pSoundEmitterSystem->IsValidIndex( nSoundIndex ) )
		{
			Warning( "Attempted to play invalid sound \"%s\"\n", pSoundName );
			return NULL;
		}

		const char *pSourceFile = g_pSoundEmitterSystem->GetSourceFileForSound( nSoundIndex );
		if ( !Q_stristr( pSourceFile, "game_sounds_ui.txt" ) )
		{
			Warning( "Attempted to play invalid sound \"%s\". This sound must be defined\n"
				"in game_sounds_ui.txt but was defined in \"%s\" instead.\n", pSoundName, pSourceFile );
			return NULL;
		}

		// Pull data from parameters
		CSoundParameters params;
		HSOUNDSCRIPTHASH handle = SOUNDEMITTER_INVALID_HASH;
		if ( !g_pSoundEmitterSystem->GetParametersForSoundEx( pSoundName, handle, params, GENDER_NONE, true ) )
			return NULL;

		if ( !params.soundname[0] )
			return NULL;

		char pFileName[ MAX_PATH ];
		Q_snprintf( pFileName, sizeof(pFileName), "sound/%s", params.soundname );
		CAudioSource *pAudioSource = g_pSoundSystem->FindOrAddSound( pFileName );
		if ( !pAudioSource )
			return NULL;

		// NOTE: We're currently ignoring the sound delay time from the parameters,
		// as well as a bunch of other parameters. Oh well.
		CAudioMixer *pMixer = NULL;
		g_pSoundSystem->PlaySound( pAudioSource, params.volume, &pMixer );
		return pAudioSource;

	}

	void StopSound( void *pSoundHandle )
	{
		if ( !pSoundHandle || !g_pSoundSystem )
			return;

		CAudioSource *pAudioSource = (CAudioSource*)pSoundHandle;
		Assert( pAudioSource );
		CAudioMixer *pMixer = g_pSoundSystem->FindMixer( pAudioSource );
		g_pSoundSystem->StopSound( pMixer );
	}
};

static CDefaultGameUISoundPlayback s_DefaultGameUISoundPlayback;


//-----------------------------------------------------------------------------
// Default implementation of gameui system mgr
//-----------------------------------------------------------------------------
static CGameUISystemMgr s_GameUISystemMgr;
CGameUISystemMgr *g_pGameUISystemMgrImpl = &s_GameUISystemMgr;

void LinkGameControlsLib()
{
	// This function is required for the linker to include CGameUISystemMgr
}

//-----------------------------------------------------------------------------
// Used to allow us to install the game ui system into the list of app systems
// that the client wants the engine to initialize
//-----------------------------------------------------------------------------
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameUISystemMgr, IGameUISystemMgr, GAMEUISYSTEMMGR_INTERFACE_VERSION, s_GameUISystemMgr );


//-----------------------------------------------------------------------------
// shader programs
//-----------------------------------------------------------------------------
static const char s_pVertexShader[] = 
"float4 v4OffsetScale : register( c0 );                                     "
"float4 v4InstanceCount : register( c255 );                                 "
"struct VS_INPUT															"
"{																			"
"	float3 vPos						: POSITION0;							"
"	float4 vColor					: COLOR0;								"
"	float2 vTexCoord				: TEXCOORD0;							"
"};																			"
"																			"
"struct VS_OUTPUT															"
"{																			"
"	float4 vColor					: COLOR0;								"
"	float2 vTexCoord				: TEXCOORD0;							"
"	float4 projPos					: POSITION0;							"
"};																			"
"																			"
"VS_OUTPUT main( const VS_INPUT v )											"
"{																			"
"	VS_OUTPUT o = ( VS_OUTPUT )0;											"
"																			"
"   o.projPos.xy = 2.0f * ( v.vPos.xy - v4OffsetScale.xy ) / ( v4OffsetScale.zw ) - float2( 1.0f, 1.0f );"
"   o.projPos.y *= -1.0f;													"
"	o.projPos.z = 1;		    											"
"	o.projPos.w = 1;														"
"	o.vColor = v.vColor;													"
"   o.vTexCoord = v.vTexCoord;                                              "
"	return o;																"
"}																			"
"";

static const char s_pPixelShader[] = 
"struct PS_INPUT															"
"{																			"
"	float4 vColor					: COLOR0;								"
"	float2 vTexCoord				: TEXCOORD0;							"
"};																			"
"																			"
"sampler BaseTextureSampler		: register( s0 );                           "
"float4 main( const PS_INPUT i ) : COLOR									"
"{																			"
"   return i.vColor * tex2D( BaseTextureSampler, i.vTexCoord );				"
"}																			"
"";

static RenderInputLayoutField_t s_pGameUILayout[] =
{
	DEFINE_PER_VERTEX_FIELD( 0, "position", 0, GameUIVertex_t, m_vecPosition )
	DEFINE_PER_VERTEX_FIELD( 0, "color",	0, GameUIVertex_t, m_color )
	DEFINE_PER_VERTEX_FIELD( 0, "texcoord", 0, GameUIVertex_t, m_vecTexCoord )
};



IScriptManager *g_pScriptManager;


//-----------------------------------------------------------------------------
// Inherited from IAppSystem
//-----------------------------------------------------------------------------
bool CGameUISystemMgr::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	g_pScriptManager = (IScriptManager *)factory( VSCRIPT_INTERFACE_VERSION, NULL );

	if ( !g_pScriptManager )
		return false;

	return true;
}

void CGameUISystemMgr::Disconnect()
{
	g_pScriptManager = NULL;
	BaseClass::Disconnect();
}

void *CGameUISystemMgr::QueryInterface( const char *pInterfaceName )
{
	if (!Q_strncmp(	pInterfaceName, GAMEUISYSTEMMGR_INTERFACE_VERSION, Q_strlen( GAMEUISYSTEMMGR_INTERFACE_VERSION ) + 1))
		return (IGameUISystemMgr*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Less function for use with CUtlMap and CUtlString keys
//-----------------------------------------------------------------------------
bool UtlStringLessFunc( const CUtlString &lhs, const CUtlString &rhs )
{
	return ( Q_stricmp( lhs, rhs ) < 0 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameUISystemMgr::CGameUISystemMgr() :
	m_MenuSoundMap( UtlStringLessFunc ),
	m_ScreenControllerMap( UtlStringLessFunc ),
	m_GraphicClassMap( UtlStringLessFunc )
{
	m_hInputLayout = RENDER_INPUT_LAYOUT_INVALID;
	m_hVertexShader = RENDER_SHADER_HANDLE_INVALID;
	m_hPixelShader = RENDER_SHADER_HANDLE_INVALID;
	m_hConstBuffer = CONSTANT_BUFFER_HANDLE_INVALID;

	m_Viewport.x = 0;
	m_Viewport.y = 0;
	m_Viewport.width = 0;
	m_Viewport.height = 0;
	m_nWindowWidth = 0;
	m_nWindowHeight = 0;

	m_flCurrentTime = DmeTime_t(0);
	m_Scheme = NULL;

	m_RequestedKeyFocus = NULL;

	m_bForceFocusUpdate = false;
	m_bUseGameInputQueue = false;

	m_hInputContext = INPUT_CONTEXT_HANDLE_INVALID;

	m_pSoundPlayback = &s_DefaultGameUISoundPlayback;
	
	m_bVisible = true;

	m_bSetReleaseTimer = false;
	m_ReleaseStartTime = DmeTime_t(0);
	m_ReleaseTime = DmeTime_t(0);

	if ( !g_pGameUISystemMgr )
		g_pGameUISystemMgr = this;

	m_pCursorText = NULL;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CGameUISystemMgr::~CGameUISystemMgr()
{
	if ( m_pCursorText )
	{
		delete m_pCursorText;
	}
}


//-----------------------------------------------------------------------------
// Initialize 
//-----------------------------------------------------------------------------
static AppSystemInfo_t s_pDependencies[] = 
{
	{ "vstdlib.dll",	EVENTSYSTEM_INTERFACE_VERSION },
	{ "vscript.dll",	VSCRIPT_INTERFACE_VERSION },
	{ "inputsystem.dll",	INPUTSYSTEM_INTERFACE_VERSION },
	{ "inputsystem.dll",	INPUTSTACKSYSTEM_INTERFACE_VERSION },
	{ "soundemittersystem.dll",	SOUNDEMITTERSYSTEM_INTERFACE_VERSION },
	{ NULL, NULL }
};

const AppSystemInfo_t* CGameUISystemMgr::GetDependencies()
{
	return s_pDependencies;
}


//-----------------------------------------------------------------------------
// Initialize 
//-----------------------------------------------------------------------------
InitReturnVal_t CGameUISystemMgr::Init( )
{
	m_bForceFocusUpdate = false;
	m_bUseGameInputQueue = false;
	m_hInputLayout = RENDER_INPUT_LAYOUT_INVALID;
	m_hInputContext = INPUT_CONTEXT_HANDLE_INVALID;
	m_pSoundPlayback = &s_DefaultGameUISoundPlayback;

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	g_pInputGameUI->Init();
	if ( g_pRenderDevice )
	{
		m_hInputLayout = g_pRenderDevice->CreateInputLayout( "GameUILayout", ARRAYSIZE( s_pGameUILayout ), s_pGameUILayout );
		m_hVertexShader = g_pRenderDevice->CreateShader( RENDER_VERTEX_SHADER, s_pVertexShader, sizeof(s_pVertexShader), g_pRenderDevice->GetShaderVersionString(RENDER_VERTEX_SHADER) );
		m_hPixelShader = g_pRenderDevice->CreateShader( RENDER_PIXEL_SHADER, s_pPixelShader, sizeof(s_pPixelShader), g_pRenderDevice->GetShaderVersionString(RENDER_PIXEL_SHADER) );
		m_hConstBuffer = g_pRenderDevice->CreateConstantBuffer( 4 * sizeof( float ) );
		Assert( m_hVertexShader != RENDER_SHADER_HANDLE_INVALID );
		Assert( m_hPixelShader != RENDER_SHADER_HANDLE_INVALID );
	}

	g_pGameUISystemSurface->Init();

	// Tell the input system to generate UI events
	g_pInputSystem->AddUIEventListener();

	m_Scheme = NULL;
	SetViewportSize( 1024, 768 );
	SetWindowSize( 1024, 768 );

	m_bVisible = true;
	m_RequestedKeyFocus = NULL;

	m_flCurrentTime = DmeTime_t(0);

	m_bSetReleaseTimer = false;
	m_ReleaseTime = DmeTime_t(0);

	
	s_pDynamicTextures = new CGameUIDynamicTextures;

	// Load in a default scheme
	if (!g_pGameUISchemeManager->LoadSchemeFromFile( "resource/BoxRocket.res", "GameUIDefaultScheme" ))
	{
		Assert( 0 );
	}

	// Dynamic textures not yet supported for source 2
	if ( g_pMaterialSystem )
	{
		g_pMaterialSystem->AddRestoreFunc( OnRestore );
	}

	return INIT_OK;
}

//-----------------------------------------------------------------------------
// Init any render targets needed by the UI.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::InitRenderTargets()
{
	s_pDynamicTextures->InitRenderTargets();
}

IMaterialProxy *CGameUISystemMgr::CreateProxy( const char *proxyName )
{
	if ( s_pDynamicTextures )
	{
		return s_pDynamicTextures->CreateProxy( proxyName );
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Shutdown 
//-----------------------------------------------------------------------------
void CGameUISystemMgr::Shutdown()
{
	if ( g_pMaterialSystem )
	{
		g_pMaterialSystem->RemoveRestoreFunc( OnRestore );
	}
	
	if ( s_pDynamicTextures )
	{
		delete s_pDynamicTextures;
	}

	if ( g_pRenderDevice )
	{
		g_pRenderDevice->DestroyConstantBuffer( m_hConstBuffer );
		g_pRenderDevice->DestroyShader( RENDER_PIXEL_SHADER, m_hPixelShader );
		g_pRenderDevice->DestroyShader( RENDER_VERTEX_SHADER, m_hVertexShader );
		g_pRenderDevice->DestroyInputLayout( m_hInputLayout );
		m_hInputLayout = RENDER_INPUT_LAYOUT_INVALID;
	}

	// release all menus.
	ReleaseAllGameUIScreens();

	g_pGameUISystemSurface->Shutdown();

	// Tell the input system we no longer need UI events
	g_pInputSystem->RemoveUIEventListener();
	
	BaseClass::Shutdown();
}

//-----------------------------------------------------------------------------
// Set visibility of the entire gameui.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::SetGameUIVisible( bool bVisible )
{
	m_bVisible = bVisible;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameUISystemMgr::UseGameInputSystemEventQueue( bool bEnable )
{
	m_bUseGameInputQueue = bEnable;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameUISystemMgr::RegisterInputEvent( const InputEvent_t &iEvent )
{
	Assert( m_bUseGameInputQueue );
	m_InputQueue.AddToTail( iEvent );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameUISystemMgr::SendEventToAllScreens( KeyValues *kvGlobalEvent )
{
	KeyValues::AutoDelete autodelete( kvGlobalEvent );

	for ( int j = 0; j < m_ActiveMenuList.Count(); ++j )
	{
		if ( m_bVisible && m_ActiveMenuList[j]->Definition().GetVisible() )
		{
			m_ActiveMenuList[j]->ExecuteScript( kvGlobalEvent );
		}	
	}
}

void CGameUISystemMgr::PostEventToAllScreens( KeyValues *kvGlobalEvent )
{
	m_GameUIEventMainQueue.AddToTail( kvGlobalEvent );
}


//-----------------------------------------------------------------------------
//  Return true if any menu is visible.
//-----------------------------------------------------------------------------
bool CGameUISystemMgr::IsMenuVisible() const
{
	if ( !m_bVisible )
		return false;

	int nCount = m_ActiveMenuList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_ActiveMenuList[i] )
		{
			if ( m_ActiveMenuList[i]->Definition().GetVisible() )
				return true;
		}	
	}
	return false;
}

//-----------------------------------------------------------------------------
// Specifies the input context handle to use to allow it to interoperate
// with other input clients
//-----------------------------------------------------------------------------
void CGameUISystemMgr::SetInputContext( InputContextHandle_t hInputContext )
{
	m_hInputContext = hInputContext;

	if ( m_hInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pInputStackSystem->EnableInputContext( m_hInputContext, IsMenuVisible() );
		g_pInputStackSystem->SetCursorIcon( m_hInputContext, g_pInputSystem->GetStandardCursor( INPUT_CURSOR_ARROW ) );
	}
}


//-----------------------------------------------------------------------------
// Returns the input context to use to control the cursor
//-----------------------------------------------------------------------------
InputContextHandle_t CGameUISystemMgr::GetInputContext( ) const
{
	return m_hInputContext;
}


//-----------------------------------------------------------------------------
// Allows you to control sound playback
//-----------------------------------------------------------------------------
void CGameUISystemMgr::SetSoundPlayback( IGameUISoundPlayback *pPlayback )
{
	m_pSoundPlayback = pPlayback ? pPlayback : &s_DefaultGameUISoundPlayback;
}


//-----------------------------------------------------------------------------
// Assign the scheme
// This is because the scheme is needed to load text ui elements
// so the def has to be able to assign it.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::SetScheme( IGameUIScheme * scheme )
{
	if ( m_Scheme && m_Scheme != scheme )
	{
		Warning( "Warning game menus do not all share the same scheme, text might look strange or fail to display.\n" );
	}
	m_Scheme = scheme;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IGameUIScheme * CGameUISystemMgr::GetCurrentScheme()
{
	// Currently the rendersystemtest text is being loaded with no scheme
	return m_Scheme;
}

//-----------------------------------------------------------------------------
// Load a game UI into memory.
// Return the menu id.
//-----------------------------------------------------------------------------
IGameUISystem * CGameUISystemMgr::LoadGameUIScreen( KeyValues *kvScreenLoadSettings )
{
	if ( !kvScreenLoadSettings )
		return NULL;

	// Make a copy for safe modifications
	kvScreenLoadSettings = kvScreenLoadSettings->MakeCopy();
	KeyValues::AutoDelete autodelete_kvScreenLoadSettings( kvScreenLoadSettings );

	char const *szName = kvScreenLoadSettings->GetName();
	CFmtStr sFilename( "vguiedit/%s.gui", szName );
	const char *pFilename = sFilename;

	// Current menu
	IGameUISystem *pPreviousScreen = NULL;
	if ( m_ActiveMenuList.Count() )
	{
		pPreviousScreen = m_ActiveMenuList[ m_ActiveMenuList.Count() - 1 ];
	}

	// Always load a new instance of the menu
	CGameUISystem *pNewScreen = NULL;
	if ( !pNewScreen )
	{
		bool bSuccess = false;
		if ( szName && *szName )
		{
			CUtlBuffer buf( 0, 0, IsPlatformX360() ? 0 : CUtlBuffer::TEXT_BUFFER );
			if ( IsPlatformX360() )
			{
				buf.ActivateByteSwapping( true );
			}
			
			bSuccess = g_pFullFileSystem->ReadFile( pFilename, "GAME", buf );
			if ( bSuccess )
			{
				CGameUISystem *pGameUI = new CGameUISystem;
				bSuccess = pGameUI->LoadGameUIDefinition( buf, pFilename );
				if ( bSuccess )
				{
					pNewScreen = pGameUI;

					pGameUI->Init( kvScreenLoadSettings );
				}
				else
				{
					delete pGameUI;
					Warning( "Failed to load game ui file %s\n", pFilename );
				}	
			}
			else
			{
				Warning( "Failed to read game ui file %s\n", pFilename );
			}	
		}
		
		// Failed to read or load file. Load an empty ui instead.
		if ( !bSuccess )
		{
			CGameUISystem *pGameUI = new CGameUISystem;
			// for the name use the menu name of the file we tried to load
			char pFilenameStripped[ MAX_PATH ];
			int len = Q_strlen( pFilename );
			Q_StripExtension( pFilename, pFilenameStripped, len+1 );

			CUtlVector<char *> words;
			V_SplitString( pFilenameStripped, "/", words );
			int lastWord = words.Count() - 1;
			char *pMenuName = words[lastWord];

			pGameUI->LoadEmptyGameUI( pMenuName );
			pGameUI->Init( kvScreenLoadSettings );
			
			IGameUIScheme *pNewUIScheme = pGameUI->Definition().GetScheme();
			// Empty UI's should never stomp an already existing scheme.
			if ( !m_Scheme )
			{
				m_Scheme = pNewUIScheme;
			}

			pNewScreen = pGameUI;
		}
	}

	Assert( pNewScreen );
	
	pNewScreen->Definition().InitializeScripts();

	// Set it visible etc.
	pNewScreen->Definition().SetVisible( true );
	pNewScreen->Definition().SetAcceptInput( false );
	
	kvScreenLoadSettings->SetName( "OnLoad" );
	kvScreenLoadSettings->SetInt( "scripthandle", pNewScreen->GetScriptHandle() );
	pNewScreen->ExecuteScript( kvScreenLoadSettings );
	
	pNewScreen->Definition().SetAcceptInput( true );

	// Stack operation:
	char const *szDefaultStackOperation = "removeall";
	char const *szStackOperation = kvScreenLoadSettings->GetString( "stack", szDefaultStackOperation );
	if ( Q_stricmp( "append", szStackOperation ) != 0 )
	{
		// Take the old stack and move it into the m_ReleasedMenuList
		for ( int i = 0; i < m_ActiveMenuList.Count(); ++i )
		{
			m_ReleasedMenuList.AddToTail( m_ActiveMenuList[i] );
		}

		// Tell the previous stack to exit
		KeyValues *kvEventOnExit = new KeyValues( "OnExit");
		KeyValues::AutoDelete autodelete_kvEventOnExit( kvEventOnExit );
		for ( int i = 0; i < m_ReleasedMenuList.Count(); ++i )
		{
			m_ReleasedMenuList[i]->Definition().SetAcceptInput( false );
			m_ReleasedMenuList[i]->ExecuteScript( kvEventOnExit );

			// null out requested focus for a released panel 
			if ( m_RequestedKeyFocus && m_ReleasedMenuList[i]->Definition().HasGraphic( m_RequestedKeyFocus ) )
			{
				m_RequestedKeyFocus = NULL;
			}
		}
		m_ReleaseTime = DmeTime_t(0);
		m_bSetReleaseTimer = true;

		m_ActiveMenuList.RemoveAll();
	}

	// Push the new screen onto the stack
	m_ActiveMenuList.AddToTail( pNewScreen );

	// Init the newly loaded menu
	// ? not sure if we would want to merge load and init ?
	// we may control execution of scripts with passed in keyvalues
	kvScreenLoadSettings->SetName( "OnInit" );
	pNewScreen->ExecuteScript( kvScreenLoadSettings );


	return pNewScreen;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::ReleaseAllGameUIScreens()
{
	while ( m_ActiveMenuList.Count() )
	{
		CGameUISystem *pTopScreen = m_ActiveMenuList.Tail();
		m_ActiveMenuList.RemoveMultipleFromTail( 1 );
		
		// need to release at safe point: pTopScreen->Release();
		DevMsg( "CGameUISystemMgr scheduled screen %p for release\n", pTopScreen );
		m_ReleasedMenuList.AddToTail( pTopScreen );

		m_ReleaseTime = DmeTime_t(0);
		m_bSetReleaseTimer = true;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::OnScreenReleased( CGameUISystem *pScreen )
{
	// Assuming that all the logic of transferring context/focus
	// from the released screen already happened
	if ( m_ActiveMenuList.FindAndRemove( pScreen ) )
	{
		DevWarning( "CGameUISystemMgr::OnScreenReleased( %p ) while screen is active!\n", pScreen );
	}

	// null out requested focus for a released panel 
	if ( m_RequestedKeyFocus && pScreen->Definition().HasGraphic( m_RequestedKeyFocus ) )
	{
		m_RequestedKeyFocus = NULL;
	}
}


//-----------------------------------------------------------------------------
// Process input.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::RunFrame()
{
	// All released menus are removed after a few seconds to allow them to clean up.
	if ( m_ReleaseTime > DmeTime_t( 5.0 ) && m_ReleasedMenuList.Count() )
	{
		for ( int i = 0; i < m_ReleasedMenuList.Count(); ++i )
		{
			CGameUISystem *&pReleasedScreen = m_ReleasedMenuList[i];
			pReleasedScreen->Release();	
			pReleasedScreen = NULL;
		}
		m_ReleasedMenuList.RemoveAll();
	}

	//
	// MAIN UPDATE LOOP
	//
	{	
		// Regenerate if we should.
		if ( s_pDynamicTextures )
		{
			s_pDynamicTextures->RegenerateTexture( 0 );
		}
		
		
		// Update keyfocused and reset mouse and key states.
		g_pInputGameUI->RunFrame();
		m_RequestedKeyFocus = NULL;

		// Generate all gameui event messages
		// Run scripts to handle them.
		for ( int i = 0; i < m_GameUIEventMainQueue.Count(); ++i )
		{	
			KeyValues *kvEvent = m_GameUIEventMainQueue[i];
			KeyValues::AutoDelete autodelete( kvEvent );

			for ( int j = 0; j < m_ActiveMenuList.Count(); ++j )
			{
				if ( m_bVisible && m_ActiveMenuList[j]->Definition().GetVisible() )
				{
					m_ActiveMenuList[j]->ExecuteScript( kvEvent );
				}	
			}
		}
		m_GameUIEventMainQueue.RemoveAll();

		// Generate all input messages
		// this will generate all key and mouse events
		
		const InputEvent_t* pEvents = NULL;
		int nEventCount = 0;
		if ( m_bUseGameInputQueue )
		{
			nEventCount = m_InputQueue.Count();
			pEvents = m_InputQueue.Base();		
		}
		else
		{
			nEventCount = g_pInputSystem->GetEventCount();
			pEvents = g_pInputSystem->GetEventData();	
		}

		for ( int i = 0; i < nEventCount; ++i )
		{	
			InputGameUIHandleInputEvent( pEvents[i] );	
		}

		m_InputQueue.RemoveAll();

		// Specifically post the current mouse position as a message
		g_pInputGameUI->PostCursorMessage();
		if ( m_bForceFocusUpdate )
		{
			g_pInputGameUI->ForceInputFocusUpdate();
			m_bForceFocusUpdate = false;
		}

		g_pInputGameUI->ProcessEvents();

		// Run scripts.
		KeyValues *kvEventOnUpdate = new KeyValues( "OnUpdate" );
		KeyValues::AutoDelete autodelete_kvEventOnUpdate( kvEventOnUpdate );
		for ( int i = 0; i < m_ActiveMenuList.Count(); ++i )
		{
			if ( m_ActiveMenuList[i]->Definition().GetVisible() )
			{
				m_ActiveMenuList[i]->ExecuteScript( kvEventOnUpdate );
			}	
		}
		for ( int i = 0; i < m_ReleasedMenuList.Count(); ++i )
		{
			if ( m_ReleasedMenuList[i]->Definition().GetVisible() )
			{
				m_ReleasedMenuList[i]->ExecuteScript( kvEventOnUpdate );
			}	
		}
		
	}

	// FIXME: Need better logic here.
	// Our input context is enabled if any menus are visible
	if ( m_hInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pInputStackSystem->EnableInputContext( m_hInputContext, IsMenuVisible() );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameUISystemMgr::RegisterScreenControllerFactory( char const *szControllerName, IGameUIScreenControllerFactory *pFactory )
{
	m_ScreenControllerMap.InsertOrReplace( szControllerName, pFactory );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IGameUIScreenControllerFactory *CGameUISystemMgr::GetScreenControllerFactory( char const *szControllerName )
{
	ScreenControllerFactoryMap::IndexType_t idx = m_ScreenControllerMap.Find( szControllerName );
	if ( idx != m_ScreenControllerMap.InvalidIndex() )
		return m_ScreenControllerMap[idx];
	else
		return NULL;
}


//-----------------------------------------------------------------------------
// Registers a factory to create a graphic class type (rect, text, hitarea)
//-----------------------------------------------------------------------------
void CGameUISystemMgr::RegisterGraphicClassFactory( char const *szGraphicClassName, IGameUIGraphicClassFactory *pFactory )
{
	m_GraphicClassMap.InsertOrReplace( szGraphicClassName, pFactory );
}

//-----------------------------------------------------------------------------
// Get the factory to create an instance of a graphic class ( rect, text, hitarea)
//-----------------------------------------------------------------------------
IGameUIGraphicClassFactory *CGameUISystemMgr::GetGraphicClassFactory( char const *szGraphicClassName )
{
	GameUIGraphicClassFactoryMap::IndexType_t idx = m_GraphicClassMap.Find( szGraphicClassName );
	if ( idx != m_GraphicClassMap.InvalidIndex() )
		return m_GraphicClassMap[idx];
	else
		return NULL;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IGameUISystemSurface * CGameUISystemMgr::GetSurface()
{
	return g_pGameUISystemSurface;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IGameUISchemeMgr * CGameUISystemMgr::GetSchemeMgr()
{
	return g_pGameUISchemeManager;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IGameUIMiscUtils * CGameUISystemMgr::GetMiscUtils()
{
	return g_pGameUIMiscUtils;
}

//-----------------------------------------------------------------------------
// ConCommand to trigger the menu for testing purposes
//-----------------------------------------------------------------------------
CON_COMMAND_F( ui_setmenus_hidden, "Hide the game ui", FCVAR_DEVELOPMENTONLY )
{
	g_pGameUISystemMgrImpl->SetGameUIVisible( false );	
}

//-----------------------------------------------------------------------------
// Sets the current game UI menu. 
//-----------------------------------------------------------------------------
CON_COMMAND_F( ui_setmenu, "Pass in a name; load it as the current game menu.", FCVAR_DEVELOPMENTONLY )
{
	if ( args.ArgC() != 2 )
	{
		Msg( "ui_setmenu <name>\n" );
		return;
	}

	const char *szName = args[ 1 ];
	g_pGameUISystemMgrImpl->LoadGameUIScreen( KeyValues::AutoDeleteInline( new KeyValues( szName ) ) );
}


//-----------------------------------------------------------------------------
// Source 1
// Draw each gameui, note game uis render from back to front. 
// A newly loaded gameui goes in the front.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::Render( const Rect_t &viewport, DmeTime_t flCurrentTime )
{
	if ( m_bSetReleaseTimer )
	{
		m_ReleaseStartTime = flCurrentTime;
		m_bSetReleaseTimer = false;
	}
	else
	{
		m_ReleaseTime =	flCurrentTime - m_ReleaseStartTime;
	}
	m_flCurrentTime = flCurrentTime;

	if ( !m_bVisible )
		return;



	// Check if the viewport size has changed.
	if ( ( m_Viewport.width != viewport.width ) || 
		( m_Viewport.height != viewport.height ) )
	{	
		m_Viewport.width = viewport.width;
		m_Viewport.height = viewport.height;

		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		int nWidth, nHeight;
		pRenderContext->GetWindowSize( nWidth, nHeight );
		SetWindowSize( nWidth, nHeight );

		// Update stage render transforms so the font size loaded is correct.
		for ( int i = 0; i < m_ActiveMenuList.Count(); ++i )
		{
			m_ActiveMenuList[i]->Definition().UpdateAspectRatio( viewport );		
		}

		// Reload the fonts!
		g_pGameUISystemSurface->ResetFontCaches();
	}


	for ( int i = 0; i < m_ActiveMenuList.Count(); ++i )
	{
		m_ActiveMenuList[i]->Render( viewport );		
	}

	// Render menus that are going away last (so they are in front)
	for ( int i = 0; i < m_ReleasedMenuList.Count(); ++i )
	{
		m_ReleasedMenuList[i]->Render( viewport );		
	}	
}

//-----------------------------------------------------------------------------
// Source 2
//-----------------------------------------------------------------------------
void CGameUISystemMgr::Render( IRenderContext *pRenderContext, PlatWindow_t hWnd, const Rect_t &viewport, DmeTime_t flCurrentTime )
{
	// Check if the viewport size has changed.
	if ( ( m_Viewport.width != viewport.width ) || 
		( m_Viewport.height != viewport.height ) )
	{
		// Reload the fonts!
		m_Viewport.width = viewport.width;
		m_Viewport.height = viewport.height;

		int nWidth, nHeight;
		Plat_GetWindowClientSize( hWnd, &nWidth, &nHeight );
		SetWindowSize( nWidth, nHeight );

		// Update stage render transforms so the font size loaded is correct.
		for ( int i = 0; i < m_ActiveMenuList.Count(); ++i )
		{
			m_ActiveMenuList[i]->Definition().UpdateAspectRatio( viewport );		
		}
	
		g_pGameUISystemSurface->ResetFontCaches();
	}

	m_flCurrentTime = flCurrentTime;

	for ( int i = 0; i < m_ActiveMenuList.Count(); ++i )
	{
		m_ActiveMenuList[i]->Render( pRenderContext, viewport );
	}
}

//-----------------------------------------------------------------------------
// Get the current graphic that can accept mouse input and is under the mouse.
//-----------------------------------------------------------------------------
CHitArea *CGameUISystemMgr::GetMouseFocus()
{
	return g_pInputGameUI->GetMouseFocus();
}

//-----------------------------------------------------------------------------
// Get the graphic that can accept mouse input and is under the mouse.
// We check from front to back.
//-----------------------------------------------------------------------------
CHitArea *CGameUISystemMgr::GetMouseFocus( int x, int y )
{
	for ( int i = m_ActiveMenuList.Count()-1; i >=0; --i )
	{
		CHitArea *pGraphic = m_ActiveMenuList[i]->Definition().GetMouseFocus( x, y );
		if ( pGraphic )
		{
			return pGraphic;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Get the graphic that can accept key input
//-----------------------------------------------------------------------------
CHitArea *CGameUISystemMgr::GetKeyFocus()
{
	return g_pInputGameUI->GetKeyFocus();
}

//-----------------------------------------------------------------------------
// Get the graphic that would like to accept key input
//-----------------------------------------------------------------------------
CHitArea *CGameUISystemMgr::GetRequestedKeyFocus()
{
	return m_RequestedKeyFocus;
}


//-----------------------------------------------------------------------------
// Purpose: Request key focus. If focus is changed play a sound.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::RequestKeyFocus( CHitArea *pGraphic, KeyValues *args )
{
	// Find the menu this graphic is in
	for ( int i = m_ActiveMenuList.Count()-1; i >=0; --i )
	{
		if ( m_ActiveMenuList[i]->Definition().GetVisible() )
		{
			if ( m_ActiveMenuList[i]->Definition().HasGraphic( pGraphic ) )
			{
				if ( !m_ActiveMenuList[i]->Definition().CanAcceptInput() )
				{
					Warning( "Key Focus requested for graphic %d that does not have input on in its menu!", pGraphic->GetName() );
					return;
				}
			}
		}
	}

	// Has nobody asked for it?
	if ( m_RequestedKeyFocus == NULL )
	{
		if ( pGraphic->IsGroup() )
		{
			// Find a hitarea inside.
			CGraphicGroup *pGroup = (CGraphicGroup *)pGraphic;
			CHitArea *pFocusGraphic = pGroup->GetKeyFocusRequestGraphic();
			if ( pFocusGraphic )
			{
				m_RequestedKeyFocus = pFocusGraphic;
				PlayMenuSound( args->GetString( "sound" ) );
			}

		}
		else
		{
			m_RequestedKeyFocus = pGraphic;
			PlayMenuSound( args->GetString( "sound" ) );
		}	
	}	
}


//-----------------------------------------------------------------------------
// Purpose: Obtain the screen size.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::GetScreenSize( int &nWide, int &nTall )
{
	if ( !g_pMaterialSystem )
	{
		nWide = m_Viewport.width;
		nTall = m_Viewport.height;
	}
	else
	{
		Rect_t viewport;
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->GetViewport( viewport.x, viewport.y, nWide, nTall );
	}
}

//-----------------------------------------------------------------------------
// Assumes all menus are full screen and same size right now.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::GetScreenHeightForFontLoading( int &nTall )
{
	Vector2D stageSize;
	if ( m_ActiveMenuList.Count() )
	{
		 m_ActiveMenuList[0]->Definition().GetMaintainAspectRatioStageSize( stageSize );
		 nTall = stageSize.y;
	}
	else
	{	
		int nWide;
		GetScreenSize( nWide, nTall );
	}	
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
DmeTime_t CGameUISystemMgr::GetTime()
{
	return m_flCurrentTime;
}

//-----------------------------------------------------------------------------
// Purpose: If the hit graphic'c menu is set to have mouse focus change keyboard focus, update kb focus.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::OnMouseFocusGained( CHitArea *mouseFocus )
{
	for ( int i = m_ActiveMenuList.Count()-1; i >= 0; --i )
	{
		if ( m_ActiveMenuList[i]->Definition().GetVisible() &&
			m_ActiveMenuList[i]->Definition().HasGraphic( mouseFocus ) )
		{
			if ( m_ActiveMenuList[i]->Definition().IsMouseFocusEqualToKeyboardFocus() )
			{
				m_RequestedKeyFocus = mouseFocus;		
			}
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: If the hit graphic'c menu is set to have mouse focus change keyboard focus, update kb focus.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::OnMouseFocusLost( CHitArea *mouseFocus )
{
	for ( int i = m_ActiveMenuList.Count()-1; i >= 0; --i )
	{
		if ( m_ActiveMenuList[i]->Definition().GetVisible() &&
			m_ActiveMenuList[i]->Definition().HasGraphic( mouseFocus ) )
		{
			if ( m_ActiveMenuList[i]->Definition().IsMouseFocusEqualToKeyboardFocus() )
			{
				m_RequestedKeyFocus = NULL;		
			}
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// This fxn was called because nothing in the ui has keyfocus and a key was typed.
//-----------------------------------------------------------------------------
bool CGameUISystemMgr::OnKeyCodeTyped( const ButtonCode_t &code )
{
	if ( code == KEY_TAB )
	{
		for ( int i = m_ActiveMenuList.Count()-1; i >= 0; --i )
		{
			CHitArea *pGraphic = m_ActiveMenuList[i]->Definition().GetNextFocus( g_pInputGameUI->GetKeyFocus() );
			if ( pGraphic )
			{
				RequestKeyFocus( pGraphic );
				//pGraphic->OnGainKeyFocus(); // hack to make on focus anims and script events play
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CGameUISystemMgr::OnGameGraphicScriptEvent( CGameGraphic *pGraphic, KeyValues *kvEvent )
{
	// Find the menu that owns this graphic
	for ( int i = m_ActiveMenuList.Count()-1; i >= 0; --i )
	{
		if ( m_ActiveMenuList[i]->Definition().HasGraphic( pGraphic ) )
		{
			CUtlString sGraphicName;
			m_ActiveMenuList[i]->Definition().BuildScopedGraphicName( sGraphicName, pGraphic );
			kvEvent->SetString( "graphic", sGraphicName.Get() );
			return m_ActiveMenuList[i]->ExecuteScript( kvEvent );
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// This fxn was called because nothing in the ui has keyfocus and a key code was typed.
//-----------------------------------------------------------------------------
bool CGameUISystemMgr::OnKeyTyped( const wchar_t &unichar )
{
	return false;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::PlayMenuSound( const char *pSoundName )
{
	if ( pSoundName == NULL || pSoundName[ 0 ] == '\0' )
		return;

	int index = m_MenuSoundMap.Find( pSoundName );
	if ( index != m_MenuSoundMap.InvalidIndex() )
	{  
		m_pSoundPlayback->StopSound( m_MenuSoundMap[index] );
	}
	else
	{
		index = m_MenuSoundMap.Insert( pSoundName, NULL );
	}

	m_MenuSoundMap[index] = m_pSoundPlayback->EmitSound( pSoundName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::StopMenuSound( const char *pSoundName )
{
	int index = m_MenuSoundMap.Find( pSoundName );
	if ( index == m_MenuSoundMap.InvalidIndex() )
		return;

	if ( m_MenuSoundMap[index] )
	{
		m_pSoundPlayback->StopSound( m_MenuSoundMap[index] );
		m_MenuSoundMap[index] = NULL;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::SetWindowSize( int nWidth, int nHeight )
{
	m_nWindowWidth = nWidth;
	m_nWindowHeight = nHeight;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::GetWindowSize( int &nWidth, int &nHeight )
{
	nWidth = m_nWindowWidth;
	nHeight = m_nWindowHeight;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::SetViewportSize( int nWidth, int nHeight )
{
	m_Viewport.width = nWidth;
	m_Viewport.height = nHeight;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::GetViewportSize( int &nWidth, int &nHeight )
{
	nWidth = m_Viewport.width;
	nHeight = m_Viewport.height;
}


//-----------------------------------------------------------------------------
// Create an entry for this alias
//-----------------------------------------------------------------------------
void CGameUISystemMgr::InitImageAlias( const char *pAlias )
{
	if ( s_pDynamicTextures == NULL )
	{
		s_pDynamicTextures = new CGameUIDynamicTextures;
	}

	if ( !pAlias || !*pAlias )
		return;


	ImageAliasData_t *pImageData = s_pDynamicTextures->GetImageAliasData( pAlias );
	if ( s_pDynamicTextures->IsErrorImageAliasData( pImageData ) )
	{
		// If we didn't have an entry for this alias populate it now.
		ImageAliasData_t *pImageData = GetImageAliasData( "errorImageAlias" );
		ImageAliasData_t imageData;
		imageData.m_XPos = pImageData->m_XPos;
		imageData.m_YPos = pImageData->m_YPos;
		imageData.m_Width = pImageData->m_Width;
		imageData.m_Height = pImageData->m_Height;
		imageData.m_szBaseTextureName = pImageData->m_szBaseTextureName;
		imageData.m_Material = pImageData->m_Material;
		imageData.m_bIsInSheet = pImageData->m_bIsInSheet;
		imageData.m_nNodeIndex = pImageData->m_nNodeIndex;
		s_pDynamicTextures->SetImageEntry( pAlias, imageData );
	}

	pImageData = s_pDynamicTextures->GetImageAliasData( pAlias );
	pImageData->m_nRefCount++;
}

//-----------------------------------------------------------------------------
// Associate this image alias name with a .vtf texture.
// This fxn loads the texture into the GameControls shader and makes a material 
// for you
//-----------------------------------------------------------------------------
void CGameUISystemMgr::LoadImageAliasTexture( const char *pAlias, const char *pBaseTextureName )
{
	if ( s_pDynamicTextures == NULL )
	{
		s_pDynamicTextures = new CGameUIDynamicTextures;
	}

	s_pDynamicTextures->LoadImageAlias( pAlias, pBaseTextureName );
}

//-----------------------------------------------------------------------------
// Release all aliases associated with this menu from the packer
//-----------------------------------------------------------------------------
void CGameUISystemMgr::ReleaseImageAlias(  const char *pAlias )
{
	s_pDynamicTextures->ReleaseImageAlias( pAlias );
}

//-----------------------------------------------------------------------------
// Return the material bound to this image alias.
// If the texture was not found you will see the purple and black checkerboard
// error texture.
//-----------------------------------------------------------------------------
IMaterial *CGameUISystemMgr::GetImageAliasMaterial( const char *pAlias )
{ 
	if ( s_pDynamicTextures == NULL )
		return NULL;

	return s_pDynamicTextures->GetImageAliasMaterial( pAlias );	
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
ImageAliasData_t *CGameUISystemMgr::GetImageAliasData( const char *pAlias )
{
	if ( s_pDynamicTextures == NULL )
		return NULL;

	return s_pDynamicTextures->GetImageAliasData( pAlias );
}


//-----------------------------------------------------------------------------
// Transform this texcoord into sheet coordinates.
// If the image is not in a sheet, does nothing.
//-----------------------------------------------------------------------------
void CGameUISystemMgr::TexCoordsToSheetTexCoords( const char *pAlias, Vector2D texCoords, Vector2D &sheetTexCoords )
{
	ImageAliasData_t *pImageData = GetImageAliasData( pAlias );
	if ( pImageData && pImageData->m_bIsInSheet )
	{
		// Transform texture coords to sheet texture coords.
		int nSheetWidth = 0;
		int nSheetHeight = 0;
		s_pDynamicTextures->GetDynamicSheetSize( nSheetWidth, nSheetHeight );
		Assert( nSheetWidth != 0 );
		Assert( nSheetHeight != 0 );
		float sampleWidth = ( ( (float)pImageData->m_Width - 1 ) / (float)nSheetWidth );
		float sampleHeight = ( ( (float)pImageData->m_Height - 1 ) / (float)nSheetHeight );
		float left = (pImageData->m_XPos + 0.5f) / (float)nSheetWidth;
		float top = (pImageData->m_YPos + 0.5f) / (float)nSheetHeight;

		sheetTexCoords.x = left + ( texCoords.x * sampleWidth );
		sheetTexCoords.y = top  + ( texCoords.y * sampleHeight );

		Assert( sheetTexCoords.x >= 0 );
		Assert( sheetTexCoords.x <= 1 );
		Assert( sheetTexCoords.y >= 0 );
		Assert( sheetTexCoords.y <= 1 );
	}
	else
	{
		sheetTexCoords.x = texCoords.x;
		sheetTexCoords.y = texCoords.y;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::DrawDynamicTexture( const char *pAlias, int x, int y )
{
	if ( s_pDynamicTextures == NULL )
		return;

	s_pDynamicTextures->DrawDynamicTexture( pAlias, x, y );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
Vector2D CGameUISystemMgr::CursorToStage( Vector2D cursorPos )
{
	Vector2D stageCoords;
	Vector2D stageSize;
	m_ActiveMenuList[0]->Definition().GetStageSize( stageSize );

	// cursor is in viewport coords.
	stageCoords.x = cursorPos.x * (stageSize.x/m_Viewport.width) - stageSize.x/2;
	stageCoords.y = cursorPos.y * (stageSize.y/m_Viewport.height) - stageSize.y/2;

	return stageCoords;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::ShowCursorCoords()
{
	if ( !m_pCursorText )
	{
		m_pCursorText = new CGameText( "cursorcoords" );
		Vector2D stageSize;
		m_ActiveMenuList[0]->Definition().GetStageSize( stageSize );
		m_ActiveMenuList[0]->Definition().AddGraphicToLayer( m_pCursorText, SUBLAYER_FONT );
	}

	int x, y;
	g_pInputGameUI->GetCursorPos( x, y );
	Vector2D coords = CursorToStage( Vector2D( x, y ));
	m_pCursorText->SetCenter( coords.x, coords.y );
	static color32 color;
	color.r = rand()%256;
	color.g = rand()%256;
	color.b = rand()%256;
	color.a = 255;
	m_pCursorText->SetColor( color );
	//pText->SetPos( -(stageSize.x/2)+5, (stageSize.y/2) - 20 );
	CFmtStr szCursorMsg( "Cursor Position: %.0f, %.0f", coords.x, coords.y );
	m_pCursorText->SetText( szCursorMsg.Access() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemMgr::ShowGraphicName()
{
	// Find the graphic directly under the cursor.
	CGameGraphic *pGraphic = NULL;
	int x, y;
	g_pInputGameUI->GetCursorPos( x, y );

	for ( int i = m_ActiveMenuList.Count() - 1; i >= 0; --i )
	{
		pGraphic = m_ActiveMenuList[i]->Definition().GetGraphic( x, y );
		if ( pGraphic )
			break;
	}

	if ( pGraphic == NULL )
	{
		if ( m_pCursorText )
		{
			CFmtStr szCursorMsg( "");
			m_pCursorText->SetText( szCursorMsg.Access() );
		}
		return;
	}

	if ( !m_pCursorText )
	{
		m_pCursorText = new CGameText( "cursorcoords" );
		Vector2D stageSize;
		m_ActiveMenuList[0]->Definition().GetStageSize( stageSize );
		m_ActiveMenuList[0]->Definition().AddGraphicToLayer( m_pCursorText, SUBLAYER_FONT );
	}

	
	Vector2D coords = CursorToStage( Vector2D( x, y ));
	m_pCursorText->SetCenter( coords.x, coords.y );
	static color32 color;
	color.r = rand()%256;
	color.g = rand()%256;
	color.b = rand()%256;
	color.a = 255;
	m_pCursorText->SetColor( color );
	//pText->SetPos( -(stageSize.x/2)+5, (stageSize.y/2) - 20 );

	CFmtStr szCursorMsg( "Graphic name: %s", pGraphic->GetName() );
	m_pCursorText->SetText( szCursorMsg.Access() );

	
}











