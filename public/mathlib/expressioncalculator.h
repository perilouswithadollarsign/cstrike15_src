//====== Copyright c 1996-2008, Valve Corporation, All rights reserved. =======

#ifndef MATHLIB_EXPRESSION_CALCULATOR_H
#define MATHLIB_EXPRESSION_CALCULATOR_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstring.h"
#include "tier1/utlstack.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Calculator Parsing class
// precedence order:
//		unary operators: + - ! func var
//		* / %
//		+ -
//		< > <= >=
//		== !=
//		&&
//		||
//		?:
//-----------------------------------------------------------------------------
class CExpressionCalculator
{
public:
	CExpressionCalculator( const char *expr = NULL ) : m_expr( expr ) {}

	CExpressionCalculator( const CExpressionCalculator& x );
	CExpressionCalculator& operator=( const CExpressionCalculator& x );
	
public:
	void SetExpression( const char *expr ) 
	{
		m_expr = expr;
	}

	void SetVariable( const char *var, float value );
	void SetVariable( int nVariableIndex, float value );
	void ModifyVariable( const char *var, float value );

	int FindVariableIndex( const char *var );

	bool Evaluate( float &value );

	// Builds a list of variable names from the expression
	bool BuildVariableListFromExpression( );

	// Iterate over variables
	int VariableCount();
	const char *VariableName( int nIndex );

private:
	bool ParseExpr		 ( const char *&expr );
	bool ParseConditional( const char *&expr );
	bool ParseOr		 ( const char *&expr );
	bool ParseAnd		 ( const char *&expr );
	bool ParseEquality	 ( const char *&expr );
	bool ParseLessGreater( const char *&expr );
	bool ParseAddSub	 ( const char *&expr );
	bool ParseDivMul	 ( const char *&expr );
	bool ParseUnary		 ( const char *&expr );
	bool ParsePrimary	 ( const char *&expr );
	bool Parse1ArgFunc	 ( const char *&expr );
	bool Parse2ArgFunc	 ( const char *&expr );
	bool Parse3ArgFunc	 ( const char *&expr );
	//	bool Parse4ArgFunc	 ( const char *&expr );
	bool Parse5ArgFunc	 ( const char *&expr );

	CUtlString m_expr;
	CUtlVector< CUtlString > m_varNames;
	CUtlVector<float> m_varValues;
	CUtlStack<float> m_stack;
	bool m_bIsBuildingArgumentList;
};

// simple warppers for using cExpressionCalculator
float EvaluateExpression( char const *pExprString, float flValueToReturnIfFailure );


#endif // MATHLIB_EXPRESSION_CALCULATOR_H
