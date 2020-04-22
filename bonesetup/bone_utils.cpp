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
#ifdef POSIX
#define _rotl(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
#endif
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
#include "mathlib/capsule.h"

#include "tier0/miniprofiler.h"

#ifdef CLIENT_DLL
	#include "posedebugger.h"
#endif

#include "engine/ivdebugoverlay.h"

#include "bone_utils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// -----------------------------------------------------------------

CBoneSetupMemoryPool<BoneQuaternionAligned> g_QuaternionPool;
CBoneSetupMemoryPool<BoneVector> g_VectorPool;
CBoneSetupMemoryPool<matrix3x4a_t> g_MatrixPool;

// -----------------------------------------------------------------
CBoneCache *CBoneCache::CreateResource( const bonecacheparams_t &params )
{
	BONE_PROFILE_FUNC();
	short studioToCachedIndex[MAXSTUDIOBONES];
	short cachedToStudioIndex[MAXSTUDIOBONES];
	int cachedBoneCount = 0;
	for ( int i = 0; i < params.pStudioHdr->numbones(); i++ )
	{
		// skip bones that aren't part of the boneMask (and aren't the root bone)
		if (i != 0 && !(params.pStudioHdr->boneFlags(i) & params.boneMask))
		{
			studioToCachedIndex[i] = -1;
			continue;
		}
		studioToCachedIndex[i] = cachedBoneCount;
		cachedToStudioIndex[cachedBoneCount] = i;
		cachedBoneCount++;
	}
	int tableSizeStudio = sizeof(short) * params.pStudioHdr->numbones();
	int tableSizeCached = sizeof(short) * cachedBoneCount;
	int matrixSize = sizeof(matrix3x4_t) * cachedBoneCount;
	size_t size = AlignValue( sizeof(CBoneCache) + tableSizeStudio + tableSizeCached, 16 ) + matrixSize;
	
	CBoneCache *pMem = (CBoneCache *)MemAlloc_AllocAligned( size, 16 );
	Construct( pMem );
	Assert( size == ( uint )size ); // make sure we're not trimming the int in 64bit
	pMem->Init( params, size, studioToCachedIndex, cachedToStudioIndex, cachedBoneCount );
	return pMem;
}

unsigned int CBoneCache::EstimatedSize( const bonecacheparams_t &params )
{
	// conservative estimate - max size
	return ( params.pStudioHdr->numbones() * (sizeof(short) + sizeof(short) + sizeof(matrix3x4_t)) + 3 ) & ~3;
}

void CBoneCache::DestroyResource()
{
	MemAlloc_FreeAligned( this );
}


CBoneCache::CBoneCache()
{
	m_size = 0;
	m_cachedBoneCount = 0;
}

void CBoneCache::Init( const bonecacheparams_t &params, unsigned int size, short *pStudioToCached, short *pCachedToStudio, int cachedBoneCount ) 
{
	BONE_PROFILE_FUNC();
	m_cachedBoneCount = cachedBoneCount;
	m_size = size;
	m_timeValid = params.curtime;
	m_boneMask = params.boneMask;

	int studioTableSize = params.pStudioHdr->numbones() * sizeof(short);
	m_cachedToStudioOffset = studioTableSize;
	memcpy( StudioToCached(), pStudioToCached, studioTableSize );

	int cachedTableSize = cachedBoneCount * sizeof(short);
	memcpy( CachedToStudio(), pCachedToStudio, cachedTableSize );

	m_matrixOffset = AlignValue( sizeof(CBoneCache) + m_cachedToStudioOffset + cachedTableSize, 16 );
	
	UpdateBones( params.pBoneToWorld, params.pStudioHdr->numbones(), params.curtime );
}

void CBoneCache::UpdateBones( const matrix3x4a_t *pBoneToWorld, int numbones, float curtime )
{
	BONE_PROFILE_FUNC();
	matrix3x4a_t *pBones = BoneArray();
	const short *pCachedToStudio = CachedToStudio();

	for ( int i = 0; i < m_cachedBoneCount; i++ )
	{
		int index = pCachedToStudio[i];
		//MatrixCopy( pBoneToWorld[index], pBones[i] );

		const float *pInput = pBoneToWorld[index].Base();
		float *pOutput = pBones[i].Base();

		fltx4 fl4Tmp0 = LoadAlignedSIMD( pInput );
		StoreAlignedSIMD( pOutput, fl4Tmp0 );
		fltx4 fl4Tmp1 = LoadAlignedSIMD( pInput + 4 );
		StoreAlignedSIMD( pOutput+4, fl4Tmp1 );
		fltx4 fl4Tmp2 = LoadAlignedSIMD( pInput + 8 );
		StoreAlignedSIMD( pOutput+8, fl4Tmp2 );
	}
	m_timeValid = curtime;
}

matrix3x4a_t *CBoneCache::GetCachedBone( int studioIndex )
{
	BONE_PROFILE_FUNC();
	int cachedIndex = StudioToCached()[studioIndex];
	if ( cachedIndex >= 0 )
	{
		return BoneArray() + cachedIndex;
	}
	return NULL;
}

void CBoneCache::ReadCachedBones( matrix3x4a_t *pBoneToWorld )
{
	BONE_PROFILE_FUNC();
	matrix3x4a_t *pBones = BoneArray();
	const short *pCachedToStudio = CachedToStudio();
	for ( int i = 0; i < m_cachedBoneCount; i++ )
	{
		//MatrixCopy( pBones[i], pBoneToWorld[pCachedToStudio[i]] );

		const float *pInput = pBones[i].Base();
		float *pOutput = pBoneToWorld[pCachedToStudio[i]].Base();
		fltx4 fl4Tmp0 = LoadAlignedSIMD( pInput );
		StoreAlignedSIMD( pOutput, fl4Tmp0 );
		fltx4 fl4Tmp1 = LoadAlignedSIMD( pInput + 4 );
		StoreAlignedSIMD( pOutput+4, fl4Tmp1 );
		fltx4 fl4Tmp2 = LoadAlignedSIMD( pInput + 8 );
		StoreAlignedSIMD( pOutput+8, fl4Tmp2 );
	}
}

void CBoneCache::ReadCachedBonePointers( matrix3x4_t **bones, int numbones )
{
	BONE_PROFILE_FUNC();
	memset( bones, 0, sizeof(matrix3x4_t *) * numbones );
	matrix3x4a_t *pBones = BoneArray();
	const short *pCachedToStudio = CachedToStudio();
	for ( int i = 0; i < m_cachedBoneCount; i++ )
	{
		bones[pCachedToStudio[i]] = pBones + i;
	}
}

bool CBoneCache::IsValid( float curtime, float dt )
{
	if ( curtime - m_timeValid <= dt )
		return true;
	return false;
}


// private functions
matrix3x4a_t *CBoneCache::BoneArray()
{
	return (matrix3x4a_t *)( (byte *)(this) + m_matrixOffset );
}

short *CBoneCache::StudioToCached()
{
	return (short *)( (char *)(this+1) );
}

short *CBoneCache::CachedToStudio()
{
	return (short *)( (char *)(this+1) + m_cachedToStudioOffset );
}

// Construct a singleton
static CDataManager<CBoneCache, bonecacheparams_t, CBoneCache *, CThreadFastMutex> g_StudioBoneCache( 128 * 1024L );

void Studio_LockBoneCache()
{
	g_StudioBoneCache.AccessMutex().Lock();
}

void Studio_UnlockBoneCache()
{
	g_StudioBoneCache.AccessMutex().Unlock();
}

CBoneCache *Studio_GetBoneCache( memhandle_t cacheHandle, bool bLock )
{
	AUTO_LOCK( g_StudioBoneCache.AccessMutex() );
	if ( !bLock )
	{
		return g_StudioBoneCache.GetResource_NoLock( cacheHandle );
	}
	else
	{
		return g_StudioBoneCache.LockResource( cacheHandle );
	}
}

void Studio_ReleaseBoneCache( memhandle_t cacheHandle )
{
	g_StudioBoneCache.UnlockResource( cacheHandle );
	g_StudioBoneCache.FlushToTargetSize();
}

memhandle_t Studio_CreateBoneCache( bonecacheparams_t &params )
{
	AUTO_LOCK( g_StudioBoneCache.AccessMutex() );
	return g_StudioBoneCache.CreateResource( params );
}

void Studio_DestroyBoneCache( memhandle_t cacheHandle )
{
	AUTO_LOCK( g_StudioBoneCache.AccessMutex() );
	g_StudioBoneCache.DestroyResource( cacheHandle );
}

