//===== Copyright � 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: particle system code
//
//===========================================================================//

#include "tier0/platform.h"
#include "particles/particles.h"
#include "bitmap/psheet.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"
#include "tier1/utlbuffer.h"
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"
#include "dmxloader/dmxloader.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imesh.h"
#include "tier0/vprof.h"
#include "tier1/keyvalues.h"
#include "tier1/lzmaDecoder.h"
#include "random_floats.h"
#include "vtf/vtf.h"
#include "studio.h"
#include "particles_internal.h"
#include "ivrenderview.h"
#include "materialsystem/imaterialsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"




// rename table from the great rename 
static char *s_RemapOperatorNameTable[]={
	"alpha_fade", "Alpha Fade and Decay",
	"alpha_fade_in_random", "Alpha Fade In Random",
	"alpha_fade_out_random", "Alpha Fade Out Random",
	"basic_movement", "Movement Basic",
	"color_fade", "Color Fade",
	"controlpoint_light", "Color Light From Control Point",
	"Dampen Movement Relative to Control Point", "Movement Dampen Relative to Control Point",
	"Distance Between Control Points Scale", "Remap Distance Between Two Control Points to Scalar",
	"Distance to Control Points Scale", "Remap Distance to Control Point to Scalar",
	"lifespan_decay", "Lifespan Decay",
	"lock to bone",	"Movement Lock to Bone",
	"postion_lock_to_controlpoint", "Movement Lock to Control Point",
	"maintain position along path", "Movement Maintain Position Along Path",
	"Match Particle Velocities", "Movement Match Particle Velocities",
	"Max Velocity", "Movement Max Velocity",
	"noise", "Noise Scalar",
	"vector noise", "Noise Vector",
	"oscillate_scalar", "Oscillate Scalar",
	"oscillate_vector", "Oscillate Vector",
	"Orient Rotation to 2D Direction", "Rotation Orient to 2D Direction",
	"radius_scale", "Radius Scale",
	"Random Cull", "Cull Random",
	"remap_scalar", "Remap Scalar",
	"rotation_movement", "Rotation Basic",
	"rotation_spin", "Rotation Spin Roll",
	"rotation_spin yaw", "Rotation Spin Yaw",
	"alpha_random", "Alpha Random",
	"color_random", "Color Random",
	"create from parent particles", "Position From Parent Particles",
	"Create In Hierarchy", "Position In CP Hierarchy",
	"random position along path", "Position Along Path Random",
	"random position on model", "Position on Model Random",
	"sequential position along path", "Position Along Path Sequential",
	"position_offset_random", "Position Modify Offset Random",
	"position_warp_random", "Position Modify Warp Random",
	"position_within_box", "Position Within Box Random",
	"position_within_sphere", "Position Within Sphere Random",
	"Inherit Velocity", "Velocity Inherit from Control Point",
	"Initial Repulsion Velocity", "Velocity Repulse from World",
	"Initial Velocity Noise", "Velocity Noise",
	"Initial Scalar Noise", "Remap Noise to Scalar",
	"Lifespan from distance to world", "Lifetime from Time to Impact",
	"Pre-Age Noise", "Lifetime Pre-Age Noise",
	"lifetime_random", "Lifetime Random",
	"radius_random", "Radius Random",
	"random yaw", "Rotation Yaw Random",
	"Randomly Flip Yaw", "Rotation Yaw Flip Random",
	"rotation_random", "Rotation Random",
	"rotation_speed_random", "Rotation Speed Random",
	"sequence_random", "Sequence Random",
	"second_sequence_random", "Sequence Two Random",
	"trail_length_random", "Trail Length Random",
	"velocity_random", "Velocity Random",
};

static char const *RemapOperatorName( char const *pOpName )
{
	for( int i = 0 ; i < ARRAYSIZE( s_RemapOperatorNameTable ) ; i += 2 )
	{
		if ( Q_stricmp( pOpName, s_RemapOperatorNameTable[i] ) == 0 )
		{
			return s_RemapOperatorNameTable[i + 1 ];
		}
	}
	return pOpName;
}


//-----------------------------------------------------------------------------
// Default implementation of particle system mgr
//-----------------------------------------------------------------------------
static CParticleSystemMgr s_ParticleSystemMgr;
CParticleSystemMgr *g_pParticleSystemMgr = &s_ParticleSystemMgr;
CParticleSystemMgr::ParticleAttribute_t CParticleSystemMgr::s_AttributeTable[ MAX_PARTICLE_ATTRIBUTES ];


int g_nParticle_Multiplier = 1;
	

//-----------------------------------------------------------------------------
// Particle dictionary
//-----------------------------------------------------------------------------
class CParticleSystemDictionary
{
public:
	~CParticleSystemDictionary();

	CParticleSystemDefinition* AddParticleSystem( CDmxElement *pParticleSystem );
	int Count() const;
	int NameCount() const;
	CParticleSystemDefinition* GetParticleSystem( int i );
	ParticleSystemHandle_t FindParticleSystemHandle( const char *pName );
	ParticleSystemHandle_t FindOrAddParticleSystemHandle( const char *pName );
	CParticleSystemDefinition* FindParticleSystem( ParticleSystemHandle_t h );
	CParticleSystemDefinition* FindParticleSystem( const char *pName );
	CParticleSystemDefinition* FindParticleSystem( const DmObjectId_t &id );
	
	CParticleSystemDefinition* operator[]( int idx )
	{
		return m_ParticleNameMap[ idx ];
	}

private:
	typedef CUtlStringMap< CParticleSystemDefinition * > ParticleNameMap_t;
	typedef CUtlVector< CParticleSystemDefinition* > ParticleIdMap_t;

	void DestroyExistingElement( CDmxElement *pElement );

	ParticleNameMap_t m_ParticleNameMap;
	ParticleIdMap_t m_ParticleIdMap;
};


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CParticleSystemDictionary::~CParticleSystemDictionary()
{
	int nCount = m_ParticleIdMap.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		delete m_ParticleIdMap[i];
	}
}


//-----------------------------------------------------------------------------
// Destroys an existing element, returns if this element should be added to the name list
//-----------------------------------------------------------------------------
void CParticleSystemDictionary::DestroyExistingElement( CDmxElement *pElement )
{
	const char *pParticleSystemName = pElement->GetName();
	bool bPreventNameBasedLookup = pElement->GetValue<bool>( "preventNameBasedLookup" );
	if ( !bPreventNameBasedLookup )
	{
		if ( m_ParticleNameMap.Defined( pParticleSystemName ) )
		{
			CParticleSystemDefinition *pDef = m_ParticleNameMap[ pParticleSystemName ];
			delete pDef;
			m_ParticleNameMap[ pParticleSystemName ] = NULL;
		}
		return;
	}
	
	// Use id based lookup instead
	int nCount = m_ParticleIdMap.Count();
	const DmObjectId_t& id = pElement->GetId();
	for ( int i = 0; i < nCount; ++i )
	{
		// Was already removed by the name lookup
		if ( !IsUniqueIdEqual( m_ParticleIdMap[i]->GetId(), id ) )
			continue;

		CParticleSystemDefinition *pDef = m_ParticleIdMap[ i ];
		m_ParticleIdMap.FastRemove( i );
		delete pDef;
		break;
	}
}


//-----------------------------------------------------------------------------
// Adds a destructor
//-----------------------------------------------------------------------------
CParticleSystemDefinition* CParticleSystemDictionary::AddParticleSystem( CDmxElement *pParticleSystem )
{
	if ( Q_stricmp( pParticleSystem->GetTypeString(), "DmeParticleSystemDefinition" ) )
		return NULL;

	DestroyExistingElement( pParticleSystem );

	CParticleSystemDefinition *pDef = new CParticleSystemDefinition;

	// Must add the def to the maps before Read() because Read() may create new child particle systems
	bool bPreventNameBasedLookup = pParticleSystem->GetValue<bool>( "preventNameBasedLookup" );
	if ( !bPreventNameBasedLookup )
	{
		m_ParticleNameMap[ pParticleSystem->GetName() ] = pDef;
	}
	else
	{
		m_ParticleIdMap.AddToTail( pDef );
	}

	pDef->Read( pParticleSystem );
	return pDef;
}

int CParticleSystemDictionary::NameCount() const
{
	return m_ParticleNameMap.GetNumStrings();
}

int CParticleSystemDictionary::Count() const
{
	return m_ParticleIdMap.Count();
}

CParticleSystemDefinition* CParticleSystemDictionary::GetParticleSystem( int i )
{
	return m_ParticleIdMap[i];
}

ParticleSystemHandle_t CParticleSystemDictionary::FindParticleSystemHandle( const char *pName )
{
	return m_ParticleNameMap.Find( pName );
}

ParticleSystemHandle_t CParticleSystemDictionary::FindOrAddParticleSystemHandle( const char *pName )
{
	int nCount = m_ParticleNameMap.GetNumStrings();
	ParticleSystemHandle_t hSystem = m_ParticleNameMap.AddString( pName );
	if ( hSystem >= nCount )
	{
		m_ParticleNameMap[ hSystem ] = NULL;
	}
	return hSystem;
}

CParticleSystemDefinition* CParticleSystemDictionary::FindParticleSystem( ParticleSystemHandle_t h )
{
	if ( h == UTL_INVAL_SYMBOL || h >= m_ParticleNameMap.GetNumStrings() )
		return NULL;
	return m_ParticleNameMap[ h ];
}

CParticleSystemDefinition* CParticleSystemDictionary::FindParticleSystem( const char *pName )
{
	if ( m_ParticleNameMap.Defined( pName ) )
		return m_ParticleNameMap[ pName ];
	return NULL;
}

CParticleSystemDefinition* CParticleSystemDictionary::FindParticleSystem( const DmObjectId_t &id )
{
	int nCount = m_ParticleIdMap.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsUniqueIdEqual( m_ParticleIdMap[i]->GetId(), id ) )
			return m_ParticleIdMap[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// For editing, create a faked particle operator definition for children
// The only thing used in here is GetUnpackStructure.
//-----------------------------------------------------------------------------
BEGIN_DMXELEMENT_UNPACK( ParticleChildrenInfo_t ) 
	DMXELEMENT_UNPACK_FIELD( "delay", "0.0", float, m_flDelay )
	DMXELEMENT_UNPACK_FIELD( "end cap effect", "0", bool, m_bEndCap )
END_DMXELEMENT_UNPACK( ParticleChildrenInfo_t, s_ChildrenInfoUnpack )

class CChildOperatorDefinition : public IParticleOperatorDefinition
{
public:
	virtual const char *GetName() const { Assert(0); return NULL; }
	virtual CParticleOperatorInstance *CreateInstance( const DmObjectId_t &id ) const { Assert(0); return NULL; }
	//	virtual void DestroyInstance( CParticleOperatorInstance *pInstance ) const { Assert(0); }
	virtual const DmxElementUnpackStructure_t* GetUnpackStructure() const
	{
		return s_ChildrenInfoUnpack;
	}
	virtual ParticleOperatorId_t GetId() const { return OPERATOR_GENERIC; }
	virtual uint32 GetFilter() const { return 0; }
	virtual bool IsObsolete() const { return false; }
};

static CChildOperatorDefinition s_ChildOperatorDefinition;


//-----------------------------------------------------------------------------
//
// CParticleSystemDefinition
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Unpack structure for CParticleSystemDefinition
//-----------------------------------------------------------------------------
BEGIN_DMXELEMENT_UNPACK( CParticleSystemDefinition ) 
	DMXELEMENT_UNPACK_FIELD( "max_particles", "1000", int, m_nMaxParticles )
	DMXELEMENT_UNPACK_FIELD( "initial_particles", "0", int, m_nInitialParticles )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING_USERDATA( "material", "vgui/white", m_MaterialName, "vmtPicker" )
	DMXELEMENT_UNPACK_FIELD( "bounding_box_min", "-10 -10 -10", Vector, m_BoundingBoxMin )
	DMXELEMENT_UNPACK_FIELD( "bounding_box_max", "10 10 10", Vector, m_BoundingBoxMax )
	DMXELEMENT_UNPACK_FIELD( "cull_radius", "0", float, m_flCullRadius )
	DMXELEMENT_UNPACK_FIELD( "cull_cost", "1", float, m_flCullFillCost )
	DMXELEMENT_UNPACK_FIELD( "cull_control_point", "0", int, m_nCullControlPoint )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "cull_replacement_definition", "", m_CullReplacementName )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "fallback replacement definition", "", m_FallbackReplacementName )
	DMXELEMENT_UNPACK_FIELD( "fallback max count", "-1", int, m_nFallbackMaxCount )
	DMXELEMENT_UNPACK_FIELD( "radius", "5", float, m_flConstantRadius )
	DMXELEMENT_UNPACK_FIELD( "color", "255 255 255 255", Color, m_ConstantColor )
	DMXELEMENT_UNPACK_FIELD( "rotation", "0", float, m_flConstantRotation )
	DMXELEMENT_UNPACK_FIELD( "rotation_speed", "0", float, m_flConstantRotationSpeed )
	DMXELEMENT_UNPACK_FIELD( "normal", "0 0 1", Vector, m_ConstantNormal )	
	DMXELEMENT_UNPACK_FIELD_USERDATA( "sequence_number", "0", int, m_nConstantSequenceNumber, "sheetsequencepicker" )
	DMXELEMENT_UNPACK_FIELD_USERDATA( "sequence_number 1", "0", int, m_nConstantSequenceNumber1, "sheetsequencepicker_second" )
	DMXELEMENT_UNPACK_FIELD( "group id", "0", int, m_nGroupID )
	DMXELEMENT_UNPACK_FIELD( "maximum time step", "0.1", float, m_flMaximumTimeStep )
	DMXELEMENT_UNPACK_FIELD( "maximum sim tick rate", "0.0", float, m_flMaximumSimTime )
	DMXELEMENT_UNPACK_FIELD( "minimum sim tick rate", "0.0", float, m_flMinimumSimTime )
	DMXELEMENT_UNPACK_FIELD( "minimum rendered frames", "0", int, m_nMinimumFrames )
	DMXELEMENT_UNPACK_FIELD( "control point to disable rendering if it is the camera", "-1", int, m_nSkipRenderControlPoint )
	DMXELEMENT_UNPACK_FIELD( "control point to only enable rendering if it is the camera", "-1", int, m_nAllowRenderControlPoint )
	DMXELEMENT_UNPACK_FIELD( "maximum draw distance", "100000.0", float, m_flMaxDrawDistance )
	DMXELEMENT_UNPACK_FIELD( "time to sleep when not drawn", "8", float, m_flNoDrawTimeToGoToSleep )
	DMXELEMENT_UNPACK_FIELD( "Sort particles", "1", bool, m_bShouldSort )
	DMXELEMENT_UNPACK_FIELD( "batch particle systems", "0", bool, m_bShouldBatch )
	DMXELEMENT_UNPACK_FIELD( "view model effect", "0", bool, m_bViewModelEffect )
	DMXELEMENT_UNPACK_FIELD( "screen space effect", "0", bool, m_bScreenSpaceEffect )
	DMXELEMENT_UNPACK_FIELD( "draw through leafsystem", "1", bool, m_bDrawThroughLeafSystem )
    DMXELEMENT_UNPACK_FIELD( "maximum portal recursion depth", "8", int, m_nMaxRecursionDepth )
    DMXELEMENT_UNPACK_FIELD( "aggregation radius", "0", float, m_flAggregateRadius )
    DMXELEMENT_UNPACK_FIELD( "minimum free particles to aggregate", "0", int, m_nAggregationMinAvailableParticles )
    DMXELEMENT_UNPACK_FIELD( "minimum simulation time step", "0", float, m_flMinimumTimeStep )
	DMXELEMENT_UNPACK_FIELD( "minimum CPU level", "0", int, m_nMinCPULevel )
	DMXELEMENT_UNPACK_FIELD( "minimum GPU level", "0", int, m_nMinGPULevel )
    DMXELEMENT_UNPACK_FIELD( "freeze simulation after time", "1000000000", float, m_flStopSimulationAfterTime )
END_DMXELEMENT_UNPACK( CParticleSystemDefinition, s_pParticleSystemDefinitionUnpack )



//-----------------------------------------------------------------------------
//
// CParticleOperatorDefinition begins here
// A template describing how a particle system will function
//
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::UnlinkAllCollections()
{
	while ( m_pFirstCollection )
	{
		m_pFirstCollection->UnlinkFromDefList();
	}

	Assert( m_nFallbackCurrentCount == 0 );
}

const char *CParticleSystemDefinition::GetName() const
{
	return m_Name;
}


//-----------------------------------------------------------------------------
// Should we always precache this?
//-----------------------------------------------------------------------------
bool CParticleSystemDefinition::ShouldAlwaysPrecache() const
{
	return m_bAlwaysPrecache;
}


//-----------------------------------------------------------------------------
// Precache/uncache
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::Precache()
{
	if ( m_bIsPrecached )
		return;

	m_bIsPrecached = true;
#ifndef DEDICATED
	if ( !UTIL_IsDedicatedServer() &&  g_pMaterialSystem )
	{
		m_Material.Init( MaterialName(), TEXTURE_GROUP_OTHER, true );
		if ( m_Material->HasProxy() )
		{
			Warning( "Material %s used by particle systems cannot use proxies!\n", m_Material->GetName() );
			m_Material.Init( "debug/particleerror", TEXTURE_GROUP_OTHER, true );
		}
		g_pParticleSystemMgr->FindOrLoadSheet( this );

		// NOTE: Subtle. This has to be called after HasProxy, which will
		// have loaded all material vars. The "queue friendly" version of a material
		// doesn't precache material vars
		m_Material->GetColorModulation( &m_vecMaterialModulation[0], &m_vecMaterialModulation[1], &m_vecMaterialModulation[2] );
		m_vecMaterialModulation[3] = m_Material->GetAlphaModulation();
	}
#endif

	if ( HasFallback() )
	{
		CParticleSystemDefinition *pFallback = GetFallbackReplacementDefinition();
		if ( pFallback )
		{
			pFallback->Precache();
		}
	}
	// call the precache method of the renderers in case they need assets
	for( int i = 0; i < m_Renderers.Count(); i++ )
	{
		CParticleOperatorInstance *pOp = m_Renderers[i];
		pOp->Precache();
	}

	int nChildCount = m_Children.Count();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CParticleSystemDefinition *pChild;
		if ( m_Children[i].m_bUseNameBasedLookup )
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Name );
		}
		else
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Id );
		}

		if ( pChild )
		{
			pChild->Precache();
		}
	}
}

void CParticleSystemDefinition::Uncache()
{
	if ( !m_bIsPrecached )
		return;

	m_bIsPrecached = false;
	m_Material.Shutdown();	
//	m_Material.Init( "debug/particleerror", TEXTURE_GROUP_OTHER, true );
//	m_vecMaterialModulation.Init( 1.0f, 1.0f, 1.0f, 1.0f );
	if ( HasFallback() )
	{
		CParticleSystemDefinition *pFallback = GetFallbackReplacementDefinition();
		if ( pFallback )
		{
			pFallback->Uncache();
		}
	}
	for( int i = 0; i < m_Renderers.Count(); i++ )
	{
		CParticleOperatorInstance *pOp = m_Renderers[i];
		pOp->Uncache();
	}

	int nChildCount = m_Children.Count();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CParticleSystemDefinition *pChild;
		if ( m_Children[i].m_bUseNameBasedLookup )
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Name );
		}
		else
		{
			pChild = g_pParticleSystemMgr->FindParticleSystem( m_Children[i].m_Id );
		}

		if ( pChild )
		{
			pChild->Uncache();
		}
	}
}


//-----------------------------------------------------------------------------
// Has this been precached?
//-----------------------------------------------------------------------------
bool CParticleSystemDefinition::IsPrecached() const
{
	return m_bIsPrecached;
}

//-----------------------------------------------------------------------------
// Helper methods to help with unserialization
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::ParseOperators(
	const char *pszOpKey, ParticleFunctionType_t nFunctionType,
	CDmxElement *pElement,
	CUtlVector<CParticleOperatorInstance *> &outList)
{
	const CDmxAttribute* pAttribute = pElement->GetAttribute( pszOpKey );
	if ( !pAttribute || pAttribute->GetType() != AT_ELEMENT_ARRAY )
		return;

	const CUtlVector<IParticleOperatorDefinition *> &flist = g_pParticleSystemMgr->GetAvailableParticleOperatorList( nFunctionType );

	const CUtlVector< CDmxElement* >& ops = pAttribute->GetArray<CDmxElement*>( );
	int nCount = ops.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pOrigName = ops[i]->GetValueString( "functionName" );
		char const *pOpName = RemapOperatorName( pOrigName );
		if ( pOpName != pOrigName )
		{
			pElement->SetValue( "functionName", pOpName );
		}
		bool bFound = false;
		int nFunctionCount = flist.Count();
		for( int j = 0; j < nFunctionCount; ++j )
		{
			if ( Q_stricmp( pOpName, flist[j]->GetName() ) )
				continue;

			// found it!
			bFound = true;

			CParticleOperatorInstance *pNewRef = flist[j]->CreateInstance( ops[i]->GetId() );
			const DmxElementUnpackStructure_t *pUnpack = flist[j]->GetUnpackStructure();
			if ( pUnpack )
			{
				ops[i]->UnpackIntoStructure( pNewRef, pUnpack );
			}
			pNewRef->InitParams( this );
			pNewRef->CheckForFastPath();

			m_nAttributeReadMask |= pNewRef->GetReadAttributes();
			m_nControlPointReadMask |= pNewRef->GetReadControlPointMask();
			m_nControlPointNonPositionalMask |= pNewRef->GetNonPositionalControlPointMask();

			switch( nFunctionType )
			{
			case FUNCTION_INITIALIZER:
			case FUNCTION_EMITTER:
				m_nPerParticleInitializedAttributeMask |= pNewRef->GetWrittenAttributes();
				Assert( pNewRef->GetReadInitialAttributes() == 0 );
				break;

			case FUNCTION_OPERATOR:
			case FUNCTION_FORCEGENERATOR:
			case FUNCTION_CONSTRAINT:
				m_nPerParticleUpdatedAttributeMask |= pNewRef->GetWrittenAttributes();
				m_nInitialAttributeReadMask |= pNewRef->GetReadInitialAttributes();
				break;
				
			case FUNCTION_RENDERER:
				m_nPerParticleUpdatedAttributeMask |= pNewRef->GetWrittenAttributes();
				m_nInitialAttributeReadMask |= pNewRef->GetReadInitialAttributes();
				break;
			}

			// Special case: Reading particle ID means we're reading the initial particle id
			if ( ( pNewRef->GetReadAttributes() | pNewRef->GetReadInitialAttributes() ) & PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK )
			{
				m_nInitialAttributeReadMask |= PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK;
				m_nPerParticleInitializedAttributeMask |= PARTICLE_ATTRIBUTE_PARTICLE_ID_MASK;
			}

			outList.AddToTail( pNewRef );
			break;
		}

		if ( !bFound )
		{
			if ( flist.Count() )							// don't warn if no ops of that type defined (server)
				Warning( "Didn't find particle function %s\n", pOpName );
		}
	}
}

