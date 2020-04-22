//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:   Provides IAsyncFileRequest and IAsyncFileSystem Interfaces
//                     and supporting objects and values
//
// $NoKeywords: $
//===========================================================================//

#ifndef IASYNCFILESYSTEM_H
#define IASYNCFILESYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include <limits.h>

#include "tier0/threadtools.h"
#include "tier0/memalloc.h"
#include "tier0/tslist.h"
#include "tier1/interface.h"
#include "tier1/utlsymbol.h"
#include "tier1/functors.h" 
#include "tier1/checksum_crc.h"
#include "tier1/utlqueue.h"
#include "appframework/iappsystem.h"
#include "tier2/tier2.h"



//-----------------------------------------------------------------------------
// "New" (circa 2009) Async FileSystem Interface
//-----------------------------------------------------------------------------
// This is a "newer" asynchronous-only file system interface.  Using the
// global that exposes it is intended to guarantee that no file I/O operations
// will be blocking (unless you submit them synchronously).
//

//-----------------------------------------------------------------------------
// Forward declarations we need
//-----------------------------------------------------------------------------
class CIOCompletionQueue;
class IAsyncFileRequest;
class IAsyncSearchRequest;


//-----------------------------------------------------------------------------
// AsyncRequestStatus_t - Contains the success / error status of an  
//                        asynchronous file operation
//-----------------------------------------------------------------------------
enum AsyncRequestStatus_t
{
	ASYNC_REQUEST_ERROR_BADPTR				= -99,			// The request Pointer passed in is bad
	ASYNC_REQUEST_ERROR_ALREADYSUBMITTED	= -98,			// The request was already submitted
	ASYNC_REQUEST_ERROR_BADOPER				= -97,			// The operation type (read, write, append, etc) is invalid
	ASYNC_REQUEST_ERROR_BADUSERBUFFER		= -96,			// The supplied user buffer is incomplete
	ASYNC_REQUEST_ERROR_NOBUFFER			= -95,			// The operation requested requires a valid user buffer
	ASYNC_REQUEST_ERROR_NONOTIFICATION		= -94,			// User did not specify a callback or an IOResultQueue to message
	ASYNC_REQUEST_ERROR_NOTSUBMITTED		= -93,			// The request has not yet been submitted
	ASYNC_REQUEST_ERROR_ALREADYSERVICED		= -92,			// The request has already been serviced

	ASYNC_REQUEST_ERROR_PATHTOOLONG			= -89,			// The filepath+name is too long
	ASYNC_REQUEST_ERROR_INVALIDFILENAME		= -88,			// The Filename contains invalid characters
	ASYNC_REQUEST_ERROR_BADPATHSPEC			= -87,			// adjacent path separators in the filename or ends in path char
	ASYNC_REQUEST_ERROR_FILENAMETOOLONG		= -86,			// the filename is too long...
	ASYNC_REQUEST_ERROR_NOTVALIDSYNCRONOUS	= -85,			// Request (callbacks) not valid in synchronous mode

	// these mirror the existing async file system return codes.   remove when changed
	ASYNC_REQUEST_ERROR_ALIGNMENT    		= -16,			// read parameters invalid for unbuffered IO
	ASYNC_REQUEST_ERROR_FAILURE      		= -15,			// hard subsystem failure
	ASYNC_REQUEST_ERROR_READING      		= -14,			// read error on file
	ASYNC_REQUEST_ERROR_NOMEMORY     		= -13,			// out of memory for file read
	ASYNC_REQUEST_ERROR_UNKNOWNID    		= -12,			// caller's provided id is not recognized
	ASYNC_REQUEST_ERROR_FILEOPEN     		= -11,			// filename could not be opened (bad path, not exist, etc)

	ASYNC_REQUEST_STATUS_UNDEFINED			= -1,			// should never be this (indicates incomplete code path)

	ASYNC_REQUEST_OK               			= 0,			// operation is successful

	
	ASYNC_REQUEST_FORCE_INT32				= INT32_MAX		// force AsyncRequestStatus_t to 32 bits
};


