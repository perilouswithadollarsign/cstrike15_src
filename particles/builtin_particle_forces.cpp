//===== Copyright � 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: particle system code
//
//===========================================================================//

#include "tier0/platform.h"
#include "particles/particles.h"
#include "filesystem.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"

#ifdef USE_BLOBULATOR
// TODO: These should be in public by the time the SDK ships
#include "../common/blobulator/physics/physparticle.h"
#include "../common/blobulator/physics/physparticlecache_inl.h"
#include "../common/blobulator/physics/phystiler.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_OP_RandomForce : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_RandomForce );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}


	virtual void AddForces( FourVectors *pAccumulatedForces, 
							CParticleCollection *pParticles,
							int nBlocks,
							float flStrength,
							void *pContext ) const;

	Vector m_MinForce;
	Vector m_MaxForce;
};

void C_OP_RandomForce::AddForces( FourVectors *pAccumulatedForces, 
								  CParticleCollection *pParticles,
								  int nBlocks,
								  float flStrength,					  
								  void *pContext ) const
{
	FourVectors box_min,box_max;
	box_min.DuplicateVector( m_MinForce * flStrength );
	box_max.DuplicateVector( m_MaxForce * flStrength);
	box_max -= box_min;
	int nContext = GetSIMDRandContext();
	for(int i=0;i<nBlocks;i++)
	{
		pAccumulatedForces->x = AddSIMD(
			pAccumulatedForces->x, AddSIMD( box_min.x, MulSIMD( box_max.x, RandSIMD( nContext) ) ) );
		pAccumulatedForces->y = AddSIMD(									   
			pAccumulatedForces->y, AddSIMD( box_min.y, MulSIMD( box_max.y, RandSIMD( nContext) ) ) );
		pAccumulatedForces->z = AddSIMD(									   
			pAccumulatedForces->z, AddSIMD( box_min.z, MulSIMD( box_max.z, RandSIMD( nContext) ) ) );
		pAccumulatedForces++;
	}
	ReleaseSIMDRandContext( nContext );
}

DEFINE_PARTICLE_OPERATOR( C_OP_RandomForce, "random force", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_RandomForce ) 
	DMXELEMENT_UNPACK_FIELD( "min force", "0 0 0", Vector, m_MinForce )
	DMXELEMENT_UNPACK_FIELD( "max force", "0 0 0", Vector, m_MaxForce )
END_PARTICLE_OPERATOR_UNPACK( C_OP_RandomForce )

class C_OP_ParentVortices : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_ParentVortices );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}


	virtual void AddForces( FourVectors *pAccumulatedForces, 
							CParticleCollection *pParticles,
							int nBlocks,
							float flStrength,
							void *pContext ) const;

	float m_flForceScale;
	Vector m_vecTwistAxis;
	bool m_bFlipBasedOnYaw;
};

struct VortexParticle_t
{
	FourVectors m_v4Center;
	FourVectors m_v4TwistAxis;
	fltx4 m_fl4OORadius;
	fltx4 m_fl4Strength;
};

