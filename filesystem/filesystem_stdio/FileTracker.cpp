//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "basefilesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"



bool CompareFilenames( const char *pa, const char *pb )
{
	// Case-insensitive and path separator-insensitive compare.
	const char *a = pa;
	const char *b = pb;
	while ( *a && *b )
	{
		char ca = *a;
		char cb = *b;
		
		if ( ca >= 'a' && ca <= 'z' )
			ca = 'A' + (ca - 'a');
		else if ( ca == '/' )
			ca = '\\';
		
		if ( cb >= 'a' && cb <= 'z' )
			cb = 'A' + (cb - 'a');
		else if ( cb == '/' )
			cb = '\\';
		
		if ( ca != cb )
			return false;
		
		++a;
		++b;
	}
	
	// Filenames also must be the same length.
	if ( *a != *b )
		return false;

	return true;
}
		

//-----------------------------------------------------------------------------
// CFileTracker.
//-----------------------------------------------------------------------------

CFileTracker::CFileTracker( CBaseFileSystem *pFileSystem )
{
	m_pFileSystem = pFileSystem;
}

CFileTracker::~CFileTracker()
{
	Clear();
}

void CFileTracker::NoteFileLoadedFromDisk( const char *pFilename, const char *pPathID, FileHandle_t fp )
{
	AUTO_LOCK( m_Mutex );

	if ( !pPathID )
		pPathID = "";
	
	CPathIDFileList *pPath = GetPathIDFileList( pPathID );
	CFileInfo *pInfo = pPath->FindFileInfo( pFilename );

	if ( m_pFileSystem->m_WhitelistSpewFlags & WHITELIST_SPEW_WHILE_LOADING )
	{
		if ( pInfo )
			Warning( "(Duplicate): [%s]\\%s", pPathID, pFilename );
		else
			Warning( "(Unique   ): [%s]\\%s", pPathID, pFilename );
	}

	if ( pInfo )
	{
		// Clear all the flags, but remember if we ever had a CRC.
		pInfo->m_Flags &= k_eFileFlagsGotCRCOnce;
		pInfo->m_Flags &= ~k_eFileFlagsFailedToLoadLastTime;
	}
	else
	{
		pInfo = pPath->AddFileInfo( pFilename );
		pInfo->m_Flags = (EFileFlags)0;
	}
	
	if ( !fp )
	{
		if ( m_pFileSystem->m_WhitelistSpewFlags & WHITELIST_SPEW_WHILE_LOADING )
		{
			Warning( "\n" );
		}

		return;
	}

	
	// Remember that we calculated the CRC and that it is unverified.
	pInfo->m_CRC = CalculateCRCForFile( fp );
	pInfo->m_Flags |= k_eFileFlagsHasCRC | k_eFileFlagsGotCRCOnce;
	if ( pInfo->m_iNeedsVerificationListIndex == -1 )
		pInfo->m_iNeedsVerificationListIndex = m_NeedsVerificationList.AddToTail( pInfo );

	if ( m_pFileSystem->m_WhitelistSpewFlags & WHITELIST_SPEW_WHILE_LOADING )
	{
		Warning( " - %u\n", pInfo->m_CRC );
	}
}

void CFileTracker::NoteFileFailedToLoad( const char *pFilename, const char *pPathID )
{
	CPathIDFileList *pPath = GetPathIDFileList( pPathID );
	CFileInfo *pInfo = pPath->FindFileInfo( pFilename );
	if ( pInfo )
	{
		pInfo->m_Flags |= k_eFileFlagsFailedToLoadLastTime;
	}
}

CRC32_t CFileTracker::CalculateCRCForFile( FileHandle_t fp )
{	
	CRC32_t crc;
	
	// Calculate the CRC.
	unsigned int initialFilePos = m_pFileSystem->Tell( fp );
	m_pFileSystem->Seek( fp, 0, FILESYSTEM_SEEK_HEAD );

	#define	CRC_CHUNK_SIZE	(32*1024)
	char tempBuf[CRC_CHUNK_SIZE];

	CRC32_Init( &crc );
	
	unsigned int fileLength = m_pFileSystem->Size( fp );
		
	int nChunks = fileLength / CRC_CHUNK_SIZE + 1;
	unsigned int curStartByte = 0;
	for ( int iChunk=0; iChunk < nChunks; iChunk++ )
	{
		int curEndByte = min( curStartByte + CRC_CHUNK_SIZE, fileLength );
		int chunkLen = curEndByte - curStartByte;
		if ( chunkLen == 0 )
			break;
			
		m_pFileSystem->Read( tempBuf, chunkLen, fp ); // TODO: handle errors here..
		CRC32_ProcessBuffer( &crc, tempBuf, chunkLen );
		
		curStartByte += CRC_CHUNK_SIZE;
	}
	CRC32_Final( &crc );

	// Go back to where we started.
	m_pFileSystem->Seek( fp, initialFilePos, FILESYSTEM_SEEK_HEAD );
	return crc;
}

