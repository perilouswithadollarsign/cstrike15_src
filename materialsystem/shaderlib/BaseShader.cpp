//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "shaderlib/BaseShader.h"
#include "shaderlib/ShaderDLL.h"
#include "tier0/dbg.h"
#include "shaderDLL_Global.h"
#include "materialsystem/ishadersystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/ishaderapi.h"
#include "materialsystem/materialsystem_config.h"
#include "shaderlib/cshader.h"
#include "shaderlib/commandbuilder.h"
#include "renderparm.h"
#include "mathlib/vmatrix.h"
#include "tier1/strtools.h"
#include "convar.h"
#include "tier0/vprof.h"
#include "shaderapifast.h"

// NOTE: This must be the last include file in a .cpp file!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Storage buffer used for instance command buffers
//-----------------------------------------------------------------------------
class CPerInstanceContextData : public CBasePerInstanceContextData
{
public:
	CPerInstanceContextData() : m_pCommandBuffer( NULL ), m_nSize( 0 ) {}
	virtual ~CPerInstanceContextData()
	{ 
		if ( m_pCommandBuffer )
		{
			delete m_pCommandBuffer; 
		}
	}

	virtual unsigned char* GetInstanceCommandBuffer()
	{
		return m_pCommandBuffer;
	}

	unsigned char *m_pCommandBuffer;
	int m_nSize;
};


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
const char *CBaseShader::s_pTextureGroupName = NULL;
IMaterialVar **CBaseShader::s_ppParams = NULL;
IShaderShadow *CBaseShader::s_pShaderShadow;
IShaderDynamicAPI *CBaseShader::s_pShaderAPI;
IShaderInit *CBaseShader::s_pShaderInit;
int CBaseShader::s_nModulationFlags;
int CBaseShader::s_nPassCount = 0;
CPerInstanceContextData** CBaseShader::s_pInstanceDataPtr = NULL;
static bool s_bBuildingInstanceCommandBuffer = false;
static CInstanceCommandBufferBuilder< CFixedCommandStorageBuffer< 512 > > s_InstanceCommandBuffer;

bool g_shaderConfigDumpEnable = false; //true;		//DO NOT CHECK IN ENABLED FIXME
	
//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CBaseShader::CBaseShader()
{
	GetShaderDLL()->InsertShader( this );
}


//-----------------------------------------------------------------------------
// Shader parameter info
//-----------------------------------------------------------------------------
// Look in BaseShader.h for the enumeration for these.
// Update there if you update here.
static ShaderParamInfo_t s_StandardParams[NUM_SHADER_MATERIAL_VARS] =
{
	{ "$flags",				"flags",			SHADER_PARAM_TYPE_INTEGER,	"0", SHADER_PARAM_NOT_EDITABLE },
	{ "$flags_defined",		"flags_defined",	SHADER_PARAM_TYPE_INTEGER,	"0", SHADER_PARAM_NOT_EDITABLE },
	{ "$flags2",  			"flags2",			SHADER_PARAM_TYPE_INTEGER,	"0", SHADER_PARAM_NOT_EDITABLE },
	{ "$flags_defined2",	"flags2_defined",	SHADER_PARAM_TYPE_INTEGER,	"0", SHADER_PARAM_NOT_EDITABLE },
	{ "$color",		 		"color",			SHADER_PARAM_TYPE_COLOR,	"[1 1 1]", 0 },
	{ "$alpha",	   			"alpha",			SHADER_PARAM_TYPE_FLOAT,	"1.0", 0 },
	{ "$basetexture",  		"Base Texture with lighting built in", SHADER_PARAM_TYPE_TEXTURE, "shadertest/BaseTexture", 0 },
	{ "$frame",	  			"Animation Frame",	SHADER_PARAM_TYPE_INTEGER,	"0", 0 },
	{ "$basetexturetransform", "Base Texture Texcoord Transform",SHADER_PARAM_TYPE_MATRIX,	"center .5 .5 scale 1 1 rotate 0 translate 0 0", 0 },
	{ "$flashlighttexture",  		"flashlight spotlight shape texture", SHADER_PARAM_TYPE_TEXTURE, "effects/flashlight001", SHADER_PARAM_NOT_EDITABLE },
	{ "$flashlighttextureframe",	"Animation Frame for $flashlight",	SHADER_PARAM_TYPE_INTEGER, "0", SHADER_PARAM_NOT_EDITABLE },
	{ "$color2",		 		"color2",			SHADER_PARAM_TYPE_COLOR,	"[1 1 1]", 0 },
	{ "$srgbtint",		 		"tint value to be applied when running on new-style srgb parts",			SHADER_PARAM_TYPE_COLOR,	"[1 1 1]", 0 },
};


