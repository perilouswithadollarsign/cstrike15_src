//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IVMODELINFO_H
#define IVMODELINFO_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "datacache/imdlcache.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IMaterial;
class KeyValues;
struct vcollide_t;
struct model_t;
class Vector;
class QAngle;
class CGameTrace;
struct cplane_t;
typedef CGameTrace trace_t;
struct studiohdr_t;
struct virtualmodel_t;
typedef unsigned char byte;
struct virtualterrainparams_t;
class CPhysCollide;
typedef unsigned short MDLHandle_t;
class CUtlBuffer;
class IClientRenderable;


//-----------------------------------------------------------------------------
// Indicates the type of translucency of an unmodulated renderable
//-----------------------------------------------------------------------------
enum RenderableTranslucencyType_t
{
	RENDERABLE_IS_OPAQUE = 0,
	RENDERABLE_IS_TRANSLUCENT,
	RENDERABLE_IS_TWO_PASS,	// has both translucent and opaque sub-partsa
};


//-----------------------------------------------------------------------------
// Purpose: a callback class that is notified when a model has finished loading
//-----------------------------------------------------------------------------
abstract_class IModelLoadCallback
{
public:
	virtual void OnModelLoadComplete( const model_t* pModel ) = 0;

protected:
	// Protected destructor so that nobody tries to delete via this interface.
	// Automatically unregisters if the callback is destroyed while still pending.
	~IModelLoadCallback();
};


//-----------------------------------------------------------------------------
// Purpose: Automate refcount tracking on a model index
//-----------------------------------------------------------------------------
class CRefCountedModelIndex
{
private:
	int m_nIndex;
public:
	CRefCountedModelIndex() : m_nIndex( -1 ) { }
	~CRefCountedModelIndex() { Set( -1 ); }

	CRefCountedModelIndex( const CRefCountedModelIndex& src ) : m_nIndex( -1 ) { Set( src.m_nIndex ); }
	CRefCountedModelIndex& operator=( const CRefCountedModelIndex& src ) { Set( src.m_nIndex ); return *this; }

	explicit CRefCountedModelIndex( int i ) : m_nIndex( -1 ) { Set( i ); }
	CRefCountedModelIndex& operator=( int i ) { Set( i ); return *this; }

	int Get() const { return m_nIndex; }
	void Set( int i );
	void Clear() { Set( -1 ); }

	operator int () const { return m_nIndex; }
};


//-----------------------------------------------------------------------------
// Model info interface
//-----------------------------------------------------------------------------

// change this when the new version is incompatable with the old
#define VMODELINFO_CLIENT_INTERFACE_VERSION	"VModelInfoClient004"
#define VMODELINFO_SERVER_INTERFACE_VERSION	"VModelInfoServer002"

// MODEL INDEX RULES
// If index >= 0, then index references the precached model string table
// If index == -1, then the model is invalid
// If index < -1, then the model is DYNAMIC and has a DYNAMIC INDEX of (-2 - index)
// - if the dynamic index is ODD, then the model is CLIENT ONLY
//   and has a m_LocalDynamicModels lookup index of (dynamic index)>>1
// - if the dynamic index is EVEN, then the model is NETWORKED
//   and has a dynamic model string table index of (dynamic index)>>1

inline bool IsDynamicModelIndex( int modelindex ) { return modelindex < -1; }
inline bool IsClientOnlyModelIndex( int modelindex ) { return modelindex < -1 && (modelindex & 1); }

class IVModelInfo
{
public:
	virtual							~IVModelInfo( void ) { }

	virtual const model_t			*GetModel( int modelindex ) const = 0;
	// Returns index of model by name
	virtual int						GetModelIndex( const char *name ) const = 0;

	// Returns name of model
	virtual const char				*GetModelName( const model_t *model ) const = 0;
	virtual vcollide_t				*GetVCollide( const model_t *model ) const = 0;
	virtual vcollide_t				*GetVCollide( int modelindex ) const = 0;
	virtual void					GetModelBounds( const model_t *model, Vector& mins, Vector& maxs ) const = 0;
	virtual	void					GetModelRenderBounds( const model_t *model, Vector& mins, Vector& maxs ) const = 0;
	virtual int						GetModelFrameCount( const model_t *model ) const = 0;
	virtual int						GetModelType( const model_t *model ) const = 0;
	virtual void					*GetModelExtraData( const model_t *model ) = 0;
	virtual bool					ModelHasMaterialProxy( const model_t *model ) const = 0;
	virtual bool					IsTranslucent( model_t const* model ) const = 0;
	virtual bool					IsTranslucentTwoPass( const model_t *model ) const = 0;
	virtual void					Unused0() {};
	virtual RenderableTranslucencyType_t ComputeTranslucencyType( const model_t *model, int nSkin, int nBody ) = 0;
	virtual int						GetModelMaterialCount( const model_t* model ) const = 0;
	virtual int 					GetModelMaterials( const model_t *model, int count, IMaterial** ppMaterial ) = 0;
	virtual bool					IsModelVertexLit( const model_t *model ) const = 0;
	virtual const char				*GetModelKeyValueText( const model_t *model ) = 0;
	virtual bool					GetModelKeyValue( const model_t *model, CUtlBuffer &buf ) = 0; // supports keyvalue blocks in submodels
	virtual float					GetModelRadius( const model_t *model ) = 0;

	virtual const studiohdr_t		*FindModel( const studiohdr_t *pStudioHdr, void **cache, const char *modelname ) const = 0;
	virtual const studiohdr_t		*FindModel( void *cache ) const = 0;
	virtual	virtualmodel_t			*GetVirtualModel( const studiohdr_t *pStudioHdr ) const = 0;
	virtual byte					*GetAnimBlock( const studiohdr_t *pStudioHdr, int nBlock, bool bPreloadIfMissing ) const = 0;
	virtual bool					HasAnimBlockBeenPreloaded( const studiohdr_t *pStudioHdr, int nBlock ) const = 0;

