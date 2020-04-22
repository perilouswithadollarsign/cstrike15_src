//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "sfhudcallvotepanel.h"
#include "hud_macros.h"
#include <vgui/ILocalize.h>
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "basepanel.h"
#include "gametypes.h"
#include "cs_gamerules.h"
#include "c_user_message_register.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_test_vote_scroll( "cl_test_vote_scroll", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY );

SFHudCallVotePanel* SFHudCallVotePanel::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF		
	SFUI_DECL_METHOD( PopulatePlayerTargets ),
	SFUI_DECL_METHOD( PopulateMapTargets ),	
	SFUI_DECL_METHOD( PopulateBackupFilenames ),	
	SFUI_DECL_METHOD( GetNumberOfValidMapsInGroup ),	
	SFUI_DECL_METHOD( GetNumberOfValidKickTargets ),	
	SFUI_DECL_METHOD( IsQueuedMatchmaking ),
	SFUI_DECL_METHOD( IsEndMatchMapVoteEnabled ),
	SFUI_DECL_METHOD( IsPlayingClassicCompetitive ),
SFUI_END_GAME_API_DEF( SFHudCallVotePanel, CallVotePanel );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SFHudCallVotePanel::SFHudCallVotePanel() : ScaleformFlashInterface()
{		
}


void SFHudCallVotePanel::LoadDialog( void )
{
	if ( m_pInstance )
	{
		m_pInstance->Show();
	}
	else
	{
		m_pInstance = new SFHudCallVotePanel();
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, SFHudCallVotePanel, m_pInstance, CallVotePanel );		
	}
}

void SFHudCallVotePanel::UnloadDialog( void )
{
	if ( m_pInstance )
	{
		m_pInstance->RemoveFlashElement();
		m_pInstance = NULL;
	}
}

void SFHudCallVotePanel::PostUnloadFlash( void )
{
	BasePanel()->DismissPauseMenu();	

	m_pInstance = NULL;
	delete this;
}


void SFHudCallVotePanel::LevelShutdown( void )
{
	UnloadDialog();
}

void SFHudCallVotePanel::FlashReady( void )
{
	if ( !m_FlashAPI )
	{
		return;
	}
	
	Show();	

}

bool SFHudCallVotePanel::PreUnloadFlash( void )
{
	return true;
}

void SFHudCallVotePanel::Show()
{
	WITH_SLOT_LOCKED
	{
		if ( m_pScaleformUI )
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );			
		}
	}
}

void SFHudCallVotePanel::Hide( void )
{
	WITH_SLOT_LOCKED
	{
		if ( m_pScaleformUI )
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );			
		}
	}			
}

void SFHudCallVotePanel::PopulatePlayerTargets( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( g_PR )
	{
		int slot = 0;
		SFVALUE result = CreateFlashObject();
		SFVALUE names = CreateFlashArray( MAX_PLAYERS );
		SFVALUE ids = CreateFlashArray( MAX_PLAYERS );

		for ( int player = 1 ; player < MAX_PLAYERS; ++player )
		{	
			if (	g_PR->IsConnected( player ) && 
					( cl_test_vote_scroll.GetBool() || !g_PR->IsFakePlayer( player ) ) && 
					GetLocalPlayerIndex() != player &&
					g_PR->GetTeam( player ) == g_PR->GetTeam( GetLocalPlayerIndex() ) )
			{
				player_info_t playerInfo;
				if ( engine->GetPlayerInfo( player, &playerInfo ) )
				{
					wchar_t szName[MAX_DECORATED_PLAYER_NAME_LENGTH];
                    wchar_t szSafeName[MAX_DECORATED_PLAYER_NAME_LENGTH];

					g_pVGuiLocalize->ConvertANSIToUnicode( g_PR->GetPlayerName( player ), szName, sizeof(szName) );
                    g_pScaleformUI->MakeStringSafe( szName, szSafeName, sizeof( szSafeName ) );
					TruncatePlayerName( szSafeName, ARRAYSIZE( szSafeName ), CALLVOTE_NAME_TRUNCATE_AT );
					m_pScaleformUI->Value_SetArrayElement( names, slot, szSafeName );
					m_pScaleformUI->Value_SetArrayElement( ids, slot, playerInfo.userID );
					slot++;
				}
			}
		}			

		m_pScaleformUI->Value_SetMember( result, "names", names );
		m_pScaleformUI->Value_SetMember( result, "ids", ids );
		m_pScaleformUI->Value_SetMember( result, "count", slot );
		m_pScaleformUI->Params_SetResult( obj, result );	

		SafeReleaseSFVALUE( names );
		SafeReleaseSFVALUE( ids );
		SafeReleaseSFVALUE( result );
	}
	
}