//-----------------------------------------------------------------------------
// Gets the standard shader parameter names
// FIXME: Turn this into one function?
//-----------------------------------------------------------------------------
int CBaseShader::GetParamCount( ) const
{ 
	return NUM_SHADER_MATERIAL_VARS; 
}

const ShaderParamInfo_t &CBaseShader::GetParamInfo( int nParamIndex ) const
{
	Assert( nParamIndex < NUM_SHADER_MATERIAL_VARS );
	return s_StandardParams[nParamIndex];
}


//-----------------------------------------------------------------------------
// Necessary to snag ahold of some important data for the helper methods
//-----------------------------------------------------------------------------
void CBaseShader::InitShaderParams( IMaterialVar** ppParams, const char *pMaterialName )
{
	// Re-entrancy check
	Assert( !s_ppParams );

	s_ppParams = ppParams;

	OnInitShaderParams( ppParams, pMaterialName );

	s_ppParams = NULL;
}

void CBaseShader::InitShaderInstance( IMaterialVar** ppParams, IShaderInit *pShaderInit, const char *pMaterialName, const char *pTextureGroupName )
{
	// Re-entrancy check
	Assert( !s_ppParams );

	s_ppParams = ppParams;
	s_pShaderInit = pShaderInit;
	s_pTextureGroupName = pTextureGroupName;

	OnInitShaderInstance( ppParams, pShaderInit, pMaterialName );

	s_pTextureGroupName = NULL;
	s_ppParams = NULL;
	s_pShaderInit = NULL;
}

void CBaseShader::DrawElements( IMaterialVar **ppParams, int nModulationFlags,
	IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI, VertexCompressionType_t vertexCompression, CBasePerMaterialContextData **pContextDataPtr, CBasePerInstanceContextData** pInstanceDataPtr )
{
	VPROF("CBaseShader::DrawElements");
	// Re-entrancy check
	Assert( !s_ppParams );

	s_ppParams = ppParams;
	s_pShaderAPI = pShaderAPI;
	s_pShaderShadow = pShaderShadow;
	s_nModulationFlags = nModulationFlags;
	s_pInstanceDataPtr = (CPerInstanceContextData**)( pInstanceDataPtr );
	s_nPassCount = 0;

	if ( IsSnapshotting() )
	{
		// Set up the shadow state
		SetInitialShadowState( );
	}

	OnDrawElements( ppParams, pShaderShadow, pShaderAPI, vertexCompression, pContextDataPtr );

	s_pInstanceDataPtr = NULL;
	s_nPassCount = 0;
	s_nModulationFlags = 0;
	s_ppParams = NULL;
	s_pShaderAPI = NULL;
	s_pShaderShadow = NULL;
}


//-----------------------------------------------------------------------------
// Sets the default shadow state
//-----------------------------------------------------------------------------
void CBaseShader::SetInitialShadowState( )
{
	// Set the default state
	s_pShaderShadow->SetDefaultState();

	// Init the standard states...
	int flags = s_ppParams[FLAGS]->GetIntValue();
	if (flags & MATERIAL_VAR_IGNOREZ)
	{
		s_pShaderShadow->EnableDepthTest( false );
		s_pShaderShadow->EnableDepthWrites( false );
	}

	if (flags & MATERIAL_VAR_DECAL)
	{
		s_pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_DECAL );
		s_pShaderShadow->EnableDepthWrites( false );
	}

	if (flags & MATERIAL_VAR_NOCULL)
	{
		s_pShaderShadow->EnableCulling( false );
	}

	if (flags & MATERIAL_VAR_ZNEARER)
	{
		s_pShaderShadow->DepthFunc( SHADER_DEPTHFUNC_NEARER );
	}

	if (flags & MATERIAL_VAR_WIREFRAME)
	{
		s_pShaderShadow->PolyMode( SHADER_POLYMODEFACE_FRONT_AND_BACK, SHADER_POLYMODE_LINE );
	}

	// Set alpha to coverage
	if (flags & MATERIAL_VAR_ALLOWALPHATOCOVERAGE)
	{
		// Force the bit on and then check against alpha blend and test states in CShaderShadowDX8::ComputeAggregateShadowState()
		s_pShaderShadow->EnableAlphaToCoverage( true );
	}
}


