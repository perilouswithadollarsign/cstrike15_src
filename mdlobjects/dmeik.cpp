//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Dme Ik rules
//
//===========================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeik.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// DmeIkChain
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeIkChain, CDmeIkChain );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkChain::OnConstruction()
{
	m_sEndJoint.Init( this, "endJoint" );
	m_flHeight.InitAndSet( this, "height", 18.0f );
	m_flPad.Init( this, "pad" );
	m_flFloor.Init( this, "floor" );
	m_vKnee.Init( this, "knee" );
	m_vCenter.Init( this, "center" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkChain::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// DmeIkLock
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeIkLock, CDmeIkLock );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkLock::OnConstruction()
{
	m_eIkChain.Init( this, "ikChain", FATTRIB_NEVERCOPY );
	m_flLockPosition.Init( this, "lockPosition" );
	m_flLockRotation.Init( this, "lockRotation" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkLock::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// DmeIkRange
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeIkRange, CDmeIkRange );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkRange::OnConstruction()
{
	m_nStartFrame.Init( this, "startFrame" );
	m_nEndFrame.Init( this, "endFrame" );
	m_nMaxStartFrame.Init( this, "maxStartFrame" );
	m_nMaxEndFrame.Init( this, "maxEndFrame" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkRange::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// DmeIkRule
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeIkRule, CDmeIkRule );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkRule::OnConstruction()
{
	m_eIkChain.Init( this, "ikChain", FATTRIB_NEVERCOPY );
	m_eRange.InitAndCreate( this, "range" );
	m_nUseType.InitAndSet( this, "useType", USE_SEQUENCE );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkRule::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// DmeIkTouchRule
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeIkTouchRule, CDmeIkTouchRule );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkTouchRule::OnConstruction()
{
	m_sBoneName.Init( this, "boneName" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkTouchRule::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// DmeIkFootstepRule
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeIkFootstepRule, CDmeIkFootstepRule );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkFootstepRule::OnConstruction()
{
	// These are optional
//	m_nContact.Init( this, "contact" );
//	m_flHeight.Init( this, "height" );
//	m_flFloor.Init( this, "floor" );
//	m_flPad.Init( this, "pad" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkFootstepRule::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// DmeIkAttachmentRule
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeIkAttachmentRule, CDmeIkAttachmentRule );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkAttachmentRule::OnConstruction()
{
	m_sAttachmentName.Init( this, "attachmentName" );
	m_flRadius.Init( this, "radius" );

	// These are optional
//	m_sFallbackBone.Init( this, "fallbackBone" );
//	m_vFallbackPoint.Init( this, "fallbackPoint" );
//	m_qFallbackRotation.Init( this, "fallbackRotation" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkAttachmentRule::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// DmeIkReleaseRule
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeIkReleaseRule, CDmeIkReleaseRule );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkReleaseRule::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIkReleaseRule::OnDestruction()
{
}