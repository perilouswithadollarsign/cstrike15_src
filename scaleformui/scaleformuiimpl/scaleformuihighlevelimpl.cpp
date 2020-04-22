//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#include "stdafx.h"
#include "tier1/keyvalues.h"
#include "matchmaking/imatchframework.h"
#include "GFx_AMP.h"

#if defined( _PS3 )
#include "ps3/ps3_console.h"
#include "tls_ps3.h"
#endif

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

using namespace SF::GFx;
using namespace SF::Render;

CON_COMMAND( sf4_meshcache_stats, "Outputs Scaleform 4 mesh cache stats" )
{
	ScaleformUIImpl::m_Instance.DumpMeshCacheStats();
}

bool g_bScaleformIMEDetailedLogging = false;
#ifndef Log_Detailed
#define Log_Detailed( Channel, /* [LoggingMetaData_t *], [Color], Message, */ ... ) do { if (g_bScaleformIMEDetailedLogging) InternalMsg( Channel, LS_MESSAGE, /* [Color], Message, */ ##__VA_ARGS__ );  } while( 0 )
#endif

DECLARE_LOGGING_CHANNEL( LOG_SCALEFORM_IME );

DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_SCALEFORM_IME, "Scaleform IME" );

static XUID NormalizeXuidForAccountID( XUID xuid )
{
	if ( ( xuid & 0xFFFFFFFFull ) == xuid )
		return xuid; // AccountID only only

	CSteamID steamID( xuid );
	if ( steamID.IsValid() && steamID.BIndividualAccount() )
		return steamID.GetAccountID(); // trim upper part of SteamID

	return xuid;
}

void ScaleformUIImpl::InitHighLevelImpl( void )
{
	m_iLastMouseX = -1;
	m_iLastMouseY = -1;

	m_iKeyboardSlot = 0;

	m_bEatPS3MouseEvent = true;
	
	V_memset( m_fJoyValues, 0, sizeof( m_fJoyValues ) );
	V_memset( m_iJoyAxisButtonsDown, 0, sizeof( m_iJoyAxisButtonsDown ) );

	SetDefLessFunc( m_mapUserXuidToAvatar );
	SetDefLessFunc( m_mapItemIdToImage );
	SetDefLessFunc( m_mapImageIdToChromeImage );

#if !defined( NO_STEAM )
	m_bSteamCallbacksConfigured = false;
#endif
	
#if defined( CSTRIKE15 ) && !defined( _X360 )
	// $TODO: Figure out why we aren't properly loading the default texture on Xbox
	// Load the default avatar image bits and store them in a raw buffer
	CUtlBuffer bufFile;
	static const char* cDefaultAvatarImageFileName = "materials/vgui/avatar_default_64" PLATFORM_EXT ".vtf";
	if ( g_pFullFileSystem->ReadFile( cDefaultAvatarImageFileName, NULL, bufFile ) )
	{
		m_pDefaultAvatarTexture = CreateVTFTexture();
#if !defined( _GAMECONSOLE )
		if ( !m_pDefaultAvatarTexture->Unserialize( bufFile ) )
#else
		if ( !m_pDefaultAvatarTexture->UnserializeFromBuffer( bufFile, true, false, false, 0 ) )
#endif
		{
			Warning( "Invalid or corrupt default avatar image (%s)\n", cDefaultAvatarImageFileName );
			DestroyVTFTexture( m_pDefaultAvatarTexture );
			m_pDefaultAvatarTexture = NULL;
		}
	}
	else
	{
		Warning( "Failed to read the default avatar image file (%s)\n", cDefaultAvatarImageFileName );
	}

	// Load the default inventory image bits and store them in a raw buffer
	bufFile.Clear();
	static const char* cDefaultInventoryImageFileName = "materials/vgui/inventory_default" PLATFORM_EXT ".vtf";
	if ( g_pFullFileSystem->ReadFile( cDefaultInventoryImageFileName, NULL, bufFile ) )
	{
		m_pDefaultInventoryTexture = CreateVTFTexture();
#if !defined( _GAMECONSOLE )
		if ( !m_pDefaultInventoryTexture->Unserialize( bufFile ) )
#else
		if ( !m_pDefaultInventoryTexture->UnserializeFromBuffer( bufFile, true, false, false, 0 ) )
#endif
		{
			Warning( "Invalid or corrupt default inventory image (%s)\n", cDefaultInventoryImageFileName );
			DestroyVTFTexture( m_pDefaultInventoryTexture );
			m_pDefaultInventoryTexture = NULL;
		}
	}
	else
	{
		Warning( "Failed to read the default inventory image file (%s)\n", cDefaultInventoryImageFileName );
	}
#endif // CSTRIKE15

	m_CurrentKey = BUTTON_CODE_INVALID;
}

#if !defined( NO_STEAM )
void ScaleformUIImpl::EnsureSteamCallbacksConfigured()
{
	if ( m_bSteamCallbacksConfigured )
		return;
	m_bSteamCallbacksConfigured = true;

	m_CallbackPersonaStateChanged.Register( this, &ScaleformUIImpl::Steam_OnPersonaStateChanged );
	m_CallbackAvatarImageLoaded.Register( this, &ScaleformUIImpl::Steam_OnAvatarImageLoaded );
}

void ScaleformUIImpl::Steam_OnAvatarImageLoaded( AvatarImageLoaded_t *pParam )
{
	if ( pParam )
	{
		AvatarImageReload( pParam->m_steamID.ConvertToUint64(), NULL );
	}
}

void ScaleformUIImpl::Steam_OnPersonaStateChanged( PersonaStateChange_t *pParam )
{
	if ( pParam && ( pParam->m_nChangeFlags & k_EPersonaChangeAvatar ) )
	{
		AvatarImageReload( pParam->m_ulSteamID, NULL );
	}
}
#endif

void ScaleformUIImpl::ShutdownHighLevelImpl( void )
{
	if ( m_pDefaultAvatarTexture )
	{
		DestroyVTFTexture( m_pDefaultAvatarTexture );
		m_pDefaultAvatarTexture = NULL;
	}

	if ( m_pDefaultAvatarImage )
	{
		delete m_pDefaultAvatarImage;
		m_pDefaultAvatarImage = NULL;
	}

	if ( m_pDefaultInventoryTexture )
	{
		DestroyVTFTexture( m_pDefaultInventoryTexture );
		m_pDefaultInventoryTexture = NULL;
	}

	if ( m_pDefaultInventoryImage )
	{
		delete m_pDefaultInventoryImage;
		m_pDefaultInventoryImage = NULL;
	}

	if ( m_pDefaultChromeHTMLImage )
	{
		delete m_pDefaultChromeHTMLImage;
		m_pDefaultChromeHTMLImage = NULL;
	}

#ifdef USE_DEFAULT_INVENTORY_ICON_BACKGROUNDS
	for ( CUtlHashFast< DefaultInventoryIcon_t >::UtlHashFastIterator_t i = m_defaultInventoryIcons.First();  m_defaultInventoryIcons.IsValidIterator( i ); i = m_defaultInventoryIcons.Next( i ) )
	{
		if ( m_defaultInventoryIcons[ i ].m_pTexture )
		{
			DestroyVTFTexture( m_defaultInventoryIcons[ i ].m_pTexture );
			m_defaultInventoryIcons[ i ].m_pTexture = NULL;
		}
		if ( m_defaultInventoryIcons[ i ].m_pImage )
		{
			delete m_defaultInventoryIcons[ i ].m_pImage;
			m_defaultInventoryIcons[ i ].m_pImage = NULL;
		}
	}
	m_defaultInventoryIcons.RemoveAll();
#endif

	m_CurrentKey = BUTTON_CODE_INVALID;
}

