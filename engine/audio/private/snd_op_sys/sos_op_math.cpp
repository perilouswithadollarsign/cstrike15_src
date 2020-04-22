//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"
#include "math.h"
#include "sos_op.h"
#include "sos_op_math.h"


#include "snd_dma.h"
#include "../../cl_splitscreen.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// ------------------------------------------------------------------------
// GOOD OLD MACROS
#define SOS_DtoR 0.01745329251994329500
#define RtoD 57.29577951308232300000

#define SOS_DEG2RAD(d) ((float)(d) * DtoR)
#define SOS_RAD2DEG(r) ((float)(r) * RtoD) 

#define SOS_RADIANS(deg) ((deg)*DtoR)
#define SOS_DEGREES(rad) ((rad)*RtoD)

//-----------------------------------------------------------------------------

#define SOS_MIN(a,b)   (((a) < (b)) ? (a) : (b))
#define SOS_MAX(a,b)   (((a) > (b)) ? (a) : (b))
#define SOS_ABS(a)     (((a) < 0) ? -(a) : (a))
#define SOS_FLOOR(a)   ((a) > 0 ? (int)(a) : -(int)(-a))
#define SOS_CEILING(a) ((a)==(int)(a) ? (a) : (a)>0 ? 1+(int)(a) : -(1+(int)(-a)))
#define SOS_ROUND(a)   ((a)>0 ? (int)(a+0.5) : -(int)(0.5-a))
#define SOS_SGN(a)     (((a)<0) ? -1 : 1)
#define SOS_SQR(a)     ((a)*(a))
#define SOS_MOD(a,b)   (a)%(b)

// ----------------------------------------------------------------------------

#define SOS_RAMP(value,a,b) (((a) - (float)(value)) / ((a) - (b)))
#define SOS_LERP(factor,a,b) ((a) + (((b) - (a)) * (factor)))
#define SOS_RESCALE(X,Xa,Xb,Ya,Yb) SOS_LERP(SOS_RAMP(X,Xa,Xb),Ya,Yb)

#define SOS_INRANGE(x,a,b) (((x) >= SOS_MIN(a,b)) && ((x) <= SOS_MAX(a,b)))
#define SOS_CLAMP(x,a,b) ((x)<SOS_MIN(a,b)?SOS_MIN(a,b):(x)>MAX(a,b)?SOS_MAX(a,b):(x))

#define SOS_CRAMP(value,a,b) SOS_CLAMP(SOS_RAMP(value,a,b),0,1)
#define SOS_CLERP(factor,a,b) SOS_CLAMP(SOS_LERP(factor,a,b),a,b)
#define SOS_CRESCALE(X,Xa,Xb,Ya,Yb) SOS_CLAMP(SOS_RESCALE(X,Xa,Xb,Ya,Yb),Ya,Yb)

#define SOS_SIND(deg) sin(SOS_RADIANS(deg))
#define SOS_COSD(deg) cos(SOS_RADIANS(deg))
#define SOS_TAND(deg) tan(SOS_RADIANS(deg))
#define SOS_ATAN2(a,b) atan2(a,b)

#define SOS_UNITSINUSOID(x) SOS_LERP(SOS_SIND(SOS_CLERP(x,-90,90)),0.5,1.0)
#define SOS_ELERP(factor,a,b) SOS_LERP(SOS_UNITSINUSOID(factor),a,b)

//-----------------------------------------------------------


extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


SOFunc1Type_t S_GetFunc1Type( const char *pValueString )
{
	if ( !V_strcasecmp( pValueString, "none" ) )
	{
		return SO_FUNC1_NONE;
	}
	else if ( !V_strcasecmp( pValueString, "sin" ) )
	{
		return SO_FUNC1_SIN;
	}
	else if ( !V_strcasecmp( pValueString, "asin" ) )
	{
		return SO_FUNC1_ASIN;
	}
	else if ( !V_strcasecmp( pValueString, "cos" ) )
	{
		return SO_FUNC1_COS;
	}
	else if ( !V_strcasecmp( pValueString, "acos" ) )
	{
		return SO_FUNC1_ACOS;
	}
	else if ( !V_strcasecmp( pValueString, "tan" ) )
	{
		return SO_FUNC1_TAN;
	}
	else if ( !V_strcasecmp( pValueString, "atan" ) )
	{
		return SO_FUNC1_ATAN;
	}
	else if ( !V_strcasecmp( pValueString, "sinh" ) )
	{
		return SO_FUNC1_SINH;
	}
	else if ( !V_strcasecmp( pValueString, "asinh" ) )
	{
		return SO_FUNC1_ASINH;
	}
	else if ( !V_strcasecmp( pValueString, "cosh" ) )
	{
		return SO_FUNC1_COSH;
	}
	else if ( !V_strcasecmp( pValueString, "acosh" ) )
	{
		return SO_FUNC1_ACOSH;
	}
	else if ( !V_strcasecmp( pValueString, "tanh" ) )
	{
		return SO_FUNC1_TANH;
	}
	else if ( !V_strcasecmp( pValueString, "atanh" ) )
	{
		return SO_FUNC1_ATANH;
	}
	else if ( !V_strcasecmp( pValueString, "exp" ) )
	{
		return SO_FUNC1_EXP;
	}
	else if ( !V_strcasecmp( pValueString, "expm1" ) )
	{
		return SO_FUNC1_EXPM1;
	}
	else if ( !V_strcasecmp( pValueString, "exp2" ) )
	{
		return SO_FUNC1_EXP2;
	}
	else if ( !V_strcasecmp( pValueString, "log" ) )
	{
		return SO_FUNC1_LOG;
	}
	else if ( !V_strcasecmp( pValueString, "log2" ) )
	{
		return SO_FUNC1_LOG2;
	}
	else if ( !V_strcasecmp( pValueString, "log1p" ) )
	{
		return SO_FUNC1_LOG1P;
	}
	else if ( !V_strcasecmp( pValueString, "log10" ) )
	{
		return SO_FUNC1_LOG10;
	}
	else if ( !V_strcasecmp( pValueString, "logb" ) )
	{
		return SO_FUNC1_LOGB;
	}
	else if ( !V_strcasecmp( pValueString, "fabs" ) )
	{
		return SO_FUNC1_FABS;
	}
	else if ( !V_strcasecmp( pValueString, "sqrt" ) )
	{
		return SO_FUNC1_SQRT;
	}
	else if ( !V_strcasecmp( pValueString, "erf" ) )
	{
		return SO_FUNC1_ERF;
	}
	else if ( !V_strcasecmp( pValueString, "erfc" ) )
	{
		return SO_FUNC1_ERFC;
	}
	else if ( !V_strcasecmp( pValueString, "gamma" ) )
	{
		return SO_FUNC1_GAMMA;
	}
	else if ( !V_strcasecmp( pValueString, "lgamma" ) )
	{
		return SO_FUNC1_LGAMMA;
	}
	else if ( !V_strcasecmp( pValueString, "ceil" ) )
	{
		return SO_FUNC1_CEIL;
	}
	else if ( !V_strcasecmp( pValueString, "floor" ) )
	{
		return SO_FUNC1_FLOOR;
	}
	else if ( !V_strcasecmp( pValueString, "rint" ) )
	{
		return SO_FUNC1_RINT;
	}
	else if ( !V_strcasecmp( pValueString, "nearbyint" ) )
	{
		return SO_FUNC1_NEARBYINT;
	}
	else if ( !V_strcasecmp( pValueString, "rintol" ) )
	{
		return SO_FUNC1_RINTOL;
	}
	else if ( !V_strcasecmp( pValueString, "round" ) )
	{
		return SO_FUNC1_ROUND;
	}
	else if ( !V_strcasecmp( pValueString, "roundtol" ) )
	{
		return SO_FUNC1_ROUNDTOL;
	}
	else if ( !V_strcasecmp( pValueString, "trunc" ) )
	{
		return SO_FUNC1_TRUNC;
	}
	else
	{
		return  SO_FUNC1_NONE;
	}
}

