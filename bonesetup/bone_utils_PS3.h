//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BONE_UTILS_PS3_H
#define BONE_UTILS_PS3_H

#ifndef _PS3
#error "This header is for PS3 target only"
#endif

#include "vjobs_interface.h"

#if defined(__SPU__)
#include "ps3/spu_job_shared.h"
#include "cell/dma.h"
#endif

#if 1
#define DotProduct_PS3				DotProduct
#define MatrixAngles_PS3			MatrixAngles
#define VectorRotate_PS3			VectorRotate
#define VectorSubtract_PS3			VectorSubtract
#define MatrixPosition_PS3			MatrixPosition
#define _VMX_VectorNormalize_PS3	_VMX_VectorNormalize
#define VectorNormalize_PS3			VectorNormalize
#define VectorMultiply_PS3			VectorMultiply
#define VectorScale_PS3				VectorScale
#define VectorMAInline_PS3			VectorMAInline
#define VectorMA_PS3				VectorMA
#define SimpleSpline_PS3			SimpleSpline
#define ConcatTransforms_PS3		ConcatTransforms
#define ConcatTransforms_Aligned_PS3	ConcatTransforms_Aligned
//#define QuaternionMatrix_PS3			QuaternionMatrix
//#define QuaternionAlign_PS3			QuaternionAlign
//#define QuaternionSlerp_PS3			QuaternionSlerp
//#define QuaternionSlerpNoAlign_PS3	QuaternionSlerpNoAlign
//#define QuaternionNormalize_PS3		QuaternionNormalize
//#define QuaternionBlend_PS3			QuaternionBlend
//#define QuaternionBlendNoAlign_PS3	QuaternionBlendNoAlign
//#define QuaternionIdentityBlend_PS3	QuaternionIdentityBlend
//#define QuaternionScale_PS3			QuaternionScale
//#define QuaternionAdd_PS3			QuaternionAdd
//#define QuaternionDotProduct_PS3	QuaternionDotProduct
//#define QuaternionMult_PS3			QuaternionMult
#define MatrixSetColumn_PS3			MatrixSetColumn
#define MatrixGetColumn_PS3			MatrixGetColumn
#define MatrixInvert_PS3			MatrixInvert
#define VectorRotate_PS3			VectorRotate
#define AngleMatrix_PS3				AngleMatrix
#define AngleQuaternion_PS3			AngleQuaternion
#define Hermite_Spline_PS3			Hermite_Spline
#define Hermite_SplineBasis_PS3		Hermite_SplineBasis
#define SetIdentityMatrix_PS3		SetIdentityMatrix

FORCEINLINE Vector Lerp_PS3( float flPercent, Vector const &A, Vector const &B )
{
	return A + (B - A) * flPercent;
}
#endif


// from mathlib.h, mathlib_base.cpp
#if 0
FORCEINLINE float DotProduct_PS3(const Vector& a, const Vector& b) 
{ 
	//	return( a.x*b.x + a.y*b.y + a.z*b.z ); 
	return a.Dot(b);
}
FORCEINLINE float DotProduct_PS3(const float* a, const float* b) 
{ 
	return( a[0]*b[0] + a[1]*b[1] + a[2]*b[2] ); 
}
void ConcatTransforms_PS3( const matrix3x4a_t &m0, const matrix3x4a_t &m1, matrix3x4a_t &out );
void ConcatTransforms_Aligned_PS3( const matrix3x4a_t &m0, const matrix3x4a_t &m1, matrix3x4a_t &out );
void MatrixAngles_PS3( const matrix3x4_t & matrix, float *angles ); // !!!!
void MatrixAngles_PS3( const matrix3x4_t& matrix, RadianEuler &angles, Vector &position );
void MatrixAngles_PS3( const matrix3x4_t &matrix, Quaternion &q, Vector &pos );
inline void MatrixAngles_PS3( const matrix3x4_t &matrix, RadianEuler &angles )
{
	MatrixAngles_PS3( matrix, &angles.x );

	angles.Init( DEG2RAD( angles.z ), DEG2RAD( angles.x ), DEG2RAD( angles.y ) );
}
void MatrixGetColumn_PS3( const matrix3x4_t& in, int column, Vector &out );
void MatrixSetColumn_PS3( const Vector &in, int column, matrix3x4_t& out );
void MatrixInvert_PS3( const matrix3x4_t& in, matrix3x4_t& out );
void VectorRotate_PS3( const float *in1, const matrix3x4_t & in2, float *out);
inline void VectorRotate_PS3( const Vector& in1, const matrix3x4_t &in2, Vector &out)
{
	VectorRotate_PS3( &in1.x, in2, &out.x );
}
FORCEINLINE void VectorSubtract_PS3( const Vector& a, const Vector& b, Vector& c )
{
	c.x = a.x - b.x;
	c.y = a.y - b.y;
	c.z = a.z - b.z;
}
inline void MatrixPosition_PS3( const matrix3x4_t &matrix, Vector &position )
{
	position[0] = matrix[0][3];
	position[1] = matrix[1][3];
	position[2] = matrix[2][3];
}
FORCEINLINE float _VMX_VectorNormalize_PS3( Vector &vec )
{
	vec_float4 vIn;
	vec_float4 v0, v1;
	vector unsigned char permMask;
	v0	 = vec_ld( 0, vec.Base() );			
	permMask = vec_lvsl( 0, vec.Base() );	
	v1	 = vec_ld( 11, vec.Base() );			
	vIn  = vec_perm(v0, v1, permMask);
	float mag = vmathV3Length((VmathVector3 *)&vIn);
	float den = 1.f / (mag + FLT_EPSILON );
	vec.x *= den;
	vec.y *= den;
	vec.z *= den;
	return mag;
}
FORCEINLINE float VectorNormalize_PS3( Vector& v )
{
	return _VMX_VectorNormalize_PS3( v );
}


