//========== Copyright © 2009, Valve Corporation, All rights reserved. ========
//
// file:	filesystem_newasync.cpp
//
// Purpose: Upgraded Async I/O system for source 2.0
//
//
// System Support (so far): Win32, 360  (see #POSIX usage in filesystem_async.cpp)
//
//=============================================================================



#include "basefilesystem.h"
#include "filesystem.h"
#include "filesystemasync.h"
#include "threadtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



#ifdef _DEBUG
	#define  VALIDATE_REQUEST_MODIFICATION    	if ( !ValidateRequestModification() ) { return; }
#else
	#define  VALIDATE_REQUEST_MODIFICATION      (void) 0;
#endif

//-----------------------------------------------------------------------------
// Globals object instance?  put in tier2.cpp?
//-----------------------------------------------------------------------------
extern CAsyncFileSystem g_FileSystem_Async;



//-----------------------------------------------------------------------------
// CAsyncRequestBase::Default Constructor
//		-- inits all members common to all Async Requests
//-----------------------------------------------------------------------------
CAsyncRequestBase::CAsyncRequestBase()
{
	m_pOuter = NULL;
	
	m_pCallback	= NULL;
	m_pResultQueue = NULL;
	
	m_Operation	= ASYNC_OP_UNDEFINED;
	m_priority = 0;

	m_pNext	= NULL;
	m_pPrev	= NULL;
	
	m_RequestState = ASYNC_REQUEST_STATE_COMPOSING;
	m_RequestStatus	= ASYNC_REQUEST_OK;
	
	m_bAbortRequest	= false;
	m_bProcessingCallback = false;
	m_bDontAutoRelease = false;
	
	m_pSyncThreadEvent = NULL;
	
	m_pOldAsyncControl = NULL;
	m_pOldAsyncStatus = FSASYNC_OK;
	
	m_pGroup = NULL;
	
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::Destructor
//		
//-----------------------------------------------------------------------------
CAsyncRequestBase::~CAsyncRequestBase()
{
	RefreshCallback( NULL );		// Release functor
	
	// We do this in the base class as it will be the last destructor called

	g_FileSystem_Async.RemoveRequest( this );		// Tell the Async System to remove us from its lists
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::Destructor
//		
//-----------------------------------------------------------------------------
IAsyncRequestBase* CAsyncRequestBase::GetInterfaceBase()
{
	if ( m_pOuter != NULL )
	{
		switch ( m_Operation )
		{
			case ASYNC_OP_READFILE:
			case ASYNC_OP_WRITEFILE:
			case ASYNC_OP_APPENDFILE:
			{
				CAsyncFileRequest* pOuter = ( CAsyncFileRequest* ) m_pOuter;
				return static_cast< IAsyncRequestBase* >( pOuter );
			}

			case ASYNC_OP_SCANDIR:
			{
				CAsyncSearchRequest* pOuter = ( CAsyncSearchRequest* ) m_pOuter;
				return static_cast< IAsyncRequestBase* >( pOuter );
			}

			default:
			{
				Error( "Bad Outer in CAsyncRequestBase::DeleteOuter" );
				break;
			}
		}
	}
	
	return NULL;
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::AssignCallback
//		Add a completion callback to this request
//-----------------------------------------------------------------------------
void CAsyncRequestBase::AssignCallback( CFunctor* pCallback )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	RefreshCallback( pCallback );
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::AssignResultQueue
//		Send the results to this queue object
//-----------------------------------------------------------------------------
void CAsyncRequestBase::AssignResultQueue( CIOCompletionQueue* pMsgQueue )
{
	VALIDATE_REQUEST_MODIFICATION;
	m_pResultQueue = pMsgQueue;
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::AssignCallbackAndQueue
//		
//-----------------------------------------------------------------------------
void CAsyncRequestBase::AssignCallbackAndQueue( CIOCompletionQueue* pMsgQueue, CFunctor* pCallback )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	RefreshCallback( pCallback );
	m_pResultQueue = pMsgQueue;
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::RefreshFileName
//		Change the filename, release memory held by previous
//-----------------------------------------------------------------------------
void CAsyncRequestBase::ProcessCallback( bool bRelease )
{
	if ( m_RequestState != ASYNC_REQUEST_STATE_AWATING_FINISH )
	{
		Error( "Callback called at wrong time" );
		return;
	}

	// Callback is a one shot thing	
	if ( m_pCallback != NULL )
	{
		m_bProcessingCallback = true;
		(*m_pCallback)();
		m_pCallback->Release();
		m_pCallback = NULL;
		m_bProcessingCallback = false;
	}
	
	// Callback is completed (or NULL), move to next state
	m_RequestState = ASYNC_REQUEST_STATE_COMPLETED;	

	// Do we release automatically this object?
	if ( bRelease && !m_bDontAutoRelease )
	{
		// Upon return, this object will no longer be in the Async System...
		Release();
	}
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::Release
//	   Let's the user manually release/destroy an async request object	
//     Can only be done once the request is complete, if the async system
//     needs to at other times, call DeleteOuter() instead
//-----------------------------------------------------------------------------
void CAsyncRequestBase::Release()
{
	if ( m_RequestState != ASYNC_REQUEST_STATE_COMPLETED )
	{
		Error( "Async Request has not finished, unable to release" );
		return;
	}

	// The magic is in the destructor...
	// the destructor will detach this request from the async filesystem 
	// and clean everything up
	
	DeleteOuter();
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::DeleteOuter
//	   calls the destructor on the containing object	
//     must update this when adding a new async request type
//-----------------------------------------------------------------------------
void CAsyncRequestBase::DeleteOuter()
{
	if ( m_pOuter != NULL )
	{
		switch ( m_Operation )
		{
			case ASYNC_OP_READFILE:
			case ASYNC_OP_WRITEFILE:
			case ASYNC_OP_APPENDFILE:
			{
				CAsyncFileRequest* pOuter = ( CAsyncFileRequest* ) m_pOuter;
				delete pOuter;
				return;
			}

			case ASYNC_OP_SCANDIR:
			{
				CAsyncSearchRequest* pOuter = ( CAsyncSearchRequest* ) m_pOuter;
				delete pOuter;
				return;
			}

			default:
			{
				Error( "Bad Outer in CAsyncRequestBase::DeleteOuter" );
				break;
			}
		}
	}
}




//-----------------------------------------------------------------------------
// CAsyncRequestBase::SetPriority
//		
//-----------------------------------------------------------------------------
void CAsyncRequestBase::SetPriority( int32 nPriority )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	m_priority = nPriority;
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::RefreshCallback
//		Update the callback functor, release the previous one (if any)
//-----------------------------------------------------------------------------
void CAsyncRequestBase::RefreshCallback( CFunctor* pCallback )
{
	// release existing callback
	if ( m_pCallback != NULL )
	{
		m_pCallback->Release();
	}
	m_pCallback = pCallback;
}


//-----------------------------------------------------------------------------
// CAsyncRequestBase::AbortAfterServicing
//	  calls the correct containing object's method and does any cleanup needed
//    by the base object
//-----------------------------------------------------------------------------
 void CAsyncRequestBase::AbortAfterServicing( CAsyncResultInfo_t& results )
 {
 
	if ( m_pOuter != NULL )
	{
		switch ( m_Operation )
		{
			case ASYNC_OP_READFILE:
			case ASYNC_OP_WRITEFILE:
			case ASYNC_OP_APPENDFILE:
			{
				CAsyncFileRequest* pOuter = ( CAsyncFileRequest* ) m_pOuter;
				pOuter->AbortAfterServicing( results );
				break;
			}

			case ASYNC_OP_SCANDIR:
			{
				CAsyncSearchRequest* pOuter = ( CAsyncSearchRequest* ) m_pOuter;
				pOuter->AbortAfterServicing( results );
				break;
			}

			default:
			{
				Error( "Bad Outer in CAsyncRequestBase::AbortAfterServicing" );
				break;
			}
		}
	}
	
 }
 
 
//-----------------------------------------------------------------------------
// CAsyncRequestBase::UpdateAfterServicing
//	  calls the correct containing object's method and does any update needed
//    by the base object
//-----------------------------------------------------------------------------
void CAsyncRequestBase::UpdateAfterServicing( CAsyncResultInfo_t& results )
 {
 
 	if ( m_pOuter != NULL )
	{
		switch ( m_Operation )
		{
			case ASYNC_OP_READFILE:
			case ASYNC_OP_WRITEFILE:
			case ASYNC_OP_APPENDFILE:
			{
				CAsyncFileRequest* pOuter = ( CAsyncFileRequest* ) m_pOuter;
				pOuter->UpdateAfterServicing( results );
				break;
			}

			case ASYNC_OP_SCANDIR:
			{
				CAsyncSearchRequest* pOuter = ( CAsyncSearchRequest* ) m_pOuter;
				pOuter->UpdateAfterServicing( results );
				break;
			}

			default:
			{
				Error( "Bad Outer in CAsyncRequestBase::UpdateAfterServicing" );
				break;
			}
		}
	}

 }
 
 
//-----------------------------------------------------------------------------
// CAsyncRequestBase::ValidateSubmittedRequest
//	  calls the correct containing object's method and does any validation needed
//    by the base object
//-----------------------------------------------------------------------------
 AsyncRequestStatus_t CAsyncRequestBase::ValidateSubmittedRequest( bool bPerformSync )
 {

 	if ( m_pOuter != NULL )
	{
		switch ( m_Operation )
		{
			case ASYNC_OP_READFILE:
			case ASYNC_OP_WRITEFILE:
			case ASYNC_OP_APPENDFILE:
			{
				CAsyncFileRequest* pOuter = ( CAsyncFileRequest* ) m_pOuter;
				return pOuter->ValidateSubmittedRequest( bPerformSync );
			}

			case ASYNC_OP_SCANDIR:
			{
				CAsyncSearchRequest* pOuter = ( CAsyncSearchRequest* ) m_pOuter;
				return pOuter->ValidateSubmittedRequest( bPerformSync );
			}

			default:
			{
				Error( "Bad Outer in CAsyncRequestBase::ValidateSubmittedRequest" );
				break;
			}
		}
	}

 
   return ASYNC_REQUEST_OK;
 }


//-----------------------------------------------------------------------------
// CAsyncRequestBase::ValidateRequestModification
//		Makes sure it's ok to modify this request
//-----------------------------------------------------------------------------
bool CAsyncRequestBase::ValidateRequestModification()
{
	if ( m_RequestState == ASYNC_REQUEST_STATE_COMPOSING )
	{
		return true;
	}
	
	Error( "Modifying Async Filesystem Request after it has been submitted" );
	return false;
}




//-----------------------------------------------------------------------------
// CAsyncGroupRequest::Default Constructor
//		-- inits all members
//-----------------------------------------------------------------------------
CAsyncGroupRequest::CAsyncGroupRequest() 	
{
	m_Base.SetOuter( this );
	m_nNumRequestsOutstanding = 0;
}


//-----------------------------------------------------------------------------
// CAsyncGroupRequest::Destructor
//		-- cleans up a group request..  we should assume that 
//-----------------------------------------------------------------------------
CAsyncGroupRequest::~CAsyncGroupRequest() 	
{
	// We shouldn't be destroying ourself unless the entire group has released
	
	Assert ( m_RequestList.Count() == 0 );
	Assert ( m_nNumRequestsOutstanding == 0 );
		
}


//-----------------------------------------------------------------------------
// CAsyncGroupRequest::AddAsyncRequest   Adds a request to a group.
//	
//-----------------------------------------------------------------------------
void CAsyncGroupRequest::AddAsyncRequest( IAsyncRequestBase* pRequest )
{
	// A bunch of assertions.  we're moving away from error checking in release builds
	Assert( pRequest != NULL );	
	Assert( g_FileSystem_Async.ValidateRequestPtr( pRequest->GetBase() ) );
	Assert( m_RequestList.Find( pRequest ) == -1 );
	Assert( pRequest->GetRequestStatus()  == ASYNC_REQUEST_STATE_COMPOSING );
	Assert( pRequest->GetBase()->m_pGroup == NULL );

	// Add a valid request to the end of the list
	m_RequestList.AddToTail( pRequest );
	m_ValidList.AddToTail( true );
	
}



//-----------------------------------------------------------------------------
// CAsyncGroupRequest::GetAsyncRequest - 
//	
//-----------------------------------------------------------------------------
IAsyncRequestBase* CAsyncGroupRequest::GetAsyncRequest( int32 nRNum )
{
	if ( nRNum < 0 || nRNum >=  m_RequestList.Count() )
	{
		Assert( false );
		return NULL;	
	}
	
	return m_RequestList[ nRNum ];
}


//-----------------------------------------------------------------------------
// CAsyncGroupRequest::GetAsyncFileRequest
//	
//-----------------------------------------------------------------------------
IAsyncFileRequest* CAsyncGroupRequest::GetAsyncFileRequest( int32 nRNum )
{
	if ( nRNum < 0 || nRNum >=  m_RequestList.Count() )
	{
		Assert( false );
		return NULL;	
	}
	
	IAsyncRequestBase* pRequest = m_RequestList[ nRNum ];
	AsyncFileOperation_t opType = pRequest->GetAsyncOperationType();
	
	if ( opType == ASYNC_OP_READFILE || opType == ASYNC_OP_WRITEFILE || opType == ASYNC_OP_APPENDFILE )
	{
		return ( IAsyncFileRequest* ) pRequest;
	}
	else
	{
		return NULL;
	}

}


//-----------------------------------------------------------------------------
// CAsyncGroupRequest::GetAsyncSearchRequest
//	
//-----------------------------------------------------------------------------
IAsyncSearchRequest* CAsyncGroupRequest::GetAsyncSearchRequest( int32 nRNum )
{
	if ( nRNum < 0 || nRNum >=  m_RequestList.Count() )
	{
		Assert( false );
		return NULL;	
	}
	
	IAsyncRequestBase* pRequest = m_RequestList[ nRNum ];
	AsyncFileOperation_t opType = pRequest->GetAsyncOperationType();
	
	if ( opType == ASYNC_OP_SCANDIR )
	{
		return ( IAsyncSearchRequest* ) pRequest;
	}
	else
	{
		return NULL;
	}
}



//-----------------------------------------------------------------------------
// CAsyncGroupRequest::
//	
//-----------------------------------------------------------------------------
void CAsyncGroupRequest::ReleaseAsyncRequest( int32 nRNum )
{


}



//-----------------------------------------------------------------------------
// CAsyncGroupRequest::
//	
//-----------------------------------------------------------------------------
void CAsyncGroupRequest::NotifyOfCompletion(  IAsyncRequestBase* pRequest )
{


}



//-----------------------------------------------------------------------------
// CAsyncFileRequest::Default Constructor
//		-- inits all members
//-----------------------------------------------------------------------------
CAsyncFileRequest::CAsyncFileRequest() 	
{
	m_Base.SetOuter( this );
	m_pFileName	= NULL;
	
	m_pUserProvidedDataBuffer = NULL;
	m_nUserProvidedBufferSize = 0;
	
	m_nFileSeekOffset = 0;
	m_nMaxIOSizeInBytes = 0;
	
	m_pResultsBuffer = NULL;
	m_nResultsBufferSize = 0;
	m_nIOActualSize	= 0;
	
	m_bDeleteBufferMemory = false;

}



									
//-----------------------------------------------------------------------------
// CAsyncFileRequest::Destructor
//		
//-----------------------------------------------------------------------------
CAsyncFileRequest::~CAsyncFileRequest()
{
	// Release memory we own...
	RefreshFileName( NULL );		// Release our copy of filename
	
	// and the data buffer? 
	if ( m_bDeleteBufferMemory )
	{
		if ( m_pResultsBuffer != NULL )
		{
			g_FileSystem_Async.ReleaseBuffer( m_pResultsBuffer );
		}
	}
	
	// The CAsyncRequestBase Release/DeleteOuter will take care of the rest and the release
}

	
//-----------------------------------------------------------------------------
// methods from the IAsyncFileRequest Interface
//-----------------------------------------------------------------------------
	
		
//-----------------------------------------------------------------------------
// CAsyncFileRequest::LoadFile
//	 	
//-----------------------------------------------------------------------------
void CAsyncFileRequest::LoadFile( const char* pFileName )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	RefreshFileName( pFileName );
	m_Base.m_Operation = ASYNC_OP_READFILE;
}


//-----------------------------------------------------------------------------
// CAsyncFileRequest::SaveFile
//		
//-----------------------------------------------------------------------------
void CAsyncFileRequest::SaveFile( const char* pFileName )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	RefreshFileName( pFileName );
	m_Base.m_Operation = ASYNC_OP_WRITEFILE;
}


//-----------------------------------------------------------------------------
// CAsyncFileRequest::AppendFile
//		
//-----------------------------------------------------------------------------
void CAsyncFileRequest::AppendFile( const char* pFileName )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	RefreshFileName( pFileName );
	m_Base.m_Operation = ASYNC_OP_APPENDFILE;
}


//-----------------------------------------------------------------------------
// CAsyncFileRequest::AppendFile
//		
//-----------------------------------------------------------------------------
void CAsyncFileRequest::SetFileName( const char* pFileName )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	RefreshFileName( pFileName );
}


//-----------------------------------------------------------------------------
// CAsyncFileRequest::SetUserBuffer
//		- user supplies buffer to use
//-----------------------------------------------------------------------------
void CAsyncFileRequest::SetUserBuffer( void* pDataBuffer, size_t nBufferSize )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	m_pUserProvidedDataBuffer = pDataBuffer;
	m_nUserProvidedBufferSize = nBufferSize;
	m_bDeleteBufferMemory = false;
}


//-----------------------------------------------------------------------------
// CAsyncFileRequest::ProvideDataBuffer
//		file system provide a buffer with the results
//-----------------------------------------------------------------------------
void CAsyncFileRequest::ProvideDataBuffer()
{
	VALIDATE_REQUEST_MODIFICATION;
	
	m_pUserProvidedDataBuffer = NULL;
	m_nUserProvidedBufferSize = 0;
	m_bDeleteBufferMemory = true;
}
		

//-----------------------------------------------------------------------------
// CAsyncFileRequest::ReadFileDataAt
//		Read file data starting at supplied offset, optional max size to read
//-----------------------------------------------------------------------------
void CAsyncFileRequest::ReadFileDataAt( int64 nOffset, size_t nReadSize )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	m_nFileSeekOffset	= nOffset;
	m_nMaxIOSizeInBytes = nReadSize;
}
//-----------------------------------------------------------------------------
// CAsyncFileRequest::WriteFileDataAt
//		Write data to file at supplied offset, optional size to write (max size of buffer)
//-----------------------------------------------------------------------------
void CAsyncFileRequest::WriteFileDataAt( int64 nOffset, size_t nWriteSize )
{
	VALIDATE_REQUEST_MODIFICATION;
	
	m_nFileSeekOffset	  = nOffset;
	m_nMaxIOSizeInBytes   = nWriteSize;
	m_bDeleteBufferMemory = false;
}

