//====== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. =====
//
// DmeAnimationAssemblyCommand
//
//=============================================================================


#ifndef DMEANIMATIONASSEMBLYCOMMAND_H
#define DMEANIMATIONASSEMBLYCOMMAND_H


#if defined( _WIN32 )
#pragma once
#endif


// Valve includes
#include "mdlobjects/dmeassemblycommand.h"
#include "mdlobjects/dmesequence.h"
#include "movieobjects/dmelog.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeSequence;


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeAnimationAssemblyCommand : public CDmeAssemblyCommand
{
	DEFINE_ELEMENT( CDmeAnimationAssemblyCommand, CDmeAssemblyCommand );

public:
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeFixupLoop : public CDmeAnimationAssemblyCommand
{
	DEFINE_ELEMENT( CDmeFixupLoop, CDmeAnimationAssemblyCommand );

public:
	// From CDmeAssemblyCommand
	virtual bool Apply( CDmElement *pDmElement );

	CDmaVar< int > m_nStartFrame;
	CDmaVar< int > m_nEndFrame;

protected:
	template< class T > void Apply(
		CDmeTypedLog< T > *pDmeTypedLog,
		const DmeTime_t &dmeTimeStart,
		const DmeTime_t &dmeTimeEnd ) const;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeSubtract : public CDmeAnimationAssemblyCommand
{
	DEFINE_ELEMENT( CDmeSubtract, CDmeAnimationAssemblyCommand );

public:
	// From CDmeAssemblyCommand
	virtual bool Apply( CDmElement *pDmElement );

	CDmaElement< CDmeSequenceBase > m_eSequence;
	CDmaVar< int > m_nFrame;

protected:
	template< class T > void Subtract(
		CDmeTypedLog< T > *pDmeTypedLogDst,
		const CDmeTypedLog< T > *pDmeTypedLogSrc,
		const DmeTime_t &dmeTimeSrc ) const;

	virtual void Subtract(
		Vector &vResult,
		const Vector &vDst,
		const Vector &vSrc ) const;

	virtual void Subtract(
		Quaternion &vResult,
		const Quaternion &vDst,
		const Quaternion &vSrc ) const;

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmePreSubtract : public CDmeSubtract
{
	DEFINE_ELEMENT( CDmePreSubtract, CDmeSubtract );

public:

protected:
	virtual void Subtract(
		Vector &vResult,
		const Vector &vDst,
		const Vector &vSrc ) const;

	virtual void Subtract(
		Quaternion &vResult,
		const Quaternion &vDst,
		const Quaternion &vSrc ) const;

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeRotateTo : public CDmeAnimationAssemblyCommand
{
	DEFINE_ELEMENT( CDmeRotateTo, CDmeAnimationAssemblyCommand );

public:
	// From CDmeAssemblyCommand
	virtual bool Apply( CDmElement *pDmElement );

	CDmaVar< float > m_flAngle;	// Specified in degrees

protected:
	void SubApply( CDmeDag *pDmeDag, CDmeChannelsClip *pDmeChannelsClip, bool bZUp );

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeBoneMaskCmd : public CDmeAnimationAssemblyCommand
{
	DEFINE_ELEMENT( CDmeBoneMaskCmd, CDmeAnimationAssemblyCommand );

public:
	// From CDmeAssemblyCommand
	virtual bool Apply( CDmElement *pDmElement );

protected:
	void SubApply( CDmeChannelsClip *pDmeChannelsClip, CDmeDag *pDmeDag, CDmeBoneMask *pDmeBoneMask );

};


#endif // DMEANIMATIONASSEMBLYCOMMAND_H