//========= Copyright © 2006, Electonic Arts(C) 2006 - All Rights Reserved ============//

#ifndef FILEGROUP_H
#define FILEGROUP_H

#include <sys/stat.h>
#include "zlib/zlib.h"

#ifdef _PS3

// #ifndef _RETAIL
// #define TIME_FILE_OPERATIONS
// #endif

//#define MEMCMP_FILE_OPERATIONS
#include "tier0/platform.h"
#include <sys/synchronization.h>
#include <sys/types.h>
#include <sys/ppu_thread.h>
#include <sys/stat.h>
#endif //_PS3

#define FILEGROUP_FORMAT_ID "FGP"
#define PREVIOUS_FILEGROUP_FORMAT_VERSION 1 //TEMP: Used for backwards compatibility
#define FILEGROUP_FORMAT_VERSION 2 //Preload section enhancement
#define FILEGROUP_FORMAT_VERSION_COMPILED_DIRECTORY 3 //Preprocessed directory section
const int MAX_FILE_GROUPS = 7;
const int FILEGROUP_UNCOMPRESSED_BUFFER_SIZE = (8 * 1024);
const int FILEGROUP_READ_CHUNK_SIZE = (64 * 1024);
const int FILEGROUP_READ_STORE_SIZE = FILEGROUP_READ_CHUNK_SIZE * 2;
const int FILEGROUP_READ_SIGNAL_SIZE = FILEGROUP_READ_CHUNK_SIZE * 2;
const int FILEGROUP_READ_THREAD_BUFFER_SIZE = FILEGROUP_READ_CHUNK_SIZE * 4;

#define FILEGROUP_USE_HASH_DIRECTORY
#ifdef FILEGROUP_USE_HASH_DIRECTORY
const int FILEGROUP_DIRECTORY_BUCKETS = 256; //Must be power of 2
const int FILEGROUP_BUCKET_MOD = FILEGROUP_DIRECTORY_BUCKETS-1;
#endif

struct FileIdentifier
{
    char*        mName;
    unsigned int mNameId;
    bool         mNameIdSet;
};

struct DirectoryHeader
{
    char    mId[4];
    int     mVersion;
    int     mNumEntries;
    int     mNumPreloadEntries; //Added for FILEGROUP_FORMAT_VERSION 2
};

struct PreviousDirectoryHeader //TEMP: Used for backwards compatibility
{
    char    mId[4];
    int     mVersion;
    int     mNumEntries;
};

struct DirectoryEntry
{
    char	mName[MAX_PATH];
	size_t	mPosition;
	size_t	mLength;
    size_t  mCompressedLength; //Set to zero if compression not used
};

struct SmallDirectoryEntry
{
//	char	mName[116];
	unsigned int mNameId;
	size_t	mPosition;
	size_t	mLength;
	size_t  mCompressedLength; //Set to zero if compression not used
};

#ifdef _PS3

#ifdef MEMCMP_FILE_OPERATIONS
struct DirectoryEntryExtraInfo
{
    char mFullFileName[MAX_PATH];
};
#endif

struct FileGroupReadBuffer
{
    void                    AllocateBuffer(size_t bufferSize);
    void                    FreeBuffer();
    void                    CloseFile();
    void                    WaitOnDiskEjectRecovery();
    sys_ppu_thread_t        mReadThread;
    unsigned char*          mBuffer;
    size_t                  mBufferSize;
    size_t                  mReadChunkSize;
//    unsigned char           mBuffer[FILEGROUP_READ_THREAD_BUFFER_SIZE];
    int                     mFile;
    char                    mFileName[MAX_PATH]; //Need to store filegroup name so that file can be reopened following disk eject
    uint64_t                mFileBlockSize;
    uint64_t                mFileSectorSize;

