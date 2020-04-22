//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IRENDERDEVICE_H
#define IRENDERDEVICE_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "appframework/iappsystem.h"
#include "bitmap/imageformat.h"
#include "tier1/utlbuffer.h"
#include "mathlib/vector4d.h"
#include "bitmap/colorformat.h"
#include "rendersystem/renderstate.h"
#include "resourcesystem/stronghandle.h"
#include "rendersystem/schema/texture.g.h"
#include "rendersystem/schema/renderbuffer.g.h"
#include "rendersystem/schema/renderable.g.h"
#include "tier0/platwindow.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class KeyValues;
struct RenderInputLayoutField_t;
class IRenderContext;
struct Rect_t;


//-----------------------------------------------------------------------------
// turn this on to get logging
//----------------------------------------------------------------------------
//#define LOG_COMMAND_BUFFER_EXECUTE

//-----------------------------------------------------------------------------
// Adapter info
//-----------------------------------------------------------------------------
enum
{
	RENDER_ADAPTER_NAME_LENGTH = 512
};

struct RenderAdapterInfo_t
{
	char m_pDriverName[RENDER_ADAPTER_NAME_LENGTH];
	unsigned int m_VendorID;
	unsigned int m_DeviceID;
	unsigned int m_SubSysID;
	unsigned int m_Revision;
	int m_nDXSupportLevel;			// This is the *preferred* dx support level
	int m_nMinDXSupportLevel;
	int m_nMaxDXSupportLevel;
	unsigned int m_nDriverVersionHigh;
	unsigned int m_nDriverVersionLow;
};


//-----------------------------------------------------------------------------
// Flags to be used with the CreateDevice call
//-----------------------------------------------------------------------------
enum RenderCreateDeviceFlags_t
{
	RENDER_CREATE_DEVICE_RESIZE_WINDOWS			= 0x1,
};


//-----------------------------------------------------------------------------
// Describes how to set the mode
//-----------------------------------------------------------------------------
#define RENDER_DISPLAY_MODE_VERSION 1

struct RenderDisplayMode_t
{
	RenderDisplayMode_t() { memset( this, 0, sizeof(RenderDisplayMode_t) ); m_nVersion = RENDER_DISPLAY_MODE_VERSION; }

	int m_nVersion;
	int m_nWidth;					// 0 when running windowed means use desktop resolution
	int m_nHeight;
	ImageFormat m_Format;			// use ImageFormats (ignored for windowed mode)
	int m_nRefreshRateNumerator;	// Refresh rate. Use 0 in numerator + denominator for a default setting.
	int m_nRefreshRateDenominator;	// Refresh rate = numerator / denominator.
};

	
//-----------------------------------------------------------------------------
// Describes how to set the device
//-----------------------------------------------------------------------------
#define RENDER_DEVICE_INFO_VERSION 1

struct RenderDeviceInfo_t
{
	RenderDeviceInfo_t() { memset( this, 0, sizeof(RenderDeviceInfo_t) ); m_nVersion = RENDER_DEVICE_INFO_VERSION; m_DisplayMode.m_nVersion = RENDER_DISPLAY_MODE_VERSION; }

	int m_nVersion;
	RenderDisplayMode_t m_DisplayMode;
	int m_nBackBufferCount;				// valid values are 1 or 2 [2 results in triple buffering]
	RenderMultisampleType_t m_nMultisampleType;	

	bool m_bFullscreen : 1;
	bool m_bUseStencil : 1;
	bool m_bWaitForVSync : 1;			// Would we not present until vsync?
	bool m_bScaleToOutputResolution : 1;// 360 ONLY: sets up hardware scaling
	bool m_bProgressive : 1;			// 360 ONLY: interlaced or progressive
	bool m_bUsingMultipleWindows : 1; 	// Forces D3DPresent to use _COPY instead
};

//-----------------------------------------------------------------------------
// Vertex field description
//-----------------------------------------------------------------------------
enum
{
	RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE = 32,
};

