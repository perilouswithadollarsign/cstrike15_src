//===== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ======//
struct PS_IN
{
	float2 TexCoord : TEXCOORD;
};

float SrgbGammaToLinear( float flSrgbGammaValue )
{
	float x = saturate( flSrgbGammaValue );
	return ( x <= 0.04045f ) ? ( x / 12.92f ) : ( pow( ( x + 0.055f ) / 1.055f, 2.4f ) );
}


float X360LinearToGamma( float flLinearValue )
{
	float fl360GammaValue;
	flLinearValue = saturate( flLinearValue );
	if ( flLinearValue < ( 128.0f / 1023.0f ) )
	{
		if ( flLinearValue < ( 64.0f / 1023.0f ) )
		{
			fl360GammaValue = flLinearValue * ( 1023.0f * ( 1.0f / 255.0f ) );
		}
		else
		{
			fl360GammaValue = flLinearValue * ( ( 1023.0f / 2.0f ) * ( 1.0f / 255.0f ) ) + ( 32.0f / 255.0f );
		}
	}
	else
	{
		if ( flLinearValue < ( 512.0f / 1023.0f ) )
		{
			fl360GammaValue = flLinearValue * ( ( 1023.0f / 4.0f ) * ( 1.0f / 255.0f ) ) + ( 64.0f / 255.0f );
		}
		else
		{
			fl360GammaValue = flLinearValue * ( ( 1023.0f /8.0f ) * ( 1.0f / 255.0f ) ) + ( 128.0f /255.0f );
			if ( fl360GammaValue > 1.0f )
			{
				fl360GammaValue = 1.0f;
			}
		}
	}
	fl360GammaValue = saturate( fl360GammaValue );
	return fl360GammaValue;
}

sampler detail : register( s0 );

float4 main( PS_IN In ) : COLOR  
{  
	float4 vTextureColor = tex2D( detail, In.TexCoord );
	return vTextureColor;
};