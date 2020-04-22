//========= Copyright © 2006, Electonic Arts(C) 2006 - All Rights Reserved ============//

#include "tier0/platform.h"

#ifdef _PS3
#include "FileGroup.h"
#include <unistd.h>
#include <sys/timer.h>
#include "memalloc.h" 
// #include "debug/inc/debug.h"
// #include <tty.h>
#include "generichash.h"
#include <ctype.h> 
#include "ps3/ps3_console.h"
#include <sys/stat.h>
#include "tier1/strtools.h"
#include "tier0/dbg.h"
//#include <libsntuner.h>
#ifdef _DEBUG
int CFileGroupOpenedFile::refCount = 0;
int CFileGroupSystem::refCount = 0;
int CFileGroup::refCount = 0;
#endif

const int MAX_OPENED_FILES = 8;
const int FILEGROUP_READ_CMD_PRIO = 1000;
const int FILEGROUP_READ_CMD_STACKSIZE = 16*1024;

#define DebugPrint(fmt, ...)	Msg(fmt, ## __VA_ARGS__)

static void TidyUpSynchObjects(FileGroupReadBuffer* pBuf)
{
	int ret = sys_lwmutex_destroy(&pBuf->mBufferStartEndPosMutex);
	if (ret != CELL_OK) {
		printf("TidyUpSynchObjects sys_lwmutex_destroy failed: %d\n", ret);
		exit(ret);
	}
    ret = sys_cond_destroy(pBuf->mReadThreadSignal);
    if (ret != CELL_OK) {
        printf("TidyUpSynchObjects sys_cond_destroy failed: %d\n", ret);
        exit(ret);
    }
    ret = sys_mutex_destroy(pBuf->mReadThreadSignalMutex);
    if (ret != CELL_OK) {
        printf("TidyUpSynchObjects sys_mutex_destroy failed: %d\n", ret);
        exit(ret);
    }
}

void FileGroupReadBuffer::WaitOnDiskEjectRecovery()
{
    //Grab mutex
    int ret = sys_lwmutex_trylock(&mEjectRecoveryMutex);

    if (ret == CELL_OK) 
    {
        int fileGroupHandle = 0;
        CellFsErrno retFs;
        while(retFs != CELL_FS_SUCCEEDED)
        {
            if(fileGroupHandle)
            {
                cellFsClose(fileGroupHandle);
            }
            retFs = PS3_RetryOpenWhileDiskEjected(mFileName, CELL_FS_O_RDONLY, &fileGroupHandle);
            if (retFs == CELL_FS_SUCCEEDED)
            {
                mFile = fileGroupHandle;
                uint64_t pos = 0;
                retFs = cellFsLseek(mFile,(uint64_t)mCurrentPos,CELL_FS_SEEK_SET,&pos);
                if(retFs == CELL_FS_EIO)
                {
                    PS3_DisplayDiskErrorDialog(); //Assume dirty disk, rather than disk eject if this point is reached
                }
            }
            else
            {
                Msg("FileGroupReadBuffer::WaitOnDiskEjectRecovery: Fatal disk error\n");
                exit(-1);
            }
        }
        PS3_ClearDiskErrorDialog();
        sys_lwmutex_unlock(&mEjectRecoveryMutex);
    }
    else //i.e. another thread is running 
    {
        Msg("Disk eject waiting on retry\n");
        ret = sys_lwmutex_lock(&mEjectRecoveryMutex,0); //Wait for disk eject recovery to complete
        ret = sys_lwmutex_unlock(&mEjectRecoveryMutex);
        Msg("Disk eject waiting on retry complete\n");
    }
}

void FileGroupReadBuffer::CloseFile()
{
    if (mFile) 
    {
        cellFsClose(mFile); 
        mFile = NULL;
        int ret = sys_lwmutex_destroy(&mEjectRecoveryMutex);
        if (ret != CELL_OK) {
            Msg("sys_lwmutex_destroy failed for mEjectRecoveryMutex: %d\n", ret);
        }
        ret = sys_lwmutex_destroy(&mBufferMutex);
        if (ret != CELL_OK) {
            printf("TidyUpSynchObjects sys_lwmutex_destroy failed: %d\n", ret);
        }
        ret = sys_lwmutex_destroy(&mNextConsumeByteMutex);
        if (ret != CELL_OK) {
            printf("TidyUpSynchObjects sys_lwmutex_destroy failed: %d\n", ret);
        }
    }
}

void FileGroupReadBuffer::AllocateBuffer(size_t readBufferSize)
{
	MEM_ALLOC_CREDIT();
    if (mBuffer)
    {
        FreeBuffer();
    }

    mBuffer = new unsigned char[readBufferSize];
    mBufferSize = readBufferSize;
    mCurrentByte = mBuffer;
}

void FileGroupReadBuffer::FreeBuffer()
{
    if (mBuffer)
    {
        delete[] mBuffer;
        mBuffer = NULL;
        mBufferSize = 0;
        mCurrentByte = NULL;
		mNextConsumePos = 0;
		mAwaitingConsumer = false;
		mBufferEndPos = 0;
		mBufferStartPos = 0;
		mCurrentPos = 0;
    }
}

static void thr_FileGroupRead(uint64_t arg)
{
    FileGroupReadBuffer* pBuf = reinterpret_cast<FileGroupReadBuffer*>(arg);
    while (1) {
        int ret = sys_mutex_lock(pBuf->mReadThreadSignalMutex, 0);
        while (!pBuf->mAwaitingConsumer && !pBuf->mFileGroupDeleted) // Data ready for read and filegroup not flagged for deletion
        {
            ret = sys_lwmutex_lock(&pBuf->mBufferMutex,0);
            //Read next chunk of data
            if ((pBuf->mBufferEndPos > 0) && //i.e. we're overwriting existing data
				(pBuf->mNextConsumePos > 0) && 
				((pBuf->mCurrentPos + FILEGROUP_READ_CHUNK_SIZE) - pBuf->mNextConsumePos) > (pBuf->mBufferSize - FILEGROUP_READ_STORE_SIZE))
            {
                pBuf->mAwaitingConsumer = true; //Need to wait for consumer to read more data
            }
            else
            {

                //Do the seek and read
#ifdef TIME_FILE_OPERATIONS
                system_time_t time1 = sys_time_get_system_time();
#endif
                uint64_t pos;
                uint64_t bytesRead;
                ret = cellFsLseek(pBuf->mFile,(int64_t)pBuf->mCurrentPos,CELL_FS_SEEK_SET,&pos);
                if (ret == CELL_FS_SUCCEEDED)
                {
                    ret = cellFsRead(pBuf->mFile,pBuf->mCurrentByte,FILEGROUP_READ_CHUNK_SIZE,&bytesRead);
                }
                while (ret!= CELL_FS_SUCCEEDED)
                {
                    cellFsClose(pBuf->mFile);
                    sys_timer_usleep(250000); //Sleep for 0.25 seconds
                    pBuf->WaitOnDiskEjectRecovery();
                    ret = cellFsLseek(pBuf->mFile,(int64_t)pBuf->mCurrentPos,CELL_FS_SEEK_SET,&pos);
                    if(ret == CELL_FS_SUCCEEDED)
                    {
                        ret = cellFsRead(pBuf->mFile,pBuf->mCurrentByte,FILEGROUP_READ_CHUNK_SIZE,&bytesRead);
                    }
                    if(ret == CELL_FS_EIO) //Assume dirty disk
                    {
                        PS3_DisplayDiskErrorDialog();
                    }
                    else if (ret == CELL_FS_SUCCEEDED)
                    {
                        PS3_ClearDiskErrorDialog();
                    }
                }
#ifdef TIME_FILE_OPERATIONS
                system_time_t time2 = sys_time_get_system_time();
                pBuf->m_fReadTime += (time2-time1);
                pBuf->m_fReadBytes += bytesRead;
#endif

                if (pBuf->mCurrentByte == pBuf->mBuffer)
                {
					ret = sys_lwmutex_lock(&pBuf->mBufferStartEndPosMutex,0);
					pBuf->mBufferStartPos = pBuf->mCurrentPos;
					ret = sys_lwmutex_unlock(&pBuf->mBufferStartEndPosMutex);
                }
                pBuf->mCurrentPos += bytesRead;
                pBuf->mCurrentByte += bytesRead;
                if (bytesRead == FILEGROUP_READ_CHUNK_SIZE)
                {
                    if ((pBuf->mCurrentByte - pBuf->mBuffer) >= pBuf->mBufferSize)
                    {
						//We've reached the end of the buffer
						sys_lwmutex_lock(&pBuf->mBufferStartEndPosMutex,0); //Prevent any 'consumes' from calculating available data while buffer is reset 
                        pBuf->mBufferEndPos = pBuf->mCurrentPos;
                        pBuf->mCurrentByte = pBuf->mBuffer;
                        if (pBuf->mNextConsumePos == 0) 
                        {
                            pBuf->mAwaitingConsumer = true;
                        }
						sys_lwmutex_unlock(&pBuf->mBufferStartEndPosMutex);
                    }
                }
                else
                {
                    //Assume EOF - await signal from consumer that there is more data to read
                    pBuf->mAwaitingConsumer = true;
                }
           }
            ret = sys_lwmutex_unlock(&pBuf->mBufferMutex);
        }
        if (pBuf->mFileGroupDeleted)
        {
           int ret = sys_mutex_unlock(pBuf->mReadThreadSignalMutex);
           TidyUpSynchObjects(pBuf);
           pBuf->CloseFile(); 
           pBuf->FreeBuffer();
           pBuf->mReadThread = NULL;
           pBuf->mFileGroupDeleted = false;
           sys_ppu_thread_exit(1);
        }
        ret = sys_cond_wait(pBuf->mReadThreadSignal, 0);
        if (ret == CELL_OK)
        {
            pBuf->mAwaitingConsumer = false;
        }
    }
}

CFileGroupSystem::CFileGroupSystem()
{
    m_numFileGroups = 0; 
    m_lastGroupPopulated = -1;
    m_currentFileGroup = 0;
    for(int i=0;i<MAX_FILE_GROUPS;i++) 
    {
        m_fileGroups[i]=NULL;
    }

    //Create the filesystem mutex
    sys_lwmutex_attribute_t mutexAttr;
    sys_lwmutex_attribute_initialize(mutexAttr);
    mutexAttr.attr_recursive = SYS_SYNC_RECURSIVE;
    int ret = sys_lwmutex_create(&mFileGroupSystemMutex, &mutexAttr);
    if (ret != CELL_OK) {
        printf("CFileGroupSystem::CFileGroupSystem sys_lwmutex_create failed: %d\n", ret);
        exit(1);
    }

    m_openedFilePool = NULL;
    m_availableFileNodes = NULL;
    m_openedFileNodes = NULL;
    m_totalOpenedFiles = 0;
    m_initComplete = false;

#ifdef _DEBUG
    refCount++;
#endif
}

