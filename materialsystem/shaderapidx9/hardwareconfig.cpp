//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#define DISABLE_PROTECTED_THINGS
#include "togl/rendermechanism.h"
#include "hardwareconfig.h"
#include "shaderapi/ishaderutil.h"
#include "shaderapi_global.h"
#include "materialsystem/materialsystem_config.h"
#include "tier1/convar.h"
#include "shaderdevicebase.h"
#include "tier0/icommandline.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


extern ConVar mat_slopescaledepthbias_shadowmap;
extern ConVar mat_depthbias_shadowmap;
static ConVar developer( "developer", "0", FCVAR_RELEASE, "Set developer message level" ); 

//-----------------------------------------------------------------------------
//
// Hardware Config!
//
//-----------------------------------------------------------------------------
static CHardwareConfig s_HardwareConfig;
CHardwareConfig *g_pHardwareConfig = &s_HardwareConfig;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CHardwareConfig, IMaterialSystemHardwareConfig, 
	MATERIALSYSTEM_HARDWARECONFIG_INTERFACE_VERSION, s_HardwareConfig )


CHardwareConfig::CHardwareConfig()
{
	memset( &m_Caps, 0, sizeof( HardwareCaps_t ) );
	memset( &m_ActualCaps, 0, sizeof( HardwareCaps_t ) );
	memset( &m_UnOverriddenCaps, 0, sizeof( HardwareCaps_t ) );

	m_bHDREnabled = false;

	// FIXME: This is kind of a hack to deal with DX8 worldcraft startup.
	// We can at least have this much texture 
	m_Caps.m_MaxTextureWidth = m_Caps.m_MaxTextureHeight = m_Caps.m_MaxTextureDepth = 256;
}


//-----------------------------------------------------------------------------

bool CHardwareConfig::GetHDREnabled( void ) const
{
//	printf("\n CHardwareConfig::GetHDREnabled returning m_bHDREnabled value of %s on %8x", m_bHDREnabled?"true":"false", this );
	return m_bHDREnabled;
}

void CHardwareConfig::SetHDREnabled( bool bEnable )
{
//	printf("\n CHardwareConfig::SetHDREnabled setting m_bHDREnabled to value of %s on %8x", bEnable?"true":"false",  this );
	m_bHDREnabled = bEnable;
}


