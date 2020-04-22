//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#include "c_surfacerender.h"

#include "view.h"
#include "view_shared.h"
#include "iviewrender.h"
#include "engine/ivdebugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifdef USE_BLOBULATOR

ConVar r_surface_draw_isosurface( "r_surface_draw_isosurface", "1", 0, "Draws the surface as an isosurface" );

ConVar r_surface_rotate( "r_surface_rotate", "1", FCVAR_NONE, "Whether to rotate for transparency" );
ConVar r_surface_rotate_by90( "r_surface_rotate_by90", "1", FCVAR_NONE, "Whether to only rotate in 90 degree increments" );

ConVar r_surface_blr_scale( "r_surface_blr_scale", "1.0", FCVAR_NONE, "Scale all surface rendering parameters." );
ConVar r_surface_blr_cubewidth( "r_surface_blr_cubewidth", "0.8", FCVAR_NONE, "Set cubewidth (coarseness of the mesh)" );
ConVar r_surface_blr_render_radius( "r_surface_blr_render_radius", "1.3", FCVAR_NONE, "Set render radius (how far from particle center surface will be)" );
ConVar r_surface_blr_cutoff_radius( "r_surface_blr_cutoff_radius", "3.3", FCVAR_NONE, "Set cutoff radius (how far field extends from each particle)" );

ConVar r_surface_calc_uv_and_tan( "r_surface_calc_uv_and_tan", "1", FCVAR_ARCHIVE, "Calculate UVs and Tangents" );
ConVar r_surface_calc_tan_only( "r_surface_calc_tan_only", "0", FCVAR_ARCHIVE, "Calculate Only Tangents" );
ConVar r_surface_calc_color( "r_surface_calc_color", "0", FCVAR_ARCHIVE, "Just interpolate colors" );
ConVar r_surface_calc_hifreq_color( "r_surface_calc_hifreq_color", "0", FCVAR_ARCHIVE, "Experimental hi-freq colors" );
ConVar r_surface_calc_tile_color( "r_surface_calc_tile_color", "0", FCVAR_ARCHIVE, "Shows color of the tile" );

ConVar r_surface_debug_use_tiler("r_surface_debug_use_tiler", "1", FCVAR_NONE, "Use the tiler");
ConVar r_surface_debug_max_tiles( "r_surface_debug_max_tiles", "-1", FCVAR_NONE, "The maximum number of tiles to draw" );
ConVar r_surface_debug_max_slices( "r_surface_debug_max_slices", "-1", FCVAR_NONE, "The maximum number of slices to draw" );
ConVar r_surface_debug_tile("r_surface_debug_tile", "1", FCVAR_NONE, "If tiler is enabled, whether we draw all tiles or just the central one.");
ConVar r_surface_debug_draw_margin("r_surface_debug_draw_margin", "0", FCVAR_NONE, "If tiler is disabled, whether to draw the margin.");
ConVar r_surface_debug_draw_tile_boundaries("r_surface_debug_draw_tile_boundaries", "0", FCVAR_NONE, "Whether to draw outlines of all tiles.");

ConVar	r_surface_wireframe( "r_surface_wireframe", "0", FCVAR_NONE, "Draw wireframe" );
ConVar	r_surface_material("r_surface_material", "-1", FCVAR_NONE, "Choose a material from 0 to N");
ConVar	r_surface_shader("r_surface_shader", "", FCVAR_NONE, "Choose a shader");


CUtlVector< ImpParticleWithFourInterpolants, CUtlMemoryAligned< ImpParticleWithFourInterpolants, 16 > > g_SurfaceRenderParticles;	// This must be aligned due to SSE use
const QAngle g_SurfaceRenderAnglesAngles( 0.0f, 0.0f, 0.0f );	// our local coordinate system is always aligned to world space


//-----------------------------------------------------------------------------
// Purpose: Draw a Sphere
//-----------------------------------------------------------------------------