void S_PrintFunc1Type( SOFunc1Type_t nType, int nLevel )
{
	const char *pType;
	switch ( nType )
	{
	case SO_FUNC1_NONE:
		pType = "none";
		break;	
	case SO_FUNC1_SIN:
		pType = "sin";
		break;
	case SO_FUNC1_ASIN:
		pType = "asin";
		break;
	case SO_FUNC1_COS:
		pType = "cos";
		break;
	case SO_FUNC1_ACOS:
		pType = "acos";
		break;
	case SO_FUNC1_TAN:
		pType = "tan";
		break;
	case SO_FUNC1_ATAN:
		pType = "atan";
		break;
	case SO_FUNC1_SINH:
		pType = "sinh";
		break;
	case SO_FUNC1_ASINH:
		pType = "asinh";
		break;
	case SO_FUNC1_COSH:
		pType = "cosh";
		break;
	case SO_FUNC1_ACOSH:
		pType = "acosh";
		break;
	case SO_FUNC1_TANH:
		pType = "tanh";
		break;
	case SO_FUNC1_ATANH:
		pType = "atanh";
		break;
	case SO_FUNC1_EXP:
		pType = "exp";
		break;
	case SO_FUNC1_EXPM1:
		pType = "expm1";
		break;
	case SO_FUNC1_EXP2:
		pType = "exp2";
		break;
	case SO_FUNC1_LOG:
		pType = "log";
		break;
	case SO_FUNC1_LOG2:
		pType = "log2";
		break;
	case SO_FUNC1_LOG1P:
		pType = "log1p";
		break;
	case SO_FUNC1_LOG10:
		pType = "log10";
		break;
	case SO_FUNC1_LOGB:
		pType = "logb";
		break;
	case SO_FUNC1_FABS:
		pType = "fabs";
		break;
	case SO_FUNC1_SQRT:
		pType = "sqrt";
		break;
	case SO_FUNC1_ERF:
		pType = "erf";
		break;
	case SO_FUNC1_ERFC:
		pType = "erfc";
		break;
	case SO_FUNC1_GAMMA:
		pType = "gamma";
		break;
	case SO_FUNC1_LGAMMA:
		pType = "lgamma";
		break;
	case SO_FUNC1_CEIL:
		pType = "ceil";
		break;
	case SO_FUNC1_FLOOR:
		pType = "floor";
		break;
	case SO_FUNC1_RINT:
		pType = "rint";
		break;
	case SO_FUNC1_NEARBYINT:
		pType = "nearbyint";
		break;
	case SO_FUNC1_RINTOL:
		pType = "rintol";
		break;
	case SO_FUNC1_ROUND:
		pType = "round";
		break;
	case SO_FUNC1_ROUNDTOL:
		pType = "roundtol";
		break;
	case SO_FUNC1_TRUNC:
		pType = "trunc";
		break;
	default:
		pType = "none";
		break;

	}
	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, OpColor, "%*sFunction: %s\n", nLevel, "    ", pType );
}

SOOpType_t S_GetExpressionType( const char *pValueString )
{
	if ( !V_strcasecmp( pValueString, "none" ) )
	{
		return SO_OP_NONE;
	}
	else if ( !V_strcasecmp( pValueString, "set" ) )
	{
		return SO_OP_SET;
	}
	else if ( !V_strcasecmp( pValueString, "add" ) )
	{
		return SO_OP_ADD;
	}
	else if ( !V_strcasecmp( pValueString, "sub" ) )
	{
		return SO_OP_SUB;
	}
	else if ( !V_strcasecmp( pValueString, "mult" ) )
	{
		return SO_OP_MULT;
	}
	else if ( !V_strcasecmp( pValueString, "div" ) )
	{
		return SO_OP_DIV;
	}
	else if ( !V_strcasecmp( pValueString, "mod" ) )
	{
		return SO_OP_MOD;
	}
	else if ( !V_strcasecmp( pValueString, "max" ) )
	{
		return SO_OP_MAX;
	}
	else if ( !V_strcasecmp( pValueString, "min" ) )
	{
		return SO_OP_MIN;
	}
	else if ( !V_strcasecmp( pValueString, "invert" ) )
	{
		return SO_OP_INV;
	}
	else if ( !V_strcasecmp( pValueString, "greater_than" ) )
	{
		return SO_OP_GT;
	}
	else if ( !V_strcasecmp( pValueString, "less_than" ) )
	{
		return SO_OP_LT;
	}
	else if ( !V_strcasecmp( pValueString, "greater_than_or_equal" ) )
	{
		return SO_OP_GTOE;
	}
	else if ( !V_strcasecmp( pValueString, "less_than_or_equal" ) )
	{
		return SO_OP_LTOE;
	}
	else if ( !V_strcasecmp( pValueString, "equals" ) )
	{
		return SO_OP_EQ;
	}
	else if ( !V_strcasecmp( pValueString, "not_equal" ) )
	{
		return SO_OP_NOT_EQ;
	}
	else if ( !V_strcasecmp( pValueString, "invert_scale" ) )
	{
		return SO_OP_INV_SCALE;
	}
	else if ( !V_strcasecmp( pValueString, "pow" ) )
	{
		return SO_OP_POW;
	}
	else
	{
		return  SO_OP_NONE;
	}
}

