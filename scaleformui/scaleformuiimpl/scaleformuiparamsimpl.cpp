//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

using namespace Scaleform::GFx;

// Uncomment this to check you are not trying to access an invalid params and the
// type is correct
// Comment in pc-beta
//#define SF_CHECK_PARAMS_VALID

#if defined( SF_CHECK_PARAMS_VALID )
#define SF_PARAMS_ASSERT( _exp, _msg )	AssertMsgAlways( _exp, _msg )
#else
#define SF_PARAMS_ASSERT( _exp, _msg )	((void)0)
#endif

SFVALUEARRAY ScaleformUIImpl::Params_GetArgs( SFPARAMS sfparams )
{
	FunctionHandler::Params* pParams = FromSFPARAMS(sfparams);
	return SFVALUEARRAY( pParams->ArgCount, ToSFVALUE( pParams->pArgs ) );
}

unsigned int ScaleformUIImpl::Params_GetNumArgs( SFPARAMS params )
{
	return FromSFPARAMS(params)->ArgCount;
}

SFVALUE ScaleformUIImpl::Params_GetArg( SFPARAMS params, int index )
{
	FunctionHandler::Params* pParams = FromSFPARAMS(params);
	SF_PARAMS_ASSERT( (uint)index < pParams->ArgCount, "SF param out of range!" );
	return ToSFVALUE( &( pParams->pArgs[index] ) );
}

IScaleformUI::_ValueType ScaleformUIImpl::Params_GetArgType( SFPARAMS params, int index )
{
	FunctionHandler::Params* pParams = FromSFPARAMS(params);
	SF_PARAMS_ASSERT( (uint)index < pParams->ArgCount, "SF param out of range!" );
	return ((uint)index < pParams->ArgCount) ? ValueType_SDK_to_SFUI( pParams->pArgs[index].GetType() ) : IScaleformUI::VT_Undefined;
}

bool ScaleformUIImpl::Params_ArgIs( SFPARAMS params, unsigned int index, IScaleformUI::_ValueType v )
{
	if ( index < ( FromSFPARAMS(params) )->ArgCount )
	{
		return ( ValueType_SDK_to_SFUI( FromSFPARAMS(params)->pArgs[index].GetType() ) == v );
	}
	else
	{
		return false;
	}

}

double ScaleformUIImpl::Params_GetArgAsNumber( SFPARAMS params, int index )
{
	FunctionHandler::Params* pParams = FromSFPARAMS(params);
	SF_PARAMS_ASSERT( (uint)index < pParams->ArgCount, "SF param out of range!" );
	SF_PARAMS_ASSERT( pParams->pArgs[index].IsNumber(), "SF param is not a number!" );
	return ( double ) pParams->pArgs[index].GetNumber();
}

bool ScaleformUIImpl::Params_GetArgAsBool( SFPARAMS params, int index )
{
	FunctionHandler::Params* pParams = FromSFPARAMS(params);
	SF_PARAMS_ASSERT( (uint)index < pParams->ArgCount, "SF param out of range!" );
	SF_PARAMS_ASSERT( pParams->pArgs[index].IsBool(), "SF param is not a boolean!" );
	return pParams->pArgs[index].GetBool();
}

const char* ScaleformUIImpl::Params_GetArgAsString( SFPARAMS params, int index )
{
	FunctionHandler::Params* pParams = FromSFPARAMS(params);
	SF_PARAMS_ASSERT( (uint)index < pParams->ArgCount, "SF param out of range!" );
	SF_PARAMS_ASSERT( pParams->pArgs[index].IsString(), "SF param is not a string!" );
	return pParams->pArgs[index].GetString();
}

const wchar_t* ScaleformUIImpl::Params_GetArgAsStringW( SFPARAMS params, int index )
{
	FunctionHandler::Params* pParams = FromSFPARAMS(params);
	SF_PARAMS_ASSERT( (uint)index < pParams->ArgCount, "SF param out of range!" );
	SF_PARAMS_ASSERT( pParams->pArgs[index].IsStringW(), "SF param is not a StringW!" );
	return pParams->pArgs[index].GetStringW();
}