void C_OP_ParentVortices::AddForces( FourVectors *pAccumulatedForces, 
									  CParticleCollection *pParticles,
									  int nBlocks,
									  float flStrength,
									  void *pContext ) const
{
	if ( pParticles->m_pParent && ( pParticles->m_pParent->m_nActiveParticles ) )
	{

		FourVectors Twist_AxisInWorldSpace;
		Twist_AxisInWorldSpace.DuplicateVector( m_vecTwistAxis );
		Twist_AxisInWorldSpace.VectorNormalize();

		int nVortices = pParticles->m_pParent->m_nActiveParticles;
		VortexParticle_t *pVortices = ( VortexParticle_t * ) stackalloc( nVortices * sizeof( VortexParticle_t ) );
		for( int i = 0; i < nVortices; i++ )
		{
			pVortices[i].m_v4TwistAxis = Twist_AxisInWorldSpace;
			pVortices[i].m_fl4OORadius = ReplicateX4( 1.0 / ( 0.00001 +  *( pParticles->m_pParent->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, i ) ) ) );
			pVortices[i].m_fl4OORadius = MulSIMD( pVortices[i].m_fl4OORadius, pVortices[i].m_fl4OORadius );
			pVortices[i].m_fl4Strength = ReplicateX4( m_flForceScale * flStrength * ( * ( pParticles->m_pParent->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_ALPHA, i ) ) ) );
			float const *pXYZ = pParticles->m_pParent->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, i );
			pVortices[i].m_v4Center.x = ReplicateX4( pXYZ[0] );
			pVortices[i].m_v4Center.y = ReplicateX4( pXYZ[4] );
			pVortices[i].m_v4Center.z = ReplicateX4( pXYZ[8] );
			if ( m_bFlipBasedOnYaw )
			{
				float const *pYaw = pParticles->m_pParent->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_YAW, i );
				if ( pYaw[0] >= M_PI )
				{
					pVortices[i].m_v4Center *= Four_NegativeOnes;
				}
			}
		}
		size_t nPosStride;
		const FourVectors *pPos=pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &nPosStride );
		for(int i=0; i < nBlocks ; i++)
		{
			FourVectors v4SumForces;
			v4SumForces.x = Four_Zeros;
			v4SumForces.y = Four_Zeros;
			v4SumForces.z = Four_Zeros;
			for( int v = 0; v < nVortices; v++ )
			{
				FourVectors v4Ofs = *pPos;
				v4Ofs -= pVortices[v].m_v4Center;
				fltx4 v4DistSQ = v4Ofs * v4Ofs;
				bi32x4 bGoodLen = CmpGtSIMD( v4DistSQ, Four_Epsilons );
				v4Ofs.VectorNormalize();
				FourVectors v4Parallel_comp = v4Ofs;
				v4Parallel_comp *= ( v4Ofs * pVortices[v].m_v4TwistAxis );
				v4Ofs -= v4Parallel_comp;
				bGoodLen = AndSIMD( bGoodLen, CmpGtSIMD( v4Ofs * v4Ofs, Four_Epsilons ) );
				v4Ofs.VectorNormalize();
				FourVectors v4TangentialForce = v4Ofs ^ pVortices[v].m_v4TwistAxis;
				fltx4 fl4Strength = pVortices[v].m_fl4Strength;
				fl4Strength = MulSIMD( fl4Strength, MaxSIMD( Four_Zeros, SubSIMD( Four_Ones, MulSIMD( v4DistSQ, pVortices[v].m_fl4OORadius ) ) ) ); // scale so strength = 0.0 at radius or farther
				v4TangentialForce *= fl4Strength;
				v4TangentialForce.x = AndSIMD( v4TangentialForce.x, bGoodLen );
				v4TangentialForce.y = AndSIMD( v4TangentialForce.y, bGoodLen );
				v4TangentialForce.z = AndSIMD( v4TangentialForce.z, bGoodLen );
				v4SumForces += v4TangentialForce;
			}
			*( pAccumulatedForces++ ) += v4SumForces;
			pPos += nPosStride;
		}
	}
}

DEFINE_PARTICLE_OPERATOR( C_OP_ParentVortices, "Create vortices from parent particles", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_ParentVortices ) 
	DMXELEMENT_UNPACK_FIELD( "amount of force", "0", float, m_flForceScale )
	DMXELEMENT_UNPACK_FIELD( "twist axis", "0 0 1", Vector, m_vecTwistAxis )
	DMXELEMENT_UNPACK_FIELD( "flip twist axis with yaw","0", bool, m_bFlipBasedOnYaw )
END_PARTICLE_OPERATOR_UNPACK( C_OP_ParentVortices )

class C_OP_TwistAroundAxis : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_TwistAroundAxis );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}


	virtual void AddForces( FourVectors *pAccumulatedForces, 
							CParticleCollection *pParticles,
							int nBlocks,
							float flStrength,
							void *pContext ) const;

	float m_fForceAmount;
	Vector m_TwistAxis;
	bool m_bLocalSpace;
};