//-----------------------------------------------------------------------------
// AsyncRequestState_t - indicates what state / stage of processing that an 
//                     async request is in
//-----------------------------------------------------------------------------
enum AsyncRequestState_t
{
	ASYNC_REQUEST_STATE_UNDEFINED   = 0,					// The request is probably invalid
	ASYNC_REQUEST_STATE_COMPOSING	= 1,					// The request is being composed (open for edit)
	ASYNC_REQUEST_STATE_SUBMITTED,							// The request is in the priority queue waiting to be serviced
	ASYNC_REQUEST_STATE_SERVICING,							// The request is being processed by a job thread
	ASYNC_REQUEST_STATE_AWATING_FINISH,						// The request has completed IO, and waiting to be post-processed by user code
	ASYNC_REQUEST_STATE_COMPLETED,							// The request has completed all tasks
	
	ASYNC_REQUEST_STATE_FORCE_INT32		= INT32_MAX			// force AsyncRequestStatus_t to 32 bits
};


//-----------------------------------------------------------------------------
// AsyncFileOperation_t - list of I/O options the new async system supports
//	
//-----------------------------------------------------------------------------
enum AsyncFileOperation_t
{
	ASYNC_OP_UNDEFINED			= 0,				// mapping undefined to 0 so that a value must be set (constructors memsets to 0)
	ASYNC_OP_READFILE,								// This is a read request
	ASYNC_OP_WRITEFILE,								// Write data to a file, delete if existing??
	ASYNC_OP_APPENDFILE,							// Append to end of file, must file exist?
	
	ASYNC_OP_SCANDIR,								// Scan a directory looking for name-matched files
	
	ASYNC_OP_COUNT,									// Automatic list count
	
	ASYNC_OP_GROUP,									// Object is a container of requests
	
	ASYNC_OP_FORCE_INT32		= INT32_MAX
};



//-----------------------------------------------------------------------------
// IAsyncRequestBase - Interface common to all types of 
//                     asynchronous file IO requests
//	
//-----------------------------------------------------------------------------
class IAsyncRequestBase
{
	friend class CAsyncFileSystem;
	friend class CAsyncGroupRequest;

	public:
		// functions to query operation
		virtual AsyncFileOperation_t		GetAsyncOperationType() = 0;							// Interface to determine what the derived type is
		
		// Completion options									
		virtual void						AssignCallback( CFunctor* pCallback ) = 0;						// Add a completion callback to this request
		virtual void						AssignResultQueue( CIOCompletionQueue* pMsgQueue ) = 0;			// Send a completion notification to this queue object
		virtual void						AssignCallbackAndQueue( CIOCompletionQueue* pMsgQueue, CFunctor* pCallback ) = 0;	// assign a callback and a completion queue 

		// Completion processing functions		
		virtual void						ProcessCallback( bool bReleaseRequest = true ) = 0;		// Perform any assigned callbacks

		virtual void						KeepRequestPostCallback() = 0;							// User wants to keep request alive after the callback completes
		virtual void						DontKeepRequestPostCallback() = 0;						// User doesn't want to keep request after the callback completes 

		virtual void						Release() = 0;											// lets user manually release the async request (only valid when completed)

		// Priority Functions
		virtual void						SetPriority( int32 nPriority ) = 0;						// lets the user set or change the priority of this request
		virtual int32						GetPriority() = 0;										// queries the priority of this request
	
		// Status & Results functions
		virtual AsyncRequestState_t			GetRequestState() = 0;									// Returns what phase of the Async Process the request is in
		virtual AsyncRequestStatus_t		GetRequestStatus() = 0;									// Returns the error status of the last IO operation performed by this request

	private:		
		virtual class CAsyncRequestBase*	GetBase() = 0;											// Used internally by the AsyncFileSystem

};






