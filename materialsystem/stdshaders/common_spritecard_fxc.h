#ifdef PIXELSHADER
	#define VS_OUTPUT PS_INPUT
#endif

#ifndef ADDBASETEXTURE2
	#error "missing define"
#endif

#ifndef EXTRACTGREENALPHA
	#error "missing define"
#endif

#ifndef ANIMBLEND
	#error "missing define"
#endif

#ifndef MAXLUMFRAMEBLEND1
	#error "missing define"
#endif

#ifndef DUALSEQUENCE
	#error "missing define"
#endif

#ifndef PACKED_INTERPOLATOR
	#error "missing define"
#endif

#define HAS_BLENDFACTOR0 ( ANIMBLEND || MAXLUMFRAMEBLEND1 || EXTRACTGREENALPHA || DUALSEQUENCE )

struct VS_OUTPUT
{
#ifndef PIXELSHADER
	float4 projPos			: POSITION;	
#endif

	float4 texCoord0_1	: TEXCOORD0;

#if ( PACKED_INTERPOLATOR == 0 )
	// in packed-interpolator case, texCoord0_1.zw store argbcolor.ra
	float4 argbcolor		: COLOR;
#endif

#if HAS_BLENDFACTOR0
	float4 blendfactor0		: TEXCOORD1;
#endif

#if ADDBASETEXTURE2
	float2 texCoord2		: TEXCOORD2;
#endif

#if	EXTRACTGREENALPHA
	float4 blendfactor1		: TEXCOORD3;
#endif

#if DUALSEQUENCE
	float4 vSeq2TexCoord0_1   : TEXCOORD4;
#else
    float4 vecOutlineTint : TEXCOORD4;
#endif

#if DEPTHBLEND
	float4 vProjPos		: TEXCOORD5;
#endif

#ifndef PIXELSHADER
	#if !defined( _X360 ) && !defined( SHADER_MODEL_VS_3_0 )
		float fog : FOG;
	#endif
#endif
};
