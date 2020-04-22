//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "tier0/dbg.h"
#include "mathlib/mathlib.h"
#include "bone_setup_PS3.h"

#if !defined(__SPU__)
#include <string.h>
#include "tier0/vprof.h"
#include "bone_accessor.h"
#endif

#include "mathlib/ssequaternion.h"
#include "bone_utils_PS3.h"




//-----------------------------------------------------------------------------
// Purpose: return a sub frame rotation for a single bone
//-----------------------------------------------------------------------------
void ExtractAnimValue_PS3( int frame, mstudioanimvalue_t_PS3 *pEA_animvalue, float scale, float &v1, float &v2 )
{
	if( !pEA_animvalue )
	{
		v1 = v2 = 0;
		return;
	}

	byte ls_buf[2 * 0x10] ALIGN16;		// 32bytes to ensure alignment on lower 4 bits of address and crossover of 16B boundary since we may fetch 2 at a time
	byte ls_buf_v1[2 * 0x10] ALIGN16;
	byte ls_buf_v2[2 * 0x10] ALIGN16;

	mstudioanimvalue_t_PS3 *pLS_animvalue;
	mstudioanimvalue_t_PS3 *pLS_animvalue_v1;
	mstudioanimvalue_t_PS3 *pLS_animvalue_v2;

	pLS_animvalue = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_animvalue, sizeof( mstudioanimvalue_t_PS3 ) * 2 ); // * 2 to fetch [1] below
	// Avoids a crash reading off the end of the data
	// There is probably a better long-term solution; Ken is going to look into it.
	if( ( pLS_animvalue->num.total == 1 ) && ( pLS_animvalue->num.valid == 1 ) )
	{
		v1 = v2 = pLS_animvalue[1].value * scale;
		return;
	}

	int k = frame;

	// find the data list that has the frame
	while( pLS_animvalue->num.total <= k )
	{
		k -= pLS_animvalue->num.total;	// k -= panimvalue->num.total;
		
		pEA_animvalue += pLS_animvalue->num.valid + 1;	//panimvalue += panimvalue->num.valid + 1;
		pLS_animvalue = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_animvalue, sizeof( mstudioanimvalue_t_PS3 ) ); // * 2 to fetch [1] below

		if( pLS_animvalue->num.total == 0 )
		{
			Assert( 0 ); // running off the end of the animation stream is bad
			v1 = v2 = 0;
			return;
		}
	}
	if( pLS_animvalue->num.valid > k )
	{
		// has valid animation data
		//v1 = panimvalue[k+1].value * scale;
		pLS_animvalue_v1 = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf_v1, (uint32)(pEA_animvalue + k + 1), sizeof( mstudioanimvalue_t_PS3 ) * 2 ); // * 2 to fetch k+2 below
		v1 = pLS_animvalue_v1->value * scale;

		if( pLS_animvalue->num.valid > k + 1 )
		{
			// has valid animation blend data
			//v2 = panimvalue[k+2].value * scale;
			pLS_animvalue_v2 = pLS_animvalue_v1 + 1;	// already loaded above

			v2 = pLS_animvalue_v2->value * scale;
		}
		else
		{
			if( pLS_animvalue->num.total > k + 1 )
			{
				// data repeats, no blend
				v2 = v1;
			}
			else
			{
				pLS_animvalue_v2 = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf_v2, (uint32)(pEA_animvalue + pLS_animvalue->num.valid + 2), sizeof( mstudioanimvalue_t_PS3 ) ); 

				// pull blend from first data block in next list
				//v2 = panimvalue[panimvalue->num.valid+2].value * scale;
				v2 = pLS_animvalue_v2->value * scale;
			}
		}
	}
	else
	{
		// get last valid data block
		pLS_animvalue_v1 = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf_v1, (uint32)(pEA_animvalue + pLS_animvalue->num.valid), sizeof( mstudioanimvalue_t_PS3 ) ); 

		v1 = pLS_animvalue_v1->value * scale; //v1 = panimvalue[panimvalue->num.valid].value * scale;

		if( pLS_animvalue->num.total > k + 1 )
		{
			// data repeats, no blend
			v2 = v1;
		}
		else
		{
			pLS_animvalue_v2 = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf_v1, (uint32)(pEA_animvalue + pLS_animvalue->num.valid + 2), sizeof( mstudioanimvalue_t_PS3 ) ); 

			// pull blend from first data block in next list
			v2 = pLS_animvalue_v2->value * scale; // v2 = panimvalue[panimvalue->num.valid + 2].value * scale;
		}
	}
}

void ExtractAnimValue_PS3( int frame, mstudioanimvalue_t_PS3 *pEA_animvalue, float scale, float &v1 )
{
	if( !pEA_animvalue )
	{
		v1 = 0;
		return;
	}

	byte ls_buf[2 * 0x10] ALIGN16;
	byte ls_buf_v1[2 * 0x10] ALIGN16;

	mstudioanimvalue_t_PS3 *pLS_animvalue;
	mstudioanimvalue_t_PS3 *pLS_animvalue_v1;

	pLS_animvalue = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_animvalue, sizeof( mstudioanimvalue_t_PS3 ) ); 

	int k = frame;

	while( pLS_animvalue->num.total <= k )
	{
		k -= pLS_animvalue->num.total;

		pEA_animvalue += pLS_animvalue->num.valid + 1;	//panimvalue += panimvalue->num.valid + 1;
		pLS_animvalue = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_animvalue, sizeof( mstudioanimvalue_t_PS3 ) ); 

		if( pLS_animvalue->num.total == 0 )
		{
			Assert( 0 ); // running off the end of the animation stream is bad
			v1 = 0;
			return;
		}
	}

	if( pLS_animvalue->num.valid > k )
	{
		pLS_animvalue_v1 = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf_v1, (uint32)(pEA_animvalue + k + 1), sizeof( mstudioanimvalue_t_PS3 ) ); 

		v1 = pLS_animvalue_v1->value * scale;	// v1 = panimvalue[k+1].value * scale;
	}
	else
	{
		pLS_animvalue_v1 = (mstudioanimvalue_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf_v1, (uint32)(pEA_animvalue + pLS_animvalue->num.valid), sizeof( mstudioanimvalue_t_PS3 ) ); 

		// get last valid data block
		v1 = pLS_animvalue_v1->value * scale;	// v1 = panimvalue[panimvalue->num.valid].value * scale;
	}
}

//-----------------------------------------------------------------------------
// Purpose: return a sub frame rotation for a single bone
//-----------------------------------------------------------------------------
void CalcBoneQuaternion_PS3( int frame, float s, 
						     const Quaternion &baseQuat, const RadianEuler &baseRot, const Vector &baseRotScale, 
							 int iBaseFlags, const Quaternion &baseAlignment, 
							 const mstudio_rle_anim_t_PS3 *pLS_anim, mstudio_rle_anim_t_PS3 *pEA_anim, BoneQuaternion &q )
{
	byte ls_buf[ 2 * 0x10 ] ALIGN16;	// max of Quat48, Quat64, mstudioanim_valueptr_t_PS3

	if( pLS_anim->flags & STUDIO_ANIM_RAWROT )
	{
		byte *pEA_quat48 = (byte *)pLS_anim->pQuat48( pEA_anim );

		Quaternion48 *pQuat48 = (Quaternion48 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_quat48, sizeof( Quaternion48 ) ); 

//		byte ls_Q48[sizeof(Quaternion48)] ALIGN16;
//		memcpy(ls_Q48, pQuat48, sizeof(Quaternion48));
//		q = *((Quaternion48 *)&ls_Q48);

		q = *(pQuat48);

		AssertFatal( q.IsValid() );
		return;
	} 

	if( pLS_anim->flags & STUDIO_ANIM_RAWROT2 )
	{
		byte *pEA_quat64 = (byte *)pLS_anim->pQuat64( pEA_anim );

		Quaternion64 *pQuat64 = (Quaternion64 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_quat64, sizeof( Quaternion64 ) ); 

//		byte ls_Q64[sizeof(Quaternion64)] ALIGN16;
//		memcpy(ls_Q64, pQuat64, sizeof(Quaternion64));
//		q = *((Quaternion64 *)&ls_Q64);

		q = *(pQuat64);		// q = *(panim->pQuat64());

		AssertFatal( q.IsValid() );
		return;
	}

	if( !(pLS_anim->flags & STUDIO_ANIM_ANIMROT) )
	{
		if( pLS_anim->flags & STUDIO_ANIM_DELTA )
		{
			q.Init( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		else
		{
			q = baseQuat;
		}
		return;
	}

	mstudioanim_valueptr_t_PS3 *pLS_ValuesPtr;

	mstudioanim_valueptr_t_PS3 *pEA_ValuesPtr = pLS_anim->pRotV( pEA_anim );

	pLS_ValuesPtr = (mstudioanim_valueptr_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_ValuesPtr, sizeof( mstudioanim_valueptr_t_PS3 ) ); 

	if( s > 0.001f )
	{
		BoneQuaternion	q1, q2;
		RadianEuler		angle1 ALIGN16;
		RadianEuler		angle2 ALIGN16;

		ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, 0 ), baseRotScale.x, angle1.x, angle2.x );
		ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, 1 ), baseRotScale.y, angle1.y, angle2.y );
		ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, 2 ), baseRotScale.z, angle1.z, angle2.z );

		if( !(pLS_anim->flags & STUDIO_ANIM_DELTA) )
		{
			fltx4 a1, a2, br1;

			a1  = LoadAlignedSIMD( angle1.Base() );
			a2  = LoadAlignedSIMD( angle2.Base() );
			br1 = LoadUnalignedSIMD( baseRot.Base() );

			a1 = AddSIMD( a1, br1 );
			a2 = AddSIMD( a2, br1 );

			StoreUnaligned3SIMD( angle1.Base(), a1 );
			StoreUnaligned3SIMD( angle2.Base(), a2 );

// 			angle1.x = angle1.x + baseRot.x;
// 			angle1.y = angle1.y + baseRot.y;
// 			angle1.z = angle1.z + baseRot.z;
// 
// 			angle2.x = angle2.x + baseRot.x;
// 			angle2.y = angle2.y + baseRot.y;
// 			angle2.z = angle2.z + baseRot.z;
		}

		AssertFatal( angle1.IsValid() && angle2.IsValid() );
		if( angle1.x != angle2.x || angle1.y != angle2.y || angle1.z != angle2.z )
		{
			AngleQuaternion_PS3( angle1, q1 );
			AngleQuaternion_PS3( angle2, q2 );

#ifdef _X360
			fltx4 q1simd, q2simd, qsimd;
			q1simd = LoadAlignedSIMD( q1 );
			q2simd = LoadAlignedSIMD( q2 );
			qsimd = QuaternionBlendSIMD( q1simd, q2simd, s );
			StoreUnalignedSIMD( q.Base(), qsimd );
#else
			QuaternionBlend_PS3( q1, q2, s, q );
//			QuaternionBlend( q1, q2, s, q );
#endif
		}
		else
		{
			AngleQuaternion_PS3( angle1, q );
		}
	}
	else
	{
		RadianEuler			angle ALIGN16;


		ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, 0 ), baseRotScale.x, angle.x );
		ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, 1 ), baseRotScale.y, angle.y );
		ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, 2 ), baseRotScale.z, angle.z );

		if( !(pLS_anim->flags & STUDIO_ANIM_DELTA) )
		{
			fltx4 a1, br1;

			a1  = LoadAlignedSIMD( angle.Base() );
			br1 = LoadUnalignedSIMD( baseRot.Base() );

			a1 = AddSIMD( a1, br1 );

			StoreUnaligned3SIMD( angle.Base(), a1 );

// 			angle.x = angle.x + baseRot.x;
// 			angle.y = angle.y + baseRot.y;
// 			angle.z = angle.z + baseRot.z;
		}

		AssertFatal( angle.IsValid() );
		AngleQuaternion_PS3( angle, q );
	}

	AssertFatal( q.IsValid() );

	// align to unified bone
	if( !(pLS_anim->flags & STUDIO_ANIM_DELTA) && (iBaseFlags & BONE_FIXED_ALIGNMENT) )
	{
		QuaternionAlign_PS3( baseAlignment, q, q );
	}
}

