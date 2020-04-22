// ps_console.cpp - for communicating with vxconsole_ps3

#include <string.h>
#include <sys/event.h>
#include <string.h>

#include "ps3/ps3_console.h"
#include "ps3/ps3_vxconsole.h"
#include "ps3/ps3_win32stubs.h"
#include "../utils/ps3/vpbdm/vpbdm_exports.h"
#include "ps3/ps3_helpers.h"
#include "ps3_pathinfo.h"
// #include "cmd.h"

#ifdef _RETAIL
//Stubs for retail
bool PS3_DebugString( unsigned int color, const char* format, ... )
{
	return false;
}
bool PS3_IsConsoleConnected()
{
	return false;
}
void PS3_InitConsoleMonitor( bool bWaitForConnect)
{

}
void PS3_UpdateConsoleMonitor()
{

}

int XBX_rAddCommands(int numCommands, const char* commands[], const char* help[])
{
	return 0;
}
#else
//Development

static PS3_LoadAppSystemInterface_Parameters_t s_VXBDMPrxLoadParameters;
IPS3Console *g_pValvePS3Console = NULL;

// an empty implementation to use if the debug PRX isn't available
class CPS3DummyDebugConsole : public IPS3Console
{
public:
	virtual void	SendRemoteCommand( const char *dbgCommand, bool bAsync ) {}
	virtual void	SendPrefixedDECIMessage( const char *prefix, const char *message, bool async ) {};
	virtual void	DebugString( unsigned int color, const char *format, ... ) {}
	virtual bool	IsConsoleConnected() {return false;}
	virtual void	InitConsoleMonitor( bool bWaitForConnect = false ) {}
	virtual void	DisconnectConsoleMonitor() {}
	virtual void	FlushDebugOutput() {}
	virtual bool	GetXboxName( char *, unsigned * ) { return false; }
	virtual void	CrashDump( bool ) {}
	virtual void	CrashDumpFullHeap( bool ) {}
	virtual void	DumpDllInfo( const char *pBasePath ) {}
	// virtual void	OutputDebugString( const char * ) = 0;
	virtual bool	IsDebuggerPresent() { return false; }

	virtual int		SetProfileAttributes( const char *pProfileName, int numCounters, const char *names[], COLORREF colors[] ) { return 0; }
	virtual void	SetProfileData( const char *pProfileName, int numCounters, unsigned int *counters ) {}
	virtual int		MemDump( const char *pDumpFileName ) { return -1; }
	virtual int		TimeStampLog( float time, const char *pString ) { return -1; }
	virtual int		MaterialList( int nMaterials, const xMaterialList_t *pXMaterialList ) { return -1; }
	virtual int		TextureList( int nTextures, const xTextureList_t *pXTextureList ) { return -1; }
	virtual int		SoundList( int nSounds, const xSoundList_t *pXSoundList ) { return -1; }
	virtual int		MapInfo( const xMapInfo_t *pXMapInfo ) { return -1; }
	virtual int		AddCommands( int numCommands, const char *commands[], const char* help[] ) { return -1; }
	virtual int		ModelList( int nModels, const xModelList_t *pList ) { return -1; }
	virtual int		DataCacheList( int nItems, const xDataCacheItem_t* pItems ) { return -1; }
	virtual int		VProfNodeList( int nItems, const xVProfNodeItem_t *pItems ) { return -1; }
	virtual int		TraceComplete( void ) { return -1; }
	virtual int		BugReporter( void ) { return -1; }
	virtual bool	SendBinaryData( const void *pData, int iDataSize, bool bAsync = true, DWORD dwSyncTimout = 15000 ) { return false; }
	virtual int		SyncDvdDevCache() { return -1; }
	virtual int		SyncShaderCache() { return -1; }
	virtual int		Version( int nVersion ) { return -1; }
	virtual void	TransmitScreenshot( char *pFrameBuffer, uint32 uWidth, uint32 uHeight, uint32 uPitch, uint32 uColorFmt ){  }