void CParticleSystemDefinition::ParseChildren( CDmxElement *pElement )
{
	const CUtlVector<CDmxElement*>& children = pElement->GetArray<CDmxElement*>( "children" );
	int nCount = children.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pChild = children[i]->GetValue<CDmxElement*>( "child" );
		if ( !pChild || Q_stricmp( pChild->GetTypeString(), "DmeParticleSystemDefinition" ) )
			continue;

		int j = m_Children.AddToTail();
		children[i]->UnpackIntoStructure( &m_Children[j], s_ChildrenInfoUnpack );
		m_Children[j].m_bUseNameBasedLookup = !pChild->GetValue<bool>( "preventNameBasedLookup" );
		if ( m_Children[j].m_bUseNameBasedLookup )
		{
			m_Children[j].m_Name = g_pParticleSystemMgr->FindOrAddParticleSystemIndex( pChild->GetName() );
		}
		else
		{
			CopyUniqueId( pChild->GetId(), &m_Children[j].m_Id );
		}

		// Check to see if this child has been encountered already, and if not, then
		// create a new particle definition for this child
		g_pParticleSystemMgr->AddParticleSystem( pChild );
	}
}

void CParticleSystemDefinition::Read( CDmxElement *pElement )
{
	m_Name = pElement->GetName();
	CopyUniqueId( pElement->GetId(), &m_Id );
	pElement->UnpackIntoStructure( this, s_pParticleSystemDefinitionUnpack );

	if ( m_nInitialParticles < 0 )
	{
		m_nInitialParticles = 0;
	}
	if ( m_nMaxParticles < 1 )
	{
		m_nMaxParticles = 1;
	}
	m_nMaxParticles *= g_nParticle_Multiplier;
	m_nMaxParticles = MIN( m_nMaxParticles, MAX_PARTICLES_IN_A_SYSTEM );
	if ( m_flCullRadius > 0 )
	{
		m_nControlPointReadMask |= 1ULL << m_nCullControlPoint;
	}

	ParseOperators( "renderers", FUNCTION_RENDERER, pElement, m_Renderers );
	ParseOperators( "operators", FUNCTION_OPERATOR, pElement, m_Operators );
	ParseOperators( "initializers", FUNCTION_INITIALIZER, pElement, m_Initializers );
	ParseOperators( "emitters", FUNCTION_EMITTER, pElement, m_Emitters );
	ParseChildren( pElement );
	ParseOperators( "forces", FUNCTION_FORCEGENERATOR, pElement, m_ForceGenerators );
	ParseOperators( "constraints", FUNCTION_CONSTRAINT, pElement, m_Constraints );
	SetupContextData();
}

IMaterial *CParticleSystemDefinition::GetMaterial() const
{
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	Assert( IsPrecached() );
	if ( !IsPrecached() )
		return NULL;
	return (IMaterial *) ( (const IMaterial *) m_Material );
}


CUtlSymbol CParticleSystemDefinition::GetSheetSymbol() const
{
	Assert( IsSheetSymbolCached() );
	return m_SheetSymbol;
}

void CParticleSystemDefinition::CacheSheetSymbol( CUtlSymbol sheetSymbol )
{
	m_SheetSymbol = sheetSymbol;
	m_bSheetSymbolCached = true;
}

bool CParticleSystemDefinition::IsSheetSymbolCached() const
{
	return m_bSheetSymbolCached;
}

void CParticleSystemDefinition::InvalidateSheetSymbol()
{
	m_bSheetSymbolCached = false;
}


//----------------------------------------------------------------------------------
// Returns the particle system fallback
//----------------------------------------------------------------------------------
CParticleSystemDefinition *CParticleSystemDefinition::GetFallbackReplacementDefinition() const
{
	if ( HasFallback() )
	{
		if ( !m_pFallback() )
		{
			const_cast< CParticleSystemDefinition* >( this )->m_pFallback.Set( g_pParticleSystemMgr->FindParticleSystem( m_FallbackReplacementName ) );
		}
		return m_pFallback();
	}
	return NULL;
}


//----------------------------------------------------------------------------------
// Does the particle system use the power of two frame buffer texture (refraction?)
//----------------------------------------------------------------------------------
bool CParticleSystemDefinition::UsesPowerOfTwoFrameBufferTexture()
{
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	Assert( IsPrecached() );
	return g_pMaterialSystem && m_Material && m_Material->NeedsPowerOfTwoFrameBufferTexture( false ); // The false checks if it will ever need the frame buffer, not just this frame
}

//----------------------------------------------------------------------------------
// Does the particle system use the power of two frame buffer texture (refraction?)
//----------------------------------------------------------------------------------
bool CParticleSystemDefinition::UsesFullFrameBufferTexture()
{
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	Assert( IsPrecached() );
	return g_pMaterialSystem && m_Material && m_Material->NeedsFullFrameBufferTexture( false ); // The false checks if it will ever need the frame buffer, not just this frame
}

//-----------------------------------------------------------------------------
// Helper methods to write particle systems
//-----------------------------------------------------------------------------
void CParticleSystemDefinition::WriteOperators( CDmxElement *pElement, 
	const char *pOpKeyName, const CUtlVector<CParticleOperatorInstance *> &inList )
{
	CDmxElementModifyScope modify( pElement );
	CDmxAttribute* pAttribute = pElement->AddAttribute( pOpKeyName );
	CUtlVector< CDmxElement* >& ops = pAttribute->GetArrayForEdit<CDmxElement*>( );

	int nCount = inList.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pOperator = CreateDmxElement( "DmeParticleOperator" );
		ops.AddToTail( pOperator );

		const IParticleOperatorDefinition *pDef = inList[i]->GetDefinition();
		pOperator->SetValue( "name", pDef->GetName() );
		pOperator->SetValue( "functionName", pDef->GetName() );

		const DmxElementUnpackStructure_t *pUnpack = pDef->GetUnpackStructure();
		if ( pUnpack )
		{
			pOperator->AddAttributesFromStructure( inList[i], pUnpack );
		}
	}
}

void CParticleSystemDefinition::WriteChildren( CDmxElement *pElement )
{
	CDmxElementModifyScope modify( pElement );
	CDmxAttribute* pAttribute = pElement->AddAttribute( "children" );
	CUtlVector< CDmxElement* >& children = pAttribute->GetArrayForEdit<CDmxElement*>( );
	int nCount = m_Children.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pChildRef = CreateDmxElement( "DmeParticleChild" );
		children.AddToTail( pChildRef );
		children[i]->AddAttributesFromStructure( &m_Children[i], s_ChildrenInfoUnpack );
		CDmxElement *pChildParticleSystem;
		if ( m_Children[i].m_bUseNameBasedLookup )
		{
			pChildParticleSystem = g_pParticleSystemMgr->CreateParticleDmxElement( 
				g_pParticleSystemMgr->GetParticleSystemNameFromIndex( m_Children[i].m_Name ) );
		}
		else
		{
			pChildParticleSystem = g_pParticleSystemMgr->CreateParticleDmxElement( m_Children[i].m_Id );
		}
		pChildRef->SetValue( "name", pChildParticleSystem->GetName() );
		pChildRef->SetValue( "child", pChildParticleSystem );
	}
}

CDmxElement *CParticleSystemDefinition::Write()
{
	const char *pName = GetName();

	CDmxElement *pElement = CreateDmxElement( "DmeParticleSystemDefinition" );
	pElement->SetValue( "name", pName );
	pElement->AddAttributesFromStructure( this, s_pParticleSystemDefinitionUnpack );
	WriteOperators( pElement, "renderers",m_Renderers );
	WriteOperators( pElement, "operators", m_Operators );
	WriteOperators( pElement, "initializers", m_Initializers );
	WriteOperators( pElement, "emitters", m_Emitters );
	WriteChildren( pElement );
	WriteOperators( pElement, "forces", m_ForceGenerators );
	WriteOperators( pElement, "constraints", m_Constraints );

	return pElement;
}

void CParticleSystemDefinition::SetupContextData( void )
{
	// calculate sizes and offsets for context data
	CUtlVector<CParticleOperatorInstance *> *olists[] = {
		&m_Operators, &m_Renderers, &m_Initializers, &m_Emitters, &m_ForceGenerators,
		&m_Constraints
	};
	CUtlVector<size_t> *offsetLists[] = {
		&m_nOperatorsCtxOffsets, &m_nRenderersCtxOffsets,
		&m_nInitializersCtxOffsets, &m_nEmittersCtxOffsets,
		&m_nForceGeneratorsCtxOffsets, &m_nConstraintsCtxOffsets,
	};

	// loop through all operators, fill in offset entries, and calulate total data needed
	m_nContextDataSize = 0;
	for( int i = 0; i < NELEMS( olists ); i++ )
	{
		int nCount = olists[i]->Count();
		for( int j = 0; j < nCount; j++ )
		{
			offsetLists[i]->AddToTail( m_nContextDataSize );
			m_nContextDataSize += (*olists[i])[j]->GetRequiredContextBytes();
			// align context data
			m_nContextDataSize = (m_nContextDataSize + 15) & (~0xf );
		}
	}
}


//-----------------------------------------------------------------------------
// Finds an operator by id
//-----------------------------------------------------------------------------
CUtlVector<CParticleOperatorInstance *> *CParticleSystemDefinition::GetOperatorList( ParticleFunctionType_t type )
{
	switch( type )
	{
	case FUNCTION_EMITTER:
		return &m_Emitters;
	case FUNCTION_RENDERER:
		return &m_Renderers;
	case FUNCTION_INITIALIZER:
		return &m_Initializers;
	case FUNCTION_OPERATOR:
		return &m_Operators;
	case FUNCTION_FORCEGENERATOR:
		return &m_ForceGenerators;
	case FUNCTION_CONSTRAINT:
		return &m_Constraints;
	default:
		Assert(0);
		return NULL;
	}
}


//-----------------------------------------------------------------------------
// Finds an operator by id
//-----------------------------------------------------------------------------
CParticleOperatorInstance *CParticleSystemDefinition::FindOperatorById( ParticleFunctionType_t type, const DmObjectId_t &id )
{
	CUtlVector<CParticleOperatorInstance *> *pVec = GetOperatorList( type );
	if ( !pVec )
		return NULL;

	int nCount = pVec->Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( IsUniqueIdEqual( id, pVec->Element(i)->GetId() ) )
			return pVec->Element(i);
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Finds an operator by name (slow!)
//-----------------------------------------------------------------------------
CParticleOperatorInstance *CParticleSystemDefinition::FindOperatorByName( const char *pOperatorName )
{
	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; i++ )
	{
		CUtlVector<CParticleOperatorInstance *> *pVec = ( i == FUNCTION_CHILDREN ) ? NULL : GetOperatorList( (ParticleFunctionType_t)i );
		if ( !pVec )
			continue;

		int nCount = pVec->Count();
		for ( int j = 0; j < nCount; ++j )
		{
			if ( !Q_stricmp( pOperatorName, pVec->Element(j)->GetDefinition()->GetName() ) )
				return pVec->Element(j);
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
//
// CParticleOperatorInstance
//
//-----------------------------------------------------------------------------
void CParticleOperatorInstance::InitNewParticles( CParticleCollection *pParticles,
												  int nFirstParticle, int nParticleCount,
												  int nAttributeWriteMask, void *pContext ) const
{
	if ( !nParticleCount )
		return;

	if ( nParticleCount < 16 )								// don't bother with vectorizing
															// unless enough particles to bother
	{
		InitNewParticlesScalar( pParticles, nFirstParticle, nParticleCount, nAttributeWriteMask, pContext );
		return;
	}
	
	int nHead = nFirstParticle & 3;
	if ( nHead )
	{
		// need to init up to 3 particles before we are block aligned
		int nHeadCount = MIN( nParticleCount, 4 - nHead );
		InitNewParticlesScalar( pParticles, nFirstParticle, nHeadCount, nAttributeWriteMask, pContext );
		nParticleCount -= nHeadCount;
		nFirstParticle += nHeadCount;
	}

	// now, we are aligned
	int nBlockCount = nParticleCount / 4;
	if ( nBlockCount )
	{
		InitNewParticlesBlock( pParticles, nFirstParticle / 4, nBlockCount, nAttributeWriteMask, pContext );
		nParticleCount -= 4 * nBlockCount;
		nFirstParticle += 4 * nBlockCount;
	}

	// do tail
	if ( nParticleCount )
	{
		InitNewParticlesScalar( pParticles, nFirstParticle, nParticleCount, nAttributeWriteMask, pContext );
	}
}


//-----------------------------------------------------------------------------
//
// CParticleCollection
//
//-----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// need custom new/delete for alignment for simd
//------------------------------------------------------------------------------
#include "tier0/memdbgoff.h"
void *CParticleCollection::operator new( size_t nSize )
{
	return MemAlloc_AllocAligned( nSize, 16 );
}

void* CParticleCollection::operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_AllocAlignedFileLine( nSize, 16, pFileName, nLine );
}

void CParticleCollection::operator delete(void *pData)
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void CParticleCollection::operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void *CWorldCollideContextData::operator new( size_t nSize )
{
	return MemAlloc_AllocAligned( nSize, 16 );
}

void* CWorldCollideContextData::operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_AllocAlignedFileLine( nSize, 16, pFileName, nLine );
}

void CWorldCollideContextData::operator delete(void *pData)
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void CWorldCollideContextData::operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}


#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CParticleCollection::CParticleCollection( )
{
	COMPILE_TIME_ASSERT( ( MAX_RANDOM_FLOATS & ( MAX_RANDOM_FLOATS - 1 ) ) == 0 );
	COMPILE_TIME_ASSERT( sizeof( s_pRandomFloats ) / sizeof( float ) >= MAX_RANDOM_FLOATS );

	Plat_FastMemset( this, 0, sizeof(CParticleCollection) );
	m_flOOMaxDistSqr = 1.0f;


	m_flPreviousDt = 0.05f;
	m_nParticleFlags = PCFLAGS_FIRST_FRAME;
	m_LocalLighting = Color(255, 255, 255, 255);
	m_LocalLightingCP = -1;
	m_bPendingRestart = false;
	m_flTargetDrawTime = 0;
	m_bQueuedStartEmission = false;
	m_bFrozen = false;
	m_pCPInfo = NULL;
	m_pCachedParticleBatches = NULL;
	m_bTriedLoadingSheet = false;
}

CParticleCollection::~CParticleCollection( void )
{
	UnlinkFromDefList();

	m_Children.Purge();

	if ( m_pParticleMemory )
	{
		MemAlloc_FreeAligned( m_pParticleMemory );
		m_pParticleMemory = NULL;
	}

	if ( m_pPreviousAttributeMemory )
	{
		MemAlloc_FreeAligned( m_pPreviousAttributeMemory );
		m_pPreviousAttributeMemory = NULL;
	}

	if ( m_pParticleInitialMemory )
	{
		MemAlloc_FreeAligned( m_pParticleInitialMemory );
		m_pParticleInitialMemory = NULL;
	}
	if ( m_pConstantMemory )
	{
		MemAlloc_FreeAligned( m_pConstantMemory );
		m_pConstantMemory = NULL;
	}
	if ( m_pOperatorContextData )
	{
		MemAlloc_FreeAligned( m_pOperatorContextData );
		m_pOperatorContextData = NULL;
	}

	for( int i = 0 ; i < ARRAYSIZE( m_pCollisionCacheData ) ; i++ )
	{
		if ( m_pCollisionCacheData[i] )
		{
			delete m_pCollisionCacheData[i];
			m_pCollisionCacheData[i] = NULL;
		}
	}
	if ( m_pCPInfo )
	{
		delete[] m_pCPInfo;
		m_pCPInfo = NULL;
	}

	if ( m_pCachedParticleBatches )
	{
		delete m_pCachedParticleBatches;
		m_pCachedParticleBatches = NULL;
	}
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
void CParticleCollection::Init( CParticleSystemDefinition *pDef, float flDelay, int nRandomSeed )
{
	m_pDef = pDef;
	
	// Link into def list
	LinkIntoDefList();

	m_pRenderable = NULL;

	InitStorage( pDef );
	
	// Initialize sheet data
	m_Sheet.Set( g_pParticleSystemMgr->FindOrLoadSheet( pDef ) );
	m_bTriedLoadingSheet = false;

	// FIXME: This seed needs to be recorded per instance!
	m_bIsScrubbable = ( nRandomSeed != 0 );
	if ( m_bIsScrubbable )
	{
		m_nRandomSeed = nRandomSeed;
	}
	else
	{
		m_nRandomSeed = (int)(intp)this;
#ifndef _DEBUG
		m_nRandomSeed += Plat_MSTime();
#endif
	}

	SetAttributeToConstant( PARTICLE_ATTRIBUTE_XYZ, 0.0f, 0.0f, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_PREV_XYZ, 0.0f, 0.0f, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_LIFE_DURATION, 1.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_RADIUS, pDef->m_flConstantRadius );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ROTATION, pDef->m_flConstantRotation );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ROTATION_SPEED, pDef->m_flConstantRotationSpeed );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_TINT_RGB,
		pDef->m_ConstantColor.r() / 255.0f, pDef->m_ConstantColor.g() / 255.0f,
		pDef->m_ConstantColor.b() / 255.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ALPHA, pDef->m_ConstantColor.a() / 255.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_CREATION_TIME, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, pDef->m_nConstantSequenceNumber );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1, pDef->m_nConstantSequenceNumber1 );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_TRAIL_LENGTH, 0.1f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_PARTICLE_ID, 0 );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_YAW, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_ALPHA2, 1.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_PITCH, 0.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_NORMAL, 
		pDef->m_ConstantNormal.x, pDef->m_ConstantNormal.y, pDef->m_ConstantNormal.z );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_GLOW_RGB, 1.0f, 1.0f, 1.0f );
	SetAttributeToConstant( PARTICLE_ATTRIBUTE_GLOW_ALPHA, 1.0f );

	// Offset the child in time
	m_flCurTime = -flDelay;
	m_fl4CurTime = ReplicateX4( m_flCurTime );
	if ( m_pDef->m_nContextDataSize )
	{
		m_pOperatorContextData = reinterpret_cast<uint8 *> 
			( MemAlloc_AllocAligned( m_pDef->m_nContextDataSize, 16 ) );
	}
	
	m_flNextSleepTime = g_pParticleSystemMgr->GetLastSimulationTime() + pDef->m_flNoDrawTimeToGoToSleep;

	m_nControlPointReadMask = pDef->m_nControlPointReadMask;
	m_nControlPointNonPositionalMask = pDef->m_nControlPointNonPositionalMask;

	// Instance child particle systems
	int nChildCount = pDef->m_Children.Count();
	for ( int i = 0; i < nChildCount; ++i )
	{
		if ( nRandomSeed != 0 )
		{
			nRandomSeed += 129;
		}

		CParticleCollection *pChild;
		if ( pDef->m_Children[i].m_bUseNameBasedLookup )
		{
			pChild = g_pParticleSystemMgr->CreateParticleCollection( pDef->m_Children[i].m_Name, -m_flCurTime + pDef->m_Children[i].m_flDelay, nRandomSeed );
		}
		else
		{
			pChild = g_pParticleSystemMgr->CreateParticleCollection( pDef->m_Children[i].m_Id, -m_flCurTime + pDef->m_Children[i].m_flDelay, nRandomSeed );
		}
		if ( pChild )
		{
			pChild->m_pParent = this;
			m_Children.AddToTail( pChild );
			m_nControlPointReadMask |= pChild->m_nControlPointReadMask;
			m_nControlPointNonPositionalMask |= pChild->m_nControlPointNonPositionalMask;
			if ( pDef->m_Children[i].m_bEndCap )
			{
				pChild->m_flCurTime = -FLT_MAX;
			}
		}
	}

	if ( !IsValid() )
		return;

	m_bIsTranslucent = ComputeIsTranslucent();
	m_bIsTwoPass = ComputeIsTwoPass();
	m_bIsBatchable = ComputeIsBatchable();
	m_bIsOrderImportant = ComputeIsOrderImportant();
	m_bRunForParentApplyKillList = ComputeRunForParentApplyKillList();
	LabelTextureUsage();
	m_bAnyUsesPowerOfTwoFrameBufferTexture = ComputeUsesPowerOfTwoFrameBufferTexture();
	m_bAnyUsesFullFrameBufferTexture = ComputeUsesFullFrameBufferTexture();

	m_bInEndCap = false;

	// now, allocate the control point data.
	// we will always allocate on extra, so that if someone sets a control point that the particle system doesn't
	// use, then there will be a dummy one to write to.
	int nHighest;
	for( nHighest = 63; nHighest > 0 ; nHighest-- )
	{
		if ( m_nControlPointReadMask & ( 1ll << nHighest ) )
		{
			break;
		}
	}
	m_nNumControlPointsAllocated = MIN( MAX_PARTICLE_CONTROL_POINTS, 2 + nHighest );
	//Warning( " save %d bytes by only allocating %d cp's\n", ( 64 - m_nNumControlPointsAllocated ) * sizeof( CParticleCPInfo ), m_nNumControlPointsAllocated );
	m_pCPInfo = new CParticleCPInfo[ m_nNumControlPointsAllocated];

	// align all control point orientations with the global world
	Plat_FastMemset( m_pCPInfo, 0, sizeof( CParticleCPInfo ) * m_nNumControlPointsAllocated );
	for( int i=0; i < m_nNumControlPointsAllocated; i++ )
	{
		ControlPoint(i).m_ForwardVector.y = 1.0f;
		ControlPoint(i).m_UpVector.z = 1.0f;
		ControlPoint(i).m_RightVector.x = 1.0f;
	}
	// now, init context data
	CUtlVector<CParticleOperatorInstance *> *olists[] =
	{
		&(m_pDef->m_Operators), &(m_pDef->m_Renderers), 
		&(m_pDef->m_Initializers), &(m_pDef->m_Emitters),
		&(m_pDef->m_ForceGenerators),
		&(m_pDef->m_Constraints),
	};
	CUtlVector<size_t> *offsetlists[]=
	{
		&(m_pDef->m_nOperatorsCtxOffsets), &(m_pDef->m_nRenderersCtxOffsets),
		&(m_pDef->m_nInitializersCtxOffsets), &(m_pDef->m_nEmittersCtxOffsets),
		&(m_pDef->m_nForceGeneratorsCtxOffsets),
		&(m_pDef->m_nConstraintsCtxOffsets),

	};

	for( int i=0; i<NELEMS( olists ); i++ )
	{
		int nOperatorCount = olists[i]->Count();
		for( int j=0; j < nOperatorCount; j++ )
		{
			(*olists[i])[j]->InitializeContextData( this, m_pOperatorContextData+ (*offsetlists)[i][j] );
		}
	}


}