FORCEINLINE void VectorMultiply_PS3( const Vector& a, float b, Vector& c )
{
	c.x = a.x * b;
	c.y = a.y * b;
	c.z = a.z * b;
}

FORCEINLINE void VectorMultiply_PS3( const Vector& a, const Vector& b, Vector& c )
{
	c.x = a.x * b.x;
	c.y = a.y * b.y;
	c.z = a.z * b.z;
}

inline void VectorScale_PS3 ( const Vector& in, float scale, Vector& result )
{
	VectorMultiply_PS3( in, scale, result );
}
FORCEINLINE Vector Lerp_PS3( float flPercent, Vector const &A, Vector const &B )
{
	return A + (B - A) * flPercent;
}
FORCEINLINE void VectorMAInline_PS3( const float* start, float scale, const float* direction, float* dest )
{
	dest[0]=start[0]+direction[0]*scale;
	dest[1]=start[1]+direction[1]*scale;
	dest[2]=start[2]+direction[2]*scale;
}
FORCEINLINE void VectorMAInline_PS3( const Vector& start, float scale, const Vector& direction, Vector& dest )
{
	dest.x=start.x+direction.x*scale;
	dest.y=start.y+direction.y*scale;
	dest.z=start.z+direction.z*scale;
}
FORCEINLINE void VectorMA_PS3( const Vector& start, float scale, const Vector& direction, Vector& dest )
{
	VectorMAInline_PS3(start, scale, direction, dest);
}
FORCEINLINE void VectorMA_PS3( const float * start, float scale, const float *direction, float *dest )
{
	VectorMAInline_PS3(start, scale, direction, dest);
}
void AngleMatrix_PS3( RadianEuler const &angles, const Vector &position, matrix3x4_t& matrix );
void AngleMatrix_PS3( const RadianEuler& angles, matrix3x4_t& matrix );
void AngleMatrix_PS3( const QAngle &angles, const Vector &position, matrix3x4_t& matrix );
void AngleMatrix_PS3( const QAngle &angles, matrix3x4_t& matrix );
void AngleQuaternion_PS3( const RadianEuler &angles, Quaternion &outQuat );
void Hermite_Spline_PS3( const Vector &p1, const Vector &p2, const Vector &d1, const Vector &d2, float t, Vector& output );
float Hermite_Spline_PS3( float p1, float p2, float d1, float d2, float t );
void Hermite_SplineBasis_PS3( float t, float basis[4] );
void Hermite_Spline_PS3( const Vector &p0, const Vector &p1, const Vector &p2, float t, Vector& output );
float Hermite_Spline_PS3( float p0, float p1, float p2,	float t );
void Hermite_Spline_PS3( const Quaternion &q0, const Quaternion &q1, const Quaternion &q2, float t, Quaternion &output );
inline float SimpleSpline_PS3( float value )
{
	float valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return (3.0f * valueSquared - 2.0f * valueSquared * value);
}
#endif


