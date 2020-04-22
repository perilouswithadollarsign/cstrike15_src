//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Dme Ik rules
//
//===========================================================================

#ifndef DMEIK_H
#define DMEIK_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// DmeIkChain
//-----------------------------------------------------------------------------
class CDmeIkChain : public CDmElement
{
	DEFINE_ELEMENT( CDmeIkChain, CDmElement );

public:
	CDmaString					m_sEndJoint;
	CDmaVar< float >			m_flHeight;
	CDmaVar< float >			m_flPad;
	CDmaVar< float >			m_flFloor;
	CDmaVar< Vector >			m_vKnee;
	CDmaVar< Vector >			m_vCenter;
};


//-----------------------------------------------------------------------------
// DmeIkLock
//-----------------------------------------------------------------------------
class CDmeIkLock : public CDmElement
{
	DEFINE_ELEMENT( CDmeIkLock, CDmElement );

public:
	CDmaElement< CDmeIkChain >	m_eIkChain;
	CDmaVar< float >			m_flLockPosition;
	CDmaVar< float >			m_flLockRotation;
};


//-----------------------------------------------------------------------------
// DmeIkRange
//-----------------------------------------------------------------------------
class CDmeIkRange : public CDmElement
{
	DEFINE_ELEMENT( CDmeIkRange, CDmElement );

public:
	CDmaVar< int >				m_nStartFrame;
	CDmaVar< int >				m_nMaxStartFrame;
	CDmaVar< int >				m_nMaxEndFrame;
	CDmaVar< int >				m_nEndFrame;
};


//-----------------------------------------------------------------------------
// DmeIkRule
//-----------------------------------------------------------------------------
class CDmeIkRule : public CDmElement
{
	DEFINE_ELEMENT( CDmeIkRule, CDmElement );

public:
	enum Use_t
	{
		USE_NONE = 0,
		USE_SEQUENCE = 1,
		USE_SOURCE = 2
	};

	CDmaElement< CDmeIkChain >	m_eIkChain;
	CDmaElement< CDmeIkRange >	m_eRange;
	CDmaVar< int >				m_nUseType;
};


//-----------------------------------------------------------------------------
// DmeIkTouchRule
//-----------------------------------------------------------------------------
class CDmeIkTouchRule : public CDmeIkRule
{
	DEFINE_ELEMENT( CDmeIkTouchRule, CDmeIkRule );

public:
	CDmaString					m_sBoneName;
};


//-----------------------------------------------------------------------------
// DmeIkFootstepRule
//-----------------------------------------------------------------------------
class CDmeIkFootstepRule : public CDmeIkRule
{
	DEFINE_ELEMENT( CDmeIkFootstepRule, CDmeIkRule );

public:
	// These are optional
//	CDmaVar< int >				m_nContact;
//	CDmaVar< float >			m_flHeight;
//	CDmaVar< float >			m_flFloor;
//	CDmaVar< float >			m_flPad;
};


//-----------------------------------------------------------------------------
// DmeIkAttachmentRule
//-----------------------------------------------------------------------------
class CDmeIkAttachmentRule : public CDmeIkRule
{
	DEFINE_ELEMENT( CDmeIkAttachmentRule, CDmeIkRule );

public:

	CDmaString					m_sAttachmentName;
	CDmaVar< float >			m_flRadius;

	// These are optional
//	CDmaString					m_sFallbackBone;
//	CDmaVar< Vector >			m_vFallbackPoint;
//	CDmaVar< Quaternion >		m_qFallbackRotation;
};


//-----------------------------------------------------------------------------
// DmeIkReleaseRule
//-----------------------------------------------------------------------------
class CDmeIkReleaseRule : public CDmeIkRule
{
	DEFINE_ELEMENT( CDmeIkReleaseRule, CDmeIkRule );

public:
};


#endif // DMEIK_H