int CParticleCollection::GetCurrentParticleDefCount( CParticleSystemDefinition* pDef )
{
	int nDefCount = pDef->m_nFallbackCurrentCount;
	CParticleSystemDefinition *pFallback = pDef;
	while ( pFallback && pFallback->HasFallback() )
	{
		pFallback = pFallback->GetFallbackReplacementDefinition();
		if ( !pFallback )
		{
			break;
		}
		nDefCount += pFallback->m_nFallbackCurrentCount;
	}
	return nDefCount;
}

bool CParticleCollection::Init( CParticleSystemDefinition *pDef )
{
	if ( pDef->GetMinCPULevel() > g_pParticleSystemMgr->GetParticleCPULevel() || pDef->GetMinGPULevel() > g_pParticleSystemMgr->GetParticleGPULevel() )
	{
		pDef = NULL;
		return false;
	}

	float flFallbackMultiplier = g_pParticleSystemMgr->GetFallbackMultiplier();
	float flFallbackBase = g_pParticleSystemMgr->GetFallbackBase();
	float flThresholdSimMS = g_pParticleSystemMgr->GetSimFallbackThresholdMs();

	int nFallbackCount = GetCurrentParticleDefCount( pDef );

	// Determine if we've gone past our maximum allowable time for simulating particles.
	// NOTE: If particle simulation starts overlapping client operations, then we'll need to 
	//       make setting and querying of sim duration threadsafe.
	float flPreviousSimMS = g_pParticleSystemMgr->GetLastSimulationDuration() * 1000.0f;
	if ( flPreviousSimMS > flThresholdSimMS )
	{
		float flMSOver = flPreviousSimMS - flThresholdSimMS;
		float flSimFallbackBaseMultiplier = g_pParticleSystemMgr->GetSimFallbackBaseMultiplier();

		// Increase the fallback base by a factor of r_particle_sim_fallback_base_multiplier 
		// for each millisecond we're over the threshold
		flFallbackBase += flMSOver * flSimFallbackBaseMultiplier;

		// Uncomment to spew when we're trying to fall back because sim time took too long
		//Warning( "Particle sim took too long: %f, threshold %f\n", flPreviousSimMS, flThresholdSimMS );
	}

	// If our maximum number of simultaneous definitions has been exceeded fallback to the appropriate def
	while ( pDef && pDef->HasFallback() && 
		( ( nFallbackCount * flFallbackMultiplier ) + flFallbackBase ) >= pDef->m_nFallbackMaxCount )
	{
		nFallbackCount -= pDef->m_nFallbackCurrentCount;
		pDef = pDef->GetFallbackReplacementDefinition();
	}

	if ( !pDef ) // || !pDef->IsPrecached() )
	{
		Warning( "Particlelib: Missing precache for particle system type \"%s\"!\n", pDef ? pDef->GetName() : "unknown" );
		CParticleSystemDefinition *pErrorDef = g_pParticleSystemMgr->FindParticleSystem( "error" );
		if ( pErrorDef )
		{
			pDef = pErrorDef;
		}
	}
	Init( pDef, 0.0f, 0 );
	return IsValid();
}

bool CParticleCollection::Init( const char *pParticleSystemName )
{
	if ( !pParticleSystemName )
		return false;

	CParticleSystemDefinition *pDef = g_pParticleSystemMgr->FindParticleSystem( pParticleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type \"%s\"!\n", pParticleSystemName );
		return false;
	}

	return Init( pDef );
}


bool CParticleCollection::IsFullyValid( void ) const
{
	if ( m_pDef.GetObject() == NULL )
		return false;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( !p->IsFullyValid() )
			return false;
	}

	return true;
}

bool CParticleCollection::DependsOnSystem( const char *pName ) const
{
	if ( m_pDef.GetObject() == NULL )
		return false;

	if ( m_pDef->m_Name == pName )
		return true;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->DependsOnSystem(pName) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// List management for collections sharing the same particle definition
//-----------------------------------------------------------------------------
void CParticleCollection::LinkIntoDefList( )
{
	Assert( !m_pPrevDef && !m_pNextDef );

	++( m_pDef->m_nFallbackCurrentCount );
	m_pPrevDef = NULL;
	m_pNextDef = m_pDef->m_pFirstCollection;
	m_pDef->m_pFirstCollection = this;
	if ( m_pNextDef )
	{
		m_pNextDef->m_pPrevDef = this;
	}

#ifdef _DEBUG
	CParticleCollection *pCollection = m_pDef->FirstCollection();
	while ( pCollection )
	{
		Assert( pCollection->m_pDef == m_pDef );
		pCollection = pCollection->GetNextCollectionUsingSameDef();
	}
#endif
}

void CParticleCollection::UnlinkFromDefList( )
{
	if ( !m_pDef )
		return;

	--( m_pDef->m_nFallbackCurrentCount );
	if ( m_pDef->m_pFirstCollection == this )
	{
		m_pDef->m_pFirstCollection = m_pNextDef;
		Assert( !m_pPrevDef );
	}
	else 
	{
		Assert( m_pPrevDef );
		m_pPrevDef->m_pNextDef = m_pNextDef;
	}

	if ( m_pNextDef )
	{
		m_pNextDef->m_pPrevDef = m_pPrevDef;
	}

	m_pNextDef = m_pPrevDef = NULL;

#ifdef _DEBUG
	CParticleCollection *pCollection = m_pDef->FirstCollection();
	while ( pCollection )
	{
		Assert( pCollection->m_pDef == m_pDef );
		pCollection = pCollection->GetNextCollectionUsingSameDef();
	}
#endif

	m_pDef = NULL;
}


//-----------------------------------------------------------------------------
// Reset the particle cache if the frame has changed
//-----------------------------------------------------------------------------
void CParticleCollection::ResetParticleCache()
{
	if ( m_pCachedParticleBatches )
	{

		uint32 nCurrentFrame = g_pMaterialSystem->GetCurrentFrameCount();
		if ( m_pCachedParticleBatches->m_nLastValidParticleCacheFrame != nCurrentFrame )
		{
			m_pCachedParticleBatches->ClearBatches();
			m_pCachedParticleBatches->m_nLastValidParticleCacheFrame = nCurrentFrame;
		}
	}
}

//-----------------------------------------------------------------------------
// Get the cached particle batches for this particular particle collection
//-----------------------------------------------------------------------------
CCachedParticleBatches *CParticleCollection::GetCachedParticleBatches()
{
	if ( !m_pCachedParticleBatches )
	{
		m_pCachedParticleBatches = new CCachedParticleBatches();
	}

	return m_pCachedParticleBatches;
}

//-----------------------------------------------------------------------------
// Particle memory initialization
//-----------------------------------------------------------------------------
void CParticleCollection::InitStorage( CParticleSystemDefinition *pDef )
{
	Assert( pDef->m_nMaxParticles < 65536 );

	m_nMaxAllowedParticles = MIN( MAX_PARTICLES_IN_A_SYSTEM, pDef->m_nMaxParticles );
	m_nAllocatedParticles = 4 + 4 * ( ( m_nMaxAllowedParticles + 3 ) / 4 );						    

	int nConstantMemorySize = 3 * 4 * MAX_PARTICLE_ATTRIBUTES * sizeof(float) + 16;
						 
	// Align allocation for constant attributes to 16 byte boundaries
	m_pConstantMemory =	(uint8 * ) MemAlloc_AllocAligned( nConstantMemorySize, 16 );
	m_pConstantAttributes = ( float * ) m_pConstantMemory;

	m_nPerParticleInitializedAttributeMask = pDef->m_nPerParticleInitializedAttributeMask;
	m_nPerParticleUpdatedAttributeMask = pDef->m_nPerParticleUpdatedAttributeMask;

	// Only worry about initial attributes that are per-particle *and* are updated at a later time
	// If they aren't updated at a later time, then we can just point the initial + current pointers at the same memory
	m_nPerParticleReadInitialAttributeMask = pDef->m_nInitialAttributeReadMask & 
		( pDef->m_nPerParticleInitializedAttributeMask & pDef->m_nPerParticleUpdatedAttributeMask );

	// This is the mask of attributes which are initialized per-particle, but never updated
	// *and* where operators want to read initial particle state
	int nPerParticleReadConstantAttributeMask = pDef->m_nInitialAttributeReadMask & 
		( pDef->m_nPerParticleInitializedAttributeMask & ( ~pDef->m_nPerParticleUpdatedAttributeMask ) );

	int sz = 0;
	int nInitialAttributeSize = 0;
	int nPerParticleAttributeMask = m_nPerParticleInitializedAttributeMask | m_nPerParticleUpdatedAttributeMask;
	for( int bit = 0; bit < MAX_PARTICLE_ATTRIBUTES; bit++ )
	{
		int nAttrSize = ( ( 1 << bit ) & ATTRIBUTES_WHICH_ARE_VEC3S_MASK ) ? 3 : 1;
		if ( nPerParticleAttributeMask & ( 1 << bit ) )
		{ 
			sz += nAttrSize;
		}
		if ( m_nPerParticleReadInitialAttributeMask & ( 1 << bit ) )
		{
			nInitialAttributeSize += nAttrSize;
		}
	}

	// Gotta allocate a couple extra floats to account for padding
	int nAllocationSize = m_nAllocatedParticles * sz * sizeof(float) + sizeof( FourVectors );
	m_pParticleMemory = ( uint8 * ) MemAlloc_AllocAligned( nAllocationSize, 16 );
	m_nAttributeMemorySize = nAllocationSize;
	Plat_FastMemset( m_pParticleMemory, 0, nAllocationSize );

	// Allocate space for the initial attributes
	if ( nInitialAttributeSize != 0 )
	{
		int nInitialAllocationSize = m_nAllocatedParticles * nInitialAttributeSize * sizeof(float) + sizeof( FourVectors );
		m_pParticleInitialMemory = ( uint8 * ) MemAlloc_AllocAligned( nInitialAllocationSize, 16 );
		Plat_FastMemset( m_pParticleInitialMemory, 0, nInitialAllocationSize );
	}

	// Align allocation to 16-byte boundaries
	float *pMem = ( float* ) m_pParticleMemory;
	float *pInitialMem = ( float* )( m_pParticleInitialMemory );

	// Point each attribute to memory associated with that attribute
	for( int bit = 0; bit < MAX_PARTICLE_ATTRIBUTES; bit++ )
	{
		int nAttrSize = ( ( 1 << bit ) & ATTRIBUTES_WHICH_ARE_VEC3S_MASK ) ? 3 : 1;

		if ( nPerParticleAttributeMask & ( 1 << bit ) )
		{ 
			m_ParticleAttributes.m_pAttributes[ bit ] = pMem;
			m_ParticleAttributes.m_nFloatStrides[ bit ] = nAttrSize * 4;
			pMem += nAttrSize * m_nAllocatedParticles;
		}
		else
		{
			m_ParticleAttributes.m_pAttributes[ bit ] = GetConstantAttributeMemory( bit );
			m_ParticleAttributes.m_nFloatStrides[ bit ] = 0;
		}

		// Are we reading
		if ( pDef->m_nInitialAttributeReadMask & ( 1 << bit ) )
		{
			if ( m_nPerParticleReadInitialAttributeMask & ( 1 << bit ) )
			{
				Assert( pInitialMem );
				m_ParticleInitialAttributes.m_pAttributes[ bit ] = pInitialMem;
				m_ParticleInitialAttributes.m_nFloatStrides[ bit ] = nAttrSize * 4;
				pInitialMem += nAttrSize * m_nAllocatedParticles;
			}
			else if ( nPerParticleReadConstantAttributeMask & ( 1 << bit ) )
			{
				m_ParticleInitialAttributes.m_pAttributes[ bit ] = m_ParticleAttributes.m_pAttributes[ bit ];
				m_ParticleInitialAttributes.m_nFloatStrides[ bit ] = m_ParticleAttributes.m_nFloatStrides[ bit ];
			}
			else
			{
				m_ParticleInitialAttributes.m_pAttributes[ bit ] = GetConstantAttributeMemory( bit );
				m_ParticleInitialAttributes.m_nFloatStrides[ bit ] = 0;
			}
		}
		else
		{
			// Catch errors where code is reading data it didn't request
			m_ParticleInitialAttributes.m_pAttributes[ bit ] = NULL;
			m_ParticleInitialAttributes.m_nFloatStrides[ bit ] = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Returns the particle collection name
//-----------------------------------------------------------------------------
const char *CParticleCollection::GetName() const
{
	return m_pDef ? m_pDef->GetName() : "";
}


//-----------------------------------------------------------------------------
// Does the particle system use the frame buffer texture (refraction?)
//-----------------------------------------------------------------------------
bool CParticleCollection::UsesPowerOfTwoFrameBufferTexture( bool bThisFrame ) const
{
	if ( ! m_bAnyUsesPowerOfTwoFrameBufferTexture )			// quick out if neither us or our children ever use
	{
		return false;
	}
	if ( bThisFrame )
	{
		return SystemContainsParticlesWithBoolSet( &CParticleCollection::m_bUsesPowerOfTwoFrameBufferTexture );
	}
	return true;
}
//-----------------------------------------------------------------------------
// Does the particle system use the full frame buffer texture (soft particles)
//-----------------------------------------------------------------------------
bool CParticleCollection::UsesFullFrameBufferTexture( bool bThisFrame ) const
{
	if ( ! m_bAnyUsesFullFrameBufferTexture )				// quick out if neither us or our children ever use
	{
		return false;
	}
	if ( bThisFrame )
	{
		return SystemContainsParticlesWithBoolSet( &CParticleCollection::m_bUsesFullFrameBufferTexture );
	}
	return true;
}

bool CParticleCollection::SystemContainsParticlesWithBoolSet( bool CParticleCollection::*pField ) const
{
	if ( m_nActiveParticles && ( this->*pField ) )
		return true;
	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->SystemContainsParticlesWithBoolSet( pField ) )
			return true;
	}
	return false;

}

void CParticleCollection::LabelTextureUsage( void )
{
	if ( m_pDef )
	{
		m_bUsesPowerOfTwoFrameBufferTexture = m_pDef->UsesPowerOfTwoFrameBufferTexture();
		m_bUsesFullFrameBufferTexture = m_pDef->UsesFullFrameBufferTexture();
	}

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		p->LabelTextureUsage();
	}
}

bool CParticleCollection::ComputeUsesPowerOfTwoFrameBufferTexture()
{
	if ( !m_pDef )
		return false;

	if ( m_pDef->UsesPowerOfTwoFrameBufferTexture() )
		return true;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->UsesPowerOfTwoFrameBufferTexture( false ) )
			return true;
	}

	return false;
}

