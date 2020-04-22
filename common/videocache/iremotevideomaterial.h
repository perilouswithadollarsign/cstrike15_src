//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
//  iremotevideomaterial.h
// 
//  Purpose: provides an abstract interface to extract singleframes from
//           a video file
//
//=============================================================================

#pragma once

#ifndef IREMOTEVIDEOMATERIAL_H
#define IREMOTEVIDEOMATERIAL_H

#include "videocache/videomaterialdefs.h"

// needed for definitions of ITextureRegenerator & IMaterial
class IMaterial;


//-----------------------------------------------------------------------------
// Interface class for connecting to a remote video and retrieving specific
//   frames from the video.
// 
//
//
//-----------------------------------------------------------------------------

class IRemoteVideoMaterial
{
	public:
 		virtual void				Release() = 0;
 
 		virtual bool				IsInitialized() = 0;
		virtual	bool				IsRemoteVideoAvailable() = 0;
		virtual bool				IsConnectedToRemoteVideo() = 0;

		// commands that involve communication with the remote video server
		virtual bool				ConnectToRemoteVideo( const char* videoFileName ) = 0;
		virtual bool				GetRemoteVideoFrame( float theTime ) = 0;
		virtual bool				DisconnectFromRemoteVideo() = 0;

		// Get information about the remote video
		virtual const char		   *GetRemoteVideoFileName() = 0;
		virtual float				GetRemoteVideoDuration() = 0;
		virtual float				GetRemoteVideoFPS() = 0;
		virtual float				GetLastFrameTime() = 0;
		
		// Get a Texture/Material with the video frame in it
		virtual IMaterial		   *GetRemoteVideoFrameMaterial() = 0;
		virtual bool				GetRemoteVideoFrameSize( int &width, int &height ) = 0;
		virtual bool				GetRemoteVideoFrameTextureCoordRange( float &maxU, float &maxV ) = 0;
		
		// Get the raw video frame buffer
		virtual void			   *GetRemoteVideoFrameBuffer() = 0;
		virtual int					GetRemoteVideoFrameBufferSize() = 0;
		virtual eVideoFrameFormat_t	GetRemoteVideoFrameBufferFormat() = 0;
};



#endif	// IREMOTEVIDEOMATERIAL_H