float g_FastSpherePosData[51][8] = {
	{  0.0000,  0.0000,  1.0000,  0.0000,  0.0000,  0.0000,  0.0000,  1.0000 },
	{ -0.0000,  0.5000,  0.8660,  0.2500,  0.1667, -0.0000,  0.5000,  0.8660 },
	{  0.5000,  0.0000,  0.8660,  0.0000,  0.1667,  0.5000,  0.0000,  0.8660 },
	{  0.0000,  0.0000,  1.0000,  0.5000,  0.0000,  0.0000,  0.0000,  1.0000 },
	{ -0.5000, -0.0000,  0.8660,  0.5000,  0.1667, -0.5000, -0.0000,  0.8660 },
	{  0.0000,  0.0000, -1.0000,  0.0000,  1.0000,  0.0000,  0.0000, -1.0000 },
	{  0.5000,  0.0000, -0.8660,  0.0000,  0.8333,  0.5000,  0.0000, -0.8660 },
	{ -0.0000,  0.5000, -0.8660,  0.2500,  0.8333, -0.0000,  0.5000, -0.8660 },
	{  0.0000,  0.0000, -1.0000,  0.5000,  1.0000,  0.0000,  0.0000, -1.0000 },
	{ -0.5000, -0.0000, -0.8660,  0.5000,  0.8333, -0.5000, -0.0000, -0.8660 },
	{  0.0000, -0.5000,  0.8660,  0.7500,  0.1667,  0.0000, -0.5000,  0.8660 },
	{  0.0000,  0.0000,  1.0000,  1.0000,  0.0000,  0.0000,  0.0000,  1.0000 },
	{  0.5000,  0.0000,  0.8660,  1.0000,  0.1667,  0.5000,  0.0000,  0.8660 },
	{  0.0000, -0.5000, -0.8660,  0.7500,  0.8333,  0.0000, -0.5000, -0.8660 },
	{  0.0000,  0.0000, -1.0000,  1.0000,  1.0000,  0.0000,  0.0000, -1.0000 },
	{  0.5000,  0.0000, -0.8660,  1.0000,  0.8333,  0.5000,  0.0000, -0.8660 },
	{  0.6124,  0.6124,  0.5000,  0.1250,  0.3333,  0.6124,  0.6124,  0.5000 },
	{  0.8660,  0.0000,  0.5000,  0.0000,  0.3333,  0.8660,  0.0000,  0.5000 },
	{ -0.0000,  0.8660,  0.5000,  0.2500,  0.3333, -0.0000,  0.8660,  0.5000 },
	{  0.8660,  0.0000, -0.5000,  0.0000,  0.6667,  0.8660,  0.0000, -0.5000 },
	{  0.6124,  0.6124, -0.5000,  0.1250,  0.6667,  0.6124,  0.6124, -0.5000 },
	{ -0.0000,  0.8660, -0.5000,  0.2500,  0.6667, -0.0000,  0.8660, -0.5000 },
	{ -0.6124,  0.6124,  0.5000,  0.3750,  0.3333, -0.6124,  0.6124,  0.5000 },
	{ -0.8660, -0.0000,  0.5000,  0.5000,  0.3333, -0.8660, -0.0000,  0.5000 },
	{ -0.6124,  0.6124, -0.5000,  0.3750,  0.6667, -0.6124,  0.6124, -0.5000 },
	{ -0.8660, -0.0000, -0.5000,  0.5000,  0.6667, -0.8660, -0.0000, -0.5000 },
	{ -0.6124, -0.6124,  0.5000,  0.6250,  0.3333, -0.6124, -0.6124,  0.5000 },
	{  0.0000, -0.8660,  0.5000,  0.7500,  0.3333,  0.0000, -0.8660,  0.5000 },
	{ -0.6124, -0.6124, -0.5000,  0.6250,  0.6667, -0.6124, -0.6124, -0.5000 },
	{  0.0000, -0.8660, -0.5000,  0.7500,  0.6667,  0.0000, -0.8660, -0.5000 },
	{  0.6124, -0.6124,  0.5000,  0.8750,  0.3333,  0.6124, -0.6124,  0.5000 },
	{  0.8660,  0.0000,  0.5000,  1.0000,  0.3333,  0.8660,  0.0000,  0.5000 },
	{  0.6124, -0.6124, -0.5000,  0.8750,  0.6667,  0.6124, -0.6124, -0.5000 },
	{  0.8660,  0.0000, -0.5000,  1.0000,  0.6667,  0.8660,  0.0000, -0.5000 },
	{  0.9239,  0.3827,  0.0000,  0.0625,  0.5000,  0.9239,  0.3827,  0.0000 },
	{  1.0000,  0.0000,  0.0000,  0.0000,  0.5000,  1.0000,  0.0000,  0.0000 },
	{  0.7071,  0.7071,  0.0000,  0.1250,  0.5000,  0.7071,  0.7071,  0.0000 },
	{  0.3827,  0.9239,  0.0000,  0.1875,  0.5000,  0.3827,  0.9239,  0.0000 },
	{ -0.0000,  1.0000,  0.0000,  0.2500,  0.5000, -0.0000,  1.0000,  0.0000 },
	{ -0.3827,  0.9239,  0.0000,  0.3125,  0.5000, -0.3827,  0.9239,  0.0000 },
	{ -0.7071,  0.7071,  0.0000,  0.3750,  0.5000, -0.7071,  0.7071,  0.0000 },
	{ -0.9239,  0.3827,  0.0000,  0.4375,  0.5000, -0.9239,  0.3827,  0.0000 },
	{ -1.0000, -0.0000,  0.0000,  0.5000,  0.5000, -1.0000, -0.0000,  0.0000 },
	{ -0.9239, -0.3827,  0.0000,  0.5625,  0.5000, -0.9239, -0.3827,  0.0000 },
	{ -0.7071, -0.7071,  0.0000,  0.6250,  0.5000, -0.7071, -0.7071,  0.0000 },
	{ -0.3827, -0.9239,  0.0000,  0.6875,  0.5000, -0.3827, -0.9239,  0.0000 },
	{  0.0000, -1.0000,  0.0000,  0.7500,  0.5000,  0.0000, -1.0000,  0.0000 },
	{  0.3827, -0.9239,  0.0000,  0.8125,  0.5000,  0.3827, -0.9239,  0.0000 },
	{  0.7071, -0.7071,  0.0000,  0.8750,  0.5000,  0.7071, -0.7071,  0.0000 },
	{  0.9239, -0.3827,  0.0000,  0.9375,  0.5000,  0.9239, -0.3827,  0.0000 },
	{  1.0000,  0.0000,  0.0000,  1.0000,  0.5000,  1.0000,  0.0000,  0.0000 }
};