bool CParticleCollection::ComputeUsesFullFrameBufferTexture()
{
	if ( !m_pDef )
		return false;

	if ( m_pDef->UsesFullFrameBufferTexture() )
		return true;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->UsesFullFrameBufferTexture( false ) )
			return true;
	}

	return false;
}
//-----------------------------------------------------------------------------
// Is the particle system two-pass?
//-----------------------------------------------------------------------------
bool CParticleCollection::ContainsOpaqueCollections()
{
	if ( !m_pDef )
		return false;

	IMaterial *pMaterial = m_pDef->GetMaterial();

	if ( pMaterial && ( !m_pDef->GetMaterial()->IsTranslucent() ) )
		return true;
	
	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->ContainsOpaqueCollections( ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Is the particle system two-pass?
//-----------------------------------------------------------------------------
bool CParticleCollection::IsTwoPass() const
{
	return m_bIsTwoPass;
}

bool CParticleCollection::ComputeIsTwoPass()
{
	if ( !ComputeIsTranslucent() )
		return false;

	return ContainsOpaqueCollections();
}


//-----------------------------------------------------------------------------
// Is the particle system translucent
//-----------------------------------------------------------------------------
bool CParticleCollection::IsTranslucent() const
{
	return m_bIsTranslucent;
}

bool CParticleCollection::ComputeIsTranslucent()
{
	if ( !m_pDef )
		return false;

	IMaterial *pMaterial = m_pDef->GetMaterial();

	if ( pMaterial && ( m_pDef->GetMaterial()->IsTranslucent() ) )
		return true;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->IsTranslucent( ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Is the particle system batchable
//-----------------------------------------------------------------------------
bool CParticleCollection::IsBatchable() const
{
	return m_bIsBatchable;
}

bool CParticleCollection::ComputeIsBatchable()
{
	int nRendererCount = GetRendererCount();
	for( int i = 0; i < nRendererCount; i++ )
	{
		if ( !GetRenderer( i )->IsBatchable() )
			return false;
	}

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( !p->IsBatchable() )
			return false;
	}

	return true;
}



//-----------------------------------------------------------------------------
// Is the order of the particles important
//-----------------------------------------------------------------------------
bool CParticleCollection::IsOrderImportant() const
{
	return m_bIsOrderImportant;
}

bool CParticleCollection::ComputeIsOrderImportant()
{
	int nRendererCount = GetRendererCount();
	for( int i = 0; i < nRendererCount; i++ )
	{
		if ( GetRenderer( i )->IsOrderImportant() )
			return true;
	}

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		if ( p->IsOrderImportant() )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Does this system want to run inside its parent's ApplyKillList?
//-----------------------------------------------------------------------------
bool CParticleCollection::ShouldRunForParentApplyKillList() const
{
	return m_bRunForParentApplyKillList;
}

bool CParticleCollection::ComputeRunForParentApplyKillList()
{
	// Only run the system during ApplyKillList if an emitter operator wants to run then
	// (initializers may then run subsequently, but they won't without an emitter!)
	bool bApplyingKillList = true;
	int nEmitterCount = m_pDef->m_Emitters.Count();
	for( int i = 0; i < nEmitterCount; i++ )
	{
		if ( m_pDef->m_Emitters[i]->ShouldRun( bApplyingKillList ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Renderer iteration
//-----------------------------------------------------------------------------
int CParticleCollection::GetRendererCount() const
{
	return IsValid() ? m_pDef->m_Renderers.Count() : 0;
}

CParticleOperatorInstance *CParticleCollection::GetRenderer( int i )
{
	return IsValid() ? m_pDef->m_Renderers[i] : NULL;
}

void *CParticleCollection::GetRendererContext( int i )
{
	return IsValid() ? m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i] : NULL;
}


//-----------------------------------------------------------------------------
// Visualize operators (for editing/debugging)
//-----------------------------------------------------------------------------
void CParticleCollection::VisualizeOperator( const DmObjectId_t *pOpId )
{
	m_pRenderOp = NULL;
	if ( !pOpId || !m_pDef )
		return;

	m_pRenderOp = m_pDef->FindOperatorById( FUNCTION_EMITTER, *pOpId );
	if ( !m_pRenderOp )
	{
		m_pRenderOp = m_pDef->FindOperatorById( FUNCTION_INITIALIZER, *pOpId );
		if ( !m_pRenderOp )
		{
			m_pRenderOp = m_pDef->FindOperatorById( FUNCTION_OPERATOR, *pOpId );
		}
	}
}


float FadeInOut( float flFadeInStart, float flFadeInEnd, float flFadeOutStart, float flFadeOutEnd, float flCurTime )
{
	if ( flFadeInStart > flCurTime )						// started yet?
		return 0.0;

	if ( ( flFadeOutEnd > 0. ) && ( flFadeOutEnd < flCurTime ) ) // timed out?
		return 0.;

	// handle out of order cases
	flFadeInEnd = MAX( flFadeInEnd, flFadeInStart );
	flFadeOutStart = MAX( flFadeOutStart, flFadeInEnd );
	flFadeOutEnd = MAX( flFadeOutEnd, flFadeOutStart );

	float flStrength = 1.0;
	if (
		( flFadeInEnd > flCurTime ) &&
		( flFadeInEnd > flFadeInStart ) )
		flStrength = MIN( flStrength, FLerp( 0, 1, flFadeInStart, flFadeInEnd, flCurTime ) );

	if ( ( flCurTime > flFadeOutStart) &&
		 ( flFadeOutEnd > flFadeOutStart) )
		flStrength = MIN( flStrength, FLerp( 0, 1, flFadeOutEnd, flFadeOutStart, flCurTime ) );

	return flStrength;

}

bool CParticleCollection::CheckIfOperatorShouldRun( 
	CParticleOperatorInstance const * pOp ,
	float *pflCurStrength,
	bool bApplyingParentKillList )
{
	if ( !pOp->ShouldRun( bApplyingParentKillList ) )
		return false;

	if ( pOp->m_bStrengthFastPath )
	{
		*pflCurStrength = 1.0;
		return true;
	}
	if ( pOp->m_nOpEndCapState != -1 )
	{
		if ( m_bInEndCap != ( pOp->m_nOpEndCapState == 1 ) )
			return false;
	}
	float flTime=m_flCurTime;
	if ( pOp->m_nOpTimeOffsetSeed )							// allow per-instance-of-particle-system random phase control for operator strength.
	{
		float flOffset = RandomFloat( pOp->m_nOpTimeOffsetSeed, pOp->m_flOpTimeOffsetMin, pOp->m_flOpTimeOffsetMax );
		flTime += flOffset;
		flTime = MAX( 0.0, flTime );
	}
	if ( pOp->m_nOpTimeScaleSeed && ( flTime > pOp->m_flOpStartFadeInTime ) )
	{
		float flTimeScalar = 1.0 / MAX( .0001, RandomFloat( pOp->m_nOpTimeScaleSeed, pOp->m_flOpTimeScaleMin, pOp->m_flOpTimeScaleMax ) );
		flTime = pOp->m_flOpStartFadeInTime + flTimeScalar * ( flTime - pOp->m_flOpStartFadeInTime );
	}
	if ( pOp->m_flOpFadeOscillatePeriod > 0.0 )
	{
		flTime = fmod( m_flCurTime*( 1.0/pOp->m_flOpFadeOscillatePeriod ), 1.0 );
	}

	float flStrength = FadeInOut( pOp->m_flOpStartFadeInTime, pOp->m_flOpEndFadeInTime,
								  pOp->m_flOpStartFadeOutTime, pOp->m_flOpEndFadeOutTime,
								  flTime );
	if ( pOp->m_nOpStrengthScaleSeed )
	{
		float flStrengthMultiplier = RandomFloat( pOp->m_nOpStrengthScaleSeed, pOp->m_flOpStrengthMinScale, pOp->m_flOpStrengthMaxScale );
		flStrength *= MAX( 0., flStrength * flStrengthMultiplier );
	}
	*pflCurStrength = flStrength;
	return ( flStrength > 0.0 );
}


void CParticleOperatorInstance::CheckForFastPath( void )
{
	// store away whether this operator has any of the operator modulation params set (most ops dont)
	if (
		( m_flOpStartFadeInTime == 0. ) &&
		( m_flOpEndFadeOutTime == 0. ) &&
		( m_flOpStartFadeOutTime == 0. ) &&
		( m_flOpEndFadeOutTime == 0. ) &&
		( m_flOpTimeOffsetMin == 0 ) &&
		( m_flOpTimeOffsetMax == 0 ) &&
		( m_flOpTimeScaleMin == 1 ) &&
		( m_flOpTimeScaleMax == 1 ) &&
		( m_flOpStrengthMaxScale == 1 ) &&
		( m_flOpStrengthMinScale == 1 ) &&
		( m_nOpEndCapState == -1 ) )
	{
		m_bStrengthFastPath = true;
	}
	else
	{
		m_bStrengthFastPath = false;
	}

}

bool CParticleOperatorInstance::HasAttribute( CParticleCollection *pParticles, int nAttribute ) const
{
	return ( pParticles->m_ParticleAttributes.Stride( nAttribute ) > 0 );
}

KillListItem_t *CParticleOperatorInstance::GetParentKillList( CParticleCollection *pParticles, int &nNumParticlesToKill ) const
{
	if ( pParticles->m_pParent )
	{
		nNumParticlesToKill = pParticles->m_pParent->m_nNumParticlesToKill;
		return pParticles->m_pParent->m_pParticleKillList;
	}
	nNumParticlesToKill = 0;
	return NULL;
}

#ifdef NDEBUG
#define CHECKSYSTEM( p ) 0
#else
static void CHECKSYSTEM( CParticleCollection *pParticles )
{
//	Assert( pParticles->m_nActiveParticles <= pParticles->m_pDef->m_nMaxParticles );
	for ( int i = 0; i < pParticles->m_nActiveParticles; ++i )
	{
		const float *xyz = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, i );
		const float *rad = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, i );
		const float *xyz_prev = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, i );
		Assert( IsFinite( rad[0] ) );
		Assert( IsFinite( xyz[0] ) );
		Assert( IsFinite( xyz[4] ) );
		Assert( IsFinite( xyz[8] ) );
		Assert( IsFinite( xyz_prev[0] ) );
		Assert( IsFinite( xyz_prev[4] ) );
		Assert( IsFinite( xyz_prev[8] ) );
	}
}
#endif

void CParticleCollection::RunRestartedEmitters( void )
{
	// run all emitters once that want to respond to a restart
	if ( m_nParticleFlags & PCFLAGS_FIRST_FRAME )			// in case we aggregated twice before _any_ sim
	{
		SimulateFirstFrame();
		m_nParticleFlags &= ~PCFLAGS_FIRST_FRAME;
	}
	else
	{
		UpdatePrevControlPoints( m_flPreviousDt );				// make sure control points are virgin
	}

	int nEmitterCount = m_pDef->m_Emitters.Count();
	for( int i=0; i < nEmitterCount; i++ )
	{
		int nOldParticleCount = m_nActiveParticles;
		float flEmitStrength = 0;
		if ( CheckIfOperatorShouldRun( m_pDef->m_Emitters[i], &flEmitStrength ) )
		{
			uint32 nInittedMask = m_pDef->m_Emitters[i]->Emit( 
				this, flEmitStrength, 
				m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i] );
			if ( nOldParticleCount != m_nActiveParticles )
			{
				// init newly emitted particles
				InitializeNewParticles( nOldParticleCount, m_nActiveParticles - nOldParticleCount, nInittedMask );
				CHECKSYSTEM( this );
			}
		}
	}
	for( CParticleCollection *pChild = m_Children.m_pHead; pChild != NULL; pChild = pChild->m_pNext )
		pChild->RunRestartedEmitters();
	
}

// rj: this may not be the correct thing to do, to set all of the children to the same renderable.  particles_new may need to set the renderable to themselves instead.
void CParticleCollection::SetRenderable( void *pRenderable )
{
	m_pRenderable = pRenderable;

	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		p->SetRenderable( pRenderable );
	}
}

//-----------------------------------------------------------------------------
// Restarts a particle system
//-----------------------------------------------------------------------------
void CParticleCollection::Restart( EParticleRestartMode_t eMode )
{
	// Always reset the framecount for tick rates - this needs to be reset so that systems which are framerate dependent get updated frame counts
	m_nDrawnFrames = 0;
	// if we already have a pending restart, process it now
	if ( m_bPendingRestart )
	{
		RunRestartedEmitters();
		m_bPendingRestart = false;
	}
	if ( eMode == RESTART_RESET_AND_MAKE_SURE_EMITS_HAPPEN )
	{
		m_bPendingRestart = true;
		m_nParticleFlags &= ~PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED;
	}
	int nEmitterCount = m_pDef->m_Emitters.Count();
	for( int i = 0; i < nEmitterCount; i++ )
	{
		m_pDef->m_Emitters[i]->Restart( this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i] );
	}

	int nInitializerCount = m_pDef->m_Initializers.Count();
	for( int i = 0; i < nInitializerCount; i++ )
	{
		m_pDef->m_Initializers[i]->Restart( this, m_pOperatorContextData + m_pDef->m_nInitializersCtxOffsets[i] );
	}

	// Update all children
	for( CParticleCollection *pChild = m_Children.m_pHead; pChild != NULL; pChild = pChild->m_pNext )
	{
		// Remove any delays from the time (otherwise we're offset by it oddly)
		pChild->Restart( eMode );
	}

}


//-----------------------------------------------------------------------------
// Main entry point for rendering
//-----------------------------------------------------------------------------
void CParticleCollection::Render( int nViewRecursionLevel, IMatRenderContext *pRenderContext, const Vector4D &vecDiffuseModulation, bool bTranslucentOnly, void *pCameraObject )
{
	if ( !IsValid() )
		return;

	if ( !m_Sheet() && !m_bTriedLoadingSheet )
	{
		m_bTriedLoadingSheet = true;
		m_Sheet.Set( g_pParticleSystemMgr->FindOrLoadSheet( m_pDef, true ) );
	}

	m_flNextSleepTime = MAX( m_flNextSleepTime, ( g_pParticleSystemMgr->GetLastSimulationTime() + m_pDef->m_flNoDrawTimeToGoToSleep ));

	if ( m_nActiveParticles != 0 )
	{
		Vector4D vecActualModulation;
		Vector4DMultiply( vecDiffuseModulation, m_pDef->m_vecMaterialModulation, vecActualModulation );
		IMaterial *pMaterial = m_pDef->GetMaterial();

		if ( pMaterial &&
			 ( !bTranslucentOnly || m_pDef->GetMaterial()->IsTranslucent() || ( vecActualModulation[3] != 1.0f ) ) )
		{
#if MEASURE_PARTICLE_PERF
			double flSTime = Plat_FloatTime();
#endif

			int nCount = m_pDef->m_Renderers.Count();
			for( int i = 0; i < nCount; i++ )
			{
				float flStrength;
				if ( !CheckIfOperatorShouldRun( m_pDef->m_Renderers[i], &flStrength ) )
					continue;

				if ( m_pDef->IsScreenSpaceEffect() )
				{
					pRenderContext->MatrixMode( MATERIAL_VIEW );
					pRenderContext->PushMatrix();
					pRenderContext->LoadIdentity();
					pRenderContext->MatrixMode( MATERIAL_PROJECTION );
					pRenderContext->PushMatrix();
					pRenderContext->LoadIdentity();
					pRenderContext->Ortho( -100, -100, 100, 100, -100, 100 );
					m_pDef->m_Renderers[i]->Render(
						pRenderContext, this, vecActualModulation, m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i], nViewRecursionLevel );
					pRenderContext->MatrixMode( MATERIAL_VIEW );
					pRenderContext->PopMatrix();
					pRenderContext->MatrixMode( MATERIAL_PROJECTION );
					pRenderContext->PopMatrix();
				}
				else
				{
					m_pDef->m_Renderers[i]->Render(
						pRenderContext, this, vecActualModulation, m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i], nViewRecursionLevel );
				}
			}

#if MEASURE_PARTICLE_PERF
			float flETime = Plat_FloatTime() - flSTime;
			m_pDef->m_flUncomittedTotalRenderTime += flETime;
			m_pDef->m_flMaxMeasuredRenderTime = MAX( m_pDef->m_flMaxMeasuredRenderTime, flETime );
#endif
		}
	}
	
	// let children render
	for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
	{
		p->Render( nViewRecursionLevel, pRenderContext, vecDiffuseModulation, bTranslucentOnly, pCameraObject );
	}

	// Visualize specific ops for debugging/editing
	if ( m_pRenderOp )
	{
		m_pRenderOp->Render( this );
	}
}

void CParticleCollection::UpdatePrevControlPoints( float dt )
{
	m_flPreviousDt = dt;
	for(int i=0; i <= m_nHighestCP; i++ )
		ControlPoint( i ).m_PrevPosition = ControlPoint( i ).m_Position;
	m_nParticleFlags |= PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED;
}

#if MEASURE_PARTICLE_PERF

#if VPROF_LEVEL > 0
#define START_OP float flOpStartTime = Plat_FloatTime(); VPROF_ENTER_SCOPE(pOp->GetDefinition()->GetName())
#else
#define START_OP float flOpStartTime = Plat_FloatTime();
#endif

#if VPROF_LEVEL > 0
#define END_OP  if ( 1 ) {																						\
	float flETime = Plat_FloatTime() - flOpStartTime;									\
	IParticleOperatorDefinition *pDef = (IParticleOperatorDefinition *) pOp->m_pDef;	\
	pDef->RecordExecutionTime( flETime );												\
} \
	VPROF_EXIT_SCOPE()
#else
#define END_OP  if ( 1 ) {																						\
	float flETime = Plat_FloatTime() - flOpStartTime;									\
	IParticleOperatorDefinition *pDef = (IParticleOperatorDefinition *) pOp->m_pDef;	\
	pDef->RecordExecutionTime( flETime );												\
}
#endif
#else
#define START_OP
#define END_OP
#endif

void CParticleCollection::InitializeNewParticles( int nFirstParticle, int nParticleCount, uint32 nInittedMask, bool bApplyingParentKillList )
{
	VPROF_BUDGET( "CParticleCollection::InitializeNewParticles", VPROF_BUDGETGROUP_PARTICLE_SIMULATION );

#ifdef _DEBUG
	m_bIsRunningInitializers = true;
#endif

	// now, initialize the attributes of all the new particles
	int nPerParticleAttributeMask = m_nPerParticleInitializedAttributeMask | m_nPerParticleUpdatedAttributeMask;
	int nAttrsLeftToInit = nPerParticleAttributeMask & ~nInittedMask;
	int nInitializerCount = m_pDef->m_Initializers.Count();
	for ( int i = 0; i < nInitializerCount; i++ )
	{
		CParticleOperatorInstance *pOp = m_pDef->m_Initializers[i];
		int nInitializerAttrMask = pOp->GetWrittenAttributes();
		if ( ( ( nInitializerAttrMask & nAttrsLeftToInit ) == 0 ) || pOp->InitMultipleOverride() )
			continue;
		if ( !pOp->ShouldRun( bApplyingParentKillList ) )
			continue;

		void *pContext = m_pOperatorContextData + m_pDef->m_nInitializersCtxOffsets[i];
		START_OP;
		if ( m_bIsScrubbable && !pOp->IsScrubSafe() )
		{
			for ( int j = 0; j < nParticleCount; ++j )
			{
				pOp->InitNewParticles( this, nFirstParticle + j, 1, nAttrsLeftToInit, pContext );
			}
		}
		else
		{
			pOp->InitNewParticles( this, nFirstParticle, nParticleCount, nAttrsLeftToInit, pContext );
		}
		END_OP;
		nAttrsLeftToInit &= ~nInitializerAttrMask;
	}

	// always run second tier initializers (modifiers) after first tier - this ensures they don't get stomped.
	for ( int i = 0; i < nInitializerCount; i++ )
	{
		int nInitializerAttrMask = m_pDef->m_Initializers[i]->GetWrittenAttributes();
		CParticleOperatorInstance *pOp = m_pDef->m_Initializers[i];
		if ( !pOp->InitMultipleOverride() )
			continue;
		if ( !pOp->ShouldRun( bApplyingParentKillList ) )
			continue;

		void *pContext = m_pOperatorContextData + m_pDef->m_nInitializersCtxOffsets[i];
		START_OP;
		if ( m_bIsScrubbable && !pOp->IsScrubSafe() )
		{
			for ( int j = 0; j < nParticleCount; ++j )
			{
				pOp->InitNewParticles( this, nFirstParticle + j, 1, nAttrsLeftToInit, pContext );
			}
		}
		else
		{
			pOp->InitNewParticles( this, nFirstParticle, nParticleCount, nAttrsLeftToInit, pContext );
		}
		END_OP;
		nAttrsLeftToInit &= ~nInitializerAttrMask;
	}

#ifdef _DEBUG
	m_bIsRunningInitializers = false;
#endif

	InitParticleAttributes( nFirstParticle, nParticleCount, nAttrsLeftToInit );

	CopyInitialAttributeValues( nFirstParticle, nParticleCount );
}

void CParticleCollection::SkipToTime( float t )
{
	if ( t > m_flCurTime )
	{
		UpdatePrevControlPoints( t - m_flCurTime );
		m_flCurTime = t;	
		m_fl4CurTime = ReplicateX4( t );
		m_nParticleFlags &= ~PCFLAGS_FIRST_FRAME;
		
		// FIXME: In future, we may have to tell operators, initializers about this too
		int nEmitterCount = m_pDef->m_Emitters.Count();
		int i;
		for( i = 0; i < nEmitterCount; i++ )
		{
			m_pDef->m_Emitters[i]->SkipToTime( t, this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i] );
		}

		CParticleCollection *pChild;

		// Update all children
		for( i = 0, pChild = m_Children.m_pHead; pChild != NULL; pChild = pChild->m_pNext, i++ )
		{
			// Remove any delays from the time (otherwise we're offset by it oddly)
			pChild->SkipToTime( t - m_pDef->m_Children[i].m_flDelay );
		}
	}
}


void CParticleCollection::SimulateFirstFrame( )
{
	m_flPrevSimTime = 1.0e23;
	m_flDt = 0.0f;
	m_nDrawnFrames = 0;

	// For the first frame, copy over the initial control points
	if ( ( m_nParticleFlags & PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED ) == 0 )
	{
		UpdatePrevControlPoints( 0.05f );
	}

	m_nOperatorRandomSampleOffset = 0;
	int nCount = m_pDef->m_Operators.Count();
	for( int i = 0; i < nCount; i++ )
	{
		float flStrength = 0;
		CParticleOperatorInstance *pOp = m_pDef->m_Operators[i];
		if ( pOp->ShouldRunBeforeEmitters() &&
			 CheckIfOperatorShouldRun( pOp, &flStrength ) )
		{
			pOp->Operate( this, flStrength, m_pOperatorContextData + m_pDef->m_nOperatorsCtxOffsets[i] );
			CHECKSYSTEM( this );
			UpdatePrevControlPoints( 0.05f );
		}
		m_nOperatorRandomSampleOffset += 17;
	}
	
	// first, create initial particles
	int nNumToCreate = MIN( m_pDef->m_nInitialParticles, m_nMaxAllowedParticles );
	if ( nNumToCreate > 0 )
	{
		SetNActiveParticles( nNumToCreate );
		InitializeNewParticles( 0, nNumToCreate, 0 );
		CHECKSYSTEM( this );
	}
}

void CParticleCollection::EmitAndInit( CParticleCollection *pCollection, bool bApplyingParentKillList ) // static
{
	if ( bApplyingParentKillList && !pCollection->ShouldRunForParentApplyKillList() )
		return;

	int nEmitterCount = pCollection->m_pDef->m_Emitters.Count();
	for( int i = 0; i < nEmitterCount; i++ )
	{
		int nOldParticleCount = pCollection->m_nActiveParticles;
		float flEmitStrength = 0;
		if ( pCollection->CheckIfOperatorShouldRun( pCollection->m_pDef->m_Emitters[i], &flEmitStrength, bApplyingParentKillList ) )
		{
			uint32 nInittedMask = pCollection->m_pDef->m_Emitters[i]->Emit( 
				pCollection, flEmitStrength, pCollection->m_pOperatorContextData + pCollection->m_pDef->m_nEmittersCtxOffsets[i] );
			if ( nOldParticleCount != pCollection->m_nActiveParticles )
			{
				// init newly emitted particles
				pCollection->InitializeNewParticles( nOldParticleCount, pCollection->m_nActiveParticles - nOldParticleCount, nInittedMask, bApplyingParentKillList );
				CHECKSYSTEM( pCollection );
			}
		}
	}
}


