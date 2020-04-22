//===== Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: particle system definitions
//
//===========================================================================//

#ifndef PARTICLES_H
#define PARTICLES_H
#ifdef _WIN32
#pragma once
#endif

#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "mathlib/ssemath.h"
#include "appframework/iappsystem.h"
#if 1
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#else
class IMaterial;
class IMatRenderContext;
#endif

#include "dmxloader/dmxelement.h"
#include "tier1/utlintrusivelist.h"
#include "vstdlib/random.h"
#include "tier1/utlobjectreference.h"
#include "tier1/UtlStringMap.h"
#include "tier1/utlmap.h"
#include "trace.h"
#include "tier1/utlsoacontainer.h"
#include "raytrace.h"
#include "materialsystem/imesh.h"
#if defined( CLIENT_DLL )
#include "c_pixel_visibility.h"
#endif

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct DmxElementUnpackStructure_t;
class CParticleSystemDefinition;
class CParticleCollection;
class CParticleOperatorInstance;
class CParticleSystemDictionary;
class CUtlBuffer;
class IParticleOperatorDefinition;
class CSheet;
class CMeshBuilder;
extern float s_pRandomFloats[];
				

//-----------------------------------------------------------------------------
// Random numbers
//-----------------------------------------------------------------------------
#define MAX_RANDOM_FLOATS 4096
#define RANDOM_FLOAT_MASK ( MAX_RANDOM_FLOATS - 1 )

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

// Get a list of the files inside the particle manifest file
void GetParticleManifest( CUtlVector<CUtlString>& list );
void GetParticleManifest( CUtlVector<CUtlString>& list, const char *pFile );

//-----------------------------------------------------------------------------
// Particle attributes
//-----------------------------------------------------------------------------
#define MAX_PARTICLE_ATTRIBUTES 24

#define DEFPARTICLE_ATTRIBUTE( name, bit, datatype )			\
	const int PARTICLE_ATTRIBUTE_##name##_MASK = (1 << bit);	\
	const int PARTICLE_ATTRIBUTE_##name = bit;					\
	const EAttributeDataType PARTICLE_ATTRIBUTE_##name##_DATATYPE = datatype;

// required
DEFPARTICLE_ATTRIBUTE( XYZ, 0, ATTRDATATYPE_4V );

// particle lifetime (duration) of particle as a float.
DEFPARTICLE_ATTRIBUTE( LIFE_DURATION, 1, ATTRDATATYPE_FLOAT );

// prev coordinates for verlet integration
DEFPARTICLE_ATTRIBUTE( PREV_XYZ, 2, ATTRDATATYPE_4V );

// radius of particle
DEFPARTICLE_ATTRIBUTE( RADIUS, 3, ATTRDATATYPE_FLOAT );

// rotation angle of particle
DEFPARTICLE_ATTRIBUTE( ROTATION, 4, ATTRDATATYPE_FLOAT );

// rotation speed of particle
DEFPARTICLE_ATTRIBUTE( ROTATION_SPEED, 5, ATTRDATATYPE_FLOAT );

// tint of particle
DEFPARTICLE_ATTRIBUTE( TINT_RGB, 6, ATTRDATATYPE_4V );

// alpha tint of particle
DEFPARTICLE_ATTRIBUTE( ALPHA, 7, ATTRDATATYPE_FLOAT );

// creation time stamp (relative to particle system creation)
DEFPARTICLE_ATTRIBUTE( CREATION_TIME, 8, ATTRDATATYPE_FLOAT );

// sequnece # (which animation sequence number this particle uses )
DEFPARTICLE_ATTRIBUTE( SEQUENCE_NUMBER, 9, ATTRDATATYPE_FLOAT );

// length of the trail 
DEFPARTICLE_ATTRIBUTE( TRAIL_LENGTH, 10, ATTRDATATYPE_FLOAT );

// unique particle identifier
DEFPARTICLE_ATTRIBUTE( PARTICLE_ID, 11, ATTRDATATYPE_INT );

// unique rotation around up vector
DEFPARTICLE_ATTRIBUTE( YAW, 12, ATTRDATATYPE_FLOAT );

// second sequnece # (which animation sequence number this particle uses )
DEFPARTICLE_ATTRIBUTE( SEQUENCE_NUMBER1, 13, ATTRDATATYPE_FLOAT );

// hit box index
DEFPARTICLE_ATTRIBUTE( HITBOX_INDEX, 14, ATTRDATATYPE_INT );

DEFPARTICLE_ATTRIBUTE( HITBOX_RELATIVE_XYZ, 15, ATTRDATATYPE_4V );

DEFPARTICLE_ATTRIBUTE( ALPHA2, 16, ATTRDATATYPE_FLOAT );

// particle trace caching fields
DEFPARTICLE_ATTRIBUTE( SCRATCH_VEC, 17, ATTRDATATYPE_4V );		//scratch field used for storing arbitraty vec data	
DEFPARTICLE_ATTRIBUTE( SCRATCH_FLOAT, 18, ATTRDATATYPE_4V );	//scratch field used for storing arbitraty float data		
DEFPARTICLE_ATTRIBUTE( UNUSED, 19, ATTRDATATYPE_FLOAT );	
DEFPARTICLE_ATTRIBUTE( PITCH, 20, ATTRDATATYPE_4V );	

DEFPARTICLE_ATTRIBUTE( NORMAL, 21, ATTRDATATYPE_4V );			// 0 0 0 if none

DEFPARTICLE_ATTRIBUTE( GLOW_RGB, 22, ATTRDATATYPE_4V );			// glow color
DEFPARTICLE_ATTRIBUTE( GLOW_ALPHA, 23, ATTRDATATYPE_FLOAT );	// glow alpha

#define MAX_PARTICLE_CONTROL_POINTS 64

#define ATTRIBUTES_WHICH_ARE_VEC3S_MASK ( PARTICLE_ATTRIBUTE_SCRATCH_VEC_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK | \
                                          PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_TINT_RGB_MASK | \
                                          PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK  | PARTICLE_ATTRIBUTE_NORMAL_MASK | \
	                                      PARTICLE_ATTRIBUTE_GLOW_RGB_MASK )

#define ATTRIBUTES_WHICH_ARE_0_TO_1 (PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_ALPHA2_MASK)
#define ATTRIBUTES_WHICH_ARE_ANGLES (PARTICLE_ATTRIBUTE_ROTATION_MASK | PARTICLE_ATTRIBUTE_YAW_MASK | PARTICLE_ATTRIBUTE_PITCH_MASK )
#define ATTRIBUTES_WHICH_ARE_INTS (PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK | PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK )

// Auto filters
#define ATTRIBUTES_WHICH_ARE_POSITION_AND_VELOCITY (PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK)
#define ATTRIBUTES_WHICH_ARE_LIFE_DURATION (PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK)
#define ATTRIBUTES_WHICH_ARE_ROTATION (PARTICLE_ATTRIBUTE_ROTATION_MASK | PARTICLE_ATTRIBUTE_YAW_MASK | PARTICLE_ATTRIBUTE_ROTATION_SPEED_MASK | PARTICLE_ATTRIBUTE_PITCH_MASK )
#define ATTRIBUTES_WHICH_ARE_SIZE (PARTICLE_ATTRIBUTE_RADIUS_MASK | PARTICLE_ATTRIBUTE_TRAIL_LENGTH_MASK)
#define ATTRIBUTES_WHICH_ARE_COLOR_AND_OPACITY (PARTICLE_ATTRIBUTE_TINT_RGB_MASK | PARTICLE_ATTRIBUTE_GLOW_RGB_MASK | PARTICLE_ATTRIBUTE_ALPHA_MASK | PARTICLE_ATTRIBUTE_ALPHA2_MASK | PARTICLE_ATTRIBUTE_GLOW_ALPHA_MASK )
#define ATTRIBUTES_WHICH_ARE_ANIMATION_SEQUENCE (PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK | PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1_MASK)
#define ATTRIBUTES_WHICH_ARE_HITBOX (PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK | PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK)
#define ATTRIBUTES_WHICH_ARE_NORMAL (PARTICLE_ATTRIBUTE_NORMAL_MASK)

#if defined( _GAMECONSOLE )
#define MAX_PARTICLES_IN_A_SYSTEM 2000
#else
#define MAX_PARTICLES_IN_A_SYSTEM 5000
#endif

// Set this to 1 or 0 to enable or disable particle profiling.
// Note that this profiling is expensive on Linux, and some anti-virus
// products can make this very expensive on Windows.
#define MEASURE_PARTICLE_PERF 0

#define MIN_PARTICLE_SPEED 0.001

#define MAX_PARTICLE_ORIENTATION_TYPES 3

//-----------------------------------------------------------------------------
// Particle function types
//-----------------------------------------------------------------------------
enum ParticleFunctionType_t
{
	FUNCTION_RENDERER = 0,
	FUNCTION_OPERATOR,
	FUNCTION_INITIALIZER,
	FUNCTION_EMITTER,
	FUNCTION_CHILDREN,	// NOTE: This one is a fake function type, only here to help eliminate a ton of duplicated code in the editor
    FUNCTION_FORCEGENERATOR,
    FUNCTION_CONSTRAINT,
	PARTICLE_FUNCTION_COUNT
};

//-----------------------------------------------------------------------------
// Particle filter types
// Used for classifying operators in the interface
// Can only have 32 altogether
//-----------------------------------------------------------------------------
enum ParticleFilterType_t
{
	FILTER_NOT_SPECIAL = 0,
	FILTER_POSITION_AND_VELOCITY,
	FILTER_LIFE_DURATION,
	FILTER_PARAMETER_REMAPPING,
	FILTER_ROTATION,
	FILTER_SIZE,
	FILTER_COLOR_AND_OPACITY,
	FILTER_ANIMATION_SEQUENCE,
	FILTER_HITBOX,
	FILTER_NORMAL,
	FILTER_CONTROL_POINTS,
	FILTER_COUNT
};

enum ParticleFilterMask_t
{
	FILTER_NOT_SPECIAL_MASK				= 0,
	FILTER_POSITION_AND_VELOCITY_MASK	= 1 << 1,
	FILTER_LIFE_DURATION_MASK			= 1 << 2,
	FILTER_PARAMETER_REMAPPING_MASK		= 1 << 3,
	FILTER_ROTATION_MASK				= 1 << 4,
	FILTER_SIZE_MASK					= 1 << 5,
	FILTER_COLOR_AND_OPACITY_MASK		= 1 << 6,
	FILTER_ANIMATION_SEQUENCE_MASK		= 1 << 7,
	FILTER_HITBOX_MASK					= 1 << 8,
	FILTER_NORMAL_MASK					= 1 << 9,
	FILTER_CONTROL_POINTS_MASK			= 1 << 10
};

struct CParticleVisibilityInputs
{
	float	m_flInputMin;
	float	m_flInputMax;
	float	m_flAlphaScaleMin;
	float	m_flAlphaScaleMax;
	float	m_flRadiusScaleMin;
	float	m_flRadiusScaleMax;
	float	m_flRadiusScaleFOVBase;
	float	m_flProxyRadius;
	float	m_flDistanceInputMin;
	float	m_flDistanceInputMax;
	float	m_flDotInputMin;
	float	m_flDotInputMax;
	int		m_nCPin;
};

struct ModelHitBoxInfo_t
{
	Vector m_vecBoxMins;
	Vector m_vecBoxMaxes;
	matrix3x4_t m_Transform;
};

class CModelHitBoxesInfo
{
public:
	float m_flLastUpdateTime;
	float m_flPrevLastUpdateTime;
	int m_nNumHitBoxes;
	int m_nNumPrevHitBoxes;
	ModelHitBoxInfo_t *m_pHitBoxes;
	ModelHitBoxInfo_t *m_pPrevBoxes;

	bool CurAndPrevValid( void ) const
	{
		return ( m_nNumHitBoxes && ( m_nNumPrevHitBoxes == m_nNumHitBoxes ) );
	}

	CModelHitBoxesInfo( void )
	{
		m_flLastUpdateTime = -1;
		m_nNumHitBoxes = 0;
		m_nNumPrevHitBoxes = 0;
		m_pHitBoxes = NULL;
		m_pPrevBoxes = NULL;

	}

	~CModelHitBoxesInfo( void )
	{
		if ( m_pHitBoxes )
			delete[] m_pHitBoxes;
		if ( m_pPrevBoxes )
			delete[] m_pPrevBoxes;
	}

};


//-----------------------------------------------------------------------------
// Particle kill list
//-----------------------------------------------------------------------------
#define KILL_LIST_INDEX_BITS 24
#define KILL_LIST_FLAGS_BITS ( 32 - KILL_LIST_INDEX_BITS )
#define KILL_LIST_INDEX_MASK ( ( 1 << KILL_LIST_INDEX_BITS ) - 1 )
#define KILL_LIST_FLAGS_MASK ( ( 1 << KILL_LIST_FLAGS_BITS ) - 1 )
struct KillListItem_t
{
	unsigned int nIndex : KILL_LIST_INDEX_BITS;
	unsigned int nFlags : KILL_LIST_FLAGS_BITS;
};
enum KillListFlags
{
	// TODO: use this in ApplyKillList (the idea: pass particles to a child system, but dont then kill them)
	KILL_LIST_FLAG_DONT_KILL = ( 1 << 0 )
};


//-----------------------------------------------------------------------------
// Interface to allow the particle system to call back into the client
//-----------------------------------------------------------------------------

#define PARTICLE_SYSTEM_QUERY_INTERFACE_VERSION "VParticleSystemQuery004"

class IParticleSystemQuery : public IAppSystem
{
public:
	virtual bool IsEditor( ) = 0;

	virtual void GetLightingAtPoint( const Vector& vecOrigin, Color &tint ) = 0;
	virtual void TraceLine( const Vector& vecAbsStart,
							const Vector& vecAbsEnd, unsigned int mask, 
							const class IHandleEntity *ignore,
							int collisionGroup,
							CBaseTrace *ptr ) = 0;

	virtual bool IsPointInSolid( const Vector& vecPos, const int nContentsMask ) = 0;

	// given a possible spawn point, tries to movie it to be on or in the source object. returns
	// true if it succeeded
	virtual bool MovePointInsideControllingObject( CParticleCollection *pParticles,
												   void *pObject,
												   Vector *pPnt )
	{
		return true;
	}

	virtual	bool IsPointInControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, Vector vecPos, bool bBBoxOnly = false )
	{
		return true;
	}


	virtual int GetRayTraceEnvironmentFromName( const char *pszRtEnvName )
	{
		return 0;											// == PRECIPITATION
	}

	virtual int GetCollisionGroupFromName( const char *pszCollisionGroupName )
	{
		return 0;											// == COLLISION_GROUP_NONE
	}
	
	virtual void GetRandomPointsOnControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsOut,
		float flBBoxScale,
		int nNumTrysToGetAPointInsideTheModel,
		Vector *pPntsOut,
		Vector vecDirectionBias,
		Vector *pHitBoxRelativeCoordOut = NULL,
		int *pHitBoxIndexOut = NULL, 
		int nDesiredHitbox = -1, 
		const char *pszHitboxSetName = NULL ) = 0;

	virtual void GetClosestControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsIn,
		float flBBoxScale,
		Vector *pPntsIn,
		Vector *pHitBoxRelativeCoordOut = NULL,
		int *pHitBoxIndexOut = NULL,
		int nDesiredHitbox = -1, 
		const char *pszHitboxSetName = NULL ) = 0;

	virtual int GetControllingObjectHitBoxInfo(
		CParticleCollection *pParticles,
		int nControlPointNumber,
		int nBufSize,										// # of output slots available
		ModelHitBoxInfo_t *pHitBoxOutputBuffer,
		const char *pszHitboxSetName )
	{
		// returns number of hit boxes output
		return 0;
	}

	virtual void GetControllingObjectOBBox( CParticleCollection *pParticles,
		int nControlPointNumber,
		Vector vecMin, Vector vecMax )
	{
		vecMin = vecMax = vec3_origin;
	}


	// Traces Four Rays against a defined RayTraceEnvironment
	virtual void TraceAgainstRayTraceEnv( 
		int envnumber, 
		const FourRays &rays, fltx4 TMin, fltx4 TMax,
		RayTracingResult *rslt_out, int32 skip_id ) const = 0;

	virtual Vector GetLocalPlayerPos( void )
	{
		return vec3_origin;
	}

	virtual void GetLocalPlayerEyeVectors( Vector *pForward, Vector *pRight = NULL, Vector *pUp = NULL )
	{
		*pForward = vec3_origin;
		*pRight = vec3_origin;
		*pUp = vec3_origin;
	}

	virtual Vector GetCurrentViewOrigin()
	{
		return vec3_origin;
	}

	virtual int GetActivityCount() = 0;

	virtual const char *GetActivityNameFromIndex( int nActivityIndex ) { return 0; }

	virtual int GetActivityNumber( void *pModel, const char *m_pszActivityName ) { return -1; }

	virtual float GetPixelVisibility( int *pQueryHandle, const Vector &vecOrigin, float flScale ) = 0;

	virtual void SetUpLightingEnvironment( const Vector& pos )
	{
	}

	virtual void PreSimulate( ) = 0;

	virtual void PostSimulate( ) = 0;

	virtual void DebugDrawLine(const Vector& origin, const Vector& dest, int r, int g, int b,bool noDepthTest, float duration) = 0;

	virtual void *GetModel( char const *pMdlName ) { return NULL; }

	virtual void DrawModel( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber, int nBodyPart, int nSubModel,
							int nSkin, int nAnimationSequence = 0, float flAnimationRate = 30.0f, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f ) = 0;

	virtual void BeginDrawModels( int nNumModels, Vector const &vecCenter, CParticleCollection *pParticles ) {}

	virtual void FinishDrawModels( CParticleCollection *pParticles ) {}

	virtual void UpdateProjectedTexture( const int nParticleID, IMaterial *pMaterial, Vector &vOrigin, float flRadius, float flRotation, float r, float g, float b, float a, void *&pUserVar ) = 0;
};


