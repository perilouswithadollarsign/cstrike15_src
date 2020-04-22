#include "r_studiosubd_patches.h"
#include "tier1/convar.h"
#include <stdio.h>

#define PI 3.14159265

#ifdef _DEBUG
CUtlVector<Vector4D> g_DebugCornerPositions;
CUtlVector<Vector4D> g_DebugEdgePositions;
CUtlVector<Vector4D> g_DebugInteriorPositions;
#endif

//----------------------------------------------------------------------------------------------
// static stencil buffers
//----------------------------------------------------------------------------------------------

#if !defined( USE_OPT )

static float sPosCornerStencil[MAX_VALENCE+1][(MAX_VALENCE+1)*2];
static float sPosEdge1Stencil[MAX_VALENCE+1][6];
static float sPosEdge2Stencil[MAX_VALENCE+1][6];
static float sPosInteriorStencil[MAX_VALENCE+1][4];

static float sCCLimitTanStencil1[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static float sCCLimitTanStencil2[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static float sCCLimitTanBndStencil1[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static float sCCLimitTanBndStencil2[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static float sCCLimitTanCornerStencil1[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static float sCCLimitTanCornerStencil2[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];

static float sPosGregoryInterior1Stencil[6];
static float sPosGregoryInterior2Stencil[6];

static float sPosCornerBndStencil[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static float sPosEdge1BndStencil[MAX_VALENCE+1][6];
static float sPosEdge2BndStencil[MAX_VALENCE+1][6];
static float sPosInteriorBndStencil[MAX_VALENCE+1][4];

static float sPosEdge1CornerStencil[MAX_VALENCE+1][6];
static float sPosEdge2CornerStencil[MAX_VALENCE+1][6];

#endif

static bool sTableInited = false;
static bool sCornerCorrection = false;
static bool sShowACCGeometryTangents = false;
static bool sUseCornerTangents = true;

void set_ShowACCGeometryTangents(bool v)
{
	sShowACCGeometryTangents = v;
}

void set_CornerCorrection(bool v)
{
	sCornerCorrection = v;
}

void set_UseCornerTangents(bool v)
{
	sUseCornerTangents = v;
}

// averaging function over geometry patch tangents.
static float tangentAveraging( int n, int j)
{
	return sin( PI * j / (float) n );
}

//--------------------------------------------------------------------------------------
// Subdiv Stencils
//--------------------------------------------------------------------------------------
#if !defined( USE_OPT )

static void ComputeCatmullClarkLimitPosStencil(byte boundary, int n, float *stencilBuffer)
{
	VPROF_BUDGET( "ComputeCatmullClarkLimitPosStencil", _T("SubD Rendering") );

	memset(stencilBuffer, 0, 2*n*sizeof(float));

	if (!boundary)
	{
		float scale = 1.0f / (n*n + 5.0f*n);

		stencilBuffer[0] = n*n * scale;

		for (int i=0; i<n; i++)
		{
			stencilBuffer[2*i+1] = 4.0f * scale;
			stencilBuffer[2*i+2] = 1.0f * scale;
		}
	}
	else 
	{
		int k = n-1; 

		float s = 1.0f / 6.0f;
		stencilBuffer[0]     = s * 4.0f;
		stencilBuffer[1]     = s * 1.0f;
		stencilBuffer[2*k+1] = s * 1.0f;
	}
}

static void ComputeCatmullClarkLimitTanStencil(bool bndVtx, bool cornerVtx, const int n, float *stencilBuffer1, float *stencilBuffer2)
{
	VPROF_BUDGET( "ComputeCatmullClarkLimitTanStencil", _T("SubD Rendering") );

	memset( stencilBuffer1, 0, sizeof(float) * 2*n );
	memset( stencilBuffer2, 0, sizeof(float) * 2*n );

	if ( !bndVtx )
	{
		float scale_beta  = 1.0f / (n * sqrtf( 4.0f + cos( PI / n ) * cos( PI / n ) ) );
		float scale_alpha = 1.0f / n + cos( PI / n ) * scale_beta;

		for ( int i=0; i<n; i++ )
		{
			stencilBuffer1[2*i+1] = cos( 2*PI*i/n ) * scale_alpha;
			stencilBuffer1[2*i+2] = cos((2*PI*i+PI)/n ) * scale_beta;

			int j = (i - 1)%n;
			stencilBuffer2[2*i+1] = cos( 2*PI*j/n ) * scale_alpha;
			stencilBuffer2[2*i+2] = cos((2*PI*j+PI)/n ) * scale_beta;
		}
	}
	else 
	{
		// boundary vertex cases

		if ( cornerVtx )
		{
			if ( n<=2 ) 
				return;
			
			float sectorScale = 0, w;
			// treat first and last tangent (crease edges) separately
			w = tangentAveraging( n-1, 0 ); sectorScale += w;
			stencilBuffer1[ 1] +=  0.5 * w; 
			stencilBuffer1[ 0] += -0.5 * w;
			
			w = tangentAveraging( n-1, n-1 ); sectorScale += w;
			stencilBuffer1[ 2*(n-1)+1] +=  0.5 * w;
			stencilBuffer1[ 0 ]        += -0.5 * w;
			
			// inner tangents are computed using the 6 weights from the geometery edge construction.
			for (int k=1; k<(n-1); k++)
			{
				w = tangentAveraging( n-1, k ); sectorScale += w;
				float scale = 1.0f / (2.0f*n + 10.0f);
				
				stencilBuffer1[        0] += w * (2.0f*n * scale - 1.0f);
				stencilBuffer1[2*(k-1)+1] += w *  2.0f   * scale;
				stencilBuffer1[2*(k-1)+2] += w *  1.0f   * scale;
				stencilBuffer1[2*(k-1)+3] += w *  4.0f   * scale;
				stencilBuffer1[2*(k-1)+4] += w *  1.0f   * scale;
				stencilBuffer1[2*(k-1)+5] += w *  2.0f   * scale;
			}
			
			// rescale weights
			for (int k = 0; k<2*n; k++)
			{
				stencilBuffer1[k] /= sectorScale;
			}
	
		}
		else
		{
			// special case to avoid colinear tangents
			if ( n==2 )
			{
				float s = 1.0f / 2.0f;
				stencilBuffer1[1] = 1.0 * s;
				stencilBuffer1[3] =-1.0 * s;

				stencilBuffer2[1] =-1.0 * s;
				stencilBuffer2[3] = 1.0 * s;
				
				
				// regularization term to avoid collinearity and preserve limit normal at the boundary
				float eps = 1e-4;
				stencilBuffer1[0] += eps * (-4.0/3.0);
				stencilBuffer1[1] += eps * (1.0/2.0);
				stencilBuffer1[2] += eps * (1.0/3.0);
				stencilBuffer1[3] += eps * (1.0/2.0);

				stencilBuffer2[0] += eps * (-4.0/3.0);
				stencilBuffer2[1] += eps * (1.0/2.0);
				stencilBuffer2[2] += eps * (1.0/3.0);
				stencilBuffer2[3] += eps * (1.0/2.0);

			}
			else
			{
				int k = n-1;
				float c = cos( PI / k ), s=sin( PI / k );

				stencilBuffer1[2*0+1] =  0.5f;
				stencilBuffer1[2*k+1] = -0.5f;

				stencilBuffer2[0] = -4.0f*s / (3.0f*k + c);  // gamma

				for (int i=0; i<k; ++i)
				{
					stencilBuffer2[2*i+1] = 4*sin(PI*i/k)/(3*k+c);                   // alpha_i
					stencilBuffer2[2*i+2] = (sin(PI*i/k)+sin(PI*(i+1)/k)) / (3.0f*k+c);   // beta_i
				}

				stencilBuffer2[2*0+1] = stencilBuffer2[2*k+1] = -( (1+2*c)*sqrt(1+c) ) / ( (3*k+c)*sqrt(1-c) );  // alpha_0, alpha_k
			}
		}

	} 

}


static void computeACCEdgePosStencils(byte boundary, byte corner, int n, float *stencilBuffer1, float *stencilBuffer2)
{
	VPROF_BUDGET( "ComputeACCEdgePosStencils", _T("SubD Rendering") );

	memset(stencilBuffer1, 0, 6*sizeof(float));
	memset(stencilBuffer2, 0, 6*sizeof(float));

	if ( !boundary )
	{
		float scale = 1.0f / (2.0f*n + 10.0f);

		stencilBuffer1[0] = 2.0f*n * scale; stencilBuffer2[0] = 4.0f * scale;
		stencilBuffer1[1] = 2.0f * scale;   stencilBuffer2[1] = 1.0f * scale;
		stencilBuffer1[2] = 1.0f * scale;   stencilBuffer2[2] = 2.0f * scale;
		stencilBuffer1[3] = 4.0f * scale;   stencilBuffer2[3] = 2.0f*n* scale;
		stencilBuffer1[4] = 1.0f * scale;   stencilBuffer2[4] = 2.0f * scale;
		stencilBuffer1[5] = 2.0f * scale;   stencilBuffer2[5] = 1.0f * scale;
	}
	else
	{ // boundary stencil
		if ( corner )
		{
			float scale = 1.0f / (3.0f);

			stencilBuffer1[0] = 2.0f * scale; stencilBuffer2[0] = 1.0f * scale;
			stencilBuffer1[3] = 1.0f * scale; stencilBuffer2[3] = 2.0f * scale;
		} 
		else 
		{
			float scale = 1.0f / 3.0f;

			stencilBuffer1[0] = 2.0f * scale; stencilBuffer2[0] = 1.0f * scale;
			stencilBuffer1[3] = 1.0f * scale; stencilBuffer2[3] = 2.0f * scale;
		}
	}
}

static void computeACCInteriorPosStencil(byte boundary, int n, float *stencilBuffer)
{
	VPROF_BUDGET( "ComputeACCInteriorPosStencil", _T("SubD Rendering") );

	float scale = 1.0f / (n + 5.0f);

	stencilBuffer[0] = n * scale;
	stencilBuffer[1] = 2.0f * scale;
	stencilBuffer[2] = 1.0f * scale;
	stencilBuffer[3] = 2.0f * scale;
}


void FillTables()
{
	if ( sTableInited )	return;

	for ( int val=0; val<=MAX_VALENCE; val++ )
	{
		// interior stencils
		computeCatmullClarkLimitPosStencil(false, val, sPosCornerStencil[val]);
		computeACCEdgePosStencils(false, false, val, sPosEdge1Stencil[val], sPosEdge2Stencil[val]);
		computeACCInteriorPosStencil(false, val, sPosInteriorStencil[val]);

		// boundary stencils
		computeCatmullClarkLimitPosStencil(true, val, sPosCornerBndStencil[val]);
		computeACCEdgePosStencils(true, false, val, sPosEdge1BndStencil[val], sPosEdge2BndStencil[val]);
		computeACCEdgePosStencils(true, true, val, sPosEdge1CornerStencil[val], sPosEdge2CornerStencil[val]);
		computeACCInteriorPosStencil(true, val, sPosInteriorBndStencil[val]);

		computeCatmullClarkLimitTanStencil(false, false, val, sCCLimitTanStencil1[val], sCCLimitTanStencil2[val]);
		computeCatmullClarkLimitTanStencil(true, false, val, sCCLimitTanBndStencil1[val], sCCLimitTanBndStencil2[val]);
		computeCatmullClarkLimitTanStencil(true, true, val, sCCLimitTanCornerStencil1[val], sCCLimitTanCornerStencil2[val]);
	}

	sTableInited = true;
}



//--------------------------------------------------------------------------------------
// Runtime
//--------------------------------------------------------------------------------------

#ifdef _DEBUG
static ConVar mat_tess_dump( "mat_tess_dump", "0", FCVAR_CHEAT );
#endif

// Compute corner control points for each patch
inline void ComputeCatmullClarkLimitPosition( Vector4D *pPos, unsigned short *oneRing,
											 unsigned short vtx1RingSize, unsigned short minOneRingIndex, unsigned short bndVtx,
											 unsigned short cornerVtx, unsigned short valence, unsigned short nbCorners, Vector4D &limitPos )
{
	VPROF_BUDGET( "ComputeCatmullClarkLimitPosition", _T("SubD Rendering") );

	if ( cornerVtx > 0 )
	{
		limitPos = pPos[ oneRing[0] ];
	}
	else
	{
		assert( valence <= MAX_VALENCE );

		float *pStencil = bndVtx ? sPosCornerBndStencil[ valence ] : sPosCornerStencil[ valence ];

		// pStencil[0] is always the largest value (see Figures 4 and 5 in Loop and Schaefer)
		limitPos = pStencil[0] * pPos[ oneRing[0] ];
		for ( int k = 0; k < vtx1RingSize; k++ )
		{
			int idx = ( k + minOneRingIndex ) % vtx1RingSize;	// Shuffle to get the minimum index consistently first in order
			if ( idx != 0 )										// Don't do pStencil[0] again
			{
				limitPos += pStencil[idx] * pPos[ oneRing[idx] ];
			}
		}
	}
#ifdef _DEBUG
	g_DebugCornerPositions.AddToTail( limitPos );
#endif
}

inline Vector4D CrossProduct(const Vector4D& a, const Vector4D& b) 
{ 
	return Vector4D( a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x, 0.0f ); 
}

inline float VectorNormalize(Vector4D& vec) 
{ 
	float radius = sqrtf(vec.x*vec.x + vec.y*vec.y + vec.z*vec.z);

	// FLT_EPSILON is added to the radius to eliminate the possibility of divide by zero.
	float iradius = 1.f / ( radius + FLT_EPSILON );
	
	vec.x *= iradius;
	vec.y *= iradius;
	vec.z *= iradius;
	
	return radius;
}

FORCEINLINE float DotProduct(const Vector4D& a, const Vector4D& b) 
{ 
	return ( a.x*b.x + a.y*b.y + a.z*b.z ); 
}

inline void ComputeCatmullClarkLimitTangents( int idx, Vector4D *pPos, unsigned short *oneRing, unsigned short vtx1RingSize,
											 unsigned short centerOffset, unsigned short bndVtx, unsigned short cornerVtx, 
											 unsigned short valence, unsigned short &loopGapAngle,
											 Vector4D &limitTanU, Vector4D &limitTanV )
{
	// for valence=1, no need to have separate tangents

	float tanUSign[] = {1,-1,-1,1};
	float tanVSign[] = {1,1,-1,-1};

	VPROF_BUDGET( "ComputeCatmullClarkLimitTangents", _T("SubD Rendering") );
	
	if ( !sUseCornerTangents ) 
		cornerVtx = 0;
	
	if ( !bndVtx )			// interior vertices
	{
		float *stencil1 = sCCLimitTanStencil1[ valence ];
		float *stencil2 = sCCLimitTanStencil2[ valence ];
		
		limitTanU = Vector4D(0,0,0,0); 
		limitTanV = Vector4D(0,0,0,0);

		for (int k = 0; k < vtx1RingSize; ++k)
		{
			limitTanU += stencil1[k] * pPos[ oneRing[k] ];
			limitTanV += stencil2[k] * pPos[ oneRing[k] ];
		}

	} 
	else if ( (!cornerVtx) || (cornerVtx == CORNER_WITH_SMOOTHBNDTANGENTS) )	// smooth boundary vertices
	{
	
		float *stencil1 = sCCLimitTanBndStencil1[ valence ];
		float *stencil2 = sCCLimitTanBndStencil2[ valence ];
		
		Vector4D r0 = Vector4D(0,0,0,0);
		Vector4D r1 = Vector4D(0,0,0,0);
		
		for (int k = 0; k < vtx1RingSize; ++k)
		{
			r0 += stencil1[k] * pPos[ oneRing[k] ];
			r1 += stencil2[k] * pPos[ oneRing[k] ];
		} 
		
		int j1 = (centerOffset - 1) / 2;
		int j2 = j1+1;
		int K = (valence - 1);
		
		if (valence == 2)
		{
			limitTanU = r0;
			limitTanV = r1;
		}
		else
		{
			limitTanU = cos(PI*j1 / K) * r0 + sin(PI*j1 / K) * r1;
			limitTanV = cos(PI*j2 / K) * r0 + sin(PI*j2 / K) * r1;
		}
	}
	else // corner vertices
	{
		if ( valence == 2 ) 
			return; 

		float *pEdgeStencil = sPosEdge1Stencil[ valence ];
//		float *avgStencil  = sCCLimitTanCornerStencil1[ valence ];

		// compute tangents
		Vector4D c0 = pPos[ oneRing[1] ] - pPos[ oneRing[0] ];  c0.w = 0;
		Vector4D c1 = pPos[ oneRing[vtx1RingSize - 1] ] - pPos[ oneRing[0] ]; c1.w = 0;
		
		Vector4D e0 = (pEdgeStencil[0] - 1.0f ) * pPos[ oneRing[0] ];
		Vector4D e1 = (pEdgeStencil[0] - 1.0f ) * pPos[ oneRing[0] ];
		for (int k = 1; k < 6; k++ )
		{
			e0 += pEdgeStencil[k] * pPos[ oneRing[ k ] ];
			e1 += pEdgeStencil[k] * pPos[ oneRing[ vtx1RingSize - 6 + k ] ];
		}
		e0.w = 0; e1.w = 0;
		
		// compute average tangent plane normal
		Vector4D n0 = CrossProduct( c0, e0 ); VectorNormalize( n0 );
		Vector4D n1 = CrossProduct( e1, c1 ); VectorNormalize( n1 );
		Vector4D N = n0 + n1;
//		N = N - ( DotProduct( N, tAvg )/ DotProduct(tAvg, tAvg) ) * tAvg;
		VectorNormalize( N );

		// project into tangent plane
		
		c0 = c0 - DotProduct(c0, N) * N;
		c1 = c1 - DotProduct(c1, N) * N;
		
		float c0l = Vector4DLength( c0 ); c0 = c0 / c0l;
		float c1l = Vector4DLength( c1 ); c1 = c1 / c1l;
		float cAvg = (c0l + c1l) / 2;
		
		// compute angle
		Vector4D c0p = CrossProduct(N, c0);
		float angle = PI - atan2( DotProduct(c0p, c1), -DotProduct(c0, c1) );
				
		loopGapAngle = (unsigned int) ((65535.0 * angle) / (2*PI));

		// compute final tangent vector
		int j1 = (centerOffset - 1) / 2;
		int j2 = j1+1;
		int K = (valence - 1);
		
		limitTanU = cAvg * ( cos(angle*j1 / K) * c0 + sin(angle*j1 / K) * c0p );
		limitTanV = cAvg * ( cos(angle*j2 / K) * c0 + sin(angle*j2 / K) * c0p );
	} 
	
	// flip tangents so they point in u/v direction
	if ( idx & 1 )
	{
		swap(limitTanU, limitTanV);
	}
	limitTanU *= tanUSign[idx]; 
	limitTanV *= tanVSign[idx];
}


inline void ComputeACCEdgePositions( Vector4D *pPos, unsigned short *oneRing, unsigned short centerOffset,
									unsigned short bndEdge, unsigned short bndVtx0, unsigned short bndVtx1,
									unsigned short cornerVtx0, unsigned short cornerVtx1, unsigned short loopGapAngle0, unsigned short loopGapAngle1, 
									unsigned short edgeBias0, unsigned short edgeBias1, unsigned short val0, unsigned short val1,
									unsigned short minOneRingOffset, unsigned short vtx1RingSize,
									Vector4D &edgePos0, Vector4D &edgePos1)
{
	VPROF_BUDGET( "ComputeACCEdgePositions", _T("SubD Rendering") );

	if ( bndVtx0 )
	{
		val0 = 2*(val0 - 1);
	}

	if ( bndVtx1 )
	{
		val1 = 2*(val1 - 1);
	}

	Assert( val0 <= MAX_VALENCE );
	Assert( val1 <= MAX_VALENCE );

	float* pStencil0 = (bndEdge) ? (cornerVtx0) ? sPosEdge1CornerStencil[ val0 ] : sPosEdge1BndStencil[ val0 ] : sPosEdge1Stencil[ val0 ];
	float* pStencil1 = (bndEdge) ? (cornerVtx1) ? sPosEdge2CornerStencil[ val1 ] : sPosEdge2BndStencil[ val1 ] : sPosEdge2Stencil[ val1 ];

	int kEnd = (bndEdge) ? 4 : 6;

	if ( ( edgeBias0 == 16384 ) && ( edgeBias1 == 16384 ) )
	{
		int oneRingIndex[6] = { 0, 0, 0, 0, 0, 0 };
		for ( int i = 1; i < kEnd; i++ )
		{
			oneRingIndex[i] = centerOffset + i - 1;
		}

		edgePos0 = edgePos1 = Vector4D(0,0,0,0);
		for ( int k = 0; k < kEnd; k++ )
		{
			int idx = ( k + minOneRingOffset ) % kEnd;	// Offset to min index to enforce evaluation order between neighboring patches
			edgePos0 += pStencil0[idx] * pPos[ oneRing[ oneRingIndex[idx] ] ];
			edgePos1 += pStencil1[idx] * pPos[ oneRing[ oneRingIndex[idx] ] ];
		}
	} 
	else 
	{
		float b0, b1;
		b1 = edgeBias0 / 32768.0, b0 = 1.0f-b1;
		edgePos0 = (val0 * pPos[ oneRing[0] ] + 2*b0*pPos[ oneRing[centerOffset + 0] ] + 1*b0*pPos[ oneRing[centerOffset + 1] ] +    2*pPos[ oneRing[centerOffset + 2] ] + 1*b1*pPos[ oneRing[centerOffset + 3] ] + 2*b1*pPos[ oneRing[centerOffset + 4] ] ) / (val0 + 5.0f);
		b1 = edgeBias1 / 32768.0, b0 = 1.0f-b1;
		edgePos1 = (   2 * pPos[ oneRing[0] ] + 1*b0*pPos[ oneRing[centerOffset + 0] ] + 2*b0*pPos[ oneRing[centerOffset + 1] ] + val1*pPos[ oneRing[centerOffset + 2] ] + 2*b1*pPos[ oneRing[centerOffset + 3] ] + 1*b1*pPos[ oneRing[centerOffset + 4] ] ) / (val1 + 5.0f);
	}

#ifdef _DEBUG
	g_DebugEdgePositions.AddToTail( edgePos0 );
	g_DebugEdgePositions.AddToTail( edgePos1 );
#endif
}


inline void ComputeACCInteriorPosition( Vector4D *pPos, unsigned short *oneRing, unsigned short centerOffset, unsigned short bndVtx, unsigned short valence, Vector4D &interiorPos )
{
	VPROF_BUDGET( "ComputeACCInteriorPosition", _T("SubD Rendering") );

	if ( bndVtx )
	{
		valence = valence>2 ?  2*(valence - 1) : 4*(valence - 1);
	}

	Assert( valence<=MAX_VALENCE );

	float *stencil = sPosInteriorStencil[ valence ];

	interiorPos = stencil[0] * pPos[ oneRing[0] ];
	for ( int k = 1; k < 4; ++k )
	{
		interiorPos += stencil[k] * pPos[ oneRing[ centerOffset + k - 1 ] ];
	}

#ifdef _DEBUG
	g_DebugInteriorPositions.AddToTail( interiorPos );
#endif

}

inline void ComputeACCGeometryPatchTangents( Vector4D *Pos, Vector4D *TanU, Vector4D *TanV )
{
	VPROF_BUDGET( "ComputeACCGeometryPatchTangents", _T("SubD Rendering") );

	for ( int j=0; j<3; j++ )
	{
		for ( int i=0; i<4; i++ )
		{
			TanU[i*3+j] = 3*( Pos[i*4+j+1]   - Pos[i*4+j] );
			TanV[j*4+i] = 3*( Pos[(j+1)*4+i] - Pos[j*4+i] );
		}
	}
}

void ComputeACCGeometryPatch( Vector4D* pPos, TopologyIndexStruct *quad, Vector4D* Pos)
{
	VPROF_BUDGET( "ComputeACCGeometryPatch", _T("SubD Rendering") );

	int MOD4[8] = {0,1,2,3,0,1,2,3};

	int accCorner[]   = {0,3,15,12};
	int accEdge1[]    = {4,2,11,13};
	int accEdge2[]    = {8,1,7,14};
	int accInterior[] = {5,6,10,9};

	int vtx1RingStart = 0;

	unsigned short *oneRing = quad->oneRing;

	for ( int i=0; i<4; i++ ) // 4 corner vertices
	{
		ComputeCatmullClarkLimitPosition( pPos, &oneRing[vtx1RingStart], quad->vtx1RingSize[i], quad->minOneRingOffset[i], quad->bndVtx[i], quad->cornerVtx[i], quad->valences[i], quad->nbCornerVtx[i], Pos[ accCorner[i] ] );

		ComputeACCEdgePositions( pPos, &oneRing[vtx1RingStart], quad->vtx1RingCenterQuadOffset[i], 
			quad->bndEdge[ MOD4[i+3] ], 
			quad->bndVtx[i], quad->bndVtx[MOD4[i+3]], 
			quad->cornerVtx[i],    quad->cornerVtx[MOD4[i+3]], 
			quad->loopGapAngle[i], quad->loopGapAngle[MOD4[i+3]], 
			quad->edgeBias[ 2*MOD4[i+3] ], quad->edgeBias[ 2*MOD4[i+3] + 1 ],
			quad->valences[i], quad->valences[MOD4[i+3]],
			quad->minOneRingOffset[i], quad->vtx1RingSize[i],
			Pos[accEdge1[i]], Pos[accEdge2[i]] );

		ComputeACCInteriorPosition( pPos, &oneRing[vtx1RingStart], quad->vtx1RingCenterQuadOffset[i], quad->bndVtx[i], quad->cornerVtx[i], quad->loopGapAngle[i], quad->valences[i], Pos[ accInterior[i] ] );

		vtx1RingStart += quad->vtx1RingSize[i];
	}
}


void ComputeACCTangentPatches( Vector4D* pPos, TopologyIndexStruct* quad, Vector4D* Pos, Vector4D* TanU, Vector4D* TanV )
{
	VPROF_BUDGET( "ComputeACCTangentPatches", _T("SubD Rendering") );

	int MOD4[8] = {0,1,2,3,0,1,2,3};

	int accTanCornerU[] = {0,2,11,9};  // counterclockwise orders!
	int accTanCornerV[] = {0,3,11,8};

	unsigned short *oneRing = quad->oneRing;

	ComputeACCGeometryPatchTangents(Pos, TanU, TanV);

#if !defined( NO_TANGENTS )
	if ( !sShowACCGeometryTangents ) 
	{
		// compute corner tangents ( = subdivision surface limit tangents)
		int vtx1RingStart = 0;
		for ( int i=0; i<4; i++ )
		{
			int vtx1RingSize = quad->vtx1RingSize[i];

			Vector4D &accTanU = TanU[ accTanCornerU[i] ];
			Vector4D &accTanV = TanV[ accTanCornerV[i] ];

			ComputeCatmullClarkLimitTangents(i, pPos, &oneRing[vtx1RingStart], vtx1RingSize, quad->vtx1RingCenterQuadOffset[i], quad->bndVtx[i], quad->cornerVtx[i], quad->valences[i], quad->loopGapAngle[i], accTanU, accTanV );

			vtx1RingStart += vtx1RingSize;
		}

		// compute correction component to boundary tangents for tangent plane continuity
		//                             /TanV/ /TanU/ / TanV / /TanU/
		static int   CB_CornerIdx[]   = {0,1,2, 3,7,11, 11,10,9, 8,4,0 };
		static int   CB_InteriorIdx[] = {1,2,   5,8,    10,9,    6,3 };
		static float CB_sign[]        = {1,-1,1,-1};

		for ( int i=0; i<4; i++ ) // for all quad edges
		{
			if ( !quad->bndEdge[i] )
			{
				Vector4D *CBTanV = (i&1) ? TanU : TanV;
				Vector4D *CBTanU = (i&1) ? TanV : TanU;
				
				Vector4D u00 = CBTanU[CB_CornerIdx[3*i + 0]];
				Vector4D u10 = CBTanU[CB_CornerIdx[3*i + 1]];
				Vector4D u20 = CBTanU[CB_CornerIdx[3*i + 2]];
				
				int val0 = quad->valences[i];
				int val1 = quad->valences[MOD4[i+1]];
				
				if ( quad->bndVtx[i] ) 
					val0--;
				if ( quad->bndVtx[MOD4[i+1]] ) 
					val1--;
				
				float c0 = cos( (2*PI * quad->loopGapAngle[     i   ] / 65535.0f) / val0 );
				float c1 = cos( (2*PI * quad->loopGapAngle[MOD4[i+1]] / 65535.0f) / val1 );
				
				CBTanV[ CB_InteriorIdx[2*i + 0] ] += CB_sign[i]*( 2*c0*u10 -   c1*u00 )/3.0f;
				CBTanV[ CB_InteriorIdx[2*i + 1] ] += CB_sign[i]*(   c0*u20 - 2*c1*u10 )/3.0f;
			}
		}
		
	}
#endif

}
#endif  // !defined( USE_OPT )

#if defined( USE_OPT )

#define M_PI2			6.28318530717958647692f

static fltx4 Four_NegativeThirds;
static fltx4 Four_Fives;
static fltx4 Four_Tens;
static fltx4 Four_N[32];
static fltx4 Four_TwoPI;
static fltx4 Four_Valence[MAX_VALENCE];
static fltx4 Four_ValencePlus5[MAX_VALENCE];

static fltx4 sPosCornerStencil[MAX_VALENCE+1][(MAX_VALENCE+1)*2];
static fltx4 sPosEdge1Stencil[MAX_VALENCE+1][6];
static fltx4 sPosEdge2Stencil[MAX_VALENCE+1][6];
static fltx4 sPosInteriorStencil[MAX_VALENCE+1][4];

static fltx4 sCCLimitTanStencil1[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static fltx4 sCCLimitTanStencil2[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static fltx4 sCCLimitTanBndStencil1[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static fltx4 sCCLimitTanBndStencil2[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static fltx4 sCCLimitTanCornerStencil1[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static fltx4 sCCLimitTanCornerStencil2[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];

static fltx4 sPosCornerBndStencil[MAX_VALENCE+1][(MAX_VALENCE+1)*2+1];
static fltx4 sPosEdge1BndStencil[MAX_VALENCE+1][6];
static fltx4 sPosEdge2BndStencil[MAX_VALENCE+1][6];
static fltx4 sPosInteriorBndStencil[MAX_VALENCE+1][4];

static fltx4 sPosEdge1CornerStencil[MAX_VALENCE+1][6];
static fltx4 sPosEdge2CornerStencil[MAX_VALENCE+1][6];

static fltx4 sCCSinPI[MAX_VALENCE*2][MAX_VALENCE];
static fltx4 sCCCosPI[MAX_VALENCE*2][MAX_VALENCE];

static float Valence_MinusOne[MAX_VALENCE];


static void ComputeCatmullClarkLimitPosStencil(byte boundary, int n, fltx4 *stencilBuffer)
{
	VPROF_BUDGET( "ComputeCatmullClarkLimitPosStencil", _T("SubD Rendering") );

	for ( int i=0; i<2*n; ++i )
	{
		stencilBuffer[i] = Four_Zeros;
	}

	if ( !boundary )
	{
		float scale = 1.0f / (n*n + 5.0f*n);

		stencilBuffer[0] = ReplicateX4( n*n * scale );

		for ( int i=0; i<n; i++ )
		{
			stencilBuffer[2*i+1] = ReplicateX4( 4.0f * scale );
			stencilBuffer[2*i+2] = ReplicateX4( 1.0f * scale );
		}
	}
	else 
	{
		int k = n-1; 

		float s = 1.0f / 6.0f;
		stencilBuffer[0]     = ReplicateX4( s * 4.0f );
		stencilBuffer[1]     = ReplicateX4( s * 1.0f );
		stencilBuffer[2*k+1] = ReplicateX4( s * 1.0f );
	}
}

static void ComputeCatmullClarkLimitTanStencil(bool bndVtx, bool cornerVtx, const int n, fltx4 *stencilBuffer1, fltx4 *stencilBuffer2)
{
	VPROF_BUDGET( "ComputeCatmullClarkLimitTanStencil", _T("SubD Rendering") );

	for ( int i=0; i<2*n; ++i )
	{
		stencilBuffer1[i] = Four_Zeros;
		stencilBuffer2[i] = Four_Zeros;
	}

	if ( !bndVtx )
	{
		float scale_beta  = 1.0f / (n * sqrtf(4.0f + cos(PI/n)*cos(PI/n)));
		float scale_alpha = 1.0f/n + cos(PI/n) * scale_beta;

		for ( int i=0; i<n; i++ )
		{
			stencilBuffer1[2*i+1] = ReplicateX4( cos( 2*PI*i/n ) * scale_alpha );
			stencilBuffer1[2*i+2] = ReplicateX4( cos((2*PI*i+PI)/n ) * scale_beta );

			int j = (i - 1)%n;
			stencilBuffer2[2*i+1] = ReplicateX4( cos( 2*PI*j/n ) * scale_alpha );
			stencilBuffer2[2*i+2] = ReplicateX4( cos((2*PI*j+PI)/n ) * scale_beta );
		}
	}
	else 
	{
		// boundary vertex cases
		if ( cornerVtx )
		{
			if ( n<=2 ) 
				return;
			
			float sectorScale = 0, w;
			// treat first and last tangent (crease edges) separately
			w = tangentAveraging( n-1, 0 ); sectorScale += w;
			stencilBuffer1[ 1] = stencilBuffer1[ 1] + ReplicateX4( 0.5 * w ); 
			stencilBuffer1[ 0] = stencilBuffer1[ 0] + ReplicateX4( -0.5 * w );
			
			w = tangentAveraging( n-1, n-1 ); sectorScale += w;
			stencilBuffer1[ 2*(n-1)+1] = stencilBuffer1[ 2*(n-1)+1] + ReplicateX4( 0.5 * w );
			stencilBuffer1[ 0 ]        = stencilBuffer1[ 0 ]        + ReplicateX4( -0.5 * w );
			
			// inner tangents are computed using the 6 weights from the geometery edge construction.
			for (int k=1; k<(n-1); k++)
			{
				w = tangentAveraging( n-1, k ); sectorScale += w;
				float scale = 1.0f / (2.0f*n + 10.0f);
				
				stencilBuffer1[        0] = stencilBuffer1[        0] + ReplicateX4( w * (2.0f*n * scale - 1.0f) );
				stencilBuffer1[2*(k-1)+1] = stencilBuffer1[2*(k-1)+1] + ReplicateX4( w *  2.0f   * scale );
				stencilBuffer1[2*(k-1)+2] = stencilBuffer1[2*(k-1)+2] + ReplicateX4( w *  1.0f   * scale );
				stencilBuffer1[2*(k-1)+3] = stencilBuffer1[2*(k-1)+3] + ReplicateX4( w *  4.0f   * scale );
				stencilBuffer1[2*(k-1)+4] = stencilBuffer1[2*(k-1)+4] + ReplicateX4( w *  1.0f   * scale );
				stencilBuffer1[2*(k-1)+5] = stencilBuffer1[2*(k-1)+5] + ReplicateX4( w *  2.0f   * scale );
			}
			
			// rescale weights
			fltx4 fltx4Scale = ReplicateX4( sectorScale );
			for ( int k = 0; k<2*n; ++k )
			{
				stencilBuffer1[k] = DivSIMD( stencilBuffer1[k], fltx4Scale );
			}
	
		}
		else
		{
			// special case to avoid colinear tangents
			if ( n==2 )
			{
				float s = 1.0f / 2.0f;
				stencilBuffer1[1] = ReplicateX4(  1.0 * s );
				stencilBuffer1[3] = ReplicateX4( -1.0 * s );

				stencilBuffer2[1] = ReplicateX4( -1.0 * s );
				stencilBuffer2[3] = ReplicateX4(  1.0 * s );
				
				
				// regularization term to avoid collinearity and preserve limit normal at the boundary
				float eps = 1e-4;
				stencilBuffer1[0] = AddSIMD( stencilBuffer1[0], ReplicateX4( eps * (-4.0/3.0) ) );
				stencilBuffer1[1] = AddSIMD( stencilBuffer1[1], ReplicateX4( eps * (1.0/2.0) ) );
				stencilBuffer1[2] = AddSIMD( stencilBuffer1[2], ReplicateX4( eps * (1.0/3.0) ) );
				stencilBuffer1[3] = AddSIMD( stencilBuffer1[3], ReplicateX4( eps * (1.0/2.0) ) );

				stencilBuffer2[0] = AddSIMD( stencilBuffer2[0], ReplicateX4( eps * (-4.0/3.0) ) );
				stencilBuffer2[1] = AddSIMD( stencilBuffer2[1], ReplicateX4( eps * (1.0/2.0) ) );
				stencilBuffer2[2] = AddSIMD( stencilBuffer2[2], ReplicateX4( eps * (1.0/3.0) ) );
				stencilBuffer2[3] = AddSIMD( stencilBuffer2[3], ReplicateX4( eps * (1.0/2.0) ) );
			}
			else
			{
				int k = n-1;
				float c = cos( PI / k ), s=sin( PI / k );

				stencilBuffer1[2*0+1] = ReplicateX4(  0.5f );
				stencilBuffer1[2*k+1] = ReplicateX4( -0.5f );

				stencilBuffer2[0] = ReplicateX4( -4.0f*s / (3.0f*k + c) );  // gamma

				for ( int i=0; i<k; ++i )
				{
					stencilBuffer2[2*i+1] = ReplicateX4( 4*sin(PI*i/k)/(3*k+c) );                   // alpha_i
					stencilBuffer2[2*i+2] = ReplicateX4( (sin(PI*i/k)+sin(PI*(i+1)/k)) / (3.0f*k+c) );   // beta_i
				}

				stencilBuffer2[2*0+1] = stencilBuffer2[2*k+1] = ReplicateX4( -( (1+2*c)*sqrt(1+c) ) / ( (3*k+c)*sqrt(1-c) ) );  // alpha_0, alpha_k
			}
		}

	} 

}

static void ComputeACCEdgePosStencils(byte boundary, byte corner, int n, fltx4 *stencilBuffer1, fltx4 *stencilBuffer2)
{
	VPROF_BUDGET( "ComputeACCEdgePosStencils", _T("SubD Rendering") );

	for ( int i=0; i<6; ++i )
	{
		stencilBuffer1[i] = Four_Zeros;
		stencilBuffer2[i] = Four_Zeros;
	}

	if ( !boundary )
	{
		float scale = 1.0f / (2.0f*n + 10.0f);

		stencilBuffer1[0] = ReplicateX4( 2.0f*n * scale ); stencilBuffer2[0] = ReplicateX4( 4.0f * scale );
		stencilBuffer1[1] = ReplicateX4( 2.0f * scale );   stencilBuffer2[1] = ReplicateX4( 1.0f * scale );
		stencilBuffer1[2] = ReplicateX4( 1.0f * scale );   stencilBuffer2[2] = ReplicateX4( 2.0f * scale );
		stencilBuffer1[3] = ReplicateX4( 4.0f * scale );   stencilBuffer2[3] = ReplicateX4( 2.0f*n* scale );
		stencilBuffer1[4] = ReplicateX4( 1.0f * scale );   stencilBuffer2[4] = ReplicateX4( 2.0f * scale );
		stencilBuffer1[5] = ReplicateX4( 2.0f * scale );   stencilBuffer2[5] = ReplicateX4( 1.0f * scale );
	}
	else
	{ 
		// boundary stencil
		if ( corner )
		{
			float scale = 1.0f / (3.0f);

			stencilBuffer1[0] = ReplicateX4( 2.0f * scale ); stencilBuffer2[0] = ReplicateX4( 1.0f * scale );
			stencilBuffer1[3] = ReplicateX4( 1.0f * scale ); stencilBuffer2[3] = ReplicateX4( 2.0f * scale );
		} 
		else 
		{
			float scale = 1.0f / 3.0f;

			stencilBuffer1[0] = ReplicateX4( 2.0f * scale ); stencilBuffer2[0] = ReplicateX4( 1.0f * scale );
			stencilBuffer1[3] = ReplicateX4( 1.0f * scale ); stencilBuffer2[3] = ReplicateX4( 2.0f * scale );
		}
	}
}

static void ComputeACCInteriorPosStencil(byte boundary, int n, fltx4 *stencilBuffer)
{
	VPROF_BUDGET( "ComputeACCInteriorPosStencil", _T("SubD Rendering") );

	float scale = 1.0f / (n + 5.0f);

	stencilBuffer[0] = ReplicateX4( n * scale );
	stencilBuffer[1] = ReplicateX4( 2.0f * scale );
	stencilBuffer[2] = ReplicateX4( 1.0f * scale );
	stencilBuffer[3] = ReplicateX4( 2.0f * scale );
}

static void ComputeACCSinCosPITables()
{
	fltx4 PI4 = ReplicateX4( M_PI );

	for ( int j=0; j<MAX_VALENCE*2; ++j )
	{
		fltx4 j4 = ReplicateX4( (float)j );

		for ( int k=0; k<MAX_VALENCE; ++k )
		{
			fltx4 k4 = ReplicateX4( (float)k );
			fltx4 radians = DivSIMD( MulSIMD( PI4, j4 ), k4 );

			// not really simd
			SinCosSIMD( sCCSinPI[j][k], sCCCosPI[j][k], radians );
		}
	}
}

void FillTables()
{
	if ( sTableInited )	
		return;

	// Some simd stuff
	Four_TwoPI = ReplicateX4( 2*M_PI );
	Four_Tens = ReplicateX4( 10.0f );
	Four_Fives = ReplicateX4( 5 );
	Four_NegativeThirds = ReplicateX4( -0.333333333333333f );
	for ( int i=0; i<32; ++i )
	{
		Four_N[i] = ReplicateX4( (float)i );
	}
	for ( int i=0; i<MAX_VALENCE; ++i )
	{
		Four_Valence[i] = ReplicateX4( (float)i );
		Four_ValencePlus5[i] = ReplicateX4( (float)i + 5.0f );
		Valence_MinusOne[i] = (float)(i-1);
	}

	for ( int val=0; val<=MAX_VALENCE; val++ )
	{
		// interior stencils
		ComputeCatmullClarkLimitPosStencil( false, val, sPosCornerStencil[val] );
		ComputeACCEdgePosStencils( false, false, val, sPosEdge1Stencil[val], sPosEdge2Stencil[val] );
		ComputeACCInteriorPosStencil( false, val, sPosInteriorStencil[val] );

		// boundary stencils
		ComputeCatmullClarkLimitPosStencil( true, val, sPosCornerBndStencil[val] );
		ComputeACCEdgePosStencils( true, false, val, sPosEdge1BndStencil[val], sPosEdge2BndStencil[val] );
		ComputeACCEdgePosStencils( true, true, val, sPosEdge1CornerStencil[val], sPosEdge2CornerStencil[val] );
		ComputeACCInteriorPosStencil( true, val, sPosInteriorBndStencil[val] );

		ComputeCatmullClarkLimitTanStencil( false, false, val, sCCLimitTanStencil1[val], sCCLimitTanStencil2[val] );
		ComputeCatmullClarkLimitTanStencil( true, false, val, sCCLimitTanBndStencil1[val], sCCLimitTanBndStencil2[val] );
		ComputeCatmullClarkLimitTanStencil( true, true, val, sCCLimitTanCornerStencil1[val], sCCLimitTanCornerStencil2[val] );
	}

	// sincos tables
	ComputeACCSinCosPITables();

	sTableInited = true;
}

//--------------------------------------------------------------------------------------
// Runtime
//--------------------------------------------------------------------------------------
FORCEINLINE void ComputeCatmullClarkLimitPosition( fltx4 *pPos, unsigned short *pOneRing,
												   unsigned short vtx1RingSize, unsigned short minOneRingIndex, unsigned short bndVtx,
												   unsigned short cornerVtx, unsigned short valence, fltx4 &limitPos )
{
	VPROF_BUDGET( "ComputeCatmullClarkLimitPosition (SIMD)", _T( "SubD Rendering" ) );

	assert( pPos );
	assert( pOneRing );

	if ( cornerVtx > 0 )
	{
		limitPos = pPos[ pOneRing[0]  ];
	}
	else
	{
		assert( valence <= MAX_VALENCE );

		fltx4 *pStencil = bndVtx ? sPosCornerBndStencil[ valence ] : sPosCornerStencil[ valence ];

		// pStencil[0] is always the largest value (see Figures 4 and 5 in Loop and Schaefer)
		limitPos = MulSIMD( pStencil[0], pPos[ pOneRing[0] ] );
		for ( int k = 0; k < vtx1RingSize; k++ )
		{
			int idx = ( k + minOneRingIndex ) % vtx1RingSize;	// Shuffle to get the minimum index consistently first in order
			if ( idx != 0 )										// Don't do pStencil[0] again
			{
				limitPos = MaddSIMD( pStencil[idx], pPos[ pOneRing[idx] ], limitPos );
			}
		}
	}
}

FORCEINLINE fltx4 VectorNormalize( fltx4 &A )
{
	fltx4 mag_sq = Dot3SIMD( A, A );						// length^2
	fltx4 invSqrt = ReciprocalSqrtEstSIMD(mag_sq);
	return MulSIMD( A, invSqrt );
}

FORCEINLINE fltx4 VectorLength( fltx4 &A )
{
	fltx4 mag_sq = Dot3SIMD( A, A );						// length^2
	fltx4 invSqrt = ReciprocalSqrtEstSIMD(mag_sq);
	return invSqrt;
}

FORCEINLINE fltx4 CrossProduct( const fltx4 &A, const fltx4 &B )
{
#if defined( _X360 )
	return XMVector3Cross( A, B );
#elif defined( _WIN32 )
	fltx4 A1 = _mm_shuffle_ps( A, A, MM_SHUFFLE_REV( 1, 2, 0, 3 ) );
	fltx4 B1 = _mm_shuffle_ps( B, B, MM_SHUFFLE_REV( 2, 0, 1, 3 ) );
	fltx4 Result1 = MulSIMD( A1, B1 );
	fltx4 A2 = _mm_shuffle_ps( A, A, MM_SHUFFLE_REV( 2, 0, 1, 3 ) );
	fltx4 B2 = _mm_shuffle_ps( B, B, MM_SHUFFLE_REV( 1, 2, 0, 3 ) );
	fltx4 Result2 = MulSIMD( A2, B2 );
	return SubSIMD( Result1, Result2 );
#else
	fltx4 CrossVal;
	SubFloat( CrossVal, 0 ) = SubFloat( A, 1 )*SubFloat( B, 2 ) - SubFloat( A, 2 )*SubFloat( B, 1 );
	SubFloat( CrossVal, 1 ) = SubFloat( A, 2 )*SubFloat( B, 0 ) - SubFloat( A, 0 )*SubFloat( B, 2 );
	SubFloat( CrossVal, 2 ) = SubFloat( A, 0 )*SubFloat( B, 1 ) - SubFloat( A, 1 )*SubFloat( B, 0 );
	SubFloat( CrossVal, 3 ) = 0;
	return CrossVal;
#endif
}

FORCEINLINE void ComputeCatmullClarkLimitTangents( int idx, fltx4 *pPos, unsigned short *pOneRing, unsigned short vtx1RingSize,
											 unsigned short centerOffset, unsigned short bndVtx, unsigned short cornerVtx, 
											 unsigned short valence, float &loopGapAngle, fltx4 &limitTanU, fltx4 &limitTanV )
{
	VPROF_BUDGET( "ComputeCatmullClarkLimitTangents (SIMD)", _T( "SubD Rendering" ) );

	// for valence=1, no need to have separate tangents
	static const fltx4 tanUSign[4] = { Four_Ones, Four_NegativeOnes, Four_NegativeOnes, Four_Ones };
	static const fltx4 tanVSign[4] = { Four_Ones, Four_Ones, Four_NegativeOnes, Four_NegativeOnes };
	
	if (!sUseCornerTangents) cornerVtx = 0;
	
	// interior vertices
	if ( !bndVtx )
	{
		fltx4 *pStencil0 = sCCLimitTanStencil1[ valence ];
		fltx4 *pStencil1 = sCCLimitTanStencil2[ valence ];
		
		limitTanU = limitTanV = Four_Zeros;

		for ( int k = 0; k < vtx1RingSize; k++ )
		{
			limitTanU = MaddSIMD( pStencil0[k], pPos[ pOneRing[ k ] ], limitTanU );
			limitTanV = MaddSIMD( pStencil1[k], pPos[ pOneRing[ k ] ], limitTanV );
		}

	} 
	else if ( (!cornerVtx) || (cornerVtx == CORNER_WITH_SMOOTHBNDTANGENTS) )
	{
		// smooth boundary vertices
		fltx4 *pStencil0 = sCCLimitTanBndStencil1[ valence ];
		fltx4 *pStencil1 = sCCLimitTanBndStencil2[ valence ];
		
		fltx4 r0 = Four_Zeros;
		fltx4 r1 = Four_Zeros;
		
		for (int k = 0; k < vtx1RingSize; ++k)
		{
			r0 = MaddSIMD( pStencil0[k], pPos[ pOneRing[ k ] ], r0 );
			r1 = MaddSIMD( pStencil1[k], pPos[ pOneRing[ k ] ], r1 );
		}
		
		int j1 = ( centerOffset - 1 ) / 2;
		int j2 = j1 + 1;
		int k = valence - 1;
		
		if ( valence == 2 )
		{
			limitTanU = r0;
			limitTanV = r1;
		}
		else
		{
			limitTanU = AddSIMD( MulSIMD( sCCCosPI[j1][k], r0 ), MulSIMD( sCCSinPI[j1][k], r1 ) );
			limitTanV = AddSIMD( MulSIMD( sCCCosPI[j2][k], r0 ), MulSIMD( sCCSinPI[j2][k], r1 ) );
		}
	}
	else 
	{
		// Corner vertices
		if ( valence == 2 ) 
			return; 

		fltx4 *pEdgeStencil = sPosEdge1Stencil[ valence ];

		// Compute tangents
		fltx4 c0 = SubSIMD( pPos[ pOneRing[ 1 ] ], pPos[ pOneRing[ 0 ] ] );
		fltx4 c1 = SubSIMD( pPos[ pOneRing[ vtx1RingSize - 1 ] ], pPos[ pOneRing[ 0 ] ] );
		
		fltx4 e0 = MulSIMD( SubSIMD( pEdgeStencil[0], Four_Ones ), pPos[ pOneRing[ 0 ] ] );
		fltx4 e1 = e0;
		for ( int k = 1; k < 6; k++ )
		{
			e0 = MaddSIMD( pEdgeStencil[k], pPos[ pOneRing[ k ] ], e0 );
			e1 = MaddSIMD( pEdgeStencil[k], pPos[ pOneRing[ vtx1RingSize - 6 + k ] ], e1 );
		}
		
		// Compute average tangent plane normal
		fltx4 n0 = CrossProduct( c0, e0 ); 
		n0 = VectorNormalize( n0 );
		fltx4 n1 = CrossProduct( e1, c1 ); 
		n1 = VectorNormalize( n1 );
		fltx4 N = AddSIMD( n0, n1 );
		N = VectorNormalize( N );

		// Project into tangent plane
		fltx4 DotC0N = Dot3SIMD( c0, N );
		fltx4 DotC1N = Dot3SIMD( c1, N );

		c0 = SubSIMD( c0, MulSIMD( DotC0N, N ) );
		c1 = SubSIMD( c1, MulSIMD( DotC1N, N ) );
		
		fltx4 c0l = VectorLength( c0 ); 
		c0 = DivSIMD( c0, c0l );
		fltx4 c1l = VectorLength( c1 ); 
		c1 = DivSIMD( c1, c1l );
		fltx4 cAvg = MulSIMD( AddSIMD(c0l,c1l), Four_PointFives );
		
		// Compute angle
		fltx4 c0p = CrossProduct(N, c0);
		fltx4 dot1 = Dot3SIMD(c0p, c1);
		fltx4 dot2 = Dot3SIMD(c0, c1);
		
		float angle = PI - atan2( SubFloat( dot1, 0 ), -SubFloat( dot2, 0 ) );
				
		loopGapAngle = angle;
		
		// Compute final tangent vector
		int j1 = ( centerOffset - 1 ) / 2;
		int j2 = j1 + 1;
		int K = (valence - 1);
		
		static float fK[MAX_VALENCE] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 
										 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f,
										 17.0f, 18.0f };
		// Compute final tangent vector
		float flK = fK[K];
		
		fltx4 Cos0 = ReplicateX4( cos( angle*j1 / flK ) );
		fltx4 Sin0 = ReplicateX4( sin( angle*j1 / flK ) );
		fltx4 Cos1 = ReplicateX4( cos( angle*j2 / flK ) );
		fltx4 Sin1 = ReplicateX4( sin( angle*j2 / flK ) );

		limitTanU = cAvg * ( Cos0 * c0 + Sin0 * c0p );
		limitTanV = cAvg * ( Cos1 * c0 + Sin1 * c0p );	
	} 
	
	// Flip tangents so they point in u/v direction
	if ( idx & 1 )
	{
		V_swap( limitTanU, limitTanV );
	}

	limitTanU = MulSIMD( limitTanU, tanUSign[idx] );
	limitTanV = MulSIMD( limitTanV, tanVSign[idx] );
}

FORCEINLINE void ComputeACCEdgePositions( fltx4 *pPos, unsigned short *oneRing, unsigned short centerOffset,
									unsigned short bndEdge, unsigned short bndVtx0, unsigned short bndVtx1,
									unsigned short cornerVtx0, unsigned short cornerVtx1,
									unsigned short edgeBias0, unsigned short edgeBias1, 
									unsigned short val0, unsigned short val1, 
									unsigned short minOneRingOffset, unsigned short vtx1RingSize,
									fltx4 &edgePos0, fltx4 &edgePos1)
{
	VPROF_BUDGET( "ComputeACCEdgePositions (SIMD)", _T("SubD Rendering") );

	if ( bndVtx0 )
	{
		val0 = 2*(val0 - 1);
	}

	if ( bndVtx1 ) 
	{
		val1 = 2*(val1 - 1);
	}
	
	Assert( val0 <= MAX_VALENCE );
	Assert( val1 <= MAX_VALENCE );

	fltx4 *pStencil0 = (bndEdge) ? (cornerVtx0) ? sPosEdge1CornerStencil[ val0 ] : sPosEdge1BndStencil[ val0 ] : sPosEdge1Stencil[ val0 ];
	fltx4 *pStencil1 = (bndEdge) ? (cornerVtx1) ? sPosEdge2CornerStencil[ val1 ] : sPosEdge2BndStencil[ val1 ] : sPosEdge2Stencil[ val1 ];

	int kEnd = (bndEdge) ? 4 : 6;

	if ( ( edgeBias0 == 16384 ) && ( edgeBias1 == 16384 ) )
	{
		int oneRingIndex[6] = { 0, 0, 0, 0, 0, 0 };
		for ( int i = 1; i < kEnd; i++ )
		{
			oneRingIndex[i] = centerOffset + i - 1;
		}

		edgePos0 = edgePos1 = Four_Zeros;
		for ( int k = 0; k < kEnd; k++ )
		{
			int idx = ( k + minOneRingOffset ) % kEnd;	// Offset to min index to enforce evaluation order between neighboring patches
			edgePos0 = MaddSIMD( pStencil0[idx], pPos[ oneRing[ oneRingIndex[idx] ] ], edgePos0 );
			edgePos1 = MaddSIMD( pStencil1[idx], pPos[ oneRing[ oneRingIndex[idx] ] ], edgePos1 );
		}
	} 
	else 
	{
		fltx4 b0, b1;
		b1 = ReplicateX4( edgeBias0 / 32768.0f );
		b0 = SubSIMD( Four_Ones, b1 );
		edgePos0 = DivSIMD( ( Four_Valence[val0]*pPos[ oneRing[0] ] + 
					    Four_Twos*b0*pPos[ oneRing[ centerOffset] ] + 
							   b0*pPos[ oneRing[centerOffset + 1] ] + 
						Four_Twos*pPos[ oneRing[centerOffset + 2] ] + 
							   b1*pPos[ oneRing[centerOffset + 3] ] + 
					 Four_Twos*b1*pPos[ oneRing[centerOffset + 4] ] ), Four_ValencePlus5[val0] );

		b1 = ReplicateX4( edgeBias1 / 32768.0f );
		b0 = SubSIMD( Four_Ones, b1 );
		edgePos1 = DivSIMD( ( Four_Twos*pPos[ oneRing[0] ] +
					  b0*pPos[ oneRing[centerOffset + 0] ] + 
			Four_Twos*b0*pPos[ oneRing[centerOffset + 1] ] +
	  Four_Valence[val1]*pPos[ oneRing[centerOffset + 2] ] +
			Four_Twos*b1*pPos[ oneRing[centerOffset + 3] ] +
					  b1*pPos[ oneRing[centerOffset + 4] ] ), Four_ValencePlus5[val0] );
	}
}

FORCEINLINE void ComputeACCInteriorPosition( fltx4 *pPos, unsigned short *oneRing, unsigned short centerOffset, unsigned short bndVtx, unsigned short valence, fltx4 &interiorPos )
{
	VPROF_BUDGET( "ComputeACCInteriorPosition (SIMD)", _T( "SubD Rendering" ) );

	if ( bndVtx ) 
	{
		valence = valence > 2 ?  2 * (valence - 1) : 4 * (valence - 1);
	}

	Assert( valence <= MAX_VALENCE );

	fltx4 *pStencil = sPosInteriorStencil[ valence ];

	interiorPos = MulSIMD( pStencil[0], pPos[ oneRing[0] ] );
	for ( int k = 1; k < 4; k++ )
	{
		interiorPos = MaddSIMD( pStencil[k], pPos[ oneRing[ centerOffset + k - 1 ] ], interiorPos );
	}
}

FORCEINLINE void ComputeACCGeometryPatchTangents( fltx4 *Pos, fltx4 *TanU, fltx4 *TanV )
{
	//VPROF_BUDGET( "ComputeACCGeometryPatchTangents", _T("SubD Rendering") );
	TanU[0] = MulSIMD( Four_Threes, SubSIMD( Pos[1], Pos[0] ) );
	TanV[0] = MulSIMD( Four_Threes, SubSIMD( Pos[4], Pos[0] ) );
	TanU[3] = MulSIMD( Four_Threes, SubSIMD( Pos[5], Pos[4] ) );
	TanV[1] = MulSIMD( Four_Threes, SubSIMD( Pos[5], Pos[1] ) );
	TanU[6] = MulSIMD( Four_Threes, SubSIMD( Pos[9], Pos[8] ) );
	TanV[2] = MulSIMD( Four_Threes, SubSIMD( Pos[6], Pos[2] ) );
	TanU[9] = MulSIMD( Four_Threes, SubSIMD( Pos[13], Pos[12] ) );
	TanV[3] = MulSIMD( Four_Threes, SubSIMD( Pos[7], Pos[3] ) );
	TanU[1] = MulSIMD( Four_Threes, SubSIMD( Pos[2], Pos[1] ) );
	TanV[4] = MulSIMD( Four_Threes, SubSIMD( Pos[8], Pos[4] ) );
	TanU[4] = MulSIMD( Four_Threes, SubSIMD( Pos[6], Pos[5] ) );
	TanV[5] = MulSIMD( Four_Threes, SubSIMD( Pos[9], Pos[5] ) );
	TanU[7] = MulSIMD( Four_Threes, SubSIMD( Pos[10], Pos[9] ) );
	TanV[6] = MulSIMD( Four_Threes, SubSIMD( Pos[10], Pos[6] ) );
	TanU[10] = MulSIMD( Four_Threes, SubSIMD( Pos[14], Pos[13] ) );
	TanV[7] = MulSIMD( Four_Threes, SubSIMD( Pos[11], Pos[7] ) );
	TanU[2] = MulSIMD( Four_Threes, SubSIMD( Pos[3], Pos[2] ) );
	TanV[8] = MulSIMD( Four_Threes, SubSIMD( Pos[12], Pos[8] ) );
	TanU[5] = MulSIMD( Four_Threes, SubSIMD( Pos[7], Pos[6] ) );
	TanV[9] = MulSIMD( Four_Threes, SubSIMD( Pos[13], Pos[9] ) );
	TanU[8] = MulSIMD( Four_Threes, SubSIMD( Pos[11], Pos[10] ) );
	TanV[10] = MulSIMD( Four_Threes, SubSIMD( Pos[14], Pos[10] ) );
	TanU[11] = MulSIMD( Four_Threes, SubSIMD( Pos[15], Pos[14] ) );
	TanV[11] = MulSIMD( Four_Threes, SubSIMD( Pos[15], Pos[11] ) );
}

void ComputeACCAllPatches( fltx4* pPos, TopologyIndexStruct* quad, Vector4D* Pos, Vector4D* TanU, Vector4D* TanV, bool bRegularPatch )
{
	VPROF_BUDGET( "ComputeACCAllPatches (SIMD)", _T( "SubD Rendering" ) );
	int accCorner[]     = { 0, 3, 15, 12 };
	int accEdge1[]      = { 4, 2, 11, 13 };
	int accEdge2[]      = { 8, 1, 7,  14 };
	int accInterior[]   = { 5, 6, 10, 9  };
	int accTanCornerU[] = { 0, 2, 11, 9  };  // counterclockwise orders!
	int accTanCornerV[] = { 0, 3, 11, 8  };

	fltx4 OutPos[16], OutTanU[16], OutTanV[16];

	// Point to four one-rings
	int vtx1RingStart = 0;
	unsigned short* pOneRing[4];
	for ( int i = 0; i < 4; i++ )
	{
		unsigned short vtx1RingSize = quad->vtx1RingSize[i];
		pOneRing[i] = &(quad->oneRing[vtx1RingStart]);
		vtx1RingStart += vtx1RingSize;
	}

	{
		VPROF_BUDGET( "ComputeACCAllPatches - Geometry Control Points (SIMD)", _T( "SubD Rendering" ) );

		ComputeCatmullClarkLimitPosition( pPos, pOneRing[0], quad->vtx1RingSize[0], quad->minOneRingOffset[0], quad->bndVtx[0], quad->cornerVtx[0], quad->valences[0], OutPos[ accCorner[0] ] );
		ComputeCatmullClarkLimitPosition( pPos, pOneRing[1], quad->vtx1RingSize[1], quad->minOneRingOffset[1], quad->bndVtx[1], quad->cornerVtx[1], quad->valences[1], OutPos[ accCorner[1] ] );
		ComputeCatmullClarkLimitPosition( pPos, pOneRing[2], quad->vtx1RingSize[2], quad->minOneRingOffset[2], quad->bndVtx[2], quad->cornerVtx[2], quad->valences[2], OutPos[ accCorner[2] ] );
		ComputeCatmullClarkLimitPosition( pPos, pOneRing[3], quad->vtx1RingSize[3], quad->minOneRingOffset[3], quad->bndVtx[3], quad->cornerVtx[3], quad->valences[3], OutPos[ accCorner[3] ] );

		ComputeACCEdgePositions( pPos, pOneRing[0], quad->vtx1RingCenterQuadOffset[0], 
								 quad->bndEdge[3], quad->bndVtx[0], quad->bndVtx[3], 
								 quad->cornerVtx[0], quad->cornerVtx[3],
								 quad->edgeBias[6], quad->edgeBias[7],
								 quad->valences[0], quad->valences[3], 
								 quad->minOneRingOffset[0], quad->vtx1RingSize[0],
								 OutPos[accEdge1[0]], OutPos[accEdge2[0]] );
		ComputeACCEdgePositions( pPos, pOneRing[1], quad->vtx1RingCenterQuadOffset[1], 
								 quad->bndEdge[0], quad->bndVtx[1], quad->bndVtx[0], 
								 quad->cornerVtx[1], quad->cornerVtx[0],
								 quad->edgeBias[0], quad->edgeBias[1],
								 quad->valences[1], quad->valences[0],
								 quad->minOneRingOffset[1], quad->vtx1RingSize[1],
								 OutPos[accEdge1[1]], OutPos[accEdge2[1]] );
		ComputeACCEdgePositions( pPos, pOneRing[2], quad->vtx1RingCenterQuadOffset[2], 
								 quad->bndEdge[1], quad->bndVtx[2], quad->bndVtx[1], 
								 quad->cornerVtx[2], quad->cornerVtx[1],
								 quad->edgeBias[2], quad->edgeBias[3],
								 quad->valences[2], quad->valences[1], 
								 quad->minOneRingOffset[2], quad->vtx1RingSize[2],
								 OutPos[accEdge1[2]], OutPos[accEdge2[2]] );
		ComputeACCEdgePositions( pPos, pOneRing[3], quad->vtx1RingCenterQuadOffset[3], 
								 quad->bndEdge[2], quad->bndVtx[3], quad->bndVtx[2], 
								 quad->cornerVtx[3], quad->cornerVtx[2],
								 quad->edgeBias[4], quad->edgeBias[5],
								 quad->valences[3], quad->valences[2], 
								 quad->minOneRingOffset[3], quad->vtx1RingSize[3],
								 OutPos[accEdge1[3]], OutPos[accEdge2[3]] );

		ComputeACCInteriorPosition( pPos, pOneRing[0], quad->vtx1RingCenterQuadOffset[0], quad->bndVtx[0], quad->valences[0], OutPos[ accInterior[0] ] );
		ComputeACCInteriorPosition( pPos, pOneRing[1], quad->vtx1RingCenterQuadOffset[1], quad->bndVtx[1], quad->valences[1], OutPos[ accInterior[1] ] );
		ComputeACCInteriorPosition( pPos, pOneRing[2], quad->vtx1RingCenterQuadOffset[2], quad->bndVtx[2], quad->valences[2], OutPos[ accInterior[2] ] );
		ComputeACCInteriorPosition( pPos, pOneRing[3], quad->vtx1RingCenterQuadOffset[3], quad->bndVtx[3], quad->valences[3], OutPos[ accInterior[3] ] );
	}

#if !defined( NO_TANGENTS )
	// Don't compute tangents for regular patches
#if defined( SEPARATE_REGULAR_AND_EXTRA )
	if ( !bRegularPatch )
#endif
	{
		VPROF_BUDGET( "ComputeACCAllPatches - Tangents (SIMD)", _T( "SubD Rendering" ) );

		ComputeACCGeometryPatchTangents( OutPos, OutTanU, OutTanV );

		float flLoopGap[4];
		flLoopGap[0] = ( M_PI2 * quad->loopGapAngle[0] ) / 65535.0f;
		flLoopGap[1] = ( M_PI2 * quad->loopGapAngle[1] ) / 65535.0f;
		flLoopGap[2] = ( M_PI2 * quad->loopGapAngle[2] ) / 65535.0f;
		flLoopGap[3] = ( M_PI2 * quad->loopGapAngle[3] ) / 65535.0f;
		if ( !sShowACCGeometryTangents ) 
		{
			{
				ComputeCatmullClarkLimitTangents( 0, pPos, pOneRing[0], quad->vtx1RingSize[0], quad->vtx1RingCenterQuadOffset[0], 
					quad->bndVtx[0], quad->cornerVtx[0], quad->valences[0], flLoopGap[0], OutTanU[ accTanCornerU[0] ], OutTanV[ accTanCornerV[0] ] );
				ComputeCatmullClarkLimitTangents( 1, pPos, pOneRing[1], quad->vtx1RingSize[1], quad->vtx1RingCenterQuadOffset[1], 
					quad->bndVtx[1], quad->cornerVtx[1], quad->valences[1], flLoopGap[1], OutTanU[ accTanCornerU[1] ], OutTanV[ accTanCornerV[1] ] );
				ComputeCatmullClarkLimitTangents( 2, pPos, pOneRing[2], quad->vtx1RingSize[2], quad->vtx1RingCenterQuadOffset[2], 
					quad->bndVtx[2], quad->cornerVtx[2], quad->valences[2], flLoopGap[2], OutTanU[ accTanCornerU[2] ], OutTanV[ accTanCornerV[2] ] );
				ComputeCatmullClarkLimitTangents( 3, pPos, pOneRing[3], quad->vtx1RingSize[3], quad->vtx1RingCenterQuadOffset[3], 
					quad->bndVtx[3], quad->cornerVtx[3], quad->valences[3], flLoopGap[3], OutTanU[ accTanCornerU[3] ], OutTanV[ accTanCornerV[3] ] );
			}

			// compute correction component to boundary tangents for tangent plane continuity
			//                             /TanV/ /TanU/ / TanV / /TanU/
			static int   CB_CornerIdx[]   = {0,1,2, 3,7,11, 11,10,9, 8,4,0 };
			static int   CB_InteriorIdx[] = {1,2,   5,8,    10,9,    6,3 };
			static fltx4 CB_sign[4] = {Four_Ones,Four_NegativeOnes,Four_Ones,Four_NegativeOnes};

			{
				// Unroll, since the compiler wants to keep it rolled, and we get better perf unrolled
				{
					fltx4 u00 = OutTanU[CB_CornerIdx[0]];
					fltx4 u10 = MulSIMD( OutTanU[CB_CornerIdx[1]], Four_Twos );
					fltx4 u20 = OutTanU[CB_CornerIdx[2]];

					int val0 = quad->valences[0];	int val1 = quad->valences[1];
					if ( quad->bndVtx[0] ) val0--;
					if ( quad->bndVtx[1] ) val1--;

					fltx4 c0 = ReplicateX4( cosf( (flLoopGap[0]) / val0 ) );
					fltx4 c1 = ReplicateX4( cosf( (flLoopGap[1]) / val1 ) );

					fltx4 A = MulSIMD( c0, u10 ); fltx4 B = MulSIMD( c1, u00 );	fltx4 C = MulSIMD( c0, u20 ); fltx4 D = MulSIMD( c1, u10 );
					fltx4 E = DivSIMD( SubSIMD( A, B ), Four_Threes ); fltx4 F = DivSIMD( SubSIMD( C, D ), Four_Threes );

					OutTanV[CB_InteriorIdx[0] ] = AddSIMD( OutTanV[CB_InteriorIdx[0] ], E );
					OutTanV[CB_InteriorIdx[1] ] = AddSIMD( OutTanV[CB_InteriorIdx[1] ], F );
				}

				{
					fltx4 u00 = OutTanV[CB_CornerIdx[3]];
					fltx4 u10 = MulSIMD( OutTanV[CB_CornerIdx[4]], Four_Twos );
					fltx4 u20 = OutTanV[CB_CornerIdx[5]];

					int val0 = quad->valences[1];	int val1 = quad->valences[2];
					if ( quad->bndVtx[1] ) val0--;
					if ( quad->bndVtx[2] ) val1--;

					fltx4 c0 = ReplicateX4( cosf( (flLoopGap[1]) / val0 ) );
					fltx4 c1 = ReplicateX4( cosf( (flLoopGap[2]) / val1 ) );

					fltx4 A = MulSIMD( c0, u10 ); fltx4 B = MulSIMD( c1, u00 );	fltx4 C = MulSIMD( c0, u20 ); fltx4 D = MulSIMD( c1, u10 );
					fltx4 E = DivSIMD( SubSIMD( A, B ), Four_Threes ); fltx4 F = DivSIMD( SubSIMD( C, D ), Four_Threes );

					OutTanU[CB_InteriorIdx[2] ] = SubSIMD( OutTanU[CB_InteriorIdx[2] ], E );
					OutTanU[CB_InteriorIdx[3] ] = SubSIMD( OutTanU[CB_InteriorIdx[3] ], F );
				}

				{
					fltx4 u00 = OutTanU[CB_CornerIdx[6]];
					fltx4 u10 = MulSIMD( OutTanU[CB_CornerIdx[7]], Four_Twos );
					fltx4 u20 = OutTanU[CB_CornerIdx[8]];

					int val0 = quad->valences[2];	int val1 = quad->valences[3];
					if ( quad->bndVtx[2] ) val0--;
					if ( quad->bndVtx[3] ) val1--;

					fltx4 c0 = ReplicateX4( cosf( (flLoopGap[2]) / val0 ) );
					fltx4 c1 = ReplicateX4( cosf( (flLoopGap[3]) / val1 ) );

					fltx4 A = MulSIMD( c0, u10 ); fltx4 B = MulSIMD( c1, u00 );	fltx4 C = MulSIMD( c0, u20 ); fltx4 D = MulSIMD( c1, u10 );
					fltx4 E = DivSIMD( SubSIMD( A, B ), Four_Threes ); fltx4 F = DivSIMD( SubSIMD( C, D ), Four_Threes );

					OutTanV[CB_InteriorIdx[4] ] = AddSIMD( OutTanV[CB_InteriorIdx[4] ], E );
					OutTanV[CB_InteriorIdx[5] ] = AddSIMD( OutTanV[CB_InteriorIdx[5] ], F );
				}

				{
					fltx4 u00 = OutTanV[CB_CornerIdx[9]];
					fltx4 u10 = MulSIMD( OutTanV[CB_CornerIdx[10]], Four_Twos );
					fltx4 u20 = OutTanV[CB_CornerIdx[11]];

					int val0 = quad->valences[3];	int val1 = quad->valences[0];
					if ( quad->bndVtx[3] ) val0--;
					if ( quad->bndVtx[0] ) val1--;

					fltx4 c0 = ReplicateX4( cosf( (flLoopGap[3]) / val0 ) );
					fltx4 c1 = ReplicateX4( cosf( (flLoopGap[0]) / val1 ) );

					fltx4 A = MulSIMD( c0, u10 ); fltx4 B = MulSIMD( c1, u00 );	fltx4 C = MulSIMD( c0, u20 ); fltx4 D = MulSIMD( c1, u10 );
					fltx4 E = DivSIMD( SubSIMD( A, B ), Four_Threes ); fltx4 F = DivSIMD( SubSIMD( C, D ), Four_Threes );

					OutTanU[CB_InteriorIdx[6] ] = SubSIMD( OutTanU[CB_InteriorIdx[6] ], E );
					OutTanU[CB_InteriorIdx[7] ] = SubSIMD( OutTanU[CB_InteriorIdx[7] ], F );
				}
			}
		}

		StoreAlignedSIMD( (float*)&TanU[0], OutTanU[0] );
		StoreAlignedSIMD( (float*)&TanU[1], OutTanU[1] );
		StoreAlignedSIMD( (float*)&TanU[2], OutTanU[2] );
		StoreAlignedSIMD( (float*)&TanU[3], OutTanU[3] );
		StoreAlignedSIMD( (float*)&TanU[4], OutTanU[4] );
		StoreAlignedSIMD( (float*)&TanU[5], OutTanU[5] );
		StoreAlignedSIMD( (float*)&TanU[6], OutTanU[6] );
		StoreAlignedSIMD( (float*)&TanU[7], OutTanU[7] );
		StoreAlignedSIMD( (float*)&TanU[8], OutTanU[8] );
		StoreAlignedSIMD( (float*)&TanU[9], OutTanU[9] );
		StoreAlignedSIMD( (float*)&TanU[10], OutTanU[10] );
		StoreAlignedSIMD( (float*)&TanU[11], OutTanU[11] );

		StoreAlignedSIMD( (float*)&TanV[0], OutTanV[0] );
		StoreAlignedSIMD( (float*)&TanV[1], OutTanV[1] );
		StoreAlignedSIMD( (float*)&TanV[2], OutTanV[2] );
		StoreAlignedSIMD( (float*)&TanV[3], OutTanV[3] );
		StoreAlignedSIMD( (float*)&TanV[4], OutTanV[4] );
		StoreAlignedSIMD( (float*)&TanV[5], OutTanV[5] );
		StoreAlignedSIMD( (float*)&TanV[6], OutTanV[6] );
		StoreAlignedSIMD( (float*)&TanV[7], OutTanV[7] );
		StoreAlignedSIMD( (float*)&TanV[8], OutTanV[8] );
		StoreAlignedSIMD( (float*)&TanV[9], OutTanV[9] );
		StoreAlignedSIMD( (float*)&TanV[10], OutTanV[10] );
		StoreAlignedSIMD( (float*)&TanV[11], OutTanV[11] );
	}

#endif

	StoreAlignedSIMD( (float*)&Pos[0], OutPos[0] );
	StoreAlignedSIMD( (float*)&Pos[1], OutPos[1] );
	StoreAlignedSIMD( (float*)&Pos[2], OutPos[2] );
	StoreAlignedSIMD( (float*)&Pos[3], OutPos[3] );
	StoreAlignedSIMD( (float*)&Pos[4], OutPos[4] );
	StoreAlignedSIMD( (float*)&Pos[5], OutPos[5] );
	StoreAlignedSIMD( (float*)&Pos[6], OutPos[6] );
	StoreAlignedSIMD( (float*)&Pos[7], OutPos[7] );
	StoreAlignedSIMD( (float*)&Pos[8], OutPos[8] );
	StoreAlignedSIMD( (float*)&Pos[9], OutPos[9] );
	StoreAlignedSIMD( (float*)&Pos[10], OutPos[10] );
	StoreAlignedSIMD( (float*)&Pos[11], OutPos[11] );
	StoreAlignedSIMD( (float*)&Pos[12], OutPos[12] );
	StoreAlignedSIMD( (float*)&Pos[13], OutPos[13] );
	StoreAlignedSIMD( (float*)&Pos[14], OutPos[14] );
	StoreAlignedSIMD( (float*)&Pos[15], OutPos[15] );
}

#endif