//-----------------------------------------------------------------------------
// Draws a snapshot
//-----------------------------------------------------------------------------
void CBaseShader::Draw( bool bMakeActualDrawCall )
{
	// You forgot to call PI_EndCommandBuffer
	Assert( !s_bBuildingInstanceCommandBuffer );

	if ( IsSnapshotting() )
	{
		// Turn off transparency if we're asked to....
		if (g_pConfig->bNoTransparency && 
			((s_ppParams[FLAGS]->GetIntValue() & MATERIAL_VAR_NO_DEBUG_OVERRIDE) == 0))
		{
			s_pShaderShadow->EnableDepthWrites( true );
 			s_pShaderShadow->EnableBlending( false );
		}

		GetShaderSystem()->TakeSnapshot();

		// Automagically add skinning + vertex lighting
		if ( !s_pInstanceDataPtr[s_nPassCount] )
		{
			bool bIsSkinning = CShader_IsFlag2Set( s_ppParams, MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
			bool bIsVertexLit = CShader_IsFlag2Set( s_ppParams, MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
			if ( bIsSkinning || bIsVertexLit )
			{
				PI_BeginCommandBuffer();
				
				// NOTE: EndCommandBuffer will insert the appropriate commands
				PI_EndCommandBuffer();
			}
		}
	}
	else
	{
		//SNPROF("CBaseShader::Draw");

		GetShaderSystem()->DrawSnapshot( s_pInstanceDataPtr[s_nPassCount] ? 
			s_pInstanceDataPtr[s_nPassCount]->m_pCommandBuffer : NULL, bMakeActualDrawCall );
	}

	++s_nPassCount;
}


//-----------------------------------------------------------------------------
// Methods related to building per-instance command buffers
//-----------------------------------------------------------------------------
void CBaseShader::PI_BeginCommandBuffer()
{
	// NOTE: This assertion is here because the memory allocation strategy
	// is perhaps not the best if this is used in dynamic states; we should
	// rethink in that case.
	Assert( IsSnapshotting() );

	Assert( !s_bBuildingInstanceCommandBuffer );
	s_bBuildingInstanceCommandBuffer = true;
	s_InstanceCommandBuffer.Reset();
}

void CBaseShader::PI_EndCommandBuffer()
{
	Assert( s_bBuildingInstanceCommandBuffer );

	// Automagically add skinning
	if ( CShader_IsFlag2Set( s_ppParams, MATERIAL_VAR2_SUPPORTS_HW_SKINNING ) )
	{
		PI_SetSkinningMatrices();
	}

	if ( CShader_IsFlag2Set( s_ppParams, MATERIAL_VAR2_LIGHTING_VERTEX_LIT ) )
	{
		PI_SetVertexShaderLocalLighting();
	}

	s_bBuildingInstanceCommandBuffer = false;
	s_InstanceCommandBuffer.End();
	int nSize = s_InstanceCommandBuffer.Size();
	if ( nSize > 0 )
	{
		CPerInstanceContextData *pContextData = s_pInstanceDataPtr[ s_nPassCount ];
		if ( !pContextData )
		{
			pContextData = new CPerInstanceContextData;
			s_pInstanceDataPtr[ s_nPassCount ] = pContextData;
		}
		unsigned char *pBuf = pContextData->m_pCommandBuffer;
		if ( pContextData->m_nSize < nSize )
		{
			if ( pContextData->m_pCommandBuffer )
			{
				delete pContextData->m_pCommandBuffer;
			}
			pBuf = new unsigned char[nSize];
			pContextData->m_pCommandBuffer = pBuf;
			pContextData->m_nSize = nSize;
		}
		memcpy( pBuf, s_InstanceCommandBuffer.Base(), nSize );
	}
}


//-----------------------------------------------------------------------------
// Queues commands onto the instance command buffer
//-----------------------------------------------------------------------------
void CBaseShader::PI_SetPixelShaderAmbientLightCube( int nFirstRegister )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	s_InstanceCommandBuffer.SetPixelShaderAmbientLightCube( nFirstRegister );
}

void CBaseShader::PI_SetPixelShaderLocalLighting( int nFirstRegister )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	s_InstanceCommandBuffer.SetPixelShaderLocalLighting( nFirstRegister );
}

void CBaseShader::PI_SetVertexShaderAmbientLightCube( /*int nFirstRegister*/ )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	s_InstanceCommandBuffer.SetVertexShaderAmbientLightCube( /*nFirstRegister*/ );
}

void CBaseShader::PI_SetVertexShaderLocalLighting()
{
	Assert( s_bBuildingInstanceCommandBuffer );
	s_InstanceCommandBuffer.SetVertexShaderLocalLighting( );
}