void SFHudCallVotePanel::GetNumberOfValidKickTargets( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( g_PR )
	{
		int slot = 0;
		SFVALUE result = CreateFlashObject();

		for ( int player = 1; player < MAX_PLAYERS; ++player )
		{
			if ( g_PR->IsConnected( player ) &&
				 ( cl_test_vote_scroll.GetBool() || !g_PR->IsFakePlayer( player ) ) &&
				 GetLocalPlayerIndex() != player &&
				 g_PR->GetTeam( player ) == g_PR->GetTeam( GetLocalPlayerIndex() ) )
			{
				player_info_t playerInfo;
				if ( engine->GetPlayerInfo( player, &playerInfo ) )
				{
					slot++;
				}
			}
		}

		m_pScaleformUI->Value_SetMember( result, "count", slot );
		m_pScaleformUI->Params_SetResult( obj, result );	

		SafeReleaseSFVALUE( result );
	}

}

void SFHudCallVotePanel::PopulateMapTargets( SCALEFORM_CALLBACK_ARGS_DECL )
{	
	bool bIncludeCurrentMap = false;

	if ( pui->Params_GetNumArgs( obj ) >= 1 )
	{
		bIncludeCurrentMap = pui->Params_GetArgAsBool( obj, 0 );
	}

	SFVALUE result = CreateFlashObject();
	SFVALUE names = CreateFlashArray( MAX_TARGET_COUNT );
	SFVALUE ids = CreateFlashArray( MAX_TARGET_COUNT );

	int numMaps = 0; //The number of maps in this cycle group
	int actualNumMaps = 0; //The number of maps we display (in case we have any errors)

	if ( g_pGameTypes )
	{
		const char* mapGroupName = engine->GetMapGroupName();
		const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( mapGroupName );

		if ( mapsInGroup )
		{
			numMaps = mapsInGroup->Count();
			char szCurLevel[MAX_PATH];
			V_strcpy_safe( szCurLevel, engine->GetLevelName() );
			V_FixSlashes(szCurLevel, '/' ); // use consistent slashes.
			V_StripExtension( szCurLevel, szCurLevel, sizeof( szCurLevel ) );	
			for( int i = 0 ; i < numMaps ; ++i )
			{
				const char* internalMapName = (*mapsInGroup)[i];
				if ( !bIncludeCurrentMap && V_strstr( szCurLevel, V_GetFileName( internalMapName ) ) )
				{
					// don't populate the list with the same map that we're on
					continue;
				}

				if ( internalMapName && V_strcmp( "undefined", internalMapName ) != 0 && V_strlen( internalMapName ) > 0 )
				{
					const wchar_t* friendlyMapName = CSGameRules()->GetFriendlyMapName(internalMapName);

					if ( friendlyMapName )
					{
						m_pScaleformUI->Value_SetArrayElement( names, actualNumMaps, friendlyMapName );	
					}
					else
					{
						m_pScaleformUI->Value_SetArrayElement( names, actualNumMaps, internalMapName );	
					}
			
					m_pScaleformUI->Value_SetArrayElement( ids, actualNumMaps, internalMapName );	

					actualNumMaps++;					
				}
			}
		}
	}			

	m_pScaleformUI->Value_SetMember( result, "friendlyNames", names );
	m_pScaleformUI->Value_SetMember( result, "names", ids );
	m_pScaleformUI->Value_SetMember( result, "count", actualNumMaps );
	m_pScaleformUI->Params_SetResult( obj, result );	

	SafeReleaseSFVALUE( names );
	SafeReleaseSFVALUE( ids );
	SafeReleaseSFVALUE( result );
}

void SFHudCallVotePanel::PopulateBackupFilenames( SCALEFORM_CALLBACK_ARGS_DECL )
{
	engine->ServerCmd( "send_round_backup_file_list" );
}