CFileGroupSystem::~CFileGroupSystem()
{
    for(int i=0;i<MAX_FILE_GROUPS;i++) 
    {
        if (m_fileGroups[i])
        {
            m_fileGroups[i]->Clear();
            delete m_fileGroups[i];
        }
    }
    int ret = sys_lwmutex_destroy(&mFileGroupSystemMutex);
    if (ret != CELL_OK) {
        printf("CFileGroupSystem::~CFileGroupSystem sys_lwmutex_destroy failed: %d\n", ret);
    }
#ifdef _DEBUG
    refCount--;
#endif
}

void CFileGroupSystem::Init()
{
	MEM_ALLOC_CREDIT();

    //Initialise pool of CFileGroupOpenedFile objects:
    DebugPrint("Initialising pool of %d FileGroupFilePoolNodes for filegroupsystem\n", MAX_OPENED_FILES);
    m_openedFilePool = new FileGroupFilePoolNode[MAX_OPENED_FILES];
    m_availableFileNodes = m_openedFilePool;
    for(int i=0;i<(MAX_OPENED_FILES-1);i++)
    {
        m_openedFilePool[i].nextNode = &m_openedFilePool[i+1];
    }
    m_openedFilePool[MAX_OPENED_FILES-1].nextNode = NULL;

    //DebugPrint("Initialising filegroup objects for filegroupsystem\n");
    for (int i = 0; i<(MAX_FILE_GROUPS); i++)
    {
        m_fileGroups[i] = new CFileGroup(this);
        m_fileGroups[i]->Clear();
    }
    //int fileSystemMemAllocation = sizeof(FileGroupFilePoolNode)*MAX_OPENED_FILES + sizeof(CFileGroup)*(MAX_FILE_GROUPS);
    //DebugPrint("Approx filegroupsystem memory usage: %d\n", fileSystemMemAllocation);
    m_initComplete = true;
}

int CFileGroupSystem::AddFileGroup(const char* fileGroupName, bool useReadThread /* = true */, int* group_index /* = NULL */, bool usePreload /* = false */, int bufferSize /* = FILEGROUP_READ_CHUNK_SIZE */, int readChunkSize /* = FILEGROUP_READ_CHUNK_SIZE */)
{
    Lock(); //Critical Section
 
    int retVal = -1;
    int fileGroupIndex = -1;
    if (group_index)
    {
        *group_index = -1;
    }
    if( bufferSize < FILEGROUP_READ_CHUNK_SIZE * 4 )
    {
        bufferSize = FILEGROUP_READ_CHUNK_SIZE * 4;
    }
    Assert(readChunkSize <= bufferSize);
    if(readChunkSize <= bufferSize)
    {
        if (!m_initComplete) //Delay initialisation until this point so that we don't allocate buffer memory unless it's needed 
        {
            Init();
        }
        CFileGroup* newGroup = NULL;
        //Get free filegroup object
        for(int i=0; (i<MAX_FILE_GROUPS) && !newGroup ;i++) 
        {
            if (!m_fileGroups[i]->IsPopulated())
            {
                newGroup = m_fileGroups[i];
                fileGroupIndex = i;
            }
        }
        if (newGroup)
        {
            newGroup->Clear();
            int entriesPopulated = newGroup->Populate(fileGroupName, usePreload);
            if (entriesPopulated > 0) 
            {
                retVal = entriesPopulated;

                if (useReadThread)
                {
                    newGroup->AllocateReadBuffer(bufferSize);
                    newGroup->StartReadThread();
                }
                else
                {
                    newGroup->AllocateReadBuffer(bufferSize);
                    newGroup->SetReadChunkSize(readChunkSize);
                }

                if (fileGroupIndex > -1)
                {
                    m_numFileGroups++;
                    m_currentFileGroup = fileGroupIndex;
                    if (group_index)
                    {
                        *group_index = fileGroupIndex; //No need to set last group populated - assume caller will specify the index on calls to deletefilegroup
                    }
                    else
                    {
                        m_lastGroupPopulated = fileGroupIndex;
                    }
                }
                
                DebugPrint("CFileGroupSystem: Successfully opened file group named %s as index %d\n", fileGroupName, fileGroupIndex);
                if(usePreload)
                {
                    DebugPrint(" using preload\n");
                }
                else
                {
                    DebugPrint("\n");
                }
            }
            else
            {
                if(entriesPopulated < 0)
                {
                    DebugPrint("CFileGroupSystem: Error opening file group named %s\n", fileGroupName);
                }
                else
                {
                    DebugPrint("Error opening file group %s, no entries found.\n", fileGroupName);
                }
                newGroup->Clear();
            }
        }
        else
        {
            DebugPrint("CFileGroupSystem: Error opening file group %s. All %d file groups are already in use.\n", fileGroupName, MAX_FILE_GROUPS);
        }
    }
    Unlock(); //Critical Section End
    return retVal;
}

int CFileGroupSystem::CurrentFileGroup()
{
    return m_lastGroupPopulated;
}

//Delete most recently added file group
void CFileGroupSystem::DeleteFileGroup()
{
    Lock();
    if (m_lastGroupPopulated > -1)
    {
        DeleteFileGroup(m_lastGroupPopulated);
    }
    else
    {
        DebugPrint("CFileGroupSystem::deleteFileGroup Error, no filegroups exist\n"); 
    }
    Unlock();
}

void CFileGroupSystem::DeleteFileGroup(int groupIndex)
{
    Lock();
    if (groupIndex > -1 && m_fileGroups[groupIndex])
    {
        DeleteFileGroup(m_fileGroups[groupIndex], groupIndex);
    }
    else
    {
        DebugPrint("CFileGroupSystem::deleteFileGroup Error, filegroup %d does not exist\n", groupIndex); 
    }
    Unlock();
}

void CFileGroupSystem::DeleteFileGroup(CFileGroup* fileGroup)
{
    Lock();
    for(int i=0; i<MAX_FILE_GROUPS; i++)
    {
        if (m_fileGroups[i] == fileGroup)
        {
            DeleteFileGroup(fileGroup, i);
        }
    }
    Unlock();
}

#ifdef TIME_FILE_OPERATIONS
void CFileGroup::PrintFileGroupStats()
{
    DebugPrint("\n");
    DebugPrint("Stats for file group %s:\n", mReadBuffer.mFileName);
    DebugPrint("filegroup, Memcpy(s), fread(s), inflate(s), fsreads, fgets, inflates, rewinds, filejumps, bytes from buffer, bytes from file, fread(bytes)\n");
    DebugPrint("Level,%5.2f,%5.2f,%5.2f,%d,%d,%d,%d,%d,%d,%d,%d\n",
        float(m_memCopyTime)/1000000.0, float(GetfReadTime())/1000000.0, float(m_uncompressTime)/1000000.0,
        m_fsReads, m_fgets, m_uncompressCalls, m_rewinds, m_fileJumps,
        m_bytesReadFromBuffer, m_bytesReadFromFileByConsumer, GetfReadBytes());
    DebugPrint("\n");
}
#endif

void CFileGroupSystem::DeleteFileGroup(CFileGroup* fileGroup, int groupIndex) 
{
    Lock();
    if (fileGroup->GetOpenedCount() == 0)
    {
        DebugPrint("Deleting file group %d\n", groupIndex);
        fileGroup->Clear();
        m_numFileGroups--;
#ifdef TIME_FILE_OPERATIONS
        fileGroup->PrintFileGroupStats();
#endif
        if(fileGroup->UsingReadThread())
        {
            fileGroup->StopReadThread();
        }
        else
        {
            fileGroup->CloseFile();
            fileGroup->FreeReadBuffer();
        }
    }
    else
    {
        DebugPrint("Flagging file group %d for deletion, %d files still open\n", groupIndex, fileGroup->GetOpenedCount());
        fileGroup->FlagForDeletion();
    }
    if (m_currentFileGroup == groupIndex)
    {
        m_currentFileGroup = 0;
    }
    if (m_lastGroupPopulated == groupIndex)
    {
        m_lastGroupPopulated = -1;
    }
    Unlock();
}

CFileGroupOpenedFile* CFileGroupSystem::FS_fopen( FileIdentifier *filename, const char *options, unsigned int flags, __int64 *size )
{
    Lock(); //Critical section
    CFileGroupOpenedFile* retVal = NULL;
	if ( options[0] == 'r' ) //Filegroup files can only be opened for read
    {
        int iFileGroup = m_currentFileGroup; //Start at the current file group
        for(int i=0; (i<MAX_FILE_GROUPS) && (!retVal) ;i++, iFileGroup++)
        {
            //At the end of the list, loop back to the beginning:
            if (iFileGroup==MAX_FILE_GROUPS)
            {
                iFileGroup=0;
            }
            if (m_fileGroups[iFileGroup] && m_fileGroups[iFileGroup]->IsPopulated())
            {
                retVal = m_fileGroups[iFileGroup]->FS_fopen(filename,options,flags,size);
                if (retVal)
                {
                    m_currentFileGroup = iFileGroup;
                }
            }
        }
    }

    Unlock();
    return retVal;
}

CFileGroupOpenedFile* CFileGroupSystem::FS_fopen( int filegroup_index, FileIdentifier *filename, const char *options, unsigned int flags, __int64 *size )
{
    Lock(); //Critical section
    CFileGroupOpenedFile* retVal = NULL;
    if ( options[0] == 'r' ) //Filegroup files can only be opened for read
    {
        if (filegroup_index >= 0 && filegroup_index < MAX_FILE_GROUPS)
        {
            if (m_fileGroups[filegroup_index] && m_fileGroups[filegroup_index]->IsPopulated())
            {
                retVal = m_fileGroups[filegroup_index]->FS_fopen(filename,options,flags,size);
            }
        }
    }
    Unlock();
    return retVal;
}