// inline void CalcBoneQuaternion_PS3( int frame, float s, 
// 							   const mstudiobone_t *pBone,
// 							   const mstudiolinearbone_t *pLinearBones,
// 							   const mstudio_rle_anim_t *panim, BoneQuaternion &q )
// {
// 	if( pLinearBones )
// 	{
// 		CalcBoneQuaternion_PS3( frame, s, pLinearBones->quat(panim->bone), pLinearBones->rot(panim->bone), pLinearBones->rotscale(panim->bone), pLinearBones->flags(panim->bone), pLinearBones->qalignment(panim->bone), panim, q );
// 	}
// 	else
// 	{
// 		CalcBoneQuaternion_PS3( frame, s, pBone->quat, pBone->rot, pBone->rotscale, pBone->flags, pBone->qAlignment, panim, q );
// 	}
// }



//-----------------------------------------------------------------------------
// Purpose: return a sub frame position for a single bone
//-----------------------------------------------------------------------------
void CalcBonePosition_PS3(	int frame, float s,
							const Vector &basePos, const Vector &baseBoneScale, 
							const mstudio_rle_anim_t_PS3 *pLS_anim, mstudio_rle_anim_t_PS3 *pEA_anim, BoneVector &pos )
{
	byte ls_buf[ 2 * 0x10 ] ALIGN16;	// max Vec48, studioanim_valueptr_t_PS3

	if( pLS_anim->flags & STUDIO_ANIM_RAWPOS )
	{
		byte *pEA_pos = (byte *)pLS_anim->pPos( pEA_anim );
		Vector48 *pPos = (Vector48 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_pos, sizeof( Vector48 ) ); 

//		byte ls_P48[sizeof(Vector48)] ALIGN16;
//		memcpy(ls_P48, pPos, sizeof(Vector48));
//		pos = *((Vector48 *)&ls_P48);
		pos = *(pPos);

		AssertFatal( pos.IsValid() );

		return;
	}
	else if( !(pLS_anim->flags & STUDIO_ANIM_ANIMPOS) )
	{
		if( pLS_anim->flags & STUDIO_ANIM_DELTA )
		{
			pos.Init( 0.0f, 0.0f, 0.0f );
		}
		else
		{
			pos = basePos;
		}
		return;
	}

	mstudioanim_valueptr_t_PS3  *pLS_ValuesPtr;
	mstudioanim_valueptr_t_PS3  *pEA_ValuesPtr = pLS_anim->pPosV( pEA_anim );

	pLS_ValuesPtr = (mstudioanim_valueptr_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_ValuesPtr, sizeof( mstudioanim_valueptr_t_PS3 ) ); 
	int	j;

	if( s > 0.001f )
	{
//		float v1, v2;
// 		for( j = 0; j < 3; j++ )
// 		{
// 			ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, j ), baseBoneScale[j], v1, v2 );
// 			pos[j] = v1 * (1.0f - s) + v2 * s;
// 		}
		VectorAligned p1, p2;
		ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, 0 ), baseBoneScale[0], p1[0], p2[0] );
		ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, 1 ), baseBoneScale[1], p1[1], p2[1] );
		ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, 2 ), baseBoneScale[2], p1[2], p2[2] );

		fltx4 vp1, vp2;
		fltx4 s1, pos1;
		vp1  = LoadAlignedSIMD( &p1 );
		vp2  = LoadAlignedSIMD( &p2 );
		s1   = ReplicateX4( s );
		pos1 = MsubSIMD( vp1, s1, vp1 );
		pos1 = MaddSIMD( vp2, s1, pos1 );

		StoreAlignedSIMD( pos.Base(), pos1 );
	}
	else
	{
		for( j = 0; j < 3; j++ )
		{
			ExtractAnimValue_PS3( frame, pLS_ValuesPtr->pAnimvalue( pEA_ValuesPtr, j ), baseBoneScale[j], pos[j] );
		}
	}

	if( !(pLS_anim->flags & STUDIO_ANIM_DELTA) )
	{
		fltx4 bp1, p1;

		p1  = LoadAlignedSIMD( &pos );
		bp1 = LoadUnalignedSIMD( &basePos );
		p1  = AddSIMD( p1, bp1 );

		StoreAlignedSIMD( pos.Base(), p1 );

// 		pos.x = pos.x + basePos.x;
// 		pos.y = pos.y + basePos.y;
// 		pos.z = pos.z + basePos.z;
	}

	AssertFatal( pos.IsValid() );
}


// inline void CalcBonePosition_PS3( int frame, float s, 
// 							 const mstudiobone_t *pBone,
// 							 const mstudiolinearbone_t *pEA_LinearBones,
// 							 const mstudio_rle_anim_t *panim, BoneVector &pos )
// {
// 	if( pEA_LinearBones )
// 	{
// 		CalcBonePosition_PS3( frame, s, pLinearBones->pos(panim->bone), pLinearBones->posscale(panim->bone), panim, pos );
// 	}
// 	else
// 	{
// 		CalcBonePosition_PS3( frame, s, pBone->pos, pBone->posscale, panim, pos );
// 	}
// }