const char* Helper_SFTypeToCString( Value::ValueType type )
{
	switch ( type )
	{
		case Value::VT_Undefined:			return "undefined";
		case Value::VT_Null:				return "null";
		case Value::VT_Boolean:			return "boolean";
		case Value::VT_Int:				return "int";
		case Value::VT_UInt:				return "uint";
		case Value::VT_Number:				return "number";
		case Value::VT_String:				return "string";
		case Value::VT_StringW:			return "stringw";
		case Value::VT_Object:				return "object";
		case Value::VT_Array:				return "array";
		case Value::VT_DisplayObject:		return "displayobject";
		case Value::VT_Closure:			return "closure";
		case Value::VT_ConvertBoolean:		return "convertboolean";
		case Value::VT_ConvertInt:			return "convertint";
		case Value::VT_ConvertUInt:		return "convertuint";
		case Value::VT_ConvertNumber:		return "convertnumber";
		case Value::VT_ConvertString:		return "convertstring";
		case Value::VT_ConvertStringW:		return "convertstringw";

		default:
			AssertMsg( 0, "Unknown ValueType\n" );
			return "undefined";
	}
}

void ScaleformUIImpl::Params_DebugSpew( SFPARAMS params )
{
	FunctionHandler::Params* pParams = FromSFPARAMS( params );
	for ( unsigned i = 0; i < pParams->ArgCount; ++i )
	{
		Msg( "Param %d: %s (%s) \n", i, pParams->pArgs[ i ].ToString().ToCStr(), Helper_SFTypeToCString( pParams->pArgs[ i ].GetType() ) );
	}
}

void ScaleformUIImpl::Params_SetResult( SFPARAMS params, SFVALUE value )
{
	*FromSFPARAMS( params )->pRetVal = *FromSFVALUE( value );
}

void ScaleformUIImpl::Params_SetResult( SFPARAMS params, int value )
{
	FromSFPARAMS(params)->pRetVal->SetNumber( ( SF::Double ) value );
}

void ScaleformUIImpl::Params_SetResult( SFPARAMS params, float value )
{
	FromSFPARAMS(params)->pRetVal->SetNumber( ( SF::Double ) value );
}

void ScaleformUIImpl::Params_SetResult( SFPARAMS params, bool value )
{
	FromSFPARAMS(params)->pRetVal->SetBoolean( value );
}

void ScaleformUIImpl::Params_SetResult( SFPARAMS params, const char* value, bool bMakeNewValue /* = true */ )
{
	char dummy_value;
	const char* stack_addr = &dummy_value;

	const intp diff = value - stack_addr;
	if ( ( diff > -128*1024 && diff < 128*1024 ) || bMakeNewValue )
	{
		// This string looks like it is on the stack. Make a Scaleform-managed string
		// to duplicate it into.
		SFVALUE managed_value = Params_CreateNewString( params, value );
		Params_SetResult( params, managed_value );
		ReleaseValue( managed_value );
	}
	else
	{

		( reinterpret_cast<FunctionHandler::Params*>(params)->pRetVal )->SetString( value );
	}
}

void ScaleformUIImpl::Params_SetResult( SFPARAMS params, const wchar_t* value, bool bMakeNewValue /* = true */ )
{
	wchar_t dummy_value;
	const wchar_t* stack_addr = &dummy_value;

	const intp diff = value - stack_addr;
	if ( ( diff > -128*1024 && diff < 128*1024 ) || bMakeNewValue )
	{
		// This string looks like it is on the stack. Make a Scaleform-managed string
		// to duplicate it into.
		SFVALUE managed_value = Params_CreateNewString( params, value );
		Params_SetResult( params, managed_value );
		ReleaseValue( managed_value );
	}
	else
	{
		( ( ( FunctionHandler::Params* ) params )->pRetVal )->SetStringW( value );
	}
}

SFVALUE ScaleformUIImpl::Params_CreateNewObject( SFPARAMS params )
{
	return MovieView_CreateObject( ToSFMOVIE( FromSFPARAMS(params)->pMovie ) );
}

SFVALUE ScaleformUIImpl::Params_CreateNewArray( SFPARAMS params, int size )
{
	return MovieView_CreateArray( ToSFMOVIE( FromSFPARAMS(params)->pMovie ), size );
}

SFVALUE ScaleformUIImpl::Params_CreateNewString( SFPARAMS params, const char* value )
{
	return MovieView_CreateString( ToSFMOVIE( FromSFPARAMS(params)->pMovie ), value );
}

SFVALUE ScaleformUIImpl::Params_CreateNewString( SFPARAMS params, const wchar_t* value )
{
	return MovieView_CreateStringW( ToSFMOVIE( FromSFPARAMS(params)->pMovie ), value );
}