struct RenderInputLayoutField_t
{
	char m_pSemanticName[RENDER_INPUT_LAYOUT_FIELD_SEMANTIC_NAME_SIZE];
	int m_nSemanticIndex;
	ColorFormat_t m_Format;
	int m_nOffset;
	int m_nSlot;
	RenderSlotType_t m_nSlotType;
	int m_nInstanceStepRate;
};

#define DEFINE_PER_VERTEX_FIELD( _slot, _name, _index, _vertexformat, _field )	\
	{ _name, _index, ComputeColorFormat( &(((_vertexformat*)0)->_field) ), offsetof( _vertexformat, _field ), _slot, RENDER_SLOT_PER_VERTEX, 0 },

#define DEFINE_PER_INSTANCE_FIELD( _slot, _stepRate, _name, _index, _vertexformat, _field )	\
	{ _name, _index, ComputeColorFormat( &(((_vertexformat*)0)->_field) ), offsetof( _vertexformat, _field ), _slot, RENDER_SLOT_PER_INSTANCE, _stepRate }, 


//-----------------------------------------------------------------------------
// When we switch render target bindings, should we keep the contents?
//-----------------------------------------------------------------------------
enum RenderTargetBindingFlags_t
{
	DISCARD_CONTENTS		= 0x00,                    
	PRESERVE_COLOR0			= 0x01,	// corresponds to Render Target 0
	PRESERVE_COLOR1			= 0x02,	// corresponds to Render Target 1
	PRESERVE_COLOR2			= 0x04,
	PRESERVE_COLOR3			= 0x08,
	PRESERVE_DEPTHSTENCIL	= 0x10,
	PRESERVE_CONTENTS = PRESERVE_COLOR0 | PRESERVE_COLOR1 | PRESERVE_COLOR2 | PRESERVE_COLOR3 | PRESERVE_DEPTHSTENCIL,

	TILE_VERTICALLY			= 0x20,	// by default, we tile horizontally (splits are vertical)
};

//-----------------------------------------------------------------------------
// Variations on input layouts
// These are used to support various types of instancing
//-----------------------------------------------------------------------------
enum InputLayoutVariation_t
{
	INPUT_LAYOUT_VARIATION_DEFAULT = 0x00,
	INPUT_LAYOUT_VARIATION_STREAM1_MAT3X4,
	INPUT_LAYOUT_VARIATION_STREAM1_MAT4X4,

	INPUT_LAYOUT_VARIATION_MAX
};

//-----------------------------------------------------------------------------
// Handle to a vertex format
//-----------------------------------------------------------------------------
DECLARE_HANDLE_32BIT( RenderInputLayout_t );
#define RENDER_INPUT_LAYOUT_INVALID	( RenderInputLayout_t::MakeHandle( (uint32)~0 ) )


//-----------------------------------------------------------------------------
// Standard texture handles
//-----------------------------------------------------------------------------
#define RENDER_TEXTURE_DEFAULT_RENDER_TARGET	( (HRenderTexture)-1 )


//-----------------------------------------------------------------------------
// Handle to a render resource
//-----------------------------------------------------------------------------
DECLARE_POINTER_HANDLE( RenderResourceHandle_t );
#define RENDER_RESOURCE_HANDLE_INVALID	( (RenderResourceHandle_t)0 )


//-----------------------------------------------------------------------------
// Handle to a class that describes which render targets are in use 
//-----------------------------------------------------------------------------
DECLARE_DERIVED_POINTER_HANDLE( RenderTargetBinding_t, RenderResourceHandle_t );
#define RENDER_TARGET_BINDING_INVALID ( (RenderTargetBinding_t)0 )
#define RENDER_TARGET_BINDING_BACK_BUFFER ( (RenderTargetBinding_t)-1 )


//-----------------------------------------------------------------------------
// Handle to a vertex, pixel, geometry, etc. shader
//-----------------------------------------------------------------------------
DECLARE_DERIVED_POINTER_HANDLE( RenderShaderHandle_t, RenderResourceHandle_t );
#define RENDER_SHADER_HANDLE_INVALID	( (RenderShaderHandle_t)0 )

enum RenderShaderType_t
{
	RENDER_SHADER_TYPE_INVALID = -1,
	RENDER_PIXEL_SHADER = 0,
	RENDER_VERTEX_SHADER,
	RENDER_GEOMETRY_SHADER,

