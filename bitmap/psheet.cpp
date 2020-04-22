//===== Copyright � 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: sheet code for particles and other sprite functions
//
//===========================================================================//

#include "bitmap/psheet.h"
#include "tier1/UtlStringMap.h"
#include "tier1/utlbuffer.h"
#include "tier2/fileutils.h"

// MOC_TODO: These probably shouldn't be here - maybe we should put CSheetExtended somewhere else, like materialsystem?
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CSheet::CSheet( void )
{
}

CSheet::CSheet( CUtlBuffer &buf )
{
	// lets read a sheet
	buf.ActivateByteSwappingIfBigEndian();
	int nVersion = buf.GetInt();								// version#
	int nNumCoordsPerFrame = (nVersion)?MAX_IMAGES_PER_FRAME_ON_DISK:1;

	int nNumSequences = buf.GetInt();
	int nNumToAllocate = nNumSequences;
	// The old hardcoded arrays were 64.
	// We will allocate 64 for now to handle content issues with 
	// rendering system trying to get at empty sequences.
	if ( nNumToAllocate < 64 )
		 nNumToAllocate = 64;

	m_SheetInfo.EnsureCapacity( nNumSequences );
	for ( int i = 0; i < nNumToAllocate; ++i )
	{
		m_SheetInfo.AddToTail();
		m_SheetInfo[i].m_pSamples = NULL;
		m_SheetInfo[i].m_SeqFlags = 0;
		m_SheetInfo[i].m_bSequenceIsCopyOfAnotherSequence = 0;
		m_SheetInfo[i].m_nNumFrames = 0;
		m_SheetInfo[i].m_flFrameSpan = 0.0f;
	}

	while ( nNumSequences-- )
	{
		int nSequenceIndex = buf.GetInt();
		if ( nSequenceIndex < 0 )
		{
			Warning( "Invalid sequence number (%d)!!!\n", nSequenceIndex );
			return;
		}
		else if ( nSequenceIndex >= m_SheetInfo.Count() )
		{
			// This can happen if users delete intermediate sequences.
			// For example you can wind up with n sequences if you delete a sequence between 0 and n
			// In this case we will pad out the vector.
			int i = -1;
			while ( i < nSequenceIndex )
			{
				i = m_SheetInfo.AddToTail();
				m_SheetInfo[i].m_pSamples = NULL;
				m_SheetInfo[i].m_SeqFlags = 0;
				m_SheetInfo[i].m_bSequenceIsCopyOfAnotherSequence = 0;
				m_SheetInfo[i].m_nNumFrames = 0;
				m_SheetInfo[i].m_flFrameSpan = 0.0f;
			}
		}

		m_SheetInfo[nSequenceIndex].m_SeqFlags = (uint8)(0xFF & buf.GetInt()); // reading an int, but not worth storing it all
		int nFrameCount = buf.GetInt();
		// Save off how many frames we have for this sequence
		m_SheetInfo[nSequenceIndex].m_nNumFrames = nFrameCount;
		Assert( nFrameCount >= 0 );
		bool bSingleFrameSequence = ( nFrameCount == 1 );

		int nTimeSamples = bSingleFrameSequence ? 1 : SEQUENCE_SAMPLE_COUNT;

		if ( m_SheetInfo[nSequenceIndex].m_pSamples )
		{
			Warning( "Invalid particle sheet sequence index.  There are more than one items with a sequence index of %d. We are only using the last one we found..\n", nSequenceIndex );
			delete[] m_SheetInfo[nSequenceIndex].m_pSamples;
		}
		m_SheetInfo[nSequenceIndex].m_pSamples = new SheetSequenceSample_t[ nTimeSamples ];

		int fTotalSequenceTime = ( int )buf.GetFloat();
		float InterpKnot[SEQUENCE_SAMPLE_COUNT];
		float InterpValue[SEQUENCE_SAMPLE_COUNT];
		SheetSequenceSample_t Samples[SEQUENCE_SAMPLE_COUNT];
		float fCurTime = 0.;
		for( int nFrm = 0 ; nFrm < nFrameCount; nFrm++ )
		{
			float fThisDuration = buf.GetFloat();
			InterpValue[ nFrm ] = nFrm;
			InterpKnot [ nFrm ] = SEQUENCE_SAMPLE_COUNT*( fCurTime/ fTotalSequenceTime );
			SheetSequenceSample_t &seq = Samples[ nFrm ];
			seq.m_fBlendFactor = 0.0f;
			for(int nImage = 0 ; nImage< nNumCoordsPerFrame; nImage++ )
			{
				SequenceSampleTextureCoords_t &s=seq.m_TextureCoordData[nImage];
				s.m_fLeft_U0	= buf.GetFloat();
				s.m_fTop_V0		= buf.GetFloat();
				s.m_fRight_U0	= buf.GetFloat();
				s.m_fBottom_V0	= buf.GetFloat();
			}
			if ( nNumCoordsPerFrame == 1 )
				seq.CopyFirstFrameToOthers();
			fCurTime += fThisDuration;
			m_SheetInfo[nSequenceIndex].m_flFrameSpan = fCurTime; 
		}
		// now, fill in the whole table
		for( int nIdx = 0; nIdx < nTimeSamples; nIdx++ )
		{
			float flIdxA, flIdxB, flInterp;
			GetInterpolationData( InterpKnot, InterpValue, nFrameCount,
								  SEQUENCE_SAMPLE_COUNT,
								  nIdx, 
								  ! ( m_SheetInfo[nSequenceIndex].m_SeqFlags & SEQ_FLAG_CLAMP ),
								  &flIdxA, &flIdxB, &flInterp );
			SheetSequenceSample_t sA = Samples[(int) flIdxA];
			SheetSequenceSample_t sB = Samples[(int) flIdxB];
			SheetSequenceSample_t &oseq = m_SheetInfo[nSequenceIndex].m_pSamples[nIdx];

			oseq.m_fBlendFactor = flInterp;
			for(int nImage = 0 ; nImage< MAX_IMAGES_PER_FRAME_IN_MEMORY; nImage++ )
			{
				SequenceSampleTextureCoords_t &src0=sA.m_TextureCoordData[nImage];
				SequenceSampleTextureCoords_t &src1=sB.m_TextureCoordData[nImage];
				SequenceSampleTextureCoords_t &o=oseq.m_TextureCoordData[nImage];
				o.m_fLeft_U0 = src0.m_fLeft_U0;
				o.m_fTop_V0	= src0.m_fTop_V0;
				o.m_fRight_U0 = src0.m_fRight_U0;
				o.m_fBottom_V0 = src0.m_fBottom_V0;
				o.m_fLeft_U1 = src1.m_fLeft_U0;
				o.m_fTop_V1	= src1.m_fTop_V0;
				o.m_fRight_U1 = src1.m_fRight_U0;
				o.m_fBottom_V1 = src1.m_fBottom_V0;
			}
		}
	}
	// now, fill in all unseen sequences with copies of the first seen sequence to prevent crashes
	// while editing
	int nFirstSequence = -1;
	for(int i= 0 ; i < m_SheetInfo.Count(); i++)
	{
		if ( m_SheetInfo[i].m_pSamples )
		{
			nFirstSequence = i;
			break;
		}
	}

	if ( nFirstSequence != -1 )
	{
		for(int i=0 ; i < m_SheetInfo.Count(); i++)
		{
			if ( m_SheetInfo[i].m_pSamples == NULL )
			{
				m_SheetInfo[i].m_pSamples = m_SheetInfo[nFirstSequence].m_pSamples;
				m_SheetInfo[i].m_SeqFlags = m_SheetInfo[nFirstSequence].m_SeqFlags;
				m_SheetInfo[i].m_nNumFrames = m_SheetInfo[nFirstSequence].m_nNumFrames;
				Assert( m_SheetInfo[i].m_nNumFrames >= 1 );
				m_SheetInfo[i].m_bSequenceIsCopyOfAnotherSequence = true;
			}
		}
	}
}

