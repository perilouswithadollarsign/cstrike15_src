//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox
//
//=====================================================================================//

#include "../pch_tier0.h"
#ifdef PLATFORM_PS3
#include "ps3/ps3_core.h"
#include "ps3/ps3_win32stubs.h"
#include "ps3/ps3_console.h"
#include "tls_ps3.h"
#include <sysutil/sysutil_common.h>
#include <sys/process.h>
#include <sysutil/sysutil_sysparam.h>
#else
#include "xbox/xbox_console.h"
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_launch.h"
#endif
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"
#include "tier0/memdbgon.h"

class CXbxEventQueue
{
public:
	CXbxEventQueue() : m_pQueue( NULL ), m_dwCount( 0 ), m_dwAllocated( 0 ), m_bDispatching( false ) {}
	
public:
	void AppendEvent( xevent_t const &xevt );
	void Dispatch();
	bool IsDispatching() const;

protected:
	CThreadFastMutex m_mtx;
	xevent_t *m_pQueue;
	DWORD m_dwCount;
	DWORD m_dwAllocated;
	bool m_bDispatching;

	static const int s_nQueueNormalSize = 32;
	static const int s_nQueueAbnormalSize = 128;
};

static CXbxEventQueue g_xbx_EventQueue;

DWORD			g_iStorageDeviceId[ XUSER_MAX_COUNT ] = 
	{
		XBX_INVALID_STORAGE_ID, XBX_INVALID_STORAGE_ID, XBX_INVALID_STORAGE_ID, XBX_INVALID_STORAGE_ID
		#ifdef PLATFORM_PS3
		, XBX_INVALID_STORAGE_ID, XBX_INVALID_STORAGE_ID, XBX_INVALID_STORAGE_ID
		#endif
	};
DWORD			g_iPrimaryUserId = XBX_INVALID_USER_ID;
DWORD			g_bPrimaryUserIsGuest = 0;
DWORD			g_numGameUsers = 0;
DWORD			g_InvitedUserId = XBX_INVALID_USER_ID;
XUID			g_InvitedUserXuid = 0ull;
XNKID			g_InviteSessionId;
#ifdef _X360
HANDLE			g_hListenHandle = INVALID_HANDLE_VALUE;
ULONG64			g_ListenCategories = 0;
#elif defined( _PS3 )
ps3syscbckeventhdlr_t g_ps3SysCbckEvHdlr;
#endif

//-----------------------------------------------------------------------------
//	Convert an Xbox notification to a custom windows message
//-----------------------------------------------------------------------------
static int NotificationToWindowsMessage( DWORD id )
{
#ifdef _X360
	switch( id )
	{
	case XN_SYS_UI:							return WM_SYS_UI;						
	case XN_SYS_SIGNINCHANGED:				return WM_SYS_SIGNINCHANGED;				
	case XN_SYS_STORAGEDEVICESCHANGED:		return WM_SYS_STORAGEDEVICESCHANGED;		
	case XN_SYS_PROFILESETTINGCHANGED:		return WM_SYS_PROFILESETTINGCHANGED;		
	case XN_SYS_MUTELISTCHANGED:			return WM_SYS_MUTELISTCHANGED;				
	case XN_SYS_INPUTDEVICESCHANGED:		return WM_SYS_INPUTDEVICESCHANGED;			
	case XN_SYS_INPUTDEVICECONFIGCHANGED:	return WM_SYS_INPUTDEVICECONFIGCHANGED;		
	case XN_LIVE_CONNECTIONCHANGED:			return WM_LIVE_CONNECTIONCHANGED;			
	case XN_LIVE_INVITE_ACCEPTED:			return WM_LIVE_INVITE_ACCEPTED;				
	case XN_LIVE_LINK_STATE_CHANGED:		return WM_LIVE_LINK_STATE_CHANGED;			
	case XN_LIVE_CONTENT_INSTALLED:			return WM_LIVE_CONTENT_INSTALLED;			
	case XN_LIVE_MEMBERSHIP_PURCHASED:		return WM_LIVE_MEMBERSHIP_PURCHASED;		
	case XN_LIVE_VOICECHAT_AWAY:			return WM_LIVE_VOICECHAT_AWAY;				
	case XN_LIVE_PRESENCE_CHANGED:			return WM_LIVE_PRESENCE_CHANGED;			
	case XN_FRIENDS_PRESENCE_CHANGED:		return WM_FRIENDS_PRESENCE_CHANGED;			
	case XN_FRIENDS_FRIEND_ADDED:			return WM_FRIENDS_FRIEND_ADDED;				
	case XN_FRIENDS_FRIEND_REMOVED:			return WM_FRIENDS_FRIEND_REMOVED;			
	// deprecated in Jun08 XDK: case XN_CUSTOM_GAMEBANNERPRESSED:		return WM_CUSTOM_GAMEBANNERPRESSED;			
	case XN_CUSTOM_ACTIONPRESSED:			return WM_CUSTOM_ACTIONPRESSED;				
	case XN_XMP_STATECHANGED:				return WM_XMP_STATECHANGED;					
	case XN_XMP_PLAYBACKBEHAVIORCHANGED:	return WM_XMP_PLAYBACKBEHAVIORCHANGED;		
	case XN_XMP_PLAYBACKCONTROLLERCHANGED:	return WM_XMP_PLAYBACKCONTROLLERCHANGED;
	default:
		Warning( "Unrecognized notification id %d\n", id );
		return 0;
	}
#endif
	Assert( 0 );
	return 0;
}

