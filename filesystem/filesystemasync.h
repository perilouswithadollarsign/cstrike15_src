//===== Copyright © 2009, Valve Corporation, All rights reserved. ======//
// filesyste_newasync.h
//
// Purpose: 
//
// $NoKeywords: $async file system
//===========================================================================//

#include <limits.h>

#include "tier0/threadtools.h"
#include "tier0/memalloc.h"
#include "tier1/interface.h"
#include "tier1/utlsymbol.h"
#include "appframework/iappsystem.h"
#include "tier1/checksum_crc.h"
#include "filesystem/iasyncfilesystem.h"

#ifndef FILESYSTEMASYNC_H
#define FILESYSTEMASYNC_H

#ifdef _WIN32
	#pragma once
#endif

#ifdef OSX
#pragma GCC diagnostic ignored "-Wreturn-type"			// control reaches end of non-void function, for unsupported assignment operators below
#endif

class CAsyncGroupRequest;


struct CAsyncResultInfo_t
{
	public:
		void*				m_pAllocatedBuffer;
		int					m_nBytesTransferred;
		FSAsyncStatus_t		m_ErrorCode;
		
		CAsyncResultInfo_t() : m_pAllocatedBuffer(NULL), m_nBytesTransferred(0) {}
};


//-----------------------------------------------------------------------------
// CAsyncRequestBase - Common items to all async filesystem requests
//
// Allows the async filesystem to have a standard way to deal with different
// types of requests equally re: priority, callbacks, state, aborting, etc
//	
//-----------------------------------------------------------------------------
class CAsyncRequestBase
{

	public:
	
		void*							m_pOuter;					// pointer to object containing this base
	
		// -------------------------------------------------------
		// Members to set by the user before submission

		CFunctor*						m_pCallback;				// Optional Callback object (we own this)
		CIOCompletionQueue*				m_pResultQueue;				// Message Queue object to send results to
		
		AsyncFileOperation_t			m_Operation;				// operation to perform
		int32							m_priority;					// inter list priority, 0=lowest??

		// -------------------------------------------------------
		// Used internally by the filesystem to keep track of the request
		
		CAsyncRequestBase*				m_pNext;					// used to maintain the priority linked list
		CAsyncRequestBase*				m_pPrev;	

		// -------------------------------------------------------
		// Members which get set by the file system after submission

		AsyncRequestState_t				m_RequestState;				// State of this request (which stage it is n)
		AsyncRequestStatus_t			m_RequestStatus;			// Error/Success Status from the last operation attempted on this request
		
		bool							m_bAbortRequest;			// Flag indicating this request is to be aborted
		bool							m_bProcessingCallback;		// Flag indicating a callback is being processed
		bool							m_bDontAutoRelease;			// Flag indicating not to automatically release the request once the callback finishes
//		bool							m_bIsAContainer
		
		CThreadEvent*					m_pSyncThreadEvent;			// Thread event to signal in synchronous mode...
		
		FSAsyncControl_t				m_pOldAsyncControl;			// old Async System control Handle
		FSAsyncStatus_t					m_pOldAsyncStatus;			// old Async System IO Status
		
		CAsyncGroupRequest*				m_pGroup;					// group this request belongs to


	public:
										CAsyncRequestBase();		// Default Constructor - We want NO implicit constructions
										~CAsyncRequestBase();		// Destructor - not virtual anymore

		void							SetOuter( void* pOuter )			{ m_pOuter = pOuter; }
		void*							GetOuter()							{ return m_pOuter; }

		IAsyncRequestBase*				GetInterfaceBase();

		// functions to query operation
		AsyncFileOperation_t			GetAsyncOperationType()				{ return m_Operation; }

		// Set completion options								
		void							AssignCallback( CFunctor* pCallback );					// Add a completion callback to this request
		void							AssignResultQueue( CIOCompletionQueue* pMsgQueue );		// Send the results to this queue object
		void							AssignCallbackAndQueue( CIOCompletionQueue* pMsgQueue, CFunctor* pCallback );

		// Completion processing functions		
		void							ProcessCallback( bool bRelease = true );	// Execute the optional callback functor

		void							KeepRequestPostCallback()			{ m_bDontAutoRelease = true; }		// User wants to keep request alive after the callback completes
		void							DontKeepRequestPostCallback()		{ m_bDontAutoRelease = false; }		// User doesn't want to keep request after the callback completes 