int g_FastSphereTriData[84][3] = {
	{ 0, 1, 2 },
	{ 0, 3, 1 },
	{ 3, 4, 1 },
	{ 5, 6, 7 },
	{ 5, 7, 8 },
	{ 8, 7, 9 },
	{ 3, 10, 4 },
	{ 3, 11, 10 },
	{ 11, 12, 10 },
	{ 8, 9, 13 },
	{ 8, 13, 14 },
	{ 14, 13, 15 },
	{ 2, 16, 17 },
	{ 2, 1, 16 },
	{ 1, 18, 16 },
	{ 6, 19, 20 },
	{ 6, 20, 7 },
	{ 7, 20, 21 },
	{ 1, 22, 18 },
	{ 1, 4, 22 },
	{ 4, 23, 22 },
	{ 7, 21, 24 },
	{ 7, 24, 9 },
	{ 9, 24, 25 },
	{ 4, 26, 23 },
	{ 4, 10, 26 },
	{ 10, 27, 26 },
	{ 9, 25, 28 },
	{ 9, 28, 13 },
	{ 13, 28, 29 },
	{ 10, 30, 27 },
	{ 10, 12, 30 },
	{ 12, 31, 30 },
	{ 13, 29, 32 },
	{ 13, 32, 15 },
	{ 15, 32, 33 },
	{ 17, 34, 35 },
	{ 17, 16, 34 },
	{ 16, 36, 34 },
	{ 19, 35, 34 },
	{ 19, 34, 20 },
	{ 20, 34, 36 },
	{ 16, 37, 36 },
	{ 16, 18, 37 },
	{ 18, 38, 37 },
	{ 20, 36, 37 },
	{ 20, 37, 21 },
	{ 21, 37, 38 },
	{ 18, 39, 38 },
	{ 18, 22, 39 },
	{ 22, 40, 39 },
	{ 21, 38, 39 },
	{ 21, 39, 24 },
	{ 24, 39, 40 },
	{ 22, 41, 40 },
	{ 22, 23, 41 },
	{ 23, 42, 41 },
	{ 24, 40, 41 },
	{ 24, 41, 25 },
	{ 25, 41, 42 },
	{ 23, 43, 42 },
	{ 23, 26, 43 },
	{ 26, 44, 43 },
	{ 25, 42, 43 },
	{ 25, 43, 28 },
	{ 28, 43, 44 },
	{ 26, 45, 44 },
	{ 26, 27, 45 },
	{ 27, 46, 45 },
	{ 28, 44, 45 },
	{ 28, 45, 29 },
	{ 29, 45, 46 },
	{ 27, 47, 46 },
	{ 27, 30, 47 },
	{ 30, 48, 47 },
	{ 29, 46, 47 },
	{ 29, 47, 32 },
	{ 32, 47, 48 },
	{ 30, 49, 48 },
	{ 30, 31, 49 },
	{ 31, 50, 49 },
	{ 32, 48, 49 },
	{ 32, 49, 33 },
	{ 33, 49, 50 }
};