bool __MsgFunc_RoundBackupFilenames( const CCSUsrMsg_RoundBackupFilenames &msg )
{

	if ( SFHudCallVotePanel::m_pInstance )
	{
		SFHudCallVotePanel::m_pInstance->PopulateBackupFilenames_Callback( msg );
	}

	return true;
}
USER_MESSAGE_REGISTER( RoundBackupFilenames );

void SFHudCallVotePanel::PopulateBackupFilenames_Callback( const CCSUsrMsg_RoundBackupFilenames &msg )
{
	if ( !FlashAPIIsValid() )
		return;

	static CCSUsrMsg_RoundBackupFilenames files[ 10 ];

	files[ clamp( msg.index(), 0, 9 ) ] = msg;

	// don't do anything until we get the last file msg.
	if ( msg.index() < ( msg.count() - 1 ) )
		return;
	
	char szCommaDelimitedFilenames[1024] = { 0 };
	char szCommaDelimitedNicenames[1024] = { 0 };

	for ( int i = 0; i < min ( msg.count(), 10 ); i++ )
	{
		if ( *szCommaDelimitedFilenames )
			V_strcat_safe( szCommaDelimitedFilenames, "," );

		V_strcat_safe( szCommaDelimitedFilenames, files[ i ].filename().c_str() );

		if ( *szCommaDelimitedNicenames )
			V_strcat_safe( szCommaDelimitedNicenames, "," );

		V_strcat_safe( szCommaDelimitedNicenames, files[ i ].nicename().c_str() );
	}

	WITH_SFVALUEARRAY( data, 2 )
	{
		m_pScaleformUI->ValueArray_SetElement( data, 0, szCommaDelimitedFilenames );
		m_pScaleformUI->ValueArray_SetElement( data, 1, szCommaDelimitedNicenames );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "PopulateBackupFilenames_Callback", data, 2 );
	}
}

void SFHudCallVotePanel::GetNumberOfValidMapsInGroup( SCALEFORM_CALLBACK_ARGS_DECL )
{	
	bool bIncludeCurrentMap = false;

	if ( pui->Params_GetNumArgs( obj ) >= 1 )
	{
		bIncludeCurrentMap = pui->Params_GetArgAsBool( obj, 0 );
	}

	SFVALUE result = CreateFlashObject();

	int numMaps = 0; //The number of maps in this cycle group
	int actualNumMaps = 0; //The number of maps we display (in case we have any errors)
	static ConVarRef sv_vote_to_changelevel_before_match_point( "sv_vote_to_changelevel_before_match_point" );

	if ( g_pGameTypes && CSGameRules() 
			&& !( sv_vote_to_changelevel_before_match_point.GetInt() > 0 && ( CSGameRules()->IsMatchPoint() || CSGameRules()->IsIntermission() ) ) )
	{
		const char* mapGroupName = engine->GetMapGroupName();
		const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( mapGroupName );

		if ( mapsInGroup )
		{
			numMaps = mapsInGroup->Count();

			for( int i = 0 ; i < numMaps ; ++i )
			{
				const char* internalMapName = ( *mapsInGroup )[i];
				if ( !bIncludeCurrentMap && 0 == V_strcmp( engine->GetLevelNameShort(), internalMapName ) )
				{
					// don't populate the list with the same map that we're on
					continue;
				}
				else if ( internalMapName && V_strlen( internalMapName ) > 0 )
				{
					actualNumMaps++;					
				}
			}
		}
	}			

	m_pScaleformUI->Value_SetMember( result, "count", actualNumMaps );
	m_pScaleformUI->Params_SetResult( obj, result );	
	SafeReleaseSFVALUE( result );
}

void SFHudCallVotePanel::IsQueuedMatchmaking( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bQ = CSGameRules() && CSGameRules()->IsQueuedMatchmaking();

	m_pScaleformUI->Params_SetResult( obj, bQ );
}

void SFHudCallVotePanel::IsEndMatchMapVoteEnabled( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bY = false;

	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
	if ( cs_PR && CSGameRules()->IsEndMatchVotingForNextMapEnabled()  )
		bY = true;

	m_pScaleformUI->Params_SetResult( obj, bY );
}

void SFHudCallVotePanel::IsPlayingClassicCompetitive( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj,  ( CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() ) );
}



#endif // INCLUDE_SCALEFORM