int CFileGroupSystem::FS_stat( FileIdentifier *path, CellFsStat *buf )
{
    Lock(); //Critical section
    int retVal = -1;
    int iFileGroup = m_currentFileGroup; //Start at the current file group
    for(int i=0; (i<MAX_FILE_GROUPS) && (retVal == -1) ;i++, iFileGroup++)
    {
        //At the end of the list, loop back to the beginning:
        if (iFileGroup==MAX_FILE_GROUPS)
        {
            iFileGroup=0;
        }
        if (m_fileGroups[i] && m_fileGroups[i]->IsPopulated())
        {
            retVal = m_fileGroups[i]->FS_stat(path,buf);
        }
    }
    Unlock();
    return retVal;
}

int CFileGroupSystem::FS_stat( int filegroup_index, FileIdentifier *path, CellFsStat *buf )
{
    Lock(); //Critical section
    int retVal = -1;
    if (filegroup_index >= 0 && filegroup_index < MAX_FILE_GROUPS)
    {
        if (m_fileGroups[filegroup_index] && m_fileGroups[filegroup_index]->IsPopulated())
        {
            retVal = m_fileGroups[filegroup_index]->FS_stat(path,buf);
        }
    }
    Unlock();
    return retVal;
}

int CFileGroupSystem::FS_stat( FileIdentifier *path, struct stat *buf )
{
    int ret;
    CellFsStat cellBuf;
    CellFsErrno retFs = FS_stat(path, &cellBuf);
    if(retFs == CELL_FS_SUCCEEDED)
    {
        buf->st_atime = cellBuf.st_atime;
        buf->st_blksize = cellBuf.st_blksize;
        buf->st_ctime = cellBuf.st_ctime;
        buf->st_gid = cellBuf.st_gid;
        buf->st_mode = cellBuf.st_mode;
        buf->st_mtime = cellBuf.st_mtime;
        buf->st_size = cellBuf.st_size;
        buf->st_uid = cellBuf.st_uid;
        buf->st_dev = 0;
        buf->st_ino = 0;
        buf->st_nlink = 0;
        buf->st_rdev = 0;
        buf->st_blocks = 0;
        ret = 0;
    }
    else
    {
        ret = -1;
        //TBD: SET ERRNO
    }
    return ret;
}

int CFileGroupSystem::FS_stat( int filegroup_index, FileIdentifier *path, struct stat *buf )
{
    int ret;
    CellFsStat cellBuf;
    CellFsErrno retFs = FS_stat(filegroup_index, path, &cellBuf);
    if(retFs == CELL_FS_SUCCEEDED)
    {
        buf->st_atime = cellBuf.st_atime;
        buf->st_blksize = cellBuf.st_blksize;
        buf->st_ctime = cellBuf.st_ctime;
        buf->st_gid = cellBuf.st_gid;
        buf->st_mode = cellBuf.st_mode;
        buf->st_mtime = cellBuf.st_mtime;
        buf->st_size = cellBuf.st_size;
        buf->st_uid = cellBuf.st_uid;
        buf->st_dev = 0;
        buf->st_ino = 0;
        buf->st_nlink = 0;
        buf->st_rdev = 0;
        buf->st_blocks = 0;
        ret = 0;
    }
    else
    {
        ret = -1;
        //TBD: SET ERRNO
    }
    return ret;
}

CFileGroupOpenedFile* CFileGroupSystem::GetOpenedFile() //Get next available CFileGroupOpenedFile from pool
{
    CFileGroupOpenedFile* retVal = NULL;
    Assert(m_availableFileNodes);
    if (m_availableFileNodes) //Take the next available node from the head of the list
    {
        FileGroupFilePoolNode* thisNode = m_availableFileNodes;
        retVal = &thisNode->thisFile;
        //Remove from list of available nodes
        m_availableFileNodes = thisNode->nextNode; 
        //Add to list of opened nodes
        thisNode->nextNode = m_openedFileNodes;
        m_openedFileNodes = thisNode;
        m_totalOpenedFiles++;
    }
    return retVal;
}

void CFileGroupSystem::FreeOpenedFile(CFileGroupOpenedFile* freeFile) //Get next available CFileGroupOpenedFile from pool
{
    FileGroupFilePoolNode* thisNode = m_openedFileNodes; 
    FileGroupFilePoolNode* prevNode = NULL; 
    //Find the node object in the list of opened files
    while (thisNode && (&thisNode->thisFile != freeFile))
    {
        prevNode = thisNode;
        thisNode = thisNode->nextNode; 
    }
    Assert(thisNode); //Check that the node object was found
    if (thisNode)
    {
        //Remove from list of opened nodes
        if (prevNode)
        {
            prevNode->nextNode = thisNode->nextNode;
        }
        else
        {
            m_openedFileNodes = thisNode->nextNode;
        }
        m_totalOpenedFiles--;
        //Add to list of available nodes
        thisNode->nextNode = m_availableFileNodes;
        m_availableFileNodes = thisNode; 
    }
}

CFileGroup::CFileGroup(CFileGroupSystem* pFs):
mDirectoryEntries(NULL),
mFs(pFs),
mOpenedCount(0),
mPreloadSection(NULL),
mPreloadEntries(0)
{
#ifdef _DEBUG
    refCount++;
#endif
    mReadBuffer.mFile = NULL;
    mReadBuffer.mFileBlockSize = 0;
    mReadBuffer.mFileSectorSize = 0;
    mReadBuffer.mReadThread = NULL;
    mReadBuffer.mAwaitingConsumer = false;
    mReadBuffer.mFileGroupDeleted = false;
	mReadBuffer.mNextConsumePos = 0;
    mReadBuffer.mBuffer = NULL;
    mReadBuffer.mBufferSize = 0;
    mReadBuffer.mCurrentByte = NULL;
	mReadBuffer.mBufferEndPos = 0;
	mReadBuffer.mBufferStartPos = 0;
	mReadBuffer.mCurrentPos = 0;
    mReadBuffer.mReadChunkSize = FILEGROUP_READ_CHUNK_SIZE;
    mFlaggedForDeletion = false;
#ifdef TIME_FILE_OPERATIONS
    ResetFileTimings();
#endif
#ifdef MEMCMP_FILE_OPERATIONS
    mDirectoryExtraInfo = NULL;
#endif
#ifdef FILEGROUP_USE_HASH_DIRECTORY
    mHashDirectory = NULL;
#endif
}

CFileGroup::~CFileGroup()
{
    if (mDirectoryEntries)
    {
        delete[] mDirectoryEntries;
    }
    if (mReadBuffer.mFile) 
    {
        cellFsClose(mReadBuffer.mFile);
    }
#ifdef _DEBUG
    refCount--;
#endif
#ifdef MEMCMP_FILE_OPERATIONS
    if (mDirectoryExtraInfo)
    {
        delete[] mDirectoryExtraInfo;
    }
#endif
}

unsigned int CFileGroup::HashFileName(const char * fName)
{
	return HashStringCaselessConventional(fName);
}

#ifdef TIME_FILE_OPERATIONS
void CFileGroup::ResetFileTimings()
{
    m_memCopyTime = 0;
    mReadBuffer.m_fReadTime = 0;
    mReadBuffer.m_fReadBytes = 0;
    m_uncompressTime = 0;
    m_fsReads = 0;
    m_fgets = 0;
    m_uncompressCalls = 0;
    m_rewinds = 0;
    m_fileJumps = 0;
    m_bytesReadFromFileByConsumer = 0;
    m_bytesReadFromBuffer = 0;
}
#endif

void CFileGroup::StartReadThread()
{
	sys_lwmutex_attribute_initialize(mReadBuffer.mBufferStartEndPosMutexAttr);
	mReadBuffer.mBufferStartEndPosMutexAttr.attr_recursive = SYS_SYNC_RECURSIVE;
	int ret = sys_lwmutex_create(&mReadBuffer.mBufferStartEndPosMutex, &mReadBuffer.mBufferStartEndPosMutexAttr);
	if (ret != CELL_OK) {
		printf("CFileGroup::StartReadThread sys_lwmutex_create failed: %d\n", ret);
		exit(1);
	}

	//Create the condition object
    sys_mutex_attribute_initialize(mReadBuffer.mReadThreadSignalMutexAttr);
    ret = sys_mutex_create(&mReadBuffer.mReadThreadSignalMutex, &mReadBuffer.mReadThreadSignalMutexAttr);
    sys_cond_attribute_initialize(mReadBuffer.mReadThreadSignalAttr);
    ret = sys_cond_create(&mReadBuffer.mReadThreadSignal, mReadBuffer.mReadThreadSignalMutex, &mReadBuffer.mReadThreadSignalAttr);

    mReadBuffer.mCurrentPos = mPosOffset;
    mReadBuffer.mFileGroupDeleted = false;

    sys_ppu_thread_create(&mReadBuffer.mReadThread,
                      thr_FileGroupRead, reinterpret_cast<uint64_t>(&mReadBuffer),
                      FILEGROUP_READ_CMD_PRIO, FILEGROUP_READ_CMD_STACKSIZE,
                      NULL, "Filegroup_ReadThread");
}

void CFileGroup::StopReadThread()
{
    mReadBuffer.mFileGroupDeleted = true;
    sys_cond_signal_to(mReadBuffer.mReadThreadSignal,mReadBuffer.mReadThread);
}

void CFileGroup::Clear()
{
    Lock();
    if (mOpenedCount > 0)
    {
        DebugPrint("WARNING: Clearing filegroup while %d entries are still open\n", mOpenedCount); 
    }
    if (mDirectoryEntries)
    {
        delete[] mDirectoryEntries;
        mDirectoryEntries = NULL;
    }
#ifdef FILEGROUP_USE_HASH_DIRECTORY
    if (mHashDirectory)
    {
        delete[] mHashDirectory;
        mHashDirectory = NULL;
    }
    memset(mHashBuckets,0,FILEGROUP_DIRECTORY_BUCKETS*sizeof(HashBucket));
    Assert((FILEGROUP_DIRECTORY_BUCKETS & FILEGROUP_BUCKET_MOD) == 0); //Confirm FILEGROUP_DIRECTORY_BUCKETS is power of two
#endif
#ifdef MEMCMP_FILE_OPERATIONS
    if (mDirectoryExtraInfo)
    {
        delete[] mDirectoryExtraInfo;
        mDirectoryExtraInfo = NULL;
    }
#endif
    if (mPreloadSection)
    {
        free(mPreloadSection);
        mPreloadSection=NULL;
    }
    mPreloadEntries = 0;
    mLastFailedId = 0;
    mNumDirectoryEntries = 0;
    mLastOpenedDirectoryIndex = 0;
    mFlaggedForDeletion = false;
    mOpenedCount = 0;
    mReadBuffer.mReadChunkSize = FILEGROUP_READ_CHUNK_SIZE;

    Unlock();
}