CSheet::~CSheet( void )
{
	for( int i = 0; i < m_SheetInfo.Count(); i++ )
	{
		if ( m_SheetInfo[i].m_pSamples && ( !m_SheetInfo[i].m_bSequenceIsCopyOfAnotherSequence ) )
		{
			delete[] m_SheetInfo[i].m_pSamples;
		}
	}
}

const SheetSequenceSample_t *CSheet::GetSampleForSequence( float flAge, float flAgeScale, int nSequence, bool bForceLoop )
{
	if ( m_SheetInfo[nSequence].m_nNumFrames == 1 )
		return (const SheetSequenceSample_t *) &m_SheetInfo[nSequence].m_pSamples[0];

	flAge *= flAgeScale;
	unsigned int nFrame = ( int )flAge;

	if ( ( m_SheetInfo[nSequence].m_SeqFlags & SEQ_FLAG_CLAMP ) && !bForceLoop )
	{
		nFrame = MIN( nFrame, SEQUENCE_SAMPLE_COUNT-1 );
	}
	else
	{
		nFrame &= SEQUENCE_SAMPLE_COUNT-1;
	}

	return (const SheetSequenceSample_t *) &m_SheetInfo[nSequence].m_pSamples[nFrame];
}


//////////////////////////////////////////////////////////////////////////

