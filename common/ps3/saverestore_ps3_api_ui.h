//===== Copyright © 1996-2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: A convenient, clean interface for communicating between UI and
// the PS3 save system.
//
// $NoKeywords: $
//===========================================================================//

#ifndef PS3_SAVEUIAPI_H
#define PS3_SAVEUIAPI_H

#include "threadtools.h"
#include "ps3_pathinfo.h"


class CUtlBuffer;

 
enum eSaveOperationTag
{
	kSAVE_TAG_UNKNOWN = 0,
	kSAVE_TAG_INITIALIZE,
	kSAVE_TAG_WRITE_STEAMINFO,
	kSAVE_TAG_WRITE_AUTOSAVE,		// writing an autosave to the container
	kSAVE_TAG_READ_SAVE,			// reading a save from the container
	kSAVE_TAG_WRITE_SAVE,			// writing a manual save to the container
	kSAVE_TAG_READ_SCREENSHOT,		// reading a screenshot from the container
	kSAVE_TAG_DELETE_SAVE,			// deleting a save from the container
};


// this class will hold the result of an async operation.
// poll JobDone() until it returns true, then you can
// look at the other fields for what actually transpired.
class CPS3SaveRestoreAsyncStatus
{
public:
	inline bool JobDone() { return !!m_bDone; }

	inline int GetSonyReturnValue(); // will return either an error from the enum below or one of these (defined by Sony):
	/*
		CELL_SAVEDATA_RET_OK
		success

		CELL_SAVEDATA_ERROR_CBRESULT
		Callback function returned an error

		CELL_SAVEDATA_ERROR_ACCESS_ERROR
		HDD access error

		CELL_SAVEDATA_ERROR_INTERNAL
		Fatal internal error

		CELL_SAVEDATA_ERROR_PARAM
		Error in parameter to be set to utility (application bug)

		CELL_SAVEDATA_ERROR_NOSPACE
		Insufficient free space (application bug: lack of free space must be judged and handled within the callback function)

		CELL_SAVEDATA_ERROR_BROKEN
		Save data corrupted (modification detected, etc.)

		CELL_SAVEDATA_ERROR_FAILURE
		Save/load of save data failed (file could not be found, etc.)

		CELL_SAVEDATA_ERROR_BUSY
		Save data utility function was called simultaneously

		CELL_SAVEDATA_ERROR_NOUSER
		Specified user does not exist

		CELL_SAVEDATA_ERROR_SIZEOVER			
		Exceeds the maximum size of the saved data 

		CELL_SAVEDATA_ERROR_NODATA				
		Specified save data does not exist on the HDD 

		CELL_SAVEDATA_ERROR_NOTSUPPORTED		
		Called in an unsupported state
	*/
	enum eSaveErrors // our own error types; not from Sony but formatted the same so as to look uniform.
	{
		CELL_SAVEDATA_ERROR_THREAD_WAS_BUSY = -11,
		CELL_SAVEDATA_ERROR_FILE_NOT_FOUND = -12, // you tried to load a file that wasn't in the container
		CELL_SAVEDATA_ERROR_WRONG_USER = -13, // tried to open or operate on a container belonging to a different user
		CELL_SAVEDATA_ERROR_NO_TOC = -14, // failed to read TOC
		CELL_SAVEDATA_ERROR_WRAPPER = -15, // a general failure somewhere in the wrapper classes.
		CELL_SAVEDATA_ERROR_NO_WORK_TO_DO = -16,
		CELL_SAVEDATA_WARNING_ASYNC_FAILSAFE = -17, // an operation completed but somehow forgot to trip the async object (so a failsafe triggered)
		
	};
	
	uint32 m_nCurrentOperationTag; // an arbitrary enum that you can set to whatever you like, except 0. Zero means "not doing anything."
	
	int m_nSonyRetValue;
	bool m_bUseSystemDialogs; ///< when true, says that we should have the OS pop up dialogs for error conditions, rather than letting the game handle them based on the return value here.
	CInterlockedInt m_bDone;
	uint32 m_uiAdditionalDetails;

	CPS3SaveRestoreAsyncStatus() : m_bDone(true), m_bUseSystemDialogs(false), m_nSonyRetValue(0), m_nCurrentOperationTag(kSAVE_TAG_UNKNOWN), m_uiAdditionalDetails( 0 )
	{};
};

inline int CPS3SaveRestoreAsyncStatus::GetSonyReturnValue()
{
	return m_nSonyRetValue;
}

class IPS3SaveSteamInfoProvider
{
public:
	virtual CUtlBuffer * GetInitialLoadBuffer() = 0;
	virtual CUtlBuffer * GetSaveBufferForCommit() = 0;
	virtual CUtlBuffer * PrepareSaveBufferForCommit() = 0;
};

// the interface class
class IPS3SaveRestoreToUI : public IAppSystem
{
public:
	// the maximum possible size of a COMMENT field (including the terminal zero)
	enum { PS3_SAVE_COMMENT_LENGTH = 128 };

	enum FileSecurity_t
	{
		kSECURE,
		kHACKABLE,
		kSYSTEM,
	};

	// information about one file in the container 
	struct CPS3ContainerFileInfo 
	{		

		char fileName[64];
		char comment[PS3_SAVE_COMMENT_LENGTH];
		FileSecurity_t fileType;	
		uint32 size;		// in bytes					
		time_t modtime;		// modification time as a POSIX time_t

		CPS3ContainerFileInfo() { memset(fileName, 0, sizeof(fileName));  memset(comment, 0, sizeof(comment)); }
	};

