//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VOICE_H
#define VOICE_H
#pragma once


#include "ivoicetweak.h"


/*! @defgroup Voice Voice
Defines the engine's interface to the voice code.
@{
*/


#define VOICE_OUTPUT_SAMPLE_RATE			22050	// Sample rate that we feed to the mixer for celt codec.
#define VOICE_OUTPUT_SAMPLE_RATE_SPEEX		11025	// Sample rate that we feed to the mixer for speex codec.


//! Returned on error from certain voice functions.
#define VOICE_CHANNEL_ERROR			-1
#define VOICE_CHANNEL_IN_TWEAK_MODE	-2	// Returned by AssignChannel if currently in tweak mode (not an error).

#define VOICE_CURRENT_VERSION 3


//! Initialize the voice code.
bool Voice_Init(const char *pCodec, int iVersion );

//! Force Initialization with default codec.
void Voice_ForceInit();

//! Shutdown the voice code.
void Voice_Deinit();

//! Returns true if the client has voice enabled
bool Voice_Enabled( void );

//! Returns true if the client has voice enabled for casters
bool Voice_CasterEnabled( uint32 uCasterAccountID );

//! Set account ID of caster ( 0 = no caster )
void Voice_SetCaster( uint32 uCasterAccountID );

//! Returns true if the client has voice system enabled
bool Voice_SystemEnabled( void );


//! Returns true if the user can hear themself speak.
bool Voice_GetLoopback();


//! This is called periodically by the engine when the server acks the local player talking.
//! This tells the client DLL that the local player is talking and fades after about 200ms.
void Voice_LocalPlayerTalkingAck( int iSsSlot );


//! Call every frame to update the voice stuff.
bool Voice_Idle(float frametime);


//! Returns true if mic input is currently being recorded.
bool Voice_IsRecording();

//! Begin recording input from the mic.
bool Voice_RecordStart(
	//! Filename to store incoming mic data, or NULL if none.
	const char *pUncompressedFile,	
	
	//! Filename to store the output of compression and decompressiong with the codec, or NULL if none.
	const char *pDecompressedFile,	
	
	//! If this is non-null, the voice manager will use this file for input instead of the mic.
	const char *pMicInputFile		
	);

//! Stop recording from the mic.
bool Voice_RecordStop();

enum VoiceFormat_t
{
	VoiceFormat_Steam,
	VoiceFormat_Engine
};

//! Get the most recent N bytes of compressed data. If nCount is less than the number of
//! available bytes, it discards the first bytes and gives you the last ones.
//! Set bFinal to true on the last call to this (it will flush out any stored voice data).
//!
//! pnOutSectionNumber can return a 'section number", which is simply a number that will
//! increment every time there is a new contiguous block of non-silence.
//!
//! pnOutSequenceNumber is a compressed byte offset of the data, within the current section.
//! this is a TCP-style sequence number, so it actually refers to the byte offset PLUS
//! the length of the returned packet.
//!
//! pnOutUncompressedSampleOffset will return an "absolute" timestamp corresponding to the
//! start of the packet, with a precision of VOICE_OUTPUT_SAMPLE_RATE.
int Voice_GetCompressedData(char *pchData, int nCount, bool bFinal, VoiceFormat_t *pOutFormat = NULL, uint8 *pnOutSectionNumber = NULL, uint32 *pnOutSectionSequenceNumber = NULL, uint32 *pnOutUncompressedSampleOffset = NULL );


//! Pass incoming data from the server into here.
//! The data should have been compressed and gotten through a Voice_GetCompressedData call.
void Voice_AddIncomingData(
	//! Channel index.
	int nChannel, 
	//! Compressed data to add to the channel.
	const char *pchData, 
	//! Number of bytes in pchData.
	int nCount,
	//! A number that changes whenever we get a new non-silence section of audio.  Used to handle silence and deal with resetting the codec internal state
	uint8 nSectionNumber,
	//! Byte offset of compressed data, within the current sequence.  Used to handle missing packets.  For historical reasons, this should include the current packet compressed size.  (It is actually the byte offset of the packet to follow.)
	uint32 nSectionSequenceNumber,
	//! Uncompressed timestamp.  Used to handle silence and dropped packets properly
	uint32 nUncompressedSampleOffset,
	//! Use Steam or engine voice data format
	VoiceFormat_t format
	);

#define VOICE_TIME_PADDING	0.2f	// Time between receiving the first voice packet and actually starting
									// to play the sound. This accounts for frametime differences on the clients
									// and the server.


//! Call this to reserve a voice channel for the specified entity to talk into.
//! \return A channel index for use with Voice_AddIncomingData or VOICE_CHANNEL_ERROR on error.
int Voice_AssignChannel(int nEntity, bool bProximity, bool bCaster = false, float timePadding = VOICE_TIME_PADDING );

//! Call this to get the channel index that the specified entity is talking into.
//! \return A channel index for use with Voice_AddIncomingData or VOICE_CHANNEL_ERROR if the entity isn't talking.
int Voice_GetChannel(int nEntity);

#if !defined( NO_VOICE )
extern IVoiceTweak g_VoiceTweakAPI;
#endif

/*! @} */


#endif // VOICE_H
