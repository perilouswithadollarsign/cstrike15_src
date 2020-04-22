//====== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. =====
//
// Purpose:
//
//=============================================================================


#ifndef DMEMATSYSROOT_H
#define DMEMATSYSROOT_H

#if defined( _WIN32 )
#pragma once
#endif


// Valve includes
#include "datamodel/dmattributevar.h"
#include "datamodel/dmelement.h"
#include "mathlib/mathlib.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeshape.h"
#include "movieobjects/dmemdl.h"


//=============================================================================
// CDmeMatSysSettings
//=============================================================================
class CDmeMatSysPanelSettings : public CDmElement
{
	DEFINE_ELEMENT( CDmeMatSysPanelSettings, CDmElement );

public:
	CDmaVar< Color > m_cBackgroundColor;
	CDmaVar< Color > m_cAmbientColor;
	CDmaVar< bool > m_bDrawGroundPlane;
	CDmaVar< bool > m_bDrawOriginAxis;
};


//=============================================================================
// CDmeMatSysRoot
//=============================================================================
class CDmeMatSysRoot : public CDmeDag
{
	DEFINE_ELEMENT( CDmeMatSysRoot, CDmeDag );

public:
	CDmaElement< CDmeMatSysPanelSettings > m_Settings;
};


//=============================================================================
//
//=============================================================================
class IDmeMatSysModel
{
	virtual void GetSequenceList( CUtlVector< CUtlString > *pOutList ) = 0;
	virtual void GetActivityList( CUtlVector< CUtlString > *pOutList ) = 0;
	// Returns number of frame or -1 for error
	virtual int SelectSequence( const char *pszSequenceName );
	virtual void SetTime( DmeTime_t dmeTime );
	virtual void SetFrame( float flFrame );
};


//=============================================================================
// CDmeMatSysMDLDag
//=============================================================================
class CDmeMatSysMDLDag : public IDmeMatSysModel, public CDmeDag
{
	DEFINE_ELEMENT( CDmeMatSysMDLDag, CDmeDag );

public:
	studiohdr_t *GetStudioHdr() const;

	// From IDmeMatSysModel
	virtual void GetSequenceList( CUtlVector< CUtlString > *pOutList );
	virtual void GetActivityList( CUtlVector< CUtlString > *pOutList );
	virtual int SelectSequence( const char *pszSequenceName );
	virtual void SetTime( DmeTime_t dmeTime );
	virtual void SetFrame( float flFrame );

	// Convenience Functions calling into CDmeMDL which is the shape
	void SetMDL( MDLHandle_t hMDL );
	MDLHandle_t GetMDL() const;


protected:
	CDmeMDL *GetDmeMDL() const;
};


//=============================================================================
// CDmeMatSysDMXDag
//=============================================================================
class CDmeMatSysDMXDag : public IDmeMatSysModel, public CDmeDag
{
	DEFINE_ELEMENT( CDmeMatSysDMXDag, CDmeDag );

public:
	// From CDmeDag
	virtual void Draw( CDmeDrawSettings *pDrawSettings = NULL );

	// From IDmeMatSysModel
	virtual void GetSequenceList( CUtlVector< CUtlString > *pOutList );
	virtual void GetActivityList( CUtlVector< CUtlString > *pOutList );

	void SetDmxRoot( CDmElement *pDmxRoot );

protected:
	CDmaElement< CDmElement > m_eDmxRoot;
	DmElementHandle_t m_hDmxModel;
};


//=============================================================================
// CDmeMatSysMPPDag
//=============================================================================
class CDmeMatSysMPPDag : public IDmeMatSysModel, public CDmeDag
{
	DEFINE_ELEMENT( CDmeMatSysMPPDag, CDmeDag );

public:
	// From CDmeDag
	virtual void Draw( CDmeDrawSettings *pDrawSettings = NULL );

	// From IDmeMatSysModel
	virtual void GetSequenceList( CUtlVector< CUtlString > *pOutList );
	virtual void GetActivityList( CUtlVector< CUtlString > *pOutList );
	virtual int SelectSequence( const char *pszSequenceName );
	virtual void SetTime( DmeTime_t dmeTime );
	virtual void SetFrame( float flFrame );

	void SetMppRoot( CDmElement *pMppRoot );

protected:
	CDmaElement< CDmElement > m_eMppRoot;
	DmElementHandle_t m_hDmeBodyGroupList;
	DmElementHandle_t m_hDmeSequenceList;
	CUtlVector< DmElementHandle_t > m_hChildren;

	// These are used for animation playback
	DmElementHandle_t m_hDmeSequence;
	CUtlVector< IDmeOperator * > m_dmeOperatorList;

	void RemoveNullAndImplicitChildren();
};


#endif // DMEMATSYSROOT_H