inline void CalcBonePositionQuaternion_PS3( int frame, float s, 
										    mstudiobone_t_PS3 *pEA_bone,
										    const bonejob_SPU *pBonejob,
										    mstudiolinearbone_t_PS3 *pEA_linearBones,
										    mstudio_rle_anim_t_PS3 *pLS_anim, mstudio_rle_anim_t_PS3 *pEA_anim, BoneVector &pos, BoneQuaternion &q )
{
	byte boneData[ 16 * 0x10  ] ALIGN16;

	if( pBonejob->pEA_studiohdr_linearBones )
	{
		int			*pLS_flags;
		Vector		*pLS_pos;
		Vector		*pLS_posscale;
		Vector		*pLS_rotscale;
		Quaternion	*pLS_quat;
		Quaternion	*pLS_qalignment;
		RadianEuler *pLS_rot;

		// pos
		pLS_pos = (Vector *)SPUmemcpy_UnalignedGet_MustSync( boneData, (uint32)((Vector *)pBonejob->pEA_linearbones_pos + pLS_anim->bone), sizeof(Vector), DMATAG_ANIM );

		// posscale
		pLS_posscale = (Vector *)SPUmemcpy_UnalignedGet_MustSync( boneData + (2*0x10), (uint32)((Vector *)pBonejob->pEA_linearbones_posscale + pLS_anim->bone), sizeof(Vector), DMATAG_ANIM );

		SPUmemcpy_Sync( 1<<DMATAG_ANIM );

		CalcBonePosition_PS3( frame, s, (Vector &)*pLS_pos, (Vector &)*pLS_posscale, pLS_anim, pEA_anim, pos );

		// rotscale
		pLS_rotscale = (Vector *)SPUmemcpy_UnalignedGet_MustSync( boneData + (4*0x10), (uint32)((Vector *)pBonejob->pEA_linearbones_rotscale + pLS_anim->bone), sizeof(Vector), DMATAG_ANIM );

		// quat
		pLS_quat = (Quaternion *)SPUmemcpy_UnalignedGet_MustSync( boneData + (6*0x10), (uint32)((Quaternion *)pBonejob->pEA_linearbones_quat + pLS_anim->bone), sizeof(Quaternion), DMATAG_ANIM );

		// quat
		pLS_qalignment = (Quaternion *)SPUmemcpy_UnalignedGet_MustSync( boneData + (8*0x10), (uint32)((Quaternion *)pBonejob->pEA_linearbones_qalignment + pLS_anim->bone), sizeof(Quaternion), DMATAG_ANIM );

		// rot
		pLS_rot = (RadianEuler *)SPUmemcpy_UnalignedGet_MustSync( boneData + (10*0x10), (uint32)((RadianEuler *)pBonejob->pEA_linearbones_rot + pLS_anim->bone), sizeof(RadianEuler), DMATAG_ANIM );

		// flags
		pLS_flags = (int *)SPUmemcpy_UnalignedGet_MustSync( boneData + (12*0x10), (uint32)((int *)pBonejob->pEA_linearbones_flags + pLS_anim->bone), sizeof(int), DMATAG_ANIM );

		SPUmemcpy_Sync( 1<<DMATAG_ANIM );

		CalcBoneQuaternion_PS3( frame, s, (Quaternion &)*pLS_quat, (RadianEuler &)*pLS_rot, (Vector &)*pLS_rotscale, *pLS_flags, (Quaternion &)*pLS_qalignment, pLS_anim, pEA_anim, q );
	}
	else
	{
		mstudiobone_t_PS3_postoqalignment *pLS_bonedata;
		
		pLS_bonedata = (mstudiobone_t_PS3_postoqalignment *)SPUmemcpy_UnalignedGet( boneData, (uint32)pEA_bone, sizeof(mstudiobone_t_PS3_postoqalignment) );

		CalcBonePosition_PS3( frame, s, pLS_bonedata->pos, pLS_bonedata->posscale, pLS_anim, pEA_anim, pos );
		CalcBoneQuaternion_PS3( frame, s, pLS_bonedata->quat, pLS_bonedata->rot, pLS_bonedata->rotscale, pLS_bonedata->flags, pLS_bonedata->qAlignment, pLS_anim, pEA_anim, q );
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CalcDecompressedAnimation_PS3( void *pEA_Compressed, const mstudiocompressedikerror_t_PS3 *pLS_Compressed, int iFrame, float fraq, BoneVector &pos, BoneQuaternion &q )
{
	if( fraq > 0.0001f ) 
	{
		VectorAligned p1;
		VectorAligned p2;
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 0 ), pLS_Compressed->scale[0], p1.x, p2.x );
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 1 ), pLS_Compressed->scale[1], p1.y, p2.y );
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 2 ), pLS_Compressed->scale[2], p1.z, p2.z );

//		pos = p1 * (1.0f - fraq) + p2 * fraq;
		fltx4 vp1, vp2;
		fltx4 f1, pos1;
		vp1  = LoadAlignedSIMD( &p1 );
		vp2  = LoadAlignedSIMD( &p2 );
		f1   = ReplicateX4( fraq );
		pos1 = MsubSIMD( vp1, f1, vp1 );
		pos1 = MaddSIMD( vp2, f1, pos1 );
		StoreAlignedSIMD( pos.Base(), pos1 );

		BoneQuaternion	q1, q2;
		RadianEuler		angle1, angle2;
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 3 ), pLS_Compressed->scale[3], angle1.x, angle2.x );
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 4 ), pLS_Compressed->scale[4], angle1.y, angle2.y );
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 5 ), pLS_Compressed->scale[5], angle1.z, angle2.z );

		if( angle1.x != angle2.x || angle1.y != angle2.y || angle1.z != angle2.z )
		{
			AngleQuaternion_PS3( angle1, q1 );
			AngleQuaternion_PS3( angle2, q2 );
			QuaternionBlend_PS3( q1, q2, fraq, q );
//			QuaternionBlend( q1, q2, fraq, q );
		}
		else
		{
			AngleQuaternion_PS3( angle1, q );
		}
	}
	else
	{
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 0 ), pLS_Compressed->scale[0], pos.x );
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 1 ), pLS_Compressed->scale[1], pos.y );
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 2 ), pLS_Compressed->scale[2], pos.z );

		RadianEuler			angle;
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 3 ), pLS_Compressed->scale[3], angle.x );
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 4 ), pLS_Compressed->scale[4], angle.y );
		ExtractAnimValue_PS3( iFrame, pLS_Compressed->pAnimvalue( pEA_Compressed, 5 ), pLS_Compressed->scale[5], angle.z );

		AngleQuaternion_PS3( angle, q );
	}

	AssertFatal( pos.IsValid() );
	AssertFatal( q.IsValid() );
}

