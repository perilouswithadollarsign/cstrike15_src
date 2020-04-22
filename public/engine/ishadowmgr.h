//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =====//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef ISHADOWMGR_H
#define ISHADOWMGR_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "mathlib/vmatrix.h"


//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

class IMaterial;
class Vector;
class Vector2D;
struct model_t;
typedef unsigned short ModelInstanceHandle_t;
class IClientRenderable;
class ITexture;
struct FlashlightInstance_t;
struct FlashlightState_t;

// change this when the new version is incompatable with the old
#define ENGINE_SHADOWMGR_INTERFACE_VERSION	"VEngineShadowMgr002"


//-----------------------------------------------------------------------------
// Flags for the creation method
//-----------------------------------------------------------------------------
enum ShadowFlags_t
{
	SHADOW_FLAGS_FLASHLIGHT				= (1 << 0),
	SHADOW_FLAGS_SHADOW					= (1 << 1),
	SHADOW_FLAGS_SIMPLE_PROJECTION		= (1 << 2),

	// Update this if you add flags
	SHADOW_FLAGS_LAST_FLAG				= SHADOW_FLAGS_SIMPLE_PROJECTION
};

#define SHADOW_FLAGS_PROJECTED_TEXTURE_TYPE_MASK ( SHADOW_FLAGS_FLASHLIGHT | SHADOW_FLAGS_SHADOW | SHADOW_FLAGS_SIMPLE_PROJECTION )


//-----------------------------------------------------------------------------
//
// Shadow-related functionality exported by the engine
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// This is a handle	to shadows, clients can create as many as they want
//-----------------------------------------------------------------------------
typedef unsigned short ShadowHandle_t;

enum
{
	SHADOW_HANDLE_INVALID = (ShadowHandle_t)~0
};


//-----------------------------------------------------------------------------
// Used for the creation Flags field of CreateShadow
//-----------------------------------------------------------------------------
enum ShadowCreateFlags_t
{
	SHADOW_CACHE_VERTS			= ( 1 << 0 ),
	SHADOW_FLASHLIGHT			= ( 1 << 1 ),
	SHADOW_SIMPLE_PROJECTION	= ( 1 << 2 ),
	SHADOW_ANY_SPLITSCREEN_SLOT = ( 1 << 3 ),

	SHADOW_LAST_FLAG = SHADOW_SIMPLE_PROJECTION,
};


//-----------------------------------------------------------------------------
// Information about a particular shadow
//-----------------------------------------------------------------------------
struct ShadowInfo_t
{
	// Transforms from world space into texture space of the shadow
	VMatrix		m_WorldToShadow;

	// The shadow should no longer be drawn once it's further than MaxDist
	// along z in shadow texture coordinates.
	float			m_FalloffOffset;
	float			m_MaxDist;
	float			m_FalloffAmount;	// how much to lighten the shadow maximally
	Vector2D		m_TexOrigin;
	Vector2D		m_TexSize;
	unsigned char	m_FalloffBias;
};

typedef void (*ShadowDrawCallbackFn_t)( void * );

typedef void (*ShadowDrawCallbackFn_t)( void * );

