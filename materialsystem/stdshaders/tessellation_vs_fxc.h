//========== Copyright (c) Valve Corporation, All rights reserved. ==========//
//
// Purpose: Common code for tessellation
//
// $NoKeywords: $
//
//===========================================================================//


#ifndef TESSELLATION_VS_FXC_H_
#define TESSELLATION_VS_FXC_H_
#ifdef SHADER_MODEL_VS_3_0

#define TESSELLATION_MODE_ACC_PATCHES_EXTRA	1
#define TESSELLATION_MODE_ACC_PATCHES_REG	2

struct VS_INPUT
{
	float2 UV				: POSITION0;	// Cartesian UV coordinates
	float4 BasisU			: TEXCOORD0;	// BasisU ( precalculated Bernstein basis functions for U )
	float4 BasisV			: TEXCOORD1;	// BasisV ( precalculated Bernstein basis functions for V )
	
	float4 V0_TanU			: POSITION1;	// Superprim vertex 0	
	float4 V0_tc01			: TEXCOORD2;
	float4 V0_tc23			: TEXCOORD3;
	
	float4 V1_TanU			: POSITION2;	// Superprim vertex 1
	float4 V1_tc01			: TEXCOORD4;
	float4 V1_tc23			: TEXCOORD5;
	
	float4 V2_TanU			: POSITION3;	// Superprim vertex 2
	float4 V2_tc01			: TEXCOORD6;
	float4 V2_tc23			: TEXCOORD7;
	
	float4 V3_TanU			: POSITION4;	// Superprim vertex 3
	float4 V3_tc01			: TEXCOORD8;
	float4 V3_tc23			: TEXCOORD9;
	
	float PatchID		    : TEXCOORD10;	// ID for this patch
};

void LoadACCPatchPos( float patchIndex, out float3 Bez[16],
					  float flOneOverSubDHeight, sampler2D sampSubD )
{
	float idx = ( patchIndex + 0.5 ) * flOneOverSubDHeight;
	[unroll]
    for (int i = 0; i < 4; i++)
    {
        float4 tmp[3];

        tmp[0] = tex2Dlod( sampSubD, float4((i * 3 + 0.5) / 30, idx, 0, 0) );
        tmp[1] = tex2Dlod( sampSubD, float4((i * 3 + 1.5) / 30, idx, 0, 0) );
        tmp[2] = tex2Dlod( sampSubD, float4((i * 3 + 2.5) / 30, idx, 0, 0) );

        Bez[4 * i + 0] = tmp[0].xyz;
        Bez[4 * i + 1] = float3( tmp[0].w, tmp[1].xy );
        Bez[4 * i + 2] = float3( tmp[1].zw, tmp[2].x );
        Bez[4 * i + 3] = tmp[2].yzw;
    }
}

void LoadACCPatchTan( float patchIndex, out float3 TanU[12], out float3 TanV[12], 
					  float flOneOverSubDHeight, sampler2D sampSubD )
{
	float idx = ( patchIndex + 0.5 ) * flOneOverSubDHeight;
	
	// Tangents
	[unroll]
    for (int i = 0; i < 3; i++)
    {
        float4 tmp[3];

        tmp[0] = tex2Dlod(sampSubD, float4((i * 3 + 0.5 + 12) / 30, idx, 0, 0));
        tmp[1] = tex2Dlod(sampSubD, float4((i * 3 + 1.5 + 12) / 30, idx, 0, 0));
        tmp[2] = tex2Dlod(sampSubD, float4((i * 3 + 2.5 + 12) / 30, idx, 0, 0));

        TanU[4 * i + 0] = tmp[0].xyz;
        TanU[4 * i + 1] = float3( tmp[0].w, tmp[1].xy );
        TanU[4 * i + 2] = float3( tmp[1].zw, tmp[2].x );
        TanU[4 * i + 3] = tmp[2].yzw;
    }
    
    // Tangents
	[unroll]
    for (int i = 0; i < 3; i++)
    {
        float4 tmp[3];

        tmp[0] = tex2Dlod(sampSubD, float4((i * 3 + 0.5 + 21) / 30, idx, 0, 0));
        tmp[1] = tex2Dlod(sampSubD, float4((i * 3 + 1.5 + 21) / 30, idx, 0, 0));
        tmp[2] = tex2Dlod(sampSubD, float4((i * 3 + 2.5 + 21) / 30, idx, 0, 0));

        TanV[4 * i + 0] = tmp[0].xyz;
        TanV[4 * i + 1] = float3( tmp[0].w, tmp[1].xy );
        TanV[4 * i + 2] = float3( tmp[1].zw, tmp[2].x );
        TanV[4 * i + 3] = tmp[2].yzw;
    }
}

