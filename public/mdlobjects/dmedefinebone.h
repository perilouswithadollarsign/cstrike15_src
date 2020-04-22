//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// Dme representation of QC: $definebone
//
//===========================================================================//

#ifndef DMEDEFINEBONE_H
#define DMEDEFINEBONE_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "datamodel/dmelement.h"


//-----------------------------------------------------------------------------
// Dme representation of QC: $definebone
//-----------------------------------------------------------------------------
class CDmeDefineBone : public CDmElement
{
	DEFINE_ELEMENT( CDmeDefineBone, CDmElement );

public:
	CDmaString			m_Parent;
	CDmaVar< Vector >	m_Translation;
	CDmaVar< QAngle >	m_Rotation;
	CDmaVar< Vector >	m_RealignTranslation;
	CDmaVar< QAngle >	m_RealignRotation;
	CDmaString m_sContentsDescription;

};


#endif // DMEDEFINEBONE_H
