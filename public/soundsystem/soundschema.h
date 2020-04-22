//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//
//
//==================================================================================================

#ifndef SOUNDSCHEMA_H
#define SOUNDSCHEMA_H
#ifdef _WIN32
#pragma once
#endif

#ifndef SOURCE1
#include "resourcefile/resourcefile.h"
#include "resourcefile/resourcetype.h"
#endif

FORWARD_DECLARE_HANDLE( memhandle_t );

schema struct CEmphasisSample_t
{
public:
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS(CEmphasisSample_t)

	float32		time;
	float32		value;
};

schema struct CBasePhonemeTag_t
{
public:
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS(CBasePhonemeTag_t)

	float GetStartTime() const				{ return m_flStartTime; }
	float GetEndTime() const				{ return m_flEndTime; }
	int GetPhonemeCode() const				{ return m_nPhonemeCode; }

public:
	float32			m_flStartTime;
	float32			m_flEndTime;
	uint16			m_nPhonemeCode;
};

// A sentence can be closed captioned
// The default case is the entire sentence shown at start time
// 
// "<persist:2.0><clr:255,0,0,0>The <I>default<I> case"
// "<sameline>is the <U>entire<U> sentence shown at <B>start time<B>"

// Commands that aren't closed at end of phrase are automatically terminated
//
// Commands
// <linger:2.0>	The line should persist for 2.0 seconds beyond m_flEndTime
// <sameline>		Don't go to new line for next phrase on stack
// <clr:r,g,b,a>	Push current color onto stack and start drawing with new
//  color until we reach the next <clr> marker or a <clr> with no commands which
//  means restore the previous color
// <U>				Underline text (start/end)
// <I>				Italics text (start/end)
// <B>				Bold text (start/end)
// <position:where>	Draw caption at special location ??? needed
// <cr>				Go to new line

// Close Captioning Support
// The phonemes drive the mouth in english, but the CC text can
//  be one of several languages