//-----------------------------------------------------------------------------
// Gets the recommended configuration associated with a particular dx level
//-----------------------------------------------------------------------------
void CHardwareConfig::ForceCapsToDXLevel( HardwareCaps_t *pCaps, int nDxLevel, const HardwareCaps_t &actualCaps )
{
	if ( !IsPC() || nDxLevel > 100 )
		return;

	pCaps->m_nDXSupportLevel = nDxLevel;
	switch( nDxLevel )
	{
	case 90:
		pCaps->m_NumVertexSamplers = 0;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bSupportsVertexTextures = false;
		pCaps->m_bSupportsBorderColor = false;

		// 2b gets four lights, 2.0 gets two...
		pCaps->m_SupportsPixelShaders_2_b = false;
		pCaps->m_SupportsShaderModel_3_0 = false;
		pCaps->m_MaxNumLights = 2;
		pCaps->m_nMaxViewports = 1;
		pCaps->m_NumPixelShaderConstants = 32;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bDX10Card = false;
		pCaps->m_bDX10Blending = false;
		pCaps->m_MaxVertexShader30InstructionSlots = 0;
		pCaps->m_MaxPixelShader30InstructionSlots  = 0;
		pCaps->m_bSupportsCascadedShadowMapping = false;
		pCaps->m_nCSMQuality = 0;
		break;

	case 92:
		pCaps->m_NumVertexSamplers = 0;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bSupportsVertexTextures = false;
		pCaps->m_bSupportsBorderColor = false;

		// 2b gets four lights (iff supports static control flow otherwise 2), 2.0 gets two...
		pCaps->m_SupportsShaderModel_3_0 = false;
		if ( IsOpenGL() )
		{
            if ( IsOSX() )
            {
                pCaps->m_bSupportsStaticControlFlow = CommandLine()->CheckParm( "-glslcontrolflow" ) != NULL;
            }
            else
            {
                pCaps->m_bSupportsStaticControlFlow = !CommandLine()->CheckParm( "-noglslcontrolflow" );
            }
            
			pCaps->m_MaxUserClipPlanes = 2;
			pCaps->m_UseFastClipping = false;
			pCaps->m_MaxNumLights = pCaps->m_bSupportsStaticControlFlow ? 4 : 2;
		}
		else
		{
			pCaps->m_MaxNumLights = MAX_NUM_LIGHTS;
		}

		pCaps->m_nMaxViewports = 1;
		pCaps->m_NumPixelShaderConstants = 32;
		pCaps->m_nMaxVertexTextureDimension = 0;
		pCaps->m_bDX10Card = false;
		pCaps->m_bDX10Blending = false;
		pCaps->m_MaxVertexShader30InstructionSlots = 0;
		pCaps->m_MaxPixelShader30InstructionSlots  = 0;
        pCaps->m_bSupportsCascadedShadowMapping = false;
		pCaps->m_nCSMQuality = 0;
		break;

	case 95:
		pCaps->m_bDX10Card = false;
		pCaps->m_bDX10Blending = false;
		pCaps->m_nMaxViewports = 1;
		pCaps->m_bSupportsBorderColor = false;
            
        if ( IsOpenGL() )
        {
            if ( IsOSX() )
            {
                pCaps->m_bSupportsStaticControlFlow = CommandLine()->CheckParm( "-glslcontrolflow" ) != NULL;
            }
            else
            {
                pCaps->m_bSupportsStaticControlFlow = !CommandLine()->CheckParm( "-noglslcontrolflow" );
            }
                
            pCaps->m_MaxUserClipPlanes = 2;
            pCaps->m_UseFastClipping = false;
            pCaps->m_MaxNumLights = pCaps->m_bSupportsStaticControlFlow ? 4 : 2;
        }
        else
        {
            pCaps->m_MaxNumLights = MAX_NUM_LIGHTS;
        }

		break;

	case 100:
        if ( IsOpenGL() )
        {
            if ( IsOSX() )
            {
                pCaps->m_bSupportsStaticControlFlow = CommandLine()->CheckParm( "-glslcontrolflow" ) != NULL;
            }
            else
            {
                pCaps->m_bSupportsStaticControlFlow = !CommandLine()->CheckParm( "-noglslcontrolflow" );
            }
                
            pCaps->m_MaxUserClipPlanes = 2;
            pCaps->m_UseFastClipping = false;
            pCaps->m_MaxNumLights = pCaps->m_bSupportsStaticControlFlow ? 4 : 2;
        }
        else
        {
            pCaps->m_MaxNumLights = MAX_NUM_LIGHTS;
        }
		break;

	default:
		Assert( 0 );
		break;
	}

#ifdef _PS3
	pCaps->m_NumPixelShaderConstants = MAX_FRAGMENT_PROGRAM_CONSTS; // this is somewhat of a lie... fragment shader constants are special on PS3 and we actually have a larger number of these
#endif
}


