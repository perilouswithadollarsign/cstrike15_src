//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "tier0/dbg.h"
#include "mathlib/mathlib.h"
#include "bone_setup.h"
#include <string.h>

#include "collisionutils.h"
#include "vstdlib/random.h"
#include "tier0/vprof.h"
#include "bone_accessor.h"
#include "mathlib/ssequaternion.h"
#include "bitvec.h"
#include "datamanager.h"
#include "convar.h"
#include "tier0/tslist.h"
#include "vphysics_interface.h"
#include "datacache/idatacache.h"
#include "posedebugger.h"
#include "mathlib/softbody.h"
#include "tier0/miniprofiler.h"

#ifdef CLIENT_DLL
	#include "posedebugger.h"
#endif

#include "bone_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//-----------------------------------------------------------------------------
void InitPose(
	const CStudioHdr *pStudioHdr,
	BoneVector pos[], 
	BoneQuaternionAligned q[],
	int boneMask 
	)
{
	// const fltx4 zeroQ = Four_Origin;
	BONE_PROFILE_FUNC();
	SNPROF_ANIM("InitPose");

	if( mstudiolinearbone_t *pLinearBones = pStudioHdr->pLinearBones() )
	{
		int numBones = pStudioHdr->numbones();

		Assert( sizeof(Quaternion) == sizeof(BoneQuaternion) );
		memcpy( q, (((byte *)pLinearBones) + pLinearBones->quatindex), sizeof( Quaternion ) * numBones );

		if( sizeof(Vector) == sizeof(BoneVector) )
		{
			memcpy( pos, (((byte *)pLinearBones) + pLinearBones->posindex), sizeof( Vector ) * numBones );
		}
		else
		{
			Vector *pSrcPos = (Vector *)(((byte *)pLinearBones) + pLinearBones->posindex);
			for( int i = 0; i < pStudioHdr->numbones(); i++ )
			{
				//if( pStudioHdr->boneFlags(  i ) & boneMask ) 
				{
					pos[i] = pSrcPos[i];
				}
			}
		}
	}
	else
	{
		for( int i = 0; i < pStudioHdr->numbones(); i++ )
		{
			if( pStudioHdr->boneFlags(  i ) & boneMask )  
			{
				const mstudiobone_t *pbone = pStudioHdr->pBone( i );
				pos[i] = pbone->pos;
				q[i] = pbone->quat;
			}
			/* // unnecessary to initialize unused bones since they are ignored downstream.
			else
			{
				pos[i].Zero();
				// q[i] = zeroQ;
				StoreAlignedSIMD(q[i].Base(), zeroQ);
			}
			*/
		}
	}
}
	

