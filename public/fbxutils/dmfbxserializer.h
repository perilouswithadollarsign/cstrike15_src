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
#include "movieobjects/dmeaxissystem.h"
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

	// Feed the CDmElement returned by ReadFBX to see if there were non-fatal conversion errors which the user should be informed about
	static bool HasConversionErrors( CDmElement *pDmRoot );

	// Feed the CDmElement returned by ReadFBX to see if there were non-fatal conversion errors which the user should be informed about, they are added to pConversionErrors, if no errors, pConversionErrors is not touched
	static void GetConversionErrors( CDmElement *pDmRoot, CUtlVector< CUtlString > *pConversionErrors );

	bool Verbose1() const { return m_nOptVerbosity >= 1; }
	bool Verbose2() const { return m_nOptVerbosity >= 2; }

	// UtlHashtable is non-functional in //ValveGames/main 5/16/2012
	typedef CUtlMap< FbxNode *, CDmeDag * > FbxToDmxMap_t;

	typedef void( *HandleUVFunc_t )( CUtlVector< int > &, FbxMesh *, FbxGeometryElementUV *, int, int );

protected:
	struct UVChannelData_t
	{
		FbxGeometryElementUV *m_pFbxElementUV;
		HandleUVFunc_t m_pFunc;
		CUtlVector< int > m_nIndices;
	};

	//-----------------------------------------------------------------------------
	// For oddly encoded vertex paint data coming from 3DSMax
	//-----------------------------------------------------------------------------
	struct UVColorChannelData_t
	{
	public:
		UVColorChannelData_t()
		{
			for ( int i = 0; i < ARRAYSIZE( m_uvChannelData ); ++i )
			{
				m_uvChannelData[i].m_pFbxElementUV = NULL;
				m_uvChannelData[i].m_pFunc = NULL;
			}
		}

		CUtlString m_sChannelName;

		UVChannelData_t m_uvChannelData[3];
	};


	FbxScene *LoadFbxScene( FbxTime::EMode &eFbxTimeMode, const char *pszFilename );
	void LoadModelAndSkeleton_R( FbxToDmxMap_t &fbxToDmxMap, CDmeModel *pDmeModel, CDmeDag *pDmeDagParent, FbxNode *pFbxNode, bool bAnimation, int nDepth ) const;
	CDmeDag *FbxNodeToDmeDag( CDmeDag *pDmeDagParent, FbxNode *pFbxNode, const char *pszDmeType, FbxMatrix *pmOutScale = NULL ) const;
	CDmeMesh *FbxShapeToDmeMesh( CDmeDag *pDmeDag, FbxNode *pFbxNode, const FbxMatrix &mScale ) const;
	bool FbxMeshToDmeFaceSets( CDmeDag *pDmeDag, CDmeMesh *pDmeMesh, FbxMesh *pFbxMesh, CUtlVector< int > &nPolygonToFaceSetMap ) const;
	bool FindMaterialResource( CUtlString &sOutMaterialPath, const char *pszInMaterialName, CUtlVector< CUtlString > &materialSearchErrorList ) const;
	bool GetFbxMaterialPathFromFbxFileTexture( CUtlString &sMaterialPath, FbxFileTexture *pFileTexture, CUtlVector< CUtlString > &materialSearchErrorList ) const;
	bool GetFbxMaterialPath( CUtlString &sMaterialPath, FbxSurfaceMaterial *pFbxMat, CUtlVector< CUtlString > &materialSearchErrorList ) const;
	void SkinMeshes_R( const FbxToDmxMap_t &fbxToDmxMap, CDmeModel *pDmeModel, FbxNode *pFbxNode ) const;
	void SkinMesh( CDmeDag *pDmeDag, const FbxToDmxMap_t &fbxToDmxMap, CDmeModel *pDmeModel, FbxNode *pFbxNode ) const;
	void AddBlendShapes_R( const FbxToDmxMap_t &fbxToDmxMap, CDmElement *pDmeRoot, FbxNode *pFbxNode ) const;
	void AddBlendShape( CDmeDag *pDmeDag, const FbxToDmxMap_t &fbxToDmxMap, CDmElement *pDmeRoot, FbxNode *pFbxNode ) const;
	CDmeCombinationOperator *FindOrCreateComboOp( CDmElement *pDmeRoot ) const;
	bool FindOrCreateControl( CDmeCombinationOperator *pDmeComboOp, const char *pszName ) const;

	void GetName( CUtlString &sCleanName, const FbxNode *pFbxNode ) const;
	void CleanupName( CUtlString &sCleanName, const char *pszName ) const;

	void LoadAnimation( CDmElement *pDmeRoot, CDmeModel *pDmeModel, const FbxToDmxMap_t &fbxToDmxMap, FbxScene *pFbxScene, FbxNode *pFbxRootNode, FbxTime::EMode eFbxTimeMode ) const;

	static FbxManager *GetFbxManager();

	static void AddConversionError( DmFileId_t nDmFileId, const char *pszErrorMsg );

	void ComputeVstFlexSliderAnimDataList_R( FbxAnimLayer *pFbxAnimLayer, CUtlVector< struct FbxDmxAnimData_t * > &animDataList, class CDmeChannelsClip *pDmeChannelsClip, const CDmFbxSerializer::FbxToDmxMap_t &fbxToDmxMap, FbxNode *pFbxNode ) const;
	void ComputeAnimDataList_R( FbxAnimLayer *pFbxAnimLayer, CUtlVector< struct FbxDmxAnimData_t * > &animDataList, class CDmeChannelsClip *pDmeChannelsClip, const CDmFbxSerializer::FbxToDmxMap_t &fbxToDmxMap, FbxNode *pFbxNode, struct FbxDmxAnimData_t *pAnimDataParent ) const;
	void AddColorData( class CDmeVertexData *pDmeVertexData, const CUtlVector< int > &nIndices, const FbxGeometryElementVertexColor *pFbxElement ) const;
	void AddUVColorData( CDmeVertexData *pDmeVertexData, const UVColorChannelData_t &uvColorData ) const;

public:
	int m_nOptVerbosity;
	bool m_bOptUnderscoreForCorrectors;
	bool m_bAnimation;
	bool m_bReturnDmeModel;

	CDmeAxisSystem::Axis_t m_eOptUpAxis;
	CDmeAxisSystem::ForwardParity_t m_eOptForwardParity;
	CDmeAxisSystem::CoordSys_t m_eCoordSys;

	CUtlVector< CUtlString > m_sOptMaterialSearchPathList;

	float m_flOptScale;

};


#endif // DMFBXSERIALIZER_H
