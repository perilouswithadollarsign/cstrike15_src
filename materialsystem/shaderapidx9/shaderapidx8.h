//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef SHADERAPIDX8_H
#define SHADERAPIDX8_H

#include "shaderapibase.h"
#include "shaderapi/ishadershadow.h"
#include "materialsystem/IShader.h"
#include "locald3dtypes.h"

//-----------------------------------------------------------------------------
// Vendor-specific defines
//-----------------------------------------------------------------------------
#define ATI_FETCH4_ENABLE		MAKEFOURCC('G','E','T','4')
#define ATI_FETCH4_DISABLE		MAKEFOURCC('G','E','T','1')
#define ATISAMP_FETCH4			D3DSAMP_MIPMAPLODBIAS

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CMeshBase;
class CMeshBuilder;
struct ShadowState_t;
struct DepthTestState_t;
class IMaterialInternal;
struct MeshInstanceData_t;

//#define _X360_GPU_OWN_RESOURCES
#if defined( _X360_GPU_OWN_RESOURCES )
#define IsGPUOwnSupported()		( true )
#else
#define IsGPUOwnSupported()		( false )
#endif

#if defined( _X360 )
// Define this to link shader compilation code from D3DX9.LIB
//#define X360_LINK_WITH_SHADER_COMPILE 1
#endif
#if defined( X360_LINK_WITH_SHADER_COMPILE ) && defined( _CERT )
#error "Don't ship with X360_LINK_WITH_SHADER_COMPILE defined!! It causes 2MB+ DLL bloat. Only define it while revving XDKs."
#endif

//-----------------------------------------------------------------------------
// State that matters to buffered meshes... (for debugging only)
//-----------------------------------------------------------------------------
struct BufferedState_t
{
	D3DXMATRIX m_Transform[3];
	D3DVIEWPORT9 m_Viewport;
	int m_BoundTexture[16];
	void *m_VertexShader;
	void *m_PixelShader;
};


//-----------------------------------------------------------------------------
// Compiled lighting state
//-----------------------------------------------------------------------------
struct CompiledLightingState_t
{
	Vector4D	m_AmbientLightCube[6];
	int			m_nLocalLightCount;
	Vector4D	m_PixelShaderLocalLights[6];
	Vector4D	m_VertexShaderLocalLights[20];
	int			m_VertexShaderLocalLightLoopControl[4];
	int			m_VertexShaderLocalLightEnable[VERTEX_SHADER_LIGHT_ENABLE_BOOL_CONST_COUNT];
};

struct InstanceInfo_t
{
	// Have we compiled various bits of lighting state?
	bool		m_bAmbientCubeCompiled : 1;
	bool		m_bPixelShaderLocalLightsCompiled : 1;
	bool		m_bVertexShaderLocalLightsCompiled : 1;

	// Have we set various shader constants?
	bool		m_bSetSkinConstants : 1;
	bool		m_bSetLightVertexShaderConstants : 1;
};


//-----------------------------------------------------------------------------
// The DX8 shader API
//-----------------------------------------------------------------------------
// FIXME: Remove this! Either move them into CShaderAPIBase or CShaderAPIDx8
class IShaderAPIDX8 : public CShaderAPIBase
{
public:
	// Draws the mesh
	virtual void DrawMesh( CMeshBase *pMesh, int nCount, const MeshInstanceData_t *pInstances, VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo ) = 0;

	// Draw the mesh with the currently bound vertex and index buffers.
	virtual void DrawWithVertexAndIndexBuffers( void ) = 0;

	// Gets the current buffered state... (debug only)
	virtual void GetBufferedState( BufferedState_t &state ) = 0;

	// Gets the current backface cull state....
	virtual D3DCULL GetCullMode() const = 0;

	// Measures fill rate
	virtual void ComputeFillRate() = 0;

	// Selection mode methods
	virtual bool IsInSelectionMode() const = 0;

