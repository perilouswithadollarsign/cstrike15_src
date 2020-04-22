//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Animation commands
//
//===========================================================================


#ifndef DMEANIMCMD_H
#define DMEANIMCMD_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "movieobjects/dmelog.h"


// Forward declarations
class CDmeMotionControl;
class CDmeSequenceBase;
class CDmeSequence;


//-----------------------------------------------------------------------------
// Animation commands
//-----------------------------------------------------------------------------
class CDmeAnimCmd : public CDmElement
{
	DEFINE_ELEMENT( CDmeAnimCmd, CDmElement );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

	bool HasAssemblyDmElement() const { return Q_strcmp( "", GetAssemblyDmElementTypeString() ) != 0; }
};


//-----------------------------------------------------------------------------
// QC fixuploop
//-----------------------------------------------------------------------------
class CDmeAnimCmdFixupLoop : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdFixupLoop, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString();

	CDmaVar< int > m_nStartFrame;
	CDmaVar< int > m_nEndFrame;

};


//-----------------------------------------------------------------------------
// QC weightlisrt
//-----------------------------------------------------------------------------
class CDmeAnimCmdWeightList : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdWeightList, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

	CDmaString m_sWeightListName;
};


//-----------------------------------------------------------------------------
// QC subtract
//-----------------------------------------------------------------------------
class CDmeAnimCmdSubtract : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdSubtract, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString();

	CDmaElement< CDmeSequenceBase > m_eAnimation;
	CDmaVar< int > m_nFrame;
};


//-----------------------------------------------------------------------------
// QC presubtract
//-----------------------------------------------------------------------------
class CDmeAnimCmdPreSubtract : public CDmeAnimCmdSubtract
{
	DEFINE_ELEMENT( CDmeAnimCmdPreSubtract, CDmeAnimCmdSubtract );

public:
	static const char *GetAssemblyDmElementTypeString();

};


//-----------------------------------------------------------------------------
// QC align, alignto, alignbone, alignboneto
//
// align animationName, boneName = "", any motion, sourceFrame, destinationFrame
// alignTo animationName, boneName = "", motion = X | Y, sourceFrame = 0, destinationFrame = 0
// alignBone animationName, bone, any motion, sourceFrame, destinationFrame
// alignBoneTo animationName, bone, motion = X | Y, sourceFrame = 0, destinationFrame = 0
//-----------------------------------------------------------------------------
class CDmeAnimCmdAlign : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdAlign, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

	CDmaElement< CDmeSequenceBase > m_eAnimation;
	CDmaString m_sBoneName;
	CDmaVar< int > m_nSourceFrame;
	CDmaVar< int > m_nDestinatonFrame;
	CDmaElement< CDmeMotionControl > m_eMotionControl;
};


// TODO: QC match
// TODO: QC matchblend
// TODO: QC worldspaceblend
// TODO: QC worldspaceblendloop

//-----------------------------------------------------------------------------
// QC rotateTo
//-----------------------------------------------------------------------------
class CDmeAnimCmdRotateTo : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdRotateTo, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString();

	CDmaVar< float > m_flAngle;	// Specified in degrees
};


// ikRule, ikFixup handled separately


//-----------------------------------------------------------------------------
// QC walkframe
//-----------------------------------------------------------------------------
class CDmeAnimCmdWalkFrame : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdWalkFrame, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

	CDmaElement< CDmeMotionControl > m_eMotionControl;
	CDmaVar< int > m_nEndFrame;
};

// TODO: QC walkalignto
// TODO: QC walkalign

//-----------------------------------------------------------------------------
// QC derivative
//-----------------------------------------------------------------------------
class CDmeAnimCmdDerivative : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdDerivative, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

	CDmaVar< float > m_flScale;
};

// TODO: QC noanimation


//-----------------------------------------------------------------------------
// QC lineardelta
//-----------------------------------------------------------------------------
class CDmeAnimCmdLinearDelta : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdLinearDelta, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

};


//-----------------------------------------------------------------------------
// QC splinedelta
//-----------------------------------------------------------------------------
class CDmeAnimCmdSplineDelta : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdSplineDelta, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

};


//-----------------------------------------------------------------------------
// QC compress
//-----------------------------------------------------------------------------
class CDmeAnimCmdCompress : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdCompress, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

	CDmaVar< int > m_nSkipFrames;
};


//-----------------------------------------------------------------------------
// QC numframes
//-----------------------------------------------------------------------------
class CDmeAnimCmdNumFrames : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdNumFrames, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

	CDmaVar< int > m_nFrames;
};

// TODO: counterrotate
// TODO: counterrotateto
// TODO: localhierarchy


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeAnimCmdLocalHierarchy : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdLocalHierarchy, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

	CDmaString m_sBoneName;
	CDmaString m_sParentBoneName;
	CDmaVar< float > m_flStartFrame;
	CDmaVar< float > m_flPeakFrame;
	CDmaVar< float > m_flTailFrame;
	CDmaVar< float > m_flEndFrame;

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeAnimCmdNoAnimation : public CDmeAnimCmd
{
	DEFINE_ELEMENT( CDmeAnimCmdNoAnimation, CDmeAnimCmd );

public:
	static const char *GetAssemblyDmElementTypeString() { return ""; }

	CDmaVar< bool > m_bNullAttr;
};


#endif // DMEANIMCMD_H