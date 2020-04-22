//===== Copyright (c) Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef ISHADERAPI_H
#define ISHADERAPI_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector4d.h"
#include "shaderapi/ishaderdynamic.h"
#include "shaderapi/IShaderDevice.h"
#include "materialsystem/deformations.h"
#include "shaderlib/shadercombosemantics.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class IShaderUtil;
class IFileSystem;
class CPixelWriter;
class CMeshBuilder;
struct MaterialVideoMode_t;
class IMesh;
class IVertexBuffer;
class IIndexBuffer;
struct MeshDesc_t;
enum MaterialCullMode_t;
class IDataCache;   
struct MorphWeight_t;
struct MeshInstanceData_t;
#ifdef _X360
enum RTMultiSampleCount360_t;
#endif
struct ShaderComboInformation_t;

//-----------------------------------------------------------------------------
// This must match the definition in playback.cpp!
//-----------------------------------------------------------------------------
enum ShaderRenderTarget_t
{
	SHADER_RENDERTARGET_BACKBUFFER = -1,
	SHADER_RENDERTARGET_DEPTHBUFFER = -1,
	// GR - no RT, used to disable depth buffer
	SHADER_RENDERTARGET_NONE = -2,
};


//-----------------------------------------------------------------------------
// The state snapshot handle
//-----------------------------------------------------------------------------
typedef short StateSnapshot_t;


//-----------------------------------------------------------------------------
// The state snapshot handle
//-----------------------------------------------------------------------------
typedef int ShaderDLL_t;


//-----------------------------------------------------------------------------
// Texture creation
//-----------------------------------------------------------------------------
enum CreateTextureFlags_t
{
	TEXTURE_CREATE_CUBEMAP		       = 0x00001,
	TEXTURE_CREATE_RENDERTARGET        = 0x00002,
	TEXTURE_CREATE_MANAGED		       = 0x00004,
	TEXTURE_CREATE_DEPTHBUFFER	       = 0x00008,
	TEXTURE_CREATE_DYNAMIC		       = 0x00010,
	TEXTURE_CREATE_AUTOMIPMAP          = 0x00020,
	TEXTURE_CREATE_VERTEXTEXTURE       = 0x00040,
	TEXTURE_CREATE_CACHEABLE           = 0x00080,	// 360:		texture may be subject to streaming
	TEXTURE_CREATE_NOD3DMEMORY         = 0x00100,	// CONSOLE:	real allocation needs to occur later
	TEXTURE_CREATE_REDUCED             = 0x00200,	// CONSOLE:	true dimensions forced smaller (i.e. exclusion)
	TEXTURE_CREATE_EXCLUDED            = 0x00400,	// CONSOLE:	marked as excluded
	TEXTURE_CREATE_DEFAULT_POOL	       = 0x00800,
	TEXTURE_CREATE_UNFILTERABLE_OK     = 0x01000,
	TEXTURE_CREATE_CANCONVERTFORMAT    = 0x02000,	// 360:		allow format conversions at load
	TEXTURE_CREATE_PWLCORRECTED        = 0x04000,	// 360:		texture is pwl corrected
	TEXTURE_CREATE_ERROR               = 0x08000,	// CONSOLE:	texture was forced to checkerboard
	TEXTURE_CREATE_SYSMEM              = 0x10000,
	TEXTURE_CREATE_SRGB                = 0x20000,	// Posix/GL only, for textures which are SRGB-readable
	TEXTURE_CREATE_ANISOTROPIC		   = 0x40000,	// Posix/GL only, for textures which are flagged to use max aniso
	TEXTURE_CREATE_REUSEHANDLES		   = 0x80000,	// hint to re-use supplied texture handles
};

//-----------------------------------------------------------------------------
// Viewport structure
//-----------------------------------------------------------------------------
#define SHADER_VIEWPORT_VERSION 1
struct ShaderViewport_t
{
	int m_nVersion;
	int m_nTopLeftX;
	int m_nTopLeftY;
	int m_nWidth;
	int m_nHeight;
	float m_flMinZ;
	float m_flMaxZ;

	ShaderViewport_t() : m_nVersion( SHADER_VIEWPORT_VERSION ) {}

	void Init()
	{
		memset( this, 0, sizeof(ShaderViewport_t) );
		m_nVersion = SHADER_VIEWPORT_VERSION;
	}

	void Init( int x, int y, int nWidth, int nHeight, float flMinZ = 0.0f, float flMaxZ = 1.0f )
	{
		m_nVersion = SHADER_VIEWPORT_VERSION;
		m_nTopLeftX = x; m_nTopLeftY = y; m_nWidth = nWidth; m_nHeight = nHeight;
		m_flMinZ = flMinZ;
		m_flMaxZ = flMaxZ;
	}
};