inline bool PoseIsAllZeros( 
	const CStudioHdr *pStudioHdr,
	int sequence, 
	mstudioseqdesc_t	&seqdesc,
	int i0,
	int i1
	)
{
	int baseanim;
		
	// remove "zero" positional blends
	baseanim = pStudioHdr->iRelativeAnim( sequence, seqdesc.anim(i0  ,i1 ) );
	mstudioanimdesc_t		&anim = ((CStudioHdr *)pStudioHdr)->pAnimdesc( baseanim );
	return (anim.flags & STUDIO_ALLZEROS) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: turn a 2x2 blend into a 3 way triangle blend
// Returns: returns the animination indices and barycentric coordinates of a triangle
//			the triangle is a right triangle, and the diagonal is between elements [0] and [2]
//-----------------------------------------------------------------------------

static ConVar anim_3wayblend( "anim_3wayblend", "1", FCVAR_REPLICATED, "Toggle the 3-way animation blending code." );

void Calc3WayBlendIndices( int i0, int i1, float s0, float s1, const mstudioseqdesc_t &seqdesc, int *pAnimIndices, float *pWeight )
{
	BONE_PROFILE_FUNC();
	// Figure out which bi-section direction we are using to make triangles.
	bool bEven = ( ( ( i0 + i1 ) & 0x1 ) == 0 );

	int x1, y1;
	int x2, y2;
	int x3, y3;

	// diagonal is between elements 1 & 3
	// TL to BR
	if ( bEven )
	{
		if ( s0 > s1 )
		{
			// B
			x1 = 0; y1 = 0;
			x2 = 1; y2 = 0;
			x3 = 1; y3 = 1;
			pWeight[0] = (1.0f - s0);
			pWeight[1] = s0 - s1;
		}
		else
		{
			// C
			x1 = 1; y1 = 1;
			x2 = 0; y2 = 1;
			x3 = 0; y3 = 0;
			pWeight[0] = s0;
			pWeight[1] = s1 - s0;
		}
	}
	// BL to TR
	else
	{
		float flTotal = s0 + s1;

		if( flTotal > 1.0f )
		{
			// D
			x1 = 1; y1 = 0;
			x2 = 1; y2 = 1;
			x3 = 0; y3 = 1;
			pWeight[0] = (1.0f - s1);
			pWeight[1] = s0 - 1.0f + s1;
		}
		else
		{
			// A
			x1 = 0; y1 = 1;
			x2 = 0; y2 = 0;
			x3 = 1; y3 = 0;
			pWeight[0] = s1;
			pWeight[1] = 1.0f - s0 - s1;
		}
	}

	pAnimIndices[0] = seqdesc.anim( i0 + x1, i1 + y1 );
	pAnimIndices[1] = seqdesc.anim( i0 + x2, i1 + y2 );
	pAnimIndices[2] = seqdesc.anim( i0 + x3, i1 + y3 );

	/*
	float w0 = ((x2-x3)*(y3-s1) - (x3-s0)*(y2-y3)) / ((x1-x3)*(y2-y3) - (x2-x3)*(y1-y3));
	float w1 = ((x1-x3)*(y3-s1) - (x3-s0)*(y1-y3)) / ((x2-x3)*(y1-y3) - (x1-x3)*(y2-y3));
	Assert( pWeight[0] == w0 && pWeight[1] == w1 );
	*/

	// clamp the diagonal
	if (pWeight[1] < 0.001f)
		pWeight[1] = 0.0f;
	pWeight[2] = 1.0f - pWeight[0] - pWeight[1];

	Assert( pWeight[0] >= 0.0f && pWeight[0] <= 1.0f );
	Assert( pWeight[1] >= 0.0f && pWeight[1] <= 1.0f );
	Assert( pWeight[2] >= 0.0f && pWeight[2] <= 1.0f );
}


//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//-----------------------------------------------------------------------------
bool CalcPoseSingle(
	const CStudioHdr *pStudioHdr,
	BoneVector pos[], 
	BoneQuaternionAligned q[], 
	mstudioseqdesc_t &seqdesc,
	int sequence, 
	float cycle,
	const float poseParameter[],
	int boneMask,
	float flTime
	)
{
	BONE_PROFILE_FUNC(); // ex: x360: up to 1.3ms
	SNPROF_ANIM("CalcPoseSingle");

	bool bResult = true;
	
	BoneVector *pos2 = g_VectorPool.Alloc();
	BoneQuaternionAligned *q2 = g_QuaternionPool.Alloc();
	BoneVector *pos3 = g_VectorPool.Alloc();
	BoneQuaternionAligned *q3 = g_QuaternionPool.Alloc();


	if ( sequence < 0 || sequence >= pStudioHdr->GetNumSeq()) 
	{
		AssertMsg( false, "Trying to CalcPoseSingle with an out-of-range sequence!\n" );
		return false;

		//sequence = 0;
		//seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( sequence );
	}


	float s0 = 0, s1 = 0;

	int i0 = Studio_LocalPoseParameter( pStudioHdr, poseParameter, seqdesc, sequence, 0, s0 );
	int i1 = Studio_LocalPoseParameter( pStudioHdr, poseParameter, seqdesc, sequence, 1, s1 );


	if (seqdesc.flags & STUDIO_REALTIME)
	{
		float cps = Studio_CPS( pStudioHdr, seqdesc, sequence, poseParameter );
		cycle = flTime * cps;
		cycle = cycle - (int)cycle;
	}
	else if (seqdesc.flags & STUDIO_CYCLEPOSE)
	{
		int iPose = pStudioHdr->GetSharedPoseParameter( sequence, seqdesc.cycleposeindex );
		if (iPose != -1)
		{
			/*
			const mstudioposeparamdesc_t &Pose = pStudioHdr->pPoseParameter( iPose );
			cycle = poseParameter[ iPose ] * (Pose.end - Pose.start) + Pose.start;
			*/
			cycle = poseParameter[ iPose ];
		}
		else
		{
			cycle = 0.0f;
		}
	}
	else if (cycle < 0 || cycle >= 1)
	{
		if (seqdesc.flags & STUDIO_LOOPING)
		{
			cycle = cycle - (int)cycle;
			if (cycle < 0) cycle += 1;
		}
		else
		{
			cycle = clamp( cycle, 0.0f, 1.0f );
		}
	}

	if (s0 < 0.001)
	{
		if (s1 < 0.001)
		{
			if (PoseIsAllZeros( pStudioHdr, sequence, seqdesc, i0, i1 ))
			{
				bResult = false;
			}
			else
			{
				CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0  , i1   ), cycle, boneMask );
			}
		}
		else if (s1 > 0.999)
		{
			CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0  , i1+1 ), cycle, boneMask );
		}
		else
		{
			CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0  , i1   ), cycle, boneMask );
			CalcAnimation( pStudioHdr, pos2, q2, seqdesc, sequence, seqdesc.anim( i0  , i1+1 ), cycle, boneMask );
			BlendBones( pStudioHdr, q, pos, seqdesc, sequence, q2, pos2, s1, boneMask );
		}
	}
	else if (s0 > 0.999)
	{
		if (s1 < 0.001)
		{
			if (PoseIsAllZeros( pStudioHdr, sequence, seqdesc, i0+1, i1 ))
			{
				bResult = false;
			}
			else
			{
				CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0+1, i1   ), cycle, boneMask );
			}
		}
		else if (s1 > 0.999)
		{
			CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0+1, i1+1 ), cycle, boneMask );
		}
		else
		{
			CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0+1, i1   ), cycle, boneMask );
			CalcAnimation( pStudioHdr, pos2, q2, seqdesc, sequence, seqdesc.anim( i0+1, i1+1 ), cycle, boneMask );
			BlendBones( pStudioHdr, q, pos, seqdesc, sequence, q2, pos2, s1, boneMask );
		}
	}
	else
	{
		if (s1 < 0.001)
		{
			if (PoseIsAllZeros( pStudioHdr, sequence, seqdesc, i0+1, i1 ))
			{
				CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0  ,i1  ), cycle, boneMask );
				ScaleBones( pStudioHdr, q, pos, sequence, 1.0 - s0, boneMask );
			}
			else if (PoseIsAllZeros( pStudioHdr, sequence, seqdesc, i0, i1 ))
			{
				CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0+1  ,i1  ), cycle, boneMask );
				ScaleBones( pStudioHdr, q, pos, sequence, s0, boneMask );
			}
			else
			{
				CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0  ,i1  ), cycle, boneMask );
				CalcAnimation( pStudioHdr, pos2, q2, seqdesc, sequence, seqdesc.anim( i0+1,i1  ), cycle, boneMask );

				BlendBones( pStudioHdr, q, pos, seqdesc, sequence, q2, pos2, s0, boneMask );
			}
		}
		else if (s1 > 0.999)
		{
			CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0  ,i1+1  ), cycle, boneMask );
			CalcAnimation( pStudioHdr, pos2, q2, seqdesc, sequence, seqdesc.anim( i0+1,i1+1  ), cycle, boneMask );
			BlendBones( pStudioHdr, q, pos, seqdesc, sequence, q2, pos2, s0, boneMask );
		}
		else if ( !anim_3wayblend.GetBool() )
		{
			CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, seqdesc.anim( i0  ,i1  ), cycle, boneMask );
			CalcAnimation( pStudioHdr, pos2, q2, seqdesc, sequence, seqdesc.anim( i0+1,i1  ), cycle, boneMask );
			BlendBones( pStudioHdr, q, pos, seqdesc, sequence, q2, pos2, s0, boneMask );

			CalcAnimation( pStudioHdr, pos2, q2, seqdesc, sequence, seqdesc.anim( i0  , i1+1), cycle, boneMask );
			CalcAnimation( pStudioHdr, pos3, q3, seqdesc, sequence, seqdesc.anim( i0+1, i1+1), cycle, boneMask );
			BlendBones( pStudioHdr, q2, pos2, seqdesc, sequence, q3, pos3, s0, boneMask );

			BlendBones( pStudioHdr, q, pos, seqdesc, sequence, q2, pos2, s1, boneMask );
		}
		else
		{
			int		iAnimIndices[3];
			float	weight[3];

			Calc3WayBlendIndices( i0, i1, s0, s1, seqdesc, iAnimIndices, weight );

			/*
			char buf[256];
			sprintf( buf, "%d %6.2f  %d %6.2f : %6.2f %6.2f %6.2f\n", i0, s0, i1, s1, weight[0], weight[1], weight[2] );
			OutputDebugString( buf );
			*/

			if (weight[1] < 0.001)
			{
				// on diagonal
				CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, iAnimIndices[0], cycle, boneMask );
				CalcAnimation( pStudioHdr, pos2, q2, seqdesc, sequence, iAnimIndices[2], cycle, boneMask );
				BlendBones( pStudioHdr, q, pos, seqdesc, sequence, q2, pos2, weight[2] / (weight[0] + weight[2]), boneMask );
			}
			else
			{
				CalcAnimation( pStudioHdr, pos,  q,  seqdesc, sequence, iAnimIndices[0], cycle, boneMask );
				CalcAnimation( pStudioHdr, pos2, q2, seqdesc, sequence, iAnimIndices[1], cycle, boneMask );
				BlendBones( pStudioHdr, q, pos, seqdesc, sequence, q2, pos2, weight[1] / (weight[0] + weight[1]), boneMask );

				CalcAnimation( pStudioHdr, pos3, q3, seqdesc, sequence, iAnimIndices[2], cycle, boneMask );
				BlendBones( pStudioHdr, q, pos, seqdesc, sequence, q3, pos3, weight[2], boneMask );
			}
		}
	}

	g_VectorPool.Free( pos2 );
	g_QuaternionPool.Free( q2 );
	g_VectorPool.Free( pos3 );
	g_QuaternionPool.Free( q3 );

	return bResult;
}