	// We hit somefin in selection mode
	virtual void RegisterSelectionHit( float minz, float maxz ) = 0;

	// Get the currently bound material
	virtual IMaterialInternal* GetBoundMaterial() = 0;

	// These methods are called by the transition table
	// They depend on dynamic state so can't exist inside the transition table
	virtual void ApplyZBias( const DepthTestState_t  &shaderState ) = 0;
	virtual void ApplyCullEnable( bool bEnable ) = 0;
	virtual void ApplyFogMode( ShaderFogMode_t fogMode, bool bVertexFog, bool bSRGBWritesEnabled, bool bDisableGammaCorrection ) = 0;

	virtual int GetActualSamplerCount() const = 0;

	virtual bool IsRenderingMesh() const = 0;

	// Fog methods...
	virtual void EnableFixedFunctionFog( bool bFogEnable ) = 0;

	virtual int GetCurrentFrameCounter( void ) const = 0;

	// Workaround hack for visualization of selection mode
	virtual void SetupSelectionModeVisualizationState() = 0;

	virtual bool UsingSoftwareVertexProcessing() const = 0;

	//notification that the SRGB write state is being changed
	virtual void EnabledSRGBWrite( bool bEnabled ) = 0;
	virtual void SetSRGBWrite( bool bState ) = 0;

	// Alpha to coverage
	virtual void ApplyAlphaToCoverage( bool bEnable ) = 0;

	virtual void PrintfVA( char *fmt, va_list vargs ) = 0;
	virtual void Printf( char *fmt, ... ) = 0;	
	virtual float Knob( char *knobname, float *setvalue = NULL ) = 0;

	virtual void NotifyShaderConstantsChangedInRenderPass() = 0;

	virtual void GenerateNonInstanceRenderState( MeshInstanceData_t *pInstance, CompiledLightingState_t** pCompiledState, InstanceInfo_t **pInfo ) = 0;

	// Executes the per-instance command buffer
	virtual void ExecuteInstanceCommandBuffer( const unsigned char *pCmdBuf, int nInstanceIndex, bool bForceStateSet ) = 0;

	// Sets the vertex decl
	virtual void SetVertexDecl( VertexFormat_t vertexFormat, bool bHasColorMesh, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec ) = 0;

	// Set Tessellation Enable
#if ENABLE_TESSELLATION
	virtual void SetTessellationMode( TessellationMode_t mode ) = 0;
#else
	void SetTessellationMode( TessellationMode_t mode ) {}
#endif

	virtual void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics ) = 0;

	virtual float GetLightMapScaleFactor() const = 0;
};

#ifdef _PS3
//////////////////////////////////////////////////////////////////////////
//
// PS3 non-virtual implementation proxy
//
// cat shaderapidx8.h | nonvirtualscript.pl > shaderapidx8_ps3nonvirt.inl
struct CPs3NonVirt_IShaderAPIDX8
{
//NONVIRTUALSCRIPTBEGIN
//NONVIRTUALSCRIPT/PROXY/CPs3NonVirt_IShaderAPIDX8
//NONVIRTUALSCRIPT/DELEGATE/g_ShaderAPIDX8.CShaderAPIDx8::

	static ShaderAPITextureHandle_t GetStandardTextureHandle(StandardTextureId_t id);