void S_PrintOpType( SOOpType_t nType, int nLevel )
{
	const char *pType;
	switch ( nType )
	{
	case SO_OP_NONE:
		pType = "none";
		break;	
	case SO_OP_SET:
		pType = "set";
		break;
	case SO_OP_ADD:
		pType = "add";
		break;
	case SO_OP_SUB:
		pType = "sub";
		break;
	case SO_OP_MULT:
		pType = "mult";
		break;
	case SO_OP_DIV:
		pType = "div";
		break;
	case SO_OP_MOD:
		pType = "mod";
		break;
	case SO_OP_MAX:
		pType = "max";
		break;
	case SO_OP_MIN:
		pType = "min";
		break;
	case SO_OP_INV:
		pType = "invert";
		break;
	case SO_OP_GT:
		pType = "greater_than";
		break;
	case SO_OP_LT:
		pType = "less_than";
		break;
	case SO_OP_GTOE:
		pType = "greater_than_or_equal";
		break;
	case SO_OP_LTOE:
		pType = "less_than_or_equal";
		break;
	case SO_OP_EQ:
		pType = "equals";
		break;
	case SO_OP_NOT_EQ:
		pType = "not_equal";
		break;
	case SO_OP_INV_SCALE:
		pType = "invert_scale";
		break;
	case SO_OP_POW:
		pType = "pow";
		break;
	default:
		pType = "none";
		break;

	}
	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, OpColor, "%*sOperation: %s\n", nLevel, "    ", pType );
}

//-----------------------------------------------------------------------------
// CSosOperatorFunc1
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorFunc1, "math_func1" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorFunc1, m_flOutput, SO_SINGLE, "output" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFunc1, m_flInput1, SO_SINGLE, "input1" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorFunc1, "math_func1"  )

void CSosOperatorFunc1::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorFunc1_t *pStructMem = (CSosOperatorFunc1_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInput1, SO_SINGLE, 0.0 )

		// do nothing by default
	pStructMem->m_funcType = SO_FUNC1_NONE;
	pStructMem->m_bNormTrig = false;
}

void CSosOperatorFunc1::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorFunc1_t *pStructMem = (CSosOperatorFunc1_t *)pVoidMem;

	float flResult = 0.0;

	switch ( pStructMem->m_funcType )
	{
	case SO_OP_NONE:
		break;
	case SO_FUNC1_SIN:
		flResult = sin( pStructMem->m_flInput1[0] );
		break;
	case SO_FUNC1_ASIN:
		flResult = asin( pStructMem->m_flInput1[0] );
		break;
	case SO_FUNC1_COS:
		flResult = cos( pStructMem->m_flInput1[0] );
		break;
	case SO_FUNC1_ACOS:
		flResult = acos( pStructMem->m_flInput1[0] );
		break;
	case SO_FUNC1_TAN:
		flResult = tan( pStructMem->m_flInput1[0] );
		break;
	case SO_FUNC1_ATAN:
		flResult = atan( pStructMem->m_flInput1[0] );
		break;
	case SO_FUNC1_SINH:
		flResult = sinh( pStructMem->m_flInput1[0] );
		break;
// 	case SO_FUNC1_ASINH:
// 		flResult = asinh( pStructMem->m_flInput1[0] );
// 		break;
	case SO_FUNC1_COSH:
		flResult = cosh( pStructMem->m_flInput1[0] );
		break;
// 	case SO_FUNC1_ACOSH:
// 		flResult = acosh( pStructMem->m_flInput1[0] );
// 		break;
	case SO_FUNC1_TANH:
		flResult = tanh( pStructMem->m_flInput1[0] );
		break;
// 	case SO_FUNC1_ATANH:
// 		flResult = atanh( pStructMem->m_flInput1[0] );
// 		break;
	case SO_FUNC1_EXP:
		flResult = exp( pStructMem->m_flInput1[0] );
		break;
// 	case SO_FUNC1_EXPM1:
// 		flResult = expm1( pStructMem->m_flInput1[0] );
// 		break;
// 	case SO_FUNC1_EXP2:
// 		flResult = exp2( pStructMem->m_flInput1[0] );
// 		break;
	case SO_FUNC1_LOG:
		flResult = log( pStructMem->m_flInput1[0] );
		break;
// 	case SO_FUNC1_LOG2:
// 		flResult = log2( pStructMem->m_flInput1[0] );
// 		break;
// 	case SO_FUNC1_LOG1P:
// 		flResult = log1p( pStructMem->m_flInput1[0] );
// 		break;
	case SO_FUNC1_LOG10:
		flResult = log10( pStructMem->m_flInput1[0] );
		break;
// 	case SO_FUNC1_LOGB:
// 		flResult = logb( pStructMem->m_flInput1[0] );
// 		break;
	case SO_FUNC1_FABS:
		flResult = fabs( pStructMem->m_flInput1[0] );
		break;
	case SO_FUNC1_SQRT:
		flResult = sqrt( pStructMem->m_flInput1[0] );
		break;
// 	case SO_FUNC1_ERF:
// 		flResult = erf( pStructMem->m_flInput1[0] );
// 		break;
// 	case SO_FUNC1_ERFC:
// 		flResult = erfc( pStructMem->m_flInput1[0] );
// 		break;
// 	case SO_FUNC1_GAMMA:
// 		flResult = gamma( pStructMem->m_flInput1[0] );
// 		break;
// 	case SO_FUNC1_LGAMMA:
// 		flResult = lgamma( pStructMem->m_flInput1[0] );
// 		break;
	case SO_FUNC1_CEIL:
		flResult = ceil( pStructMem->m_flInput1[0] );
		break;
	case SO_FUNC1_FLOOR:
		flResult = floor( pStructMem->m_flInput1[0] );
		break;
// 	case SO_FUNC1_RINT:
// 		flResult = rint( pStructMem->m_flInput1[0] );
// 		break;
// 	case SO_FUNC1_NEARBYINT:
// 		flResult = nearbyint( pStructMem->m_flInput1[0] );
// 		break;
// 	case SO_FUNC1_RINTOL:
// 		flResult = rintol( pStructMem->m_flInput1[0] );
// 		break;
	case SO_FUNC1_ROUND:
		flResult = SOS_ROUND( pStructMem->m_flInput1[0] );
		break;
// 	case SO_FUNC1_ROUNDTOL:
// 		flResult = roundtol( pStructMem->m_flInput1[0] );
// 		break;
// 	case SO_FUNC1_TRUNC:
// 		flResult = trunc( pStructMem->m_flInput1[0] );
// 		break;
	default:
		break;
	}
	if( pStructMem->m_bNormTrig )
	{
		flResult = ( flResult + 1.0 ) * 0.5;
	}

	pStructMem->m_flOutput[0] = flResult;

}
void CSosOperatorFunc1::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorFunc1_t *pStructMem = (CSosOperatorFunc1_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	S_PrintFunc1Type( pStructMem->m_funcType, nLevel );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*snormalize_trig: %s\n", nLevel, "    ", pStructMem->m_bNormTrig ? "true": "false" );	


}
void CSosOperatorFunc1::OpHelp( ) const
{

}
void CSosOperatorFunc1::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorFunc1_t *pStructMem = (CSosOperatorFunc1_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "function" ) )
				{
					pStructMem->m_funcType = S_GetFunc1Type( pValueString );
				}
				else if ( !V_strcasecmp( pParamString, "normalize_trig" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bNormTrig = true;
					}
					else
					{
						pStructMem->m_bNormTrig = false;
					}
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}