void EvaluateCubicACCPosPatch( in float4 BasisU, in float4 BasisV, float2 UV, float3 cpP[16], out float3 pos )
{
	pos  = (BasisU.x * cpP[ 0] + BasisU.y * cpP[ 1] + BasisU.z * cpP[ 2] + BasisU.w * cpP[ 3]) * BasisV.x + 
		   (BasisU.x * cpP[ 4] + BasisU.y * cpP[ 5] + BasisU.z * cpP[ 6] + BasisU.w * cpP[ 7]) * BasisV.y + 
		   (BasisU.x * cpP[ 8] + BasisU.y * cpP[ 9] + BasisU.z * cpP[10] + BasisU.w * cpP[11]) * BasisV.z + 
		   (BasisU.x * cpP[12] + BasisU.y * cpP[13] + BasisU.z * cpP[14] + BasisU.w * cpP[15]) * BasisV.w;

}

//--------------------------------------------------------------------------------------
// Cubic Bernstein basis functions
// http://mathworld.wolfram.com/BernsteinPolynomial.html
//--------------------------------------------------------------------------------------
float4 BernsteinBasis( float t )
{
	float invT = 1.0f-t;
	return float4( invT*invT*invT, 3.0*t*invT*invT,	3.0*t*t*invT, t*t*t );
}

float3 BersteinBasisQuad( float t )
{
	float invT = 1.0f-t;
	return float3( invT * invT,	2 * invT * t, t * t );
}

void EvaluateCubicACCTanPatches( in float4 BasisU, in float4 BasisV, float2 UV, float3 cpU[12], float3 cpV[12], 
								 out float3 tanU, out float3 tanV )
{
	// quadratic bernstein basis functions
	float3 qBasisU = BersteinBasisQuad( UV.x );
	float3 qBasisV = BersteinBasisQuad( UV.y );

	tanU = (qBasisU.x * cpU[ 0] + qBasisU.y * cpU[ 1] + qBasisU.z * cpU[ 2]) * BasisV.x + 
		   (qBasisU.x * cpU[ 3] + qBasisU.y * cpU[ 4] + qBasisU.z * cpU[ 5]) * BasisV.y + 
		   (qBasisU.x * cpU[ 6] + qBasisU.y * cpU[ 7] + qBasisU.z * cpU[ 8]) * BasisV.z + 
		   (qBasisU.x * cpU[ 9] + qBasisU.y * cpU[10] + qBasisU.z * cpU[11]) * BasisV.w;

	tanV = (BasisU.x * cpV[ 0] + BasisU.y * cpV[ 1] + BasisU.z * cpV[ 2] + BasisU.w * cpV[ 3]) * qBasisV.x + 
		   (BasisU.x * cpV[ 4] + BasisU.y * cpV[ 5] + BasisU.z * cpV[ 6] + BasisU.w * cpV[ 7]) * qBasisV.y + 
		   (BasisU.x * cpV[ 8] + BasisU.y * cpV[ 9] + BasisU.z * cpV[10] + BasisU.w * cpV[11]) * qBasisV.z;
}

// We define a patch owner for each edge and vertex of the mesh.
// When sampling a displacement map on the boundaries and corners, owner coords are used
//
//  Each patch stores:      The superprim verts can store
//                          all of this data like so:
//    -- patch U -->
//  |							X        Y        Z        W
//	p	t3|t2 t1|t3			+-----------------------------------+
//	a	--0-----1--			|  tanX  |  tanY  |  tanZ  | sBWrnk | <- Binormal sign flip bit and wrinkle weight
//	t	t1|t0 t0|t2			+-----------------------------------+
//	c	  |     |			| innerU | innerV | edgeVU | edgeVV |
//	h	t2|t0 t0|t1			+-----------------------------------+
//	|	--3-----2--			| edgeUU | edgeUV | cornerU| cornerV| 
//	V	t3|t1 t2|t3			+-----------------------------------+
//
float2 ComputeConsistentDisplacementUVs( float2 UV,
										 float4 V0_tc01, float4 V0_tc23,
										 float4 V1_tc01, float4 V1_tc23,
										 float4 V2_tc01, float4 V2_tc23,
										 float4 V3_tc01, float4 V3_tc23 )
{
	// Use the tie-breaking scheme for sampling texture coordinates to avoid cracking
	float2 t0[4], t1[4], t2[4], t3[4];
	
	t0[0] = V0_tc01.xy;
	t0[1] = V0_tc01.zw;
	t0[2] = V0_tc23.xy;
	t0[3] = V0_tc23.zw;
	
	t1[0] = V1_tc01.xy;
	t1[1] = V1_tc01.zw;
	t1[2] = V1_tc23.xy;
	t1[3] = V1_tc23.zw;
	
	t2[0] = V2_tc01.xy;
	t2[1] = V2_tc01.zw;
	t2[2] = V2_tc23.xy;
	t2[3] = V2_tc23.zw;
	
	t3[0] = V3_tc01.xy;
	t3[1] = V3_tc01.zw;
	t3[2] = V3_tc23.xy;
	t3[3] = V3_tc23.zw;
	
	float flMaxUV = 0.99;
	float flMinUV = 0.01;

	int i0 = 2 * (UV.x < flMinUV) +     (UV.y < flMinUV);
	int i1 =     (UV.x > flMaxUV) + 2 * (UV.y < flMinUV);
	int i2 = 2 * (UV.x > flMaxUV) +     (UV.y > flMaxUV);
	int i3 =     (UV.x < flMinUV) + 2 * (UV.y > flMaxUV);

	float2 bottom = lerp( t0[i0], t1[i1], UV.x );
    float2 top = lerp( t3[i3], t2[i2], UV.x );
    return lerp( bottom, top, UV.y );
}