void CBaseShader::PI_SetSkinningMatrices()
{
	Assert( s_bBuildingInstanceCommandBuffer );
	s_InstanceCommandBuffer.SetSkinningMatrices();
}

void CBaseShader::PI_SetPixelShaderAmbientLightCubeLuminance( int nFirstRegister )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	s_InstanceCommandBuffer.SetPixelShaderAmbientLightCubeLuminance( nFirstRegister );
}

void CBaseShader::PI_SetPixelShaderGlintDamping( int nFirstRegister )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	s_InstanceCommandBuffer.SetPixelShaderGlintDamping( nFirstRegister );
}

void CBaseShader::PI_SetModulationPixelShaderDynamicState_LinearColorSpace_LinearScale( int nRegister, float scale )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	Vector color2( 1.0f, 1.0f, 1.0f );
	ApplyColor2Factor( color2.Base(), true );
	s_InstanceCommandBuffer.SetModulationPixelShaderDynamicState_LinearColorSpace_LinearScale( nRegister, color2, scale );
}

void CBaseShader::PI_SetModulationPixelShaderDynamicState_LinearScale( int nRegister, float scale )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	Vector color2( 1.0f, 1.0f, 1.0f );
	ApplyColor2Factor( color2.Base() );
	s_InstanceCommandBuffer.SetModulationPixelShaderDynamicState_LinearScale( nRegister, color2, scale );
}

void CBaseShader::PI_SetModulationPixelShaderDynamicState_LinearScale_ScaleInW( int nRegister, float scale )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	Vector color2( 1.0f, 1.0f, 1.0f );
	ApplyColor2Factor( color2.Base() );
	s_InstanceCommandBuffer.SetModulationPixelShaderDynamicState_LinearScale_ScaleInW( nRegister, color2, scale );
}

void CBaseShader::PI_SetModulationPixelShaderDynamicState_LinearColorSpace( int nRegister )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	Vector color2( 1.0f, 1.0f, 1.0f );
	ApplyColor2Factor( color2.Base(), true );
	s_InstanceCommandBuffer.SetModulationPixelShaderDynamicState_LinearColorSpace( nRegister, color2 );
}

void CBaseShader::PI_SetModulationPixelShaderDynamicState( int nRegister )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	Vector color2( 1.0f, 1.0f, 1.0f );
	ApplyColor2Factor( color2.Base() );
	s_InstanceCommandBuffer.SetModulationPixelShaderDynamicState( nRegister, color2 );
}

void CBaseShader::PI_SetModulationVertexShaderDynamicState()
{
	Assert( s_bBuildingInstanceCommandBuffer );
	Vector color2( 1.0f, 1.0f, 1.0f );
	ApplyColor2Factor( color2.Base() );
	s_InstanceCommandBuffer.SetModulationVertexShaderDynamicState( VERTEX_SHADER_MODULATION_COLOR, color2 );
}

void CBaseShader::PI_SetModulationVertexShaderDynamicState_LinearScale( float flScale )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	Vector color2( 1.0f, 1.0f, 1.0f );
	ApplyColor2Factor( color2.Base() );
	s_InstanceCommandBuffer.SetModulationVertexShaderDynamicState_LinearScale( VERTEX_SHADER_MODULATION_COLOR, color2, flScale );
}

void CBaseShader::PI_SetModulationPixelShaderDynamicState_Identity( int nRegister )
{
	Assert( s_bBuildingInstanceCommandBuffer );
	s_InstanceCommandBuffer.SetModulationPixelShaderDynamicState_Identity( nRegister );
}

