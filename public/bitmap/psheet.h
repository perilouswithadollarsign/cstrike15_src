//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: sheet definitions for particles and other sprite functions
//
//===========================================================================//

#ifndef PSHEET_H
#define PSHEET_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlobjectreference.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier1/utlvector.h"

class CUtlBuffer;

// classes for keeping a dictionary of sheet files in memory.  A sheet is a bunch of frames packewd
// within one texture. Each sheet has 1 or more frame sequences stored for it.


// for fast lookups to retrieve sequence data, we store the sequence information discretized into
// a fixed # of frames. If this discretenesss is a visual problem, you can lerp the blend values to get it
// perfect.
#define SEQUENCE_SAMPLE_COUNT 512

#define MAX_IMAGES_PER_FRAME_ON_DISK 4
#define MAX_IMAGES_PER_FRAME_IN_MEMORY 2

struct SequenceSampleTextureCoords_t
{
	float m_fLeft_U0;
	float m_fTop_V0;
	float m_fRight_U0;
	float m_fBottom_V0;

	float m_fLeft_U1;
	float m_fTop_V1;
	float m_fRight_U1;
	float m_fBottom_V1;
};

struct SheetSequenceSample_t
{
	// coordinates of two rectangles (old and next frame coords)

	SequenceSampleTextureCoords_t m_TextureCoordData[MAX_IMAGES_PER_FRAME_IN_MEMORY];

	float m_fBlendFactor;

	void CopyFirstFrameToOthers(void)
	{
		// for old format files only supporting one image per frame
		for(int i=1; i < MAX_IMAGES_PER_FRAME_IN_MEMORY; i++)
		{
			m_TextureCoordData[i] = m_TextureCoordData[0];
		}
	}

};

enum SheetSequenceFlags_t
{
	SEQ_FLAG_CLAMP = 0x1, // as opposed to loop
	SEQ_FLAG_NO_ALPHA = 0x2, // packed as sequence-rgb (alpha channel should be ignored)
	SEQ_FLAG_NO_COLOR = 0x4, // packed as sequence-a (color channels should be ignored)
};

class CSheet
{
public:
	// read form a .sht file. This is the usual thing to do
	CSheet( CUtlBuffer &buf );
	CSheet( void );
	~CSheet( void );

	const SheetSequenceSample_t *GetSampleForSequence( float flAge, float flAgeScale, int nSequence, bool bForceLoop );

	// references for smart ptrs
	CUtlReferenceList<CSheet> m_References;

	struct SheetInfo_t
	{
		SheetSequenceSample_t *m_pSamples;
		uint8 m_SeqFlags;
		bool m_bSequenceIsCopyOfAnotherSequence;
		int16 m_nNumFrames;
		float m_flFrameSpan;
	};

	CUtlVector< SheetInfo_t > m_SheetInfo;
};

//////////////////////////////////////////////////////////////////////////

class IMesh;
class IMaterial;

// A heavier-weight version of CSheet with more bells and whistles

class CSheetExtended
{
public:
	explicit CSheetExtended( IMaterial* pMaterial );
	~CSheetExtended();
	
	int GetSheetSequenceCount();
	int GetNthSequenceIndex( int nSequenceNumber );
	const SheetSequenceSample_t *GetSampleForSequence( float flAge, float flAgeScale, int nSequence, bool bForceLoop );
	float GetSequenceTimeSpan( int nSequenceIndex );

	void DrawSheet( IMesh *pMesh, const Vector &vCenter, float flRadius, int nSheetSequence, float flAge, float flSheetPreviewSpeed, bool bLoopSheetPreview, int nSecondarySequence=-1, bool bOverrideSpriteCard=false );

	bool ValidSheetData();

	bool SequenceHasAlphaData( int nSequenceIndex );
	bool SequenceHasColorData( int nSequenceIndex );

	// Helper
	static bool IsMaterialSeparateAlphaColorMaterial( IMaterial* pMat );
	static bool IsMaterialDualSequence( IMaterial* pMat );

private:
	void LoadFromBuffer( CUtlBuffer& buf );
	void LoadFromMaterial( IMaterial* pMaterial );

	// TEMP: Store in a CSheet for now - eventually we'll want more data
	CSheet* m_pSheetData;
	CMaterialReference m_Material;
};


#endif   // PSHEET_H