    sys_lwmutex_t           mBufferMutex; //Used for all updates to the buffer data
    sys_lwmutex_attribute_t mBufferMutexAttr;
    sys_lwmutex_t           mNextConsumeByteMutex; //Used for synchronization of multiple consumer threads
    sys_lwmutex_attribute_t mNextConsumeByteMutexAttr;
	sys_lwmutex_t           mBufferStartEndPosMutex; //Used for synchronization of buffer start and end positions - don't want to update these while in use by consumer 
	sys_lwmutex_attribute_t mBufferStartEndPosMutexAttr;
    sys_mutex_t             mReadThreadSignalMutex;
    sys_mutex_attribute_t   mReadThreadSignalMutexAttr;
    sys_cond_t              mReadThreadSignal; //Signaled by consumer thread indicating that either:
                                               //- there is space for the read thread to carry on reading
                                               //- or the filegroup has been deleted
    sys_cond_attribute_t    mReadThreadSignalAttr;
    sys_lwmutex_t           mEjectRecoveryMutex; //Used to ensure only one thread performs the disk eject recovery procedure
    sys_lwmutex_attribute_t mEjectRecoveryMutexAttr;

    bool            mFileGroupDeleted;
	size_t          mNextConsumePos; //Position of next byte due for read by the consumer thread
	                                 //Read thread does not overwrite any bytes FILEGROUP_READ_STORE_SIZE bytes before this point.

    //unsigned char*  mNextConsumeByte; //Position of next byte due for read by the consumer thread
    //                                  //Read thread does not overwrite bytes beyond this point 


    size_t          mBufferStartPos; //Position in file corresponding to the beginning of the buffer
    size_t          mCurrentPos; //Current position in file
    unsigned char*  mCurrentByte; //Next byte to be read to in buffer
    size_t          mBufferEndPos; //Position in file corresponding to the end of the buffer (set to zero until initial population of buffer)
    bool            mAwaitingConsumer; //If set, indicates that the read thread is waiting for data to be read by the consumer thread             
#ifdef TIME_FILE_OPERATIONS
    system_time_t m_fReadTime;
    int           m_fReadBytes;
#endif

};

class CFileGroup;

class CFileGroupOpenedFile
{
public:
    CFileGroupOpenedFile();  
    ~CFileGroupOpenedFile();
#ifdef MEMCMP_FILE_OPERATIONS
    void Init(const SmallDirectoryEntry& dirEntry, CFileGroup* parentFileGroup, FileGroupReadBuffer* parentBuffer, char* fullFileName, bool preload);
#else
    void Init(const SmallDirectoryEntry& dirEntry, CFileGroup* parentFileGroup, FileGroupReadBuffer* parentBuffer, bool preload);
#endif
    virtual void FS_fseek( __int64 pos, int seekType );
    virtual long FS_ftell();
    virtual int FS_feof();
    virtual size_t FS_fread( void *dest, size_t destSize, size_t size);
    virtual char *FS_fgets( char *dest, int destSize );
//    const char* GetName() const {return mDirEntry->mName;}
    size_t GetPosition() const {return mDirEntry->mPosition;}
    size_t GetLength() const {return mDirEntry->mLength;}
    size_t GetCompressedLength() const {return mDirEntry->mCompressedLength;}
    bool IsPreloaded() const {return mPreloaded;}
    void FS_fclose();
#ifdef _DEBUG
    static int refCount;
#endif
private:
    void                Rewind(); 
    size_t              ReadFromCompressedData( void *dest, size_t readSize);
    size_t              ReadFromUncompressedData( void *dest, size_t readSize);
    void                TraceMemCopy(void* pDest, const void* pSource, size_t nBytes); 
    SmallDirectoryEntry const * mDirEntry;
    CFileGroup*         mParentFileGroup;
    size_t              mSeekPosIndicator;// This is current position as returned by FS_ftell.
    size_t              mActualPosIndicator;// This is the uncompressed position within the file corresponding to the last read.  
    size_t              mCompressedPosIndicator; //This is the number of compressed bytes which have been read.
    bool                mEof; //Set to true on attempting to read beyond the end of the file

