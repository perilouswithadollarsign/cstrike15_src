//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Xbox console
//
//=====================================================================================//

#pragma once

#include "tier0/platform.h"

#ifndef STATIC_TIER0

#ifdef TIER0_DLL_EXPORT
	#define XBXCONSOLE_INTERFACE	DLL_EXPORT
	#define XBXCONSOLE_OVERLOAD	DLL_GLOBAL_EXPORT
#else
	#define XBXCONSOLE_INTERFACE	DLL_IMPORT
	#define XBXCONSOLE_OVERLOAD	DLL_GLOBAL_IMPORT
#endif

#else	// BUILD_AS_DLL

#define XBXCONSOLE_INTERFACE	extern
#define XBXCONSOLE_OVERLOAD	

#endif	// BUILD_AS_DLL

// all redirecting funneled here, stop redirecting in this module only
#undef OutputDebugString

#define XBX_MAX_PROFILE_COUNTERS 64

#if !defined( _X360 )
#define xMaterialList_t		void
#define xTextureList_t		void
#define xSoundList_t		void
#define xMapInfo_t			void
#define xModelList_t		void
#define xDataCacheItem_t	void
#define xVProfNodeItem_t	void
#define xBudgetInfo_t		void
#endif

class IXboxConsole
{
public:
	virtual void	SendRemoteCommand( const char *dbgCommand, bool bAsync ) = 0;
	virtual void	SendArbitraryPrefixedTextMessage( const char *prefix, const char *message, bool async ) = 0;
	virtual void	DebugString( unsigned int color, const char *format, ... ) = 0;
	virtual bool	IsConsoleConnected() = 0;
	virtual void	InitConsoleMonitor( bool bWaitForConnect = false ) = 0;
	virtual void	DisconnectConsoleMonitor() = 0;
	virtual void	FlushDebugOutput() = 0;
	virtual bool	GetXboxName( char *, unsigned * ) = 0;
	virtual void	CrashDump( bool ) = 0;
	virtual void	CrashDumpFullHeap( bool ) = 0;
	virtual void	DumpDllInfo( const char *pBasePath ) = 0;
	virtual void	OutputDebugString( const char * ) = 0;
	virtual bool	IsDebuggerPresent() = 0;

	virtual int		SetProfileAttributes( const char *pProfileName, int numCounters, const char *names[], COLORREF colors[] ) = 0;
	virtual void	SetProfileData( const char *pProfileName, int numCounters, unsigned int *counters ) = 0;
	virtual int		MemDump( const char *pDumpFileName ) = 0;
	virtual int		TimeStampLog( float time, const char *pString ) = 0;
	virtual int		MaterialList( int nMaterials, const xMaterialList_t *pXMaterialList ) = 0;
	virtual int		TextureList( int nTextures, const xTextureList_t *pXTextureList ) = 0;
	virtual int		SoundList( int nSounds, const xSoundList_t *pXSoundList ) = 0;
	virtual int		MapInfo( const xMapInfo_t *pXMapInfo ) = 0;
	virtual int		BudgetInfo( const xBudgetInfo_t *pXBudgetInfo ) = 0;
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
};

class CXboxConsole : public IXboxConsole
{
public:
	void	SendRemoteCommand( const char* dbgCommand, bool bAsync );
	void	SendArbitraryPrefixedTextMessage( const char *prefix, const char *message, bool async );
	void	DebugString( unsigned int color, const char* format, ... );
	bool	IsConsoleConnected();
	void	InitConsoleMonitor( bool bWaitForConnect = false );
	void	DisconnectConsoleMonitor();
	void	FlushDebugOutput();
	bool	GetXboxName( char *, unsigned * );
	void	CrashDump( bool );
	void	CrashDumpFullHeap( bool );
	int		DumpModuleSize( const char *pName );
	void	DumpDllInfo( const char *pBasePath );
	void	OutputDebugString( const char * );
	bool	IsDebuggerPresent();