char* CFileGroup::ReformatFileName(char* inputFileName)
{
    //Sort out filename...
    //Fix slashes
    char* fname = inputFileName;
    char correctSeparator = '/';
    char incorrectSeparator = '\\';
    while ( *fname )
    {
        if ( *fname == incorrectSeparator || *fname == correctSeparator)
        {
            *fname = correctSeparator;
            if(fname[1] == incorrectSeparator || fname[1] == correctSeparator)
            {
                strcpy(fname+1,fname+2); //Remove multiple slashes from filenames
            }
        }
        fname++;
    }

    //Adjust filename to remove the initial "/...path.../game/...gamemode.../"
    fname = inputFileName;
    char* ptr = NULL;
    while ((ptr = strstr(fname,"/game/")) ||
        (ptr = strstr(fname,"/GAME/")))
    {
        fname = ptr + strlen("/game");
    }
    if (fname != inputFileName) //i.e. we located a game directory
    {
        if (ptr=strchr(fname+1,'/'))
        {
            fname = ptr+1; //Ignore subdirectory
        }
    }
    else
    {
        //instead search for stdshaders directory (to handle the special case where the shaders filegroup is generated within the src directory)
        while ((ptr = strstr(fname,"stdshaders/")) ||
            (ptr = strstr(fname,"STDSHADERS/")))
        {
            fname = ptr + strlen("stdshaders/");
        }
    }

    //Convert to upper case
    char *str = fname;
    while( str && *str )
    {
        *str = (char)toupper(*str);
        str++;
    }

    return fname;

}

CellFsErrno CFileGroup::PopulateDirectory(DirectoryHeader* hdr, int* entriesPopulated)
{
	DirectoryEntry fullDirEntry;
    uint64_t bytesRead;
    mDirectoryEntries = new SmallDirectoryEntry[hdr->mNumEntries];
    mNumDirectoryEntries = hdr->mNumEntries;
#ifdef FILEGROUP_USE_HASH_DIRECTORY
    mHashDirectory = new unsigned int[hdr->mNumEntries];
#endif
#ifdef MEMCMP_FILE_OPERATIONS
    mDirectoryExtraInfo = new DirectoryEntryExtraInfo[hdr->mNumEntries];
#endif
    CellFsErrno retFs = CELL_FS_SUCCEEDED;
    *entriesPopulated = 0;
    if(hdr->mVersion == FILEGROUP_FORMAT_VERSION_COMPILED_DIRECTORY)
    {
        retFs = cellFsRead(mReadBuffer.mFile,mDirectoryEntries,(uint64_t)(sizeof(SmallDirectoryEntry)*hdr->mNumEntries),&bytesRead);
        mPosOffset += bytesRead;
#ifdef FILEGROUP_USE_HASH_DIRECTORY
        if(retFs == CELL_FS_SUCCEEDED)
        {
            for(int i=0;i<hdr->mNumEntries;i++)
            {
                int bucketId = mDirectoryEntries[i].mNameId & FILEGROUP_BUCKET_MOD;
                ++(mHashBuckets[bucketId].mCount);
            }
        }
#endif
    }
    else
    {
        for(int i=0;(i<hdr->mNumEntries)&&(mNumDirectoryEntries>0)&&(retFs == CELL_FS_SUCCEEDED);i++)
        {
            //bytesRead = fread(&fullDirEntry,1,sizeof(DirectoryEntry),mReadBuffer.mFile);
            retFs = cellFsRead(mReadBuffer.mFile,&fullDirEntry,(uint64_t)sizeof(DirectoryEntry),&bytesRead);
            if(retFs == CELL_FS_SUCCEEDED)
            {
#ifdef MEMCMP_FILE_OPERATIONS
                strcpy(mDirectoryExtraInfo[i].mFullFileName,fullDirEntry.mName);
#endif
                if (bytesRead != sizeof(DirectoryEntry)) 
                {
                    //ERROR: abort at this point
                    DebugPrint("ERROR: FilegroupSystem error reading directory entry\n");
                    delete[] mDirectoryEntries; 
                    mDirectoryEntries = NULL;
                    mNumDirectoryEntries = 0;
                    return retFs;
                }
                else
                {
                    mPosOffset += bytesRead;
                    mDirectoryEntries[i].mCompressedLength = fullDirEntry.mCompressedLength;
                    mDirectoryEntries[i].mLength = fullDirEntry.mLength;
                    mDirectoryEntries[i].mPosition = fullDirEntry.mPosition;

                    char* fname = ReformatFileName(fullDirEntry.mName);

                    mDirectoryEntries[i].mNameId = HashFileName(fname);
#ifdef FILEGROUP_USE_HASH_DIRECTORY
                    int bucketId = mDirectoryEntries[i].mNameId & FILEGROUP_BUCKET_MOD;
                    ++(mHashBuckets[bucketId].mCount);
#endif
                } //if (bytesRead != sizeof(DirectoryEntry))
            } //if (retFs == CELL_FS_SUCCEEDED)
        } //for loop
    }

    if(retFs == CELL_FS_SUCCEEDED)
    {
#ifdef FILEGROUP_USE_HASH_DIRECTORY
        //populate hash directory buckets
        int currentPos = 0;
        for(int j = 0; j<FILEGROUP_DIRECTORY_BUCKETS; j++)
        {
            if (mHashBuckets[j].mCount>0)
            {
                mHashBuckets[j].mPosition = currentPos;
                currentPos += mHashBuckets[j].mCount;
                mHashBuckets[j].mCount=0; //Reset for use as a counter in the next section
            }
        }
        for(int i = 0; i<mNumDirectoryEntries; i++)
        {
            int bucketId = mDirectoryEntries[i].mNameId & FILEGROUP_BUCKET_MOD;
            mHashDirectory[mHashBuckets[bucketId].mPosition + mHashBuckets[bucketId].mCount] = i;
            ++(mHashBuckets[bucketId].mCount);
        }
#endif
        *entriesPopulated = mNumDirectoryEntries;
    }
    return retFs;
}

int CFileGroup::Populate(const char* fileGroupName, bool usePreload /*=false*/)
{
	MEM_ALLOC_CREDIT();
    Lock();

    int entriesPopulated = 0;
    CellFsStat fileGroupStat;
    int fileGroupHandle = 0;
    DirectoryHeader hdr;
    uint64_t pos;
    bool firstFail = true;

    CellFsErrno retFs = CELL_FS_SUCCEEDED-1; //Initialise to a non-success value
    while(retFs != CELL_FS_SUCCEEDED) //Loop is repeated following any stat/read errors
    {
        if(fileGroupHandle)
        {
            //Clear up any previous half completed attempt
            cellFsClose(fileGroupHandle);
            Clear();
            sys_timer_usleep(250000); //Sleep for 0.25 seconds
            if(!firstFail)
            {
                PS3_DisplayDiskErrorDialog(); //Probably dirty disk if we hit this point
            }
            else
            {
                firstFail = false;
            }
        }
        retFs = PS3_RetryStatWhileDiskEjected(fileGroupName, &fileGroupStat);
        if(retFs != CELL_FS_SUCCEEDED)
        {
            entriesPopulated = -1;
            goto xit;
        }
        retFs = PS3_RetryOpenWhileDiskEjected(fileGroupName, CELL_FS_O_RDONLY, &fileGroupHandle);
        if(retFs != CELL_FS_SUCCEEDED)
        {
            entriesPopulated = -1;
            goto xit;
        }

        retFs = cellFsFGetBlockSize(fileGroupHandle,&mReadBuffer.mFileSectorSize,&mReadBuffer.mFileBlockSize);
        if(retFs == CELL_FS_SUCCEEDED)
        {
            //DebugPrint("Filegroup %s has blocksize %d, sectorsize %d\n", mReadBuffer.mFileName, mReadBuffer.mFileBlockSize, mReadBuffer.mFileSectorSize);
            mReadBuffer.mFile = fileGroupHandle;
            Q_strncpy(mReadBuffer.mFileName, fileGroupName, MAX_PATH);
            mFileStat = fileGroupStat;
            mPosOffset = 0;

            retFs = cellFsLseek(mReadBuffer.mFile,0,CELL_FS_SEEK_SET, &pos); //Ensure we are at the beginning of the file

            if(retFs == CELL_FS_SUCCEEDED)
            {
                uint64_t bytesRead;
                retFs = cellFsRead(mReadBuffer.mFile,&hdr,(uint64_t)sizeof(hdr),&bytesRead);
                if(retFs == CELL_FS_SUCCEEDED)
                {
                    mPosOffset += bytesRead;
                    if (!usePreload)
                    {
                        hdr.mNumPreloadEntries = 0;
                    }

                    if ((hdr.mVersion >= FILEGROUP_FORMAT_VERSION) && (hdr.mNumEntries > 0) && (!mDirectoryEntries)) //Check that filegroup is not already populated, and that there are entries available to read 
                    {
                        retFs = PopulateDirectory(&hdr,&entriesPopulated);
                        if(retFs == CELL_FS_SUCCEEDED && entriesPopulated > 0)
                        {
                            if (hdr.mNumPreloadEntries > 0)
                            {
                                if(mPreloadSection != NULL)
                                {
                                    free(mPreloadSection);
                                }
                                mPreloadSection = malloc(mDirectoryEntries[hdr.mNumPreloadEntries].mPosition);
                                retFs = cellFsRead(mReadBuffer.mFile,mPreloadSection,(uint64_t)mDirectoryEntries[hdr.mNumPreloadEntries].mPosition,&bytesRead);
                                if ((retFs == CELL_FS_SUCCEEDED) && (bytesRead==mDirectoryEntries[hdr.mNumPreloadEntries].mPosition))
                                {
                                    mPreloadEntries = hdr.mNumPreloadEntries;
                                }
                                else
                                {
                                    printf("ERROR: Unable to read %d bytes of filegroup preload section. Preload disabled\n", mDirectoryEntries[hdr.mNumPreloadEntries].mPosition);
                                }
                            }
                        } //if(retFs == CELL_FS_SUCCEEDED && entriesPopulated > 0)
                    }//if ((hdr.mVersion == FILEGROUP_FORMAT_VERSION) && (hdr.mNumEntries > 0) && (!mDirectoryEntries))
                } //if (retFs == CELL_FS_SUCCEEDED) cellfsread hdr 
            } //if (retFs == CELL_FS_SUCCEEDED) cellFsLseek
        } //if (retFs == CELL_FS_SUCCEEDED) cellFsFGetBlockSize
    } //while(retFs != CELL_FS_SUCCEEDED)
    PS3_ClearDiskErrorDialog();

    if(entriesPopulated > 0)
    {
        //Initialise mutex used for synchronising recovery from disk eject
        sys_lwmutex_attribute_initialize(mReadBuffer.mEjectRecoveryMutexAttr);
        int ret = sys_lwmutex_create(&mReadBuffer.mEjectRecoveryMutex, &mReadBuffer.mEjectRecoveryMutexAttr);
        if (ret != CELL_OK) {
            Msg("CFileGroup::Populate sys_lwmutex_create of mEjectRecoveryMutex failed: %d\n", ret);
        }

        //Initialise read buffer mutex
        sys_lwmutex_attribute_initialize(mReadBuffer.mBufferMutexAttr);
        mReadBuffer.mBufferMutexAttr.attr_recursive = SYS_SYNC_RECURSIVE;
        ret = sys_lwmutex_create(&mReadBuffer.mBufferMutex, &mReadBuffer.mBufferMutexAttr);
        if (ret != CELL_OK) {
            printf("CFileGroup::Populate sys_lwmutex_create failed: %d\n", ret);
            exit(1);
        }

        //Initialise read consumer mutex
        sys_lwmutex_attribute_initialize(mReadBuffer.mNextConsumeByteMutexAttr);
        mReadBuffer.mNextConsumeByteMutexAttr.attr_recursive = SYS_SYNC_RECURSIVE;
        ret = sys_lwmutex_create(&mReadBuffer.mNextConsumeByteMutex, &mReadBuffer.mNextConsumeByteMutexAttr);
        if (ret != CELL_OK) {
            printf("CFileGroup::Populate sys_lwmutex_create failed: %d\n", ret);
            exit(1);
        }

        mLastOpenedDirectoryIndex = 0;
#ifdef TIME_FILE_OPERATIONS
        ResetFileTimings();
#endif
   }

xit:
    Unlock();
    return entriesPopulated;
}