    unsigned char       mUncompressedBuffer[FILEGROUP_UNCOMPRESSED_BUFFER_SIZE];
    //unsigned char*      mRemainingUncompressedBuffer; //Pointer to bytes within the uncompressed buffer which have yet to be read
    //size_t              mNumRemainingUncompressedBytes; //Number of bytes in the compressed buffer which have yet to be read
    int                 mUncompressedBufferStartPos;

    z_stream            mStrm; //Zlib stream
    FileGroupReadBuffer* mParentReadBuffer;
    bool                mPreloaded; 
#ifdef MEMCMP_FILE_OPERATIONS 
    int                 mOrdinaryFile;
#endif

};

class CFileGroupSystem
{
public:
    CFileGroupSystem();
    ~CFileGroupSystem();
    void Init();
    int AddFileGroup(const char* fileGroupName, bool useReadThread = true, int* filegroup_index = NULL, bool usepreload = false, int bufferSize = FILEGROUP_READ_CHUNK_SIZE, int readChunkSize = FILEGROUP_READ_CHUNK_SIZE); 
    void DeleteFileGroup(); 
    void DeleteFileGroup(int ID); 
    void DeleteFileGroup(CFileGroup* fileGroup); 
    void DeleteFileGroup(CFileGroup* fileGroup, int groupIndex); 
    int CurrentFileGroup();
    CFileGroupOpenedFile* FS_fopen( FileIdentifier *filename, const char *options, unsigned int flags, __int64 *size );
    CFileGroupOpenedFile* FS_fopen( int filegroup_index, FileIdentifier *filename, const char *options, unsigned int flags, __int64 *size );
    int FS_stat( FileIdentifier *path, struct stat *buf );
    int FS_stat( FileIdentifier *path, CellFsStat *buf );
    int FS_stat( int filegroup_index, FileIdentifier *path, struct stat *buf );
    int FS_stat( int filegroup_index, FileIdentifier *path, CellFsStat *buf );
    void Lock() {int ret = sys_lwmutex_lock( &mFileGroupSystemMutex, 0 ); if (ret != CELL_OK) printf("Error locking filegroup system mutex %d\n", ret);}
    void Unlock() {int ret = sys_lwmutex_unlock( &mFileGroupSystemMutex); if (ret != CELL_OK) printf("Error unlocking filegroup system mutex %d\n", ret);}
    void DecrementNumFileGroups(){m_numFileGroups--;}
    CFileGroupOpenedFile* GetOpenedFile(); //Get next available CFileGroupOpenedFile from pool
    void FreeOpenedFile(CFileGroupOpenedFile*); //Return CFileGroupOpenedFile to pool
#ifdef _DEBUG
    static int refCount;
#endif
private:
    CFileGroup* m_mapGroup;
    CFileGroup* m_shaderGroup;
    CFileGroup* m_fileGroups[MAX_FILE_GROUPS];
    int m_numFileGroups; 
    int m_currentFileGroup;
	sys_lwmutex_t mFileGroupSystemMutex;
    int m_lastGroupPopulated;

    //Pool of CFileGroupOpenedFile objects
    struct FileGroupFilePoolNode
    {
        CFileGroupOpenedFile thisFile;
        FileGroupFilePoolNode* nextNode;
    };
    FileGroupFilePoolNode* m_openedFilePool;
    FileGroupFilePoolNode* m_availableFileNodes;
    FileGroupFilePoolNode* m_openedFileNodes;
    int m_totalOpenedFiles;
    bool m_initComplete;
};