//-----------------------------------------------------------------------------
//	XBX_Error
//
//-----------------------------------------------------------------------------
void XBX_Error( const char* format, ... )
{
	va_list args;
	char	message[XBX_MAX_MESSAGE];

	va_start( args, format );
	_vsnprintf( message, sizeof( message ), format, args );
	va_end( args );

	message[sizeof( message )-1] = '\0';

#if defined( _X360 ) 
	XBX_DebugString( XMAKECOLOR(255,0,0), message );
	XBX_FlushDebugOutput();

	DebugBreak();
	static volatile int doReturn;

	while ( !doReturn );
#elif defined( _PS3 )
	XBX_DebugString( 0xFFFFFFFF, message );
	
	// primitive crash technique for now
	DebuggerBreak();
	return;

#endif

}

//-----------------------------------------------------------------------------
//	XBX_OutputDebugStringA
//
//	Replaces 'OutputDebugString' to send through debugging channel
//-----------------------------------------------------------------------------
void XBX_OutputDebugStringA( LPCSTR lpOutputString )
{
#ifdef _X360
	XBX_DebugString( XMAKECOLOR(0,0,0), lpOutputString );
#endif
}

//-----------------------------------------------------------------------------
//	XBX_ProcessXCommand
//
//-----------------------------------------------------------------------------
static void XBX_ProcessXCommand( xevent_t const &xe )
{
	const char *pCommand = (char*)xe.arg1;
#if defined( _X360 ) 
	// remote command
	// pass it game via windows message
	HWND hWnd = GetFocus();
	WNDPROC windowProc = ( WNDPROC)GetWindowLong( hWnd, GWL_WNDPROC );
	if ( windowProc )
	{
		windowProc( hWnd, WM_XREMOTECOMMAND, 0, (LPARAM)pCommand );
	}
#elif defined ( _PS3 )
	if ( g_ps3SysCbckEvHdlr.pfnHandler )
	{
		// recreate the event and send it to the handler (wtf?)
		xevent_t ev = { XEV_REMOTECMD, WM_XREMOTECOMMAND, reinterpret_cast<int>(pCommand), 0 };
		g_ps3SysCbckEvHdlr.pfnHandler( ev );
	}

#endif
	//
	// VXBDM gives us a command as a pointer to the global buffer
	// and then monitors this global buffer on a separate thread.
	// As soon as the first byte in the buffer is set to NULL this
	// serves as a signal to VXBDM that the command has been received
	// and processed and that it can enqueue another command.
	// Signal to VXBDM here:
	( const_cast< char * >( pCommand ) )[0] = 0;
}

//-----------------------------------------------------------------------------
//	XBX_ProcessListenerNotification
//
//-----------------------------------------------------------------------------
static void XBX_ProcessListenerNotification( xevent_t const &xe )
{
#ifdef _X360
	// pass it game via windows message
	HWND hWnd = GetFocus();
	WNDPROC windowProc = ( WNDPROC)GetWindowLong( hWnd, GWL_WNDPROC );
	if ( windowProc )
	{
		windowProc( hWnd, xe.arg1, 0, (LPARAM)xe.arg2 );
	}
#elif defined( _PS3 )
	if ( g_ps3SysCbckEvHdlr.pfnHandler )
	{
		g_ps3SysCbckEvHdlr.pfnHandler( xe );
	}
#else
	Assert( 0 );
#endif

	switch ( xe.arg1 )
	{
	case WM_SYS_SIGNINCHANGED:
		delete reinterpret_cast< xevent_SYS_SIGNINCHANGED_t * >( xe.arg2 );
		break;
	}
}

//-----------------------------------------------------------------------------
//	XBX_QueueEvent
//
//-----------------------------------------------------------------------------
void XBX_QueueEvent(xevent_e event, int arg1, int arg2, int arg3)
{
	xevent_t xEvt;
	memset( &xEvt, 0, sizeof( xEvt ) );
	xEvt.event = event;
	xEvt.arg1 = arg1;
	xEvt.arg2 = arg2;
	xEvt.arg3 = arg3;
	
	g_xbx_EventQueue.AppendEvent( xEvt );
}

#ifdef _PS3
static void XBX_QueueRemoteCommandEvent( const char *pBuf )
{
	XBX_QueueEvent( XEV_REMOTECMD, ( int )pBuf, 0, 0 );
}
void PS3_CellSysutilCallback_Function(
									  uint64_t status,
									  uint64_t param,
									  void *userdata
									  );

#endif