//-----------------------------------------------------------------------------
// Sets up the hardware caps given the specified DX level
//-----------------------------------------------------------------------------
void CHardwareConfig::SetupHardwareCaps( int nDXLevel, const HardwareCaps_t &actualCaps )
{
	Assert( nDXLevel != 0 );

	if ( nDXLevel < actualCaps.m_nMinDXSupportLevel )
	{
		Warning( "Trying to set dxlevel (%d) which is lower than the card can support (%d)!\n", nDXLevel, actualCaps.m_nMinDXSupportLevel );
	}
	if ( nDXLevel > actualCaps.m_nMaxDXSupportLevel )
	{
		Warning( "Trying to set dxlevel (%d) which is higher than the card can support (%d)!\n", nDXLevel, actualCaps.m_nMaxDXSupportLevel );
	}

	memcpy( &m_Caps, &actualCaps, sizeof(HardwareCaps_t) );
	memcpy( &m_UnOverriddenCaps, &actualCaps, sizeof(HardwareCaps_t) );

	// Don't bother with fallbacks for DX10 or consoles
#ifdef DX_TO_GL_ABSTRACTION
	if ( nDXLevel >= 100 )
#else
	if ( !( IsPC() || IsPosix() ) || ( nDXLevel >= 100 ) )
#endif
		return;

	// Don't bother with fallbacks for consoles.
	if ( IsGameConsole() )
		return;

	int nForceDXLevel = CommandLine()->ParmValue( "-maxdxlevel", 0 );
	if ( nForceDXLevel >= 90 )
	{
		nDXLevel = nForceDXLevel;
	}
	else 
	{
		// Don't bother with fallbacks for DX10 or consoles
		if ( !IsPC() || !IsPosix() || ( nDXLevel >= 100 ) )
			return;
	}
	
	// Slam the support level to what we were requested
	m_Caps.m_nDXSupportLevel = nDXLevel;
	int nMaxDXLevel = CommandLine()->ParmValue( "-maxdxlevel", m_Caps.m_nMaxDXSupportLevel );
	if ( IsOpenGL() )
	{
		// Prevent customers from ever trying to slam the dxlevel too low in GL mode.
		nMaxDXLevel = MAX( nMaxDXLevel, 90 );
	}
	{
		// We're falling back to some other dx level
		ForceCapsToDXLevel( &m_Caps, m_Caps.m_nDXSupportLevel, m_ActualCaps );
	}

	// Read dxsupport.cfg which has config overrides for particular cards.
	g_pShaderDeviceMgr->ReadHardwareCaps( m_Caps, m_Caps.m_nDXSupportLevel );

	// This is the spot to validate read in caps versus actual caps.
	if ( m_Caps.m_MaxUserClipPlanes > m_ActualCaps.m_MaxUserClipPlanes )
	{
		m_Caps.m_MaxUserClipPlanes = m_ActualCaps.m_MaxUserClipPlanes;
	}
	if ( m_Caps.m_MaxUserClipPlanes == 0 )
	{
		m_Caps.m_UseFastClipping = true;
	}

	// 2b supports more lights than just 2.0
	if ( ( m_Caps.m_SupportsPixelShaders_2_b ) && ( m_Caps.m_nDXSupportLevel >= 92 ) )
	{
		m_Caps.m_MaxNumLights = MAX_NUM_LIGHTS;
	}
	else
	{
		m_Caps.m_MaxNumLights = MAX_NUM_LIGHTS-2;
	}

	if ( IsOpenGL() )
	{
		m_Caps.m_MaxNumLights = MIN( m_Caps.m_bSupportsStaticControlFlow ? MAX_NUM_LIGHTS : 2, m_Caps.m_MaxNumLights );
		m_Caps.m_bSupportsShadowDepthTextures = true;
	}
	
	m_Caps.m_MaxNumLights = MIN( m_Caps.m_MaxNumLights, MAX_NUM_LIGHTS );

	memcpy( &m_UnOverriddenCaps, &m_Caps, sizeof(HardwareCaps_t) );
}


//-----------------------------------------------------------------------------
// Sets up the hardware caps given the specified DX level
//-----------------------------------------------------------------------------
void CHardwareConfig::SetupHardwareCaps( const ShaderDeviceInfo_t& mode, const HardwareCaps_t &actualCaps )
{
	memcpy( &m_ActualCaps, &actualCaps, sizeof(HardwareCaps_t) );
	SetupHardwareCaps( mode.m_nDXLevel, actualCaps );
}


void CHardwareConfig::OverrideStreamOffsetSupport( bool bOverrideEnabled, bool bEnableSupport )
{
	if ( bOverrideEnabled )
	{
		m_Caps.m_bSupportsStreamOffset = bEnableSupport;
		if ( !m_ActualCaps.m_bSupportsStreamOffset )
		{
			m_Caps.m_bSupportsStreamOffset = false;
		}
	}
	else
	{
		// Go back to default
		m_Caps.m_bSupportsStreamOffset = m_UnOverriddenCaps.m_bSupportsStreamOffset;
	}
}


//-----------------------------------------------------------------------------
// Implementation of IMaterialSystemHardwareConfig
//-----------------------------------------------------------------------------
bool CHardwareConfig::HasStencilBuffer() const
{
	return StencilBufferBits() > 0;
}

int	 CHardwareConfig::GetFrameBufferColorDepth() const
{
	if ( !g_pShaderDevice )
		return 0;
	return ShaderUtil()->ImageFormatInfo( g_pShaderDevice->GetBackBufferFormat() ).m_nNumBytes;
}

int CHardwareConfig::GetSamplerCount() const
{
	return m_Caps.m_NumSamplers;
}

int CHardwareConfig::GetVertexSamplerCount() const
{
	return m_Caps.m_NumVertexSamplers;
}

bool CHardwareConfig::HasSetDeviceGammaRamp() const
{
	return m_Caps.m_HasSetDeviceGammaRamp;
}

VertexCompressionType_t CHardwareConfig::SupportsCompressedVertices() const
{
	return m_Caps.m_SupportsCompressedVertices;
}