		void							Release();							// lets user manually release the async request (only valid when completed)
		void							DeleteOuter();						// Used by async filesystem to delete the containing object (as well as the CAsyncRequestBase)

		// Priority Functions
		void							SetPriority( int32 nPriority );
		int32							GetPriority()						{ return m_priority; }

		// Status & Results functions
		AsyncRequestState_t				GetRequestState()					{ return m_RequestState; }
		AsyncRequestStatus_t			GetRequestStatus()					{ return m_RequestStatus; }
		
		// --------------------------------------------------------------
		// Gateway to outer's Public Methods not included in the interface, used by Async FileSystem
		
		void							AbortAfterServicing( CAsyncResultInfo_t& results );								// Handle a request that got aborted while being serviced
		void							UpdateAfterServicing( CAsyncResultInfo_t& results );							// Handle a request that got aborted while being serviced
		AsyncRequestStatus_t			ValidateSubmittedRequest( bool bPerformSync );									// Validates the derived object data at request submission
		bool							ValidateRequestModification();													// core check to determine if modification is allowed at this time
		
	protected:
		// Helper functions for repeated operations
		void							RefreshCallback( CFunctor* pCallback );

	private:

		explicit						CAsyncRequestBase( const CAsyncRequestBase& rhs );	// copy constructor - NOT SUPPORTED
		CAsyncRequestBase&				operator=( const CAsyncRequestBase& rhs );				// assignment operator - NOT SUPPORTED	
									

};




//-----------------------------------------------------------------------------
// CAsyncGroupRequest - Concrete implementation for a group of async requests
//
// 
//	
//-----------------------------------------------------------------------------
class CAsyncGroupRequest : public IAsyncGroupRequest
{
	public:
	
		CAsyncRequestBase				m_Base;							// Base request - we can't derive from due to multiple inheritance problems
		
		CUtlVector<IAsyncRequestBase*>	m_RequestList;					// List of requests to process
		CUtlVector<bool>				m_ValidList;					// List of requests which are valid
		
		CInterlockedInt					m_nNumRequestsOutstanding;
		


	public:
	
										CAsyncGroupRequest();			// Default Constructor - We want NO implicit constructions
		virtual							~CAsyncGroupRequest();			// Destructor

		// ---------------------------------------------------------
		// methods from the IAsyncRequestBase Interface
		// ---------------------------------------------------------

		// functions to query operation
		virtual AsyncFileOperation_t	GetAsyncOperationType()				{ return m_Base.GetAsyncOperationType(); }

		// Set completion options								
		virtual void					AssignCallback( CFunctor* pCallback )				{  m_Base.AssignCallback( pCallback ); }
		virtual void					AssignResultQueue( CIOCompletionQueue* pMsgQueue )	{  m_Base.AssignResultQueue( pMsgQueue ); }
		virtual void					AssignCallbackAndQueue( CIOCompletionQueue* pMsgQueue, CFunctor* pCallback )  {  m_Base.AssignCallbackAndQueue( pMsgQueue, pCallback); }

		// Completion processing functions		
		virtual void					ProcessCallback( bool bRelease = true )	{  m_Base.ProcessCallback( bRelease ); }

		virtual void					KeepRequestPostCallback()			{  m_Base.KeepRequestPostCallback(); }
		virtual void					DontKeepRequestPostCallback()		{  m_Base.DontKeepRequestPostCallback(); }

		virtual void					Release()							{  m_Base.Release(); }

		// Priority Functions
		virtual void					SetPriority( int32 nPriority )		{  m_Base.SetPriority( nPriority ); }
		virtual int32					GetPriority()						{ return  m_Base.GetPriority(); }

		// Status & Results functions
		virtual AsyncRequestState_t		GetRequestState()					{ return  m_Base.GetRequestState(); }
		virtual AsyncRequestStatus_t	GetRequestStatus()					{ return  m_Base.GetRequestStatus(); }

	private:
		virtual CAsyncRequestBase*		GetBase()							{ return (CAsyncRequestBase*) &m_Base; }

		// ----------------------------------------------------
		// methods from the IAsyncGroupRequest Interface
		// ----------------------------------------------------

	public:	

		virtual void					AddAsyncRequest( IAsyncRequestBase* pRequest );
		
		virtual int32					GetAsyncRequestCount()			{ return m_RequestList.Count(); }
		virtual IAsyncRequestBase*		GetAsyncRequest( int32 nRNum );
		virtual IAsyncFileRequest*		GetAsyncFileRequest( int32 nRNum );
		virtual IAsyncSearchRequest*	GetAsyncSearchRequest( int32 nRNum );
		
