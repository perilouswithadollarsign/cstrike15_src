//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// A port of CRnSoftbody
// Note: mathlib is tentative place for this code. We will probably move it to a separate lib or dll or vphysics.dll
//
#ifndef MATHLIB_SOFTBODY_HDR
#define MATHLIB_SOFTBODY_HDR

#include "rubikon/param_types.h"
#include "rubikon/debugname.h"
#include "rubikon/intersection.h"

// #include "rnmath.h"
// #include "geometry.h"
// #include "mass.h"
// #include "graphedge.h"
// #include "collisionfilter.h"
// #include "rnserialize.h"
// #include "legacyobject.h"
#include "tier1/utlbuffer.h"
#include "mathlib/femodel.h"
#include "tier1/hierarchicalbitvec.h"
#include "rubikon/serializehelpers.h"

class CRnSoftbodyDesc;
class CSoftbodyEnvironment;
class CRnBody;
class CRnJoint;
struct PhysSoftbodyDesc_t;
class CJob ;
class CSoftbody;
class CFeModel;
struct FilterTransformsParams_t;
struct SetNodePositionsParams_t;
class CFeModelReplaceContext;
class IVDebugOverlay;
class IMesh;


struct FilterTransformsParams_t
{
	matrix3x4a_t *pOutputWorldTransforms;
	const int16 *pCtrlToBone;
	const uint32 *pValidTransforms;
	VectorAligned *pNodePos;
	bool flMatrixScale; // Scale to convey in matrices; 1.0 will not scale anything (appropriate for Hammer conventions). 0.0 means use the model scale (appropriate for game conventions)
};


struct SetNodePositionsParams_t
{
	SetNodePositionsParams_t()
	{
		nFrame = 1;
	}
	const VectorAligned *pPos;
	int nCount;
	Vector vAbsOrigin;
	QAngle vAbsAngles;
	int nFrame;
};



class CParticleGlue
{
public:
	float m_flStickiness;
	float m_flWeight1; // 0: only use parent [0], >0 : blend from [0] to [1]
	uint16 m_nParentNode[ 2 ]; // this is actual node index; we're mostly parented to static nodes
public:
	CParticleGlue(){}
	CParticleGlue( uint nParentNode, float flStickiness )
	{
		m_flStickiness = flStickiness;
		m_flWeight1 = 0;
		m_nParentNode[ 0 ] = m_nParentNode[ 1 ] = nParentNode;
	}
};


class ALIGN16 CSoftbody
{
public:
	CSoftbody( void );
	CSoftbody( CSoftbodyEnvironment *pWorld, const CFeModel *pFeModel );
	~CSoftbody();

	void Shutdown( );

	void Init( int numModelBones = 0);
	void Init( CSoftbodyEnvironment *pWorld, const CFeModel *pFeModel, int numModelBones );

	void InitFeModel( int numModelBones = 0 );

	void InitDefaults( );
	
	void Integrate( float flTimeStep );
	void IntegrateWind( VectorAligned *pPos, float flTimeStep );
	void RawSimulate( int nIterations, float flTimeStep );
	void ValidatingSimulate( int nIterations, float flTimeStep );
	void AddAnimationAttraction( float flTimeStep );
	void Predict( float flTimeStep );
	void Post( );
	void Collide();
	void CollideWithWorldInternal();
	void CollideWithRigidsInternal();
// 	void CollideTaperedCapsule( uint16 nCollisionMask, const VectorAligned &vCenter0, float flRadius0, const VectorAligned &vCenter1, float flRadius1, const Vector &vAxis, float flDist );
// 	void CollideTaperedCapsule( uint16 nCollisionMask, const VectorAligned &vCenter0, float flRadius0, const VectorAligned &vCenter1, float flRadius1 );

