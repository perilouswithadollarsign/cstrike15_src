//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

using namespace Scaleform::GFx;
using namespace Scaleform::Render;

/*********************************************
 * We're going to prime the value caches with some statically allocated
 * values and value arrays.  We'll start with VALUE_BANK_SIZE individual
 * Values, and NUMBER_OF_PRECACHED_x_ELEMENT_ARRAYS for the first few
 * arrays
 *
 * we need to be a little careful when we clean up these caches so that
 * we don't try to delete these statically allocated Values.
 */

const int VALUE_BANK_SIZE = 32;
const int NUMBER_OF_PRECACHED_2_ELEMENT_ARRAYS = 1;
const int NUMBER_OF_PRECACHED_3_ELEMENT_ARRAYS = 1;
const int NUMBER_OF_PRECACHED_4_ELEMENT_ARRAYS = 1;

const int VALUEARRAY_BANK_SIZE = NUMBER_OF_PRECACHED_2_ELEMENT_ARRAYS * 2 + NUMBER_OF_PRECACHED_3_ELEMENT_ARRAYS * 3 + NUMBER_OF_PRECACHED_4_ELEMENT_ARRAYS * 4;

static Value *VALUE_BANK = NULL;
static Value *VALUE_ARRAY_BANK = NULL;

inline int IndexFromArrayLength( int length )
{
	return length - 2;
}

inline int ArrayLengthFromIndex( int index )
{
	return index + 2;
}

/*****************************
 * internal functions
 */

inline bool PtrInValueBank( Value* ptr )
{
	return ( ( ptr >= &VALUE_BANK[0] ) && ( ptr < &VALUE_BANK[VALUE_BANK_SIZE] ) );
}

inline bool PtrInValueArrayBank( Value* ptr )
{
	return ( ( ptr >= &VALUE_ARRAY_BANK[0] ) && ( ptr < &VALUE_ARRAY_BANK[VALUEARRAY_BANK_SIZE] ) );
}

void ScaleformUIImpl::InitValueImpl( void )
{
	VALUE_BANK = new Value[VALUE_BANK_SIZE];
	VALUE_ARRAY_BANK = new Value[VALUEARRAY_BANK_SIZE];

	for ( int i = 0; i < VALUE_BANK_SIZE; i++ )
	{
		m_ValueCache.AddToTail( &VALUE_BANK[i] );
	}

	// initialize the 2, 3, and 4, element array caches with one
	// array each.
	int element = 0;
	for ( int i = 0; i <= 2; i++ )
	{
		m_ValueArrayCaches[i].AddToTail( &VALUE_ARRAY_BANK[element] );
		element += ArrayLengthFromIndex( i );
	}

	m_pFunctionAdapter = *new ScaleformFunctionHandlerAdapter();
}

void ScaleformUIImpl::ShutdownValueImpl( void )
{
	int j = m_ValueCache.Count();
	while( j-- )
	{
		if ( !PtrInValueBank( m_ValueCache[j] ) )
		{
			delete m_ValueCache[j];
		}
	}

	m_ValueCache.Purge();

	for ( int i = 0; i < NUM_VALUEARRAY_SLOTS; i++ )
	{
		j = m_ValueArrayCaches[i].Count();
		while( j-- )
		{
			if ( !PtrInValueArrayBank( m_ValueArrayCaches[i][j] ) )
			{
				delete[] m_ValueArrayCaches[i][j];
			}
		}

		m_ValueArrayCaches[i].Purge();
	}

	m_pFunctionAdapter = NULL;

#if defined( _DEBUG )
	bool showBlankLine = true;
	if ( m_ValuesInUse.Count() )
	{
		showBlankLine = false;
		LogPrintf( "\nUnreleased Value* handles!\n" );
		LogPrintf( "  handles (there are %i):\n", m_ValuesInUse.Count() );

		// print dangling error
		int i = m_ValuesInUse.Count();
		while( i-- )
		{
			LogPrintf( "    0x%p\n", m_ValuesInUse[i] );

			if ( !PtrInValueBank( m_ValuesInUse[i] ) )
			{
				delete m_ValuesInUse[i];
			}
		}
		LogPrintf( "\n" );

		m_ValuesInUse.Purge();
	}

	bool showedHeader = false;

	for ( int i = 0; i < NUM_VALUEARRAY_SLOTS; i++ )
	{
		if ( m_ValueArraysInUse[i].Count() )
		{
			if ( !showedHeader )
			{
				showedHeader = true;
				if ( showBlankLine )
				{
					showBlankLine = false;
					LogPrintf( "\n" );
				}

				LogPrintf( "Unreleased SFVALUEARRAY handles!\n" );
			}

			LogPrintf( "  %i element handles (there are %i):\n", ArrayLengthFromIndex( i ), m_ValueArraysInUse[i].Count() );

			// print dangling error
			int j = m_ValueArraysInUse[i].Count();
			while( j-- )
			{
				LogPrintf( "    0x%p\n", m_ValueArraysInUse[i][j] );

				if ( !PtrInValueArrayBank( m_ValueArraysInUse[i][j] ) )
				{
					delete[] m_ValueArraysInUse[i][j];
				}
			}

			m_ValueArraysInUse[i].Purge();
		}

	}

	if ( showedHeader )
	{
		LogPrintf( "\n" );
	}

#endif
	// Intentionally leak VALUE_BANK and VALUE_ARRAY_BANK to workaround crash on Mac @Value::ObjectInterface::ObjectRelease(Value*, void*)
	//delete [] VALUE_BANK;
	//delete [] VALUE_ARRAY_BANK;
	VALUE_BANK = NULL;
	VALUE_ARRAY_BANK = NULL;
}

