//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_SFX_H
#define SND_SFX_H

#if defined( _WIN32 )
#pragma once
#endif

class CAudioSource;

class CSfxTable
{
public:
	CSfxTable();

	// gets sound name, possible decorated with prefixes
	virtual const char	*getname( char *pBuf, size_t bufLen );
	// gets the filename, the part after the optional prefixes
	const char			*GetFileName(char *pBuf, size_t bufLen );
	FileNameHandle_t	GetFileNameHandle();

	void				SetNamePoolIndex( int index );
	bool				IsPrecachedSound();
	void				OnNameChanged( const char *pName );

	int					m_namePoolIndex;
	CAudioSource		*pSource;

	bool				m_bUseErrorFilename : 1;
	bool				m_bIsUISound : 1;
	bool				m_bIsLateLoad : 1;
	bool				m_bMixGroupsCached : 1;
	bool				m_bIsMusic : 1;
	bool				m_bIsCreatedByQueuedLoader : 1;

	byte				m_mixGroupCount;
	// UNDONE: Use a fixed bit vec here?
	byte				m_mixGroupList[8];

private:
	// Only set in debug mode so you can see the name.
	const char *m_pDebugName;
};

#endif // SND_SFX_H
