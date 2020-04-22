//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// studio_render.cpp: routines for drawing Half-Life 3DStudio models
// updates:
// 1-4-99	fixed AdvanceFrame wraping bug

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

#include <windows.h> // for OutputDebugString. . has to be a better way!


#include "ViewerSettings.h"
#include "StudioModel.h"
#include "vphysics/constraints.h"
#include "physmesh.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "matsyswin.h"
#include "istudiorender.h"
#include "utldict.h"
#include "filesystem.h"
#include "studio_render.h"
#include "materialsystem/IMesh.h"
#include "bone_setup.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "MDLViewer.h"
#include "bone_accessor.h"
#include "jigglebones.h"
#include "debugdrawmodel.h"

// FIXME:
extern ViewerSettings g_viewerSettings;
int g_dxlevel = 0;

#pragma warning( disable : 4244 ) // double to float

////////////////////////////////////////////////////////////////////////

CStudioHdr		*g_pCacheHdr = NULL;

Vector			g_flexedverts[MAXSTUDIOVERTS];
Vector			g_flexednorms[MAXSTUDIOVERTS];
int				g_flexages[MAXSTUDIOVERTS];

Vector			*g_pflexedverts;
Vector			*g_pflexednorms;
int				*g_pflexages;

int				g_smodels_total;				// cookie

matrix3x4a_t	g_viewtransform;				// view transformation
//matrix3x4_t	g_posetoworld[MAXSTUDIOBONES];	// bone transformation matrix
matrix3x4_t		g_mCachedViewTransform;			// copy of view transform for boneMerge passes

static int			maxNumVertices;
static int			first = 1;

#define PHYSICS_HULL_ARC_SEGS 32

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
mstudioseqdesc_t &StudioModel::GetSeqDesc( int seq )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	return pStudioHdr->pSeqdesc( seq );
}

mstudioanimdesc_t &StudioModel::GetAnimDesc( int anim )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	return pStudioHdr->pAnimdesc( anim );
}


//-----------------------------------------------------------------------------
// Purpose: Keeps a global clock to autoplay sequences to run from
//			Also deals with speedScale changes
//-----------------------------------------------------------------------------
float GetAutoPlayTime( void )
{
	static int g_prevTicks;
	static float g_time;

	int ticks = GetTickCount();
	// limit delta so that float time doesn't overflow
	if (g_prevTicks == 0)
		g_prevTicks = ticks;

	g_time += ( (ticks - g_prevTicks) / 1000.0f ) * g_viewerSettings.speedScale;
	g_prevTicks = ticks;

	return g_time;
}


//-----------------------------------------------------------------------------
// Purpose: Keeps a global clock for "realtime" overlays to run from
//-----------------------------------------------------------------------------
float GetRealtimeTime( void )
{
	// renamed static's so debugger doesn't get confused and show the wrong one
	static int g_prevTicksRT;
	static float g_timeRT;

	int ticks = GetTickCount();
	// limit delta so that float time doesn't overflow
	if (g_prevTicksRT == 0)
		g_prevTicksRT = ticks;

	g_timeRT += ( (ticks - g_prevTicksRT) / 1000.0f );
	g_prevTicksRT = ticks;

	return g_timeRT;
}

void StudioModel::AdvanceFrame( float dt )
{
	if (dt > 0.1)
		dt = 0.1f;

	m_dt = dt;

	float t = GetDuration( );

	if (t > 0)
	{
		if (dt > 0)
		{
			m_cycle += dt / t;
			m_sequencetime += dt;

			// wrap
			m_cycle -= (int)(m_cycle);
		}
	}
	else
	{
		m_cycle = 0;
	}

	
	for (int i = 0; i < MAXSTUDIOANIMLAYERS; i++)
	{
		t = GetDuration( m_Layer[i].m_sequence );
		if (t > 0)
		{
			if (dt > 0)
			{
				m_Layer[i].m_cycle += (dt / t) * m_Layer[i].m_playbackrate;
				m_Layer[i].m_cycle -= (int)(m_Layer[i].m_cycle);
			}
		}
		else
		{
			m_Layer[i].m_cycle = 0;
		}
	}
}

float StudioModel::GetInterval( void )
{
	return m_dt;
}

float StudioModel::GetCycle( void )
{
	return m_cycle;
}

float StudioModel::GetFrame( void )
{
	return GetCycle() * GetMaxFrame();
}

int StudioModel::GetMaxFrame( void )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	return Studio_MaxFrame( pStudioHdr, m_sequence, m_poseparameter );
}

int StudioModel::SetFrame( int frame )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	if ( frame <= 0 )
		frame = 0;

	int maxFrame = GetMaxFrame();
	if ( frame >= maxFrame )
	{
		frame = maxFrame;
		m_cycle = 0.99999;
		return frame;
	}

	m_cycle = frame / (float)maxFrame;
	return frame;
}


float StudioModel::GetCycle( int iLayer )
{
	if (iLayer == 0)
	{
		return m_cycle;
	}
	else if (iLayer <= MAXSTUDIOANIMLAYERS)
	{
		int index = iLayer - 1;
		return m_Layer[index].m_cycle;
	}
	return 0;
}


float StudioModel::GetFrame( int iLayer )
{
	return GetCycle( iLayer ) * GetMaxFrame( iLayer );
}


int StudioModel::GetMaxFrame( int iLayer )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( pStudioHdr )
	{
		if (iLayer == 0)
			return Studio_MaxFrame( pStudioHdr, m_sequence, m_poseparameter );

		if (iLayer <= MAXSTUDIOANIMLAYERS)
		{
			int index = iLayer - 1;
			return Studio_MaxFrame( pStudioHdr, m_Layer[index].m_sequence, m_poseparameter );
		}
	}

	return 0;
}


int StudioModel::SetFrame( int iLayer, int frame )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	if ( frame <= 0 )
		frame = 0;

	int maxFrame = GetMaxFrame( iLayer );
	float cycle = 0;
	if (maxFrame)
	{
		if ( frame >= maxFrame )
		{
			frame = maxFrame;
			cycle = 0.99999;
		}
		cycle = frame / (float)maxFrame;
	}

	if (iLayer == 0)
	{
		m_cycle = cycle;
	}
	else if (iLayer <= MAXSTUDIOANIMLAYERS)
	{
		int index = iLayer - 1;
		m_Layer[index].m_cycle = cycle;
	}

	return frame;
}



//-----------------------------------------------------------------------------
// Purpose: Maps from local axis (X,Y,Z) to Half-Life (PITCH,YAW,ROLL) axis/rotation mappings
//-----------------------------------------------------------------------------
static int RemapAxis( int axis )
{
	switch( axis )
	{
	case 0:
		return 2;
	case 1:
		return 0;
	case 2:
		return 1;
	}

	return 0;
}

void StudioModel::Physics_SetPreview( int previewBone, int axis, float t )
{
	m_physPreviewBone = previewBone;
	m_physPreviewAxis = axis;
	m_physPreviewParam = t;
}


void StudioModel::OverrideBones( bool *override )
{
	matrix3x4_t basematrix;
	matrix3x4_t bonematrix;

	QAngle tmp;
	// offset for the base pose to world transform of 90 degrees around up axis
	tmp[0] = 0; tmp[1] = 90; tmp[2] = 90;
	AngleMatrix( tmp, bonematrix );
	ConcatTransforms( g_viewtransform, bonematrix, basematrix );

	for ( int i = 0; i < m_pPhysics->Count(); i++ )
	{
		CPhysmesh *pmesh = m_pPhysics->GetMesh( i );
		// BUGBUG: Cache this if you care about performance!
		int boneIndex = FindBone(pmesh->m_boneName);
		
		// bone is not constrained, don't override rotations
		if ( pmesh->m_constraint.parentIndex == 0 && pmesh->m_constraint.childIndex == 0 )
		{
			boneIndex = -1;
		}

		if ( boneIndex >= 0 )
		{
			matrix3x4_t *parentMatrix = &basematrix;
			override[boneIndex] = true;
			int parentBone = -1;
			if ( pmesh->m_constraint.parentIndex >= 0 )
			{
				parentBone = FindBone( m_pPhysics->GetMesh(pmesh->m_constraint.parentIndex)->m_boneName );
			}
			if ( parentBone >= 0 )
			{
				parentMatrix = &m_pBoneToWorld[ parentBone ];
			}

			if ( m_physPreviewBone == i )
			{
				matrix3x4_t tmpmatrix;
				QAngle rot;
				constraint_axislimit_t *axis = pmesh->m_constraint.axes + m_physPreviewAxis;

				int hlAxis = RemapAxis( m_physPreviewAxis );
				rot.Init();
				rot[hlAxis] = axis->minRotation + (axis->maxRotation - axis->minRotation) * m_physPreviewParam;
				AngleMatrix( rot, tmpmatrix );
				ConcatTransforms( pmesh->m_matrix, tmpmatrix, bonematrix );
			}
			else
			{
				MatrixCopy( pmesh->m_matrix, bonematrix );
			}

			ConcatTransforms( *parentMatrix, bonematrix, m_pBoneToWorld[ boneIndex ] );
		}
	}
}


int StudioModel::BoneMask( void )
{
	int lod = g_viewerSettings.autoLOD ? 0 : g_viewerSettings.lod;

	int mask = BONE_USED_BY_VERTEX_AT_LOD(lod);
	if (g_viewerSettings.showAttachments || g_viewerSettings.m_iEditAttachment != -1 || m_nSolveHeadTurn != 0 || LookupAttachment( "eyes" ) != -1)
	{
		mask |= BONE_USED_BY_ATTACHMENT;
	}
	
	if (g_viewerSettings.showBones || g_viewerSettings.highlightBone >= 0)
	{
		mask |= BONE_USED_BY_ANYTHING;
	}

	if (g_viewerSettings.showHitBoxes)
	{
		mask |= BONE_USED_BY_HITBOX;
	}

	mask |= BONE_USED_BY_BONE_MERGE;

	return mask;
	// return BONE_USED_BY_ANYTHING_AT_LOD( lod );

	// return BONE_USED_BY_ANYTHING;
}

