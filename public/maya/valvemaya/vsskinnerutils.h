//====== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. =====
//
// vsskinnerutils
//
//=============================================================================


#ifndef VSSKINNERUTILS_H
#define VSSKINNERUTILS_H


// Maya includes
#include <maya/MDagPath.h>
#include <maya/MSelectionList.h>
#include <maya/MMatrix.h>


// Valve includes
#include "mathlib/vmatrix.h"


#if defined( _WIN32 )
#pragma once
#endif


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CVsSkinnerUtils
{
public:

	static MDagPath GetSpecifiedSkinnerNode( const MSelectionList &iList );

	static MStatus GetSpecifiedSkinnerNodes( const MSelectionList &iList, MSelectionList &oList );

	static MStatus FindSkinnerNodesInHierarchy( const MDagPath &iDagPath, MSelectionList &oList );

	static MStatus ConnectedToSkinnerNode( const MDagPath &iDagPath, MDagPath &oDagPath );

	static MStatus IsSkinnerNode( const MObject &iObject );

	static MStatus IsSkinnerNode( const MDagPath &iDagPath );

	static MStatus IsSkinnerVol( const MObject &iObject );

	static MStatus IsSkinnerVol( const MDagPath &iDagPath );

	static MStatus IsSkinnerJoint( const MObject &iObject );

	static MStatus IsSkinnerJoint( const MDagPath &mDagPath );

	static MStatus IsSkinnerBone( const MObject &iObject );

	static MObject GetSkinnerBoneFromSkinnerJoint( const MObject &iObject );

	static MObject GetSkinnerBoneFromSkinnerJoint( const MDagPath &iDagPath );

	static VMatrix MMatrixToVMatrix( const MMatrix &mMatrix );
};


#endif // VSSKINNERUTILS_H