SFVALUE ScaleformUIImpl::CreateGFxValue( SFVALUE value )
{
	MEM_ALLOC_CREDIT();
	Value* pResult;
	Value *pValue = FromSFVALUE( value );

	if ( m_ValueCache.Count() )
	{
		pResult = m_ValueCache.Tail();
		m_ValueCache.FastRemove( m_ValueCache.Count()-1 );
		if ( pValue )
		{
			*pResult =  *pValue;
		}
	}
	else
	{
		pResult = ( pValue ) ? new Value( *pValue ) : new Value();
	}

#if defined( _DEBUG )
	m_ValuesInUse.AddToTail( pResult );
#endif

	return ToSFVALUE( pResult );
}

void ScaleformUIImpl::ReleaseGFxValue( SFVALUE value )
{
	Value *pValue = FromSFVALUE( value );
	pValue->SetNull();
	m_ValueCache.AddToTail( pValue );

#if defined( _DEBUG )
	m_ValuesInUse.FindAndFastRemove( pValue );
#endif
}

/**********************************
 *  value creation and release
 */

void ScaleformUIImpl::ReleaseValue( SFVALUE value )
{
	if ( value != NULL )
	{
		ReleaseGFxValue( value );
	}
}

SFVALUE ScaleformUIImpl::CreateValue( SFVALUE value )
{
	return CreateGFxValue( value );
}

SFVALUE ScaleformUIImpl::CreateValue( int value )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );
	pResult->SetNumber( ( SF::Double ) value );
	return ToSFVALUE( pResult );

}
SFVALUE ScaleformUIImpl::CreateValue( float value )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );
	pResult->SetNumber( ( SF::Double ) value );
	return ToSFVALUE( pResult );
}

SFVALUE ScaleformUIImpl::CreateValue( bool value )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );
	pResult->SetBoolean( value );
	return ToSFVALUE( pResult );
}

SFVALUE ScaleformUIImpl::CreateValue( const char* value )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );
	pResult->SetString( value );
	return ToSFVALUE( pResult );
}

SFVALUE ScaleformUIImpl::CreateValue( const wchar_t* value )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );
	pResult->SetStringW( value );
	return ToSFVALUE( pResult );
}

void ScaleformUIImpl::Value_SetValue( SFVALUE obj, SFVALUE value )
{
	DebugBreakIfNotLocked();
	*FromSFVALUE( obj ) = *FromSFVALUE( value );
}

void ScaleformUIImpl::Value_SetValue( SFVALUE obj, int value )
{
	DebugBreakIfNotLocked();
	FromSFVALUE( obj )->SetNumber( ( SF::Double ) value );
}

void ScaleformUIImpl::Value_SetValue( SFVALUE obj, float value )
{
	DebugBreakIfNotLocked();
	FromSFVALUE( obj )->SetNumber( ( SF::Double ) value );
}