	virtual void PumpMessage( fRemoteCommandSink_t ) {}
	virtual int	SendBinaryDECI( const uint8 *dbgCommand, uint length, bool ){return 0;}

	virtual void AddOnConnectDelegate( fOnConnectDelegate_t pOnConnectDelegate ) {};
	virtual void AddOnDisconnectDelegate( fOnDisconnectDelegate_t pOnDisconnectDelegate ) {};
};
CPS3DummyDebugConsole g_ValveDummyDebugConsoleForUseWhenThePRXContainingTheRealOneIsntAvailable;

// try to load the debug library 
void ValvePS3ConsoleInit()
{
	if ( g_pValvePS3Console != NULL ) 
	{
		AssertMsg(false,"Called ValvePS3ConsoleInit twice!\n");
		// emergency cleanup
		ValvePS3ConsoleShutdown();
		g_pValvePS3Console = NULL;
	}

	memset( &s_VXBDMPrxLoadParameters, 0, sizeof( s_VXBDMPrxLoadParameters ) );
	s_VXBDMPrxLoadParameters.cbSize = sizeof( s_VXBDMPrxLoadParameters );

	char szAbsoluteModuleName[1024];
	// getcwd not supported on ps3; use PRX path instead (TODO: fallback to DISK path too)
	snprintf( szAbsoluteModuleName, sizeof(szAbsoluteModuleName), "%s/%s",
		g_pPS3PathInfo->PrxPath(), "vxbdm_ps3.sprx" );

	int loadresult = IsCert() ? ENOENT : PS3_PrxLoad( szAbsoluteModuleName, &s_VXBDMPrxLoadParameters );
	if ( loadresult < CELL_OK )
	{
		// we failed to load the module. This might be because we're a cert build, so it's fine.
		Msg("VXBDM: not loaded\n");
		g_pValvePS3Console = &g_ValveDummyDebugConsoleForUseWhenThePRXContainingTheRealOneIsntAvailable;
	}
	else // loaded successfully 
	{
		g_pValvePS3Console = reinterpret_cast< IPS3Console *> ( (*s_VXBDMPrxLoadParameters.pfnCreateInterface)(NULL, NULL) );
		Msg("VXBDM: loaded %x!\n", loadresult);
	}
}

void ValvePS3ConsoleShutdown()
{
	if ( !g_pValvePS3Console )
		return;

	g_pValvePS3Console = NULL;
	PS3_PrxUnload( s_VXBDMPrxLoadParameters.sysPrxId );
};


//defined in ps3_events.cpp, needed to process commands sent from the VXConsole
extern void XBX_ProcessXCommand(const char* command);


bool PS3_DebugString( unsigned int color, const char* format, ... )
{
	return false;
}

bool PS3_IsConsoleConnected()
{
	return false;
}

int XBX_rAddCommands(int numCommands, const char* commands[], const char* help[])
{
#if 0
	// PS3_UpdateConsoleMonitor();

	if(PS3_IsConsoleConnected())
	{
		/*
		for(int i=0;i<numCommands;i++)
		{
			sprintf(ps3ConsoleBuffer, "AddCommand() %s %s", commands[i], help[i]);
			sys_deci3_send(ps3ConsoleSessionId,(uint8_t*)ps3ConsoleBuffer,strlen(ps3ConsoleBuffer)+1);
		}
		return 1;
		*/
	}
#else
	Error("Deprecated function called.\n");
#endif
	return 0;
}

void PS3_InitConsoleMonitor( bool bWaitForConnect )
{
	Error("You didn't implement this you idiot\n");
}


//-----------------------------------------------------------------------------
//	XBX_ProcessXCommand
//
//-----------------------------------------------------------------------------
void XBX_ProcessXCommand(const char* command)
{
#if 0 // This is the Windows way of doing things. We are not Windows.
	// if we WERE Windows, we would do this, which would call a function
	// which found a function pointer which called a function which
	// dispatched a switch which etc. etc....
	// by the way, function call overhead is at least 21 cycles on PS3.
	// remote command
	// pass it game via windows message
	HWND hWnd = GetFocus();
	WNDPROC windowProc = ( WNDPROC)GetWindowLong(hWnd, GWL_WNDPROC );
	if ( windowProc )
	{
		windowProc( hWnd, WM_XREMOTECOMMAND, 0, (LPARAM)command );
	}
#elif 0 // until finally just doing this:
	ECommandTarget_t t = Cbuf_GetCurrentPlayer();
	Cbuf_AddText( t, command );
	Cbuf_AddText( t, "\n" );
#else
#pragma message("You must implement XBX_ProcessXCommand")
#endif
}

