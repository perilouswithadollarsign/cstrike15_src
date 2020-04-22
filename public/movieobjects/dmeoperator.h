//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// The abstract base operator class - all actions within the scenegraph happen via operators
//
//=============================================================================

#ifndef DMEOPERATOR_H
#define DMEOPERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"

#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// A class representing an generic operator
//-----------------------------------------------------------------------------
class CDmeOperator : public IDmeOperator, public CDmElement
{
	DEFINE_ELEMENT( CDmeOperator, CDmElement );

public:
	virtual bool IsDirty(); // ie needs to operate
	virtual void Operate() {}
	virtual const char* GetOperatorName() const { return m_Name.Get(); }

	virtual void GetInputAttributes ( CUtlVector< CDmAttribute * > &attrs ) {}
	virtual void GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs ) {}
	virtual void GatherInputOperators( CUtlVector< CDmeOperator * > &operators );

	virtual void SetSortKey( int key );
	virtual int GetSortKey() const;
	int	m_nSortKey;
};

void GatherOperatorsForElement(  CDmElement *pElement, CUtlVector< CDmeOperator * > &operators );

#endif // DMEOPERATOR_H
