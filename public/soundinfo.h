//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SOUNDINFO_H
#define SOUNDINFO_H

#ifdef _WIN32
#pragma once
#endif

#include "bitbuf.h"
#include "const.h"
#include "soundflags.h"
#include "coordsize.h"
#include "mathlib/vector.h"
#include "netmessages.h"


#define WRITE_DELTA_UINT( name, length )	\
	if ( name == delta->name )		\
		buffer.WriteOneBit(0);		\
	else					\
	{					\
		buffer.WriteOneBit(1);		\
		buffer.WriteUBitLong( name, length ); \
	}

#define READ_DELTA_UINT( name, length )			\
	if ( buffer.ReadOneBit() != 0 )			\
	{ name = buffer.ReadUBitLong( length ); }\
	else { name = delta->name; }

#define WRITE_DELTA_SINT( name, length )	\
	if ( name == delta->name )		\
		buffer.WriteOneBit(0);		\
	else					\
	{					\
		buffer.WriteOneBit(1);		\
		buffer.WriteSBitLong( name, length ); \
	}

#define WRITE_DELTA_SINT_SCALE( name, scale, length )	\
	if ( name == delta->name )		\
	buffer.WriteOneBit(0);		\
	else					\
	{					\
	buffer.WriteOneBit(1);		\
	buffer.WriteSBitLong( name / scale, length ); \
	}

#define READ_DELTA_SINT( name, length )			\
	if ( buffer.ReadOneBit() != 0 )			\
	{ name = buffer.ReadSBitLong( length ); }	\
	else { name = delta->name; }

#define READ_DELTA_SINT_SCALE( name, scale, length )			\
	if ( buffer.ReadOneBit() != 0 )			\
	{ name = scale * buffer.ReadSBitLong( length ); }	\
	else { name = delta->name; }

#define SOUND_SEQNUMBER_BITS	10
#define SOUND_SEQNUMBER_MASK	( (1<<SOUND_SEQNUMBER_BITS) - 1 )

// offset sound delay encoding by 60ms since we encode negative sound delays with less precision
// This means any negative sound delay greater than -100ms will get encoded at full precision
#define SOUND_DELAY_OFFSET					(0.100f)

#pragma pack(4)
// the full float time for now.
#define SEND_SOUND_TIME 1

//-----------------------------------------------------------------------------
struct SoundInfo_t
{
	Vector			vOrigin;
	Vector			vDirection;
	Vector			vListenerOrigin;
	const char		*pszName;		// UNDONE: Make this a FilenameHandle_t to avoid bugs with arrays of these
	float			fVolume;
	float			fDelay;
	float			fTickTime;			// delay is encoded relative to this tick, fix up if packet is delayed
	int				nSequenceNumber;
	int				nEntityIndex;
	int				nChannel;
	int				nPitch;
	int				nFlags;
	unsigned int 	nSoundNum;
	int				nSpeakerEntity;
	int				nRandomSeed;
	soundlevel_t	Soundlevel;
	bool			bIsSentence;
	bool			bIsAmbient;
	bool			bLooping;

	
	//---------------------------------
	
	enum SoundInfoInit_t
	{
		SOUNDINFO_SETDEFAULT,
		SOUNDINFO_NO_SETDEFAULT,
	};
	SoundInfo_t( SoundInfoInit_t Init = SOUNDINFO_SETDEFAULT )
	{
		if( Init == SOUNDINFO_SETDEFAULT )
		{
			SetDefault();
		}
	}

	void Set(int newEntity, int newChannel, const char *pszNewName, const Vector &newOrigin, const Vector& newDirection, 
			float newVolume, soundlevel_t newSoundLevel, bool newLooping, int newPitch, const Vector &vecListenerOrigin, int speakerentity, int nSeed )
	{
		nEntityIndex = newEntity;
		nChannel = newChannel;
		pszName = pszNewName;
		vOrigin = newOrigin;
		vDirection = newDirection;
		fVolume = newVolume;
		Soundlevel = newSoundLevel;
		bLooping = newLooping;
		nPitch = newPitch;
		vListenerOrigin = vecListenerOrigin;
		nSpeakerEntity = speakerentity;
		nRandomSeed = nSeed;
	}

	void SetDefault()
	{
		fDelay = DEFAULT_SOUND_PACKET_DELAY;
		fTickTime = 0;
		fVolume = DEFAULT_SOUND_PACKET_VOLUME;
		Soundlevel = SNDLVL_NORM;
		nPitch = DEFAULT_SOUND_PACKET_PITCH;

		nEntityIndex = 0;
		nSpeakerEntity = -1;
		nChannel = CHAN_STATIC;
		nSoundNum = 0;
		nFlags = 0;
		nSequenceNumber = 0;
		nRandomSeed = 0;

		pszName = NULL;
	
		bLooping = false;
		bIsSentence = false;
		bIsAmbient = false;
		
		vOrigin.Init();
		vDirection.Init();
		vListenerOrigin.Init();
	}

	void ClearStopFields()
	{
		fVolume = 0;
		Soundlevel = SNDLVL_NONE;
		nPitch = PITCH_NORM;
		pszName = NULL;
		fDelay = 0.0f;
		nSequenceNumber = 0;

		vOrigin.Init();
		nSpeakerEntity = -1;
	}