//-----------------------------------------------------------------------------
// CSosOperatorFloat
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorFloat, "math_float" )
	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorFloat, m_flOutput, SO_SINGLE, "output" );
	SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloat, m_flInput1, SO_SINGLE, "input1" )
	SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloat, m_flInput2, SO_SINGLE, "input2")
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorFloat, "math_float"  )

void CSosOperatorFloat::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorFloat_t *pStructMem = (CSosOperatorFloat_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInput1, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInput2, SO_SINGLE, 0.0 )

	// do nothing by default
	pStructMem->m_opType = SO_OP_MULT;
}

void CSosOperatorFloat::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorFloat_t *pStructMem = (CSosOperatorFloat_t *)pVoidMem;

	float flResult = 0.0;

	switch ( pStructMem->m_opType )
	{
	case SO_OP_NONE:
		break;
	case SO_OP_SET:
		flResult = pStructMem->m_flInput1[0];
		break;
	case SO_OP_ADD:
		flResult = pStructMem->m_flInput1[0] + pStructMem->m_flInput2[0];
		break;
	case SO_OP_SUB:
		flResult = pStructMem->m_flInput1[0] - pStructMem->m_flInput2[0];
		break;
	case SO_OP_MULT:
		flResult = pStructMem->m_flInput1[0] * pStructMem->m_flInput2[0];
		break;
	case SO_OP_DIV:
		if ( pStructMem->m_flInput2[0] > 0.0 )
		{
			 flResult = pStructMem->m_flInput1[0] / pStructMem->m_flInput2[0];
		}
		break;
	case SO_OP_MOD:
		if ( pStructMem->m_flInput2[0] > 0.0 )
		{
			flResult = fmodf( pStructMem->m_flInput1[0], pStructMem->m_flInput2[0] );
		}
		break;
	case SO_OP_MAX:
		flResult = MAX( pStructMem->m_flInput1[0], pStructMem->m_flInput2[0] );
		break;
	case SO_OP_MIN:
		flResult = MIN( pStructMem->m_flInput1[0], pStructMem->m_flInput2[0] );
		break;
	case SO_OP_INV:
		flResult = 1.0 - pStructMem->m_flInput1[0];
		break;
	case SO_OP_GT:
		flResult = pStructMem->m_flInput1[0] > pStructMem->m_flInput2[0] ? 1.0 : 0.0;
		break;
	case SO_OP_LT:
		flResult = pStructMem->m_flInput1[0] < pStructMem->m_flInput2[0] ? 1.0 : 0.0;
		break;
	case SO_OP_GTOE:
		flResult = pStructMem->m_flInput1[0] >= pStructMem->m_flInput2[0] ? 1.0 : 0.0;
		break;
	case SO_OP_LTOE:
		flResult = pStructMem->m_flInput1[0] <= pStructMem->m_flInput2[0] ? 1.0 : 0.0;
		break;
	case SO_OP_EQ:
		flResult = pStructMem->m_flInput1[0] == pStructMem->m_flInput2[0] ? 1.0 : 0.0;
		break;
	case SO_OP_NOT_EQ:
		flResult = pStructMem->m_flInput1[0] != pStructMem->m_flInput2[0] ? 1.0 : 0.0;
		break;
	case SO_OP_INV_SCALE:
		flResult = 1.0 - ( ( 1.0 - pStructMem->m_flInput1[0] ) * pStructMem->m_flInput2[0] );
		break;
	case SO_OP_POW:
		flResult = FastPow( pStructMem->m_flInput1[0], pStructMem->m_flInput2[0] );
		break;
	default:
		break;
	}

	pStructMem->m_flOutput[0] = flResult;

}
void CSosOperatorFloat::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorFloat_t *pStructMem = (CSosOperatorFloat_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	S_PrintOpType( pStructMem->m_opType, nLevel );
}
void CSosOperatorFloat::OpHelp( ) const
{

}
void CSosOperatorFloat::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorFloat_t *pStructMem = (CSosOperatorFloat_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "apply" ) )
				{
					pStructMem->m_opType = S_GetExpressionType( pValueString );
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// CSosOperatorVec3
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------

SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorVec3, "math_vec3" )
	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorVec3, m_flOutput, SO_VEC3, "output" );
	SOS_REGISTER_INPUT_FLOAT( CSosOperatorVec3, m_flInput1, SO_VEC3, "input1" )
	SOS_REGISTER_INPUT_FLOAT( CSosOperatorVec3, m_flInput2, SO_VEC3, "input2")
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorVec3, "math_vec3"  )

void CSosOperatorVec3::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorVec3_t *pStructMem = (CSosOperatorVec3_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInput1, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInput2, SO_VEC3, 0.0 )

	pStructMem->m_opType = SO_OP_MULT;

}

void CSosOperatorVec3::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorVec3_t *pStructMem = (CSosOperatorVec3_t *)pVoidMem;

	for( int i = 0; i < SO_POSITION_ARRAY_SIZE; i++ )
	{
		switch ( pStructMem->m_opType )
		{
		case SO_OP_NONE:
			break;
		case SO_OP_SET:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i];
			break;
		case SO_OP_ADD:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] + pStructMem->m_flInput2[i];
			break;
		case SO_OP_SUB:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] - pStructMem->m_flInput2[i];
			break;
		case SO_OP_MULT:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] * pStructMem->m_flInput2[i];
			break;
		case SO_OP_DIV:
			if ( pStructMem->m_flInput2[i] > 0.0 )
			{
				pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] / pStructMem->m_flInput2[i];
			}
			break;
		case SO_OP_MOD:
			if ( pStructMem->m_flInput2[i] > 0.0 )
			{
				pStructMem->m_flOutput[i] = fmodf( pStructMem->m_flInput1[i], pStructMem->m_flInput2[i] );
			}
			break;
		case SO_OP_MAX:
			pStructMem->m_flOutput[i] = MAX(pStructMem->m_flInput1[i], pStructMem->m_flInput2[i]);
			break;
		case SO_OP_MIN:
			pStructMem->m_flOutput[i] = MIN(pStructMem->m_flInput1[i], pStructMem->m_flInput2[i]);
			break;
		case SO_OP_INV:
			pStructMem->m_flOutput[i] = 1.0 - pStructMem->m_flInput1[i];
			break;
		case SO_OP_GT:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] > pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		case SO_OP_LT:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] < pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		case SO_OP_GTOE:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] >= pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		case SO_OP_LTOE:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] <= pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		case SO_OP_EQ:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] == pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		default:
			break;
		}
	}

}



