//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

using namespace SF::GFx;
using namespace SF::Render;


void ScaleformUIImpl::InitRendererImpl( void )
{
	if ( m_pShaderDeviceMgr )
	{
		m_pDeviceCallbacks = new DeviceCallbacks();
		m_pDeviceCallbacks->m_pScaleform = this;
		m_pShaderDeviceMgr->AddDeviceDependentObject( m_pDeviceCallbacks );
	}
	m_iScreenHeight = 768;
	m_iScreenWidth = 1024;
}

void ScaleformUIImpl::ShutdownRendererImpl( void )
{
	if ( m_pShaderDeviceMgr )
	{
		if ( m_pDeviceCallbacks )
		{
			m_pShaderDeviceMgr->RemoveDeviceDependentObject( m_pDeviceCallbacks );
			delete m_pDeviceCallbacks;
			m_pDeviceCallbacks = NULL;
		}
	}

#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( m_pD3D9Stateblock )
	{
		m_pD3D9Stateblock->Release();
		m_pD3D9Stateblock = NULL;
	}
#endif

	if ( m_pRenderHAL )
	{
		m_pRenderHAL->ShutdownHAL();
		m_pRenderer2D.Clear();
		m_pRenderHAL.Clear();
	}
}

void ScaleformUIImpl::SaveRenderingState( void )
{
#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( m_pD3D9Stateblock )
	{
		m_pD3D9Stateblock->Capture();
	}

	// SF does not touch SRGB settings so make sure they are consistent
	m_pDevice->GetRenderState( D3DRS_SRGBWRITEENABLE, &m_srgbRenderState );
	m_pDevice->SetRenderState( D3DRS_SRGBWRITEENABLE, FALSE );
	for ( int i = 0; i < 16; i++ )
	{
		m_pDevice->GetSamplerState( i, D3DSAMP_SRGBTEXTURE, &m_pSavedSrgbSamplerStates[i] );
		m_pDevice->SetSamplerState( i, D3DSAMP_SRGBTEXTURE, FALSE );
	}
#elif defined( DX_TO_GL_ABSTRACTION )
	if (m_pDevice)
	{
		m_pDevice->SaveGLState();
	}
#endif
}

void ScaleformUIImpl::RestoreRenderingState( void )
{
#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( m_pD3D9Stateblock )
	{
		m_pD3D9Stateblock->Apply();
	}

	// Restore srgb render and sampler states
	// (Looks like state block capture/apply didn't work, should investigate at a later state)
	m_pDevice->SetRenderState( D3DRS_SRGBWRITEENABLE, m_srgbRenderState );
	for ( int i = 0; i < 16; i++ )
	{
		m_pDevice->SetSamplerState( i, D3DSAMP_SRGBTEXTURE, m_pSavedSrgbSamplerStates[i] );
	}
#elif  defined( DX_TO_GL_ABSTRACTION )
	if (m_pDevice)
	{
		m_pDevice->RestoreGLState();
	}
#endif
}

