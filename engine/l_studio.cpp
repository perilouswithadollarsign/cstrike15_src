//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// models are the only shared resource between a client and server running
// on the same machine.
//===========================================================================//

#include "render_pch.h"
#include "client.h"
#include "gl_model_private.h"
#include "studio.h"
#include "phyfile.h"
#include "cdll_int.h"
#include "istudiorender.h"
#include "client_class.h"
#include "float.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/ivballoctracker.h"
#include "modelloader.h"
#include "lightcache.h"
#include "studio_internal.h"
#include "cdll_engine_int.h"
#include "vphysics_interface.h"
#include "utllinkedlist.h"
#include "studio.h"
#include "icliententitylist.h"
#include "engine/ivmodelrender.h"
#include "optimize.h"
#include "icliententity.h"
#include "sys_dll.h"
#include "debugoverlay.h"
#include "enginetrace.h"
#include "l_studio.h"
#include "filesystem_engine.h"
#include "ModelInfo.h"
#include "cl_main.h"
#include "tier0/vprof.h"
#include "r_decal.h"
#include "vstdlib/random.h"
#include "datacache/idatacache.h"
#include "materialsystem/materialsystem_config.h"
#include "IHammer.h"
#if defined( _WIN32 ) && !defined( _X360 )
#include <xmmintrin.h>
#endif
#include "staticpropmgr.h"
#include "materialsystem/hardwareverts.h"
#include "tier1/callqueue.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "tier1/utlsortvector.h"
#include "tier1/lzmaDecoder.h"
#include "ipooledvballocator.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// #define VISUALIZE_TIME_AVERAGE 1

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
void R_StudioInitLightingCache( void );
float Engine_WorldLightDistanceFalloff( const dworldlight_t *wl, const Vector& delta, bool bNoRadiusCheck = false );
void SetRootLOD_f( IConVar *var, const char *pOldString, float flOldValue );
void r_lod_f( IConVar *var, const char *pOldValue, float flOldValue );
void FlushLOD_f();

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

ConVar r_drawmodelstatsoverlay( "r_drawmodelstatsoverlay", "0", FCVAR_CHEAT );
ConVar r_drawmodelstatsoverlaydistance( "r_drawmodelstatsoverlaydistance", "500", FCVAR_CHEAT );
ConVar r_drawmodelstatsoverlayfilter( "r_drawmodelstatsoverlayfilter", "-1", FCVAR_CHEAT );
ConVar r_drawmodellightorigin( "r_DrawModelLightOrigin", "0", FCVAR_CHEAT );
extern ConVar r_worldlights;
ConVar r_lod( "r_lod", "-1", 0, "", r_lod_f );
static ConVar r_entity( "r_entity", "-1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
static ConVar r_lightaverage( "r_lightaverage", "1", 0, "Activates/deactivate light averaging" );
static ConVar r_lightinterp( "r_lightinterp", "5", FCVAR_CHEAT, "Controls the speed of light interpolation, 0 turns off interpolation" );
static ConVar r_eyeglintlodpixels( "r_eyeglintlodpixels", "20.0", 0, "The number of pixels wide an eyeball has to be before rendering an eyeglint.  Is a floating point value." );
ConVar r_rootlod( "r_rootlod", "0", FCVAR_MATERIAL_SYSTEM_THREAD, "Root LOD", true, 0, true, MAX_NUM_LODS-1, SetRootLOD_f );
#ifndef _PS3
static ConVar r_decalstaticprops( "r_decalstaticprops", "1", 0, "Decal static props test" );
#else
static ConVar r_decalstaticprops( "r_decalstaticprops", "0", 0, "Decal static props test" );
#endif
static ConCommand r_flushlod( "r_flushlod", FlushLOD_f, "Flush and reload LODs." );
ConVar r_debugrandomstaticlighting( "r_debugrandomstaticlighting", "0", FCVAR_CHEAT, "Set to 1 to randomize static lighting for debugging.  Must restart for change to take affect." );
ConVar r_proplightingfromdisk( "r_proplightingfromdisk", "1", 0, "0=Off, 1=On, 2=Show Errors" );
static ConVar r_itemblinkmax( "r_itemblinkmax", ".3", FCVAR_CHEAT );
static ConVar r_itemblinkrate( "r_itemblinkrate", "4.5", FCVAR_CHEAT );
static ConVar r_proplightingpooling( "r_proplightingpooling", "-1.0", FCVAR_CHEAT, "0 - off, 1 - static prop color meshes are allocated from a single shared vertex buffer (on hardware that supports stream offset)" );

//-----------------------------------------------------------------------------
// StudioRender config 
//-----------------------------------------------------------------------------
static ConVar	r_showenvcubemap( "r_showenvcubemap", "0", FCVAR_CHEAT );
static ConVar	r_eyemove		( "r_eyemove", "1", FCVAR_ARCHIVE ); // look around
static ConVar	r_eyeshift_x	( "r_eyeshift_x", "0", FCVAR_ARCHIVE ); // eye X position
static ConVar	r_eyeshift_y	( "r_eyeshift_y", "0", FCVAR_ARCHIVE ); // eye Y position
static ConVar	r_eyeshift_z	( "r_eyeshift_z", "0", FCVAR_ARCHIVE ); // eye Z position
static ConVar	r_eyesize		( "r_eyesize", "0", FCVAR_ARCHIVE ); // adjustment to iris textures
static ConVar	mat_softwareskin( "mat_softwareskin", "0", FCVAR_CHEAT );
static ConVar	r_nohw			( "r_nohw", "0", FCVAR_CHEAT );
static ConVar	r_nosw			( "r_nosw", "0", FCVAR_CHEAT );
static ConVar	r_teeth			( "r_teeth", "1" );
static ConVar	r_drawentities	( "r_drawentities", "1", FCVAR_CHEAT );
static ConVar	r_flex			( "r_flex", "1" );
static ConVar	r_eyes			( "r_eyes", "1" );
static ConVar	r_skin			( "r_skin", "0", FCVAR_CHEAT );
static ConVar	r_modelwireframedecal ( "r_modelwireframedecal", "0", FCVAR_CHEAT );
static ConVar	r_maxmodeldecal ( "r_maxmodeldecal", "50" );
ConVar			r_slowpathwireframe( "r_slowpathwireframe", "0", FCVAR_CHEAT );

static ConVar r_ignoreStaticColorChecksum( "r_ignoreStaticColorChecksum", "1", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY, "0 - validate vhvhdr and studiohdr checksum, 1 - default, ignore checksum (useful if iterating physics model only for example)" );


static StudioRenderConfig_t s_StudioRenderConfig;
//#define VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK 1
#define VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( name ) VPROF_( name , 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 )



void UpdateStudioRenderConfig( void )
{
	// This can happen during initialization
	if ( !g_pMaterialSystemConfig || !g_pStudioRender )
		return;

	memset( &s_StudioRenderConfig, 0, sizeof(s_StudioRenderConfig) );

	s_StudioRenderConfig.bEyeMove = !!r_eyemove.GetInt();
	s_StudioRenderConfig.fEyeShiftX = r_eyeshift_x.GetFloat();
	s_StudioRenderConfig.fEyeShiftY = r_eyeshift_y.GetFloat();
	s_StudioRenderConfig.fEyeShiftZ = r_eyeshift_z.GetFloat();
	s_StudioRenderConfig.fEyeSize = r_eyesize.GetFloat();	
	if ( IsPC() && ( mat_softwareskin.GetInt() || ShouldDrawInWireFrameMode() || r_slowpathwireframe.GetInt() ) )
	{
		s_StudioRenderConfig.bSoftwareSkin = true;
	}
	else
	{
		s_StudioRenderConfig.bSoftwareSkin = false;
	}
	s_StudioRenderConfig.bNoHardware = !!r_nohw.GetInt();
	s_StudioRenderConfig.bNoSoftware = !!r_nosw.GetInt();
	s_StudioRenderConfig.bTeeth = !!r_teeth.GetInt();
	s_StudioRenderConfig.drawEntities = r_drawentities.GetInt();
	s_StudioRenderConfig.bFlex = !!r_flex.GetInt();
	s_StudioRenderConfig.bEyes = !!r_eyes.GetInt();
	s_StudioRenderConfig.bWireframe = ShouldDrawInWireFrameMode() || r_slowpathwireframe.GetInt();
	s_StudioRenderConfig.bDrawZBufferedWireframe = s_StudioRenderConfig.bWireframe && ( WireFrameMode() != 1 );
	s_StudioRenderConfig.bDrawNormals = mat_normals.GetBool();
	s_StudioRenderConfig.skin = r_skin.GetInt();
	s_StudioRenderConfig.maxDecalsPerModel = r_maxmodeldecal.GetInt();
	s_StudioRenderConfig.bWireframeDecals = r_modelwireframedecal.GetInt() != 0;
	
	s_StudioRenderConfig.fullbright = g_pMaterialSystemConfig->nFullbright;
	s_StudioRenderConfig.bSoftwareLighting = g_pMaterialSystemConfig->bSoftwareLighting;

	s_StudioRenderConfig.bShowEnvCubemapOnly = r_showenvcubemap.GetBool();
	s_StudioRenderConfig.fEyeGlintPixelWidthLODThreshold = r_eyeglintlodpixels.GetFloat();

	g_pStudioRender->UpdateConfig( s_StudioRenderConfig );
}

void R_InitStudio( void )
{
#ifndef DEDICATED
	R_StudioInitLightingCache();
#endif
}

//-----------------------------------------------------------------------------
// Converts world lights to materialsystem lights
//-----------------------------------------------------------------------------

#define MIN_LIGHT_VALUE 0.03f

bool WorldLightToMaterialLight( dworldlight_t* pWorldLight, LightDesc_t& light )
{
	int nType = pWorldLight->type;

	if ( nType == emit_surface )
	{
		// A 180 degree spotlight
		light.m_Type = MATERIAL_LIGHT_SPOT;
		light.m_Color = pWorldLight->intensity;
		light.m_Position = pWorldLight->origin;
		light.m_Direction = pWorldLight->normal;
		light.m_Range = pWorldLight->radius;
		light.m_Falloff = 1.0f;
		light.m_Attenuation0 = 0.0f;
		light.m_Attenuation1 = 0.0f;
		light.m_Attenuation2 = 1.0f;
		light.m_Theta = M_PI * 0.5f;
		light.m_Phi = M_PI * 0.5f;
		light.m_ThetaDot = 0.0f;
		light.m_PhiDot = 0.0f;
		light.m_OneOverThetaDotMinusPhiDot = 1.0f;
		light.m_Flags = LIGHTTYPE_OPTIMIZATIONFLAGS_DERIVED_VALUES_CALCED | LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2;
		return true;
	}
	float flAttenuation0 = 0.0f;
	float flAttenuation1 = 0.0f;
	float flAttenuation2 = 0.0f;
	light.m_OneOverThetaDotMinusPhiDot = 1.0f;
	switch(nType)
	{
	case emit_spotlight:
		light.m_Type = MATERIAL_LIGHT_SPOT;
		flAttenuation0 = pWorldLight->constant_attn;
		flAttenuation1 = pWorldLight->linear_attn;
		flAttenuation2 = pWorldLight->quadratic_attn;
		light.m_Theta = acos( pWorldLight->stopdot );
		light.m_Phi = acos( pWorldLight->stopdot2 );
		light.m_ThetaDot = pWorldLight->stopdot;
		light.m_PhiDot = pWorldLight->stopdot2;
		light.m_Falloff = pWorldLight->exponent ? pWorldLight->exponent : 1.0f;
		light.RecalculateOneOverThetaDotMinusPhiDot();
		break;

	case emit_point:
		light.m_Type = MATERIAL_LIGHT_POINT;
		flAttenuation0 = pWorldLight->constant_attn;
		flAttenuation1 = pWorldLight->linear_attn;
		flAttenuation2 = pWorldLight->quadratic_attn;
		break;

	case emit_skylight:
		light.m_Type = MATERIAL_LIGHT_DIRECTIONAL;
		break;

	// NOTE: Can't do quake lights in hardware (x-r factor)
	case emit_quakelight:	// not supported
	case emit_skyambient:	// doesn't factor into local lighting
		// skip these
		return false;
	}

	// No attenuation case..
	if ((flAttenuation0 == 0.0f) && (flAttenuation1 == 0.0f) && (flAttenuation2 == 0.0f))
	{
		flAttenuation0 = 1.0f;
	}

	// renormalize light intensity...
	light.m_Color = pWorldLight->intensity;
	light.m_Position = pWorldLight->origin;
	light.m_Direction = pWorldLight->normal;

	// Compute the light range based on attenuation factors
	float flRange = pWorldLight->radius;
	if (flRange == 0.0f)
	{
		// Make it stop when the lighting gets to min%...
		float intensity = sqrtf( DotProduct( light.m_Color, light.m_Color ) );
		// FALLBACK: older lights use this
		if (flAttenuation2 == 0.0f)
		{
			if (flAttenuation1 == 0.0f)
			{
				flRange = sqrtf(FLT_MAX);
			}
			else
			{
				flRange = (intensity / MIN_LIGHT_VALUE - flAttenuation0) / flAttenuation1;
			}
		}
		else
		{
			float a = flAttenuation2;
			float b = flAttenuation1;
			float c = flAttenuation0 - intensity / MIN_LIGHT_VALUE;
			float discrim = b * b - 4 * a * c;
			if (discrim < 0.0f)
			{
				flRange = sqrtf(FLT_MAX);
			}
			else
			{
				flRange = (-b + sqrtf(discrim)) / (2.0f * a);
				if (flRange < 0)
				{
					flRange = 0;
				}
			}
		}
	}
	uint nFlags = LIGHTTYPE_OPTIMIZATIONFLAGS_DERIVED_VALUES_CALCED;
	if( flAttenuation0 != 0.0f )
	{
		nFlags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION0;
	}
	if( flAttenuation1 != 0.0f )
	{
		nFlags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION1;
	}
	if( flAttenuation2 != 0.0f )
	{
		nFlags |= LIGHTTYPE_OPTIMIZATIONFLAGS_HAS_ATTENUATION2;
	}
	light.m_Attenuation0 = flAttenuation0;
	light.m_Attenuation1 = flAttenuation1;
	light.m_Attenuation2 = flAttenuation2;
	light.m_Range = flRange;
	light.m_Flags = nFlags;
	return true;
}


//-----------------------------------------------------------------------------
// Sets the hardware lighting state
//-----------------------------------------------------------------------------

static void R_SetNonAmbientLightingState( int numLights, dworldlight_t *locallight[MAXLOCALLIGHTS],
										  int *pNumLightDescs, LightDesc_t *pLightDescs, bool bUpdateStudioRenderLights )
{
	VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK("R_SetNonAmbientLightingState");
	Assert( numLights >= 0 && numLights <= MAXLOCALLIGHTS );

	// convert dworldlight_t's to LightDesc_t's and send 'em down to g_pStudioRender->
	*pNumLightDescs = 0;

	LightDesc_t *pLightDesc;
	for ( int i = 0; i < numLights; i++)
	{
		pLightDesc = &pLightDescs[*pNumLightDescs];
		if (!WorldLightToMaterialLight( locallight[i], *pLightDesc ))
			continue;

		// Apply lightstyle
		float bias = LightStyleValue( locallight[i]->style );

		// Deal with overbrighting + bias
		pLightDesc->m_Color[0] *= bias;
		pLightDesc->m_Color[1] *= bias;
		pLightDesc->m_Color[2] *= bias;

		*pNumLightDescs += 1;
		Assert( *pNumLightDescs <= MAXLOCALLIGHTS );
	}

	if ( bUpdateStudioRenderLights )
	{
		g_pStudioRender->SetLocalLights( *pNumLightDescs, pLightDescs );
	}
}


//-----------------------------------------------------------------------------
// Computes the center of the studio model for illumination purposes
//-----------------------------------------------------------------------------
void R_ComputeLightingOrigin( IClientRenderable *pRenderable, studiohdr_t* pStudioHdr, const matrix3x4_t &matrix, Vector& center )
{
	pRenderable->ComputeLightingOrigin( pStudioHdr->IllumPositionAttachmentIndex(), pStudioHdr->illumposition, matrix, center );
}



// TODO: move cone calcs to position
// TODO: cone clipping calc's wont work for boxlight since the player asks for a single point.  Not sure what the volume is.
float Engine_WorldLightDistanceFalloff( const dworldlight_t *wl, const Vector& delta, bool bNoRadiusCheck )
{
	float falloff;

	switch (wl->type)
	{
		case emit_surface:
#if 1
			// Cull out stuff that's too far
			if (wl->radius != 0)
			{
				if ( DotProduct( delta, delta ) > (wl->radius * wl->radius))
					return 0.0f;
			}

			return InvRSquared(delta);
#else
			// 1/r*r
			falloff = DotProduct( delta, delta );
			if (falloff < 1)
				return 1.f;
			else
				return 1.f / falloff;
#endif

			break;

		case emit_skylight:
			return 1.f;
			break;

		case emit_quakelight:
			// X - r;
			falloff = wl->linear_attn - FastSqrt( DotProduct( delta, delta ) );
			if (falloff < 0)
				return 0.f;

			return falloff;
			break;

		case emit_skyambient:
			return 1.f;
			break;

		case emit_point:
		case emit_spotlight:	// directional & positional
			{
				float dist2, dist;

				dist2 = DotProduct( delta, delta );
				dist = FastSqrt( dist2 );

				// Cull out stuff that's too far
				if (!bNoRadiusCheck && (wl->radius != 0) && (dist > wl->radius))
					return 0.f;

				return 1.f / (wl->constant_attn + wl->linear_attn * dist + wl->quadratic_attn * dist2);
			}

			break;
		default:
			// Bug: need to return an error
			break;
	}
	return 1.f;
}

/*
  light_normal (lights normal translated to same space as other normals)
  surface_normal
  light_direction_normal | (light_pos - vertex_pos) |
*/
float Engine_WorldLightAngle( const dworldlight_t *wl, const Vector& lnormal, const Vector& snormal, const Vector& delta )
{
	float dot, dot2, ratio = 0;

	switch (wl->type)
	{
		case emit_surface:
			dot = DotProduct( snormal, delta );
			if (dot < 0)
				return 0;

			dot2 = -DotProduct (delta, lnormal);
			if (dot2 <= ON_EPSILON/10)
				return 0; // behind light surface

			return dot * dot2;

		case emit_point:
			dot = DotProduct( snormal, delta );
			if (dot < 0)
				return 0;
			return dot;

		case emit_spotlight:
//			return 1.0; // !!!
			dot = DotProduct( snormal, delta );
			if (dot < 0)
				return 0;

			dot2 = -DotProduct (delta, lnormal);
			if (dot2 <= wl->stopdot2)
				return 0; // outside light cone

			ratio = dot;
			if (dot2 >= wl->stopdot)
				return ratio;	// inside inner cone

			if ((wl->exponent == 1) || (wl->exponent == 0))
			{
				ratio *= (dot2 - wl->stopdot2) / (wl->stopdot - wl->stopdot2);
			}
			else
			{
				ratio *= pow((dot2 - wl->stopdot2) / (wl->stopdot - wl->stopdot2), wl->exponent );
			}
			return ratio;

		case emit_skylight:
			dot2 = -DotProduct( snormal, lnormal );
			if (dot2 < 0)
				return 0;
			return dot2;

		case emit_quakelight:
			// linear falloff
			dot = DotProduct( snormal, delta );
			if (dot < 0)
				return 0;
			return dot;

		case emit_skyambient:
			// not supported
			return 1;

		default:
			// Bug: need to return an error
			break;
	} 
	return 0;
}

//-----------------------------------------------------------------------------
// Allocator for color mesh vertex buffers (for use with static props only).
// It uses a trivial allocation scheme, which assumes that allocations and
// deallocations are not interleaved (you do all allocs, then all deallocs).
//-----------------------------------------------------------------------------
class CPooledVBAllocator_ColorMesh : public IPooledVBAllocator
{
public:

	CPooledVBAllocator_ColorMesh();
	virtual ~CPooledVBAllocator_ColorMesh();

	// Allocate the shared mesh (vertex buffer)
	virtual bool			Init( VertexFormat_t format, int numVerts );
	// Free the shared mesh (after Deallocate is called for all sub-allocs)
	virtual void			Clear();

	// Get the shared mesh (vertex buffer) from which sub-allocations are made
	virtual IMesh			*GetSharedMesh() { return m_pMesh; }

	// Get a pointer to the start of the vertex buffer data
	virtual void			*GetVertexBufferBase() { return m_pVertexBufferBase; }
	virtual int				GetNumVertsAllocated() { return m_totalVerts; } 

	// Allocate a sub-range of 'numVerts' from free space in the shared vertex buffer
	// (returns the byte offset from the start of the VB to the new allocation)
	virtual int				Allocate( int numVerts );
	// Deallocate an existing allocation
	virtual void			Deallocate( int offset, int numVerts );

private:

	// Assert/warn that the allocator is in a clear/empty state (returns FALSE if not)
	bool					CheckIsClear( void );

	IMesh		*m_pMesh;				// The shared mesh (vertex buffer) from which sub-allocations are made
	void		*m_pVertexBufferBase;	// A pointer to the start of the vertex buffer data
	int			m_totalVerts;			// The number of verts in the shared vertex buffer
	int			m_vertexSize;			// The stride of the shared vertex buffer

	int			m_numAllocations;		// The number of extant allocations
	int			m_numVertsAllocated;	// The number of vertices in extant allocations
	int			m_nextFreeOffset;		// The offset to be returned by the next call to Allocate()
	// (incremented as a simple stack)
	bool		m_bStartedDeallocation;	// This is set when Deallocate() is called for the first time,
	// at which point Allocate() cannot be called again until all
	// extant allocations have been deallocated.
};

struct colormeshparams_t
{
	int					m_nMeshes;
	int					m_nTotalVertexes;
	// Given memory alignment (VBs must be 4-KB aligned on X360, for example), it can be more efficient
	// to allocate many color meshes out of a single shared vertex buffer (using vertex 'stream offset')
	IPooledVBAllocator *m_pPooledVBAllocator;
	int					m_nVertexes[256];
	FileNameHandle_t	m_fnHandle;
};

class CColorMeshData
{
public:
	void DestroyResource()
	{
		g_pFileSystem->AsyncFinish( m_hAsyncControl, true );
		g_pFileSystem->AsyncRelease( m_hAsyncControl );

		// release the array of meshes
		CMatRenderContextPtr pRenderContext( materials );

		for ( int i=0; i<m_nMeshes; i++ )
		{
			if ( m_pMeshInfos[i].m_pPooledVBAllocator )
			{
				// Let the pooling allocator dealloc this sub-range of the shared vertex buffer
				m_pMeshInfos[i].m_pPooledVBAllocator->Deallocate( m_pMeshInfos[i].m_nVertOffsetInBytes, m_pMeshInfos[i].m_nNumVerts );
			}
			else
			{
				// Free this standalone mesh
				pRenderContext->DestroyStaticMesh( m_pMeshInfos[i].m_pMesh );
			}
		}
		delete [] m_pMeshInfos;
		delete [] m_ppTargets;
		delete this;
	}

	CColorMeshData *GetData()
	{ 
		return this; 
	}

	unsigned int Size()
	{ 
		return m_nTotalSize; 
	}

	static CColorMeshData *CreateResource( const colormeshparams_t &params )
	{
		CColorMeshData *data = new CColorMeshData;
		static ConVarRef r_staticlight_streams( "r_staticlight_streams" );
		int numLightingComponents = r_staticlight_streams.GetInt();

		data->m_bHasInvalidVB = false;
		data->m_bColorMeshValid = false;
		data->m_bNeedsRetry = false;
		data->m_hAsyncControl = NULL;
		data->m_fnHandle = params.m_fnHandle;

		data->m_nTotalSize = params.m_nMeshes*sizeof( IMesh* ) + params.m_nTotalVertexes*4*numLightingComponents;
		data->m_nMeshes = params.m_nMeshes;
		data->m_pMeshInfos = new ColorMeshInfo_t[params.m_nMeshes];
		Q_memset( data->m_pMeshInfos, 0, params.m_nMeshes*sizeof( ColorMeshInfo_t ) );
		data->m_ppTargets = new unsigned char *[params.m_nMeshes];

#if !defined( DX_TO_GL_ABSTRACTION )
		CMeshBuilder meshBuilder;
		MaterialLock_t hLock = materials->Lock();
#endif
		CMatRenderContextPtr pRenderContext( materials );
	
		for ( int i=0; i<params.m_nMeshes; i++ )
		{
			VertexFormat_t vertexFormat = VERTEX_SPECULAR;

			if ( numLightingComponents > 1 )
			{
				vertexFormat = VERTEX_NORMAL;
			}

			data->m_pMeshInfos[i].m_pMesh				= NULL;
			data->m_pMeshInfos[i].m_pPooledVBAllocator	= params.m_pPooledVBAllocator;
			data->m_pMeshInfos[i].m_nVertOffsetInBytes	= 0;
			data->m_pMeshInfos[i].m_nNumVerts			= params.m_nVertexes[i];

			if ( params.m_pPooledVBAllocator != NULL )
			{
				// Allocate a portion of a single, shared VB for each color mesh
				data->m_pMeshInfos[i].m_nVertOffsetInBytes = params.m_pPooledVBAllocator->Allocate( params.m_nVertexes[i] );
				
				if ( data->m_pMeshInfos[i].m_nVertOffsetInBytes == -1 )
				{
					// Failed (fall back to regular allocations)
					data->m_pMeshInfos[i].m_pPooledVBAllocator = NULL;
					data->m_pMeshInfos[i].m_nVertOffsetInBytes = 0;
				}
				else
				{
					// Set up the mesh+data pointers
					data->m_pMeshInfos[i].m_pMesh	= params.m_pPooledVBAllocator->GetSharedMesh();
					data->m_ppTargets[i]			= ( (unsigned char *)params.m_pPooledVBAllocator->GetVertexBufferBase() ) + data->m_pMeshInfos[i].m_nVertOffsetInBytes;
				}
			}

			if ( data->m_pMeshInfos[i].m_pMesh == NULL )
			{
				if ( g_VBAllocTracker )
					g_VBAllocTracker->TrackMeshAllocations( "CColorMeshData::CreateResource" );

				// Allocate a standalone VB per color mesh
				data->m_pMeshInfos[i].m_pMesh = pRenderContext->CreateStaticMesh( vertexFormat, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_COLOR );

#if !defined( DX_TO_GL_ABSTRACTION )
				// build out the underlying vertex buffer
				// lock now in same thread as draw, otherwise d3drip
				meshBuilder.Begin( data->m_pMeshInfos[i].m_pMesh, MATERIAL_HETEROGENOUS,
					params.m_nVertexes[i], 0 );
				if ( IsPC() && meshBuilder.VertexSize() == 0 )	 // HACK: mesh creation can return null vertex buffer if alt-tabbed away
				{
					data->m_ppTargets[i] = NULL;
					data->m_bHasInvalidVB = true;
				}
				else if ( numLightingComponents > 1 )
				{
					data->m_ppTargets[i] = reinterpret_cast< unsigned char * >( const_cast< float * >( meshBuilder.Normal() ) );
				}
				else
				{
					data->m_ppTargets[i] = ( meshBuilder.Specular() );
				}
				meshBuilder.End();
#endif

				if ( g_VBAllocTracker )
					g_VBAllocTracker->TrackMeshAllocations( NULL );
			}

			Assert( data->m_pMeshInfos[i].m_pMesh );
			if ( !data->m_pMeshInfos[i].m_pMesh )
			{
				data->DestroyResource();
#if !defined( DX_TO_GL_ABSTRACTION )
				materials->Unlock( hLock );
#endif
				return NULL;
			}
		}
#if !defined( DX_TO_GL_ABSTRACTION )
		materials->Unlock( hLock );
#endif
		return data;
	}

	static unsigned int EstimatedSize( const colormeshparams_t &params )
	{
		// Color per vertex is 4 bytes (1 color) or 12 bytes (3 colors)
		static ConVarRef r_staticlight_streams( "r_staticlight_streams" );
		int numLightingComponents = r_staticlight_streams.GetInt();
		return params.m_nMeshes*sizeof( IMesh* ) + params.m_nTotalVertexes*4*numLightingComponents;
	}

	int					m_nMeshes;
	ColorMeshInfo_t		*m_pMeshInfos;
	unsigned char		**m_ppTargets;
	unsigned int		m_nTotalSize;
	FSAsyncControl_t	m_hAsyncControl;
	unsigned int		m_bHasInvalidVB : 1;
	unsigned int		m_bColorMeshValid : 1;
	unsigned int		m_bNeedsRetry : 1;
	FileNameHandle_t	m_fnHandle;
};

//-----------------------------------------------------------------------------
//
// Implementation of IVModelRender
//
//-----------------------------------------------------------------------------

#include "tier0/memdbgoff.h"
// UNDONE: Move this to hud export code, subsume previous functions
class CModelRender : public IVModelRender,
					 public CManagedDataCacheClient< CColorMeshData, colormeshparams_t >
{
public:
	// members of the IVModelRender interface
	virtual void ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType = OVERRIDE_NORMAL, int nMaterialIndex = -1 );
	virtual bool IsForcedMaterialOverride();
	virtual int DrawModel( 	
					int flags, IClientRenderable *cliententity,
					ModelInstanceHandle_t instance, int entity_index, const model_t *model, 
					const Vector& origin, QAngle const& angles,
					int skin, int body, int hitboxset, 
					const matrix3x4_t* pModelToWorld,
					const matrix3x4_t *pLightingOffset );

	virtual void  SetViewTarget( const CStudioHdr *pStudioHdr, int nBodyIndex, const Vector& target );

	// Creates, destroys instance data to be associated with the model
	virtual ModelInstanceHandle_t CreateInstance( IClientRenderable *pRenderable, LightCacheHandle_t* pHandle );
	virtual void SetStaticLighting( ModelInstanceHandle_t handle, LightCacheHandle_t* pCache );
	virtual LightCacheHandle_t GetStaticLighting( ModelInstanceHandle_t handle );
	virtual void DestroyInstance( ModelInstanceHandle_t handle );
	virtual bool ChangeInstance( ModelInstanceHandle_t handle, IClientRenderable *pRenderable );

	// Creates a decal on a model instance by doing a planar projection
	// along the ray. The material is the decal material, the radius is the
	// radius of the decal to create.
	virtual void AddDecal( ModelInstanceHandle_t handle, Ray_t const& ray, 
		const Vector& decalUp, int decalIndex, int body, bool noPokethru = false, int maxLODToDecal = ADDDECAL_TO_ALL_LODS, IMaterial *pSpecifyMaterial = NULL, float w=1.0f, float h=1.0f, void *pvProxyUserData = NULL, int nAdditionalDecalFlags = 0 ) OVERRIDE;

	// Removes all the decals on a model instance
	virtual void RemoveAllDecals( ModelInstanceHandle_t handle );

	// Returns true if both the instance handle is valid and has a non-zero decal count
	virtual bool ModelHasDecals( ModelInstanceHandle_t handle );

	// Remove all decals from all models
	virtual void RemoveAllDecalsFromAllModels( bool bRenderContextValid );

	// Shadow rendering (render-to-texture)
	virtual matrix3x4a_t* DrawModelShadowSetup( IClientRenderable *pRenderable, int body, int skin, DrawModelInfo_t *pInfo, matrix3x4a_t *pBoneToWorld );
	virtual void DrawModelShadow( IClientRenderable *pRenderable, const DrawModelInfo_t &info, matrix3x4a_t *pBoneToWorld );

	// Used to allow the shadow mgr to manage a list of shadows per model
	unsigned short& FirstShadowOnModelInstance( ModelInstanceHandle_t handle ) { return m_ModelInstances[handle].m_FirstShadow; }

	// This gets called when overbright, etc gets changed to recompute static prop lighting.
	virtual bool RecomputeStaticLighting( ModelInstanceHandle_t handle );

	// Handlers for alt-tab
	virtual void ReleaseAllStaticPropColorData( void );
	virtual void RestoreAllStaticPropColorData( void );

	// Extended version of drawmodel
	virtual bool DrawModelSetup( IMatRenderContext *pRenderContext, ModelRenderInfo_t &pInfo, DrawModelState_t *pState, matrix3x4_t **ppBoneToWorldOut );
	virtual int	DrawModelEx( ModelRenderInfo_t &pInfo );
	virtual int	DrawModelExStaticProp( IMatRenderContext *pRenderContext, ModelRenderInfo_t &pInfo );
	virtual int DrawStaticPropArrayFast( StaticPropRenderInfo_t *pProps, int count, bool bShadowDepth );

	// Sets up lighting context for a point in space
	virtual void SetupLighting( const Vector &vecCenter );
	virtual void SuppressEngineLighting( bool bSuppress );
	virtual void ComputeLightingState( int nCount, const LightingQuery_t *pQuery, MaterialLightingState_t *pState, ITexture **ppEnvCubemapTexture );
	virtual void ComputeStaticLightingState( int nCount, const StaticLightingQuery_t *pQuery, MaterialLightingState_t *pState, MaterialLightingState_t *pDecalState, ColorMeshInfo_t **ppStaticLighting, ITexture **ppEnvCubemapTexture, DataCacheHandle_t *pColorMeshHandles );
	virtual void GetModelDecalHandles( StudioDecalHandle_t *pDecals, int nDecalStride, int nCount, const ModelInstanceHandle_t *pHandles );
	virtual void CleanupStaticLightingState( int nCount, DataCacheHandle_t *pColorMeshHandles );
	void UnlockCacheCacheHandleArray( int nCount, DataCacheHandle_t *pColorMeshHandles )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			if ( pColorMeshHandles[i] != DC_INVALID_HANDLE )
			{
				CacheUnlock( pColorMeshHandles[i] );
			}
		}
	}

	inline vertexFileHeader_t *CacheVertexData() 
	{
		return g_pMDLCache->GetVertexData( VoidPtrToMDLHandle( m_pStudioHdr->VirtualModel() ) );
	}

	bool Init();
	void Shutdown();

	bool GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen );

	struct staticPropAsyncContext_t
	{
		DataCacheHandle_t	m_ColorMeshHandle;
		CColorMeshData		*m_pColorMeshData;
		int					m_StaticPropIndex;
		int					m_nMeshes;
		unsigned int		m_nRootLOD;
		char				m_szFilename[MAX_PATH];
	};
	void StaticPropColorMeshCallback( void *pContext, const void *pData, int numReadBytes, FSAsyncStatus_t asyncStatus );

	// 360 holds onto static prop color meshes during same map transitions
	void PurgeCachedStaticPropColorData();
	bool IsStaticPropColorDataCached( const char *pName );
	DataCacheHandle_t GetCachedStaticPropColorData( const char *pName );

	virtual void SetupColorMeshes( int nTotalVerts );

	virtual void SetupLightingEx( const Vector &vecCenter, ModelInstanceHandle_t handle );

	virtual bool GetBrightestShadowingLightSource( const Vector &vecCenter, Vector& lightPos, Vector& lightBrightness, bool bAllowNonTaggedLights );