void ScaleformUIImpl::Value_SetValue( SFVALUE obj, bool value )
{
	DebugBreakIfNotLocked();
	FromSFVALUE( obj )->SetBoolean( value );
}

void ScaleformUIImpl::Value_SetValue( SFVALUE obj, const char* value )
{
	DebugBreakIfNotLocked();
	FromSFVALUE( obj )->SetString( value );
}

void ScaleformUIImpl::Value_SetValue( SFVALUE obj, const wchar_t* value )
{
	DebugBreakIfNotLocked();
	FromSFVALUE( obj )->SetStringW( value );
}

void ScaleformUIImpl::Value_SetText( SFVALUE obj, const char* value )
{
	DebugBreakIfNotLocked();
	FromSFVALUE( obj )->SetText( value );
}

void ScaleformUIImpl::Value_SetText( SFVALUE obj, const wchar_t* value )
{
	DebugBreakIfNotLocked();
	FromSFVALUE( obj )->SetText( value );
}

void ScaleformUIImpl::Value_SetTextHTML( SFVALUE obj, const char* value )
{
	DebugBreakIfNotLocked();
	FromSFVALUE( obj )->SetTextHTML( value );
}

void ScaleformUIImpl::Value_SetTextHTML( SFVALUE obj, const wchar_t* value )
{
	DebugBreakIfNotLocked();
	FromSFVALUE( obj )->SetTextHTML( value );
}

int ScaleformUIImpl::Value_SetFormattedText( SFVALUE obj, const char* pFormat, ... )
{
	DebugBreakIfNotLocked();
	va_list marker;
	va_start( marker, pFormat );
	int len = V_vsnprintf( m_cTemporaryBuffer, TEMPORARY_BUFFER_SIZE, pFormat, marker );
	va_end( marker );
	Value_SetTextHTML( obj, m_cTemporaryBuffer );
	return len;
}



void ScaleformUIImpl::Value_SetColor( SFVALUE obj, int color )
{
	Value_SetColor( obj, ( color >> 16 ) & 0xFF, ( color >> 8 )  & 0xFF,  ( color ) & 0xFF, ( color >> 24 ) & 0xFF );	
}

void ScaleformUIImpl::Value_SetColor( SFVALUE obj, float r, float g, float b, float a )
{
	DebugBreakIfNotLocked();
	
	Value *pValue = FromSFVALUE( obj );

	Cxform cxform;

	cxform.M[0][0] = 0;
	cxform.M[0][1] = 0;
	cxform.M[0][2] = 0;
	cxform.M[0][3] = 0;

	cxform.M[1][0] = r * 255.0f;
	cxform.M[1][1] = g * 255.0f;
	cxform.M[1][2] = b * 255.0f;
	cxform.M[1][3] = a * 255.0f;

	pValue->SetColorTransform( cxform );
}

void ScaleformUIImpl::Value_SetTint( SFVALUE obj, int color )
{
	DebugBreakIfNotLocked();
	
	Value_SetTint( obj, 
				   ( ( color >> 16 ) & 0xFF ) / 255.0f,
				   ( ( color >> 8 ) & 0xFF ) / 255.0f,
				   ( color & 0xFF ) / 255.0f,
				   ( ( color >> 24 ) & 0xFF ) / 255.0f );
}

void ScaleformUIImpl::Value_SetTint( SFVALUE obj, float r, float g, float b, float a )
{
	DebugBreakIfNotLocked();

	Value *pValue = FromSFVALUE( obj );

	Cxform cxform;

	cxform.M[0][0] = r;
	cxform.M[0][1] = g;
	cxform.M[0][2] = b;
	cxform.M[0][3] = a;

	cxform.M[1][0] = 0;
	cxform.M[1][1] = 0;
	cxform.M[1][2] = 0;
	cxform.M[1][3] = 0;

	pValue->SetColorTransform( cxform );
}

void ScaleformUIImpl::Value_SetColorTransform( SFVALUE obj, int colorMultiply, int colorAdd )
{
	DebugBreakIfNotLocked();
	Cxform cxform;

	Value_SetColorTransform( obj, 
			
			( ( colorMultiply >> 16 ) & 0xFF ) / 255.0f,
			( ( colorMultiply >> 8 ) & 0xFF ) / 255.0f,
			( colorMultiply & 0xFF ) / 255.0f,
			( ( colorMultiply >> 24 ) & 0xFF ) / 255.0f,

			colorAdd )	;
}