	// @begin_publish
	uint Step( int nIterations, float flTimeStep );
	uint Step( float flTimeStep );
	uint GetStateHash()const;
	void TouchAnimatedTransforms();
	void SetAnimatedTransform( int nParticle, const matrix3x4a_t &transform );
	void SetAnimatedTransforms( const matrix3x4a_t *pSimulationWorldTransforms );
	void SetAnimatedTransformsNoScale( const matrix3x4a_t *pSimulationWorldTransforms );
	matrix3x4a_t* GetParticleTransforms( const VectorAligned *pInputNodePos = NULL, uint nFlags = 0 );
	const VectorAligned* GetNodePositions( int nFrame = 1 ) const { return nFrame ? m_pPos1 : m_pPos0; }
	VectorAligned* GetNodePositions( int nFrame = 1 ) { return nFrame ? m_pPos1 : m_pPos0; }
	void SetNodePositions( const Vector *pPos, int nCount, int nFrame = 1 );
	void SetNodePositions( SetNodePositionsParams_t &params );
	uint GetNodeCount() const { return m_nNodeCount; }
	uint GetCtrlCount() const { return m_nParticleCount; }
	Vector GetNodeVelocity( uint nNode ) const { return ( m_pPos1[ nNode ] - m_pPos0[ nNode ] ) / m_flLastTimestep;  }
	Vector GetCtrlVelocity( uint nCtrl ) const;
	matrix3x4a_t* GetAnimatedTransforms( ) { return m_pParticles; }
	const matrix3x4a_t* GetAnimatedTransforms() const { return m_pParticles; }
	matrix3x4a_t* GetSimulatedTransforms() { return m_pParticles + m_nParticleCount; }
	const matrix3x4a_t* GetSimulatedTransforms() const { return m_pParticles + m_nParticleCount; }
	const CFeModel *GetFeModel()const { return m_pFeModel; }
	CFeModel *GetFeModel( ){ return m_pFeModel; }
	CSoftbodyEnvironment *GetEnvironment( ) const { return m_pEnvironment;  }

	int GetParticleCount( ) const { return m_nParticleCount; }

	void SetDebugNameV( const char* pNameFormat, va_list args ) { m_DebugName.SetV( pNameFormat, args );}
	const char *GetDebugName() const { return m_DebugName.Get(); }
	void AppendDebugInfo( CUtlString &line );
	void Draw( const RnDebugDrawOptions_t &options, IVDebugOverlay* pDebugOverlay );
	void Draw( const RnDebugDrawOptions_t &options, IMesh *pDynamicMesh );
	void SetPose() { SetPose( m_pFeModel->m_pInitPose ); }
	void SetPose( const CTransform *pPose );
	void SetPose( const CTransform &tm ) { SetPose( tm, m_pFeModel->m_pInitPose ); }
	void SetPose( const CTransform &tm, const CTransform *pPose );
	void SetPoseFromBones( const int16 *pCtrlToBone, const matrix3x4a_t *pBones, float flScale = 1.0f );
	void SetCtrl( int nParticle, const CTransform &tm );
	void SetDebugSelection( int nSelection );
	void SetSimFlags( uint nNewSimFlags );
	void AddSimFlags( uint nAddSimFlags ) { SetSimFlags( m_nSimFlags | nAddSimFlags ); }
	void ClearSimFlags( uint nClearSimFlags ) { SetSimFlags( m_nSimFlags & ~nClearSimFlags ); }
	void SetAnimSpace( PhysicsSoftbodyAnimSpace_t nAnimSpace ) { m_nAnimSpace = nAnimSpace; }
	PhysicsSoftbodyAnimSpace_t GetAnimSpace()const { return m_nAnimSpace; }
	uint GetSimFlags()const { return m_nSimFlags; }
	int CastCone( const Vector &vStart, const Vector &vDir, float flConePitch );
	uint GetContactCount( )const { return 0; }

	void SetOverPredict( float flOverPredict ) { m_flOverPredict = flOverPredict;  }
	float GetOverPredict( ) const { return m_flOverPredict; }

	void ResetVelocities( );

	void SetThreadStretch( float flBendUnderRelax ) { m_flThreadStretch = flBendUnderRelax; }
	float GetThreadStretch(  )const  { return m_flThreadStretch; }

	void SetSurfaceStretch( float flStretchUnderRelax ) { m_flSurfaceStretch = flStretchUnderRelax; }
	float GetSurfaceStretch(  )const { return m_flSurfaceStretch; }

	void SetStepUnderRelax( float flStepUnderRelax ) { m_flStepUnderRelax = flStepUnderRelax;  }
	float GetStepUnderRelax( void ) const { return m_flStepUnderRelax; }
	AABB_t BuildBounds( )const;
	void FilterTransforms( const FilterTransformsParams_t &params );
	void FilterTransforms( matrix3x4a_t *pModelBones );

