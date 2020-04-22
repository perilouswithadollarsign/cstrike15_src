//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PURE_SERVER_H
#define PURE_SERVER_H
#ifdef _WIN32
#pragma once
#endif


#include "ifilelist.h"
#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"


class KeyValues;


class CPureServerWhitelist
{
public:
	friend class CAllowFromDiskList;
	friend class CForceMatchList;
	friend class CPureFileTracker;

	// Call this to free it.
	static CPureServerWhitelist* Create( IFileSystem *pFileSystem );
	void				Release();
	
	// Get the file lists.
	IFileList*			GetAllowFromDiskList();		// This is the list of files that can be loaded off disk. Make sure to Release() it when finished!
	IFileList*			GetForceMatchList();		// This is the list of files that must match CRCs. Make sure to Release() it when finished!
	
	// Load up any entries in the KV file.
	bool				LoadFromKeyValues( KeyValues *kv );

	// Encode for networking.
	void				Encode( CUtlBuffer &buf );
	void				Decode( CUtlBuffer &buf );

	// Calls IFileSystem::CacheFileCRCs for all files and directories marked check_crc.
	void				CacheFileCRCs();

	// Instead of loading keyvalues, you can call this to make it force all files to come from Steam.
	// This is used with sv_pure 2.
	void				EnableFullyPureMode();
	bool				IsInFullyPureMode() const;

	// Used by sv_pure command when you're connected to a server.
	void				PrintWhitelistContents();

private:

	CPureServerWhitelist();
	~CPureServerWhitelist();

	void				Init( IFileSystem *pFileSystem );
	void				Term();

private:

	class CAllowFromDiskList : public IFileList
	{
	public:
		virtual bool			IsFileInList( const char *pFilename );
		virtual void			Release();

		CPureServerWhitelist	*m_pWhitelist;
	};
	class CForceMatchList : public IFileList
	{
	public:
		virtual bool			IsFileInList( const char *pFilename );
		virtual void			Release();
	
		CPureServerWhitelist	*m_pWhitelist;
	};

	class CCommand
	{
	public:
		CCommand();
		~CCommand();

	public:		
		bool			m_bAllowFromDisk;
		bool			m_bCheckCRC;
		unsigned short	m_LoadOrder;	// What order this thing was specified in the whitelist file.
	};


private:

	void				UpdateCommandStats( CUtlDict<CPureServerWhitelist::CCommand*,int> &commands, int *pHighest, int *pLongestPathName );
	void				PrintCommand( const char *pFileSpec, const char *pExt, int maxPathnameLen, CPureServerWhitelist::CCommand *pCommand );
	int					FindCommandByLoadOrder( CUtlDict<CPureServerWhitelist::CCommand*,int> &commands, int iLoadOrder );

	void InternalCacheFileCRCs( CUtlDict<CCommand*,int> &theList, ECacheCRCType eType );

	CCommand* GetBestEntry( const char *pFilename );

	void EncodeCommandList( CUtlDict<CPureServerWhitelist::CCommand*,int> &theList, CUtlBuffer &buf );
	void DecodeCommandList( CUtlDict<CPureServerWhitelist::CCommand*,int> &theList, CUtlBuffer &buf );

	CPureServerWhitelist::CCommand* CheckEntry( 
		CUtlDict<CPureServerWhitelist::CCommand*,int> &dict, 
		const char *pEntryName, 
		CPureServerWhitelist::CCommand *pBestEntry );

private:
	unsigned short	m_LoadCounter;	// Incremented as we load things so their m_LoadOrder increases.
	int m_RefCount;

	// Commands are applied to files in order.
	CUtlDict<CCommand*,int>	m_FileCommands;				// file commands
	CUtlDict<CCommand*,int> m_RecursiveDirCommands;		// ... commands
	CUtlDict<CCommand*,int> m_NonRecursiveDirCommands;	// *.* commands
	IFileSystem *m_pFileSystem;
	
