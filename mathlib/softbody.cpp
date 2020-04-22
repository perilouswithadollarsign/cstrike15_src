//========= Copyright © Valve Corporation, All rights reserved. ============//
#include "softbody.h"
#include "vstdlib/jobthread.h"
#include "tier1/fmtstr.h"
#include "tier1/utlbuffer.h"
#include "mathlib/femodel.h"
#include "mathlib/femodel.inl"
#include "tier1/fmtstr.h"
#include "modellib/clothhelpers.h"
#include "mathlib/femodeldesc.h"
#include "mathlib/softbodyenvironment.h"
#include "tier0/miniprofiler.h"
#include "bitvec.h"
#include "filesystem.h"
#include "mathlib/dynamictree.h"
#include "engine/ivdebugoverlay.h"
#include "mathlib/vertexcolor.h"
#include "rubikon/param_types.h"
#include "materialsystem/imesh.h"
#include "mathlib/softbody.inl"

// #include "rnthread.h"
// #include "rnsimd.h"
// #include "modellib/model.h"
// #include "broadphase.h"
// #include "dynamictree.h"
// #include "sphereshape.h"
// #include "capsuleshape.h"
// #include "hullshape.h"
// #include "meshshape.h"

extern const CPUInformation &cpuInfo;
DECLARE_LOGGING_CHANNEL( LOG_PHYSICS );

enum ClothDebugFlagEnum_t
{
	CLOTH_DEBUG_SIM_ANIM_POS = 1 << 0, // GetParticleTransforms() returns sim pos = anim pos
	CLOTH_DEBUG_SIM_ANIM_ROT = 1 << 1,
	CLOTH_DEBUG_SNAP_TO_ANIM = 1 << 2, // copy positions from GetAnim() in Post()
	CLOTH_SKIP_FILTER_TRANSFORMS = 1 << 3,
	CLOTH_FORCE_INTERPOLATION_1 = 1 << 4
};


const int g_nClothDebug = 0;
// the verlet integrator has some root differences from the explicit Euler that the old cloth system employs.
// The factor of 2 here reflects one of those (at^2/2 doesn't work the same for the implicit integrator)
const float g_flClothAttrVel = 2; // the 2nd substep in Source1 (SysIterateOverTime) is made with 1/2 timestep and the force computed from animation force attraction is double of the intended...
const float g_flClothAttrPos = 1;
const float g_flClothDampingMultiplier = 1;
float g_flClothNodeVelocityLimit = 1000000;
float g_flClothGroundPlaneThickness = 3;

static const float g_flStickyDist = 2.0f;

static const float	g_flRopeSize = 20.0f;

static const float	g_flTeleportDeltaSq = ( 200.0f * 200.0f );
static const float	g_flNoTeleportDeltaSq = ( 100.0f * 100.0f );

MPROF_NODE( SoftbodyFilterTransforms, "Softbody:FilterTransforms", "Physics" );
MPROF_NODE( SoftbodyDraw, "Softbody:Draw", "Physics" );
MPROF_NODE( SoftbodyStep, "Softbody:Step", "Physics" );
MPROF_NODE( SoftbodyStepIntegrate, "Softbody:Step/Integrate", "Physics" )
MPROF_NODE( SoftbodyStepPredict, "Softbody:Step/Predict", "Physics" )
MPROF_NODE( SoftbodyStepCollide, "Softbody:Step/Collide", "Physics" )
MPROF_NODE( SoftbodyStepIterate, "Softbody:Step/Iterate", "Physics" )
MPROF_NODE( SoftbodyStepPost, "Softbody:Step/Post", "Physics" )
MPROF_NODE( FeRelaxRods, "Softbody:FeRelaxRods", "Physics" );
MPROF_NODE( FeRelaxQuads, "Softbody:FeRelaxQuads", "Physics" );
MPROF_NODE( SoftbodyStepCollideCompute, "Softbody:Step/Collide/Compute", "Physics" );
MPROF_NODE( SoftbodyStepCollideComputeOuter, "Softbody:Step/Collide/ComputeOuter", "Physics" );
//MPROF_NODE( SoftbodyTreeBounds, "Softbody:ComputeTreeBounds", "Physics" );




float g_flClothGuardThreshold = 1000;
int g_nClothWatch = 1;

class CRnSoftbodyChangeGuard
{
	const CSoftbody *m_pSoftbody;
	const char *m_pName;
	AABB_t m_Box0, m_Box1;
public:
	static float Difference( const AABB_t &a, const AABB_t &b )
	{
		return ( a.m_vMinBounds - b.m_vMinBounds ).Length() + ( a.m_vMaxBounds - b.m_vMaxBounds ).Length();
	}

	CRnSoftbodyChangeGuard( const CSoftbody *pSoftbody, const char *pName )
	{
		m_pSoftbody = pSoftbody;
		m_pName = pName;
		m_Box0 = GetAabb( pSoftbody->GetNodePositions( 0 ), pSoftbody->GetNodeCount() );
		m_Box1 = GetAabb( pSoftbody->GetNodePositions( 1 ), pSoftbody->GetNodeCount() );
	}

	~CRnSoftbodyChangeGuard()
	{
		AABB_t box0 = GetAabb( m_pSoftbody->GetNodePositions( 0 ), m_pSoftbody->GetNodeCount() );
		AABB_t box1 = GetAabb( m_pSoftbody->GetNodePositions( 1 ), m_pSoftbody->GetNodeCount() );
		float flMove = Difference( m_Box0, box0 ) + Difference( m_Box1, box1 );
		if ( flMove > g_flClothGuardThreshold )
		{
			if ( m_pSoftbody->GetIndexInWorld() == g_nClothWatch )
			{
				Log_Msg( LOG_PHYSICS, "Cloth %d %s in %s changed %g: from {%.0f,%.0f}\xB1{%.0f,%.0f} to {%.0f,%.0f}\xB1{%.0f,%.0f}\n", m_pSoftbody->m_nIndexInWorld, m_pSoftbody->m_DebugName.GetSafe(), m_pName, flMove,
					box0.GetCenter().x, box0.GetCenter().y, box0.GetSize().x, box0.GetSize().y,
					box1.GetCenter().x, box1.GetCenter().y, box1.GetSize().x, box1.GetSize().y
				);
			}
		}
	}
};

#if defined(_DEBUG)
#define CHANGE_GUARD() //CRnSoftbodyChangeGuard changeGuard( this, __FUNCTION__)
#else
#define CHANGE_GUARD() 
#endif

CSoftbody::CSoftbody( void )
{
	// this constructor prepares softbody for a snoop or deserialization
	m_pEnvironment = NULL;
	m_pPos0 = m_pPos1 = NULL;
	m_pParticles = NULL;
	m_flThreadStretch = 0;
	m_flSurfaceStretch = 0;
	InitDefaults( );
}


CSoftbody::CSoftbody( CSoftbodyEnvironment *pWorld, const CFeModel *pFeModel )
{
	Init( pWorld, pFeModel , 0 );
}

void CSoftbody::Init( CSoftbodyEnvironment *pWorld, const CFeModel *pFeModel, int numModelBones )
{
	m_pEnvironment = pWorld;
	m_pFeModel = const_cast< CFeModel* >( pFeModel );
	Init( numModelBones );
	m_pEnvironment->Register( this );
}



CSoftbody::~CSoftbody()
{
	Shutdown( );
}



void AddOrigin( matrix3x4a_t &tm, const Vector &vDelta )
{
	tm.SetOrigin( tm.GetOrigin() + vDelta );
}

void CSoftbody::ReplaceFeModel( CFeModelReplaceContext &context )
{
	uint8 *pOldBuffer = ( uint8* )m_pParticles;
	matrix3x4a_t *pOldAnim = GetAnimatedTransforms(), *pOldSim = GetSimulatedTransforms();
	VectorAligned *pOldPos0 = m_pPos0, *pOldPos1 = m_pPos1;
	m_StickyBuffer.Clear();

	m_pFeModel = const_cast< CFeModel* >( context.GetNew() );
	InitFeModel();

	Vector vSimOrigin = m_nAnimSpace == SOFTBODY_ANIM_SPACE_LOCAL ? vec3_origin : m_vSimOrigin;

	// heuristics: sim origin is probably where the user expects to find new pieces of their new cloth
	for ( int nNewNode = 0; nNewNode < ( int )m_nNodeCount; ++nNewNode )
	{
		// remap what little we can, shift what we can't
		int nOldNode = context.NewToOldNode( nNewNode );
		if ( nOldNode < 0 )
		{
			m_pPos0[ nNewNode ] += vSimOrigin;
			m_pPos1[ nNewNode ] += vSimOrigin;
		}
		else
		{
			m_pPos0[ nNewNode ] = pOldPos0[ nOldNode ];
			m_pPos1[ nNewNode ] = pOldPos1[ nOldNode ];
		}
	}

	// and map the animations, too
	for ( int nNewCtrl = 0; nNewCtrl < ( int )m_nParticleCount; ++nNewCtrl )
	{
		int nOldCtrl = context.NewToOldCtrl( nNewCtrl );
		if ( nOldCtrl < 0 )
		{
			AddOrigin( GetAnim( nNewCtrl ), vSimOrigin );
			AddOrigin( GetSim( nNewCtrl ), vSimOrigin );
		}
		else
		{
			GetAnim( nNewCtrl ) = pOldAnim[ nOldCtrl ];
			GetSim( nNewCtrl ) = pOldSim[ nOldCtrl ];
		}
	}

	GoWakeup(); // we could copy the old positions here, but I doubt anyone needs it
	MemAlloc_FreeAligned( pOldBuffer );
}




uint Align4( uint a )
{
	return ( a + 3 ) & ~3;
}


void CSoftbody::InitDefaults( )
{
	m_nDebugSelection = 0;
	m_nDebugDrawTreeEndLevel = 0;
	m_nDebugDrawTreeBeginLevel = 0;
	m_nDebugDrawTreeFlags = 0;
	m_flVelAirDrag = m_flExpAirDrag = 0.0f;
	m_flVelQuadAirDrag = m_flExpQuadAirDrag = 0.0f;
	m_flVelRodAirDrag = m_flExpRodAirDrag = 0.0f;
	m_flQuadVelocitySmoothRate = 0.0f;
	m_flRodVelocitySmoothRate = 0.0f;
	m_flVolumetricSolveAmount = 0.0f;
	m_nRodVelocitySmoothIterations = 0;
	m_nQuadVelocitySmoothIterations = 0;

	m_nSimFlags = 0;
	m_nAnimSpace = SOFTBODY_ANIM_SPACE_WORLD;
	m_flLastTimestep = 1.0f / 60.0f; 
	m_flOverPredict = 0;
	m_bAnimTransformChanged = false;
	m_bSimTransformsOutdated = false;
	m_bGravityDisabled = false;
	m_bEnableAnimationAttraction = true;
	m_bEnableFollowNodes = true;
	m_bEnableInclusiveCollisionSpheres = true;
	m_bEnableExclusiveCollisionSpheres = true;
	m_bEnableCollisionPlanes = true;
	m_bEnableGroundCollision = true;
	m_bEnableGroundTrace = false;
	m_bEnableFtlPass = false;
	m_bEnableSprings = true;
	m_bFrozen = false;
	m_bDebugDraw = true;
	m_bEnableSimd = GetCPUInformation().m_bSSE2;
	m_nEnableWorldShapeCollision = 0;
	m_nStepsSimulated = 0;
	m_flGravityScale = 1.0f;
	m_flModelScale = 1.0f;
	m_flClothScale = 1.0f;
	m_flVelocityDamping = 0.0f;
	m_flStepUnderRelax = 0.0f;
	m_flDampingMultiplier = 1.0f;
	m_vGround = vec3_origin;
	m_vSimOrigin = vec3_origin;
	m_vSimAngles.Init( );
	m_vRopeOffset = Vector( 0, -1, 0 ) * g_flRopeSize; // this is the "right" (?!) vector in dota heroes
	m_nActivityState = STATE_DORMANT;
	//m_nDebugNode = 0xFFFFFFFF;
/*
	m_bTeleportOnNextSetAbsOrigin = true;
	m_bTeleportOnNextSetAbsAngles = true;
*/
	m_nStateCounter = 0;
}


void CSoftbody::SetInstanceSettings( void *pSettings )
{
	if ( pSettings )
	{
		m_flClothScale = *( const float* ) pSettings;
	}
}


void CSoftbody::Init( int numModelBones )
{
	InitDefaults( );

	V_memset( m_pUserData, 0, sizeof( m_pUserData ) );

	InitFeModel( numModelBones );

	if ( IsDebug() ) Post();

	Validate();
}



void CSoftbody::InitFeModel( int numModelBones )
{
	m_nNodeCount = m_pFeModel->m_nNodeCount;
	m_nParticleCount = m_pFeModel->m_nCtrlCount;
	m_flThreadStretch = m_pFeModel->m_flDefaultThreadStretch;
	m_flSurfaceStretch = m_pFeModel->m_flDefaultSurfaceStretch;
	m_flGravityScale = m_pFeModel->m_flDefaultGravityScale;
	if ( m_flGravityScale == 0.0f )
	{
		Warning( "Graivty Scale 0, probably invalid default. Changing to 1\n" );
		m_flGravityScale = 1.0f;
	}

	m_flVelAirDrag = m_pFeModel->m_flDefaultVelAirDrag;
	m_flExpAirDrag = m_pFeModel->m_flDefaultExpAirDrag;
	m_flVelQuadAirDrag = m_pFeModel->m_flDefaultVelQuadAirDrag;
	m_flExpQuadAirDrag = m_pFeModel->m_flDefaultExpQuadAirDrag;
	m_flVelRodAirDrag = m_pFeModel->m_flDefaultVelRodAirDrag;
	m_flExpRodAirDrag = m_pFeModel->m_flDefaultExpRodAirDrag;
	m_flQuadVelocitySmoothRate = m_pFeModel->m_flQuadVelocitySmoothRate;
	m_flRodVelocitySmoothRate = m_pFeModel->m_flRodVelocitySmoothRate;
	m_flVolumetricSolveAmount = m_pFeModel->m_flDefaultVolumetricSolveAmount;
	m_nRodVelocitySmoothIterations = m_pFeModel->m_nRodVelocitySmoothIterations;
	m_nQuadVelocitySmoothIterations = m_pFeModel->m_nQuadVelocitySmoothIterations;

	uint bTreeCollisionCount = ( m_pFeModel->m_nTaperedCapsuleStretchCount | m_pFeModel->m_nTaperedCapsuleRigidCount | m_pFeModel->m_nSphereRigidCount | ( m_pFeModel->m_nDynamicNodeFlags & FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_MASK ) ) ;
	uint bAnyCollisionCount = bTreeCollisionCount | m_pFeModel->m_nCollisionPlanes | m_pFeModel->m_nCollisionSpheres[ 0 ];
	uint nAabbs = bTreeCollisionCount ? m_pFeModel->GetDynamicNodeCount() - 1 : 0;
	uint nParticleGlue = bAnyCollisionCount ? m_pFeModel->GetDynamicNodeCount() : 0;
	uint nMemSize = GetParticleArrayCount() * sizeof( matrix3x4a_t ) + m_nNodeCount * 2 * sizeof( VectorAligned ) + nAabbs * sizeof( FeAabb_t ) + nParticleGlue * sizeof( CParticleGlue );
	if ( numModelBones )
	{
		nMemSize += numModelBones * sizeof( *m_pModelBoneToCtrl ) + m_pFeModel->m_nCtrlCount * sizeof( *m_pCtrlToModelBone );
	}

	CBufferStrider buffer( MemAlloc_AllocAligned( nMemSize, 16 ) );
	V_memset( buffer.Get( ), 0, nMemSize );
	uint8 *pMemEnd = buffer.Get( ) + nMemSize; NOTE_UNUSED( pMemEnd );

	m_pParticles = buffer.Stride< matrix3x4a_t >( GetParticleArrayCount( ) );
	m_pPos0 = buffer.Stride< VectorAligned >( m_nNodeCount );
	m_pPos1 = buffer.Stride< VectorAligned >( m_nNodeCount );
	m_pAabb = nAabbs ? buffer.Stride< FeAabb_t >( nAabbs ) : NULL;
	if ( nParticleGlue )
	{
		m_StickyBuffer.EnsureBitExists( nParticleGlue - 1 );
		m_pParticleGlue = buffer.Stride< CParticleGlue >( nParticleGlue );
	}
	else
	{
		m_pParticleGlue = NULL;
	}
	if ( numModelBones )
	{
		m_pModelBoneToCtrl  = buffer.Stride< int16 >( numModelBones );
		for ( int i = 0; i < numModelBones; ++i )
		{
			m_pModelBoneToCtrl[ i ] = -1;
		}
		m_pCtrlToModelBone = buffer.Stride< int16 >( m_pFeModel->m_nCtrlCount );
		for ( uint i = 0; i < m_pFeModel->m_nCtrlCount; ++i )
		{
			m_pCtrlToModelBone[ i ] = -1;
		}
	}
	else
	{
		m_pModelBoneToCtrl = NULL;
		m_pCtrlToModelBone = NULL;
	}

	m_bEnableFtlPass = ( m_pFeModel->m_nDynamicNodeFlags & FE_FLAG_ENABLE_FTL ) != 0;

	m_nEnableWorldShapeCollision = ( m_pFeModel->m_nDynamicNodeFlags >> FE_FLAG_ENABLE_WORLD_SHAPE_COLLISION_SHIFT ) & ( ( 1 << SHAPE_COUNT ) - 1 );

	if ( !m_pFeModel->m_pNodeToCtrl && m_pFeModel->m_pInitPose )
	{


		AssertDbg( m_pFeModel->m_nCtrlCount == m_pFeModel->m_nNodeCount );
		for ( int nNode = 0; nNode < m_nNodeCount; ++nNode )
		{
			m_pPos0[ nNode ] = m_pPos1[ nNode ] = m_pFeModel->m_pInitPose[ nNode ].GetOrigin( );
			GetAnim( nNode ) = GetSim( nNode ) = TransformMatrix( m_pFeModel->m_pInitPose[ nNode ] );
		}
	}
	else
	{
		// simple initialization
		V_memset( m_pPos0, 0, m_nNodeCount * sizeof( *m_pPos0 ) );
		V_memset( m_pPos1, 0, m_nNodeCount * sizeof( *m_pPos1 ) );
		for ( int nCtrl = 0; nCtrl < GetParticleArrayCount( ); ++nCtrl )
		{
			m_pParticles[ nCtrl ] = g_MatrixIdentity;
		}
	}

	AssertDbg( pMemEnd == buffer.Get( ) );

	m_nSimFlags = m_pEnvironment->GetSoftbodySimulationFlags();
}

void CSoftbody::BindModelBoneToCtrl( int nModelBone, int nCtrl )
{
	if ( nCtrl >= 0 )
	{
		m_pCtrlToModelBone[ nCtrl ] = nModelBone;
	}
	if ( nModelBone >= 0 )
	{
		m_pModelBoneToCtrl[ nModelBone ] = nCtrl;
	}
}

void CSoftbody::SetSimFlags( uint nNewSimFlags )
{
	uint nDiffFlags = m_nSimFlags ^ nNewSimFlags;
	if ( nDiffFlags )
	{
		// reset?
		m_nSimFlags = nNewSimFlags;
	}
}


void CSoftbody::DebugDump( )
{
	if ( IsDebug() && g_nClothDebug && m_nStepsSimulated < 7 )
	{
		CUtlString line;
		AppendDebugInfo( line );
		Msg( "%s\n", line.Get( ) );
	}
}

void CSoftbody::AppendDebugInfo( CUtlString &line )
{
	AABB_t bbox = BuildBounds( );
	Vector c = bbox.GetCenter( ), e = bbox.GetSize( );
	CUtlString desc;
	const char *pActivityState = "Undefined";
	switch ( m_nActivityState )
	{
	case STATE_ACTIVE:
		pActivityState = "active";
		break;
	case STATE_DORMANT:
		pActivityState = "dormant";
		break;
	case STATE_WAKEUP:
		pActivityState = "wakeup";
		break;
	}
	desc.Format( "Softbody #%d %s.%d %s (%.4g,%.4g)\xb1(%.4g,%.4g)", m_nIndexInWorld, pActivityState, m_nStateCounter, m_DebugName.GetSafe( ), c.x, c.y, e.x, e.y );
	line += desc;
}



void CSoftbody::Validate()
{
	if ( IsDebug( ) )
	{
		const matrix3x4a_t *pSim = m_pParticles + m_nParticleCount; NOTE_UNUSED( pSim );
		for ( int nCtrl = 0; nCtrl < m_nParticleCount; ++nCtrl )
		{
			uint nNode = m_pFeModel->CtrlToNode( nCtrl );													NOTE_UNUSED( nNode );
			bool bIsSimulated = ( nNode < m_nParticleCount && nNode > m_pFeModel->m_nStaticNodes );	NOTE_UNUSED( bIsSimulated );
			AssertDbg( IsGoodWorldTransform( pSim[ nCtrl ] ) );
			AssertDbg( IsGoodWorldTransform( Descale( m_pParticles[ nCtrl ] ) ) );
		}
	}
}




void CSoftbody::SetPose( const CTransform *pPose )
{
	CHANGE_GUARD();
	for ( int nParticle = 0; nParticle < m_nParticleCount; ++nParticle )
	{
		SetCtrl( nParticle, pPose[ nParticle ] );
	}
	Validate( );

	m_bAnimTransformChanged = true;
	m_bSimTransformsOutdated = false;
}



void CSoftbody::SetPose( const CTransform &tm, const CTransform *pPose )
{
	CHANGE_GUARD();
	AssertDbg( FloatsAreEqual( QuaternionLength( tm.m_orientation ), 1.0f, 1e-5f ) );
	for ( int nParticle = 0; nParticle < m_nParticleCount; ++nParticle )
	{
		CTransform pose = ConcatTransforms( tm, pPose[ nParticle ] );
		SetCtrl( nParticle, pose );
	}
	Validate( );
	
	m_bAnimTransformChanged = true;
	m_bSimTransformsOutdated = false;
}


void CSoftbody::SetCtrl( int nParticle, const CTransform &pose )
{
	CHANGE_GUARD();
	matrix3x4a_t tmPose = TransformMatrix( pose );
	GetAnim( nParticle ) = tmPose;

	uint nNode = m_pFeModel->CtrlToNode( nParticle );
	if ( nNode < m_nNodeCount )
	{
		if ( m_pFeModel->m_pNodeIntegrator )
		{
			float flDamping = m_flLastTimestep * ( m_pFeModel->m_pNodeIntegrator[ nNode ].flPointDamping * m_flDampingMultiplier * g_flClothDampingMultiplier );
			if ( flDamping < 1.0f )
			{
				GetSim( nParticle ).SetOrigin(
					m_pPos0[ nNode ] = m_pPos1[ nNode ] =
					pose.GetOrigin( ) * ( 1 - flDamping ) + m_pPos1[ nNode ] * flDamping
				);
			}
		}
		else
		{
			m_pPos0[ nNode ] = m_pPos1[ nNode ] = pose.GetOrigin( );
		}
	}
	else
	{
		GetSim( nParticle ) = tmPose;
	}
}


void CSoftbody::SetPoseFromBones( const int16 *pCtrlToBone, const matrix3x4a_t *pBones, float flScale )
{
	CHANGE_GUARD();
	if ( m_pFeModel->m_pCtrlToNode )
	{
		for ( int nParticle = 0; nParticle < m_nParticleCount; ++nParticle )
		{
			if ( pCtrlToBone[ nParticle ] < 0 )
				continue;
			const matrix3x4a_t &pose = pBones[ pCtrlToBone[ nParticle ] ];
			AssertDbg( IsGoodWorldTransform( pose, flScale ) );
			GetSim( nParticle ) = GetAnim( nParticle ) = ScaleMatrix3x3( pose, flScale ) ;
			uint nNode = m_pFeModel->m_pCtrlToNode[ nParticle ];
			if ( nNode < m_nNodeCount )
			{
				m_pPos0[ nNode ] = m_pPos1[ nNode ] = pose.GetOrigin( );
			}
		}
	}
	else
	{
		for ( int nParticle = 0; nParticle < m_nParticleCount; ++nParticle )
		{
			if ( pCtrlToBone[ nParticle ] < 0 )
				continue;
			const matrix3x4a_t &pose = pBones[ pCtrlToBone[ nParticle ] ];
			const matrix3x4a_t poseNoScale = ScaleMatrix3x3( pose, flScale );
			AssertDbg( IsGoodWorldTransform( poseNoScale, 1.0f ) );
			GetSim( nParticle ) = GetAnim( nParticle ) = poseNoScale;
			uint nNode = nParticle;
			m_pPos0[ nNode ] = m_pPos1[ nNode ] = pose.GetOrigin( );
		}
	}

	m_bAnimTransformChanged = true;
	m_bSimTransformsOutdated = false;
	Validate( );
}