void Surface_DrawFastSphere( CMeshBuilder &meshBuilder, const Vector &center, float radius, float r, float g, float b )
{
	int offset = meshBuilder.GetCurrentVertex();

	Vector pos;
	for (int i = 0; i < 51; i++)
	{
		pos.x = g_FastSpherePosData[i][0] + center.x + g_FastSpherePosData[i][5] * radius;
		pos.y = g_FastSpherePosData[i][1] + center.y + g_FastSpherePosData[i][6] * radius;
		pos.z = g_FastSpherePosData[i][2] + center.z + g_FastSpherePosData[i][7] * radius;

		meshBuilder.Position3fv( pos.Base() );
		meshBuilder.Normal3fv( &g_FastSpherePosData[i][5] );
		meshBuilder.TexCoord2fv( 0, &g_FastSpherePosData[i][3] );
		meshBuilder.Color3f( r, g, b );
		meshBuilder.AdvanceVertex();
	}

	for (int i = 0; i < 84; i++)
	{
		meshBuilder.FastIndex( g_FastSphereTriData[i][0] + offset );
		meshBuilder.FastIndex( g_FastSphereTriData[i][1] + offset );
		meshBuilder.FastIndex( g_FastSphereTriData[i][2] + offset );
	}
}

struct Surface_DrawSpheres_sortParticles_t
{
	int no;
	float dist;

	class C
	{
	public:
		static bool gt(Surface_DrawSpheres_sortParticles_t a, Surface_DrawSpheres_sortParticles_t b)
		{
			return a.dist < b.dist;
		}
	};
};

void Surface_DrawSpheres( IMaterial *pMaterial, float flRadius )
{
	Point3D eye = view->GetViewSetup()->origin;

	SmartArray<Surface_DrawSpheres_sortParticles_t> sort_particles;
	sort_particles.ensureCapacity(g_SurfaceRenderParticles.Count());
	sort_particles.size = g_SurfaceRenderParticles.Count();
	for(int i = 0; i < g_SurfaceRenderParticles.Count(); i++)
	{
		sort_particles[i].no = i;
		sort_particles[i].dist = g_SurfaceRenderParticles[i].center.length(eye);
	}
	sort_particles.sort<Surface_DrawSpheres_sortParticles_t::C>();

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->Bind( pMaterial );

	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	int vertMax = MIN( 24000 / 51, 32768 / (84 * 3) );

	int j = 0;
	// Msg( "point %.2f %.2f %.2f\n", m_vecSurfacePos[0].x, m_vecSurfacePos[0].y, m_vecSurfacePos[0].z );
	while (j < g_SurfaceRenderParticles.Count())
	{
		int total = MIN( g_SurfaceRenderParticles.Count() - j, vertMax );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, total * 51, total * 84 * 3 );

		int i = 0;
		while (i < vertMax && j < g_SurfaceRenderParticles.Count())
		{
			ImpParticleWithOneInterpolant* imp_particle = &(g_SurfaceRenderParticles[sort_particles[j].no]);

			if (imp_particle->scale > 0.01)
			{
				Surface_DrawFastSphere( meshBuilder, imp_particle->center.AsVector(), flRadius * imp_particle->scale, imp_particle->interpolants1[0], imp_particle->interpolants1[1], imp_particle->interpolants1[2] );
				i++;
			}
			j++;
		}

		meshBuilder.End();
		pMesh->Draw();
	}
}