//-----------------------------------------------------------------------------
// Fill modes
//-----------------------------------------------------------------------------
enum ShaderFillMode_t
{
	SHADER_FILL_SOLID = 0,
	SHADER_FILL_WIREFRAME,
};


//-----------------------------------------------------------------------------
// Rasterization state object
//-----------------------------------------------------------------------------
struct ShaderRasterState_t
{
	ShaderFillMode_t m_FillMode;
	MaterialCullMode_t m_CullMode;
	bool m_bCullEnable : 1;
	bool m_bDepthBias : 1;
	bool m_bScissorEnable : 1;
	bool m_bMultisampleEnable : 1;
};


//-----------------------------------------------------------------------------
// allowed stencil operations. These match the d3d operations
//-----------------------------------------------------------------------------
enum ShaderStencilOp_t 
{
#if !defined( _X360 )
	SHADER_STENCILOP_KEEP = 1,
	SHADER_STENCILOP_ZERO = 2,
	SHADER_STENCILOP_SET_TO_REFERENCE = 3,
	SHADER_STENCILOP_INCREMENT_CLAMP = 4,
	SHADER_STENCILOP_DECREMENT_CLAMP = 5,
	SHADER_STENCILOP_INVERT = 6,
	SHADER_STENCILOP_INCREMENT_WRAP = 7,
	SHADER_STENCILOP_DECREMENT_WRAP = 8,
#else
	SHADER_STENCILOP_KEEP = D3DSTENCILOP_KEEP,
	SHADER_STENCILOP_ZERO = D3DSTENCILOP_ZERO,
	SHADER_STENCILOP_SET_TO_REFERENCE = D3DSTENCILOP_REPLACE,
	SHADER_STENCILOP_INCREMENT_CLAMP = D3DSTENCILOP_INCRSAT,
	SHADER_STENCILOP_DECREMENT_CLAMP = D3DSTENCILOP_DECRSAT,
	SHADER_STENCILOP_INVERT = D3DSTENCILOP_INVERT,
	SHADER_STENCILOP_INCREMENT_WRAP = D3DSTENCILOP_INCR,
	SHADER_STENCILOP_DECREMENT_WRAP = D3DSTENCILOP_DECR,
#endif
	SHADER_STENCILOP_FORCE_DWORD = 0x7fffffff
};

enum ShaderStencilFunc_t 
{
#if !defined( _X360 )
	SHADER_STENCILFUNC_NEVER = 1,
	SHADER_STENCILFUNC_LESS = 2,
	SHADER_STENCILFUNC_EQUAL = 3,
	SHADER_STENCILFUNC_LEQUAL = 4,
	SHADER_STENCILFUNC_GREATER = 5,
	SHADER_STENCILFUNC_NOTEQUAL = 6,
	SHADER_STENCILFUNC_GEQUAL = 7,
	SHADER_STENCILFUNC_ALWAYS = 8,
#else
	SHADER_STENCILFUNC_NEVER = D3DCMP_NEVER,
	SHADER_STENCILFUNC_LESS = D3DCMP_LESS,
	SHADER_STENCILFUNC_EQUAL = D3DCMP_EQUAL,
	SHADER_STENCILFUNC_LEQUAL = D3DCMP_LESSEQUAL,
	SHADER_STENCILFUNC_GREATER = D3DCMP_GREATER,
	SHADER_STENCILFUNC_NOTEQUAL = D3DCMP_NOTEQUAL,
	SHADER_STENCILFUNC_GEQUAL = D3DCMP_GREATEREQUAL,
	SHADER_STENCILFUNC_ALWAYS = D3DCMP_ALWAYS,
#endif

	SHADER_STENCILFUNC_FORCE_DWORD = 0x7fffffff
};

#if defined( _X360 )
enum ShaderHiStencilFunc_t 
{
	SHADER_HI_STENCILFUNC_EQUAL = D3DHSCMP_EQUAL,
	SHADER_HI_STENCILFUNC_NOTEQUAL = D3DHSCMP_NOTEQUAL,

	SHADER_HI_STENCILFUNC_FORCE_DWORD = 0x7fffffff
};
#endif