//-----------------------------------------------------------------------------
// The engine's interface to the shadow manager
//-----------------------------------------------------------------------------
abstract_class IShadowMgr
{
public:
	// Create, destroy shadows (see ShadowCreateFlags_t for creationFlags)
	virtual ShadowHandle_t CreateShadow( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags ) = 0;
	virtual void DestroyShadow( ShadowHandle_t handle ) = 0;

	// Resets the shadow material (useful for shadow LOD.. doing blobby at distance) 
	virtual void SetShadowMaterial( ShadowHandle_t handle, IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy ) = 0;

	// Shadow opacity
//	virtual void SetShadowOpacity( ShadowHandle_t handle, float alpha ) = 0;
//	virtual float GetShadowOpacity( ShadowHandle_t handle ) const = 0;

	// Project a shadow into the world
	// The two points specify the upper left coordinate and the lower-right
	// coordinate of the shadow specified in a shadow "viewplane". The
	// projection matrix is a shadow viewplane->world transformation,
	// and can be orthographic orperspective.

	// I expect that the client DLL will call this method any time the shadow
	// changes because the light changes, or because the entity casting the
	// shadow moves

	// Note that we can't really control the shadows from the engine because
	// the engine only knows about pevs, which don't exist on the client

	// The shadow matrix specifies a world-space transform for the shadow
	// the shadow is projected down the z direction, and the origin of the
	// shadow matrix is the origin of the projection ray. The size indicates
	// the shadow size measured in the space of the shadow matrix; the
	// shadow goes from +/- size.x/2 along the x axis of the shadow matrix
	// and +/- size.y/2 along the y axis of the shadow matrix.
	virtual void ProjectShadow( ShadowHandle_t handle, const Vector &origin, 
		const Vector& projectionDir, const VMatrix& worldToShadow, const Vector2D& size,
		int nLeafCount, const int *pLeafList,
		float maxHeight, float falloffOffset, float falloffAmount, const Vector &vecCasterOrigin ) = 0;

	virtual void ProjectFlashlight( ShadowHandle_t handle, const VMatrix &worldToShadow, int nLeafCount, const int *pLeafList ) = 0;

	// Gets at information about a particular shadow
	virtual const ShadowInfo_t &GetInfo( ShadowHandle_t handle ) = 0;

	virtual const Frustum_t &GetFlashlightFrustum( ShadowHandle_t handle ) = 0;

	// Methods related to shadows on brush models
	virtual void AddShadowToBrushModel( ShadowHandle_t handle, 
		model_t* pModel, const Vector& origin, const QAngle& angles ) = 0;

	// Removes all shadows from a brush model
	virtual void RemoveAllShadowsFromBrushModel( model_t* pModel ) = 0;

	// Sets the texture coordinate range for a shadow...
	virtual void SetShadowTexCoord( ShadowHandle_t handle, float x, float y, float w, float h ) = 0;

	// Methods related to shadows on studio models
	virtual void AddShadowToModel( ShadowHandle_t shadow, ModelInstanceHandle_t instance ) = 0;
	virtual void RemoveAllShadowsFromModel( ModelInstanceHandle_t instance ) = 0;

	// Set extra clip planes related to shadows...
	// These are used to prevent pokethru and back-casting
	virtual void ClearExtraClipPlanes( ShadowHandle_t shadow ) = 0;
	virtual void AddExtraClipPlane( ShadowHandle_t shadow, const Vector& normal, float dist ) = 0;

	// Allows us to disable particular shadows
	virtual void EnableShadow( ShadowHandle_t shadow, bool bEnable ) = 0;

	// Set the darkness falloff bias
	virtual void SetFalloffBias( ShadowHandle_t shadow, unsigned char ucBias ) = 0;

	// Update the state for a flashlight.
	virtual void UpdateFlashlightState( ShadowHandle_t shadowHandle, const FlashlightState_t &lightState ) = 0;

	virtual void DrawFlashlightDepthTexture( ) = 0;

	virtual ShadowHandle_t CreateShadowEx( IMaterial* pMaterial, IMaterial* pModelMaterial, void* pBindProxy, int creationFlags, int nEntIndex ) = 0;

	virtual void SetFlashlightDepthTexture( ShadowHandle_t shadowHandle, ITexture *pFlashlightDepthTexture, unsigned char ucShadowStencilBit ) = 0;

	virtual const FlashlightState_t &GetFlashlightState( ShadowHandle_t handle ) = 0;

	virtual void SetFlashlightRenderState( ShadowHandle_t handle ) = 0;

	virtual void DrawVolumetrics() = 0;

	virtual int GetNumShadowsOnModel( ModelInstanceHandle_t instance ) = 0;
	virtual int GetShadowsOnModel( ModelInstanceHandle_t instance, ShadowHandle_t* pShadowArray, bool bNormalShadows, bool bFlashlightShadows ) = 0;

	virtual void FlashlightDrawCallback( ShadowDrawCallbackFn_t pCallback, void *pData ) = 0; //used to draw each additive flashlight pass. The callback is called once per flashlight state for an additive pass.

	//Way for the client to determine which flashlight to use in single-pass modes. Does not actually enable the flashlight in any way.
	virtual void SetSinglePassFlashlightRenderState( ShadowHandle_t handle ) = 0;

	//Enable/Disable the flashlight state set with SetSinglePassFlashlightRenderState.
	virtual void PushSinglePassFlashlightStateEnabled( bool bEnable ) = 0;
	virtual void PopSinglePassFlashlightStateEnabled( void ) = 0;

	virtual bool SinglePassFlashlightModeEnabled( void ) = 0;

	// Determine a unique list of flashlights which hit at least one of the specified models
	// Accepts an instance count and an array of ModelInstanceHandle_ts.
	// Returns the number of FlashlightInstance_ts it's found that affect the models.
	// Also fills in a mask of which flashlights affect each ModelInstanceHandle_t
	// There can be at most MAX_FLASHLIGHTS_PER_INSTANCE_DRAW_CALL pFlashlights,
	// and the size of the pModelUsageMask array must be nInstanceCount.
	virtual int SetupFlashlightRenderInstanceInfo( ShadowHandle_t *pUniqueFlashlights, uint32 *pModelUsageMask, int nUsageStride, int nInstanceCount, const ModelInstanceHandle_t *pInstance ) = 0;

	// Returns the flashlight state for multiple flashlights
	virtual void GetFlashlightRenderInfo( FlashlightInstance_t *pFlashlightState, int nCount, const ShadowHandle_t *pHandles ) = 0;

	virtual void RemoveAllDecalsFromShadow( ShadowHandle_t handle ) = 0;

	virtual void SkipShadowForEntity( int nEntIndex ) = 0;

	//designed for portal usage. Recursive drawing leaves the scissor rect in a bad state for translucents
	virtual void PushFlashlightScissorBounds( void ) = 0;
	virtual void PopFlashlightScissorBounds( void ) = 0;

	// Disables projected shadows
	virtual void DisableDropShadows() = 0;
};


#endif
