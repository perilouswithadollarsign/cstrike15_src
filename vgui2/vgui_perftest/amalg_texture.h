//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef AMALGAMATEDTEXTURE_H
#define AMALGAMATEDTEXTURE_H
#ifdef _WIN32
#pragma once
#endif

#include "amalg_texture_vars.h"
#include "bitmap/floatbitmap.h"
#include "tier1/utlmap.h"
#include "tier1/utlstringmap.h"
#include "tier2/tier2.h"


struct Sequence;
struct SequenceFrame
{
	SequenceFrame() : m_mapSequences( DefLessFunc( Sequence * ) ) {}

	FloatBitMap_t *m_pImage;
	int m_XCoord, m_YCoord;									// where it ended up packed
	CUtlMap< Sequence *, int > m_mapSequences;
};

struct SequenceEntry
{
	SequenceFrame *m_pSeqFrame[MAX_IMAGES_PER_FRAME];
	float m_fDisplayTime;
};

struct Sequence
{
	int m_nSequenceNumber;
	bool m_Clamp;											// as opposed to loop
	int m_eMode;
	CUtlVector<SequenceEntry> m_Frames;

	Sequence( void )
	{
		m_Clamp = true;
		m_eMode = SQM_RGBA;
	}

};


// The texture itself.
class CAmalgamatedTexture
{
public:
	CAmalgamatedTexture();
	void Init( const char *pShtFileName = NULL );
	void WriteFile();

	void DetermineBestPacking();
	bool PackImages( char const *pFilename, int nWidth );

	int GetWidth() { return m_nWidth; }

	void SetCurrentSequenceClamp( bool bState );
	void SetPackingMode( int mode );

	void CreateNewSequence( int sequenceNumber, int mode );
	void ValidateSequenceType( int eMode, char *word );
	void SetSequenceType( int eMode );
	bool CurrentSequenceExists();

	void CreateFrame( float ftime, CUtlVector<char *> &frameNames );	
	void LoadFrame( SequenceEntry &newSequenceEntry, char *fnamebuf, int frameNumber );

private:

	int GetPackingMode();	
	int GetSequenceType();
	
	void ValidateFramePacking( SequenceFrame *pBitmap, char *fileName );
	
	bool PackImages_Flat( char const *pFilename, int nWidth );
	bool PackImages_Rgb_A( char const *pFilename, int nWidth );


	float UCoord( int u )
	{
		float uc = u + 0.5;
		return uc / (float) m_nWidth;
	}
	float VCoord( int v )
	{
		float vc = v + 0.5;
		return vc / (float) m_nHeight;
	}

	const char *m_pShtFile;

	CUtlStringMap<SequenceFrame *> m_ImageList;
	int m_ePackingMode;

	CUtlVector<Sequence *> m_Sequences;
	Sequence *m_pCurSequence;

	int m_nWidth;
	int m_nHeight;

};


#endif // AMALGAMATEDTEXTURE_H