//-----------------------------------------------------------------------------
//	XBX_ProcessEvents
//
//	Assumed one per frame only!
//-----------------------------------------------------------------------------
void XBX_ProcessEvents(void)
{
	Assert( ThreadInMainThread() );
	if ( g_xbx_EventQueue.IsDispatching() )
	{
		DevWarning( "XBX_ProcessEvents is ignored while the queue is dispatching!\n" );
		return;
	}

#ifdef _X360

	DWORD id;
	ULONG parameter;
 	while ( XNotifyGetNext( g_hListenHandle, 0, &id, &parameter ) )
 	{
 		// Special handling
 		switch( id )
 		{
 		case XN_SYS_STORAGEDEVICESCHANGED:
			{
				bool bWarnOfDeviceChange = false;
#if defined ( CSTRIKE15 )
				// cstrike15 saves info to profile; so we always want to check for profile
				// backing store whenever a storage device is changed
				bWarnOfDeviceChange = true;
#endif

				for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
 				{
 					// Has anybody selected a storage device?
 					DWORD storageID = XBX_GetStorageDeviceId( k );
					if ( XBX_DescribeStorageDevice( storageID ) == false )
 						continue;
	 
 					// Validate the selected storage device
 					XDEVICE_DATA deviceData;
 					DWORD ret = XContentGetDeviceData( storageID, &deviceData );
 					if ( ret != ERROR_SUCCESS )
 					{
 						// Device was removed
						Warning( "XN_SYS_STORAGEDEVICESCHANGED: device 0x%08X removed for ctrlr%d!\n",
							storageID, k );
 						XBX_SetStorageDeviceId( k, XBX_INVALID_STORAGE_ID );

						int iSplitscreenSlot = XBX_GetSlotByUserId( k );
						if ( iSplitscreenSlot >= 0 )
						{
							bWarnOfDeviceChange = true;
						}
 					}
 				}
				
				// Notify the user if something important has changed
				if ( bWarnOfDeviceChange )
				{
					XBX_QueueEvent( XEV_LISTENER_NOTIFICATION, NotificationToWindowsMessage( id ), 0, 0 );
				}
			}
			break;

		case XN_SYS_SIGNINCHANGED:
			{
				xevent_SYS_SIGNINCHANGED_t *pSysEvent = new xevent_SYS_SIGNINCHANGED_t;	// deleted during dispatch
				pSysEvent->dwParam = parameter;
				for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
				{
					pSysEvent->state[k] = XUserGetSigninState( k );
					pSysEvent->xuid[k] = 0ull;

					if ( pSysEvent->state[k] != eXUserSigninState_NotSignedIn )
					{
						XUSER_SIGNIN_INFO xsi = {0};
						if ( ERROR_SUCCESS != XUserGetSigninInfo( k, XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY, &xsi ) ||
							!xsi.xuid )
						{
							if ( ERROR_SUCCESS == XUserGetXUID( k, &xsi.xuid ) )
								pSysEvent->xuid[k] = xsi.xuid;
						}
						else
						{
							pSysEvent->xuid[k] = xsi.xuid;
						}
					}
				}
				XBX_QueueEvent( XEV_LISTENER_NOTIFICATION, NotificationToWindowsMessage( id ), reinterpret_cast< int >( pSysEvent ), parameter );
			}
			break;
 
		default:
			XBX_QueueEvent( XEV_LISTENER_NOTIFICATION, NotificationToWindowsMessage( id ), parameter, 0 );
 			break;
 		}
 	}

#elif defined( _PS3 )
	if ( g_pValvePS3Console )
	{
		g_pValvePS3Console->PumpMessage( XBX_QueueRemoteCommandEvent );
	}

	int retval = cellSysutilCheckCallback();
	if ( retval < 0 )
	{
		DevWarning( "cellSysutilCheckCallback failed! error 0x%08X!\n", retval );
	}
#ifndef _CERT
	static float s_flQuitAfter = CommandLine()->ParmValue( "-quitafter", 0.0f );
	if( s_flQuitAfter > 0 && Plat_FloatTime() > s_flQuitAfter )
	{
		PS3_CellSysutilCallback_Function( CELL_SYSUTIL_REQUEST_EXITGAME, 0, 0 );
	}
#endif
#endif
}

void XBX_DispatchEventsQueue()
{
	g_xbx_EventQueue.Dispatch();
}

#ifdef _PS3
void PS3_CellSysutilCallback_Function(
							uint64_t status,
							uint64_t param,
							void *userdata
							)
{
	Assert( g_ps3SysCbckEvHdlr.pfnHandler );

	// Prepare to queue the event
	xevent_t xe;
	memset( &xe, 0, sizeof( xe ) );
	xe.event = XEV_LISTENER_NOTIFICATION;

	switch ( status )
	{
	case CELL_SYSUTIL_REQUEST_EXITGAME:
		xe.arg1 = WM_SYS_SHUTDOWNREQUEST;
		GetTLSGlobals()->bNormalQuitRequested = true; // special flag to prevent the error screen from appearing
		Warning( "[PS3 SYSTEM] REQUEST EXITGAME RECEIVED @ %.3f\n", Plat_FloatTime() );
		break;

	case CELL_SYSUTIL_DRAWING_BEGIN:
	case CELL_SYSUTIL_DRAWING_END:
		// not interesting notifications
		return;

	case CELL_SYSUTIL_SYSTEM_MENU_OPEN:
	case CELL_SYSUTIL_SYSTEM_MENU_CLOSE:
		xe.arg1 = WM_SYS_UI;
		xe.arg2 = !!( status == CELL_SYSUTIL_SYSTEM_MENU_OPEN );
		break;

	case CELL_SYSUTIL_BGMPLAYBACK_PLAY:
	case CELL_SYSUTIL_BGMPLAYBACK_STOP:
		xe.arg1 = WM_XMP_PLAYBACKCONTROLLERCHANGED;
		xe.arg2 = !!( status == CELL_SYSUTIL_BGMPLAYBACK_STOP );
		break;

	case CELL_SYSUTIL_NP_INVITATION_SELECTED:
		xe.arg1 = WM_LIVE_INVITE_ACCEPTED;
		xe.arg2 = XBX_GetPrimaryUserId();	// accept on primary controller
		break;
	}

	// Queue the event
	xe.arg3 = 1; // means that event has sysutil payload
	xe.sysutil_status = status;
	xe.sysutil_param = param;
	g_xbx_EventQueue.AppendEvent( xe );
}
#endif

