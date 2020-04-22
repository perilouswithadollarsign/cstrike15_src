//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#include "stdafx.h"
#include "tier1/keyvalues.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Controls.h"
#include "convar.h"
#include "../game/shared/cstrike15/dlchelper.h"
#include <vstdlib/vstrtools.h>
#include "vgui/ISystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define FLASH_ARRAY_ID "_array_"
#define FLASH_ARRAY_NAME_ID "_name_"
#define FLASH_ARRAY_ELEMENT_ID "_"

static const float CHANGE_CONTROLLER_TIMEOUT = 0.5f;

using namespace SF::GFx;


/*******************************
 * visitors to allow flash objects to be serialized
 */

class FlashArrayVisitor: public Value::ArrayVisitor
{
public:
	KeyValues* m_pKV;
	const Value* m_pOriginalValue;

	void AddObject( const Value& val );
	void AddArray( const Value& val )
	{
		KeyValues* pnew = m_pKV->CreateNewKey();
		pnew->SetName( FLASH_ARRAY_ID );

		FlashArrayVisitor visitor;

		visitor.m_pKV = pnew;
		visitor.m_pOriginalValue = &val;

		val.VisitElements( &visitor );

		if ( pnew->IsEmpty() )
		{
			m_pKV->RemoveSubKey( pnew );
		}
	}

	void Visit( unsigned int idx, const Value& val )
	{
		if ( *m_pOriginalValue == val )
		{
			return;
		}

		SF::Double number;
		int inumber;
		KeyValues* pnew = NULL;

		switch ( val.GetType() )
		{
			case Value::VT_Boolean:
				pnew = m_pKV->CreateNewKey();
				pnew->SetBool( NULL, val.GetBool() );
				break;

			case Value::VT_Number:
				number = val.GetNumber();
				inumber = ( int ) number;
				pnew = NULL;

				if ( ( SF::Double ) inumber == number )
				{
					pnew = m_pKV->CreateNewKey();
					pnew->SetInt( NULL, inumber );
				}
				else
				{
					pnew = m_pKV->CreateNewKey();
					pnew->SetFloat( NULL, ( float ) number );
				}
				break;

			case Value::VT_String:
				pnew = m_pKV->CreateNewKey();
				pnew->SetString( NULL, val.GetString() );
				break;

			case Value::VT_Object:
				AddObject( val );
				break;

			case Value::VT_Array:
				AddArray( val );
				break;

			default:
				break;
		}

		if ( pnew )
			pnew->SetName( FLASH_ARRAY_ELEMENT_ID );

	}

};

class FlashObjectVisitor: public Value::ObjectVisitor
{
public:
	KeyValues* m_pKV;
	const Value* m_pOriginalValue;

	void AddObject( const char* name, const Value& val )
	{
		KeyValues* pnew = m_pKV->CreateNewKey();
		pnew->SetName( name );

		FlashObjectVisitor visitor;
		visitor.m_pKV = pnew;
		visitor.m_pOriginalValue = &val;

		val.VisitMembers( &visitor );

		if ( pnew->IsEmpty() )
		{
			m_pKV->RemoveSubKey( pnew );
		}
	}

	void AddArray( const char* name, const Value& val )
	{
		KeyValues* pnew = m_pKV->CreateNewKey();
		pnew->SetName( FLASH_ARRAY_ID );
		pnew->SetString( FLASH_ARRAY_NAME_ID, name );

		FlashArrayVisitor visitor;

		visitor.m_pKV = pnew;
		visitor.m_pOriginalValue = &val;

		val.VisitElements( &visitor );

		if ( pnew->IsEmpty() )
		{
			m_pKV->RemoveSubKey( pnew );
		}
	}

	void Visit( const char* name, const Value& val )
	{
		if ( *m_pOriginalValue == val )
		{
			return;
		}

		SF::Double number;
		int inumber;

		switch ( val.GetType() )
		{
			case Value::VT_Boolean:
				m_pKV->SetBool( name, val.GetBool() );
				break;

			case Value::VT_Number:
				number = val.GetNumber();
				inumber = ( int ) number;

				if ( ( SF::Double ) inumber == number )
				{
					m_pKV->SetInt( name, inumber );
				}
				else
				{
					m_pKV->SetFloat( name, ( float ) number );
				}
				break;

			case Value::VT_String:
				m_pKV->SetString( name, val.GetString() );
				break;

			case Value::VT_Object:
				AddObject( name, val );
				break;

			case Value::VT_Array:
				AddArray( name, val );
				break;

			default:
				break;
		}

	}
};

void FlashArrayVisitor::AddObject( const Value& val )
{
	KeyValues* pnew = m_pKV->CreateNewKey();
	pnew->SetName( FLASH_ARRAY_ELEMENT_ID );

	FlashObjectVisitor visitor;
	visitor.m_pKV = pnew;
	visitor.m_pOriginalValue = &val;

	val.VisitMembers( &visitor );

	if ( pnew->IsEmpty() )
	{
		m_pKV->RemoveSubKey( pnew );
	}
}

BaseSlot::BaseSlot()
{
	m_pMovieDef = NULL;
	m_pMovieView = NULL;
	m_iRefCount = 1;
	m_iSlot = -1;
	m_fChangeControllerTimeout = 0;
	m_advanceCount = 0;
}

bool BaseSlot::ConvertValue( Value *value, KeyValues* kv )
{
	bool result = true;

	switch ( kv->GetDataType() )
	{
		case KeyValues::TYPE_STRING:
			m_pMovieView->CreateString( value, kv->GetString() );
			break;

		case KeyValues::TYPE_INT:
			value->SetNumber( ( SF::Double ) kv->GetInt() );
			break;

		case KeyValues::TYPE_FLOAT:
			value->SetNumber( ( SF::Double ) kv->GetFloat() );
			break;

		case KeyValues::TYPE_WSTRING:
			m_pMovieView->CreateStringW( value, kv->GetWString() );
			break;

		case KeyValues::TYPE_UINT64:
			value->SetNumber( ( SF::Double ) kv->GetUint64() );
			break;

		default:
			result = false;
			break;
	}

	return result;

}