void StudioModel::SetUpBones( bool mergeBones )
{
	int					i, j;

	const mstudiobone_t		*pbones;

	static Vector		pos[MAXSTUDIOBONES];
	ALIGN16 matrix3x4_t			bonematrix;
	static QuaternionAligned	q[MAXSTUDIOBONES];
	bool				override[MAXSTUDIOBONES];

	static ALIGN16 matrix3x4_t	boneCache[MAXSTUDIOBONES];

	// For blended transitions
	static Vector		pos2[MAXSTUDIOBONES];
	static Quaternion	q2[MAXSTUDIOBONES];

	CStudioHdr *pStudioHdr = GetStudioHdr();
	mstudioseqdesc_t	&seqdesc = pStudioHdr->pSeqdesc( m_sequence );

	QAngle a1;
	Vector p1;
	MatrixAngles( g_viewtransform, a1, p1 );
	CIKContext *pIK = NULL;
	m_ik.Init( pStudioHdr, a1, p1, GetRealtimeTime(), m_iFramecounter, BoneMask( ) );
	if ( g_viewerSettings.enableIK )
	{
		pIK = &m_ik;
	}

	IBoneSetup boneSetup( pStudioHdr, BoneMask(), m_poseparameter );

	boneSetup.InitPose( pos, q );
	
	boneSetup.AccumulatePose( pos, q, m_sequence, m_cycle, 1.0, GetRealtimeTime(), pIK );

	if ( g_viewerSettings.blendSequenceChanges &&
		m_sequencetime < m_blendtime && 
		m_prevsequence != m_sequence &&
		m_prevsequence < pStudioHdr->GetNumSeq() &&
		!(seqdesc.flags & STUDIO_SNAP) )
	{
		// Make sure frame is valid
		if ( m_prevcycle >= 1.0 )
		{
			m_prevcycle = 0.0f;
		}

		float s = 1.0 - ( m_sequencetime / m_blendtime );
		s = 3 * s * s - 2 * s * s * s;

		boneSetup.AccumulatePose( pos, q, m_prevsequence, m_prevcycle, s, GetRealtimeTime(), NULL );
		// Con_DPrintf("%d %f : %d %f : %f\n", pev->sequence, f, pev->prevsequence, pev->prevframe, s );
	}
	else
	{
		m_prevcycle = m_cycle;
	}

	int iMaxPriority = 0;
	for (i = 0; i < MAXSTUDIOANIMLAYERS; i++)
	{
		if (m_Layer[i].m_weight > 0)
		{
			iMaxPriority = max( m_Layer[i].m_priority, iMaxPriority );
		}
	}

	for (j = 0; j <= iMaxPriority; j++)
	{
		for (i = 0; i < MAXSTUDIOANIMLAYERS; i++)
		{
			if (m_Layer[i].m_priority == j && m_Layer[i].m_weight > 0)
			{
				boneSetup.AccumulatePose( pos, q, m_Layer[i].m_sequence, m_Layer[i].m_cycle, m_Layer[i].m_weight, GetRealtimeTime(), pIK );
			}
		}
	}

	if (m_nSolveHeadTurn != 0)
	{
		GetBodyPoseParametersFromFlex( );
	}

	CalcHeadRotation( pos, q );

	CIKContext auto_ik;
	auto_ik.Init( pStudioHdr, a1, p1, 0.0, 0, BoneMask( ) );
	boneSetup.CalcAutoplaySequences( pos, q, GetAutoPlayTime(), &auto_ik );

	boneSetup.CalcBoneAdj( pos, q, m_controller );

	CBoneBitList boneComputed;
	if (pIK)
	{
		Vector deltaPos;
		QAngle deltaAngles;

		GetMovement( m_prevIKCycles, deltaPos, deltaAngles );

		Vector tmp;
		VectorRotate( deltaPos, g_viewtransform, tmp );
		deltaPos = tmp;

		pIK->UpdateTargets( pos, q, m_pBoneToWorld, boneComputed );

		// FIXME: check number of slots?
		for (int i = 0; i < pIK->m_target.Count(); i++)
		{
			trace_t tr;
			CIKTarget *pTarget = &pIK->m_target[i];

			switch( pTarget->type )
			{
			case IK_GROUND:
				{
					// drawLine( pTarget->est.pos, pTarget->est.pos + pTarget->offset.pos, 0, 255, 0 );

					// hack in movement
					pTarget->est.pos -= deltaPos;

					matrix3x4_t invViewTransform;
					MatrixInvert( g_viewtransform, invViewTransform );
					Vector tmp;
					VectorTransform( pTarget->est.pos, invViewTransform, tmp );
					tmp.z = pTarget->est.floor;
					VectorTransform( tmp, g_viewtransform, pTarget->est.pos );
					Vector p1;
					Quaternion q1;
					MatrixAngles( g_viewtransform, q1, p1 );
					pTarget->est.q = q1;

					float color[4] = { 0, 0, 0, 0 };
					float wirecolor[4] = { 1, 1, 0, 1 };
					if (pTarget->est.latched > 0.0)
					{
						wirecolor[1] = 1.0 - pTarget->est.flWeight;
					}
					else
					{
						wirecolor[0] = 1.0 - pTarget->est.flWeight;
					}

					float r = max(pTarget->est.radius,1);
					Vector p0 = tmp + Vector( -r, -r, 0 );
					Vector p2 = tmp + Vector( r, r, 0 );
					drawTransparentBox( p0, p2, g_viewtransform, color, wirecolor );


					if (!g_viewerSettings.enableTargetIK)
					{
						pTarget->est.flWeight = 0.0;
					}
				}
				break;
			case IK_ATTACHMENT:
				{
					matrix3x4_t m;

					QuaternionMatrix( pTarget->est.q, pTarget->est.pos, m );

					drawTransform( m, g_viewerSettings.originAxisLength * 0.4f );
				}
				break;
			}

			// drawLine( pTarget->est.pos, pTarget->latched.pos, 255, 0, 0 );
		}
		
		pIK->SolveDependencies( pos, q, m_pBoneToWorld, boneComputed );
	}

	pbones = pStudioHdr->pBone( 0 );

	memset( override, 0, sizeof(bool)*pStudioHdr->numbones() );

	if ( g_viewerSettings.showPhysicsPreview )
	{
		OverrideBones( override );
	}

	int nMergedModelIndex = -1;
	for ( int n=0; n<HLMV_MAX_MERGED_MODELS; n++ )
	{
		if ( g_pStudioExtraModel[n] && g_pStudioExtraModel[n]->GetStudioHdr() == pStudioHdr )
		{
			if ( g_MergeModelBonePairs[n].szLocalBone[0] && g_MergeModelBonePairs[n].szTargetBone[0] )
			{
				nMergedModelIndex = n;	
			}
			break;
		}
	}

	for (i = 0; i < pStudioHdr->numbones(); i++) 
	{
		if ( !(pStudioHdr->pBone( i )->flags & BoneMask()))
		{
			int j, k;
			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 4; k++)
				{
					m_pBoneToWorld[i][j][k] = VEC_T_NAN;
				}
			}
			continue;
		}

		if ( override[i] )
		{
			continue;
		}
		else if (boneComputed.IsBoneMarked(i))
		{
			// already calculated
		}
		else if (CalcProceduralBone( pStudioHdr, i, CBoneAccessor( m_pBoneToWorld ) ))
		{
			continue;
		}
		else
		{
			QuaternionMatrix( q[i], bonematrix );

			bonematrix[0][3] = pos[i][0];
			bonematrix[1][3] = pos[i][1];
			bonematrix[2][3] = pos[i][2];

			if ( (pStudioHdr->pBone( 0 )[i].flags & BONE_ALWAYS_PROCEDURAL) && 
				 (pStudioHdr->pBone( 0 )[i].proctype == STUDIO_PROC_JIGGLE) )
			{
				//
				// Physics-based "jiggle" bone
				// Bone is assumed to be along the Z axis
				// Pitch around X, yaw around Y
				//

				// compute desired bone orientation
				matrix3x4_t goalMX;

				if (pbones[i].parent == -1) 
				{
					ConcatTransforms( g_viewtransform, bonematrix, goalMX );
				} 
				else 
				{
					ConcatTransforms( m_pBoneToWorld[ pbones[i].parent ], bonematrix, goalMX );
				}

				// get jiggle properties from QC data
				mstudiojigglebone_t *jiggleInfo = (mstudiojigglebone_t *)pStudioHdr->pBone( 0 )[i].pProcedure( );

				if (!m_pJiggleBones)
				{
					m_pJiggleBones = new CJiggleBones;
				}

				// do jiggle physics
				m_pJiggleBones->BuildJiggleTransformations( i, GetRealtimeTime(), jiggleInfo, goalMX, m_pBoneToWorld[ i ], false );
			}
			else if (pbones[i].parent == -1) 
			{
				ConcatTransforms( g_viewtransform, bonematrix, m_pBoneToWorld[ i ] );
				// MatrixCopy(bonematrix, g_bonetoworld[i]);
			} 
			else 
			{
				ConcatTransforms( m_pBoneToWorld[ pbones[i].parent ], bonematrix, m_pBoneToWorld[ i ] );
			}

			if (pStudioHdr->pBone( 0 )[i].flags & BONE_WORLD_ALIGN)
			{
				Vector vecOrigin = m_pBoneToWorld[ i ].GetOrigin();
				m_pBoneToWorld[ i ].Init( Vector(1,0,0), Vector(0,1,0), Vector(0,0,1), vecOrigin );
			}

		}

		if (!mergeBones)
		{
			g_pCacheHdr = pStudioHdr;
			MatrixCopy( m_pBoneToWorld[ i ], boneCache[i] );
		}
		else if (g_pCacheHdr)
		{
			

			// attempt to attach merge models with force bone-to-bone setting
			if ( nMergedModelIndex != -1 )
			{

				const char* szDesiredMergeModelLocalBoneName = g_MergeModelBonePairs[nMergedModelIndex].szLocalBone;
				const char* szDesiredTargetBoneName = g_MergeModelBonePairs[nMergedModelIndex].szTargetBone;

				const char* szCurrentMergeModelBoneName = pStudioHdr->pBone( i )->pszName();

				int nMergeModelLocalBone = -1;
				int nMergeModelAttachment = -1;

				matrix3x4_t matMergeModelAttachmentLocal;
				SetIdentityMatrix( matMergeModelAttachmentLocal );

				int nTargetBone = -1;
				int nTargetAttachment = -1;
				
				matrix3x4_t matTargetBoneToWorld;
				SetIdentityMatrix( matTargetBoneToWorld );
				matrix3x4_t matTargetAttachmentLocal;
				SetIdentityMatrix( matTargetAttachmentLocal );
					

				//check the attachments
				for ( int n=0; n<pStudioHdr->GetNumAttachments(); n++ )
				{
					mstudioattachment_t &pModelAttachment = (mstudioattachment_t &)pStudioHdr->pAttachment( n );
					if ( Q_stricmp( pModelAttachment.pszName(), szDesiredMergeModelLocalBoneName ) == 0	)
					{
						pStudioHdr->setBoneFlags( pModelAttachment.localbone, BONE_USED_BY_BONE_MERGE );
						if ( pModelAttachment.localbone == i )
						{
							matrix3x4_t matAttachmentToWorld;
							ConcatTransforms( m_pBoneToWorld[ pModelAttachment.localbone ], pModelAttachment.local, matAttachmentToWorld );

							matrix3x4_t matAttachmentToWorldInv;
							MatrixInvert( matAttachmentToWorld, matAttachmentToWorldInv );

							ConcatTransforms( matAttachmentToWorldInv, m_pBoneToWorld[0], matMergeModelAttachmentLocal );
							nMergeModelLocalBone = i;
							nMergeModelAttachment = n;
						}
					}
				}

				// otherwise check if we're already on the merge model local bone
				if ( nMergeModelLocalBone == -1 && Q_stricmp( szCurrentMergeModelBoneName, szDesiredMergeModelLocalBoneName ) == 0 )
				{
					nMergeModelLocalBone = i;
				}

				// if the local bone is valid, let's look for the target bone
				if ( nMergeModelLocalBone != -1 )
				{

					//search master mdl for target bone name
					for ( int n=0; n<g_pCacheHdr->numbones(); n++ )
					{
						const char* szCurrentTargetModelBoneName = g_pCacheHdr->pBone( n )->pszName();
						if ( Q_stricmp( szCurrentTargetModelBoneName, szDesiredTargetBoneName ) == 0 )
						{
							nTargetBone = n;
							MatrixCopy( *g_pStudioModel->BoneToWorld( nTargetBone ), matTargetBoneToWorld );
							break;
						}
					}
					//if we didn't find it, look at the attachments
					if ( nTargetBone == -1 )
					{
						for ( int a = 0; a < g_pCacheHdr->GetNumAttachments(); a++)
						{
							mstudioattachment_t &pModelAttachment = (mstudioattachment_t &)g_pCacheHdr->pAttachment( a );
							const char* szCurrentTargetModelBoneName = pModelAttachment.pszName();
							if ( Q_stricmp( szCurrentTargetModelBoneName, szDesiredTargetBoneName ) == 0 )
							{
								nTargetBone = pModelAttachment.localbone;
								MatrixCopy( *g_pStudioModel->BoneToWorld( nTargetBone ), matTargetBoneToWorld );
								nTargetAttachment = a;
								MatrixCopy( pModelAttachment.local, matTargetAttachmentLocal );
								break;
							}

						}
					}


					// both local and target are valid. Let's connect the two
					if ( nTargetBone != -1 )
					{

						pStudioHdr->setBoneFlags( 0, BONE_USED_BY_BONE_MERGE );
						pStudioHdr->setBoneFlags( nMergeModelLocalBone, BONE_USED_BY_BONE_MERGE );
						g_pCacheHdr->setBoneFlags( nTargetBone, BONE_USED_BY_BONE_MERGE );

						matrix3x4_t matTargetBoneToWorldFinal;
						ConcatTransforms( matTargetBoneToWorld, matTargetAttachmentLocal, matTargetBoneToWorldFinal );

						ConcatTransforms( matTargetBoneToWorldFinal, matMergeModelAttachmentLocal, matTargetBoneToWorldFinal );

						MatrixCopy( matTargetBoneToWorldFinal, m_pBoneToWorld[ 0 ] );
					}

				}

			}
			else
			{
				for (j = 0; j < g_pCacheHdr->numbones(); j++)
				{
					if ( Q_stricmp( pStudioHdr->pBone( i )->pszName(), g_pCacheHdr->pBone( j )->pszName() ) == 0 )
						break;
				}
				if (j < g_pCacheHdr->numbones())
				{
					pStudioHdr->setBoneFlags( i, BONE_USED_BY_BONE_MERGE );
					g_pCacheHdr->setBoneFlags( j, BONE_USED_BY_BONE_MERGE );
					MatrixCopy( boneCache[j], m_pBoneToWorld[ i ] );
				}
			}
			
		}
	}

	if ( mergeBones )
	{
		Studio_RunBoneFlexDrivers( m_flexweight, pStudioHdr, pos, m_pBoneToWorld, g_mCachedViewTransform );
	}
	else
	{
		MatrixCopy( g_viewtransform, g_mCachedViewTransform );
		Studio_RunBoneFlexDrivers( m_flexweight, pStudioHdr, pos, m_pBoneToWorld, g_viewtransform );
	}

	if ( g_viewerSettings.simulateSoftbodies )
	{
		if ( CSoftbody *pSoftbody = pStudioHdr->GetSoftbody() )
		{
			pSoftbody->GoWakeup();
			pSoftbody->SetAnimatedTransforms( m_pBoneToWorld );
			pSoftbody->FilterTransforms( m_pBoneToWorld );
		}
	}

	if (g_viewerSettings.showAttachments)
	{
		// drawTransform( m_pBoneToWorld[ 0 ] );
	}
}



/*
================
StudioModel::SetupLighting
	set some global variables based on entity position
inputs:
outputs:
================
*/
void StudioModel::SetupLighting ( )
{

	// DEBUG: Spin the light around the head for debugging
	//	g_viewerSettings.lightrot = QAngle( 0, 0, 0 );
	//	g_viewerSettings.lightrot.y = fmod( (90 * GetTickCount( ) / 1000.0), 360.0);

	if (g_viewerSettings.secondaryLights)
	{
		LightDesc_t light[3];

		for ( int i=0; i < 3; i++ )
		{
			light[i].m_Type = MATERIAL_LIGHT_DIRECTIONAL;
			light[i].m_Attenuation0 = 1.0f;
			light[i].m_Attenuation1 = 0.0;
			light[i].m_Attenuation2 = 0.0;
			light[i].m_Color[0] = g_viewerSettings.lColor[0];
			light[i].m_Color[1] = g_viewerSettings.lColor[1];
			light[i].m_Color[2] = g_viewerSettings.lColor[2];
			light[i].m_Range = 2000;

			AngleVectors(g_viewerSettings.lightrot, &light[i].m_Direction, NULL, NULL);
		}

		light[1].m_Color[0] = 0.3f;
		light[1].m_Color[1] = 0.4f;
		light[1].m_Color[2] = 0.5f;
		AngleVectors(g_viewerSettings.lightrot + QAngle(180, 0, 0), &light[1].m_Direction, NULL, NULL);

		light[2].m_Color[0] = 0.5f;
		light[2].m_Color[1] = 0.4f;
		light[2].m_Color[2] = 0.3f;
		AngleVectors(g_viewerSettings.lightrot + QAngle(0, 90, 0), &light[2].m_Direction, NULL, NULL);

		g_pStudioRender->SetLocalLights(3, light);
	}
	else
	{
		LightDesc_t light[1];

		light[0].m_Type = MATERIAL_LIGHT_DIRECTIONAL;
		light[0].m_Attenuation0 = 1.0f;
		light[0].m_Attenuation1 = 0.0;
		light[0].m_Attenuation2 = 0.0;
		light[0].m_Color[0] = g_viewerSettings.lColor[0];
		light[0].m_Color[1] = g_viewerSettings.lColor[1];
		light[0].m_Color[2] = g_viewerSettings.lColor[2];
		light[0].m_Range = 2000;

		light[0].m_Position = Vector( 0, 0, 0 );

		AngleVectors(g_viewerSettings.lightrot, &light[0].m_Direction, NULL, NULL);

		g_pStudioRender->SetLocalLights(1, light);
	}

	for (int i = 0; i < g_pStudioRender->GetNumAmbientLightSamples(); i++)
	{
		m_AmbientLightColors[i][0] = g_viewerSettings.aColor[0];
		m_AmbientLightColors[i][1] = g_viewerSettings.aColor[1];
		m_AmbientLightColors[i][2] = g_viewerSettings.aColor[2];
	}

	g_pStudioRender->SetAmbientLightColors(m_AmbientLightColors);

}