	// Available on client only!!!
	virtual void					GetModelMaterialColorAndLighting( const model_t *model, Vector const& origin,
										QAngle const& angles, trace_t* pTrace, 
										Vector& lighting, Vector& matColor ) = 0;
	virtual void					GetIlluminationPoint( const model_t *model, IClientRenderable *pRenderable, Vector const& origin, 
										QAngle const& angles, Vector* pLightingCenter ) = 0;

	virtual int						GetModelContents( int modelIndex ) const = 0;
	virtual studiohdr_t				*GetStudiomodel( const model_t *mod ) = 0;
	virtual int						GetModelSpriteWidth( const model_t *model ) const = 0;
	virtual int						GetModelSpriteHeight( const model_t *model ) const = 0;

	// Sets/gets a map-specified fade range (client only)
	virtual void					SetLevelScreenFadeRange( float flMinSize, float flMaxSize ) = 0;
	virtual void					GetLevelScreenFadeRange( float *pMinArea, float *pMaxArea ) const = 0;

	// Sets/gets a map-specified per-view fade range (client only)
	virtual void					SetViewScreenFadeRange( float flMinSize, float flMaxSize ) = 0;

	// Computes fade alpha based on distance fade + screen fade (client only)
	virtual unsigned char			ComputeLevelScreenFade( const Vector &vecAbsOrigin, float flRadius, float flFadeScale ) const = 0;
	virtual unsigned char			ComputeViewScreenFade( const Vector &vecAbsOrigin, float flRadius, float flFadeScale ) const = 0;

	// both client and server
	virtual int						GetAutoplayList( const studiohdr_t *pStudioHdr, unsigned short **pAutoplayList ) const = 0;

	// Gets a virtual terrain collision model (creates if necessary)
	// NOTE: This may return NULL if the terrain model cannot be virtualized
	virtual CPhysCollide			*GetCollideForVirtualTerrain( int index ) = 0;

	virtual bool					IsUsingFBTexture( const model_t *model, int nSkin, int nBody, void /*IClientRenderable*/ *pClientRenderable ) const = 0;

	virtual const model_t			*FindOrLoadModel( const char *name ) const = 0;

	virtual MDLHandle_t				GetCacheHandle( const model_t *model ) const = 0;

	// Returns planes of non-nodraw brush model surfaces
	virtual int						GetBrushModelPlaneCount( const model_t *model ) const = 0;
	virtual void					GetBrushModelPlane( const model_t *model, int nIndex, cplane_t &plane, Vector *pOrigin ) const = 0;
	virtual int						GetSurfacepropsForVirtualTerrain( int index ) = 0;
	virtual bool					UsesEnvCubemap( const model_t *model ) const = 0;
	virtual bool					UsesStaticLighting( const model_t *model ) const = 0;

	// Returns index of model by name, dynamically registered if not already known.
	virtual int						RegisterDynamicModel( const char *name, bool bClientSide ) = 0;
	virtual int						RegisterCombinedDynamicModel( const char *pszName, MDLHandle_t Handle ) = 0;
	virtual void					UpdateCombinedDynamicModel( int nModelIndex, MDLHandle_t Handle ) = 0;

	virtual int						BeginCombinedModel( const char *pszName, bool bReuseExisting ) = 0;
	virtual bool					SetCombineModels( int nModelIndex, const CUtlVector< SCombinerModelInput_t > &vecModelsToCombine ) = 0;
	virtual bool					FinishCombinedModel( int nModelIndex, CombinedModelLoadedCallback pFunc, void *pUserData = NULL ) = 0;
	virtual void					ReleaseCombinedModel( int nModelIndex ) = 0;

	virtual bool					IsDynamicModelLoading( int modelIndex ) = 0;
	virtual void					AddRefDynamicModel( int modelIndex ) = 0;
	virtual void					ReleaseDynamicModel( int modelIndex ) = 0;

	// Registers callback for when dynamic model has finished loading.
	virtual bool					RegisterModelLoadCallback( int modelindex, IModelLoadCallback* pCallback, bool bCallImmediatelyIfLoaded = true ) = 0;
	virtual void					UnregisterModelLoadCallback( int modelindex, IModelLoadCallback* pCallback ) = 0;

	// Poked by engine
	virtual void					OnLevelChange() = 0;

	virtual KeyValues				*GetModelKeyValues( const model_t *pModel ) = 0;

	virtual void					UpdateViewWeaponModelCache( const char **ppWeaponModels, int nWeaponModels ) = 0;
	virtual void					TouchWorldWeaponModelCache( const char **ppWeaponModels, int nWeaponModels ) = 0;
};


class IVModelInfoClient : public IVModelInfo
{
public:
	// Returns a model as a client-side index which is stable across server changes
	virtual int						GetModelClientSideIndex( const char *name ) const = 0;

	// Poked by engine
	virtual void					OnDynamicModelStringTableChanged( int nStringIndex, const char *pString, const void *pData ) = 0;

	// Reference and Unload
	// Don't assume Reference flags - doing this for now
	virtual model_t					*ReferenceModel( const char *name ) = 0;
	virtual void					UnreferenceModel( model_t *model ) = 0;
	virtual void					UnloadUnreferencedModels( void ) = 0;
};


struct virtualterrainparams_t
{
	// UNDONE: Add grouping here, specified in BSP file? (test grouping to see if this is necessary)
	int index;
};

#endif // IVMODELINFO_H
