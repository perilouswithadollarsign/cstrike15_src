//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
// This is what all shaders inherit from.
//===========================================================================//

#ifndef BASESHADER_H
#define BASESHADER_H

#ifdef _WIN32		   
#pragma once
#endif

#include "materialsystem/IShader.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/ishaderapi.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "shaderlib/baseshader_declarations.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IMaterialVar;
class CPerInstanceContextData;





//-----------------------------------------------------------------------------
// Base class for shaders, contains helper methods.
//-----------------------------------------------------------------------------
class CBaseShader : public IShader
{
public:
	// constructor
	CBaseShader();

	// Methods inherited from IShader
	virtual char const* GetFallbackShader( IMaterialVar** params ) const { return 0; }
	virtual int GetParamCount( ) const;
	virtual const ShaderParamInfo_t& GetParamInfo( int paramIndex ) const;

	virtual void InitShaderParams( IMaterialVar** ppParams, const char *pMaterialName );
	virtual void InitShaderInstance( IMaterialVar** ppParams, IShaderInit *pShaderInit, const char *pMaterialName, const char *pTextureGroupName );
	virtual void DrawElements( IMaterialVar **params, int nModulationFlags, IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI,
								VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContext, CBasePerInstanceContextData** pInstanceDataPtr );

	virtual int ComputeModulationFlags( IMaterialVar** params, IShaderDynamicAPI* pShaderAPI );
	virtual bool NeedsPowerOfTwoFrameBufferTexture( IMaterialVar **params, bool bCheckSpecificToThisFrame = true ) const;
	virtual bool NeedsFullFrameBufferTexture( IMaterialVar **params, bool bCheckSpecificToThisFrame = true ) const;
	virtual bool IsTranslucent( IMaterialVar **params ) const;

public:
	// These functions must be implemented by the shader
	virtual void OnInitShaderParams( IMaterialVar** ppParams, const char *pMaterialName ) {}
	virtual void OnInitShaderInstance( IMaterialVar** ppParams, IShaderInit *pShaderInit, const char *pMaterialName ) = 0;
	virtual void OnDrawElements( IMaterialVar **params, IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI, VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr ) = 0;

	// Sets the default shadow state
	void SetInitialShadowState( );
 
	// Draws a snapshot
	void Draw( bool bMakeActualDrawCall = true );

	// Are we currently taking a snapshot?
	bool IsSnapshotting() const;

	// Methods related to building per-instance ("PI_") command buffers
	void PI_BeginCommandBuffer();
	void PI_EndCommandBuffer();
	void PI_SetPixelShaderAmbientLightCube( int nFirstRegister );
	void PI_SetPixelShaderLocalLighting( int nFirstRegister );
	void PI_SetPixelShaderAmbientLightCubeLuminance( int nFirstRegister );
	void PI_SetPixelShaderGlintDamping( int nFirstRegister );
	void PI_SetVertexShaderAmbientLightCube( /*int nFirstRegister*/ );
	void PI_SetModulationPixelShaderDynamicState( int nRegister );
	void PI_SetModulationPixelShaderDynamicState_LinearColorSpace_LinearScale( int nRegister, float scale );
	void PI_SetModulationPixelShaderDynamicState_LinearScale( int nRegister, float scale );
	void PI_SetModulationPixelShaderDynamicState_LinearScale_ScaleInW( int nRegister, float scale );
	void PI_SetModulationPixelShaderDynamicState_LinearColorSpace( int nRegister );
	void PI_SetModulationPixelShaderDynamicState_Identity( int nRegister );
	void PI_SetModulationVertexShaderDynamicState( void );
	void PI_SetModulationVertexShaderDynamicState_LinearScale( float flScale );

	// Gets at the current materialvar flags
	int CurrentMaterialVarFlags() const;

	// Gets at the current materialvar2 flags
	int CurrentMaterialVarFlags2() const;

	// Finds a particular parameter	(works because the lowest parameters match the shader)
	int FindParamIndex( const char *pName ) const;

	// Are we using graphics?
	bool IsUsingGraphics();

	// Are we using editor materials?
	bool CanUseEditorMaterials() const;

	// Loads a texture
	void LoadTexture( int nTextureVar, int nAdditionalCreationFlags = 0 );

	// Loads a bumpmap
	void LoadBumpMap( int nTextureVar, int nAdditionalCreationFlags = 0 );

	// Loads a cubemap
	void LoadCubeMap( int nTextureVar, int nAdditionalCreationFlags = 0  );

