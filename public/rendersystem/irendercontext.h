//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IRENDERCONTEXT_H
#define IRENDERCONTEXT_H

#ifdef _WIN32
#pragma once
#endif

#include "rendersystem/irenderdevice.h"
#include "mathlib/vector4d.h"
#include "tier1/generichash.h"
#include "rendersystem/renderstate.h"
#include "mathlib/vmatrix.h"
#include "rendersystem/schema/renderable.g.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

enum RenderBufferStride_t
{
	RENDER_BUFFER_STRIDE_INVALID = -1,
};


//-----------------------------------------------------------------------------
// Index/vertex buffer lock info
//-----------------------------------------------------------------------------
struct LockDesc_t
{
	void *m_pMemory;
};


//-----------------------------------------------------------------------------
// Viewport structure
//-----------------------------------------------------------------------------
#define RENDER_VIEWPORT_VERSION 1
struct RenderViewport_t
{
	int m_nVersion;
	int m_nTopLeftX;
	int m_nTopLeftY;
	int m_nWidth;
	int m_nHeight;
	float m_flMinZ;
	float m_flMaxZ;

	RenderViewport_t() : m_nVersion( RENDER_VIEWPORT_VERSION ) {}

	void Init()
	{
		memset( this, 0, sizeof(RenderViewport_t) );
		m_nVersion = RENDER_VIEWPORT_VERSION;
	}

	void Init( int x, int y, int nWidth, int nHeight, float flMinZ = 0.0f, float flMaxZ = 1.0f )
	{
		m_nVersion = RENDER_VIEWPORT_VERSION;
		m_nTopLeftX = x; m_nTopLeftY = y; m_nWidth = nWidth; m_nHeight = nHeight;
		m_flMinZ = flMinZ;
		m_flMaxZ = flMaxZ;
	}
};


// clear flags
enum RenderClearFlags_t
{
	RENDER_CLEAR_FLAGS_CLEAR_DEPTH = 0x1,
	RENDER_CLEAR_FLAGS_CLEAR_STENCIL = 0x2
};


enum StandardTextureID_t
{
	STDTEXTURE_NORMAL_DEPTH_BUFFER = 0,
	STDTEXTURE_LIGHTPASS_OUTPUT,

	STDTEXTURE_NUMBER_OF_IDS
};


class CDisplayList											// subclasses in devices
{
public:
	CDisplayList *m_pNext;
};


//-----------------------------------------------------------------------------
// Data for the material system
//-----------------------------------------------------------------------------
#define RENDER_MATERIAL_NUM_WORLD_MATRICES 53
struct RenderMaterialData_t
{
	int nMode;

	float flTime;

	float mWorldArray[ RENDER_MATERIAL_NUM_WORLD_MATRICES ][3][4];
	VMatrix mView;
	VMatrix mProjection;
	VMatrix mVP;
	VMatrix mWVP;

	Vector vCameraPosition;
	Vector vCameraForwardVec;
	Vector vCameraUpVec;
	float flNearPlane;
	float flFarPlane;
	float vViewportSize[2];

	HRenderTexture customTextures[2];

	void Init()
	{
		nMode = 0;

		flTime = 0.0f;

		float m4x3Identity[3][4] = { 1.0f, 0.0f, 0.0f, 0.0f,
									 0.0f, 1.0f, 0.0f, 0.0f,
									 0.0f, 0.0f, 1.0f, 0.0f };

		for ( int i = 0; i < RENDER_MATERIAL_NUM_WORLD_MATRICES; i++ )
		{
			memcpy( mWorldArray[i], m4x3Identity, sizeof( m4x3Identity ) );
		}

		mView.Identity();
		mProjection.Identity();
		mVP.Identity();
		mWVP.Identity();

		vCameraPosition.Init( 0.0f, 0.0f, 0.0f );
		vCameraForwardVec.Init( 0.0f, 0.0f, 0.0f );
		vCameraUpVec.Init( 0.0f, 0.0f, 0.0f );
		flNearPlane = 0.0f;
		flFarPlane = 1.0f;
		vViewportSize[0] = 0.0f;
		vViewportSize[1] = 0.0f;

		customTextures[0] = RENDER_TEXTURE_HANDLE_INVALID;
		customTextures[1] = RENDER_TEXTURE_HANDLE_INVALID;
	}
};