//-----------------------------------------------------------------------------
//
// Particle system manager. Using a class because tools need it that way
// so the SFM and PET tools can share managers despite being linked to 
// separate particle system .libs
//
//-----------------------------------------------------------------------------
typedef int ParticleSystemHandle_t;

class CParticleSystemMgr
{
public:
	// Constructor, destructor
	CParticleSystemMgr();
	~CParticleSystemMgr();

	// Initialize the particle system
	bool Init( IParticleSystemQuery *pQuery, bool bAllowPrecache );
	void Shutdown();

	// methods to add builtin operators. If you don't call these at startup, you won't be able to sim or draw. These are done separately from Init, so that
	// the server can omit the code needed for rendering/simulation, if desired.
	void AddBuiltinSimulationOperators( void );
	void AddBuiltinRenderingOperators( void );

	// Registration of known operators
	void AddParticleOperator( ParticleFunctionType_t nOpType, IParticleOperatorDefinition *pOpFactory );

	// Read a particle config file, add it to the list of particle configs
	bool ReadParticleConfigFile( const char *pFileName, bool bPrecache, bool bDecommitTempMemory = true );
	bool ReadParticleConfigFile( CUtlBuffer &buf, bool bPrecache, bool bDecommitTempMemory = true, const char *pFileName = NULL );
	void DecommitTempMemory();

	// For recording, write a specific particle system to a CUtlBuffer in DMX format
	bool WriteParticleConfigFile( const char *pParticleSystemName, CUtlBuffer &buf, bool bPreventNameBasedLookup = false );
	bool WriteParticleConfigFile( const DmObjectId_t& id, CUtlBuffer &buf, bool bPreventNameBasedLookup = false );

	// create a particle system by name. returns null if one of that name does not exist
	CParticleCollection *CreateParticleCollection( const char *pParticleSystemName, float flDelay = 0.0f, int nRandomSeed = 0 );
	CParticleCollection *CreateParticleCollection( ParticleSystemHandle_t particleSystemName, float flDelay = 0.0f, int nRandomSeed = 0 );

	// create a particle system given a particle system id
	CParticleCollection *CreateParticleCollection( const DmObjectId_t &id, float flDelay = 0.0f, int nRandomSeed = 0 );

	// Is a particular particle system defined?
	bool IsParticleSystemDefined( const char *pParticleSystemName );
	bool IsParticleSystemDefined( const DmObjectId_t &id );

	// Returns the index of the specified particle system. 
	ParticleSystemHandle_t GetParticleSystemIndex( const char *pParticleSystemName );
	ParticleSystemHandle_t FindOrAddParticleSystemIndex( const char *pParticleSystemName );

	// Returns the name of the specified particle system.
	const char *GetParticleSystemNameFromIndex( ParticleSystemHandle_t iIndex );

	// Return the number of particle systems in our dictionary
	int GetParticleSystemCount( void );

	// Get the label for a filter
	const char *GetFilterName( ParticleFilterType_t nFilterType ) const;

	// call to get available particle operator definitions
	// NOTE: FUNCTION_CHILDREN will return a faked one, for ease of writing the editor
	CUtlVector< IParticleOperatorDefinition *> &GetAvailableParticleOperatorList( ParticleFunctionType_t nWhichList );

	void GetParticleSystemsInFile( const char *pFileName, CUtlVector<CUtlString> *pOutSystemNameList );
	void GetParticleSystemsInBuffer( CUtlBuffer &buf, CUtlVector<CUtlString> *pOutSystemNameList );

	// Returns the unpack structure for a particle system definition
	const DmxElementUnpackStructure_t *GetParticleSystemDefinitionUnpackStructure();

	// Particle sheet management
	void ShouldLoadSheets( bool bLoadSheets );
	CSheet *FindOrLoadSheet( CParticleSystemDefinition *pDef, bool bTryReloading = false );
	void FlushAllSheets( void );

	// Render cache used to render opaque particle collections
	void ResetRenderCache( void );
	void AddToRenderCache( CParticleCollection *pParticles );
	void DrawRenderCache( IMatRenderContext *pRenderContext, bool bShadowDepth );

	IParticleSystemQuery *Query( void ) { return m_pQuery; }

	// return the particle field name
	const char* GetParticleFieldName( int nParticleField ) const;

	// WARNING: the pointer returned by this function may be invalidated 
	// *at any time* by the editor, so do not ever cache it.
	CParticleSystemDefinition* FindParticleSystem( const char *pName );
	CParticleSystemDefinition* FindParticleSystem( const DmObjectId_t& id );
	CParticleSystemDefinition* FindParticleSystem( ParticleSystemHandle_t hParticleSystem );
	CParticleSystemDefinition* FindPrecachedParticleSystem( int nPrecacheIndex );


	void CommitProfileInformation( bool bCommit );			// call after simulation, if you want
															// sim time recorded. if oyu pass
															// flase, info will be thrown away and
															// uncomitted time reset.  Having this
															// function lets you only record
															// profile data for slow frames if
															// desired.


	void DumpProfileInformation( void );					// write particle_profile.csv

	void DumpParticleList( const char *pNameSubstring );

	// Cache/uncache materials used by particle systems
	void PrecacheParticleSystem( int nStringNumber, const char *pName );
	void UncacheAllParticleSystems();


	// Sets the last simulation time, used for particle system sleeping logic
	void SetLastSimulationTime( float flTime );
	float GetLastSimulationTime() const;

	// Sets the last simulation duration ( the amount of time we spent simulating particle ) last frame
	// Used to fallback to cheaper particle systems under load
	void SetLastSimulationDuration( float flDuration ); 
	float GetLastSimulationDuration() const; 

	void SetFallbackParameters( float flBase, float flMultiplier, float flSimFallbackBaseMultiplier, float flSimThresholdMs );
	float GetFallbackBase() const;
	float GetFallbackMultiplier() const;
	float GetSimFallbackThresholdMs() const;
	float GetSimFallbackBaseMultiplier() const;

	void SetSystemLevel( int nCPULevel, int nGPULevel );
	int GetParticleCPULevel() const;
	int GetParticleGPULevel() const;

	void LevelShutdown( void );								// called at level unload time


	void FrameUpdate( void );								// call this once per frame on main thread

	// Particle attribute query funcs
	int GetParticleAttributeByName( const char *pAttribute ) const;		// SLOW! returns -1 on error
	const char *GetParticleAttributeName( int nAttribute ) const;		// returns 'unknown' on error
	EAttributeDataType GetParticleAttributeDataType( int nAttribute ) const;

private:
	struct RenderCache_t
	{
		IMaterial *m_pMaterial;
		CUtlVector< CParticleCollection * > m_ParticleCollections;
	};

	struct BatchStep_t
	{
		CParticleCollection *m_pParticles;
		CParticleOperatorInstance *m_pRenderer;
		void *m_pContext;
		int m_nFirstParticle;
		int m_nParticleCount;
		int m_nVertCount;
	};

	struct Batch_t
	{
		int m_nVertCount;
		int m_nIndexCount;
		CUtlVector< BatchStep_t > m_BatchStep; 
	};

	struct ParticleAttribute_t
	{
		EAttributeDataType nDataType;
		const char *pName;
	};

	// Unserialization-related methods
	bool ReadParticleDefinitions( CUtlBuffer &buf, const char *pFileName, bool bPrecache, bool bDecommitTempMemory );
	void AddParticleSystem( CDmxElement *pParticleSystem );

	// Serialization-related methods
	CDmxElement *CreateParticleDmxElement( const DmObjectId_t &id );
	CDmxElement *CreateParticleDmxElement( const char *pParticleSystemName );

	bool WriteParticleConfigFile( CDmxElement *pParticleSystem, CUtlBuffer &buf, bool bPreventNameBasedLookup );

	// Builds a list of batches to render
	void BuildBatchList( int iRenderCache, IMatRenderContext *pRenderContext, CUtlVector< Batch_t >& batches );

	// Known operators
	CUtlVector<IParticleOperatorDefinition *> m_ParticleOperators[PARTICLE_FUNCTION_COUNT];

	// Particle system dictionary
	CParticleSystemDictionary *m_pParticleSystemDictionary;
	
	// typedef CUtlMap< ITexture *, CSheet* > SheetsCache;
	typedef CUtlStringMap< CSheet* > SheetsCache_t;
	SheetsCache_t m_SheetList;

	// attaching and dtaching killlists. when simulating, a particle system gets a kill list. after
	// simulating, the memory for that will be used for the next particle system.  This matters for
	// threaded particles, because we don't want to share the same kill list between simultaneously
	// simulating particle systems.
	void AttachKillList( CParticleCollection *pParticles);
	void DetachKillList( CParticleCollection *pParticles);

	// Set up s_AttributeTable
	void InitAttributeTable( void );

	// For visualization (currently can only visualize one operator at a time)
	CParticleCollection *m_pVisualizedParticles;
	DmObjectId_t m_VisualizedOperatorId;
	IParticleSystemQuery *m_pQuery;
	CUtlVector< RenderCache_t > m_RenderCache;
	IMaterial *m_pShadowDepthMaterial;
	float m_flLastSimulationTime;
	float m_flLastSimulationDuration;

	CUtlVector< ParticleSystemHandle_t > m_PrecacheLookup;
	CUtlVector< ParticleSystemHandle_t > m_ClientPrecacheLookup;

	bool m_bDidInit;
	bool m_bUsingDefaultQuery;
	bool m_bShouldLoadSheets;
	bool m_bAllowPrecache;

	int m_nNumFramesMeasured;

	float m_flFallbackBase;
	float m_flFallbackMultiplier;
	float m_flSimFallbackBaseMultiplier;
	float m_flSimThresholdMs;

	int m_nCPULevel;
	int m_nGPULevel;

	static ParticleAttribute_t s_AttributeTable[MAX_PARTICLE_ATTRIBUTES];

	friend class CParticleSystemDefinition;
	friend class CParticleCollection;
};

extern CParticleSystemMgr *g_pParticleSystemMgr;


//-----------------------------------------------------------------------------
// A particle system can only have 1 operator using a particular ID
//-----------------------------------------------------------------------------
enum ParticleOperatorId_t
{
	// Generic IDs
	OPERATOR_GENERIC = -2,		// Can have as many of these as you want
	OPERATOR_SINGLETON = -1,	// Can only have 1 operator with the same name as this one

	// Renderer operator IDs

	// Operator IDs

	// Initializer operator IDs
	OPERATOR_PI_POSITION,		// Particle initializer: position (can only have 1 position setter)
	OPERATOR_PI_RADIUS,
	OPERATOR_PI_ALPHA,
	OPERATOR_PI_TINT_RGB,
	OPERATOR_PI_ROTATION,
	OPERATOR_PI_YAW,

	// Emitter IDs

	OPERATOR_ID_COUNT,
};


//-----------------------------------------------------------------------------
// Class factory for particle operators
//-----------------------------------------------------------------------------
class IParticleOperatorDefinition
{
public:
	virtual const char *GetName() const = 0;
	virtual CParticleOperatorInstance *CreateInstance( const DmObjectId_t &id ) const = 0;
//	virtual void DestroyInstance( CParticleOperatorInstance *pInstance ) const = 0;
	virtual const DmxElementUnpackStructure_t* GetUnpackStructure() const = 0;
	virtual ParticleOperatorId_t GetId() const = 0;
	virtual uint32 GetFilter() const = 0;
	virtual bool IsObsolete() const = 0;

#if MEASURE_PARTICLE_PERF
	// performance monitoring
	float m_flMaxExecutionTime;
	float m_flTotalExecutionTime;
	float m_flUncomittedTime;

	FORCEINLINE void RecordExecutionTime( float flETime )
	{
		m_flUncomittedTime += flETime;
		m_flMaxExecutionTime = MAX( m_flMaxExecutionTime, flETime );
	}

	FORCEINLINE float TotalRecordedExecutionTime( void ) const
	{
		return m_flTotalExecutionTime;
	}

	FORCEINLINE float MaximumRecordedExecutionTime( void ) const
	{
		return m_flMaxExecutionTime;
	}
#else
	FORCEINLINE void RecordExecutionTime( float flETime )
	{
	}
#endif
};


//-----------------------------------------------------------------------------
// Particle operators
//-----------------------------------------------------------------------------
class CParticleOperatorInstance
{
public:
	// custom allocators so we can be simd aligned
	void *operator new( size_t nSize );
	void* operator new( size_t size, int nBlockUse, const char *pFileName, int nLine );
	void operator delete( void *pData );
	void operator delete( void* p, int nBlockUse, const char *pFileName, int nLine );