void ScaleformUIImpl::Value_SetColorTransform( SFVALUE obj, float r, float g, float b, float a, int colorAdd )
{
	DebugBreakIfNotLocked();
	Cxform cxform;

	cxform.M[0][0] = r;
	cxform.M[0][1] = g;
	cxform.M[0][2] = b;
	cxform.M[0][3] = a;

	cxform.M[1][0] = ( colorAdd >> 16 ) & 0xFF;
	cxform.M[1][1] = ( colorAdd >> 8 )& 0xFF;
	cxform.M[1][2] = colorAdd& 0xFF;
	cxform.M[1][3] = ( colorAdd >> 24 ) & 0xFF;

	FromSFVALUE( obj )->SetColorTransform( cxform );
}


/**********************************
 * array manipulation
 */


void ScaleformUIImpl::CreateValueArray( SFVALUEARRAY& valueArray, int length )
{
	Value* pValues = NULL;
	if ( length > 0 && length <= MAX_VALUES_IN_ARRAY )
	{
		if ( length == 1 )
		{
			pValues = FromSFVALUE( CreateGFxValue() );
		}
		else
		{
			int index = IndexFromArrayLength( length );
			int count = m_ValueArrayCaches[index].Count();

			if ( count )
			{
				pValues = m_ValueArrayCaches[index].Tail();
				m_ValueArrayCaches[index].RemoveMultipleFromTail( 1 );
			}
			else
				pValues = new Value[length];
#if _DEBUG
			// we don't have to do this if the length was 1
			// because it will be taken care of by the single
			// value allocator.
			m_ValueArraysInUse[index].AddToTail( pValues );
#endif

			for (int i = 0; i < length; i++ )
			{
				pValues[i].SetNull();
			}
		}

	}

	valueArray.SetValues( length, ToSFVALUE( pValues ) );
}


SFVALUEARRAY ScaleformUIImpl::CreateValueArray( int length )
{
	Value* pValues = NULL;
	if ( length > 0 && length <= MAX_VALUES_IN_ARRAY )
	{
		if ( length == 1 )
		{
			pValues = FromSFVALUE( CreateGFxValue() );
		}
		else
		{
			int index = IndexFromArrayLength( length );
			int count = m_ValueArrayCaches[index].Count();

			if ( count )
			{
				pValues = m_ValueArrayCaches[index].Tail();
				m_ValueArrayCaches[index].RemoveMultipleFromTail( 1 );
			}
			else
				pValues = new Value[length];
#if _DEBUG
			// we don't have to do this if the length was 1
			// because it will be taken care of by the single
			// value allocator.
			m_ValueArraysInUse[index].AddToTail( pValues );
#endif

			for (int i = 0; i < length; i++ )
			{
				pValues[i].SetNull();
			}
		}
	}

	return SFVALUEARRAY( length, ToSFVALUE( pValues ) );
}

void ScaleformUIImpl::ReleaseValueArray( SFVALUEARRAY& valueArray )
{
	if ( valueArray.GetValues() == NULL )
		return;

	if ( valueArray.Count() == 1 )
	{
		ReleaseGFxValue( valueArray[0] );
	}
	else
	{
		for (int i = 0; i < valueArray.Count(); i++ )
		{
			FromSFVALUE( valueArray[i] )->SetNull();
		}

		int index = IndexFromArrayLength( valueArray.Count() );
		m_ValueArrayCaches[index].AddToTail( FromSFVALUE( valueArray.GetValues() ) );

#if _DEBUG
		// we don't have to do this if the length is 1 because the single value
		// releaser will handle that case.
		m_ValueArraysInUse[index].FindAndFastRemove( FromSFVALUE( valueArray[0] ) );
#endif
	}
	valueArray.SetValues(0, NULL);
}

void ScaleformUIImpl::ReleaseValueArray( SFVALUEARRAY& valueArray, int count )
{
	Assert(count == valueArray.Count());
	ReleaseValueArray(valueArray);
}


