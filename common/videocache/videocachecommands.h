//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: Video Caching Command Messages
//			Enums, structs, objects
//
// Used by //utils/videocache.exe and any app the talks to it.
//
// $Header: $
// $NoKeywords: $
//===========================================================================//


#ifndef VIDEO_CACHE_COMMANDS_H
#define VIDEO_CACHE_COMMANDS_H

#include "videocache/videomaterialdefs.h"

// -----------------------------------------------------------------------
// Command type enumeration,   parameters specified in comments
// Required CVidCmd Parameters noted as follows:
// 
//  f0 - f2:   32-bit float parameters
//  i0 - I2:   32-bit int parameters
//  u0 - u2:   unsigned 32-bit int parameters
//  np = no additional parameters
//
//  Additional parameters that follow the CVidCmd struct are noted in "[]"
// -----------------------------------------------------------------------
enum eVidCacheCommand_t
{
	cVCC_Unknown			= 0,			// Used to indicate uninitialized command
	cVCC_Ping				= 1,			// request ack from video cache app:  np
	cVCC_Quit				= 2,			// request video cache app shutdown:  np
	cVCC_Ack				= 3,			// general purpose response :    np
	
	cVCC_BeginCacheSession	= 0x09,			// Request to begin a video caching session.	i0 = local Id  [CVideoCacheSessionInfo]
	cVCC_BeginCacheSessionResult = 0x0A,	// Begin Session Result:						i0 = local Id, b = Success bool, e = reason
	
	cVCC_EndCacheSession	= 0x0C,			// Request to end a video caching session		i0 = Session ID
	// Response is Ack
	
	cVCC_CacheFile			= 0x11,			// Request to cache a file:						i0 = Local Id, i1 = FileNameLen,  [filename string]
	cVCC_CacheFileResult	= 0x12,			// Cache file Result Status:					i0 = Local Id, b = Success bool, e = reason, u1 = Server Handle, [CVideoFrameInfo]
	
	cVCC_UncacheFile		= 0x17,			// Request to stop caching a file (no longer needed):  u0 = Server Handle		
	// Response is Ack
	cVCC_UncacheAllFiles	= 0x18,			// Request to unload all cached files (no longer needed):  np
	// Response is Ack
	
	cVCC_RequestFrame		= 0x21,			// Request a frame from a file:	u0 = Server Handle, f1 = Frame Time
	cVCC_RequestFrameResult = 0x22,			// Frame Request Result:		u0 = Server Handle, f1 = Frame time, b = Success bool, e = reason  [CVideoFrameInfo], [Bitmap Pixels]
	
	cVCC_BeginEncodingSession = 0x31,		// Begin a session to encode a movie:  i0 = Local Id, i1 = FileNameLen, [CVideoEncodeSessionParams] [filename string] 
	cVCC_BeginEncodingResult = 0x32,		// Results of encoding session begin:  i0 = Local ID, b = Success bool, e = reason, u1 = Server Handle
	
	cVCC_EncodeFrame		= 0x37,			// Receive Frame of Video to encode:  u0 = Server Handle, i1 = Frame Number  [ Video Frame Data ]
	cVCC_EncodeFrameResult  = 0x38,			// Encode Frame Result                u0 = Server Handle, i1 = Frame Number, bi2 = Success bool

	cVCC_EndEncodingSession	= 0x3A,			// End an encoding sessions:		u0 = Server Handle
	cVCC_EndEncodingResult  = 0x3B,			// Result of encoding session end:	u0 = Server Handle, i1 = Success bool

	cVCC_ForceUint8			= UINT8_MAX
};


// enum eVidCacheError_t
// {
// 	cVCE_NoErr				= 0,			// success!
// 	
// 	cVCE_NoChacheSession,					// not in a current cache session
// 	cVCE_CacheSessionOpen,					// already opened a cache session
// 	cVCE_FileNotFound,						// the file doesn't exist
// 	cVCE_FileNotMovie,						// unable to open file as a movie
// 	
// 	cVCE_ForceUint8			= UINT8_MAX
// };



struct CVideoCacheSessionInfo
{
	public:
		int32				m_nFrameWidth;
		int32				m_nFrameHeight;
		eVideoFrameFormat_t	m_FrameFormat;
		
		CVideoCacheSessionInfo()
		{
			V_memset( this, 0, sizeof( *this ) );
		}		
		
		CVideoCacheSessionInfo( int width, int height, eVideoFrameFormat_t format ) :
			m_nFrameWidth( width ),
			m_nFrameHeight( height ),
			m_FrameFormat( format )
		{
		}
		
};