CFileInfo* CFileTracker::GetFileInfo( const char *pFilename, const char *pPathID )
{
	AUTO_LOCK( m_Mutex );
	
	CPathIDFileList *pPath = GetPathIDFileList( pPathID, false );
	if ( !pPath )
		return NULL;

	return pPath->FindFileInfo( pFilename );
}

int CFileTracker::GetFileInfos( CFileInfo **ppFileInfos, int nMaxFileInfos, const char *pFilename )
{
	AUTO_LOCK( m_Mutex );

	int nOut = 0;
	for ( int i=m_PathIDs.First(); i != m_PathIDs.InvalidIndex(); i=m_PathIDs.Next( i ) )
	{
		CFileInfo *pCur = m_PathIDs[i]->FindFileInfo( pFilename );
		if ( pCur )
		{
			if ( nOut < nMaxFileInfos )
			{
				ppFileInfos[nOut++] = pCur;
			}
			else
			{
				Assert( !"CFileTracker::GetFileInfos - overflowed list!" );
			}
		}
	}

	return nOut;
}

void CFileTracker::NoteFileLoadedFromSteam( const char *pFilename, const char *pPathID, bool bForcedLoadFromSteam )
{
	AUTO_LOCK( m_Mutex );

	if ( !pPathID )
		pPathID = "";
	
	CPathIDFileList *pPath = GetPathIDFileList( pPathID );
	CFileInfo *pInfo = pPath->FindFileInfo( pFilename );
	if ( !pInfo )
		pInfo = pPath->AddFileInfo( pFilename );

	if ( m_pFileSystem->m_WhitelistSpewFlags & WHITELIST_SPEW_WHILE_LOADING )
	{
		Warning( "From Steam: [%s]\\%s\n", pPathID, pFilename );
	}

	pInfo->m_Flags = k_eFileFlagsLoadedFromSteam;
	if ( bForcedLoadFromSteam )
		pInfo->m_Flags |= k_eFileFlagsForcedLoadFromSteam;
}


void CFileTracker::CalculateMissingCRCs( IFileList *pWantCRCList )
{
	// First build a list of files that need a CRC and don't have one.
	m_Mutex.Lock();
	CUtlLinkedList<CFileInfo*,int> needCRCList;

	for ( int i=m_PathIDs.First(); i != m_PathIDs.InvalidIndex(); i=m_PathIDs.Next( i ) )
	{
		CPathIDFileList *pPath = m_PathIDs[i];
		FOR_EACH_LL( pPath->m_Files, j )
		{
			CFileInfo *pInfo = pPath->m_Files[j];
			
			if ( !( pInfo->m_Flags & k_eFileFlagsLoadedFromSteam ) && !( pInfo->m_Flags & k_eFileFlagsHasCRC ) )
			{
				// If the new "force match" list doesn't care whether the file has a CRC or not, then don't bother to calculate it.
				if ( !pWantCRCList->IsFileInList( pInfo->m_pFilename ) )
					continue;
				
				needCRCList.AddToTail( pInfo );
			}
		}
	}

	m_Mutex.Unlock();
	
	// Then, when the mutex is not locked, go generate the CRCs for them.
	FOR_EACH_LL( needCRCList, i )
	{
		CFileInfo *pInfo = needCRCList[i];
		CalculateMissingCRC( pInfo->m_pFilename, pInfo->GetPathIDString() );
	}
}


void CFileTracker::CacheFileCRC( const char *pPathID, const char *pRelativeFilename )
{
	Assert( ThreadInMainThread() );
	
	// Get the file's info. Load the file if necessary.
	CFileInfo *pInfo = GetFileInfo( pRelativeFilename, pPathID );
	if ( !pInfo )
	{
		CalculateMissingCRC( pRelativeFilename, pPathID );
		pInfo = GetFileInfo( pRelativeFilename, pPathID );
	}
	
	if ( !pInfo )
		return;
	
	// Already cached a CRC for this file?
	if ( !( pInfo->m_Flags & k_eFileFlagsGotCRCOnce ) )
	{
		// Ok, it's from disk but we don't have the CRC.
		CalculateMissingCRC( pInfo->GetFilename(), pInfo->GetPathIDString() );
	}
}