	int		SetProfileAttributes( const char *pProfileName, int numCounters, const char *names[], COLORREF colors[] );
	void	SetProfileData( const char *pProfileName, int numCounters, unsigned int *counters );
	int		MemDump( const char *pDumpFileName );
	int		TimeStampLog( float time, const char *pString );
	int		MaterialList( int nMaterials, const xMaterialList_t *pXMaterialList );
	int		TextureList( int nTextures, const xTextureList_t *pXTextureList );
	int		SoundList( int nSounds, const xSoundList_t *pXSoundList );
	int		MapInfo( const xMapInfo_t *pXMapInfo );
	int		BudgetInfo( const xBudgetInfo_t *pXBudgetInfo );
	int		AddCommands( int numCommands, const char *commands[], const char *help[] );
	int		ModelList( int nModels, const xModelList_t *pList );
	int		DataCacheList( int nItems, const xDataCacheItem_t *pItems );
	int		VProfNodeList( int nItems, const xVProfNodeItem_t *pItems );
	int		TraceComplete( void );
	int		BugReporter( void );
	bool	SendBinaryData( const void *pData, int iDataSize, bool bAsync, DWORD dwSyncTimout );
	int		SyncDvdDevCache();
	int		SyncShaderCache();
	int		Version( int nVersion );
};

XBXCONSOLE_INTERFACE IXboxConsole *g_pXboxConsole;
XBXCONSOLE_INTERFACE void XboxConsoleInit();

#define XBX_SendRemoteCommand			if ( !g_pXboxConsole ) ; else g_pXboxConsole->SendRemoteCommand
#define XBX_SendPrefixedMsg				if ( !g_pXboxConsole ) ; else g_pXboxConsole->SendArbitraryPrefixedTextMessage
#define XBX_DebugString					if ( !g_pXboxConsole ) ; else g_pXboxConsole->DebugString
#define XBX_IsConsoleConnected			( !g_pXboxConsole ) ? false : g_pXboxConsole->IsConsoleConnected
#define XBX_InitConsoleMonitor			if ( !g_pXboxConsole ) ; else g_pXboxConsole->InitConsoleMonitor
#define XBX_DisconnectConsoleMonitor	if ( !g_pXboxConsole ) ; else g_pXboxConsole->DisconnectConsoleMonitor
#define XBX_FlushDebugOutput			if ( !g_pXboxConsole ) ; else g_pXboxConsole->FlushDebugOutput
#define XBX_GetXboxName					( !g_pXboxConsole ) ? false : g_pXboxConsole->GetXboxName
#define XBX_CrashDump					if ( !g_pXboxConsole ) ; else g_pXboxConsole->CrashDump
#define XBX_CrashDumpFullHeap			if ( !g_pXboxConsole ) ; else g_pXboxConsole->CrashDumpFullHeap
#define XBX_DumpDllInfo					if ( !g_pXboxConsole ) ; else g_pXboxConsole->DumpDllInfo
#define XBX_OutputDebugString			if ( !g_pXboxConsole ) ; else g_pXboxConsole->OutputDebugString
#define XBX_IsDebuggerPresent			( !g_pXboxConsole ) ? false : g_pXboxConsole->IsDebuggerPresent

#define XBX_rSetProfileAttributes		( !g_pXboxConsole ) ? 0 : g_pXboxConsole->SetProfileAttributes
#define XBX_rSetProfileData				if ( !g_pXboxConsole ) ; else g_pXboxConsole->SetProfileData
#define XBX_rMemDump					( !g_pXboxConsole ) ? 0 : g_pXboxConsole->MemDump
#define XBX_rTimeStampLog				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->TimeStampLog
#define XBX_rMaterialList				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->MaterialList
#define XBX_rTextureList				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->TextureList
#define XBX_rSoundList					( !g_pXboxConsole ) ? 0 : g_pXboxConsole->SoundList
#define XBX_rMapInfo					( !g_pXboxConsole ) ? 0 : g_pXboxConsole->MapInfo
#define XBX_rBudgetInfo					( !g_pXboxConsole ) ? 0 : g_pXboxConsole->BudgetInfo
#define XBX_rAddCommands				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->AddCommands
#define XBX_rModelList					( !g_pXboxConsole ) ? 0 : g_pXboxConsole->ModelList
#define XBX_rDataCacheList				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->DataCacheList
#define XBX_rVProfNodeList				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->VProfNodeList
#define XBX_rTraceComplete				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->TraceComplete
#define XBX_rBugReporter				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->BugReporter
#define XBX_SendBinaryData				( !g_pXboxConsole ) ? 0 : g_pXboxConsole->SendBinaryData
#define XBX_rSyncDvdDevCache			( !g_pXboxConsole ) ? 0 : g_pXboxConsole->SyncDvdDevCache
#define XBX_rSyncShaderCache			( !g_pXboxConsole ) ? 0 : g_pXboxConsole->SyncShaderCache
#define XBX_rVersion					( !g_pXboxConsole ) ? 0 : g_pXboxConsole->Version