//-----------------------------------------------------------------------------
// CAsyncFileRequest::AbortAfterServicing
//		Handle a request that got aborted while being serviced
//-----------------------------------------------------------------------------
void CAsyncFileRequest::AbortAfterServicing( CAsyncResultInfo_t& results )
{

	// was it a read and did the request allocate a buffer?
	if ( GetAsyncOperationType()== ASYNC_OP_READFILE && m_pUserProvidedDataBuffer == NULL )
	{
		// the old async code allocated a buffer that we aren't going to use/return
		if ( results.m_pAllocatedBuffer != NULL )
		{
			// this should call our allocation function
			delete[] results.m_pAllocatedBuffer;
		}
	}
}


//-----------------------------------------------------------------------------
// CAsyncFileRequest::UpdateAfterServicing
//		Handle a request that was successfully serviced
//-----------------------------------------------------------------------------
void CAsyncFileRequest::UpdateAfterServicing( CAsyncResultInfo_t& results )
{

	// copy our return info from the old results struct
	m_pResultsBuffer = results.m_pAllocatedBuffer;
	m_nResultsBufferSize = ( m_pUserProvidedDataBuffer == NULL ) ? results.m_nBytesTransferred : m_nUserProvidedBufferSize;
	m_nIOActualSize = results.m_nBytesTransferred;

}


