//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Builds physics collision models from studio model source
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

// NOTE: The term joint here is used to mean a bone, collision model, and a joint.
// Each "joint" is the collision geometry at a named bone (or set of bones that have been merged)
// and the joint (with constraints) between that set and its parent.  The root "joint" has
// no constraints.
// I chose to refer to them as joints to avoid confusion.  Yes they encompass bones and joints,
// but they use the same names, and the data is actually linked.

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>

#include "vphysics/constraints.h"
#include "collisionmodelsource.h"
#include "collisionmodel.h"
//#include "physics2collision.h"
#include "cmdlib.h"
#include "scriplib.h"
#include "mathlib/mathlib.h"
#include "studio.h"
#include "studiomdl.h"
#include "physdll.h"
#include "phyfile.h"
#include "utlvector.h"
#include "vcollide_parse.h"
#include "tier1/strtools.h"
#include "tier2/tier2.h"
#include "keyvalues.h"
#include "tier1/smartptr.h"
#include "tier2/p4helpers.h"
#include "datamodel/dmattributevar.h"
#include "datamodel/dmelement.h"

#ifdef MDLCOMPILE
#include "mdlobjects/dmecollisionjoints.h"
#endif // #ifdef MDLCOMPILE
//#include "vphysics2_interface.h"

// Finds the bone index for a particular source
extern int FindLocalBoneNamed( const s_source_t *pSource, const char *pName );

// these functions just wrap atoi/atof and check for NULL
static float Safe_atof( const char *pString );
static int Safe_atoi( const char *pString );

IPhysicsCollision *physcollision = NULL;
IPhysicsSurfaceProps *physprops = NULL;

float g_WeldVertEpsilon = 0.0f;
float g_WeldNormalEpsilon = 0.999f;
bool g_ConvexHullCountOverride = false;

//-----------------------------------------------------------------------------
// Purpose: Contains a single convex element of a physical collision system
//-----------------------------------------------------------------------------
class CPhysCollisionModel
{
public:
	CPhysCollisionModel( void )
	{
		memset( this, 0, sizeof(*this) );
	}

	const char	*m_parent;
	const char	*m_name;

	// physical properties stored on disk
	float		m_mass;
	float		m_volume;
	float		m_surfaceArea;
	float		m_damping;
	float		m_rotdamping;
	float		m_inertia;
	float		m_dragCoefficient;

	// these tune the model building process, they don't go in the file
	float		m_massBias;

	CPhysCollide	*m_pCollisionData;
	CPhysCollisionModel	*m_pNext;
};

enum jointlimit_t
{
	JOINT_FREE = 0,
	JOINT_FIXED = 1,
	JOINT_LIMIT = 2,
};



//-----------------------------------------------------------------------------
// Purpose: element of a list of constraints for a jointed model
//-----------------------------------------------------------------------------
class CJointConstraint
{
public:
	CJointConstraint( void )
	{
		m_pJointName = NULL;
	}

	CJointConstraint( const char *pName, int axis, jointlimit_t type, float min, float max, float friction )
		: m_axis(axis), m_jointType(type), m_limitMin(min), m_limitMax(max), m_friction(friction)
	{
		m_pJointName = pName;
	}

	const char		*m_pJointName;
	int				m_axis;
	jointlimit_t	m_jointType;
	float			m_limitMin;
	float			m_limitMax;
	float			m_friction;

	CJointConstraint *m_pNext;
};

struct mergelist_t
{
	char		*pParent;
	char		*pChild;
};

struct collisionpair_t
{
	int obj0;
	int obj1;
	const char *pName0;
	const char *pName1;
	collisionpair_t *pNext;
};



// Returns the index to pName in g_bonetable
int FindBoneInTable( const char *pName )
{
	return findGlobalBone( pName );
}


//-----------------------------------------------------------------------------
// Purpose: Contains a complete physical joint system with constraint relationships
//-----------------------------------------------------------------------------
// This class is really just a namespace for a set of globals...
class CJointedModel: public CCollisionModelSource
{
public:
	int						m_collisionCount;
	CPhysCollisionModel		*m_pCollisionList;
	collisionpair_t			*m_pCollisionPairs;
	float					m_totalMass;
	CJointConstraint		*m_pConstraintList;
	int						m_constraintCount;
	int						m_totalVerts;
	bool					m_isMassCenterForced;
	bool					m_noSelfCollisions;
	bool					m_remove2d;
	Vector					m_massCenterForced;

	float					m_defaultDamping;
	float					m_defaultRotdamping;
	float					m_defaultInertia;
	float					m_defaultDrag;
	CUtlVector<char>		m_textCommands;
	CUtlVector<mergelist_t> m_mergeList;

	CJointedModel( void );

	void SetSource( s_source_t *pmodel );
	void SetOverrideName( const char *pName )
	{
		if ( m_pOverrideName )
		{
			delete[] m_pOverrideName;
		}
		if ( pName )
		{
			int len = V_strlen(pName);
			if ( len )
			{
				len++;
				m_pOverrideName = new char[len];
				V_strncpy( m_pOverrideName, pName, len );
			}
		}
	}

	void AddMergeCommand( char const *pParent, char const *pChild );
	int BoneIndex( const char *pName );
	void AppendCollisionModel( CPhysCollisionModel *pCollide );
	void UnlinkCollisionModel( CPhysCollisionModel *pCollide );
	CPhysCollisionModel *GetCollisionModel( const char *pName );
	void AppendCollisionPair( const char *pName0, const char *pName1 );
	void RemoveCollisionPair( const char *pName0, const char *pName1 );
	void AddConstraint( const char *pJointName, int axis, jointlimit_t jointType, float limitMin, float limitMax, float friction );
	int CollisionIndex( const char *pName );
	void SortCollisionList( void );
	void ForceMassCenter( const Vector &centerOfMass );
	void AllowConcave( void ) { m_allowConcave = true; }
	void AllowConcaveJoints() { m_allowConcaveJoints = true; }
	void Remove2DConvex() { m_remove2d = true; }
	void SetMaxConvex( int newMax ) { m_maxConvex = newMax; }
	void DefaultDamping( float damping );
	void DefaultRotdamping( float rotdamping );
	void DefaultInertia( float inertia );
	void DefaultDrag( float drag );
	void SetTotalMass( float mass );
	void SetAutoMass( void );
	void SetNoSelfCollisions();
	void SetCollisionModelDefaults( CPhysCollisionModel *pModel );
	CPhysCollisionModel *InitCollisionModel( const char *pJointName );

	void JointDamping( const char *pJointName, float damping );
	void JointRotdamping( const char *pJointName, float rotdamping );
	void JointInertia( const char *pJointName, float inertia );
	void JointMassBias( const char *pJointName, float massBias );
	void FixBoneList();
	const char *FixParent( const char *pParentName );
	void FixCollisionHierarchy( );
	int ProcessSingleBody();
	int ProcessJointedModel();
	
	int CopyFaceVertsByBone( Vector **verts, Vector *worldVerts, int boneIndex );

	void AddConvexSrc( const char *szFileName );

	void AddText( const char *pText )
	{
		int len = strlen(pText);
		int count = m_textCommands.Count();
		m_textCommands.AddMultipleToTail( len );
		memcpy( m_textCommands.Base() + count, pText, len );
	}
	void ComputeMass( void );

	float					m_flFrictionTimeIn;
	float					m_flFrictionTimeOut;
	float					m_flFrictionTimeHold;
	int						m_iMinAnimatedFriction;
	int						m_iMaxAnimatedFriction;
	bool					m_bHasAnimatedFriction;
};


CJointedModel g_JointedModel;

CJointedModel::CJointedModel( void )
{
	m_pModel = NULL;
	
	for ( int i=0; i<=MAX_EXTRA_COLLISION_MODELS; i++ )
	{
		m_ExtraModels[i].m_pSrc = NULL;
		m_ExtraModels[i].m_bConcave = false;
	}

	m_bRootCollisionIsEmpty = false;

	m_collisionCount = 0;
	m_pCollisionList = NULL;
	m_pCollisionPairs = NULL;
	m_totalMass = 1.0;

	m_bonemap.SetSize(0);
	m_pConstraintList = NULL;
	m_constraintCount = 0;
	
	m_totalVerts = 0;

	// UNDONE: Move these defaults elsewhere?  They are all overrideable by the QC/script
	// These defaults are also in the CDmeCollisionModel/CDmeCollisionJoints
	m_defaultDamping = 0;
	m_defaultRotdamping = 0;
	m_defaultInertia = 1.0;
	m_defaultDrag = -1;
	m_allowConcave = false;
	m_allowConcaveJoints = false;
	m_remove2d = false;
	m_maxConvex = 40;
	m_isMassCenterForced = false;
	m_noSelfCollisions = false;
	m_massCenterForced.Init();

	m_flFrictionTimeIn = 0.0f;
	m_flFrictionTimeOut = 0.0f;
	m_iMinAnimatedFriction = 1.0f;
	m_iMaxAnimatedFriction = 1.0f;
	m_bHasAnimatedFriction = false;
	m_pOverrideName = NULL;
}



void CJointedModel::SetSource( s_source_t *pmodel )
{
	m_pModel = pmodel;
	InitBoneMap();
	m_totalVerts = pmodel->numvertices;
}



void CJointedModel::AddMergeCommand( char const *pParent, char const *pChild )
{
	int i = m_mergeList.AddToTail();
	m_mergeList[i].pParent = strdup(pParent);
	m_mergeList[i].pChild = strdup(pChild);
}



int CJointedModel::BoneIndex( const char *pName )
{
	pName = RenameBone( pName );
	for ( int boneIndex = 0; boneIndex < m_pModel->numbones; boneIndex++ )
	{
		if ( !stricmp( m_pModel->localBone[boneIndex].name, pName ) )
			return boneIndex;
	}

	return -1;
}


void CJointedModel::AppendCollisionModel( CPhysCollisionModel *pCollide )
{
	if ( m_isMassCenterForced )
	{
		physcollision->CollideSetMassCenter( pCollide->m_pCollisionData, m_massCenterForced );
	}

	pCollide->m_pNext = m_pCollisionList;
	m_pCollisionList = pCollide;
	m_collisionCount++;
}


void CJointedModel::UnlinkCollisionModel( CPhysCollisionModel *pCollide )
{
	CPhysCollisionModel **pList = &m_pCollisionList;

	if ( !pCollide )
		return;

	while ( *pList )
	{
		CPhysCollisionModel *pNode = *pList;
		if ( pNode == pCollide )
		{
			*pList = pCollide->m_pNext;
			m_collisionCount--;
			pCollide->m_pNext = NULL;
			return;
		}
		pList = &pNode->m_pNext;
	}
}

