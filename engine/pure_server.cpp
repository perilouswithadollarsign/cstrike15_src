//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#include "utlstring.h"
#include "checksum_crc.h"
#include "userid.h"
#include "pure_server.h"
#include "common.h"
#include "tier1/keyvalues.h"
#include "convar.h"
#include "filesystem_engine.h"
#include "server.h"
#include "sv_filter.h"
#include <utlsortvector.h>

extern ConVar sv_pure_consensus;
extern ConVar sv_pure_retiretime;
extern ConVar sv_pure_trace;

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static char *g_SvPure2_ProtectedDirs[] =
{
	"sound",
	"models",
	"materials"
};

static bool IsProtectedBySvPure2( const char *pFilename )
{
	for ( int i=0; i < ARRAYSIZE( g_SvPure2_ProtectedDirs ); i++ )
	{
		const char *pProtectedDir = g_SvPure2_ProtectedDirs[i];
		int len = V_strlen( pProtectedDir );
		
		if ( V_strlen( pFilename ) < len+1 )
			return false;
		
		char tempStr[512];
		Assert( len < ARRAYSIZE( tempStr ) );
		memcpy( tempStr, pFilename, len );
		tempStr[len] = 0;
		
		if ( V_stricmp( tempStr, pProtectedDir ) == 0 )
		{
			if ( pFilename[len] == '/' || pFilename[len] == '\\' )
				return true;
		}		
	}
	return false;
}



CPureServerWhitelist::CCommand::CCommand()
{
}

CPureServerWhitelist::CCommand::~CCommand()
{
}


CPureServerWhitelist* CPureServerWhitelist::Create( IFileSystem *pFileSystem )
{
	CPureServerWhitelist *pRet = new CPureServerWhitelist;
	pRet->Init( pFileSystem );
	return pRet;
}


CPureServerWhitelist::CPureServerWhitelist()
{
	m_pFileSystem = NULL;
	m_LoadCounter = 0;
	m_AllowFromDiskList.m_pWhitelist = this;
	m_ForceMatchList.m_pWhitelist = this;
	m_RefCount = 1;
	m_bFullyPureMode = false;
}


CPureServerWhitelist::~CPureServerWhitelist()
{
	Term();
}


void CPureServerWhitelist::Init( IFileSystem *pFileSystem )
{
	Term();
	m_pFileSystem = pFileSystem;
}


void CPureServerWhitelist::Term()
{
	m_FileCommands.PurgeAndDeleteElements();
	m_RecursiveDirCommands.PurgeAndDeleteElements();
	m_NonRecursiveDirCommands.PurgeAndDeleteElements();
	m_pFileSystem = NULL;
	m_LoadCounter = 0;
}


bool CPureServerWhitelist::LoadFromKeyValues( KeyValues *kv )
{
	for ( KeyValues *pCurItem = kv->GetFirstValue(); pCurItem; pCurItem = pCurItem->GetNextValue() )
	{
		char szPathName[ MAX_PATH ];
		const char *pKeyValue = pCurItem->GetName();
		const char *pModifiers = pCurItem->GetString();
		if ( !pKeyValue || !pModifiers )
			continue;
	
		Q_strncpy( szPathName, pKeyValue, sizeof(szPathName) );
		Q_FixSlashes( szPathName );
		const char *pValue = szPathName;

		// Figure out the modifiers.
		bool bFromSteam = false, bAllowFromDisk = false, bCheckCRC = false;
		CSplitString mods( pModifiers, "+" );
		for ( int i=0; i < mods.Count(); i++ )
		{
			if ( V_stricmp( mods[i], "from_steam" ) == 0 )
				bFromSteam = true;
			else if ( V_stricmp( mods[i], "allow_from_disk" ) == 0 )
				bAllowFromDisk = true;
			else if ( V_stricmp( mods[i], "check_crc" ) == 0 )
				bCheckCRC = true;
			else
				Warning( "Unknown modifier in whitelist file: %s.\n", mods[i] );
		}
		// we don't have to purge really; and if we do, we CAN'T delete elements 
		if ( bFromSteam && (bAllowFromDisk || bCheckCRC) )
		{
			bAllowFromDisk = bCheckCRC = false;
			Warning( "Whitelist: from_steam not compatible with other modifiers (used on %s).\n", pValue );
			Warning( "           Other markers removed.\n" );
		}

		
		// Setup a new CCommand to hold this command.
		CPureServerWhitelist::CCommand *pCommand = new CPureServerWhitelist::CCommand;
		pCommand->m_LoadOrder = m_LoadCounter++;
		pCommand->m_bAllowFromDisk = bAllowFromDisk;
		pCommand->m_bCheckCRC = bCheckCRC;

		// Figure out if they're referencing a file, a recursive directory, or a nonrecursive directory.		
		CUtlDict<CCommand*,int> *pList;
		const char *pEndPart = V_UnqualifiedFileName( pValue );
		if ( Q_stricmp( pEndPart, "..." ) == 0 )
			pList = &m_RecursiveDirCommands;
		else if ( Q_stricmp( pEndPart, "*.*" ) == 0 )
			pList = &m_NonRecursiveDirCommands;
		else
			pList = &m_FileCommands;
		
		// If it's a directory command, get rid of the *.* or ...
		char filePath[MAX_PATH];
		if ( pList == &m_RecursiveDirCommands || pList == &m_NonRecursiveDirCommands )
			V_ExtractFilePath( pValue, filePath, sizeof( filePath ) );
		else
			V_strncpy( filePath, pValue, sizeof( filePath ) );
		
		V_FixSlashes( filePath );

		// Add the command to the appropriate list.
		if ( pList->Find( filePath ) == pList->InvalidIndex() )
		{
			pList->Insert( filePath, pCommand );
		}
		else
		{
			Error( "Pure server whitelist entry '%s' is a duplicate.\n", filePath );
		}		
	}	
	
	return true;
}