//-----------------------------------------------------------------------------
// CAsyncFileRequest::ValidateSubmittedRequest
//		Validate that this request can be submitted successfully
//-----------------------------------------------------------------------------
AsyncRequestStatus_t CAsyncFileRequest::ValidateSubmittedRequest( bool bPerformSync )
{

	// filename must be supplied
	if ( GetFileName() == NULL )
	{
		return ASYNC_REQUEST_ERROR_BADOPER;			
	}

#ifdef _DEBUG
	// In debug mode, Check the filename for length, usage issues
	AsyncRequestStatus_t err =  ValidateFileName( GetFileName() );
	if ( err != ASYNC_REQUEST_OK ) return err;
#endif	
	// if the user supplies a buffer, it must have a size
	if ( GetUserBuffer() != NULL && GetUserBufferSize() == 0 )
	{
		return ASYNC_REQUEST_ERROR_BADUSERBUFFER;		// cant supply a zero sized buffer
	}

	// if we are writing, then a buffer must be specified
	if ( ( CAsyncFileRequest::GetAsyncOperationType() == ASYNC_OP_WRITEFILE || CAsyncFileRequest::GetAsyncOperationType() == ASYNC_OP_APPENDFILE ) && GetUserBuffer() == NULL )   
	{
		return ASYNC_REQUEST_ERROR_NOBUFFER;
	}

	return ASYNC_REQUEST_OK;
}



		
					
//-----------------------------------------------------------------------------
// CAsyncFileRequest::RefreshFileName
//		Change the filename, release memory held by previous
//-----------------------------------------------------------------------------
void CAsyncFileRequest::RefreshFileName( const char* pNewFileName )
{
	// release existing filename
	if ( m_pFileName != NULL )
	{
		delete[] m_pFileName;
	}
	// we duplicate the file name, and own its memory
	if ( pNewFileName != NULL )
	{
		m_pFileName	= strdup( pNewFileName );
	}
	else
	{
		m_pFileName = NULL;
	}
}

//-----------------------------------------------------------------------------
// CAsyncFileRequest::ValidateRequestModification
//		Makes sure it's ok to modify this request
//-----------------------------------------------------------------------------
bool CAsyncFileRequest::ValidateRequestModification()
{
	return m_Base.ValidateRequestModification();
}





//-----------------------------------------------------------------------------
// CAsyncSearchRequest::Default Constructor
//		-- inits all members
//-----------------------------------------------------------------------------
CAsyncSearchRequest::CAsyncSearchRequest()
{
	m_Base.SetOuter( this );

	V_memset( m_FileSpec, 0, sizeof( m_FileSpec ) );
	V_memset( m_PathID, 0, sizeof( m_PathID ) );
	V_memset( m_SearchPath, 0, sizeof( m_SearchPath ) );
	V_memset( m_SearchSpec, 0, sizeof( m_SearchSpec ) );
	
	m_nNumResults = 0;
	m_bRecurseSubdirs = false;
}


//-----------------------------------------------------------------------------
// CAsyncSearchRequest::Destructor
//		
//-----------------------------------------------------------------------------
CAsyncSearchRequest::~CAsyncSearchRequest()
{
	// Release any memory we own...
	
	
	// The base object destructor will take care of the rest and the release
}



	
//-----------------------------------------------------------------------------
// Methods from the IAsyncSearchRequest Interface
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// CAsyncSearchRequest::SetSearchFilespec
//    - assign the path and filename to search for
//-----------------------------------------------------------------------------
void CAsyncSearchRequest::SetSearchFilespec( const char* pFullSearchSpec )
{
	VALIDATE_REQUEST_MODIFICATION;

	Assert( pFullSearchSpec != NULL && strlen( pFullSearchSpec ) < sizeof( m_SearchSpec ) );
	
	V_strcpy( m_SearchSpec, pFullSearchSpec );
	
	m_Base.m_Operation = ASYNC_OP_SCANDIR;
}



//-----------------------------------------------------------------------------
// CAsyncSearchRequest::SetSearchFilespec
//    - assign the path and filename to search for
//-----------------------------------------------------------------------------
void CAsyncSearchRequest::SetSearchPathAndFileSpec( const char* pPathId, const char* pRelativeSearchSpec )
{
	VALIDATE_REQUEST_MODIFICATION;

	Assert( pPathId != NULL && strlen( pPathId ) < sizeof( m_PathID ) );
	Assert( pRelativeSearchSpec != NULL && strlen( pRelativeSearchSpec ) < sizeof( m_FileSpec ) );

	Q_strcpy( m_FileSpec, V_UnqualifiedFileName( pRelativeSearchSpec) );

	V_ExtractFilePath (pRelativeSearchSpec, m_SearchPath, sizeof(m_SearchPath) );
	V_StripTrailingSlash( m_SearchPath );
	
	char pTempPath[MAX_PATH];
	g_pFullFileSystem->RelativePathToFullPath( m_SearchPath, pPathId, pTempPath, sizeof(pTempPath) );
	
	char pFixedupPath[MAX_PATH];
	Q_FixupPathName( pFixedupPath, sizeof(pFixedupPath), pTempPath );
	
	V_ComposeFileName( pFixedupPath, m_FileSpec, m_SearchSpec, sizeof(m_SearchSpec) );
	
	m_Base.m_Operation = ASYNC_OP_SCANDIR;

}



//-----------------------------------------------------------------------------
// CAsyncSearchRequest::SetSearchFilespec
//    - assign the path and filename to search for
//-----------------------------------------------------------------------------
void CAsyncSearchRequest::SetSearchPathAndFileSpec( const char* pPathId, const char* pRelativeSearchPath, const char* pSearchSpec )
{
	VALIDATE_REQUEST_MODIFICATION;

	Assert( pPathId != NULL && strlen( pPathId ) < sizeof( m_PathID ) );
	Assert( pRelativeSearchPath != NULL && strlen( pRelativeSearchPath ) < sizeof( m_SearchPath ) );
	Assert( pSearchSpec != NULL && strlen( pSearchSpec ) < sizeof( m_FileSpec ) );


	Q_strcpy( m_FileSpec, pSearchSpec );
	Q_strcpy( m_SearchPath, pRelativeSearchPath );
	V_StripTrailingSlash( m_SearchPath );
	
	char pTempPath[MAX_PATH];
	g_pFullFileSystem->RelativePathToFullPath( m_SearchPath, pPathId, pTempPath, sizeof(pTempPath) );
	
	char pFixedupPath[MAX_PATH];
	Q_FixupPathName( pFixedupPath, sizeof(pFixedupPath), pTempPath );
	
	V_ComposeFileName( pFixedupPath, m_FileSpec, m_SearchSpec, sizeof(m_SearchSpec) );
	
	m_Base.m_Operation = ASYNC_OP_SCANDIR;
}




//-----------------------------------------------------------------------------
// CAsyncSearchRequest::SetSubdirectoryScan
// 
//-----------------------------------------------------------------------------
void CAsyncSearchRequest::SetSubdirectoryScan( const bool bInclude )
{
	VALIDATE_REQUEST_MODIFICATION;

	m_bRecurseSubdirs = bInclude;
}



//-----------------------------------------------------------------------------
// CAsyncSearchRequest::GetResult
//    returns a search result
//-----------------------------------------------------------------------------
CDirectoryEntryInfo_t*	CAsyncSearchRequest::GetResult( int rNum )
{
	if ( m_Base.GetRequestState() < ASYNC_REQUEST_STATE_AWATING_FINISH || m_Base.GetRequestState() > ASYNC_REQUEST_STATE_COMPLETED )
	{
		Assert( false );
		return NULL;
	}

	if ( rNum < 0 || rNum >= m_nNumResults )
	{
		Assert( false );
		return NULL;
	}
	
	return &( m_Results[ rNum ] );
}


//-----------------------------------------------------------------------------
// CAsyncSearchRequest::GetMatchedFile
//    returns a search result
//-----------------------------------------------------------------------------
const char*	CAsyncSearchRequest::GetMatchedFile( int rNum )
{
	if ( m_Base.GetRequestState() < ASYNC_REQUEST_STATE_AWATING_FINISH || m_Base.GetRequestState() > ASYNC_REQUEST_STATE_COMPLETED )
	{
		Assert( false );
		return NULL;
	}

	if ( rNum < 0 || rNum >= m_nNumResults )
	{
		Assert( false );
		return NULL;
	}
	
	return m_Results[ rNum ].m_FullFileName ;
}


//-----------------------------------------------------------------------------
// CAsyncSearchRequest::AbortAfterServicing
//		Handle a request that got aborted while being serviced
//-----------------------------------------------------------------------------
void CAsyncSearchRequest::AbortAfterServicing( CAsyncResultInfo_t& results ) 
{
	// nothing dynamic to release
}


//-----------------------------------------------------------------------------
// CAsyncSearchRequest::UpdateAfterServicing
//		Handle a request that was successfully serviced
//-----------------------------------------------------------------------------
void CAsyncSearchRequest::UpdateAfterServicing( CAsyncResultInfo_t& results ) 
{
	// determine request specific values
	m_nNumResults = m_Results.Count();
}