//-----------------------------------------------------------------------------
// Render context interface
//-----------------------------------------------------------------------------
abstract_class IRenderContext
{
	// rendering functions. in flux
public:
	virtual void SetSwapChain( SwapChainHandle_t hSwapChain ) = 0;

	// Called when the context is handed to a thread.
	virtual void AttachToCurrentThread() = 0;

	virtual void Clear( const Vector4D &vecRGBAColor, int nFlags = 0 ) = 0; // any RENDER_CLEAR_FLAGS_CLEAR_xxx flags

	virtual void SetViewports( int nCount, const RenderViewport_t* pViewports, bool setImmediately = false ) = 0;
	virtual void GetViewport( RenderViewport_t *pViewport, int nViewport ) = 0;

	// Restriction: This can only be called as the first call after a
	// context is created, or the first call after a Submit.
	virtual void BindRenderTargets( RenderTargetBinding_t hRenderTargetBinding ) = 0;

	// set this to actually draw, and clear display list data
	virtual void Submit( void ) =0;

	// use this interface to finish a command list but not submit it until later. This call resets
	// the rendercontext
	virtual CDisplayList *DetachCommandList ( void ) =0;


	// add items to the next submitted batches. Doing a submit resets this
	virtual void DependsOn( CDependencyDescriptor *pDesc ) = 0;
	virtual void Satisfies( CDependencyDescriptor *pDesc ) = 0;

	// Creates/destroys vertex + index buffers
	// For CreateDynamicIndexBuffer, nMaxInstanceCount == 0 means we don't expect
	// the buffer to be used w/ instanced rendering. 
	// mNaxInstanceCount == INT_MAX means we have no idea how many instances will be rendered
	virtual HRenderBuffer CreateDynamicVertexBuffer( const BufferDesc_t& desc ) = 0;
	virtual void DestroyDynamicVertexBuffer( HRenderBuffer hVertexBuffer ) = 0;
	virtual HRenderBuffer CreateDynamicIndexBuffer( const BufferDesc_t& desc, int nMaxInstanceCount = 0 ) = 0;
	virtual void DestroyDynamicIndexBuffer( HRenderBuffer hIndexBuffer ) = 0;

	// Use this to read or write buffers, whether lock is for read or write
	// depends on the type of buffer (dynamic, staging, semistatic, etc)
	virtual bool LockIndexBuffer( HRenderBuffer hIndexBuffer, int nMaxSizeInBytes, LockDesc_t *pDesc ) = 0; 
	virtual void UnlockIndexBuffer( HRenderBuffer hIndexBuffer, int nWrittenSizeInBytes, LockDesc_t *pDesc ) = 0; 
	virtual bool LockVertexBuffer( HRenderBuffer hVertexBuffer, int nMaxSizeInBytes, LockDesc_t *pDesc ) = 0; 
	virtual void UnlockVertexBuffer( HRenderBuffer hVertexBuffer, int nWrittenSizeInBytes, LockDesc_t *pDesc ) = 0; 

	// Binds an vertex/index buffer
	virtual bool BindIndexBuffer( HRenderBuffer hIndexBuffer, int nOffset ) = 0;
	virtual bool BindVertexBuffer( int nSlot, HRenderBuffer hVertexBuffer, int nOffset, int nStride = RENDER_BUFFER_STRIDE_INVALID ) = 0;

	// Binds a vertex shader
	virtual void BindVertexShader( RenderShaderHandle_t hVertexShader, RenderInputLayout_t hInputLayout ) = 0;

	// Binds all other shader types
	virtual void BindShader( RenderShaderType_t nType, RenderShaderHandle_t hShader ) = 0;

	// textures
	virtual void BindTexture( int nSamplerIndex, HRenderTexture hTexture, RenderShaderType_t nTargetPipelineStage = RENDER_PIXEL_SHADER ) = 0;

	virtual void SetSamplerStatePS( int nSamplerIndex, RsFilter_t eFilterMode, RsTextureAddressMode_t eUWrapMode = RS_TEXTURE_ADDRESS_WRAP,
									RsTextureAddressMode_t eVWrapMode = RS_TEXTURE_ADDRESS_WRAP, RsTextureAddressMode_t eWWrapMode = RS_TEXTURE_ADDRESS_WRAP,
									RsComparison_t eTextureComparisonMode = RS_CMP_LESS ) = 0;

	virtual void SetSamplerStateVS( int nSamplerIndex, RsFilter_t eFilterMode, RsTextureAddressMode_t eUWrapMode = RS_TEXTURE_ADDRESS_WRAP,
									RsTextureAddressMode_t eVWrapMode = RS_TEXTURE_ADDRESS_WRAP, RsTextureAddressMode_t eWWrapMode = RS_TEXTURE_ADDRESS_WRAP,
									RsComparison_t eTextureComparisonMode = RS_CMP_LESS ) = 0;

	virtual void SetSamplerState( int32 nSamplerIndex, const CSamplerStateDesc *pSamplerDesc, RenderShaderType_t nTargetPipelineStage = RENDER_PIXEL_SHADER ) = 0;

	// download data into a texture. It is acceptable to pass NULL for pData, in which case a d3d
	// texture will be allocated and set up, but with unset bits (this is useful for getting a
	// render target into memory for instance).
	// NOTE: This doesn't work on file-backed textures
	virtual void SetTextureData( HRenderTexture hTexture, const TextureDesc_t *pDataDesc, const void *pData, int nDataSize, bool bIsPreTiled, int nSpecificMipLevelToSet = -1, Rect_t const *pSubRectToUpdate = NULL ) = 0;

	// Draws stuff!
	// If nMaxVertexCount == 0, then calculate max vertex count from the size of the vertex buffer
	virtual void Draw( RenderPrimitiveType_t type, int nFirstVertex, int nVertexCount ) = 0;
	virtual void DrawInstanced( RenderPrimitiveType_t type, int nFirstVertex, int nVertexCountPerInstance, int nInstanceCount ) = 0;
	virtual void DrawIndexed( RenderPrimitiveType_t type, int nFirstIndex, int nIndexCount, int nMaxVertexCount = 0 ) = 0;
	virtual void DrawIndexedInstanced( RenderPrimitiveType_t type, int nFirstIndex, int nIndexCountPerInstance, int nInstanceCount, int nMaxVertexCount = 0 ) = 0;

	// misc state setting
	virtual void SetCullMode( RenderCullMode_t eCullMode ) = 0;
	virtual void SetBlendMode( RenderBlendMode_t eBlendMode, float const *pBlendFactor = NULL ) = 0;
	virtual void SetZBufferMode( RenderZBufferMode_t eZBufferMode ) = 0;

	virtual RsRasterizerStateHandle_t FindOrCreateRasterizerState( const RsRasterizerStateDesc_t *pRsDesc ) = 0;
	virtual void SetRasterizerState( RsRasterizerStateHandle_t rasterizerState ) = 0;

	virtual RsDepthStencilStateHandle_t FindOrCreateDepthStencilState( const RsDepthStencilStateDesc_t *pRsDesc ) = 0;
	virtual void SetDepthStencilState( RsDepthStencilStateHandle_t rasterizerState, uint32 nStencilRef = 0 ) = 0;

	virtual RsBlendStateHandle_t FindOrCreateBlendState( const RsBlendStateDesc_t *pBlendDesc ) = 0;
	virtual void SetBlendState( RsBlendStateHandle_t blendState, float const *pBlendFactor = NULL, uint32 nSampleMask = 0xFFFFFFFF ) = 0;

	// set the data in a constant buffer
	virtual void SetConstantBufferData( ConstantBufferHandle_t hConstantBuffer, void const *pData, int nDataSize ) = 0;

	// bind constant buffers
	virtual void BindConstantBuffer( RenderShaderType_t nType, ConstantBufferHandle_t hConstantBuffer, int nSlot, int nRegisterBaseForDx9 ) = 0;

	// get access to per-frame pooled constant buffers.
	virtual ConstantBufferHandle_t GetDynamicConstantBuffer( int nSize, void const *pData = NULL ) =0;

	// Get the input layout associated with this renderbuffer.  Returns RENDER_INPUT_LAYOUT_INVALID if none is associated.
	virtual RenderInputLayout_t GetInputLayoutForVertexBuffer( HRenderBuffer hBuffer, InputLayoutVariation_t nVariation = INPUT_LAYOUT_VARIATION_DEFAULT ) = 0;

	// Blocks the thread until the render thread completely empties
	virtual void Flush() = 0;

	// Forces a device lost
	virtual void ForceDeviceLost() = 0;

	// Returns the device associated w/ the context
	virtual IRenderDevice *GetDevice() = 0;

	// PIX events for debugging/perf analysis
	virtual void BeginPixEvent( color32 c, const char *pEventName ) = 0;
	virtual void EndPixEvent( ) = 0;
	virtual void PixSetMarker( color32 c, const char *pEventName ) = 0;


	// "standard" texture support.
	virtual void BindStandardTexture( int nSamplerIndex, StandardTextureID_t nTextureID, RenderShaderType_t nTargetPipelineStage = RENDER_PIXEL_SHADER ) = 0;
	virtual void SetStandardTexture( StandardTextureID_t nTextureID, HRenderTexture hTexture ) =0;
	virtual HRenderTexture GetStandardTexture( StandardTextureID_t nTextureID ) =0;

	// Material system data
	virtual RenderMaterialData_t &GetMaterialData() = 0;

protected:
	// Don't allow delete calls on an IRenderContext
	virtual ~IRenderContext() {}
};


