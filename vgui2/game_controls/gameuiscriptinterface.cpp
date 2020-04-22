//===== Copyright ©            Valve Corporation, All rights reserved. ======//
//
// Purpose: Defines scripting system.
//
//===========================================================================//

#include "gameuiscriptInterface.h"
#include "gameuisystemmgr.h"
#include "gameuidefinition.h"
#include "gamelayer.h"
#include "gamegraphic.h"
#include "fmtstr.h"
#include "gameuiscript.h"
#include "gameuisystem.h"
#include "gametext.h"
#include "dynamicrect.h"

#include "tier2/tier2.h"
#include "matchmaking/imatchframework.h"

BEGIN_SCRIPTDESC_ROOT_NAMED( CGameUIScriptInterface, "CGameUIScriptInterface", SCRIPT_SINGLETON "" )
// HSCRIPT table-kv functions
DEFINE_SCRIPTFUNC( LoadMenu, "LoadMenu( name, {params} ) : Load a menu." )
DEFINE_SCRIPTFUNC( CreateGraphic, "CreateGraphic( classname, {params} ) : Create a graphic." )
DEFINE_SCRIPTFUNC( CallScript, "CallScript( scripthandle, function, {params} ) : Execute other script function (scripthandle=0 will run local script)." )
DEFINE_SCRIPTFUNC( CallGraphic, "CallGraphic( graphichandle, commandname, {params} ) : Execute a graphic function." )
DEFINE_SCRIPTFUNC( Nugget, "Nugget( action, {params} ) : Interface with nuggets." )
END_SCRIPTDESC()


static ConVar ui_script_spew_level( "ui_script_spew_level", 0, FCVAR_DEVELOPMENTONLY );


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameUIScriptInterface::CGameUIScriptInterface( IScriptVM *pScriptVM, CGameUIDefinition *pDef  ) :
	m_Nuggets( UtlStringLessFunc ),
	m_GraphicScriptInstances( UtlStringLessFunc ),
	m_pScriptVM( pScriptVM ),
	m_pMenu( pDef )
{
	m_Scope = m_pScriptVM->RegisterInstance( this, "c" );
}

CGameUIScriptInterface::~CGameUIScriptInterface()
{
	Shutdown();
}

void CGameUIScriptInterface::Shutdown()
{
	// Unload all nuggets
	for ( unsigned short k = m_Nuggets.FirstInorder(); k != m_Nuggets.InvalidIndex(); k = m_Nuggets.NextInorder( k ) )
	{
		IGameUIScreenController *pNugget = m_Nuggets.Element( k );
		pNugget->OnScreenDisconnected( m_pMenu->GetGameUISystem() );
	}
	m_Nuggets.Purge();
}