void ScaleformUIImpl::SetRenderingDevice( IDirect3DDevice9 *pDevice, D3DPRESENT_PARAMETERS *pPresentParameters, HWND hWnd )
{
#if defined(DX_TO_GL_ABSTRACTION)
	if ( !m_pThreadCommandQueue )
	{
		m_pThreadCommandQueue = new CScaleFormThreadCommandQueue;
	}
#endif

	if ( !m_pRenderHAL )
	{
		// This is startup initialisation

		m_pDevice = pDevice;
#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
		m_pRenderHAL = *new D3D9::HAL();
		m_pRenderHAL->InitHAL( D3D9::HALInitParams( m_pDevice, *pPresentParameters, D3D9::HALConfig_NoSceneCalls ) );
#else
		m_pRenderHAL = *new GL::HAL( m_pThreadCommandQueue );
		m_pRenderHAL->InitHAL( GL::HALInitParams( GL::HALConfig_DisableBinaryShaders | GL::HALConfig_DisableShaderPipelines, SF::ThreadId(), SF::String(), true ) );

		if ( m_pThreadCommandQueue )
		{
			m_pThreadCommandQueue->pHAL = m_pRenderHAL;
		}
#endif

		Assert ( !m_pRenderer2D );

		m_pRenderer2D = *new Renderer2D( m_pRenderHAL.GetPtr() );

		if ( m_pThreadCommandQueue )
		{
			m_pThreadCommandQueue->pR2D = m_pRenderer2D;
		}

		InitFonts();

		SF::Ptr<CScaleformImageCreator> pImageCreator = *new CScaleformImageCreator( this, (TextureManager*)m_pRenderHAL->GetTextureManager() );
		m_pLoader->SetImageCreator( pImageCreator );
	}
	else
	{
#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
		m_pRenderHAL->RestoreAfterReset();
#else
		m_pRenderHAL->ResetContext();
#endif
	}
	
#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	pDevice->CreateStateBlock( D3DSBT_ALL, &m_pD3D9Stateblock );
#endif

#if defined( SF_ENABLE_IME ) && defined( SF_ENABLE_IME_WIN32 )
	if ( !m_pIMEManager )
	{
		m_pIMEManager = *new ScaleformeUIIMEManager( hWnd, m_pGameEventManager );
		if ( m_pIMEManager )
		{
			m_pIMEManager->Init( m_pLoader->GetLog(), static_cast<Scaleform::GFx::FileOpener*>(m_pLoader->GetFileOpener().GetPtr()), NULL, false );
			m_pIMEManager->SetIMEMoviePath( "ime.swf" );
 			m_pLoader->SetIMEManager( m_pIMEManager );
			m_pIMEManager->EnableIME( m_bIMEEnabled );
		}
	}
#else
	NOTE_UNUSED( hWnd );
#endif

	D3DVIEWPORT9 viewport;
	pDevice->GetViewport( &viewport );
	m_iScreenWidth = viewport.Width;
	m_iScreenHeight = viewport.Height;
    
	SetScreenSize( m_iScreenWidth, m_iScreenHeight );
}

void ScaleformUIImpl::NotifyRenderingDeviceLost( void )
{
#if defined( WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( !m_pRenderHAL )
		return;

	m_pRenderHAL->PrepareForReset();

	m_pD3D9Stateblock->Release();
	m_pD3D9Stateblock = NULL;
#endif
}
#if defined( _WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
void DumpMeshBufferSetStats( D3D9::MeshBufferSet *pMBSet )
{
	int totalMem = 0;

	for ( int i = 0; i < MC_NUM_BUFFER_BUCKETS; i++ )
	{
		D3D9::MeshBufferBucket *pBucket = &pMBSet->m_buckets[i];

		Msg("Bucket %2d: created = %3d, free = %3d, total = %6d bytes (%4d KB)\n", 
			i, pBucket->m_numCreated, pBucket->m_numFree,
			pBucket->m_totalMemUsed, pBucket->m_totalMemUsed / 1024 );

		totalMem += pBucket->m_totalMemUsed;
	}

	Msg( "Total = %d KB\n", totalMem/1024 );	

}
#endif

void ScaleformUIImpl::DumpMeshCacheStats( void )
{

#if defined( _WIN32 ) && !defined( DX_TO_GL_ABSTRACTION )
	if ( !m_pRenderHAL )
	{
		Msg( "No renderer, no HAL, no stats.\n" );
		return;	
	}

	D3D9::MeshCache *mc = &m_pRenderHAL->GetMeshCache();

	// Log stats
	Msg( "SF MeshBufferSet, VB mem summary\n" );
	DumpMeshBufferSetStats( &mc->VertexBuffers );

	Msg( "SF MeshBufferSet, IB mem summary\n" );
	DumpMeshBufferSetStats( &mc->IndexBuffers );

#endif
}

