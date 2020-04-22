//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef ITOOLFRAMEWORK_H
#define ITOOLFRAMEWORK_H
#ifdef _WIN32
#pragma once
#endif

#include "appframework/iappsystem.h"
#include "materialsystem/imaterialproxy.h"
#include "toolframework/itoolentity.h"
#include "mathlib/vector.h"
#include "color.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IToolSystem;
struct SpatializationInfo_t;
class KeyValues;
class CBoneList;


//-----------------------------------------------------------------------------
// Standard messages
//-----------------------------------------------------------------------------
struct EffectRecordingState_t
{
	bool m_bVisible : 1;
	bool m_bThirdPerson : 1;
	Color m_Color;
	float m_Scale;
	const char *m_pMaterialName;
	int m_nAttachment;
	Vector m_vecAttachment; // only used if m_nAttachment is -1
};

struct BaseEntityRecordingState_t
{
	BaseEntityRecordingState_t() :	
		m_flTime( 0.0f ),
		m_pModelName( 0 ),
		m_nOwner( -1 ),
		m_fEffects( 0 ),
		m_bVisible( false ),
		m_bRecordFinalVisibleSample( false ),
		m_numEffects( 0 ),
		m_pEffects( NULL ),
		m_nFollowEntity( -1 )
	{
		m_vecRenderOrigin.Init();
		m_vecRenderAngles.Init();
	}

	float m_flTime;
	const char *m_pModelName;
	int m_nOwner;
	int m_fEffects;
	bool m_bVisible : 1;
	bool m_bRecordFinalVisibleSample : 1;
	Vector m_vecRenderOrigin;
	QAngle m_vecRenderAngles;
	int m_nFollowEntity;

	int m_numEffects;
	EffectRecordingState_t *m_pEffects;
};

struct SpriteRecordingState_t
{
	float m_flRenderScale;
	float m_flFrame;
	int m_nRenderMode;
	bool m_nRenderFX;
	Color m_Color;
	float m_flProxyRadius;
};

struct BaseAnimatingHighLevelRecordingState_t
{
	BaseAnimatingHighLevelRecordingState_t()
	:	m_bClearIkTargets( false ),
		m_bIsRagdoll( false ),
		m_bShouldCreateIkContext( false ),
		m_nNumPoseParams( 0 ),
		m_flCycle( 0.0f ),
		m_flPlaybackRate( 1.0f ),
		m_flCycleRate( 0.0f ),
		m_nFrameCount( 0 ),
		m_bInterpEffectActive( false )
	{
	}

	bool m_bClearIkTargets;
	bool m_bIsRagdoll;
	bool m_bShouldCreateIkContext;
	int m_nNumPoseParams;

	float m_flCycle;
	float m_flPlaybackRate;
	float m_flCycleRate;
	int m_nFrameCount;

	float m_flPoseParameter[MAXSTUDIOPOSEPARAM];

	bool m_bInterpEffectActive;
};

struct BaseAnimatingRecordingState_t
{
	BaseAnimatingHighLevelRecordingState_t m_highLevelState;

	int m_nSkin;
	int m_nBody;
	int m_nSequence;
	CBoneList *m_pBoneList;
};

struct BaseFlexRecordingState_t
{
	int m_nFlexCount;
	float *m_pDestWeight;
	Vector m_vecViewTarget;
};

struct CameraRecordingState_t
{
	bool m_bThirdPerson;
	float m_flFOV;
	Vector m_vecEyePosition;
	QAngle m_vecEyeAngles;
	bool m_bPlayerEyeIsPortalled; //for portal. Need to reverse some portal recursion draw logic when the player eye and body straddle a portal
};

struct MonitorRecordingState_t
{
	bool	m_bActive;
	float	m_flFOV;
	bool	m_bFogEnabled;
	float	m_flFogStart;
	float	m_flFogEnd;
	Color	m_FogColor;
};

struct EntityTeleportedRecordingState_t
{
	Vector m_vecTo;
	QAngle m_qaTo;
	bool m_bTeleported;
	bool m_bViewOverride;
	matrix3x4_t m_teleportMatrix;
};

struct PortalRecordingState_t
{
	int				m_nPortalId;
	int				m_nLinkedPortalId;
	float			m_fStaticAmount;
	float			m_fSecondaryStaticAmount;
	float			m_fOpenAmount;
	float			m_fHalfWidth;
	float			m_fHalfHeight;
	const char *	m_portalType;
	bool			m_bIsPortal2; //for any set of portals, one must be portal 1, and the other portal 2. Uses different render targets
};

struct ParticleSystemCreatedState_t
{
	int				m_nParticleSystemId;
	const char *	m_pName;
	float			m_flTime;
	int				m_nOwner;
};

struct ParticleSystemDestroyedState_t
{
	int				m_nParticleSystemId;
	float			m_flTime;
};

struct ParticleSystemStopEmissionState_t
{
	int				m_nParticleSystemId;
	float			m_flTime;
	bool			m_bInfiniteOnly;
};