void CPureServerWhitelist::EnableFullyPureMode()
{
	// In this mode, all files must come from Steam.
	m_FileCommands.PurgeAndDeleteElements();
	m_RecursiveDirCommands.PurgeAndDeleteElements();
	m_NonRecursiveDirCommands.PurgeAndDeleteElements();
	m_bFullyPureMode = true;
}


bool CPureServerWhitelist::IsInFullyPureMode() const
{
	return m_bFullyPureMode;
}


void CPureServerWhitelist::UpdateCommandStats( CUtlDict<CPureServerWhitelist::CCommand*,int> &commands, int *pHighest, int *pLongestPathName )
{
	for ( int i=commands.First(); i != commands.InvalidIndex(); i=commands.Next( i ) )
	{
		*pHighest = MAX( *pHighest, commands[i]->m_LoadOrder );
		
		int len = V_strlen( commands.GetElementName( i ) );
		*pLongestPathName = MAX( *pLongestPathName, len );
	}
}

void CPureServerWhitelist::PrintCommand( const char *pFileSpec, const char *pExt, int maxPathnameLen, CPureServerWhitelist::CCommand *pCommand )
{
	// Get rid of the trailing slash if there is one.
	char tempFileSpec[MAX_PATH];
	V_strncpy( tempFileSpec, pFileSpec, sizeof( tempFileSpec ) );
	int len = V_strlen( tempFileSpec );
	if ( len > 0 && (tempFileSpec[len-1] == '/' || tempFileSpec[len-1] == '\\') )
		tempFileSpec[len-1] = 0;
		
	if ( pExt )
		Msg( "%s%c%s", tempFileSpec, CORRECT_PATH_SEPARATOR, pExt );
	else
		Msg( "%s", tempFileSpec );

	len = V_strlen( pFileSpec );
	for ( int i=len; i < maxPathnameLen+6; i++ )
	{
		Msg( " " );
	}
	
	Msg( "\t" );
	if ( pCommand->m_bCheckCRC )
		Msg( "check_crc" );
	else
		Msg( "allow_from_disk" );
	
	Msg( "\n" );
}


int CPureServerWhitelist::FindCommandByLoadOrder( CUtlDict<CPureServerWhitelist::CCommand*,int> &commands, int iLoadOrder )
{
	for ( int i=commands.First(); i != commands.InvalidIndex(); i=commands.Next( i ) )
	{
		if ( commands[i]->m_LoadOrder == iLoadOrder )
			return i;
	}
	return -1;
}


void CPureServerWhitelist::PrintWhitelistContents()
{
	int highestLoadOrder = 0, longestPathName = 0;
	UpdateCommandStats( m_FileCommands, &highestLoadOrder, &longestPathName );
	UpdateCommandStats( m_RecursiveDirCommands, &highestLoadOrder, &longestPathName );
	UpdateCommandStats( m_NonRecursiveDirCommands, &highestLoadOrder, &longestPathName );
	
	for ( int iLoadOrder=0; iLoadOrder <= highestLoadOrder; iLoadOrder++ )
	{
		// Check regular file commands.
		int iCommand = FindCommandByLoadOrder( m_FileCommands, iLoadOrder );
		if ( iCommand != -1 )
		{
			PrintCommand( m_FileCommands.GetElementName( iCommand ), NULL, longestPathName, m_FileCommands[iCommand] );
		}
		else
		{
			// Check recursive commands.
			iCommand = FindCommandByLoadOrder( m_RecursiveDirCommands, iLoadOrder );
			if ( iCommand != -1 )
			{
				PrintCommand( m_RecursiveDirCommands.GetElementName( iCommand ), "...", longestPathName, m_RecursiveDirCommands[iCommand] );
			}
			else
			{
				// Check *.* commands.
				iCommand = FindCommandByLoadOrder( m_NonRecursiveDirCommands, iLoadOrder );
				if ( iCommand != -1 )
				{
					PrintCommand( m_NonRecursiveDirCommands.GetElementName( iCommand ), "*.*", longestPathName, m_NonRecursiveDirCommands[iCommand] );
				}
			}
		}
	}
}