void PS3_ReceiveCommand(const char *strCommand)
{
#if 0
	// skip over the command prefix and the exclamation mark
	strCommand += strlen( XBX_DBGCOMMANDPREFIX ) + 1;

	if ( strCommand[0] == '\0' )
	{
		// just a ping
		char tmp[1]={'\0'};
		sys_deci3_send(ps3ConsoleSessionId,(uint8_t*)tmp,1);
		goto cleanUp;
	}

	if ( strncmp( strCommand, "__connect__", 11 ) ==0)
	{
		//respond that we're connected
		sys_deci3_send(ps3ConsoleSessionId,(uint8_t*)strCommand,strlen(strCommand)+1);

		if(!isConsoleConnected)
		{
			//initial connect - get the console variables (used for autocomplete on the vxconsole)
			XBX_ProcessXCommand("getcvars");
		}
		isConsoleConnected=true;
		goto cleanUp;
	}

	if ( strncmp( strCommand, "__disconnect__", 14 ) ==0)
	{
		isConsoleConnected=false;
		goto cleanUp;
	}

	if ( strncmp( strCommand, "__complete__", 12 ) ==0)
	{
		//isn't used for ps3 vxconsole
		goto cleanUp;
	}

	if ( strncmp( strCommand, "__memory__", 10 ) ==0)
	{		
		//...
		goto cleanUp;
	}

	XBX_ProcessXCommand(strCommand);
cleanUp:
	return;
#else
	Warning("PS3_ReceiveCommand() is not implemented.\n");
#endif
}

void PS3_UpdateConsoleMonitor()
{
#if 0
	if(ps3ConsoleSessionId==-1)
		return;

	const int MAX_EVENTS=32;
	int numEvents;
	sys_event_t events[MAX_EVENTS];
	int ret = sys_event_queue_tryreceive(ps3ConsoleEventQueue, events,MAX_EVENTS, &numEvents);
	if(ret!=CELL_OK) 
	{
		//fprintf(stderr, "sys_event_queue_tryreceive() failed: %d\n", ret);
		return;
	}

	for(int i=0;i<numEvents;i++)
	{
		sys_event &event=events[i];
		switch(event.data1)
		{
		case SYS_DECI3_EVENT_COMM_ENABLED:
			//(wait for a "__connect__" packet instead of assuming an enabled connection means its working)
			//isConsoleConnected=true;
			break;
		case SYS_DECI3_EVENT_COMM_DISABLED:
			isConsoleConnected=false;
			break;
		case SYS_DECI3_EVENT_DATA_READY:
			ret = sys_deci3_receive(ps3ConsoleSessionId,(uint8_t*)ps3ConsoleBuffer,event.data2);
			if(ret==CELL_OK)
			{
				PS3_ReceiveCommand(ps3ConsoleBuffer);
			}
			break;
		}
	}
#endif
}

int	XBX_rSetProfileAttributes(const char *pProfileName, int numCounters, const char *names[], COLORREF colors[])
{
	return ( !g_pValvePS3Console ) ? 0 : g_pValvePS3Console->SetProfileAttributes( pProfileName, numCounters, names, colors );
}

void XBX_rSetProfileData( const char *pProfileName, int numCounters, unsigned int *counters )
{
	g_pValvePS3Console->SetProfileData( pProfileName, numCounters, counters );
}

void XBX_rVProfNodeList( int nItems, const xVProfNodeItem_t *pItems )
{
	g_pValvePS3Console->VProfNodeList( nItems, pItems );
}

#endif//!_RETAIL