SFVALUE ScaleformUIImpl::ValueArray_GetElement( SFVALUEARRAY valueArray, int index )
{
	return valueArray[index];
}

IScaleformUI::_ValueType ScaleformUIImpl::ValueArray_GetType( SFVALUEARRAY valueArray, int index )
{
	return ValueType_SDK_to_SFUI( FromSFVALUE( valueArray[index] )->GetType() );
}

double ScaleformUIImpl::ValueArray_GetNumber( SFVALUEARRAY valueArray, int index )
{
	return FromSFVALUE(valueArray[index])->GetNumber();
}

bool ScaleformUIImpl::ValueArray_GetBool( SFVALUEARRAY valueArray, int index )
{
	return FromSFVALUE(valueArray[index])->GetBool();
}

const char* ScaleformUIImpl::ValueArray_GetString( SFVALUEARRAY valueArray, int index )
{
	return FromSFVALUE(valueArray[index])->GetString();
}

const wchar_t* ScaleformUIImpl::ValueArray_GetStringW( SFVALUEARRAY valueArray, int index )
{
	return FromSFVALUE(valueArray[index])->GetStringW();
}

void ScaleformUIImpl::ValueArray_SetElement( SFVALUEARRAY valueArray, int index, SFVALUE value )
{
	*FromSFVALUE(valueArray[index]) = *FromSFVALUE(value);
}

void ScaleformUIImpl::ValueArray_SetElement( SFVALUEARRAY valueArray, int index, int value )
{
	FromSFVALUE(valueArray[index])->SetNumber( ( SF::Double ) value );
}

void ScaleformUIImpl::ValueArray_SetElement( SFVALUEARRAY valueArray, int index, float value )
{
	FromSFVALUE(valueArray[index])->SetNumber( ( SF::Double ) value );
}

void ScaleformUIImpl::ValueArray_SetElement( SFVALUEARRAY valueArray, int index, bool value )
{
	FromSFVALUE(valueArray[index])->SetBoolean( value );
}

void ScaleformUIImpl::ValueArray_SetElement( SFVALUEARRAY valueArray, int index, const char* value )
{
	FromSFVALUE(valueArray[index])->SetString( value );
}

void ScaleformUIImpl::ValueArray_SetElement( SFVALUEARRAY valueArray, int index, const wchar_t* value )
{
	FromSFVALUE(valueArray[index])->SetStringW( value );
}

void ScaleformUIImpl::ValueArray_SetElementText( SFVALUEARRAY valueArray, int index, const char* value )
{
	FromSFVALUE(valueArray[index])->SetText( value );
}

void ScaleformUIImpl::ValueArray_SetElementText( SFVALUEARRAY valueArray, int index, const wchar_t* value )
{
	FromSFVALUE(valueArray[index])->SetText( value );
}

void ScaleformUIImpl::ValueArray_SetElementTextHTML( SFVALUEARRAY valueArray, int index, const char* value )
{
	FromSFVALUE(valueArray[index])->SetTextHTML( value );
}

void ScaleformUIImpl::ValueArray_SetElementTextHTML( SFVALUEARRAY valueArray, int index, const wchar_t* value )
{
	FromSFVALUE(valueArray[index])->SetTextHTML( value );
}

/**********************************
 * value manipulation
 */

bool ScaleformUIImpl::Value_HasMember( SFVALUE value, const char* name )
{
	return FromSFVALUE( value )->HasMember( name );
}

SFVALUE ScaleformUIImpl::Value_GetMember( SFVALUE value, const char* name )
{
	Value newValue;
	Value* pResult = NULL;

	bool worked = FromSFVALUE( value )->GetMember( name, &newValue );

	if ( worked )
	{
		pResult = FromSFVALUE( CreateGFxValue( ToSFVALUE( &newValue ) ) );
	}

	return ToSFVALUE( pResult );

}

bool ScaleformUIImpl::Value_SetMember( SFVALUE obj, const char *name, SFVALUE value )
{
	DebugBreakIfNotLocked();
	return FromSFVALUE( obj )->SetMember( name, *FromSFVALUE(value) );
}

bool ScaleformUIImpl::Value_SetMember( SFVALUE obj, const char *name, int value )
{
	DebugBreakIfNotLocked();
	Value gfxv( ( double ) value );
	return FromSFVALUE( obj )->SetMember( name, gfxv );
}