// -----------------------------------------------------------------------
// CVideoFrameInfo - Info about the uncompressed bitmaps that
//      are served up.   32 bpp RBGA is assumed
// -----------------------------------------------------------------------
struct CVideoFrameInfo
{
	public:
		int32		m_nBitMapWidth;
		int32		m_nBitmapHeight;
		int32		m_nBitmapImageBytes;
	
		eVideoFrameFormat_t	m_BitmapFormat;
		
		float		m_nImageTime;
		
		// constructors		
		CVideoFrameInfo() :
			m_nBitMapWidth( 0 ),
			m_nBitmapHeight( 0 ),
			m_nBitmapImageBytes( 0 ),
			m_BitmapFormat( cVFF_Undefined ),
			m_nImageTime( VIDEO_TIME_UNINITALIZED )
		{
		}
			
		CVideoFrameInfo( float timeVal, int width, int height ) :
			m_nBitMapWidth( width ),
			m_nBitmapHeight( height ),
			m_nBitmapImageBytes( width * height * 4 ),
			m_nImageTime( timeVal )
		{
		}
};





// -----------------------------------------------------------------------
// CVideoEncodeSessionParams - Info about the movie we would like to
//     create from still frames (and later, Audio data)
// -----------------------------------------------------------------------
struct CVideoEncodeSessionParams
{
	public:
		int32				m_VideoFPSRate;				// Number of frames per second the video will be recorded at
		int32				m_VideoDurationInFrames;	// Number of frames total in the movie
		int32				m_VideoFrameImageWidth;		// Horizontal resolution of the video source images
		int32				m_VideoFrameImageHeight;	// Vertical resolution of the video source images
		eVideoFrameFormat_t	m_VideoFrameFormat;			// Format of the frames to be received
		int32				m_VideoCodecToUse;			// Codec to use for encoding
		int32				m_VideoQuality;				// Quality of the video encoding
		int32				m_VideoFrameImageSize;		// Size of the images buffer for one frame
		int32				m_nCurrentFrameNo;			// current frame number

		// accessors
		inline eVideoFrameFormat_t  GetFormat()					{ return m_VideoFrameFormat; }
		inline void SetFormat( eVideoFrameFormat_t theFormat )	{ m_VideoFrameFormat = theFormat; }					
		inline eVideoCodec_t	GetCodec()							{ return (eVideoCodec_t) m_VideoCodecToUse; }
		inline void SetCodec( eVideoCodec_t theCodec )			{ m_VideoCodecToUse = (int32) theCodec; }
		inline eVideoQuality_t GetQuality()						{ return (eVideoQuality_t) m_VideoQuality; }
		inline void SetQualty( eVideoQuality_t theQuality )		{ m_VideoQuality = (int32) theQuality; }

		// Constructors
		CVideoEncodeSessionParams() 
		{
			V_memset( this, 0, sizeof( *this ) );
		}

		// copy operators	
		CVideoEncodeSessionParams& operator = ( CVideoEncodeSessionParams& rhs )
		{
			V_memcpy( this, &rhs, sizeof( *this ) );
			return *this; 
		}

};


// -----------------------------------------------------------------------
// CVidCmd - command packet structure for remote video cache
//   The parameters which are Dependant upon the actual command value
//   are mapped in a union for direct access by type
// -----------------------------------------------------------------------
struct CVidCmd
{
	public:
		uint8					m_Command;			// command converted from eVidCacheCommand_t
		uint8					m_Success;			// success bool
		uint8					m_LastError;		// info on last error (if success == false)
		uint8					m_SessionID;		// session ID
		
		union
		{
			char	m_data1[12];
			float	m_fData[3];
			int32	m_iData[3];
			uint32	m_uData[3];
		};
		
		
		// constructors - must include the command
		inline CVidCmd( eVidCacheCommand_t theCommand )
		{
			V_memset( this, 0, sizeof( *this ) );
			m_Command = theCommand;
		}	
		
		inline CVidCmd( eVidCacheCommand_t theCommand, RemoteVideoSessionId_t theSession )
		{
			V_memset( this, 0, sizeof( *this ) );
			m_Command = theCommand;
			m_SessionID = theSession;
		}	

		inline CVidCmd( eVidCacheCommand_t theCommand, bool success )
		{
			V_memset( this, 0, sizeof( *this ) );
			m_Command = theCommand;
			m_Success = success;
		}	
		
		inline CVidCmd( eVidCacheCommand_t theCommand, eVidCacheError_t theErrorCode )
		{
			V_memset( this, 0, sizeof( *this ) );
			m_Command = theCommand;
			m_LastError = theErrorCode;
		}	
		
		inline CVidCmd( eVidCacheCommand_t theCommand, bool success, eVidCacheError_t theErrorCode )
		{
			V_memset( this, 0, sizeof( *this ) );
			m_Command = theCommand;
			m_Success = success;
			m_LastError = theErrorCode;
		}	
		
