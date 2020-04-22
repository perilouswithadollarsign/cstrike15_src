//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "matsys_controls/vmtpreviewpanel.h"
#include "matsys_controls/matsyscontrols.h"
#include "matsys_controls/sheetsequencepanel.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imesh.h"
#include "tier1/keyvalues.h"
#include "bitmap/psheet.h"
#include "materialsystem/imaterialvar.h"
#include "tier1/utlbuffer.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


using namespace vgui;


#define FOV 90.0f
#define ZNEAR 0.1f
#define ZFAR 2000.0f
#define ROTATION_SPEED 40.0f		// degrees/sec
#define VIEW_DISTANCE 12.0f

//-----------------------------------------------------------------------------
//
// VMT Preview panel
//
//-----------------------------------------------------------------------------

CVMTPreviewPanel::CVMTPreviewPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_pMaterialSheet = NULL;

	SetVMT( "//platform/materials/vgui/vtfnotloaded" );

	m_pLightmapTexture.Init( "//platform/materials/debug/defaultlightmap", "editor" );
	m_DefaultEnvCubemap.Init( "editor/cubemap", "editor", true );
	m_LightDirection.Init( 0.0f, 1.0f, -1.0f );
	m_LightColor.SetColor( 255, 255, 255, 255 );
	m_flLightIntensity = 2.0f;
	m_bDrawIn3DMode = false;

	m_flSheetPreviewSpeed = 750.0f;
	m_nCurrentSheetSequence = 0;
	m_nCurrentSecondarySheetSequence = 0;

	// Reset the camera direction
	m_vecCameraDirection.Init( 1.0f, 0.0f, 0.0f );
	m_flLastRotationTime = Plat_FloatTime();
	m_flLastSwitchTime = Plat_FloatTime();
}


//-----------------------------------------------------------------------------
// Sets the current VMT
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::SetVMT( const char *pMaterialName )
{
	m_Material.Init( pMaterialName, "editor material" );
	m_VMTName = pMaterialName;

	m_flLastSwitchTime = Plat_FloatTime();

	if ( m_pMaterialSheet )
	{
		delete m_pMaterialSheet;
	}

	if ( m_bDrawIn3DMode )
	{
		m_pMaterialSheet = new CSheetExtended( m_Material );
	}
	else
	{
		m_pMaterialSheet = NULL;
	}

	m_nCurrentSheetSequence = 0;
	m_nCurrentSecondarySheetSequence = 0;
}

void CVMTPreviewPanel::SetSheetPreviewSpeed( float flPreviewSpeed )
{
	m_flSheetPreviewSpeed = flPreviewSpeed;
}

//-----------------------------------------------------------------------------
// Gets the current VMT
//-----------------------------------------------------------------------------
const char *CVMTPreviewPanel::GetVMT() const
{
	return m_VMTName;
}

CSheetExtended* CVMTPreviewPanel::GetSheet()
{
	return m_pMaterialSheet;
}

IMaterial* CVMTPreviewPanel::GetMaterial()
{
	return m_Material;
}

//-----------------------------------------------------------------------------
// View it in 3D or 2D mode
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::DrawIn3DMode( bool b3DMode )
{
	m_bDrawIn3DMode = b3DMode;
}


