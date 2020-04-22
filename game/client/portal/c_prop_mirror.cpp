//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

//NOTE: Mirrors with models require an attachment named "MirrorSurface_Attach" with x facing out of the mirror plane. 
//They also require that the mirror surface be in a bodygroup by itself named "MirrorSurface" with the first index being the mirror, second being empty.
//Lastly, they require that all non-mirror geometry be in bodygroups that have the second entry as empty.
//It's a good idea to put a cubemap on the mirror surface material because they're not infinitely recursive

#include "cbase.h"
#include "portalrenderable_flatbasic.h"
#include "tier0/vprof.h"
#include "model_types.h"
#include "debugoverlay_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_Prop_Mirror : public CPortalRenderable_FlatBasic
{
public:
	DECLARE_CLASS( C_Prop_Mirror, CPortalRenderable_FlatBasic );
	DECLARE_CLIENTCLASS();

	C_Prop_Mirror( void );
	virtual void Spawn( void );
	virtual void UpdateOnRemove( void );
	virtual void OnDataChanged( DataUpdateType_t type );
	virtual bool ShouldDraw( void ) { return true; }
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual CStudioHdr *OnNewModel( void );
	virtual void ClientThink( void );
	virtual bool ShouldUpdatePortalView_BasedOnView( const CViewSetup &currentView, const CUtlVector<VPlane> &currentComplexFrustum );
	virtual bool ShouldUpdatePortalView_BasedOnPixelVisibility( float fScreenFilledByStencilMaskLastFrame_Normalized ) { return true; }
	void UpdateReflectionPlane( void );
	void UpdateReflectionPolygon( void ); //call this before accessing m_LocalSpaceReflectionPolygonVerts, it'll detect if an update is necessary and do it

	virtual void DrawStencilMask( IMatRenderContext *pRenderContext );
	virtual void DrawPostStencilFixes( IMatRenderContext *pRenderContext ); 
	virtual void RenderPortalViewToBackBuffer( CViewRender *pViewRender, const CViewSetup &cameraView );
	bool CalcFrustumThroughPortal( const Vector &ptCurrentViewOrigin, Frustum OutputFrustum );

	static CMaterialReference sm_Mirror_Stencil;
	float m_fWidth;
	float m_fHeight;
	int m_iMirrorSurfaceBodyGroup;
	int m_iMirrorFaceAttachment;
	bool m_bModel;
	Vector m_LocalSpaceReflectionPolygonVerts[10]; //best guess at the reflection polygon by intersecting the reflection plane with the local space rendering OBB
	int m_LocalSpaceReflectionPolygonVertCount;

	struct ReflectPlaneCachedData_t
	{
		//Vector vAttachmentOrigin; //handled by m_ptOrigin
		QAngle qAttachmentAngle;

		bool bModel;
		Vector vLocalSpaceAttachmentOrigin;
		QAngle qLocalSpaceAttachmentAngles;
		Vector vRenderOBB_Mins;
		Vector vRenderOBB_Maxs;
	};
	ReflectPlaneCachedData_t m_CachedReflectedData;
};

CMaterialReference C_Prop_Mirror::sm_Mirror_Stencil;

IMPLEMENT_CLIENTCLASS_DT( C_Prop_Mirror, DT_Prop_Mirror, CProp_Mirror )
	RecvPropFloat( RECVINFO(m_fWidth) ),
	RecvPropFloat( RECVINFO(m_fHeight) ),
END_RECV_TABLE()

C_Prop_Mirror::C_Prop_Mirror( void )
{
	m_ptOrigin.Invalidate();
	m_CachedReflectedData.qAttachmentAngle.Invalidate();
	m_CachedReflectedData.vLocalSpaceAttachmentOrigin.Invalidate();
	m_CachedReflectedData.qLocalSpaceAttachmentAngles.Invalidate();
	m_CachedReflectedData.vRenderOBB_Maxs.Invalidate();

	m_matrixThisToLinked.m[3][0] = 0.0f;
	m_matrixThisToLinked.m[3][1] = 0.0f;
	m_matrixThisToLinked.m[3][2] = 0.0f;
	m_matrixThisToLinked.m[3][3] = 1.0f;
}