//-----------------------------------------------------------------------------
// Stencil state
//-----------------------------------------------------------------------------
struct ShaderStencilState_t
{
	bool m_bEnable;
	ShaderStencilOp_t m_FailOp;
	ShaderStencilOp_t m_ZFailOp;
	ShaderStencilOp_t m_PassOp;
	ShaderStencilFunc_t m_CompareFunc;
	int m_nReferenceValue;
	uint32 m_nTestMask;
	uint32 m_nWriteMask;

#if defined( _X360 )
	bool m_bHiStencilEnable;
	bool m_bHiStencilWriteEnable;
	ShaderHiStencilFunc_t m_HiStencilCompareFunc;
	int m_nHiStencilReferenceValue;
#endif

	ShaderStencilState_t()
	{
		m_bEnable = false;
		m_PassOp = m_FailOp = m_ZFailOp = SHADER_STENCILOP_KEEP;
		m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
		m_nReferenceValue = 0;
		m_nTestMask = m_nWriteMask = 0xFFFFFFFF;

#if defined( _X360 )
		m_bHiStencilEnable = false;
		m_bHiStencilWriteEnable = false;
		m_HiStencilCompareFunc = SHADER_HI_STENCILFUNC_EQUAL;
		m_nHiStencilReferenceValue = 0;
#endif
	}
};


//-----------------------------------------------------------------------------
// Used for occlusion queries
//-----------------------------------------------------------------------------
DECLARE_POINTER_HANDLE( ShaderAPIOcclusionQuery_t );
#define INVALID_SHADERAPI_OCCLUSION_QUERY_HANDLE ( (ShaderAPIOcclusionQuery_t)0 )

enum ShaderAPIOcclusionQueryResult_t
{
	OCCLUSION_QUERY_RESULT_PENDING	=	-1,
	OCCLUSION_QUERY_RESULT_ERROR	=	-2
};
#define OCCLUSION_QUERY_FINISHED( iQueryResult ) ( ( iQueryResult ) != OCCLUSION_QUERY_RESULT_PENDING )


//-----------------------------------------------------------------------------
// Used on the 360 to create meshes from data in write-combined memory
//-----------------------------------------------------------------------------
struct ExternalMeshInfo_t
{
	IMaterial *m_pMaterial;
	VertexFormat_t m_VertexFormat;
	bool m_bFlexMesh;
	IMesh* m_pVertexOverride;
	IMesh* m_pIndexOverride;
};

struct ExternalMeshData_t
{
	uint8 *m_pVertexData;
	int m_nVertexCount;
	int m_nVertexSizeInBytes;
	uint16 *m_pIndexData;
	int m_nIndexCount;
};