void Surface_SafeLightCubeUpdate( const Vector &vecRenderOrigin, Vector4D *cachedCubeColours )
{
	// We sometimes get an invalid ambient lightcube for the isosurface sample point, this func smooths over that
	// FIXME: this is probably to do with sampling at invalid locations
	// FIXME: we need more complex lighting data for isosurfaces anyway
	//        (per-vertex light sample indices ala skinning, or UVWs into a light sample texture)

	Vector boxColors[6];
	engine->ComputeLightingCube( vecRenderOrigin, false, boxColors );

	// Are we trying to sample outside the world?
	bool invalidOrigin = enginetrace->PointOutsideWorld( vecRenderOrigin );

	for ( int i = 0; i < 6; i++ )
	{
		if( !IsFinite( boxColors[i].x ) || !IsFinite( boxColors[i].y ) || !IsFinite( boxColors[i].z ) )
		{
			DevWarning( "Isosurface lighting cube with infinite values!\n%s",
						invalidOrigin ? "(lighting origin is !OUTSIDE! the world)\n" : "" );
			return;
		}
	}

	for ( int i = 0; i < 6; i++ )
	{
		if( boxColors[i].x < -0.1f || boxColors[i].y < -0.1f || boxColors[i].z < -0.1f )
		{
			DevWarning( "Isosurface lighting cube negative: %f %f %f\n%s", boxColors[i].x, boxColors[i].y, boxColors[i].z,
						invalidOrigin ? "(lighting origin is !OUTSIDE! the world)\n" : "" );
		}

		cachedCubeColours[i].AsVector3D() = Lerp( clamp( 2.0f * gpGlobals->frametime, 0.0f, 1.0f ), cachedCubeColours[i].AsVector3D(), boxColors[i] );
	}

	bool bBadLightCube = false;
	for ( int i = 0; i < 6; i++ )
	{
		if ( !IsFinite( cachedCubeColours[i].x ) || !IsFinite( cachedCubeColours[i].y ) || !IsFinite( cachedCubeColours[i].z ) ||
			 ( cachedCubeColours[i].x < -0.01f ) || ( cachedCubeColours[i].y < -0.01f ) || ( cachedCubeColours[i].z < -0.01f )  )
		{
			DevWarning( "Isosurface lighting cube bad. Resetting.\n%s",
						invalidOrigin ? "(lighting origin is !OUTSIDE! the world)\n" : "" );
			bBadLightCube = true;
			break;
		}
	}

	if ( bBadLightCube )
	{
		memset( cachedCubeColours, 0, sizeof( cachedCubeColours ) );
	}
}


IMaterial* GetDrawMaterial( void )
{
	if ( r_surface_wireframe.GetBool() )
	{
		if( r_surface_material.GetInt() < 0 )
		{
			return materials->FindMaterial("shadertest/wireframe", TEXTURE_GROUP_OTHER);
		}
		else
		{
			return materials->FindMaterial("shadertest/wireframevertexcolornocull", TEXTURE_GROUP_OTHER);
		}
	}
	else if ( strlen( r_surface_shader.GetString() ) > 0 )
	{
		return materials->FindMaterial( r_surface_shader.GetString(), TEXTURE_GROUP_OTHER, true );
	}
	else
	{
		switch ( r_surface_material.GetInt() )
		{
			case 0:
				return materials->FindMaterial( "models/debug/debugwhite", TEXTURE_GROUP_OTHER, true );
			case 1:
				return materials->FindMaterial( "models/debug/debugwhite2", TEXTURE_GROUP_OTHER, true );
			case 2:
				return materials->FindMaterial( "models/debug/debugwhite3", TEXTURE_GROUP_OTHER, true );
			case 3:
				return materials->FindMaterial( "debug/debugvertexcolor", TEXTURE_GROUP_OTHER, true );
			case 4:
				return materials->FindMaterial( "debug/env_cubemap_model", TEXTURE_GROUP_OTHER, true );
			case 5:
				return materials->FindMaterial( "models/blob/env_cubemap_model_translucent_fountain", TEXTURE_GROUP_OTHER, true );
			case 6:
				return materials->FindMaterial( "models/debug/debugmesh", TEXTURE_GROUP_OTHER, true );
			case 7:
				return materials->FindMaterial( "models/debug/debugmesh_transparent", TEXTURE_GROUP_OTHER, true );
			case 8:
				return materials->FindMaterial( "models/ihvtest/tongue_bumped", TEXTURE_GROUP_OTHER, true );
			case 9:
				return materials->FindMaterial( "models/debug/debugbumps", TEXTURE_GROUP_OTHER, true );
			case 10:
				return materials->FindMaterial( "debug/env_cubemap_model_translucent_no_bumps", TEXTURE_GROUP_OTHER, true );
			case 11:
				return materials->FindMaterial( "models/shadertest/predator", TEXTURE_GROUP_OTHER, true );
			default:
				return NULL;
		}
	}
}


