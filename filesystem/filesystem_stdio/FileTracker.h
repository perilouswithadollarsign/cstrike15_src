//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef FILETRACKER_H
#define FILETRACKER_H
#ifdef _WIN32
#pragma once
#endif


#include "ifilelist.h"


class CBaseFileSystem;
class CFileHandle;


enum EFileFlags
{
	k_eFileFlagsLoadedFromSteam			= 0x0001,
	
	k_eFileFlagsHasCRC					= 0x0002,	// m_CRC represents the most recently-loaded version of the file. This might be 
													// unset but m_CRC (and k_eFileFlagsGotCRCOnce) could be set, signifying that
													// the file has been opened since last time we calculated the CRC but we didn't
													// calculate a CRC for that file version.
	
	k_eFileFlagsForcedLoadFromSteam		= 0x0004,	// Set if k_eFileFlagsLoadedFromSteam is set AND we forced Steam to not check the disk for this file.
	
	k_eFileFlagsGotCRCOnce				= 0x0008,	// This is set if we EVER had k_eFileFlagsHasCRC set.. m_CRC will still be set in that case,
													// but it'll be the last CRC we calculated and not necessarily 
	
	k_eFileFlagsFailedToLoadLastTime	= 0x0010	// This is used if we had a record of the file and the game tried to open it, but
													// it couldn't be opened. This will happen if the file was loaded from disk, then
													// sv_pure is turned on and the file is forced to come from Steam, but there is no
													// Steam version. In that case, the game should be told to retry the file
													// next time sv_pure is changed because if sv_pure goes back to 0 it -would- load
													// the file legitimately.
};


class CPathIDFileList;


class CFileInfo
{
public:
	CFileInfo( const char *pFilename );
	~CFileInfo();
	
	const char* GetFilename();
	const char* GetPathIDString();
	
public:	
	//TODO: Optimize this. Probably should use a hierarchical structure with a list of files in each directory.
	char			*m_pFilename;
	unsigned short	m_Flags;	// This is a combination of EFileFlags.
	CRC32_t			m_CRC;		// The CRC for this file.
	CPathIDFileList	*m_pPathIDFileList;
	int				m_iNeedsVerificationListIndex;	// Index into m_NeedsVerificationList or -1 if not in the list.
};


// This tracks a list of files for the specified path ID.
class CPathIDFileList
{
public:
	~CPathIDFileList();
	CFileInfo*		FindFileInfo( const char *pFilename );
	CFileInfo*		AddFileInfo( const char *pFilename );

public:
	CUtlSymbol						m_PathID;				// "" for a null path ID.
	CUtlLinkedList<CFileInfo*,int>	m_Files;
	CUtlLinkedList<CFileInfo*,int>	m_UnverifiedCRCFiles;	// These are the files whose CRCs have not been verified yet.
															// These just point at entries in m_Files.
};


//-----------------------------------------------------------------------------
// This tracks the files that have been opened by the filesystem.
// It remembers if they were loaded from Steam or off-disk.
// If the filesystem is tracking CRCs, then it will calculate a CRC
// for each file that came off disk.
//
// TODO: This is similar to CBaseFileSystem::m_OpenedFiles - it could probably
// manage both sets of files in the same list. Having 2 separate lists might
// be confusing.
//-----------------------------------------------------------------------------
class CFileTracker
{
public:
	CFileTracker( CBaseFileSystem *pFileSystem );
	~CFileTracker();

	// If this is true, then we'll calculate CRCs for each file that came off disk.
	void	SetWantFileCRCs( bool bWantCRCs );

	// As files are opened, if it is calculating CRCs, it will add those files and their
	// CRCs to the "unverified CRC" list. The client can then ask the server to verify
	// those CRCs to make sure the client is "pure".
	void	MarkAllCRCsUnverified();
	void	MarkAllCRCsVerified( bool bLockMutex=true );

	// Cache a file's CRC. Loads the file and calculates the CRC if we don't have it yet.
	void			CacheFileCRC( const char *pPathID, const char *pRelativeFilename );
	EFileCRCStatus	CheckCachedFileCRC( const char *pPathID, const char *pRelativeFilename, CRC32_t *pCRC );

	// This is like CacheFileCRC, but it assumes that the same file would be found by pPathIDToCopyFrom, so it just
	// copies the CRC record from that path ID into the one in pPathID and avoids a redundant CRC calculation.
	void			CacheFileCRC_Copy( const char *pPathID, const char *pRelativeFilename, const char *pPathIDToCopyFrom );

	// When we don't have a whitelist, it loads files without bothering to calculate their CRCs (we'd only
	// need them on a pure server), but when we get a whitelist, we'll want the CRCs, so it goes back, opens those
	// files, and calculates the CRCs for them.
	void	CalculateMissingCRCs( IFileList *pForceMatchList );

	int		GetUnverifiedCRCFiles( CUnverifiedCRCFile *pFiles, int nMaxFiles );

	// Note that we just opened this file and calculate a CRC for it.
	void	NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, FileHandle_t fp );
	void	NoteFileLoadedFromSteam( const char *pFilename, const char *pPathID, bool bForcedLoadFromSteam );
	void	NoteFileFailedToLoad( const char *pFilename, const char *pPathID );
	
	// Get a file info from a specific path ID.
	CFileInfo*	GetFileInfo( const char *pFilename, const char *pPathID );
	
	// Get all file infos with the specified filename (i.e. in all path IDs).
	int			GetFileInfos( CFileInfo **ppFileInfos, int nMaxFileInfos, const char *pFilename );

	// Clear everything.
	void	Clear();

private:
	void					CalculateMissingCRC( const char *pFilename, const char *pPathID );
	CPathIDFileList*		GetPathIDFileList( const char *pPathID, bool bAutoAdd=true );

	CRC32_t					CalculateCRCForFile( FileHandle_t fp );

private:
	CUtlLinkedList<CFileInfo*>		m_NeedsVerificationList;	// The list of files that need CRCs verified.
	CUtlDict<CPathIDFileList*,int>	m_PathIDs;
	CBaseFileSystem					*m_pFileSystem;
	CThreadMutex					m_Mutex;	// Threads call into here, so we need to be safe.
};


inline const char* CFileInfo::GetFilename()
{
	return m_pFilename;
}

inline const char* CFileInfo::GetPathIDString()
{
	return m_pPathIDFileList->m_PathID.String();
}


#endif // FILETRACKER_H
