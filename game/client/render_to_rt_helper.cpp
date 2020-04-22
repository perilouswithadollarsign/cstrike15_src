//===== Copyright � 2013-2013, Valve Corporation, All rights reserved. ======//
//
// Purpose: Class to help with rendering CMDLs & CMergedMDLs to textures.
//
//===========================================================================//

#include "render_to_rt_helper.h"
#include "istudiorender.h"
#include "tier2/renderutils.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "renderparm.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// uncomment these to write out VTFs of the inventory icons with no lighting to be used for the "default icon images"
//#define WRITE_OUT_VTF
//#define ICON_VTF_BASE_PATH "d:/temp"

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CRenderToRTHelper s_RenderToRTHelper;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CRenderToRTHelper, IRenderToRTHelper, RENDER_TO_RT_HELPER_INTERFACE_VERSION, s_RenderToRTHelper );

CRenderToRTHelper::CRenderToRTHelper()
	: m_pCurrentObjectToRender( NULL )
	, m_pPixelsReadEvent( NULL )
	, m_pRenderTarget( NULL )
{
}

CRenderToRTHelper::~CRenderToRTHelper()
{
}

bool ProcessRenderToRTHelper()
{
	return ( g_pRenderToRTHelper != NULL ) ? g_pRenderToRTHelper->Process() : false;
}

bool CRenderToRTHelper::Init()
{
	if ( !g_pMaterialSystem )
		return false;

	memset( &m_LightingState, 0, sizeof(MaterialLightingState_t) );
	for ( int i = 0; i < 6; ++i )
	{
#ifdef WRITE_OUT_ICON_VTFS
		m_LightingState.m_vecAmbientCube[i].Init( 0.f, 0.f, 0.f );
#else
		m_LightingState.m_vecAmbientCube[i].Init( 0.15f, 0.15f, 0.15f );
#endif
	}

	//the default light is a blueish rim
	m_LightingState.m_pLocalLightDesc[0].InitDirectional( Vector( -1.0f, 0.3f, 1.0f ), Vector( 1.5f, 1.8f, 2.0f ) );
	m_LightingState.m_nLocalLightCount = 1;

	m_pRenderTarget = g_pMaterialSystem->FindTexture( "render_to_rt_helper", TEXTURE_GROUP_RENDER_TARGET );
	Assert( m_pRenderTarget );
	m_pRenderTarget->AddRef();

	g_pMaterialSystem->AddEndFramePriorToNextContextFunc( ::ProcessRenderToRTHelper );

	return true;
}


void CRenderToRTHelper::Shutdown( void )
{
	g_pMaterialSystem->RemoveEndFramePriorToNextContextFunc( ::ProcessRenderToRTHelper );

	if ( m_pRenderTarget )
	{
		m_pRenderTarget->Release();
		m_pRenderTarget = NULL;
	}
}


void CRenderToRTHelper::LookAt( Camera_t& camera, const Vector &vecCenter, float flRadius, QAngle cameraAngles, Vector cameraOffset )
{
	// Compute the distance to the camera for the object based on its
	// radius and fov.

	// clamp to a reasonable range
	flRadius = clamp(flRadius, 6.0f, 20.0f);

	// since tan( fov/2 ) = f/d
	// cos( fov/2 ) = r / r' where r = sphere radius, r' = perp distance from sphere center to max extent of camera
	// d/f = r'/d' where d' is distance of camera to sphere
	// d' = r' / tan( fov/2 ) * r' = r / ( cos (fov/2) * tan( fov/2 ) ) = r / sin( fov/2 )
	float flFOVx = camera.m_flFOVX;
	flFOVx *= M_PI / 360.0f;
	Vector vecCameraOffset( cameraOffset );
	vecCameraOffset.x -= ( flRadius / sin( flFOVx ) );

	// now setup the camera's origin and angles
	matrix3x4_t matCameraPivot;
	matrix3x4_t offset;
	matrix3x4_t worldToCamera;
	AngleMatrix( cameraAngles, vecCenter, matCameraPivot );
	SetIdentityMatrix( offset );
	MatrixSetColumn( vecCameraOffset, 3, offset );
	ConcatTransforms( matCameraPivot, offset, worldToCamera );
	MatrixAngles( worldToCamera, camera.m_angles, camera.m_origin );
}