private:
	enum
	{
		CURRENT_LIGHTING_UNINITIALIZED = -999999
	};

	enum ModelInstanceFlags_t
	{
		MODEL_INSTANCE_HAS_STATIC_LIGHTING    = 0x1,
		MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR = 0x2,
		MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD = 0x4,
		MODEL_INSTANCE_HAS_COLOR_DATA		  = 0x8
	};

	struct ModelInstanceLightingState_t
	{
		// Stores off the current lighting state
		float m_flLightingTime;
		LightingState_t m_CurrentLightingState;
		LightingState_t	m_AmbientLightingState;
		Vector m_flLightIntensity[MAXLOCALLIGHTS];
		DECLARE_FIXEDSIZE_ALLOCATOR( ModelInstanceLightingState_t );
	};

	struct ModelInstance_t
	{
		ModelInstance_t()
		{
			m_pLightingState = new ModelInstanceLightingState_t;
		}

		~ModelInstance_t()
		{
			delete m_pLightingState;
		}

		IClientRenderable* m_pRenderable;

		// Need to store off the model. When it changes, we lose all instance data..
		model_t* m_pModel;
		StudioDecalHandle_t	m_DecalHandle;

		// First shadow projected onto the model
		unsigned short	m_FirstShadow;
		unsigned short  m_nFlags;

		// Static lighting
		LightCacheHandle_t m_LightCacheHandle;

		// Color mesh managed by cache
		DataCacheHandle_t m_ColorMeshHandle;

		ModelInstanceLightingState_t *m_pLightingState;
	};

	int ComputeLOD( IMatRenderContext *pRenderContext, const ModelRenderInfo_t &info, studiohwdata_t *pStudioHWData );

	void DrawModelExecute( IMatRenderContext *pRenderContext, const DrawModelState_t &state, const ModelRenderInfo_t &pInfo, matrix3x4_t *pCustomBoneToWorld = NULL );

	void InitColormeshParams( ModelInstance_t &instance, studiohwdata_t *pStudioHWData, colormeshparams_t *pColorMeshParams );
	CColorMeshData *FindOrCreateStaticPropColorData( ModelInstanceHandle_t handle );
	void DestroyStaticPropColorData( ModelInstanceHandle_t handle );
	bool UpdateStaticPropColorData( IHandleEntity *pEnt, ModelInstanceHandle_t handle );
	void ProtectColorDataIfQueued( DataCacheHandle_t );
	void ComputeAmbientBoost( int nCount, const LightingQuery_t *pQuery, MaterialLightingState_t *pState );

	void ValidateStaticPropColorData( ModelInstanceHandle_t handle );
	bool LoadStaticPropColorData( IHandleEntity *pProp, DataCacheHandle_t colorMeshHandle, studiohwdata_t *pStudioHWData );

	// Returns true if the model instance is valid
	bool IsModelInstanceValid( ModelInstanceHandle_t handle );
	
	void DebugDrawLightingOrigin( const DrawModelState_t& state, const ModelRenderInfo_t &pInfo );

	LightingState_t *TimeAverageLightingState( ModelInstanceHandle_t handle,
		LightingState_t *pLightingState, int nEntIndex, const Vector *pLightingOrigin );

	// Cause the current lighting state to match the given one
	void SnapCurrentLightingState( ModelInstance_t &inst, LightingState_t *pLightingState );

	// Sets up lighting state for rendering
	void StudioSetupLighting( const DrawModelState_t &state, const Vector& absEntCenter, 
		LightCacheHandle_t* pLightcache, bool bVertexLit, bool bNeedsEnvCubemap, bool &bStaticLighting, 
		DrawModelInfo_t &drawInfo, const ModelRenderInfo_t &pInfo, int drawFlags );

	// Time average the ambient term
	void TimeAverageAmbientLight( LightingState_t &actualLightingState, ModelInstance_t &inst, 
		float flAttenFactor, LightingState_t *pLightingState, const Vector *pLightingOrigin );

	int GetLightingConditions( const Vector &vecLightingOrigin, Vector *pColors, int nMaxLocalLights, LightDesc_t *pLocalLights,
		ITexture *&pEnvCubemapTexture, ModelInstanceHandle_t handle, bool bAllowFast = false );

	// Old-style computation of vertex lighting
	void ComputeModelVertexLightingOld( mstudiomodel_t *pModel, 
		matrix3x4_t& matrix, const LightingState_t &lightingState, color24 *pLighting,
		bool bUseConstDirLighting, float flConstDirLightAmount );

	// New-style computation of vertex lighting
	void ComputeModelVertexLighting( IHandleEntity *pProp, 
		mstudiomodel_t *pModel, OptimizedModel::ModelLODHeader_t *pVtxLOD,
		matrix3x4_t& matrix, Vector4D *pTempMem, color24 *pLighting );

	void SetFullbrightLightingState( int nCount, MaterialLightingState_t *pState );

	void EngineLightingToMaterialLighting( MaterialLightingState_t *pLightingState, const Vector &vecLightingOrigin, const LightingState_t &srcLightingState );

	// Model instance data
	CUtlLinkedList< ModelInstance_t, ModelInstanceHandle_t > m_ModelInstances; 

	// current active model
	studiohdr_t *m_pStudioHdr;

	bool m_bSuppressEngineLighting;

	CUtlDict< DataCacheHandle_t, int > m_CachedStaticPropColorData;
	CThreadFastMutex m_CachedStaticPropMutex;

	// Allocator for static prop color mesh vertex buffers (all are pooled into one VB)
	CPooledVBAllocator_ColorMesh	m_colorMeshVBAllocator;
};

static CModelRender s_ModelRender;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CModelRender, IVModelRender, VENGINE_HUDMODEL_INTERFACE_VERSION, s_ModelRender );
IVModelRender* modelrender = &s_ModelRender;

DEFINE_FIXEDSIZE_ALLOCATOR( CModelRender::ModelInstanceLightingState_t, 100, CUtlMemoryPool::GROW_SLOW );

#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Resource loading for static prop lighting
//-----------------------------------------------------------------------------
class CResourcePreloadPropLighting : public CResourcePreload
{
	virtual bool CreateResource( const char *pName )
	{
		if ( !r_proplightingfromdisk.GetBool() )
		{
			// do nothing, not an error
			return true;
		}

		char szBasename[MAX_PATH];
		char szFilename[MAX_PATH];
		V_FileBase( pName, szBasename, sizeof( szBasename ) );
		V_snprintf( szFilename, sizeof( szFilename ), "%s%s.vhv", szBasename, GetPlatformExt() );

		// static props have the same name across maps
		// can check if loading the same map and early out if data present
		if ( g_pQueuedLoader->IsSameMapLoading() && s_ModelRender.IsStaticPropColorDataCached( szFilename ) )
		{
			// same map is loading, all disk prop lighting was left in the cache
			// otherwise the pre-purge operation below will do the cleanup
			return true;
		}

		// create an anonymous job to get the lighting data in memory, claim during static prop instancing
		LoaderJob_t loaderJob;
		loaderJob.m_pFilename = szFilename;
		loaderJob.m_pPathID = "GAME";
		loaderJob.m_Priority = LOADERPRIORITY_DURINGPRELOAD;
		g_pQueuedLoader->AddJob( &loaderJob );
		return true;
	}

	//-----------------------------------------------------------------------------
	// Pre purge operation before i/o commences
	//-----------------------------------------------------------------------------
	virtual void PurgeUnreferencedResources()
	{
		if ( g_pQueuedLoader->IsSameMapLoading() )
		{
			// do nothing, same map is loading, correct disk prop lighting will still be in data cache
			return;
		}

		// Map is different, need to purge any existing disk prop lighting
		// before anonymous i/o commences, otherwise 2x memory usage
		s_ModelRender.PurgeCachedStaticPropColorData();
	}

	virtual void PurgeAll()
	{
		s_ModelRender.PurgeCachedStaticPropColorData();
	}

#if defined( _PS3 )
	virtual bool RequiresRendererLock()
	{
		return true;
	}
#endif // _PS3
};
static CResourcePreloadPropLighting s_ResourcePreloadPropLighting;

//-----------------------------------------------------------------------------
// Init, shutdown studiorender
//-----------------------------------------------------------------------------
void InitStudioRender( void )
{
	UpdateStudioRenderConfig();
	s_ModelRender.Init();
}

void ShutdownStudioRender( void )
{
	s_ModelRender.Shutdown();
}