//-----------------------------------------------------------------------------
//	XBX_NotifyCreateListener
//
//	Add notification categories to the listener object
//-----------------------------------------------------------------------------
bool XBX_NotifyCreateListener( uint64 categories )
{
#ifdef _X360
	if ( categories != 0 )
	{
		categories |= g_ListenCategories;
	}

	g_hListenHandle = XNotifyCreateListener( categories );
	if ( g_hListenHandle == NULL || g_hListenHandle == INVALID_HANDLE_VALUE )
	{
		return false;
	}

	g_ListenCategories = categories;
#elif defined( _PS3 )
	Assert( categories == (uint64)( (size_t) categories ) );
	if ( ps3syscbckeventhdlr_t const *pHdlr = reinterpret_cast< ps3syscbckeventhdlr_t const * >( (size_t) categories ) )
	{
		Assert( !g_ps3SysCbckEvHdlr.pfnHandler );
		g_ps3SysCbckEvHdlr = *pHdlr;

		int retval = cellSysutilRegisterCallback( 0, PS3_CellSysutilCallback_Function, NULL );
		if ( retval < 0 )
		{
			DevWarning( "cellSysutilRegisterCallback failed with error 0x%08X!\n", retval );
		}
	}
	else
	{
		int retval = cellSysutilUnregisterCallback( 0 );
		if ( retval < 0 )
		{
			DevWarning( "cellSysutilUnregisterCallback failed with error 0x%08X!\n", retval );
		}
		memset( &g_ps3SysCbckEvHdlr, 0, sizeof( g_ps3SysCbckEvHdlr ) );
	}
#endif
	return true;
}

struct Language_t
{
	const char *pString;
	int			id;
	bool		bLocalizedAudio;
};
Language_t s_ValidLanguages[] =
{
#ifdef _GAMECONSOLE
		#ifdef _PS3
			#define XC_LANGUAGE_JAPANESE CELL_SYSUTIL_LANG_JAPANESE
#if CELL_SDK_VERSION >= 0x400000
			#define XC_LANGUAGE_ENGLISH CELL_SYSUTIL_LANG_ENGLISH_US
#else
			#define XC_LANGUAGE_ENGLISH CELL_SYSUTIL_LANG_ENGLISH
#endif
			#define XC_LANGUAGE_FRENCH CELL_SYSUTIL_LANG_FRENCH
			#define XC_LANGUAGE_SPANISH CELL_SYSUTIL_LANG_SPANISH
			#define XC_LANGUAGE_GERMAN CELL_SYSUTIL_LANG_GERMAN
			#define XC_LANGUAGE_ITALIAN CELL_SYSUTIL_LANG_ITALIAN
			#define XC_LANGUAGE_DUTCH CELL_SYSUTIL_LANG_DUTCH
#if CELL_SDK_VERSION >= 0x400000
#define XC_LANGUAGE_PORTUGUESE CELL_SYSUTIL_LANG_PORTUGUESE_PT
#else
#define XC_LANGUAGE_PORTUGUESE CELL_SYSUTIL_LANG_PORTUGUESE
#endif
			#define XC_LANGUAGE_RUSSIAN CELL_SYSUTIL_LANG_RUSSIAN
			#define XC_LANGUAGE_KOREAN CELL_SYSUTIL_LANG_KOREAN
			#define XC_LANGUAGE_TCHINESE CELL_SYSUTIL_LANG_CHINESE_T
			#define XC_LANGUAGE_SCHINESE CELL_SYSUTIL_LANG_CHINESE_S
			#define XC_LANGUAGE_FINNISH CELL_SYSUTIL_LANG_FINNISH
			#define XC_LANGUAGE_SWEDISH CELL_SYSUTIL_LANG_SWEDISH
			#define XC_LANGUAGE_DANISH CELL_SYSUTIL_LANG_DANISH
			#define XC_LANGUAGE_NORWEGIAN CELL_SYSUTIL_LANG_NORWEGIAN
			#define XC_LANGUAGE_POLISH CELL_SYSUTIL_LANG_POLISH
		#endif
		#ifdef _X360
			// Xbox 360 doesn't support these languages in OS, but people can still test
			// with them by running with appropriate command line
			#define XC_LANGUAGE_DUTCH		0x7F1A4EF1
			#define XC_LANGUAGE_FINNISH		0x7F1A4EF2
			#define XC_LANGUAGE_SWEDISH		0x7F1A4EF3
			#define XC_LANGUAGE_DANISH		0x7F1A4EF4
			#define XC_LANGUAGE_NORWEGIAN	0x7F1A4EF5
		#endif
	#if 0 // to turn off all localization change it to #if 1
		{"english",		XC_LANGUAGE_ENGLISH,	true},
	#else
		// known supported 360 languages
		{"japanese",	XC_LANGUAGE_JAPANESE,	false},
		{"german",		XC_LANGUAGE_GERMAN,		true},
		{"french",		XC_LANGUAGE_FRENCH,		true},	
		{"spanish",		XC_LANGUAGE_SPANISH,	true},
		{"italian",		XC_LANGUAGE_ITALIAN,	false},
		{"dutch",		XC_LANGUAGE_DUTCH,		false},
		{"korean",		XC_LANGUAGE_KOREAN,		false},
		{"tchinese",	XC_LANGUAGE_TCHINESE,	false},
		{"portuguese",	XC_LANGUAGE_PORTUGUESE,	false},
		{"schinese",	XC_LANGUAGE_SCHINESE,	false},
		{"finnish",		XC_LANGUAGE_FINNISH,	false},
		{"swedish",		XC_LANGUAGE_SWEDISH,	false},
		{"danish",		XC_LANGUAGE_DANISH,		false},
		{"norwegian",	XC_LANGUAGE_NORWEGIAN,	false},
		{"polish",		XC_LANGUAGE_POLISH,		false},
	#if defined( _X360 )
		{"russian",		XC_LANGUAGE_RUSSIAN,	false},
	#else
		{"russian",		XC_LANGUAGE_RUSSIAN,	true},
	#endif
	#endif
#else
#define XC_LANGUAGE_ENGLISH 0
		{"english",		XC_LANGUAGE_ENGLISH,	true},
#endif
};
static const char *SupportedLanguageIDToString( int id )
{
	// find it or force to english
	for ( int i = 0; i < ARRAYSIZE( s_ValidLanguages ); i++ )
	{
		if ( id == s_ValidLanguages[i].id )
		{
			return s_ValidLanguages[i].pString;
		}
	}
	return "english";
}
static int SupportedLanguageStringToID( const char *pName )
{
	// find it or force to english
	for ( int i = 0; i < ARRAYSIZE( s_ValidLanguages ); i++ )
	{
		// caller's argument could be substring from command line, i.e. "french -another arg"
		if ( !strncmp( pName, s_ValidLanguages[i].pString, strlen( s_ValidLanguages[i].pString ) ) )
		{
			return s_ValidLanguages[i].id;
		}
	}
	return XC_LANGUAGE_ENGLISH;
}

