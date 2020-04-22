//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

//#include "tier0/dbg.h"
#include "mathlib/mathlib.h"
#include "bone_setup_PS3.h"

#if !defined(__SPU__)
#include <string.h>
#include "tier0/vprof.h"
#endif

#include "bone_utils_PS3.h"



class CIKSolver_PS3
{
public:
	//-------- SOLVE TWO LINK INVERSE KINEMATICS -------------
	// Author: Ken Perlin
	//
	// Given a two link joint from [0,0,0] to end effector position P,
	// let link lengths be a and b, and let norm |P| = c.  Clearly a+b <= c.
	//
	// Problem: find a "knee" position Q such that |Q| = a and |P-Q| = b.
	//
	// In the case of a point on the x axis R = [c,0,0], there is a
	// closed form solution S = [d,e,0], where |S| = a and |R-S| = b:
	//
	//    d2+e2 = a2                  -- because |S| = a
	//    (c-d)2+e2 = b2              -- because |R-S| = b
	//
	//    c2-2cd+d2+e2 = b2           -- combine the two equations
	//    c2-2cd = b2 - a2
	//    c-2d = (b2-a2)/c
	//    d - c/2 = (a2-b2)/c / 2
	//
	//    d = (c + (a2-b2/c) / 2      -- to solve for d and e.
	//    e = sqrt(a2-d2)

	static float findD(float a, float b, float c) {
		return (c + (a*a-b*b)/c) / 2;
	}
	static float findE(float a, float d) { return sqrt(a*a-d*d); } 

	// This leads to a solution to the more general problem:
	//
	//   (1) R = Mfwd(P)         -- rotate P onto the x axis
	//   (2) Solve for S
	//   (3) Q = Minv(S)         -- rotate back again

	float Mfwd[3][3];
	float Minv[3][3];

	bool solve(float A, float B, float const P[], float const D[], float Q[]) {
		float R[3];
		defineM(P,D);
		rot(Minv,P,R);
		float r = length(R);
		float d = findD(A,B,r);
		float e = findE(A,d);
		float S[3] = {d,e,0};
		rot(Mfwd,S,Q);
		return d > (r - B) && d < A;
	}

	// If "knee" position Q needs to be as close as possible to some point D,
	// then choose M such that M(D) is in the y>0 half of the z=0 plane.
	//
	// Given that constraint, define the forward and inverse of M as follows:

	void defineM(float const P[], float const D[]) {
		float *X = Minv[0], *Y = Minv[1], *Z = Minv[2];

		// Minv defines a coordinate system whose x axis contains P, so X = unit(P).
		int i;
		for (i = 0 ; i < 3 ; i++)
			X[i] = P[i];
		normalize(X);

		// Its y axis is perpendicular to P, so Y = unit( E - X(E·X) ).

		float dDOTx = dot(D,X);
		for (i = 0 ; i < 3 ; i++)
			Y[i] = D[i] - dDOTx * X[i];
		normalize(Y);

		// Its z axis is perpendicular to both X and Y, so Z = X×Y.

		cross(X,Y,Z);

		// Mfwd = (Minv)T, since transposing inverts a rotation matrix.

		for (i = 0 ; i < 3 ; i++) {
			Mfwd[i][0] = Minv[0][i];
			Mfwd[i][1] = Minv[1][i];
			Mfwd[i][2] = Minv[2][i];
		}
	}

	//------------ GENERAL VECTOR MATH SUPPORT -----------

	static float dot(float const a[], float const b[]) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }

	static float length(float const v[]) { return sqrt( dot(v,v) ); }

	static void normalize(float v[]) {
		float norm = length(v);
		for (int i = 0 ; i < 3 ; i++)
			v[i] /= norm;
	}

	static void cross(float const a[], float const b[], float c[]) {
		c[0] = a[1] * b[2] - a[2] * b[1];
		c[1] = a[2] * b[0] - a[0] * b[2];
		c[2] = a[0] * b[1] - a[1] * b[0];
	}

	static void rot(float const M[3][3], float const src[], float dst[]) {
		for (int i = 0 ; i < 3 ; i++)
			dst[i] = dot(M[i],src);
	}
};



