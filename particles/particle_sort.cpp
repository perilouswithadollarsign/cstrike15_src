//===== Copyright 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: particle system code
//
//===========================================================================//

#include <algorithm>
#include "tier0/platform.h"
#include "tier0/vprof.h"
#include "particles/particles.h"
#include "particles_internal.h"
#include "bitmap/psheet.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define SORTBUFSIZE0 (( MAX_PARTICLES_IN_A_SYSTEM + 4 ) * sizeof( ParticleRenderData_t ) )
#define SORTBUFSIZE1 ( ( 1 + MAX_PARTICLES_IN_A_SYSTEM / 4 )* sizeof( ParticleFullRenderData_Scalar_View ) )
#define SORTBUFSIZE2 ( ( 1 + MAX_PARTICLES_IN_A_SYSTEM / 4 )* sizeof( ParticleRenderDataWithOutlineInformation_Scalar_View ) )

static ALIGN16 uint8 s_SortBuffer[ COMPILETIME_MAX( COMPILETIME_MAX( SORTBUFSIZE0, SORTBUFSIZE1 ), SORTBUFSIZE2 )] ALIGN16_POST;

static ALIGN16 ParticleFullRenderData_Scalar_View *s_pParticlePtrs[MAX_PARTICLES_IN_A_SYSTEM] ALIGN16_POST;



enum EParticleSortKeyType
{
	SORT_KEY_NONE,
	SORT_KEY_DISTANCE,
	SORT_KEY_CREATION_TIME,
};


void C4VInterpolatedAttributeIterator::Init( int nAttribute, CParticleCollection *pParticles )
{
	// initializing this is somewhat complicated by the behavior of prev_xyz and xyz

	m_pData = pParticles->Get4VAttributePtr( nAttribute, &m_nStride );
	Assert( pParticles->GetPrevAttributeMemory() );
	if ( m_nStride )
	{
		m_nOldDataOffset = 
			pParticles->m_PreviousFrameAttributes.ByteAddress( nAttribute ) - pParticles->m_ParticleAttributes.ByteAddress( nAttribute );
	}
	else
	{
		// it's constant data
		m_nOldDataOffset = 0;
	}
}