int CFileGroup::FindFile(FileIdentifier *pFileName)
{
    int foundIndex = -1;

    if (!mFlaggedForDeletion && !mReadBuffer.mFileGroupDeleted)
    {
        if(!pFileName->mNameIdSet)
        {
            pFileName->mNameId = HashFileName(pFileName->mName);
            pFileName->mNameIdSet = true;
        }
        unsigned int index = mLastOpenedDirectoryIndex;
        //Don't repeat the search if we already know this file doesn't exist
        if (!mLastFailedId || (pFileName->mNameId != mLastFailedId)) 
        {
            //First check the current and next positions in the ordered list
            if ((mDirectoryEntries[index].mNameId == pFileName->mNameId) || //current
                ((++index < mNumDirectoryEntries) && (mDirectoryEntries[index].mNameId == pFileName->mNameId))) //next
            {
                foundIndex = index;
            }
            else
            {
#ifdef FILEGROUP_USE_HASH_DIRECTORY
                //At this point resort to the hash directory
                int bucketId = pFileName->mNameId & FILEGROUP_BUCKET_MOD;
                //printf("Using hash table to search %d entries for filename %d in bucket %d\n", mHashBuckets[bucketId].mCount, fileNameId, bucketId);
                //Search through the relevant bucket
                for (int i=mHashBuckets[bucketId].mPosition; i< (mHashBuckets[bucketId].mPosition + mHashBuckets[bucketId].mCount) && foundIndex < 0; i++)
                {
                    if (mDirectoryEntries[mHashDirectory[i]].mNameId == pFileName->mNameId)
                    {
                        foundIndex = mHashDirectory[i];
                    }
                }
#else
                //Loop through the remaining directory entries, starting with the current entry
                for(int i=index, j=0; j<(mNumDirectoryEntries-2) && foundIndex < 0; i++,j++)
                {
                    //At the end of the list, loop back to the beginning:
                    if (i==mNumDirectoryEntries)
                    {
                        i=0;
                    }
                    if (mDirectoryEntries[i].mNameId == fileNameId)
                    {
                        foundIndex = i;
                    }
                }
#endif
            }
        }
        if(foundIndex < 0)
        {
            mLastFailedId = pFileName->mNameId;
        }
    }
    return foundIndex;
}

CFileGroupOpenedFile* CFileGroup::FS_fopen( FileIdentifier *pFileName, const char *pOptions, unsigned int flags, __int64 *size )
{
    Lock();

    CFileGroupOpenedFile* retVal = NULL;
    int foundIndex = FindFile(pFileName);

    if (foundIndex>=0)
    {
        CFileGroupOpenedFile* openedFile = mFs->GetOpenedFile();
        Assert(openedFile);
#ifdef MEMCMP_FILE_OPERATIONS
        openedFile->Init(mDirectoryEntries[foundIndex],this,&mReadBuffer,mDirectoryExtraInfo[foundIndex].mFullFileName), (foundIndex<mPreloadEntries);
#else
        openedFile->Init(mDirectoryEntries[foundIndex],this,&mReadBuffer, (foundIndex<mPreloadEntries));
#endif
        if(!openedFile->IsPreloaded())
        {
#ifdef TIME_FILE_OPERATIONS
            if (!((foundIndex == mLastOpenedDirectoryIndex) || (foundIndex == mLastOpenedDirectoryIndex+1)))
            {
                ++m_fileJumps;
//                if(mPreloadEntries>0)
//                {
//                    DebugPrint("Jump from level load filegroup index %d to index %d\n", mLastOpenedDirectoryIndex, foundIndex );
//                }
            }
#endif
            mLastOpenedDirectoryIndex = foundIndex;
        }
            
        if (size)
        {
            *size = mDirectoryEntries[foundIndex].mLength;
        }
        retVal = openedFile;
    }

    Unlock();
    return retVal;
}

int CFileGroup::FS_stat( FileIdentifier *path, CellFsStat *buf )
{
    Lock();
    int retVal = -1;
    int foundIndex = FindFile(path);

    if (foundIndex > -1)
    {
        //Default the stat details to those of the parent group file
        *buf = mFileStat;
        buf->st_size = mDirectoryEntries[foundIndex].mLength; //Set the correct filesize
        buf->st_mode = S_IFREG | S_IRUSR ; //Set the bitmask to indicate a read only file
        //CM:TODO Set Block count????
        retVal = 0; 
    }

    Unlock();
    return retVal;
}

void CFileGroup::DecrementOpenedCount()
{
    Lock();
    mOpenedCount--; 
    if (mFlaggedForDeletion && (mOpenedCount == 0))
    {
        mFs->DeleteFileGroup(this);
    }
    Unlock();
}

CFileGroupOpenedFile::CFileGroupOpenedFile()
: mEof(false), mSeekPosIndicator(-1), mActualPosIndicator(-1), mParentFileGroup(NULL), mDirEntry(NULL),
 mPreloaded(false)
{
#ifdef _DEBUG
    refCount++;
#endif
}  

CFileGroupOpenedFile::~CFileGroupOpenedFile()
{
#ifdef _DEBUG
    refCount--;
#endif
}

#ifdef MEMCMP_FILE_OPERATIONS
void CFileGroupOpenedFile::Init(const SmallDirectoryEntry& dirEntry, CFileGroup* parentFileGroup, FileGroupReadBuffer* parentBuffer, char* fullFileName, bool preloaded)
#else
void CFileGroupOpenedFile::Init(const SmallDirectoryEntry& dirEntry, CFileGroup* parentFileGroup, FileGroupReadBuffer* parentBuffer, bool preloaded)
#endif
{
    mPreloaded=preloaded;
    mDirEntry = &dirEntry;
    mParentFileGroup = parentFileGroup;
    mParentReadBuffer = parentBuffer;
    mSeekPosIndicator = 0; 
    mActualPosIndicator = 0;
    mCompressedPosIndicator = 0;
    mUncompressedBufferStartPos = -1;
    if (mDirEntry->mCompressedLength > 0)
    {
        //Zlib initialisation
        mStrm.zalloc = Z_NULL;
        mStrm.zfree = Z_NULL;
        mStrm.opaque = Z_NULL;
        mStrm.avail_in = 0;
        mStrm.next_in = Z_NULL;
		Assert( 0 );
        //int InfRet = inflateInit(&mStrm);
    }
#ifdef MEMCMP_FILE_OPERATIONS
    if (!(strstr(fullFileName,"/Resource/")))
    {
        mOrdinaryFile = fopen(fullFileName,"rb");
    }
    else
    {
        mOrdinaryFile = NULL;
    }
#endif
    mParentFileGroup->IncrementOpenedCount();
}

void CFileGroupOpenedFile::FS_fclose()
{
    if (mDirEntry->mCompressedLength > 0)
    {
		Assert( 0 );
        //inflateEnd(&mStrm);
    }
    mParentFileGroup->Lock();
    mParentFileGroup->DecrementOpenedCount();
    mParentFileGroup->FreeOpenedFile(this);
#ifdef MEMCMP_FILE_OPERATIONS
    if (mOrdinaryFile)
    {
        fclose(mOrdinaryFile);
        mOrdinaryFile = NULL;
    }
#endif
    mParentFileGroup->Unlock();
}

void CFileGroupOpenedFile::FS_fseek( __int64 pos, int seekType )
{
    //mParentFileGroup->Lock();
    //CM:TODO Possibly change this so that seek isn't actually done until read????
    __int64 absPosition;
    switch (seekType)
    {
    case SEEK_CUR:
        absPosition = mSeekPosIndicator + pos;
        break;
    case SEEK_END:
        absPosition = mDirEntry->mLength + pos;
        break;
    default:
        absPosition = pos;
    }

    bool eofSeek = false; //Record eof setting at this point, but only set mEof following a successful seek
    if(absPosition > mDirEntry->mLength)
    {
        absPosition = mDirEntry->mLength;
        eofSeek = true;
        //CM:TODO Raise error condition?
    }

    //Don't actually do the seek at this point, wait until next read

    mSeekPosIndicator = absPosition;
    mEof = eofSeek;
    
#ifdef MEMCMP_FILE_OPERATIONS
    if (mOrdinaryFile)
    {
        fseek(mOrdinaryFile,pos,seekType);
    }
#endif
    //if (!isCurrent)
    //{
    //    mParentFileGroup->MakeCurrentEntry(this);
    //}
    //mParentFileGroup->Unlock();
}