//-----------------------------------------------------------------------------
// Sets up lighting state
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::SetupLightingState()
{
	MaterialLightingState_t state;
	memset( &state, 0, sizeof(state) );

	state.m_nLocalLightCount = 1;
	LightDesc_t &desc = state.m_pLocalLightDesc[0];

	desc.m_Type = MATERIAL_LIGHT_DIRECTIONAL;

	desc.m_Color[0] = m_LightColor.r();
	desc.m_Color[1] = m_LightColor.g();
	desc.m_Color[2] = m_LightColor.b();
	desc.m_Color *= m_flLightIntensity / 255.0f;

	desc.m_Attenuation0 = 1.0f;
	desc.m_Attenuation1 = 0.0f;
	desc.m_Attenuation2 = 0.0f;

 	desc.m_Direction = m_LightDirection;
	VectorNormalize( desc.m_Direction );

	desc.m_Theta = 0.0f;
	desc.m_Phi = 0.0f;
	desc.m_Falloff = 1.0f;
	desc.RecalculateDerivedValues();

	CMatRenderContextPtr pRenderContext( MaterialSystem() );
	pRenderContext->SetLightingState( state );
}

	
//-----------------------------------------------------------------------------
// Draw a sphere
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::RenderSphere( const Vector &vCenter, float flRadius, int nTheta, int nPhi )
{
	int nVertices =  nTheta * nPhi;
	int nIndices = 2 * ( nTheta + 1 ) * ( nPhi - 1 );
	
	CMatRenderContextPtr pRenderContext( MaterialSystem() );

	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, nVertices, nIndices );

	bool bIsUsingLightmap = m_Material->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_LIGHTMAP );
	bool bIsUsingBumpedLightmap = m_Material->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS );

	int nLightmapWidth = m_pLightmapTexture->GetActualWidth();
	float flHalfLuxel = 0.5f / nLightmapWidth;

	//
	// Build the index buffer.
	//
	int i, j;
	for ( i = 0; i < nPhi; ++i )
	{
		for ( j = 0; j < nTheta; ++j )
		{
			float u = j / ( float )(nTheta - 1);
			float v = i / ( float )(nPhi - 1);
			float theta = ( j != nTheta-1 ) ? 2.0f * M_PI * u : 0.0f;
			float phi = M_PI * v;

			Vector vecPos;
			vecPos.x = flRadius * sin(phi) * cos(theta);
			vecPos.y = flRadius * sin(phi) * sin(theta); 
			vecPos.z = flRadius * cos(phi);
			    
			Vector vecNormal = vecPos;
			VectorNormalize( vecNormal );

			Vector4D vecTangentS;
			Vector vecTangentT;
			vecTangentS.Init( vecPos.z, -vecPos.x, 0.0f, 1.0f );
			if ( VectorNormalize( vecTangentS.AsVector3D() ) == 0.0f )
			{
				vecTangentS.Init( 1.0f, 0.0f, 0.0f, 1.0f );
			}

			CrossProduct( vecNormal, vecTangentS.AsVector3D(), vecTangentT );

			unsigned char red = (int)( u * 255.0f );
			unsigned char green = (int)( v * 255.0f );
			unsigned char blue = (int)( v * 255.0f );
			unsigned char alpha = (int)( v * 255.0f );

			vecPos += vCenter;

			float u1, u2, v1, v2;
			u1 = u2 = u;
			v1 = v2 = v;

			if ( bIsUsingLightmap )
			{
				u1 = RemapVal( u1, 0.0f, 1.0f, flHalfLuxel, 0.25 - flHalfLuxel );

				if ( bIsUsingBumpedLightmap )
				{
					u2 = 0.25f;
					v2 = 0.0f;
				}
			}
					  
			meshBuilder.Position3fv( vecPos.Base() );
			meshBuilder.Normal3fv( vecNormal.Base() );
			meshBuilder.Color4ub( red, green, blue, alpha );
			meshBuilder.TexCoord4f( 0, 2.0f * u, v, 0, 0 );
			meshBuilder.TexCoord4f( 1, u1, v1, 0, 0 );
			meshBuilder.TexCoord4f( 2, u2, v2, 0, 0 );
			meshBuilder.TexCoord4f( 3, u1, v1, 0, 0 );
			meshBuilder.TexCoord4f( 4, u2, v2, 0, 0 );
			meshBuilder.TangentS3fv( vecTangentS.Base() );
			meshBuilder.TangentT3fv( vecTangentT.Base() );
			meshBuilder.BoneWeight( 0, 1.0f );
			meshBuilder.BoneMatrix( 0, 0 );
			meshBuilder.UserData( vecTangentS.Base() );
			meshBuilder.AdvanceVertex();
		}
	}

	//
	// Emit the triangle strips.
	//
	int idx = 0;
	for ( i = 0; i < nPhi - 1; ++i )
	{
		for ( j = 0; j < nTheta; ++j )
		{
			idx = nTheta * i + j;

			meshBuilder.FastIndex( idx );
			meshBuilder.FastIndex( idx + nTheta );
		}

		//
		// Emit a degenerate triangle to skip to the next row without
		// a connecting triangle.
		//
		if ( i < nPhi - 2 )
		{
			meshBuilder.FastIndex( idx + 1 );
			meshBuilder.FastIndex( idx + 1 + nTheta );
		}
	}

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Draw sprite-card based materials
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::RenderSheet( const Vector &vCenter, float flRadius )
{
	CMatRenderContextPtr pRenderContext( MaterialSystem() );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();

	if ( !m_pMaterialSheet->ValidSheetData() )
		return;

	float flAge = fmodf( Plat_FloatTime() - m_flLastSwitchTime, m_pMaterialSheet->GetSequenceTimeSpan( m_nCurrentSheetSequence ) );
	m_pMaterialSheet->DrawSheet( pMesh, vCenter, flRadius, m_nCurrentSheetSequence, flAge, m_flSheetPreviewSpeed, true, m_nCurrentSecondarySheetSequence );
}