bool ScaleformUIImpl::Value_SetMember( SFVALUE obj, const char *name, float value )
{
	DebugBreakIfNotLocked();
	Value gfxv( ( double ) value );
	return FromSFVALUE( obj )->SetMember( name, gfxv );
}

bool ScaleformUIImpl::Value_SetMember( SFVALUE obj, const char *name, bool value )
{
	DebugBreakIfNotLocked();
	Value gfxv( value );
	return FromSFVALUE( obj )->SetMember( name, gfxv );
}

bool ScaleformUIImpl::Value_SetMember( SFVALUE obj, const char *name, const char* value )
{
	DebugBreakIfNotLocked();
	Value gfxv( value );
	return FromSFVALUE( obj )->SetMember( name, gfxv );
}

bool ScaleformUIImpl::Value_SetMember( SFVALUE obj, const char *name, const wchar_t* value )
{
	DebugBreakIfNotLocked();
	Value gfxv( value );
	return FromSFVALUE( obj )->SetMember( name, gfxv );
}


bool InvokeHelper( Value* pObj, const char* methodName, Value* pArgs, unsigned int numArgs, Value* pResult )
{
	MEM_ALLOC_CREDIT();

	if ( pArgs && numArgs )
	{
		return pObj->Invoke( methodName, pResult, pArgs, numArgs );
	}
	else
	{
		return pObj->Invoke( methodName, pResult );
	}
}

bool ScaleformUIImpl::Value_InvokeWithoutReturn( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args )
{
	DebugBreakIfNotLocked();
	if ( !obj )
		return false;
	return InvokeHelper( FromSFVALUE( obj ), methodName, FromSFVALUE( args.GetValues() ), args.Count(), NULL );
}

bool ScaleformUIImpl::Value_InvokeWithoutReturn( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args, int numArgs )
{
	Assert(numArgs == args.Count());
	if ( numArgs != args.Count() )
	{
		Warning("Value_Invoke(%s) called with numArgs=%i but SFVALUEARRAY size=%i\n", methodName, numArgs, args.Count());
	}

	return Value_InvokeWithoutReturn(obj, methodName, args);
}

bool ScaleformUIImpl::Value_InvokeWithoutReturn( SFVALUE obj, const char* methodName, SFVALUE arg, int numArgs )
{
	DebugBreakIfNotLocked();
	Assert( (numArgs == 0 && arg == NULL ) || (numArgs == 1 && arg != NULL) );

	if ( !obj )
		return false;
	return InvokeHelper( FromSFVALUE( obj ), methodName, FromSFVALUE( arg ), numArgs, NULL );
}

SFVALUE ScaleformUIImpl::Value_Invoke( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args )
{
	DebugBreakIfNotLocked();
	Value newValue;
	Value* pResult = NULL;

	bool worked = InvokeHelper( FromSFVALUE( obj ), methodName, FromSFVALUE( args.GetValues() ), args.Count(), &newValue );

	if ( worked )
	{
		pResult = FromSFVALUE( CreateGFxValue( ToSFVALUE( &newValue ) ) );
	}

	return ToSFVALUE( pResult );
}

SFVALUE ScaleformUIImpl::Value_Invoke( SFVALUE obj, const char* methodName, const SFVALUEARRAY& args, int numArgs )
{
	Assert(numArgs == args.Count());
	if ( numArgs != args.Count() )
	{
		Warning("Value_Invoke(%s) called with numArgs=%i but SFVALUEARRAY size=%i\n", methodName, numArgs, args.Count());
	}

	return Value_Invoke(obj, methodName, args);
}

SFVALUE ScaleformUIImpl::Value_Invoke( SFVALUE obj, const char* methodName, SFVALUE arg, int numArgs )
{
	DebugBreakIfNotLocked();
	Assert( (numArgs == 0 && arg == NULL ) || (numArgs == 1 && arg != NULL) );

	Value newValue;
	Value* pResult = NULL;

	bool worked = InvokeHelper( FromSFVALUE( obj ), methodName, FromSFVALUE( arg ), numArgs, &newValue );

	if ( worked )
	{
		pResult = FromSFVALUE( CreateGFxValue( ToSFVALUE( &newValue ) ) );
	}

	return ToSFVALUE( pResult );
}