uint CSoftbody::Step( float flTimeStep )
{
	return Step( m_pEnvironment->GetSoftbodyIterations(), flTimeStep );
}


const float g_flClothStep = 1.0f;
const int g_nClothCompatibility = 1;


void CSoftbody::DebugPreStep(float flTimeStep )
{
#ifdef _DEBUG
	static bool s_bRead = false;
	CUtlString buf; NOTE_UNUSED( buf );
	if ( g_nClothDebug )
	{
		buf = PrintParticleState();
	}
	if ( s_bRead )
	{
		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( g_pFullFileSystem->ReadFile( "cloth.txt", NULL, buf ) )
		{
			ParseParticleState( buf, flTimeStep );
		}
		s_bRead = false;
	}
#endif
}



uint CSoftbody::Step( int nIterations, float flTimeStep )
{
	if ( m_bFrozen )
		return 0;
	CHANGE_GUARD();

	MPROF_AUTO_FAST( SoftbodyStep );

	flTimeStep *= g_flClothStep;
	if ( m_nActivityState == STATE_ACTIVE && flTimeStep > 1e-5f )
	{
		Validate(); // DebugPreStep(flTimeStep);
		RawSimulate( nIterations, flTimeStep );
		m_bSimTransformsOutdated = true;
		m_nStepsSimulated++;
		Post( ); 
		Validate( ); // DebugDump( );

		return 1;
	}
	else
	{
		return 0;
	}
}


uint CSoftbody::GetStateHash()const
{
	CRC32_t hash;
	CRC32_Init( &hash );
	CRC32_ProcessBuffer( &hash, m_pPos0, sizeof( *m_pPos0 ) * m_pFeModel->m_nNodeCount );
	CRC32_ProcessBuffer( &hash, m_pPos1, sizeof( *m_pPos1 ) * m_pFeModel->m_nNodeCount );
	CRC32_Final( &hash );
	return hash;
}

bool IsEqual( const CUtlVector< uint > &a, const CUtlVector< uint > & b )
{
	if ( a.Count() != b.Count() ) return false;
	for ( int nIndex = 0; nIndex < a.Count(); ++nIndex )
		if ( a[ nIndex ] != b[ nIndex ] )
			return false;
	return true;
}


// check that RawSimulate is deterministic
void CSoftbody::ValidatingSimulate( int nIterations, float flTimeStep )
{
	// remember the initial state
	CUtlVector< VectorAligned > pos0, pos1;
	int nNodes = m_pFeModel->m_nNodeCount;
	pos0.CopyArray( m_pPos0, nNodes );
	pos1.CopyArray( m_pPos1, nNodes );
	bool bWasAnimTransformChanged = m_bAnimTransformChanged;
	bool bRepeat = false;
	
	for ( ;; )
	{
		RawSimulate( nIterations, flTimeStep );
		CUtlVector< VectorAligned > res0, res1; // initial results
		res0.CopyArray( m_pPos0, nNodes );
		res1.CopyArray( m_pPos1, nNodes );

		CUtlVector< VectorAligned > rep0, rep1; // repeat input state
		rep0.CopyArray( pos0.Base(), nNodes );
		rep1.CopyArray( pos1.Base(), nNodes );

		VectorAligned *pSavePos0 = m_pPos0, *pSavePos1 = m_pPos1; // save the previous pointers
		m_pPos0 = rep0.Base();
		m_pPos1 = rep1.Base();
		m_bAnimTransformChanged = bWasAnimTransformChanged;
		RawSimulate( nIterations, flTimeStep );                   // repeat simulation with the repeat input state
		if ( m_pPos0 == rep1.Base() ) { rep0.Swap( rep1 ); } // m_Pos were swapped, swap rep
		m_pPos0 = pSavePos0;									  // recover buffer pointers
		m_pPos1 = pSavePos1;

		// check that the initial results equal to repeat results
		for ( int n = 0; n < nNodes; ++n )
		{
			Assert( res0[ n ] == rep0[ n ] );
			Assert( res1[ n ] == rep1[ n ] );
		}

		if ( !bRepeat )
			break;
		
		m_bAnimTransformChanged = bWasAnimTransformChanged;
		// start all over from the copied initial state
		V_memcpy( m_pPos0, pos0.Base(), sizeof( VectorAligned ) * nNodes );
		V_memcpy( m_pPos1, pos1.Base(), sizeof( VectorAligned ) * nNodes );
	}
}


void CSoftbody::RawSimulate( int nIterations, float flTimeStep )
{
	if ( g_nClothCompatibility >= 2 && ( m_pFeModel->m_nDynamicNodeFlags & FE_FLAG_UNINERTIAL_CONSTRAINTS ) )
	{
		Integrate_S1( flTimeStep );
		ResolveStretch_S1( flTimeStep ); // this folds in the constraint iterator loop
		ResolveAnimAttraction_S1( flTimeStep );
		Collide();
	}
	else
	{
		Integrate( flTimeStep );
		Predict( flTimeStep );
		AddAnimationAttraction( flTimeStep );
		Collide();
		{
			CConstraintIterator iterator( this );
			iterator.Iterate( nIterations );
		}
	}

}


void CSoftbody::ParseParticleState( CUtlBuffer &buf, float flTimeStep )
{
	char szName[ 300 ];
	Vector vOrigin, vAnimTarget, vVelocity;
	uint nTotalFound = 0; NOTE_UNUSED( nTotalFound );
	while ( 10 == buf.Scanf( "%s %f %f %f %f %f %f %f %f %f\n", szName, &vOrigin.x, &vOrigin.y, &vOrigin.z, &vAnimTarget.x, &vAnimTarget.y, &vAnimTarget.z, &vVelocity.x, &vVelocity.y, &vVelocity.z ) )
	{
		// find this bone
		bool bFound = false;
		for ( uint nCtrl = 0; nCtrl < m_pFeModel->m_nCtrlCount; ++nCtrl )
		{
			if ( !V_stricmp( m_pFeModel->m_pCtrlName[ nCtrl ], szName ) )
			{
				uint nNode = m_pFeModel->CtrlToNode( nCtrl );
				bFound = true;
				m_pPos1[ nNode ] = vOrigin;
				m_pPos0[ nNode ] = vOrigin - flTimeStep * vVelocity;
				GetAnim( nCtrl ).SetOrigin( vAnimTarget );
				break;
			}
		}
		if ( bFound )
		{
			nTotalFound++;
		}
		else
		{
			Msg( "Not found: %s\n", szName );
		}
	}
	Msg( "%d bones found and initialized\n", nTotalFound );
}


CUtlString CSoftbody::PrintParticleState( )const
{
	CUtlString buf;
	for ( uint nCtrl = 0; nCtrl < m_pFeModel->m_nCtrlCount ; ++nCtrl )
	{
		CUtlString line;
		uint nNode = m_pFeModel->CtrlToNode( nCtrl );
		const VectorAligned &vOrigin = m_pPos1[ nNode ];
		Vector vAnimTarget = GetAnim( nCtrl ).GetOrigin(), vVelocity = ( m_pPos1[ nNode ] - m_pPos0[ nNode ] ) / m_flLastTimestep;

		line.Format( "%s %g %g %g %g %g %g %g %g %g\n", m_pFeModel->m_pCtrlName[ nCtrl ],
			vOrigin.x, vOrigin.y, vOrigin.z, vAnimTarget.x, vAnimTarget.y, vAnimTarget.z, vVelocity.x, vVelocity.y, vVelocity.z
		);
		buf += line;
	}
	return buf;
}