//-----------------------------------------------------------------------------
// Finds a particular parameter	(works because the lowest parameters match the shader)
//-----------------------------------------------------------------------------
int CBaseShader::FindParamIndex( const char *pName ) const
{
	int numParams = GetParamCount();
	for( int i = 0; i < numParams; i++ )
	{
		if( Q_strnicmp( GetParamInfo( i ).m_pName, pName, 64 ) == 0 )
		{
			return i;
		}
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Are we using graphics?
//-----------------------------------------------------------------------------
bool CBaseShader::IsUsingGraphics()
{
	return GetShaderSystem()->IsUsingGraphics();
}


//-----------------------------------------------------------------------------
// Are we using graphics?
//-----------------------------------------------------------------------------
bool CBaseShader::CanUseEditorMaterials() const
{
	return GetShaderSystem()->CanUseEditorMaterials();
}


//-----------------------------------------------------------------------------
// Loads a texture
//-----------------------------------------------------------------------------
void CBaseShader::LoadTexture( int nTextureVar, int nAdditionalCreationFlags /* = 0 */ )
{
	if ((!s_ppParams) || (nTextureVar == -1))
		return;

	IMaterialVar* pNameVar = s_ppParams[nTextureVar];
	if( pNameVar && pNameVar->IsDefined() )
	{
		s_pShaderInit->LoadTexture( pNameVar, s_pTextureGroupName, nAdditionalCreationFlags );
	}
}


//-----------------------------------------------------------------------------
// Loads a bumpmap
//-----------------------------------------------------------------------------
void CBaseShader::LoadBumpMap( int nTextureVar, int nAdditionalCreationFlags )
{
	if ((!s_ppParams) || (nTextureVar == -1))
		return;

	IMaterialVar* pNameVar = s_ppParams[nTextureVar];
	if( pNameVar && pNameVar->IsDefined() )
	{
		s_pShaderInit->LoadBumpMap( pNameVar, s_pTextureGroupName, nAdditionalCreationFlags );
	}
}


//-----------------------------------------------------------------------------
// Loads a cubemap
//-----------------------------------------------------------------------------
void CBaseShader::LoadCubeMap( int nTextureVar, int nAdditionalCreationFlags /* = 0 */ )
{
	if ((!s_ppParams) || (nTextureVar == -1))
		return;

	IMaterialVar* pNameVar = s_ppParams[nTextureVar];
	if( pNameVar && pNameVar->IsDefined() )
	{
		s_pShaderInit->LoadCubeMap( s_ppParams, pNameVar, nAdditionalCreationFlags );
	}
}


ShaderAPITextureHandle_t CBaseShader::GetShaderAPITextureBindHandle( int nTextureVar, int nFrameVar, int nTextureChannel )
{
	Assert( !IsSnapshotting() );
	Assert( nTextureVar != -1 );
	Assert ( s_ppParams );

	IMaterialVar* pTextureVar = s_ppParams[nTextureVar];
	IMaterialVar* pFrameVar = (nFrameVar != -1) ? s_ppParams[nFrameVar] : NULL;
	int nFrame = pFrameVar ? pFrameVar->GetIntValue() : 0;
	return GetShaderSystem()->GetShaderAPITextureBindHandle( pTextureVar->GetTextureValue(), nFrame, nTextureChannel );
}

void CBaseShader::BindVertexTexture( VertexTextureSampler_t vtSampler, int nTextureVar, int nFrame /* = 0  */)
{
	Assert( !IsSnapshotting() );

	IMaterialVar* pTextureVar = s_ppParams[nTextureVar];
	if ( !pTextureVar )
		return;

	GetShaderSystem()->BindVertexTexture( vtSampler, pTextureVar->GetTextureValue() );
}

ShaderAPITextureHandle_t CBaseShader::GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrame, int nTextureChannel )
{
	return GetShaderSystem()->GetShaderAPITextureBindHandle( pTexture, nFrame, nTextureChannel );
}

//-----------------------------------------------------------------------------
// Four different flavors of BindTexture(), handling the two-sampler
// case as well as ITexture* versus textureVar forms
//-----------------------------------------------------------------------------

void CBaseShader::BindTexture( Sampler_t sampler1, TextureBindFlags_t nBindFlags,  int nTextureVar, int nFrameVar /* = -1 */ )
{
	BindTexture( sampler1, SHADER_SAMPLER_INVALID, nBindFlags, nTextureVar, nFrameVar );
}


void CBaseShader::BindTexture( Sampler_t sampler1, Sampler_t sampler2, TextureBindFlags_t nBindFlags, int nTextureVar, int nFrameVar /* = -1 */ )
{
	Assert( !IsSnapshotting() );
	Assert( nTextureVar != -1 );
	Assert ( s_ppParams );

	IMaterialVar* pTextureVar = s_ppParams[nTextureVar];
	IMaterialVar* pFrameVar = (nFrameVar != -1) ? s_ppParams[nFrameVar] : NULL;
	if (pTextureVar)
	{
		int nFrame = pFrameVar ? pFrameVar->GetIntValue() : 0;

		if ( sampler2 == -1 )
		{
			GetShaderSystem()->BindTexture( sampler1, nBindFlags, pTextureVar->GetTextureValue(), nFrame );
		}
		else
		{
			GetShaderSystem()->BindTexture( sampler1, sampler2, nBindFlags, pTextureVar->GetTextureValue(), nFrame );
		}
	}
}


void CBaseShader::BindTexture( Sampler_t sampler1, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrame /* = 0 */ )
{
	BindTexture( sampler1, SHADER_SAMPLER_INVALID, nBindFlags, pTexture, nFrame );
}

void CBaseShader::BindTexture( Sampler_t sampler1, Sampler_t sampler2, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrame /* = 0 */ )
{
	Assert( !IsSnapshotting() );

	if ( sampler2 == -1 )
	{
		GetShaderSystem()->BindTexture( sampler1, nBindFlags, pTexture, nFrame );
	}
	else
	{
		GetShaderSystem()->BindTexture( sampler1, sampler2, nBindFlags, pTexture, nFrame );
	}
}


//-----------------------------------------------------------------------------
// Does the texture store translucency in its alpha channel?
//-----------------------------------------------------------------------------
bool CBaseShader::TextureIsTranslucent( int textureVar, bool isBaseTexture )
{
	if (textureVar < 0)
		return false;

	IMaterialVar** params = s_ppParams;
	if (params[textureVar]->GetType() == MATERIAL_VAR_TYPE_TEXTURE)
	{
		if (!isBaseTexture)
		{
			return params[textureVar]->GetTextureValue()->IsTranslucent();
		}
		else
		{
			// Override translucency settings if this flag is set.
			if (IS_FLAG_SET(MATERIAL_VAR_OPAQUETEXTURE))
				return false;

			bool bHasSelfIllum				= ( ( CurrentMaterialVarFlags() & MATERIAL_VAR_SELFILLUM ) != 0 );
			bool bHasSelfIllumMask			= ( ( CurrentMaterialVarFlags2() & MATERIAL_VAR2_SELFILLUMMASK ) != 0 );
			bool bHasBaseAlphaEnvmapMask	= ( ( CurrentMaterialVarFlags() & MATERIAL_VAR_BASEALPHAENVMAPMASK ) != 0 );
			bool bUsingBaseTextureAlphaForSelfIllum = bHasSelfIllum && !bHasSelfIllumMask;
			// Check if we are using base texture alpha for something other than translucency.
			if ( !bUsingBaseTextureAlphaForSelfIllum && !bHasBaseAlphaEnvmapMask )
			{
				// We aren't using base alpha for anything other than trancluceny.

				// check if the material is marked as translucent or alpha test.
				if ((CurrentMaterialVarFlags() & MATERIAL_VAR_TRANSLUCENT) ||
					(CurrentMaterialVarFlags() & MATERIAL_VAR_ALPHATEST))
				{
					// Make sure the texture has an alpha channel.
					return params[textureVar]->GetTextureValue()->IsTranslucent();
				}
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
//
// Helper methods for color modulation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Are we alpha or color modulating?
//-----------------------------------------------------------------------------
bool CBaseShader::IsAlphaModulating()
{
	return (s_nModulationFlags & SHADER_USING_ALPHA_MODULATION) != 0;
}


//-----------------------------------------------------------------------------
// FIXME: Figure out a better way to do this?
//-----------------------------------------------------------------------------
int CBaseShader::ComputeModulationFlags( IMaterialVar** params, IShaderDynamicAPI* pShaderAPI )
{
 	s_pShaderAPI = pShaderAPI;

	int mod = 0;
	if ( UsingFlashlight(params) )
	{
		mod |= SHADER_USING_FLASHLIGHT;
	}
	
	if ( UsingEditor(params) )
	{
		mod |= SHADER_USING_EDITOR;
	}

	if ( IsRenderingPaint(params) )
	{
		mod |= SHADER_USING_PAINT;
	}

	if ( IsSnapshotting() )
	{
		if ( IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER0 ) )
			mod |= SHADER_USING_GBUFFER0;
		if ( IS_FLAG2_SET( MATERIAL_VAR2_USE_GBUFFER1 ) )
			mod |= SHADER_USING_GBUFFER1;
	}
	else
	{
		int nFixedLightingMode = ShaderApiFast( pShaderAPI )->GetIntRenderingParameter( INT_RENDERPARM_ENABLE_FIXED_LIGHTING );
		if ( nFixedLightingMode & 1 )
			mod |= SHADER_USING_GBUFFER0;
		if ( nFixedLightingMode & 2 )
			mod |= SHADER_USING_GBUFFER1;
	}
	s_pShaderAPI = NULL;
	return mod;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CBaseShader::NeedsPowerOfTwoFrameBufferTexture( IMaterialVar **params, bool bCheckSpecificToThisFrame ) const 
{ 
	return CShader_IsFlag2Set( params, MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE ); 
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CBaseShader::NeedsFullFrameBufferTexture( IMaterialVar **params, bool bCheckSpecificToThisFrame ) const 
{ 
	return CShader_IsFlag2Set( params, MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE ); 
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CBaseShader::IsTranslucent( IMaterialVar **params ) const
{
	return IS_FLAG_SET( MATERIAL_VAR_TRANSLUCENT );
}

//-----------------------------------------------------------------------------
// Returns the translucency...
//-----------------------------------------------------------------------------
void CBaseShader::ApplyColor2Factor( float *pColorOut, bool isLinearSpace ) const // (*pColorOut) *= COLOR2
{
	if ( !g_pConfig->bShowDiffuse )
	{
		pColorOut[0] = pColorOut[1] = pColorOut[2] = 0.0f;
		return;
	}

	IMaterialVar* pColor2Var = s_ppParams[COLOR2];
	if ( pColor2Var->GetType() == MATERIAL_VAR_TYPE_VECTOR )
	{
		float flColor2[3];
		pColor2Var->GetVecValue( flColor2, 3 );
		
		pColorOut[0] *= flColor2[0];
		pColorOut[1] *= flColor2[1];
		pColorOut[2] *= flColor2[2];
	}
#ifndef _PS3
	if ( g_pHardwareConfig->UsesSRGBCorrectBlending() )
	{
		IMaterialVar* pSRGBVar = s_ppParams[SRGBTINT];
		if (pSRGBVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
		{
			float flSRGB[3];
			pSRGBVar->GetVecValue( flSRGB, 3 );
			
			if ( isLinearSpace )
			{
				pColorOut[0] *= flSRGB[0];
				pColorOut[1] *= flSRGB[1];
				pColorOut[2] *= flSRGB[2];
			}
			else
			{
				pColorOut[0] *= GammaToLinearFullRange( flSRGB[0] );
				pColorOut[1] *= GammaToLinearFullRange( flSRGB[1] );
				pColorOut[2] *= GammaToLinearFullRange( flSRGB[2] );
			}
		}
	}
#endif
}


//-----------------------------------------------------------------------------
//
// Helper methods for alpha blending....
//
//-----------------------------------------------------------------------------
void CBaseShader::EnableAlphaBlending( ShaderBlendFactor_t src, ShaderBlendFactor_t dst )
{
	Assert( IsSnapshotting() );
	s_pShaderShadow->EnableBlending( true );
	s_pShaderShadow->BlendFunc( src, dst );
	s_pShaderShadow->EnableDepthWrites(false);
}

void CBaseShader::DisableAlphaBlending()
{
	Assert( IsSnapshotting() );
	s_pShaderShadow->EnableBlending( false );
}

void CBaseShader::SetNormalBlendingShadowState( int textureVar, bool isBaseTexture )
{
	Assert( IsSnapshotting() );

	// Either we've got a constant modulation
	bool isTranslucent = IsAlphaModulating();

	// Or we've got a vertex alpha
	isTranslucent = isTranslucent || (CurrentMaterialVarFlags() & MATERIAL_VAR_VERTEXALPHA);

	// Or we've got a texture alpha
	isTranslucent = isTranslucent || ( TextureIsTranslucent( textureVar, isBaseTexture ) &&
		                               !(CurrentMaterialVarFlags() & MATERIAL_VAR_ALPHATEST ) );

	if (isTranslucent)
	{
		EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
	}
	else
	{
		DisableAlphaBlending();
	}
}

//ConVar mat_debug_flashlight_only( "mat_debug_flashlight_only", "0" );
void CBaseShader::SetAdditiveBlendingShadowState( int textureVar, bool isBaseTexture )
{
	Assert( IsSnapshotting() );

	// Either we've got a constant modulation
	bool isTranslucent = IsAlphaModulating();

	// Or we've got a vertex alpha
	isTranslucent = isTranslucent || (CurrentMaterialVarFlags() & MATERIAL_VAR_VERTEXALPHA);

	// Or we've got a texture alpha
	isTranslucent = isTranslucent || ( TextureIsTranslucent( textureVar, isBaseTexture ) &&
		                               !(CurrentMaterialVarFlags() & MATERIAL_VAR_ALPHATEST ) );

	/*
	if ( mat_debug_flashlight_only.GetBool() )
	{
		if (isTranslucent)
		{
			EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA);
			//s_pShaderShadow->EnableAlphaTest( true );
			//s_pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_GREATER, 0.99f );
		}
		else
		{
			EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ZERO);
		}
	}
	else
	*/
	{
		if (isTranslucent)
		{
			EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE );
		}
		else
		{
			EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
		}
	}
}

void CBaseShader::SetDefaultBlendingShadowState( int textureVar, bool isBaseTexture ) 
{
	if ( CurrentMaterialVarFlags() & MATERIAL_VAR_ADDITIVE )
	{
		SetAdditiveBlendingShadowState( textureVar, isBaseTexture );
	}
	else
	{
		SetNormalBlendingShadowState( textureVar, isBaseTexture );
	}
}

void CBaseShader::SetBlendingShadowState( BlendType_t nMode )
{
	switch ( nMode )
	{
		case BT_NONE:
			DisableAlphaBlending();
			break;

		case BT_BLEND:
			EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			break;

		case BT_ADD:
			EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE );
			break;

		case BT_BLENDADD:
			EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE );
			break;
	}
}



//-----------------------------------------------------------------------------
// Sets lightmap blending mode for single texturing
//-----------------------------------------------------------------------------
void CBaseShader::SingleTextureLightmapBlendMode( )
{
	Assert( IsSnapshotting() );

	s_pShaderShadow->EnableBlending( true );
	s_pShaderShadow->BlendFunc( SHADER_BLEND_DST_COLOR, SHADER_BLEND_SRC_COLOR );
}

FORCEINLINE void CBaseShader::SetFogMode( ShaderFogMode_t fogMode )
{
	bool bVertexFog = ((CurrentMaterialVarFlags() & MATERIAL_VAR_VERTEXFOG) != 0);

	if (( CurrentMaterialVarFlags() & MATERIAL_VAR_NOFOG ) == 0)
	{
		s_pShaderShadow->FogMode( fogMode, bVertexFog );
	}
	else
	{
		s_pShaderShadow->FogMode( SHADER_FOGMODE_DISABLED, bVertexFog );
	}
}

//-----------------------------------------------------------------------------
//
// Helper methods for fog
//
//-----------------------------------------------------------------------------
void CBaseShader::FogToOOOverbright( void )
{
	Assert( IsSnapshotting() );
	SetFogMode( SHADER_FOGMODE_OO_OVERBRIGHT );
}

void CBaseShader::FogToWhite( void )
{
	Assert( IsSnapshotting() );
	SetFogMode( SHADER_FOGMODE_WHITE );
}
void CBaseShader::FogToBlack( void )
{
	Assert( IsSnapshotting() );
	SetFogMode( SHADER_FOGMODE_BLACK );
}

void CBaseShader::FogToGrey( void )
{
	Assert( IsSnapshotting() );
	SetFogMode( SHADER_FOGMODE_GREY );
}

void CBaseShader::FogToFogColor( void )
{
	Assert( IsSnapshotting() );
	SetFogMode( SHADER_FOGMODE_FOGCOLOR );
}

void CBaseShader::DisableFog( void )
{
	Assert( IsSnapshotting() );
	s_pShaderShadow->FogMode( SHADER_FOGMODE_DISABLED, false );
}

void CBaseShader::DefaultFog( void )
{
	if ( CurrentMaterialVarFlags() & MATERIAL_VAR_ADDITIVE )
	{
		FogToBlack();
	}
	else
	{
		FogToFogColor();
	}
}

bool CBaseShader::UsingFlashlight( IMaterialVar **params ) const
{
	if( IsSnapshotting() )
	{
		return CShader_IsFlag2Set( params, MATERIAL_VAR2_USE_FLASHLIGHT );
	}
	else
	{
		return s_pShaderAPI->InFlashlightMode();
	}
}

bool CBaseShader::UsingEditor( IMaterialVar **params ) const
{
	if( IsSnapshotting() )
	{
		return CShader_IsFlag2Set( params, MATERIAL_VAR2_USE_EDITOR );
	}
	else
	{
		return s_pShaderAPI->InEditorMode();
	}
}

bool CBaseShader::IsRenderingPaint( IMaterialVar **params ) const
{
	if( IsSnapshotting() )
	{
		// NOTE: This only works because IsRenderingPaint
		// really only affects lightmappedgeneric in a specific way.
		// If we make it used more generally, then we'll need a pattern
		// more similar to UsingEditor or UsingFlashlight
		return CShader_IsFlag2Set( params, MATERIAL_VAR2_USE_PAINT );
	}
	else
	{
		if ( !g_pConfig->m_bPaintInGame || !g_pConfig->m_bPaintInMap )
			return false;
		return s_pShaderAPI->IsRenderingPaint();
	}
}

bool CBaseShader::IsHDREnabled( void )
{
#ifdef _PS3
	return true;
#else
	// HDRFIXME!  Need to fix this for vgui materials
	HDRType_t hdr_mode = g_pHardwareConfig->GetHDRType();
	return ( hdr_mode == HDR_TYPE_INTEGER ) || ( hdr_mode == HDR_TYPE_FLOAT );
#endif
}