//-----------------------------------------------------------------------------
// This is what the material system gets to see.
//-----------------------------------------------------------------------------
#define SHADERAPI_INTERFACE_VERSION		"ShaderApi029"
abstract_class IShaderAPI : public IShaderDynamicAPI
{
public:
	//
	// NOTE: These methods have been ported to DX10
	//

	// Viewport methods
	virtual void SetViewports( int nCount, const ShaderViewport_t* pViewports, bool setImmediately = false ) = 0;
	virtual int GetViewports( ShaderViewport_t* pViewports, int nMax ) const = 0;

	// Buffer clearing
	virtual void ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight ) = 0;
	virtual void ClearColor3ub( unsigned char r, unsigned char g, unsigned char b ) = 0;
	virtual void ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a ) = 0;

	// Methods related to binding shaders
	virtual void BindVertexShader( VertexShaderHandle_t hVertexShader ) = 0;
	virtual void BindGeometryShader( GeometryShaderHandle_t hGeometryShader ) = 0;
	virtual void BindPixelShader( PixelShaderHandle_t hPixelShader ) = 0;

	// Methods related to state objects
	virtual void SetRasterState( const ShaderRasterState_t& state ) = 0;

	//
	// NOTE: These methods have not yet been ported to DX10
	//

	// Sets the mode...
	virtual bool SetMode( void* hwnd, int nAdapter, const ShaderDeviceInfo_t &info ) = 0;

	virtual void ChangeVideoMode( const ShaderDeviceInfo_t &info ) = 0;

	// Returns the snapshot id for the shader state
	virtual StateSnapshot_t	 TakeSnapshot( ) = 0;

	virtual void TexMinFilter( ShaderTexFilterMode_t texFilterMode ) = 0;
	virtual void TexMagFilter( ShaderTexFilterMode_t texFilterMode ) = 0;
	virtual void TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode ) = 0;

	virtual void CopyRenderTargetToTexture( ShaderAPITextureHandle_t textureHandle ) = 0;

	// Binds a particular material to render with
	virtual void Bind( IMaterial* pMaterial ) = 0;

	// Gets the dynamic mesh; note that you've got to render the mesh
	// before calling this function a second time. Clients should *not*
	// call DestroyStaticMesh on the mesh returned by this call.
	virtual IMesh* GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool bBuffered = true,
		IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0) = 0;
	virtual IMesh* GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, 
		bool bBuffered = true, IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0 ) = 0;

	// Methods to ask about particular state snapshots
	virtual bool IsTranslucent( StateSnapshot_t id ) const = 0;
	virtual bool IsAlphaTested( StateSnapshot_t id ) const = 0;
	virtual bool UsesVertexAndPixelShaders( StateSnapshot_t id ) const = 0;
	virtual bool IsDepthWriteEnabled( StateSnapshot_t id ) const = 0;

	// Gets the vertex format for a set of snapshot ids
	virtual VertexFormat_t ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds ) const = 0;

	// What fields in the vertex do we actually use?
	virtual VertexFormat_t ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds ) const = 0;

	// Begins a rendering pass
	virtual void BeginPass( StateSnapshot_t snapshot ) = 0;

	// Renders a single pass of a material
	virtual void RenderPass( const unsigned char *pInstanceCommandBuffer, int nPass, int nPassCount ) = 0;

	// Set the number of bone weights
	virtual void SetNumBoneWeights( int numBones ) = 0;

	// Sets the lights
	virtual void SetLights( int nCount, const LightDesc_t *pDesc ) = 0;

	// Lighting origin for the current model
	virtual void SetLightingOrigin( Vector vLightingOrigin ) = 0;

	virtual void SetLightingState( const MaterialLightingState_t& state ) = 0;
	virtual void SetAmbientLightCube( Vector4D cube[6] ) = 0;

	// The shade mode
	virtual void ShadeMode( ShaderShadeMode_t mode ) = 0;

	// The cull mode
	virtual void CullMode( MaterialCullMode_t cullMode ) = 0;
	virtual void FlipCullMode( void ) = 0; //CW->CCW or CCW->CW, intended for mirror support where the view matrix is flipped horizontally

	virtual void BeginGeneratingCSMs()= 0;
	virtual void EndGeneratingCSMs() = 0;
	virtual void PerpareForCascadeDraw( int cascade, float fShadowSlopeScaleDepthBias, float fShadowDepthBias ) = 0;

	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	virtual void ForceDepthFuncEquals( bool bEnable ) = 0;

	// Forces Z buffering to be on or off
	virtual void OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable = true ) = 0;

	virtual void SetHeightClipZ( float z ) = 0; 
	virtual void SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode ) = 0; 

	virtual void SetClipPlane( int index, const float *pPlane ) = 0;
	virtual void EnableClipPlane( int index, bool bEnable ) = 0;
	
	// Returns the nearest supported format
	virtual ImageFormat GetNearestSupportedFormat( ImageFormat fmt, bool bFilteringRequired = true ) const = 0;
	virtual ImageFormat GetNearestRenderTargetFormat( ImageFormat fmt ) const = 0;

	// When AA is enabled, render targets are not AA and require a separate
	// depth buffer.
	virtual bool DoRenderTargetsNeedSeparateDepthBuffer() const = 0;

	// Texture management methods
	// For CreateTexture also see CreateTextures below
	virtual ShaderAPITextureHandle_t CreateTexture( 
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int flags, 
		const char *pDebugName,
		const char *pTextureGroupName ) = 0;

	virtual void DeleteTexture( ShaderAPITextureHandle_t textureHandle ) = 0;

	virtual ShaderAPITextureHandle_t CreateDepthTexture( 
		ImageFormat renderTargetFormat, 
		int width, 
		int height, 
		const char *pDebugName,
		bool bTexture,
		bool bAliasDepthSurfaceOverColorX360 = false ) = 0;

	virtual bool IsTexture( ShaderAPITextureHandle_t textureHandle ) = 0;
	virtual bool IsTextureResident( ShaderAPITextureHandle_t textureHandle ) = 0;

	// Indicates we're going to be modifying this texture
	// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
	// all use the texture specified by this function.
	virtual void ModifyTexture( ShaderAPITextureHandle_t textureHandle ) = 0;

	virtual void TexImage2D(
		int level,
		int cubeFaceID,
		ImageFormat dstFormat,
		int zOffset,
		int width,
		int height,
		ImageFormat srcFormat, 
		bool bSrcIsTiled,		// NOTE: for X360 only
		void *imageData ) = 0;

	virtual void TexSubImage2D( 
		int level, 
		int cubeFaceID, 
		int xOffset, 
		int yOffset, 
		int zOffset, 
		int width, 
		int height,
		ImageFormat srcFormat, 
		int srcStride, 
		bool bSrcIsTiled,		// NOTE: for X360 only
		void *imageData ) = 0;
	
	// An alternate (and faster) way of writing image data
	// (locks the current Modify Texture). Use the pixel writer to write the data
	// after Lock is called
	// Doesn't work for compressed textures 
	virtual bool TexLock( int level, int cubeFaceID, int xOffset, int yOffset, 
		int width, int height, CPixelWriter& writer ) = 0;
	virtual void TexUnlock( ) = 0;

	// Copy sysmem surface to default pool surface asynchronously
	virtual void UpdateTexture( int xOffset, int yOffset, int w, int h, ShaderAPITextureHandle_t hDstTexture, ShaderAPITextureHandle_t hSrcTexture ) = 0;
	virtual void *LockTex( ShaderAPITextureHandle_t hTexture ) = 0;
	virtual void UnlockTex( ShaderAPITextureHandle_t hTexture ) = 0;

	// These are bound to the texture
	virtual void TexSetPriority( int priority ) = 0;

	// Sets the texture state
	virtual void BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle ) = 0;

	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	// Set to SHADER_RENDERTARGET_DEPTHBUFFER if you want to use the regular z buffer.
	virtual void SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, 
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER ) = 0;
	
	// stuff that isn't to be used from within a shader
	virtual void ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth ) = 0;
	virtual void ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = NULL ) = 0;
	virtual void ReadPixelsAsync( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, ITexture *pRenderTargetTexture = NULL, CThreadEvent *pPixelsReadEvent = NULL ) = 0;
	virtual void ReadPixelsAsyncGetResult( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat, CThreadEvent *pGetResultEvent = NULL ) = 0;
	virtual void ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride ) = 0;

	virtual void FlushHardware() = 0;

	// Use this to begin and end the frame
	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;

	// Selection mode methods
	virtual int  SelectionMode( bool selectionMode ) = 0;
	virtual void SelectionBuffer( unsigned int* pBuffer, int size ) = 0;
	virtual void ClearSelectionNames( ) = 0;
	virtual void LoadSelectionName( int name ) = 0;
	virtual void PushSelectionName( int name ) = 0;
	virtual void PopSelectionName() = 0;

	// Force the hardware to finish whatever it's doing
	virtual void ForceHardwareSync() = 0;

	// Used to clear the transition table when we know it's become invalid.
	virtual void ClearSnapshots() = 0;

	virtual void FogStart( float fStart ) = 0;
	virtual void FogEnd( float fEnd ) = 0;
	virtual void SetFogZ( float fogZ ) = 0;	
	// Scene fog state.
	virtual void SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b ) = 0;
	virtual void GetSceneFogColor( unsigned char *rgb ) = 0;
	virtual void SceneFogMode( MaterialFogMode_t fogMode ) = 0;

	// Can we download textures?
	virtual bool CanDownloadTextures() const = 0;

	virtual void ResetRenderState( bool bFullReset = true ) = 0;

	// We use smaller dynamic VBs during level transitions, to free up memory
	virtual int  GetCurrentDynamicVBSize( void ) = 0;
	virtual void DestroyVertexBuffers( bool bExitingLevel = false ) = 0;

	virtual void EvictManagedResources() = 0;

	// Get stats on GPU memory usage
	virtual void GetGPUMemoryStats( GPUMemoryStats &stats ) = 0;

	// Level of anisotropic filtering
	virtual void SetAnisotropicLevel( int nAnisotropyLevel ) = 0;

	// For debugging and building recording files. This will stuff a token into the recording file,
	// then someone doing a playback can watch for the token.
	virtual void SyncToken( const char *pToken ) = 0;

	// Setup standard vertex shader constants (that don't change)
	// This needs to be called anytime that overbright changes.
	virtual void SetStandardVertexShaderConstants( float fOverbright ) = 0;

	//
	// Occlusion query support
	//
	
	// Allocate and delete query objects.
	virtual ShaderAPIOcclusionQuery_t CreateOcclusionQueryObject( void ) = 0;
	virtual void DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t ) = 0;

	// Bracket drawing with begin and end so that we can get counts next frame.
	virtual void BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t ) = 0;
	virtual void EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t ) = 0;

	// OcclusionQuery_GetNumPixelsRendered
	//	Get the number of pixels rendered between begin and end on an earlier frame.
	//	Calling this in the same frame is a huge perf hit!
	// Returns iQueryResult:
	//	iQueryResult >= 0					-	iQueryResult is the number of pixels rendered
	//	OCCLUSION_QUERY_RESULT_PENDING		-	query results are not available yet
	//	OCCLUSION_QUERY_RESULT_ERROR		-	query failed
	// Use OCCLUSION_QUERY_FINISHED( iQueryResult ) to test if query finished.
	virtual int OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t hQuery, bool bFlush = false ) = 0;

	virtual void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture ) = 0;
			
	virtual bool IsCascadedShadowMapping() const = 0;
	virtual void SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas ) = 0;
	virtual const CascadedShadowMappingState_t &GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale = false ) const = 0;

	virtual void ClearVertexAndPixelShaderRefCounts() = 0;
	virtual void PurgeUnusedVertexAndPixelShaders() = 0;

	// Called when the dx support level has changed
	virtual void DXSupportLevelChanged( int nDXLevel ) = 0;

	// By default, the material system applies the VIEW and PROJECTION matrices	to the user clip
	// planes (which are specified in world space) to generate projection-space user clip planes
	// Occasionally (for the particle system in hl2, for example), we want to override that
	// behavior and explictly specify a View transform for user clip planes. The PROJECTION
	// will be mutliplied against this instead of the normal VIEW matrix.
	virtual void EnableUserClipTransformOverride( bool bEnable ) = 0;
	virtual void UserClipTransform( const VMatrix &worldToView ) = 0;

	// ----------------------------------------------------------------------------------
	// Everything after this point added after HL2 shipped.
	// ----------------------------------------------------------------------------------

	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	// Set to SHADER_RENDERTARGET_DEPTHBUFFER if you want to use the regular z buffer.
	virtual void SetRenderTargetEx( int nRenderTargetID, 
		ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, 
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER ) = 0;

	virtual void CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) = 0;
	virtual void CopyTextureToRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) = 0;

	// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
	virtual void HandleDeviceLost() = 0;

	virtual void EnableLinearColorSpaceFrameBuffer( bool bEnable ) = 0;

	// Lets the shader know about the full-screen texture so it can 
	virtual void SetFullScreenTextureHandle( ShaderAPITextureHandle_t h ) = 0;

	// Rendering parameters control special drawing modes withing the material system, shader
	// system, shaders, and engine. renderparm.h has their definitions.
	virtual void SetFloatRenderingParameter(int parm_number, float value) = 0;
	virtual void SetIntRenderingParameter(int parm_number, int value) = 0;
	virtual void SetVectorRenderingParameter(int parm_number, Vector const &value) = 0;

	virtual float GetFloatRenderingParameter(int parm_number) const = 0;
	virtual int GetIntRenderingParameter(int parm_number) const = 0;
	virtual Vector GetVectorRenderingParameter(int parm_number) const = 0;

	virtual void SetFastClipPlane( const float *pPlane ) = 0;
	virtual void EnableFastClip( bool bEnable ) = 0;

	// Returns the number of vertices + indices we can render using the dynamic mesh
	// Passing true in the second parameter will return the max # of vertices + indices
	// we can use before a flush is provoked and may return different values 
	// if called multiple times in succession. 
	// Passing false into the second parameter will return
	// the maximum possible vertices + indices that can be rendered in a single batch
	virtual void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices ) = 0;

	// Returns the max number of vertices we can render for a given material
	virtual int GetMaxVerticesToRender( IMaterial *pMaterial ) = 0;
	virtual int GetMaxIndicesToRender( ) = 0;

	// stencil methods
	virtual void SetStencilState( const ShaderStencilState_t& state ) = 0;
	virtual void ClearStencilBufferRectangle(int xmin, int ymin, int xmax, int ymax, int value) = 0;

	// disables all local lights
	virtual void DisableAllLocalLights() = 0;
	virtual int CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 ) = 0;

	virtual IMesh *GetFlexMesh() = 0;

	virtual void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture ) = 0;

	virtual bool SupportsMSAAMode( int nMSAAMode ) = 0;