//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//			adds autolayers, runs local ik rukes
//-----------------------------------------------------------------------------
void CBoneSetup::AddSequenceLayers(
	BoneVector pos[], 
	BoneQuaternion q[], 
	mstudioseqdesc_t &seqdesc,
	int sequence, 
	float cycle,
	float flWeight,
	float flTime,
	CIKContext *pIKContext
	)
{
	BONE_PROFILE_FUNC(); // ex: x360: 1.84ms
	SNPROF_ANIM("CBoneSetup::AddSequenceLayers");

	for (int i = 0; i < seqdesc.numautolayers; i++)
	{
		mstudioautolayer_t *pLayer = seqdesc.pAutolayer( i );

		if (pLayer->flags & STUDIO_AL_LOCAL)
			continue;

		float layerCycle = cycle;
		float layerWeight = flWeight;

		if (pLayer->start != pLayer->end)
		{
			float s = 1.0;
			float index;

			if (!(pLayer->flags & STUDIO_AL_POSE))
			{
				index = cycle;
			}
			else
			{
				int iSequence = m_pStudioHdr->iRelativeSeq( sequence, pLayer->iSequence );
				int iPose = m_pStudioHdr->GetSharedPoseParameter( iSequence, pLayer->iPose );
				if (iPose != -1)
				{
					const mstudioposeparamdesc_t &Pose = ((CStudioHdr *)m_pStudioHdr)->pPoseParameter( iPose );
					index = m_flPoseParameter[ iPose ] * (Pose.end - Pose.start) + Pose.start;
				}
				else
				{
					index = 0;
				}
			}

			if (index < pLayer->start)
				continue;
			if (index >= pLayer->end)
				continue;

			if (index < pLayer->peak && pLayer->start != pLayer->peak)
			{
				s = (index - pLayer->start) / (pLayer->peak - pLayer->start);
			}
			else if (index > pLayer->tail && pLayer->end != pLayer->tail)
			{
				s = (pLayer->end - index) / (pLayer->end - pLayer->tail);
			}

			if (pLayer->flags & STUDIO_AL_SPLINE)
			{
				s = clamp( SimpleSpline( s ), 0, 1 ); // SimpleSpline imprecision can push some float values outside 0..1
			}

			if ((pLayer->flags & STUDIO_AL_XFADE) && (index > pLayer->tail))
			{
				layerWeight = ( s * flWeight ) / ( 1 - flWeight + s * flWeight );
			}
			else if (pLayer->flags & STUDIO_AL_NOBLEND)
			{
				layerWeight = s;
			}
			else
			{
				layerWeight = flWeight * s;
			}

			if (!(pLayer->flags & STUDIO_AL_POSE))
			{
				layerCycle = (cycle - pLayer->start) / (pLayer->end - pLayer->start);
			}
		}
		else if ( pLayer->start == 0 && pLayer->end == 0 && (pLayer->flags & STUDIO_AL_POSE) )
		{
			int iSequence = m_pStudioHdr->iRelativeSeq( sequence, pLayer->iSequence );
			int iPose = m_pStudioHdr->GetSharedPoseParameter( iSequence, pLayer->iPose );
			if (iPose == -1)
				continue;
			
			const mstudioposeparamdesc_t &Pose = ((CStudioHdr *)m_pStudioHdr)->pPoseParameter( iPose );
			float s = m_flPoseParameter[ iPose ] * (Pose.end - Pose.start) + Pose.start;

			Assert( (pLayer->tail - pLayer->peak) != 0 );

			s = clamp( (s - pLayer->peak) / (pLayer->tail - pLayer->peak), 0, 1 );

			if (pLayer->flags & STUDIO_AL_SPLINE)
			{
				s = clamp( SimpleSpline( s ), 0, 1 ); // SimpleSpline imprecision can push some float values outside 0..1
			}

			layerWeight = flWeight * s;			
		}

		int iSequence = m_pStudioHdr->iRelativeSeq( sequence, pLayer->iSequence );
		AccumulatePose( pos, q, iSequence, layerCycle, layerWeight, flTime, pIKContext );
	}
}


//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//			adds autolayers, runs local ik rukes
//-----------------------------------------------------------------------------
void CBoneSetup::AddLocalLayers(
	BoneVector pos[], 
	BoneQuaternion q[], 
	mstudioseqdesc_t &seqdesc,
	int sequence, 
	float cycle,
	float flWeight,
	float flTime,
	CIKContext *pIKContext
	)
{
	BONE_PROFILE_FUNC();
	SNPROF_ANIM("CBoneSetup::AddLocalLayers");

	if (!(seqdesc.flags & STUDIO_LOCAL))
	{
		return;
	}

	for (int i = 0; i < seqdesc.numautolayers; i++)
	{
		mstudioautolayer_t *pLayer = seqdesc.pAutolayer( i );

		if (!(pLayer->flags & STUDIO_AL_LOCAL))
			continue;

		float layerCycle = cycle;
		float layerWeight = flWeight;

		if (pLayer->start != pLayer->end)
		{
			float s = 1.0;

			if (cycle < pLayer->start)
				continue;
			if (cycle >= pLayer->end)
				continue;

			if (cycle < pLayer->peak && pLayer->start != pLayer->peak)
			{
				s = (cycle - pLayer->start) / (pLayer->peak - pLayer->start);
			}
			else if (cycle > pLayer->tail && pLayer->end != pLayer->tail)
			{
				s = (pLayer->end - cycle) / (pLayer->end - pLayer->tail);
			}

			if (pLayer->flags & STUDIO_AL_SPLINE)
			{
				s = SimpleSpline( s );
			}

			if ((pLayer->flags & STUDIO_AL_XFADE) && (cycle > pLayer->tail))
			{
				layerWeight = ( s * flWeight ) / ( 1 - flWeight + s * flWeight );
			}
			else if (pLayer->flags & STUDIO_AL_NOBLEND)
			{
				layerWeight = s;
			}
			else
			{
				layerWeight = flWeight * s;
			}

			layerCycle = (cycle - pLayer->start) / (pLayer->end - pLayer->start);
		}

		int iSequence = m_pStudioHdr->iRelativeSeq( sequence, pLayer->iSequence );
		AccumulatePose( pos, q, iSequence, layerCycle, layerWeight, flTime, pIKContext );
	}
}

//-----------------------------------------------------------------------------
// Purpose: my sleezy attempt at an interface only class
//-----------------------------------------------------------------------------

IBoneSetup::IBoneSetup( const CStudioHdr *pStudioHdr, int boneMask, const float poseParameter[], IPoseDebugger *pPoseDebugger )
{
	m_pBoneSetup = new CBoneSetup( pStudioHdr, boneMask, poseParameter, pPoseDebugger );
}