		inline CVidCmd( eVidCacheCommand_t theCommand, RemoteVideoSessionId_t theSession, bool success )
		{
			V_memset( this, 0, sizeof( *this ) );
			m_Command = theCommand;
			m_Success = success;
			m_SessionID = theSession;
		}	
		
		inline CVidCmd( eVidCacheCommand_t theCommand, RemoteVideoSessionId_t theSession, bool success, eVidCacheError_t theErrorCode )
		{
			V_memset( this, 0, sizeof( *this ) );
			m_Command = theCommand;
			m_Success = success;
			m_LastError = theErrorCode;
			m_SessionID = theSession;
		}	
		
		// accessors		
		inline eVidCacheCommand_t		GetCommand()		{ return (eVidCacheCommand_t) m_Command; }
		
		inline bool						GetSuccess()		{ return ( m_Success != 0 ); }
		inline eVidCacheError_t			GetError()			{ return (eVidCacheError_t) m_LastError; }	
		inline RemoteVideoSessionId_t	GetSessionID()		{ return m_SessionID; }
		
		inline void SetSuccess( bool success )				{ m_Success = success; }
		inline void SetError( eVidCacheError_t theErr )		{ m_LastError = theErr; }
		inline void	SetSession( RemoteVideoSessionId_t id ) { m_SessionID = id; }
		
	private:
		inline CVidCmd()			{ V_memset( this, 0, sizeof( *this ) ); }		// disable
		

};

// -----------------------------------------------------------------------
// Quick Constructors for putting together specific commands
// -----------------------------------------------------------------------
struct CVidCacheCmdPing : public CVidCmd
{
	CVidCacheCmdPing( ) : CVidCmd( cVCC_Ping )  {}
};

struct CVidCacheCmdQuit : public CVidCmd
{
	CVidCacheCmdQuit( ) : CVidCmd( cVCC_Quit )  {}
};

struct CVidCacheCmdAck : public CVidCmd
{
	CVidCacheCmdAck( bool success = true ) : CVidCmd( cVCC_Ack, success ) {}
	CVidCacheCmdAck( eVidCacheError_t err ) : CVidCmd( cVCC_Ack, ( err == cVCE_NoErr ), err ) {}
};

// Open / Close a video caching session
struct CVidCacheCmdBeginCacheSession : public CVidCmd
{
	CVidCacheCmdBeginCacheSession( int localId ) : CVidCmd( cVCC_BeginCacheSession )
	{
		m_iData[0] = localId;
	}
	inline int GetLocalId()		   { return m_iData[0]; }
};

struct CVidCacheCmdBeginCacheSessionResult : public CVidCmd
{
	CVidCacheCmdBeginCacheSessionResult( int localId, RemoteVideoSessionId_t id, bool success, eVidCacheError_t err ) : CVidCmd( cVCC_BeginCacheSessionResult, id, success, err )
	{
		m_iData[0] = localId;
	}
	inline int GetLocalId()		   { return m_iData[0]; }
};

struct CVidCacheCmdEndCacheSession : public CVidCmd
{
	CVidCacheCmdEndCacheSession( RemoteVideoSessionId_t id ) : CVidCmd( cVCC_EndCacheSession, id ) {}
};

// cache / uncache individual video file
struct CVidCacheCmdCacheFile : public CVidCmd
{
	CVidCacheCmdCacheFile( int localId, RemoteVideoSessionId_t id, const char* fileName ) : CVidCmd( cVCC_CacheFile, id ) 
	{
		m_iData[0] = localId;
		m_iData[1] = V_strlen( fileName ) + 1;		// Include trailing NULL
	}
	inline int GetLocalId()		   { return m_iData[0]; }
	inline int GetFileNameBufferLen() { return m_iData[1]; }
};

struct CVidCacheCmdCacheFileResult : public CVidCmd
{
	CVidCacheCmdCacheFileResult( int localId, bool success, eVidCacheError_t err, ServerVideoHandle_t serverHandle ) : CVidCmd( cVCC_CacheFileResult, success, err ) 
	{
		m_iData[0] = localId;
		m_uData[1] = serverHandle;
	}
	int GetLocalId()		   { return m_iData[0]; }
	ServerVideoHandle_t GetServerHandle()  { return (ServerVideoHandle_t) m_uData[1]; }
};

struct CVidCacheCmdUncacheFile : public CVidCmd
{
	CVidCacheCmdUncacheFile( ServerVideoHandle_t serverHandle, RemoteVideoSessionId_t id ) : CVidCmd( cVCC_UncacheFile, id ) 
	{
		m_uData[0] = serverHandle;
	}
	ServerVideoHandle_t GetServerHandle()  { return (ServerVideoHandle_t) m_uData[0]; }
};