//-----------------------------------------------------------------------------
// Hook needed for shadows to work
//-----------------------------------------------------------------------------
unsigned short& FirstShadowOnModelInstance( ModelInstanceHandle_t handle )
{
	return s_ModelRender.FirstShadowOnModelInstance( handle );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void R_RemoveAllDecalsFromAllModels()
{
	s_ModelRender.RemoveAllDecalsFromAllModels( true );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CModelRender::Init()
{
	// start a managed section in the cache
	DataCacheLimits_t limits( (unsigned)-1, (unsigned)-1, 0, 0 );
	CCacheClientBaseClass::Init( g_pDataCache, "ColorMesh", limits );

	if ( IsGameConsole() )
	{
		// due to static prop light pooling and expecting to NEVER LRU purge due to -1 max bytes limit
		// not allowing console mem_force_flush to unexpectedly flush, which would otherwise destabilizes color meshes
		GetCacheSection()->SetOptions( GetCacheSection()->GetOptions() | DC_NO_USER_FORCE_FLUSH );

		g_pQueuedLoader->InstallLoader( RESOURCEPRELOAD_STATICPROPLIGHTING, &s_ResourcePreloadPropLighting );
	}
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CModelRender::Shutdown()
{
	// end the managed section
	CCacheClientBaseClass::Shutdown();
	m_colorMeshVBAllocator.Clear();
}


//-----------------------------------------------------------------------------
// Used by the client to allow it to set lighting state instead of this code
//-----------------------------------------------------------------------------
void CModelRender::SuppressEngineLighting( bool bSuppress )
{
	m_bSuppressEngineLighting = bSuppress;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CModelRender::GetItemName( DataCacheClientID_t clientId, const void *pItem, char *pDest, unsigned nMaxLen )
{
	CColorMeshData *pColorMeshData = (CColorMeshData *)pItem;
	g_pFileSystem->String( pColorMeshData->m_fnHandle, pDest, nMaxLen );
	return true;
}


//-----------------------------------------------------------------------------
// Cause the current lighting state to match the given one
//-----------------------------------------------------------------------------
void CModelRender::SnapCurrentLightingState( ModelInstance_t &inst, LightingState_t *pLightingState )
{
	ModelInstanceLightingState_t &lightingState = *inst.m_pLightingState;
	lightingState.m_CurrentLightingState = *pLightingState;
	for ( int i = 0; i < MAXLOCALLIGHTS; ++i )
	{
		if ( i < pLightingState->numlights )
		{
			lightingState.m_flLightIntensity[i] = pLightingState->locallight[i]->intensity; 
		}
		else
		{
			lightingState.m_flLightIntensity[i].Init( 0.0f, 0.0f, 0.0f );
		}
	}

#ifndef DEDICATED
	lightingState.m_flLightingTime = GetBaseLocalClient().GetTime();
#endif
}


#define AMBIENT_MAX 8.0

//-----------------------------------------------------------------------------
// Time average the ambient term
//-----------------------------------------------------------------------------
void CModelRender::TimeAverageAmbientLight( LightingState_t &actualLightingState, 
										   ModelInstance_t &inst, float flAttenFactor, LightingState_t *pLightingState, const Vector *pLightingOrigin )
{
	ModelInstanceLightingState_t &lightingState = *inst.m_pLightingState;
	flAttenFactor = clamp( flAttenFactor, 0, 1 );   // don't need this but alex is a coward
	Vector vecDelta;
	for ( int i = 0; i < 6; ++i )
	{
		VectorSubtract( pLightingState->r_boxcolor[i], lightingState.m_CurrentLightingState.r_boxcolor[i], vecDelta );
		vecDelta *= flAttenFactor;
		lightingState.m_CurrentLightingState.r_boxcolor[i] = pLightingState->r_boxcolor[i] - vecDelta;

#if defined( VISUALIZE_TIME_AVERAGE ) && !defined( DEDICATED )
		if ( pLightingOrigin )
		{
			Vector vecDir = vec3_origin;
			vecDir[ i >> 1 ] = (i & 0x1) ? -1.0f : 1.0f;
			CDebugOverlay::AddLineOverlay( *pLightingOrigin, *pLightingOrigin + vecDir * 20, 
				255 * lightingState.m_CurrentLightingState.r_boxcolor[i].x, 
				255 * lightingState.m_CurrentLightingState.r_boxcolor[i].y,
				255 * lightingState.m_CurrentLightingState.r_boxcolor[i].z, 255, false, 5.0f );

			CDebugOverlay::AddLineOverlay( *pLightingOrigin + Vector(5, 5, 5), *pLightingOrigin + vecDir * 50, 
				255 * pLightingState->r_boxcolor[i].x, 
				255 * pLightingState->r_boxcolor[i].y,
				255 * pLightingState->r_boxcolor[i].z, 255, true, 5.0f );
		}
#endif
		// haven't been able to find this rare bug which results in ambient light getting "stuck"
		// on the viewmodel extremely rarely , presumably with infinities. So, mask the bug
		// (hopefully) and warn by clamping.
#ifndef NDEBUG
		Assert( inst.m_pLightingState->m_CurrentLightingState.r_boxcolor[i].IsValid() );
		for( int nComp = 0 ; nComp < 3; nComp++ )
		{
			Assert( inst.m_pLightingState->m_CurrentLightingState.r_boxcolor[i][nComp] >= 0.0 );
			Assert( inst.m_pLightingState->m_CurrentLightingState.r_boxcolor[i][nComp] <= AMBIENT_MAX );
		}
#endif
		lightingState.m_CurrentLightingState.r_boxcolor[i].x = clamp( lightingState.m_CurrentLightingState.r_boxcolor[i].x, 0, AMBIENT_MAX );
		lightingState.m_CurrentLightingState.r_boxcolor[i].y = clamp( lightingState.m_CurrentLightingState.r_boxcolor[i].y, 0, AMBIENT_MAX );
		lightingState.m_CurrentLightingState.r_boxcolor[i].z = clamp( lightingState.m_CurrentLightingState.r_boxcolor[i].z, 0, AMBIENT_MAX );
	}
	memcpy( &actualLightingState.r_boxcolor, &lightingState.m_CurrentLightingState.r_boxcolor, sizeof(lightingState.m_CurrentLightingState.r_boxcolor) );
}




//-----------------------------------------------------------------------------
// Do time averaging of the lighting state to avoid popping...
//-----------------------------------------------------------------------------
static LightingState_t actualLightingState;
LightingState_t *CModelRender::TimeAverageLightingState( ModelInstanceHandle_t handle, LightingState_t *pLightingState, int nEntIndex, const Vector *pLightingOrigin )
{
	if ( r_lightaverage.GetInt() == 0 )
		return pLightingState;

#ifndef DEDICATED

	float flInterpFactor = r_lightinterp.GetFloat();
	if ( flInterpFactor == 0 )
		return pLightingState;

	if ( handle == MODEL_INSTANCE_INVALID)
		return pLightingState;

	ModelInstance_t &inst = m_ModelInstances[handle];
	ModelInstanceLightingState_t &instanceLightingState = *inst.m_pLightingState;
	if ( instanceLightingState.m_flLightingTime == CURRENT_LIGHTING_UNINITIALIZED )
	{
		SnapCurrentLightingState( inst, pLightingState );
		return pLightingState;
	}

	float dt = (GetBaseLocalClient().GetTime() - instanceLightingState.m_flLightingTime);
	if ( dt <= 0.0f )
	{
		dt = 0.0f;
	}
	else
	{
		instanceLightingState.m_flLightingTime = GetBaseLocalClient().GetTime();
	}

	static dworldlight_t s_WorldLights[MAXLOCALLIGHTS];
	
	// I'm creating the equation v = vf - (vf-vi)e^-at 
	// where vf = this frame's lighting value, vi = current time averaged lighting value
	int i;
	Vector vecDelta;
	float flAttenFactor = exp( -flInterpFactor * dt );
	TimeAverageAmbientLight( actualLightingState, inst, flAttenFactor, pLightingState, pLightingOrigin );

	// Max # of lights...
	int nWorldLights;
	if ( !g_pMaterialSystemConfig->bSoftwareLighting )
	{
		nWorldLights = MIN( g_pMaterialSystemHardwareConfig->MaxNumLights(), r_worldlights.GetInt() );
	}
	else
	{
		nWorldLights = r_worldlights.GetInt();
	}

	// Create a mapping of identical lights
	int nMatchCount = 0;
	bool pMatch[MAXLOCALLIGHTS];
	Vector pLight[MAXLOCALLIGHTS];
	dworldlight_t *pSourceLight[MAXLOCALLIGHTS];

	memset( pMatch, 0, sizeof(pMatch) );
	for ( i = 0; i < pLightingState->numlights; ++i )
	{
		// By default, assume the light doesn't match an existing light, so blend up from 0
		pLight[i].Init( 0.0f, 0.0f, 0.0f );
		int j;
		for ( j = 0; j < instanceLightingState.m_CurrentLightingState.numlights; ++j )
		{
			if ( pLightingState->locallight[i] == instanceLightingState.m_CurrentLightingState.locallight[j] )
			{
				// Ok, we found a matching light, so use the intensity of that light at the moment
				++nMatchCount;
				pMatch[j] = true;
				pLight[i] = instanceLightingState.m_flLightIntensity[j];
				break;
			}
		}
	}

	// For the lights in the current lighting state, attenuate them toward their actual value
	for ( i = 0; i < pLightingState->numlights; ++i )
	{
		actualLightingState.locallight[i] = &s_WorldLights[i];
		memcpy( &s_WorldLights[i], pLightingState->locallight[i], sizeof(dworldlight_t) );

		// Light already exists? Attenuate to it...
		VectorSubtract( pLightingState->locallight[i]->intensity, pLight[i], vecDelta );
		vecDelta *= flAttenFactor;

		s_WorldLights[i].intensity = pLightingState->locallight[i]->intensity - vecDelta;
		pSourceLight[i] = pLightingState->locallight[i];
	}

	// Ramp down any light we can; we may not be able to ramp them all down
	int nCurrLight = pLightingState->numlights;
	for ( i = 0; i < instanceLightingState.m_CurrentLightingState.numlights; ++i )
	{
		if ( pMatch[i] )
			continue;

		// Has it faded out to black? Then remove it.
		if ( instanceLightingState.m_flLightIntensity[i].LengthSqr() < 1 )
			continue;

		if ( nCurrLight >= MAXLOCALLIGHTS )
			break;

		actualLightingState.locallight[nCurrLight] = &s_WorldLights[nCurrLight];
		memcpy( &s_WorldLights[nCurrLight], instanceLightingState.m_CurrentLightingState.locallight[i], sizeof(dworldlight_t) );

		// Attenuate to black (fade out)
		VectorMultiply( instanceLightingState.m_flLightIntensity[i], flAttenFactor, vecDelta );

		s_WorldLights[nCurrLight].intensity = vecDelta;
		pSourceLight[nCurrLight] = instanceLightingState.m_CurrentLightingState.locallight[i];

		if (( nCurrLight >= nWorldLights ) && pLightingOrigin)
		{
			AddWorldLightToAmbientCube( &s_WorldLights[nCurrLight], *pLightingOrigin, actualLightingState.r_boxcolor, true ); 
		}

		++nCurrLight;
	}

	actualLightingState.numlights = MIN( nCurrLight, nWorldLights );
	instanceLightingState.m_CurrentLightingState.numlights = nCurrLight;

	for ( i = 0; i < nCurrLight; ++i )
	{
		instanceLightingState.m_CurrentLightingState.locallight[i] = pSourceLight[i];
		instanceLightingState.m_flLightIntensity[i] = s_WorldLights[i].intensity;

#if defined( VISUALIZE_TIME_AVERAGE ) && !defined( DEDICATED )
		Vector vecColor = pSourceLight[i]->intensity;
		float flMax = max( vecColor.x, vecColor.y );
		flMax = max( flMax, vecColor.z );
		if ( flMax == 0.0f )
		{
			flMax = 1.0f;
		}
		vecColor *= 255.0f / flMax;
		float flRatio = instanceLightingState.m_flLightIntensity[i].Length() / pSourceLight[i]->intensity.Length();
		vecColor *= flRatio;
		CDebugOverlay::AddLineOverlay( *pLightingOrigin, pSourceLight[i]->origin, 
			vecColor.x, vecColor.y, vecColor.z, 255, false, 5.0f );
#endif
	}

	return &actualLightingState;
#else
	return pLightingState;
#endif
}

// Ambient boost settings
static ConVar r_ambientboost( "r_ambientboost", "1", 0, "Set to boost ambient term if it is totally swamped by local lights" );
static ConVar r_ambientmin( "r_ambientmin", "0.3", 0, "Threshold above which ambient cube will not boost (i.e. it's already sufficiently bright" );
static ConVar r_ambientfraction( "r_ambientfraction", "0.2", FCVAR_CHEAT, "Fraction of direct lighting used to boost lighting when model requests" );
static ConVar r_ambientfactor( "r_ambientfactor", "5", 0, "Boost ambient cube by no more than this factor" );
static ConVar r_lightcachemodel ( "r_lightcachemodel", "-1", FCVAR_CHEAT, "" );
static ConVar r_drawlightcache ("r_drawlightcache", "0", FCVAR_CHEAT, "0: off\n1: draw light cache entries\n2: draw rays\n");

static ConVar r_modelAmbientMin( "r_modelAmbientMin", "0.0", FCVAR_CHEAT, "Minimum value for the ambient lighting on dynamic models with more than one bone (like players and their guns)." );

//-----------------------------------------------------------------------------
// Sets up lighting state for rendering
//-----------------------------------------------------------------------------
void CModelRender::StudioSetupLighting( const DrawModelState_t &state, const Vector& absEntCenter, 
	LightCacheHandle_t* pLightcache, bool bVertexLit, bool bNeedsEnvCubemap, bool &bStaticLighting, 
	DrawModelInfo_t &drawInfo, const ModelRenderInfo_t &pInfo, int drawFlags )
{
	VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "CModelRender::StudioSetupLighting");
	if ( m_bSuppressEngineLighting )
		return;

#ifndef DEDICATED
	ITexture *pEnvCubemapTexture = NULL;
	LightingState_t lightingState;

	Vector pSaveLightPos[MAXLOCALLIGHTS];

	Vector *pDebugLightingOrigin = NULL;
	Vector vecDebugLightingOrigin = vec3_origin;

	// Cache off lighting data for rendering decals - only on dx8/dx9.
	LightingState_t lightingDecalState;

	drawInfo.m_bStaticLighting = bStaticLighting;
	drawInfo.m_LightingState.m_nLocalLightCount = 0;

	// Compute lighting origin from input
	Vector vLightingOrigin( 0.0f, 0.0f, 0.0f );
	CMatRenderContextPtr pRenderContext( materials );
	if ( pInfo.pLightingOrigin )
	{
		vLightingOrigin = *pInfo.pLightingOrigin;
	}
	else
	{
		vLightingOrigin = absEntCenter;
		if ( pInfo.pLightingOffset )
		{
			VectorTransform( absEntCenter, *pInfo.pLightingOffset, vLightingOrigin );
		}
	}

	// Set the lighting origin state
	pRenderContext->SetLightingOrigin( vLightingOrigin );

	ModelInstance_t *pModelInst = NULL;
	bool bHasDecals = false;
	if ( pInfo.instance != m_ModelInstances.InvalidIndex() )
	{
		pModelInst = &m_ModelInstances[pInfo.instance];
		if ( pModelInst )
		{
			bHasDecals = ( pModelInst->m_DecalHandle != STUDIORENDER_DECAL_INVALID );
		}
	}

	if ( pLightcache )
	{
		VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "StudioSetupLighting (pLightcache)");
		// static prop case.
		if ( bStaticLighting )
		{
			LightingState_t *pLightingState = NULL;

			// dx8 and dx9 case. . .hardware can do baked lighting plus other dynamic lighting
			// We already have the static part baked into a color mesh, so just get the dynamic stuff.
			const model_t* pModel = pModelInst->m_pModel;
			if ( pModel->flags & MODELFLAG_STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY )
			{
				pLightingState = LightcacheGetStatic( *pLightcache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
			}
			else
			{
				pLightingState = LightcacheGetStatic( *pLightcache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
			}
			Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );

			lightingState = *pLightingState;
		}

		if ( !bStaticLighting )
		{
			lightingState = *(LightcacheGetStatic( *pLightcache, &pEnvCubemapTexture ));
			Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
		}

		if ( r_decalstaticprops.GetBool() && pModelInst && drawInfo.m_bStaticLighting && bHasDecals )
		{
			for ( int iCube = 0; iCube < 6; ++iCube )
			{
				drawInfo.m_LightingState.m_vecAmbientCube[iCube] = pModelInst->m_pLightingState->m_AmbientLightingState.r_boxcolor[iCube] + lightingState.r_boxcolor[iCube];
			}

			lightingDecalState.CopyLocalLights( pModelInst->m_pLightingState->m_AmbientLightingState );
			lightingDecalState.AddAllLocalLights( lightingState, vLightingOrigin );

			Assert( lightingDecalState.numlights >= 0 && lightingDecalState.numlights <= MAXLOCALLIGHTS );
		}
	}
	else	// !pLightcache
	{
		VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "StudioSetupLighting (not pLightcache)");
		vecDebugLightingOrigin = vLightingOrigin;
		pDebugLightingOrigin = &vecDebugLightingOrigin;

		// If we don't have a lightcache entry, but we have bStaticLighting, that means
		// that we are a prop_physics that has fallen asleep.
		if ( bStaticLighting )
		{
			LightcacheGetDynamic_Stats stats;
			pEnvCubemapTexture = LightcacheGetDynamic( vLightingOrigin, lightingState, 
				stats, state.m_pRenderable, LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
			Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
		}

		if ( !bStaticLighting )
		{
			LightcacheGetDynamic_Stats stats;

			// For special r_drawlightcache mode, we only draw models containing the substring set in r_lightcachemodel
			bool bDebugModel = false;
			if ( r_drawlightcache.GetInt() == 5 )
			{
				if ( pModelInst && pModelInst->m_pModel && pModelInst->m_pModel->szPathName )
				{
					const char *szModelName = r_lightcachemodel.GetString();
					bDebugModel = V_stristr( pModelInst->m_pModel->szPathName, szModelName ) != NULL;
				}
			}
	
			pEnvCubemapTexture = LightcacheGetDynamic( vLightingOrigin, lightingState, stats, state.m_pRenderable,
				LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST, bDebugModel );

			Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
		}
		
		if ( pInfo.pLightingOffset && !pInfo.pLightingOrigin )
		{
			for ( int i = 0; i < lightingState.numlights; ++i )
			{
				pSaveLightPos[i] = lightingState.locallight[i]->origin; 
				VectorITransform( pSaveLightPos[i], *pInfo.pLightingOffset, lightingState.locallight[i]->origin );
			}
		}

		// Cache lighting for decals.
		if ( pModelInst && drawInfo.m_bStaticLighting && bHasDecals )
		{
			// Only do this on dx8/dx9.
			LightcacheGetDynamic_Stats stats;
			LightcacheGetDynamic( vLightingOrigin, lightingDecalState, stats, state.m_pRenderable,
				LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST );

			Assert( lightingDecalState.numlights >= 0 && lightingDecalState.numlights <= MAXLOCALLIGHTS);
			
			for ( int iCube = 0; iCube < 6; ++iCube )
			{
				VectorCopy( lightingDecalState.r_boxcolor[iCube], drawInfo.m_LightingState.m_vecAmbientCube[iCube] );
			}

			if ( pInfo.pLightingOffset && !pInfo.pLightingOrigin )
			{
				for ( int i = 0; i < lightingDecalState.numlights; ++i )
				{
					pSaveLightPos[i] = lightingDecalState.locallight[i]->origin; 
					VectorITransform( pSaveLightPos[i], *pInfo.pLightingOffset, lightingDecalState.locallight[i]->origin );
				}
			}
		}
	}

	Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
	
	// Do time averaging of the lighting state to avoid popping...
	LightingState_t *pState;
	if ( !bStaticLighting && !pLightcache )
	{
		pState = TimeAverageLightingState( pInfo.instance, &lightingState, pInfo.entity_index, pDebugLightingOrigin );
	}
	else
	{
		pState = &lightingState;
	}

	if ( bNeedsEnvCubemap && pEnvCubemapTexture )
	{
		pRenderContext->BindLocalCubemap( pEnvCubemapTexture );
	}

	if ( g_pMaterialSystemConfig->nFullbright == 1 )
	{
		static Vector white[6] = 
		{
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
			Vector( 1.0, 1.0, 1.0 ),
		};

		g_pStudioRender->SetAmbientLightColors( white );

		// Disable all the lights..
		pRenderContext->DisableAllLocalLights();
	}
	else if ( bVertexLit )
	{
		if( drawFlags & STUDIORENDER_DRAW_ITEM_BLINK )
		{
			float add = r_itemblinkmax.GetFloat() * ( FastCos( r_itemblinkrate.GetFloat() * Sys_FloatTime() ) + 1.0f );
			Vector additiveColor( add, add, add );
			static Vector temp[6];
			int i;
			for( i = 0; i < 6; i++ )
			{
				temp[i][0] = MIN( 1.0f, pState->r_boxcolor[i][0] + additiveColor[0] );
				temp[i][1] = MIN( 1.0f, pState->r_boxcolor[i][1] + additiveColor[1] );
				temp[i][2] = MIN( 1.0f, pState->r_boxcolor[i][2] + additiveColor[2] );
			}
			g_pStudioRender->SetAmbientLightColors( temp );
		}
		else
		{
			// If we have any lights and want to do ambient boost on this model
			if ( (pState->numlights > 0) && (pInfo.pModel->flags & MODELFLAG_STUDIOHDR_AMBIENT_BOOST) && r_ambientboost.GetBool() )
			{
				Vector lumCoeff( 0.3f, 0.59f, 0.11f );
				float avgCubeLuminance = 0.0f;
				float minCubeLuminance = FLT_MAX;
				float maxCubeLuminance = 0.0f;

				// Compute average luminance of ambient cube
				for( int i = 0; i < 6; i++ )
				{
					float luminance = DotProduct( pState->r_boxcolor[i], lumCoeff );	// compute luminance
					minCubeLuminance = fpmin(minCubeLuminance, luminance);				// min luminance
					maxCubeLuminance = fpmax(maxCubeLuminance, luminance);				// max luminance
					avgCubeLuminance += luminance;										// accumulate luminance
				}
				avgCubeLuminance /= 6.0f;												// average luminance

				// Compute the amount of direct light reaching the center of the model (attenuated by distance)
				float fDirectLight = 0.0f;
				for( int i = 0; i < pState->numlights; i++ )
				{
					Vector vLight = pState->locallight[i]->origin - vLightingOrigin;
					float d2 = DotProduct( vLight, vLight );
					float d = sqrtf( d2 );
					float fAtten = 1.0f;

					float denom = pState->locallight[i]->constant_attn +
								pState->locallight[i]->linear_attn * d +
								pState->locallight[i]->quadratic_attn * d2;

					if ( denom > 0.00001f )
					{
						fAtten = 1.0f / denom;
					}

					Vector vLit = pState->locallight[i]->intensity * fAtten;
					fDirectLight += DotProduct( vLit, lumCoeff );
				}

				// If ambient cube is sufficiently dim in absolute terms and ambient cube is swamped by direct lights
				if ( avgCubeLuminance < r_ambientmin.GetFloat() && (avgCubeLuminance < (fDirectLight * r_ambientfraction.GetFloat())) )	
				{
					Vector vFinalAmbientCube[6];
					float fBoostFactor =  MIN( (fDirectLight * r_ambientfraction.GetFloat()) / maxCubeLuminance, r_ambientfactor.GetFloat() ); // boost no more than a certain factor
					for( int i = 0; i < 6; i++ )
					{
						vFinalAmbientCube[i] = pState->r_boxcolor[i] * fBoostFactor;
					}
					g_pStudioRender->SetAmbientLightColors( vFinalAmbientCube );		// Boost
				}
				else
				{
					g_pStudioRender->SetAmbientLightColors( pState->r_boxcolor );		// No Boost
				}
			}
			else if ( state.m_pStudioHdr && state.m_pStudioHdr->numbones > 1 && r_modelAmbientMin.GetFloat() > 0.0f )
			{
				//We check the number of bones to make sure this is a player model (or the gun).
				float minAmbient = r_modelAmbientMin.GetFloat();
				Vector vFinalAmbientCube[6];
				float *result = (float *) vFinalAmbientCube;
				float *src = (float *) pState->r_boxcolor;
				
				AssertMsg( sizeof(Vector) == sizeof(float) * 3, "The size of the Vector structure has changed.  You must update this function." );

				for( int i = 0; i < 6 * 3; i++ )
				{
					if ( src[i] < minAmbient )
					{
						result[i] = minAmbient;
					}
					else
					{
						result[i] = src[i];
					}
				}

				g_pStudioRender->SetAmbientLightColors( vFinalAmbientCube );
			}
			else // Don't bother with ambient boost, just use the ambient cube as is
			{
				g_pStudioRender->SetAmbientLightColors( pState->r_boxcolor );			// No Boost
			}
		}

		R_SetNonAmbientLightingState( pState->numlights, pState->locallight,
			                          &drawInfo.m_LightingState.m_nLocalLightCount, drawInfo.m_LightingState.m_pLocalLightDesc, true );

		// Cache lighting for decals.
		if( pModelInst && drawInfo.m_bStaticLighting && bHasDecals )
		{
			R_SetNonAmbientLightingState( lightingDecalState.numlights, lightingDecalState.locallight,
				                          &drawInfo.m_LightingState.m_nLocalLightCount, drawInfo.m_LightingState.m_pLocalLightDesc, false );
		}
	}

	if ( pInfo.pLightingOffset && !pInfo.pLightingOrigin )
	{
		for ( int i = 0; i < lightingState.numlights; ++i )
		{
			lightingState.locallight[i]->origin = pSaveLightPos[i];
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Converts lighting state
//-----------------------------------------------------------------------------
void CModelRender::EngineLightingToMaterialLighting( MaterialLightingState_t *pLightingState, const Vector &vecLightingOrigin, const LightingState_t &srcLightingState )
{
	memcpy( pLightingState->m_vecAmbientCube, srcLightingState.r_boxcolor, sizeof(srcLightingState.r_boxcolor) );
	pLightingState->m_vecLightingOrigin = vecLightingOrigin;

	int nLightCount = 0;
	for ( int i = 0; i < srcLightingState.numlights; ++i )
	{
		LightDesc_t *pLightDesc = &( pLightingState->m_pLocalLightDesc[nLightCount] );
		if ( !WorldLightToMaterialLight( srcLightingState.locallight[i], *pLightDesc ) )
			continue;

		// Apply lightstyle
		if ( LightStyleIsModified(srcLightingState.locallight[i]->style) )
		{
			// Deal with overbrighting + bias
			float bias = LightStyleValue( srcLightingState.locallight[i]->style );
			pLightDesc->m_Color[0] *= bias;
			pLightDesc->m_Color[1] *= bias;
			pLightDesc->m_Color[2] *= bias;
		}

		if ( ++nLightCount >= MATERIAL_MAX_LIGHT_COUNT )
			break;
	}
	pLightingState->m_nLocalLightCount = nLightCount;
}


//-----------------------------------------------------------------------------
// FIXME: a duplicate of what's in CEngineTool::GetLightingConditions
//-----------------------------------------------------------------------------
int CModelRender::GetLightingConditions( const Vector &vecLightingOrigin, Vector *pColors, int nMaxLocalLights, LightDesc_t *pLocalLights,
										 ITexture *&pEnvCubemapTexture, ModelInstanceHandle_t handle, bool bAllowFast )
{
	int nLightCount = 0;
#ifndef DEDICATED
	LightcacheGetDynamic_Stats stats;
	LightingState_t state;
	pEnvCubemapTexture = NULL;
	const IClientRenderable* pRenderable = NULL;
	if ( handle != MODEL_INSTANCE_INVALID )
	{
		pRenderable = m_ModelInstances[ handle ].m_pRenderable;

		if ( IsX360() || IsPS3() )
		{
			COMPILE_TIME_ASSERT( ALIGN_VALUE( sizeof( ModelInstanceLightingState_t ), 128 ) == 256 || !( IsX360() || IsPS3() ) );
			PREFETCH360( m_ModelInstances[handle].m_pLightingState, 0 );
			PREFETCH360( m_ModelInstances[handle].m_pLightingState, 128 );
		}
	}

	int nFlags = LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE; 
	if ( bAllowFast )
	{
		nFlags |= LIGHTCACHEFLAGS_ALLOWFAST;
	}
	pEnvCubemapTexture = LightcacheGetDynamic( vecLightingOrigin, state, stats, pRenderable, nFlags );
	Assert( state.numlights >= 0 && state.numlights <= MAXLOCALLIGHTS );

	LightingState_t *pState = &state;
	if( handle != MODEL_INSTANCE_INVALID )
	{
		pState = TimeAverageLightingState( handle, &state, 0, &vecLightingOrigin );
	}

	memcpy( pColors, pState->r_boxcolor, sizeof(pState->r_boxcolor) );

	for ( int i = 0; i < pState->numlights; ++i )
	{
		LightDesc_t *pLightDesc = &pLocalLights[nLightCount];
		if ( !WorldLightToMaterialLight( pState->locallight[i], *pLightDesc ) )
			continue;

		// Apply lightstyle
		if ( LightStyleIsModified(pState->locallight[i]->style) )
		{
			// Deal with overbrighting + bias
			float bias = LightStyleValue( pState->locallight[i]->style );
			pLightDesc->m_Color[0] *= bias;
			pLightDesc->m_Color[1] *= bias;
			pLightDesc->m_Color[2] *= bias;
		}

		if ( ++nLightCount >= nMaxLocalLights )
			break;
	}
#endif
	return nLightCount;
}

void CModelRender::SetupLighting( const Vector &vecCenter )
{
	SetupLightingEx( vecCenter, MODEL_INSTANCE_INVALID );
}

// FIXME: a duplicate of what's in CCDmeMdlRenderable<T>::SetUpLighting and CDmeEmitter::SetUpLighting
void CModelRender::SetupLightingEx( const Vector &vecCenter, ModelInstanceHandle_t handle )
{
#ifndef DEDICATED
	// Set up lighting conditions
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	MaterialLightingState_t state;
	ITexture *pEnvCubemapTexture = NULL;
	state.m_vecLightingOrigin = vecCenter;
	state.m_nLocalLightCount = GetLightingConditions( vecCenter, state.m_vecAmbientCube, MATERIAL_MAX_LIGHT_COUNT, state.m_pLocalLightDesc, pEnvCubemapTexture, handle );
	pRenderContext->SetLightingState( state );
	if ( pEnvCubemapTexture )
	{
		pRenderContext->BindLocalCubemap( pEnvCubemapTexture );
	}
#endif
}


//-----------------------------------------------------------------------------
// Fast-path method to compute lighting state
// Don't mess with this without being careful about regressing perf!
//-----------------------------------------------------------------------------
void CModelRender::SetFullbrightLightingState( int nCount, MaterialLightingState_t *pState )
{
	for ( int i = 0; i < nCount; ++i )
	{
		MaterialLightingState_t &state = pState[i];
		state.m_nLocalLightCount = 0;
		state.m_vecAmbientCube[0].Init( 1.0f, 1.0f, 1.0f );
		state.m_vecAmbientCube[1].Init( 1.0f, 1.0f, 1.0f );
		state.m_vecAmbientCube[2].Init( 1.0f, 1.0f, 1.0f );
		state.m_vecAmbientCube[3].Init( 1.0f, 1.0f, 1.0f );
		state.m_vecAmbientCube[4].Init( 1.0f, 1.0f, 1.0f );
		state.m_vecAmbientCube[5].Init( 1.0f, 1.0f, 1.0f );
	}
}

void CModelRender::ComputeAmbientBoost( int nCount, const LightingQuery_t *pQuery, MaterialLightingState_t *pState )
{
	// If we have any lights and want to do ambient boost on this model
	if ( !r_ambientboost.GetBool() )
		return;

	Vector lumCoeff( 0.3f, 0.59f, 0.11f );
	float avgCubeLuminance = 0.0f;
	float minCubeLuminance = FLT_MAX;
	float maxCubeLuminance = 0.0f;

	for ( int i = 0; i < nCount; ++i )
	{
		const LightingQuery_t &query = pQuery[i];
		MaterialLightingState_t &state = pState[i];
		if ( !query.m_bAmbientBoost || ( state.m_nLocalLightCount == 0 ) )
			continue;

		// Compute average luminance of ambient cube
		for( int i = 0; i < 6; i++ )
		{
			float luminance = DotProduct( state.m_vecAmbientCube[i], lumCoeff );// compute luminance
			minCubeLuminance = fpmin(minCubeLuminance, luminance);				// min luminance
			maxCubeLuminance = fpmax(maxCubeLuminance, luminance);				// max luminance
			avgCubeLuminance += luminance;										// accumulate luminance
		}
		avgCubeLuminance /= 6.0f;												// average luminance

		// Compute the amount of direct light reaching the center of the model (attenuated by distance)
		float fDirectLight = 0.0f;
		for( int i = 0; i < state.m_nLocalLightCount; i++ )
		{
			Vector vLight = state.m_pLocalLightDesc[i].m_Position - state.m_vecLightingOrigin;
			float d2 = DotProduct( vLight, vLight );
			float d = sqrtf( d2 );
			float fAtten = 1.0f;

			float denom = state.m_pLocalLightDesc[i].m_Attenuation0 +
				state.m_pLocalLightDesc[i].m_Attenuation1 * d +
				state.m_pLocalLightDesc[i].m_Attenuation2 * d2;

			if ( denom > 0.00001f )
			{
				fAtten = 1.0f / denom;
			}

			Vector vLit = state.m_pLocalLightDesc[i].m_Color * fAtten;
			fDirectLight += DotProduct( vLit, lumCoeff );
		}
		fDirectLight *= r_ambientfraction.GetFloat();

		// If ambient cube is sufficiently dim in absolute terms and ambient cube is swamped by direct lights
		if ( ( avgCubeLuminance < r_ambientmin.GetFloat() ) && ( avgCubeLuminance < fDirectLight ) )	
		{
			float fBoostFactor =  MIN( fDirectLight / maxCubeLuminance, r_ambientfactor.GetFloat() ); // boost no more than a certain factor
			for( int i = 0; i < 6; i++ )
			{
				state.m_vecAmbientCube[i] *= fBoostFactor;
			}
		}
	}
}

void CModelRender::ComputeLightingState( int nCount, const LightingQuery_t *pQuery, MaterialLightingState_t *pState, ITexture **ppEnvCubemapTexture )
{
	for ( int i = 0; i < nCount; ++i )
	{
		const LightingQuery_t &query = pQuery[i];
		MaterialLightingState_t &state = pState[i];

		state.m_nLocalLightCount = GetLightingConditions( query.m_LightingOrigin, 
			state.m_vecAmbientCube, ARRAYSIZE( state.m_pLocalLightDesc ), state.m_pLocalLightDesc,
			ppEnvCubemapTexture[i], query.m_InstanceHandle, true );
		state.m_vecLightingOrigin = query.m_LightingOrigin;
	}

	ComputeAmbientBoost( nCount, pQuery, pState );

	if ( mat_fullbright.GetInt() == 1 )
	{
		SetFullbrightLightingState( nCount, pState );
	}
}


//-----------------------------------------------------------------------------
// Computes lighting state for static props
//-----------------------------------------------------------------------------
void CModelRender::CleanupStaticLightingState( int nCount, DataCacheHandle_t *pColorMeshHandles )
{
	CMatRenderContextPtr pRenderContext( materials );
	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
	if ( !pCallQueue )
	{
		for ( int i = 0; i < nCount; ++i )
		{
			if ( pColorMeshHandles[i] != DC_INVALID_HANDLE )
			{
				CacheUnlock( pColorMeshHandles[i] );
			}
		}
		return;
	}

	CMatRenderData< DataCacheHandle_t > renderData( pRenderContext, nCount, pColorMeshHandles );
	pCallQueue->QueueCall( this, &CModelRender::UnlockCacheCacheHandleArray, nCount, renderData.Base() );
}

//-----------------------------------------------------------------------------
// Computes lighting state for static props
//-----------------------------------------------------------------------------
void CModelRender::ComputeStaticLightingState( int nCount, const StaticLightingQuery_t *pQuery, 
	MaterialLightingState_t *pLightingState, MaterialLightingState_t *pDecalState, 
	ColorMeshInfo_t **ppStaticLighting, ITexture **ppEnvCubemapTexture, DataCacheHandle_t *pColorMeshHandles )
{
#ifndef DEDICATED
	// Deal with fullbright case
	if ( mat_fullbright.GetInt() == 1 )
	{
		SetFullbrightLightingState( nCount, pLightingState );
		SetFullbrightLightingState( nCount, pDecalState );
		memset( pColorMeshHandles, 0, nCount * sizeof(DataCacheHandle_t) );
		for ( int i = 0; i < nCount; ++i )
		{
			pLightingState[i].m_vecLightingOrigin = pQuery[i].m_LightingOrigin;
			pDecalState[i].m_vecLightingOrigin = pQuery[i].m_LightingOrigin;
			ppStaticLighting[i] = NULL;
	
			// Get the env_cubemap
			LightCacheHandle_t* pLightCache = NULL;
			if ( pQuery[i].m_InstanceHandle != MODEL_INSTANCE_INVALID )
			{
				ModelInstance_t *pInstance = &m_ModelInstances[ pQuery[i].m_InstanceHandle ];
				if ( ( pInstance->m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING ) && pInstance->m_LightCacheHandle )
				{
					pLightCache = &pInstance->m_LightCacheHandle;
				}
			}

			if ( pLightCache )
			{
				LightcacheGetStatic( *pLightCache, &ppEnvCubemapTexture[i], LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
			}
			else
			{
				LightingState_t lightingState;
				LightcacheGetDynamic_Stats stats;
				ppEnvCubemapTexture[i] = LightcacheGetDynamic( pQuery[i].m_LightingOrigin, lightingState, 
					stats, pQuery[i].m_pRenderable, (LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST), false );
			}
		}
		return;
	}

	// Collate all color mesh handles so we can lock all of them at once to reduce datacache overhead
	CColorMeshData **ppColorMeshData = ( CColorMeshData** )stackalloc( nCount * sizeof( CColorMeshData* ) );
	for ( int i = 0; i < nCount; ++i )
	{
		ModelInstanceHandle_t hInstance = pQuery[i].m_InstanceHandle;
		IClientRenderable *pRenderable = pQuery[i].m_pRenderable;
		const model_t* pModel = pRenderable->GetModel();
		bool bStaticLighting = ( hInstance != MODEL_INSTANCE_INVALID ) && modelinfo->UsesStaticLighting( pModel );
		pColorMeshHandles[i] = bStaticLighting ? m_ModelInstances[ hInstance ].m_ColorMeshHandle : DC_INVALID_HANDLE;
	}
	CacheGetAndLockMultiple( ppColorMeshData, nCount, pColorMeshHandles );

	for ( int i = 0; i < nCount; ++i )
	{
		ModelInstanceHandle_t hInstance = pQuery[i].m_InstanceHandle;
		IClientRenderable *pRenderable = pQuery[i].m_pRenderable;
		bool bHasDecals = ( hInstance != MODEL_INSTANCE_INVALID ) && ( m_ModelInstances[ hInstance ].m_DecalHandle != STUDIORENDER_DECAL_INVALID ) ? true : false;

		// get the static lighting from the cache
		bool bStaticLighting = false;
		ppStaticLighting[i] = NULL;
		if ( pColorMeshHandles[i] != DC_INVALID_HANDLE )
		{
			bStaticLighting = true;

			// have static lighting, get from cache
			if ( !ppColorMeshData[i] || ppColorMeshData[i]->m_bNeedsRetry )
			{
				// color meshes are not present, try to re-establish
				if ( UpdateStaticPropColorData( pRenderable->GetIClientUnknown(), hInstance ) )
				{
					ppColorMeshData[i] = CacheGet( pColorMeshHandles[i] );

					// CacheCreate above will call functions that won't take place until later.
					// If color mesh isn't used right away, it could get dumped
					if ( !CacheLock( pColorMeshHandles[i] ) ) 
					{
						// No lock occured, ensure the handle is invalid, this prevents an unpaired unlock
						// from occuring in CleanupStaticLightingState
						pColorMeshHandles[i] = DC_INVALID_HANDLE;
						ppColorMeshData[i] = NULL;
					}
				}
				else if ( !ppColorMeshData[i] || !ppColorMeshData[i]->m_bNeedsRetry )
				{
					// Prevent asymmetric unlock
					if ( !ppColorMeshData[i] )
					{
						pColorMeshHandles[i] = DC_INVALID_HANDLE;
					}

					// failed, draw without static lighting
					ppColorMeshData[i] = NULL;
				}
			}

			if ( ppColorMeshData[i] && ppColorMeshData[i]->m_bColorMeshValid )
			{
				ppStaticLighting[i] = ppColorMeshData[i]->m_pMeshInfos;

			}
			else
			{
				// failed, draw without static lighting
				bStaticLighting = false;
			}
		}

		// See if we're using static lighting
		LightCacheHandle_t* pLightCache = NULL;
		ITexture *pEnvCubemapTexture = NULL;
		ModelInstance_t *pInstance = NULL;
		if ( hInstance != MODEL_INSTANCE_INVALID )
		{
			pInstance = &m_ModelInstances[hInstance];
			if ( ( pInstance->m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING ) && pInstance->m_LightCacheHandle )
			{
				pLightCache = &pInstance->m_LightCacheHandle;
			}
		}

		LightingState_t lightingState, decalLightingState;
		LightingState_t *pState = &lightingState;
		LightingState_t *pDecalLightState = &decalLightingState;
		if ( pLightCache )
		{
			// dx8 and dx9 case. . .hardware can do baked lighting plus other dynamic lighting
			// We already have the static part baked into a color mesh, so just get the dynamic stuff.
			if ( bStaticLighting )
			{
				const model_t* pModel = pRenderable->GetModel();
				if ( pModel->flags & MODELFLAG_STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY )
				{
					pState = LightcacheGetStatic( *pLightCache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
				}
				else
				{
					pState = LightcacheGetStatic( *pLightCache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
				}
				Assert( pState->numlights >= 0 && pState->numlights <= MAXLOCALLIGHTS );
			}
			else
			{
				pState = LightcacheGetStatic( *pLightCache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
				Assert( pState->numlights >= 0 && pState->numlights <= MAXLOCALLIGHTS );
			}

			if ( bHasDecals )
			{
				// FIXME: Shitty. Should work directly in terms of MaterialLightingState_t.
				// We should get rid of LightingState_t altogether.
				for ( int iCube = 0; iCube < 6; ++iCube )
				{
					pDecalLightState->r_boxcolor[iCube] = pInstance->m_pLightingState->m_AmbientLightingState.r_boxcolor[iCube] + pState->r_boxcolor[iCube];
				}
				pDecalLightState->CopyLocalLights( pInstance->m_pLightingState->m_AmbientLightingState );
				pDecalLightState->AddAllLocalLights( *pState, pQuery[i].m_LightingOrigin );
			}
			else
			{
				pDecalLightState = pState;
			}
		}
		else	// !pLightcache
		{
			// UNDONE: is it possible to end up here in the static prop case?
			Vector vLightingOrigin = pQuery[i].m_LightingOrigin;
			int lightCacheFlags = ( bStaticLighting && ( !( pRenderable->GetModel()->flags & MODELFLAG_STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY ) ) ) ? ( LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE )
				: (LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE | LIGHTCACHEFLAGS_ALLOWFAST);
			LightcacheGetDynamic_Stats stats;
			pEnvCubemapTexture = LightcacheGetDynamic( vLightingOrigin, lightingState, 
				stats, pRenderable, lightCacheFlags, false );
			Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
			pState = &lightingState;

			if ( bHasDecals )
			{
				LightcacheGetDynamic_Stats stats;
				LightcacheGetDynamic( vLightingOrigin, *pDecalLightState, stats, pRenderable, 
					LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST );
			}
			else
			{
				pDecalLightState = pState;
			}
		}

		EngineLightingToMaterialLighting( &pLightingState[i], pQuery[i].m_LightingOrigin, *pState );
		if ( bHasDecals )
		{
			EngineLightingToMaterialLighting( &pDecalState[i], pQuery[i].m_LightingOrigin, *pDecalLightState );
		}
		ppEnvCubemapTexture[i] = pEnvCubemapTexture;
	}
#endif
}


//-----------------------------------------------------------------------------
// Fast-path method to get decal data
// Don't mess with this without being careful about regressing perf!
//-----------------------------------------------------------------------------
void CModelRender::GetModelDecalHandles( StudioDecalHandle_t *pDecals, int nDecalStride, int nCount, const ModelInstanceHandle_t *pHandles )
{
	for ( int i = 0; i < nCount; ++i, pDecals = (StudioDecalHandle_t*)( (char*)pDecals + nDecalStride ) )
	{
		const ModelInstanceHandle_t h = pHandles[i];
		*pDecals = ( h != MODEL_INSTANCE_INVALID ) ? 
			m_ModelInstances[ h ].m_DecalHandle : STUDIORENDER_DECAL_INVALID;
	}
}

bool CModelRender::GetBrightestShadowingLightSource( const Vector &vecCenter, Vector& lightPos, Vector& lightBrightness, bool bAllowNonTaggedLights )
{
#ifndef DEDICATED
	LightcacheGetDynamic_Stats stats;
	LightingState_t state;

	// FIXME: Workaround for local lightsource shadows bouncing on the 360. For some reason passing in a slightly different vecCenter causes the
	// lightcache lookup to return different leaves from CM_PointLeafnum(). In turn, one of these leaves has an empty vis set so that no lights
	// are found touching that leaf, leading to an empty lightcache entry. This causes a local light source shadow to pick the global shadow direction.
	Vector vc( vecCenter );
	vc.x = floor( 100.0f * vc.x + 0.5f ) / 100.0f;
	vc.y = floor( 100.0f * vc.y + 0.5f ) / 100.0f;
	vc.z = floor( 100.0f * vc.z + 0.5f ) / 100.0f;

	LightcacheGetDynamic( vc, state, stats, NULL, LIGHTCACHEFLAGS_STATIC );	// static light only for now
	Assert( state.numlights >= 0 && state.numlights <= MAXLOCALLIGHTS );

	float fMaxBrightness = 0.0f;
	float fLightFalloff = 0.0f;
	int nLightIdx = -1;

	static Vector colorToGray( 0.3f, 0.59f, 0.11f );

	for ( int i = 0; i < state.numlights; ++i )
	{
		if ( ( state.locallight[i]->flags & DWL_FLAGS_CASTENTITYSHADOWS ) || bAllowNonTaggedLights )
		{
			Vector lightOrigin = state.locallight[i]->origin + state.locallight[i]->shadow_cast_offset;
			if ( lightOrigin.z < vecCenter.z )
			{
				// don't cast shadows from below 'cause it looks stupid with an orthographic projection
				continue;
			}

			float fBrightness = DotProduct( state.locallight[i]->intensity, colorToGray );
			if ( fBrightness <= fMaxBrightness )
			{
				continue;
			}

			// Apply lightstyle?
			//float scale = LightStyleValue( pState->locallight[i]->style );

			// use the unmodified light origin to compute light brightness
			Vector delta( state.locallight[i]->origin - vecCenter );
			float fFalloff = Engine_WorldLightDistanceFalloff( state.locallight[i], delta, false );
			if ( fBrightness*fFalloff <= fMaxBrightness )
			{
				continue;
			}

			delta.NormalizeInPlace();
			fFalloff *= Engine_WorldLightAngle( state.locallight[i], state.locallight[i]->normal, delta, delta );
			if ( fBrightness*fFalloff > fMaxBrightness )
			{
				nLightIdx = i;
				fMaxBrightness = fBrightness * fFalloff;
				fLightFalloff = fFalloff;
			}
		}
	}

	if ( nLightIdx > -1 )
	{
		lightPos = state.locallight[nLightIdx]->origin + state.locallight[nLightIdx]->shadow_cast_offset;
		lightBrightness = fLightFalloff * state.locallight[nLightIdx]->intensity;
		return true;
	}
#endif // DEDICATED
	return false;
}

//-----------------------------------------------------------------------------
// Uses this material instead of the one the model was compiled with
//-----------------------------------------------------------------------------
void CModelRender::ForcedMaterialOverride( IMaterial *newMaterial, OverrideType_t nOverrideType, int nMaterialIndex )
{
	g_pStudioRender->ForcedMaterialOverride( newMaterial, nOverrideType, nMaterialIndex );
}

bool CModelRender::IsForcedMaterialOverride()
{
	return g_pStudioRender->IsForcedMaterialOverride();
}

struct ModelDebugOverlayData_t
{
	DrawModelInfo_t m_ModelInfo;
	DrawModelResults_t m_ModelResults;
	Vector m_Origin;

	ModelDebugOverlayData_t() {}

private:
	ModelDebugOverlayData_t( const ModelDebugOverlayData_t &vOther );
};

static CUtlVector<ModelDebugOverlayData_t> s_SavedModelInfo;

void DrawModelDebugOverlay( const DrawModelInfo_t& info, const DrawModelResults_t &results, const Vector &origin, float r = 1.0f, float g = 1.0f, float b = 1.0f )
{
#ifndef DEDICATED
	float alpha = 1;

	bool bHasFilter = V_stricmp( r_drawmodelstatsoverlayfilter.GetString(), "-1" ) != 0;

	// If the model is valid, has a valid name and our filter is interesting
	if ( bHasFilter && info.m_pStudioHdr && info.m_pStudioHdr->pszName() )
	{
		// Check the name against our filter convar and bail if this model doesn't match
		if ( !V_stristr( info.m_pStudioHdr->pszName(), r_drawmodelstatsoverlayfilter.GetString() ) )
		{
			return;
		}
	}

	// If we don't have a string filter, use distance filtering
	if ( !bHasFilter )
	{
		if( r_drawmodelstatsoverlaydistance.GetFloat() == 1 )
		{
			alpha = 1.f - clamp( CurrentViewOrigin().DistTo( origin ) / r_drawmodelstatsoverlaydistance.GetFloat(), 0, 1.f );
		}
		else
		{
			float flDistance = CurrentViewOrigin().DistTo( origin );

			// The view model keeps throwing up its data and it looks like garbage, so I am trying to get rid of it.
			if ( flDistance < 36.0f )
				return;

			if ( flDistance > r_drawmodelstatsoverlaydistance.GetFloat() )
				return;
		}
	}

	Assert( info.m_pStudioHdr );
	Assert( info.m_pStudioHdr->pszName() );
	Assert( info.m_pHardwareData );
	float duration = 0.0f;
	int lineOffset = 0;
	if( !info.m_pStudioHdr || !info.m_pStudioHdr->pszName() || !info.m_pHardwareData )
	{
		CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, 1.0f, 0.8f, 0.8f, 1.0f, "This model has problems! See a programmer!" );
		return;
	}

	char buf[1024];
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, info.m_pStudioHdr->pszName() );
	Q_snprintf( buf, sizeof( buf ), "lod: %d/%d\n", results.m_nLODUsed+1, ( int )info.m_pHardwareData->m_NumLODs );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );
	Q_snprintf( buf, sizeof( buf ), "tris: %d\n",  results.m_ActualTriCount );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );
	Q_snprintf( buf, sizeof( buf ), "hardware bones: %d\n",  results.m_NumHardwareBones );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		
	Q_snprintf( buf, sizeof( buf ), "num batches: %d\n",  results.m_NumBatches );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		
	Q_snprintf( buf, sizeof( buf ), "has shadow lod: %s\n", ( info.m_pStudioHdr->flags & STUDIOHDR_FLAGS_HASSHADOWLOD ) ? "true" : "false" );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		
	Q_snprintf( buf, sizeof( buf ), "num materials: %d\n", results.m_NumMaterials );
	CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		

	int i;
	for( i = 0; i < results.m_Materials.Count(); i++ )
	{
		IMaterial *pMaterial = results.m_Materials[i];
		if( pMaterial )
		{
			int numPasses = pMaterial->GetNumPasses();
			Q_snprintf( buf, sizeof( buf ), "\t%s (%d %s)\n", results.m_Materials[i]->GetName(), numPasses, numPasses > 1 ? "passes" : "pass" );
			CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );		
		}
	}
	if( results.m_Materials.Count() > results.m_NumMaterials )
	{
		CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, "(Remaining materials not shown)\n" );		
	}
	if( r_drawmodelstatsoverlay.GetInt() == 2 )
	{
		Q_snprintf( buf, sizeof( buf ), "Render Time: %0.1f ms\n", results.m_RenderTime.GetDuration().GetMillisecondsF());
		CDebugOverlay::AddTextOverlay( origin, lineOffset++, duration, r, g, b, alpha, buf );
	}

	//Q_snprintf( buf, sizeof( buf ), "Render Time: %0.1f ms\n", info.m_pClientEntity 
#endif
}

void AddModelDebugOverlay( const DrawModelInfo_t& info, const DrawModelResults_t &results, const Vector& origin )
{
	ModelDebugOverlayData_t &tmp = s_SavedModelInfo[s_SavedModelInfo.AddToTail()];
	tmp.m_ModelInfo = info;
	tmp.m_ModelResults = results;
	tmp.m_Origin = origin;
}

void ClearSaveModelDebugOverlays( void )
{
	s_SavedModelInfo.RemoveAll();
}

int SavedModelInfo_Compare_f( const void *l, const void *r )
{
	ModelDebugOverlayData_t *left = ( ModelDebugOverlayData_t * )l;
	ModelDebugOverlayData_t *right = ( ModelDebugOverlayData_t * )r;
	return left->m_ModelResults.m_RenderTime.GetDuration().GetSeconds() < right->m_ModelResults.m_RenderTime.GetDuration().GetSeconds();
}

static ConVar r_drawmodelstatsoverlaymin( "r_drawmodelstatsoverlaymin", "0.1", FCVAR_ARCHIVE, "time in milliseconds that a model must take to render before showing an overlay in r_drawmodelstatsoverlay 2" );
static ConVar r_drawmodelstatsoverlaymax( "r_drawmodelstatsoverlaymax", "1.5", FCVAR_ARCHIVE, "time in milliseconds beyond which a model overlay is fully red in r_drawmodelstatsoverlay 2" );

void DrawSavedModelDebugOverlays( void )
{
	if( s_SavedModelInfo.Count() == 0 )
	{
		return;
	}
	float min = r_drawmodelstatsoverlaymin.GetFloat();
	float max = r_drawmodelstatsoverlaymax.GetFloat();
	float ooRange = 1.0f / ( max - min );

	int i;
	for( i = 0; i < s_SavedModelInfo.Count(); i++ )
	{
		float r, g, b;
		float t = s_SavedModelInfo[i].m_ModelResults.m_RenderTime.GetDuration().GetMillisecondsF();
		if( t > min )
		{
			if( t >= max )
			{
				r = 1.0f; g = 0.0f; b = 0.0f;
			}
			else
			{
				r = ( t - min ) * ooRange;
				g = 1.0f - r;
				b = 0.0f;
			}
			DrawModelDebugOverlay( s_SavedModelInfo[i].m_ModelInfo, s_SavedModelInfo[i].m_ModelResults, s_SavedModelInfo[i].m_Origin, r, g, b );
		}
	}
	ClearSaveModelDebugOverlays();
}

void CModelRender::DebugDrawLightingOrigin( const DrawModelState_t& state, const ModelRenderInfo_t &pInfo )
{
#ifndef DEDICATED
	// determine light origin in world space
	Vector illumPosition;
	Vector lightOrigin;
	if ( pInfo.pLightingOrigin )
	{
		illumPosition = *pInfo.pLightingOrigin;
		lightOrigin = illumPosition;
	}
	else
	{
		R_ComputeLightingOrigin( state.m_pRenderable, state.m_pStudioHdr, *state.m_pModelToWorld, illumPosition );
		lightOrigin = illumPosition;
		if ( pInfo.pLightingOffset )
		{
			VectorTransform( illumPosition, *pInfo.pLightingOffset, lightOrigin );
		}
	}

	// draw z planar cross at lighting origin
	Vector pt0;
	Vector pt1;
	pt0    = lightOrigin;
	pt1    = lightOrigin;
	pt0.x -= 4;
	pt1.x += 4;
	CDebugOverlay::AddLineOverlay( pt0, pt1, 0, 255, 0, 255, true, 0.0f );
	pt0    = lightOrigin;
	pt1    = lightOrigin;
	pt0.y -= 4;
	pt1.y += 4;
	CDebugOverlay::AddLineOverlay( pt0, pt1, 0, 255, 0, 255, true, 0.0f );

	// draw lines from the light origin to the hull boundaries to identify model
	Vector pt;
	pt0.x = state.m_pStudioHdr->hull_min.x;
	pt0.y = state.m_pStudioHdr->hull_min.y;
	pt0.z = state.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_min.x;
	pt0.y = state.m_pStudioHdr->hull_max.y;
	pt0.z = state.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_max.x;
	pt0.y = state.m_pStudioHdr->hull_max.y;
	pt0.z = state.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_max.x;
	pt0.y = state.m_pStudioHdr->hull_min.y;
	pt0.z = state.m_pStudioHdr->hull_min.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );

	pt0.x = state.m_pStudioHdr->hull_min.x;
	pt0.y = state.m_pStudioHdr->hull_min.y;
	pt0.z = state.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_min.x;
	pt0.y = state.m_pStudioHdr->hull_max.y;
	pt0.z = state.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_max.x;
	pt0.y = state.m_pStudioHdr->hull_max.y;
	pt0.z = state.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );
	pt0.x = state.m_pStudioHdr->hull_max.x;
	pt0.y = state.m_pStudioHdr->hull_min.y;
	pt0.z = state.m_pStudioHdr->hull_max.z;
	VectorTransform( pt0, *state.m_pModelToWorld, pt1 );
	CDebugOverlay::AddLineOverlay( lightOrigin, pt1, 100, 100, 150, 255, true, 0.0f  );	
#endif
}
//-----------------------------------------------------------------------------
// Actually renders the model
//-----------------------------------------------------------------------------
void CModelRender::DrawModelExecute( IMatRenderContext *pRenderContext, const DrawModelState_t &state, const ModelRenderInfo_t &pInfo, matrix3x4_t *pBoneToWorld )
{
#ifndef DEDICATED
	bool bShadowDepth = (pInfo.flags & STUDIO_SHADOWDEPTHTEXTURE) != 0;
	bool bSSAODepth = ( pInfo.flags & STUDIO_SSAODEPTHTEXTURE ) != 0;
	bool bSkipDecals = ( pInfo.flags & STUDIO_SKIP_DECALS ) != 0;
	bool bSkipFlexes = ( pInfo.flags & STUDIO_SKIP_FLEXES ) != 0;

	// Bail if we're rendering into shadow depth map and this model doesn't cast shadows
	if ( bShadowDepth && ( ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_DO_NOT_CAST_SHADOWS ) != 0 ) )
		return;

	if ( g_bTextMode )
		return;

	// Sets up flexes
	float *pFlexWeights = NULL;
	float *pFlexDelayedWeights = NULL;
	CMatRenderData< float > rdFlexWeights( pRenderContext );
	CMatRenderData< float > rdDelayedFlexWeights( pRenderContext );
	if ( !bSkipFlexes && ( state.m_pStudioHdr->numflexdesc > 0 ) )
	{
		// Does setup for flexes
		Assert( pBoneToWorld );
		pFlexWeights = rdFlexWeights.Lock( state.m_pStudioHdr->numflexdesc );
		if ( state.m_pRenderable->UsesFlexDelayedWeights() )
		{
			pFlexDelayedWeights = rdDelayedFlexWeights.Lock( state.m_pStudioHdr->numflexdesc );
		}
		if ( pFlexWeights )
		{
			state.m_pRenderable->SetupWeights( pBoneToWorld, state.m_pStudioHdr->numflexdesc, pFlexWeights, pFlexDelayedWeights );
		}
	}

	DrawModelInfo_t info;
	ColorMeshInfo_t *pColorMeshes = NULL;
	DataCacheHandle_t hColorMeshData = DC_INVALID_HANDLE;
	if ( ( pInfo.flags & STUDIO_KEEP_SHADOWS ) != 0 )
	{
		info.m_bStaticLighting = false;
	}
	else if ( !bShadowDepth && !bSSAODepth && ( ( pInfo.flags & STUDIO_NOLIGHTING_OR_CUBEMAP ) == 0 ) )
	{
		// Shadow state...
		g_pShadowMgr->SetModelShadowState( pInfo.instance );

		// OPTIMIZE: Try to precompute part of this mess once a frame at the very least.
		bool bUsesBumpmapping = ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_USES_BUMPMAPPING ) ? true : false;
		static ConVarRef r_staticlight_streams( "r_staticlight_streams" );
		int numLightingComponents = r_staticlight_streams.GetInt();

		bool bStaticLighting = ( state.m_drawFlags & STUDIORENDER_DRAW_STATIC_LIGHTING ) &&
									( state.m_pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) && 
								( !bUsesBumpmapping || numLightingComponents > 1 ) && 
									( pInfo.instance != MODEL_INSTANCE_INVALID );

		bool bVertexLit = ( pInfo.pModel->flags & MODELFLAG_VERTEXLIT ) != 0;

		bool bNeedsEnvCubemap = r_showenvcubemap.GetInt() || ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP );
		
		if ( r_drawmodellightorigin.GetBool() && !bShadowDepth && !bSSAODepth )
		{
			DebugDrawLightingOrigin( state, pInfo );
		}

		if ( bStaticLighting )
		{
			// have static lighting, get from cache
			hColorMeshData = m_ModelInstances[pInfo.instance].m_ColorMeshHandle;
			CColorMeshData *pColorMeshData = CacheGet( hColorMeshData );
			if ( !pColorMeshData || pColorMeshData->m_bNeedsRetry )
			{
				// color meshes are not present, try to re-establish
				if ( RecomputeStaticLighting( pInfo.instance ) )
				{
					pColorMeshData = CacheGet( hColorMeshData );
				}
				else if ( !pColorMeshData || !pColorMeshData->m_bNeedsRetry )
				{
					// can't draw
					return;
				}
			}

			if ( pColorMeshData && pColorMeshData->m_bColorMeshValid )
			{
				pColorMeshes = pColorMeshData->m_pMeshInfos;
			}
			else
			{
				// failed, draw without static lighting
				bStaticLighting = false;
			}
		}

		info.m_bStaticLighting = false;

		// get lighting from ambient light sources and radiosity bounces
		// also set up the env_cubemap from the light cache if necessary.
		if ( ( bVertexLit || bNeedsEnvCubemap ) && !bSSAODepth )
		{
			// See if we're using static lighting
			LightCacheHandle_t* pLightCache = NULL;
			if ( pInfo.instance != MODEL_INSTANCE_INVALID )
			{
				if ( ( m_ModelInstances[pInfo.instance].m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING ) && m_ModelInstances[pInfo.instance].m_LightCacheHandle )
				{
					pLightCache = &m_ModelInstances[pInfo.instance].m_LightCacheHandle;
				}
			}

			// Choose the lighting origin
			Vector entOrigin;
			R_ComputeLightingOrigin( state.m_pRenderable, state.m_pStudioHdr, *state.m_pModelToWorld, entOrigin );

			// Set up lighting based on the lighting origin
			StudioSetupLighting( state, entOrigin, pLightCache, bVertexLit, bNeedsEnvCubemap, bStaticLighting, info, pInfo, state.m_drawFlags );
		}
	}
	else
	{
		info.m_bStaticLighting = false;
		g_pStudioRender->ClearAllShadows();
	}

	// Set up the camera state
	g_pStudioRender->SetViewState( CurrentViewOrigin(), CurrentViewRight(), CurrentViewUp(), CurrentViewForward() );

	// Color + alpha modulation
	g_pStudioRender->SetColorModulation( r_colormod );
	g_pStudioRender->SetAlphaModulation( r_blend );

	Assert( modelloader->IsLoaded( pInfo.pModel ) );
	info.m_pStudioHdr = state.m_pStudioHdr;
	info.m_pHardwareData = state.m_pStudioHWData;
	info.m_Skin = pInfo.skin;
	info.m_Body = pInfo.body;
	info.m_HitboxSet = pInfo.hitboxset;
	info.m_pClientEntity = (void*)state.m_pRenderable;
	info.m_Lod = state.m_lod;
	info.m_pColorMeshes = pColorMeshes;

	// Don't do decals if shadow depth mapping...
	info.m_Decals = (bShadowDepth || bSSAODepth) ? STUDIORENDER_DECAL_INVALID : state.m_decals;

	// Get perf stats if we are going to use them.
	int overlayVal = r_drawmodelstatsoverlay.GetInt();
	int drawFlags = state.m_drawFlags;

	if ( bShadowDepth )
	{
		drawFlags |= STUDIORENDER_DRAW_OPAQUE_ONLY;
		drawFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE;
	}

	if ( bSSAODepth == true )
	{
		drawFlags |= STUDIORENDER_DRAW_OPAQUE_ONLY;
		drawFlags |= STUDIORENDER_SSAODEPTHTEXTURE;
	}

	if ( overlayVal && !bShadowDepth && !bSSAODepth )
	{
		drawFlags |= STUDIORENDER_DRAW_GET_PERF_STATS;
	}

	if ( bSkipDecals )
	{
		drawFlags |= STUDIORENDER_SKIP_DECALS;
	}

	if ( bSkipFlexes )
	{
		drawFlags |= STUDIORENDER_DRAW_NO_FLEXES;
	}

	if ( ( pInfo.flags & STUDIO_KEEP_SHADOWS ) != 0 )
	{
		drawFlags |= STUDIORENDER_NO_PRIMARY_DRAW;
	}

	DrawModelResults_t results;
	g_pStudioRender->DrawModel( &results, info, pBoneToWorld, pFlexWeights, 
		pFlexDelayedWeights, pInfo.origin, drawFlags );
	info.m_Lod = results.m_nLODUsed;

	if ( overlayVal && !bShadowDepth && !bSSAODepth )
	{
		if ( overlayVal != 2 )
		{
			DrawModelDebugOverlay( info, results, pInfo.origin );
		}
		else
		{
			AddModelDebugOverlay( info, results, pInfo.origin );
		}
	}

	if ( pColorMeshes)
	{
		ProtectColorDataIfQueued( hColorMeshData );
	}