	RENDER_SHADER_TYPE_COUNT,
};

//-----------------------------------------------------------------------------
// Handle to a vertex buffer/indexbuffer
//-----------------------------------------------------------------------------
DECLARE_DERIVED_POINTER_HANDLE( VertexBufferHandle_t, RenderResourceHandle_t );
DECLARE_DERIVED_POINTER_HANDLE( IndexBufferHandle_t, RenderResourceHandle_t );

#define VERTEX_BUFFER_HANDLE_INVALID	( (VertexBufferHandle_t)0 )
#define INDEX_BUFFER_HANDLE_INVALID		( (IndexBufferHandle_t)0 )

//-----------------------------------------------------------------------------
// constant buffers
//-----------------------------------------------------------------------------
DECLARE_DERIVED_POINTER_HANDLE( ConstantBufferHandle_t, RenderResourceHandle_t );
#define CONSTANT_BUFFER_HANDLE_INVALID ( ( ConstantBufferHandle_t ) 0 )


//-----------------------------------------------------------------------------
// A shader buffer returns a block of memory which must be released when done with it
//-----------------------------------------------------------------------------
abstract_class IRenderShaderBuffer
{
public:
	virtual size_t GetSize() const = 0;
	virtual const void* GetBits() const = 0;
	virtual void Release() = 0;
};


//-----------------------------------------------------------------------------
// Swap chain handle
//-----------------------------------------------------------------------------
DECLARE_POINTER_HANDLE( SwapChainHandle_t );
#define SWAP_CHAIN_HANDLE_INVALID	( (SwapChainHandle_t)0 )


//-----------------------------------------------------------------------------
// Mode change callback
//-----------------------------------------------------------------------------
typedef void (*RenderModeChangeCallbackFunc_t)( void );


//-----------------------------------------------------------------------------
// This is a little wacky. It's super convenient for app systems to be
// able to use the render device in their Init() blocks. This is tricky 
// however because we don't have a great way of getting data to the 
// devicemgr so it knows how to create the device. Specifically, we need to
// tell it which adapter to use, whether we're going to have resizing windows,
// and what window we're going to have 3D on (this is a dx9 restriction).
// So, we need to be able to run application code right after renderdevice init
// but before any other inits. The other alternative is for the application
// to send information to the render device mgr post connect but pre init.
// The first option is better because it lets the application do arbitrary
// stuff after it can query information about adapters, etc.
//-----------------------------------------------------------------------------
abstract_class IRenderDeviceSetup
{
public:
	// This will be called by the render device mgr after it initializes itself
	virtual bool CreateRenderDevice() = 0;
};


//-----------------------------------------------------------------------------
// Methods related to discovering and selecting devices
//-----------------------------------------------------------------------------
abstract_class IRenderDeviceMgr : public IAppSystem
{
public:
	// Gets the number of adapters...
	virtual int	 GetAdapterCount() const = 0;

	// Returns info about each adapter
	virtual void GetAdapterInfo( int nAdapter, RenderAdapterInfo_t& info ) const = 0;

	// Gets recommended congifuration for a particular adapter at a particular dx level
	virtual bool GetRecommendedConfigurationInfo( int nAdapter, int nDXLevel, KeyValues *pConfiguration ) = 0;

	// Returns the number of modes
	virtual int	 GetModeCount( int nAdapter ) const = 0;

	// Returns mode information..
	virtual void GetModeInfo( RenderDisplayMode_t* pInfo, int nAdapter, int nMode ) const = 0;

	// Returns the current mode info for the requested adapter
	virtual void GetCurrentModeInfo( RenderDisplayMode_t* pInfo, int nAdapter ) const = 0;

	// Use the returned factory to get at an IRenderDevice and an IHardwareConfig
	// and any other interfaces we decide to create.
	// A returned factory of NULL indicates the device was not created properly.
	virtual CreateInterfaceFn CreateDevice( int nAdapter, int nFlags, int nDXLevel = 0 ) = 0;

	// Installs a callback to get called 
	virtual void AddModeChangeCallback( RenderModeChangeCallbackFunc_t func ) = 0;
	virtual void RemoveModeChangeCallback( RenderModeChangeCallbackFunc_t func ) = 0;

	virtual bool GetRecommendedVideoConfig( int nAdapter, KeyValues *pConfiguration ) = 0;

	// Destroys the device
	virtual void DestroyDevice() = 0;

	// Method to allow callbacks to set up the device during init
	// See big comment above IRenderDeviceSetup for why this is necessary
	virtual void InstallRenderDeviceSetup( IRenderDeviceSetup *pSetup ) = 0;
};


