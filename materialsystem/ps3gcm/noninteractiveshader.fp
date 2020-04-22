//===== Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ======//
struct PS_IN
{
	float2 TexCoord : TEXCOORD;
};

sampler detail : register( s0 );

float4 main( PS_IN In ) : COLOR  
{  
	return tex2D( detail, In.TexCoord );
}
