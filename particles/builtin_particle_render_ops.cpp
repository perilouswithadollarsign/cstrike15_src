//===== Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: particle system code
//
//===========================================================================//

#include "tier0/platform.h"
#include "particles/particles.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"
#include "tier2/renderutils.h"
#include "tier2/beamsegdraw.h"
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"
#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "bitmap/psheet.h"
#include "particles_internal.h"
#include "tier0/vprof.h"

#ifdef USE_BLOBULATOR
// TODO: These should be in public by the time the SDK ships
	#include "../common/blobulator/implicit/impdefines.h"
	#include "../common/blobulator/implicit/imprenderer.h"
	#include "../common/blobulator/implicit/imptiler.h"
	#include "../common/blobulator/implicit/userfunctions.h"
	#include "../common/blobulator/iblob_renderer.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Vertex instancing (1 vert submitted per particle, duplicated to 4 (a quad) on the GPU) is supported only on 360
const bool bUseInstancing = IsX360();

//-----------------------------------------------------------------------------
// Utility method to compute the max # of particles per batch
//-----------------------------------------------------------------------------
static inline int GetMaxParticlesPerBatch( IMatRenderContext *pRenderContext, IMaterial *pMaterial, bool bWithInstancing )
{
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int nMaxIndices  = pRenderContext->GetMaxIndicesToRender();

	if ( bWithInstancing )
		return nMaxVertices;
	else
		return MIN( (nMaxVertices / 4), (nMaxIndices / 6) );
}

void SetupParticleVisibility( CParticleCollection *pParticles, CParticleVisibilityData *pVisibilityData, const CParticleVisibilityInputs *pVisibilityInputs, int *nQueryHandle, IMatRenderContext *pRenderContext )
{
	Vector vecOrigin;
	float flVisibility = 1.0f;

	if ( pVisibilityInputs->m_nCPin >= 0 )
	{
		vecOrigin = pParticles->GetControlPointAtCurrentTime( pVisibilityInputs->m_nCPin );

		// Pixel Visibility
		if ( pVisibilityInputs->m_flInputMin != pVisibilityInputs->m_flInputMax )
		{
			float flScale = pVisibilityInputs->m_flProxyRadius;
			flVisibility = g_pParticleSystemMgr->Query()->GetPixelVisibility( nQueryHandle, vecOrigin, flScale );
			flVisibility *= RemapValClamped( flScale, pVisibilityInputs->m_flInputMin, pVisibilityInputs->m_flInputMax, 0.0f , 1.0f );
		}
		// Dot
		if ( pVisibilityInputs->m_flDotInputMin != pVisibilityInputs->m_flDotInputMax )
		{
			CParticleSIMDTransformation pXForm1;
			pParticles->GetControlPointTransformAtTime( pVisibilityInputs->m_nCPin, pParticles->m_flCurTime, &pXForm1 );
			Vector vecInput1 = pXForm1.m_v4Fwd.Vec( 0 );
			Vector vecInput2 = pXForm1.m_v4Origin.Vec( 0 ) - g_pParticleSystemMgr->Query()->GetCurrentViewOrigin();
			VectorNormalize( vecInput2 );

			float flDotVisibility = DotProduct( vecInput1, vecInput2 );

			flVisibility *= RemapValClamped( flDotVisibility, pVisibilityInputs->m_flDotInputMin, pVisibilityInputs->m_flDotInputMax, 0.0f , 1.0f );
		}

		// Distance
		if ( pVisibilityInputs->m_flDistanceInputMin != pVisibilityInputs->m_flDistanceInputMax )
		{
			Vector vecCameraPos;
			if ( pParticles->m_pDef->IsScreenSpaceEffect() )
			{
				pRenderContext->MatrixMode( MATERIAL_VIEW );
				pRenderContext->PopMatrix();
				pRenderContext->MatrixMode( MATERIAL_PROJECTION );
				pRenderContext->PopMatrix();
				pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );
				pRenderContext->MatrixMode( MATERIAL_VIEW );
				pRenderContext->PushMatrix();
				pRenderContext->LoadIdentity();
				pRenderContext->MatrixMode( MATERIAL_PROJECTION );
				pRenderContext->PushMatrix();
				pRenderContext->LoadIdentity();
				pRenderContext->Ortho( -100, -100, 100, 100, -100, 100 );
			}
			else
			{
				pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );
			}
			Vector vecDelta = vecOrigin - vecCameraPos;
			float flDistance = vecDelta.Length();

			flVisibility *= RemapValClamped( flDistance, pVisibilityInputs->m_flDistanceInputMin, pVisibilityInputs->m_flDistanceInputMax, 0.0f , 1.0f );
		}
	}

	pVisibilityData->m_flAlphaVisibility  = Lerp( flVisibility, pVisibilityInputs->m_flAlphaScaleMin, pVisibilityInputs->m_flAlphaScaleMax  );
	pVisibilityData->m_flRadiusVisibility = Lerp( flVisibility, pVisibilityInputs->m_flRadiusScaleMin, pVisibilityInputs->m_flRadiusScaleMax  );

	// FOV
	if ( pVisibilityInputs->m_flRadiusScaleFOVBase != 0.0f )
	{
		// m_flRadiusScaleFOVBase represents 'neutral'; scale particles up when FOV is higher and down when FOV is lower,
		// so their pixel width onscreen is constant as the camera zooms (though distance to the camera still has an effect)
		const float DEGREES_TO_RADIANS = 0.01745329f; 
		matrix3x4_t projMatrix;

		pRenderContext->GetMatrix( MATERIAL_PROJECTION, &projMatrix );
		float flMatrixX = projMatrix.m_flMatVal[0][0];
		float flNeutralMatrixX = 1.0f / tanf( 0.5f*pVisibilityInputs->m_flRadiusScaleFOVBase*DEGREES_TO_RADIANS );

		pVisibilityData->m_flRadiusVisibility *= ( flNeutralMatrixX / flMatrixX );
	}
}

//-----------------------------------------------------------------------------
// Cull systems by control point attributes
// Cull if dot( camera.Position - controlpoint.Position, controlpoint.forward ) < 0
//-----------------------------------------------------------------------------
#define CULL_CP_NORMAL_DESCRIPTOR "cull system when CP normal faces away from camera"
#define CULL_RECURSION_DEPTH_DESCRIPTOR "cull system starting at this recursion depth"
	
struct CullSystemByControlPointData_t
{
	int m_nCullControlPoint;		// Control point who's position and orientation we use for culling (-1 for no culling)
	int m_nViewRecursionDepthStart; // Start culling at this view recursion depth (-1 for no culling)
};

bool ShouldCullParticleSystem( const CullSystemByControlPointData_t *pCullData, CParticleCollection *pParticles, IMatRenderContext *pRenderContext, int nViewResursionDepth )
{
	// Not for screenspace effects
	if ( pParticles->m_pDef->IsScreenSpaceEffect() )
		return false;

	// If recursiondepthstart is -1 or m_nCullControlPoint is -1, then culling is disabled
	if ( pCullData->m_nCullControlPoint == -1 || pCullData->m_nViewRecursionDepthStart == -1 )
		return false;

	// Make sure we're at or past the recursion depth start
	if ( nViewResursionDepth < pCullData->m_nViewRecursionDepthStart )
		return false;

	// Otherwise cull when the control point is facing away from the camera
	Vector vCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vCameraPos );
	const Vector &vCullPosition = pParticles->GetControlPointAtCurrentTime( pCullData->m_nCullControlPoint );
	Vector vRight;
	Vector vUp;
	Vector vControlPointForward;
	pParticles->GetControlPointOrientationAtCurrentTime( pCullData->m_nCullControlPoint, &vRight, &vUp, &vControlPointForward );

	Vector vControlPointToCamera = vCameraPos - vCullPosition;
	vControlPointToCamera.NormalizeInPlace();
	float flDot = DotProduct( vControlPointToCamera, vControlPointForward );

	const float flCosAngleThreshold = -0.10f; // MAGIC NUMBER: cos of ~95 degrees
	return ( flDot < flCosAngleThreshold ) ? true : false;
}


static SheetSequenceSample_t s_DefaultSheetSequence = 
{
	0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f,

	1.0f,
};


class C_OP_RenderPoints : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderPoints );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = 0;
		if ( VisibilityInputs.m_nCPin >= 0 )
			nMask |= 1ULL << VisibilityInputs.m_nCPin; 
		return nMask;
	}

	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const;

	struct C_OP_RenderPointsContext_t
	{
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderPointsContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RenderPointsContext_t *pCtx = reinterpret_cast<C_OP_RenderPointsContext_t *>( pContext );
		pCtx->m_VisibilityData.m_bUseVisibility = false;
		pCtx->m_nQueryHandle = 0;
	}
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderPoints, "render_points", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderPoints ) 
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderPoints )

void C_OP_RenderPoints::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
{
	C_OP_RenderPointsContext_t *pCtx = reinterpret_cast<C_OP_RenderPointsContext_t *>( pContext );
	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	int nParticles;
	const ParticleRenderData_t *pRenderList = 
		pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData  );

	size_t xyz_stride;
	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	pRenderContext->Bind( pMaterial );

	CMeshBuilder meshBuilder;

	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	while ( nParticles )
	{
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		int nParticlesInBatch = MIN( nMaxVertices, nParticles );
		meshBuilder.Begin( pMesh, MATERIAL_POINTS, nParticlesInBatch );
		nParticles -= nParticlesInBatch;
		for( int i = 0; i < nParticlesInBatch; i++ )
		{
			int hParticle = (--pRenderList)->m_nIndex;
			int nIndex = ( hParticle / 4 ) * xyz_stride;
			int nOffset = hParticle & 0x3;
			meshBuilder.Position3f( SubFloat( xyz[nIndex], nOffset ), SubFloat( xyz[nIndex+1], nOffset ), SubFloat( xyz[nIndex+2], nOffset ) );
			meshBuilder.Color4ub( 255, 255, 255, 255 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();
		}
		meshBuilder.End();
		pMesh->DrawModulated( vecDiffuseModulation );
	}
}

//-----------------------------------------------------------------------------
//
// Sprite Rendering
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Utility struct to help with sprite rendering
//-----------------------------------------------------------------------------
struct SpriteRenderInfo_t
{
	size_t m_nXYZStride;
	const fltx4 *m_pXYZ;
	size_t m_nRotStride;
	const fltx4 *m_pRot;
	size_t m_nYawStride;
	const fltx4 *m_pYaw;
	size_t m_nRGBStride;
	const fltx4 *m_pRGB;
	size_t m_nCreationTimeStride;
	const fltx4 *m_pCreationTimeStamp;
	size_t m_nSequenceStride;
	const fltx4 *m_pSequenceNumber;
	size_t m_nSequence1Stride;
	const fltx4 *m_pSequence1Number;
	float m_flAgeScale;
	float m_flAgeScale2;

	CSheet *m_pSheet;
	int m_nVertexOffset;
	CParticleCollection *m_pParticles;

	void Init( CParticleCollection *pParticles, int nVertexOffset, float flAgeScale, float flAgeScale2, CSheet *pSheet )
	{
		m_pParticles = pParticles;
		m_pXYZ = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &m_nXYZStride );
		m_pRot = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ROTATION, &m_nRotStride );
		m_pYaw = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_YAW, &m_nYawStride );
		m_pRGB = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, &m_nRGBStride );
		m_pCreationTimeStamp = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &m_nCreationTimeStride );
		m_pSequenceNumber = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, &m_nSequenceStride );
		m_pSequence1Number = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1, &m_nSequence1Stride );
		m_flAgeScale = flAgeScale;
		m_flAgeScale2 = flAgeScale2;
		m_pSheet = pSheet;
		m_nVertexOffset = nVertexOffset;
	}
};