	void SetGravityScale( float flScale ) { m_flGravityScale = flScale; }
	void EnableGravity( bool bEnableGravity ) { m_bGravityDisabled = !bEnableGravity; }
	void EnableGravity( ) { m_bGravityDisabled = false; }
	void DisableGravity( ) { m_bGravityDisabled = true; }
	bool IsGravityEnabled( ) const { return !m_bGravityDisabled; }
	bool IsGravityDisabled( ) const { return m_bGravityDisabled; }
	bool IsFtlPassEnabled( ) const { return m_bEnableFtlPass; }
	void EnableFtlPass( bool bEnable ) { m_bEnableFtlPass = bEnable; }

	float GetGravityScale( void ) const{ return m_flGravityScale; }
	Vector GetEffectiveGravity( void ) const;
	float GetEffectiveGravityScale( void ) const;
	void EnableAnimationAttraction( bool bEnable ) { m_bEnableAnimationAttraction = bEnable; }
	bool IsAnimationAttractionEnabled( )const { return m_bEnableAnimationAttraction; }
	void EnableFollowNode( bool bEnable ) { m_bEnableFollowNodes = bEnable;  }
	bool IsFollowNodeEnabled( ) const { return m_bEnableFollowNodes;  }
	void EnableSprings( bool bEnable ){ m_bEnableSprings = bEnable;  }
	bool AreSpringsEnabled( )const { return m_bEnableSprings; }
	void EnableInclusiveCollisionSpheres( bool bEnable ) { m_bEnableInclusiveCollisionSpheres = bEnable; }
	bool AreInclusiveCollisionSpheresEnabled( ) const { return m_bEnableInclusiveCollisionSpheres; }
	void EnableExclusiveCollisionSpheres( bool bEnable ) { m_bEnableExclusiveCollisionSpheres = bEnable; }
	bool AreExclusiveCollisionSpheresEnabled( ) const { return m_bEnableExclusiveCollisionSpheres; }
	void EnableCollisionPlanes( bool bEnable ) { m_bEnableCollisionPlanes = bEnable; }
	bool AreCollisionPlanesEnabled( ) const { return m_bEnableCollisionPlanes; }
	void EnableGroundCollision( bool bEnable ) { m_bEnableGroundCollision = bEnable; }
	bool IsGroundCollisionEnabled( ) const { return m_bEnableGroundCollision; }
	void EnableGroundTrace( bool bEnable ) { m_bEnableGroundTrace = bEnable; }
	bool IsGroundTraceEnabled( ) const { return m_bEnableGroundTrace; }

	float GetTimeStep( void )const { return m_flLastTimestep;  }
	void SetModelScale( float flModelScale ) { m_flModelScale = flModelScale; }
	float GetModelScale( )const { return m_flModelScale; } 
	void SetVelocityDamping( float flDamping ) { m_flVelocityDamping = flDamping; } // 1.0f - full damping; 0.0 - no damping (default)
	float GetVelocityDamping( )const { return m_flVelocityDamping; }
	void SetUserData( uint nIndex, void *pData );
	void* GetUserData( uint nIndex );
	//void SetOrigin( const Vector &vAbsOrigin );
	void InitializeTransforms( const int16 *pCtrlToBone, const matrix3x4a_t *pSimulationWorldTransforms );
	void SetAbsAngles( const QAngle &vNewAngles, bool bTeleport );
	const QAngle GetAbsAngles() const { return m_vSimAngles; }
	void SetAbsOrigin( const Vector &vNewOrigin, bool bTeleport );
	const Vector GetAbsOrigin()const { return m_vSimOrigin; }
	float GetEnergy( PhysicsSoftbodyEnergyTypeEnum_t nEnergy )const;
	float GetElasticEnergy( )const;
	float GetPotentialEnergy( )const;
	float GetKinematicEnergy( )const;
	void SetDampingMultiplier( float flMul ) { m_flDampingMultiplier = flMul; }
	float GetDampingMultiplier( )const { return m_flDampingMultiplier; }
	void SetGroundZ( float flGroundZ ) { m_vGround.Init( m_vSimOrigin.x, m_vSimOrigin.y, flGroundZ );  }
	float GetGroundZ( )const { return m_vGround.z; }