void SetUpImpRendererUserDefinedFuncs( void )
{
	RENDERER_CLASS::setCalcSignFunc(calcSign);
	RENDERER_CLASS::setCalcSign2Func(calcSign2);

	if(r_surface_calc_uv_and_tan.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_UV_TAN_CI_SIZE, calcCornerNormalColorUVTan);
		//RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColorUVTan);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNTexCoord4); // HACKHACK: This is blob specific!
	}
	else if(r_surface_calc_tan_only.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_UV_TAN_CI_SIZE, calcCornerNormalColorTanNoUV);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColorTanNoUV);
	}
	else if	(r_surface_calc_color.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_CI_SIZE, calcCornerNormalColor);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColor);
	}
	else if (r_surface_calc_hifreq_color.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_COLOR_CI_SIZE, calcCornerNormalHiFreqColor);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalNColor);
	}
	else if (r_surface_calc_tile_color.GetBool())
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_CI_SIZE, calcCornerNormal);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormalDebugColor);
	}
	else
	{
		RENDERER_CLASS::setCalcCornerFunc(CALC_CORNER_NORMAL_CI_SIZE, calcCornerNormal);
		RENDERER_CLASS::setCalcVertexFunc(calcVertexNormal);
	}
}


void TransformParticles( float angle, const Vector& center, Vector& transformedCenter, Vector& transformedEye, VMatrix& rotationMatrix, VMatrix& invRotationMatrix )
{
	for (int i = 0; i < g_SurfaceRenderParticles.Count(); i++)
	{
		ImpParticleWithFourInterpolants* imp_particle = &(g_SurfaceRenderParticles[i]);

		Vector vParticle = imp_particle->center.AsVector();
		Vector transformedParticle = vParticle-center;

		transformedParticle = rotationMatrix.ApplyRotation(transformedParticle);

		Point3D pParticle = transformedParticle; 

		imp_particle->center = pParticle;

		// HACK ALERT!!!
		// TODO: This code should not be here as this is very specific to blobs.

		// interpolants1[0..2] is the color. interpolants1[3] is the v coordinate
		// imp_particle1->interpolants1[3] = MIN(MAX(1.4f - vec.length()/17.0f, 0.0f), 1.0f);
		// interpolants2[0..2] is the tangent vector.
		// interpolants3[0..2] and interpolants4[0..2] are the normal and
		// binormal which are used to generate a u coordinate
		Point3D pCenter = transformedCenter;
		Point3D vec = (pParticle - pCenter);

		imp_particle->interpolants2 = vec.unit();
		imp_particle->interpolants4.set(0.0f, 0.0f, -1.0f);
		imp_particle->interpolants3 = imp_particle->interpolants2.crossProduct(imp_particle->interpolants4);
		imp_particle->interpolants3.normalize();
		imp_particle->interpolants4 = imp_particle->interpolants2.crossProduct(imp_particle->interpolants3);
		imp_particle->interpolants4.normalize();
	}
}


