//======= Copyright © 1996-2008, Valve Corporation, All rights reserved. ======
//
// Purpose: MDL Import class for Maya
//
//=============================================================================

#ifndef MDLIMPORT_H
#define MDLIMPORT_H


// Maya includes
#include <maya/MDagPathArray.h>
#include <maya/MMatrix.h>
#include <maya/MObjectArray.h>
#include <maya/MSelectionList.h>


// Valve includes
#include "datacache/imdlcache.h"
#include "ValveMaya/Undo.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct mstudioboneweight_t;


namespace ValveMaya
{

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CMDLImport
{
public:
	CMDLImport();

	CMDLImport( ValveMaya::CUndo &undo );

	MStatus DoIt( const char *pFilename, MSelectionList &createdNodes );

	MStatus DoIt( const char *pFilename );

	bool m_bOptMeshOnly : 1;		// Only import meshes
	bool m_bOptSingleMesh : 1;		// Merge all meshes in a MDL into a single Maya mesh
	bool m_bOptVstInfo : 1;			// Group everything under a vstInfo node
	bool m_bOptImportSkeleton : 1;	// Import the skeleton
	bool m_bOptImportMesh : 1;		// Import the mesh
	bool m_bOptEngineSpace : 1;		// Import data in engine space
	bool m_bOptNormals : 1;			// Set normals on mesh
	bool m_bOptEdgeHardness : 1;	// Set edge hard/softness data
	bool m_bOptSkinMeshes : 1;		// Apply bone weights to meshes
	int m_nOptLod;					// Import the specified LOD only, -2 means root LOD, -1 means all LODs, otherwise the LOD that matches, or all if no match
	int m_nOptSkin;					// Import the specified Skin only, -1 means all skins, otherwise the skin that matches or all if no match
	MStringArray m_optRemovePrefix;	// A list of prefixes to remove on import

protected:
	void Init();

	static MDLHandle_t GetMDLHandle( const char *pFilename );

	MObject CreateVstInfo( const char *pModelName, MSelectionList &createdNodes );

	static MStatus AddDagMObject( const MObject &dagObj, MSelectionList &createdNodes );

	void ImportBones( MDLHandle_t mdlHandle, const MObject &parentObj, MDagPathArray &mayaBones, MSelectionList &createdNodes );

	static MStatus ApplyCompDefaultShader( MObject &geoObj, ValveMaya::CUndo &undo );

	static MObject FindOrCreateFileObj( ValveMaya::CUndo &undo, const MString &materialPath, MSelectionList &createdNodes );

	MStatus AssignShadingGroup(
		ValveMaya::CUndo &undo,
		const MDagPath &meshDagPath,
		MIntArray &faceList,
		const MStringArray &materialPaths,
		MSelectionList &createdList ) const;

	void SkinMesh( MDagPath &meshDagPath, const MDagPathArray &mayaBones, const CUtlVector< const mstudioboneweight_t * > &tmpVertexBoneWeightArray );

	void ImportMeshes( MDLHandle_t mdlHandle, MObject parentObj, const MDagPathArray &mayaBones, MSelectionList &createdNodes );

	void ReorientMdl( MDLHandle_t mdlHandle, MObject &vstInfoObj, const MDagPathArray &mayaBones );

	static void InitBones( Vector p[ MAXSTUDIOBONES ], QuaternionAligned q[ MAXSTUDIOBONES ], studiohdr_t *pHdr );

	static void TransformBones( Vector p[ MAXSTUDIOBONES ], QuaternionAligned q[ MAXSTUDIOBONES ], studiohdr_t *pHdr );

	static void ComputeSequenceBones( Vector p[ MAXSTUDIOBONES ], QuaternionAligned q[ MAXSTUDIOBONES ], studiohdr_t *pHdr, int nSequence = 0, float flTime = 0.0f );

	bool ComputeMdlToMayaMatrix( MMatrix &mMdlToMaya, studiohdr_t* pHdr ) const;

	void TransformPointArray( MFloatPointArray &vertexArray, const MMatrix &mMatrix ) const;

	void TransformNormalArray( MVectorArray &normalArray, const MMatrix &mMatrix ) const;

	MString GetBoneName( MString boneName ) const;

	ValveMaya::CUndo &m_undo;
	ValveMaya::CUndo m_tmpUndo;
};

}
#endif // MDLIMPORT_H