		virtual void					ReleaseAsyncRequest( int32 nRNum );

		void							NotifyOfCompletion(  IAsyncRequestBase* pRequest );

	private:

		explicit						CAsyncGroupRequest( const CAsyncRequestBase& rhs );		// copy constructor - NOT SUPPORTED
		CAsyncRequestBase&				operator=( const CAsyncGroupRequest& rhs );				// assignment operator - NOT SUPPORTED	

};













//-----------------------------------------------------------------------------
// CAsyncFileRequest - Concrete implementation for the IAsyncFileRequest interface
//
//   Manages a read or write to file request
//	
//-----------------------------------------------------------------------------
class CAsyncFileRequest : public IAsyncFileRequest
{
	
	public:
	
		CAsyncRequestBase				m_Base;							// Base request - we can't derive from due to multiple inheritance problems
	
		// -------------------------------------------------------
		// Members to set by the user before submission
		
		const char*						m_pFileName;					// file system name (copy of string that we own)
		
		void*							m_pUserProvidedDataBuffer;		// optional (if read op), system will alloc one if NULL/free if NULL
		size_t							m_nUserProvidedBufferSize;		// size of the memory buffer			
		
		int64							m_nFileSeekOffset;				// (optional) position in the file to seek to/begin xfer at, 0=beginning
		int64							m_nMaxIOSizeInBytes;			// optional read clamp, 0=full read

		// -------------------------------------------------------
		// Members which get set by the file system after submission	

		void*							m_pResultsBuffer;				// Buffer results were placed in, could be from user (or not)
		size_t							m_nResultsBufferSize;			// size of results buffer
		size_t							m_nIOActualSize;				// actual size of IO performed
		
		bool							m_bDeleteBufferMemory;			// Flag indicating the memory holding the file data is deleted when the request is released
		
	public:
									
										CAsyncFileRequest();			// Default Constructor - We want NO implicit constructions
		virtual							~CAsyncFileRequest();			// Destructor

		// ---------------------------------------------------------
		// methods from the IAsyncRequestBase Interface
		// ---------------------------------------------------------

		// functions to query operation
		virtual AsyncFileOperation_t	GetAsyncOperationType()				{ return m_Base.GetAsyncOperationType(); }

		// Set completion options								
		virtual void					AssignCallback( CFunctor* pCallback )				{  m_Base.AssignCallback( pCallback ); }
		virtual void					AssignResultQueue( CIOCompletionQueue* pMsgQueue )	{  m_Base.AssignResultQueue( pMsgQueue ); }
		virtual void					AssignCallbackAndQueue( CIOCompletionQueue* pMsgQueue, CFunctor* pCallback )  {  m_Base.AssignCallbackAndQueue( pMsgQueue, pCallback); }

		// Completion processing functions		
		virtual void					ProcessCallback( bool bRelease = true )	{  m_Base.ProcessCallback( bRelease ); }

		virtual void					KeepRequestPostCallback()			{  m_Base.KeepRequestPostCallback(); }
		virtual void					DontKeepRequestPostCallback()		{  m_Base.DontKeepRequestPostCallback(); }

		virtual void					Release()							{  m_Base.Release(); }

		// Priority Functions
		virtual void					SetPriority( int32 nPriority )		{  m_Base.SetPriority( nPriority ); }
		virtual int32					GetPriority()						{ return  m_Base.GetPriority(); }

		// Status & Results functions
		virtual AsyncRequestState_t		GetRequestState()					{ return  m_Base.GetRequestState(); }
		virtual AsyncRequestStatus_t	GetRequestStatus()					{ return  m_Base.GetRequestStatus(); }

	private:
		virtual CAsyncRequestBase*		GetBase()							{ return (CAsyncRequestBase*) &m_Base; }

		// ----------------------------------------------------
		// methods from the IAsyncFileRequest Interface
		// ----------------------------------------------------

	public:	
		// functions to set filename and operation
		virtual void					LoadFile( const char* pFileName );									// make this a 'read data from file' request
		virtual void					SaveFile( const char* pFileName );									// make this a 'write data to file' request
		virtual void					AppendFile( const char* pFileName );								// make this a 'append data to file' request
																											