void Studio_InvalidateBoneCacheIfNotMatching( memhandle_t cacheHandle, float flTimeValid )
{
	AUTO_LOCK( g_StudioBoneCache.AccessMutex() );
	CBoneCache *pCache = g_StudioBoneCache.GetResource_NoLock( cacheHandle );
	if ( pCache && pCache->m_timeValid != flTimeValid )
	{
		pCache->m_timeValid = -1.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------

void BuildBoneChain(
	const CStudioHdr *pStudioHdr,
	const matrix3x4a_t &rootxform,
	const BoneVector pos[], 
	const BoneQuaternion q[], 
	int	iBone,
	matrix3x4a_t *pBoneToWorld )
{
	CBoneBitList boneComputed;
	BuildBoneChainPartial( pStudioHdr, rootxform, pos, q, iBone, pBoneToWorld, boneComputed, -1 );
	return;
}



//-----------------------------------------------------------------------------
// Purpose: build boneToWorld transforms for a specific bone
//-----------------------------------------------------------------------------
void BuildBoneChain(
	const CStudioHdr *pStudioHdr,
	const matrix3x4a_t &rootxform,
	const BoneVector pos[], 
	const BoneQuaternion q[], 
	int	iBone,
	matrix3x4a_t *pBoneToWorld,
	CBoneBitList &boneComputed )
{
	BuildBoneChainPartial( pStudioHdr, rootxform, pos, q, iBone, pBoneToWorld, boneComputed, -1 );
}


void BuildBoneChainPartial(
	const CStudioHdr *pStudioHdr,
	const matrix3x4_t &rootxform,
	const BoneVector pos[], 
	const BoneQuaternion q[], 
	int	iBone,
	matrix3x4_t *pBoneToWorld,
	CBoneBitList &boneComputed,
	int iRoot )
{
	if ( boneComputed.IsBoneMarked(iBone) )
		return;

	matrix3x4_t bonematrix;
	QuaternionMatrix( q[iBone], pos[iBone], bonematrix );

	int parent = pStudioHdr->boneParent( iBone );
	if (parent == -1 || iBone == iRoot) 
	{
		ConcatTransforms( rootxform, bonematrix, pBoneToWorld[iBone] );
	}
	else
	{
		// evil recursive!!!
		BuildBoneChainPartial( pStudioHdr, rootxform, pos, q, parent, pBoneToWorld, boneComputed, iRoot );
		ConcatTransforms( pBoneToWorld[parent], bonematrix, pBoneToWorld[iBone]);
	}

	boneComputed.MarkBone(iBone);
}


//-----------------------------------------------------------------------------
// Purpose: qt = ( s * p ) * q
//-----------------------------------------------------------------------------
void QuaternionSM( float s, const Quaternion &p, const Quaternion &q, Quaternion &qt )
{
	Quaternion		p1, q1;

	QuaternionScale( p, s, p1 );
	QuaternionMult( p1, q, q1 );
	QuaternionNormalize( q1 );
	qt[0] = q1[0];
	qt[1] = q1[1];
	qt[2] = q1[2];
	qt[3] = q1[3];
}

#if ALLOW_SIMD_QUATERNION_MATH
FORCEINLINE fltx4 QuaternionSMSIMD( const fltx4 &s, const fltx4 &p, const fltx4 &q )
{
	fltx4 p1, q1, result;
	p1 = QuaternionScaleSIMD( p, s );
	q1 = QuaternionMultSIMD( p1, q );
	result = QuaternionNormalizeSIMD( q1 );
	return result;
}

FORCEINLINE fltx4 QuaternionSMSIMD( float s, const fltx4 &p, const fltx4 &q )
{
	return QuaternionSMSIMD( ReplicateX4(s), p, q );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: qt = p * ( s * q )
//-----------------------------------------------------------------------------
void QuaternionMA( const Quaternion &p, float s, const Quaternion &q, Quaternion &qt )
{
	Quaternion p1, q1;

	QuaternionScale( q, s, q1 );
	QuaternionMult( p, q1, p1 );
	QuaternionNormalize( p1 );
	qt[0] = p1[0];
	qt[1] = p1[1];
	qt[2] = p1[2];
	qt[3] = p1[3];
}

#if ALLOW_SIMD_QUATERNION_MATH

FORCEINLINE fltx4 QuaternionMASIMD( const fltx4 &p, const fltx4 &s, const fltx4 &q )
{
	fltx4 p1, q1, result;
	q1 = QuaternionScaleSIMD( q, s );
	p1 = QuaternionMultSIMD( p, q1 );
	result = QuaternionNormalizeSIMD( p1 );
	return result;
}

FORCEINLINE fltx4 QuaternionMASIMD( const fltx4 &p, float s, const fltx4 &q )
{
	return QuaternionMASIMD(p, ReplicateX4(s), q);
}
#endif


//-----------------------------------------------------------------------------
// Purpose: qt = p + s * q
//-----------------------------------------------------------------------------
void QuaternionAccumulate( const Quaternion &p, float s, const Quaternion &q, Quaternion &qt )
{
	Quaternion q2;
	QuaternionAlign( p, q, q2 );

	qt[0] = p[0] + s * q2[0];
	qt[1] = p[1] + s * q2[1];
	qt[2] = p[2] + s * q2[2];
	qt[3] = p[3] + s * q2[3];
}

#if ALLOW_SIMD_QUATERNION_MATH
FORCEINLINE fltx4 QuaternionAccumulateSIMD( const fltx4 &p, float s, const fltx4 &q )
{
	fltx4 q2, s4, result;
	q2 = QuaternionAlignSIMD( p, q );
	s4 = ReplicateX4( s );
	result = MaddSIMD( s4, q2, p );
	return result;
}
#endif



//-----------------------------------------------------------------------------
// Purpose: blend together in world space q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------

void WorldSpaceSlerp(
	const CStudioHdr *pStudioHdr,
	BoneQuaternion q1[MAXSTUDIOBONES], 
	BoneVector pos1[MAXSTUDIOBONES], 
	mstudioseqdesc_t &seqdesc, 
	int sequence, 
	const BoneQuaternion q2[MAXSTUDIOBONES], 
	const BoneVector pos2[MAXSTUDIOBONES], 
	float s,
	int boneMask )
{
	BONE_PROFILE_FUNC();
	int			i, j;
	float		s1; // weight of parent for q2, pos2
	float		s2; // weight for q2, pos2

	// make fake root transform
	matrix3x4a_t rootXform;
	SetIdentityMatrix( rootXform );

	// matrices for q2, pos2
	matrix3x4a_t *srcBoneToWorld = g_MatrixPool.Alloc();
	CBoneBitList srcBoneComputed;

	matrix3x4a_t *destBoneToWorld = g_MatrixPool.Alloc();
	CBoneBitList destBoneComputed;

	matrix3x4a_t *targetBoneToWorld = g_MatrixPool.Alloc();
	CBoneBitList targetBoneComputed;

	virtualmodel_t *pVModel = pStudioHdr->GetVirtualModel();
	const virtualgroup_t *pSeqGroup = NULL;
	if (pVModel)
	{
		pSeqGroup = pVModel->pSeqGroup( sequence );
	}

	const mstudiobone_t *pbone = pStudioHdr->pBone( 0 );

	for (i = 0; i < pStudioHdr->numbones(); i++)
	{
		// skip unused bones
		if (!(pStudioHdr->boneFlags(i) & boneMask))
		{
			continue;
		}

		int n = pbone[i].parent;
		s1 = 0.0;
		if (pSeqGroup)
		{
			j = pSeqGroup->boneMap[i];
			if (j >= 0)
			{
				s2 = s * seqdesc.weight( j );	// blend in based on this bones weight
				if (n != -1)
				{
					s1 = s * seqdesc.weight( pSeqGroup->boneMap[n] );
				}
			}
			else
			{
				s2 = 0.0;
			}
		}
		else
		{
			s2 = s * seqdesc.weight( i );	// blend in based on this bones weight
			if (n != -1)
			{
				s1 = s * seqdesc.weight( n );
			}
		}

		if ( s2 > 0.0 || s1 > 0.0 )
		{
			Quaternion srcQ, destQ;
			Vector srcPos, destPos;
			Quaternion targetQ;
			Vector targetPos;
			Vector tmp;

			BuildBoneChain( pStudioHdr, rootXform, pos1, q1, i, destBoneToWorld, destBoneComputed );
			BuildBoneChain( pStudioHdr, rootXform, pos2, q2, i, srcBoneToWorld, srcBoneComputed );

			MatrixAngles( destBoneToWorld[i], destQ, destPos );
			MatrixAngles( srcBoneToWorld[i], srcQ, srcPos );

			QuaternionSlerp( destQ, srcQ, s2, targetQ );
			AngleMatrix( RadianEuler(targetQ), destPos, targetBoneToWorld[i] );

			// back solve
			if (n == -1)
			{
				MatrixAngles( targetBoneToWorld[i], q1[i], tmp );
			}
			else
			{
				matrix3x4a_t worldToBone;
				MatrixInvert( targetBoneToWorld[n], worldToBone );

				matrix3x4a_t local;
				ConcatTransforms_Aligned( worldToBone, targetBoneToWorld[i], local );
				MatrixAngles( local, q1[i], tmp );

				// blend bone lengths (local space)
				//pos1[i] = Lerp( s2, pos1[i], pos2[i] );
				pos1[i] = pos1[i] + (pos2[i] - pos1[i]) * s2;
			}
		}
	}
	g_MatrixPool.Free( srcBoneToWorld );
	g_MatrixPool.Free( destBoneToWorld );
	g_MatrixPool.Free( targetBoneToWorld );
}

#define PARANOID_SIMD_DOUBLECHECK 0  // set this to one to perform both SIMD and scalar bones every frame,
									 // then compare the results.
#define PARANOID_SIMD_TIMING_TEST 0 // enable to allow running many iterations of SlerpBones per frame
									// for timing purposes

#ifdef _X360
// SIMD bone setup is a perf win on 360
static ConVar cl_simdbones( "cl_simdbones", "1", FCVAR_REPLICATED, "Use SIMD bone setup." );
#else
// SIMD bone setup is a perf loss on the PC
static ConVar cl_simdbones( "cl_simdbones", "0", FCVAR_REPLICATED, "Use SIMD bone setup." );
#endif
void SlerpBonesSpeedy( 
					  const CStudioHdr *pStudioHdr,
					  BoneQuaternionAligned q1[MAXSTUDIOBONES], 
					  BoneVector pos1[MAXSTUDIOBONES], 
					  mstudioseqdesc_t &seqdesc,  // source of q2 and pos2
					  int sequence, 
					  const BoneQuaternionAligned q2[MAXSTUDIOBONES], 
					  const BoneVector pos2[MAXSTUDIOBONES], 
					  float s,
					  int boneMask );
volatile int iForBreakpoint;

//-----------------------------------------------------------------------------
// Purpose: blend together q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------
#if PARANOID_SIMD_TIMING_TEST
static ConVar cl_bones_simd_timing_version( "cl_bones_simd_timing_version", "0", FCVAR_REPLICATED, "0 = scalar version, 1 = simd version." );
void SlerpBonesSlow( 
#else
void SlerpBones( 
#endif
	const CStudioHdr *pStudioHdr,
	BoneQuaternion * RESTRICT q1, 
	BoneVector * RESTRICT pos1, 
	mstudioseqdesc_t &seqdesc,  // source of q2 and pos2
	int sequence, 
	const BoneQuaternionAligned * RESTRICT q2, // [MAXSTUDIOBONES], 
	const BoneVector * RESTRICT pos2, // [MAXSTUDIOBONES], 
	float s,
	int boneMask )
{
	BONE_PROFILE_FUNC();
	SNPROF_ANIM("SlerpBones");
#if PARANOID_SIMD_DOUBLECHECK
	// copy off the input arrays so we can do them twice
	static CThreadFastMutex m_mutex;
	AUTO_LOCK( m_mutex );
	static BoneQuaternionAligned doublecheckQuat[MAXSTUDIOBONES];
	static BoneQuaternionAligned doublecheckOriginalQuat[MAXSTUDIOBONES];
	static BoneVector doublecheckPos[MAXSTUDIOBONES];
	static BoneVector doublecheckOriginalPos[MAXSTUDIOBONES];
#if ( PARANOID_SIMD_DOUBLECHECK == 2 )
	BoneVector *originalPosPointer = pos1;
	BoneQuaternion *originalQuatPointer = q1;
#endif
	{
		memcpy( doublecheckQuat, q1, MAXSTUDIOBONES * sizeof(BoneQuaternionAligned) );
		memcpy( doublecheckOriginalQuat, q1, MAXSTUDIOBONES * sizeof(BoneQuaternionAligned) );
		memcpy( doublecheckPos, pos1, MAXSTUDIOBONES * sizeof(BoneVector) );
		memcpy( doublecheckOriginalPos, pos1, MAXSTUDIOBONES * sizeof(BoneVector) );
	}
#endif

	// Test for 16-byte alignment, and if present, use the speedy SIMD version.
	if ( (reinterpret_cast<uintp>(q1) & 0x0F) == 0 && 
		 (reinterpret_cast<uintp>(q2) & 0x0F) == 0 )
	{
		// Msg("Aligned\n");
		if ( cl_simdbones.GetBool() 
#if PARANOID_SIMD_TIMING_TEST
				&& (cl_bones_simd_timing_version.GetInt() != 0)
#endif
			)
		{

#if ( PARANOID_SIMD_DOUBLECHECK == 1 ) // do simd into sep array, scalar into original, then compare
			// if double checking, write to static arrays
			// then do things the ordinary way 
			// then check up at the end.
			SlerpBonesSpeedy(pStudioHdr,
				reinterpret_cast<BoneQuaternionAligned *>(doublecheckQuat),
				doublecheckPos,
				seqdesc,
				sequence,
				q2,
				pos2,
				s,
				boneMask
				);
#elif ( PARANOID_SIMD_DOUBLECHECK == 2 )
			// if double checking, write to static arrays
			// then do things the ordinary way 
			// then check up at the end.
			SlerpBonesSpeedy(pStudioHdr,
				reinterpret_cast<BoneQuaternionAligned *>(q1),
				pos1,
				seqdesc,
				sequence,
				q2,
				pos2,
				s,
				boneMask
				);
			pos1 = doublecheckPos;
			q1 = doublecheckQuat;
#else
			return SlerpBonesSpeedy(pStudioHdr,
				reinterpret_cast<BoneQuaternionAligned *>(q1),
				pos1,
				seqdesc,
				sequence,
				q2,
				pos2,
				s,
				boneMask
				);
#endif
		}
	}
	else
	{
		// Msg("misaligned\n");
	}

	if (s <= 0.0f) 
		return;
	if (s > 1.0f)
	{
		s = 1.0f;		
	}

	if ( (seqdesc.flags & STUDIO_WORLD) || (seqdesc.flags & STUDIO_WORLD_AND_RELATIVE) )
	{
		WorldSpaceSlerp( pStudioHdr, q1, pos1, seqdesc, sequence, q2, pos2, s, boneMask );
		
		if (seqdesc.flags & STUDIO_WORLD)
			return;
	}

	int			i, j;
	virtualmodel_t *pVModel = pStudioHdr->GetVirtualModel();
	const virtualgroup_t *pSeqGroup = NULL;
	if (pVModel)
	{
		pSeqGroup = pVModel->pSeqGroup( sequence );
	}

	// Build weightlist for all bones
	int nBoneCount = pStudioHdr->numbones();
	float *pS2 = (float*)stackalloc( nBoneCount * sizeof(float) );
	for (i = 0; i < nBoneCount; i++)
	{
		// skip unused bones
		if (!(pStudioHdr->boneFlags(i) & boneMask))
		{
			pS2[i] = 0.0f;
			continue;
		}

		if ( !pSeqGroup )
		{
			pS2[i] = s * seqdesc.weight( i );	// blend in based on this bones weight
			continue;
		}

		j = pSeqGroup->boneMap[i];
		if ( j >= 0 )
		{
			pS2[i] = s * seqdesc.weight( j );	// blend in based on this bones weight
		}
		else
		{
			pS2[i] = 0.0;
		}
	}

	float s1, s2;
	if ( seqdesc.flags & STUDIO_DELTA )
	{
		for ( i = 0; i < nBoneCount; i++ )
		{
			s2 = pS2[i];
			if ( s2 <= 0.0f )
				continue;

			if ( seqdesc.flags & STUDIO_POST )
			{
#ifndef _X360
				QuaternionMA( q1[i], s2, q2[i], q1[i] );
#else
				fltx4 q1simd = LoadUnalignedSIMD( q1[i].Base() );
				fltx4 q2simd = LoadAlignedSIMD( q2[i] );
				fltx4 result = QuaternionMASIMD( q1simd, s2, q2simd );
				StoreUnalignedSIMD( q1[i].Base(), result );
#endif
			}
			else
			{
#ifndef _X360
				QuaternionSM( s2, q2[i], q1[i], q1[i] );
#else
				fltx4 q1simd = LoadUnalignedSIMD( q1[i].Base() );
				fltx4 q2simd = LoadAlignedSIMD( q2[i] );
				fltx4 result = QuaternionSMSIMD( s2, q2simd, q1simd );
				StoreUnalignedSIMD( q1[i].Base(), result );
#endif

			}
			// do this explicitly to make the scheduling better
			// (otherwise it might think pos1 and pos2 overlap,
			// and thus save one before starting the next)
			float x,y,z;
			x = pos1[i][0] + pos2[i][0] * s2;
			y = pos1[i][1] + pos2[i][1] * s2;
			z = pos1[i][2] + pos2[i][2] * s2;
			pos1[i][0] = x;
			pos1[i][1] = y;
			pos1[i][2] = z;
		}
		return;
	}

	BoneQuaternionAligned q3;
	for (i = 0; i < nBoneCount; i++)
	{
		s2 = pS2[i];
		if ( s2 <= 0.0f )
			continue;

		s1 = 1.0 - s2;

#ifdef _X360
		fltx4  q1simd, q2simd, result;
		q1simd = LoadUnalignedSIMD( q1[i].Base() );
		q2simd = LoadAlignedSIMD( q2[i] );
#endif
		if ( pStudioHdr->boneFlags(i) & BONE_FIXED_ALIGNMENT )
		{
#ifndef _X360
			QuaternionSlerpNoAlign( q2[i], q1[i], s1, q3 );
#else
			result = QuaternionSlerpNoAlignSIMD( q2simd, q1simd, s1 );
#endif
		}
		else
		{
#ifndef _X360
			QuaternionSlerp( q2[i], q1[i], s1, q3 );
#else
			result = QuaternionSlerpSIMD( q2simd, q1simd, s1 );
#endif
		}

#ifndef _X360
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
#else
		StoreUnalignedSIMD( q1[i].Base(), result );
#endif

		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s2;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s2;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s2;
	}

#if PARANOID_SIMD_DOUBLECHECK
	// check everything
	if (cl_simdbones.GetBool())
	{
	#if ( PARANOID_SIMD_DOUBLECHECK == 2)
		pos1 = originalPosPointer ;
		q1 = originalQuatPointer  ;
	#endif
		for (i = 0 ; i < nBoneCount ; ++i)
		{
			static volatile int PARANOID_II = i;
			if ( pS2[i] <= 0.0f )
			{
				// these aren't used, but test them to make sure they haven't been overwritten.
				// it's important that the garbage there remain garbage, for some reason.
				const unsigned int *ORIGINAL_Q1, *SCALAR_Q1, *SIMD_Q1, *Q2;
	#if ( PARANOID_SIMD_DOUBLECHECK == 2 )
				SCALAR_Q1 = reinterpret_cast<const unsigned int *>(doublecheckQuat[i].Base());
				SIMD_Q1 = reinterpret_cast<const unsigned int *>(q1[i].Base());
	#else
				SIMD_Q1 = reinterpret_cast<const unsigned int *>(doublecheckQuat[i].Base());
				SCALAR_Q1 = reinterpret_cast<const unsigned int *>(q1[i].Base());
	#endif
				ORIGINAL_Q1 = reinterpret_cast<const unsigned int *>(doublecheckOriginalQuat[i].Base());
				Q2 = reinterpret_cast<const unsigned int *>(q2[i].Base());
				if(!( SIMD_Q1[0] == SCALAR_Q1[0] && 
						SIMD_Q1[1] == SCALAR_Q1[1] && 
						SIMD_Q1[2] == SCALAR_Q1[2] &&
						SIMD_Q1[3] == SCALAR_Q1[3] ))
				{
					AssertMsg(false,"Wrote invalid quats\n");
					++iForBreakpoint;
				}


				const unsigned int *ORIGINAL_V1, *SCALAR_V1, *SIMD_V1, *V2;
	#if ( PARANOID_SIMD_DOUBLECHECK == 2 )
				SCALAR_V1 = reinterpret_cast<const unsigned int *>(doublecheckPos[i].Base());
				SIMD_V1 = reinterpret_cast<const unsigned int *>(pos1[i].Base());
	#else
				SIMD_V1 = reinterpret_cast<const unsigned int *>(doublecheckPos[i].Base());
				SCALAR_V1 = reinterpret_cast<const unsigned int *>(pos1[i].Base());
	#endif
				ORIGINAL_V1 =  reinterpret_cast<const unsigned int *>(doublecheckOriginalPos[i].Base());
				V2 =  reinterpret_cast<const unsigned int *>(pos2[i].Base());
				if(!( SIMD_V1[0] == SCALAR_V1[0] && 
					SIMD_V1[1] == SCALAR_V1[1] && 
					SIMD_V1[2] == SCALAR_V1[2] ))
				{
					AssertMsg(false,"Wrote invalid pos\n");
					++iForBreakpoint;
				}

			}
			else
			{
				// test quaternions, unless they were slerped from opposite directions
				if ( !(QuaternionDotProduct(doublecheckQuat[i], q1[i])  > 0.99f) &&
					 !(QuaternionDotProduct(doublecheckQuat[i], q1[i]) < -0.99f)  )
				{
					BoneQuaternionAligned ORIGINAL_Q1, SCALAR_Q1, SIMD_Q1, Q2;
	#if ( PARANOID_SIMD_DOUBLECHECK == 2 )
					SCALAR_Q1 = doublecheckQuat[i];
					SIMD_Q1 = q1[i];
	#else
					SIMD_Q1 = doublecheckQuat[i];
					SCALAR_Q1 = q1[i];
	#endif
					ORIGINAL_Q1 = doublecheckOriginalQuat[i];
					Q2 = q2[i];

					AssertMsg( false, "SIMD and scalar SlerpBones quats do not match up.\n" );
				}

				// test positions, unless they were slerped from opposite directions
				BoneVector posDiff;
				posDiff = pos1[i] - doublecheckPos[i];
				if ( !posDiff.IsZero() )
				{
					BoneVector ORIGINAL_V1, SCALAR_V1, SIMD_V1, V2;
	#if ( PARANOID_SIMD_DOUBLECHECK == 2 )
					SCALAR_V1 = doublecheckPos[i];
					SIMD_V1 = pos1[i];
	#else
					SIMD_V1 = doublecheckPos[i];
					SCALAR_V1 = pos1[i];
	#endif
					ORIGINAL_V1 = doublecheckOriginalPos[i];
					V2 = pos2[i];

					AssertMsg( false, "SIMD and scalar SlerpBones pos do not match up.\n" );
				}
			}
		}
		// compare the slack space in the array -- did we overwrite unused bones?
		for ( i ; i < MAXSTUDIOBONES ; ++ i)
		{
			if ( memcmp(pos1+i, doublecheckOriginalPos+i, sizeof(BoneVector)) != 0)
			{
				AssertMsg(false, "slack positions overwritten\n");
				++iForBreakpoint;
			}
			if ( memcmp(q1+i, doublecheckOriginalQuat+i, sizeof(BoneVector)) != 0)
			{
				AssertMsg(false, "slack quats overwritten\n");
				++iForBreakpoint;
			}
		}

	#if ( PARANOID_SIMD_DOUBLECHECK == 1 )
		// dupe SIMD version back over, becaus ewe wrote it into this other array
		memcpy(q1, doublecheckQuat, nBoneCount * sizeof(BoneQuaternionAligned) );
		memcpy(pos1, doublecheckPos, nBoneCount * sizeof(BoneVector) );
	#elif ( PARANOID_SIMD_DOUBLECHECK == 2 )
		memcpy(pos1, doublecheckPos, nBoneCount * sizeof(BoneVector) );
	#endif
	}
#endif
}


ConVar cl_use_simd_bones( "cl_use_simd_bones", "1", FCVAR_REPLICATED, "1 use SIMD bones 0 use scalar bones." );
//-----------------------------------------------------------------------------
// Purpose: blend together q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			Uses four-at-a-time SIMD.
//-----------------------------------------------------------------------------
void SlerpBonesSpeedy( 
				const CStudioHdr * RESTRICT pStudioHdr,
				BoneQuaternionAligned q1[MAXSTUDIOBONES], 
				BoneVector pos1[MAXSTUDIOBONES], 
				mstudioseqdesc_t &seqdesc,  // source of q2 and pos2
				int sequence, 
				const BoneQuaternionAligned q2[MAXSTUDIOBONES], 
				const BoneVector pos2[MAXSTUDIOBONES], 
				float s,
				int boneMask )
{
	BONE_PROFILE_FUNC(); // ex: x360: 1.2ms
	// Assert 16-byte alignment of in and out arrays.
	AssertMsg( 
		((reinterpret_cast<uintp>(q1)   & 0x0F)==0) &&
		((reinterpret_cast<uintp>(q2)   & 0x0F)==0) ,
		"Input arrays to SlerpBones are not aligned! Catastrophe is inevitable.\n"); 

	// Test for overlapping buffers
#if PARANOID_SIMD_DOUBLECHECK
	{
		int nBoneCount = pStudioHdr->numbones();

		int qbot = reinterpret_cast<int>(q1);
		int qtop = reinterpret_cast<int>(q1 + nBoneCount);

		int pbot = reinterpret_cast<int>(pos1);
		int ptop = reinterpret_cast<int>(pos1 + nBoneCount);

		if ( ((pbot >= qbot) && (pbot <= qtop)) ||
			 ((ptop >= qbot) && (ptop <= qtop)) ||
			 ((qbot >= pbot) && (qbot <= ptop)) ||
			 ((qtop >= pbot) && (qtop <= ptop)) )
		{
			DebuggerBreak();
		}
	}
#endif

	if (s <= 0.0f) 
		return;
	if (s > 1.0f)
	{
		s = 1.0f;		
	}

	if ( (seqdesc.flags & STUDIO_WORLD) || (seqdesc.flags & STUDIO_WORLD_AND_RELATIVE) )
	{
		WorldSpaceSlerp( pStudioHdr, q1, pos1, seqdesc, sequence, q2, pos2, s, boneMask );
		
		if (seqdesc.flags & STUDIO_WORLD)
			return;
	}

	// haul the input arrays into cache if they're not there already
	PREFETCH360(q1,0);
	PREFETCH360(pos1,0);
	PREFETCH360(q2,0);
	PREFETCH360(pos2,0);

	int			i;
	virtualmodel_t *pVModel = pStudioHdr->GetVirtualModel();
	const virtualgroup_t * RESTRICT pSeqGroup = NULL;
	if (pVModel)
	{
		pSeqGroup = pVModel->pSeqGroup( sequence );
	}

	// Build weightlist for all bones
	int nBoneCount = pStudioHdr->numbones();
	float * RESTRICT pS2 = (float*)stackalloc( nBoneCount * sizeof(float) ); // 16-byte aligned

	if ( pSeqGroup ) // hoist this branch outside of the inner loop for speed (even correctly predicted branches are an eight cycle latency)
	{
		for (i = 0; i < nBoneCount; i++)
		{
			// skip unused bones
			if (!(pStudioHdr->boneFlags(i) & boneMask) ||
				pSeqGroup->boneMap[i] < 0 )
			{
				pS2[i] = 0.0f;
			}
			else
			{
				// boneMap[i] is not a float, don't be lured by the siren call of fcmp
				pS2[i] = s * seqdesc.weight( pSeqGroup->boneMap[i] );
			}
		}
	} 
	else // !pSeqGroup
	{
		for (i = 0; i < nBoneCount; i++)
		{
			// skip unused bones
			if (!(pStudioHdr->boneFlags(i) & boneMask))
			{
				pS2[i] = 0.0f;
			}
			else
			{
				pS2[i] = s * seqdesc.weight( i );	// blend in based on this bones weight
			}
		}
	}

	float weight;
	int nBoneCountRoundedFour = ( nBoneCount ) & (~(3));
	if ( seqdesc.flags & STUDIO_DELTA )
	{
		// do as many as we can four at a time, then take care of stragglers.
		for ( i = 0; i < nBoneCountRoundedFour; i+=4 )
		{
			// drag the next cache line in
			PREFETCH360(q1,i*16 + 128);
			PREFETCH360(pos1,i*16 + 128);
			PREFETCH360(q2,i*16 + 128);
			PREFETCH360(pos2,i*16 + 128);

			fltx4 weightfour = LoadAlignedSIMD(pS2+i); // four weights

			FourQuaternions q1four, q2four;
			FourQuaternions result;

			q1four.LoadAndSwizzleAligned(q1+i); // four quaternions
			q2four.LoadAndSwizzleAligned(q2+i); // four quaternions

			if ( seqdesc.flags & STUDIO_POST )
			{

				// result = q1 * ( weight * q2 ) 
				result = q1four.MulAc(weightfour, q2four);
			}
			else
			{

				// result = ( s * q1 ) * q2
				result = q2four.ScaleMul(weightfour, q1four);
			}

			// mask out unused channels, replacing them with original data
			{
				bi32x4 tinyScales = CmpLeSIMD( weightfour, Four_Zeros );
				result.x = MaskedAssign(tinyScales, q1four.x, result.x);
				result.y = MaskedAssign(tinyScales, q1four.y, result.y);
				result.z = MaskedAssign(tinyScales, q1four.z, result.z);
				result.w = MaskedAssign(tinyScales, q1four.w, result.w);
			}


			result.SwizzleAndStoreAlignedMasked(q1+i, CmpGtSIMD(weightfour,Four_Zeros) );

			fltx4 originalpos1simd[4], pos1simd[4], pos2simd[4];
			originalpos1simd[0] = pos1simd[0] = LoadUnalignedSIMD(pos1[i+0].Base());
			originalpos1simd[1] = pos1simd[1] = LoadUnalignedSIMD(pos1[i+1].Base());
			originalpos1simd[2] = pos1simd[2] = LoadUnalignedSIMD(pos1[i+2].Base());
			originalpos1simd[3] = pos1simd[3] = LoadUnalignedSIMD(pos1[i+3].Base());
			pos2simd[0] = LoadUnalignedSIMD(pos2[i+0].Base());
			pos2simd[1] = LoadUnalignedSIMD(pos2[i+1].Base());
			pos2simd[2] = LoadUnalignedSIMD(pos2[i+2].Base());
			pos2simd[3] = LoadUnalignedSIMD(pos2[i+3].Base());
			
			fltx4 splatweights[4] = { SplatXSIMD(weightfour),
									  SplatYSIMD(weightfour),
									  SplatZSIMD(weightfour),
									  SplatWSIMD(weightfour) };

			fltx4 Zero = Four_Zeros;
			pos1simd[0] = MaddSIMD(pos2simd[0], splatweights[0], pos1simd[0] );
				splatweights[0] = ( fltx4 ) CmpGtSIMD(splatweights[0], Zero);
			pos1simd[1] = MaddSIMD(pos2simd[1], splatweights[1], pos1simd[1] );
				splatweights[1] = ( fltx4 ) CmpGtSIMD(splatweights[1], Zero);
			pos1simd[2] = MaddSIMD(pos2simd[2], splatweights[2], pos1simd[2] );
				splatweights[2] = ( fltx4 ) CmpGtSIMD(splatweights[2], Zero);
			pos1simd[3] = MaddSIMD(pos2simd[3], splatweights[3], pos1simd[3] );
				splatweights[3] = ( fltx4 ) CmpGtSIMD(splatweights[3], Zero);

			// mask out unweighted bones
			/*
			if (pS2[i+0] > 0)
				StoreUnaligned3SIMD( pos1[i + 0].Base(), pos1simd[0] );
			if (pS2[i+1] > 0)
				StoreUnaligned3SIMD( pos1[i + 1].Base(), pos1simd[1] );
			if (pS2[i+2] > 0)
				StoreUnaligned3SIMD( pos1[i + 2].Base(), pos1simd[2] );
			if (pS2[i+3] > 0)
				StoreUnaligned3SIMD( pos1[i + 3].Base(), pos1simd[3] );
			*/
			StoreUnaligned3SIMD( pos1[i + 0].Base(), MaskedAssign( ( bi32x4 ) splatweights[0], pos1simd[0], originalpos1simd[0] ) );
			StoreUnaligned3SIMD( pos1[i + 1].Base(), MaskedAssign( ( bi32x4 ) splatweights[1], pos1simd[1], originalpos1simd[1] ) );
			StoreUnaligned3SIMD( pos1[i + 2].Base(), MaskedAssign( ( bi32x4 ) splatweights[2], pos1simd[2], originalpos1simd[2] ) );
			StoreUnaligned3SIMD( pos1[i + 3].Base(), MaskedAssign( ( bi32x4 ) splatweights[3], pos1simd[3], originalpos1simd[3] ) );

		}

		// take care of stragglers
		for ( false ; i < nBoneCount; i++ )
		{
			weight = pS2[i];
			if ( weight <= 0.0f )
				continue;

			if ( seqdesc.flags & STUDIO_POST )
			{
#ifndef _X360
				QuaternionMA( q1[i], weight, q2[i], q1[i] );
#else
				fltx4 q1simd = LoadUnalignedSIMD( q1[i].Base() );
				fltx4 q2simd = LoadAlignedSIMD( q2[i] );
				fltx4 result = QuaternionMASIMD( q1simd, weight, q2simd );
				StoreUnalignedSIMD( q1[i].Base(), result );
#endif
				// FIXME: are these correct?
				pos1[i][0] = pos1[i][0] + pos2[i][0] * weight;
				pos1[i][1] = pos1[i][1] + pos2[i][1] * weight;
				pos1[i][2] = pos1[i][2] + pos2[i][2] * weight;
			}
			else
			{
#ifndef _X360
				QuaternionSM( weight, q2[i], q1[i], q1[i] );
#else
				fltx4 q1simd = LoadUnalignedSIMD( q1[i].Base() );
				fltx4 q2simd = LoadAlignedSIMD( q2[i] );
				fltx4 result = QuaternionSMSIMD( weight, q2simd, q1simd );
				StoreUnalignedSIMD( q1[i].Base(), result );
#endif

				// FIXME: are these correct?
				pos1[i][0] = pos1[i][0] + pos2[i][0] * weight;
				pos1[i][1] = pos1[i][1] + pos2[i][1] * weight;
				pos1[i][2] = pos1[i][2] + pos2[i][2] * weight;
			}
		}
		return;
	}

	//// SLERP PHASE

	// Some bones need to be slerped with alignment.
	// Others do not.
	// Some need to be ignored altogether.
	// Build arrays indicating which are which. 
	// This is the corral approach. Another approach
	// would be to compute both the aligned and unaligned
	// slerps of each bone in the first pass through the 
	// array, and then do a masked selection of each 
	// based on the masks. However there really isn't 
	// a convenient way to turn the int flags that
	// specify which approach to take, into fltx4 masks.

	// float * RESTRICT pS2 = (float*)stackalloc( nBoneCount * sizeof(float) );
	int * RESTRICT aBonesSlerpAlign   = (int *)stackalloc(nBoneCount * sizeof(int));
	float * RESTRICT aBonesSlerpAlignWeights     = (float *)stackalloc(nBoneCount * sizeof(float));
	int * RESTRICT aBonesSlerpNoAlign = (int *)stackalloc(nBoneCount * sizeof(int));
	float * RESTRICT aBonesSlerpNoAlignWeights   = (float *)stackalloc(nBoneCount * sizeof(float));
	int numBonesSlerpAlign = 0;
	int numBonesSlerpNoAlign = 0;
	
	// BoneQuaternionAligned * RESTRICT testOutput = (BoneQuaternionAligned *)stackalloc(nBoneCount * sizeof(BoneQuaternionAligned));

	// sweep forward through the array and determine where to corral each bone.
	for ( i = 0 ; i < nBoneCount ; ++i )
	{
		float weight = pS2[i];
		if (weight == 1.0f)
		{
			q1[i] = q2[i];
			pos1[i] = pos2[i];
		}
		else if (weight > 0.0f) // ignore small bones
		{	
			if ( pStudioHdr->boneFlags(i) & BONE_FIXED_ALIGNMENT )
			{
				aBonesSlerpNoAlign[numBonesSlerpNoAlign] = i;
				aBonesSlerpNoAlignWeights[numBonesSlerpNoAlign] = weight;
				++numBonesSlerpNoAlign;
			}
			else
			{
				aBonesSlerpAlign[numBonesSlerpAlign] = i;
				aBonesSlerpAlignWeights[numBonesSlerpAlign] = weight;
				++numBonesSlerpAlign;
			}
		}
	}

	// okay, compute all the aligned, and all the unaligned bones, four at
	// a time if possible.
	const fltx4 One = Four_Ones;
	/////////////////
	// // // Aligned!
	nBoneCountRoundedFour = (numBonesSlerpAlign) & ~3;
	for (i = 0 ; i < nBoneCountRoundedFour ; i+=4 )
	{
		// drag the next cache line in
		PREFETCH360(q1, i*16 + 128);
		PREFETCH360(pos1, i*sizeof(*pos1) + 128);
		PREFETCH360(q2, i*16 + 128);
		PREFETCH360(pos2, i*sizeof(*pos2) + 128);

		fltx4 weights = LoadAlignedSIMD( aBonesSlerpAlignWeights+i );
		fltx4 oneMinusWeight = SubSIMD(One, weights);

		// position component:
		// pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * weight;
		fltx4 pos1simd[4];
		fltx4 pos2simd[4];
		pos1simd[0] = LoadUnaligned3SIMD(pos1[aBonesSlerpAlign[i+0]].Base()); 
		pos1simd[1] = LoadUnaligned3SIMD(pos1[aBonesSlerpAlign[i+1]].Base()); 
		pos1simd[2] = LoadUnaligned3SIMD(pos1[aBonesSlerpAlign[i+2]].Base()); 
		pos1simd[3] = LoadUnaligned3SIMD(pos1[aBonesSlerpAlign[i+3]].Base()); 
		pos2simd[0] = LoadUnaligned3SIMD(pos2[aBonesSlerpAlign[i+0]].Base()); 
		pos2simd[1] = LoadUnaligned3SIMD(pos2[aBonesSlerpAlign[i+1]].Base()); 
		pos2simd[2] = LoadUnaligned3SIMD(pos2[aBonesSlerpAlign[i+2]].Base()); 
		pos2simd[3] = LoadUnaligned3SIMD(pos2[aBonesSlerpAlign[i+3]].Base()); 

		pos1simd[0] = MulSIMD( SplatXSIMD(oneMinusWeight) , pos1simd[0] );
		pos1simd[1] = MulSIMD( SplatYSIMD(oneMinusWeight) , pos1simd[1] );
		pos1simd[2] = MulSIMD( SplatZSIMD(oneMinusWeight) , pos1simd[2] );
		pos1simd[3] = MulSIMD( SplatWSIMD(oneMinusWeight) , pos1simd[3] );

		fltx4 posWriteMasks[4]; // don't overwrite where there was zero weight
		{
			fltx4 splatweights[4];
			fltx4 Zero = Four_Zeros;
			splatweights[0] = SplatXSIMD(weights);
			splatweights[1] = SplatYSIMD(weights);
			splatweights[2] = SplatZSIMD(weights);
			splatweights[3] = SplatWSIMD(weights);

			pos1simd[0] = MaddSIMD( splatweights[0] , pos2simd[0], pos1simd[0] );
				posWriteMasks[0] = ( fltx4 ) CmpGtSIMD(splatweights[0], Zero);
			pos1simd[1] = MaddSIMD( splatweights[1] , pos2simd[1], pos1simd[1] );
				posWriteMasks[1] = ( fltx4 ) CmpGtSIMD(splatweights[1], Zero);
			pos1simd[2] = MaddSIMD( splatweights[2] , pos2simd[2], pos1simd[2] );
				posWriteMasks[2] = ( fltx4 ) CmpGtSIMD(splatweights[2], Zero);
			pos1simd[3] = MaddSIMD( splatweights[3] , pos2simd[3], pos1simd[3] );
				posWriteMasks[3] = ( fltx4 ) CmpGtSIMD(splatweights[3], Zero);
		}


		FourQuaternions q1four, q2four, result;		
		q1four.LoadAndSwizzleAligned(	q1 + aBonesSlerpAlign[i+0],
										q1 + aBonesSlerpAlign[i+1],
										q1 + aBonesSlerpAlign[i+2],
										q1 + aBonesSlerpAlign[i+3] );

#if 0
		// FIXME: the SIMD slerp doesn't handle quaternions that have opposite signs
		q2four.LoadAndSwizzleAligned(	q2 + aBonesSlerpAlign[i+0],
										q2 + aBonesSlerpAlign[i+1],
										q2 + aBonesSlerpAlign[i+2],
										q2 + aBonesSlerpAlign[i+3] );
		result = q2four.Slerp(q1four, oneMinusWeight);
#else
		// force the quaternions to be the same sign (< 180 degree separation)
		BoneQuaternionAligned q20, q21, q22, q23;
		QuaternionAlign( q1[aBonesSlerpAlign[i+0]], q2[aBonesSlerpAlign[i+0]], q20 );
		QuaternionAlign( q1[aBonesSlerpAlign[i+1]], q2[aBonesSlerpAlign[i+1]], q21 );
		QuaternionAlign( q1[aBonesSlerpAlign[i+2]], q2[aBonesSlerpAlign[i+2]], q22 );
		QuaternionAlign( q1[aBonesSlerpAlign[i+3]], q2[aBonesSlerpAlign[i+3]], q23 );
		q2four.LoadAndSwizzleAligned( &q20, &q21, &q22, &q23 );
		result = q2four.SlerpNoAlign(q1four, oneMinusWeight);
#endif

		result.SwizzleAndStoreAligned( q1 + aBonesSlerpAlign[i+0],
			q1 + aBonesSlerpAlign[i+1],
			q1 + aBonesSlerpAlign[i+2],
			q1 + aBonesSlerpAlign[i+3] );

		StoreUnaligned3SIMD( pos1[aBonesSlerpAlign[i+0]].Base(), pos1simd[0] );
		StoreUnaligned3SIMD( pos1[aBonesSlerpAlign[i+1]].Base(), pos1simd[1] );
		StoreUnaligned3SIMD( pos1[aBonesSlerpAlign[i+2]].Base(), pos1simd[2] );
		StoreUnaligned3SIMD( pos1[aBonesSlerpAlign[i+3]].Base(), pos1simd[3] );
	}

	// handle stragglers
	for ( i ; i < numBonesSlerpAlign ; ++i )
	{
		BoneQuaternionAligned q3;
		weight = aBonesSlerpAlignWeights[i];
		int k = aBonesSlerpAlign[i];

		float s1 = 1.0 - weight;

#ifdef _X360
		fltx4  q1simd, q2simd, result;
		q1simd = LoadAlignedSIMD( q1[k].Base() );
		q2simd = LoadAlignedSIMD( q2[k] );
#endif

#ifndef _X360
		QuaternionSlerp( q2[k], q1[k], s1, q3 );
#else
		result = QuaternionSlerpSIMD( q2simd, q1simd, s1 );
#endif

#ifndef _X360
		q1[k][0] = q3[0];
		q1[k][1] = q3[1];
		q1[k][2] = q3[2];
		q1[k][3] = q3[3];
#else
		StoreAlignedSIMD( q1[k].Base(), result );
#endif

		pos1[k][0] = pos1[k][0] * s1 + pos2[k][0] * weight;
		pos1[k][1] = pos1[k][1] * s1 + pos2[k][1] * weight;
		pos1[k][2] = pos1[k][2] * s1 + pos2[k][2] * weight;
	}
	///////////////////
	// // // Unaligned!
	nBoneCountRoundedFour = (numBonesSlerpNoAlign) & ~3;
	for (i = 0 ; i < nBoneCountRoundedFour ; i+=4 )
	{
		// drag the next cache line in
		PREFETCH360(q1, i*16 + 128);
		PREFETCH360(pos1, i*sizeof(*pos1) + 128);
		PREFETCH360(q2, i*16 + 128);
		PREFETCH360(pos2, i*sizeof(*pos2) + 128);

		fltx4 weights = LoadAlignedSIMD( aBonesSlerpNoAlignWeights+i );
		fltx4 oneMinusWeight = SubSIMD(One, weights);

		// position component:
		// pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * weight;
		fltx4 pos1simd[4];
		fltx4 pos2simd[4];
		pos1simd[0] = LoadUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+0]].Base()); 
		pos1simd[1] = LoadUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+1]].Base()); 
		pos1simd[2] = LoadUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+2]].Base()); 
		pos1simd[3] = LoadUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+3]].Base()); 
		pos2simd[0] = LoadUnaligned3SIMD(pos2[aBonesSlerpNoAlign[i+0]].Base()); 
		pos2simd[1] = LoadUnaligned3SIMD(pos2[aBonesSlerpNoAlign[i+1]].Base()); 
		pos2simd[2] = LoadUnaligned3SIMD(pos2[aBonesSlerpNoAlign[i+2]].Base()); 
		pos2simd[3] = LoadUnaligned3SIMD(pos2[aBonesSlerpNoAlign[i+3]].Base()); 

		pos1simd[0] = MulSIMD( SplatXSIMD(oneMinusWeight) , pos1simd[0] );
		pos1simd[1] = MulSIMD( SplatYSIMD(oneMinusWeight) , pos1simd[1] );
		pos1simd[2] = MulSIMD( SplatZSIMD(oneMinusWeight) , pos1simd[2] );
		pos1simd[3] = MulSIMD( SplatWSIMD(oneMinusWeight) , pos1simd[3] );

		pos1simd[0] = MaddSIMD( SplatXSIMD(weights) , pos2simd[0], pos1simd[0] );
		pos1simd[1] = MaddSIMD( SplatYSIMD(weights) , pos2simd[1], pos1simd[1] );
		pos1simd[2] = MaddSIMD( SplatZSIMD(weights) , pos2simd[2], pos1simd[2] );
		pos1simd[3] = MaddSIMD( SplatWSIMD(weights) , pos2simd[3], pos1simd[3] );

		FourQuaternions q1four, q2four, result;		
		q1four.LoadAndSwizzleAligned(	q1 + aBonesSlerpNoAlign[i+0],
			q1 + aBonesSlerpNoAlign[i+1],
			q1 + aBonesSlerpNoAlign[i+2],
			q1 + aBonesSlerpNoAlign[i+3] );
		q2four.LoadAndSwizzleAligned(	q2 + aBonesSlerpNoAlign[i+0],
			q2 + aBonesSlerpNoAlign[i+1],
			q2 + aBonesSlerpNoAlign[i+2],
			q2 + aBonesSlerpNoAlign[i+3] );

		result = q2four.SlerpNoAlign(q1four, oneMinusWeight);

		result.SwizzleAndStoreAligned( q1 + aBonesSlerpNoAlign[i+0],
			q1 + aBonesSlerpNoAlign[i+1],
			q1 + aBonesSlerpNoAlign[i+2],
			q1 + aBonesSlerpNoAlign[i+3] );

		StoreUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+0]].Base(), pos1simd[0]);
		StoreUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+1]].Base(), pos1simd[1]);
		StoreUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+2]].Base(), pos1simd[2]);
		StoreUnaligned3SIMD(pos1[aBonesSlerpNoAlign[i+3]].Base(), pos1simd[3]);
	}
	// handle stragglers
	for ( i ; i < numBonesSlerpNoAlign ; ++i )
	{
		weight = aBonesSlerpNoAlignWeights[i];
		int k = aBonesSlerpNoAlign[i];

		float s1 = 1.0 - weight;

#ifdef _X360
		fltx4  q1simd, q2simd, result;
		q1simd = LoadAlignedSIMD( q1[k].Base() );
		q2simd = LoadAlignedSIMD( q2[k] );
#endif

#ifndef _X360
		BoneQuaternionAligned q3;
		QuaternionSlerpNoAlign( q2[k], q1[k], s1, q3 );
#else
		result = QuaternionSlerpNoAlignSIMD( q2simd, q1simd, s1 );
#endif

#ifndef _X360
		q1[k][0] = q3[0];
		q1[k][1] = q3[1];
		q1[k][2] = q3[2];
		q1[k][3] = q3[3];
#else
		StoreAlignedSIMD( q1[k].Base(), result );
#endif

		pos1[k][0] = pos1[k][0] * s1 + pos2[k][0] * weight;
		pos1[k][1] = pos1[k][1] * s1 + pos2[k][1] * weight;
		pos1[k][2] = pos1[k][2] * s1 + pos2[k][2] * weight;
	}

}



