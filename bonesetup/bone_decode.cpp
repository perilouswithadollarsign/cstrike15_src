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

#include "tier0/miniprofiler.h"

#ifdef CLIENT_DLL
	#include "posedebugger.h"
#endif

#include "bone_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: return a sub frame rotation for a single bone
//-----------------------------------------------------------------------------
void ExtractAnimValue( int frame, mstudioanimvalue_t *panimvalue, float scale, float &v1, float &v2 )
{
	BONE_PROFILE_FUNC();
	if ( !panimvalue )
	{
		v1 = v2 = 0;
		return;
	}

	// Avoids a crash reading off the end of the data
	// There is probably a better long-term solution; Ken is going to look into it.
	if ( ( panimvalue->num.total == 1 ) && ( panimvalue->num.valid == 1 ) )
	{
		v1 = v2 = panimvalue[1].value * scale;
		return;
	}

	int k = frame;

	// find the data list that has the frame
	while (panimvalue->num.total <= k)
	{
		k -= panimvalue->num.total;
		panimvalue += panimvalue->num.valid + 1;
		if ( panimvalue->num.total == 0 )
		{
			Assert( 0 ); // running off the end of the animation stream is bad
			v1 = v2 = 0;
			return;
		}
	}
	if (panimvalue->num.valid > k)
	{
		// has valid animation data
		v1 = panimvalue[k+1].value * scale;

		if (panimvalue->num.valid > k + 1)
		{
			// has valid animation blend data
			v2 = panimvalue[k+2].value * scale;
		}
		else
		{
			if (panimvalue->num.total > k + 1)
			{
				// data repeats, no blend
				v2 = v1;
			}
			else
			{
				// pull blend from first data block in next list
				v2 = panimvalue[panimvalue->num.valid+2].value * scale;
			}
		}
	}
	else
	{
		// get last valid data block
		v1 = panimvalue[panimvalue->num.valid].value * scale;
		if (panimvalue->num.total > k + 1)
		{
			// data repeats, no blend
			v2 = v1;
		}
		else
		{
			// pull blend from first data block in next list
			v2 = panimvalue[panimvalue->num.valid + 2].value * scale;
		}
	}
}


void ExtractAnimValue( int frame, mstudioanimvalue_t *panimvalue, float scale, float &v1 )
{
	BONE_PROFILE_FUNC();
	if ( !panimvalue )
	{
		v1 = 0;
		return;
	}

	int k = frame;

	while (panimvalue->num.total <= k)
	{
		k -= panimvalue->num.total;
		panimvalue += panimvalue->num.valid + 1;
		if ( panimvalue->num.total == 0 )
		{
			Assert( 0 ); // running off the end of the animation stream is bad
			v1 = 0;
			return;
		}
	}
	if (panimvalue->num.valid > k)
	{
		v1 = panimvalue[k+1].value * scale;
	}
	else
	{
		// get last valid data block
		v1 = panimvalue[panimvalue->num.valid].value * scale;
	}
}

//-----------------------------------------------------------------------------
// Purpose: return a sub frame rotation for a single bone
//-----------------------------------------------------------------------------
void CalcBoneQuaternion( int frame, float s, 
						const Quaternion &baseQuat, const RadianEuler &baseRot, const Vector &baseRotScale, 
						int iBaseFlags, const Quaternion &baseAlignment, 
						const mstudio_rle_anim_t *panim, Quaternion &q )
{
	BONE_PROFILE_FUNC();
	if ( panim->flags & STUDIO_ANIM_RAWROT )
	{
		q = *(panim->pQuat48());
		Assert( q.IsValid() );
		return;
	} 
	
	if ( panim->flags & STUDIO_ANIM_RAWROT2 )
	{
		q = *(panim->pQuat64());
		Assert( q.IsValid() );
		return;
	}

	if ( !(panim->flags & STUDIO_ANIM_ANIMROT) )
	{
		if (panim->flags & STUDIO_ANIM_DELTA)
		{
			q.Init( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		else
		{
			q = baseQuat;
		}
		return;
	}

	mstudioanim_valueptr_t *pValuesPtr = panim->pRotV();

	if (s > 0.001f)
	{
		QuaternionAligned	q1, q2;
		RadianEuler			angle1, angle2;

		ExtractAnimValue( frame, pValuesPtr->pAnimvalue( 0 ), baseRotScale.x, angle1.x, angle2.x );
		ExtractAnimValue( frame, pValuesPtr->pAnimvalue( 1 ), baseRotScale.y, angle1.y, angle2.y );
		ExtractAnimValue( frame, pValuesPtr->pAnimvalue( 2 ), baseRotScale.z, angle1.z, angle2.z );

		if (!(panim->flags & STUDIO_ANIM_DELTA))
		{
			angle1.x = angle1.x + baseRot.x;
			angle1.y = angle1.y + baseRot.y;
			angle1.z = angle1.z + baseRot.z;
			angle2.x = angle2.x + baseRot.x;
			angle2.y = angle2.y + baseRot.y;
			angle2.z = angle2.z + baseRot.z;
		}

		Assert( angle1.IsValid() && angle2.IsValid() );
		if (angle1.x != angle2.x || angle1.y != angle2.y || angle1.z != angle2.z)
		{
			AngleQuaternion( angle1, q1 );
			AngleQuaternion( angle2, q2 );

	#ifdef _X360
			fltx4 q1simd, q2simd, qsimd;
			q1simd = LoadAlignedSIMD( q1 );
			q2simd = LoadAlignedSIMD( q2 );
			qsimd = QuaternionBlendSIMD( q1simd, q2simd, s );
			StoreUnalignedSIMD( q.Base(), qsimd );
	#else
			QuaternionBlend( q1, q2, s, q );
	#endif
		}
		else
		{
			AngleQuaternion( angle1, q );
		}
	}
	else
	{
		RadianEuler			angle;

		ExtractAnimValue( frame, pValuesPtr->pAnimvalue( 0 ), baseRotScale.x, angle.x );
		ExtractAnimValue( frame, pValuesPtr->pAnimvalue( 1 ), baseRotScale.y, angle.y );
		ExtractAnimValue( frame, pValuesPtr->pAnimvalue( 2 ), baseRotScale.z, angle.z );

		if (!(panim->flags & STUDIO_ANIM_DELTA))
		{
			angle.x = angle.x + baseRot.x;
			angle.y = angle.y + baseRot.y;
			angle.z = angle.z + baseRot.z;
		}

		Assert( angle.IsValid() );
		AngleQuaternion( angle, q );
	}

	Assert( q.IsValid() );

	// align to unified bone
	if (!(panim->flags & STUDIO_ANIM_DELTA) && (iBaseFlags & BONE_FIXED_ALIGNMENT))
	{
		QuaternionAlign( baseAlignment, q, q );
	}
}

inline void CalcBoneQuaternion( int frame, float s, 
						const mstudiobone_t *pBone,
						const mstudiolinearbone_t *pLinearBones,
						const mstudio_rle_anim_t *panim, Quaternion &q )
{
	if (pLinearBones)
	{
		CalcBoneQuaternion( frame, s, pLinearBones->quat(panim->bone), pLinearBones->rot(panim->bone), pLinearBones->rotscale(panim->bone), pLinearBones->flags(panim->bone), pLinearBones->qalignment(panim->bone), panim, q );
	}
	else
	{
		CalcBoneQuaternion( frame, s, pBone->quat, pBone->rot, pBone->rotscale, pBone->flags, pBone->qAlignment, panim, q );
	}
}



//-----------------------------------------------------------------------------
// Purpose: return a sub frame position for a single bone
//-----------------------------------------------------------------------------
void CalcBonePosition(	int frame, float s,
						const Vector &basePos, const Vector &baseBoneScale, 
						const mstudio_rle_anim_t *panim, BoneVector &pos	)
{
	BONE_PROFILE_FUNC();
	if (panim->flags & STUDIO_ANIM_RAWPOS)
	{
		pos = *(panim->pPos());
		Assert( pos.IsValid() );

		return;
	}
	else if (!(panim->flags & STUDIO_ANIM_ANIMPOS))
	{
		if (panim->flags & STUDIO_ANIM_DELTA)
		{
			pos.Init( 0.0f, 0.0f, 0.0f );
		}
		else
		{
			pos = basePos;
		}
		return;
	}

	mstudioanim_valueptr_t *pPosV = panim->pPosV();
	int					j;

	if (s > 0.001f)
	{
		float v1, v2;
		for (j = 0; j < 3; j++)
		{
			ExtractAnimValue( frame, pPosV->pAnimvalue( j ), baseBoneScale[j], v1, v2 );
			pos[j] = v1 * (1.0 - s) + v2 * s;
		}
	}
	else
	{
		for (j = 0; j < 3; j++)
		{
			ExtractAnimValue( frame, pPosV->pAnimvalue( j ), baseBoneScale[j], pos[j] );
		}
	}

	if (!(panim->flags & STUDIO_ANIM_DELTA))
	{
		pos.x = pos.x + basePos.x;
		pos.y = pos.y + basePos.y;
		pos.z = pos.z + basePos.z;
	}

	Assert( pos.IsValid() );
}


inline void CalcBonePosition( int frame, float s, 
						const mstudiobone_t *pBone,
						const mstudiolinearbone_t *pLinearBones,
						const mstudio_rle_anim_t *panim, BoneVector &pos )
{
	if (pLinearBones)
	{
		CalcBonePosition( frame, s, pLinearBones->pos(panim->bone), pLinearBones->posscale(panim->bone), panim, pos );
	}
	else
	{
		CalcBonePosition( frame, s, pBone->pos, pBone->posscale, panim, pos );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CalcDecompressedAnimation( const mstudiocompressedikerror_t *pCompressed, int iFrame, float fraq, BoneVector &pos, BoneQuaternion &q )
{
	BONE_PROFILE_FUNC();
	if (fraq > 0.0001f)
	{
		Vector p1, p2;
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 0 ), pCompressed->scale[0], p1.x, p2.x );
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 1 ), pCompressed->scale[1], p1.y, p2.y );
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 2 ), pCompressed->scale[2], p1.z, p2.z );
		pos = p1 * (1 - fraq) + p2 * fraq;

		Quaternion			q1, q2;
		RadianEuler			angle1, angle2;
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 3 ), pCompressed->scale[3], angle1.x, angle2.x );
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 4 ), pCompressed->scale[4], angle1.y, angle2.y );
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 5 ), pCompressed->scale[5], angle1.z, angle2.z );

		if (angle1.x != angle2.x || angle1.y != angle2.y || angle1.z != angle2.z)
		{
			AngleQuaternion( angle1, q1 );
			AngleQuaternion( angle2, q2 );
			QuaternionBlend( q1, q2, fraq, q );
		}
		else
		{
			AngleQuaternion( angle1, q );
		}
	}
	else
	{
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 0 ), pCompressed->scale[0], pos.x );
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 1 ), pCompressed->scale[1], pos.y );
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 2 ), pCompressed->scale[2], pos.z );

		RadianEuler			angle;
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 3 ), pCompressed->scale[3], angle.x );
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 4 ), pCompressed->scale[4], angle.y );
		ExtractAnimValue( iFrame, pCompressed->pAnimvalue( 5 ), pCompressed->scale[5], angle.z );

		AngleQuaternion( angle, q );
	}
}