IBoneSetup::~IBoneSetup( void )
{
	if ( m_pBoneSetup )
	{
		delete m_pBoneSetup;
	}
}

void IBoneSetup::InitPose( BoneVector pos[], BoneQuaternionAligned q[] )
{
	::InitPose( m_pBoneSetup->m_pStudioHdr, pos, q, m_pBoneSetup->m_boneMask );
}

void IBoneSetup::AccumulatePose( BoneVector pos[], BoneQuaternion q[], int sequence, float cycle, float flWeight, float flTime, CIKContext *pIKContext )
{
	m_pBoneSetup->AccumulatePose( pos, q, sequence, cycle, flWeight, flTime, pIKContext );
}

void IBoneSetup::CalcAutoplaySequences(	BoneVector pos[], BoneQuaternion q[], float flRealTime, CIKContext *pIKContext )
{
	m_pBoneSetup->CalcAutoplaySequences( pos, q, flRealTime, pIKContext );
}

// takes a "controllers[]" array normalized to 0..1 and adds in the adjustments to pos[], and q[].
void IBoneSetup::CalcBoneAdj( BoneVector pos[], BoneQuaternion q[], const float controllers[] )
{
	::CalcBoneAdj( m_pBoneSetup->m_pStudioHdr, pos, q, controllers, m_pBoneSetup->m_boneMask );
}

CStudioHdr *IBoneSetup::GetStudioHdr()
{
	return (CStudioHdr *)m_pBoneSetup->m_pStudioHdr;
}

CBoneSetup::CBoneSetup( const CStudioHdr *pStudioHdr, int boneMask, const float poseParameter[], IPoseDebugger *pPoseDebugger )
{
	m_pStudioHdr = pStudioHdr;
	m_boneMask = boneMask;
	m_flPoseParameter = poseParameter;
	m_pPoseDebugger = pPoseDebugger;
}


#if 0
//-----------------------------------------------------------------------------
// Purpose: calculate a pose for a single sequence
//			adds autolayers, runs local ik rukes
//-----------------------------------------------------------------------------
void CalcPose(
	const CStudioHdr *pStudioHdr,
	CIKContext *pIKContext,
	BoneVector pos[], 
	BoneQuaternionAligned q[], 
	int sequence, 
	float cycle,
	const float poseParameter[],
	int boneMask,
	float flWeight,
	float flTime
	)
{
	BONE_PROFILE_FUNC();
	mstudioseqdesc_t	&seqdesc = pStudioHdr->pSeqdesc( sequence );

	Assert( flWeight >= 0.0f && flWeight <= 1.0f );
	// This shouldn't be necessary, but the Assert should help us catch whoever is screwing this up
	flWeight = clamp( flWeight, 0.0f, 1.0f );

	// add any IK locks to prevent numautolayers from moving extremities 
	CIKContext seq_ik;
	if (seqdesc.numiklocks)
	{
		seq_ik.Init( pStudioHdr, vec3_angle, vec3_origin, 0.0, 0, boneMask ); // local space relative so absolute position doesn't mater
		seq_ik.AddSequenceLocks( seqdesc, pos, q );
	}

	CalcPoseSingle( pStudioHdr, pos, q, seqdesc, sequence, cycle, poseParameter, boneMask, flTime );

	if ( pIKContext )
	{
		pIKContext->AddDependencies( seqdesc, sequence, cycle, poseParameter, flWeight );
	}
	
	AddSequenceLayers( pStudioHdr, pIKContext, pos, q, seqdesc, sequence, cycle, poseParameter, boneMask, flWeight, flTime );

	if (seqdesc.numiklocks)
	{
		seq_ik.SolveSequenceLocks( seqdesc, pos, q );
	}
}
#endif

extern ConVar cl_use_simd_bones;
//-----------------------------------------------------------------------------
// Purpose: accumulate a pose for a single sequence on top of existing animation
//			adds autolayers, runs local ik rukes
//-----------------------------------------------------------------------------
void CBoneSetup::AccumulatePose(
	BoneVector pos[], 
	BoneQuaternion q[], 
	int sequence, 
	float cycle,
	float flWeight,
	float flTime,
	CIKContext *pIKContext
	)
{
	BONE_PROFILE_FUNC(); // ex: x360: up to 3.6ms
#if _DEBUG
	VPROF_INCREMENT_COUNTER("AccumulatePose",1);
#endif

	VPROF( "AccumulatePose" );

	SNPROF_ANIM( "CBoneSetup::AccumulatePose" );

	// Check alignment.
	if ( cl_use_simd_bones.GetBool() && (!  (reinterpret_cast<uintp>(q) & 0x0F) == 0 ) )
	{
		DebuggerBreakIfDebugging();
		AssertMsg(false,
		"Arguments to AccumulatePose are unaligned. Disaster will result.\n"
		);
	}

	Assert( flWeight >= 0.0f && flWeight <= 1.0f );
	// This shouldn't be necessary, but the Assert should help us catch whoever is screwing this up
	flWeight = clamp( flWeight, 0.0f, 1.0f );

	if ( sequence < 0 || sequence >= m_pStudioHdr->GetNumSeq() )
	{
		AssertMsg( false, "Trying to AccumulatePose with an out-of-range sequence!\n" );
		return;
	}

	// This should help re-use the memory for vectors/quaternions
	// 	BoneVector		pos2[MAXSTUDIOBONES];
	// 	BoneQuaternion	q2[MAXSTUDIOBONES];
	BoneVector *pos2 = g_VectorPool.Alloc();
	BoneQuaternionAligned * q2 = ( BoneQuaternionAligned * ) g_QuaternionPool.Alloc();

	PREFETCH360( pos2, 0 );
	PREFETCH360( q2, 0 );

	// Trigger pose debugger
	if (m_pPoseDebugger)
	{
		m_pPoseDebugger->AccumulatePose( m_pStudioHdr, pIKContext, pos, q, sequence, cycle, m_flPoseParameter, m_boneMask, flWeight, flTime );
	}

	mstudioseqdesc_t	&seqdesc = ((CStudioHdr *)m_pStudioHdr)->pSeqdesc( sequence );

	// add any IK locks to prevent extremities from moving
	CIKContext seq_ik;
	if (seqdesc.numiklocks)
	{
		seq_ik.Init( m_pStudioHdr, vec3_angle, vec3_origin, 0.0, 0, m_boneMask );  // local space relative so absolute position doesn't mater
		seq_ik.AddSequenceLocks( seqdesc, pos, q );
	}

	if ((seqdesc.flags & STUDIO_LOCAL) || (seqdesc.flags & STUDIO_ROOTXFORM) || (seqdesc.flags & STUDIO_WORLD_AND_RELATIVE))
	{
		::InitPose( m_pStudioHdr, pos2, q2, m_boneMask );
	}

	if (CalcPoseSingle( m_pStudioHdr, pos2, q2, seqdesc, sequence, cycle, m_flPoseParameter, m_boneMask, flTime ))
	{

		if ( (seqdesc.flags & STUDIO_ROOTXFORM) && seqdesc.rootDriverIndex > 0 )
		{

			// hack: Remap the driver bone if it's coming in from an included virtual model and the indices might not match
			// poseparam input is ignored for now
			int nRemappedDriverBone = seqdesc.rootDriverIndex;
			virtualmodel_t *pVModel = m_pStudioHdr->GetVirtualModel();
			if (pVModel)
			{
				const virtualgroup_t *pAnimGroup;
				const studiohdr_t *pAnimStudioHdr;
				int baseanimation = m_pStudioHdr->iRelativeAnim( sequence, 0 );
				pAnimGroup = pVModel->pAnimGroup( baseanimation );
				pAnimStudioHdr = ((CStudioHdr *)m_pStudioHdr)->pAnimStudioHdr( baseanimation );
				nRemappedDriverBone = pAnimGroup->masterBone[nRemappedDriverBone];
			}

			matrix3x4a_t rootDriverXform;
			AngleMatrix( RadianEuler(q2[nRemappedDriverBone]), pos2[nRemappedDriverBone], rootDriverXform );

			matrix3x4a_t rootToMove;
			AngleMatrix( RadianEuler(q[0]), pos[0], rootToMove );

			matrix3x4a_t rootMoved;
			ConcatTransforms_Aligned( rootDriverXform, rootToMove, rootMoved );

			MatrixAngles( rootMoved, q2[0], pos2[0] );
		}

		// this weight is wrong, the IK rules won't composite at the correct intensity
		AddLocalLayers( pos2, q2, seqdesc, sequence, cycle, 1.0, flTime, pIKContext );
		SlerpBones( m_pStudioHdr, q, pos, seqdesc, sequence, q2, pos2, flWeight, m_boneMask );
	}

	g_VectorPool.Free( pos2 );
	g_QuaternionPool.Free( q2 );

	if ( pIKContext )
	{
		pIKContext->AddDependencies( seqdesc, sequence, cycle, m_flPoseParameter, flWeight );
	}

	AddSequenceLayers( pos, q, seqdesc, sequence, cycle, flWeight, flTime, pIKContext );

	if (seqdesc.numiklocks)
	{
		seq_ik.SolveSequenceLocks( seqdesc, pos, q );
	}
}