void C_Prop_Mirror::Spawn( void )
{
	BaseClass::Spawn();
	if( !sm_Mirror_Stencil.IsValid() )
	{
		sm_Mirror_Stencil.Init( "decals/portalstencildecal", TEXTURE_GROUP_CLIENT_EFFECTS );
	}

	m_pLinkedPortal = this;
	g_pPortalRender->AddPortal( this ); //will know if we're already added and avoid adding twice
}

void C_Prop_Mirror::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
	g_pPortalRender->RemovePortal( this );
}

CStudioHdr *C_Prop_Mirror::OnNewModel( void )
{
	CStudioHdr *pRetVal = BaseClass::OnNewModel();

	if( (GetModel() != NULL) )
	{
		m_iMirrorSurfaceBodyGroup = FindBodygroupByName( "MirrorSurface" );
		m_iMirrorFaceAttachment  = LookupAttachment( "MirrorSurface_Attach" );
		if( (m_iMirrorSurfaceBodyGroup >= 0) && (m_iMirrorFaceAttachment >= 0) )
		{
			m_bModel = true;
			SetBodygroup( m_iMirrorSurfaceBodyGroup, 1 ); //by default the mirror surface is hidden
		}
		else
		{
			Warning( "Prop_Mirror model missing vital data %s\n", (m_iMirrorFaceAttachment < 0) ? 
				((m_iMirrorSurfaceBodyGroup < 0) ? "MirrorSurface bodygroup and MirrorSurface_Attach attachment point" : "MirrorSurface_Attach attachment point") : 
				"MirrorSurface bodygroup" );

			m_bModel = false;
		}
	}

	if( m_bModel )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
	else
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}

	return pRetVal;
}

void C_Prop_Mirror::UpdateReflectionPlane( void )
{
	PortalMoved();
	Vector vToOrigin( m_ptOrigin.Dot( m_vForward ), m_ptOrigin.Dot( m_vRight ), -m_ptOrigin.Dot( m_vUp ) );

	//generate mirroring matrix. Move mirror to origin using base vectors, flip on forward axis, move back to position and orientation
	{
		m_matrixThisToLinked.m[0][0] = (-m_vForward.x * m_vForward.x) + (m_vRight.x * m_vRight.x) + (m_vUp.x * m_vUp.x);
		m_matrixThisToLinked.m[0][1] = (-m_vForward.x * m_vForward.y) + (m_vRight.x * m_vRight.y) + (m_vUp.x * m_vUp.y);
		m_matrixThisToLinked.m[0][2] = (-m_vForward.x * m_vForward.z) + (m_vRight.x * m_vRight.z) + (m_vUp.x * m_vUp.z);
		m_matrixThisToLinked.m[0][3] = (vToOrigin.x * m_vForward.x) - (vToOrigin.y * m_vRight.x) + (vToOrigin.z * m_vUp.x) + m_ptOrigin.x;
		m_matrixThisToLinked.m[1][0] = m_matrixThisToLinked.m[0][1]; //rotation portion of the matrix is equal to it's own transpose
		m_matrixThisToLinked.m[1][1] = (-m_vForward.y * m_vForward.y) + (m_vRight.y * m_vRight.y) + (m_vUp.y * m_vUp.y);
		m_matrixThisToLinked.m[1][2] = (-m_vForward.y * m_vForward.z) + (m_vRight.y * m_vRight.z) + (m_vUp.y * m_vUp.z);
		m_matrixThisToLinked.m[1][3] = (vToOrigin.x * m_vForward.y) - (vToOrigin.y * m_vRight.y) + (vToOrigin.z * m_vUp.y) + m_ptOrigin.y;
		m_matrixThisToLinked.m[2][0] = m_matrixThisToLinked.m[0][2]; //rotation portion of the matrix is equal to it's own transpose
		m_matrixThisToLinked.m[2][1] = m_matrixThisToLinked.m[1][2]; //rotation portion of the matrix is equal to it's own transpose
		m_matrixThisToLinked.m[2][2] = (-m_vForward.z * m_vForward.z) + (m_vRight.z * m_vRight.z) + (m_vUp.z * m_vUp.z);	
		m_matrixThisToLinked.m[2][3] = (vToOrigin.x * m_vForward.z) - (vToOrigin.y * m_vRight.z) + (vToOrigin.z * m_vUp.z) + m_ptOrigin.z;
	}

	UpdateReflectionPolygon();
}