	// unpack structure will be applied by creator. add extra initialization needed here
	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
	}

	virtual size_t GetRequiredContextBytes( ) const 
	{
		return 0;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
	}

	virtual uint32 GetWrittenAttributes( void ) const = 0;
	virtual uint32 GetReadAttributes( void ) const = 0;

	virtual uint64 GetReadControlPointMask() const
	{
		return 0;
	}

	virtual uint32 GetFilter( void ) const
	{
		uint32 filter = 0;
		uint32 wrAttrib = GetWrittenAttributes();

		if (wrAttrib & ATTRIBUTES_WHICH_ARE_POSITION_AND_VELOCITY)
		{
			filter = filter | FILTER_POSITION_AND_VELOCITY_MASK;
		}
		if (wrAttrib & ATTRIBUTES_WHICH_ARE_LIFE_DURATION)
		{
			filter = filter | FILTER_LIFE_DURATION_MASK;
		}
		if (wrAttrib & ATTRIBUTES_WHICH_ARE_ROTATION)
		{
			filter = filter | FILTER_ROTATION_MASK;
		}
		if (wrAttrib & ATTRIBUTES_WHICH_ARE_SIZE)
		{
			filter = filter | FILTER_SIZE_MASK;
		}
		if (wrAttrib & ATTRIBUTES_WHICH_ARE_COLOR_AND_OPACITY)
		{
			filter = filter | FILTER_COLOR_AND_OPACITY_MASK;
		}
		if (wrAttrib & ATTRIBUTES_WHICH_ARE_ANIMATION_SEQUENCE)
		{
			filter = filter | FILTER_ANIMATION_SEQUENCE_MASK;
		}
		if (wrAttrib & ATTRIBUTES_WHICH_ARE_HITBOX)
		{
			filter = filter | FILTER_HITBOX_MASK;
		}
		if (wrAttrib & ATTRIBUTES_WHICH_ARE_NORMAL)
		{
			filter = filter | FILTER_NORMAL_MASK;
		}

		return filter;
	}


	// these control points are NOT positions or matrices (ie don't try to transform them)
	virtual uint64 GetNonPositionalControlPointMask() const
	{
		return 0;
	}

	//  Used when an operator needs to read the attributes of a particle at spawn time
	virtual uint32 GetReadInitialAttributes( void ) const
	{
		return 0;
	}

	// a particle simulator does this
	virtual void Operate( CParticleCollection *pParticles, float flOpStrength, void *pContext ) const
	{
	}

	virtual void PostSimulate( CParticleCollection *pParticles, void *pContext ) const
	{
	}

	// a renderer overrides this
	virtual void Render( IMatRenderContext *pRenderContext, 
						 CParticleCollection *pParticles, const Vector4D &vecDiffuseModulation, void *pContext, int nViewRecursionDepth ) const
	{
	}

	virtual bool IsBatchable() const
	{
		return true;
	}

	virtual bool IsOrderImportant() const
	{
		return false;
	}

	virtual bool ShouldRun( bool bApplyingParentKillList ) const
	{
		return !bApplyingParentKillList;
	}

	virtual void RenderUnsorted( CParticleCollection *pParticles, void *pContext, IMatRenderContext *pRenderContext, CMeshBuilder &meshBuilder, int nVertexOffset, int nFirstParticle, int nParticleCount ) const
	{
	}

	// Returns the number of verts + indices to render
	virtual int GetParticlesToRender( CParticleCollection *pParticles, void *pContext, int nFirstParticle, int nRemainingVertices, int nRemainingIndices, int *pVertsUsed, int *pIndicesUsed ) const
	{
		*pVertsUsed = 0;
		*pIndicesUsed = 0;
		return 0;
	}


	// emitters over-ride this. Return a mask of what fields you initted
	virtual uint32 Emit( CParticleCollection *pParticles, float flOpCurStrength,
						 void *pContext ) const
	{
		return 0;
	}

	// emitters over-ride this. 
	virtual void StopEmission( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly = false ) const
	{
	}
	virtual void StartEmission( CParticleCollection *pParticles, void *pContext, bool bInfiniteOnly = false ) const
	{
	}
	virtual void Restart( CParticleCollection *pParticles, void *pContext ) {}

	// initters over-ride this
	virtual void InitParticleSystem( CParticleCollection *pParticles, void *pContext ) const
	{
	}


	// a force generator does this. It accumulates in the force array
	virtual void AddForces( FourVectors *AccumulatedForces, 
							CParticleCollection *pParticles,
							int nBlocks,
							float flCurStrength,
							void *pContext ) const
	{
	}


	// this is called for each constarint every frame. It can set up data like nearby world traces,
	// etc
	virtual void SetupConstraintPerFrameData( CParticleCollection *pParticles,
											  void *pContext ) const
	{
	}


	// a constraint overrides this. It shold return a true if it did anything
	virtual bool EnforceConstraint( int nStartBlock,
									int nNumBlocks,
									CParticleCollection *pParticles,
									void *pContext, 
									int nNumValidParticlesInLastChunk ) const
	{
		return false;
	}
	
	// should the constraint be run only once after all other constraints?
	virtual bool IsFinalConstaint( void ) const
	{
		return false;
	}

	// determines if a mask needs to be initialized multiple times. 
	virtual bool InitMultipleOverride()
	{
		return false;
	}


	// Indicates if this initializer is scrub-safe (initializers don't use random numbers, for example)
	virtual bool IsScrubSafe()
	{
		return false;
	}

	// particle-initters over-ride this
	virtual void InitNewParticlesScalar( CParticleCollection *pParticles, int nFirstParticle, int n_particles, int attribute_write_mask, void *pContext ) const
	{
	}

	// init new particles in blocks of 4. initters that have sse smarts should over ride this. the scalar particle initter will still be cllaed for head/tail.
	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, int start_block, int n_blocks, int attribute_write_mask, void *pContext ) const
	{
		// default behaviour is to call the scalar one 4x times
		InitNewParticlesScalar( pParticles, 4*start_block, 4*n_blocks, attribute_write_mask, pContext );
	}

	// splits particle initialization up into scalar and block sections, callingt he right code
	void InitNewParticles( CParticleCollection *pParticles, int nFirstParticle, int n_particles, int attribute_write_mask , void *pContext) const;


	// this function is queried to determine if a particle system is over and doen with. A particle
	// system is done with when it has noparticles and no operators intend to create any more
	virtual bool MayCreateMoreParticles( CParticleCollection const *pParticles, void *pContext ) const
	{
		return false;
	}

	// Returns the operator definition that spawned this operator
	const IParticleOperatorDefinition *GetDefinition()
	{
		return m_pDef;
	}

	virtual bool ShouldRunBeforeEmitters( void ) const
	{
		return false;
	}

	// Called when the SFM wants to skip forward in time
	virtual void SkipToTime( float flTime, CParticleCollection *pParticles, void *pContext ) const {}

	// Returns a unique ID for this definition
	const DmObjectId_t& GetId() { return m_Id; }

	// Used for editing + debugging to visualize the operator in 3D
	virtual void Render( CParticleCollection *pParticles ) const {}

	// Used as a debugging mechanism to prevent bogus calls to RandomInt or RandomFloat inside operators
	// Use CParticleCollection::RandomInt/RandomFloat instead
	int RandomInt( int nMin, int nMax )
	{
		// NOTE: Use CParticleCollection::RandomInt! 
		Assert(0);
		return 0;
	}

	float RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f )
	{
		// NOTE: Use CParticleCollection::RandomFloat! 
		Assert(0);
		return 0.0f;
	}

	float RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f )
	{
		// NOTE: Use CParticleCollection::RandomFloatExp! 
		Assert(0);
		return 0.0f;
	}

	float m_flOpStartFadeInTime;
	float m_flOpEndFadeInTime;
	float m_flOpStartFadeOutTime;
	float m_flOpEndFadeOutTime;
	float m_flOpFadeOscillatePeriod;

	float m_flOpTimeOffsetMin;
	float m_flOpTimeOffsetMax;
	int m_nOpTimeOffsetSeed;

	int m_nOpStrengthScaleSeed;
	float m_flOpStrengthMinScale;
	float m_flOpStrengthMaxScale;

    int m_nOpTimeScaleSeed;
    float m_flOpTimeScaleMin;
    float m_flOpTimeScaleMax;

	bool m_bStrengthFastPath;								// set for operators which just always have strengh = 0
	
	int m_nOpEndCapState;

	virtual void Precache( void )
	{
	}

	virtual void Uncache( void )
	{
	}



	virtual ~CParticleOperatorInstance( void )
	{
		// so that sheet references, etc can be cleaned up
	}

protected:
	// utility function for initting a scalar attribute to a random range in an sse fashion
	void InitScalarAttributeRandomRangeExpBlock( int nAttributeId, float fMinValue, float fMaxValue, float fExp,
		CParticleCollection *pParticles, int nStartBlock, int nBlockCount, bool bRandomlyInvert = false ) const;
	void AddScalarAttributeRandomRangeExpBlock( int nAttributeId, float fMinValue, float fMaxValue, float fExp,
		CParticleCollection *pParticles, int nStartBlock, int nBlockCount, bool bRandomlyInvert = false ) const;

	void InitScalarAttributeRandomRangeExpScalar( int nAttributeId, float fMinValue, float fMaxValue, float fExp,
												  CParticleCollection *pParticles, int nStartParticle, int nParticleCount ) const;

	void CheckForFastPath( void );							// call at operator init time

	// utility funcs to access CParticleCollection data:
	bool HasAttribute( CParticleCollection *pParticles, int nAttribute ) const;
	KillListItem_t *GetParentKillList( CParticleCollection *pParticles, int &nNumParticlesToKill ) const;

private:
	friend class CParticleCollection;
	friend class CParticleSystemDefinition;
	friend class CParticleSystemMgr;

	const IParticleOperatorDefinition *m_pDef;
	void SetDefinition( const IParticleOperatorDefinition * pDef, const DmObjectId_t &id )
	{
		m_pDef = pDef;
		CopyUniqueId( id, &m_Id );
	}

	DmObjectId_t m_Id;

	template <typename T> friend class CParticleOperatorDefinition;
};

class CParticleInitializerOperatorInstance : public CParticleOperatorInstance
{
public:

	virtual bool ShouldRun( bool bApplyingParentKillList ) const
	{
		return ( !bApplyingParentKillList ) || m_bRunForParentApplyKillList;
	}

	bool m_bRunForParentApplyKillList;
};

class CParticleRenderOperatorInstance : public CParticleOperatorInstance
{
public:

	CParticleVisibilityInputs VisibilityInputs;
};

//-----------------------------------------------------------------------------
// Helper macro for creating particle operator factories
//-----------------------------------------------------------------------------
template < class T >
class CParticleOperatorDefinition : public IParticleOperatorDefinition
{
public:
	CParticleOperatorDefinition( const char *pFactoryName, ParticleOperatorId_t id, bool bIsObsolete ) : m_pFactoryName( pFactoryName ), m_Id( id )
	{
#if MEASURE_PARTICLE_PERF
		m_flTotalExecutionTime = 0.0f;
		m_flMaxExecutionTime = 0.0f;
		m_flUncomittedTime = 0.0f;
#endif
		m_bIsObsolete = bIsObsolete;
	}

	virtual const char *GetName() const
	{
		return m_pFactoryName;
	}

	virtual ParticleOperatorId_t GetId() const
	{
		return m_Id;
	}

	virtual CParticleOperatorInstance *CreateInstance( const DmObjectId_t &id ) const
	{
		CParticleOperatorInstance *pOp = new T;
		pOp->SetDefinition( this, id );
		return pOp;
	}

	virtual const DmxElementUnpackStructure_t* GetUnpackStructure() const
	{
		return m_pUnpackParams;
	}

	// Editor won't display obsolete operators
	virtual bool IsObsolete() const 
	{ 
		return m_bIsObsolete; 
	}

	virtual uint32 GetFilter() const { T temp; return temp.GetFilter(); }

private:
	const char *m_pFactoryName;
	ParticleOperatorId_t m_Id;
	bool m_bIsObsolete;
	static DmxElementUnpackStructure_t *m_pUnpackParams;
};

#define DECLARE_PARTICLE_OPERATOR( _className )				\
	DECLARE_DMXELEMENT_UNPACK()								\
	friend class CParticleOperatorDefinition<_className >

#define DEFINE_PARTICLE_OPERATOR( _className, _operatorName, _id )	\
	static CParticleOperatorDefinition<_className> s_##_className##Factory( _operatorName, _id, false )

#define DEFINE_PARTICLE_OPERATOR_OBSOLETE( _className, _operatorName, _id )	\
	static CParticleOperatorDefinition<_className> s_##_className##Factory( _operatorName, _id, true )

#define BEGIN_PARTICLE_OPERATOR_UNPACK( _className )										\
	BEGIN_DMXELEMENT_UNPACK( _className )													\
	DMXELEMENT_UNPACK_FIELD( "operator start fadein","0", float, m_flOpStartFadeInTime )	\
	DMXELEMENT_UNPACK_FIELD( "operator end fadein","0", float, m_flOpEndFadeInTime )		\
	DMXELEMENT_UNPACK_FIELD( "operator start fadeout","0", float, m_flOpStartFadeOutTime )	\
	DMXELEMENT_UNPACK_FIELD( "operator end fadeout","0", float, m_flOpEndFadeOutTime ) \
    DMXELEMENT_UNPACK_FIELD( "operator fade oscillate","0", float, m_flOpFadeOscillatePeriod ) \
    DMXELEMENT_UNPACK_FIELD( "operator time offset seed","0", int, m_nOpTimeOffsetSeed ) \
    DMXELEMENT_UNPACK_FIELD( "operator time offset min","0", float, m_flOpTimeOffsetMin ) \
    DMXELEMENT_UNPACK_FIELD( "operator time offset max","0", float, m_flOpTimeOffsetMax ) \
    DMXELEMENT_UNPACK_FIELD( "operator time scale seed","0", int, m_nOpTimeScaleSeed ) \
    DMXELEMENT_UNPACK_FIELD( "operator time scale min","1", float, m_flOpTimeScaleMin ) \
    DMXELEMENT_UNPACK_FIELD( "operator time scale max","1", float, m_flOpTimeScaleMax ) \
    DMXELEMENT_UNPACK_FIELD( "operator time strength random scale max", "1", float, m_flOpStrengthMaxScale ) \
    DMXELEMENT_UNPACK_FIELD( "operator strength scale seed","0", int, m_nOpStrengthScaleSeed ) \
    DMXELEMENT_UNPACK_FIELD( "operator strength random scale min", "1", float, m_flOpStrengthMinScale ) \
    DMXELEMENT_UNPACK_FIELD( "operator strength random scale max", "1", float, m_flOpStrengthMaxScale ) \
    DMXELEMENT_UNPACK_FIELD( "operator end cap state", "-1", int, m_nOpEndCapState ) 
#define END_PARTICLE_OPERATOR_UNPACK( _className )		\
	END_DMXELEMENT_UNPACK_TEMPLATE( _className, CParticleOperatorDefinition<_className>::m_pUnpackParams )

#define BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( _className )									\
	BEGIN_PARTICLE_OPERATOR_UNPACK( _className )													\
    DMXELEMENT_UNPACK_FIELD( "run for killed parent particles", "1", bool, m_bRunForParentApplyKillList )

#define BEGIN_PARTICLE_RENDER_OPERATOR_UNPACK( _className )											\
	BEGIN_PARTICLE_OPERATOR_UNPACK( _className )													\
	DMXELEMENT_UNPACK_FIELD( "Visibility Proxy Input Control Point Number", "-1", int, VisibilityInputs.m_nCPin )	\
	DMXELEMENT_UNPACK_FIELD( "Visibility Proxy Radius", "1.0", float, VisibilityInputs.m_flProxyRadius )				\
	DMXELEMENT_UNPACK_FIELD( "Visibility input minimum","0", float, VisibilityInputs.m_flInputMin )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input maximum","1", float, VisibilityInputs.m_flInputMax )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input dot minimum","0", float, VisibilityInputs.m_flDotInputMin )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input dot maximum","0", float, VisibilityInputs.m_flDotInputMax )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input distance minimum","0", float, VisibilityInputs.m_flDistanceInputMin )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility input distance maximum","0", float, VisibilityInputs.m_flDistanceInputMax )					\
	DMXELEMENT_UNPACK_FIELD( "Visibility Alpha Scale minimum","0", float, VisibilityInputs.m_flAlphaScaleMin )		\
	DMXELEMENT_UNPACK_FIELD( "Visibility Alpha Scale maximum","1", float, VisibilityInputs.m_flAlphaScaleMax )		\
	DMXELEMENT_UNPACK_FIELD( "Visibility Radius Scale minimum","1", float, VisibilityInputs.m_flRadiusScaleMin )		\
	DMXELEMENT_UNPACK_FIELD( "Visibility Radius Scale maximum","1", float, VisibilityInputs.m_flRadiusScaleMax )		\
	DMXELEMENT_UNPACK_FIELD( "Visibility Radius FOV Scale base","0", float, VisibilityInputs.m_flRadiusScaleFOVBase )		