	// the struct that gets written into by GetContainerInfo()
	struct CPS3ContainerFacts
	{
		int hddFreeSizeKB;	// free size on disk IN KILOBYTES
		int sizeKB;		// current size of container in kilobytes
		bool bOwnedByAnotherUser;  // can't load if this is the case

		CUtlVectorConservative< CPS3ContainerFileInfo > files;
	};


	struct PS3SaveGameInfo_t
	{
		PS3SaveGameInfo_t() : m_nFileTime(0) {}

		CUtlString		m_InternalName; // eg 0000005.SAV
		CUtlString		m_Filename;  // eg autosave.ps3.sav
		CUtlString		m_ScreenshotFilename; // eg autosave.ps3.tga  if one exists
		CUtlString		m_Comment;
		time_t			m_nFileTime;
	};

	virtual ~IPS3SaveRestoreToUI(){};
public:

	/// You have to call this before doing any other save operation. In particular,
	/// there may be an error opening the container. Poll on Async, and when it's done,
	/// look in the return value to see if it succeeded, or if not, why not. 
	/// When bCreateIfMissing is set, it will create a new container where none exists. 
	virtual void Initialize( CPS3SaveRestoreAsyncStatus *pAsync, IPS3SaveSteamInfoProvider *pSteamInfoProvider, bool bCreateIfMissing, int nKBRequired ) = 0;

	// Save the given file into the container. (ie /dev_hdd1/tempsave/foo.ps3.sav will 
	// be written into the container as foo.ps3.sav ). You can optionally specify a second
	// file to be written at the same time, eg a screenshot, because batching two writes
	// to happen at once is far far faster than having two batches one after another.
	// ALL game progress files must be 
	// written as secure; for now, even the screenshots should be, as that puts the work
	// of CRC onto the operating system. 
	virtual void Write( CPS3SaveRestoreAsyncStatus *pAsync, const char *pSourcepath, const char *pScreenshotPath, const char *pComment, FileSecurity_t nSecurity = kSECURE ) = 0;

	// A more complicated and intelligent form of save writing, specifically for the case of autosaves.
	// The source filename given (as an absolute path) will be written to "autosave.ps3.sav". 
	// Meanwhile, the existing "autosave.ps3.sav" will be renamed "autosave01.ps3.sav",
	// any "autosave01.ps3.sav" will be renamed "autosave02.ps3.sav", and so on up to a maximum
	// number of autosaves N, plus the base autosave.ps3.sav. The highest "autosave%02d.ps3.sav"
	// will therefore have a number N. Excess autosaves with numbers >N will be deleted.
	// If you specify a ScreenshotExtension (such as "tga"), the same operation is performed
	// for every file above, where ."sav" is replaced with the given extension. 
	virtual void WriteAutosave(  CPS3SaveRestoreAsyncStatus *pAsync, 
		const char *pSourcePath, // eg "/dev_hdd1/tempsave/autosave.ps3.sav"
		const char *pComment, // the comment field for the new autosave.
		const unsigned int nMaxNumAutosaves ) = 0; // should be at least 1; the highest numbered autosave will be N-1.

	// A way of writing clouded files into container, clouded files over
	// a certain age are purged from container
	virtual void WriteCloudFile( CPS3SaveRestoreAsyncStatus *pAsync,
		const char *pSourcePath,
		const unsigned int nMaxNumCloudFiles ) = 0; // should be at least 1; the highest numbered cloud file will be N-1.

	// Load a file from the container into the given directory. 
	// give it a pointer to a CPS3SaveRestoreAsyncStatus struct that you have created and
	// intend to poll.
	virtual void Load( CPS3SaveRestoreAsyncStatus *pAsync, const char *pFilename, const char *pDestFullPath ) = 0;

	// kill one or two files (eg, save and screenshot). 
	// async will respond when done.
	virtual void Delete( CPS3SaveRestoreAsyncStatus *pAsync, const char *pFilename, const char *pOtherFilename = NULL ) = 0;

	// synchronously retrieve information on the files in the container. Lacks some of the container-wide'
	// info of the above function, and may have slightly out of date information, but is a synchronous call
	// and returns precisely the structure needed by CBaseModPanel::GetSaveGameInfos(). 
	virtual void GetFileInfoSync( CUtlVector< PS3SaveGameInfo_t > &saveGameInfos, bool bFindAll ) = 0;

	// try to find Steam's schema file for the user and stuff it into the container.
	// returns false if it couldn't find the file locally (in which case you should
	// not wait for the async object to be "done", as the job wasn't initiated); 
	// true if it found it locally and queued up an async job to write it.
	virtual void WriteSteamInfo( CPS3SaveRestoreAsyncStatus *pAsync ) = 0;

	// returns whether save thread is busy
	virtual bool IsSaveUtilBusy() = 0;

	// returns the m_nCurrentOperationTag field of the most recent async op to
	// have run, or kSAVE_TAG_UNKNOWN if none has been enqueued yet. This tag
	// changes the moment a job is made active and remains until the next job
	// starts.
	virtual uint32 GetCurrentOpTag() = 0;

	// returns the version of container, used to fire off events when container
	// contents changes.
	virtual uint32 GetContainerModificationVersion() = 0;

	// sets the cloud crypto key.
	virtual void SetCloudFileCryptoKey( uint64 uiCloudCryptoKey ) = 0;
};


#define IPS3SAVEUIAPI_VERSION_STRING "IPS3SAVEUIAPI_001"

#endif