void Surface_DrawIsoSurface( IClientRenderable *pClientRenderable, const Vector &vecRenderOrigin, IMaterial *pMaterial, float flCubeWidth )
{
	IMaterial *pSpecialMaterial = GetDrawMaterial();

	if( pSpecialMaterial && !IsErrorMaterial( pSpecialMaterial ) )
	{
		pMaterial = pSpecialMaterial;
	}

	//////////////////////////////////////////////////
	// Set up the render context
	/////////////////////////////////////////////////
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->Bind( pMaterial, pClientRenderable );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();


	////////////////////////////////////////////////////
	// Set up rotations and translations and corresponding matrices.
	////////////////////////////////////////////////////
	Vector transformedCenter = vecRenderOrigin;

	pRenderContext->Translate(vecRenderOrigin.x, vecRenderOrigin.y, vecRenderOrigin.z);

	VMatrix rotationMatrix;
	VMatrix invRotationMatrix;
	Vector transformedEye;
	float angle = 0.0f;
	if( r_surface_rotate.GetBool() )
	{
		angle = view->GetViewSetup()->angles.y+180.0f;

		if( r_surface_rotate_by90.GetBool() )
		{
			// This is a test mode which locks the rotation to a multiple of 90.
			angle = float(int((angle + 45.0f) / 90.0f) * 90);
		}

		pRenderContext->Rotate(angle, 0.0f, 0.0f, 1.0f);

		rotationMatrix = SetupMatrixAxisRot(Vector(0.0f, 0.0f, 1.0f), -angle);
		invRotationMatrix = SetupMatrixAxisRot(Vector(0.0f, 0.0f, 1.0f), angle);
		Vector eye = view->GetViewSetup()->origin;
		transformedEye = eye-vecRenderOrigin;
		transformedEye = rotationMatrix.ApplyRotation(transformedEye);
	}
	else
	{
		rotationMatrix.Identity();
		invRotationMatrix.Identity();
		transformedEye.Init();
		angle = 0.0f;
	}

	///////////////////////////////////////////////////////////////
	// Begin Rendering by initializing the sweepRenderer or Tiler
	// (depending on whether we're using the tiler)
	///////////////////////////////////////////////////////////////

	// TODO: These probably shouldn't be static variables (not thread safe)
	RENDERER_CLASS::setCubeWidth( flCubeWidth * r_surface_blr_scale.GetFloat() * r_surface_blr_cubewidth.GetFloat());
	RENDERER_CLASS::setRenderR( flCubeWidth * r_surface_blr_scale.GetFloat() * r_surface_blr_render_radius.GetFloat());
	RENDERER_CLASS::setCutoffR( flCubeWidth * r_surface_blr_scale.GetFloat() * r_surface_blr_cutoff_radius.GetFloat());
	SetUpImpRendererUserDefinedFuncs();

	TransformParticles( angle, vecRenderOrigin, transformedCenter, transformedEye, rotationMatrix, invRotationMatrix );

	///////////////////////////////////////////////////////////////
	// Insert the particles into the tiler/renderer, and do a rendering
	// pass. If we are tiling, we can also visualize the tiles and slices.
	///////////////////////////////////////////////////////////////
	if( r_surface_debug_use_tiler.GetBool() )
	{
		ImpTiler* tiler = ImpTilerFactory::factory->getTiler();
		RENDERER_CLASS* sweepRenderer = tiler->getRenderer();

		tiler->setMaxNoTilesToDraw(r_surface_debug_max_tiles.GetInt());
		sweepRenderer->setMaxNoSlicesToDraw(r_surface_debug_max_slices.GetInt());

		// TODO: Do we always want to center the tiler on 0,0,0?
		// I guess if we are centering the render coordinates, then the answer
		// is a yes.
		tiler->beginFrame(Point3D(0.0f, 0.0f, 0.0f), (void*)&pRenderContext, !(r_surface_debug_draw_margin.GetBool()));

		for (int i = 0; i < g_SurfaceRenderParticles.Count(); i++)
		{
			tiler->insertParticle(&g_SurfaceRenderParticles[i]);
		}

		if(r_surface_debug_tile.GetBool())
		{
			if(r_surface_rotate.GetBool())
			{
				tiler->drawSurfaceSorted(Point3D(transformedEye));
			}
			else
			{
				tiler->drawSurface();
			}

			// This is code used for visualizing the boundaries of every tile that's drawn
			// and maybe in the future, slices within the tiles.
			if( r_surface_debug_draw_tile_boundaries.GetBool() == true )
			{
				for( int i = 0; i < tiler->getNoTiles(); i++ )
				{
					Vector overlayCenter = invRotationMatrix.ApplyRotation(tiler->getTileOffset(i).AsVector());
					Vector mins = tiler->getRenderDim().AsVector() * -0.5f;
					Vector maxs = tiler->getRenderDim().AsVector() * 0.5f;
					debugoverlay->AddBoxOverlay(overlayCenter + vecRenderOrigin, mins, maxs, QAngle( 0, angle, 0 ), 0, 255, 0, 0, 0 );
				}
			}
		}
		else
		{
			tiler->drawTile(0,0,0);

			// This is code used for visualizing the boundaries of the last tile drawn
			// slices within that tile
			if( r_surface_debug_max_tiles.GetInt() > 0 || r_surface_debug_tile.GetBool() == false )
			{
				Vector overlayCenter = invRotationMatrix.ApplyRotation(tiler->getLastTilesOffset().AsVector());
				Vector mins = tiler->getRenderDim().AsVector() * -0.5f;
				Vector maxs = tiler->getRenderDim().AsVector() * 0.5f;
				debugoverlay->AddBoxOverlay(overlayCenter + vecRenderOrigin, mins, maxs, QAngle( 0, angle, 0 ), 0, 255, 0, 0, 0 );

				if( r_surface_debug_max_slices.GetInt() > 0 )
				{
					Vector sliceMins = mins;
					Vector sliceMaxs = maxs;
					sliceMins.x += (sweepRenderer->getLastSliceDrawn() - sweepRenderer->getMarginNCubes()) * sweepRenderer->getCubeWidth();
					sliceMaxs.x = sliceMins.x + sweepRenderer->getCubeWidth();
					debugoverlay->AddBoxOverlay( overlayCenter + vecRenderOrigin, sliceMins, sliceMaxs, QAngle( 0, angle, 0 ), 255, 0, 0, 0, 0 );
				}
			}
		}

		tiler->endFrame();

		ImpTilerFactory::factory->returnTiler(tiler);
	}
	else
	{
		RENDERER_CLASS* sweepRenderer = ImpRendererFactory::factory->getRenderer();

		// This is the no tiler version. We just draw the center tile.
		sweepRenderer->beginFrame(!(r_surface_debug_draw_margin.GetBool()), (void*)&pRenderContext);
		sweepRenderer->setOffset(Point3D(0.0f, 0.0f, 0.0f));
		sweepRenderer->beginTile(NULL);

		for (int i = 0; i < g_SurfaceRenderParticles.Count(); i++)
		{
			sweepRenderer->addParticle(&(g_SurfaceRenderParticles[i]));
		}

		sweepRenderer->endTile();
		sweepRenderer->endFrame();

		ImpRendererFactory::factory->returnRenderer(sweepRenderer);
	}

	//////////////////////////////////////
	// Clean up render context
	//////////////////////////////////////
	pRenderContext->PopMatrix();
}