CSheetExtended::CSheetExtended( IMaterial* pMaterial )
{
	m_Material.Init( pMaterial );
	m_pSheetData = NULL;
	LoadFromMaterial(pMaterial);
}

CSheetExtended::~CSheetExtended()
{
	delete m_pSheetData;
}

void CSheetExtended::LoadFromMaterial( IMaterial* pMaterial )
{
	if ( pMaterial == NULL )
		return;

	bool bFound = false;
	IMaterialVar *pVar = pMaterial->FindVar( "$basetexture", &bFound );
	if ( !pVar || !bFound || !pVar->IsDefined() )
		return;

	ITexture *pTex = pVar->GetTextureValue();
	if ( !pTex || pTex->IsError() )
		return;

	size_t nBytes;
	void const *pSheetData = pTex->GetResourceData( VTF_RSRC_SHEET, &nBytes );

	if ( pSheetData )
	{
		CUtlBuffer bufLoad( pSheetData, nBytes, CUtlBuffer::READ_ONLY );
		LoadFromBuffer( bufLoad );
	}
}

void CSheetExtended::LoadFromBuffer( CUtlBuffer& buf )
{
	m_pSheetData = new CSheet(buf);
}

int CSheetExtended::GetSheetSequenceCount()
{
	if ( m_pSheetData == NULL )
	{
		return 0;
	}

	int nUniqueSequences = 0;

	for ( int i = 0; i < m_pSheetData->m_SheetInfo.Count(); ++i )
	{
		if ( !m_pSheetData->m_SheetInfo[i].m_bSequenceIsCopyOfAnotherSequence )
		{
			nUniqueSequences++;
		}
	}

	return nUniqueSequences;
}

int CSheetExtended::GetNthSequenceIndex( int nSequenceNumber )
{
	if ( m_pSheetData == NULL )
	{
		return 0;
	}

	int nCountValidSequences = 0;
	for ( int i = 0; i < m_pSheetData->m_SheetInfo.Count(); ++i )
	{
		if ( !m_pSheetData->m_SheetInfo[i].m_bSequenceIsCopyOfAnotherSequence )
		{
			if ( nCountValidSequences == nSequenceNumber )
			{
				return i;
			}
			else
			{
				nCountValidSequences++;
			}
		}
	}

	return 0;
}

static SheetSequenceSample_t s_DefaultSheetSequence = 
{
	0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f,

	1.0f,
};

const SheetSequenceSample_t *CSheetExtended::GetSampleForSequence( float flAge, float flAgeScale, int nSequence, bool bForceLoop )
{
	if ( m_pSheetData == NULL )
		return &s_DefaultSheetSequence;
	return m_pSheetData->GetSampleForSequence( flAge, flAgeScale, nSequence, bForceLoop );
}

float CSheetExtended::GetSequenceTimeSpan( int nSequenceIndex )
{
	if ( m_pSheetData == NULL )
	{
		return 0.f;
	}
	
	return m_pSheetData->m_SheetInfo[nSequenceIndex].m_flFrameSpan;
}

bool CSheetExtended::ValidSheetData()
{
	return (m_pSheetData != NULL);
}

bool CSheetExtended::SequenceHasAlphaData( int nSequenceIndex )
{
	return !(m_pSheetData->m_SheetInfo[nSequenceIndex].m_SeqFlags & SEQ_FLAG_NO_ALPHA);
}

bool CSheetExtended::SequenceHasColorData( int nSequenceIndex )
{
	return !(m_pSheetData->m_SheetInfo[nSequenceIndex].m_SeqFlags & SEQ_FLAG_NO_COLOR);
}

inline void TexCoords0( CMeshBuilder& meshBuilder, int nChannel, const SequenceSampleTextureCoords_t* pSample )
{
	meshBuilder.TexCoord4f( nChannel, pSample->m_fLeft_U0, pSample->m_fTop_V0, pSample->m_fRight_U0, pSample->m_fBottom_V0 );
}

inline void TexCoords1( CMeshBuilder& meshBuilder, int nChannel, const SequenceSampleTextureCoords_t* pSample )
{
	meshBuilder.TexCoord4f( nChannel, pSample->m_fLeft_U1, pSample->m_fTop_V1, pSample->m_fRight_U1, pSample->m_fBottom_V1 );
}

