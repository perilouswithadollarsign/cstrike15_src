// ClientInferno.cpp
// Render client-side Inferno effects
// Author: Michael Booth, February 2005
// Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved

#include "cbase.h"
#include "igamesystem.h"
#include "hud_macros.h"
#include "view.h"
#include "enginesprite.h"
#include "precache_register.h"
#include "iefx.h"
#include "dlight.h"
#include "tier0/vprof.h"
#include "debugoverlay_shared.h"
#include "basecsgrenade_projectile.h"
#include "clientinferno.h"


// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


PRECACHE_REGISTER_BEGIN( GLOBAL, InfernoMaterials )
	PRECACHE( MATERIAL, "sprites/white" )
PRECACHE_REGISTER_END()

ConVar InfernoDlightSpacing( "inferno_dlight_spacing", "200", FCVAR_CHEAT, "Inferno dlights are at least this far apart" );
ConVar InfernoDlights( "inferno_dlights", "30", 0, "Min FPS at which molotov dlights will be created" );
//ConVar InfernoParticles( "inferno_particles", "molotov_groundfire", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar InfernoFire( "inferno_fire", "2" );
enum FireMaskType
{
	OLD_FIRE_MASK = 1,
	NEW_FIRE_MASK = 2
};

IMPLEMENT_CLIENTCLASS_DT( C_Inferno, DT_Inferno, CInferno )
	RecvPropArray3( RECVINFO_ARRAY( m_fireXDelta ), RecvPropInt( RECVINFO(m_fireXDelta[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_fireYDelta ), RecvPropInt( RECVINFO(m_fireYDelta[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_fireZDelta ), RecvPropInt( RECVINFO(m_fireZDelta[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bFireIsBurning ), RecvPropBool( RECVINFO(m_bFireIsBurning[0] ) ) ),
	//RecvPropArray3( RECVINFO_ARRAY( m_BurnNormal ), RecvPropVector( RECVINFO(m_BurnNormal[0] ) ) ),
	RecvPropInt( RECVINFO( m_fireCount ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------------------------
C_Inferno::C_Inferno()
{
	m_maxFireHalfWidth = 30.0f;
	m_maxFireHeight = 80.0f;
	m_burnParticleEffect = NULL;
}


//-----------------------------------------------------------------------------------------------
C_Inferno::~C_Inferno()
{
	if ( m_burnParticleEffect.IsValid() )
	{
		m_burnParticleEffect->StopEmission();
	}
}


//-----------------------------------------------------------------------------------------------
void C_Inferno::Spawn( void )
{
	BaseClass::Spawn();


	m_fireCount = 0;
	m_lastFireCount = 0;

	m_drawableCount = 0;
	m_burnParticleEffect = NULL;

	m_minBounds = Vector( 0, 0, 0 );
	m_maxBounds = Vector( 0, 0, 0 );

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}


//-----------------------------------------------------------------------------------------------
/**
 * Monitor changes and recompute render bounds
 */
void C_Inferno::ClientThink()
{
	VPROF_BUDGET( "C_Inferno::ClientThink", "Magic" );

	bool bIsAttachedToMovingObject = (GetMoveParent() != NULL) ? true : false;

	if (true || m_lastFireCount != m_fireCount || bIsAttachedToMovingObject )
	{
		SynchronizeDrawables();
		m_lastFireCount = m_fireCount;
	}
	
	bool bDidRecomputeBounds = false;

	// update Drawables
	for( int i=0; i<m_drawableCount; ++i )
	{
		Drawable *draw = &m_drawable[i];

		switch( draw->m_state )
		{
			case STARTING:
			{
				float growRate = draw->m_maxSize/2.0f;
				draw->m_size = growRate * (gpGlobals->realtime - draw->m_stateTimestamp);
				if (draw->m_size > draw->m_maxSize)
				{
					draw->m_size = draw->m_maxSize;
					draw->SetState( BURNING );
				}
				break;
			}

			case GOING_OUT:
			{
				float dieRate = draw->m_maxSize/2.0f;
				draw->m_size = draw->m_maxSize - dieRate * (gpGlobals->realtime - draw->m_stateTimestamp);
				if (draw->m_size <= 0.0f)
				{
					draw->SetState( FIRE_OUT );

					// render bounds changed
					RecomputeBounds();
					bDidRecomputeBounds = true;
					if ( GetRenderHandle() != INVALID_CLIENT_RENDER_HANDLE )
					{
						ClientLeafSystem()->RenderableChanged( GetRenderHandle() );
					}
				}
				break;
			}
		}
	}

	if( bIsAttachedToMovingObject && !bDidRecomputeBounds )
	{
		RecomputeBounds();
	}

	UpdateParticles(); 
}


//--------------------------------------------------------------------------------------------------------
void C_Inferno::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( FStrEq( pszParticleName, GetParticleEffectName() ) )
	{
		m_burnParticleEffect = pNewParticleEffect;
	}
}


//--------------------------------------------------------------------------------------------------------
void C_Inferno::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	if ( m_burnParticleEffect == pParticleEffect )
	{
		m_burnParticleEffect = NULL;
	}
}


//--------------------------------------------------------------------------------------------------------
void C_Inferno::UpdateParticles( void )
{
	if ( m_drawableCount > 0 && (InfernoFire.GetInt() & NEW_FIRE_MASK) != 0 )
	{
		if ( !m_burnParticleEffect.IsValid() )
		{
			MDLCACHE_CRITICAL_SECTION();
			m_burnParticleEffect = ParticleProp()->Create( GetParticleEffectName(), PATTACH_ABSORIGIN_FOLLOW );
			/*
			DevMsg( "inferno @ %f %f %f / %f %f %f\n",
				GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z,
				GetAbsAngles()[PITCH], GetAbsAngles()[YAW], GetAbsAngles()[ROLL] );
			NDebugOverlay::Cross3D( GetAbsOrigin(), 5, 255, 0, 0, false, 30.0f );
			NDebugOverlay::Cross3D( GetAbsOrigin(), 5, 128, 0, 0, true, 30.0f );
			*/
		}
		else
		{
			for( int i=0; i<m_drawableCount && i<MAX_PARTICLE_CONTROL_POINTS; ++i )
			{
				Drawable *draw = &m_drawable[i];
				// TODO: need a way to disable a control point!!!
				if ( draw->m_state >= FIRE_OUT )
				{
					Vector vecCenter = draw->m_pos;
					//VectorLerp( m_minBounds, m_maxBounds, 0.5f, vecCenter );
 					draw->m_pos = vecCenter + Vector( 0, 0, -9999 );
 					draw->m_size = 0;
					// this sucks
					if ( i != 0 )
						m_burnParticleEffect->SetControlPoint( i, vec3_invalid );

					//engine->Con_NPrintf( i + 10, "0 0 0" );

					//NDebugOverlay::Cross3D( draw->m_pos, 5, 0, 0, 255, false, 5.1f );
				}
				else
				{
					//NDebugOverlay::Cross3D( draw->m_pos, 5, 0, 0, 255, false, 0.1f );
					//NDebugOverlay::Line( GetAbsOrigin(), draw->m_pos, 0, 255, 0, true, 0.1f );
					m_burnParticleEffect->SetControlPointEntity( i, NULL );
					m_burnParticleEffect->SetControlPoint( i, draw->m_pos );

					// FIXME - Set orientation to burn normal once we have per particle normals.
					//m_burnParticleEffect->SetControlPointOrientation( i, Orientation );

					if ( i % 2 == 0 )
					{
						//Elight, for perf reasons only for every other fire
						dlight_t *el = effects->CL_AllocElight( draw->m_dlightIndex );
						el->origin = draw->m_pos;
						el->origin[2] += 64;
						el->color.r = 254;
						el->color.g = 100;
						el->color.b = 10;
						el->radius = random->RandomFloat(60, 120);
						el->die = gpGlobals->curtime + random->RandomFloat( 0.01, 0.025 );
						el->color.exponent = 5;
					}
				}
			}

			SetNextClientThink( 0.1f );
			m_burnParticleEffect->SetNeedsBBoxUpdate( true );

			Vector vecCenter = GetRenderOrigin();
			m_burnParticleEffect->SetSortOrigin( vecCenter );

			//NDebugOverlay::Cross3D( vecCenter, 5, 255, 0, 255, false, 0.5f );
			Vector vecMin, vecMax;
			GetRenderBounds( vecMin, vecMax );
			//NDebugOverlay::Box( vecCenter, vecMin, vecMax, 255, 0, 255, 255, 0.5 );
		}
	}
	else
	{
		if ( m_burnParticleEffect.IsValid() )
		{
			m_burnParticleEffect->StopEmission();
//			m_burnParticleEffect->SetRemoveFlag();
		}
	}
}

//-----------------------------------------------------------------------------------------------
void C_Inferno::GetRenderBounds( Vector& mins, Vector& maxs )
{
	if (m_drawableCount)
	{
		mins = m_minBounds - GetRenderOrigin();
		maxs = m_maxBounds - GetRenderOrigin();
	}
	else
	{
		mins = Vector( 0, 0, 0 );
		maxs = Vector( 0, 0, 0 );
	}
}


//-----------------------------------------------------------------------------------------------
/**
 * Returns the bounds as an AABB in worldspace
 */
void C_Inferno::GetRenderBoundsWorldspace( Vector& mins, Vector& maxs )
{
	if (m_drawableCount)
	{
		mins = m_minBounds;
		maxs = m_maxBounds;
	}
	else
	{
		mins = Vector( 0, 0, 0 );
		maxs = Vector( 0, 0, 0 );
	}
}


//-----------------------------------------------------------------------------------------------
void C_Inferno::RecomputeBounds( void )
{
	m_minBounds = GetAbsOrigin() + Vector( 64.9f, 64.9f, 64.9f );
	m_maxBounds = GetAbsOrigin() + Vector( -64.9f, -64.9f, -64.9f );

	for( int i=0; i<m_drawableCount; ++i )
	{
		Drawable *draw = &m_drawable[i];

		if (draw->m_state == FIRE_OUT)
			continue;

		if (draw->m_pos.x - m_maxFireHalfWidth < m_minBounds.x)
			m_minBounds.x = draw->m_pos.x - m_maxFireHalfWidth;

		if (draw->m_pos.x + m_maxFireHalfWidth > m_maxBounds.x)
			m_maxBounds.x = draw->m_pos.x + m_maxFireHalfWidth;

		if (draw->m_pos.y - m_maxFireHalfWidth < m_minBounds.y)
			m_minBounds.y = draw->m_pos.y - m_maxFireHalfWidth;

		if (draw->m_pos.y + m_maxFireHalfWidth > m_maxBounds.y)
			m_maxBounds.y = draw->m_pos.y + m_maxFireHalfWidth;

		if (draw->m_pos.z < m_minBounds.z)
			m_minBounds.z = draw->m_pos.z;

		if (draw->m_pos.z + m_maxFireHeight > m_maxBounds.z)
			m_maxBounds.z = draw->m_pos.z + m_maxFireHeight;
	}
}


//-----------------------------------------------------------------------------------------------
/**
 * Given a position, return the fire there
 */
C_Inferno::Drawable *C_Inferno::GetDrawable( const Vector &pos )
{
	for( int i=0; i<m_drawableCount; ++i )
	{
		const float equalTolerance = 12.0f;
		if (VectorsAreEqual( m_drawable[i].m_pos, pos, equalTolerance ))
		{
			m_drawable[ i ].m_pos = pos;
			return &m_drawable[i];
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------------------------
/**
 * Compare m_fireX etc to m_drawable and update states
 */
void C_Inferno::SynchronizeDrawables( void )
{
	VPROF_BUDGET( "C_Inferno::SynchronizeDrawables", "Magic" );

	int i;

	// mark all fires that are "burning" as "unknown" - active ones will be reset
	for( i=0; i<m_drawableCount; ++i )
	{
		if (m_drawable[i].m_state == BURNING)
		{
			m_drawable[i].m_state = UNKNOWN;
		}
	}

	Vector vInfernoOrigin = GetAbsOrigin();

	for( i=0; i<m_fireCount; ++i )
	{
		Vector firePos = vInfernoOrigin;
		
		Vector vecFlamePos = Vector( m_fireXDelta[ i ], m_fireYDelta[ i ], m_fireZDelta[ i ] );

		firePos.x += vecFlamePos.x;
		firePos.y += vecFlamePos.y;
		firePos.z += vecFlamePos.z;

		Vector fireNormal = m_BurnNormal[i];
		Drawable *info = GetDrawable( firePos );

		if ( m_bFireIsBurning[i] == false )
		{
			if ( info && info->m_state != FIRE_OUT )
			{
				info->m_state = FIRE_OUT;

				// render bounds changed
				RecomputeBounds();
				if ( GetRenderHandle() != INVALID_CLIENT_RENDER_HANDLE )
				{
					ClientLeafSystem()->RenderableChanged( GetRenderHandle() );
				}
			}

			continue;
		}
		else if ( info )
		{
			// existing fire continues to burn
			if (info->m_state == UNKNOWN)
			{
				info->m_state = BURNING;
			}
		}
		else if (m_drawableCount < MAX_INFERNO_FIRES)
		{
			// new fire
			info = &m_drawable[ m_drawableCount ];

			info->SetState( STARTING );
			info->m_pos = firePos;
			info->m_normal = fireNormal;
			info->m_frame = 0;
			info->m_framerate = random->RandomFloat( 0.04f, 0.06f );
			info->m_mirror = (random->RandomInt( 0, 100 ) < 50);
			info->m_size = 0.0f;
			info->m_maxSize = random->RandomFloat( 70.0f, 90.0f );

			bool closeDlight = false;
			for ( int i=0; i<m_drawableCount; ++i )
			{
				if ( m_drawable[i].m_state != FIRE_OUT )
				{
					if ( m_drawable[i].m_dlightIndex > 0 )
					{
						if ( m_drawable[i].m_pos.DistToSqr( firePos ) < InfernoDlightSpacing.GetFloat() * InfernoDlightSpacing.GetFloat() )
						{
							closeDlight = true;
							break;
						}
					}
				}
			}

			if ( closeDlight )
			{
				info->m_dlightIndex = 0;
			}
			else
			{
				info->m_dlightIndex = LIGHT_INDEX_TE_DYNAMIC + index + m_drawableCount;
			}

			// render bounds changed
			RecomputeBounds();
			if ( GetRenderHandle() != INVALID_CLIENT_RENDER_HANDLE )
			{
				ClientLeafSystem()->RenderableChanged( GetRenderHandle() );
			}

			++m_drawableCount;
		}
	}

	// any fires still in the UNKNOWN state are now GOING_OUT
	for( i=0; i<m_drawableCount; ++i )
	{
		if (m_drawable[i].m_state == UNKNOWN)
		{
			m_drawable[i].SetState( GOING_OUT );
		}
	}
}


//-----------------------------------------------------------------------------------------------
int C_Inferno::DrawModel( int flags, const RenderableInstance_t &instance )
{
	VPROF_BUDGET( "C_Inferno::DrawModel", "DeadRun" );

	IMaterial		*material;
	CEngineSprite	*sprite;
	const model_t	*model;

	if ( (InfernoFire.GetInt() & OLD_FIRE_MASK) == 0 )
		return 0;

	model = modelinfo->GetModel( modelinfo->GetModelIndex( "sprites/fire1.vmt" ) );
	if (model == NULL)
		return 0;

	sprite = (CEngineSprite *)modelinfo->GetModelExtraData( model );
	if (sprite == NULL)
		return 0;

	material = sprite->GetMaterial( kRenderTransAdd );
	if (material == NULL)
		return 0;

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( material );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();
	if ( pMesh )
	{
		// draw the actual flames
		for( int i=0; i<m_drawableCount; ++i )
		{
			int frame = (int)(gpGlobals->realtime/m_drawable[i].m_framerate) % sprite->GetNumFrames();
			sprite->SetFrame( kRenderTransAdd, frame );

			DrawFire( &m_drawable[i], pMesh );
		}
	}

	return 0;
}


//-----------------------------------------------------------------------------------------------
/**
 * Render an individual fire sprite
 */
void C_Inferno::DrawFire( C_Inferno::Drawable *fire, IMesh *mesh )
{
	const float halfWidth = fire->m_size/3.0f;
	//unsigned char color[4] = { 255,255,255,255 };
	unsigned char color[4] = { 150,150,150,255 };

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( mesh, MATERIAL_QUADS, 1 );

	const Vector &right = (fire->m_mirror) ? -CurrentViewRight() : CurrentViewRight();
	Vector up( 0.0f, 0.0f, 1.0f );
	Vector top = fire->m_pos + up * fire->m_size;
	const Vector &bottom = fire->m_pos;

	Vector pos = top + right * halfWidth;

	meshBuilder.Position3fv( pos.Base() );
	meshBuilder.Color4ubv( color );
	meshBuilder.TexCoord2f( 0, 1, 0 );
	meshBuilder.AdvanceVertex();

	pos = bottom + right * halfWidth;

	meshBuilder.Position3fv( pos.Base() );
	meshBuilder.Color4ubv( color );
	meshBuilder.TexCoord2f( 0, 1, 1 );
	meshBuilder.AdvanceVertex();

	pos = bottom - right * halfWidth;

	meshBuilder.Position3fv( pos.Base() );
	meshBuilder.Color4ubv( color );
	meshBuilder.TexCoord2f( 0, 0, 1 );
	meshBuilder.AdvanceVertex();

	pos = top - right * halfWidth;

	meshBuilder.Position3fv( pos.Base() );
	meshBuilder.Color4ubv( color );
	meshBuilder.TexCoord2f( 0, 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	mesh->Draw();

	if ( fire->m_dlightIndex > 0 && InfernoDlights.GetFloat() >= 1 )
	{
		static float lastRealTime = -1.0f;
		float realFrameTime = gpGlobals->realtime - lastRealTime;
		if ( realFrameTime > 2 )
		{
			realFrameTime = -1.0f;
		}
		if ( realFrameTime > 0 )
		{
			static float AverageFPS = -1;
			static int high = -1;
			static int low = -1;
			int nFps = -1;

			const float NewWeight  = 0.1f;
			float NewFrame = 1.0f / realFrameTime;

			if ( AverageFPS < 0.0f )
			{

				AverageFPS = NewFrame;
				high = (int)AverageFPS;
				low = (int)AverageFPS;
			} 
			else
			{				
				AverageFPS *= ( 1.0f - NewWeight ) ;
				AverageFPS += ( ( NewFrame ) * NewWeight );
			}

			int NewFrameInt = (int)NewFrame;
			if( NewFrameInt < low ) low = NewFrameInt;
			if( NewFrameInt > high ) high = NewFrameInt;	

			nFps = static_cast<int>( AverageFPS );
			if ( nFps < InfernoDlights.GetFloat() )
			{
				fire->m_dlightIndex = 0;
				lastRealTime = gpGlobals->realtime;
				return;
			}
		}
		lastRealTime = gpGlobals->realtime;

		// These are the dlight params from the Ep1 fire glows, with a slightly larger flicker
		// (radius delta is larger, starting from 250 instead of 400).
		float scale = fire->m_size / fire->m_maxSize * 1.5f;

		dlight_t *el = effects->CL_AllocElight( fire->m_dlightIndex );
		el->origin = bottom;
		el->origin[2] += 16.0f * scale;
		el->color.r = 254;
		el->color.g = 100;
		el->color.b = 10;
		el->radius = random->RandomFloat(50, 131) * scale;
		el->die = gpGlobals->curtime + 0.1f;
		el->color.exponent = 5;


		/*
		dlight_t *dl = effects->CL_AllocDlight ( fire->m_dlightIndex );
		dl->origin = bottom;
		dl->origin[2] += 16.0f * scale;
		dl->color.r = 254;
		dl->color.g = 174;
		dl->color.b = 10;
		dl->radius = random->RandomFloat(350,431) * scale;
		dl->die = gpGlobals->curtime + 0.1f;
		*/
	}
}


IMPLEMENT_CLIENTCLASS_DT( C_FireCrackerBlast, DT_FireCrackerBlast, CFireCrackerBlast )
END_RECV_TABLE()