void DeCasteljau(float u, float3 p0, float3 p1, float3 p2, float3 p3, out float3 p)
{
	float3 q0, q1, q2;
	float3 r0, r1;

	[isolate]
	{
		q0 = lerp( p0, p1, u );
		q1 = lerp( p1, p2, u );
		q2 = lerp( p2, p3, u );
		r0 = lerp( q0, q1, u );
		r1 = lerp( q1, q2, u );
		p  = lerp( r0, r1, u );
	}
}

void DeCasteljau(float u, float3 p0, float3 p1, float3 p2, float3 p3, out float3 p, out float3 dp)
{
	float3 q0, q1, q2;
	float3 r0, r1;

	[isolate]
	{
		q0 = lerp( p0, p1, u );
		q1 = lerp( p1, p2, u );
		q2 = lerp( p2, p3, u );
		r0 = lerp( q0, q1, u );
		r1 = lerp( q1, q2, u );
		p  = lerp( r0, r1, u );
	}

	dp = r0 - r1;
}

void EvaluateBezierRegular( float2 uv, float3 p[16], out float3 pos, out float3 nor )
{
      float3 t0, t1, t2, t3;
      float3 p0, p1, p2, p3;

	  [isolate]
	  {
		  DeCasteljau( uv.x, p[ 0], p[ 1], p[ 2], p[ 3], p0, t0 );
		  DeCasteljau( uv.x, p[ 4], p[ 5], p[ 6], p[ 7], p1, t1 );
		  DeCasteljau( uv.x, p[ 8], p[ 9], p[10], p[11], p2, t2 );
		  DeCasteljau( uv.x, p[12], p[13], p[14], p[15], p3, t3 );
	  }

      float3 du, dv;
      DeCasteljau( uv.y, p0, p1, p2, p3, pos, dv );
      DeCasteljau( uv.y, t0, t1, t2, t3, du );

      nor = normalize( cross(3 * dv, 3 * du) );
}

void EvaluateBezierPosition( float2 uv, float3 p[16], out float3 pos )
{
	float3 t0, t1, t2, t3;
	float3 p0, p1, p2, p3;

	[isolate]
	{
		DeCasteljau( uv.x, p[ 0], p[ 1], p[ 2], p[ 3], p0, t0 );
		DeCasteljau( uv.x, p[ 4], p[ 5], p[ 6], p[ 7], p1, t1 );
		DeCasteljau( uv.x, p[ 8], p[ 9], p[10], p[11], p2, t2 );
		DeCasteljau( uv.x, p[12], p[13], p[14], p[15], p3, t3 );
	}
	DeCasteljau( uv.y, p0, p1, p2, p3, pos );
}