void ScaleformUIImpl::ClearCache( void )
{
	// Do not clear the cache straight away. We need to ensure that we are clearing the mesh cache on the 
	// render thread => cache being actually cleared in RenderSlot.
	m_bClearMeshCacheQueued = true;
}

const char* ScaleformUIImpl::CorrectFlashFileName( const char * name )
{

	// make sure the name is long enough to have an extension, but not too long
	int len = V_strlen( name );

	if ( len < 4 || len >= TEMPORARY_BUFFER_SIZE )
	{
		return name;
	}

#if 1

	//
	// Allow -customswf to override directory from which SWF files are loaded
	// otherwise load directly from main resource directory
	//

	static char const *szSwfDirOverride = CommandLine()->ParmValue( "-customswf", ( const char * ) NULL );
	static int nSwfDirOverrideLen = szSwfDirOverride ? V_strlen( szSwfDirOverride ) : 0;
	if ( szSwfDirOverride && *szSwfDirOverride && ( len + nSwfDirOverrideLen + 4 < TEMPORARY_BUFFER_SIZE ) )
	{
		char const *szDirSeparator = strrchr( name, '/' );
		if ( szDirSeparator )
		{
			V_sprintf_safe( m_cTemporaryBuffer, "%.*s%s%s", ( szDirSeparator - name + 1 ), name, szSwfDirOverride, szDirSeparator );
			if ( g_pFullFileSystem->FileExists( m_cTemporaryBuffer, "GAME" ) )
			{
				DevMsg( "-customswf: %s\n", m_cTemporaryBuffer );
				return m_cTemporaryBuffer;
			}
		}
	}

	return name;

#else

	// we only want to continue if the filename is a .swf or a .gfx file
	// otherwise we can just use the original name

	const char* pFirstEXTChar = V_strstr( ".swf.gfx", &name[len-4] );

	if ( !pFirstEXTChar )
	{
		return name;
	}

	// point to the first char of the extension so we
	// can test later
	pFirstEXTChar++;

	// this is a utility variable.
	// we'll use it to point to the actual string we want to
	// use
	const char* namePtr;


	// if we're supposed to try SWF's first, see if an
	// SWF file exists
	if ( m_bTrySWFFirst )
	{
		if ( *pFirstEXTChar == 's' )
		{
			namePtr = name;
		}
		else
		{
			V_strncpy( m_cTemporaryBuffer, name, TEMPORARY_BUFFER_SIZE );
			V_strcpy( &m_cTemporaryBuffer[len-3], "swf" );
			namePtr = m_cTemporaryBuffer;
		}

		if ( g_pFullFileSystem->FileExists( namePtr, "GAME" ) )
		{
			return namePtr;
		}

	}

	// convert filename extension to gfx

	if ( *pFirstEXTChar == 'g' )
	{
		namePtr = name;
	}
	else
	{
		V_strncpy( m_cTemporaryBuffer, name, TEMPORARY_BUFFER_SIZE );
		V_strcpy( &m_cTemporaryBuffer[len-3], "gfx" );
		namePtr = m_cTemporaryBuffer;
	}

	return namePtr;
#endif
}

void ScaleformUIImpl::SendUIEvent( const char* action, const char* eventData, int slot )
{
	IGameEvent * pEvent = m_pGameEventManager->CreateEvent( "sfuievent" );

	if ( pEvent )
	{
		pEvent->SetString( "action", action );
		pEvent->SetString( "data", eventData );
		pEvent->SetInt( "slot", slot );
		m_pGameEventManager->FireEventClientSide( pEvent );
		SFDevMsg("sfuievent action=%s data=%d slot=%d\n", action?action:"", eventData?eventData:"", slot);
	}
}


float ScaleformUIImpl::GetJoyValue( int slot, int stickIndex, int axis )
{
	AssertMsg( SF_SS_SLOT( slot ) < MAX_SLOTS, "Invalid slot index in GetJoyValue" );

	return m_fJoyValues[ JOY_VALUE_INDEX( slot, stickIndex, axis ) ];
}

void ScaleformUIImpl::SetJoyValue( int slot, int stickIndex, int axis, int value )
{
	AssertMsg( SF_SS_SLOT( slot ) < MAX_SLOTS, "Invalid slot index in SetJoyValue" );

	m_fJoyValues[ JOY_VALUE_INDEX( slot, stickIndex, axis ) ] = (float)value / 32768.0f;
}


void ScaleformUIImpl::SetScreenSize( int x, int y )
{
	MEM_ALLOC_CREDIT();
	m_iScreenWidth = x;
	m_iScreenHeight = y;

	SetSlotViewport( SF_FULL_SCREEN_SLOT, 0, 0, x, y );
	SetSlotViewport( SF_RESERVED_CURSOR_SLOT, 0, 0, x, y );
}

void ScaleformUIImpl::SetSingleThreadedMode( bool bSingleThreded )
{
	m_bSingleThreaded = bSingleThreded;

	if ( !m_pRenderHAL )
	{
		Assert( CommandLine()->FindParm( "-noshaderapi" ) ); // For transcoding demos.
		return;
	}

	if ( m_bSingleThreaded )
	{
		m_pRenderHAL->GetTextureManager()->SetRenderThreadIdToCurrentThread();
	}
	else
	{
		// RenderThreadId will be set in RenderSlot - Just reseting for now
		// so that textures are not created on the main thread.
		m_pRenderHAL->GetTextureManager()->ResetRenderThreadId();
	}
}

void ScaleformUIImpl::RunFrame( float time )
{
	m_fTime = time;
		
#ifndef SF_BUILD_SHIPPING	
	if ( m_bPumpScaleformStats )
	{
		AMP::Server::GetInstance().AdvanceFrame();		
	}
#endif
	SNPROF("ScaleformUIImpl::RunFrame");

#ifdef _PS3
	if ( GetTLSGlobals()->bNormalQuitRequested )
	{
			return; // do not disconnect recursively on QUIT
	}
#endif

	UpdateCursorLazyHide( m_fTime );

	UpdateAvatarImages();

	// Removed advance slot from RunFrame. AdvanceSlot is now called just before rendering (fix hud element lagging)
}