template<EParticleSortKeyType eSortKeyMode, bool bCull> void s_GenerateData( void *pOutData, Vector CameraPos, Vector *pCameraFwd, 
																			 CParticleVisibilityData *pVisibilityData, CParticleCollection *pParticles )
{
	fltx4 *pOutUnSorted = reinterpret_cast<fltx4 *>( pOutData );

	C4VAttributeIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
	CM128AttributeIterator pCreationTimeStamp( PARTICLE_ATTRIBUTE_CREATION_TIME, pParticles );
	CM128AttributeIterator pAlpha( PARTICLE_ATTRIBUTE_ALPHA, pParticles );
	CM128AttributeIterator pAlpha2( PARTICLE_ATTRIBUTE_ALPHA2, pParticles );
 	CM128AttributeIterator pRadius( PARTICLE_ATTRIBUTE_RADIUS, pParticles );

	int nParticles = pParticles->m_nActiveParticles;

	FourVectors EyePos;
	EyePos.DuplicateVector( CameraPos );
	FourVectors v4Fwd;
	if ( bCull )
		v4Fwd.DuplicateVector( *pCameraFwd );


	fltx4 fl4AlphaVis = ReplicateX4( pVisibilityData->m_flAlphaVisibility );
	fltx4 fl4RadVis = ReplicateX4( pVisibilityData->m_flRadiusVisibility );

	// indexing. We will generate the index as float and use magicf2i to convert to integer
	fltx4 fl4OutIdx = g_SIMD_0123; // 0 1 2 3

	fl4OutIdx = AddSIMD( fl4OutIdx, Four_2ToThe23s);							// fix as int

	bool bUseVis = pVisibilityData->m_bUseVisibility;

	fltx4 fl4AlphaScale = ReplicateX4( 255.0 );
	fltx4 fl4SortKey = Four_Zeros;

	do
	{
		fltx4 fl4FinalAlpha = MulSIMD( *pAlpha, *pAlpha2 );
		fltx4 fl4FinalRadius = *pRadius;

		if ( bUseVis )
		{
			fl4FinalAlpha = MaxSIMD ( Four_Zeros, MinSIMD( Four_Ones, MulSIMD( fl4FinalAlpha, fl4AlphaVis) ) );
			fl4FinalRadius = MulSIMD( fl4FinalRadius, fl4RadVis );
		}
		// convert float 0..1 to int 0..255
		fl4FinalAlpha = AddSIMD( MulSIMD( fl4FinalAlpha, fl4AlphaScale ), Four_2ToThe23s );

		if ( eSortKeyMode == SORT_KEY_CREATION_TIME )
		{
			fl4SortKey = *pCreationTimeStamp;
		}
		if ( bCull || ( eSortKeyMode == SORT_KEY_DISTANCE ) )
		{
			fltx4 fl4X = pXYZ->x;
			fltx4 fl4Y = pXYZ->y;
			fltx4 fl4Z = pXYZ->z;
			fltx4 Xdiff = SubSIMD( fl4X, EyePos.x );
			fltx4 Ydiff = SubSIMD( fl4Y, EyePos.y );
			fltx4 Zdiff = SubSIMD( fl4Z, EyePos.z );
			if ( bCull )
			{
				fltx4 dot = AddSIMD( MulSIMD( Xdiff, v4Fwd.x ),
									 AddSIMD(
										 MulSIMD( Ydiff, v4Fwd.y ),
										 MulSIMD( Zdiff, v4Fwd.z ) ) );
				fl4FinalAlpha = AndSIMD( fl4FinalAlpha, CmpGeSIMD( dot, Four_Zeros ) );
			}
			if ( eSortKeyMode == SORT_KEY_DISTANCE )
			{
				fl4SortKey = AddSIMD( MulSIMD( Xdiff, Xdiff ),
									  AddSIMD( MulSIMD( Ydiff, Ydiff ),
											   MulSIMD( Zdiff, Zdiff ) ) );
			}
		}
		// now, we will use simd transpose to write the output
		fltx4 i4Indices = AndSIMD( fl4OutIdx, 	LoadAlignedSIMD( (float *) g_SIMD_Low16BitsMask ) );
		TransposeSIMD( fl4SortKey, i4Indices, fl4FinalRadius, fl4FinalAlpha );
		pOutUnSorted[0] = fl4SortKey;
		pOutUnSorted[1] = i4Indices;
		pOutUnSorted[2] = fl4FinalRadius;
		pOutUnSorted[3] = fl4FinalAlpha;
		
		pOutUnSorted += 4;

		fl4OutIdx = AddSIMD( fl4OutIdx, Four_Fours );

		nParticles -= 4;

		++pXYZ;
		++pAlpha;
		++pAlpha2;
		++pRadius;
	} while( nParticles > 0 );								// we're not called with 0
}



#define TREATASINT(x) ( *(  ( (int32 const *)( &(x) ) ) ) )

static bool SortLessFunc( const ParticleRenderData_t &left, const ParticleRenderData_t &right )
{
	return TREATASINT( left.m_flSortKey ) < TREATASINT( right.m_flSortKey );
	
}


