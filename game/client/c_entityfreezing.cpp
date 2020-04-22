//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"

#include "c_entityfreezing.h"
#include "studio.h"
#include "bone_setup.h"
#include "c_surfacerender.h"
#include "engine/ivdebugoverlay.h"
#include "dt_utlvector_recv.h"
#include "debugoverlay_shared.h"
#include "animation.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar cl_blobulator_freezing_max_metaball_radius( "cl_blobulator_freezing_max_metaball_radius", 
												#ifdef INFESTED_DLL
												  "25.0", // Don't need as much precision in Alien swarm because everything is zoomed out
												#else
												  "12.0", 
												#endif
												  FCVAR_NONE, "Setting this can create more complex surfaces on large hitboxes at the cost of performance.", true, 12.0f, true, 100.0f );


//PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectFreezing )
//	PRECACHE( MATERIAL,"effects/tesla_glow_noz" )
//	PRECACHE( MATERIAL,"effects/spark" )
//	PRECACHE( MATERIAL,"effects/combinemuzzle2" )
//PRECACHE_REGISTER_END()

//-----------------------------------------------------------------------------
// Networking
//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT( C_EntityFreezing, DT_EntityFreezing, CEntityFreezing )
	RecvPropVector( RECVINFO(m_vFreezingOrigin) ),
	RecvPropArray3( RECVINFO_ARRAY(m_flFrozenPerHitbox), RecvPropFloat( RECVINFO( m_flFrozenPerHitbox[0] ) ) ),
	RecvPropFloat( RECVINFO(m_flFrozen) ),
	RecvPropBool( RECVINFO(m_bFinishFreezing) ),
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityFreezing::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if ( GetMoveParent() )
	{
		GetMoveParent()->GetRenderBounds( theMins, theMaxs );
	}
	else
	{
		theMins = GetAbsOrigin();
		theMaxs = theMaxs;
	}
}


//-----------------------------------------------------------------------------
// Yes we bloody are
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t C_EntityFreezing::ComputeTranslucencyType( )
{
	return RENDERABLE_IS_TRANSLUCENT;
}