#if 0 // not supported on SPU yet, rare path
//-----------------------------------------------------------------------------
// Purpose: translate animations done in a non-standard parent space
//-----------------------------------------------------------------------------
static void CalcLocalHierarchyAnimation_PS3( 
										const bonejob_SPU *pBonejob,
										const CStudioHdr *pStudioHdr,
										matrix3x4a_t *boneToWorld,
										CBoneBitList_PS3 &boneComputed,
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

	CalcDecompressedAnimation_PS3( pHierarchy->pLocalAnim(), iFrame - pHierarchy->iStart, flFraq, localPos, localQ );

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
	BuildBoneChainPartial_PS3( pBonejob, rootXform, pos, q, iBone, boneToWorld, boneComputed, iRoot1 );
	BuildBoneChainPartial_PS3( pBonejob, rootXform, pos, q, iNewParent, boneToWorld, boneComputed, iRoot1 );

	matrix3x4a_t localXform;
	AngleMatrix_PS3( localQ, localPos, localXform );

	ConcatTransforms_Aligned_PS3( boneToWorld[iNewParent], localXform, boneToWorld[iBone] );

	// back solve
	BoneVector p1;
	BoneQuaternion q1;
	int n = pbone[iBone].parent;
	if (n == -1)
	{
		if (weight == 1.0f)
		{
			MatrixAngles_PS3( boneToWorld[iBone], q[iBone], pos[iBone] );
		}
		else
		{
			MatrixAngles_PS3( boneToWorld[iBone], q1, p1 );
			QuaternionSlerp_PS3( q[iBone], q1, weight, q[iBone] );
			pos[iBone] = Lerp_PS3( weight, p1, pos[iBone] );
		}
	}
	else
	{
		matrix3x4a_t worldToBone;
		MatrixInvert_PS3( boneToWorld[n], worldToBone );

		matrix3x4a_t local;
		ConcatTransforms_Aligned_PS3( worldToBone, boneToWorld[iBone], local );
		if (weight == 1.0f)
		{
			MatrixAngles_PS3( local, q[iBone], pos[iBone] );
		}
		else
		{
			MatrixAngles_PS3( local, q1, p1 );
			QuaternionSlerp_PS3( q[iBone], q1, weight, q[iBone] );
			pos[iBone] = Lerp_PS3( weight, p1, pos[iBone] );
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Calc Zeroframe Data
//-----------------------------------------------------------------------------
static void CalcZeroframeData_PS3( const animData_SPU *pAnim, const mstudioanimdesc_t_PS3 *pLS_animdesc, const bonejob_SPU *pBonejob, int *animgroup_masterBone, int *animbone_flags, float fFrame, BoneVector *pos, BoneQuaternion *q, int boneMask, float flWeight )
{
	void *pEA_animdesc = pAnim->pEA_animdesc;

	byte *pEA_Data = pLS_animdesc->pZeroFrameData( pEA_animdesc );//byte *)pAnim->pEA_animdesc_pZeroFrameData;
	byte *pLS_Data;

	if( !pEA_Data )
		return;

	int i, j;

	byte ls_buf[ 0x10 * 4  ] ALIGN16;	// Vector48 * 3 and overlap


	if( pLS_animdesc->zeroframecount == 1 )
	{
		// get pAnimbone flags 
		// get masterbone
		
		for( j = 0; j < pAnim->animstudiohdr_numbones; j++ )
		{
			if( animgroup_masterBone )
			{
				i = animgroup_masterBone[ j ];
			}
			else
			{
				i = j;
			}

			if( animbone_flags[ j ] & BONE_HAS_SAVEFRAME_POS )
			{
				if( (i >= 0) && (pBonejob->boneFlags[ i ] & boneMask) )
				{
					// TODO: simple data fetch for now, optimize when working
					pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_Data, sizeof( Vector48 ) );
					Vector p = *(Vector48 *)pLS_Data;

					//pos[i] = pos[i] * (1.0f - flWeight) + p * flWeight;
					fltx4 posi, w1, p1;

					posi = LoadAlignedSIMD( &pos[i] );
					p1   = LoadUnalignedSIMD( &p );
					w1   = ReplicateX4( flWeight );
					posi = MsubSIMD( posi, w1, posi );
					posi = MaddSIMD( p1, w1, posi );

					StoreAlignedSIMD( pos[i].Base(), posi );

					AssertFatal( pos[i].IsValid() );
				}
				pEA_Data += sizeof( Vector48 );
			}
			if( animbone_flags[ j ] & BONE_HAS_SAVEFRAME_ROT64 )
			{
				if( (i >= 0) && (pBonejob->boneFlags[ i ] & boneMask) )
				{
					pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_Data, sizeof( Quaternion64 ) );
					Quaternion q0 = *(Quaternion64 *)pLS_Data;
					QuaternionBlend_PS3( q[i], q0, flWeight, q[i] );
//					QuaternionBlend( q[i], q0, flWeight, q[i] );
					AssertFatal( q[i].IsValid() );
				}
				pEA_Data += sizeof( Quaternion64 );
			}
			else if( animbone_flags[ j ] & BONE_HAS_SAVEFRAME_ROT32 )
			{
				if( (i >= 0) && (pBonejob->boneFlags[ i ] & boneMask) )
				{
					pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_Data, sizeof( Quaternion32 ) );
					Quaternion q0 = *(Quaternion32 *)pLS_Data;
					QuaternionBlend_PS3( q[i], q0, flWeight, q[i] );
//					QuaternionBlend( q[i], q0, flWeight, q[i] );
					AssertFatal( q[i].IsValid() );
				}
				pEA_Data += sizeof( Quaternion32 );
			}
		}
	}
	else
	{
		float s1;
		int index = (int)(fFrame / (float)pLS_animdesc->zeroframespan);
		if( index >= pLS_animdesc->zeroframecount - 1 )
		{
			index = pLS_animdesc->zeroframecount - 2;
			s1 = 1.0f;
		}
		else
		{
			s1 = clamp( (fFrame - index * pLS_animdesc->zeroframespan) / pLS_animdesc->zeroframespan, 0.0f, 1.0f );
		}
		int i0 = MAX( index - 1, 0 );
		int i1 = index;
		int i2 = MIN( index + 1, pLS_animdesc->zeroframecount - 1 );

		int off1 = (i1 - i0);
		int off2 = (i2 - i0);

		for( j = 0; j < pAnim->animstudiohdr_numbones; j++ )
		{
			if( animgroup_masterBone )
			{
				i = animgroup_masterBone[ j ];
			}
			else
			{
				i = j;
			}

			if( animbone_flags[ j ] & BONE_HAS_SAVEFRAME_POS )
			{
				if( (i >= 0) && (pBonejob->boneFlags[ i ] & boneMask) )
				{
					pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)(((Vector48 *)pEA_Data) + i0), sizeof( Vector48 ) * 3 );

					Vector p0 = *(((Vector48 *)pLS_Data));			// pEA_Data + i0
					Vector p1 = *(((Vector48 *)pLS_Data) + off1);	// pEA_Data + i1
					Vector p2 = *(((Vector48 *)pLS_Data) + off2);	// pEA_Data + i2

					if( flWeight == 1.0f )
					{
						// don't blend into an uninitialized value
						Hermite_Spline_PS3( p0, p1, p2, s1, pos[i] );
					}
					else
					{
						Vector p3;
						Hermite_Spline_PS3( p0, p1, p2, s1, p3 );
						pos[i] = pos[i] * (1.0f - flWeight) + p3 * flWeight;
					}

					AssertFatal( pos[i].IsValid() );
				}
				pEA_Data += sizeof( Vector48 ) * pLS_animdesc->zeroframecount;
			}
			if( animbone_flags[ j ] & BONE_HAS_SAVEFRAME_ROT64 )
			{
				if( (i >= 0) && (pBonejob->boneFlags[ i ] & boneMask) )
				{
					pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)(((Quaternion64 *)pEA_Data) + i0), sizeof( Quaternion64 ) * 3 );

					Quaternion q0 = *(((Quaternion64 *)pLS_Data));			// pEA_Data + i0
					Quaternion q1 = *(((Quaternion64 *)pLS_Data) + off1);	// pEA_Data + i1
					Quaternion q2 = *(((Quaternion64 *)pLS_Data) + off2);	// pEA_Data + i2

					if( flWeight == 1.0f )
					{
						// don't blend into an uninitialized value
						Hermite_Spline_PS3( q0, q1, q2, s1, q[i] );
					}
					else
					{
						Quaternion q3;
						Hermite_Spline_PS3( q0, q1, q2, s1, q3 );
						QuaternionBlend_PS3( q[i], q3, flWeight, q[i] );
//						QuaternionBlend( q[i], q3, flWeight, q[i] );
					}
					AssertFatal( q[i].IsValid() );
				}
				pEA_Data += sizeof( Quaternion64 ) * pLS_animdesc->zeroframecount;
			}
			else if( animbone_flags[ j ] & BONE_HAS_SAVEFRAME_ROT32 )
			{
				if( (i >= 0) && (pBonejob->boneFlags[ i ] & boneMask) )
				{
					pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)(((Quaternion32 *)pEA_Data) + i0), sizeof( Quaternion32 ) * 3 );

					Quaternion q0 = *(((Quaternion32 *)pLS_Data));			// pEA_Data + i0
					Quaternion q1 = *(((Quaternion32 *)pLS_Data) + off1);	// pEA_Data + i1
					Quaternion q2 = *(((Quaternion32 *)pLS_Data) + off2);	// pEA_Data + i2
					if (flWeight == 1.0f)
					{
						// don't blend into an uninitialized value
						Hermite_Spline_PS3( q0, q1, q2, s1, q[i] );
					}
					else
					{
						Quaternion q3;
						Hermite_Spline_PS3( q0, q1, q2, s1, q3 );
						QuaternionBlend_PS3( q[i], q3, flWeight, q[i] );
//						QuaternionBlend( q[i], q3, flWeight, q[i] );
					}
					AssertFatal( q[i].IsValid() );
				}
				pEA_Data += sizeof( Quaternion32 ) * pLS_animdesc->zeroframecount;
			}
		}
	}
}
//-----------------------------------------------------------------------------
// Purpose: Extract and blend two frames from a mstudio_frame_anim_t block of data
//-----------------------------------------------------------------------------
inline byte *ExtractTwoFrames_PS3( byte flags, float s, byte *pEA_FrameData, byte *&pEA_ConstantData, int framelength, BoneQuaternion &q, BoneVector &pos, bool bIsDelta = false, const mstudiolinearbone_t_PS3 *pEA_LinearBones = NULL, int bone = 0 )
{
	byte ls_buf1[ 0x10 * 2  ] ALIGN16;	
	byte ls_buf2[ 0x10 * 2  ] ALIGN16;	

	byte *pLS_Data1;
	byte *pLS_Data2;

	if( flags & STUDIO_FRAME_ANIM_ROT )
	{
		pLS_Data2 = (byte *)SPUmemcpy_UnalignedGet_MustSync( ls_buf2, (uint32)(pEA_FrameData + framelength), sizeof( Quaternion48 ), DMATAG_ANIM ); 
		pLS_Data1 = (byte *)SPUmemcpy_UnalignedGet( ls_buf1, (uint32)pEA_FrameData, sizeof( Quaternion48 ) );

		fltx4 q1 = UnpackQuaternion48SIMD( (Quaternion48 *)(pLS_Data1) );
		SPUmemcpy_Sync( 1<<DMATAG_ANIM );
		fltx4 q2 = UnpackQuaternion48SIMD( (Quaternion48 *)(pLS_Data2) );
		
		fltx4 qBlend = QuaternionBlendSIMD( q1, q2, s );
		StoreAlignedSIMD( (BoneQuaternion*)&q, qBlend );

		AssertFatal( q.IsValid() );

		pEA_FrameData += sizeof( Quaternion48 );
	}
	else if( flags & STUDIO_FRAME_ANIM_ROT2 ) 
	{
		pLS_Data2 = (byte *)SPUmemcpy_UnalignedGet_MustSync( ls_buf2, (uint32)(pEA_FrameData + framelength), sizeof( Quaternion48S ), DMATAG_ANIM );  // 2*sizeof() ensures no overlap
		pLS_Data1 = (byte *)SPUmemcpy_UnalignedGet( ls_buf1, (uint32)pEA_FrameData, sizeof( Quaternion48S ) );

		fltx4 q1;
		fltx4 q2;
		q1 = *((Quaternion48S *)(pLS_Data1));
		SPUmemcpy_Sync( 1<<DMATAG_ANIM );
		q2 = *((Quaternion48S *)(pLS_Data2));

		//ALIGN StoreUnalignedSIMD( q.Base(), QuaternionBlendSIMD( q1, q2, s ) );
		StoreAlignedSIMD( q.Base(), QuaternionBlendSIMD( q1, q2, s ) );

		AssertFatal( q.IsValid() );

		pEA_FrameData += sizeof( Quaternion48S );
	}
	else if( flags & STUDIO_FRAME_CONST_ROT )
	{
		pLS_Data1 = (byte *)SPUmemcpy_UnalignedGet( ls_buf1, (uint32)pEA_ConstantData, sizeof( Quaternion48 ) );

		fltx4 flt = UnpackQuaternion48SIMD( (Quaternion48 *)(pLS_Data1) );
		
		StoreAlignedSIMD( (QuaternionAligned*)&q, flt );

		AssertFatal( q.IsValid() );
	
		pEA_ConstantData += sizeof( Quaternion48 );
	}
	else if( flags & STUDIO_FRAME_CONST_ROT2 )
	{
		pLS_Data1 = (byte *)SPUmemcpy_UnalignedGet( ls_buf1, (uint32)pEA_ConstantData, sizeof( Quaternion48S ) );
		
		//ALIGN StoreUnalignedSIMD(  q.Base(), (fltx4) *((Quaternion48S *)(pLS_Data1)) );
		StoreAlignedSIMD(  q.Base(), (fltx4) *((Quaternion48S *)(pLS_Data1)) );

		AssertFatal( q.IsValid() );

		pEA_ConstantData += sizeof( Quaternion48S );
	}
	// the non-virtual version needs initializers for no-animation
	else if( pEA_LinearBones )
	{
		if( bIsDelta )
		{
			q.Init( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		else
		{
			// take from ::init results
			q = g_qInit[ bone ];
			//q = pLinearBones->quat( bone );
		}
	}

	if( flags & STUDIO_FRAME_ANIM_POS )
	{
		pLS_Data2 = (byte *)SPUmemcpy_UnalignedGet_MustSync( ls_buf2, (uint32)(pEA_FrameData + framelength), sizeof(Vector48), DMATAG_ANIM );  
		pLS_Data1 = (byte *)SPUmemcpy_UnalignedGet( ls_buf1, (uint32)pEA_FrameData, sizeof(Vector48) );

		fltx4 p1 = UnpackVector48SIMD( (Vector48 *)(pLS_Data1) );

		SPUmemcpy_Sync( 1<<DMATAG_ANIM);
		fltx4 p2 = UnpackVector48SIMD( (Vector48 *)(pLS_Data2) );

		fltx4 f2 = ReplicateX4( s );
		fltx4 f1 = SubSIMD( Four_Ones, f2 );

		p2 = MulSIMD( p2, f2 );
		p1 = MaddSIMD( p1, f1, p2 );
		//ALIGN StoreUnaligned3SIMD( pos.Base(), p1 );
		StoreAlignedSIMD( pos.Base(), p1 );

		AssertFatal( pos.IsValid() );

		pEA_FrameData += sizeof( Vector48 );
	}
	else if( flags & STUDIO_FRAME_CONST_POS )
	{
		pLS_Data1 = (byte *)SPUmemcpy_UnalignedGet( ls_buf1, (uint32)pEA_ConstantData, sizeof(Vector48) );

		fltx4 flt = UnpackVector48SIMD( (Vector48 *)(pLS_Data1) );
		
		// ALIGN StoreUnaligned3SIMD( pos.Base(), flt );
		StoreAlignedSIMD( pos.Base(), flt );

		AssertFatal( pos.IsValid() );
		
		pEA_ConstantData += sizeof( Vector48 );
	}
	else if( flags & STUDIO_FRAME_ANIM_POS2 )
	{
		pLS_Data2 = (byte *)SPUmemcpy_UnalignedGet_MustSync( ls_buf2, (uint32)(pEA_FrameData + framelength), sizeof(Vector), DMATAG_ANIM );  
		pLS_Data1 = (byte *)SPUmemcpy_UnalignedGet( ls_buf1, (uint32)pEA_FrameData, sizeof(Vector) );

		fltx4 p1 = LoadUnaligned3SIMD( (float *)(pLS_Data1) );
		SPUmemcpy_Sync( 1<<DMATAG_ANIM );
		fltx4 p2 = LoadUnaligned3SIMD( (float *)(pLS_Data2) );

		fltx4 f2 = ReplicateX4( s );
		fltx4 f1 = SubSIMD( Four_Ones, f2 );

		p2 = MulSIMD( p2, f2 );
		p1 = MaddSIMD( p1, f1, p2 );
		//ALIGN StoreUnaligned3SIMD( pos.Base(), p1 );
		StoreAlignedSIMD( pos.Base(), p1 );

		AssertFatal( pos.IsValid() );
		pEA_FrameData += sizeof( Vector );
	}
	else if( flags & STUDIO_FRAME_CONST_POS2 )
	{
		pLS_Data1 = (byte *)SPUmemcpy_UnalignedGet( ls_buf1, (uint32)pEA_ConstantData, sizeof(Vector) );

		fltx4 flt = LoadUnaligned3SIMD( (float *)(pLS_Data1) );

		//ALIGN StoreUnaligned3SIMD( pos.Base(), flt );
		StoreAlignedSIMD( pos.Base(), flt );

		AssertFatal( pos.IsValid() );

		pEA_ConstantData += sizeof( Vector );
	}
	// the non-virtual version needs initializers for no-animation
	else if( pEA_LinearBones )
	{
		if( bIsDelta )
		{
			pos.Init( 0.0f, 0.0f, 0.0f );
		}
		else
		{
			// take from ::init results, is this okay?
			pos = g_posInit[ bone ];
			//pos = pLinearBones->pos( bone );
		}
	}

	return pEA_FrameData;
}

//-----------------------------------------------------------------------------
// Purpose: Extract one frame from a mstudio_frame_anim_t block of data
//-----------------------------------------------------------------------------
inline byte *ExtractSingleFrame_PS3( byte flags, byte *pEA_FrameData, byte *&pEA_ConstantData, BoneQuaternion &q, BoneVector &pos, bool bIsDelta = false, const mstudiolinearbone_t_PS3 *pEA_LinearBones = NULL, int bone = 0 )
{
	byte ls_buf[ 0x10 * 2  ] ALIGN16;	
	byte *pLS_Data;

	if( flags & STUDIO_FRAME_ANIM_ROT )
	{
		pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_FrameData, sizeof( Quaternion48 ) );

		fltx4 flt = UnpackQuaternion48SIMD( (Quaternion48 *)(pLS_Data) );
		StoreAlignedSIMD( (QuaternionAligned*)&q, flt );
		AssertFatal( q.IsValid() );
		
		pEA_FrameData += sizeof( Quaternion48 );
	}
	else if( flags & STUDIO_FRAME_ANIM_ROT2 )
	{
		pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_FrameData, sizeof( Quaternion48S ) );

		//ALIGN StoreUnalignedSIMD( q.Base(), (fltx4) *((Quaternion48S *)(pLS_Data)) );
		StoreAlignedSIMD( q.Base(), (fltx4) *((Quaternion48S *)(pLS_Data)) );

		AssertFatal( q.IsValid() );

		pEA_FrameData += sizeof( Quaternion48S );
	}
	else if( flags & STUDIO_FRAME_CONST_ROT )
	{
		pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_ConstantData, sizeof( Quaternion48 ) );

		fltx4 flt = UnpackQuaternion48SIMD( (Quaternion48 *)(pLS_Data) );
		StoreAlignedSIMD( (QuaternionAligned*)&q, flt );

		AssertFatal( q.IsValid() );
		
		pEA_ConstantData += sizeof( Quaternion48 );
	}
	else if( flags & STUDIO_FRAME_CONST_ROT2 )
	{
		pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_ConstantData, sizeof( Quaternion48S ) );

		//ALIGN StoreUnalignedSIMD(  q.Base(), (fltx4) *((Quaternion48S *)(pLS_Data)) );
		StoreAlignedSIMD(  q.Base(), (fltx4) *((Quaternion48S *)(pLS_Data)) );

		AssertFatal( q.IsValid() );

		pEA_ConstantData += sizeof( Quaternion48S );
	}
	// the non-virtual version needs initializers for no-animation
	else if( pEA_LinearBones )
	{
		if( bIsDelta )
		{
			q.Init( 0.0f, 0.0f, 0.0f, 1.0f );
		}
		else
		{
			// take from ::init results
			q = g_qInit[ bone ];
			//q = pLinearBones->quat( bone );
		}
	}

	if( flags & STUDIO_FRAME_ANIM_POS )
	{
		pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_FrameData, sizeof( Vector48 ) );

		fltx4 flt = UnpackVector48SIMD( (Vector48 *)(pLS_Data) );
		//ALIGN StoreUnaligned3SIMD( pos.Base(), flt );
		StoreAlignedSIMD( pos.Base(), flt );

		AssertFatal( pos.IsValid() );
		
		pEA_FrameData += sizeof( Vector48 );
	}
	else if( flags & STUDIO_FRAME_CONST_POS )
	{
		pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_ConstantData, sizeof( Vector48 ) );

		fltx4 flt = UnpackVector48SIMD( (Vector48 *)(pLS_Data) );
		//ALIGN StoreUnaligned3SIMD( pos.Base(), flt );
		StoreAlignedSIMD( pos.Base(), flt );

		AssertFatal( pos.IsValid() );
		
		pEA_ConstantData += sizeof( Vector48 );
	}
	else if( flags & STUDIO_FRAME_ANIM_POS2 )
	{
		pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_FrameData, sizeof( Vector ) );

		fltx4 flt = LoadUnaligned3SIMD( (float *)(pLS_Data) );
		//ALIGN StoreUnaligned3SIMD( pos.Base(), flt );
		StoreAlignedSIMD( pos.Base(), flt );
		
		AssertFatal( pos.IsValid() );

		pEA_FrameData += sizeof( Vector );
	}
	else if( flags & STUDIO_FRAME_CONST_POS2 ) 
	{
		pLS_Data = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_ConstantData, sizeof( Vector ) );

		fltx4 flt = LoadUnaligned3SIMD( (float *)(pLS_Data) );
		//ALIGN StoreUnaligned3SIMD( pos.Base(), flt );
		StoreAlignedSIMD( pos.Base(), flt );

		AssertFatal( pos.IsValid() );

		pEA_ConstantData += sizeof( Vector );
	}
	// the non-virtual version needs initializers for no-animation
	else if( pEA_LinearBones )
	{
		if (bIsDelta)
		{
			pos.Init( 0.0f, 0.0f, 0.0f );
		}
		else
		{
			// take from ::init results
			pos = g_posInit[ bone ];
			//pos = pLinearBones->pos( bone );
		}
	}

	return pEA_FrameData;
}