bool ScaleformUIImpl::DistributeEvent( Event& event, int slotNumber, bool toAllSlots, bool clearControllerUI )
{
	bool result = false;

	CursorSlot* pCursorSlot = ( CursorSlot* )LockSlotPtr( SF_RESERVED_CURSOR_SLOT );

	if ( pCursorSlot )
	{
		pCursorSlot->m_pMovieView->HandleEvent( event );
	}

	UnlockSlotPtr( SF_RESERVED_CURSOR_SLOT );

	int slots[2] = { SF_FULL_SCREEN_SLOT, SF_SS_SLOT( slotNumber ) };

	for ( int i = 0; i < 2; i++ )
	{
		if ( slots[i] < MAX_SLOTS )
		{
			BaseSlot* pSlot = LockSlotPtr( slots[i] );

			if ( pSlot && pSlot->ConsumesInputEvents() )
			{

				if ( clearControllerUI )
				{
					bool isKeyOrButtonPress = (event.Type == Event::MouseDown || event.Type == Event::KeyDown );
					pSlot->SetToControllerUI( false, isKeyOrButtonPress );
				}

				unsigned int code = pSlot->m_pMovieView->HandleEvent( event );

				if ( code & Movie::HE_NoDefaultAction )
				{
					result = true;
				}
			}

			UnlockSlotPtr( slots[i] );

			// [jason] slots are listed in order of priority, so if one has already handled the event it supersedes any other slots
			if ( result && !toAllSlots )
			{
				break;
			}
		}
	}

	return result;
}

bool ScaleformUIImpl::DistributeKeyEvent( bool keyDown, bool fromController, const char* binding, ButtonCode_t code, ButtonCode_t vkey, int slotNumber, bool toAllSlots )
{
	bool result = false;

	int slots[2] = { SF_FULL_SCREEN_SLOT, SF_SS_SLOT( slotNumber ) };

	for ( int i = 0; i < 2; i++ )
	{
		if ( slots[i] < MAX_SLOTS )
		{
			BaseSlot* pSlot = LockSlotPtr( slots[i] );

			if ( pSlot && pSlot->ConsumesInputEvents() )
			{
				bool isButtonPress = keyDown && code < JOYSTICK_FIRST_AXIS_BUTTON;
				pSlot->SetToControllerUI( fromController, isButtonPress );
				result = pSlot->HandleKeyEvent( keyDown, code, vkey, binding, slotNumber ) || result;
			}

			UnlockSlotPtr( slots[i] );

			// [jason] slots are listed in order of priority, so if one has already handled the event it supersedes any other slots
			if ( result && !toAllSlots )
			{
				break;
			}
		}
	}

	return result;
}

bool ScaleformUIImpl::DistributeCharTyped( wchar_t code )
{
	wchar_t msgString[2];
	msgString[0] = code;
	msgString[1] = L'\0';

	int slots[2] = { SF_FULL_SCREEN_SLOT, SF_SS_SLOT( m_iKeyboardSlot ) };

	bool result = false;

	for ( int i = 0; i < 2; i++ )
	{
		if ( slots[i] < MAX_SLOTS )
		{

			BaseSlot* pSlot = LockSlotPtr( slots[i] );

			if ( pSlot && pSlot->ConsumesInputEvents() )
			{
				pSlot->SetToControllerUI( false, true );
				result = pSlot->HandleCharTyped( msgString, m_iKeyboardSlot ) || result;
			}

			UnlockSlotPtr( slots[i] );

			// [jason] slots are listed in order of priority, so if one has already handled the event it supersedes any other slots
			if ( result )
			{
				break;
			}
		}
	}

	return result;
}


bool ScaleformUIImpl::HitTest( int x, int y )
{
	MEM_ALLOC_CREDIT();

	bool result = false;

	int slots[2] = { SF_FULL_SCREEN_SLOT, SF_SS_SLOT( m_iKeyboardSlot ) };

	for ( int i = 0; i < 2 && !result; i++ )
	{
		if ( slots[i] < MAX_SLOTS )
		{
			BaseSlot* pSlot = LockSlotPtr( slots[i] );

			if ( pSlot && pSlot->ConsumesInputEvents() )
			{
				if ( pSlot->m_pMovieView->HitTest( x, y, Movie::HitTest_Shapes ) )
				{
					result = true;
				}
			}

			UnlockSlotPtr( slots[i] );
		}
	}

	return result;
}

bool ScaleformUIImpl::TallyAxisButtonEvent( int slot, int code, bool down )
{
	bool result = false;
	int sfSlot = SF_SS_SLOT( slot );

	if ( sfSlot < MAX_SLOTS )
	{
		if ( code >= JOYSTICK_FIRST_AXIS_BUTTON && code <= JOYSTICK_LAST_AXIS_BUTTON )
		{
			int mask = 1 << ( code - JOYSTICK_FIRST_AXIS_BUTTON );

			if ( down )
			{
				// on a down event, only let us through if we haven't disabled stick navigation

				if ( !AnalogStickNavigationDisabled( sfSlot ) )
				{
					result = true;
					m_iJoyAxisButtonsDown[slot] |= mask;
				}
			}
			else
			{
				// on an up event, we just care about if we captured a previous down event
				result = ( m_iJoyAxisButtonsDown[slot] & mask ) != 0;
				m_iJoyAxisButtonsDown[slot] &= ~mask;
			}

		}
		else
		{
			// if this isn't a joystick axis button, then just let it go through
			result = true;
		}
	}

	return result;

		 
}