//-----------------------------------------------------------------------------
// On data changed
//-----------------------------------------------------------------------------
void C_EntityFreezing::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );
	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_EntityFreezing::ClientThink( void )
{
#ifdef _PS3
	__nop();
#elif defined(LINUX)
#elif defined( __clang__ )
	asm("nop");
#elif defined( _WIN32 ) && !defined( WIN64 )
	__asm nop;
#endif
	//C_BaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : NULL;
	//if (!pAnimating)
	//	return;

	//color32 color = pAnimating->GetRenderColor();

	//color.r = color.g = ( 1.0f - m_flFrozen ) * 255.0f;

	//// Setup the entity fade
	//pAnimating->SetRenderMode( kRenderTransColor );
	//pAnimating->SetRenderColor( color.r, color.g, color.b, color.a );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flags - 
// Output : int
//-----------------------------------------------------------------------------
int C_EntityFreezing::DrawModel( int flags, const RenderableInstance_t &instance )
{
#ifdef USE_BLOBULATOR

	// See if we should draw
	if ( m_bReadyToDraw == false )
		return 0;

	// The parent needs to be a base animating
	C_BaseAnimating *pAnimating = GetMoveParent() ? GetMoveParent()->GetBaseAnimating() : NULL;
	if ( pAnimating == NULL )
		return 0;

	// Make sure we have hitboxes
	matrix3x4_t	*hitboxbones[MAXSTUDIOBONES];
	if ( pAnimating->HitboxToWorldTransforms( hitboxbones ) == false )
		return 0;

	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( pAnimating->GetModel() );
	if ( pStudioHdr == NULL )
		return 0;

	int nEffectsHitboxSet = FindHitboxSetByName( pAnimating->GetModelPtr(), "effects" );
	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( nEffectsHitboxSet != -1 ? nEffectsHitboxSet : pAnimating->GetHitboxSet() );
	if ( !set )
		return 0;

	// FIXME: No idea how many particles we'll need, so let's start with 1000
	g_SurfaceRenderParticles.SetCount( 1000 );

	int iNumParticles = 0;

	for ( int i = 0; i < set->numhitboxes; ++i )
	{
		mstudiobbox_t *pBox = set->pHitbox(i);
		matrix3x4_t matBone = *hitboxbones[ pBox->bone ];

		Vector vecHorizontal = pBox->bbmax - pBox->bbmin;

		// Get the particle radius
		float flShortestAxis = MIN( MIN( vecHorizontal.x, vecHorizontal.y ), vecHorizontal.z );
		float flDiameter = clamp( flShortestAxis, 1.0f, cl_blobulator_freezing_max_metaball_radius.GetFloat() );
		float flRadius = flDiameter * 0.5f;
		float flVarience = flRadius * 0.25f;
		float flRadiusIsoSurface = flRadius / 12.0f;

		// Get the hitbox data
		EntityFreezingHitboxBlobData_t *pHitboxBlobData = NULL;

		if ( m_HitboxBlobData.Count() <= i )
		{
			// We don't have data for this hitbox yet, so build need relative point positions that are along its faces
			int nNewHitboxBlobData = m_HitboxBlobData.AddToTail();
			pHitboxBlobData = &(m_HitboxBlobData[ nNewHitboxBlobData ]);

			// Start in the min corner
			Vector vecStartPoint = pBox->bbmin + ReplicateToVector( flRadius * 0.5f );
			Vector vecEndPoint = pBox->bbmax - ReplicateToVector( flRadius * 0.5f );
			Vector vecPoint;

			bool bEdgeX = true;
			vecPoint.x = vecStartPoint.x;

			// Loop across each axis
			while ( vecPoint.x <= vecEndPoint.x )
			{
				bool bEdgeY = true;
				vecPoint.y = vecStartPoint.y;

				while ( vecPoint.y <= vecEndPoint.y )
				{
					bool bEdgeZ = true;
					vecPoint.z = vecStartPoint.z;

					while ( vecPoint.z <= vecEndPoint.z )
					{
						// Only add particles not in the middle of the box
						if ( bEdgeX || bEdgeY || bEdgeZ )
						{
							int nNewPoint = pHitboxBlobData->m_vPoints.AddToTail();
							pHitboxBlobData->m_vPoints[ nNewPoint ] = vecPoint + RandomVector( -flVarience, flVarience );
						}

						// Make sure the final particles don't stick out past the edge
						bEdgeZ = ( vecPoint.z < vecEndPoint.z && ( ( !bEdgeX && !bEdgeY ) || vecPoint.z + flRadius >= vecEndPoint.z ) );
						if ( bEdgeZ )
						{
							vecPoint.z = vecEndPoint.z;
						}
						else
						{
							vecPoint.z += flRadius;
						}
					}

					// Make sure the final particles don't stick out past the edge
					bEdgeY = ( vecPoint.y < vecEndPoint.y && vecPoint.y + flRadius >= vecEndPoint.y );
					if ( bEdgeY )
					{
						vecPoint.y = vecEndPoint.y;
					}
					else
					{
						vecPoint.y += flRadius;
					}
				}

				// Make sure the final particles don't stick out past the edge
				bEdgeX = ( vecPoint.x < vecEndPoint.x && vecPoint.x + flRadius >= vecEndPoint.x );
				if ( bEdgeX )
				{
					vecPoint.x = vecEndPoint.x;
				}
				else
				{
					vecPoint.x += flRadius;
				}
			}
		}
		else
		{
			pHitboxBlobData = &(m_HitboxBlobData[ i ]);
		}

		// Decide which of the hitbox points to draw based on how frozen the hitbox is and transform them into worldspace
		for ( int nPoint = 0; nPoint < pHitboxBlobData->m_vPoints.Count() && iNumParticles < 1000; ++nPoint )
		{
			// Fast out if the min Z surrounding this hitbox is above the cut off height
			Vector vecBoxAbsMins, vecBoxAbsMaxs;
			TransformAABB( matBone, pBox->bbmin, pBox->bbmax, vecBoxAbsMins, vecBoxAbsMaxs );

			if ( m_bFinishFreezing )
			{
				m_flFrozenPerHitbox[ i ] = MIN( 1.0f, m_flFrozenPerHitbox[ i ] + gpGlobals->frametime * 0.75f );
			}

			if ( m_flFrozenPerHitbox[ i ] <= 0.0f )
			{
				// No particles will be below the freezing line, skip this hitbox
				continue;
			}

			float fCutOffHeight = vecBoxAbsMins.z + ( vecBoxAbsMaxs.z - vecBoxAbsMins.z ) * m_flFrozenPerHitbox[ i ];

			// Get the point in worldspace
			Vector vecTransformedPoint;
			VectorTransform( pHitboxBlobData->m_vPoints[ nPoint ], matBone, vecTransformedPoint );

			// Only add particles below this height and if it's not in the middle of the box
			if ( vecTransformedPoint.z < fCutOffHeight )
			{
				ImpParticleWithOneInterpolant* imp_particle = &(g_SurfaceRenderParticles[ iNumParticles ]);
				imp_particle->center = vecTransformedPoint;
				imp_particle->setFieldScale( flRadiusIsoSurface );
				imp_particle->interpolants1.set( 1.0f, 1.0f, 1.0f );
				imp_particle->interpolants1[ 3 ] = 0.0f;
				++iNumParticles;
			}
		}
	}

	g_SurfaceRenderParticles.SetCountNonDestructively( iNumParticles );

	// Set up lighting
	modelrender->SetupLighting( GetRenderOrigin() );

	Surface_Draw( GetClientRenderable(), GetRenderOrigin(), materials->FindMaterial( "models/weapons/w_icegun/ice_surface", TEXTURE_GROUP_OTHER, true ), 6.5f );
#endif

	return 1;
}