void C_Prop_Mirror::UpdateReflectionPolygon( void )
{
	if( m_bModel != m_CachedReflectedData.bModel )
	{
		m_CachedReflectedData.qAttachmentAngle.Invalidate();
		m_CachedReflectedData.vLocalSpaceAttachmentOrigin.Invalidate();
		m_CachedReflectedData.qLocalSpaceAttachmentAngles.Invalidate();
		m_CachedReflectedData.vRenderOBB_Maxs.Invalidate();
		m_CachedReflectedData.bModel = m_bModel;
	}

	if( m_bModel )
	{
		Vector vMins, vMaxs;
		GetRenderBounds( vMins, vMaxs );

		Vector vLocalAttachmentOrigin;
		QAngle qLocalAttachmentAngles;
		GetAttachmentLocal( m_iMirrorFaceAttachment, vLocalAttachmentOrigin, qLocalAttachmentAngles );

		if( (vMins == m_CachedReflectedData.vRenderOBB_Mins) && (vMaxs == m_CachedReflectedData.vRenderOBB_Maxs) && 
			(vLocalAttachmentOrigin == m_CachedReflectedData.vLocalSpaceAttachmentOrigin) && (qLocalAttachmentAngles == m_CachedReflectedData.qLocalSpaceAttachmentAngles) )
		{
			return; //nothing to update
		}

		m_CachedReflectedData.vRenderOBB_Mins = vMins;
		m_CachedReflectedData.vRenderOBB_Maxs = vMaxs;
		m_CachedReflectedData.vLocalSpaceAttachmentOrigin = vLocalAttachmentOrigin;
		m_CachedReflectedData.qLocalSpaceAttachmentAngles = qLocalAttachmentAngles;

		Vector vAttachmentVectors[3];
		AngleVectors( qLocalAttachmentAngles, &vAttachmentVectors[0], &vAttachmentVectors[1], &vAttachmentVectors[2] );
		float fLargestOBBDiff = vMaxs.x - vMins.x;
		for( int i = 1; i != 3; ++i )
		{
			float fDiff = vMaxs[i] - vMins[i];
			if( fDiff > fLargestOBBDiff )
			{
				fLargestOBBDiff = fDiff;
			}
		}
		fLargestOBBDiff *= 4.0f; //to easily cover diagonal intersection and then some

		Vector vClipBuffers[2][10]; //4 starting points, possible to create 1 extra point per cut, 6 cuts
		vClipBuffers[0][0] = vLocalAttachmentOrigin + (vAttachmentVectors[1] * fLargestOBBDiff) + (vAttachmentVectors[2] * fLargestOBBDiff);
		vClipBuffers[0][1] = vLocalAttachmentOrigin - (vAttachmentVectors[1] * fLargestOBBDiff) + (vAttachmentVectors[2] * fLargestOBBDiff);
		vClipBuffers[0][2] = vLocalAttachmentOrigin - (vAttachmentVectors[1] * fLargestOBBDiff) - (vAttachmentVectors[2] * fLargestOBBDiff);
		vClipBuffers[0][3] = vLocalAttachmentOrigin + (vAttachmentVectors[1] * fLargestOBBDiff) - (vAttachmentVectors[2] * fLargestOBBDiff);
		int iVertCount = 4;

		VPlane vClipPlanes[6];
		vClipPlanes[0].Init( Vector( 1.0f, 0.0f, 0.0f ), vMins.x );
		vClipPlanes[1].Init( Vector( -1.0f, 0.0f, 0.0f ), -vMaxs.x );
		vClipPlanes[2].Init( Vector( 0.0f, 1.0f, 0.0f ), vMins.y );
		vClipPlanes[3].Init( Vector( 0.0f, -1.0f, 0.0f ), -vMaxs.y );
		vClipPlanes[4].Init( Vector( 0.0f, 0.0f, 1.0f ), vMins.z );
		vClipPlanes[5].Init( Vector( 0.0f, 0.0f, -1.0f ), -vMaxs.z );

		for( int i = 0; i != 6; ++i )
		{
			iVertCount = ClipPolyToPlane( vClipBuffers[i & 1], iVertCount, vClipBuffers[(i & 1) ^ 1], vClipPlanes[i].m_Normal, vClipPlanes[i].m_Dist, 0.01f ); 
		}
		Assert( iVertCount >= 3 );
		
		m_LocalSpaceReflectionPolygonVertCount = iVertCount;
		memcpy( m_LocalSpaceReflectionPolygonVerts, vClipBuffers[0], sizeof( Vector ) * iVertCount );
	}
	else
	{
		if( (m_CachedReflectedData.vRenderOBB_Maxs.x == m_fWidth) && (m_CachedReflectedData.vRenderOBB_Maxs.y == m_fHeight) )
			return;

		m_LocalSpaceReflectionPolygonVertCount = 4;
		float fHalfWidth = GetHalfWidth();
		float fHalfHeight = GetHalfHeight();
		m_LocalSpaceReflectionPolygonVerts[0].Init( 0.0f, fHalfWidth, fHalfHeight );
		m_LocalSpaceReflectionPolygonVerts[1].Init( 0.0f, -fHalfWidth, fHalfHeight );
		m_LocalSpaceReflectionPolygonVerts[2].Init( 0.0f, -fHalfWidth, -fHalfHeight );
		m_LocalSpaceReflectionPolygonVerts[3].Init( 0.0f, fHalfWidth, -fHalfHeight );
	}
}