#endif
}

//-----------------------------------------------------------------------------
// Main entry point for model rendering in the engine
//-----------------------------------------------------------------------------
int CModelRender::DrawModel( 	
	int flags,
	IClientRenderable *pRenderable,
	ModelInstanceHandle_t instance,
	int entity_index, 
	const model_t *pModel, 
	const Vector &origin,
	const QAngle &angles,
	int skin,
	int body,
	int hitboxset,
	const matrix3x4_t* pModelToWorld,
	const matrix3x4_t *pLightingOffset )
{
	ModelRenderInfo_t sInfo;
	sInfo.flags = flags;
	sInfo.pRenderable = pRenderable;
	sInfo.instance = instance;
	sInfo.entity_index = entity_index;
	sInfo.pModel = pModel;
	sInfo.origin = origin;
	sInfo.angles = angles;
	sInfo.skin = skin;
	sInfo.body = body;
	sInfo.hitboxset = hitboxset;
	sInfo.pModelToWorld = pModelToWorld;
	sInfo.pLightingOffset = pLightingOffset;

	if ( (r_entity.GetInt() == -1) || (r_entity.GetInt() == entity_index) )
	{
		return DrawModelEx( sInfo );
	}

	return 0;
}

static inline int GetLOD()
{
#ifdef CSTRIKE15
	// Always slamp r_lod to 0 in CS:GO.
	return 0;
#else
	return r_lod.GetInt();
#endif
}