void ScaleformUIImpl::AddAPIFunctionToObject( SFVALUE pApi, SFMOVIE pMovie, ScaleformUIFunctionHandlerObject* object, const ScaleformUIFunctionHandlerDefinition* pFunctionDef )
{
	Value func;
	ScaleformCallbackHolder* holder = new ScaleformCallbackHolder( object, pFunctionDef->m_pHandler );
	FromSFMOVIE( pMovie )->CreateFunction( &func, m_pFunctionAdapter, ( void* )holder );
	func.SetUserData( holder );
	FromSFVALUE( pApi )->SetMember( pFunctionDef->m_pName, func );

}

void ScaleformUIImpl::DebugBreakIfNotLocked( void )
{
#if defined ( WIN32 )

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//if ( !OwnsAtLeastOneMutex() )
	//{
	//	// DebuggerBreakIfDebugging();
	//	Warning( "WARNING: Some code is calling a scaleform method without any locked mutex! Run with a debugger attached to catch this callstack.\n" );
	//}
#endif
}

bool ScaleformUIImpl::OwnsAtLeastOneMutex( void )
{
#if defined ( WIN32 )

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//for ( int i = 0; i < MAX_SLOTS; i++ )
	//{
	//	if ( m_SlotMutexes[i].IsOwnedByCurrentThread_DebugOnly() )
			return true;
	//}
#endif
	return false;

}

void ScaleformUIImpl::SetVisible( SFVALUE pgfx, bool visible )
{
	Value::DisplayInfo info( visible );
	FromSFVALUE( pgfx )->SetDisplayInfo( info );
}

void ScaleformUIImpl::Value_SetVisible( SFVALUE obj, bool visible )
{
	SetVisible( obj, visible );
}

void ScaleformUIImpl::Value_GetDisplayInfo( SFVALUE obj, ScaleformDisplayInfo* dinfo )
{	
	Value* pVal = FromSFVALUE( obj );

	if ( pVal->GetType() == VT_Null )
	{
		AssertMsg( 0, "ScaleformUIImpl::Value_GetDisplayInfo: Passed a null typed Value*!" );
		Warning( "ScaleformUIImpl::Value_GetDisplayInfo: Passed a null typed Value*!" );
		dinfo->Clear();
		return;
	}

	Value::DisplayInfo info;
	if ( ( pVal )->GetDisplayInfo( &info ) )
	{
		dinfo->SetX( info.GetX() );
		dinfo->SetY( info.GetY() );
		dinfo->SetRotation( info.GetRotation() );
		dinfo->SetAlpha( info.GetAlpha() );
		dinfo->SetVisibility( info.GetVisible() );
		dinfo->SetXScale( info.GetXScale() );
		dinfo->SetYScale( info.GetYScale() );
	}

	dinfo->Clear();
}

void ScaleformUIImpl::Value_SetDisplayInfo( SFVALUE obj, const ScaleformDisplayInfo* dinfo )
{
	Value::DisplayInfo info;

	if ( dinfo->IsXSet() )
	{
		info.SetX( dinfo->GetX() );
	}

	if ( dinfo->IsYSet() )
	{
		info.SetY( dinfo->GetY() );
	}

	if ( dinfo->IsRotationSet() )
	{
		info.SetRotation( dinfo->GetRotation() );
	}

	if ( dinfo->IsAlphaSet() )
	{
		info.SetAlpha( dinfo->GetAlpha() );
	}

	if ( dinfo->IsVisibilitySet() )
	{
		info.SetVisible( dinfo->GetVisibility() );
	}

	if ( dinfo->IsXScaleSet() )
	{
		info.SetXScale( dinfo->GetXScale() );
	}

	if ( dinfo->IsYScaleSet() )
	{
		info.SetYScale( dinfo->GetYScale() );
	}

	Value *pVal = FromSFVALUE( obj );
	if ( pVal->GetType() == VT_Null )
	{
		AssertMsg( 0, "ScaleformUIImpl::Value_SetDisplayInfo: Passed a null typed Value*!" );
		Warning( "ScaleformUIImpl::Value_SetDisplayInfo: Passed a null typed Value*!" );
	}
	else
	{
		pVal->SetDisplayInfo( info );
	}


}