void CSosOperatorVec3::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorVec3_t *pStructMem = (CSosOperatorVec3_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	S_PrintOpType( pStructMem->m_opType, nLevel );

}
void CSosOperatorVec3::OpHelp( ) const
{

}

void CSosOperatorVec3::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorVec3_t *pStructMem = (CSosOperatorVec3_t *)pVoidMem;

	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "apply" ) )
				{
					pStructMem->m_opType = S_GetExpressionType( pValueString );
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// CSosOperatorSpeakers
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorSpeakers, "math_speakers" )
	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSpeakers, m_flOutput, SO_SPEAKERS, "output" );
	SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpeakers, m_flInput1, SO_SPEAKERS, "input1" )
	SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpeakers, m_flInput2, SO_SPEAKERS, "input2")
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorSpeakers, "math_speakers"  )

void CSosOperatorSpeakers::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorSpeakers_t *pStructMem = (CSosOperatorSpeakers_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SPEAKERS, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInput1, SO_SPEAKERS, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInput2, SO_SPEAKERS, 0.0 )

	pStructMem->m_opType = SO_OP_MULT;

}

void CSosOperatorSpeakers::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorSpeakers_t *pStructMem = (CSosOperatorSpeakers_t *)pVoidMem;

	for( int i = 0; i < SO_MAX_SPEAKERS; i++ )
	{
		switch ( pStructMem->m_opType )
		{
		case SO_OP_NONE:
			break;
		case SO_OP_SET:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i];
			break;
		case SO_OP_ADD:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] + pStructMem->m_flInput2[i];
			break;
		case SO_OP_SUB:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] - pStructMem->m_flInput2[i];
			break;
		case SO_OP_MULT:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] * pStructMem->m_flInput2[i];
			break;
		case SO_OP_DIV:
			if ( pStructMem->m_flInput2[i] > 0.0 )
			{
				pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] / pStructMem->m_flInput2[i];
			}
			break;
		case SO_OP_MOD:
			if ( pStructMem->m_flInput2[i] > 0.0 )
			{
				pStructMem->m_flOutput[i] = fmodf( pStructMem->m_flInput1[i], pStructMem->m_flInput2[i] );
			}
			break;
		case SO_OP_MAX:
			pStructMem->m_flOutput[i] = MAX( pStructMem->m_flInput1[i], pStructMem->m_flInput2[i] );
			break;
		case SO_OP_MIN:
			pStructMem->m_flOutput[i] = MIN( pStructMem->m_flInput1[1], pStructMem->m_flInput2[i] );
			break;
		case SO_OP_INV:
			pStructMem->m_flOutput[i] = 1.0 - pStructMem->m_flInput1[i];
			break;
		case SO_OP_GT:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] > pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		case SO_OP_LT:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] < pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		case SO_OP_GTOE:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] >= pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		case SO_OP_LTOE:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] <= pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		case SO_OP_EQ:
			pStructMem->m_flOutput[i] = pStructMem->m_flInput1[i] == pStructMem->m_flInput2[i] ? 1.0 : 0.0;
			break;
		default:
			break;
		}
	}

}


void CSosOperatorSpeakers::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorSpeakers_t *pStructMem = (CSosOperatorSpeakers_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	S_PrintOpType( pStructMem->m_opType, nLevel );

}

void CSosOperatorSpeakers::OpHelp( ) const
{

}
void CSosOperatorSpeakers::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorSpeakers_t *pStructMem = (CSosOperatorSpeakers_t *)pVoidMem;

	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "apply" ) )
				{
					pStructMem->m_opType = S_GetExpressionType( pValueString );
				}
				else if ( !V_strcasecmp( pParamString, "left_front" ) )
				{
					pStructMem->m_flInput1[IFRONT_LEFT] = RandomInterval( ReadInterval( pValueString ) );
				}
				else if ( !V_strcasecmp( pParamString, "right_front" ) )
				{
					pStructMem->m_flInput1[IFRONT_RIGHT] = RandomInterval( ReadInterval( pValueString ) );
				}
				else if ( !V_strcasecmp( pParamString, "left_rear" ) )
				{
					pStructMem->m_flInput1[IREAR_LEFT] = RandomInterval( ReadInterval( pValueString ) );
				}
				else if ( !V_strcasecmp( pParamString, "right_rear" ) )
				{
					pStructMem->m_flInput1[IREAR_RIGHT] = RandomInterval( ReadInterval( pValueString ) );
				}
				else if ( !V_strcasecmp( pParamString, "center" ) )
				{
					pStructMem->m_flInput1[IFRONT_CENTER] = RandomInterval( ReadInterval( pValueString ) );
				}
				else if ( !V_strcasecmp( pParamString, "lfe" ) )
				{
					pStructMem->m_flInput1[IFRONT_CENTER0] = RandomInterval( ReadInterval( pValueString ) );
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// CSosOperatorFloatAccumulate12
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorFloatAccumulate12, "math_float_accumulate12" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flOutput, SO_SINGLE, "output" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput1, SO_SINGLE, "input1" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput2, SO_SINGLE, "input2")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput3, SO_SINGLE, "input3")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput4, SO_SINGLE, "input4")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput5, SO_SINGLE, "input5")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput6, SO_SINGLE, "input6")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput7, SO_SINGLE, "input7")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput8, SO_SINGLE, "input8")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput9, SO_SINGLE, "input9")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput10, SO_SINGLE, "input10")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput11, SO_SINGLE, "input11")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatAccumulate12, m_flInput12, SO_SINGLE, "input12")
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorFloatAccumulate12, "math_float_accumulate12"  )

void CSosOperatorFloatAccumulate12::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorFloatAccumulate12_t *pStructMem = (CSosOperatorFloatAccumulate12_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput1, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput2, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput3, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput4, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput5, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput6, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput7, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput8, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput9, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput10, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput11, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput12, SO_SINGLE, 1.0 )

	// do nothing by default
	pStructMem->m_opType = SO_OP_MULT;
}

