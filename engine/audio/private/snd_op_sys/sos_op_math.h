//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_MATH_H
#define SOS_OP_MATH_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


enum SOOpCurveType_t
{
	SO_OP_CURVETYPE_NONE = 0,
	SO_OP_CURVETYPE_STEP,
	SO_OP_CURVETYPE_LINEAR,
};

enum SOOpType_t
{
	SO_OP_NONE = 0,
	SO_OP_SET,
	SO_OP_ADD,
	SO_OP_SUB,
	SO_OP_MULT,
	SO_OP_DIV,
	SO_OP_MOD,
	SO_OP_MAX,
	SO_OP_MIN,
	SO_OP_INV,
	SO_OP_GT,
	SO_OP_LT,
	SO_OP_GTOE,
	SO_OP_LTOE,
	SO_OP_EQ,
	SO_OP_NOT_EQ,
	SO_OP_INV_SCALE,
	SO_OP_POW


};

enum SOFunc1Type_t
{
	SO_FUNC1_NONE = 0,
	SO_FUNC1_SIN,
	SO_FUNC1_ASIN,
	SO_FUNC1_COS,
	SO_FUNC1_ACOS,
	SO_FUNC1_TAN,
	SO_FUNC1_ATAN,
	SO_FUNC1_SINH,
	SO_FUNC1_ASINH,
	SO_FUNC1_COSH,
	SO_FUNC1_ACOSH,
	SO_FUNC1_TANH,
	SO_FUNC1_ATANH,
	SO_FUNC1_EXP,
	SO_FUNC1_EXPM1,
	SO_FUNC1_EXP2,
	SO_FUNC1_LOG,
	SO_FUNC1_LOG2,
	SO_FUNC1_LOG1P,
	SO_FUNC1_LOG10,
	SO_FUNC1_LOGB,
	SO_FUNC1_FABS,
	SO_FUNC1_SQRT,
	SO_FUNC1_ERF,
	SO_FUNC1_ERFC,
	SO_FUNC1_GAMMA,
	SO_FUNC1_LGAMMA,
	SO_FUNC1_CEIL,
	SO_FUNC1_FLOOR,
	SO_FUNC1_RINT,
	SO_FUNC1_NEARBYINT,
	SO_FUNC1_RINTOL,
	SO_FUNC1_ROUND,
	SO_FUNC1_ROUNDTOL,
	SO_FUNC1_TRUNC,
};

//-----------------------------------------------------------------------------
// simple operator for single argument functions
//-----------------------------------------------------------------------------
struct CSosOperatorFunc1_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput1, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	SOFunc1Type_t m_funcType;
	bool m_bNormTrig;

};

class CSosOperatorFunc1 : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorFunc1 )

};
//-----------------------------------------------------------------------------
// simple operator for setting a single scratchpad Expression
//-----------------------------------------------------------------------------
struct CSosOperatorFloat_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput1, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput2, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	SOOpType_t m_opType;

};

class CSosOperatorFloat : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorFloat )

};

//-----------------------------------------------------------------------------
// simple operator for setting a single scratchpad Expression
//-----------------------------------------------------------------------------
struct CSosOperatorFloatAccumulate12_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput1, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput2, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput3, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput4, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput5, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput6, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput7, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput8, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput9, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput10, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput11, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput12, SO_SINGLE )

	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	SOOpType_t m_opType;

};

class CSosOperatorFloatAccumulate12 : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorFloatAccumulate12 )
};

//-----------------------------------------------------------------------------
// simple operator for setting a single position
//-----------------------------------------------------------------------------
struct CSosOperatorVec3_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput1, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInput2, SO_VEC3 )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_VEC3 )
	SOOpType_t m_opType;

};

class CSosOperatorVec3 : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorVec3 )
};

//-----------------------------------------------------------------------------
// simple operator for settting a single speaker scratchpad expression
//-----------------------------------------------------------------------------
struct CSosOperatorSpeakers_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput1, SO_SPEAKERS )
	SOS_INPUT_FLOAT( m_flInput2, SO_SPEAKERS )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SPEAKERS )
	SOOpType_t m_opType;

};

class CSosOperatorSpeakers : public CSosOperator
{

	SOS_HEADER_DESC( CSosOperatorSpeakers )

};

//-----------------------------------------------------------------------------
// distance between 2 positions
//-----------------------------------------------------------------------------
struct CSosOperatorSourceDistance_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputPos, SO_VEC3 )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	bool m_b2D;
	bool m_bForceNotPlayerSound;
};

class CSosOperatorSourceDistance : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorSourceDistance )

};

//-----------------------------------------------------------------------------
// RemapValue
//-----------------------------------------------------------------------------
struct CSosOperatorRemapValue_t : CSosOperator_t
{
	SOS_INPUT_FLOAT(m_flInputMin, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputMax, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputMapMin, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputMapMax, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInput, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )

	bool m_bClampRange;
	bool m_bDefaultMax;
};

class CSosOperatorRemapValue : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorRemapValue )
};

//-----------------------------------------------------------------------------
// Curve4
//-----------------------------------------------------------------------------
struct CSosOperatorCurve4_t : CSosOperator_t
{
	SOS_INPUT_FLOAT(m_flInputX1, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputY1, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputX2, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputY2, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputX3, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputY3, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputX4, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputY4, SO_SINGLE )

	SOS_INPUT_FLOAT(m_flInput, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )

	SOOpCurveType_t m_nCurveType;
};

class CSosOperatorCurve4 : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorCurve4 )
};

//-----------------------------------------------------------------------------
// Facing
//-----------------------------------------------------------------------------
struct CSosOperatorFacing_t : CSosOperator_t
{
	SOS_INPUT_FLOAT(m_flInputAngles, SO_VEC3 )
		SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
};

class CSosOperatorFacing : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorFacing )
};

//-----------------------------------------------------------------------------
// Facing
//-----------------------------------------------------------------------------
struct CSosOperatorRandom_t : CSosOperator_t
{
	SOS_INPUT_FLOAT(m_flInputMin, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputMax, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputSeed, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	bool m_bRoundToInt;
};

class CSosOperatorRandom : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorRandom )
};


//-----------------------------------------------------------------------------
// Logic Switch
//-----------------------------------------------------------------------------
struct CSosOperatorLogicSwitch_t : CSosOperator_t
{
	SOS_INPUT_FLOAT(m_flInput1, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInput2, SO_SINGLE )
	SOS_INPUT_FLOAT(m_flInputSwitch, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
};

class CSosOperatorLogicSwitch : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorLogicSwitch )
};

#endif // SOS_OP_MATH_H
