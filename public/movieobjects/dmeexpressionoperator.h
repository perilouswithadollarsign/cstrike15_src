//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// The expression operator class - scalar math calculator
// for a good list of operators and simple functions, see:
//   \\fileserver\user\MarcS\boxweb\aliveDistLite\v4.2.0\doc\alive\functions.txt
// (although we'll want to implement elerp as the standard 3x^2 - 2x^3 with rescale)
//
//=============================================================================

#ifndef DMEEXPRESSIONOPERATOR_H
#define DMEEXPRESSIONOPERATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmeoperator.h"
#include "mathlib/expressioncalculator.h"


//-----------------------------------------------------------------------------
// An operator which computes the value of expressions 
//-----------------------------------------------------------------------------
class CDmeExpressionOperator : public CDmeOperator
{
	DEFINE_ELEMENT( CDmeExpressionOperator, CDmeOperator );

public:
	virtual void Operate();

	virtual void GetInputAttributes ( CUtlVector< CDmAttribute * > &attrs );
	virtual void GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs );

	void		SetSpewResult( bool state );

protected:
	bool Parse( const char *expr );

	bool IsInputAttribute( CDmAttribute *pAttribute );

	CExpressionCalculator m_calc;

	CDmaVar< float > m_result;
	CDmaString    m_expr;
	CDmaVar< bool >  m_bSpewResult;
};


#endif // DMEEXPRESSIONOPERATOR_H