#define REGISTER_PARTICLE_OPERATOR( _type, _className )	\
	g_pParticleSystemMgr->AddParticleOperator( _type, &s_##_className##Factory )

// need to think about particle constraints in terms of segregating affected particles so as to
// run multi-pass constraints on only a subset


//-----------------------------------------------------------------------------
// flags for particle systems
//-----------------------------------------------------------------------------
enum
{
	PCFLAGS_FIRST_FRAME = 0x1,
	PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED = 0x2,
};



#define DEBUG_PARTICLE_SORT 0

//------------------------------------------------------------------------------
// particle render helpers
//------------------------------------------------------------------------------
struct CParticleVisibilityData
{
	float	m_flAlphaVisibility;
	float	m_flRadiusVisibility;
	bool	m_bUseVisibility;
};
// sorting functionality for rendering. Call GetRenderList( bool bSorted ) to get the list of
// particles to render (sorted or not, including children).
// **do not casually change this structure**. The sorting code treats it interchangably as an SOA
// and accesses it using sse. Any changes to this struct need the sort code updated.**
struct ParticleRenderData_t
{
	float m_flSortKey;					// what we sort by
	int   m_nIndex;						// index or fudged index (for child particles)
	float m_flRadius;					// effective radius, using visibility
#if PLAT_LITTLE_ENDIAN
	uint8 m_nAlpha;						// effective alpha, combining alpha and alpha2 and vis. 0 - 255
	uint8 m_nAlphaPad[3];				// this will be written to
#endif
#if PLAT_BIG_ENDIAN
	uint8 m_nAlphaPad[3];				// this will be written to
	uint8 m_nAlpha;						// effective alpha, combining alpha and alpha2 and vis. 0 - 255
#endif
};

struct ExtendedParticleRenderData_t : ParticleRenderData_t
{
	float m_flX;
	float m_flY;
	float m_flZ;
	float m_flPad;
};


typedef struct ALIGN16 _FourInts
{
	int32 m_nValue[4];
} ALIGN16_POST FourInts;


struct ParticleBaseRenderData_SIMD_View
{
	fltx4 m_fl4SortKey;
	FourVectors m_fl4XYZ;
	fltx4 m_fl4Alpha;
	fltx4 m_fl4Red;
	fltx4 m_fl4Green;
	fltx4 m_fl4Blue;
	fltx4 m_fl4Radius;
	fltx4 m_fl4AnimationTimeValue;
	fltx4 m_fl4SequenceID;									// 56 bytes per particle
};

struct ParticleFullRenderData_SIMD_View : public ParticleBaseRenderData_SIMD_View
{
	fltx4 m_fl4Rotation;
	fltx4 m_fl4Yaw;

	// no-op operation so templates can compile
	FORCEINLINE void SetARGB2( fltx4 const &fl4Red, fltx4 const &fl4Green, fltx4 const &fl4Blue, fltx4 const &fl4Alpha )
	{
	}

	FORCEINLINE void SetNormal( fltx4 const &fl4NormalX, fltx4 const &fl4NormalY, fltx4 const &fl4NormalZ )
	{
	}
};

struct ParticleRenderDataWithOutlineInformation_SIMD_View : public ParticleFullRenderData_SIMD_View
{
	FourVectors m_v4Color2;
	fltx4 m_fl4Alpha2;

	FORCEINLINE void SetARGB2( fltx4 const &fl4Red, fltx4 const &fl4Green, fltx4 const &fl4Blue, fltx4 const &fl4Alpha )
	{
		m_v4Color2.x = fl4Red;
		m_v4Color2.y = fl4Green;
		m_v4Color2.z = fl4Blue;
		m_fl4Alpha2 = fl4Alpha;
	}
};

struct ParticleRenderDataWithNormal_SIMD_View : public ParticleFullRenderData_SIMD_View
{
	FourVectors m_v4Normal;

	FORCEINLINE void SetNormal( fltx4 const &fl4NormalX, fltx4 const &fl4NormalY, fltx4 const &fl4NormalZ )
	{
		m_v4Normal.x = fl4NormalX;
		m_v4Normal.y = fl4NormalY;
		m_v4Normal.z = fl4NormalZ;
	}

};



// definitions for byte fields ( colors ). endian-ness matters here.
#if PLAT_LITTLE_ENDIAN
#define BYTE_FIELD(x)							\
    uint8 x;									\
    uint8 x##_Pad[3 + 3 * 4 ];
#endif

#if PLAT_BIG_ENDIAN
#define BYTE_FIELD(x)							\
    uint8 x##_Pad0[3];							\
	uint8 x;									\
    uint8 x##_Pad[3 * 4 ];
#endif

#define FLOAT_FIELD( x )						\
    float x;									\
    float x##_Pad[3];

struct ParticleBaseRenderData_Scalar_View
{
	int32 m_nSortKey;
	int32 m_nPad00[3];
	FLOAT_FIELD( m_flX );
	FLOAT_FIELD( m_flY );
	FLOAT_FIELD( m_flZ );

	BYTE_FIELD( m_nAlpha );
	BYTE_FIELD( m_nRed );
	BYTE_FIELD( m_nGreen );
	BYTE_FIELD( m_nBlue );

	FLOAT_FIELD( m_flRadius );
	FLOAT_FIELD( m_flAnimationTimeValue );
#if PLAT_LITTLE_ENDIAN
	uint8 m_nSequenceID;
	uint8 m_nSequenceID1;
	uint8 m_nPadSequence[2 + 3 * 4];
#endif
#if PLAT_BIG_ENDIAN
	uint8 m_nPadSequence[2];
	uint8 m_nSequenceID1;
	uint8 m_nSequenceID;
	uint8 m_nPadSequence1[ 3 * 4];
#endif
};
struct ParticleFullRenderData_Scalar_View : public ParticleBaseRenderData_Scalar_View
{
	FLOAT_FIELD( m_flRotation );
	FLOAT_FIELD( m_flYaw );
	
	float Red2( void ) const { return 1.0; }
	float Green2( void ) const { return 1.0; }
	float Blue2( void ) const { return 1.0; }
	float Alpha2( void ) const { return 1.0; }
	
	float NormalX( void ) const { return 1.0; }
	float NormalY( void ) const { return 1.0; }
	float NormalZ( void ) const { return 1.0; }
};

struct ParticleRenderDataWithOutlineInformation_Scalar_View : public ParticleFullRenderData_Scalar_View
{
	FLOAT_FIELD( m_flRed2 );
	FLOAT_FIELD( m_flGreen2 );
	FLOAT_FIELD( m_flBlue2 );
	FLOAT_FIELD( m_flAlpha2 );

	float Red2( void ) const { return m_flRed2; }
	float Green2( void ) const { return m_flGreen2; }
	float Blue2( void ) const { return m_flBlue2; }
	float Alpha2( void ) const { return m_flAlpha2; }
};

struct ParticleRenderDataWithNormal_Scalar_View : public ParticleFullRenderData_Scalar_View
{
	FLOAT_FIELD( m_flNormalX );
	FLOAT_FIELD( m_flNormalY );
	FLOAT_FIELD( m_flNormalZ );

	float NormalX( void ) const { return m_flNormalX; }
	float NormalY( void ) const { return m_flNormalY; }
	float NormalZ( void ) const { return m_flNormalZ; }

};



ParticleFullRenderData_Scalar_View **GetExtendedRenderList( CParticleCollection *pParticles,
															IMatRenderContext *pRenderContext,
															bool bSorted, int *pNparticles,
															CParticleVisibilityData *pVisibilityData);


ParticleRenderDataWithOutlineInformation_Scalar_View **GetExtendedRenderListWithPerParticleGlow(
	CParticleCollection *pParticles,
	IMatRenderContext *pRenderContext,
	bool bSorted, int *pNparticles,
	CParticleVisibilityData *pVisibilityData );


ParticleRenderDataWithNormal_Scalar_View **GetExtendedRenderListWithNormals(
	CParticleCollection *pParticles,
	IMatRenderContext *pRenderContext,
	bool bSorted, int *pNparticles,
	CParticleVisibilityData *pVisibilityData );

// returns # of particles
int GenerateExtendedSortedIndexList( Vector vecCamera, Vector *pCameraFwd, CParticleVisibilityData *pVisibilityData, 
									 CParticleCollection *pParticles, bool bSorted, void *pOutBuf, 
									 ParticleFullRenderData_Scalar_View **pParticlePtrs );


//------------------------------------------------------------------------------
// CParticleSnapshot wraps a CSOAContainer, so that a particle system
// can write to it or read from it (e.g. this can be attached to a control point).
//------------------------------------------------------------------------------
class CParticleSnapshot
{
	DECLARE_DMXELEMENT_UNPACK();

public:
	CParticleSnapshot()  { Purge(); }
	~CParticleSnapshot() { Purge(); }

	struct AttributeMap
	{
		AttributeMap( int nContainerAttribute, int nParticleAttribute ) : m_nContainerAttribute( nContainerAttribute ), m_nParticleAttribute( nParticleAttribute ) {}
		int m_nContainerAttribute, m_nParticleAttribute;
	};
	typedef CUtlVector< AttributeMap > AttributeMapVector;

	// Has the CParticleSnapshot been fully initialized?
	bool IsValid( void ) { return !!m_pContainer; }

	// Initialize from a .psf DMX file:
	bool Unserialize( const char *pFullPath );
	// Serialize to a .psf DMX file (NOTE: external containers will serialize out fine, but won't be external when read back in)
	bool Serialize( const char *pFullPath, bool bTextMode = true ); // TODO: once this stabilizes, switch to binary mode

	// Initialize by creating a new container, with a specified attribute mapping (will clear out old data if it exists)
	//  - each map entry specifies container attribute and associated particle attribute
	//  - the mapping should be one-to-one (container attributes are 'labeled' with particle attributes),
	//    so each particle attribute and container field should be used *AT MOST* once
	//  - NOTE: many-to-one and one-to-many mappings may be implemented by particle read/write operators
	bool Init( int nX, int nY, int nZ, const AttributeMapVector &attributeMaps );
	// Same as the other Init, except the attribute mapping are specified with varargs instead of a vector
	// (pairs of int params specify container attribute and associated particle attribute, terminated by -1)
	bool Init( int nX, int nY, int nZ, ... );

	// Initialize by wrapping a pre-existing container, with a specified attribute mapping
	//  - same conditions/parameters as above
	//  - existing container attributes are expected to match the particle attribute data types
	//  - if a new attribute is added to the external container, call InitExternal again to update the snapshot
	bool InitExternal( CSOAContainer *pContainer, const AttributeMapVector &attributeMaps );
	bool InitExternal( CSOAContainer *pContainer, ... );

	// Clear the snapshot (and container) back to its initial state
	void Purge( void );

	// This provides READ-ONLY access to the snapshot's container (all write accessors should have wrappers here, to ensure
	// that the container's set of attributes is not modified, which would invalidate the snapshot attribute mapping table)
	const CSOAContainer *GetContainer( void ) { return m_pContainer; }


	// Does the snapshot have data (of the appropriate type) for the given particle attribute?
	bool HasAttribute( int nParticleAttribute, EAttributeDataType nDataType ) const
	{
		Assert( ( nParticleAttribute >= 0 ) && ( nParticleAttribute < MAX_PARTICLE_ATTRIBUTES ) );
		int nContainerIndex = m_ParticleAttributeToContainerAttribute[ nParticleAttribute ];
		return ( ( nContainerIndex != -1 ) && ( m_pContainer->GetAttributeType( nContainerIndex ) == nDataType ) );
	}

	// ---------- Wrappers for CSOAContainer members ---------- 
	int NumCols( void ) { return m_pContainer->NumCols(); }
	int NumRows( void ) { return m_pContainer->NumRows(); }
	int NumSlices( void ) { return m_pContainer->NumSlices(); }
	// Read data from the container for the given particle attribute, at index (nIndex,0,0)
	template<class T> T *ElementPointer( int nParticleAttribute, int nX = 0, int nY = 0, int nZ = 0 ) const
	{
		Assert( ( nParticleAttribute >= 0 ) && ( nParticleAttribute < MAX_PARTICLE_ATTRIBUTES ) );
		Assert( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ] != -1 );
		return m_pContainer->ElementPointer<T>( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ], nX, nY, nZ );
	}
	FourVectors *ElementPointer4V( int nParticleAttribute, int nX = 0, int nY = 0, int nZ = 0 ) const
	{
		Assert( ( nParticleAttribute >= 0 ) && ( nParticleAttribute < MAX_PARTICLE_ATTRIBUTES ) );
		Assert( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ] != -1 );
		return m_pContainer->ElementPointer4V( m_ParticleAttributeToContainerAttribute[ nParticleAttribute ], nX, nY, nZ );
	}
	// ---------- Wrappers for CSOAContainer members ---------- 


private:

	enum ParticleSnapshotDmxVersion_t { PARTICLE_SNAPSHOT_DMX_VERSION = 1 };

	// Whether we're using an external container (as opposed to our embedded one)
	bool UsingExternalContainer( void ) { return ( m_pContainer && ( m_pContainer != &m_Container ) ); }

	// Utility function used by the Init methods to add+validate an attribute mapping pair:
	bool AddAttributeMapping(      int nFieldNumber, int nParticleAttribute, const char *pFunc );
	// Utility function used by the Init methods to validate data types for an attribute mapping pair:
	bool ValidateAttributeMapping( int nFieldNumber, int nParticleAttribute, const char *pFunc );

	// Utility function to validate the embedded container after it is unserialized
	bool EmbeddedContainerIsValid( void );

	// Check whether the particle system's defined attributes have been changed (this won't compile if they have), so we can update the serialization code if need be
	void CheckParticleAttributesForChanges( void );


	CSOAContainer	m_Container;	// Embedded container
	CSOAContainer *	m_pContainer;	// Pointer either to the embedded container or an external one

	// For each particle attribute, this contains the index of the corresponding container attribute (-1 means 'none')
	int m_ParticleAttributeToContainerAttribute[ MAX_PARTICLE_ATTRIBUTES ];
	int m_ContainerAttributeToParticleAttribute[ MAX_SOA_FIELDS ]; // Reverse mapping (used for error-checking)
};


//------------------------------------------------------------------------------
// structure describing the parameter block used by operators which use the path between two points to
// control particles.
//------------------------------------------------------------------------------
struct CPathParameters
{
	int m_nStartControlPointNumber;
	int m_nEndControlPointNumber;
	int m_nBulgeControl;
	float m_flBulge;
	float m_flMidPoint;

	void ClampControlPointIndices( void )
	{
		m_nStartControlPointNumber = MAX(0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nStartControlPointNumber ) );
		m_nEndControlPointNumber = MAX(0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nEndControlPointNumber ) );
	}
};


struct CParticleControlPoint
{
	Vector m_Position;
	Vector m_PrevPosition;

	// orientation
	Vector m_ForwardVector;
	Vector m_UpVector;
	Vector m_RightVector;

	// reference to entity or whatever this control point comes from
	void *m_pObject;

	// parent for hierarchies
	int m_nParent;

	// CParticleSnapshot which particles can read data from or write data to:
	CParticleSnapshot *m_pSnapshot;
};

struct CParticleCPInfo
{
	CParticleControlPoint m_ControlPoint;
	CModelHitBoxesInfo m_CPHitBox;
};


// struct for simd xform to transform a point from an identitiy coordinate system to that of the control point
struct CParticleSIMDTransformation
{
	FourVectors m_v4Origin;
	FourVectors m_v4Fwd;
	FourVectors m_v4Up;
	FourVectors m_v4Right;


	FORCEINLINE void VectorRotate( FourVectors &InPnt )
	{
		fltx4 fl4OutX = SubSIMD( AddSIMD( MulSIMD( InPnt.x, m_v4Fwd.x ), MulSIMD( InPnt.z, m_v4Up.x ) ), MulSIMD( InPnt.y, m_v4Right.x ) );
		fltx4 fl4OutY = SubSIMD( AddSIMD( MulSIMD( InPnt.x, m_v4Fwd.y ), MulSIMD( InPnt.z, m_v4Up.y ) ), MulSIMD( InPnt.y, m_v4Right.y ) );
		InPnt.z = SubSIMD( AddSIMD( MulSIMD( InPnt.x, m_v4Fwd.z ), MulSIMD( InPnt.z, m_v4Up.z ) ), MulSIMD( InPnt.y, m_v4Right.z ) );
		InPnt.x = fl4OutX;
		InPnt.y = fl4OutY;
	}

	FORCEINLINE void VectorTransform( FourVectors &InPnt )
	{
		VectorRotate( InPnt );
		InPnt.x = AddSIMD( InPnt.x, m_v4Origin.x );
		InPnt.y = AddSIMD( InPnt.y, m_v4Origin.y );
		InPnt.z = AddSIMD( InPnt.z, m_v4Origin.z );
	}
};

#define NUM_COLLISION_CACHE_MODES 4

//-----------------------------------------------------------------------------
//
// CParticleCollection
//
//-----------------------------------------------------------------------------

enum EParticleRestartMode_t
{
	RESTART_NORMAL,											// just reset emitters
	RESTART_RESET_AND_MAKE_SURE_EMITS_HAPPEN,				// reset emitters. If another restart has already happened, emit particles right now to handle multiple resets per frame.

};

struct CParticleAttributeAddressTable
{
	float *m_pAttributes[MAX_PARTICLE_ATTRIBUTES];
	size_t m_nFloatStrides[MAX_PARTICLE_ATTRIBUTES];

	FORCEINLINE size_t Stride( int nAttr ) const
	{
		return m_nFloatStrides[nAttr];
	}

