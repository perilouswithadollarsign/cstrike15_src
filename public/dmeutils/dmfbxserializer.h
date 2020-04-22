//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
// Autodesk FBX <-> Valve DMX
//
//=============================================================================


#ifndef DMFBXSERIALIZER_H
#define DMFBXSERIALIZER_H


#if defined( _WIN32 )
#pragma once
#endif


// FBX includes
#include <fbxsdk.h>


// Valve includes
#include "datamodel/idatamodel.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlmap.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeCombinationOperator;
class CDmeDag;
class CDmeMesh;
class CDmeModel;


//-----------------------------------------------------------------------------
// Serialization class for FBX files
//-----------------------------------------------------------------------------
class CDmFbxSerializer
{
public:
	CDmFbxSerializer();
	virtual ~CDmFbxSerializer();

	const char *GetName() const { return "fbx"; }
	const char *GetDescription() const { return "Autodesk® FBX®"; }

	// CDmFbxSerializer
	CDmElement *ReadFBX( const char *pszFilename );

	bool Verbose1() const { return m_nOptVerbosity >= 1; }
	bool Verbose2() const { return m_nOptVerbosity >= 2; }

	// UtlHashtable is non-functional in //ValveGames/main 5/16/2012
	typedef CUtlMap< FbxNode *, CDmeDag * > FbxToDmxMap_t;

protected:

	FbxManager *CreateFbxManager();
	void DestroyFbxManager();
	FbxScene *LoadFbxScene( const char *pszFilename );
	void LoadModelAndSkeleton_R( FbxToDmxMap_t &fbxToDmxMap, CDmeModel *pDmeModel, CDmeDag *pDmeDagParent, FbxNode *pFbxNode, bool bAnimation, int nDepth ) const;
	CDmeDag *FbxNodeToDmeDag( CDmeDag *pDmeDagParent, FbxNode *pFbxNode, const char *pszDmeType ) const;
	CDmeMesh *FbxShapeToDmeMesh( CDmeDag *pDmeDag, FbxNode *pFbxNode ) const;
	bool FbxMeshToDmeFaceSets( CDmeDag *pDmeDag, CDmeMesh *pDmeMesh, FbxMesh *pFbxMesh, CUtlVector< int > &nPolygonToFaceSetMap ) const;
	void GetDmeMaterialPathFromFbxFileTexture( CUtlString &sMaterialPath, FbxFileTexture *pFileTexture ) const;
	void GetFbxMaterialNameAndPath( CUtlString &sMaterialName, CUtlString &sMaterialPath, FbxSurfaceMaterial *pFbxMat ) const;
	void SkinMeshes_R( const FbxToDmxMap_t &fbxToDmxMap, CDmeModel *pDmeModel, FbxNode *pFbxNode ) const;
	void SkinMesh( CDmeDag *pDmeDag, const FbxToDmxMap_t &fbxToDmxMap, CDmeModel *pDmeModel, FbxNode *pFbxNode ) const;
	void AddBlendShapes_R( const FbxToDmxMap_t &fbxToDmxMap, CDmElement *pDmeRoot, FbxNode *pFbxNode ) const;
	void AddBlendShape( CDmeDag *pDmeDag, const FbxToDmxMap_t &fbxToDmxMap, CDmElement *pDmeRoot, FbxNode *pFbxNode ) const;
	CDmeCombinationOperator *FindOrCreateComboOp( CDmElement *pDmeRoot ) const;
	bool FindOrCreateControl( CDmeCombinationOperator *pDmeComboOp, const char *pszName ) const;

	void GetName( CUtlString &sCleanName, const FbxNode *pFbxNode ) const;
	void CleanupName( CUtlString &sCleanName, const char *pszName ) const;

	void LoadAnimation( CDmElement *pDmeRoot, CDmeModel *pDmeModel, const FbxToDmxMap_t &fbxToDmxMap, FbxScene *pFbxScene, FbxNode *pFbxRootNode ) const;

	FbxManager *m_pFbxManager;

public:
	int m_nOptVerbosity;
	bool m_bOptUnderscoreForCorrectors;
	bool m_bAnimation;

};


#endif // DMFBXSERIALIZER_H