void CParticleCollection::Simulate( float dt )
{
	VPROF_BUDGET( "CParticleCollection::Simulate", VPROF_BUDGETGROUP_PARTICLE_SIMULATION );
	if ( ( dt < 0.0f ) || ( m_bFrozen ) )
		return;

	if ( !m_pDef )
		return;

	// Don't do anything until we've hit t == 0
	// This is used for delayed children
	if ( m_flCurTime < 0.0f )
	{
		if ( dt >= 1.0e-22 )
		{
			m_flCurTime += dt;
			m_fl4CurTime = ReplicateX4( m_flCurTime );
			UpdatePrevControlPoints( dt );
		}
		return;
	}

	// run initializers if necessary (once we hit t == 0)
	if ( m_nParticleFlags & PCFLAGS_FIRST_FRAME )
	{
		SimulateFirstFrame();
		m_nParticleFlags &= ~PCFLAGS_FIRST_FRAME;
	}

	else
	{
		// if the system has been Reset, we need to copy the control points to prev control points
		if ( ! ( m_nParticleFlags & PCFLAGS_PREV_CONTROL_POINTS_INITIALIZED ) )
			UpdatePrevControlPoints( m_flPreviousDt );
	}

	if ( dt < 1.0e-22 )
		return;

	m_bPendingRestart = false;


#if MEASURE_PARTICLE_PERF
	float flStartSimTime = Plat_FloatTime();
#endif

	bool bAttachedKillList = false;
	
	if ( ! HasAttachedKillList() )
	{
		g_pParticleSystemMgr->AttachKillList( this );
		bAttachedKillList = true;
	}
	
	float flRemainingDt = dt;
	float flMaxDT = 0.1;									// default
	if ( m_pDef->m_flMaximumTimeStep > 0.0 )
		flMaxDT = m_pDef->m_flMaximumTimeStep;

	// Limit timestep if needed (prevents short lived particles from being created and destroyed before being rendered.
	//if ( m_pDef->m_flMaximumSimTime != 0.0 && !m_bHasDrawnOnce )
	if ( m_pDef->m_flMaximumSimTime != 0.0 && ( m_nDrawnFrames <= m_pDef->m_nMinimumFrames ) )
	{
		if ( ( flRemainingDt + m_flCurTime ) > m_pDef->m_flMaximumSimTime )
		{
			//if delta+current > checkpoint then delta = checkpoint - current
			flRemainingDt = m_pDef->m_flMaximumSimTime - m_flCurTime;
			flRemainingDt = MAX( m_pDef->m_flMinimumSimTime, flRemainingDt );
		}
		m_nDrawnFrames += 1;
	}

	flRemainingDt = MIN( flRemainingDt, 10 * flMaxDT );	// no more than 10 passes ever

	m_flTargetDrawTime += flRemainingDt;

	if ( ( m_flTargetDrawTime >= m_flPrevSimTime ) && ( m_flTargetDrawTime < m_flCurTime ) )
	{
		// we can skip simulation
		flRemainingDt = 0;
	}

	float flMinTime = m_pDef->m_flMinimumTimeStep;
	bool bSaveOldValuesForInterpolation = false;

	while( flRemainingDt > 0.0 )
	{
		float flDT_ThisStep = flRemainingDt;
		if ( flDT_ThisStep > flMaxDT )
		{
			flDT_ThisStep = flMaxDT;
		}
		else
		{
			if ( flMinTime > flDT_ThisStep )				// can't do lerping if its going to take multiple steps?
			{
				flDT_ThisStep = flMinTime;
				bSaveOldValuesForInterpolation = true;
			}
		}
		flRemainingDt -= flDT_ThisStep;
		if ( m_flDt )
			m_flPreviousDt = m_flDt;
		m_flDt = flDT_ThisStep;
		m_flPrevSimTime = m_flCurTime;
		m_flCurTime += flDT_ThisStep;
		m_fl4CurTime = ReplicateX4( m_flCurTime );
		
		// now, if we are oging to interpolate, copy the current values of all attributes away
		if ( bSaveOldValuesForInterpolation )
		{
			// !! speed - we could copy just the active region.
			if ( ! m_pPreviousAttributeMemory )
			{
				m_pPreviousAttributeMemory = ( uint8 * ) MemAlloc_AllocAligned( m_nAttributeMemorySize, 16 );
				memset( m_pPreviousAttributeMemory, 0, m_nAttributeMemorySize );
				// set up the pointers
				m_PreviousFrameAttributes = m_ParticleAttributes;
				for( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; i++ )
				{
					if ( m_ParticleAttributes.Stride( i ) )
					{
						m_PreviousFrameAttributes.m_pAttributes[i] = ( float * ) 
							( GetPrevAttributeMemory() + 
							  ( m_ParticleAttributes.ByteAddress( i ) - GetAttributeMemory() ) );
					}
				}
			}
			CopyParticleAttributesToPreviousAttributes();
		}

#ifdef _DEBUG
		m_bIsRunningOperators = true;
#endif
		
		m_nOperatorRandomSampleOffset = 0;
		int nCount = m_pDef->m_Operators.Count();
		for( int i = 0; i < nCount; i++ )
		{
			float flStrength;
			CParticleOperatorInstance *pOp = m_pDef->m_Operators[i];
			if ( pOp->ShouldRunBeforeEmitters() &&
				 CheckIfOperatorShouldRun( pOp, &flStrength ) )
			{
				START_OP;
				pOp->Operate( this, flStrength, m_pOperatorContextData + m_pDef->m_nOperatorsCtxOffsets[i] );
				END_OP;
				CHECKSYSTEM( this );
				if ( m_nNumParticlesToKill )
				{
					ApplyKillList();
				}
				m_nOperatorRandomSampleOffset += 17;
			}
		}
#ifdef _DEBUG
		m_bIsRunningOperators = false;
#endif

		// Run emitters and initializers:
		EmitAndInit( this );

#ifdef _DEBUG
		m_bIsRunningOperators = true;
#endif
		
		m_nOperatorRandomSampleOffset = 0;
		nCount = m_pDef->m_Operators.Count();
		if ( m_nActiveParticles )
			for( int i = 0; i < nCount; i++ )
			{
				float flStrength;
				CParticleOperatorInstance *pOp = m_pDef->m_Operators[i];
				if ( (!  pOp->ShouldRunBeforeEmitters() ) &&
					 CheckIfOperatorShouldRun( pOp, &flStrength ) )
				{
					START_OP;
					pOp->Operate( this, flStrength, m_pOperatorContextData + m_pDef->m_nOperatorsCtxOffsets[i] );
					END_OP;
					CHECKSYSTEM( this );
					if ( m_nNumParticlesToKill )
					{
						ApplyKillList();
						if ( ! m_nActiveParticles )
							break;								// don't run any more operators
					}
					m_nOperatorRandomSampleOffset += 17;
				}
			}
#ifdef _DEBUG
		m_bIsRunningOperators = false;
#endif

		nCount = m_pDef->m_Renderers.Count();
		for( int i = 0; i < nCount; i++ )
		{
			CParticleOperatorInstance *pOp = m_pDef->m_Renderers[i];
			START_OP;
			pOp->PostSimulate( this, m_pOperatorContextData + m_pDef->m_nRenderersCtxOffsets[i] );
			END_OP;
		}
	}

#if MEASURE_PARTICLE_PERF
	m_pDef->m_nMaximumActiveParticles = MAX( m_pDef->m_nMaximumActiveParticles, m_nActiveParticles );
	float flETime = Plat_FloatTime() - flStartSimTime;
	m_pDef->m_flUncomittedTotalSimTime += flETime;
	m_pDef->m_flMaxMeasuredSimTime = MAX( m_pDef->m_flMaxMeasuredSimTime, flETime );
#endif

	// let children simulate
	for( CParticleCollection *i = m_Children.m_pHead; i; i = i->m_pNext )
	{
		LoanKillListTo( i );								// re-use the allocated kill list for the children
		i->Simulate( dt );
		i->m_pParticleKillList = NULL;
	}
	if ( bAttachedKillList )
		g_pParticleSystemMgr->DetachKillList( this );
	UpdatePrevControlPoints( dt );

	// Bloat the bounding box by bounds around the control point
	BloatBoundsUsingControlPoint();

	// check for freezing
	if ( m_pDef->m_flStopSimulationAfterTime < m_flCurTime )
	{
		m_bFrozen = true;
	}


// FIXME: Is there a way of doing this iteratively?
//	RecomputeBounds();
}


//-----------------------------------------------------------------------------
// Copies the constant attributes into the per-particle attributes
//-----------------------------------------------------------------------------
void CParticleCollection::InitParticleAttributes( int nStartParticle, int nNumParticles, int nAttrsLeftToInit )
{
	if ( nAttrsLeftToInit == 0 )
		return;

	// !! speed!! do sse init here
	for( int i = nStartParticle; i < nStartParticle + nNumParticles; i++ )
	{
		for ( int nAttr = 0; nAttr < MAX_PARTICLE_ATTRIBUTES; ++nAttr )
		{
			if ( ( nAttrsLeftToInit & ( 1 << nAttr ) ) == 0 )
				continue;

			float *pAttrData = GetFloatAttributePtrForWrite( nAttr, i );

			// Special case for particle id
			if ( nAttr == PARTICLE_ATTRIBUTE_PARTICLE_ID )
			{
				*( (int*)pAttrData ) = ( m_nRandomSeed + m_nUniqueParticleId ) & RANDOM_FLOAT_MASK;
				m_nUniqueParticleId++;
				continue;
			}

			// Special case for the creation time mask
			if ( nAttr == PARTICLE_ATTRIBUTE_CREATION_TIME )
			{
				*pAttrData = m_flCurTime;
				continue;
			}

			// If this assertion fails, it means we're writing into constant memory, which is a nono
			Assert( m_ParticleAttributes.Stride( nAttr ) != 0 );
			float *pConstantAttr = GetConstantAttributeMemory( nAttr );
			*pAttrData = *pConstantAttr;
			if ( m_ParticleAttributes.Stride( nAttr ) == 12 )
			{
				pAttrData[4] = pConstantAttr[4];
				pAttrData[8] = pConstantAttr[8];
			}
		}
	}
}

void CParticleCollection::CopyInitialAttributeValues( int nStartParticle, int nNumParticles )
{
	// if doinginterpolated sim, update the previous values to be the current ones, otherwise we may interpolate between
	// old values based upon a previous particle that was in this slot.

	if ( m_nPerParticleReadInitialAttributeMask == 0 )
		return;

	// FIXME: Do SSE copy here
	for( int i = nStartParticle; i < nStartParticle + nNumParticles; i++ )
	{
		for ( int nAttr = 0; nAttr < MAX_PARTICLE_ATTRIBUTES; ++nAttr )
		{
			if ( m_nPerParticleReadInitialAttributeMask & (1 << nAttr) )
			{
				const float *pSrcAttribute = GetFloatAttributePtr( nAttr, i );
				float *pDestAttribute = GetInitialFloatAttributePtrForWrite( nAttr, i );
				Assert( m_ParticleInitialAttributes.Stride( nAttr ) != 0 );
				Assert( m_ParticleAttributes.Stride( nAttr ) == m_ParticleInitialAttributes.Stride( nAttr ) );
				*pDestAttribute = *pSrcAttribute;
				if ( m_ParticleAttributes.Stride( nAttr ) == 12 )
				{
					pDestAttribute[4] = pSrcAttribute[4];
					pDestAttribute[8] = pSrcAttribute[8];
				}
			}
		}
	}
}

void CParticleCollection::CopyParticleAttributesToPreviousAttributes( void ) const
{
	for( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; i++ )
	{
		if ( m_PreviousFrameAttributes.Stride( i ) )
		{
			int nSz = m_nPaddedActiveParticles * 4 * m_PreviousFrameAttributes.Stride( i );
			memcpy( m_PreviousFrameAttributes.Address( i ), m_ParticleAttributes.Address( i ), nSz );
		}
	}
}

//-----------------------------------------------------------------------------e
// Computes a random vector inside a sphere
//-----------------------------------------------------------------------------
float CParticleCollection::RandomVectorInUnitSphere( int nRandomSampleId, Vector *pVector )
{
	// Guarantee uniform random distribution within a sphere
	// Graphics gems III contains this algorithm ("Nonuniform random point sets via warping")
	float u = RandomFloat( nRandomSampleId, 0.0001f, 1.0f );
	float v = RandomFloat( nRandomSampleId+1, 0.0001f, 1.0f );
	float w = RandomFloat( nRandomSampleId+2, 0.0001f, 1.0f );

	float flPhi = acos( 1 - 2 * u );
	float flTheta = 2 * M_PI * v;
	float flRadius = powf( w, 1.0f / 3.0f );

	float flSinPhi, flCosPhi;
	float flSinTheta, flCosTheta;
	SinCos( flPhi, &flSinPhi, &flCosPhi );
	SinCos( flTheta, &flSinTheta, &flCosTheta );

	pVector->x = flRadius * flSinPhi * flCosTheta;
	pVector->y = flRadius * flSinPhi * flSinTheta;
	pVector->z = flRadius * flCosPhi;
	return flRadius;
}



//-----------------------------------------------------------------------------
// Used to retrieve the position of a control point
// somewhere between m_flCurTime and m_flCurTime - m_fPreviousDT
//-----------------------------------------------------------------------------
void CParticleCollection::GetControlPointAtTime( int nControlPoint, float flTime, Vector *pControlPoint )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
// 	if ( nControlPoint > GetHighestControlPoint() )
// 	{
// 		DevWarning(2, "Warning : Particle system (%s) using unassigned ControlPoint %d!\n", GetName(), nControlPoint );
// 	}

	float flPrevTime = m_flCurTime - m_flDt;

	// While this assert is valid, the below if statement is a good enough band-aid to make it so that 
	// particles aren't appearing a weird locations.
	//	Assert( flTime + 0.5f >= flPrevTime && flTime <= m_flCurTime );

	if ( flTime < flPrevTime )
	{
		flTime = flPrevTime;
	}

	float deltaTime = ( flTime - flPrevTime );

	if ( m_flDt == 0.0f || ( deltaTime == 0.0f ) )
	{
		VectorCopy( ControlPoint( nControlPoint ).m_Position, *pControlPoint );
		return;
	}

	float t = deltaTime / m_flDt;
	VectorLerp( ControlPoint( nControlPoint ).m_PrevPosition, ControlPoint( nControlPoint ).m_Position, t, *pControlPoint );
	Assert( IsFinite(pControlPoint->x) && IsFinite(pControlPoint->y) && IsFinite(pControlPoint->z) );
}

//-----------------------------------------------------------------------------
// Used to retrieve the previous position of a control point
// 
//-----------------------------------------------------------------------------
void CParticleCollection::GetControlPointAtPrevTime( int nControlPoint, Vector *pControlPoint )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	*pControlPoint = ControlPoint( nControlPoint ).m_PrevPosition;
}

void CParticleCollection::GetControlPointTransformAtCurrentTime( int nControlPoint, matrix3x4_t *pMat )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	const Vector &vecControlPoint = GetControlPointAtCurrentTime( nControlPoint );

	// FIXME: Use quaternion lerp to get control point transform at time
	Vector left;
	VectorMultiply( ControlPoint( nControlPoint ).m_RightVector, -1.0f, left );
	pMat->Init( ControlPoint( nControlPoint ).m_ForwardVector, left, ControlPoint( nControlPoint ).m_UpVector, vecControlPoint );
}

void CParticleCollection::GetControlPointTransformAtCurrentTime( int nControlPoint, VMatrix *pMat )
{
	GetControlPointTransformAtCurrentTime( nControlPoint, &pMat->As3x4() );
	pMat->m[3][0] = pMat->m[3][1] = pMat->m[3][2] = 0.0f; pMat->m[3][3] = 1.0f;
}

void CParticleCollection::GetControlPointOrientationAtTime( int nControlPoint, float flTime, Vector *pForward, Vector *pRight, Vector *pUp )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );

	// FIXME: Use quaternion lerp to get control point transform at time
	*pForward = ControlPoint( nControlPoint ).m_ForwardVector;
	*pRight = ControlPoint( nControlPoint ).m_RightVector;
	*pUp = ControlPoint( nControlPoint ).m_UpVector;
}

void CParticleCollection::GetControlPointTransformAtTime( int nControlPoint, float flTime, matrix3x4_t *pMat )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	Vector vecControlPoint;
	GetControlPointAtTime( nControlPoint, flTime, &vecControlPoint );

	// FIXME: Use quaternion lerp to get control point transform at time
	Vector left;
	VectorMultiply( ControlPoint(nControlPoint).m_RightVector, -1.0f, left );
	pMat->Init( ControlPoint( nControlPoint ).m_ForwardVector, left, ControlPoint( nControlPoint ).m_UpVector, vecControlPoint );
}

void CParticleCollection::GetControlPointTransformAtTime( int nControlPoint, float flTime, VMatrix *pMat )
{
	GetControlPointTransformAtTime( nControlPoint, flTime, &pMat->As3x4() );
	pMat->m[3][0] = pMat->m[3][1] = pMat->m[3][2] = 0.0f; pMat->m[3][3] = 1.0f;
}

void CParticleCollection::GetControlPointTransformAtTime( int nControlPoint, float flTime, CParticleSIMDTransformation *pXForm )
{
	Assert( m_pDef->ReadsControlPoint( nControlPoint ) );
	Vector vecControlPoint;
	GetControlPointAtTime( nControlPoint, flTime, &vecControlPoint );

	pXForm->m_v4Origin.DuplicateVector( vecControlPoint );
	pXForm->m_v4Fwd.DuplicateVector( ControlPoint( nControlPoint ).m_ForwardVector );
	pXForm->m_v4Up.DuplicateVector( ControlPoint( nControlPoint ).m_UpVector );
	//Vector left;
	//VectorMultiply( ControlPoint(nControlPoint).m_RightVector, -1.0f, left );
	pXForm->m_v4Right.DuplicateVector( ControlPoint( nControlPoint ).m_RightVector );

}

int CParticleCollection::GetHighestControlPoint( void ) const
{
	return m_nHighestCP;
}

//-----------------------------------------------------------------------------
// Returns the render bounds
//-----------------------------------------------------------------------------
void CParticleCollection::GetBounds( Vector *pMin, Vector *pMax )
{
	*pMin = m_MinBounds;
	*pMax = m_MaxBounds;
}

//-----------------------------------------------------------------------------
// Bloat the bounding box by bounds around the control point
//-----------------------------------------------------------------------------
void CParticleCollection::BloatBoundsUsingControlPoint()
{
	// more specifically, some particle systems were using "start" as an input, so it got set as control point 1,
	// so other particle systems had an extra point in their bounding box, that generally remained at the world origin
	RecomputeBounds();

	// Deal with children
	// NOTE: Bounds have been recomputed for children prior to this call in Simulate
	bool bIsValid = m_bBoundsValid;
	Vector vecMins, vecMaxs;
	for( CParticleCollection *i = m_Children.m_pHead; i; i = i->m_pNext )
	{
		// mdonofrio - skip screen space effects, they have CPs at the origin
		if( (i->m_nActiveParticles > 0 ) &&
			(!i->m_pDef->IsScreenSpaceEffect())  )
		{
			i->GetBounds( &vecMins, &vecMaxs );
			VectorMin( m_MinBounds, vecMins, m_MinBounds );
			VectorMax( m_MaxBounds, vecMaxs, m_MaxBounds );
			bIsValid = ( bIsValid || i->m_bBoundsValid );
		}
	}

	m_bBoundsValid = bIsValid;
}


//-----------------------------------------------------------------------------
// Recomputes the bounds
//-----------------------------------------------------------------------------
inline void UpdateBounds( fltx4 &min_, fltx4 &max_, fltx4 &sum_, fltx4 val )
{
	min_ = MinSIMD( min_, val );
	max_ = MaxSIMD( max_, val );
	sum_ = AddSIMD( sum_, val );
}

inline void UpdateBounds( float &min_, float &max_, float &sum_, float val )
{
	min_ = MIN( min_, val );
	max_ = MAX( max_, val );
	sum_ = sum_ + val;
}

void CParticleCollection::RecomputeBounds( void )
{
	// mdonofrio - skip screen space effects (as they have CPs at the origin) as well as those with 0 active particles 
	if( ( m_nActiveParticles == 0 ) ||
		m_pDef->IsScreenSpaceEffect() )
	{
		m_bBoundsValid = false;
		m_MinBounds.Init( FLT_MAX, FLT_MAX, FLT_MAX );
		m_MaxBounds.Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );
		m_Center.Init();
		return;
	}

	fltx4 min_x = ReplicateX4(1.0e23);
	fltx4 min_y = min_x;
	fltx4 min_z = min_x;
	fltx4 max_x = ReplicateX4(-1.0e23);
	fltx4 max_y = max_x;
	fltx4 max_z = max_x;

	fltx4 sum_x = Four_Zeros;
	fltx4 sum_y = Four_Zeros;
	fltx4 sum_z = Four_Zeros;

	float flMaxTail = m_pDef->GetMaxTailLength();
	float flOODt = ( m_flDt != 0.0f ) ? ( 1.0f / m_flDt ) : 1.0f;
	fltx4 maxtail = ReplicateX4( flMaxTail );
	fltx4 oodt = ReplicateX4( flOODt );

	size_t xyz_stride, prev_stride, trail_stride;
	const fltx4 *xyz   = GetM128AttributePtr( PARTICLE_ATTRIBUTE_XYZ,			&xyz_stride );
	const fltx4 *prev  = GetM128AttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ,		&prev_stride );
	const fltx4 *trail = GetM128AttributePtr( PARTICLE_ATTRIBUTE_TRAIL_LENGTH,	&trail_stride );

	int ctr = m_nActiveParticles/4;
	bool bHasTail = ( flMaxTail > 0.0f );
	if ( bHasTail )
	{
		while ( ctr-- )
		{
			UpdateBounds( min_x, max_x, sum_x, xyz[0] );
			UpdateBounds( min_y, max_y, sum_y, xyz[1] );
			UpdateBounds( min_z, max_z, sum_z, xyz[2] );

			fltx4 delta_x = SubSIMD( prev[0], xyz[0] );
			fltx4 delta_y = SubSIMD( prev[1], xyz[1] );
			fltx4 delta_z = SubSIMD( prev[2], xyz[2] );

			fltx4 d2_x = MulSIMD( delta_x, delta_x );
			fltx4 d2_y = MulSIMD( delta_y, delta_y );
			fltx4 d2_z = MulSIMD( delta_z, delta_z );

			fltx4 lensq = AddSIMD( d2_z, AddSIMD( d2_y, d2_x ) );
			fltx4 len = MaxSIMD( ReplicateX4( 0.001f ), SqrtSIMD( lensq ) );
			fltx4 invlen = ReciprocalSIMD( len );

			delta_x = MulSIMD( delta_x, invlen );
			delta_y = MulSIMD( delta_y, invlen );
			delta_z = MulSIMD( delta_z, invlen );

			len = MulSIMD( len, trail[0] );
			len = MulSIMD( len, oodt );
			len = MinSIMD( len, maxtail );

			delta_x = MulSIMD( delta_x, len );
			delta_y = MulSIMD( delta_y, len );
			delta_z = MulSIMD( delta_z, len );

			fltx4 tail_x = AddSIMD( xyz[0], delta_x );
			fltx4 tail_y = AddSIMD( xyz[1], delta_y );
			fltx4 tail_z = AddSIMD( xyz[2], delta_z );

			UpdateBounds( min_x, max_x, sum_x, tail_x );
			UpdateBounds( min_y, max_y, sum_y, tail_y );
			UpdateBounds( min_z, max_z, sum_z, tail_z );

			xyz += xyz_stride;
			prev += prev_stride;
			trail += trail_stride;
		}
	}
	else
	{
		while ( ctr-- )
		{
			UpdateBounds( min_x, max_x, sum_x, xyz[0] );
			UpdateBounds( min_y, max_y, sum_y, xyz[1] );
			UpdateBounds( min_z, max_z, sum_z, xyz[2] );

			xyz += xyz_stride;
		}
	}

	m_bBoundsValid = true;
	m_MinBounds.x = MIN( MIN( SubFloat( min_x, 0 ), SubFloat( min_x, 1 ) ), MIN( SubFloat( min_x, 2 ), SubFloat( min_x, 3 ) ) );
	m_MinBounds.y = MIN( MIN( SubFloat( min_y, 0 ), SubFloat( min_y, 1 ) ), MIN( SubFloat( min_y, 2 ), SubFloat( min_y, 3 ) ) );
	m_MinBounds.z = MIN( MIN( SubFloat( min_z, 0 ), SubFloat( min_z, 1 ) ), MIN( SubFloat( min_z, 2 ), SubFloat( min_z, 3 ) ) );
							  
	m_MaxBounds.x = MAX( MAX( SubFloat( max_x, 0 ), SubFloat( max_x, 1 ) ), MAX( SubFloat( max_x, 2 ), SubFloat( max_x, 3 ) ) );
	m_MaxBounds.y = MAX( MAX( SubFloat( max_y, 0 ), SubFloat( max_y, 1 ) ), MAX( SubFloat( max_y, 2 ), SubFloat( max_y, 3 ) ) );
	m_MaxBounds.z = MAX( MAX( SubFloat( max_z, 0 ), SubFloat( max_z, 1 ) ), MAX( SubFloat( max_z, 2 ), SubFloat( max_z, 3 ) ) );

	float fsum_x = SubFloat( sum_x, 0 ) + SubFloat( sum_x, 1 ) + SubFloat( sum_x, 2 ) + SubFloat( sum_x, 3 );
	float fsum_y = SubFloat( sum_y, 0 ) + SubFloat( sum_y, 1 ) + SubFloat( sum_y, 2 ) + SubFloat( sum_y, 3 );
	float fsum_z = SubFloat( sum_z, 0 ) + SubFloat( sum_z, 1 ) + SubFloat( sum_z, 2 ) + SubFloat( sum_z, 3 );

	// now, handle "tail" in a non-sse manner
	for( int i=0; i < ( m_nActiveParticles & 3 ); i++)
	{
		Vector pos( SubFloat( xyz[0], i ), SubFloat( xyz[1], i ), SubFloat( xyz[2], i ) );

		UpdateBounds( m_MinBounds.x, m_MaxBounds.x, fsum_x, pos.x );
		UpdateBounds( m_MinBounds.y, m_MaxBounds.y, fsum_y, pos.y );
		UpdateBounds( m_MinBounds.z, m_MaxBounds.z, fsum_z, pos.z );
		if ( bHasTail )

		if ( flMaxTail > 0.0f )
		{
			Vector pos_prev( SubFloat( prev[0], i ), SubFloat( prev[1], i ), SubFloat( prev[2], i ) );
			Vector dir = pos_prev - pos;
			float len = VectorNormalize( dir );
			len = MIN( MAX( len, 0.001f ) * SubFloat( trail[0], i ) * flOODt, flMaxTail );
			Vector tail = pos + dir * len;

			UpdateBounds( m_MinBounds.x, m_MaxBounds.x, fsum_x, tail.x );
			UpdateBounds( m_MinBounds.y, m_MaxBounds.y, fsum_y, tail.y );
			UpdateBounds( m_MinBounds.z, m_MaxBounds.z, fsum_z, tail.z );
		}
	}

	VectorAdd( m_MinBounds, m_pDef->m_BoundingBoxMin, m_MinBounds );
	VectorAdd( m_MaxBounds, m_pDef->m_BoundingBoxMax, m_MaxBounds );

	// calculate center
	float flOONumParticles = 1.0 / ( ( flMaxTail > 0.0f ) ? 2 * m_nActiveParticles : m_nActiveParticles );
	m_Center.x = flOONumParticles * fsum_x;
	m_Center.y = flOONumParticles * fsum_y;
	m_Center.z = flOONumParticles * fsum_z;
}