int	CModelRender::ComputeLOD( IMatRenderContext *pRenderContext, const ModelRenderInfo_t &info, studiohwdata_t *pStudioHWData )
{
	int lod = GetLOD();
	// FIXME!!!  This calc should be in studiorender, not here!!!!!  But since the bone setup
	// is done here, and we need the bone mask, we'll do it here for now.
	if ( lod == -1 )
	{
		float screenSize = pRenderContext->ComputePixelWidthOfSphere(info.pRenderable->GetRenderOrigin(), 0.5f );
		float metric = pStudioHWData->LODMetric(screenSize);
		lod = pStudioHWData->GetLODForMetric(metric);
	}
	else
	{
		if ( ( info.flags & STUDIOHDR_FLAGS_HASSHADOWLOD ) && ( lod > pStudioHWData->m_NumLODs - 2 ) )
		{
			lod = pStudioHWData->m_NumLODs - 2;
		}
		else if ( lod > pStudioHWData->m_NumLODs - 1 )
		{
			lod = pStudioHWData->m_NumLODs - 1;
		}
		else if( lod < 0 )
		{
			lod = 0;
		}
	}

	if ( lod < 0 )
	{
		lod = 0;
	}
	else if ( lod >= pStudioHWData->m_NumLODs )
	{
		lod = pStudioHWData->m_NumLODs - 1;
	}

	// clamp to root lod
	if (lod < pStudioHWData->m_RootLOD)
	{
		lod = pStudioHWData->m_RootLOD;
	}

	Assert( lod >= 0 && lod < pStudioHWData->m_NumLODs );
	return lod;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &pInfo - 
//-----------------------------------------------------------------------------
bool CModelRender::DrawModelSetup( IMatRenderContext *pRenderContext, ModelRenderInfo_t &pInfo, DrawModelState_t *pState, matrix3x4_t **ppBoneToWorldOut )
{
	*ppBoneToWorldOut = NULL;

#ifdef DEDICATED
	return false;
#else

#if _DEBUG
	if ( (char*)pInfo.pRenderable < (char*)1024 )
	{
		Error( "CModelRender::DrawModel: pRenderable == 0x%p", pInfo.pRenderable );
	}
#endif

	// Can only deal with studio models
	Assert( pInfo.pModel->type == mod_studio );
	Assert( modelloader->IsLoaded( pInfo.pModel ) );

	DrawModelState_t &state = *pState;
	state.m_pStudioHdr = g_pMDLCache->GetStudioHdr( pInfo.pModel->studio );
	state.m_pRenderable = pInfo.pRenderable;

	// Quick exit if we're just supposed to draw a specific model...
	if ( (r_entity.GetInt() != -1) && (r_entity.GetInt() != pInfo.entity_index) )
		return false;

	// quick exit
	if (state.m_pStudioHdr->numbodyparts == 0)
		return false;

	if ( !pInfo.pModelToWorld )
	{
		Assert( 0 );
		return false;
	}

	state.m_pModelToWorld = pInfo.pModelToWorld;

	Assert ( pInfo.pRenderable );

	if ( IsGameConsole() && ( pInfo.pModel->flags & MODELFLAG_VIEW_WEAPON_MODEL ) && !modelloader->IsViewWeaponModelResident( pInfo.pModel ) )
	{
		state.m_pStudioHWData = NULL;
		return false;
	}

	state.m_pStudioHWData = g_pMDLCache->GetHardwareData( pInfo.pModel->studio );
	if ( !state.m_pStudioHWData )
		return false;

	state.m_lod = ComputeLOD( pRenderContext, pInfo, state.m_pStudioHWData );
	
	int boneMask = BONE_USED_BY_VERTEX_AT_LOD( state.m_lod );
	// Why isn't this always set?!?

	if ( ( pInfo.flags & STUDIO_RENDER ) == 0 )
	{
		// no rendering, just force a bone setup.  Don't copy the bones
		bool bOk = pInfo.pRenderable->SetupBones( NULL, MAXSTUDIOBONES, boneMask, GetBaseLocalClient().GetTime() );
		return bOk;
	}

	int nBoneCount = state.m_pStudioHdr->numbones;
	CMatRenderData< matrix3x4a_t > rdBoneToWorld( pRenderContext );
	matrix3x4a_t *pBoneToWorld = rdBoneToWorld.Lock( nBoneCount );
	const bool bOk = pInfo.pRenderable->SetupBones( pBoneToWorld, nBoneCount, boneMask, GetBaseLocalClient().GetTime() );
	if ( !bOk )
		return false;

	*ppBoneToWorldOut = pBoneToWorld;

	// Convert the instance to a decal handle.
	state.m_decals = STUDIORENDER_DECAL_INVALID;
	if (pInfo.instance != MODEL_INSTANCE_INVALID)
	{
		state.m_decals = m_ModelInstances[pInfo.instance].m_DecalHandle;
	}

	state.m_drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL;
	if ( pInfo.flags & STUDIO_TWOPASS )
	{
		if ( pInfo.flags & STUDIO_TRANSPARENCY )
		{
			state.m_drawFlags = STUDIORENDER_DRAW_TRANSLUCENT_ONLY; 
		}
		else
		{
			state.m_drawFlags = STUDIORENDER_DRAW_OPAQUE_ONLY; 
		}
	}
	if ( pInfo.flags & STUDIO_STATIC_LIGHTING )
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_STATIC_LIGHTING;
	}
	
	if( pInfo.flags & STUDIO_ITEM_BLINK )
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_ITEM_BLINK;
	}

	if ( pInfo.flags & STUDIO_WIREFRAME )
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_WIREFRAME;
	}

	if ( pInfo.flags & STUDIO_NOSHADOWS )
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_NO_SHADOWS;
	}

	if ( r_drawmodelstatsoverlay.GetInt() == 2)
	{
		state.m_drawFlags |= STUDIORENDER_DRAW_ACCURATETIME;
	}

	if ( pInfo.flags & STUDIO_SHADOWDEPTHTEXTURE )
	{
		state.m_drawFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE;
	}

	if ( pInfo.flags & STUDIO_SSAODEPTHTEXTURE )
	{
		state.m_drawFlags |= STUDIORENDER_SSAODEPTHTEXTURE;
	}


	if ( IsGameConsole() && ( pInfo.pModel->flags & MODELFLAG_VIEW_WEAPON_MODEL ) && modelloader->IsModelInWeaponCache( pInfo.pModel ) )
	{
		// queued rendering needs to know about these because their data can be evicted while they are queued
		state.m_drawFlags |= STUDIORENDER_MODEL_IS_CACHEABLE;
	}

	return true;
#endif
}

int	CModelRender::DrawModelEx( ModelRenderInfo_t &pInfo )
{
#ifndef DEDICATED
	DrawModelState_t state;

	matrix3x4_t tmpmat;
	if ( !pInfo.pModelToWorld )
	{
		pInfo.pModelToWorld = &tmpmat;

		// Turns the origin + angles into a matrix
		AngleMatrix( pInfo.angles, pInfo.origin, tmpmat );
	}

	CMatRenderContextPtr pRenderContext( materials );
	CMatRenderDataReference rd( pRenderContext );

	matrix3x4_t *pBoneToWorld;
	if ( !DrawModelSetup( pRenderContext, pInfo, &state, &pBoneToWorld ) )
		return 0;

	if ( pInfo.flags & STUDIO_RENDER )
	{
		DrawModelExecute( pRenderContext, state, pInfo, pBoneToWorld );
	}

	return 1;
#else
	return 0;
#endif
}

int	CModelRender::DrawModelExStaticProp( IMatRenderContext *pRenderContext, ModelRenderInfo_t &pInfo )
{
	VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "CModelRender::DrawModelExStaticProp");

#ifndef DEDICATED
	bool bShadowDepth = ( pInfo.flags & STUDIO_SHADOWDEPTHTEXTURE ) != 0;
	bool bSSAODepth = ( pInfo.flags & STUDIO_SSAODEPTHTEXTURE ) != 0;

#if _DEBUG
	if ( (char*)pInfo.pRenderable < (char*)1024 )
	{
		Error( "CModelRender::DrawModel: pRenderable == 0x%p", pInfo.pRenderable );
	}	

	// Can only deal with studio models
	if ( pInfo.pModel->type != mod_studio )
		return 0;
#endif
	Assert( modelloader->IsLoaded( pInfo.pModel ) );

	DrawModelState_t state;
	state.m_pStudioHdr = g_pMDLCache->GetStudioHdr( pInfo.pModel->studio );
	state.m_pRenderable = pInfo.pRenderable;

	// quick exit
	if ( state.m_pStudioHdr->numbodyparts == 0 || g_bTextMode )
		return 1;

	state.m_pStudioHWData = g_pMDLCache->GetHardwareData( pInfo.pModel->studio );
	if ( !state.m_pStudioHWData )
		return 0;

	Assert( pInfo.pModelToWorld );
	state.m_pModelToWorld = pInfo.pModelToWorld;
	Assert ( pInfo.pRenderable );

	int lod = ComputeLOD( pRenderContext, pInfo, state.m_pStudioHWData );
	// int boneMask = BONE_USED_BY_VERTEX_AT_LOD( lod );
	// Why isn't this always set?!?
	if ( !(pInfo.flags & STUDIO_RENDER) )
		return 0;

	// Convert the instance to a decal handle.
	StudioDecalHandle_t decalHandle = STUDIORENDER_DECAL_INVALID;
	if ( (pInfo.instance != MODEL_INSTANCE_INVALID) && !(pInfo.flags & STUDIO_SHADOWDEPTHTEXTURE) )
	{
		decalHandle = m_ModelInstances[pInfo.instance].m_DecalHandle;
	}

	int drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL;
	if ( pInfo.flags & STUDIO_TWOPASS )
	{
		if ( pInfo.flags & STUDIO_TRANSPARENCY )
		{
			drawFlags = STUDIORENDER_DRAW_TRANSLUCENT_ONLY; 
		}
		else
		{
			drawFlags = STUDIORENDER_DRAW_OPAQUE_ONLY; 
		}
	}

	if ( pInfo.flags & STUDIO_STATIC_LIGHTING )
	{
		drawFlags |= STUDIORENDER_DRAW_STATIC_LIGHTING;
	}

	if ( pInfo.flags & STUDIO_WIREFRAME )
	{
		drawFlags |= STUDIORENDER_DRAW_WIREFRAME;
	}

	// Shadow state...
	g_pShadowMgr->SetModelShadowState( pInfo.instance );

	// OPTIMIZE: Try to precompute part of this mess once a frame at the very least.
	bool bUsesBumpmapping = ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_USES_BUMPMAPPING ) ? true : false;
	static ConVarRef r_staticlight_streams( "r_staticlight_streams" );
	int numLightingComponents = r_staticlight_streams.GetInt();

	bool bStaticLighting = (( drawFlags & STUDIORENDER_DRAW_STATIC_LIGHTING ) &&
		( state.m_pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) && 
		( !bUsesBumpmapping || numLightingComponents > 1 ) && 
		( pInfo.instance != MODEL_INSTANCE_INVALID ) );

	bool bVertexLit = ( pInfo.pModel->flags & MODELFLAG_VERTEXLIT ) != 0;
	bool bNeedsEnvCubemap = r_showenvcubemap.GetInt() || ( pInfo.pModel->flags & MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP );

	if ( r_drawmodellightorigin.GetBool() )
	{
		DebugDrawLightingOrigin( state, pInfo );
	}

	ColorMeshInfo_t *pColorMeshes = NULL;
	DataCacheHandle_t hColorMeshData = DC_INVALID_HANDLE;
	if ( bStaticLighting )
	{
		VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "DrawModelExStaticProp get static lighting");
		// have static lighting, get from cache
		hColorMeshData = m_ModelInstances[pInfo.instance].m_ColorMeshHandle;
		CColorMeshData *pColorMeshData = CacheGet( hColorMeshData );
		if ( !pColorMeshData || pColorMeshData->m_bNeedsRetry )
		{
			// color meshes are not present, try to re-establish
			if ( RecomputeStaticLighting( pInfo.instance ) )
			{
				pColorMeshData = CacheGet( hColorMeshData );
			}
			else if ( !pColorMeshData || !pColorMeshData->m_bNeedsRetry )
			{
				// can't draw
				return 0;
			}
		}

		if ( pColorMeshData && pColorMeshData->m_bColorMeshValid )
		{
			pColorMeshes = pColorMeshData->m_pMeshInfos;
		}
		else
		{
			// failed, draw without static lighting
			bStaticLighting = false;
		}
	}

	DrawModelInfo_t info;
	info.m_bStaticLighting = false;

	// Get lighting from ambient light sources and radiosity bounces
	// also set up the env_cubemap from the light cache if necessary.
	// Don't bother if we're rendering to shadow depth texture
	if ( ( bVertexLit || bNeedsEnvCubemap ) && !bShadowDepth && !bSSAODepth )
	{
		VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "DrawModelExStaticProp setup dyn lighting");
		// See if we're using static lighting
		LightCacheHandle_t* pLightCache = NULL;
		if ( pInfo.instance != MODEL_INSTANCE_INVALID )
		{
			if ( ( m_ModelInstances[pInfo.instance].m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING ) && m_ModelInstances[pInfo.instance].m_LightCacheHandle )
			{
				pLightCache = &m_ModelInstances[pInfo.instance].m_LightCacheHandle;
			}
		}

		// Choose the lighting origin
		Vector entOrigin;
		if ( !pLightCache )
		{
			R_ComputeLightingOrigin( state.m_pRenderable, state.m_pStudioHdr, *state.m_pModelToWorld, entOrigin );
		}

		// Set up lighting based on the lighting origin
		StudioSetupLighting( state, entOrigin, pLightCache, bVertexLit, bNeedsEnvCubemap, bStaticLighting, info, pInfo, drawFlags );
	}

	Assert( modelloader->IsLoaded( pInfo.pModel ) );
	info.m_pStudioHdr = state.m_pStudioHdr;
	info.m_pHardwareData = state.m_pStudioHWData;
	info.m_Decals = decalHandle;
	info.m_Skin = pInfo.skin;
	info.m_Body = pInfo.body;
	info.m_HitboxSet = pInfo.hitboxset;
	info.m_pClientEntity = (void*)state.m_pRenderable;
	info.m_Lod = lod;
	info.m_pColorMeshes = pColorMeshes;

	if ( bShadowDepth )
	{
		drawFlags |= STUDIORENDER_SHADOWDEPTHTEXTURE;
	}

	if ( bSSAODepth )
	{
		drawFlags |= STUDIORENDER_SSAODEPTHTEXTURE;
	}


#ifdef _DEBUG
	Vector tmp;
	MatrixGetColumn( *pInfo.pModelToWorld, 3, tmp );
	Assert( VectorsAreEqual( pInfo.origin, tmp, 1e-3 ) );
#endif

	g_pStudioRender->DrawModelStaticProp( info, *pInfo.pModelToWorld, drawFlags );

	if ( pColorMeshes)
	{
		ProtectColorDataIfQueued( hColorMeshData );
	}

	return 1;
#else
	return 0;
#endif
}

struct robject_t
{
	const matrix3x4_t	*pMatrix;
	IClientRenderable	*pRenderable;
	ColorMeshInfo_t		*pColorMeshes;
	ITexture			*pEnvCubeMap;
	Vector				*pLightingOrigin;
	short				modelIndex;
	short				lod;
	ModelInstanceHandle_t instance;
	short				skin;
	short				lightIndex;
	uint8				alpha;
	uint8				pad0;
};

struct rmodel_t
{
	const model_t *			pModel;
	studiohdr_t*			pStudioHdr;
	studiohwdata_t*			pStudioHWData;
	float					maxArea;
	short					lodStart;
	byte					lodCount;
	byte					bVertexLit : 1;
	byte					bNeedsCubemap : 1;
	byte					bStaticLighting : 1;
};

class CRobjectLess
{
public:
	bool Less( const robject_t& lhs, const robject_t& rhs, void *pContext )
	{
		rmodel_t *pModels = static_cast<rmodel_t *>(pContext);
		if ( lhs.modelIndex == rhs.modelIndex )
		{
			if ( lhs.skin != rhs.skin )
				return lhs.skin < rhs.skin;
			return lhs.lod < rhs.lod;
		}
		if ( pModels[lhs.modelIndex].maxArea == pModels[rhs.modelIndex].maxArea )
			return lhs.modelIndex < rhs.modelIndex;
		return pModels[lhs.modelIndex].maxArea > pModels[rhs.modelIndex].maxArea;
	}
};

struct rdecalmodel_t
{
	short			objectIndex;
	short			lightIndex;
};
/*
// ----------------------------------------
// not yet implemented

struct rlod_t
{
	short groupCount;
	short groupStart;
};

struct rgroup_t
{
	IMesh	*pMesh;
	short	batchCount;
	short	batchStart;
	short	colorMeshIndex;
	short	pad0;
};

struct rbatch_t
{
	IMaterial		*pMaterial;
	short			primitiveType;
	short			pad0;
	unsigned short	indexOffset;
	unsigned short	indexCount;
};
// ----------------------------------------
*/

inline int FindModel( const rmodel_t *pList, int listCount, const model_t *pModel )
{
	for ( int j = listCount; --j >= 0 ; )
	{
		if ( pList[j].pModel == pModel )
			return j;
	}
	return -1;
}


// NOTE: UNDONE: This is a work in progress of a new static prop rendering pipeline
// UNDONE: Expose drawing commands from studiorender and draw here
// UNDONE: Build a similar pipeline for non-static prop models
// UNDONE: Split this into several functions in a sub-object
ConVar r_staticprop_lod( "r_staticprop_lod", "-1", FCVAR_DEVELOPMENTONLY );
int CModelRender::DrawStaticPropArrayFast( StaticPropRenderInfo_t *pProps, int count, bool bShadowDepth )
{
#ifndef DEDICATED
	VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "CModelRender::DrawStaticPropArrayFast");
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );
	CMatRenderContextPtr pRenderContext( materials );
	const int MAX_OBJECTS = 1024;
	CUtlSortVector<robject_t, CRobjectLess> objectList(0, MAX_OBJECTS);
	CUtlVectorFixedGrowable<rmodel_t,256> modelList;
	CUtlVectorFixedGrowable<short,256> lightObjects;
	CUtlVectorFixedGrowable<short,64> shadowObjects;
	CUtlVectorFixedGrowable<rdecalmodel_t,64> decalObjects;
	CUtlVectorFixedGrowable<LightingState_t,256> lightStates;
	bool bForceCubemap = r_showenvcubemap.GetBool();
	int drawnCount = 0;
	int forcedLodSetting = GetLOD();
#ifndef CSTRIKE15
	if ( r_staticprop_lod.GetInt() >= 0 )
	{
		forcedLodSetting = r_staticprop_lod.GetInt();
	}
#endif
#ifdef VPROF_ENABLED
	g_VProfCurrentProfile.EnterScope( "build unique model list", 2, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
#endif
	// build list of objects and unique models
	for ( int i = 0; i < count; i++ )
	{
		drawnCount++;
		// UNDONE: This is a perf hit in some scenes!  Use a hash?
		int modelIndex = FindModel( modelList.Base(), modelList.Count(), pProps[i].pModel );
		if ( modelIndex < 0 )
		{
			modelIndex = modelList.AddToTail();
			modelList[modelIndex].pModel = pProps[i].pModel;
		}
		robject_t obj;
		obj.pMatrix = pProps[i].pModelToWorld;
		obj.pRenderable = pProps[i].pRenderable;
		obj.modelIndex = modelIndex;
		obj.instance = pProps[i].instance;
		obj.skin = pProps[i].skin;
		obj.alpha = pProps[i].alpha;
		obj.lod = 0;
		obj.pColorMeshes = NULL;
		obj.pEnvCubeMap = NULL;
		obj.lightIndex = -1;
		obj.pLightingOrigin = pProps[i].pLightingOrigin;
		objectList.InsertNoSort(obj);
	}

	// process list of unique models
	int lodStart = 0;
	for ( int i = 0; i < modelList.Count(); i++ )
	{
		const model_t *pModel = modelList[i].pModel;
		Assert( modelloader->IsLoaded( pModel ) );
		unsigned int flags = pModel->flags;
		modelList[i].pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );
		modelList[i].pStudioHWData = g_pMDLCache->GetHardwareData( pModel->studio );
		modelList[i].maxArea = 1.0f;
		modelList[i].lodStart = lodStart;
		modelList[i].lodCount = modelList[i].pStudioHWData->m_NumLODs;
		bool bBumpMapped = (flags & MODELFLAG_STUDIOHDR_USES_BUMPMAPPING) != 0;
		static ConVarRef r_staticlight_streams( "r_staticlight_streams" );
		int numLightingComponents = r_staticlight_streams.GetInt();
		modelList[i].bStaticLighting = (( modelList[i].pStudioHdr->flags & STUDIOHDR_FLAGS_STATIC_PROP ) != 0) && ( !bBumpMapped || numLightingComponents > 1 );
		modelList[i].bVertexLit = ( flags & MODELFLAG_VERTEXLIT ) != 0;
		modelList[i].bNeedsCubemap = ( flags & MODELFLAG_STUDIOHDR_USES_ENV_CUBEMAP ) != 0;

		lodStart += modelList[i].lodCount;
	}
#ifdef VPROF_ENABLED
	g_VProfCurrentProfile.ExitScope();