const char* BaseSlot::PopulateArray( Value* value, KeyValues* kv )
{
	m_pMovieView->CreateArray( value );

	Value workValue;

	// count the number of array slots needed
	int numElements = 0;

	for ( KeyValues* piter = kv->GetFirstSubKey(); piter; piter = piter->GetNextKey() )
	{
		if ( Q_strcasecmp( FLASH_ARRAY_NAME_ID, piter->GetName() ) )
			numElements++;
	}

	value->SetArraySize( numElements );

	int currentElement = 0;

	for ( KeyValues* piter = kv->GetFirstSubKey(); piter; piter = piter->GetNextKey() )
	{
		// skip the element that defines the array object name
		if ( Q_strcasecmp( FLASH_ARRAY_NAME_ID, piter->GetName() ) )
		{
			if ( !Q_strcasecmp( FLASH_ARRAY_ID, piter->GetName() ) )
			{
				PopulateArray( &workValue, piter );
			}
			else if ( !ConvertValue( &workValue, piter ) )
			{
				PopulateObject( &workValue, piter );
			}

			value->SetElement( currentElement, workValue );
			workValue.SetNull();
			currentElement++;
		}
	}

	return kv->GetString( FLASH_ARRAY_NAME_ID, NULL );

}

void BaseSlot::PopulateObject( Value* value, KeyValues* kv )
{
	m_pMovieView->CreateObject( value );

	Value workValue;

	// first iterate through the values at this level and assign them to the m_pObject
	for ( KeyValues* piter = kv->GetFirstValue(); piter; piter = piter->GetNextValue() )
	{
		if ( ConvertValue( &workValue, piter ) )
		{
			value->SetMember( piter->GetName(), workValue );
			workValue.SetNull();
		}
	}

	// now make the sub objects

	for ( KeyValues* piter = kv->GetFirstTrueSubKey(); piter; piter = piter->GetNextTrueSubKey() )
	{
		if ( !Q_strcasecmp( FLASH_ARRAY_ID, piter->GetName() ) )
		{
			const char* name = PopulateArray( &workValue, piter );

			value->SetMember( name, workValue );
		}
		else
		{

			m_pMovieView->CreateObject( &workValue );

			PopulateObject( &workValue, piter );

			value->SetMember( piter->GetName(), workValue );
		}

		workValue.SetNull();
	}

}

void BaseSlot::LoadKVFile( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	const char* filename = params->pArgs[0].GetString();

	const char* section = filename;

	if ( params->ArgCount > 1 )
	{
		section = params->pArgs[1].GetString();
	}

	const char* startDir = "GAME";

	if ( params->ArgCount > 2 )
	{
		startDir = params->pArgs[2].GetString();
	}

	// See if we don't want to load any dlc keys
	bool checkForDlcFiles = true;
	if ( params->ArgCount > 3 )
	{
		checkForDlcFiles = params->pArgs[3].GetBool();
	}

	KeyValues* pkeyValueData = new KeyValues( section );

	// Load the config data
	if ( pkeyValueData )
	{
		Value rootValue;

		pkeyValueData->LoadFromFile( g_pFullFileSystem, filename, startDir );

		// Add/update any key value changes from DLC files
		if ( checkForDlcFiles )
		{
			DLCHelper::AppendDLCKeyValues( pkeyValueData, filename, startDir );
		}

		PopulateObject( params->pRetVal, pkeyValueData );

		pkeyValueData->deleteThis();
	}

}

void BaseSlot::SaveKVFile( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	Value* pvalue = &params->pArgs[0];

	const char* filename = params->pArgs[1].GetString();

	const char* section = filename;

	if ( params->ArgCount > 2 )
	{
		section = params->pArgs[2].GetString();
	}

	const char* where = "GAME";

	if ( params->ArgCount > 3 )
	{
		where = params->pArgs[3].GetString();
	}

	KeyValues* pKV = new KeyValues( section );

	FlashObjectVisitor visitor;
	visitor.m_pKV = pKV;
	visitor.m_pOriginalValue = pvalue;

	pvalue->VisitMembers( &visitor );

	pKV->SaveToFile( g_pFullFileSystem, filename, where );

	pKV->deleteThis();

}

void BaseSlot::GetConvar( FunctionHandler::Params* params, CVAR_TYPE_WANTED typeWanted )
{
	bool resultValid = false;

	if ( params->ArgCount != 1 )
	{
		SFINST.LogPrintf( "scaleform: script called GetConvar* with %d args\n", params->ArgCount );
	}
	else
	{
		const char* convarname = params->pArgs[0].GetString();

		if ( !convarname || !*convarname )
		{
			SFINST.LogPrintf( "scaleform: script called GetConvar* with bad first argument\n" );
		}
		else
		{
			ConVarRef convar( convarname );

			if ( convar.IsValid() )
			{
				switch ( typeWanted )
				{
					case CVAR_WANT_NUMBER:
						params->pRetVal->SetNumber( convar.GetFloat() );
						resultValid = true;
						break;
					case CVAR_WANT_NUMBER_MIN:
						params->pRetVal->SetNumber( convar.GetMin() );
						resultValid = true;
						break;
					case CVAR_WANT_NUMBER_MAX:
						params->pRetVal->SetNumber( convar.GetMax() );
						resultValid = true;
						break;

					case CVAR_WANT_BOOL:
						params->pRetVal->SetBoolean( convar.GetBool() );
						resultValid = true;
						break;

					case CVAR_WANT_STRING:
						if ( !convar.IsFlagSet( FCVAR_NEVER_AS_STRING ) )
						{
							const char* str = convar.GetString();

							if ( str )
							{
								m_pMovieView->CreateString( params->pRetVal, str );
								resultValid = true;
							}
						}
						break;

					default:
						break;
				}
			}
		}
	}

	if ( !resultValid )
	{
		params->pRetVal->SetNull();
	}
}

void BaseSlot::GetConvarNumber( SCALEFORM_CALLBACK_ARGS_DECL )
{
	GetConvar( FromSFPARAMS( obj ), CVAR_WANT_NUMBER );
}