class C_OP_RenderSprites : public C_OP_RenderPoints
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderSprites );

	struct C_OP_RenderSpritesContext_t
	{
		unsigned int m_nOrientationVarToken;
		unsigned int m_nOrientationMatrixVarToken;
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
		bool	m_bDidPerfWarning;
		bool	m_bPerParticleGlow;
		
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderSpritesContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const;

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = 0;
		if ( m_nOrientationControlPoint >= 0 )
			nMask |= 1ULL << m_nOrientationControlPoint;
		if ( VisibilityInputs.m_nCPin >= 0 )
			nMask |= 1ULL << VisibilityInputs.m_nCPin; 
		return nMask;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_ROTATION_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK | 
			PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK |
			PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1_MASK |
			PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	virtual int GetParticlesToRender( CParticleCollection *pParticles, void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, int *pVertsUsed, int *pIndicesUsed ) const;
	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const;
	virtual void RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;
	void RenderSpriteCard( CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, int hParticle, ParticleRenderData_t const *pSortList ) const;
	template<bool bPerParticleOutline, bool bDoNormals, class T> void RenderSpriteCardNew( CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, T const *pSortList ) const;
	void RenderTwoSequenceSpriteCardNew(  CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, ParticleFullRenderData_Scalar_View const *pSortList ) const;


	void RenderNonSpriteCardCameraFacingOld( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const;
	void RenderNonSpriteCardCameraFacing( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const;

	void RenderNonSpriteCardZRotating( CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, int hParticle, const Vector& vecCameraPos, ParticleRenderData_t const *pSortList ) const;
	void RenderNonSpriteCardZRotating( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const;
	void RenderUnsortedNonSpriteCardZRotating( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;
	void RenderUnsortedNonSpriteCardZRotatingOld( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;

	void RenderNonSpriteCardOriented( CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, int hParticle, const Vector& vecCameraPos, ParticleRenderData_t const *pSortList ) const;
	void RenderNonSpriteCardOriented( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const;
	void RenderUnsortedNonSpriteCardOriented( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;

	// cycles per second
	float	m_flAnimationRate;
	float	m_flAnimationRate2;
	bool	m_bFitCycleToLifetime;
	bool	m_bAnimateInFPS;
	int		m_nOrientationType;
	int		m_nOrientationControlPoint;
	CullSystemByControlPointData_t m_cullData;
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderSprites, "render_animated_sprites", OPERATOR_GENERIC );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderSprites ) 
	DMXELEMENT_UNPACK_FIELD( "animation rate", ".1", float, m_flAnimationRate )
	DMXELEMENT_UNPACK_FIELD( "animation_fit_lifetime", "0", bool, m_bFitCycleToLifetime )
	DMXELEMENT_UNPACK_FIELD( "orientation_type", "0", int, m_nOrientationType )
	DMXELEMENT_UNPACK_FIELD( "orientation control point", "-1", int, m_nOrientationControlPoint )
	DMXELEMENT_UNPACK_FIELD( "second sequence animation rate", "0", float, m_flAnimationRate2 )
	DMXELEMENT_UNPACK_FIELD( "use animation rate as FPS", "0", bool, m_bAnimateInFPS )
	DMXELEMENT_UNPACK_FIELD( CULL_CP_NORMAL_DESCRIPTOR, "-1", int, m_cullData.m_nCullControlPoint )
	DMXELEMENT_UNPACK_FIELD( CULL_RECURSION_DEPTH_DESCRIPTOR, "-1", int, m_cullData.m_nViewRecursionDepthStart )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderSprites )

void C_OP_RenderSprites::InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );
	pCtx->m_nOrientationVarToken = 0;
	pCtx->m_nOrientationMatrixVarToken = 0;
	if ( ( VisibilityInputs.m_nCPin >= 0 ) || ( VisibilityInputs.m_flRadiusScaleFOVBase > 0 ) )
		pCtx->m_VisibilityData.m_bUseVisibility = true;
	else
		pCtx->m_VisibilityData.m_bUseVisibility = false;
	pCtx->m_bDidPerfWarning = false;

	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();
	IMaterialVar* pVar = pMaterial ? pMaterial->FindVarFast( "$perparticleoutline", &pParticles->m_pDef->m_nPerParticleOutlineMaterialVarToken ) : NULL;
	pCtx->m_bPerParticleGlow = ( pVar && ( pVar->GetIntValue() ) );
	
	pCtx->m_nQueryHandle = 0;
}

const SheetSequenceSample_t *GetSampleForSequence( CSheet *pSheet, float flAge, float flAgeScale, int nSequence )
{
	if ( pSheet == NULL )
		return NULL;

	if ( pSheet->m_SheetInfo[nSequence].m_nNumFrames == 1 )
		return (const SheetSequenceSample_t *) &pSheet->m_SheetInfo[nSequence].m_pSamples[0];

	flAge *= flAgeScale;
	unsigned int nFrame = flAge;
	if ( pSheet->m_SheetInfo[nSequence].m_SeqFlags & SEQ_FLAG_CLAMP )
	{
		nFrame = MIN( nFrame, SEQUENCE_SAMPLE_COUNT-1 );
	}
	else
	{
		nFrame &= SEQUENCE_SAMPLE_COUNT-1;
	}

	return (const SheetSequenceSample_t *) &pSheet->m_SheetInfo[nSequence].m_pSamples[nFrame];
}

int C_OP_RenderSprites::GetParticlesToRender( CParticleCollection *pParticles, 
											 void *pContext, int nFirstParticle, 
											  int nRemainingVertices, int nRemainingIndices,
											  int *pVertsUsed, int *pIndicesUsed ) const
{
	int nMaxParticles = ( (nRemainingVertices / 4) > (nRemainingIndices / 6) ) ? nRemainingIndices / 6 : nRemainingVertices / 4;
	int nParticleCount = pParticles->m_nActiveParticles - nFirstParticle;
	if ( nParticleCount > nMaxParticles )
	{
		nParticleCount = nMaxParticles;
	}
	*pVertsUsed = nParticleCount * 4;
	*pIndicesUsed = nParticleCount * 6;
	return nParticleCount;
}

void C_OP_RenderSprites::RenderNonSpriteCardCameraFacingOld( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	// generate the sort list before this code starts messing with the matrices
	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );

	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	VMatrix tempView;

	// Store matrices off so we can restore them in RenderEnd().
	pRenderContext->GetMatrix(MATERIAL_VIEW, &tempView);

	// Force the user clip planes to use the old view matrix
	pRenderContext->EnableUserClipTransformOverride( true );
	pRenderContext->UserClipTransform( tempView );

	// The particle renderers want to do things in camera space
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	size_t xyz_stride;
	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	size_t rot_stride;
	const fltx4 *pRot = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ROTATION, &rot_stride );

	size_t rgb_stride;
	const fltx4 *pRGB = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, &rgb_stride );

	size_t ct_stride;
	const fltx4 *pCreationTimeStamp = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &ct_stride );

	size_t seq_stride;
	const fltx4 *pSequenceNumber = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, &seq_stride );

	size_t ld_stride;
	const fltx4 *pLifeDuration = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_LIFE_DURATION, &ld_stride );


	float flAgeScale;
	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, false );
	
	CSheet *pSheet = pParticles->m_Sheet();
	while ( nParticles )
	{
		int nParticlesInBatch = MIN( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, nParticlesInBatch );
		for( int i = 0; i < nParticlesInBatch; i++ )
		{
			int hParticle = (--pSortList)->m_nIndex;
			int nGroup = hParticle / 4;
			int nOffset = hParticle & 0x3;

			unsigned char ac = pSortList->m_nAlpha;
			if ( ac == 0 )
				continue;

			int nColorIndex = nGroup * rgb_stride;
			float r = SubFloat( pRGB[nColorIndex], nOffset );
			float g = SubFloat( pRGB[nColorIndex+1], nOffset );
			float b = SubFloat( pRGB[nColorIndex+2], nOffset );

			Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
			Assert( (r >= 0.0f) && (g >= 0.0f) && (b >= 0.0f) );
			Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

			unsigned char rc = FastFToC( r );
			unsigned char gc = FastFToC( g );
			unsigned char bc = FastFToC( b );

			float rad = pSortList->m_flRadius;

			int nXYZIndex = nGroup * xyz_stride;
			Vector vecWorldPos( SubFloat( xyz[ nXYZIndex ], nOffset ), SubFloat( xyz[ nXYZIndex+1 ], nOffset ), SubFloat( xyz[ nXYZIndex+2 ], nOffset ) );
			Vector vecViewPos;
			Vector3DMultiplyPosition( tempView, vecWorldPos, vecViewPos );

			if (!IsFinite(vecViewPos.x))
				continue;

			float rot = SubFloat( pRot[ nGroup * rot_stride ], nOffset );
			float sa, ca;
			SinCos( rot, &sa, &ca );

			// Find the sample for this frame
			const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
			if ( pSheet )
			{
				int nSequence = SubFloat( pSequenceNumber[ nGroup * seq_stride ], nOffset );
				if ( m_bFitCycleToLifetime )
				{
					float flLifetime = SubFloat( pLifeDuration[ nGroup * ld_stride ], nOffset );
					flAgeScale = ( flLifetime > 0.0f ) ? ( 1.0f / flLifetime ) * SEQUENCE_SAMPLE_COUNT : 0.0f;
				}
				else
				{
					flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
					if ( m_bAnimateInFPS )
					{
						flAgeScale = flAgeScale / pSheet->m_SheetInfo[nSequence].m_flFrameSpan;
					}
				}
				pSample = GetSampleForSequence( pSheet,
												pParticles->m_flCurTime - SubFloat( pCreationTimeStamp[ nGroup * ct_stride ], nOffset ), 
												flAgeScale, 
												nSequence );
			}
			const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);

			meshBuilder.Position3f( vecViewPos.x + (-ca + sa) * rad, vecViewPos.y + (-sa - ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( vecViewPos.x + (-ca - sa) * rad, vecViewPos.y + (-sa + ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( vecViewPos.x + (ca - sa) * rad, vecViewPos.y + (sa + ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( vecViewPos.x + (ca + sa) * rad, vecViewPos.y + (sa - ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
		}
		meshBuilder.End();
		pMesh->DrawModulated( vecDiffuseModulation );
	}

	pRenderContext->EnableUserClipTransformOverride( false );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
}


void C_OP_RenderSprites::RenderNonSpriteCardCameraFacing( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	// generate the sort list before this code starts messing with the matrices
	int nParticles;
	ParticleFullRenderData_Scalar_View **pSortList = GetExtendedRenderList( 
		pParticles, pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );

	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	VMatrix tempView;

	// Store matrices off so we can restore them in RenderEnd().
	pRenderContext->GetMatrix(MATERIAL_VIEW, &tempView);

	// Force the user clip planes to use the old view matrix
	pRenderContext->EnableUserClipTransformOverride( true );
	pRenderContext->UserClipTransform( tempView );

	// The particle renderers want to do things in camera space
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	float flAgeScale;
	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, false );
	
	CSheet *pSheet = pParticles->m_Sheet();
	while ( nParticles )
	{
		int nParticlesInBatch = MIN( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;
		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, nParticlesInBatch );
		for( int i = 0; i < nParticlesInBatch; i++ )
		{
			ParticleFullRenderData_Scalar_View *pParticle = *(--pSortList);
			unsigned char ac = pParticle->m_nAlpha;
			if ( ac == 0 )
				continue;

			unsigned char rc = pParticle->m_nRed;
			unsigned char gc = pParticle->m_nGreen;
			unsigned char bc = pParticle->m_nBlue;

			float rad = pParticle->m_flRadius;

			Vector vecWorldPos( pParticle->m_flX, pParticle->m_flY, pParticle->m_flZ );
			Vector vecViewPos;
			Vector3DMultiplyPosition( tempView, vecWorldPos, vecViewPos );

			if (!IsFinite(vecViewPos.x))
				continue;

			float rot = pParticle->m_flRotation;
			float sa, ca;
			SinCos( rot, &sa, &ca );

			// Find the sample for this frame
			const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
			if ( pSheet )
			{
				int nSequence = pParticle->m_nSequenceID;
				flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
				if ( m_bAnimateInFPS )
				{
					flAgeScale = flAgeScale / pSheet->m_SheetInfo[nSequence].m_flFrameSpan;
				}
				pSample = GetSampleForSequence( pSheet,
												pParticle->m_flAnimationTimeValue,
												flAgeScale, 
												nSequence );
			}
			const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);

			meshBuilder.Position3f( vecViewPos.x + (-ca + sa) * rad, vecViewPos.y + (-sa - ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( vecViewPos.x + (-ca - sa) * rad, vecViewPos.y + (-sa + ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( vecViewPos.x + (ca - sa) * rad, vecViewPos.y + (sa + ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( vecViewPos.x + (ca + sa) * rad, vecViewPos.y + (sa - ca) * rad, vecViewPos.z );
			meshBuilder.Color4ub( rc, gc, bc, ac );
			meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
		}
		meshBuilder.End();
		pMesh->DrawModulated( vecDiffuseModulation );
	}

	pRenderContext->EnableUserClipTransformOverride( false );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();
}



void C_OP_RenderSprites::RenderNonSpriteCardZRotating( CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, int hParticle, const Vector& vecCameraPos, ParticleRenderData_t const *pSortList ) const
{
	Assert( hParticle != -1 );
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;


	unsigned char ac = pSortList->m_nAlpha;
	if ( ac == 0 )
		return;

	int nColorIndex = nGroup * info.m_nRGBStride;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );

	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
	Assert( (r >= 0.0f) && (g >= 0.0f) && (b >= 0.0f) );
	Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

	unsigned char rc = FastFToC( r );
	unsigned char gc = FastFToC( g );
	unsigned char bc = FastFToC( b );

	float rad = pSortList->m_flRadius;
	float rot = SubFloat( info.m_pRot[ nGroup * info.m_nRotStride ], nOffset );

	float sa, ca;
	SinCos( -rot, &sa, &ca );

	int nXYZIndex = nGroup * info.m_nXYZStride;
	Vector vecWorldPos( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );
	Vector vecViewToPos;
	VectorSubtract( vecWorldPos, vecCameraPos, vecViewToPos );
	float flLength = vecViewToPos.Length();
	if ( flLength < rad / 2 )
		return;

	Vector vecUp( 0, 0, 1 );
	Vector vecRight;
	CrossProduct( vecUp, vecCameraPos, vecRight );
	VectorNormalize( vecRight );

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		pSample = GetSampleForSequence( 
			info.m_pSheet,
			info.m_pParticles->m_flCurTime - SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_flAgeScale, 
			SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset ) );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	vecRight *= rad;

	float x, y;
	Vector vecCorner;

	x = - ca - sa; y = - ca + sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z + y * rad );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	x = - ca + sa; y = + ca + sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z + y * rad );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	x = + ca + sa; y = + ca - sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z + y * rad );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	x = + ca - sa; y = - ca - sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z + y * rad );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.FastQuad( info.m_nVertexOffset );
	info.m_nVertexOffset += 4;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_OP_RenderSprites::RenderNonSpriteCardZRotating( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );
	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;

	SpriteRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, 0, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );

	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, false );
	while ( nParticles )
	{
		int nParticlesInBatch = MIN( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nParticlesInBatch * 4, nParticlesInBatch * 6 );
		info.m_nVertexOffset = 0;

		for( int i = 0; i < nParticlesInBatch; i++ )
		{
			int hParticle = (--pSortList)->m_nIndex;
			RenderNonSpriteCardZRotating( meshBuilder, info, hParticle, vecCameraPos, pSortList );
		}
		meshBuilder.End();
		pMesh->DrawModulated( vecDiffuseModulation );
	}
}

void C_OP_RenderSprites::RenderUnsortedNonSpriteCardZRotating( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );
	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	SpriteRenderInfo_t info;
	info.Init( pParticles, nVertexOffset, flAgeScale, 0, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	int hParticle = nFirstParticle;
	for( int i = 0; i < nParticleCount; i++, hParticle++ )
	{
		RenderNonSpriteCardZRotating( meshBuilder, info, hParticle, vecCameraPos, pSortList );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_OP_RenderSprites::RenderNonSpriteCardOriented( CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, int hParticle, const Vector& vecCameraPos, ParticleRenderData_t const *pSortList ) const
{
	Assert( hParticle != -1 );
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;

	unsigned char ac = pSortList->m_nAlpha;
	if ( ac == 0 )
		return;

	int nColorIndex = nGroup * info.m_nRGBStride;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );

	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );	// infinite color = bad
	Assert( (r >= 0.0f) && (g >= 0.0f) && (b >= 0.0f) );	// negative color = bad
	//Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

	unsigned char rc = FastFToC( r );
	unsigned char gc = FastFToC( g );
	unsigned char bc = FastFToC( b );

	float rad = pSortList->m_flRadius;
	float rot = SubFloat( info.m_pRot[ nGroup * info.m_nRotStride ], nOffset );

	float sa, ca;
	SinCos( -rot, &sa, &ca );

	int nXYZIndex = nGroup * info.m_nXYZStride;
	Vector vecWorldPos( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );
	Vector vecViewToPos;
	VectorSubtract( vecWorldPos, vecCameraPos, vecViewToPos );
	float flLength = vecViewToPos.Length();
	if ( flLength < rad / 2 )
		return;

	Vector vecNormal, vecRight, vecUp;
	if ( m_nOrientationControlPoint < 0 )
	{
		vecNormal.Init( 0, 0, 1 );
		vecRight.Init( 1, 0, 0 );
		vecUp.Init( 0, -1, 0 );
	}
	else
	{
		info.m_pParticles->GetControlPointOrientationAtCurrentTime( 
			m_nOrientationControlPoint, &vecRight, &vecUp, &vecNormal );
	}

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		pSample = GetSampleForSequence( 
			info.m_pSheet,
			info.m_pParticles->m_flCurTime - SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_flAgeScale, 
			SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset ) );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	vecRight *= rad;
	vecUp *= rad;

	float x, y;
	Vector vecCorner;

	x = + ca - sa; y = - ca - sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	VectorMA( vecCorner, y, vecUp, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	x = + ca + sa; y = + ca - sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	VectorMA( vecCorner, y, vecUp, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	x = - ca + sa; y = + ca + sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	VectorMA( vecCorner, y, vecUp, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	x = - ca - sa; y = - ca + sa;
	VectorMA( vecWorldPos, x, vecRight, vecCorner );
	VectorMA( vecCorner, y, vecUp, vecCorner );
	meshBuilder.Position3f( vecCorner.x, vecCorner.y, vecCorner.z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord2f( 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.FastQuad( info.m_nVertexOffset );
	info.m_nVertexOffset += 4;
}

void C_OP_RenderSprites::RenderNonSpriteCardOriented( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMatRenderContext *pRenderContext, IMaterial *pMaterial ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	SpriteRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, 0, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );

	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, false );
	while ( nParticles )
	{
		int nParticlesInBatch = MIN( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nParticlesInBatch * 4, nParticlesInBatch * 6 );
		info.m_nVertexOffset = 0;

		for( int i = 0; i < nParticlesInBatch; i++)
		{
			int hParticle = (--pSortList)->m_nIndex;
			RenderNonSpriteCardOriented( meshBuilder, info, hParticle, vecCameraPos, pSortList );
		}

		meshBuilder.End();
		pMesh->DrawModulated( vecDiffuseModulation );
	}
}

void C_OP_RenderSprites::RenderUnsortedNonSpriteCardOriented( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );
	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	SpriteRenderInfo_t info;
	info.Init( pParticles, nVertexOffset, flAgeScale, 0, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	int hParticle = nFirstParticle;
	for( int i = 0; i < nParticleCount; i++, hParticle++ )
	{
		RenderNonSpriteCardOriented( meshBuilder, info, hParticle, vecCameraPos, pSortList );
	}
}


template<bool bPerParticleOutline, bool bDoNormals, class T> void C_OP_RenderSprites::RenderSpriteCardNew( CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, T const *pSortList ) const
{
	unsigned char ac = pSortList->m_nAlpha;
	if (! ac )
		return;

	unsigned char rc = pSortList->m_nRed;
	unsigned char gc = pSortList->m_nGreen;
	unsigned char bc = pSortList->m_nBlue;

	float rad = pSortList->m_flRadius;
	float rot = pSortList->m_flRotation;
	float yaw = pSortList->m_flYaw;

	float x = pSortList->m_flX;
	float y = pSortList->m_flY;
	float z = pSortList->m_flZ;

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		float flAgeScale = info.m_flAgeScale;
		int nSequence = pSortList->m_nSequenceID;
		if ( m_bAnimateInFPS )
		{
			flAgeScale = flAgeScale / info.m_pParticles->m_Sheet()->m_SheetInfo[nSequence].m_flFrameSpan;
		}
		pSample = GetSampleForSequence( info.m_pSheet,
										pSortList->m_flAnimationTimeValue,
										flAgeScale,
										nSequence );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	const SequenceSampleTextureCoords_t *pSecondTexture0 = &(pSample->m_TextureCoordData[1]);

	static float s_flCornerIds[] = { 0,0, 1,0, 1,1, 0,1 };

	float const *pIds = s_flCornerIds;

	for( int i = 0; i < ( bUseInstancing ? 1 : 4 ); i++ )
	{
		meshBuilder.Position3f( x, y, z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		if ( ! bUseInstancing )
		{
			meshBuilder.TexCoord2fv( 3, pIds );
			pIds += 2;
		}
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		if ( bDoNormals )
		{
			meshBuilder.TexCoord3f( 5, pSortList->NormalX(), pSortList->NormalY(), pSortList->NormalZ() );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 6>();
		}
		else
		{
			if ( bPerParticleOutline )
			{
				meshBuilder.TexCoord4f( 5, pSortList->Red2(), pSortList->Green2(), pSortList->Blue2(), pSortList->Alpha2() );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 6>();
			}
			else
			{
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 5>();
			}
		}
	}
	if ( ! bUseInstancing )
	{
		meshBuilder.FastQuad( info.m_nVertexOffset );
		info.m_nVertexOffset += 4;
	}
}

void C_OP_RenderSprites::RenderSpriteCard( CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, int hParticle, ParticleRenderData_t const *pSortList ) const
{
	Assert( hParticle != -1 );
	unsigned char ac = pSortList->m_nAlpha;
	if (! ac )
		return;
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;

	int nColorIndex = nGroup * info.m_nRGBStride;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );

	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
	Assert( (r >= 0.0f) && (g >= 0.0f) && (b >= 0.0f) );
	Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

	unsigned char rc = FastFToC( r );
	unsigned char gc = FastFToC( g );
	unsigned char bc = FastFToC( b );

	float rad = pSortList->m_flRadius;
	float rot = SubFloat( info.m_pRot[ nGroup * info.m_nRotStride ], nOffset );
	float yaw = SubFloat( info.m_pYaw[ nGroup * info.m_nYawStride ], nOffset );

	int nXYZIndex = nGroup * info.m_nXYZStride;
	float x = SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset );
	float y = SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset );
	float z = SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset );

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		float flAgeScale = info.m_flAgeScale;
// 		if ( m_bFitCycleToLifetime )
// 		{
// 			float flLifetime = SubFloat( pLifeDuration[ nGroup * ld_stride ], nOffset );
// 			flAgeScale = ( flLifetime > 0.0f ) ? ( 1.0f / flLifetime ) * SEQUENCE_SAMPLE_COUNT : 0.0f;
// 		}
		int nSequence = SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset );
		if ( m_bAnimateInFPS )
		{
			flAgeScale = flAgeScale / info.m_pParticles->m_Sheet()->m_SheetInfo[nSequence].m_flFrameSpan;
		}
		pSample = GetSampleForSequence( info.m_pSheet,
										info.m_pParticles->m_flCurTime - SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
										flAgeScale,
										nSequence );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	const SequenceSampleTextureCoords_t *pSecondTexture0 = &(pSample->m_TextureCoordData[1]);

	// Submit 1 (instanced) or 4 (non-instanced) verts (if we're instancing, we don't produce indices either)
	meshBuilder.Position3f( x, y, z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
	meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
	// FIXME: change the vertex decl (remove texcoord3/cornerid) if instancing - need to adjust elements beyond texcoord3 down, though
	if ( !bUseInstancing )
		meshBuilder.TexCoord2f( 3, 0, 0 );
	meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 5>();

	if ( !bUseInstancing )
	{
		meshBuilder.Position3f( x, y, z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 1, 0 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 5>();

		meshBuilder.Position3f( x, y, z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 1, 1 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 5>();

		meshBuilder.Position3f( x, y, z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 0, 1 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 5>();

		meshBuilder.FastQuad( info.m_nVertexOffset );
		info.m_nVertexOffset += 4;
	}
}

void C_OP_RenderSprites::RenderTwoSequenceSpriteCardNew(  CMeshBuilder &meshBuilder, SpriteRenderInfo_t& info, ParticleFullRenderData_Scalar_View const *pSortList ) const
{
	unsigned char rc = pSortList->m_nRed;
	unsigned char gc = pSortList->m_nGreen;
	unsigned char bc = pSortList->m_nBlue;
	unsigned char ac = pSortList->m_nAlpha;

	if ( ac == 0 )
		return;

	float rad = pSortList->m_flRadius;
	float rot = pSortList->m_flRotation;
	float yaw = pSortList->m_flYaw;

	float x = pSortList->m_flX;
	float y = pSortList->m_flY;
	float z = pSortList->m_flZ;

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	const SheetSequenceSample_t *pSample1 = &s_DefaultSheetSequence;

	if ( info.m_pSheet )
	{
		float flAgeScale = info.m_flAgeScale;
		float flAgeScale2 = info.m_flAgeScale2;
		float flAge = pSortList->m_flAnimationTimeValue;

		if ( m_bAnimateInFPS )
		{
			flAgeScale = flAgeScale / info.m_pParticles->m_Sheet()->m_SheetInfo[pSortList->m_nSequenceID].m_flFrameSpan;
			flAgeScale2 = flAgeScale2 / info.m_pParticles->m_Sheet()->m_SheetInfo[pSortList->m_nSequenceID1].m_flFrameSpan;;
		}
		pSample = GetSampleForSequence( 
			info.m_pSheet,
			flAge,
			flAgeScale,
			pSortList->m_nSequenceID );

		pSample1 = GetSampleForSequence( 
			info.m_pSheet,
			flAge,
			flAgeScale2,
			pSortList->m_nSequenceID1 );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);
	const SequenceSampleTextureCoords_t *pSecondTexture0 = &(pSample->m_TextureCoordData[1]);
	const SequenceSampleTextureCoords_t *pSample1Frame = &(pSample1->m_TextureCoordData[0]);

	// Submit 1 (instanced) or 4 (non-instanced) verts (if we're instancing, we don't produce indices either)
	meshBuilder.Position3f( x, y, z );
	meshBuilder.Color4ub( rc, gc, bc, ac );
	meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
	meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
	meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
	// FIXME: change the vertex decl (remove texcoord3/cornerid) if instancing - need to adjust elements beyond texcoord3 down, though
	if ( ! bUseInstancing )
		meshBuilder.TexCoord2f( 3, 0, 0 );
	meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
	meshBuilder.TexCoord4f( 5, pSample1Frame->m_fLeft_U0, pSample1Frame->m_fTop_V0, pSample1Frame->m_fRight_U0, pSample1Frame->m_fBottom_V0 );
	meshBuilder.TexCoord4f( 6, pSample1Frame->m_fLeft_U1, pSample1Frame->m_fTop_V1, pSample1Frame->m_fRight_U1, pSample1Frame->m_fBottom_V1 );
	meshBuilder.TexCoord4f( 7, pSample1->m_fBlendFactor, 0, 0, 0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 8>();

	if ( !bUseInstancing )
	{
		meshBuilder.Position3f( x, y, z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 1, 0 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 5, pSample1Frame->m_fLeft_U0, pSample1Frame->m_fTop_V0, pSample1Frame->m_fRight_U0, pSample1Frame->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 6, pSample1Frame->m_fLeft_U1, pSample1Frame->m_fTop_V1, pSample1Frame->m_fRight_U1, pSample1Frame->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 7, pSample1->m_fBlendFactor, 0, 0, 0 );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 8>();

		meshBuilder.Position3f( x, y, z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 1, 1 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 5, pSample1Frame->m_fLeft_U0, pSample1Frame->m_fTop_V0, pSample1Frame->m_fRight_U0, pSample1Frame->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 6, pSample1Frame->m_fLeft_U1, pSample1Frame->m_fTop_V1, pSample1Frame->m_fRight_U1, pSample1Frame->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 7, pSample1->m_fBlendFactor, 0, 0, 0 );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 8>();

		meshBuilder.Position3f( x, y, z );
		meshBuilder.Color4ub( rc, gc, bc, ac );
		meshBuilder.TexCoord4f( 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 1, pSample0->m_fLeft_U1, pSample0->m_fTop_V1, pSample0->m_fRight_U1, pSample0->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 2, pSample->m_fBlendFactor, rot, rad, yaw );
		meshBuilder.TexCoord2f( 3, 0, 1 );
		meshBuilder.TexCoord4f( 4, pSecondTexture0->m_fLeft_U0, pSecondTexture0->m_fTop_V0, pSecondTexture0->m_fRight_U0, pSecondTexture0->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 5, pSample1Frame->m_fLeft_U0, pSample1Frame->m_fTop_V0, pSample1Frame->m_fRight_U0, pSample1Frame->m_fBottom_V0 );
		meshBuilder.TexCoord4f( 6, pSample1Frame->m_fLeft_U1, pSample1Frame->m_fTop_V1, pSample1Frame->m_fRight_U1, pSample1Frame->m_fBottom_V1 );
		meshBuilder.TexCoord4f( 7, pSample1->m_fBlendFactor, 0, 0, 0 );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 8>();

		meshBuilder.FastQuad( info.m_nVertexOffset );
		info.m_nVertexOffset += 4;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_OP_RenderSprites::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
{
	// See if we need to cull this system
	if ( ShouldCullParticleSystem( &m_cullData, pParticles, pRenderContext, nViewRecursionDepth ) )
		return;

	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();
	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	Vector vecOrigin = vec3_origin;
	if ( pCtx->m_VisibilityData.m_bUseVisibility )
	{
		SetupParticleVisibility( pParticles, &pCtx->m_VisibilityData, &VisibilityInputs, &pCtx->m_nQueryHandle, pRenderContext );
	}

	IMaterialVar* pVar = pMaterial->FindVarFast( "$orientation", &pCtx->m_nOrientationVarToken );
	if ( pVar )
	{
		pVar->SetIntValue( MAX( 0, MIN( m_nOrientationType, MAX_PARTICLE_ORIENTATION_TYPES ) ) );
	}

	pRenderContext->Bind( pMaterial );

	if ( !pMaterial->IsSpriteCard() )
	{
		if ( !pCtx->m_bDidPerfWarning )
		{
			pCtx->m_bDidPerfWarning = true;
// 			DevWarning( "** PERF WARNING! The particle system %s is using a non-spritecard based material.\n",
// 				pParticles->m_pDef->GetName() );
		}

		switch( m_nOrientationType )
		{
		case 0:
			if ( (! m_bFitCycleToLifetime ) )
				RenderNonSpriteCardCameraFacing( pParticles, vecDiffuseModulation, pContext, pRenderContext, pMaterial );
			else
				RenderNonSpriteCardCameraFacingOld( pParticles, vecDiffuseModulation, pContext, pRenderContext, pMaterial );
			break;

		case 1:
			RenderNonSpriteCardZRotating( pParticles, vecDiffuseModulation, pContext, pRenderContext, pMaterial );
			break;
		
		case 2:
			RenderNonSpriteCardOriented( pParticles, vecDiffuseModulation, pContext, pRenderContext, pMaterial );
			break;
		}

		return;
	}

	if ( m_nOrientationType == 2 )
	{
		IMaterialVar* pVar = pMaterial->FindVarFast( "$orientationMatrix", &pCtx->m_nOrientationMatrixVarToken );
		if ( pVar )
		{
			VMatrix mat;
			if ( m_nOrientationControlPoint < 0 )
			{
				MatrixSetIdentity( mat );
			}
			else
			{
				pParticles->GetControlPointTransformAtCurrentTime( m_nOrientationControlPoint, &mat );
			}
			pVar->SetMatrixValue( mat );
		}
	}

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	float flAgeScale2 = m_flAnimationRate2 * SEQUENCE_SAMPLE_COUNT;

	SpriteRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, flAgeScale2, pParticles->m_Sheet() );

	MaterialPrimitiveType_t	primType = bUseInstancing ? MATERIAL_INSTANCED_QUADS : MATERIAL_TRIANGLES;
	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, bUseInstancing );

	// Reset the particle cache if we're sprite card material, it isn't sorted, and it doesn't use queries
	bool bShouldSort = pParticles->m_pDef->m_bShouldSort;
	CCachedParticleBatches *pCachedBatches = NULL;
	MaterialThreadMode_t nThreadMode = g_pMaterialSystem->GetThreadMode();
	if ( nThreadMode != MATERIAL_SINGLE_THREADED && !bShouldSort && !pCtx->m_VisibilityData.m_bUseVisibility )
	{
		pParticles->ResetParticleCache();
		pCachedBatches = pParticles->GetCachedParticleBatches();
	}
	int nBatchCount = 0;

	if ( pCtx->m_bPerParticleGlow )
	{
		int nParticles;
		ParticleRenderDataWithOutlineInformation_Scalar_View **pSortList = GetExtendedRenderListWithPerParticleGlow( 
			pParticles, pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );
		while ( nParticles )
		{
			int nParticlesInBatch = MIN( nMaxParticlesInBatch, nParticles );
			nParticles -= nParticlesInBatch;
			
			int vertexCount	= bUseInstancing ? nParticlesInBatch	: nParticlesInBatch * 4;
			int indexCount	= bUseInstancing ? 0					: nParticlesInBatch * 6;
			
			IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
			
			// See if we have a cached batch
			ICachedPerFrameMeshData *pCachedBatch = pCachedBatches ? pCachedBatches->GetCachedBatch( nBatchCount ) : NULL;
			if ( pCachedBatch )
			{
				// This copies all of the VB/IB pointers and data out of the pCachedBatch back into the pMesh
				pMesh->ReconstructFromCachedPerFrameMeshData( pCachedBatch );
				pSortList -= nParticlesInBatch;
			}
			else
			{
				CMeshBuilder meshBuilder;
				if ( bUseInstancing )
				{
					meshBuilder.Begin( pMesh, primType, vertexCount );
				}
				else
				{
					meshBuilder.Begin( pMesh, primType, vertexCount, indexCount );
				}
				info.m_nVertexOffset = 0;
				for( int i = 0; i < nParticlesInBatch; i++ )
				{
					ParticleRenderDataWithOutlineInformation_Scalar_View *pParticle = *(--pSortList);
					RenderSpriteCardNew<true, false>( meshBuilder, info, pParticle );
				}
				meshBuilder.End();

				// If we have a list of cached batches, cache them off so that if we try to render this sytem again for the current frame,
				// we have a cached all of the vb and ib pointers.
				if ( pCachedBatches )
				{
					pCachedBatch = pMesh->GetCachedPerFrameMeshData();
					pCachedBatches->SetCachedBatch( nBatchCount, pCachedBatch );
				}
			}

			Vector vMins, vMaxs;
			pParticles->GetBounds( &vMins, &vMaxs );

			VMatrix	MinMaxParms( vMins.x, vMins.y, vMins.z, 0.0f,
								 vMaxs.x, vMaxs.y, vMaxs.z, 0.0f,
								 0.0f, 0.0f, 0.0f, 0.0f, 
								 0.0f, 0.0f, 0.0f, 0.0f );
			pRenderContext->MatrixMode( MATERIAL_MATRIX_UNUSED0 );
			pRenderContext->LoadMatrix( MinMaxParms );

			nBatchCount++;

			pMesh->DrawModulated( vecDiffuseModulation );
		}
	}
	else
	{
		int nParticles;
		ParticleFullRenderData_Scalar_View **pSortList = NULL;
		ParticleRenderDataWithNormal_Scalar_View **pSortListWithNormal = NULL;
		if ( m_nOrientationType == 3 )
		{
			pSortListWithNormal = GetExtendedRenderListWithNormals( 
				pParticles, pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );
		}
		else
		{
			pSortList = GetExtendedRenderList( 
				pParticles, pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );
		}
		while ( nParticles )
		{
			int nParticlesInBatch = MIN( nMaxParticlesInBatch, nParticles );
			nParticles -= nParticlesInBatch;

			int vertexCount	= bUseInstancing ? nParticlesInBatch	: nParticlesInBatch * 4;
			int indexCount	= bUseInstancing ? 0					: nParticlesInBatch * 6;

			IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

			Vector vMins, vMaxs;
			pParticles->GetBounds( &vMins, &vMaxs );

			// See if we have a cached batch
			ICachedPerFrameMeshData *pCachedBatch = pCachedBatches ? pCachedBatches->GetCachedBatch( nBatchCount ) : NULL;
			if ( pCachedBatch )
			{
				// This copies all of the VB/IB pointers and data out of the pCachedBatch back into the pMesh
				pMesh->ReconstructFromCachedPerFrameMeshData( pCachedBatch );
				pSortList -= nParticlesInBatch;
			}
			else
			{
				CMeshBuilder meshBuilder;
				if ( bUseInstancing )
				{
					meshBuilder.Begin( pMesh, primType, vertexCount );
				}
				else
				{
					meshBuilder.Begin( pMesh, primType, vertexCount, indexCount );
				}
				info.m_nVertexOffset = 0;
				if ( pSortListWithNormal )					// align to particle normal
				{
					for( int i = 0; i < nParticlesInBatch; i++ )
					{
						ParticleRenderDataWithNormal_Scalar_View *pParticle = *( --pSortListWithNormal );
						RenderSpriteCardNew<false, true>( meshBuilder, info, pParticle );
					}
				}
				else
				{
					if ( meshBuilder.TextureCoordinateSize( 5 ) )		// second sequence? per particle outline?
					{
						for( int i = 0; i < nParticlesInBatch; i++ )
						{
							ParticleFullRenderData_Scalar_View *pParticle = *(--pSortList);
							RenderTwoSequenceSpriteCardNew( meshBuilder, info, pParticle );
						}
					}
					else
					{
						for( int i = 0; i < nParticlesInBatch; i++ )
						{
							ParticleFullRenderData_Scalar_View *pParticle = *(--pSortList);
							RenderSpriteCardNew<false, false>( meshBuilder, info, pParticle );
						}
					}
				}
				meshBuilder.End();

				// If we have a list of cached batches, cache them off so that if we try to render this sytem again for the current frame,
				// we have a cached all of the vb and ib pointers.
				if ( pCachedBatches )
				{
					pCachedBatch = pMesh->GetCachedPerFrameMeshData();
					pCachedBatches->SetCachedBatch( nBatchCount, pCachedBatch );
				}
			}


			VMatrix	MinMaxParms( vMins.x, vMins.y, vMins.z, 0.0f,
								 vMaxs.x, vMaxs.y, vMaxs.z, 0.0f,
								 0.0f, 0.0f, 0.0f, 0.0f, 
								 0.0f, 0.0f, 0.0f, 0.0f );
			pRenderContext->MatrixMode( MATERIAL_MATRIX_UNUSED0 );
			pRenderContext->LoadMatrix( MinMaxParms );

			nBatchCount++;

			pMesh->DrawModulated( vecDiffuseModulation );
		}
	}
}



void C_OP_RenderSprites::RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	if ( !pParticles->m_pDef->GetMaterial()->IsSpriteCard() )
	{
		switch( m_nOrientationType )
		{
		case 0:
			// FIXME: Implement! Requires removing MATERIAL_VIEW modification from sorted version
			Warning( "C_OP_RenderSprites::RenderUnsorted: Attempting to use an unimplemented sprite renderer for system \"%s\"!\n",
				pParticles->m_pDef->GetName() );
//			RenderUnsortedNonSpriteCardCameraFacing( pParticles, pContext, pRenderContext, meshBuilder, nVertexOffset, nFirstParticle, nParticleCount );
			break;

		case 1:
			RenderUnsortedNonSpriteCardZRotating( pParticles, pContext, pRenderContext, meshBuilder, nVertexOffset, nFirstParticle, nParticleCount );
			break;

		case 2:
			RenderUnsortedNonSpriteCardOriented( pParticles, pContext, pRenderContext, meshBuilder, nVertexOffset, nFirstParticle, nParticleCount );
			break;
		}
		return;
	}

	C_OP_RenderSpritesContext_t *pCtx = reinterpret_cast<C_OP_RenderSpritesContext_t *>( pContext );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	float flAgeScale2 = m_flAnimationRate2 * SEQUENCE_SAMPLE_COUNT;

	SpriteRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, flAgeScale2, pParticles->m_Sheet() );

	int hParticle = nFirstParticle;

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	for( int i = 0; i < nParticleCount; i++, hParticle++ )
	{
		RenderSpriteCard( meshBuilder, info, hParticle, pSortList );
	}
}

//
//
//
//

struct SpriteTrailRenderInfo_t : public SpriteRenderInfo_t
{
	size_t m_nPrevXYZStride;
	const fltx4 *m_pPrevXYZ;
	size_t length_stride;
	const fltx4 *m_pLength;

	const fltx4 *m_pCreationTime;
	size_t m_nCreationTimeStride;


	void Init( CParticleCollection *pParticles, int nVertexOffset, float flAgeScale, CSheet *pSheet )
	{
		SpriteRenderInfo_t::Init( pParticles, nVertexOffset, flAgeScale, 0, pSheet );
		m_pParticles = pParticles;
		m_pPrevXYZ = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, &m_nPrevXYZStride );
		m_pLength = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_TRAIL_LENGTH, &length_stride );
		m_pCreationTime = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &m_nCreationTimeStride );
	}
};

