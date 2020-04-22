#ifndef PS3_CONSOLE_H
#define PS3_CONSOLE_H

#include <cell/cell_fs.h>
#include "ps3_platform.h"
#include <sysutil/sysutil_sysconf.h>
#include "ps3/ps3_vxconsole.h"
#include "ps3/ps3_core.h"
#include "tier0/dbg.h"

#if 0
//-- The following, subject to being moved somewhere more sensible - DL
#define PS3_SYSUTIL_CALLBACK_SLOT 0
#define SOUND_SYSUTIL_CALLBACK_SLOT 1

bool PS3_RegisterSysUtilCallback( void );
bool PS3_RegisterDiscChangeCallback( void );
void PS3_UnRegisterSysUtilCallback( void );
void PS3_UnRegisterDiscChangeCallback( void );
void PS3_CheckSysUtilCallback( void );

int  PS3_OpenSystemConfigurationDialog(CellSysconfType iConfType,CellSysconfCallback pCallBack,  void * pUserData );
void PS3_CloseSystemConfigurationDialog();
void PS3_CleanSystemConfigurationDialog();

//--
#endif


/*
//void PS3_SendRemoteCommand( const char* dbgCommand, bool bAsync );
bool PS3_DebugString( unsigned int color, const char* format, ... );
bool PS3_IsConsoleConnected();
void PS3_InitConsoleMonitor( bool bWaitForConnect = false );
int XBX_rAddCommands(int numCommands, const char* commands[], const char* help[]);

//updates the status of the console connection and handles new messages
void PS3_UpdateConsoleMonitor();


//language settings
const char*  PS3_GetLanguageString( void );
bool		 PS3_IsLocalized( void );
*/

// for integration with vxconsole profile bars:
#define XBX_MAX_PROFILE_COUNTERS 64
#define XMAKECOLOR( r, g, b )			((unsigned int)(((unsigned char)(r)|((unsigned int)((unsigned char)(g))<<8))|(((unsigned int)(unsigned char)(b))<<16)))



class IPS3Console
{
public:
	// // // X360 console interface
	virtual void	SendRemoteCommand( const char *dbgCommand, bool bAsync ) = 0;
	virtual void	SendPrefixedDECIMessage( const char *prefix, const char *message, bool async ) = 0;
	virtual void	DebugString( unsigned int color, const char *format, ... ) = 0;
	virtual bool	IsConsoleConnected() = 0;
	virtual void	InitConsoleMonitor( bool bWaitForConnect = false ) = 0;
	virtual void	DisconnectConsoleMonitor() = 0;
	virtual void	FlushDebugOutput() = 0;
	virtual bool	GetXboxName( char *, unsigned * ) = 0;
	virtual void	CrashDump( bool ) = 0;
	virtual void	CrashDumpFullHeap( bool ) = 0;
	virtual void	DumpDllInfo( const char *pBasePath ) = 0;
	// virtual void	OutputDebugString( const char * ) = 0;
	virtual bool	IsDebuggerPresent() = 0;

	virtual int		SetProfileAttributes( const char *pProfileName, int numCounters, const char *names[], COLORREF colors[] ) = 0;
	virtual void	SetProfileData( const char *pProfileName, int numCounters, unsigned int *counters ) = 0;
	virtual int		MemDump( const char *pDumpFileName ) = 0;
	virtual int		TimeStampLog( float time, const char *pString ) = 0;
	virtual int		MaterialList( int nMaterials, const xMaterialList_t *pXMaterialList ) = 0;
	virtual int		TextureList( int nTextures, const xTextureList_t *pXTextureList ) = 0;
	virtual int		SoundList( int nSounds, const xSoundList_t *pXSoundList ) = 0;
	virtual int		MapInfo( const xMapInfo_t *pXMapInfo ) = 0;
	virtual int		AddCommands( int numCommands, const char *commands[], const char* help[] ) = 0;
	virtual int		ModelList( int nModels, const xModelList_t *pList ) = 0;
	virtual int		DataCacheList( int nItems, const xDataCacheItem_t* pItems ) = 0;
	virtual int		VProfNodeList( int nItems, const xVProfNodeItem_t *pItems ) = 0;
	virtual int		TraceComplete( void ) = 0;
	virtual int		BugReporter( void ) = 0;
	virtual bool	SendBinaryData( const void *pData, int iDataSize, bool bAsync = true, DWORD dwSyncTimout = 15000 ) = 0; //returns false if sync call timed out or not connected. Otherwise true
	virtual int		SyncDvdDevCache() = 0;
	virtual int		SyncShaderCache() = 0;
	virtual int		Version( int nVersion ) = 0;

	// // // PS3 specific

	// manually send a frame buffer across the DECI wire because the VRAM capture comes with all
	// sorts of untenable encumberances
	virtual void		TransmitScreenshot( char *pFrameBuffer, uint32 uWidth, uint32 uHeight, uint32 uPitch, uint32 uColorFmt ) = 0;

	// send an arbitrary blob of data of a given length. You have to supply the prefix yourself (avoids an unnecessary additional copy).
	virtual int	SendBinaryDECI( const uint8 *dbgCommand, uint length, bool bHaltOnError = true ) = 0;

	// a function that receives fake xbox messages
	typedef void (*fRemoteCommandSink_t)(const char *pRemoteCommandBuffer);
	// synchronous message pumping, could make an async thread in the future if necessary
	virtual void PumpMessage( fRemoteCommandSink_t ) = 0;  // pump up to ONE remote command message synchronously

	// Add delegates that are called when the console is connected/disconnected to VXConsole
	typedef void (*fOnConnectDelegate_t)( void );
	typedef void (*fOnDisconnectDelegate_t)( void );
	virtual void AddOnConnectDelegate( fOnConnectDelegate_t pOnConnectDelegate ) = 0;
	virtual void AddOnDisconnectDelegate( fOnDisconnectDelegate_t pOnDisconnectDelegate ) = 0;
};


PLATFORM_INTERFACE IPS3Console *g_pValvePS3Console;
PLATFORM_INTERFACE void ValvePS3ConsoleInit();
PLATFORM_INTERFACE void ValvePS3ConsoleShutdown();

#endif //PS3_CONSOLE_H