inline void SpriteCardVert( CMeshBuilder& meshBuilder,
						    const Vector &vCenter, 
						    const float flRadius,
							const SheetSequenceSample_t * pSample,
						    const SequenceSampleTextureCoords_t *pSample0,
						    const SequenceSampleTextureCoords_t *pSecondTexture0,
							const SheetSequenceSample_t *pSample1Data,
						    const SequenceSampleTextureCoords_t *pSample1,
							float flChannel3U, float flChannel3V )
{
	meshBuilder.Position3fv( vCenter.Base() );
	TexCoords0( meshBuilder, 0, pSample0 );
	TexCoords1( meshBuilder, 1, pSample0 );
	meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, 0.0f, flRadius, 0.0f );
	meshBuilder.TexCoord2f( 3, flChannel3U, flChannel3V );
	TexCoords0( meshBuilder, 4, pSecondTexture0 );

	if ( pSample1 )
	{
		TexCoords0( meshBuilder, 5, pSample1 );
		TexCoords1( meshBuilder, 6, pSample1 );
		meshBuilder.TexCoord4f( 7, pSample1Data->m_fBlendFactor, 0, 0, 0 );
	}
}

void CSheetExtended::DrawSheet( IMesh *pMesh, const Vector &vCenter, float flRadius, int nSheetSequence, float flAge, float flSheetPreviewSpeed, bool bLoopSheetPreview, int nSecondarySequence, bool bOverrideSpriteCard )
{
	// nSecondarySequence
	bool bSpriteCardMaterial = false;

	bSpriteCardMaterial = !bOverrideSpriteCard && m_Material && m_Material->IsSpriteCard();

	const SheetSequenceSample_t *pSample = GetSampleForSequence( flAge, flSheetPreviewSpeed, nSheetSequence, bLoopSheetPreview );
	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	const SequenceSampleTextureCoords_t *pSecondTexture0 = &(pSample->m_TextureCoordData[1]);
	const SheetSequenceSample_t *pSample1Data = NULL;
	const SequenceSampleTextureCoords_t *pSample1 = NULL;
	
	if ( (nSecondarySequence != -1) && IsMaterialDualSequence( m_Material ) )
	{
		const float SECONDARY_AGE_MULTIPLIER = 0.1f; // hardcoded 'best guess' at relative speed, since we don't want a whole UI for a second speed
		float flSecondaryAge = flAge * SECONDARY_AGE_MULTIPLIER;
		pSample1Data = GetSampleForSequence( flSecondaryAge, flSheetPreviewSpeed, nSecondarySequence, bLoopSheetPreview );
		pSample1 = &(pSample1Data->m_TextureCoordData[0]);
	}

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	if ( bSpriteCardMaterial )
	{
		SpriteCardVert( meshBuilder, vCenter, flRadius, pSample, pSample0, pSecondTexture0, pSample1Data, pSample1, 0, 0 );
	}
	else
	{
		meshBuilder.Position3fv( (vCenter + Vector(-flRadius,-flRadius,0)).Base() );
		meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );
	}
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	if ( bSpriteCardMaterial )
	{
		SpriteCardVert( meshBuilder, vCenter, flRadius, pSample, pSample0, pSecondTexture0, pSample1Data, pSample1, 1, 0 );
	}
	else
	{
		meshBuilder.Position3fv( (vCenter + Vector(+flRadius,-flRadius,0)).Base() );
		meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );
	}
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	if ( bSpriteCardMaterial )
	{
		SpriteCardVert( meshBuilder, vCenter, flRadius, pSample, pSample0, pSecondTexture0, pSample1Data, pSample1, 1, 1 );
	}
	else
	{
		meshBuilder.Position3fv( (vCenter + Vector(+flRadius,+flRadius,0)).Base() );
		meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	}
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	if ( bSpriteCardMaterial )
	{
		SpriteCardVert( meshBuilder, vCenter, flRadius, pSample, pSample0, pSecondTexture0, pSample1Data, pSample1, 0, 1 );
	}
	else
	{
		meshBuilder.Position3fv( (vCenter + Vector(-flRadius,+flRadius,0)).Base() );
		meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );
	}
	meshBuilder.Color4ub( 255, 255, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

bool CSheetExtended::IsMaterialDualSequence( IMaterial* pMat )
{
	if ( !pMat )
	{
		return false;
	}

	bool bFound = false;
	IMaterialVar *pVar = pMat->FindVar( "$DUALSEQUENCE", &bFound );

	return ( pVar && bFound && pVar->IsDefined() && pVar->GetIntValue() );
}

bool CSheetExtended::IsMaterialSeparateAlphaColorMaterial( IMaterial* pMat )
{
	if ( !pMat || !IsMaterialDualSequence(pMat) )
	{
		return false;
	}

	bool bFound = false;
	IMaterialVar *pVar = pMat->FindVar( "$SEQUENCE_BLEND_MODE", &bFound );

	if ( !pVar || !bFound || !pVar->IsDefined() )
		return false;

	return (pVar->GetIntValue() == 1);
}