bool ScaleformUIImpl::HandleInputEvent( const InputEvent_t &event )
{
	MEM_ALLOC_CREDIT();

	// Update cached mouse location always, even if we aren't consuming mouse events.
	// Otherwise we could have a stale value if we start consuming mouse events but the mouse doesn't actually move.
	bool mousePositionChanged = false;
	if ( event.m_nType == IE_AnalogValueChanged && event.m_nData == MOUSE_XY )
	{
		if ( m_iLastMouseX != event.m_nData2 || m_iLastMouseY != event.m_nData3 )
		{
			mousePositionChanged = true;
			m_iLastMouseX = event.m_nData2;
			m_iLastMouseY = event.m_nData3;
		}
	}

	bool result = false;
	bool isJoystickEvent = 	( event.m_nType == IE_AnalogValueChanged ) && ( event.m_nData >= JOYSTICK_FIRST_AXIS ) && ( event.m_nData <= JOYSTICK_LAST_AXIS );
	bool consumesEvents = ConsumesInputEvents();
	const char* buttonBinding;

	if ( !consumesEvents && !isJoystickEvent )
	{
		return result;
	}

	// no need to handle input event if the console is visible
	if ( m_pEngine->Con_IsVisible() )
	{
		return result;
	}

	int slot = 0;

	switch ( event.m_nType )
	{
		case IE_ButtonDoubleClicked:
		case IE_ButtonPressed:
		{
			// NOTE: data2 is the virtual key code ( data1 contains the scan-code one )
			ButtonCode_t code;

			m_CurrentKey = ( ButtonCode_t ) event.m_nData;

			DecodeButtonandSlotFromButtonCode( m_CurrentKey, code, slot );
			buttonBinding = m_pGameUIFuncs->GetBindingForButtonCode( m_CurrentKey );

			bool bIsFromController = IsJoystickCode( code ); 

			if ( bIsFromController )
			{
				ControllerMoved();
			}

			if ( IsKeyCode( code ) || bIsFromController )
			{
				if ( TallyAxisButtonEvent( slot, code, true ) )
				{
					result = DistributeKeyEvent( true, bIsFromController, buttonBinding, m_CurrentKey, ( ButtonCode_t ) event.m_nData2, slot, false );
				}
				else
				{
					result = false;
				}

			}

			if ( IsMouseCode( code ) && m_iWantCursorShown )
			{
				CursorMoved();
				if ( HitTest( m_iLastMouseX, m_iLastMouseY ) )
				{
					if ( code != MOUSE_LEFT )
					{
						result = DistributeKeyEvent( true, false, buttonBinding, m_CurrentKey, ( ButtonCode_t ) event.m_nData2, slot, false );
					}	
					else
					{
						MouseEvent mevent( Event::MouseDown, 0, m_iLastMouseX, m_iLastMouseY );
						result = DistributeEvent( mevent, slot, false );
					}

					
				}
			}

		}
		break;

		case IE_KeyTyped:
		{
			result = DistributeCharTyped( ( wchar_t )event.m_nData );
		}
		break;

		case IE_ButtonReleased:
		{
			// NOTE: data2 is the virtual key code ( data1 contains the scan-code one )
			ButtonCode_t code;

			DecodeButtonandSlotFromButtonCode( ( ButtonCode_t ) event.m_nData, code, slot );
			buttonBinding = m_pGameUIFuncs->GetBindingForButtonCode( ( ButtonCode_t ) event.m_nData );

			bool bIsFromController = IsJoystickCode( code ); 

			if ( bIsFromController )
			{
				ControllerMoved();
			}

			if ( IsKeyCode( code ) || bIsFromController )
			{
				if ( TallyAxisButtonEvent( slot, code, false ) )
				{
					result = DistributeKeyEvent( false, bIsFromController, buttonBinding, code, ( ButtonCode_t ) event.m_nData2, slot, false );
				}
				else
				{
					result = false;
				}
			}

			if ( IsMouseCode( code ) && ( m_iWantCursorShown || code != MOUSE_LEFT ) )
			{
				CursorMoved();
				if ( code != MOUSE_LEFT )
				{
					result = DistributeKeyEvent( false, false, buttonBinding, m_CurrentKey, ( ButtonCode_t ) event.m_nData2, slot, false );
				}	
				else
				{
					MouseEvent mevent( Event::MouseUp, 0, m_iLastMouseX, m_iLastMouseY );
					DistributeEvent( mevent, slot, false );
				}
				
			}
		}
		break;

		case IE_AnalogValueChanged:
		{
			switch ( event.m_nData )
			{
				case MOUSE_XY:
					if ( mousePositionChanged && m_iWantCursorShown )
					{
						// on the PS3 a single mouse update is queued even when no mouse is connected
						// So on PS3 we'll eat that message and wait for the next one before we actually
						// show the cursor or switch to the keyboard/mouse UI glyphs
						if ( IsPS3() && m_bEatPS3MouseEvent )
						{
							m_bEatPS3MouseEvent = false;
							MouseEvent mevent( Event::MouseMove, 0, m_iLastMouseX, m_iLastMouseY );
							DistributeEvent( mevent, m_iKeyboardSlot, false, false );
						}
						else
						{
							CursorMoved();
							// If we're using the steam controller, assume input coming from the controller and continue to stay in controller UI mode.
							// FIXME : Need latest SteamAPI integration
							//bool bClearControllerUI = steamapicontext && !steamapicontext->SteamController();
							bool bClearControllerUI = false;
							MouseEvent mevent( Event::MouseMove, 0, m_iLastMouseX, m_iLastMouseY );
							DistributeEvent( mevent, m_iKeyboardSlot, false, bClearControllerUI );
						}
					}
					break;

				case JOYSTICK_AXIS(0, JOY_AXIS_X ):
					SetJoyValue( 0, 0, 0, event.m_nData2 );
					break;

				case JOYSTICK_AXIS(0, JOY_AXIS_Y ):
					SetJoyValue( 0, 0, 1, event.m_nData2 );
					break;

				case JOYSTICK_AXIS(0, JOY_AXIS_U ):
					SetJoyValue( 0, 1, 0, event.m_nData2 );
					break;

				case JOYSTICK_AXIS(0, JOY_AXIS_R ):
					SetJoyValue( 0, 1, 1, event.m_nData2 );
					break;
				case JOYSTICK_AXIS(1, JOY_AXIS_X ):
					SetJoyValue( 1, 0, 0, event.m_nData2 );
					slot = 1;
					break;
				case JOYSTICK_AXIS(1, JOY_AXIS_Y ):
					SetJoyValue( 1, 0, 1, event.m_nData2 );
					slot = 1;
					break;
				case JOYSTICK_AXIS(1, JOY_AXIS_U ):
					SetJoyValue( 1, 1, 0, event.m_nData2 );
					slot = 1;
					break;
				case JOYSTICK_AXIS(1, JOY_AXIS_R ):
					SetJoyValue( 1, 1, 1, event.m_nData2 );
					slot = 1;
					break;
			}
		}
		break;

		default:
			return false;
	}

	// always eat key input when IME is up
	if ( !result && m_bIMEEnabled )
	{
		switch ( event.m_nType )
		{
		case IE_ButtonPressed:
		case IE_ButtonReleased:
		{
			ButtonCode_t code;
			m_CurrentKey = (ButtonCode_t)event.m_nData;
			DecodeButtonandSlotFromButtonCode( m_CurrentKey, code, slot );

			if ( !IsMouseCode( code ) )
			{
				result = true;
			}
		}
		}
	}

	bool returnValue;

	if ( !consumesEvents )
	{
		returnValue = false;
	}
	else if ( SlotDeniesInputToGame( SF_SS_SLOT( slot ) ) )
	{
		returnValue = true;
	}
	else
	{
		returnValue = result;
	}

	return  returnValue;
}

bool ScaleformUIImpl::IsSlotKeyboardAccessible( int slot )
{
	return ( slot == SF_FULL_SCREEN_SLOT || slot == SF_SS_SLOT( m_iKeyboardSlot ) );
}