void CFileTracker::CacheFileCRC_Copy( const char *pPathID, const char *pRelativeFilename, const char *pPathIDToCopyFrom )
{
	Assert( ThreadInMainThread() );
	
	// Get the file's info. Load the file if necessary.
	CFileInfo *pSourceInfo = GetFileInfo( pRelativeFilename, pPathIDToCopyFrom );
	if ( !pSourceInfo || !( pSourceInfo->m_Flags & k_eFileFlagsGotCRCOnce ) )
	{
		// Strange, we don't have a CRC for the one they wanted to copy from, so calculate that CRC.
		CacheFileCRC( pPathIDToCopyFrom, pRelativeFilename );
		if ( !( pSourceInfo->m_Flags & k_eFileFlagsGotCRCOnce ) )
		{
			// Still didn't get it. Ok.. well get a CRC for the target one anyway.
			CacheFileCRC( pPathID, pRelativeFilename );
			return;
		}
	}

	// Setup a CFileInfo for the target..	
	CPathIDFileList *pPath = GetPathIDFileList( pPathID );
	CFileInfo *pDestInfo = pPath->FindFileInfo( pRelativeFilename );
	if ( !pDestInfo )
		pDestInfo = pPath->AddFileInfo( pRelativeFilename );

	pDestInfo->m_CRC = pSourceInfo->m_CRC;
	pDestInfo->m_Flags = pSourceInfo->m_Flags;
}


EFileCRCStatus CFileTracker::CheckCachedFileCRC( const char *pPathID, const char *pRelativeFilename, CRC32_t *pCRC )
{
	Assert( ThreadInMainThread() );
	
	// Get the file's info. Load the file if necessary.
	CFileInfo *pInfo = GetFileInfo( pRelativeFilename, pPathID );
	if ( pInfo && (pInfo->m_Flags & k_eFileFlagsGotCRCOnce) )
	{
		*pCRC = pInfo->m_CRC;
		return k_eFileCRCStatus_GotCRC;
	}
	else
	{
		return k_eFileCRCStatus_CantOpenFile;
	}
}



void CFileTracker::CalculateMissingCRC( const char *pFilename, const char *pPathID )
{
	// Force it to make a CRC of disk files.
	FileHandle_t fh = m_pFileSystem->FindFileInSearchPaths( pFilename, "rb", pPathID, FSOPEN_FORCE_TRACK_CRC, NULL, true );
	if ( !fh )
		return;

	CFileInfo *pInfo = GetFileInfo( pFilename, pPathID );
	if ( pInfo )
	{		
		// Now we're about to modify the file itself.. lock the mutex.
		AUTO_LOCK( m_Mutex );

		// The FindFileInSearchPaths call might have done the CRC for us.
		if ( !( pInfo->m_Flags & k_eFileFlagsHasCRC ) )
		{
			pInfo->m_CRC = CalculateCRCForFile( fh );
			pInfo->m_Flags |= k_eFileFlagsHasCRC | k_eFileFlagsGotCRCOnce;
			if ( pInfo->m_iNeedsVerificationListIndex == -1 )
			{
				pInfo->m_iNeedsVerificationListIndex = m_NeedsVerificationList.AddToTail( pInfo );
			}
		}
	}
	else
	{
		Assert( false );
	}		

	m_pFileSystem->Close( fh );
}


void CFileTracker::MarkAllCRCsUnverified()
{
	AUTO_LOCK( m_Mutex );

	// First clear the 'needs verification' list.
	MarkAllCRCsVerified();

	Assert( m_NeedsVerificationList.Count() == 0 );
	for ( int i=m_PathIDs.First(); i != m_PathIDs.InvalidIndex(); i=m_PathIDs.Next( i ) )
	{
		CPathIDFileList *pPath = m_PathIDs[i];
		FOR_EACH_LL( pPath->m_Files, j )
		{
			CFileInfo *pInfo = pPath->m_Files[j];
			
			if ( !(pInfo->m_Flags & k_eFileFlagsLoadedFromSteam) && ( pInfo->m_Flags & k_eFileFlagsHasCRC ) )
			{
				pInfo->m_iNeedsVerificationListIndex = m_NeedsVerificationList.AddToTail( pInfo );
			}
		}
	}
}