//-----------------------------------------------------------------------------
// Purpose: A sentence is a box of words, and words contain phonemes
//-----------------------------------------------------------------------------
schema class CSentence_t
{
public:
	TYPEMETA( MNoScatter );
	DECLARE_SCHEMA_DATA_CLASS(CSentence_t)

	inline float			GetIntensity( float time, float endtime );
	inline CEmphasisSample_t *GetBoundedSample( int number, float endtime );
	int				GetNumSamples( void ) { return m_EmphasisSamples.Count(); }
	CEmphasisSample_t	*GetSample( int index ) { return &m_EmphasisSamples[ index ]; }

	bool			GetVoiceDuck() const { return m_bShouldVoiceDuck; }

	int				GetRuntimePhonemeCount() const { return m_RunTimePhonemes.Count(); }
	const CBasePhonemeTag_t *GetRuntimePhoneme( int i ) const { return &m_RunTimePhonemes[ i ]; }

public:
	bool				m_bShouldVoiceDuck;

	CResourceArray< CBasePhonemeTag_t > m_RunTimePhonemes;

	// Phoneme emphasis data
	CResourceArray< CEmphasisSample_t > m_EmphasisSamples;
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : number - 
// Output : CEmphasisSample_t
//-----------------------------------------------------------------------------
inline CEmphasisSample_t *CSentence_t::GetBoundedSample( int number, float endtime )
{
	// Search for two samples which span time f
	static CEmphasisSample_t nullstart;
	nullstart.time = 0.0f;
	nullstart.value = 0.5f;
	static CEmphasisSample_t nullend;
	nullend.time = endtime;
	nullend.value = 0.5f;

	if ( number < 0 )
	{
		return &nullstart;
	}
	else if ( number >= GetNumSamples() )
	{
		return &nullend;
	}

	return GetSample( number );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : time - 
//			type - 
// Output : float
//-----------------------------------------------------------------------------
inline float CSentence_t::GetIntensity( float time, float endtime )
{
	float zeroValue = 0.5f;

	int c = GetNumSamples();

	if ( c <= 0 )
	{
		return zeroValue;
	}

	int i;
	for ( i = -1 ; i < c; i++ )
	{
		CEmphasisSample_t *s = GetBoundedSample( i, endtime );
		CEmphasisSample_t *n = GetBoundedSample( i + 1, endtime );
		if ( !s || !n )
			continue;

		if ( time >= s->time && time <= n->time )
		{
			break;
		}
	}

	int prev = i - 1;
	int start = i;
	int end = i + 1;
	int next = i + 2;

	prev = MAX( -1, prev );
	start = MAX( -1, start );
	end = MIN( end, GetNumSamples() );
	next = MIN( next, GetNumSamples() );

	CEmphasisSample_t *esPre = GetBoundedSample( prev, endtime );
	CEmphasisSample_t *esStart = GetBoundedSample( start, endtime );
	CEmphasisSample_t *esEnd = GetBoundedSample( end, endtime );
	CEmphasisSample_t *esNext = GetBoundedSample( next, endtime );

	float dt = esEnd->time - esStart->time;
	dt = clamp( dt, 0.01f, 1.0f );

	Vector vPre( esPre->time, esPre->value, 0 );
	Vector vStart( esStart->time, esStart->value, 0 );
	Vector vEnd( esEnd->time, esEnd->value, 0 );
	Vector vNext( esNext->time, esNext->value, 0 );

	float f2 = ( time - esStart->time ) / ( dt );
	f2 = clamp( f2, 0.0f, 1.0f );

	Vector vOut;
	Catmull_Rom_Spline( 
		vPre,
		vStart,
		vEnd,
		vNext,
		f2, 
		vOut );

	float retval = clamp( vOut.y, 0.0f, 1.0f );
	return retval;
}

// Used for bitpacking
struct soundinfoheader_t
{
	unsigned int				m_Type : 2;  // 0 1 2 or 3
	unsigned int				m_bits : 5;  // 0 to 31
	unsigned int				m_channels : 2; // 1 or 2
	unsigned int				m_sampleSize : 3; // 1 2 or 4
	unsigned int				m_format : 2; // 1 == PCM, 2 == ADPCM
	unsigned int				m_rate : 17; // 0 to 64 K
};

schema struct VSound_t
{
	DECLARE_SCHEMA_DATA_CLASS(VSound_t)

	uint32 m_bitpackedsoundinfo;

	const soundinfoheader_t &info() const { return *(soundinfoheader_t *)&m_bitpackedsoundinfo; };

	int m_Type() const { return info().m_Type; };
	int m_bits() const { return info().m_bits; };
	int m_channels() const { return info().m_channels; };
	int m_sampleSize() const { return info().m_sampleSize; };
	int m_format() const { return info().m_format; };
	int m_rate() const { return info().m_rate; };
	
	int32 m_loopStart; // -1 for no loop
	uint32 m_sampleCount;

	float32	m_flDuration; // Duration in seconds

	// Phoneme stream (optional)
	CResourcePointer< CSentence_t >		m_Sentence;
	// Raw wave header (reinterpreted based on m_Type())
	CResourceArray< uint8 >			m_pHeader;

	uint32	m_nStreamingDataSize;

	// Any data after header is the raw sample data (PCM, ADPCM, .mp3, whatever)
	uint32	m_nStreamingDataOffset;												META( MNoSchema );
	memhandle_t m_hStreamDataCacheHandle;										META( MNoSchema );
};

#ifndef SOURCE1
#define RESOURCE_TYPE_SOUND RESOURCE_TYPE('s','n','d', 0)
DEFINE_RESOURCE_TYPE( VSound_t, RESOURCE_TYPE_SOUND, HSound, HSoundStrong );
#define SOUND_HANDLE_INVALID ( (HSound)0 )
#endif

#endif // SOUNDSCHEMA_H