struct CVidCacheCmdUncacheAllFiles : public CVidCmd
{
	CVidCacheCmdUncacheAllFiles( RemoteVideoSessionId_t id ) : CVidCmd( cVCC_UncacheAllFiles, id )  {}
};

// Requesting specific frames
struct CVidCacheCmdRequestFrame : public CVidCmd
{
	CVidCacheCmdRequestFrame( ServerVideoHandle_t serverHandle, float frameTime ) : CVidCmd( cVCC_RequestFrame ) 
	{
		m_uData[0] = serverHandle;
		m_fData[1] = frameTime;
	}
	ServerVideoHandle_t GetServerHandle()  { return (ServerVideoHandle_t) m_uData[0]; }
	float GetFrameTime() { return m_fData[1]; }
};

struct CVidCacheCmdRequestFrameResult : public CVidCmd
{
	CVidCacheCmdRequestFrameResult( ServerVideoHandle_t serverHandle, float frameTime, bool success) : CVidCmd( cVCC_RequestFrameResult, success ) 
	{
		m_uData[0] = serverHandle;
		m_fData[1] = frameTime;
	}
	ServerVideoHandle_t GetServerHandle()  { return (ServerVideoHandle_t) m_uData[0]; }
	float GetFrameTime() { return m_fData[1]; }
};

// begin / end video encoding
struct  CVidCacheCmdBeginEncodingSession : public CVidCmd
{
	CVidCacheCmdBeginEncodingSession( int localId, const char* fileName ) : CVidCmd( cVCC_BeginEncodingSession ) 
	{
		m_iData[0] = localId;
		m_iData[1] = V_strlen( fileName ) + 1;					// Include trailing NULL
	}
	int GetLocalId()		   { return m_iData[0]; }
	int GetFileNameBufferLen() { return m_iData[1]; }
};

struct CVidCacheCmdBeginEncodingResult : public CVidCmd
{
	CVidCacheCmdBeginEncodingResult( int localId, bool success, ServerVideoHandle_t serverHandle ) : CVidCmd( cVCC_BeginEncodingResult, success ) 
	{
		m_iData[0] = localId;
		m_uData[1] = serverHandle;
	}
	int GetLocalId()		   { return m_iData[0]; }
	ServerVideoHandle_t GetServerHandle()  { return (ServerVideoHandle_t) m_uData[1]; }
};

struct CVidCacheCmdEncodeFrame : public CVidCmd
{
	CVidCacheCmdEncodeFrame( ServerVideoHandle_t serverHandle, int frameNum ) : CVidCmd( cVCC_EncodeFrame ) 
	{
		m_uData[0] = serverHandle;
		m_iData[1] = frameNum;
	}
	ServerVideoHandle_t GetServerHandle()  { return (ServerVideoHandle_t) m_uData[0]; }
	int GetFrameNumber() { return m_iData[1]; }
};

struct CVidCacheCmdEncodeFrameResult : public CVidCmd
{
	CVidCacheCmdEncodeFrameResult( ServerVideoHandle_t serverHandle, int frameNum, bool success) : CVidCmd( cVCC_EncodeFrameResult, success ) 
	{
		m_uData[0] = serverHandle;
		m_iData[1] = frameNum;
	}
	ServerVideoHandle_t GetServerHandle()  { return (ServerVideoHandle_t) m_uData[0]; }
	int GetFrameNumber() { return m_iData[1]; }
};


struct  CVidCacheCmdEndEncodingSession : public CVidCmd
{
	CVidCacheCmdEndEncodingSession( ServerVideoHandle_t serverHandle ) : CVidCmd( cVCC_EndEncodingSession ) 
	{
		m_uData[0] = serverHandle;
	}
	ServerVideoHandle_t GetServerHandle()  { return (ServerVideoHandle_t) m_uData[0]; }
};

struct CVidCacheCmdEndEncodingResult : public CVidCmd
{
	CVidCacheCmdEndEncodingResult( ServerVideoHandle_t serverHandle, bool success ) : CVidCmd( cVCC_EndEncodingResult ) 
	{
		m_uData[0] = serverHandle;
		m_iData[1] = (int32) success;
	}
	bool GetSuccess()		   { return ( m_iData[1] != 0 ); }
	ServerVideoHandle_t GetServerHandle()  { return (ServerVideoHandle_t) m_uData[0]; }
};


// -----------------------------------------------------------------------
// misc definitions
// -----------------------------------------------------------------------
const uint16 cSFMVidCachePort = 26832;

#define VIDEO_CACHE_APP_MUTEX_NAME			TEXT( "sfm_vid_cache_app" )
#define VIDEO_CACHE_APP_SIGNAL_EVENT_NAME	TEXT( "sfm_vid_cache_event1" )



#endif			// VIDEO_CACHE_COMMANDS_H