IScaleformUI::_ValueType ScaleformUIImpl::Value_GetType( SFVALUE obj )
{
	return ValueType_SDK_to_SFUI( FromSFVALUE( obj )->GetType() );
}

double ScaleformUIImpl::Value_GetNumber( SFVALUE obj )
{
	return FromSFVALUE( obj )->GetNumber();
}

bool ScaleformUIImpl::Value_GetBool( SFVALUE obj )
{
	return FromSFVALUE( obj )->GetBool();
}

const char* ScaleformUIImpl::Value_GetString( SFVALUE obj )
{
	return FromSFVALUE( obj )->GetString();
}

const wchar_t* ScaleformUIImpl::Value_GetStringW( SFVALUE obj )
{
	return FromSFVALUE( obj )->GetStringW();
}

SFVALUE ScaleformUIImpl::Value_GetText( SFVALUE obj )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );
	FromSFVALUE( obj )->GetText( pResult );
	return ToSFVALUE( pResult );
}

SFVALUE ScaleformUIImpl::Value_GetTextHTML( SFVALUE obj )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );
	FromSFVALUE( obj )->GetTextHTML( pResult );
	return ToSFVALUE( pResult );
}

void ScaleformUIImpl::Value_SetArraySize( SFVALUE obj, int size )
{
	FromSFVALUE( obj )->SetArraySize( size );
}

int  ScaleformUIImpl::Value_GetArraySize( SFVALUE obj )
{
	return ( int )FromSFVALUE( obj )->GetArraySize();
}

void ScaleformUIImpl::Value_ClearArrayElements( SFVALUE obj )
{
	FromSFVALUE( obj )->ClearElements();
}


void ScaleformUIImpl::Value_RemoveArrayElement( SFVALUE obj, int index )
{
	FromSFVALUE( obj )->RemoveElement( index );
}

void ScaleformUIImpl::Value_RemoveArrayElements( SFVALUE obj, int index, int count )
{
	FromSFVALUE( obj )->RemoveElements( index, count );
}

SFVALUE ScaleformUIImpl::Value_GetArrayElement( SFVALUE obj, int index )
{
	Value* pResult = FromSFVALUE( CreateGFxValue() );

	FromSFVALUE( obj )->GetElement( index, pResult );

	return ToSFVALUE( pResult );
}

void ScaleformUIImpl::Value_SetArrayElement( SFVALUE obj, int index, SFVALUE value )
{
	Assert( OwnsAtLeastOneMutex() );
	FromSFVALUE( obj )->SetElement( index, *FromSFVALUE( value ) );
}

void ScaleformUIImpl::Value_SetArrayElement( SFVALUE obj, int index, int value )
{
	Assert( OwnsAtLeastOneMutex() );
	Value gv( ( double ) value );
	FromSFVALUE( obj )->SetElement( index, gv );
}

void ScaleformUIImpl::Value_SetArrayElement( SFVALUE obj, int index, float value )
{
	Assert( OwnsAtLeastOneMutex() );
	Value gv( ( double ) value );
	FromSFVALUE( obj )->SetElement( index, gv );
}

void ScaleformUIImpl::Value_SetArrayElement( SFVALUE obj, int index, bool value )
{
	Assert( OwnsAtLeastOneMutex() );
	Value gv( value );
	FromSFVALUE( obj )->SetElement( index, gv );
}

void ScaleformUIImpl::Value_SetArrayElement( SFVALUE obj, int index, const char* value )
{
	Assert( OwnsAtLeastOneMutex() );
	Value gv( value );
	FromSFVALUE( obj )->SetElement( index, gv );
}

void ScaleformUIImpl::Value_SetArrayElement( SFVALUE obj, int index, const wchar_t* value )
{
	Assert( OwnsAtLeastOneMutex() );
	Value gv( value );
	FromSFVALUE( obj )->SetElement( index, gv );
}

SFVALUE SFVALUEARRAY::operator[]( int index )
{
	Assert(index >= 0 && index < m_count);
	if ( index < 0 || index >= m_count )
	{
		Error("Index out of range for SFVALUEARRAY\n");
	}
	Value* pValues = FromSFVALUE(m_pValues);
	return ToSFVALUE( &pValues[index] );
}