//-----------------------------------------------------------------------------
// Purpose: Skip forward to the next bone in a mstudio_frame_anim_t block of data
//-----------------------------------------------------------------------------
inline byte *SkipBoneFrame_PS3( byte flags, byte *pEA_FrameData, byte *&pEA_ConstantData )
{
	if( flags & STUDIO_FRAME_ANIM_ROT )
	{
		pEA_FrameData += sizeof( Quaternion48 );
	}
	else if( flags & STUDIO_FRAME_ANIM_ROT2 )
	{
		pEA_FrameData += sizeof( Quaternion48S );
	}
	else if( flags & STUDIO_FRAME_CONST_ROT )
	{
		pEA_ConstantData += sizeof( Quaternion48 );
	}
	else if( flags & STUDIO_FRAME_CONST_ROT2 )
	{
		pEA_ConstantData += sizeof( Quaternion48S );
	}
	if( flags & STUDIO_FRAME_ANIM_POS )
	{
		pEA_FrameData += sizeof( Vector48 );
	}
	else if( flags & STUDIO_FRAME_CONST_POS )
	{
		pEA_ConstantData += sizeof( Vector48 );
	}
	else if( flags & STUDIO_FRAME_ANIM_POS2 )
	{
		pEA_FrameData += sizeof( Vector );
	}
	else if( flags & STUDIO_FRAME_CONST_POS2 )
	{
		pEA_ConstantData += sizeof( Vector );
	}
	return pEA_FrameData;
}