	FORCEINLINE float *Address( int nAttr ) const
	{
		return m_pAttributes[nAttr];
	}

	FORCEINLINE uint8 *ByteAddress( int nAttr ) const
	{
		return ( uint8 * ) m_pAttributes[nAttr];
	}

	FORCEINLINE float *FloatAttributePtr( int nAttribute, int nParticleNumber ) const
	{
		int block_ofs = nParticleNumber / 4;
		return m_pAttributes[ nAttribute ] + 
			m_nFloatStrides[ nAttribute ] * block_ofs +
			( nParticleNumber & 3 );
	}

	void CopyParticleAttributes( int nSrcIndex, int nDestIndex ) const;
};

//-----------------------------------------------------------------------------
// CCachedParticleBatches
//
// Caches up to MAX_CACHED_PARTICLE_BATCHES particle batches.  The 
// ICachedPerFrameMeshData comes from the dynamic mesh created when rendering
// the particles the first time.  This cache is only valid for a single frame.
//
// Only certain particle systems work with caching.  The particle sytem must
// not sort, must not alter vertices on the CPU based upon view data, and must
// not be rendering in mat_queue_mode 0.  SpriteTrail particle system that use
// the spritecard shader cache their particle batches for reuse throughout the
// frame.
//-----------------------------------------------------------------------------

#define MAX_CACHED_PARTICLE_BATCHES 8
class CCachedParticleBatches
{
public:
	uint32 m_nLastValidParticleCacheFrame;
	int m_nCachedRenderListCount;
	ICachedPerFrameMeshData *m_pCachedBatches[ MAX_CACHED_PARTICLE_BATCHES ];

	CCachedParticleBatches() : m_nLastValidParticleCacheFrame( (uint32)-1 ), m_nCachedRenderListCount( 0 )
	{
		Q_memset( m_pCachedBatches, 0, sizeof( ICachedPerFrameMeshData* ) * MAX_CACHED_PARTICLE_BATCHES );
	}
	~CCachedParticleBatches()
	{
		ClearBatches();
	}

	FORCEINLINE void ClearBatches()
	{
		m_nCachedRenderListCount = 0;
		for ( int i=0; i<MAX_CACHED_PARTICLE_BATCHES; ++i )
		{
			if ( m_pCachedBatches[ i ] ) 
				 m_pCachedBatches[ i ]->Free();
			m_pCachedBatches[ i ] = NULL;
		}
	}

	FORCEINLINE void SetCachedBatch( int nBatch, ICachedPerFrameMeshData* pBatch )
	{
		if ( nBatch >= MAX_CACHED_PARTICLE_BATCHES )
			return;

		m_pCachedBatches[ nBatch ] = pBatch;
	}

	FORCEINLINE ICachedPerFrameMeshData *GetCachedBatch( int nBatch )
	{
		if ( nBatch >= MAX_CACHED_PARTICLE_BATCHES )
			return NULL;
		
		return m_pCachedBatches[ nBatch ];
	}

	FORCEINLINE void SetCachedRenderListCount( int nParticleCount )
	{
		m_nCachedRenderListCount = nParticleCount;
	}

	FORCEINLINE int GetCachedRenderListCount()
	{
		return m_nCachedRenderListCount;
	}
};

class CParticleCollection
{
public:
	~CParticleCollection( void );

	// Restarts the particle collection, stopping all non-continuous emitters
	void Restart( EParticleRestartMode_t eMode = RESTART_NORMAL );

	// compute bounds from particle list
	void RecomputeBounds( void );


	void SetControlPoint( int nWhichPoint, const Vector &v );
	void SetControlPointObject( int nWhichPoint, void *pObject );

	void SetControlPointOrientation( int nWhichPoint, const Vector &forward,
									 const Vector &right, const Vector &up );
	void SetControlPointForwardVector( int nWhichPoint, const Vector &v );
	void SetControlPointUpVector( int nWhichPoint, const Vector &v );
	void SetControlPointRightVector( int nWhichPoint, const Vector &v );
	void SetControlPointParent( int nWhichPoint, int n );
	void SetControlPointSnapshot( int nWhichPoint, CParticleSnapshot *pSnapshot );

	void SetControlPointOrientation( int nWhichPoint, const Quaternion &q );

	// get the pointer to an attribute for a given particle.  
	// !!speed!! if you find yourself calling this anywhere that matters, 
	// you're not handling the simd-ness of the particle system well
	// and will have bad perf.
	const float *GetFloatAttributePtr( int nAttribute, int nParticleNumber ) const;
	const int *GetIntAttributePtr( int nAttribute, int nParticleNumber ) const;
	const fltx4 *GetM128AttributePtr( int nAttribute, size_t *pStrideOut ) const;
	const FourVectors *Get4VAttributePtr( int nAttribute, size_t *pStrideOut ) const;
	const FourInts *Get4IAttributePtr( int nAttribute, size_t *pStrideOut ) const;
	const int *GetIntAttributePtr( int nAttribute, size_t *pStrideOut ) const;

	Vector GetVectorAttributeValue( int nAttribute, int nParticleNumber ) const;
	float GetFloatAttributeValue( int nAttribute, int nParticleNumber ) const;

	int *GetIntAttributePtrForWrite( int nAttribute, int nParticleNumber );

	float *GetFloatAttributePtrForWrite( int nAttribute, int nParticleNumber );
	fltx4 *GetM128AttributePtrForWrite( int nAttribute, size_t *pStrideOut );
	FourVectors *Get4VAttributePtrForWrite( int nAttribute, size_t *pStrideOut );

	const float *GetInitialFloatAttributePtr( int nAttribute, int nParticleNumber ) const;
	const fltx4 *GetInitialM128AttributePtr( int nAttribute, size_t *pStrideOut ) const;
	const FourVectors *GetInitial4VAttributePtr( int nAttribute, size_t *pStrideOut ) const;
	float *GetInitialFloatAttributePtrForWrite( int nAttribute, int nParticleNumber );
	fltx4 *GetInitialM128AttributePtrForWrite( int nAttribute, size_t *pStrideOut );

	void Simulate( float dt );
	void SkipToTime( float t );

	// the camera objetc may be compared for equality against control point objects
	void Render( int nViewRecursionLevel, IMatRenderContext *pRenderContext, const Vector4D &vecDiffuseModulation, bool bTranslucentOnly = false, void *pCameraObject = NULL );

	bool IsValid( void ) const { return m_pDef != NULL; }

	// this system and all children are valid
	bool IsFullyValid( void ) const;

	const char *GetName() const;

	bool DependsOnSystem( const char *pName ) const;

	// IsFinished returns true when a system has no particles and won't be creating any more
	bool IsFinished( void ) const;

	// Used to make sure we're accessing valid memory
	bool IsValidAttributePtr( int nAttribute, const void *pPtr ) const;

	void SwapPosAndPrevPos( void );

	void SetNActiveParticles( int nCount );
	void KillParticle(int nPidx, unsigned int nFlags = 0);

	void StopEmission( bool bInfiniteOnly = false, bool bRemoveAllParticles = false, bool bWakeOnStop = false, bool bPlayEndCap = false );
	void StartEmission( bool bInfiniteOnly = false );
	void SetDormant( bool bDormant );
	bool IsEmitting() const;

	const Vector& GetControlPointAtCurrentTime( int nControlPoint ) const;
	void GetControlPointOrientationAtCurrentTime( int nControlPoint, Vector *pForward, Vector *pRight, Vector *pUp ) const;
	void GetControlPointTransformAtCurrentTime( int nControlPoint, matrix3x4_t *pMat );
	void GetControlPointTransformAtCurrentTime( int nControlPoint, VMatrix *pMat );
	int GetControlPointParent( int nControlPoint ) const;
	CParticleSnapshot *GetControlPointSnapshot( int nWhichPoint ) const;

	// Used to retrieve the position of a control point
	// somewhere between m_fCurTime and m_fCurTime - m_fPreviousDT
	void GetControlPointAtTime( int nControlPoint, float flTime, Vector *pControlPoint );
	void GetControlPointAtPrevTime( int nControlPoint, Vector *pControlPoint );
	void GetControlPointOrientationAtTime( int nControlPoint, float flTime, Vector *pForward, Vector *pRight, Vector *pUp );
	void GetControlPointTransformAtTime( int nControlPoint, float flTime, matrix3x4_t *pMat );
	void GetControlPointTransformAtTime( int nControlPoint, float flTime, VMatrix *pMat );
	void GetControlPointTransformAtTime( int nControlPoint, float flTime, CParticleSIMDTransformation *pXForm );
	int GetHighestControlPoint( void ) const;

	// Control point accessed:
	// NOTE: Unlike the definition's version of these methods,
	// these OR-in the masks of their children.
	bool ReadsControlPoint( int nPoint ) const;
	bool IsNonPositionalControlPoint( int nPoint ) const;

	// Used by particle systems to generate random numbers. Do not call these methods - use sse
	// code
	int RandomInt( int nMin, int nMax );
	float RandomFloat( float flMin, float flMax );
	float RandomFloatExp( float flMin, float flMax, float flExponent );
	void RandomVector( float flMin, float flMax, Vector *pVector );
	void RandomVector( const Vector &vecMin, const Vector &vecMax, Vector *pVector );
	float RandomVectorInUnitSphere( Vector *pVector );	// Returns the length sqr of the vector

	// NOTE: These versions will produce the *same random numbers* if you give it the same random
	// sample id. do not use these methods.
	int RandomInt( int nRandomSampleId, int nMin, int nMax );
	float RandomFloat( int nRandomSampleId, float flMin, float flMax );
	float RandomFloatExp( int nRandomSampleId, float flMin, float flMax, float flExponent );
	void RandomVector( int nRandomSampleId, float flMin, float flMax, Vector *pVector );
	void RandomVector( int nRandomSampleId, const Vector &vecMin, const Vector &vecMax, Vector *pVector );
	float RandomVectorInUnitSphere( int nRandomSampleId, Vector *pVector );	// Returns the length sqr of the vector

	fltx4 RandomFloat( const FourInts &ParticleID, int nRandomSampleOffset );


	// Random number offset (for use in getting Random #s in operators)
	int OperatorRandomSampleOffset() const;

	// Returns the render bounds
	void GetBounds( Vector *pMin, Vector *pMax );

	// Visualize operators (for editing/debugging)
	void VisualizeOperator( const DmObjectId_t *pOpId = NULL );

	// Does the particle system use the power of two frame buffer texture (refraction?)
	bool UsesPowerOfTwoFrameBufferTexture( bool bThisFrame ) const;

	// Does the particle system use the full frame buffer texture (soft particles)
	bool UsesFullFrameBufferTexture( bool bThisFrame ) const;

	// Is the particle system translucent?
	bool IsTranslucent() const;

	// Is the particle system two-pass?
	bool IsTwoPass() const;

	// Is the particle system batchable?
	bool IsBatchable() const;

	// Is the order of the particles important
	bool IsOrderImportant() const;

	// Should this system be run want to read its parent's kill list inside ApplyKillList?
	bool ShouldRunForParentApplyKillList( void ) const;

	// Renderer iteration
	int GetRendererCount() const;
	CParticleOperatorInstance *GetRenderer( int i );
	void *GetRendererContext( int i );

	bool CheckIfOperatorShouldRun( CParticleOperatorInstance const * op, float *pflCurStrength, bool bApplyingParentKillList = false );

	Vector TransformAxis( const Vector &SrcAxis, bool bLocalSpace, int nControlPointNumber = 0);

	// return backwards-sorted particle list. use --addressing
	const ParticleRenderData_t *GetRenderList( IMatRenderContext *pRenderContext, bool bSorted, int *pNparticles, CParticleVisibilityData *pVisibilityData );

	// calculate the points of a curve for a path
	void CalculatePathValues( CPathParameters const &PathIn,
							  float flTimeStamp,
							  Vector *pStartPnt,
							  Vector *pMidPnt,
							  Vector *pEndPnt
							  );

	int GetGroupID() const;

	void InitializeNewParticles( int nFirstParticle, int nParticleCount, uint32 nInittedMask, bool bApplyingParentKillList = false );
	
	// update hit boxes for control point if not updated yet for this sim step
	void UpdateHitBoxInfo( int nControlPointNumber, const char *pszHitboxSetName );

	// Used by particle system definitions to manage particle collection lists
	void UnlinkFromDefList( );

	FORCEINLINE uint8 const *GetPrevAttributeMemory( void ) const
	{
		return m_pPreviousAttributeMemory;
	}

	FORCEINLINE uint8 const *GetAttributeMemory( void ) const
	{
		return m_pParticleMemory;
	}
	
	FORCEINLINE bool IsUsingInterpolatedRendering( void ) const
	{
		return (
			( m_flTargetDrawTime < m_flCurTime ) &&
			( m_flTargetDrawTime >= m_flPrevSimTime ) &&
			( GetPrevAttributeMemory() ) &&
			( ! m_bFrozen ) );
	}

	void ResetParticleCache();
	CCachedParticleBatches *GetCachedParticleBatches();

	// render helpers
	int GenerateCulledSortedIndexList( ParticleRenderData_t *pOut, Vector vecCamera, Vector vecFwd, CParticleVisibilityData *pVisibilityData, bool bSorted );
	int GenerateSortedIndexList( ParticleRenderData_t *pOut, Vector vecCameraPos, CParticleVisibilityData *pVisibilityData, bool bSorted );

	CParticleCollection *GetNextCollectionUsingSameDef() { return m_pNextDef; }

	CUtlReference< CSheet > m_Sheet;
	bool m_bTriedLoadingSheet;



protected:
	CParticleCollection( );

	// Used by client code
	bool Init( const char *pParticleSystemName );
	bool Init( CParticleSystemDefinition *pDef );

	// Bloat the bounding box by bounds around the control point
	void BloatBoundsUsingControlPoint();

	// to run emitters on restart, out of main sim.
	void RunRestartedEmitters( void );

	void	SetRenderable( void *pRenderable );


private:



	void Init( CParticleSystemDefinition *pDef, float flDelay, int nRandomSeed );
	void InitStorage( CParticleSystemDefinition *pDef );
	void InitParticleCreationTime( int nFirstParticle, int nNumToInit );
	void CopyInitialAttributeValues( int nStartParticle, int nNumParticles );
	void ApplyKillList( void );
	void SetAttributeToConstant( int nAttribute, float fValue );
	void SetAttributeToConstant( int nAttribute, float fValueX, float fValueY, float fValueZ );
	void InitParticleAttributes( int nStartParticle, int nNumParticles, int nAttrsLeftToInit );

	// call emitter and initializer operators on the specified system
	// NOTE: this may be called from ApplyKillList, so the child can access about-to-be-killed particles
	static void EmitAndInit( CParticleCollection *pCollection, bool bApplyingParentKillList = false );

	// initialize this attribute for all active particles
	void FillAttributeWithConstant( int nAttribute, float fValue );

	// Updates the previous control points
	void UpdatePrevControlPoints( float dt );

	// Returns the memory for a particular constant attribute
	float *GetConstantAttributeMemory( int nAttribute );

	// Swaps two particles in the particle list
	void SwapAdjacentParticles( int hParticle );

	// Unlinks a particle from the list
	void UnlinkParticle( int hParticle );

	// Inserts a particle before another particle in the list
	void InsertParticleBefore( int hParticle, int hBefore );

	// Move a particle from one index to another
	void MoveParticle( int nInitialIndex, int nNewIndex );

	// Computes the sq distance to a particle position
	float ComputeSqrDistanceToParticle( int hParticle, const Vector &vecPosition ) const;

	// Grows the dist sq range for all particles
	void GrowDistSqrBounds( float flDistSqr );

	// Simulates the first frame
	void SimulateFirstFrame( );

	bool SystemContainsParticlesWithBoolSet( bool CParticleCollection::*pField ) const;
	// Does the particle collection contain opaque particle systems
	bool ContainsOpaqueCollections();
	bool ComputeUsesPowerOfTwoFrameBufferTexture();
	bool ComputeUsesFullFrameBufferTexture();
	bool ComputeIsTranslucent();
	bool ComputeIsTwoPass();
	bool ComputeIsBatchable();
	bool ComputeIsOrderImportant();
	bool ComputeRunForParentApplyKillList();

	void LabelTextureUsage( void );

	void LinkIntoDefList( );

