#include "stdafx.h"
#include "mapdoc.h"
#include "mapview.h"
#include <wintab.h>

#define PACKETDATA	PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE
#define PACKETMODE	0

#include <pktdef.h>


#define MAX_PACKETS 1000


static LOGCONTEXT LogContext;
static HCTX hGlobalContext;
static AXIS NormalAxis;
static float m_flLastPressure;
static bool bWinTabAvailable = false;
static bool bWinTabOpened = false;
static bool bLMBDown = false;
static DWORD	nLastTime = 0;


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool WinTab_Init( )
{
	WORD errmode;

	errmode = SetErrorMode( SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS );

	bWinTabAvailable = WTInfo( 0, 0, NULL ) != 0;

	SetErrorMode( errmode );

	m_flLastPressure = 1.0f;

	return bWinTabAvailable;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void WinTab_Open( HWND hWnd )
{
	if ( bWinTabAvailable == false )
	{
		return;
	}

//	pWTMutex = new CMutex( TRUE, NULL, NULL );

	memset( &LogContext, 0, sizeof( LogContext ) );

	// Get default context information
	WTInfo( WTI_DEFCONTEXT, 0, &LogContext );

	// Open the context
	LogContext.lcPktData = PACKETDATA;
	LogContext.lcPktMode = PACKETMODE;
	LogContext.lcOptions = CXO_MESSAGES | CXO_SYSTEM;

	hGlobalContext = WTOpen( hWnd, &LogContext, TRUE );

	bWinTabOpened = true;

	WTInfo( WTI_DEVICES + LogContext.lcDevice, DVC_NPRESSURE, &NormalAxis );

	WTQueueSizeSet( hGlobalContext, MAX_PACKETS );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void WinTab_Packet( WPARAM wSerial, LPARAM hContext )
{
//	CSingleLock lock( pWTMutex, TRUE );

	PACKET	WTPacketData[ MAX_PACKETS ];

	int nCount = WTPacketsGet( ( HCTX )hContext, MAX_PACKETS, WTPacketData );

	if ( nCount == 0 )
	{
		return;
	}

	m_flLastPressure = ( float )WTPacketData[ 0 ].pkNormalPressure / ( float )NormalAxis.axMax;

//	Msg( "%d %d %d %d %d  %g\n", WTPacketData.pkTime, WTPacketData.pkButtons, WTPacketData.pkX, WTPacketData.pkY, WTPacketData.pkNormalPressure, m_flLastPressure );

	CMapDoc	*pMapDoc = CMapDoc::GetActiveMapDoc();

	if ( pMapDoc != NULL )
	{
		CMapView *pMapView = pMapDoc->GetActiveMapView();

		if ( pMapView != NULL )
		{
			POINT	MousePosition;

			GetCursorPos( &MousePosition );

			AfxGetApp()->m_pMainWnd->ScreenToClient( &MousePosition );

			if ( m_flLastPressure >= 0.05f && bLMBDown == false )
			{
				bLMBDown = true;
//				SendMessage( pMapView->GetViewWnd()->m_hWnd, WM_LBUTTONDOWN, 0, MAKELPARAM( MousePosition.x, MousePosition.y ) );
			}
			if ( m_flLastPressure < 0.02f && bLMBDown == true )
			{
				bLMBDown = false;
//				SendMessage( pMapView->GetViewWnd()->m_hWnd, WM_LBUTTONUP, 0, MAKELPARAM( MousePosition.x, MousePosition.y ) );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
float WinTab_GetPressure( )
{
	if ( bLMBDown == true )
	{
		return m_flLastPressure;
	}

	return 1.0f;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool WinTab_Opened( )
{
	return bWinTabOpened;
}