void CPureServerWhitelist::Encode( CUtlBuffer &buf )
{
	EncodeCommandList( m_FileCommands, buf );
	EncodeCommandList( m_RecursiveDirCommands, buf );
	EncodeCommandList( m_NonRecursiveDirCommands, buf );
	buf.PutChar( (char)m_bFullyPureMode );
}


void CPureServerWhitelist::EncodeCommandList( CUtlDict<CPureServerWhitelist::CCommand*,int> &theList, CUtlBuffer &buf )
{
	buf.PutInt( theList.Count() );
	for ( int i=theList.First(); i != theList.InvalidIndex(); i = theList.Next( i ) )
	{
		CPureServerWhitelist::CCommand *pCommand = theList[i];
		
		unsigned char val = 0;
		if ( pCommand->m_bAllowFromDisk )
			val |= 0x01;
		if ( pCommand->m_bCheckCRC )
			val |= 0x02;
		
		buf.PutUnsignedChar( val );
		buf.PutUnsignedShort( pCommand->m_LoadOrder );
		buf.PutString( theList.GetElementName( i ) );
	}
}


void CPureServerWhitelist::Decode( CUtlBuffer &buf )
{
	DecodeCommandList( m_FileCommands, buf );
	DecodeCommandList( m_RecursiveDirCommands, buf );
	DecodeCommandList( m_NonRecursiveDirCommands, buf );

	if ( buf.GetBytesRemaining() >= 1 )
	{
		m_bFullyPureMode = (buf.GetChar() != 0);
	}
	else
	{
		m_bFullyPureMode = false;
	}
}


void CPureServerWhitelist::CacheFileCRCs()
{
	InternalCacheFileCRCs( m_FileCommands, k_eCacheCRCType_SingleFile );
	InternalCacheFileCRCs( m_NonRecursiveDirCommands, k_eCacheCRCType_Directory );
	InternalCacheFileCRCs( m_RecursiveDirCommands, k_eCacheCRCType_Directory_Recursive );
}


void CPureServerWhitelist::InternalCacheFileCRCs( CUtlDict<CCommand*,int> &theList, ECacheCRCType eType )
{
	for ( int i=theList.First(); i != theList.InvalidIndex(); i = theList.Next( i ) )
	{
		CCommand *pCommand = theList[i];
		if ( pCommand->m_bCheckCRC )
		{
			const char *pPathname = theList.GetElementName( i );
			m_pFileSystem->CacheFileCRCs( pPathname, eType, &m_ForceMatchList );
		}
	}
}


void CPureServerWhitelist::DecodeCommandList( CUtlDict<CPureServerWhitelist::CCommand*,int> &theList, CUtlBuffer &buf )
{
	int nCommands = buf.GetInt();
	
	for ( int i=0; i < nCommands; i++ )
	{
		CPureServerWhitelist::CCommand *pCommand = new CPureServerWhitelist::CCommand;
		
		unsigned char val = buf.GetUnsignedChar();
		pCommand->m_bAllowFromDisk = (( val & 0x01 ) != 0);
		pCommand->m_bCheckCRC      = (( val & 0x02 ) != 0);

		pCommand->m_LoadOrder = buf.GetUnsignedShort();

		char str[MAX_PATH];
		buf.GetString( str, sizeof( str )-1 );
		V_FixSlashes( str );
		
		theList.Insert( str, pCommand );
	}
}


CPureServerWhitelist::CCommand* CPureServerWhitelist::GetBestEntry( const char *pFilename )
{
	// NOTE: Since this is a user-specified file, we don't have the added complexity of path IDs in here.
	// So when the filesystem asks if a file is in the whitelist, we just ignore the path ID.
	
	// Make sure we have a relative pathname with fixed slashes..
	char relativeFilename[MAX_PATH];
	V_strncpy( relativeFilename, pFilename, sizeof( relativeFilename ) );

	// Convert the path to relative if necessary.
	if ( !V_IsAbsolutePath( relativeFilename ) || m_pFileSystem->FullPathToRelativePath( pFilename, relativeFilename, sizeof( relativeFilename ) ) )
	{
		V_FixSlashes( relativeFilename );
		
		// Get the directory this thing is in.
		char relativeDir[MAX_PATH];
		if ( !V_ExtractFilePath( relativeFilename, relativeDir, sizeof( relativeDir ) )	)
			relativeDir[0] = 0;
		
		
		// Check each of our dictionaries to see if there is an entry for this thing.
		CCommand *pBestEntry = NULL;
		
		pBestEntry = CheckEntry( m_FileCommands, relativeFilename, pBestEntry );
		if ( relativeDir[0] != 0 )
		{
			pBestEntry = CheckEntry( m_NonRecursiveDirCommands, relativeDir, pBestEntry );

			while ( relativeDir[0] != 0 )
			{
				// Check for this directory.
				pBestEntry = CheckEntry( m_RecursiveDirCommands, relativeDir, pBestEntry );
				if ( !V_StripLastDir( relativeDir, sizeof( relativeDir ) ) )
					break;
			}
		}
			
		return pBestEntry;
	}
	
	// Either we couldn't find an entry, or they specified an absolute path that we could not convert to a relative path.
	return NULL;
}