	bool IsDormant( )const;
	void GoDormant( );
	bool AdvanceSleepCounter();
	void GoWakeup( );
	bool IsActive()const;
	bool BeforeFilterTransforms( );
	void SetDebugDrawTreeBeginLevel( int nLevel ) { m_nDebugDrawTreeBeginLevel = nLevel; }
	int GetDebugDrawTreeBeginLevel() { return m_nDebugDrawTreeBeginLevel; }
	void SetDebugDrawTreeEndLevel( int nLevel ) { m_nDebugDrawTreeEndLevel = nLevel; }
	int GetDebugDrawTreeEndLevel() { return m_nDebugDrawTreeEndLevel; }
	void SetDebugDrawTreeFlags( uint nFlags ) { m_nDebugDrawTreeFlags = nFlags; }
	uint GetDebugDrawTreeFlags(){ return m_nDebugDrawTreeFlags; }
	void EnableDebugDraw( bool bEnable ){ m_bDebugDraw = bEnable; }
	void EnableDebugRendering( bool bEnable );

	float GetVelAirDrag() const { return m_flVelAirDrag; }
	void SetVelAirDrag( float flVelAirDrag ){ m_flVelAirDrag = flVelAirDrag; }
	float GetExpAirDrag() const { return m_flExpAirDrag; }
	void SetExpAirDrag( float flExpAirDrag ){ m_flExpAirDrag = flExpAirDrag; }
	float GetVelQuadAirDrag() const { return m_flVelQuadAirDrag; }
	void SetVelQuadAirDrag( float flVelQuadAirDrag ){ m_flVelQuadAirDrag = flVelQuadAirDrag; }
	float GetExpQuadAirDrag() const { return m_flExpQuadAirDrag; }
	void SetExpQuadAirDrag( float flExpQuadAirDrag ){ m_flExpQuadAirDrag = flExpQuadAirDrag; }
	float GetVelRodAirDrag() const { return m_flVelRodAirDrag; }
	void SetVelRodAirDrag( float flVelRodAirDrag ){ m_flVelRodAirDrag = flVelRodAirDrag; }
	float GetExpRodAirDrag() const { return m_flExpRodAirDrag; }
	void SetExpRodAirDrag( float flExpRodAirDrag ){ m_flExpRodAirDrag = flExpRodAirDrag; }
	float GetQuadVelocitySmoothRate()const { return m_flQuadVelocitySmoothRate; }
	void SetQuadVelocitySmoothRate( float flRate ){ m_flQuadVelocitySmoothRate = flRate; }
	float GetRodVelocitySmoothRate()const { return m_flRodVelocitySmoothRate; }
	void SetRodVelocitySmoothRate( float flRate ){ m_flRodVelocitySmoothRate = flRate; }
	uint16 GetQuadVelocitySmoothIterations()const { return m_nQuadVelocitySmoothIterations; }
	void SetQuadVelocitySmoothIterations( uint16 nIterations ){ m_nQuadVelocitySmoothIterations = nIterations; }
	uint16 GetRodVelocitySmoothIterations()const { return m_nRodVelocitySmoothIterations; }
	void SetRodVelocitySmoothIterations( uint16 nIterations ){ m_nRodVelocitySmoothIterations = nIterations; }

	const RnCollisionAttr_t &GetCollisionAttributes() const { return m_CollisionAttributes; }
	void SetCollisionAttributes( const RnCollisionAttr_t &attr );
	int GetIndexInWorld() const { return m_nIndexInWorld; }

	void ReplaceFeModel( CFeModelReplaceContext &context );
	matrix3x4_t GetDifferenceTransform( const Vector &vAltOrigin, const QAngle &vAltAngles );
	void ComputeInterpolatedNodePositions( float flFactor, VectorAligned *pPosOut );
	void SetInstanceSettings( void *pSettings );
	void SetFrozen( bool bFrozen ) { m_bFrozen = true; }
	bool IsFrozen()const { return m_bFrozen; }
	void SetVolumetricSolveAmount( float flVolumetricSolveAmount ) { m_flVolumetricSolveAmount = flVolumetricSolveAmount; }
	float GetVolumetricSolveAmount()const { return m_flVolumetricSolveAmount; }
	void ParseParticleState( CUtlBuffer &buf, float flTimeStep );
	CUtlString PrintParticleState( )const;
	int16 *GetModelBoneToCtrl() { return m_pModelBoneToCtrl; }
	void BindModelBoneToCtrl( int nModelBone, int nCtrl );
	//bool SetupCtrl( uint nCtrl, matrix3x4a_t &writeBone );
	// @end_publish