struct FastSpriteTrailVertex_t
{
	Vector m_vPos;
	int m_nColor;
	Vector4D m_vTexcoord[ 6 ];
};

class C_OP_RenderSpritesTrail : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderSpritesTrail );

	struct C_OP_RenderSpriteTrailContext_t
	{
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
	};

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = 0;
		if ( VisibilityInputs.m_nCPin >= 0 )
			nMask |= 1ULL << VisibilityInputs.m_nCPin; 
		return nMask;
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderSpriteTrailContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RenderSpriteTrailContext_t *pCtx = reinterpret_cast<C_OP_RenderSpriteTrailContext_t *>( pContext );
		if ( ( VisibilityInputs.m_nCPin >= 0 ) || ( VisibilityInputs.m_flRadiusScaleFOVBase > 0 ) )
			pCtx->m_VisibilityData.m_bUseVisibility = true;
		else
			pCtx->m_VisibilityData.m_bUseVisibility = false;
	}

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		pDef->SetMaxTailLength( m_flMaxLength );
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK | 
			PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK |
			PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK | PARTICLE_ATTRIBUTE_TRAIL_LENGTH_MASK;
	}

	virtual int GetParticlesToRender( CParticleCollection *pParticles, void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, int *pVertsUsed, int *pIndicesUsed ) const ;
	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const;
	virtual void RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;

	bool RenderSpriteTrail( CMeshBuilder &meshBuilder, 
							int nCurrentVertex, int nCurrentIndex,
							SpriteTrailRenderInfo_t& info, int hParticle,
							const Vector &vecCameraPos, float flOODt, ParticleRenderData_t const *pSortList ) const;

	template <bool bFastPath>
	bool RenderSpriteTrailSpriteCard( CMeshBuilder &meshBuilder, int nCurrentVertex, // Slow method params
							FastSpriteTrailVertex_t *RESTRICT pVertices, uint32 *RESTRICT pIndices, int nIndexOffset,	// Fast method params
							SpriteTrailRenderInfo_t& info, int hParticle, 
							float flOODt, ParticleRenderData_t const *pSortlist ) const;

	Vector4D m_FadeColor;
	float m_flAnimationRate;
	float m_flLengthFadeInTime;
	float m_flMaxLength;
	float m_flMinLength;
	bool m_bConstrainRadius;
	bool m_bIgnoreDT;
	CullSystemByControlPointData_t m_cullData;
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderSpritesTrail, "render_sprite_trail", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderSpritesTrail ) 
	DMXELEMENT_UNPACK_FIELD( "animation rate", ".1", float, m_flAnimationRate )
	DMXELEMENT_UNPACK_FIELD( "length fade in time", "0", float, m_flLengthFadeInTime )
	DMXELEMENT_UNPACK_FIELD( "max length", "2000", float, m_flMaxLength )
	DMXELEMENT_UNPACK_FIELD( "min length", "0", float, m_flMinLength )
	DMXELEMENT_UNPACK_FIELD( "constrain radius to length", "1", bool, m_bConstrainRadius )
	DMXELEMENT_UNPACK_FIELD( "ignore delta time", "0", bool, m_bIgnoreDT )
	DMXELEMENT_UNPACK_FIELD( "tail color and alpha scale factor", "1 1 1 1", Vector4D, m_FadeColor )
	DMXELEMENT_UNPACK_FIELD( CULL_CP_NORMAL_DESCRIPTOR, "-1", int, m_cullData.m_nCullControlPoint )
	DMXELEMENT_UNPACK_FIELD( CULL_RECURSION_DEPTH_DESCRIPTOR, "-1", int, m_cullData.m_nViewRecursionDepthStart )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderSpritesTrail )

int C_OP_RenderSpritesTrail::GetParticlesToRender( CParticleCollection *pParticles, 
												   void *pContext, int nFirstParticle, int nRemainingVertices,
												   int nRemainingIndices, 
												   int *pVertsUsed, int *pIndicesUsed ) const
{
	int nMaxParticles = ( (nRemainingVertices / 4) > (nRemainingIndices / 6) ) ? nRemainingIndices / 6 : nRemainingVertices / 4;
	int nParticleCount = pParticles->m_nActiveParticles - nFirstParticle;
	if ( nParticleCount > nMaxParticles )
	{
		nParticleCount = nMaxParticles;
	}
	*pVertsUsed = nParticleCount * 4;
	*pIndicesUsed = nParticleCount * 6;
	return nParticleCount;
}

template <bool bFastPath>
bool C_OP_RenderSpritesTrail::RenderSpriteTrailSpriteCard( CMeshBuilder &meshBuilder, int nCurrentVertex, // Slow method params

														   FastSpriteTrailVertex_t *RESTRICT pVertices, uint32 *RESTRICT pIndices, int nIndexOffset,	// Fast method params

														   // Common params
														   SpriteTrailRenderInfo_t& info, int hParticle,
														   float flOODt, ParticleRenderData_t const *pSortList ) const
{
	// Setup our alpha
	unsigned char ac = pSortList->m_nAlpha;
	if ( ac == 0 )
		return false;
	Assert( hParticle != -1 );
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;

	// Setup our colors
	unsigned char rc = 255;
	unsigned char gc = 255;
	unsigned char bc = 255;
	
	int nColorIndex = nGroup * info.m_nRGBStride;
	float a = pSortList->m_nAlpha / 255.0f;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );
	
	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
	Assert( (r >= -FLT_EPSILON) && (g >= -FLT_EPSILON) && (b >= -FLT_EPSILON) );
	Assert( (r <= 1.0f + FLT_EPSILON) && (g <= 1.0f + FLT_EPSILON) && (b <= 1.0f + FLT_EPSILON) );
		
	rc = FastFToC( r );
	gc = FastFToC( g );
	bc = FastFToC( b );

	// Setup the scale and rotation
	float rad = pSortList->m_flRadius;

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		pSample = GetSampleForSequence( 
			info.m_pSheet,
			info.m_pParticles->m_flCurTime - SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_flAgeScale, 
			SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset ) );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);

	int nCreationTimeIndex = nGroup * info.m_nCreationTimeStride;
	float flAge = info.m_pParticles->m_flCurTime - SubFloat( info.m_pCreationTimeStamp[ nCreationTimeIndex ], nOffset );

	float flLengthScale = MIN( 1.0, ( flAge / m_flLengthFadeInTime ) );

	int nXYZIndex = nGroup * info.m_nXYZStride;
	Vector vecWorldPos( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );
	Vector vecViewPos = vecWorldPos;

	// Get our screenspace last position
	int nPrevXYZIndex = nGroup * info.m_nPrevXYZStride;
	Vector vecPrevWorldPos( SubFloat( info.m_pPrevXYZ[ nPrevXYZIndex ], nOffset ), SubFloat( info.m_pPrevXYZ[ nPrevXYZIndex+1 ], nOffset ), SubFloat( info.m_pPrevXYZ[ nPrevXYZIndex+2 ], nOffset ) );
	Vector vecPrevViewPos = vecPrevWorldPos;

	// Get the delta direction and find the magnitude, then scale the length by the desired length amount
	Vector vecDelta;
	VectorSubtract( vecPrevViewPos, vecViewPos, vecDelta );
	float flMagSquared = vecDelta.LengthSqr();
	float flInvMag = ( flMagSquared == 0.0f ) ? 0 : FastRSqrtFast( flMagSquared );
	float flMag = flInvMag * flMagSquared;

	vecDelta.x *= flInvMag;
	vecDelta.y *= flInvMag;
	vecDelta.z *= flInvMag;
	float flLength = flLengthScale * flMag * flOODt * SubFloat( info.m_pLength[ nGroup * info.length_stride ], nOffset );
	if ( flLength <= 0.0f )
		return false;

	flLength = MAX( m_flMinLength, MIN( m_flMaxLength, flLength ) );

	vecDelta *= flLength;

	// Fade the width as the length fades to keep it at a square aspect ratio
	if ( m_bConstrainRadius )
	{
		rad = MIN( rad, flLength );
	}

	Vector p0 = vecWorldPos - vecDelta;
	Vector p1 = vecWorldPos;
	Vector p2 = vecWorldPos + vecDelta;
	Vector p3 = vecWorldPos + 2 * vecDelta;

	Vector4D vFadeColor = ( Vector4D( r, g, b, a ) * m_FadeColor );

	int nColor = PackRGBToPlatformColor( rc, gc, bc, ac );
	Vector4D vTextureRange( pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );

	if ( bFastPath )
	{
		// Vert0
		pVertices->m_vPos.Init( 0, 1, 0 );
		pVertices->m_nColor = nColor;
		pVertices->m_vTexcoord[ 0 ].Init( p0.x, p0.y, p0.z, rad );
		pVertices->m_vTexcoord[ 1 ].Init( p1.x, p1.y, p1.z, rad );
		pVertices->m_vTexcoord[ 2 ].Init( p2.x, p2.y, p2.z, rad );
		pVertices->m_vTexcoord[ 3 ].Init( p3.x, p3.y, p3.z, rad );
		pVertices->m_vTexcoord[ 4 ] = vTextureRange;
		pVertices->m_vTexcoord[ 5 ] = vFadeColor;
		pVertices++;

		// Vert1
		pVertices->m_vPos.Init( 0, 1, 1 );
		pVertices->m_nColor = nColor;
		pVertices->m_vTexcoord[ 0 ].Init( p0.x, p0.y, p0.z, rad );
		pVertices->m_vTexcoord[ 1 ].Init( p1.x, p1.y, p1.z, rad );
		pVertices->m_vTexcoord[ 2 ].Init( p2.x, p2.y, p2.z, rad );
		pVertices->m_vTexcoord[ 3 ].Init( p3.x, p3.y, p3.z, rad );
		pVertices->m_vTexcoord[ 4 ] = vTextureRange;
		pVertices->m_vTexcoord[ 5 ] = vFadeColor;
		pVertices++;

		// Vert2
		pVertices->m_vPos.Init( 1, 0, 1 );
		pVertices->m_nColor = nColor;
		pVertices->m_vTexcoord[ 0 ].Init( p0.x, p0.y, p0.z, rad );
		pVertices->m_vTexcoord[ 1 ].Init( p1.x, p1.y, p1.z, rad );
		pVertices->m_vTexcoord[ 2 ].Init( p2.x, p2.y, p2.z, rad );
		pVertices->m_vTexcoord[ 3 ].Init( p3.x, p3.y, p3.z, rad );
		pVertices->m_vTexcoord[ 4 ] = vTextureRange;
		pVertices->m_vTexcoord[ 5 ] = vFadeColor;
		pVertices++;

		// Vert3
		pVertices->m_vPos.Init( 1, 0, 0 );
		pVertices->m_nColor = nColor;
		pVertices->m_vTexcoord[ 0 ].Init( p0.x, p0.y, p0.z, rad );
		pVertices->m_vTexcoord[ 1 ].Init( p1.x, p1.y, p1.z, rad );
		pVertices->m_vTexcoord[ 2 ].Init( p2.x, p2.y, p2.z, rad );
		pVertices->m_vTexcoord[ 3 ].Init( p3.x, p3.y, p3.z, rad );
		pVertices->m_vTexcoord[ 4 ] = vTextureRange;
		pVertices->m_vTexcoord[ 5 ] = vFadeColor;
	}
	else
	{
		// Vert0
		meshBuilder.Position3f( nCurrentVertex, 0.0f, 1.0f, 0.0f );
		meshBuilder.Color4ub( nCurrentVertex, rc, gc, bc, ac );
		meshBuilder.TexCoord4f( nCurrentVertex, 0, p0.x, p0.y, p0.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex, 1, p1.x, p1.y, p1.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex, 2, p2.x, p2.y, p2.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex, 3, p3.x, p3.y, p3.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex, 4, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4fv( nCurrentVertex, 5, vFadeColor.Base() );
		
		// Vert1
		meshBuilder.Position3f( nCurrentVertex + 1, 0.0f, 1.0f, 1.0f );
		meshBuilder.Color4ub( nCurrentVertex + 1, rc, gc, bc, ac );
		meshBuilder.TexCoord4f( nCurrentVertex + 1, 0, p0.x, p0.y, p0.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 1, 1, p1.x, p1.y, p1.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 1, 2, p2.x, p2.y, p2.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 1, 3, p3.x, p3.y, p3.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 1, 4, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4fv( nCurrentVertex + 1, 5, vFadeColor.Base() );

		// Vert2
		meshBuilder.Position3f( nCurrentVertex + 2, 1.0f, 0.0f, 1.0f );
		meshBuilder.Color4ub( nCurrentVertex + 2, rc, gc, bc, ac );
		meshBuilder.TexCoord4f( nCurrentVertex + 2, 0, p0.x, p0.y, p0.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 2, 1, p1.x, p1.y, p1.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 2, 2, p2.x, p2.y, p2.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 2, 3, p3.x, p3.y, p3.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 2, 4, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4fv( nCurrentVertex + 2, 5, vFadeColor.Base() );

		// Vert3
		meshBuilder.Position3f( nCurrentVertex + 3, 1.0f, 0.0f, 0.0f );
		meshBuilder.Color4ub( nCurrentVertex + 3, rc, gc, bc, ac );
		meshBuilder.TexCoord4f( nCurrentVertex + 3, 0, p0.x, p0.y, p0.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 3, 1, p1.x, p1.y, p1.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 3, 2, p2.x, p2.y, p2.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 3, 3, p3.x, p3.y, p3.z, rad );
		meshBuilder.TexCoord4f( nCurrentVertex + 3, 4, pSample0->m_fLeft_U0, pSample0->m_fTop_V0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );
		meshBuilder.TexCoord4fv( nCurrentVertex + 3, 5, vFadeColor.Base() );
	}

	// Quad
	unsigned short nIndex = info.m_nVertexOffset + nIndexOffset;
	pIndices[ 0 ] = TwoIndices( nIndex, nIndex + 1 );
	pIndices[ 1 ] = TwoIndices( nIndex + 2, nIndex );
	pIndices[ 2 ] = TwoIndices( nIndex + 2, nIndex + 3 );
	info.m_nVertexOffset += 4;

	return true;
}

bool C_OP_RenderSpritesTrail::RenderSpriteTrail( CMeshBuilder &meshBuilder, 
												 int nCurrentVertex, int nCurrentIndex,
												 SpriteTrailRenderInfo_t& info, int hParticle,
												 const Vector &vecCameraPos, float flOODt, ParticleRenderData_t const *pSortList ) const
{
	Assert( hParticle != -1 );
	// Setup our alpha
	unsigned char ac = pSortList->m_nAlpha;
	if ( ac == 0 )
		return false;
	int nGroup = hParticle / 4;
	int nOffset = hParticle & 0x3;


	// Setup our colors
	int nColorIndex = nGroup * info.m_nRGBStride;
	float r = SubFloat( info.m_pRGB[nColorIndex], nOffset );
	float g = SubFloat( info.m_pRGB[nColorIndex+1], nOffset );
	float b = SubFloat( info.m_pRGB[nColorIndex+2], nOffset );

	Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
	Assert( (r >= 0.0f) && (g >= 0.0f) && (b >= 0.0f) );
	Assert( (r <= 1.0f) && (g <= 1.0f) && (b <= 1.0f) );

	unsigned char rc = FastFToC( r );
	unsigned char gc = FastFToC( g );
	unsigned char bc = FastFToC( b );

	// Setup the scale and rotation
	float rad = pSortList->m_flRadius;

	// Find the sample for this frame
	const SheetSequenceSample_t *pSample = &s_DefaultSheetSequence;
	if ( info.m_pSheet )
	{
		pSample = GetSampleForSequence( 
			info.m_pSheet,
			info.m_pParticles->m_flCurTime - SubFloat( info.m_pCreationTimeStamp[ nGroup * info.m_nCreationTimeStride ], nOffset ), 
			info.m_flAgeScale, 
			SubFloat( info.m_pSequenceNumber[ nGroup * info.m_nSequenceStride ], nOffset ) );
	}

	const SequenceSampleTextureCoords_t *pSample0 = &(pSample->m_TextureCoordData[0]);

	int nCreationTimeIndex = nGroup * info.m_nCreationTimeStride;
	float flAge = info.m_pParticles->m_flCurTime - SubFloat( info.m_pCreationTimeStamp[ nCreationTimeIndex ], nOffset );

	float flLengthScale = ( flAge >= m_flLengthFadeInTime ) ? 1.0 : ( flAge / m_flLengthFadeInTime );

	int nXYZIndex = nGroup * info.m_nXYZStride;
	Vector vecWorldPos( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );
	Vector vecViewPos = vecWorldPos;

	// Get our screenspace last position
	int nPrevXYZIndex = nGroup * info.m_nPrevXYZStride;
	Vector vecPrevWorldPos( SubFloat( info.m_pPrevXYZ[ nPrevXYZIndex ], nOffset ), SubFloat( info.m_pPrevXYZ[ nPrevXYZIndex+1 ], nOffset ), SubFloat( info.m_pPrevXYZ[ nPrevXYZIndex+2 ], nOffset ) );
	Vector vecPrevViewPos = vecPrevWorldPos;

	// Get the delta direction and find the magnitude, then scale the length by the desired length amount
	Vector vecDelta;
	// Explicitely sub and find length here, since calling VectorSubtract/VectorNormalize causes 
	// the results to be stored in memory.
	vecDelta.x = vecPrevViewPos.x - vecViewPos.x;
	vecDelta.y = vecPrevViewPos.y - vecViewPos.y;
	vecDelta.z = vecPrevViewPos.z - vecViewPos.z;
	float flMag = sqrtf( vecDelta.x * vecDelta.x + vecDelta.y * vecDelta.y + vecDelta.z * vecDelta.z );
	float flInvMag = 1.0f / flMag;
	vecDelta.x *= flInvMag;
	vecDelta.y *= flInvMag;
	vecDelta.z *= flInvMag;
	float flLength = flLengthScale * flMag * flOODt * SubFloat( info.m_pLength[ nGroup * info.length_stride ], nOffset );
	if ( flLength <= 0.0f )
		return false;

	flLength = MAX( m_flMinLength, MIN( m_flMaxLength, flLength ) );

	vecDelta *= flLength;

	// Fade the width as the length fades to keep it at a square aspect ratio
	if ( ( flLength < rad ) && ( m_bConstrainRadius ) )
	{
		rad = flLength;
	}

	// Find our tangent direction which "fattens" the line
	Vector vDirToBeam, vTangentY;
	VectorSubtract( vecWorldPos, vecCameraPos, vDirToBeam );
	CrossProduct( vDirToBeam, vecDelta, vTangentY );
	// VectorNormalizeFast stores in sse registers, does math, and then writes out... causing LHS on the consoles
	flMag = sqrtf( vTangentY.x * vTangentY.x + vTangentY.y * vTangentY.y + vTangentY.z * vTangentY.z );
	flInvMag = 1.0f / flMag;
	vTangentY.x *= flInvMag;
	vTangentY.y *= flInvMag;
	vTangentY.z *= flInvMag;

	// Calculate the verts we'll use as our points
	Vector verts[4];
	VectorMA( vecWorldPos, rad*0.5f, vTangentY, verts[0] );
	VectorMA( vecWorldPos, -rad*0.5f, vTangentY, verts[1] );
	VectorAdd( verts[0], vecDelta, verts[3] );
	VectorAdd( verts[1], vecDelta, verts[2] );
	Assert( verts[0].IsValid() && verts[1].IsValid() && verts[2].IsValid() && verts[3].IsValid() );

	meshBuilder.Position3fv( nCurrentVertex, verts[0].Base() );
	meshBuilder.Color4ub( nCurrentVertex, rc, gc, bc, ac );
	meshBuilder.TexCoord2f( nCurrentVertex, 0, pSample0->m_fLeft_U0, pSample0->m_fBottom_V0 );

	meshBuilder.Position3fv( nCurrentVertex + 1, verts[1].Base() );
	meshBuilder.Color4ub( nCurrentVertex + 1, rc, gc, bc, ac );
	meshBuilder.TexCoord2f( nCurrentVertex + 1, 0, pSample0->m_fRight_U0, pSample0->m_fBottom_V0 );

	meshBuilder.Position3fv( nCurrentVertex + 2, verts[2].Base() );
	meshBuilder.Color4ub( nCurrentVertex + 2, rc, gc, bc, ac );
	meshBuilder.TexCoord2f( nCurrentVertex + 2, 0, pSample0->m_fRight_U0, pSample0->m_fTop_V0 );

	meshBuilder.Position3fv( nCurrentVertex + 3, verts[3].Base() );
	meshBuilder.Color4ub( nCurrentVertex + 3, rc, gc, bc, ac );
	meshBuilder.TexCoord2f( nCurrentVertex + 3, 0, pSample0->m_fLeft_U0, pSample0->m_fTop_V0 );

	meshBuilder.FastQuad( nCurrentIndex, info.m_nVertexOffset );
	info.m_nVertexOffset += 4;

	return true;
}

void C_OP_RenderSpritesTrail::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
{
	// See if we need to cull this system
	if ( ShouldCullParticleSystem( &m_cullData, pParticles, pRenderContext, nViewRecursionDepth ) )
		return;

	C_OP_RenderSpriteTrailContext_t *pCtx = reinterpret_cast<C_OP_RenderSpriteTrailContext_t *>( pContext );
	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	if ( pCtx->m_VisibilityData.m_bUseVisibility )
	{
		SetupParticleVisibility( pParticles, &pCtx->m_VisibilityData, &VisibilityInputs, &pCtx->m_nQueryHandle, pRenderContext );
	}

	// Reset the particle cache if we're sprite card material, not sorted, and don't need visibility
	bool bSpriteCard = pMaterial->IsSpriteCard();
	bool bShouldSort = pParticles->m_pDef->m_bShouldSort;
	CCachedParticleBatches *pCachedBatches = NULL;
	MaterialThreadMode_t nThreadMode = g_pMaterialSystem->GetThreadMode();
	if ( nThreadMode != MATERIAL_SINGLE_THREADED && bSpriteCard && !bShouldSort && !pCtx->m_VisibilityData.m_bUseVisibility )
	{
		pParticles->ResetParticleCache();
		pCachedBatches = pParticles->GetCachedParticleBatches();
	}

	// Store matrices off so we can restore them in RenderEnd().
	pRenderContext->Bind( pMaterial );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;

	// Get the camera's worldspace position
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	SpriteTrailRenderInfo_t info;
	info.Init( pParticles, 0, flAgeScale, pParticles->m_Sheet() );

	int nSkipAheadParticles = 0;
	int nParticles = 0;
	const ParticleRenderData_t *pSortList = NULL;
	
	// Only grab the render list if we're not cached, since this can be costly for large systems.  Make sure that if we run out of cached batches below
	// that we re-grab the render list and continue with the slow path
	if ( !pCachedBatches || !pCachedBatches->GetCachedBatch( 0 ) )
	{
		pSortList = pParticles->GetRenderList( pRenderContext, true, &nParticles, &pCtx->m_VisibilityData );
		if ( pCachedBatches )
		{
			pCachedBatches->SetCachedRenderListCount( nParticles );
		}
	}
	else
	{
		nParticles = pCachedBatches->GetCachedRenderListCount();
	}

	int nMaxParticlesInBatch = GetMaxParticlesPerBatch( pRenderContext, pMaterial, false );
	float flOODt = ( m_bIgnoreDT ? 1.0 : ( pParticles->m_flDt != 0.0f ) ? ( 1.0f / pParticles->m_flDt ) : 1.0f );
	int nBatchCount = 0;
	bool bFirstBatchBatched = false;
	while ( nParticles )
	{
		int nParticlesInBatch = MIN( nMaxParticlesInBatch, nParticles );
		nParticles -= nParticlesInBatch;

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

		if ( bSpriteCard )
		{
			ICachedPerFrameMeshData *pCachedBatch = pCachedBatches ? pCachedBatches->GetCachedBatch( nBatchCount ) : NULL;
			if ( pCachedBatch )
			{
				// This copies all of the VB/IB pointers and data out of the pCachedBatch back into the pMesh
				pMesh->ReconstructFromCachedPerFrameMeshData( pCachedBatch );
				if ( nBatchCount == 0 )
					bFirstBatchBatched = true; 

				nSkipAheadParticles += pMesh->IndexCount() / 6;
			}
			else
			{
				// This fires if the first batch was cached, but some subsequent batch is not.  We can either increase MAX_CACHED_PARTICLE_BATCHES in particles.h
				// or get the render list and continue unbatched
				if ( bFirstBatchBatched )
				{
					// Get the render list and resume from where we stopped batching
					int nNewParticles = 0;
					pSortList = pParticles->GetRenderList( pRenderContext, true, &nNewParticles, &pCtx->m_VisibilityData );
					pSortList -= nSkipAheadParticles;

					// We have a different number of particles from when we cached in the beginning of the frame!
					Assert( nNewParticles == nParticles );

					bFirstBatchBatched = false;
				}

				CMeshBuilder meshBuilder;
				meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nParticlesInBatch * 4, nParticlesInBatch * 6 );
				if ( meshBuilder.m_ActualVertexSize == 0 )
				{
					// We're likely in alt+tab, and since we're using fast vertex/index routines, we need to see if we're writing into valid vertex data
					meshBuilder.End();
					return;
				}

				// Grab index and vertex pointers.  The VB will be NULL if the vertex size is not sizeof( FastSpriteTrailVertex_t )
				uint32 *pIndices = (uint32*)( meshBuilder.BaseIndexData() + meshBuilder.GetCurrentIndex() );
				FastSpriteTrailVertex_t *pVertices = (FastSpriteTrailVertex_t*)meshBuilder.GetVertexDataPtr( sizeof( FastSpriteTrailVertex_t ) );
				int nIndexOffset = meshBuilder.GetIndexOffset();
				int nVertices = 0;
				int nIndices = 0;
				info.m_nVertexOffset = 0;

				if ( pVertices )
				{
					// Fast path uses the predetermined vertex format
					for( int i = 0; i < nParticlesInBatch; i++ )
					{
						int hParticle = (--pSortList)->m_nIndex;
						if ( RenderSpriteTrailSpriteCard<true>( meshBuilder, nVertices, pVertices, pIndices, nIndexOffset, info, hParticle, flOODt, pSortList ) )
						{
							pVertices += 4;
							pIndices += 3;

							nVertices += 4;
							nIndices += 6;
						}
					}
				}
				else
				{
					// Slow path uses meshbuilder
					for( int i = 0; i < nParticlesInBatch; i++ )
					{
						int hParticle = (--pSortList)->m_nIndex;
						if ( RenderSpriteTrailSpriteCard<false>( meshBuilder, nVertices, pVertices, pIndices, nIndexOffset, info, hParticle, flOODt, pSortList ) )
						{
							pIndices += 3;

							nVertices += 4;
							nIndices += 6;
						}
					}
				}

				meshBuilder.AdvanceVerticesF<VTX_HAVEPOS | VTX_HAVECOLOR, 6>( nVertices );
				meshBuilder.AdvanceIndices( nIndices );
				meshBuilder.End();

				// If we have a list of cached batches, cache them off so that if we try to render this sytem again for the current frame,
				// we have a cached all of the vb and ib pointers.
				if ( pCachedBatches )
				{
					pCachedBatch = pMesh->GetCachedPerFrameMeshData();
					pCachedBatches->SetCachedBatch( nBatchCount, pCachedBatch );
				}
			}

		}
		else
		{
			CMeshBuilder meshBuilder;
			meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nParticlesInBatch * 4, nParticlesInBatch * 6 );
			info.m_nVertexOffset = 0;
			int nVertices = 0;
			int nIndices = 0;

			for( int i = 0; i < nParticlesInBatch; i++ )
			{
				int hParticle = (--pSortList)->m_nIndex;
				if ( RenderSpriteTrail( meshBuilder, nVertices, nIndices, info, hParticle, vecCameraPos, flOODt, pSortList ) )
				{
					nVertices += 4;
					nIndices += 6;
				}
			}

			meshBuilder.AdvanceVerticesF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>( nVertices );
			meshBuilder.AdvanceIndices( nIndices );
			meshBuilder.End();
		}

		nBatchCount++;
		pMesh->DrawModulated( vecDiffuseModulation );
	}
}