#if defined( _GAMECONSOLE )
	virtual bool PostQueuedTexture( const void *pData, int nSize, ShaderAPITextureHandle_t *pHandles, int nHandles, int nWidth, int nHeight, int nDepth, int nMips, int *pRefCount ) = 0;
#endif // _GAMECONSOLE

#if defined( _X360 )
	virtual HXUIFONT OpenTrueTypeFont( const char *pFontname, int tall, int style ) = 0;
	virtual void CloseTrueTypeFont( HXUIFONT hFont ) = 0;
	virtual bool GetTrueTypeFontMetrics( HXUIFONT hFont, wchar_t wchFirst, wchar_t wchLast, XUIFontMetrics *pFontMetrics, XUICharMetrics *pCharMetrics ) = 0;
	// Render a sequence of characters and extract the data into a buffer
	// For each character, provide the width+height of the font texture subrect,
	// an offset to apply when rendering the glyph, and an offset into a buffer to receive the RGBA data
	virtual bool GetTrueTypeGlyphs( HXUIFONT hFont, int numChars, wchar_t *pWch, int *pOffsetX, int *pOffsetY, int *pWidth, int *pHeight, unsigned char *pRGBA, int *pRGBAOffset ) = 0;
	virtual ShaderAPITextureHandle_t CreateRenderTargetSurface( int width, int height, ImageFormat format, RTMultiSampleCount360_t multiSampleCount, const char *pDebugName, const char *pTextureGroupName ) = 0;
	virtual void PersistDisplay() = 0;
	virtual void *GetD3DDevice() = 0;

	virtual void PushVertexShaderGPRAllocation( int iVertexShaderCount = 64 ) = 0;
	virtual void PopVertexShaderGPRAllocation( void ) = 0;

	// 360 allows us to bypass vsync blocking up to 60 fps without creating a new device
	virtual void EnableVSync_360( bool bEnable ) = 0; 

	virtual void SetCacheableTextureParams( ShaderAPITextureHandle_t *pHandles, int count, const char *pFilename, int mipSkipCount ) = 0;
	virtual void FlushHiStencil() = 0;
