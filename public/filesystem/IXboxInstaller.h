//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef XBOXINSTALLER_H
#define XBOXINSTALLER_H

#include "tier0/platform.h"

#if defined( PLATFORM_X360 )

#ifdef _WIN32
#pragma once
#endif

#include "appframework/iappsystem.h"

#define SOURCE_SECTOR_SIZE	2048	// DVD Sector Size
#define TARGET_SECTOR_SIZE	512		// HDD Sector Size

// all CStrike15 cached files relative to this, having a parent dir is critical
// the cache is a shared resource among titles, using the simple root would be VERY bad
#define CACHE_PATH_CSTIKRE15		"cache:/cs1501"

struct CopyStats_t
{
	DWORD				m_InstallStartTime;
	DWORD				m_InstallStopTime;

	char				m_srcFilename[MAX_PATH];
	char				m_dstFilename[MAX_PATH];

	DWORD				m_ReadSize;
	DWORD				m_WriteSize;
	DWORD				m_BytesCopied;
	DWORD				m_TotalReadTime;
	DWORD				m_TotalWriteTime;
	DWORD				m_TotalReadSize;
	DWORD				m_TotalWriteSize;
	DWORD				m_BufferReadSize;
	DWORD				m_BufferWriteSize;
	DWORD				m_BufferReadTime;
	DWORD				m_BufferWriteTime;
	DWORD				m_CopyTime;
	DWORD				m_CopyErrors;
	DWORD				m_NumReadBuffers;
	DWORD				m_NumWriteBuffers;
};

abstract_class IXboxInstaller : public IAppSystem
{
public:
	virtual bool				Setup( bool bForceInstall = false ) = 0;
	virtual void				ResetSetup() = 0;

	virtual bool				Start() = 0;
	virtual void				Stop() = 0;
	virtual bool				IsStopped( bool bForceStop ) = 0;

	virtual DWORD				GetTotalSize() = 0;
	virtual DWORD				GetVersion() = 0;
	virtual const CopyStats_t	*GetCopyStats() = 0;
	virtual bool				IsInstallEnabled() = 0;
	virtual bool				IsFullyInstalled() = 0;

	// hint to the other systems that may allow a restart
	virtual bool				ShouldRestart() = 0;

	// prefer a restart, but until that happens...
	// called by other systems when safe to alter search paths
	virtual bool				ForceCachePaths() = 0;
	virtual void				SpewStatus() = 0;
};

DECLARE_TIER1_INTERFACE( IXboxInstaller, g_pXboxInstaller );

extern bool IsAlreadyInstalledToXboxHDDCache();

#endif // _X360

#endif // XBOXINSTALLER_H