	void DebugDump( );
	void UpdateCtrlOffsets( bool bOverridePose );

	uint ComputeVirtualCtrls( CVarBitVec &virtualNodes );

	matrix3x4a_t &GetAnim( int i ) { return GetAnimatedTransforms()[ i ]; }
	const matrix3x4a_t &GetAnim( int i ) const { return GetAnimatedTransforms()[ i ]; }
	matrix3x4a_t &GetSim( int i )  { return GetSimulatedTransforms()[ i ]; }
	const matrix3x4a_t &GetSim( int i ) const { return GetSimulatedTransforms()[ i ]; }
	uint ShouldUsePreconditioner()const { return m_nSimFlags & ( SOFTBODY_SIM_DIAGONAL_PRECONDITIONER | SOFTBODY_SIM_TRIDIAGONAL_PRECONDITIONER | SOFTBODY_SIM_RELAXATION_PRECONDITIONER ); }

	class CWorldIndexPred
	{
	public:
		static int GetIndex( const CSoftbody *pBody ) { return pBody->m_nIndexInWorld; }
		static void SetIndex( CSoftbody *pBody, int nIndex ) { pBody->m_nIndexInWorld = nIndex; }
	};

	friend class CWorldIndexPred;
	uint GetParticleArrayCount( ) const { return m_nParticleCount * 2; }

	void Validate();
	void DebugPreStep( float flTimeStep );
	void DebugPostStep();

	class CConstraintIterator
	{
	public:
		CConstraintIterator( CSoftbody *pSoftbody );
		~CConstraintIterator( );
		void Iterate( int nIterations );
	protected:
		CSoftbody *m_pSoftbody;
		CSoftbodyEnvironment *m_pEnvironment;
		CUtlVectorFixedGrowable< VectorAligned, 128 > m_PosBeforeCorrect; // the biggest hero in source1 is Medusa, with 104 nodes (52 useful, 52 virtual)
	};

	friend class CConstraintIterator;

	void GlueNode( uint nDynNode, uint nParentNode, float flStickiness );
	void GlueNode( uint nDynNode, uint nParentNode0, uint nParentNode1, float flStickiness, float flWeight1 );
	void GlueNode( uint nDynNode, const CParticleGlue &glue, float flReplacementStickiness );
	void DebugTraceMove( const char *pMsg );
protected:
	void Integrate_S1( float flTimeStep );
	void ResolveStretch_S1( float flTimeStep );
	void ResolveAnimAttraction_S1( float flTimeStep );

protected:
	friend class CRnSoftbodyChangeGuard;
	CRnDebugName m_DebugName;
	CSoftbodyEnvironment *m_pEnvironment;
	RnCollisionAttr_t m_CollisionAttributes;

	CFeModel *m_pFeModel;  // Finite Element Model
	float m_flThreadStretch; // positive: underrelax; negative: overrelax
	float m_flSurfaceStretch;
	float m_flStepUnderRelax;
	float m_flOverPredict; // 0 : normal integration; positive: overpredict, correct, step back
	float m_flGravityScale;
	uint m_nNodeCount;     // actual simulated node count (includes static and dynamic nodes: even though static nodes are not simulated, we need their coordinates to simulate the other nodes connected to them)
	uint m_nParticleCount; // Ctrl count
	float m_flModelScale;
	float m_flVelocityDamping;
	float m_flDampingMultiplier;
	float m_flClothScale;
	Vector m_vGround;

	Vector m_vSimOrigin;
	QAngle m_vSimAngles;

	matrix3x4a_t *m_pParticles SERIALIZE_ARRAY_SIZE( GetParticleArrayCount() );
	VectorAligned *m_pPos0 SERIALIZE_ARRAY_SIZE( m_nNodeCount );
	VectorAligned *m_pPos1 SERIALIZE_ARRAY_SIZE( m_nNodeCount );
	FeAabb_t *m_pAabb SERIALIZE_ARRAY_SIZE( m_pFeModel->GetDynamicNodeCount() - 1 );