void CSosOperatorFloatAccumulate12::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorFloatAccumulate12_t *pStructMem = (CSosOperatorFloatAccumulate12_t *)pVoidMem;

	float flResult = 0.0;

	switch ( pStructMem->m_opType )
	{
	case SO_OP_NONE:
		break;
	case SO_OP_SET:
		Log_Warning( LOG_SND_OPERATORS, "Error: %s : Math expression type not currently supported in sound operator math_float_accumulate12\n", pStack->GetOperatorName( nOpIndex ) );
		break;
	case SO_OP_ADD:
		flResult = pStructMem->m_flInput1[0] + pStructMem->m_flInput2[0] +
			pStructMem->m_flInput3[0] + pStructMem->m_flInput4[0] +
			pStructMem->m_flInput5[0] + pStructMem->m_flInput6[0] +
			pStructMem->m_flInput7[0] + pStructMem->m_flInput8[0] +
			pStructMem->m_flInput9[0] + pStructMem->m_flInput10[0] +
			pStructMem->m_flInput11[0] + pStructMem->m_flInput12[0];			 
		break;
	case SO_OP_SUB:
		flResult = pStructMem->m_flInput1[0] - pStructMem->m_flInput2[0] -
			pStructMem->m_flInput3[0] - pStructMem->m_flInput4[0] -
			pStructMem->m_flInput5[0] - pStructMem->m_flInput6[0] -
			pStructMem->m_flInput7[0] - pStructMem->m_flInput8[0] -
			pStructMem->m_flInput9[0] - pStructMem->m_flInput10[0] -
			pStructMem->m_flInput11[0] - pStructMem->m_flInput12[0];
		break;
	case SO_OP_MULT:
		flResult = pStructMem->m_flInput1[0] * pStructMem->m_flInput2[0] *
			pStructMem->m_flInput3[0] * pStructMem->m_flInput4[0] *
			pStructMem->m_flInput5[0] * pStructMem->m_flInput6[0] *
			pStructMem->m_flInput7[0] * pStructMem->m_flInput8[0] *
			pStructMem->m_flInput9[0] * pStructMem->m_flInput10[0] *
			pStructMem->m_flInput11[0] * pStructMem->m_flInput12[0];
		break;
	case SO_OP_DIV:
		flResult = pStructMem->m_flInput1[0] / pStructMem->m_flInput2[0] /
			pStructMem->m_flInput3[0] / pStructMem->m_flInput4[0] /
			pStructMem->m_flInput5[0] / pStructMem->m_flInput6[0] /
			pStructMem->m_flInput7[0] / pStructMem->m_flInput8[0] /
			pStructMem->m_flInput9[0] / pStructMem->m_flInput10[0] /
			pStructMem->m_flInput11[0] / pStructMem->m_flInput12[0];
		break;
	case SO_OP_MAX:
		Log_Warning( LOG_SND_OPERATORS, "Error: %s : Math expression type not currently supported in sound operator math_float_accumulate12\n", pStack->GetOperatorName( nOpIndex ) );
		break;
	case SO_OP_MIN:
		Log_Warning( LOG_SND_OPERATORS, "Error: %s : Math expression type not currently supported in sound operator math_float_accumulate12\n", pStack->GetOperatorName( nOpIndex ) );
		break;
	case SO_OP_EQ:
		Log_Warning( LOG_SND_OPERATORS, "Error: %s : Math expression type not currently supported in sound operator math_float_accumulate12\n", pStack->GetOperatorName( nOpIndex ) );
		break;
	case SO_OP_INV_SCALE:
		Log_Warning( LOG_SND_OPERATORS, "Error: %s : Math expression type not currently supported in sound operator math_float_accumulate12\n", pStack->GetOperatorName( nOpIndex ) );
		break;
	default:
		Log_Warning( LOG_SND_OPERATORS, "Error: %s : Math expression type not currently supported in sound operator math_float_accumulate12\n", pStack->GetOperatorName( nOpIndex ) );
		break;
	}

	pStructMem->m_flOutput[0] = flResult;

}
void CSosOperatorFloatAccumulate12::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorFloatAccumulate12_t *pStructMem = (CSosOperatorFloatAccumulate12_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	S_PrintOpType( pStructMem->m_opType, nLevel );
}
void CSosOperatorFloatAccumulate12::OpHelp( ) const
{

}
void CSosOperatorFloatAccumulate12::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorFloatAccumulate12_t *pStructMem = (CSosOperatorFloatAccumulate12_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "apply" ) )
				{
					pStructMem->m_opType = S_GetExpressionType( pValueString );
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// CSosOperatorSourceDistance
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorSourceDistance, "calc_source_distance" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSourceDistance, m_flOutput, SO_SINGLE, "output" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSourceDistance, m_flInputPos, SO_VEC3, "input_position" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorSourceDistance, "calc_source_distance" )

void CSosOperatorSourceDistance::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorSourceDistance_t *pStructMem = (CSosOperatorSourceDistance_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputPos, SO_VEC3, 0.0 )
	pStructMem->m_b2D = false;
	pStructMem->m_bForceNotPlayerSound = false;

}

void CSosOperatorSourceDistance::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorSourceDistance_t *pStructMem = (CSosOperatorSourceDistance_t *)pVoidMem;

	pStructMem->m_flOutput[0] = FLT_MAX;
	// calculate the distance to the nearest ss player
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		Vector vSource;
		if ( pScratchPad->m_bIsPlayerSound && !pStructMem->m_bForceNotPlayerSound )
		{
			// Hack for now
			// get 2d forward direction vector, ignoring pitch angle
			Vector listener_forward2d;

			ConvertListenerVectorTo2D( &listener_forward2d, &pScratchPad->m_vPlayerRight[ hh ] );

			// player sounds originate from 1' in front of player, 2d
			VectorMultiply(listener_forward2d, 12.0, vSource );
		}
		else
		{
			Vector vPosition;
			vPosition[0] = pStructMem->m_flInputPos[0];
			vPosition[1] = pStructMem->m_flInputPos[1];
			vPosition[2] = pStructMem->m_flInputPos[2];

			VectorSubtract( vPosition, pScratchPad->m_vPlayerOrigin[ hh ], vSource );
		}

		// normalize source_vec and get distance from listener to source

		float checkDist = 0.0;

		if ( pStructMem->m_b2D )
		{
			checkDist = vSource.Length2D();
		}
		else
		{
			checkDist = VectorNormalize( vSource );
		}

		if ( checkDist < pStructMem->m_flOutput[0] )
		{
			pStructMem->m_flOutput[0] = checkDist;
		}
	}
}