CPureServerWhitelist::CCommand* CPureServerWhitelist::CheckEntry( 
	CUtlDict<CPureServerWhitelist::CCommand*,int> &dict, 
	const char *pEntryName, 
	CPureServerWhitelist::CCommand *pBestEntry )
{
	int i = dict.Find( pEntryName );
	if ( i != dict.InvalidIndex() && (!pBestEntry || dict[i]->m_LoadOrder > pBestEntry->m_LoadOrder) )
		pBestEntry = dict[i];
	
	return pBestEntry;
}


void CPureServerWhitelist::Release()
{
	if ( --m_RefCount <= 0 )
		delete this;
}


IFileList* CPureServerWhitelist::GetAllowFromDiskList()
{
	++m_RefCount;
	return &m_AllowFromDiskList;
}


IFileList* CPureServerWhitelist::GetForceMatchList()
{
	++m_RefCount;
	return &m_ForceMatchList;
}


// --------------------------------------------------------------------------------------------------- //
// CAllowFromDiskList/CForceMatchList implementation.
// --------------------------------------------------------------------------------------------------- //

bool CPureServerWhitelist::CAllowFromDiskList::IsFileInList( const char *pFilename )
{
	// In "fully pure" mode, all files must come from disk.
	if ( m_pWhitelist->m_bFullyPureMode )
	{
		// Only protect maps, models, and sounds.
		if ( IsProtectedBySvPure2( pFilename ) )
			return false;
		else
			return true;
	}
		
	CCommand *pCommand = m_pWhitelist->GetBestEntry( pFilename );
	if ( pCommand )
		return pCommand->m_bAllowFromDisk;
	else
		return true;	// All files are allowed to come from disk by default.
}

void CPureServerWhitelist::CAllowFromDiskList::Release()
{
	m_pWhitelist->Release();
}

bool CPureServerWhitelist::CForceMatchList::IsFileInList( const char *pFilename )
{
	// In "fully pure" mode, all files must match the server files
	if ( m_pWhitelist->m_bFullyPureMode )
		return true;

	CCommand *pCommand = m_pWhitelist->GetBestEntry( pFilename );
	if ( pCommand )
		return pCommand->m_bCheckCRC;
	else
		return false;	// By default, no files require the CRC check.
}

void CPureServerWhitelist::CForceMatchList::Release()
{
	m_pWhitelist->Release();
}


void CPureFileTracker::AddUserReportedFileHash( int idxFile, FileHash_t *pFileHash, USERID_t userID, bool bAddMasterRecord )
{
	UserReportedFileHash_t userFileHash;
	userFileHash.m_idxFile = idxFile;
	userFileHash.m_userID = userID;
	userFileHash.m_FileHash = *pFileHash;
	int idxUserReported = m_treeUserReportedFileHash.Find( userFileHash );
	if ( idxUserReported == m_treeUserReportedFileHash.InvalidIndex() )
	{
		idxUserReported = m_treeUserReportedFileHash.Insert( userFileHash );
		if ( bAddMasterRecord )
		{
			// count the number of matches for this idxFile
			// if it exceeds > 5 then make a master record
			int idxFirst = idxUserReported;
			int idxLast = idxUserReported;
			int ctMatches = 1;
			int ctTotalFiles = 1;
			// first go forward
			int idx = m_treeUserReportedFileHash.NextInorder( idxUserReported );
			while ( idx != m_treeUserReportedFileHash.InvalidIndex() && m_treeUserReportedFileHash[idx].m_idxFile == m_treeUserReportedFileHash[idxUserReported].m_idxFile )
			{
				if ( m_treeUserReportedFileHash[idx].m_FileHash == m_treeUserReportedFileHash[idxUserReported].m_FileHash )
					ctMatches++;
				ctTotalFiles++;
				idxLast = idx;
				idx = m_treeUserReportedFileHash.NextInorder( idx );
			}
			// then backwards
			idx = m_treeUserReportedFileHash.PrevInorder( idxUserReported );
			while ( idx != m_treeUserReportedFileHash.InvalidIndex() && m_treeUserReportedFileHash[idx].m_idxFile == m_treeUserReportedFileHash[idxUserReported].m_idxFile )
			{
				if ( m_treeUserReportedFileHash[idx].m_FileHash == m_treeUserReportedFileHash[idxUserReported].m_FileHash )
					ctMatches++;
				ctTotalFiles++;
				idxFirst = idx;
				idx = m_treeUserReportedFileHash.PrevInorder( idx );
			}
			// if ctTotalFiles >> ctMatches then that means clients are reading different bits from the file.
			// in order to get this right we need to ask them to read the entire thing
			if ( ctMatches >= sv_pure_consensus.GetInt() )
			{
				MasterFileHash_t masterFileHashNew;
				masterFileHashNew.m_idxFile = m_treeUserReportedFileHash[idxUserReported].m_idxFile;
				masterFileHashNew.m_cMatches = ctMatches;
				masterFileHashNew.m_FileHash = m_treeUserReportedFileHash[idxUserReported].m_FileHash;
				m_treeMasterFileHashes.Insert( masterFileHashNew );
				// remove all the individual records that matched the new master, we don't need them anymore
				int idxRemove = idxFirst;
				while ( idxRemove != m_treeUserReportedFileHash.InvalidIndex() )
				{
					int idxNext = m_treeUserReportedFileHash.NextInorder( idxRemove );
					if ( m_treeUserReportedFileHash[idxRemove].m_FileHash == m_treeUserReportedFileHash[idxUserReported].m_FileHash )
						m_treeUserReportedFileHash.RemoveAt( idxRemove );
					if ( idxRemove == idxLast )
						break;
					idxRemove = idxNext;
				}
			}
		}
	}
	else
	{
		m_treeUserReportedFileHash[idxUserReported].m_FileHash = *pFileHash;
	}
	// we dont have enough data to decide if you match or not yet - so we call it a match
}