bool ScaleformUIImpl::HandleIMEEvent( size_t hwnd, unsigned int uMsg, unsigned int  wParam, long lParam )
{
#if defined( PLATFORM_WINDOWS_PC )
	if ( g_bScaleformIMEDetailedLogging )
	{
		Log_Detailed( LOG_SCALEFORM_IME, "HandleIMEEvent: hWnd:0x%8.8x, uMsg:0x%8.8x, wParam:0x%8.8x, lParam:0x%8.8x\n", (uint32)hwnd, (uint32)uMsg, (uint32)wParam, (uint32)lParam );

		CUtlString messageString;
		switch ( uMsg )
		{
		case WM_LBUTTONDOWN:
			messageString = "WM_LBUTTONDOWN";
			break;
		case WM_LBUTTONUP:
			messageString = "WM_LBUTTONUP";
			break;
		case WM_KEYDOWN:
			messageString = "WM_KEYDOWN";
			break;
		case WM_KEYUP:
			messageString = "WM_KEYUP";
			break;
		case WM_CHAR:
			messageString = "WM_CHAR";
			break;
		case WM_DEADCHAR:
			messageString = "WM_DEADCHAR";
			break;
		case WM_SYSKEYDOWN:
			messageString = "WM_SYSKEYDOWN";
			break;
		case WM_SYSKEYUP:
			messageString = "WM_SYSKEYUP";
			break;
		case WM_SYSCHAR:
			messageString = "WM_SYSCHAR";
			break;
		case WM_SYSDEADCHAR:
			messageString = "WM_SYSDEADCHAR";
			break;
		case WM_UNICHAR:
			messageString = "WM_UNICHAR";
			break;

		case WM_INPUTLANGCHANGE:
			messageString = "WM_INPUTLANGCHANGE";
			break;
		case WM_IME_STARTCOMPOSITION:
			messageString = "WM_IME_STARTCOMPOSITION";
			break;
		case WM_IME_COMPOSITION:
			messageString = "WM_IME_COMPOSITION";
			break;
		case WM_IME_ENDCOMPOSITION:
			messageString = "WM_IME_ENDCOMPOSITION";
			break;
		case WM_IME_NOTIFY:
			messageString = "WM_IME_NOTIFY";
			break;
		case WM_IME_SETCONTEXT:
			messageString = "WM_IME_SETCONTEXT";
			break;
		case WM_IME_CONTROL:
			messageString = "WM_IME_CONTROL";
			break;
		case WM_IME_COMPOSITIONFULL:
			messageString = "WM_IME_COMPOSITIONFULL";
			break;
		case WM_IME_SELECT:
			messageString = "WM_IME_SELECT";
			break;
		case WM_IME_KEYDOWN:
			messageString = "WM_IME_KEYDOWN";
			break;
		case WM_IME_KEYUP:
			messageString = "WM_IME_KEYUP";
			break;
		case WM_IME_CHAR:
			messageString = "WM_IME_CHAR";
			break;
		default:
			messageString.Format( "Unknown IME message" );
		}

		CUtlString subMessageString;
		if ( uMsg == WM_IME_NOTIFY )
		{
			switch ( wParam )
			{
			case IMN_CLOSESTATUSWINDOW:
				subMessageString = "IMN_CLOSESTATUSWINDOW";
				break;
			case IMN_OPENSTATUSWINDOW:
				subMessageString = "IMN_OPENSTATUSWINDOW";
				break;
			case IMN_CHANGECANDIDATE:
				subMessageString = "IMN_CHANGECANDIDATE";
				break;
			case IMN_CLOSECANDIDATE:
				subMessageString = "IMN_CLOSECANDIDATE";
				break;
			case IMN_OPENCANDIDATE:
				subMessageString = "IMN_OPENCANDIDATE";
				break;
			case IMN_SETCONVERSIONMODE:
				subMessageString = "IMN_SETCONVERSIONMODE";
				break;
			case IMN_SETSENTENCEMODE:
				subMessageString = "IMN_SETSENTENCEMODE";
				break;
			case IMN_SETOPENSTATUS:
				subMessageString = "IMN_SETOPENSTATUS";
				break;
			case IMN_SETCANDIDATEPOS:
				subMessageString = "IMN_SETCANDIDATEPOS";
				break;
			case IMN_SETCOMPOSITIONFONT:
				subMessageString = "IMN_SETCOMPOSITIONFONT";
				break;
			case IMN_SETCOMPOSITIONWINDOW:
				subMessageString = "IMN_SETCOMPOSITIONWINDOW";
				break;
			case IMN_SETSTATUSWINDOWPOS:
				subMessageString = "IMN_SETSTATUSWINDOWPOS";
				break;
			case IMN_GUIDELINE:
				subMessageString = "IMN_GUIDELINE";
				break;
			case IMN_PRIVATE:
				subMessageString = "IMN_PRIVATE";
				break;
			default:
				subMessageString.Format( "Unknown IMN_??? message" );
			}
		}
		Log_Detailed( LOG_SCALEFORM_IME, "   HandleIMEEvent: %s %s\n", messageString.Get(), subMessageString.Get() );
	}
#endif

	if ( !m_bIMEEnabled )
	{
		return false;
	}

	bool bHandled = false;
#if defined( SF_ENABLE_IME ) && defined( SF_ENABLE_IME_WIN32 )
	BaseSlot* pSlot = LockSlotPtr( m_iIMEFocusSlot );

	if ( pSlot )
	{
		IMEWin32Event ev( IMEWin32Event::IME_Default, (SF::UPInt)hwnd, uMsg, wParam, lParam );
		bHandled = ((pSlot->m_pMovieView->HandleEvent( ev ) & Movie::HE_NoDefaultAction) > 0);
	}
	UnlockSlotPtr( m_iIMEFocusSlot );

	Log_Detailed( LOG_SCALEFORM_IME, "   HandleIMEEvent returns %d\n", bHandled );
#endif
	return bHandled;
}

bool ScaleformUIImpl::PreProcessKeyboardEvent( size_t hwnd, unsigned int uMsg, unsigned int  wParam, long lParam )
{
	if ( !m_bIMEEnabled )
		return false;

	bool bHandled = false;
#if defined( SF_ENABLE_IME ) && defined( SF_ENABLE_IME_WIN32 )
	BaseSlot* pSlot = LockSlotPtr( m_iIMEFocusSlot );
	if ( pSlot && pSlot->m_pMovieView )
	{
		IMEWin32Event ev( IMEWin32Event::IME_PreProcessKeyboard, (SF::UPInt)hwnd, uMsg, wParam, lParam );
		bHandled = ((pSlot->m_pMovieView->HandleEvent( ev ) & Movie::HE_NoDefaultAction) > 0);
	}
	UnlockSlotPtr( m_iIMEFocusSlot );
#endif
	return bHandled;
}

void ScaleformUIImpl::SetIMEEnabled( bool bEnabled )
{
	if ( m_bIMEEnabled != bEnabled )
	{
#if defined( SF_ENABLE_IME ) && defined( SF_ENABLE_IME_WIN32 )
		Log_Detailed( LOG_SCALEFORM_IME, "%s: %s\n", __FUNCTION__, bEnabled ? "True" : "False" );

		if ( m_pIMEManager )
		{
			if ( !bEnabled )
			{
				m_pIMEManager->OnFinalize();
			}
			m_pIMEManager->EnableIME( bEnabled );
		}
#endif
	}
	m_bIMEEnabled = bEnabled;
}

void ScaleformUIImpl::SetIMEFocus( int slot )
{
#if defined( SF_ENABLE_IME ) && defined( SF_ENABLE_IME_WIN32 )
	if ( m_pIMEManager )
	{
		if ( m_iIMEFocusSlot != slot )
		{
			BaseSlot* pSlot = LockSlotPtr( slot );

			if ( pSlot && pSlot->m_pMovieView )
			{
				pSlot->m_pMovieView->HandleEvent( Event::SetFocus );
				m_iIMEFocusSlot = slot;
			}
			UnlockSlotPtr( slot );
		}
	}
#endif
}

