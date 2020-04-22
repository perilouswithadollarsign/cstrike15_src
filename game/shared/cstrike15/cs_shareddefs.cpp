//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "cs_shareddefs.h"
#include "gametypes/igametypes.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

const float CS_PLAYER_SPEED_RUN				= 260.0f;
const float CS_PLAYER_SPEED_VIP				= 227.0f;
const float CS_PLAYER_SPEED_SHIELD			= 160.0f;
const float CS_PLAYER_SPEED_HAS_HOSTAGE		= 200.0f;
const float CS_PLAYER_SPEED_STOPPED			=   1.0f;
const float CS_PLAYER_SPEED_OBSERVER		= 900.0f;

const float CS_PLAYER_SPEED_DUCK_MODIFIER	= 0.34f;
const float CS_PLAYER_SPEED_WALK_MODIFIER	= 0.52f;
const float CS_PLAYER_SPEED_CLIMB_MODIFIER	= 0.34f;
const float CS_PLAYER_HEAVYARMOR_FLINCH_MODIFIER = 0.5f;

const float CS_PLAYER_DUCK_SPEED_IDEAL = 8.0f;

const char *pszWinPanelCategoryHeaders[] =
{
	"",
	"#winpanel_topdamage",
	"#winpanel_topheadshots",
	"#winpanel_kills"
};

const char *PlayerModelInfo::g_customizationModelCT = "models/player/custom_player/scaffold_ct.mdl";
const char *PlayerModelInfo::g_customizationModelT = "models/player/custom_player/scaffold_t.mdl";

const char *PlayerModelInfo::g_defaultTModel = "tm_phoenix";
const char *PlayerModelInfo::g_defaultCTModel = "ctm_st6";

#define CFG( mdl_substr, skintone, def_glove, sleeve, override_sleeve ) { #mdl_substr, #skintone, #def_glove, #sleeve, #override_sleeve },
static PlayerViewmodelArmConfig s_playerViewmodelArmConfigs[] =
{
#include "viewmodel_arm_config.inc"
/*
	// Old values, leaving for reference
	// character model substr		//skintone index	// default glove model																// associated sleeve													// econ override sleeve ( if present, overrides associated sleeve if glove is econ )
	{ "tm_leet",					"0",				"models/weapons/v_models/arms/glove_fingerless/v_glove_fingerless.mdl",				"",																		"" },
	{ "tm_phoenix",					"0",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"",																		"" },
	{ "tm_separatist",				"0",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"models/weapons/v_models/arms/separatist/v_sleeve_separatist.mdl",		"" },
	{ "tm_balkan",					"0",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"models/weapons/v_models/arms/balkan/v_sleeve_balkan.mdl",				"" },
	{ "tm_professional_var4",		"1",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"models/weapons/v_models/arms/professional/v_sleeve_professional.mdl",	"" },
	{ "tm_professional",			"0",				"models/weapons/v_models/arms/glove_fullfinger/v_glove_fullfinger.mdl",				"models/weapons/v_models/arms/professional/v_sleeve_professional.mdl",	"" },
	{ "tm_anarchist",				"0",				"models/weapons/v_models/arms/anarchist/v_glove_anarchist.mdl",						"",																		"models/weapons/v_models/arms/anarchist/v_sleeve_anarchist.mdl" },
	{ "tm_pirate",					"1",				"models/weapons/v_models/arms/bare/v_bare_hands.mdl",								"models/weapons/v_models/arms/pirate/v_pirate_watch.mdl",				"" },
	{ "tm_heavy",					"0",				"models/weapons/v_models/arms/glove_fingerless/v_glove_fingerless.mdl",				"models/weapons/v_models/arms/balkan/v_sleeve_balkan.mdl",				"" },
	{ "ctm_st6",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle.mdl",			"models/weapons/v_models/arms/st6/v_sleeve_st6.mdl",					"" },
	{ "ctm_idf",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle.mdl",			"models/weapons/v_models/arms/idf/v_sleeve_idf.mdl",					"" },
	{ "ctm_gign",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_blue.mdl",		"models/weapons/v_models/arms/gign/v_sleeve_gign.mdl",					"" },
	{ "ctm_swat_variantb",			"1",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl",		"models/weapons/v_models/arms/swat/v_sleeve_swat.mdl",					"" },
	{ "ctm_swat",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl",		"models/weapons/v_models/arms/swat/v_sleeve_swat.mdl",					"" },
	{ "ctm_gsg9",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_blue.mdl",		"models/weapons/v_models/arms/gsg9/v_sleeve_gsg9.mdl",					"" },
	{ "ctm_sas",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl",		"models/weapons/v_models/arms/sas/v_sleeve_sas.mdl",					"" },
	{ "ctm_fbi",					"0",				"models/weapons/v_models/arms/glove_hardknuckle/v_glove_hardknuckle_black.mdl",		"models/weapons/v_models/arms/fbi/v_sleeve_fbi.mdl",					"" },
*/
};
#undef CFG