//-----------------------------------------------------------------------------
// Is the particle system finished emitting + all its particles are dead?
//-----------------------------------------------------------------------------
bool CParticleCollection::IsFinished( void ) const
{
	if ( !m_pDef )
		return true;
	if ( m_nParticleFlags & PCFLAGS_FIRST_FRAME )
		return false;
	if ( m_nActiveParticles )
		return false;
	if ( m_bDormant ) 
		return false;

	// no particles. See if any emmitters intead to create more particles
	int nEmitterCount = m_pDef->m_Emitters.Count();
	for( int i=0; i < nEmitterCount; i++ )
	{
		if ( m_pDef->m_Emitters[i]->MayCreateMoreParticles( this, m_pOperatorContextData+m_pDef->m_nEmittersCtxOffsets[i] ) )
			return false;
	}


	// make sure all children are finished
	CParticleCollection *pChild = m_Children.Head();

	for( int i = 0; pChild != NULL; pChild = pChild->m_pNext, i++ )
	{
		if ( !pChild->IsFinished() && !m_pDef->m_Children[i].m_bEndCap )
			return false;
		// return false if we're currently playing our endcap effect and not finished with it
		if ( m_pDef->m_Children[i].m_bEndCap && !pChild->IsFinished() && m_bInEndCap )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop emitting particles
//-----------------------------------------------------------------------------
void CParticleCollection::StopEmission( bool bInfiniteOnly, bool bRemoveAllParticles, bool bWakeOnStop, bool bPlayEndCap )
{
	if ( !m_pDef )
		return;

	// Whenever we call stop emission, we clear out our dormancy. This ensures we
	// get deleted if we're told to stop emission while dormant. SetDormant() ensures
	// dormancy is set to true after stopping out emission.
	m_bDormant = false;
	
	if ( bWakeOnStop )
	{
		// Set next sleep time - an additional fudge factor is added over the normal time
		// so that existing particles have a chance to go away.
		m_flNextSleepTime = MAX( m_flNextSleepTime, ( g_pParticleSystemMgr->GetLastSimulationTime() + 10 ));
	}
		 
	m_bEmissionStopped = true;

	for( int i=0; i < m_pDef->m_Emitters.Count(); i++ )
	{
		m_pDef->m_Emitters[i]->StopEmission( this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i], bInfiniteOnly );
	}

	if ( bRemoveAllParticles )
	{
		SetNActiveParticles( 0 );
	}

	// Stop our children as well
	if ( bPlayEndCap )
	{
		CParticleCollection *pChild;
		int i;
		m_bInEndCap = true;
		for( i = 0, pChild = m_Children.m_pHead; pChild != NULL; pChild = pChild->m_pNext, i++ )
		{
			pChild->m_bInEndCap = true;
			if ( m_pDef->m_Children[i].m_bEndCap )
			{
				pChild->m_flCurTime = 0.0f;
				pChild->StartEmission( bInfiniteOnly );
			}
			else
				pChild->StopEmission( bInfiniteOnly, bRemoveAllParticles, bWakeOnStop, bPlayEndCap );
		}
	}
	else
	{
		for( CParticleCollection *p = m_Children.m_pHead; p; p = p->m_pNext )
		{
			p->StopEmission( bInfiniteOnly, bRemoveAllParticles );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Stop emitting particles
//-----------------------------------------------------------------------------
void CParticleCollection::StartEmission( bool bInfiniteOnly )
{
	if ( !m_pDef )
		return;
	
	m_bEmissionStopped = false;

	for( int i=0; i < m_pDef->m_Emitters.Count(); i++ )
	{
		m_pDef->m_Emitters[i]->StartEmission( this, m_pOperatorContextData + m_pDef->m_nEmittersCtxOffsets[i], bInfiniteOnly );
	}

	// Start our children as well
	CParticleCollection *pChild = m_Children.Head();
	for( int i = 0; pChild != NULL; pChild = pChild->m_pNext, i++ )
	{
		// Don't start End Cap Effects - these only play when stopping emission.
		if ( !m_pDef->m_Children[i].m_bEndCap )
		{
			pChild->StartEmission( bInfiniteOnly );
		}
	}

	// Set our sleep time to some time in the future so we update again
	m_flNextSleepTime = g_pParticleSystemMgr->GetLastSimulationTime() + m_pDef->m_flNoDrawTimeToGoToSleep;
}

//-----------------------------------------------------------------------------
// Purpose: Dormant particle systems simulate their particles, but don't emit 
//			new ones. Unlike particle systems that have StopEmission() called
//			dormant particle systems don't remove themselves when they're
//			out of particles, assuming they'll return at some point.
//-----------------------------------------------------------------------------
void CParticleCollection::SetDormant( bool bDormant )
{
	// Don't stop or start emission if we are not changing dormancy state
	if ( bDormant == m_bDormant )
		return;

	// If emissions have already been stopped, don't go dormant, we're supposed to be dying.
	if ( m_bEmissionStopped && bDormant )
		return;

	if ( bDormant )
	{
		StopEmission();
		m_bQueuedStartEmission = false;
	}
	else
	{
		//StartEmission();
		m_bQueuedStartEmission = true;						// start emission during next sim step
		// Set our sleep time to some time in the future so we update again
		m_flNextSleepTime = g_pParticleSystemMgr->GetLastSimulationTime() + m_pDef->m_flNoDrawTimeToGoToSleep;
	}

	m_bDormant = bDormant;
}

bool CParticleCollection::IsEmitting() const
{
	return !m_bEmissionStopped;
}

void CParticleAttributeAddressTable::CopyParticleAttributes( int nSrcIndex, int nDestIndex ) const
{
	for( int p = 0; p < ARRAYSIZE( m_pAttributes ); p++ )
	{
		switch( m_nFloatStrides[p] )
		{
			case 4:										// move a float
				m_pAttributes[p][nDestIndex] = m_pAttributes[p][nSrcIndex];
				break;

			case 12:										// move a vec3
			{
				// sse weirdness
				int oldidxsse = 12 * ( nDestIndex >> 2 );
				int oldofs = oldidxsse + ( nDestIndex & 3 );
				int lastidxsse = 12 * ( nSrcIndex >> 2 );
				int lastofs = lastidxsse + ( nSrcIndex & 3 );
				
				m_pAttributes[p][oldofs] = m_pAttributes[p][lastofs];
				m_pAttributes[p][4 + oldofs] = m_pAttributes[p][4 + lastofs];
				m_pAttributes[p][8 + oldofs] = m_pAttributes[p][8 + lastofs];
				break;
			}
		}
	}
}

void CParticleCollection::MoveParticle( int nInitialIndex, int nNewIndex )
{
	// Copy the per-particle attributes
	m_ParticleAttributes.CopyParticleAttributes( nInitialIndex, nNewIndex );
	m_ParticleInitialAttributes.CopyParticleAttributes( nInitialIndex, nNewIndex );
	if ( m_pPreviousAttributeMemory )
	{
		m_PreviousFrameAttributes.CopyParticleAttributes( nInitialIndex, nNewIndex );
	}
}


//-----------------------------------------------------------------------------
// Kill List processing.
//-----------------------------------------------------------------------------

#define THREADED_PARTICLES 1

#if THREADED_PARTICLES
#define MAX_SIMULTANEOUS_KILL_LISTS 16
static volatile int g_nKillBufferInUse[MAX_SIMULTANEOUS_KILL_LISTS];
static KillListItem_t *g_pKillBuffers[MAX_SIMULTANEOUS_KILL_LISTS];

void CParticleSystemMgr::DetachKillList( CParticleCollection *pParticles )
{
	if ( pParticles->m_pParticleKillList )
	{
		// find which it is
		for(int i=0; i < NELEMS( g_pKillBuffers ); i++)
		{
			if ( g_pKillBuffers[i] == pParticles->m_pParticleKillList )
			{
				pParticles->m_pParticleKillList = NULL;
				g_nKillBufferInUse[i] = 0;					// no need to interlock
				return;
			}
		}
		Assert( 0 );										// how did we get here?
	}
}

void CParticleSystemMgr::AttachKillList( CParticleCollection *pParticles )
{
	// look for a free slot
	for(;;)
	{
		for(int i=0; i < NELEMS( g_nKillBufferInUse ); i++)
		{
			if ( ! g_nKillBufferInUse[i] )					// available?
			{
				// try to take it!
				if ( ThreadInterlockedAssignIf( &( g_nKillBufferInUse[i]), 1, 0 ) )
				{
					if ( ! g_pKillBuffers[i] )
					{
						g_pKillBuffers[i] = new KillListItem_t[MAX_PARTICLES_IN_A_SYSTEM];
					}
					pParticles->m_pParticleKillList = g_pKillBuffers[i];
					return;									// done!
				}

			}
		}
		Assert(0);											// why don't we have enough buffers?
		ThreadSleep();
	}
}
#else
// use one static kill list. no worries because of not threading
static KillListItem_t g_nParticleKillList[MAX_PARTICLES_IN_A_SYSTEM];
void CParticleSystemMgr::AttachKillList( CParticleCollection *pParticles )
{
	pParticles->m_pParticleKillList = g_nParticleKillList;
}
void CParticleCollection::DetachKillList( CParticleCollection *pParticles )
{
	Assert( pParticles->m_nNumParticlesToKill == 0 );
	pParticles->m_pParticleKillList = NULL;
}
#endif


void CParticleCollection::ApplyKillList( void )
{
	// first, kill particles past bounds
	const KillListItem_t *pCurKillListSlot = m_pParticleKillList;
	while( m_nNumParticlesToKill && pCurKillListSlot[ m_nNumParticlesToKill-1 ].nIndex >= (uint)m_nActiveParticles )
	{
		m_nNumParticlesToKill--;
	}
	Assert( m_nNumParticlesToKill <= m_nActiveParticles );

	if ( m_nNumParticlesToKill == 0 )
		return;

	// next, run any child system emitter/initializer operators which request the parent's kill list:
	bool bApplyingParentKillList = true;
	for( CParticleCollection *pChild = m_Children.m_pHead; pChild != NULL; pChild = pChild->m_pNext )
	{
		// TODO: make this more general (there's a bunch of "first frame" and "frame-to-frame" setup that happens in Simulate() which is skipped here)
		EmitAndInit( pChild, bApplyingParentKillList );
	}

	// now, execute kill list (NOTE: this algorithm assumes the particles listed
	// in the kill list are in ascending order - this is checked in KillParticle)
	unsigned int nParticlesActiveNow = m_nActiveParticles;
	int nLeftInKillList = m_nNumParticlesToKill;
	if ( nLeftInKillList == m_nActiveParticles )
	{
		nParticlesActiveNow = 0; // Simply discard all particles
	}
	// TODO: check KILL_LIST_FLAG_DONT_KILL here (take no action for those kill list entries)
	else if ( IsOrderImportant() )
	{
		// shift
		m_pParticleKillList[ nLeftInKillList ].nIndex = m_nActiveParticles;
		for ( int nKilled = 0; nKilled < nLeftInKillList; )
		{
			int nWriteIndex      = m_pParticleKillList[ nKilled ].nIndex - nKilled;
			nKilled++;
			int nNextIndexToKill = m_pParticleKillList[ nKilled ].nIndex - nKilled;
			for ( nWriteIndex; nWriteIndex < nNextIndexToKill; nWriteIndex++ )
			{
				MoveParticle( ( nWriteIndex + nKilled ), nWriteIndex );
			}
		}
		nParticlesActiveNow -= nLeftInKillList;
	}
	else
	{
		while( nLeftInKillList )
		{
			unsigned int nKillIndex = (pCurKillListSlot++)->nIndex;
			nLeftInKillList--;

			// now, we will move a particle from the end to where we are
			// first, we have to find the last particle (which is not in the kill list)
			while ( nLeftInKillList &&
				( pCurKillListSlot[ nLeftInKillList-1 ].nIndex == nParticlesActiveNow-1 ))
			{
				nLeftInKillList--;
				nParticlesActiveNow--;
			}

			// we might be killing the last particle
			if ( nKillIndex == nParticlesActiveNow-1 )
			{
				// killing last one
				nParticlesActiveNow--;
				break;											// we are done
			}

			// move the last particle to this one and chop off the end of the list
			MoveParticle( nParticlesActiveNow-1, nKillIndex );
			nParticlesActiveNow--;
		}
	}

	// set count in system and wipe kill list
	SetNActiveParticles( nParticlesActiveNow );
	m_nNumParticlesToKill = 0;
}

void CParticleCollection::CalculatePathValues( CPathParameters const &PathIn,
											   float flTimeStamp,
											   Vector *pStartPnt,
											   Vector *pMidPnt,
											   Vector *pEndPnt
											   )
{
	Vector StartPnt;
	GetControlPointAtTime( PathIn.m_nStartControlPointNumber, flTimeStamp, &StartPnt );
	Vector EndPnt;
	GetControlPointAtTime( PathIn.m_nEndControlPointNumber, flTimeStamp, &EndPnt );
	
	Vector MidP;
	VectorLerp(StartPnt, EndPnt, PathIn.m_flMidPoint, MidP);

	if ( PathIn.m_nBulgeControl )
	{
		Vector vTarget=(EndPnt-StartPnt);
		float flBulgeScale = 0.0;
		int nCP=PathIn.m_nStartControlPointNumber;
		if ( PathIn.m_nBulgeControl == 2)
			nCP = PathIn.m_nEndControlPointNumber;
		Vector Fwd = ControlPoint( nCP ).m_ForwardVector;
		float len=VectorLength( vTarget);
		if ( len > 1.0e-6 )
		{
			vTarget *= (1.0/len);						// normalize
			flBulgeScale = 1.0-fabs( DotProduct( vTarget, Fwd )); // bulge inversely scaled
		}
		Vector Potential_MidP=Fwd;
		float flOffsetDist = VectorLength( Potential_MidP );
		if ( flOffsetDist > 1.0e-6 )
		{
			Potential_MidP *= (PathIn.m_flBulge*len*flBulgeScale)/flOffsetDist;
			MidP += Potential_MidP;
		}
	}
	else
	{
		Vector RndVector;
		RandomVector( 0, -PathIn.m_flBulge, PathIn.m_flBulge, &RndVector);
		MidP+=RndVector;
	}
	
	*pStartPnt = StartPnt;
	*pMidPnt = MidP;
	*pEndPnt = EndPnt;
}


//-----------------------------------------------------------------------------
//
// Default impelemtation of the query
//
//-----------------------------------------------------------------------------

class CDefaultParticleSystemQuery : public CBaseAppSystem< IParticleSystemQuery >
{
public:
	virtual bool IsEditor( ) { return false; }

	virtual void GetLightingAtPoint( const Vector& vecOrigin, Color &tint )
	{
		tint.SetColor( 255, 255, 255, 255 );
	}
	virtual void TraceLine( const Vector& vecAbsStart,
							const Vector& vecAbsEnd, unsigned int mask, 
							const class IHandleEntity *ignore,
							int collisionGroup, CBaseTrace *ptr )
	{
		ptr->fraction = 1.0;								// no hit
	}

	virtual bool IsPointInSolid( const Vector& vecPos, const int nContentsMask )
	{
		return false;
	}

	virtual void GetRandomPointsOnControllingObjectHitBox( 
		CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsOut,
		float flBBoxScale,
		int nNumTrysToGetAPointInsideTheModel,
		Vector *pPntsOut,
		Vector vecDirectionBias,
		Vector *pHitBoxRelativeCoordOut, int *pHitBoxIndexOut, 	int nDesiredHitbox,
		const char *pszHitboxSetName ) 
	{
		for ( int i = 0; i < nNumPtsOut; ++i )
		{
			pPntsOut[i].Init();
		}
	}

	virtual void GetClosestControllingObjectHitBox( CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsIn,
		float flBBoxScale,
		Vector *pPntsIn,
		Vector *pHitBoxRelativeCoordOut,
		int *pHitBoxIndexOut,
		int nDesiredHitbox, 
		const char *pszHitboxSetName )
	{
		for ( int i=0; i < nNumPtsIn; i++ )
		{
			if ( pHitBoxIndexOut )
				pHitBoxIndexOut[i] = 0;

			if ( pHitBoxRelativeCoordOut )
				pHitBoxRelativeCoordOut[i].Init();
		}
	}

	virtual void TraceAgainstRayTraceEnv( 
		int envnumber,  
		const FourRays &rays, fltx4 TMin, fltx4 TMax,
		RayTracingResult *rslt_out, int32 skip_id ) const
	{
		rslt_out->HitDistance = Four_Ones;
		rslt_out->surface_normal.DuplicateVector( vec3_origin );
	}

	virtual Vector GetCurrentViewOrigin()
	{
		return vec3_origin;
	}

	virtual int GetActivityCount() { return 0; }

	virtual const char *GetActivityNameFromIndex( int nActivityIndex ) { return 0; }
	virtual int GetActivityNumber( void *pModel, const char *m_pszActivityName ) { return -1; }

	virtual float GetPixelVisibility( int *pQueryHandle, const Vector &vecOrigin, float flScale ) { return 0.0f; }

	virtual void PreSimulate( ) { }

	virtual void PostSimulate( ) { }

	virtual void DebugDrawLine( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float duration )
	{
	}

	virtual void DrawModel( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber, int nBodyPart, int nSubModel,
		int nSkin, int nAnimationSequence = 0, float flAnimationRate = 30.0f, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f ) { }

	virtual void UpdateProjectedTexture( const int nParticleID, IMaterial *pMaterial, Vector &vOrigin, float flRadius, float flRotation, float r, float g, float b, float a, void *&pUserVar ) { }
};

static CDefaultParticleSystemQuery s_DefaultParticleSystemQuery;


//-----------------------------------------------------------------------------
//
// Particle system manager
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CParticleSystemMgr::CParticleSystemMgr()
	// m_SheetList( DefLessFunc( ITexture * ) )
{
	m_pQuery = &s_DefaultParticleSystemQuery;
	m_bDidInit = false;
	m_bUsingDefaultQuery = true;
	m_bShouldLoadSheets = true;
	m_bAllowPrecache = true;
	m_pParticleSystemDictionary = NULL;
	m_nNumFramesMeasured = 0;
	m_flLastSimulationTime = 0.0f;
	m_flLastSimulationDuration = 0.0f;
	m_pShadowDepthMaterial = NULL;

	// Init the attribute table
	InitAttributeTable();
}

CParticleSystemMgr::~CParticleSystemMgr()
{
	FlushAllSheets();
	if ( m_pParticleSystemDictionary )
	{
		delete m_pParticleSystemDictionary;
		m_pParticleSystemDictionary = NULL;
	}
}


//-----------------------------------------------------------------------------
// Initialize the particle system
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::Init( IParticleSystemQuery *pQuery, bool bAllowPrecache )
{
	if ( g_pMaterialSystem && ( !g_pMaterialSystem->QueryInterface( MATERIAL_SYSTEM_INTERFACE_VERSION ) ) )
	{
		Msg( "CParticleSystemMgr compiled using an old IMaterialSystem\n" );
		return false;
	}

	if ( m_bUsingDefaultQuery && pQuery )
	{
		m_pQuery = pQuery;
		m_bUsingDefaultQuery = false;
	}

	if ( !m_bDidInit )
	{
		m_pParticleSystemDictionary = new CParticleSystemDictionary;
		// NOTE: This is for the editor only
		AddParticleOperator( FUNCTION_CHILDREN, &s_ChildOperatorDefinition );

		if ( g_pMaterialSystem )
		{
			MEM_ALLOC_CREDIT();
			KeyValues *pVMTKeyValues = new KeyValues( "DepthWrite" );
			pVMTKeyValues->SetInt( "$no_fullbright", 1 );
			pVMTKeyValues->SetInt( "$model", 0 );
			pVMTKeyValues->SetInt( "$alphatest", 0 );
			m_pShadowDepthMaterial = g_pMaterialSystem->CreateMaterial( "__particlesDepthWrite", pVMTKeyValues );
		}
		SeedRandSIMD( 12345678 );
		m_bDidInit = true;
	}

	m_bAllowPrecache = bAllowPrecache;

	return true;
}

//-----------------------------------------------------------------------------
void CParticleSystemMgr::Shutdown()
{
	if ( m_pShadowDepthMaterial )
	{
		m_pShadowDepthMaterial->Release();
		m_pShadowDepthMaterial = NULL;
	}
}

//-----------------------------------------------------------------------------
// Init the attribute table
//-----------------------------------------------------------------------------
void CParticleSystemMgr::InitAttributeTable( void )
{
	// Init the attribute table
#define INITPARTICLE_ATTRIBUTE( name )												\
	{																				\
		int bit = PARTICLE_ATTRIBUTE_##name;										\
		s_AttributeTable[ bit ].nDataType = PARTICLE_ATTRIBUTE_##name##_DATATYPE;	\
		s_AttributeTable[ bit ].pName = #name;										\
	}

	memset( s_AttributeTable, 0, sizeof( s_AttributeTable ) );

	INITPARTICLE_ATTRIBUTE( XYZ );
	INITPARTICLE_ATTRIBUTE( LIFE_DURATION );
	INITPARTICLE_ATTRIBUTE( PREV_XYZ );
	INITPARTICLE_ATTRIBUTE( RADIUS );
	INITPARTICLE_ATTRIBUTE( ROTATION );
	INITPARTICLE_ATTRIBUTE( ROTATION_SPEED );
	INITPARTICLE_ATTRIBUTE( TINT_RGB );
	INITPARTICLE_ATTRIBUTE( ALPHA );
	INITPARTICLE_ATTRIBUTE( CREATION_TIME );
	INITPARTICLE_ATTRIBUTE( SEQUENCE_NUMBER );
	INITPARTICLE_ATTRIBUTE( TRAIL_LENGTH );
	INITPARTICLE_ATTRIBUTE( PARTICLE_ID );
	INITPARTICLE_ATTRIBUTE( YAW );
	INITPARTICLE_ATTRIBUTE( SEQUENCE_NUMBER1 );
	INITPARTICLE_ATTRIBUTE( HITBOX_INDEX );
	INITPARTICLE_ATTRIBUTE( HITBOX_RELATIVE_XYZ );
	INITPARTICLE_ATTRIBUTE( ALPHA2 );
	INITPARTICLE_ATTRIBUTE( SCRATCH_VEC );
	INITPARTICLE_ATTRIBUTE( SCRATCH_FLOAT );
	INITPARTICLE_ATTRIBUTE( UNUSED );
	INITPARTICLE_ATTRIBUTE( PITCH );
	INITPARTICLE_ATTRIBUTE( NORMAL );
	INITPARTICLE_ATTRIBUTE( GLOW_RGB );
	INITPARTICLE_ATTRIBUTE( GLOW_ALPHA );

	for ( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; i++ )
	{
		if ( !s_AttributeTable[ i ].pName )
		{
			// The above list of initializers needs updating!
			Warning( "CParticleSystemMgr::InitAttributeTable has an out-of-date attribute list! (element %d not set up)\n", i );
			Assert( 0 );
		}
	}
}

//----------------------------------------------------------------------------------
// String -> Attribute mapping
//----------------------------------------------------------------------------------
int CParticleSystemMgr::GetParticleAttributeByName( const char *pName ) const
{
	// TODO: OPTIMIZATION: use Chris's CUtlStringToken class here to speed this up
	for ( int i = 0; i < MAX_PARTICLE_ATTRIBUTES; i++ )
	{
		if ( !Q_stricmp( pName, s_AttributeTable[ i ].pName ) )
			return i;
	}
	return -1;
}

//----------------------------------------------------------------------------------
// Attribute -> String mapping
//----------------------------------------------------------------------------------
const char *CParticleSystemMgr::GetParticleAttributeName( int nAttribute ) const
{
	if ( ( nAttribute < 0 ) || ( nAttribute >= MAX_PARTICLE_ATTRIBUTES ) )
	{
		Assert( 0 );
		return "unknown";
	}
	return s_AttributeTable[ nAttribute ].pName;
}

//----------------------------------------------------------------------------------
// Get the data type of a given attribute
//----------------------------------------------------------------------------------
EAttributeDataType CParticleSystemMgr::GetParticleAttributeDataType( int nAttribute ) const
{
	Assert( nAttribute >= 0 );
	Assert( nAttribute < MAX_PARTICLE_ATTRIBUTES );
	return s_AttributeTable[ nAttribute ].nDataType;
}

//----------------------------------------------------------------------------------
// Cache/uncache materials used by particle systems
//----------------------------------------------------------------------------------
void CParticleSystemMgr::PrecacheParticleSystem( int nStringNumber, const char *pName )
{
	if ( !pName || !pName[0] )
	{
		return;
	}

	ParticleSystemHandle_t hParticleSystem = GetParticleSystemIndex( pName );

	// Used to display an error system if the requested one isn't known from the manifest
	if ( hParticleSystem == UTL_INVAL_SYMBOL )
	{
		Warning( "Attempted to precache unknown particle system \"%s\"!\n", pName );
		hParticleSystem = GetParticleSystemIndex( "error" );
	}

	CParticleSystemDefinition* pDef = FindParticleSystem( hParticleSystem );

	CUtlVector< ParticleSystemHandle_t > &lookup = ( nStringNumber >= 0 ) ? m_PrecacheLookup : m_ClientPrecacheLookup;
	if ( nStringNumber < 0 )
	{
		nStringNumber = - nStringNumber - 1;
	}
	int nCountToAdd = nStringNumber + 1 - lookup.Count();
	for ( int i = 0; i < nCountToAdd; ++i )
	{
		lookup.AddToTail( UTL_INVAL_SYMBOL );
	}
	lookup[ nStringNumber ] = hParticleSystem;

	if ( !pDef )
	{
		Warning( "Attempted to precache unknown particle system \"%s\"!\n", pName );
		return;
	}

	pDef->Precache();
}

void CParticleSystemMgr::LevelShutdown( void )
{
#ifndef SERVER_PARTICLE_LIB
//	InvalidateGlobalCollisionCache();						// keep the collision cahce out of the server binary for now
#endif
}

void CParticleSystemMgr::UncacheAllParticleSystems()
{
	m_PrecacheLookup.RemoveAll();
	m_ClientPrecacheLookup.RemoveAll();

	if ( m_pParticleSystemDictionary )
	{
		int nCount = m_pParticleSystemDictionary->Count();
		for ( int i = 0; i < nCount; ++i )
		{
			m_pParticleSystemDictionary->GetParticleSystem( i )->Uncache();
		}

		nCount = m_pParticleSystemDictionary->NameCount();
		for ( ParticleSystemHandle_t h = 0; h < nCount; ++h )
		{
			m_pParticleSystemDictionary->FindParticleSystem( h )->Uncache();
		}
	}

	// Flush sheets, as they can accumulate several MB of memory per map
	FlushAllSheets();
}


//-----------------------------------------------------------------------------
// return the particle field name
//-----------------------------------------------------------------------------
static const char *s_pParticleFieldNames[MAX_PARTICLE_ATTRIBUTES] = 
{
	"Position",			// XYZ, 0
	"Life Duration",	// LIFE_DURATION, 1 );
	"Position Previous",// PREV_XYZ 
	"Radius",			// RADIUS, 3 );

	"Roll",				// ROTATION, 4 );
	"Roll Speed",		// ROTATION_SPEED, 5 );
	"Color",			// TINT_RGB, 6 );
	"Alpha",			// ALPHA, 7 );

	"Creation Time",	// CREATION_TIME, 8 );
	"Sequence Number",	// SEQUENCE_NUMBER, 9 );
	"Trail Length",		// TRAIL_LENGTH, 10 );
	"Particle ID",		// PARTICLE_ID, 11 ); 

	"Yaw",				// YAW, 12 );
	"Sequence Number 1",// SEQUENCE_NUMBER1, 13 );
	"Hitbox Index",		// HITBOX_INDEX, 14
	"Hitbox Offset Position",	// HITBOX_XYZ_RELATIVE 15

	"Alpha Alternate",	// ALPHA2, 16
	"Scratch Vector",	// SCRATCH_VEC 17
	"Scratch Float",	// SCRATCH_FLOAT 18
	NULL,

	"Pitch",			// PITCH, 20
	"Normal",			// NORMAL, 21
	"Glow RGB",			//GLOW_RGB,22
	"Glow Alpha",		//GLOW_ALPHA,23
};

const char* CParticleSystemMgr::GetParticleFieldName( int nParticleField ) const
{
	return s_pParticleFieldNames[nParticleField];
}

//-----------------------------------------------------------------------------
// Returns the available particle operators
//-----------------------------------------------------------------------------
void CParticleSystemMgr::AddParticleOperator( ParticleFunctionType_t nOpType,
											 IParticleOperatorDefinition *pOpFactory )
{
	m_ParticleOperators[nOpType].AddToTail( pOpFactory );
}


static const char *s_pFilterNames[ ] = 
{
	"All",
	"Position and Velocity",
	"Life Duration",
	"Parameter Remapping",
	"Rotation",
	"Size",
	"Color and Opacity",
	"Animation Sequence",
	"Hitbox",
	"Normal",
	"Control Points"
};

const char *CParticleSystemMgr::GetFilterName( ParticleFilterType_t nFilterType ) const
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_pFilterNames ) == FILTER_COUNT );
	return s_pFilterNames[nFilterType];
}

CUtlVector< IParticleOperatorDefinition *> &CParticleSystemMgr::GetAvailableParticleOperatorList( ParticleFunctionType_t nWhichList )
{
	return m_ParticleOperators[nWhichList];
}

const DmxElementUnpackStructure_t *CParticleSystemMgr::GetParticleSystemDefinitionUnpackStructure()
{
	return s_pParticleSystemDefinitionUnpack;
}


//------------------------------------------------------------------------------
// custom allocators for operators so simd aligned
//------------------------------------------------------------------------------
#include "tier0/memdbgoff.h"
void *CParticleOperatorInstance::operator new( size_t nSize )
{
	return MemAlloc_AllocAligned( nSize, 16 );
}

void* CParticleOperatorInstance::operator new( size_t nSize, int nBlockUse, const char *pFileName, int nLine )
{
	return MemAlloc_AllocAlignedFileLine( nSize, 16, pFileName, nLine );
}

void CParticleOperatorInstance::operator delete(void *pData)
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

void CParticleOperatorInstance::operator delete( void* pData, int nBlockUse, const char *pFileName, int nLine )
{
	if ( pData )
	{
		MemAlloc_FreeAligned( pData );
	}
}

#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Load a PCF file and list the particle systems in it
//-----------------------------------------------------------------------------
void CParticleSystemMgr::GetParticleSystemsInFile( const char *pFileName, CUtlVector< CUtlString > *pOutSystemNameList )
{
	if( pOutSystemNameList == NULL ) 
		return;

	pOutSystemNameList->RemoveAll();

	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( pFileName, "GAME", buf ) )
	{
		return;
	}

	GetParticleSystemsInBuffer( buf, pOutSystemNameList );
}