void BaseSlot::GetConvarNumberMin( SCALEFORM_CALLBACK_ARGS_DECL )
{
	GetConvar( FromSFPARAMS( obj ), CVAR_WANT_NUMBER_MIN );
}

void BaseSlot::GetConvarNumberMax( SCALEFORM_CALLBACK_ARGS_DECL )
{
	GetConvar( FromSFPARAMS( obj ), CVAR_WANT_NUMBER_MAX );
}

void BaseSlot::GetConvarString( SCALEFORM_CALLBACK_ARGS_DECL )
{
	GetConvar( FromSFPARAMS( obj ), CVAR_WANT_STRING );
}

void BaseSlot::GetConvarBoolean( SCALEFORM_CALLBACK_ARGS_DECL )
{
	GetConvar( FromSFPARAMS( obj ), CVAR_WANT_BOOL );
}

void BaseSlot::GetPlayerColorObject( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	int nSlot = 0;
	int nPos = 0;
	if ( !params->pArgs[0].IsUndefined() )
	{
		nSlot = params->pArgs[0].GetNumber();
		nPos = params->pArgs[1].GetNumber( );
	}
	else
	{
		Warning( "Actionscript passed an undefined param to BaseSlot::GetPlayerColorObject!\n" );
	}

	if ( nSlot < 0 || nSlot > 4 )
		return;

	int r = 0;
	int g = 0;
	int b = 0;
	switch ( nSlot )
	{
		case 0:
		{
				  static ConVarRef cl_teammate_color_1( "cl_teammate_color_1" );
				  sscanf( cl_teammate_color_1.GetString(), "%i %i %i", &r, &g, &b );
				  break;
		}
		case 1:
		{
				  static ConVarRef cl_teammate_color_2( "cl_teammate_color_2" );
				  sscanf( cl_teammate_color_2.GetString(), "%i %i %i", &r, &g, &b );
				  break;
		}
		case 2:
		{
				  static ConVarRef cl_teammate_color_3( "cl_teammate_color_3" );
				  sscanf( cl_teammate_color_3.GetString(), "%i %i %i", &r, &g, &b );
				  break;
		}
		case 3:
		{
				  static ConVarRef cl_teammate_color_4( "cl_teammate_color_4" );
				  sscanf( cl_teammate_color_4.GetString(), "%i %i %i", &r, &g, &b );
				  break;
		}
		case 4:
		{
				  static ConVarRef cl_teammate_color_5( "cl_teammate_color_5" );
				  sscanf( cl_teammate_color_5.GetString(), "%i %i %i", &r, &g, &b );
				  break;
		}
	}

	if ( nPos == 0 )
	{
		params->pRetVal->SetNumber( r );
	}
	else if ( nPos == 1 )
	{
		params->pRetVal->SetNumber( g );
	}
	else if ( nPos == 2 )
	{
		params->pRetVal->SetNumber( b );
	}
}

void BaseSlot::GetPAXAvatarFromName( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	if ( params->ArgCount != 1 )
	{
		SFINST.LogPrintf( "scaleform: script called GetPAXAvatarFromName with %d args\n", params->ArgCount );
	}
	else
	{
		const char* playerName = params->pArgs[0].GetString();

		if ( !playerName || !*playerName )
		{
			SFINST.LogPrintf( "scaleform: script called GetPAXAvatarFromName with bad first argument\n" );
		}
		else
		{
			int nAvatarIndex = V_atoi( playerName + ( V_strlen( playerName ) - 1 ) );
			

			char szAvatar[256];
			V_snprintf( szAvatar, ARRAYSIZE( szAvatar ), "demo_avatars/CSGO_AVATAR_%d.dds", nAvatarIndex );	
			m_pMovieView->CreateString( params->pRetVal, szAvatar );
		}
	}
}

void BaseSlot::SetConvar( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	if ( params->ArgCount != 2 )
	{
		SFINST.LogPrintf( "scaleform: script called setconver with %d args\n", params->ArgCount );
	}
	else
	{

		const char* convarname = params->pArgs[0].GetString();

		if ( !convarname || !*convarname )
		{
			SFINST.LogPrintf( "scaleform: script called setconver with bad first argument\n" );
		}
		else
		{

			ConVarRef varref( convarname );

			switch ( params->pArgs[1].GetType() )
			{

				case Value::VT_Boolean:
					varref.SetValue( params->pArgs[1].GetBool() );
					break;

				case Value::VT_Number:
					varref.SetValue( ( float ) params->pArgs[1].GetNumber() );
					break;

				case Value::VT_String:
					varref.SetValue( params->pArgs[1].GetString() );
					break;

				default:
					SFINST.LogPrintf( "scaleform: illegal attempt to set %s to type number# 0x%x\n", convarname, params->pArgs[1].GetType() );
					break;

			}

#if defined( DEBUG )
			if ( varref.IsValid() )
			{
				SFINST.LogPrintf( "scaleform: set convar %s to %s\n", convarname, varref.GetString() );
			}
			else
			{
				SFINST.LogPrintf( "scaleform: convar %s is not valid\n", convarname );
			}
#endif

		}
	}
}

void BaseSlot::Translate( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	const char* value = NULL;
	if ( !params->pArgs[0].IsUndefined() )
	{
		value = params->pArgs[0].GetString();
	}
	else
	{
		Warning( "Actionscript passed an undefined param to BaseSlot::Translate!\n" );
	}

	if ( value && *value == '#' )
	{
		m_pMovieView->CreateStringW( params->pRetVal, SFINST.Translate( value, NULL ) );
	}
	else
	{
		*( params->pRetVal ) = params->pArgs[0];
	}
}

void BaseSlot::ReplaceGlyphs( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	const char* text = NULL;
	const wchar_t* wtext = NULL;
	int fontSize = 0;

	if ( params->pArgs[0].IsStringW() )
	{
		wtext = params->pArgs[0].GetStringW();
	}
	else
	{
		text = params->pArgs[0].GetString();
	}

	if ( params->ArgCount == 2 )
	{
		fontSize = ( int )params->pArgs[1].GetNumber();
	}

	const wchar_t* result;

	if ( text )
	{
 		result = SFINST.ReplaceGlyphKeywordsWithHTML( text, fontSize );
	}
	else
	{
		result = SFINST.ReplaceGlyphKeywordsWithHTML( wtext, fontSize );
	}

	m_pMovieView->CreateStringW( params->pRetVal, result );

}