	// Return the number of particle systems sharing the same definition
	int GetCurrentParticleDefCount( CParticleSystemDefinition* pDef );

	void CopyParticleAttributesToPreviousAttributes( void ) const;

public:
	fltx4 m_fl4CurTime;										// accumulated time

	int m_nPaddedActiveParticles;	// # of groups of 4 particles
	float m_flCurTime;				// accumulated time
	
	// support for simulating particles at < the frame rate and interpolating.
	// for a system simulating at lower frame rate, flDrawTime will be < m_flCurTime and >=m_flPrevSimTime
	float m_flPrevSimTime;									// the time of the previous sim
	float m_flTargetDrawTime;										// the timestamp for drawing

	int m_nActiveParticles;			// # of active particles
	float m_flDt;
	float m_flPreviousDt;
	float m_flNextSleepTime;								// time to go to sleep if not drawn


	CUtlReference< CParticleSystemDefinition > m_pDef;
	int m_nAllocatedParticles;
	int m_nMaxAllowedParticles;
	bool m_bDormant;
	bool m_bEmissionStopped;
	bool m_bPendingRestart;
	bool m_bQueuedStartEmission;
	bool m_bFrozen;
	bool m_bInEndCap;

	int m_LocalLightingCP;
	Color m_LocalLighting;

	// control point data.  Don't set these directly, or they won't propagate down to children
	// particle control points can act as emitter centers, repulsions points, etc.  what they are
	// used for depends on what operators and parameters your system has.
	int m_nNumControlPointsAllocated;
	CParticleCPInfo *m_pCPInfo;

	FORCEINLINE CParticleControlPoint &ControlPoint( int nIdx ) const;

	FORCEINLINE CModelHitBoxesInfo &ControlPointHitBox( int nIdx ) const;


	// public so people can call methods
	uint8 *m_pOperatorContextData;
	CParticleCollection *m_pNext;							// for linking children together
	CParticleCollection *m_pPrev;							// for linking children together

	struct CWorldCollideContextData *m_pCollisionCacheData[NUM_COLLISION_CACHE_MODES]; // children can share collision caches w/ parent

	CParticleCollection *m_pParent;
	
	CUtlIntrusiveDList<CParticleCollection>  m_Children;	// list for all child particle systems
	Vector m_Center;										// average of particle centers
	void *m_pRenderable;									// for use by client


	void *operator new(size_t nSize);
	void *operator new( size_t size, int nBlockUse, const char *pFileName, int nLine );
	void operator delete(void *pData);
	void operator delete( void* p, int nBlockUse, const char *pFileName, int nLine );


protected:
	// current bounds for the particle system
	bool m_bBoundsValid;
	Vector m_MinBounds;
	Vector m_MaxBounds;
	int m_nHighestCP;  //Highest CP set externally.  Needs to assert if a system calls to an unassigned CP.

private:


	int m_nAttributeMemorySize;
	unsigned char *m_pParticleMemory;						// fixed size at initialization. Must be aligned for SSE
	unsigned char *m_pParticleInitialMemory;				// fixed size at initialization. Must be aligned for SSE
	unsigned char *m_pConstantMemory;
	uint8 *m_pPreviousAttributeMemory;						// for simulating at less than the display rate


	int m_nPerParticleInitializedAttributeMask;
	int m_nPerParticleUpdatedAttributeMask;
	int m_nPerParticleReadInitialAttributeMask;				// What fields do operators want to see initial attribute values for?

	CParticleAttributeAddressTable m_ParticleAttributes;
	CParticleAttributeAddressTable m_ParticleInitialAttributes;
	CParticleAttributeAddressTable m_PreviousFrameAttributes;

	float *m_pConstantAttributes;

	uint64 m_nControlPointReadMask;							// Mask indicating which control points have been accessed
	uint64 m_nControlPointNonPositionalMask;				// Mask indicating which control points are non-positional (ie shouldn't be transformed)
	int m_nParticleFlags;									// PCFLAGS_xxx
	bool m_bIsScrubbable : 1;
	bool m_bIsRunningInitializers : 1;
	bool m_bIsRunningOperators : 1;
	bool m_bIsTranslucent : 1;
	bool m_bIsTwoPass : 1;
	bool m_bAnyUsesPowerOfTwoFrameBufferTexture : 1;		// whether or not we or any children use this
	bool m_bAnyUsesFullFrameBufferTexture : 1;
	bool m_bIsBatchable : 1;
	bool m_bIsOrderImportant : 1;							// is order important when deleting
	bool m_bRunForParentApplyKillList : 1;					// see ShouldRunForParentApplyKillList()

	bool m_bUsesPowerOfTwoFrameBufferTexture;				// whether or not we use this, _not_ our children
	bool m_bUsesFullFrameBufferTexture;
	
	// How many frames have we drawn?
	int m_nDrawnFrames;


	// Used to assign unique ids to each particle
	int m_nUniqueParticleId;

	// Used to generate random numbers
	int m_nRandomQueryCount;
	int m_nRandomSeed;
	int m_nOperatorRandomSampleOffset;

	float m_flMinDistSqr;
	float m_flMaxDistSqr;
	float m_flOOMaxDistSqr;
	Vector m_vecLastCameraPos;
	float m_flLastMinDistSqr;
	float m_flLastMaxDistSqr;

	// Particle collection kill list. set up by particle system mgr
	int m_nNumParticlesToKill;
	KillListItem_t *m_pParticleKillList;

	// Used to build a list of all particle collections that have the same particle def 
	CParticleCollection *m_pNextDef;
	CParticleCollection *m_pPrevDef;

	void LoanKillListTo( CParticleCollection *pBorrower ) const;
	bool HasAttachedKillList( void ) const;

	CCachedParticleBatches *m_pCachedParticleBatches;

	// For debugging
	CParticleOperatorInstance *m_pRenderOp;
	friend class CParticleSystemMgr;
	friend class CParticleOperatorInstance;
	friend class CParticleSystemDefinition;
	friend class C4VInterpolatedAttributeIterator;
	friend class CM128InterpolatedAttributeIterator;
};



class CM128InitialAttributeIterator : public CStridedConstPtr<fltx4>
{
public:
	FORCEINLINE CM128InitialAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetInitialM128AttributePtr( nAttribute, &m_nStride );
	}
};


class CM128AttributeIterator : public CStridedConstPtr<fltx4>
{
public:
	FORCEINLINE void Init( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetM128AttributePtr( nAttribute, &m_nStride );
	}
	FORCEINLINE CM128AttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}
	CM128AttributeIterator( void )
	{
	}

	FORCEINLINE fltx4 operator()( fltx4 fl4T ) const
	{
		return *( m_pData );
	}
};

class C4IAttributeIterator : public CStridedConstPtr<FourInts>
{
public:
	FORCEINLINE void Init( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->Get4IAttributePtr( nAttribute, &m_nStride );
	}
	FORCEINLINE C4IAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}
	FORCEINLINE C4IAttributeIterator( void )
	{
	}
};

class CM128AttributeWriteIterator : public CStridedPtr<fltx4>
{
public:
	FORCEINLINE CM128AttributeWriteIterator( void )
	{
	}
	FORCEINLINE void Init ( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetM128AttributePtrForWrite( nAttribute, &m_nStride );
	}
	FORCEINLINE CM128AttributeWriteIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}
};

class C4VAttributeIterator : public CStridedConstPtr<FourVectors>
{
public:
	FORCEINLINE void Init( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->Get4VAttributePtr( nAttribute, &m_nStride );
	}
	FORCEINLINE C4VAttributeIterator( void )
	{
	}

	FORCEINLINE C4VAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}

	FORCEINLINE fltx4 X( fltx4 T )
	{
		return m_pData->x;
	}

	FORCEINLINE fltx4 Y( fltx4 T )
	{
		return m_pData->y;
	}

	FORCEINLINE fltx4 Z( fltx4 T )
	{
		return m_pData->z;
	}

};

class CM128InterpolatedAttributeIterator : public CM128AttributeIterator
{
protected:
	intp m_nOldDataOffset;

	FORCEINLINE fltx4 const *PreviousData( void ) const
	{
		return ( fltx4 const * ) ( ( ( uint8 const * ) m_pData ) + m_nOldDataOffset );
	}

public:
	FORCEINLINE void Init( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetM128AttributePtr( nAttribute, &m_nStride );
		Assert( pParticles->GetPrevAttributeMemory() );
		if ( m_nStride )
		{
			m_nOldDataOffset = 
				pParticles->m_PreviousFrameAttributes.ByteAddress( nAttribute ) - pParticles->m_ParticleAttributes.ByteAddress( nAttribute );
		}
		else
		{
			m_nOldDataOffset = 0;
		}

	}
	FORCEINLINE CM128InterpolatedAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}

	CM128InterpolatedAttributeIterator( void )
	{
	}

	FORCEINLINE fltx4 operator()( fltx4 fl4T ) const
	{
		fltx4 fl4Ret = *( PreviousData() );
		return AddSIMD( fl4Ret, MulSIMD( fl4T, SubSIMD( *m_pData, fl4Ret ) ) );
	}
};

class C4VInterpolatedAttributeIterator : public C4VAttributeIterator
{
protected:
	ptrdiff_t m_nOldDataOffset;

	FORCEINLINE FourVectors const *PreviousData( void ) const
	{
		return ( FourVectors const * ) ( ( ( uint8 const * ) m_pData ) + m_nOldDataOffset );
	}

public:
	void Init( int nAttribute, CParticleCollection *pParticles );


	FORCEINLINE C4VInterpolatedAttributeIterator( void )
	{
	}

	FORCEINLINE C4VInterpolatedAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		Init( nAttribute, pParticles );
	}

	FORCEINLINE fltx4 X( fltx4 fl4T ) const
	{
		fltx4 fl4Ret = PreviousData()->x;
		return AddSIMD( fl4Ret, MulSIMD( fl4T, SubSIMD( m_pData->x, fl4Ret ) ) );
	}

	FORCEINLINE fltx4 Y( fltx4 fl4T ) const
	{
		fltx4 fl4Ret = PreviousData()->y;
		return AddSIMD( fl4Ret, MulSIMD( fl4T, SubSIMD( m_pData->y, fl4Ret ) ) );
	}

	FORCEINLINE fltx4 Z( fltx4 fl4T ) const
	{
		fltx4 fl4Ret = PreviousData()->z;
		return AddSIMD( fl4Ret, MulSIMD( fl4T, SubSIMD( m_pData->z, fl4Ret ) ) );
	}

};

class C4VInitialAttributeIterator : public CStridedConstPtr<FourVectors>
{
public:
	FORCEINLINE C4VInitialAttributeIterator( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->GetInitial4VAttributePtr( nAttribute, &m_nStride );
	}
};

class C4VAttributeWriteIterator : public CStridedPtr<FourVectors>
{
public:
	FORCEINLINE C4VAttributeWriteIterator( int nAttribute, CParticleCollection *pParticles )
	{
		m_pData = pParticles->Get4VAttributePtrForWrite( nAttribute, &m_nStride );
	}
};





//-----------------------------------------------------------------------------
// Inline methods of CParticleCollection
//-----------------------------------------------------------------------------

inline bool CParticleCollection::HasAttachedKillList( void ) const
{
	return m_pParticleKillList != NULL;
}

inline bool CParticleCollection::ReadsControlPoint( int nPoint ) const
{
	return ( m_nControlPointReadMask & ( 1ULL << nPoint ) ) != 0;
}

inline bool CParticleCollection::IsNonPositionalControlPoint( int nPoint ) const
{
	return ( m_nControlPointNonPositionalMask & ( 1ULL << nPoint ) ) != 0;
}

inline void CParticleCollection::SetNActiveParticles( int nCount )
{
	Assert( nCount >= 0 && nCount <= m_nMaxAllowedParticles );
	m_nActiveParticles = nCount;
	m_nPaddedActiveParticles = ( nCount+3 )/4;
}

inline void CParticleCollection::SwapPosAndPrevPos( void )
{
	// strides better be the same!
	Assert( m_ParticleAttributes.Stride( PARTICLE_ATTRIBUTE_XYZ ) == m_ParticleAttributes.Stride( PARTICLE_ATTRIBUTE_PREV_XYZ  ) );
	V_swap( m_ParticleAttributes.m_pAttributes[ PARTICLE_ATTRIBUTE_XYZ ], m_ParticleAttributes.m_pAttributes[ PARTICLE_ATTRIBUTE_PREV_XYZ ] );
}

FORCEINLINE CParticleControlPoint &CParticleCollection::ControlPoint( int nIdx ) const
{
	Assert( nIdx >= 0 && nIdx < m_nNumControlPointsAllocated );
	return m_pCPInfo[ MAX(0, MIN( nIdx, m_nNumControlPointsAllocated - 1 )) ].m_ControlPoint;
}

FORCEINLINE CModelHitBoxesInfo &CParticleCollection::ControlPointHitBox( int nIdx ) const
{
	return m_pCPInfo[ MAX(0, MIN( nIdx, m_nNumControlPointsAllocated - 1 )) ].m_CPHitBox;
}

inline void CParticleCollection::LoanKillListTo( CParticleCollection *pBorrower ) const
{
	Assert(! pBorrower->m_pParticleKillList );
	pBorrower->m_nNumParticlesToKill = 0;
	pBorrower->m_pParticleKillList = m_pParticleKillList;
}

inline void CParticleCollection::SetAttributeToConstant( int nAttribute, float fValue )
{
	float *fconst = m_pConstantAttributes + 4*3*nAttribute;
	fconst[0] = fconst[1] = fconst[2] = fconst[3] = fValue;
}

inline void CParticleCollection::SetAttributeToConstant( int nAttribute, float fValueX, float fValueY, float fValueZ )
{
	float *fconst = m_pConstantAttributes + 4*3*nAttribute;
	fconst[0] = fconst[1] = fconst[2] = fconst[3] = fValueX;
	fconst[4] = fconst[5] = fconst[6] = fconst[7] = fValueY;
	fconst[8] = fconst[9] = fconst[10] = fconst[11] = fValueZ;
}

inline void CParticleCollection::SetControlPoint( int nWhichPoint, const Vector &v )
{
	Assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated )
	{
		ControlPoint( nWhichPoint ).m_Position = v;
		m_nHighestCP = MAX( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.m_pHead; i; i=i->m_pNext )
	{
		i->SetControlPoint( nWhichPoint, v );
	}
}

inline void CParticleCollection::SetControlPointObject( int nWhichPoint, void *pObject )
{
	Assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated ) 
	{
		ControlPoint( nWhichPoint ).m_pObject = pObject;
		m_nHighestCP = MAX( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.m_pHead; i; i=i->m_pNext )
	{
		i->SetControlPointObject( nWhichPoint, pObject );
	}
}

inline void CParticleCollection::SetControlPointOrientation( int nWhichPoint, const Vector &forward,
															 const Vector &right, const Vector &up )
{
	Assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );

	// check perpendicular
	Assert( fabs( DotProduct( forward, up ) ) <= 0.1f );
	Assert( fabs( DotProduct( forward, right ) ) <= 0.1f );
	Assert( fabs( DotProduct( right, up ) ) <= 0.1f );

	if ( nWhichPoint < m_nNumControlPointsAllocated ) 
	{
		ControlPoint( nWhichPoint ).m_ForwardVector = forward;
		ControlPoint( nWhichPoint ).m_UpVector = up;
		ControlPoint( nWhichPoint ).m_RightVector = right;
		m_nHighestCP = MAX( m_nHighestCP, nWhichPoint );
	}
	// make sure all children are finished
	for( CParticleCollection *i = m_Children.m_pHead; i; i=i->m_pNext )
	{
		i->SetControlPointOrientation( nWhichPoint, forward, right, up );
	}
}

inline Vector CParticleCollection::TransformAxis( const Vector &SrcAxis, bool bLocalSpace,
												  int nControlPointNumber)
{
	if ( bLocalSpace )
	{
		return												// mxmul
			( SrcAxis.x * ControlPoint( nControlPointNumber ).m_RightVector )+
			( SrcAxis.y * ControlPoint( nControlPointNumber ).m_ForwardVector )+
			( SrcAxis.z * ControlPoint( nControlPointNumber ).m_UpVector );
	}
	else
		return SrcAxis;
}