//-----------------------------------------------------------------------------
// Paints a regular texture
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::DrawRectangle( void )
{		     
	// Get the aspect ratio of the material
	int tw = m_Material->GetMappingWidth();
 	int th = m_Material->GetMappingHeight();

	if ( tw <= 0 || th <= 0 )
		return;

	int w, h;
	GetSize( w, h );
	if ( w == 0 || h == 0 )
		return;

 	SetupOrthoMatrix( w, h );
	SetupLightingState();

	CMatRenderContextPtr pRenderContext( MaterialSystem() );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();

	IMesh* pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 4, 4 );

	bool bIsUsingLightmap = m_Material->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_LIGHTMAP );
	bool bIsUsingBumpedLightmap = m_Material->GetPropertyFlag( MATERIAL_PROPERTY_NEEDS_BUMPED_LIGHTMAPS );

	int nLightmapWidth = m_pLightmapTexture->GetActualWidth();
	float flHalfLuxel = 0.5f / nLightmapWidth;
	Vector2D halfTexel( 0.5f / tw, 0.5f / th );

	Vector vecNormal( 0.0f, 0.0f, 1.0f );
	Vector4D vecTangentS( 1.0f, 0.0f, 0.0f, 1.0f );
	Vector vecTangentT;
	CrossProduct( vecNormal, vecTangentS.AsVector3D(), vecTangentT );

	float screenaspect = (float)tw / (float)th;
	float aspect = (float)w / (float)h;

	float ratio = screenaspect / aspect;
	   
	// Screen is wider, need bars at top and bottom
	int x2, y2;
	int x, y;
	x = y = 0;
	int nXBorder = w > 15 ? 5 : w / 3;
	int nYBorder = h > 15 ? 5 : h / 3;
	w -= 2 * nXBorder;
	h -= 2 * nYBorder;
	if ( ratio > 1.0f )
	{
		int usetall = (float)w / screenaspect;
		y = ( h - usetall ) / 2;
		h = usetall;
	}
	// Screen is narrower, need bars at left/right
	else
	{
		int usewide = (float)h * screenaspect;
		x = ( w - usewide ) / 2;
		w = usewide;
	}
	x += nXBorder;
	y += nYBorder;

	x2 = x+w; y2 = y+h;

	float u = halfTexel.x;
	float v = halfTexel.y;

	float u1_l, u1_r, v1_t, v1_b;
	float u2_l, u2_r, v2_t, v2_b;

	u1_l = u2_l = u;
	u1_r = u2_r = 1.0f - u;
	v1_t = v2_t = v;
	v1_b = v2_b = 1.0f - v;

	if ( bIsUsingLightmap )
	{
		u1_l = v1_t = flHalfLuxel;
		u1_r = v1_b = 0.25 - flHalfLuxel;
		if ( bIsUsingBumpedLightmap )
		{
			u2_l = u2_r = 0.25f;
			v2_t = v2_b = 0.0f;
		}
	}

	bool m_bPreviewVertexColors = false;
	  
	meshBuilder.Position3f( x, y2, 0.0f );
	meshBuilder.Normal3fv( vecNormal.Base() );
	if ( m_bPreviewVertexColors )
	{
		meshBuilder.Color4ub( 255, 0, 0, 255 );
	}
	else
	{
		meshBuilder.Color4ub( 255, 255, 255, 255 );
	}
	meshBuilder.TexCoord4f( 0, u, v, 0, 0 );
	meshBuilder.TexCoord4f( 1, u1_l, v1_t, 0, 0 );
	meshBuilder.TexCoord4f( 2, u2_l, v2_t, 0, 0 );
	meshBuilder.TexCoord4f( 3, u1_l, v1_t, 0, 0 );
	meshBuilder.TexCoord4f( 4, u2_l, v2_t, 0, 0 );
	meshBuilder.TexCoord4f( 5, u1_l, v1_t, 0, 0 );
	meshBuilder.TexCoord4f( 6, u2_l, v2_t, 0, 0 );
	meshBuilder.TexCoord4f( 7, u1_l, v1_t, 0, 0 );
	meshBuilder.TangentS3fv( vecTangentS.Base() );
	meshBuilder.TangentT3fv( vecTangentT.Base() );
	meshBuilder.BoneWeight( 0, 1.0f );
	meshBuilder.BoneMatrix( 0, 0 );
	meshBuilder.UserData( vecTangentS.Base() );
	meshBuilder.AdvanceVertex();
				    
	meshBuilder.Position3f( x, y, 0.0f );
	meshBuilder.Normal3fv( vecNormal.Base() );
	if ( m_bPreviewVertexColors )
	{
		meshBuilder.Color4ub( 255, 255, 255, 64 );
	}
	else
	{
		meshBuilder.Color4ub( 255, 255, 255, 255 );
	}
	meshBuilder.TexCoord4f( 0, u, 1.0f - v, 0, 0 );
	meshBuilder.TexCoord4f( 1, u1_l, v1_b, 0, 0 );
	meshBuilder.TexCoord4f( 2, u2_l, v2_b, 0, 0 );
	meshBuilder.TexCoord4f( 3, u1_l, v1_b, 0, 0 );
	meshBuilder.TexCoord4f( 4, u2_l, v2_b, 0, 0 );
	meshBuilder.TexCoord4f( 5, u1_l, v1_b, 0, 0 );
	meshBuilder.TexCoord4f( 6, u2_l, v2_b, 0, 0 );
	meshBuilder.TexCoord4f( 7, u1_l, v1_b, 0, 0 );
	meshBuilder.TangentS3fv( vecTangentS.Base() );
	meshBuilder.TangentT3fv( vecTangentT.Base() );
	meshBuilder.BoneWeight( 0, 1.0f );
	meshBuilder.BoneMatrix( 0, 0 );
	meshBuilder.UserData( vecTangentS.Base() );
	meshBuilder.AdvanceVertex();
			    
	meshBuilder.Position3f( x2, y2, 0.0f );
	meshBuilder.Normal3fv( vecNormal.Base() );
	if ( m_bPreviewVertexColors )
	{
		meshBuilder.Color4ub( 0, 0, 255, 255 );
	}
	else
	{
		meshBuilder.Color4ub( 255, 255, 255, 255 );
	}
	meshBuilder.TexCoord4f( 0, 1.0f - u, v, 0, 0 );
	meshBuilder.TexCoord4f( 1, u1_r, v1_t, 0, 0 );
	meshBuilder.TexCoord4f( 2, u2_r, v2_t, 0, 0 );
	meshBuilder.TexCoord4f( 3, u1_r, v1_t, 0, 0 );
	meshBuilder.TexCoord4f( 4, u2_r, v2_t, 0, 0 );
	meshBuilder.TexCoord4f( 5, u1_r, v1_t, 0, 0 );
	meshBuilder.TexCoord4f( 6, u2_r, v2_t, 0, 0 );
	meshBuilder.TexCoord4f( 7, u1_r, v1_t, 0, 0 );
	meshBuilder.TangentS3fv( vecTangentS.Base() );
	meshBuilder.TangentT3fv( vecTangentT.Base() );
	meshBuilder.BoneWeight( 0, 1.0f );
	meshBuilder.BoneMatrix( 0, 0 );
	meshBuilder.UserData( vecTangentS.Base() );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x2, y, 0.0f );
	meshBuilder.Normal3fv( vecNormal.Base() );
	if ( m_bPreviewVertexColors )
	{
		meshBuilder.Color4ub( 0, 255, 0, 64 );
	}
	else
	{
		meshBuilder.Color4ub( 255, 255, 255, 255 );
	}
	meshBuilder.TexCoord4f( 0, 1.0f - u, 1.0f - v, 0, 0 );
	meshBuilder.TexCoord4f( 1, u1_r, v1_b, 0, 0 );
	meshBuilder.TexCoord4f( 2, u2_r, v2_b, 0, 0 );
	meshBuilder.TexCoord4f( 3, u1_r, v1_b, 0, 0 );
	meshBuilder.TexCoord4f( 4, u2_r, v2_b, 0, 0 );
	meshBuilder.TexCoord4f( 5, u1_r, v1_b, 0, 0 );
	meshBuilder.TexCoord4f( 6, u2_r, v2_b, 0, 0 );
	meshBuilder.TexCoord4f( 7, u1_r, v1_b, 0, 0 );
	meshBuilder.TangentS3fv( vecTangentS.Base() );
	meshBuilder.TangentT3fv( vecTangentT.Base() );
	meshBuilder.BoneWeight( 0, 1.0f );
	meshBuilder.BoneMatrix( 0, 0 );
	meshBuilder.UserData( vecTangentS.Base() );
	meshBuilder.AdvanceVertex();
						  
	meshBuilder.FastIndex( 0 );
	meshBuilder.FastIndex( 1 );
	meshBuilder.FastIndex( 2 );
	meshBuilder.FastIndex( 3 );

	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Paints a cubemap texture
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::DrawSphere( void )
{
	float flNewTime = Plat_FloatTime();

	// Circle the camera around the origin
	VMatrix rot;
	MatrixBuildRotateZ( rot, ROTATION_SPEED * (flNewTime - m_flLastRotationTime ) );
	Vector vecTemp;
	Vector3DMultiply( rot, m_vecCameraDirection, vecTemp );
	m_vecCameraDirection = vecTemp;
	m_flLastRotationTime = flNewTime;

	int w, h;
	GetSize( w, h );
 	SetupProjectionMatrix( w, h );
	SetupLightingState();

	LookAt( vec3_origin, VIEW_DISTANCE );

	// Draw a sphere at the origin
//	RenderSphere( vec3_origin, 10.0f, 20, 20 );
	if ( m_pMaterialSheet && m_pMaterialSheet->ValidSheetData() )
	{
		RenderSheet( vec3_origin, 10.0f );
	}
	else
	{
		RenderSphere( vec3_origin, 10.0f, 20, 20 );
	}
}