struct ParticleSystemSetControlPointObjectState_t
{
	int				m_nParticleSystemId;
	float			m_flTime;
	int				m_nControlPoint;
	int				m_nObject;
};

struct ParticleSystemSetControlPointPositionState_t
{
	int				m_nParticleSystemId;
	float			m_flTime;
	int				m_nControlPoint;
	Vector			m_vecPosition;
};

struct ParticleSystemSetControlPointOrientationState_t
{
	int				m_nParticleSystemId;
	float			m_flTime;
	int				m_nControlPoint;
	Quaternion		m_qOrientation;
};


//-----------------------------------------------------------------------------
// Purpose: This interface lives in the engine and handles loading up/unloading all 
//  available tools
//-----------------------------------------------------------------------------
class IToolFrameworkInternal : public IAppSystem
{
public: // Client Hooks
	virtual bool	ClientInit( CreateInterfaceFn clientFactory ) = 0; 
	virtual void	ClientShutdown() = 0;

	// Level init, shutdown
	virtual void	ClientLevelInitPreEntityAllTools() = 0;
	// entities are created / spawned / precached here
	virtual void	ClientLevelInitPostEntityAllTools() = 0;

	virtual void	ClientLevelShutdownPreEntityAllTools() = 0;
	// Entities are deleted / released here...
	virtual void	ClientLevelShutdownPostEntityAllTools() = 0;

	virtual void	ClientPreRenderAllTools() = 0;
	virtual void	ClientPostRenderAllTools() = 0;

	// Should we render with a thirdperson camera?
	virtual bool	IsThirdPersonCamera() = 0;

	// is the current tool recording?
	virtual bool	IsToolRecording() = 0;

public:  // Server Hooks
	// Level init, shutdown
	virtual bool	ServerInit( CreateInterfaceFn serverFactory ) = 0; 
	virtual void	ServerShutdown() = 0;

	virtual void	ServerLevelInitPreEntityAllTools() = 0;
	// entities are created / spawned / precached here
	virtual void	ServerLevelInitPostEntityAllTools() = 0;

	virtual void	ServerLevelShutdownPreEntityAllTools() = 0;
	// Entities are deleted / released here...
	virtual void	ServerLevelShutdownPostEntityAllTools() = 0;
	// end of level shutdown

	// Called each frame before entities think
	virtual void	ServerFrameUpdatePreEntityThinkAllTools() = 0;
	// called after entities think
	virtual void	ServerFrameUpdatePostEntityThinkAllTools() = 0;
	virtual void	ServerPreClientUpdateAllTools() = 0;

	virtual void	ServerPreSetupVisibilityAllTools() = 0;

public:  // Other Hooks
	// If any tool returns false, the engine will not actually quit
	// FIXME:  Not implemented yet
	virtual bool	CanQuit() = 0;

	// Called at end of Host_Init
	virtual bool	PostInit() = 0;

	virtual void	Think( bool finalTick ) = 0;

	virtual void	PostMessage( KeyValues *msg ) = 0;

	virtual bool	GetSoundSpatialization( int iUserData, int guid, SpatializationInfo_t& info ) = 0;

	virtual void	HostRunFrameBegin() = 0;
	virtual void	HostRunFrameEnd() = 0;

	virtual void	RenderFrameBegin() = 0;
	virtual void	RenderFrameEnd() = 0;

	// Paintmode is an enum declared in enginevgui.h
	virtual void	VGui_PreRenderAllTools( int paintMode ) = 0;
	virtual void	VGui_PostRenderAllTools( int paintMode ) = 0;

	virtual void	VGui_PreSimulateAllTools() = 0;
	virtual void	VGui_PostSimulateAllTools() = 0;

	// Are we using tools?
	virtual bool	InToolMode() = 0;

	// Should the game be allowed to render the world?
	virtual bool	ShouldGameRenderView() = 0;

	// Should sounds from the game be played?
	virtual bool	ShouldGamePlaySounds() = 0;

	virtual IMaterialProxy *LookupProxy( const char *proxyName ) = 0;

public:  // general framework hooks
	virtual bool		LoadFilmmaker() = 0;
	virtual void		UnloadFilmmaker() = 0;
	virtual int			GetToolCount() = 0;
	virtual char const	*GetToolName( int index ) = 0;
	virtual void		SwitchToTool( int index ) = 0;
	virtual IToolSystem *SwitchToTool( const char *pToolName ) = 0;
	virtual bool		IsTopmostTool( const IToolSystem *sys ) = 0;
	virtual const IToolSystem *GetToolSystem( int index ) const = 0;
	virtual IToolSystem *GetTopmostTool() = 0;
	// If module not already loaded, loads it and optionally switches to first tool in module.  Returns false if load failed or tool already loaded
	virtual bool		LoadToolModule( char const *pToolModule, bool bSwitchToFirst ) = 0;
};

// Expose to rest of engine as a singleton
extern IToolFrameworkInternal *toolframework;

// Exposed to launcher to automatically add AppSystemGroup hooks
#define VTOOLFRAMEWORK_INTERFACE_VERSION  "VTOOLFRAMEWORKVERSION002"

#endif // ITOOLFRAMEWORK_H