PlayerModelInfo PlayerModelInfo::s_PlayerModelInfo;

PlayerModelInfo::PlayerModelInfo()
{
#if !defined( CLIENT_DLL )
	m_NumTModels = 0;
	m_NumCTModels = 0;
	m_NumModels = 0;
	m_nNextClassT = -1;
	m_nNextClassCT = -1;

	m_mapName[0] = 0;

	for ( int i=0; i<CS_MAX_PLAYER_MODELS; ++i )
	{
		m_ClassModelPaths[i][0] = 0;
		m_ClassNames[i][0] = 0;
	}

#endif // !CLIENT_DLL
} 

const PlayerViewmodelArmConfig *GetPlayerViewmodelArmConfigForPlayerModel( const char* szPlayerModel )
{
	if ( szPlayerModel != NULL )
	{
		for ( int i=0; i<ARRAYSIZE(s_playerViewmodelArmConfigs); i++ )
		{
			if ( V_stristr( szPlayerModel, s_playerViewmodelArmConfigs[i].szPlayerModelSearchSubStr ) )
				return &s_playerViewmodelArmConfigs[i];
		}
	}

	AssertMsg1( false, "Could not determine viewmodel config for character model: %s", szPlayerModel );
	return &s_playerViewmodelArmConfigs[0];
}

#if !defined( CLIENT_DLL )

bool PlayerModelInfo::IsTClass( int i )
{
	// The 0 model is class NONE
	return (i >= GetFirstTClass() && i <= GetLastTClass() );
} 

bool PlayerModelInfo::IsCTClass( int i )
{
	// The 0 model is class NONE
	return (i >= GetFirstCTClass() && i <= GetLastCTClass() );
} 

const char *PlayerModelInfo::GetClassName( int classID )
{
	AssertMsg( classID >= GetFirstClass() && classID <= GetLastClass(), "Invalid class ID for models loaded.\n ");
	return m_ClassNames[classID-1];
}

const char *PlayerModelInfo::GetClassModelPath( int classID )
{
	AssertMsg( classID >= GetFirstClass() && classID <= GetLastClass(), "Invalid class ID for models loaded.\n ");
	return m_ClassModelPaths[classID-1];
}

int PlayerModelInfo::GetNextClassForTeam( int team )
{
	if ( team == TEAM_TERRORIST )
	{
		if ( m_nNextClassT == -1 )
		{
			m_nNextClassT = RandomInt( GetFirstTClass(), GetLastTClass() );
		}

		++m_nNextClassT;

		if ( m_nNextClassT > GetLastTClass() )
		{
			m_nNextClassT = GetFirstTClass();
		}

		return m_nNextClassT;
	}
	else if ( team == TEAM_CT )
	{
		if ( m_nNextClassCT == -1 )
		{
			m_nNextClassCT = RandomInt( GetFirstCTClass(), GetLastCTClass() );	
		}

		++m_nNextClassCT;

		if ( m_nNextClassCT > GetLastCTClass() )
		{
			m_nNextClassCT = GetFirstCTClass();
		}

		return m_nNextClassCT;
	}
	else
	{
		return GetFirstClass();
	}
}

void PlayerModelInfo::InitializeForCurrentMap( void )
{
	const char *mapName = ( gpGlobals ? STRING( gpGlobals->mapname ) : NULL );
	if ( mapName && 
		 V_stricmp( m_mapName, mapName ) == 0 )
	{
		// We have already cached the model information for this map.
		return;
	}

	if ( mapName )
	{
		V_strcpy( m_mapName, mapName );
	}
	else
	{
		m_mapName[0] = '\0';
	}

	m_NumTModels = 0;
	m_NumCTModels = 0;
	m_NumModels = 0;
	m_nNextClassCT = -1;
	m_nNextClassT = -1;

	bool bUseCosmetics = false;

	// If the custom character system is enabled, we should load invisible skeleton players that will be eventually clothed.
	if ( bUseCosmetics )
	{
		//FIXME: custom characters have no audio persona; that is they don't speak yet (who would they sound like?)

		AddModel( g_customizationModelT );
		m_NumTModels = 1;
		AddModel( g_customizationModelCT );
		m_NumCTModels = 1;

		DevMsg( "Using script-defined character customization definitions\n" );
	}
	else
	{

		// Add the terrorist models.
		// NOTE: Terrorists must be loaded first since it is assumed they are first in the list of models.
		const CUtlStringList *pTModelNames = g_pGameTypes->GetTModelsForMap( m_mapName );
		if ( pTModelNames )
		{
			FOR_EACH_VEC( *pTModelNames, iModel )
			{
				const char *modelName = (*pTModelNames)[iModel];
				if ( modelName )
				{
					AddModel( modelName );
				}
			}

			m_NumTModels = m_NumModels;
		}

		// Add the default terrorist model, if no models were loaded.
		if ( m_NumTModels == 0 )
		{
			Warning( "PlayerModelInfo: missing terrorist models for map %s. Adding the default model %s.\n", m_mapName, g_defaultTModel );
			AddModel( g_defaultTModel );
			m_NumTModels = 1;
		}

		// Add the counter-terrorist models.
		const CUtlStringList *pCTModelNames = g_pGameTypes->GetCTModelsForMap( m_mapName );
		if ( pCTModelNames )
		{
			FOR_EACH_VEC( *pCTModelNames, iModel )
			{
				const char *modelName = (*pCTModelNames)[iModel];
				if ( modelName )
				{
					AddModel( modelName );
				}
			}

			m_NumCTModels = m_NumModels - m_NumTModels;
		}

		// Add the default counter-terrorist model, if no models were loaded.
		if ( m_NumCTModels == 0 )
		{
			Warning( "PlayerModelInfo: missing counter-terrorist models for map %s. Adding the default model %s.\n", m_mapName, g_defaultCTModel );
			AddModel( g_defaultCTModel );
			m_NumCTModels = 1;
		}

	}
	
}