int CParticleCollection::GenerateSortedIndexList( ParticleRenderData_t *pOut, Vector vecCamera, CParticleVisibilityData *pVisibilityData, bool bSorted )
{
	VPROF_BUDGET( "CParticleCollection::GenerateSortedIndexList", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	int nParticles = m_nActiveParticles;
	if ( bSorted )
	{
		s_GenerateData<SORT_KEY_DISTANCE, false>( pOut, vecCamera, NULL, pVisibilityData, this );
	}
	else
		s_GenerateData<SORT_KEY_NONE, false>( pOut, vecCamera, NULL, pVisibilityData, this );

	if ( bSorted )
	{
		// sort the output in place
		std::make_heap( pOut, pOut + nParticles, SortLessFunc );
		std::sort_heap( pOut, pOut + nParticles, SortLessFunc );
	}
	return nParticles;
}

int CParticleCollection::GenerateCulledSortedIndexList( 
	ParticleRenderData_t *pOut, Vector vecCamera, Vector vecFwd, CParticleVisibilityData *pVisibilityData, bool bSorted )
{
	VPROF_BUDGET( "CParticleCollection::GenerateSortedIndexList", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	int nParticles = m_nActiveParticles;
	if ( bSorted )
	{
		s_GenerateData<SORT_KEY_DISTANCE, true>( pOut, vecCamera, &vecFwd, pVisibilityData, this );
	}
	else
		s_GenerateData<SORT_KEY_NONE, true>( pOut, vecCamera, &vecFwd, pVisibilityData, this );

#ifndef SWDS
	if ( bSorted )
	{
		// sort the output in place
		std::make_heap( pOut, pOut + nParticles, SortLessFunc );
		std::sort_heap( pOut, pOut + nParticles, SortLessFunc );
	}
#endif
	return nParticles;
}

const ParticleRenderData_t *CParticleCollection::GetRenderList( IMatRenderContext *pRenderContext,
																bool bSorted, int *pNparticles,
																CParticleVisibilityData *pVisibilityData)
{
	if ( bSorted )
		bSorted = m_pDef->m_bShouldSort;

	Vector vecCamera;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );
	ParticleRenderData_t *pOut = ( ParticleRenderData_t * ) s_SortBuffer;
	// check if the camera is inside the bounding box to see whether culling is worth it
	int nParticles;

	if ( vecCamera.WithinAABox( m_MinBounds, m_MaxBounds ) )
	{
		Vector vecFwd, vecRight, vecUp;
		pRenderContext->GetWorldSpaceCameraVectors( &vecFwd, &vecRight, &vecUp );
		
		nParticles = GenerateCulledSortedIndexList( pOut,
												   vecCamera, vecFwd,
												   pVisibilityData, bSorted );
	}
	else
	{
		// outside the bounds. don't bother agressive culling
		nParticles = GenerateSortedIndexList( pOut, vecCamera, pVisibilityData, bSorted );
	}
	*pNparticles = nParticles;
	return pOut + nParticles;
}

template<EParticleSortKeyType eSortKeyMode, bool bCull, bool bLerpCoords, class OutType_t, bool bDoColor2, bool bDoNormalVector, class VECTORITERATOR, class SCALARITERATOR>
void GenerateExtendedData(
	void *pOutbuf, ParticleFullRenderData_Scalar_View **pIndexBuffer, 
	Vector CameraPos, Vector *pCameraFwd, CParticleVisibilityData *pVisibilityData, CParticleCollection *pParticles,
	float flInterpT )
{
	OutType_t * RESTRICT pOutUnSorted = reinterpret_cast<OutType_t*>( pOutbuf );

	// interpolated values
	VECTORITERATOR pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
	VECTORITERATOR pRGB( PARTICLE_ATTRIBUTE_TINT_RGB, pParticles );
	VECTORITERATOR pRGB2;
	SCALARITERATOR pAlpha( PARTICLE_ATTRIBUTE_ALPHA, pParticles );
	SCALARITERATOR pAlpha2( PARTICLE_ATTRIBUTE_ALPHA2, pParticles );
 	SCALARITERATOR pRadius( PARTICLE_ATTRIBUTE_RADIUS, pParticles );
	SCALARITERATOR pRot( PARTICLE_ATTRIBUTE_ROTATION, pParticles );
	SCALARITERATOR pYaw( PARTICLE_ATTRIBUTE_YAW, pParticles );
	SCALARITERATOR pGlowAlpha;

 	// non-interpolated values
	CM128AttributeIterator pSeq( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, pParticles );
 	CM128AttributeIterator pSeq1( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1, pParticles );
	CM128AttributeIterator pCreationTimeStamp( PARTICLE_ATTRIBUTE_CREATION_TIME, pParticles );

	if ( bDoColor2 )
	{
		pGlowAlpha.Init( PARTICLE_ATTRIBUTE_GLOW_ALPHA, pParticles );
		pRGB2.Init( PARTICLE_ATTRIBUTE_GLOW_RGB, pParticles );
	}

	C4VAttributeIterator pNormal;
	if ( bDoNormalVector )
	{
		pNormal.Init( PARTICLE_ATTRIBUTE_NORMAL, pParticles );
	}

	int nParticles = pParticles->m_nActiveParticles;
	fltx4 fl4CurTime;
	fltx4 fl4MaximumCreationTimeToDraw = Four_2ToThe23s;
	
	if ( bLerpCoords )
	{
		fl4CurTime = ReplicateX4( pParticles->m_flTargetDrawTime );
		fl4MaximumCreationTimeToDraw = ReplicateX4( pParticles->m_flPrevSimTime );
	}
	else
	{
		fl4CurTime = pParticles->m_fl4CurTime;
	}
	fltx4 Four_256s = ReplicateX4( 256.0 );
	fltx4 fl4T = ReplicateX4( flInterpT );

	FourVectors EyePos;
	EyePos.DuplicateVector( CameraPos );
	FourVectors v4Fwd;
	if ( bCull )
		v4Fwd.DuplicateVector( *pCameraFwd );


	fltx4 fl4AlphaVis = ReplicateX4( pVisibilityData->m_flAlphaVisibility );
	fltx4 fl4RadVis = ReplicateX4( pVisibilityData->m_flRadiusVisibility );

	bool bUseVis = pVisibilityData->m_bUseVisibility;

	fltx4 fl4AlphaScale = ReplicateX4( 255.0 );
	uint8 **pOutPtrs = reinterpret_cast<uint8 **>( pIndexBuffer );

	do
	{
		// Note: UNDER NO CIRCUMSTANCES should you write through these
		// pointers inside the scope of this function! pOutUnSorted is 
		// marked as __restrict, meaning that we have promised the compiler
		// that it is unaliased. Therefore any writes through an alias
		// will cause DOOM.
		pOutPtrs[0] = reinterpret_cast<uint8 *>( pOutUnSorted );
		pOutPtrs[1] = reinterpret_cast<uint8 *>( pOutUnSorted ) + sizeof( float );
		pOutPtrs[2] = reinterpret_cast<uint8 *>( pOutUnSorted ) + 2 * sizeof( float );
		pOutPtrs[3] = reinterpret_cast<uint8 *>( pOutUnSorted ) + 3 * sizeof( float );
	

		fltx4 fl4FinalAlpha = MulSIMD( pAlpha( fl4T ), pAlpha2( fl4T ) );
		fltx4 fl4FinalRadius = pRadius( fl4T );

		if ( bUseVis )
		{
			fl4FinalAlpha = MaxSIMD ( Four_Zeros, MinSIMD( Four_Ones, MulSIMD( fl4FinalAlpha, fl4AlphaVis) ) );
			fl4FinalRadius = MulSIMD( fl4FinalRadius, fl4RadVis );
		}
		// convert float 0..1 to int 0..255
		fl4FinalAlpha = AddSIMD( MulSIMD( fl4FinalAlpha, fl4AlphaScale ), Four_2ToThe23s );

		fltx4 fl4X = pXYZ.X( fl4T );
		fltx4 fl4Y = pXYZ.Y( fl4T );
		fltx4 fl4Z = pXYZ.Z( fl4T );
		if ( eSortKeyMode == SORT_KEY_CREATION_TIME )
		{
			pOutUnSorted->m_fl4SortKey = *pCreationTimeStamp;
		}
		if ( bCull || ( eSortKeyMode == SORT_KEY_DISTANCE ) )
		{
			fltx4 Xdiff = SubSIMD( fl4X, EyePos.x );
			fltx4 Ydiff = SubSIMD( fl4Y, EyePos.y );
			fltx4 Zdiff = SubSIMD( fl4Z, EyePos.z );
			if ( bCull )
			{
				fltx4 dot = AddSIMD( MulSIMD( Xdiff, v4Fwd.x ),
									 AddSIMD(
										 MulSIMD( Ydiff, v4Fwd.y ),
										 MulSIMD( Zdiff, v4Fwd.z ) ) );
				fl4FinalAlpha = AndSIMD( fl4FinalAlpha, CmpGeSIMD( dot, Four_Zeros ) );
			}
			if ( eSortKeyMode == SORT_KEY_DISTANCE )
			{
				pOutUnSorted->m_fl4SortKey = AddSIMD( MulSIMD( Xdiff, Xdiff ),
													  AddSIMD( MulSIMD( Ydiff, Ydiff ),
															   MulSIMD( Zdiff, Zdiff ) ) );
			}
		}
		fltx4 fl4Age = SubSIMD( fl4CurTime, *pCreationTimeStamp );
		// if we are lerping, we need to supress particles which didn't exist on the last sim
		if ( bLerpCoords )
		{
			fl4FinalAlpha = AndSIMD( fl4FinalAlpha, CmpLtSIMD( *pCreationTimeStamp, fl4MaximumCreationTimeToDraw ) );
		}
		pOutUnSorted->m_fl4XYZ.x = fl4X;
		pOutUnSorted->m_fl4XYZ.y = fl4Y;
		pOutUnSorted->m_fl4XYZ.z = fl4Z;
		pOutUnSorted->m_fl4Alpha = fl4FinalAlpha;
		pOutUnSorted->m_fl4Red = AddSIMD( MulSIMD( pRGB.X( fl4T ), fl4AlphaScale ), Four_2ToThe23s );
		pOutUnSorted->m_fl4Green = AddSIMD( MulSIMD( pRGB.Y( fl4T ), fl4AlphaScale ), Four_2ToThe23s );
		pOutUnSorted->m_fl4Blue = AddSIMD( MulSIMD( pRGB.Z( fl4T ), fl4AlphaScale ), Four_2ToThe23s );
		pOutUnSorted->m_fl4Radius = fl4FinalRadius;
		pOutUnSorted->m_fl4AnimationTimeValue = fl4Age;
		pOutUnSorted->m_fl4Rotation = pRot( fl4T );
		pOutUnSorted->m_fl4Yaw = pYaw( fl4T );
 		if ( pSeq1.Stride() )
		{
 			pOutUnSorted->m_fl4SequenceID = AddSIMD( AddSIMD( *pSeq, MulSIMD( Four_256s, *pSeq1 ) ), Four_2ToThe23s );
		}
 		else
		{
			pOutUnSorted->m_fl4SequenceID = AddSIMD( *pSeq, Four_2ToThe23s );
		}

		if ( bDoColor2 )
		{
			pOutUnSorted->SetARGB2( pRGB2.X( fl4T ), pRGB2.Y( fl4T ), pRGB2.Z( fl4T ), *pGlowAlpha );
			++pRGB2;
			++pGlowAlpha;
		}
		if ( bDoNormalVector )
		{
			pOutUnSorted->SetNormal( pNormal.X( fl4T ), pNormal.Y( fl4T ), pNormal.Z( fl4T ) );
			++pNormal;
		}


		pOutUnSorted++;
		nParticles -= 4;
		pOutPtrs += 4; 

		++pXYZ;
		++pAlpha;
		++pAlpha2;
		++pRadius;
		++pRGB;
		++pYaw;
		++pRot;
		++pSeq;
		++pSeq1;
		++pCreationTimeStamp;

	} while( nParticles > 0 );								// we're not called with 0

}

template<EParticleSortKeyType eSortKeyMode, bool bCull, bool bLerpCoords, class OutType_t, bool bDoColor2, 
		 bool bDoNormalVector>
void GenerateExtendedData(
	void *pOutbuf, ParticleFullRenderData_Scalar_View **pIndexBuffer, 
	Vector CameraPos, Vector *pCameraFwd, CParticleVisibilityData *pVisibilityData, CParticleCollection *pParticles,
	float flInterpT )
{
	if ( bLerpCoords )
	{
		GenerateExtendedData<eSortKeyMode, bCull, true, OutType_t,
			bDoColor2, bDoNormalVector, C4VInterpolatedAttributeIterator, CM128InterpolatedAttributeIterator>(
				pOutbuf, pIndexBuffer, CameraPos, pCameraFwd, pVisibilityData, pParticles, flInterpT );

	}
	else
	{
		GenerateExtendedData<eSortKeyMode, bCull, false, OutType_t,
			bDoColor2, bDoNormalVector, C4VAttributeIterator, CM128AttributeIterator>(
				pOutbuf, pIndexBuffer, CameraPos, pCameraFwd, pVisibilityData, pParticles, flInterpT );
	}

}


template<bool bCull, bool bLerpCoords, class OutType_t, bool bDoColor2, bool bDoNormal>
void s_GenerateExtendedData(
	void *pOutbuf, ParticleFullRenderData_Scalar_View **pIndexBuffer, 
	Vector CameraPos, Vector *pCameraFwd, CParticleVisibilityData *pVisibilityData, CParticleCollection *pParticles,
	float flInterpT, bool bSort )
{
	if ( bSort )
	{
		GenerateExtendedData<SORT_KEY_DISTANCE, bCull, bLerpCoords, OutType_t, bDoColor2, bDoNormal>( 
			pOutbuf, pIndexBuffer,
			CameraPos, pCameraFwd, pVisibilityData,
			pParticles, flInterpT );
	}
	else
	{
		GenerateExtendedData<SORT_KEY_NONE, bCull, bLerpCoords, OutType_t, bDoColor2, bDoNormal>(
			pOutbuf, pIndexBuffer,
			CameraPos, pCameraFwd, pVisibilityData,
			pParticles, flInterpT );
	}

}



static bool SortLessFuncExtended( ParticleFullRenderData_Scalar_View * const &left, const ParticleFullRenderData_Scalar_View * const &right )
{
	return left->m_nSortKey < right->m_nSortKey;
	
}

int GenerateExtendedSortedIndexList( Vector vecCamera, Vector *pCameraFwd, CParticleVisibilityData *pVisibilityData, 
									 CParticleCollection *pParticles, bool bSorted, void *pOutBuf, 
									 ParticleFullRenderData_Scalar_View **pParticlePtrs )
{
	// check interpolation
	if ( pParticles->IsUsingInterpolatedRendering() )
	{
		float t = ( pParticles->m_flTargetDrawTime - pParticles->m_flPrevSimTime ) /
			( pParticles->m_flCurTime - pParticles->m_flPrevSimTime );
		Assert( ( t >= 0.0 ) && ( t <= 1.0 ) );
		s_GenerateExtendedData<false, true, ParticleFullRenderData_SIMD_View, false, false>( pOutBuf, pParticlePtrs, vecCamera, NULL, pVisibilityData, pParticles, t, bSorted );
	}
	else
	{
		s_GenerateExtendedData<false, false, ParticleFullRenderData_SIMD_View, false, false>( pOutBuf, pParticlePtrs, vecCamera, NULL, pVisibilityData, pParticles, 0., bSorted );
	}
	int nParticles = pParticles->m_nActiveParticles;
	if ( bSorted )
	{
		// sort the output in place
		std::make_heap( pParticlePtrs, pParticlePtrs + nParticles, SortLessFuncExtended );
		std::sort_heap( pParticlePtrs, pParticlePtrs + nParticles, SortLessFuncExtended );
	}
	return nParticles;
}

int GenerateExtendedSortedIndexListWithPerParticleGlow(
	Vector vecCamera, Vector *pCameraFwd, CParticleVisibilityData *pVisibilityData, 
	CParticleCollection *pParticles, bool bSorted, void *pOutBuf, 
	ParticleRenderDataWithOutlineInformation_Scalar_View **pParticlePtrs )
{
	// check interpolation
	if ( pParticles->IsUsingInterpolatedRendering() )
	{
		float t = ( pParticles->m_flTargetDrawTime - pParticles->m_flPrevSimTime ) /
			( pParticles->m_flCurTime - pParticles->m_flPrevSimTime );
		Assert( ( t >= 0.0 ) && ( t <= 1.0 ) );
		s_GenerateExtendedData<false, true, ParticleRenderDataWithOutlineInformation_SIMD_View, true, false>(
			pOutBuf, ( ParticleFullRenderData_Scalar_View **) pParticlePtrs, vecCamera, NULL, pVisibilityData, pParticles, t, bSorted );
	}
	else
	{
		s_GenerateExtendedData<false, false, ParticleRenderDataWithOutlineInformation_SIMD_View, true, false>(
			pOutBuf, ( ParticleFullRenderData_Scalar_View **) pParticlePtrs, vecCamera, NULL, pVisibilityData, pParticles, 0., bSorted );
	}
	int nParticles = pParticles->m_nActiveParticles;
	if ( bSorted )
	{
		// sort the output in place
		std::make_heap( pParticlePtrs, pParticlePtrs + nParticles, SortLessFuncExtended );
		std::sort_heap( pParticlePtrs, pParticlePtrs + nParticles, SortLessFuncExtended );
	}
	return nParticles;
}

int GenerateExtendedSortedIndexListWithNormals(
	Vector vecCamera, Vector *pCameraFwd, CParticleVisibilityData *pVisibilityData, 
	CParticleCollection *pParticles, bool bSorted, void *pOutBuf, 
	ParticleRenderDataWithNormal_Scalar_View **pParticlePtrs )
{
	// check interpolation
	if ( pParticles->IsUsingInterpolatedRendering() )
	{
		float t = ( pParticles->m_flTargetDrawTime - pParticles->m_flPrevSimTime ) /
			( pParticles->m_flCurTime - pParticles->m_flPrevSimTime );
		Assert( ( t >= 0.0 ) && ( t <= 1.0 ) );
		s_GenerateExtendedData<false, true, ParticleRenderDataWithNormal_SIMD_View, false, true>(
			pOutBuf, ( ParticleFullRenderData_Scalar_View **) pParticlePtrs, vecCamera, NULL, pVisibilityData, pParticles, t, bSorted );
	}
	else
	{
		s_GenerateExtendedData<false, false, ParticleRenderDataWithNormal_SIMD_View, false, true>(
			pOutBuf, ( ParticleFullRenderData_Scalar_View **) pParticlePtrs, vecCamera, NULL, pVisibilityData, pParticles, 0., bSorted );
	}
	int nParticles = pParticles->m_nActiveParticles;
	if ( bSorted )
	{
		// sort the output in place
		std::make_heap( pParticlePtrs, pParticlePtrs + nParticles, SortLessFuncExtended );
		std::sort_heap( pParticlePtrs, pParticlePtrs + nParticles, SortLessFuncExtended );
	}
	return nParticles;
}




ParticleFullRenderData_Scalar_View **GetExtendedRenderList( CParticleCollection *pParticles,
															IMatRenderContext *pRenderContext,
															bool bSorted, int *pNparticles,
															CParticleVisibilityData *pVisibilityData)
{
	Assert( sizeof( ParticleFullRenderData_Scalar_View ) == sizeof( ParticleFullRenderData_SIMD_View ) );
	if ( bSorted )
		bSorted = pParticles->m_pDef->m_bShouldSort;

	Vector vecCamera;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );
	int nParticles = GenerateExtendedSortedIndexList( vecCamera, NULL, pVisibilityData, pParticles, bSorted, s_SortBuffer, s_pParticlePtrs );
	*pNparticles = nParticles;
	return s_pParticlePtrs + nParticles;
}

