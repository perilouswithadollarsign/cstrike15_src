//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//
#ifndef _MM_TITLE_CONTEXTVALUES_H
#define _MM_TITLE_CONTEXTVALUES_H
#pragma once

#include "csgo.spa.h"

static ContextValue_t g_MapGroupContexts[] = 
{
	{ "mg_bomb",			CONTEXT_CSS_MAP_GROUP_BOMB_MG },
	{ "mg_hostage",			CONTEXT_CSS_MAP_GROUP_HOSTAGE_MG },
	{ "mg_demolition",		CONTEXT_CSS_MAP_GROUP_DEMOLITION_MG },
	{ "mg_armsrace",		CONTEXT_CSS_MAP_GROUP_ARMSRACE_MG },
	{ "mg_de_aztec",		CONTEXT_CSS_MAP_GROUP_AZTEC_MG },
	{ "mg_ar_baggage",		CONTEXT_CSS_MAP_GROUP_BAGGAGE_MG },
	{ "mg_de_bank",			CONTEXT_CSS_MAP_GROUP_BANK_MG },
	{ "mg_de_dust",			CONTEXT_CSS_MAP_GROUP_DUST_MG },
	{ "mg_de_dust2",		CONTEXT_CSS_MAP_GROUP_DUST2_MG },
	{ "mg_de_inferno",		CONTEXT_CSS_MAP_GROUP_INFERNO_MG },
	{ "mg_cs_italy",		CONTEXT_CSS_MAP_GROUP_ITALY_MG },
	{ "mg_de_lake",			CONTEXT_CSS_MAP_GROUP_LAKE_MG },
	{ "mg_de_nuke",			CONTEXT_CSS_MAP_GROUP_NUKE_MG },
	{ "mg_cs_office",		CONTEXT_CSS_MAP_GROUP_OFFICE_MG },
	{ "mg_de_safehouse",	CONTEXT_CSS_MAP_GROUP_SAFEHOUSE_MG },
	{ "mg_ar_shoots",		CONTEXT_CSS_MAP_GROUP_SHOOTS_MG },
	{ "mg_de_shorttrain",	CONTEXT_CSS_MAP_GROUP_SHORTTRAIN_MG },
	{ "mg_de_stmarc",		CONTEXT_CSS_MAP_GROUP_STMARC_MG },
	{ "mg_de_sugarcane",	CONTEXT_CSS_MAP_GROUP_SUGARCANE_MG },
	{ "mg_de_train",		CONTEXT_CSS_MAP_GROUP_TRAIN_MG },
	{ "mg_training",		CONTEXT_CSS_MAP_GROUP_TRAINING_MG },
	{ NULL,				0xFFFF },
};

static ContextValue_t g_LevelContexts[] =
{
	{ "cs_italy",		CONTEXT_CSS_LEVEL_ITALY },
	{ "cs_office",		CONTEXT_CSS_LEVEL_OFFICE },
	{ "de_aztec",		CONTEXT_CSS_LEVEL_AZTEC },
	{ "de_dust",		CONTEXT_CSS_LEVEL_DUST },
	{ "de_dust2",		CONTEXT_CSS_LEVEL_DUST2 },
	{ "de_inferno",		CONTEXT_CSS_LEVEL_INFERNO },
	{ "de_nuke",		CONTEXT_CSS_LEVEL_NUKE },
	{ "ar_baggage",		CONTEXT_CSS_LEVEL_BAGGAGE },
	{ "ar_shoots",		CONTEXT_CSS_LEVEL_SHOOTS },
	{ "de_lake",		CONTEXT_CSS_LEVEL_LAKE },
	{ "de_bank",		CONTEXT_CSS_LEVEL_BANK },
	{ "de_safehouse",	CONTEXT_CSS_LEVEL_SAFEHOUSE },
	{ "de_sugarcane",	CONTEXT_CSS_LEVEL_SUGARCANE },
	{ "de_stmarc",		CONTEXT_CSS_LEVEL_STMARC },
	{ "de_shorttrain",	CONTEXT_CSS_LEVEL_SHORTTRAIN },
	{ "de_train",		CONTEXT_CSS_LEVEL_TRAIN },
	{ "training1",		CONTEXT_CSS_LEVEL_TRAINING },
	{ NULL,				0xFFFF },
};

static ContextValue_t g_GameModeContexts[] =
{
	{ "casual",				CONTEXT_CSS_GAME_MODE_CASUAL },
	{ "competitive",		CONTEXT_CSS_GAME_MODE_COMPETITIVE },
	{ "gungameprogressive",	CONTEXT_CSS_GAME_MODE_GUNGAMEPROGRESSIVE },
	{ "gungametrbomb",		CONTEXT_CSS_GAME_MODE_GUNGAMEBOMB },
	{ NULL,					0xFFFF },
};

static ContextValue_t g_GameModeAsNumberContexts[] =
{
	{ "casual",				0 },
	{ "competitive",		1 },
	{ "competitive_unranked",		2 },
	{ "pro",				4 },
	{ "gungameprogressive",	10 },
	{ "gungameselect",		20 },
	{ "gungametrbomb",		30 },
	{ NULL,				0xFFFF },
};

static ContextValue_t g_GameTypeContexts[] =
{
	{ "classic",		CONTEXT_CSS_GAME_TYPE_CLASSIC },
	{ "gungame",		CONTEXT_CSS_GAME_TYPE_GUNGAME },
	{ NULL,				0xFFFF },
};

static ContextValue_t g_PrivacyContexts[] =
{
	{ "public",			CONTEXT_CSS_PRIVACY_PUBLIC },
	{ "private",		CONTEXT_CSS_PRIVACY_INVITE_ONLY },
	{ "friends",		CONTEXT_CSS_PRIVACY_FRIENDS },
	{ NULL,				0xFFFF },
};

#endif // _MM_TITLE_CONTEXTVALUES_H
