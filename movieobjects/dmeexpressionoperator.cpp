//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// The expression operator class - scalar math calculator
// for a good list of operators and simple functions, see:
//   \\fileserver\user\MarcS\boxweb\aliveDistLite\v4.2.0\doc\alive\functions.txt
// (although we'll want to implement elerp as the standard 3x^2 - 2x^3 with rescale)
//
//=============================================================================
#include "movieobjects/dmeexpressionoperator.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmattribute.h"
#include "mathlib/noise.h"
#include "mathlib/vector.h"
#include <ctype.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


void TestCalculator( const char *expr, float answer )
{
	CExpressionCalculator calc( expr );
	float result = 0.0f;

#ifdef _DEBUG
	bool success =
#endif
		calc.Evaluate( result );
	Assert( success && ( result == answer ) );
}

void TestCalculator( const char *expr, float answer, const char *var, float value )
{
	CExpressionCalculator calc( expr );
	calc.SetVariable( var, value );
	float result = 0.0f;

#ifdef _DEBUG
	bool success =
#endif
		calc.Evaluate( result );
	Assert( success && ( result == answer ) );
}

void TestCalculator()
{
//	TestCalculator( "-1", 1 );
	TestCalculator( "2 * 3 + 4", 10 );
	TestCalculator( "2 + 3 * 4", 14 );
	TestCalculator( "2 * 3 * 4", 24 );
	TestCalculator( "2 * -3 + 4", -2 );
	TestCalculator( "12.0 / 2.0", 6 );
	TestCalculator( "(2*3)+4", 10 );
	TestCalculator( "( 1 + 2 ) / (1+2)", 1 );
	TestCalculator( "(((5)))", 5 );
	TestCalculator( "--5", 5 );
	TestCalculator( "3.5 % 2", 1.5 );
	TestCalculator( "1e-2", 0.01 );
	TestCalculator( "9 == ( 3 * ( 1 + 2 ) )", 1 );
	TestCalculator( "9 != ( 3 * ( 1 + 2 ) )", 0 );
	TestCalculator( "9 <= ( 3 * ( 1 + 2 ) )", 1 );
	TestCalculator( "9 < ( 3 * ( 1 + 2 ) )", 0 );
	TestCalculator( "9 < 3", 0 );
	TestCalculator( "10 >= 5", 1 );
//	TestCalculator( "9 < ( 3 * ( 2 + 2 ) )", 0 );
	TestCalculator( "x + 1", 5, "x", 4 );
	TestCalculator( "pi - 3.14159", 0, "pi", 3.14159 );
//	TestCalculator( "pi / 2", 0, "pi", 3.14159 );
	TestCalculator( "abs(-10)", 10 );
	TestCalculator( "sqr(-5)", 25 );
	TestCalculator( "sqrt(9)", 3 );
//	TestCalculator( "sqrt(-9)", -3 );
	TestCalculator( "pow(2,3)", 8 );
	TestCalculator( "min(abs(-4),2+3/2)", 3.5 );
	TestCalculator( "round(0.5)", 1 );
	TestCalculator( "round(0.49)", 0 );
	TestCalculator( "round(-0.5)", 0 );
	TestCalculator( "round(-0.51)", -1 );
	TestCalculator( "inrange( 5, -8, 10 )", 1 );
	TestCalculator( "inrange( 5, 5, 10 )", 1 );
	TestCalculator( "inrange( 5, 6, 10 )", 0 );
	TestCalculator( "elerp( 1/4, 0, 1 )", 3/16.0f - 1/32.0f );
	TestCalculator( "rescale( 0.5, -1, 1, 0, 100 )", 75 );
	TestCalculator( "1 > 2 ? 6 : 9", 9 );
	TestCalculator( "1 ? 1 ? 2 : 4 : 1 ? 6 : 8", 2 );
	TestCalculator( "0 ? 1 ? 2 : 4 : 1 ? 6 : 8", 6 );
	TestCalculator( "noise( 0.123, 4.56, 78.9 )", ImprovedPerlinNoise( Vector( 0.123, 4.56, 78.9 ) ) );
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeExpressionOperator, CDmeExpressionOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeExpressionOperator::OnConstruction()
{
	m_result.Init( this, "result" );
	m_expr.Init( this, "expr" );
	m_bSpewResult.Init( this, "spewresult" );

#ifdef _DEBUG
	TestCalculator();
#endif _DEBUG
}

void CDmeExpressionOperator::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDmeExpressionOperator::IsInputAttribute( CDmAttribute *pAttribute )
{
	const char *pName = pAttribute->GetName( );
#if 0 // skip this test, since none of these are float attributes, but leave the code as a reminder
	if ( Q_strcmp( pName, "name" ) == 0 )
		return false;

	if ( Q_strcmp( pName, "expr" ) == 0 )
		return false;
#endif

	if ( Q_strcmp( pName, "result" ) == 0 )
		return false;

	if ( pAttribute->GetType() != AT_FLOAT )
		return false;

	return true;
}

void CDmeExpressionOperator::Operate()
{
	m_calc.SetExpression( m_expr.Get() );

	for ( CDmAttribute *pAttribute = FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( IsInputAttribute( pAttribute ) )
		{
			const char *pName = pAttribute->GetName( );
			m_calc.SetVariable( pName, pAttribute->GetValue< float >() );
		}
	}

	float oldValue = m_result;

	m_calc.Evaluate( oldValue );

	m_result = oldValue;

	if ( m_bSpewResult )
	{
		Msg( "%s = '%f'\n", GetName(), (float)m_result );
	}
}

void CDmeExpressionOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	for ( CDmAttribute *pAttribute = FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( IsInputAttribute( pAttribute ) )
		{
			attrs.AddToTail( pAttribute );
		}
	}

	attrs.AddToTail( m_expr.GetAttribute() );
}

void CDmeExpressionOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_result.GetAttribute() );
}


void CDmeExpressionOperator::SetSpewResult( bool state )
{
	m_bSpewResult = state;
}