void C_OP_TwistAroundAxis::AddForces( FourVectors *pAccumulatedForces, 
									  CParticleCollection *pParticles,
									  int nBlocks,
									  float flStrength,
									  void *pContext ) const
{
	FourVectors Twist_AxisInWorldSpace;
	Twist_AxisInWorldSpace.DuplicateVector( pParticles->TransformAxis( m_TwistAxis, m_bLocalSpace ) );
	Twist_AxisInWorldSpace.VectorNormalize();

	Vector vecCenter;
	pParticles->GetControlPointAtTime( 0, pParticles->m_flCurTime, &vecCenter );
	FourVectors Center;
	Center.DuplicateVector( vecCenter );
	size_t nPosStride;
	fltx4 ForceScale = ReplicateX4( m_fForceAmount * flStrength );
	const FourVectors *pPos=pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &nPosStride );
	for(int i=0;i<nBlocks;i++)
	{
		FourVectors ofs=*pPos;
		ofs -= Center;
		bi32x4 bGoodLen = CmpGtSIMD( ofs*ofs, Four_Epsilons );
		ofs.VectorNormalize();
		FourVectors parallel_comp=ofs;
		parallel_comp *= ( ofs*Twist_AxisInWorldSpace );
		ofs-=parallel_comp;
		bGoodLen = AndSIMD( bGoodLen, CmpGtSIMD( ofs*ofs, Four_Epsilons ) );
		ofs.VectorNormalize();
		FourVectors TangentialForce = ofs ^ Twist_AxisInWorldSpace;
		TangentialForce *= ForceScale;
		TangentialForce.x = AndSIMD( TangentialForce.x, bGoodLen );
		TangentialForce.y = AndSIMD( TangentialForce.y, bGoodLen );
		TangentialForce.z = AndSIMD( TangentialForce.z, bGoodLen );

		*(pAccumulatedForces++) += TangentialForce;
		pPos += nPosStride;
	}

}

DEFINE_PARTICLE_OPERATOR( C_OP_TwistAroundAxis, "twist around axis", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_TwistAroundAxis ) 
	DMXELEMENT_UNPACK_FIELD( "amount of force", "0", float, m_fForceAmount )
	DMXELEMENT_UNPACK_FIELD( "twist axis", "0 0 1", Vector, m_TwistAxis )
	DMXELEMENT_UNPACK_FIELD( "object local space axis 0/1","0", bool, m_bLocalSpace )
END_PARTICLE_OPERATOR_UNPACK( C_OP_TwistAroundAxis )

class C_OP_AttractToControlPoint : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_AttractToControlPoint );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}


	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}


	virtual void AddForces( FourVectors *pAccumulatedForces, 
							CParticleCollection *pParticles,
							int nBlocks,
							float flStrength,
							void *pContext ) const;

	float m_fForceAmount;
	float m_fFalloffPower;
	int m_nControlPointNumber;
};

void C_OP_AttractToControlPoint::AddForces( FourVectors *pAccumulatedForces, 
											CParticleCollection *pParticles,
											int nBlocks,
											float flStrength,
											void *pContext ) const
{
	int power_frac=-4.0*m_fFalloffPower;					// convert to what pow_fixedpoint_exponent_simd wants
	fltx4 fForceScale=ReplicateX4( -m_fForceAmount * flStrength );

	Vector vecCenter;
	pParticles->GetControlPointAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &vecCenter );
	FourVectors Center;
	Center.DuplicateVector( vecCenter );
	size_t nPosStride;
	const FourVectors *pPos=pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &nPosStride );

	for(int i=0;i<nBlocks;i++)
	{
		FourVectors ofs=*pPos;
		ofs -= Center;
		fltx4 len = ofs.length();
		ofs *= MulSIMD( fForceScale, ReciprocalSaturateSIMD( len )); // normalize and scale
		ofs *= Pow_FixedPoint_Exponent_SIMD( len, power_frac ); // * 1/pow(dist, exponent)
		bi32x4 bGood = CmpGtSIMD( len, Four_Epsilons );
		ofs.x = AndSIMD( bGood, ofs.x );
		ofs.y = AndSIMD( bGood, ofs.y );
		ofs.z = AndSIMD( bGood, ofs.z );
		*(pAccumulatedForces++) += ofs;
		pPos += nPosStride;
	}
	
}

DEFINE_PARTICLE_OPERATOR( C_OP_AttractToControlPoint, "Pull towards control point", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_AttractToControlPoint ) 
	DMXELEMENT_UNPACK_FIELD( "amount of force", "0", float, m_fForceAmount )
	DMXELEMENT_UNPACK_FIELD( "falloff power", "2", float, m_fFalloffPower )
	DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
