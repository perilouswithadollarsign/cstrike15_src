//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================
#ifndef GAME_ITEM_SCHEMA_H
#define GAME_ITEM_SCHEMA_H
#ifdef _WIN32
#pragma once
#endif

#if defined(TF_CLIENT_DLL) || defined(TF_DLL) || defined(TF_GC_DLL)
// TF
	class CTFItemSchema;
	class CTFItemDefinition;
	class CTFItemSystem;
	
	typedef CTFItemSchema		GameItemSchema_t;
	typedef CTFItemDefinition	GameItemDefinition_t;
	typedef CTFItemSystem		GameItemSystem_t;

	#include "tf_item_schema.h"
#elif defined( DOTA_CLIENT_DLL ) || defined( DOTA_DLL ) || defined ( DOTA_GC_DLL ) 
// DOTA
	class CDOTAItemSchema;
	class CDOTAItemDefinition;
	class CDOTAItemSystem;

	typedef CDOTAItemSchema		GameItemSchema_t;
	typedef CDOTAItemDefinition	GameItemDefinition_t;
	typedef CDOTAItemSystem		GameItemSystem_t;

	#include "econ/dota_item_schema.h"
#elif defined( PORTAL2 ) || defined( PORTAL2_GC_DLL )
// PORTAL2
	class CPortal2ItemSchema;
	class CPortal2ItemDefinition;
	class CPortal2ItemSystem;
	
	typedef CPortal2ItemSchema		GameItemSchema_t;
	typedef CPortal2ItemDefinition	GameItemDefinition_t;
	typedef CPortal2ItemSystem		GameItemSystem_t;

	#include "portal2_item_schema.h"	
#elif defined( CSTRIKE15 ) || defined( CSTRIKE_GC_DLL )
	class CCStrike15ItemSchema;
	class CCStrike15ItemDefinition;
	class CCStrike15ItemSystem;

	typedef CCStrike15ItemSchema		GameItemSchema_t;
	typedef CCStrike15ItemDefinition	GameItemDefinition_t;
	typedef CCStrike15ItemSystem		GameItemSystem_t;

	#include "cstrike15_item_schema.h"
#else
	// Fallback Case
	class CEconItemSchema;
	class CEconItemDefinition;
	class CEconItemSystem;

	typedef CEconItemSchema		GameItemSchema_t;
	typedef CEconItemDefinition	GameItemDefinition_t;
	typedef CEconItemSystem		GameItemSystem_t;

	#include "econ_item_schema.h"
#endif

extern GameItemSchema_t *GetItemSchema();

#endif // GAME_ITEM_SYSTEM_H
