//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef ITOOLENTITY_H
#define ITOOLENTITY_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "tier1/utlvector.h"
#include "color.h"
#include "basehandle.h"
#include "iclientrenderable.h"
#include "engine/ishadowmgr.h"
#include "engine/ivmodelinfo.h"
#include "engine/IClientLeafSystem.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IServerEntity;
class IClientEntity;
class IToolSystem;
class IClientRenderable;
class Vector;
class QAngle;


//-----------------------------------------------------------------------------
// Safe accessor to an entity
//-----------------------------------------------------------------------------
typedef unsigned int HTOOLHANDLE;
enum
{
	HTOOLHANDLE_INVALID = 0
};


//-----------------------------------------------------------------------------
// If you change this, change the flags in IClientShadowMgr.h also
//-----------------------------------------------------------------------------
enum ClientShadowFlags_t
{
	SHADOW_FLAGS_USE_RENDER_TO_TEXTURE	= (SHADOW_FLAGS_LAST_FLAG<<1),
	SHADOW_FLAGS_ANIMATING_SOURCE		= (SHADOW_FLAGS_LAST_FLAG<<2),
	SHADOW_FLAGS_USE_DEPTH_TEXTURE		= (SHADOW_FLAGS_LAST_FLAG<<3),
	SHADOW_FLAGS_CUSTOM_DRAW			= (SHADOW_FLAGS_LAST_FLAG<<4),
	// Update this if you add flags
	CLIENT_SHADOW_FLAGS_LAST_FLAG		= SHADOW_FLAGS_CUSTOM_DRAW
};


//-----------------------------------------------------------------------------
// Opaque pointer returned from Find* methods, don't store this, you need to 
// Attach it to a tool entity or discard after searching
//-----------------------------------------------------------------------------
typedef void *EntitySearchResult;
typedef void *ParticleSystemSearchResult;


//-----------------------------------------------------------------------------
// Purpose: Client side tool interace (right now just handles IClientRenderables).
//  In theory could support hooking into client side entities directly
//-----------------------------------------------------------------------------
class IClientTools : public IBaseInterface
{
public:
	// Allocates or returns the handle to an entity previously found using the Find* APIs below
	virtual HTOOLHANDLE		AttachToEntity( EntitySearchResult entityToAttach ) = 0;
	virtual void			DetachFromEntity( EntitySearchResult entityToDetach ) = 0;

	virtual EntitySearchResult	GetEntity( HTOOLHANDLE handle ) = 0;

	// Checks whether a handle is still valid.
	virtual bool			IsValidHandle( HTOOLHANDLE handle ) = 0;

	// Iterates the list of entities which have been associated with tools
	virtual int				GetNumRecordables() = 0;
	virtual HTOOLHANDLE		GetRecordable( int index ) = 0;

	// Iterates through ALL entities (separate list for client vs. server)
	virtual EntitySearchResult	NextEntity( EntitySearchResult currentEnt ) = 0;
	EntitySearchResult			FirstEntity() { return NextEntity( NULL ); }

	// Use this to turn on/off the presence of an underlying game entity
	virtual void			SetEnabled( HTOOLHANDLE handle, bool enabled ) = 0;
	// Use this to tell an entity to post "state" to all listening tools
	virtual void			SetRecording( HTOOLHANDLE handle, bool recording ) = 0;
	// Some entities are marked with ShouldRecordInTools false, such as ui entities, etc.
	virtual bool			ShouldRecord( HTOOLHANDLE handle ) = 0;

	virtual HTOOLHANDLE		GetToolHandleForEntityByIndex( int entindex ) = 0;

	virtual int				GetModelIndex( HTOOLHANDLE handle ) = 0;
	virtual const char*		GetModelName ( HTOOLHANDLE handle ) = 0;
	virtual const char*		GetClassname ( HTOOLHANDLE handle ) = 0;