int CJointedModel::CollisionIndex( const char *pName )
{
	if ( !pName )
		return -1;

	CPhysCollisionModel *pList = m_pCollisionList;
	int index = 0;
	while ( pList )
	{
		if ( !stricmp( pName, pList->m_name ) )
			return index;
		
		pList = pList->m_pNext;
		index++;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Sort the list so that parents come before their children
//-----------------------------------------------------------------------------
void CJointedModel::SortCollisionList( void )
{
	if ( !m_collisionCount )
		return;

	CPhysCollisionModel **pArray;
	pArray = new CPhysCollisionModel *[m_collisionCount];
	CPhysCollisionModel *pList = m_pCollisionList;
	
	// make an array to make sorting easier
	int i = 0;

	while ( pList )
	{
		pArray[i++] = pList;
		pList = pList->m_pNext;
	}

	// really stupid bubble sort!
	// this is really inefficient but it was easy to code and there are never
	// more than maxConvex elements.
	bool swapped = true;

	while ( swapped )
	{
		swapped = false;
		// loop over all solids and swap any parent/child pairs that are out of order
		for ( i = 0; i < m_collisionCount; i++ )
		{
			CPhysCollisionModel *pPhys = pArray[i];
			if ( !pPhys->m_parent )
				continue;

			// Don't try to move ones where the pPhys and its parent have the same name
			// otherwise an infinite loop results
			if ( !Q_stricmp( pPhys->m_name, pPhys->m_parent ) )
				continue;

			// find the parent
			int j;
			for ( j = 0; j < m_collisionCount; j++ )
			{
				if ( j == i )
					continue;

				if ( !stricmp( pPhys->m_parent, pArray[j]->m_name ) )
					break;
			}

			// if the child came before the parent, then swap the parent and child positions
			if ( j > i && j < m_collisionCount )
			{
				swapped = true;
				pArray[i] = pArray[j];
				pArray[j] = pPhys;
			}
		}
	}

	// link up the sorted list
	for ( i = 0; i < m_collisionCount-1; i++ )
	{
		pArray[i]->m_pNext = pArray[i+1];
	}
	// terminate
	pArray[i]->m_pNext = NULL;
	// point the list to first joint
	m_pCollisionList = pArray[0];

	// delete the working array
	delete[] pArray;
}

void CJointedModel::AddConvexSrc( const char *szFileName )
{
	s_source_t *pmodel;

	for ( int i=0; i<MAX_EXTRA_COLLISION_MODELS; i++ )
	{
		if ( m_ExtraModels[i].m_pSrc == NULL )
		{

			int nummaterials = g_nummaterials;
			int numtextures = g_numtextures;

			pmodel = Load_Source( szFileName, "SMD", false, false, false );
			if ( !pmodel )
				return;

			// auto-remove any new materials/textures
			if (nummaterials && numtextures && (numtextures != g_numtextures || nummaterials != g_nummaterials))
			{
				g_numtextures = numtextures;
				g_nummaterials = nummaterials;

				pmodel->texmap[0] = 0;
			}

			m_ExtraModels[i].m_pSrc = pmodel;
			m_ExtraModels[i].m_matOffset.SetToIdentity();

			if ( TokenAvailable() )
			{
				GetToken(false);

				if ( !V_strncmp( token, "offset", 6 ) )
				{

					Vector vecOffsetPosition; vecOffsetPosition.Init();
					QAngle angOffsetAngle; angOffsetAngle.Init();
					float flScale = 1;

					int nCount = sscanf( token, "offset pos[ %f %f %f ] angle[ %f %f %f ] scale[ %f ]",
						&vecOffsetPosition.x, &vecOffsetPosition.y, &vecOffsetPosition.z,
						&angOffsetAngle.x, &angOffsetAngle.y, &angOffsetAngle.z,
						&flScale );
					
					if ( nCount == 7 )
					{
						// physics model SMDs are in a different space, so this hacky conversion happens 
						// to their offset matrix so the matrices fed to the src combiner are always the same.
						// see: https://intranet.valvesoftware.com/wiki/3D_Coordinate_Systems

						matrix3x4_t matLocal; AngleMatrix( angOffsetAngle, vecOffsetPosition, matLocal );

						matLocal.ScaleUpper3x3Matrix( flScale * (1.0f / g_currentscale) );

						matrix3x4_t matConvert;	matConvert.InitXYZ( Vector(0,1,0), Vector(-1,0,0), Vector(0,0,1), Vector(0,0,0) );
						ConcatTransforms( matLocal, matConvert.InverseTR(), matLocal );
						matrix3x4_t matRotate; matRotate.InitFromQAngles( QAngle(0,90,0) );
						ConcatTransforms( matRotate, matLocal, matLocal );
						MatrixCopy( matLocal, m_ExtraModels[i].m_matOffset );

					}
					else
					{
						MdlError( "Malformed offset parameters to $addconvexsrc." );
						return;
					}

				}
				else
				{
					UnGetToken();
				}
			}

			if ( TokenAvailable() )
			{
				GetToken(false);
				if ( !V_strncmp( token, "concave", 7 ) )
				{
					m_ExtraModels[i].m_bConcave = true;
				}
				else
				{
					UnGetToken();
				}
			}

			return;
		}
	}

	MdlWarning( "Cannot add more than %i extra collision models. Ignoring $addconvexsrc \"%s\".\n", MAX_EXTRA_COLLISION_MODELS, szFileName );
}

void CJointedModel::AppendCollisionPair( const char *pName0, const char *pName1 )
{
	collisionpair_t *pPair = new collisionpair_t;
	pPair->obj0 = -1;
	pPair->obj1 = -1;
	int jointIndex0 = FindLocalBoneNamed( pName0 );
	pPair->pName0 = (jointIndex0 >= 0) ? m_pModel->localBone[jointIndex0].name : NULL;
	int jointIndex1 = FindLocalBoneNamed( pName1 );
	pPair->pName1 = (jointIndex1 >= 0) ? m_pModel->localBone[jointIndex1].name : NULL;

	//printf("Appending collision pair: %s to %s\n", pPair->pName0, pPair->pName1 );

	pPair->pNext = m_pCollisionPairs;
	m_pCollisionPairs = pPair;
}

void CJointedModel::RemoveCollisionPair( const char *pName0, const char *pName1 )
{
	int jointIndex0 = FindLocalBoneNamed( pName0 );
	const char *szName0 = m_pModel->localBone[jointIndex0].name;
	int jointIndex1 = FindLocalBoneNamed( pName1 );
	const char *szName1 = m_pModel->localBone[jointIndex1].name;
	
	collisionpair_t *pPairToRemove = NULL;

	// find the pair to remove
	collisionpair_t *pPair = m_pCollisionPairs;
	while ( pPair )
	{
		if ( !strcmp( pPair->pName0, szName0 ) && !strcmp( pPair->pName1, szName1 ) )
		{
			pPairToRemove = pPair;
			break;
		}
		pPair = pPair->pNext;
	}

	if ( pPairToRemove )
	{
		// find the prev
		collisionpair_t *pPairPrev = NULL;

		pPair = m_pCollisionPairs;
		while ( pPair )
		{
			if ( pPair->pNext == pPairToRemove )
			{
				pPairPrev = pPair;
				break;
			}
			pPair = pPair->pNext;
		}

		if ( pPairPrev )
		{
			pPairPrev->pNext = pPairToRemove->pNext;
		}
		else
		{
			// the pair we're removing is at the front
			m_pCollisionPairs = pPairToRemove->pNext;
		}

		//printf("Removing collision pair: %s to %s\n", szName0, szName1 );
	}
	else
	{
		//MdlWarning( "No such collision pair exists: [%s] to [%s]\n", pName0, pName1 );
	}
}

void CJointedModel::ForceMassCenter( const Vector &centerOfMass )
{
	m_isMassCenterForced = true;
	m_massCenterForced = centerOfMass;
}



CPhysCollisionModel *CJointedModel::GetCollisionModel( const char *pName )
{
	if ( !pName )
		return NULL;

	CPhysCollisionModel *pList = m_pCollisionList;
	while ( pList )
	{
		if ( !stricmp( pName, pList->m_name ) )
			return pList;
		
		pList = pList->m_pNext;
	}

	return NULL;
}

void CJointedModel::AddConstraint( const char *pJointName, int axis, jointlimit_t jointType, float limitMin, float limitMax, float friction )
{
	// In the editor/qc friction values are shown as 5X so 1.0 can be the default.
	CJointConstraint *pConstraint = new CJointConstraint( pJointName, axis, jointType, limitMin, limitMax, friction * (1.0f/5.0f) );

	// link it in
	pConstraint->m_pNext = m_pConstraintList;
	m_pConstraintList = pConstraint;
	m_constraintCount++;
}

void CJointedModel::DefaultDamping( float damping )
{
	m_defaultDamping = damping;
}

void CJointedModel::DefaultRotdamping( float rotdamping )
{
	m_defaultRotdamping = rotdamping;
}

void CJointedModel::DefaultInertia( float inertia )
{
	m_defaultInertia = inertia;
}

void CJointedModel::SetTotalMass( float mass )
{
	m_totalMass = mass;
}

void CJointedModel::SetAutoMass( void )
{
	m_totalMass = -1;
}

void CJointedModel::SetNoSelfCollisions()
{
	m_noSelfCollisions = true;
}

void CJointedModel::SetCollisionModelDefaults( CPhysCollisionModel *pModel )
{
	pModel->m_damping = m_defaultDamping;
	pModel->m_inertia = m_defaultInertia;
	pModel->m_rotdamping = m_defaultRotdamping;
	pModel->m_massBias = 1.0;
	
	// not written unless modified
	pModel->m_dragCoefficient = m_defaultDrag;
}



void CJointedModel::ComputeMass( void )
{
	// already set
	if ( m_totalMass >= 0 )
		return;

	CPhysCollisionModel *pList = m_pCollisionList;
	m_totalMass = 0;

	while ( pList )
	{
		char* pSurfaceProps = GetSurfaceProp( pList->m_name );
		int index = physprops->GetSurfaceIndex( pSurfaceProps );
		float density, thickness;
		physprops->GetPhysicsProperties( index, &density, &thickness, NULL, NULL );

		if ( thickness > 0 )
		{
			m_totalMass += pList->m_surfaceArea * thickness * CUBIC_METERS_PER_CUBIC_INCH * density;
		}
		else
		{
			// density is in kg/m^3, volume is in in^3
			m_totalMass += pList->m_volume * CUBIC_METERS_PER_CUBIC_INCH * density;
		}
		pList = pList->m_pNext;
	}

	if( !g_quiet )
	{
		printf("Computed Mass: %.2f kg\n", m_totalMass );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates a collision object using the defaults in joints
// Input  : &joints - joint system to create the model in
//			*pJointName - name to give this model
// Output : static CPhysCollisionModel
//-----------------------------------------------------------------------------
CPhysCollisionModel *CJointedModel::InitCollisionModel( const char *pJointName )
{
	CPhysCollisionModel *pModel = GetCollisionModel( pJointName );
	if ( !pModel )
	{
		int boneIndex = BoneIndex( pJointName );
		if ( boneIndex < 0 )
			return NULL;

		pModel = new CPhysCollisionModel;
		// this name is the same as pJointName, but guaranteed to be non-volatile (we'd have to copy pJointName)
		pModel->m_name = m_pModel->localBone[boneIndex].name;
		if ( m_pModel->localBone[boneIndex].parent >= 0 )
		{
			pModel->m_parent = m_pModel->localBone[m_pModel->localBone[boneIndex].parent].name;
		}
		else
		{
			pModel->m_parent = NULL;
		}

		SetCollisionModelDefaults( pModel );
		AppendCollisionModel( pModel );
	}

	return pModel;
}

void CJointedModel::JointDamping( const char *pJointName, float damping )
{
	CPhysCollisionModel *pModel = InitCollisionModel( pJointName );
	if ( pModel )
	{
		pModel->m_damping = damping;
	}
}

void CJointedModel::JointRotdamping( const char *pJointName, float rotdamping )
{
	CPhysCollisionModel *pModel = InitCollisionModel( pJointName );
	if ( pModel )
	{
		pModel->m_rotdamping = rotdamping;
	}
}

void CJointedModel::JointMassBias( const char *pJointName, float massBias )
{
	CPhysCollisionModel *pModel = InitCollisionModel( pJointName );
	if ( pModel )
	{
		pModel->m_massBias = massBias;
	}
}

void CJointedModel::JointInertia( const char *pJointName, float inertia )
{
	CPhysCollisionModel *pModel = InitCollisionModel( pJointName );
	if ( pModel )
	{
		pModel->m_inertia = inertia;
	}
}


void CJointedModel::DefaultDrag( float drag )
{
	m_defaultDrag = drag;
}









//-----------------------------------------------------------------------------
// Purpose: Copy all verts assigned to this bone.
//			NOTE: Leaves gaps in the model around joints
// Input  : **verts - 
//			*worldVerts - 
//			&joints - 
//			boneIndex - 
// Output : int vertCount
//-----------------------------------------------------------------------------
int CopyVertsByBone( Vector **verts, Vector *worldVerts, const CJointedModel &joints, int boneIndex )
{
	int vertCount = 0;
	s_source_t *pmodel = joints.m_pModel;

	// loop through each vert to find those assigned to this bone
	for ( int i = 0; i < pmodel->numvertices; i++ )
	{
		s_boneweight_t *pweight = &pmodel->vertex[ i ].boneweight;

		// look at each assignment for this vert
		for ( int j = 0; j < pweight->numbones; j++ )
		{
			// Discover the local bone index for this bone
			int localBone = pweight->bone[j];

			// assigned to boneIndex?
			if ( joints.RemapBone( localBone ) == boneIndex )
			{
				// add this vert to model
				verts[vertCount++] = &worldVerts[i];
			}
		}
	}

	return vertCount;
}


//-----------------------------------------------------------------------------
// Purpose: Copy all verts that are referenced by a face which has a vert assigned
//			to this bone.
//			NOTE: convex hulls of each bone will overlap at the joints
// Input  : **verts - 
//			*worldVerts - 
//			&joints - 
//			boneIndex - 
// Output : int
//-----------------------------------------------------------------------------
int CJointedModel::CopyFaceVertsByBone( Vector **verts, Vector *worldVerts, int boneIndex )
{
	int vertCount = 0;

	int *vertChecked = new int[m_pModel->numvertices];
	for ( int b = 0; b < m_pModel->numvertices; b++ )
	{
		vertChecked[b] = 0;
	}

	for ( int i = 0; i < m_pModel->nummeshes; i++ )
	{
		s_mesh_t *pmesh = m_pModel->mesh + m_pModel->meshindex[i];
		for ( int j = 0; j < pmesh->numfaces; j++ )
		{
			s_face_t *face = m_pModel->face + pmesh->faceoffset + j;
			s_face_t globalFace;
			GlobalFace( &globalFace, pmesh, face );
			if ( FaceHasVertOnBone( globalFace, boneIndex ) )
			{
				if ( !vertChecked[globalFace.a] )
				{
					// add this vert to model
					verts[vertCount++] = &worldVerts[globalFace.a];
				}
				if ( !vertChecked[globalFace.b] )
				{
					// add this vert to model
					verts[vertCount++] = &worldVerts[globalFace.b];
				}
				if ( !vertChecked[globalFace.c] )
				{
					// add this vert to model
					verts[vertCount++] = &worldVerts[globalFace.c];
				}
				// mark these verts so you only add them once
				vertChecked[globalFace.a] = 1;
				vertChecked[globalFace.b] = 1;
				vertChecked[globalFace.c] = 1;
			}
		}
	}

	delete[] vertChecked;
	return vertCount;
}



//-----------------------------------------------------------------------------
// Purpose: Find all verts that differ only by texture coordinates - this allows
//			us to ignore texture coordinates on collision models
// Input  : *weldTable - output table
//			*pmodel - input model
//-----------------------------------------------------------------------------
void BuildVertWeldTable( int *weldTable, s_source_t *pmodel )
{
	for ( int i = 0; i < pmodel->numvertices; i++ )
	{
		bool found = false;
		for ( int j = 0; j < i; j++ )
		{
			float dist = (pmodel->vertex[j].position - pmodel->vertex[i].position).Length();
			float normalDist = DotProduct( pmodel->vertex[j].normal, pmodel->vertex[i].normal );
			if ( dist <= g_WeldVertEpsilon && normalDist > g_WeldNormalEpsilon )
			{
				found = true;
				weldTable[i] = j;
				break;
			}
		}

		if ( !found )
		{
			weldTable[i] = i;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: marks all verts with a unique ID.  Each set of connected verts has
//			the same ID.  IDs are the index of the lowest numbered face on the 
//			mesh
// Input  : *vertID - array that holds IDs
//			*pmodel - model to process
//-----------------------------------------------------------------------------
void MarkConnectedMeshes( int *vertID, s_source_t *pmodel, int *vertMap )
{
	int i;

	// mark all verts as max faceid + 1
	for ( i = 0; i < pmodel->numvertices; i++ )
	{
		// If these verts have been welded to a lower-index vert, mark them
		// as already processed to avoid making additional convex objects out of them.
		if ( vertMap[i] != i )
		{
			vertID[i] = -1;
		}
		else
		{
			vertID[i] = pmodel->numfaces+1;
		}
	}

	int marked = 0;
	int faceid = 0;
	// iterate the face list, minimizing the vertID at each vert
	// until we have an iteration where no vertIDs are changed
	do 
	{
		marked = 0;
		faceid = 0;

		for ( i = 0; i < pmodel->nummeshes; i++ )
		{
			s_mesh_t *pmesh = pmodel->mesh + pmodel->meshindex[i];
			for ( int j = 0; j < pmesh->numfaces; j++ )
			{
				s_face_t *face = pmodel->face + pmesh->faceoffset + j;
				s_face_t globalFace;
				GlobalFace( &globalFace, pmesh, face );
				// account for welding
				globalFace.a = vertMap[globalFace.a];
				globalFace.b = vertMap[globalFace.b];
				globalFace.c = vertMap[globalFace.c];


				// find min(faceid, vertID[a], vertID[b], vertID[c]);
				int newid = MIN(faceid, vertID[globalFace.a]);
				newid = MIN( newid, vertID[globalFace.b]);
				newid = MIN( newid, vertID[globalFace.c]);
				
				// mark all verts with the minimum, count the number we had to mark
				if ( vertID[globalFace.a] != newid )
				{
					vertID[globalFace.a] = newid;
					marked++;
				}
				if ( vertID[globalFace.b] != newid )
				{
					vertID[globalFace.b] = newid;
					marked++;
				}
				if ( vertID[globalFace.c] != newid )
				{
					vertID[globalFace.c] = newid;
					marked++;
				}
				faceid++;
			}
		}
	} while ( marked != 0 );
}



//-----------------------------------------------------------------------------
// Purpose: Finds a CPhysCollisionModel in a linked list of models.
// Input  : *pHead - 
//			*pName - 
// Output : CPhysCollisionModel
//-----------------------------------------------------------------------------
CPhysCollisionModel *FindObjectInList( CPhysCollisionModel *pHead, const char *pName )
{
	while ( pHead )
	{
		if ( !stricmp( pName, pHead->m_name ) )
			break;
		pHead = pHead->m_pNext;
	}

	return pHead;
}


//-----------------------------------------------------------------------------
// Purpose: Fix all bones to reference the remapped/collapsed bone structure
void CJointedModel::FixBoneList()
{
	if ( !m_isJointed )
		return;

	CPhysCollisionModel *pmodel = m_pCollisionList;
	while ( pmodel )
	{
		int nodeIndex = FindLocalBoneNamed( pmodel->m_name );
		if ( nodeIndex < 0 )
		{
			MdlWarning("Physics for unknown bone %s\n", pmodel->m_name );
		}
		else
		{
			int count = 0;
			// remove simplified bones
			while ( m_pModel->boneLocalToGlobal[nodeIndex] < 0 )
			{
				if ( count++ > MAXSTUDIOSRCBONES )
					break;

				// simplified out, move up to the parent
				nodeIndex = m_pModel->localBone[nodeIndex].parent;
			}

			if ( nodeIndex >= 0 )
			{
				// bone collapse may have changed parent hierarchy, and the root name. 
				// The vertices are converted to the new reference by ConvertToWorldSpace(), as well as RemapVerticesToGlobalBones()
				pmodel->m_name = g_bonetable[  m_pModel->boneLocalToGlobal[nodeIndex] ].name;
				pmodel->m_parent = NULL;
				int parentIndex = m_pModel->localBone[nodeIndex].parent;
				if ( parentIndex >= 0 && parentIndex != nodeIndex )
				{
					parentIndex = m_bonemap[parentIndex];
					if (m_pModel->boneLocalToGlobal[parentIndex] < 0)
					{
						pmodel->m_parent = m_pModel->localBone[parentIndex].name;
					}
					else
					{
						pmodel->m_parent = g_bonetable[  m_pModel->boneLocalToGlobal[parentIndex] ].name;
					}
				}
			}
			else
			{
				MdlWarning("Physics for unknown bone %s\n", pmodel->m_name );
			}
		}

		pmodel = pmodel->m_pNext;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Fixup all references to parents by walking up on models whose parents
//			have no collision geometry.  Bones without geometry cannot be physically
//			simulated, so they must be removed.
//			NOTE: This is broken.  It won't work for tree structures with an empty parent
//			(i.e. 2 children attached to a parent bone that has no physics geometry - thus empty)
//			It will not convert that parent into a constraint between 2 children
// Input  : *pList - 
//			*pSource - 
//			*pParentName - 
// Output : const char
//-----------------------------------------------------------------------------
const char *CJointedModel::FixParent( const char *pParentName )
{
	while ( pParentName )
	{
		if ( FindObjectInList( m_pCollisionList, pParentName ) )
		{
			return pParentName;
		}
		int nodeIndex = FindLocalBoneNamed( pParentName );
		if ( nodeIndex < 0 )
			return NULL;
		int parentIndex = m_pModel->localBone[nodeIndex].parent;
		if ( parentIndex < 0 )
		{
			break;
		}

		pParentName = m_pModel->localBone[parentIndex].name;
	}

	return NULL;
}


struct boundingvolume_t
{
	Vector	mins;
	Vector	maxs;
};


void CreateCollide( CPhysCollisionModel *pBase, CPhysConvex **pElements, int elementCount, const boundingvolume_t &bv )
{
	int i;

	if ( !pBase )
		return;

	// NOTE: Must do this before building collide
	pBase->m_volume = 0;
	pBase->m_surfaceArea = 0;
	for ( i = 0; i < elementCount; i++ )
	{
		pBase->m_volume += physcollision->ConvexVolume( pElements[i] );
		pBase->m_surfaceArea += physcollision->ConvexSurfaceArea( pElements[i] );
	}

	convertconvexparams_t params;
	params.Defaults();
	params.buildOuterConvexHull = true;
	params.buildDragAxisAreas = true;
	params.checkOptimalTracing = true;
	Vector size = bv.maxs - bv.mins;

	int largest = 0;
	float minSurfaceArea = -1.0f;
	for ( i = 0; i < 3; i++ )
	{
		if ( size[i] > size[largest] )
		{
			largest = i;
		}

		int other = (i+1)%3;
		int cross = (i+2)%3;
		float surfaceArea = size[other] * size[cross];
		if ( minSurfaceArea < 0 || surfaceArea < minSurfaceArea )
		{
			minSurfaceArea = surfaceArea;
		}
	}
	// this can be really slow with super-large models and a low error tolerance
	// Basically you get a ray cast through each square of epsilon surface area on each OBB side
	// So compute it for 0.01% error (on the smallest side, less on larger sides)
	params.dragAreaEpsilon = clamp( minSurfaceArea * 1e-4f, 0.25f, 128.0f );

	Vector tmp = size;
	tmp[largest] = 0;
	float len = tmp.Length();
	if ( len > 0 )
	{
		float sizeRatio = size[largest] / len;
		// HACKHACK: Hardcoded size ratio to induce damping
		// This prevents long skinny objects from rolling endlessly
		if ( sizeRatio > 9 )
		{
			pBase->m_rotdamping = 1.0f;
		}
	}
	// THIS DESTROYS pConvex!!
	pBase->m_pCollisionData = physcollision->ConvertConvexToCollideParams( pElements, elementCount, params );

	// debug output for the drag area calculations
#if 0
	Msg("Drag epsilon is %.3f\n", params.dragAreaEpsilon );
	Vector areas = physcollision->CollideGetOrthographicAreas( pBase->m_pCollisionData );
	Msg("Drag fractions are %.3f %.3f %.3f\n", areas.x, areas.y, areas.z );
#endif
}


// is this list of verts contained in a slab of epsilon width?  If so, it's probably
// an error of some kind - we shouldn't be authoring flat or 2d collision models
bool IsApproximatelyPlanar( Vector **verts, int vertCount, float epsilon )
{
	if ( vertCount < 4 )
		return true;

	// If we're using an un-welded model, then this may generate a degenerate normal
	// loop to search for an actual plane
	int v0 = 1, v1 = 2;
	Vector normal;
	while ( v0 < vertCount && v1 < vertCount )
	{
		Vector edge0 = *verts[v0] - *verts[0];
		Vector edge1 = *verts[v1] - *verts[0];

		normal = CrossProduct( edge0, edge1 );
		float len = VectorNormalize( normal );
		if ( len > 0.001 )
			break;
		if ( edge0.Length() < 0.001 )
		{
			// verts[0] and v0 are coincident, try new verts
			v0++;
			v1++;
		}
		else
		{
			// v0 seems fine, try a new v1 -- it's probably coincident with v0
			v1++;
		}
	}

	// form the plane and project all of the verts into it
	float minDist = DotProduct( normal, *verts[0] );
	float maxDist = minDist;

	for ( int i = 0; i < vertCount; i++ )
	{
		float d = DotProduct( *verts[i], normal );
		if ( d < minDist )
		{
			minDist = d;
		}
		else if ( d > maxDist )
		{
			maxDist = d;
		}
		// at least one vert out of the plane, we've got something 3 dimensional
		if ( fabsf(maxDist-minDist) > epsilon )
			return false;
	}
	return true;
}



void BuildConvexListByVertID( s_source_t *pmodel, CUtlVector<convexlist_t> &convexList, CUtlVector<int> &vertList, CUtlVector<int> &vertID )
{
	// loop through each island of verts and append it to the convex list
	convexlist_t current;
	for ( int i = 0; i < pmodel->numvertices; i++ )
	{
		// already processed this group
		if ( vertID[i] < 0 || vertID[i] > pmodel->numfaces )
			continue;

		current.firstVertIndex = vertList.Count();
		current.numVertIndex = 0;

		int id = vertID[i];

		for ( int j = i; j < pmodel->numvertices; j++ )
		{
			if ( vertID[j] == id )
			{
				vertList.AddToTail(j);
				current.numVertIndex++;
				// don't reuse this vert
				vertID[j] = -1;
			}
		}
		convexList.AddToTail(current);
	}
}

// build a list of vertex indices for each connected sub-piece
void BuildSingleConvexForFaceList( s_source_t *pmodel, CUtlVector<convexlist_t> &convexList, CUtlVector<int> &vertList, const CUtlVector<s_face_t> &faceList )
{
	CUtlVector<int> vertID;
	vertID.SetCount(pmodel->numvertices);
	int i;
	for ( i = 0; i < pmodel->numvertices; i++ )
	{
		vertID[i] = -1;
	}

	for ( i = 0; i < faceList.Count(); i++ )
	{
		const s_face_t &globalFace = faceList[i];
		vertID[globalFace.a] = 1;
		vertID[globalFace.b] = 1;
		vertID[globalFace.c] = 1;
	}
	BuildConvexListByVertID( pmodel, convexList, vertList, vertID );
}

void BuildConvexListForFaceList( s_source_t *pmodel, CUtlVector<convexlist_t> &convexList, CUtlVector<int> &vertList, const CUtlVector<s_face_t> &faceList )
{
	CUtlVector<int> weldTable;
	weldTable.SetCount(pmodel->numvertices);
	BuildVertWeldTable( weldTable.Base(), pmodel );

	int i;
	CUtlVector<int> vertID;
	vertID.SetCount(pmodel->numvertices);

	// mark all verts as max faceid + 1
	for ( i = 0; i < pmodel->numvertices; i++ )
	{
		// If these verts have been welded to a lower-index vert, mark them
		// as already processed to avoid making additional convex objects out of them.
		if ( weldTable[i] != i )
		{
			vertID[i] = -1;
		}
		else
		{
			vertID[i] = pmodel->numfaces+1;
		}
	}

	Assert(convexList.Count()==0);
	Assert(vertList.Count()==0);

	int marked = 0;
	int faceid = 0;
	// iterate the face list, minimizing the vertID at each vert
	// until we have an iteration where no vertIDs are changed
	do 
	{
		marked = 0;
		faceid = 0;

		// basically this flood fills ids out to the verts until each island of connected 
		// verts shares a single id (so new verts got marked)
		for ( i = 0; i < faceList.Count(); i++ )
		{
			s_face_t globalFace = faceList[i];
			// account for welding
			globalFace.a = weldTable[globalFace.a];
			globalFace.b = weldTable[globalFace.b];
			globalFace.c = weldTable[globalFace.c];


			int newid = MIN(i, vertID[globalFace.a]);
			newid = MIN( newid, vertID[globalFace.b]);
			newid = MIN( newid, vertID[globalFace.c]);

			// mark all verts with the minimum, count the number we had to mark
			if ( vertID[globalFace.a] != newid )
			{
				vertID[globalFace.a] = newid;
				marked++;
			}
			if ( vertID[globalFace.b] != newid )
			{
				vertID[globalFace.b] = newid;
				marked++;
			}
			if ( vertID[globalFace.c] != newid )
			{
				vertID[globalFace.c] = newid;
				marked++;
			}
		}
	} while ( marked != 0 );

	BuildConvexListByVertID( pmodel, convexList, vertList, vertID );
}


// take a list of convex elements (lists of vert indices into master vert list) and build CPhysConvex out of them
// return true if there are no errors detected
bool BuildConvexesForLists( CUtlVector<CPhysConvex *> &convexOut, const CUtlVector<convexlist_t> &convexList, const CUtlVector<int> &vertList, const CUtlVector<Vector> &worldspaceVerts, bool bRemove2d )
{
	bool bValid = true;
	CUtlVector<Vector *> vertsThisConvex;
	for ( int i = 0; i < convexList.Count(); i++ )
	{
		const convexlist_t &elem = convexList[i];
		vertsThisConvex.RemoveAll();
		for ( int j = 0; j < elem.numVertIndex; j++ )
		{
			// this is ok because physcollision won't modify these, but wants non-const
			Vector *pVert = const_cast<Vector *>(&worldspaceVerts[vertList[j + elem.firstVertIndex]]);
			vertsThisConvex.AddToTail( pVert );
		}

		// need at least 3 verts to build a CPhysConvex
		if ( vertsThisConvex.Count() > 2 )
		{
			const float g_epsilon_2d = 0.5f;
			// HACKHACK: A heuristic to detect models without smoothing groups set
			// UNDONE: Do a BSP to decompose arbitrary models to convex?
			if ( IsApproximatelyPlanar( vertsThisConvex.Base(), vertsThisConvex.Count(), g_epsilon_2d ) )
			{
				if ( bRemove2d )
					continue;
				MdlWarning("Model has 2-dimensional geometry (less than %.3f inches thick on any axis)!!!\n", g_epsilon_2d );
				bValid = false;
			}
			// go ahead and build it out
			CPhysConvex *pConvex = physcollision->ConvexFromVerts( vertsThisConvex.Base(), vertsThisConvex.Count() );
			if ( pConvex )
			{
				// Got something valid, attach this convex data to the root model
				physcollision->SetConvexGameData( pConvex, 0 );
				convexOut.AddToTail(pConvex);
			}
		}
	}

	return bValid;
}

//-----------------------------------------------------------------------------
// Purpose: Build a jointed collision model with constraints
// Output : int
//-----------------------------------------------------------------------------
int CJointedModel::ProcessJointedModel()
{
	if( !g_quiet )
	{
		printf("Processing jointed collision model\n" );
	}
	// loop through each bone and form a collision model
	for ( int boneIndex = 0; boneIndex < m_pModel->numbones; boneIndex++ )
	{
		if ( !ShouldProcessBone( boneIndex ) )
			continue;

		CUtlVector<Vector> bonespaceVerts;
		bonespaceVerts.SetCount(m_pModel->numvertices);
		ConvertToBoneSpace( boneIndex, bonespaceVerts );
		CUtlVector<s_face_t> faceList;
		CUtlVector<convexlist_t> convexList;
		CUtlVector<int> vertList;
		CUtlVector<CPhysConvex *> convexOut;
		bool bValid = false;

		for ( int i = 0; i < m_pModel->nummeshes; i++ )
		{
			s_mesh_t *pmesh = m_pModel->mesh + m_pModel->meshindex[i];
			for ( int j = 0; j < pmesh->numfaces; j++ )
			{
				s_face_t *face = m_pModel->face + pmesh->faceoffset + j;
				s_face_t globalFace;
				GlobalFace( &globalFace, pmesh, face );
				if ( FaceHasVertOnBone( globalFace, boneIndex ) )
				{
					faceList.AddToTail( globalFace );
				}
			}
			
			if ( m_allowConcaveJoints )
			{
				BuildConvexListForFaceList( m_pModel, convexList, vertList, faceList );
			}
			else
			{
				BuildSingleConvexForFaceList( m_pModel, convexList, vertList, faceList );
			}

			bValid = BuildConvexesForLists( convexOut, convexList, vertList, bonespaceVerts, m_remove2d );
		}

		if ( convexOut.Count() > m_maxConvex )
		{
			MdlWarning("COSTLY COLLISION MODEL!!!! (%d parts - %d allowed)\n", convexOut.Count(), m_maxConvex );
			bValid = false;
		}

		if ( !bValid && convexOut.Count() )
		{
			MdlWarning("Error with convex elements of %s, building single convex!!!!\n", m_pModel->filename );
			for ( int i = 0; i < convexOut.Count(); i++ )
			{
				physcollision->ConvexFree( convexOut[i] );
			}
			convexOut.Purge();
		}

		if ( convexOut.Count() )
		{
			int i;

			CPhysCollisionModel *pPhys = InitCollisionModel( m_pModel->localBone[boneIndex].name );

			pPhys->m_mass = 1.0;
			pPhys->m_name = m_pModel->localBone[boneIndex].name;
			if ( m_pModel->localBone[boneIndex].parent >= 0 )
			{
				pPhys->m_parent = m_pModel->localBone[m_pModel->localBone[boneIndex].parent].name;
			}
			else
			{
				pPhys->m_parent = NULL;
			}

			boundingvolume_t bv;
			ClearBounds( bv.mins, bv.maxs );
			int vertCount = 0;
			for ( i = 0; i < convexList.Count(); i++ )
			{
				const convexlist_t &elem = convexList[i];
				for ( int j = 0; j < elem.numVertIndex; j++ )
				{
					AddPointToBounds( bonespaceVerts[vertList[elem.firstVertIndex+j]], bv.mins, bv.maxs );
					vertCount++;
				}
			}
			for ( i = 0; i < convexOut.Count(); i++ )
			{
				// Attach this convex data to this particular bone
				int globalBoneIndex = m_pModel->boneLocalToGlobal[boneIndex];
				physcollision->SetConvexGameData( convexOut[i], globalBoneIndex + 1 );
			}

			CreateCollide( pPhys, convexOut.Base(), convexOut.Count(), bv );
			if( !g_quiet )
			{
				printf("%-24s (%3d verts, %d convex elements) volume: %4.2f\n", pPhys->m_name, vertCount, convexOut.Count(), pPhys->m_volume );
			}
			UnlinkCollisionModel( pPhys );
			AppendCollisionModel( pPhys );
		}
	}
	// remove any non-physical joints at this point
	CPhysCollisionModel *pPhys = m_pCollisionList;
	while (pPhys)
	{
		CPhysCollisionModel *pNext = pPhys->m_pNext;
		if ( !pPhys->m_pCollisionData )
		{
			UnlinkCollisionModel(pPhys);
			delete pPhys;
		}
		pPhys = pNext;
	}

	return 1;
}


#if 0
// debug visualization code - use this to dump out intermediate geometry files for visualization in glview.exe
void DumpToGLView( char const *pName, s_source_t *pmodel, Vector *worldVerts, int *used )
{
	int i;

	for ( i = 0; i < pmodel->numvertices; i++ )
		used[i] = -1;

	FILE *fp = fopen( pName, "w" );
	
	// dump the model to a glview file
	for ( i = 0; i < pmodel->nummeshes; i++ )
	{
		s_mesh_t *pmesh = pmodel->mesh + pmodel->meshindex[i];
		for ( int j = 0; j < pmesh->numfaces; j++ )
		{
			s_face_t *face = pmodel->face + pmesh->faceoffset + j;
			s_face_t globalFace;
			GlobalFace( &globalFace, pmesh, face );

			fprintf( fp, "3\n" );
			fprintf( fp, "%6.3f %6.3f %6.3f 0 1 0\n", worldVerts[globalFace.b].x, worldVerts[globalFace.b].y, worldVerts[globalFace.b].z );
			fprintf( fp, "%6.3f %6.3f %6.3f 1 0 0\n", worldVerts[globalFace.a].x, worldVerts[globalFace.a].y, worldVerts[globalFace.a].z );
			fprintf( fp, "%6.3f %6.3f %6.3f 0 0 1\n", worldVerts[globalFace.c].x, worldVerts[globalFace.c].y, worldVerts[globalFace.c].z );
			used[globalFace.a] = 0;
			used[globalFace.b] = 0;
			used[globalFace.c] = 0;
		}
	}

	// dump a triangle expanded around each vert to the file (to show degenerate tris' verts).
	for ( i = 0; i < pmodel->numvertices; i++ )
	{
		if ( used[i] < 0 )
			continue;

		fprintf( fp, "3\n" );
		Vector vert;
		vert = worldVerts[i] + Vector(0,0,5);
		fprintf( fp, "%6.3f %6.3f %6.3f 1 0 0\n", vert.x, vert.y, vert.z );
		vert = worldVerts[i] + Vector(5,0,-5);
		fprintf( fp, "%6.3f %6.3f %6.3f 0 1 0\n", vert.x, vert.y, vert.z );
		vert = worldVerts[i] + Vector(-5,0,-5);
		fprintf( fp, "%6.3f %6.3f %6.3f 0 0 1\n", vert.x, vert.y, vert.z );

	}

	fclose( fp );
}
#endif


int CJointedModel::ProcessSingleBody()
{

	// THIS CODE IS ONLY EXECUTED ON PROPS - i.e. NON-JOINTED MODELS


	static const int nMaxModels = MAX_EXTRA_COLLISION_MODELS + 1;

	if ( !m_bRootCollisionIsEmpty )
	{
		m_ExtraModels[MAX_EXTRA_COLLISION_MODELS].m_pSrc = m_pModel;
		m_ExtraModels[MAX_EXTRA_COLLISION_MODELS].m_bConcave = m_allowConcave;
		m_ExtraModels[MAX_EXTRA_COLLISION_MODELS].m_matOffset.SetToIdentity();
	}

	// Transform all the extra models using their offset matrices. May as well do this right away.
	for ( int i = 0; i < nMaxModels; i++ )
	{
		if ( m_ExtraModels[i].m_pSrc != NULL )
		{
			if ( !m_allowConcave )
				m_ExtraModels[i].m_bConcave = false;

			// Note this doesn't even touch the verts if the offset matrix is identity (assumed no position/rotation change in that case).
			ApplyOffsetToSrcVerts( m_ExtraModels[i].m_pSrc, m_ExtraModels[i].m_matOffset );
		}
	}

	// the root collision model is now 'empty' meaning that the overall collision model will be built entirely from
	// appended sources in the extra models list. If the extra model list is empty, we've got nothing to build
	// and that's an error.

	// find and group up the concave sources into a single welded source
	s_source_t *pConcaveSrc = NULL;
	s_source_t *pFallbackSrc = NULL;

	for ( int i = 0; i < nMaxModels; i++ )
	{
		if ( m_ExtraModels[i].m_pSrc != NULL )
		{
			if ( !pFallbackSrc )
				pFallbackSrc = m_ExtraModels[i].m_pSrc;

			if ( m_ExtraModels[i].m_bConcave )
			{
				if ( !pConcaveSrc )
				{
					pConcaveSrc = m_ExtraModels[i].m_pSrc;
				}
				else
				{
					AddSrcToSrc( pConcaveSrc, m_ExtraModels[i].m_pSrc );
				}
			}
		}
	}

	if ( !m_pModel )
	{
		if ( pConcaveSrc )
		{
			m_pModel = pConcaveSrc;
		}
		else if ( pFallbackSrc )
		{
			m_pModel = pFallbackSrc;
		}
		else
		{
			Error( "No valid physics source mesh!\n" );
		}
	}
	


	CUtlVector<CPhysConvex *> convexOut;
	CUtlVector<convexlist_t> convexList;

	CUtlVector<Vector> allworldspaceVerts;

	bool bValid = true;

	// if concavity is allowed, build out pConcaveSrc
	if ( pConcaveSrc && m_allowConcave )
	{

		CUtlVector<Vector> worldspaceVerts;
		worldspaceVerts.SetCount(pConcaveSrc->numvertices);
		ConvertToWorldSpace( worldspaceVerts, pConcaveSrc );

		allworldspaceVerts.AddVectorToTail( worldspaceVerts );
	
		CUtlVector<s_face_t> faceList;
		CUtlVector<int> vertList;
		
		for ( int i = 0; i < pConcaveSrc->nummeshes; i++ )
		{
			s_mesh_t *pmesh = pConcaveSrc->mesh + pConcaveSrc->meshindex[i];
			for ( int j = 0; j < pmesh->numfaces; j++ )
			{
				s_face_t *face = pConcaveSrc->face + pmesh->faceoffset + j;
				s_face_t globalFace;
				GlobalFace( &globalFace, pmesh, face );
				faceList.AddToTail( globalFace );
			}
		}
		BuildConvexListForFaceList( pConcaveSrc, convexList, vertList, faceList );
		bValid = BuildConvexesForLists( convexOut, convexList, vertList, worldspaceVerts, m_remove2d );
		
	}

	// now add convex extramodel sources that are themselves convex but want to be part of the larger concave system


	// we need to do this because once we've welded a combined model together, we can't tell what pieces of it
	// used their renderable geometry to create a convex hull. The next best (but still gross) assumption is
	// that if the physics src is the same src as the renderable geo then it should get naively convex-hulled,
	// then transformed into the given offset in the (possibly concave) overall physics model.

	for ( int i = 0; i < nMaxModels; i++ )
	{	
		if ( m_ExtraModels[i].m_pSrc != NULL && !m_ExtraModels[i].m_bConcave )
		{
			s_source_t *pmodel = m_ExtraModels[i].m_pSrc;
	
			CUtlVector<Vector> worldspaceVertsExtra;
			worldspaceVertsExtra.SetCount(pmodel->numvertices);
			ConvertToWorldSpace( worldspaceVertsExtra, pmodel );

			allworldspaceVerts.AddVectorToTail( worldspaceVertsExtra );
	
			CUtlVector<Vector *> vertsThisConvex;
			vertsThisConvex.RemoveAll();
	
			FOR_EACH_VEC( worldspaceVertsExtra, j )
			{
				// transform the verts using the offset
				//worldspaceVertsExtra[j] = VectorTransform( worldspaceVertsExtra[j], m_ExtraModels[i].m_matOffset );
	
				// this is ok because physcollision won't modify these, but wants non-const
				Vector *pVert = const_cast<Vector *>(&worldspaceVertsExtra[j]);
				vertsThisConvex.AddToTail( pVert );
			}
	
			CPhysConvex *pConvex = physcollision->ConvexFromVerts( vertsThisConvex.Base(), vertsThisConvex.Count() );
			if ( pConvex )
			{
				// Got something valid, attach this convex data to the root model
				physcollision->SetConvexGameData( pConvex, 0 );
				convexOut.AddToTail(pConvex);
			}
			else
			{
				MdlWarning("Error with convex elements of %s!\n", pmodel->filename );
				bValid = false;
			}
		}
	}

	if ( convexOut.Count() > m_maxConvex )
	{
		if ( g_ConvexHullCountOverride )
		{
			MdlWarning("Allowing costly collision model. Please be careful. (%d parts - %d normally allowed)\n", convexOut.Count(), m_maxConvex);
		}
		else
		{
			MdlWarning("COSTLY COLLISION MODEL!!!! (%d parts - %d allowed)\n", convexOut.Count(), m_maxConvex);
			bValid = false;
		}
	}

	if ( !bValid )
	{
		for ( int i = 0; i < convexOut.Count(); i++ )
		{
			physcollision->ConvexFree( convexOut[i] );
		}
		convexOut.Purge();
	}

	// either we don't want concave, or there was an error building it
	if ( !convexOut.Count() || !m_allowConcave )
	{
		convexOut.Purge();
	
		CUtlVector<Vector *> vertsThisConvex;
		vertsThisConvex.RemoveAll();
	
		FOR_EACH_VEC( allworldspaceVerts, j )
		{
			// this is ok because physcollision won't modify these, but wants non-const
			Vector *pVert = const_cast<Vector *>(&allworldspaceVerts[j]);
			vertsThisConvex.AddToTail( pVert );
		}
	
		CPhysConvex *pConvex = physcollision->ConvexFromVerts( vertsThisConvex.Base(), vertsThisConvex.Count() );
		if ( pConvex )
		{
			// Got something valid, attach this convex data to the root model
			physcollision->SetConvexGameData( pConvex, 0 );
			convexOut.AddToTail(pConvex);
		}
		else
		{
			Error( "Error building fallback convex hull!\n" );
		}
	}

	if ( convexOut.Count() )
	{
		if( !g_quiet )
		{
			printf("Model has %d convex sub-parts\n", convexOut.Count() );
		}

		CPhysCollisionModel *pPhys = new CPhysCollisionModel;
		SetCollisionModelDefaults( pPhys );

		boundingvolume_t bv;
		ClearBounds( bv.mins, bv.maxs );
		for ( int i = allworldspaceVerts.Count()-1; --i >= 0; )
		{
			AddPointToBounds( allworldspaceVerts[i], bv.mins, bv.maxs );
		}
		CreateCollide( pPhys, convexOut.Base(), convexOut.Count(), bv );

		// Init mass, write routine will distribute the total mass
		pPhys->m_mass = 1.0;
		char tmp[512];
		Q_FileBase( m_pModel->filename, tmp, sizeof( tmp ) );

		// UNDONE: Memory leak
		char *out = new char[strlen(tmp)+1];
		strcpy( out, tmp );
		pPhys->m_name = out;
		pPhys->m_parent = NULL;

		AppendCollisionModel( pPhys );
	}
	return 1;
}



#define MAX_ARGS	16
#define ARG_SIZE	256

//-----------------------------------------------------------------------------
// Purpose: HACKETY HACK - get the args into a buffer.
//			This checks for overflow, but it's not very robust - shouldn't be necessary though
// Input  : pArgs[][ARG_SIZE] - 
//			maxCount - array size of pargs
// Output : int - count actually used
//-----------------------------------------------------------------------------
int ReadArgs( char pArgs[][ARG_SIZE], int maxCount )
{
	int argCount = 0;

	while ( argCount < maxCount && TokenAvailable() )
	{
		GetToken(false);
		strncpy( pArgs[argCount], token, ARG_SIZE );
		argCount++;
	}

	return argCount;
}


//-----------------------------------------------------------------------------
// Purpose: Simple atof wrapper to keep from crashing on bad user input
// Input  : *pString - 
// Output : float
//-----------------------------------------------------------------------------
float Safe_atof( const char *pString )
{
	if ( !pString )
		return 0;

	return atof(pString);
}

//-----------------------------------------------------------------------------
// Purpose: Simple atoi wrapper to avoid crashing on bad user input
// Input  : *pString - 
// Output : int
//-----------------------------------------------------------------------------
int Safe_atoi( const char *pString )
{
	if ( !pString )
		return 0;

	return atoi(pString);
}


//-----------------------------------------------------------------------------
// Purpose: Add a constraint to our joint system
// Input  : &joints - 
//			*pJointName - 
//			*pJointAxis - 
//			*pJointType - 
//			*pLimitMin - 
//			*pLimitMax - 
//-----------------------------------------------------------------------------
void CCmd_JointConstrain( CJointedModel &joints, const char *pJointName, const char *pJointAxis, const char *pJointType, const char *pLimitMin, const char *pLimitMax, const char *pFriction )
{
	float limitMin = Safe_atof(pLimitMin);
	float limitMax = Safe_atof(pLimitMax);
	float friction = Safe_atof(pFriction);
	
	int axis = -1;
	int jointIndex = joints.FindLocalBoneNamed( pJointName );
	if ( !g_bCreateMakefile && jointIndex < 0 )
	{
		MdlWarning("Can't find joint %s\n", pJointName );
		return;
	}
	pJointName = joints.m_pModel->localBone[jointIndex].name;

	if ( pJointAxis )
	{
		axis = tolower(pJointAxis[0]) - 'x';
	}
	if ( axis < 0 || axis > 2 || limitMin > limitMax )
	{
		MdlError("Invalid joint constraint for %s\nCan't build ragdoll!\n", pJointName );
		return;
	}

	jointlimit_t jointType = JOINT_FREE;
	if ( !stricmp( pJointType, "free" ) )
	{
		jointType = JOINT_FREE;
	}
	else if ( !stricmp( pJointType, "fixed" ) )
	{
		jointType = JOINT_FIXED;
	}
	else if ( !stricmp( pJointType, "limit" ) )
	{
		jointType = JOINT_LIMIT;
	}
	else
	{
		MdlWarning("Unknown joint type %s (must be free, fixed, or limit)\n", pJointType );
		return;
	}
	joints.AddConstraint( pJointName, axis, jointType, limitMin, limitMax, friction );
}


//-----------------------------------------------------------------------------
// Purpose: Add a constraint to our joint system
// Input  : &joints - 
//			*pJointName - 
//			*pJointAxis - 
//			*pJointType - 
//			*pLimitMin - 
//			*pLimitMax - 
//-----------------------------------------------------------------------------
#ifdef MDLCOMPILE
void CCmd_JointConstrain( CJointedModel &joints, int nAxis, const char *pJointName, CDmeJointConstrain *pJointConstrain )
{
	if ( !pJointConstrain )
		return;

	const int jointIndex = FindLocalBoneNamed( joints.m_pModel, pJointName );
	if ( !g_bCreateMakefile && jointIndex < 0 )
	{
		MdlWarning("Can't find joint %s\n", pJointConstrain->GetName() );
		return;
	}
	pJointName = joints.m_pModel->localBone[jointIndex].name;

	const float limitMin = pJointConstrain->m_aLimitMin.Get();
	const float limitMax = pJointConstrain->m_aLimitMax.Get();
	const float friction = pJointConstrain->m_flFriction.Get();

	if ( nAxis < 0 || nAxis > 2 || limitMin > limitMax )
	{
		MdlError( "Invalid joint constraint for %s\nCan't build ragdoll!\n", pJointName );
		return;
	}

	const int nJointType = pJointConstrain->m_nType.Get();
	if ( nJointType < 0 || nJointType > 2 )
	{
		MdlWarning("Invalid joint constraint for %s, Unknown joint type %d (must be 0:free, 1:fixed, or 2:limit)\n", pJointName, nJointType );
		return;
	}

	const jointlimit_t jointType = static_cast< jointlimit_t >( nJointType );
	joints.AddConstraint( pJointName, nAxis, jointType, limitMin, limitMax, friction );
}
#endif // #ifdef MDLCOMPILE


//-----------------------------------------------------------------------------
// Purpose: Remove a joint from the system (don't create physical geometry for it)
// Input  : &joints - 
//			args[][ARG_SIZE] - 
//			argCount - 
//-----------------------------------------------------------------------------
// UNDONE: Automatically skip joints that will have mass that is too low?
void CCmd_JointSkip( CJointedModel &joints, const char *pName )
{
	int boneIndex = joints.FindLocalBoneNamed( pName );
	if ( boneIndex < 0 )
	{
		MdlWarning("Can't skip joint %s, not found\n", pName );
	}
	else
	{
//			printf("skipping joint %s\n", pName );
		joints.SkipBone( boneIndex );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Sets the object's mass.  The code will distribute this mass to each
//			part based on the collision model's volume
// Input  : &joints - 
//			*pMass - 
//-----------------------------------------------------------------------------
void CCmd_TotalMass( CJointedModel &joints, const char *pMass )
{
	joints.SetTotalMass( Safe_atof(pMass) );
}


//-----------------------------------------------------------------------------
// Purpose: verts from the bone named pChild are added to the collision model of pParent
// Input  : *pmodel - source model
//			*pParent - destination bone name
//			*pChild - source bone name
//-----------------------------------------------------------------------------
void CCmd_JointMerge( CJointedModel &joints, const char *pParent, const char *pChild )
{
	joints.AddMergeCommand( pParent, pChild );
	joints.MergeBones( pParent , pChild );
}


void CCmd_JointRoot( CJointedModel &joints, const char *pBone )
{
	// save the root bone name
	strcpy( joints.m_rootName, pBone );
}


void CCmd_JoinAnimatedFriction( CJointedModel &joints, const char *pMinFriction, const char *pMaxFriction, const char *pTimeIn, const char *pTimeHold, const char *pTimeOut )
{
	joints.m_flFrictionTimeIn = Safe_atof( pTimeIn );
	joints.m_flFrictionTimeOut = Safe_atof( pTimeOut );
	joints.m_flFrictionTimeHold = Safe_atof( pTimeHold );
	joints.m_iMinAnimatedFriction = Safe_atoi( pMinFriction );
	joints.m_iMaxAnimatedFriction = Safe_atoi( pMaxFriction );
	joints.m_bHasAnimatedFriction = true;
}

#ifdef MDLCOMPILE
void CCmd_JoinAnimatedFriction( CJointedModel &joints, CDmeJointAnimatedFriction *pJaf )
{
	if ( !pJaf )
		return;

	joints.m_flFrictionTimeIn = pJaf->m_tTimeIn.Get().GetSeconds();
	joints.m_flFrictionTimeOut = pJaf->m_tTimeOut.Get().GetSeconds();
	joints.m_flFrictionTimeHold = pJaf->m_tTimeHold.Get().GetSeconds();
	joints.m_iMinAnimatedFriction = pJaf->m_nMinFriction.Get();
	joints.m_iMaxAnimatedFriction = pJaf->m_nMaxFriction.Get();
	joints.m_bHasAnimatedFriction = true;
}
#endif // #ifdef MDLCOMPILE


//-----------------------------------------------------------------------------
// Purpose: Parses all legal commands inside the $collisionjoints {} block
// Input  : &joints - 
//-----------------------------------------------------------------------------
void ParseCollisionCommands( CJointedModel &joints )
{
	char command[512];

	char args[MAX_ARGS][ARG_SIZE];
	int argCount;

	g_ConvexHullCountOverride = false;

	while( GetToken( true ) )
	{
		if ( !strcmp( token, "}" ) )
			return;

		strcpy( command, token );

		if ( !stricmp( command, "$mass" ) )
		{
			argCount = ReadArgs( args, 1 );
			CCmd_TotalMass( joints, args[0] );
		}
		// default properties
		else if ( !stricmp( command, "$automass" ) )
		{
			joints.SetAutoMass();
		}
		else if ( !stricmp( command, "$inertia" ) )
		{
			argCount = ReadArgs( args, 1 );
			joints.DefaultInertia( Safe_atof( args[0] ) );
		}
		else if ( !stricmp( command, "$damping" ) )
		{
			argCount = ReadArgs( args, 1 );
			joints.DefaultDamping( Safe_atof( args[0] ) );
		}
		else if ( !stricmp( command, "$rotdamping" ) )
		{
			argCount = ReadArgs( args, 1 );
			joints.DefaultRotdamping( Safe_atof( args[0] ) );
		}
		else if ( !stricmp( command, "$drag" ) )
		{
			argCount = ReadArgs( args, 1 );
			joints.DefaultDrag( Safe_atof( args[0] ) );
		}
		else if ( !stricmp( command, "$rollingDrag" ) )
		{
			argCount = ReadArgs( args, 1 );
			// JAY: Removed this in favor of heuristic/tuning approach
			//joints.DefaultRollingDrag( Safe_atof( args[0] ) );
		}
		else if ( !stricmp( command, "$maxconvexpieces") )
		{
			argCount = ReadArgs( args, 1 );
			joints.SetMaxConvex( Safe_atoi(args[0]) );
		}
		else if ( !stricmp( command, "$remove2d") )
		{
			joints.Remove2DConvex();
		}
		else if ( !stricmp( command, "$concaveperjoint") )
		{
			joints.AllowConcaveJoints();
		}
		else if ( !stricmp( command, "$weldposition") )
		{
			argCount = ReadArgs(args,1);
			g_WeldVertEpsilon = Safe_atof( args[0] );
		}
		else if ( !stricmp( command, "$weldnormal") )
		{
			argCount = ReadArgs(args,1);
			g_WeldNormalEpsilon = Safe_atof( args[0] );
		}
		else if ( !stricmp( command, "$concave" ) )
		{
			joints.AllowConcave();
		}
		else if (!stricmp(command, "$convexhullcountoverride"))
		{
			argCount = ReadArgs(args, 1);
			g_ConvexHullCountOverride = true;
		}
		else if ( !stricmp( command, "$masscenter" ) )
		{
			argCount = ReadArgs( args, 3 );
			Vector center;
			center.Init( Safe_atof(args[0]), Safe_atof(args[1]), Safe_atof(args[2]) );
			joints.ForceMassCenter( center );
		}
		// joint commands
		else if ( !stricmp( command, "$jointskip" ) )
		{
			argCount = ReadArgs( args, 1 );
			CCmd_JointSkip( joints, args[0] );
		}
		else if ( !stricmp( command, "$jointmerge" ) )
		{
			argCount = ReadArgs( args, 2 );
			CCmd_JointMerge( joints, args[0], args[1] );
		}
		else if ( !stricmp( command, "$rootbone" ) )
		{
			argCount = ReadArgs( args, 1 );
			CCmd_JointRoot( joints, args[0] );
		}
		else if ( !stricmp( command, "$jointconstrain" ) )
		{
			argCount = ReadArgs( args, 6 );
			char *pFriction = args[5];
			if ( argCount < 6 )
			{
				pFriction = "1.0";
			}
			CCmd_JointConstrain( joints, args[0], args[1], args[2], args[3], args[4], pFriction );
		}
		// joint properties
		else if ( !stricmp( command, "$jointinertia" ) )
		{
			argCount = ReadArgs( args, 2 );
			joints.JointInertia( args[0], Safe_atof( args[1] ) );
		}
		else if ( !stricmp( command, "$jointdamping" ) )
		{
			argCount = ReadArgs( args, 2 );
			joints.JointDamping( args[0], Safe_atof( args[1] ) );
		}
		else if ( !stricmp( command, "$jointrotdamping" ) )
		{
			argCount = ReadArgs( args, 2 );
			joints.JointRotdamping( args[0], Safe_atof( args[1] ) );
		}
		else if ( !stricmp( command, "$jointmassbias" ) )
		{
			argCount = ReadArgs( args, 2 );
			joints.JointMassBias( args[0], Safe_atof( args[1] ) );
		}
		else if ( !stricmp( command, "$noselfcollisions" ) )
		{
			joints.SetNoSelfCollisions();
		}
		else if ( !stricmp( command, "$jointcollide" ) )
		{
			argCount = ReadArgs( args, 2 );
			joints.AppendCollisionPair( args[0], args[1] );
		}
		else if ( !stricmp( command, "$animatedfriction" ) )
		{
			argCount = ReadArgs( args, 5 );

			if ( argCount == 5 )
			{
				CCmd_JoinAnimatedFriction( joints, args[0], args[1], args[2], args[3], args[4] );
			}
		}
		else if ( !stricmp( command, "$assumeworldspace") )
		{
			joints.m_bAssumeWorldspace = true;
		}
		else if ( !stricmp( command, "$addconvexsrc" ) )
		{
			argCount = ReadArgs( args, 1 );
			joints.AddConvexSrc( args[0] );
		}
		else if ( !stricmp( command, "$jointcollidealltoall" ) )
		{
			char szTempNames[32][256];
			int nNumEntries = 0;

			GetToken( true );
			if ( token[0] == '{' )
			{
				while ( GetToken(true) && nNumEntries < 32 && strcmp( token, "}" ) )
				{
					V_strcpy_safe( szTempNames[nNumEntries], token );
					nNumEntries++;
				}
			}

			//printf( "Num entries: %i\n", nNumEntries );
			
			for ( int i=0; i<nNumEntries; i++ )
			{
				for ( int j=0; j<nNumEntries; j++ )
				{
					if ( i != j )
					{
						const char* szBoneNameA = szTempNames[i];
						const char* szBoneNameB = szTempNames[j];

						joints.AppendCollisionPair( szBoneNameA, szBoneNameB );
					}
				}
			}

		}
		else if ( !stricmp( command, "$jointnocollide" ) )
		{
			argCount = ReadArgs( args, 2 );
			joints.RemoveCollisionPair( args[0], args[1] );
			joints.RemoveCollisionPair( args[1], args[0] ); // lol
		}
		else
		{
			MdlWarning("Unknown command %s in collision series\n", command );
		}
	}
}


void Cmd_CollisionText( void )
{
	int level = 1;

	if ( !GetToken( true ) )
		return;

	if ( token[0] != '{' )
		return;


	while ( GetToken(true) )
	{
		if ( !strcmp( token, "}" ) )
		{
			level--;
			if ( level <= 0 )
				break;
			g_JointedModel.AddText( " }\n" );
		}
		else if ( !strcmp( token, "{" ) )
		{
			g_JointedModel.AddText( "{" );
			level++;
		}
		else
		{
			// tokens inside braces  are quoted
			if ( level > 1 )
			{
				g_JointedModel.AddText( "\"" );
				g_JointedModel.AddText( token );
				g_JointedModel.AddText( "\" " );
			}
			else
			{
				g_JointedModel.AddText( token );
				g_JointedModel.AddText( " " );
			}
		}
	}
}


static bool LoadSurfaceProps( const char *pMaterialFilename )
{
	if ( !physprops )
		return false;

	FileHandle_t fp = g_pFileSystem->Open( pMaterialFilename, "rb", TOOLS_READ_PATH_ID );
	if ( fp == FILESYSTEM_INVALID_HANDLE )
		return false;

	int len = g_pFileSystem->Size( fp );
	char *pText = new char[len+1];
	g_pFileSystem->Read( pText, len, fp );
	g_pFileSystem->Close( fp );
	
	pText[len]=0;

	physprops->ParseSurfaceData( pMaterialFilename, pText );

	delete[] pText;

	return true;
}

void LoadSurfacePropsAll()
{
	static bool bIsLoaded = false;
	// already loaded
	if ( bIsLoaded )
		return;

	const char *SURFACEPROP_MANIFEST_FILE = "scripts/surfaceproperties_manifest.txt";
	KeyValues *manifest = new KeyValues( SURFACEPROP_MANIFEST_FILE );
	if ( manifest->LoadFromFile( g_pFileSystem, SURFACEPROP_MANIFEST_FILE, "GAME" ) )
	{
		bIsLoaded = true;
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "file" ) )
			{
				// Add
				LoadSurfaceProps( sub->GetString() );
				continue;
			}
		}
	}

	manifest->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: Entry point for script processing.  Delegate to necessary subroutines.
//			Parse the collisionmodel {} and collisionjoints {} chunks
// Input  : separateJoints - whether this has a constraint system or not (true if it does)
// Output : int
//-----------------------------------------------------------------------------
int DoCollisionModel( bool separateJoints )
{
	char name[512];
	s_source_t *pmodel;

	// name
	if ( !GetToken(false) ) 
		return 0;

	strcpyn( name, token );

	PhysicsDLLPath( "VPHYSICS.DLL" );

//	CreateInterfaceFn physicsFactory = GetPhysicsFactory();
	CreateInterfaceFn physicsFactory = Sys_GetFactory(Sys_LoadModule( "vphysics.dll" ));
	if ( !physicsFactory )
		return 0;

//	g_pPhysics2 = (IPhysics2*)physicsFactory(VPHYSICS2_INTERFACE_VERSION, NULL);
	physcollision = (IPhysicsCollision *)physicsFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, NULL );
	physprops = (IPhysicsSurfaceProps *)physicsFactory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL );
	LoadSurfacePropsAll();

	int nummaterials = g_nummaterials;
	int numtextures = g_numtextures;

	// Special case where the input collision is empty.
	// This means that a list of collision meshes follows.
	if ( !V_strcmp( name, "blank" ) )
	{
		g_JointedModel.m_bRootCollisionIsEmpty = true;

		// g_JointedModel.m_pModel is still NULL,
		// which will be handled when the additional meshes are appended.
	}
	else
	{

		pmodel = Load_Source( name, "SMD", false, false, false );
		if ( !pmodel )
			return 0;

		// auto-remove any new materials/textures
		if (nummaterials && numtextures && (numtextures != g_numtextures || nummaterials != g_nummaterials))
		{
			g_numtextures = numtextures;
			g_nummaterials = nummaterials;

			pmodel->texmap[0] = 0;
		}

		// all bones map to themselves by default
		g_JointedModel.SetSource( pmodel );
	}
	
	bool parseCommands = false;

	// If the next token is a { that means a data block for the collision model
	if (GetToken(true))
	{
		if ( !strcmp( token, "{" ) )
		{
			parseCommands = true;
		}
		else
		{
			UnGetToken();
		}
	}

	if ( parseCommands )
	{
		ParseCollisionCommands( g_JointedModel );
	}

	g_JointedModel.m_isJointed = separateJoints;

	// collision script is stored in g_JointedModel for later processing
	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: Walk the list of models, add up the volume
// Input  : *pList - 
// Output : float
//-----------------------------------------------------------------------------
float TotalVolume( CPhysCollisionModel *pList )
{
	float volume = 0;
	while ( pList )
	{
		volume += pList->m_volume * pList->m_massBias;
		pList = pList->m_pNext;
	}

	return volume;
}

//-----------------------------------------------------------------------------
// Purpose: Write key/value pairs out to a file
// Input  : *fp - output file
//			*pKeyName - key name
//			outputData - type specific output data
//-----------------------------------------------------------------------------
void KeyWriteInt( FILE *fp, const char *pKeyName, int outputData )
{
	fprintf( fp, "\"%s\" \"%d\"\n", pKeyName, outputData );
}

void KeyWriteIntPair( FILE *fp, const char *pKeyName, int outputData0, int outputData1 )
{
	fprintf( fp, "\"%s\" \"%d,%d\"\n", pKeyName, outputData0, outputData1 );
}
void KeyWriteString( FILE *fp, const char *pKeyName, const char *outputData )
{
	fprintf( fp, "\"%s\" \"%s\"\n", pKeyName, outputData );
}

void KeyWriteVector3( FILE *fp, const char *pKeyName, const Vector& outputData )
{
	fprintf( fp, "\"%s\" \"%f %f %f\"\n", pKeyName, outputData[0], outputData[1], outputData[2] );
}

void KeyWriteQAngle( FILE *fp, const char *pKeyName, const QAngle& outputData )
{
	fprintf( fp, "\"%s\" \"%f %f %f\"\n", pKeyName, outputData[0], outputData[1], outputData[2] );
}

void KeyWriteFloat( FILE *fp, const char *pKeyName, float outputData )
{
	fprintf( fp, "\"%s\" \"%f\"\n", pKeyName, outputData );
}


void CJointedModel::FixCollisionHierarchy( )
{
	if ( m_pCollisionList )
	{
		CPhysCollisionModel *pPhys = m_pCollisionList;

		FixBoneList( );
		// Point parents at joints that are actually in the model
		for ( ;pPhys; pPhys = pPhys->m_pNext )
		{
			pPhys->m_parent = FixParent( pPhys->m_parent );
		}

		// sort the list so parents come before children
		SortCollisionList();
		// Now remap the constraints to bones to 
		// Now that bones are in order, set physics indices in main bone structure

		CJointConstraint *pList = g_JointedModel.m_pConstraintList;
		while ( pList )
		{
			pList->m_pJointName = FixParent( pList->m_pJointName );
			pList = pList->m_pNext;
		}

		pPhys = m_pCollisionList;
		int i;
		for ( i = 0; i < g_numbones; i++ )
		{
			g_bonetable[i].physicsBoneIndex = -1;
		}
		int index = 0;
		while ( pPhys )
		{
			int boneIndex = FindBoneInTable( pPhys->m_name );
			if ( boneIndex >= 0 )
			{
				g_bonetable[boneIndex].physicsBoneIndex = index;
			}
			pPhys = pPhys->m_pNext;
			index ++;
		}
		for ( i = 0; i < g_numbones; i++ )
		{
			// if no bone was set, set to parent bone
			if ( g_bonetable[i].physicsBoneIndex < 0 )
			{
				int index = g_bonetable[i].parent;
				int bone = -1;
				while ( index >= 0 )
				{
					bone = g_bonetable[index].physicsBoneIndex;
					if ( bone >= 0 )
						break;
					index = g_bonetable[index].parent;
				}

				// found one?
				if ( bone >= 0 )
				{
					g_bonetable[i].physicsBoneIndex = bone;
				}
				else
				{
					// just set physics to affect root
					g_bonetable[i].physicsBoneIndex = 0;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Builds the physics/collision model.
//			This must execute after the model has been simplified!!
//-----------------------------------------------------------------------------
void CollisionModel_Build( void )
{
	// no collision model referenced
	if ( !g_JointedModel.m_pModel && !g_JointedModel.m_bRootCollisionIsEmpty )
		return;

//	Physics2Collision_Build(&g_JointedModel);

	g_JointedModel.Simplify();
	if ( g_JointedModel.m_isJointed )
	{
		g_JointedModel.ProcessJointedModel( );
	}
	else
	{
		g_JointedModel.ProcessSingleBody( );
	}
	g_JointedModel.FixCollisionHierarchy(  );
	if( !g_quiet )
	{
		printf("Collision model completed.\n" );
	}
	g_JointedModel.ComputeMass();
}

void BuildRagdollConstraint( CPhysCollisionModel *pPhys, constraint_ragdollparams_t &ragdoll )
{
	memset( &ragdoll, 0, sizeof(ragdoll) );
	ragdoll.parentIndex = g_JointedModel.CollisionIndex(pPhys->m_parent);
	ragdoll.childIndex = g_JointedModel.CollisionIndex(pPhys->m_name);
	if ( ragdoll.parentIndex < 0 || ragdoll.childIndex < 0 )
	{
		MdlWarning("Constraint between bone %s and %s\n", pPhys->m_name, pPhys->m_parent );
		if ( ragdoll.childIndex < 0 )
			MdlWarning("\"%s\" does not appear in collision model!!!\n", pPhys->m_name );
		if ( ragdoll.parentIndex < 0 )
			MdlWarning("\"%s\" does not appear in collision model!!!\n", pPhys->m_parent );
		MdlError("Bad constraint in ragdoll\n");
	}
	CJointConstraint *pList = g_JointedModel.m_pConstraintList;
	while ( pList )
	{
		int index = g_JointedModel.CollisionIndex(pList->m_pJointName);
		CPhysCollisionModel *pListModel = g_JointedModel.GetCollisionModel(pList->m_pJointName);
		if ( index < 0 )
		{
			MdlError("Rotation constraint on bone \"%s\" which does not appear in collision model!!!\n", pList->m_pJointName );
		}
		else if ( (!pListModel->m_parent || g_JointedModel.CollisionIndex(pListModel->m_parent) < 0) && stricmp( pList->m_pJointName, g_JointedModel.m_rootName ) )
		{
			MdlError("Rotation constraint on bone \"%s\" which has no parent!!!\n", pList->m_pJointName );
		}
		else if ( index == ragdoll.childIndex )
		{
			switch ( pList->m_jointType )
			{
			case JOINT_LIMIT:
				ragdoll.axes[pList->m_axis].SetAxisFriction( pList->m_limitMin, pList->m_limitMax, pList->m_friction );
				break;
			case JOINT_FIXED:
				ragdoll.axes[pList->m_axis].SetAxisFriction( 0,0,0 );
				break;
			case JOINT_FREE:
				ragdoll.axes[pList->m_axis].SetAxisFriction( -360, 360, pList->m_friction );
				break;
			}
		}
		pList = pList->m_pNext;
	}
}

float GetCollisionModelMass()
{
	return g_JointedModel.m_totalMass;
}

void CollisionModel_ExpandBBox( Vector &mins, Vector &maxs )
{
	// don't do fixup for ragdolls
	if ( g_JointedModel.m_isJointed )
		return;

	if ( g_JointedModel.m_pCollisionList )
	{
		Vector collideMins, collideMaxs;

		physcollision->CollideGetAABB( &collideMins, &collideMaxs, g_JointedModel.m_pCollisionList->m_pCollisionData, vec3_origin, vec3_angle );
		
		// add the 0.25 inch collision separation as well
		const float radius = 0.25;
		collideMins -= Vector(radius,radius,radius);
		collideMaxs += Vector(radius,radius,radius);

		AddPointToBounds( collideMins, mins, maxs );
		AddPointToBounds( collideMaxs, mins, maxs );
	}
}

void CollisionModel_SetName( const char *pName )
{
	g_JointedModel.SetOverrideName(pName);
}

//-----------------------------------------------------------------------------
// Purpose: Write out any data that's been saved in the globals
//-----------------------------------------------------------------------------
void CollisionModel_Write( long checkSum )
{
//	Physics2Collision_Write();

	if ( g_JointedModel.m_pCollisionList )
	{
		CPhysCollisionModel *pPhys = g_JointedModel.m_pCollisionList;

		char filename[512];

		strcpy( filename, gamedir );
//		if( *g_pPlatformName )
//		{
//			strcat( filename, "platform_" );
//			strcat( filename, g_pPlatformName );
//			strcat( filename, "/" );	
//		}
		strcat( filename, "models/" );	
		strcat( filename, g_JointedModel.m_pOverrideName ? g_JointedModel.m_pOverrideName : g_outname );	

		float volume = TotalVolume( pPhys );
		if ( volume <= 0 )
			volume = 1;
		if( !g_quiet )
		{
			printf("Collision model volume %.2f in^3\n", volume );
		}

		Q_SetExtension( filename, ".phy", sizeof( filename ) );
		CPlainAutoPtr< CP4File > spFile( g_p4factory->AccessFile( filename ) );
		spFile->Edit();
		FILE *fp = fopen( filename, "wb" );
		if ( fp )
		{
			// write out the collision header (size is version)
			phyheader_t header;
			header.size = sizeof(header);
			header.id = 0;
			header.checkSum = checkSum;

			header.solidCount = 0;
			pPhys = g_JointedModel.m_pCollisionList;
			while ( pPhys )
			{
				header.solidCount++;
				pPhys = pPhys->m_pNext;
			}

			fwrite( &header, sizeof(header), 1, fp );

			// Write out the binary physics collision data
			pPhys = g_JointedModel.m_pCollisionList;
			while ( pPhys )
			{
				int size = physcollision->CollideSize( pPhys->m_pCollisionData );
				fwrite( &size, sizeof(int), 1, fp );
				char *buf = (char *)malloc( size );
				physcollision->CollideWrite( buf, pPhys->m_pCollisionData );
				fwrite( buf, size, 1, fp );
				free( buf );
				pPhys = pPhys->m_pNext;
			}

			// write out the properties of each solid
			int solidIndex = 0;
			pPhys = g_JointedModel.m_pCollisionList;
			while ( pPhys )
			{
				pPhys->m_mass = ((pPhys->m_volume * pPhys->m_massBias) / volume) * g_JointedModel.m_totalMass;
				if ( pPhys->m_mass < 1.0 )
					pPhys->m_mass = 1.0;

				fprintf( fp, "solid {\n" );
				KeyWriteInt( fp, "index", solidIndex );
				KeyWriteString( fp, "name", pPhys->m_name );
				if ( pPhys->m_parent )
				{
					KeyWriteString( fp, "parent", pPhys->m_parent );
				}
			
				KeyWriteFloat( fp, "mass", pPhys->m_mass );
				//KeyWriteFloat( fp, "volume", pPhys->m_volume );

				char* pSurfaceProps = GetSurfaceProp( pPhys->m_name );

				KeyWriteString( fp, "surfaceprop", pSurfaceProps );
				KeyWriteFloat( fp, "damping", pPhys->m_damping );
				KeyWriteFloat( fp, "rotdamping", pPhys->m_rotdamping );
				
				if ( pPhys->m_dragCoefficient != -1 )
				{
					KeyWriteFloat( fp, "drag", pPhys->m_dragCoefficient );
				}
				KeyWriteFloat( fp, "inertia", pPhys->m_inertia );
				KeyWriteFloat( fp, "volume", pPhys->m_volume );
				if ( pPhys->m_massBias != 1.0f )
				{
					KeyWriteFloat( fp, "massbias", pPhys->m_massBias );
				}

				fprintf( fp, "}\n" );
				pPhys = pPhys->m_pNext;
				solidIndex++;

			}

			// by default, write constraints from each limb to its parent
			pPhys = g_JointedModel.m_pCollisionList;
			while ( pPhys )
			{
				// check to see if bone collapse/remap has left this with parent pointing at itself
				if ( pPhys->m_parent )
				{
					constraint_ragdollparams_t ragdoll;
					BuildRagdollConstraint( pPhys, ragdoll );
					if ( ragdoll.parentIndex != ragdoll.childIndex )
					{
						fprintf( fp, "ragdollconstraint {\n" );
						KeyWriteInt( fp, "parent", ragdoll.parentIndex );
						KeyWriteInt( fp, "child", ragdoll.childIndex );
						KeyWriteFloat( fp, "xmin", ragdoll.axes[0].minRotation );
						KeyWriteFloat( fp, "xmax", ragdoll.axes[0].maxRotation );
						KeyWriteFloat( fp, "xfriction", ragdoll.axes[0].torque );
						KeyWriteFloat( fp, "ymin", ragdoll.axes[1].minRotation );
						KeyWriteFloat( fp, "ymax", ragdoll.axes[1].maxRotation );
						KeyWriteFloat( fp, "yfriction", ragdoll.axes[1].torque );
						KeyWriteFloat( fp, "zmin", ragdoll.axes[2].minRotation );
						KeyWriteFloat( fp, "zmax", ragdoll.axes[2].maxRotation );
						KeyWriteFloat( fp, "zfriction", ragdoll.axes[2].torque );
						fprintf( fp, "}\n" );
					}
				}
				pPhys = pPhys->m_pNext;

			}
			if ( g_JointedModel.m_noSelfCollisions )
			{
				fprintf(fp, "collisionrules {\n" );
				KeyWriteInt( fp, "selfcollisions", 0 );
				fprintf(fp, "}\n");
			}
			else if ( g_JointedModel.m_pCollisionPairs )
			{
				fprintf(fp, "collisionrules {\n" );
				collisionpair_t *pPair = g_JointedModel.m_pCollisionPairs;
				while ( pPair )
				{
					pPair->obj0 = g_JointedModel.CollisionIndex( pPair->pName0 );
					pPair->obj1 = g_JointedModel.CollisionIndex( pPair->pName1 );
					if ( pPair->obj0 >= 0 && pPair->obj1 >= 0 && pPair->obj0 != pPair->obj1 )
					{
						KeyWriteIntPair( fp, "collisionpair", pPair->obj0, pPair->obj1 );
					}
					else
					{
						MdlWarning("Invalid collision pair (%s, %s)\n", pPair->pName0, pPair->pName1 );
					}
					pPair = pPair->pNext;
				}
				fprintf(fp, "}\n");
			}

			if ( g_JointedModel.m_bHasAnimatedFriction == true )
			{
				fprintf( fp, "animatedfriction {\n" );
				KeyWriteFloat( fp, "animfrictionmin", g_JointedModel.m_iMinAnimatedFriction );
				KeyWriteFloat( fp, "animfrictionmax", g_JointedModel.m_iMaxAnimatedFriction );
				KeyWriteFloat( fp, "animfrictiontimein", g_JointedModel.m_flFrictionTimeIn );
				KeyWriteFloat( fp, "animfrictiontimeout", g_JointedModel.m_flFrictionTimeOut );
				KeyWriteFloat( fp, "animfrictiontimehold", g_JointedModel.m_flFrictionTimeHold );
				fprintf( fp, "}\n" );
			}

			// block that is only parsed by the editor
			fprintf( fp, "editparams {\n" );
			KeyWriteString( fp, "rootname", g_JointedModel.m_rootName );
			KeyWriteFloat( fp, "totalmass", g_JointedModel.m_totalMass );
			if ( g_JointedModel.m_allowConcave )
			{
				KeyWriteInt( fp, "concave", 1 );
			}
			for ( int k = 0; k < g_JointedModel.m_mergeList.Count(); k++ )
			{
				char buf[512];
				Q_snprintf( buf, sizeof(buf), "%s,%s", g_JointedModel.m_mergeList[k].pParent, g_JointedModel.m_mergeList[k].pChild );
				KeyWriteString( fp, "jointmerge", buf );
			}

			fprintf( fp, "}\n" );

			char terminator = 0;
			if ( g_JointedModel.m_textCommands.Count() )
			{
				fwrite( g_JointedModel.m_textCommands.Base(), g_JointedModel.m_textCommands.Count(), 1, fp );
			}
			fwrite( &terminator, sizeof(terminator), 1, fp );
			fclose( fp );
			spFile->Add();
		}
		else
		{
			MdlWarning("Error writing %s!!!\n", filename );
		}
	}
}



#ifdef MDLCOMPILE
//-----------------------------------------------------------------------------
// mdlcompile
//
// mdlcompile uses DMX instead of qc as input
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Parses all legal commands inside the $collisionjoints {} block
// Input  : &joints - 
//-----------------------------------------------------------------------------
void ParseCollisionCommands( CJointedModel &joints, CDmeCollisionModel *pCollisionModel, bool bStaticProp )
{
	g_JointedModel.m_isJointed = false;

	if ( !pCollisionModel )
		return;

	if ( pCollisionModel->m_bAutomaticMassComputation.Get() )
	{
		joints.SetAutoMass();
	}
	else
	{
		joints.SetTotalMass( pCollisionModel->m_flMass.Get() );
	}

	joints.DefaultInertia( pCollisionModel->m_flInertia.Get() );
	joints.DefaultDamping( pCollisionModel->m_flDamping.Get() );
	joints.DefaultRotdamping( pCollisionModel->m_flRotationalDamping.Get() );
	joints.DefaultDrag( pCollisionModel->m_flDrag.Get() );
	joints.SetMaxConvex( pCollisionModel->m_nMaxConvexPieces.Get() );

	if ( pCollisionModel->m_bRemove2D.Get() )
	{
		joints.Remove2DConvex();
	}

	float flWeld = pCollisionModel->m_flWeldPositionTolerance.Get();
	if ( flWeld < 0.0f )
	{
		MdlWarning( "1003: Invalid negative weld position tolerance (%f), ignoring and using %f\n", flWeld, g_WeldVertEpsilon );
	}
	else
	{
		g_WeldVertEpsilon = flWeld;
	}

	flWeld = pCollisionModel->m_flWeldNormalTolerance.Get();
	if ( flWeld < 0.0f )
	{
		MdlWarning( "1004: Invalid negative weld normal tolerance (%f), ignoring and using %f\n", flWeld, g_WeldNormalEpsilon );
	}
	else if ( flWeld > g_WeldNormalEpsilon )
	{
		MdlWarning( "1005: Weld normal tolerance too high (%f), should be slightly less than 1, ignoring and using %f\n", flWeld, g_WeldNormalEpsilon );
	}
	else
	{
		g_WeldNormalEpsilon = flWeld;
	}

	if ( pCollisionModel->m_bConcave.Get() )
	{
		joints.AllowConcave();
	}

	if ( pCollisionModel->m_bForceMassCenter.Get() )
	{
		joints.ForceMassCenter( pCollisionModel->m_vecMassCenter.Get() );
	}

	joints.m_bAssumeWorldspace = pCollisionModel->m_bAssumeWorldSpace.Get();

	CDmeCollisionJoints *pCollisionJoints = CastElement< CDmeCollisionJoints >( pCollisionModel );
	if ( !bStaticProp && pCollisionJoints )
	{
		if ( pCollisionJoints->m_bConcavePerJoint.Get() )
		{
			joints.AllowConcaveJoints();
		}

		if ( !pCollisionJoints->m_bSelfCollisions.Get() )
		{
			joints.SetNoSelfCollisions();
		}

		if ( !pCollisionJoints->m_bSelfCollisions.Get() )
		{
			joints.SetNoSelfCollisions();
		}

		CCmd_JointRoot( joints, pCollisionJoints->m_RootBone.Get() );
		CCmd_JoinAnimatedFriction( joints, pCollisionJoints->m_AnimatedFriction.GetElement() );

		for ( int nIndex = 0; nIndex < pCollisionJoints->m_JointSkipList.Count(); ++nIndex )
		{
			CCmd_JointSkip( joints, pCollisionJoints->m_JointSkipList.Element( nIndex ) );
		}

		int nValidCollisionJointCount = 0;

		for ( int i = 0; i < pCollisionJoints->m_JointList.Count(); ++i )
		{
			const CDmeCollisionJoint *pCollisionJoint = pCollisionJoints->m_JointList.Element( i );
			if ( !pCollisionJoint )
			{
				MdlWarning( "1000: root.collisionModel.joints[ %d ] exists but undefined\n", i );
				continue;
			}

			const char *pJointName = pCollisionJoint->GetName();

			const int nJointIndex = FindLocalBoneNamed( joints.m_pModel, pJointName );
			if ( !g_bCreateMakefile && nJointIndex < 0 )
			{
				MdlWarning( "1001: root.collisionModel.joints[ %d ] refers to joint \"%s\" but that joint wasn't defined in the model\n", i, pJointName );
				continue;
			}

			pJointName = joints.m_pModel->localBone[nJointIndex].name;

			joints.JointMassBias( pJointName, pCollisionJoint->m_flMassBias.Get() );
			joints.JointInertia( pJointName, pCollisionJoint->m_flInertia.Get() );
			joints.JointDamping( pJointName, pCollisionJoint->m_flDamping.Get() );
			joints.JointRotdamping( pJointName, pCollisionJoint->m_flRotDamping.Get() );

			CCmd_JointConstrain( joints, 0 /* x */, pJointName, pCollisionJoint->m_ConstrainX.GetElement() );
			CCmd_JointConstrain( joints, 1 /* y */, pJointName, pCollisionJoint->m_ConstrainY.GetElement() );
			CCmd_JointConstrain( joints, 2 /* z */, pJointName, pCollisionJoint->m_ConstrainZ.GetElement() );

			for ( int j = 0; j < pCollisionJoint->m_JointMergeList.Count(); ++j )
			{
				CCmd_JointMerge( joints, pJointName, pCollisionJoint->m_JointMergeList.Element( j ) );
			}

			for ( int j = 0; j < pCollisionJoint->m_JointMergeList.Count(); ++j )
			{
				joints.AppendCollisionPair( pJointName, pCollisionJoint->m_JointCollideList.Element( j ) );
			}

			++nValidCollisionJointCount;
		}

		if ( nValidCollisionJointCount > 0 )
		{
			g_JointedModel.m_isJointed = true;
		}
		else
		{
			MdlWarning( "1002: Jointed collision model defined but no valid CDmeCollisionJoints, making non-jointed\n" );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Entry point for script processing. This version is used by preprocessed files
//-----------------------------------------------------------------------------
int DoCollisionModel( s_source_t *pSource, CDmElement *pInfo, bool bStaticProp )
{
	CDmeCollisionModel *pCollisionModel = CastElement< CDmeCollisionModel >( pInfo );
	if ( !pCollisionModel )
		return 0;

	PhysicsDLLPath( "VPHYSICS.DLL" );
	CreateInterfaceFn physicsFactory = GetPhysicsFactory();
	if ( !physicsFactory )
		return 0;

	physcollision = (IPhysicsCollision *)physicsFactory( VPHYSICS_COLLISION_INTERFACE_VERSION, NULL );
	physprops = (IPhysicsSurfaceProps *)physicsFactory( VPHYSICS_SURFACEPROPS_INTERFACE_VERSION, NULL );
	LoadSurfacePropsAll();

	// all bones map to themselves by default
	g_JointedModel.SetSource( pSource );
	ParseCollisionCommands( g_JointedModel, pCollisionModel, bStaticProp );

	// collision script is stored in g_JointedModel for later processing
	return 1;
}


//-----------------------------------------------------------------------------
// Loads collision text keyvalues from the passed string
// NOTE: It essentially is packing keyValues -> keyValues
//       but the .phy keyValues parser is more particular
//       about the format
//-----------------------------------------------------------------------------
void LoadCollisionText( const char *pszKeyValues )
{
	if ( !pszKeyValues )
		return;

	KeyValues *pKeyValues = new KeyValues( "collisionText" );
	if ( !pKeyValues )
		return;

	KeyValues::AutoDelete adKeyValues( pKeyValues );

	pKeyValues->UsesEscapeSequences( true );
	if ( pKeyValues->LoadFromBuffer( "collisionText", pszKeyValues ) )
	{
		while ( pKeyValues )
		{
			g_JointedModel.AddText( pKeyValues->GetName() );

			g_JointedModel.AddText( " {" );
			for ( KeyValues *pKv = pKeyValues->GetFirstValue(); pKv; pKv = pKv->GetNextValue() )
			{
				g_JointedModel.AddText( " \"" );
				g_JointedModel.AddText( pKv->GetName() );
				g_JointedModel.AddText( "\" \"" );
				g_JointedModel.AddText( pKv->GetString() );
				g_JointedModel.AddText( "\"" );
			}
			g_JointedModel.AddText( " }\n" );

			pKeyValues = pKeyValues->GetNextKey();
		}
	}
}

#endif // #ifdef MDLCOMPILE