#if PARANOID_SIMD_TIMING_TEST
static ConVar cl_bones_simd_timing_iter( "cl_bones_simd_timing_iter", "100", FCVAR_REPLICATED, "number of times to run SlerpBones." );
void SlerpBones( 
				const CStudioHdr *pStudioHdr,
				Quaternion q1[MAXSTUDIOBONES], 
				BoneVector pos1[MAXSTUDIOBONES], 
				mstudioseqdesc_t &seqdesc,  // source of q2 and pos2
				int sequence, 
				const BoneQuaternionAligned q2[MAXSTUDIOBONES], 
				const BoneVector pos2[MAXSTUDIOBONES], 
				float s,
				int boneMask )
{
	BONE_PROFILE_FUNC();
	// copy off the input arrays for safety
	int numBones =  pStudioHdr->numbones();
	BoneQuaternionAligned fake_q1[MAXSTUDIOBONES];
	BoneVector fake_pos1[MAXSTUDIOBONES];
	bool version = cl_bones_simd_timing_version.GetBool();

	// fruitlessly run as many times as specified
	for (int i = cl_bones_simd_timing_iter.GetInt() ; i > 0 ; --i )
	{
		memcpy( fake_q1, q1, numBones * sizeof(Quaternion) );
		memcpy( fake_pos1, pos1, numBones * sizeof(BoneVector) );

		if (version) // 1 = simd  0 = scalar
		{
			SlerpBonesSpeedy(pStudioHdr,
				fake_q1,
				fake_pos1,
				seqdesc,
				sequence,
				q2,
				pos2,
				s,
				boneMask
				);
		}
		else
		{

			SlerpBonesSlow(pStudioHdr,
				fake_q1,
				fake_pos1,
				seqdesc,
				sequence,
				q2,
				pos2,
				s,
				boneMask
				);
		}
	}

	// run once for real
	if (version) // 1 = simd  0 = scalar
	{
		SlerpBonesSpeedy(pStudioHdr,
			static_cast<BoneQuaternionAligned *>(q1),
			pos1,
			seqdesc,
			sequence,
			q2,
			pos2,
			s,
			boneMask
			);
	}
	else
	{

		SlerpBonesSlow(pStudioHdr,
			q1,
			pos1,
			seqdesc,
			sequence,
			q2,
			pos2,
			s,
			boneMask
			);
	}
}
#endif