void BaseSlot::GetClipboardText( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int size = g_pVGuiSystem->GetClipboardTextCount();
	if ( size )
	{
		wchar_t* buffer = new wchar_t[size];
		g_pVGuiSystem->GetClipboardText( 0, buffer, size * sizeof( wchar_t ) );

		m_pMovieView->CreateStringW( FromSFPARAMS( obj )->pRetVal, buffer );

		delete[] buffer;
	}
	else
	{
		FromSFPARAMS( obj )->pRetVal->SetNull();
	}

}

void BaseSlot::SetClipboardText( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );
	
	if ( params->ArgCount > 0 ) 
	{
		if ( params->pArgs[0].IsStringW() )
		{
			const wchar_t* pString = params->pArgs[0].GetStringW();
			if ( pString )
			{
				int len = V_wcslen( pString );
				g_pVGuiSystem->SetClipboardText( pString, len+1 );
			}
		}
		else if ( params->pArgs[0].IsString() )
		{
			const char* pString = params->pArgs[0].GetString();
			if ( pString )
			{
				int len = strlen( pString );
				g_pVGuiSystem->SetClipboardText( pString, len+1 );
			}
		}
	}
}


void BaseSlot::MakeStringSafe( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	const char* intext = NULL;

	if ( params && params->ArgCount > 0 )
		intext = params->pArgs[0].GetString();

	if ( !intext )
	{
		m_pMovieView->CreateStringW( params->pRetVal, L"" );
		return;
	}

	int originalStringLength = V_strlen( intext )+1;
	wchar_t* pFirstBuffer = new wchar_t[originalStringLength];
		
	V_UTF8ToUnicode( intext, pFirstBuffer, originalStringLength*sizeof( wchar_t ) );

	wchar_t* pSecondBuffer = new wchar_t[originalStringLength*15];

	SFINST.MakeStringSafe( pFirstBuffer, pSecondBuffer, originalStringLength * sizeof( wchar_t )*15 );

	m_pMovieView->CreateStringW( params->pRetVal, pSecondBuffer );

	delete[] pFirstBuffer;
	delete[] pSecondBuffer;
}


void BaseSlot::ConsoleCommand( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	const char *command = params->pArgs[0].GetString();

	SFINST.GetEnginePtr()->ClientCmd( command );
}

void BaseSlot::ConsoleCommandExecute( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	const char *command = params->pArgs[0].GetString();

	SFINST.GetEnginePtr()->ExecuteClientCmd( command );
}

void BaseSlot::SendUIEvent( SCALEFORM_CALLBACK_ARGS_DECL )
{
	FunctionHandler::Params* params = FromSFPARAMS( obj );

	SFINST.SendUIEvent( params->pArgs[0].GetString(), params->pArgs[1].GetString(), m_iSlot);
}


void BaseSlot::UpdateSafeZone( void )
{
	m_GlobalValue.Invoke( "UpdateSafeZone", 0 );
}

bool BaseSlot::SetToControllerUI( bool value, bool isKeyOrButtonPress )
{
	if ( value && g_pInputSystem->GetCurrentInputDevice() == INPUT_DEVICE_STEAM_CONTROLLER )
	{
		// When we are using a steam controller we never want to use
		// controller UI.
		value = false;
	}

#if defined WIN32
	if ( g_pInputSystem->IsSamplingForCurrentDevice( ) && isKeyOrButtonPress )
	{
		if( value )
		{
			g_pInputSystem->SetCurrentInputDevice( INPUT_DEVICE_GAMEPAD );
			ConVarRef var( "joystick" );
			if( var.IsValid( ) )
				var.SetValue( 1 );
		}
		else
		{
			g_pInputSystem->SetCurrentInputDevice( INPUT_DEVICE_KEYBOARD_MOUSE );
			ConVarRef var( "joystick" );
			if( var.IsValid( ) )
				var.SetValue( 0 );

		}
		g_pInputSystem->SampleInputToFindCurrentDevice( false );
	}
#endif  
	
	if ( value != m_bControllerUI )
	{
		if ( m_fChangeControllerTimeout <= 0 )
		{
			Value showem( value );
			m_bControllerUI = value;
			m_GlobalValue.Invoke( "SetToControllerUI", NULL, &showem, 1 );
			m_fChangeControllerTimeout = CHANGE_CONTROLLER_TIMEOUT;
			return true;
		}
	}

	return false;
}

bool BaseSlot::IsSetToControllerUI( void )
{
	return m_bControllerUI;
}

void BaseSlot::UpdateTint( void )
{
	m_GlobalValue.Invoke( "UpdateTint", 0 );
}

bool BaseSlot::HandleKeyEvent( bool keyDown, ButtonCode_t code, ButtonCode_t vkey, const char* binding, int slot )
{
	Value args[4];
	Value flashResult;
	flashResult.SetNull();
	
	args[0].SetNumber( ( SF::Double ) code );
	args[1].SetNumber( ( SF::Double ) vkey );
	args[2].SetNumber( ( SF::Double ) slot );
	if ( binding && *binding == '+' )
		binding++;
	args[3].SetString( binding );
	m_GlobalValue.Invoke( keyDown ? "KeyDownEvent" : "KeyUpEvent", &flashResult, args, 4 );

	if (flashResult.IsBool())
		return flashResult.GetBool();
	else
		return false;
}

bool BaseSlot::HandleCharTyped( const wchar_t* typed, int slot )
{
	Value args[2];
	Value result;
	result.SetNull();
	
	args[0].SetStringW( typed );
	args[1].SetNumber( ( SF::Double ) slot );

#if defined( _OSX )
	// [will] - HACK: Change Mac's backspace 127 (del) character into '\b' for chat window.
	if( *typed == 127 )
	{
		args[0].SetString( "\b" );
	}
#endif

	m_GlobalValue.Invoke( "CharTyped", &result, args, 2 );

	return !result.IsNull() && result.GetBool();
}


