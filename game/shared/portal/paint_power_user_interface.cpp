//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the interface class for all paint power users.
//
//=============================================================================//

#include "cbase.h"

#include "paint_power_user_interface.h"
#include "paintable_entity.h"

#ifdef GAME_DLL
#include "world.h"
#else
#include "c_world.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sv_enable_paint_power_user_debug("sv_enable_paint_power_user_debug", "0", FCVAR_REPLICATED | FCVAR_CHEAT,"Enable debug spew for paint power users.");
extern ConVar sv_debug_draw_contacts;

IPaintPowerUser::~IPaintPowerUser()
{}


void MapSurfaceToPower( PaintPowerInfo_t& info )
{
	IHandleEntity* pEntityHandle = info.m_HandleToOther.Get();
	CBaseEntity* pEnt = pEntityHandle != NULL ? EntityFromEntityHandle( pEntityHandle ) : NULL;

	// If the power hasn't been set to anything elsewhere, and the entity is valid
	if ( info.m_PaintPowerType == NO_POWER && pEnt != NULL )
	{
		// Treat portalsimulator_collisionentity as the world
		if( FClassnameIs( pEnt, "portalsimulator_collisionentity" ) )
		{
#ifdef GAME_DLL
			pEnt = GetWorldEntity();
#else
			pEnt = GetClientWorldEntity();
#endif
		}
				
		// If this is a world entity
		if( pEnt->IsBSPModel() )
		{
			Vector vStart = info.m_ContactPoint + info.m_SurfaceNormal;
			Vector vEnd = vStart - 10.f * info.m_SurfaceNormal;
			Ray_t ray;
			ray.Init( vStart, vEnd );
			trace_t tr;
			UTIL_TraceRay( ray, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

			// Trace into the paint map to find the power if the surface is paintable
			if ( UTIL_IsPaintableSurface( tr.surface ) )
			{
				Vector contactPoint = info.m_ContactPoint + 0.1f * info.m_SurfaceNormal;
				info.m_PaintPowerType = UTIL_Paint_TracePower( pEnt, contactPoint, info.m_SurfaceNormal );
			}
			else
			{
				info.m_PaintPowerType = NO_POWER;
			}
		}
		else if( !pEnt->IsPlayer() )
		{
			// The dynamic_cast here shouldn't be any more of a performance concern than the
			// dynamic_cast that would be necessary when this PaintPowerInfo_t was added in the first place.
			const IPaintableEntity* pPaintableEnt = dynamic_cast< const IPaintableEntity* >( pEnt );
			if( pPaintableEnt )
			{
				info.m_PaintPowerType = pPaintableEnt->GetPaintPowerAtPoint( info.m_ContactPoint );
			}
			else
			{
				//Use the render color of the owner if this entity is a bone follower
				if( FClassnameIs( pEnt, "phys_bone_follower" ) )
				{
					CBaseEntity *pBoneOwner = pEnt->GetOwnerEntity();

					if( pBoneOwner )
					{
						info.m_PaintPowerType = MapColorToPower( pBoneOwner->GetRenderColor() );
					}
				}
				else //Use the render color of this entity
				{
					info.m_PaintPowerType = MapColorToPower( pEnt->GetRenderColor() );
				}

				if( sv_enable_paint_power_user_debug.GetBool() )
				{
					Warning( "Non-world, non-player entity, %s, doesn't implement IPaintableEntity. See the global version of MapSurfacesToPowers() in paint_power_user_interface.cpp.\n", pEnt->GetClassname() );
				}
			}
		}
	}

	if( sv_debug_draw_contacts.GetInt() == 2 )
	{
		Color color = MapPowerToVisualColor( info.m_PaintPowerType );
		NDebugOverlay::Sphere( info.m_ContactPoint, 5.0f, color.r(), color.g(), color.b(), true, 0 );
		NDebugOverlay::Line( info.m_ContactPoint, info.m_ContactPoint + 20.0f * info.m_SurfaceNormal, color.r(), color.g(), color.b(), true, 0 );
	}
}