#endif
#if defined( _GAMECONSOLE )
	virtual void BeginConsoleZPass2( int nNumDynamicIndicesNeeded ) = 0;
	virtual void EndConsoleZPass() = 0;
	virtual unsigned int GetConsoleZPassCounter() const  = 0;
#endif
	
#if defined( _PS3 )
	virtual void FlushTextureCache() = 0;
#endif
	virtual void AntiAliasingHint( int nHint ) = 0;

	virtual bool OwnGPUResources( bool bEnable ) = 0;

	//get fog distances entered with FogStart(), FogEnd(), and SetFogZ()
	virtual void GetFogDistances( float *fStart, float *fEnd, float *fFogZ ) = 0;

	// Hooks for firing PIX events from outside the Material System...
	virtual void BeginPIXEvent( unsigned long color, const char *szName ) = 0;
	virtual void EndPIXEvent() = 0;
	virtual void SetPIXMarker( unsigned long color, const char *szName ) = 0;

	// Enables and disables for Alpha To Coverage
	virtual void EnableAlphaToCoverage() = 0;
	virtual void DisableAlphaToCoverage() = 0;

	// Computes the vertex buffer pointers 
	virtual void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc ) const = 0;
	virtual int VertexFormatSize( VertexFormat_t vertexFormat ) const = 0;

	virtual void SetDisallowAccess( bool ) = 0;
	virtual void EnableShaderShaderMutex( bool ) = 0;
	virtual void ShaderLock() = 0;
	virtual void ShaderUnlock() = 0;

	virtual void SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias ) = 0;