//-----------------------------------------------------------------------------
// CAsyncSearchRequest::ValidateSubmittedRequest
//		Validate that this request can be submitted successfully
//-----------------------------------------------------------------------------
AsyncRequestStatus_t CAsyncSearchRequest::ValidateSubmittedRequest( bool bPerformSync )
{
	// filespec missing?
	if ( V_strlen( m_FileSpec ) < 1 )
	{
		return ASYNC_REQUEST_ERROR_INVALIDFILENAME;
	}

	return ASYNC_REQUEST_OK;
}





//-----------------------------------------------------------------------------
// CAsyncSearchRequest::ValidateRequestModification
//		Makes sure it's ok to modify this request
//-----------------------------------------------------------------------------
bool CAsyncSearchRequest::ValidateRequestModification()
{
	return m_Base.ValidateRequestModification();
}







//-----------------------------------------------------------------------------
// CAsyncRequestQueue::constructor
//		
//-----------------------------------------------------------------------------
CAsyncRequestQueue::CAsyncRequestQueue()
{
	m_nQueueSize = 0;
	m_pHead = NULL;
	m_pTail = NULL;
}


//-----------------------------------------------------------------------------
// CAsyncRequestQueue::destructor
//		
//-----------------------------------------------------------------------------
CAsyncRequestQueue::~CAsyncRequestQueue()
{
	AUTO_LOCK_FM( m_Mutex );		// wait for any other operations to finish
	
	// Delete any requests remaining in the queue
	if ( m_pHead != NULL )
	{
		Warning( " CAsyncRequestQueue destructor called while queue was not empty" );
		while ( m_pHead != NULL )
		{
			CAsyncRequestBase* pNext = m_pHead->m_pNext;
			delete m_pHead;
			m_pHead = pNext;	
		}
	}
}


//-----------------------------------------------------------------------------
// CAsyncRequestQueue::IsInQueue
//			- asks if a request is in this queue
//-----------------------------------------------------------------------------
bool CAsyncRequestQueue::IsInQueue( CAsyncRequestBase* pItem )
{
	AUTO_LOCK_FM( m_Mutex );		// synchronize since we are scanning the queue?
	
	if ( pItem == NULL || m_pHead == NULL ) return false;
	
	CAsyncRequestBase* pCur = m_pHead;
	while ( pCur != NULL )
	{
		if ( pCur == pItem ) return true;
		pCur = pCur->m_pNext;
	}
	
	return false;
}


//-----------------------------------------------------------------------------
// CAsyncRequestQueue::IsInQueueIp - Interface pointer version of IsInQueue
//			- asks if a request is in this queue
//-----------------------------------------------------------------------------
bool CAsyncRequestQueue::IsInQueueIp( const IAsyncRequestBase* pInterfaceBase )
{
	AUTO_LOCK_FM( m_Mutex );		// synchronize since we are scanning the queue?

	if ( pInterfaceBase == NULL || m_pHead == NULL ) return false;
	
	CAsyncRequestBase* pCur = m_pHead;
	while ( pCur != NULL )
	{
		if ( ( const IAsyncRequestBase* ) ( pCur->GetOuter() ) == pInterfaceBase ) return true;
		pCur = pCur->m_pNext;
	}
	
	return false;

}


		
//-----------------------------------------------------------------------------
// CAsyncRequestQueue::AddToHead
//		
//-----------------------------------------------------------------------------
void CAsyncRequestQueue::AddToHead( CAsyncRequestBase* pItem )
{
	AUTO_LOCK_FM( m_Mutex );
		
	// Empty queue?
	if ( m_pHead == NULL )
	{
		Assert( pItem != NULL );
		Assert( m_pHead == m_pTail && m_nQueueSize == 0 );
		
		m_pHead = m_pTail = pItem;
		pItem->m_pPrev = NULL;
		pItem->m_pNext = NULL;
		m_nQueueSize = 1;
	}
	else
	{
		pItem->m_pPrev = NULL;
		pItem->m_pNext = m_pHead;
		m_pHead = pItem;
		pItem->m_pNext->m_pPrev = pItem;	// Fixup previous head item
		m_nQueueSize++;
	}
	
}	


//-----------------------------------------------------------------------------
// CAsyncRequestQueue::AddToTail
//		
//-----------------------------------------------------------------------------
void CAsyncRequestQueue::AddToTail( CAsyncRequestBase* pItem )
{
	AUTO_LOCK_FM( m_Mutex );
		
	// Empty queue?
	if ( m_pTail == NULL )
	{
		Assert( pItem != NULL );
		Assert( m_pHead == m_pTail && m_nQueueSize == 0 );
		
		m_pHead = m_pTail = pItem;
		pItem->m_pPrev = NULL;
		pItem->m_pNext = NULL;
		m_nQueueSize = 1;
	}
	else
	{
		pItem->m_pPrev = m_pTail;
		pItem->m_pNext = NULL;
		m_pTail = pItem;
		pItem->m_pPrev->m_pNext = pItem;	// Fixup previous tail item
		m_nQueueSize++;
	}
	
}	


//-----------------------------------------------------------------------------
// CAsyncRequestQueue::InsertBefore
//		
//-----------------------------------------------------------------------------
void CAsyncRequestQueue::InsertBefore( CAsyncRequestBase* pItem, CAsyncRequestBase* pInsertAt )
{
	AUTO_LOCK_FM( m_Mutex );
	
	if ( pInsertAt == NULL || m_nQueueSize == 0 || pItem == NULL || !IsInQueue( pInsertAt ) )
	{
		return;
	}
	
	// Inserting at the head of the queue?
	if ( pInsertAt == m_pHead ) 
	{
		AddToHead( pItem );
	}
	else // is somewhere in the middle of the list
	{
		CAsyncRequestBase* pPrev = pInsertAt->m_pPrev;
		pPrev->m_pNext = pItem;
		pInsertAt->m_pPrev = pItem;
		pItem->m_pPrev = pPrev;
		pItem->m_pNext = pInsertAt;
		m_nQueueSize++;
	}

};


//-----------------------------------------------------------------------------
// CAsyncRequestQueue::InsertAfter
//		
//-----------------------------------------------------------------------------
void CAsyncRequestQueue::InsertAfter( CAsyncRequestBase* pItem, CAsyncRequestBase* pInsertAt )
{
	AUTO_LOCK_FM( m_Mutex );
	
	if ( pInsertAt == NULL || m_nQueueSize == 0 || pItem == NULL ||  !IsInQueue( pInsertAt ) )
	{
		return;
	}
	
	// Inserting at the end of the queue?
	if ( pInsertAt == m_pTail ) 
	{
		AddToTail( pItem );
	}
	else // is somewhere in the middle of the list
	{
		CAsyncRequestBase* pNext = pInsertAt->m_pNext;
		pNext->m_pPrev = pItem;
		pInsertAt->m_pNext = pItem;
		pItem->m_pPrev = pInsertAt;
		pItem->m_pNext = pNext;
		m_nQueueSize++;
	}
}
	
	
//-----------------------------------------------------------------------------
// CAsyncRequestQueue::PriorityInsert
//		- inserts an async request in the list (assumed sorted) by priority
//-----------------------------------------------------------------------------
void CAsyncRequestQueue::PriorityInsert( CAsyncRequestBase* pItem )
{
	AUTO_LOCK_FM( m_Mutex );
	
	if ( pItem == NULL )
	{	
		return;
	}
	
	CAsyncRequestBase* pCur = m_pHead;
	bool bInserted = false;
	
	while ( pCur != NULL )
	{
		if ( pCur->GetPriority() < pItem->GetPriority() )
		{
			InsertBefore( pItem, pCur );
			bInserted = true;
			break;
		}
		pCur = pCur->m_pNext;
	}

	// Did not find a lower priority item to insert before?	
	if ( !bInserted )
	{
		AddToTail( pItem );
	}
	
}
	
											
//-----------------------------------------------------------------------------
// CAsyncRequestQueue::Remove
//		
//-----------------------------------------------------------------------------
void CAsyncRequestQueue::Remove( CAsyncRequestBase* pItem )											
{
	AUTO_LOCK_FM( m_Mutex );
		
	if ( pItem == NULL || m_nQueueSize == 0 || !IsInQueue( pItem ) )
	{
		return;
	}
	
	if ( m_nQueueSize == 1 )		// emptying queue?
	{
		m_pHead = m_pTail = NULL;
	}
	else
	{
		// removing head?
		if ( pItem->m_pPrev == NULL )
		{
			Assert( pItem == m_pHead );
			m_pHead = pItem->m_pNext;
		}
		else
		{
			pItem->m_pPrev->m_pNext = pItem->m_pNext;
		}
		// removing tail?
		if ( pItem->m_pNext == NULL )
		{
			Assert( pItem == m_pTail );
			m_pTail = pItem->m_pPrev;
		}
		else
		{
			pItem->m_pNext->m_pPrev = pItem->m_pPrev;
		}
	}
	m_nQueueSize--;
	
	pItem->m_pNext = NULL;			// clear links inside removed request
	pItem->m_pPrev = NULL;

}



//-----------------------------------------------------------------------------
// CAsyncRequestQueue::RemoveHead
//		Pops the first item off the queue and returns it
//-----------------------------------------------------------------------------
CAsyncRequestBase* CAsyncRequestQueue::RemoveHead()											
{
	AUTO_LOCK_FM( m_Mutex );
	
	if ( m_nQueueSize == 0 || m_pHead == NULL )
	{
		return NULL;
	}
	
	CAsyncRequestBase* pItem = m_pHead;	
	Remove( pItem );
	
	return pItem;
}