#endif
	// -1 is automatic lod
	if ( forcedLodSetting < 0 )
	{
		VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "Compute LOD");
		// compute the lod of each object
		for ( int i = 0; i < objectList.Count(); i++ )
		{
			Vector org;
			MatrixGetColumn( *objectList[i].pMatrix, 3, org );
			float screenSize = pRenderContext->ComputePixelWidthOfSphere(org, 0.5f );
			const rmodel_t &model = modelList[objectList[i].modelIndex];
			float metric = model.pStudioHWData->LODMetric(screenSize);
			objectList[i].lod = model.pStudioHWData->GetLODForMetric(metric);
			if ( objectList[i].lod < model.pStudioHWData->m_RootLOD )
			{
				objectList[i].lod = model.pStudioHWData->m_RootLOD;
			}
			modelList[objectList[i].modelIndex].maxArea = MAX(modelList[objectList[i].modelIndex].maxArea, screenSize);
		}
	}
	else
	{
		// force the lod of each object
		for ( int i = 0; i < objectList.Count(); i++ )
		{
			const rmodel_t &model = modelList[objectList[i].modelIndex];
			objectList[i].lod = clamp(forcedLodSetting, model.pStudioHWData->m_RootLOD, model.lodCount-1);
		}
	}
	// UNDONE: Don't sort if rendering transparent objects - for now this isn't called in the transparent case
	// sort by model, then by lod
	objectList.SetLessContext( static_cast<void *>(modelList.Base()) );
	objectList.RedoSort(true);

	ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
	
	// now build out the lighting states
	if ( !bShadowDepth )
	{
		VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "build lighting states");
		for ( int i = 0; i < objectList.Count(); i++ )
		{
			robject_t &obj = objectList[i];
			rmodel_t &model = modelList[obj.modelIndex];
			bool bStaticLighting = (model.bStaticLighting && obj.instance != MODEL_INSTANCE_INVALID);
			bool bVertexLit = model.bVertexLit;
			bool bNeedsEnvCubemap = bForceCubemap || model.bNeedsCubemap;
			bool bHasDecals = ( m_ModelInstances[obj.instance].m_DecalHandle != STUDIORENDER_DECAL_INVALID ) ? true : false;
			LightingState_t *pDecalLightState = NULL;
			if ( bHasDecals )
			{
				rdecalmodel_t decalModel;
				decalModel.lightIndex = lightStates.AddToTail();
				pDecalLightState = &lightStates[decalModel.lightIndex];
				decalModel.objectIndex = i;
				decalObjects.AddToTail( decalModel );
			}
			// for now we skip models that have shadows - will update later to include them in a post-pass
			if ( g_pShadowMgr->ModelHasShadows( obj.instance ) )
			{
				shadowObjects.AddToTail(i);
			}

			// get the static lighting from the cache
			DataCacheHandle_t hColorMeshData = DC_INVALID_HANDLE;
			if ( bStaticLighting )
			{
				// have static lighting, get from cache
				hColorMeshData = m_ModelInstances[obj.instance].m_ColorMeshHandle;
				CColorMeshData *pColorMeshData = CacheGet( hColorMeshData );
				if ( !pColorMeshData || pColorMeshData->m_bNeedsRetry )
				{
					// color meshes are not present, try to re-establish
					
					if ( UpdateStaticPropColorData( obj.pRenderable->GetIClientUnknown(), obj.instance ) )
					{
						pColorMeshData = CacheGet( hColorMeshData );
					}
					else if ( !pColorMeshData || !pColorMeshData->m_bNeedsRetry )
					{
						// can't draw
						continue;
					}
				}

				if ( pColorMeshData && pColorMeshData->m_bColorMeshValid )
				{
					obj.pColorMeshes = pColorMeshData->m_pMeshInfos;
					if ( pCallQueue )
					{
						if ( CacheLock( hColorMeshData ) ) // CacheCreate above will call functions that won't take place until later. If color mesh isn't used right away, it could get dumped
						{
							pCallQueue->QueueCall( this, &CModelRender::CacheUnlock, hColorMeshData );
						}
					}
				}
				else
				{
					// failed, draw without static lighting
					bStaticLighting = false;
				}
			}

			// Get lighting from ambient light sources and radiosity bounces
			// also set up the env_cubemap from the light cache if necessary.
			if ( ( bVertexLit || bNeedsEnvCubemap ) )
			{
				// See if we're using static lighting
				LightCacheHandle_t* pLightCache = NULL;
				ITexture *pEnvCubemapTexture = NULL;
				if ( obj.instance != MODEL_INSTANCE_INVALID )
				{
					if ( ( m_ModelInstances[obj.instance].m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING ) && m_ModelInstances[obj.instance].m_LightCacheHandle )
					{
						pLightCache = &m_ModelInstances[obj.instance].m_LightCacheHandle;
					}
				}

				Assert(pLightCache);
				LightingState_t lightingState;
				LightingState_t *pState = &lightingState;
				if ( pLightCache )
				{
					// dx8 and dx9 case. . .hardware can do baked lighting plus other dynamic lighting
					// We already have the static part baked into a color mesh, so just get the dynamic stuff.
					if ( bStaticLighting )
					{
						const model_t* pModel = model.pModel;
						if ( pModel->flags & MODELFLAG_STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY )
						{
							pState = LightcacheGetStatic( *pLightCache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
						}
						else
						{
							pState = LightcacheGetStatic( *pLightCache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
						}
						Assert( pState->numlights >= 0 && pState->numlights <= MAXLOCALLIGHTS );
					}
					else
					{
						pState = LightcacheGetStatic( *pLightCache, &pEnvCubemapTexture, LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
						Assert( pState->numlights >= 0 && pState->numlights <= MAXLOCALLIGHTS );
					}

					if ( bHasDecals )
					{
						for ( int iCube = 0; iCube < 6; ++iCube )
						{
							pDecalLightState->r_boxcolor[iCube] = m_ModelInstances[obj.instance].m_pLightingState->m_AmbientLightingState.r_boxcolor[iCube] + pState->r_boxcolor[iCube];
						}
						pDecalLightState->CopyLocalLights( m_ModelInstances[obj.instance].m_pLightingState->m_AmbientLightingState );
						pDecalLightState->AddAllLocalLights( *pState, *obj.pLightingOrigin );
					}
				}
				else	// !pLightcache
				{
					// UNDONE: is it possible to end up here in the static prop case?
					Vector vLightingOrigin = *obj.pLightingOrigin;
					int lightCacheFlags = ( bStaticLighting && ( !( model.pModel->flags & MODELFLAG_STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY ) ) ) ? ( LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE )
						: (LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE | LIGHTCACHEFLAGS_ALLOWFAST);
					LightcacheGetDynamic_Stats stats;
					pEnvCubemapTexture = LightcacheGetDynamic( vLightingOrigin, lightingState, 
						stats, obj.pRenderable, lightCacheFlags, false );
					Assert( lightingState.numlights >= 0 && lightingState.numlights <= MAXLOCALLIGHTS );
					pState = &lightingState;

					if ( bHasDecals )
					{
						LightcacheGetDynamic_Stats stats;
						LightcacheGetDynamic( vLightingOrigin, *pDecalLightState, stats, obj.pRenderable, 
							LIGHTCACHEFLAGS_STATIC|LIGHTCACHEFLAGS_DYNAMIC|LIGHTCACHEFLAGS_LIGHTSTYLE|LIGHTCACHEFLAGS_ALLOWFAST );
					}
				}

				if ( bNeedsEnvCubemap && pEnvCubemapTexture )
				{
					obj.pEnvCubeMap = pEnvCubemapTexture;
				}

				if ( bVertexLit )
				{
					// if we have any real lighting state we need to save it for this object
					if ( pState->numlights || pState->HasAmbientColors() )
					{
						obj.lightIndex = lightStates.AddToTail(*pState);
						lightObjects.AddToTail( i );
					}
				}
			}
		}
	}
	// now render the baked lighting props with no lighting state
	float color[3];
	color[0] = color[1] = color[2] = 1.0f;
	g_pStudioRender->SetColorModulation(color);
	g_pStudioRender->SetAlphaModulation(1.0f);
	g_pStudioRender->SetViewState( CurrentViewOrigin(), CurrentViewRight(), CurrentViewUp(), CurrentViewForward() );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	g_pStudioRender->ClearAllShadows();
	pRenderContext->DisableAllLocalLights();
	DrawModelInfo_t info;
	for ( int i = 0; i < 6; i++ )
	{
		info.m_LightingState.m_vecAmbientCube[i].Init();
	}
	g_pStudioRender->SetAmbientLightColors( info.m_LightingState.m_vecAmbientCube );
	info.m_LightingState.m_nLocalLightCount = 0;
	info.m_bStaticLighting = false;

	int drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL | STUDIORENDER_DRAW_STATIC_LIGHTING;
	if (bShadowDepth)
	{
		drawFlags |= STUDIO_SHADOWDEPTHTEXTURE;
	}
	info.m_Decals = STUDIORENDER_DECAL_INVALID;
	info.m_Body = 0;
	info.m_HitboxSet = 0;

	const int MAX_MESH_INSTANCE = 64;
	CMatRenderData< matrix3x4_t > rdPoseToWorld( pRenderContext, MAX_MESH_INSTANCE );
	matrix3x4_t *pPoseToWorld = rdPoseToWorld.Base();
	MeshInstanceData_t instanceData[MAX_MESH_INSTANCE];
	int lastModel = -1;
	int nInstanceCount = 0;
	info.m_Lod = 0;
	ColorMeshInfo_t *colorMeshes[MAX_MESH_INSTANCE];

	// g_VProfCurrentProfile.EnterScope( "draw nonlit props", VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 0 );
	{VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "draw nonlit props" );
	for ( int i = 0; i < objectList.Count(); i++ )
	{
		robject_t &obj = objectList[i];
		if ( obj.lightIndex >= 0 )
			continue;
		rmodel_t &model = modelList[obj.modelIndex];

		if ( lastModel != obj.modelIndex || info.m_Skin != obj.skin || obj.lod != info.m_Lod || nInstanceCount >= MAX_MESH_INSTANCE )
		{
			if ( nInstanceCount > 0 )
			{
				VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "flush to DrawModelArrayStaticProp()");
				g_pStudioRender->DrawModelArrayStaticProp( info, nInstanceCount, instanceData, colorMeshes );
				rdPoseToWorld.Release();
				pPoseToWorld = rdPoseToWorld.Lock( MAX_MESH_INSTANCE );
			}
			nInstanceCount = 0;
			info.m_pStudioHdr = model.pStudioHdr;
			info.m_pHardwareData = model.pStudioHWData;
			info.m_Skin = obj.skin;
			info.m_pClientEntity = static_cast<void*>(obj.pRenderable);
			info.m_Lod = obj.lod;
			lastModel = obj.modelIndex;
		}

		MeshInstanceData_t &instance = instanceData[nInstanceCount];
		memset( &instance, 0, sizeof(instance) );
		instance.m_nBoneCount = 1;
		instance.m_pPoseToWorld = &pPoseToWorld[nInstanceCount];
		instance.m_pEnvCubemap = obj.pEnvCubeMap;
		instance.m_bColorBufferHasIndirectLightingOnly = ( model.pStudioHdr->flags & STUDIOHDR_BAKED_VERTEX_LIGHTING_IS_INDIRECT_ONLY ) ? true : false;
		Assert( obj.pRenderable );
		// Should call GetColorModuation or use engine global?  Look at other code (r_color)
		// diffuse color modulation
		obj.pRenderable->GetColorModulation( instance.m_DiffuseModulation.AsVector3D().Base() );
		// alpha modulation
		instance.m_DiffuseModulation.w = obj.alpha * ( 1.0f / 255.0f );
		memcpy( instance.m_pPoseToWorld, obj.pMatrix, sizeof( pPoseToWorld[0] ) );
		colorMeshes[nInstanceCount] = obj.pColorMeshes;
		++nInstanceCount;
	}
	if ( nInstanceCount > 0 )
	{
		g_pStudioRender->DrawModelArrayStaticProp( info, nInstanceCount, instanceData, colorMeshes );
	}
	rdPoseToWorld.Release();
	}//g_VProfCurrentProfile.ExitScope();

	// now render the vertex lit props
	int				nLocalLightCount = 0;
	LightDesc_t		localLightDescs[4];
	float			vColorModulation[3] = {1.0f, 1.0f, 1.0f};

	drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL | STUDIORENDER_DRAW_STATIC_LIGHTING;
	if ( lightObjects.Count() )
	{
		for ( int i = 0; i < lightObjects.Count(); i++ )
		{
			VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "render vertex lit prop");
			robject_t &obj = objectList[lightObjects[i]];
			rmodel_t &model = modelList[obj.modelIndex];

			if ( obj.pEnvCubeMap )
			{
				VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "BindLocalCubemap");
				pRenderContext->BindLocalCubemap( obj.pEnvCubeMap );
			}

			LightingState_t *pState = &lightStates[obj.lightIndex];
			{
				VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "SetAmbientLightColors and origin");
				g_pStudioRender->SetAmbientLightColors( pState->r_boxcolor );
				pRenderContext->SetLightingOrigin( *obj.pLightingOrigin );
			}
			{
				VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "R_SetNonAmbientLightingState");
				R_SetNonAmbientLightingState( pState->numlights, pState->locallight, &nLocalLightCount, localLightDescs, true );
			}

			//Include static model specific tinting.
			obj.pRenderable->GetColorModulation( vColorModulation );
			g_pStudioRender->SetColorModulation( vColorModulation );

			info.m_pStudioHdr = model.pStudioHdr;
			info.m_pHardwareData = model.pStudioHWData;
			info.m_Skin = obj.skin;
			info.m_pClientEntity = static_cast<void*>(obj.pRenderable);
			info.m_Lod = obj.lod;
			info.m_pColorMeshes = obj.pColorMeshes;
			g_pStudioRender->DrawModelStaticProp( info, *obj.pMatrix, drawFlags );
		}
	}

	if ( !g_pShadowMgr->SinglePassFlashlightModeEnabled() && shadowObjects.Count() )
	{
		drawFlags = STUDIORENDER_DRAW_ENTIRE_MODEL;
		for ( int i = 0; i < shadowObjects.Count(); i++ )
		{
			VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "render shadow objects");
			// draw just the shadows!
			robject_t &obj = objectList[shadowObjects[i]];
			rmodel_t &model = modelList[obj.modelIndex];
			g_pShadowMgr->SetModelShadowState( obj.instance );

			//Include static model specific tinting.
			obj.pRenderable->GetColorModulation( vColorModulation );
			g_pStudioRender->SetColorModulation( vColorModulation );

			info.m_pStudioHdr = model.pStudioHdr;
			info.m_pHardwareData = model.pStudioHWData;
			info.m_Skin = obj.skin;
			info.m_pClientEntity = static_cast<void*>(obj.pRenderable);
			info.m_Lod = obj.lod;
			info.m_pColorMeshes = obj.pColorMeshes;
			g_pStudioRender->DrawStaticPropShadows( info, *obj.pMatrix, drawFlags );
		}
		g_pStudioRender->ClearAllShadows();
	}

	for ( int i = 0; i < decalObjects.Count(); i++ )
	{
		VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "render decal objects");
		// draw just the decals!
		robject_t &obj = objectList[decalObjects[i].objectIndex];
		rmodel_t &model = modelList[obj.modelIndex];
		LightingState_t *pState = &lightStates[decalObjects[i].lightIndex];
		g_pStudioRender->SetAmbientLightColors( pState->r_boxcolor );
		pRenderContext->SetLightingOrigin( *obj.pLightingOrigin );
		R_SetNonAmbientLightingState( pState->numlights, pState->locallight, &nLocalLightCount, localLightDescs, true );


		//Include static model specific tinting.
		obj.pRenderable->GetColorModulation( vColorModulation );
		g_pStudioRender->SetColorModulation( vColorModulation );


		info.m_pStudioHdr = model.pStudioHdr;
		info.m_pHardwareData = model.pStudioHWData;
		info.m_Decals = m_ModelInstances[obj.instance].m_DecalHandle;
		info.m_Skin = obj.skin;
		info.m_pClientEntity = static_cast<void*>(obj.pRenderable);
		info.m_Lod = obj.lod;
		info.m_pColorMeshes = obj.pColorMeshes;
		g_pStudioRender->DrawStaticPropDecals( info, *obj.pMatrix );
	}

	// Restore the matrices if we're skinning
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
	return drawnCount;
#else // DEDICATED
	return 0;
#endif // DEDICATED
}

//-----------------------------------------------------------------------------
// Shadow rendering
//-----------------------------------------------------------------------------
matrix3x4a_t* CModelRender::DrawModelShadowSetup( IClientRenderable *pRenderable, int body, int skin, DrawModelInfo_t *pInfo, matrix3x4a_t *pCustomBoneToWorld )
{
#ifndef DEDICATED
	DrawModelInfo_t &info = *pInfo;
	static ConVar r_shadowlod("r_shadowlod", "-1");
	static ConVar r_shadowlodbias("r_shadowlodbias", "2");

	model_t const* pModel = pRenderable->GetModel();
	if ( !pModel )
		return NULL;

	// FIXME: Make brush shadows work
	if ( pModel->type != mod_studio )
		return NULL;

	Assert( modelloader->IsLoaded( pModel ) && ( pModel->type == mod_studio ) );

	info.m_pStudioHdr = g_pMDLCache->GetStudioHdr( pModel->studio );
	info.m_pColorMeshes = NULL;

	// quick exit
	if (info.m_pStudioHdr->numbodyparts == 0)
		return NULL;

	Assert ( pRenderable );
	info.m_pHardwareData = g_pMDLCache->GetHardwareData( pModel->studio );
	if ( !info.m_pHardwareData )
		return NULL;

	info.m_Decals = STUDIORENDER_DECAL_INVALID;
	info.m_Skin = skin;
	info.m_Body = body;
	info.m_pClientEntity = (void*)pRenderable;
	info.m_HitboxSet = 0;

	CMatRenderContextPtr pRenderContext( materials );
	info.m_Lod = r_shadowlod.GetInt();
	// If the .mdl has a shadowlod, force the use of that one instead
	if ( info.m_pStudioHdr->flags & STUDIOHDR_FLAGS_HASSHADOWLOD )
	{
		info.m_Lod = info.m_pHardwareData->m_NumLODs-1;
	}
	else if ( info.m_Lod == USESHADOWLOD )
	{
		int lastlod = info.m_pHardwareData->m_NumLODs - 1;
		info.m_Lod = lastlod;
	}
	else if ( info.m_Lod < 0 )
	{
		// Compute the shadow LOD...
		float factor = r_shadowlodbias.GetFloat() > 0.0f ? 1.0f / r_shadowlodbias.GetFloat() : 1.0f;
		float screenSize = factor * pRenderContext->ComputePixelWidthOfSphere( pRenderable->GetRenderOrigin(), 0.5f );
		info.m_Lod = g_pStudioRender->ComputeModelLod( info.m_pHardwareData, screenSize ); 
		info.m_Lod = info.m_pHardwareData->m_NumLODs-2;
		if ( info.m_Lod < 0 )
		{
			info.m_Lod = 0;
		}
	}

	// clamp to root lod
	if (info.m_Lod < info.m_pHardwareData->m_RootLOD)
	{
		info.m_Lod = info.m_pHardwareData->m_RootLOD;
	}

	matrix3x4a_t *pBoneToWorld = pCustomBoneToWorld;
	CMatRenderData< matrix3x4a_t > rdBoneToWorld( pRenderContext );
	if ( !pBoneToWorld )
	{
		pBoneToWorld = rdBoneToWorld.Lock( info.m_pStudioHdr->numbones );
	}
	const bool bOk = pRenderable->SetupBones( pBoneToWorld, info.m_pStudioHdr->numbones, BONE_USED_BY_VERTEX_AT_LOD(info.m_Lod), GetBaseLocalClient().GetTime() );
	if ( !bOk )
		return NULL;
	return pBoneToWorld;
#else
	return NULL;
#endif
}

void CModelRender::DrawModelShadow( IClientRenderable *pRenderable, const DrawModelInfo_t &info, matrix3x4a_t *pBoneToWorld )
{
#ifndef DEDICATED
	// Needed because we don't call SetupWeights
	g_pStudioRender->SetEyeViewTarget( info.m_pStudioHdr, info.m_Body, vec3_origin );

	// Color + alpha modulation
	Vector white( 1, 1, 1 );
	g_pStudioRender->SetColorModulation( white.Base() );
	g_pStudioRender->SetAlphaModulation( 1.0f );

	if ((info.m_pStudioHdr->flags & STUDIOHDR_FLAGS_USE_SHADOWLOD_MATERIALS) == 0)
	{
		g_pStudioRender->ForcedMaterialOverride( g_pMaterialShadowBuild, OVERRIDE_BUILD_SHADOWS );
	}

	g_pStudioRender->DrawModel( NULL, info, pBoneToWorld, NULL, NULL, pRenderable->GetRenderOrigin(),
		STUDIORENDER_DRAW_NO_SHADOWS | STUDIORENDER_DRAW_ENTIRE_MODEL | STUDIORENDER_DRAW_NO_FLEXES );
	g_pStudioRender->ForcedMaterialOverride( 0 );
#endif
}

void  CModelRender::SetViewTarget( const CStudioHdr *pStudioHdr, int nBodyIndex, const Vector& target )
{
	g_pStudioRender->SetEyeViewTarget( pStudioHdr->GetRenderHdr(), nBodyIndex, target );
}

