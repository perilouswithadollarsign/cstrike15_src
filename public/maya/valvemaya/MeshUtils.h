//======= Copyright © 1996-2008, Valve Corporation, All rights reserved. ======
//
// Purpose:
//
//=============================================================================

#ifndef MESHUTIL_H
#define MESHUTIL_H

#include <maya/MDagPath.h>
#include <maya/MVector.h>

namespace ValveMaya
{
	MStatus SetEdgeHardnessFromNormals( MObject &meshObj, double dTol = MVector_kTol );

	MStatus SetEdgeHardnessFromNormals( const MDagPath &meshDagPath, double dTol = MVector_kTol );
}

#endif // MESHUTIL_H