//-----------------------------------------------------------------------------
// Sets the camera to look at the the thing we're spinning around
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::LookAt( const Vector &vecLookAt, float flRadius )
{
	// Compute the distance to the camera for the object based on its
	// radius and fov.

	// since tan( fov/2 ) = f/d
	// cos( fov/2 ) = r / r' where r = sphere radius, r' = perp distance from sphere center to max extent of camera
	// d/f = r'/d' where d' is distance of camera to sphere
	// d' = r' / tan( fov/2 ) * r' = r / ( cos (fov/2) * tan( fov/2 ) ) = r / sin( fov/2 )
	float flFOVx = FOV;

	// Compute fov/2 in radians
	flFOVx *= M_PI / 360.0f;

	// Compute an effective fov	based on the aspect ratio 
	// if the height is smaller than the width
	int w, h;
	GetSize( w, h );
	if ( h < w )
	{
		flFOVx = atan( h * tan( flFOVx ) / w );
	}

	float flDistance = flRadius / sin( flFOVx );

	Vector vecMDLOrigin = vecLookAt;
	Vector vecCameraOrigin;
	VectorMA( vecMDLOrigin, -flDistance, m_vecCameraDirection, vecCameraOrigin );

	CMatRenderContextPtr pRenderContext( MaterialSystem() );
	QAngle angles;
	VectorAngles( m_vecCameraDirection, angles );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->LoadIdentity();

	// convert from a right handed system to a left handed system
	// since dx for wants it that way.  
//	pRenderContext->Scale( 1.0f, 1.0f, -1.0f );

	pRenderContext->Rotate( -90,  1, 0, 0 );	    // put Z going up
	pRenderContext->Rotate( 90,  0, 0, 1 );	    // put Z going up
	pRenderContext->Rotate( -angles[2],  1, 0, 0 );
	pRenderContext->Rotate( -angles[0],  0, 1, 0 );
	pRenderContext->Rotate( -angles[1],  0, 0, 1 );
	pRenderContext->Translate( -vecCameraOrigin[0],  -vecCameraOrigin[1],  -vecCameraOrigin[2] );
}