	int16 *m_pModelBoneToCtrl;
	int16 *m_pCtrlToModelBone;

	CHierarchicalBitVector m_StickyBuffer; // sticky particles
	CParticleGlue *m_pParticleGlue SERIALIZE_ARRAY_SIZE( m_pFeModel->GetDynamicNodeCount() );

	enum StateEnum_t
	{
		STATE_ACTIVE, // actively simulating, taking in transforms, filtering out transforms
		STATE_DORMANT,// not simulating, not taking in transforms, not filtering anything
		STATE_WAKEUP,	// StateCounter == 0 : not simulating, readying to take in transforms, not filtering anything, not copying transforms
						// StateCounter >  0 : not simulating, taking in transforms, not filtering anything, copying transforms
		STEPS_INVISIBLE_BEFORE_DORMANT = 12,
		FRAMES_INVISIBLE_BEFORE_DORMANT = 3
	};

	uint32 m_nSimFlags;
	uint32 m_nStepsSimulated;

	int8 m_nDebugDrawTreeEndLevel;
	int8 m_nDebugDrawTreeBeginLevel;
	uint8 m_nDebugDrawTreeFlags;

	// STATE_ACTIVE: how many steps we've taken without having FilterTransforms called once
	// STATE_DORMANT: doesn't matter
	// STATE_WAKEUP: how many times we've set animated transforms (need 2 to switch to ACTIVE state)
	uint8 m_nStateCounter; 
	float m_flLastTimestep;
	float m_flVelAirDrag;
	float m_flExpAirDrag;
	float m_flVelQuadAirDrag;
	float m_flExpQuadAirDrag;
	float m_flVelRodAirDrag;
	float m_flExpRodAirDrag;
	float m_flQuadVelocitySmoothRate;
	float m_flRodVelocitySmoothRate;
	float m_flVolumetricSolveAmount;
	uint16 m_nQuadVelocitySmoothIterations;
	uint16 m_nRodVelocitySmoothIterations;

	int m_nIndexInWorld;
	int m_nDebugSelection;
	
	Vector m_vRopeOffset; // <sergiy> a horrible S1 cloth rope hack I'm faithfully replicating so that dota cloth looks exactly the same

	uintp m_pUserData[ 2 ] ; 
	StateEnum_t m_nActivityState;

	//uint m_nDebugNode;
	uint8 m_nEnableWorldShapeCollision : 4;
	PhysicsSoftbodyAnimSpace_t m_nAnimSpace : 2;
	bool m_bAnimTransformChanged : 1; // True means that the animation transforms, that the game sets from outside, have changed and need to be propagated into the simulation
	bool m_bSimTransformsOutdated : 1; // True means that the sim transforms, that the game queries from outside, are out of date and need to be copied from the simulation (and their rotations computed)
	bool m_bGravityDisabled : 1;
	bool m_bEnableAnimationAttraction : 1;
	bool m_bEnableFollowNodes : 1;
	bool m_bEnableSprings : 1;
	bool m_bEnableInclusiveCollisionSpheres : 1;
	bool m_bEnableExclusiveCollisionSpheres : 1;
	bool m_bEnableCollisionPlanes : 1;
	bool m_bEnableGroundCollision : 1;
	bool m_bEnableGroundTrace : 1;
	bool m_bEnableFtlPass : 1;
	bool m_bFrozen : 1;
	bool m_bDebugDraw : 1;
	bool m_bEnableSimd : 1;
	/*
	bool m_bTeleportOnNextSetAbsOrigin : 1;
	bool m_bTeleportOnNextSetAbsAngles : 1;
*/

	friend class CTaperedCapsuleColliderFunctor;
	friend class CGluePredictFunctor;
	friend class CSphereColliderFunctor;
} ALIGN16_POST;



inline void CSoftbody::GlueNode( uint nDynNode, uint nParentNode, float flStickiness )
{
	Assert( nDynNode < m_pFeModel->GetDynamicNodeCount() );
	m_StickyBuffer.Set( nDynNode );
	CParticleGlue &glue = m_pParticleGlue[ nDynNode ];
	glue.m_flStickiness = flStickiness;
	glue.m_flWeight1 = 0;
	glue.m_nParentNode[ 0 ] = nParentNode;
	glue.m_nParentNode[ 1 ] = nParentNode;
}