	// get the shaderapi handle for a texture. BE CAREFUL WITH THIS. 
	ShaderAPITextureHandle_t GetShaderAPITextureBindHandle( int nTextureVar, int nFrameVar, int nTextureChannel = 0 );
	ShaderAPITextureHandle_t GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrame, int nTextureChannel = 0 );

	// Binds a texture
	void BindTexture( Sampler_t sampler1, Sampler_t sampler2, TextureBindFlags_t nBindFlags, int nTextureVar, int nFrameVar = -1 );
	void BindTexture( Sampler_t sampler1, TextureBindFlags_t nBindFlags, int nTextureVar, int nFrameVar = -1 );
	void BindTexture( Sampler_t sampler1, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrame = 0 );
	void BindTexture( Sampler_t sampler1, Sampler_t sampler2, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrame = 0 );

	// Bind vertex texture
	void BindVertexTexture( VertexTextureSampler_t vtSampler, int nTextureVar, int nFrame = 0 );

	// Is the texture translucent?
	bool TextureIsTranslucent( int textureVar, bool isBaseTexture );

	// Is the color var white?
	bool IsWhite( int colorVar );

	// Helper methods for fog
	void FogToOOOverbright( void );
	void FogToWhite( void );
	void FogToBlack( void );
	void FogToGrey( void );
	void FogToFogColor( void );
	void DisableFog( void );
	void DefaultFog( void );
	
	// Helpers for alpha blending
	void EnableAlphaBlending( ShaderBlendFactor_t src, ShaderBlendFactor_t dst );
	void DisableAlphaBlending();

	void SetBlendingShadowState( BlendType_t nMode );

	void SetNormalBlendingShadowState( int textureVar = -1, bool isBaseTexture = true );
	void SetAdditiveBlendingShadowState( int textureVar = -1, bool isBaseTexture = true );
	void SetDefaultBlendingShadowState( int textureVar = -1, bool isBaseTexture = true );
	void SingleTextureLightmapBlendMode( );

	// Helpers for color modulation
	bool IsAlphaModulating();
	// Helpers for HDR
	bool IsHDREnabled( void );

	bool UsingFlashlight( IMaterialVar **params ) const;
	bool UsingEditor( IMaterialVar **params ) const;
	bool IsRenderingPaint( IMaterialVar **params ) const;

	void ApplyColor2Factor( float *pColorOut, bool isLinearSpace = false ) const;		// (*pColorOut) *= COLOR2
	
	static inline IMaterialVar **GetPPParams();
	void SetPPParams( IMaterialVar **params ) { s_ppParams = params; }
	void SetModulationFlags( int modulationFlags ) { s_nModulationFlags = modulationFlags; }

private:
	// This is a per-instance state which is handled completely by the system
	void PI_SetSkinningMatrices();
	void PI_SetVertexShaderLocalLighting( );

	FORCEINLINE void SetFogMode( ShaderFogMode_t fogMode );
	
protected:
	static IMaterialVar **s_ppParams;	
	static const char *s_pTextureGroupName; // Current material's texture group name.
	static IShaderShadow *s_pShaderShadow;
	static IShaderDynamicAPI *s_pShaderAPI;
	static IShaderInit *s_pShaderInit;
private:
	static int s_nPassCount;
	static int s_nModulationFlags;
	static CPerInstanceContextData** s_pInstanceDataPtr;

	template <class T> friend class CBaseCommandBufferBuilder;
};

//-----------------------------------------------------------------------------
// Gets at the current materialvar flags
//-----------------------------------------------------------------------------
inline int CBaseShader::CurrentMaterialVarFlags() const
{
	return s_ppParams[FLAGS]->GetIntValue();
}

//-----------------------------------------------------------------------------
// Gets at the current materialvar2 flags
//-----------------------------------------------------------------------------
inline int CBaseShader::CurrentMaterialVarFlags2() const
{
	return s_ppParams[FLAGS2]->GetIntValue();
}

//-----------------------------------------------------------------------------
// Are we currently taking a snapshot?
//-----------------------------------------------------------------------------
inline bool CBaseShader::IsSnapshotting() const
{
	return (s_pShaderShadow != NULL);
}

//-----------------------------------------------------------------------------
// Is the color var white?
//-----------------------------------------------------------------------------
inline bool CBaseShader::IsWhite( int colorVar )
{
	if (colorVar < 0)
		return true;

	if (!s_ppParams[colorVar]->IsDefined())
		return true;

	float color[3];
	s_ppParams[colorVar]->GetVecValue( color, 3 );
	return (color[0] >= 1.0f) && (color[1] >= 1.0f) && (color[2] >= 1.0f);
}

//-----------------------------------------------------------------------------
// Returns the s_ppParams static member variable - for internal use only.
//-----------------------------------------------------------------------------
inline IMaterialVar **CBaseShader::GetPPParams()
{
	return s_ppParams;
}

#endif // BASESHADER_H
