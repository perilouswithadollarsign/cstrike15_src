//
// Custom Fresnel with low, mid and high parameters defining a piecewise continuous function
// with traditional fresnel (0 to 1 range) as input.  The 0 to 0.5 range blends between
// low and mid while the 0.5 to 1 range blends between mid and high
//
//    |
//    |    .  M . . . H
//    | . 
//    L
//    |
//    +----------------
//    0               1
//
float FresnelHack( const float3 vNormal, const float3 vEyeDir, float3 vRanges, float fSpecMask )
{
	float result, f = Fresnel( vNormal, vEyeDir );			// Traditional Fresnel

	if ( f > 0.5f )
	{
		result = lerp( vRanges.y, vRanges.z, (2*f)-1 );		// Blend between mid and high values
	}
	else
	{
		result = lerp( fSpecMask * vRanges.x, vRanges.y, 2*f );			// Blend between low and mid values
	}

	return result;
}
