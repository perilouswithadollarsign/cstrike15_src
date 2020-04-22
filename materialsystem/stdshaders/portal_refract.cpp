//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//

#include "BaseVSShader.h"
#include "portal_refract_helper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DEFINE_FALLBACK_SHADER( PortalRefract, PortalRefract_dx9 )
BEGIN_VS_SHADER( PortalRefract_dx9, "PortalRefract" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( STAGE, SHADER_PARAM_TYPE_INTEGER, "0", "Stage of portal rendering (0, 1, 2)" )
		SHADER_PARAM( PORTALOPENAMOUNT, SHADER_PARAM_TYPE_FLOAT, "0.0", "Portal open amount 0.0-1.0" )
		SHADER_PARAM( PORTALSTATIC, SHADER_PARAM_TYPE_FLOAT, "0.0", "Portal static amount 0.0-1.0" )
		SHADER_PARAM( PORTALMASKTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Mask texture" )
		SHADER_PARAM( TEXTURETRANSFORM, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "Texcoord transform" )
		SHADER_PARAM( PORTALCOLORTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "Color texture" )
		SHADER_PARAM( PORTALCOLORGRADIENTDARK, SHADER_PARAM_TYPE_COLOR, "[0.0 0.0 0.0]", "The dark end of a tint gradient if not using a color texture" )
		SHADER_PARAM( PORTALCOLORGRADIENTLIGHT, SHADER_PARAM_TYPE_COLOR, "[1.0 1.0 1.0]", "The light end of a tint gradient if not using a color texture" )
		SHADER_PARAM( PORTALCOLORSCALE, SHADER_PARAM_TYPE_FLOAT, "0.0", "Portal color scale" )
		SHADER_PARAM( TIME, SHADER_PARAM_TYPE_FLOAT, "0.0", "Needs CurrentTime Proxy" )
		SHADER_PARAM( USEONSTATICPROP, SHADER_PARAM_TYPE_BOOL, "0", "Activate special mode to use this shader on a static prop" )
	END_SHADER_PARAMS

	void SetupVarsPortalRefract( PortalRefractVars_t &info )
	{
		info.m_nStage = STAGE;
		info.m_nPortalOpenAmount = PORTALOPENAMOUNT;
		info.m_nPortalStatic = PORTALSTATIC;
		info.m_nPortalMaskTexture = PORTALMASKTEXTURE;
		info.m_nTextureTransform = TEXTURETRANSFORM;
		info.m_nPortalColorTexture = PORTALCOLORTEXTURE;
		info.m_nPortalColorGradientDark = PORTALCOLORGRADIENTDARK;
		info.m_nPortalColorGradientLight = PORTALCOLORGRADIENTLIGHT;
		info.m_nPortalColorScale = PORTALCOLORSCALE;
		info.m_nTime = TIME;
		info.m_nUseOnStaticProp = USEONSTATICPROP;
	}

	bool NeedsPowerOfTwoFrameBufferTexture( IMaterialVar **params, bool bCheckSpecificToThisFrame ) const 
	{
		// For setting model flag at load time
		if ( bCheckSpecificToThisFrame == false )
			return true;

		// We only need the texture for stage 0
		if ( params[STAGE]->GetIntValue() == 0 )
		{
			//Msg( "Portal Refract @ %f time\n", params[TIME]->GetFloatValue() );
			return true;
		}
		else
		{
			return false;
		}
	}

	SHADER_INIT_PARAMS()
	{
		PortalRefractVars_t info;
		SetupVarsPortalRefract( info );
		InitParamsPortalRefract( this, params, pMaterialName, info );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		PortalRefractVars_t info;
		SetupVarsPortalRefract( info );
		InitPortalRefract( this, params, info );
	}

	SHADER_DRAW
	{
		// Skip drawing if in stage 0 and the portal isn't partially opened
		bool bDraw = true;
		if ( ( params[STAGE]->GetIntValue() == 0 ) && ( ( params[PORTALOPENAMOUNT]->GetFloatValue() <= 0.0f ) || ( params[PORTALOPENAMOUNT]->GetFloatValue() >= 1.0f ) ) ) // If in stage 0 and not partially opened
		{
			bDraw = false;
		}

		// If ( snapshotting ) or ( we need to draw this frame )
		if ( ( pShaderShadow != NULL ) || ( bDraw == true ) )
		{
			PortalRefractVars_t info;
			SetupVarsPortalRefract( info );
			DrawPortalRefract( this, params, pShaderAPI, pShaderShadow, info, vertexCompression );
		}
		else // We're not snapshotting and we don't need to draw this frame
		{
			// Skip this pass!
			Draw( false );
		}
	}
END_SHADER