void PlayerModelInfo::AddModel( const char *modelName )
{
	Assert( modelName );
	if ( !modelName ||
		 modelName[0] == '\0' )
	{
		return;
	}

	Assert( m_NumModels >= 0 && m_NumModels < CS_MAX_PLAYER_MODELS );
	if ( m_NumModels >= 0 && m_NumModels < CS_MAX_PLAYER_MODELS )
	{
		V_strcpy_safe( m_ClassNames[m_NumModels], modelName );

		if ( V_stristr( modelName, "scaffold" ) )
		{
			V_strcpy_safe( m_ClassModelPaths[m_NumModels], modelName );
		}
		else
		{
			char szCleanedPath[MAX_MODEL_STRING_SIZE];
			V_StrSubst( m_ClassNames[m_NumModels], "models/player", "", szCleanedPath, sizeof(szCleanedPath) );
			char szRedirectedPath[MAX_MODEL_STRING_SIZE];
			V_sprintf_safe( szRedirectedPath, "models/player/custom_player/legacy/%s", szCleanedPath );
			if ( !V_stristr( szRedirectedPath, ".mdl" ) )
				V_strcat_safe( szRedirectedPath, ".mdl" );

			if ( !filesystem->FileExists( szRedirectedPath, "MOD" ) )
			{
				AssertMsg1( false, "Verify map .kv: Player model doesn't exist: %s.", szRedirectedPath );

				// Get a full path to the mdl file.
				if ( !V_stristr( modelName, "models/player" ) && !V_stristr( modelName, ".mdl" ) )
				{
					V_snprintf( m_ClassModelPaths[m_NumModels], sizeof( m_ClassModelPaths[m_NumModels] ), "models/player/%s.mdl", modelName );
				}
				else
				{
					V_snprintf( m_ClassModelPaths[m_NumModels], sizeof( m_ClassModelPaths[m_NumModels] ), "%s", modelName );
				}
			}
			else
			{
				V_strcpy_safe( m_ClassModelPaths[m_NumModels], szRedirectedPath );
			}
		}

		++m_NumModels;
	}
	else
	{
		Warning( "PlayerModelInfo: model count has exceeded the maximum (%d) for map \"%s\". Ignoring model %s.\n",
			CS_MAX_PLAYER_MODELS, m_mapName, modelName );
	}
}

#endif // !CLIENT_DLL

const char* QuestProgress::ReasonString(Reason reason)
{
	switch(reason)
	{
	case QUEST_OK:
		return "ok";
	case QUEST_NOT_ENOUGH_PLAYERS:
		return "not_enough_players";
	case QUEST_WARMUP:
		return "warmup";
	case QUEST_NOT_CONNECTED_TO_STEAM:
		return "not_connected_to_steam";
	case QUEST_NONOFFICIAL_SERVER:
		return "nonofficial_server";
	case QUEST_NO_ENTITLEMENT:
		return "no_entitlement";
	case QUEST_NO_QUEST:
		return "no_quest";
	case QUEST_PLAYER_IS_BOT:
		return "player_is_bot";
	case QUEST_WRONG_MAP:
		return "wrong_map";
	case QUEST_WRONG_MODE:
		return "wrong_mode";
	case QUEST_NOT_SYNCED_WITH_SERVER:
		return "not_synced_with_server";
	
	case QUEST_NONINITIALIZED: // treat as 'unknown reason'.
	default:
		return "unknown";
	}
}