RenderToRTData_t *CRenderToRTHelper::CreateRenderToRTData( IRenderToRTHelperObject *pObject, IVTFTexture *pResultVTF )
{
	RenderToRTData_t *pRenderToRTData = new RenderToRTData_t;
	pRenderToRTData->m_pObject = pObject;
	pRenderToRTData->m_pResultVTF = pResultVTF;
	pRenderToRTData->m_LightingState = m_LightingState;
	pRenderToRTData->m_cameraAngles = QAngle( 0.0f, -150.0f, 0.0f );
	pRenderToRTData->m_cameraOffset = Vector( 0.0f, 1.0f, -1.0f );
	pRenderToRTData->m_cameraFOV = 20.0f;
	pRenderToRTData->m_bUsingExplicitModelCameraPosAnglesFromAttachment = false;
	SetIdentityMatrix( pRenderToRTData->m_rootToWorld );
	pRenderToRTData->m_stage = RENDER_TO_RT_STAGE_CREATED;

	m_objectsToRender.AddToTail( pRenderToRTData );
	
	return pRenderToRTData;
}


void CRenderToRTHelper::DestroyRenderToRTData( RenderToRTData_t *pRenderToRTData )
{
	if ( pRenderToRTData->m_stage == RENDER_TO_RT_STAGE_WAITING_FOR_READ_BACK && m_pPixelsReadEvent )
	{
		m_pPixelsReadEvent->Wait();
		delete m_pPixelsReadEvent;
		m_pPixelsReadEvent = NULL;
	}

	if ( m_pCurrentObjectToRender == pRenderToRTData )
	{
		m_pCurrentObjectToRender = NULL;
	}

	pRenderToRTData->m_stage = RENDER_TO_RT_STAGE_UNDEFINED;

	m_objectsToRender.FindAndRemove( pRenderToRTData );
	delete pRenderToRTData;
}

void CRenderToRTHelper::StartRenderToRT( RenderToRTData_t *pRenderToRTData )
{
	if ( pRenderToRTData->m_stage == RENDER_TO_RT_STAGE_CREATED )
	{
		// update camera here, because m_cameraAngles may have been changed
		pRenderToRTData->m_Camera.Init( Vector( 0, 0, 0 ), QAngle( 0, 0, 0 ), 1.0f, 1000.0f, pRenderToRTData->m_cameraFOV, 1.0f );

		if ( !pRenderToRTData->m_bUsingExplicitModelCameraPosAnglesFromAttachment )
		{
			float flRadius;
			Vector vecCenter;
			pRenderToRTData->m_pObject->GetBoundingSphere( vecCenter, flRadius );
			LookAt( pRenderToRTData->m_Camera, vecCenter, flRadius, pRenderToRTData->m_cameraAngles, pRenderToRTData->m_cameraOffset );
		}
		else
		{
			pRenderToRTData->m_Camera.InitViewParameters( pRenderToRTData->m_cameraOffset, pRenderToRTData->m_cameraAngles );
		}

#ifdef WRITE_OUT_ICON_VTFS
		pRenderToRTData->m_LightingState.m_pLocalLightDesc[0].InitDirectional( Vector( 0.0f, 0.0f, 0.0f ), Vector( 0.0f, 0.0f, 0.0f ) );
		pRenderToRTData->m_LightingState.m_nLocalLightCount = 1;
#endif

		pRenderToRTData->m_stage = RENDER_TO_RT_STAGE_STARTED;
	}
}

