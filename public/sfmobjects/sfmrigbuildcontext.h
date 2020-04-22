//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef SFMRIGBUILDCONTEXT_H
#define SFMRIGBUILDCONTEXT_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmerigconstraintoperators.h"
#include "movieobjects/dmelog.h"
#include "studio.h"
#include "tier1/utldict.h"

class CDmeAnimationSet;
class CDmeFilmClip;
class CDmeClip;
class CDmeRigHandle;
class CDmeChannelsClip;
class CDmeGameModel;


class CRigBuildContext
{

public:

	enum 
	{
		DESC_SKELETON = (1<<0),
		DESC_RIG = (1<<1),
		DESC_CONSTRAINTS = (1<<2),
		DESC_ALL = DESC_SKELETON | DESC_RIG | DESC_CONSTRAINTS,
	};

	enum eRigBuildContextType
	{
		RBC_ATTACH = 0,
		RBC_DETACH,
	};

	explicit CRigBuildContext( eRigBuildContextType type, CDmeAnimationSet *pAnimSet, CDmeFilmClip *pShot, CDmeClip *pMovie );
	~CRigBuildContext();
	bool IsValid();

	CDmeRigHandle *FindRigHandle( char const *pchHandleName );
	CDmeRigHandle *CreateRigHandle( char const *pchHandleName, char const *pchRigSubGroup, const Vector &p, const Quaternion &q );

	CDmeRigBaseConstraintOperator *FindOrAddConstraintTypeForObject( bool *pbUpdatingHandles, EConstraintType eType, char const *pchObjectName, char const *pchConstraintName, const DmFileId_t &fileId );
	void CreateWeightChannels( CDmeRigBaseConstraintOperator *pConstraint, int nCount, CDmeDag *const pRigHandles[] );
	Vector	GetBonePosition( char const *pchBoneName ) const;
	CDmeDag *FindBone( const char *pchBoneName ) const;
	void	SetRigName( char const *pchRigName );
	void	Describe( int flags );
	void	DisconnectBoneChannel( CUtlVector< CDmeChannel * > &list, int boneIndex, CDmeDag *pBone, EConstraintType eType );
	void	AttachHandleToBone( CDmeRigHandle *pRigHandle, CDmeDag *pBone ) const;

	// Creates a new set of bone channels for the "detached" bone
	void		PerformDetach();
	void		FinishAttach();

protected:

	CDmeRigBaseConstraintOperator *InstanceConstraint( EConstraintType eType, char const *pchName, const DmFileId_t &fileId );

	void CreateRigHandleChannels( CDmeChannel *list[ 2 ], CDmeChannelsClip *pChannelsClip, CDmeRigHandle *pHandle );
	CDmElement *CreateRigAnimationSetControl( CDmeAnimationSet *pAnimSet, CDmeDag *pDag, CDmeChannel *pDagChannel, int controlType );
	void AddControlToAnimationSetRig( CDmeAnimationSet *pAnimSet, char const *pchGroupName, char const *pchControlGroupName, char const *pchControlName );
	CDmeChannelsClip* CreateChannelsClip();
	void CreateTransformChannels( CDmeDag *pDag, const char* baseName, CDmeChannelsClip *pChannelsClip );

	void DescribeSkeleton();
	void DescribeRig();
	void DescribeConstraints();

	void DescribeSkeleton_R( int depth, CDmeDag *bone );

	void DescribeOperatorsForDag( int depth, CDmeDag *pDag );

	CDmeChannel *DisconnectBoneChannel( CDmeDag *pBone, char const *pchTargetAttribute );
	void		DetachAddBoneChannelsToChannelsClip( CUtlVector< CDmeChannel * > &boneChannels );
	void		DetachCreateBoneChannels( CStudioHdr &hdr, CUtlVector< CDmeChannel * > &boneChannels );
	void		DetachGatherAllOperators( CUtlRBTree< CDmeOperator * > &operators );

	void		AttachInit();
	void		DetachInit();

	CDmeDag		*FindSceneRoot();
	void		InsertSubHandles_R( CDmeRigHandle *pRoot );


	void		FindChannelsForDag( CDmeDag* pDagNode, CUtlRBTree< CDmeOperator * > &operators );
	void		FindBoneChannels_R( CDmeDag *bone, CUtlRBTree< CDmeOperator * > &operators );
	void		FindBoneChannels( CUtlRBTree< CDmeOperator * > &operators );
	void		FindRigHandleChannels( CUtlRBTree< CDmeOperator * > &operators );

public:

	bool							m_bValid;
	int								m_nRigNumber;
	DmeLog_TimeSelection_t			m_timeSelection;
	CDmeHandle< CDmeAnimationSet >	m_hAnimationSet;
	CDmeHandle< CDmeFilmClip >		m_hShot;
	CDmeHandle< CDmeClip >			m_hMovie;
	CDmeHandle< CDmeGameModel >		m_hGameModel;
	CDmeHandle< CDmeDag >			m_hRigSceneRoot;
	CDmeHandle< CDmeChannelsClip >	m_hDstChannelsClip;

	const CStudioHdr				*m_pStudioHdr;
	matrix3x4_t						m_ReferencePoseBoneToWorld[ MAXSTUDIOBONES ];

	CUtlVector< CDmeChannel * >		m_DetachChannels;
	CUtlDict< CDmeHandle< CDmeRigHandle >, short >				m_RigHandles;
	CUtlVector< CDmeHandle< CDmeRigBaseConstraintOperator > >	m_Constraints;

};


#endif // SFMRIGBUILDCONTEXT_H