void C_Prop_Mirror::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );
	SetHalfSizes( m_fWidth / 2.0f, m_fHeight / 2.0f );

	if( m_bModel )
	{
		Vector vMirrorAttachmentOrigin;
		QAngle qMirrorAttachmentAngles;
		GetAttachment( m_iMirrorFaceAttachment, vMirrorAttachmentOrigin, qMirrorAttachmentAngles );
		
		if( (m_ptOrigin != vMirrorAttachmentOrigin) || (m_CachedReflectedData.qAttachmentAngle != qMirrorAttachmentAngles) )
		{
			m_ptOrigin = vMirrorAttachmentOrigin;
			m_CachedReflectedData.qAttachmentAngle = qMirrorAttachmentAngles;
			AngleVectors( qMirrorAttachmentAngles, &m_vForward, &m_vRight, &m_vUp );
			UpdateReflectionPlane();
		}
	}
	else
	{
		if( (m_ptOrigin != GetRenderOrigin()) || (m_CachedReflectedData.qAttachmentAngle != GetRenderAngles()) )
		{
			m_ptOrigin = GetRenderOrigin();
			m_CachedReflectedData.qAttachmentAngle = GetRenderAngles();
			AngleVectors( GetRenderAngles(), &m_vForward, &m_vRight, &m_vUp );
			UpdateReflectionPlane();
		}
	}	
}



void C_Prop_Mirror::ClientThink( void )
{
	BaseClass::ClientThink();

	if( m_bModel )
	{
		Vector vMirrorAttachmentOrigin;
		QAngle qMirrorAttachmentAngles;
		GetAttachment( m_iMirrorFaceAttachment, vMirrorAttachmentOrigin, qMirrorAttachmentAngles );
		
		if( (m_ptOrigin != vMirrorAttachmentOrigin) || (m_CachedReflectedData.qAttachmentAngle != qMirrorAttachmentAngles) )
		{
			m_ptOrigin = vMirrorAttachmentOrigin;
			AngleVectors( qMirrorAttachmentAngles, &m_vForward, &m_vRight, &m_vUp );
			UpdateReflectionPlane();
		}
	}
}