//-----------------------------------------------------------------------------
// simple helper class with all inlines
//-----------------------------------------------------------------------------
class CRenderContextPtr
{
public:										   
	CRenderContextPtr( IRenderDevice *pDevice, RenderTargetBinding_t hRenderTargetBinding = RENDER_TARGET_BINDING_INVALID );
	~CRenderContextPtr( void );
	IRenderContext *operator->( void ) const;
	operator IRenderContext*() const;
	void Release( );

protected:
	IRenderContext *m_pContext;
	IRenderDevice  *m_pDevice;
};

FORCEINLINE CRenderContextPtr::CRenderContextPtr( IRenderDevice *pDevice, RenderTargetBinding_t hRenderTargetBinding )
{
	m_pDevice = pDevice;
	m_pContext = pDevice->GetRenderContext( );
	m_pContext->BindRenderTargets( hRenderTargetBinding );
}

FORCEINLINE CRenderContextPtr::~CRenderContextPtr( void )
{
	Release();
}

FORCEINLINE void CRenderContextPtr::Release( )
{
	if ( m_pContext )
	{
		m_pContext->Submit( );
		m_pDevice->ReleaseRenderContext( m_pContext );
		m_pContext = NULL;
		m_pDevice = NULL;
	}
}

// delegate to context via -> override
FORCEINLINE IRenderContext *CRenderContextPtr::operator->( void ) const
{
	return m_pContext;
}

// Cast operator (to pass to other methods)
FORCEINLINE CRenderContextPtr::operator IRenderContext*() const
{
	return m_pContext;
}


//-----------------------------------------------------------------------------
// Pix measurement helper class
//-----------------------------------------------------------------------------
class CRenderPixEvent
{
public:
	CRenderPixEvent( IRenderContext *pRenderContext, color32 c, const char *pName )
	{
		m_pContext = pRenderContext;
		m_pContext->BeginPixEvent( c, pName );
	}

	~CRenderPixEvent()
	{
		Release();
	}

	void Release()
	{
		if ( m_pContext )
		{
			m_pContext->EndPixEvent();
			m_pContext = NULL;
		}
	}

private:
	IRenderContext *m_pContext;
};


#endif // IRENDERCONTEXT_H