//-----------------------------------------------------------------------------
// Data for vertex/index buffer creation
//-----------------------------------------------------------------------------
struct BufferDesc_t
{
	int					m_nElementCount;		// Number of vertices/indices
	int					m_nElementSizeInBytes;	// Size of a single vertex/index
	const char *		m_pDebugName;			// Used to debug buffers
	const char *		m_pBudgetGroupName;
};


//-----------------------------------------------------------------------------
// Base class for abstract dependency class obtained and managed by the render device.  These MUST
// be gotten from the device in order to be used in submits. Application code only needs to treat
// these as pointers/handles
//-----------------------------------------------------------------------------
class CDependencyDescriptor;


enum RenderSystemAssetFileLoadMode_t // controls behavior when rendersystem assets are created from files
{
	LOADMODE_IMMEDIATE,										// asset is created and loaded from disk immediately
	LOADMODE_ASYNCHRONOUS,									// asset will start loading asynchronously
	LOADMODE_STREAMED,										// asset will be asynchronously loaded when referenced.
};

//-----------------------------------------------------------------------------
// Methods related to control of the device
//-----------------------------------------------------------------------------
abstract_class IRenderDevice
{
public:
	// Creates a 'swap chain' which represents a back buffer and a window.
	// When present happens, you must pass it a swap chain handle.
	virtual SwapChainHandle_t CreateSwapChain( PlatWindow_t hWnd, const RenderDeviceInfo_t &mode ) = 0;
	virtual void DestroySwapChain( SwapChainHandle_t hSwapChain ) = 0;

	virtual void UpdateSwapChain( SwapChainHandle_t hSwapChain, const RenderDeviceInfo_t &mode ) = 0; 
	virtual const RenderDeviceInfo_t &GetSwapChainInfo( SwapChainHandle_t hSwapChain ) const = 0;

	// Returns the window associated with a swap chain
	virtual PlatWindow_t GetSwapChainWindow( SwapChainHandle_t hSwapChain ) const = 0;

	// Releases/reloads resources when other apps want some memory
	virtual void ReleaseResources() = 0;
	virtual void ReacquireResources() = 0;

	// returns the backbuffer format and dimensions
	virtual ImageFormat GetBackBufferFormat( SwapChainHandle_t hSwapChain ) const = 0;
	virtual void GetBackBufferDimensions( SwapChainHandle_t hSwapChain, int *pWidth, int *pHeight ) const = 0;

	// Returns the current adapter in use
	virtual int GetCurrentAdapter() const = 0;

	// Are we using graphics?
	virtual bool IsUsingGraphics() const = 0;

	// Use this to spew information about the 3D layer 
	virtual void SpewDriverInfo() const = 0;

	// What's the bit depth of the stencil buffer?
	virtual int StencilBufferBits() const = 0;

	// Are we using a mode that uses MSAA
	virtual bool IsAAEnabled() const = 0;

	// Which version string should we use when compiling shaders
	virtual const char *GetShaderVersionString( RenderShaderType_t nType ) const = 0;

	// Does a page flip
	virtual void Present( ) = 0;

	// Gamma ramp control
	virtual void SetHardwareGammaRamp( float fGamma, float fGammaTVRangeMin, float fGammaTVRangeMax, float fGammaTVExponent, bool bTVEnabled ) = 0;

	// Shader compilation
	virtual IRenderShaderBuffer* CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion ) = 0;

	// Shader creation, destruction
	virtual RenderShaderHandle_t CreateShader( RenderShaderType_t nType, IRenderShaderBuffer* pShaderBuffer ) = 0;
	virtual void DestroyShader( RenderShaderType_t nType, RenderShaderHandle_t hShader ) = 0;

	// Defines input layouts
	virtual RenderInputLayout_t CreateInputLayout( const char *pLayoutName, int nFieldCount, const RenderInputLayoutField_t *pFieldDescs ) = 0;
	virtual void DestroyInputLayout( RenderInputLayout_t hInputLayout ) = 0;

	// Defines render target bind objects
	// NOTE: Use RENDER_TARGET_BINDING_BACK_BUFFER for the back buffer
	// Also note: If you need to re-use a buffer and render into it a second time,
	// create a new render target binding to the same buffer. That will help sorting
	virtual RenderTargetBinding_t CreateRenderTargetBinding( int nFlags, int nRenderTargetCount, const HRenderTexture *pRenderTargets, HRenderTexture hDepthStencilTexture ) = 0;
	RenderTargetBinding_t CreateRenderTargetBinding( int nFlags, HRenderTexture hRenderTarget, HRenderTexture hDepthStencilTexture );
	virtual void DestroyRenderTargetBinding( RenderTargetBinding_t hBinding ) = 0;

	// Utility methods to make shader creation simpler
	// NOTE: For the utlbuffer version, use a binary buffer for a compiled shader
	// and a text buffer for a source-code (.fxc) shader
	RenderShaderHandle_t CreateShader( RenderShaderType_t nType, const char *pProgram, size_t nBufLen, const char *pShaderVersion );
	RenderShaderHandle_t CreateShader( RenderShaderType_t nType, CUtlBuffer &buf, const char *pShaderVersion = NULL );
	RenderShaderHandle_t CreateShader( RenderShaderType_t nType, const void *pCompiledProgram, size_t nBufLen );

	// Creates render state objects
	virtual RsRasterizerStateHandle_t FindOrCreateRasterizerState( const RsRasterizerStateDesc_t *pRsDesc ) = 0;
	virtual RsDepthStencilStateHandle_t FindOrCreateDepthStencilState( const RsDepthStencilStateDesc_t *pDsDesc ) = 0;
	virtual RsBlendStateHandle_t FindOrCreateBlendState( const RsBlendStateDesc_t *pBlendDesc ) = 0;

	// Creates/destroys vertex + index buffers
	// For CreateIndexBuffer, nMaxInstanceCount == 0 means we don't expect
	// the buffer to be used w/ instanced rendering. 
	// mNaxInstanceCount == INT_MAX means we have no idea how many instances will be rendered
	virtual HRenderBuffer CreateRenderBuffer( const char *pGroupName, const char *pResourceName, const RenderBufferDesc_t *pDescriptor, size_t nDataSize ) = 0;
	virtual void DestroyRenderBuffer( HRenderBuffer hBuffer ) = 0;
	virtual HRenderBuffer CreateVertexBuffer( RenderBufferType_t nType, const BufferDesc_t& desc ) = 0;
	virtual void DestroyVertexBuffer( HRenderBuffer hVertexBuffer ) = 0;
	virtual HRenderBuffer CreateIndexBuffer( RenderBufferType_t nType, const BufferDesc_t& desc, int nMaxInstanceCount = 0 ) = 0;
	virtual void DestroyIndexBuffer( HRenderBuffer hIndexBuffer ) = 0;

	// Query buffer info
	virtual void GetVertexBufferDesc( HRenderBuffer hVertexBuffer, BufferDesc_t *pDesc ) = 0;
	virtual void GetIndexBufferDesc( HRenderBuffer hIndexBuffer, BufferDesc_t *pDesc ) = 0;

	// textures
	virtual HRenderTexture FindOrCreateTexture( const char *pGroupName, const char *pResourceName, const TextureHeader_t *pDescriptor ) = 0;

	// manage file-backed textures.
	virtual HRenderTexture FindOrCreateFileTexture( const char *pFileName, RenderSystemAssetFileLoadMode_t nLoadMode = LOADMODE_ASYNCHRONOUS ) = 0;
	virtual HRenderTexture FindFileTexture( ResourceId_t nId, RenderSystemAssetFileLoadMode_t nLoadMode = LOADMODE_ASYNCHRONOUS ) = 0;

	// Allows use of render contexts
	virtual void EnableRenderContexts( bool bEnable ) = 0;

	// render contexts
	virtual IRenderContext *GetRenderContext( ) = 0;
	virtual void ReleaseRenderContext( IRenderContext *pContext ) = 0;

	// submitting pre-built display lists
	virtual void SubmitDisplayList( class CDisplayList *pCommandList ) =0;
	

	// there is no release. These are automatically released when satisfied, and as far as the client is concerned, they are all gone after Present().
	virtual CDependencyDescriptor *GetDependencyDescriptor( int nNumBatchesWhichWillBeSubmitted = 1, char const *pDebugString = NULL ) =0;

	// create/destroy constant buffers
	virtual ConstantBufferHandle_t CreateConstantBuffer( size_t nNumBytes ) { return NULL; };
	virtual void DestroyConstantBuffer( ConstantBufferHandle_t hConstantBuffer ) {};

	// Forces a device lost
	virtual void ForceDeviceLost() = 0;

	// Reads the contents of a texture
	virtual void ReadTexturePixels( HRenderTexture hTexture, Rect_t *pSrcRect, Rect_t *pDstRect, void *pData, ImageFormat dstFormat, int nDstStride ) = 0;
};