void CSosOperatorSourceDistance::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorSourceDistance_t *pStructMem = (CSosOperatorSourceDistance_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorSourceDistance::OpHelp( ) const
{
}

void CSosOperatorSourceDistance::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorSourceDistance_t *pStructMem = (CSosOperatorSourceDistance_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "in2D" ) )
				{
					if ( !V_strcasecmp( pValueString, "false" ) )
					{
						pStructMem->m_b2D = false;
					}
					else if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_b2D = true;
					}
				}
				else if ( !V_strcasecmp( pParamString, "force_not_player_sound" ) )
				{
					if ( !V_strcasecmp( pValueString, "false" ) )
					{
						pStructMem->m_bForceNotPlayerSound = false;
					}
					else if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bForceNotPlayerSound = true;
					}
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// CSosOperatorFacing
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorFacing, "calc_angles_facing" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFacing, m_flInputAngles, SO_VEC3, "input_angles" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorFacing, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorFacing, "calc_angles_facing"  )

void CSosOperatorFacing::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorFacing_t *pStructMem = (CSosOperatorFacing_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInputAngles, SO_VEC3, 0.0 )
		SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )

}

void CSosOperatorFacing::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorFacing_t *pStructMem = (CSosOperatorFacing_t *)pVoidMem;

	if( !pChannel )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
		return;
	}
	QAngle vAngles;
	vAngles[0] = pStructMem->m_flInputAngles[0];
	vAngles[1] = pStructMem->m_flInputAngles[1];
	vAngles[2] = pStructMem->m_flInputAngles[2];
	float flFacing =  SND_GetFacingDirection( pChannel, pScratchPad->m_vBlendedListenerOrigin, vAngles );

	pStructMem->m_flOutput[0] = (flFacing + 1.0) * 0.5;

}
void CSosOperatorFacing::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorFacing_t *pStructMem = (CSosOperatorFacing_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorFacing::OpHelp( ) const
{

}

void CSosOperatorFacing::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorFacing_t *pStructMem = (CSosOperatorFacing_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// CSosOperatorRemapValue
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorRemapValue, "math_remap_float" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorRemapValue, m_flInput, SO_SINGLE, "input" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorRemapValue, m_flInputMin, SO_SINGLE, "input_min" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorRemapValue, m_flInputMax, SO_SINGLE, "input_max" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorRemapValue, m_flInputMapMin, SO_SINGLE, "input_map_min" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorRemapValue, m_flInputMapMax, SO_SINGLE, "input_map_max" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorRemapValue, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorRemapValue, "math_remap_float"  )

void CSosOperatorRemapValue::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorRemapValue_t *pStructMem = (CSosOperatorRemapValue_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInputMin, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputMax, SO_SINGLE, 0.1 )
	SOS_INIT_INPUT_VAR( m_flInputMapMin, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputMapMax, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInput, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )

	pStructMem->m_bClampRange = true;
	pStructMem->m_bDefaultMax = true;

}

void CSosOperatorRemapValue::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorRemapValue_t *pStructMem = (CSosOperatorRemapValue_t *)pVoidMem;

	float flResult;
	float flValue = pStructMem->m_flInput[0];
	float flMin = pStructMem->m_flInputMin[0];
	float flMax = pStructMem->m_flInputMax[0];
	float flMapMin = pStructMem->m_flInputMapMin[0];
	float flMapMax = pStructMem->m_flInputMapMax[0];

// 	if ( flMin > flMax )
// 	{
// 		Log_Warning( LOG_SND_OPERATORS, "Warning: remap_value operator min arg is greater than max arg\n");
// 
// 	}

	if ( flMapMin > flMapMax && flMin != flMax )
	{
		// swap everything around
		float flTmpMin = flMapMin;
		flMapMin = flMapMax;
		flMapMax = flTmpMin;
		flTmpMin = flMin;
		flMin = flMax;
		flMax = flTmpMin;
//		Log_Warning( LOG_SND_OPERATORS, "Warning: remap_value operator map min arg is greater than map max arg\n");
	}

	if( flMin == flMax )
	{
		if( flValue <  flMin)
		{
			flResult = flMapMin;
		}
		else if( flValue >  flMax )
		{
			flResult = flMapMax;
		}
		else
		{
			flResult = pStructMem->m_bDefaultMax ? flMapMax : flMapMin;
		}
	}
	else if ( flMapMin == flMapMax )
	{
		flResult = flMapMin;
	}
	else if( flValue <= flMin && flMin < flMax )
	{
		flResult = flMapMin;		
	}
	else if( flValue >= flMax && flMax > flMin)
	{
		flResult = flMapMax;		
	}
	else if( flValue >= flMin && flMin > flMax )
	{
		flResult = flMapMin;		
	}
	else if( flValue <= flMax && flMax < flMin)
	{
		flResult = flMapMax;		
	}
	else
	{

		flResult = RemapVal( flValue, flMin, flMax, flMapMin, flMapMax );
//		flResult = 	SOS_RESCALE( flValue, flMin, flMax, flMapMin, flMapMax );

	}

	if( pStructMem->m_bClampRange )
	{
		if( flMapMin < flMapMax )
		{
			flResult = clamp( flResult, flMapMin, flMapMax );
		}
		else
		{
			flResult = clamp( flResult, flMapMax, flMapMin );
		}

	}

	pStructMem->m_flOutput[0] = flResult;

}
void CSosOperatorRemapValue::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorRemapValue_t *pStructMem = (CSosOperatorRemapValue_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
}
void CSosOperatorRemapValue::OpHelp( ) const
{
}

void CSosOperatorRemapValue::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorRemapValue_t *pStructMem = (CSosOperatorRemapValue_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "clamp_range" ) )
				{
					if ( !V_strcasecmp( pValueString, "false" ) )
					{
						pStructMem->m_bClampRange = false;
					}
				}
				else if ( !V_strcasecmp( pParamString, "default_to_max" ) )
				{
					if ( !V_strcasecmp( pValueString, "false" ) )
					{
						pStructMem->m_bDefaultMax = false;
					}
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// CSosOperatorCurve4
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorCurve4, "math_curve_2d_4knot" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorCurve4, m_flInput, SO_SINGLE, "input" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorCurve4, m_flOutput, SO_SINGLE, "output" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorCurve4, m_flInputX1, SO_SINGLE, "input_X1" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorCurve4, m_flInputY1, SO_SINGLE, "input_Y1" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorCurve4, m_flInputX2, SO_SINGLE, "input_X2" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorCurve4, m_flInputY2, SO_SINGLE, "input_Y2" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorCurve4, m_flInputX3, SO_SINGLE, "input_X3" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorCurve4, m_flInputY3, SO_SINGLE, "input_Y3" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorCurve4, m_flInputX4, SO_SINGLE, "input_X4" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorCurve4, m_flInputY4, SO_SINGLE, "input_Y4" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorCurve4, "math_curve_2d_4knot"  )

