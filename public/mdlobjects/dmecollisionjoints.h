//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme $collisionjoints
//
//===========================================================================//

#ifndef DMECOLLISIONJOINTS_H
#define DMECOLLISIONJOINTS_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmecollisionmodel.h"


//-----------------------------------------------------------------------------
// Dme $jointconstrain
//-----------------------------------------------------------------------------
class CDmeJointConstrain : public CDmElement
{
	DEFINE_ELEMENT( CDmeJointConstrain, CDmElement );

	virtual void OnAttributeChanged( CDmAttribute *pAttribute );

public:
	CDmaVar< int >								m_nType;		// 0: Free, 1: Fixed, 2: Limit
	CDmaVar< float >							m_aLimitMin;
	CDmaVar< float >							m_aLimitMax;
	CDmaVar< float >							m_flFriction;

};


//-----------------------------------------------------------------------------
// Dme $animatedfriction
//-----------------------------------------------------------------------------
class CDmeJointAnimatedFriction : public CDmElement
{
	DEFINE_ELEMENT( CDmeJointAnimatedFriction, CDmElement );

public:
	CDmaVar< int >								m_nMinFriction;
	CDmaVar< int >								m_nMaxFriction;
	CDmaVar< DmeTime_t >						m_tTimeIn;
	CDmaVar< DmeTime_t >						m_tTimeHold;
	CDmaVar< DmeTime_t >						m_tTimeOut;

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeCollisionJoint : public CDmElement
{
	DEFINE_ELEMENT( CDmeCollisionJoint, CDmElement );

#ifndef SWIG
public:
#endif // #ifndef SWIG

	CDmaVar< float>								m_flMassBias;
	CDmaVar< float>								m_flDamping;
	CDmaVar< float>								m_flRotDamping;
	CDmaVar< float>								m_flInertia;
	CDmaElement< CDmeJointConstrain >			m_ConstrainX;
	CDmaElement< CDmeJointConstrain >			m_ConstrainY;
	CDmaElement< CDmeJointConstrain >			m_ConstrainZ;
	CDmaStringArray								m_JointMergeList;
	CDmaStringArray								m_JointCollideList;
};


//-----------------------------------------------------------------------------
// Dme $collisionjoints
//-----------------------------------------------------------------------------
class CDmeCollisionJoints : public CDmeCollisionModel
{
	DEFINE_ELEMENT( CDmeCollisionJoints, CDmeCollisionModel );

#ifndef SWIG
public:
#endif // #ifndef SWIG

	CDmaVar< bool >								m_bConcavePerJoint;	
	CDmaVar< bool >								m_bSelfCollisions;	
	CDmaVar< bool >								m_bBoneFollower;
	CDmaString									m_RootBone;
	CDmaElement< CDmeJointAnimatedFriction>		m_AnimatedFriction;
	CDmaStringArray								m_JointSkipList;
	CDmaElementArray< CDmeCollisionJoint >		m_JointList;
};


#endif // DMECOLLISIONJOINTS_H