void C_Prop_Mirror::DrawStencilMask( IMatRenderContext *pRenderContext )
{
	if( m_bModel )
	{
		int iNumBodyGroups = GetNumBodyGroups();
		int *pOldBodyGroups = (int *)stackalloc( sizeof( int ) * iNumBodyGroups );
		for( int i = 0; i != iNumBodyGroups; ++i )
		{
			pOldBodyGroups[i] = GetBodygroup( i );
			SetBodygroup( i, 1 );
		}
		pOldBodyGroups[m_iMirrorSurfaceBodyGroup] = 1;
		SetBodygroup( m_iMirrorSurfaceBodyGroup, 0 );

		RenderableInstance_t tempInstance;
		tempInstance.m_nAlpha = 255;
		modelrender->ForcedMaterialOverride( (IMaterial *)(const IMaterial *)g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
		DrawModel( STUDIO_RENDER, tempInstance );
		modelrender->ForcedMaterialOverride( NULL );

		for( int i = 0; i != iNumBodyGroups; ++i )
		{
			SetBodygroup( i, pOldBodyGroups[i] );
		}
	}
	else
	{
		DrawSimplePortalMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
	}
}

void C_Prop_Mirror::DrawPostStencilFixes( IMatRenderContext *pRenderContext )
{
	if( m_bModel )
	{
		int iNumBodyGroups = GetNumBodyGroups();
		int *pOldBodyGroups = (int *)stackalloc( sizeof( int ) * iNumBodyGroups );
		for( int i = 0; i != iNumBodyGroups; ++i )
		{
			pOldBodyGroups[i] = GetBodygroup( i );
			SetBodygroup( i, 1 );
		}
		pOldBodyGroups[m_iMirrorSurfaceBodyGroup] = 1;
		SetBodygroup( m_iMirrorSurfaceBodyGroup, 0 );

		RenderableInstance_t tempInstance;
		tempInstance.m_nAlpha = 255;
		modelrender->ForcedMaterialOverride( (IMaterial *)(const IMaterial *)g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
		DrawModel( STUDIO_RENDER, tempInstance );
		modelrender->ForcedMaterialOverride( NULL );

		for( int i = 0; i != iNumBodyGroups; ++i )
		{
			SetBodygroup( i, pOldBodyGroups[i] );
		}
	}
	else
	{
		DrawSimplePortalMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
	}
}

bool C_Prop_Mirror::ShouldUpdatePortalView_BasedOnView( const CViewSetup &currentView, const CUtlVector<VPlane> &currentComplexFrustum )
{
	if( m_vForward.Dot( currentView.origin ) <= m_vForward.Dot( m_ptOrigin ) )
		return false; //camera is behind the mirror plane

	UpdateReflectionPolygon();
	matrix3x4_t matRenderTransform;
	AngleMatrix( GetRenderAngles(), GetRenderOrigin(), matRenderTransform );

	Vector *pClipBuffers[2];
	int clipAllocSize = (m_LocalSpaceReflectionPolygonVertCount + currentComplexFrustum.Count() + 2); //possible to add 1 point per cut, m_LocalSpaceReflectionPolygonVertCount starting points, currentComplexFrustum.Count() plane cuts, 2 extra because I'm paranoid
	pClipBuffers[0] = (Vector *)stackalloc( sizeof( Vector ) * clipAllocSize * 2 );
	pClipBuffers[1] = pClipBuffers[0] + clipAllocSize;

	for( int i = 0; i != m_LocalSpaceReflectionPolygonVertCount; ++i )
	{
		VectorTransform( m_LocalSpaceReflectionPolygonVerts[i], matRenderTransform, pClipBuffers[0][i] );
	}

	const VPlane *currentFrustum = currentComplexFrustum.Base();
	int iCurrentFrustumPlanes = currentComplexFrustum.Count();

	//clip by first plane and put output into pInVerts
	int iVertCount = ClipPolyToPlane( pClipBuffers[0], m_LocalSpaceReflectionPolygonVertCount, pClipBuffers[1], currentFrustum[0].m_Normal, currentFrustum[0].m_Dist, 0.01f );

	for( int i = 1; i != iCurrentFrustumPlanes; ++i )
	{
		if( iVertCount < 3 )
			return false; //nothing left in the frustum

		iVertCount = ClipPolyToPlane( pClipBuffers[i&1], iVertCount, pClipBuffers[(i&1)^1], currentFrustum[i].m_Normal, currentFrustum[i].m_Dist, 0.01f );
	}

#if 0 //for visibility culling debugging
	int iFinalBuffer = (iCurrentFrustumPlanes & 1) ^ 1;
	if( g_pPortalRender->GetViewRecursionLevel() == 0 )
	{
		NDebugOverlay::Line( pClipBuffers[iFinalBuffer][iVertCount - 1], pClipBuffers[iFinalBuffer][0], 255, 0, 0, false, 0.0f );
		for( int j = 0; j != iVertCount - 1; ++j )
		{
			NDebugOverlay::Line( pClipBuffers[iFinalBuffer][j], pClipBuffers[iFinalBuffer][j+1], 255, 0, 0, false, 0.0f );
		}
	}
#endif

	return (iVertCount >= 3);


#if 0
	if( m_bModel )
	{
		//it would be complicated to get the exact shape of the mirror surface and cut it up, so we'll just use the model's oriented bounding box.
		//which can provide up to 3 visible quads, if any 1 of them passes the test then the entire thing is visible.

		if( m_vForward.Dot( currentView.origin ) <= m_vForward.Dot( m_ptOrigin ) )
			return false; //camera is behind the mirror plane

		Vector vMins, vMaxs;
		GetRenderBounds( vMins, vMaxs );		

		Vector vRenderOrigin = GetRenderOrigin();
		Vector vOBBVectors[3];
#if 1
		AngleVectors( GetRenderAngles(), &vOBBVectors[0], &vOBBVectors[1], &vOBBVectors[2] );
		vOBBVectors[1] = -vOBBVectors[1];
#else
		{
			matrix3x4_t fRotateMatrix;
			AngleMatrix( GetRenderAngles(), fRotateMatrix );
			VectorRotate( Vector( 1.0f, 0.0f, 0.0f ), fRotateMatrix, vOBBVectors[0] );
			VectorRotate( Vector( 0.0f, 1.0f, 0.0f ), fRotateMatrix, vOBBVectors[1] );
			VectorRotate( Vector( 0.0f, 0.0f, 1.0f ), fRotateMatrix, vOBBVectors[2] );
		}
#endif
		
		Vector vRenderOriginToCamera = currentView.origin - vRenderOrigin;

		Vector *pClipBuffers[2];
		int clipAllocSize = (6 + currentComplexFrustum.Count()); //possible to add 1 point per cut, 4 starting points, N plane cuts, 2 extra because I'm paranoid
		pClipBuffers[0] = (Vector *)stackalloc( sizeof( Vector ) * clipAllocSize * 2 );
		pClipBuffers[1] = pClipBuffers[0] + clipAllocSize;

		VPlane *currentFrustum = currentComplexFrustum.Base();
		int iCurrentFrustumPlanes = currentComplexFrustum.Count();

		for( int i = 0; i != 3; ++i )
		{
			Vector vPolyStartPoint;

			float fDot = vRenderOriginToCamera.Dot( vOBBVectors[i] );
			
			if( fDot > vMaxs[i] )
			{
				//on the positive side of this axis					
				vPolyStartPoint = vRenderOrigin + (vOBBVectors[i] * vMaxs[i]);
			}
			else if( fDot < vMins[i] )
			{
				//on the negative side of this axis
				vPolyStartPoint = vRenderOrigin + (vOBBVectors[i] * vMins[i]);
			}			
			else
			{
				//can't see the outside of the face
				continue;
			}
			

			int iBasis[2];
			iBasis[0] = (i&1) ^ 1;
			iBasis[1] = (i&2) ^ 2;

			pClipBuffers[0][0] = vPolyStartPoint + (vOBBVectors[iBasis[0]] * vMins[iBasis[0]]) + (vOBBVectors[iBasis[1]] * vMaxs[iBasis[1]]);
			pClipBuffers[0][1] = vPolyStartPoint + (vOBBVectors[iBasis[0]] * vMaxs[iBasis[0]]) + (vOBBVectors[iBasis[1]] * vMaxs[iBasis[1]]);
			pClipBuffers[0][2] = vPolyStartPoint + (vOBBVectors[iBasis[0]] * vMaxs[iBasis[0]]) + (vOBBVectors[iBasis[1]] * vMins[iBasis[1]]);
			pClipBuffers[0][3] = vPolyStartPoint + (vOBBVectors[iBasis[0]] * vMins[iBasis[0]]) + (vOBBVectors[iBasis[1]] * vMins[iBasis[1]]);

			//clip by first plane and put output into pInVerts
			int iVertCount = ClipPolyToPlane( pClipBuffers[0], 4, pClipBuffers[1], currentFrustum[0].m_Normal, currentFrustum[0].m_Dist, 0.01f );

			for( int j = 1; j != iCurrentFrustumPlanes; ++j )
			{
				if( iVertCount < 3 )
					break; //nothing left in the frustum

				iVertCount = ClipPolyToPlane( pClipBuffers[j&1], iVertCount, pClipBuffers[(j&1)^1], currentFrustum[j].m_Normal, currentFrustum[j].m_Dist, 0.01f );
			}

			if( iVertCount >= 3 )
			{
				return true; //this polygon is visible, we're done
			}
		}
		
		return false; //none of the polygons are visible
	}
	else
	{
		//rely on flatbasic's technique since we're just using a quad
		return BaseClass::ShouldUpdatePortalView_BasedOnView( currentView, currentComplexFrustum );
	}
#endif
}

extern ConVar r_portal_use_complex_frustums;
bool C_Prop_Mirror::CalcFrustumThroughPortal( const Vector &ptCurrentViewOrigin, Frustum OutputFrustum )
{
	if( r_portal_use_complex_frustums.GetBool() == false )
		return false;

	if( m_vForward.Dot( ptCurrentViewOrigin ) <= m_vForward.Dot( m_ptOrigin ) )
		return false; //looking at portal backface

	Vector pTransformedVerts[10];
	const matrix3x4_t &matLocalToWorld = EntityToWorldTransform();
	for( int i = 0; i != m_LocalSpaceReflectionPolygonVertCount; ++i )
	{
		VectorTransform( &m_LocalSpaceReflectionPolygonVerts[i].x, matLocalToWorld, &pTransformedVerts[i].x );
	}

	return CalcFrustumThroughPolygon( pTransformedVerts, m_LocalSpaceReflectionPolygonVertCount, ptCurrentViewOrigin, OutputFrustum );
}

void C_Prop_Mirror::RenderPortalViewToBackBuffer( CViewRender *pViewRender, const CViewSetup &cameraView )
{
	VPROF( "C_Prop_Mirror::RenderPortalViewToBackBuffer" );

	Frustum FrustumBackup;
	memcpy( FrustumBackup, pViewRender->GetFrustum(), sizeof( Frustum ) );

	Frustum seeThroughFrustum;
	bool bUseSeeThroughFrustum;

	bUseSeeThroughFrustum = CalcFrustumThroughPortal( cameraView.origin, seeThroughFrustum );

	Vector vCameraForward;
	AngleVectors( cameraView.angles, &vCameraForward, NULL, NULL );

	// Setup fog state for the camera.
	Vector ptPOVOrigin = m_matrixThisToLinked * cameraView.origin;	
	Vector vPOVForward = m_matrixThisToLinked.ApplyRotation( vCameraForward );

	CViewSetup portalView = cameraView;

	if( portalView.zNear < 1.0f )
		portalView.zNear = 1.0f;

	QAngle qPOVAngles = TransformAnglesToWorldSpace( cameraView.angles, m_matrixThisToLinked.As3x4() );	

	portalView.origin = ptPOVOrigin;
	portalView.angles = qPOVAngles;
	
	VMatrix matCurrentView;
	if( cameraView.m_bCustomViewMatrix )
	{
		matCurrentView.CopyFrom3x4( cameraView.m_matCustomViewMatrix );
	}
	else
	{
		matCurrentView.Identity();

		//generate the view matrix for the existing position and angle, then wedge our mirror matrix onto it as a world transformation that prepends the view
		MatrixRotate( matCurrentView, Vector( 1, 0, 0 ), -cameraView.angles[2] );
		MatrixRotate( matCurrentView, Vector( 0, 1, 0 ), -cameraView.angles[0] );
		MatrixRotate( matCurrentView, Vector( 0, 0, 1 ), -cameraView.angles[1] );
		MatrixTranslate( matCurrentView, -cameraView.origin );
	}

	VMatrix matTemp = matCurrentView * m_matrixThisToLinked; //technically we should be using the inverse of m_matrixThisToLinked, but it's the same matrix!
	portalView.m_matCustomViewMatrix = matTemp.As3x4();
	portalView.m_bCustomViewMatrix = true;

	CopyToCurrentView( pViewRender, portalView );

	CMatRenderContextPtr pRenderContext( materials );

	pRenderContext->FlipCullMode(); //mirroring the world requires a flip in cull mode or we'll draw everything bass ackwards

	//if we look through multiple unique pairs of portals, we have to take care not to clip too much
	bool bReplaceOldPortalClipPlane = (g_pPortalRender->GetViewRecursionLevel() != 0) && (dynamic_cast<CPortalRenderable_FlatBasic *>(g_pPortalRender->GetCurrentViewExitPortal()) != NULL);

	{
		float fCustomClipPlane[4];
		fCustomClipPlane[0] = m_vForward.x;
		fCustomClipPlane[1] = m_vForward.y;
		fCustomClipPlane[2] = m_vForward.z;
		fCustomClipPlane[3] = m_vForward.Dot( m_ptOrigin );

		if( bReplaceOldPortalClipPlane )
			pRenderContext->PopCustomClipPlane(); //HACKHACK: We really only want to remove the clip plane of the current portal view. This assumes we're the only ones leaving clip planes on the stack

		pRenderContext->PushCustomClipPlane( fCustomClipPlane );
	}


	{
		ViewCustomVisibility_t customVisibility;
		m_pLinkedPortal->AddToVisAsExitPortal( &customVisibility );
		render->Push3DView( pRenderContext, portalView, 0, NULL, pViewRender->GetFrustum() );		
		{
			if( bUseSeeThroughFrustum)
				memcpy( pViewRender->GetFrustum(), seeThroughFrustum, sizeof( Frustum ) );

			render->OverrideViewFrustum( pViewRender->GetFrustum() );
			SetViewRecursionLevel( g_pPortalRender->GetViewRecursionLevel() + 1 );

			CPortalRenderable *pRenderingViewForPortalBackup = g_pPortalRender->GetCurrentViewEntryPortal();
			CPortalRenderable *pRenderingViewExitPortalBackup = g_pPortalRender->GetCurrentViewExitPortal();
			SetViewEntranceAndExitPortals( this, m_pLinkedPortal );

			//DRAW!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			ViewDrawScene_PortalStencil( pViewRender, portalView, &customVisibility );

			SetViewEntranceAndExitPortals( pRenderingViewForPortalBackup, pRenderingViewExitPortalBackup );

			if( m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration && (g_pPortalRender->GetRemainingPortalViewDepth() == 1) )
			{
				//save the view matrix for usage with the depth doubler. 
				//It's important that we do this AFTER using the depth doubler this frame to compensate for the fact that the front buffer is 1 frame behind the current view matrix
				//otherwise we get a lag effect when the player changes their viewing angles
				pRenderContext->GetMatrix( MATERIAL_VIEW, &m_InternallyMaintainedData.m_DepthDoublerTextureView[GET_ACTIVE_SPLITSCREEN_SLOT()] );
			}

			SetViewRecursionLevel( g_pPortalRender->GetViewRecursionLevel() - 1 );
		}
		render->PopView( pRenderContext, pViewRender->GetFrustum() );

		//restore old frustum
		memcpy( pViewRender->GetFrustum(), FrustumBackup, sizeof( Frustum ) );
		render->OverrideViewFrustum( FrustumBackup );
	}

	pRenderContext->FlipCullMode(); //flip the cull mode again to restore it to it's original setting

	pRenderContext->PopCustomClipPlane();

	if( bReplaceOldPortalClipPlane )
	{
		CPortalRenderable_FlatBasic *pCurrentExitPortal = (CPortalRenderable_FlatBasic *)g_pPortalRender->GetCurrentViewExitPortal();

		float fCustomClipPlane[4];
		fCustomClipPlane[0] = pCurrentExitPortal->m_vForward.x;
		fCustomClipPlane[1] = pCurrentExitPortal->m_vForward.y;
		fCustomClipPlane[2] = pCurrentExitPortal->m_vForward.z;
		fCustomClipPlane[3] = pCurrentExitPortal->m_vForward.Dot( pCurrentExitPortal->m_ptOrigin - (pCurrentExitPortal->m_vForward * 0.5f) ); //moving it back a smidge to eliminate visual artifacts for half-in objects

		pRenderContext->PushCustomClipPlane( fCustomClipPlane );
	}

	//restore old vis data
	CopyToCurrentView( pViewRender, cameraView );
}

int C_Prop_Mirror::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if( g_pPortalRender->GetRemainingPortalViewDepth() == 0 )
	{
		SetBodygroup( m_iMirrorSurfaceBodyGroup, 0 );
	}
		
	int iRetVal = BaseClass::DrawModel( flags, instance );

	if( g_pPortalRender->GetRemainingPortalViewDepth() == 0 )
	{
		SetBodygroup( m_iMirrorSurfaceBodyGroup, 1 );
	}

	return iRetVal;
}
