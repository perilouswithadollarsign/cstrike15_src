//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme $collisionjoints
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmecollisionjoints.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//===========================================================================
// Expose DmeJointConstrain class to the scene database 
//===========================================================================
IMPLEMENT_ELEMENT_FACTORY( DmeJointConstrain, CDmeJointConstrain );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeJointConstrain::OnConstruction()
{
	m_nType.InitAndSet( this, "type", 1 );	// 0: Free, 1: Fixed, 2: Limit
	m_aLimitMin.Init( this, "minAngle" );
	m_aLimitMax.Init( this, "maxAngle" );
	m_flFriction.Init( this, "friction" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeJointConstrain::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeJointConstrain::OnAttributeChanged( CDmAttribute *pAttribute )
{
	// Limit "type" to [0:2]
	// TODO: Check for recursive behavior

	if ( pAttribute == m_nType.GetAttribute() )
	{
		m_nType.Set( clamp( m_nType.Get(), 0, 2 ) );
	}
}


//===========================================================================
// Expose DmeJointAnimatedFriction class to the scene database 
//===========================================================================
IMPLEMENT_ELEMENT_FACTORY( DmeJointAnimatedFriction, CDmeJointAnimatedFriction );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeJointAnimatedFriction::OnConstruction()
{
	m_nMinFriction.InitAndSet( this, "minFriction", 1.0f );
	m_nMaxFriction.InitAndSet( this, "maxFriction", 1.0f );
	m_tTimeIn.Init( this, "timeIn" );
	m_tTimeHold.Init( this, "timeHold" );
	m_tTimeOut.Init( this, "timeOut" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeJointAnimatedFriction::OnDestruction()
{
}


//===========================================================================
// Expose DmeCollisionJoint class to the scene database 
//===========================================================================
IMPLEMENT_ELEMENT_FACTORY( DmeCollisionJoint, CDmeCollisionJoint );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCollisionJoint::OnConstruction()
{
	m_flMassBias.InitAndSet( this, "massBias", 1.0f );
	m_flDamping.InitAndSet( this, "damping", 0.0f );
	m_flRotDamping.InitAndSet( this, "rotationalDamping", 0.0f );
	m_flInertia.InitAndSet( this, "inertia", 1.0f );
	m_ConstrainX.Init( this, "constrainX" );
	m_ConstrainY.Init( this, "constrainY" );
	m_ConstrainZ.Init( this, "constrainZ" );
	m_JointMergeList.Init( this, "jointMergeList" );
	m_JointCollideList.Init( this, "jointCollideList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCollisionJoint::OnDestruction()
{
}



//===========================================================================
// Expose DmeKeyValueString class to the scene database 
//===========================================================================
IMPLEMENT_ELEMENT_FACTORY( DmeCollisionJoints, CDmeCollisionJoints );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCollisionJoints::OnConstruction()
{
	m_bConcavePerJoint.InitAndSet( this, "concavePerJoint", false );
	m_bSelfCollisions.InitAndSet( this, "selfCollisions", true );
	m_bBoneFollower.InitAndSet( this, "boneFollower", false );
	m_RootBone.Init( this, "rootBone" );
	m_AnimatedFriction.Init( this, "animatedFriction" );
	m_JointSkipList.Init( this, "jointSkipList" );
	m_JointList.Init( this, "jointList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCollisionJoints::OnDestruction()
{
}