	//
	// IShaderAPI
	//
	static void SetViewports( int nCount, const ShaderViewport_t* pViewports, bool setImmediately = false );
	static int GetViewports( ShaderViewport_t* pViewports, int nMax );
	static void ClearBuffers( bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight );
	static void ClearColor3ub( unsigned char r, unsigned char g, unsigned char b );
	static void ClearColor4ub( unsigned char r, unsigned char g, unsigned char b, unsigned char a );
	static void BindVertexShader( VertexShaderHandle_t hVertexShader );
	static void BindGeometryShader( GeometryShaderHandle_t hGeometryShader );
	static void BindPixelShader( PixelShaderHandle_t hPixelShader );
	static void SetRasterState( const ShaderRasterState_t& state );
	static bool SetMode( void* hwnd, int nAdapter, const ShaderDeviceInfo_t &info );
	static void ChangeVideoMode( const ShaderDeviceInfo_t &info );
	static StateSnapshot_t	 TakeSnapshot( );
	static void TexMinFilter( ShaderTexFilterMode_t texFilterMode );
	static void TexMagFilter( ShaderTexFilterMode_t texFilterMode );
	static void TexWrap( ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode );
	static void CopyRenderTargetToTexture( ShaderAPITextureHandle_t textureHandle );
	static void Bind( IMaterial* pMaterial );
	static IMesh* GetDynamicMesh( IMaterial* pMaterial, int nHWSkinBoneCount, bool bBuffered = true, IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0);
	static IMesh* GetDynamicMeshEx( IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount, bool bBuffered = true, IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0 );
	static bool IsTranslucent( StateSnapshot_t id );
	static bool IsAlphaTested( StateSnapshot_t id );
	static bool UsesVertexAndPixelShaders( StateSnapshot_t id );
	static bool IsDepthWriteEnabled( StateSnapshot_t id );
	static VertexFormat_t ComputeVertexFormat( int numSnapshots, StateSnapshot_t* pIds );
	static VertexFormat_t ComputeVertexUsage( int numSnapshots, StateSnapshot_t* pIds );
	static void BeginPass( StateSnapshot_t snapshot );
	static void RenderPass( const unsigned char *pInstanceCommandBuffer, int nPass, int nPassCount );
	static void SetNumBoneWeights( int numBones );
	static void SetLights( int nCount, const LightDesc_t *pDesc );
	static void SetLightingOrigin( Vector vLightingOrigin );
	static void SetLightingState( const MaterialLightingState_t& state );
	static void SetAmbientLightCube( Vector4D cube[6] );
	static void ShadeMode( ShaderShadeMode_t mode );
	static void CullMode( MaterialCullMode_t cullMode );
	static void FlipCullMode();
	static void ForceDepthFuncEquals( bool bEnable );
	static void OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable = true );
	static void SetHeightClipZ( float z ); 
	static void SetHeightClipMode( enum MaterialHeightClipMode_t heightClipMode ); 
	static void SetClipPlane( int index, const float *pPlane );
	static void EnableClipPlane( int index, bool bEnable );
	static ImageFormat GetNearestSupportedFormat( ImageFormat fmt );
	static ImageFormat GetNearestRenderTargetFormat( ImageFormat fmt );
	static bool DoRenderTargetsNeedSeparateDepthBuffer();
	static ShaderAPITextureHandle_t CreateTexture( int width, int height, int depth, ImageFormat dstImageFormat, int numMipLevels, int numCopies, int flags, const char *pDebugName, const char *pTextureGroupName );
	static void DeleteTexture( ShaderAPITextureHandle_t textureHandle );
	static ShaderAPITextureHandle_t CreateDepthTexture( ImageFormat renderTargetFormat, int width, int height, const char *pDebugName, bool bTexture, bool bAliasDepthSurfaceOverColorX360 = false );
	static bool IsTexture( ShaderAPITextureHandle_t textureHandle );
	static bool IsTextureResident( ShaderAPITextureHandle_t textureHandle );
	static void ModifyTexture( ShaderAPITextureHandle_t textureHandle );
	static void TexImage2D( int level, int cubeFaceID, ImageFormat dstFormat, int zOffset, int width, int height, ImageFormat srcFormat, bool bSrcIsTiled, void *imageData );
	static void TexSubImage2D( int level, int cubeFaceID, int xOffset, int yOffset, int zOffset, int width, int height, ImageFormat srcFormat, int srcStride, bool bSrcIsTiled, void *imageData );
	static bool TexLock( int level, int cubeFaceID, int xOffset, int yOffset, int width, int height, CPixelWriter& writer );
	static void TexUnlock( );
	static void UpdateTexture( int xOffset, int yOffset, int w, int h, ShaderAPITextureHandle_t hDstTexture, ShaderAPITextureHandle_t hSrcTexture );
	static void *LockTex( ShaderAPITextureHandle_t hTexture );
	static void UnlockTex( ShaderAPITextureHandle_t hTexture );
	static void TexSetPriority( int priority );
	static void BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle );
	static void SetRenderTarget( ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER );
	static void ClearBuffersObeyStencil( bool bClearColor, bool bClearDepth );
	static void ReadPixels( int x, int y, int width, int height, unsigned char *data, ImageFormat dstFormat );
	static void ReadPixels( Rect_t *pSrcRect, Rect_t *pDstRect, unsigned char *data, ImageFormat dstFormat, int nDstStride );
	static void FlushHardware();
	static void BeginFrame();
	static void EndFrame();
	static int  SelectionMode( bool selectionMode );
	static void SelectionBuffer( unsigned int* pBuffer, int size );
	static void ClearSelectionNames( );
	static void LoadSelectionName( int name );
	static void PushSelectionName( int name );
	static void PopSelectionName();
	static void ForceHardwareSync();
	static void ClearSnapshots();
	static void FogStart( float fStart );
	static void FogEnd( float fEnd );
	static void SetFogZ( float fogZ );	
	static void SceneFogColor3ub( unsigned char r, unsigned char g, unsigned char b );
	static void GetSceneFogColor( unsigned char *rgb );
	static void SceneFogMode( MaterialFogMode_t fogMode );
	static bool CanDownloadTextures();
	static void ResetRenderState( bool bFullReset = true );
	static int  GetCurrentDynamicVBSize();
	static void DestroyVertexBuffers( bool bExitingLevel = false );
	static void EvictManagedResources();
	static void GetGPUMemoryStats( GPUMemoryStats &stats );
	static void SetAnisotropicLevel( int nAnisotropyLevel );
	static void SyncToken( const char *pToken );
	static void SetStandardVertexShaderConstants( float fOverbright );
	static ShaderAPIOcclusionQuery_t CreateOcclusionQueryObject();
	static void DestroyOcclusionQueryObject( ShaderAPIOcclusionQuery_t q );
	static void BeginOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t q );
	static void EndOcclusionQueryDrawing( ShaderAPIOcclusionQuery_t q );
	static int OcclusionQuery_GetNumPixelsRendered( ShaderAPIOcclusionQuery_t hQuery, bool bFlush = false );
	static void SetFlashlightState( const FlashlightState_t &state, const VMatrix &worldToTexture );
	static void ClearVertexAndPixelShaderRefCounts();
	static void PurgeUnusedVertexAndPixelShaders();
	static void DXSupportLevelChanged( int nDXLevel );
	static void EnableUserClipTransformOverride( bool bEnable );
	static void UserClipTransform( const VMatrix &worldToView );
	static void SetRenderTargetEx( int nRenderTargetID, ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER, ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER );
	static void CopyRenderTargetToTextureEx( ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL );
	static void HandleDeviceLost();
	static void EnableLinearColorSpaceFrameBuffer( bool bEnable );
	static void SetFullScreenTextureHandle( ShaderAPITextureHandle_t h );
	static void SetFloatRenderingParameter(int parm_number, float value);
	static void SetIntRenderingParameter(int parm_number, int value);
	static void SetVectorRenderingParameter(int parm_number, Vector const &value);
	static float GetFloatRenderingParameter(int parm_number);
	static int GetIntRenderingParameter(int parm_number);
	static Vector GetVectorRenderingParameter(int parm_number);
	static void SetFastClipPlane( const float *pPlane );
	static void EnableFastClip( bool bEnable );
	static void GetMaxToRender( IMesh *pMesh, bool bMaxUntilFlush, int *pMaxVerts, int *pMaxIndices );
	static int GetMaxVerticesToRender( IMaterial *pMaterial );
	static int GetMaxIndicesToRender( );
	static void SetStencilState( const ShaderStencilState_t& state );
	static void ClearStencilBufferRectangle(int xmin, int ymin, int xmax, int ymax, int value);
	static void DisableAllLocalLights();
	static int CompareSnapshots( StateSnapshot_t snapshot0, StateSnapshot_t snapshot1 );
	static IMesh *GetFlexMesh();
	static void SetFlashlightStateEx( const FlashlightState_t &state, const VMatrix &worldToTexture, ITexture *pFlashlightDepthTexture );
	static void SetCascadedShadowMappingState( const CascadedShadowMappingState_t &state, ITexture *pDepthTextureAtlas );
	static const CascadedShadowMappingState_t &GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale = false );
	static bool SupportsMSAAMode( int nMSAAMode );
	static bool PostQueuedTexture( const void *pData, int nSize, ShaderAPITextureHandle_t *pHandles, int nHandles, int nWidth, int nHeight, int nDepth, int nMips, int *pRefCount );
	static void ReloadZcullMemory( int nStencilRef );
	static void AntiAliasingHint( int nHint );
	static void FlushTextureCache();
	static void InvokeGpuDataTransferCache( uint32 uiDepthBufferCacheOperation );
	static bool OwnGPUResources( bool bEnable );
	static void GetFogDistances( float *fStart, float *fEnd, float *fFogZ );
	static void BeginPIXEvent( unsigned long color, const char *szName );
	static void EndPIXEvent();
	static void SetPIXMarker( unsigned long color, const char *szName );
	static void EnableAlphaToCoverage();
	static void DisableAlphaToCoverage();
	static void ComputeVertexDescription( unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc );
	static int VertexFormatSize( VertexFormat_t vertexFormat );
	static void SetDisallowAccess( bool b );
	static void EnableShaderShaderMutex( bool b );
	static void ShaderLock();
	static void ShaderUnlock();
	static void SetShadowDepthBiasFactors( float fShadowSlopeScaleDepthBias, float fShadowDepthBias );
	static void BindVertexBuffer( int nStreamID, IVertexBuffer *pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1 );
	static void BindIndexBuffer( IIndexBuffer *pIndexBuffer, int nOffsetInBytes );
	static void Draw( MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount );
	static void PerformFullScreenStencilOperation();
	static void SetScissorRect( const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor );
	static bool SupportsCSAAMode( int nNumSamples, int nQualityLevel );
	static void InvalidateDelayedShaderConstants();
	static float GammaToLinear_HardwareSpecific( float fGamma );
	static float LinearToGamma_HardwareSpecific( float fLinear );
	static void SetLinearToGammaConversionTextures( ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture );
	static void BindVertexTexture( VertexTextureSampler_t nSampler, ShaderAPITextureHandle_t textureHandle );
	static void EnableHWMorphing( bool bEnable );
	static void SetFlexWeights( int nFirstWeight, int nCount, const MorphWeight_t* pWeights );
	static void FogMaxDensity( float flMaxDensity );
	static void CreateTextures( ShaderAPITextureHandle_t *pHandles, int count, int width, int height, int depth, ImageFormat dstImageFormat, int numMipLevels, int numCopies, int flags, const char *pDebugName, const char *pTextureGroupName );
	static void AcquireThreadOwnership();
	static void ReleaseThreadOwnership();
	static void EnableBuffer2FramesAhead( bool bEnable );
	static void FlipCulling( bool bFlipCulling );
	static void SetTextureRenderingParameter(int parm_number, ITexture *pTexture);
	static void EnableSinglePassFlashlightMode( bool bEnable );
	static void MatrixMode( MaterialMatrixMode_t matrixMode );
	static void PushMatrix();
	static void PopMatrix();
	static void LoadMatrix( float *m );
	static void MultMatrix( float *m );
	static void MultMatrixLocal( float *m );
	static void LoadIdentity();
	static void LoadCameraToWorld();
	static void Ortho( double left, double right, double bottom, double top, double zNear, double zFar );
	static void PerspectiveX( double fovx, double aspect, double zNear, double zFar );
	static	void PickMatrix( int x, int y, int width, int height );
	static void Rotate( float angle, float x, float y, float z );
	static void Translate( float x, float y, float z );
	static void Scale( float x, float y, float z );
	static void ScaleXY( float x, float y );
	static void PerspectiveOffCenterX( double fovx, double aspect, double zNear, double zFar, double bottom, double top, double left, double right );
	static void LoadBoneMatrix( int boneIndex, const float *m );
	static void SetStandardTextureHandle( StandardTextureId_t nId, ShaderAPITextureHandle_t nHandle );
	static void DrawInstances( int nInstanceCount, const MeshInstanceData_t *pInstance );
	static void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable );
	static void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable );
	static void ClearBuffersObeyStencilEx( bool bClearColor, bool bClearAlpha, bool bClearDepth );
	static void OnPresent();
	static void UpdateGameTime( float flTime );

	//
	// IShaderDynamicAPI
	//
	static double CurrentTime();
	static void GetLightmapDimensions( int *w, int *h );
	static MaterialFogMode_t GetSceneFogMode( );
	static void SetVertexShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false );
	static void SetPixelShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false );
	static void SetDefaultState();
	static void GetWorldSpaceCameraPosition( float* pPos );
	static void GetWorldSpaceCameraDirection( float* pDir );
	static int GetCurrentNumBones();
	static MaterialFogMode_t GetCurrentFogType();
	static void SetVertexShaderIndex( int vshIndex = -1 );
	static void SetPixelShaderIndex( int pshIndex = 0 );
	static void GetBackBufferDimensions( int& width, int& height );
	static const AspectRatioInfo_t &GetAspectRatioInfo( void );
	static void GetCurrentRenderTargetDimensions( int& nWidth, int& nHeight );
	static void GetCurrentViewport( int& nX, int& nY, int& nWidth, int& nHeight );
	static void SetPixelShaderFogParams( int reg );
	static bool InFlashlightMode();
	static const FlashlightState_t &GetFlashlightState( VMatrix &worldToTexture );
	static bool InEditorMode();
	static void BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id );
	static ITexture *GetRenderTargetEx( int nRenderTargetID );
	static void SetToneMappingScaleLinear( const Vector &scale );
	static const Vector &GetToneMappingScaleLinear();
	static const FlashlightState_t &GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture );
	static void GetDX9LightState( LightState_t *state );
	static int GetPixelFogCombo( );
	static void BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id );
	static bool IsHWMorphingEnabled( );
	static void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id );
	static void SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	static void SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );
	static void SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false );
	static void SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false );
	static bool ShouldWriteDepthToDestAlpha();
	static void GetMatrix( MaterialMatrixMode_t matrixMode, float *dst );
	static void PushDeformation( DeformationBase_t const *Deformation );
	static void PopDeformation( );
	static int GetNumActiveDeformations();
	static int GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations, float *pConstantValuesOut, int nBufferSize, int nMaximumDeformations, int *pNumDefsOut );
	static void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords );
	static void ExecuteCommandBuffer( uint8 *pCmdBuffer );