void FileRenderHelper( USERID_t userID, const char *pchMessage, const char *pchPath, const char *pchFileName, FileHash_t *pFileHash, int nFileFraction, FileHash_t *pFileHashLocal )
{
	char rgch[256];
	char hex[ 34 ];
	Q_memset( hex, 0, sizeof( hex ) );
	Q_binarytohex( (const byte *)&pFileHash->m_md5contents.bits, sizeof( pFileHash->m_md5contents.bits ), hex, sizeof( hex ) );

	char hex2[ 34 ];
	Q_memset( hex2, 0, sizeof( hex2 ) );
	if ( pFileHashLocal )
		Q_binarytohex( (const byte *)&pFileHashLocal->m_md5contents.bits, sizeof( pFileHashLocal->m_md5contents.bits ), hex2, sizeof( hex2 ) );

	if ( pFileHash->m_PackFileID )
	{
		Q_snprintf( rgch, 256, "Pure server: file: %s\\%s ( %d %d %8.8x %6.6x ) %s : %s : %s\n", 
			pchPath, pchFileName,
			pFileHash->m_PackFileID, pFileHash->m_nPackFileNumber, nFileFraction, pFileHash->m_cbFileLen,
			pchMessage, 
			hex, hex2 );
	}
	else
	{
		Q_snprintf( rgch, 256, "Pure server: file: %s\\%s ( %d %d %lx ) %s : %s : %s\n", 
			pchPath, pchFileName,
			pFileHash->m_eFileHashType, pFileHash->m_cbFileLen, pFileHash->m_eFileHashType ? pFileHash->m_crcIOSequence : 0,
			pchMessage, 
			hex, hex2 );
	}
	if ( userID.idtype != 0 )
		Msg( "[%s] %s\n", GetUserIDString(userID), rgch );
	else
		Msg( "%s", rgch );

}