void CFileTracker::MarkAllCRCsVerified( bool bLockMutex )
{
	if ( bLockMutex )
		m_Mutex.Lock();

	FOR_EACH_LL( m_NeedsVerificationList, i )
	{
		m_NeedsVerificationList[i]->m_iNeedsVerificationListIndex = -1;
	}
	
	m_NeedsVerificationList.Purge();

	if ( bLockMutex )
		m_Mutex.Unlock();
}


int CFileTracker::GetUnverifiedCRCFiles( CUnverifiedCRCFile *pFiles, int nMaxFiles )
{
	Assert( nMaxFiles > 0 );

	AUTO_LOCK( m_Mutex );

	int iOutFile = 0;
	int iNext = 0;
	for ( int i=m_NeedsVerificationList.Head(); i != m_NeedsVerificationList.InvalidIndex(); i=iNext )
	{
		iNext = m_NeedsVerificationList.Next( i );

		CFileInfo *pInfo = m_NeedsVerificationList[i];

		// Remove this entry from the list.
		m_NeedsVerificationList.Remove( i );
		pInfo->m_iNeedsVerificationListIndex = -1;

		// This can happen if a file that was in this list was loaded from Steam since it got added to the list.
		// In that case, just act like it's not in the list.
		if ( pInfo->m_Flags & k_eFileFlagsLoadedFromSteam )
			continue;

		Assert( pInfo->m_Flags & k_eFileFlagsHasCRC );

		// Add this file to their list.
		CUnverifiedCRCFile *pOutFile = &pFiles[iOutFile];
		
		V_strncpy( pOutFile->m_Filename, pInfo->m_pFilename, sizeof( pOutFile->m_Filename ) );
		V_strncpy( pOutFile->m_PathID, pInfo->m_pPathIDFileList->m_PathID.String(), sizeof( pOutFile->m_PathID ) );
		pOutFile->m_CRC = pInfo->m_CRC;
	

		++iOutFile;
		if ( iOutFile >= nMaxFiles )
			return iOutFile;
	}
	
	return iOutFile;
}


void CFileTracker::Clear()
{
	AUTO_LOCK( m_Mutex );

	m_PathIDs.PurgeAndDeleteElements();
}

CPathIDFileList* CFileTracker::GetPathIDFileList( const char *pPathID, bool bAutoAdd )
{
	AUTO_LOCK( m_Mutex );

	if ( !pPathID )
		pPathID = "";
		
	int i = m_PathIDs.Find( pPathID );
	if ( i == m_PathIDs.InvalidIndex() )
	{
		if ( bAutoAdd )
		{
			CPathIDFileList *pPath = new CPathIDFileList;
			pPath->m_PathID = pPathID;
			m_PathIDs.Insert( pPathID, pPath );
			return pPath;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		return m_PathIDs[i];
	}
}

//-----------------------------------------------------------------------------
// CFileInfo implementation.
//-----------------------------------------------------------------------------

CFileInfo::CFileInfo( const char *pFilename )
{
	int len = V_strlen( pFilename ) + 1;
	m_pFilename = new char[ len ];
	Q_strncpy( m_pFilename, pFilename, len );
	m_iNeedsVerificationListIndex = -1;
}

CFileInfo::~CFileInfo()
{
	delete [] m_pFilename;
}


//-----------------------------------------------------------------------------
// CPathIDFileList implementation..
//-----------------------------------------------------------------------------

CPathIDFileList::~CPathIDFileList()
{
	m_Files.PurgeAndDeleteElements();
}

CFileInfo* CPathIDFileList::FindFileInfo( const char *pFilename )
{
	Assert( !V_IsAbsolutePath( pFilename ) );
	
	FOR_EACH_LL( m_Files, i )
	{
		CFileInfo *pFileInfo = m_Files[i];
		
		if ( CompareFilenames( pFilename, pFileInfo->GetFilename() ) )
			return m_Files[i];
	}
	
	return NULL;
}

CFileInfo* CPathIDFileList::AddFileInfo( const char *pFilename )
{
	Assert( !V_IsAbsolutePath( pFilename ) );
	
	CFileInfo *pFileInfo = new CFileInfo( pFilename );
	pFileInfo->m_pPathIDFileList = this;
	m_Files.AddToTail( pFileInfo );
	return pFileInfo;
}




