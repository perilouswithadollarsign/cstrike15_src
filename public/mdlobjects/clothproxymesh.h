//========== Copyright (c) Valve Corporation. All Rights Reserved. ============
#ifndef MDLOBJECTS_CLOTHPROXYMESH_HDR
#define MDLOBJECTS_CLOTHPROXYMESH_HDR



class CVClothProxyMesh
{
public:
	CVClothProxyMesh()
	{
		m_meshName = "";
		m_meshFile = "";
		m_bMapUsingUVs = false;
		m_bBackSolveJoints = false;
		m_flBackSolveInfluenceThreshold = 0.05f;
		m_flEnvelope = 1000;
		m_flCollapseTinyEdges = 0.001f;
		m_bFlexClothBorders = false;
		//m_bEnableVolumetricSolve = false;
	}

	bool IsSharedMeshProcessing() const
	{
		return m_bMapUsingUVs == false
			&& m_bBackSolveJoints == false
			&& fabsf( m_flBackSolveInfluenceThreshold - 0.05f ) < 0.00001f
			&& fabsf( m_flEnvelope - 1000 ) < 0.01f
			&& fabsf( m_flCollapseTinyEdges - 0.001f ) < 0.000001f
			&& m_bFlexClothBorders == false
			//&& m_bEnableVolumetricSolve == false
			;
	}

	~CVClothProxyMesh()
	{

	}

	bool operator==( const CVClothProxyMesh &mesh ) const
	{
		if ( m_meshName != mesh.m_meshName )
			return false;
		if ( m_meshFile != mesh.m_meshFile )
			return false;
		return true;
	}

	CUtlString	m_meshName;		
	CUtlString	m_meshFile;		 // ProcessFilepath
	bool m_bMapUsingUVs; 
	// Note: m_bBackSolveJoints is implicitly assumed to be true when m_bDriveMeshesWithBacksolvedJointsOnly is true
	bool m_bBackSolveJoints; 
	//bool m_bEnableVolumetricSolve;
	bool m_bFlexClothBorders; 
	float32 m_flBackSolveInfluenceThreshold; 
	float32 m_flEnvelope;  
	float32 m_flCollapseTinyEdges; 
};


class CVClothProxyMeshOptions
{
public:

	CVClothProxyMeshOptions()
	{
		m_flClothEnableThreshold = 0.05f;
		m_bCreateStaticBone = false;
		m_nMaxBonesPerVertex = 4;
		m_bRemoveUnusedBonesEnabled = false;
		m_flMatchProxiesToMeshes = 1.0f;
		m_bDriveMeshesWithBacksolvedJointsOnly = true;

		m_flReservedFloat = 0;
		m_nReservedInt = 0;
		m_bReservedBool1 = false;
		m_bReservedBool2 = false;
		m_bReservedBool3 = false;
		m_bReservedBool4 = false;
	}

	bool IsDefault()const
	{
		return fabsf( m_flClothEnableThreshold - 0.05f ) < 0.0001f
			&& m_bCreateStaticBone != false
			&& m_nMaxBonesPerVertex != 4
			&& m_bRemoveUnusedBonesEnabled == false
			&& fabsf( m_flMatchProxiesToMeshes - 1.0f ) < 0.0001f
			&& m_bDriveMeshesWithBacksolvedJointsOnly == true;
	}

	float m_flClothEnableThreshold;
	bool m_bCreateStaticBone;
	int m_nMaxBonesPerVertex;	
	bool m_bRemoveUnusedBonesEnabled; 
	// this will ignore cloth attributes in meshes if we back-solve, so that back-solved joints may drive meshes
	bool m_bDriveMeshesWithBacksolvedJointsOnly; 
	float m_flMatchProxiesToMeshes; 

	float m_flReservedFloat; 
	int m_nReservedInt; 
	bool m_bReservedBool1; 
	bool m_bReservedBool2; 
	bool m_bReservedBool3; 
	bool m_bReservedBool4; 

};


class CVClothProxyMeshList : public CVClothProxyMeshOptions
{
public:

	CVClothProxyMeshList()
	{
	}

	virtual ~CVClothProxyMeshList() {}
	CUtlVector< CVClothProxyMesh > m_clothProxyMeshList; 

	bool IgnoreClothInMeshes() const
	{
		// Note: m_bBackSolveJoints is implicitly assumed to be true when m_bDriveMeshesWithBacksolvedJointsOnly is true
		return m_bDriveMeshesWithBacksolvedJointsOnly;
	}

	bool IsSharedMeshProcessing()const
	{
		if ( m_clothProxyMeshList.IsEmpty() )
			return true;
		if ( !IsDefault() )
			return false;
		for ( const CVClothProxyMesh &proxy : m_clothProxyMeshList )
		{
			if ( !proxy.IsSharedMeshProcessing() )
			{
				return false;
			}
		}
		return true;
	}
};

#endif // MDLOBJECTS_CLOTHPROXYMESH_HDR