//-----------------------------------------------------------------------------
// IAsyncRequestGroup - Interface for grouping requests together
//                     
//	
//-----------------------------------------------------------------------------
class IAsyncGroupRequest : public IAsyncRequestBase
{
	public:
		virtual void							AddAsyncRequest( IAsyncRequestBase* pRequest ) = 0;
		
		virtual int32							GetAsyncRequestCount() = 0;
		
		virtual IAsyncRequestBase*				GetAsyncRequest( int32 nRNum ) = 0;
		virtual IAsyncFileRequest*				GetAsyncFileRequest( int32 nRNum ) = 0;
		virtual IAsyncSearchRequest*			GetAsyncSearchRequest( int32 nRNum ) = 0;
		
		virtual void							ReleaseAsyncRequest( int32 nRNum ) = 0;

	
};



//-----------------------------------------------------------------------------
// IAsyncFileRequest - Interface for setting and reading the results of an
//                     asynchronous file IO request
//	
//-----------------------------------------------------------------------------
class IAsyncFileRequest : public IAsyncRequestBase
{
	public:
	
		// functions to set filename and operation
		virtual void						LoadFile( const char* pFileName ) = 0;					// make this a 'read data from file' request
		virtual void						SaveFile( const char* pFileName ) = 0;					// make this a 'write data to file' request
		virtual void						AppendFile( const char* pFileName ) = 0;				// make this a 'append data to file' request
		
		virtual void						SetFileName( const char* pFileName ) = 0;				// assign the filename to use
		virtual const char*					GetFileName() = 0;										// get the filename we've assigned
		
		// Buffer control functions	- user supplied data buffer
		virtual void						SetUserBuffer( void* pDataBuffer, size_t nBufferSize ) = 0;		// User supplies a memory buffer to use, which they own the memory for
		virtual void*						GetUserBuffer() = 0;											// returns the address of the user supplied buffer
		virtual size_t						GetUserBufferSize() = 0;										// returns the size of the user supplied buffer
		
		virtual void						ProvideDataBuffer() = 0;								// Let the async file system provide a buffer to hold the data transferred (valid for Load/Read only)

		// Buffer results - the data buffer can be user supplied or async system supplied
		virtual void*						GetResultBuffer() = 0;									// Returns the address of the data transferred
		virtual size_t						GetResultBufferSize() = 0;								// Returns the size of the buffer holding the data transferred
		virtual size_t						GetIOTransferredSize() = 0;								// Returns the number of bytes of data actually transferred

		// Memory control functions for buffers allocated by the async file system
		virtual void						KeepResultBuffer() = 0;									// User wants to keeps buffer allocated by the file system
		virtual void						ReleaseResultBuffer() = 0;								// User decides they want the request to take care of releasing the buffer

		// file position functions											
		virtual void						ReadFileDataAt( int64 nFileOffset, size_t nReadSize = 0 ) = 0;		// Read file data starting at supplied offset, optional max size to read
		virtual void						WriteFileDataAt( int64 nFileOffset, size_t nWriteSize = 0 )	= 0;	// Write data to file at supplied offset, optional size to write (max size of buffer)
		
};


//-----------------------------------------------------------------------------
// CDirectoryEntryInfo_t - File Information gathered by async directory searches
//                     Designed to be cross platform           
//	
//-----------------------------------------------------------------------------
struct CDirectoryEntryInfo_t
{
	public:
		char			m_FullFileName[MAX_PATH];
};


//-----------------------------------------------------------------------------
// IAsyncSearchRequest - Interface for setting and reading the results
//                                of an asynchronous directory search request
//	
//-----------------------------------------------------------------------------
class IAsyncSearchRequest : public IAsyncRequestBase
{
	public:
		// functions to define the request

		virtual void						SetSearchFilespec( const char* pFullSearchSpec ) = 0;
		virtual void						SetSearchPathAndFileSpec( const char* pPathId, const char* pRelativeSearchSpec ) = 0;
		virtual void						SetSearchPathAndFileSpec( const char* pPathId, const char* pRelativeSearchPath, const char* pSearchSpec ) = 0;