void QuaternionMatrix_PS3( const Quaternion &q, const Vector &pos, matrix3x4a_t& matrix );
void QuaternionAlign_PS3( const Quaternion &p, const Quaternion &q, QuaternionAligned &qt );
void QuaternionSlerp_PS3( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt );
void QuaternionSlerpNoAlign_PS3( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt );
float QuaternionNormalize_PS3( Quaternion &q );
void QuaternionBlend_PS3( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt );
void QuaternionBlendNoAlign_PS3( const Quaternion &p, const Quaternion &q, float t, Quaternion &qt );
void QuaternionIdentityBlend_PS3( const Quaternion &p, float t, Quaternion &qt );
void QuaternionScale_PS3( const Quaternion &p, float t, Quaternion &q );
void QuaternionAdd_PS3( const Quaternion &p, const Quaternion &q, Quaternion &qt );
float QuaternionDotProduct_PS3( const Quaternion &p, const Quaternion &q );
void QuaternionMult_PS3( const Quaternion &p, const Quaternion &q, Quaternion &qt );



void AddDependencies_SPU( bonejob_SPU *pBonejobSPU, accumposeentry_SPU *pPoseEntry, float flWeight = 1.0f );
//void AddDependencies_PPU( CStudioHdr *pHdr, float m_flTime, int m_boneMask, mstudioseqdesc_t &seqdesc, int iSequence, float flCycle, const float poseParameters[], float flWeight );

void GetBoneMapBoneWeight_SPU( bonejob_SPU *pSPUJob, accumposeentry_SPU *pPoseEntry, int *&pLS_boneMap, float *&pLS_boneWeight );

void CalcDecompressedAnimation_PS3( void *pEA_Compressed, const mstudiocompressedikerror_t_PS3 *pLS_Compressed, int iFrame, float fraq, BoneVector &pos, BoneQuaternion &q );
void QuaternionAccumulate_PS3( const Quaternion &p, float s, const Quaternion &q, Quaternion &qt );
void CalcAnimation_PS3( const bonejob_SPU *pBonejob, const accumposeentry_SPU *pPoseEntry, BoneVector *pos, BoneQuaternion *q, const int *boneMap, const float *boneWeight, int animIndex, float cycle, int boneMask );
void BlendBones_PS3( const bonejob_SPU *pBonejob, const accumposeentry_SPU *pPoseEntry, BoneQuaternion *q1, BoneVector *pos1, const int *boneMap, const float *boneWeight, const BoneQuaternion *q2, const BoneVector *pos2, float s, int boneMask );
void ScaleBones_PS3( const bonejob_SPU *pBonejob, const accumposeentry_SPU *pPoseEntry, BoneQuaternion *q1, BoneVector *pos1, const int *boneMap, const	float *boneWeight, float s, int boneMask );

// void CalcPose( const CStudioHdr *pStudioHdr, CIKContext *pIKContext, Vector pos[], BoneQuaternion q[], int sequence, float cycle, const float poseParameter[], int boneMask, float flWeight = 1.0f, float flTime = 0.0f );
//bool CalcPoseSingle( const CStudioHdr *pStudioHdr, Vector pos[], BoneQuaternion q[], mstudioseqdesc_t &seqdesc, int sequence, float cycle, const float poseParameter[], int boneMask, float flTime );

// void CalcBoneAdj( const CStudioHdr *pStudioHdr, Vector pos[], Quaternion q[], const float controllers[], int boneMask );

void BuildBoneChainPartial_PS3(
	const int *pBoneParent,
	const matrix3x4a_t &rootxform,
	const BoneVector pos[], 
	const BoneQuaternion q[], 
	int	iBone,
	matrix3x4a_t *pBoneToWorld,
	CBoneBitList_PS3 &boneComputed,
	int iRoot );

struct PS3BoneJobData;

#define BONEJOB_ERROR_EXCEEDEDPQSTACK	(1<<0)
#define BONEJOB_ERROR_EXCEEDEDMAXCALLS  (1<<1)
#define BONEJOB_ERROR_LOCALHIER			(1<<2)

#if !defined(__SPU__)


class CBoneSetup_PS3
{
public:

	CBoneSetup_PS3( const CStudioHdr *pStudioHdr, int boneMask, const float poseParameter[], bonejob_SPU *pBoneJobSPU );
	
	void CalcAutoplaySequences_AddPoseCalls( float flRealTime );
	void AccumulatePose_AddToBoneJob( bonejob_SPU* pSPUJob, int sequence, float cycle, float flWeight, CIKContext *pIKContext, int pqStackLevel );