void CParticleSystemMgr::GetParticleSystemsInBuffer( CUtlBuffer &buf, CUtlVector<CUtlString> *pOutSystemNameList )
{
	if( pOutSystemNameList == NULL ) 
		return;

	pOutSystemNameList->RemoveAll();

	DECLARE_DMX_CONTEXT( );

	CDmxElement *pRoot;
	if ( !UnserializeDMX( buf, &pRoot ) )
	{
		return;
	}

	if ( !Q_stricmp( pRoot->GetTypeString(), "DmeParticleSystemDefinition" ) )
	{
		pOutSystemNameList->AddToTail( pRoot->GetName() );
		CleanupDMX( pRoot );
		return;
	}

	const CDmxAttribute *pDefinitions = pRoot->GetAttribute( "particleSystemDefinitions" );
	if ( !pDefinitions || pDefinitions->GetType() != AT_ELEMENT_ARRAY )
	{
		CleanupDMX( pRoot );
		return;
	}

	const CUtlVector< CDmxElement* >& definitions = pDefinitions->GetArray<CDmxElement*>( );
	int nCount = definitions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		pOutSystemNameList->AddToTail( definitions[i]->GetName() );
	}

	CleanupDMX( pRoot );
}


//-----------------------------------------------------------------------------
// Read the particle config file from a utlbuffer
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::ReadParticleDefinitions( CUtlBuffer &buf, const char *pFileName, bool bPrecache, bool bDecommitTempMemory )
{
	DECLARE_DMX_CONTEXT_DECOMMIT( bDecommitTempMemory );

	CDmxElement *pRoot;
	if ( !UnserializeDMX( buf, &pRoot, pFileName ) )
	{
		Warning( "Unable to read particle definition %s! UtlBuffer is probably the wrong type!\n", pFileName );
		return false;
	}

	if ( !Q_stricmp( pRoot->GetTypeString(), "DmeParticleSystemDefinition" ) )
	{
		CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->AddParticleSystem( pRoot );
		if ( pDef && bPrecache )
		{
			pDef->m_bAlwaysPrecache = true;
			if ( m_bAllowPrecache )
			{
				pDef->Precache();
			}
		}
		CleanupDMX( pRoot );
		return true;
	}

	const CDmxAttribute *pDefinitions = pRoot->GetAttribute( "particleSystemDefinitions" );
	if ( !pDefinitions || pDefinitions->GetType() != AT_ELEMENT_ARRAY )
	{
		CleanupDMX( pRoot );
		return false;
	}

	const CUtlVector< CDmxElement* >& definitions = pDefinitions->GetArray<CDmxElement*>( );
	int nCount = definitions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->AddParticleSystem( definitions[i] );
		if ( pDef && bPrecache )
		{
			pDef->m_bAlwaysPrecache = true;
			if ( m_bAllowPrecache )
			{
				pDef->Precache();
			}
		}

	}

	CleanupDMX( pRoot );
	return true;
}


//-----------------------------------------------------------------------------
// Decommits temporary memory
//-----------------------------------------------------------------------------
void CParticleSystemMgr::DecommitTempMemory()
{
	DecommitDMXMemory();
}


//-----------------------------------------------------------------------------
// Sets the last simulation time, used for particle system sleeping logic
//-----------------------------------------------------------------------------
void CParticleSystemMgr::SetLastSimulationTime( float flTime )
{
	m_flLastSimulationTime = flTime;
}

float CParticleSystemMgr::GetLastSimulationTime() const
{
	return m_flLastSimulationTime;
}

//-----------------------------------------------------------------------------
// Sets the last simulation duration ( the amount of time we spent simulating particle ) last frame
// Used to fallback to cheaper particle systems under load
//-----------------------------------------------------------------------------
void CParticleSystemMgr::SetLastSimulationDuration( float flDuration )
{
	m_flLastSimulationDuration = flDuration;
}

float CParticleSystemMgr::GetLastSimulationDuration() const
{
	return m_flLastSimulationDuration;
}

//-----------------------------------------------------------------------------
// GPU/CPU Level
//-----------------------------------------------------------------------------
void CParticleSystemMgr::SetSystemLevel( int nCPULevel, int nGPULevel )
{
	m_nCPULevel = nCPULevel;
	m_nGPULevel = nGPULevel;
}

int CParticleSystemMgr::GetParticleCPULevel() const
{
	return m_nCPULevel;
}

int CParticleSystemMgr::GetParticleGPULevel() const
{
	return m_nGPULevel;
}


//-----------------------------------------------------------------------------
// Fallback paramters
//-----------------------------------------------------------------------------
void CParticleSystemMgr::SetFallbackParameters( float flBase, float flMultiplier, float flSimFallbackBaseMultiplier, float flSimThresholdMs )
{
	m_flFallbackBase = flBase;
	m_flFallbackMultiplier = flMultiplier;
	m_flSimFallbackBaseMultiplier = flSimFallbackBaseMultiplier;
	m_flSimThresholdMs = flSimThresholdMs;
}

float CParticleSystemMgr::GetFallbackBase() const
{
	return m_flFallbackBase;
}

float CParticleSystemMgr::GetFallbackMultiplier() const
{
	return m_flFallbackMultiplier;
}

float CParticleSystemMgr::GetSimFallbackThresholdMs() const
{
	return m_flSimThresholdMs;
}

float CParticleSystemMgr::GetSimFallbackBaseMultiplier() const
{
	return m_flSimFallbackBaseMultiplier;
}

//-----------------------------------------------------------------------------
// Unserialization-related methods
//-----------------------------------------------------------------------------
void CParticleSystemMgr::AddParticleSystem( CDmxElement *pParticleSystem )
{
	m_pParticleSystemDictionary->AddParticleSystem( pParticleSystem );
}

CParticleSystemDefinition* CParticleSystemMgr::FindParticleSystem( const char *pName )
{
	return m_pParticleSystemDictionary->FindParticleSystem( pName );
}

CParticleSystemDefinition* CParticleSystemMgr::FindParticleSystem( const DmObjectId_t& id )
{
	return m_pParticleSystemDictionary->FindParticleSystem( id );
}

CParticleSystemDefinition* CParticleSystemMgr::FindParticleSystem( ParticleSystemHandle_t hParticleSystem )
{
	return m_pParticleSystemDictionary->FindParticleSystem( hParticleSystem );
}

CParticleSystemDefinition* CParticleSystemMgr::FindPrecachedParticleSystem( int nPrecacheIndex )
{
	CUtlVector< ParticleSystemHandle_t > &lookup = ( nPrecacheIndex >= 0 ) ? m_PrecacheLookup : m_ClientPrecacheLookup;
	if ( nPrecacheIndex < 0 )
	{
		nPrecacheIndex = - nPrecacheIndex - 1;
	}

	if ( nPrecacheIndex >= lookup.Count() )
		return NULL;
	return FindParticleSystem( lookup[nPrecacheIndex] );
}

//-----------------------------------------------------------------------------
// Read the particle config file from a utlbuffer
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::ReadParticleConfigFile( CUtlBuffer &buf, bool bPrecache, bool bDecommitTempMemory, const char *pFileName )
{
	return ReadParticleDefinitions( buf, pFileName, bPrecache, bDecommitTempMemory );
}


//-----------------------------------------------------------------------------
// Read the particle config file from a utlbuffer
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::ReadParticleConfigFile( const char *pFileName, bool bPrecache, bool bDecommitTempMemory )
{
	// Names starting with a '!' are always precached.
	if ( pFileName[0] == '!' )
	{
		bPrecache = true;
		++pFileName;
	}

	if ( PLATFORM_EXT[0] )
	{
		char szTargetName[MAX_PATH];
		CreatePlatformFilename( pFileName, szTargetName, sizeof( szTargetName ) );

		CUtlBuffer fileBuffer;
		bool bHaveParticles = g_pFullFileSystem->ReadFile( szTargetName, "GAME", fileBuffer );
		if ( bHaveParticles )
		{			
			fileBuffer.SetBigEndian( false );
			return ReadParticleConfigFile( fileBuffer, bPrecache, bDecommitTempMemory, szTargetName );
		}
		else
		{
			// 360/PS3 version should have been there, 360/PS3 zips can only have binary particles
			Warning( "Particles: Missing '%s'\n", szTargetName );
			return false;
		}
	}

//	char pFallbackBuf[MAX_PATH];
	if ( IsPC() )
	{
		// Look for fallback particle systems
		char pTemp[MAX_PATH];
		Q_StripExtension( pFileName, pTemp, sizeof(pTemp) );
		const char *pExt = Q_GetFileExtension( pFileName );
		if ( !pExt )
		{
			pExt = "pcf";
		}
		
		/*
		// FIXME: Hook GPU level and/or CPU level into fallbacks instead of dxsupport level
		if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() < 90 )
		{
			Q_snprintf( pFallbackBuf, sizeof(pFallbackBuf), "%s_dx80.%s", pTemp, pExt );
			if ( g_pFullFileSystem->FileExists( pFallbackBuf ) )
			{
				pFileName = pFallbackBuf;
			}
		}
		else if ( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() == 90 &&  g_pMaterialSystemHardwareConfig->PreferReducedFillrate() )
		{
			Q_snprintf( pFallbackBuf, sizeof(pFallbackBuf), "%s_dx90_slow.%s", pTemp, pExt );
			if ( g_pFullFileSystem->FileExists( pFallbackBuf ) )
			{
				pFileName = pFallbackBuf;
			}
		}
		*/
	}

	CUtlBuffer buf( 0, 0, 0 );
	if ( IsX360() || IsPS3() )
	{
		// fell through, load as pc particle resource file
		buf.ActivateByteSwapping( true );
	}

	if ( g_pFullFileSystem->ReadFile( pFileName, "GAME", buf ) )
	{
		return ReadParticleConfigFile( buf, bPrecache, bDecommitTempMemory, pFileName );
	}

	Warning( "Particles: Missing '%s'\n", pFileName );
	return false;
}


//-----------------------------------------------------------------------------
// Write a specific particle config to a utlbuffer
//-----------------------------------------------------------------------------
bool CParticleSystemMgr::WriteParticleConfigFile( const char *pParticleSystemName, CUtlBuffer &buf, bool bPreventNameBasedLookup )
{
	DECLARE_DMX_CONTEXT();
	// Create DMX elements representing the particle system definition
	CDmxElement *pParticleSystem = CreateParticleDmxElement( pParticleSystemName );
	return WriteParticleConfigFile( pParticleSystem, buf, bPreventNameBasedLookup );
}

bool CParticleSystemMgr::WriteParticleConfigFile( const DmObjectId_t& id, CUtlBuffer &buf, bool bPreventNameBasedLookup )
{
	DECLARE_DMX_CONTEXT();
	// Create DMX elements representing the particle system definition
	CDmxElement *pParticleSystem = CreateParticleDmxElement( id );
	return WriteParticleConfigFile( pParticleSystem, buf, bPreventNameBasedLookup );
}

bool CParticleSystemMgr::WriteParticleConfigFile( CDmxElement *pParticleSystem, CUtlBuffer &buf, bool bPreventNameBasedLookup )
{
	pParticleSystem->SetValue( "preventNameBasedLookup", bPreventNameBasedLookup );

	CDmxAttribute* pAttribute = pParticleSystem->GetAttribute( "children" );
	const CUtlVector< CDmxElement* >& children = pAttribute->GetArray<CDmxElement*>( );
	int nCount = children.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmxElement *pChildRef = children[ i ];
		CDmxElement *pChild = pChildRef->GetValue<CDmxElement*>( "child" );
		pChild->SetValue( "preventNameBasedLookup", bPreventNameBasedLookup );
	}

	// Now write the DMX elements out
	bool bOk = SerializeDMX( buf, pParticleSystem );
	CleanupDMX( pParticleSystem );
	return bOk;
}