void ScaleformUIImpl::ShutdownIME()
{
#if defined( SF_ENABLE_IME ) && defined( SF_ENABLE_IME_WIN32 )
	if ( m_pIMEManager && m_bIMEEnabled )
	{
		m_bIMEEnabled = false;
		m_pIMEManager->EnableIME( false );
	}
#endif
}

void ScaleformUIImpl::UpdateAvatarImages( void )
{
	MEM_ALLOC_CREDIT();

	FOR_EACH_MAP_FAST( m_mapUserXuidToAvatar, i )
	{
		m_mapUserXuidToAvatar[i]->Update();
	}
}

bool ScaleformUIImpl::AvatarImageAddRef( XUID playerID )
{
	EnsureSteamCallbacksConfigured();

	playerID = NormalizeXuidForAccountID( playerID );

	if ( !m_pRenderHAL )
	{
		Assert( CommandLine()->FindParm( "-noshaderapi" ) ); // For transcoding demos.
		return true;
	}

	Assert( playerID );
	MEM_ALLOC_CREDIT();
	
	ScaleformUIAvatarImage *pImage = NULL;
	int iIndex = m_mapUserXuidToAvatar.Find( playerID );
	if ( iIndex == m_mapUserXuidToAvatar.InvalidIndex() )
	{
		if ( m_pDefaultAvatarTexture )
		{
			pImage = new ScaleformUIAvatarImage( playerID, m_pDefaultAvatarTexture->ImageData(), m_pDefaultAvatarTexture->Width(), m_pDefaultAvatarTexture->Height(), m_pDefaultAvatarTexture->Format(), m_pRenderHAL->GetTextureManager() );
		}
		else
		{
			static const byte defaultTextureBits[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			pImage = new ScaleformUIAvatarImage( playerID, defaultTextureBits, 2, 2, IMAGE_FORMAT_RGBA8888, m_pRenderHAL->GetTextureManager() );
		}

		if ( !pImage->LoadAvatarImage() )
		{
			Warning( "We failed to load the avatar image for user %llX\n", playerID );
			
#ifndef NO_STEAM
			extern CSteamAPIContext *steamapicontext;
			// We will retry if the user is actually logged into steam, otherwise just use the default avatar image
			if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamUserStats() && steamapicontext->SteamUser()->BLoggedOn() )
#endif
			{	// Other platforms should always retry loading the avatar
				Assert( 0 );
				delete pImage;
				return false;
			}
		}
	
		iIndex = m_mapUserXuidToAvatar.Insert( playerID, pImage );
	}
	else
	{
		pImage = m_mapUserXuidToAvatar.Element( iIndex );
	}

	Assert( pImage );
	int nRefcount = pImage->AddRef();
	DevMsg( "Avatar image for user %llX cached [refcount=%d]\n", playerID, nRefcount );

	return true;
}

void ScaleformUIImpl::AvatarImageRelease( XUID playerID )
{
	playerID = NormalizeXuidForAccountID( playerID );

	Assert( playerID );
	MEM_ALLOC_CREDIT();

	int iIndex = m_mapUserXuidToAvatar.Find( playerID );
	if ( iIndex != m_mapUserXuidToAvatar.InvalidIndex() )
	{
		ScaleformUIAvatarImage *pImage = m_mapUserXuidToAvatar.Element( iIndex );
		int nRemainingRefCount = pImage->Release();
		if ( nRemainingRefCount <= 0 )
		{
			m_mapUserXuidToAvatar.RemoveAt( iIndex );
		}
		DevMsg( "Avatar image for user %llX released [refcount=%d]\n", playerID, nRemainingRefCount );
	}
	else
	{
		// We have a ref count problem if we get here because we tried to release an
		// avatar image that doesn't exist!
		Assert( CommandLine()->FindParm( "-noshaderapi" ) ); // This is okay if we're in noshaderapi mode (used to transcode demos).
	}
}

void ScaleformUIImpl::AvatarImageReload( XUID playerID, IScaleformAvatarImageProvider *pProvider )
{
	if ( pProvider && !ScaleformUIAvatarImage::sm_pProvider )
	{
		ScaleformUIAvatarImage::sm_pProvider = pProvider;
	}

	playerID = NormalizeXuidForAccountID( playerID );

	Assert( playerID );
	MEM_ALLOC_CREDIT();

	int iIndex = m_mapUserXuidToAvatar.Find( playerID );
	if ( iIndex != m_mapUserXuidToAvatar.InvalidIndex() )
	{
		ScaleformUIAvatarImage *pImage = m_mapUserXuidToAvatar.Element( iIndex );
		pImage->LoadAvatarImage( pProvider );
		DevMsg( 2, "Avatar image for user %llX reloaded\n", playerID );
	}
}

void ScaleformUIImpl::InventoryImageUpdate( uint64 iItemID, IScaleformInventoryImageProvider *pIScaleformInventoryImageProvider )
{
	ScaleformUIInventoryImage *pImage = NULL;
	int iIndex = m_mapItemIdToImage.Find( iItemID );
	if ( iIndex != m_mapItemIdToImage.InvalidIndex() )
	{
		pImage = m_mapItemIdToImage.Element( iIndex );

		IScaleformInventoryImageProvider::ImageInfo_t imgInfo;
		bool bImgInfoValid = pIScaleformInventoryImageProvider->GetInventoryImageInfo( iItemID, &imgInfo );

		if ( bImgInfoValid && !pImage->LoadInventoryImage( imgInfo.m_bufImageDataRGBA, imgInfo.m_nWidth, imgInfo.m_nHeight, IMAGE_FORMAT_BGRA8888 ) )
		{
			Warning( "We failed to update the inventory image for item %llX\n", iItemID );
			Assert( 0 );
		}
	}
}