//-----------------------------------------------------------------------------
//	Returns the true xbox language id, possibly an id that is not supported.
//-----------------------------------------------------------------------------
static int GetLanguage( void )
{
	static int languageId = -1;
	if ( languageId == -1 )
	{
#ifdef _X360
		languageId = XGetLanguage();
#elif defined( _PS3 )
		if ( cellSysutilGetSystemParamInt( CELL_SYSUTIL_SYSTEMPARAM_ID_LANG, &languageId ) < 0 )
			languageId = XC_LANGUAGE_ENGLISH;
#else
		languageId = XC_LANGUAGE_ENGLISH;
#endif

		// allow language to be overriden via command line for easier development
		// otherwise must set via dashboard
		const char *pLanguage = CommandLine()->ParmValue( "-language", (const char *)NULL );
		if ( pLanguage && pLanguage[0] )
		{
			languageId = SupportedLanguageStringToID( pLanguage );
		}
	}
	return languageId;
}

const char *XBX_GetNextSupportedLanguage( const char *pLanguage, bool *pbHasAudio )
{
	int i = 0;
	if ( pLanguage && pLanguage[0] )
	{
		for ( i = 0; i < ARRAYSIZE( s_ValidLanguages ); i++ )
		{
			if ( !V_tier0_stricmp( pLanguage, s_ValidLanguages[i].pString ) )
			{
				i++;
				break;
			} 
		}
	}

	if ( i >= ARRAYSIZE( s_ValidLanguages ) )
	{
		// end of list
		return NULL;
	}

	if ( pbHasAudio )
	{
		*pbHasAudio = s_ValidLanguages[i].bLocalizedAudio;
	}
	return s_ValidLanguages[i].pString;
}

//-----------------------------------------------------------------------------
//	XBX_GetLanguageString
//
//	Returns the supported xbox language setting as a string, otherwise "english".
//-----------------------------------------------------------------------------
const char* XBX_GetLanguageString( void )
{
	return ( SupportedLanguageIDToString( GetLanguage() ) );
}

//-----------------------------------------------------------------------------
//	XBX_IsLocalized
//
//	Returns true if configured for a supported non-english localization.
//-----------------------------------------------------------------------------
bool XBX_IsLocalized( void )
{
	return ( V_tier0_stricmp( XBX_GetLanguageString(), "english" ) != 0 );
}