END_PARTICLE_OPERATOR_UNPACK( C_OP_AttractToControlPoint )

class C_OP_ForceBasedOnDistanceToPlane : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_ForceBasedOnDistanceToPlane );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}


	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}


	virtual void AddForces( FourVectors *pAccumulatedForces, 
							CParticleCollection *pParticles,
							int nBlocks,
							float flStrength,
							void *pContext ) const;

	float m_flMinDist;
	Vector m_vecForceAtMinDist;
	float m_flMaxDist;
	Vector m_vecForceAtMaxDist;

	Vector m_vecPlaneNormal;
	int m_nControlPointNumber;

	float m_flExponent;
};

void C_OP_ForceBasedOnDistanceToPlane::AddForces( FourVectors *pAccumulatedForces, 
												  CParticleCollection *pParticles,
												  int nBlocks,
												  float flStrength,
												  void *pContext ) const
{
	float flDeltaDistances = m_flMaxDist - m_flMinDist;
	fltx4 fl4OORange = Four_Zeros;
	if ( flDeltaDistances )
	{
		fl4OORange = ReplicateX4( 1.0 / flDeltaDistances );
	}
	Vector vecPointOnPlane =  pParticles->GetControlPointAtCurrentTime( m_nControlPointNumber );
	FourVectors v4PointOnPlane;
	v4PointOnPlane.DuplicateVector( vecPointOnPlane );
	FourVectors v4PlaneNormal;
	v4PlaneNormal.DuplicateVector( m_vecPlaneNormal );
	fltx4 fl4MinDist = ReplicateX4( m_flMinDist );

	C4VAttributeIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );

	FourVectors v4Force0;
	v4Force0.DuplicateVector( m_vecForceAtMinDist );
	FourVectors v4ForceDelta;
	v4ForceDelta.DuplicateVector( m_vecForceAtMaxDist - m_vecForceAtMinDist );

	int nPowValue = 4.0 * m_flExponent;
	for(int i=0 ; i < nBlocks ; i++)
	{
		FourVectors v4Ofs = *pXYZ;
		v4Ofs -= v4PointOnPlane;
		fltx4 fl4DistanceFromPlane = v4Ofs * v4PlaneNormal;
		fl4DistanceFromPlane = MulSIMD( SubSIMD( fl4DistanceFromPlane, fl4MinDist ), fl4OORange );
		fl4DistanceFromPlane = MaxSIMD( Four_Zeros, MinSIMD( Four_Ones, fl4DistanceFromPlane ) );
		fl4DistanceFromPlane = Pow_FixedPoint_Exponent_SIMD( fl4DistanceFromPlane, nPowValue );
		// now, calculate lerped force
		FourVectors v4OutputForce = v4ForceDelta;
		v4OutputForce *= fl4DistanceFromPlane;
		v4OutputForce += v4Force0;
		*( pAccumulatedForces++ ) += v4OutputForce;
		++pXYZ;
	}
	
}

DEFINE_PARTICLE_OPERATOR( C_OP_ForceBasedOnDistanceToPlane, "Force based on distance from plane", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_ForceBasedOnDistanceToPlane ) 
    DMXELEMENT_UNPACK_FIELD( "Min distance from plane", "0", float, m_flMinDist )
    DMXELEMENT_UNPACK_FIELD( "Force at Min distance", "0 0 0", Vector, m_vecForceAtMinDist )
    DMXELEMENT_UNPACK_FIELD( "Max Distance from plane", "1", float, m_flMaxDist )
    DMXELEMENT_UNPACK_FIELD( "Force at Max distance", "0 0 0", Vector, m_vecForceAtMaxDist )
    DMXELEMENT_UNPACK_FIELD( "Plane Normal", "0 0 1", Vector, m_vecPlaneNormal )
    DMXELEMENT_UNPACK_FIELD( "Control point number", "0", int, m_nControlPointNumber )
    DMXELEMENT_UNPACK_FIELD( "Exponent", "1", float, m_flExponent )
END_PARTICLE_OPERATOR_UNPACK( C_OP_ForceBasedOnDistanceToPlane )

#undef USE_BLOBULATOR // TODO (Ilya): Must fix this code

#ifdef USE_BLOBULATOR