// ------------ New Vertex/Index Buffer interface ----------------------------
	virtual void BindVertexBuffer( int nStreamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 ) = 0;
	virtual void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes ) = 0;
	virtual void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount ) = 0;
// ------------ End ----------------------------


	// Apply stencil operations to every pixel on the screen without disturbing depth or color buffers
	virtual void PerformFullScreenStencilOperation( void ) = 0;

	virtual void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor ) = 0;

	// nVidia CSAA modes, different from SupportsMSAAMode()
	virtual bool SupportsCSAAMode( int nNumSamples, int nQualityLevel ) = 0;

	//Notifies the shaderapi to invalidate the current set of delayed constants because we just finished a draw pass. Either actual or not.
	virtual void InvalidateDelayedShaderConstants( void ) = 0; 

	// Gamma<->Linear conversions according to the video hardware we're running on
	virtual float GammaToLinear_HardwareSpecific( float fGamma ) const =0;
	virtual float LinearToGamma_HardwareSpecific( float fLinear ) const =0;

	//Set's the linear->gamma conversion textures to use for this hardware for both srgb writes enabled and disabled(identity)
	virtual void SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture ) = 0;

	virtual void BindVertexTexture( VertexTextureSampler_t nSampler, ShaderAPITextureHandle_t textureHandle ) = 0;

	// Enables hardware morphing
	virtual void EnableHWMorphing( bool bEnable ) = 0;

	// Sets flexweights for rendering
	virtual void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights ) = 0;

	virtual void FogMaxDensity( float flMaxDensity ) = 0;

	virtual void *GetD3DTexturePtr( ShaderAPITextureHandle_t hTexture ) = 0;
