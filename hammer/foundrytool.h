//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Foundry tool; main UI smarts class
//
//=============================================================================

#ifndef FOUNDRYTOOL_H
#define FOUNDRYTOOL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "datamodel/idatamodel.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeEditorTypeDictionary;
class CDmeVMFEntity;
class CMapDoc;

namespace vgui
{
	class Panel;
}


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------
extern CDmeEditorTypeDictionary *g_pEditorTypeDict;


//-----------------------------------------------------------------------------
// Allows the doc to call back into the Foundry editor tool
//-----------------------------------------------------------------------------
abstract_class IFoundryDocCallback
{
public:
	// Called by the doc when the data changes
	virtual void OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags ) = 0;
};


//-----------------------------------------------------------------------------
// Global methods of the foundry tool
//-----------------------------------------------------------------------------
abstract_class IFoundryTool
{
public:
	// Gets at the rool panel (for modal dialogs)
	virtual vgui::Panel *GetRootPanel() = 0;

	// Gets the registry name (for saving settings)
	virtual const char *GetRegistryName() = 0;

	// Updates live entity in game with the given properties
	virtual bool UpdateEntity( int iHammerID, CUtlVector<char*> &keys, CUtlVector<char*> &values ) = 0;

	// Destroys live entity and creates a new one with the given properties
	virtual void RespawnEntitiesWithEdits( CMapClass **ppEntities, int nEntities ) = 0;

	// Destroy a live entity
	virtual void DestroyEntity( int iHammerID ) = 0;

	// Switch to the engine's window with the engine driving input.
	virtual void SwitchToEngine() = 0;

	// Move the engine's view to this place.
	virtual void MoveEngineViewTo( const Vector &vPos, const QAngle &vAngles ) = 0;

	// Run a console command.
	virtual void ConsoleCommand( const char *pConCommand ) = 0;

	// Hooked to foundry_draw_hammer_models.
	virtual bool ShouldRender3DModels() = 0;

	// Called when a CMapDoc goes away.
	virtual void OnMapDocDestroy( CMapDoc *pDoc ) = 0;
};

extern IFoundryTool *g_pFoundryTool;


#endif // FOUNDRYTOOL_H
