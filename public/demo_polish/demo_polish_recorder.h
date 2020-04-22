//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef C_DEMO_POLISH_RECORDER_H
#define C_DEMO_POLISH_RECORDER_H
#ifdef _WIN32
#pragma once
#endif

//------------------------------------------------------------------------------------------------------------------------

class CUserCmd;
class CIntDemoPolishFile;
class CFinalDemoPolishFile;
class FinalPolishElementData;
class CPathManager;
class CBonePolishData;
class CIntDemoPolishFile;
class CAnimDataFile;
class CDemoPolishFile;
class CUserInputDataFile;
class CHeadingDataFile;
class CHeadingData;
class CBoneAccessor;

//------------------------------------------------------------------------------------------------------------------------

//
// Records to intermediate polish files.  These files are then analyzed (on shutdown)
// and converted into a final set of polish data, which can be interpreted during demo
// playback.
//
class CDemoPolishRecorder
{
public:
	static CDemoPolishRecorder& Instance();

	bool Init( char const* pDemoBaseFileName );
	bool Shutdown();

	void Think( float flTime );

	void RecordAnimData( int iPlayerEntIndex, CStudioHdr* pStudioHdr, float fCycle, matrix3x4_t const& renderTransform,
		                 Vector pos[], Quaternion q[], int iBoneMask, CBoneAccessor const& boneAccessor );
	void RecordStandingHeadingChange( int iPlayerEntIndex, float flCurrentHeading, float flGoalHeading );
	void RecordJumpEvent( int iPlayerEntIndex );
	void RecordUserInput( CUserCmd const* pUserCmd );

public:
	bool m_bInit;

private:
	CDemoPolishRecorder();
	~CDemoPolishRecorder();

	enum EFileType
	{
		kAnim,
		kEvents,
		kNumFileTypes
	};

	bool InitLocalFileSystem();
	bool SetupFiles();
	bool WriteHeader();
	bool InitUserInputFile();
	bool SetupFile( char const* pFilenameStem, int iPlayerIndex, int iEntIndex, CUtlVector< CDemoPolishFile* >& vFiles, int iFileType );
	bool ClosePolishFiles( EFileType iFileType );
	CDemoPolishFile* GetFileForPlayerAt( int iPlayerEntIndex, EFileType iFileType );
	float GetTime() const;
	void FlushQueuedTurnEvents();

	char m_szDemoBaseFileName[MAX_PATH];

	CUtlVector< CDemoPolishFile* >* m_vFileLists[kNumFileTypes];
	CUtlVector< CDemoPolishFile* > m_vAnimFiles;
	CUtlVector< CDemoPolishFile* > m_vEventFiles;

	CUtlVector< int > m_vPlayerEntIndexMap;		// Maps from player lookup index to entity index

	CUtlVector< CHeadingData* > m_standingHeadingEvents;	// In-place turn events that will be filtered out before written to disk

	CPathManager* m_pPathManager;
};

//------------------------------------------------------------------------------------------------------------------------

#endif // C_DEMO_POLISH_RECORDER_H