void CModelRender::InitColormeshParams( ModelInstance_t &instance, studiohwdata_t *pStudioHWData, colormeshparams_t *pColorMeshParams )
{
	pColorMeshParams->m_nMeshes = 0;
	pColorMeshParams->m_nTotalVertexes = 0;
	pColorMeshParams->m_pPooledVBAllocator = NULL;

	if ( ( instance.m_nFlags & MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR ) &&
		g_pMaterialSystemHardwareConfig->SupportsStreamOffset() &&
		 ( r_proplightingpooling.GetInt() == 1 ) )
	{
		// Color meshes can be allocated in a shared pool for static props
		// (saves memory on X360 due to 4-KB VB alignment)
		pColorMeshParams->m_pPooledVBAllocator = (IPooledVBAllocator *)&m_colorMeshVBAllocator;
	}

	for ( int lodID = pStudioHWData->m_RootLOD; lodID < pStudioHWData->m_NumLODs; lodID++ )
	{
		studioloddata_t *pLOD = &pStudioHWData->m_pLODs[lodID];
		for ( int meshID = 0; meshID < pStudioHWData->m_NumStudioMeshes; meshID++ )
		{
			studiomeshdata_t *pMesh = &pLOD->m_pMeshData[meshID];
			for ( int groupID = 0; groupID < pMesh->m_NumGroup; groupID++ )
			{
				pColorMeshParams->m_nVertexes[pColorMeshParams->m_nMeshes++] = pMesh->m_pMeshGroup[groupID].m_NumVertices;
				Assert( pColorMeshParams->m_nMeshes <= ARRAYSIZE( pColorMeshParams->m_nVertexes ) );

				pColorMeshParams->m_nTotalVertexes += pMesh->m_pMeshGroup[groupID].m_NumVertices;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Allocates the static prop color data meshes
//-----------------------------------------------------------------------------
// FIXME? : Move this to StudioRender?
CColorMeshData *CModelRender::FindOrCreateStaticPropColorData( ModelInstanceHandle_t handle )
{
	if ( handle == MODEL_INSTANCE_INVALID )
	{
		// the card can't support it
		return NULL;
	}

	ModelInstance_t& instance = m_ModelInstances[handle];
	CColorMeshData *pColorMeshData = CacheGet( instance.m_ColorMeshHandle );
	if ( pColorMeshData )
	{
		// found in cache
		return pColorMeshData;
	}

	Assert( instance.m_pModel );
	if ( !instance.m_pModel )
	{
		// Avoid crash in mat_reloadallmaterials
		return NULL;
	}

	Assert( modelloader->IsLoaded( instance.m_pModel ) && ( instance.m_pModel->type == mod_studio ) );
	studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( instance.m_pModel->studio );
	Assert( pStudioHWData );
	if ( !pStudioHWData )
	{
		char fn[ MAX_PATH ];
		g_pFullFileSystem->String( instance.m_pModel->fnHandle, fn, sizeof( fn ) );
		Sys_Error( "g_pMDLCache->GetHardwareData failed for %s\n", fn );
		return NULL;
	}

	colormeshparams_t params;
	InitColormeshParams( instance, pStudioHWData, &params );
	if ( params.m_nMeshes <= 0 )
	{
		// nothing to create
		return NULL;
	}

	// create the meshes
	params.m_fnHandle = instance.m_pModel->fnHandle;
	instance.m_ColorMeshHandle = CacheCreate( params );
	ProtectColorDataIfQueued( instance.m_ColorMeshHandle );
	pColorMeshData = CacheGet( instance.m_ColorMeshHandle );

	return pColorMeshData;
}

//-----------------------------------------------------------------------------
// Allocates the static prop color data meshes
//-----------------------------------------------------------------------------
// FIXME? : Move this to StudioRender?
void CModelRender::ProtectColorDataIfQueued( DataCacheHandle_t hColorMesh )
{
	if ( hColorMesh != DC_INVALID_HANDLE)
	{
		CMatRenderContextPtr pRenderContext( materials );
		ICallQueue *pCallQueue = pRenderContext->GetCallQueue();
		if ( pCallQueue )
		{
			if ( CacheLock( hColorMesh ) ) // CacheCreate above will call functions that won't take place until later. If color mesh isn't used right away, it could get dumped
			{
				pCallQueue->QueueCall( this, &CModelRender::CacheUnlock, hColorMesh );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Old-style computation of vertex lighting ( Currently In Use )
//-----------------------------------------------------------------------------
void CModelRender::ComputeModelVertexLightingOld( mstudiomodel_t *pModel, 
	matrix3x4_t& matrix, const LightingState_t &lightingState, color24 *pLighting,
	bool bUseConstDirLighting, float flConstDirLightAmount )
{
	Vector			worldPos, worldNormal, destColor;
	int				nNumLightDesc;
	LightDesc_t		lightDesc[MAXLOCALLIGHTS];
	LightingState_t *pLightingState;

	pLightingState = (LightingState_t*)&lightingState;

	// build the lighting descriptors
	R_SetNonAmbientLightingState( pLightingState->numlights, pLightingState->locallight, &nNumLightDesc, lightDesc, false );

	if ( IsGameConsole() )
	{
		// On console, we depend on VRAD to compute ALL prop lighting, so this model must have been
		// changed since the BSP was last rebuilt - flag it visually by setting it to fullbright.
		for ( int i = 0; i < pModel->numvertices; ++i )
		{
			pLighting[i].r = 255;
			pLighting[i].g = 255;
			pLighting[i].b = 255;
		}
		return;
	}

	const thinModelVertices_t		*thinVertData	= NULL;
	const mstudio_modelvertexdata_t	*vertData		= pModel->GetVertexData();
	mstudiovertex_t					*pFatVerts		= NULL;
	if ( vertData )
	{
		pFatVerts = vertData->Vertex( 0 );
	}
	else
	{
		thinVertData = pModel->GetThinVertexData();
		Assert( thinVertData );
		if ( !thinVertData )
			return;
	}

	bool bHasSSE = MathLib_SSEEnabled();

	// light all vertexes
	for ( int i = 0; i < pModel->numvertices; ++i )
	{
		if ( vertData )
		{
#ifdef _WIN32
			if ( bHasSSE )
			{
				// hint the next vertex
				// data is loaded with one extra vertex for read past
#if !defined( _X360 ) // X360TBD
				_mm_prefetch( (char*)&pFatVerts[i+1], _MM_HINT_T0 );
#endif
			}
#endif

			VectorTransform( pFatVerts[i].m_vecPosition, matrix, worldPos );
			VectorRotate( pFatVerts[i].m_vecNormal, matrix, worldNormal );
		}
		else
		{
			Vector position;
			Vector normal;
			thinVertData->GetModelPosition(	pModel, i, &position );
			thinVertData->GetModelNormal( pModel, i, &normal );
			VectorTransform( position, matrix, worldPos );
			VectorRotate( normal, matrix, worldNormal );
		}

		if ( bUseConstDirLighting )
		{
			g_pStudioRender->ComputeLightingConstDirectional( pLightingState->r_boxcolor,
				nNumLightDesc, lightDesc, worldPos, worldNormal, destColor, flConstDirLightAmount );
		}
		else
		{
            g_pStudioRender->ComputeLighting( pLightingState->r_boxcolor,
				nNumLightDesc, lightDesc, worldPos, worldNormal, destColor );
		}
		
		// to gamma space
		destColor[0] = LinearToVertexLight( destColor[0] );
		destColor[1] = LinearToVertexLight( destColor[1] );
		destColor[2] = LinearToVertexLight( destColor[2] );

		Assert( (destColor[0] >= 0.0f) && (destColor[0] <= 1.0f) );
		Assert( (destColor[1] >= 0.0f) && (destColor[1] <= 1.0f) );
		Assert( (destColor[2] >= 0.0f) && (destColor[2] <= 1.0f) );

		pLighting[i].r = FastFToC(destColor[0]);
		pLighting[i].g = FastFToC(destColor[1]);
		pLighting[i].b = FastFToC(destColor[2]);
	}
}


//-----------------------------------------------------------------------------
// New-style computation of vertex lighting ( Not Used Yet )
//-----------------------------------------------------------------------------
void CModelRender::ComputeModelVertexLighting( IHandleEntity *pProp, 
	mstudiomodel_t *pModel, OptimizedModel::ModelLODHeader_t *pVtxLOD,
	matrix3x4_t& matrix, Vector4D *pTempMem, color24 *pLighting )
{
#ifndef DEDICATED
	if ( IsGameConsole() )
		return;

	int i;
	unsigned char *pInSolid = (unsigned char*)stackalloc( ((pModel->numvertices + 7) >> 3) * sizeof(unsigned char) );
	Vector worldPos, worldNormal;

	const mstudio_modelvertexdata_t *vertData = pModel->GetVertexData();
	Assert( vertData );
	if ( !vertData )
		return;

	for ( i = 0; i < pModel->numvertices; ++i )
	{
		const Vector &pos = *vertData->Position( i );
		const Vector &normal = *vertData->Normal( i );
		VectorTransform( pos, matrix, worldPos );
		VectorRotate( normal, matrix, worldNormal );
		bool bNonSolid = ComputeVertexLightingFromSphericalSamples( worldPos, worldNormal, pProp, &(pTempMem[i].AsVector3D()) );
		
		int nByte = i >> 3;
		int nBit = i & 0x7;

		if ( bNonSolid )
		{
			pTempMem[i].w = 1.0f;
			pInSolid[ nByte ] &= ~(1 << nBit);
		}
		else
		{
			pTempMem[i].Init( );
			pInSolid[ nByte ] |= (1 << nBit);
		}
	}

	// Must iterate over each triangle to average out the colors for those
	// vertices in solid.
	// Iterate over all the meshes....
	for (int meshID = 0; meshID < pModel->nummeshes; ++meshID)
	{
		Assert( pModel->nummeshes == pVtxLOD->numMeshes );
		mstudiomesh_t* pMesh = pModel->pMesh(meshID);
		OptimizedModel::MeshHeader_t* pVtxMesh = pVtxLOD->pMesh(meshID);

		// Iterate over all strip groups.
		for( int stripGroupID = 0; stripGroupID < pVtxMesh->numStripGroups; ++stripGroupID )
		{
			OptimizedModel::StripGroupHeader_t* pStripGroup = pVtxMesh->pStripGroup(stripGroupID);
			
			// Iterate over all indices
			Assert( pStripGroup->numIndices % 3 == 0 );
			for (i = 0; i < pStripGroup->numIndices; i += 3)
			{
				unsigned short nIndex1 = *pStripGroup->pIndex( i );
				unsigned short nIndex2 = *pStripGroup->pIndex( i + 1 );
				unsigned short nIndex3 = *pStripGroup->pIndex( i + 2 );

				int v[3];
				v[0] = pStripGroup->pVertex( nIndex1 )->origMeshVertID + pMesh->vertexoffset;
				v[1] = pStripGroup->pVertex( nIndex2 )->origMeshVertID + pMesh->vertexoffset;
				v[2] = pStripGroup->pVertex( nIndex3 )->origMeshVertID + pMesh->vertexoffset;

				Assert( v[0] < pModel->numvertices );
				Assert( v[1] < pModel->numvertices );
				Assert( v[2] < pModel->numvertices );

				bool bSolid[3];
				bSolid[0] = ( pInSolid[ v[0] >> 3 ] & ( 1 << ( v[0] & 0x7 ) ) ) != 0;
				bSolid[1] = ( pInSolid[ v[1] >> 3 ] & ( 1 << ( v[1] & 0x7 ) ) ) != 0;
				bSolid[2] = ( pInSolid[ v[2] >> 3 ] & ( 1 << ( v[2] & 0x7 ) ) ) != 0;

				int nValidCount = 0;
				int nAverage[3];
				if ( !bSolid[0] ) { nAverage[nValidCount++] = v[0]; }
				if ( !bSolid[1] ) { nAverage[nValidCount++] = v[1]; }
				if ( !bSolid[2] ) { nAverage[nValidCount++] = v[2]; }

				if ( nValidCount == 3 )
					continue;

				Vector vecAverage( 0, 0, 0 );
				for ( int j = 0; j < nValidCount; ++j )
				{
					vecAverage += pTempMem[nAverage[j]].AsVector3D();
				}

				if (nValidCount != 0)
				{
					vecAverage /= nValidCount;
				}

				if ( bSolid[0] ) { pTempMem[ v[0] ].AsVector3D() += vecAverage; pTempMem[ v[0] ].w += 1.0f; }
				if ( bSolid[1] ) { pTempMem[ v[1] ].AsVector3D() += vecAverage; pTempMem[ v[1] ].w += 1.0f; }
				if ( bSolid[2] ) { pTempMem[ v[2] ].AsVector3D() += vecAverage; pTempMem[ v[2] ].w += 1.0f; }
			}
		}
	}

	Vector destColor;
	for ( i = 0; i < pModel->numvertices; ++i )
	{
		if ( pTempMem[i].w != 0.0f )
		{
			pTempMem[i] /= pTempMem[i].w;
		}

		destColor[0] = LinearToVertexLight( pTempMem[i][0] );
		destColor[1] = LinearToVertexLight( pTempMem[i][1] );
		destColor[2] = LinearToVertexLight( pTempMem[i][2] );

		ColorClampTruncate( destColor );

		pLighting[i].r = FastFToC(destColor[0]);
		pLighting[i].g = FastFToC(destColor[1]);
		pLighting[i].b = FastFToC(destColor[2]);
	}
#endif
}

//-----------------------------------------------------------------------------
// Sanity check and setup the compiled color mesh for an optimal async load
// during runtime.
//-----------------------------------------------------------------------------
void CModelRender::ValidateStaticPropColorData( ModelInstanceHandle_t handle )
{
	if ( !r_proplightingfromdisk.GetBool() )
	{
		return;
	}

	ModelInstance_t *pInstance = &m_ModelInstances[handle];
	IHandleEntity* pProp = pInstance->m_pRenderable->GetIClientUnknown();

	if ( !StaticPropMgr()->IsStaticProp( pProp ) )
	{
		// can't support it or not a static prop
		return;
	}

	if ( !g_bLoadedMapHasBakedPropLighting )
	{
		return;
	}

	MEM_ALLOC_CREDIT();

	// fetch the header
	CUtlBuffer utlBuf;
	char fileName[MAX_PATH];
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE || g_bBakedPropLightingNoSeparateHDR )
	{
		Q_snprintf( fileName, sizeof( fileName ), "sp_%d%s.vhv", StaticPropMgr()->GetStaticPropIndex( pProp ), GetPlatformExt() );
	}
	else
	{	
		Q_snprintf( fileName, sizeof( fileName ), "sp_hdr_%d%s.vhv", StaticPropMgr()->GetStaticPropIndex( pProp ), GetPlatformExt() );
	}

	if ( IsGameConsole()  )
	{
		DataCacheHandle_t hColorMesh = GetCachedStaticPropColorData( fileName );
		if ( hColorMesh != DC_INVALID_HANDLE )
		{
			// already have it
			pInstance->m_ColorMeshHandle = hColorMesh;
			pInstance->m_nFlags &= ~MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD;
			pInstance->m_nFlags |= MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR;
			return;
		}
	}

	if ( !g_pFileSystem->ReadFile( fileName, "GAME", utlBuf, sizeof( HardwareVerts::FileHeader_t ), 0 ) )
	{
		// not available
		return;
	}

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( pInstance->m_pModel->studio );

	static ConVarRef r_staticlight_streams( "r_staticlight_streams" );
	unsigned int numLightingComponents = r_staticlight_streams.GetInt();

	HardwareVerts::FileHeader_t *pVhvHdr = (HardwareVerts::FileHeader_t *)utlBuf.Base();

	// total the static prop verts
	int numStudioHdrVerts = 0;
	studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( pInstance->m_pModel->studio );
	if ( pStudioHWData != NULL )
	{
		for ( int lodID = pStudioHWData->m_RootLOD; lodID < pStudioHWData->m_NumLODs; lodID++ )
		{
			studioloddata_t *pLOD = &pStudioHWData->m_pLODs[lodID];
			for ( int meshID = 0; meshID < pStudioHWData->m_NumStudioMeshes; meshID++ )
			{
				studiomeshdata_t *pMesh = &pLOD->m_pMeshData[meshID];
				for ( int groupID = 0; groupID < pMesh->m_NumGroup; groupID++ )
				{
					numStudioHdrVerts += pMesh->m_pMeshGroup[groupID].m_NumVertices;
				}
			}
		}
	}

	if ( ( pVhvHdr->m_nVersion != VHV_VERSION ) || 
		 ( ( pVhvHdr->m_nChecksum != (unsigned int)pStudioHdr->checksum ) && ( !r_ignoreStaticColorChecksum.GetBool() ) ) || 
		 ( pStudioHWData ? ( pVhvHdr->m_nVertexes != numStudioHdrVerts ) : ( pVhvHdr->m_nChecksum != (unsigned int)pStudioHdr->checksum ) ) ||
		 ( pVhvHdr->m_nVertexSize != 4 * numLightingComponents ) )
	{
		// out of sync
		// mark for debug visualization
		pInstance->m_nFlags |= MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD;
		return;
	}

	// async callback can safely stream data into targets
	pInstance->m_nFlags &= ~MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD;
	pInstance->m_nFlags |= MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR;
}

//-----------------------------------------------------------------------------
// Async loader callback
// Called from async i/o thread - must spend minimal cycles in this context
//-----------------------------------------------------------------------------
void CModelRender::StaticPropColorMeshCallback( void *pContext, const void *pData, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	#if defined( DEVELOPMENT_ONLY ) || defined( ALLOW_TEXT_MODE )
		static bool s_bTextMode = CommandLine()->HasParm( "-textmode" );
	#else
		static bool s_bTextMode = false;
	#endif

	// get our preserved data
	Assert( pContext );
	staticPropAsyncContext_t *pStaticPropContext = (staticPropAsyncContext_t *)pContext;

	HardwareVerts::FileHeader_t *pVhvHdr;
	byte *pOriginalData = NULL;

	static ConVarRef r_staticlight_streams( "r_staticlight_streams" );

	unsigned int accSunAmount = 0;

	if ( asyncStatus != FSASYNC_OK )
	{
		// any i/o error
		goto cleanUp;
	}

	if ( IsGameConsole() )
	{
		// only the 360 has compressed VHV data
		CLZMA lzma;

		// the compressed data is after the header
		byte *pCompressedData = (byte *)pData + sizeof( HardwareVerts::FileHeader_t );
		if ( lzma.IsCompressed( pCompressedData ) )
		{
			// create a buffer that matches the original
			int actualSize = lzma.GetActualSize( pCompressedData );
			pOriginalData = (byte *)malloc( sizeof( HardwareVerts::FileHeader_t ) + actualSize );

			// place the header, then uncompress directly after it
			V_memcpy( pOriginalData, pData, sizeof( HardwareVerts::FileHeader_t ) );
			int outputLength = lzma.Uncompress( pCompressedData, pOriginalData + sizeof( HardwareVerts::FileHeader_t ) );
			if ( outputLength != actualSize )
			{
				goto cleanUp;
			}
			pData = pOriginalData;
		}
	}

	pVhvHdr = (HardwareVerts::FileHeader_t *)pData;

	int startMesh;
	for ( startMesh=0; startMesh<pVhvHdr->m_nMeshes; startMesh++ )
	{
		// skip past higher detail lod meshes that must be ignored
		// find first mesh that matches desired lod
		if ( pVhvHdr->pMesh( startMesh )->m_nLod == pStaticPropContext->m_nRootLOD )
		{
			break;
		}
	}

	int meshID;
	int numLightingComponents;
	numLightingComponents = r_staticlight_streams.GetInt();

	for ( meshID = startMesh; meshID<pVhvHdr->m_nMeshes; meshID++ )
	{
		int numVertexes = pVhvHdr->pMesh( meshID )->m_nVertexes;
		if ( numVertexes != pStaticPropContext->m_pColorMeshData->m_pMeshInfos[meshID-startMesh].m_nNumVerts )
		{
			// meshes are out of sync, discard data
			break;
		}
#if defined( DX_TO_GL_ABSTRACTION )
		int nID = meshID-startMesh;

		unsigned char *pIn = (unsigned char *) pVhvHdr->pVertexBase( meshID );
		//			unsigned char *pOut = pStaticPropContext->m_pColorMeshData->m_ppTargets[ nID ];
		unsigned char *pOut = NULL;

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pStaticPropContext->m_pColorMeshData->m_pMeshInfos[ nID ].m_pMesh, MATERIAL_HETEROGENOUS, numVertexes, 0 );
		if ( numLightingComponents > 1 )
		{
			pOut = reinterpret_cast< unsigned char * >( const_cast< float * >( meshBuilder.Normal() ) );
		}
		else
		{
			pOut = meshBuilder.Specular();
		}

		// OPENGL_SWAP_COLORS
		// If we are in text mode, we don't actually have backing for this memory so we can't write to it.
		if ( !s_bTextMode )
		{
			for ( int i=0; i < (numVertexes * numLightingComponents ); i++ )
			{
				unsigned char red = *pIn++;
				unsigned char green = *pIn++;
				unsigned char blue = *pIn++;
	
				*pOut++ = blue;
				*pOut++ = green;
				*pOut++ = red;
				*pOut++ = *pIn++; // Alpha goes straight across
			}
		}

		pIn = (unsigned char *)pVhvHdr->pVertexBase( meshID );
		if ( g_pMaterialSystemHardwareConfig->GetCSMAccurateBlending() )
		{
			for ( int i = 0; i < numVertexes; i++ )
			{
				int vertexSunAmount = 0;
				for ( int j = 0; j < numLightingComponents; j++ )
				{
					vertexSunAmount += (unsigned int)(pIn[3]);
					pIn += 4;
				}
				accSunAmount += vertexSunAmount / numLightingComponents;
			}
		}
		else
		{
			for ( int i = 0; i < (numVertexes * numLightingComponents); i++ )
			{
				accSunAmount += 255 - (unsigned int)(pIn[3]);
				pIn += 4;
			}
		}

		meshBuilder.End();
#elif defined( _PS3 )
		// CELL_GCM_SWAP_COLORS
		unsigned char *pIn = (unsigned char *) pVhvHdr->pVertexBase( meshID );
		unsigned char *pOut = pStaticPropContext->m_pColorMeshData->m_ppTargets[meshID-startMesh];

		for ( int i=0; i < (numVertexes * numLightingComponents ); i++ )
		{
			unsigned char red = *pIn++;
			unsigned char green = *pIn++;
			unsigned char blue = *pIn++;
			unsigned char alpha = *pIn++;
			*pOut++ = alpha;
			*pOut++ = blue;
			*pOut++ = green;
			*pOut++ = red;
		}
#else
		V_memcpy( (void*)pStaticPropContext->m_pColorMeshData->m_ppTargets[meshID-startMesh], pVhvHdr->pVertexBase( meshID ), numVertexes*4*numLightingComponents );
	
		unsigned char *pIn = (unsigned char *)pVhvHdr->pVertexBase( meshID );
		if ( g_pMaterialSystemHardwareConfig->GetCSMAccurateBlending() )
		{
			for ( int i = 0; i < numVertexes; i++ )
			{
				int vertexSunAmount = 0;
				for ( int j = 0; j < numLightingComponents; j++ )
				{
					vertexSunAmount += (unsigned int)(pIn[3]);
					pIn += 4;
				}
				accSunAmount += vertexSunAmount / numLightingComponents;
			}
		}
		else
		{
			for ( int i = 0; i < (numVertexes * numLightingComponents); i++ )
			{
				accSunAmount += 255 - (unsigned int)(pIn[3]);
				pIn += 4;
			}
		}
#endif
	}
	
	// crude way to determine whether static prop makes any visual contribution to csm's
	// accumulate contribution from sun (csm casting light) - stored in the alpha channel of each color, already used for blending CSM with baked vertex lighting in shaders
	// total contribution of zero => totally in shadow, take this to mean zero contribution and cull when rendering CSM's
	// could also switch off CSM combo for this prop, but combo is static right now
	// TODO: move the above accumulation and flag setting to vrad
	if ( accSunAmount == 0 ) 
	{
		StaticPropMgr()->DisableCSMRenderingForStaticProp( pStaticPropContext->m_StaticPropIndex );
	}

cleanUp:
	if ( IsGameConsole() )
	{
		AUTO_LOCK_FM( m_CachedStaticPropMutex );
		// track the color mesh's datacache handle so that we can find it long after the model instance's are gone
		// the static prop filenames are guaranteed uniquely decorated
		m_CachedStaticPropColorData.Insert( pStaticPropContext->m_szFilename, pStaticPropContext->m_ColorMeshHandle );
	}

	// mark as completed in single atomic operation
	pStaticPropContext->m_pColorMeshData->m_bColorMeshValid = true;
	CacheUnlock( pStaticPropContext->m_ColorMeshHandle );
	
	AssertMsgOnce( CacheGet( pStaticPropContext->m_ColorMeshHandle ), "ERROR! Failed to cache static prop color data!" );

	delete pStaticPropContext;

	if ( pOriginalData )
	{
		free( pOriginalData );
	}
}

//-----------------------------------------------------------------------------
// Async loader callback
// Called from async i/o thread - must spend minimal cycles in this context
//-----------------------------------------------------------------------------
static void StaticPropColorMeshCallback( const FileAsyncRequest_t &request, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	s_ModelRender.StaticPropColorMeshCallback( request.pContext, request.pData, numReadBytes, asyncStatus );
}

//-----------------------------------------------------------------------------
// Queued loader callback
// Called from async i/o thread - must spend minimal cycles in this context
//-----------------------------------------------------------------------------
static void QueuedLoaderCallback_PropLighting( void *pContext, void *pContext2, const void *pData, int nSize, LoaderError_t loaderError )
{
	// translate error
	FSAsyncStatus_t asyncStatus = ( loaderError == LOADERERROR_NONE ? FSASYNC_OK : FSASYNC_ERR_READING );

	// mimic async i/o completion
	s_ModelRender.StaticPropColorMeshCallback( pContext, pData, nSize, asyncStatus );
}

//-----------------------------------------------------------------------------
// Loads the serialized static prop color data.
// Returns false if legacy path should be used.
//-----------------------------------------------------------------------------
bool CModelRender::LoadStaticPropColorData( IHandleEntity *pProp, DataCacheHandle_t colorMeshHandle, studiohwdata_t *pStudioHWData )
{
	if ( !g_bLoadedMapHasBakedPropLighting || !r_proplightingfromdisk.GetBool() )
	{
		return false;
	}

	// lock the mesh memory during async transfer
	// the color meshes should already have low quality data to be used during rendering
	CColorMeshData *pColorMeshData = CacheLock( colorMeshHandle );
	if ( !pColorMeshData )
	{
		return false;
	}

	if ( IsGameConsole() && pColorMeshData->m_bColorMeshValid )
	{
		// This prevents excessive pointless i/o of the same data.
		// For the 360, the disk bits are invariant (HDR only), no reason to reload.
		// The loading pattern causes these to hit multiple times, the first time is correct (color meshes are invalid)
		// due to LevelInitClient(). The additional calls are due to RecomputeStaticLighting() due to lighting config
		// chage/dirty that is tripped at conclusion of load. 
		CacheUnlock( colorMeshHandle );
		return true;
	}

	if ( pColorMeshData->m_hAsyncControl )
	{
		// load in progress, ignore additional request 
		// or already loaded, ignore until discarded from cache
		CacheUnlock( colorMeshHandle );
		return true;
	}

	// each static prop has its own compiled color mesh
	char fileName[MAX_PATH];
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE || g_bBakedPropLightingNoSeparateHDR )
	{
        Q_snprintf( fileName, sizeof( fileName ), "sp_%d%s.vhv", StaticPropMgr()->GetStaticPropIndex( pProp ), GetPlatformExt() );
	}
	else
	{
        Q_snprintf( fileName, sizeof( fileName ), "sp_hdr_%d%s.vhv", StaticPropMgr()->GetStaticPropIndex( pProp ), GetPlatformExt() );
	}

	// mark as invalid, async callback will set upon completion
	// prevents rendering during async transfer into locked mesh, otherwise d3drip
	pColorMeshData->m_bColorMeshValid = false;

	// async load high quality lighting from file
	// can't optimal async yet, because need flat ppColorMesh[], so use callback to distribute
	// create our private context of data for the callback
	staticPropAsyncContext_t *pContext = new staticPropAsyncContext_t;
	pContext->m_nRootLOD = pStudioHWData->m_RootLOD;
	pContext->m_nMeshes = pColorMeshData->m_nMeshes;
	pContext->m_ColorMeshHandle = colorMeshHandle;
	pContext->m_pColorMeshData = pColorMeshData;
	pContext->m_StaticPropIndex = StaticPropMgr()->GetStaticPropIndex( pProp );
	V_strncpy( pContext->m_szFilename, fileName, sizeof( pContext->m_szFilename ) );

	if ( IsGameConsole() && g_pQueuedLoader->IsMapLoading() )
	{
		if ( !g_pQueuedLoader->ClaimAnonymousJob( fileName, QueuedLoaderCallback_PropLighting, (void *)pContext ) )
		{
			// not there as expected
			// as a less optimal fallback during loading, issue as a standard queued loader job
			LoaderJob_t loaderJob;
			loaderJob.m_pFilename = fileName;
			loaderJob.m_pPathID = "GAME";
			loaderJob.m_pCallback = QueuedLoaderCallback_PropLighting;
			loaderJob.m_pContext = (void *)pContext;
			loaderJob.m_Priority = LOADERPRIORITY_BEFOREPLAY;
			g_pQueuedLoader->AddJob( &loaderJob );
		}
		return true;
	}

	// Check if the device was created with the D3DCREATE_MULTITHREADED flags in which case
	// the d3d device is thread safe. (cf CShaderDeviceDx8::InvokeCreateDevice() in shaderdevicedx8.cpp)
	// The StaticPropColorMeshCallback() callback will create, lock/unlock vertex and index buffers and
	// therefore can only be called from one of the IO thread if the d3d device is thread safe. Performs
	// the IO job synchronously if not.
	// TODO Find another way of detecting the device is thread safe (Add method to the material system?)
	bool bD3DDeviceThreadSafe = false;
	ConVarRef mat_queue_mode( "mat_queue_mode" );
	if (mat_queue_mode.GetInt() == 2 ||
		(mat_queue_mode.GetInt() == -2 && GetCPUInformation().m_nPhysicalProcessors >= 2) ||
		(mat_queue_mode.GetInt() == -1 && GetCPUInformation().m_nPhysicalProcessors >= 2))
	{
		bD3DDeviceThreadSafe = true;
	}

	// async load the file
	FileAsyncRequest_t fileRequest;
	fileRequest.pContext = (void *)pContext;
	fileRequest.pfnCallback = ::StaticPropColorMeshCallback;
	fileRequest.pData = NULL;
	fileRequest.pszFilename = fileName;
	fileRequest.nOffset = 0;
	fileRequest.flags = bD3DDeviceThreadSafe ? 0 : FSASYNC_FLAGS_SYNC;
	fileRequest.nBytes = 0;

	// PS3 static prop lighting (legacy async IO still in flight catching
	// non reslist-lighting buffers) is writing data into raw pointers
	// to RSX memory which have been acquired before material system
	// switches to multithreaded mode. During switch to multithreaded
	// mode RSX moves its memory so pointers become invalid and thus
	// all IO must be finished and callbacks fired before
	// Host_AllowQueuedMaterialSystem
	// see: CL_FullyConnected g_pFullFileSystem->AsyncFinishAll
	// AsyncFinishAll will not finish jobs with priority <0
	fileRequest.priority = IsPS3() ? 0 : -1;

	fileRequest.pszPathID = "GAME";
	
	// queue for async load
	MEM_ALLOC_CREDIT();
	g_pFileSystem->AsyncRead( fileRequest, &pColorMeshData->m_hAsyncControl );

	return true;
}

//-----------------------------------------------------------------------------
// Computes the static prop color data.
// Data calculation may be delayed if data is disk based.
// Returns FALSE if data not available or error. For retry polling pattern.
// Resturns TRUE if operation succesful or in progress (succeeds later).
//-----------------------------------------------------------------------------
bool CModelRender::UpdateStaticPropColorData( IHandleEntity *pProp, ModelInstanceHandle_t handle )
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

#ifndef DEDICATED
	// find or allocate color meshes
	CColorMeshData *pColorMeshData = FindOrCreateStaticPropColorData( handle );
	if ( !pColorMeshData )
	{
		return false;
	}

	// HACK: on PC, VB creation can fail due to device loss
	if ( IsPC() && pColorMeshData->m_bHasInvalidVB )
	{
		// Don't retry until color data is flushed by device restore
		pColorMeshData->m_bColorMeshValid = false;
		pColorMeshData->m_bNeedsRetry = false;
		return false;
	}

	unsigned char debugColor[3];
	bool bDebugColor = false;
	if ( r_debugrandomstaticlighting.GetBool() )
	{
		// randomize with bright colors, skip black and white
		// purposely not deterministic to catch bugs with excessive re-baking (i.e. disco)
		Vector fRandomColor;
		int nColor = RandomInt(1,6);
		fRandomColor.x = (nColor>>2) & 1;
		fRandomColor.y = (nColor>>1) & 1;
		fRandomColor.z = nColor & 1;
		VectorNormalize( fRandomColor );
		debugColor[0] = fRandomColor[0] * 255.0f;
		debugColor[1] = fRandomColor[1] * 255.0f;
		debugColor[2] = fRandomColor[2] * 255.0f;
		bDebugColor = true;
	}

	// FIXME? : Move this to StudioRender?
	ModelInstance_t &inst = m_ModelInstances[handle];
	Assert( inst.m_pModel );
	Assert( modelloader->IsLoaded( inst.m_pModel ) && ( inst.m_pModel->type == mod_studio ) );

	if ( r_proplightingfromdisk.GetInt() == 2 )
	{
		// This visualization debug mode is strictly to debug which static prop models have valid disk
		// based lighting. There should be no red models, only green or yellow. Yellow models denote the legacy
		// lower quality runtime baked lighting.
		if ( inst.m_nFlags & MODEL_INSTANCE_DISKCOMPILED_COLOR_BAD )
		{
			// prop was compiled for static prop lighting, but out of sync
			// bad disk data for model, show as red
			debugColor[0] = 255.0f;
			debugColor[1] = 0;
			debugColor[2] = 0;
		}
		else if ( inst.m_nFlags & MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR )
		{
			// valid disk data, show as green
			debugColor[0] = 0;
			debugColor[1] = 255.0f;
			debugColor[2] = 0;
		}
		else
		{
			// no disk based data, using runtime method, show as yellow
			// identifies a prop that wasn't compiled for static prop lighting
			debugColor[0] = 255.0f;
			debugColor[1] = 255.0f;
			debugColor[2] = 0;
		}
		bDebugColor = true;
	}

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( inst.m_pModel->studio );
	studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( inst.m_pModel->studio );
	Assert( pStudioHdr && pStudioHWData );

	// Models which can't use the color mesh data (e.g. due to bumpmapping) need to pre-warm the light cache here or else
	// the game will hitch when first rendering them
	if ( (inst.m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING) && inst.m_LightCacheHandle && !modelinfo->UsesStaticLighting( inst.m_pModel ) )
	{
		LightcacheGetStatic( inst.m_LightCacheHandle, NULL, LIGHTCACHEFLAGS_STATIC | LIGHTCACHEFLAGS_DYNAMIC | LIGHTCACHEFLAGS_LIGHTSTYLE );
	}

	if ( !bDebugColor && ( inst.m_nFlags & MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR ) )
	{
		// start an async load on available higher quality disc based data
		if ( LoadStaticPropColorData( pProp, inst.m_ColorMeshHandle, pStudioHWData ) )
		{
			// async in progress, operation expected to succeed
			// async callback handles finalization
			return true;
		}
	}
	
	// lighting calculation path
	// calculation may abort due to lack of async requested data, caller should retry 
	pColorMeshData->m_bColorMeshValid = false;
	pColorMeshData->m_bNeedsRetry = true;

	if ( !bDebugColor )
	{
		// vertexes must be available for lighting calculation
		vertexFileHeader_t *pVertexHdr = g_pMDLCache->GetVertexData( VoidPtrToMDLHandle( pStudioHdr->VirtualModel() ) );
		if ( !pVertexHdr )
		{
			// data not available yet
			return false;
		}
	}

	inst.m_nFlags |= MODEL_INSTANCE_HAS_COLOR_DATA;

	// calculate lighting, set for access to verts
	m_pStudioHdr = pStudioHdr;

	// Sets the model transform state in g_pStudioRender
	matrix3x4_t matrix;
	AngleMatrix( inst.m_pRenderable->GetRenderAngles(), inst.m_pRenderable->GetRenderOrigin(), matrix );
	
	// Get static lighting only!!  We'll add dynamic and lightstyles in in the vertex shader. . .
	LightingState_t lightingState;
	if ( (inst.m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING) && inst.m_LightCacheHandle )
	{
		lightingState = *(LightcacheGetStatic( inst.m_LightCacheHandle, NULL, LIGHTCACHEFLAGS_STATIC ));
	}
	else
	{
		// Choose the lighting origin
		Vector entOrigin;
		R_ComputeLightingOrigin( inst.m_pRenderable, pStudioHdr, matrix, entOrigin );
		LightcacheGetDynamic_Stats stats;
		LightcacheGetDynamic( entOrigin, lightingState, stats, inst.m_pRenderable, LIGHTCACHEFLAGS_STATIC );
	}

	// See if the studiohdr wants to use constant directional light, ie
	// the surface normal plays no part in determining light intensity
	bool bUseConstDirLighting = false;
	float flConstDirLightingAmount = 0.0;
	if ( pStudioHdr->flags & STUDIOHDR_FLAGS_CONSTANT_DIRECTIONAL_LIGHT_DOT )
	{
		bUseConstDirLighting = true;
		flConstDirLightingAmount =  (float)( pStudioHdr->constdirectionallightdot ) / 255.0;
	}

	CUtlMemory< color24 > tmpLightingMem; 
	
	// Iterate over every body part...
	for ( int bodyPartID = 0; bodyPartID < pStudioHdr->numbodyparts; ++bodyPartID )
	{
		mstudiobodyparts_t* pBodyPart = pStudioHdr->pBodypart( bodyPartID );

		// Iterate over every submodel...
		for ( int modelID = 0; modelID < pBodyPart->nummodels; ++modelID )
		{
			mstudiomodel_t* pModel = pBodyPart->pModel(modelID);

			// Make sure we've got enough space allocated
			tmpLightingMem.EnsureCapacity( pModel->numvertices );

			if ( !bDebugColor )
			{
				// Compute lighting for each unique vertex in the model exactly once
				ComputeModelVertexLightingOld( pModel, matrix, lightingState, tmpLightingMem.Base(), bUseConstDirLighting, flConstDirLightingAmount );
			}
			else
			{
				for ( int i=0; i<pModel->numvertices; i++ )
				{
					tmpLightingMem[i].r = debugColor[0];
					tmpLightingMem[i].g = debugColor[1];
					tmpLightingMem[i].b = debugColor[2];
				}
			}

			// distribute the lighting results to the mesh's vertexes
			for ( int lodID = pStudioHWData->m_RootLOD; lodID < pStudioHWData->m_NumLODs; ++lodID )
			{
				studioloddata_t *pStudioLODData = &pStudioHWData->m_pLODs[lodID];
				studiomeshdata_t *pStudioMeshData = pStudioLODData->m_pMeshData;

				// Iterate over all the meshes....
				for ( int meshID = 0; meshID < pModel->nummeshes; ++meshID)
				{
					mstudiomesh_t* pMesh = pModel->pMesh( meshID );

					// Iterate over all strip groups.
					for ( int stripGroupID = 0; stripGroupID < pStudioMeshData[pMesh->meshid].m_NumGroup; ++stripGroupID )
					{
						studiomeshgroup_t* pMeshGroup = &pStudioMeshData[pMesh->meshid].m_pMeshGroup[stripGroupID];
						ColorMeshInfo_t* pColorMeshInfo = &pColorMeshData->m_pMeshInfos[pMeshGroup->m_ColorMeshID];

						CMeshBuilder meshBuilder;
						meshBuilder.Begin( pColorMeshInfo->m_pMesh, MATERIAL_HETEROGENOUS, pMeshGroup->m_NumVertices, 0 );
						
						if ( !meshBuilder.VertexSize() )
						{
							meshBuilder.End();
							return false;		// Aborting processing, since something was wrong with D3D
						}

						// We need to account for the stream offset used by pool-allocated (static-lit) color meshes:
						int streamOffset = pColorMeshInfo->m_nVertOffsetInBytes / meshBuilder.VertexSize();
						meshBuilder.AdvanceVertices( streamOffset );

						// Iterate over all vertices
						for ( int i = 0; i < pMeshGroup->m_NumVertices; ++i)
						{
							int nVertIndex = pMesh->vertexoffset + pMeshGroup->m_pGroupIndexToMeshIndex[i];
							Assert( nVertIndex < pModel->numvertices );
							meshBuilder.Specular3ub( tmpLightingMem[nVertIndex].r, tmpLightingMem[nVertIndex].g, tmpLightingMem[nVertIndex].b );
							meshBuilder.AdvanceVertex();
						}

						meshBuilder.End();
					}
				}
			}
		}
	}
	
	pColorMeshData->m_bColorMeshValid = true;
	pColorMeshData->m_bNeedsRetry = false;
#endif
	
	return true;
}


//-----------------------------------------------------------------------------
// FIXME? : Move this to StudioRender?
//-----------------------------------------------------------------------------
void CModelRender::DestroyStaticPropColorData( ModelInstanceHandle_t handle )
{
#ifndef DEDICATED
	if ( handle == MODEL_INSTANCE_INVALID )
		return;

	if ( m_ModelInstances[handle].m_ColorMeshHandle != DC_INVALID_HANDLE )
	{
		CacheRemove( m_ModelInstances[handle].m_ColorMeshHandle );
		m_ModelInstances[handle].m_ColorMeshHandle = DC_INVALID_HANDLE;
	}
#endif
}


void CModelRender::ReleaseAllStaticPropColorData( void )
{
	FOR_EACH_LL( m_ModelInstances, i )
	{
		DestroyStaticPropColorData( i );
	}
	if ( IsGameConsole() )
	{
		PurgeCachedStaticPropColorData();
	}
}


void CModelRender::RestoreAllStaticPropColorData( void )
{
#if !defined( DEDICATED )
	if ( !host_state.worldmodel )
		return;

	// invalidate all static lighting cache data
	InvalidateStaticLightingCache();

	// rebake
	FOR_EACH_LL( m_ModelInstances, i )
	{
		UpdateStaticPropColorData( m_ModelInstances[i].m_pRenderable->GetIClientUnknown(), i );
	}
#endif
}

void RestoreAllStaticPropColorData( void )
{
	s_ModelRender.RestoreAllStaticPropColorData();
}


//-----------------------------------------------------------------------------
// Creates, destroys instance data to be associated with the model
//-----------------------------------------------------------------------------
ModelInstanceHandle_t CModelRender::CreateInstance( IClientRenderable *pRenderable, LightCacheHandle_t *pCache )
{
	Assert( pRenderable );

	// ensure all components are available
	model_t *pModel = (model_t*)pRenderable->GetModel();

	// We're ok, allocate a new instance handle
	ModelInstanceHandle_t handle = m_ModelInstances.AddToTail();
	ModelInstance_t& instance = m_ModelInstances[handle];

	instance.m_pRenderable = pRenderable;
	instance.m_DecalHandle = STUDIORENDER_DECAL_INVALID;
	instance.m_pModel = (model_t*)pModel;
	instance.m_ColorMeshHandle = DC_INVALID_HANDLE;
	instance.m_pLightingState->m_flLightingTime = CURRENT_LIGHTING_UNINITIALIZED;
	instance.m_nFlags = 0;
	instance.m_LightCacheHandle = 0;

	instance.m_pLightingState->m_AmbientLightingState.ZeroLightingState();
	for ( int i = 0; i < 6; ++i )
	{
		// To catch errors with uninitialized m_AmbientLightingState...
		// force to pure red
		instance.m_pLightingState->m_AmbientLightingState.r_boxcolor[i].x = 1.0;
	}

#ifndef DEDICATED
	instance.m_FirstShadow = g_pShadowMgr->InvalidShadowIndex();
#endif

	// Static props use baked lighting for performance reasons
	if ( pCache )
	{
		SetStaticLighting( handle, pCache );

		// validate static color meshes once, now at load/create time
		ValidateStaticPropColorData( handle );
	
		// 360 persists the color meshes across same map loads
		if ( !IsGameConsole() || instance.m_ColorMeshHandle == DC_INVALID_HANDLE )
		{
			// builds out color meshes or loads disk colors, now at load/create time
			RecomputeStaticLighting( handle );
		}
		else
			if ( r_decalstaticprops.GetBool() && instance.m_LightCacheHandle )
			{
#ifndef DEDICATED
				instance.m_pLightingState->m_AmbientLightingState = *(LightcacheGetStatic( *pCache, NULL, LIGHTCACHEFLAGS_STATIC ));
#endif
			}
	}
	
	return handle;
}


//-----------------------------------------------------------------------------
// Assigns static lighting to the model instance
//-----------------------------------------------------------------------------
void CModelRender::SetStaticLighting( ModelInstanceHandle_t handle, LightCacheHandle_t *pCache )
{
	// FIXME: If we make static lighting available for client-side props,
	// we must clean up the lightcache handles as the model instances are removed.
	// At the moment, since only the static prop manager uses this, it cleans up all LightCacheHandles 
	// at level shutdown.

	// The reason I moved the lightcache handles into here is because this place needs
	// to know about lighting overrides when restoring meshes for alt-tab reasons
	// It was a real pain to do this from within the static prop mgr, where the
	// lightcache handle used to reside
	if (handle != MODEL_INSTANCE_INVALID)
	{
		ModelInstance_t& instance = m_ModelInstances[handle];
		if ( pCache )
		{
			instance.m_LightCacheHandle = *pCache;
			instance.m_nFlags |= MODEL_INSTANCE_HAS_STATIC_LIGHTING;
		}
		else
		{
			instance.m_LightCacheHandle = 0;
			instance.m_nFlags &= ~MODEL_INSTANCE_HAS_STATIC_LIGHTING;
		}
	}
}

LightCacheHandle_t CModelRender::GetStaticLighting( ModelInstanceHandle_t handle )
{
	if (handle != MODEL_INSTANCE_INVALID)
	{
		ModelInstance_t& instance = m_ModelInstances[handle];
		if ( instance.m_nFlags & MODEL_INSTANCE_HAS_STATIC_LIGHTING )
			return instance.m_LightCacheHandle;
		return 0;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// This gets called when overbright, etc gets changed to recompute static prop lighting.
// Returns FALSE if needed async data not available to complete computation or an error (don't draw).
// Returns TRUE if operation succeeded or computation skipped (ok to draw).
// Callers use this to track state in a retry pattern, so the expensive computation
// only happens once as needed or can continue to be polled until success.
//-----------------------------------------------------------------------------
bool CModelRender::RecomputeStaticLighting( ModelInstanceHandle_t handle )
{
#ifndef DEDICATED

	VPROF_TEMPORARY_OVERRIDE_BECAUSE_SPECIFYING_LEVEL_2_FROM_THE_COMMANDLINE_DOESNT_WORK( "CModelRender::RecomputeStaticLighting");
	if ( handle == MODEL_INSTANCE_INVALID )
	{
		return false;
	}

	ModelInstance_t& instance = m_ModelInstances[handle];
	Assert( modelloader->IsLoaded( instance.m_pModel ) && ( instance.m_pModel->type == mod_studio ) );

	if ( instance.m_pModel->flags & MODELFLAG_STUDIOHDR_IS_STATIC_PROP )
	{
		if ( r_decalstaticprops.GetBool() && instance.m_LightCacheHandle )
		{
			instance.m_pLightingState->m_AmbientLightingState = *(LightcacheGetStatic( instance.m_LightCacheHandle, NULL, LIGHTCACHEFLAGS_STATIC ));
		}

		// get data, possibly delayed due to async
		studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( instance.m_pModel->studio );
		if ( !pStudioHWData )
		{
			// data not available
			return false;
		}

		return UpdateStaticPropColorData( instance.m_pRenderable->GetIClientUnknown(), handle );
	}

#endif
	// success
	return true;
}

void CModelRender::PurgeCachedStaticPropColorData( void )
{
	// valid for console only
	Assert( IsGameConsole() );
	if ( IsPC() )
	{
		return;
	}

	// flush all the color mesh data
	GetCacheSection()->Flush( true, true );
	DataCacheStatus_t status;
	GetCacheSection()->GetStatus( &status );
	if ( status.nBytes )
	{
		DevWarning( "CModelRender: ColorMesh %d bytes failed to flush!\n", status.nBytes );
	}

	m_colorMeshVBAllocator.Clear();
	m_CachedStaticPropColorData.Purge();
}

bool CModelRender::IsStaticPropColorDataCached( const char *pName )
{
	// valid for console only
	Assert( IsGameConsole() );
	if ( IsPC() )
	{
		return false;
	}

	DataCacheHandle_t hColorMesh = DC_INVALID_HANDLE;
	{
		AUTO_LOCK_FM( m_CachedStaticPropMutex );
		int iIndex = m_CachedStaticPropColorData.Find( pName );
		if ( m_CachedStaticPropColorData.IsValidIndex( iIndex ) )
		{
			hColorMesh = m_CachedStaticPropColorData[iIndex];
		}
	}

	CColorMeshData *pColorMeshData = CacheGetNoTouch( hColorMesh );
	if ( pColorMeshData )
	{
		// color mesh data is in cache
		return true;
	}

	return false;
}

DataCacheHandle_t CModelRender::GetCachedStaticPropColorData( const char *pName )
{
	// valid for console only
	Assert( IsGameConsole() );
	if ( IsPC() )
	{
		return DC_INVALID_HANDLE;
	}

	DataCacheHandle_t hColorMesh = DC_INVALID_HANDLE;
	{
		AUTO_LOCK_FM( m_CachedStaticPropMutex );
		int iIndex = m_CachedStaticPropColorData.Find( pName );
		if ( m_CachedStaticPropColorData.IsValidIndex( iIndex ) )
		{
			hColorMesh = m_CachedStaticPropColorData[iIndex];
		}
	}

	return hColorMesh;
}

void CModelRender::SetupColorMeshes( int nTotalVerts )
{
	// valid for console only
	Assert( IsGameConsole() );
	if ( IsPC() )
	{
		return;
	}

	if ( !g_pQueuedLoader->IsMapLoading() )
	{
		// oops, the queued loader didn't run which does the pre-purge cleanup
		// do the cleanup now
		PurgeCachedStaticPropColorData();
	}

	// Set up the appropriate default value for color mesh pooling
	if ( r_proplightingpooling.GetInt() == -1 )
	{
		// This is useful on X360 because VBs are 4-KB aligned, so using a shared VB saves tons of memory
		r_proplightingpooling.SetValue( true );
	}

	if ( r_proplightingpooling.GetInt() == 1 )
	{
		if ( m_colorMeshVBAllocator.GetNumVertsAllocated() == 0 )
		{
			if ( nTotalVerts )
			{
				// Allocate a mesh (vertex buffer) big enough to accommodate all static prop color meshes
				// (which are allocated inside CModelRender::FindOrCreateStaticPropColorData() ):
				m_colorMeshVBAllocator.Init( VERTEX_SPECULAR, nTotalVerts );
			}
		}
		else
		{
			// already allocated
			// 360 keeps the color meshes during same map loads
			// vb allocator already allocated, needs to match
			Assert( m_colorMeshVBAllocator.GetNumVertsAllocated() == nTotalVerts );
		}
	}
}

void CModelRender::DestroyInstance( ModelInstanceHandle_t handle )
{
	if ( handle == MODEL_INSTANCE_INVALID )
		return;

	g_pStudioRender->DestroyDecalList( m_ModelInstances[handle].m_DecalHandle );
#ifndef DEDICATED
	g_pShadowMgr->RemoveAllShadowsFromModel( handle );
#endif

	// 360 holds onto static prop disk color data only, to avoid redundant work during same map load
	// can only persist props with disk based lighting
	// check for dvd mode as a reasonable assurance that the queued loader will be responsible for a possible purge
	// if the queued loader doesn't run, the purge will get caught later than intended
	bool bPersistLighting = IsGameConsole() && 
		( m_ModelInstances[handle].m_nFlags & MODEL_INSTANCE_HAS_DISKCOMPILED_COLOR ) && 
		( g_pFullFileSystem->GetDVDMode() != DVDMODE_OFF );
	if ( !bPersistLighting )
	{
		DestroyStaticPropColorData( handle );
	}

	m_ModelInstances.Remove( handle );
}

bool CModelRender::ChangeInstance( ModelInstanceHandle_t handle, IClientRenderable *pRenderable )
{
	if ( handle == MODEL_INSTANCE_INVALID || !pRenderable )
		return false;

	ModelInstance_t& instance = m_ModelInstances[handle];

	if ( instance.m_pModel != pRenderable->GetModel() )
	{
		DevMsg("MoveInstanceHandle: models are different!\n");
		return false;
	}

#ifndef DEDICATED
	g_pShadowMgr->RemoveAllShadowsFromModel( handle );
#endif

	// ok, models are the same, change renderable pointer
	instance.m_pRenderable = pRenderable;

	return true;
}


//-----------------------------------------------------------------------------
// It's not valid if the model index changed + we have non-zero instance data
//-----------------------------------------------------------------------------
bool CModelRender::IsModelInstanceValid( ModelInstanceHandle_t handle )
{
	if ( handle == MODEL_INSTANCE_INVALID )
		return false;

	ModelInstance_t& inst = m_ModelInstances[handle];
	if ( inst.m_DecalHandle == STUDIORENDER_DECAL_INVALID )
		return false;

	if ( inst.m_pRenderable == NULL )
		return false;

	model_t const* pModel = inst.m_pRenderable->GetModel();
	return inst.m_pModel == pModel;
}


//-----------------------------------------------------------------------------
// Creates a decal on a model instance by doing a planar projection
//-----------------------------------------------------------------------------
static unsigned int s_DecalFadeVarCache = 0;
static unsigned int s_DecalSkipVarCache = 0;

void CModelRender::AddDecal( ModelInstanceHandle_t handle, Ray_t const& ray,
	const Vector& decalUp, int decalIndex, int body, bool noPokeThru, int maxLODToDecal, IMaterial *pSpecifyMaterial, float w, float h, void *pvProxyUserData, int nAdditionalDecalFlags )
{
#ifndef DEDICATED
	if (handle == MODEL_INSTANCE_INVALID)
		return;

	// Get the decal material + radius
	IMaterial* pDecalMaterial;

	if ( pSpecifyMaterial == NULL )
	{

		R_DecalGetMaterialAndSize( decalIndex, pDecalMaterial, w, h );
		if ( !pDecalMaterial )
		{
			DevWarning("Bad decal index %d\n", decalIndex );
			return;
		}
		w *= 0.5f;
		h *= 0.5f;

	}
	else
	{
		pDecalMaterial = pSpecifyMaterial;
	}

	// FIXME: For now, don't render fading decals on props...
	IMaterialVar* pFadeVar = pDecalMaterial->FindVarFast( "$decalFadeDuration", &s_DecalFadeVarCache );
	if ( pFadeVar )
		return;

	bool skipRiggedModels = false;
	IMaterialVar* pSkipModelVar = pDecalMaterial->FindVarFast( "$skipRiggedModels", &s_DecalSkipVarCache );
	if ( pSkipModelVar )
	{
		skipRiggedModels = ( pSkipModelVar->GetIntValue() > 0 );
	}


	// FIXME: Pass w and h into AddDecal
	float radius = (w > h) ? w : h;

	ModelInstance_t& inst = m_ModelInstances[handle];
	if (!IsModelInstanceValid(handle))
	{
		g_pStudioRender->DestroyDecalList(inst.m_DecalHandle);
		inst.m_DecalHandle = STUDIORENDER_DECAL_INVALID;
	}

	Assert( modelloader->IsLoaded( inst.m_pModel ) && ( inst.m_pModel->type == mod_studio ) );
	if ( inst.m_DecalHandle == STUDIORENDER_DECAL_INVALID )
	{
		studiohwdata_t *pStudioHWData = g_pMDLCache->GetHardwareData( inst.m_pModel->studio );
		inst.m_DecalHandle = g_pStudioRender->CreateDecalList( pStudioHWData );
	}

	studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( inst.m_pModel );
	if ( pStudioHdr->numbodyparts == 0 )
		return;

	if ( skipRiggedModels && pStudioHdr->numbones > 1 )
	{
		return;
	}

	// Set up skinning state
	int nBoneCount = pStudioHdr->numbones;
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CMatRenderData< matrix3x4a_t > rdBoneToWorld( pRenderContext, nBoneCount );
	inst.m_pRenderable->SetupBones( rdBoneToWorld.Base(), nBoneCount, BONE_USED_BY_ANYTHING, GetBaseLocalClient().GetTime() ); // hack hack
	g_pStudioRender->AddDecal( inst.m_DecalHandle, g_pMDLCache->GetStudioHdr( inst.m_pModel->studio ),
		 rdBoneToWorld.Base(), ray, decalUp, pDecalMaterial, radius, body, noPokeThru, maxLODToDecal, pvProxyUserData, nAdditionalDecalFlags );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if both the instance handle is valid and has a non-zero decal count
//-----------------------------------------------------------------------------
bool CModelRender::ModelHasDecals( ModelInstanceHandle_t handle )
{
	if (handle == MODEL_INSTANCE_INVALID)
		return false;

	ModelInstance_t& inst = m_ModelInstances[handle];
	if (!IsModelInstanceValid(handle))
		return false;

	if ( inst.m_DecalHandle != STUDIORENDER_DECAL_INVALID )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Removes all the decals on a model instance
//-----------------------------------------------------------------------------
void CModelRender::RemoveAllDecals( ModelInstanceHandle_t handle )
{
	if (handle == MODEL_INSTANCE_INVALID)
		return;
	
	ModelInstance_t& inst = m_ModelInstances[handle];
	if (!IsModelInstanceValid(handle))
		return;

	g_pStudioRender->DestroyDecalList( inst.m_DecalHandle );
	inst.m_DecalHandle = STUDIORENDER_DECAL_INVALID;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModelRender::RemoveAllDecalsFromAllModels( bool bRenderContextValid )
{
	for ( ModelInstanceHandle_t i = m_ModelInstances.Head(); 
		i != m_ModelInstances.InvalidIndex(); 
		i = m_ModelInstances.Next( i ) )
	{
		RemoveAllDecals( i );
	}

#ifndef DEDICATED
	if ( g_ClientDLL )
	{
		g_ClientDLL->RetireAllPlayerDecals( bRenderContextValid );
	}
#endif
}

const vertexFileHeader_t * mstudiomodel_t::CacheVertexData( void *pModelData )
{
	// make requested data resident
	Assert( pModelData == NULL );
	return s_ModelRender.CacheVertexData();
}

bool CheckVarRange_r_rootlod()
{
	return CheckVarRange_Generic( &r_rootlod, 0, 2 );
}

bool CheckVarRange_r_lod()
{
	return CheckVarRange_Generic( &r_lod, -1, 2 );
}


// Convar callback to change lod 
//-----------------------------------------------------------------------------
void r_lod_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	CheckVarRange_r_lod();
}

//-----------------------------------------------------------------------------
// Convar callback to change root lod 
//-----------------------------------------------------------------------------
void SetRootLOD_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	// Make sure the variable is in range.
	if ( CheckVarRange_r_rootlod() )
		return;	// was called recursively.

	ConVarRef var( pConVar );
	UpdateStudioRenderConfig();
	if ( !g_LostVideoMemory && Q_strcmp( var.GetString(), pOldString ) )
	{
		// reload only the necessary models to desired lod
		modelloader->Studio_ReloadModels( IModelLoader::RELOAD_LOD_CHANGED );
	}
}

//-----------------------------------------------------------------------------
// Discard and reload (rebuild, rebake, etc) models to the current lod
//-----------------------------------------------------------------------------
void FlushLOD_f()
{
	UpdateStudioRenderConfig();
	if ( !g_LostVideoMemory )
	{
		// force a full discard and rebuild of all loaded models
		modelloader->Studio_ReloadModels( IModelLoader::RELOAD_EVERYTHING );
	}
}

//-----------------------------------------------------------------------------
//
// CPooledVBAllocator_ColorMesh implementation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// CPooledVBAllocator_ColorMesh constructor
//-----------------------------------------------------------------------------
CPooledVBAllocator_ColorMesh::CPooledVBAllocator_ColorMesh()
: m_pMesh( NULL )
{
	Clear();
}

//-----------------------------------------------------------------------------
// CPooledVBAllocator_ColorMesh destructor
//  - Clear should have been called
//-----------------------------------------------------------------------------
CPooledVBAllocator_ColorMesh::~CPooledVBAllocator_ColorMesh()
{
	CheckIsClear();

	// Clean up, if it hadn't been done already
	Clear();
}

//-----------------------------------------------------------------------------
// Init
//  - Allocate the internal shared mesh (vertex buffer)
//-----------------------------------------------------------------------------
bool CPooledVBAllocator_ColorMesh::Init( VertexFormat_t format, int numVerts )
{
	if ( !CheckIsClear() )
		return false;

	if ( g_VBAllocTracker )
		g_VBAllocTracker->TrackMeshAllocations( "CPooledVBAllocator_ColorMesh::Init" );

	CMatRenderContextPtr pRenderContext( materials );
	m_pMesh = pRenderContext->CreateStaticMesh( format, TEXTURE_GROUP_STATIC_VERTEX_BUFFER_COLOR );
	if ( m_pMesh )
	{
		// Build out the underlying vertex buffer
		CMeshBuilder meshBuilder;
		int numIndices = 0;
		meshBuilder.Begin( m_pMesh, MATERIAL_HETEROGENOUS, numVerts, numIndices );
		{
			m_pVertexBufferBase	= meshBuilder.Specular();
			m_totalVerts		= numVerts;
			m_vertexSize		= meshBuilder.VertexSize();
			// Probably good to catch any change to vertex size... there may be assumptions based on it:
			Assert( m_vertexSize == 4 );
			// Start at the bottom of the VB and work your way up like a simple stack
			m_nextFreeOffset	= 0;
		}
		meshBuilder.End();
	}

	if ( g_VBAllocTracker )
		g_VBAllocTracker->TrackMeshAllocations( NULL );

	return ( m_pMesh != NULL );
}

//-----------------------------------------------------------------------------
// Clear
//  - frees the shared mesh (vertex buffer), resets member variables
//-----------------------------------------------------------------------------
void CPooledVBAllocator_ColorMesh::Clear( void )
{
	if ( m_pMesh != NULL )
	{
		if ( m_numAllocations > 0 )
		{
			Warning( "ERROR: CPooledVBAllocator_ColorMesh::Clear should not be called until all allocations released!\n" );
			Assert( m_numAllocations == 0 );
		}
		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->DestroyStaticMesh( m_pMesh );
		m_pMesh = NULL;
	}

	m_pVertexBufferBase		= NULL;
	m_totalVerts			= 0;
	m_vertexSize			= 0;

	m_numAllocations		= 0;
	m_numVertsAllocated		= 0;
	m_nextFreeOffset		= -1;
	m_bStartedDeallocation	= false;
}

//-----------------------------------------------------------------------------
// CheckIsClear
//  - assert/warn if the allocator isn't in a clear state
//    (no extant allocations, no internal mesh)
//-----------------------------------------------------------------------------
bool CPooledVBAllocator_ColorMesh::CheckIsClear( void )
{
	if ( m_pMesh )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh's internal mesh (vertex buffer) should have been freed!\n" );
		Assert( m_pMesh == NULL );
		return false;
	}

	if ( m_numAllocations > 0 )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh has unfreed allocations!" );
		Assert( m_numAllocations == 0 );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Allocate
//  - Allocate a sub-range of 'numVerts' from free space in the shared vertex buffer
//    (returns the byte offset from the start of the VB to the new allocation)
//  - returns -1 on failure
//-----------------------------------------------------------------------------
int CPooledVBAllocator_ColorMesh::Allocate( int numVerts )
{
	if ( m_pMesh == NULL )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Allocate cannot be called before Init (expect a crash)\n" );
		Assert( m_pMesh );
		return -1;
	}

	// Once we start deallocating, we have to keep going until everything has been freed
	if ( m_bStartedDeallocation )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Allocate being called after some (but not all) calls to Deallocate have been called - invalid! (expect visual artifacts)\n" );
		Assert( !m_bStartedDeallocation );
		return -1;
	}

	if ( numVerts > ( m_totalVerts - m_numVertsAllocated ) )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Allocate failing - not enough space left in the vertex buffer!\n" );
		Assert( numVerts <= ( m_totalVerts - m_numVertsAllocated ) );
		return -1;
	}

	int result = m_nextFreeOffset;

	m_numAllocations	+= 1;
	m_numVertsAllocated	+= numVerts;
	m_nextFreeOffset	+= numVerts*m_vertexSize;

	return result;
}

//-----------------------------------------------------------------------------
// Deallocate
//  - Deallocate an existing allocation
//-----------------------------------------------------------------------------
void CPooledVBAllocator_ColorMesh::Deallocate( int offset, int numVerts )
{
	if ( m_pMesh == NULL )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Deallocate cannot be called before Init\n" );
		Assert( m_pMesh != NULL );
		return;
	}

	if ( m_numAllocations == 0 )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Deallocate called too many times! (bug in calling code)\n" );
		Assert( m_numAllocations > 0 );
		return;
	}

	if ( numVerts > m_numVertsAllocated )
	{
		Warning( "ERROR: CPooledVBAllocator_ColorMesh::Deallocate called with too many verts, trying to free more than were allocated (bug in calling code)\n" );
		Assert( numVerts <= m_numVertsAllocated );
		numVerts = m_numVertsAllocated; // Hack (avoid counters ever going below zero)
	}

	// Now all extant allocations must be freed before we make any new allocations
	m_bStartedDeallocation = true;

	m_numAllocations	-= 1;
	m_numVertsAllocated	-= numVerts;
	m_nextFreeOffset	 = 0; // (we shouldn't be returning this until everything's free, at which point 0 is valid)

	// Are we empty?
	if ( m_numAllocations == 0 )
	{
		if ( m_numVertsAllocated != 0 )
		{
			Warning( "ERROR: CPooledVBAllocator_ColorMesh::Deallocate, after all allocations have been freed too few verts total have been deallocated (bug in calling code)\n" );
			Assert( m_numVertsAllocated == 0 );
		}

		// We can start allocating again, now
		m_bStartedDeallocation = false;
	}
}