//-----------------------------------------------------------------------------
// Purpose: translate animations done in a non-standard parent space
//-----------------------------------------------------------------------------
static void CalcLocalHierarchyAnimation( 
	const CStudioHdr *pStudioHdr,
	matrix3x4a_t *boneToWorld,
	CBoneBitList &boneComputed,
	BoneVector *pos, 
	BoneQuaternion *q,
	//const mstudioanimdesc_t &animdesc,
	const mstudiobone_t *pbone,
	mstudiolocalhierarchy_t *pHierarchy,
	int iBone,
	int iNewParent,
	float cycle, 
	int iFrame,
	float flFraq,
	int boneMask
	)
{
	BONE_PROFILE_FUNC();
	BoneVector localPos;
	BoneQuaternion localQ;

	// make fake root transform
	static matrix3x4a_t rootXform;
	SetIdentityMatrix( rootXform );

	// FIXME: missing check to see if seq has a weight for this bone
	float weight = 1.0f;

	// check to see if there's a ramp on the influence
	if ( pHierarchy->tail - pHierarchy->peak < 1.0f  )
	{
		float index = cycle;

		if (pHierarchy->end > 1.0f && index < pHierarchy->start)
			index += 1.0f;

		if (index < pHierarchy->start)
			return;
		if (index >= pHierarchy->end)
			return;

		if (index < pHierarchy->peak && pHierarchy->start != pHierarchy->peak)
		{
			weight = (index - pHierarchy->start) / (pHierarchy->peak - pHierarchy->start);
		}
		else if (index > pHierarchy->tail && pHierarchy->end != pHierarchy->tail)
		{
			weight = (pHierarchy->end - index) / (pHierarchy->end - pHierarchy->tail);
		}

		weight = SimpleSpline( weight );
	}

	CalcDecompressedAnimation( pHierarchy->pLocalAnim(), iFrame - pHierarchy->iStart, flFraq, localPos, localQ );

	// find first common root bone
	int iRoot1 = iBone;
	int iRoot2 = iNewParent;
	while (iRoot1 != iRoot2 && iRoot1 != -1)
	{
		if (iRoot1 > iRoot2)
			iRoot1 = pStudioHdr->boneParent( iRoot1 );
		else
			iRoot2 = pStudioHdr->boneParent( iRoot2 );
	}

	// BUGBUG: pos and q only valid if local weight
	BuildBoneChainPartial( pStudioHdr, rootXform, pos, q, iBone, boneToWorld, boneComputed, iRoot1 );
	BuildBoneChainPartial( pStudioHdr, rootXform, pos, q, iNewParent, boneToWorld, boneComputed, iRoot1 );

	matrix3x4a_t localXform;
	AngleMatrix( RadianEuler(localQ), localPos, localXform );

	ConcatTransforms_Aligned( boneToWorld[iNewParent], localXform, boneToWorld[iBone] );

	// back solve
	BoneVector p1;
	BoneQuaternion q1;
	int n = pbone[iBone].parent;
	if (n == -1)
	{
		if (weight == 1.0f)
		{
			MatrixAngles( boneToWorld[iBone], q[iBone], pos[iBone] );
		}
		else
		{
			MatrixAngles( boneToWorld[iBone], q1, p1 );
			QuaternionSlerp( q[iBone], q1, weight, q[iBone] );
			//pos[iBone] = Lerp( weight, p1, pos[iBone] );
			pos[iBone] = p1 + (pos[iBone] - p1) * weight;
		}
	}
	else
	{
		matrix3x4a_t worldToBone;
		MatrixInvert( boneToWorld[n], worldToBone );

		matrix3x4a_t local;
		ConcatTransforms_Aligned( worldToBone, boneToWorld[iBone], local );
		if (weight == 1.0f)
		{
			MatrixAngles( local, q[iBone], pos[iBone] );
		}
		else
		{
			MatrixAngles( local, q1, p1 );
			QuaternionSlerp( q[iBone], q1, weight, q[iBone] );
			//pos[iBone] = Lerp( weight, p1, pos[iBone] );
			pos[iBone] = p1 + (pos[iBone] - p1) * weight;
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: Calc Zeroframe Data
//-----------------------------------------------------------------------------

static void CalcZeroframeData( const CStudioHdr *pStudioHdr, const studiohdr_t *pAnimStudioHdr, const virtualgroup_t *pAnimGroup, const mstudiobone_t *pAnimbone, mstudioanimdesc_t &animdesc, float fFrame, BoneVector *pos, BoneQuaternion *q, int boneMask, float flWeight )
{
	BONE_PROFILE_FUNC();
	byte *pData = animdesc.pZeroFrameData();

	if (!pData)
		return;

	int i, j;

	// Msg("zeroframe %s\n", animdesc.pszName() );
	if (animdesc.zeroframecount == 1)
	{
		for (j = 0; j < pAnimStudioHdr->numbones; j++)
		{
			if (pAnimGroup)
				i = pAnimGroup->masterBone[j];
			else
				i = j;

			if (pAnimbone[j].flags & BONE_HAS_SAVEFRAME_POS)
			{
				if ((i >= 0) && (pStudioHdr->boneFlags(i) & boneMask))
				{
					Vector p = *(Vector48 *)pData;
					pos[i] = pos[i] * (1.0f - flWeight) + p * flWeight;
					Assert( pos[i].IsValid() );
				}
				pData += sizeof( Vector48 );
			}
			if (pAnimbone[j].flags & BONE_HAS_SAVEFRAME_ROT64)
			{
				if ((i >= 0) && (pStudioHdr->boneFlags(i) & boneMask))
				{
					Quaternion q0 = *(Quaternion64 *)pData;
					QuaternionBlend( q[i], q0, flWeight, q[i] );
					Assert( q[i].IsValid() );
				}
				pData += sizeof( Quaternion64 );
			}
			else if (pAnimbone[j].flags & BONE_HAS_SAVEFRAME_ROT32)
			{
				if ((i >= 0) && (pStudioHdr->boneFlags(i) & boneMask))
				{
					Quaternion q0 = *(Quaternion32 *)pData;
					QuaternionBlend( q[i], q0, flWeight, q[i] );
					Assert( q[i].IsValid() );
				}
				pData += sizeof( Quaternion32 );
			}
		}
	}
	else
	{
		float s1;
		int index = fFrame / animdesc.zeroframespan;
		if (index >= animdesc.zeroframecount - 1)
		{
			index = animdesc.zeroframecount - 2;
			s1 = 1.0f;
		}
		else
		{
			s1 = clamp( (fFrame - index * animdesc.zeroframespan) / animdesc.zeroframespan, 0.0f, 1.0f );
		}
		int i0 = MAX( index - 1, 0 );
		int i1 = index;
		int i2 = MIN( index + 1, animdesc.zeroframecount - 1 );
		for (j = 0; j < pAnimStudioHdr->numbones; j++)
		{
			if (pAnimGroup)
				i = pAnimGroup->masterBone[j];
			else
				i = j;

			if (pAnimbone[j].flags & BONE_HAS_SAVEFRAME_POS)
			{
				if ((i >= 0) && (pStudioHdr->boneFlags(i) & boneMask))
				{
					Vector p0 = *(((Vector48 *)pData) + i0);
					Vector p1 = *(((Vector48 *)pData) + i1);
					Vector p2 = *(((Vector48 *)pData) + i2);
					if (flWeight == 1.0f)
					{
						// don't blend into an uninitialized value
						Hermite_Spline( p0, p1, p2, s1, pos[i] );
					}
					else
					{
						Vector p3;
						Hermite_Spline( p0, p1, p2, s1, p3 );
						pos[i] = pos[i] * (1.0f - flWeight) + p3 * flWeight;
					}

					Assert( pos[i].IsValid() );
				}
				pData += sizeof( Vector48 ) * animdesc.zeroframecount;
			}
			if (pAnimbone[j].flags & BONE_HAS_SAVEFRAME_ROT64)
			{
				if ((i >= 0) && (pStudioHdr->boneFlags(i) & boneMask))
				{
					Quaternion q0 = *(((Quaternion64 *)pData) + i0);
					Quaternion q1 = *(((Quaternion64 *)pData) + i1);
					Quaternion q2 = *(((Quaternion64 *)pData) + i2);
					if (flWeight == 1.0f)
					{
						// don't blend into an uninitialized value
						Hermite_Spline( q0, q1, q2, s1, q[i] );
					}
					else
					{
						Quaternion q3;
						Hermite_Spline( q0, q1, q2, s1, q3 );
						QuaternionBlend( q[i], q3, flWeight, q[i] );
					}
					Assert( q[i].IsValid() );
				}
				pData += sizeof( Quaternion64 ) * animdesc.zeroframecount;
			}
			else if (pAnimbone[j].flags & BONE_HAS_SAVEFRAME_ROT32)
			{
				if ((i >= 0) && (pStudioHdr->boneFlags(i) & boneMask))
				{
					Quaternion q0 = *(((Quaternion32 *)pData) + i0);
					Quaternion q1 = *(((Quaternion32 *)pData) + i1);
					Quaternion q2 = *(((Quaternion32 *)pData) + i2);
					if (flWeight == 1.0f)
					{
						// don't blend into an uninitialized value
						Hermite_Spline( q0, q1, q2, s1, q[i] );
					}
					else
					{
						Quaternion q3;
						Hermite_Spline( q0, q1, q2, s1, q3 );
						QuaternionBlend( q[i], q3, flWeight, q[i] );
					}
					Assert( q[i].IsValid() );
				}
				pData += sizeof( Quaternion32 ) * animdesc.zeroframecount;
			}
		}
	}
}
//-----------------------------------------------------------------------------
// Purpose: Extract and blend two frames from a mstudio_frame_anim_t block of data
//-----------------------------------------------------------------------------

inline byte *ExtractTwoFrames( byte flags, float s, byte *RESTRICT pFrameData, byte *&pConstantData, int framelength, BoneQuaternion &q, BoneVector &pos, bool bIsDelta = false, const mstudiolinearbone_t *pLinearBones = NULL, int bone = 0 )
{
	BONE_PROFILE_FUNC();
#ifdef _GAMECONSOLE
	if (flags & STUDIO_FRAME_ANIM_ROT)
	{
		fltx4 q1 = UnpackQuaternion48SIMD( (Quaternion48 *)(pFrameData) );
		fltx4 q2 = UnpackQuaternion48SIMD( (Quaternion48 *)(pFrameData + framelength) );
		
		fltx4 qBlend = QuaternionBlendSIMD( q1, q2, s );
		StoreAlignedSIMD( (QuaternionAligned*)&q, qBlend );
		pFrameData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_ROT2)
	{
		if ( false ) // slow/naive 
		{
			Quaternion q1;
			Quaternion q2;
			q1 = *((Quaternion48S *)(pFrameData));
			q2 = *((Quaternion48S *)(pFrameData + framelength));
			QuaternionBlend( q1, q2, s, q );
			Assert( q.IsValid() );
			pFrameData += sizeof( Quaternion48S );
		}
		else // simd
		{
			fltx4 q1;
			fltx4 q2;
			q1 = *((Quaternion48S *)(pFrameData));
			q2 = *((Quaternion48S *)(pFrameData + framelength));
			StoreUnalignedSIMD( q.Base(), QuaternionBlendSIMD( q1, q2, s ) );
			Assert( q.IsValid() );
			pFrameData += sizeof( Quaternion48S );
		}
	}
	else if (flags & STUDIO_FRAME_CONST_ROT)
	{
		fltx4 flt = UnpackQuaternion48SIMD( (Quaternion48 *)(pConstantData) );
		
		StoreAlignedSIMD( (QuaternionAligned*)&q, flt );
		
		pConstantData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_CONST_ROT2)
	{
		if ( false )  // slow/naive 
		{
			q = *((Quaternion48S *)(pConstantData));
			Assert( q.IsValid() );
			pConstantData += sizeof( Quaternion48S );
		}
		else
		{
			// q = *((Quaternion48S *)(pConstantData));
			StoreUnalignedSIMD(  q.Base(), (fltx4) *((Quaternion48S *)(pConstantData)) );
			Assert( q.IsValid() );
			pConstantData += sizeof( Quaternion48S );
		}
	}
	// the non-virtual version needs initializers for no-animation
	else if (pLinearBones)
	{
		if (bIsDelta)
		{
			q.Init( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		else
		{
			q = pLinearBones->quat( bone );
		}
	}
	if (flags & STUDIO_FRAME_ANIM_POS)
	{
		fltx4 p1 = UnpackVector48SIMD( (Vector48 *)(pFrameData) );
		fltx4 p2 = UnpackVector48SIMD( (Vector48 *)(pFrameData + framelength) );
		fltx4 f2 = ReplicateX4( s );
		fltx4 f1 = SubSIMD( Four_Ones, f2 );

		p2 = MulSIMD( p2, f2 );
		p1 = MaddSIMD( p1, f1, p2 );
		StoreUnaligned3SIMD( pos.Base(), p1 );

		pFrameData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_CONST_POS)
	{
		fltx4 flt = UnpackVector48SIMD( (Vector48 *)(pConstantData) );
		
		StoreUnaligned3SIMD( pos.Base(), flt );
		
		pConstantData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_POS2)
	{
		fltx4 p1 = LoadUnaligned3SIMD( (float *)(pFrameData) );
		fltx4 p2 = LoadUnaligned3SIMD( (float *)(pFrameData + framelength) );
		fltx4 f2 = ReplicateX4( s );
		fltx4 f1 = SubSIMD( Four_Ones, f2 );

		p2 = MulSIMD( p2, f2 );
		p1 = MaddSIMD( p1, f1, p2 );
		StoreUnaligned3SIMD( pos.Base(), p1 );

		pFrameData += sizeof( Vector );
	}
	else if (flags & STUDIO_FRAME_CONST_POS2)
	{
		fltx4 flt = LoadUnaligned3SIMD( (float *)(pConstantData) );

		StoreUnaligned3SIMD( pos.Base(), flt );

		pConstantData += sizeof( Vector );
	}
	// the non-virtual version needs initializers for no-animation
	else if (pLinearBones)
	{
		if (bIsDelta)
		{
			pos.Init( 0.0f, 0.0f, 0.0f );
		}
		else
		{
			pos = pLinearBones->pos( bone );
		}
	}

#else
	Quaternion q1, q2;
	// Making these aligned.  Could be VectorAligned instead, but I don't want to change the behavior of this code.
	ALIGN16 Vector p1;
	ALIGN16 Vector p2;

	if (flags & STUDIO_FRAME_ANIM_ROT)
	{
		q1 = *((Quaternion48 *)(pFrameData));
		q2 = *((Quaternion48 *)(pFrameData + framelength));
		QuaternionBlend( q1, q2, s, q );
		Assert( q.IsValid() );
		pFrameData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_ROT2)
	{
		q1 = *((Quaternion48S *)(pFrameData));
		q2 = *((Quaternion48S *)(pFrameData + framelength));
		QuaternionBlend( q1, q2, s, q );
		Assert( q.IsValid() );
		pFrameData += sizeof( Quaternion48S );
	}
	else if (flags & STUDIO_FRAME_CONST_ROT)
	{
		q = *((Quaternion48 *)(pConstantData));
		Assert( q.IsValid() );
		pConstantData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_CONST_ROT2)
	{
		q = *((Quaternion48S *)(pConstantData));
		Assert( q.IsValid() );
		pConstantData += sizeof( Quaternion48S );
	}
	// the non-virtual version needs initializers for no-animation
	else if (pLinearBones)
	{
		if (bIsDelta)
		{
			q.Init( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		else
		{
			q = pLinearBones->quat( bone );
		}
	}
	if (flags & STUDIO_FRAME_ANIM_POS)
	{
		p1 = *((Vector48 *)(pFrameData));
		p2 = *((Vector48 *)(pFrameData + framelength));
		pos = p1 * (1.0 - s) + p2 * s;
		Assert( pos.IsValid() );
		pFrameData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_CONST_POS)
	{
		pos = *((Vector48 *)(pConstantData));
		Assert( pos.IsValid() );
		pConstantData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_POS2)
	{
		// pFrameData has no alignment guarantees, so using V_memcpy.
		V_memcpy( &p1, pFrameData, sizeof( p1 ) );
		V_memcpy( &p2, pFrameData + framelength, sizeof( p2 ) );
		pos = p1 * (1.0 - s) + p2 * s;
		Assert( pos.IsValid() );
		pFrameData += sizeof( Vector );
	}
	else if (flags & STUDIO_FRAME_CONST_POS2)
	{
		// pFrameData has no alignment guarantees, so using V_memcpy.
		V_memcpy( &pos, pConstantData, sizeof( pos ) );
		Assert( pos.IsValid() );
		pConstantData += sizeof( Vector );
	}
	// the non-virtual version needs initializers for no-animation
	else if (pLinearBones)
	{
		if (bIsDelta)
		{
			pos.Init( 0.0f, 0.0f, 0.0f );
		}
		else
		{
			pos = pLinearBones->pos( bone );
		}
	}
#endif
	return pFrameData;
}

//-----------------------------------------------------------------------------
// Purpose: Extract one frame from a mstudio_frame_anim_t block of data
//-----------------------------------------------------------------------------

inline byte *ExtractSingleFrame( byte flags, byte *pFrameData, byte *&pConstantData, BoneQuaternion &q, BoneVector &pos, bool bIsDelta = false, const mstudiolinearbone_t *pLinearBones = NULL, int bone = 0 )
{
	BONE_PROFILE_FUNC();
#ifdef _GAMECONSOLE
	if (flags & STUDIO_FRAME_ANIM_ROT)
	{
		fltx4 flt = UnpackQuaternion48SIMD( (Quaternion48 *)(pFrameData) );
		
		StoreAlignedSIMD( (QuaternionAligned*)&q, flt );
		// FIXME: If this path needs to work on PS3, this might be the right line to replace the 360-specific code above.
//		StoreAlignedSIMD( ( QuaternionAligned * )&q, flt );
		
		pFrameData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_ROT2)
	{
		if ( false )  // slow/naive 
		{
			q = *((Quaternion48S *)(pFrameData));
			Assert( q.IsValid() );
			pFrameData += sizeof( Quaternion48S );
		}
		else
		{
			StoreUnalignedSIMD(  q.Base(), (fltx4) *((Quaternion48S *)(pFrameData)) );
			Assert( q.IsValid() );
		Assert( QuaternionsAreEqual( q, (Quaternion) *((Quaternion48S *)(pFrameData)), 0.001f ) );
			pFrameData += sizeof( Quaternion48S );
		}
	}
	else if (flags & STUDIO_FRAME_CONST_ROT)
	{
		fltx4 flt = UnpackQuaternion48SIMD( (Quaternion48 *)(pConstantData) );
		
		StoreAlignedSIMD( (QuaternionAligned*)&q, flt );
		// FIXME: If this path needs to work on PS3, this might be the right line to replace the 360-specific code above.
//		StoreAlignedSIMD( ( QuaternionAligned * )&q, flt );
		
		pConstantData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_CONST_ROT2)
	{
		if ( false )  // slow/naive 
		{
			q = *((Quaternion48S *)(pConstantData));
			Assert( q.IsValid() );
			pConstantData += sizeof( Quaternion48S );
		}
		else
		{
			StoreUnalignedSIMD(  q.Base(), (fltx4) *((Quaternion48S *)(pConstantData)) );
			Assert( q.IsValid() );
		Assert( QuaternionsAreEqual( q, (Quaternion) *((Quaternion48S *)(pConstantData)), 0.001f ) );
			pConstantData += sizeof( Quaternion48S );
		}
	}
	// the non-virtual version needs initializers for no-animation
	else if (pLinearBones)
	{
		if (bIsDelta)
		{
			q.Init( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		else
		{
			q = pLinearBones->quat( bone );
		}
	}
	if (flags & STUDIO_FRAME_ANIM_POS)
	{
		fltx4 flt = UnpackVector48SIMD( (Vector48 *)(pFrameData) );
		
		StoreUnaligned3SIMD( pos.Base(), flt );
		
		pFrameData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_CONST_POS)
	{
		fltx4 flt = UnpackVector48SIMD( (Vector48 *)(pConstantData) );
		
		StoreUnaligned3SIMD( pos.Base(), flt );
		
		pConstantData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_POS2)
	{
		fltx4 flt = LoadUnaligned3SIMD( (float *)(pFrameData) );
		StoreUnaligned3SIMD( pos.Base(), flt );

		pFrameData += sizeof( Vector );
	}
	else if (flags & STUDIO_FRAME_CONST_POS2)
	{
		fltx4 flt = LoadUnaligned3SIMD( (float *)(pConstantData) );
		StoreUnaligned3SIMD( pos.Base(), flt );

		pConstantData += sizeof( Vector );
	}
	// the non-virtual version needs initializers for no-animation
	else if (pLinearBones)
	{
		if (bIsDelta)
		{
			pos.Init( 0.0f, 0.0f, 0.0f );
		}
		else
		{
			pos = pLinearBones->pos( bone );
		}
	}
#else
	if (flags & STUDIO_FRAME_ANIM_ROT)
	{
		q = *((Quaternion48 *)(pFrameData));
		Assert( q.IsValid() );
		pFrameData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_ROT2)
	{
		q = *((Quaternion48S *)(pFrameData));
		Assert( q.IsValid() );
		pFrameData += sizeof( Quaternion48S );
	}
	else if (flags & STUDIO_FRAME_CONST_ROT)
	{
		q = *((Quaternion48 *)(pConstantData));
		Assert( q.IsValid() );
		pConstantData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_CONST_ROT2)
	{
		q = *((Quaternion48S *)(pConstantData));
		Assert( q.IsValid() );
		pConstantData += sizeof( Quaternion48S );
	}
	// the non-virtual version needs initializers for no-animation
	else if (pLinearBones)
	{
		if (bIsDelta)
		{
			q.Init( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		else
		{
			q = pLinearBones->quat( bone );
		}
	}
	if (flags & STUDIO_FRAME_ANIM_POS)
	{
		pos = *((Vector48 *)(pFrameData));
		Assert( pos.IsValid() );
		pFrameData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_CONST_POS)
	{
		pos = *((Vector48 *)(pConstantData));
		Assert( pos.IsValid() );
		pConstantData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_POS2)
	{
		// pFrameData has no guarantee on alignment, so using V_memcpy.
		V_memcpy( &pos, pFrameData, sizeof( Vector ) );
		Assert( pos.IsValid() );
		pFrameData += sizeof( Vector );
	}
	else if (flags & STUDIO_FRAME_CONST_POS2)
	{
		// pFrameData has no guarantee on alignment, so using V_memcpy.
		V_memcpy( &pos, pConstantData, sizeof( Vector ) );
		Assert( pos.IsValid() );
		pConstantData += sizeof( Vector );
	}
	// the non-virtual version needs initializers for no-animation
	else if (pLinearBones)
	{
		if (bIsDelta)
		{
			pos.Init( 0.0f, 0.0f, 0.0f );
		}
		else
		{
			pos = pLinearBones->pos( bone );
		}
	}
#endif

	return pFrameData;
}



//-----------------------------------------------------------------------------
// Purpose: Skip forward to the next bone in a mstudio_frame_anim_t block of data
//-----------------------------------------------------------------------------

inline byte *SkipBoneFrame( byte flags, byte * RESTRICT pFrameData, byte *&pConstantData )
{
	BONE_PROFILE_FUNC();
	if (flags & STUDIO_FRAME_ANIM_ROT)
	{
		pFrameData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_ROT2)
	{
		pFrameData += sizeof( Quaternion48S );
	}
	else if (flags & STUDIO_FRAME_CONST_ROT)
	{
		pConstantData += sizeof( Quaternion48 );
	}
	else if (flags & STUDIO_FRAME_CONST_ROT2)
	{
		pConstantData += sizeof( Quaternion48S );
	}
	if (flags & STUDIO_FRAME_ANIM_POS)
	{
		pFrameData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_CONST_POS)
	{
		pConstantData += sizeof( Vector48 );
	}
	else if (flags & STUDIO_FRAME_ANIM_POS2)
	{
		pFrameData += sizeof( Vector );
	}
	else if (flags & STUDIO_FRAME_CONST_POS2)
	{
		pConstantData += sizeof( Vector );
	}
	return pFrameData;
}


//-----------------------------------------------------------------------------
// Purpose: Extract a single bone of animation
//-----------------------------------------------------------------------------

void SetupSingleBoneMatrix( 
	CStudioHdr *pOwnerHdr, 
	int nSequence, 
	int iFrame,
	int iBone, 
	matrix3x4_t &mBoneLocal )
{
	BONE_PROFILE_FUNC();
	// FIXME: why does anyone call this instead of just looking up that entities cached animation?
	// Reading the callers, I don't see how what it returns is of any use

	mstudioseqdesc_t &seqdesc = pOwnerHdr->pSeqdesc( nSequence );
	mstudioanimdesc_t &animdesc = pOwnerHdr->pAnimdesc( seqdesc.anim( 0, 0 ) );
	int iLocalFrame = iFrame;
	float s = 0;
	const mstudiobone_t *pbone = pOwnerHdr->pBone( iBone );

	BoneQuaternion boneQuat;
	BoneVector bonePos;

	bool bFound = false;

	if (animdesc.flags & STUDIO_FRAMEANIM)
	{
		/*
		mstudio_frame_anim_t *pFrameanim = (mstudio_frame_anim_t *)animdesc.pAnim( &iLocalFrame );

		if (pFrameanim)
		{
			byte *pBoneFlags = pFrameanim->pBoneFlags( );
			byte *pConstantData = pFrameanim->pConstantData( );
			byte *pFrameData = pFrameanim->pFrameData( iLocalFrame );

			// FIXME: this is the local bone index, not the global bone index
			for (int i = 0; i < iBone; i++, pBoneFlags++)
			{
				pFrameData = SkipBoneFrame( *pBoneFlags, pFrameData, pConstantData );
			}
			pFrameData = ExtractSingleFrame( *pBoneFlags, pFrameData, pConstantData, boneQuat, bonePos );
			bFound = true;
		}
		*/
	}
	else
	{
		mstudio_rle_anim_t *panim = (mstudio_rle_anim_t *)animdesc.pAnim( &iLocalFrame );

		// search for bone
		// FIXME: this is the local bone index, not the global bone index
		while (panim && panim->bone != iBone)
		{
			panim = panim->pNext();
		}

		// look up animation if found, if not, initialize
		if (panim && seqdesc.weight(iBone) > 0)
		{
			CalcBoneQuaternion( iLocalFrame, s, pbone, NULL, panim, boneQuat );
			CalcBonePosition  ( iLocalFrame, s, pbone, NULL, panim, bonePos );
			bFound = true;
		}
	}

	if (!bFound)
	{
		if (animdesc.flags & STUDIO_DELTA)
		{
			boneQuat.Init( 0.0f, 0.0f, 0.0f, 1.0f );
			bonePos.Init( 0.0f, 0.0f, 0.0f );
		}
		else
		{
			boneQuat = pbone->quat;
			bonePos = pbone->pos;
		}
	}

	QuaternionMatrix( boneQuat, bonePos, mBoneLocal );
}


//-----------------------------------------------------------------------------
// Purpose: Find and decode a sub-frame of animation, remapping the skeleton bone indexes
//-----------------------------------------------------------------------------
static void CalcVirtualAnimation( virtualmodel_t *pVModel, const CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion *q, 
	mstudioseqdesc_t &seqdesc, int sequence, int animation,
	float cycle, int boneMask )
{
	BONE_PROFILE_FUNC(); // ex: x360: up to 1.4ms 
	SNPROF_ANIM("CalcVirtualAnimation");

	int	i, j, k;

	const mstudiobone_t *pbone;
	const virtualgroup_t *pSeqGroup;
	const studiohdr_t *pSeqStudioHdr;
	const mstudiolinearbone_t *pSeqLinearBones;
	const mstudiobone_t *pSeqbone;
	const studiohdr_t *pAnimStudioHdr;
	const mstudiolinearbone_t *pAnimLinearBones;
	const mstudiobone_t *pAnimbone;
	const virtualgroup_t *pAnimGroup;

	pSeqGroup = pVModel->pSeqGroup( sequence );
	int baseanimation = pStudioHdr->iRelativeAnim( sequence, animation );
	mstudioanimdesc_t &animdesc = ((CStudioHdr *)pStudioHdr)->pAnimdesc( baseanimation );
	pSeqStudioHdr = ((CStudioHdr *)pStudioHdr)->pSeqStudioHdr( sequence );
	pSeqLinearBones = pSeqStudioHdr->pLinearBones();
	pSeqbone = pSeqStudioHdr->pBone( 0 );
	pAnimGroup = pVModel->pAnimGroup( baseanimation );
	pAnimStudioHdr = ((CStudioHdr *)pStudioHdr)->pAnimStudioHdr( baseanimation );
	pAnimLinearBones = pAnimStudioHdr->pLinearBones();
	pAnimbone = pAnimStudioHdr->pBone( 0 );

#if _DEBUG
	extern IDataCache *g_pDataCache;
#ifndef _GAMECONSOLE
	// Consoles don't need to lock the modeldata cache since it never flushes
	static IDataCacheSection *pModelCache = g_pDataCache->FindSection( "ModelData" );
	AssertOnce( pModelCache->IsFrameLocking() );
#endif
	static IDataCacheSection *pAnimblockCache = g_pDataCache->FindSection( "AnimBlock" );
	AssertOnce( pAnimblockCache->IsFrameLocking() );
#endif

	int					iFrame;
	float				s;

	float fFrame = cycle * (animdesc.numframes - 1);

	iFrame = (int)fFrame;
	s = (fFrame - iFrame);

	int iLocalFrame = iFrame;
	float flStall;

	const mstudio_rle_anim_t *panim = NULL;
	const mstudio_frame_anim_t *pFrameanim = NULL;
	
	byte *pBoneFlags = NULL;
	byte *pConstantData = NULL;
	byte *pFrameData = NULL;
	byte *pFrameDataNext = NULL;
	int framelength = 0;
	
	if (animdesc.flags & STUDIO_FRAMEANIM)
	{
		pFrameanim = (mstudio_frame_anim_t *)animdesc.pAnim( &iLocalFrame, flStall );
		if ( pFrameanim )
		{
			pBoneFlags = pFrameanim->pBoneFlags( );
			pConstantData = pFrameanim->pConstantData( );
			pFrameData = pFrameanim->pFrameData( iLocalFrame );
			framelength = pFrameanim->framelength;
			pFrameDataNext = pFrameData + framelength;

			PREFETCH360( pBoneFlags, 0 );
			PREFETCH360( pFrameData, 0 );
			PREFETCH360( pConstantData, 0 );
			PREFETCH360( pFrameDataNext, 0 );
		}
	}
	else
	{
		panim = (mstudio_rle_anim_t *)animdesc.pAnim( &iLocalFrame, flStall );
	}

	float *pweight = seqdesc.pBoneweight( 0 );
	pbone = pStudioHdr->pBone( 0 );

	int nBoneList[MAXSTUDIOBONES];
	int nBoneListCount = 0;
	for (i = 0; i < pStudioHdr->numbones(); i++)
	{
		if (pStudioHdr->boneFlags(i) & boneMask)
		{
			int j = pSeqGroup->boneMap[i];
			if (j >= 0 && pweight[j] > 0.0f)
			{
				nBoneList[nBoneListCount++] = i;
			}
		}
	}

	if ( animdesc.flags & STUDIO_DELTA )
	{
		for ( i = 0; i < nBoneListCount; i++ )
		{
			int nBone = nBoneList[i];
			q[nBone].Init( 0.0f, 0.0f, 0.0f, 1.0f );
			pos[nBone].Init( 0.0f, 0.0f, 0.0f );
		}
	}
	else if (pSeqLinearBones)
	{
		const Quaternion *pLinearQuat = &pSeqLinearBones->quat( 0 );
		const Vector *pLinearPos = &pSeqLinearBones->pos( 0 );
		for ( i = 0; i < nBoneListCount; i++ )
		{
			int nBone = nBoneList[i];
			int j = pSeqGroup->boneMap[nBone];
			q[nBone] = pLinearQuat[j];
			pos[nBone] = pLinearPos[j];
		}
	}
	else
	{
		for ( i = 0; i < nBoneListCount; i++ )
		{
			int nBone = nBoneList[i];
			int j = pSeqGroup->boneMap[nBone];
			q[nBone] = pSeqbone[j].quat;
			pos[nBone] = pSeqbone[j].pos;
		}
	}
#ifdef STUDIO_ENABLE_PERF_COUNTERS
	pStudioHdr->m_nPerfUsedBones += nBoneListCount;
#endif

	// decode frame animation
	if (pFrameanim)
	{
// 		byte *pBoneFlags = pFrameanim->pBoneFlags( );
// 		byte *pConstantData = pFrameanim->pConstantData( );
// 		byte *pFrameData = pFrameanim->pFrameData( iLocalFrame );
// 		int framelength = pFrameanim->framelength;

		if (s > 0.0)
		{
			for (i = 0; i < pAnimStudioHdr->numbones; i++)
			{
				j = pAnimGroup->masterBone[i];
				if ( j >= 0 && ( pStudioHdr->boneFlags(j) & boneMask ) )
				{
					pFrameData = ExtractTwoFrames( *pBoneFlags, s, pFrameData, pConstantData, framelength, q[j], pos[j] );
	#ifdef STUDIO_ENABLE_PERF_COUNTERS
					pStudioHdr->m_nPerfAnimatedBones++;
	#endif
				}
				else
				{
					pFrameData = SkipBoneFrame( *pBoneFlags, pFrameData, pConstantData );
				}
				pBoneFlags++;
			}
		}
		else
		{
			for (i = 0; i < pAnimStudioHdr->numbones; i++)
			{
				j = pAnimGroup->masterBone[i];
				if ( j >= 0 && ( pStudioHdr->boneFlags(j) & boneMask ) )
				{
					pFrameData = ExtractSingleFrame( *pBoneFlags, pFrameData, pConstantData, q[j], pos[j] );
	#ifdef STUDIO_ENABLE_PERF_COUNTERS
					pStudioHdr->m_nPerfAnimatedBones++;
	#endif
				}
				else
				{
					pFrameData = SkipBoneFrame( *pBoneFlags, pFrameData, pConstantData );
				}
				pBoneFlags++;
			}
		}
	}
	else if (panim)
	{
		// FIXME: change encoding so that bone -1 is never the case
		while (panim && panim->bone < 255)
		{
			j = pAnimGroup->masterBone[panim->bone];
			if ( j >= 0 && ( pStudioHdr->boneFlags(j) & boneMask ) )
			{
				k = pSeqGroup->boneMap[j];

				if (k >= 0 && pweight[k] > 0.0f)
				{
					CalcBoneQuaternion( iLocalFrame, s, &pAnimbone[panim->bone], pAnimLinearBones, panim, q[j] );
					CalcBonePosition  ( iLocalFrame, s, &pAnimbone[panim->bone], pAnimLinearBones, panim, pos[j] );
	#ifdef STUDIO_ENABLE_PERF_COUNTERS
					pStudioHdr->m_nPerfAnimatedBones++;
	#endif
				}
			}
			panim = panim->pNext();
		}
	}
	else
	{
		CalcZeroframeData( pStudioHdr, pAnimStudioHdr, pAnimGroup, pAnimbone, animdesc, fFrame, pos, q, boneMask, 1.0 );
		return;
	}

	// cross fade in previous zeroframe data
	if (flStall > 0.0f)
	{
		CalcZeroframeData( pStudioHdr, pAnimStudioHdr, pAnimGroup, pAnimbone, animdesc, fFrame, pos, q, boneMask, flStall );
	}

	// calculate a local hierarchy override
	if (animdesc.numlocalhierarchy)
	{
		matrix3x4a_t *boneToWorld = g_MatrixPool.Alloc();
		CBoneBitList boneComputed;

		int i;
		for (i = 0; i < animdesc.numlocalhierarchy; i++)
		{
			mstudiolocalhierarchy_t *pHierarchy = animdesc.pHierarchy( i );

			if ( !pHierarchy )
				break;

			int iBone = pAnimGroup->masterBone[pHierarchy->iBone];
			if (iBone >= 0 && (pStudioHdr->boneFlags(iBone) & boneMask))
			{
				int iNewParent = pAnimGroup->masterBone[pHierarchy->iNewParent];
				if (iNewParent >= 0 && (pStudioHdr->boneFlags(iNewParent) & boneMask))
				{
					CalcLocalHierarchyAnimation( pStudioHdr, boneToWorld, boneComputed, pos, q, pbone, pHierarchy, iBone, iNewParent, cycle, iFrame, s, boneMask );
				}
			}
		}

		g_MatrixPool.Free( boneToWorld );
	}
}



//-----------------------------------------------------------------------------
// Purpose: Find and decode a sub-frame of animation
//-----------------------------------------------------------------------------
void CalcAnimation( const CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion *q, 
	mstudioseqdesc_t &seqdesc,
	int sequence, int animation,
	float cycle, int boneMask )
{
	BONE_PROFILE_FUNC();
#ifdef STUDIO_ENABLE_PERF_COUNTERS
	pStudioHdr->m_nPerfAnimationLayers++;
#endif

	virtualmodel_t *pVModel = pStudioHdr->GetVirtualModel();

	if (pVModel)
	{
		CalcVirtualAnimation( pVModel, pStudioHdr, pos, q, seqdesc, sequence, animation, cycle, boneMask );
		return;
	}

	SNPROF_ANIM("CalcAnimation");

#if _DEBUG
	extern IDataCache *g_pDataCache;
#ifndef _GAMECONSOLE
	// Consoles don't need to lock the modeldata cache since it never flushes
	static IDataCacheSection *pModelCache = g_pDataCache->FindSection( "ModelData" );
	AssertOnce( pModelCache->IsFrameLocking() );
#endif
	static IDataCacheSection *pAnimblockCache = g_pDataCache->FindSection( "AnimBlock" );
	AssertOnce( pAnimblockCache->IsFrameLocking() );
#endif

	mstudioanimdesc_t &animdesc = ((CStudioHdr *)pStudioHdr)->pAnimdesc( animation );
	const mstudiobone_t *pbone = pStudioHdr->pBone( 0 );
	const mstudiolinearbone_t *pLinearBones = pStudioHdr->pLinearBones();

	int					i;
	int					iFrame;
	float				s;

	float fFrame = cycle * (animdesc.numframes - 1);

	iFrame = (int)fFrame;
	s = (fFrame - iFrame);

	int iLocalFrame = iFrame;
	float flStall = 0.0f;

	const mstudio_rle_anim_t *panim = NULL;
	const mstudio_frame_anim_t *pFrameanim = NULL;
	
	byte *pBoneFlags = NULL;
	byte *pConstantData = NULL;
	byte *pFrameData = NULL;
	byte *pFrameDataNext = NULL;
	int framelength = NULL;

	if (animdesc.flags & STUDIO_FRAMEANIM)
	{
		pFrameanim = (mstudio_frame_anim_t *)animdesc.pAnim( &iLocalFrame, flStall );
		if ( pFrameanim )
		{
			pBoneFlags = pFrameanim->pBoneFlags( );
			pConstantData = pFrameanim->pConstantData( );
			pFrameData = pFrameanim->pFrameData( iLocalFrame );
			framelength = pFrameanim->framelength;
			pFrameDataNext = pFrameData + framelength;

			PREFETCH360( pBoneFlags, 0 );
			PREFETCH360( pFrameData, 0 );
			PREFETCH360( pConstantData, 0 );
			PREFETCH360( pFrameDataNext, 0 );
		}
	}
	else
	{
		panim = (mstudio_rle_anim_t *)animdesc.pAnim( &iLocalFrame, flStall );
	}

	float *pweight = seqdesc.pBoneweight( 0 );
	bool bIsDelta = (animdesc.flags & STUDIO_DELTA) != 0;

	// if the animation isn't available, look for the zero frame cache
	if (!panim && !pFrameanim)
	{
		// Msg("zeroframe %s\n", animdesc.pszName() );
		// pre initialize
		for (i = 0; i < pStudioHdr->numbones(); i++, pbone++, pweight++)
		{
			if (*pweight > 0 && (pStudioHdr->boneFlags(i) & boneMask))
			{
				if (bIsDelta)
				{
					q[i].Init( 0.0f, 0.0f, 0.0f, 1.0f );
					pos[i].Init( 0.0f, 0.0f, 0.0f );
				}
				else
				{
					q[i] = pbone->quat;
					pos[i] = pbone->pos;
				}
			}
		}

		CalcZeroframeData( pStudioHdr, pStudioHdr->GetRenderHdr(), NULL, pStudioHdr->pBone( 0 ), animdesc, fFrame, pos, q, boneMask, 1.0 );

		return;
	}

	// decode frame animation
	if (pFrameanim)
	{
// 		byte *pBoneFlags = pFrameanim->pBoneFlags( );
// 		byte *pConstantData = pFrameanim->pConstantData( );
// 		byte *pFrameData = pFrameanim->pFrameData( iLocalFrame );
// 		int framelength = pFrameanim->framelength;

		if (s > 0.0)
		{
			for (i = 0; i < pStudioHdr->numbones(); i++, pBoneFlags++, pweight++)
			{
				if (*pweight > 0 && (pStudioHdr->boneFlags(i) & boneMask))
				{
					pFrameData = ExtractTwoFrames( *pBoneFlags, s, pFrameData, pConstantData, framelength, q[i], pos[i], bIsDelta, pLinearBones, i );
	#ifdef STUDIO_ENABLE_PERF_COUNTERS
					pStudioHdr->m_nPerfAnimatedBones++;
	#endif
				}
				else
				{
					pFrameData = SkipBoneFrame( *pBoneFlags, pFrameData, pConstantData );
				}
				pStudioHdr->m_nPerfUsedBones++;
			}
		}
		else
		{
			for (i = 0; i < pStudioHdr->numbones(); i++, pBoneFlags++, pweight++)
			{
				if (*pweight > 0 && (pStudioHdr->boneFlags(i) & boneMask))
				{
					pFrameData = ExtractSingleFrame( *pBoneFlags, pFrameData, pConstantData, q[i], pos[i], bIsDelta, pLinearBones, i );
	#ifdef STUDIO_ENABLE_PERF_COUNTERS
					pStudioHdr->m_nPerfAnimatedBones++;
	#endif
				}
				else
				{
					pFrameData = SkipBoneFrame( *pBoneFlags, pFrameData, pConstantData );
				}
				pStudioHdr->m_nPerfUsedBones++;
			}
		}
	}
	else
	{
		// BUGBUG: the sequence, the anim, and the model can have all different bone mappings.
		for (i = 0; i < pStudioHdr->numbones(); i++, pbone++, pweight++)
		{
			if (panim && panim->bone == i)
			{
				if (*pweight > 0 && (pStudioHdr->boneFlags(i) & boneMask))
				{
					CalcBoneQuaternion( iLocalFrame, s, pbone, pLinearBones, panim, q[i] );
					CalcBonePosition  ( iLocalFrame, s, pbone, pLinearBones, panim, pos[i] );
	#ifdef STUDIO_ENABLE_PERF_COUNTERS
					pStudioHdr->m_nPerfAnimatedBones++;
					pStudioHdr->m_nPerfUsedBones++;
	#endif
				}
				panim = panim->pNext();
			}
			else if (*pweight > 0 && (pStudioHdr->boneFlags(i) & boneMask))
			{
				if (bIsDelta)
				{
					q[i].Init( 0.0f, 0.0f, 0.0f, 1.0f );
					pos[i].Init( 0.0f, 0.0f, 0.0f );
				}
				else
				{
					q[i] = pbone->quat;
					pos[i] = pbone->pos;
				}
	#ifdef STUDIO_ENABLE_PERF_COUNTERS
				pStudioHdr->m_nPerfUsedBones++;
	#endif
			}
		}
	}

	// cross fade in previous zeroframe data
	if (flStall > 0.0f)
	{
		CalcZeroframeData( pStudioHdr, pStudioHdr->GetRenderHdr(), NULL, pStudioHdr->pBone( 0 ), animdesc, fFrame, pos, q, boneMask, flStall );
	}

	if (animdesc.numlocalhierarchy)
	{
		matrix3x4a_t *boneToWorld = g_MatrixPool.Alloc();
		CBoneBitList boneComputed;

		int i;
		for (i = 0; i < animdesc.numlocalhierarchy; i++)
		{
			mstudiolocalhierarchy_t *pHierarchy = animdesc.pHierarchy( i );

			if ( !pHierarchy )
				break;

			if (pStudioHdr->boneFlags(pHierarchy->iBone) & boneMask)
			{
				if (pStudioHdr->boneFlags(pHierarchy->iNewParent) & boneMask)
				{
					CalcLocalHierarchyAnimation( pStudioHdr, boneToWorld, boneComputed, pos, q, pbone, pHierarchy, pHierarchy->iBone, pHierarchy->iNewParent, cycle, iFrame, s, boneMask );
				}
			}
		}

		g_MatrixPool.Free( boneToWorld );
	}
}