	virtual void			AddClientRenderable( IClientRenderable *pRenderable, bool bDrawWithViewModels, RenderableTranslucencyType_t nType, RenderableModelType_t nModelType = RENDERABLE_MODEL_UNKNOWN_TYPE ) = 0;
	virtual void			RemoveClientRenderable( IClientRenderable *pRenderable ) = 0;
	virtual void			SetTranslucencyType( IClientRenderable *pRenderable, RenderableTranslucencyType_t nType ) = 0;
	virtual void			MarkClientRenderableDirty( IClientRenderable *pRenderable ) = 0;
    virtual void			UpdateProjectedTexture( ClientShadowHandle_t h, bool bForce ) = 0;

	virtual bool			DrawSprite( IClientRenderable *pRenderable, float scale, float frame, int rendermode, int renderfx, const Color &color, float flProxyRadius, int *pVisHandle ) = 0;
	virtual void			DrawSprite( const Vector &vecOrigin, float flWidth, float flHeight, color32 color ) = 0;

	virtual EntitySearchResult	GetLocalPlayer() = 0;
	virtual bool			GetLocalPlayerEyePosition( Vector& org, QAngle& ang, float &fov ) = 0;

	// See ClientShadowFlags_t above
	virtual ClientShadowHandle_t CreateShadow( CBaseHandle handle, int nFlags ) = 0;
	virtual void			DestroyShadow( ClientShadowHandle_t h ) = 0;

	virtual ClientShadowHandle_t CreateFlashlight( const FlashlightState_t &lightState ) = 0;
	virtual void			DestroyFlashlight( ClientShadowHandle_t h ) = 0;
	virtual void			UpdateFlashlightState( ClientShadowHandle_t h, const FlashlightState_t &lightState ) = 0;

	virtual void			AddToDirtyShadowList( ClientShadowHandle_t h, bool force = false ) = 0;
	virtual void			MarkRenderToTextureShadowDirty( ClientShadowHandle_t h ) = 0;

	// Global toggle for recording
	virtual void			EnableRecordingMode( bool bEnable ) = 0;
	virtual bool			IsInRecordingMode() const = 0;

	// Trigger a temp entity
	virtual void			TriggerTempEntity( KeyValues *pKeyValues ) = 0;

	// get owning weapon (for viewmodels)
	virtual int				GetOwningWeaponEntIndex( int entindex ) = 0;
	virtual int				GetEntIndex( EntitySearchResult entityToAttach ) = 0;

	virtual int				FindGlobalFlexcontroller( char const *name ) = 0;
	virtual char const		*GetGlobalFlexControllerName( int idx ) = 0;

	// helper for traversing ownership hierarchy
	virtual EntitySearchResult	GetOwnerEntity( EntitySearchResult currentEnt ) = 0;

	// common and useful types to query for hierarchically
	virtual bool			IsPlayer			( EntitySearchResult currentEnt ) = 0;
	virtual bool			IsCombatCharacter	( EntitySearchResult currentEnt ) = 0;
	virtual bool			IsNPC				( EntitySearchResult currentEnt ) = 0;
	virtual bool			IsRagdoll			( EntitySearchResult currentEnt ) = 0;
	virtual bool			IsViewModel			( EntitySearchResult currentEnt ) = 0;
	virtual bool			IsViewModelOrAttachment( EntitySearchResult currentEnt ) = 0;
	virtual bool			IsWeapon			( EntitySearchResult currentEnt ) = 0;
	virtual bool			IsSprite			( EntitySearchResult currentEnt ) = 0;
	virtual bool			IsProp				( EntitySearchResult currentEnt ) = 0;
	virtual bool			IsBrush				( EntitySearchResult currentEnt ) = 0;

	virtual Vector			GetAbsOrigin( HTOOLHANDLE handle ) = 0;
	virtual QAngle			GetAbsAngles( HTOOLHANDLE handle ) = 0;