ParticleSystemHandle_t CParticleSystemMgr::GetParticleSystemIndex( const char *pParticleSystemName )
{
	if ( !pParticleSystemName )
		return UTL_INVAL_SYMBOL;

	return m_pParticleSystemDictionary->FindParticleSystemHandle( pParticleSystemName );
}

ParticleSystemHandle_t CParticleSystemMgr::FindOrAddParticleSystemIndex( const char *pParticleSystemName )
{
	if ( !pParticleSystemName )
		return UTL_INVAL_SYMBOL;

	return m_pParticleSystemDictionary->FindOrAddParticleSystemHandle( pParticleSystemName );
}


const char *CParticleSystemMgr::GetParticleSystemNameFromIndex( ParticleSystemHandle_t iIndex )
{
	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( iIndex );
	return pDef ? pDef->GetName() : "Unknown";
}

int CParticleSystemMgr::GetParticleSystemCount( void )
{
	return m_pParticleSystemDictionary->NameCount();
}


//-----------------------------------------------------------------------------
// Factory method for creating particle collections
//-----------------------------------------------------------------------------
CParticleCollection *CParticleSystemMgr::CreateParticleCollection( const char *pParticleSystemName, float flDelay, int nRandomSeed )
{
	if ( !pParticleSystemName )
		return NULL;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( pParticleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system type %s\n", pParticleSystemName );
		return NULL;
	}
	CParticleCollection *pParticleCollection = new CParticleCollection;
	pParticleCollection->Init( pDef, flDelay, nRandomSeed );
	return pParticleCollection;
}

CParticleCollection *CParticleSystemMgr::CreateParticleCollection( ParticleSystemHandle_t particleSystemName, float flDelay, int nRandomSeed )
{
	if ( particleSystemName == UTL_INVAL_SYMBOL )
		return NULL;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( particleSystemName );
	if ( !pDef )
	{
		Warning( "Attempted to create unknown particle system with unknown symbol\n" );
		return NULL;
	}
	CParticleCollection *pParticleCollection = new CParticleCollection;
	pParticleCollection->Init( pDef, flDelay, nRandomSeed );
	return pParticleCollection;
}

CParticleCollection *CParticleSystemMgr::CreateParticleCollection( const DmObjectId_t &id, float flDelay, int nRandomSeed )
{
	if ( !IsUniqueIdValid( id ) )
		return NULL;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( id );
	if ( !pDef )
	{
		char pBuf[256];
		UniqueIdToString( id, pBuf, sizeof(pBuf) );
		Warning( "Attempted to create unknown particle system id %s\n", pBuf );
		return NULL;
	}
	CParticleCollection *pParticleCollection = new CParticleCollection;
	pParticleCollection->Init( pDef, flDelay, nRandomSeed );
	return pParticleCollection;
}


//--------------------------------------------------------------------------------
// Is a particular particle system defined?
//--------------------------------------------------------------------------------
bool CParticleSystemMgr::IsParticleSystemDefined( const DmObjectId_t &id )
{
	if ( !IsUniqueIdValid( id ) )
		return false;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( id );
	return ( pDef != NULL );
}


//--------------------------------------------------------------------------------
// Is a particular particle system defined?
//--------------------------------------------------------------------------------
bool CParticleSystemMgr::IsParticleSystemDefined( const char *pName )
{
	if ( !pName || !pName[0] )
		return false;

	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( pName );
	return ( pDef != NULL );
}


//--------------------------------------------------------------------------------
// Particle kill list
//--------------------------------------------------------------------------------


//--------------------------------------------------------------------------------
// Serialization-related methods
//--------------------------------------------------------------------------------
CDmxElement *CParticleSystemMgr::CreateParticleDmxElement( const DmObjectId_t &id )
{
	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( id );

	// Create DMX elements representing the particle system definition
	return pDef->Write( );
}

CDmxElement *CParticleSystemMgr::CreateParticleDmxElement( const char *pParticleSystemName )
{
	CParticleSystemDefinition *pDef = m_pParticleSystemDictionary->FindParticleSystem( pParticleSystemName );

	// Create DMX elements representing the particle system definition
	return pDef->Write( );
}


//--------------------------------------------------------------------------------
// Particle sheets
//--------------------------------------------------------------------------------
static unsigned int s_nBaseTextureVarCache = 0;

CSheet *CParticleSystemMgr::FindOrLoadSheet( CParticleSystemDefinition *pDef, bool bTryReloading )
{
	if ( !m_bShouldLoadSheets )
		return NULL;

	if ( !bTryReloading )
	{
		if ( pDef->IsSheetSymbolCached() )
		{
			if ( !pDef->GetSheetSymbol().IsValid() )
				return NULL;
			return m_SheetList[ pDef->GetSheetSymbol() ];
		}

		pDef->CacheSheetSymbol( UTL_INVAL_SYMBOL );
	}

	IMaterial *pMaterial = pDef->GetMaterial();
	if ( !pMaterial )
		return NULL;

	IMaterialVar *pVar = pMaterial->FindVarFast( "$basetexture", &s_nBaseTextureVarCache );
	if ( !pVar || !pVar->IsDefined() )
		return NULL;

	ITexture *pTex = pVar->GetTextureValue();
	if ( !pTex || pTex->IsError() )
		return NULL;

	CSheet *pNewSheet = NULL;
	int nCurCount = m_SheetList.GetNumStrings();
	CUtlSymbol sheetName = m_SheetList.AddString( pTex->GetName() );
	if ( ( sheetName < nCurCount ) && ( !bTryReloading ) )
	{
		// Means the string was already known
		pNewSheet = m_SheetList[ sheetName ];
	}
	else
	{
		// get compact sheet representation held by texture
		size_t numBytes;
		void const *pSheetData = pTex->GetResourceData( VTF_RSRC_SHEET, &numBytes );
		if ( pSheetData )
		{
			// expand compact sheet into fatter runtime form
			CUtlBuffer bufLoad( pSheetData, numBytes, CUtlBuffer::READ_ONLY );
			pNewSheet = new CSheet( bufLoad );
		}
		m_SheetList[ sheetName ] = pNewSheet;
	}

	pDef->CacheSheetSymbol( sheetName );
	return pNewSheet;
}

void CParticleSystemMgr::FlushAllSheets( void )
{
	m_SheetList.PurgeAndDeleteElements();
	if ( !m_pParticleSystemDictionary )
		return;

	int nCount = m_pParticleSystemDictionary->Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CParticleSystemDefinition* pDef = m_pParticleSystemDictionary->GetParticleSystem( i );
		pDef->InvalidateSheetSymbol();
	}

	nCount = m_pParticleSystemDictionary->NameCount();
	for ( ParticleSystemHandle_t h = 0; h < nCount; ++h )
	{
		CParticleSystemDefinition* pDef = m_pParticleSystemDictionary->FindParticleSystem( h );
		pDef->InvalidateSheetSymbol();
	}
}

void CParticleSystemMgr::ShouldLoadSheets( bool bLoadSheets )
{
	// Client loads sheets for rendering, server doesn't need to.
	m_bShouldLoadSheets = bLoadSheets;
}

//-----------------------------------------------------------------------------
// Render cache
//-----------------------------------------------------------------------------
void CParticleSystemMgr::ResetRenderCache( void )
{
	int nCount = m_RenderCache.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_RenderCache[i].m_ParticleCollections.RemoveAll();
	}
}

void CParticleSystemMgr::AddToRenderCache( CParticleCollection *pParticles )
{
	if ( !pParticles->IsValid() )
		return;

	IMaterial *pMaterial = pParticles->m_pDef->GetMaterial();

	if ( ( !pMaterial) || pMaterial->IsTranslucent() )
		return;

	pParticles->m_flNextSleepTime = MAX( pParticles->m_flNextSleepTime, ( g_pParticleSystemMgr->GetLastSimulationTime() + pParticles->m_pDef->m_flNoDrawTimeToGoToSleep ));
	// Find the current rope list.
	int iRenderCache = 0;
	int nRenderCacheCount = m_RenderCache.Count();
	for ( ; iRenderCache < nRenderCacheCount; ++iRenderCache )
	{
		if ( ( pParticles->m_pDef->GetMaterial() == m_RenderCache[iRenderCache].m_pMaterial ) )
			break;
	}

	// A full rope list should have been generate in CreateRenderCache
	// If we didn't find one, then allocate the mofo.
	if ( iRenderCache == nRenderCacheCount )
	{
		iRenderCache = m_RenderCache.AddToTail();
		m_RenderCache[iRenderCache].m_pMaterial = pParticles->m_pDef->GetMaterial();
	}

	m_RenderCache[iRenderCache].m_ParticleCollections.AddToTail( pParticles );

	for( CParticleCollection *p = pParticles->m_Children.m_pHead; p; p = p->m_pNext )
	{
		AddToRenderCache( p );
	}
}


void CParticleSystemMgr::BuildBatchList( int iRenderCache, IMatRenderContext *pRenderContext, CUtlVector< Batch_t >& batches )
{
	batches.RemoveAll();

	IMaterial *pMaterial = m_RenderCache[iRenderCache].m_pMaterial;
	int nMaxVertices = pRenderContext->GetMaxVerticesToRender( pMaterial );
	int nMaxIndices = pRenderContext->GetMaxIndicesToRender();

	int nRemainingVertices = nMaxVertices;
	int nRemainingIndices = nMaxIndices;

	int i = batches.AddToTail();
	Batch_t* pBatch = &batches[i];
	pBatch->m_nVertCount = 0;
	pBatch->m_nIndexCount = 0;

	// Ask each renderer about the # of verts + ints it will draw
	int nCacheCount = m_RenderCache[iRenderCache].m_ParticleCollections.Count();
	for ( int iCache = 0; iCache < nCacheCount; ++iCache )
	{
		CParticleCollection *pParticles = m_RenderCache[iRenderCache].m_ParticleCollections[iCache];
		if ( !pParticles->IsValid() )
			continue;

		int nRenderCount = pParticles->GetRendererCount();
		for ( int j = 0; j < nRenderCount; ++j )
		{
			int nFirstParticle = 0;
			while ( nFirstParticle < pParticles->m_nActiveParticles )
			{
				int i;

				BatchStep_t step;
				step.m_pParticles = pParticles;
				step.m_pRenderer = pParticles->GetRenderer( j );
				step.m_pContext = pParticles->GetRendererContext( j ); 
				step.m_nFirstParticle = nFirstParticle;
				step.m_nParticleCount = step.m_pRenderer->GetParticlesToRender( pParticles, 
					step.m_pContext, nFirstParticle, nRemainingVertices, nRemainingIndices, &step.m_nVertCount, &i );
				nFirstParticle += step.m_nParticleCount;

				if ( step.m_nParticleCount > 0 )
				{
					pBatch->m_nVertCount += step.m_nVertCount;
					pBatch->m_nIndexCount += i;
					pBatch->m_BatchStep.AddToTail( step );
					Assert( pBatch->m_nVertCount <= nMaxVertices && pBatch->m_nIndexCount <= nMaxIndices );
				}
				else
				{
					if ( pBatch->m_nVertCount == 0 )
						break;

					// Not enough room
					Assert( pBatch->m_nVertCount > 0 && pBatch->m_nIndexCount > 0 ); 
					int j = batches.AddToTail();
					pBatch = &batches[j];
					pBatch->m_nVertCount = 0;
					pBatch->m_nIndexCount = 0;
					nRemainingVertices = nMaxVertices;
					nRemainingIndices = nMaxIndices;
				}
			}
		}
	}

	if ( pBatch->m_nVertCount <= 0 || pBatch->m_nIndexCount <= 0 ) 
	{
		batches.FastRemove( batches.Count() - 1 );
	}
}

void CParticleSystemMgr::DumpParticleList( const char *pNameSubstring /* = NULL */ )
{
	if ( pNameSubstring )
	{
		DevMsg( "New Particle Systems Matching '%s':\n", pNameSubstring );
	}
	else
	{
		DevMsg( "All Particle Systems:\n" );
	}

	for ( int i = 0; i < m_pParticleSystemDictionary->NameCount(); i++ )
	{
		CParticleSystemDefinition *p = ( *m_pParticleSystemDictionary )[ i ];

		if ( !pNameSubstring || V_stristr(p->GetName(),pNameSubstring) )
		{
			for ( CParticleCollection *c = p->FirstCollection(); c; c = c->GetNextCollectionUsingSameDef() )
			{
				Vector min,max,center;
				c->GetBounds( &min, &max );
				center = (min+max)*0.5f;
				DevMsg( "%40s: Age: %6.2f, NumActive: %3d, Bounds Center: (%.2f,%.2f,%.2f) (0x%p)\n", p->GetName(), c->m_flCurTime, c->m_nActiveParticles, center.x, center.y, center.z, c );
			}
		}
	}
}

void CParticleSystemMgr::DumpProfileInformation( void )
{
#if MEASURE_PARTICLE_PERF
	int nTotalTests = 0;
	int nActualTests = 0;
	FileHandle_t fh = g_pFullFileSystem->Open( "particle_profile.csv", "w", "DEFAULT_WRITE_PATH" );
	if ( fh == FILESYSTEM_INVALID_HANDLE )
	{
		Warning( "*** Unable to open profile file!\n" );
		return;
	}
	g_pFullFileSystem->FPrintf( fh, "numframes,%d\n", m_nNumFramesMeasured );
	g_pFullFileSystem->FPrintf( fh, "name, total time, max time, max particles, allocated particles, total render time, max render time, number of intersection tests, number of actual traces\n");
	for( int i=0; i < m_pParticleSystemDictionary->NameCount(); i++ )
	{
		CParticleSystemDefinition *p = ( *m_pParticleSystemDictionary )[ i ];
		if ( p->m_nMaximumActiveParticles )
		{
			nTotalTests += p->m_nNumIntersectionTests;
			nActualTests +=p->m_nNumActualRayTraces;
			g_pFullFileSystem->FPrintf( fh, "%s,%f,%f,%d,%d,%f,%f,%d,%d\n", p->m_Name.Get(), 
				p->m_flTotalSimTime, p->m_flMaxMeasuredSimTime, p->m_nMaximumActiveParticles, 
										p->m_nMaxParticles, p->m_flTotalRenderTime, p->m_flMaxMeasuredRenderTime, p->m_nNumIntersectionTests(), p->m_nNumActualRayTraces() );
		}
	}
	if ( nTotalTests )
	{
		g_pFullFileSystem->FPrintf( fh, "\n\nTrace cache efficiency = %f%%\n\n", 100.0 * ( 1.0 - ( nActualTests * ( 1.0 / nTotalTests ) ) ) );
	}
	g_pFullFileSystem->FPrintf( fh, "\n\nopname, total time, max time, total render time, max render time\n");
	for(int i=0; i < ARRAYSIZE( m_ParticleOperators ); i++)
	{
		for(int j=0; j < m_ParticleOperators[i].Count() ; j++ )
		{
			float flmax = m_ParticleOperators[i][j]->MaximumRecordedExecutionTime();
			float fltotal = m_ParticleOperators[i][j]->TotalRecordedExecutionTime();
			if ( fltotal > 0.0 )
			{
				g_pFullFileSystem->FPrintf( fh, "%s,%f,%f\n", 
											m_ParticleOperators[i][j]->GetName(), fltotal, flmax );
			}
		}
	}
	g_pFullFileSystem->Close( fh );
#endif
}

void CParticleSystemMgr::CommitProfileInformation( bool bCommit )
{
#if MEASURE_PARTICLE_PERF
	if ( 1 )
	{
		if ( bCommit )
		{
			m_nNumFramesMeasured++;
		}
		for( int i=0; i < m_pParticleSystemDictionary->NameCount(); i++ )
		{
			CParticleSystemDefinition *p = ( *m_pParticleSystemDictionary )[ i ];
			if ( bCommit )
			{
				p->m_flTotalSimTime		+= p->m_flUncomittedTotalSimTime;
				p->m_flTotalRenderTime	+= p->m_flUncomittedTotalRenderTime;
			}
			p->m_flUncomittedTotalSimTime = 0.;
			p->m_flUncomittedTotalRenderTime = 0.;
		}
		for(int i=0; i < ARRAYSIZE( m_ParticleOperators ); i++)
		{
			for(int j=0; j < m_ParticleOperators[i].Count() ; j++ )
			{
				if ( bCommit )
				{
					m_ParticleOperators[i][j]->m_flTotalExecutionTime += m_ParticleOperators[i][j]->m_flUncomittedTime;
				}
				m_ParticleOperators[i][j]->m_flUncomittedTime = 0;
			}
		}
	}
#endif
}


void CParticleSystemMgr::DrawRenderCache( IMatRenderContext *pRenderContext, bool bShadowDepth )
{
	int nRenderCacheCount = m_RenderCache.Count();
	if ( nRenderCacheCount == 0 )
		return;

	VPROF_BUDGET( "CParticleSystemMgr::DrawRenderCache", VPROF_BUDGETGROUP_PARTICLE_RENDERING );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	CUtlVector< Batch_t > batches( 0, 8 );

	for ( int iRenderCache = 0; iRenderCache < nRenderCacheCount; ++iRenderCache )
	{
		int nCacheCount = m_RenderCache[iRenderCache].m_ParticleCollections.Count();
		if ( nCacheCount == 0 )
			continue;

		// FIXME: When rendering shadow depth, do it all in 1 batch
		IMaterial *pMaterial = bShadowDepth ? m_pShadowDepthMaterial : m_RenderCache[iRenderCache].m_pMaterial;		

		BuildBatchList( iRenderCache, pRenderContext, batches );
		int nBatchCount = batches.Count();
		if ( nBatchCount == 0 )
			continue;

		pRenderContext->Bind( pMaterial );
		CMeshBuilder meshBuilder;
		IMesh* pMesh = pRenderContext->GetDynamicMesh( );

		for ( int i = 0; i < nBatchCount; ++i )
		{
			const Batch_t& batch = batches[i];
			Assert( batch.m_nVertCount > 0 && batch.m_nIndexCount > 0 );
			meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, batch.m_nVertCount, batch.m_nIndexCount );

			int nVertexOffset = 0;
			int nBatchStepCount = batch.m_BatchStep.Count();
			for ( int j = 0; j < nBatchStepCount; ++j )
			{
				const BatchStep_t &step = batch.m_BatchStep[j];
				// FIXME: this will break if it ever calls into C_OP_RenderSprites::Render[TwoSequence]SpriteCard()
				//        (need to protect against that and/or split the meshBuilder batch to support that path here)
				step.m_pRenderer->RenderUnsorted( step.m_pParticles, step.m_pContext, pRenderContext, 
					meshBuilder, nVertexOffset, step.m_nFirstParticle, step.m_nParticleCount );
				nVertexOffset += step.m_nVertCount;
			}

			meshBuilder.End();
			pMesh->Draw();
		}
	}

	ResetRenderCache( );

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}




void IParticleSystemQuery::GetRandomPointsOnControllingObjectHitBox(
	CParticleCollection *pParticles,
	int nControlPointNumber, 
	int nNumPtsOut,
	float flBBoxScale,
	int nNumTrysToGetAPointInsideTheModel,
	Vector *pPntsOut,
	Vector vecDirectionalBias,
	Vector *pHitBoxRelativeCoordOut,
	int *pHitBoxIndexOut,
	int nDesiredHitbox, 
	const char *pszHitboxSetName
	)
{
	for(int i=0; i < nNumPtsOut; i++)
	{
		pPntsOut[i]=pParticles->ControlPoint( nControlPointNumber ).m_Position;
		if ( pHitBoxRelativeCoordOut )
			pHitBoxRelativeCoordOut[i].Init();
		if ( pHitBoxIndexOut )
			pHitBoxIndexOut[i] = -1;
	}
}



void CParticleCollection::UpdateHitBoxInfo( int nControlPointNumber, const char *pszHitboxSetName )
{
	CModelHitBoxesInfo &hb = ControlPointHitBox( nControlPointNumber );

	if ( hb.m_flLastUpdateTime == m_flCurTime )
		return;												// up to date

	hb.m_flLastUpdateTime = m_flCurTime;

	// make sure space allocated
	if ( ! hb.m_pHitBoxes )
		hb.m_pHitBoxes = new ModelHitBoxInfo_t[ MAXSTUDIOBONES ];
	if ( ! hb.m_pPrevBoxes )
		hb.m_pPrevBoxes = new ModelHitBoxInfo_t[ MAXSTUDIOBONES ];

	// save current into prev
	hb.m_nNumPrevHitBoxes = hb.m_nNumHitBoxes;
	hb.m_flPrevLastUpdateTime = hb.m_flLastUpdateTime;
	V_swap( hb.m_pHitBoxes, hb.m_pPrevBoxes );

	// issue hitbox query
	hb.m_nNumHitBoxes = g_pParticleSystemMgr->Query()->GetControllingObjectHitBoxInfo(
		this, nControlPointNumber, MAXSTUDIOBONES, hb.m_pHitBoxes, pszHitboxSetName );

}

void GetParticleManifest( CUtlVector<CUtlString>& list )
{
	GetParticleManifest( list, "particles/particles_manifest.txt" );
}

void GetParticleManifest( CUtlVector<CUtlString>& list, const char *pFile )
{
	// Open the manifest file, and read the particles specified inside it
	KeyValues *manifest = new KeyValues( pFile );
	if ( manifest->LoadFromFile( g_pFullFileSystem, pFile, "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "file" ) )
			{
				list.AddToTail( sub->GetString() );
				continue;
			}

			Warning( "CParticleMgr::Init:  Manifest '%s' with bogus file type '%s', expecting 'file'\n", pFile, sub->GetName() );
		}
	}
	else
	{
		Warning( "PARTICLE SYSTEM: Unable to load manifest file '%s'\n", pFile );
	}

	manifest->deleteThis();
}