bool CPureFileTracker::DoesFileMatch( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash, USERID_t userID )
{
	// if the server has been idle for more than 15 minutes, discard all this data
	const float flRetireTime = sv_pure_retiretime.GetFloat();
	float flCurTime = Plat_FloatTime();
	if ( ( flCurTime - m_flLastFileReceivedTime ) > flRetireTime )
	{
		m_treeMasterFileHashes.RemoveAll();
		m_treeUserReportedFileHash.RemoveAll();
		m_treeMasterFileHashes.RemoveAll();
	}
	m_flLastFileReceivedTime = flCurTime;

	// The clients must send us all files. We decide if it is whitelisted or not
	// That way the clients can not hide modified files in a whitelisted directory
	if ( pFileHash->m_PackFileID == 0 && 
		!sv.GetPureServerWhitelist()->GetForceMatchList()->IsFileInList( pRelativeFilename ) )
	{

		if ( sv_pure_trace.GetInt() == 4 )
		{
			char warningStr[1024] = {0};
			V_snprintf( warningStr, sizeof( warningStr ), "Pure server: file [%s]\\%s ignored by whitelist.", pPathID, pRelativeFilename );
			Msg( "[%s] %s\n", GetUserIDString(userID), warningStr );
		}

		return true;
	}

	char rgchFilenameFixed[MAX_PATH];
	Q_strncpy( rgchFilenameFixed, pRelativeFilename, sizeof( rgchFilenameFixed ) );
	Q_FixSlashes( rgchFilenameFixed );

	// first look up the file and see if we have ever seen it before
	CRC32_t crcFilename;
	CRC32_Init( &crcFilename );
	CRC32_ProcessBuffer( &crcFilename, rgchFilenameFixed, Q_strlen( rgchFilenameFixed ) );
	CRC32_ProcessBuffer( &crcFilename, pPathID, Q_strlen( pPathID ) );
	CRC32_Final( &crcFilename );
	UserReportedFile_t ufile;
	ufile.m_crcIdentifier = crcFilename;
	ufile.m_filename = rgchFilenameFixed;
	ufile.m_path = pPathID;
	ufile.m_nFileFraction = nFileFraction;
	int idxFile = m_treeAllReportedFiles.Find( ufile );
	if ( idxFile == m_treeAllReportedFiles.InvalidIndex() )
	{
		idxFile = m_treeAllReportedFiles.Insert( ufile );
	}
	else
	{
		m_cMatchedFile++;
	}
	// then check if we have a master CRC for the file
	MasterFileHash_t masterFileHash;
	masterFileHash.m_idxFile = idxFile;
	int idxMaster = m_treeMasterFileHashes.Find( masterFileHash );
	// dont do anything with this yet

	// check to see if we have loaded the file locally and can match it
	FileHash_t filehashLocal;
	EFileCRCStatus eStatus = g_pFileSystem->CheckCachedFileHash( pPathID, rgchFilenameFixed, nFileFraction, &filehashLocal );
	if ( eStatus == k_eFileCRCStatus_FileInVPK)
	{
		// you managed to load a file outside a VPK that the server has in the VPK
		// this is possible if the user explodes the VPKs into individual files and then deletes the VPKs
		FileRenderHelper( userID, "file should be in VPK", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, NULL );
		return false;
	}
	// if the user sent us a full file hash, but we dont have one, hash it now
	if ( pFileHash->m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile && 
		( eStatus != k_eFileCRCStatus_GotCRC || filehashLocal.m_eFileHashType != FileHash_t::k_EFileHashTypeEntireFile ) )
	{
		// lets actually read the file so we get a complete file hash
		FileHandle_t f = g_pFileSystem->Open( rgchFilenameFixed, "rb", pPathID);
		// try to load the file and really compute the hash - should only have to do this once ever
		if ( f )
		{
			// load file into a null-terminated buffer
			int fileSize = g_pFileSystem->Size( f );
			unsigned bufSize = g_pFileSystem->GetOptimalReadSize( f, fileSize );

			char *buffer = (char*)g_pFileSystem->AllocOptimalReadBuffer( f, bufSize );
			Assert( buffer );

			// read into local buffer
			bool bRetOK = ( g_pFileSystem->ReadEx( buffer, bufSize, fileSize, f ) != 0 );
			bRetOK;
			g_pFileSystem->FreeOptimalReadBuffer( buffer );

			g_pFileSystem->Close( f );	// close file after reading

			eStatus = g_pFileSystem->CheckCachedFileHash( pPathID, rgchFilenameFixed, nFileFraction, &filehashLocal );
		}
		else
		{
			// what should we do if we couldn't open the file? should probably kick
			FileRenderHelper( userID, "could not open file to hash ( benign for now )", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, NULL );
		}
	}
	if ( eStatus == k_eFileCRCStatus_GotCRC )
	{
		if ( filehashLocal.m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile &&
			pFileHash->m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile )
		{
			if ( filehashLocal == *pFileHash )
			{
				m_cMatchedFileFullHash++;
				return true;
			}
			else
			{
				// don't need to check anything else
				// did not match - record so that we have a record of the file that did not match ( just for reporting )
				AddUserReportedFileHash( idxFile, pFileHash, userID, false );
				FileRenderHelper( userID, "file does not match", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, &filehashLocal );
				return false;
			}
		}
	}

	// if this is a VPK file, we have completely cataloged all the VPK files, so no suprises are allowed
	if ( pFileHash->m_PackFileID )
	{
		AddUserReportedFileHash( idxFile, pFileHash, userID, false );
		FileRenderHelper( userID, "unrecognized vpk file", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, NULL );
		return false;
	}


	// now lets see if we have a master file hash for this
	if ( idxMaster != m_treeMasterFileHashes.InvalidIndex() )
	{
		m_cMatchedMasterFile++;

		FileHash_t *pFileHashLocal = &m_treeMasterFileHashes[idxMaster].m_FileHash;
		if ( *pFileHashLocal == *pFileHash )
		{
			m_cMatchedMasterFileHash++;
			return true;
		}
		else
		{
			// did not match - record so that we have a record of the file that did not match ( just for reporting )
			AddUserReportedFileHash( idxFile, pFileHash, userID, false );
			// and then return failure
			FileRenderHelper( userID, "file does not match server master file", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, pFileHashLocal );
			return false;
		}
	}

	// no master record, accumulate individual record so we can get a consensus
	if ( sv_pure_trace.GetInt() == 3 )
	{
		FileRenderHelper( userID, "server does not have hash for this file. Waiting for consensus", pPathID, rgchFilenameFixed, pFileHash, nFileFraction, NULL );
	}

	AddUserReportedFileHash( idxFile, pFileHash, userID, true );

	// we dont have enough data to decide if you match or not yet - so we call it a match
	return true;
}