int FindBoneIndex( CStudioHdr *pstudiohdr, const char *pName )
{
	const mstudiobone_t *pbones = pstudiohdr->pBone( 0 );
	for (int i = 0; i < pstudiohdr->numbones(); i++)
	{
		if ( !strcmpi( pName, pbones[i].pszName() ) )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Find the named bone index, -1 if not found
// Input  : *pName - bone name
//-----------------------------------------------------------------------------
int StudioModel::FindBone( const char *pName )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	return FindBoneIndex( pStudioHdr, pName );
}


int StudioModel::Physics_GetBoneIndex( const char *pName )
{
	for (int i = 0; i < m_pPhysics->Count(); i++)
	{
		CPhysmesh *pmesh = m_pPhysics->GetMesh(i);
		if ( !strcmpi( pName, pmesh[i].m_boneName ) )
			return i;
	}

	return -1;
}


/*
=================
StudioModel::SetupModel
	based on the body part, figure out which mesh it should be using.
inputs:
	currententity
outputs:
	pstudiomesh
	pmdl
=================
*/

void StudioModel::SetupModel ( int bodypart )
{
	int index;

	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (bodypart > pStudioHdr->numbodyparts())
	{
		// Con_DPrintf ("StudioModel::SetupModel: no such bodypart %d\n", bodypart);
		bodypart = 0;
	}

	mstudiobodyparts_t   *pbodypart = pStudioHdr->pBodypart( bodypart );

	index = m_bodynum / pbodypart->base;
	index = index % pbodypart->nummodels;

	m_pmodel = pbodypart->pModel( index );

	if(first){
		maxNumVertices = m_pmodel->numvertices;
		first = 0;
	}
}


static IMaterial *g_pAlpha;


//-----------------------------------------------------------------------------
// Draws a box, not wireframed
//-----------------------------------------------------------------------------

void StudioModel::drawBox (Vector const *v, float const * color )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;

	// The four sides
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 * 4 );
	for (int i = 0; i < 10; i++)
	{
		meshBuilder.Position3fv (v[i & 7].Base() );
		meshBuilder.Color4fv( color );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End();
	pMesh->Draw();

	// top and bottom
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

	meshBuilder.Position3fv (v[6].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[0].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[4].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[2].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

	meshBuilder.Position3fv (v[1].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[7].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[3].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[5].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Draws a wireframed box
//-----------------------------------------------------------------------------

void StudioModel::drawWireframeBox (Vector const *v, float const* color )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;

	// The four sides
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 4 );
	for (int i = 0; i < 10; i++)
	{
		meshBuilder.Position3fv (v[i & 7].Base());
		meshBuilder.Color4fv( color );
		meshBuilder.AdvanceVertex();
	}
	meshBuilder.End();
	pMesh->Draw();

	// top and bottom
	meshBuilder.Begin( pMesh, MATERIAL_LINE_STRIP, 4 );

	meshBuilder.Position3fv (v[6].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[0].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[2].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[4].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[6].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	meshBuilder.Begin( pMesh, MATERIAL_LINE_STRIP, 4 );

	meshBuilder.Position3fv (v[1].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[7].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[5].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[3].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3fv (v[1].Base());
	meshBuilder.Color4fv( color );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Draws the position and axies of a transformation matrix, x=red,y=green,z=blue
//-----------------------------------------------------------------------------
void StudioModel::drawTransform( matrix3x4_t& m, float flLength )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;

	for (int k = 0; k < 3; k++)
	{
		static unsigned char color[3][3] =
		{
			{ 255, 0, 0 },
			{ 0, 255, 0 },
			{ 0, 0, 255 }
		};

		meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );

		meshBuilder.Color3ubv( color[k] );
		meshBuilder.Position3f( m[0][3], m[1][3], m[2][3]);
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3ubv( color[k] );
		meshBuilder.Position3f( m[0][3] + m[0][k] * flLength, m[1][3] + m[1][k] * flLength, m[2][3] + m[2][k] * flLength);
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

void drawLine( Vector const &p1, Vector const &p2, int r, int g, int b, bool noDepthTest, float duration )
{
	g_pStudioModel->drawLine( p1, p2, r, g, b );
}


void StudioModel::drawLine( Vector const &p1, Vector const &p2, int r, int g, int b )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( g_materialVertexColor );

	IMesh* pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;

	meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );

	meshBuilder.Color3ub( r, g, b );
	meshBuilder.Position3f( p1.x, p1.y, p1.z );
	meshBuilder.AdvanceVertex();

	meshBuilder.Color3ub( r, g, b );
	meshBuilder.Position3f(  p2.x, p2.y, p2.z );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

#define CHAR_WIDTH 0.0625f // 1/16
#define CHAR_HEIGHT 0.0625f // 1/16

float buildTextCharQuad( CMeshBuilder* pMeshBuilder, const char* szLetter, Vector const &pos, float flScale )
{
	Vector vecPos;
	VectorCopy( pos, vecPos );

	float flPolyWidth = flScale;
	float flPolyHeight = flScale;

	vecPos.z -= flPolyHeight * 0.5f;

	int nCharIdx = (int)((char)*szLetter) - 32;
	int nRow = nCharIdx / 16;
	int nCol = nCharIdx % 16;

	float flU = (nCol * CHAR_WIDTH);
	float flV = (nRow * CHAR_HEIGHT);

	flV += CHAR_HEIGHT;
	pMeshBuilder->Position3fv( vecPos.Base() );
	pMeshBuilder->TexCoord2f( 0, flU, flV );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->AdvanceVertex();

	vecPos.z += flPolyHeight;
	flV -= CHAR_HEIGHT;
	pMeshBuilder->Position3fv( vecPos.Base() );
	pMeshBuilder->TexCoord2f( 0, flU, flV );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->AdvanceVertex();

	vecPos.y += flPolyWidth;
	flU += CHAR_WIDTH;
	pMeshBuilder->Position3fv( vecPos.Base() );
	pMeshBuilder->TexCoord2f( 0, flU, flV );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->AdvanceVertex();

	vecPos.z -= flPolyHeight;
	flV += CHAR_HEIGHT;
	pMeshBuilder->Position3fv( vecPos.Base() );
	pMeshBuilder->TexCoord2f( 0, flU, flV );
	pMeshBuilder->Color4ub( 255, 255, 255, 255 );
	pMeshBuilder->AdvanceVertex();

	return flPolyWidth * 0.6f;
}

void StudioModel::drawText( Vector const &pos, const char* szText )
{
	int nNumChars = strlen( szText );
	if ( !nNumChars )
		return;

	Vector vecStartPos;
	VectorCopy( pos, vecStartPos );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( g_materialDebugText );

	float screenSize = 50.0f / MAX( pRenderContext->ComputePixelWidthOfSphere( vecStartPos, 1.0f ), 0.001f );

	IMesh* pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;

	meshBuilder.Begin( pMesh, MATERIAL_QUADS, nNumChars );
	
	for ( int i=0; i<nNumChars; i++ )
	{
		vecStartPos.y += buildTextCharQuad( &meshBuilder, szText, vecStartPos, screenSize );
		szText++;
	}
	
	meshBuilder.End();
	pMesh->Draw();
}

// todo: draw a capsule procedurally instead of using these baked-in unit capsule verts
#define CAPSULE_VERTS 290
#define CAPSULE_TRIS 576
#define CAPSULE_LINES 148

static float g_capsuleVertPositions[CAPSULE_VERTS][3]={
	{-0.02,1.0,0.0},{-0.27,0.96,0.0},{-0.52,0.86,0.0},{-0.72,0.7,0.0},{-0.88,0.5,0.0},{-0.98,0.25,0.0},{-1.02,0.0,0.0},{-0.98,-0.26,0.0},
	{-0.88,-0.51,0.0},{-0.72,-0.71,0.0},{-0.52,-0.87,0.0},{-0.27,-0.97,0.0},{-0.02,-1.01,0.0},{0.24,-0.97,0.0},{0.48,-0.87,0.0},{0.69,-0.71,0.0},
	{0.85,-0.51,0.0},{0.95,-0.26,0.0},{0.98,-0.01,0.0},{0.95,0.25,0.0},{0.85,0.5,0.0},{0.69,0.7,0.0},{0.48,0.86,0.0},{0.24,0.96,0.0},{-0.02,0.96,-0.26},
	{-0.27,0.93,-0.26},{-0.5,0.83,-0.26},{-0.7,0.68,-0.26},{-0.85,0.48,-0.26},{-0.95,0.25,-0.26},{-0.98,0.0,-0.26},{-0.95,-0.26,-0.26},{-0.85,-0.49,-0.26},
	{-0.7,-0.69,-0.26},{-0.5,-0.84,-0.26},{-0.27,-0.94,-0.26},{-0.02,-0.97,-0.26},{0.23,-0.94,-0.26},{0.47,-0.84,-0.26},{0.67,-0.69,-0.26},{0.82,-0.49,-0.26},
	{0.92,-0.26,-0.26},{0.95,-0.01,-0.26},{0.92,0.25,-0.26},{0.82,0.48,-0.26},{0.67,0.68,-0.26},{0.47,0.83,-0.26},{0.23,0.93,-0.26},{-0.02,0.86,-0.51},
	{-0.24,0.83,-0.51},{-0.45,0.75,-0.51},{-0.63,0.61,-0.51},{-0.77,0.43,-0.51},{-0.85,0.22,-0.51},{-0.88,0.0,-0.51},{-0.85,-0.23,-0.51},{-0.77,-0.44,-0.51},
	{-0.63,-0.62,-0.51},{-0.45,-0.76,-0.51},{-0.24,-0.84,-0.51},{-0.02,-0.87,-0.51},{0.21,-0.84,-0.51},{0.42,-0.76,-0.51},{0.6,-0.62,-0.51},{0.73,-0.44,-0.51},
	{0.82,-0.23,-0.51},{0.85,-0.01,-0.51},{0.82,0.22,-0.51},{0.73,0.43,-0.51},{0.6,0.61,-0.51},{0.42,0.75,-0.51},{0.21,0.83,-0.51},{-0.02,0.7,-0.71},
	{-0.2,0.68,-0.71},{-0.37,0.61,-0.71},{-0.52,0.5,-0.71},{-0.63,0.35,-0.71},{-0.7,0.18,-0.71},{-0.72,0.0,-0.71},{-0.7,-0.19,-0.71},{-0.63,-0.36,-0.71},
	{-0.52,-0.51,-0.71},{-0.37,-0.62,-0.71},{-0.2,-0.69,-0.71},{-0.02,-0.71,-0.71},{0.17,-0.69,-0.71},{0.34,-0.62,-0.71},{0.48,-0.51,-0.71},{0.6,-0.36,-0.71},
	{0.67,-0.19,-0.71},{0.69,-0.01,-0.71},{0.67,0.18,-0.71},{0.6,0.35,-0.71},{0.48,0.5,-0.71},{0.34,0.61,-0.71},{0.17,0.68,-0.71},{-0.02,0.5,-0.87},
	{-0.14,0.48,-0.87},{-0.27,0.43,-0.87},{-0.37,0.35,-0.87},{-0.45,0.25,-0.87},{-0.5,0.12,-0.87},{-0.52,0.0,-0.87},{-0.5,-0.13,-0.87},{-0.45,-0.26,-0.87},
	{-0.37,-0.36,-0.87},{-0.27,-0.44,-0.87},{-0.14,-0.49,-0.87},{-0.02,-0.51,-0.87},{0.11,-0.49,-0.87},{0.23,-0.44,-0.87},{0.34,-0.36,-0.87},{0.42,-0.26,-0.87},
	{0.47,-0.13,-0.87},{0.48,-0.01,-0.87},{0.47,0.12,-0.87},{0.42,0.25,-0.87},{0.34,0.35,-0.87},{0.23,0.43,-0.87},{0.11,0.48,-0.87},{-0.02,0.25,-0.97},
	{-0.08,0.25,-0.97},{-0.14,0.22,-0.97},{-0.2,0.18,-0.97},{-0.24,0.12,-0.97},{-0.27,0.06,-0.97},{-0.27,0.0,-0.97},{-0.27,-0.07,-0.97},{-0.24,-0.13,-0.97},
	{-0.2,-0.19,-0.97},{-0.14,-0.23,-0.97},{-0.08,-0.26,-0.97},{-0.02,-0.26,-0.97},{0.05,-0.26,-0.97},{0.11,-0.23,-0.97},{0.17,-0.19,-0.97},{0.21,-0.13,-0.97},
	{0.23,-0.07,-0.97},{0.24,-0.01,-0.97},{0.23,0.06,-0.97},{0.21,0.12,-0.97},{0.17,0.18,-0.97},{0.11,0.22,-0.97},{0.05,0.25,-0.97},{-0.02,0.0,-1.01},
	{-0.27,-0.97,0.0},{-0.02,-1.01,0.0},{0.24,-0.97,0.0},{0.48,-0.87,0.0},{0.69,-0.71,0.0},{0.85,-0.51,0.0},{0.95,-0.26,0.0},{0.98,-0.01,0.0},{0.95,0.25,0.0},
	{0.85,0.5,0.0},{-0.02,1.0,0.0},{0.24,0.96,0.0},{0.48,0.86,0.0},{0.69,0.7,0.0},{-0.52,-0.87,0.0},{-0.72,-0.71,0.0},{-0.88,-0.51,0.0},{-0.98,-0.26,0.0},
	{-1.02,-0.01,0.0},{-0.98,0.25,0.0},{-0.88,0.5,0.0},{-0.72,0.7,0.0},{-0.52,0.86,0.0},{-0.27,0.96,0.0},{-0.02,0.96,0.25},{0.23,0.93,0.25},{0.47,0.83,0.25},
	{0.67,0.68,0.25},{0.82,0.48,0.25},{0.92,0.25,0.25},{0.95,-0.01,0.25},{0.92,-0.26,0.25},{0.82,-0.49,0.25},{0.67,-0.69,0.25},{0.47,-0.84,0.25},
	{0.23,-0.94,0.25},{-0.02,-0.97,0.25},{-0.27,-0.94,0.25},{-0.5,-0.84,0.25},{-0.7,-0.69,0.25},{-0.85,-0.49,0.25},{-0.95,-0.26,0.25},{-0.98,-0.01,0.25},
	{-0.95,0.25,0.25},{-0.85,0.48,0.25},{-0.7,0.68,0.25},{-0.5,0.83,0.25},{-0.27,0.93,0.25},{-0.02,0.86,0.5},{0.21,0.83,0.5},{0.42,0.75,0.5},{0.6,0.61,0.5},
	{0.73,0.43,0.5},{0.82,0.22,0.5},{0.85,-0.01,0.5},{0.82,-0.23,0.5},{0.73,-0.44,0.5},{0.6,-0.62,0.5},{0.42,-0.76,0.5},{0.21,-0.84,0.5},{-0.02,-0.87,0.5},
	{-0.24,-0.84,0.5},{-0.45,-0.76,0.5},{-0.63,-0.62,0.5},{-0.77,-0.44,0.5},{-0.85,-0.23,0.5},{-0.88,-0.01,0.5},{-0.85,0.22,0.5},{-0.77,0.43,0.5},
	{-0.63,0.61,0.5},{-0.45,0.75,0.5},{-0.24,0.83,0.5},{-0.02,0.7,0.7},{0.17,0.68,0.7},{0.34,0.61,0.7},{0.48,0.5,0.7},{0.6,0.35,0.7},{0.67,0.18,0.7},
	{0.69,-0.01,0.7},{0.67,-0.19,0.7},{0.6,-0.36,0.7},{0.48,-0.51,0.7},{0.34,-0.62,0.7},{0.17,-0.69,0.7},{-0.02,-0.71,0.7},{-0.2,-0.69,0.7},{-0.37,-0.62,0.7},
	{-0.52,-0.51,0.7},{-0.63,-0.36,0.7},{-0.7,-0.19,0.7},{-0.72,-0.01,0.7},{-0.7,0.18,0.7},{-0.63,0.35,0.7},{-0.52,0.5,0.7},{-0.37,0.61,0.7},{-0.2,0.68,0.7},
	{-0.02,0.5,0.86},{0.11,0.48,0.86},{0.23,0.43,0.86},{0.34,0.35,0.86},{0.42,0.25,0.86},{0.47,0.12,0.86},{0.48,-0.01,0.86},{0.47,-0.13,0.86},{0.42,-0.26,0.86},
	{0.34,-0.36,0.86},{0.23,-0.44,0.86},{0.11,-0.49,0.86},{-0.02,-0.51,0.86},{-0.14,-0.49,0.86},{-0.27,-0.44,0.86},{-0.37,-0.36,0.86},{-0.45,-0.26,0.86},
	{-0.5,-0.13,0.86},{-0.52,-0.01,0.86},{-0.5,0.12,0.86},{-0.45,0.25,0.86},{-0.37,0.35,0.86},{-0.27,0.43,0.86},{-0.14,0.48,0.86},{-0.02,0.25,0.96},
	{0.05,0.25,0.96},{0.11,0.22,0.96},{0.17,0.18,0.96},{0.21,0.12,0.96},{0.23,0.06,0.96},{0.24,-0.01,0.96},{0.23,-0.07,0.96},{0.21,-0.13,0.96},
	{0.17,-0.19,0.96},{0.11,-0.23,0.96},{0.05,-0.26,0.96},{-0.02,-0.26,0.96},{-0.08,-0.26,0.96},{-0.14,-0.23,0.96},{-0.2,-0.19,0.96},{-0.24,-0.13,0.96},
	{-0.27,-0.07,0.96},{-0.27,-0.01,0.96},{-0.27,0.06,0.96},{-0.24,0.12,0.96},{-0.2,0.18,0.96},{-0.14,0.22,0.96},{-0.08,0.25,0.96},{-0.02,-0.01,1.0},
};

static int g_capsuleTriIndices[ CAPSULE_TRIS ][ 3 ] = {
	{25,1,0},{0,24,25},{26,2,1},{1,25,26},{27,3,2},{2,26,27},{28,4,3},{3,27,28},{29,5,4},{4,28,29},{30,6,5},{5,29,30},{31,7,6},{6,30,31},{32,8,7},{7,31,32},
	{33,9,8},{8,32,33},{34,10,9},{9,33,34},{35,11,10},{10,34,35},{36,12,11},{11,35,36},{37,13,12},{12,36,37},{38,14,13},{13,37,38},{39,15,14},{14,38,39},
	{40,16,15},{15,39,40},{41,17,16},{16,40,41},{42,18,17},{17,41,42},{43,19,18},{18,42,43},{44,20,19},{19,43,44},{45,21,20},{20,44,45},{46,22,21},{21,45,46},
	{47,23,22},{22,46,47},{24,0,23},{23,47,24},{49,25,24},{24,48,49},{50,26,25},{25,49,50},{51,27,26},{26,50,51},{52,28,27},{27,51,52},{53,29,28},{28,52,53},
	{54,30,29},{29,53,54},{55,31,30},{30,54,55},{56,32,31},{31,55,56},{57,33,32},{32,56,57},{58,34,33},{33,57,58},{59,35,34},{34,58,59},{60,36,35},{35,59,60},
	{61,37,36},{36,60,61},{62,38,37},{37,61,62},{63,39,38},{38,62,63},{64,40,39},{39,63,64},{65,41,40},{40,64,65},{66,42,41},{41,65,66},{67,43,42},{42,66,67},
	{68,44,43},{43,67,68},{69,45,44},{44,68,69},{70,46,45},{45,69,70},{71,47,46},{46,70,71},{48,24,47},{47,71,48},{73,49,48},{48,72,73},{74,50,49},{49,73,74},
	{75,51,50},{50,74,75},{76,52,51},{51,75,76},{77,53,52},{52,76,77},{78,54,53},{53,77,78},{79,55,54},{54,78,79},{80,56,55},{55,79,80},{81,57,56},{56,80,81},
	{82,58,57},{57,81,82},{83,59,58},{58,82,83},{84,60,59},{59,83,84},{85,61,60},{60,84,85},{86,62,61},{61,85,86},{87,63,62},{62,86,87},{88,64,63},{63,87,88},
	{89,65,64},{64,88,89},{90,66,65},{65,89,90},{91,67,66},{66,90,91},{92,68,67},{67,91,92},{93,69,68},{68,92,93},{94,70,69},{69,93,94},{95,71,70},{70,94,95},
	{72,48,71},{71,95,72},{97,73,72},{72,96,97},{98,74,73},{73,97,98},{99,75,74},{74,98,99},{100,76,75},{75,99,100},{101,77,76},{76,100,101},{102,78,77},
	{77,101,102},{103,79,78},{78,102,103},{104,80,79},{79,103,104},{105,81,80},{80,104,105},{106,82,81},{81,105,106},{107,83,82},{82,106,107},{108,84,83},
	{83,107,108},{109,85,84},{84,108,109},{110,86,85},{85,109,110},{111,87,86},{86,110,111},{112,88,87},{87,111,112},{113,89,88},{88,112,113},{114,90,89},
	{89,113,114},{115,91,90},{90,114,115},{116,92,91},{91,115,116},{117,93,92},{92,116,117},{118,94,93},{93,117,118},{119,95,94},{94,118,119},{96,72,95},
	{95,119,96},{121,97,96},{96,120,121},{122,98,97},{97,121,122},{123,99,98},{98,122,123},{124,100,99},{99,123,124},{125,101,100},{100,124,125},{126,102,101},
	{101,125,126},{127,103,102},{102,126,127},{128,104,103},{103,127,128},{129,105,104},{104,128,129},{130,106,105},{105,129,130},{131,107,106},{106,130,131},
	{132,108,107},{107,131,132},{133,109,108},{108,132,133},{134,110,109},{109,133,134},{135,111,110},{110,134,135},{136,112,111},{111,135,136},{137,113,112},
	{112,136,137},{138,114,113},{113,137,138},{139,115,114},{114,138,139},{140,116,115},{115,139,140},{141,117,116},{116,140,141},{142,118,117},{117,141,142},
	{143,119,118},{118,142,143},{120,96,119},{119,143,120},{144,121,120},{144,122,121},{144,123,122},{144,124,123},{144,125,124},{144,126,125},{144,127,126},
	{144,128,127},{144,129,128},{144,130,129},{144,131,130},{144,132,131},{144,133,132},{144,134,133},{144,135,134},{144,136,135},{144,137,136},{144,138,137},
	{144,139,138},{144,140,139},{144,141,140},{144,142,141},{144,143,142},{144,120,143},{0,1,168},{168,155,0},{1,2,167},{167,168,1},{2,3,166},{166,167,2},
	{3,4,165},{165,166,3},{4,5,164},{164,165,4},{5,6,163},{163,164,5},{6,7,162},{162,163,6},{7,8,161},{161,162,7},{8,9,160},{160,161,8},{9,10,159},{159,160,9},
	{10,11,145},{145,159,10},{11,12,146},{146,145,11},{12,13,147},{147,146,12},{13,14,148},{148,147,13},{14,15,149},{149,148,14},{15,16,150},{150,149,15},
	{16,17,151},{151,150,16},{17,18,152},{152,151,17},{18,19,153},{153,152,18},{19,20,154},{154,153,19},{20,21,158},{158,154,20},{21,22,157},{157,158,21},
	{22,23,156},{156,157,22},{23,0,155},{155,156,23},{170,156,155},{155,169,170},{171,157,156},{156,170,171},{172,158,157},{157,171,172},{173,154,158},
	{158,172,173},{174,153,154},{154,173,174},{175,152,153},{153,174,175},{176,151,152},{152,175,176},{177,150,151},{151,176,177},{178,149,150},{150,177,178},
	{179,148,149},{149,178,179},{180,147,148},{148,179,180},{181,146,147},{147,180,181},{182,145,146},{146,181,182},{183,159,145},{145,182,183},{184,160,159},
	{159,183,184},{185,161,160},{160,184,185},{186,162,161},{161,185,186},{187,163,162},{162,186,187},{188,164,163},{163,187,188},{189,165,164},{164,188,189},
	{190,166,165},{165,189,190},{191,167,166},{166,190,191},{192,168,167},{167,191,192},{169,155,168},{168,192,169},{194,170,169},{169,193,194},{195,171,170},
	{170,194,195},{196,172,171},{171,195,196},{197,173,172},{172,196,197},{198,174,173},{173,197,198},{199,175,174},{174,198,199},{200,176,175},{175,199,200},
	{201,177,176},{176,200,201},{202,178,177},{177,201,202},{203,179,178},{178,202,203},{204,180,179},{179,203,204},{205,181,180},{180,204,205},{206,182,181},
	{181,205,206},{207,183,182},{182,206,207},{208,184,183},{183,207,208},{209,185,184},{184,208,209},{210,186,185},{185,209,210},{211,187,186},{186,210,211},
	{212,188,187},{187,211,212},{213,189,188},{188,212,213},{214,190,189},{189,213,214},{215,191,190},{190,214,215},{216,192,191},{191,215,216},{193,169,192},
	{192,216,193},{218,194,193},{193,217,218},{219,195,194},{194,218,219},{220,196,195},{195,219,220},{221,197,196},{196,220,221},{222,198,197},{197,221,222},
	{223,199,198},{198,222,223},{224,200,199},{199,223,224},{225,201,200},{200,224,225},{226,202,201},{201,225,226},{227,203,202},{202,226,227},{228,204,203},
	{203,227,228},{229,205,204},{204,228,229},{230,206,205},{205,229,230},{231,207,206},{206,230,231},{232,208,207},{207,231,232},{233,209,208},{208,232,233},
	{234,210,209},{209,233,234},{235,211,210},{210,234,235},{236,212,211},{211,235,236},{237,213,212},{212,236,237},{238,214,213},{213,237,238},{239,215,214},
	{214,238,239},{240,216,215},{215,239,240},{217,193,216},{216,240,217},{242,218,217},{217,241,242},{243,219,218},{218,242,243},{244,220,219},{219,243,244},
	{245,221,220},{220,244,245},{246,222,221},{221,245,246},{247,223,222},{222,246,247},{248,224,223},{223,247,248},{249,225,224},{224,248,249},{250,226,225},
	{225,249,250},{251,227,226},{226,250,251},{252,228,227},{227,251,252},{253,229,228},{228,252,253},{254,230,229},{229,253,254},{255,231,230},{230,254,255},
	{256,232,231},{231,255,256},{257,233,232},{232,256,257},{258,234,233},{233,257,258},{259,235,234},{234,258,259},{260,236,235},{235,259,260},{261,237,236},
	{236,260,261},{262,238,237},{237,261,262},{263,239,238},{238,262,263},{264,240,239},{239,263,264},{241,217,240},{240,264,241},{266,242,241},{241,265,266},
	{267,243,242},{242,266,267},{268,244,243},{243,267,268},{269,245,244},{244,268,269},{270,246,245},{245,269,270},{271,247,246},{246,270,271},{272,248,247},
	{247,271,272},{273,249,248},{248,272,273},{274,250,249},{249,273,274},{275,251,250},{250,274,275},{276,252,251},{251,275,276},{277,253,252},{252,276,277},
	{278,254,253},{253,277,278},{279,255,254},{254,278,279},{280,256,255},{255,279,280},{281,257,256},{256,280,281},{282,258,257},{257,281,282},{283,259,258},
	{258,282,283},{284,260,259},{259,283,284},{285,261,260},{260,284,285},{286,262,261},{261,285,286},{287,263,262},{262,286,287},{288,264,263},{263,287,288},
	{265,241,264},{264,288,265},{289,266,265},{289,267,266},{289,268,267},{289,269,268},{289,270,269},{289,271,270},{289,272,271},{289,273,272},{289,274,273},
	{289,275,274},{289,276,275},{289,277,276},{289,278,277},{289,279,278},{289,280,279},{289,281,280},{289,282,281},{289,283,282},{289,284,283},{289,285,284},
	{289,286,285},{289,287,286},{289,288,287},{289,265,288},
};

static int g_capsuleLineIndices[CAPSULE_LINES][2] = {
	{ 0, 1 },		{ 0, 24 },		{ 1, 2 },		{ 2, 3 },		{ 3, 4 },		{ 4, 5 },		{ 6, 30 },		{ 5, 6 },		{ 6, 7 },		{ 7, 8 },		{ 8, 9 },		{ 9, 10 },		{ 10, 11 },		{ 12, 36 },
	{ 11, 12 },		{ 12, 13 },		{ 13, 14 },		{ 14, 15 },		{ 15, 16 },		{ 16, 17 },		{ 18, 42 },		{ 17, 18 },		{ 18, 19 },		{ 19, 20 },		{ 20, 21 },		{ 21, 22 },		{ 22, 23 },		{ 0, 23 },
	{ 24, 48 },		{ 48, 49 },		{ 49, 50 },		{ 50, 51 },		{ 51, 52 },		{ 52, 53 },		{ 30, 54 },		{ 53, 54 },		{ 54, 55 },		{ 55, 56 },		{ 56, 57 },		{ 57, 58 },		{ 58, 59 },		{ 36, 60 },
	{ 59, 60 },		{ 60, 61 },		{ 61, 62 },		{ 62, 63 },		{ 63, 64 },		{ 64, 65 },		{ 42, 66 },		{ 65, 66 },		{ 66, 67 },		{ 67, 68 },		{ 68, 69 },		{ 69, 70 },		{ 70, 71 },		{ 48, 71 },
	{ 48, 72 },		{ 54, 78 },		{ 60, 84 },		{ 66, 90 },		{ 72, 96 },		{ 78, 102 },	{ 84, 108 },	{ 90, 114 },	{ 96, 120 },	{ 102, 126 },	{ 108, 132 },	{ 114, 138 },	{ 120, 144 },	{ 126, 144 },	
	{ 132, 144 },	{ 138, 144 },	{ 155, 168 },	{ 0, 155 },		{ 167, 168 },	{ 166, 167 },	{ 165, 166 },	{ 164, 165 },	{ 6, 163 },		{ 163, 164 },	{ 162, 163 },	{ 161, 162 },	{ 160, 161 },	{ 159, 160 },	
	{ 145, 159 },	{ 12, 146 },	{ 145, 146 },	{ 146, 147 },	{ 147, 148 },	{ 148, 149 },	{ 149, 150 },	{ 150, 151 },	{ 18, 152 },	{ 151, 152 },	{ 152, 153 },	{ 153, 154 },	{ 154, 158 },	{ 157, 158 },	
	{ 156, 157 },	{ 155, 156 },	{ 155, 169 },	{ 152, 175 },	{ 146, 181 },	{ 163, 187 },	{ 169, 193 },	{ 193, 194 },	{ 194, 195 },	{ 195, 196 },	{ 196, 197 },	{ 197, 198 },	{ 175, 199 },	{ 198, 199 },
	{ 199, 200 },	{ 200, 201 },	{ 201, 202 },	{ 202, 203 },	{ 203, 204 },	{ 181, 205 },	{ 204, 205 },	{ 205, 206 },	{ 206, 207 },	{ 207, 208 },	{ 208, 209 },	{ 209, 210 },	{ 187, 211 },	{ 210, 211 },	
	{ 211, 212 },	{ 212, 213 },	{ 213, 214 },	{ 214, 215 },	{ 215, 216 },	{ 193, 216 },	{ 193, 217 },	{ 199, 223 },	{ 205, 229 },	{ 211, 235 },	{ 217, 241 },	{ 223, 247 },	{ 229, 253 },	{ 235, 259 },	
	{ 241, 265 },	{ 247, 271 },	{ 253, 277 },	{ 259, 283 },	{ 265, 289 },	{ 271, 289 },	{ 277, 289 },	{ 283, 289 },
};


void StudioModel::drawCapsule( Vector const &bbmin, Vector const &bbmax, float flRadius, const matrix3x4_t& m, float const *interiorcolor, float const *wirecolor )
{
	Vector vecCapsuleCoreNormal = ( bbmin - bbmax ).Normalized();
	
	matrix3x4_t matCapsuleRotationSpace;
	VectorMatrix( Vector(0,0,1), matCapsuleRotationSpace );

	matrix3x4_t matCapsuleSpace;
	VectorMatrix( vecCapsuleCoreNormal, matCapsuleSpace );

	Vector v[CAPSULE_VERTS];
	for ( int i=0; i<CAPSULE_VERTS; i++ )
	{
		Vector vecCapsuleVert = Vector( g_capsuleVertPositions[i][0], g_capsuleVertPositions[i][1], g_capsuleVertPositions[i][2] );
		
		VectorRotate( vecCapsuleVert, matCapsuleRotationSpace, vecCapsuleVert );
		VectorRotate( vecCapsuleVert, matCapsuleSpace, vecCapsuleVert );

		vecCapsuleVert *= flRadius;

		if ( i < CAPSULE_VERTS/2 )
		{
			vecCapsuleVert += bbmin;
		}
		else
		{
			vecCapsuleVert += bbmax;
		}
		
		VectorTransform( vecCapsuleVert, m, v[i] );
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	pRenderContext->Bind( g_materialBones );

	IMesh* pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;

	for ( int i=0; i<CAPSULE_LINES; i++ )
	{
		meshBuilder.Begin( pMesh, MATERIAL_LINES, 2 );

		meshBuilder.Position3fv (v[g_capsuleLineIndices[i][0]].Base());
		meshBuilder.Color4fv( wirecolor );
		meshBuilder.AdvanceVertex();
		
		meshBuilder.Position3fv (v[g_capsuleLineIndices[i][1]].Base());
		meshBuilder.Color4fv( wirecolor );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
	}
	pMesh->Draw();

	pRenderContext->Bind( g_pAlpha );

	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, CAPSULE_TRIS );

	for ( int i=0; i<CAPSULE_TRIS; i++ )
	{
		meshBuilder.Position3fv (v[g_capsuleTriIndices[i][2]].Base());
		meshBuilder.Color4fv( interiorcolor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv (v[g_capsuleTriIndices[i][1]].Base());
		meshBuilder.Color4fv( interiorcolor );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3fv (v[g_capsuleTriIndices[i][0]].Base());
		meshBuilder.Color4fv( interiorcolor );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Draws a transparent box with a wireframe outline
//-----------------------------------------------------------------------------
void StudioModel::drawTransparentBox( Vector const &bbmin, Vector const &bbmax, 
					const matrix3x4_t& m, float const *color, float const *wirecolor )
{
	Vector v[8], v2[8];

	v[0][0] = bbmin[0];
	v[0][1] = bbmax[1];
	v[0][2] = bbmin[2];

	v[1][0] = bbmin[0];
	v[1][1] = bbmin[1];
	v[1][2] = bbmin[2];

	v[2][0] = bbmax[0];
	v[2][1] = bbmax[1];
	v[2][2] = bbmin[2];

	v[3][0] = bbmax[0];
	v[3][1] = bbmin[1];
	v[3][2] = bbmin[2];

	v[4][0] = bbmax[0];
	v[4][1] = bbmax[1];
	v[4][2] = bbmax[2];

	v[5][0] = bbmax[0];
	v[5][1] = bbmin[1];
	v[5][2] = bbmax[2];

	v[6][0] = bbmin[0];
	v[6][1] = bbmax[1];
	v[6][2] = bbmax[2];

	v[7][0] = bbmin[0];
	v[7][1] = bbmin[1];
	v[7][2] = bbmax[2];

	VectorTransform (v[0], m, v2[0]);
	VectorTransform (v[1], m, v2[1]);
	VectorTransform (v[2], m, v2[2]);
	VectorTransform (v[3], m, v2[3]);
	VectorTransform (v[4], m, v2[4]);
	VectorTransform (v[5], m, v2[5]);
	VectorTransform (v[6], m, v2[6]);
	VectorTransform (v[7], m, v2[7]);
	
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( g_pAlpha );
	drawBox( v2, color );

	pRenderContext->Bind( g_materialBones );
	drawWireframeBox( v2, wirecolor );
}



void StudioModel::UpdateStudioRenderConfig( bool bWireframe, bool bZBufferWireframe, bool bNormals, bool bTangentFrame )
{
	StudioRenderConfig_t config;
	memset( &config, 0, sizeof( config ) );
	config.fEyeShiftX = 0.0f;
	config.fEyeShiftY = 0.0f;
	config.fEyeShiftZ = 0.0f;
	config.fEyeSize = 0;
	config.drawEntities = 1;
	config.skin = 0;
	config.fullbright = 0;
	config.bEyeMove = true;
	config.bWireframe = bWireframe;

	if ( g_viewerSettings.renderMode == RM_WIREFRAME || g_viewerSettings.softwareSkin || config.bWireframe || bNormals || bTangentFrame )
	{
		config.bSoftwareSkin = true;
	}
	else
	{
		config.bSoftwareSkin = false;
	}

	config.bSoftwareLighting = false;
	config.bNoHardware = false;
	config.bNoSoftware = false;
	config.bTeeth = true;
	config.bEyes = true;
	config.bFlex = true;
	config.bDrawNormals = bNormals;
	config.bDrawTangentFrame = bTangentFrame;
	config.bDrawZBufferedWireframe = bZBufferWireframe;
	config.bShowEnvCubemapOnly = false;
	g_pStudioRender->UpdateConfig( config );

	MaterialSystem_Config_t matSysConfig = g_pMaterialSystem->GetCurrentConfigForVideoCard();
	extern void InitMaterialSystemConfig(MaterialSystem_Config_t *pConfig);
	InitMaterialSystemConfig( &matSysConfig );
	matSysConfig.nFullbright = 0;
	if( g_viewerSettings.renderMode == RM_SMOOTHSHADED )
	{
		matSysConfig.nFullbright = 2;
	}

	if ( g_dxlevel != 0 )
	{
		matSysConfig.dxSupportLevel = g_dxlevel;
	}
	g_pMaterialSystem->OverrideConfig( matSysConfig, false );
}


//-----------------------------------------------------------------------------
// Draws the skeleton
//-----------------------------------------------------------------------------
void StudioModel::DrawBones( )
{
	// draw bones
	if (!g_viewerSettings.showBones && (g_viewerSettings.highlightBone < 0))
		return;

	CStudioHdr *pStudioHdr = GetStudioHdr();
	const mstudiobone_t *pbones = pStudioHdr->pBone( 0 );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( g_materialBones );

	IMesh* pMesh = pRenderContext->GetDynamicMesh( );
	CMeshBuilder meshBuilder;

	bool drawRed = (g_viewerSettings.highlightBone >= 0);

	for (int i = 0; i < pStudioHdr->numbones(); i++)
	{
		if ( !(pStudioHdr->pBone( i )->flags & BoneMask()))
			continue;

		if ( pbones[i].parent >= 0 )
		{
			int j = pbones[i].parent;
			if ( (g_viewerSettings.highlightBone < 0 ) || (j == g_viewerSettings.highlightBone) )
			{
				drawTransform( m_pBoneToWorld[i], g_viewerSettings.originAxisLength * 0.1f );

				meshBuilder.Begin( pMesh, MATERIAL_LINES, 1 );

				if (drawRed)
					meshBuilder.Color3ub( 255, 255, 0 );
				else
					meshBuilder.Color3ub( 0, 255, 255 );
				meshBuilder.Position3f( m_pBoneToWorld[j][0][3], m_pBoneToWorld[j][1][3], m_pBoneToWorld[j][2][3]);
				meshBuilder.AdvanceVertex();

				if (drawRed)
					meshBuilder.Color3ub( 255, 255, 0 );
				else
					meshBuilder.Color3ub( 0, 255, 255 );
				meshBuilder.Position3f( m_pBoneToWorld[i][0][3], m_pBoneToWorld[i][1][3], m_pBoneToWorld[i][2][3]);
				meshBuilder.AdvanceVertex();

				meshBuilder.End();
				pMesh->Draw();
			}
		}

		if (g_viewerSettings.highlightBone >= 0)
		{
			if (i != g_viewerSettings.highlightBone)
				continue;
		}

		drawTransform( m_pBoneToWorld[i], g_viewerSettings.originAxisLength * 0.4f );
	}

	if ( g_viewerSettings.showBoneNames )
	{
		for (int i = 0; i < pStudioHdr->numbones(); i++)
		{
			if ( g_viewerSettings.highlightBone >= 0 && i != g_viewerSettings.highlightBone )
				continue;

			char szBoneName[128] = "~"; // a silly bone icon
			V_strcat_safe( szBoneName, pbones[i].pszName() );

			drawText( m_pBoneToWorld[i].GetOrigin(), szBoneName );
		}
	}

	// manadatory to access correct verts
	SetCurrentModel();

	// highlight used vertices with point
	/*
	if (g_viewerSettings.highlightBone >= 0)
	{
		int k, j, n;
		for (i = 0; i < pStudioHdr->numbodyparts; i++)
		{
			for (j = 0; j < pStudioHdr->pBodypart( i )->nummodels; j++)
			{
				mstudiomodel_t *pModel = pStudioHdr->pBodypart( i )->pModel( j );

				const mstudio_modelvertexdata_t *vertData = pModel->GetVertexData();
				Assert( vertData ); // This can only return NULL on X360 for now

				meshBuilder.Begin( pMesh, MATERIAL_POINTS, 1 );

				for (k = 0; k < pModel->numvertices; k++)
				{
					for (n = 0; n < vertData->BoneWeights( k )->numbones; n++)
					{
						if (vertData->BoneWeights( k )->bone[n] == g_viewerSettings.highlightBone)
						{
							Vector tmp;
							Transform( *vertData->Position( k ), vertData->BoneWeights( k ), tmp );

							meshBuilder.Color3ub( 0, 255, 255 );
							meshBuilder.Position3f( tmp.x, tmp.y, tmp.z );
							meshBuilder.AdvanceVertex();
							break;
						}
					}
				}

				meshBuilder.End();
				pMesh->Draw();
			}
		}
	}
	*/
}


//-----------------------------------------------------------------------------
// Draws attachments
//-----------------------------------------------------------------------------
void StudioModel::DrawAttachments( )
{
	if ( !g_viewerSettings.showAttachments )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( g_materialBones );

	CStudioHdr *pStudioHdr = GetStudioHdr();
	for (int i = 0; i < pStudioHdr->GetNumAttachments(); i++)
	{
		mstudioattachment_t &pattachments = (mstudioattachment_t &)pStudioHdr->pAttachment( i );

		matrix3x4_t world;
		ConcatTransforms( m_pBoneToWorld[ pStudioHdr->GetAttachmentBone( i ) ], pattachments.local, world );

		drawTransform( world, g_viewerSettings.originAxisLength * 0.4f );
	}
}


//-----------------------------------------------------------------------------
// Draws Axis
//-----------------------------------------------------------------------------
void StudioModel::DrawOriginAxis( )
{
	if ( !g_viewerSettings.showOriginAxis )
			return;

	const float fAxisLength = g_viewerSettings.originAxisLength;
	if ( fAxisLength <= 0.0f )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( g_materialBones );

	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PushMatrix();;
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PushMatrix();;
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity( );

	pRenderContext->Rotate( -90,  1, 0, 0 );	    // put Z going up
	pRenderContext->Rotate( -90,  0, 0, 1 );

    pRenderContext->Translate( -g_pStudioModel->m_origin[0],  -g_pStudioModel->m_origin[1],  -g_pStudioModel->m_origin[2] );

	pRenderContext->Rotate( g_pStudioModel->m_angles[1],  0, 0, 1 );
    pRenderContext->Rotate( g_pStudioModel->m_angles[0],  0, 1, 0 );
    pRenderContext->Rotate( g_pStudioModel->m_angles[2],  1, 0, 0 );

	IMesh *pMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 3 );

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.Color4ub( 255, 0, 0, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( fAxisLength, 0.0f, 0.0f );
	meshBuilder.Color4ub( 255, 0, 0, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.Color4ub( 0, 255, 0, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, fAxisLength, 0.0f );
	meshBuilder.Color4ub( 0, 255, 0, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.Color4ub( 0, 0, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, fAxisLength );
	meshBuilder.Color4ub( 0, 0, 255, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode(MATERIAL_MODEL);
	pRenderContext->PopMatrix();
	pRenderContext->MatrixMode(MATERIAL_VIEW);
	pRenderContext->PopMatrix();
}


void StudioModel::DrawEditAttachment()
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	int iEditAttachment = g_viewerSettings.m_iEditAttachment;
	if ( iEditAttachment >= 0 && iEditAttachment < pStudioHdr->GetNumAttachments() )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->Bind( g_materialBones );
		
		mstudioattachment_t &pAttachment = (mstudioattachment_t &)pStudioHdr->pAttachment( iEditAttachment );

		matrix3x4_t world;
		ConcatTransforms( m_pBoneToWorld[ pStudioHdr->GetAttachmentBone( iEditAttachment ) ], pAttachment.local, world );

		drawTransform( world, g_viewerSettings.originAxisLength * 0.4f );
	}
}


//-----------------------------------------------------------------------------
// Draws hitboxes
//-----------------------------------------------------------------------------


static float hullcolor[8][4] = 
{
	{ 1.0, 1.0, 1.0, 1.0 },
	{ 1.0, 0.5, 0.5, 1.0 },
	{ 0.5, 1.0, 0.5, 1.0 },
	{ 1.0, 1.0, 0.5, 1.0 },
	{ 0.5, 0.5, 1.0, 1.0 },
	{ 1.0, 0.5, 1.0, 1.0 },
	{ 0.5, 1.0, 1.0, 1.0 },
	{ 1.0, 1.0, 1.0, 1.0 }
};


void StudioModel::DrawHitboxes( )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if (!g_pAlpha)
	{
		g_pAlpha = g_pMaterialSystem->FindMaterial("debug/debughitbox", TEXTURE_GROUP_OTHER, false);
	}

	if (g_viewerSettings.showHitBoxes || (g_viewerSettings.highlightHitbox >= 0))
	{
		int hitboxset = g_MDLViewer->GetCurrentHitboxSet();

		bool bDrawWidget = false;
		matrix3x4_t matWidgetSaved;
		Vector vecWidgetPosSaved;

		HitboxList_t &list = g_pStudioModel->m_HitboxSets[ hitboxset ].m_Hitboxes;
		for (unsigned short j = list.Head(); j != list.InvalidIndex(); j = list.Next(j) )
		{
			
			mstudiobbox_t *pBBox = &list[j].m_BBox;

			float interiorcolor[4];
			int c = pBBox->group % 8;
			interiorcolor[0] = hullcolor[c][0] * 0.7;
			interiorcolor[1] = hullcolor[c][1] * 0.7;
			interiorcolor[2] = hullcolor[c][2] * 0.7;
			interiorcolor[3] = hullcolor[c][3] * 0.4;

			matrix3x4_t hitboxMatrix;
			AngleMatrix( pBBox->angOffsetOrientation, hitboxMatrix);
			MatrixMultiply( m_pBoneToWorld[ pBBox->bone ], hitboxMatrix, hitboxMatrix );

			matrix3x4_t hitboxMatrixPlusWidgetRotation;
			hitboxMatrixPlusWidgetRotation = hitboxMatrix;

			Vector bbMinPreview = pBBox->bbmin;
			Vector bbMaxPreview = pBBox->bbmax;

			float flWidgetDelta = g_pWidgetControl->m_vecWidgetDeltaCoord.x * 0.1f;
			if ( g_pWidgetControl->m_WidgetState != WIDGET_STATE_NONE )
			{
				
				Vector vecDelta;
				vecDelta.Init();

				if ( g_pWidgetControl->m_WidgetState == WIDGET_CHANGE_X )
					vecDelta.x = flWidgetDelta;
				else if ( g_pWidgetControl->m_WidgetState == WIDGET_CHANGE_Z )
					vecDelta.y = flWidgetDelta;
				else if ( g_pWidgetControl->m_WidgetState == WIDGET_CHANGE_Y )
					vecDelta.z = flWidgetDelta;

				g_pWidgetControl->m_vecValue = vecDelta;

			}

			//show unselected hitboxes faintly
			if ( (g_viewerSettings.highlightHitbox >= 0) && (g_viewerSettings.highlightHitbox != j) )
			{
				float interiorcolorDim[4];
				float wirecolorDim[4];
				
				for ( int i=0; i<4; i++ )
				{
					interiorcolorDim[i] = interiorcolor[i] * 0.6;
					wirecolorDim[i] = hullcolor[ c ][i] * 0.6;
				}

				if ( pBBox->flCapsuleRadius > 0 )
				{
					drawCapsule( pBBox->bbmin, pBBox->bbmax, pBBox->flCapsuleRadius, hitboxMatrix, interiorcolorDim, wirecolorDim );
				}
				else
				{
					drawTransparentBox( pBBox->bbmin, pBBox->bbmax, hitboxMatrix, interiorcolorDim, wirecolorDim );
				}

			}
			else if ( (g_viewerSettings.highlightHitbox >= 0) && (g_viewerSettings.highlightHitbox == j) )
			{
				//selected hitbox

				if ( g_viewerSettings.hitboxEditMode == HITBOX_EDIT_ROTATION )
				{
					g_pWidgetControl->m_WidgetType = WIDGET_ROTATE;

					QAngle storedValue = QAngle( g_pWidgetControl->m_vecValue.x, g_pWidgetControl->m_vecValue.y, g_pWidgetControl->m_vecValue.z );
				
					if ( g_pWidgetControl->m_WidgetState == WIDGET_STATE_NONE && g_pWidgetControl->HasStoredValue() )
					{
						pBBox->angOffsetOrientation += storedValue;
						g_pWidgetControl->m_vecValue.Init();
					}

					AngleMatrix( pBBox->angOffsetOrientation + storedValue, hitboxMatrixPlusWidgetRotation);
					MatrixMultiply( m_pBoneToWorld[ pBBox->bone ], hitboxMatrixPlusWidgetRotation, hitboxMatrixPlusWidgetRotation );
				}
				else if ( g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMIN || g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMAX )
				{
					g_pWidgetControl->m_WidgetType = WIDGET_TRANSLATE;

					Vector vecValueSwizzleZXY = Vector( g_pWidgetControl->m_vecValue.z, g_pWidgetControl->m_vecValue.x, g_pWidgetControl->m_vecValue.y );

					if ( g_pWidgetControl->m_WidgetState == WIDGET_STATE_NONE && g_pWidgetControl->HasStoredValue() )
					{
						if ( g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMIN )
							pBBox->bbmin += vecValueSwizzleZXY;
						if ( g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMAX )
							pBBox->bbmax += vecValueSwizzleZXY;
						g_pWidgetControl->m_vecValue.Init();
					}

					if ( g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMIN )
						bbMinPreview += vecValueSwizzleZXY;
					if ( g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMAX )
						bbMaxPreview += vecValueSwizzleZXY;

				}

				if ( pBBox->flCapsuleRadius > 0 )
				{
					drawCapsule( bbMinPreview, bbMaxPreview, pBBox->flCapsuleRadius, hitboxMatrixPlusWidgetRotation, interiorcolor, hullcolor[c] );
				}
				else
				{
					drawTransparentBox( bbMinPreview, bbMaxPreview, hitboxMatrixPlusWidgetRotation, interiorcolor, hullcolor[ c ] );
				}

				bDrawWidget = true;
				
				CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
				pRenderContext->GetMatrix( MATERIAL_VIEW, &matWidgetSaved );
				MatrixMultiply( matWidgetSaved, hitboxMatrixPlusWidgetRotation, matWidgetSaved );
				
				if ( g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMIN )
				{
					vecWidgetPosSaved = bbMinPreview;
				}
				else if ( g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMAX )
				{
					vecWidgetPosSaved = bbMaxPreview;
				}

			}
			else
			{
				if ( pBBox->flCapsuleRadius > 0 )
				{
					drawCapsule( pBBox->bbmin, pBBox->bbmax, pBBox->flCapsuleRadius, hitboxMatrix, interiorcolor, hullcolor[ c ] );
				}
				else
				{
					drawTransparentBox( pBBox->bbmin, pBBox->bbmax, hitboxMatrix, interiorcolor, hullcolor[ c ] );
				}
			}
			
		}

		if ( bDrawWidget && g_pWidgetControl->GetWidgetModel() )
		{
			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
				
			pRenderContext->MatrixMode(MATERIAL_VIEW);
			pRenderContext->PushMatrix();
			
			pRenderContext->LoadMatrix( matWidgetSaved );
			
			if ( g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMIN || g_viewerSettings.hitboxEditMode == HITBOX_EDIT_BBMAX )
			{
				pRenderContext->Translate( vecWidgetPosSaved.x, vecWidgetPosSaved.y, vecWidgetPosSaved.z );
			}
			
			g_pWidgetControl->GetWidgetModel()->DrawWidgetModel( );
			
			pRenderContext->MatrixMode(MATERIAL_VIEW);
			pRenderContext->PopMatrix();
		}

	}

	/*
	float color2[] = { 0, 0.7, 1, 0.6 };
	float wirecolor2[] = { 0, 1, 1, 1.0 };
	drawTransparentBox( pStudioHdr->min, pStudioHdr->max, g_viewtransform, color2, wirecolor2 );
	*/

	if (g_viewerSettings.showSequenceBoxes)
	{
		float color[] = { 0.7, 1, 0, 0.6 };
		float wirecolor[] = { 1, 1, 0, 1.0 };

		drawTransparentBox( pStudioHdr->pSeqdesc( m_sequence ).bbmin, pStudioHdr->pSeqdesc( m_sequence ).bbmax, g_viewtransform, color, wirecolor );
	}
}

void StudioModel::DrawIllumPosition( )
{
	if( !g_viewerSettings.showIllumPosition )
		return;

	CStudioHdr *pStudioHdr = GetStudioHdr();

	Vector modelPt0;
	Vector modelPt1;
	Vector worldPt0;
	Vector worldPt1;

	// draw axis through illum position
	VectorCopy(pStudioHdr->illumposition(), modelPt0);
	VectorCopy(pStudioHdr->illumposition(), modelPt1);
	modelPt0.x -= 4;
	modelPt1.x += 4;
	VectorTransform (modelPt0, g_viewtransform, worldPt0);
	VectorTransform (modelPt1, g_viewtransform, worldPt1);
	drawLine( worldPt0, worldPt1, 255, 0, 0 );

	VectorCopy(pStudioHdr->illumposition(), modelPt0);
	VectorCopy(pStudioHdr->illumposition(), modelPt1);
	modelPt0.y -= 4;
	modelPt1.y += 4;
	VectorTransform (modelPt0, g_viewtransform, worldPt0);
	VectorTransform (modelPt1, g_viewtransform, worldPt1);
	drawLine( worldPt0, worldPt1, 0, 255, 0 );

	VectorCopy(pStudioHdr->illumposition(), modelPt0);
	VectorCopy(pStudioHdr->illumposition(), modelPt1);
	modelPt0.z -= 4;
	modelPt1.z += 4;
	VectorTransform (modelPt0, g_viewtransform, worldPt0);
	VectorTransform (modelPt1, g_viewtransform, worldPt1);
	drawLine( worldPt0, worldPt1, 0, 0, 255 );

}

//-----------------------------------------------------------------------------
// Draws the physics model
//-----------------------------------------------------------------------------

void StudioModel::DrawPhysicsModel( )
{
	if (!g_viewerSettings.showPhysicsModel)
		return;

	if ( g_viewerSettings.renderMode == RM_WIREFRAME )
	{
		for (int i = 0; i < m_pPhysics->Count(); i++)
		{
			CPhysmesh *pmesh = m_pPhysics->GetMesh(i);
			int boneIndex = FindBone(pmesh->m_boneName);
			// show the convex pieces in solid
			DrawPhysConvex( pmesh, boneIndex, g_materialFlatshaded );
		}
	}
	else
	{
		for (int i = 0; i < m_pPhysics->Count(); i++)
		{
			float red[] = { 1.0, 0, 0, 0.25 };
			float yellow[] = { 1.0, 1.0, 0, 0.5 };

			CPhysmesh *pmesh = m_pPhysics->GetMesh(i);
			int boneIndex = FindBone(pmesh->m_boneName);

			if ( boneIndex >= 0 )
			{
				constraint_ragdollparams_t *pConstraintInfo = &pmesh->m_constraint;
				if ( (i+1) == g_viewerSettings.highlightPhysicsBone )
				{
					DrawPhysmesh( pmesh, boneIndex, g_materialBones, red );
					
					if ( pConstraintInfo )
					{
						DrawRangeOfMotionArcs( pmesh, boneIndex, g_materialArcActive );
					}
				}
				else
				{
					if ( g_viewerSettings.highlightPhysicsBone < 1 )
					{
						// yellow for most
						DrawPhysmesh( pmesh, boneIndex, g_materialBones, yellow );

						if ( pConstraintInfo )
						{
							DrawRangeOfMotionArcs( pmesh, boneIndex, g_materialArcInActive );
						}
					}
				}
			}
			else
			{
				DrawPhysmesh( pmesh, -1, g_materialBones, red );
			}
		}
	}
}


void StudioModel::DrawSoftbody()
{
	if ( CSoftbody *pSoftbody = GetSoftbody() )
	{
		{
			CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
			pRenderContext->Bind( g_materialFlatshaded );
			IMesh* pMatMesh = pRenderContext->GetDynamicMesh();
			pSoftbody->Draw( g_viewerSettings.softbodyDrawOptions, pMatMesh );
			pMatMesh->Draw();
		}

/*
		if ( g_viewerSettings.softbodyDrawOptions.m_nLayers & RN_SOFTBODY_DRAW_INDICES )
		{
			const VectorAligned *pPos = pSoftbody->GetNodePositions();
			for ( uint i = 0; i < pSoftbody->GetNodeCount(); ++i )
			{
				
			}	
		}
*/
	}
}




void StudioModel::SetViewTarget( void )
{
	// only valid if the attachment bones are used
	if ((BoneMask() & BONE_USED_BY_ATTACHMENT) == 0)
	{
		return;
	}

	int iEyeAttachment = LookupAttachment( "eyes" );
	
	if (iEyeAttachment == -1)
		return;

	Vector local;
	Vector tmp;

	// look forward
	CStudioHdr *pStudioHdr = GetStudioHdr();
	mstudioattachment_t &patt = (mstudioattachment_t &)pStudioHdr->pAttachment( iEyeAttachment );
	matrix3x4_t attToWorld;
	ConcatTransforms( m_pBoneToWorld[ pStudioHdr->GetAttachmentBone( iEyeAttachment ) ], patt.local, attToWorld ); 
	local = Vector( 32, 0, 0 );
	Vector vEyes;
	MatrixPosition( attToWorld, vEyes );

	// aim the eyes if there's a target
	if (m_vecHeadTargets.Count() > 0 && !m_vecHeadTargets.Tail().m_bSelf)
	{
		VectorITransform( m_vecHeadTargets.Tail().m_vecPosition - vEyes, attToWorld, local );
	}
	
	float flDist = local.Length();

	VectorNormalize( local );

	// calculate animated eye deflection
	Vector eyeDeflect;
	QAngle eyeAng( GetFlexController("eyes_updown"), GetFlexController("eyes_rightleft"), 0 );

	// debugoverlay->AddTextOverlay( m_vecOrigin + Vector( 0, 0, 64 ), 0, 0, "%.2f %.2f", eyeAng.x, eyeAng.y );

	AngleVectors( eyeAng, &eyeDeflect );
	eyeDeflect.x = 0;

	// reduce deflection the more the eye is off center
	// FIXME: this angles make no damn sense
	eyeDeflect = eyeDeflect * (local.x * local.x);
	local = local + eyeDeflect;
	VectorNormalize( local );

	// check to see if the eye is aiming outside the max eye deflection
	float flMaxEyeDeflection = pStudioHdr->MaxEyeDeflection();
	if ( local.x < flMaxEyeDeflection )
	{
		// if so, clamp it to 30 degrees offset
		// debugoverlay->AddTextOverlay( GetAbsOrigin() + Vector( 0, 0, 64 ), 1, 0, "%5.3f %5.3f %5.3f", local.x, local.y, local.z );
		local.x = 0;
		float d = local.LengthSqr();
		if ( d > 0.0f )
		{
			d = sqrtf( ( 1.0f - flMaxEyeDeflection * flMaxEyeDeflection ) / ( local.y*local.y + local.z*local.z ) );
			local.x = flMaxEyeDeflection;
			local.y = local.y * d;
			local.z = local.z * d;
		}
		else
		{
			local.x = 1.0;
		}
	}
	local = local * flDist;
	VectorTransform( local, attToWorld, tmp );

	g_pStudioRender->SetEyeViewTarget( pStudioHdr->GetRenderHdr(), m_bodynum, tmp );
}


float UTIL_VecToYaw( const matrix3x4_t& matrix, const Vector &vec )
{
	Vector tmp = vec;
	VectorNormalize( tmp );

	float x = matrix[0][0] * tmp.x + matrix[1][0] * tmp.y + matrix[2][0] * tmp.z;
	float y = matrix[0][1] * tmp.x + matrix[1][1] * tmp.y + matrix[2][1] * tmp.z;

	if (x == 0.0f && y == 0.0f)
		return 0.0f;
	
	float yaw = atan2( -y, x );

	yaw = RAD2DEG(yaw);

	if (yaw < 0)
		yaw += 360;

	return yaw;
}


float UTIL_VecToPitch( const matrix3x4_t& matrix, const Vector &vec )
{
	float pitch = 0;
	Vector tmp = vec;
	if ( VectorNormalize( tmp ) > 0 )
	{
		float z = matrix[0][2] * tmp.x + matrix[1][2] * tmp.y + matrix[2][2] * tmp.z;
		pitch = RAD2DEG( asin( -z ) );
		if (pitch < 0)
			pitch += 360;
	}
	return pitch;
}


float UTIL_AngleDiff( float destAngle, float srcAngle )
{
	float delta;

	delta = destAngle - srcAngle;
	if ( destAngle > srcAngle )
	{
		while ( delta >= 180 )
			delta -= 360;
	}
	else
	{
		while ( delta <= -180 )
			delta += 360;
	}
	return delta;
}


void StudioModel::UpdateBoneChain(
	Vector pos[], 
	Quaternion q[], 
	int	iBone,
	matrix3x4_t *pBoneToWorld )
{
	matrix3x4_t bonematrix;

	QuaternionMatrix( q[iBone], pos[iBone], bonematrix );

	CStudioHdr *pStudioHdr = GetStudioHdr();
	int parent = pStudioHdr->pBone( iBone )->parent;
	if (parent == -1) 
	{
		ConcatTransforms( g_viewtransform, bonematrix, pBoneToWorld[iBone] );
	}
	else
	{
		// evil recursive!!!
		UpdateBoneChain( pos, q, parent, pBoneToWorld );
		ConcatTransforms( pBoneToWorld[parent], bonematrix, pBoneToWorld[iBone] );
	}
}




void StudioModel::GetBodyPoseParametersFromFlex( )
{
	float flGoal;

	flGoal = GetFlexController( "move_rightleft" );
	SetPoseParameter( "body_trans_Y", flGoal );
	
	flGoal = GetFlexController( "move_forwardback" );
	SetPoseParameter( "body_trans_X", flGoal );

	flGoal = GetFlexController( "move_updown" );
	SetPoseParameter( "body_lift", flGoal );

	flGoal = GetFlexController( "body_rightleft" ) + GetBodyYaw();
	SetPoseParameter( "body_yaw", flGoal );

	flGoal = GetFlexController( "body_updown" );
	SetPoseParameter( "body_pitch", flGoal );

	flGoal = GetFlexController( "body_tilt" );
	SetPoseParameter( "body_roll", flGoal );

	flGoal = GetFlexController( "chest_rightleft" ) + GetSpineYaw();
	SetPoseParameter( "spine_yaw", flGoal );

	flGoal = GetFlexController( "chest_updown" );
	SetPoseParameter( "spine_pitch", flGoal );

	flGoal = GetFlexController( "chest_tilt" );
	SetPoseParameter( "spine_roll", flGoal );

	flGoal = GetFlexController( "head_forwardback" );
	SetPoseParameter( "neck_trans", flGoal );

	flGoal = GetFlexController( "gesture_updown" );
	SetPoseParameter( "gesture_height", flGoal );

	flGoal = GetFlexController( "gesture_rightleft" );
	SetPoseParameter( "gesture_width", flGoal );
}




void StudioModel::CalcHeadRotation( Vector pos[], Quaternion q[] )
{
	static Vector		pos2[MAXSTUDIOBONES];
	static Quaternion	q2[MAXSTUDIOBONES];

	if (m_nSolveHeadTurn == 0)
		return;

	if (m_dt == 0.0f)
	{
		m_dt = 0.1;
	}

	// GetAttachment( "eyes", vEyePosition, vEyeAngles );
	int iForwardAttachment = LookupAttachment( "forward" );
	if (iForwardAttachment == -1)
		return;

	CStudioHdr *pStudioHdr = GetStudioHdr();
	mstudioattachment_t &patt = (mstudioattachment_t &)pStudioHdr->pAttachment( iForwardAttachment );

	matrix3x4_t attToWorld;
	int iBone =  pStudioHdr->GetAttachmentBone( iForwardAttachment );
	BuildBoneChain( pStudioHdr, g_viewtransform, pos, q, iBone, m_pBoneToWorld );
	ConcatTransforms( m_pBoneToWorld[ iBone ], patt.local, attToWorld );

	Vector vForward;
	VectorRotate( Vector( 1, 0, 0 ), attToWorld, vForward );

  	float dt = m_dt;
  	if (m_nSolveHeadTurn == 2)
  	{
  		dt = 0.1;
  	}

	Vector vEyes;
	MatrixPosition( attToWorld, vEyes );
 
	Vector vHead = vForward;
	float flHeadInfluence = 0.0;
	int i;
	for (i = 0; i < m_vecHeadTargets.Count(); i++)
	{
		Vector dir;

		if (m_vecHeadTargets[i].m_bSelf)
		{
			dir = vForward;
		}
		else
		{
			dir = m_vecHeadTargets[i].m_vecPosition - vEyes;
		}
		VectorNormalize( dir );
		float flInterest = m_vecHeadTargets[i].m_flWeight;
		if (flInterest > 0.0)
		{
			if (flHeadInfluence == 0.0)
			{
				vHead = dir;
				flHeadInfluence = flInterest;
			}
			else
			{
				flHeadInfluence = flHeadInfluence * (1 - flInterest) + flInterest;
				float w = flInterest / flHeadInfluence;
				vHead = vHead * (1 - w) + dir * w;
			}
		}
	}

	Vector vTargetDir = Vector( 0, 0, 0 );
	vTargetDir = vForward * (1.0 - flHeadInfluence) + vHead * flHeadInfluence;
	VectorNormalize( vTargetDir );

	SetPoseParameter( "head_pitch", 0.0 );
	SetPoseParameter( "head_yaw", 0.0 );
	SetPoseParameter( "head_roll", 0.0 );
	SetHeadPosition( attToWorld, vTargetDir, dt );

	// Msg( "yaw %f pitch %f\n", vEyeAngles.y, vEyeAngles.x );
}



float StudioModel::SetHeadPosition( matrix3x4_t& attToWorld, Vector const &vTargetPos, float dt )
{
	float flDiff;
	int iPose;
	QAngle vEyeAngles;
	float flMoved = 0.0f;
	matrix3x4a_t targetXform, invAttToWorld;
	matrix3x4_t headXform;

	// align current "forward direction" to target direction
	targetXform = attToWorld;
	Studio_AlignIKMatrix( targetXform, vTargetPos );

	// calc head movement needed
	MatrixInvert( attToWorld, invAttToWorld );
	ConcatTransforms( invAttToWorld, targetXform, headXform );

	MatrixAngles( headXform, vEyeAngles );

	// FIXME: add chest compression

	// Msg( "yaw %f pitch %f\n", vEyeAngles.y, vEyeAngles.x );

	float flMin, flMax;

#if 1
	//--------------------------------------
	// Set head yaw
	//--------------------------------------
	// flDiff = vEyeAngles.y + GetFlexController( "head_rightleft" );
	iPose = LookupPoseParameter( "head_yaw" );
	GetPoseParameterRange( iPose, &flMin, &flMax );
	flDiff = RangeCompressor( vEyeAngles.y + GetFlexController( "head_rightleft" ), flMin, flMax, 0.0 );
	SetPoseParameter( iPose, flDiff );
#endif

#if 1
	//--------------------------------------
	// Set head pitch
	//--------------------------------------
	iPose = LookupPoseParameter( "head_pitch" );
	GetPoseParameterRange( iPose, &flMin, &flMax );
	flDiff = RangeCompressor( vEyeAngles.x + GetFlexController( "head_updown" ), flMin, flMax, 0.0 );
	SetPoseParameter( iPose, flDiff );
#endif

#if 1
	//--------------------------------------
	// Set head roll
	//--------------------------------------
	iPose = LookupPoseParameter( "head_roll" );
	GetPoseParameterRange( iPose, &flMin, &flMax );
	flDiff = RangeCompressor( vEyeAngles.z + GetFlexController( "head_tilt" ), flMin, flMax, 0.0 );
	SetPoseParameter( iPose, flDiff );
#endif

	return flMoved;
}


DrawModelInfo_t g_DrawModelInfo;
DrawModelResults_t g_DrawModelResults;
bool g_bDrawModelInfoValid = false;


void StudioModel::GetModelTransform( matrix3x4_t &mat )
{
	AngleMatrix( m_angles, mat );

	Vector vecModelOrigin;
	VectorMultiply( m_origin, -1.0f, vecModelOrigin );
	MatrixSetColumn( vecModelOrigin, 3, mat );
}

void StudioModel::SetModelTransform( const matrix3x4_t &mat )
{
	m_origin.x = -mat.m_flMatVal[0][3];
	m_origin.y = -mat.m_flMatVal[1][3];
	m_origin.z = -mat.m_flMatVal[2][3];

	MatrixAngles( mat, m_angles );
}


/*
================
StudioModel::DrawModel
inputs:
	currententity
	r_entorigin
================
*/
int StudioModel::DrawModel( bool mergeBones, int nRenderPassMode )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if ( !HasModel())
		return 0;
	CStudioHdr *pStudioHdr = GetStudioHdr();

	g_smodels_total++; // render data cache cookie

	// JasonM & garymcthack - should really only do this once a frame and at init time.
	UpdateStudioRenderConfig( g_viewerSettings.renderMode == RM_WIREFRAME, false,
							  g_viewerSettings.showNormals,
							  g_viewerSettings.showTangentFrame );

	// NOTE: UpdateStudioRenderConfig can delete the studio hdr


	// Construct a transform to apply to the model. The camera is stuck in a fixed position
	GetModelTransform( g_viewtransform );

	Vector vecModelOrigin;
	VectorMultiply( m_origin, -1.0f, vecModelOrigin );
	MatrixSetColumn( vecModelOrigin, 3, g_viewtransform );
	
	// These values HAVE to be sent down for LOD to work correctly.
	Vector viewOrigin, viewRight, viewUp, viewPlaneNormal;
	g_pStudioRender->SetViewState( vec3_origin, Vector(0, 1, 0), Vector(0, 0, 1), Vector( 1, 0, 0 ) );

	//	g_pStudioRender->SetEyeViewTarget( viewOrigin );
	
	SetUpBones( mergeBones );

	SetupLighting( );

	SetViewTarget( );


	extern float g_flexdescweight[MAXSTUDIOFLEXDESC]; // garymcthack
	extern float g_flexdescweight2[MAXSTUDIOFLEXDESC]; // garymcthack

	int i;
	for (i = 0; i < pStudioHdr->numflexdesc(); i++)
	{
		g_flexdescweight[i] = 0.0;
	}

	RunFlexRules( );

	float d = 0.8;

	if (m_dt != 0)
	{
		d = ExponentialDecay( 0.8, 0.033, m_dt );
	}

	float *pFlexWeights = NULL, *pFlexDelayedWeights = NULL;
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CMatRenderData< float > rdFlexWeights( pRenderContext );
	CMatRenderData< float > rdDelayedFlexWeights( pRenderContext );
	int nFlexCount = pStudioHdr->numflexdesc();
	if ( nFlexCount )
	{
		pFlexWeights = rdFlexWeights.Lock( nFlexCount );
		pFlexDelayedWeights = rdDelayedFlexWeights.Lock( nFlexCount );
		for ( i = 0; i < nFlexCount; i++ )
		{
			g_flexdescweight2[i] = g_flexdescweight2[i] * d + g_flexdescweight[i] * (1 - d);

			pFlexWeights[i] = g_flexdescweight[i];
			pFlexDelayedWeights[i] = g_flexdescweight2[i];
		}
	}
	
	// draw

	g_pStudioRender->SetAlphaModulation( 1.0f );

	g_bDrawModelInfoValid = true;
	memset( &g_DrawModelInfo, 0, sizeof( g_DrawModelInfo ) );
	g_DrawModelInfo.m_pStudioHdr = (studiohdr_t *)pStudioHdr->GetRenderHdr();
	g_DrawModelInfo.m_pHardwareData = GetHardwareData();
	if ( !g_DrawModelInfo.m_pHardwareData )
		return 0;
	g_DrawModelInfo.m_Decals = STUDIORENDER_DECAL_INVALID;
	g_DrawModelInfo.m_Skin = m_skinnum;
	g_DrawModelInfo.m_Body = m_bodynum;
	g_DrawModelInfo.m_HitboxSet = g_MDLViewer->GetCurrentHitboxSet();
	g_DrawModelInfo.m_pClientEntity = NULL;
	g_DrawModelInfo.m_Lod = g_viewerSettings.autoLOD ? -1 : g_viewerSettings.lod;
	g_DrawModelInfo.m_pColorMeshes = NULL;

	if( nRenderPassMode == PASS_DEFAULT || nRenderPassMode == PASS_MODELONLY )
	{

	if( g_viewerSettings.renderMode == RM_SHOWBADVERTEXDATA )
	{
		DebugDrawModelBadVerts( g_pStudioRender, g_DrawModelInfo, m_pBoneToWorld, vecModelOrigin );

		DebugDrawModelWireframe( g_pStudioRender, g_DrawModelInfo, m_pBoneToWorld, vecModelOrigin, Vector( 0.2f, 0.2f, 0.2f ) );

		g_DrawModelInfo.m_Lod = m_LodUsed;
		g_pStudioRender->GetPerfStats( &g_DrawModelResults, g_DrawModelInfo, NULL );

#if 0
		// overlay wireframe

		// Set the state to trigger wireframe rendering
		UpdateStudioRenderConfig( true, true, false, false );

		// Draw wireframe
		count = g_pStudioRender->DrawModel( &g_DrawModelResults, g_DrawModelInfo, m_pBoneToWorld, 
			pFlexWeights, pFlexDelayedWeights, vecModelOrigin, STUDIORENDER_DRAW_ENTIRE_MODEL );
		m_LodUsed = g_DrawModelResults.m_nLODUsed;
		m_LodMetric = g_DrawModelResults.m_flLodMetric;
		g_DrawModelInfo.m_Lod = m_LodUsed;

		// Restore the studio render config
		UpdateStudioRenderConfig( g_viewerSettings.renderMode == RM_WIREFRAME, false,
			g_viewerSettings.showNormals,
			g_viewerSettings.showTangentFrame );
#endif
	}
	else if( g_viewerSettings.renderMode == RM_BONEWEIGHTS )
	{
		DebugDrawModelBoneWeights( g_pStudioRender, g_DrawModelInfo, m_pBoneToWorld, vecModelOrigin );
		DebugDrawModelWireframe( g_pStudioRender, g_DrawModelInfo, m_pBoneToWorld, vecModelOrigin, Vector( 0.2f, 0.2f, 0.2f ) );
		g_pStudioRender->GetPerfStats( &g_DrawModelResults, g_DrawModelInfo, NULL );
		m_LodUsed = g_DrawModelInfo.m_Lod;
	}
	else if( g_viewerSettings.renderMode == RM_TEXCOORDS )
	{
		const char *pMatName = "";
		if ( g_DrawModelInfo.m_pHardwareData->m_pLODs && g_viewerSettings.materialIndex < g_DrawModelInfo.m_pHardwareData->m_pLODs[0].numMaterials )
		{
			pMatName = g_DrawModelInfo.m_pHardwareData->m_pLODs[0].ppMaterials[g_viewerSettings.materialIndex]->GetName();
		}
		DebugDrawModelTexCoord( g_pStudioRender, pMatName, g_DrawModelInfo, m_pBoneToWorld, g_viewerSettings.width, g_viewerSettings.height );
		g_pStudioRender->GetPerfStats( &g_DrawModelResults, g_DrawModelInfo, NULL );
		m_LodUsed = g_DrawModelInfo.m_Lod;
	}
	else if ( g_viewerSettings.renderMode == RM_SHOWCOLOCATED )
	{
		DebugDrawModelVertColocation( g_pStudioRender, g_DrawModelInfo, m_pBoneToWorld, vecModelOrigin );
		DebugDrawModelWireframe( g_pStudioRender, g_DrawModelInfo, m_pBoneToWorld, vecModelOrigin, Vector( 0.2f, 0.2f, 0.2f ) );
		g_pStudioRender->GetPerfStats( &g_DrawModelResults, g_DrawModelInfo, NULL );
		m_LodUsed = g_DrawModelInfo.m_Lod;
	}
	else
	{
		// Draw the model normally (may include normal and/or tangent line segments)
		g_pStudioRender->DrawModel( &g_DrawModelResults, g_DrawModelInfo, m_pBoneToWorld, 
			pFlexWeights, pFlexDelayedWeights, vecModelOrigin );
		m_LodUsed = g_DrawModelResults.m_nLODUsed;
		m_LodMetric = g_DrawModelResults.m_flLODMetric;

		g_pStudioRender->GetPerfStats( &g_DrawModelResults, g_DrawModelInfo, NULL );

		// Optionally overlay wireframe...
		if ( g_viewerSettings.overlayWireframe && !(g_viewerSettings.renderMode == RM_WIREFRAME) )
		{
			// Set the state to trigger wireframe rendering
			UpdateStudioRenderConfig( true, true, false, false );

			// Draw the wireframe over top of the model
			g_pStudioRender->DrawModel( NULL, g_DrawModelInfo, m_pBoneToWorld, 
				pFlexWeights, pFlexDelayedWeights, vecModelOrigin );

			// Restore the studio render config
			UpdateStudioRenderConfig( g_viewerSettings.renderMode == RM_WIREFRAME, false,
										g_viewerSettings.showNormals,
										g_viewerSettings.showTangentFrame );
		}
	}

	}

	int nCount = g_DrawModelResults.m_ActualTriCount;

	if ( !mergeBones && (nRenderPassMode == PASS_DEFAULT || nRenderPassMode == PASS_EXTRASONLY) )
	{
		DrawBones();
		DrawAttachments();
		DrawOriginAxis();
		DrawEditAttachment();
		DrawHitboxes();
		DrawPhysicsModel();
		DrawSoftbody();
		DrawIllumPosition();
	}

	// Only draw the shadow if the ground is also drawn
	if ( g_viewerSettings.showShadow &&  g_viewerSettings.showGround )
	{
		matrix3x4_t invViewTransform;

		MatrixInvert( g_viewtransform, invViewTransform );

		for (int i = 0; i < pStudioHdr->numbones(); i++) 
		{
			matrix3x4_t *pMatrix = &m_pBoneToWorld[ i ];

			matrix3x4_t tmp1;

			ConcatTransforms( invViewTransform, *pMatrix, tmp1 );
			tmp1[2][0] = 0.0;
			tmp1[2][1] = 0.0;
			tmp1[2][2] = 0.0;
			tmp1[2][3] = 0.05;
			ConcatTransforms( g_viewtransform, tmp1, *pMatrix );
		}
		g_DrawModelInfo.m_Lod = GetHardwareData()->m_NumLODs - 1;

		float zero[4] = { 0, 0, 0, 0 };
		g_pStudioRender->SetColorModulation( zero );
		g_pStudioRender->ForcedMaterialOverride( g_materialShadow );

		// Turn off any wireframe, normals or tangent frame display for the drop shadow
		UpdateStudioRenderConfig( false, false, false, false );

		g_pStudioRender->DrawModel( NULL, g_DrawModelInfo, m_pBoneToWorld, 
			pFlexWeights, pFlexDelayedWeights, vecModelOrigin );

		// Restore the studio render config
		UpdateStudioRenderConfig( g_viewerSettings.renderMode == RM_WIREFRAME, false,
								  g_viewerSettings.showNormals,
								  g_viewerSettings.showTangentFrame );

		g_pStudioRender->ForcedMaterialOverride( NULL );
		float one[4] = { 1, 1, 1, 1 };
		g_pStudioRender->SetColorModulation( one );
	}

	return nCount;
}


void StudioModel::DrawWidgetModel( )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if ( !HasModel())
		return;
	CStudioHdr *pStudioHdr = GetStudioHdr();

	g_smodels_total++; // render data cache cookie

	// JasonM & garymcthack - should really only do this once a frame and at init time.
	UpdateStudioRenderConfig( g_viewerSettings.renderMode == RM_WIREFRAME, false,
							  g_viewerSettings.showNormals,
							  g_viewerSettings.showTangentFrame );

	// NOTE: UpdateStudioRenderConfig can delete the studio hdr
	if ( !GetStudioHdr() )
		return;

	// Construct a transform to apply to the model. The camera is stuck in a fixed position
	GetModelTransform( g_viewtransform );

	Vector vecModelOrigin;
	VectorMultiply( m_origin, -1.0f, vecModelOrigin );
	MatrixSetColumn( vecModelOrigin, 3, g_viewtransform );
	
	// These values HAVE to be sent down for LOD to work correctly.
	Vector viewOrigin, viewRight, viewUp, viewPlaneNormal;
	g_pStudioRender->SetViewState( vec3_origin, Vector(0, 1, 0), Vector(0, 0, 1), Vector( 1, 0, 0 ) );

	//	g_pStudioRender->SetEyeViewTarget( viewOrigin );
	
	SetUpBones( false );

	SetupLighting( );

	SetViewTarget( );


	// draw

	g_pStudioRender->SetAlphaModulation( 1.0f );

	g_bDrawModelInfoValid = true;
	memset( &g_DrawModelInfo, 0, sizeof( g_DrawModelInfo ) );
	g_DrawModelInfo.m_pStudioHdr = (studiohdr_t *)pStudioHdr->GetRenderHdr();
	g_DrawModelInfo.m_pHardwareData = GetHardwareData();
	if ( !g_DrawModelInfo.m_pHardwareData )
		return;
	g_DrawModelInfo.m_Decals = STUDIORENDER_DECAL_INVALID;
	g_DrawModelInfo.m_Skin = m_skinnum;
	g_DrawModelInfo.m_Body = m_bodynum;
	g_DrawModelInfo.m_HitboxSet = g_MDLViewer->GetCurrentHitboxSet();
	g_DrawModelInfo.m_pClientEntity = NULL;
	g_DrawModelInfo.m_Lod = -1;
	g_DrawModelInfo.m_pColorMeshes = NULL;

	g_pStudioRender->DrawModel( &g_DrawModelResults, g_DrawModelInfo, m_pBoneToWorld, NULL, NULL, vecModelOrigin );
	
}

void StudioModel::DrawRangeOfMotionArcs( CPhysmesh *pMesh, int boneIndex, IMaterial* pMaterial )
{
	matrix3x4_t *pMatrix;
	if ( boneIndex >= 0 )
	{
		pMatrix = &m_pBoneToWorld[ boneIndex ];
	}
	else
	{
		pMatrix = &g_viewtransform;
	}

	for( int iAxis=0; iAxis<3; iAxis++ )
	{

		constraint_ragdollparams_t *pConstraintInfo = &pMesh->m_constraint;
		if ( !pConstraintInfo )
			continue;
		constraint_axislimit_t *axis = &pConstraintInfo->axes[iAxis];

		Vector vecAxis;
		vecAxis.Init();
		vecAxis[iAxis] = 1;

		int iVisAxis = RemapAxis( iAxis );
		
		Vector arcVerts[30];
		for ( int i=0; i<30; i++ )
		{
			float flArc = float(i)/29.0f;

			arcVerts[i].Init();
			arcVerts[i][iVisAxis] = 1;

			VMatrix matRot;
			matRot.Identity();
			MatrixRotate( matRot, vecAxis, axis->minRotation + (axis->maxRotation - axis->minRotation) * flArc );

			VectorTransform( arcVerts[i], matRot.As3x4(), arcVerts[i] );
			arcVerts[i] = arcVerts[i].Normalized() * 4.0f;
		}

		float flColor[] = {0,0,0,0.5};
		flColor[iAxis] = 1;

		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->Bind( pMaterial );
		IMesh* pMatMesh = pRenderContext->GetDynamicMesh( );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMatMesh, MATERIAL_TRIANGLES, 29 );

		int vertIndex = 0;
		for ( int i = 0; i < 32; i+=1 )
		{
			Vector v;
			
			VectorTransform (arcVerts[vertIndex], *pMatrix, v);
			meshBuilder.Position3fv( v.Base() );
			meshBuilder.Color4fv( flColor );
			meshBuilder.AdvanceVertex();

			vertIndex ++;

			VectorTransform (arcVerts[vertIndex], *pMatrix, v);
			meshBuilder.Position3fv( v.Base() );
			meshBuilder.Color4fv( flColor );
			meshBuilder.AdvanceVertex();

			VectorTransform (Vector(0,0,0), *pMatrix, v);
			meshBuilder.Position3fv( v.Base() );							 
			meshBuilder.Color4fv( flColor );
			meshBuilder.AdvanceVertex();
		}
		meshBuilder.End();
		pMatMesh->Draw();

		//draw center line
		{
			Vector vecStart;
			vecStart.Init();
			Vector vecEnd;
			vecEnd.Init();

			vecEnd[iVisAxis] = 4.2f;

			if ( iAxis == m_physPreviewAxis && ( axis->maxRotation != axis->minRotation ) )
			{
				VMatrix matRot;
				matRot.Identity();
				MatrixRotate( matRot, vecAxis, axis->minRotation + (axis->maxRotation - axis->minRotation) * (1.0f-m_physPreviewParam) );
				VectorTransform( vecEnd, matRot.As3x4(), vecEnd );
			}

			VectorTransform (vecStart, *pMatrix, vecStart);
			VectorTransform (vecEnd, *pMatrix, vecEnd);

			drawLine( vecStart, vecEnd, flColor[0]*255, flColor[1]*255, flColor[2]*255 );
		}

	}
}

void StudioModel::DrawPhysmesh( CPhysmesh *pMesh, int boneIndex, IMaterial* pMaterial, float* color )
{
	matrix3x4_t *pMatrix;
	if ( boneIndex >= 0 )
	{
		pMatrix = &m_pBoneToWorld[ boneIndex ];
	}
	else
	{
		pMatrix = &g_viewtransform;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( pMaterial );
	IMesh* pMatMesh = pRenderContext->GetDynamicMesh( );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMatMesh, MATERIAL_TRIANGLES, pMesh->m_vertCount/3 );

	int vertIndex = 0;
	for ( int i = 0; i < pMesh->m_vertCount; i+=3 )
	{
		Vector v;
		
		VectorTransform (pMesh->m_pVerts[vertIndex], *pMatrix, v);
		meshBuilder.Position3fv( v.Base() );
		meshBuilder.Color4fv( color );
		meshBuilder.AdvanceVertex();

		vertIndex ++;
		VectorTransform (pMesh->m_pVerts[vertIndex], *pMatrix, v);
		meshBuilder.Position3fv( v.Base() );
		meshBuilder.Color4fv( color );
		meshBuilder.AdvanceVertex();

		vertIndex ++;
		VectorTransform (pMesh->m_pVerts[vertIndex], *pMatrix, v);
		meshBuilder.Position3fv( v.Base() );							 
		meshBuilder.Color4fv( color );
		meshBuilder.AdvanceVertex();

		vertIndex ++;
	}
	meshBuilder.End();
	pMatMesh->Draw();
}


void RandomColor( float *color, int key )
{
	static bool first = true;
	static colorVec colors[256];

	if ( first )
	{
		int r, g, b;
		first = false;
		for ( int i = 0; i < 256; i++ )
		{
			do 
			{
				r = rand()&255;
				g = rand()&255;
				b = rand()&255;
			} while ( (r+g+b)<256 );
			colors[i].r = r;
			colors[i].g = g;
			colors[i].b = b;
			colors[i].a = 255;
		}
	}

	int index = key & 255;
	color[0] = colors[index].r * (1.f / 255.f);
	color[1] = colors[index].g * (1.f / 255.f);
	color[2] = colors[index].b * (1.f / 255.f);
	color[3] = colors[index].a * (1.f / 255.f);
}

void StudioModel::DrawPhysConvex( CPhysmesh *pMesh, int boneIndex, IMaterial* pMaterial )
{
	matrix3x4_t &matrix = m_pBoneToWorld[ boneIndex ];

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( pMaterial );

	for ( int i = 0; i < pMesh->m_pCollisionModel->ConvexCount(); i++ )
	{
		float color[4];
		RandomColor( color, (i+1) * (boneIndex+1) );
		IMesh* pMatMesh = pRenderContext->GetDynamicMesh( );
		CMeshBuilder meshBuilder;
		int triCount = pMesh->m_pCollisionModel->TriangleCount( i );
		meshBuilder.Begin( pMatMesh, MATERIAL_TRIANGLES, triCount );

		for ( int j = 0; j < triCount; j++ )
		{
			Vector objectSpaceVerts[3];
			pMesh->m_pCollisionModel->GetTriangleVerts( i, j, objectSpaceVerts );

			for ( int k = 0; k < 3; k++ )
			{
				Vector v;
				
				VectorTransform (objectSpaceVerts[k], matrix, v);
				meshBuilder.Position3fv( v.Base() );
				meshBuilder.Color4fv( color );
				meshBuilder.AdvanceVertex();
			}
		}
		meshBuilder.End();
		pMatMesh->Draw();
	}
}



void StudioModel::ExtractVertExtents( Vector &vecMin, Vector &vecMax )
{
	studiohdr_t *pStudioHdr = GetStudioRenderHdr();
	if ( !pStudioHdr )
		return;

	SetUpBones( false );

	DebugModelVertExtents( g_pStudioRender, g_DrawModelInfo, m_pBoneToWorld, vecMin, vecMax );
}



/*
================

================
*/


int StudioModel::GetLodUsed( void )
{
	return m_LodUsed;
}

float StudioModel::GetLodMetric( void )
{
	return m_LodMetric;
}


const char *StudioModel::GetKeyValueText( int iSequence )
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	return Studio_GetKeyValueText( pStudioHdr, iSequence );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : solve - 
//-----------------------------------------------------------------------------
void StudioModel::SetSolveHeadTurn( int solve )
{
	m_nSolveHeadTurn = solve;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int StudioModel::GetSolveHeadTurn() const
{
	return m_nSolveHeadTurn;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : target - 
//-----------------------------------------------------------------------------
void StudioModel::ClearLookTargets( void )
{
	m_vecHeadTargets.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : target - 
//-----------------------------------------------------------------------------
void StudioModel::AddLookTarget( const Vector& vecPosition, float flWeight )
{
	if (m_vecHeadTargets.Count() > 8)
		return;

	StudioLookTarget tmp;

	tmp.m_flWeight = flWeight;
	tmp.m_vecPosition = vecPosition;
	tmp.m_bSelf = false;

	m_vecHeadTargets.AddToTail( tmp );
}


void StudioModel::AddLookTargetSelf( float flWeight )
{
	if (m_vecHeadTargets.Count() > 8)
		return;

	StudioLookTarget tmp;

	tmp.m_flWeight = flWeight;
	tmp.m_vecPosition = Vector(0,0,0);
	tmp.m_bSelf = true;

	m_vecHeadTargets.AddToTail( tmp );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output :
//-----------------------------------------------------------------------------
void StudioModel::SetModelYaw( float flYaw )
{
	m_flModelYaw = flYaw;
}

float StudioModel::GetModelYaw( void ) const 
{
	return m_flModelYaw;
}

void StudioModel::SetBodyYaw( float flYaw )
{
	m_flBodyYaw = flYaw;
}

float StudioModel::GetBodyYaw( void ) const 
{
	return m_flBodyYaw;
}

void StudioModel::SetSpineYaw( float flYaw )
{
	m_flSpineYaw = flYaw;
}

float StudioModel::GetSpineYaw( void ) const 
{
	return m_flSpineYaw;
}


int	StudioModel::GetNumIncludeModels() const
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return 0;

	const studiohdr_t *pRenderHdr = pStudioHdr->GetRenderHdr();
	if ( !pRenderHdr )
		return 0;

	return pRenderHdr->numincludemodels;
}

const char *StudioModel::GetIncludeModelName( int index ) const
{
	CStudioHdr *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return "";

	const studiohdr_t *pRenderHdr = pStudioHdr->GetRenderHdr();
	if ( !pRenderHdr )
		return "";

	if ( index < 0 || index > pRenderHdr->numincludemodels )
		return "";

	mstudiomodelgroup_t *pIncludeModel = pRenderHdr->pModelGroup( index );
	if ( !pIncludeModel )
		return "";

	return pIncludeModel->pszName();
}