		virtual void					SetFileName( const char* pFileName );								// assign the filename to use
		virtual const char*				GetFileName()				{ return m_pFileName; }					// get the filename we've assigned
		
		// Buffer control functions		
		virtual void					SetUserBuffer( void* pDataBuffer, size_t nBufferSize );				// User supplies a memory buffer to use, which they own the memory for
		virtual void*					GetUserBuffer()				{ return m_pUserProvidedDataBuffer; }	// returns the address of the user supplied buffer
		virtual size_t					GetUserBufferSize()			{ return m_nUserProvidedBufferSize; }	// returns the size of the user supplied buffer

		virtual void					ProvideDataBuffer();												// file system provide a buffer with the results
		
		// returned buffer (read) functions		
		virtual void*					GetResultBuffer()			{ return m_pResultsBuffer; }			// Returns the address of the data transferred
		virtual size_t					GetResultBufferSize()		{ return m_nResultsBufferSize; }		// Returns the size of the buffer holding the data transferred
		virtual size_t					GetIOTransferredSize()		{ return m_nIOActualSize; }				// Returns the number of bytes of data actually transferred

		// Memory control functions for buffers allocated by the async file system
		virtual void					KeepResultBuffer()			{ m_bDeleteBufferMemory = false; }		// User wants to keeps buffer allocated by the file system
		virtual void					ReleaseResultBuffer()		{ m_bDeleteBufferMemory = true; }		// User decides they want the request to take care of releasing the buffer

		// file position functions										
		virtual void					ReadFileDataAt( int64 nOffset, size_t nReadSize = 0 );				// Read file data starting at supplied offset, optional max size to read
		virtual void					WriteFileDataAt( int64 nOffset, size_t nWriteSize = 0 );			// Write data to file at supplied offset, optional size to write (max size of buffer)
	

		// --------------------------------------------------------------
		// Public Methods not included in the interface, used by Async FileSystem
		
		void							AbortAfterServicing( CAsyncResultInfo_t& results );								// Handle a request that got aborted while being serviced
		void							UpdateAfterServicing( CAsyncResultInfo_t& results );							// Handle a request that got aborted while being serviced
		AsyncRequestStatus_t			ValidateSubmittedRequest( bool bPerformSync );									// Validates the derived object data at request submission
		bool							ValidateRequestModification();													// Check to determine if modification is allowed at this time
	
	protected:
		// Helper functions for repeated operations
		void							RefreshFileName( const char* pNewFileName );

	private:
		// disabled functions
		explicit						CAsyncFileRequest( const CAsyncFileRequest& rhs );				// copy constructor - NOT SUPPORTED
		CAsyncFileRequest&				operator=( const CAsyncFileRequest& rhs );						// assignment operator - NOT SUPPORTED	
									
};



//-----------------------------------------------------------------------------
// CAsyncSearchRequest - Implementation of the Interface for setting and reading 
//                       the results of an asynchronous directory search request
//	
//-----------------------------------------------------------------------------
class CAsyncSearchRequest  :  public IAsyncSearchRequest
{

	public:
		CAsyncRequestBase					m_Base;						// Base request - we can't derive from due to multiple inheritance problems
	
//		CUtlStringList						m_DirectoryList;			// List of directories we have scanned
		
		int32								m_nNumResults;				// number of results 
		bool								m_bRecurseSubdirs;			// flag to activate recursing of subdirectories
		
		CUtlVector<CDirectoryEntryInfo_t>	m_Results;					// Search results...

		char								m_FileSpec[MAX_PATH];		// filespec of the files we are looking for
		char								m_PathID[MAX_PATH];
		char								m_SearchPath[MAX_PATH];
		
		char								m_SearchSpec[MAX_PATH];

	public:
	
										CAsyncSearchRequest();						// Default Constructor - We want NO implicit constructions
		virtual							~CAsyncSearchRequest();						// Destructor

		// ---------------------------------------------------------
		// methods from the IAsyncRequestBase Interface
		// ---------------------------------------------------------
		
		// functions to query operation
		virtual AsyncFileOperation_t	GetAsyncOperationType()				{ return m_Base.GetAsyncOperationType(); }

		// Set completion options								
		virtual void					AssignCallback( CFunctor* pCallback )				{  m_Base.AssignCallback( pCallback ); }
		virtual void					AssignResultQueue( CIOCompletionQueue* pMsgQueue )	{  m_Base.AssignResultQueue( pMsgQueue ); }
		virtual void					AssignCallbackAndQueue( CIOCompletionQueue* pMsgQueue, CFunctor* pCallback )  {  m_Base.AssignCallbackAndQueue( pMsgQueue, pCallback); }