struct FindFileIndex_t
{
	int	idxFindFile;
};

class CStupidLess
{
public:
	bool Less( const FindFileIndex_t &src1, const FindFileIndex_t &src2, void *pCtx )
	{
		if ( src1.idxFindFile < src2.idxFindFile )
			return true;

		return false;
	}
};

int CPureFileTracker::ListUserFiles( bool bListAll, const char *pchFilenameFind )
{
	CUtlSortVector< FindFileIndex_t, CStupidLess > m_vecReportedFiles;
	int idxFindFile = m_treeAllReportedFiles.FirstInorder();
	while ( idxFindFile != m_treeAllReportedFiles.InvalidIndex() )
	{
		UserReportedFile_t &ufile = m_treeAllReportedFiles[idxFindFile];
		if ( pchFilenameFind && Q_stristr( ufile.m_filename, pchFilenameFind ) )
		{
			FileHash_t filehashLocal;
			EFileCRCStatus eStatus = g_pFileSystem->CheckCachedFileHash( ufile.m_path.String(), ufile.m_filename.String(), 0, &filehashLocal );

			if ( eStatus == k_eFileCRCStatus_GotCRC )
			{
				USERID_t useridFake = { 0, 0 };
				FileRenderHelper( useridFake, "Found: ",ufile.m_path.String(),ufile.m_filename.String(), &filehashLocal, 0, NULL );
				FindFileIndex_t ffi;
				ffi.idxFindFile = idxFindFile;
				m_vecReportedFiles.Insert( ffi );
			}
			else
			{
				Msg( "File not found %s %s %x\n", ufile.m_filename.String(), ufile.m_path.String(), idxFindFile );
			}
		}
		idxFindFile = m_treeAllReportedFiles.NextInorder( idxFindFile );
	}


	int cTotalFiles = 0;
	int cTotalMatches = 0;
	int idx = m_treeUserReportedFileHash.FirstInorder();
	while ( idx != m_treeUserReportedFileHash.InvalidIndex() )
	{
		UserReportedFileHash_t &file = m_treeUserReportedFileHash[idx];

		int idxNext = m_treeUserReportedFileHash.NextInorder( idx );
		int ctMatches = 1;
		int ctFiles = 1;
		// check this against all others for the same file
		while ( idxNext != m_treeUserReportedFileHash.InvalidIndex() && m_treeUserReportedFileHash[idx].m_idxFile == m_treeUserReportedFileHash[idxNext].m_idxFile )
		{
			if ( m_treeUserReportedFileHash[idx].m_FileHash == m_treeUserReportedFileHash[idxNext].m_FileHash )
			{
				ctMatches++;
				cTotalMatches++;
			}
			ctFiles++;
			idxNext = m_treeUserReportedFileHash.NextInorder( idxNext );
		}
		idx = m_treeUserReportedFileHash.NextInorder( idx );
		cTotalFiles++;

		// do we have a master for this one?
		MasterFileHash_t masterFileHashFind;
		masterFileHashFind.m_idxFile = file.m_idxFile;
		int idxMaster = m_treeMasterFileHashes.Find( masterFileHashFind );

		UserReportedFile_t &ufile = m_treeAllReportedFiles[file.m_idxFile];

		bool bOutput = false;
		if ( Q_stristr( ufile.m_filename.String(), "bin\\pak01" )!=NULL || Q_stristr( ufile.m_filename.String(), ".vpk" )!=NULL )
			bOutput = true;
		else
		{
			FileHash_t filehashLocal;
			EFileCRCStatus eStatus = g_pFileSystem->CheckCachedFileHash( ufile.m_path.String(), ufile.m_filename.String(), 0, &filehashLocal );
			if ( eStatus == k_eFileCRCStatus_GotCRC )
			{
				if ( filehashLocal.m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile &&
					file.m_FileHash.m_eFileHashType == FileHash_t::k_EFileHashTypeEntireFile &&
					filehashLocal != file.m_FileHash )
				{
					bOutput = true;
				}
			}
		}

		FindFileIndex_t ffi;
		ffi.idxFindFile = file.m_idxFile;

		if ( ctMatches != ctFiles || idxMaster != m_treeMasterFileHashes.InvalidIndex() || bListAll || ( pchFilenameFind && m_vecReportedFiles.Find( ffi ) != -1 ) || bOutput )
		{
			char rgch[256];
			V_sprintf_safe( rgch, "reports=%d matches=%d Hash details:", ctFiles, ctMatches );
			FileHash_t *pFileHashMaster = NULL;
			if ( idxMaster != m_treeMasterFileHashes.InvalidIndex() )
				pFileHashMaster = &m_treeMasterFileHashes[idxMaster].m_FileHash;
			FileRenderHelper( file.m_userID, rgch, ufile.m_path.String(), ufile.m_filename.String(), &file.m_FileHash, 0, pFileHashMaster );
		}
	}
	Msg( "Total user files %d %d %d \n", m_treeUserReportedFileHash.Count(), cTotalFiles, cTotalMatches );
	Msg( "Total files %d, total with authoritative hashes %d \n", m_treeAllReportedFiles.Count(), m_treeMasterFileHashes.Count() );
	Msg( "Matching files %d %d %d \n", m_cMatchedFile, m_cMatchedMasterFile, m_cMatchedMasterFileHash );

	return 0;
}