//-----------------------------------------------------------------------------
// CAsyncFileSystem::constructor
//	
//-----------------------------------------------------------------------------
CAsyncFileSystem::CAsyncFileSystem()
{
	m_nJobsInflight = 0;
	m_nSuspendCount	= 0;
	
	m_bIOSuspended = false;
	
}

//-----------------------------------------------------------------------------
// CAsyncFileSystem::destructor
//	
//-----------------------------------------------------------------------------
CAsyncFileSystem::~CAsyncFileSystem()
{	
	m_nSuspendCount = 1;			// don't let any jobs slip status on us
	AbortAllAsyncIO( true );		// finish any work in flight
	
	// Queue destructors will release the requests
}

											

//-----------------------------------------------------------------------------
// CAsyncFileSystem::QueryInterface
//	
//-----------------------------------------------------------------------------
void *CAsyncFileSystem::QueryInterface( const char *pInterfaceName )
{
	// Make sure we are asking for the right version	
	if ( !Q_strncmp(	pInterfaceName, ASYNCFILESYSTEM_INTERFACE_VERSION, Q_strlen( ASYNCFILESYSTEM_INTERFACE_VERSION ) + 1) )
	{
		return ( IAsyncFileSystem* ) this;
	}

	return NULL;
}



//-----------------------------------------------------------------------------
// CAsyncFileSystem::CreateNewFileRequest
//	 Gets a new async read/write request object
//-----------------------------------------------------------------------------
IAsyncFileRequest* CAsyncFileSystem::CreateNewFileRequest()
{
	// Allocate a new request object, it's constructor will set everything up
	CAsyncFileRequest* pNewRequest = new CAsyncFileRequest;
	
	// add it to our list of requests being composed
	m_Composing.AddToTail( &pNewRequest->m_Base );
	
	// pass back the public interface pointer
	return pNewRequest;
}