		// Completion processing functions		
		virtual void					ProcessCallback( bool bRelease = true )	{  m_Base.ProcessCallback( bRelease ); }

		virtual void					KeepRequestPostCallback()			{  m_Base.KeepRequestPostCallback(); }
		virtual void					DontKeepRequestPostCallback()		{  m_Base.DontKeepRequestPostCallback(); }

		virtual void					Release()							{  m_Base.Release(); }

		// Priority Functions
		virtual void					SetPriority( int32 nPriority )		{  m_Base.SetPriority( nPriority ); }
		virtual int32					GetPriority()						{ return  m_Base.GetPriority(); }

		// Status & Results functions
		virtual AsyncRequestState_t		GetRequestState()					{ return  m_Base.GetRequestState(); }
		virtual AsyncRequestStatus_t	GetRequestStatus()					{ return  m_Base.GetRequestStatus(); }

	private:
		virtual CAsyncRequestBase*		GetBase()							{ return (CAsyncRequestBase*) &m_Base; }

		// ---------------------------------------------------------
		// methods from the IAsyncSearchRequest Interface
		// ---------------------------------------------------------

	public:
		// functions to define the request

		virtual void					SetSearchFilespec( const char* pFullSearchSpec );
		virtual void					SetSearchPathAndFileSpec( const char* pPathId, const char* pRelativeSearchSpec );
		virtual void					SetSearchPathAndFileSpec( const char* pPathId, const char* pRelativeSearchPath, const char* pSearchSpec );
		
		virtual void					SetSubdirectoryScan( const bool bInclude );
		virtual bool					GetSubdirectoryScan()				{ return m_bRecurseSubdirs; }
		
		// Functions to return the results.
		
		virtual int						GetResultCount()					{ return m_nNumResults; }
		virtual CDirectoryEntryInfo_t*	GetResult( int rNum = 0 );
		virtual const char*				GetMatchedFile( int rNum = 0 );

		// --------------------------------------------------------------
		// Public Methods not included in the interface, used by Async FileSystem
		
		void							AbortAfterServicing( CAsyncResultInfo_t& results );									// Handle a request that got aborted while being serviced
		void							UpdateAfterServicing( CAsyncResultInfo_t& results );								// Handle a request that got aborted while being serviced
		AsyncRequestStatus_t			ValidateSubmittedRequest( bool bPerformSync );										// Validates the derived object data at request submission
		bool							ValidateRequestModification();														// Check to determine if modification is allowed at this time

	private:
		// disabled functions
		explicit						CAsyncSearchRequest( const CAsyncSearchRequest& rhs );	// copy constructor - NOT SUPPORTED
		CAsyncSearchRequest&			operator=( const CAsyncSearchRequest& rhs );			// assignment operator - NOT SUPPORTED	

};


//-----------------------------------------------------------------------------
// CAsyncRequestQueue
//		Helper object for filesystem - Linked list of objects
//      dreeived from CAsyncRequestBase
//-----------------------------------------------------------------------------
class CAsyncRequestQueue
{
	private:
		CThreadFastMutex			m_Mutex;
		int32						m_nQueueSize;
		CAsyncRequestBase*			m_pHead;
		CAsyncRequestBase*			m_pTail;

	public:
		CAsyncRequestQueue();
		~CAsyncRequestQueue();

		int32						GetSize()	{ return m_nQueueSize; }
		bool						IsInQueue( CAsyncRequestBase* pItem );
		bool						IsInQueueIp( const IAsyncRequestBase* pInterfaceBase );
		
		void						AddToHead( CAsyncRequestBase* pItem );
		void						AddToTail( CAsyncRequestBase* pItem );
		void						InsertBefore( CAsyncRequestBase* pItem, CAsyncRequestBase* pInsertAt );
		void						InsertAfter( CAsyncRequestBase* pItem, CAsyncRequestBase* pInsertAt );
									
		void						PriorityInsert( CAsyncRequestBase* pItem );
									
		void						Remove( CAsyncRequestBase* pItem );
		CAsyncRequestBase*			RemoveHead();

};


//-----------------------------------------------------------------------------
// CAsyncFileSystem - implements IAsyncFileSystem interface
//	
//-----------------------------------------------------------------------------
class CAsyncFileSystem : public CTier2AppSystem< IAsyncFileSystem >
{
	typedef  CTier2AppSystem< IAsyncFileSystem >  BaseClass;
	