void Surface_CullOutOfViewParticles( void )
{
	int iParticlesRemoved = 0;

	for ( int i = 0; i < g_SurfaceRenderParticles.Count() - iParticlesRemoved; ++i )
	{
		ImpParticleWithOneInterpolant* imp_particle = &(g_SurfaceRenderParticles[ i ]);
		Vector vCenter = Vector( imp_particle->center[ 0 ], imp_particle->center[ 1 ], imp_particle->center[ 2 ] );

		if ( R_CullSphere( view->GetFrustum(), 5, &vCenter, imp_particle->scale * 24.0f ) )
		{
			ImpParticleWithOneInterpolant* last_particle = &(g_SurfaceRenderParticles[ g_SurfaceRenderParticles.Count() - iParticlesRemoved - 1 ]);
			*imp_particle = *last_particle;

			++iParticlesRemoved;
			--i;
		}
	}

	if ( iParticlesRemoved )
	{
		g_SurfaceRenderParticles.SetCountNonDestructively( g_SurfaceRenderParticles.Count() - iParticlesRemoved );
	}
}


void Surface_Draw( IClientRenderable *pClientRenderable, const Vector &vecRenderOrigin, IMaterial *pMaterial, float flCubeWidth, bool bSurfaceNoParticleCull )
{
	if ( !bSurfaceNoParticleCull )
	{
		// Reduce the complexity by first removing particles that won't visibly affect the view
		Surface_CullOutOfViewParticles();
	}

	if ( !r_surface_draw_isosurface.GetBool() )
	{
		// This draws particles as spheres instead of an isosurface
		Surface_DrawSpheres( pMaterial, flCubeWidth );
	}
	else
	{
		Surface_DrawIsoSurface( pClientRenderable, vecRenderOrigin, pMaterial, flCubeWidth );
	}
}

#endif