void BaseSlot::LockInputToSlot( int slot )
{
	Value args[1];
	args[0].SetNumber( ( SF::Double ) slot );
	m_GlobalValue.Invoke( "LockInputToSlot", NULL, args, 1 );
}

void BaseSlot::UnlockInput( void )
{
	m_GlobalValue.Invoke( "UnlockInput", NULL );
}

void BaseSlot::ForceCollectGarbage( void )
{
	if ( m_pMovieView )
		m_pMovieView->ForceCollectGarbage( );
}

void BaseSlot::CreateKeyTable()
{
	Value keyTable;
	m_pMovieView->CreateObject( &keyTable );
	m_GlobalValue.SetMember( "ValveKeyTable", keyTable );

#define KEYENTRY( value) \
	{																	\
		Value gfxv;														\
		char numberAsString[10];											\
																			\
		gfxv.SetNumber((SF::Double)(value));									\
		keyTable.SetMember(#value, gfxv);										\
																			\
		V_snprintf(numberAsString, sizeof(numberAsString), "%d", value);	\
		gfxv.SetString(#value);												\
		keyTable.SetMember(numberAsString, gfxv);                           \
	}


	KEYENTRY( KEY_NONE );
	KEYENTRY( KEY_0 );
	KEYENTRY( KEY_1 );
	KEYENTRY( KEY_2 );
	KEYENTRY( KEY_3 );
	KEYENTRY( KEY_4 );
	KEYENTRY( KEY_5 );
	KEYENTRY( KEY_6 );
	KEYENTRY( KEY_7 );
	KEYENTRY( KEY_8 );
	KEYENTRY( KEY_9 );
	KEYENTRY( KEY_A );
	KEYENTRY( KEY_B );
	KEYENTRY( KEY_C );
	KEYENTRY( KEY_D );
	KEYENTRY( KEY_E );
	KEYENTRY( KEY_F );
	KEYENTRY( KEY_G );
	KEYENTRY( KEY_H );
	KEYENTRY( KEY_I );
	KEYENTRY( KEY_J );
	KEYENTRY( KEY_K );
	KEYENTRY( KEY_L );
	KEYENTRY( KEY_M );
	KEYENTRY( KEY_N );
	KEYENTRY( KEY_O );
	KEYENTRY( KEY_P );
	KEYENTRY( KEY_Q );
	KEYENTRY( KEY_R );
	KEYENTRY( KEY_S );
	KEYENTRY( KEY_T );
	KEYENTRY( KEY_U );
	KEYENTRY( KEY_V );
	KEYENTRY( KEY_W );
	KEYENTRY( KEY_X );
	KEYENTRY( KEY_Y );
	KEYENTRY( KEY_Z );
	KEYENTRY( KEY_PAD_0 );
	KEYENTRY( KEY_PAD_1 );
	KEYENTRY( KEY_PAD_2 );
	KEYENTRY( KEY_PAD_3 );
	KEYENTRY( KEY_PAD_4 );
	KEYENTRY( KEY_PAD_5 );
	KEYENTRY( KEY_PAD_6 );
	KEYENTRY( KEY_PAD_7 );
	KEYENTRY( KEY_PAD_8 );
	KEYENTRY( KEY_PAD_9 );
	KEYENTRY( KEY_PAD_DIVIDE );
	KEYENTRY( KEY_PAD_MULTIPLY );
	KEYENTRY( KEY_PAD_MINUS );
	KEYENTRY( KEY_PAD_PLUS );
	KEYENTRY( KEY_PAD_ENTER );
	KEYENTRY( KEY_PAD_DECIMAL );
	KEYENTRY( KEY_LBRACKET );
	KEYENTRY( KEY_RBRACKET );
	KEYENTRY( KEY_SEMICOLON );
	KEYENTRY( KEY_APOSTROPHE );
	KEYENTRY( KEY_BACKQUOTE );
	KEYENTRY( KEY_COMMA );
	KEYENTRY( KEY_PERIOD );
	KEYENTRY( KEY_SLASH );
	KEYENTRY( KEY_BACKSLASH );
	KEYENTRY( KEY_MINUS );
	KEYENTRY( KEY_EQUAL );
	KEYENTRY( KEY_ENTER );
	KEYENTRY( KEY_SPACE );
	KEYENTRY( KEY_BACKSPACE );
	KEYENTRY( KEY_TAB );
	KEYENTRY( KEY_CAPSLOCK );
	KEYENTRY( KEY_NUMLOCK );
	KEYENTRY( KEY_ESCAPE );
	KEYENTRY( KEY_SCROLLLOCK );
	KEYENTRY( KEY_INSERT );
	KEYENTRY( KEY_DELETE );
	KEYENTRY( KEY_HOME );
	KEYENTRY( KEY_END );
	KEYENTRY( KEY_PAGEUP );
	KEYENTRY( KEY_PAGEDOWN );
	KEYENTRY( KEY_BREAK );
	KEYENTRY( KEY_LSHIFT );
	KEYENTRY( KEY_RSHIFT );
	KEYENTRY( KEY_LALT );
	KEYENTRY( KEY_RALT );
	KEYENTRY( KEY_LCONTROL );
	KEYENTRY( KEY_RCONTROL );
	KEYENTRY( KEY_LWIN );
	KEYENTRY( KEY_RWIN );
	KEYENTRY( KEY_APP );
	KEYENTRY( KEY_UP );
	KEYENTRY( KEY_LEFT );
	KEYENTRY( KEY_DOWN );
	KEYENTRY( KEY_RIGHT );
	KEYENTRY( KEY_F1 );
	KEYENTRY( KEY_F2 );
	KEYENTRY( KEY_F3 );
	KEYENTRY( KEY_F4 );
	KEYENTRY( KEY_F5 );
	KEYENTRY( KEY_F6 );
	KEYENTRY( KEY_F7 );
	KEYENTRY( KEY_F8 );
	KEYENTRY( KEY_F9 );
	KEYENTRY( KEY_F10 );
	KEYENTRY( KEY_F11 );
	KEYENTRY( KEY_F12 );
	KEYENTRY( KEY_CAPSLOCKTOGGLE );
	KEYENTRY( KEY_NUMLOCKTOGGLE );
	KEYENTRY( KEY_SCROLLLOCKTOGGLE );

	KEYENTRY( MOUSE_LEFT );
	KEYENTRY( MOUSE_RIGHT );
	KEYENTRY( MOUSE_MIDDLE );
	KEYENTRY( MOUSE_4 );
	KEYENTRY( MOUSE_5 );
	KEYENTRY( MOUSE_WHEEL_UP );
	KEYENTRY( MOUSE_WHEEL_DOWN );
	
	
	KEYENTRY( KEY_XBUTTON_UP );
	KEYENTRY( KEY_XBUTTON_RIGHT );
	KEYENTRY( KEY_XBUTTON_DOWN );
	KEYENTRY( KEY_XBUTTON_LEFT );

	KEYENTRY( KEY_XBUTTON_A );
	KEYENTRY( KEY_XBUTTON_B );
	KEYENTRY( KEY_XBUTTON_X );
	KEYENTRY( KEY_XBUTTON_Y );
	KEYENTRY( KEY_XBUTTON_LEFT_SHOULDER );
	KEYENTRY( KEY_XBUTTON_RIGHT_SHOULDER );
	KEYENTRY( KEY_XBUTTON_BACK );
	KEYENTRY( KEY_XBUTTON_START );
	KEYENTRY( KEY_XBUTTON_STICK1 );
	KEYENTRY( KEY_XBUTTON_STICK2 );
	KEYENTRY( KEY_XBUTTON_INACTIVE_START );

	KEYENTRY( KEY_XSTICK1_RIGHT ); 
	KEYENTRY( KEY_XSTICK1_LEFT );
	KEYENTRY( KEY_XSTICK1_DOWN );
	KEYENTRY( KEY_XSTICK1_UP );
	KEYENTRY( KEY_XBUTTON_LTRIGGER );
	KEYENTRY( KEY_XBUTTON_RTRIGGER );
	KEYENTRY( KEY_XSTICK2_RIGHT );
	KEYENTRY( KEY_XSTICK2_LEFT );
	KEYENTRY( KEY_XSTICK2_DOWN );
	KEYENTRY( KEY_XSTICK2_UP );

	KEYENTRY( KEY_XBUTTON_FIREMODE_SELECTOR_1 );
	KEYENTRY( KEY_XBUTTON_FIREMODE_SELECTOR_2 );
	KEYENTRY( KEY_XBUTTON_FIREMODE_SELECTOR_3 );
	KEYENTRY( KEY_XBUTTON_RELOAD );
	KEYENTRY( KEY_XBUTTON_TRIGGER );
	KEYENTRY( KEY_XBUTTON_PUMP_ACTION );
	KEYENTRY( KEY_XBUTTON_ROLL_RIGHT );
	KEYENTRY( KEY_XBUTTON_ROLL_LEFT );


}

void BaseSlot::Init( const char* movieName, int slot )
{
	SFDevMsg("BaseSlot::Init(%s,%d)\n", movieName, slot);
	m_iSlot = slot;
	m_pMovieDef = FromSFMOVIEDEF( SFINST.CreateMovieDef( movieName ) );
	if ( m_pMovieDef )
	{
		m_pMovieView = FromSFMOVIE( SFINST.MovieDef_CreateInstance( ToSFMOVIEDEF( m_pMovieDef ), true ) );
		m_pMovieView->SetViewScaleMode( Movie::SM_NoScale );
		m_pMovieView->SetViewAlignment( Movie::Align_TopLeft );

		m_pMovieView->GetVariable( &m_GlobalValue, "_global" );

		if ( slot == SF_FULL_SCREEN_SLOT )		// main menu slot receives IME focus on startup
		{
			m_pMovieView->HandleEvent( Event::SetFocus );
		}

		Value platformCode;
		m_bControllerUI = true;

		if ( IsX360() )
		{
			platformCode.SetNumber( 1 );
		}
		else if ( IsPS3() || SFINST.GetForcePS3() )
		{
			platformCode.SetNumber( 2 );
		}
		else if ( IsOSX() )
		{
			platformCode.SetNumber( 3 );
			m_bControllerUI = false;
		}
		else
		{
			platformCode.SetNumber( 0 );
			m_bControllerUI = false;
		}

		if ( !m_bControllerUI )
			m_bControllerUI = ( m_iSlot != SF_FULL_SCREEN_SLOT ) && ( ( m_iSlot - SF_FIRST_SS_SLOT ) != 0 );

		m_GlobalValue.SetMember( "PlatformCode", platformCode );

		// we will have to revisit this when the keyboard can be assigned to player 2
		m_GlobalValue.SetMember( "wantControllerShown", m_bControllerUI );

		Value number;
		number.SetNumber( slot );
		m_GlobalValue.SetMember( "UISlot", number );

		const ScaleformUIFunctionHandlerDefinition* table = GetSlotAPITable();

		if ( table )
		{

			m_pMovieView->CreateObject( &m_GameAPI );

			while ( table->m_pName != NULL )
			{
				SFINST.AddAPIFunctionToObject( ToSFVALUE( &m_GameAPI ), ToSFMOVIE( m_pMovieView ), reinterpret_cast<ScaleformUIFunctionHandlerObject*> ( this ), table );
				table++;
			}

			m_GlobalValue.SetMember( "GameInterface", m_GameAPI );
			UpdateTint();
		}

		CreateKeyTable();

		m_pMovieView->SetBackgroundAlpha( 0 );

		Advance( 0 );

		Value initFunc;
		initFunc.SetNull();

		if (m_GlobalValue.GetMember( "InitSlot", &initFunc ) )
		{
			m_GlobalValue.Invoke( "InitSlot" );
		}

		initFunc.SetNull();

		m_pMovieView->SetViewport( SFINST.GetScreenWidth(), SFINST.GetScreenHeight(), 0, 0, SFINST.GetScreenWidth(), SFINST.GetScreenHeight() );
		m_GlobalValue.Invoke( "ForceResize" );
	}
	else
	{
		// Movie is corrupt or signature mismatch (pcbeta rel)
		Error( "Error loading %s!\n", movieName );
	}
}

void BaseSlot::Unload( void )
{
	SFDevMsg("BaseSlot::Unload slot=%d\n", m_iSlot);
	if ( m_pMovieView )
	{
		// Last-chance function before the slot is unloaded
		// if we're not rendering scaleform, ShutdownSlot may not have a value.
		if ( m_GlobalValue.HasMember( "ShutdownSlot") )
			m_GlobalValue.Invoke( "ShutdownSlot" );

		// Unload all global objects
		for ( UtlSymId_t sym = m_mapGlobalObjects.Head(); sym != m_mapGlobalObjects.InvalidIndex(); sym = m_mapGlobalObjects.Next( sym ) )
		{
			if ( m_mapGlobalObjects[ sym ] )
			{
				Value value;
				value.SetNull();
				m_GlobalValue.SetMember( m_mapGlobalObjects.String( sym ), value );

				m_mapGlobalObjects[sym]->SetNull();
				delete m_mapGlobalObjects[sym];
				m_mapGlobalObjects[sym] = NULL;
			}
		}
		m_mapGlobalObjects.Purge();

		// Unset gameinterface
		Value value;
		value.SetNull();
		m_GlobalValue.SetMember( "GameInterface", value );		

		m_GameAPI.SetNull();
		m_GlobalValue.SetNull();

		SFINST.ReleaseMovieView( ToSFMOVIE( m_pMovieView ) );
		SFINST.ReleaseMovieDef( ToSFMOVIEDEF( m_pMovieDef ) );

		m_pMovieView = NULL;
		m_pMovieDef = NULL;
	}
	m_iSlot = -1;
}

void BaseSlot::AddRef( void )
{
	ThreadInterlockedIncrement( &m_iRefCount );	
}

bool BaseSlot::Release( void )
{
	ThreadInterlockedDecrement( &m_iRefCount );

	bool result = ( m_iRefCount == 0 );

	if ( result )
	{
		Unload();
		delete this;
	}

	return result;
}

void BaseSlot::Advance( float time )
{
	const int kNumCatchupFrames = 0;

	m_pMovieView->Advance( time, kNumCatchupFrames );

	m_fChangeControllerTimeout -= time;
}

void BaseSlot::LockMostRecentInputDevice( void )
{
	if( m_bControllerUI )
	{
		g_pInputSystem->SetCurrentInputDevice( INPUT_DEVICE_GAMEPAD );
		ConVarRef var( "joystick" );
		if( var.IsValid( ) )
			var.SetValue( 1 );
	}
	else
	{
		g_pInputSystem->SetCurrentInputDevice( INPUT_DEVICE_KEYBOARD_MOUSE );
		ConVarRef var( "joystick" );
		if( var.IsValid( ) )
			var.SetValue( 0 );
	}
}

/***********************************************************************************************************/

static const ScaleformUIFunctionHandlerDefinition rootGameAPITable[] = {
		{ "AddInputConsumer", ToScaleformUIFunctionHandler ( &MovieSlot::AddInputConsumer ) },
		{ "RemoveInputConsumer", ToScaleformUIFunctionHandler ( &MovieSlot::RemoveInputConsumer ) },
		{ "SetCursorShape", ToScaleformUIFunctionHandler ( &MovieSlot::SetCursorShape ) },
		{ "ShowCursor", ToScaleformUIFunctionHandler ( &MovieSlot::ShowCursor ) },
		{ "HideCursor", ToScaleformUIFunctionHandler ( &MovieSlot::HideCursor ) },
		{ "LoadKVFile", ToScaleformUIFunctionHandler ( &MovieSlot::LoadKVFile ) },
		{ "SaveKVFile", ToScaleformUIFunctionHandler ( &MovieSlot::SaveKVFile ) },
		{ "SetConvar", ToScaleformUIFunctionHandler ( &MovieSlot::SetConvar ) },
		{ "GetConvarNumber", ToScaleformUIFunctionHandler ( &MovieSlot::GetConvarNumber ) },
		{ "GetConvarNumberMin", ToScaleformUIFunctionHandler ( &MovieSlot::GetConvarNumberMin ) },
		{ "GetConvarNumberMax", ToScaleformUIFunctionHandler ( &MovieSlot::GetConvarNumberMax ) },
		{ "GetConvarString", ToScaleformUIFunctionHandler ( &MovieSlot::GetConvarString ) },
		{ "GetConvarBoolean", ToScaleformUIFunctionHandler ( &MovieSlot::GetConvarBoolean ) },
		{ "GetPAXAvatarFromName", ToScaleformUIFunctionHandler ( &MovieSlot::GetPAXAvatarFromName ) },
		{ "GetPlayerColorObject", ToScaleformUIFunctionHandler ( &MovieSlot::GetPlayerColorObject ) },
		{ "Translate", ToScaleformUIFunctionHandler ( &MovieSlot::Translate ) },
		{ "ReplaceGlyphs", ToScaleformUIFunctionHandler ( &MovieSlot::ReplaceGlyphs ) },
		{ "PlaySound", ToScaleformUIFunctionHandler ( &MovieSlot::PlaySoundScaleform ) },
		{ "ConsoleCommand", ToScaleformUIFunctionHandler ( &MovieSlot::ConsoleCommand ) },
		{ "ConsoleCommandExecute", ToScaleformUIFunctionHandler ( &MovieSlot::ConsoleCommandExecute ) },
		{ "DisableAnalogStickNavigation", ToScaleformUIFunctionHandler ( &MovieSlot::DisableAnalogStickNavigation ) },
		{ "DenyInputToGame", ToScaleformUIFunctionHandler ( &MovieSlot::DenyInputToGameFromFlash ) },
		{ "SendUIEvent", ToScaleformUIFunctionHandler ( &MovieSlot::SendUIEvent ) },
		{ "MakeStringSafe", ToScaleformUIFunctionHandler ( &MovieSlot::MakeStringSafe ) },
		{ "GetClipboardText", ToScaleformUIFunctionHandler ( &MovieSlot::GetClipboardText ) },
		{ "SetClipboardText", ToScaleformUIFunctionHandler ( &MovieSlot::SetClipboardText ) },

		{ NULL, NULL } };

const ScaleformUIFunctionHandlerDefinition* MovieSlot::GetSlotAPITable( void )
{
	return rootGameAPITable;
}

MovieSlot::MovieSlot() :
	BaseSlot()
{
	m_iNumInputConsumers = 0;
	m_bDisableAnalogNavigation = false;
}

void MovieSlot::AddInputConsumer( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_iNumInputConsumers++;
}

void MovieSlot::RemoveInputConsumer( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( m_iNumInputConsumers <= 0 )
		DebuggerBreakIfDebugging();

	m_iNumInputConsumers = ( m_iNumInputConsumers <= 1 ) ? 0 : m_iNumInputConsumers--;
}

void MovieSlot::SetCursorShape( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// get the cursor shape index
	int i = ( int ) pui->Params_GetArgAsNumber( obj, 0 );
	SFINST.SetCursorShape( i );

}

void MovieSlot::ShowCursor( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( SFINST.IsSlotKeyboardAccessible( m_iSlot ) )
		SFINST.ShowCursor();
}

void MovieSlot::HideCursor( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( SFINST.IsSlotKeyboardAccessible( m_iSlot ) )
		SFINST.HideCursor();
}

#if defined( _PS3 )
void MovieSlot::PS3UseMoveCursor( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( SFINST.IsSlotKeyboardAccessible( m_iSlot ) )
		SFINST.PS3UseMoveCursor();
}

void MovieSlot::PS3UseStandardCursor( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( SFINST.IsSlotKeyboardAccessible( m_iSlot ) )
		SFINST.PS3UseStandardCursor();
}
void MovieSlot::PS3ForceCursorStart( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( SFINST.IsSlotKeyboardAccessible( m_iSlot ) )
		SFINST.PS3ForceCursorStart();
}
void MovieSlot::PS3ForceCursorEnd( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( SFINST.IsSlotKeyboardAccessible( m_iSlot ) )
		SFINST.PS3ForceCursorEnd();
}
#endif

void MovieSlot::PlaySoundScaleform( SCALEFORM_CALLBACK_ARGS_DECL )
{
	const char* soundname = pui->Params_GetArgAsString( obj, 0 );	
	vgui::surface()->PlaySound( soundname );
}

void MovieSlot::DenyInputToGameFromFlash( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bDenyInput = pui->Params_GetArgAsBool( obj, 0 );
	SFINST.DenyInputToGameFromFlash( m_iSlot, bDenyInput );
}

void MovieSlot::DisableAnalogStickNavigation( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bDisable = true;

	if ( pui->Params_GetNumArgs( obj ) > 0 )
	{
		bDisable = pui->Params_GetArgAsBool( obj, 0 );
	}

	m_bDisableAnalogNavigation = bDisable;
}


bool MovieSlot::AnalogStickNavigationDisabled( void )
{
	return m_bDisableAnalogNavigation;
}

void MovieSlot::RequestElement( const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject )
{
	if ( m_pMovieView )
	{
		Value args[2];

		m_pMovieView->CreateString( &args[0], elementName );
		m_pMovieView->CreateObject( &args[1] );

		const ScaleformUIFunctionHandlerDefinition* table = tableObject->GetTable();

		while ( table->m_pName != NULL )
		{
			SFINST.AddAPIFunctionToObject( ToSFVALUE( &args[1] ), ToSFMOVIE( m_pMovieView ), object, table );
			table++;
		}

		// now that we've got the api, call flash's load element

		m_GlobalValue.Invoke( "RequestElement", NULL, args, 2 );

	}
}

void MovieSlot::RemoveElement( Value* element )
{
	if ( m_pMovieView )
	{
		m_GlobalValue.Invoke( "RemoveElement", NULL, element, 1 );
	}
}

void MovieSlot::InstallGlobalObject( const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject, SF::GFx::Value* *pInstalledGlobalObjectResult )
{
	if ( m_mapGlobalObjects.Defined( elementName ) && m_mapGlobalObjects[elementName] )
	{
		Error( "SF Global Object %s is already installed!\n", elementName );
		return;
	}

	const ScaleformUIFunctionHandlerDefinition* table = tableObject->GetTable();
	if ( !table || !object )
	{
		Error( "SF Global Object %s has NULL table!\n", elementName );
		return;
	}

	Value *pValue = new Value;
	m_pMovieView->CreateObject( pValue );

	while ( table->m_pName != NULL )
	{
		SFINST.AddAPIFunctionToObject( ToSFVALUE( pValue ), ToSFMOVIE( m_pMovieView ), object, table );
		table++;
	}

	m_GlobalValue.SetMember( elementName, *pValue );
	m_mapGlobalObjects[ elementName ] = pValue;

	*pInstalledGlobalObjectResult = pValue;
}

void MovieSlot::RemoveGlobalObject( SF::GFx::Value* element )
{
	for ( UtlSymId_t sym = m_mapGlobalObjects.Head(); sym != m_mapGlobalObjects.InvalidIndex(); sym = m_mapGlobalObjects.Next( sym ) )
	{
		if ( m_mapGlobalObjects[ sym ] == element )
		{
			Value value;
			value.SetNull();
			m_GlobalValue.SetMember( m_mapGlobalObjects.String( sym ), value );		

			m_mapGlobalObjects[sym]->SetNull();
			delete m_mapGlobalObjects[sym];
			m_mapGlobalObjects[sym] = NULL;
			return;
		}
	}

	Error( "Attempted to remove SF Global Object which is not installed!\n" );
}


/***************************************************************************************************/

void CursorSlot::SetCursorShape( int shape )
{
	Value newShape;

	newShape.SetNumber( shape );

	m_GlobalValue.Invoke( "SetCursorShape", NULL, &newShape, 1 );
}

void CursorSlot::Hide( void )
{
	if ( !m_bUIHidden )
	{
		m_bUIHidden = true;
		m_GlobalValue.Invoke( "Hide", NULL );
	}
}

void CursorSlot::Show( void )
{
	if ( m_bUIHidden )
	{
		m_bUIHidden = false;
		m_GlobalValue.Invoke( "Show", NULL );
	}
}



