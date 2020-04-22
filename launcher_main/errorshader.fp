/*   SCE CONFIDENTIAL                                       */
/*   PlayStation(R)3 Programmer Tool Runtime Library 350.001 */
/*   Copyright (C) 2006 Sony Computer Entertainment Inc.    */
/*   All Rights Reserved.                                   */

void main
(
	float4 color_in      : COLOR,
	float4 normal_in     : TEX0,
	out float4 color_out : COLOR
)
{
	color_out = color_in * max( 0.15, 0.15 + ( dot( normalize( normal_in ), float4( 0.577,0.577,-0.577,0 ) ) ) ) 
		//+ float4( 0.2 * abs( normal_in.xyz ), 0 )
		;
}