		virtual void						SetSubdirectoryScan( const bool bInclude ) = 0;
		virtual bool						GetSubdirectoryScan() = 0;
		
		// Functions to return the results.
		
		virtual int							GetResultCount() = 0;
		virtual CDirectoryEntryInfo_t*		GetResult( int rNum = 0 ) = 0;
		virtual const char*					GetMatchedFile( int rNum = 0 ) = 0;

};



//-----------------------------------------------------------------------------
// CAsyncIOResult_t - Holds the result of an async file I/O request in a 
//	                  CIOCompletionQueue
//-----------------------------------------------------------------------------
struct CAsyncIOResult_t
{
	public:
		IAsyncRequestBase*		m_pRequest;				// Handle of the job
		AsyncRequestStatus_t	m_Status;				// Result status

		inline					CAsyncIOResult_t();
		inline void 			ProcessCallback();

};

//-----------------------------------------------------------------------------
// CAsyncIOResult_t - Inline method Implementations
//
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// CAsyncIOResult_t - Constructor
//-----------------------------------------------------------------------------
inline CAsyncIOResult_t::CAsyncIOResult_t()
{
	m_pRequest = NULL;
	m_Status   = ASYNC_REQUEST_STATUS_UNDEFINED;

}

//-----------------------------------------------------------------------------
// CAsyncIOResult_t - process the user callback
//-----------------------------------------------------------------------------
inline void CAsyncIOResult_t::ProcessCallback()
{
	if ( m_pRequest != NULL )
	{
		m_pRequest->ProcessCallback();
	}
}


//-----------------------------------------------------------------------------
// CIOCompletionQueue - Dedicated Queue containing Async IO Notifications
//-----------------------------------------------------------------------------
class CIOCompletionQueue 
{
	private:
		 CTSQueue< CAsyncIOResult_t >   m_TSIOResultQueue;

	public:
		inline void 	Insert( CAsyncIOResult_t& theResult);
		inline bool 	IsEmpty();
		inline bool 	IsNotEmpty();
		inline void 	ProcessAllResultCallbacks();
		inline bool 	RemoveAtHead( CAsyncIOResult_t& theResult );

};


//-----------------------------------------------------------------------------
// CIOCompletionQueue - Inline Implementation
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// CIOCompletionQueue - insert message into queue
//-----------------------------------------------------------------------------
inline void CIOCompletionQueue::Insert( CAsyncIOResult_t& theResult)
{
	m_TSIOResultQueue.PushItem( theResult );
}
							
//-----------------------------------------------------------------------------
// CIOCompletionQueue - check if queue empty
//-----------------------------------------------------------------------------
inline bool CIOCompletionQueue::IsEmpty()
{
	return ( m_TSIOResultQueue.Count() == 0 );
}

//-----------------------------------------------------------------------------
// CIOCompletionQueue - check if queue is not empty
//-----------------------------------------------------------------------------
inline bool CIOCompletionQueue::IsNotEmpty()
{
	return ( m_TSIOResultQueue.Count() != 0 );
}

//-----------------------------------------------------------------------------
// CIOCompletionQueue constructor
//-----------------------------------------------------------------------------
inline void CIOCompletionQueue::ProcessAllResultCallbacks()
{
	CAsyncIOResult_t IOResult;
	while ( m_TSIOResultQueue.PopItem( &IOResult ) ) 
	{
		IOResult.ProcessCallback();				// no lock needed here, we are outside of the queue
	};
}

//-----------------------------------------------------------------------------
// CIOCompletionQueue - pop off the first item
//-----------------------------------------------------------------------------
inline bool CIOCompletionQueue::RemoveAtHead( CAsyncIOResult_t& theResult )
{
	return ( m_TSIOResultQueue.PopItem( &theResult ) );
}



//-----------------------------------------------------------------------------
// IAsyncFileSystem  - Interface to new async user methods
//	
//-----------------------------------------------------------------------------
class IAsyncFileSystem : public IAppSystem
{
public:
	//------------------------------------
	// Global operations
	//------------------------------------
	