//-----------------------------------------------------------------------------
// Purpose: blend together q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------
void CalcBoneAdj(
	const CStudioHdr *pStudioHdr,
	BoneVector pos[], 
	BoneQuaternion q[], 
	const float controllers[],
	int boneMask
	)
{
	BONE_PROFILE_FUNC();
	int					i, j, k;
	float				value;
	mstudiobonecontroller_t *pbonecontroller;
	Vector p0;
	RadianEuler a0;
	Quaternion q0;
	
	for (j = 0; j < pStudioHdr->numbonecontrollers(); j++)
	{
		pbonecontroller = pStudioHdr->pBonecontroller( j );
		k = pbonecontroller->bone;

		if (pStudioHdr->boneFlags( k ) & boneMask)
		{
			i = pbonecontroller->inputfield;
			value = controllers[i];
			if (value < 0) value = 0;
			if (value > 1.0) value = 1.0;
			value = (1.0 - value) * pbonecontroller->start + value * pbonecontroller->end;

			switch(pbonecontroller->type & STUDIO_TYPES)
			{
			case STUDIO_XR: 
				a0.Init( value * (M_PI / 180.0), 0, 0 ); 
				AngleQuaternion( a0, q0 );
				QuaternionSM( 1.0, q0, q[k], q[k] );
				break;
			case STUDIO_YR: 
				a0.Init( 0, value * (M_PI / 180.0), 0 ); 
				AngleQuaternion( a0, q0 );
				QuaternionSM( 1.0, q0, q[k], q[k] );
				break;
			case STUDIO_ZR: 
				a0.Init( 0, 0, value * (M_PI / 180.0) ); 
				AngleQuaternion( a0, q0 );
				QuaternionSM( 1.0, q0, q[k], q[k] );
				break;
			case STUDIO_X:	
				pos[k].x += value;
				break;
			case STUDIO_Y:
				pos[k].y += value;
				break;
			case STUDIO_Z:
				pos[k].z += value;
				break;
			}
		}
	}
}


void CalcBoneDerivatives( Vector &velocity, AngularImpulse &angVel, const matrix3x4_t &prev, const matrix3x4_t &current, float dt )
{
	float scale = 1.0;
	if ( dt > 0 )
	{
		scale = 1.0 / dt;
	}
	
	Vector endPosition, startPosition, deltaAxis;
	QAngle endAngles, startAngles;
	float deltaAngle;

	MatrixAngles( prev, startAngles, startPosition );
	MatrixAngles( current, endAngles, endPosition );

	velocity.x = (endPosition.x - startPosition.x) * scale;
	velocity.y = (endPosition.y - startPosition.y) * scale;
	velocity.z = (endPosition.z - startPosition.z) * scale;
	RotationDeltaAxisAngle( startAngles, endAngles, deltaAxis, deltaAngle );
	VectorScale( deltaAxis, (deltaAngle * scale), angVel );
}

void CalcBoneVelocityFromDerivative( const QAngle &vecAngles, Vector &velocity, AngularImpulse &angVel, const matrix3x4_t &current )
{
	Vector vecLocalVelocity;
	AngularImpulse LocalAngVel;
	Quaternion q;
	float angle;
	MatrixAngles( current, q, vecLocalVelocity );
	QuaternionAxisAngle( q, LocalAngVel, angle );
	LocalAngVel *= angle;

	matrix3x4a_t matAngles;
	AngleMatrix( vecAngles, matAngles );
	VectorTransform( vecLocalVelocity, matAngles, velocity );
	VectorTransform( LocalAngVel, matAngles, angVel );
}