class CFileGroup
{
public:
    CFileGroup(CFileGroupSystem* pFs);
    ~CFileGroup();
    void Clear();
    int Populate(const char* fileGroupName, bool usepreload = false);
    CFileGroupOpenedFile* FS_fopen( FileIdentifier *pFileName, const char *pOptions, unsigned int flags, __int64 *size );
    int FS_stat( FileIdentifier *path, CellFsStat *buf );
    void DecrementOpenedCount();
    void IncrementOpenedCount(){mOpenedCount++;}
    int GetFile() const {return mReadBuffer.mFile;}
    size_t GetPosOffset() const {return mPosOffset;}
    int  GetOpenedCount(){return mOpenedCount;}
    void* GetPreloadData(){return mPreloadSection;}
    void FlagForDeletion(){mFlaggedForDeletion = true;}
    void Lock() {mFs->Lock();}
    void Unlock() {mFs->Unlock();}
    bool IsPopulated() {return (mReadBuffer.mFile);}
    void FreeOpenedFile(CFileGroupOpenedFile* freeFile){mFs->FreeOpenedFile(freeFile);}
    void StartReadThread();
    void StopReadThread();
    void TidyUpSynchObjects();
    sys_ppu_thread_t GetReadThread(){return mReadBuffer.mReadThread;}
    bool UsingReadThread(){return (mReadBuffer.mReadThread);}
//    unsigned char* GetIgnoreUncompressBuffer(){return mFs->GetIgnoreUncompressBuffer();}
    size_t Size(){return mFileStat.st_size;}
    bool HasBeenDeleted(){return mReadBuffer.mFileGroupDeleted;}
    void CloseFile(){mReadBuffer.CloseFile();}
    void AllocateReadBuffer(size_t bufSize){mReadBuffer.AllocateBuffer(bufSize);}
    void SetReadChunkSize(size_t readChunkSize){mReadBuffer.mReadChunkSize = readChunkSize;}
    void FreeReadBuffer(){mReadBuffer.FreeBuffer();}
#ifdef _DEBUG
    static int refCount;
#endif
#ifdef TIME_FILE_OPERATIONS
    void PrintFileGroupStats();
    void ResetFileTimings();
    system_time_t GetfReadTime(){return mReadBuffer.m_fReadTime;}
    int GetfReadBytes(){return mReadBuffer.m_fReadBytes;}
    void IncrReadTime(system_time_t timeVal){mReadBuffer.m_fReadTime += timeVal;}
    void IncrReadBytes(int bytesVal){mReadBuffer.m_fReadBytes += bytesVal; /*if(mReadBuffer.m_fReadBytes%10485760 == 0) PrintFileGroupStats();*/};
    system_time_t m_memCopyTime;
    system_time_t m_uncompressTime;
    int m_fsReads;
    int m_fgets;
    int m_uncompressCalls;
    int m_rewinds;
    int m_fileJumps;
    int m_bytesfRead;
    int m_bytesReadFromFileByConsumer;
    int m_bytesReadFromBuffer;
#endif
private:

    CellFsErrno PopulateDirectory(DirectoryHeader* hdr, int* entriesPopulated);
    int FindFile(FileIdentifier *pFileName);
    char* ReformatFileName(char* inputFileName);
	unsigned int HashFileName(const char * fName);
    SmallDirectoryEntry*     mDirectoryEntries;
    unsigned int		     mNumDirectoryEntries;
    void*                    mPreloadSection;
    int                      mPreloadEntries;
#ifdef FILEGROUP_USE_HASH_DIRECTORY
    struct HashBucket
    {
        unsigned int mPosition;
        unsigned int mCount;
    };
    unsigned int*            mHashDirectory; //ordered into buckets
    HashBucket               mHashBuckets[FILEGROUP_DIRECTORY_BUCKETS]; 
#endif
	unsigned int		     mLastOpenedDirectoryIndex;
    unsigned int		     mLastFailedId;
    CellFsStat                      mFileStat;
    size_t                          mPosOffset; //Position at which the data starts
    CFileGroupSystem*               mFs;
    int                             mOpenedCount;
    bool                            mFlaggedForDeletion;
    FileGroupReadBuffer             mReadBuffer;
#ifdef MEMCMP_FILE_OPERATIONS 
    DirectoryEntryExtraInfo*    mDirectoryExtraInfo;
#endif
};

#endif //_PS3
#endif //FILEGROUP_H

