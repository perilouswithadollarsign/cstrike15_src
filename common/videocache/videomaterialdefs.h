//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
//  videomaterialdefs.h
// 
//  Purpose: provides an abstract interface to extract singleframes from
//           a video file
//
//=============================================================================

#pragma once

#ifndef VIDEOMATERIALDEFS_H
#define VIDEOMATERIALDEFS_H

// -----------------------------------------------------------------------
// ServerVideoHandle_t - Handle to a cached video asset
// -----------------------------------------------------------------------
typedef uint32  ServerVideoHandle_t;
typedef uint8	RemoteVideoSessionId_t;

// -----------------------------------------------------------------------
// eVideoFrameFormat_t - bitformat for transfered video frames
// -----------------------------------------------------------------------
enum eVideoFrameFormat_t
{
	cVFF_Undefined = 0,
	cVFF_R8G8B8A8_32Bit,
	cVFF_R8G8B8_24Bit,
	
	cVFF_Count,						// Auto list counter
	cVFF_ForceInt32	= INT32_MAX		// Make sure eNum is (at least) an int32
};


// -----------------------------------------------------------------------
// eVideoCodec_t available video codecs 
// -----------------------------------------------------------------------
enum eVideoCodec_t
{
	cVC_H264VideoCodec	= 0,
};


// -----------------------------------------------------------------------
// eVideoQuality_t - encoding quality options
// -----------------------------------------------------------------------
enum eVideoQuality_t
{
	cVQ_MinQuality = 0,
	cVQ_LowQuality,
	cVQ_MediumQuality,
	cVQ_HighQuality,
	cVQ_MaxQuality,
	cVQ_LoselessQuality,
};


// -----------------------------------------------------------------------
// eVidCacheError_t - error codes from the remote video cache app
// -----------------------------------------------------------------------
enum eVidCacheError_t
{
	cVCE_NoErr				= 0,			// success!
	
	cVCE_NoChacheSession,					// not in a current cache session
	cVCE_CacheSessionOpen,					// already opened a cache session
	cVCE_InvalidSessionID,					// don't know what you are talking about...
	cVCE_InvalidFileName,					// problem with the filename
	cVCE_FileNotFound,						// the file doesn't exist
	cVCE_FileNotMovie,						// unable to open file as a movie
	cVCE_BadFormatData,						// problem with the video frame size or buffer format
	
	cVCE_ForceUint8			= UINT8_MAX
};


// Misc constants

static const ServerVideoHandle_t INVALID_VIDEO_HANDLE = 0;

static const RemoteVideoSessionId_t REMOTE_SESSION_ID_NONE = 0;

static const float VIDEO_TIME_UNINITALIZED = -1.0f;

static const int	cMinVideoFrameWidth = 16;				// Minimum video frame width supported
static const int	cMinVideoFrameHeight = 16;				// Minimum video frame height supported
static const int	cMaxVideoFrameWidth = 2048;				// Maximum video frame width supported
static const int	cMaxVideoFrameHeight = 2048;			// Maximum video frame height supported


#endif