void C_OP_RenderSpritesTrail::RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	C_OP_RenderSpriteTrailContext_t *pCtx = reinterpret_cast<C_OP_RenderSpriteTrailContext_t *>( pContext );
	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	Vector vecCameraPos;
	pRenderContext->GetWorldSpaceCameraPosition( &vecCameraPos );

	float flAgeScale = m_flAnimationRate * SEQUENCE_SAMPLE_COUNT;
	SpriteTrailRenderInfo_t info;
	info.Init( pParticles, nVertexOffset, flAgeScale, pParticles->m_Sheet() );

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	float flOODt = ( m_bIgnoreDT ? 1.0 : ( pParticles->m_flDt != 0.0f ) ? ( 1.0f / pParticles->m_flDt ) : 1.0f );
	int hParticle = nFirstParticle;
	int nVertices = 0;
	int nIndices = 0;

	for( int i = 0; i < nParticleCount; i++, hParticle++ )
	{
		if ( RenderSpriteTrail( meshBuilder, nVertices, nIndices, info, hParticle, vecCameraPos, flOODt, pSortList ) )
		{
			nVertices += 4;
			nIndices += 6;
		}
	}

	meshBuilder.AdvanceVerticesF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>( nVertices );
	meshBuilder.AdvanceIndices( nIndices );
}


//-----------------------------------------------------------------------------
//
// Rope renderer
//
//-----------------------------------------------------------------------------
struct RopeRenderInfo_t
{
	size_t m_nXYZStride;
	const fltx4 *m_pXYZ;
	size_t m_nRadStride;
	const fltx4 *m_pRadius;
	size_t m_nRGBStride;
	const fltx4 *m_pRGB;
	size_t m_nAlphaStride;
	const fltx4 *m_pAlpha;
	size_t m_nAlpha2Stride;
	const fltx4 *m_pAlpha2;
	CParticleCollection *m_pParticles;

	void Init( CParticleCollection *pParticles )
	{
		m_pParticles = pParticles;
		m_pXYZ = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &m_nXYZStride );
		m_pRadius = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_RADIUS, &m_nRadStride );
		m_pRGB = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, &m_nRGBStride );
		m_pAlpha = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ALPHA, &m_nAlphaStride );
		m_pAlpha2 = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ALPHA2, &m_nAlpha2Stride );
	}

	void GenerateSeg( int hParticle, BeamSeg_t& seg )
	{
		Assert( hParticle != -1 );
		int nGroup = hParticle / 4;
		int nOffset = hParticle & 0x3;

		int nXYZIndex = nGroup * m_nXYZStride;
		int nColorIndex = nGroup * m_nRGBStride;
		seg.m_vPos.Init( SubFloat( m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( m_pXYZ[ nXYZIndex+2 ], nOffset ) );
		seg.SetColor( SubFloat( m_pRGB[ nColorIndex ], nOffset ), SubFloat( m_pRGB[ nColorIndex+1 ], nOffset ), SubFloat( m_pRGB[nColorIndex+2], nOffset ), SubFloat( ( m_pAlpha[ nGroup * m_nAlphaStride ] * m_pAlpha2[ nGroup * m_nAlpha2Stride ] ), nOffset ) );
		seg.m_flWidth = SubFloat( m_pRadius[ nGroup * m_nRadStride ], nOffset );
	}
};


struct RenderRopeContext_t
{
	float m_flRenderedRopeLength;
};