	virtual AsyncRequestStatus_t			SubmitAsyncFileRequest( const IAsyncRequestBase* pRequest ) = 0;			
	virtual AsyncRequestStatus_t			SubmitSyncFileRequest( const IAsyncRequestBase* pRequest ) = 0;			

	virtual AsyncRequestStatus_t			GetAsyncFileRequestStatus( const IAsyncRequestBase* pRequest ) = 0;
	virtual AsyncRequestStatus_t			AbortAsyncFileRequest( const IAsyncRequestBase* pRequest ) = 0;

	virtual void							SuspendAllAsyncIO( bool bWaitForIOCompletion = true ) = 0;
	virtual void							ResumeAllAsyncIO() = 0;
	virtual AsyncRequestStatus_t			AbortAllAsyncIO( bool bWaitForIOCompletion = true ) = 0;

	virtual IAsyncFileRequest*				CreateNewFileRequest() = 0;
	virtual IAsyncSearchRequest*			CreateNewSearchRequest() = 0;
	virtual IAsyncGroupRequest*				CreateNewAsyncRequestGroup() = 0;
	
	virtual void							ReleaseAsyncRequest( const IAsyncRequestBase* pRequest ) = 0;
	virtual bool							BlockUntilAsyncIOComplete( const IAsyncRequestBase* pRequest ) = 0;

	virtual void*							AllocateBuffer( size_t nBufferSize, int nAlignment = 4 ) = 0;
	virtual void							ReleaseBuffer( void* pBuffer ) = 0;

	
};



//-----------------------------------------------------------------------------
// Globals Exposed
//-----------------------------------------------------------------------------
DECLARE_TIER2_INTERFACE( IAsyncFileSystem, g_pAsyncFileSystem );


//-----------------------------------------------------------------------------
//                    New Async Filesystem Request Helpers
//-----------------------------------------------------------------------------
// These are functions that simplify and clarify the setup of common file
// operations in the new asynchronous file system.
//
// Placed inline for linkage convenience
//
// If you have a generic helper you commonly use, add it to this list
//

//-----------------------------------------------------------------------------
// ::AsyncReadFile() - Helper function to setup a New Async Request
//		load an entire file file, allocate a buffer, use all other defaults
//-----------------------------------------------------------------------------
inline IAsyncFileRequest*  AsyncReadFile( const char* pFileName )
{
	IAsyncFileRequest*	theRequest = g_pAsyncFileSystem->CreateNewFileRequest();	// Open a new request
	theRequest->LoadFile( pFileName );					// specify file and set request type to read
	theRequest->ProvideDataBuffer();					// Let the request provide and own the memory
	return theRequest;									// return request object pointer
}


//-----------------------------------------------------------------------------
// ::AsyncReadFileToBuffer() - Helper function to setup a New Async Request
//		load an entire file file, to a supplied buffer, use all defaults
//-----------------------------------------------------------------------------
inline IAsyncFileRequest*  AsyncReadFileToBuffer( const char* pFileName, void* pBuffer, size_t nBufferSize )
{
	IAsyncFileRequest*	theRequest = g_pAsyncFileSystem->CreateNewFileRequest();	// Open a new request
	theRequest->LoadFile( pFileName );					// specify file and set request type to read
	theRequest->SetUserBuffer( pBuffer, nBufferSize );	// Specify the buffer we are using
	return theRequest;									// return request object pointer
}


//-----------------------------------------------------------------------------
// ::AsyncReadFileBlock() - Helper function to setup a New Async Request
//		load an portion of a file, allocate a buffer, use all defaults
//-----------------------------------------------------------------------------
inline IAsyncFileRequest*  AsyncReadFileBlock( const char* pFileName, int64 nOffset, size_t nLoadSize )
{
	IAsyncFileRequest*	theRequest = g_pAsyncFileSystem->CreateNewFileRequest();	// Open a new request
	theRequest->LoadFile( pFileName );					// specify file and set request type to read
	theRequest->ReadFileDataAt( nOffset, nLoadSize );	// Specify the part of the file we want to read
	theRequest->ProvideDataBuffer();					// Let the request provide and own the memory
	return theRequest;									// return request object pointer
}