//-----------------------------------------------------------------------------
// Show the ID menu next frame, and hide this menu next frame.
//-----------------------------------------------------------------------------
HSCRIPT CGameUIScriptInterface::LoadMenu( const char *szMenuName, HSCRIPT hParams )
{
	if ( !szMenuName || !*szMenuName )
		return NULL;

	// Build the key values for the command and broadcast (deleted inside broadcast system)
	KeyValues *kvCommand = ScriptTableToKeyValues( m_pScriptVM, szMenuName, hParams );
	KeyValues::AutoDelete autodelete_kvCommand( kvCommand );

	if ( ui_script_spew_level.GetInt() > 0 )
	{
		DevMsg( "CGameUIScriptInterface::LoadMenu\n" );
		KeyValuesDumpAsDevMsg( kvCommand );
	}

	IGameUISystem *pUI = g_pGameUISystemMgrImpl->LoadGameUIScreen( kvCommand );
	if ( !pUI )
		return NULL;
	
	KeyValues *kvResult = new KeyValues( "" );
	KeyValues::AutoDelete autodelete_kvResult( kvResult );
	kvResult->SetInt( "scripthandle", pUI->GetScriptHandle() );

	return ScriptTableFromKeyValues( m_pScriptVM, kvResult );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
HSCRIPT CGameUIScriptInterface::CreateGraphic( const char *szGraphicClassName, HSCRIPT hParams )
{
	if ( !szGraphicClassName || !*szGraphicClassName )
		return NULL;

	if ( !m_pMenu || !m_pMenu->GetGameUISystem() )
	{
		DevWarning( "Scripts not connected to game UI system and cannot create graphics!\n" );
		return NULL;
	}

	// Build the key values for the command
	KeyValues *kvCommand = ScriptTableToKeyValues( m_pScriptVM, szGraphicClassName, hParams );
	KeyValues::AutoDelete autodelete_kvCommand( kvCommand );

	if ( ui_script_spew_level.GetInt() > 0 )
	{
		DevMsg( "CGameUIScriptInterface::CreateGraphic\n" );
		KeyValuesDumpAsDevMsg( kvCommand );
	}

	const char* szGraphicName = kvCommand->GetString( "name", NULL );
	// Check if this instance is already created
	if ( szGraphicName == NULL )
	{
		DevWarning( "A must have a name!\n", szGraphicName );
		return NULL;
	}

	// Check if an instance of this graphic already exists
	CGameGraphic *pGraphic = m_pMenu->GraphicExists( szGraphicName );
	if ( pGraphic )
	{
		DevWarning( "A graphic with this name %s is already loaded!\n", szGraphicName );
		return NULL;
	}

	IGameUIGraphicClassFactory *pFactory = g_pGameUISystemMgrImpl->GetGraphicClassFactory( szGraphicClassName );
	if ( !pFactory )
	{
		DevWarning( "No graphic class factory for %s!\n", szGraphicClassName );
		return NULL;
	}

	CGameGraphic *pNewGraphic = pFactory->CreateNewGraphicClass( kvCommand, m_pMenu );
	if ( !pNewGraphic )
	{
		DevWarning( "No graphic in factory %s!\n", szGraphicClassName );
		return NULL;
	}

	KeyValues *kvResult = new KeyValues( "" );
	KeyValues::AutoDelete autodelete_kvResult( kvResult );
	kvResult->SetInt( "scripthandle", pNewGraphic->GetScriptHandle() );
	return ScriptTableFromKeyValues( m_pScriptVM, kvResult );
}

HSCRIPT CGameUIScriptInterface::CallScript( int32 iScriptHandle, const char *szCommandName, HSCRIPT hParams )
{
	if ( !szCommandName || !*szCommandName )
		return NULL;

	// Try to resolve other script handle specified
	CGameUISystem *pOtherScript = iScriptHandle ? CGameUISystem::FromScriptHandle( iScriptHandle ) : ( CGameUISystem * ) m_pMenu->GetGameUISystem();
	if ( !pOtherScript )
	{
		Warning( "CGameUIScriptInterface::CallScript with invalid script handle %d!\n", iScriptHandle );
		return NULL;
	}

	// Build the key values for the command and call other script
	KeyValues *kvCommand = ScriptTableToKeyValues( m_pScriptVM, szCommandName, hParams );
	KeyValues::AutoDelete autodelete_kvCommand( kvCommand );

	if ( ui_script_spew_level.GetInt() > 0 )
	{
		DevMsg( "CGameUIScriptInterface::CallScript( %d : %s )\n", iScriptHandle, pOtherScript->GetName() );
		KeyValuesDumpAsDevMsg( kvCommand );
	}

	// Pass the command to another script
	KeyValues *kvResult = NULL;
	pOtherScript->ExecuteScript( kvCommand, &kvResult );
	
	if ( ui_script_spew_level.GetInt() > 0 )
	{
		KeyValuesDumpAsDevMsg( kvResult );
	}

	KeyValues::AutoDelete autodelete_kvResult( kvResult );
	return ScriptTableFromKeyValues( m_pScriptVM, kvResult );
}

HSCRIPT CGameUIScriptInterface::CallGraphic( int32 iGraphicHandle, const char *szCommandName, HSCRIPT hParams )
{
	if ( !szCommandName || !*szCommandName || !iGraphicHandle )
		return NULL;

	CGameGraphic *pGraphic = CGameGraphic::FromScriptHandle( iGraphicHandle );
	if ( !pGraphic )
	{
		Warning( "CGameUIScriptInterface::CallGraphic with invalid graphic handle %d!\n", iGraphicHandle );
		return NULL;
	}

	// Build the key values for the command and call other script
	KeyValues *kvCommand = ScriptTableToKeyValues( m_pScriptVM, szCommandName, hParams );
	KeyValues::AutoDelete autodelete_kvCommand( kvCommand );

	if ( ui_script_spew_level.GetInt() > 0 )
	{
		DevMsg( "CGameUIScriptInterface::CallGraphic( %d : %s )\n", iGraphicHandle, pGraphic->GetName() );
		KeyValuesDumpAsDevMsg( kvCommand );
	}

	// Pass the command to graphic
	KeyValues *kvResult = pGraphic->HandleScriptCommand( kvCommand);
	if ( ui_script_spew_level.GetInt() > 0 )
	{
		KeyValuesDumpAsDevMsg( kvResult );
	}

	KeyValues::AutoDelete autodelete_kvResult( kvResult );
	return ScriptTableFromKeyValues( m_pScriptVM, kvResult );
}


HSCRIPT CGameUIScriptInterface::Nugget( const char *szCommandName, HSCRIPT hParams )
{
	if ( !szCommandName || !*szCommandName )
		return NULL;

	if ( !m_pMenu || !m_pMenu->GetGameUISystem() )
	{
		DevWarning( "Scripts not connected to game UI system and cannot use nuggets!\n" );
		return NULL;
	}

	// Build the key values for the command
	KeyValues *kvCommand = ScriptTableToKeyValues( m_pScriptVM, szCommandName, hParams );
	KeyValues::AutoDelete autodelete_kvCommand( kvCommand );

	if ( ui_script_spew_level.GetInt() > 0 )
	{
		DevMsg( "CGameUIScriptInterface::Nugget\n" );
		KeyValuesDumpAsDevMsg( kvCommand );
	}

	// Parse the command
	if ( char const *szUseName = StringAfterPrefix( kvCommand->GetName(), "load:" ) )
	{
		char const *szNuggetName = szUseName;
		szUseName = kvCommand->GetString( "usename", szNuggetName );
		
		// Check if the nugget is already loaded
		if ( m_Nuggets.Find( szUseName ) != m_Nuggets.InvalidIndex() )
		{
			DevWarning( "Nugget factory %s is already loaded!\n", szUseName );
			return NULL;
		}

		IGameUIScreenControllerFactory *pFactory = g_pGameUISystemMgrImpl->GetScreenControllerFactory( szNuggetName );
		if ( !pFactory )
		{
			DevWarning( "No nugget factory for %s!\n", szNuggetName );
			return NULL;
		}
		
		kvCommand->SetName( szNuggetName );
		IGameUIScreenController *pNugget = pFactory->GetController( kvCommand );
		if ( !pNugget )
		{
			DevWarning( "No nugget in factory %s!\n", szNuggetName );
			return NULL;
		}

		// Connect the nugget with our screen
		m_Nuggets.Insert( szUseName, pNugget );
		pNugget->OnScreenConnected( m_pMenu->GetGameUISystem() );

		KeyValues *kvResult = new KeyValues( "" );
		KeyValues::AutoDelete autodelete_kvResult( kvResult );
		kvResult->SetInt( "scripthandle", m_pMenu->GetGameUISystem()->GetScriptHandle() );
		kvResult->SetString( "usename", szUseName );
		kvResult->SetInt( "ptr", reinterpret_cast< int >( pNugget ) );

		if ( ui_script_spew_level.GetInt() > 0 )
		{
			DevMsg( "Loaded nugget %s\n", szNuggetName );
			KeyValuesDumpAsDevMsg( kvResult );
		}
		return ScriptTableFromKeyValues( m_pScriptVM, kvResult );
	}

	if ( char const *szUseName = StringAfterPrefix( kvCommand->GetName(), "ref:" ) )
	{
		int iRefScriptHandle = kvCommand->GetInt( "scripthandle" );
		char const *szRefUseName = kvCommand->GetString( "usename", szUseName );

		// Check if the nugget is already loaded
		if ( m_Nuggets.Find( szUseName ) != m_Nuggets.InvalidIndex() )
		{
			DevWarning( "Nugget factory %s is already loaded!\n", szUseName );
			return NULL;
		}

		CGameUISystem *pMenu = iRefScriptHandle ? CGameUISystem::FromScriptHandle( iRefScriptHandle ) : ( CGameUISystem * ) m_pMenu->GetGameUISystem();
		if ( !pMenu )
		{
			DevWarning( "Nugget reference request %s with invalid script handle %d!\n", szUseName, iRefScriptHandle );
			return NULL;
		}

		CGameUIScriptInterface *pRefInterface = NULL;
		if ( CGameUIScript *pScript = pMenu->Definition().GetScript() )
			pRefInterface = pScript->GetScriptInterface();

		if ( !pRefInterface )
		{
			DevWarning( "Nugget reference request %s with script handle %d(%s) which has no scripts!\n", szUseName, iRefScriptHandle, pMenu->GetName() );
			return NULL;
		}

		unsigned short usIdx = pRefInterface->m_Nuggets.Find( szRefUseName );
		if ( usIdx == pRefInterface->m_Nuggets.InvalidIndex() )
		{
			DevWarning( "Nugget reference request %s with script handle %d(%s) which has no nugget %s!\n", szUseName, iRefScriptHandle, pMenu->GetName(), szRefUseName );
			return NULL;
		}

		IGameUIScreenController *pNugget = pRefInterface->m_Nuggets.Element( usIdx );
		if ( reinterpret_cast< int >( pNugget ) != kvCommand->GetInt( "ptr" ) )
		{
			DevWarning( "Nugget reference request %s with script handle %d(%s) for nugget %s yielding %08X instead of expected %08X!\n",
				szUseName, iRefScriptHandle, pMenu->GetName(), szRefUseName, reinterpret_cast< int >( pNugget ), kvCommand->GetInt( "ptr" ) );
		}

		// Connect the nugget with our screen
		m_Nuggets.Insert( szUseName, pNugget );
		pNugget->OnScreenConnected( m_pMenu->GetGameUISystem() );

		KeyValues *kvResult = new KeyValues( "" );
		KeyValues::AutoDelete autodelete_kvResult( kvResult );
		kvResult->SetInt( "scripthandle", m_pMenu->GetGameUISystem()->GetScriptHandle() );
		kvResult->SetString( "usename", szUseName );
		kvResult->SetInt( "ptr", reinterpret_cast< int >( pNugget ) );

		if ( ui_script_spew_level.GetInt() > 0 )
		{
			DevMsg( "Referenced nugget %s from %d(%s):%s\n", szUseName, iRefScriptHandle, pMenu->GetName(), szRefUseName );
			KeyValuesDumpAsDevMsg( kvResult );
		}
		return ScriptTableFromKeyValues( m_pScriptVM, kvResult );
	}

	if ( char const *szUseName = StringAfterPrefix( kvCommand->GetName(), "free:" ) )
	{
		// Check if the nugget is already loaded
		unsigned short usIdx = m_Nuggets.Find( szUseName );
		if ( usIdx == m_Nuggets.InvalidIndex() )
		{
			DevWarning( "Nugget factory %s is not loaded!\n", szUseName );
			return NULL;
		}

		// Unload the nugget
		IGameUIScreenController *pNugget = m_Nuggets.Element( usIdx );
		m_Nuggets.RemoveAt( usIdx );

		pNugget->OnScreenDisconnected( m_pMenu->GetGameUISystem() );
		if ( ui_script_spew_level.GetInt() > 0 )
		{
			DevMsg( "Unloaded nugget %s\n", szUseName );
		}
		return NULL;
	}

	if ( char const *szUse = StringAfterPrefix( kvCommand->GetName(), "use:" ) )
	{
		char const *szUseName = szUse;
		// Split off nugget name by :
		if ( char const *pch = strchr( szUseName, ':' ) )
		{
			char *buf = ( char * ) stackalloc( pch - szUseName + 1 );
			Q_strncpy( buf, szUseName, pch - szUseName + 1 );
			szUseName = buf;
			kvCommand->SetName( pch + 1 );
		}
		else
		{
			kvCommand->SetName( "" );
		}

		// Check if the nugget is already loaded
		unsigned short usIdx = m_Nuggets.Find( szUseName );
		if ( usIdx == m_Nuggets.InvalidIndex() )
		{
			DevWarning( "Nugget factory %s is not loaded!\n", szUseName );
			return NULL;
		}

		// Nugget operation
		IGameUIScreenController *pNugget = m_Nuggets.Element( usIdx );
		KeyValues *kvResult = pNugget->OnScreenEvent( m_pMenu->GetGameUISystem(), kvCommand );
		
		KeyValues::AutoDelete autodelete_kvResult( kvResult );
		if ( ui_script_spew_level.GetInt() > 0 )
		{
			KeyValuesDumpAsDevMsg( kvResult );
		}

		// Push results for the script
		return ScriptTableFromKeyValues( m_pScriptVM, kvResult );
	}

	return NULL;
}

bool CGameUIScriptInterface::ScriptVmKeyValueToVariant( IScriptVM *pVM, KeyValues *val, ScriptVariant_t &varValue, char chScratchBuffer[KV_VARIANT_SCRATCH_BUF_SIZE] )
{
	switch ( val->GetDataType() )
	{
	case KeyValues::TYPE_STRING:
		varValue = val->GetString();
		return true;
	case KeyValues::TYPE_INT:
		varValue = val->GetInt();
		return true;
	case KeyValues::TYPE_FLOAT:
		varValue = val->GetFloat();
		return true;
	case KeyValues::TYPE_UINT64:
		Q_snprintf( chScratchBuffer, KV_VARIANT_SCRATCH_BUF_SIZE, "%llu", val->GetUint64() );
		varValue = chScratchBuffer;
		return true;
	case KeyValues::TYPE_NONE:
		varValue = ScriptTableFromKeyValues( pVM, val );
		return true;
	default:
		Warning( "ScriptVmKeyValueToVariant failed to package parameter %s (type %d)\n", val->GetName(), val->GetDataType() );
		return false;
	}
}

bool CGameUIScriptInterface::ScriptVmStringFromVariant( ScriptVariant_t &varValue, char chScratchBuffer[KV_VARIANT_SCRATCH_BUF_SIZE] )
{
	switch ( varValue.m_type )
	{
	case FIELD_INTEGER:
		Q_snprintf( chScratchBuffer, KV_VARIANT_SCRATCH_BUF_SIZE, "%d", varValue.m_int );
		return true;
	case FIELD_FLOAT:
		Q_snprintf( chScratchBuffer, KV_VARIANT_SCRATCH_BUF_SIZE, "%f", varValue.m_float );
		return true;
	case FIELD_BOOLEAN:
		Q_snprintf( chScratchBuffer, KV_VARIANT_SCRATCH_BUF_SIZE, "%d", varValue.m_bool );
		return true;
	case FIELD_CHARACTER:
		Q_snprintf( chScratchBuffer, KV_VARIANT_SCRATCH_BUF_SIZE, "%c", varValue.m_char );
		return true;
	case FIELD_CSTRING:
		Q_snprintf( chScratchBuffer, KV_VARIANT_SCRATCH_BUF_SIZE, "%s", varValue.m_pszString ? varValue.m_pszString : "" );
		return true;
	default:
		Warning( "ScriptVmStringFromVariant failed to unpack parameter variant type %d\n", varValue.m_type );
		return false;
	}
}

KeyValues * CGameUIScriptInterface::ScriptVmKeyValueFromVariant( IScriptVM *pVM, ScriptVariant_t &varValue )
{
	KeyValues *val = NULL;

	switch ( varValue.m_type )
	{
	case FIELD_INTEGER:
		val = new KeyValues( "" );
		val->SetInt( NULL, varValue.m_int );
		return val;
	case FIELD_FLOAT:
		val = new KeyValues( "" );
		val->SetFloat( NULL, varValue.m_float );
		return val;
	case FIELD_BOOLEAN:
		val = new KeyValues( "" );
		val->SetInt( NULL, varValue.m_bool ? 1 : 0 );
		return val;
	case FIELD_CHARACTER:
		val = new KeyValues( "" );
		val->SetString( NULL, CFmtStr( "%c", varValue.m_char ) );
		return val;
	case FIELD_CSTRING:
		val = new KeyValues( "" );
		val->SetString( NULL, varValue.m_pszString ? varValue.m_pszString : "" );
		return val;
	case FIELD_HSCRIPT:
		return ScriptTableToKeyValues( pVM, "", varValue.m_hScript );
	default:
		Warning( "ScriptVmKeyValueFromVariant failed to unpack parameter variant type %d\n", varValue.m_type );
		return NULL;
	}
}

KeyValues * CGameUIScriptInterface::ScriptTableToKeyValues( IScriptVM *pVM, char const *szName, HSCRIPT hTable )
{
	if ( !szName )
		szName = "";
	
	KeyValues *kv = new KeyValues( szName );

	if ( hTable && pVM )
	{
		int numKeys = pVM->GetNumTableEntries( hTable );
		for ( int k = 0; k < numKeys; ++ k )
		{
			ScriptVariant_t varKey, varValue;
			pVM->GetKeyValue( hTable, k, &varKey, &varValue );

			char chScratchBuffer[ KV_VARIANT_SCRATCH_BUF_SIZE ];
			if ( !ScriptVmStringFromVariant( varKey, chScratchBuffer ) )
			{
				Assert( 0 );
				continue;
			}

			KeyValues *sub = ScriptVmKeyValueFromVariant( pVM, varValue );
			if ( !sub )
			{
				Assert( 0 );
				// sub->deleteThis();
				// continue;
				// still proceed - it will be a key with no data
				sub = new KeyValues( "" );
			}
			sub->SetName( chScratchBuffer );
			
			kv->AddSubKey( sub );
		}
	}

	return kv;
}

HSCRIPT CGameUIScriptInterface::ScriptTableFromKeyValues( IScriptVM *pVM, KeyValues *kv )
{
	if ( !kv || !pVM )
		return NULL;

	ScriptVariant_t varTable;
	pVM->CreateTable( varTable );

	for ( KeyValues *val = kv->GetFirstSubKey(); val; val = val->GetNextKey() )
	{
		ScriptVariant_t varValue;
		char chScratchBuffer[ KV_VARIANT_SCRATCH_BUF_SIZE ];
		if ( !ScriptVmKeyValueToVariant( pVM, val, varValue, chScratchBuffer ) )
			continue;

#ifdef GAMEUI_SCRIPT_LOWERCASE_ALL_TABLE_KEYS
		char chNameBuffer[ KV_VARIANT_SCRATCH_BUF_SIZE ];
		Q_strncpy( chNameBuffer, val->GetName(), KV_VARIANT_SCRATCH_BUF_SIZE );
		Q_strlower( chNameBuffer );
#else
		char const *chNameBuffer = val->GetName();
#endif

		pVM->SetValue( varTable.m_hScript, chNameBuffer, varValue );
	}

	return varTable.m_hScript;
}