class C_OP_RenderRope : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderRope );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK | 
			PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK |
			PARTICLE_ATTRIBUTE_ALPHA2_MASK;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		RenderRopeContext_t *pCtx = reinterpret_cast<RenderRopeContext_t *>( pContext );
		pCtx->m_flRenderedRopeLength = false;
		float *pSubdivList = (float*)( pCtx + 1 );
		for ( int iSubdiv = 0; iSubdiv < m_nSubdivCount; iSubdiv++ )
		{
			pSubdivList[iSubdiv] = (float)iSubdiv / (float)m_nSubdivCount;
		}

		// NOTE: Has to happen here, and not in InitParams, since the material isn't set up yet
		IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();
		float flTscale = 1.0;
		if ( pMaterial )
		{
			flTscale = 1.0f / ( pMaterial->GetMappingHeight() * m_flTexelSizeInUnits );
		}
		const_cast<C_OP_RenderRope*>( this )->m_flTextureScale = flTscale; // this is a little bogus but safe
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( RenderRopeContext_t ) + m_nSubdivCount * sizeof(float);
	}

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
		if ( m_nSubdivCount <= 0 )
		{
			m_nSubdivCount = 1;
		}
		if ( m_flTexelSizeInUnits <= 0 )
		{
			m_flTexelSizeInUnits = 1.0f;
		}
		m_flTStep = 1.0 / m_nSubdivCount;
		if ( ( m_bScaleByControlPointDistance || m_bScaleScrollByControlPointDistance || m_bScaleOffsetByControlPointDistance ) && ( m_nScaleCP1 > -1 && m_nScaleCP2 > -1 ) )
			m_bUsesCPScaling = true;
		else
			m_bUsesCPScaling = false;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		if ( m_bUsesCPScaling )
			return ( 1ULL << m_nScaleCP1 ) | ( 1ULL << m_nScaleCP2 );
		return 0;
	}

	virtual bool IsOrderImportant() const
	{
		return true;
	}

	virtual int GetParticlesToRender( CParticleCollection *pParticles, void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, int *pVertsUsed, int *pIndicesUsed ) const;
	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const;
	void RenderSpriteCard( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMaterial *pMaterial ) const;
	virtual void RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const;

	template< class T >
	void RenderSpriteCard_Internal( T *pVertices, CCachedParticleBatches *pCachedBatches, IMesh *pMesh, CMeshBuilder &meshBuilder, 
		int nSegmentsAvailableInBuffer, int nNumSegmentsIWillRenderPerBatch, float flMaterialMappingHeight,
		CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation ) const;

	int		m_nSubdivCount;
	int		m_nScaleCP1;
	int		m_nScaleCP2;

	float	m_flTexelSizeInUnits;
	float	m_flTextureScale;
	float	m_flTextureScrollRate;
	float	m_flTextureOffset;
	float	m_flTStep;

	bool	m_bScaleByControlPointDistance;
	bool	m_bScaleScrollByControlPointDistance;
	bool	m_bScaleOffsetByControlPointDistance;
	bool	m_bUsesCPScaling;

	CullSystemByControlPointData_t m_cullData;
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderRope, "render_rope", OPERATOR_SINGLETON );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_RenderRope ) 
	DMXELEMENT_UNPACK_FIELD( "subdivision_count", "3", int, m_nSubdivCount )
	DMXELEMENT_UNPACK_FIELD( "texel_size", "4.0f", float, m_flTexelSizeInUnits )
	DMXELEMENT_UNPACK_FIELD( "texture_scroll_rate", "0.0f", float, m_flTextureScrollRate )
	DMXELEMENT_UNPACK_FIELD( "texture_offset", "0.0f", float, m_flTextureOffset )
	DMXELEMENT_UNPACK_FIELD( "scale CP start", "-1", int, m_nScaleCP1 )
	DMXELEMENT_UNPACK_FIELD( "scale CP end", "-1", int, m_nScaleCP2 )
	DMXELEMENT_UNPACK_FIELD( "scale texture by CP distance", "0", bool, m_bScaleByControlPointDistance )
	DMXELEMENT_UNPACK_FIELD( "scale scroll by CP distance", "0", bool, m_bScaleScrollByControlPointDistance )
	DMXELEMENT_UNPACK_FIELD( "scale offset by CP distance", "0", bool, m_bScaleOffsetByControlPointDistance )
	DMXELEMENT_UNPACK_FIELD( CULL_CP_NORMAL_DESCRIPTOR, "-1", int, m_cullData.m_nCullControlPoint )
	DMXELEMENT_UNPACK_FIELD( CULL_RECURSION_DEPTH_DESCRIPTOR, "-1", int, m_cullData.m_nViewRecursionDepthStart )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderRope )

//-----------------------------------------------------------------------------
// Returns the number of particles to render
//-----------------------------------------------------------------------------
int C_OP_RenderRope::GetParticlesToRender( CParticleCollection *pParticles, 
	void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, 
										   int *pVertsUsed, int *pIndicesUsed ) const
{
	if ( ( nFirstParticle >= pParticles->m_nActiveParticles - 1 ) || ( pParticles->m_nActiveParticles <= 1 ) )
	{
		*pVertsUsed = 0;
		*pIndicesUsed = 0;
		return 0;
	}

	// NOTE: This is only true for particles *after* the first particle.
	// First particle takes 2 verts, no indices.
	int nVertsPerParticle = 2 * m_nSubdivCount;
	int nIndicesPerParticle = 6 * m_nSubdivCount;

	// Subtract 2 is because the first particle uses an extra pair of vertices
	int nMaxParticleCount = 1 + ( nRemainingVertices - 2 ) / nVertsPerParticle;
	int nMaxParticleCount2 = nRemainingIndices / nIndicesPerParticle;
	if ( nMaxParticleCount > nMaxParticleCount2 )
	{
		nMaxParticleCount = nMaxParticleCount2;
	}

	int nParticleCount = pParticles->m_nActiveParticles - nFirstParticle;

	// We can't choose a max particle count so that we only have 1 particle to render next time
	if ( nMaxParticleCount == nParticleCount - 1 )
	{
		--nMaxParticleCount;
		Assert( nMaxParticleCount > 0 );
	}

	if ( nParticleCount > nMaxParticleCount )
	{
		nParticleCount = nMaxParticleCount;
	}

	*pVertsUsed = ( nParticleCount - 1 ) * m_nSubdivCount * 2 + 2;
	*pIndicesUsed = nParticleCount * m_nSubdivCount * 6;
	return nParticleCount;
}
			
struct FastRopeVertex_t
{
	Vector m_vPosition;
	int m_vColor;
	Vector4D m_vP0;
	Vector4D m_vP1;
	Vector4D m_vP2;
	Vector4D m_vP3;
	Vector4D m_vCorners;
	Vector4D m_vEndPointColor;
	
	FORCEINLINE void SetNormals( Vector &vecNorm0, Vector &vecNorm1 )
	{
		// Intentionally do nothing, no normals on a FastRopeVertex_t
	}
};

struct FastRopeVertexNormal_t
{
	Vector m_vPosition;
	int m_vColor;
	Vector4D m_vP0;
	Vector4D m_vP1;
	Vector4D m_vP2;
	Vector4D m_vP3;
	Vector4D m_vCorners;
	Vector4D m_vEndPointColor;
	Vector m_vNormal0;
	Vector m_vNormal1;
	
	FORCEINLINE void SetNormals( Vector &vecNorm0, Vector &vecNorm1 )
	{
		m_vNormal0 = vecNorm0;
		m_vNormal1 = vecNorm1;
	}
};

struct FastRopeVertexNormalCacheAligned_t : public FastRopeVertexNormal_t
{
	int m_nPadding[ 2 ];
	
	// On the PC, vertex structures need to be sized in multiples of 16 bytes
	FORCEINLINE void Check() { COMPILE_TIME_ASSERT( !IsPC() || ( sizeof( *this ) % 16 ) == 0 ); }
};

template < class T >
FORCEINLINE void Output2SplineVerts( T *&pVertices, int &nVertices, int nPackedColor, float flT, float flU, Vector4D &vecP0, Vector4D &vecP1, Vector4D &vecP2, Vector4D &vecP3, Vector4D &vecEndPointColor, Vector &vecNorm0, Vector &vecNorm1 )
{
	pVertices->m_vPosition.Init( flT, flU, 0 );
	pVertices->m_vColor = nPackedColor;	
	pVertices->m_vP0 = vecP0;
	pVertices->m_vP1 = vecP1;
	pVertices->m_vP2 = vecP2;	
	pVertices->m_vP3 = vecP3;	
	pVertices->m_vCorners.Init( 0, 0, 1, 1 );			
	pVertices->m_vEndPointColor = vecEndPointColor;														
	pVertices->SetNormals( vecNorm0, vecNorm1 );
	pVertices++;														
	pVertices->m_vPosition.Init( flT, flU, 1 );							
	pVertices->m_vColor = nPackedColor;								
	pVertices->m_vP0 = vecP0;											
	pVertices->m_vP1 = vecP1;											
	pVertices->m_vP2 = vecP2;											
	pVertices->m_vP3 = vecP3;											
	pVertices->m_vCorners.Init( 0, 0, 1, 1 );							
	pVertices->m_vEndPointColor = vecEndPointColor;	
	pVertices->SetNormals( vecNorm0, vecNorm1 );													
	pVertices++;														
	nVertices += 2;
}

#define OUTPUT_SPLINE_INDICES( nCurIDX )										\
			{																	\
			unsigned short _nIndex = nCurIDX + nIndexOffset;					\
			*pIndices = TwoIndices( _nIndex, _nIndex + 1 ); pIndices++;			\
			*pIndices = TwoIndices( _nIndex + 2, _nIndex + 1 ); pIndices++;		\
			*pIndices = TwoIndices( _nIndex + 3, _nIndex + 2 ); pIndices++;		\
			nIndices += 6;														\
			}								

template< class T >
void C_OP_RenderRope::RenderSpriteCard_Internal( T *pVertices, CCachedParticleBatches *pCachedBatches, IMesh *pMesh, CMeshBuilder &meshBuilder, 
								  int nSegmentsAvailableInBuffer, int nNumSegmentsIWillRenderPerBatch, float flMaterialMappingHeight,
								  CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation ) const
{
	uint32 *pIndices = (uint32*)( meshBuilder.BaseIndexData() + meshBuilder.GetCurrentIndex() );
	int nIndexOffset = meshBuilder.GetIndexOffset();

	int nParticles = pParticles->m_nActiveParticles; 
	int nSegmentsToRender = nParticles - 1;

	const float *pXYZ = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, 0 );
	const float *pColor = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, 0 );
	const float *pRadius = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, 0 );
	const float *pAlpha = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_ALPHA, 0 );
	const float *pAlpha2 = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_ALPHA2, 0 );
	const float *pNorm = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_NORMAL, 0 );

	bool bFirstPoint = true;
	float flTextureScrollRate = m_flTextureScrollRate;
	float flU = m_flTextureOffset;
	float flDist = 0;
	
	float flOffsetScaled = m_flTextureOffset / flMaterialMappingHeight;
	float flTextureScale = m_flTexelSizeInUnits;

	if ( m_bUsesCPScaling )
	{
		flDist = pParticles->GetControlPointAtCurrentTime( m_nScaleCP1 ).DistTo( pParticles->GetControlPointAtCurrentTime( m_nScaleCP2 ) );

		if ( m_bScaleByControlPointDistance )
		{
			flTextureScale = 1.0f / ( ( flDist * m_flTexelSizeInUnits ) + FLT_EPSILON );
		}
		if ( m_bScaleScrollByControlPointDistance )
		{
			flTextureScrollRate *= ( flDist / flMaterialMappingHeight ) * flTextureScale;
		}
		if ( m_bScaleOffsetByControlPointDistance )
		{
			flOffsetScaled += flOffsetScaled * ( flDist / flMaterialMappingHeight );
		}
	}
	flTextureScrollRate *= pParticles->m_flCurTime;
	flOffsetScaled += flTextureScrollRate;

	flU += flOffsetScaled;

	// initialize first spline segment
	Vector4D vecP1( pXYZ[0], pXYZ[4], pXYZ[8], pRadius[0] );
	Vector4D vecP2( pXYZ[1], pXYZ[5], pXYZ[9], pRadius[1] );
	Vector4D vecP0 = vecP1;
	Vector vecNorm0( pNorm[0], pNorm[4], pNorm[8] );
	Vector vecNorm1( pNorm[1], pNorm[5], pNorm[9] );

	uint8 nRed = FastFToC( pColor[0] );
	uint8 nGreen = FastFToC( pColor[4] );
	uint8 nBlue = FastFToC( pColor[8] );
	uint8 nAlpha = FastFToC( pAlpha[0] * pAlpha2[0] );

	Vector4D vecDelta = vecP2;
	vecDelta -= vecP1;
	vecP0 -= vecDelta;

	Vector4D vecP3;
	Vector4D vecEndPointColor( pColor[1], pColor[5], pColor[9], pAlpha[1] * pAlpha2[1] );

	if ( nParticles < 3 )
	{
		vecP3 = vecP2;
		vecP3 += vecDelta;
	}
	else
	{
		vecP3.Init( pXYZ[2], pXYZ[6], pXYZ[10], pRadius[2] );
	}
	int nPnt = 3;
	int nCurIDX = 0;

	int nVertices = 0;
	int nIndices = 0;
	float flDUScale = ( m_flTStep * flTextureScale );

	float flT = 0;
	int nBatchCount = 0;

	do
	{
		if ( !nSegmentsAvailableInBuffer )
		{
			meshBuilder.AdvanceVerticesF<VTX_HAVEPOS | VTX_HAVECOLOR, 8>( nVertices );
			meshBuilder.AdvanceIndices( nIndices );
			meshBuilder.End();

			// Store this off for the next frame
			if ( pCachedBatches )
			{
				pCachedBatches->SetCachedBatch( nBatchCount, pMesh->GetCachedPerFrameMeshData() );
				nBatchCount++;
			}

			int nNumIndicesPerSegment = 6 * m_nSubdivCount;
			int nNumVerticesPerSegment = 2 * m_nSubdivCount;

			pMesh->DrawModulated( vecDiffuseModulation );
			meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 2 + nNumSegmentsIWillRenderPerBatch * nNumVerticesPerSegment,
				nNumIndicesPerSegment * nNumSegmentsIWillRenderPerBatch );

			pIndices = (uint32*)( meshBuilder.BaseIndexData() + meshBuilder.GetCurrentIndex() );
			nIndexOffset = meshBuilder.GetIndexOffset();
			pVertices = (T*)meshBuilder.GetVertexDataPtr( sizeof( T ) );
			nVertices = 0;
			nIndices = 0;

			// copy the last emitted points
			int nPackedColor = PackRGBToPlatformColor( nRed, nGreen, nBlue, nAlpha ); 
			Output2SplineVerts( pVertices, nVertices, nPackedColor, flT, flU, vecP0, vecP1, vecP2, vecP3, vecEndPointColor, vecNorm0, vecNorm1 );

			nSegmentsAvailableInBuffer = nNumSegmentsIWillRenderPerBatch;
			nCurIDX = 0;
		}
		nSegmentsAvailableInBuffer--;
		flT = 0.;
		float flDu = flDUScale * ( vecP2.AsVector3D() - vecP1.AsVector3D() ).Length();

		// Vertices first
		int nPackedColor = PackRGBToPlatformColor( nRed, nGreen, nBlue, nAlpha ); 
		for( int nSlice = 0 ; nSlice < m_nSubdivCount; nSlice++ )
		{
			Output2SplineVerts( pVertices, nVertices, nPackedColor, flT, flU, vecP0, vecP1, vecP2, vecP3, vecEndPointColor, vecNorm0, vecNorm1 );
			flT += m_flTStep;
			flU += flDu;
		}

		// Indices second, but output m_nSubdivCount-1 indices if it's our first time through
		for( int nSlice = bFirstPoint ? 1 : 0 ; nSlice < m_nSubdivCount; nSlice++ )
		{
			OUTPUT_SPLINE_INDICES( nCurIDX );
			nCurIDX += 2;
		}
		bFirstPoint = false;

		// next segment
		if ( nSegmentsToRender > 1 )
		{
			vecP0 = vecP1;
			vecP1 = vecP2;
			vecP2 = vecP3;
			nRed = FastFToC( vecEndPointColor.x );
			nGreen = FastFToC( vecEndPointColor.y );
			nBlue = FastFToC( vecEndPointColor.z );
			nAlpha = FastFToC( vecEndPointColor.w );
			vecNorm0 = vecNorm1;

			const float *pRadius = pParticles->GetFloatAttributePtr( 
				PARTICLE_ATTRIBUTE_RADIUS, nPnt );
			const float *pAlpha = pParticles->GetFloatAttributePtr( 
				PARTICLE_ATTRIBUTE_ALPHA, nPnt -1 );
			const float *pAlpha2 = pParticles->GetFloatAttributePtr( 
				PARTICLE_ATTRIBUTE_ALPHA2, nPnt - 1 );
			const float *pColor = pParticles->GetFloatAttributePtr( 
				PARTICLE_ATTRIBUTE_TINT_RGB, nPnt - 1 );
			vecEndPointColor.Init( pColor[0], pColor[4], pColor[8], pAlpha[0] * pAlpha2[0] );

			if ( nPnt < nParticles )
			{
				pXYZ = pParticles->GetFloatAttributePtr( 
					PARTICLE_ATTRIBUTE_XYZ, nPnt );
				vecP3.Init( pXYZ[0], pXYZ[4], pXYZ[8], pRadius[0] );
				pNorm = pParticles->GetFloatAttributePtr( 
					PARTICLE_ATTRIBUTE_NORMAL, nPnt );
				vecNorm1.Init( pNorm[0], pNorm[4], pNorm[8] );
				nPnt++;
			}
			else
			{
				// fake last point by extrapolating
				vecP3 += vecP2;
				vecP3 -= vecP1;
			}
		}
	} while( --nSegmentsToRender );

	// output last piece
	int nPackedColor = PackRGBToPlatformColor( nRed, nGreen, nBlue, nAlpha ); 
	Output2SplineVerts( pVertices, nVertices, nPackedColor, 1.0, flU, vecP0, vecP1, vecP2, vecP3, vecEndPointColor, vecNorm0, vecNorm1 );
	OUTPUT_SPLINE_INDICES( nCurIDX );

	meshBuilder.AdvanceVerticesF<VTX_HAVEPOS | VTX_HAVECOLOR, 8>( nVertices );
	meshBuilder.AdvanceIndices( nIndices );
	meshBuilder.End();

	// Store this off for the next frame
	if ( pCachedBatches )
	{
		pCachedBatches->SetCachedBatch( nBatchCount, pMesh->GetCachedPerFrameMeshData() );
	}

	pMesh->DrawModulated( vecDiffuseModulation );
}