template <int N>
struct GetLog2_t
{};
template<>
struct GetLog2_t<0x00100000>
{
	enum {kLog2 = 20};
};

inline void AlwaysAssert(bool condition)
{
	Assert(condition);
}

bool IsInList(int value, const int *pBegin, const int *pEnd)
{
	for(const int *p = pBegin; p < pEnd; ++p)
		if(*p == value)
			return true;
	return false;
}

//CLinkedMiniProfiler g_lmp_BlendBones1("BlendBones1",&g_pPhysicsMiniProfilers);
//CLinkedMiniProfiler g_lmp_BlendBones2("BlendBones2",&g_pPhysicsMiniProfilers);

ConVar g_cv_BlendBonesMode("BlendBonesMode", "2", FCVAR_REPLICATED);


//---------------------------------------------------------------------
// Make sure quaternions are within 180 degrees of one another, if not, reverse q
//---------------------------------------------------------------------
FORCEINLINE fltx4 BoneQuaternionAlignSIMD( const fltx4 &p, const fltx4 &q )
{
	// decide if one of the quaternions is backwards
	bi32x4 cmp = CmpLtSIMD( Dot4SIMD(p,q), Four_Zeros );
	fltx4 result = MaskedAssign( cmp, NegSIMD(q), q );
	return result;
}


// SSE + X360 implementation
FORCEINLINE fltx4 BoneQuaternionNormalizeSIMD( const fltx4 &q )
{
	fltx4 radius, result;
	bi32x4 mask;
	radius = Dot4SIMD( q, q );
	mask = CmpEqSIMD( radius, Four_Zeros ); // all ones iff radius = 0
	result = ReciprocalSqrtSIMD( radius );
	result = MulSIMD( result, q );
	return MaskedAssign( mask, q, result );	// if radius was 0, just return q
}





//-----------------------------------------------------------------------------
// Purpose: Inter-animation blend.  Assumes both types are identical.
//			blend together q1,pos1 with q2,pos2.  Return result in q1,pos1.  
//			0 returns q1, pos1.  1 returns q2, pos2
//-----------------------------------------------------------------------------