class C_OP_LennardJonesForce : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_LennardJonesForce );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		//m_pParticleCache = new ParticleCache(m_fInteractionRadius);
		m_pPhysTiler = new PhysTiler(m_fInteractionRadius);
	}

	virtual void AddForces( FourVectors *pAccumulatedForces, 
		CParticleCollection *pParticles,
		int nBlocks,
		float flStrength,
		void *pContext ) const;

	// TODO: Have to destroy PhysTiler in destructor somewhere!!!!

	//ParticleCache* m_pParticleCache;
	PhysTiler* m_pPhysTiler;
	float m_fInteractionRadius;
	float m_fSurfaceTension;
	float m_fLennardJonesRepulsion;
	float m_fLennardJonesAttraction;
	float m_fMaxRepulsion;
	float m_fMaxAttraction;

private:
	//virtual void addParticleForce(PhysParticle* a, PhysParticleCacheNode* bcn, float flStrength, float ts) const;
	virtual void addParticleForce(PhysParticle* a, PhysParticle* b, float distSq, float flStrength, float ts) const;
};




// TODO: I should make sure I don't have divide by zero errors.
// TODO: ts is not used
void C_OP_LennardJonesForce::addParticleForce(PhysParticle* a, PhysParticle* b, float distSq, float flStrength, float ts) const
{
	float d = sqrtf(distSq);

	//========================================================
	// based on equation of force between two molecules which is
	// factor * ((distance/bond_length)^-7 - (distance/bond_length)^-13)

	float f;
	if(a->group == b->group) // In the same group
	{
		float p = a->radius * 2.0f / (d+FLT_EPSILON);
		float p2 = p * p;
		float p4 = p2 * p2;


		// Surface tension:

		//Notes:
		// Can average the neighbor count between the two particles...
		// I tried this, and discovered that rather than averaging, I can take maybe take the
		// larger of the two neighbor counts, so the attraction between two particles on the surface will be strong, but
		// the attraction between a particle inside and a particle on the surface will be weak. I can also try
		// taking the min so that the attraction between a particle on the surface and a particle inside the fluid will
		// be strong, but the attraction between two particles completely on the inside will be weak.
		//
		// int symmetric_neighbor_count = min(a->neighbor_count, b->neighbor_count);
		//
		// Can try having neighbors only cause stronger attraction (no repulsion)
		// Can try lower exponents for the LennardJones forces.

		// This is a trick to prevent single particles from floating off... the less neighbors a particle has.. the more it sticks
		// This also tends to simulate surface tension
		float surface_tension_modifier = ((24.0f * m_fSurfaceTension) / (a->neighbor_count + b->neighbor_count + 0.1f)) + 1.0f;
		//float lennard_jones_force = fLennardJones * 2.0f * (p2 - (p4 * p4));
		float lennard_jones_force = m_fLennardJonesAttraction * p2 - m_fLennardJonesRepulsion*p4;
		f = surface_tension_modifier * lennard_jones_force;

		// This is some older code:
		//f = ((35.0f * LampScene::simulationSurfaceTension) / (a->neighbor_count + 0.1f)) * (p2 - (p4 * p4));
		// used to be 68'


		//float factor = (b->neighbor_count < 13 && neighbor_count < 13 ? 4.0f : 0.5f);
		//f = factor * (p2 - (p2 * p2 * p2 * p2));
	}
	else
	{
		// This was 3.5 ... made 3.0 so particles get closer when they collide
		if(d > a->radius * 3.0f) return;

		float p = a->radius * 4.0f / d;
		f = -1.0f * p * p;
	}

	// These checks are great to have, but are they really necessary?
	// It might also be good to have a limit on velocity

	// Attraction is a positive value.
	// Repulsion is negative.
	if(f < -m_fMaxRepulsion) f = -m_fMaxRepulsion;
	if(f > m_fMaxAttraction) f = m_fMaxAttraction;

	Point3D scaledr = (b->center - a->center) * (f/(d+FLT_EPSILON)); // Dividing by d scales distance down to a unit vector
	a->force.add(scaledr); 
	b->force.subtract(scaledr);
}