void C_OP_RenderRope::RenderSpriteCard( CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, IMaterial *pMaterial ) const
{
	int nParticles = pParticles->m_nActiveParticles; 

	int nSegmentsToRender = nParticles - 1;
	if ( ! nSegmentsToRender )
		return;

	// Reset the particle cache if we're sprite card material (doesn't use camerapos) and isn't sorted
	bool bShouldSort = pParticles->m_pDef->m_bShouldSort;
	CCachedParticleBatches *pCachedBatches = NULL;
	MaterialThreadMode_t nThreadMode = g_pMaterialSystem->GetThreadMode();
	if ( nThreadMode != MATERIAL_SINGLE_THREADED && !bShouldSort )
	{
		pParticles->ResetParticleCache();
		pCachedBatches = pParticles->GetCachedParticleBatches();
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->Bind( pMaterial );

	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();

	int nNumIndicesPerSegment = 6 * m_nSubdivCount;
	int nNumVerticesPerSegment = 2 * m_nSubdivCount;

	int nNumSegmentsPerBatch = MIN( ( nMaxVertices - 2 )/nNumVerticesPerSegment,
									( nMaxIndices ) / nNumIndicesPerSegment );
	
	int nNumSegmentsIWillRenderPerBatch = MIN( nNumSegmentsPerBatch, nSegmentsToRender );
	int nSegmentsAvailableInBuffer = nNumSegmentsIWillRenderPerBatch;

	// Early out in the case of having cached batches
	int nBatchCount = 0;
	ICachedPerFrameMeshData *pCachedBatch = pCachedBatches ? pCachedBatches->GetCachedBatch( nBatchCount ) : NULL;
	if ( pCachedBatch )
	{
		do
		{
			if ( !nSegmentsAvailableInBuffer )
			{
				IMesh *pMesh = pRenderContext->GetDynamicMesh( true );
				pMesh->ReconstructFromCachedPerFrameMeshData( pCachedBatch );
				pMesh->DrawModulated( vecDiffuseModulation );
				nSegmentsAvailableInBuffer = nNumSegmentsIWillRenderPerBatch;

				// Next cached batch
				pCachedBatch = pCachedBatches->GetCachedBatch( ++nBatchCount );
			}
			int nSegs = MIN(nSegmentsToRender, nSegmentsAvailableInBuffer);
			nSegmentsToRender -= nSegs;
			nSegmentsAvailableInBuffer -= nSegs;
		} while( nSegmentsToRender );

		// Render the last batch
		IMesh *pMesh = pRenderContext->GetDynamicMesh( true );
		pMesh->ReconstructFromCachedPerFrameMeshData( pCachedBatch );
		pMesh->DrawModulated( vecDiffuseModulation );

		return;
	}

	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	CMeshBuilder meshBuilder;

	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES,
					   2 + nNumSegmentsIWillRenderPerBatch * nNumVerticesPerSegment,
					   nNumIndicesPerSegment * nNumSegmentsIWillRenderPerBatch );
	if ( meshBuilder.m_ActualVertexSize == 0 )
	{
		// We're likely in alt+tab, and since we're using fast vertex/index routines, we need to see if we're writing into valid vertex data
		meshBuilder.End();
		return;
	}

	FastRopeVertex_t *pVertices = (FastRopeVertex_t*)meshBuilder.GetVertexDataPtr( sizeof( FastRopeVertex_t ) );
	if ( pVertices )
	{
		// No normal components in ropes
		RenderSpriteCard_Internal( pVertices, pCachedBatches, pMesh, meshBuilder, nSegmentsAvailableInBuffer, nNumSegmentsIWillRenderPerBatch, pMaterial->GetMappingHeight(), pParticles, vecDiffuseModulation );
	}
	else
	{
		// Two normal components in ropes
		FastRopeVertexNormal_t *pVerticesNormal = (FastRopeVertexNormal_t*)meshBuilder.GetVertexDataPtr( sizeof( FastRopeVertexNormal_t ) );
		if ( pVerticesNormal )
		{
			RenderSpriteCard_Internal( pVerticesNormal, pCachedBatches, pMesh, meshBuilder, nSegmentsAvailableInBuffer, nNumSegmentsIWillRenderPerBatch, pMaterial->GetMappingHeight(), pParticles, vecDiffuseModulation );
		}
		else
		{
			// Cached aligned
			FastRopeVertexNormalCacheAligned_t *pVerticesNormalCacheAligned = (FastRopeVertexNormalCacheAligned_t*)meshBuilder.GetVertexDataPtr( sizeof( FastRopeVertexNormalCacheAligned_t ) );
			if ( pVerticesNormalCacheAligned )
			{
				RenderSpriteCard_Internal( pVerticesNormalCacheAligned, pCachedBatches, pMesh, meshBuilder, nSegmentsAvailableInBuffer, nNumSegmentsIWillRenderPerBatch, pMaterial->GetMappingHeight(), pParticles, vecDiffuseModulation );
			}
			else
			{
				Assert( 0 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Renders particles, sorts them (?)
//-----------------------------------------------------------------------------
void C_OP_RenderRope::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
{
	// See if we need to cull this system
	if ( ShouldCullParticleSystem( &m_cullData, pParticles, pRenderContext, nViewRecursionDepth ) )
		return;

	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();
	if ( pMaterial->IsSpriteCard() )
	{
		RenderSpriteCard( pParticles, vecDiffuseModulation, pContext, pMaterial );
		return;
	}

	pRenderContext->Bind( pMaterial );

	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();
	int nParticles = pParticles->m_nActiveParticles; 

	int nFirstParticle = 0;
	while ( nParticles )
	{
		int nVertCount, nIndexCount;
		int nParticlesInBatch = GetParticlesToRender( pParticles, pContext, nFirstParticle, nMaxVertices, nMaxIndices, &nVertCount, &nIndexCount );
		if ( nParticlesInBatch == 0 )
			break;

		nParticles -= nParticlesInBatch;

		IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, nVertCount, nIndexCount );

		RenderUnsorted( pParticles, pContext, pRenderContext, meshBuilder, 0, nFirstParticle, nParticlesInBatch );

		meshBuilder.End();
		pMesh->DrawModulated( vecDiffuseModulation );

		nFirstParticle += nParticlesInBatch;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void C_OP_RenderRope::RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
{
	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	// Right now we only have a meshbuilder version!
	Assert( pMaterial->IsSpriteCard() == false );
	if ( pMaterial->IsSpriteCard() )
		return;

	RenderRopeContext_t *pCtx = reinterpret_cast<RenderRopeContext_t *>( pContext );
	float *pSubdivList = (float*)( pCtx + 1 );
	if ( nFirstParticle == 0 )
	{
		pCtx->m_flRenderedRopeLength = 0.0f;
	}

	float flTexOffset = m_flTextureScrollRate;
	float flTextureScale = m_flTextureScale;

	RopeRenderInfo_t info;
	info.Init( pParticles );

	flTexOffset *= pParticles->m_flCurTime;

	float flDist = 0;
	float flOffsetScaled = m_flTextureOffset;
	if ( m_bUsesCPScaling )
	{
		flDist = pParticles->GetControlPointAtCurrentTime( m_nScaleCP1 ).DistTo( pParticles->GetControlPointAtCurrentTime( m_nScaleCP2 ) );

		if ( m_bScaleByControlPointDistance )		// scale by distance to first control point?
		{
			flTextureScale = 1.0f / ( ( flDist * m_flTexelSizeInUnits ) + FLT_EPSILON );
		}
		if ( m_bScaleScrollByControlPointDistance )
		{
			flOffsetScaled *= ( flDist / pMaterial->GetMappingHeight() );
		}
		if ( m_bScaleOffsetByControlPointDistance )
		{
			flOffsetScaled += m_flTextureOffset * ( flDist / pMaterial->GetMappingHeight() );
		}
	}

	flTexOffset += flOffsetScaled;
	
	
	CBeamSegDraw beamSegment;
	beamSegment.Start( pRenderContext, ( nParticleCount - 1 ) * m_nSubdivCount + 1, pMaterial, &meshBuilder, nVertexOffset );

	Vector vecCatmullRom[4];
	BeamSeg_t seg[2];
	info.GenerateSeg( nFirstParticle, seg[0] );
	seg[0].m_flTexCoord = ( pCtx->m_flRenderedRopeLength + flTexOffset ) * flTextureScale;

	beamSegment.NextSeg( &seg[0] );
	vecCatmullRom[1] = seg[0].m_vPos;
	if ( nFirstParticle == 0 )
	{
		vecCatmullRom[0] = vecCatmullRom[1];
	}
	else
	{
		int nGroup = ( nFirstParticle-1 ) / 4;
		int nOffset = ( nFirstParticle-1 ) & 0x3;
		int nXYZIndex = nGroup * info.m_nXYZStride;
		vecCatmullRom[0].Init( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );
	}

	float flOOSubDivCount = 1.0f / m_nSubdivCount;
	int hParticle = nFirstParticle + 1;
	for ( int i = 1; i < nParticleCount; ++i, ++hParticle )
	{
		int nCurr = i & 1;
		int nPrev = 1 - nCurr;
		info.GenerateSeg( hParticle, seg[nCurr] );
		pCtx->m_flRenderedRopeLength += seg[nCurr].m_vPos.DistTo( seg[nPrev].m_vPos );
		seg[nCurr].m_flTexCoord = ( pCtx->m_flRenderedRopeLength + flTexOffset ) * flTextureScale;

		if ( m_nSubdivCount > 1 )
		{
			vecCatmullRom[ (i+1) & 0x3 ] = seg[nCurr].m_vPos;
			if ( hParticle != info.m_pParticles->m_nActiveParticles - 1 )
			{
				int nGroup = ( hParticle+1 ) / 4;
				int nOffset = ( hParticle+1 ) & 0x3;
				int nXYZIndex = nGroup * info.m_nXYZStride;
				vecCatmullRom[ (i+2) & 0x3 ].Init( SubFloat( info.m_pXYZ[ nXYZIndex ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+1 ], nOffset ), SubFloat( info.m_pXYZ[ nXYZIndex+2 ], nOffset ) );
			}
			else
			{
				vecCatmullRom[ (i+2) & 0x3 ] = vecCatmullRom[ (i+1) & 0x3 ];
			}

			BeamSeg_t &subDivSeg = seg[nPrev];
			Vector4D vecColor, vecNextColor;
			seg[nPrev].GetColor( &vecColor );
			seg[nCurr].GetColor( &vecNextColor ); 
			Vector4D vecColorInc;
			Vector4DSubtract( vecNextColor, vecColor, vecColorInc );
			vecColorInc *= flOOSubDivCount;
			float flTexcoordInc = ( seg[nCurr].m_flTexCoord - seg[nPrev].m_flTexCoord ) * flOOSubDivCount;
			float flWidthInc = ( seg[nCurr].m_flWidth - seg[nPrev].m_flWidth ) * flOOSubDivCount;
			for( int iSubdiv = 1; iSubdiv < m_nSubdivCount; ++iSubdiv )
			{
				vecColor += vecColorInc;
				subDivSeg.SetColor( vecColor.x, vecColor.y, vecColor.z, vecColor.w );
				subDivSeg.m_flTexCoord += flTexcoordInc;
				subDivSeg.m_flWidth += flWidthInc;

				Catmull_Rom_Spline( vecCatmullRom[ (i+3) & 0x3 ], vecCatmullRom[ i & 0x3 ],
					vecCatmullRom[ (i+1) & 0x3 ], vecCatmullRom[ (i+2) & 0x3 ],
					pSubdivList[iSubdiv], subDivSeg.m_vPos );

				beamSegment.NextSeg( &subDivSeg );
			}
		}

		beamSegment.NextSeg( &seg[nCurr] );
	}

	beamSegment.End();
}

#ifdef USE_BLOBULATOR										// Enable blobulator for EP3

//-----------------------------------------------------------------------------
// Installs renderers
//-----------------------------------------------------------------------------
class C_OP_RenderBlobs : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderBlobs );

	float m_cubeWidth;
	float m_cutoffRadius;
	float m_renderRadius;


	struct C_OP_RenderBlobsContext_t
	{
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
	};

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = 0;
		if ( VisibilityInputs.m_nCPin >= 0 )
			nMask |= 1ULL << VisibilityInputs.m_nCPin; 
		return nMask;
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderBlobsContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RenderBlobsContext_t *pCtx = reinterpret_cast<C_OP_RenderBlobsContext_t *>( pContext );
		if ( ( VisibilityInputs.m_nCPin >= 0 ) || ( VisibilityInputs.m_flRadiusScaleFOVBase > 0 ) )
			pCtx->m_VisibilityData.m_bUseVisibility = true;
		else
			pCtx->m_VisibilityData.m_bUseVisibility = false;
	}

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const;
	
	virtual bool IsBatchable() const
	{
		return false;
	}
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderBlobs, "render_blobs", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderBlobs ) 
	DMXELEMENT_UNPACK_FIELD( "cube_width", "1.0f", float, m_cubeWidth )
	DMXELEMENT_UNPACK_FIELD( "cutoff_radius", "3.3f", float, m_cutoffRadius )
	DMXELEMENT_UNPACK_FIELD( "render_radius", "1.3f", float, m_renderRadius )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderBlobs )


void C_OP_RenderBlobs::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
{
	C_OP_RenderBlobsContext_t *pCtx = reinterpret_cast<C_OP_RenderBlobsContext_t *>( pContext );

	if ( pCtx->m_VisibilityData.m_bUseVisibility )
	{
		SetupParticleVisibility( pParticles, &pCtx->m_VisibilityData, &VisibilityInputs, &pCtx->m_nQueryHandle, pRenderContext );
	}



	#if 0
		// Note: it is not good to have these static variables here.
		static RENDERER_CLASS* sweepRenderer = NULL;
		static ImpTiler* tiler = NULL;
		if(!sweepRenderer)
		{
		sweepRenderer = new RENDERER_CLASS();
		tiler = new ImpTiler(sweepRenderer);
		}
	#endif

	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	// TODO: I don't need to load this as a sorted list. See Lennard Jones forces for better way!
	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );
	size_t xyz_stride;
	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	Vector bbMin;
	Vector bbMax;
	pParticles->GetBounds( &bbMin, &bbMax );
	Vector bbCenter = 0.5f * ( bbMin + bbMax );

	// FIXME: Make this configurable. Not all shaders perform lighting. Although it's pretty likely for isosurface shaders.
	g_pParticleSystemMgr->Query()->SetUpLightingEnvironment( bbCenter );

	ImpParticleList particleList;
	particleList.EnsureCount( nParticles );
	for( int i = 0; i < nParticles; i++ )
	{
		int hParticle = (--pSortList)->m_nIndex;
		int nIndex = ( hParticle / 4 ) * xyz_stride;
		int nOffset = hParticle & 0x3;
		float x = SubFloat( xyz[nIndex], nOffset );
		float y = SubFloat( xyz[nIndex+1], nOffset );
		float z = SubFloat( xyz[nIndex+2], nOffset );

		ImpParticle* imp_particle = &particleList[i];
		imp_particle->center[0]=x;
		imp_particle->center[1]=y;
		imp_particle->center[2]=z;
		imp_particle->setFieldScale(1.0f);
	}

	Blobulator::BlobRenderInfo_t blobRenderInfo;
	blobRenderInfo.m_flCubeWidth = m_cubeWidth;
	blobRenderInfo.m_flCutoffRadius = m_cutoffRadius;
	blobRenderInfo.m_flRenderRadius = m_renderRadius;
	blobRenderInfo.m_flViewScale = ( nViewRecursionDepth == 0 ) ? 1.f : 1.6f;
	blobRenderInfo.m_nViewID = nViewRecursionDepth;

	Blobulator::RenderBlob( true, pRenderContext, pMaterial, blobRenderInfo, NULL, 0, particleList.Base(), nParticles );
}

#endif //blobs



//-----------------------------------------------------------------------------
// Installs renderers
//-----------------------------------------------------------------------------
class C_OP_RenderScreenVelocityRotate : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderScreenVelocityRotate );

	float m_flRotateRateDegrees;
	float m_flForwardDegrees;

	struct C_OP_RenderScreenVelocityRotateContext_t
	{
		CParticleVisibilityData m_VisibilityData;
		int		m_nQueryHandle;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RenderScreenVelocityRotateContext_t );
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = 0;
		if ( VisibilityInputs.m_nCPin >= 0 )
			nMask |= 1ULL << VisibilityInputs.m_nCPin; 
		return nMask;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RenderScreenVelocityRotateContext_t *pCtx = reinterpret_cast<C_OP_RenderScreenVelocityRotateContext_t *>( pContext );
		if ( ( VisibilityInputs.m_nCPin >= 0 ) || ( VisibilityInputs.m_flRadiusScaleFOVBase > 0 ) )
			pCtx->m_VisibilityData.m_bUseVisibility = true;
		else
			pCtx->m_VisibilityData.m_bUseVisibility = false;
	}
	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_ROTATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_ROTATION_MASK ;
	}

	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const;
};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderScreenVelocityRotate, "render_screen_velocity_rotate", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderScreenVelocityRotate ) 
	DMXELEMENT_UNPACK_FIELD( "rotate_rate(dps)", "0.0f", float, m_flRotateRateDegrees )
	DMXELEMENT_UNPACK_FIELD( "forward_angle", "-90.0f", float, m_flForwardDegrees )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderScreenVelocityRotate )


void C_OP_RenderScreenVelocityRotate::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
{
	C_OP_RenderScreenVelocityRotateContext_t *pCtx = reinterpret_cast<C_OP_RenderScreenVelocityRotateContext_t *>( pContext );
	if ( pCtx->m_VisibilityData.m_bUseVisibility )
	{
		SetupParticleVisibility( pParticles, &pCtx->m_VisibilityData, &VisibilityInputs, &pCtx->m_nQueryHandle, pRenderContext );
	}

	// NOTE: This is interesting to support because at first we won't have all the various
	// pixel-shader versions of SpriteCard, like modulate, twotexture, etc. etc.
	VMatrix tempView;

	// Store matrices off so we can restore them in RenderEnd().
	pRenderContext->GetMatrix(MATERIAL_VIEW, &tempView);

	int nParticles;
	const ParticleRenderData_t *pSortList = pParticles->GetRenderList( pRenderContext, false, &nParticles, &pCtx->m_VisibilityData );

	size_t xyz_stride;
	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	size_t prev_xyz_stride;
	const fltx4 *prev_xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, &prev_xyz_stride );

	size_t rot_stride;
	// const fltx4 *pRot = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ROTATION, &rot_stride );
	fltx4 *pRot = pParticles->GetM128AttributePtrForWrite( PARTICLE_ATTRIBUTE_ROTATION, &rot_stride );

	float flForwardRadians = m_flForwardDegrees * ( M_PI / 180.0f );
	//float flRotateRateRadians = m_flRotateRateDegrees * ( M_PI / 180.0f );

	for( int i = 0; i < nParticles; i++ )
	{
		int hParticle = (--pSortList)->m_nIndex;
		int nGroup = ( hParticle / 4 );
		int nOffset = hParticle & 0x3;

		int nXYZIndex = nGroup * xyz_stride;
		Vector vecWorldPos( SubFloat( xyz[ nXYZIndex ], nOffset ), SubFloat( xyz[ nXYZIndex+1 ], nOffset ), SubFloat( xyz[ nXYZIndex+2 ], nOffset ) );
		Vector vecViewPos;
		Vector3DMultiplyPosition( tempView, vecWorldPos, vecViewPos );

		if (!IsFinite(vecViewPos.x))
			continue;

		int nPrevXYZIndex = nGroup * prev_xyz_stride;
		Vector vecPrevWorldPos( SubFloat( prev_xyz[ nPrevXYZIndex ], nOffset ), SubFloat( prev_xyz[ nPrevXYZIndex+1 ], nOffset ), SubFloat( prev_xyz[ nPrevXYZIndex+2 ], nOffset ) );
		Vector vecPrevViewPos;
		Vector3DMultiplyPosition( tempView, vecPrevWorldPos, vecPrevViewPos );

		float rot = atan2( vecViewPos.y - vecPrevViewPos.y, vecViewPos.x - vecPrevViewPos.x ) + flForwardRadians;
		SubFloat( pRot[ nGroup * rot_stride ], nOffset ) = rot;
	}
}





#define MAX_MODEL_CHOICES	1


class C_OP_RenderModels : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderModels );


	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK | PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_ALPHA2_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK |
			   PARTICLE_ATTRIBUTE_ROTATION_MASK | PARTICLE_ATTRIBUTE_YAW_MASK | PARTICLE_ATTRIBUTE_PITCH_MASK | PARTICLE_ATTRIBUTE_NORMAL_MASK | 1 << m_nAnimationScaleField;
	}

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	virtual bool IsBatchable() const
	{
		return false;
	}

	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const;

	virtual void Precache( void );
	char m_ActivityName[256];
	char m_pszModelNames[ MAX_MODEL_CHOICES ][256];
	void *m_pModels[ MAX_MODEL_CHOICES ];
	bool m_bOrientZ;
	bool m_bScaleAnimationRate;
	int m_nAnimationScaleField;
	int m_nSkin;
	int m_nActivity;
	float m_flAnimationRate;

};

DEFINE_PARTICLE_OPERATOR( C_OP_RenderModels, "Render models", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderModels ) 
	DMXELEMENT_UNPACK_FIELD_STRING_USERDATA( "sequence 0 model", "NONE", m_pszModelNames[0], "mdlPicker" )
	DMXELEMENT_UNPACK_FIELD( "animation rate", "30.0", float, m_flAnimationRate )
	DMXELEMENT_UNPACK_FIELD( "scale animation rate", "0", bool, m_bScaleAnimationRate )
	DMXELEMENT_UNPACK_FIELD_USERDATA( "animation rate scale field", "10", int, m_nAnimationScaleField, "intchoice particlefield_scalar" )
	DMXELEMENT_UNPACK_FIELD( "orient model z to normal", "0", bool, m_bOrientZ )
	DMXELEMENT_UNPACK_FIELD( "skin number", "0", int, m_nSkin )
	DMXELEMENT_UNPACK_FIELD_STRING( "activity override", "", m_ActivityName )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderModels )


void C_OP_RenderModels::Precache( void )
{
	// this is the the render operator sequences above, as each one has to be hard coded
	Assert( MAX_MODEL_CHOICES == 1 );	

	for( int i = 0; i < MAX_MODEL_CHOICES ; i++ )
	{
		m_pModels[ i ] = g_pParticleSystemMgr->Query()->GetModel( m_pszModelNames[i] );
	}
	
	// Have to do this here or the model isn't loaded yet
	if ( V_strcmp( m_ActivityName, "" ) )
		m_nActivity = g_pParticleSystemMgr->Query()->GetActivityNumber( m_pModels[ 0 ], m_ActivityName );
	else
		m_nActivity = -1;
}

// return a vector perpendicular to another, with smooth variation
static void AVectorPerpendicularToVector( Vector const &in, Vector *pvecOut )
{
	float flY = in.y * in.y;
	pvecOut->x = RemapVal( flY, 0, 1, in.z, 1 );
	pvecOut->y = 0;
	pvecOut->z = -in.x;
	pvecOut->NormalizeInPlace();
	float flDot = DotProduct( *pvecOut, in );
	*pvecOut -= flDot * in;
	pvecOut->NormalizeInPlace();
}

void C_OP_RenderModels::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
{
	int nNumParticles;
	CParticleVisibilityData visibilityData;
	visibilityData.m_flAlphaVisibility = 1.0;
	visibilityData.m_flRadiusVisibility = 1.0;
	visibilityData.m_bUseVisibility = false;

	const ParticleRenderData_t *pRenderList = 
		pParticles->GetRenderList( pRenderContext, false, &nNumParticles, &visibilityData );

	g_pParticleSystemMgr->Query()->BeginDrawModels( nNumParticles, pParticles->m_Center, pParticles );
	size_t xyz_stride;
	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

	size_t seq_stride;
	const fltx4 *pSequenceNumber = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, &seq_stride );

	size_t seq1_stride;
	const fltx4 *pSequence1Number = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1, &seq1_stride );

	size_t rgb_stride;
	const fltx4 *pRGB = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, &rgb_stride );

	size_t nAlphaStride;
	const fltx4 *pAlpha = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ALPHA, &nAlphaStride );

	size_t nAlpha2Stride;
	const fltx4 *pAlpha2 = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ALPHA2, &nAlpha2Stride );

	size_t nRadStride;
	const fltx4 *pRadius = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_RADIUS, &nRadStride );

	size_t nRotStride;
	const fltx4 *pRot = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_ROTATION, &nRotStride );

	size_t nYawStride;
	const fltx4 *pYaw = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_YAW, &nYawStride );

	size_t nPitchStride;
	const fltx4 *pPitch = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_PITCH, &nPitchStride );

	size_t nScalerStride;
	const fltx4 *pAnimationScale = pParticles->GetM128AttributePtr( m_nAnimationScaleField, &nScalerStride );

	for( int i = 0; i < nNumParticles; i++ )
	{
		int hParticle = ( --pRenderList )->m_nIndex;
		int nGroup = ( hParticle / 4 );
		int nOffset = hParticle & 0x3;

		int nSequence = ( int )SubFloat( pSequenceNumber[ nGroup * seq_stride ], nOffset );
		int nAnimationSequence = m_nActivity;
		if ( nAnimationSequence == -1 )
			nAnimationSequence = ( int )SubFloat( pSequence1Number[ nGroup * seq1_stride ], nOffset );
		float flAnimationRate = m_flAnimationRate;
		if ( m_bScaleAnimationRate )
			flAnimationRate *= SubFloat( pAnimationScale[ nGroup * nScalerStride ], nOffset );

		int nXYZIndex = nGroup * xyz_stride;
		Vector vecWorldPos( SubFloat( xyz[ nXYZIndex ], nOffset ), SubFloat( xyz[ nXYZIndex+1 ], nOffset ), SubFloat( xyz[ nXYZIndex+2 ], nOffset ) );

		const float *pNormal = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_NORMAL, hParticle );
		Vector vecFwd, vecRight, vecUp;
		SetVectorFromAttribute( vecFwd, pNormal);
		vecFwd.NormalizeInPlace();
		AVectorPerpendicularToVector( vecFwd, &vecRight );
		float flDot = fabs( DotProduct( vecFwd, vecRight ) );
		Assert( flDot < 0.1f );
		if ( flDot >= 0.1f )
		{
			AVectorPerpendicularToVector( vecFwd, &vecRight );
		}
		vecUp = CrossProduct( vecFwd, vecRight );
		Assert( fabs( DotProduct( vecFwd, vecUp ) ) < 0.1f );
		Assert( fabs( DotProduct( vecRight, vecUp ) ) < 0.1f );

		int nColorIndex = nGroup * rgb_stride;
		float r = SubFloat( pRGB[ nColorIndex ], nOffset );
		float g = SubFloat( pRGB[ nColorIndex + 1 ], nOffset );
		float b = SubFloat( pRGB[ nColorIndex + 2 ], nOffset );
		float a = SubFloat( ( pAlpha[ nGroup * nAlphaStride ] * pAlpha2[ nGroup * nAlpha2Stride ] ), nOffset );

		float flScale = SubFloat( pRadius[ nGroup * nRadStride ], nOffset );

		float rot = SubFloat( pRot[ nGroup * nRotStride ], nOffset );
		float yaw = SubFloat( pYaw[ nGroup * nRotStride ], nOffset );
		float pitch = SubFloat( pPitch[ nGroup * nRotStride ], nOffset );

		matrix3x4_t matRotate, matDir, matFinal;

		QAngle qa( RAD2DEG( pitch ), RAD2DEG( yaw ), RAD2DEG( rot ) );
		AngleMatrix( qa, matRotate );

		if ( m_bOrientZ )
		{
			matDir = matrix3x4_t( vecUp * flScale, -vecRight * flScale, vecFwd * flScale, vec3_origin );
		}
		else
		{
			matDir = matrix3x4_t( vecFwd * flScale, vecRight * flScale, vecUp * flScale, vec3_origin );
		}

		MatrixMultiply( matDir, matRotate, matFinal );

		matFinal.SetOrigin( vecWorldPos );

		g_pParticleSystemMgr->Query()->DrawModel( m_pModels[ 0 ], matFinal, pParticles, hParticle, nSequence, 1, m_nSkin, nAnimationSequence, flAnimationRate, r, g, b, a );
	}

	g_pParticleSystemMgr->Query()->FinishDrawModels( pParticles );
}





// rj: this is just temporary until I get another aspect of this done

//-----------------------------------------------------------------------------
//
// Projected renderer
//
//-----------------------------------------------------------------------------
class C_OP_RenderProjected : public CParticleRenderOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RenderProjected );

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK | PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_ALPHA2_MASK |
			   PARTICLE_ATTRIBUTE_RADIUS_MASK | PARTICLE_ATTRIBUTE_ROTATION_MASK;
	}

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		void **pCtx = reinterpret_cast< void ** >( pContext );

		*pCtx = NULL;
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( void * );
	}

	virtual void PostSimulate( CParticleCollection *pParticles, void *pContext ) const
	{
		void **pCtx = reinterpret_cast< void ** >( pContext );

		IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

		//	size_t xyz_stride;
		//	const fltx4 *xyz = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ, &xyz_stride );

		if ( pParticles->m_nActiveParticles >= 1 )
		{
			for ( int i = 0; i < pParticles->m_nActiveParticles; ++i )
			{
				Vector vPosition;

				const float *pflXYZ = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, i );
				SetVectorFromAttribute( vPosition, pflXYZ );

				const int *pParticleID = pParticles->GetIntAttributePtr( PARTICLE_ATTRIBUTE_PARTICLE_ID, i );

				const float *pAlpha = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_ALPHA, i );
				const float *pAlpha2 = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_ALPHA2, i );
				const float *pColor = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, i );
				const float *pRadius = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, i );
				const float *pRotation = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_ROTATION, i );

				g_pParticleSystemMgr->Query()->UpdateProjectedTexture( *pParticleID, pMaterial, vPosition, pRadius[ 0 ], pRotation[ 0 ], pColor[ 0 ], pColor[ 1 ], pColor[ 2 ], pAlpha[ 0 ] * pAlpha2[ 0 ], *pCtx );
				break;
			}
		}
		else
		{
			*pCtx = NULL;
		}
	}

	virtual void Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const;
};


DEFINE_PARTICLE_OPERATOR( C_OP_RenderProjected, "Render projected", OPERATOR_SINGLETON );

BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( C_OP_RenderProjected ) 
END_PARTICLE_OPERATOR_UNPACK( C_OP_RenderProjected )


void C_OP_RenderProjected::Render( IMatRenderContext *pRenderContext, CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
{
	if ( g_pParticleSystemMgr->Query()->IsEditor() == false )
	{
		return;
	}

	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();
	pRenderContext->Bind( pMaterial );

	for ( int i = 0; i < pParticles->m_nActiveParticles; ++i )
	{
		Vector vPosition;

		const float *pflXYZ = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, i );
		SetVectorFromAttribute( vPosition, pflXYZ );

//		const int *pParticleID = pParticles->GetIntAttributePtr( PARTICLE_ATTRIBUTE_PARTICLE_ID, i );

		const float *pAlpha = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_ALPHA, i );
		const float *pAlpha2 = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_ALPHA2, i );
		const float *pColor = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_TINT_RGB, i );
		const float *pRadius = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, i );
		const float *pRotation = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_ROTATION, i );

		FlashlightState_t state;
		VMatrix WorldToTexture;

		state.m_vecLightOrigin = Vector( pflXYZ[ 0 ], pflXYZ[ 1 ] , pflXYZ[ 2 ] );
		float flAlpha = pAlpha[ 0 ] * pAlpha2[ 0 ];
		state.m_Color[0] = pColor[ 0 ] * flAlpha;
		state.m_Color[1] = pColor[ 1 ] * flAlpha;
		state.m_Color[2] = pColor[ 2 ] * flAlpha;
		state.m_Color[3] = 0.0f; // fixme: need to make ambient work m_flAmbient;
		state.m_flProjectionSize = pRadius[ 0 ];
		state.m_flProjectionRotation = pRotation[ 0 ];

		pRenderContext->SetFlashlightState( state, WorldToTexture );

//		g_pParticleSystemMgr->Query()->UpdateProjectedTexture( *pParticleID, pMaterial, vPosition, pRadius[ 0 ], pRotation[ 0 ], pColor[ 0 ], pColor[ 1 ], pColor[ 2 ], pAlpha[ 0 ] * pAlpha2[ 0 ], *pCtx );

		CMeshBuilder meshBuilder;
		IMesh *pMesh = pRenderContext->GetDynamicMesh( true );

		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Position3f( -1000.0f, -1000.0f, 0.0f );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		meshBuilder.Position3f( 1000.0f, -1000.0f, 0.0f );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		meshBuilder.Position3f( 1000.0f, 1000.0f, 0.0f );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		meshBuilder.Position3f( -1000.0f, 1000.0f, 0.0f );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		meshBuilder.End();
		pMesh->DrawModulated( vecDiffuseModulation );

		break;
	}
}










//-----------------------------------------------------------------------------
// Installs renderers
//-----------------------------------------------------------------------------
void AddBuiltInParticleRenderers( void )
{
#ifdef _DEBUG
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderPoints );
#endif
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderSprites );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderSpritesTrail );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderRope );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderScreenVelocityRotate );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderModels );
#ifdef USE_BLOBULATOR
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderBlobs );
#endif // blobs
	REGISTER_PARTICLE_OPERATOR( FUNCTION_RENDERER, C_OP_RenderProjected );
}