//-----------------------------------------------------------------------------
// Helper wrapper for IRenderShaderBuffer for reading precompiled shader files
// NOTE: This is meant to be instanced on the stack; so don't call Release!
//-----------------------------------------------------------------------------
class CUtlRenderShaderBuffer : public IRenderShaderBuffer
{
public:
	CUtlRenderShaderBuffer( CUtlBuffer &buf ) : m_pBuf( &buf ) {}

	virtual size_t GetSize() const
	{
		return m_pBuf->TellMaxPut();
	}

	virtual const void* GetBits() const
	{
		return m_pBuf->Base();
	}

	virtual void Release()
	{
		Assert( 0 );
	}

private:
	CUtlBuffer *m_pBuf;
};


//-----------------------------------------------------------------------------
// Inline methods of IRenderDevice
//-----------------------------------------------------------------------------
inline RenderTargetBinding_t IRenderDevice::CreateRenderTargetBinding( int nFlags, HRenderTexture hRenderTarget, HRenderTexture hDepthStencilTexture )
{
	return CreateRenderTargetBinding( nFlags, 1, &hRenderTarget, hDepthStencilTexture );
}

inline RenderShaderHandle_t IRenderDevice::CreateShader( RenderShaderType_t nType, const char *pProgram, size_t nBufLen, const char *pShaderVersion )
{
	RenderShaderHandle_t hShader = RENDER_SHADER_HANDLE_INVALID;
	IRenderShaderBuffer* pShaderBuffer = CompileShader( pProgram, nBufLen, pShaderVersion );
	if ( pShaderBuffer )
	{
		hShader = CreateShader( nType, pShaderBuffer );
		pShaderBuffer->Release();
	}
	return hShader;
}

inline RenderShaderHandle_t IRenderDevice::CreateShader( RenderShaderType_t nType, CUtlBuffer &buf, const char *pShaderVersion )
{
	// NOTE: Text buffers are assumed to have source-code shader files
	// Binary buffers are assumed to have compiled shader files
	if ( buf.IsText() )
	{
		Assert( pShaderVersion );
		return CreateShader( nType, (const char *)buf.Base(), buf.TellMaxPut(), pShaderVersion );
	}

	CUtlRenderShaderBuffer shaderBuffer( buf );
	return CreateShader( nType, &shaderBuffer );
}

inline RenderShaderHandle_t IRenderDevice::CreateShader( RenderShaderType_t nType, const void *pCompiledProgram, size_t nBufLen )
{
	Assert( nBufLen == ( int )nBufLen ); // make sure we're not trimming 4Gb+ sizes
	CUtlBuffer tmpBuf( pCompiledProgram, ( int )nBufLen, CUtlBuffer::READ_ONLY );
	return CreateShader( nType, tmpBuf, NULL );
}


#endif // IRENDERDEVICE_H