void BlendBones( 
	const CStudioHdr *pStudioHdr,
	BoneQuaternionAligned q1[MAXSTUDIOBONES],
	BoneVector pos1[MAXSTUDIOBONES], 
	mstudioseqdesc_t &seqdesc, 
	int sequence,
	const BoneQuaternionAligned q2[MAXSTUDIOBONES], 
	const BoneVector pos2[MAXSTUDIOBONES], 
	float s,
	int boneMask )
{
	AlwaysAssert(0 == ((uintp(q1)|uintp(pos1)|uintp(q2)|uintp(pos2)) & 0xF));
	BONE_PROFILE_FUNC(); // in: x360: up to 1.67 ms
	int			i, j;
	Quaternion		q3;

	virtualmodel_t *pVModel = pStudioHdr->GetVirtualModel();
	const virtualgroup_t *pSeqGroup = NULL;
	if (pVModel)
	{
		pSeqGroup = pVModel->pSeqGroup( sequence );
	}

	if (s <= 0)
	{
		Assert(0); // shouldn't have been called
		return;
	}
	else if (s >= 1.0)
	{
		//CMiniProfilerGuard mpguard(&g_lmp_BlendBones1, pStudioHdr->numbones());

		Assert(0); // shouldn't have been called
		for (i = 0; i < pStudioHdr->numbones(); i++)
		{
			// skip unused bones
			if (!(pStudioHdr->boneFlags(i) & boneMask))
			{
				continue;
			}

			if (pSeqGroup)
			{
				j = pSeqGroup->boneMap[i];
			}
			else
			{
				j = i;
			}

			if (j >= 0 && seqdesc.weight( j ) > 0.0)
			{
				q1[i] = q2[i];
				pos1[i] = pos2[i];
			}
		}

		return;
	}

	float s2 = s;
	float s1 = 1.0 - s2;

	//CMiniProfilerGuard mpguard(&g_lmp_BlendBones2,pStudioHdr->numbones()); // 130-180 ticks without profilers; 167-190 ticks with all profilers on

	int nMode = g_cv_BlendBonesMode.GetInt();
#ifndef	DEDICATED
	if(nMode)
	{
		const int numBones = pStudioHdr->numbones();
		const int *RESTRICT pBonePseudoWeight = (int*)seqdesc.pBoneweight(0);  // we'll treat floats as ints to check for > 0.0
		int *RESTRICT pActiveBones = (int*)stackalloc(numBones * sizeof(int) * 2), *RESTRICT pActiveBonesEnd = pActiveBones;
		{
			BONE_PROFILE_LOOP(BlendBoneLoop2a,numBones); // 20 ticks straight; 12-14 ticks 4 at a time; 14-19 ticks 8 at a time (compiler generated code)

			i = 0;
#ifdef _X360 // on PC, this is slower
			for(; i+3 < numBones; i+=4)
			{
				int isBoneActiveA = pStudioHdr->boneFlags(i  ) & boneMask;
				int isBoneActiveB = pStudioHdr->boneFlags(i+1) & boneMask;
				int isBoneActiveC = pStudioHdr->boneFlags(i+2) & boneMask;
				int isBoneActiveD = pStudioHdr->boneFlags(i+3) & boneMask;
				isBoneActiveA = isBoneActiveA | -isBoneActiveA; // the high bit is now 1 iff the flags check 
				isBoneActiveB = isBoneActiveB | -isBoneActiveB; // the high bit is now 1 iff the flags check 
				isBoneActiveC = isBoneActiveC | -isBoneActiveC; // the high bit is now 1 iff the flags check 
				isBoneActiveD = isBoneActiveD | -isBoneActiveD; // the high bit is now 1 iff the flags check 
				isBoneActiveA = _rotl(isBoneActiveA,1) & 1;  // now it's either 0 or 1
				isBoneActiveB = _rotl(isBoneActiveB,1) & 1;  // now it's either 0 or 1
				isBoneActiveC = _rotl(isBoneActiveC,1) & 1;  // now it's either 0 or 1
				isBoneActiveD = _rotl(isBoneActiveD,1) & 1;  // now it's either 0 or 1
				*pActiveBonesEnd = i+0;
				pActiveBonesEnd += isBoneActiveA;
				*pActiveBonesEnd = i+1;
				pActiveBonesEnd += isBoneActiveB;
				*pActiveBonesEnd = i+2;
				pActiveBonesEnd += isBoneActiveC;
				*pActiveBonesEnd = i+3;
				pActiveBonesEnd += isBoneActiveD;
			}
#endif
			for(; i < numBones; ++i)
			{
				*pActiveBonesEnd = i;
				int isBoneActive = pStudioHdr->boneFlags(i) & boneMask;
				isBoneActive = isBoneActive | -isBoneActive; // the high bit is now 1 iff the flags check 
				isBoneActive = _rotl(isBoneActive,1) & 1;  // now it's either 0 or 1
				pActiveBonesEnd += isBoneActive;
			}
		}

		// now we have a list of bones whose flags & mask != 0
		// we need to create bone pay
		if(pSeqGroup)
		{
			int *pEnd = pActiveBones;
			{
				BONE_PROFILE_LOOP(BlendBoneLoop2b,pActiveBonesEnd - pActiveBones);//21-25 straight; 16-18 4 at a time;

				int *RESTRICT pActiveBone = pActiveBones;
#ifdef _X360 // on PC, this is slower
				for(; pActiveBone + 3 < pActiveBonesEnd; pActiveBone += 4)
				{
					int nActiveBoneA = pActiveBone[0];
					int nActiveBoneB = pActiveBone[1];
					int nActiveBoneC = pActiveBone[2];
					int nActiveBoneD = pActiveBone[3];
					int nMappedBoneA = pSeqGroup->boneMap[nActiveBoneA];
					int nMappedBoneB = pSeqGroup->boneMap[nActiveBoneB];
					int nMappedBoneC = pSeqGroup->boneMap[nActiveBoneC];
					int nMappedBoneD = pSeqGroup->boneMap[nActiveBoneD];
					pEnd[numBones] = nMappedBoneA;
					*pEnd = nActiveBoneA;
					pEnd += _rotl(~nMappedBoneA,1) & 1; // if nMappedBone < 0, don't advance the end
					pEnd[numBones] = nMappedBoneB;
					*pEnd = nActiveBoneB;
					pEnd += _rotl(~nMappedBoneB,1) & 1; // if nMappedBone < 0, don't advance the end
					pEnd[numBones] = nMappedBoneC;
					*pEnd = nActiveBoneC;
					pEnd += _rotl(~nMappedBoneC,1) & 1; // if nMappedBone < 0, don't advance the end
					pEnd[numBones] = nMappedBoneD;
					*pEnd = nActiveBoneD;
					pEnd += _rotl(~nMappedBoneD,1) & 1; // if nMappedBone < 0, don't advance the end
				}
#endif
				for(; pActiveBone < pActiveBonesEnd; ++pActiveBone)
				{
					int nActiveBone = *pActiveBone;
					int nMappedBone = pSeqGroup->boneMap[nActiveBone];
					pEnd[numBones] = nMappedBone;
					*pEnd = nActiveBone;
					pEnd += _rotl(~nMappedBone,1) & 1; // if nMappedBone < 0, don't advance the end
				}
			}

			pActiveBonesEnd = pEnd; // the new end of the array of active bones, with negatively-mapped bones taken out
			// now get rid of non-positively-weighted bones
			pEnd = pActiveBones;
			{
				BONE_PROFILE_LOOP(BlendBoneLoop2c,pActiveBonesEnd - pActiveBones);//18-23 straight; 14-17 ticks 4 at a time

				int *RESTRICT pActiveBone = pActiveBones;
#ifdef _X360 // on PC, this is slower
				int *RESTRICT pMappedBone = pActiveBones+numBones;
				for(; pActiveBone+3 < pActiveBonesEnd; pActiveBone += 4, pMappedBone += 4)
				{
					int nActiveBoneA = pActiveBone[0];
					int nActiveBoneB = pActiveBone[1];
					int nActiveBoneC = pActiveBone[2];
					int nActiveBoneD = pActiveBone[3];
					int nMappedBoneA = pMappedBone[0];
					int nMappedBoneB = pMappedBone[1];
					int nMappedBoneC = pMappedBone[2];
					int nMappedBoneD = pMappedBone[3];
					int pseudoWeightA = pBonePseudoWeight[nMappedBoneA];
					int pseudoWeightB = pBonePseudoWeight[nMappedBoneB];
					int pseudoWeightC = pBonePseudoWeight[nMappedBoneC];
					int pseudoWeightD = pBonePseudoWeight[nMappedBoneD];

					*pEnd = nActiveBoneA;
					pEnd += _rotl(-pseudoWeightA, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
					*pEnd = nActiveBoneB;
					pEnd += _rotl(-pseudoWeightB, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
					*pEnd = nActiveBoneC;
					pEnd += _rotl(-pseudoWeightC, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
					*pEnd = nActiveBoneD;
					pEnd += _rotl(-pseudoWeightD, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
				}
#endif
				for(; pActiveBone < pActiveBonesEnd; ++pActiveBone)
				{
					int nActiveBone = *pActiveBone;
					int nMappedBone = pActiveBone[numBones];
					int pseudoWeight = pBonePseudoWeight[nMappedBone];

					*pEnd = nActiveBone;
					pEnd += _rotl(-pseudoWeight, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
				}
			}
			pActiveBonesEnd = pEnd;
		}
		else
		{
			// one mapping stage off
			// now get rid of non-positively-weighted bones
			int *pEnd = pActiveBones;
			{BONE_PROFILE_LOOP(BlendBoneLoop2d,pActiveBonesEnd-pActiveBones);//20-50
			for(int *RESTRICT pActiveBone = pActiveBones; pActiveBone < pActiveBonesEnd; ++pActiveBone)
			{
				int nActiveBone = *pActiveBone;
				int pseudoWeight = pBonePseudoWeight[nActiveBone];

				*pEnd = nActiveBone;
				pEnd += _rotl(-pseudoWeight, 1) & 1; // pseudoWeight must be strictly positive to advance and let this bone stay
			}}
			pActiveBonesEnd = pEnd;
		}

		enum
		{
			nBoneFixedAlignmentShift = GetLog2_t<BONE_FIXED_ALIGNMENT>::kLog2
		};

		// NOTE: When merging back to main, enable this code because Fixed-Alignment is not used in L4D, but may be used in main
			fltx4 scale1 = ReplicateX4( s1 );
			fltx4 scale2 = SubSIMD( Four_Ones, scale1 );
			//fltx4 maskW = LoadAlignedSIMD( (const float *)(g_SIMD_ComponentMask[3]) );


			// pass through all active bones to blend them; those that need it are already aligned
			{
				// 120-155 ticks 4 horizontal at a time; 130 ticks with 1 dot quaternion alignment
				// 
				BONE_PROFILE_LOOP(BlendBoneLoop2g,pActiveBonesEnd-pActiveBones);

				const int *RESTRICT p = pActiveBones, *RESTRICT pNext;
#if 0//ndef _X360
				// swizzled (vertical) 4 at a time processing
				for(; (pNext = p+4) < pActiveBonesEnd; p = pNext)
				{
					int nBoneA = p[0], nBoneB = p[1], nBoneC = p[2], nBoneD = p[3];

					BoneQuaternionAligned *RESTRICT pq1A = &q1[nBoneA]; 
					BoneQuaternionAligned *RESTRICT pq1B = &q1[nBoneB]; 
					BoneQuaternionAligned *RESTRICT pq1C = &q1[nBoneC]; 
					BoneQuaternionAligned *RESTRICT pq1D = &q1[nBoneD]; 

					const BoneQuaternionAligned *RESTRICT pq2A = &q2[nBoneA]; 
					const BoneQuaternionAligned *RESTRICT pq2B = &q2[nBoneB]; 
					const BoneQuaternionAligned *RESTRICT pq2C = &q2[nBoneC]; 
					const BoneQuaternionAligned *RESTRICT pq2D = &q2[nBoneD]; 

					float *pp1A = pos1[nBoneA].Base();
					float *pp1B = pos1[nBoneB].Base();
					float *pp1C = pos1[nBoneC].Base();
					float *pp1D = pos1[nBoneD].Base();

					const float *pp2A = pos2[nBoneA].Base();
					const float *pp2B = pos2[nBoneB].Base();
					const float *pp2C = pos2[nBoneC].Base();
					const float *pp2D = pos2[nBoneD].Base();

					FourQuaternions four4q1, four4q2;
					four4q1.LoadAndSwizzleAligned(pq1A,pq1B,pq1C,pq1D);
					four4q2.LoadAndSwizzleAligned(pq2A,pq2B,pq2C,pq2D);

					FourVectors four4Pos1, four4Pos2;
					four4Pos1.LoadAndSwizzleUnaligned(pp1A,pp1B,pp1C,pp1D);
					four4Pos2.LoadAndSwizzleUnaligned(pp2A,pp2B,pp2C,pp2D);

					four4q1 = QuaternionAlign(four4q2, four4q1);

					FourQuaternions four4Blended = QuaternionNormalize(Madd( four4q1, scale1, Mul( four4q2 , scale2 )));
					// now blend the linear parts
					FourVectors f4PosBlended = Madd(four4Pos1, scale1, Mul(four4Pos2, scale2));
					f4PosBlended.TransposeOntoUnaligned3(*(fltx4*)pp1A, *(fltx4*)pp1B, *(fltx4*)pp1C, *(fltx4*)pp1D);

					four4Blended.SwizzleAndStoreAligned(pq1A,pq1B,pq1C,pq1D);
				}
#else
				// horizontal 4 at a time processing
				for(; (pNext = p+4) < pActiveBonesEnd; p = pNext)
				{
					int nBoneA = p[0], nBoneB = p[1], nBoneC = p[2], nBoneD = p[3];
					//PREFETCH_CACHE_LINE(&q1[nBoneD+2],0);
					//PREFETCH_CACHE_LINE(&q2[nBoneD+2],0);
					//PREFETCH_CACHE_LINE(&pos1[nBoneD+2],0);
					//PREFETCH_CACHE_LINE(&pos2[nBoneD+2],0);
					float *RESTRICT pq1A = q1[nBoneA].Base(), *pp1A = pos1[nBoneA].Base();
					float *RESTRICT pq1B = q1[nBoneB].Base(), *pp1B = pos1[nBoneB].Base();
					float *RESTRICT pq1C = q1[nBoneC].Base(), *pp1C = pos1[nBoneC].Base();
					float *RESTRICT pq1D = q1[nBoneD].Base(), *pp1D = pos1[nBoneD].Base();
					const float *RESTRICT pq2A = q2[nBoneA].Base(), *pp2A = pos2[nBoneA].Base();
					const float *RESTRICT pq2B = q2[nBoneB].Base(), *pp2B = pos2[nBoneB].Base();
					const float *RESTRICT pq2C = q2[nBoneC].Base(), *pp2C = pos2[nBoneC].Base();
					const float *RESTRICT pq2D = q2[nBoneD].Base(), *pp2D = pos2[nBoneD].Base();
					fltx4 f4q1A = LoadAlignedSIMD(pq1A), f4q2A = LoadAlignedSIMD(pq2A);
					fltx4 f4q1B = LoadAlignedSIMD(pq1B), f4q2B = LoadAlignedSIMD(pq2B);
					fltx4 f4q1C = LoadAlignedSIMD(pq1C), f4q2C = LoadAlignedSIMD(pq2C);
					fltx4 f4q1D = LoadAlignedSIMD(pq1D), f4q2D = LoadAlignedSIMD(pq2D);
					fltx4 f4Pos1A = LoadUnaligned3SIMD(pp1A), f4Pos2A = LoadUnaligned3SIMD(pp2A);
					fltx4 f4Pos1B = LoadUnaligned3SIMD(pp1B), f4Pos2B = LoadUnaligned3SIMD(pp2B);
					fltx4 f4Pos1C = LoadUnaligned3SIMD(pp1C), f4Pos2C = LoadUnaligned3SIMD(pp2C);
					fltx4 f4Pos1D = LoadUnaligned3SIMD(pp1D), f4Pos2D = LoadUnaligned3SIMD(pp2D);
					f4q1A = BoneQuaternionAlignSIMD(f4q2A, f4q1A);
					f4q1B = BoneQuaternionAlignSIMD(f4q2B, f4q1B);
					f4q1C = BoneQuaternionAlignSIMD(f4q2C, f4q1C);
					f4q1D = BoneQuaternionAlignSIMD(f4q2D, f4q1D);
					fltx4 f4BlendedA = MulSIMD( scale2, f4q2A );
					fltx4 f4BlendedB = MulSIMD( scale2, f4q2B );
					fltx4 f4BlendedC = MulSIMD( scale2, f4q2C );
					fltx4 f4BlendedD = MulSIMD( scale2, f4q2D );
					f4BlendedA = MaddSIMD( scale1, f4q1A, f4BlendedA );
					f4BlendedB = MaddSIMD( scale1, f4q1B, f4BlendedB );
					f4BlendedC = MaddSIMD( scale1, f4q1C, f4BlendedC );
					f4BlendedD = MaddSIMD( scale1, f4q1D, f4BlendedD );
					f4BlendedA = BoneQuaternionNormalizeSIMD(f4BlendedA);
					f4BlendedB = BoneQuaternionNormalizeSIMD(f4BlendedB);
					f4BlendedC = BoneQuaternionNormalizeSIMD(f4BlendedC);
					f4BlendedD = BoneQuaternionNormalizeSIMD(f4BlendedD);
					// now blend the linear parts
					fltx4 f4PosBlendedA = MaddSIMD(scale1, f4Pos1A, MulSIMD(scale2,f4Pos2A));
					fltx4 f4PosBlendedB = MaddSIMD(scale1, f4Pos1B, MulSIMD(scale2,f4Pos2B));
					fltx4 f4PosBlendedC = MaddSIMD(scale1, f4Pos1C, MulSIMD(scale2,f4Pos2C));
					fltx4 f4PosBlendedD = MaddSIMD(scale1, f4Pos1D, MulSIMD(scale2,f4Pos2D));
					//f4PosBlended = MaskedAssign(maskW, f4Pos1, f4PosBlended);

					StoreAlignedSIMD(pq1A,f4BlendedA);
					StoreUnaligned3SIMD(pp1A, f4PosBlendedA);
					StoreAlignedSIMD(pq1B,f4BlendedB);
					StoreUnaligned3SIMD(pp1B, f4PosBlendedB);
					StoreAlignedSIMD(pq1C,f4BlendedC);
					StoreUnaligned3SIMD(pp1C, f4PosBlendedC);
					StoreAlignedSIMD(pq1D,f4BlendedD);
					StoreUnaligned3SIMD(pp1D, f4PosBlendedD);
				}
#endif
				for(; p < pActiveBonesEnd; ++p)
				{
					int nBone = *p;
					float *RESTRICT pq1 = q1[nBone].Base(), *RESTRICT pp1 = pos1[nBone].Base();
					const float *RESTRICT pq2 = q2[nBone].Base(), *RESTRICT pp2 = pos2[nBone].Base();
					fltx4 f4q1 = LoadAlignedSIMD(pq1), f4q2 = LoadAlignedSIMD(pq2);
					fltx4 f4Pos1 = LoadUnaligned3SIMD(pp1), f4Pos2 = LoadUnaligned3SIMD(pp2);
					f4q1 = BoneQuaternionAlignSIMD(f4q2, f4q1);
					fltx4 f4Blended = MulSIMD( scale2, f4q2 );
					f4Blended = MaddSIMD( scale1, f4q1, f4Blended );
					f4Blended = BoneQuaternionNormalizeSIMD(f4Blended);
					// now blend the linear parts
					fltx4 f4PosBlended = MaddSIMD(scale1, f4Pos1, MulSIMD(scale2,f4Pos2));
					//f4PosBlended = MaskedAssign(maskW, f4Pos1, f4PosBlended);

					StoreAlignedSIMD(pq1,f4Blended);
					StoreUnaligned3SIMD(pp1, f4PosBlended);
				}
			}
	}
	else
#endif // POSIX
	{
		// 360-400 ticks per loop pass
		// there are usually 40-100 bones on average in a frame
		for (i = 0; i < pStudioHdr->numbones(); i++) 
		{
			// skip unused bones
			if (!(pStudioHdr->boneFlags(i) & boneMask))
			{
				continue;
			}

			if (pSeqGroup)
			{
				j = pSeqGroup->boneMap[i];
			}
			else
			{
				j = i;
			}

			if (j >= 0 && seqdesc.weight( j ) > 0.0)
			{
				if (pStudioHdr->boneFlags(i) & BONE_FIXED_ALIGNMENT)
				{
					QuaternionBlendNoAlign( q2[i], q1[i], s1, q3 );
				}
				else
				{
					QuaternionBlend( q2[i], q1[i], s1, q3 );
				}
				q1[i][0] = q3[0];
				q1[i][1] = q3[1];
				q1[i][2] = q3[2];
				q1[i][3] = q3[3];
				pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s2;
				pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s2;
				pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s2;
			}
		}
	}
}




//-----------------------------------------------------------------------------
// Purpose: Scale a set of bones.  Must be of type delta
//-----------------------------------------------------------------------------
void ScaleBones( 
	const CStudioHdr *pStudioHdr,
	BoneQuaternion q1[MAXSTUDIOBONES], 
	BoneVector pos1[MAXSTUDIOBONES], 
	int sequence,
	float s,
	int boneMask )
{
	BONE_PROFILE_FUNC();
	int			i, j;
	Quaternion		q3;

	mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( sequence );

	virtualmodel_t *pVModel = pStudioHdr->GetVirtualModel();
	const virtualgroup_t *pSeqGroup = NULL;
	if (pVModel)
	{
		pSeqGroup = pVModel->pSeqGroup( sequence );
	}

	float s2 = s;
	float s1 = 1.0 - s2;

	for (i = 0; i < pStudioHdr->numbones(); i++)
	{
		// skip unused bones
		if (!(pStudioHdr->boneFlags(i) & boneMask))
		{
			continue;
		}

		if (pSeqGroup)
		{
			j = pSeqGroup->boneMap[i];
		}
		else
		{
			j = i;
		}

		if (j >= 0 && seqdesc.weight( j ) > 0.0)
		{
			QuaternionIdentityBlend( q1[i], s1, q1[i] );
			VectorScale( pos1[i], s2, pos1[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: resolve a global pose parameter to the specific setting for this sequence
//-----------------------------------------------------------------------------
int Studio_LocalPoseParameter( const CStudioHdr *pStudioHdr, const float poseParameter[], mstudioseqdesc_t &seqdesc, int iSequence, int iLocalIndex, float &flSetting )
{
	BONE_PROFILE_FUNC();
	int iPose = pStudioHdr->GetSharedPoseParameter( iSequence, seqdesc.paramindex[iLocalIndex] );

	if (iPose == -1)
	{
		flSetting = 0;
		return 0;
	}

	const mstudioposeparamdesc_t &Pose = ((CStudioHdr *)pStudioHdr)->pPoseParameter( iPose );

	float flValue = poseParameter[iPose];

	if (Pose.loop)
	{
		float wrap = (Pose.start + Pose.end) / 2.0 + Pose.loop / 2.0;
		float shift = Pose.loop - wrap;

		flValue = flValue - Pose.loop * floor((flValue + shift) / Pose.loop);
	}

	int nIndex = 0;
	if (seqdesc.posekeyindex == 0)
	{
		float flLocalStart	= ((float)seqdesc.paramstart[iLocalIndex] - Pose.start) / (Pose.end - Pose.start);
		float flLocalEnd	= ((float)seqdesc.paramend[iLocalIndex] - Pose.start) / (Pose.end - Pose.start);

		// convert into local range
		flSetting = (flValue - flLocalStart) / (flLocalEnd - flLocalStart);

		// clamp.  This shouldn't ever need to happen if it's looping.
		if (flSetting < 0)
			flSetting = 0;
		if (flSetting > 1)
			flSetting = 1;

		nIndex = 0;
		if (seqdesc.groupsize[iLocalIndex] > 2 )
		{
			// estimate index
			nIndex = (int)(flSetting * (seqdesc.groupsize[iLocalIndex] - 1));
			if (nIndex == seqdesc.groupsize[iLocalIndex] - 1) 
			{
				nIndex = seqdesc.groupsize[iLocalIndex] - 2;
			}
			flSetting = flSetting * (seqdesc.groupsize[iLocalIndex] - 1) - nIndex;
		}
	}
	else
	{
		flValue = flValue * (Pose.end - Pose.start) + Pose.start;
		nIndex = 0;
			
		// FIXME: this needs to be 2D
		// FIXME: this shouldn't be a linear search

		while (1)
		{
			flSetting = (flValue - seqdesc.poseKey( iLocalIndex, nIndex )) / (seqdesc.poseKey( iLocalIndex, nIndex + 1 ) - seqdesc.poseKey( iLocalIndex, nIndex ));
			/*
			if (index > 0 && flSetting < 0.0)
			{
				index--;
				continue;
			}
			else 
			*/
			if (nIndex < seqdesc.groupsize[iLocalIndex] - 2 && flSetting > 1.0)
			{
				nIndex++;
				continue;
			}
			break;
		}

		// clamp.
		if (flSetting < 0.0f)
			flSetting = 0.0f;
		if (flSetting > 1.0f)
			flSetting = 1.0f;
	}
	return nIndex;
}

void Studio_CalcBoneToBoneTransform( const CStudioHdr *pStudioHdr, int inputBoneIndex, int outputBoneIndex, matrix3x4_t& matrixOut )
{
	const mstudiobone_t *pbone = pStudioHdr->pBone( inputBoneIndex );

	matrix3x4a_t inputToPose;
	MatrixInvert( pbone->poseToBone, inputToPose );
	ConcatTransforms( pStudioHdr->pBone( outputBoneIndex )->poseToBone, inputToPose, matrixOut );
}


//-----------------------------------------------------------------------------
// Purpose:  Lookup a bone controller
//-----------------------------------------------------------------------------



static mstudiobonecontroller_t* FindController( const CStudioHdr *pStudioHdr, int iController)
{
	// find first controller that matches the index
	for (int i = 0; i < pStudioHdr->numbonecontrollers(); i++)
	{
		if (pStudioHdr->pBonecontroller( i )->inputfield == iController)
			return pStudioHdr->pBonecontroller( i );
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: converts a ranged bone controller value into a 0..1 encoded value
// Output: 	ctlValue contains 0..1 encoding.
//			returns clamped ranged value
//-----------------------------------------------------------------------------

float Studio_SetController( const CStudioHdr *pStudioHdr, int iController, float flValue, float &ctlValue )
{
	BONE_PROFILE_FUNC();
	if (! pStudioHdr)
		return flValue;

	mstudiobonecontroller_t *pbonecontroller = FindController(pStudioHdr, iController);
	if(!pbonecontroller)
	{
		ctlValue = 0;
		return flValue;
	}

	// wrap 0..360 if it's a rotational controller
	if (pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		// ugly hack, invert value if end < start
		if (pbonecontroller->end < pbonecontroller->start)
			flValue = -flValue;

		// does the controller not wrap?
		if (pbonecontroller->start + 359.0 >= pbonecontroller->end)
		{
			if (flValue > ((pbonecontroller->start + pbonecontroller->end) / 2.0) + 180)
				flValue = flValue - 360;
			if (flValue < ((pbonecontroller->start + pbonecontroller->end) / 2.0) - 180)
				flValue = flValue + 360;
		}
		else
		{
			if (flValue > 360)
				flValue = flValue - (int)(flValue / 360.0) * 360.0;
			else if (flValue < 0)
				flValue = flValue + (int)((flValue / -360.0) + 1) * 360.0;
		}
	}

	ctlValue = (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start);
	if (ctlValue < 0) ctlValue = 0;
	if (ctlValue > 1) ctlValue = 1;

	float flReturnVal = ((1.0 - ctlValue)*pbonecontroller->start + ctlValue *pbonecontroller->end);

	// ugly hack, invert value if a rotational controller and end < start
	if (pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR) &&
		pbonecontroller->end < pbonecontroller->start				)
	{
		flReturnVal *= -1;
	}
	
	return flReturnVal;
}


//-----------------------------------------------------------------------------
// Purpose: converts a 0..1 encoded bone controller value into a ranged value
// Output: 	returns ranged value
//-----------------------------------------------------------------------------

float Studio_GetController( const CStudioHdr *pStudioHdr, int iController, float ctlValue )
{
	if (!pStudioHdr)
		return 0.0;

	mstudiobonecontroller_t *pbonecontroller = FindController(pStudioHdr, iController);
	if(!pbonecontroller)
		return 0;

	return ctlValue * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}


//-----------------------------------------------------------------------------
// Purpose: Calculates default values for the pose parameters
// Output: 	fills in an array
//-----------------------------------------------------------------------------

void Studio_CalcDefaultPoseParameters( const CStudioHdr *pStudioHdr, float flPoseParameter[], int nCount )
{
	int nPoseCount = pStudioHdr->GetNumPoseParameters();
	int nNumParams = MIN( nCount, MAXSTUDIOPOSEPARAM );

	for ( int i = 0; i < nNumParams; ++i )
	{
		// Default to middle of the pose parameter range
		flPoseParameter[ i ] = 0.5f;
		if ( i < nPoseCount )
		{
			const mstudioposeparamdesc_t &Pose = ((CStudioHdr *)pStudioHdr)->pPoseParameter( i );

			// Want to try for a zero state.  If one doesn't exist set it to .5 by default.
			if ( Pose.start < 0.0f && Pose.end > 0.0f )
			{
				float flPoseDelta = Pose.end - Pose.start;
				flPoseParameter[i] = -Pose.start / flPoseDelta;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: converts a ranged pose parameter value into a 0..1 encoded value
// Output: 	ctlValue contains 0..1 encoding.
//			returns clamped ranged value
//-----------------------------------------------------------------------------

float Studio_SetPoseParameter( const CStudioHdr *pStudioHdr, int iParameter, float flValue, float &ctlValue )
{
	if (iParameter < 0 || iParameter >= pStudioHdr->GetNumPoseParameters())
	{
		ctlValue = 0;
		return 0;
	}

	const mstudioposeparamdesc_t &PoseParam = ((CStudioHdr *)pStudioHdr)->pPoseParameter( iParameter );

	Assert( IsFinite( flValue ) );

	if (PoseParam.loop)
	{
		float wrap = (PoseParam.start + PoseParam.end) / 2.0 + PoseParam.loop / 2.0;
		float shift = PoseParam.loop - wrap;

		flValue = flValue - PoseParam.loop * floor((flValue + shift) / PoseParam.loop);
	}

	ctlValue = (flValue - PoseParam.start) / (PoseParam.end - PoseParam.start);

	if (ctlValue < 0) ctlValue = 0;
	if (ctlValue > 1) ctlValue = 1;

	Assert( IsFinite( ctlValue ) );

	return ctlValue * (PoseParam.end - PoseParam.start) + PoseParam.start;
}


//-----------------------------------------------------------------------------
// Purpose: converts a 0..1 encoded pose parameter value into a ranged value
// Output: 	returns ranged value
//-----------------------------------------------------------------------------

float Studio_GetPoseParameter( const CStudioHdr *pStudioHdr, int iParameter, float ctlValue )
{
	if (iParameter < 0 || iParameter >= pStudioHdr->GetNumPoseParameters())
	{
		return 0;
	}

	const mstudioposeparamdesc_t &PoseParam = ((CStudioHdr *)pStudioHdr)->pPoseParameter( iParameter );

	return ctlValue * (PoseParam.end - PoseParam.start) + PoseParam.start;
}


#pragma warning (disable : 4701)

static int ClipRayToCapsule( const Ray_t &ray, mstudiobbox_t *pbox, matrix3x4_t& matrix, trace_t &tr )
{
	BONE_PROFILE_FUNC();

	Vector vecCapsuleCenters[ 2 ];
	VectorTransform( pbox->bbmin, matrix, vecCapsuleCenters[0] );
	VectorTransform( pbox->bbmax, matrix, vecCapsuleCenters[1] );

	CShapeCastResult cast;
	Assert( tr.fraction >= 0 && tr.fraction <= 1.0f );
	CastCapsuleRay( cast, ray.m_Start /*+start offset?*/, ray.m_Delta * tr.fraction, vecCapsuleCenters, pbox->flCapsuleRadius );
	if ( cast.DidHit() )
	{
		tr.fraction *= cast.m_flHitTime;
		if ( cast.m_bStartInSolid )
		{
			tr.startsolid = true;
			// tr.allsolid - not computed yet
		}

		// tr.contents, dispFlags - not computed yet
		tr.endpos = cast.m_vHitPoint;
		tr.plane.normal = cast.m_vHitNormal;

		//extern IVDebugOverlay *debugoverlay;
		//debugoverlay->AddCapsuleOverlay( vecCapsuleCenters[ 0 ], vecCapsuleCenters[ 1 ], pbox->flCapsuleRadius, 0, 255, 0, 255, 10 );
		//debugoverlay->AddLineOverlay( ray.m_Start /*+offset?*/, cast.m_vHitPoint, 0, 0, 255, 200, 0.25f, 10 );
		//debugoverlay->AddLineOverlay( cast.m_vHitPoint, cast.m_vHitPoint + 4 * cast.m_vHitNormal, 0, 255, 0, 200, 0.25f, 10 );

		// plane.dist and others are not computed yet
		return 0; // hitside is not computed (yet?)
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
static int ClipRayToHitbox( const Ray_t &ray, mstudiobbox_t *pbox, matrix3x4_t& matrix, trace_t &tr )
{
	const float flProjEpsilon = 0.01f;
	BONE_PROFILE_FUNC();

	if ( pbox->flCapsuleRadius > 0 )
	{
		return ClipRayToCapsule( ray, pbox, matrix, tr );
	}

	// scale by current t so hits shorten the ray and increase the likelihood of early outs
	Vector delta2;
	VectorScale( ray.m_Delta, (0.5f * tr.fraction), delta2 );

	// OPTIMIZE: Store this in the box instead of computing it here
	// compute center in local space
	Vector boxextents;
	boxextents.x = (pbox->bbmin.x + pbox->bbmax.x) * 0.5; 
	boxextents.y = (pbox->bbmin.y + pbox->bbmax.y) * 0.5; 
	boxextents.z = (pbox->bbmin.z + pbox->bbmax.z) * 0.5; 
	
	// transform to world space
	Vector boxCenter;
	VectorTransform( boxextents, matrix, boxCenter );

	// calc extents from local center
	boxextents.x = pbox->bbmax.x - boxextents.x;
	boxextents.y = pbox->bbmax.y - boxextents.y;
	boxextents.z = pbox->bbmax.z - boxextents.z;
	
	// OPTIMIZE: This is optimized for world space.  If the transform is fast enough, it may make more
	// sense to just xform and call UTIL_ClipToBox() instead.  MEASURE THIS.

	// save the extents of the ray along 
	Vector extent, uextent;
	Vector segmentCenter;
	segmentCenter.x = ray.m_Start.x + delta2.x - boxCenter.x;
	segmentCenter.y = ray.m_Start.y + delta2.y - boxCenter.y;
	segmentCenter.z = ray.m_Start.z + delta2.z - boxCenter.z;

	extent.Init();

	// check box axes for separation
	for ( int j = 0; j < 3; j++ )
	{
		extent[j] = delta2.x * matrix[0][j] + delta2.y * matrix[1][j] +	delta2.z * matrix[2][j];
		uextent[j] = fabsf(extent[j]);
		float coord = segmentCenter.x * matrix[0][j] + segmentCenter.y * matrix[1][j] +	segmentCenter.z * matrix[2][j];
		coord = fabsf(coord);

		if ( coord > (boxextents[j] + uextent[j]) )
			return -1;
	}

	// now check cross axes for separation
	float tmp, cextent;
	Vector cross;
	CrossProduct( delta2, segmentCenter, cross );
	cextent = cross.x * matrix[0][0] + cross.y * matrix[1][0] + cross.z * matrix[2][0];
	cextent = fabsf(cextent);
	tmp = boxextents[1]*uextent[2] + boxextents[2]*uextent[1];
	tmp = MAX(tmp, flProjEpsilon);
	if ( cextent > tmp )
		return -1;

	cextent = cross.x * matrix[0][1] + cross.y * matrix[1][1] + cross.z * matrix[2][1];
	cextent = fabsf(cextent);
	tmp = boxextents[0]*uextent[2] + boxextents[2]*uextent[0];
	tmp = MAX(tmp, flProjEpsilon);
	if ( cextent > tmp )
		return -1;

	cextent = cross.x * matrix[0][2] + cross.y * matrix[1][2] + cross.z * matrix[2][2];
	cextent = fabsf(cextent);
	tmp = boxextents[0]*uextent[1] + boxextents[1]*uextent[0];
	tmp = MAX(tmp, flProjEpsilon);
	if ( cextent > tmp )
		return -1;

	Vector start;

	// Compute ray start in bone space
	VectorITransform( ray.m_Start, matrix, start );
	// extent is delta2 in bone space, recompute delta in bone space
	VectorScale( extent, 2, extent );

	// delta was prescaled by the current t, so no need to see if this intersection
	// is closer
	trace_t boxTrace;
	if ( !IntersectRayWithBox( start, extent, pbox->bbmin, pbox->bbmax, 0.0f, &boxTrace ) )
		return -1;

	Assert( IsFinite(boxTrace.fraction) );
	tr.fraction *= boxTrace.fraction;
	tr.startsolid = boxTrace.startsolid;
	int hitside = boxTrace.plane.type;
	if ( boxTrace.plane.normal[hitside] >= 0 )
	{
		hitside += 3;
	}
	return hitside;
}

#pragma warning (default : 4701)


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool SweepBoxToStudio( IPhysicsSurfaceProps *pProps, const Ray_t& ray, CStudioHdr *pStudioHdr, mstudiohitboxset_t *set, 
				   matrix3x4_t **hitboxbones, int fContentsMask, trace_t &tr )
{
	BONE_PROFILE_FUNC();
	tr.fraction = 1.0;
	tr.startsolid = false;

	// OPTIMIZE: Partition these?
	Ray_t clippedRay = ray;
	int hitbox = -1;
	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		mstudiobbox_t *pbox = set->pHitbox(i);

		// Filter based on contents mask
		int fBoneContents = pStudioHdr->pBone( pbox->bone )->contents;
		if ( ( fBoneContents & fContentsMask ) == 0 )
			continue;
		
		//FIXME: Won't work with scaling!
		trace_t obbTrace;
		if ( IntersectRayWithOBB( clippedRay, *hitboxbones[pbox->bone], pbox->bbmin, pbox->bbmax, 0.0f, &obbTrace ) )
		{
			tr.startpos = obbTrace.startpos;
			tr.endpos = obbTrace.endpos;
			tr.plane = obbTrace.plane;
			tr.startsolid = obbTrace.startsolid;
			tr.allsolid = obbTrace.allsolid;

			// This logic here is to shorten the ray each time to get more early outs
			tr.fraction *= obbTrace.fraction;
			clippedRay.m_Delta *= obbTrace.fraction;
			hitbox = i;
			if (tr.startsolid)
				break;
		}
	}

	if ( hitbox >= 0 )
	{
		tr.hitgroup = set->pHitbox(hitbox)->group;
		tr.hitbox = hitbox;
		const mstudiobone_t *pBone = pStudioHdr->pBone( set->pHitbox(hitbox)->bone );
		tr.contents = pBone->contents | CONTENTS_HITBOX;
		tr.physicsbone = pBone->physicsbone;
		tr.surface.name = "**studio**";
		tr.surface.flags = SURF_HITBOX;
		tr.surface.surfaceProps = pBone->GetSurfaceProp();

		Assert( tr.physicsbone >= 0 );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool TraceToStudio( IPhysicsSurfaceProps *pProps, const Ray_t& ray, CStudioHdr *pStudioHdr, mstudiohitboxset_t *set, 
				   matrix3x4_t **hitboxbones, int fContentsMask, const Vector &vecOrigin, float flScale, trace_t &tr )
{
	BONE_PROFILE_FUNC();
	if ( !ray.m_IsRay )
	{
		return SweepBoxToStudio( pProps, ray, pStudioHdr, set, hitboxbones, fContentsMask, tr );
	}

	tr.fraction = 1.0;
	tr.startsolid = false;

	// no hit yet
	int hitbox = -1;
	int hitside = -1;

	// OPTIMIZE: Partition these?
	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		mstudiobbox_t *pbox = set->pHitbox(i);

		// Filter based on contents mask
		int fBoneContents = pStudioHdr->pBone( pbox->bone )->contents;
		if ( ( fBoneContents & fContentsMask ) == 0 )
			continue;
		
		// columns are axes of the bones in world space, translation is in world space
		matrix3x4_t& matrix = *hitboxbones[pbox->bone];
		
		// Because we're sending in a matrix with scale data, and because the matrix inversion in the hitbox
		// code does not handle that case, we pre-scale the bones and ray down here and do our collision checks
		// in unscaled space.  We can then rescale the results afterwards.

		int side = -1;
		if ( flScale < 1.0f-FLT_EPSILON || flScale > 1.0f+FLT_EPSILON )
		{
			matrix3x4_t matScaled;
			MatrixCopy( matrix, matScaled );
			
			float invScale = 1.0f / flScale;

			Vector vecBoneOrigin;
			MatrixGetColumn( matScaled, 3, vecBoneOrigin );
			
			// Pre-scale the origin down
			Vector vecNewOrigin = vecBoneOrigin - vecOrigin;
			vecNewOrigin *= invScale;
			vecNewOrigin += vecOrigin;
			MatrixSetColumn( vecNewOrigin, 3, matScaled );

			// Scale it uniformly
			VectorScale( matScaled[0], invScale, matScaled[0] );
			VectorScale( matScaled[1], invScale, matScaled[1] );
			VectorScale( matScaled[2], invScale, matScaled[2] );
			
			// Pre-scale our ray as well
			Vector vecRayStart = ray.m_Start - vecOrigin;
			vecRayStart *= invScale;
			vecRayStart += vecOrigin;
			
			Vector vecRayDelta = ray.m_Delta * invScale;

			Ray_t newRay;
			newRay.Init( vecRayStart, vecRayStart + vecRayDelta );  
			
			side = ClipRayToHitbox( newRay, pbox, matScaled, tr );
		}
		else
		{
			side = ClipRayToHitbox( ray, pbox, matrix, tr );
		}

		if ( side >= 0 )
		{
			hitbox = i;
			hitside = side;
		}
	}

	if ( hitbox >= 0 )
	{
		mstudiobbox_t *pbox = set->pHitbox(hitbox);
		VectorMA( ray.m_Start, tr.fraction, ray.m_Delta, tr.endpos );
		tr.hitgroup = set->pHitbox(hitbox)->group;
		tr.hitbox = hitbox;
		const mstudiobone_t *pBone = pStudioHdr->pBone( pbox->bone );
		tr.contents = pBone->contents | CONTENTS_HITBOX;
		tr.physicsbone = pBone->physicsbone;
		tr.surface.name = "**studio**";
		tr.surface.flags = SURF_HITBOX;
		tr.surface.surfaceProps = pBone->GetSurfaceProp();

		Assert( tr.physicsbone >= 0 );
		matrix3x4_t& matrix = *hitboxbones[pbox->bone];
		if ( hitside >= 3 )
		{
			hitside -= 3;
			tr.plane.normal[0] = matrix[0][hitside];
			tr.plane.normal[1] = matrix[1][hitside];
			tr.plane.normal[2] = matrix[2][hitside];
			//tr.plane.dist = DotProduct( tr.plane.normal, Vector(matrix[0][3], matrix[1][3], matrix[2][3] ) ) + pbox->bbmax[hitside];
		}
		else
		{
			tr.plane.normal[0] = -matrix[0][hitside];
			tr.plane.normal[1] = -matrix[1][hitside];
			tr.plane.normal[2] = -matrix[2][hitside];
			//tr.plane.dist = DotProduct( tr.plane.normal, Vector(matrix[0][3], matrix[1][3], matrix[2][3] ) ) - pbox->bbmin[hitside];
		}
		// simpler plane constant equation
		tr.plane.dist = DotProduct( tr.endpos, tr.plane.normal );
		tr.plane.type = 3;
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool TraceToStudioCsgoHitgroupsPriority( IPhysicsSurfaceProps *pProps, const Ray_t& ray, CStudioHdr *pStudioHdr, mstudiohitboxset_t *set, 
	matrix3x4_t **hitboxbones, int fContentsMask, const Vector &vecOrigin, float flScale, trace_t &tr )
{
	BONE_PROFILE_FUNC();
	if ( !ray.m_IsRay )
	{
		return SweepBoxToStudio( pProps, ray, pStudioHdr, set, hitboxbones, fContentsMask, tr );
	}

	tr.fraction = 1.0;
	tr.startsolid = false;

	//
	// We will collect trace results depending on hit group type of hitboxes
	// and prefer to hit the hitboxes in order of damage.
	//
	enum EHitGroupType_t
	{
		k_EHitGroupType_Head,
		k_EHitGroupType_Stomach,
		k_EHitGroupType_Chest,
		k_EHitGroupType_Arms,
		k_EHitGroupType_General,
		k_EHitGroupType_Legs,
		k_EHitGroupType_Count
	};

	struct HitGroupResult_t
	{
		trace_t m_trHitGroup;
		int m_nHitbox; // index of the hitbox hit, -1 if no it
		int m_nHitSide; // hit side
	};

	// We'll collect results here, initialize to nothing hit
	HitGroupResult_t arrHitGroupResults[ k_EHitGroupType_Count ];
	for ( int j = 0; j < Q_ARRAYSIZE( arrHitGroupResults ); ++ j )
	{
		Q_memcpy( &arrHitGroupResults[j].m_trHitGroup, &tr, sizeof( arrHitGroupResults[j].m_trHitGroup ) );
		arrHitGroupResults[j].m_nHitbox = -1;
		arrHitGroupResults[j].m_nHitSide = -1;
	}

	// OPTIMIZE: Partition these?
	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		mstudiobbox_t *pbox = set->pHitbox(i);

		// Filter based on contents mask
		int fBoneContents = pStudioHdr->pBone( pbox->bone )->contents;
		if ( ( fBoneContents & fContentsMask ) == 0 )
			continue;

		// Collect the results into appropriate hitgroup bucket
		HitGroupResult_t *pHitGroupResult = &arrHitGroupResults[ k_EHitGroupType_General ];
		switch ( pbox->group )
		{
		case 1:
			pHitGroupResult = &arrHitGroupResults[ k_EHitGroupType_Head ];
			break;
		case 3:
			pHitGroupResult = &arrHitGroupResults[ k_EHitGroupType_Stomach ];
			break;
		case 2:
			pHitGroupResult = &arrHitGroupResults[ k_EHitGroupType_Chest ];
			break;
		case 4:
		case 5:
			pHitGroupResult = &arrHitGroupResults[ k_EHitGroupType_Arms ];
			break;
		case 6:
		case 7:
			pHitGroupResult = &arrHitGroupResults[ k_EHitGroupType_Legs ];
			break;
		}
		Assert( IsFinite( pHitGroupResult->m_trHitGroup.fraction ) );

		// columns are axes of the bones in world space, translation is in world space
		matrix3x4_t& matrix = *hitboxbones[pbox->bone];

		// Because we're sending in a matrix with scale data, and because the matrix inversion in the hitbox
		// code does not handle that case, we pre-scale the bones and ray down here and do our collision checks
		// in unscaled space.  We can then rescale the results afterwards.

		int side = -1;
		if ( flScale < 1.0f-FLT_EPSILON || flScale > 1.0f+FLT_EPSILON )
		{
			matrix3x4_t matScaled;
			MatrixCopy( matrix, matScaled );

			matrix3x4_t matOrientation;
			AngleMatrix(pbox->angOffsetOrientation, matOrientation);
			MatrixMultiply(matScaled, matOrientation, matScaled);

			float invScale = 1.0f / flScale;

			Vector vecBoneOrigin;
			MatrixGetColumn( matScaled, 3, vecBoneOrigin );

			// Pre-scale the origin down
			Vector vecNewOrigin = vecBoneOrigin - vecOrigin;
			vecNewOrigin *= invScale;
			vecNewOrigin += vecOrigin;
			MatrixSetColumn( vecNewOrigin, 3, matScaled );

			// Scale it uniformly
			VectorScale( matScaled[0], invScale, matScaled[0] );
			VectorScale( matScaled[1], invScale, matScaled[1] );
			VectorScale( matScaled[2], invScale, matScaled[2] );

			// Pre-scale our ray as well
			Vector vecRayStart = ray.m_Start - vecOrigin;
			vecRayStart *= invScale;
			vecRayStart += vecOrigin;

			Vector vecRayDelta = ray.m_Delta * invScale;

			Ray_t newRay;
			newRay.Init( vecRayStart, vecRayStart + vecRayDelta );  

			side = ClipRayToHitbox( newRay, pbox, matScaled, pHitGroupResult->m_trHitGroup );
		}
		else
		{

			matrix3x4_t matCopy;
			MatrixCopy( matrix, matCopy );

			matrix3x4_t matOrientation;
			AngleMatrix(pbox->angOffsetOrientation, matOrientation);
			MatrixMultiply(matCopy, matOrientation, matCopy);

			side = ClipRayToHitbox( ray, pbox, matCopy, pHitGroupResult->m_trHitGroup );
		}
		Assert( IsFinite( pHitGroupResult->m_trHitGroup.fraction ) );

		if ( side >= 0 )
		{
			pHitGroupResult->m_nHitbox = i;
			pHitGroupResult->m_nHitSide = side;
		}
	}

	//
	// Now based on bucketing hitbox group results determine which hitbox we will return
	// and copy the trace results to the output parameter.
	//
	int hitbox = -1;
	int hitside = -1;
	// CSGO specific hitbox computation - characters' neck hitbox is classified as a headshot, but
	// it deeply interpenetrates the chest. We don't want players shooting at the middle of the chest
	// to register a headshot by penetrating into neck through chest or stomach, so if we have a
	// headshot trace make sure that it doesn't occur by penetrating chest or stomach.
	if ( arrHitGroupResults[k_EHitGroupType_Head].m_nHitbox >= 0 )
	{
		// We have a potential headshot, check if it's penetrating via stomach or chest
		for ( int j = k_EHitGroupType_Stomach; j <= k_EHitGroupType_Chest; ++ j )
		{
			if ( arrHitGroupResults[j].m_trHitGroup.fraction < arrHitGroupResults[k_EHitGroupType_Head].m_trHitGroup.fraction )
			{
				// The bullet first hit the stomach/chest hitbox, so ignore the headshot
				arrHitGroupResults[k_EHitGroupType_Head].m_nHitbox = -1;
				break;
			}
		}
	}
	// Now pick the hitbox hit with the highest priority for damage
	for ( int j = 0; j < Q_ARRAYSIZE( arrHitGroupResults ); ++ j )
	{
		if ( arrHitGroupResults[j].m_nHitbox >= 0 )
		{
			hitbox = arrHitGroupResults[j].m_nHitbox;
			hitside = arrHitGroupResults[j].m_nHitSide;
			Q_memcpy( &tr, &arrHitGroupResults[j].m_trHitGroup, sizeof( arrHitGroupResults[j].m_trHitGroup ) );
			break;
		}
	}

	if ( hitbox >= 0 )
	{
		mstudiobbox_t *pbox = set->pHitbox(hitbox);
		VectorMA( ray.m_Start, tr.fraction, ray.m_Delta, tr.endpos );
		tr.hitgroup = set->pHitbox(hitbox)->group;
		tr.hitbox = hitbox;
		const mstudiobone_t *pBone = pStudioHdr->pBone( pbox->bone );
		tr.contents = pBone->contents | CONTENTS_HITBOX;
		tr.physicsbone = pBone->physicsbone;
		tr.surface.name = "**studio**";
		tr.surface.flags = SURF_HITBOX;
		tr.surface.surfaceProps = pBone->GetSurfaceProp();

		Assert( tr.physicsbone >= 0 );
		matrix3x4_t& matrix = *hitboxbones[pbox->bone];
		if ( hitside >= 3 )
		{
			hitside -= 3;
			tr.plane.normal[0] = matrix[0][hitside];
			tr.plane.normal[1] = matrix[1][hitside];
			tr.plane.normal[2] = matrix[2][hitside];
			//tr.plane.dist = DotProduct( tr.plane.normal, Vector(matrix[0][3], matrix[1][3], matrix[2][3] ) ) + pbox->bbmax[hitside];
		}
		else
		{
			tr.plane.normal[0] = -matrix[0][hitside];
			tr.plane.normal[1] = -matrix[1][hitside];
			tr.plane.normal[2] = -matrix[2][hitside];
			//tr.plane.dist = DotProduct( tr.plane.normal, Vector(matrix[0][3], matrix[1][3], matrix[2][3] ) ) - pbox->bbmin[hitside];
		}
		// simpler plane constant equation
		tr.plane.dist = DotProduct( tr.endpos, tr.plane.normal );
		tr.plane.type = 3;
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
/**
 * TERROR: Version of TraceToStudio that favors certain high-damage hitgroups such as the head
 */
bool TraceToStudioGrouped( IPhysicsSurfaceProps *pProps, const Ray_t& ray, CStudioHdr *pStudioHdr, mstudiohitboxset_t *set, 
				   matrix3x4_t **hitboxbones, int fContentsMask, trace_t &tr, const CUtlVector< int > &sortedHitgroups )
{
	BONE_PROFILE_FUNC();
	if ( !ray.m_IsRay )
	{
		return SweepBoxToStudio( pProps, ray, pStudioHdr, set, hitboxbones, fContentsMask, tr );
	}

	tr.fraction = 1.0;
	tr.startsolid = false;

	// no hit yet
	int hitbox = -1;
	int hitside = -1;

	for ( int n=0; n<sortedHitgroups.Count(); ++n )
	{
		// OPTIMIZE: Partition these?
		for ( int i = 0; i < set->numhitboxes; i++ )
		{
			mstudiobbox_t *pbox = set->pHitbox(i);
			if ( pbox->group != sortedHitgroups[n] )
				continue;

			// Filter based on contents mask
			int fBoneContents = pStudioHdr->pBone( pbox->bone )->contents;
			if ( ( fBoneContents & fContentsMask ) == 0 )
				continue;

			// columns are axes of the bones in world space, translation is in world space
			matrix3x4_t& matrix = *hitboxbones[pbox->bone];

			int side = ClipRayToHitbox( ray, pbox, matrix, tr );
			if ( side >= 0 )
			{
				hitbox = i;
				hitside = side;
			}
		}

		// If a high damage hitgroup was traced, stop here (ignore closer, lower-damage hitgroups)
		if ( hitbox >= 0 )
		{
			break;
		}
	}

	if ( hitbox >= 0 )
	{
		mstudiobbox_t *pbox = set->pHitbox(hitbox);
		VectorMA( ray.m_Start, tr.fraction, ray.m_Delta, tr.endpos );
		tr.hitgroup = set->pHitbox(hitbox)->group;
		tr.hitbox = hitbox;
		const mstudiobone_t *pBone = pStudioHdr->pBone( pbox->bone );
		tr.contents = pBone->contents | CONTENTS_HITBOX;
		tr.physicsbone = pBone->physicsbone;
		tr.surface.surfaceProps = pBone->GetSurfaceProp();
		tr.surface.name = "**studio**";
		tr.surface.flags = SURF_HITBOX;
		Assert( tr.physicsbone >= 0 );
		matrix3x4_t& matrix = *hitboxbones[pbox->bone];
		if ( hitside >= 3 )
		{
			hitside -= 3;
			tr.plane.normal[0] = matrix[0][hitside];
			tr.plane.normal[1] = matrix[1][hitside];
			tr.plane.normal[2] = matrix[2][hitside];
			//tr.plane.dist = DotProduct( tr.plane.normal, Vector(matrix[0][3], matrix[1][3], matrix[2][3] ) ) + pbox->bbmax[hitside];
		}
		else
		{
			tr.plane.normal[0] = -matrix[0][hitside];
			tr.plane.normal[1] = -matrix[1][hitside];
			tr.plane.normal[2] = -matrix[2][hitside];
			//tr.plane.dist = DotProduct( tr.plane.normal, Vector(matrix[0][3], matrix[1][3], matrix[2][3] ) ) - pbox->bbmin[hitside];
		}
		// simpler plane constant equation
		tr.plane.dist = DotProduct( tr.endpos, tr.plane.normal );
		tr.plane.type = 3;
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns array of animations and weightings for a sequence based on current pose parameters
//-----------------------------------------------------------------------------

void Studio_SeqAnims( const CStudioHdr *pStudioHdr, mstudioseqdesc_t &seqdesc, int iSequence, const float poseParameter[], mstudioanimdesc_t *panim[4], float *weight )
{
	BONE_PROFILE_FUNC();
#if _DEBUG
	VPROF_INCREMENT_COUNTER("SEQ_ANIMS",1);
#endif
	if (!pStudioHdr || iSequence >= pStudioHdr->GetNumSeq())
	{
		weight[0] = weight[1] = weight[2] = weight[3] = 0.0;
		return;
	}

	float s0 = 0, s1 = 0;
	
	int i0 = Studio_LocalPoseParameter( pStudioHdr, poseParameter, seqdesc, iSequence, 0, s0 );
	int i1 = Studio_LocalPoseParameter( pStudioHdr, poseParameter, seqdesc, iSequence, 1, s1 );

	panim[0] = &((CStudioHdr *)pStudioHdr)->pAnimdesc( ((CStudioHdr *)pStudioHdr)->iRelativeAnim( iSequence, seqdesc.anim( i0  , i1 ) ) );
	weight[0] = (1 - s0) * (1 - s1);

	panim[1] = &((CStudioHdr *)pStudioHdr)->pAnimdesc( ((CStudioHdr *)pStudioHdr)->iRelativeAnim( iSequence, seqdesc.anim( i0+1, i1 ) ) );
	weight[1] = (s0) * (1 - s1);

	panim[2] = &((CStudioHdr *)pStudioHdr)->pAnimdesc( ((CStudioHdr *)pStudioHdr)->iRelativeAnim( iSequence, seqdesc.anim( i0  , i1+1 ) ) );
	weight[2] = (1 - s0) * (s1);

	panim[3] = &((CStudioHdr *)pStudioHdr)->pAnimdesc( ((CStudioHdr *)pStudioHdr)->iRelativeAnim( iSequence, seqdesc.anim( i0+1, i1+1 ) ) );
	weight[3] = (s0) * (s1);

	Assert( weight[0] >= 0.0f && weight[1] >= 0.0f && weight[2] >= 0.0f && weight[3] >= 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: returns max frame number for a sequence
//-----------------------------------------------------------------------------

int Studio_MaxFrame( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[] )
{
	mstudioanimdesc_t *panim[4];
	float	weight[4];

	mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequence );
	Studio_SeqAnims( pStudioHdr, seqdesc, iSequence, poseParameter, panim, weight );

	float maxFrame = 0;
	for (int i = 0; i < 4; i++)
	{
		if (weight[i] > 0)
		{
			maxFrame += panim[i]->numframes * weight[i];
		}
	}

	if ( maxFrame > 1 )
		maxFrame -= 1;
	

	// FIXME: why does the weights sometimes not exactly add it 1.0 and this sometimes rounds down?
	return (maxFrame + 0.01);
}


//-----------------------------------------------------------------------------
// Purpose: returns frames per second of a sequence
//-----------------------------------------------------------------------------

float Studio_FPS( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[] )
{
	mstudioanimdesc_t *panim[4];
	float	weight[4];

	mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequence );
	Studio_SeqAnims( pStudioHdr, seqdesc, iSequence, poseParameter, panim, weight );

	float t = 0;

	for (int i = 0; i < 4; i++)
	{
		if (weight[i] > 0)
		{
			t += panim[i]->fps * weight[i];
		}
	}
	return t;
}


//-----------------------------------------------------------------------------
// Purpose: returns cycles per second of a sequence (cycles/second)
//-----------------------------------------------------------------------------

float Studio_CPS( const CStudioHdr *pStudioHdr, mstudioseqdesc_t &seqdesc, int iSequence, const float poseParameter[] )
{
	mstudioanimdesc_t *panim[4];
	float	weight[4];

	Studio_SeqAnims( pStudioHdr, seqdesc, iSequence, poseParameter, panim, weight );

	float t = 0;

	for (int i = 0; i < 4; i++)
	{
		if (weight[i] > 0 && panim[i]->numframes > 1)
		{
			t += (panim[i]->fps / (panim[i]->numframes - 1)) * weight[i];
		}
	}

	// FIXME: add support for more than just start 0 and end 0 pose param layers
	for (int j = 0; j < seqdesc.numautolayers; j++)
	{
		mstudioautolayer_t *pLayer = seqdesc.pAutolayer( j );

		if (pLayer->flags & STUDIO_AL_LOCAL)
			continue;

		float layerWeight = 0;

		int iSequenceLocal = pStudioHdr->iRelativeSeq( iSequence, pLayer->iSequence );

		if ( pLayer->start == 0 && pLayer->end == 0 && (pLayer->flags & STUDIO_AL_POSE) )
		{
			int iPose = pStudioHdr->GetSharedPoseParameter( iSequenceLocal, pLayer->iPose );
			if (iPose == -1)
				continue;
			
			const mstudioposeparamdesc_t &Pose = ((CStudioHdr *)pStudioHdr)->pPoseParameter( iPose );
			float s = poseParameter[ iPose ] * (Pose.end - Pose.start) + Pose.start;

			Assert( (pLayer->tail - pLayer->peak) != 0 );

			s = clamp( (s - pLayer->peak) / (pLayer->tail - pLayer->peak), 0, 1 );

			if (pLayer->flags & STUDIO_AL_SPLINE)
			{
				s = SimpleSpline( s );
			}

			layerWeight = seqdesc.weight(0) * s;
		}

		if ( layerWeight )
		{
			mstudioseqdesc_t &seqdescLocal = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequenceLocal );
			Studio_SeqAnims( pStudioHdr, seqdescLocal, iSequenceLocal, poseParameter, panim, weight );

			float flLocalT = 0;

			for (int i = 0; i < 4; i++)
			{
				if (weight[i] > 0 && panim[i]->numframes > 1)
				{
					flLocalT += (panim[i]->fps / (panim[i]->numframes - 1)) * weight[i];
				}
			}

			if ( flLocalT )
			{
				t = Lerp( layerWeight, t, flLocalT );
			}
		}
	}

	return t;
}

//-----------------------------------------------------------------------------
// Purpose: returns length (in seconds) of a sequence (seconds/cycle)
//-----------------------------------------------------------------------------

float Studio_Duration( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[] )
{
	mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequence );
	float cps = Studio_CPS( pStudioHdr, seqdesc, iSequence, poseParameter );

	if( cps == 0 )
		return 0.0f;

	return 1.0f/cps;
}


//-----------------------------------------------------------------------------
// Purpose: calculate changes in position and angle relative to the start of an animations cycle
// Output:	updated position and angle, relative to the origin
//			returns false if animation is not a movement animation
//-----------------------------------------------------------------------------

bool Studio_AnimPosition( mstudioanimdesc_t *panim, float flCycle, Vector &vecPos, QAngle &vecAngle )
{
	BONE_PROFILE_FUNC();
	float	prevframe = 0;
	vecPos.Init( );
	vecAngle.Init( );

	if (panim->nummovements == 0)
		return false;

	int iLoops = 0;
	if (flCycle > 1.0)
	{
		iLoops = (int)flCycle;
	}
	else if (flCycle < 0.0)
	{
		iLoops = (int)flCycle - 1;
	}
	flCycle = flCycle - iLoops;

	float	flFrame = flCycle * (panim->numframes - 1);

	for (int i = 0; i < panim->nummovements; i++)
	{
		mstudiomovement_t *pmove = panim->pMovement( i );

		if (pmove->endframe >= flFrame)
		{
			float f = (flFrame - prevframe) / (pmove->endframe - prevframe);

			float d = pmove->v0 * f + 0.5 * (pmove->v1 - pmove->v0) * f * f;

			vecPos = vecPos + d * pmove->vector;
			vecAngle.y = vecAngle.y * (1 - f) + pmove->angle * f;
			if (iLoops != 0)
			{
				mstudiomovement_t *pmove = panim->pMovement( panim->nummovements - 1 );
				vecPos = vecPos + iLoops * pmove->position; 
				vecAngle.y = vecAngle.y + iLoops * pmove->angle; 
			}
			return true;
		}
		else
		{
			prevframe = pmove->endframe;
			vecPos = pmove->position;
			vecAngle.y = pmove->angle;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: calculate instantaneous velocity in ips at a given point 
//			in the animations cycle
// Output:	velocity vector, relative to identity orientation
//			returns false if animation is not a movement animation
//-----------------------------------------------------------------------------

bool Studio_AnimVelocity( mstudioanimdesc_t *panim, float flCycle, Vector &vecVelocity )
{
	float	prevframe = 0;

	float	flFrame = flCycle * (panim->numframes - 1);
	flFrame = flFrame - (int)(flFrame / (panim->numframes - 1));

	for (int i = 0; i < panim->nummovements; i++)
	{
		mstudiomovement_t *pmove = panim->pMovement( i );

		if (pmove->endframe >= flFrame)
		{
			float f = (flFrame - prevframe) / (pmove->endframe - prevframe);

			float vel = pmove->v0 * (1 - f) + pmove->v1 * f;
			// scale from per block to per sec velocity
			vel = vel * panim->fps / (pmove->endframe - prevframe);

			vecVelocity = pmove->vector * vel;
			return true;
		}
		else
		{
			prevframe = pmove->endframe;
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: calculate changes in position and angle between two points in an animation cycle
// Output:	updated position and angle, relative to CycleFrom being at the origin
//			returns false if animation is not a movement animation
//-----------------------------------------------------------------------------

bool Studio_AnimMovement( mstudioanimdesc_t *panim, float flCycleFrom, float flCycleTo, Vector &deltaPos, QAngle &deltaAngle )
{
	if (panim->nummovements == 0)
		return false;

	Vector startPos;
	QAngle startA;
	Studio_AnimPosition( panim, flCycleFrom, startPos, startA );

	Vector endPos;
	QAngle endA;
	Studio_AnimPosition( panim, flCycleTo, endPos, endA );

	Vector tmp = endPos - startPos;
	deltaAngle.y = endA.y - startA.y;
	VectorYawRotate( tmp, -startA.y, deltaPos );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: finds how much of an animation to play to move given linear distance
//-----------------------------------------------------------------------------

float Studio_FindAnimDistance( mstudioanimdesc_t *panim, float flDist )
{
	float	prevframe = 0;

	if (flDist <= 0)
		return 0.0;

	for (int i = 0; i < panim->nummovements; i++)
	{
		mstudiomovement_t *pmove = panim->pMovement( i );

		float flMove = (pmove->v0 + pmove->v1) * 0.5;

		if (flMove >= flDist)
		{
			float root1, root2;

			// d = V0 * t + 1/2 (V1-V0) * t^2
			if (SolveQuadratic( 0.5 * (pmove->v1 - pmove->v0), pmove->v0, -flDist, root1, root2 ))
			{
				float cpf = 1.0 / (panim->numframes - 1);  // cycles per frame

				return (prevframe + root1 * (pmove->endframe - prevframe)) * cpf;
			}
			return 0.0;
		}
		else
		{
			flDist -= flMove;
			prevframe = pmove->endframe;
		}
	}
	return 1.0;
}


//-----------------------------------------------------------------------------
// Purpose: calculate changes in position and angle between two points in a sequences cycle
// Output:	updated position and angle, relative to CycleFrom being at the origin
//			returns false if sequence is not a movement sequence
//-----------------------------------------------------------------------------

bool Studio_SeqMovement( const CStudioHdr *pStudioHdr, int iSequence, float flCycleFrom, float flCycleTo, const float poseParameter[], Vector &deltaPos, QAngle &deltaAngles )
{
	mstudioanimdesc_t *panim[4];
	float	weight[4];

	mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequence );

	Studio_SeqAnims( pStudioHdr, seqdesc, iSequence, poseParameter, panim, weight );
	
	deltaPos.Init( );
	deltaAngles.Init( );

	bool found = false;

	for (int i = 0; i < 4; i++)
	{
		if (weight[i])
		{
			Vector localPos;
			QAngle localAngles;

			localPos.Init();
			localAngles.Init();

			if (Studio_AnimMovement( panim[i], flCycleFrom, flCycleTo, localPos, localAngles ))
			{
				found = true;
				deltaPos = deltaPos + localPos * weight[i];
				// FIXME: this makes no sense
				deltaAngles = deltaAngles + localAngles * weight[i];
			}
			else if (!(panim[i]->flags & STUDIO_DELTA) && panim[i]->nummovements == 0 && seqdesc.weight(0) > 0.0)
			{
				found = true;
			}
		}
	}

	// FIXME: add support for more than just start 0 and end 0 pose param layers (currently no cycle handling or angular delta)
	for (int j = 0; j < seqdesc.numautolayers; j++)
	{
		mstudioautolayer_t *pLayer = seqdesc.pAutolayer( j );

		if (pLayer->flags & STUDIO_AL_LOCAL)
			continue;

		float layerWeight = 0;

		int iSequenceLocal = pStudioHdr->iRelativeSeq( iSequence, pLayer->iSequence );

		if ( pLayer->start == 0 && pLayer->end == 0 && (pLayer->flags & STUDIO_AL_POSE) )
		{
			int iPose = pStudioHdr->GetSharedPoseParameter( iSequenceLocal, pLayer->iPose );
			if (iPose == -1)
				continue;
			
			const mstudioposeparamdesc_t &Pose = ((CStudioHdr *)pStudioHdr)->pPoseParameter( iPose );
			float s = poseParameter[ iPose ] * (Pose.end - Pose.start) + Pose.start;

			Assert( (pLayer->tail - pLayer->peak) != 0 );

			s = clamp( (s - pLayer->peak) / (pLayer->tail - pLayer->peak), 0, 1 );

			if (pLayer->flags & STUDIO_AL_SPLINE)
			{
				s = SimpleSpline( s );
			}

			layerWeight = seqdesc.weight(0) * s;
		}

		if ( layerWeight )
		{
			Vector layerPos;
			//QAngle layerAngles;
		
			layerPos.Init();
			//layerAngles.Init();

			bool bLayerFound = false;

			mstudioseqdesc_t &seqdescLocal = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequenceLocal );
			Studio_SeqAnims( pStudioHdr, seqdescLocal, iSequenceLocal, poseParameter, panim, weight );

			for (int i = 0; i < 4; i++)
			{
				if (weight[i])
				{
					Vector localPos;
					QAngle localAngles;

					localPos.Init();
					//localAngles.Init();

					if ( Studio_AnimMovement( panim[i], flCycleFrom, flCycleTo, localPos, localAngles ) )
					{
						bLayerFound = true;
						layerPos = layerPos + localPos * weight[i];
						// FIXME: do angles
						//layerAngles = layerAngles + localAngles * weight[i];
					}
				}
			}

			if ( bLayerFound )
			{
				deltaPos = Lerp( layerWeight, deltaPos, layerPos );
			}
		}
	}

	return found;
}


//-----------------------------------------------------------------------------
// Purpose: calculate changes in position and angle between two points in a sequences cycle
// Output:	updated position and angle, relative to CycleFrom being at the origin
//			returns false if sequence is not a movement sequence
//-----------------------------------------------------------------------------
float Studio_SeqMovementAndDuration( const CStudioHdr *pStudioHdr, int iSequence, float flCycleFrom, float flCycleTo, const float poseParameter[], Vector &deltaPos )
{
	mstudioanimdesc_t *panim[4];
	float	weight[4];

	mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequence );

	Studio_SeqAnims( pStudioHdr, seqdesc, iSequence, poseParameter, panim, weight );

	deltaPos.Init( );

	Vector localPos;
	QAngle localAngles;

	float t = 0;
	for ( int i = 0; i < 4; i++ )
	{
		if ( weight[i] == 0.0f )
			continue;

		if ( panim[i]->numframes > 1 )
		{
			t += ( panim[i]->fps / ( panim[i]->numframes - 1 ) ) * weight[i];
		}

		if ( Studio_AnimMovement( panim[i], flCycleFrom, flCycleTo, localPos, localAngles ) )
		{
			VectorMA( deltaPos, weight[i], localPos, deltaPos );
		}
	}
	return ( t != 0.0f ) ? 1.0f / t : 0.0f;
}


//-----------------------------------------------------------------------------
// Purpose: calculate instantaneous velocity in ips at a given point in the sequence's cycle
// Output:	velocity vector, relative to identity orientation
//			returns false if sequence is not a movement sequence
//-----------------------------------------------------------------------------

bool Studio_SeqVelocity( const CStudioHdr *pStudioHdr, int iSequence, float flCycle, const float poseParameter[], Vector &vecVelocity )
{
	mstudioanimdesc_t *panim[4];
	float	weight[4];

	mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequence );
	Studio_SeqAnims( pStudioHdr, seqdesc, iSequence, poseParameter, panim, weight );
	
	vecVelocity.Init( );

	bool found = false;

	for (int i = 0; i < 4; i++)
	{
		if (weight[i])
		{
			Vector vecLocalVelocity;

			if (Studio_AnimVelocity( panim[i], flCycle, vecLocalVelocity ))
			{
				vecVelocity = vecVelocity + vecLocalVelocity * weight[i];
				found = true;
			}
		}
	}
	return found;
}

//-----------------------------------------------------------------------------
// Purpose: finds how much of an sequence to play to move given linear distance
//-----------------------------------------------------------------------------

float Studio_FindSeqDistance( const CStudioHdr *pStudioHdr, int iSequence, const float poseParameter[], float flDist )
{
	mstudioanimdesc_t *panim[4];
	float	weight[4];

	mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequence );
	Studio_SeqAnims( pStudioHdr, seqdesc, iSequence, poseParameter, panim, weight );
	
	float flCycle = 0;

	for (int i = 0; i < 4; i++)
	{
		if (weight[i])
		{
			float flLocalCycle = Studio_FindAnimDistance( panim[i], flDist );
			flCycle = flCycle + flLocalCycle * weight[i];
		}
	}
	return flCycle;
}

//-----------------------------------------------------------------------------
// Purpose: lookup attachment by name
//-----------------------------------------------------------------------------

int Studio_FindAttachment( const CStudioHdr *pStudioHdr, const char *pAttachmentName )
{
	if ( pStudioHdr && pStudioHdr->SequencesAvailable() )
	{
		// Extract the bone index from the name
		for (int i = 0; i < pStudioHdr->GetNumAttachments(); i++)
		{
			if (!stricmp(pAttachmentName,((CStudioHdr *)pStudioHdr)->pAttachment(i).pszName( ))) 
			{
				return i;
			}
		}
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: lookup attachments by substring. Randomly return one of the matching attachments.
//-----------------------------------------------------------------------------

int Studio_FindRandomAttachment( const CStudioHdr *pStudioHdr, const char *pAttachmentName )
{
	if ( pStudioHdr )
	{
		// First move them all matching attachments into a list
		CUtlVector<int> matchingAttachments;

		// Extract the bone index from the name
		for (int i = 0; i < pStudioHdr->GetNumAttachments(); i++)
		{
			if ( strstr( ((CStudioHdr *)pStudioHdr)->pAttachment(i).pszName(), pAttachmentName ) ) 
			{
				matchingAttachments.AddToTail(i);
			}
		}

		// Then randomly return one of the attachments
		if ( matchingAttachments.Count() > 0 )
			return matchingAttachments[ RandomInt( 0, matchingAttachments.Count()-1 ) ];
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: lookup bone by name
//-----------------------------------------------------------------------------

int Studio_BoneIndexByName( const CStudioHdr *pStudioHdr, const char *pName )
{
	// binary search for the bone matching pName
	int start = 0, end = pStudioHdr->numbones()-1;
	const byte *pBoneTable = pStudioHdr->GetBoneTableSortedByName();
	const mstudiobone_t *pbones = pStudioHdr->pBone( 0 );
	while (start <= end)
	{
		int mid = (start + end) >> 1;
		int cmp = Q_stricmp( pbones[pBoneTable[mid]].pszName(), pName );
		
		if ( cmp < 0 )
		{
			start = mid + 1;
		}
		else if ( cmp > 0 )
		{
			end = mid - 1;
		}
		else
		{
			return pBoneTable[mid];
		}
	}
	return -1;
}

const char *Studio_GetDefaultSurfaceProps( CStudioHdr *pstudiohdr )
{
	return pstudiohdr->pszSurfaceProp();
}

float Studio_GetMass( CStudioHdr *pstudiohdr )
{
	return pstudiohdr->mass();
}

//-----------------------------------------------------------------------------
// Purpose: return pointer to sequence key value buffer
//-----------------------------------------------------------------------------

const char *Studio_GetKeyValueText( const CStudioHdr *pStudioHdr, int iSequence )
{
	if (pStudioHdr && pStudioHdr->SequencesAvailable())
	{
		if (iSequence >= 0 && iSequence < pStudioHdr->GetNumSeq())
		{
			return ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequence ).KeyValueText();
		}
	}
	return NULL;
}

bool Studio_PrefetchSequence( const CStudioHdr *pStudioHdr, int iSequence )
{
	bool pendingload = false;
	mstudioseqdesc_t &seqdesc = ((CStudioHdr *)pStudioHdr)->pSeqdesc( iSequence );
	int size0 = seqdesc.groupsize[ 0 ];
	int size1 = seqdesc.groupsize[ 1 ];
	for ( int i = 0; i < size0; ++i )
	{
		for ( int j = 0; j < size1; ++j )
		{
			mstudioanimdesc_t &animdesc = ((CStudioHdr *)pStudioHdr)->pAnimdesc( seqdesc.anim( i, j ) );
			int iFrame = 0;
			byte *panim = animdesc.pAnim( &iFrame );
			if ( !panim )
			{
				pendingload = true;
			}
		}
	}

	// Everything for this sequence is resident?
	return !pendingload;
}


//-----------------------------------------------------------------------------
// Purpose: Drive a flex controller from a component of a bone
//-----------------------------------------------------------------------------
void Studio_RunBoneFlexDrivers( float *pflFlexControllerWeights, const CStudioHdr *pStudioHdr, const Vector *pvPositions, const matrix3x4_t *pBoneToWorld, const matrix3x4_t &mRootToWorld )
{
	bool bRootToWorldInvComputed = false;
	matrix3x4_t mRootToWorldInv;
	matrix3x4_t mParentInv;
	matrix3x4_t mBoneLocal;

	const int nBoneFlexDriverCount = pStudioHdr->BoneFlexDriverCount();

	for ( int i = 0; i < nBoneFlexDriverCount; ++i )
	{
		const mstudioboneflexdriver_t *pBoneFlexDriver = pStudioHdr->BoneFlexDriver( i );
		const mstudiobone_t *pStudioBone = pStudioHdr->pBone( pBoneFlexDriver->m_nBoneIndex );

		const int nControllerCount = pBoneFlexDriver->m_nControlCount;

		if ( pStudioBone->flags & BONE_USED_BY_BONE_MERGE )
		{
			// The local space version of the bone is not available if this is a bonemerged bone
			// so do the slow computation of the local version of the bone from boneToWorld

			if ( pStudioBone->parent < 0 )
			{
				if ( !bRootToWorldInvComputed )
				{
					MatrixInvert( mRootToWorld, mRootToWorldInv );
					bRootToWorldInvComputed = true;
				}

				MatrixMultiply( mRootToWorldInv, pBoneToWorld[ pBoneFlexDriver->m_nBoneIndex ], mBoneLocal );
			}
			else
			{
				MatrixInvert( pBoneToWorld[ pStudioBone->parent ], mParentInv );
				MatrixMultiply( mParentInv, pBoneToWorld[ pBoneFlexDriver->m_nBoneIndex ], mBoneLocal );
			}

			for ( int j = 0; j < nControllerCount; ++j )
			{
				const mstudioboneflexdrivercontrol_t *pController = pBoneFlexDriver->pBoneFlexDriverControl( j );
				const mstudioflexcontroller_t *pFlexController = pStudioHdr->pFlexcontroller( static_cast< LocalFlexController_t >( pController->m_nFlexControllerIndex ) );

				if ( pFlexController->localToGlobal < 0 )
					continue;

				Assert( pController->m_nFlexControllerIndex >= 0 && pController->m_nFlexControllerIndex < pStudioHdr->numflexcontrollers() );
				Assert( pController->m_nBoneComponent >= 0 && pController->m_nBoneComponent <= 2 );
				pflFlexControllerWeights[pFlexController->localToGlobal] =
					RemapValClamped( mBoneLocal[pController->m_nBoneComponent][3], pController->m_flMin, pController->m_flMax, 0.0f, 1.0f );
			}
		}
		else
		{
			// Use the local space version of the bone directly for non-bonemerged bones

			const Vector &position = pvPositions[ pBoneFlexDriver->m_nBoneIndex ];

			for ( int j = 0; j < nControllerCount; ++j )
			{
				const mstudioboneflexdrivercontrol_t *pController = pBoneFlexDriver->pBoneFlexDriverControl( j );
				const mstudioflexcontroller_t *pFlexController = pStudioHdr->pFlexcontroller( static_cast< LocalFlexController_t >( pController->m_nFlexControllerIndex ) );

				if ( pFlexController->localToGlobal < 0 )
					continue;

				Assert( pController->m_nFlexControllerIndex >= 0 && pController->m_nFlexControllerIndex < pStudioHdr->numflexcontrollers() );
				Assert( pController->m_nBoneComponent >= 0 && pController->m_nBoneComponent <= 2 );
				pflFlexControllerWeights[pFlexController->localToGlobal] =
					RemapValClamped( position[pController->m_nBoneComponent], pController->m_flMin, pController->m_flMax, 0.0f, 1.0f );
			}
		}
	}
}