inline void CParticleCollection::SetControlPointOrientation( int nWhichPoint, const Quaternion &q )
{
	matrix3x4_t mat;
	Vector vecForward, vecUp, vecRight;
	QuaternionMatrix( q, mat );
	MatrixVectors( mat, &vecForward, &vecRight, &vecUp );
	SetControlPointOrientation( nWhichPoint, vecForward, vecRight, vecUp );
}

inline void CParticleCollection::SetControlPointForwardVector( int nWhichPoint, const Vector &v )
{
	Assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated ) 
	{
		ControlPoint( nWhichPoint ).m_ForwardVector = v;
		m_nHighestCP = MAX( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.m_pHead; i; i=i->m_pNext )
	{
		i->SetControlPointForwardVector( nWhichPoint, v );
	}
}

inline void CParticleCollection::SetControlPointUpVector( int nWhichPoint, const Vector &v )
{
	Assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated ) 
	{
		ControlPoint( nWhichPoint ).m_UpVector = v;
		m_nHighestCP = MAX( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.m_pHead; i; i=i->m_pNext )
	{
		i->SetControlPointUpVector( nWhichPoint, v );
	}
}

inline void CParticleCollection::SetControlPointRightVector( int nWhichPoint, const Vector &v)
{
	Assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated )
	{
		ControlPoint( nWhichPoint ).m_RightVector = v;
		m_nHighestCP = MAX( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.m_pHead; i; i=i->m_pNext )
	{
		i->SetControlPointRightVector( nWhichPoint, v );
	}
}

inline void CParticleCollection::SetControlPointParent( int nWhichPoint, int n )
{
	Assert( ( nWhichPoint >= 0) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated )
	{
		ControlPoint( nWhichPoint ).m_nParent = n;
		m_nHighestCP = MAX( m_nHighestCP, nWhichPoint );
	}
	for( CParticleCollection *i = m_Children.m_pHead; i; i=i->m_pNext )
	{
		i->SetControlPointParent( nWhichPoint, n );
	}
}

inline void CParticleCollection::SetControlPointSnapshot( int nWhichPoint, CParticleSnapshot *pSnapshot )
{
	Assert( ( nWhichPoint >= 0 ) && ( nWhichPoint < MAX_PARTICLE_CONTROL_POINTS ) );
	if ( nWhichPoint < m_nNumControlPointsAllocated )
	{
		ControlPoint( nWhichPoint ).m_pSnapshot = pSnapshot;
	}
	for( CParticleCollection *i = m_Children.m_pHead; i; i=i->m_pNext )
	{
		i->SetControlPointSnapshot( nWhichPoint, pSnapshot );
	}
}


// Returns the memory for a particular constant attribute
inline float *CParticleCollection::GetConstantAttributeMemory( int nAttribute )
{
	return m_pConstantAttributes + 3 * 4 * nAttribute;
}

// Random number offset (for use in getting Random #s in operators)
inline int CParticleCollection::OperatorRandomSampleOffset() const
{
	return m_nOperatorRandomSampleOffset;
}

// Used by particle systems to generate random numbers
inline int CParticleCollection::RandomInt( int nRandomSampleId, int nMin, int nMax )
{
	// do not call
	float flRand = s_pRandomFloats[ ( m_nRandomSeed + nRandomSampleId ) & RANDOM_FLOAT_MASK ];
	flRand *= ( nMax + 1 - nMin );
	int nRand = (int)flRand + nMin;
	return nRand;
}

inline float CParticleCollection::RandomFloat( int nRandomSampleId, float flMin, float flMax )
{
	// do not call
	float flRand = s_pRandomFloats[ ( m_nRandomSeed + nRandomSampleId ) & RANDOM_FLOAT_MASK ];
	flRand *= ( flMax - flMin );
	flRand += flMin;
	return flRand;
}

inline fltx4 CParticleCollection::RandomFloat( const FourInts &ParticleID, int nRandomSampleOffset )
{
	fltx4 Retval;
	int nOfs=m_nRandomSeed+nRandomSampleOffset;
	SubFloat( Retval, 0 ) = s_pRandomFloats[ ( nOfs + ParticleID.m_nValue[0] ) & RANDOM_FLOAT_MASK ];
	SubFloat( Retval, 1 ) = s_pRandomFloats[ ( nOfs + ParticleID.m_nValue[1] ) & RANDOM_FLOAT_MASK ];
	SubFloat( Retval, 2 ) = s_pRandomFloats[ ( nOfs + ParticleID.m_nValue[2] ) & RANDOM_FLOAT_MASK ];
	SubFloat( Retval, 3 ) = s_pRandomFloats[ ( nOfs + ParticleID.m_nValue[3] ) & RANDOM_FLOAT_MASK ];
	return Retval;
}


inline float CParticleCollection::RandomFloatExp( int nRandomSampleId, float flMin, float flMax, float flExponent )
{
	// do not call
	float flRand = s_pRandomFloats[ ( m_nRandomSeed + nRandomSampleId ) & RANDOM_FLOAT_MASK ];
	flRand = powf( flRand, flExponent );
	flRand *= ( flMax - flMin );
	flRand += flMin;
	return flRand;
}

inline void CParticleCollection::RandomVector( int nRandomSampleId, float flMin, float flMax, Vector *pVector )
{
	// do not call
	float flDelta = flMax - flMin;
	int nBaseId = m_nRandomSeed + nRandomSampleId;

	pVector->x = s_pRandomFloats[ nBaseId & RANDOM_FLOAT_MASK ];
	pVector->x *= flDelta;
	pVector->x += flMin;

	pVector->y = s_pRandomFloats[ ( nBaseId + 1 ) & RANDOM_FLOAT_MASK ];
	pVector->y *= flDelta;
	pVector->y += flMin;

	pVector->z = s_pRandomFloats[ ( nBaseId + 2 ) & RANDOM_FLOAT_MASK ];
	pVector->z *= flDelta;
	pVector->z += flMin;
}

inline void CParticleCollection::RandomVector( int nRandomSampleId, const Vector &vecMin, const Vector &vecMax, Vector *pVector )
{
	// do not call
	int nBaseId = m_nRandomSeed + nRandomSampleId;
	pVector->x = RandomFloat( nBaseId,     vecMin.x, vecMax.x );
	pVector->y = RandomFloat( nBaseId + 1, vecMin.y, vecMax.y );
	pVector->z = RandomFloat( nBaseId + 2, vecMin.z, vecMax.z );
}

// Used by particle systems to generate random numbers
inline int CParticleCollection::RandomInt( int nMin, int nMax )
{
	// do not call
	return RandomInt( m_nRandomQueryCount++, nMin, nMax );
}

inline float CParticleCollection::RandomFloat( float flMin, float flMax )
{
	// do not call
	return RandomFloat( m_nRandomQueryCount++, flMin, flMax );
}

inline float CParticleCollection::RandomFloatExp( float flMin, float flMax, float flExponent )
{
	// do not call
	return RandomFloatExp( m_nRandomQueryCount++, flMin, flMax, flExponent );
}

inline void CParticleCollection::RandomVector( float flMin, float flMax, Vector *pVector )
{
	// do not call
	RandomVector( m_nRandomQueryCount, flMin, flMax, pVector );
	m_nRandomQueryCount +=3;
}

inline void CParticleCollection::RandomVector( const Vector &vecMin, const Vector &vecMax, Vector *pVector )
{
	// do not call
	RandomVector( m_nRandomQueryCount, vecMin, vecMax, pVector );
	m_nRandomQueryCount +=3;
}

inline float CParticleCollection::RandomVectorInUnitSphere( Vector *pVector )
{
	// do not call
	float flUnitSphere = RandomVectorInUnitSphere( m_nRandomQueryCount, pVector );
	m_nRandomQueryCount +=3;
	return flUnitSphere;
}


// get the pointer to an attribute for a given particle.  !!speed!! if you find yourself
// calling this anywhere that matters, you're not handling the simd-ness of the particle system
// well and will have bad perf.
inline const float *CParticleCollection::GetFloatAttributePtr( int nAttribute, int nParticleNumber ) const
{
	Assert( nParticleNumber < m_nAllocatedParticles );
	return m_ParticleAttributes.FloatAttributePtr( nAttribute, nParticleNumber );
}

inline int *CParticleCollection::GetIntAttributePtrForWrite( int nAttribute, int nParticleNumber )
{
	return reinterpret_cast< int* >( GetFloatAttributePtrForWrite( nAttribute, nParticleNumber ) );
}

inline const int *CParticleCollection::GetIntAttributePtr( int nAttribute, int nParticleNumber ) const
{
	return (int*)GetFloatAttributePtr( nAttribute, nParticleNumber );
}

