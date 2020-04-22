//=========== Copyright (c) Valve Corporation, All rights reserved. ===========
//
//=============================================================================

#ifndef DMEFLEXRULES_H
#define DMEFLEXRULES_H

#if defined( COMPILER_MSVC )
#pragma once
#endif


// Valve includes
#include "datamodel/dmattributevar.h"
#include "mathlib/expressioncalculator.h"
#include "movieobjects/dmeoperator.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeCombinationOperator;


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeFlexRuleBase : public CDmeOperator
{
	DEFINE_ELEMENT( CDmeFlexRuleBase, CDmeOperator );

public:

	// CDmeOperator
	virtual void GetInputAttributes ( CUtlVector< CDmAttribute * > &attrs );
	virtual void GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs );

	CDmAttribute *ResultAttr() { return m_flResult.GetAttribute(); }

protected:
	friend class CDmeFlexRules;

	CDmeFlexRules *GetFlexRules() const;

	CDmaVar< float > m_flResult;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeFlexRulePassThrough : public CDmeFlexRuleBase
{
	DEFINE_ELEMENT( CDmeFlexRulePassThrough, CDmeFlexRuleBase );

public:
	// CDmeOperator
	virtual void Operate();

protected:
	friend class CDmeFlexRules;

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeFlexRuleExpression : public CDmeFlexRuleBase
{
	DEFINE_ELEMENT( CDmeFlexRuleExpression, CDmeFlexRuleBase );

public:

	// CDmeOperator
	virtual void Operate();
	virtual void Resolve();

	bool SetExpression( const char *pszExpression );
	const char *GetExpression() const { return m_expr.Get(); }

protected:
	friend class CDmeFlexRules;

	CDmaString m_expr;
	CExpressionCalculator m_calc;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeFlexRuleLocalVar : public CDmeFlexRuleBase
{
	DEFINE_ELEMENT( CDmeFlexRuleLocalVar, CDmeFlexRuleBase );

public:

protected:
	friend class CDmeFlexRules;
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeFlexRules : public CDmeOperator
{
	DEFINE_ELEMENT( CDmeFlexRules, CDmeOperator );

public:
	// From DmeOperator
	virtual void GetInputAttributes ( CUtlVector< CDmAttribute * > &attrs );
	virtual void GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs );
	virtual void Operate();
	virtual void Resolve();

	void AddFlexRule( CDmeFlexRuleBase *pDmeFlexRule );
	int GetRuleCount() const { return m_eDeltaStates.Count(); }
	CDmeFlexRuleBase *GetRule( int i ) const { return ( i < 0 || i >= GetRuleCount() ) ? NULL : m_eDeltaStates[i]; }
	void RemoveAllRules();

	void SetTarget( CDmElement *pDmElement );

	int GetDeltaStateIndex( const char *pszDeltaName ) const;
	float GetDeltaStateWeight( const char *pszDeltaName ) const;

protected:
	friend class CDmeFlexRuleBase;
	friend class CDmeFlexRuleExpression;

	// Required to drive this node from a DmeCombinationOperator
	CDmaElementArray< CDmeFlexRuleBase > m_eDeltaStates;
	CDmaArray< Vector2D > m_vDeltaStateWeights;

	CDmaElement< CDmElement > m_eTarget;

	CDmeCombinationOperator *GetComboOp() const;

	CUtlVector< int > m_deltaToTargetMap;
};


#endif // DMEFLEXRULES_H