	int RunAccumulatePoseJobs_PPU( bonejob_SPU *pBoneJob );
	int RunAccumulatePoseJobs_SPU( bonejob_SPU *pBoneJob, job_accumpose::JobDescriptor_t *pJobDescriptor );

private:

	bool SetAnimData( accumposeentry_SPU *pPoseEntry, const CStudioHdr *pStudioHdr, mstudioseqdesc_t &seqdesc, int sequence, int x, int y, int animIndex, float weight );

	bool CalcPoseSingle( accumposeentry_SPU *pPoseEntry, const CStudioHdr *pStudioHdr, mstudioseqdesc_t &seqdesc, int sequence, float cycle, const float poseParameter[], float flTime );

	int  AddSequenceLayers( bonejob_SPU *pSPUJob, mstudioseqdesc_t &seqdesc, int sequence, float cycle, float flWeight, CIKContext *pIKContext, int pqStackLevel );
	int  AddLocalLayers( bonejob_SPU *pSPUJob, mstudioseqdesc_t &seqdesc, int sequence, float cycle, float flWeight, CIKContext *pIKContext, int pqStackLevel );

public:

	const CStudioHdr	*m_pStudioHdr;
	int					m_boneMask;
	const float			*m_flPoseParameter;

	bonejob_SPU			*m_pBoneJobSPU;

	int					m_errorFlags;		// accpose call failure flags (if so do not run on SPU, could be for a number of reasons, right now => exceeded PQ stack, or an anim uses the local hierarchy path
};


#endif  // #if !defined(__SPU__)



//-------------------------------------------------------------------------------------------------------------
// SPU dummy funcs
//-------------------------------------------------------------------------------------------------------------

#define DMATAG_ANIM_SYNC_BONEMAPWEIGHT	(DMATAG_ANIM+1)
#define DMATAG_ANIM_SYNC_POSQ			(DMATAG_ANIM+2)

FORCEINLINE void *SPUmemcpy_UnalignedGet( void *ls, uint32 ea, uint32_t size )
{
	void *aligned_ls;	

	aligned_ls = (void *)((uint32)ls | (ea & 0xf));	// + 0xf in case ls not 16B aligned

#if defined(__SPU__)
	//spu_printf("GET ls:0x%x, ea:0x%x, size:%d\n", (uint32_t)aligned_ls, ea, size);
	// SPU
	cellDmaUnalignedGet( aligned_ls, ea, size, DMATAG_SYNC, 0, 0 );
	cellDmaWaitTagStatusAny( 1 << DMATAG_SYNC );
#else
	// PPU
	memcpy( aligned_ls, (void *)ea, size );
#endif

	return aligned_ls;
}


FORCEINLINE void *SPUmemcpy_UnalignedGet_MustSync( void *ls, uint32 ea, uint32_t size, uint32_t dmatag )
{
	void *aligned_ls;	

	aligned_ls = (void *)((uint32)ls | (ea & 0xf));	// + 0xf in case ls not 16B aligned

#if defined(__SPU__)
	//spu_printf("GET ls:0x%x, ea:0x%x, size:%d\n", (uint32_t)aligned_ls, ea, size);
	// SPU
	cellDmaUnalignedGet( aligned_ls, ea, size, dmatag, 0, 0 );
#else
	// PPU
	memcpy( aligned_ls, (void *)ea, size );
#endif


	return aligned_ls;
}


FORCEINLINE void SPUmemcpy_Sync( uint32_t dmatag )
{
#if defined(__SPU__)
//	cellDmaWaitTagStatusAll( 1 << dmatag );
	cellDmaWaitTagStatusAll( dmatag );
#endif
}


FORCEINLINE void SPUmemcpy_UnalignedPut( void *ls, uint32 ea, uint32_t size )
{
#if defined(__SPU__)
	//spu_printf("PUT ls:0x%x, ea:0x%x, size:%d\n", (uint32_t)ls, ea, size);
	// SPU
	cellDmaUnalignedPut( ls, ea, size, DMATAG_SYNC, 0, 0 );
	cellDmaWaitTagStatusAny( 1 << DMATAG_SYNC );
#else
	Assert(((uint32)ls&0xf) == ea&0xf);

	// PPU
	memcpy( (void *)ea, ls, size );
#endif
}


//=============================================================================//
//
//
//
//
//
//=============================================================================//
#endif