#ifdef _PS3
	virtual void GetPs3Texture(void* tex, ShaderAPITextureHandle_t hTexture ) = 0;
	virtual void GetPs3Texture(void* tex, StandardTextureId_t nTextureId ) = 0;
#endif 
	virtual bool IsStandardTextureHandleValid( StandardTextureId_t textureId ) = 0;

	// Create a multi-frame texture (equivalent to calling "CreateTexture" multiple times, but more efficient)
	virtual void CreateTextures( 
		ShaderAPITextureHandle_t *pHandles,
		int count,
		int width, 
		int height,
		int depth,
		ImageFormat dstImageFormat, 
		int numMipLevels, 
		int numCopies, 
		int flags, 
		const char *pDebugName,
		const char *pTextureGroupName ) = 0;

	virtual void AcquireThreadOwnership() = 0;
	virtual void ReleaseThreadOwnership() = 0;

	// Only does anything on XBox360. This is useful to eliminate stalls
	virtual void EnableBuffer2FramesAhead( bool bEnable ) = 0;

	virtual void FlipCulling( bool bFlipCulling ) = 0;

	virtual void SetTextureRenderingParameter(int parm_number, ITexture *pTexture) = 0;

	//only actually sets a bool that can be read in from shaders, doesn't do any of the legwork
	virtual void EnableSinglePassFlashlightMode( bool bEnable ) = 0;

	// stuff related to matrix stacks
	virtual void MatrixMode( MaterialMatrixMode_t matrixMode ) = 0;
	virtual void PushMatrix() = 0;
	virtual void PopMatrix() = 0;
	virtual void LoadMatrix( float *m ) = 0;
	virtual void MultMatrix( float *m ) = 0;
	virtual void MultMatrixLocal( float *m ) = 0;
	virtual void LoadIdentity( void ) = 0;
	virtual void LoadCameraToWorld( void ) = 0;
	virtual void Ortho( double left, double right, double bottom, double top, double zNear, double zFar ) = 0;
	virtual void PerspectiveX( double fovx, double aspect, double zNear, double zFar ) = 0;
	virtual	void PickMatrix( int x, int y, int width, int height ) = 0;
	virtual void Rotate( float angle, float x, float y, float z ) = 0;
	virtual void Translate( float x, float y, float z ) = 0;
	virtual void Scale( float x, float y, float z ) = 0;
	virtual void ScaleXY( float x, float y ) = 0;
	virtual void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right ) = 0;

	virtual void LoadBoneMatrix( int boneIndex, const float *m ) = 0;

	// interface for mat system to tell shaderapi about standard texture handles
	virtual void SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t nHandle ) =0;

	virtual void DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance ) = 0;

	// Allows us to override the color/alpha write settings of a material
	virtual void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable ) = 0;
	virtual void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable ) = 0;

	//extended clear buffers function with alpha independent from color
	virtual void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth ) = 0;

	virtual void OnPresent( void ) = 0;

	virtual void UpdateGameTime( float flTime ) = 0;

#ifdef _GAMECONSOLE
	// Backdoor used by the queued context to directly use write-combined memory	
	virtual IMesh *GetExternalMesh( const ExternalMeshInfo_t& info ) = 0;
	virtual void SetExternalMeshData( IMesh *pMesh, const ExternalMeshData_t &data ) = 0;
	virtual IIndexBuffer *GetExternalIndexBuffer( int nIndexCount, uint16 *pIndexData ) = 0;
	virtual void FlushGPUCache( void *pBaseAddr, size_t nSizeInBytes ) = 0;
#endif

	virtual bool IsStereoSupported() const = 0;
	virtual void UpdateStereoTexture( ShaderAPITextureHandle_t texHandle, bool *pStereoActiveThisFrame ) = 0;
	
	virtual void SetSRGBWrite( bool bState ) = 0;

	// debug logging
	// only implemented in some subclasses
	virtual void PrintfVA( char *fmt, va_list vargs ) = 0;
	virtual void Printf( char *fmt, ... ) = 0;
	virtual float Knob( char *knobname, float *setvalue = NULL ) = 0;

	virtual void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics ) = 0;

	virtual float GetLightMapScaleFactor() const = 0;

	virtual ShaderAPITextureHandle_t FindTexture( const char *pDebugName ) = 0;
	virtual void GetTextureDimensions( ShaderAPITextureHandle_t hTexture, int &nWidth, int &nHeight, int &nDepth ) = 0;

	virtual ShaderAPITextureHandle_t GetStandardTextureHandle(StandardTextureId_t id) = 0;
};


#endif // ISHADERAPI_H