//-----------------------------------------------------------------------------
// ::AsyncReadFileBlockToBuffer() - Helper function to setup a New Async Request
//		load an portion of a file, to a supplied buffer, use all defaults
//-----------------------------------------------------------------------------
inline IAsyncFileRequest*  AsyncReadFileBlockToBuffer( const char* pFileName, int64 nOffset, size_t nLoadSize, void* pBuffer, size_t nBufferSize )
{
	IAsyncFileRequest*	theRequest = g_pAsyncFileSystem->CreateNewFileRequest();	// Open a new request
	
	theRequest->LoadFile( pFileName );					// specify file and set request type to read
	theRequest->ReadFileDataAt( nOffset, nLoadSize );	// Specify the part of the file we want to read
	theRequest->SetUserBuffer( pBuffer, nBufferSize );	// Specify the buffer we are using

	return theRequest;									// return request object pointer
}


//-----------------------------------------------------------------------------
// ::AsyncWriteBufferToFile() - Helper function to setup a New Async Request
//		write a buffer to a file, overwriting existing, use all defaults
//-----------------------------------------------------------------------------
inline IAsyncFileRequest*  AsyncWriteBufferToFile( const char* pFileName, void* pBuffer, size_t nBufferSize )
{
	IAsyncFileRequest*	theRequest = g_pAsyncFileSystem->CreateNewFileRequest();	// Open a new request
	
	theRequest->SaveFile( pFileName );					// Specify file and set request type to write 
	theRequest->SetUserBuffer( pBuffer, nBufferSize );	// Specify the buffer we are using
	
	return theRequest;									// return request object pointer
}


//-----------------------------------------------------------------------------
// ::AsyncAppendBufferToFile() - Helper function to setup a New Async Request
//		write a buffer to a file, append to if existing, use all defaults
//-----------------------------------------------------------------------------
inline IAsyncFileRequest*  AsyncAppendBufferToFile( const char* pFileName, void* pBuffer, size_t nBufferSize )
{
	IAsyncFileRequest*	theRequest = g_pAsyncFileSystem->CreateNewFileRequest();	// Open a new request
	
	theRequest->AppendFile( pFileName );				// Specify file and set request type to append data to
	theRequest->SetUserBuffer( pBuffer, nBufferSize );	// Specify the buffer we are using
	
	return theRequest;									// return request object pointer
}

//-----------------------------------------------------------------------------
// ::AsyncRequestRelease() - Helper function to release Async Request
//		calls release, sets the handle to null
//-----------------------------------------------------------------------------
inline void AsyncRequestRelease( IAsyncRequestBase* pRequest )
{
	g_pAsyncFileSystem->ReleaseAsyncRequest( pRequest );
	pRequest = NULL;
}


//-----------------------------------------------------------------------------
// ::NewAsyncRequestGroup() - Helper function to set up a new file search
//		used with a search path..
//-----------------------------------------------------------------------------
inline IAsyncSearchRequest* AsyncFindFiles( const char* pPathID, const char* pSearchSpec )
{
	IAsyncSearchRequest* theRequest = g_pAsyncFileSystem->CreateNewSearchRequest();   // Open a new request
	
	theRequest->SetSearchPathAndFileSpec( pPathID, pSearchSpec );
	theRequest->SetSubdirectoryScan( false );
	
	return theRequest;									// return request object pointer
}


//-----------------------------------------------------------------------------
// ::NewAsyncRequestGroup() - Helper function to set up a new request group
//		object
//-----------------------------------------------------------------------------
inline IAsyncGroupRequest* NewAsyncRequestGroup()
{
	return g_pAsyncFileSystem->CreateNewAsyncRequestGroup();
}




#endif	// IASYNCFILESYSTEM_H