void CSosOperatorCurve4::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorCurve4_t *pStructMem = (CSosOperatorCurve4_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInput, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )

	SOS_INIT_INPUT_VAR( m_flInputX1, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputY1, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputX2, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputY2, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputX3, SO_SINGLE, 2.0 )
	SOS_INIT_INPUT_VAR( m_flInputY3, SO_SINGLE, 2.0 )
	SOS_INIT_INPUT_VAR( m_flInputX4, SO_SINGLE, 3.0 )
	SOS_INIT_INPUT_VAR( m_flInputY4, SO_SINGLE, 3.0 )

	pStructMem->m_nCurveType = SO_OP_CURVETYPE_STEP;

}

void CSosOperatorCurve4::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorCurve4_t *pStructMem = (CSosOperatorCurve4_t *)pVoidMem;

	float flResult = 0.0;

	float flInput = pStructMem->m_flInput[0];

	if ( flInput >= pStructMem->m_flInputX4[0] )
	{
		flResult = pStructMem->m_flInputY4[0];
	}
	else if ( flInput <= pStructMem->m_flInputX1[0] )
	{
		flResult = pStructMem->m_flInputY1[0];
	}
	else
	{
		float flX[4];
		float flY[4];
		flX[0] = pStructMem->m_flInputX1[0];
		flX[1] = pStructMem->m_flInputX2[0];
		flX[2] = pStructMem->m_flInputX3[0];
		flX[3] = pStructMem->m_flInputX4[0];
		flY[0] = pStructMem->m_flInputY1[0];
		flY[1] = pStructMem->m_flInputY2[0];
		flY[2] = pStructMem->m_flInputY3[0];
		flY[3] = pStructMem->m_flInputY4[0];

		int i;
		for( i = 0; i < (4 - 1); i++ )
		{
			if( flInput > flX[i] && flInput < flX[i+1] )
			{
				break;
			}
		}
		if( pStructMem->m_nCurveType == SO_OP_CURVETYPE_STEP )
		{
			flResult = flY[i];
		}
		else if( pStructMem->m_nCurveType == SO_OP_CURVETYPE_LINEAR )
		{
			flResult = SOS_RESCALE( flInput, flX[i], flX[i+1], flY[i], flY[i+1] );
		}
	}
	pStructMem->m_flOutput[0] = flResult;

}
void CSosOperatorCurve4::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorCurve4_t *pStructMem = (CSosOperatorCurve4_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*scurve_type: %s\n", nLevel, "    ", pStructMem->m_nCurveType == SO_OP_CURVETYPE_STEP ? "step": "linear" );
}
void CSosOperatorCurve4::OpHelp( ) const
{
}

void CSosOperatorCurve4::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorCurve4_t *pStructMem = (CSosOperatorCurve4_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "curve_type" ) )
				{
					if ( !V_strcasecmp( pValueString, "step" ) )
					{
						pStructMem->m_nCurveType = SO_OP_CURVETYPE_STEP;
					}
					else if ( !V_strcasecmp( pValueString, "linear" ) )
					{
						pStructMem->m_nCurveType = SO_OP_CURVETYPE_LINEAR;
					}
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// CSosOperatorRandom
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorRandom, "math_random" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorRandom, m_flInputMin, SO_SINGLE, "input_min" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorRandom, m_flInputMax, SO_SINGLE, "input_max" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorRandom, m_flInputSeed, SO_SINGLE, "input_seed" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorRandom, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorRandom, "math_random"  )

void CSosOperatorRandom::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorRandom_t *pStructMem = (CSosOperatorRandom_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInputMin, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputMax, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputSeed, SO_SINGLE, -1.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )
	pStructMem->m_bRoundToInt = false;
}

void CSosOperatorRandom::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorRandom_t *pStructMem = (CSosOperatorRandom_t *)pVoidMem;

	if( pStructMem->m_flInputSeed[0] < 0.0 )
	{
		int iSeed = (int)( Plat_FloatTime() * 100 );
		g_pSoundOperatorSystem->m_operatorRandomStream.SetSeed( iSeed );
	}
	else
	{
		g_pSoundOperatorSystem->m_operatorRandomStream.SetSeed( (int) pStructMem->m_flInputSeed[0] );
	}
	float fResult = g_pSoundOperatorSystem->m_operatorRandomStream.RandomFloat( pStructMem->m_flInputMin[0], pStructMem->m_flInputMax[0] );

	if( pStructMem->m_bRoundToInt )
	{
		int nRound = (int) (fResult + 0.5);
		if( nRound < (int) pStructMem->m_flInputMin[0] )
		{
			nRound++;
		}
		else if( nRound > (int) pStructMem->m_flInputMax[0] )
		{
			nRound--;
		}
		fResult = (float) nRound;
	}
	
	pStructMem->m_flOutput[0] = fResult;

}
void CSosOperatorRandom::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorRandom_t *pStructMem = (CSosOperatorRandom_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorRandom::OpHelp( ) const
{

}

void CSosOperatorRandom::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorRandom_t *pStructMem = (CSosOperatorRandom_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "round_to_int" ) )
				{
					if ( !V_strcasecmp( pValueString, "false" ) )
					{
						pStructMem->m_bRoundToInt = false;
					}
					else if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bRoundToInt = true;
					}
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}


//-----------------------------------------------------------------------------
// CSosOperatorLogicOperator
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorLogicSwitch, "math_logic_switch" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorLogicSwitch, m_flInput1, SO_SINGLE, "input1" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorLogicSwitch, m_flInput2, SO_SINGLE, "input2" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorLogicSwitch, m_flInputSwitch, SO_SINGLE, "input_switch" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorLogicSwitch, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorLogicSwitch, "math_logic_switch"  )

void CSosOperatorLogicSwitch::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorLogicSwitch_t *pStructMem = (CSosOperatorLogicSwitch_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInput1, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInput2, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputSwitch, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )
}

void CSosOperatorLogicSwitch::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorLogicSwitch_t *pStructMem = (CSosOperatorLogicSwitch_t *)pVoidMem;
		
	pStructMem->m_flOutput[0] = ( (pStructMem->m_flInputSwitch[0] > 0) ? ( pStructMem->m_flInput2[0] ) : ( pStructMem->m_flInput1[0] ) );

}
void CSosOperatorLogicSwitch::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorRandom_t *pStructMem = (CSosOperatorRandom_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorLogicSwitch::OpHelp( ) const
{

}

void CSosOperatorLogicSwitch::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorLogicSwitch_t *pStructMem = (CSosOperatorLogicSwitch_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}

				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}