bool CHardwareConfig::SupportsBorderColor() const
{
	return m_Caps.m_bSupportsBorderColor;
}

bool CHardwareConfig::SupportsFetch4() const
{
	return m_Caps.m_bSupportsFetch4;
}

float CHardwareConfig::GetShadowDepthBias() const
{
	// FIXME: Should these not use convars?
	return mat_depthbias_shadowmap.GetFloat();
}

float CHardwareConfig::GetShadowSlopeScaleDepthBias() const
{
	// FIXME: Should these not use convars?
	return mat_slopescaledepthbias_shadowmap.GetFloat();
}

bool CHardwareConfig::PreferZPrepass() const
{
	return m_Caps.m_bPreferZPrepass;
}

bool CHardwareConfig::SuppressPixelShaderCentroidHackFixup() const
{
	return m_Caps.m_bSuppressPixelShaderCentroidHackFixup;
}

bool CHardwareConfig::PreferTexturesInHWMemory() const
{
	return m_Caps.m_bPreferTexturesInHWMemory;
}

bool CHardwareConfig::PreferHardwareSync() const
{
	return m_Caps.m_bPreferHardwareSync;
}

bool CHardwareConfig::SupportsStaticControlFlow() const
{
	return m_Caps.m_bSupportsStaticControlFlow;
}

bool CHardwareConfig::IsUnsupported() const
{
	return m_Caps.m_bUnsupported;
}

ShadowFilterMode_t CHardwareConfig::GetShadowFilterMode( bool bForceLowQualityShadows, bool bPS30 ) const
{
#if PLATFORM_POSIX || !defined( PLATFORM_X360 )
	static ConVarRef gpu_level( "gpu_level" );
	int nGPULevel = gpu_level.GetInt();
	
    const bool bUseLowQualityShadows = ( nGPULevel < 2 ) || ( bForceLowQualityShadows );
#endif
	
#if PLATFORM_POSIX
	// Currently Mac or PS3
	if ( !m_Caps.m_bSupportsShadowDepthTextures )
		return SHADOWFILTERMODE_DEFAULT;

    if ( IsOSXOpenGL() &&
         ( bUseLowQualityShadows || ( m_Caps.m_VendorID == VENDORID_INTEL ) ) )
    {
        return NVIDIA_PCF_CHEAP;
    }

	if( IsPS3() )
	{
		// PS3 shaders doesn't use the regular PC/POSIX values. It supports either 9 (the default) or 1 tap (fast) filtering.
		return bForceLowQualityShadows ? GAMECONSOLE_SINGLE_TAP_PCF : GAMECONSOLE_NINE_TAP_PCF;
	}
#elif defined( PLATFORM_X360 )
	// X360
	return bForceLowQualityShadows ? GAMECONSOLE_SINGLE_TAP_PCF : GAMECONSOLE_NINE_TAP_PCF;
#else
	// PC
	if ( !m_Caps.m_bSupportsShadowDepthTextures || !ShaderUtil()->GetConfig().ShadowDepthTexture() )
		return SHADOWFILTERMODE_DEFAULT;
			
	switch ( m_Caps.m_ShadowDepthTextureFormat )
	{
		case IMAGE_FORMAT_D16_SHADOW:
		case IMAGE_FORMAT_D24X8_SHADOW:
			if ( ( m_Caps.m_VendorID == VENDORID_NVIDIA ) || ( m_Caps.m_VendorID == VENDORID_INTEL ) )
			{
				if ( bUseLowQualityShadows )
					return NVIDIA_PCF_CHEAP;				// NVIDIA hardware bilinear PCF
				else
					return NVIDIA_PCF;						// NVIDIA hardware PCF with larger kernel
			}

			if ( m_Caps.m_VendorID == VENDORID_ATI )
			{
				// PS30 shaders purposely don't support ATI_NOPCF to reduce the combo permutation space.
				if ( ( !bPS30 ) && ( bUseLowQualityShadows ) )
				{
					return ATI_NOPCF;						// Don't bother with a cheap Fetch 4
				}
				else
				{
					static bool bForceATIFetch4 = CommandLine()->CheckParm( "-forceatifetch4" ) ? true : false;

					// Either PS30, or high quality shadows.
					if ( m_Caps.m_bDX10Card && !bForceATIFetch4 )
						return ( bUseLowQualityShadows ) ? NVIDIA_PCF_CHEAP : NVIDIA_PCF;					// ATI wants us to run NVIDIA PCF on DX10 parts (this is the common case)
					else if ( m_Caps.m_bSupportsFetch4 )
						return ATI_NO_PCF_FETCH4;			// ATI fetch4 depth texture sampling
					else if ( bPS30 )
					{
						// We can't return ATI_NOPCF when using PS30 shaders. (This path should actually never get hit - either we're on a DX10 card or its fetch10 capable, I think.)
						return ATI_NO_PCF_FETCH4;
					}
					else
					{
						return ATI_NOPCF;					// ATI vanilla depth texture sampling
					}
				}
			}
			break;

		default:
			return SHADOWFILTERMODE_DEFAULT;
	}
#endif

	return SHADOWFILTERMODE_DEFAULT;
}