long CFileGroupOpenedFile::FS_ftell()
{
    return mSeekPosIndicator; 
    //CM:TODO If an error occurs, -1L is returned, and the global variable errno is set to a positive value. 
}

int CFileGroupOpenedFile::FS_feof()
{
    return mEof; 
}

void CFileGroupOpenedFile::Rewind()
{
#ifdef TIME_FILE_OPERATIONS
    mParentFileGroup->m_rewinds++;
#endif
	Assert( 0 );
    //inflateEnd(&mStrm);
    mStrm.zalloc = Z_NULL;
    mStrm.zfree = Z_NULL;
    mStrm.opaque = Z_NULL;
    mStrm.avail_in = 0;
    mStrm.next_in = Z_NULL;
	Assert( 0 );
    //int InfRet = inflateInit(&mStrm);
    mCompressedPosIndicator = 0;
    mActualPosIndicator = 0;
    mUncompressedBufferStartPos = -1;
}

void CFileGroupOpenedFile::TraceMemCopy(void* pDest, const void* pSource, size_t nBytes)
{
#ifdef TIME_FILE_OPERATIONS
    system_time_t time1 = sys_time_get_system_time();
#endif

    memcpy(pDest,pSource,nBytes);

#ifdef TIME_FILE_OPERATIONS
    system_time_t time2 = sys_time_get_system_time();
    mParentFileGroup->m_memCopyTime += (time2-time1);
#endif

}

//ASSUMPTIONS: destination has readSize bytes available
size_t CFileGroupOpenedFile::ReadFromCompressedData( void *dest, size_t readSize)
{
    uint64_t bytesRead = 0; 
    bool dataCopied = false;
    size_t destBytesWritten = 0;
    size_t bytesToIgnore = 0;

    if (mSeekPosIndicator < mActualPosIndicator) //Check for Rewind
    {
        //First check if there is any data left over from a previous inflate
        if ((mUncompressedBufferStartPos > -1) && (mSeekPosIndicator >= mUncompressedBufferStartPos))
        {
            unsigned char* seekPosUncompressedBuffer = mUncompressedBuffer + (mSeekPosIndicator - mUncompressedBufferStartPos);
            int bytesToCopy = MIN(mActualPosIndicator - mSeekPosIndicator, readSize);
            TraceMemCopy(dest,seekPosUncompressedBuffer,bytesToCopy);
            destBytesWritten += bytesToCopy;
        }
        else
        {
            Rewind();
        }
    }
    else
    {
        bytesToIgnore = mSeekPosIndicator - mActualPosIndicator;
    }

    size_t parentCompressedPos = mParentFileGroup->GetPosOffset() + mDirEntry->mPosition + mCompressedPosIndicator;
    size_t remainingBytesToIgnore;
    size_t avail_in_before_inflate;
    size_t remainingCompressedData = mDirEntry->mCompressedLength - mCompressedPosIndicator;

    while((destBytesWritten < readSize) && (remainingCompressedData > 0))
    {
        bool compressedDataFound = false;

        if (mPreloaded)
        {
            compressedDataFound=true;
            mStrm.avail_in = remainingCompressedData;
            mStrm.next_in = (Bytef*)mParentFileGroup->GetPreloadData() + mDirEntry->mPosition + mCompressedPosIndicator;
        }
		else 
        {
            sys_lwmutex_lock(&mParentReadBuffer->mNextConsumeByteMutex,0); //Ensure that no other consumer updates the 'next consume byte' 
            if (parentCompressedPos < mParentReadBuffer->mCurrentPos)
		    {
                if (mParentFileGroup->UsingReadThread())
                {
                    size_t availablePos = 0;
			        sys_lwmutex_lock(&mParentReadBuffer->mBufferStartEndPosMutex,0); //Don't want read thread to 'wrap around' while we calculate the available data 
                    //Compressed data may exist in read thread buffer 
                    //First find the start of the section of the buffer which won't be overwritten:
                    if ((mParentReadBuffer->mBufferEndPos > 0) && (mParentReadBuffer->mBufferEndPos == mParentReadBuffer->mBufferStartPos)) //Include data from previous buffer cycle
                    {
                        if (mParentReadBuffer->mNextConsumePos > 0) //Read thread won't overwrite FILEGROUP_READ_STORE_SIZE bytes up to NextConsumeByte
                        {
                            if (mParentReadBuffer->mNextConsumePos > (mParentFileGroup->GetPosOffset() + FILEGROUP_READ_STORE_SIZE))
                            {
                                availablePos = mParentReadBuffer->mNextConsumePos - FILEGROUP_READ_STORE_SIZE;
                            }
                            else
                            {
                                availablePos = mParentFileGroup->GetPosOffset();
                            }
                        }
                        else
                        {
                            //This should only occur when read thread has filled buffer and no bytes have been read
                            Assert(mParentReadBuffer->mCurrentByte == mParentReadBuffer->mBuffer); 
                            Assert(mParentReadBuffer->mAwaitingConsumer);
                            availablePos = mParentReadBuffer->mBufferStartPos;
                        }
                    }
                    else //No data available from previous cycle
                    {
                        if (mParentReadBuffer->mNextConsumePos > 0)
                        {
                            Assert(mParentReadBuffer->mNextConsumePos <= mParentReadBuffer->mCurrentPos);
                            if ((mParentReadBuffer->mNextConsumePos - mParentReadBuffer->mBufferStartPos) > FILEGROUP_READ_STORE_SIZE)
                            {
                                availablePos = mParentReadBuffer->mNextConsumePos - FILEGROUP_READ_STORE_SIZE;
                            }
                            else
                            {
                                availablePos = mParentReadBuffer->mBufferStartPos;
                            }
                        }
                        else
                        {
                            availablePos = mParentReadBuffer->mBufferStartPos;
                        }
                    }
                    //Now check if compressed data exists within the 'safe' portion of the buffer: 
                    if (parentCompressedPos >= availablePos)
                    {
                        compressedDataFound = true;
                        if ((mParentReadBuffer->mBufferEndPos > 0) && (mParentReadBuffer->mBufferEndPos == mParentReadBuffer->mBufferStartPos) &&
                            (parentCompressedPos < mParentReadBuffer->mBufferEndPos)) //Data wraps round end of buffer 
                        {
                            mStrm.avail_in = MIN((mParentReadBuffer->mBufferEndPos - parentCompressedPos),remainingCompressedData);
                            mStrm.next_in = (mParentReadBuffer->mBuffer + mParentReadBuffer->mBufferSize) - (mParentReadBuffer->mBufferEndPos - parentCompressedPos);
                        }
                        else
                        {
                            mStrm.avail_in = MIN((mParentReadBuffer->mCurrentPos - parentCompressedPos),remainingCompressedData);
                            mStrm.next_in = mParentReadBuffer->mBuffer + (parentCompressedPos - mParentReadBuffer->mBufferStartPos);
                        }
                    }

                    sys_lwmutex_unlock(&mParentReadBuffer->mBufferStartEndPosMutex); //Happy for read thread to 'wrap around' from this point on 
                }
                else
                {
                    //If not using read thread, we don't need to worry about data being overwritten, check whether the data exists anywhere in the buffer
                    if (parentCompressedPos >= mParentReadBuffer->mBufferStartPos)
                    {
                        compressedDataFound = true;
                        mStrm.avail_in = MIN((mParentReadBuffer->mCurrentPos - parentCompressedPos),remainingCompressedData);
                        mStrm.next_in = mParentReadBuffer->mBuffer + (parentCompressedPos - mParentReadBuffer->mBufferStartPos);
                    }
                }
            }
        }

        if (!compressedDataFound)
        {
			//Read compressed data from file
    		sys_lwmutex_lock(&mParentReadBuffer->mBufferMutex,0); //Lock buffer for write
#ifdef TIME_FILE_OPERATIONS
            system_time_t time1 = sys_time_get_system_time();
#endif
            uint64_t pos;
            uint64_t alignedSeekPos = parentCompressedPos - parentCompressedPos%mParentReadBuffer->mFileBlockSize;
            CellFsErrno retFs = cellFsLseek(mParentReadBuffer->mFile,alignedSeekPos,CELL_FS_SEEK_SET,&pos);
            if(retFs == CELL_FS_SUCCEEDED)
            {
                retFs = cellFsRead(mParentReadBuffer->mFile,mParentReadBuffer->mBuffer,(uint64_t)FILEGROUP_READ_CHUNK_SIZE,&bytesRead);
            }
            while(retFs != CELL_FS_SUCCEEDED)
            {
                cellFsClose(mParentReadBuffer->mFile);
                sys_timer_usleep(250000); //Sleep for 0.25 seconds
                mParentReadBuffer->WaitOnDiskEjectRecovery();
                retFs = cellFsLseek(mParentReadBuffer->mFile,(int64_t)alignedSeekPos,CELL_FS_SEEK_SET,&pos);
                if(retFs == CELL_FS_SUCCEEDED)
                {
                    retFs = cellFsRead(mParentReadBuffer->mFile,mParentReadBuffer->mBuffer,(uint64_t)FILEGROUP_READ_CHUNK_SIZE,&bytesRead);
                }
                if(retFs == CELL_FS_EIO)
                {
                    PS3_DisplayDiskErrorDialog(); //Assume dirty disk, rather than disk eject if this point is reached
                }
                else if(retFs == CELL_FS_SUCCEEDED)
                {
                    PS3_ClearDiskErrorDialog();
                }
            }
            if (bytesRead > 0)
            {
                //Reset buffer data so that read thread continues from this point
                mParentReadBuffer->mBufferEndPos = 0;
                mParentReadBuffer->mBufferStartPos = alignedSeekPos;
                mParentReadBuffer->mCurrentByte = mParentReadBuffer->mBuffer + (int)bytesRead;
                mParentReadBuffer->mCurrentPos = alignedSeekPos + (int)bytesRead;
				mParentReadBuffer->mNextConsumePos = 0;
				if (mParentFileGroup->UsingReadThread() &&
					mParentReadBuffer->mAwaitingConsumer &&
					mParentReadBuffer->mCurrentPos < mParentFileGroup->Size())
				{
					int ret = sys_cond_signal_to(mParentReadBuffer->mReadThreadSignal,mParentFileGroup->GetReadThread());
				}

            }
#ifdef TIME_FILE_OPERATIONS
            system_time_t time2 = sys_time_get_system_time();
            mParentFileGroup->IncrReadTime(time2-time1);
            mParentFileGroup->m_bytesReadFromFileByConsumer += (int)bytesRead;
            mParentFileGroup->IncrReadBytes(bytesRead);
#endif
            mStrm.avail_in = MIN((int)(bytesRead - (parentCompressedPos-alignedSeekPos)),remainingCompressedData);
            mStrm.next_in = mParentReadBuffer->mBuffer+(parentCompressedPos-alignedSeekPos);
			sys_lwmutex_unlock(&mParentReadBuffer->mBufferMutex); //Now safe for other threads to continue updating buffer 
        }
        //mParentReadBuffer->mNextConsumeByte = mStrm.next_in; //This ensures bytes aren't overwritten during inflate SHOULD ONLY EVER INCREASE NEXT CONSUME BYTE
        //Assert(((mParentReadBuffer->mNextConsumeByte - mParentReadBuffer->mBuffer) < FILEGROUP_READ_THREAD_BUFFER_SIZE));

//        mStrm.avail_out = FILEGROUP_UNCOMPRESSED_BUFFER_SIZE;
//        bool directInflate = ((bytesToIgnore <= 0) && (readSize - destBytesWritten) >= FILEGROUP_UNCOMPRESSED_BUFFER_SIZE);
//        if (directInflate)
//        {
//            mStrm.next_out = (unsigned char*)dest + destBytesWritten;
//        }
//        else
//        {
//            mStrm.next_out = mUncompressedBuffer;
//        }

//		while((mStrm.avail_in > 0)&&(destBytesWritten < readSize)) 
//		{

#ifdef TIME_FILE_OPERATIONS
			system_time_t time1 = sys_time_get_system_time();
#endif

			mStrm.avail_out = FILEGROUP_UNCOMPRESSED_BUFFER_SIZE;
			mStrm.next_out = mUncompressedBuffer;
	        avail_in_before_inflate = mStrm.avail_in;
			Assert( 0 );
		    int ret = 0;//inflate(&mStrm, Z_NO_FLUSH);

			Assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			switch (ret) 
			{case Z_NEED_DICT:
			ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				//(void)inflateEnd(&mStrm);
				Assert(0);
                printf("Error %d decompressing data, returning %d\n", ret, destBytesWritten);
				return destBytesWritten;
			}

#ifdef TIME_FILE_OPERATIONS
			system_time_t time2 = sys_time_get_system_time();
			mParentFileGroup->m_uncompressCalls++;
			mParentFileGroup->m_bytesReadFromBuffer += (avail_in_before_inflate - mStrm.avail_in);
			mParentFileGroup->m_uncompressTime += (time2-time1);
#endif

			int bytesInflated = FILEGROUP_UNCOMPRESSED_BUFFER_SIZE - mStrm.avail_out;
			//        if (directInflate)
			//        {
			mUncompressedBufferStartPos = mActualPosIndicator;
			//        }
			//        else
			//        {
			//            mUncompressedBufferStartPos = -1;
			//        }
			mActualPosIndicator += bytesInflated;

			if (bytesToIgnore > 0)
			{
				if (bytesInflated > bytesToIgnore)
				{
					int bytesOfInterest = MIN(readSize,(bytesInflated - bytesToIgnore));
					//                if (!directInflate)
					//                {
					TraceMemCopy(dest, mUncompressedBuffer + bytesToIgnore, bytesOfInterest);
					//                }
					destBytesWritten += bytesOfInterest;
					bytesToIgnore = 0;
				}
				else
				{
					bytesToIgnore -= bytesInflated;
				}
			}
			else
			{
				int bytesOfInterest = MIN((readSize-destBytesWritten),bytesInflated);
				//            if (!directInflate)
				//            {
				TraceMemCopy((unsigned char*)dest + destBytesWritten, mUncompressedBuffer, bytesOfInterest);
				//            }
				destBytesWritten += bytesOfInterest;
			}

			mCompressedPosIndicator += (avail_in_before_inflate - mStrm.avail_in);
			parentCompressedPos += (avail_in_before_inflate - mStrm.avail_in);
			remainingCompressedData -= (avail_in_before_inflate - mStrm.avail_in);
			Assert(mStrm.next_in); 

	    //		} //while inflate loop

		//Wait until this point before updating NextConsumePos, otherwise data may be overwritten during inflate.
		//Only update "NextConsumePos" if it has increased, otherwise it is possible to get into a situation  
		// where some of the FILEGROUP_READ_STORE_SIZE bytes leading up to NextConsumePos are overwritten by the read thread.
		if ((!mPreloaded) && (parentCompressedPos > mParentReadBuffer->mNextConsumePos))
		{
			mParentReadBuffer->mNextConsumePos = parentCompressedPos;

			//Check if need to signal to read thread
			if (mParentFileGroup->UsingReadThread() &&
				mParentReadBuffer->mAwaitingConsumer &&
				mParentReadBuffer->mCurrentPos < mParentFileGroup->Size() &&
				((mParentReadBuffer->mCurrentPos + FILEGROUP_READ_CHUNK_SIZE) - mParentReadBuffer->mNextConsumePos) <= (mParentReadBuffer->mBufferSize - FILEGROUP_READ_SIGNAL_SIZE)) 
			{
				int ret = sys_cond_signal_to(mParentReadBuffer->mReadThreadSignal,mParentFileGroup->GetReadThread());
			}
		}

        if (!mPreloaded)
        {
            sys_lwmutex_unlock(&mParentReadBuffer->mNextConsumeByteMutex); //Release consumer lock
        }
    } //while check available data loop

    return destBytesWritten;
}