//-----------------------------------------------------------------------------
// CAsyncFileSystem::CreateNewSearchRequest
//	 Gets a new async directory search request object
//-----------------------------------------------------------------------------
IAsyncSearchRequest* CAsyncFileSystem::CreateNewSearchRequest()
{
	// Allocate a new request object, it's constructor will set everything up
	CAsyncSearchRequest* pNewRequest = new CAsyncSearchRequest;
	
	// add it to our list of requests being composed
	m_Composing.AddToTail( &pNewRequest->m_Base );
	
	// pass back the public interface pointer
	return pNewRequest;
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::CreateNewAsyncRequestGroup
//	 Gets a new async request group object
//-----------------------------------------------------------------------------
IAsyncGroupRequest*	CAsyncFileSystem::CreateNewAsyncRequestGroup()
{
	// Allocate a new request object, it's constructor will set everything up
	CAsyncGroupRequest* pNewRequest = new CAsyncGroupRequest;
	
	// add it to our list of requests being composed
	m_Composing.AddToTail( &pNewRequest->m_Base );
	
	// pass back the public interface pointer
	return pNewRequest;

}



		
//-----------------------------------------------------------------------------
// CAsyncFileSystem::ReleaseAsyncRequest
//	 Disposes of a File Request Object
//   Public interface 
//-----------------------------------------------------------------------------
void CAsyncFileSystem::ReleaseAsyncRequest( const IAsyncRequestBase* pRequest )
{
	CAsyncRequestBase* pRequestBase = ( (IAsyncRequestBase* ) pRequest )->GetBase();
	
	AUTO_LOCK_FM ( m_AsyncStateUpdateMutex );

	if ( !ValidateRequestPtr( pRequestBase ) )
	{
		Error( "Bad Release Request" );
		return;
	}

	// If we are servicing the request, just mark it to abort when done	
	if ( pRequestBase->m_RequestState == ASYNC_REQUEST_STATE_SERVICING ) 
	{
		pRequestBase->m_bAbortRequest = true;		// have it delete itself when done
		return;
	}

	// If we are processing a callback, wait until finished
	if ( pRequestBase->m_bProcessingCallback )
	{
		pRequestBase->m_bDontAutoRelease = false;
		return;
	}

	// Delete the request object
	pRequestBase->DeleteOuter();
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::SubmitAsyncIORequest
//	
//-----------------------------------------------------------------------------
AsyncRequestStatus_t CAsyncFileSystem::SubmitAsyncFileRequest( const IAsyncRequestBase* pRequest )
{
	CAsyncRequestBase* pRequestBase = ( (IAsyncRequestBase* ) pRequest )->GetBase();

	// Validate the request from the user....
	AsyncRequestStatus_t RStatus = ValidateRequest( pRequestBase, false );
	
	// An error is preventing this from being submitted
	if ( RStatus != ASYNC_REQUEST_OK )
	{
		m_AsyncStateUpdateMutex.Lock();
		{
			// Move the job to the completed queue
			m_Composing.Remove( pRequestBase );
			m_Completed.AddToTail( pRequestBase );
	
			// mark request as completed, with error
			pRequestBase->m_RequestState = ASYNC_REQUEST_STATE_COMPLETED;
			pRequestBase->m_RequestStatus = RStatus;
		}		
		m_AsyncStateUpdateMutex.Unlock();

		// notify any designed message queue and/or callback
		NotifyMessageQueueOrCallback( pRequestBase );
		
		// notify the submitter as well
		return RStatus;
	}
	
	m_AsyncStateUpdateMutex.Lock();
	{
		// Move the job to the submitted queue and insert according to priority
		m_Composing.Remove( pRequestBase );
		pRequestBase->m_RequestState = ASYNC_REQUEST_STATE_SUBMITTED;
		m_Submitted.PriorityInsert( pRequestBase );
	}		
	m_AsyncStateUpdateMutex.Unlock();
	
	// See if we can hand this request off to the old async system
	KickOffFileJobs();
	
	return RStatus;
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::SubmitSyncIORequest
//	
//-----------------------------------------------------------------------------
AsyncRequestStatus_t CAsyncFileSystem::SubmitSyncFileRequest( const IAsyncRequestBase* pRequest )
{
	CAsyncRequestBase* pRequestBase = ( (IAsyncRequestBase* ) pRequest)->GetBase();

	// Validate the request from the user....
	AsyncRequestStatus_t RStatus = ValidateRequest( pRequestBase, true );
	if ( RStatus != ASYNC_REQUEST_OK )
	{
		m_AsyncStateUpdateMutex.Lock();
		{
			// Move the job to the completed queue
			m_Composing.Remove( pRequestBase );
			m_Completed.AddToTail( pRequestBase );
	
			// mark request as completed, with error
			pRequestBase->m_RequestState = ASYNC_REQUEST_STATE_COMPLETED;
			pRequestBase->m_RequestStatus = RStatus;
		}		
		m_AsyncStateUpdateMutex.Unlock();
	
		return RStatus;
	}

	CThreadEvent syncEvent;
	
	// Move the job to the submitted queue and insert according to priority

	m_AsyncStateUpdateMutex.Lock();
	{
		m_Composing.Remove( pRequestBase );
		pRequestBase->m_RequestState = ASYNC_REQUEST_STATE_SUBMITTED;

		// We are making a local thread event, so this routine can be called simultaneously / overlapping
		// by multiple threads.  I know it is "expensive" to new up a CThreadEvent, but we want to discourage
		// people from doing this anyway...
		pRequestBase->m_pSyncThreadEvent = &syncEvent;
		
		m_Submitted.PriorityInsert( pRequestBase );
	}		
	m_AsyncStateUpdateMutex.Unlock();

	// Submit to the async system
	KickOffFileJobs();

	// Wait for the job to finish, and for us to be signaled
	pRequestBase->m_pSyncThreadEvent->Wait();

	// Get the IO status to return....  (user has to delete the object anyway)	
	RStatus = pRequestBase->GetRequestStatus();
	
	return RStatus;
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::KickOffFileJobs
//	 hand off async jobs to the old async file system
//-----------------------------------------------------------------------------
void CAsyncFileSystem::KickOffFileJobs()
{
	// Are we suspended?
	if ( m_nSuspendCount > 0 )
	{	
		return;
	}

#if 0
	// Are the too many requests already being serviced?
	// TODO - throttle at 2 jobs?
	if ( m_nJobsInflight >= 1 )
	{	
		return;
	}

#endif

	m_AsyncStateUpdateMutex.Lock();

	// are there any requests waiting to be submitted?
	if ( m_Submitted.GetSize() < 1 )
	{	
		m_AsyncStateUpdateMutex.Unlock();
		return;
	}

	// ok.  translate our request to the current system

	CAsyncRequestBase* pRequestBase = m_Submitted.RemoveHead();

	FileAsyncRequest_t asJobReq;
	
	CAsyncFileRequest* pFileRequest = NULL;
	CAsyncSearchRequest* pSearchRequest = NULL;
	
	
	char* pSearchSpec = NULL;
	bool bRecurseFolders = false;
	
	if ( pRequestBase->GetAsyncOperationType() == ASYNC_OP_READFILE ||
		 pRequestBase->GetAsyncOperationType() == ASYNC_OP_WRITEFILE || 
		 pRequestBase->GetAsyncOperationType() == ASYNC_OP_APPENDFILE )
	{
		// Get the FileRequest that contains the base
		pFileRequest = reinterpret_cast< CAsyncFileRequest* >( pRequestBase->GetOuter() );
		

		// IO operation size is smaller of optionally supplied buffer size and optionally supplied IO size	
		
		int nIOMax = 0;
		if ( pFileRequest->GetUserBuffer() != NULL )	
		{
			nIOMax = pFileRequest->GetUserBufferSize();
			if ( pFileRequest->m_nMaxIOSizeInBytes > 0 && pFileRequest->m_nMaxIOSizeInBytes > nIOMax )
			{
				Error( "Buffer not big enough to hold requested File IO" );
			}
		}
		if ( pFileRequest->m_nMaxIOSizeInBytes > 0 && pFileRequest->m_nMaxIOSizeInBytes < nIOMax ) 
		{
			nIOMax = pFileRequest->m_nMaxIOSizeInBytes;
		}
		
		// translate to old request format...
		asJobReq.pszFilename = pFileRequest->GetFileName();
		asJobReq.pData = pFileRequest->GetUserBuffer();
		asJobReq.nOffset = pFileRequest->m_nFileSeekOffset;
		asJobReq.nBytes = nIOMax;
		asJobReq.pfnCallback = &CAsyncFileSystem::AsyncIOCallbackGateway;
		asJobReq.pContext = (void*) pRequestBase;
		asJobReq.priority = 0;
		asJobReq.flags = ( pFileRequest->GetUserBuffer() == NULL ) ? FSASYNC_FLAGS_ALLOCNOFREE : 0 ;
		asJobReq.pszPathID = NULL;
		asJobReq.hSpecificAsyncFile = FS_INVALID_ASYNC_FILE;
		asJobReq.pfnAlloc = &CAsyncFileSystem::OldAsyncAllocatorCallback;
	}
	
	if ( pRequestBase->GetAsyncOperationType() == ASYNC_OP_SCANDIR )
	{
		// Get the Search Request that contains the base
		pSearchRequest = reinterpret_cast< CAsyncSearchRequest* >( pRequestBase->GetOuter() );
		
		pSearchSpec = pSearchRequest->m_SearchSpec;
		bRecurseFolders = pSearchRequest->m_bRecurseSubdirs;
		
		
		
		
	
	}
	
	m_InFlight.AddToTail( pRequestBase );
	
	m_nJobsInflight++;
	pRequestBase->m_RequestState = ASYNC_REQUEST_STATE_SERVICING;
	
	// Make the call based on what sort of job is it...
	FSAsyncStatus_t	jStatus = FSASYNC_OK;

	switch ( pRequestBase->GetAsyncOperationType() )
	{
		case ASYNC_OP_READFILE:
		{
			jStatus = g_pFullFileSystem->AsyncRead( asJobReq, &(pRequestBase->m_pOldAsyncControl) );
			break;
		}
		case ASYNC_OP_WRITEFILE:
		{
			jStatus = g_pFullFileSystem->AsyncWrite( asJobReq.pszFilename, asJobReq.pData, pFileRequest->GetResultBufferSize(), false, false, &(pRequestBase->m_pOldAsyncControl) );
			break;
		}
		case ASYNC_OP_SCANDIR:
		{
			jStatus = g_pFullFileSystem->AsyncDirectoryScan( pSearchSpec, bRecurseFolders, pRequestBase, &CAsyncFileSystem::AsyncSearchCallbackAddItem, &CAsyncFileSystem::AsyncSearchCallbackGateway, &(pRequestBase->m_pOldAsyncControl) );
			break;
		}
		default:
		{
			Error( "Unable to determing async job type" );
			break;
		}
	}
	
	if ( jStatus != FSASYNC_OK )
	{
		// Error translating...
		Error( "Basefilesystem Async Job submission failed" );
		
		// TO-DO: go straight to notification/callback with failure info
	}
	
	m_AsyncStateUpdateMutex.Unlock();

}

//-----------------------------------------------------------------------------
// STATIC CAsyncFileSystem::OldAsyncAllocatorCallback
//	 - receives callbacks from the old Async file system 
//     allocates a buffer from the new async system
//-----------------------------------------------------------------------------
void* CAsyncFileSystem::OldAsyncAllocatorCallback( const char *pszFilename, unsigned nBytes )
{
	return ((CAsyncFileSystem*) g_pAsyncFileSystem)->AllocateBuffer( nBytes, 16 );
}


//-----------------------------------------------------------------------------
// STATIC CAsyncFileSystem::AsyncIOCallbackGateway
//	 - receives callbacks from the old Async file system 
//     Thunks the callback back into the context of the CAsyncFileSystem object
//-----------------------------------------------------------------------------
void CAsyncFileSystem::AsyncIOCallbackGateway(const FileAsyncRequest_t &request, int nBytesRead, FSAsyncStatus_t err)
{
	CAsyncRequestBase* pRequest = static_cast<CAsyncRequestBase*>( request.pContext );

	// Pack the results we care about into our own structure	
	CAsyncResultInfo_t results;
	results.m_nBytesTransferred = nBytesRead;
	results.m_pAllocatedBuffer = request.pData;
	results.m_ErrorCode = err;

	// gateway back to the CAsyncFileSystem object instance
	((CAsyncFileSystem*) g_pAsyncFileSystem)->AsyncIOCallBackHandler( pRequest, results );
}


//-----------------------------------------------------------------------------
// STATIC CAsyncFileSystem::AsyncSearchCallbackGateway
//	 - receives callbacks from the old Async file system search function
//     Thunks the callback back into the context of the CAsyncFileSystem object
//-----------------------------------------------------------------------------
void CAsyncFileSystem::AsyncSearchCallbackGateway( void* pContext, FSAsyncStatus_t err )
{
	CAsyncRequestBase* pRequest = static_cast<CAsyncRequestBase*>( pContext );
	
	// Pack the results we care about into our own structure	
	CAsyncResultInfo_t results;
	results.m_ErrorCode = err;
	
	// gateway back to the CAsyncFileSystem object instance
	((CAsyncFileSystem*) g_pAsyncFileSystem)->AsyncIOCallBackHandler( pRequest, results );
}



//-----------------------------------------------------------------------------
// CAsyncFileSystem::AsyncIOCallBackReadJob
//	 - Handler for a completed file Read Job
//-----------------------------------------------------------------------------
void CAsyncFileSystem::AsyncIOCallBackHandler(CAsyncRequestBase* pRequest, CAsyncResultInfo_t& results )
{
	// Get the base info from the request
	// CAsyncRequestBase* pRequest = static_cast<CAsyncRequestBase*>( request.pContext );
	
	if ( !m_InFlight.IsInQueue( pRequest ) )
	{
		Error( "Can't find completed Async IO request" );
		return;
	}

	// Move the request to the completed list
	m_AsyncStateUpdateMutex.Lock();
	
	m_InFlight.Remove( pRequest );
	pRequest->m_RequestState = ASYNC_REQUEST_STATE_AWATING_FINISH;
	
	m_nJobsInflight--;

	// Were we asked to abort while we were servicing?
	if ( pRequest->m_bAbortRequest )
	{
		pRequest->AbortAfterServicing( results );			// Clean up any work in progress
		pRequest->DeleteOuter();							// Destroy the request
		
		m_AsyncStateUpdateMutex.Unlock();		
		KickOffFileJobs();									// go on to next ho
		return;
	}
	
	// we're not aborting, so do the task specific work
	pRequest->UpdateAfterServicing( results );

	// handle the error results from the old system
	pRequest->m_pOldAsyncStatus = results.m_ErrorCode;
	
	switch ( results.m_ErrorCode )
	{
		case FSASYNC_ERR_ALIGNMENT:
		{
			pRequest->m_RequestStatus = ASYNC_REQUEST_ERROR_ALIGNMENT;
			break;	
		}
		case FSASYNC_ERR_FAILURE:
		{
			pRequest->m_RequestStatus = ASYNC_REQUEST_ERROR_FAILURE; 
			break;
		}
		case FSASYNC_ERR_READING:
		{
			pRequest->m_RequestStatus = ASYNC_REQUEST_ERROR_READING; 
			break;
		}
		case FSASYNC_ERR_NOMEMORY:
		{
			pRequest->m_RequestStatus = ASYNC_REQUEST_ERROR_NOMEMORY; 
			break;
		}
		case FSASYNC_ERR_UNKNOWNID:
		{
			pRequest->m_RequestStatus = ASYNC_REQUEST_ERROR_UNKNOWNID; 
			break;
		}
		case FSASYNC_ERR_FILEOPEN:
		{
			pRequest->m_RequestStatus = ASYNC_REQUEST_ERROR_FILEOPEN; 
			break;
		}
		case FSASYNC_OK:
		{
			pRequest->m_RequestStatus = ASYNC_REQUEST_OK; 
			break;	
		}
		case FSASYNC_STATUS_PENDING:		
		case FSASYNC_STATUS_INPROGRESS:		
		case FSASYNC_STATUS_ABORTED:		
		case FSASYNC_STATUS_UNSERVICED:		
		default:
		{
			Error( "Async result Status makes no sense" );
			pRequest->m_RequestStatus = ASYNC_REQUEST_OK;
			break;
		}
	};

	m_Completed.AddToTail( pRequest );

	m_AsyncStateUpdateMutex.Unlock();

	// Notify anyone who is listening that an IO has completed
	m_CompletionSignal.Set();
	
	// if we have other jobs in the priority queue, we have the ability to submit them now
	KickOffFileJobs();

	// Was this a synchronous operation?
	if ( pRequest->m_pSyncThreadEvent != NULL )
	{
		// wake up the calling thread...
		pRequest->m_pSyncThreadEvent->Set();
	}
	else
	{
		// This was an Async operation, so we notify anyone who cares...
		NotifyMessageQueueOrCallback( pRequest );
	}
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::NotifyMessageQueueOrCallback
//    Places a notification in the designated message queue.
//    if no queue is specified, a callback can be directly performed
//	
//-----------------------------------------------------------------------------
void CAsyncFileSystem::NotifyMessageQueueOrCallback( CAsyncRequestBase* pRequest )
{
	// ok, do we have an IO completion queue to add this to?
	if ( pRequest->m_pResultQueue != NULL )	
	{
		CAsyncIOResult_t result;
		result.m_pRequest = pRequest->GetInterfaceBase();
		result.m_Status	= pRequest->GetRequestStatus();	
		
		pRequest->m_pResultQueue->Insert( result );
	}
	else	// otherwise we want to do the callback directly from this thread?
	{
		pRequest->ProcessCallback( true );			// remove request upon completion
	}
}





//-----------------------------------------------------------------------------
// STATIC CAsyncFileSystem::AsyncSearchCallbackAddItem
//	 - receives callbacks from the old Async file system directory search 
//     Adds the results to the desired list
//-----------------------------------------------------------------------------
void CAsyncFileSystem::AsyncSearchCallbackAddItem( void* pContext, char* pFoundPath, char* pFoundFile )
{
	CAsyncSearchRequest* pRequest = reinterpret_cast< CAsyncSearchRequest* >( ((CAsyncRequestBase*) pContext)->GetOuter() );

	CDirectoryEntryInfo_t result;
	V_ComposeFileName( pFoundPath, pFoundFile, result.m_FullFileName, sizeof(result.m_FullFileName) );
	
	pRequest->m_Results.AddToTail( result );
}



//-----------------------------------------------------------------------------
// CAsyncFileSystem::GetAsyncIORequestStatus
//	
//-----------------------------------------------------------------------------
AsyncRequestStatus_t CAsyncFileSystem::GetAsyncFileRequestStatus( const IAsyncRequestBase* pRequest )
{
	CAsyncRequestBase* pRequestBase = ( (IAsyncRequestBase* ) pRequest)->GetBase();

	if ( !ValidateRequestPtr( pRequestBase ) )
	{
		return ASYNC_REQUEST_ERROR_BADPTR;
	}

	return pRequestBase->m_RequestStatus;
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::AbortAsyncIORequest
//	
//-----------------------------------------------------------------------------
AsyncRequestStatus_t CAsyncFileSystem::AbortAsyncFileRequest( const IAsyncRequestBase* pRequest )
{
	AUTO_LOCK_FM( m_AsyncStateUpdateMutex );
	
	CAsyncRequestBase*	pRequestBase = NULL;
	AsyncRequestState_t CurrentStage = ASYNC_REQUEST_STATE_UNDEFINED;

	// determine if the pointer is still valid, and get info on it if so
	if ( !ResolveAsyncRequest( pRequest, pRequestBase, CurrentStage ) )
	{
		return ASYNC_REQUEST_ERROR_BADPTR;
	}

	switch( CurrentStage )
	{
		case ASYNC_REQUEST_STATE_COMPOSING:
		{
			return ASYNC_REQUEST_ERROR_NOTSUBMITTED;
		}
		case ASYNC_REQUEST_STATE_SUBMITTED:
		{
			pRequestBase->DeleteOuter();	// destroy request, it's destructor will remove it from m_Submitted
			return ASYNC_REQUEST_OK;
		
		}
		case ASYNC_REQUEST_STATE_SERVICING:
		{
			// Need to flag it for auto-release once it completes
			return ASYNC_REQUEST_ERROR_NOTSUBMITTED;
		}
		
		// already completed, nothing to abort
		case ASYNC_REQUEST_STATE_AWATING_FINISH:
		case ASYNC_REQUEST_STATE_COMPLETED:
		{
			return ASYNC_REQUEST_ERROR_ALREADYSERVICED;
		}
		default:
		{
			return ASYNC_REQUEST_ERROR_BADPTR;
		}
	}

}



//-----------------------------------------------------------------------------
// CAsyncFileSystem::ResolveAsyncRequest - take an interface pointer supplied
//	  by a caller and 1) Determine if it is valid and 2) return base info if so
//-----------------------------------------------------------------------------
bool CAsyncFileSystem::ResolveAsyncRequest( const IAsyncRequestBase* pRequest, CAsyncRequestBase*& pRequestBase, AsyncRequestState_t& CurrentStage )
{
	// We should only be called when the Mutex is taken
	Assert( m_AsyncStateUpdateMutex.GetDepth() > 0 );

	// search our queues for the pointer
	// We do it the slow way, on the rare chance that the pointer pRequest points to invalid memory
	// Once we know it is valid, we can probe it.

	if ( m_Composing.IsInQueueIp( pRequest ) )
	{
		pRequestBase = ( (IAsyncRequestBase* ) pRequest)->GetBase();
		CurrentStage = ASYNC_REQUEST_STATE_COMPOSING;
		return true;
	}

	if ( m_Submitted.IsInQueueIp( pRequest ) )
	{
		pRequestBase = ( (IAsyncRequestBase* ) pRequest)->GetBase();
		CurrentStage = ASYNC_REQUEST_STATE_SUBMITTED;
		return true;
	}
	
	if ( m_InFlight.IsInQueueIp( pRequest ) )
	{
		pRequestBase = ( (IAsyncRequestBase* ) pRequest)->GetBase();
		CurrentStage = ASYNC_REQUEST_STATE_SERVICING;
		return true;
	}
	
	if ( m_Completed.IsInQueueIp( pRequest ) )
	{
		pRequestBase = ( (IAsyncRequestBase* ) pRequest)->GetBase();
		CurrentStage = ( (IAsyncRequestBase* ) pRequest)->GetRequestState();
		return true;
	}

	pRequestBase = NULL;
	CurrentStage = ASYNC_REQUEST_STATE_UNDEFINED;
	return false;

}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::RemoveRequest
//	  - remove request from any queue (assume it is being deleted)
//-----------------------------------------------------------------------------
void CAsyncFileSystem::RemoveRequest( CAsyncRequestBase* pRequest )
{
	AUTO_LOCK_FM( m_AsyncStateUpdateMutex );
	
	// Is status trustworthy?  maybe not?

	switch ( pRequest->m_RequestState )
	{
		case ASYNC_REQUEST_STATE_COMPOSING:
		{
			m_Composing.Remove( pRequest );
			break;
		}
		case ASYNC_REQUEST_STATE_SUBMITTED:
		{
			m_Submitted.Remove( pRequest );
			break;
		}
		case ASYNC_REQUEST_STATE_SERVICING:		
		{
			m_InFlight.Remove( pRequest);
			break;
		}
		case ASYNC_REQUEST_STATE_AWATING_FINISH:
		case ASYNC_REQUEST_STATE_COMPLETED:
		{
			m_Completed.Remove( pRequest );
			break;
		}
		default:
		{
			Error( " Couldn't Find Async Request to Remove" );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::AsyncIOSuspendAll
//	  - will suspend the servicing of queued file requests
//    - returns immediately unless bWaitForIOCompletion set, in which case
//        returns after all jobs in the process of being serviced complete
//-----------------------------------------------------------------------------
void CAsyncFileSystem::SuspendAllAsyncIO(  bool bWaitForIOCompletion )
{
	m_nSuspendCount++;

	if ( bWaitForIOCompletion && m_nSuspendCount > 0 )
	{
		WaitForServincingIOCompletion();
	}
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::ResumeAllAsyncIO
//	 - Cancels a previous request to suspect all Async IO
//   - resumes the servicing of async jobs that are queued
//-----------------------------------------------------------------------------
void CAsyncFileSystem::ResumeAllAsyncIO()
{
	Assert ( m_nSuspendCount > 0 );
	
	m_nSuspendCount--;
	
	if ( m_nSuspendCount <= 0 )
	{
		Assert( m_nSuspendCount == 0 );
		m_nSuspendCount = 0;
		KickOffFileJobs();
	}
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::AsyncIOAbortAll
//	    aborts all file requests submitted but not serviced
//      requests that have been created, but not submitted are unaffected
//-----------------------------------------------------------------------------
AsyncRequestStatus_t CAsyncFileSystem::AbortAllAsyncIO( bool bWaitForIOCompletion )
{
	{
		AUTO_LOCK_FM( m_AsyncStateUpdateMutex );

		if ( m_Submitted.GetSize() <= 0 )
		{
			// return immediately if we're not waiting on jobs to finish 
			if ( m_nJobsInflight == 0 || bWaitForIOCompletion == false )
			{
				return ASYNC_REQUEST_OK;
			}
		}
	
		// Remove all jobs from the priority queue and discard
		while ( m_Submitted.GetSize() > 0 )
		{
			CAsyncRequestBase* pRequest = m_Submitted.RemoveHead();
			pRequest->DeleteOuter();		// Destroy the request
		}

		// Tell jobs currently being serviced to abort when finished
		while ( m_InFlight.GetSize() > 0 )
		{
			CAsyncRequestBase* pRequest = m_InFlight.RemoveHead();
			pRequest->m_bAbortRequest = true;
		}
	}
	
	if ( bWaitForIOCompletion )
	{
		WaitForServincingIOCompletion();
	}


	return ASYNC_REQUEST_OK;

}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::BlockUntilAsyncIOComplete
//      Suspends the calling thread until a pending IO completes
//-----------------------------------------------------------------------------
bool CAsyncFileSystem::BlockUntilAsyncIOComplete( const IAsyncRequestBase* pRequest )
{
	//CAsyncRequestBase* theRequest = reinterpret_cast<CAsyncRequestBase*>( const_cast<IAsyncRequestBase*>( pRequest ) );
	
	CAsyncRequestBase* pRequestBase = ( (IAsyncRequestBase* ) pRequest)->GetBase();

	// Validate the request from the user....
	if ( !ValidateRequestPtr( pRequestBase ) )
	{
		return false;
	}
	
	// Can't block while all IO is turned off
	if ( m_bIOSuspended )
	{
		return false;
	}

	// has to be submitted
	if ( pRequestBase->GetRequestState() <= ASYNC_REQUEST_STATE_COMPOSING )
	{
		return false;
	}

	// Ok, we have to grab the mutex to safely check the request state
	bool waitforIt = false;
	CThreadEvent syncEvent;
	
	m_AsyncStateUpdateMutex.Lock();
	// Can we just add the signaling request?
	if ( pRequestBase->GetRequestState() < ASYNC_REQUEST_STATE_AWATING_FINISH )
	{
		Assert( pRequestBase->m_pSyncThreadEvent);
		pRequestBase->m_pSyncThreadEvent = &syncEvent;
		waitforIt = true;
	}
	m_AsyncStateUpdateMutex.Unlock();
	
	// ok, no matetr what the request has the event set before it was finished, so even if
	// it finished in the meantime, the thred event will get signaled and it won't notify a completion queue
	if ( waitforIt )
	{
		pRequestBase->m_pSyncThreadEvent->Wait();
		NotifyMessageQueueOrCallback( pRequestBase );
	}
	
	return true;

}

		
//-----------------------------------------------------------------------------
// CAsyncFileSystem::WaitForServincingIOCompletion
//	    pauses the calling thread until all IO jobs being serviced
//      by the old system are completed..
//-----------------------------------------------------------------------------
void CAsyncFileSystem::WaitForServincingIOCompletion()
{
	while ( m_nJobsInflight > 0 )
	{
		m_CompletionSignal.Wait( 10 );
	}
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::AllocateBuffer
//	
//-----------------------------------------------------------------------------
void* CAsyncFileSystem::AllocateBuffer( size_t nBufferSize, int nAlignment)
{
	Assert( nAlignment >= 0 );
	Assert( ValueIsPowerOfTwo( nAlignment ) );

	void* pMem = MemAlloc_AllocAligned( nBufferSize, nAlignment );
 #if defined(_DEBUG)
 	// In debug mode we track allocations so we later can track down leaks, etc
 	m_MemoryTrackMutex.Lock();
 	m_AllocList.AddToTail( pMem );
 	m_MemoryTrackMutex.Unlock();
 #endif

	return pMem;
}



//-----------------------------------------------------------------------------
// CAsyncFileSystem::ReleaseBuffer
//	
//-----------------------------------------------------------------------------
void CAsyncFileSystem::ReleaseBuffer( void* pBuffer )
{
	Assert ( pBuffer != NULL );

#if defined(_DEBUG)
	// In debug mode we verify that we had actually allocated this
	m_MemoryTrackMutex.Lock();
	Assert( m_AllocList.FindAndFastRemove( pBuffer ) == true );
	m_MemoryTrackMutex.Unlock();
#endif

	MemAlloc_FreeAligned( pBuffer );
}




//-----------------------------------------------------------------------------
// CAsyncFileSystem::ValidateRequestPtr
//	
//-----------------------------------------------------------------------------
bool CAsyncFileSystem::ValidateRequestPtr( CAsyncRequestBase* pRequest )
{
	// can't let it slip state between lines here...
	AUTO_LOCK_FM( m_AsyncStateUpdateMutex );
	
	switch ( pRequest->m_RequestState )
	{
		case ASYNC_REQUEST_STATE_COMPOSING:
		{
			if ( m_Composing.IsInQueue( pRequest ) )
			{
				return true;
			}
		}
		case ASYNC_REQUEST_STATE_SUBMITTED:
		{
			if ( m_Submitted.IsInQueue( pRequest ) ) 
			{
				return true;
			}
		}
		case ASYNC_REQUEST_STATE_SERVICING:		
		{
			if ( m_InFlight.IsInQueue( pRequest ) )
			{
				return true;
			}
		}
		case ASYNC_REQUEST_STATE_AWATING_FINISH:
		case ASYNC_REQUEST_STATE_COMPLETED:
		{
			if ( m_Completed.IsInQueue( pRequest ) )
			{
				return true;
			}
		}
		default:
		{
			return false;
		}
	}
}


//-----------------------------------------------------------------------------
// CAsyncFileSystem::ValidateRequest
//	
//-----------------------------------------------------------------------------
AsyncRequestStatus_t CAsyncFileSystem::ValidateRequest( CAsyncRequestBase* pRequest, bool bPerformSync )
{

	// Is this a request we know about and is awaiting submission?
	if ( !m_Composing.IsInQueue( pRequest ) ) 
	{
		// Can we find it already being processed?
		if ( m_Submitted.IsInQueue( pRequest ) ||
		      m_InFlight.IsInQueue( pRequest ) || 
		      m_Completed.IsInQueue( pRequest ) )
		{
			return ASYNC_REQUEST_ERROR_ALREADYSUBMITTED;
		}
		return ASYNC_REQUEST_ERROR_BADPTR;			// We think the pointer is bad
	}

	// Now we check the contents of the request to make sure it makes sense...
	
	// file operation must be set
	if ( pRequest->GetAsyncOperationType() <= ASYNC_OP_UNDEFINED || pRequest->GetAsyncOperationType() >= ASYNC_OP_COUNT )
	{
		return ASYNC_REQUEST_ERROR_BADOPER;	
	}
	

	// Are we doing synchronous IO?
	if ( bPerformSync )
	{
		// No callback is allowable in sync mode
		if ( pRequest->m_pCallback != NULL || pRequest->m_pResultQueue != NULL )
		{
			// return ASYNC_REQUEST_ERROR_NOTVALIDSYNCRONOUS;
			// just ignore the callbacks and do the type specific validation
		}
	}
	else
	{
		// We need a completion queue, a callback or both
		if ( pRequest->m_pResultQueue == NULL && pRequest->m_pCallback == NULL )
		{
			return ASYNC_REQUEST_ERROR_NONOTIFICATION;	// must have a notification or callback
		}
	}

	// Do the request specific validation
	return pRequest->ValidateSubmittedRequest( bPerformSync );


}

//-----------------------------------------------------------------------------
// s_FileNameCharMap  - helper table for validating filenames and path processing
//  The values are control vales for ASCII character 0 to 127.  They are
//
//  0  - invalid character
//  1  - uppercase character (convert to lower and complain)
//  2  - valid anytime character
//  3  - path divider ( "\" or "/" )
//  4  - valid only in the middle of a name
//  5  - name extension character ( "." )
//  6  - drive specifier ":"
//	
//-----------------------------------------------------------------------------
byte s_FileNameCharMap[128] = {
// 0-31 - control codes
           0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,   0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
// 32-63  sp  !	 "  #   $  %  &  '   (  )  *  +   ,  -  .  /    0  1  2  3   4  5  6  7   8  9  :  ;   <  =  >  ?
           4, 2, 0, 2,  2, 2, 2, 2,  2, 2, 0, 0,  2, 2, 5, 3,   2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 6, 0,  0, 0, 0, 0,
// 64-95   @  A  B  C   D  E  F  G   H  I  J  K   L  M  N  O    P  Q  R  S   T  U  V  W   X  Y  Z  [   \  ]  ^  _
           2, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,   1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 0,  3, 0, 2, 2, 
// 96-127  `  a  b  c   d  e  f  g   h  i  j  k   l  m  n  o    p  q  r  s   t  u  v  w   x  y  z  {   |  }  ~  del
           2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 2,   2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 2, 2,  0, 2, 2, 0  };


//-----------------------------------------------------------------------------
// CAsyncFileSystem::ValidateFileName
//	  - examines the filename for problems, fixes or rejects it if so.
//  
//-----------------------------------------------------------------------------
AsyncRequestStatus_t ValidateFileName( const char* pFileName )
{
	// todo - this code doesn't examine each filename or path as closely as could be done
	// According to ChrisB, we shouldn't do all this validation as it's a time suck
	//  and our pack files won't care...
	
	int32	theLength = Q_strlen( pFileName );
	
	char*	theFilename = const_cast<char*>( pFileName );
	
	if ( theLength >= MAX_PATH )
	{
		return ASYNC_REQUEST_ERROR_PATHTOOLONG;
	}
	
	int32   lastPathSeparator = -1;
	int32	i = 0;
	bool	bUpperCaseWarn = false;
	
	while ( i < theLength )
	{
		char c = theFilename[i];
		if ( c > 250 )   // allow western european file chars...
		{
			
			return ASYNC_REQUEST_ERROR_INVALIDFILENAME;		
		}
		switch ((int32) s_FileNameCharMap[c])
		{
			case 0:	// invalid character
			{
				return ASYNC_REQUEST_ERROR_INVALIDFILENAME;		
			}
			case 1: // uppercase character
			{
				theFilename[i] = (char) (c + 0x20);		// convert to lower case
				bUpperCaseWarn = true;
				break;
			}
			case 2: // valid character
			{
				break;
			}
			case 3: // path divider
			{
				if ( lastPathSeparator >= 0 )
				{
					if ( i-1 == lastPathSeparator || i == theLength-1 )
					{
						return ASYNC_REQUEST_ERROR_BADPATHSPEC;
					}
				}
				lastPathSeparator = i;
				break;
			}
			case 4:  // valid only in the middle of a filename
			{
				if ( i-1 == lastPathSeparator )	// 1st character in this section?
					{
						return ASYNC_REQUEST_ERROR_INVALIDFILENAME;
					}
				break;
			}
			case 5:	 // extension separator
			{
				break;
			}
			case 6:	// drive : 
			{
				// Always needs to occur as the 2nd character in the name if at all
				if ( i != 1 )
				{
					return ASYNC_REQUEST_ERROR_BADPATHSPEC;
				}
				break;
			}
			default:
			{
				break;
			}
		}
	
		i++;
	}
	
	// check filename length
	int32 nameLen;
	if ( lastPathSeparator < 0 )
	{
		nameLen = i;
	}
	else
	{
		nameLen = i - lastPathSeparator -1;
	}
	
	if ( nameLen >= CAsyncFileSystem::s_MaxPlatformFileNameLength )			// 40 is maximum length for Xbox STFS
	{
		return ASYNC_REQUEST_ERROR_FILENAMETOOLONG;
	}

	// make sure the path characters are platform correct	
	if ( lastPathSeparator >= 0 )
	{
		Q_FixSlashes( theFilename );		// use the library to correct the path dividers for the OS
											// Yes, I know.. not as efficient as we could make it...
	}

	if ( bUpperCaseWarn )
	{
		Warning( "Linux/platform issue - filename '%s' contains uppercase\n", theFilename );
	}

	return ASYNC_REQUEST_OK;

}

