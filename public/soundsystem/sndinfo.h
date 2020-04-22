//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef SNDINFO_H
#define SNDINFO_H
#ifdef _WIN32
#pragma once
#endif

class Vector;
#include "utlsymbol.h"

// Handy defines for EmitSound
#define SOUND_FROM_LOCAL_PLAYER		-1
#define SOUND_FROM_WORLD			0

// These are used to feed a soundlevel to the sound system and have it use
// goldsrc-type attenuation. We should use this as little as possible and 
// phase it out as soon as possible.

// Take a regular sndlevel and convert it to compatibility mode.
#define SNDLEVEL_TO_COMPATIBILITY_MODE( x )		((soundlevel_t)(int)( (x) + 256 ))

// Take a compatibility-mode sndlevel and get the REAL sndlevel out of it.
#define SNDLEVEL_FROM_COMPATIBILITY_MODE( x )	((soundlevel_t)(int)( (x) - 256 ))

// Tells if the given sndlevel is marked as compatibility mode.
#define SNDLEVEL_IS_COMPATIBILITY_MODE( x )		( (x) >= 256 )

// Sound guids are assigned on the server starting at 1
// On the client, they are assigned by the sound system starting at 0x80000001
typedef uint32 SoundGuid_t;
#define INVALID_SOUND_GUID (SoundGuid_t)0


//-----------------------------------------------------------------------------
// Purpose:  Client side only 
//-----------------------------------------------------------------------------
struct SndInfo_t
{
	// Sound Guid
	SoundGuid_t		m_nGuid;
	FileNameHandle_t m_filenameHandle;		// filesystem filename handle - call IFilesystem to conver this to a string
	CEntityIndex m_nSoundSource;
	int			m_nChannel;
	// If a sound is being played through a speaker entity (e.g., on a monitor,), this is the
	//  entity upon which to show the lips moving, if the sound has sentence data
	CEntityIndex m_nSpeakerEntity;
	float		m_flVolume;
	float		m_flLastSpatializedVolume;
	// Radius of this sound effect (spatialization is different within the radius)
	float		m_flRadius;
	int			m_nPitch;
	Vector		*m_pOrigin;
	Vector		*m_pDirection;

	// if true, assume sound source can move and update according to entity
	bool		m_bUpdatePositions;
	// true if playing linked sentence
	bool		m_bIsSentence;
	// if true, bypass all dsp processing for this sound (ie: music)	
	bool		m_bDryMix;
	// true if sound is playing through in-game speaker entity.
	bool		m_bSpeaker;
	// for snd_show, networked sounds get colored differently than local sounds
	bool		m_bFromServer;
};

//-----------------------------------------------------------------------------
// Hearing info
//-----------------------------------------------------------------------------
struct AudioState_t
{
	AudioState_t()
	{
		Clear();
	}

	void Clear()
	{
		m_Origin.Init();
		m_Angles.Init();
		m_nViewEntity.SetRaw( -1 );
		m_bValid = false;
		m_bIsUnderwater = false;
	}

	Vector	m_Origin;
	QAngle	m_Angles;
	CEntityIndex m_nViewEntity;
	bool	m_bIsUnderwater : 1;
	bool	m_bValid : 1;
};

#endif // SNDINFO_H