#if defined( CSTRIKE15 ) && defined( _X360 )
static ConVar r_shader_srgb( "r_shader_srgb", "0", 0, "-1 = use hardware caps. 0 = use hardware srgb. 1 = use shader srgb(software lookup)" );		// -1=use caps 0=off 1=on
static ConVar r_shader_srgbread( "r_shader_srgbread", "1", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#else
static ConVar r_shader_srgb( "r_shader_srgb", "0", 0, "-1 = use hardware caps. 0 = use hardware srgb. 1 = use shader srgb(software lookup)" );		// -1=use caps 0=off 1=on
static ConVar r_shader_srgbread( "r_shader_srgbread", "0", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#endif

int CHardwareConfig::NeedsShaderSRGBConversion() const
{
	if ( IsX360() )
	{
#if defined( CSTRIKE15 )
		// [mariod] TODO - tidy up the use of this (now mostly obsolete) convar after PAX
		if( r_shader_srgbread.GetBool() )
		{
			return false;
		}
#else
		// 360 always now uses a permanent hw solution
		return false;
#endif
	}

	if ( IsPS3() )
	{
		// PS3 natively supports srgb in hardware
		return false;
	}

	int cValue = r_shader_srgb.GetInt();
	switch( cValue )
	{
		case 0:
			return false;

		case 1:
			return true;

		default:
			return m_ActualCaps.m_bDX10Blending;			// !!! change to return false after portal depot built!!!!!
	}
}

bool CHardwareConfig::UsesSRGBCorrectBlending() const
{
	int cValue = r_shader_srgb.GetInt();
	return ( cValue == 0 ) && ( ( m_ActualCaps.m_bDX10Blending ) || IsX360() );
}

static ConVar mat_disablehwmorph( "mat_disablehwmorph", "0", FCVAR_DEVELOPMENTONLY, "Disables HW morphing for particular mods" );
static int s_bEnableFastVertexTextures = -1;
static bool s_bDisableHWMorph = false;
bool CHardwareConfig::HasFastVertexTextures() const
{
	// NOTE: This disallows you to change mat_disablehwmorph on the fly
	if ( s_bEnableFastVertexTextures < 0 )
	{
		s_bEnableFastVertexTextures = 1;
		if ( CommandLine()->FindParm( "-disallowhwmorph" ) )
		{
			s_bEnableFastVertexTextures = 0;
		}
		s_bDisableHWMorph = ( mat_disablehwmorph.GetInt() != 0 );
	}

	return ( s_bEnableFastVertexTextures != 0 ) && ( !s_bDisableHWMorph ) && ( GetDXSupportLevel() >= 100 );
}

bool CHardwareConfig::ActualHasFastVertexTextures() const
{
	// NOTE: This disallows you to change mat_disablehwmorph on the fly
	if ( s_bEnableFastVertexTextures < 0 )
	{
		s_bEnableFastVertexTextures = 1;
		if ( CommandLine()->FindParm( "-disallowhwmorph" ) )
		{
			s_bEnableFastVertexTextures = 0;
		}
		s_bDisableHWMorph = ( mat_disablehwmorph.GetInt() != 0 );
	}

	return ( s_bEnableFastVertexTextures != 0 ) && ( !s_bDisableHWMorph ) && ( GetMaxDXSupportLevel() >= 100 );
}

int CHardwareConfig::MaxHWMorphBatchCount() const
{
	return ShaderUtil()->MaxHWMorphBatchCount();
}

int CHardwareConfig::MaximumAnisotropicLevel() const
{
	return m_Caps.m_nMaxAnisotropy;
}

int CHardwareConfig::MaxTextureWidth() const
{
	return m_Caps.m_MaxTextureWidth;
}

int CHardwareConfig::MaxTextureHeight() const
{
	return m_Caps.m_MaxTextureHeight;
}

int	CHardwareConfig::TextureMemorySize() const
{
	return m_Caps.m_TextureMemorySize;
}

bool CHardwareConfig::SupportsMipmappedCubemaps() const
{
	return m_Caps.m_SupportsMipmappedCubemaps;
}

int	 CHardwareConfig::NumVertexShaderConstants() const
{
	return m_Caps.m_NumVertexShaderConstants;
}

int	 CHardwareConfig::NumBooleanVertexShaderConstants() const
{
	return m_Caps.m_NumBooleanVertexShaderConstants;
}

int	 CHardwareConfig::NumIntegerVertexShaderConstants() const
{
	return m_Caps.m_NumIntegerVertexShaderConstants;
}

int	 CHardwareConfig::NumPixelShaderConstants() const
{
	return m_Caps.m_NumPixelShaderConstants;
}

int	 CHardwareConfig::NumBooleanPixelShaderConstants() const
{
	return m_Caps.m_NumBooleanPixelShaderConstants;
}

int	 CHardwareConfig::NumIntegerPixelShaderConstants() const
{
	return m_Caps.m_NumIntegerPixelShaderConstants;
}

int	 CHardwareConfig::MaxNumLights() const
{
	return m_Caps.m_MaxNumLights;
}

int CHardwareConfig::MaxTextureAspectRatio() const
{
	return m_Caps.m_MaxTextureAspectRatio;
}

int	 CHardwareConfig::MaxVertexShaderBlendMatrices() const
{
	return m_Caps.m_MaxVertexShaderBlendMatrices;
}

// Useful for testing fastclip on Windows
extern ConVar mat_fastclip;

int CHardwareConfig::MaxUserClipPlanes() const
{
	if ( mat_fastclip.GetBool() )
		return 0;

	return m_Caps.m_MaxUserClipPlanes;
}

bool CHardwareConfig::UseFastClipping() const
{
	// rbarris broke this up for easier view of outcome in debugger
	bool fastclip = mat_fastclip.GetBool();
	
	bool result = m_Caps.m_UseFastClipping || fastclip;
	
	return result;
}

int CHardwareConfig::MaxTextureDepth() const
{
	return m_Caps.m_MaxTextureDepth;
}

int CHardwareConfig::GetDXSupportLevel() const
{
	return m_Caps.m_nDXSupportLevel;
}

const char *CHardwareConfig::GetShaderDLLName() const
{
	return ( m_Caps.m_pShaderDLL && m_Caps.m_pShaderDLL[0] ) ? m_Caps.m_pShaderDLL : "DEFAULT";
}

bool CHardwareConfig::ReadPixelsFromFrontBuffer() const
{
	if ( IsX360() )
	{
		// future proof safety, not allowing the front read path
		return false;
	}

	// GR - in DX 9.0a can blit from MSAA back buffer
	return false;
}

bool CHardwareConfig::PreferDynamicTextures() const
{
	if ( IsX360() )
	{
		// future proof safety, not allowing these
		return false;
	}

	return m_Caps.m_PreferDynamicTextures;
}

bool CHardwareConfig::SupportsHDR() const
{
	// This is a deprecated function. . use GetHDRType instead.  For shipping HL2, this always being false is correct.
	Assert( 0 );
	return false;
}

bool CHardwareConfig::SupportsHDRMode( HDRType_t nHDRType ) const
{
	switch( nHDRType )
	{
		case HDR_TYPE_NONE:
			return true;

		case HDR_TYPE_INTEGER:
			return ( m_Caps.m_MaxHDRType == HDR_TYPE_INTEGER ) || ( m_Caps.m_MaxHDRType == HDR_TYPE_FLOAT );

		case HDR_TYPE_FLOAT:
			return ( m_Caps.m_MaxHDRType == HDR_TYPE_FLOAT );
			
	}
	return false;

}

bool CHardwareConfig::NeedsAAClamp() const
{
	return false;
}

bool CHardwareConfig::NeedsATICentroidHack() const
{
	return m_Caps.m_bNeedsATICentroidHack;
}

// This is the max dx support level supported by the card
int CHardwareConfig::GetMaxDXSupportLevel() const
{
	return m_ActualCaps.m_nMaxDXSupportLevel;
}

int	CHardwareConfig::GetMinDXSupportLevel() const
{
	return ( developer.GetInt() > 0 ) ? 90 : m_ActualCaps.m_nMinDXSupportLevel;
}

bool CHardwareConfig::SpecifiesFogColorInLinearSpace() const
{
	return m_Caps.m_bFogColorSpecifiedInLinearSpace;
}

bool CHardwareConfig::SupportsSRGB() const
{
	return m_Caps.m_SupportsSRGB;
}

bool CHardwareConfig::FakeSRGBWrite() const
{
	return m_Caps.m_FakeSRGBWrite;
}

bool CHardwareConfig::CanDoSRGBReadFromRTs() const
{
	return m_Caps.m_CanDoSRGBReadFromRTs;
}

bool CHardwareConfig::SupportsGLMixedSizeTargets() const
{
	return m_Caps.m_bSupportsGLMixedSizeTargets;
}

bool CHardwareConfig::IsAAEnabled() const
{
	return g_pShaderDevice ? g_pShaderDevice->IsAAEnabled() : false;
//	bool bAntialiasing = ( m_PresentParameters.MultiSampleType != D3DMULTISAMPLE_NONE );
//	return bAntialiasing;
}

int CHardwareConfig::GetMaxVertexTextureDimension() const
{
	return m_Caps.m_nMaxVertexTextureDimension;
}

HDRType_t CHardwareConfig::GetHDRType() const
{
	// On MacOS, this value comes down from the engine, which read it from the registry...which doesn't exist on Mac, so we're slamming to true here
	if ( IsOpenGL() )
	{
		g_pHardwareConfig->SetHDREnabled( true );
	}

	bool enabled = m_bHDREnabled;
	int dxlev = GetDXSupportLevel();
	int dxsupp = dxlev >= 90;
	HDRType_t caps_hdr = m_Caps.m_HDRType;
	HDRType_t result = HDR_TYPE_NONE;
	
	//printf("\nCHardwareConfig::GetHDRType...");
	if (enabled)
	{
		//printf("-> enabled...");
		if (dxsupp)
		{
			//printf("-> supported...");
			result = caps_hdr;
		}
	}
	
	//printf("-> result is %d.\n", result);
	return result;

/*
	if ( m_bHDREnabled && ( GetDXSupportLevel() >= 90 ) )
		return m_Caps.m_HDRType;
	return HDR_TYPE_NONE;
*/
}

float CHardwareConfig::GetLightMapScaleFactor( void ) const
{
#ifdef _PS3
	// PS3 uses floating point lightmaps but not the full HDR_TYPE_FLOAT codepath
	return 1.0f;
#else // _PS3
	switch( GetHDRType() )
	{
	case HDR_TYPE_FLOAT:
		return 1.0;
		break;

	case HDR_TYPE_INTEGER:
		return 16.0;

	case HDR_TYPE_NONE:
	default:
		return GammaToLinearFullRange( 2.0 );	// light map scale
	}
#endif // !_PS3
}

HDRType_t CHardwareConfig::GetHardwareHDRType() const
{
	return m_Caps.m_HDRType;
}

bool CHardwareConfig::SupportsStreamOffset() const
{
	return m_Caps.m_bSupportsStreamOffset;
}

int CHardwareConfig::StencilBufferBits() const
{
	return g_pShaderDevice ? g_pShaderDevice->StencilBufferBits() : 0;
}

int CHardwareConfig:: MaxViewports() const
{
	return m_Caps.m_nMaxViewports;
}

int CHardwareConfig::GetActualSamplerCount() const
{
	return m_ActualCaps.m_NumSamplers;
}

int CHardwareConfig::GetActualVertexSamplerCount() const
{
	return m_ActualCaps.m_NumVertexSamplers;
}

const char *CHardwareConfig::GetHWSpecificShaderDLLName()	const
{
	return m_Caps.m_pShaderDLL && m_Caps.m_pShaderDLL[0] ? m_Caps.m_pShaderDLL : NULL;
}

bool CHardwareConfig::SupportsShadowDepthTextures( void ) const
{
	return m_Caps.m_bSupportsShadowDepthTextures;
}

ImageFormat CHardwareConfig::GetShadowDepthTextureFormat( void ) const
{
	return m_Caps.m_ShadowDepthTextureFormat;
}

ImageFormat CHardwareConfig::GetHighPrecisionShadowDepthTextureFormat( void ) const
{
	return m_Caps.m_HighPrecisionShadowDepthTextureFormat;
}

ImageFormat CHardwareConfig::GetNullTextureFormat( void ) const
{
	return m_Caps.m_NullTextureFormat;
}

bool CHardwareConfig::SupportsCascadedShadowMapping( void ) const
{
#if defined(_PS3) 
	return m_Caps.m_bSupportsCascadedShadowMapping;
#elif defined(_X360)
	return m_Caps.m_bSupportsCascadedShadowMapping;
#else
    return m_Caps.m_bSupportsCascadedShadowMapping && ( GetDXSupportLevel() >= 95 );
#endif
}

CSMQualityMode_t CHardwareConfig::GetCSMQuality( void ) const
{
#if defined( _X360 ) || defined( _PS3 )
	return CSMQUALITY_VERY_LOW;
#else
	return (CSMQualityMode_t)m_Caps.m_nCSMQuality;
#endif
}

bool CHardwareConfig::SupportsBilinearPCFSampling() const
{
	if( IsOpenGL() || IsPS3() || IsX360() )
		return true;

	if ( ( m_Caps.m_VendorID == VENDORID_NVIDIA ) || ( m_Caps.m_VendorID == VENDORID_INTEL ) )
		return true;
	
	static bool bForceATIFetch4 = CommandLine()->CheckParm( "-forceatifetch4" ) ? true : false;
	if ( bForceATIFetch4 )
		return false;

	// Non-DX10 class ATI cards (pre-X2000) don't support bilinear PCF in hardware.
	if ( ( m_Caps.m_VendorID == VENDORID_ATI ) && ( m_Caps.m_bDX10Card ) )
		return true;

	return false;
}

// Returns the CSM static combo to select given the current card's capablities and the configured CSM quality level.
CSMShaderMode_t CHardwareConfig::GetCSMShaderMode( CSMQualityMode_t nQualityLevel ) const
{
#if defined( _X360 ) || defined( _PS3 )
	return CSMSHADERMODE_LOW_OR_VERY_LOW;
#endif

	// Special case for ATI DX9-class (pre ATI HD 2xxx) cards that don't support NVidia-style PCF filtering - always set to CSMSHADERMODE_ATIFETCH4.
	if ( !SupportsBilinearPCFSampling() )
		return CSMSHADERMODE_ATIFETCH4;

	int nMode = nQualityLevel - 1;
	if ( nMode < CSMSHADERMODE_LOW_OR_VERY_LOW )
		nMode = CSMSHADERMODE_LOW_OR_VERY_LOW;
	else if ( nMode > CSMSHADERMODE_HIGH )
		nMode = CSMSHADERMODE_HIGH;

	return static_cast< CSMShaderMode_t >( nMode );
}

bool CHardwareConfig::GetCSMAccurateBlending( void ) const
{
	return m_bCSMAccurateBlending;
}

void CHardwareConfig::SetCSMAccurateBlending( bool bEnable )
{
	m_bCSMAccurateBlending = bEnable;
}

bool CHardwareConfig::SupportsResolveDepth( void ) const
{
	static ConVarRef mat_resolveFullFrameDepth( "mat_resolveFullFrameDepth" );
	static ConVarRef gpu_level( "gpu_level" );

	if ( ( gpu_level.GetInt() >= 2 ) &&
		 ( mat_resolveFullFrameDepth.GetInt() == 1 ) )
	{
#if defined(DX_TO_GL_ABSTRACTION)
		{
			if ( gGL->m_bHave_GL_EXT_framebuffer_blit )
			{
				return true;
			}
			else
			{
				return false;
			}
		}
#else
		{
			if ( g_pHardwareConfig->ActualCaps().m_bSupportsINTZ &&
				 ( g_pHardwareConfig->ActualCaps().m_bSupportsRESZ || ( g_pHardwareConfig->ActualCaps().m_VendorID == VENDORID_NVIDIA ) ) )
			{
				return true;
			}
			else
			{
				return false;
			}
		}
#endif
	}
	else
	{
		return false;
	}
}

bool CHardwareConfig::HasFullResolutionDepthTexture(void) const
{
	static ConVarRef mat_resolveFullFrameDepth( "mat_resolveFullFrameDepth" );

	if ( SupportsResolveDepth() || ( mat_resolveFullFrameDepth.GetInt() == 2 ) )
	{
		return true;
	}
	else
	{
		return false;
	}
}

#ifdef _PS3
#include "hardwareconfig_ps3nonvirt.h"
#include "hardwareconfig_ps3nonvirt.inl"
#endif