bool CRenderToRTHelper::Process()
{
	bool bDidWork = false;

	if ( !m_pCurrentObjectToRender && m_objectsToRender.Count() > 0 )
	{
		int nIndex = m_objectsToRender.Head();
		while ( m_objectsToRender.IsValidIndex( nIndex ) )
		{
			if ( m_objectsToRender.Element( nIndex )->m_stage == RENDER_TO_RT_STAGE_STARTED )
			{
				m_pCurrentObjectToRender = m_objectsToRender.Element( nIndex );
				break;
			}
			nIndex = m_objectsToRender.Next( nIndex );
		}
	}

	if ( m_pCurrentObjectToRender )
	{	
		if ( m_pCurrentObjectToRender->m_stage == RENDER_TO_RT_STAGE_STARTED && m_pRenderTarget != NULL && materials->CanDownloadTextures() )
		{
			// render it and save

			m_pCurrentObjectToRender->m_pResultVTF->Init( m_pRenderTarget->GetActualWidth(), m_pRenderTarget->GetActualHeight(), m_pRenderTarget->GetActualDepth(), IMAGE_FORMAT_BGRA8888, 0, 1 );

			int resolutionX = m_pRenderTarget->GetActualWidth();
			int resolutionY = m_pRenderTarget->GetActualHeight();
			unsigned char *pDestImage = m_pCurrentObjectToRender->m_pResultVTF->ImageData( 0, 0, 0 );

			StudioRenderConfig_t oldStudioRenderConfig;
			g_pStudioRender->GetCurrentConfig( oldStudioRenderConfig );

			MaterialLock_t hLock = materials->Lock();

			//init render context and set the the custom weapon RT as the current target
			CMatRenderContextPtr pRenderContext( materials );
	
			pRenderContext->PushRenderTargetAndViewport( m_pRenderTarget, 0, 0, resolutionX, resolutionY );

			VMatrix view, projection;
			ComputeViewMatrix( &view, m_pCurrentObjectToRender->m_Camera );
			ComputeProjectionMatrix( &projection, m_pCurrentObjectToRender->m_Camera, resolutionX, resolutionY );

			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PushMatrix();
			pRenderContext->LoadIdentity( );

			pRenderContext->MatrixMode( MATERIAL_VIEW );
			pRenderContext->PushMatrix();
			pRenderContext->LoadMatrix( view );

			pRenderContext->MatrixMode( MATERIAL_PROJECTION );
			pRenderContext->PushMatrix();
			pRenderContext->LoadMatrix( projection );
		
			pRenderContext->SetLightingState( m_pCurrentObjectToRender->m_LightingState );

			pRenderContext->ClearColor4ub( 128, 128, 128, 0 ); 
			//pRenderContext->ClearBuffersObeyStencilEx( true, true, true );
			pRenderContext->ClearBuffers( true, true, true );

			// We want the models to use their natural alpha, not depth in alpha
			pRenderContext->SetIntRenderingParameter( INT_RENDERPARM_WRITE_DEPTH_TO_DESTALPHA, 0 );

			// flashlights can't work in the model panel under queued mode (the state isn't ready yet, so causes a crash)
			pRenderContext->SetFlashlightMode( false );

			pRenderContext->BindLocalCubemap( m_pCurrentObjectToRender->m_pObject->GetEnvCubeMap() );

			m_pCurrentObjectToRender->m_pObject->Draw( m_pCurrentObjectToRender->m_rootToWorld );

			pRenderContext->ReadPixels( 0, 0, resolutionX, resolutionY, pDestImage, IMAGE_FORMAT_RGBA8888, m_pRenderTarget );

			pRenderContext->PopRenderTargetAndViewport();

			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->PopMatrix();

			pRenderContext->MatrixMode( MATERIAL_VIEW );
			pRenderContext->PopMatrix();

			pRenderContext->MatrixMode( MATERIAL_PROJECTION );
			pRenderContext->PopMatrix();
			
			pRenderContext->Flush();
			materials->Unlock( hLock );

			g_pStudioRender->UpdateConfig( oldStudioRenderConfig );

			m_pCurrentObjectToRender->m_stage = RENDER_TO_RT_STAGE_DONE;

			bDidWork = true;

#ifdef WRITE_OUT_ICON_VTFS
			static int s_nIconNumber = 0;
			CUtlBuffer buf;
			m_pCurrentObjectToRender->m_pResultVTF->Serialize(buf);

			char szTextureName[MAX_PATH];
			V_snprintf(szTextureName, MAX_PATH, "%s/inventory_icon_%s.vtf", ICON_VTF_BASE_PATH, m_pCurrentObjectToRender->m_pszIconNameSuffix );
			
			FileHandle_t f = g_pFullFileSystem->Open(szTextureName, "wb", NULL);

			if ( f != FILESYSTEM_INVALID_HANDLE )
			{
				g_pFullFileSystem->Write( buf.Base(), buf.TellMaxPut(), f );
				g_pFullFileSystem->Close(f);
			}
#endif
			m_pCurrentObjectToRender = NULL;
		}
	}

	return bDidWork;
}
