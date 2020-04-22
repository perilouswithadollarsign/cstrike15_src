//===== Copyright (c) 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: shader for drawing sprites as cards, with animation frame lerping
//
// $Header: $
// $NoKeywords: $
//===========================================================================//

#include "BaseVSShader.h"
#include "convar.h"

// STDSHADER_DX9_DLL_EXPORT
#include "spritecard_ps20.inc"
#include "spritecard_ps20b.inc"
#include "spritecard_vs20.inc"
#include "splinecard_vs20.inc"
#include "common_hlsl_cpp_consts.h"

#include "tier0/icommandline.h" //command line


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define DEFAULT_PARTICLE_FEATHERING_ENABLED 1

static ConVar mat_depthfeather_enable( "mat_depthfeather_enable", "1", FCVAR_DEVELOPMENTONLY );

#if defined( CSTRIKE15 ) && defined( _X360 )
static ConVar r_shader_srgbread( "r_shader_srgbread", "1", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#else
static ConVar r_shader_srgbread( "r_shader_srgbread", "0", 0, "1 = use shader srgb texture reads, 0 = use HW" );
#endif

int GetDefaultDepthFeatheringValue( void ) //Allow the command-line to go against the default soft-particle value
{
	static int iRetVal = -1;

	if ( iRetVal == -1 )
	{
		#if ( DEFAULT_PARTICLE_FEATHERING_ENABLED == 1 )
		{
			if ( CommandLine()->CheckParm( "-softparticlesdefaultoff" ) )
				iRetVal = 0;
			else
				iRetVal = 1;
		}
		#else
		{
			if ( CommandLine()->CheckParm( "-softparticlesdefaulton" ) )
				iRetVal = 1;
			else
				iRetVal = 0;
		}
		#endif
	}

	// On low end parts on the Mac, we reduce particles and shut off depth blending here
	static ConVarRef mat_reduceparticles( "mat_reduceparticles" );
	if ( mat_reduceparticles.GetBool() )
	{
		iRetVal = 0;
	}

	return iRetVal;
}


BEGIN_VS_SHADER_FLAGS( Spritecard, "Help for Spritecard", SHADER_NOT_EDITABLE )

	BEGIN_SHADER_PARAMS
        SHADER_PARAM( DEPTHBLEND, SHADER_PARAM_TYPE_INTEGER, "0", "fade at intersection boundaries" )
		SHADER_PARAM( SCENEDEPTH, SHADER_PARAM_TYPE_TEXTURE, "", "" )
		SHADER_PARAM( DEPTHBLENDSCALE, SHADER_PARAM_TYPE_FLOAT, "50.0", "Amplify or reduce DEPTHBLEND fading. Lower values make harder edges." )
		SHADER_PARAM( INVERSEDEPTHBLEND, SHADER_PARAM_TYPE_BOOL, "0", "calculate 1-depthblendalpha so that sprites appear when they are near geometry" )
	    SHADER_PARAM( ORIENTATION, SHADER_PARAM_TYPE_INTEGER, "0", "0 = always face camera, 1 = rotate around z, 2= parallel to ground 3=use normal 4=face camera POS" )
		SHADER_PARAM( AIMATCAMERA, SHADER_PARAM_TYPE_BOOL, "0", "Aim at camera using orientation type 1" )
	    SHADER_PARAM( ADDBASETEXTURE2, SHADER_PARAM_TYPE_FLOAT, "0.0", "amount to blend second texture into frame by" )
	    SHADER_PARAM( OVERBRIGHTFACTOR, SHADER_PARAM_TYPE_FLOAT, "1.0", "overbright factor for texture. For HDR effects.")
	    SHADER_PARAM( DUALSEQUENCE, SHADER_PARAM_TYPE_INTEGER, "0", "blend two separate animated sequences.")
	    SHADER_PARAM( SEQUENCE_BLEND_MODE, SHADER_PARAM_TYPE_INTEGER, "0", "defines the blend mode between the images un dual sequence particles. 0 = avg, 1=alpha from first, rgb from 2nd, 2= first over second" )
		SHADER_PARAM( MAXLUMFRAMEBLEND1, SHADER_PARAM_TYPE_INTEGER, "0", "instead of blending between animation frames for the first sequence, select pixels based upon max luminance" )
		SHADER_PARAM( MAXLUMFRAMEBLEND2, SHADER_PARAM_TYPE_INTEGER, "0", "instead of blending between animation frames for the 2nd sequence, select pixels based upon max luminance" )
		SHADER_PARAM( RAMPTEXTURE, SHADER_PARAM_TYPE_TEXTURE, "", "if specified, then the red value of the image is used to index this ramp to produce the output color" )
	    SHADER_PARAM( ZOOMANIMATESEQ2, SHADER_PARAM_TYPE_FLOAT, "1.0", "amount to gradually zoom between frames on the second sequence. 2.0 will double the size of a frame over its lifetime.")
	    SHADER_PARAM( EXTRACTGREENALPHA, SHADER_PARAM_TYPE_INTEGER, "0", "grayscale data sitting in green/alpha channels")
		SHADER_PARAM( ADDOVERBLEND, SHADER_PARAM_TYPE_INTEGER, "0", "use ONE:INVSRCALPHA blending")
	    SHADER_PARAM( ADDSELF, SHADER_PARAM_TYPE_FLOAT, "0.0", "amount of base texture to additively blend in" )
	    SHADER_PARAM( BLENDFRAMES, SHADER_PARAM_TYPE_BOOL, "1", "whether or not to smoothly blend between animated frames" )
	    SHADER_PARAM( MINSIZE, SHADER_PARAM_TYPE_FLOAT, "0.0", "minimum screen fractional size of particle")
	    SHADER_PARAM( STARTFADESIZE, SHADER_PARAM_TYPE_FLOAT, "10.0", "screen fractional size to start fading particle out")
	    SHADER_PARAM( ENDFADESIZE, SHADER_PARAM_TYPE_FLOAT, "20.0", "screen fractional size to finish fading particle out")
	    SHADER_PARAM( MAXSIZE, SHADER_PARAM_TYPE_FLOAT, "20.0", "maximum screen fractional size of particle")
	    SHADER_PARAM( USEINSTANCING, SHADER_PARAM_TYPE_BOOL, "1", "whether to use GPU vertex instancing (submit 1 vert per particle quad)")
	    SHADER_PARAM( SPLINETYPE, SHADER_PARAM_TYPE_INTEGER, "0", "spline type 0 = none,  1=ctamull rom")
	    SHADER_PARAM( MAXDISTANCE, SHADER_PARAM_TYPE_FLOAT, "100000.0", "maximum distance to draw particles at")
	    SHADER_PARAM( FARFADEINTERVAL, SHADER_PARAM_TYPE_FLOAT, "400.0", "interval over which to fade out far away particles")
		SHADER_PARAM( SHADERSRGBREAD360, SHADER_PARAM_TYPE_BOOL, "0", "Simulate srgb read in shader code")
		SHADER_PARAM( ORIENTATIONMATRIX, SHADER_PARAM_TYPE_MATRIX, "center .5 .5 scale 1 1 rotate 0 translate 0 0", "Matrix used to orient in orientation mode #2" )
		SHADER_PARAM( MOD2X, SHADER_PARAM_TYPE_BOOL, "0", "whether or not to multiply the result of the pixel shader * 2 against the framebuffer" )
	    SHADER_PARAM( ALPHATRAILFADE, SHADER_PARAM_TYPE_FLOAT, "1", "Amount to scale alpha by between start and end of trail/rope" )
	    SHADER_PARAM( RADIUSTRAILFADE, SHADER_PARAM_TYPE_FLOAT, "1", "Amount to scale radis by between start and end of trail/rope" )
		SHADER_PARAM( SHADOWDEPTH,SHADER_PARAM_TYPE_INTEGER, "0", "writing to a shadow depth buffer" )
		SHADER_PARAM( OPAQUE, SHADER_PARAM_TYPE_BOOL, "0", "Are we opaque? (defaults to 0)" )
		SHADER_PARAM( CROPFACTOR,SHADER_PARAM_TYPE_VEC2, "[1 1]", "writing to a shadow depth buffer" )
		SHADER_PARAM( VERTEXCOLORLERP, SHADER_PARAM_TYPE_BOOL, "0", "Enable computing vertex color by interpolating two color based on vertex r color channel" )
		SHADER_PARAM( LERPCOLOR1, SHADER_PARAM_TYPE_VEC3, "[1 0 0]", "Lerp color 1" )
		SHADER_PARAM( LERPCOLOR2, SHADER_PARAM_TYPE_VEC3, "[0 1 0]", "Lerp color 2" )
        SHADER_PARAM( VERTEXFOGAMOUNT, SHADER_PARAM_TYPE_FLOAT, "1", "Amount of vertex fog to apply" )

       // distance outline control
        SHADER_PARAM( DISTANCEALPHA, SHADER_PARAM_TYPE_BOOL, "0", "Use distance-coded alpha generated from hi-res texture by vtex.")

		SHADER_PARAM( SOFTEDGES, SHADER_PARAM_TYPE_BOOL, "0", "Enable soft edges to distance coded textures.")
	    SHADER_PARAM( EDGESOFTNESSSTART, SHADER_PARAM_TYPE_FLOAT, "0.6", "Start value for soft edges for distancealpha.");
		SHADER_PARAM( EDGESOFTNESSEND, SHADER_PARAM_TYPE_FLOAT, "0.5", "End value for soft edges for distancealpha.");

		SHADER_PARAM( OUTLINE, SHADER_PARAM_TYPE_BOOL, "0", "Enable outline for distance coded textures.")
		SHADER_PARAM( OUTLINECOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "color of outline for distance coded images." )
		SHADER_PARAM( OUTLINEALPHA, SHADER_PARAM_TYPE_FLOAT, "0.0", "alpha value for outline")
		SHADER_PARAM( OUTLINESTART0, SHADER_PARAM_TYPE_FLOAT, "0.0", "outer start value for outline")
		SHADER_PARAM( OUTLINESTART1, SHADER_PARAM_TYPE_FLOAT, "0.0", "inner start value for outline")
		SHADER_PARAM( OUTLINEEND0, SHADER_PARAM_TYPE_FLOAT, "0.0", "inner end value for outline")
		SHADER_PARAM( OUTLINEEND1, SHADER_PARAM_TYPE_FLOAT, "0.0", "outer end value for outline")
		SHADER_PARAM( PERPARTICLEOUTLINE, SHADER_PARAM_TYPE_BOOL, "0", "Allow per particle outline control" )

		SHADER_PARAM( MULOUTPUTBYALPHA, SHADER_PARAM_TYPE_BOOL, "0", "Multiply output RGB by output alpha to avoid precision problems" );
		SHADER_PARAM( INTENSITY, SHADER_PARAM_TYPE_FLOAT, "1.0", "Multiply output RGB by intensity factor" );
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		INIT_FLOAT_PARM( MAXDISTANCE, 100000.0 );
		INIT_FLOAT_PARM( FARFADEINTERVAL, 400.0 );
		INIT_FLOAT_PARM( MAXSIZE, 20.0 );
		INIT_FLOAT_PARM( ENDFADESIZE, 20.0 );
		INIT_FLOAT_PARM( STARTFADESIZE, 10.0 );
		INIT_FLOAT_PARM( DEPTHBLENDSCALE, 50.0 );
		INIT_FLOAT_PARM( OVERBRIGHTFACTOR, 1.0 );
		INIT_FLOAT_PARM( ADDBASETEXTURE2, 0.0 );
		INIT_FLOAT_PARM( ADDSELF, 0.0 );
		INIT_FLOAT_PARM( ZOOMANIMATESEQ2, 0.0 );
		INIT_FLOAT_PARM( ALPHATRAILFADE, 1. );
		INIT_FLOAT_PARM( RADIUSTRAILFADE, 1. );
		INIT_FLOAT_PARM( VERTEXFOGAMOUNT, 0.0f );
		INIT_FLOAT_PARM( OUTLINEALPHA, 1.0 );

		if ( !params[ORIENTATIONMATRIX]->IsDefined() )				
		{
			VMatrix mat;
			MatrixSetIdentity( mat );
			params[ORIENTATIONMATRIX]->SetMatrixValue( mat );
		}

		if ( !params[CROPFACTOR]->IsDefined() )				
		{
			params[CROPFACTOR]->SetVecValue( 1.0f, 1.0f );
		}

		if ( !params[DEPTHBLEND]->IsDefined() )
		{
			params[ DEPTHBLEND ]->SetIntValue( GetDefaultDepthFeatheringValue() );
		}
		if ( !g_pHardwareConfig->SupportsPixelShaders_2_b() )
		{
			params[ DEPTHBLEND ]->SetIntValue( 0 );
		}
		InitIntParam( DUALSEQUENCE, params, 0 );
		InitIntParam( MAXLUMFRAMEBLEND1, params, 0 );
		InitIntParam( MAXLUMFRAMEBLEND2, params, 0 );
		InitIntParam( EXTRACTGREENALPHA, params, 0 );
		InitIntParam( ADDOVERBLEND, params, 0 );
		InitIntParam( BLENDFRAMES, params, 1 );
		
		InitIntParam( DISTANCEALPHA, params, 0 );
		InitIntParam( OUTLINE, params, 0 );
		InitIntParam( SOFTEDGES, params, 0 );
		InitIntParam( PERPARTICLEOUTLINE, params, 0 );
		InitIntParam( MULOUTPUTBYALPHA, params, 0 );
		InitFloatParam( INTENSITY, params, 1.0f );
			

		if ( !params[USEINSTANCING]->IsDefined() )
		{
			params[ USEINSTANCING ]->SetIntValue( IsX360() ? 1 : 0 );
		}

		// srgb read 360
		InitIntParam( SHADERSRGBREAD360, params, 0 );

		// default to being translucent since that's what we always were for historical reasons.
		InitIntParam( OPAQUE, params, 0 );

		InitIntParam( VERTEXCOLORLERP, params, 0 );
		if ( !params[LERPCOLOR1]->IsDefined() )
		{
			params[LERPCOLOR1]->SetVecValue( 1.0f, 0.0f, 0.0f );
		}
		if ( !params[LERPCOLOR2]->IsDefined() )
		{
			params[LERPCOLOR2]->SetVecValue( 0.0f, 1.0f, 0.0f );
		}

		if ( params[OPAQUE]->GetIntValue() != 0 )
		{
			// none of these make sense if we have $opaque 1:
			params[ADDBASETEXTURE2]->SetFloatValue( 0.0f );
			params[DUALSEQUENCE]->SetIntValue( 0 );
			params[SEQUENCE_BLEND_MODE]->SetIntValue( 0 );
			params[MAXLUMFRAMEBLEND1]->SetIntValue( 0 );
			params[MAXLUMFRAMEBLEND2]->SetIntValue( 0 );
			params[EXTRACTGREENALPHA]->SetIntValue( 0 );
			params[RAMPTEXTURE]->SetUndefined();
			params[ZOOMANIMATESEQ2]->SetIntValue( 0 );
			params[ADDOVERBLEND]->SetIntValue( 0 );
			params[ADDSELF]->SetIntValue( 0 );
			params[BLENDFRAMES]->SetIntValue( 0 );
			params[DEPTHBLEND]->SetIntValue( 0 );
			params[INVERSEDEPTHBLEND]->SetIntValue( 0 );
		}

		if ( IsPS3() && !params[SCENEDEPTH]->IsDefined() )
		{
			params[SCENEDEPTH]->SetStringValue( "^PS3^DEPTHBUFFER" );
		}

		if ( g_pHardwareConfig->HasFullResolutionDepthTexture() )
		{
			params[SCENEDEPTH]->SetStringValue( "_rt_FullFrameDepth" );
		}

		SET_FLAGS2( MATERIAL_VAR2_IS_SPRITECARD );
	}

	SHADER_FALLBACK
	{
		return 0;
	}

	SHADER_INIT
	{
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );

		if ( params[BASETEXTURE]->IsDefined() )
		{
			bool bExtractGreenAlpha = false;
			if ( params[EXTRACTGREENALPHA]->IsDefined() )
			{
				bExtractGreenAlpha = params[EXTRACTGREENALPHA]->GetIntValue() != 0;
			}

			LoadTexture( BASETEXTURE, bExtractGreenAlpha ? 0 : TEXTUREFLAGS_SRGB  );
		}
		if ( params[RAMPTEXTURE]->IsDefined() )
		{
			LoadTexture( RAMPTEXTURE, TEXTUREFLAGS_SRGB );
		}

		if( IsPS3() && params[SCENEDEPTH]->IsDefined() )
		{
			LoadTexture( SCENEDEPTH, 0 );
		}

		if ( g_pHardwareConfig->HasFullResolutionDepthTexture() )
		{
			LoadTexture( SCENEDEPTH, 0 );
		}
	}

	SHADER_DRAW
	{
		bool bUseRampTexture = ( params[RAMPTEXTURE]->IsDefined() );
		bool bZoomSeq2 = ( ( params[ZOOMANIMATESEQ2]->GetFloatValue()) > 1.0 );
		bool bDepthBlend = false;

		if ( g_pHardwareConfig->HasFullResolutionDepthTexture() )
		{
			// If we didn't create the depth texture.  We are looking at not using depth feathering to save memory.
			static bool bNoDepthTexture = ( CommandLine()->FindParm( "-nodepthtexture" ) ) ? true : false;
			if ( !bNoDepthTexture )
			{
				bDepthBlend = ( params[DEPTHBLEND]->GetIntValue() != 0 ) && mat_depthfeather_enable.GetBool();
			}
		}
		bool bInverseDepthBlend = ( params[INVERSEDEPTHBLEND]->GetIntValue() != 0 );
		bool bAdditive2ndTexture = params[ADDBASETEXTURE2]->GetFloatValue() != 0.0;
		bool bExtractGreenAlpha = ( params[EXTRACTGREENALPHA]->GetIntValue() != 0 );
		int nSplineType = params[SPLINETYPE]->GetIntValue();
		bool bUseInstancing = IsX360() ? ( params[ USEINSTANCING ]->GetIntValue() != 0 ) : false;
#if defined( CSTRIKE15 )
		bool bShaderSrgbRead = IsX360() && r_shader_srgbread.GetBool();
#else
		bool bShaderSrgbRead = ( IsX360() && IS_PARAM_DEFINED( SHADERSRGBREAD360 ) && params[SHADERSRGBREAD360]->GetIntValue() );
#endif
		bool bCrop = ( params[CROPFACTOR]->GetVecValue()[0] != 1.0f ) || ( params[CROPFACTOR]->GetVecValue()[1] != 1.0f );
		bool bSecondSequence = params[DUALSEQUENCE]->GetIntValue() != 0;
		bool bBlendFrames = ( params[BLENDFRAMES]->GetIntValue() != 0 );

		bool bDistanceAlpha = ( params[DISTANCEALPHA]->GetIntValue() != 0 );
		bool bOutLine = bDistanceAlpha && ( params[OUTLINE]->GetIntValue() != 0 );
		bool bSoftEdges = bDistanceAlpha && ( params[OUTLINE]->GetIntValue() != 0 );
		bool bPerParticleOutline = bDistanceAlpha && ( !bSecondSequence ) && ( params[PERPARTICLEOUTLINE]->GetIntValue() );
		float flIntensity = params[INTENSITY]->GetFloatValue();
							
		if ( nSplineType )
		{
			bDepthBlend = false; //splinecard_vsxx.fxc doesn't output all the data necessary for depth blending
			bUseInstancing = false;
			bBlendFrames = false;
		}

		bool bColorLerpPS = ( params[VERTEXCOLORLERP]->GetIntValue() != 0 );
		bool bPackedInterpolator = bColorLerpPS && !( bExtractGreenAlpha || bSecondSequence || bBlendFrames || params[MAXLUMFRAMEBLEND1]->GetIntValue() );

		bool bFog = params[VERTEXFOGAMOUNT]->GetFloatValue() != 0;
		SHADOW_STATE
		{
			bool bAddOverBlend = params[ADDOVERBLEND]->GetIntValue() != 0;
			bool bMod2X = params[MOD2X]->GetIntValue() != 0;
			bool bShadowDepth = ( params[SHADOWDEPTH]->GetIntValue() != 0 );
			bool bAddSelf = params[ADDSELF]->GetFloatValue() != 0.0;
			if ( bFog )
			{
				pShaderShadow->FogMode( SHADER_FOGMODE_FOGCOLOR, true );
			}

			// draw back-facing because of yaw spin
			pShaderShadow->EnableCulling( false );

			// Don't write to dest alpha.
			pShaderShadow->EnableAlphaWrites( false );

			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );

			if ( bUseRampTexture )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, !bShaderSrgbRead );
			}
			
			if ( bDepthBlend )
			{
				pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			}

			if ( bAdditive2ndTexture || bAddSelf )
				pShaderShadow->EnableAlphaTest( false );
			else
				pShaderShadow->EnableAlphaTest( true );

			pShaderShadow->AlphaFunc( SHADER_ALPHAFUNC_GREATER, 1 / 255 );

			if ( bMod2X )
			{
				EnableAlphaBlending( SHADER_BLEND_DST_COLOR, SHADER_BLEND_SRC_COLOR );
			}
			else if ( bAdditive2ndTexture || bAddOverBlend || bAddSelf )
			{
				EnableAlphaBlending( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}
			else
			{
				if ( IS_FLAG_SET(MATERIAL_VAR_ADDITIVE) )
				{
					EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE );
				}
				else
				{
					EnableAlphaBlending( SHADER_BLEND_SRC_ALPHA, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
				}
			}

			bool bUseNormal = ( params[ORIENTATION]->GetIntValue() == 3 );
			unsigned int flags = VERTEX_POSITION | VERTEX_COLOR;
			static int s_TexCoordSize[8]={4,				// 0 = sheet bounding uvs, frame0
										  4,				// 1 = sheet bounding uvs, frame 1
										  4,				// 2 = frame blend, rot, radius, ???
										  2,				// 3 = corner identifier ( 0/0,1/0,1/1, 1/0 )
										  4,				// 4 = texture 2 bounding uvs
										  4,				// 5 = second sequence bounding uvs, frame0.. or glow tint, or normal
										  4,				// 6 = second sequence bounding uvs, frame1
										  4,				// 7 = second sequence frame blend, ?,?,?
			};
			static int s_TexCoordSizeSpline[]={4,			// 0 = xyz rad pt0
											   4,			// 1 = xyz rad pt1
											   4,			// 2 = xyz rad pt2
											   4,			// 3 = xyz rad pt3
											   4,			// 4 = texture range u0,v0-u1,v1
											   4,			// 5 = rgba pt1
											   3,			// 6 = normal0
											   3,			// 7 = normal1

			};

			int numTexCoords = 5;
			if ( nSplineType )
			{
				numTexCoords += 1;								// need second rope color
				if ( bUseNormal )
					numTexCoords += 2;							// need normal vectors	
			}
			else
			{
				if ( bPerParticleOutline || bUseNormal )
				{
					numTexCoords = 6;
				}
				else
				{
					if ( bSecondSequence )
					{
						// the whole shebang - 2 sequences, with a possible multi-image sequence first
						numTexCoords = 8;
					}
				}
			}
			pShaderShadow->VertexShaderVertexFormat( flags,
													 numTexCoords, 
													 nSplineType? s_TexCoordSizeSpline : s_TexCoordSize, 0 );

			if ( nSplineType )
			{
				DECLARE_STATIC_VERTEX_SHADER( splinecard_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( ADDBASETEXTURE2, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( EXTRACTGREENALPHA, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( DUALSEQUENCE, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( DEPTHBLEND, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( PACKED_INTERPOLATOR, 0 );
				SET_STATIC_VERTEX_SHADER_COMBO( ANIMBLEND_OR_MAXLUMFRAMEBLEND1, bBlendFrames || ( params[MAXLUMFRAMEBLEND1]->GetIntValue() != 0 ) );
				SET_STATIC_VERTEX_SHADER( splinecard_vs20 );
			}
			else
			{
				DECLARE_STATIC_VERTEX_SHADER( spritecard_vs20 );
				SET_STATIC_VERTEX_SHADER_COMBO( SPRITECARDVERTEXFOG, bFog );
				SET_STATIC_VERTEX_SHADER_COMBO( DUALSEQUENCE, bSecondSequence );
				SET_STATIC_VERTEX_SHADER_COMBO( ZOOM_ANIMATE_SEQ2, bZoomSeq2 );
				SET_STATIC_VERTEX_SHADER_COMBO( ADDBASETEXTURE2, bAdditive2ndTexture );
				SET_STATIC_VERTEX_SHADER_COMBO( EXTRACTGREENALPHA, bExtractGreenAlpha );
				SET_STATIC_VERTEX_SHADER_COMBO( DEPTHBLEND, bDepthBlend );
				SET_STATIC_VERTEX_SHADER_COMBO( ANIMBLEND_OR_MAXLUMFRAMEBLEND1, bBlendFrames || ( params[MAXLUMFRAMEBLEND1]->GetIntValue() != 0 ) );
				SET_STATIC_VERTEX_SHADER_COMBO( CROP, bCrop );
				SET_STATIC_VERTEX_SHADER_COMBO( PACKED_INTERPOLATOR, bPackedInterpolator );
				SET_STATIC_VERTEX_SHADER_COMBO( HARDWAREFOGBLEND, !IsX360() && bFog && ( g_pHardwareConfig->GetDXSupportLevel() <= 90 ) );
				SET_STATIC_VERTEX_SHADER_COMBO( PERPARTICLEOUTLINE, bPerParticleOutline );
				SET_STATIC_VERTEX_SHADER( spritecard_vs20 );
			}

			bool bMulOutputByAlpha = params[MULOUTPUTBYALPHA]->GetIntValue() != 0;

			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_STATIC_PIXEL_SHADER( spritecard_ps20b );
				SET_STATIC_PIXEL_SHADER_COMBO( ADDBASETEXTURE2, bAdditive2ndTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( ADDSELF, bAddSelf );
				SET_STATIC_PIXEL_SHADER_COMBO( ANIMBLEND, bBlendFrames );
				SET_STATIC_PIXEL_SHADER_COMBO( DUALSEQUENCE, bSecondSequence );
				SET_STATIC_PIXEL_SHADER_COMBO( SEQUENCE_BLEND_MODE, bSecondSequence ? params[SEQUENCE_BLEND_MODE]->GetIntValue() : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( MAXLUMFRAMEBLEND1, params[MAXLUMFRAMEBLEND1]->GetIntValue() );
				SET_STATIC_PIXEL_SHADER_COMBO( MAXLUMFRAMEBLEND2, bSecondSequence? params[MAXLUMFRAMEBLEND1]->GetIntValue() : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( COLORRAMP, bUseRampTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( EXTRACTGREENALPHA, bExtractGreenAlpha );
				SET_STATIC_PIXEL_SHADER_COMBO( DEPTHBLEND, bDepthBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( INVERSEDEPTHBLEND, bInverseDepthBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
				SET_STATIC_PIXEL_SHADER_COMBO( MOD2X, bMod2X );
				SET_STATIC_PIXEL_SHADER_COMBO( PACKED_INTERPOLATOR, bPackedInterpolator );
				SET_STATIC_PIXEL_SHADER_COMBO( COLOR_LERP_PS, bColorLerpPS );
				SET_STATIC_PIXEL_SHADER_COMBO( DISTANCEALPHA, bDistanceAlpha );
				SET_STATIC_PIXEL_SHADER_COMBO( OUTLINE, bOutLine );
				SET_STATIC_PIXEL_SHADER_COMBO( SOFTEDGES, bSoftEdges );
				SET_STATIC_PIXEL_SHADER_COMBO( MULOUTPUTBYALPHA, bMulOutputByAlpha );
				SET_STATIC_PIXEL_SHADER( spritecard_ps20b );
			}
			else
			{
				DECLARE_STATIC_PIXEL_SHADER( spritecard_ps20 );
				SET_STATIC_PIXEL_SHADER_COMBO( ADDBASETEXTURE2, bAdditive2ndTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( DUALSEQUENCE, bSecondSequence );
				SET_STATIC_PIXEL_SHADER_COMBO( ADDSELF, bAddSelf );
				SET_STATIC_PIXEL_SHADER_COMBO( ANIMBLEND, bBlendFrames );
				SET_STATIC_PIXEL_SHADER_COMBO( SEQUENCE_BLEND_MODE, bSecondSequence ? params[SEQUENCE_BLEND_MODE]->GetIntValue() : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( MAXLUMFRAMEBLEND1, params[MAXLUMFRAMEBLEND1]->GetIntValue() );
				SET_STATIC_PIXEL_SHADER_COMBO( MAXLUMFRAMEBLEND2, bSecondSequence? params[MAXLUMFRAMEBLEND1]->GetIntValue() : 0 );
				SET_STATIC_PIXEL_SHADER_COMBO( COLORRAMP, bUseRampTexture );
				SET_STATIC_PIXEL_SHADER_COMBO( EXTRACTGREENALPHA, bExtractGreenAlpha );
				SET_STATIC_PIXEL_SHADER_COMBO( INVERSEDEPTHBLEND, bInverseDepthBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( SHADER_SRGB_READ, bShaderSrgbRead );
				SET_STATIC_PIXEL_SHADER_COMBO( MOD2X, bMod2X );
				SET_STATIC_PIXEL_SHADER_COMBO( DEPTHBLEND, bDepthBlend );
				SET_STATIC_PIXEL_SHADER_COMBO( PACKED_INTERPOLATOR, bPackedInterpolator );
				SET_STATIC_PIXEL_SHADER_COMBO( COLOR_LERP_PS, bColorLerpPS );
				SET_STATIC_PIXEL_SHADER_COMBO( DISTANCEALPHA, bDistanceAlpha );
				SET_STATIC_PIXEL_SHADER_COMBO( OUTLINE, bOutLine );
				SET_STATIC_PIXEL_SHADER_COMBO( SOFTEDGES, bSoftEdges );
				SET_STATIC_PIXEL_SHADER_COMBO( MULOUTPUTBYALPHA, bMulOutputByAlpha );
				SET_STATIC_PIXEL_SHADER( spritecard_ps20 );
			}

			if ( bShadowDepth )
			{
				pShaderShadow->EnableColorWrites( false );
				pShaderShadow->EnableAlphaWrites( false );
				pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_SHADOW_BIAS );
			}
			pShaderShadow->EnableSRGBWrite( true );

			if( !bExtractGreenAlpha )
			{
				pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, !bShaderSrgbRead );
			}
		}
		DYNAMIC_STATE
		{
			BindTexture( SHADER_SAMPLER0, SRGBReadMask( (! bExtractGreenAlpha ) && ( ! bShaderSrgbRead ) ), BASETEXTURE, FRAME );

			if ( bUseRampTexture )
			{
				BindTexture( SHADER_SAMPLER1, SRGBReadMask( !bShaderSrgbRead ), RAMPTEXTURE, FRAME );
			}

			if ( bDepthBlend )
			{
				BindTexture( SHADER_SAMPLER2, TEXTURE_BINDFLAGS_NONE, SCENEDEPTH, -1 );
			}

			int nOrientation = params[ORIENTATION]->GetIntValue();
			nOrientation = clamp( nOrientation, 0, 4 );

			if ( nOrientation == 1 && params[AIMATCAMERA]->GetIntValue() > 0 )
				nOrientation = 4;

			switch ( nOrientation )
			{
			case 0:
				// We need these only when screen-orienting
				LoadModelViewMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0 );
				LoadProjectionMatrixIntoVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_3 );
				break;

			case 2:
				// We need this only when doing fixed orientation
				SetVertexShaderMatrix3x4( VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, ORIENTATIONMATRIX );
				break;
			}

			if ( bZoomSeq2 || bFog )
			{
				float flZScale=1.0/(params[ZOOMANIMATESEQ2]->GetFloatValue());
				float C0[4]={ 0.5*(1.0+flZScale), flZScale, params[VERTEXFOGAMOUNT]->GetFloatValue(), 0 };
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, C0, ARRAYSIZE(C0)/4 );
			}

			// set fade constants in vsconsts 8 and 9
			float flMaxDistance = params[MAXDISTANCE]->GetFloatValue();
			float flStartFade = MAX( 1.0, flMaxDistance - params[FARFADEINTERVAL]->GetFloatValue() );

			float VC0[8]={ params[MINSIZE]->GetFloatValue(), params[MAXSIZE]->GetFloatValue(),
						   params[STARTFADESIZE]->GetFloatValue(), params[ENDFADESIZE]->GetFloatValue(),
						   flStartFade, 1.0/(flMaxDistance-flStartFade),
						   params[ALPHATRAILFADE]->GetFloatValue(),
						   params[RADIUSTRAILFADE]->GetFloatValue() };

			pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_8, VC0, ARRAYSIZE(VC0)/4 );

			if ( bDepthBlend )
			{
				pShaderAPI->SetDepthFeatheringShaderConstants( 2, params[DEPTHBLENDSCALE]->GetFloatValue() );
			}

			// Get viewport and render target dimensions and set shader constant to do a 2D mad
			int nViewportX, nViewportY, nViewportWidth, nViewportHeight;
			pShaderAPI->GetCurrentViewport( nViewportX, nViewportY, nViewportWidth, nViewportHeight );

			int nRtWidth, nRtHeight;
			pShaderAPI->GetCurrentRenderTargetDimensions( nRtWidth, nRtHeight );

			float vViewportMad[4];
			// Compute viewport mad that takes projection space coords (post divide by W) into normalized screenspace, taking into account the currently set viewport.
			vViewportMad[0] =  .5f * ( ( float )nViewportWidth / ( float )nRtWidth );
			vViewportMad[1] = -.5f * ( ( float )nViewportHeight / ( float )nRtHeight );
			vViewportMad[2] =  vViewportMad[0] + ( ( float )nViewportX / ( float )nRtWidth );
			vViewportMad[3] = -vViewportMad[1] + ( ( float )nViewportY / ( float )nRtHeight );
			pShaderAPI->SetPixelShaderConstant( DEPTH_FEATHER_VIEWPORT_MAD, vViewportMad, 1 );

			if ( bCrop )
			{
				float vCropFactors[4];
				params[CROPFACTOR]->GetVecValue( vCropFactors, 2 );
				vCropFactors[2] = -0.5f * vCropFactors[0] + 0.5f;
				vCropFactors[3] = -0.5f * vCropFactors[1] + 0.5f;
				pShaderAPI->SetVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_CONST_11, vCropFactors, 1 );
			}

			if ( bColorLerpPS )
			{
				float vLerpColors[8];
				params[LERPCOLOR1]->GetVecValue( vLerpColors, 3 );
				params[LERPCOLOR2]->GetVecValue( vLerpColors+4, 3 );
				vLerpColors[3] = 1.0f;
				vLerpColors[7] = 1.0f;
				SetPixelShaderConstantGammaToLinear( 5, vLerpColors, 2 );
			}

			float C0[4]={ params[ADDBASETEXTURE2]->GetFloatValue(), params[OVERBRIGHTFACTOR]->GetFloatValue(), params[ADDSELF]->GetFloatValue(), flIntensity };

			BOOL nBoolShaderConstant = bUseInstancing ? 1 : 0; // Convert to BOOL, which is int
			pShaderAPI->SetBooleanVertexShaderConstant( VERTEX_SHADER_SHADER_SPECIFIC_BOOL_CONST_0, &nBoolShaderConstant, 1 );
			pShaderAPI->SetPixelShaderConstant( 0, C0, ARRAYSIZE(C0)/4 );

			// Set Mod2xIdentity to be 0.5 if we blend in linear space, or 0.5 Gamma->Linear if we blend in gamma space
			float vPsConst1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			vPsConst1[0] = g_pHardwareConfig->UsesSRGBCorrectBlending() ? 0.5f : SrgbGammaToLinear( 0.5f );
			pShaderAPI->SetPixelShaderConstant( 1, vPsConst1, 1 );


			// set up distance-coding constants
			if ( bDistanceAlpha )
			{
				float vDistanceConstants[4 * 3];			// c7, c8,c9
				params[OUTLINECOLOR]->GetVecValue( vDistanceConstants, 3 );
				vDistanceConstants[3] = params[OUTLINEALPHA]->GetFloatValue();
				vDistanceConstants[4] = params[OUTLINESTART0]->GetFloatValue();
				vDistanceConstants[5] = params[OUTLINESTART1]->GetFloatValue();
				vDistanceConstants[6] = params[OUTLINEEND0]->GetFloatValue();
				vDistanceConstants[7] = params[OUTLINEEND1]->GetFloatValue();
				vDistanceConstants[8] = params[EDGESOFTNESSSTART]->GetFloatValue();
				vDistanceConstants[9] = params[EDGESOFTNESSEND]->GetFloatValue();
				vDistanceConstants[10] = 0;
				vDistanceConstants[11] = 0;
				pShaderAPI->SetPixelShaderConstant( 7, vDistanceConstants, ARRAYSIZE( vDistanceConstants ) / 4 );
			}

			if ( nSplineType )
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( splinecard_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( ORIENTATION, nOrientation );
				SET_DYNAMIC_VERTEX_SHADER( splinecard_vs20 );
			}
			else
			{
				DECLARE_DYNAMIC_VERTEX_SHADER( spritecard_vs20 );
				SET_DYNAMIC_VERTEX_SHADER_COMBO( ORIENTATION, nOrientation );
				SET_DYNAMIC_VERTEX_SHADER( spritecard_vs20 );
			}


			if( g_pHardwareConfig->SupportsPixelShaders_2_b() )
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( spritecard_ps20b );
				SET_DYNAMIC_PIXEL_SHADER( spritecard_ps20b );
			}
			else
			{
				DECLARE_DYNAMIC_PIXEL_SHADER( spritecard_ps20 );
				SET_DYNAMIC_PIXEL_SHADER( spritecard_ps20 );
			}
		}
		Draw( );
	}
END_SHADER