void CSoftbody::Integrate_S1( float flTimeStep )
{
	const uint nStaticNodes = m_pFeModel->m_nStaticNodes;
	const FeNodeIntegrator_t *pNodeIntegrator = m_pFeModel->m_pNodeIntegrator;

	float flTickRate = 1.0f / flTimeStep, flPrevTickRate = 1.0f / m_flLastTimestep;

	VectorAligned *pPos0 = m_pPos0, *pPos1 = m_pPos1;

	if ( m_bAnimTransformChanged )
	{
		// <sergiy> Note that the animation is copied into static nodes in Source1 in CClothModelPiece::SetupBone(), cloth_system.cpp#36:3397
		// it's probably a good idea to continue doing that regardless of the slight bug in source1 that overshot static bones slightly, unless artists unwittingly relied on that bug in some cases
		{
			for ( uint nStaticNode = 0; nStaticNode < nStaticNodes; ++nStaticNode )
			{
				uint nCtrl = m_pFeModel->NodeToCtrl( nStaticNode );
				Assert( nCtrl < m_nParticleCount );
				pPos0[ nStaticNode ] = pPos1[ nStaticNode ];
				pPos1[ nStaticNode ] = GetAnim( nCtrl ).GetOrigin();
			}
		}
		m_bAnimTransformChanged = false;

		if ( m_bEnableFollowNodes )
		{
			for ( uint nFlwr = 0, nFollowers = m_pFeModel->m_nFollowNodeCount; nFlwr < nFollowers; ++nFlwr )
			{
				const FeFollowNode_t fn = m_pFeModel->m_pFollowNodes[ nFlwr ];
				Vector vDelta = ( pPos1[ fn.nParentNode ] - pPos0[ fn.nParentNode ] ) * fn.flWeight;
				// why pos0-=, and not pos1+=? because we're computing pos2=pos1 + ( pos1 - pos0 ) later, and advancing pos1 will have the effect of doubling the velocity of the child
				pPos1[ fn.nChildNode ] += vDelta;
				pPos0[ fn.nChildNode ] += vDelta;
			}
		}
	}
	else
	{
		// since we're double-buffering the positions, we need to copy the positions for at least one frame
		// after animation stopped updating them. We can add another flag here to avoid memcpy if it ever becomes noticeable CPU drain
		V_memcpy( m_pPos0, m_pPos1, sizeof( *m_pPos0 ) * nStaticNodes );
	}

	Vector vGravity( 0, 0, -( m_bGravityDisabled ? 0 : m_flGravityScale ) );

	AssertDbg( !m_pFeModel->m_pCtrlToNode ); // it should be safe to assume we have no node-to-ctrl mapping on Source1-imported content
	for ( uint nDynNode = nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
	{
		// CClothParticleState::InitialForces()
		VectorAligned &pos1 = pPos1[ nDynNode ], &pos0 = pPos0[ nDynNode ];
		Vector vOrigin = pos1, vVelocity = ( pos1 - pos0 ) * flPrevTickRate;
		float flInvMass = m_pFeModel->m_pNodeInvMasses[ nDynNode ];
		Vector vForce;
		// animation attraction
		if ( pNodeIntegrator )
		{
			const FeNodeIntegrator_t &integrator = pNodeIntegrator[ nDynNode ];
			vForce = vGravity * integrator.flGravity / flInvMass;
			Vector vAnimPos = GetAnim( nDynNode ).GetOrigin();
			Vector vDelta = vAnimPos - vOrigin;
			float flAnimationForceAttraction = integrator.flAnimationForceAttraction / flInvMass; // this is predivided by mass in CAuthClothParser::ParseLegacyDotaNodeGrid(), line 1894
			vForce += vDelta * ( flAnimationForceAttraction * g_flClothAttrVel * flTickRate );
			vForce -= ( pNodeIntegrator[ nDynNode ].flPointDamping / flInvMass ) * vVelocity; // it's safe to assume S1-imported cloth has Unitless Damping = false, so all the damping values are premultiplied by mass
		}
		else
		{
			vForce = vGravity * 360 / flInvMass; // 360 is the default gravity, if there's no integrator, 360 is the assumed gravity per node
		}

		// CClothParticleState::Integrate()
		float flDeltaTimeMass = flTimeStep * flInvMass;
		vVelocity += vForce * flDeltaTimeMass;
		pos1 = vOrigin + flTimeStep * vVelocity;
		pos0 = vVelocity; // pos0 will temporarily store velocity!
	}

	m_flLastTimestep = flTimeStep;
}


void CSoftbody::ResolveStretch_S1( float flTimeStep )
{
	VectorAligned *pVel = m_pPos0, *pPos = m_pPos1;
	float flConstraintScale = m_flModelScale * m_flClothScale;
	uint nStaticNodes = m_pFeModel->m_nStaticNodes;
	const float *pStretchForce = m_pFeModel->m_pLegacyStretchForce;

	for ( uint nRod = 0; nRod < m_pFeModel->m_nRodCount; ++nRod )
	{
		const FeRodConstraint_t &rod = m_pFeModel->m_pRods[ nRod ];
		VectorAligned &vOrigin1 = pPos[ rod.nNode[ 0 ] ], &vOrigin2 = pPos[ rod.nNode[ 1 ] ];
		VectorAligned &vVelocity1 = pVel[ rod.nNode[ 0 ] ], &vVelocity2 = pVel[ rod.nNode[ 1 ] ];

		//
		// CClothSpring::ResolveStretch()
		//
		Vector	vDeltaOrigin = vOrigin1 - vOrigin2;
		float	flDistance = vDeltaOrigin.Length();

		float flRestLength = rod.flMaxDist;
		if ( flDistance > flRestLength )
		{
			// Note: CClothSpring::m_flStretchiness = 1 - SpringStretchiness from keyvalue file.
			float flSpringStretchiness = rod.flRelaxationFactor;

			vDeltaOrigin /= flDistance;
			flDistance -= flRestLength;
			flDistance *= flSpringStretchiness;

			Vector vDeltaDistance = vDeltaOrigin * flDistance;

			if ( ( rod.nNode[ 0 ] <= rod.nNode[ 1 ] || rod.nNode[ 0 ] < nStaticNodes ) && rod.nNode[1] >= nStaticNodes )
			{
				vOrigin2 += vDeltaDistance;
				if ( pStretchForce )
				{
					Vector vDeltaVelocity = vDeltaDistance * pStretchForce[ rod.nNode[ 1 ] ];
					vVelocity2 += vDeltaVelocity;
				}
			}
			else if ( rod.nNode[ 0 ] >= nStaticNodes )
			{
				vOrigin1 -= vDeltaDistance;
				if ( pStretchForce )
				{
					Vector vDeltaVelocity = vDeltaDistance * pStretchForce[ rod.nNode[ 0 ] ];
					vVelocity1 -= vDeltaVelocity;
				}
			}
		}
	}
	// the rest is forward compatibility stuff
	float flRodStiffness = expf( -m_flThreadStretch );
	float flSurfaceStiffness = expf( -m_flSurfaceStretch );
	m_pFeModel->RelaxBend( pPos, flRodStiffness );
	{
		MPROF_AUTO_FAST( FeRelaxQuads );
		if ( m_bEnableSimd )
		{
			m_pFeModel->RelaxSimdQuads( pPos, flSurfaceStiffness, flConstraintScale, m_nSimFlags & SOFTBODY_SIM_EXPERIMENTAL_1 );
			m_pFeModel->RelaxSimdTris( pPos, flSurfaceStiffness, flConstraintScale );
		}
		else
		{
			m_pFeModel->RelaxQuads( pPos, flSurfaceStiffness, flConstraintScale, m_nSimFlags & SOFTBODY_SIM_EXPERIMENTAL_1 );
			m_pFeModel->RelaxTris( pPos, flSurfaceStiffness, flConstraintScale );
		}
	}
}


void CSoftbody::ResolveAnimAttraction_S1( float flTimeStep )
{
	VectorAligned *pPos = m_pPos1;
	AssertDbg( !m_pFeModel->m_pNodeToCtrl );
	if ( const FeNodeIntegrator_t *pIntegrator = m_pFeModel->m_pNodeIntegrator )
	{
		for ( uint nDynNode = m_pFeModel->m_nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
		{
			VectorAligned &refParticleOrigin = pPos[ nDynNode ];
			Vector vAnimPos = GetAnim( nDynNode ).GetOrigin();

			Vector vDelta = vAnimPos - refParticleOrigin;

			vDelta *= pIntegrator[ nDynNode ].flAnimationVertexAttraction * flTimeStep;

			refParticleOrigin += vDelta;
		}
	}

	// convert velocities back to positions for possible verlet integration on the next step
	for ( uint nDynNode = m_pFeModel->m_nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
	{
		m_pPos0[ nDynNode ] = pPos[ nDynNode ] - m_pPos0[ nDynNode ] * flTimeStep;
	}
}



//-------------------------------------------------------------------------------------------------
FORCEINLINE AABB_t AsBounds3( const FeAabb_t &bbox )
{
	AABB_t out;
	out.m_vMinBounds = AsVector( bbox.m_vMinBounds );
	out.m_vMaxBounds = AsVector( bbox.m_vMaxBounds );
	return out;
}




void CSoftbody::Collide()
{
	if ( !m_pAabb )
		return; // no memory preallocated to compute AABBs, presumably because we don't have anything to collide with
	CHANGE_GUARD();

	//if ( !( ( m_nEnableWorldShapeCollision && m_pEnvironment ) | m_pFeModel->m_nTaperedCapsuleStretchCount | m_pFeModel->m_nTaperedCapsuleRigidCount | m_pFeModel->m_nSphereRigidCount ) )
	//	return; // nothing to collide with, let's not recompute AABBs
	
	MPROF_AUTO_FAST( SoftbodyStepCollide );
	m_pFeModel->ComputeCollisionTreeBounds( m_pPos1 + m_pFeModel->m_nStaticNodes, m_pAabb );

	CollideWithRigidsInternal();

	CollideWithWorldInternal();
}





void ProjectParticleOutOfSphere( VectorAligned &refParticle, const Vector &vSphereCenter, float flSumRadius )
{
	Vector vDist = refParticle - vSphereCenter;
	float flDistSqr = vDist.LengthSqr();
	if ( flDistSqr < 0.01f )
	{
		// unstable; TODO: go along the normal?
		refParticle.z = vSphereCenter.z + flSumRadius;
	}
	else
	{
		if ( flDistSqr < Sqr( flSumRadius ) )
		{
			Vector vRenormalized = vDist * flSumRadius / sqrtf( flDistSqr );
			refParticle = vSphereCenter + vRenormalized;
		}
	}
}


float ProjectParticleOutOfSphere_Sticky( VectorAligned &refParticle, const Vector &vSphereCenter, float flSumRadius, float flStickiness )
{
	Vector vDist = refParticle - vSphereCenter;
	float flDistSqr = vDist.LengthSqr();
	if ( flDistSqr < 0.01f )
	{
		// unstable; TODO: go along the normal?
		refParticle.z = vSphereCenter.z + flSumRadius;
		return flStickiness; // full stickiness
	}
	else if ( flDistSqr < Sqr( flSumRadius ) )
	{
		Vector vRenormalized = vDist * flSumRadius / sqrtf( flDistSqr );
		refParticle = vSphereCenter + vRenormalized;
		return flStickiness; // full stickiness
	}
	else if ( flDistSqr < Sqr( flSumRadius + g_flStickyDist ) )
	{
		return flStickiness * ( flSumRadius + g_flStickyDist - sqrtf( flDistSqr ) );
	}

	return 0.0f;//too far for any stickiness
}



class CTaperedCapsuleColliderFunctor_Slidy : public CTaperedCapsuleColliderFunctor
{
public:
	void operator()( uint nDynNode ) const
	{
		VectorAligned &refParticle = m_pDynPos1[ nDynNode ];
		Vector d = refParticle - m_vSphereCenter0;
		float flNodeRadius = m_pNodeCollisionRadii ? m_pNodeCollisionRadii[ nDynNode ] : 0;
		float flNodeCoreRadius = flNodeRadius * 0.5f;
		float proj = DotProduct( d, m_vAxisX );
		Vector vOrtho = d - m_vAxisX * proj;
		float flLineDist = vOrtho.Length();
		float flAffineProj = proj + flLineDist * m_flSlope;
		if ( flAffineProj <= 0 )
		{
			// projection is onto Sphere0
			ProjectParticleOutOfSphere( refParticle, m_vSphereCenter0, flNodeCoreRadius + m_vSphereCenter0.w );
		}
		else if ( flAffineProj > m_flDist )
		{
			// projection is onto Sphere1
			ProjectParticleOutOfSphere( refParticle, m_vSphereCenter1, flNodeRadius + m_vSphereCenter1.w );
		}
		else
		{
			// projection is onto the interior conical surface of the tapered capsule
			float flBlend = flAffineProj / m_flDist;
			ProjectParticleOutOfSphere( refParticle, m_vSphereCenter0 * ( 1 - flBlend ) + m_vSphereCenter1 * flBlend, flNodeRadius + m_vSphereCenter0.w * ( 1 - flBlend ) + m_vSphereCenter1.w * flBlend );
		}
	}
};




class CTaperedCapsuleColliderFunctor_Sticky : public CTaperedCapsuleColliderFunctor
{
public:
	void operator()( uint nDynNode ) const
	{
		VectorAligned &refParticle = m_pDynPos1[ nDynNode ];
		Vector d = refParticle - m_vSphereCenter0;
		float flNodeRadius = m_pNodeCollisionRadii ? m_pNodeCollisionRadii[ nDynNode ] : 0;
		float flNodeCoreRadius = flNodeRadius * 0.5f;
		float proj = DotProduct( d, m_vAxisX );
		Vector vOrtho = d - m_vAxisX * proj;
		float flLineDist = vOrtho.Length();
		float flAffineProj = proj + flLineDist * m_flSlope;
		if ( flAffineProj <= 0 )
		{
			// projection is onto Sphere0
			float flNewStickiness = ProjectParticleOutOfSphere_Sticky( refParticle, m_vSphereCenter0, flNodeCoreRadius + m_vSphereCenter0.w, m_flStickiness );
			if ( flNewStickiness > 0.01f )
			{
				m_pSoftbody->GlueNode( nDynNode, m_nParentNode[ 0 ], flNewStickiness );
			}
		}
		else if ( flAffineProj > m_flDist )
		{
			// projection is onto Sphere1
			float flNewStickiness = ProjectParticleOutOfSphere_Sticky( refParticle, m_vSphereCenter1, flNodeRadius + m_vSphereCenter1.w, m_flStickiness );
			if ( flNewStickiness > 0.01f )
			{
				m_pSoftbody->GlueNode( nDynNode, m_nParentNode[ 1 ], flNewStickiness );
			}
		}
		else
		{
			// projection is onto the interior conical surface of the tapered capsule
			float flBlend = flAffineProj / m_flDist;
			float flNewStickiness = ProjectParticleOutOfSphere_Sticky( refParticle, m_vSphereCenter0 * ( 1 - flBlend ) + m_vSphereCenter1 * flBlend, flNodeRadius + m_vSphereCenter0.w * ( 1 - flBlend ) + m_vSphereCenter1.w * flBlend, m_flStickiness );
			if ( flNewStickiness > 0.01f )
			{
				m_pSoftbody->GlueNode( nDynNode, m_nParentNode[ 0 ], m_nParentNode[ 1 ], flNewStickiness, flBlend );
			}
		}
	}
};





class CSphereColliderFunctor_Slidy : public CSphereColliderFunctor
{
public:
	CSphereColliderFunctor_Slidy( CSoftbody *pSoftbody, const Vector &vSphereCenter, float flSphereRadius, float flStickiness, uint16 nParentNode ) :
		CSphereColliderFunctor( pSoftbody, vSphereCenter, flSphereRadius, flStickiness, nParentNode )
	{
	}

	void operator () ( uint nDynNode )const
	{
		VectorAligned &refParticle = m_pDynPos1[ nDynNode ];
		float flSumRadius = m_Sphere.w;
		if ( m_pNodeCollisionRadii )
		{
			flSumRadius += m_pNodeCollisionRadii[ nDynNode ];
		}
		ProjectParticleOutOfSphere( refParticle, m_Sphere, flSumRadius );
	}
};





class CSphereColliderFunctor_Sticky : public CSphereColliderFunctor
{
public:
	
	void operator () ( uint nDynNode )const
	{
		VectorAligned &refParticle = m_pDynPos1[ nDynNode ];
		float flSumRadius = m_Sphere.w;
		if ( m_pNodeCollisionRadii )
		{
			flSumRadius += m_pNodeCollisionRadii[ nDynNode ];
		}
		float flNewStickiness = ProjectParticleOutOfSphere_Sticky( refParticle, m_Sphere, flSumRadius, m_flStickiness );
		if ( flNewStickiness > 0.01f )
		{
			m_pSoftbody->GlueNode( nDynNode, m_nParentNode, flNewStickiness );
		}
	}
};


void CSphereColliderFunctor::Collide( uint16 nCollisionMask )
{
	FeAabb_t aabb;
	fltx4 f4CenterAndRadius = LoadAlignedSIMD( m_Sphere.Base() ), f4SphereRadius = SplatWSIMD( f4CenterAndRadius );
	aabb.m_vMinBounds = f4CenterAndRadius - f4SphereRadius;
	aabb.m_vMaxBounds = f4CenterAndRadius + f4SphereRadius;

	const CFeModel *pFeModel = m_pSoftbody->GetFeModel();

	if ( m_flStickiness <= 0.01f )
	{
		pFeModel->CastBox( nCollisionMask, aabb, m_pSoftbody->m_pAabb, static_cast< CSphereColliderFunctor_Slidy& >( *this ) );
	}
	else
	{
		aabb.m_vMinBounds -= ReplicateX4( g_flStickyDist );
		aabb.m_vMaxBounds += ReplicateX4( g_flStickyDist );

		pFeModel->CastBox( nCollisionMask, aabb, m_pSoftbody->m_pAabb, static_cast< CSphereColliderFunctor_Sticky& >( *this ) );
	}
}



void CTaperedCapsuleColliderFunctor::Collide( uint16 nCollisionMask )
{
	Vector vAxis = m_vSphereCenter1 - m_vSphereCenter0;
	float flDist = vAxis.Length();
	float flStickOut = flDist - ( m_vSphereCenter1.w - m_vSphereCenter0.w ); // how much the small sphere sticks out of the large sphere
	if ( flStickOut > /*tc.flRadius[ 0 ] **/ 0.5f )  // only collide with full capsule if sticks out enough
	{
		FeAabb_t aabb;
		fltx4 f4Center0 = LoadAlignedSIMD( m_vSphereCenter0.Base() ), f4Center1 = LoadAlignedSIMD( m_vSphereCenter1.Base() ), f4SphereRadius0 = SplatWSIMD( f4Center0 ), f4SphereRadius1 = SplatWSIMD( f4Center1 );
		aabb.m_vMinBounds = MinSIMD( f4Center0 - f4SphereRadius0, f4Center1 - f4SphereRadius1 );
		aabb.m_vMaxBounds = MaxSIMD( f4Center0 + f4SphereRadius0, f4Center1 + f4SphereRadius1 );

		const CFeModel *pFeModel = m_pSoftbody->GetFeModel();

		if ( m_flStickiness <= 0.01f )
		{
			pFeModel->CastBox( nCollisionMask, aabb, m_pSoftbody->m_pAabb, static_cast< CTaperedCapsuleColliderFunctor_Slidy& >( *this ) );
		}
		else
		{
			aabb.m_vMinBounds -= ReplicateX4( g_flStickyDist );
			aabb.m_vMinBounds += ReplicateX4( g_flStickyDist );
			pFeModel->CastBox( nCollisionMask, aabb, m_pSoftbody->m_pAabb, static_cast< CTaperedCapsuleColliderFunctor_Sticky& >( *this ) );
		}
	}
	else
	{
		CSphereColliderFunctor sphere;
		sphere.m_Sphere = m_vSphereCenter1;
		sphere.m_pDynPos1 = m_pDynPos1;
		sphere.m_pNodeCollisionRadii = m_pNodeCollisionRadii;
		sphere.m_pSoftbody = m_pSoftbody;
		sphere.m_flStickiness = m_flStickiness;
		sphere.m_nParentNode = m_nParentNode[ 1 ];
		sphere.Collide( nCollisionMask ); // just do the big sphere, it almost encloses the small sphere
	}
}




class CRnSphereColliderFunctor
{
	VectorAligned m_Sphere;
	VectorAligned *m_pDynPos1;
	const float *m_pNodeCollisionRadii;
	float m_flRadiusScale;
public:
	CRnSphereColliderFunctor( const Vector &vCenter, float flRadius, VectorAligned *pDynPos1, const float *pNodeCollisionRadii, float flRadiusScale )
	{
		m_Sphere = vCenter;
		m_Sphere.w = flRadius;
		m_pDynPos1 = pDynPos1;
		m_pNodeCollisionRadii = pNodeCollisionRadii;
		m_flRadiusScale = flRadiusScale;
	}

	void operator () ( uint nDynNode )const
	{
		VectorAligned &refParticle = m_pDynPos1[ nDynNode ];
		float flSumNodeRadius = m_pNodeCollisionRadii ? m_pNodeCollisionRadii[ nDynNode ] * m_flRadiusScale + m_Sphere.w : m_Sphere.w;
		ProjectParticleOutOfSphere( refParticle, m_Sphere, flSumNodeRadius );
	}
};



class CRnCapsuleColliderFunctor
{
	Vector m_vCenter0;
	Vector m_vCenter1;
	Vector m_vAxisX;
	float m_flRadius;
	float m_flDist;
	VectorAligned *m_pDynPos1;
	const float *m_pNodeCollisionRadii;
	float m_flRadiusScale;
public:
	CRnCapsuleColliderFunctor( const Vector &vCenter0, const Vector &vCenter1, float flRadius, VectorAligned *pDynPos1, const float *pNodeCollisionRadii, float flRadiusScale )
	{
		m_vCenter0 = vCenter0;
		m_vCenter1 = vCenter1;
		m_flRadius = flRadius;
		m_vAxisX = vCenter1 - vCenter0;
		m_flDist = m_vAxisX.Length();
		m_vAxisX /= m_flDist;
		m_pDynPos1 = pDynPos1;
		m_pNodeCollisionRadii = pNodeCollisionRadii;
		m_flRadiusScale = flRadiusScale;
	}

	void operator()( uint nDynNode ) const
	{
		VectorAligned &refParticle = m_pDynPos1[ nDynNode ];
		Vector d = refParticle - m_vCenter0;
		float flSumNodeRadius = m_pNodeCollisionRadii ? m_pNodeCollisionRadii[ nDynNode ] * m_flRadiusScale + m_flRadius : m_flRadius;
		float proj = DotProduct( d, m_vAxisX );
		if ( proj <= 0 )
		{
			// projection is onto Sphere0
			ProjectParticleOutOfSphere( refParticle, m_vCenter0, flSumNodeRadius );
		}
		else if ( proj > m_flDist )
		{
			// projection is onto Sphere1
			ProjectParticleOutOfSphere( refParticle, m_vCenter1, flSumNodeRadius );
		}
		else
		{
			Vector vOrtho = d - m_vAxisX * proj;
			float flLineDist = vOrtho.Length();
			float flPushOutBy = flSumNodeRadius - flLineDist;
			if ( flPushOutBy > 0 )
			{
				// project outward
				if ( flLineDist > 0.0001f )
				{
					refParticle += ( flPushOutBy / flLineDist ) * vOrtho;
				}
				else
				{
					refParticle += VectorPerpendicularToVector( m_vAxisX ) * flSumNodeRadius;
				}
			}
		}
	}
};



#ifdef SOURCE2_SUPPORT
bool SoftbodyGjk( Vector &refOut, const Vector &vParticle, const CGJKHull& proxy2, int nIterationCount = 16 )
{
	// Build initial simplex  
	CRnSimplex simplex, last_simplex;
	simplex.Init( vParticle, proxy2.GetVertex( 0 ) );

	// Compute initial distance
	float d2 = FLT_MAX;
	bool bOverlap = false; NOTE_UNUSED( bOverlap );

	// Run GJK
	for ( int nIteration = 0; nIteration < nIterationCount; ++nIteration )
	{
		// Solve simplex and check if associated tetrahedron contains the origin
		simplex.Solve();

		if ( simplex.VertexCount() == MAX_SIMPLEX_VERTICES )
		{
			// Overlap
			bOverlap = true;
			break;
		}

		// Test distance progression
		float d2_old = d2;
		Vector cp = simplex.GetClosestPoint();
		d2 = Dot( cp, cp );

		if ( d2 >= d2_old )
		{
			// Reconstruct last simplex
			AssertDbg( !last_simplex.IsEmpty() );
			simplex = last_simplex;
			break;
		}

		// Build a new tentative support vertex
		Vector v = simplex.GetSearchDirection();
		if ( LengthSq( v ) < 1000.0f * FLT_MIN )
		{
			// The origin is probably contained by a line segment
			// or triangle. Thus the shapes are overlapped.

			// We should return zero here because there may be overlap.
			// In case the simplex is a point, segment, or triangle it is difficult
			// to determine if the origin is contained in the CSO or very close to it.
			// We'll use SAT outside to resolve this - when Distance is exactly 0.0, we should run SAT as it may mean negative distance (penetration) in reality, and GJK doesn not compute penetration distance.
			bOverlap = true;
			break;
		}

		const Vector &w1 = vParticle;
		int nIndex2 = proxy2.GetSupport( v );
		Vector w2 = proxy2.GetVertex( nIndex2 );

		last_simplex = simplex;
		if ( !simplex.AddVertex( 0, w1, nIndex2, w2 ) )
		{
			// cache hit; we may start infinitely cycling - break the cycle
			break;
		}
	}

	Vector vPoint1, vPoint2;
	simplex.BuildWitnessPoints( vPoint1, vPoint2 );

	Assert( !bOverlap || Distance( vPoint1, vPoint2 ) < FLT_EPSILON );

	refOut = vPoint2;
	return !bOverlap;
}


class CRnHullColliderFunctor : public CGJKHull
{
	const Transform &m_Xform;
	VectorAligned *m_pDynPos1;
	const float *m_pNodeCollisionRadii;
	float m_flRadiusScale;
	float m_flAddRadius;
public:
	CRnHullColliderFunctor( const RnHull_t *pHull, float flHullScale, const Transform &xform, float flAddRadius, VectorAligned *pDynPos1, const float *pNodeCollisionRadii, float flRadiusScale ) :
		CGJKHull( pHull, flHullScale ),
		m_Xform( xform ),
		m_flAddRadius( flAddRadius ),
		m_flRadiusScale( flRadiusScale ),
		m_pDynPos1( pDynPos1 ),
		m_pNodeCollisionRadii( pNodeCollisionRadii )
	{
	}

	static float Distance( const RnPlane_t &plane, float flScale, const Vector &vParticle )
	{
		return DotProduct( plane.m_vNormal, vParticle ) - flScale * plane.m_flOffset;
	}

	AABB_t GetAabb()
	{
		AABB_t aabb;
		aabb.MakeInvalid();
		for ( int nVertex = 0; nVertex < m_pHull->GetVertexCount(); ++nVertex )
		{
			aabb |= Mul( m_Xform, GetVertex( nVertex ) );	// GetVertex takes the scale into account
		}
		aabb.Expand( m_flAddRadius );
		return aabb;
	}

	void operator()( uint nDynNode ) const
	{
		VectorAligned &refParticle = m_pDynPos1[ nDynNode ];
		Vector vLocalParticle = TMul( m_Xform, refParticle );
		RnDistanceQueryResult_t result;
		Vector vPointOnHull;
		float flSumNodeRadius = m_pNodeCollisionRadii ? m_pNodeCollisionRadii[ nDynNode ] * m_flRadiusScale + m_flAddRadius : m_flAddRadius;
		if ( SoftbodyGjk( vPointOnHull, vLocalParticle, *this ) )
		{
			// GJK succeeded
			Vector vNormal = vLocalParticle - vPointOnHull;
			float flDistance = vNormal.Length();
			if ( flDistance < flSumNodeRadius )
			{
				AssertDbg( flDistance > FLT_EPSILON );
				refParticle += Mul( m_Xform.R, vNormal ) * ( flSumNodeRadius / flDistance - 1 );
			}
		}
		else
		{
			// GJK failed; run SAT
			const RnPlane_t *pPlanes = m_pHull->m_Planes.Base();
			int nPlaneCount = m_pHull->GetPlaneCount();
			float flScale = m_flScale;
			int nBestPlane = 0;
			float flMaxDist = Distance( pPlanes[0], flScale, vLocalParticle );
			for ( int nPlane = 1; nPlane < nPlaneCount; ++nPlane )
			{
				const RnPlane_t &plane = pPlanes[ nPlane ];
				float flDistance = Distance( plane, flScale, vLocalParticle );
				if ( flDistance > flMaxDist )
				{
					flMaxDist = flDistance;
					nBestPlane = nPlane;
				}
			}
			AssertDbg( flMaxDist < FLT_EPSILON );
			if ( flMaxDist < flSumNodeRadius )
			{
				refParticle += ( flSumNodeRadius - flMaxDist ) * Mul( m_Xform.R, pPlanes[ nBestPlane ].m_vNormal );
			}
		}
	}
};


class CRnTriColliderFunctor
{
	CFeModel::Aabb_t m_Aabb;
	VectorAligned *m_pDynPos1;
	const float *m_pNodeCollisionRadii;
	float m_flRadiusScale;
	float m_flAddRadius;
public:
	Vector m_v0;
	Vector m_vNormal;
	Vector m_d1;
	Vector m_d2;
	float m_flD1x; // the edge 0-1 goes from (0,0) to ( D1x, 0)
	Vector2D m_D2; // the edge 0-2 goes from (0,0) to ( D2x, D2y )
	Vector2D m_n12;// 2D normal to edge 1-2, pointing inside triangle, is -D2y, D2x-D1x
	Vector2D m_n20;// 2D normal to edge 2-0, pointing inside triangle, is D2y, -D2x

	mutable int m_nProbedNodes;
// 	mutable int m_nMovedNodes;
public:
	CRnTriColliderFunctor( float flAddRadius, VectorAligned *pDynPos1, const float *pNodeCollisionRadii, float flRadiusScale ) :
		m_flAddRadius( flAddRadius ),
		m_flRadiusScale( flRadiusScale ),
		m_pDynPos1( pDynPos1 ),
		m_pNodeCollisionRadii( pNodeCollisionRadii )
	{
 		m_nProbedNodes = 0;
// 		m_nMovedNodes = 0;
	}

	void PrepareTriangle( CRnMeshShape *pMesh, uint nTri, const Transform &bodyTransform )
	{
		VectorAligned v0, v1, v2;
		pMesh->GetTriangle( nTri, v0, v1, v2 );
		m_Aabb.m_vMaxBounds = m_Aabb.m_vMinBounds = LoadAlignedSIMD( &v0 );
		m_Aabb |= LoadAlignedSIMD( &v1 );
		m_Aabb |= LoadAlignedSIMD( &v2 );

		m_v0 = bodyTransform * v0;
		Vector d1 = bodyTransform.R * ( v1 - v0 );
		m_flD1x = d1.Length();
		m_d1 = d1 / m_flD1x;
		Vector d2 = bodyTransform.R * ( v2 - v0 );
		m_D2.x = DotProduct( m_d1, d2 );
		m_d2 = d2 - m_d1 * m_D2.x;
		m_D2.y = m_d2.Length();
		m_d2 /= m_D2.y;
		m_n12 = Vector2D( -m_D2.y, m_D2.x - m_flD1x );
		m_n12.NormalizeInPlace();
		m_n20 = Vector2D( m_D2.y, -m_D2.x );
		m_n20.NormalizeInPlace();

		m_vNormal = CrossProduct( m_d1, m_d2 );
		AssertDbg( fabsf( m_vNormal.Length() - 1 ) < 0.001f );
	}

	const CFeModel::Aabb_t &GetAabb()const
	{
		return m_Aabb;
	}

	void operator()( uint nDynNode )const 
	{
		m_nProbedNodes++;
		VectorAligned &refParticle = m_pDynPos1[ nDynNode ];
		float flSumNodeRadius = m_pNodeCollisionRadii ? m_pNodeCollisionRadii[ nDynNode ] * m_flRadiusScale + m_flAddRadius : m_flAddRadius;
		Vector d = refParticle - m_v0;
		float flDistNormal = DotProduct( m_vNormal, d );
		if ( fabsf( flDistNormal ) > flSumNodeRadius )
			return;
		float x = DotProduct( m_d1, d ), y = DotProduct( m_d2, d ); // x, y of the projection of the point onto the plane
		// 2D SAT: go through all edges, find the best separating axis (min edge penetration )
		float flPenetration01 = y;
		float flPenetration12 = m_n12.x * ( x - m_flD1x ) + m_n12.y * y;
		float flPenetration20 = m_n20.x * x + m_n20.y * y;
		float flEdgePenetration = Min( flPenetration01, Min( flPenetration12, flPenetration20 ) );
		if ( flEdgePenetration >= 0 )
		{
			// the point projects inside the triangle
			refParticle += ( flSumNodeRadius - flDistNormal ) * m_vNormal;
			//++m_nMovedNodes;
		}
		else if ( flDistNormal > 0 )
		{
			// the point is outside the triangle. Find the closest point and see if it's farther than the edge
			float flDistSqr0 = Sqr( x ) + Sqr( y );
			float flDistSqr1 = Sqr( x  - m_flD1x ) + Sqr( y );
			float flDistSqr2 = Sqr( x - m_D2.x ) + Sqr( y - m_D2.y );
			float flVertDistSqr = Min( flDistSqr0 , Min( flDistSqr1, flDistSqr2 ) ); // closest-vertex distance sqr
			float flDistEdgeSqr = flEdgePenetration * flEdgePenetration; // note: edge penetration = -distance; square removes the sign
			float flDistXYSqr = Max( flVertDistSqr, flDistEdgeSqr );
			float flDist = sqrtf( flDistXYSqr + Sqr( flDistNormal ) );
			if ( flDist < flSumNodeRadius )
			{
				refParticle += ( flSumNodeRadius - flDist ) * m_vNormal;
				//++m_nMovedNodes;
			}
		}
		else
		{
			// from behind the triangle, we'll just ignore the particle
		}
	}
};
#endif

void CSoftbody::CollideWithRigidsInternal()
{
	for ( uint nTapCap = m_pFeModel->m_nTaperedCapsuleStretchCount; nTapCap-- > 0; )
	{
		const FeTaperedCapsuleStretch_t &tc = m_pFeModel->m_pTaperedCapsuleStretches[ nTapCap ];

		const VectorAligned &vCenter1 = m_pPos1[ tc.nNode[ 1 ] ];
		AssertDbg( tc.flRadius[ 0 ] <= tc.flRadius[ 1 ] );
		if ( tc.nNode[ 0 ] == tc.nNode[ 1 ] )
		{
			CSphereColliderFunctor sphere( this, vCenter1, tc.flRadius[ 1 ] * m_flModelScale, tc.flStickiness, tc.nNode[ 1 ] );
			sphere.Collide( tc.nCollisionMask ); // this collision is a sphere
		}
		else
		{
			const VectorAligned &vCenter0 = m_pPos1[ tc.nNode[ 0 ] ];
			CTaperedCapsuleColliderFunctor capsule( this, vCenter0, tc.flRadius[ 0 ] * m_flModelScale, vCenter1, tc.flRadius[ 1 ] * m_flModelScale, tc.flStickiness, tc.nNode[ 0 ], tc.nNode[ 1 ] );
			capsule.Collide( tc.nCollisionMask );
		}
	}

	for ( uint nTapCap = m_pFeModel->m_nTaperedCapsuleRigidCount; nTapCap-- > 0; )
	{
		const FeTaperedCapsuleRigid_t &tc = m_pFeModel->m_pTaperedCapsuleRigids[ nTapCap ];
		const matrix3x4a_t &tm = GetAnim( m_pFeModel->NodeToCtrl( tc.nNode ) );
		CTaperedCapsuleColliderFunctor capsule( this, tm.TransformVector( tc.vCenter[ 0 ] * m_flModelScale ), tc.flRadius[ 0 ] * m_flModelScale, tm.TransformVector( tc.vCenter[ 1 ] * m_flModelScale ), tc.flRadius[ 1 ] * m_flModelScale, tc.flStickiness, tc.nNode, tc.nNode );
		capsule.Collide( tc.nCollisionMask );
	}
	for ( uint nSphere = m_pFeModel->m_nSphereRigidCount; nSphere-- > 0; )
	{
		const FeSphereRigid_t &sr = m_pFeModel->m_pSphereRigids[ nSphere ];
		const matrix3x4a_t &tm = GetAnim( m_pFeModel->NodeToCtrl( sr.nNode ) );
		CSphereColliderFunctor sphere( this, tm.TransformVector( sr.vCenter * m_flModelScale ), sr.flRadius* m_flModelScale, sr.flStickiness, sr.nNode );
		sphere.Collide( sr.nCollisionMask );
	}
}


void CSoftbody::SetCollisionAttributes( const RnCollisionAttr_t &attr )
{
	m_CollisionAttributes = attr;
}


//MPROF_NODE( NodeMeshProbes, "Softbody:Node-Tris", "Physics" );

void CSoftbody::CollideWithWorldInternal()
{
	uint8 nEnableWorldShapeCollision = m_nEnableWorldShapeCollision;
	if ( !( nEnableWorldShapeCollision && m_pEnvironment ) )
		return;

	// probe the broadphase
	const FeAabb_t &bbox = m_pAabb[ m_pFeModel->GetTreeRootAabbIndex() ]; // 
	CProxyVector proxies;
	CDynamicTree *pTree = m_pEnvironment->GetBroadphaseTree();
	if ( !pTree )
		return;
	pTree->Query( proxies, AsBounds3( bbox ) );
	const CFeModel *pFeModel = GetFeModel();
	for ( int ndx = 0; ndx < proxies.Count(); ++ndx )
	{
		int32 nProxy = proxies[ ndx ];
		CSoftbodyCollisionShape *pShape = ( CSoftbodyCollisionShape* )pTree->GetUserData( nProxy );

		uint16 nPairFlags = m_pEnvironment->m_Filter.TestSimulation( pShape->GetCollisionAttributes(), GetCollisionAttributes() );
		if ( !( nPairFlags & INTERSECTION_PAIR_RESOLVE_CONTACTS ) )
		{
			continue;  // We don't need to resolve contacts. And we don't really do anything else in cloth right now.
		}

		PhysicsShapeType_t nShapeType = pShape->GetType();
		if ( nEnableWorldShapeCollision & ( 1 << nShapeType ) )
		{
			switch ( nShapeType )
			{
				case SHAPE_SPHERE:
				{
					CSoftbodyCollisionSphere *pSphere = static_cast< CSoftbodyCollisionSphere * >( pShape );
					Vector vCenter = pSphere->GetCenter();
					float flRadius = pSphere->GetRadius() + pFeModel->m_flAddWorldCollisionRadius * m_flModelScale;
					fltx4 v4Center = LoadUnaligned3SIMD( &vCenter ), f4Radius = ReplicateX4( flRadius );

					CRnSphereColliderFunctor collider( vCenter, flRadius, m_pPos1 + pFeModel->m_nStaticNodes, pFeModel->m_pNodeCollisionRadii, m_flModelScale );
					FeAabb_t shapeAabb;
					shapeAabb.m_vMinBounds = v4Center - f4Radius;
					shapeAabb.m_vMaxBounds = v4Center + f4Radius;
					pFeModel->CastBox<CRnSphereColliderFunctor, true>( 0, shapeAabb, m_pAabb, collider );
				}
				break;

				case SHAPE_CAPSULE:
				{
					CSoftbodyCollisionCapsule *pCapsule = static_cast< CSoftbodyCollisionCapsule* >( pShape );

					const Vector &vCenter0 = pCapsule->GetCenter( 0 ), &vCenter1 = pCapsule->GetCenter( 1 );
					float flRadius = pCapsule->GetRadius() + pFeModel->m_flAddWorldCollisionRadius * m_flModelScale;
					fltx4 v4Center0 = LoadUnaligned3SIMD( &vCenter0 ), v4Center1 = LoadUnaligned3SIMD( &vCenter1 ), f4Radius = ReplicateX4( flRadius );

					CRnCapsuleColliderFunctor collider( vCenter0, vCenter1, flRadius, m_pPos1 + pFeModel->m_nStaticNodes, pFeModel->m_pNodeCollisionRadii, m_flModelScale );
					FeAabb_t shapeAabb;
					shapeAabb.m_vMinBounds = MinSIMD( v4Center0, v4Center1 ) - f4Radius;
					shapeAabb.m_vMaxBounds = MaxSIMD( v4Center0, v4Center1 ) + f4Radius;
					pFeModel->CastBox< CRnCapsuleColliderFunctor, true >( 0, shapeAabb, m_pAabb, collider );
				}
				break;
#ifdef SOURCE2
				case SHAPE_HULL:
				{
					CRnHullShape *pHull = static_cast< CRnHullShape* >( pShape );
					CRnHullColliderFunctor collider( pHull->GetHull(), pHull->GetScale(), pBody->GetTransform(), pFeModel->m_flAddWorldCollisionRadius * m_flModelScale, m_pPos1 + pFeModel->m_nStaticNodes, pFeModel->m_pNodeCollisionRadii, m_flModelScale );

					pFeModel->CastBox< CRnHullColliderFunctor, true >( 0, CFeModel::Aabb_t( collider.GetAabb() ), m_pAabb, collider );
				}
				break;

				case SHAPE_MESH:
				{

					CRnMeshShape *pMesh = static_cast< CRnMeshShape* >( pShape );
					// cheap test version
					CTriangleVector tris;
					float flAddRadius = pFeModel->m_flAddWorldCollisionRadius * m_flModelScale;
					CRnTriColliderFunctor collider( flAddRadius, m_pPos1 + pFeModel->m_nStaticNodes, pFeModel->m_pNodeCollisionRadii, m_flModelScale );
					Transform bodyTransform = pBody->GetTransform();
					FeAabb_t bboxClothForMesh = bbox; // blow up the cloth for mesh query
					fltx4 f4AddWorldCollisionRadius = ReplicateX4( flAddRadius );
					bboxClothForMesh.AddExtents( f4AddWorldCollisionRadius );
					pMesh->Query( tris, TMul( bodyTransform, AsBounds3( bboxClothForMesh ) ) );
					//MPROF_AUTO_FAST( NodeMeshProbes );
					for ( uint32 nTri : tris )// for ( int nTri = 0; nTri < pMesh->GetMesh()->m_Triangles.Count(); ++nTri )
					{
						// float flTransientError = pFeModel->ComputeCollisionTreeBoundsError( m_pPos1 + pFeModel->m_nStaticNodes, m_pAabb ); // we move nodes around, so they'll be slightly outside of their respective parents' bboxes, but hopefully that's ok. Updating bboxes is possible, but adds a factor of logN to the complexity, or an extra linear search at the end of the overlap query
						collider.PrepareTriangle( pMesh, nTri, bodyTransform );
						CFeModel::Aabb_t bboxCollider = collider.GetAabb();
						bboxCollider.AddExtents( f4AddWorldCollisionRadius ); // triangle is thin, particles are thin, we need to add some thickness to one or the other. Can't add it to the particle tree without increasing complexity of the query, so I will have to add it to triangle
						pFeModel->CastBox< CRnTriColliderFunctor, true >( 0, bboxCollider, m_pAabb, collider ); // for ( uint nDynNode = 0; nDynNode < pFeModel->GetDynamicNodeCount(); ++nDynNode ) collider( nDynNode );
					}
					//MPROF_SET_COUNT( NodeMeshProbes, collider.m_nProbedNodes );
				}
				break;
#endif
			}
		}
	}
}


bool CSoftbody::AdvanceSleepCounter()
{
	if ( m_nStateCounter++ > FRAMES_INVISIBLE_BEFORE_DORMANT )
	{
		GoDormant();
		return true;
	}
	else
	{
		return false;
	}
}


CSoftbody::CConstraintIterator::CConstraintIterator( CSoftbody *pSoftbody )
{
	m_pSoftbody = pSoftbody;
	m_pEnvironment = pSoftbody->m_pEnvironment;

	if ( pSoftbody->m_pFeModel->m_pLegacyStretchForce || ( pSoftbody->m_pFeModel->m_nDynamicNodeFlags & FE_FLAG_UNINERTIAL_CONSTRAINTS ) )
	{
		m_PosBeforeCorrect.CopyArray( pSoftbody->m_pPos1, pSoftbody->m_nNodeCount );
	}
}



const float g_flClothLegacyStretchForce = 0.95f;// this only matters for Dota legacy content

CSoftbody::CConstraintIterator::~CConstraintIterator()
{
	if ( !m_PosBeforeCorrect.IsEmpty( ) )
	{
		//CRnSoftbodyChangeGuard cg( m_pSoftbody, "~CConstraintIterator" );
		const CFeModel *pFeModel = m_pSoftbody->m_pFeModel;
		VectorAligned *pPos0 = m_pSoftbody->m_pPos0, *pPos1 = m_pSoftbody->m_pPos1;
		// In Source1, static particle velocities are always reset to 0, see CClothParticleState::Integrate()
		uint nStaticNodes = pFeModel->m_nStaticNodes;
		for ( uint nNode = 0; nNode < nStaticNodes; ++nNode )
		{
			pPos0[ nNode ] = pPos1[ nNode ];
		}

		if ( const float *pLegacyStretchForce = m_pSoftbody->m_pFeModel->m_pLegacyStretchForce )
		{
			AssertDbg( pFeModel->m_nDynamicNodeFlags & FE_FLAG_UNINERTIAL_CONSTRAINTS );
			float dt = m_pSoftbody->m_flLastTimestep;
			for ( uint nNode = pFeModel->m_nStaticNodes, nNodeCount = pFeModel->m_nNodeCount; nNode < nNodeCount; ++nNode )
			{
				float flLegacyStretchForce = pLegacyStretchForce[ nNode ] * dt; // 0.0 means no velocity at all will be created by solving any constraints; 1.0 means full velocity will be applied

				pPos0[ nNode ] += ( pPos1[ nNode ] - m_PosBeforeCorrect[ nNode ] ) * clamp( 1.0f - flLegacyStretchForce, 0.0f, g_flClothLegacyStretchForce );
			}
		}
		else
		{
			AssertDbg( pFeModel->m_nDynamicNodeFlags & FE_FLAG_UNINERTIAL_CONSTRAINTS ); // the only other useful case
			for ( uint nNode = pFeModel->m_nStaticNodes, nNodeCount = pFeModel->m_nNodeCount; nNode < nNodeCount; ++nNode )
			{
				pPos0[ nNode ] += ( pPos1[ nNode ] - m_PosBeforeCorrect[ nNode ] );
			}
		}
	}
}




void CSoftbody::CConstraintIterator::Iterate( int nIterations )
{
	VectorAligned /*pPos0 = m_pSoftbody->m_pPos0, */*pPos1 = m_pSoftbody->m_pPos1;
	const CFeModel *pFeModel = m_pSoftbody->m_pFeModel;

	float flRodStiffness = expf( -m_pSoftbody->m_flThreadStretch / nIterations );
	float flSurfaceStiffness = expf( -m_pSoftbody->m_flSurfaceStretch / nIterations );
	float flConstraintScale = m_pSoftbody->m_flModelScale * m_pSoftbody->m_flClothScale;
	bool bEnableSimd = m_pSoftbody->m_bEnableSimd;
	for ( int nIteration = 0; nIteration < nIterations; ++nIteration )
	{
		if ( flRodStiffness > 0.01f )
		{
			MPROF_AUTO_FAST( FeRelaxRods );
			if ( m_pSoftbody->m_bEnableFtlPass && nIteration + 1 == nIterations )
			{
				if ( bEnableSimd )
				{
					pFeModel->RelaxSimdRodsFtl( ( fltx4* ) pPos1, flRodStiffness, flConstraintScale );
				}
				else
				{
					pFeModel->RelaxRods2Ftl( pPos1, flRodStiffness, flConstraintScale );
				}
			}
			else
			{
				if ( bEnableSimd )
				{
					pFeModel->RelaxSimdRods( ( fltx4* ) pPos1, flRodStiffness, flConstraintScale );
				}
				else
				{
					pFeModel->RelaxRods2( pPos1, flRodStiffness, flConstraintScale );
				}
			}
		}
		pFeModel->RelaxBend( pPos1, flRodStiffness );
		{
			MPROF_AUTO_FAST_COUNT( FeRelaxQuads, bEnableSimd ? pFeModel->m_nSimdQuadCount[ 0 ] * 4 : pFeModel->m_nQuadCount[ 0 ] );
			if ( bEnableSimd )
			{
				pFeModel->RelaxSimdQuads( pPos1, flSurfaceStiffness, flConstraintScale, m_pSoftbody->m_nSimFlags & SOFTBODY_SIM_EXPERIMENTAL_1 );
				pFeModel->RelaxSimdTris( pPos1, flSurfaceStiffness, flConstraintScale );
			}
			else
			{
				pFeModel->RelaxQuads( pPos1, flSurfaceStiffness, flConstraintScale, m_pSoftbody->m_nSimFlags & SOFTBODY_SIM_EXPERIMENTAL_1 );
				pFeModel->RelaxTris( pPos1, flSurfaceStiffness, flConstraintScale );
			}
		}
	}
}



AABB_t CSoftbody::BuildBounds( )const
{
	fltx4 f4MinBounds, f4MaxBounds;
	if ( m_pAabb )
	{
		const FeAabb_t &bbox = m_pAabb[ m_pFeModel->GetTreeRootAabbIndex() ];
		f4MinBounds = bbox.m_vMinBounds;
		f4MaxBounds = bbox.m_vMaxBounds;
	}
	else
	{
		f4MinBounds = Four_FLT_MAX;
		f4MaxBounds = Four_Negative_FLT_MAX;
		for ( uint nNode = 0; nNode < m_nNodeCount; nNode++ )
		{
			fltx4 pos = LoadAlignedSIMD( m_pPos1[ nNode ].Base() );
			f4MinBounds = MinSIMD( f4MinBounds, pos );
			f4MaxBounds = MaxSIMD( f4MaxBounds, pos );
		}
	}
	AABB_t result;
	result.m_vMinBounds = AsVector( f4MinBounds );
	result.m_vMaxBounds = AsVector( f4MaxBounds );
	return result;
}


Quaternion PartialRotation( const Quaternion &q, float f )
{
	Assert( q.w >= 0 );
	Quaternion p = q;
	p.x *= f;
	p.y *= f;
	p.z *= f;
	p.w = sqrtf( Max( 0.0f, 1.0f - Sqr( q.x ) + Sqr( q.y ) + Sqr( q.z ) ) );
	return p;
}


void CSoftbody::SetAbsAngles( const QAngle &vNewAngles, bool bTeleport )
{
	if ( m_vSimAngles == vNewAngles )
		return;
	CHANGE_GUARD();

	float flLocalSpaceCloth = ( m_nAnimSpace == SOFTBODY_ANIM_SPACE_LOCAL ) ? 1.0f : 0.0f;
	if ( m_pFeModel->m_pLocalRotation && !bTeleport )
	{
		Quaternion simTarget = AngleQuaternion( vNewAngles );
		Quaternion qDelta( simTarget * Conjugate( AngleQuaternion( m_vSimAngles ) ) );
		// qDelta is small in most cases; we could compute the approximate exp( flLocalRotation * lo( qDelta ) ) here
		// since x,y,z of a quaternion is sin( theta/2 ) * axis, we can just scale the axis
		if ( qDelta.w < 0 )
			qDelta = -qDelta;
		Vector vSimOrigin = m_vSimOrigin;

		for ( int nNode = m_pFeModel->m_nStaticNodes; nNode < m_nNodeCount; ++nNode )
		{
			float flLocalRotation = m_pFeModel->m_pLocalRotation[ nNode - m_pFeModel->m_nStaticNodes ] - flLocalSpaceCloth;
			if ( flLocalRotation > 0 )
			{
				m_pPos0[ nNode ] = VectorRotate( m_pPos0[ nNode ] - vSimOrigin, PartialRotation( qDelta, flLocalRotation ) ) + vSimOrigin;
				m_pPos1[ nNode ] = VectorRotate( m_pPos1[ nNode ] - vSimOrigin, PartialRotation( qDelta, flLocalRotation ) ) + vSimOrigin;
			}
		}
		m_bSimTransformsOutdated = true;
	}
	else
	{
		float flLocalRotation = m_pFeModel->m_flLocalRotation - flLocalSpaceCloth;

		if ( flLocalRotation != 0.0f || bTeleport )
		{
			Quaternion simTarget = AngleQuaternion( vNewAngles );
			Quaternion qDelta( simTarget * Conjugate( AngleQuaternion( m_vSimAngles ) ) );
			// qDelta is small in most cases; we could compute the approximate exp( flLocalRotation * lo( qDelta ) ) here
			// since x,y,z of a quaternion is sin( theta/2 ) * axis, we can just scale the axis
			if ( qDelta.w < 0 )
				qDelta = -qDelta;

			matrix3x4a_t transform = QuaternionMatrix( bTeleport ? qDelta : PartialRotation( qDelta, flLocalRotation ) );
			if ( m_nAnimSpace != SOFTBODY_ANIM_SPACE_LOCAL )
			{
				transform.SetOrigin( m_vSimOrigin - VectorRotate( m_vSimOrigin, transform ) );
			}

			for ( int nNode = 0; nNode < m_nNodeCount; ++nNode )
			{
				m_pPos0[ nNode ] = VectorTransform( m_pPos0[ nNode ], transform ); // equivalent to : VectorRotate( m_pPos0[ nNode ] - m_vSimOrigin, transform ) + m_vSimOrigin;
				m_pPos1[ nNode ] = VectorTransform( m_pPos1[ nNode ], transform ); // equivalent to : VectorRotate( m_pPos1[ nNode ] - m_vSimOrigin, transform ) + m_vSimOrigin;
			}
			m_bSimTransformsOutdated = true;
		}
	}
	m_vSimAngles = vNewAngles;
	if ( flLocalSpaceCloth == 1.0f )
	{
		m_vRopeOffset = Vector( 0, -1, 0 );
	}
	else
	{
		AngleVectors( vNewAngles, NULL, &m_vRopeOffset, NULL );
		m_vRopeOffset *= g_flRopeSize;
	}
}



matrix3x4_t CSoftbody::GetDifferenceTransform( const Vector &vAltOrigin, const QAngle &vAltAngles )
{
	Quaternion simTarget = AngleQuaternion( vAltAngles );
	Quaternion qDelta( simTarget * Conjugate( AngleQuaternion( m_vSimAngles ) ) );

	matrix3x4a_t transform = QuaternionMatrix( qDelta );
	transform.SetOrigin( vAltOrigin - VectorRotate( m_vSimOrigin, transform ) );
	return transform;
}


void CSoftbody::ComputeInterpolatedNodePositions( float flFactor, VectorAligned *pPosOut )
{
	for ( uint nNode = 0; nNode < m_nNodeCount; ++nNode )
	{
		pPosOut[ nNode ] = m_pPos1[ nNode ] * flFactor + m_pPos0[ nNode ] * ( 1 - flFactor );
	}
}



void CSoftbody::DebugTraceMove( const char *pMsg )
{
	Vector d = m_pPos1[ 0 ] - m_vSimOrigin;
	Msg( "%s a1 :{r=%.1f,a=%.1f}\n", pMsg, sqrtf( d.x * d.x + d.y * d.y ), RAD2DEG( atan2f( d.y, d.x ) ) - m_vSimAngles.y );
	//Msg( "%s d1 {%.1f,%.1f}\n", pMsg, d.x, d.y );
}


void CSoftbody::SetAbsOrigin( const Vector &vNewOrigin, bool bTeleport )
{
/*
	if ( m_bTeleportOnNextSetAbsOrigin )
	{
		if ( m_nActivityState == STATE_ACTIVE )
		{
			m_bTeleportOnNextSetAbsOrigin = false;
		}
		bTeleport = true;
	}

*/
	if ( m_vSimOrigin == vNewOrigin )
		return;
	CHANGE_GUARD();

	Vector vDelta = vNewOrigin - m_vSimOrigin;
	float flLocalSpaceCloth = ( m_nAnimSpace == SOFTBODY_ANIM_SPACE_LOCAL ) ? 1.0f : 0.0f;
	float flFractionTeleport = -flLocalSpaceCloth, flIsFullTeleport = 0.0f;
	// m_pFeModel->m_flLocalForce == 0 means that moving absOrigin should not affect the m_Pos arrays

	if ( bTeleport || m_pFeModel->m_flLocalForce <= 0.0f )
	{
		// if we're told to teleport - either by explicit flag, or by "LocalForce" == 0, then
		// teleport, not matter how close the new position is
		flIsFullTeleport = flFractionTeleport = 1.0f - flLocalSpaceCloth;
	}
	else 
	{
		float flDeltaLenSq = vDelta.LengthSqr( );
		
		// which fraction of this delta should we move the particles? If it's 200 inches, source1 assumes teleportation
		AssertDbg( g_flNoTeleportDeltaSq < g_flTeleportDeltaSq );
		
		flIsFullTeleport = ( flDeltaLenSq - g_flNoTeleportDeltaSq ) * ( 1.0f / ( g_flTeleportDeltaSq - g_flNoTeleportDeltaSq ) );

		flFractionTeleport = clamp( flIsFullTeleport, 1.0f - m_pFeModel->m_flLocalForce, 1 ) - flLocalSpaceCloth;
	}

	// WARNING flIsFullTeleport goes from -Big to +Big, but it's logicaly clamped to 0,1

	if ( flFractionTeleport != 0 )
	{
		// Note: when we move a local-space entity (such as in Hammer), we still simulate and animate in entity space, not world space. This means entity's cloth moves in the opposite direction to moving the entity itself.
		Vector vPartialDelta = vDelta * flFractionTeleport;
		if ( m_pFeModel->m_pLocalForce && flFractionTeleport < 1.0f )
		{
			AssertDbg( m_pFeModel->m_flLocalForce > 0.0f );
			uint nStaticNodes = m_pFeModel->m_nStaticNodes;
			for ( int nNode = 0; nNode < nStaticNodes; ++nNode )
			{
				m_pPos0[ nNode ] += vPartialDelta;
				m_pPos1[ nNode ] += vPartialDelta;
			}
			for ( int nNode = nStaticNodes; nNode < m_nNodeCount; ++nNode )
			{
				Vector vPartialDelta = vDelta * ( clamp( flIsFullTeleport, 1.0f - m_pFeModel->m_pLocalForce[ nNode - nStaticNodes ] - flLocalSpaceCloth, 1.0f ) );
				m_pPos0[ nNode ] += vPartialDelta;
				m_pPos1[ nNode ] += vPartialDelta;
			}
		}
		else
		{
			for ( int nNode = 0; nNode < m_nNodeCount; ++nNode )
			{
				m_pPos0[ nNode ] += vPartialDelta;
				m_pPos1[ nNode ] += vPartialDelta;
			}
		}

		m_bSimTransformsOutdated = true;
	}

	m_vSimOrigin = vNewOrigin;
	if ( m_bEnableGroundTrace  // 
		 && m_pFeModel->m_nWorldCollisionNodeCount // no need to trace the ground plane if we don't have any world-collision nodes
		 && Sqr( m_vGround.x - vNewOrigin.x ) + Sqr( m_vGround.y - vNewOrigin.y ) > 16 * 16 // no need to retrace the ground plane until we move away at least 1 ft
	)
	{
#ifdef SOURCE2_SUPPORT
		// we have world collision nodes, let's update the ground plane
		PhysicsTrace_t trace;
		Vector vRayStart = vNewOrigin + Vector( 0,0, m_pEnvironment->GetSoftbodyGroundTraceRaise() ), vRayDelta( 0, 0, -200 );
		RnQueryAttr_t attr;
		m_pEnvironment->CastRaySingle( trace, vRayStart, vRayDelta, SELECT_STATIC, attr );
		if ( trace.DidHit( ) )
		{
			m_vGround.z = trace.m_vHitPoint.z + g_flClothGroundPlaneThickness.GetFloat();
		}
		else
		{
			m_vGround.z = vRayStart.z + vRayDelta.z;
		}
		m_vGround.x = m_vSimOrigin.x;
		m_vGround.y = m_vSimOrigin.y;
#endif
	}
	DebugDump();
}




static float s_flTransformErrorTolerance = 0.001f;


/*
bool CSoftbody::SetupCtrl( uint nCtrl, matrix3x4a_t &writeBone )
{
	if ( !BeforeFilterTransforms() )
		return false;
	MPROF_AUTO_FAST( SoftbodyFilterTransforms );
	GetAnim( nCtrl ) = writeBone;
	// just a marker to let the next simulation step know we need to update static particles
	m_bAnimTransformChanged = true;
	float flMatrixScale = m_flModelScale;
	//float flInvScale = 1.0f / flMatrixScale;
	// GetSim( nCtrl ) = ScaleMatrix3x3( writeBone, flInvScale ); ?????

	///////
	// recompute Sim particle rotations if needed (if we simulated particles since the last Filter was called)
	matrix3x4a_t *pSim = GetParticleTransforms( );
	uint nNode = m_pFeModel->m_pCtrlToNode[ nCtrl ];
	///////
	// copy all the dynamic sim transforms (computed in GetParticleTransforms()) into outWorld
	// we cannot do that in the same loop with the parent transforms because the order of controls (sim order) is independent of the animation bone order, so parent-to-child ordering is not preserved
	if( nNode >= m_pFeModel->m_nRotLockStaticNodes )
	{
		AssertDbg( / *nNode > nNodeCount - m_pFeModel->m_nFitMatrixCount || * /IsGoodWorldTransform( pSim[ nCtrl ] ) ); // the last N nodes are FitMatrices, which may have non-uniform scale at some point
		writeBone = ScaleMatrix3x3( pSim[ nCtrl ], flMatrixScale );
		return true;
	}
	else
	{
		return false;
	}

	///////
	// now that we have copied all the world transforms back and forth between internal and external caches,
	// compute the corresponding parent transforms.
	///	outWorldBone = ScaleMatrix3x3( pSim[ nCtrl ], flMatrixScale ); ????????
}
*/





void CSoftbody::FilterTransforms( const FilterTransformsParams_t &params )
{
// 	if ( IsDebug() && m_nIndexInWorld == g_nClothWatch)
// 	{
// 		AABB_t box0 = GetAabb( GetNodePositions(), m_nNodeCount );
// 		char * states[] = { "Active", "Dormant", "Waking up" };
// 		Log_Msg( LOG_PHYSICS, "Cloth %d %s FilterTransforms({%.0f,%.0f}\xB1{%.0f,%.0f})\n", m_nIndexInWorld, states[ m_nActivityState ], box0.GetCenter().x, box0.GetCenter().y, box0.GetSize().x, box0.GetSize().y );
// 	}
	if ( !BeforeFilterTransforms() )
		return;

	MPROF_AUTO_FAST( SoftbodyFilterTransforms );

	///////
	// just a marker to let the next simulation step know we need to update static particles
	m_bAnimTransformChanged = true; 

	float flMatrixScale = ( params.flMatrixScale == 0.0f ? m_flModelScale : params.flMatrixScale );

	if ( ( m_bSimTransformsOutdated || params.pNodePos ) && params.pCtrlToBone )
	{
		float flInvScale = 1.0f / flMatrixScale;
		for ( uint nNode = 0; nNode < m_pFeModel->m_nStaticNodes; ++nNode )
		{
			uint nCtrl = m_pFeModel->NodeToCtrl( nNode );
			int nBone = params.pCtrlToBone[ nCtrl ];
			if ( nBone >= 0 )
			{
				AssertDbg( !params.pValidTransforms || BitVec_IsBitSet( params.pValidTransforms, nBone ) );
				GetSim( nCtrl ) = ScaleMatrix3x3( params.pOutputWorldTransforms[ nBone ], flInvScale );
			}
		}
	}

	const VectorAligned *pNodePos = params.pNodePos;

	///////
	// recompute Sim particle rotations if needed (if we simulated particles since the last Filter was called)
	matrix3x4a_t *pSim = GetParticleTransforms( pNodePos );
	uint nNodeCount = m_pFeModel->m_nNodeCount;
	///////
	// copy all the dynamic sim transforms (computed in GetParticleTransforms()) into outWorld
	// we cannot do that in the same loop with the parent transforms because the order of controls (sim order) is independent of the animation bone order, so parent-to-child ordering is not preserved
	for ( uint nNode = m_pFeModel->m_nRotLockStaticNodes; nNode < nNodeCount; ++nNode )
	{
		uint nCtrl = m_pFeModel->NodeToCtrl( nNode ); AssertDbg( nCtrl < m_nParticleCount );
		int nBone = params.pCtrlToBone[ nCtrl ];
		if ( nBone >= 0 )
		{
			matrix3x4_t &refWorldBone = params.pOutputWorldTransforms[ nBone ];
			AssertDbg( /*nNode > nNodeCount - m_pFeModel->m_nFitMatrixCount || */IsGoodWorldTransform( pSim[ nCtrl ] ) ); // the last N nodes are FitMatrices, which may have non-uniform scale at some point
			refWorldBone = ScaleMatrix3x3( pSim[ nCtrl ], flMatrixScale );
		}
	}



	///////
	// now that we have copied all the world transforms back and forth between internal and external caches,
	// compute the corresponding parent transforms.
	for ( uint nNode = m_pFeModel->m_nRotLockStaticNodes; nNode < nNodeCount; ++nNode )
	{
		uint nCtrl = m_pFeModel->NodeToCtrl( nNode );
		int nBone = params.pCtrlToBone[ nCtrl ]; // Note: if we don't want to overwrite static (rotate-locked) bones, we can just put -1 into the corresponding entries of this map
		if ( nBone < 0 )
			continue; // this is a virtual node (does not have a corresponding bone)

		AssertDbg( m_pFeModel->CtrlToNode( nCtrl ) < m_nNodeCount );
		matrix3x4a_t &outWorldBone = params.pOutputWorldTransforms[ nBone ];
		outWorldBone = ScaleMatrix3x3( pSim[ nCtrl ], flMatrixScale );
	}
}


void CSoftbody::FilterTransforms( matrix3x4a_t *pModelBones )
{
	if ( !BeforeFilterTransforms() )
		return;

	MPROF_AUTO_FAST( SoftbodyFilterTransforms );

	///////
	// just a marker to let the next simulation step know we need to update static particles
	m_bAnimTransformChanged = true;

	float flMatrixScale = m_flModelScale ;

	if ( m_bSimTransformsOutdated )
	{
		float flInvScale = 1.0f / flMatrixScale;
		for ( uint nNode = 0; nNode < m_pFeModel->m_nStaticNodes; ++nNode )
		{
			uint nCtrl = m_pFeModel->NodeToCtrl( nNode );
			int nBone = m_pCtrlToModelBone[ nCtrl ];
			if ( nBone >= 0 )
			{
				GetSim( nCtrl ) = ScaleMatrix3x3( pModelBones[ nBone ], flInvScale );
			}
		}
	}

	const VectorAligned *pNodePos = NULL;

	///////
	// recompute Sim particle rotations if needed (if we simulated particles since the last Filter was called)
	matrix3x4a_t *pSim = GetParticleTransforms( pNodePos );
	uint nNodeCount = m_pFeModel->m_nNodeCount;
	///////
	// copy all the dynamic sim transforms (computed in GetParticleTransforms()) into outWorld
	// we cannot do that in the same loop with the parent transforms because the order of controls (sim order) is independent of the animation bone order, so parent-to-child ordering is not preserved
	for ( uint nNode = m_pFeModel->m_nRotLockStaticNodes; nNode < nNodeCount; ++nNode )
	{
		uint nCtrl = m_pFeModel->NodeToCtrl( nNode ); AssertDbg( nCtrl < m_nParticleCount );
		int nBone = m_pCtrlToModelBone[ nCtrl ];
		if ( nBone >= 0 )
		{
			matrix3x4_t &refWorldBone = pModelBones[ nBone ];
			AssertDbg( /*nNode > nNodeCount - m_pFeModel->m_nFitMatrixCount || */IsGoodWorldTransform( pSim[ nCtrl ] ) ); // the last N nodes are FitMatrices, which may have non-uniform scale at some point
			refWorldBone = ScaleMatrix3x3( pSim[ nCtrl ], flMatrixScale );
		}
	}
}


bool CSoftbody::IsDormant() const
{
	return m_nActivityState == STATE_DORMANT;
}

bool CSoftbody::IsActive() const
{
	return m_nActivityState == STATE_ACTIVE;
}

void CSoftbody::GoDormant( )
{
	if ( m_nActivityState == STATE_ACTIVE )
	{
// 		if ( IsDebug() && m_nIndexInWorld == g_nClothWatch )
// 		{
// 			Log_Msg( LOG_PHYSICS, "Cloth %d goes dormant\n", m_nIndexInWorld );
// 		}
		m_nActivityState = STATE_DORMANT;
	}
/*
	m_bTeleportOnNextSetAbsOrigin = true;
	m_bTeleportOnNextSetAbsAngles = true;
*/
	//Log_Detailed( LOG_PHYSICS, "Softbody::GoDormant(%s, skel %p)\n", GetDebugName( ), GetUserData( 0 ) );
}


void CSoftbody::GoWakeup( )
{
	if ( IsDormant( ) )
	{
// 		if ( IsDebug() && m_nIndexInWorld == g_nClothWatch )
// 		{
// 			Log_Msg( LOG_PHYSICS, "Cloth %d wakes up\n", m_nIndexInWorld );
// 		}
		m_nActivityState = STATE_ACTIVE;
		m_nStateCounter = 0;
/*
		m_bTeleportOnNextSetAbsOrigin = true;
		m_bTeleportOnNextSetAbsAngles = true;
*/
	}
	//Log_Detailed( LOG_PHYSICS, "Softbody::GoWakeup(%s, skel %p)\n", GetDebugName( ), GetUserData( 0 ) );
}

bool CSoftbody::BeforeFilterTransforms( )
{
	switch ( m_nActivityState )
	{
	case STATE_ACTIVE:
		m_nStateCounter = 0; // we've simulated 0 frames since last FilterTransforms
		return true; // yes, we need to FilterTranforms
	
	case STATE_WAKEUP:
		// we do not need to Filter Transforms yet, but will shortly
		// StateCounter == 0  :  we definitely do NOT want to filter transforms, we have the wrong transforms and they will stretch the cloth visually
		// StateCounter  > 0  :  we MAY filter transforms, but they are not simulating yet. Maybe we need to do it anyway to account for discrepancies pre- and post-filter
		return m_nStateCounter > 0;

	default:
		GoWakeup( );
		return false; 
	}
}

ConVar cloth_wind( "cloth_wind", "0" );
ConVar cloth_windage_multiplier( "cloth_windage_multiplier", "1", FCVAR_CHEAT );
ConVar cloth_wind_pitch( "cloth_wind_pitch", "0" );
void CSoftbody::IntegrateWind( VectorAligned *pPos, float flTimeStep )
{
	{
		const Vector4DAligned &vWindDesc = m_pEnvironment->GetWindDesc();
		float flNormalPressure = m_pFeModel->m_flWindage * cloth_windage_multiplier.GetFloat() * flTimeStep * vWindDesc.w;
		if ( flNormalPressure != 0 )
		{
			m_pFeModel->ApplyQuadWind( pPos, vWindDesc.AsVector3D() * flNormalPressure, m_pFeModel->m_flWindDrag );
		}
	}
	{
		float flDebugWind = cloth_wind.GetFloat();
		if ( flDebugWind != 0 )
		{
			Vector vDebugWindVector;
			QAngle vecWindAngle( 0, cloth_wind_pitch.GetFloat(), 0 );
			AngleVectors( vecWindAngle, &vDebugWindVector );
			float flDebugWindPressure = m_pFeModel->m_flWindage * flTimeStep * flDebugWind;
			if ( flDebugWindPressure != 0 )
			{
				m_pFeModel->ApplyQuadWind( pPos, vDebugWindVector * flDebugWindPressure, m_pFeModel->m_flWindDrag );
			}
		}
	}
}


// add acceleration components
void CSoftbody::Integrate( float flTimeStep )
{
	if ( m_bEnableSprings )
	{
		MPROF_AUTO_FAST( SoftbodyStepIntegrate );
		m_pFeModel->IntegrateSprings( m_pPos0, m_pPos1, flTimeStep, m_flModelScale );
	}

	
	IntegrateWind( m_pPos1, flTimeStep );

	AssertDbg( m_flExpAirDrag >= 0 && m_flVelAirDrag >= 0 );
	if ( m_flExpAirDrag + m_flVelAirDrag != 0 )
	{
		m_pFeModel->ApplyAirDrag( m_pPos0, m_pPos1, 1.0f - expf( -m_flExpAirDrag * flTimeStep ), m_flVelAirDrag );
	}

	AssertDbg( m_flExpQuadAirDrag >= 0 && m_flVelQuadAirDrag >= 0 );
	if ( m_flExpQuadAirDrag + m_flVelQuadAirDrag != 0 )
	{
		m_pFeModel->ApplyQuadAirDrag( ( fltx4* )m_pPos0, ( const fltx4* )m_pPos1, 1.0f - expf( -m_flExpQuadAirDrag * flTimeStep ), m_flVelQuadAirDrag );
	}

	AssertDbg( m_flExpRodAirDrag >= 0 && m_flVelRodAirDrag >= 0 );
	if ( m_flExpRodAirDrag + m_flVelRodAirDrag != 0 )
	{
		m_pFeModel->ApplyRodAirDrag( ( fltx4* )m_pPos0, ( const fltx4* )m_pPos1, 1.0f - expf( -m_flExpRodAirDrag * flTimeStep ), m_flVelRodAirDrag );
	}

	float flQuadVelocitySmoothRate = m_flQuadVelocitySmoothRate;
	if ( flQuadVelocitySmoothRate != 0 )
	{
		float flMul = 1.0f - clamp( flQuadVelocitySmoothRate, 0, 1 );
		for ( int nIt = ( int )m_nQuadVelocitySmoothIterations; nIt-- > 0; )
		{
			m_pFeModel->SmoothQuadVelocityField( ( fltx4* )m_pPos0, ( const fltx4* )m_pPos1, flMul );
		}
	}
	float flRodVelocitySmoothRate = m_flRodVelocitySmoothRate;
	if ( flRodVelocitySmoothRate != 0 )
	{
		float flMul = 1.0f - clamp( flRodVelocitySmoothRate, 0, 1 );
		for ( int nIt = ( int )m_nRodVelocitySmoothIterations; nIt-- > 0; )
		{
			m_pFeModel->SmoothRodVelocityField( ( fltx4* )m_pPos0, ( const fltx4* )m_pPos1, flMul );
		}
	}
}


inline fltx4 LoadOriginAligned( const matrix3x4a_t &tm )
{
	const fltx4 &x = tm.SIMDRow( 0 ), &y = tm.SIMDRow( 1 ), &z = tm.SIMDRow( 2 );
	fltx4 nnxy = _mm_unpackhi_ps( x, y ), xyzz = _mm_shuffle_ps( nnxy, z, MM_SHUFFLE_REV( 2, 3, 3, 3 ) );
	return xyzz;
}



// Note: animation attraction (neither positional nor velocity) should NOT be affected immediately by the damping. Source1 crazy explicit-Euler-ish integration scheme 
// bypasses damping when computing animation attraction, folding it into the "Force" that affects velocity, that will affect force again, but not until the next frame.
// This scheme is unstable in multiple subtle ways, but artists learned to work around it by tweaking numbers.
void CSoftbody::AddAnimationAttraction( float flTimeStep )
{
	const uint nStaticNodes = m_pFeModel->m_nStaticNodes;
	const FeNodeIntegrator_t *pNodeIntegrator = m_pFeModel->m_pNodeIntegrator;
	// <sergiy> this is source1's AnimationVertexAttraction simulation. The values are always very small in Dota, so it's not really necessary
	if ( m_bEnableAnimationAttraction && pNodeIntegrator && ( m_pFeModel->m_nDynamicNodeFlags & ( FE_FLAG_HAS_ANIMATION_VERTEX_ATTRACTION | FE_FLAG_HAS_ANIMATION_FORCE_ATTRACTION ) ) )
	{
		if ( m_bEnableSimd )
		{
			//fltx4 f4TimeStep = ReplicateX4( flTimeStep );
			for ( uint nDynNode = nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
			{
				const FeNodeIntegrator_t &integrator = pNodeIntegrator[ nDynNode ];
				fltx4 &pos0 = ( fltx4& )m_pPos0[ nDynNode ], &pos1 = ( fltx4& )m_pPos1[ nDynNode ];

				uint nCtrl = m_pFeModel->NodeToCtrl( nDynNode );
				fltx4 vAnimationTarget = LoadOriginAligned( GetAnim( nCtrl ) ), vAnimDelta = vAnimationTarget - pos1;

				// the position attraction does not involve velocity and is inertia-less. That's why we add the delta to both previous and current time stp
				fltx4 vPositionAttraction = vAnimDelta * ReplicateX4( Min( 1.0f, integrator.flAnimationVertexAttraction * flTimeStep * g_flClothAttrPos ) );
				fltx4 vVelocityAttraction = vAnimDelta * ReplicateX4( integrator.flAnimationForceAttraction * flTimeStep * g_flClothAttrVel );
				pos0 += vPositionAttraction;
				pos1 += vPositionAttraction + vVelocityAttraction;
			}
		}
		else
		{
			for ( uint nDynNode = nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
			{
				const FeNodeIntegrator_t &integrator = pNodeIntegrator[ nDynNode ];
				VectorAligned &pos0 = m_pPos0[ nDynNode ], &pos1 = m_pPos1[ nDynNode ];

				uint nCtrl = m_pFeModel->NodeToCtrl( nDynNode );
				VectorAligned vAnimationTarget( GetAnim( nCtrl ).GetOrigin( ) ), vAnimDelta( vAnimationTarget - pos1 );

				// the position attraction does not involve velocity and is inertia-less. That's why we add the delta to both previous and current time stp
				Vector vPositionAttraction( vAnimDelta * Min( 1.0f, integrator.flAnimationVertexAttraction * flTimeStep * g_flClothAttrPos ) );
				Vector vVelocityAttraction = vAnimDelta * ( integrator.flAnimationForceAttraction * flTimeStep * g_flClothAttrVel );
				pos0 += vPositionAttraction; 
				pos1 += vPositionAttraction + vVelocityAttraction;
			}
		}
		/*if ( m_nDebugNode < m_nNodeCount )
		{
			Vector vAnimTarget = GetAnim( m_pFeModel->NodeToCtrl( m_nDebugNode ) ).GetOrigin(), vPos0 = m_pPos0[ m_nDebugNode ], vPos1 = m_pPos1[ m_nDebugNode ];
			Vector vVel1 = ( vPos1 - vPos0 ) / m_flLastTimestep;
			Msg( "\t%.2f %.2f %.2f\t%.2f %.2f %.2f\t%.2f %.2f %.2f\n",
				( vAnimTarget - vPos0 ).x, ( vAnimTarget - vPos0 ).y, ( vAnimTarget - vPos0 ).z,
				vVel1.x, vVel1.y, vVel1.z,
				vPos1.x - vPos0.x, vPos1.y - vPos0.y, vPos1.z - vPos0.z
				);
		}*/
	}
}

class CGluePredictFunctor
{
	const CParticleGlue *m_pGlue;
	fltx4 *m_pPos0;
	fltx4 *m_pPos1;
	float m_flTimestepScale;
	uint m_nStaticNodes;
public:
	CGluePredictFunctor( const CParticleGlue *pGlue, fltx4 *pPos0, fltx4 *pPos1, float flTimestepScale, uint nStaticNodes )
		: m_pGlue( pGlue )
		, m_pPos0( pPos0 )
		, m_pPos1( pPos1 )
		, m_flTimestepScale( flTimestepScale )
		, m_nStaticNodes( nStaticNodes )
	{
	}

	void operator() ( uint nDynNode )const
	{
		const CParticleGlue &glue = m_pGlue[ nDynNode ];
		fltx4 vDelta = m_pPos1[ glue.m_nParentNode[ 0 ] ] - m_pPos0[ glue.m_nParentNode[ 0 ] ];
		if ( glue.m_flWeight1 > 0.0f )
		{
			AssertDbg( glue.m_flWeight1 <= 1.0f );
			fltx4 vDelta1 = m_pPos1[ glue.m_nParentNode[ 1 ] ] - m_pPos0[ glue.m_nParentNode[ 1 ] ];
			fltx4 f4Weight1 = ReplicateX4( glue.m_flWeight1 );
			vDelta = vDelta * ( Four_Ones - f4Weight1 ) + vDelta1 * f4Weight1;
		}
		vDelta *= ReplicateX4( m_flTimestepScale * glue.m_flStickiness );
		m_pPos1[ m_nStaticNodes + nDynNode ] += vDelta;
		m_pPos0[ m_nStaticNodes + nDynNode ] += vDelta;
	}
};


void CSoftbody::Predict( float flTimeStep )
{
	MPROF_AUTO_FAST( SoftbodyStepPredict );
	const uint nStaticNodes = m_pFeModel->m_nStaticNodes;
	const FeNodeIntegrator_t *pNodeIntegrator = m_pFeModel->m_pNodeIntegrator;

	float flTimestepScale = ( 1 - m_flVelocityDamping ) * ( 1 + m_flOverPredict ) * flTimeStep / Max( 0.25f * flTimeStep, m_flLastTimestep );

	fltx4 *pPos0 = ( fltx4* ) m_pPos0, *pPos1 = ( fltx4* ) m_pPos1;

	float flVelLim = g_flClothNodeVelocityLimit;
	if ( flVelLim < 1e5 )  // experimental
	{
		float flVelLimSqr = Sqr( flVelLim );
		for ( int nNode = 0; nNode < m_nNodeCount; ++nNode )
		{
			Vector vDelta = m_pPos1[ nNode ] - m_pPos0[ nNode ];
			float flLenSqr = vDelta.LengthSqr();
			if ( flLenSqr > flVelLimSqr )
			{
				m_pPos0[ nNode ] += ( 1.0f - sqrtf( flVelLimSqr / flLenSqr ) ) * vDelta;
			}
		}
	}

	if ( m_bAnimTransformChanged /*|| ( pNodeIntegrator && ( m_pFeModel->m_nStaticNodeFlags & FE_FLAG_HAS_ANIMATION_VERTEX_ATTRACTION ) )*/ )
	{
/*
		float flTimeStepAdjusted = flTimeStep * g_flClothAttrPos;
		if ( pNodeIntegrator )
		{
			for ( uint nStaticNode = 0; nStaticNode < nStaticNodes; ++nStaticNode )
			{
				fltx4 vTarget = LoadOriginAligned( GetAnim( m_pFeModel->NodeToCtrl( nStaticNode ) ) );
				float flAttract = pNodeIntegrator[ nStaticNode ].flAnimationVertexAttraction * flTimeStepAdjusted;
				fltx4 f4Attract = MinSIMD( ReplicateX4( flAttract ), Four_Ones );
				pPos0[ nStaticNode ] = f4Attract * vTarget + ( Four_Ones - f4Attract ) * pPos1[ nStaticNode ];
			}
		}
		else
*/
		// <sergiy> Note that the animation is copied into static nodes in Source1 in CClothModelPiece::SetupBone(), cloth_system.cpp#36:3397
		// it's probably a good idea to continue doing that regardless of the slight bug in source1 that overshot static bones slightly, unless artists unwittingly relied on that bug in some cases
		{
			for ( uint nStaticNode = 0; nStaticNode < nStaticNodes; ++nStaticNode )
			{
				uint nCtrl = m_pFeModel->NodeToCtrl( nStaticNode );
				Assert( nCtrl < m_nParticleCount );
				fltx4 vTarget = LoadOriginAligned( GetAnim( nCtrl ) );
				pPos0[ nStaticNode ] = vTarget;
			}
		}
		m_bAnimTransformChanged = false;
	}
	else
	{
		// since we're double-buffering the positions, we need to copy the positions for at least one frame
		// after animation stopped updating them. We can add another flag here to avoid memcpy if it ever becomes noticeable CPU drain
		V_memcpy( m_pPos0, m_pPos1, sizeof( *m_pPos0 ) * nStaticNodes );
	}

	if ( m_bEnableFollowNodes )
	{
		for ( uint nFlwr = 0, nFollowers = m_pFeModel->m_nFollowNodeCount; nFlwr < nFollowers; ++nFlwr )
		{
			const FeFollowNode_t fn = m_pFeModel->m_pFollowNodes[ nFlwr ];
			fltx4 vDelta = ( pPos1[ fn.nParentNode ] - pPos0[ fn.nParentNode ] ) * ReplicateX4( flTimestepScale * fn.flWeight );
			// why pos0-=, and not pos1+=? because we're computing pos2=pos1 + ( pos1 - pos0 ) later, and advancing pos1 will have the effect of doubling the velocity of the child
			pPos1[ fn.nChildNode ] += vDelta;
			pPos0[ fn.nChildNode ] += vDelta;
		}
	}

	if ( m_pParticleGlue )
	{
		CGluePredictFunctor functor( m_pParticleGlue, pPos0, pPos1, flTimestepScale, m_pFeModel->m_nStaticNodes );
		m_StickyBuffer.ScanBits( functor );
		m_StickyBuffer.Clear();
	}

	float flGravityStepScale = -GetEffectiveGravityScale( ) * flTimeStep * flTimeStep;
	fltx4 vGravityStepScaled = { 0, 0, flGravityStepScale, 0 };
	fltx4 f4TimestepScale = ReplicateX4( flTimestepScale );
	// verlet integration
	if ( pNodeIntegrator )
	{
		if ( m_flDampingMultiplier > 0 && ( m_pFeModel->m_nDynamicNodeFlags & FE_FLAG_HAS_NODE_DAMPING ) )
		{
			float flTimeStep_with_DampingMultiplier = flTimeStep * m_flDampingMultiplier * g_flClothDampingMultiplier;
			for ( uint nDynNode = nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
			{
				const FeNodeIntegrator_t &integrator = pNodeIntegrator[ nDynNode ];
				float flDamping = integrator.flPointDamping * flTimeStep_with_DampingMultiplier ;
				fltx4 f4DampingMul = MaxSIMD( Four_Zeros, Four_Ones - ReplicateX4( flDamping ) );
				fltx4 &pos0 = pPos0[ nDynNode ], &pos1 = pPos1[ nDynNode ];
				pos0 = pos1 + ( pos1 - pos0 ) * f4TimestepScale * f4DampingMul + ReplicateX4( integrator.flGravity ) * vGravityStepScaled;
			}
		}
		else
		{
			// we still have custom gravity in node integrators..
			for ( uint nDynNode = nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
			{
				const FeNodeIntegrator_t &integrator = pNodeIntegrator[ nDynNode ];
				fltx4 &pos0 = pPos0[ nDynNode ], &pos1 = pPos1[ nDynNode ];
				pos0 = pos1 + ( ( pos1 - pos0 ) * f4TimestepScale ) + ReplicateX4( integrator.flGravity ) * vGravityStepScaled;
			}
		}
	}
	else
	{
		fltx4 vGravityStep = -ReplicateX4( flGravityStepScale ) * LoadAlignedSIMD( m_pEnvironment->GetGravity( ).Base() );
		for ( uint nDynNode = nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
		{
			pPos0[ nDynNode ] = pPos1[ nDynNode ] + ( pPos1[ nDynNode ] - pPos0[ nDynNode ] ) * f4TimestepScale + vGravityStep;
		}
	}
	Swap( m_pPos0, m_pPos1 );

	m_flLastTimestep = flTimeStep;
}


Vector CSoftbody::GetEffectiveGravity( void ) const
{
	return m_pEnvironment->GetGravity( ) * GetEffectiveGravityScale( );
}

float CSoftbody::GetEffectiveGravityScale( void ) const
{
	return m_bGravityDisabled ? 0 : m_flGravityScale * ( 1 + m_flOverPredict );
}


inline float EllipsoidIsoParm( const Vector &vLocalPos, const Vector &vInvRadius )
{
	return ScaleVector( vLocalPos, vInvRadius ).LengthSqr( );
}

void CSoftbody::Post( )
{
	MPROF_AUTO_FAST( SoftbodyStepPost );
	if ( IsDebug( ) )
	{
		for ( uint nNode = 0; nNode < m_nNodeCount; ++nNode )
		{
			AssertDbg( m_pPos1[ nNode ].IsValid( ) );
		}
	}

	// modify pos1
	if ( m_flOverPredict != 0 || m_flStepUnderRelax != 0 )
	{
		float w1 = expf( -m_flStepUnderRelax ) / ( 1 + m_flOverPredict );
		fltx4 f4Weight1 = ReplicateX4( w1 ); // 0: just take pos0; 1: just leave pos1
		fltx4 f4Weight0 = Four_Ones - f4Weight1;
		for ( uint nDynNode = m_pFeModel->m_nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
		{
			 fltx4 f4BlendedPos = LoadAlignedSIMD( m_pPos0[ nDynNode ].Base() ) * f4Weight0 + LoadAlignedSIMD( m_pPos1[ nDynNode ].Base() ) * f4Weight1;
			 StoreAlignedSIMD( &m_pPos1[ nDynNode ].x, f4BlendedPos );
		}
	}

	if ( g_nClothDebug & CLOTH_DEBUG_SNAP_TO_ANIM )
	{
		for ( uint nNode = 0; nNode < m_nNodeCount; ++nNode )
		{
			m_pPos1[ nNode ] = GetAnim( m_pFeModel->NodeToCtrl( nNode ) ).GetOrigin( );
		}
	}

	// apply constraints now
	// inclusive collision ellipsoids
	if ( m_bEnableInclusiveCollisionSpheres )
	{
		for ( uint nSphere = 0; nSphere < m_pFeModel->m_nCollisionSpheres[ 1 ]; ++nSphere )
		{
			const FeCollisionSphere_t &fce = m_pFeModel->m_pCollisionSpheres[ nSphere ];
			const matrix3x4a_t &anim = GetAnim( fce.nCtrlParent );
			VectorAligned &pos = m_pPos1[ fce.nChildNode ];
			Vector vLocalPos = VectorITransform( pos, anim ) - fce.m_vOrigin;
			float flIsoParm = vLocalPos.LengthSqr( ) * fce.m_flRFactor; // r-factor is 1/radius^2
			if ( flIsoParm > 1.0f )
			{
				//<sergiy> This is not a true projection onto ellipsoid, although it is if the ellipsoid is a sphere.
				//         I hope this clamping will be enough for our purposes, and it's much cheaper than true projection
				pos = VectorTransform( vLocalPos / sqrtf( flIsoParm ) + fce.m_vOrigin, anim );
			}
		}
	}
	// exclusive collision ellipsoids
	if ( m_bEnableExclusiveCollisionSpheres )
	{
		for ( uint nSphere = m_pFeModel->m_nCollisionSpheres[ 1 ]; nSphere < m_pFeModel->m_nCollisionSpheres[ 0 ]; ++nSphere )
		{
			const FeCollisionSphere_t &fce = m_pFeModel->m_pCollisionSpheres[ nSphere ];
			const matrix3x4a_t &anim = GetAnim( fce.nCtrlParent );
			VectorAligned &pos = m_pPos1[ fce.nChildNode ];
			Vector vLocalPos = VectorITransform( pos, anim ) - fce.m_vOrigin;
			float flDistSqr = vLocalPos.LengthSqr( );
			if ( flDistSqr < Sqr( fce.m_flRFactor ) ) // r-factor is radius
			{
				//<sergiy> This is not a true projection onto ellipsoid, although it is if the ellipsoid is a sphere.
				//         I hope this clamping will be enough for our purposes, and it's much cheaper than true projection
				Vector vSurface;
				if ( flDistSqr < 0.0001f )
				{
					vSurface = Vector( 0, 0, fce.m_flRFactor );
				}
				else
				{
					vSurface = vLocalPos * ( fce.m_flRFactor / sqrtf( flDistSqr ) );
				}

				pos = VectorTransform( vSurface + fce.m_vOrigin, anim );
			}
		}
	}
	if ( m_bEnableCollisionPlanes )
	{
		for ( uint nPlane = 0; nPlane < m_pFeModel->m_nCollisionPlanes; ++nPlane )
		{
			const FeCollisionPlane_t &shape = m_pFeModel->m_pCollisionPlanes[ nPlane ];
			const matrix3x4a_t &anim = GetAnim( shape.nCtrlParent );
			VectorAligned &pos = m_pPos1[ shape.nChildNode ];
			Vector vPlaneNormalWorld = VectorRotate( shape.m_Plane.m_vNormal, anim );
			float flDist = DotProduct( pos - anim.GetOrigin( ), vPlaneNormalWorld ) - shape.m_Plane.m_flOffset;
			if ( flDist < 0 )
			{
				pos -= flDist * vPlaneNormalWorld;
			}
		}
	}

	if ( m_bEnableGroundCollision )
	{
		const uint16 *pNodes = m_pFeModel->m_pWorldCollisionNodes;
		for ( uint nParm = 0; nParm < m_pFeModel->m_nWorldCollisionParamCount; ++nParm )
		{
			const FeWorldCollisionParams_t &parm = m_pFeModel->m_pWorldCollisionParams[ nParm ];
			for ( uint n = parm.nListBegin; n < parm.nListEnd; ++n )
			{
				uint nNode = pNodes[ n ];
				VectorAligned &pos1 = m_pPos1[ nNode ];

				float h = pos1.z - m_vGround.z;
				const float flContactSlop = 0.0f;
				if ( h >= flContactSlop )
					continue; // no contact
				float flGoZ = parm.flWorldFriction, flStopZ = 1 - flGoZ;
				/*if ( h > 0 )
				{
					float flTuneOut = 1.0f - h * ( 1.0f / flContactSlop ); // lerp between ( stay, attract ) and ( 1, 0 )
					flAttract *= flTuneOut;
					flStay = 1.0f - flAttract;
				}
				else*/
				{
					pos1.z = m_vGround.z;
				}
				float flGoXY = ( 1 - parm.flGroundFriction ) * flGoZ, flStopXY = 1 - flGoXY; // ground friction 1.0 means full attract 

				VectorAligned &pos0 = m_pPos0[ nNode ];
				pos0.x = pos0.x * flGoXY + pos1.x * flStopXY;
				pos0.y = pos0.y * flGoXY + pos1.y * flStopXY;
				pos0.z = pos0.z * flGoZ  + pos1.z * flStopZ ;
			}
		}
	}

	m_pFeModel->FitCenters( ( fltx4* )m_pPos1 );

	if ( m_flVolumetricSolveAmount > 0.0f )
	{
		m_pFeModel->FeedbackFitTransforms( m_pPos1, m_flVolumetricSolveAmount );
	}
}



void CSoftbody::ResetVelocities( )
{
	V_memcpy( m_pPos0, m_pPos1, sizeof( *m_pPos0 ) * m_nNodeCount );
}


MPROF_NODE( SoftbodyParticleTransforms_Bases, "Softbody:ParticleTransforms:Bases", "Softbody" );

matrix3x4a_t* CSoftbody::GetParticleTransforms( const VectorAligned *pInputNodePos, uint nFlags )
{
	AssertDbg( !IsDormant( ) ); // sanity check: if the cloth is dormant, why is someone asking for the cloth transforms?
	matrix3x4a_t *pSim = m_pParticles + m_nParticleCount;

	Validate( );

	Assert( !pInputNodePos ); // I don't need this for interpolation anymore
	VectorAligned *pNodePos = /*pInputNodePos ? pInputNodePos : */m_pPos1;

	if ( m_bSimTransformsOutdated )
	{
		// all the dynamic node sim transforms are generated in this routine from pNodePos, and orientations from GetAnim(), so there's no need to copy the positions or orientations, they may be trash
		// it may however be necessary to have all the static transforms copied
		/*for ( uint nDynNode = m_pFeModel->m_nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
		{
			uint nCtrl = m_pFeModel->NodeToCtrl( nDynNode );
			GetSim( nCtrl ).SetOrigin( pNodePos[ nDynNode ] );
		}*/

		uint nStaticNodeCount = m_pFeModel->m_nStaticNodes, nRotLockNodeCount = m_pFeModel->m_nRotLockStaticNodes; NOTE_UNUSED( nStaticNodeCount ); NOTE_UNUSED( nRotLockNodeCount );

		if ( nFlags & SOFTBODY_SIM_TRANSFORMS_INCLUDE_STATIC )
		{
			// the most conservative approach to returning sim particle transforms is to take all static particles from animation at the start, so that if we forget to compute some, we'll have a purely animation-driven transform
			for ( uint nNode = 0; nNode < nRotLockNodeCount; ++nNode )
			{
				uint nCtrl = m_pFeModel->NodeToCtrl( nNode );
				pSim[ nCtrl ] = GetAnim( nCtrl );
			}
		}
		for ( uint nNode = nRotLockNodeCount; nNode < nStaticNodeCount; ++nNode )
		{
			uint nCtrl = m_pFeModel->NodeToCtrl( nNode );
			matrix3x4_t tm = GetAnim( nCtrl );
			tm.SetOrigin( pNodePos[ nNode ] ); // we're about to overwrite the orientation of this transform with bases; and position with (sometimes interpolated) pNodePos
			pSim[ nCtrl ] = tm;
		}
		
		{
			MPROF_AUTO_FAST_COUNT( SoftbodyParticleTransforms_Bases, m_pFeModel->m_nNodeBaseCount );
			for ( uint nBase = 0; nBase < m_pFeModel->m_nNodeBaseCount; ++nBase )
			{
				const FeNodeBase_t &basis = m_pFeModel->m_pNodeBases[ nBase ];
#if 0
				Vector vAxisX = ( pNodePos[ basis.nNodeX1 ] - pNodePos[ basis.nNodeX0 ] ).NormalizedSafe( Vector( 0, 0, -1 ) );
				Vector vAxisY = ( pNodePos[ basis.nNodeY1 ] - pNodePos[ basis.nNodeY0 ] );
				vAxisY = ( vAxisY - DotProduct( vAxisY, vAxisX ) * vAxisX );
				float flAxisYlen = vAxisY.Length();
				if ( flAxisYlen > 0.001f )
				{
					vAxisY /= flAxisYlen;
				}
				else
				{
					// there's no useful direction for Y, just pick an orthogonal direction. Best if it's stable
					VectorPerpendicularToVector( vAxisX, &vAxisY );
				}
#else
				// this version is more consistent with Source1 bone normal/matrix computation
				Vector vAxisX = ( pNodePos[ basis.nNodeX1 ] - pNodePos[ basis.nNodeX0 ] );
				Vector vAxisY = ( pNodePos[ basis.nNodeY1 ] - pNodePos[ basis.nNodeY0 ] ).NormalizedSafe( Vector( 0, 0, -1 ) );
				vAxisX = ( vAxisX - DotProduct( vAxisY, vAxisX ) * vAxisY );
				float flAxisXlen = vAxisX.Length();
				if ( flAxisXlen > 0.05f )
				{
					vAxisX /= flAxisXlen;
				}
				else
				{
					// there's no useful direction for Y, just pick an orthogonal direction. Best if it's stable
					VectorPerpendicularToVector( vAxisY, &vAxisX );
				}
#endif

				matrix3x4a_t tmPredicted;
				tmPredicted.InitXYZ( vAxisX, vAxisY, CrossProduct( vAxisX, vAxisY ), pNodePos[ basis.nNode ] ); // in source1 it's down(-up), forward, right. Forward computed first and the others are going from it
				AssertDbg( IsGoodWorldTransform( tmPredicted, 1, 0.01f ) );

				matrix3x4a_t &tmNode = GetSim( m_pFeModel->NodeToCtrl( basis.nNode ) );
				if ( basis.qAdjust.w == 1.0f )
				{
					tmNode = tmPredicted; 
				}
				else
				{
					tmNode = ConcatTransforms( tmPredicted, QuaternionMatrix( basis.qAdjust ) );
				}

				AssertDbg( IsGoodWorldTransform( tmNode, 1, 0.01f ) );
				AssertDbg( tmNode.GetOrigin() == pNodePos[ basis.nNode ] );
			}
		}

		const uint16 *pRopes = m_pFeModel->m_pRopes;
		uint nRopeBegin = m_pFeModel->m_nRopeCount;
		for ( uint nRopeIndex = 0; nRopeIndex < m_pFeModel->m_nRopeCount; ++nRopeIndex )
		{
			uint nRopeEnd = m_pFeModel->m_pRopes[ nRopeIndex ], nEndNode = pRopes[ nRopeEnd - 1 ], nEndCtrl = m_pFeModel->NodeToCtrl( nEndNode );
			// take care of the terminator of the chain
			if ( nRopeEnd - nRopeBegin == 2 )
			{
				uint nNode = pRopes[ nRopeEnd - 1 ], nPrevNode = pRopes[ nRopeEnd - 2 ];
				GetSim( nEndCtrl ) = AlignX( GetAnim( nEndCtrl ), pNodePos[ nNode ] - pNodePos[ nPrevNode ], pNodePos[ nNode ] );
			}
			else
			{
				AssertDbg( nRopeEnd - nRopeBegin > 2 );
				for ( uint nNextLinkIndex = nRopeBegin + 2; ; )
				{
					uint nNode = pRopes[ nNextLinkIndex - 1 ], nCtrl = m_pFeModel->NodeToCtrl( nNode ), nNextNode = pRopes[ nNextLinkIndex ];
					AssertDbg( IsGoodWorldTransform( GetAnim( nCtrl ) ) );
					matrix3x4a_t tmAligned = AlignX( GetAnim( nCtrl ), pNodePos[ nNextNode ] - pNodePos[ nNode ], pNodePos[ nNode ] );
					AssertDbg( IsGoodWorldTransform( tmAligned, 1.0f, 0.0005f ) );
					GetSim( nCtrl ) = tmAligned;

					++nNextLinkIndex;
					if ( nNextLinkIndex >= nRopeEnd )
					{
						Assert( nNextLinkIndex == nRopeEnd );
						Set3x3( GetSim( nEndCtrl ), tmAligned, pNodePos[ nEndNode ] );
						break;
					}
				}
			}
			nRopeBegin = nRopeEnd;
		}

		for ( uint nFreeNodeIndex = m_pFeModel->m_nFreeNodeCount; nFreeNodeIndex-- > 0; )
		{
			uint nNode = m_pFeModel->m_pFreeNodes[ nFreeNodeIndex ];
			uint nCtrl = m_pFeModel->NodeToCtrl( nNode );

			Set3x3( GetSim( nCtrl ), GetAnim( nCtrl ), pNodePos[ nNode ] );
		}

		// fix up the bones that need to be oriented according to some polygon because they don't have enough influences otherwise
		for ( uint nRevOffsetIndex = m_pFeModel->m_nReverseOffsetCount; nRevOffsetIndex-- > 0; )
		{
			const FeNodeReverseOffset_t &revOffset = m_pFeModel->m_pReverseOffsets[ nRevOffsetIndex ];
			matrix3x4a_t &tmBone = pSim[ revOffset.nBoneCtrl ];
			Vector vOffsetWs = VectorRotate( revOffset.vOffset, tmBone );
			Vector vTargetCenter = pNodePos[ revOffset.nTargetNode ] - vOffsetWs;
			tmBone.SetOrigin( vTargetCenter );
			uint nBoneNode = m_pFeModel->CtrlToNode( revOffset.nBoneCtrl );
			m_pPos1[ nBoneNode ] = m_pPos0[ nBoneNode ] = vTargetCenter; // we can only compute this efficiently here, where we have all the matrices
		}

		m_pFeModel->FitTransforms( pNodePos, pSim );

		if ( int nClothDebug = g_nClothDebug )
		{
			for ( int nCtrl = 0; nCtrl < m_nParticleCount; ++nCtrl )
			{
				if ( nClothDebug & CLOTH_DEBUG_SIM_ANIM_POS )
				{
					GetSim( nCtrl ).SetOrigin( GetAnim( nCtrl ).GetOrigin( ) );
				}
				else if ( nClothDebug & CLOTH_DEBUG_SIM_ANIM_ROT )
				{
					Set3x3( GetSim( nCtrl ), GetAnim( nCtrl ) );
				}
			}
		}

		AssertDbg( nRopeBegin == m_pFeModel->m_nRopeIndexCount );
		m_bSimTransformsOutdated = false;
		Validate();
	}

	return pSim;
}




void CSoftbody::SetAnimatedTransform( int nParticle, const matrix3x4a_t &transform )
{
	GetAnim( nParticle ) = transform;
	m_bAnimTransformChanged = true;
}

void CSoftbody::TouchAnimatedTransforms()
{
	m_bAnimTransformChanged = true;
}


bool IsBoneMapUnique( const int16 *pBones, uint nCount )
{
	CUtlHashtable< int16 > used;
	for ( uint nBone = 0; nBone < nCount; ++nBone )
	{
		if ( pBones[ nBone ] >= 0 )
		{
			if ( used.HasElement( pBones[ nBone ] ) )
				return false;
			used.Insert( pBones[ nBone ] );
		}
	}
	return true;
}


void CSoftbody::SetAnimatedTransforms( const matrix3x4a_t *pSimulationWorldTransforms )
{
	float flInvScale = 1.0f / m_flModelScale;

	AssertDbg( IsBoneMapUnique( m_pCtrlToModelBone, m_nParticleCount ) );
	for( int nParticle = 0; nParticle < m_nParticleCount; ++nParticle )
	{
		int nBone = m_pCtrlToModelBone[ nParticle ];
		if ( nBone >= 0 )
		{
			const matrix3x4a_t &tm = pSimulationWorldTransforms[ nBone ];
			// AssertDbg( ( tm.GetOrigin() - m_vSimOrigin ).LengthSqr() < 500 * 500 ); // we shouldn't set bones far away from sim origin, or we won't be able to detect teleportation and adjust in time
			matrix3x4a_t tmUnscaled = ScaleMatrix3x3( tm, flInvScale );
			AssertDbg( IsGoodWorldTransform( tmUnscaled ) );
			GetAnim( nParticle ) = tmUnscaled;
		}
	}

	UpdateCtrlOffsets( false );
	m_bAnimTransformChanged = true;
}

void CSoftbody::SetAnimatedTransformsNoScale( const matrix3x4a_t *pSimulationWorldTransforms )
{
	AssertDbg( IsBoneMapUnique( m_pCtrlToModelBone, m_nParticleCount ) );
	for ( int nParticle = 0; nParticle < m_nParticleCount; ++nParticle )
	{
		int nBone = m_pCtrlToModelBone[ nParticle ];
		if ( nBone >= 0 )
		{
			const matrix3x4a_t &tm = pSimulationWorldTransforms[ nBone ];
			AssertDbg( IsGoodWorldTransform( tm ) );
			GetAnim( nParticle ) = tm;
		}
	}

	UpdateCtrlOffsets( false );

	m_bAnimTransformChanged = true;
}



void CSoftbody::UpdateCtrlOffsets( bool bOverridePose )
{
	float flConstraintScale = m_flModelScale * m_flClothScale;
	for ( int nOffset = 0; nOffset < m_pFeModel->m_nCtrlOffsets; ++nOffset )
	{
		const FeCtrlOffset_t &offset = m_pFeModel->m_pCtrlOffsets[ nOffset ];
		matrix3x4a_t tm = GetAnim( offset.nCtrlParent );
		tm.SetOrigin( VectorTransform( offset.vOffset * flConstraintScale, tm ) );
		GetAnim( offset.nCtrlChild ) = tm;
		if ( bOverridePose )
		{
			GetSim( offset.nCtrlChild ) = tm;
			uint nNodeChild = m_pFeModel->CtrlToNode( offset.nCtrlChild );
			m_pPos0[ nNodeChild ] = m_pPos1[ nNodeChild ] = tm.GetOrigin( );
		}
	}

	for ( int nOsOffset = 0; nOsOffset < m_pFeModel->m_nCtrlOsOffsets; ++nOsOffset )
	{
		const FeCtrlOsOffset_t &offset = m_pFeModel->m_pCtrlOsOffsets[ nOsOffset ];
		matrix3x4a_t tm = GetAnim( offset.nCtrlParent );
		tm.SetOrigin( m_vRopeOffset + tm.GetOrigin() );
		GetAnim( offset.nCtrlChild ) = tm;
		if ( bOverridePose )
		{
			GetSim( offset.nCtrlChild ) = tm;
			uint nNodeChild = m_pFeModel->CtrlToNode( offset.nCtrlChild );
			m_pPos0[ nNodeChild ] = m_pPos1[ nNodeChild ] = tm.GetOrigin( );
		}
	}
}


void CSoftbody::InitializeTransforms( const int16 *pCtrlToBone, const matrix3x4a_t *pSimulationWorldTransforms )
{
	float flInvScale = 1.0f / m_flModelScale; NOTE_UNUSED( flInvScale );
	for ( uint nParticle = 0; nParticle < m_nParticleCount; ++nParticle )
	{
		int nBone = pCtrlToBone[ nParticle ];
		if ( nBone >= 0 )
		{
			matrix3x4a_t tm = pSimulationWorldTransforms[ nBone ];
			MatrixNormalize( tm, tm );
			//AssertDbg( IsGoodWorldTransform( tm, m_flModelScale ) );
			//tm.ScaleUpper3x3Matrix( flInvScale );
			GetAnim( nParticle ) = GetSim( nParticle ) = tm;
			AssertDbg( IsGoodWorldTransform( tm ) );
			uint nNode = m_pFeModel->CtrlToNode( nParticle );
			m_pPos0[ nNode ] = m_pPos1[ nNode ] = tm.GetOrigin( );
		}
		else
		{
			//GetAnim( nParticle ) = GetSim( nParticle ) = g_MatrixIdentity;
			//m_pPos0[ nNode ] = m_pPos1[ nNode ] = vec3_origin;
			// this means some nodes will not be set by animation, which will cause trouble esp. when they're static nodes that need to be animated
			AssertDbg( !g_nClothDebug || m_pFeModel->FindCtrlOffsetByChild( nParticle ) || m_pFeModel->FindCtrlOsOffsetByChild( nParticle ) );
		}
	}
	//
	//  also force all simulated particles to the new animated position
	//
	UpdateCtrlOffsets( true );
	m_bAnimTransformChanged = false;
}



void DrawTaperedCapsule( IVDebugOverlay* pDebugOverlay, Vector v0, Vector v1, float r0, float r1 )
{
	if ( r0 > r1 )
	{
		Swap( r0, r1 );
		Swap( v0, v1 );
	}
	Vector vAxis = v1 - v0;
	float flAxisLen = vAxis.Length();
	float flSlope = 0;
	int nStacks0, nStacks = 8;
	if ( flAxisLen <= r1 - r0 + 0.001f )
	{
		// just draw a sphere
		v0 = v1;
		r0 = r1;
		vAxis = Vector( 1, 0, 0 );
		nStacks0 = nStacks / 2;
	}
	else
	{
		flSlope = asinf( ( r1 - r0 ) / flAxisLen );
		vAxis /= flAxisLen;
		nStacks0 = Min( nStacks - 1, Max( 1, int( ceil( ( M_PI / 2 - flSlope ) / ( M_PI / nStacks ) + .5f) ) ) );
	}
	int nSegments = 8;
	CUtlVector< Vector > verts;
	verts.SetCount( nStacks * nSegments + 2 );

	verts[ 0 ] = v0 - vAxis * r0;
	Vector vNormal0 = VectorPerpendicularToVector( vAxis );
	Vector vNormal1 = CrossProduct( vAxis, vNormal0 );

	for ( int nStack = 0; nStack < nStacks0; ++nStack )
	{
		float flStackAngle = ( nStack + 1 ) * ( M_PI / 2 - flSlope ) / nStacks0;
		float csr0 = cosf( flStackAngle ) * r0, ssr0 = sinf( flStackAngle ) * r0;
		for ( int nSeg = 0; nSeg < nSegments; ++nSeg )
		{
			float flPsi = 2 * nSeg * M_PI / nSegments;
			verts[ 1 + nStack * nSegments + nSeg ] = v0 - csr0 * vAxis + cosf( flPsi ) * ssr0 * vNormal0 + sinf( flPsi ) * ssr0 * vNormal1;
		}
	}
	for ( int nStack = nStacks0; nStack < nStacks; ++nStack )
	{
		float flStackAngle = ( M_PI / 2 - flSlope ) + ( nStack - nStacks0 ) * ( M_PI / 2 + flSlope ) / ( nStacks - nStacks0 );
		float csr1 = cosf( flStackAngle ) * r1, ssr1 = sinf( flStackAngle ) * r1;
		for ( int nSeg = 0; nSeg < nSegments; ++nSeg )
		{
			float flPsi = 2 * nSeg * M_PI / nSegments;
			verts[ 1 + nStack * nSegments + nSeg ] = v1 - csr1 * vAxis + cosf( flPsi ) * ssr1 * vNormal0 + sinf( flPsi ) * ssr1 * vNormal1;
		}
	}
	verts.Tail() = v1 + vAxis * r1;


	int r = 128, g = 128, b = 128, a = 32;
	for ( int nSeg = 0; nSeg < nSegments; ++nSeg )
	{
		int nNextSeg = ( nSeg + 1 ) % nSegments;
		pDebugOverlay->AddTriangleOverlay( verts[ 0 ], verts[ 1 + nSeg ], verts[ 1 + nNextSeg ], r, g, b, a, false, 0 );
		for ( int nStack = 1; nStack < nStacks; ++nStack )
		{
			int nPrevStack = nStack - 1;
			pDebugOverlay->AddTriangleOverlay( verts[ 1 + nPrevStack * nSegments + nSeg ], verts[ 1 + nPrevStack * nSegments + nNextSeg ], verts[ 1 + nStack * nSegments + nNextSeg ], r, g, b, a, false, 0 );
			pDebugOverlay->AddTriangleOverlay( verts[ 1 + nStack * nSegments + nNextSeg ], verts[ 1 + nStack * nSegments + nSeg ], verts[ 1 + nPrevStack * nSegments + nSeg ], r, g, b, a, false, 0 );
		}
		pDebugOverlay->AddTriangleOverlay( verts.Tail(), verts[ 1 + ( nStacks - 1 ) * nSegments + nNextSeg ], verts[ 1 + ( nStacks - 1 ) * nSegments + nSeg ], r, g, b, a, false, 0 );
	}

	for ( int nSeg = 0; nSeg < nSegments; ++nSeg )
	{
		for ( int nStack = nStacks0 - 1; nStack <= nStacks0; ++nStack )
		{
			int nNextSeg = ( nSeg + 1 ) % nSegments;
			pDebugOverlay->AddLineOverlay( verts[ 1 + nStack * nSegments + nSeg ], verts[ 1 + nStack * nSegments + nNextSeg ], 200, 200, 200, 200, false, 0 );
		}
	}
}


void AddLineOverlay( CMeshBuilder &meshBuilder, const matrix3x4a_t &tmViewModel, const Vector &v0, const Vector &v1, unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	Vector v0v = VectorTransform( v0, tmViewModel );
	Vector v1v = VectorTransform( v1, tmViewModel );
	meshBuilder.Position3fv( v0v.Base() );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();
	meshBuilder.Position3fv( v1v.Base() );
	meshBuilder.Color4ub( r, g, b, a );
	meshBuilder.AdvanceVertex();
}

// HLMV: Called from StudioModel::DrawModel->DrawSoftbody, in studio_render.cpp
// Client: Called from CSoftbodyProcess::PostRender, in physics_softbody.cpp
void CSoftbody::Draw( const RnDebugDrawOptions_t &options, IMesh *pDynamicMesh )
{
	if ( options.m_nLayers == 0 )
		return;
	VectorAligned *pPos0 = m_pPos0, *pPos1 = m_pPos1;
	CUtlVectorAligned< VectorAligned > posBuffer;
	
	matrix3x4a_t tmViewModel = g_MatrixIdentity;
	if ( m_nAnimSpace == SOFTBODY_ANIM_SPACE_LOCAL )
	{
		posBuffer.SetCount( m_nNodeCount * 2 );
		pPos0 = &posBuffer[ 0 ];
		pPos1 = &posBuffer[ m_nNodeCount ];

		AngleMatrix( m_vSimAngles, m_vSimOrigin, tmViewModel );
		for ( uint nNode = 0; nNode < m_nNodeCount; ++nNode )
		{
			pPos0[ nNode ] = VectorTransform( m_pPos0[ nNode ], tmViewModel );
			pPos1[ nNode ] = VectorTransform( m_pPos1[ nNode ], tmViewModel );
		}
	}

	if ( options.m_nLayers & RN_SOFTBODY_DRAW_WIND )
	{
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pDynamicMesh, MATERIAL_LINES, m_nNodeCount );
		CUtlVector< VectorAligned > pos2;
		pos2.CopyArray( m_pPos1, m_nNodeCount );
		IntegrateWind( pos2.Base(), 0.05f );


		float flMaxLenInv = 100.0f;
		for ( uint n = 0; n < m_nNodeCount; ++n )
		{
			float flLen = ( pos2[ n ] - m_pPos1[ n ] ).Length();
			if ( flLen * flMaxLenInv > 1.0f )
			{
				flMaxLenInv = 1.0f / flLen;
			}
		}

		for ( uint n = 0; n < m_nNodeCount; ++n )
		{
			float flLen = ( pos2[ n ] - m_pPos1[ n ] ).Length();
			if ( flLen < 0.001f )
				continue;
			meshBuilder.Position3fv( m_pPos1[ n].Base() );
			meshBuilder.Color4ub( 255, 255, 255, 255 );
			meshBuilder.AdvanceVertex();
			meshBuilder.Position3fv( pos2[ n ].Base() );
			meshBuilder.Color4ub( 255, 255 - 128 * flLen * flMaxLenInv, 255, 255 );
			meshBuilder.AdvanceVertex();
		}
		meshBuilder.End();
	}

	// find virtual nodes
	CVarBitVec virtualCtrls( m_nParticleCount );
	uint nVirtualCtrls = ComputeVirtualCtrls( virtualCtrls );

	// the positions of nodes are not correct in dormant mode
	const matrix3x4a_t *pSim = IsDormant() ? GetAnimatedTransforms() : GetParticleTransforms( NULL, SOFTBODY_SIM_TRANSFORMS_INCLUDE_STATIC ); // make the softbody compute the sim bones
	if ( m_pFeModel->m_nSimdTriCount[ 0 ] && (options.m_nLayers & RN_SOFTBODY_DRAW_POLYGONS))
	{
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pDynamicMesh, MATERIAL_TRIANGLES, m_pFeModel->m_nSimdTriCount[ 0 ] * 4 );

		uint32 fourColors[ 4 ] = { 0xBBAAAAAA, 0xCCBBBBBB, 0xDDCCCCCC, 0xEEDDDDDD };

		for ( int nSimdTri = 0; nSimdTri < m_pFeModel->m_nSimdTriCount[ 0 ]; ++nSimdTri )
		{
			const FeSimdTri_t &tri = m_pFeModel->m_pSimdTris[ nSimdTri ];
			for ( int q = 0; q < 4; ++q )
			{
				for ( int j = 0; j < 3; ++j )
				{
					meshBuilder.Position3fv( pPos1[ tri.nNode[ j ][ q ] ].Base() );
					meshBuilder.Color4ubv( ( const unsigned char* )&fourColors[ nSimdTri & 3 ] );
					meshBuilder.AdvanceVertex();
				}
			}
		}
		meshBuilder.End();
	}

	if ( m_pFeModel->m_nSimdQuadCount[ 0 ] && ( options.m_nLayers & RN_SOFTBODY_DRAW_POLYGONS ) )
	{
		CMeshBuilder meshBuilder;
		int nBaseIndex = meshBuilder.IndexCount();
		meshBuilder.Begin( pDynamicMesh, MATERIAL_LINES, m_nNodeCount, m_pFeModel->m_nSimdQuadCount[ 0 ] * 4 * 8 );

		uint32 fourColors[ 4 ] = { 0xAABBAAAA, 0xBBCCBBBB, 0xCCDDCCCC, 0xDDEEDDDD };
		for ( uint n = 0; n < m_nNodeCount; ++n )
		{
			meshBuilder.Position3fv( pPos1[ n ].Base() );
			meshBuilder.Color4ubv( ( const unsigned char* )&fourColors[ n < m_pFeModel->m_nStaticNodes ? 1 : 0 ] );
			meshBuilder.AdvanceVertex();
		}

		CUtlHashtable< uint32 > linesDrawn;

		for ( int nSimdQuad = 0; nSimdQuad < m_pFeModel->m_nSimdQuadCount[ 0 ]; nSimdQuad ++)
		{
			const FeSimdQuad_t &quad = m_pFeModel->m_pSimdQuads[ nSimdQuad ];
			for ( int q = 0; q < 4; ++q )
			{
				for ( int j = 0; j < 4; ++j )
				{
					uint jIdxs = ( 0x16BC >> ( j * 4 ) ) & 15; // 0-1, 1-2, 2-3, 3-0
					// 0x17E8 >> ... : 0-1, 1-3, 3-2, 2-0
					uint nIndex0 = quad.nNode[ jIdxs >> 2 ][ q ], nIndex1 = quad.nNode[ jIdxs & 3 ][ q ], nIndexPair = ( nIndex0 << 16 ) | nIndex1;
					if ( !linesDrawn.HasElement( nIndexPair ) )
					{
						linesDrawn.Insert( nIndexPair );
						meshBuilder.Index( nIndex0 + nBaseIndex );
						meshBuilder.AdvanceIndex();
						meshBuilder.Index( nIndex1 + nBaseIndex );
						meshBuilder.AdvanceIndex();
					}
				}
			}
		}
		meshBuilder.End();
	}

	if ( m_pFeModel->m_nSimdRodCount || nVirtualCtrls )
	{
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pDynamicMesh, MATERIAL_LINES, m_pFeModel->m_nSimdRodCount * 4 + nVirtualCtrls * 6 );

		if ( options.m_nLayers & RN_SOFTBODY_DRAW_EDGES )
		{
			uint32 fourColors[ 4 ] = { 0xBBBBAAAA, 0xCCCCBBBB, 0xDDDDCCCC, 0xEEEEDDDD };
			for ( int nSimdRod = 0; nSimdRod < m_pFeModel->m_nSimdRodCount; ++nSimdRod )
			{
				const FeSimdRodConstraint_t &rod = m_pFeModel->m_pSimdRods[ nSimdRod ];
				for ( int q = 0; q < 4; ++q )
				{
					for ( int j = 0; j < 2; ++j )
					{
						meshBuilder.Position3fv( pPos1[ rod.nNode[ j ][ q ] ].Base() );
						meshBuilder.Color4ubv( ( const unsigned char* )&fourColors[ nSimdRod & 3 ] );
						meshBuilder.AdvanceVertex();
					}
				}
			}
		}

		if ( options.m_nLayers & RN_SOFTBODY_DRAW_INDICES )
		{

		}

		if ( options.m_nLayers & RN_SOFTBODY_DRAW_BASES )
		{
			for ( int nCtrl = 0; nCtrl < m_nParticleCount; ++nCtrl )
			{
				uint nNode = m_pFeModel->CtrlToNode( nCtrl );
				if ( nNode < m_nNodeCount && !virtualCtrls.IsBitSet( nNode ) )
				{
					float flAxisScale = ( nNode < m_pFeModel->m_nRotLockStaticNodes ? 2.0f : 5.0f );

					// this is a dynamic node
					const matrix3x4a_t &node = pSim[ nCtrl ];
					AddLineOverlay( meshBuilder, tmViewModel, node.GetOrigin(), node.GetOrigin() + node.GetColumn( X_AXIS ) * flAxisScale * 2, 255, 180, 180, 255 );
					AddLineOverlay( meshBuilder, tmViewModel, node.GetOrigin(), node.GetOrigin() + node.GetColumn( Y_AXIS ) * flAxisScale, 180, 255, 180, 255 );
					AddLineOverlay( meshBuilder, tmViewModel, node.GetOrigin(), node.GetOrigin() + node.GetColumn( Z_AXIS ) * flAxisScale, 180, 180, 255, 255 );
					const matrix3x4a_t &anim = GetAnim( nCtrl );
					float flAnimAxisScale = 0.5f * flAxisScale;
					AddLineOverlay( meshBuilder, tmViewModel, anim.GetOrigin(), anim.GetOrigin() + anim.GetColumn( X_AXIS ) * flAnimAxisScale, 200, 128, 128, 255 );
					AddLineOverlay( meshBuilder, tmViewModel, anim.GetOrigin(), anim.GetOrigin() + anim.GetColumn( Y_AXIS ) * flAnimAxisScale, 128, 200, 128, 255 );
					AddLineOverlay( meshBuilder, tmViewModel, anim.GetOrigin(), anim.GetOrigin() + anim.GetColumn( Z_AXIS ) * flAnimAxisScale, 128, 128, 200, 255 );
				}
			}
		}
		meshBuilder.End();
	}

}

void CSoftbody::Draw( const RnDebugDrawOptions_t &options, IVDebugOverlay* pDebugOverlay )
{
	if ( !m_bDebugDraw )
		return;

	uint nDebugLayers = options.m_nLayers;
	MPROF_AUTO_FAST( SoftbodyDraw );

	VectorAligned *pPos0 = m_pPos0, *pPos1 = m_pPos1;
	CUtlVector< VectorAligned > posBuffer;
	if ( m_nAnimSpace == SOFTBODY_ANIM_SPACE_LOCAL )
	{
		posBuffer.SetCount( m_nNodeCount * 2 );
		pPos0 = &posBuffer[ 0 ];
		pPos1 = &posBuffer[ m_nNodeCount ];

		matrix3x4a_t tmEntity;
		AngleMatrix( m_vSimAngles, m_vSimOrigin, tmEntity );
		for ( uint nNode = 0; nNode < m_nNodeCount; ++nNode )
		{
			pPos0[ nNode ] = VectorTransform( m_pPos0[ nNode ], tmEntity );
			pPos1[ nNode ] = VectorTransform( m_pPos1[ nNode ], tmEntity );
		}
	}

	// find virtual nodes
	CVarBitVec virtualCtrls( m_nParticleCount );
	ComputeVirtualCtrls( virtualCtrls );

	// the positions of nodes are not correct in dormant mode
	const matrix3x4a_t *pSim = IsDormant() ? GetAnimatedTransforms() : GetParticleTransforms( NULL, SOFTBODY_SIM_TRANSFORMS_INCLUDE_STATIC ); // make the softbody compute the sim bones

	VertexColor_t colorNeg( 255, 255, 200, 255 ), colorPos( 255, 200, 200, 255 ), colorNeutral( 200, 200, 200, 255 );

	for ( uint nTapCap = m_pFeModel->m_nTaperedCapsuleStretchCount; nTapCap-- > 0; )
	{
		const FeTaperedCapsuleStretch_t &tc = m_pFeModel->m_pTaperedCapsuleStretches[ nTapCap ];
		DrawTaperedCapsule( pDebugOverlay, pPos1[ tc.nNode[ 0 ] ], pPos1[ tc.nNode[ 1 ] ], tc.flRadius[ 0 ] * m_flModelScale, tc.flRadius[ 1 ] * m_flModelScale );
	}
	for ( uint nTapCap = m_pFeModel->m_nTaperedCapsuleRigidCount; nTapCap-- > 0; )
	{
		const FeTaperedCapsuleRigid_t &tc = m_pFeModel->m_pTaperedCapsuleRigids[ nTapCap ];
		const matrix3x4a_t &tm = GetAnim( tc.nNode );
		DrawTaperedCapsule( pDebugOverlay, tm.TransformVector( tc.vCenter[ 0 ] * m_flModelScale ), tm.TransformVector( tc.vCenter[ 1 ] * m_flModelScale ), tc.flRadius[ 0 ] * m_flModelScale, tc.flRadius[ 1 ] * m_flModelScale );
	}
	for ( uint nSphere = m_pFeModel->m_nSphereRigidCount; nSphere-- > 0; )
	{
		const FeSphereRigid_t &tc = m_pFeModel->m_pSphereRigids[ nSphere ];
		const matrix3x4a_t &tm = GetAnim( tc.nNode );
		DrawTaperedCapsule( pDebugOverlay, tm.TransformVector( tc.vCenter * m_flModelScale ), tm.TransformVector( tc.vCenter * m_flModelScale ), tc.flRadius* m_flModelScale, tc.flRadius * m_flModelScale );
	}

	float flConstraintScale = m_flModelScale * m_flClothScale;

	if ( m_nDebugDrawTreeFlags && m_pFeModel->m_pTreeParents && m_nDebugDrawTreeBeginLevel != m_nDebugDrawTreeEndLevel )
	{
		CUtlVector< FeAabb_t > boxes;
		const uint nDynCount = m_pFeModel->GetDynamicNodeCount();
		boxes.SetCount( nDynCount - 1 );
		m_pFeModel->ComputeCollisionTreeBounds( pPos1 + m_pFeModel->m_nStaticNodes, boxes.Base() );

		CUtlVector< uint16 > levels;
		levels.SetCount( 2 * nDynCount );
		levels.FillWithValue( 0 );
		levels[ 2 * nDynCount - 1 ] = 0xABCD;
		if ( m_nDebugDrawTreeFlags & SOFTBODY_DEBUG_DRAW_TREE_BOTTOM_UP )
		{
			m_pFeModel->ComputeCollisionTreeHeightBottomUp( levels.Base() + nDynCount );
		}
		else if ( m_nDebugDrawTreeFlags & SOFTBODY_DEBUG_DRAW_TREE_TOP_DOWN )
		{
			m_pFeModel->ComputeCollisionTreeDepthTopDown( levels.Base() );
		}
		else
		{
			for ( int nDynNode = 0; nDynNode < nDynCount - 1; ++nDynNode )
			{
				levels[ nDynCount + nDynNode ] = nDynNode + 1;
			}
		}
		Assert( levels[ 2 * nDynCount - 1 ] == 0xABCD );
		//int nStep = m_nDebugDrawTreeBeginLevel < m_nDebugDrawTreeEndLevel ? 1 : -1;
		//int nColorStep = ( nStep * 200 ) / ( m_nDebugDrawTreeEndLevel - m_nDebugDrawTreeBeginLevel );
		//int nColor = 255;
		//for ( int nLevel = m_nDebugDrawTreeBeginLevel; nLevel != m_nDebugDrawTreeEndLevel; nLevel += nStep, nColor += nColorStep )
		for ( uint nDynNode = 0; nDynNode < nDynCount - 1; ++nDynNode )
		{
			int nLevel = levels[ nDynCount + nDynNode ];
			if ( ( nLevel >= m_nDebugDrawTreeBeginLevel && nLevel < m_nDebugDrawTreeEndLevel ) || ( nLevel <= m_nDebugDrawTreeBeginLevel && nLevel > m_nDebugDrawTreeEndLevel ) )
			{
				FeAabb_t &box = boxes[ nDynNode ];
				NOTE_UNUSED( box );
				int nColor = 180 - ( 80 * ( nLevel - m_nDebugDrawTreeBeginLevel ) ) / ( m_nDebugDrawTreeEndLevel - m_nDebugDrawTreeBeginLevel );
				NOTE_UNUSED( nColor );

				//pDebugOverlay->AddBoxOverlay( AsVector( box.m_vMinBounds ), AsVector( box.m_vMaxBounds ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( a.x, a.y, a.z ), Vector( b.x, a.y, a.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( a.x, a.y, a.z ), Vector( a.x, b.y, a.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( a.x, a.y, a.z ), Vector( a.x, a.y, b.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( a.x, b.y, b.z ), Vector( b.x, b.y, b.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( b.x, a.y, b.z ), Vector( b.x, b.y, b.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( b.x, b.y, a.z ), Vector( b.x, b.y, b.z ), nColor, nColor, nColor, nColor );
// 
// 				pDebugOverlay->AddLineOverlay( Vector( b.x, a.y, a.z ), Vector( b.x, b.y, a.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( a.x, b.y, a.z ), Vector( a.x, b.y, b.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( a.x, a.y, b.z ), Vector( b.x, a.y, b.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( b.x, b.y, a.z ), Vector( a.x, b.y, b.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( a.x, b.y, b.z ), Vector( b.x, a.y, b.z ), nColor, nColor, nColor, nColor );
// 				pDebugOverlay->AddLineOverlay( Vector( b.x, a.y, b.z ), Vector( b.x, b.y, a.z ), nColor, nColor, nColor, nColor );
			}
		}

		if ( m_nDebugDrawTreeFlags & ( 1 << 4 ) )
			return;
	}

	for ( uint nQuad = 0; nQuad < m_pFeModel->m_nQuadCount[2]; ++nQuad )
	{
		const FeQuad_t &quad = m_pFeModel->m_pQuads[ nQuad ];
		VectorAligned p0 = pPos1[ quad.nNode[ 0 ] ], p1 = pPos1[ quad.nNode[ 1 ] ], p2 = pPos1[ quad.nNode[ 2 ] ], p3 = pPos1[ quad.nNode[ 3 ] ];
		float flError = m_pFeModel->RelaxQuad2( 1.0f, flConstraintScale, quad, p0, p1, p2, p3 ); NOTE_UNUSED( flError );
		int nRelError = int ( 64 * Clamp< float >( flError / Sqr( .125f * Perimeter( quad ) ), 0, 1 ) );
		pDebugOverlay->AddTriangleOverlay( p0, p1, p2, 192 + nRelError, 255 - nRelError, 255, 64, false, 0 );
		pDebugOverlay->AddTriangleOverlay( p0, p2, p3, 192 + nRelError, 255 - nRelError, 245, 64, false, 0 );
	}
	for ( uint nQuad = m_pFeModel->m_nQuadCount[ 2 ]; nQuad < m_pFeModel->m_nQuadCount[ 1 ]; ++nQuad )
	{
		const FeQuad_t &quad = m_pFeModel->m_pQuads[ nQuad ];
		VectorAligned p0 = pPos1[ quad.nNode[ 0 ] ], p1 = pPos1[ quad.nNode[ 1 ] ], p2 = pPos1[ quad.nNode[ 2 ] ], p3 = pPos1[ quad.nNode[ 3 ] ];
		float flError = m_pFeModel->RelaxQuad1( 1.0f, flConstraintScale, quad, p0, p1, p2, p3 ); NOTE_UNUSED( flError );
		int nRelError = int( 64 * Clamp< float >( flError / Sqr( .125f * Perimeter( quad ) ), 0, 1 ) );
		pDebugOverlay->AddTriangleOverlay( p0, p1, p2, 192 + nRelError, 255 - nRelError, 255, 64, false, 0 );
		pDebugOverlay->AddTriangleOverlay( p0, p2, p3, 192 + nRelError, 255 - nRelError, 245, 64, false, 0 );
	}
	for ( uint nQuad = m_pFeModel->m_nQuadCount[ 1 ]; nQuad < m_pFeModel->m_nQuadCount[ 0 ]; ++nQuad )
	{
		const FeQuad_t &quad = m_pFeModel->m_pQuads[ nQuad ];
		VectorAligned p0 = pPos1[ quad.nNode[ 0 ] ], p1 = pPos1[ quad.nNode[ 1 ] ], p2 = pPos1[ quad.nNode[ 2 ] ], p3 = pPos1[ quad.nNode[ 3 ] ];
		float flError = m_pFeModel->RelaxQuad0( 1.0f, flConstraintScale, quad, p0, p1, p2, p3 ); NOTE_UNUSED( flError );
		int nRelError = int( 64 * Clamp< float >( flError / Sqr( .125f * Perimeter( quad ) ), 0, 1 ) );
		pDebugOverlay->AddTriangleOverlay( p0, p1, p2, 192 + nRelError, 255 - nRelError, 255, 64, false, 0 );
		pDebugOverlay->AddTriangleOverlay( p0, p2, p3, 192 + nRelError, 255 - nRelError, 245, 64, false, 0 );
	}

	for ( uint nTri = 0; nTri < m_pFeModel->m_nTriCount[ 2 ]; ++nTri )
	{
		const FeTri_t &tri = m_pFeModel->m_pTris[ nTri ];
		VectorAligned p0 = pPos1[ tri.nNode[ 0 ] ], p1 = pPos1[ tri.nNode[ 1 ] ], p2 = pPos1[ tri.nNode[ 2 ] ];
		float flError = m_pFeModel->RelaxTri2( 1.0f, flConstraintScale, tri, p0, p1, p2 ); NOTE_UNUSED( flError );
		int nRelError = int( 64 * Clamp< float >( flError / Sqr( .125f * Perimeter( tri ) ), 0, 1 ) );
		pDebugOverlay->AddTriangleOverlay( p0, p1, p2, 192 + nRelError, 255 - nRelError, 255, 64, false, 0 );
	}
	for ( uint nTri = m_pFeModel->m_nTriCount[ 2 ]; nTri < m_pFeModel->m_nTriCount[ 1 ]; ++nTri )
	{
		const FeTri_t &tri = m_pFeModel->m_pTris[ nTri ];
		VectorAligned p0 = pPos1[ tri.nNode[ 0 ] ], p1 = pPos1[ tri.nNode[ 1 ] ], p2 = pPos1[ tri.nNode[ 2 ] ];
		float flError = m_pFeModel->RelaxTri1( 1.0f, flConstraintScale, tri, p0, p1, p2 ); NOTE_UNUSED( flError );
		int nRelError = int( 64 * Clamp< float >( flError / Sqr( .125f * Perimeter( tri ) ), 0, 1 ) );
		pDebugOverlay->AddTriangleOverlay( p0, p1, p2, 192 + nRelError, 255 - nRelError, 255, 64, false, 0 );
	}
	for ( uint nTri = m_pFeModel->m_nTriCount[ 1 ]; nTri < m_pFeModel->m_nTriCount[ 0 ]; ++nTri )
	{
		const FeTri_t &tri = m_pFeModel->m_pTris[ nTri ];
		VectorAligned p0 = pPos1[ tri.nNode[ 0 ] ], p1 = pPos1[ tri.nNode[ 1 ] ], p2 = pPos1[ tri.nNode[ 2 ] ];
		float flError = m_pFeModel->RelaxTri0( 1.0f, flConstraintScale, tri, p0, p1, p2 ); NOTE_UNUSED( flError );
		int nRelError = int( 64 * Clamp< float >( flError / Sqr( .125f * Perimeter( tri ) ), 0, 1 ) );
		pDebugOverlay->AddTriangleOverlay( p0, p1, p2, 192 + nRelError, 255 - nRelError, 255, 64, false, 0 );
	}

	CUtlHashtable< uint32 > drawnLines;
	for ( uint nQuad = 0; nQuad < m_pFeModel->m_nQuadCount[ 0 ]; ++nQuad )
	{
		const FeQuad_t &quad = m_pFeModel->m_pQuads[ nQuad ];
		VectorAligned vPos[ 4 ] = { pPos1[ quad.nNode[ 0 ] ], pPos1[ quad.nNode[ 1 ] ], pPos1[ quad.nNode[ 2 ] ], pPos1[ quad.nNode[ 3 ] ] };
		for ( int j = 0; j < 4; ++j )
		{
			int j1 = ( j + 1 ) % 4;
			bool bStatic = j == 0 && nQuad < m_pFeModel->m_nQuadCount[ 2 ];
			uint id1 = quad.nNode[ j ] | ( quad.nNode[ j1 ] << 16 ), id2 = quad.nNode[ j1 ] | ( quad.nNode[ j ] << 16 );
			if ( drawnLines.Find( id1 ) == drawnLines.InvalidHandle() )
			{
				drawnLines.Insert( id1 );
				drawnLines.Insert( id2 );
				pDebugOverlay->AddLineOverlay( vPos[ j ], vPos[ j1 ], bStatic ? 200 : 255, bStatic ? 100 : 255, 200, 64, false, 0 );
			}
		}
	}

	if ( !m_bEnableSimd ) //  ? m_pFeModel->m_nTriCount[ 0 ] : m_pFeModel->m_nSimdTriCount[ 0 ] 
	{
		for ( uint nTri = 0; nTri < m_pFeModel->m_nTriCount[ 0 ]; ++nTri )
		{
			const FeTri_t &tri = m_pFeModel->m_pTris[ nTri ];
			VectorAligned vPos[ 3 ] = { pPos1[ tri.nNode[ 0 ] ], pPos1[ tri.nNode[ 1 ] ], pPos1[ tri.nNode[ 2 ] ] };
			for ( int j = 0; j < 3; ++j )
			{
				int j1 = ( j + 1 ) % 3;
				bool bStatic = j == 0 && nTri < m_pFeModel->m_nTriCount[ 2 ];
				uint id1 = tri.nNode[ j ] | ( tri.nNode[ j1 ] << 16 ), id2 = tri.nNode[ j1 ] | ( tri.nNode[ j ] << 16 );
				if ( drawnLines.Find( id1 ) == drawnLines.InvalidHandle( ) )
				{
					drawnLines.Insert( id1 );
					drawnLines.Insert( id2 );
					pDebugOverlay->AddLineOverlay( vPos[ j ], vPos[ j1 ], bStatic ? 200 : 255, bStatic ? 100 : 255, 200, 64, false, 0 );
				}
			}
		}
	}
	else
	{
		for ( int nSimdTri = 0; nSimdTri < m_pFeModel->m_nSimdTriCount[ 0 ]; ++nSimdTri )
		{
			const FeSimdTri_t &tri = m_pFeModel->m_pSimdTris[ nSimdTri ];
			for ( int q = 0; q < 4; ++q )
			{
				VectorAligned vPos[ 3 ] = { pPos1[ tri.nNode[ 0 ][ q ] ], pPos1[ tri.nNode[ 1 ][ q ] ], pPos1[ tri.nNode[ 2 ][ q ] ] };
				for ( int j = 0; j < 3; ++j )
				{
					int j1 = ( j + 1 ) % 3;
					bool bStatic = j == 0 && nSimdTri < m_pFeModel->m_nSimdTriCount[ 2 ];
					uint id1 = tri.nNode[ j ][ q ] | ( tri.nNode[ j1 ][ q ] << 16 ), id2 = tri.nNode[ j1 ][ q ] | ( tri.nNode[ j ][ q ] << 16 );
					if ( drawnLines.Find( id1 ) == drawnLines.InvalidHandle( ) )
					{
						drawnLines.Insert( id1 );
						drawnLines.Insert( id2 );
						pDebugOverlay->AddLineOverlay( vPos[ j ], vPos[ j1 ], bStatic ? 200 : 255, bStatic ? 100 : 255, 200, 64, false, 0 );
					}
				}
			}
		}
	}

	for ( int nSimdRod = 0; nSimdRod < m_pFeModel->m_nSimdRodCount; ++nSimdRod )
	{
		const FeSimdRodConstraint_t &rod = m_pFeModel->m_pSimdRods[ nSimdRod ];
		for ( int q = 0; q < 4; ++q )
		{
			int v0 = rod.nNode[ 0 ][ q ], v1 = rod.nNode[ 1 ][ q ];
			VectorAligned vPos[ 2 ] = { pPos1[ v0 ], pPos1[ v1 ] };
			bool bStatic = SubFloat( rod.f4Weight0, q ) == 0.0f || SubFloat( rod.f4Weight0, q ) == 1.0f ;
			bool bVirtual = virtualCtrls[ m_pFeModel->NodeToCtrl( v0 ) ] || virtualCtrls[ m_pFeModel->NodeToCtrl( v1 ) ];
			uint id1 = v0 | ( v1 << 16 ), id2 = v1 | ( v0 << 16 );
			if ( drawnLines.Find( id1 ) == drawnLines.InvalidHandle( ) )
			{
				drawnLines.Insert( id1 );
				drawnLines.Insert( id2 );
				int r = 200, g = 200, b = 255;
				if ( bStatic )
				{
					r = 150; b = 100;
				}
				if ( bVirtual )
				{
					r /= 2; g /= 2; b /= 2;
				}
				pDebugOverlay->AddLineOverlay( vPos[ 0 ], vPos[ 1 ], r, g, b, 64, false, 0 );
			}
		}
	}


	if ( nDebugLayers & RN_DRAW_SOFTBODY_BASES )
	{
		for ( int nCtrl = 0; nCtrl < m_nParticleCount; ++nCtrl )
		{
			uint nNode = m_pFeModel->CtrlToNode( nCtrl );
			if ( nNode < m_nNodeCount && !virtualCtrls.IsBitSet( nNode ) )
			{
				float flAxisScale = ( nNode < m_pFeModel->m_nRotLockStaticNodes ? 2.0f : 5.0f );

				// this is a dynamic node
				const matrix3x4a_t &node = pSim[ nCtrl ];
				pDebugOverlay->AddLineOverlay( node.GetOrigin(), node.GetOrigin() + node.GetColumn( X_AXIS ) * flAxisScale * 2, 255, 180, 180, 255, false, 0 );
				pDebugOverlay->AddLineOverlay( node.GetOrigin(), node.GetOrigin() + node.GetColumn( Y_AXIS ) * flAxisScale, 180, 255, 180, 255, false, 0 );
				pDebugOverlay->AddLineOverlay( node.GetOrigin(), node.GetOrigin() + node.GetColumn( Z_AXIS ) * flAxisScale, 180, 180, 255, 255, false, 0 );
				const matrix3x4a_t &anim = GetAnim( nCtrl );
				float flAnimAxisScale = 0.5f * flAxisScale;
				pDebugOverlay->AddLineOverlay( anim.GetOrigin( ), anim.GetOrigin( ) + anim.GetColumn( X_AXIS ) * flAnimAxisScale, 200, 128, 128, 255, false, 0 );
				pDebugOverlay->AddLineOverlay( anim.GetOrigin( ), anim.GetOrigin( ) + anim.GetColumn( Y_AXIS ) * flAnimAxisScale, 128, 200, 128, 255, false, 0 );
				pDebugOverlay->AddLineOverlay( anim.GetOrigin( ), anim.GetOrigin( ) + anim.GetColumn( Z_AXIS ) * flAnimAxisScale, 128, 128, 200, 255, false, 0 );
			}
		}
	}

	float flMaxErr = 0, flMaxVel = 0;
	Vector vMaxErr = vec3_origin, vMaxVel = vec3_origin;

	if ( nDebugLayers & RN_DRAW_SOFTBODY_FIELDS )
	{
		for ( uint nNode = 0; nNode < m_nNodeCount; ++nNode )
		{
			Vector p1 = pPos1[ nNode ], p0 = pPos0[ nNode ];
			pDebugOverlay->AddLineOverlay( p1, p0, ( p1 - p0 ).Length() > 70.0f ? 255 : 150, 150, 150, 255, false, 0 );
			uint nCtrl = m_pFeModel->NodeToCtrl( nNode );
			//if ( *m_pFeModel->GetCtrlName( nCtrl ) )
			if ( !virtualCtrls[ nCtrl ] 
				 && m_pFeModel->m_pNodeIntegrator
				&& ( m_pFeModel->m_pNodeIntegrator[ nNode ].flAnimationForceAttraction != 0 || m_pFeModel->m_pNodeIntegrator[ nNode ].flAnimationVertexAttraction != 0 )
			)
			{
				// only draw anim-sim lines for real nodes. Virtual nodes have no corresponding bones, so we don't care about them too much
				Vector vAnim = GetAnim( nCtrl ).GetOrigin( ), vSim = pSim[ nCtrl ].GetOrigin( );
				pDebugOverlay->AddLineOverlay( pPos1[ nNode ], vAnim, 120, 120, 200, 255, false, 0 );
				if ( nNode >= m_pFeModel->m_nStaticNodes )
				{
					pDebugOverlay->AddLineOverlay( vSim, vAnim, 120, 120, 120, 255, false, 0 );
				}
				float flErr = ( vSim - vAnim ).Length();
				if ( flErr > flMaxErr )
				{
					flMaxErr = flErr;
					vMaxErr = vAnim;
				}
				if ( nNode >= m_pFeModel->m_nStaticNodes )
				{
					float flVel = ( pPos1[ nNode ] - pPos0[ nNode ] ).Length( ) / m_flLastTimestep;
					if ( flVel > flMaxVel )
					{
						flMaxVel = flVel;
						vMaxVel = pPos1[ nNode ];
					}
				}
			}
		}
	}

	if ( flMaxErr > 1 )
	{
		pDebugOverlay->AddTextOverlay( vMaxErr, 0, 0, "%.2f", flMaxErr );
	}
	if ( flMaxVel > 1 )
	{
		pDebugOverlay->AddTextOverlay( vMaxVel, 1, 0.0f, "v=%.1f", flMaxVel );
	}
	
	if ( nDebugLayers & RN_DRAW_SOFTBODY_INDICES )
	{
		for ( uint nNode = 0; nNode < m_nNodeCount; ++nNode )
		{
			pDebugOverlay->AddTextOverlay( pPos1[ nNode ], 0, ( nNode < m_pFeModel->m_nStaticNodes ? "*%d %s" : "%d %s" ), nNode, m_pFeModel->GetNodeName( nNode ) );
		}
	}

	{
		if ( m_bEnableSimd )
		{
			struct CLabel
			{
				CLabel() : m_flHighlight( 1 ){}
				CUtlString m_Text;
				Vector m_vPos0, m_vPos1;
				uint m_nNode0, m_nNode1;
				float m_flHighlight;
				Vector GetPos()const
				{
					return ( m_vPos1 + m_vPos0 ) * .5f;
				}
				void Init( uint nNode0, uint nNode1, const Vector &vPos0, const Vector &vPos1, float flExpectedLength )
				{
					m_nNode1 = nNode1;
					m_nNode0 = nNode0;
					m_vPos0 = vPos0;
					m_vPos1 = vPos1;
					float flErr = ( vPos0 - vPos1 ).Length() / flExpectedLength - 1.0f;
					//if ( fabsf( flErr ) > 0.1f )
					{
						m_Text = CFmtStr( "%+.2f*%.1f in ", flErr, flExpectedLength ).Get();
					}
				}
			};
			static CUtlVector< CLabel > labelPool;
			labelPool.EnsureCapacity( m_pFeModel->m_nSimdRodCount );
			labelPool.RemoveAll();
			CUtlHashtable< uint32, int > labels;
			for ( uint nSimdRod = 0; nSimdRod < m_pFeModel->m_nSimdRodCount; ++nSimdRod )
			{
				const FeSimdRodConstraint_t &simdRod = m_pFeModel->m_pSimdRods[ nSimdRod ];
				for ( int nLane = 0; nLane < 4; ++nLane )
				{
					uint n0 = simdRod.nNode[ 0 ][ nLane ], n1 = simdRod.nNode[ 1 ][ nLane ];
					Vector vPos = ( pPos1[ n0 ] + pPos1[ n1 ] ) * 0.5f;
					float flHighlight = m_pEnvironment ? m_pEnvironment->GetDebugHighlightCone().Highlight( vPos ) : 1.0f;
					if ( flHighlight <= 0.0f )
						continue;
					if ( n0 > n1 )
						Swap( n0, n1 );

					uint nNodeKey = n0 | ( n1 << 16 );
					int nLabel = labels.Get( nNodeKey, -1 );
					CLabel *pLabel ;
					if ( nLabel < 0 )
					{
						nLabel = labelPool.AddToTail();
						pLabel = &labelPool[ nLabel ];
						pLabel->Init( n0, n1, pPos1[ n0 ], pPos1[ n1 ], SubFloat( simdRod.f4MaxDist, nLane ) );
						pLabel->m_flHighlight = flHighlight;
						labels.Insert( nNodeKey, nLabel  );
					}
					else
					{
						pLabel = &labelPool[ nLabel ];
						pLabel->m_Text.Append( ", " );
					}
					pLabel->m_Text.Append( CFmtStrN< 64 >( "%u.%u", nSimdRod, nLane ) );
				}
			}

			for ( UtlHashHandle_t iter = labels.FirstHandle(); iter != labels.InvalidHandle(); iter = labels.NextHandle( iter ) )
			{
				//uint nNodeKey = labels.Key( iter );
				CLabel *pLabel = &labelPool[ labels.Element( iter ) ];
				pDebugOverlay->AddTextOverlay( pLabel->GetPos(), 0.0f, pLabel->m_flHighlight, "%s", pLabel->m_Text.Get() );
			}
		}
		else
		{
			for ( uint nRod = 0; nRod < m_pFeModel->m_nRodCount; ++nRod )
			{
				const FeRodConstraint_t &rod = m_pFeModel->m_pRods[ nRod ];
				pDebugOverlay->AddTextOverlay( ( pPos1[ rod.nNode[ 0 ] ] + pPos1[ rod.nNode[ 1 ] ] ) * 0.5f, 0.0f, .5f, "rod%u", nRod );
			}
		}
	}
}


uint CSoftbody::ComputeVirtualCtrls( CVarBitVec &virtualCtrls )
{
	for ( uint nOffset = 0; nOffset < m_pFeModel->m_nCtrlOsOffsets; ++nOffset )
	{
		virtualCtrls.Set( m_pFeModel->m_pCtrlOsOffsets[ nOffset ].nCtrlChild );
	}

	for ( uint nOffset = 0; nOffset < m_pFeModel->m_nCtrlOffsets; ++nOffset )
	{
		virtualCtrls.Set( m_pFeModel->m_pCtrlOffsets[ nOffset ].nCtrlChild );
	}
	return m_pFeModel->m_nCtrlOsOffsets + m_pFeModel->m_nCtrlOffsets;
}



void CSoftbody::SetDebugSelection( int nSel )
{
	m_nDebugSelection = nSel;
}







int CSoftbody::CastCone( const Vector &vStart, const Vector &vDir, float flConePitch )
{
	float flClosest = FLT_MAX, flPrecision = FLT_MAX;
	int nClosest = 0;
	float flDir = vDir.LengthSqr();
	Vector vNormalizedDir = vDir / flDir;
	for ( int nNode = 0; nNode < m_nNodeCount; ++nNode )
	{
		Vector vPos = m_pPos1[ nNode ] - vStart;
		float flDist = DotProduct( vPos, vNormalizedDir );
		Vector vOrthoDist = vPos - vNormalizedDir * flDist;
		float flOrthoDistSqr = vOrthoDist.LengthSqr( );
		float flThresholdSqr = Sqr( flConePitch * flDist );
		if ( flDist > 0 && flDist < flClosest && flOrthoDistSqr < flThresholdSqr )
		{
			flClosest  = flDist;
			nClosest = nNode;
			flPrecision = flOrthoDistSqr;
		}
	}
	return nClosest;
}


void CSoftbody::SetUserData( uint nIndex, void *pData )
{
	if ( nIndex < ARRAYSIZE( m_pUserData ) )
	{
		m_pUserData[ nIndex ] = ( uintp ) pData;
	}
}

void* CSoftbody::GetUserData( uint nIndex )
{
	return ( nIndex < ARRAYSIZE( m_pUserData ) ) ? ( void* )( m_pUserData[ nIndex ] ): NULL;
}



float CSoftbody::GetEnergy( PhysicsSoftbodyEnergyTypeEnum_t nEnergy )const
{
	switch ( nEnergy )
	{
	case SOFTBODY_ENERGY_ELASTIC:
		return GetElasticEnergy( );
	case SOFTBODY_ENERGY_KINEMATIC:
		return GetKinematicEnergy( );
	case SOFTBODY_ENERGY_POTENTIAL:
		return GetPotentialEnergy( );
	default:
		return GetElasticEnergy( ) + GetKinematicEnergy( ) + GetPotentialEnergy( );
	}
}



float CSoftbody::GetElasticEnergy( )const
{
	float dtInv = 1.0f / m_flLastTimestep, dtInvSqr = Sqr( dtInv );

	float flConstraintScale = m_flModelScale * m_flClothScale;

	float flRodEnergy = m_pFeModel->ComputeElasticEnergyRods( m_pPos1, flConstraintScale ) * dtInvSqr, flQuadEnergy = m_pFeModel->ComputeElasticEnergyQuads( m_pPos1, flConstraintScale ) * dtInvSqr;
	float flSpringEnergy = m_pFeModel->ComputeElasticEnergySprings( m_pPos0, m_pPos1, m_flLastTimestep, flConstraintScale );

	return flRodEnergy + flQuadEnergy + flSpringEnergy;
}



float CSoftbody::GetPotentialEnergy( )const
{
	const FeNodeIntegrator_t *pNodeIntegrator = m_pFeModel->m_nDynamicNodeFlags & ( FE_FLAG_HAS_NODE_DAMPING | FE_FLAG_HAS_ANIMATION_FORCE_ATTRACTION ) ? m_pFeModel->m_pNodeIntegrator : NULL;
	float flTimeStep = m_flLastTimestep;

	float flPotentialEnergy = 0;

	for ( uint nDynNode = m_pFeModel->m_nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
	{
		float flDamping = 0.0f;
		float flGravity = GetEffectiveGravityScale( );

		if ( pNodeIntegrator )
		{
			const FeNodeIntegrator_t &integrator = pNodeIntegrator[ nDynNode ];
			flDamping = integrator.flPointDamping * flTimeStep * m_flDampingMultiplier;
			flGravity *= integrator.flGravity;
		}
		const VectorAligned &pos1 = m_pPos1[ nDynNode ];

		flPotentialEnergy += ( pos1.z * Max( 0.0f, 1.0f - flDamping ) * flGravity ) / m_pFeModel->m_pNodeInvMasses[ nDynNode ]; // mgh=hgm
	}
	return flPotentialEnergy;
}


float CSoftbody::GetKinematicEnergy( )const
{
	const FeNodeIntegrator_t *pNodeIntegrator = m_pFeModel->m_nDynamicNodeFlags & ( FE_FLAG_HAS_NODE_DAMPING | FE_FLAG_HAS_ANIMATION_FORCE_ATTRACTION ) ? m_pFeModel->m_pNodeIntegrator : NULL;
	float flTimeStep = m_flLastTimestep;

	float flKinematicEnergy = 0;

	for ( uint nDynNode = m_pFeModel->m_nStaticNodes; nDynNode < m_nNodeCount; ++nDynNode )
	{
		float flDamping = 0.0f;
		if ( pNodeIntegrator )
		{
			const FeNodeIntegrator_t &integrator = pNodeIntegrator[ nDynNode ];
			flDamping = integrator.flPointDamping * flTimeStep * m_flDampingMultiplier;
		}
		const VectorAligned &pos0 = m_pPos0[ nDynNode ], &pos1 = m_pPos1[ nDynNode ];

		flKinematicEnergy += ( ( pos1 - pos0 ) * Max( 0.0f, 1.0f - flDamping ) / m_flLastTimestep ).LengthSqr() * 0.5f / m_pFeModel->m_pNodeInvMasses[ nDynNode ];
	}
	return flKinematicEnergy;
}


Vector CSoftbody::GetCtrlVelocity( uint nCtrl ) const
{
	return GetNodeVelocity( m_pFeModel->CtrlToNode( nCtrl ) );  
}

//--------------------------------------------------------------------------------------------------
void CSoftbody::EnableDebugRendering( bool bEnable )
{
	m_bDebugDraw = bEnable; 
}



void CSoftbody::SetNodePositions( SetNodePositionsParams_t &params )
{
	CHANGE_GUARD();
	SetAbsOrigin( params.vAbsOrigin, true );
	SetAbsAngles( params.vAbsAngles, true );
	V_memcpy( GetNodePositions( params.nFrame ), params.pPos, sizeof( *params.pPos ) * Min<uint>( params.nCount, m_nNodeCount ) );
	m_bSimTransformsOutdated = true;
}
void CSoftbody::SetNodePositions( const Vector *pPos, int nCount, int nFrame )
{
	CHANGE_GUARD();
	VectorAligned *pInternalPos = GetNodePositions( nFrame );
	uint nUsefulNodeCount = Min<uint>( nCount, m_nNodeCount );
	for ( uint nNode = 0; nNode < nUsefulNodeCount; ++nNode )
	{
		pInternalPos[ nNode ] = pPos[ nNode ];
	}
	m_bSimTransformsOutdated = true;
}