//-----------------------------------------------------------------------------
// Purpose: run all animations that automatically play and are driven off of poseParameters
//-----------------------------------------------------------------------------
void CBoneSetup::CalcAutoplaySequences(
	BoneVector pos[], 
	BoneQuaternion q[], 
	float flRealTime,
	CIKContext *pIKContext
	)
{
	BONE_PROFILE_FUNC();
//	ASSERT_NO_REENTRY();

	SNPROF_ANIM( "CBoneSetup::CalcAutoplaySequences" );
	
	int			i;
	if ( pIKContext )
	{
		pIKContext->AddAutoplayLocks( pos, q );
	}

	unsigned short *pList = NULL;
	int count = m_pStudioHdr->GetAutoplayList( &pList );
	for (i = 0; i < count; i++)
	{
		int sequenceIndex = pList[i];
		mstudioseqdesc_t &seqdesc = ((CStudioHdr *)m_pStudioHdr)->pSeqdesc( sequenceIndex );
		if (seqdesc.flags & STUDIO_AUTOPLAY)
		{
			float cycle = 0;
			float cps = Studio_CPS( m_pStudioHdr, seqdesc, sequenceIndex, m_flPoseParameter );
			cycle = flRealTime * cps;
			cycle = cycle - (int)cycle;

			AccumulatePose( pos, q, sequenceIndex, cycle, 1.0, flRealTime, pIKContext );
		}
	}

	if ( pIKContext )
	{
		pIKContext->SolveAutoplayLocks( pos, q );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void Studio_BuildMatrices(
	const CStudioHdr *pStudioHdr,
	const QAngle& angles, 
	const Vector& origin, 
	const BoneVector pos[],
	const BoneQuaternion q[],
	int iBone,
	float flScale,
	matrix3x4a_t bonetoworld[MAXSTUDIOBONES],
	int boneMask
	)
{
	BONE_PROFILE_FUNC();
	int i, j;

	int					chain[MAXSTUDIOBONES] = {};
	int					chainlength = 0;

	if (iBone < -1 || iBone >= pStudioHdr->numbones())
		iBone = 0;

	// build list of what bones to use
	if (iBone == -1)
	{
		// all bones
		chainlength = pStudioHdr->numbones();
		for (i = 0; i < pStudioHdr->numbones(); i++)
		{
			chain[chainlength - i - 1] = i;
		}
	}
	else
	{
		// only the parent bones
		i = iBone;
		while (i != -1)
		{
			chain[chainlength++] = i;
			i = pStudioHdr->boneParent( i );
		}
	}

	matrix3x4a_t bonematrix;
	matrix3x4a_t rotationmatrix; // model to world transformation
	AngleMatrix( angles, origin, rotationmatrix );

	// Account for a change in scale
	if ( flScale < 1.0f-FLT_EPSILON || flScale > 1.0f+FLT_EPSILON )
	{
		Vector vecOffset;
		MatrixGetColumn( rotationmatrix, 3, vecOffset );
		vecOffset -= origin;
		vecOffset *= flScale;
		vecOffset += origin;
		MatrixSetColumn( vecOffset, 3, rotationmatrix );

		// Scale it uniformly
		VectorScale( rotationmatrix[0], flScale, rotationmatrix[0] );
		VectorScale( rotationmatrix[1], flScale, rotationmatrix[1] );
		VectorScale( rotationmatrix[2], flScale, rotationmatrix[2] );
	}

	// check for 16 byte alignment
	if ((((size_t)bonetoworld) % 16) != 0)
	{
		for (j = chainlength - 1; j >= 0; j--)
		{
			i = chain[j];
			if (pStudioHdr->boneFlags(i) & boneMask)
			{
				QuaternionMatrix( q[i], pos[i], bonematrix );

				if (pStudioHdr->boneParent(i) == -1) 
				{
					ConcatTransforms (rotationmatrix, bonematrix, bonetoworld[i]);
				} 
				else 
				{
					ConcatTransforms (bonetoworld[pStudioHdr->boneParent(i)], bonematrix, bonetoworld[i]);
				}
			}
		}
	}
	else
	{
		for (j = chainlength - 1; j >= 0; j--)
		{
			i = chain[j];
			if (pStudioHdr->boneFlags(i) & boneMask)
			{
				QuaternionMatrix( q[i], pos[i], bonematrix );

				if (pStudioHdr->boneParent(i) == -1) 
				{
					ConcatTransforms_Aligned (rotationmatrix, bonematrix, bonetoworld[i]);
				} 
				else 
				{
					ConcatTransforms_Aligned (bonetoworld[pStudioHdr->boneParent(i)], bonematrix, bonetoworld[i]);
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: look at single column vector of another bones local transformation 
//			and generate a procedural transformation based on how that column 
//			points down the 6 cardinal axis (all negative weights are clamped to 0).
//-----------------------------------------------------------------------------

void DoAxisInterpBone(
	const mstudiobone_t		*pbones,
	int	ibone,
	CBoneAccessor &bonetoworld
	)
{
	BONE_PROFILE_FUNC();
	matrix3x4a_t	bonematrix;
	Vector				control;

	mstudioaxisinterpbone_t *pProc = (mstudioaxisinterpbone_t *)pbones[ibone].pProcedure( );
	const matrix3x4_t &controlBone = bonetoworld.GetBone( pProc->control );
	if (pProc && pbones[pProc->control].parent != -1)
	{
		Vector tmp;
		// pull out the control column
		tmp.x = controlBone[0][pProc->axis];
		tmp.y = controlBone[1][pProc->axis];
		tmp.z = controlBone[2][pProc->axis];

		// invert it back into parent's space.
		VectorIRotate( tmp, bonetoworld.GetBone( pbones[pProc->control].parent ), control );
#if 0
		matrix3x4a_t	tmpmatrix;
		matrix3x4a_t	controlmatrix;
		MatrixInvert( bonetoworld.GetBone( pbones[pProc->control].parent ), tmpmatrix );
		ConcatTransforms_Aligned( tmpmatrix, bonetoworld.GetBone( pProc->control ), controlmatrix );

		// pull out the control column
		control.x = controlmatrix[0][pProc->axis];
		control.y = controlmatrix[1][pProc->axis];
		control.z = controlmatrix[2][pProc->axis];
#endif
	}
	else
	{
		// pull out the control column
		control.x = controlBone[0][pProc->axis];
		control.y = controlBone[1][pProc->axis];
		control.z = controlBone[2][pProc->axis];
	}

	Quaternion *q1, *q2, *q3;
	Vector *p1, *p2, *p3;

	// find axial control inputs
	float a1 = control.x;
	float a2 = control.y;
	float a3 = control.z;
	if (a1 >= 0) 
	{ 
		q1 = &pProc->quat[0];
		p1 = &pProc->pos[0];
	} 
	else 
	{ 
		a1 = -a1; 
		q1 = &pProc->quat[1];
		p1 = &pProc->pos[1];
	}

	if (a2 >= 0) 
	{ 
		q2 = &pProc->quat[2]; 
		p2 = &pProc->pos[2];
	} 
	else 
	{ 
		a2 = -a2; 
		q2 = &pProc->quat[3]; 
		p2 = &pProc->pos[3];
	}

	if (a3 >= 0) 
	{ 
		q3 = &pProc->quat[4]; 
		p3 = &pProc->pos[4];
	} 
	else 
	{ 
		a3 = -a3; 
		q3 = &pProc->quat[5]; 
		p3 = &pProc->pos[5];
	}

	// do a three-way blend
	Vector p;
	Quaternion v, tmp;
	if (a1 + a2 > 0)
	{
		float t = 1.0 / (a1 + a2 + a3);
		// FIXME: do a proper 3-way Quat blend!
		QuaternionSlerp( *q2, *q1, a1 / (a1 + a2), tmp );
		QuaternionSlerp( tmp, *q3, a3 * t, v );
		VectorScale( *p1, a1 * t, p );
		VectorMA( p, a2 * t, *p2, p );
		VectorMA( p, a3 * t, *p3, p );
	}
	else
	{
		QuaternionSlerp( *q3, *q3, 0, v ); // ??? no quat copy?
		p = *p3;
	}

	QuaternionMatrix( v, p, bonematrix );

	ConcatTransforms (bonetoworld.GetBone( pbones[ibone].parent ), bonematrix, bonetoworld.GetBoneForWrite( ibone ));
}



//-----------------------------------------------------------------------------
// Purpose: Generate a procedural transformation based on how that another bones 
//			local transformation matches a set of target orientations.
//-----------------------------------------------------------------------------
void DoQuatInterpBone(
	const mstudiobone_t		*pbones,
	int	ibone,
	CBoneAccessor &bonetoworld
	)
{
	BONE_PROFILE_FUNC();
	matrix3x4a_t	bonematrix;
	Vector				control;

	mstudioquatinterpbone_t *pProc = (mstudioquatinterpbone_t *)pbones[ibone].pProcedure( );
	if (pProc && pbones[pProc->control].parent != -1)
	{
		Quaternion	src;
		float		weight[32];
		float		scale = 0.0;
		Quaternion	quat;
		Vector		pos;

		matrix3x4a_t	tmpmatrix;
		matrix3x4a_t	controlmatrix;
		MatrixInvert( bonetoworld.GetBone( pbones[pProc->control].parent), tmpmatrix );
		ConcatTransforms_Aligned( tmpmatrix, bonetoworld.GetBone( pProc->control ), controlmatrix );

		MatrixAngles( controlmatrix, src, pos ); // FIXME: make a version without pos

		int i;
		for (i = 0; i < pProc->numtriggers; i++)
		{
			float dot = fabs( QuaternionDotProduct( pProc->pTrigger( i )->trigger, src ) );
			// FIXME: a fast acos should be acceptable
			dot = clamp( dot, -1, 1 );
			weight[i] = 1 - (2 * acos( dot ) * pProc->pTrigger( i )->inv_tolerance );
			weight[i] = MAX( 0, weight[i] );
			scale += weight[i];
		}

		if (scale <= 0.001)  // EPSILON?
		{
			AngleMatrix( RadianEuler(pProc->pTrigger( 0 )->quat), pProc->pTrigger( 0 )->pos, bonematrix );
			ConcatTransforms ( bonetoworld.GetBone( pbones[ibone].parent ), bonematrix, bonetoworld.GetBoneForWrite( ibone ) );
			return;
		}

		scale = 1.0 / scale;

		quat.Init( 0, 0, 0, 0);
		pos.Init( );

		for (i = 0; i < pProc->numtriggers; i++)
		{
			if (weight[i])
			{
				float s = weight[i] * scale;
				mstudioquatinterpinfo_t *pTrigger = pProc->pTrigger( i );

				QuaternionAlign( pTrigger->quat, quat, quat );

				quat.x = quat.x + s * pTrigger->quat.x;
				quat.y = quat.y + s * pTrigger->quat.y;
				quat.z = quat.z + s * pTrigger->quat.z;
				quat.w = quat.w + s * pTrigger->quat.w;
				pos.x = pos.x + s * pTrigger->pos.x;
				pos.y = pos.y + s * pTrigger->pos.y;
				pos.z = pos.z + s * pTrigger->pos.z;
			}
		}
		Assert( QuaternionNormalize( quat ) != 0);
		QuaternionMatrix( quat, pos, bonematrix );
	}

	ConcatTransforms_Aligned( bonetoworld.GetBone( pbones[ibone].parent ), bonematrix, bonetoworld.GetBoneForWrite( ibone ) );
}

/*
 * This is for DoAimAtBone below, was just for testing, not needed in general
 * but to turn it back on, uncomment this and the section in DoAimAtBone() below
 *

static ConVar aim_constraint( "aim_constraint", "1", FCVAR_REPLICATED, "Toggle <aimconstraint> Helper Bones" );

*/

//-----------------------------------------------------------------------------
// Purpose: Generate a procedural transformation so that one bone points at
//			another point on the model
//-----------------------------------------------------------------------------
void DoAimAtBone(
	const mstudiobone_t *pBones,
	int	iBone,
	CBoneAccessor &bonetoworld,
	const CStudioHdr *pStudioHdr
	)
{
	BONE_PROFILE_FUNC();
	mstudioaimatbone_t *pProc = (mstudioaimatbone_t *)pBones[iBone].pProcedure();

	if ( !pProc )
	{
		return;
	}

	/*
	 * Uncomment this if the ConVar above is uncommented
	 *

	if ( !aim_constraint.GetBool() )
	{
		// If the aim constraint is turned off then just copy the parent transform
		// plus the offset value

		matrix3x4a_t boneToWorldSpace;
		MatrixCopy ( bonetoworld.GetBone( pProc->parent ), boneToWorldSpace );
		Vector boneWorldPosition;
		VectorTransform( pProc->basepos, boneToWorldSpace, boneWorldPosition );
		MatrixSetColumn( boneWorldPosition, 3, boneToWorldSpace );
		MatrixCopy( boneToWorldSpace, bonetoworld.GetBoneForWrite( iBone ) );

		return;
	}

	*/

	// The world matrix of the bone to change
	matrix3x4a_t boneMatrix;

	// Guaranteed to be unit length
	const Vector &userAimVector( pProc->aimvector );

	// Guaranteed to be unit length
	const Vector &userUpVector( pProc->upvector );

	// Get to get position of bone but also for up reference
	matrix3x4a_t parentSpace;
	MatrixCopy ( bonetoworld.GetBone( pProc->parent ), parentSpace );

	// World space position of the bone to aim
	Vector aimWorldPosition;
	VectorTransform( pProc->basepos, parentSpace, aimWorldPosition );

	// The worldspace matrix of the bone to aim at
	matrix3x4a_t aimAtSpace;
	if ( pStudioHdr )
	{
		// This means it's AIMATATTACH
		const mstudioattachment_t &attachment( ((CStudioHdr *)pStudioHdr)->pAttachment( pProc->aim ) );
		ConcatTransforms(
			bonetoworld.GetBone( attachment.localbone ),
			attachment.local,
			aimAtSpace );
	}
	else
	{
		MatrixCopy( bonetoworld.GetBone( pProc->aim ), aimAtSpace );
	}

	Vector aimAtWorldPosition;
	MatrixGetColumn( aimAtSpace, 3, aimAtWorldPosition );

	// make sure the redundant parent info is correct
	Assert( pProc->parent == pBones[iBone].parent );
	// make sure the redundant position info is correct
	Assert( pProc->basepos.DistToSqr( pBones[iBone].pos ) < 0.1 );

	// The aim and up data is relative to this bone, not the parent bone
	matrix3x4a_t bonematrix;
	matrix3x4a_t boneLocalToWorld;
	AngleMatrix( RadianEuler(pBones[iBone].quat), pProc->basepos, bonematrix );
	ConcatTransforms_Aligned( bonetoworld.GetBone( pProc->parent ), bonematrix, boneLocalToWorld );

	Vector aimVector;
	VectorSubtract( aimAtWorldPosition, aimWorldPosition, aimVector );
	VectorNormalizeFast( aimVector );

	Vector axis;
	CrossProduct( userAimVector, aimVector, axis );
	VectorNormalizeFast( axis );
	Assert( 1.0f - fabs( DotProduct( userAimVector, aimVector ) ) > FLT_EPSILON );
	float angle( acosf( DotProduct( userAimVector, aimVector ) ) );
	Quaternion aimRotation;
	AxisAngleQuaternion( axis, RAD2DEG( angle ), aimRotation );

	if ( ( 1.0f - fabs( DotProduct( userUpVector, userAimVector ) ) ) > FLT_EPSILON )
	{
		matrix3x4a_t aimRotationMatrix;
		QuaternionMatrix( aimRotation, aimRotationMatrix );

		Vector tmpV;

		Vector tmp_pUp;
		VectorRotate( userUpVector, aimRotationMatrix, tmp_pUp );
		VectorScale( aimVector, DotProduct( aimVector, tmp_pUp ), tmpV );
		Vector pUp;
		VectorSubtract( tmp_pUp, tmpV, pUp );
		VectorNormalizeFast( pUp );

		Vector tmp_pParentUp;
		VectorRotate( userUpVector, boneLocalToWorld, tmp_pParentUp );
		VectorScale( aimVector, DotProduct( aimVector, tmp_pParentUp ), tmpV );
		Vector pParentUp;
		VectorSubtract( tmp_pParentUp, tmpV, pParentUp );
		VectorNormalizeFast( pParentUp );

		Quaternion upRotation;
		//Assert( 1.0f - fabs( DotProduct( pUp, pParentUp ) ) > FLT_EPSILON );
		if( 1.0f - fabs( DotProduct( pUp, pParentUp ) ) > FLT_EPSILON )
		{
			angle = acos( DotProduct( pUp, pParentUp ) );
			CrossProduct( pUp, pParentUp, axis );			
		}
		else
		{
			angle = 0;
			axis = pUp;
		}

		VectorNormalizeFast( axis );
		AxisAngleQuaternion( axis, RAD2DEG( angle ), upRotation );

		Quaternion boneRotation;
		QuaternionMult( upRotation, aimRotation, boneRotation );
		QuaternionMatrix( boneRotation, aimWorldPosition, boneMatrix );
	}
	else
	{
		QuaternionMatrix( aimRotation, aimWorldPosition, boneMatrix );
	}

	MatrixCopy( boneMatrix, bonetoworld.GetBoneForWrite( iBone ) );
}


//-----------------------------------------------------------------------------
// Purpose: Run the twist bone constraint code
//-----------------------------------------------------------------------------
static ConVar anim_twistbones_enabled( "anim_twistbones_enabled", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Enable procedural twist bones." );
void DoTwistBones(
	const mstudiobone_t *pBones,
	int	iBone,
	CBoneAccessor &bonetoworld,
	const CStudioHdr *pStudioHdr )
{
	BONE_PROFILE_FUNC();

	mstudiotwistbone_t *pProc = ( mstudiotwistbone_t * )pBones[iBone].pProcedure();

	if ( !pProc )
		return;

	matrix3x4a_t mTmp;

	// Compute local space version of parent bone matrix
	const matrix3x4a_t &mParentToWorld = bonetoworld.GetBone( pProc->m_nParentBone );
	QuaternionAligned qParent;
	const int nGrandParentBone = pBones[pProc->m_nParentBone].parent;
	if ( nGrandParentBone >= 0 )
	{
		MatrixInvert( bonetoworld.GetBone( pBones[pProc->m_nParentBone].parent), mTmp );
		matrix3x4a_t mParent;
		ConcatTransforms_Aligned( mTmp, mParentToWorld, mParent );
		MatrixQuaternion( mParent, qParent );
	}
	else
	{
		MatrixQuaternion( mParentToWorld, qParent );
	}

	// Compute local space version of child bone matrix
	matrix3x4a_t mChild;
	MatrixInvert( mParentToWorld, mTmp );
	ConcatTransforms_Aligned( mTmp, bonetoworld.GetBone( pProc->m_nChildBone ), mChild );

	float *pflWeights = ( float * )stackalloc( pProc->m_nTargetCount * sizeof( float ) );
	Quaternion *pqTwistBases = ( Quaternion * )stackalloc( pProc->m_nTargetCount * sizeof( Quaternion ) );
	Quaternion *pqTwists = ( Quaternion * )stackalloc( pProc->m_nTargetCount * sizeof( Quaternion ) );

	for ( int i = 0; i < pProc->m_nTargetCount; ++i )
	{
		const mstudiotwistbonetarget_t *pTwistTarget = pProc->pTarget( i );
		pflWeights[i] = pTwistTarget->m_flWeight;
		pqTwistBases[i] = pTwistTarget->m_qBaseRotation;
	}

	V_memcpy( pqTwists, pqTwistBases, pProc->m_nTargetCount * sizeof( Quaternion ) );

	if ( anim_twistbones_enabled.GetBool() )
		ComputeTwistBones( pqTwists, pProc->m_nTargetCount, pProc->m_bInverse, pProc->m_vUpVector, qParent, mChild, pProc->m_qBaseInv, pflWeights, pqTwistBases );

	for ( int i = 0; i < pProc->m_nTargetCount; ++i )
	{
		const mstudiotwistbonetarget_t *pTwistTarget = pProc->pTarget( i );
		AngleMatrix( RadianEuler(pqTwists[i]), pTwistTarget->m_vBaseTranslate, mTmp );
		ConcatTransforms_Aligned( mParentToWorld, mTmp, bonetoworld.GetBoneForWrite( pTwistTarget->m_nBone ) );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

bool CalcProceduralBone(
	const CStudioHdr *pStudioHdr,
	int iBone,
	CBoneAccessor &bonetoworld
	)
{
	const mstudiobone_t		*pbones = pStudioHdr->pBone( 0 );

	if ( pStudioHdr->boneFlags( iBone ) & BONE_ALWAYS_PROCEDURAL )
	{
		switch( pbones[iBone].proctype )
		{
		case STUDIO_PROC_AXISINTERP:
			DoAxisInterpBone( pbones, iBone, bonetoworld );
			return true;

		case STUDIO_PROC_QUATINTERP:
			DoQuatInterpBone( pbones, iBone, bonetoworld );
			return true;

		case STUDIO_PROC_AIMATBONE:
			DoAimAtBone( pbones, iBone, bonetoworld, NULL );
			return true;

		case STUDIO_PROC_AIMATATTACH:
			DoAimAtBone( pbones, iBone, bonetoworld, pStudioHdr );
			return true;

		case STUDIO_PROC_TWIST_MASTER:
			DoTwistBones( pbones, iBone, bonetoworld, pStudioHdr );
			return true;

		case STUDIO_PROC_TWIST_SLAVE:
			// Twist bones are grouped because many twist boens tend to share
			// a large amount of common computation
			// There is one TWIST_MASTER per group and any number of TWIST_SLAVE
			// TWIST_SLAVE data is computed when their corresponding TWIST_MASTER
			// is computed, so they don't need any explicit computation
			return true;

		default:
			return false;
		}
	}
	return false;
}