ParticleRenderDataWithOutlineInformation_Scalar_View **GetExtendedRenderListWithPerParticleGlow(
	CParticleCollection *pParticles,
	IMatRenderContext *pRenderContext,
	bool bSorted, int *pNparticles,
	CParticleVisibilityData *pVisibilityData)
{
	Assert( sizeof( ParticleRenderDataWithOutlineInformation_Scalar_View ) == sizeof( ParticleRenderDataWithOutlineInformation_SIMD_View ) );
	if ( bSorted )
		bSorted = pParticles->m_pDef->m_bShouldSort;

	Vector vecCamera;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );
	int nParticles = GenerateExtendedSortedIndexListWithPerParticleGlow(
		vecCamera, NULL, pVisibilityData, pParticles, bSorted, s_SortBuffer, 
		( ParticleRenderDataWithOutlineInformation_Scalar_View ** ) s_pParticlePtrs );
	*pNparticles = nParticles;
	return ( ParticleRenderDataWithOutlineInformation_Scalar_View ** ) ( s_pParticlePtrs + nParticles );
}


ParticleRenderDataWithNormal_Scalar_View **GetExtendedRenderListWithNormals(
	CParticleCollection *pParticles,
	IMatRenderContext *pRenderContext,
	bool bSorted, int *pNparticles,
	CParticleVisibilityData *pVisibilityData)
{
	Assert( sizeof( ParticleRenderDataWithNormal_SIMD_View ) == sizeof( ParticleRenderDataWithNormal_Scalar_View ) );
	if ( bSorted )
		bSorted = pParticles->m_pDef->m_bShouldSort;

	Vector vecCamera;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCamera );
	int nParticles = GenerateExtendedSortedIndexListWithNormals(
		vecCamera, NULL, pVisibilityData, pParticles, bSorted, s_SortBuffer, 
		( ParticleRenderDataWithNormal_Scalar_View ** ) s_pParticlePtrs );
	*pNparticles = nParticles;
	return ( ParticleRenderDataWithNormal_Scalar_View ** ) ( s_pParticlePtrs + nParticles );
}
