//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =====//
//
// Purpose: 
//
//===========================================================================//

#ifndef TOOLFRAMEWORK_CLIENT_H
#define TOOLFRAMEWORK_CLIENT_H

#ifdef _WIN32
#pragma once
#endif

#include "toolframework/itoolentity.h"
#include "vstdlib/ikeyvaluessystem.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class KeyValues;
struct AudioState_t;


//-----------------------------------------------------------------------------
// Posts a message to all tools
//-----------------------------------------------------------------------------
void ToolFramework_PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg );


//-----------------------------------------------------------------------------
// Should we render with a 3rd person camera?
//-----------------------------------------------------------------------------
bool ToolFramework_IsThirdPersonCamera( );


//-----------------------------------------------------------------------------
// Are tools enabled? 
//-----------------------------------------------------------------------------
#ifndef NO_TOOLFRAMEWORK
bool ToolsEnabled();
#else
#define ToolsEnabled() 0
#endif


//-----------------------------------------------------------------------------
// Recorded temp entity structures
//-----------------------------------------------------------------------------
enum TERecordingType_t
{
	TE_DYNAMIC_LIGHT = 0,
	TE_WORLD_DECAL,
	TE_DISPATCH_EFFECT,
	TE_MUZZLE_FLASH,
	TE_ARMOR_RICOCHET,
	TE_METAL_SPARKS,
	TE_SMOKE,
	TE_SPARKS,
	TE_BLOOD_SPRITE,
	TE_BREAK_MODEL,
	TE_GLOW_SPRITE,
	TE_PHYSICS_PROP,
	TE_SPRITE_SINGLE,
	TE_SPRITE_SPRAY,
	TE_CONCUSSIVE_EXPLOSION,
	TE_BLOOD_STREAM,
	TE_SHATTER_SURFACE,
	TE_DECAL,
	TE_PROJECT_DECAL,
	TE_EXPLOSION,

	TE_RECORDING_TYPE_COUNT,
};


//-----------------------------------------------------------------------------
// View manipulation
//-----------------------------------------------------------------------------
void ToolFramework_AdjustEngineViewport( int& x, int& y, int& width, int& height );
bool ToolFramework_SetupEngineView( Vector &origin, QAngle &angles, float &fov );
bool ToolFramework_SetupAudioState( AudioState_t &audioState );

//-----------------------------------------------------------------------------
// Helper class to indicate ownership of effects
//-----------------------------------------------------------------------------
class CRecordEffectOwner
{
public:
	CRecordEffectOwner( C_BaseEntity *pEntity, bool bIsViewModel = false );
	~CRecordEffectOwner();

private:
	bool m_bToolsEnabled;
};



//only ever create global/static instances of this to auto-register your entity keyvalues data handler
class CIFM_EntityKeyValuesHandler_AutoRegister
{
public:
	explicit CIFM_EntityKeyValuesHandler_AutoRegister( const char *szHandlerID );

	virtual void HandleData_PreUpdate( void ) {}; //called once before any received data is distributed to its handlers
	virtual void HandleData( KeyValues *pKeyValues ) = 0;
	virtual void HandleData_PostUpdate( void ) {}; //called once after all received data is distributed to its handlers
	
	static void AllHandlers_PreUpdate( void );
	static void FindAndCallHandler( const char *szHandlerID, KeyValues *pKeyValues );
	static void AllHandlers_PostUpdate( void );

	virtual void HandleData_RemoveAll( void ) {};
	static void AllHandlers_RemoveAll( void );

	static HKeySymbol GetGameKeyValuesKeySymbol( void );
	static const char *GetGameKeyValuesKeyString( void );
	static HKeySymbol GetHandlerIDKeySymbol( void );
	static const char *GetHandlerIDKeyString( void );
	static KeyValues *FindOrCreateNonConformantKeyValues( KeyValues *pParentKV );

private:


	const char *m_szHandlerID;
	CIFM_EntityKeyValuesHandler_AutoRegister *m_pNext;
	static CIFM_EntityKeyValuesHandler_AutoRegister *s_pRegisteredHandlers;
};


//assuming you've stored "entIndex" into your recorded values, this will create/destroy a unique instance per unique index
class CIFM_EntityKeyValuesHandler_RecreateEntities : CIFM_EntityKeyValuesHandler_AutoRegister
{
public:
	explicit CIFM_EntityKeyValuesHandler_RecreateEntities( const char *szHandlerID );

	virtual void HandleData_PreUpdate( void );
	virtual void HandleData( KeyValues *pKeyValues );
	virtual void HandleData_PostUpdate( void );
	virtual void HandleData_RemoveAll( void );

	virtual void *CreateInstance( void ) = 0; //make a new instance of your entity
	virtual void DestroyInstance( void *pEntity ) = 0;

	virtual void HandleInstance( void *pEntity, KeyValues *pKeyValues ) = 0; //update your entity.

private:
	struct RecordedEntity_t
	{
		void *pEntity;
		int iEntIndex;
		bool bTouched;
	};

	CUtlVector<RecordedEntity_t> m_PlaybackEntities;
};

#endif // TOOLFRAMEWORK_CLIENT_H