void C_OP_LennardJonesForce::AddForces( FourVectors *pAccumulatedForces, 
										CParticleCollection *pParticles,
										int nBlocks,
										float flStrength,					  
										void *pContext ) const
{
	int nParticles = pParticles->m_nActiveParticles; // Not sure if this is correct!

	size_t nPosStride;
	const FourVectors *pPos=pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &nPosStride );
	
	// The +4 is because particles are stored by PET in blocks of 4
	// However, not every block is full. Thus, nParticles may be
	// less than nBlocks*4. Could get rid of this if the swizzling/unswizzling
	// loop were better written.
	static SmartArray<PhysParticle> imp_particles_sa; // This doesn't specify alignment, might have problems with SSE
	while(imp_particles_sa.size < nParticles+4)
	{
		imp_particles_sa.pushAutoSize(PhysParticle());
	}
	
	/*
	size_t nPrevPosStride;
	const FourVectors *pPrevPos=pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, &nPrevPosStride );
	*/
	
	//m_pParticleCache->beginFrame();
	//m_pParticleCache->beginTile(nParticles);

	m_pPhysTiler->beginFrame(Point3D(0.0f, 0.0f, 0.0f));

	// Unswizzle from the FourVectors format into particles
	for(int i=0, p=0;i<nBlocks;i++)
	{
		FourVectors ofs=*pPos;

		PhysParticle* particle = &(imp_particles_sa[p]);
		particle->force.clear();
		if(p < nParticles)
		{
			particle->center = ofs.Vec(0);
			particle->group = 0;
			particle->neighbor_count = 0;
			m_pPhysTiler->insertParticle(particle);
		}
		p++;

		particle = &(imp_particles_sa[p]);
		particle->force.clear();
		if(p < nParticles)
		{
			particle->center = ofs.Vec(1);
			particle->group = 0;
			particle->neighbor_count = 0;
			m_pPhysTiler->insertParticle(particle);
		}
		p++;

		particle = &(imp_particles_sa[p]);
		particle->force.clear();
		if(p < nParticles)
		{
			particle->center = ofs.Vec(2);
			particle->group = 0;
			particle->neighbor_count = 0;
			m_pPhysTiler->insertParticle(particle);
		}
		p++;

		particle = &(imp_particles_sa[p]);
		particle->force.clear();
		if(p < nParticles)
		{
			particle->center = ofs.Vec(3);
			particle->group = 0;
			particle->neighbor_count = 0;
			m_pPhysTiler->insertParticle(particle);
		}
		p++;

		pPos += nPosStride;
	}

	m_pPhysTiler->processTiles();


	float timeStep = 1.0f; // This should be customizable
	float nearNeighborInteractionRadius = 2.3f;
	float nearNeighborInteractionRadiusSq = nearNeighborInteractionRadius * nearNeighborInteractionRadius;
	
	PhysParticleCache* pCache = m_pPhysTiler->getParticleCache();

	// Calculate number of near neighbors for each particle
	for(int i = 0; i < nParticles; i++)
	{
		PhysParticle *b1 = &(imp_particles_sa[i]);

		PhysParticleAndDist* node = pCache->get(b1);

		while(node->particle != NULL)
		{
			PhysParticle* b2 = node->particle;

			 // Compare addresses of the two particles. This makes sure we apply a force only once between a pair of particles.
			if(b1 < b2 && node->distSq < nearNeighborInteractionRadiusSq)
			{
				b1->neighbor_count++;
				b2->neighbor_count++;
	
			}

			node++;
		}
	}

	// Calculate forces on particles due to other particles
	for(int i = 0; i < nParticles; i++)
	{
		PhysParticle *b1 = &(imp_particles_sa[i]);

		PhysParticleAndDist* node = pCache->get(b1);

		while(node->particle != NULL)
		{
			PhysParticle* b2 = node->particle;

			// Compare addresses of the two particles. This makes sure we apply a force only once between a pair of particles.
			if(b1 < b2)
			{
				addParticleForce(b1, b2, node->distSq, flStrength, timeStep);
			}

			node++;
		}
	}


	/*
	for(ParticleListNode* bit3 = particles; bit3; bit3 = bit3->next)
	{
		Particle* b = bit3->particle;
		b->prev_group = b->group; // Set prev group
		//b1->addDirDragForce();
		b->move(ts); // Move the particle (it should never be used again until next iteration)
	}
	*/

	m_pPhysTiler->endFrame();

	// Swizzle forces back into FourVectors format
	for(int i=0;i<nBlocks;i++)
	{
		pAccumulatedForces->X(0) += imp_particles_sa[i*4].force[0];
		pAccumulatedForces->Y(0) += imp_particles_sa[i*4].force[1];
		pAccumulatedForces->Z(0) += imp_particles_sa[i*4].force[2];

		pAccumulatedForces->X(1) += imp_particles_sa[i*4+1].force[0];
		pAccumulatedForces->Y(1) += imp_particles_sa[i*4+1].force[1];		
		pAccumulatedForces->Z(1) += imp_particles_sa[i*4+1].force[2];

		pAccumulatedForces->X(2) += imp_particles_sa[i*4+2].force[0];
		pAccumulatedForces->Y(2) += imp_particles_sa[i*4+2].force[1];
		pAccumulatedForces->Z(2) += imp_particles_sa[i*4+2].force[2];

		pAccumulatedForces->X(3) += imp_particles_sa[i*4+3].force[0];
		pAccumulatedForces->Y(3) += imp_particles_sa[i*4+3].force[1];
		pAccumulatedForces->Z(3) += imp_particles_sa[i*4+3].force[2];

		pAccumulatedForces++;
	}

}