size_t CFileGroupOpenedFile::ReadFromUncompressedData( void *dest, size_t readSize)
{
    uint64_t bytesRead = 0; 
    bool dataCopied = false;
    size_t dataSeekPos = mDirEntry->mPosition + mSeekPosIndicator;
    size_t parentSeekPos = mParentFileGroup->GetPosOffset() + dataSeekPos;
    size_t destBytesWritten = 0;
    int ret;

    if(mPreloaded)
    {
        TraceMemCopy((unsigned char*)dest, (Bytef*)mParentFileGroup->GetPreloadData() + dataSeekPos, readSize);
        destBytesWritten = readSize;
        mSeekPosIndicator += readSize;
    }
    else
    {
            sys_lwmutex_lock(&mParentReadBuffer->mNextConsumeByteMutex,0); //Ensure that no other consumer updates the 'next consume byte' 

        size_t dataAvailable = 0;
        void* dataLocation = NULL; 
        size_t remainingData = mDirEntry->mLength - mSeekPosIndicator;

        while(destBytesWritten < readSize)
        {
            bool dataFound = false;

            if (parentSeekPos < mParentReadBuffer->mCurrentPos && remainingData > 0)
            {
                if (mParentFileGroup->UsingReadThread())
                {
                    size_t availablePos = 0;
                    sys_lwmutex_lock(&mParentReadBuffer->mBufferStartEndPosMutex,0); //Don't want read thread to 'wrap around' while we calculate the available data 
                    //Data may exist in read thread buffer 
                    //First find the start of the section of the buffer which won't be overwritten:
                    if ((mParentReadBuffer->mBufferEndPos > 0) && (mParentReadBuffer->mBufferEndPos == mParentReadBuffer->mBufferStartPos)) //Include data from previous buffer cycle
                    {
                        if (mParentReadBuffer->mNextConsumePos > 0) //Read thread won't overwrite FILEGROUP_READ_STORE_SIZE bytes up to NextConsumeByte
                        {
                            if (mParentReadBuffer->mNextConsumePos > (mParentFileGroup->GetPosOffset() + FILEGROUP_READ_STORE_SIZE))
                            {
                                availablePos = mParentReadBuffer->mNextConsumePos - FILEGROUP_READ_STORE_SIZE;
                            }
                            else
                            {
                                availablePos = mParentFileGroup->GetPosOffset();
                            }
                        }
                        else
                        {
                            //This should only occur when read thread has filled buffer and no bytes have been read
                            Assert(mParentReadBuffer->mCurrentByte == mParentReadBuffer->mBuffer); 
                            Assert(mParentReadBuffer->mAwaitingConsumer);
                            availablePos = mParentReadBuffer->mBufferStartPos;
                        }
                    }
                    else //No data available from previous cycle
                    {
                        if (mParentReadBuffer->mNextConsumePos > 0)
                        {
                            Assert(mParentReadBuffer->mNextConsumePos <= mParentReadBuffer->mCurrentPos);
                            if ((mParentReadBuffer->mNextConsumePos - mParentReadBuffer->mBufferStartPos) > FILEGROUP_READ_STORE_SIZE)
                            {
                                availablePos = mParentReadBuffer->mNextConsumePos - FILEGROUP_READ_STORE_SIZE;
                            }
                            else
                            {
                                availablePos = mParentReadBuffer->mBufferStartPos;
                            }
                        }
                        else
                        {
                            availablePos = mParentReadBuffer->mBufferStartPos;
                        }
                    }
                    //Now check if compressed data exists within the 'safe' portion of the buffer: 
                    if (parentSeekPos >= availablePos)
                    {
                        dataFound = true;
                        if ((mParentReadBuffer->mBufferEndPos > 0) && (mParentReadBuffer->mBufferEndPos == mParentReadBuffer->mBufferStartPos) &&
                            (parentSeekPos < mParentReadBuffer->mBufferEndPos)) //Data wraps round end of buffer 
                        {
                            dataAvailable = MIN((mParentReadBuffer->mBufferEndPos - parentSeekPos),remainingData);
                            dataLocation = (mParentReadBuffer->mBuffer + mParentReadBuffer->mBufferSize) - (mParentReadBuffer->mBufferEndPos - parentSeekPos);
                        }
                        else
                        {
                            dataAvailable = MIN((mParentReadBuffer->mCurrentPos - parentSeekPos),remainingData);
                            dataLocation = mParentReadBuffer->mBuffer + (parentSeekPos - mParentReadBuffer->mBufferStartPos);
                        }
                    }

                    sys_lwmutex_unlock(&mParentReadBuffer->mBufferStartEndPosMutex); //Happy for read thread to 'wrap around' from this point on 
                }
                else
                {
                    //Don't need to worry about read thread overwriting the buffer, check whether the data exists anywhere in the buffer
                    if (parentSeekPos >= mParentReadBuffer->mBufferStartPos)
                     {
                        dataFound = true;
                        dataAvailable = MIN((mParentReadBuffer->mCurrentPos - parentSeekPos),remainingData);
                        dataLocation = mParentReadBuffer->mBuffer + (parentSeekPos - mParentReadBuffer->mBufferStartPos);
                     }
                }
            }

            if (!dataFound)
            {
                //Read data from file
                sys_lwmutex_lock(&mParentReadBuffer->mBufferMutex,0); //Lock buffer for write
#ifdef TIME_FILE_OPERATIONS
                system_time_t time1 = sys_time_get_system_time();
#endif
                uint64_t pos;
                uint64_t alignedSeekPos = parentSeekPos - parentSeekPos%mParentReadBuffer->mFileBlockSize;
                uint64_t alignedReadSize = FILEGROUP_READ_CHUNK_SIZE;
                if(mParentReadBuffer->mReadChunkSize>FILEGROUP_READ_CHUNK_SIZE) 
                {
                    uint64_t alignedRemainingData = (parentSeekPos + remainingData) - alignedSeekPos; //Locate the remaining data from the alignedseekpos up to the end of the file
                    if(alignedRemainingData > FILEGROUP_READ_CHUNK_SIZE)
                    {
                        alignedReadSize = MIN(mParentReadBuffer->mReadChunkSize,((((parentSeekPos + remainingData) - alignedSeekPos)/FILEGROUP_READ_CHUNK_SIZE)+1)*FILEGROUP_READ_CHUNK_SIZE);
                    }
                }
                CellFsErrno retFs = cellFsLseek(mParentReadBuffer->mFile,(int64_t)alignedSeekPos,CELL_FS_SEEK_SET,&pos);
                if (retFs == CELL_FS_SUCCEEDED)
                {
                    //snStopMarker(3);
                    //char tempText[100];
                    //snprintf(tempText, 100, "cellfsread aligned pos %d, seek pos %d, read size %d", alignedSeekPos, parentSeekPos, alignedReadSize);
                    //snStartMarker((unsigned int)mParentReadBuffer->mFile, tempText);
                    retFs = cellFsRead(mParentReadBuffer->mFile,mParentReadBuffer->mBuffer,alignedReadSize,&bytesRead);
                    //snStopMarker((unsigned int)mParentReadBuffer->mFile);
                    //snStartMarker(3, "SyncRead continue");
                    //DebugPrint("ReadFromUncompressedData: pos %d\n", parentSeekPos);
                }
                while(retFs != CELL_FS_SUCCEEDED)
                {
                    cellFsClose(mParentReadBuffer->mFile);
                    sys_timer_usleep(250000); //Sleep for 0.25 seconds
                    mParentReadBuffer->WaitOnDiskEjectRecovery();
                    retFs = cellFsLseek(mParentReadBuffer->mFile,(int64_t)alignedSeekPos,CELL_FS_SEEK_SET,&pos);
                    if (retFs == CELL_FS_SUCCEEDED)
                    {
                        retFs = cellFsRead(mParentReadBuffer->mFile,mParentReadBuffer->mBuffer,alignedReadSize,&bytesRead);
                    }
                    if(retFs == CELL_FS_EIO)
                    {
                        PS3_DisplayDiskErrorDialog();
                    }
                    else if(retFs == CELL_FS_SUCCEEDED)
                    {
                        PS3_ClearDiskErrorDialog();
                    }
                }
                if (bytesRead > 0)
                {
                    //Reset buffer data so that read thread continues from this point
                    mParentReadBuffer->mBufferEndPos = 0;
                    mParentReadBuffer->mBufferStartPos = alignedSeekPos;
                    mParentReadBuffer->mCurrentByte = mParentReadBuffer->mBuffer + (int)bytesRead;
                    mParentReadBuffer->mCurrentPos = alignedSeekPos + (int)bytesRead;
                    mParentReadBuffer->mNextConsumePos = 0;
                    if (mParentFileGroup->UsingReadThread() &&
                        mParentReadBuffer->mAwaitingConsumer &&
                        mParentReadBuffer->mCurrentPos < mParentFileGroup->Size())
                    {
                        int ret = sys_cond_signal_to(mParentReadBuffer->mReadThreadSignal,mParentFileGroup->GetReadThread());
                    }

                }
#ifdef TIME_FILE_OPERATIONS
                system_time_t time2 = sys_time_get_system_time();
                mParentFileGroup->IncrReadTime(time2-time1);
                mParentFileGroup->m_bytesReadFromFileByConsumer += (int)bytesRead;
                mParentFileGroup->IncrReadBytes((int)bytesRead);
#endif
                dataAvailable = MIN((int)(bytesRead-(parentSeekPos-alignedSeekPos)),remainingData);
                dataLocation = mParentReadBuffer->mBuffer + (parentSeekPos-alignedSeekPos);
                sys_lwmutex_unlock(&mParentReadBuffer->mBufferMutex); //Now safe for other threads to continue updating buffer 
            }

            int bytesOfInterest = MIN((readSize-destBytesWritten),dataAvailable);
            //snStartMarker(1, "MemCpy");
            TraceMemCopy((unsigned char*)dest + destBytesWritten, dataLocation, bytesOfInterest);
            //snStopMarker(1);
            destBytesWritten += bytesOfInterest;
            mSeekPosIndicator += bytesOfInterest;
            parentSeekPos += bytesOfInterest;
            remainingData = mDirEntry->mLength - mSeekPosIndicator;

#ifdef TIME_FILE_OPERATIONS
            mParentFileGroup->m_bytesReadFromBuffer += bytesOfInterest;
#endif

            //Wait until this point before updating NextConsumePos, otherwise data may be overwritten during inflate.
            //Only update "NextConsumePos" if it has increased, otherwise it is possible to get into a situation  
            // where some of the FILEGROUP_READ_STORE_SIZE bytes leading up to NextConsumePos are overwritten by the read thread.
            if (parentSeekPos > mParentReadBuffer->mNextConsumePos)
            {
                mParentReadBuffer->mNextConsumePos = parentSeekPos;

                //Check if need to signal to read thread
                if (mParentFileGroup->UsingReadThread() &&
                    mParentReadBuffer->mAwaitingConsumer &&
                    mParentReadBuffer->mCurrentPos < mParentFileGroup->Size() &&
                    ((mParentReadBuffer->mCurrentPos + FILEGROUP_READ_CHUNK_SIZE) - mParentReadBuffer->mNextConsumePos) <= (mParentReadBuffer->mBufferSize - FILEGROUP_READ_SIGNAL_SIZE)) 
                {
                    int ret = sys_cond_signal_to(mParentReadBuffer->mReadThreadSignal,mParentFileGroup->GetReadThread());
                }
            }

        } //while check available data loop
            sys_lwmutex_unlock(&mParentReadBuffer->mNextConsumeByteMutex); //Release consumer lock
        }

    return destBytesWritten;
}

