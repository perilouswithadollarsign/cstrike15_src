//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SOUNDCHARS_H
#define SOUNDCHARS_H
#ifdef _WIN32
#pragma once
#endif

#define CHAR_STREAM			'*'		// as one of 1st 2 chars in name, indicates streaming wav data
#define CHAR_USERVOX		'?'		// as one of 1st 2 chars in name, indicates user realtime voice data
#define CHAR_SENTENCE		'!'		// as one of 1st 2 chars in name, indicates sentence wav
#define CHAR_DRYMIX			'#'		// as one of 1st 2 chars in name, indicates wav bypasses dsp fx
#define CHAR_DOPPLER		'>'		// as one of 1st 2 chars in name, indicates doppler encoded stereo wav: left wav (incomming) and right wav (outgoing).
#define CHAR_DIRECTIONAL	'<'		// as one of 1st 2 chars in name, indicates stereo wav has direction cone: mix left wav (front facing) with right wav (rear facing) based on soundfacing direction
#define CHAR_DISTVARIANT	'^'		// as one of 1st 2 chars in name, indicates distance variant encoded stereo wav (left is close, right is far)
#define CHAR_OMNI			'@'		// as one of 1st 2 chars in name, indicates non-directional wav (default mono or stereo)
#define CHAR_HRTF			'~'		// as one of 1st 2 chars in name, indicates wav should use HRTF spatialization for non-owners.
#define CHAR_RADIO			'+'		// as one of 1st 2 chars in name, indicates a 'radio' sound -- should be played without spatialization
#define CHAR_SPATIALSTEREO	')'		// as one of 1st 2 chars in name, indicates spatialized stereo wav
#define CHAR_DIRSTEREO		'('		// as one of 1st 2 chars in name, indicates directional stereo wav (like doppler)
#define CHAR_FAST_PITCH		'}'		// as one of 1st 2 chars in name, forces low quality, non-interpolated pitch shift
#define CHAR_SUBTITLED		'$'		// as one of 1st 2 chars in name, indicates the subtitles are forced

inline bool IsSoundChar(char c)
{
	bool b;

	b = ( c == CHAR_STREAM || c == CHAR_USERVOX || c == CHAR_SENTENCE || c == CHAR_DRYMIX || c == CHAR_OMNI || c == CHAR_RADIO || c == CHAR_DIRSTEREO );
	b = b || ( c == CHAR_DOPPLER || c == CHAR_DIRECTIONAL || c == CHAR_DISTVARIANT || c == CHAR_HRTF || c == CHAR_SPATIALSTEREO || c == CHAR_FAST_PITCH );
	b = b || ( c == CHAR_SUBTITLED );

	return b;
}

// return pointer to first valid character in file name
// by skipping over CHAR_STREAM...CHAR_DRYMIX

inline char *PSkipSoundChars(const char *pch)
{
	char *pcht = (char *)pch;

	while ( 1 )
	{
		if (!IsSoundChar(*pcht))
			break;
		pcht++;
	}

	return pcht;
}


inline bool TestSoundChar(const char *pch, char c)
{
	char *pcht = (char *)pch;

	while ( 1 )
	{
		if (!IsSoundChar(*pcht))
			break;
		if (*pcht == c)
			return true;
		pcht++;
	}

	return false;
}

#endif // SOUNDCHARS_H