#ifdef _PS3
	static void ExecuteCommandBufferPPU( uint8 *pCmdBuffer );
#endif
	static void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo );
	static ITexture *GetTextureRenderingParameter(int parm_number);
	static void SetScreenSizeForVPOS( int pshReg = 32 );
	static void SetVSNearAndFarZ( int vshReg );
	static float GetFarZ();
	static bool SinglePassFlashlightModeEnabled();
	static void GetActualProjectionMatrix( float *pMatrix );
	static void SetDepthFeatheringShaderConstants( int iConstant, float fDepthBlendScale );
	static void GetFlashlightShaderInfo( bool *pShadowsEnabled, bool *pUberLight );
	static float GetFlashlightAmbientOcclusion( );
	static void SetTextureFilterMode( Sampler_t sampler, TextureFilterMode_t nMode );
	static TessellationMode_t GetTessellationMode();
	static float GetSubDHeight();
	static bool IsRenderingPaint();
	static bool IsStereoActiveThisFrame();

	//
	// CShaderAPIBase
	//
	static bool OnDeviceInit();
	static void OnDeviceShutdown();
	static void AdvancePIXFrame();
	static void ReleaseShaderObjects( bool bReleaseManagedResources = true );
	static void RestoreShaderObjects();
	static IDirect3DBaseTexture* GetD3DTexture( ShaderAPITextureHandle_t hTexture );
	static void GetPs3Texture(void* pPs3tex, ShaderAPITextureHandle_t hTexture );
	static void GetPs3Texture(void* pPs3tex, StandardTextureId_t nTextureId  );
	static void QueueResetRenderState();

	//
	// IShaderAPIDX8
	//
	static void DrawMesh( CMeshBase *pMesh, int nCount, const MeshInstanceData_t *pInstances, VertexCompressionType_t nCompressionType, CompiledLightingState_t* pCompiledState, InstanceInfo_t *pInfo );
	static void DrawWithVertexAndIndexBuffers();
	static void GetBufferedState( BufferedState_t &state );
	static D3DCULL GetCullMode();
	static void ComputeFillRate();
	static bool IsInSelectionMode();
	static void RegisterSelectionHit( float minz, float maxz );
	static IMaterialInternal* GetBoundMaterial();
	static void ApplyZBias( const DepthTestState_t& shaderState );
	static void ApplyCullEnable( bool bEnable );
	static void ApplyFogMode( ShaderFogMode_t fogMode, bool bVertexFog, bool bSRGBWritesEnabled, bool bDisableGammaCorrection );
	static int GetActualSamplerCount();
	static bool IsRenderingMesh();
	static void EnableFixedFunctionFog( bool bFogEnable );
	static int GetCurrentFrameCounter();
	static void SetupSelectionModeVisualizationState();
	static bool UsingSoftwareVertexProcessing();
	static void EnabledSRGBWrite( bool bEnabled );
	static void ApplyAlphaToCoverage( bool bEnable );
	static void PrintfVA( char *fmt, va_list vargs );
	static void Printf( char *fmt, ... ) {}
	static float Knob( char *knobname, float *setvalue = NULL );
	static void NotifyShaderConstantsChangedInRenderPass();
	static void GenerateNonInstanceRenderState( MeshInstanceData_t *pInstance, CompiledLightingState_t** pCompiledState, InstanceInfo_t **pInfo );
	static void ExecuteInstanceCommandBuffer( const unsigned char *pCmdBuf, int nInstanceIndex, bool bForceStateSet );
	static void SetVertexDecl( VertexFormat_t vertexFormat, bool bHasColorMesh, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec );
	static void SetTessellationMode( TessellationMode_t mode );
	static IMesh *GetExternalMesh( const ExternalMeshInfo_t& info );
	static void SetExternalMeshData( IMesh *pMesh, const ExternalMeshData_t &data );
	static IIndexBuffer *GetExternalIndexBuffer( int nIndexCount, uint16 *pIndexData );
	static void FlushGPUCache( void *pBaseAddr, size_t nSizeInBytes );
	static void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics );
	static void SetSRGBWrite( bool bState );

	static void BeginConsoleZPass2( int nNumDynamicIndicesNeeded );
	static void EndConsoleZPass();

//NONVIRTUALSCRIPTEND
};

#endif


#endif // SHADERAPIDX8_H
