#ifndef R_STUDIOSUBD_PATCHES_H
#define R_STUDIOSUBD_PATCHES_H

// Maximum valence we expect to encounter for extraordinary vertices
#define MAX_VALENCE 19

#define CORNER_WITH_SMOOTHBNDTANGENTS 2

#include "mathlib/mathlib.h"
#include "mathlib/vector4d.h"
#include "mathlib/ssemath.h"
#include "tier0/vprof.h"

#define ALIGN(n) __declspec( align( n ) )

// Undef this to use the unoptimized conversion routines
#define USE_OPT

// Uncomment this to separate computation between regular patches and extraordinary patches
#define SEPARATE_REGULAR_AND_EXTRA

// Uncomment this to disable tangent patch computation
//#define NO_TANGENTS

// if you change this make sure to adjust NUM_TOPOLOGY_INDICES_ATTRIBUTES in studiorendercontext.cpp as well!
struct TopologyIndexStruct
{
	unsigned short *vtx1RingSize;
	unsigned short *vtx1RingCenterQuadOffset;

	unsigned short *valences;
	unsigned short *minOneRingOffset;

	unsigned short *bndVtx;
	unsigned short *bndEdge;
	unsigned short *cornerVtx;

	unsigned short *loopGapAngle;
	
	unsigned short *edgeBias;

	unsigned short *nbCornerVtx;

	unsigned short *oneRing;

	unsigned short *vUV0;
	unsigned short *vUV1;
	unsigned short *vUV2;
	unsigned short *vUV3;
};

void set_ShowACCGeometryTangents(bool v);
void set_CornerCorrection(bool v);
void set_UseCornerTangents(bool v);

void FillTables(); // fill patch stencil buffers

// compute patch control points
#if defined( USE_OPT )
void ComputeACCAllPatches( fltx4* pWSVertices, TopologyIndexStruct* quad, Vector4D* Pos, Vector4D* TanU, Vector4D* TanV, bool bRegularPatch = false );
#else
void ComputeACCGeometryPatch( Vector4D* pWSVertices, TopologyIndexStruct *quad, Vector4D* Pos);
void ComputeACCTangentPatches( Vector4D* pWSVertices, TopologyIndexStruct* quad, Vector4D* Pos, Vector4D* TanU, Vector4D* TanV );
#endif

void EvaluateACC(unsigned short nPoints, Vector2D *UVs, Vector4D *cpP, Vector4D *cpU, Vector4D *cpV, Vector4D *pos, Vector *nor, Vector2D *uv, Vector4D *tanU, Vector4D *tanV );
void EvaluateGregory(unsigned short nPoints, Vector2D *UVs, Vector4D *cpP, Vector4D *pos, Vector *nor, Vector2D *uv, Vector4D *tanU, Vector4D *tanV );


#endif //R_STUDIOSUBD_PATCHES_H