	friend class CAsyncGroupRequest;
	
	public:
	
	static const int				s_MaxPlatformFileNameLength = 40;
	
	public:
		CAsyncFileSystem();
		~CAsyncFileSystem();
											
		//--------------------------------------------------
		// Interface methods exposed with IAsyncFileSystem
		//--------------------------------------------------
		
		virtual AsyncRequestStatus_t			SubmitAsyncFileRequest( const IAsyncRequestBase* pRequest );			
		virtual AsyncRequestStatus_t			SubmitSyncFileRequest( const IAsyncRequestBase* pRequest );			
	
 		virtual AsyncRequestStatus_t			GetAsyncFileRequestStatus( const IAsyncRequestBase* pRequest );
		virtual AsyncRequestStatus_t			AbortAsyncFileRequest( const IAsyncRequestBase* pRequest );

		virtual void							SuspendAllAsyncIO( bool bWaitForIOCompletion = true );
		virtual void							ResumeAllAsyncIO();
		virtual AsyncRequestStatus_t			AbortAllAsyncIO( bool bWaitForIOCompletion = true );

		virtual IAsyncFileRequest*				CreateNewFileRequest();
		virtual IAsyncSearchRequest*			CreateNewSearchRequest();
		virtual IAsyncGroupRequest*				CreateNewAsyncRequestGroup();
		
		virtual void							ReleaseAsyncRequest( const IAsyncRequestBase* pRequest );
		virtual bool							BlockUntilAsyncIOComplete( const IAsyncRequestBase* pRequest );

		virtual void*							AllocateBuffer( size_t nBufferSize, int nAlignment = 4 );
		virtual void							ReleaseBuffer( void* pBuffer );

		//--------------------------------------------------
		// Public Methods used inside the filesystem
		//--------------------------------------------------
		
	
		void*								QueryInterface( const char *pInterfaceName );
		
		void								RemoveRequest( CAsyncRequestBase* pRequest );

		bool								ResolveAsyncRequest( const IAsyncRequestBase* pRequest, CAsyncRequestBase*& pRequestBase, AsyncRequestState_t& CurrentStage );

		bool								ValidateRequestPtr( CAsyncRequestBase* pRequest );

	private:
		CAsyncRequestQueue					m_Composing;				// requests in state: Composing
		CAsyncRequestQueue					m_Submitted;				// requests in state: Submitted
		CAsyncRequestQueue					m_InFlight;					// requests in state: Serviced
		CAsyncRequestQueue					m_Completed;				// requests in state: Awaiting Finished, Completed*

		
		CInterlockedInt						m_nJobsInflight;			// number of requests currently being serviced by the old system
		CInterlockedInt						m_nSuspendCount;			// number of requests to suspend all Async IO outstanding (nested request support)
		bool								m_bIOSuspended;

#if defined(_DEBUG)
		CThreadFastMutex					m_MemoryTrackMutex;
		CUtlVector<void*>					m_AllocList;				// Debug build - track allocations
#endif

		CThreadFastMutex					m_AsyncStateUpdateMutex;
		CThreadEvent						m_CompletionSignal;			// Used to signal the completion of an IO

		AsyncRequestStatus_t				ValidateRequest( CAsyncRequestBase* pRequest, bool bPerformSync = false );

		void								KickOffFileJobs();			
		void								WaitForServincingIOCompletion();	// Pauses until all jobs being serviced are complete

		static void*						OldAsyncAllocatorCallback( const char *pszFilename, unsigned nBytes );
		
		static void							AsyncIOCallbackGateway(const FileAsyncRequest_t &request, int nBytesRead, FSAsyncStatus_t err);
		static void							AsyncSearchCallbackAddItem( void* pContext, char* pFoundPath, char* pFoundFile );
		static void							AsyncSearchCallbackGateway( void* pContext, FSAsyncStatus_t err );
		
		void								AsyncIOCallBackHandler(CAsyncRequestBase* pRequest, CAsyncResultInfo_t& results );
			
		void								NotifyMessageQueueOrCallback( CAsyncRequestBase* pRequest );
			
			
};



//-----------------------------------------------------------------------------
// Utility functions
//	
//-----------------------------------------------------------------------------
AsyncRequestStatus_t ValidateFileName( const char* pFileName );



#endif
