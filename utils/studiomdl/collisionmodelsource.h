//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
                  
#ifndef COLLISION_MODEL_OPTIONS_H
#define COLLISION_MODEL_OPTIONS_H

#include "tier1/utlvector.h"

#define MAX_EXTRA_COLLISION_MODELS 24

struct extramodel_t
{
	struct s_source_t *m_pSrc;
	matrix3x4_t m_matOffset;
	bool m_bConcave;
};

class CCollisionModelSource
{
public:
	struct s_source_t		*m_pModel;
	extramodel_t			m_ExtraModels[MAX_EXTRA_COLLISION_MODELS+1];
	bool					m_isJointed;
	bool					m_bAssumeWorldspace; // assume the model is already declared in worldspace, regardless of bone names
	bool					m_allowConcave;
	int						m_maxConvex;
	char *					m_pOverrideName;
	CUtlVector<int>			m_bonemap/*[MAXSTUDIOSRCBONES]*/;
	char					m_rootName[128];
	bool					m_allowConcaveJoints;
	bool					m_bRootCollisionIsEmpty;
public:
	void ConvertToWorldSpace(CUtlVector<Vector> &worldVerts, s_source_t *pmodel);
	void ConvertToBoneSpace( int boneIndex, CUtlVector<Vector> &boneVerts );
	bool ShouldProcessBone( int boneIndex );
	void Simplify();
	void SkipBone( int boneIndex );
	void InitBoneMap( void );
	void MergeBones( int parent, int child );
	void MergeBones(const char *parent, const char *child);
	int FindLocalBoneNamed( const char *pName );
	bool FaceHasVertOnBone( const struct s_face_t &face, int boneIndex );
	s_face_t GetGlobalFace( struct s_mesh_t *pMesh, int nFace );

	void FindBoundBones(struct s_mesh_t *pMesh, CUtlVector<int>&setBones);
	void FindBoundBones(struct s_boneweight_t &weights, CUtlVector<int>&setBones);
	int	RemapBone( int boneIndex ) const;
};


// list of vertex indices that form a convex element
struct convexlist_t
{
	int	firstVertIndex;
	int numVertIndex;
};

// Purpose: Fixup the pointers in this face to reference the mesh globally (source relative)
//			(faces are mesh relative, each source has several meshes)
extern void GlobalFace( s_face_t *pout, s_mesh_t *pmesh, s_face_t *pin );

#endif