#if 0 // not supported on SPU - no callers
//-----------------------------------------------------------------------------
// Purpose: Extract a single bone of animation
//-----------------------------------------------------------------------------
void SetupSingleBoneMatrix_PS3( 
	CStudioHdr *pOwnerHdr, 
	int nSequence, 
	int iFrame,
	int iBone, 
	matrix3x4a_t &mBoneLocal )
{
	// FIXME: why does anyone call this instead of just looking up that entities cached animation?
	// Reading the callers, I don't see how what it returns is of any use

	mstudioseqdesc_t &seqdesc = pOwnerHdr->pSeqdesc( nSequence );
	mstudioanimdesc_t &animdesc = pOwnerHdr->pAnimdesc( seqdesc.anim( 0, 0 ) );
	int iLocalFrame = iFrame;
	float s = 0;
	const mstudiobone_t *pbone = pOwnerHdr->pBone( iBone );

	BoneQuaternion boneQuat;
	BoneVector     bonePos;

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
			CalcBoneQuaternion_PS3( iLocalFrame, s, pbone, NULL, panim, boneQuat );
			CalcBonePosition_PS3  ( iLocalFrame, s, pbone, NULL, panim, bonePos );
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
#endif

//-----------------------------------------------------------------------------
// Purpose: Find and decode a sub-frame of animation, remapping the skeleton bone indexes
//-----------------------------------------------------------------------------
static void CalcVirtualAnimation_PS3( const bonejob_SPU *pBonejob, const accumposeentry_SPU *pPoseEntry, const animData_SPU *pAnim, BoneVector *pos, BoneQuaternion *q, const int *boneMap, const float *boneWeight, int animIndex, float cycle, int boneMask )
{
	SNPROF_ANIM("CalcVirtualAnimation_PS3");

	int	i, j, k;

	mstudiobone_t_PS3 *pEA_Animbone = (mstudiobone_t_PS3 *)pAnim->pEA_anim_bones_pos;

	byte ls_frameanim[ sizeof(mstudio_frame_anim_t_PS3)+0x10 ] ALIGN16;

	int	ls_masterBone[ MAXSTUDIOBONES_PS3+4 ] ALIGN16;
	int	*masterBone;

	// get masterbone[] once
	masterBone = (int *)SPUmemcpy_UnalignedGet_MustSync( ls_masterBone, (uint32)pAnim->pEA_animgroup_masterbone, sizeof(int) * pAnim->animstudiohdr_numbones, DMATAG_ANIM );

	// get animdesc
	byte ls_animdesc[sizeof(mstudioanimdesc_t_PS3)+16] ALIGN16;
	mstudioanimdesc_t_PS3 *pLS_animdesc;

	pLS_animdesc = (mstudioanimdesc_t_PS3 *)SPUmemcpy_UnalignedGet( ls_animdesc, (uint32)pAnim->pEA_animdesc, sizeof(mstudioanimdesc_t_PS3) );

	int		iFrame;
	float	s;

	float fFrame	= pPoseEntry->cycle * (pLS_animdesc->numframes - 1); 
	iFrame			= (int)fFrame;	
	s				= (fFrame - iFrame);
	float flStall	= pAnim->flStall;

	mstudio_rle_anim_t_PS3		*pEA_anim			= NULL;
	mstudio_rle_anim_t_PS3		*pLS_anim			= NULL;
	mstudio_frame_anim_t_PS3	*pEA_Frameanim		= NULL;
	mstudio_frame_anim_t_PS3	*pLS_Frameanim		= NULL;


	byte boneFlags[ MAXSTUDIOBONES_PS3+0x10 ] ALIGN16;	
	byte *pBoneFlags		= NULL;
	byte *pEA_FrameData		= NULL;
	byte *pEA_ConstantData	= NULL;


	if( pLS_animdesc->flags & STUDIO_FRAMEANIM )
	{
		pEA_Frameanim			= (mstudio_frame_anim_t_PS3 *)pAnim->pEA_animdesc_pFrameanim;

		if ( pEA_Frameanim)
		{
			pLS_Frameanim = (mstudio_frame_anim_t_PS3 *)SPUmemcpy_UnalignedGet( ls_frameanim, (uint32)pEA_Frameanim, sizeof(mstudio_frame_anim_t_PS3) );

			void *pEA_boneflags = pLS_Frameanim->pBoneFlags( pEA_Frameanim );

			// prefetch
			pBoneFlags			= (byte *)SPUmemcpy_UnalignedGet_MustSync( boneFlags, (uint32)pEA_boneflags, sizeof(byte) * pAnim->animstudiohdr_numbones, DMATAG_ANIM );

			pEA_FrameData		= pLS_Frameanim->pFrameData( pEA_Frameanim, pAnim->animdesc_iLocalFrame );//(byte *)pAnim->pEA_animdesc_frameData;
			pEA_ConstantData	= pLS_Frameanim->pConstantData( pEA_Frameanim );//(byte *)pAnim->pEA_animdesc_constantData;
		}
	}
	else
	{
		pEA_anim				= (mstudio_rle_anim_t_PS3 *)pAnim->pEA_animdesc_panim;
	}

	int nBoneList[ MAXSTUDIOBONES_PS3 + 4];
	int animboneFlags[ MAXSTUDIOBONES_PS3 + 4 ];

	int *pAnimboneFlags = NULL;

	int nBoneListCount = 0;

	// bonemap
	SPUmemcpy_Sync( 1<<DMATAG_ANIM_SYNC_BONEMAPWEIGHT );

	for( i = 0; i < pBonejob->numBones; i++ )
	{
		if( pBonejob->boneFlags[i] & boneMask )
		{
			int j = boneMap[i];
			if( j >= 0 && boneWeight[j] > 0.0f )
			{
				nBoneList[ nBoneListCount++ ] = i;
			}
		}
	}

	if( pLS_animdesc->flags & STUDIO_DELTA )
	{
		for( i = 0; i < nBoneListCount; i++ )
		{
			int nBone = nBoneList[ i ];
			q[nBone].Init( 0.0f, 0.0f, 0.0f, 1.0f );
			pos[nBone].Init( 0.0f, 0.0f, 0.0f );
		}
	}
	else if( pPoseEntry->pEA_seq_linearBones )
	{
		Vector		*pLS_pos;
		Quaternion	*pLS_quat;
		byte		ls_pos[ 0x10 * 2 ] ALIGN16;
		byte		ls_q[ 0x10 * 2 ] ALIGN16;


		// try interleaving dma's with loop
		for( i = 0; i < nBoneListCount; i++ )
		{
			int nBone	= nBoneList[i];
			int j		= boneMap[nBone];

			pLS_pos		= (Vector *)SPUmemcpy_UnalignedGet_MustSync( ls_pos, (uint32)((Vector *)pPoseEntry->pEA_seq_linearbones_pos + j), sizeof(Vector), DMATAG_ANIM_SYNC_POSQ );			//		const Vector *pLinearPos = &pSeqLinearBones->pos( 0 );
			pLS_quat	= (Quaternion *)SPUmemcpy_UnalignedGet_MustSync( ls_q, (uint32)((Quaternion *)pPoseEntry->pEA_seq_linearbones_quat + j), sizeof(Quaternion), DMATAG_ANIM_SYNC_POSQ );	//		const Quaternion *pLinearQuat = &pSeqLinearBones->quat( 0 );
			SPUmemcpy_Sync( 1<<DMATAG_ANIM_SYNC_POSQ );			

			pos[nBone]	= *pLS_pos;		// pLinearPos[j];
			q[nBone]	= *pLS_quat;	// pLinearQuat[j];

			AssertFatal(pos[nBone].IsValid());
			AssertFatal(q[nBone].IsValid());
		}
	}
	else
	{
		byte		*pLS_posquat;
		Vector		*pLS_bonepos;
		Quaternion	*pLS_bonequat;
		byte		ls_posq[ 0x10 * 4 ] ALIGN16;

		for( i = 0; i < nBoneListCount; i++ )
		{
			int nBone	 = nBoneList[i];
			int j		 = boneMap[nBone];

			pLS_posquat  = (byte *)SPUmemcpy_UnalignedGet( ls_posq, (uint32)((mstudiobone_t_PS3 *)pPoseEntry->pEA_seq_bones_pos + j), sizeof(Vector) + sizeof(Quaternion) );

			pLS_bonepos  = (Vector *)pLS_posquat;
			pLS_bonequat = (Quaternion *)(pLS_posquat + sizeof(Vector));

			pos[nBone]	 = *pLS_bonepos;	// pSeqbone[j].pos;
			q[nBone]	 = *pLS_bonequat;	// pSeqbone[j].quat;

			AssertFatal(pos[nBone].IsValid());
			AssertFatal(q[nBone].IsValid());
		}
	}

	// sync on masterbone DMA
	SPUmemcpy_Sync( 1<<DMATAG_ANIM );

	// decode frame animation
	if( pAnim->pEA_animdesc_pFrameanim )
	{
		if( s > 0.0f )
		{
			for( i = 0; i < pAnim->animstudiohdr_numbones; i++ )
			{
				j = masterBone[i];

				if( j >= 0 && (pBonejob->boneFlags[j] & boneMask) )
				{
					pEA_FrameData = ExtractTwoFrames_PS3( *pBoneFlags, s, pEA_FrameData, pEA_ConstantData, pLS_Frameanim->framelength, q[j], pos[j] );
				}
				else
				{
					pEA_FrameData = SkipBoneFrame_PS3( *pBoneFlags, pEA_FrameData, pEA_ConstantData );
				}
				pBoneFlags++;
			}
		}
		else
		{
			for( i = 0; i < pAnim->animstudiohdr_numbones; i++ )
			{
				j = masterBone[i];

				if( j >= 0 && (pBonejob->boneFlags[j] & boneMask) )
				{
					pEA_FrameData = ExtractSingleFrame_PS3( *pBoneFlags, pEA_FrameData, pEA_ConstantData, q[j], pos[j] );
				}
				else
				{
					pEA_FrameData = SkipBoneFrame_PS3( *pBoneFlags, pEA_FrameData, pEA_ConstantData );
				}
				pBoneFlags++;
			}
		}
	}
	else if( pEA_anim )
	{
		byte					ls_buf[ 0x10 * 2 ] ALIGN16;

		// anim
		pLS_anim = (mstudio_rle_anim_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_anim, sizeof( mstudio_rle_anim_t_PS3 ) ); 

		// FIXME: change encoding so that bone -1 is never the case
		while( pEA_anim && pLS_anim->bone < 255 )
		{
			j = masterBone[ pLS_anim->bone ];

			if( j >= 0 && (pBonejob->boneFlags[j] & boneMask ) )
			{
				k = boneMap[ j ];

				if( k >= 0 && boneWeight[ k ] > 0.0f )
				{
					CalcBonePositionQuaternion_PS3( pAnim->animdesc_iLocalFrame, s, pEA_Animbone + pLS_anim->bone, pBonejob, (mstudiolinearbone_t_PS3 *)pAnim->pEA_anim_linearBones, pLS_anim, pEA_anim, pos[j], q[j] );
				}
			}

			pEA_anim = pLS_anim->pNext( pEA_anim );
			if( pEA_anim )
			{
				pLS_anim = (mstudio_rle_anim_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_anim, sizeof( mstudio_rle_anim_t_PS3 ) ); 
			}
		}
	}
	else
	{
			
		// gather flags, lazy looped copy for now
		// TODO: dma chain, but this is a rare path, still worthwhile?
		int  ls_int[ 8 ] ALIGN16;	// 32B
		int *pLS_int;

		pAnimboneFlags = animboneFlags;
		for( int lp = 0; lp < pAnim->animstudiohdr_numbones; lp++ )
		{
			pLS_int = (int *)SPUmemcpy_UnalignedGet( ls_int, (uint32)((mstudiobone_t_PS3 *)pAnim->pEA_anim_bones_flags + lp), sizeof( int ) ); 
			pAnimboneFlags[ lp ] = *pLS_int;
		}

		CalcZeroframeData_PS3( pAnim, pLS_animdesc, pBonejob, masterBone, pAnimboneFlags, fFrame, pos, q, boneMask, 1.0f );


		return;
	}

	// cross fade in previous zeroframe data
	if( flStall > 0.0f )
	{
		if( !pAnimboneFlags )
		{
			int  ls_int[ 8 ] ALIGN16;	// 32B
			int *pLS_int;

			pAnimboneFlags = animboneFlags;
			for( int lp = 0; lp < pAnim->animstudiohdr_numbones; lp++ )
			{
				pLS_int = (int *)SPUmemcpy_UnalignedGet( ls_int, (uint32)((mstudiobone_t_PS3 *)pAnim->pEA_anim_bones_flags + lp), sizeof( int ) ); 

				pAnimboneFlags[ lp ] = *pLS_int;
			}
		}

		CalcZeroframeData_PS3( pAnim, pLS_animdesc, pBonejob, masterBone, pAnimboneFlags, fFrame, pos, q, boneMask, 1.0f );
	}

#if 0 // don't take this path on SPU - rare path, do on PPU only
	// calculate a local hierarchy override
	if( animdesc.numlocalhierarchy )
	{
		matrix3x4a_t *boneToWorld = g_matStack[0];
		CBoneBitList_PS3 boneComputed;

		int i;
		for( i = 0; i < animdesc.numlocalhierarchy; i++ )
		{
			mstudiolocalhierarchy_t *pHierarchy = animdesc.pHierarchy( i );

			if( !pHierarchy )
				break;

			int iBone = pAnimGroup->masterBone[pHierarchy->iBone];
			if( iBone >= 0 && (pStudioHdr->boneFlags(iBone) & boneMask) )
			{
				int iNewParent = pAnimGroup->masterBone[pHierarchy->iNewParent];
				if( iNewParent >= 0 && (pStudioHdr->boneFlags(iNewParent) & boneMask) )
				{
					CalcLocalHierarchyAnimation_PS3( pBonejob, pStudioHdr, boneToWorld, boneComputed, pos, q, pbone, pHierarchy, iBone, iNewParent, cycle, iFrame, s, boneMask );
				}
			}
		}
	}
#endif


	return;
}



//-----------------------------------------------------------------------------
// Purpose: Find and decode a sub-frame of animation
//-----------------------------------------------------------------------------
void CalcAnimation_PS3( const bonejob_SPU *pBonejob, const accumposeentry_SPU *pPoseEntry, BoneVector *pos, BoneQuaternion *q, const int *boneMap, const float *boneWeight, int animIndex, float cycle, int boneMask )
{
	SNPROF_ANIM( "CalcAnimation_PS3" );

	const animData_SPU	*pAnim	= &pPoseEntry->anims[ animIndex ];

	if( pBonejob->pEA_studiohdr_vmodel )
	{
		CalcVirtualAnimation_PS3( pBonejob, pPoseEntry, pAnim, pos, q, boneMap, boneWeight, animIndex, cycle, boneMask );
		return;
	}

	byte ls_frameanim[ sizeof(mstudio_frame_anim_t_PS3)+0x10 ] ALIGN16;

	// get animdesc
	byte ls_animdesc[ sizeof(mstudioanimdesc_t_PS3)+0x10 ] ALIGN16;
	mstudioanimdesc_t_PS3 *pLS_animdesc;

	pLS_animdesc = (mstudioanimdesc_t_PS3 *)SPUmemcpy_UnalignedGet( ls_animdesc, (uint32)pAnim->pEA_animdesc, sizeof(mstudioanimdesc_t_PS3) );

	int		i, iFrame;
	float	s;

	float fFrame	= pPoseEntry->cycle * (pLS_animdesc->numframes - 1); 
	iFrame			= (int)fFrame;	
	s				= (fFrame - iFrame);

	float flStall	= pAnim->flStall;


	mstudio_rle_anim_t_PS3		*pEA_anim		= NULL;
	mstudio_rle_anim_t_PS3		*pLS_anim		= NULL;
	mstudio_frame_anim_t_PS3	*pEA_Frameanim	= NULL;
	mstudio_frame_anim_t_PS3	*pLS_Frameanim	= NULL;

	mstudiobone_t_PS3		*pEA_bone			= (mstudiobone_t_PS3 *)pBonejob->pEA_studiohdr_bones_pos;  // = pEA_studiohdr_bones + studiobone_posoffset, pts to pos member
//	Vector					*pEA_LinearBone_pos = (Vector *)pBonejob->pEA_linearbones_pos;

	const float *pweight	= boneWeight;
	bool bIsDelta			= (pLS_animdesc->flags & STUDIO_DELTA) != 0;

	byte boneFlags[ MAXSTUDIOBONES_PS3+0x10 ] ALIGN16;		
	byte *pBoneFlags		= NULL;
	byte *pEA_FrameData		= NULL;
	byte *pEA_ConstantData	= NULL;

	if( pLS_animdesc->flags & STUDIO_FRAMEANIM )
	{
		pEA_Frameanim			= (mstudio_frame_anim_t_PS3 *)pAnim->pEA_animdesc_pFrameanim;

		if( pEA_Frameanim )
		{
			pLS_Frameanim = (mstudio_frame_anim_t_PS3 *)SPUmemcpy_UnalignedGet( ls_frameanim, (uint32)pEA_Frameanim, sizeof(mstudio_frame_anim_t_PS3) );

			void *pEA_boneflags = pLS_Frameanim->pBoneFlags( pEA_Frameanim );

			// prefetch
			pBoneFlags			= (byte *)SPUmemcpy_UnalignedGet_MustSync( boneFlags, (uint32)pEA_boneflags, sizeof(byte) * pBonejob->numBones, DMATAG_ANIM );

			pEA_FrameData		= pLS_Frameanim->pFrameData( pEA_Frameanim, pAnim->animdesc_iLocalFrame );//(byte *)pAnim->pEA_animdesc_frameData;
			pEA_ConstantData	= pLS_Frameanim->pConstantData( pEA_Frameanim );//(byte *)pAnim->pEA_animdesc_constantData;
		}
	}
	else
	{
		pEA_anim				= (mstudio_rle_anim_t_PS3 *)pAnim->pEA_animdesc_panim;
	}

	SPUmemcpy_Sync( 1<<DMATAG_ANIM_SYNC_BONEMAPWEIGHT );

	// if the animation isn't available, look for the zero frame cache
	if( !pAnim->pEA_animdesc_panim && !pAnim->pEA_animdesc_pFrameanim )
	{
		// pre initialize
		if( bIsDelta )
		{
			for( i = 0; i < pBonejob->numBones; i++, pweight++ )
			{
				if( *pweight > 0.0f && (pBonejob->boneFlags[i] & boneMask) )
				{
					q[i].Init( 0.0f, 0.0f, 0.0f, 1.0f );
					pos[i].Init( 0.0f, 0.0f, 0.0f );
				}
			}
		}
		else
		{
			// TODO: can we just use ::init results here?
			// TODO: prefetch - don't worry as this path is rare?
			// TODO: dma chain?
			byte ls_posquat[ 0x10 * 3 ] ALIGN16;	

			byte		*pLS_posquat;
			Vector		*pLS_bonepos;
			Quaternion	*pLS_bonequat;


			for( i = 0; i < pBonejob->numBones; i++, pEA_bone++, pweight++ )
			{
				// NOTE: we don't want to pull in every studiobone (212B), so we short-dma just the pos and quat here 
				// could build a dma chain. DMA's assume:
				// 1. offset is offset from start of mstudiobone_t to pos field.
				// 2. quat field immeiately follows pos field.
				pLS_posquat = (byte *)SPUmemcpy_UnalignedGet( ls_posquat, (uint32)pEA_bone, sizeof(Vector) + sizeof(Quaternion) );

				pLS_bonepos  = (Vector *)pLS_posquat;
				pLS_bonequat = (Quaternion *)(pLS_posquat + sizeof(Vector));

				if( *pweight> 0.0f && (pBonejob->boneFlags[i] & boneMask) )
				{
					pos[i] = *((Vector *)(pLS_bonepos));
					q[i]   = *((Quaternion *)(pLS_bonequat));

					AssertFatal(pos[i].IsValid());
					AssertFatal(q[i].IsValid());
				}
			}
		}

		//CalcZeroframeData_PS3( pStudioHdr, pStudioHdr->GetRenderHdr(), NULL, pStudioHdr->pBone( 0 ), animdesc, fFrame, pos, q, boneMask, 1.0f );
		CalcZeroframeData_PS3( pAnim, pLS_animdesc, pBonejob, NULL, (int *)pBonejob->boneFlags, fFrame, pos, q, boneMask, 1.0f );

		return;
	}

	// sync on boneflags
	SPUmemcpy_Sync( 1<<DMATAG_ANIM );

	// decode frame animation
	if( pLS_Frameanim ) 
	{

		if( s > 0.0f )
		{
			for( i = 0; i < pBonejob->numBones; i++, pBoneFlags++, pweight++ )
			{
				if( *pweight > 0.0f && (pBonejob->boneFlags[i] & boneMask) )
				{
					pEA_FrameData = ExtractTwoFrames_PS3( *pBoneFlags, s, pEA_FrameData, pEA_ConstantData, pLS_Frameanim->framelength, q[i], pos[i], bIsDelta, (mstudiolinearbone_t_PS3 *)pBonejob->pEA_studiohdr_linearBones, i );
				}
				else
				{
					pEA_FrameData = SkipBoneFrame_PS3( *pBoneFlags, pEA_FrameData, pEA_ConstantData );
				}
			}
		}
		else
		{
			for( i = 0; i < pBonejob->numBones; i++, pBoneFlags++, pweight++ )
			{
				if( *pweight > 0.0f && (pBonejob->boneFlags[i] & boneMask) )
				{
					pEA_FrameData = ExtractSingleFrame_PS3( *pBoneFlags, pEA_FrameData, pEA_ConstantData, q[i], pos[i], bIsDelta, (mstudiolinearbone_t_PS3 *)pBonejob->pEA_studiohdr_linearBones, i );
				}
				else
				{
					pEA_FrameData = SkipBoneFrame_PS3( *pBoneFlags, pEA_FrameData, pEA_ConstantData );
				}
			}
		}
	}
	else
	{
		byte ls_buf[ 0x10 * 4 ] ALIGN16;



		// BUGBUG: the sequence, the anim, and the model can have all different bone mappings.
		for( i = 0; i < pBonejob->numBones; i++, pEA_bone++, pweight++ )
		{
			// get panim
			if( pEA_anim )
			{
				pLS_anim = (mstudio_rle_anim_t_PS3 *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_anim, sizeof( mstudio_rle_anim_t_PS3 ) ); 
			}

			if( pEA_anim && pLS_anim->bone == i )
			{
				if( *pweight > 0.0f && (pBonejob->boneFlags[i] & boneMask) )
				{
					CalcBonePositionQuaternion_PS3( pAnim->animdesc_iLocalFrame, s, pEA_bone, pBonejob, (mstudiolinearbone_t_PS3 *)pBonejob->pEA_studiohdr_linearBones, pLS_anim, pEA_anim, pos[i], q[i] );
				}
				pEA_anim = pLS_anim->pNext( pEA_anim );
			}
			else if( *pweight > 0.0f && (pBonejob->boneFlags[i] & boneMask) )
			{
				byte		*pLS_posquat;
				Vector		*pLS_bonepos;
				Quaternion	*pLS_bonequat;

				if( bIsDelta )
				{
					q[i].Init( 0.0f, 0.0f, 0.0f, 1.0f );
					pos[i].Init( 0.0f, 0.0f, 0.0f );
				}
				else
				{
					pLS_posquat = (byte *)SPUmemcpy_UnalignedGet( ls_buf, (uint32)pEA_bone, sizeof(Vector) + sizeof(Quaternion) );

					pLS_bonepos  = (Vector *)pLS_posquat;
					pLS_bonequat = (Quaternion *)(pLS_posquat + sizeof(Vector));

					pos[i] = *((Vector *)(pLS_bonepos));		// pbone->pos
					q[i]   = *((Quaternion *)(pLS_bonequat));	// pbone->quat

					AssertFatal(pos[i].IsValid());
					AssertFatal(q[i].IsValid());
				}
			}
		}
	}

	// cross fade in previous zeroframe data
	if( flStall > 0.0f )
	{
		//CalcZeroframeData_PS3( pStudioHdr, pStudioHdr->GetRenderHdr(), NULL, pStudioHdr->pBone( 0 ), animdesc, fFrame, pos, q, boneMask, flStall );
		CalcZeroframeData_PS3( pAnim, pLS_animdesc, pBonejob, NULL, (int *)pBonejob->boneFlags, fFrame, pos, q, boneMask, 1.0f );
	}

#if 0 // don't take this path on SPU - rare path, do on PPU only
	// calculate a local hierarchy override
	if( animdesc.numlocalhierarchy )
	{
		matrix3x4a_t *boneToWorld = g_matStack[0];
		CBoneBitList_PS3 boneComputed;

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
					CalcLocalHierarchyAnimation_PS3( pBonejob, pStudioHdr, boneToWorld, boneComputed, pos, q, pbone, pHierarchy, pHierarchy->iBone, pHierarchy->iNewParent, cycle, iFrame, s, boneMask );
				}
			}
		}
	}
#endif

	return;
}

