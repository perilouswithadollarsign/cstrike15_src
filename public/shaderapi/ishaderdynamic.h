//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef ISHADERDYNAMIC_H
#define ISHADERDYNAMIC_H

#ifndef SPU

#ifdef _WIN32
#pragma once
#endif

#include "shaderapi/shareddefs.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"

typedef intp ShaderAPITextureHandle_t;
#define INVALID_SHADERAPI_TEXTURE_HANDLE 0


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class CMeshBuilder;
class IMaterialVar;
struct LightDesc_t; 


//-----------------------------------------------------------------------------
// State from ShaderAPI used to select proper vertex and pixel shader combos
//-----------------------------------------------------------------------------
struct LightState_t
{
	int  m_nNumLights;
	bool m_bAmbientLight;
	bool m_bStaticLight;
	bool m_bStaticLightIndirectOnly;  // only valid if m_bStaticLight true
	inline int HasDynamicLight() { return (m_bAmbientLight || (m_nNumLights > 0)) ? 1 : 0; }
};


//-----------------------------------------------------------------------------
// Color correction info
//-----------------------------------------------------------------------------
struct ShaderColorCorrectionInfo_t
{
	bool m_bIsEnabled;
	int m_nLookupCount;
	float m_flDefaultWeight;
	float m_pLookupWeights[4];
};

#endif // SPU

//-----------------------------------------------------------------------------
// the 3D shader API interface
// This interface is all that shaders see.
//-----------------------------------------------------------------------------
enum StandardTextureId_t
{
	// Lightmaps
	TEXTURE_LIGHTMAP = 0,
	TEXTURE_LIGHTMAP_FULLBRIGHT,
	TEXTURE_LIGHTMAP_BUMPED,
	TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT,

	// Flat colors
	TEXTURE_WHITE,
	TEXTURE_BLACK,
	TEXTURE_BLACK_ALPHA_ZERO,
	TEXTURE_GREY,
	TEXTURE_GREY_ALPHA_ZERO,

	// Normalmaps
	TEXTURE_NORMALMAP_FLAT,
	TEXTURE_SSBUMP_FLAT,

	// Normalization
	TEXTURE_NORMALIZATION_CUBEMAP,
	TEXTURE_NORMALIZATION_CUBEMAP_SIGNED,

	// Frame-buffer textures
	TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0,
	TEXTURE_FRAME_BUFFER_FULL_TEXTURE_1,

	// Color correction
	TEXTURE_COLOR_CORRECTION_VOLUME_0,
	TEXTURE_COLOR_CORRECTION_VOLUME_1,
	TEXTURE_COLOR_CORRECTION_VOLUME_2,
	TEXTURE_COLOR_CORRECTION_VOLUME_3,

	// An alias to the Back Frame Buffer
	TEXTURE_FRAME_BUFFER_ALIAS,

	// Noise for shadow mapping algorithm
	TEXTURE_SHADOW_NOISE_2D,

	// A texture in which morph data gets accumulated (vs30, fast vertex textures required)
	TEXTURE_MORPH_ACCUMULATOR,

	// A texture which contains morph weights
	TEXTURE_MORPH_WEIGHTS,

	// A snapshot of the frame buffer's depth. Currently only valid on the 360
	TEXTURE_FRAME_BUFFER_FULL_DEPTH,

	// A snapshot of the frame buffer's depth. Currently only valid on the 360
	TEXTURE_IDENTITY_LIGHTWARP,

	// The current local env_cubemap
	TEXTURE_LOCAL_ENV_CUBEMAP,

	// Texture containing subdivision surface patch data
	TEXTURE_SUBDIVISION_PATCHES,

	// Screen-space texture which contains random 3D reflection vectors used in SSAO algorithm
	TEXTURE_SSAO_NOISE_2D,

	TEXTURE_PAINT,

	// Stereo-ized texture that contains eye-dependent stereo constants.
	TEXTURE_STEREO_PARAM_MAP,
	
	TEXTURE_MAX_STD_TEXTURES
};

#ifndef SPU

enum TextureFilterMode_t
{
	TFILTER_MODE_POINTSAMPLED = 1,
};

enum TessellationMode_t;