inline const fltx4 *CParticleCollection::GetM128AttributePtr( int nAttribute, size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<fltx4 *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const FourInts *CParticleCollection::Get4IAttributePtr( int nAttribute, size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<FourInts *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const int32 *CParticleCollection::GetIntAttributePtr( int nAttribute, size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleAttributes.Stride( nAttribute );
	return reinterpret_cast<int32 *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const FourVectors *CParticleCollection::Get4VAttributePtr( int nAttribute, size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleAttributes.Stride( nAttribute ) / 12;
	return reinterpret_cast<const FourVectors *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline FourVectors *CParticleCollection::Get4VAttributePtrForWrite( int nAttribute, size_t *pStrideOut ) 
{
	*( pStrideOut ) = m_ParticleAttributes.Stride( nAttribute ) / 12;
	return reinterpret_cast<FourVectors *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const FourVectors *CParticleCollection::GetInitial4VAttributePtr( int nAttribute, size_t *pStrideOut ) const
{
	*(pStrideOut) = m_ParticleInitialAttributes.Stride( nAttribute )/12;
	return reinterpret_cast<FourVectors *>( m_ParticleInitialAttributes.Address( nAttribute ) );
}

inline Vector CParticleCollection::GetVectorAttributeValue( int nAttribute, int nParticleNumber ) const
{
	Assert( nParticleNumber < m_nAllocatedParticles );
	size_t nStride;
	float const *pData = ( float const * ) Get4VAttributePtr( nAttribute, &nStride );
	int nOfs = nParticleNumber / 4;
	int nRemainder = nParticleNumber & 3;
	pData += 12 * nOfs + nRemainder;
	Vector vecRet( pData[0], pData[4], pData[8] );
	return vecRet;
}

inline float CParticleCollection::GetFloatAttributeValue( int nAttribute, int nParticleNumber ) const
{
	Assert( nParticleNumber < m_nAllocatedParticles );
	float const *pData = GetFloatAttributePtr( nAttribute, nParticleNumber );
	return *pData;
}


inline float *CParticleCollection::GetFloatAttributePtrForWrite( int nAttribute, int nParticleNumber )
{
	// NOTE: If you hit this assertion, it means your particle operator isn't returning
	// the appropriate fields in the RequiredAttributesMask call
	Assert( !m_bIsRunningInitializers || ( m_nPerParticleInitializedAttributeMask & (1 << nAttribute) ) );
	Assert( !m_bIsRunningOperators || ( m_nPerParticleUpdatedAttributeMask & (1 << nAttribute) ) );

	Assert( m_ParticleAttributes.Stride( nAttribute ) != 0 );

	Assert( nParticleNumber < m_nAllocatedParticles );
	return m_ParticleAttributes.FloatAttributePtr( nAttribute, nParticleNumber );
}

inline fltx4 *CParticleCollection::GetM128AttributePtrForWrite( int nAttribute, size_t *pStrideOut )
{
	// NOTE: If you hit this assertion, it means your particle operator isn't returning
	// the appropriate fields in the RequiredAttributesMask call
	Assert( !m_bIsRunningInitializers || ( m_nPerParticleInitializedAttributeMask & (1 << nAttribute) ) );
	Assert( !m_bIsRunningOperators || ( m_nPerParticleUpdatedAttributeMask & (1 << nAttribute) ) );
	Assert( m_ParticleAttributes.Stride( nAttribute ) != 0 );

	*( pStrideOut ) = m_ParticleAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<fltx4 *>( m_ParticleAttributes.Address( nAttribute ) );
}

inline const float *CParticleCollection::GetInitialFloatAttributePtr( int nAttribute, int nParticleNumber ) const
{
	Assert( nParticleNumber < m_nAllocatedParticles );
	return m_ParticleInitialAttributes.FloatAttributePtr( nAttribute, nParticleNumber );
}

inline const fltx4 *CParticleCollection::GetInitialM128AttributePtr( int nAttribute, size_t *pStrideOut ) const
{
	*( pStrideOut ) = m_ParticleInitialAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<fltx4 *>( m_ParticleInitialAttributes.Address( nAttribute ) );
}

inline float *CParticleCollection::GetInitialFloatAttributePtrForWrite( int nAttribute, int nParticleNumber )
{
	Assert( nParticleNumber < m_nAllocatedParticles );
	Assert( m_nPerParticleReadInitialAttributeMask & ( 1 << nAttribute ) );
	return m_ParticleInitialAttributes.FloatAttributePtr( nAttribute, nParticleNumber );
}

inline fltx4 *CParticleCollection::GetInitialM128AttributePtrForWrite( int nAttribute, size_t *pStrideOut )
{
	Assert( m_nPerParticleReadInitialAttributeMask & ( 1 << nAttribute ) );
	*( pStrideOut ) = m_ParticleInitialAttributes.Stride( nAttribute ) / 4;
	return reinterpret_cast<fltx4 *>( m_ParticleInitialAttributes.Address( nAttribute ) );
}

// Used to make sure we're accessing valid memory
inline bool CParticleCollection::IsValidAttributePtr( int nAttribute, const void *pPtr ) const
{
	if ( pPtr < m_ParticleAttributes.Address( nAttribute ) )
		return false;

	size_t nArraySize = m_ParticleAttributes.Stride( nAttribute ) * m_nAllocatedParticles / 4;
	void *pMaxPtr = m_ParticleAttributes.Address( nAttribute ) + nArraySize;
	return ( pPtr <= pMaxPtr );
}


FORCEINLINE void CParticleCollection::KillParticle( int nPidx, unsigned int nKillFlags )
{
	// add a particle to the sorted kill list. entries must be added in sorted order.
	// within a particle operator, this is safe to call. Outside of one, you have to call
	// the ApplyKillList() method yourself. The storage for the kill list is global between
	// all particle systems, so you can't kill a particle in 2 different CParticleCollections
	// w/o calling ApplyKillList
	Assert( !m_nNumParticlesToKill || ( nPidx > (int)m_pParticleKillList[ m_nNumParticlesToKill - 1 ].nIndex ) );

	// note that it is permissible to kill particles with indices>the number of active
	// particles, in order to faciliate easy sse coding (that said, we only expect the
	// particle index to be at most more than 3 larger than the particle count)
	Assert( nPidx < m_nActiveParticles + 4 );

	COMPILE_TIME_ASSERT( ( sizeof( KillListItem_t ) == 4 ) && ( MAX_PARTICLES_IN_A_SYSTEM < ( 1 << KILL_LIST_INDEX_BITS ) ) );
	Assert( !( nPidx & ~KILL_LIST_INDEX_MASK ) && !( nKillFlags & ~KILL_LIST_FLAGS_MASK ) );
	KillListItem_t killItem = { nPidx, nKillFlags };

	Assert( m_nNumParticlesToKill < MAX_PARTICLES_IN_A_SYSTEM );
	m_pParticleKillList[ m_nNumParticlesToKill++ ] = killItem;
}

// initialize this attribute for all active particles
inline void CParticleCollection::FillAttributeWithConstant( int nAttribute, float fValue )
{
	size_t stride;
	fltx4 *pAttr = GetM128AttributePtrForWrite( nAttribute, &stride );
	fltx4 fill=ReplicateX4( fValue );
	for( int i = 0; i < m_nPaddedActiveParticles; i++ )
	{
		*(pAttr) = fill;
		pAttr += stride;
	}
}


//-----------------------------------------------------------------------------
// Helper to set vector attribute values
//-----------------------------------------------------------------------------
FORCEINLINE void SetVectorAttribute( float *pAttribute, float x, float y, float z )
{
	pAttribute[0] = x;
	pAttribute[4] = y;
	pAttribute[8] = z;
}

FORCEINLINE void SetVectorAttribute( float *pAttribute, const Vector &v )
{
	pAttribute[0] = v.x;
	pAttribute[4] = v.y;
	pAttribute[8] = v.z;
}

FORCEINLINE void SetVectorFromAttribute( Vector &v, const float *pAttribute )
{
	v.x = pAttribute[0];
	v.y = pAttribute[4];
	v.z = pAttribute[8];
}


//-----------------------------------------------------------------------------
// Computes the sq distance to a particle position
//-----------------------------------------------------------------------------
FORCEINLINE float CParticleCollection::ComputeSqrDistanceToParticle( int hParticle, const Vector &vecPosition ) const
{
	const float *xyz = GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, hParticle );
	Vector vecParticlePosition( xyz[0], xyz[4], xyz[8] );
	return vecParticlePosition.DistToSqr( vecPosition );
}


//-----------------------------------------------------------------------------
// Grows the dist sq range for all particles
//-----------------------------------------------------------------------------
FORCEINLINE void CParticleCollection::GrowDistSqrBounds( float flDistSqr )
{
	if ( m_flLastMinDistSqr > flDistSqr )
	{
		m_flLastMinDistSqr = flDistSqr;
	}
	else if ( m_flLastMaxDistSqr < flDistSqr )
	{
		m_flLastMaxDistSqr = flDistSqr;
	}
}




//-----------------------------------------------------------------------------
// Data associated with children particle systems
//-----------------------------------------------------------------------------
struct ParticleChildrenInfo_t
{
	DmObjectId_t m_Id;
	ParticleSystemHandle_t m_Name;
	bool m_bUseNameBasedLookup;
	float m_flDelay;		// How much to delay this system after the parent starts
	bool m_bEndCap;			// This child only plays when an effect is stopped with endcap effects.
};


//-----------------------------------------------------------------------------
// A template describing how a particle system will function
//-----------------------------------------------------------------------------
class CParticleSystemDefinition
{
	DECLARE_DMXELEMENT_UNPACK();
	DECLARE_REFERENCED_CLASS( CParticleSystemDefinition );

	
public:
	CParticleSystemDefinition( void );
	~CParticleSystemDefinition( void );

	// Serialization, unserialization
	void Read( CDmxElement *pElement );
	CDmxElement *Write();

	const char *MaterialName() const;
	IMaterial *GetMaterial() const;
	const char *GetName() const;
    const DmObjectId_t& GetId() const;

	// Does the particle system use the power of two frame buffer texture (refraction?)
	bool UsesPowerOfTwoFrameBufferTexture();

	// Does the particle system use the full frame buffer texture (soft particles)
	bool UsesFullFrameBufferTexture();

	// Should we always precache this?
	bool ShouldAlwaysPrecache() const;

	// Should we batch particle collections using this definition up?
	bool ShouldBatch() const;

	// Is the particle system rendered on the viewmodel?
	bool IsViewModelEffect() const;

	bool IsScreenSpaceEffect() const;

	void SetDrawThroughLeafSystem( bool bDraw ) { m_bDrawThroughLeafSystem = bDraw; }
	bool IsDrawnThroughLeafSystem( void ) const { return m_bDrawThroughLeafSystem; }

	// Used to iterate over all particle collections using the same def
	CParticleCollection *FirstCollection();

	// What's the effective cull size + fill cost?
	// Used for early retirement
	float GetCullRadius() const;
	float GetCullFillCost() const;
	int GetCullControlPoint() const;
	const char *GetCullReplacementDefinition() const;

	int GetMaxRecursionDepth() const;

	// Retirement
	bool HasRetirementBeenChecked( int nFrame ) const;
	void MarkRetirementCheck( int nFrame );

	bool HasFallback() const;
	CParticleSystemDefinition *GetFallbackReplacementDefinition() const;

	int GetMinCPULevel() const;
	int GetMinGPULevel() const;

	// Control point read
	void MarkReadsControlPoint( int nPoint );
	bool ReadsControlPoint( int nPoint ) const;
	bool IsNonPositionalControlPoint( int nPoint ) const;

	float GetMaxTailLength() const;
	void  SetMaxTailLength( float flMaxTailLength );
	// Sheet symbols (used to avoid string->symbol conversions when effects are created)
	void InvalidateSheetSymbol();
	void CacheSheetSymbol( CUtlSymbol sheetSymbol );
	bool IsSheetSymbolCached() const;
	CUtlSymbol GetSheetSymbol() const;


private:
	void Precache();
	void Uncache();
	bool IsPrecached() const;

	void UnlinkAllCollections();

	void SetupContextData( );
	void ParseChildren( CDmxElement *pElement );
	void ParseOperators( const char *pszName, ParticleFunctionType_t nFunctionType,
		CDmxElement *pElement, CUtlVector<CParticleOperatorInstance *> &out_list );
	void WriteChildren( CDmxElement *pElement );
	void WriteOperators( CDmxElement *pElement, const char *pOpKeyName,
		const CUtlVector<CParticleOperatorInstance *> &inList );
	CUtlVector<CParticleOperatorInstance *> *GetOperatorList( ParticleFunctionType_t type );
	CParticleOperatorInstance *FindOperatorById( ParticleFunctionType_t type, const DmObjectId_t &id );
	CParticleOperatorInstance *FindOperatorByName( const char *pOperatorName ); // SLOW!

private:
	int m_nInitialParticles;
	int m_nPerParticleUpdatedAttributeMask;
	int m_nPerParticleInitializedAttributeMask;
	int m_nInitialAttributeReadMask;
	int m_nAttributeReadMask;
	uint64 m_nControlPointReadMask;
	uint64 m_nControlPointNonPositionalMask;
	Vector m_BoundingBoxMin;
	Vector m_BoundingBoxMax;
	CUtlString m_MaterialName;
	CMaterialReference m_Material;
	Vector4D m_vecMaterialModulation;
	CParticleCollection *m_pFirstCollection;
	CUtlString m_CullReplacementName;
	float m_flCullRadius;
	float m_flCullFillCost;
	int m_nCullControlPoint;
	int m_nRetireCheckFrame;
	int m_nMaxRecursionDepth;
	float m_flMaxTailLength;

	// Fallbacks for exceeding maximum number of the same type of system at once.
	CUtlString m_FallbackReplacementName;
	int m_nFallbackMaxCount;
	int m_nFallbackCurrentCount;
	CUtlReference< CParticleSystemDefinition > m_pFallback;

	// Default attribute values
	Color m_ConstantColor;
	Vector m_ConstantNormal;
	float m_flConstantRadius;
	float m_flConstantRotation;
	float m_flConstantRotationSpeed;
	int m_nConstantSequenceNumber;
	int m_nConstantSequenceNumber1;
	int m_nGroupID;
	float m_flMaximumTimeStep;
	float m_flMaximumSimTime;					 // maximum time to sim before drawing first frame.
	float m_flMinimumSimTime; // minimum time to sim before drawing first frame - prevents all
							  // capped particles from drawing at 0 time.
	float m_flMinimumTimeStep;				  // for simulating at < frame rate

	int m_nMinimumFrames;					  // number of frames to apply max/min simulation times

	int m_nMinCPULevel;						// minimum CPU/GPU levels for a 
	int m_nMinGPULevel;						// particle system to be allowed to spawn



	// Is the particle system rendered on the viewmodel?
	bool m_bViewModelEffect;
	bool m_bScreenSpaceEffect;
	bool m_bDrawThroughLeafSystem;
	bool m_bSheetSymbolCached;

	CUtlSymbol m_SheetSymbol;

	size_t m_nContextDataSize;
	DmObjectId_t m_Id;

public:
	float m_flMaxDrawDistance;								// distance at which to not draw. 
	float m_flNoDrawTimeToGoToSleep;						// after not beeing seen for this long, the system will sleep

	int m_nMaxParticles;
	int m_nSkipRenderControlPoint;							// if the camera is attached to the
															// object associated with this control
															// point, don't render the system

	int m_nAllowRenderControlPoint;							// if the camera is attached to the
															// object associated with this control
															// point, render the system, otherwise, don't


	int m_nAggregationMinAvailableParticles;				// only aggregate if their are this many free particles
	float m_flAggregateRadius;								// aggregate particles if this system is within radius n
	float m_flStopSimulationAfterTime;						// stop ( freeze ) simulation after this time

	CUtlString m_Name;

	CUtlVector<CParticleOperatorInstance *> m_Operators;
	CUtlVector<CParticleOperatorInstance *> m_Renderers;
	CUtlVector<CParticleOperatorInstance *> m_Initializers;
	CUtlVector<CParticleOperatorInstance *> m_Emitters;
	CUtlVector<CParticleOperatorInstance *> m_ForceGenerators;
	CUtlVector<CParticleOperatorInstance *> m_Constraints;
	CUtlVector<ParticleChildrenInfo_t> m_Children;

	CUtlVector<size_t> m_nOperatorsCtxOffsets;
	CUtlVector<size_t> m_nRenderersCtxOffsets;
	CUtlVector<size_t> m_nInitializersCtxOffsets;
	CUtlVector<size_t> m_nEmittersCtxOffsets;
	CUtlVector<size_t> m_nForceGeneratorsCtxOffsets;
	CUtlVector<size_t> m_nConstraintsCtxOffsets;


#if MEASURE_PARTICLE_PERF
	float m_flTotalSimTime;
	float m_flUncomittedTotalSimTime;
	float m_flMaxMeasuredSimTime;
	float m_flTotalRenderTime;
	float m_flMaxMeasuredRenderTime;
	float m_flUncomittedTotalRenderTime;
#endif

	uint m_nPerParticleOutlineMaterialVarToken;

	CInterlockedInt m_nNumIntersectionTests;	 // the number of particle intersectio queries done
	CInterlockedInt m_nNumActualRayTraces;			 // the total number of ray intersections done.

	int m_nMaximumActiveParticles;
	bool m_bShouldSort;
	bool m_bShouldBatch;
	bool m_bIsPrecached : 1;
	bool m_bAlwaysPrecache : 1;

	friend class CParticleCollection;
	friend class CParticleSystemMgr;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CParticleSystemDefinition::CParticleSystemDefinition( void )
{
	m_vecMaterialModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f );
	m_SheetSymbol = UTL_INVAL_SYMBOL;
	m_bSheetSymbolCached = false;
	m_nControlPointReadMask = 0;
	m_nControlPointNonPositionalMask = 0;
	m_nInitialAttributeReadMask = 0;
	m_nPerParticleInitializedAttributeMask = 0;
	m_nPerParticleUpdatedAttributeMask = 0;
	m_nAttributeReadMask = 0;
	m_nNumIntersectionTests = 0;
	m_nNumActualRayTraces = 0;
#if MEASURE_PARTICLE_PERF
	m_flTotalSimTime = 0.0;
	m_flMaxMeasuredSimTime = 0.0;
	m_flMaxMeasuredRenderTime = 0.0f;
	m_flTotalRenderTime = 0.0f;
	m_flUncomittedTotalRenderTime = 0.0f;
	m_flUncomittedTotalSimTime = 0.0f;
#endif
	m_nMaximumActiveParticles = 0;
	m_bIsPrecached = false;
	m_bAlwaysPrecache = false;
	m_bShouldBatch = false;
	m_bShouldSort = true;
	m_pFirstCollection = NULL;
	m_flCullRadius = 0.0f;
	m_flCullFillCost = 1.0f;
	m_nRetireCheckFrame = 0;
	m_nMaxRecursionDepth = 8;	
	m_nFallbackCurrentCount = 0;
	m_bDrawThroughLeafSystem = true; 
	m_flMaxTailLength = 0.0f;
	m_flMinimumTimeStep = 0;
	m_nPerParticleOutlineMaterialVarToken = 0;
}

inline CParticleSystemDefinition::~CParticleSystemDefinition( void )
{
	UnlinkAllCollections();
	m_Operators.PurgeAndDeleteElements();
	m_Renderers.PurgeAndDeleteElements();
	m_Initializers.PurgeAndDeleteElements();
	m_Emitters.PurgeAndDeleteElements();
	m_ForceGenerators.PurgeAndDeleteElements();
	m_Constraints.PurgeAndDeleteElements();
}

// Used to iterate over all particle collections using the same def
inline CParticleCollection *CParticleSystemDefinition::FirstCollection()
{ 
	return m_pFirstCollection; 
}

inline float CParticleSystemDefinition::GetCullRadius() const 
{ 
	return m_flCullRadius;
}

inline float CParticleSystemDefinition::GetCullFillCost() const
{
	return m_flCullFillCost;
}

inline const char *CParticleSystemDefinition::GetCullReplacementDefinition() const
{
	return m_CullReplacementName;
}

inline int CParticleSystemDefinition::GetCullControlPoint() const
{
	return m_nCullControlPoint;
}

inline int CParticleSystemDefinition::GetMaxRecursionDepth() const 
{ 
	return m_nMaxRecursionDepth;
}

inline bool CParticleSystemDefinition::HasFallback() const
{
	return ( m_nFallbackMaxCount > 0 );
}

inline int CParticleSystemDefinition::GetMinCPULevel() const
{
	return m_nMinCPULevel;
}

inline int CParticleSystemDefinition::GetMinGPULevel() const
{
	return m_nMinGPULevel;
}

inline void CParticleSystemDefinition::MarkReadsControlPoint( int nPoint ) 
{ 
	m_nControlPointReadMask |= ( 1ULL << nPoint );
}

inline bool CParticleSystemDefinition::IsNonPositionalControlPoint( int nPoint ) const
{
	return ( m_nControlPointNonPositionalMask & ( 1ULL << nPoint ) ) != 0;
}

inline bool CParticleSystemDefinition::ReadsControlPoint( int nPoint ) const 
{ 
	return ( m_nControlPointReadMask & ( 1ULL << nPoint ) ) != 0;
}

// Retirement
inline bool CParticleSystemDefinition::HasRetirementBeenChecked( int nFrame ) const
{
	return m_nRetireCheckFrame == nFrame;
}

inline void CParticleSystemDefinition::MarkRetirementCheck( int nFrame )
{
	m_nRetireCheckFrame = nFrame;
}

inline bool CParticleSystemDefinition::ShouldBatch() const
{
	return m_bShouldBatch;
}

inline bool CParticleSystemDefinition::IsViewModelEffect() const
{
	return m_bViewModelEffect;
}

inline bool CParticleSystemDefinition::IsScreenSpaceEffect() const
{
	return m_bScreenSpaceEffect;
}


inline float CParticleSystemDefinition::GetMaxTailLength() const
{
	return m_flMaxTailLength;
}

inline void CParticleSystemDefinition::SetMaxTailLength( float flMaxTailLength )
{
	m_flMaxTailLength = flMaxTailLength;
}

inline const char *CParticleSystemDefinition::MaterialName() const
{
	return m_MaterialName;
}

inline const DmObjectId_t& CParticleSystemDefinition::GetId() const
{
	return m_Id;
}

inline int CParticleCollection::GetGroupID( void ) const
{
	return m_pDef->m_nGroupID;
}

FORCEINLINE const Vector& CParticleCollection::GetControlPointAtCurrentTime( int nControlPoint ) const
{
	Assert( !m_pDef || m_pDef->ReadsControlPoint( nControlPoint ) );
	return ControlPoint( nControlPoint ).m_Position;
}

FORCEINLINE void CParticleCollection::GetControlPointOrientationAtCurrentTime( int nControlPoint, Vector *pForward, Vector *pRight, Vector *pUp ) const
{
	Assert( nControlPoint <= GetHighestControlPoint() );
	Assert( !m_pDef || m_pDef->ReadsControlPoint( nControlPoint ) );

	// FIXME: Use quaternion lerp to get control point transform at time
	*pForward = ControlPoint( nControlPoint).m_ForwardVector;
	*pRight = ControlPoint( nControlPoint ).m_RightVector;
	*pUp = ControlPoint( nControlPoint ).m_UpVector;
}

FORCEINLINE int CParticleCollection::GetControlPointParent( int nControlPoint ) const
{
	Assert( nControlPoint <= GetHighestControlPoint() );
	Assert( !m_pDef || m_pDef->ReadsControlPoint( nControlPoint ) );
	return ControlPoint( nControlPoint ).m_nParent;
}

FORCEINLINE CParticleSnapshot *CParticleCollection::GetControlPointSnapshot( int nControlPoint ) const
{
	Assert( nControlPoint <= GetHighestControlPoint() );
	if ( nControlPoint == -1 )
		return NULL;
	return ControlPoint( nControlPoint ).m_pSnapshot;
}


#endif	// PARTICLES_H