//-----------------------------------------------------------------------------
//	XBX_IsRestrictiveLanguage
//
//	Returns true if we are localized into one of the languages that needs
//	specialized handling
//-----------------------------------------------------------------------------
bool XBX_IsRestrictiveLanguage( void )
{
#ifdef _GAMECONSOLE
	// these languages have to use the xarial font mounted in memory
	// cannot determine this from the font system at the times we need it, encoded it here
	int languageId = GetLanguage();
	switch ( languageId )
	{
	case XC_LANGUAGE_KOREAN:
	case XC_LANGUAGE_JAPANESE:
	case XC_LANGUAGE_SCHINESE:
	case XC_LANGUAGE_TCHINESE:
		return true;
	}
#endif

	// all other languages can be handled normally
	return false;
}

//-----------------------------------------------------------------------------
//	XBX_IsAudioLocalized
//
//	Returns true if audio is localized.
//-----------------------------------------------------------------------------
bool XBX_IsAudioLocalized( void )
{
	// english is not a localized audio
	if ( XBX_IsLocalized() )
	{
		int languageId = GetLanguage();
		for ( int i = 0; i < ARRAYSIZE( s_ValidLanguages ); i++ )
		{
			if ( languageId == s_ValidLanguages[i].id )
			{
				return s_ValidLanguages[i].bLocalizedAudio;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
//	XBX_ResetStorageDeviceInfo
//
//	Returns the xbox storage device ID
//-----------------------------------------------------------------------------
void XBX_ResetStorageDeviceInfo()
{
	for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
		g_iStorageDeviceId[ k ] = XBX_INVALID_STORAGE_ID;
}

//-----------------------------------------------------------------------------
//	XBX_DescribeStorageDevice
//
//	Returns whether the storage device denotes a usable device or is
//	denoting a state of unavailable device.
//-----------------------------------------------------------------------------
DWORD XBX_DescribeStorageDevice( DWORD nStorageID )
{
	if ( nStorageID == XBX_INVALID_STORAGE_ID ||
		 nStorageID == XBX_STORAGE_DECLINED )
		 return 0;

	return 1;
}

//-----------------------------------------------------------------------------
//	XBX_MakeStorageContainerRoot
//
//	Returns whether the storage device denotes a usable device or is
//	denoting a state of unavailable device.
//-----------------------------------------------------------------------------
char const* XBX_MakeStorageContainerRoot( int iController, char const *szRootName, char *pBuffer, int numBufferBytes )
{
	if ( iController < 0 || iController >= XUSER_MAX_COUNT )
	{
		pBuffer[0] = '\0';
	}
	else
	{
		_snprintf( pBuffer, numBufferBytes, "X%d%s", iController, szRootName );
	}
	return pBuffer;
}


//-----------------------------------------------------------------------------
//	XBX_GetStorageDeviceId
//
//	Returns the xbox storage device ID for the given controller
//-----------------------------------------------------------------------------
DWORD XBX_GetStorageDeviceId( int iController )
{
#if defined( _DEMO ) && defined( _X360 )
	// Demos are not allowed to access storage devices
	return XBX_STORAGE_DECLINED;
#endif

	if ( iController >= 0 && iController < XUSER_MAX_COUNT )
		return g_iStorageDeviceId[ iController ];
	else
		return XBX_INVALID_STORAGE_ID;
}

//-----------------------------------------------------------------------------
//	XBX_SetStorageDeviceId
//
//	Sets the xbox storage device ID for the given controller
//-----------------------------------------------------------------------------
void XBX_SetStorageDeviceId( int iController, DWORD id )
{
	Msg( "XBX_SetStorageDeviceId: device 0x%08X set for ctrlr%d!\n",
		id, iController );

	if ( iController >= 0 && iController < XUSER_MAX_COUNT )
	{
		g_iStorageDeviceId[ iController ] = id;
	}
	else
	{
		Assert( iController >= 0 && iController < XUSER_MAX_COUNT );
	}
}

//-----------------------------------------------------------------------------
//	XBX_GetPrimaryUserId
//
//	Returns the active user ID
//-----------------------------------------------------------------------------
DWORD XBX_GetPrimaryUserId( void )
{
#ifdef _PS3
	return ( g_iPrimaryUserId != XBX_INVALID_USER_ID ) ? g_iPrimaryUserId : 0;
#else
	return g_iPrimaryUserId;
#endif
}

//-----------------------------------------------------------------------------
//	XBX_SetPrimaryUserId
//
//	Sets the active user ID
//-----------------------------------------------------------------------------
void XBX_SetPrimaryUserId( DWORD idx )
{
	g_iPrimaryUserId = idx;
}

//-----------------------------------------------------------------------------
//	XBX_GetPrimaryUserIsGuest
//
//	Returns zero (FALSE) if primary user is a real account
//	Returns non-zero (TRUE) if primary user is a guest account
//-----------------------------------------------------------------------------
DWORD XBX_GetPrimaryUserIsGuest( void )
{
	return g_bPrimaryUserIsGuest;
}

//-----------------------------------------------------------------------------
//	XBX_SetPrimaryUserIsGuest
//
//	Sets if primary user is a guest account
//-----------------------------------------------------------------------------
void XBX_SetPrimaryUserIsGuest( DWORD bPrimaryUserIsGuest )
{
	g_bPrimaryUserIsGuest = bPrimaryUserIsGuest;
}

//-----------------------------------------------------------------------------
//	XBX_GetNumGameUsers
//
//	Returns number of users for the game mode
//-----------------------------------------------------------------------------
DWORD XBX_GetNumGameUsers( void )
{
	return g_numGameUsers;
}

//-----------------------------------------------------------------------------
//	XBX_SetNumGameUsers
//
//	Sets number of users for the game mode
//-----------------------------------------------------------------------------
void XBX_SetNumGameUsers( DWORD numGameUsers )
{
	g_numGameUsers = numGameUsers;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the stored session ID for a cross-game invite
//-----------------------------------------------------------------------------
XNKID XBX_GetInviteSessionId( void )
{
	return g_InviteSessionId;
}

//-----------------------------------------------------------------------------
// Purpose: Store a session ID for an invitation
//-----------------------------------------------------------------------------
void XBX_SetInviteSessionId( XNKID nSessionId )
{
	g_InviteSessionId = nSessionId;
}

//-----------------------------------------------------------------------------
// Purpose: Get the Id of the user who received an invite
//-----------------------------------------------------------------------------

DWORD XBX_GetInvitedUserId( void )
{
#ifdef _PS3
	return ( g_InvitedUserId != XBX_INVALID_USER_ID ) ? 0 : XBX_INVALID_USER_ID; // invited user will be the primary user = 0
#else
	return g_InvitedUserId;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Set the Id of the user who received an invite
//-----------------------------------------------------------------------------
void XBX_SetInvitedUserId( DWORD nUserId )
{
	g_InvitedUserId = nUserId;
}

//-----------------------------------------------------------------------------
// Purpose: Get the Id of the user who received an invite
//-----------------------------------------------------------------------------

XUID XBX_GetInvitedUserXuid( void )
{
	return g_InvitedUserXuid;
}

//-----------------------------------------------------------------------------
// Purpose: Set the Id of the user who received an invite
//-----------------------------------------------------------------------------
void XBX_SetInvitedUserXuid( XUID xuid )
{
	g_InvitedUserXuid = xuid;
}

//-----------------------------------------------------------------------------
// Purpose: Maps the physical controllers to splitscreen slots
//-----------------------------------------------------------------------------
static int XBX_UserIndexToSlot[XUSER_MAX_COUNT] =
{
	0,
	1,
	2,
	3
};

//-----------------------------------------------------------------------------
// Purpose: Maps the splitscreen slots to physical controllers
//-----------------------------------------------------------------------------
static int XBX_SlotToUserIndex[XUSER_MAX_COUNT] =
{
	0,
	1,
	2,
	3
};

//-----------------------------------------------------------------------------
// Purpose: Maps slots to guest status
//-----------------------------------------------------------------------------
static DWORD XBX_SlotToUserIsGuest[XUSER_MAX_COUNT] =
{
	0,
	0,
	0,
	0
};

//-----------------------------------------------------------------------------
void XBX_ResetUserIdSlots()
{
	Msg( "XBX_ResetUserIdSlots\n" );

	for ( int i = 0; i < XUSER_MAX_COUNT; ++i )
	{
		XBX_UserIndexToSlot[i] = i;
		XBX_SlotToUserIndex[i] = i;
		XBX_SlotToUserIsGuest[i] = 0;
	}
}

//-----------------------------------------------------------------------------
void XBX_ClearUserIdSlots()
{
	Msg( "XBX_ClearUserIdSlots\n" );

	for ( int i = 0; i < XUSER_MAX_COUNT; ++i )
	{
		XBX_UserIndexToSlot[i] = (int)XBX_INVALID_USER_ID;
		XBX_SlotToUserIndex[i] = (int)XBX_INVALID_USER_ID;
		XBX_SlotToUserIsGuest[i] = 0;
	}
}

//-----------------------------------------------------------------------------
// Return the split screen slot based on a controller index
// May return -1 if no slot has been asssigned to that controller
int XBX_GetSlotByUserId( int idx ) 
{
	if ( idx >= 0 && idx <= XUSER_MAX_COUNT )
	{
		return XBX_UserIndexToSlot[ idx ];
	}
	else
	{
		Assert( idx >= 0 && idx <= XUSER_MAX_COUNT );
		return -1;
	}
}

//-----------------------------------------------------------------------------
// Return the controller assigned to a splitscreen slot
// May return -1 if no controller has been asssigned to that slot
int XBX_GetUserId( int nSlot ) 
{ 
	if ( nSlot >= 0 && nSlot <= MAX_SPLITSCREEN_CLIENTS )
	{
		return XBX_SlotToUserIndex[nSlot]; 
	}
	else
	{
		Assert( nSlot >= 0 && nSlot <= MAX_SPLITSCREEN_CLIENTS );
		return -1;
	}
}

//-----------------------------------------------------------------------------
void XBX_SetUserId( int nSlot, int idx ) 
{
	if ( ( nSlot >= 0 && nSlot <= MAX_SPLITSCREEN_CLIENTS ) &&
		 ( idx >= 0 && idx <= XUSER_MAX_COUNT ) )
	{
		XBX_SlotToUserIndex[nSlot] = idx;
		XBX_UserIndexToSlot[idx] = nSlot;
	}
	else 
	{
		Assert( "XBX_SetUserId" );
	}
}

//-----------------------------------------------------------------------------
void XBX_ClearSlot( int nSlot )
{
	if ( !( nSlot >= 0 && nSlot <= MAX_SPLITSCREEN_CLIENTS ) )
	{
		Assert( nSlot >= 0 && nSlot <= MAX_SPLITSCREEN_CLIENTS );
		return;
	}
	
	int iCtrlr = XBX_SlotToUserIndex[nSlot];
	if ( iCtrlr >= 0 && iCtrlr < XUSER_MAX_COUNT )
	{
		XBX_UserIndexToSlot[ iCtrlr ] = (int)XBX_INVALID_USER_ID;
	}
	
	XBX_SlotToUserIndex[nSlot] = (int)XBX_INVALID_USER_ID;
	XBX_SlotToUserIsGuest[nSlot] = 0;
}

//-----------------------------------------------------------------------------
void XBX_ClearUserId( int idx )
{
	if ( !( idx >= 0 && idx <= XUSER_MAX_COUNT ) )
	{
		Assert( idx >= 0 && idx <= XUSER_MAX_COUNT );
		return;
	}

	int iSlot = XBX_UserIndexToSlot[idx];
	if ( iSlot >= 0 && iSlot <= MAX_SPLITSCREEN_CLIENTS )
	{
		XBX_SlotToUserIndex[iSlot] = (int)XBX_INVALID_USER_ID;
		XBX_SlotToUserIsGuest[iSlot] = 0;
	}

	XBX_UserIndexToSlot[idx] = (int)XBX_INVALID_USER_ID;
}

//-----------------------------------------------------------------------------
DWORD XBX_GetUserIsGuest( int nSlot )
{
	if ( nSlot >= 0 && nSlot <= MAX_SPLITSCREEN_CLIENTS )
	{
		return XBX_SlotToUserIsGuest[nSlot];
	}
	else
	{
		Assert( nSlot >= 0 && nSlot <= MAX_SPLITSCREEN_CLIENTS );
		return 0;
	}
}

//-----------------------------------------------------------------------------
void XBX_SetUserIsGuest( int nSlot, DWORD dwUserIsGuest )
{
	if ( nSlot >= 0 && nSlot <= MAX_SPLITSCREEN_CLIENTS )
	{
		XBX_SlotToUserIsGuest[nSlot] = dwUserIsGuest;
	}
	else
	{
		Assert( nSlot >= 0 && nSlot <= MAX_SPLITSCREEN_CLIENTS );
	}
}


//-----------------------------------------------------------------------------
#ifdef _X360
static CXboxLaunch g_XBoxLaunch;
CXboxLaunch *XboxLaunch()
{
	return &g_XBoxLaunch;
}
#endif

//
// Xbx dynamic event queue implementation
//

void CXbxEventQueue::AppendEvent( xevent_t const &xevt )
{
	AUTO_LOCK( m_mtx );

	if ( m_bDispatching )
	{
		Assert( !m_bDispatching );
		Error( "CXbxEventQueue::AppendEvent during Dispatch is not allowed!\n" );
		return;
	}

	// Check if we need more capacity
	if ( m_dwCount == m_dwAllocated )
	{
		m_dwAllocated = MAX( m_dwAllocated * 2, s_nQueueNormalSize );	// New size is at least double old size or at least for 32 items
		xevent_t *pNewQueue = new xevent_t[ m_dwAllocated ];			// Allocate the new size
		if ( m_pQueue )
		{
			memcpy( pNewQueue, m_pQueue, m_dwCount * sizeof( xevent_t ) );	// Copy events queued so far
			delete [] m_pQueue;												// Free old event queue memory
		}
		m_pQueue = pNewQueue;											// Now we are pointing a larger chunk of memory, preserved data and size
	}

	m_pQueue[ m_dwCount ++ ] = xevt;
}

void CXbxEventQueue::Dispatch()
{
	AUTO_LOCK( m_mtx );

	if ( !ThreadInMainThread() )
	{
		Assert( !"ThreadInMainThread()" );
		Error( "CXbxEventQueue::Dispatch not on main thread!\n" );
		return;
	}

	if ( m_bDispatching )
	{
		Assert( !m_bDispatching );
		Error( "CXbxEventQueue::Dispatch is no reentry!\n" );
		return;
	}
	m_bDispatching = true;

	// pump event queue
	for ( DWORD k = 0; k < m_dwCount; ++ k )
	{
		xevent_t *pEvent = &m_pQueue[ k ];
		switch ( pEvent->event )
		{
		case XEV_REMOTECMD:
			XBX_ProcessXCommand( *pEvent );
			break;

		case XEV_LISTENER_NOTIFICATION:
			XBX_ProcessListenerNotification( *pEvent );
			break;
		}
	}

	m_dwCount = 0;
	if ( m_dwAllocated >= s_nQueueAbnormalSize )
	{
		m_dwAllocated = 0;
		delete [] m_pQueue;
		m_pQueue = NULL;
	}
	m_bDispatching = false;
}

bool CXbxEventQueue::IsDispatching() const
{
	// No mutex lock in this case because if the thread
	// is currently dispatching the queue, then it is currently
	// owning the queue mutex and can check the bool directly.
	// If the thread currently doesn't own the mutex, then the
	// operation on the queue will attempt to acquire the mutex
	// and it should be safe for the thread to stall anyway,
	// so the out-of-date information about whether the queue is
	// dispatching is not a concern.
	return m_bDispatching;
}