	CAllowFromDiskList m_AllowFromDiskList;
	CForceMatchList m_ForceMatchList;
	
	// In this mode, all files must come from Steam.
	bool m_bFullyPureMode;
};


CPureServerWhitelist* CreatePureServerWhitelist( IFileSystem *pFileSystem );

struct UserReportedFileHash_t
{
	int m_idxFile;
	USERID_t m_userID;
	FileHash_t m_FileHash;

	static bool Less( const UserReportedFileHash_t& lhs, const UserReportedFileHash_t& rhs )
	{
		if ( lhs.m_idxFile < rhs.m_idxFile )
			return true;
		if ( lhs.m_idxFile > rhs.m_idxFile )
			return false;
		if ( lhs.m_userID.uid.steamid.m_SteamLocalUserID.As64bits < rhs.m_userID.uid.steamid.m_SteamLocalUserID.As64bits )
			return true;
		return false;
	}
};

struct UserReportedFile_t
{
	CRC32_t m_crcIdentifier;
	CUtlString m_filename;
	CUtlString m_path;
	int m_nFileFraction;

	static bool Less( const UserReportedFile_t& lhs, const UserReportedFile_t& rhs )
	{
		if ( lhs.m_crcIdentifier < rhs.m_crcIdentifier )
			return true;
		if ( lhs.m_crcIdentifier > rhs.m_crcIdentifier )
			return false;
		if ( lhs.m_nFileFraction < rhs.m_nFileFraction )
			return true;
		if ( lhs.m_nFileFraction > rhs.m_nFileFraction )
			return false;
		int nCmp = Q_strcmp( lhs.m_filename.String(), rhs.m_filename.String() );
		if ( nCmp < 0 )
			return true;
		if ( nCmp > 0 )
			return false;
		nCmp = Q_strcmp( lhs.m_path.String(), rhs.m_path.String() );
		if ( nCmp < 0 )
			return true;
		return false;
	}
};

struct MasterFileHash_t
{
	int m_idxFile;
	int m_cMatches;
	FileHash_t m_FileHash;

	static bool Less( const MasterFileHash_t& lhs, const MasterFileHash_t& rhs )
	{
		return lhs.m_idxFile < rhs.m_idxFile;
	}

};

class CPureFileTracker
{
public:
	CPureFileTracker():
	  m_treeAllReportedFiles( UserReportedFile_t::Less ),
		  m_treeMasterFileHashes( MasterFileHash_t::Less ),
		  m_treeUserReportedFileHash( UserReportedFileHash_t::Less )
	  {
		  m_flLastFileReceivedTime = 0.f;
		  m_cMatchedFile = 0;
		  m_cMatchedMasterFile = 0;
		  m_cMatchedMasterFileHash = 0;
		  m_cMatchedFileFullHash = 0;
	  }

	  void AddUserReportedFileHash( int idxFile, FileHash_t *pFileHash, USERID_t userID, bool bAddMasterRecord );
	  bool DoesFileMatch( const char *pPathID, const char *pRelativeFilename, int nFileFraction, FileHash_t *pFileHash, USERID_t userID );
	  int ListUserFiles( bool bListAll, const char *pchFilenameFind );
	  int ListAllTrackedFiles( bool bListAll, const char *pchFilenameFind, int nFileFractionMin, int nFileFractionMax );

	  CUtlRBTree< UserReportedFile_t, int > m_treeAllReportedFiles;
	  CUtlRBTree< MasterFileHash_t, int > m_treeMasterFileHashes;
	  CUtlRBTree< UserReportedFileHash_t, int > m_treeUserReportedFileHash;

	  float m_flLastFileReceivedTime;
	  int m_cMatchedFile;
	  int m_cMatchedMasterFile;
	  int m_cMatchedMasterFileHash;
	  int m_cMatchedFileFullHash;

};

extern CPureFileTracker g_PureFileTracker;


#endif // PURE_SERVER_H