int CPureFileTracker::ListAllTrackedFiles( bool bListAll, const char *pchFilenameFind, int nFileFractionMin, int nFileFractionMax )
{
	g_pFileSystem->MarkAllCRCsUnverified();
	
	int cTotal = 0;
	int cTotalMatch = 0;
	int count = 0;
	do
	{
		CUnverifiedFileHash rgUnverifiedFiles[1];
		count = g_pFileSystem->GetUnverifiedFileHashes( rgUnverifiedFiles, ARRAYSIZE( rgUnverifiedFiles ) );

		if ( count && ( bListAll || ( pchFilenameFind && Q_stristr( rgUnverifiedFiles[0].m_Filename, pchFilenameFind ) && rgUnverifiedFiles[0].m_nFileFraction >= nFileFractionMin && rgUnverifiedFiles[0].m_nFileFraction <= nFileFractionMax ) ) )
		{
			USERID_t useridFake = { 0, 0 };
			FileRenderHelper( useridFake, "", rgUnverifiedFiles[0].m_PathID, rgUnverifiedFiles[0].m_Filename, &rgUnverifiedFiles[0].m_FileHash, rgUnverifiedFiles[0].m_nFileFraction, NULL );
			if ( rgUnverifiedFiles[0].m_FileHash.m_PackFileID )
			{
				g_pFileSystem->CheckVPKFileHash( rgUnverifiedFiles[0].m_FileHash.m_PackFileID, rgUnverifiedFiles[0].m_FileHash.m_nPackFileNumber, rgUnverifiedFiles[0].m_nFileFraction, rgUnverifiedFiles[0].m_FileHash.m_md5contents );
			}
			cTotalMatch++;
		}
		if ( count )
			cTotal++;
	} while ( count );

	Msg( "Total files %d Matching files %d \n", cTotal, cTotalMatch );

	return 0;
}


CPureFileTracker g_PureFileTracker;

#define DEBUG_PURE_SERVER
#ifdef DEBUG_PURE_SERVER
void CC_ListPureServerFiles(const CCommand &args)
{
	if ( !sv.IsDedicated() )
		return;
	g_PureFileTracker.ListUserFiles( args.ArgC() > 1 && (atoi(args[1]) > 0), NULL );
}

static ConCommand svpurelistuserfiles("sv_pure_listuserfiles", CC_ListPureServerFiles, "ListPureServerFiles");


void CC_PureServerFindFile(const CCommand &args)
{
	if ( !sv.IsDedicated() )
		return;
	g_PureFileTracker.ListUserFiles( false, args[1] );
}

static ConCommand svpurefinduserfiles("sv_pure_finduserfiles", CC_PureServerFindFile, "ListPureServerFiles");

void CC_PureServerListTrackedFiles(const CCommand &args)
{
	// BUGBUG! Because this code is in engine instead of server, it exists in the client - ugh!
	// Remove this command from client before shipping for realz.
	//if ( !sv.IsDedicated() )
	//	return;
	int nFileFractionMin = args.ArgC() >= 3 ? Q_atoi(args[2]) : 0;
	int nFileFractionMax = args.ArgC() >= 4 ? Q_atoi(args[3]) : nFileFractionMin;
	if ( nFileFractionMax < 0 ) 
		nFileFractionMax = 0x7FFFFFFF;
	g_PureFileTracker.ListAllTrackedFiles( args.ArgC() <= 1, args.ArgC() >= 2 ? args[1] : NULL, nFileFractionMin, nFileFractionMax );
}

static ConCommand svpurelistfiles("sv_pure_listfiles", CC_PureServerListTrackedFiles, "ListPureServerFiles");

void CC_PureServerCheckVPKFiles(const CCommand &args)
{
	if ( sv.IsDedicated() )
		BeginWatchdogTimer( 5 * 60 );							// reset watchdog timer to allow 5 minutes for the VPK check
	g_pFileSystem->CacheAllVPKFileHashes( false, true );
	if ( sv.IsDedicated() )
		EndWatchdogTimer();
}

static ConCommand svpurecheckvpks("sv_pure_checkvpk", CC_PureServerCheckVPKFiles, "CheckPureServerVPKFiles");

#endif