bool ScaleformUIImpl::InventoryImageAddRef( uint64 iItemID, IScaleformInventoryImageProvider *pIScaleformInventoryImageProvider )
{
	Assert( iItemID );
	MEM_ALLOC_CREDIT();

	if ( !m_pRenderHAL )
	{
		Assert( CommandLine()->FindParm( "-noshaderapi" ) ); // For transcoding demos.
		return true;
	}
	
	ScaleformUIInventoryImage *pImage = NULL;
	int iIndex = m_mapItemIdToImage.Find( iItemID );
	if ( iIndex == m_mapItemIdToImage.InvalidIndex() )
	{
		IScaleformInventoryImageProvider::ImageInfo_t imgInfo;
		bool bImgInfoValid = pIScaleformInventoryImageProvider->GetInventoryImageInfo( iItemID, &imgInfo );

#ifdef USE_DEFAULT_INVENTORY_ICON_BACKGROUNDS
		if ( bImgInfoValid && imgInfo.m_pDefaultIconName )
		{
			uint nHash = HashStringCaselessConventional( imgInfo.m_pDefaultIconName );
			UtlHashFastHandle_t handle = m_defaultInventoryIcons.Find( nHash );
			if ( handle != m_defaultInventoryIcons.InvalidHandle() )
			{
				DefaultInventoryIcon_t icon = m_defaultInventoryIcons.Element( handle );

				if ( icon.m_pTexture && icon.m_pTexture->Width() == imgInfo.m_nWidth )
				{
					Assert( imgInfo.m_nHeight <= icon.m_pTexture->Height() );
					pImage = new ScaleformUIInventoryImage( iItemID, icon.m_pTexture->ImageData(), imgInfo.m_nWidth, imgInfo.m_nHeight, icon.m_pTexture->Format(), m_pRenderHAL->GetTextureManager() );
				}
			}
		}
#endif

		if ( !pImage )
		{
			static const byte defaultTextureBits[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			pImage = new ScaleformUIInventoryImage( iItemID, defaultTextureBits, 2, 2, IMAGE_FORMAT_BGRA8888, m_pRenderHAL->GetTextureManager() );
		}

		if ( bImgInfoValid && !pImage->LoadInventoryImage( imgInfo.m_bufImageDataRGBA, imgInfo.m_nWidth, imgInfo.m_nHeight, IMAGE_FORMAT_BGRA8888 ) )
		{
			Warning( "We failed to load the inventory image for item %llX\n", iItemID );
			Assert( 0 );
		}
	
		iIndex = m_mapItemIdToImage.Insert( iItemID, pImage );
	}
	else
	{
		pImage = m_mapItemIdToImage.Element( iIndex );
	}

	Assert( pImage );
	int nRefcount = pImage->AddRef();
	DevMsg( "Inventory image for item %llX cached [refcount=%d]\n", iItemID, nRefcount );

	return true;
}

void ScaleformUIImpl::InventoryImageRelease( uint64 iItemID )
{
	Assert( iItemID );
	MEM_ALLOC_CREDIT();

	int iIndex = m_mapItemIdToImage.Find( iItemID );
	if ( iIndex != m_mapItemIdToImage.InvalidIndex() )
	{
		ScaleformUIInventoryImage *pImage = m_mapItemIdToImage.Element( iIndex );
		int nRemainingRefCount = pImage->Release();
		if ( nRemainingRefCount <= 0 )
		{
			m_mapItemIdToImage.RemoveAt( iIndex );
		}
		DevMsg( "Inventory image for item %llX released [refcount=%d]\n", iItemID, nRemainingRefCount );
	}
	else
	{
		// We have a ref count problem if we get here because we tried to release an
		// inventory image that doesn't exist!
		Assert( 0 ); 
	}
}

#ifdef USE_DEFAULT_INVENTORY_ICON_BACKGROUNDS
void ScaleformUIImpl::InitInventoryDefaultIcons( CUtlVector< const char * > *vecIconDefaultNames )
{
#if defined( CSTRIKE15 )
	CUtlBuffer bufFile;
	// Load the default inventory image bits and store them in a raw buffer
	FOR_EACH_VEC( *vecIconDefaultNames, i )
	{
		uint uHash = HashStringCaselessConventional( vecIconDefaultNames->Element(i) ); 
		UtlHashFastHandle_t handle = m_defaultInventoryIcons.Find( uHash );
		if ( handle == m_defaultInventoryIcons.InvalidHandle() )
		{
			DefaultInventoryIcon_t icon;
			icon.m_pImage = NULL;
			icon.m_pTexture = NULL;
			bufFile.Clear();
			const char* pDefaultInventoryImageFileName = vecIconDefaultNames->Element(i);
			if ( g_pFullFileSystem->ReadFile( pDefaultInventoryImageFileName, NULL, bufFile ) )
			{
				icon.m_pTexture = CreateVTFTexture();
#if !defined( _GAMECONSOLE )
				if ( !icon.m_pTexture->Unserialize( bufFile ) )
#else
				if ( !icon.m_pTexture->UnserializeFromBuffer( bufFile, true, false, false, 0 ) )
#endif
				{
					Warning( "Invalid or corrupt default inventory image (%s)\n", pDefaultInventoryImageFileName );
					DestroyVTFTexture( icon.m_pTexture );
					icon.m_pTexture = NULL;
				}
			}
			else
			{
				Warning( "Failed to read the default inventory image file (%s)\n", pDefaultInventoryImageFileName );
			}

			if ( icon.m_pTexture )
			{
				m_defaultInventoryIcons.FastInsert( uHash, icon );
			}
		}
	}
#endif // CSTRIKE15

}
#endif

Image* ScaleformUIImpl::CreateImageFromFile( const char *pszFileName, const ImageCreateInfo& info, int width, int height )
{
	if ( !m_pRenderHAL )
	{
		Assert( CommandLine()->FindParm( "-noshaderapi" ) ); // For transcoding demos.
		return NULL;
	}

	if ( pszFileName != NULL )
	{
		return m_pLoader->GetImageCreator()->LoadImageFile( info, SF::String( pszFileName ) );
	}
	else
	{
		return RawImage::Create( Image_R8G8B8A8, 0, ImageSize( width, height ), ImageUse_Update, 0, m_pRenderHAL->GetTextureManager() );
	}
}

void ScaleformUIImpl::AddDeviceDependentObject( IShaderDeviceDependentObject * object )
{
	if ( m_pShaderDeviceMgr )
	{
		m_pShaderDeviceMgr->AddDeviceDependentObject( object );
	}
}

void ScaleformUIImpl::RemoveDeviceDependentObject( IShaderDeviceDependentObject * object )
{
	if ( m_pShaderDeviceMgr )
	{
		m_pShaderDeviceMgr->RemoveDeviceDependentObject( object );
	}
}


ScaleformUIAvatarImage* ScaleformUIImpl::GetAvatarImage( XUID playerID )
{
	playerID = NormalizeXuidForAccountID( playerID );

	MEM_ALLOC_CREDIT();

	ScaleformUIAvatarImage *pImage = NULL;

	if ( !m_pRenderHAL )
	{
		Assert( CommandLine()->FindParm( "-noshaderapi" ) ); // For transcoding demos.
		return NULL;
	}

	if ( playerID )
	{
		int iIndex = m_mapUserXuidToAvatar.Find( playerID );
		if ( iIndex == m_mapUserXuidToAvatar.InvalidIndex() )
		{
			// If you hit this assert then the most likely problem is that the action script
			// file is trying to load an avatar image that hasn't been created yet.
			// To create the avatar image call m_pScaleformUI->AvatarImageAddRef in the client code.
			Assert( 0 );
			Warning( "Error getting avatar image: playerID(%llu), iIndex(%d)\n", playerID, iIndex );
		}
		else
		{
			pImage = m_mapUserXuidToAvatar.Element( iIndex );
		}
	}

	if ( !pImage )
	{
		if ( !m_pDefaultAvatarImage )
		{
			// Create an avatar image for the player id of 0 (or an avatar we failed to load) to use as a default
			if ( m_pDefaultAvatarTexture )
			{
				m_pDefaultAvatarImage = new ScaleformUIAvatarImage( 0, m_pDefaultAvatarTexture->ImageData(), m_pDefaultAvatarTexture->Width(), m_pDefaultAvatarTexture->Height(), m_pDefaultAvatarTexture->Format(), m_pRenderHAL->GetTextureManager() );
			}
			else
			{
				static const byte defaultTextureBits[] = { 0, 0, 0, 0 };
				m_pDefaultAvatarImage = new ScaleformUIAvatarImage( 0, defaultTextureBits, 2, 2, IMAGE_FORMAT_RGBA8888, m_pRenderHAL->GetTextureManager() );
			}
		}

		pImage = m_pDefaultAvatarImage;
	}

	return pImage;
}

ScaleformUIInventoryImage* ScaleformUIImpl::GetInventoryImage( uint64 iItemID )
{
	MEM_ALLOC_CREDIT();

	if ( !m_pRenderHAL )
	{
		Assert( CommandLine()->FindParm( "-noshaderapi" ) ); // For transcoding demos.
		return NULL;
	}

	ScaleformUIInventoryImage *pImage = NULL;

	if ( iItemID )
	{
		int iIndex = m_mapItemIdToImage.Find( iItemID );
		if ( iIndex == m_mapItemIdToImage.InvalidIndex() )
		{
			// If you hit this assert then the most likely problem is that the action script
			// file is trying to load an inventory image that hasn't been created yet.
			// To create the inventory image call m_pScaleformUI->InventoryImageAddRef in the client code.
			Assert( 0 );
			Warning( "Error getting inventory image: iItemID(%llu), iIndex(%d)\n", iItemID, iIndex );
		}
		else
		{
			pImage = m_mapItemIdToImage.Element( iIndex );
		}
	}

	if ( !pImage )
	{
		if ( !m_pDefaultInventoryImage )
		{
			// Create an inventory image for the item id of 0 (or an inventory item we failed to load/generate) to use as a default
			if ( m_pDefaultInventoryTexture )
			{
				m_pDefaultInventoryImage = new ScaleformUIInventoryImage( 0, m_pDefaultInventoryTexture->ImageData(), m_pDefaultInventoryTexture->Width(), m_pDefaultInventoryTexture->Height(), m_pDefaultInventoryTexture->Format(), m_pRenderHAL->GetTextureManager() );
			}
			else
			{
				static const byte defaultTextureBits[] = { 0, 0, 0, 0 };
				m_pDefaultInventoryImage = new ScaleformUIInventoryImage( 0, defaultTextureBits, 2, 2, IMAGE_FORMAT_RGBA8888, m_pRenderHAL->GetTextureManager() );
			}
		}

		pImage = m_pDefaultInventoryImage;
	}

	return pImage;
}


bool ScaleformUIImpl::ChromeHTMLImageAddRef( uint64 imageID )
{
	Assert( imageID );
	MEM_ALLOC_CREDIT();

	if ( !m_pRenderHAL )
	{
		Assert( CommandLine()->FindParm( "-noshaderapi" ) ); // For transcoding demos.
		return true;
	}

	ScaleformUIChromeHTMLImage *pImage = NULL;
	int iIndex = m_mapImageIdToChromeImage.Find( imageID );
	if ( iIndex == m_mapImageIdToChromeImage.InvalidIndex() )
	{
		static const byte defaultTextureBits[] = { 0, 0, 0, 0,    0, 0, 0, 0,
												   0, 0, 0, 0,    0, 0, 0, 0};
		pImage = new ScaleformUIChromeHTMLImage( imageID, defaultTextureBits, 2, 2, IMAGE_FORMAT_RGBA8888, m_pRenderHAL->GetTextureManager() );

		iIndex = m_mapImageIdToChromeImage.Insert( imageID, pImage );
	}
	else
	{
		pImage = m_mapImageIdToChromeImage.Element( iIndex );
	}

	Assert( pImage );
	int nRefcount = pImage->AddRef();
	DevMsg( "Chrome HTML image for id %llX cached [refcount=%d]\n", imageID, nRefcount );

	return true;
}

void ScaleformUIImpl::ChromeHTMLImageUpdate( uint64 imageID, const byte* rgba, int width, int height, ::ImageFormat format )
{
	ScaleformUIChromeHTMLImage *pImage = NULL;
	int iIndex = m_mapImageIdToChromeImage.Find( imageID );
	if ( iIndex != m_mapImageIdToChromeImage.InvalidIndex() )
	{
		pImage = m_mapImageIdToChromeImage.Element( iIndex );
		if ( !pImage->LoadChromeHTMLImage( rgba, width, height, format ) )
		{
			Warning( "We failed to update the chrome HTML image for item %llX\n", imageID );
			Assert( 0 );
		}
	}
}

void ScaleformUIImpl::ChromeHTMLImageRelease( uint64 imageID )
{
	Assert( imageID );
	MEM_ALLOC_CREDIT();

	int iIndex = m_mapImageIdToChromeImage.Find( imageID );
	if ( iIndex != m_mapImageIdToChromeImage.InvalidIndex() )
	{
		ScaleformUIChromeHTMLImage *pImage = m_mapImageIdToChromeImage.Element( iIndex );
		int nRemainingRefCount = pImage->Release();
		if ( nRemainingRefCount <= 0 )
		{
			m_mapImageIdToChromeImage.RemoveAt( iIndex );
		}
		DevMsg( "Chrome HTML image for id %llX released [refcount=%d]\n", imageID, nRemainingRefCount );
	}
	else
	{
		// We have a ref count problem if we get here because we tried to release an
		// chrome HTML image that doesn't exist!
		Assert( 0 ); 
	}
}

ScaleformUIChromeHTMLImage* ScaleformUIImpl::GetChromeHTMLImage( uint64 imageID )
{
	MEM_ALLOC_CREDIT();

	if ( !m_pRenderHAL )
	{
		Assert( CommandLine()->FindParm( "-noshaderapi" ) ); // For transcoding demos.
		return NULL;
	}

	ScaleformUIChromeHTMLImage *pImage = NULL;

	if ( imageID )
	{
		int iIndex = m_mapImageIdToChromeImage.Find( imageID );
		if ( iIndex == m_mapImageIdToChromeImage.InvalidIndex() )
		{
			// If you hit this assert then the most likely problem is that the action script
			// file is trying to load an avatar image that hasn't been created yet.
			// To create the avatar image call m_pScaleformUI->AvatarImageAddRef in the client code.
			Assert( 0 );
			Warning( "Error getting chrome HTML image: imageID(%llu), iIndex(%d)\n", imageID, iIndex );
		}
		else
		{
			pImage = m_mapImageIdToChromeImage.Element( iIndex );
		}
	}

	if ( !pImage )
	{
		if ( !m_pDefaultChromeHTMLImage )
		{
			static const byte defaultTextureBits[] = { 0, 0, 0, 0,    0, 0, 0, 0,
													   0, 0, 0, 0,    0, 0, 0, 0};
			m_pDefaultChromeHTMLImage = new ScaleformUIChromeHTMLImage( 0, defaultTextureBits, 2, 2, IMAGE_FORMAT_RGBA8888, m_pRenderHAL->GetTextureManager() );
		}

		pImage = m_pDefaultChromeHTMLImage;
	}

	return pImage;
}