//-----------------------------------------------------------------------------
// Set up a projection matrix for a 90 degree fov
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::SetupProjectionMatrix( int nWidth, int nHeight )
{
	VMatrix proj;
	float flFOV = FOV;
	float flZNear = ZNEAR;
	float flZFar = ZFAR;
	float flApsectRatio = (nHeight != 0.0f) ? (float)nWidth / (float)nHeight : 100.0f;

	float halfWidth = tan( flFOV * M_PI / 360.0 );
	float halfHeight = halfWidth / flApsectRatio;

	memset( proj.Base(), 0, sizeof( proj ) );
	proj[0][0]  = 1.0f / halfWidth;
	proj[1][1]  = 1.0f / halfHeight;
	proj[2][2] = flZFar / ( flZNear - flZFar );
	proj[3][2] = -1.0f;
	proj[2][3] = flZNear * flZFar / ( flZNear - flZFar );

	CMatRenderContextPtr pRenderContext( MaterialSystem() );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadMatrix( proj );
}


//-----------------------------------------------------------------------------
// Set up a orthographic projection matrix
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::SetupOrthoMatrix( int nWidth, int nHeight )
{
	CMatRenderContextPtr pRenderContext( MaterialSystem() );
	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->LoadIdentity();
	pRenderContext->Ortho( 0, 0, nWidth, nHeight, -1.0f, 1.0f );
}