	// this cries for Send/RecvTables:
	void WriteDelta( const SoundInfo_t *delta, CSVCMsg_Sounds& Msg, float finalTickTime )
	{
#define WRITE_DELTA_FIELD( _name, _protobufname)	\
	if( delta->_name != _name ) pSoundData->set_ ## _protobufname( _name );
#define WRITE_DELTA_FIELD_SCALED( _name, _protobufname, _scaled_val )	\
	if( delta->_name != _name ) pSoundData->set_ ## _protobufname( _scaled_val );

		SoundInfo_t	defaultSound( SOUNDINFO_NO_SETDEFAULT );
		CSVCMsg_Sounds::sounddata_t *pSoundData = Msg.add_sounds();

		if( !delta )
		{
			defaultSound.SetDefault();
			delta = &defaultSound;
		}

		WRITE_DELTA_FIELD( nEntityIndex, entity_index );

		WRITE_DELTA_FIELD( nFlags, flags );

		// Scriptable sounds are written as a hash, uses full 32 bits
		if( nFlags & SND_IS_SCRIPTHANDLE )
		{
			WRITE_DELTA_FIELD( nSoundNum, sound_num_handle );
		}
		else
		{
			WRITE_DELTA_FIELD( nSoundNum, sound_num );
		}

		WRITE_DELTA_FIELD( nChannel, channel );

		WRITE_DELTA_FIELD( bIsAmbient, is_ambient );
		WRITE_DELTA_FIELD( bIsSentence, is_sentence ); // NOTE: SND_STOP behavior is different depending on this flag

		if ( nFlags != SND_STOP )
		{
			WRITE_DELTA_FIELD_SCALED( nSequenceNumber, sequence_number, ( nSequenceNumber & SOUND_SEQNUMBER_MASK ) )

				WRITE_DELTA_FIELD_SCALED( fVolume, volume, ( int )( fVolume * 127.0f ) );

			WRITE_DELTA_FIELD( Soundlevel, sound_level );

			WRITE_DELTA_FIELD( nPitch, pitch );

			WRITE_DELTA_FIELD( nRandomSeed, random_seed );

			float delayValue = fDelay;
			if ( ( nFlags & SND_DELAY ) && ( fTickTime != finalTickTime ) )
			{
				delayValue += fTickTime - finalTickTime;
			}
			if ( delayValue != delta->fDelay )
			{
				pSoundData->set_delay_value( delayValue );
			}

			// don't transmit sounds with high precision
			WRITE_DELTA_FIELD_SCALED( vOrigin.x, origin_x, ( int )( vOrigin.x * ( 1.0f / 8.0f ) ) );
			WRITE_DELTA_FIELD_SCALED( vOrigin.y, origin_y, ( int )( vOrigin.y * ( 1.0f / 8.0f ) ) );
			WRITE_DELTA_FIELD_SCALED( vOrigin.z, origin_z, ( int )( vOrigin.z * ( 1.0f / 8.0f ) ) );

			WRITE_DELTA_FIELD( nSpeakerEntity, speaker_entity );
		}
		else
		{
			ClearStopFields();
		}

#undef WRITE_DELTA_FIELD
#undef WRITE_DELTA_FIELD_SCALED
	};

	void ReadDelta( const SoundInfo_t *delta, const CSVCMsg_Sounds::sounddata_t& SoundData)
	{
#define READ_DELTA_FIELD( _name, _protobufname ) \
	_name = ( SoundData.has_ ## _protobufname() ) ? SoundData._protobufname() : delta->_name;
#define READ_DELTA_FIELD_SCALED( _name, _protobufname, _scale ) \
	_name = ( SoundData.has_ ## _protobufname() ) ? ( SoundData._protobufname() * ( _scale ) ) : delta->_name;

		READ_DELTA_FIELD( nEntityIndex, entity_index );

		READ_DELTA_FIELD( nFlags, flags );

		// Scriptable sounds are written as a hash, uses full 32 bits
		if( nFlags & SND_IS_SCRIPTHANDLE )
		{
			READ_DELTA_FIELD( nSoundNum, sound_num_handle );
		}
		else
		{
			READ_DELTA_FIELD( nSoundNum, sound_num );
		}

		READ_DELTA_FIELD( nChannel, channel );

		READ_DELTA_FIELD( bIsAmbient, is_ambient );
		READ_DELTA_FIELD( bIsSentence, is_sentence ); // NOTE: SND_STOP behavior is different depending on this flag

		if ( nFlags != SND_STOP )
		{
			READ_DELTA_FIELD( nSequenceNumber, sequence_number );

			READ_DELTA_FIELD_SCALED( fVolume, volume, ( 1.0f / 127.0f ) );

			Soundlevel = ( soundlevel_t )( SoundData.has_sound_level() ? SoundData.sound_level() : delta->Soundlevel );

			READ_DELTA_FIELD( nPitch, pitch );

			READ_DELTA_FIELD( nRandomSeed, random_seed );

			READ_DELTA_FIELD( fDelay, delay_value );

			READ_DELTA_FIELD_SCALED( vOrigin.x, origin_x, 8.0f );
			READ_DELTA_FIELD_SCALED( vOrigin.y, origin_y, 8.0f );
			READ_DELTA_FIELD_SCALED( vOrigin.z, origin_z, 8.0f );

			READ_DELTA_FIELD( nSpeakerEntity, speaker_entity );
		}
		else
		{
			ClearStopFields();
		}

#undef READ_DELTA_FIELD
#undef READ_DELTA_FIELD_SCALED
	}
};

struct SpatializationInfo_t
{
	typedef enum
	{
		SI_INCREATION = 0,
		SI_INSPATIALIZATION
	} SPATIALIZATIONTYPE;

	// Inputs
	SPATIALIZATIONTYPE	type;
	// Info about the sound, channel, origin, direction, etc.
	SoundInfo_t			info;

	// Requested Outputs ( NULL == not requested )
	Vector				*pOrigin;
	QAngle				*pAngles;
	float				*pflRadius;

	CUtlVector< Vector > *m_pUtlVecMultiOrigins;
	CUtlVector< QAngle > *m_pUtlVecMultiAngles;

};
#pragma pack()

#endif // SOUNDINFO_H