size_t CFileGroupOpenedFile::FS_fread( void *dest, size_t destSize, size_t size)
{
	MEM_ALLOC_CREDIT();
//    Assert(V_stristr(mDirEntry.mName,"bars001c") == NULL);
//    mParentFileGroup->Lock();

//    int readSize = MIN(destSize,size);//Ensure that we don't read more than destSize
    if (mParentFileGroup->HasBeenDeleted())
    {
        printf("ERROR: Attempting to read from file after filegroup has been deleted\n");
    }
    int readSize = size; //Need to ignore destsize to ensure that we get the same behaviour from filegroups that we get from the ordinary filesystem 

    bool eofRead = false; //Record eof setting at this point, but only set mEof following a successful read
    if(readSize > (mDirEntry->mLength - mSeekPosIndicator))
    {
        readSize = (mDirEntry->mLength - mSeekPosIndicator);
        eofRead = true;
        //CM:TODO Raise error condition?
    }

    size_t bytesRead;
    if (mDirEntry->mCompressedLength > 0)
    {
        bytesRead = ReadFromCompressedData(dest,readSize);
        mSeekPosIndicator = mSeekPosIndicator + bytesRead;
    }
    else
    {
        bytesRead = ReadFromUncompressedData(dest,readSize);
        mActualPosIndicator = mSeekPosIndicator + bytesRead;
    }

    //CM:TODO Error handling
    //CM:TODO If this number differs from the bytes requested, set ferror and feof accordingly.
    //otherwise...
    //mActualPosIndicator = mSeekPosIndicator + bytesRead;
    mEof = eofRead;
    //if (!mParentFileGroup->IsCurrentFile(this))
    //{
    //    mParentFileGroup->MakeCurrentFile(this);
    //}
    //mParentFileGroup->Unlock();

#ifdef MEMCMP_FILE_OPERATIONS
    if (mOrdinaryFile)
    {
        unsigned char* tmpDest = new unsigned char[size];
        fread(tmpDest,1,size,mOrdinaryFile);
        Assert((memcmp(tmpDest,dest,size)==0));
        delete[] tmpDest;
    }
#endif

#ifdef TIME_FILE_OPERATIONS
    mParentFileGroup->m_fsReads++;
#endif
    return bytesRead;
}

char * CFileGroupOpenedFile::FS_fgets( char *dest, int destSize )
{
    //mParentFileGroup->Lock();
   // char* retVal = NULL;

   // int destLimit = MIN(destSize,((mDirEntry->mLength - mSeekPosIndicator) + 1));//Ensure that we don't read past the end of the file
   // mEof = (destLimit<= 1);
   // //fgets reads characters until (destLimit-1) characters have been read or either a newline or End-of-File is reached, whichever comes first.
   // if (mEof)
   // {
   //     //CM:TODO Set error conditions
   // }
   // else
   // {
   //     if (mDirEntry->mCompressedLength > 0)
   //     {
   //         retVal = FgetsFromCompressedData(dest,destLimit);
   //     }
   //     else
   //     {
   //         retVal = FgetsFromUncompressedData(dest,destLimit);
   //     }
   // }
   // if (retVal)
   // {
   //     mActualPosIndicator+=strlen(retVal);
   //     mSeekPosIndicator = mActualPosIndicator;
   //     //if (!mParentFileGroup->IsCurrentFile(this))
   //     //{
   //     //    mParentFileGroup->MakeCurrentFile(this);
   //     //}
   // }
   // //CM:TODO Error handlng, do ferror and feof need to be set?????.
   ////mParentFileGroup->Unlock();
   //mParentFileGroup->m_fgets++;
   Assert(0); //fgets not implemented
   return NULL;
}

#endif