	// This reloads a portion or all of a particle definition file.
	// It's up to the client to decide if it cares about this file
	// Use a UtlBuffer to crack the data
	virtual void			ReloadParticleDefintions( const char *pFileName, const void *pBufData, int nLen ) = 0;

	// ParticleSystem iteration, query, modification
	virtual ParticleSystemSearchResult	FirstParticleSystem() { return NextParticleSystem( NULL ); }
	virtual ParticleSystemSearchResult	NextParticleSystem( ParticleSystemSearchResult sr ) = 0;
	virtual void						SetRecording( ParticleSystemSearchResult sr, bool bRecord ) = 0;

	// Sends a mesage from the tool to the client
	virtual void			PostToolMessage( KeyValues *pKeyValues ) = 0;

	// Indicates whether the client should render particle systems
	virtual void			EnableParticleSystems( bool bEnable ) = 0;

	// Is the game rendering in 3rd person mode?
	virtual bool			IsRenderingThirdPerson() const = 0;
};

#define VCLIENTTOOLS_INTERFACE_VERSION "VCLIENTTOOLS001"


class CEntityRespawnInfo
{
public:
	int m_nHammerID;
	const char *m_pEntText;
};


//-----------------------------------------------------------------------------
// Purpose: Interface from engine to tools for manipulating entities
//-----------------------------------------------------------------------------
class IServerTools : public IBaseInterface
{
public:
	virtual IServerEntity *GetIServerEntity( IClientEntity *pClientEntity ) = 0;
	virtual bool SnapPlayerToPosition( const Vector &org, const QAngle &ang, IClientEntity *pClientPlayer = NULL ) = 0;
	virtual bool GetPlayerPosition( Vector &org, QAngle &ang, IClientEntity *pClientPlayer = NULL ) = 0;
	virtual bool SetPlayerFOV( int fov, IClientEntity *pClientPlayer = NULL ) = 0;
	virtual int GetPlayerFOV( IClientEntity *pClientPlayer = NULL ) = 0;
	virtual bool IsInNoClipMode( IClientEntity *pClientPlayer = NULL ) = 0;

	// entity searching
	virtual void *FirstEntity( void ) = 0;
	virtual void *NextEntity( void *pEntity ) = 0;
	virtual void *FindEntityByHammerID( int iHammerID ) = 0;

	// entity query
	virtual bool GetKeyValue( void *pEntity, const char *szField, char *szValue, int iMaxLen ) = 0;
	virtual bool SetKeyValue( void *pEntity, const char *szField, const char *szValue ) = 0;
	virtual bool SetKeyValue( void *pEntity, const char *szField, float flValue ) = 0;
	virtual bool SetKeyValue( void *pEntity, const char *szField, const Vector &vecValue ) = 0;

	// entity spawning
	virtual void *CreateEntityByName( const char *szClassName ) = 0;
	virtual void DispatchSpawn( void *pEntity ) = 0;
	virtual bool DestroyEntityByHammerId( int iHammerID ) = 0;
	
	// This function respawns the entity into the same entindex slot AND tricks the EHANDLE system into thinking it's the same
	// entity version so anyone holding an EHANDLE to the entity points at the newly-respawned entity.
	virtual bool RespawnEntitiesWithEdits( CEntityRespawnInfo *pInfos, int nInfos ) = 0;

	// This reloads a portion or all of a particle definition file.
	// It's up to the server to decide if it cares about this file
	// Use a UtlBuffer to crack the data
	virtual void ReloadParticleDefintions( const char *pFileName, const void *pBufData, int nLen ) = 0;

	virtual void AddOriginToPVS( const Vector &org ) = 0;
	virtual void MoveEngineViewTo( const Vector &vPos, const QAngle &vAngles ) = 0;
	
	// Call UTIL_Remove on the entity.
	virtual void RemoveEntity( int nHammerID ) = 0;
};

#define VSERVERTOOLS_INTERFACE_VERSION "VSERVERTOOLS001"

#endif // ITOOLENTITY_H