inline void CSoftbody::GlueNode( uint nDynNode, uint nParentNode0, uint nParentNode1, float flStickiness, float flWeight1 )
{
	Assert( nDynNode < m_pFeModel->GetDynamicNodeCount() );
	m_StickyBuffer.Set( nDynNode );
	CParticleGlue &glue = m_pParticleGlue[ nDynNode ];
	glue.m_flStickiness = flStickiness;
	glue.m_flWeight1 = flWeight1;
	glue.m_nParentNode[ 0 ] = nParentNode0;
	glue.m_nParentNode[ 1 ] = nParentNode1;
}


inline void CSoftbody::GlueNode( uint nDynNode, const CParticleGlue &glueBase, float flReplacementStickiness )
{
	m_StickyBuffer.Set( nDynNode );
	CParticleGlue &glueNode = m_pParticleGlue[ nDynNode ];
	glueNode.m_flStickiness = flReplacementStickiness;
	glueNode.m_flWeight1 = glueBase.m_flWeight1; // 0: only use parent [0], >0 : blend from [0] to [1]
	glueNode.m_nParentNode[ 0 ] = glueBase.m_nParentNode[ 0 ];
	glueNode.m_nParentNode[ 1 ] = glueBase.m_nParentNode[ 1 ];
}


class CSphereColliderFunctor
{
public:
	VectorAligned m_Sphere;
	VectorAligned *m_pDynPos1;
	const float *m_pNodeCollisionRadii;
	CSoftbody *m_pSoftbody;
	float m_flStickiness;
	uint16 m_nParentNode; // needed for gluing particle to this node
public:
	CSphereColliderFunctor(){}
	CSphereColliderFunctor( CSoftbody *pSoftbody, const Vector &vSphereCenter, float flSphereRadius, float flStickiness, uint16 nParentNode )
	{
		m_flStickiness = flStickiness;
		m_nParentNode = nParentNode;
		m_pSoftbody = pSoftbody;
		m_Sphere = vSphereCenter;
		m_Sphere.w = flSphereRadius;
		const CFeModel *pFeModel = pSoftbody->GetFeModel();
		m_pDynPos1 = pSoftbody->m_pPos1 + pFeModel->m_nStaticNodes;
		m_pNodeCollisionRadii = pFeModel->m_pNodeCollisionRadii;
	}

	void Collide( uint16 nCollisionMask );
};


class CTaperedCapsuleColliderFunctor
{
public:
	VectorAligned m_vSphereCenter0;
	VectorAligned m_vSphereCenter1;
	VectorAligned *m_pDynPos1;
	const float *m_pNodeCollisionRadii;
	CSoftbody *m_pSoftbody;
	float m_flStickiness;
	uint16 m_nParentNode[ 2 ];
	float m_flSlope;
	Vector m_vAxisX;
	float m_flDist;
public:
	CTaperedCapsuleColliderFunctor(){}
	CTaperedCapsuleColliderFunctor( CSoftbody *pSoftbody, const Vector &vSphereCenter0, float flSphereRadius0, const Vector &vSphereCenter1, float flSphereRadius1, float flStickiness, uint16 nParentNodes0, uint16 nParentNodes1 )
	{
		m_pSoftbody = pSoftbody;
		m_vSphereCenter0 = vSphereCenter0;
		m_vSphereCenter0.w = flSphereRadius0;
		m_vSphereCenter1 = vSphereCenter1;
		m_vSphereCenter1.w = flSphereRadius1;
		m_flStickiness = flStickiness;
		m_nParentNode[ 0 ] = nParentNodes0;
		m_nParentNode[ 1 ] = nParentNodes1;
		const CFeModel *pFeModel = pSoftbody->GetFeModel();
		m_pDynPos1 = pSoftbody->m_pPos1 + pFeModel->m_nStaticNodes;
		m_pNodeCollisionRadii = pFeModel->m_pNodeCollisionRadii;
		m_vAxisX = ( vSphereCenter1 - vSphereCenter0 ) ;
		m_flDist = m_vAxisX.Length();
		m_vAxisX /= m_flDist;
		m_flSlope = ( flSphereRadius1 - flSphereRadius0 ) / m_flDist;
	}

	void Collide( uint16 nCollisionMask );
};



#endif