void EvaluateSubdivisionSurface( const VS_INPUT v, float flOneOverSubDHeight, float flDoDisplacement, float flDoWrinkledDisplacements,
								 sampler2D BezierSampler, sampler2D sampDisplacement,

							     // Outputs
							     out float3 vWorldNormal, out float3 vWorldPos, 
							     out float3 vWorldTangentS, out float3 vWorldTangentT, out float flBiTangentSign,
							     out float flWrinkleWeight, 
							     out float2 vTexUV, out float2 vPatchUV,
							   
							     bool bTangentFrame = true )
{
	float4 vInTan;
	float2 vDispUV;
	float3 vPatchTangent;
	float3 vPatchBiTangent;
	float4 vBasisU;
	float4 vBasisV;
	float flPatchLoadIndex;

	// PatchUV is passed in for us
	vPatchUV  = v.UV;

	// compute values for tangent based on patchUV
	float4 TanUbottom = lerp( v.V0_TanU, v.V1_TanU, vPatchUV.x );
	float4 TanUtop = lerp( v.V3_TanU, v.V2_TanU, vPatchUV.x );
	vInTan = lerp( TanUbottom, TanUtop, vPatchUV.y );

	// compute values for texcoord based on patchUV
	float2 bottom = lerp( v.V0_tc01.xy, v.V1_tc01.xy, vPatchUV.x );
	float2 top = lerp( v.V3_tc01.xy, v.V2_tc01.xy, vPatchUV.x );
	vTexUV = lerp( bottom, top, vPatchUV.y ); 

	// Compute consistent displacement UVs for crack-free displacement mapping
	vDispUV = ComputeConsistentDisplacementUVs( vPatchUV,
												v.V0_tc01, v.V0_tc23,
												v.V1_tc01, v.V1_tc23,
												v.V2_tc01, v.V2_tc23,
												v.V3_tc01, v.V3_tc23 );

	// Cubic Bernstein basis coefficients are passed in for us
	vBasisU = v.BasisU;
	vBasisV = v.BasisV;

	// Patch load index is passed in for us
	flPatchLoadIndex = v.PatchID;

	float3 ControlPoints[16];
	LoadACCPatchPos( flPatchLoadIndex, ControlPoints, flOneOverSubDHeight, BezierSampler );

#if ( TESSELLATION == TESSELLATION_MODE_ACC_PATCHES_REG )
	EvaluateBezierRegular( vPatchUV, ControlPoints, vWorldPos, vWorldNormal );
#else
	// We split the loading and evaluation of Position patches and Tangent patches to reduce temp register pressure.

	// Load and evaluation position
//	EvaluateCubicACCPosPatch( vBasisU, vBasisV, vPatchUV, ControlPoints, vWorldPos );
	EvaluateBezierPosition( vPatchUV, ControlPoints, vWorldPos );

	// Load and evaluate tangent patches
	float3 ControlPointsU[12], ControlPointsV[12];
	LoadACCPatchTan( flPatchLoadIndex, ControlPointsU, ControlPointsV, flOneOverSubDHeight, BezierSampler );
	EvaluateCubicACCTanPatches( vBasisU, vBasisV, vPatchUV, ControlPointsU, ControlPointsV, vPatchTangent, vPatchBiTangent );

	vWorldNormal = normalize( cross( vPatchBiTangent, vPatchTangent ) );							// Compute world normal
#endif

	// Up to three scalar displacements for { Neutral, Compress, Stretch }
	float3 vDisplacement = tex2Dlod( sampDisplacement, float4( vDispUV, 0, 0 ) );

	flBiTangentSign = sign( vInTan.w );

	if ( bTangentFrame )
	{
		vWorldTangentS = normalize( vInTan.xyz - ( vWorldNormal * dot( vInTan.xyz, vWorldNormal ) ) );	// Orthonormalize superprim tangent
		vWorldTangentT = cross( vWorldNormal, vWorldTangentS.xyz ) * flBiTangentSign;					// Sign encodes Binormal flip
	}
	else
	{
		vWorldTangentS = vWorldTangentT = vWorldNormal;
	}

	flWrinkleWeight = abs( vInTan.w ) - 2.0f;			// Convert wrinkle weight to -1 to 1 range for pixel shader to use

	float3 vDispCoeff = float3(0,0,0);					// { Neutral, Compress, Stretch } Displacement Coefficients
	vDispCoeff.y = saturate( -flWrinkleWeight );		// One of these two is zero
	vDispCoeff.z = saturate(  flWrinkleWeight );		// while the other is in the 0..1 range
	vDispCoeff *= flDoWrinkledDisplacements;			// Separate control for presence of wrinkled displacements (just multiplying by 0 or 1 here)

	vDispCoeff.x = 1.0f - vDispCoeff.y - vDispCoeff.z;	// Derive neutral weight since these all sum to one

	// Displace along normal, using wrinkle displacement map coefficients
	vWorldPos += vWorldNormal * ( flDoDisplacement * dot( vDisplacement, vDispCoeff ) );
}


// Wrapper for no-tangent-frame, no-wrinkle version
void EvaluateSubdivisionSurface( VS_INPUT v, float flOneOverSubDHeight, float flDoDisplacement, float flDoWrinkledDisplacements,
								 sampler2D BezierSampler, sampler2D DispSampler,

								 // Outputs
								 out float3 vWorldNormal, out float3 vWorldPos,
								 out float2 vUV, out float2 vPatchUV )
{
	float3 vDummyA, vDummyB;
	float flDummyWrinkle;
	float flDummyBinormalFlip;
	EvaluateSubdivisionSurface( v, flOneOverSubDHeight, flDoDisplacement, flDoWrinkledDisplacements,
								BezierSampler, DispSampler, vWorldNormal, vWorldPos, vDummyA, vDummyB,
								flDummyBinormalFlip, flDummyWrinkle, vUV, vPatchUV, false );

}


#endif // SHADER_MODEL_VS_3_0

#endif //#ifndef TESSELLATION_VS_FXC_H_