//-----------------------------------------------------------------------------
// Power of two FB texture
//-----------------------------------------------------------------------------
static CTextureReference s_pPowerOfTwoFrameBufferTexture;

static ITexture *GetPowerOfTwoFrameBufferTexture( void )
{
	if ( !s_pPowerOfTwoFrameBufferTexture )
	{
		s_pPowerOfTwoFrameBufferTexture.Init( vgui::MaterialSystem()->FindTexture( "_rt_PowerOfTwoFB", TEXTURE_GROUP_RENDER_TARGET ) );
	}
	
	return s_pPowerOfTwoFrameBufferTexture;
}


//-----------------------------------------------------------------------------
// Paints the texture
//-----------------------------------------------------------------------------
void CVMTPreviewPanel::Paint( void )
{
	CMatRenderContextPtr pRenderContext( MaterialSystem() );
	int w, h;
	GetSize( w, h );
	vgui::MatSystemSurface()->Begin3DPaint( 0, 0, w, h );

	// Deal with refraction
	if ( m_Material->NeedsPowerOfTwoFrameBufferTexture() )
	{
		ITexture *pTexture = GetPowerOfTwoFrameBufferTexture();
		if ( pTexture && !pTexture->IsError() )
		{
			pRenderContext->CopyRenderTargetToTexture( pTexture );
			pRenderContext->SetFrameBufferCopyTexture( pTexture );
		}
	}

	pRenderContext->ClearColor4ub( 76, 88, 68, 255 ); 
	pRenderContext->ClearBuffers( true, true );
	 			   
	pRenderContext->FogMode( MATERIAL_FOG_NONE );
	pRenderContext->SetNumBoneWeights( 0 );
	pRenderContext->Bind( m_Material );
 	pRenderContext->BindLightmapTexture( m_pLightmapTexture );
	pRenderContext->BindLocalCubemap( m_DefaultEnvCubemap );

	if ( m_bDrawIn3DMode )
	{
		DrawSphere();
	}
	else
	{
		DrawRectangle();
	}

	vgui::MatSystemSurface()->End3DPaint( );
}

bool CVMTPreviewPanel::VMTUsesSheets()
{
	return m_pMaterialSheet != NULL;
}

int CVMTPreviewPanel::GetSheetSequenceCount()
{
	if ( m_pMaterialSheet == NULL )
	{
		return 0;
	}

	return m_pMaterialSheet->GetSheetSequenceCount();
}

// TODO: sort this out - sequence #n != nth sequence
int CVMTPreviewPanel::GetCurrentSequence()
{
	return m_nCurrentSheetSequence;
}

int CVMTPreviewPanel::GetCurrentSecondarySequence()
{
	return m_nCurrentSecondarySheetSequence;
}


int CVMTPreviewPanel::GetRealSequenceNumber()
{
	return m_nCurrentSheetSequence;
}

void CVMTPreviewPanel::SetSheetSequence( int nSequence )
{
	if ( m_pMaterialSheet == NULL )
	{
		return;
	}

	m_nCurrentSheetSequence = m_pMaterialSheet->GetNthSequenceIndex(nSequence);
}

void CVMTPreviewPanel::SetSecondarySheetSequence( int nSequence )
{
	if ( m_pMaterialSheet == NULL )
	{
		return;
	}

	m_nCurrentSecondarySheetSequence = m_pMaterialSheet->GetNthSequenceIndex(nSequence);
}