//-----------------------------------------------------------------------------
// The Shader interface versions
//-----------------------------------------------------------------------------
#define SHADERDYNAMIC_INTERFACE_VERSION		"ShaderDynamic001"
abstract_class IShaderDynamicAPI
{
public:

	virtual void BeginPerfEEvent( wchar* name ) {}
	virtual void EndPerfEvent() {}
	virtual void SetVertexShaderViewProj() = 0;
	virtual void UpdateVertexShaderMatrix( int m ) = 0;
	virtual void SetVertexShaderModelViewProjAndModelView() = 0;
	virtual void SetVertexShaderCameraPos() = 0;
	virtual bool SetSkinningMatrices( const MeshInstanceData_t &instance ) = 0;
	virtual void BindTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t textureHandle ) = 0;

	// returns the current time in seconds....
	virtual double CurrentTime() const = 0;

	// Gets the lightmap dimensions
	virtual void GetLightmapDimensions( int *w, int *h ) = 0;

	// Scene fog state.
	// This is used by the shaders for picking the proper vertex shader for fogging based on dynamic state.
	virtual MaterialFogMode_t GetSceneFogMode( ) = 0;
	virtual void GetSceneFogColor( unsigned char *rgb ) = 0;

	// Sets the constant register for vertex and pixel shaders
	virtual void SetVertexShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false ) = 0;
	virtual void SetPixelShaderConstant( int var, float const* pVec, int numConst = 1, bool bForce = false ) = 0;

	// Sets the default *dynamic* state
	virtual void SetDefaultState() = 0;

	// Get the current camera position in world space.
	virtual void GetWorldSpaceCameraPosition( float* pPos ) const = 0;
	virtual void GetWorldSpaceCameraDirection( float* pDir ) const = 0;

	virtual int GetCurrentNumBones( void ) const = 0;

	virtual MaterialFogMode_t GetCurrentFogType( void ) const = 0;

	// Sets the vertex and pixel shaders
	virtual void SetVertexShaderIndex( int vshIndex = -1 ) = 0;
	virtual void SetPixelShaderIndex( int pshIndex = 0 ) = 0;

	// Get the dimensions of the back buffer.
	virtual void GetBackBufferDimensions( int& width, int& height ) const = 0;

	// Get information about the physical and pixel aspect ratios of the backbuffer.
	virtual const AspectRatioInfo_t &GetAspectRatioInfo( void ) const = 0;

	// Get the dimensions of the current render target
	virtual void GetCurrentRenderTargetDimensions( int& nWidth, int& nHeight ) const = 0;

	// Get the current viewport
	virtual void GetCurrentViewport( int& nX, int& nY, int& nWidth, int& nHeight ) const = 0;

	// FIXME: The following 6 methods used to live in IShaderAPI
	// and were moved for stdshader_dx8. Let's try to move them back!

	virtual void SetPixelShaderFogParams( int reg ) = 0;

	// Use this to get the mesh builder that allows us to modify vertex data
	virtual bool InFlashlightMode() const = 0;
	virtual const FlashlightState_t &GetFlashlightState( VMatrix &worldToTexture ) const = 0;
	virtual bool InEditorMode() const = 0;

	virtual bool IsCascadedShadowMapping() const = 0;
	virtual const CascadedShadowMappingState_t &GetCascadedShadowMappingState( ITexture **pDepthTextureAtlas, bool bLightMapScale = false ) const = 0;

	// Binds a standard texture
	virtual void BindStandardTexture( Sampler_t sampler, TextureBindFlags_t nBindFlags, StandardTextureId_t id ) = 0;

	virtual ITexture *GetRenderTargetEx( int nRenderTargetID ) const = 0;

	virtual void SetToneMappingScaleLinear( const Vector &scale ) = 0;
	virtual const Vector &GetToneMappingScaleLinear( void ) const = 0;
	virtual void SetFloatRenderingParameter(int parm_number, float value) = 0;

	virtual void SetIntRenderingParameter(int parm_number, int value) = 0 ;
	virtual void SetVectorRenderingParameter(int parm_number, Vector const &value) = 0 ;

	virtual float GetFloatRenderingParameter(int parm_number) const = 0 ;

	virtual int GetIntRenderingParameter(int parm_number) const = 0 ;

	virtual Vector GetVectorRenderingParameter(int parm_number) const = 0 ;

	virtual const FlashlightState_t &GetFlashlightStateEx( VMatrix &worldToTexture, ITexture **pFlashlightDepthTexture ) const = 0;

	virtual void GetDX9LightState( LightState_t *state ) const = 0;
	virtual int GetPixelFogCombo( ) = 0; //0 is either range fog, or no fog simulated with rigged range fog values. 1 is height fog

	virtual void BindStandardVertexTexture( VertexTextureSampler_t sampler, StandardTextureId_t id ) = 0;

	// Is hardware morphing enabled?
	virtual bool IsHWMorphingEnabled( ) const = 0;

	virtual void GetStandardTextureDimensions( int *pWidth, int *pHeight, StandardTextureId_t id ) = 0;

	virtual void SetBooleanVertexShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false ) = 0;
	virtual void SetIntegerVertexShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false ) = 0;
	virtual void SetBooleanPixelShaderConstant( int var, BOOL const* pVec, int numBools = 1, bool bForce = false ) = 0;
	virtual void SetIntegerPixelShaderConstant( int var, int const* pVec, int numIntVecs = 1, bool bForce = false ) = 0;

	//Are we in a configuration that needs access to depth data through the alpha channel later?
	virtual bool ShouldWriteDepthToDestAlpha( void ) const = 0;

	// Returns current matrices
	virtual void GetMatrix( MaterialMatrixMode_t matrixMode, float *dst ) = 0;

	// deformations
	virtual void PushDeformation( DeformationBase_t const *Deformation ) = 0;
	virtual void PopDeformation( ) = 0;
	virtual int GetNumActiveDeformations() const =0;


	// for shaders to set vertex shader constants. returns a packed state which can be used to set
	// the dynamic combo. returns # of active deformations
	virtual int GetPackedDeformationInformation( int nMaskOfUnderstoodDeformations,
		float *pConstantValuesOut,
		int nBufferSize,
		int nMaximumDeformations,
		int *pNumDefsOut ) const = 0;

	// This lets the lower level system that certain vertex fields requested 
	// in the shadow state aren't actually being read given particular state
	// known only at dynamic state time. It's here only to silence warnings.
	virtual void MarkUnusedVertexFields( unsigned int nFlags, int nTexCoordCount, bool *pUnusedTexCoords ) = 0;


	virtual void ExecuteCommandBuffer( uint8 *pCmdBuffer ) =0;

	// Interface for mat system to tell shaderapi about color correction
	virtual void GetCurrentColorCorrection( ShaderColorCorrectionInfo_t* pInfo ) = 0;

	virtual ITexture *GetTextureRenderingParameter(int parm_number) const = 0;

	virtual void SetScreenSizeForVPOS( int pshReg = 32 ) = 0;
	virtual void SetVSNearAndFarZ( int vshReg ) = 0;
	virtual float GetFarZ() = 0;

	virtual bool SinglePassFlashlightModeEnabled( void ) = 0;

	virtual void GetActualProjectionMatrix( float *pMatrix ) = 0;

	virtual void SetDepthFeatheringShaderConstants( int iConstant, float fDepthBlendScale ) = 0;

	virtual void GetFlashlightShaderInfo( bool *pShadowsEnabled, bool *pUberLight ) const = 0;

	virtual float GetFlashlightAmbientOcclusion( ) const = 0;

	// allows overriding texture filtering mode on an already bound texture.
	virtual void SetTextureFilterMode( Sampler_t sampler, TextureFilterMode_t nMode ) = 0;

	virtual TessellationMode_t GetTessellationMode() const = 0;

	virtual float GetSubDHeight() = 0;

#if defined( _GAMECONSOLE )
	// Enables console-specific command predication.
	// Set values to 'true' if batches should be rendered in the z-pass and/or the render pass.
	// Disabling predication returns to default values, which allows D3D to control predication
	virtual void EnablePredication( bool bZPass, bool bRenderPass ) = 0;
	virtual void DisablePredication() = 0;
#endif // _GAMECONSOLE

	virtual bool IsRenderingPaint() const = 0;

	virtual bool IsStereoActiveThisFrame() const = 0;

	virtual bool IsStandardTextureHandleValid( StandardTextureId_t textureId ) = 0;
};

// end class IShaderDynamicAPI

#endif //SPU

#endif // ISHADERDYNAMIC_H