DEFINE_PARTICLE_OPERATOR( C_OP_LennardJonesForce, "lennard jones force", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_LennardJonesForce ) 
DMXELEMENT_UNPACK_FIELD( "interaction radius", "4", float, m_fInteractionRadius )
DMXELEMENT_UNPACK_FIELD( "surface tension", "1", float, m_fSurfaceTension )
DMXELEMENT_UNPACK_FIELD( "lennard jones attractive force", "1", float, m_fLennardJonesAttraction )
DMXELEMENT_UNPACK_FIELD( "lennard jones repulsive force", "1", float, m_fLennardJonesRepulsion )
DMXELEMENT_UNPACK_FIELD( "max repulsion", "100", float, m_fMaxRepulsion )
DMXELEMENT_UNPACK_FIELD( "max attraction", "100", float, m_fMaxAttraction )
END_PARTICLE_OPERATOR_UNPACK( C_OP_LennardJonesForce )

#endif

class C_OP_TimeVaryingForce : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_TimeVaryingForce );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}


	virtual void AddForces( FourVectors *pAccumulatedForces, 
							CParticleCollection *pParticles,
							int nBlocks,
							float flStrength,
							void *pContext ) const;

	float m_flStartLerpTime;
	Vector m_StartingForce;
	float m_flEndLerpTime;
	Vector m_EndingForce;
};

void C_OP_TimeVaryingForce::AddForces( FourVectors *pAccumulatedForces, 
								  CParticleCollection *pParticles,
								  int nBlocks,
								  float flStrength,					  
								  void *pContext ) const
{
	FourVectors box_min,box_max;
	box_min.DuplicateVector( m_StartingForce * flStrength );
	box_max.DuplicateVector( m_EndingForce * flStrength);
	box_max -= box_min;
	CM128AttributeIterator pCreationTime( PARTICLE_ATTRIBUTE_CREATION_TIME, pParticles );
	fltx4 fl4StartTime = ReplicateX4( m_flStartLerpTime );
	fltx4 fl4OODuration = ReplicateX4( 1.0 / ( m_flEndLerpTime - m_flStartLerpTime ) );
	fltx4 fl4CurTime = pParticles->m_fl4CurTime;
	for(int i=0;i<nBlocks;i++)
	{
		fltx4 fl4Age = SubSIMD( fl4CurTime, *pCreationTime );
		fl4Age = MulSIMD( fl4OODuration, SubSIMD( fl4Age, fl4StartTime ) );
		fl4Age = MaxSIMD( Four_Zeros, MinSIMD( Four_Ones, fl4Age ) );
		FourVectors v4Force = box_max;
		v4Force *= fl4Age;
		v4Force += box_min;
		(*pAccumulatedForces) += v4Force;
		++pAccumulatedForces;
		++pCreationTime;
	}
}

DEFINE_PARTICLE_OPERATOR( C_OP_TimeVaryingForce, "time varying force", OPERATOR_GENERIC );

BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_TimeVaryingForce ) 
    DMXELEMENT_UNPACK_FIELD( "time to start transition", "0", float, m_flStartLerpTime )
	DMXELEMENT_UNPACK_FIELD( "starting force", "0 0 0", Vector, m_StartingForce )
    DMXELEMENT_UNPACK_FIELD( "time to end transition", "10", float, m_flEndLerpTime )
	DMXELEMENT_UNPACK_FIELD( "ending force", "0 0 0", Vector, m_EndingForce )
END_PARTICLE_OPERATOR_UNPACK( C_OP_TimeVaryingForce )


class C_OP_TurbulenceForce : public CParticleOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_OP_TurbulenceForce );

	uint32 GetWrittenAttributes( void ) const
	{
		return 0;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}


	virtual void AddForces( FourVectors *pAccumulatedForces, 
							CParticleCollection *pParticles,
							int nBlocks,
							float flStrength,
							void *pContext ) const;

	float m_flNoiseCoordScale[4];
	Vector m_vecNoiseAmount[4];


};

void C_OP_TurbulenceForce::AddForces( FourVectors *pAccumulatedForces, 
								  CParticleCollection *pParticles,
								  int nBlocks,
								  float flStrength,					  
								  void *pContext ) const
{
	C4VAttributeIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
	fltx4 fl4Scales[4];
	FourVectors v4Amounts[4];
	for( int i = 0; i < ARRAYSIZE( fl4Scales ); i++ )
	{
		fl4Scales[i] = ReplicateX4( m_flNoiseCoordScale[i] );
		v4Amounts[i].DuplicateVector( m_vecNoiseAmount[i] );
	}
	for(int i=0;i<nBlocks;i++)
	{
		for( int j = 0; j < ARRAYSIZE( fl4Scales ); j++ )
		{
			FourVectors ppos = *pXYZ;
			ppos *= fl4Scales[j];
			ppos = DNoiseSIMD( ppos );
			ppos *= v4Amounts[j];
			(*pAccumulatedForces) += ppos;
		}
		++pAccumulatedForces;
		++pXYZ;
	}
}

DEFINE_PARTICLE_OPERATOR( C_OP_TurbulenceForce, "turbulent force", OPERATOR_GENERIC );
BEGIN_PARTICLE_OPERATOR_UNPACK( C_OP_TurbulenceForce ) 
 DMXELEMENT_UNPACK_FIELD( "Noise scale 0", "1", float, m_flNoiseCoordScale[0] )
 DMXELEMENT_UNPACK_FIELD( "Noise amount 0", "1 1 1", Vector, m_vecNoiseAmount[0] )
 DMXELEMENT_UNPACK_FIELD( "Noise scale 1", "0", float, m_flNoiseCoordScale[1] )
 DMXELEMENT_UNPACK_FIELD( "Noise amount 1", ".5 .5 .5", Vector, m_vecNoiseAmount[1] )
 DMXELEMENT_UNPACK_FIELD( "Noise scale 2", "0", float, m_flNoiseCoordScale[2] )
 DMXELEMENT_UNPACK_FIELD( "Noise amount 2", ".25 .25 .25", Vector, m_vecNoiseAmount[2] )
 DMXELEMENT_UNPACK_FIELD( "Noise scale 3", "0", float, m_flNoiseCoordScale[3] )
 DMXELEMENT_UNPACK_FIELD( "Noise amount 3", ".125 .125 .125", Vector, m_vecNoiseAmount[3] )
END_PARTICLE_OPERATOR_UNPACK( C_OP_TurbulenceForce )



void AddBuiltInParticleForceGenerators( void )
{
	REGISTER_PARTICLE_OPERATOR( FUNCTION_FORCEGENERATOR, C_OP_RandomForce );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_FORCEGENERATOR, C_OP_TwistAroundAxis );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_FORCEGENERATOR, C_OP_ParentVortices );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_FORCEGENERATOR, C_OP_AttractToControlPoint );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_FORCEGENERATOR, C_OP_TimeVaryingForce );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_FORCEGENERATOR, C_OP_TurbulenceForce );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_FORCEGENERATOR, C_OP_ForceBasedOnDistanceToPlane );
#ifdef USE_BLOBULATOR
	REGISTER_PARTICLE_OPERATOR( FUNCTION_FORCEGENERATOR, C_OP_LennardJonesForce );
#endif
}