//-----------------------------------------------------------------------------
// Purpose: for a 2 bone chain, find the IK solution and reset the matrices
//-----------------------------------------------------------------------------
bool Studio_SolveIK_PS3( Vector &kneeDir0, int bone0, int bone1, int bone2, Vector &targetFoot, matrix3x4a_t *pBoneToWorld )
{
#if 0
	// see bone_ik.cpp - something with the CS models breaks this
	if( kneeDir0.LengthSqr() > 0.0f )
	{
		Vector targetKneeDir, targetKneePos;
		// FIXME: knee length should be as long as the legs
		Vector tmp = kneeDir0;
		VectorRotate_PS3( tmp, pBoneToWorld[ bone0 ], targetKneeDir );
		MatrixPosition_PS3( pBoneToWorld[ bone1 ], targetKneePos );
		return Studio_SolveIK_PS3( bone0, bone1, bone2, targetFoot, targetKneePos, targetKneeDir, pBoneToWorld );
	}
	else
#endif
	{
		return Studio_SolveIK_PS3( bone0, bone1, bone2, targetFoot, pBoneToWorld );
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float Studio_IKRuleWeight_PS3( mstudioikrule_t_PS3 &ikRule, int anim_numframes, float flCycle, int &iFrame, float &fraq )
{
	if( ikRule.end > 1.0f && flCycle < ikRule.start )
	{
		flCycle = flCycle + 1.0f;
	}

	float value = 0.0f;
	fraq = (anim_numframes - 1) * (flCycle - ikRule.start) + ikRule.iStart;
	iFrame = (int)fraq;
	fraq = fraq - iFrame;

	if( flCycle < ikRule.start )
	{
		iFrame = ikRule.iStart;
		fraq = 0.0f;
		return 0.0f;
	}
	else if( flCycle < ikRule.peak )
	{
		value = (flCycle - ikRule.start) / (ikRule.peak - ikRule.start);
	}
	else if( flCycle < ikRule.tail )
	{
		return 1.0f;
	}
	else if( flCycle < ikRule.end )
	{
		value = 1.0f - ((flCycle - ikRule.tail) / (ikRule.end - ikRule.tail));
	}
	else
	{
		fraq = (anim_numframes - 1) * (ikRule.end - ikRule.start) + ikRule.iStart;
		iFrame = (int)fraq;
		fraq = fraq - iFrame;
	}
	return SimpleSpline_PS3( value );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float Studio_IKRuleWeight_PS3( ikcontextikrule_t_PS3 &ikRule, float flCycle )
{
	if( ikRule.end > 1.0f && flCycle < ikRule.start )
	{
		flCycle = flCycle + 1.0f;
	}

	float value = 0.0f;
	if( flCycle < ikRule.start )
	{
		return 0.0f;
	}
	else if( flCycle < ikRule.peak )
	{
		value = (flCycle - ikRule.start) / (ikRule.peak - ikRule.start);
	}
	else if( flCycle < ikRule.tail )
	{
		return 1.0f;
	}
	else if( flCycle < ikRule.end )
	{
		value = 1.0f - ((flCycle - ikRule.tail) / (ikRule.end - ikRule.tail));
	}
	return 3.0f * value * value - 2.0f * value * value * value;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float Studio_IKTail_PS3( ikcontextikrule_t_PS3 &ikRule, float flCycle )
{
	if( ikRule.end > 1.0f && flCycle < ikRule.start )
	{
		flCycle = flCycle + 1.0f;
	}

	if( flCycle <= ikRule.tail )
	{
		return 0.0f;
	}
	else if( flCycle < ikRule.end )
	{
		return ((flCycle - ikRule.tail) / (ikRule.end - ikRule.tail));
	}
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

bool Studio_IKShouldLatch_PS3( ikcontextikrule_t_PS3 &ikRule, float flCycle )
{
	if( ikRule.end > 1.0f && flCycle < ikRule.start )
	{
		flCycle = flCycle + 1.0f;
	}

	if( flCycle < ikRule.peak )
	{
		return false;
	}
	else if( flCycle < ikRule.end )
	{
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool Studio_IKAnimationError_PS3( animData_SPU *panim, mstudioanimdesc_t_PS3 *pAnimdesc, void *pEA_IKRule, mstudioikrule_t_PS3 *pLS_IKRule, float flCycle, BoneVector &pos, BoneQuaternion &q, float &flWeight )
{
	float fraq;
	int iFrame;

	if( !pEA_IKRule )
		return false;

	flWeight = Studio_IKRuleWeight_PS3( *pLS_IKRule, pAnimdesc->numframes, flCycle, iFrame, fraq );
	Assert( fraq >= 0.0f && fraq < 1.0f );
	Assert( flWeight >= 0.0f && flWeight <= 1.0f );

	// This shouldn't be necessary, but the Assert should help us catch whoever is screwing this up
	flWeight = clamp( flWeight, 0.0f, 1.0f );

	if( pLS_IKRule->type != IK_GROUND && flWeight < 0.0001f )
		return false;

	void *pEA_Error = (void *)pLS_IKRule->pError( pEA_IKRule, iFrame );

	if( pEA_Error != NULL )
	{
		mstudioikerror_t_PS3 error[ 3 ] ALIGN16;
		mstudioikerror_t_PS3 *pLS_Error;

		pLS_Error = (mstudioikerror_t_PS3 *)SPUmemcpy_UnalignedGet( error, (uint32)pEA_Error, 2*sizeof(mstudioikerror_t_PS3) );

		if( fraq < 0.001f )
		{
			q = pLS_Error[0].q;
			pos = pLS_Error[0].pos;
		}
		else
		{
			QuaternionBlend_PS3( pLS_Error[0].q, pLS_Error[1].q, fraq, q );
			pos = pLS_Error[0].pos * (1.0f - fraq) + pLS_Error[1].pos * fraq;
		}
		return true;
	}

	void *pEA_Compressed = (void *)pLS_IKRule->pCompressedError( pEA_IKRule );

	if( pEA_Compressed != NULL )
	{
		mstudiocompressedikerror_t_PS3 compressed[ 3 ] ALIGN16;
		mstudiocompressedikerror_t_PS3 *pLS_Compressed;

		pLS_Compressed = (mstudiocompressedikerror_t_PS3 *)SPUmemcpy_UnalignedGet( compressed, (uint32)pEA_Compressed, sizeof(mstudiocompressedikerror_t_PS3) );

		CalcDecompressedAnimation_PS3( pEA_Compressed, pLS_Compressed, iFrame - pLS_IKRule->iStart, fraq, pos, q );
		return true;
	}
	// no data, disable IK rule
	Assert( 0 );
	flWeight = 0.0f;
	return false;
}
//-----------------------------------------------------------------------------
// Purpose: For a specific sequence:rule, find where it starts, stops, and what 
//			the estimated offset from the connection point is.
//			return true if the rule is within bounds.
//-----------------------------------------------------------------------------
bool Studio_IKSequenceError_PS3( accumposeentry_SPU *pPoseEntry, int iRule, const float poseParameter[], ikcontextikrule_t_PS3 &ikRule )
{
	int i;

	memset( &ikRule, 0, sizeof(ikRule) );
	ikRule.start = ikRule.peak = ikRule.tail = ikRule.end = 0.0f;

	float prevStart = 0.0f;

	animData_SPU *pAnim0 = NULL;
	if( pPoseEntry->animIndices[0] != -1 )
	{
		pAnim0 = &pPoseEntry->anims[ pPoseEntry->animIndices[0] ];
	}

	//mstudioikrule_t_PS3 ikrule[4][2] ALIGN16;
	byte ikrule[4][ ROUNDUPTONEXT16B( sizeof(mstudioikrule_t_PS3) ) ] ALIGN16;

	//mstudioikrulezeroframe_t_PS3 ikrulezeroframe[4][2] ALIGN16;
	byte ikrulezeroframe[4][ ROUNDUPTONEXT16B( sizeof(mstudioikrulezeroframe_t_PS3) ) ] ALIGN16;

	//mstudioanimdesc_t_PS3 ls_animdesc[4][2] ALIGN16;
	byte ls_animdesc[4][ ROUNDUPTONEXT16B( sizeof(mstudioanimdesc_t_PS3) ) ] ALIGN16;


	mstudioikrule_t_PS3 *pLS_IKRule[4];
	mstudioikrulezeroframe_t_PS3 *pLS_IKRuleZeroFrame[4];
	mstudioanimdesc_t_PS3 *pLS_animdesc[4];

	float flCycle = pPoseEntry->cyclePoseSingle;


	// prefetch animdesc
	for( i = 0; i < 4; i++ )
	{
		int idx = pPoseEntry->animIndices[ i ];
		if( idx != -1 )
		{
			animData_SPU *pAnim = &pPoseEntry->anims[ idx ];
			pLS_animdesc[ idx ] = (mstudioanimdesc_t_PS3 *)SPUmemcpy_UnalignedGet_MustSync( ls_animdesc[ idx ], (uint32)pAnim->pEA_animdesc, sizeof(mstudioanimdesc_t_PS3), DMATAG_ANIM );
		}
		else
		{
			pLS_animdesc[ idx ] = NULL;
		}

		pLS_IKRule[ i ]			 = NULL;
		pLS_IKRuleZeroFrame[ i ] = NULL;
	}

	SPUmemcpy_Sync( 1<<DMATAG_ANIM );

	// find overall influence
	for( i = 0; i < 4; i++ )
	{
		int idx = pPoseEntry->animIndices[ i ];

		if( idx != -1 )
		{
			mstudioanimdesc_t_PS3	*pAnimdesc	= pLS_animdesc[ idx ];
			animData_SPU		*pAnim		= &pPoseEntry->anims[ idx ];

			if( iRule >= pAnimdesc->numikrules || pAnimdesc->numikrules != pLS_animdesc[ pPoseEntry->animIndices[0] ]->numikrules )
			{
				Assert( 0 );
				return false;
			}

			//mstudioikrule_t_PS3 *pRule = panim[i]->pIKRule( iRule );
			if( pAnim->pEA_animdesc_ikrule )
			{
				pLS_IKRule[ idx ] = (mstudioikrule_t_PS3 *)SPUmemcpy_UnalignedGet( ikrule[ idx ], (uint32)((mstudioikrule_t_PS3 *)pAnim->pEA_animdesc_ikrule + iRule), sizeof(mstudioikrule_t_PS3) );
				mstudioikrule_t_PS3 *pIKRule = pLS_IKRule[ idx ];

				float dt = 0.0f;
				if( prevStart != 0.0f )
				{
					if( pIKRule->start - prevStart > 0.5f )
					{
						dt = -1.0f;
					}
					else if( pIKRule->start - prevStart < -0.5f )
					{
						dt = 1.0f;
					}
				}
				else
				{
					prevStart = pIKRule->start;
				}

				ikRule.start += (pIKRule->start + dt) * pAnim->seqdesc_weight;
				ikRule.peak  += (pIKRule->peak  + dt) * pAnim->seqdesc_weight;
				ikRule.tail  += (pIKRule->tail  + dt) * pAnim->seqdesc_weight;
				ikRule.end   += (pIKRule->end   + dt) * pAnim->seqdesc_weight;
			}
			else
			{
				//mstudioikrulezeroframe_t *pZeroFrameRule = panim[i]->pIKRuleZeroFrame( iRule );
				if( pAnim->pEA_animdesc_ikrulezeroframe )
				{
					pLS_IKRuleZeroFrame[ idx ] = (mstudioikrulezeroframe_t_PS3 *)SPUmemcpy_UnalignedGet( ikrulezeroframe[ idx ], (uint32)((mstudioikrulezeroframe_t_PS3 *)pAnim->pEA_animdesc_ikrulezeroframe + iRule), sizeof(mstudioikrulezeroframe_t_PS3) );
					mstudioikrulezeroframe_t_PS3 *pIKRule = pLS_IKRuleZeroFrame[ idx ];

					float dt = 0.0f;
					if (prevStart != 0.0f)
					{
						if( pIKRule->start.GetFloat() - prevStart > 0.5f )
						{
							dt = -1.0f;
						}
						else if( pIKRule->start.GetFloat() - prevStart < -0.5f )
						{
							dt = 1.0f;
						}
					}
					else
					{
						prevStart = pIKRule->start.GetFloat();
					}

					ikRule.start += (pIKRule->start.GetFloat() + dt) * pAnim->seqdesc_weight;
					ikRule.peak  += (pIKRule->peak.GetFloat()  + dt) * pAnim->seqdesc_weight;
					ikRule.tail  += (pIKRule->tail.GetFloat()  + dt) * pAnim->seqdesc_weight;
					ikRule.end   += (pIKRule->end.GetFloat()   + dt) * pAnim->seqdesc_weight;
				}
				else
				{
					// Msg("%s %s - IK Stall\n", pStudioHdr->name(), seqdesc.pszLabel() );
					return false;
				}
			}
		}
	}

	if( ikRule.start > 1.0f )
	{
		ikRule.start -= 1.0f;
		ikRule.peak  -= 1.0f;
		ikRule.tail  -= 1.0f;
		ikRule.end   -= 1.0f;
	}
	else if( ikRule.start < 0.0f )
	{
		ikRule.start += 1.0f;
		ikRule.peak  += 1.0f;
		ikRule.tail  += 1.0f;
		ikRule.end   += 1.0f;
	}

	ikRule.flWeight = Studio_IKRuleWeight_PS3( ikRule, flCycle );
	if( ikRule.flWeight <= 0.001f )
	{
		// go ahead and allow IK_GROUND rules a virtual looping section
		if( pAnim0->pEA_animdesc_ikrule == NULL )
			return false;

		int ikrule0_type;

		int idx0 = pPoseEntry->animIndices[ 0 ];

		if( idx0 != -1 ) 
		{
			if( pLS_IKRule[ idx0 ] == NULL )
			{
				pLS_IKRule[ idx0 ] = (mstudioikrule_t_PS3 *)SPUmemcpy_UnalignedGet( ikrule[ idx0 ], (uint32)((mstudioikrule_t_PS3 *)pAnim0->pEA_animdesc_ikrule + iRule), sizeof(mstudioikrule_t_PS3) );
			}

			ikrule0_type = pLS_IKRule[ idx0 ]->type;

			if( (pLS_animdesc[ idx0 ]->flags & STUDIO_LOOPING) && ikrule0_type == IK_GROUND && ikRule.end - ikRule.start > 0.75f )
			{
				ikRule.flWeight = 0.001f;
				//?? 
				flCycle = ikRule.end - 0.001f;
			}
			else
			{
				return false;
			}
		}
	}

	Assert( ikRule.flWeight > 0.0f );

	ikRule.pos.Init();
	ikRule.q.Init();

	// find target error
	float total = 0.0f;
	for( i = 0; i < 4; i++ )
	{
		int idx = pPoseEntry->animIndices[i];

		if( idx != -1 )
		{
			mstudioanimdesc_t_PS3	*pAnimdesc	= pLS_animdesc[ idx ];
			animData_SPU			*pAnim		= &pPoseEntry->anims[ idx ];

			BoneVector     pos1;
			BoneQuaternion q1;
			float w;

			// mstudioikrule_t *pRule = panim[i]->pIKRule( iRule );
			mstudioikrule_t_PS3 *pRule = pLS_IKRule[ idx ];

			if( pRule != NULL )
			{
				ikRule.chain = pRule->chain;	// FIXME: this is anim local
				ikRule.bone  = pRule->bone;		// FIXME: this is anim local
				ikRule.type  = pRule->type;
				ikRule.slot  = pRule->slot;

				ikRule.height += pRule->height * pAnim->seqdesc_weight;
				ikRule.floor  += pRule->floor  * pAnim->seqdesc_weight;
				ikRule.radius += pRule->radius * pAnim->seqdesc_weight;
				ikRule.drop   += pRule->drop   * pAnim->seqdesc_weight;
				ikRule.top    += pRule->top    * pAnim->seqdesc_weight;
			}
			else
			{
				// look to see if there's a zeroframe version of the rule
				mstudioikrulezeroframe_t_PS3 *pZeroFrameRule = pLS_IKRuleZeroFrame[ idx ];//panim[i]->pIKRuleZeroFrame( iRule );

				if( pZeroFrameRule )
				{
					// zeroframe doesn't contain details, so force a IK_RELEASE
					ikRule.type  = IK_RELEASE;
					ikRule.chain = pZeroFrameRule->chain;
					ikRule.slot  = pZeroFrameRule->slot;
					ikRule.bone  = -1;

					// Msg("IK_RELEASE %d %d : %.2f\n", ikRule.chain, ikRule.slot, ikRule.flWeight );
				}
				else
				{
					// Msg("%s %s - IK Stall\n", pStudioHdr->name(), seqdesc.pszLabel() );
					return false;
				}
			}

			// keep track of tail condition
			ikRule.release += Studio_IKTail_PS3( ikRule, flCycle ) * pAnim->seqdesc_weight;

			// only check rules with error values
			switch( ikRule.type )
			{
			case IK_SELF:
			case IK_WORLD:
			case IK_GROUND:
			case IK_ATTACHMENT:
				{
					mstudioikrule_t_PS3 *pEA_Rule = NULL;
					
					if( pAnim->pEA_animdesc_ikrule )
					{
						pEA_Rule = (mstudioikrule_t_PS3 *)pAnim->pEA_animdesc_ikrule + iRule;
					}

					int bResult = Studio_IKAnimationError_PS3( pAnim, pAnimdesc, (void *)pEA_Rule, pRule, flCycle, pos1, q1, w );

					if (bResult)
					{
						ikRule.pos = ikRule.pos + pos1 * pAnim->seqdesc_weight;
						QuaternionAccumulate_PS3( ikRule.q, pAnim->seqdesc_weight, q1, ikRule.q );
						total +=  pAnim->seqdesc_weight;
					}
				}
				break;
			default:
				total +=  pAnim->seqdesc_weight;
				break;
			}

			ikRule.latched = Studio_IKShouldLatch_PS3( ikRule, flCycle ) * ikRule.flWeight;

			if( ikRule.type == IK_ATTACHMENT )
			{
			//	ikRule.szLabel = pRule->pszAttachment();
			}
		}
	}

	if( total <= 0.0001f )
	{
		return false;
	}

	if( total < 0.999f )
	{
		VectorScale_PS3( ikRule.pos, 1.0f / total, ikRule.pos );
		QuaternionScale_PS3( ikRule.q, 1.0f / total, ikRule.q );
	}

	if( ikRule.type == IK_SELF && ikRule.bone != -1 )
	{
		//ikRule.bone = pStudioHdr->RemapSeqBone( iSequence, ikRule.bone );
		if( pPoseEntry->pEA_seqgroup_masterBone )
		{
			int masterbone[6] ALIGN16;
			int *pLS_masterbone;

			pLS_masterbone = (int *)SPUmemcpy_UnalignedGet( masterbone, (uint32)((int *)pPoseEntry->pEA_seqgroup_masterBone + ikRule.bone), sizeof(int) );

			ikRule.bone = *pLS_masterbone;
		}

		if (ikRule.bone == -1)
			return false;
	}

	QuaternionNormalize_PS3( ikRule.q );

	return true;
}



CIKContext_PS3::CIKContext_PS3()
{
	m_targetCount   = 0;

	m_iFramecounter = -1;
	m_flTime		= -1.0f;
}


void CIKContext_PS3::ClearTargets( void )
{
	int i;
	for (i = 0; i < m_targetCount; i++)
	{
		m_target[ i ].latched.iFramecounter = -9999;
	}
}

void CIKContext_PS3::Init( bonejob_SPU *pBonejobSPU, const QAngle &angles, const Vector &pos, float flTime, int iFramecounter, int boneMask )
{
	SNPROF_ANIM( "CIKContext_PS3::Init" );

//	m_pStudioHdr = pStudioHdr;
//	m_ikChainRule.RemoveAll(); // m_numikrules = 0;
//	if (pStudioHdr->numikchains())
//	{
//		m_ikChainRule.SetSize( pStudioHdr->numikchains() );
//
//		// FIXME: Brutal hackery to prevent a crash
//		if (m_target.Count() == 0)
//		{
//			m_target.SetSize(12);
//			memset( m_target.Base(), 0, sizeof(m_target[0])*m_target.Count() );
//			ClearTargets();
//		}
//
//	}
//	else
//	{
//		m_target.SetSize( 0 );
//	}
	
	if( pBonejobSPU->numikchains )
	{
		m_targetCount = 12;
		memset( m_target, 0, sizeof( m_target ) );
		ClearTargets();
	}
	else
	{
		m_targetCount = 0;
	}

	m_numTarget		= 0;

	AngleMatrix_PS3( angles, pos, m_rootxform );

	m_iFramecounter = iFramecounter;
	m_flTime		= flTime;
	m_boneMask		= boneMask;

}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void AddDependencies_SPU( bonejob_SPU *pBonejobSPU, accumposeentry_SPU *pPoseEntry, float flWeight )
{
	SNPROF_ANIM("AddDependencies_SPU");

	int i;

	if( pBonejobSPU->numikchains == 0 )
		return;

	if( pPoseEntry->seqdesc_numikrules == 0 )
		return;

	mstudioikchain_t_PS3 ikchain[2] ALIGN16;
	int					 boneIndex[4] ALIGN16;

	ikcontextikrule_t_PS3 ikrule;

	for( i = 0; i < pPoseEntry->seqdesc_numikrules; i++ )
	{
		if( !Studio_IKSequenceError_PS3( pPoseEntry, i, pBonejobSPU->poseparam, ikrule ) )
			continue;

		//int bone = m_pStudioHdr->pIKChain( ikrule.chain )->pLink( 2 )->bone;
		mstudioikchain_t_PS3 *pEA_ikchain = (mstudioikchain_t_PS3 *)pBonejobSPU->pEA_studiohdr_ikchains + ikrule.chain;

		// get chain
		mstudioikchain_t_PS3 *pLS_ikchain = (mstudioikchain_t_PS3 *)SPUmemcpy_UnalignedGet( ikchain, (uint32)pEA_ikchain, sizeof(mstudioikchain_t_PS3) );
		
		// get bone, bone is 1st in mstudioiklink
		int *pEA_bone = (int *)pLS_ikchain->pLink( pEA_ikchain, 2 );

		int *pbone = (int *)SPUmemcpy_UnalignedGet( boneIndex, (uint32)pEA_bone, sizeof(int) );


		// don't add rule if the bone isn't going to be calculated
		if( !(pBonejobSPU->boneFlags[ *pbone ] & pBonejobSPU->boneMask) )
			continue;

		// or if its relative bone isn't going to be calculated
		if( ikrule.bone >= 0 && !(pBonejobSPU->boneFlags[ ikrule.bone ] & pBonejobSPU->boneMask) )
			continue;

#if !defined( _CERT )
#if defined(__SPU__)
		if( g_addDep_numIKRules >= MAX_IKRULES )
		{
			spu_printf( "numIkRules:%d >= MAX_IKRULES:%d\n", g_addDep_numIKRules, MAX_IKRULES );
			__asm volatile ("stopd $0,$0,$0");
		}
#else
		AssertFatal( g_addDep_numIKRules < MAX_IKRULES );
#endif
#endif // !_CERT

		ikrule.flRuleWeight = flWeight;

		// rest will be done on PPU, append to list of potential ikrules to add
		g_addDep_IKRules[ g_addDep_numIKRules++ ] = ikrule;
	}
}

//-----------------------------------------------------------------------------
// Purpose: build boneToWorld transforms for a specific bone
//-----------------------------------------------------------------------------
void CIKContext_PS3::BuildBoneChain(
								const bonejob_SPU *pBonejob, 
								const BoneVector pos[], 
								const BoneQuaternion q[], 
								int	iBone,
								matrix3x4a_t *pBoneToWorld,
								CBoneBitList_PS3 &boneComputed )
{
	::BuildBoneChain_PS3( pBonejob->boneParent, m_rootxform, pos, q, iBone, pBoneToWorld, boneComputed );
}


//-----------------------------------------------------------------------------
// Purpose: turn a specific bones boneToWorld transform into a pos and q in parents bonespace
//-----------------------------------------------------------------------------
void SolveBone_PS3( 
			   int *pBoneParent,
			   int	iBone,
			   matrix3x4a_t *pBoneToWorld,
			   BoneVector pos[], 
			   BoneQuaternion q[]
)
{
	int iParent = pBoneParent[ iBone ];

	matrix3x4a_t worldToBone;
	MatrixInvert_PS3( pBoneToWorld[iParent], worldToBone );

	matrix3x4a_t local;
	ConcatTransforms_Aligned_PS3( worldToBone, pBoneToWorld[iBone], local );

	MatrixAngles_PS3( local, q[iBone], pos[iBone] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext_PS3::AddAutoplayLocks( bonejob_SPU *pBonejob, BoneVector pos[], BoneQuaternion q[] )
{
	SNPROF_ANIM("CIKContext_PS3::AddAutoplayLocks");

	// skip all array access if no autoplay locks.
	if( pBonejob->numikAutoplayLocks == 0 )
	{
		return;
	}

	byte ls_iklock[ ROUNDUPTONEXT16B( sizeof(mstudioiklock_t_PS3) ) ] ALIGN16;
	byte ls_ikchain[ ROUNDUPTONEXT16B( sizeof(mstudioikchain_t_PS3) ) ] ALIGN16;
	byte ls_iklink0[ ROUNDUPTONEXT16B( (sizeof(int) + sizeof(Vector)) ) ] ALIGN16;
	byte ls_bone1[ sizeof(int) + 0x10 ] ALIGN16;
	byte ls_bone2[ sizeof(int) + 0x10 ] ALIGN16;


	matrix3x4a_t *boneToWorld = (matrix3x4a_t *)PushLSStack( sizeof(matrix3x4a_t) * pBonejob->maxBones );
	CBoneBitList_PS3 boneComputed;

#if !defined( _CERT )
#if defined(__SPU__)
	if( pBonejob->numikAutoplayLocks >= MAX_IKLOCKS )
	{
		spu_printf( "pBonejob->numikAutoplayLocks:%d >= MAX_IKLOCKS:%d\n", pBonejob->numikAutoplayLocks, MAX_IKLOCKS );
		__asm volatile ("stopd $0,$0,$0");
	}
#else
	AssertFatal( pBonejob->numikAutoplayLocks < MAX_IKLOCKS );
#endif
#endif // !_CERT

	memset( m_ikLock, 0, sizeof( ikcontextikrule_t_PS3 ) * pBonejob->numikAutoplayLocks );

	for( int i = 0; i < pBonejob->numikAutoplayLocks; i++ )
	{
		mstudioiklock_t_PS3  *pLock			= (mstudioiklock_t_PS3 *)SPUmemcpy_UnalignedGet( ls_iklock, (uint32)pBonejob->pEA_IKAutoplayLocks[i], sizeof(mstudioiklock_t_PS3) );
		mstudioikchain_t_PS3 *pEA_chain		= (mstudioikchain_t_PS3 *)pBonejob->pEA_studiohdr_ikchains + pLock->chain;
		mstudioikchain_t_PS3 *pLS_chain		= (mstudioikchain_t_PS3 *)SPUmemcpy_UnalignedGet( ls_ikchain, (uint32)pEA_chain, sizeof(mstudioikchain_t_PS3) );
		mstudioiklink_t_PS3	 *pEA_iklink	= pLS_chain->pLink( pEA_chain, 2 );

		int *pbone2 = (int *)SPUmemcpy_UnalignedGet( ls_bone2, (uint32)pEA_iklink, sizeof(int) );
		int bone = *pbone2;

		if( !( pBonejob->boneFlags[ bone ] & m_boneMask ) )
			continue;

		// eval current ik'd bone
		BuildBoneChain( pBonejob, pos, q, bone, boneToWorld, boneComputed );

		ikcontextikrule_t_PS3 &ikrule = m_ikLock[ i ];
		
		ikrule.chain	= pLock->chain;
		ikrule.slot		= i;
		ikrule.type		= IK_WORLD;

		MatrixAngles_PS3( boneToWorld[ bone ], ikrule.q, ikrule.pos );

		pEA_iklink = pLS_chain->pLink( pEA_chain, 0 );
		mstudioiklink_t_PS3 *plink0 = (mstudioiklink_t_PS3 *)SPUmemcpy_UnalignedGet( ls_iklink0, (uint32)pEA_iklink, sizeof(int) + sizeof(Vector) );

		// save off current knee direction
		if( plink0->kneeDir.LengthSqr() > 0.0f )
		{
			pEA_iklink = pLS_chain->pLink( pEA_chain, 1 );
			int *pbone1 = (int *)SPUmemcpy_UnalignedGet_MustSync( ls_bone1, (uint32)pEA_iklink, sizeof(int), DMATAG_ANIM );

			VectorRotate_PS3( plink0->kneeDir, boneToWorld[ plink0->bone ], ikrule.kneeDir );

			SPUmemcpy_Sync( 1<<DMATAG_ANIM );
			MatrixPosition_PS3( boneToWorld[ *pbone1 ], ikrule.kneePos ); 
		}
		else
		{
			ikrule.kneeDir.Init( );
		}
	}


	PopLSStack( sizeof(matrix3x4a_t) * pBonejob->maxBones );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CIKContext_PS3::AddSequenceLocks( bonejob_SPU *pBonejob, accumposeentry_SPU* pPoseEntry, BoneVector pos[], BoneQuaternion q[] )
{
	SNPROF_ANIM("CIKContext_PS3::AddSequenceLocks");

	matrix3x4a_t *boneToWorld = (matrix3x4a_t *)PushLSStack( sizeof(matrix3x4a_t) * pBonejob->maxBones );

	CBoneBitList_PS3 boneComputed;

	memset( m_ikLock, 0, sizeof( ikcontextikrule_t_PS3 ) * pPoseEntry->seqdesc_numiklocks );

	byte ls_ikchain[ ROUNDUPTONEXT16B( sizeof(mstudioikchain_t_PS3) ) ] ALIGN16;
	byte ls_iklink0[ ROUNDUPTONEXT16B( (sizeof(int) + sizeof(Vector)) ) ] ALIGN16;
	byte ls_bone2[ sizeof(int) + 0x10 ] ALIGN16;

	// fetch all iklocks
	byte ls_iklocks[ (ROUNDUPTONEXT16B( sizeof(mstudioiklock_t_PS3) ) * MAX_IKLOCKS) ] ALIGN16;
	mstudioiklock_t_PS3 *piklocks = (mstudioiklock_t_PS3 *)SPUmemcpy_UnalignedGet( ls_iklocks, (uint32)pPoseEntry->pEA_seqdesc_iklocks, sizeof(mstudioiklock_t_PS3) * pPoseEntry->seqdesc_numiklocks );

	for (int i = 0; i < pPoseEntry->seqdesc_numiklocks; i++)
	{
		mstudioiklock_t_PS3  *pLock			= &piklocks[i]; //seqdesc.pIKLock( i );
		mstudioikchain_t_PS3 *pEA_chain		= (mstudioikchain_t_PS3 *)pBonejob->pEA_studiohdr_ikchains + pLock->chain;
		mstudioikchain_t_PS3 *pLS_chain		= (mstudioikchain_t_PS3 *)SPUmemcpy_UnalignedGet( ls_ikchain, (uint32)pEA_chain, sizeof(mstudioikchain_t_PS3) );
		mstudioiklink_t_PS3	 *pEA_iklink	= pLS_chain->pLink( pEA_chain, 2 );

		int *pbone = (int *)SPUmemcpy_UnalignedGet( ls_bone2, (uint32)pEA_iklink, sizeof(int) );
		int bone = *pbone;//pLS_chain->pLink( pEA_chain, 2 )->bone; //pPoseEntry->ikChains[ i ].bone2;

		if( !( pBonejob->boneFlags[ bone ] & m_boneMask ) )
			continue;

		// eval current ik'd bone
		BuildBoneChain( pBonejob, pos, q, bone, boneToWorld, boneComputed );

		ikcontextikrule_t_PS3 &ikrule = m_ikLock[ i ];
		ikrule.chain = i;
		ikrule.slot  = i;
		ikrule.type  = IK_WORLD;

		MatrixAngles_PS3( boneToWorld[ bone ], ikrule.q, ikrule.pos );

		pEA_iklink = pLS_chain->pLink( pEA_chain, 0 );
		mstudioiklink_t_PS3 *plink0 = (mstudioiklink_t_PS3 *)SPUmemcpy_UnalignedGet( ls_iklink0, (uint32)pEA_iklink, sizeof(int) + sizeof(Vector) );

		// save off current knee direction
		if( plink0->kneeDir.LengthSqr() > 0.0f )
		{
			VectorRotate_PS3( plink0->kneeDir, boneToWorld[ plink0->bone ], ikrule.kneeDir );
		}
		else
		{
			ikrule.kneeDir.Init( );
		}
	}

	PopLSStack( sizeof(matrix3x4a_t) * pBonejob->maxBones );
}

//----------------------------------------------------------------------------------------------------------------------------------
// SolveDependencies path for SPU not currently supported, code here for reference
//----------------------------------------------------------------------------------------------------------------------------------

#if 0 
matrix3x4a_t			g_boneToWorld[ MAXSTUDIOBONES_PS3 ]	ALIGN128;
BoneVector				g_pos[ MAXSTUDIOBONES_PS3 ]	ALIGN128;
BoneQuaternion			g_q[ MAXSTUDIOBONES_PS3 ]	ALIGN128;

struct posq
{
	Vector		pos;
	Quaternion	q;
};


void RunSolveDependenciesJob_SPU( bonejob_SPU_2 *pBonejob2 )
{
}

void RunSolveDependenciesJob_PPU( bonejob_SPU_2 *pBonejob2 )
{
	SNPROF_ANIM("RunSolveDependenciesJob_PPU");

	BoneVector		*pos		 = (BoneVector *)pBonejob2->pEA_pos;
	BoneQuaternion	*q			 = (BoneQuaternion *)pBonejob2->pEA_q;
	matrix3x4a_t    *boneToWorld = (matrix3x4a_t *)pBonejob2->pEA_bones;

	CBoneBitList_PS3 boneComputed;
	

	// src
	memcpy( g_pos, pos, sizeof(BoneVector) * pBonejob2->studiohdr_numbones );
	memcpy( g_q, q, sizeof(BoneQuaternion) * pBonejob2->studiohdr_numbones );

	memcpy( &boneComputed.m_markedBones, pBonejob2->pEA_boneComputed, sizeof(byte) * pBonejob2->studiohdr_numbones );
	
	// just copy all bones for now, dma list later
	//memcpy( g_boneToWorld, boneToWorld, sizeof(matrix3x4a_t) * pBonejob2->studiohdr_numbones );
	int lp;

	// copy matrices that are up to date
	for( lp = 0; lp < pBonejob2->studiohdr_numbones; lp++ )
	{
		if( boneComputed.IsBoneMarked(lp) )
		{
			memcpy( &g_boneToWorld[lp], &boneToWorld[lp], sizeof(matrix3x4a_t) );
		}
	}

	SolveDependencies_SPU( pBonejob2, boneComputed );

	// copy results back (boneToWorld and boneComputed)
	memcpy( pBonejob2->pEA_boneComputed, &boneComputed.m_markedBones, sizeof(byte) * pBonejob2->studiohdr_numbones );

	// build dma list to only copy computed matrices
	for( lp = 0; lp < pBonejob2->studiohdr_numbones; lp++ )
	{
		if( boneComputed.IsBoneMarked(lp) )
		{
			memcpy( &boneToWorld[lp], &g_boneToWorld[lp], sizeof(matrix3x4a_t) );
		}
	}


}

void SolveDependencies_SPU( bonejob_SPU_2 *pBonejob2, CBoneBitList_PS3 &boneComputed )
{
	SNPROF_ANIM( "SolveDependencies_SPU" );

	matrix3x4a_t worldTarget;
	int i, j;

	ikchainresult_t_PS3 chainResult[32]; 

	// assume all these structures contain valid data
	matrix3x4a_t		*boneToWorld	= g_boneToWorld;
	BoneVector			*pos			= g_pos;
	BoneQuaternion		*q				= g_q;


	// init chain rules
	for( i = 0; i < pBonejob2->studiohdr_numikchains; i++ )
	{
		int							bone	= pBonejob2->ikChains[ i ].bone2;
		ikchainresult_t_PS3 *pChainResult	= &chainResult[ i ];

		pChainResult->target				= -1;
		pChainResult->flWeight				= 0.0f;

		// don't bother with chain if the bone isn't going to be calculated
		if( bone == -1 )	
			continue;

		// eval current ik'd bone
		BuildBoneChain_PS3( pBonejob2->boneParent, pBonejob2->rootxform, pos, q, bone, boneToWorld, boneComputed );

		MatrixAngles_PS3( boneToWorld[bone], pChainResult->q, pChainResult->pos );
	}

	ikcontextikrule_t_PS3 ikrule[2] ALIGN16;
	
	for( i = 0; i < pBonejob2->numikchainElements; i++ )
	{
		void *pEA_Rule = pBonejob2->ikchainElement_rules[ i ];

		ikcontextikrule_t_PS3	*pRule			= (ikcontextikrule_t_PS3 *)SPUmemcpy_UnalignedGet( ikrule, (uint32)pEA_Rule, sizeof(ikcontextikrule_t_PS3) );
		ikchainresult_t_PS3		*pChainResult	= &chainResult[ pRule->chain ];

		pChainResult->target					= -1;

		switch( pRule->type )
		{
		case IK_SELF:
			{
				// xform IK target error into world space
				matrix3x4a_t local;

				QuaternionMatrix_PS3( pRule->q, pRule->pos, local );

				// eval target bone space
				if (pRule->bone != -1)
				{
					BuildBoneChain_PS3( pBonejob2->boneParent, pBonejob2->rootxform, pos, q, pRule->bone, boneToWorld, boneComputed );
					ConcatTransforms_Aligned_PS3( boneToWorld[pRule->bone], local, worldTarget );
				}
				else
				{
					ConcatTransforms_Aligned_PS3( pBonejob2->rootxform, local, worldTarget );
				}

				float flWeight = pRule->flWeight * pRule->flRuleWeight;
				pChainResult->flWeight = pChainResult->flWeight * (1.0f - flWeight) + flWeight;

				BoneVector		p2;
				BoneQuaternion	q2;

				// target p and q
				MatrixAngles_PS3( worldTarget, q2, p2 );

				// debugLine( pChainResult->pos, p2, 0, 0, 255, true, 0.1 );

				// blend in position and angles
				pChainResult->pos = pChainResult->pos * (1.0f - flWeight) + p2 * flWeight;
				QuaternionSlerp_PS3( pChainResult->q, q2, flWeight, pChainResult->q );
			}
			break;
		case IK_RELEASE:
			{
				// move target back towards original location
				float flWeight = pRule->flWeight * pRule->flRuleWeight;
				int bone = pBonejob2->ikchainElement_bones[ i ];

				Vector p2;
				Quaternion q2;

				BuildBoneChain_PS3( pBonejob2->boneParent, pBonejob2->rootxform, pos, q, bone, boneToWorld, boneComputed );
				MatrixAngles_PS3( boneToWorld[bone], q2, p2 );

				// blend in position and angles
				pChainResult->pos = pChainResult->pos * (1.0f - flWeight) + p2 * flWeight;
				QuaternionSlerp_PS3( pChainResult->q, q2, flWeight, pChainResult->q );
			}
			break;
		default:
			break;
		}

	}

	CIKTarget_PS3 target[2] ALIGN16;

	for (i = 0; i < pBonejob2->iktargetcount; i++)
	{
		void			*pEA_Target  = pBonejob2->iktargets[ i ];
		CIKTarget_PS3	*pTarget	 = (CIKTarget_PS3 *)SPUmemcpy_UnalignedGet( target, (uint32)pEA_Target, sizeof(CIKTarget_PS3) );

		if( pTarget->est.flWeight > 0.0f )
		{
			matrix3x4a_t worldFootpad;
			matrix3x4a_t local;

			//mstudioikchain_t *pchain = m_pStudioHdr->pIKChain( m_target[i].chain );
			ikchainresult_t_PS3 *pChainResult = &chainResult[ pTarget->chain ];

			AngleMatrix_PS3(pTarget->offset.q, pTarget->offset.pos, local );

			AngleMatrix_PS3( pTarget->est.q, pTarget->est.pos, worldFootpad );

			ConcatTransforms_Aligned_PS3( worldFootpad, local, worldTarget );

			BoneVector		p2;
			BoneQuaternion	q2;

			// target p and q
			MatrixAngles_PS3( worldTarget, q2, p2 );

			// MatrixAngles( worldTarget, pChainResult->q, pChainResult->pos );

			// blend in position and angles
			pChainResult->flWeight = pTarget->est.flWeight;
			pChainResult->pos = pChainResult->pos * (1.0f - pChainResult->flWeight ) + p2 * pChainResult->flWeight;
			QuaternionSlerp_PS3( pChainResult->q, q2, pChainResult->flWeight, pChainResult->q );
		}

		if( pTarget->latched.bNeedsLatch )
		{
			// keep track of latch position
			pTarget->latched.bHasLatch	= true;
			pTarget->latched.q			= pTarget->est.q;
			pTarget->latched.pos		= pTarget->est.pos;
		}
	}

	for (i = 0; i < pBonejob2->studiohdr_numikchains; i++)
	{
		ikchainresult_t_PS3 *pChainResult = &chainResult[ i ];
		ikChain_SPU			*pchain       = &pBonejob2->ikChains[ i ];

		if( pChainResult->flWeight > 0.0f )
		{
			Vector tmp;
			MatrixPosition_PS3( boneToWorld[ pchain->bone2 ], tmp );

			// do exact IK solution
			// FIXME: once per link!
			if( Studio_SolveIK_PS3( pchain, pChainResult->pos, boneToWorld ) )
			{
				Vector p3;
				MatrixGetColumn_PS3( boneToWorld[ pchain->bone2 ], 3, p3 );
				QuaternionMatrix_PS3( pChainResult->q, p3, boneToWorld[ pchain->bone2 ] );

				// rebuild chain
				// FIXME: is this needed if everyone past this uses the boneToWorld array?
				SolveBone_PS3( pBonejob2->boneParent, pchain->bone2, boneToWorld, pos, q );
				SolveBone_PS3( pBonejob2->boneParent, pchain->bone1, boneToWorld, pos, q );
				SolveBone_PS3( pBonejob2->boneParent, pchain->bone0, boneToWorld, pos, q );
			}
			else
			{
				// FIXME: need to invalidate the targets that forced this...
				if( pChainResult->target != -1 )
				{
					byte	*pEA_Target  = (byte *)pBonejob2->iktargets[ pChainResult->target ];
					// only really need to get deltaPos, deltaQ
					posq	deltaPosQ[ 2 ] ALIGN16;
					int		offset_deltaPosQ = uint32(&target[0].latched.deltaPos)	- uint32(&target[0]);

					posq	*pTarget_deltaPosQ;

					pTarget_deltaPosQ	 = (posq *)SPUmemcpy_UnalignedGet( deltaPosQ, (uint32)(pEA_Target + offset_deltaPosQ), sizeof( posq ) );

					VectorScale_PS3( pTarget_deltaPosQ->pos, 0.8f, pTarget_deltaPosQ->pos );
					QuaternionScale_PS3( pTarget_deltaPosQ->q, 0.8f, pTarget_deltaPosQ->q );

					// only really need to push back deltaPos, deltaQ
					SPUmemcpy_UnalignedPut( pTarget_deltaPosQ, (uint32)(pEA_Target + offset_deltaPosQ), sizeof( posq ) );
				}
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext_PS3::SolveAutoplayLocks(
									bonejob_SPU *pBonejob, 
									BoneVector pos[], 
									BoneQuaternion q[]
)
{
	SNPROF_ANIM("CIKContext_PS3::SolveAutoplayLocks");

	matrix3x4a_t *boneToWorld = (matrix3x4a_t *)PushLSStack( sizeof(matrix3x4a_t) * pBonejob->maxBones );

	CBoneBitList_PS3 boneComputed;

	int i;

	byte ls_iklock[ ROUNDUPTONEXT16B( sizeof(mstudioiklock_t_PS3) ) ] ALIGN16;

	for( i = 0; i < pBonejob->numikAutoplayLocks; i++ )
	{
		mstudioiklock_t_PS3  *pLock	= (mstudioiklock_t_PS3 *)SPUmemcpy_UnalignedGet( ls_iklock, (uint32)pBonejob->pEA_IKAutoplayLocks[i], sizeof(mstudioiklock_t_PS3) );

		SolveLock( pBonejob, pLock, i, pos, q, boneToWorld, boneComputed );
	}

	PopLSStack( sizeof(matrix3x4a_t) * pBonejob->maxBones );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CIKContext_PS3::SolveSequenceLocks(
									bonejob_SPU *pBonejob, 
									accumposeentry_SPU* pPoseEntry, 
									BoneVector pos[], 
									BoneQuaternion q[]
)
{
	SNPROF_ANIM("CIKContext_PS3::SolveSequenceLocks");

	matrix3x4a_t *boneToWorld = (matrix3x4a_t *)PushLSStack( sizeof(matrix3x4a_t) * pBonejob->maxBones );

	CBoneBitList_PS3 boneComputed;

	int i;

	// fetch all iklocks
	byte ls_iklocks[ (ROUNDUPTONEXT16B(sizeof(mstudioiklock_t_PS3)) * MAX_IKLOCKS) ] ALIGN16;
	mstudioiklock_t_PS3 *piklocks = (mstudioiklock_t_PS3 *)SPUmemcpy_UnalignedGet( ls_iklocks, (uint32)pPoseEntry->pEA_seqdesc_iklocks, sizeof(mstudioiklock_t_PS3) * pPoseEntry->seqdesc_numiklocks );


	for( i = 0; i < pPoseEntry->seqdesc_numiklocks; i++ )
	{
		//mstudioiklock_t_PS3 *pLock = &pPoseEntry->ikLocks[ i ];
		mstudioiklock_t_PS3  *pLock	= &piklocks[i]; 

		SolveLock( pBonejob, pLock, i, pos, q, boneToWorld, boneComputed );
	}

	PopLSStack( sizeof(matrix3x4a_t) * pBonejob->maxBones );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIKContext_PS3::SolveLock(
						   bonejob_SPU *pBonejob, 
						   const mstudioiklock_t_PS3 *pLock,
						   int i,
						   BoneVector pos[], 
						   BoneQuaternion q[],
						   matrix3x4a_t boneToWorld[], 
						   CBoneBitList_PS3 &boneComputed
						   )
{
	byte ls_ikchain[ ROUNDUPTONEXT16B(sizeof(mstudioikchain_t_PS3)) ] ALIGN16;
	byte ls_iklink0[ ROUNDUPTONEXT16B( (sizeof(int) + sizeof(Vector)) ) ] ALIGN16;
	byte ls_bone1[ sizeof(int) + 0x10 ] ALIGN16;
	byte ls_bone2[ sizeof(int) + 0x10 ] ALIGN16;


	mstudioikchain_t_PS3 *pEA_chain		= (mstudioikchain_t_PS3 *)pBonejob->pEA_studiohdr_ikchains + pLock->chain;
	mstudioikchain_t_PS3 *pLS_chain		= (mstudioikchain_t_PS3 *)SPUmemcpy_UnalignedGet( ls_ikchain, (uint32)pEA_chain, sizeof(mstudioikchain_t_PS3) );
	mstudioiklink_t_PS3	 *pEA_iklink0	= pLS_chain->pLink( pEA_chain, 0 );
	mstudioiklink_t_PS3	 *pEA_iklink1	= pLS_chain->pLink( pEA_chain, 1 );
	mstudioiklink_t_PS3	 *pEA_iklink2	= pLS_chain->pLink( pEA_chain, 2 );

	mstudioiklink_t_PS3  *plink0		= (mstudioiklink_t_PS3 *)SPUmemcpy_UnalignedGet_MustSync( ls_iklink0, (uint32)pEA_iklink0, sizeof(int) + sizeof(Vector), DMATAG_ANIM );
	int *pbone1 = (int *)SPUmemcpy_UnalignedGet_MustSync( ls_bone1, (uint32)pEA_iklink1, sizeof(int), DMATAG_ANIM );
	int *pbone2 = (int *)SPUmemcpy_UnalignedGet( ls_bone2, (uint32)pEA_iklink2, sizeof(int) );

	int bone = *pbone2;//pChain->bone2;

	// don't bother with iklock if the bone isn't going to be calculated
	if( !( pBonejob->boneFlags[ bone ] & m_boneMask ) )
	{
		SPUmemcpy_Sync( 1<<DMATAG_ANIM );
		return;
	}

	// eval current ik'd bone
	BuildBoneChain( pBonejob, pos, q, bone, boneToWorld, boneComputed );

	BoneVector p1, p2, p3;
	BoneQuaternion q2, q3;

	// current p and q
	MatrixPosition_PS3( boneToWorld[ bone ], p1 );

	// blend in position
	p3 = p1 * (1.0f - pLock->flPosWeight ) + m_ikLock[ i ].pos * pLock->flPosWeight;

	SPUmemcpy_Sync( 1<<DMATAG_ANIM );

	// do exact IK solution
	if( m_ikLock[ i ].kneeDir.LengthSqr() > 0.0f )
	{
		Studio_SolveIK_PS3( plink0->bone, *pbone1, *pbone2, p3, m_ikLock[ i ].kneePos, m_ikLock[ i ].kneeDir, boneToWorld );
	}
	else
	{
		Studio_SolveIK_PS3( plink0->kneeDir, plink0->bone, *pbone1, *pbone2, p3, boneToWorld );
	}

	// slam orientation
	MatrixPosition_PS3( boneToWorld[ bone ], p3 );
	QuaternionMatrix_PS3( m_ikLock[ i ].q, p3, boneToWorld[ bone ] );

	// rebuild chain
	q2 = q[ bone ];
	SolveBone_PS3( pBonejob->boneParent, *pbone2, boneToWorld, pos, q );
	QuaternionSlerp_PS3( q[ bone ], q2, pLock->flLocalQWeight, q[ bone ] );

	SolveBone_PS3( pBonejob->boneParent, *pbone1, boneToWorld, pos, q );
	SolveBone_PS3( pBonejob->boneParent, plink0->bone, boneToWorld, pos, q );
}


#define KNEEMAX_EPSILON (0.9998f) // (0.9998 is about 1 degree)

//-----------------------------------------------------------------------------
// Purpose: Solve Knee position for a known hip and foot location, but no specific knee direction preference
//-----------------------------------------------------------------------------
bool Studio_SolveIK_PS3( int8 iThigh, int8 iKnee, int8 iFoot, Vector &targetFoot, matrix3x4a_t *pBoneToWorld )
{
	Vector worldFoot, worldKnee, worldThigh;

	MatrixPosition_PS3( pBoneToWorld[ iThigh ], worldThigh );
	MatrixPosition_PS3( pBoneToWorld[ iKnee ], worldKnee );
	MatrixPosition_PS3( pBoneToWorld[ iFoot ], worldFoot );

	Vector ikFoot, ikKnee;

	ikFoot = targetFoot - worldThigh;
	ikKnee = worldKnee - worldThigh;

	float l1 = (worldKnee - worldThigh).Length();
	float l2 = (worldFoot - worldKnee).Length();
	float l3 = (worldFoot - worldThigh).Length();

	// leg too straight to figure out knee?
	if( l3 > (l1 + l2) * KNEEMAX_EPSILON )
	{
		return false;
	}

	Vector ikHalf;
	ikHalf = (worldFoot - worldThigh) * (l1 / l3);

	// FIXME: what to do when the knee completely straight?
	Vector ikKneeDir;
	ikKneeDir = ikKnee - ikHalf;
	VectorNormalize_PS3( ikKneeDir );

	return Studio_SolveIK_PS3( iThigh, iKnee, iFoot, targetFoot, worldKnee, ikKneeDir, pBoneToWorld );
}

//-----------------------------------------------------------------------------
// Purpose: Realign the matrix so that its X axis points along the desired axis.
//-----------------------------------------------------------------------------
void Studio_AlignIKMatrix_PS3( matrix3x4a_t &mMat, const Vector &vAlignTo )
{
	Vector tmp1, tmp2, tmp3;

	// Column 0 (X) becomes the vector.
	tmp1 = vAlignTo;
	VectorNormalize_PS3( tmp1 );
	MatrixSetColumn_PS3( tmp1, 0, mMat );

	// Column 1 (Y) is the cross of the vector and column 2 (Z).
	MatrixGetColumn_PS3( mMat, 2, tmp3 );
	tmp2 = tmp3.Cross( tmp1 );
	VectorNormalize_PS3( tmp2 );
	// FIXME: check for X being too near to Z
	MatrixSetColumn_PS3( tmp2, 1, mMat );

	// Column 2 (Z) is the cross of columns 0 (X) and 1 (Y).
	tmp3 = tmp1.Cross( tmp2 );
	MatrixSetColumn_PS3( tmp3, 2, mMat );
}

//-----------------------------------------------------------------------------
// Purpose: Solve Knee position for a known hip and foot location, and a known knee direction
//-----------------------------------------------------------------------------
bool Studio_SolveIK_PS3( int8 iThigh, int8 iKnee, int8 iFoot, Vector &targetFoot, Vector &targetKneePos, Vector &targetKneeDir, matrix3x4a_t *pBoneToWorld )
{
	Vector worldFoot, worldKnee, worldThigh;

	MatrixPosition_PS3( pBoneToWorld[ iThigh ], worldThigh );
	MatrixPosition_PS3( pBoneToWorld[ iKnee ], worldKnee );
	MatrixPosition_PS3( pBoneToWorld[ iFoot ], worldFoot );

	Vector ikFoot, ikTargetKnee, ikKnee;

	ikFoot = targetFoot - worldThigh;
	ikKnee = targetKneePos - worldThigh;

	float l1 = (worldKnee - worldThigh).Length();
	float l2 = (worldFoot - worldKnee).Length();

	// exaggerate knee targets for legs that are nearly straight
	// FIXME: should be configurable, and the ikKnee should be from the original animation, not modifed
	float d = (targetFoot-worldThigh).Length() - MIN( l1, l2 );
	d = MAX( l1 + l2, d );
	// FIXME: too short knee directions cause trouble
	d = d * 100.0f;

	ikTargetKnee = ikKnee + targetKneeDir * d;

	// too far away? (0.9998 is about 1 degree)
	if( ikFoot.Length() > (l1 + l2) * KNEEMAX_EPSILON )
	{
		VectorNormalize_PS3( ikFoot );
		VectorScale_PS3( ikFoot, (l1 + l2) * KNEEMAX_EPSILON, ikFoot );
	}

	// too close?
	// limit distance to about an 80 degree knee bend
	float minDist = MAX( fabs(l1 - l2) * 1.15, MIN( l1, l2 ) * 0.15f );
	if( ikFoot.Length() < minDist )
	{
		// too close to get an accurate vector, just use original vector
		ikFoot = (worldFoot - worldThigh);
		VectorNormalize_PS3( ikFoot );
		VectorScale_PS3( ikFoot, minDist, ikFoot );
	}

	CIKSolver_PS3 ik;

	if( ik.solve( l1, l2, ikFoot.Base(), ikTargetKnee.Base(), ikKnee.Base() ) )
	{
		matrix3x4a_t& mWorldThigh = pBoneToWorld[ iThigh ];
		matrix3x4a_t& mWorldKnee = pBoneToWorld[ iKnee ];
		matrix3x4a_t& mWorldFoot = pBoneToWorld[ iFoot ];

		// build transformation matrix for thigh
		Studio_AlignIKMatrix_PS3( mWorldThigh, ikKnee );
		VectorAligned kneeToFoot;
		kneeToFoot = ikFoot - ikKnee;
		Studio_AlignIKMatrix_PS3( mWorldKnee, kneeToFoot );

		mWorldKnee[0][3] = ikKnee.x + worldThigh.x;
		mWorldKnee[1][3] = ikKnee.y + worldThigh.y;
		mWorldKnee[2][3] = ikKnee.z + worldThigh.z;

		mWorldFoot[0][3] = ikFoot.x + worldThigh.x;
		mWorldFoot[1][3] = ikFoot.y + worldThigh.y;
		mWorldFoot[2][3] = ikFoot.z + worldThigh.z;

		return true;
	}
	else
	{
		return false;
	}
}
