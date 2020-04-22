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
#include "tier2/renderutils.h"
#include "tier1/UtlStringMap.h"
#include "tier1/strtools.h"
#include "dmxloader/dmxelement.h"
#include "bitmap/psheet.h"
#include "bspflags.h"
#include "const.h"
#include "particles_internal.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


void CParticleOperatorInstance::InitScalarAttributeRandomRangeExpScalar( 
	int nAttrNum, float flMin, float flMax, float flExp,
	CParticleCollection *pParticles, int nStartParticle, int nParticleCount ) const
{
	if ( ! m_bStrengthFastPath )
	{
		// adjust max/mins based on strength
		float flStrength;
		pParticles->CheckIfOperatorShouldRun( this, &flStrength );
		flMin *= flStrength;
		flMax *= flStrength;
	}

	if ( flExp != 1.0 )
	{
		for( ; nParticleCount--; nStartParticle++ )
		{
			float *pAttr = pParticles->GetFloatAttributePtrForWrite( nAttrNum, nStartParticle );
			*pAttr = pParticles->RandomFloatExp( flMin, flMax, flExp );
		}
	}
	else
	{
		for( ; nParticleCount--; nStartParticle++ )
		{
			float *pAttr = pParticles->GetFloatAttributePtrForWrite( nAttrNum, nStartParticle );
			*pAttr = pParticles->RandomFloat( flMin, flMax );
		}
	}
}

void CParticleOperatorInstance::InitScalarAttributeRandomRangeExpBlock( 
	int attr_num, float fMin, float fMax, float fExp,
	CParticleCollection *pParticles, int nStartBlock, int nBlockCount, bool bRandomlyInvert ) const
{
	if ( ! m_bStrengthFastPath )
	{
		// adjust max/mins based on strength
		float flStrength;
		pParticles->CheckIfOperatorShouldRun( this, &flStrength );
		fMin *= flStrength;
		fMax *= flStrength;
	}

	size_t nAttrStride;
	fltx4 *pAttr = pParticles->GetM128AttributePtrForWrite( attr_num, &nAttrStride );
	pAttr += nAttrStride * nStartBlock;
	fltx4 val0 = ReplicateX4( fMin );
	fltx4 val_d = ReplicateX4( fMax - fMin );
	int nRandContext = GetSIMDRandContext();
	if ( !bRandomlyInvert )
	{
		if ( fExp == 1.0 )
		{
			while( nBlockCount-- )
			{
				*( pAttr ) = AddSIMD( val0, MulSIMD( RandSIMD( nRandContext ), val_d ) );
				pAttr += nAttrStride;
			}
		}
		else
		{
			while( nBlockCount-- )
			{
				*( pAttr ) = AddSIMD( val0, MulSIMD( Pow_FixedPoint_Exponent_SIMD( RandSIMD( nRandContext ), fExp ), val_d ) );
				pAttr += nAttrStride;
			}
		}
	}
	else
	{
		fltx4 fl4NegOne = ReplicateX4( -1.0f );
		if ( fExp == 1.0f )
		{
			while( nBlockCount-- )
			{
				fltx4 fl4RandVal = AddSIMD( val0, MulSIMD( RandSIMD( nRandContext ), val_d ) );
				fltx4 fl4Sign = MaskedAssign( CmpGeSIMD( RandSIMD( nRandContext ), Four_PointFives ), Four_Ones, fl4NegOne ); 
				*pAttr = MulSIMD( fl4RandVal, fl4Sign );
				pAttr += nAttrStride;
			}
		}
		else
		{
			while( nBlockCount-- )
			{
				fltx4 fl4RandVal = AddSIMD( val0, MulSIMD( Pow_FixedPoint_Exponent_SIMD( RandSIMD( nRandContext ), fExp ), val_d ) );
				fltx4 fl4Sign = MaskedAssign( CmpGeSIMD( RandSIMD( nRandContext ), Four_PointFives ), Four_Ones, fl4NegOne ); 
				*pAttr = MulSIMD( fl4RandVal, fl4Sign );
				pAttr += nAttrStride;
			}
		}
	}
	ReleaseSIMDRandContext( nRandContext );
}

void CParticleOperatorInstance::AddScalarAttributeRandomRangeExpBlock( 
	int nAttributeId, float fMin, float fMax, float fExp,
	CParticleCollection *pParticles, int nStartBlock, int nBlockCount, bool bRandomlyInvert ) const
{
	size_t nAttrStride;
	fltx4 *pAttr = pParticles->GetM128AttributePtrForWrite( nAttributeId, &nAttrStride );
	pAttr += nAttrStride * nStartBlock;
	fltx4 val0 = ReplicateX4( fMin );
	fltx4 val_d = ReplicateX4( fMax - fMin );
	int nRandContext = GetSIMDRandContext();
	if ( !bRandomlyInvert )
	{
		if ( fExp == 1.0f )
		{
			while( nBlockCount-- )
			{
				*pAttr = AddSIMD( *pAttr, AddSIMD( val0, MulSIMD( RandSIMD( nRandContext ), val_d ) ) );
				pAttr += nAttrStride;
			}
		}
		else
		{
			while( nBlockCount-- )
			{
				*( pAttr ) = AddSIMD( *pAttr, AddSIMD( val0, MulSIMD( Pow_FixedPoint_Exponent_SIMD( RandSIMD( nRandContext ), fExp ), val_d ) ) );
				pAttr += nAttrStride;
			}
		}
	}
	else
	{
		fltx4 fl4NegOne = ReplicateX4( -1.0f );
		if ( fExp == 1.0f )
		{
			while( nBlockCount-- )
			{
				fltx4 fl4RandVal = AddSIMD( val0, MulSIMD( RandSIMD( nRandContext ), val_d ) );
				fltx4 fl4Sign = MaskedAssign( CmpGeSIMD( RandSIMD( nRandContext ), Four_PointFives ), Four_Ones, fl4NegOne ); 
				*pAttr = AddSIMD( *pAttr, MulSIMD( fl4RandVal, fl4Sign ) );
				pAttr += nAttrStride;
			}
		}
		else
		{
			while( nBlockCount-- )
			{
				fltx4 fl4RandVal = AddSIMD( val0, MulSIMD( Pow_FixedPoint_Exponent_SIMD( RandSIMD( nRandContext ), fExp ), val_d ) );
				fltx4 fl4Sign = MaskedAssign( CmpGeSIMD( RandSIMD( nRandContext ), Four_PointFives ), Four_Ones, fl4NegOne ); 
				*pAttr = AddSIMD( *pAttr, MulSIMD( fl4RandVal, fl4Sign ) );
				pAttr += nAttrStride;
			}
		}
	}
	ReleaseSIMDRandContext( nRandContext );
}

class C_INIT_RingWave : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RingWave );

	int m_nControlPointNumber;
	int m_nOverrideCP;
	int m_nOverrideCP2;
	float m_flParticlesPerOrbit;
	float m_flInitialRadius;
	float m_flThickness;
	float m_flInitialSpeedMin;
	float m_flInitialSpeedMax;
	float m_flRoll;
	float m_flPitch;
	float m_flYaw;
	bool  m_bEvenDistribution;
	bool  m_bXYVelocityOnly;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = ( 1ULL << m_nControlPointNumber );
		if ( m_nOverrideCP != -1 )
		{
			nMask |= ( 1ULL << m_nOverrideCP );
		}
		if ( m_nOverrideCP2 != -1 )
		{
			nMask |= ( 1ULL << m_nOverrideCP2 );
		}
		return nMask;
	}

	struct RingWaveContext_t
	{
		unsigned int m_nCurrentCount;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( RingWaveContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		RingWaveContext_t *pCtx=reinterpret_cast<RingWaveContext_t *>( pContext );
		pCtx->m_nCurrentCount = 0;
	}


	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
		m_nOverrideCP = MAX( -1, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nOverrideCP ) );
		m_nOverrideCP2 = MAX( -1, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nOverrideCP2 ) );
		m_flInitialSpeedMax = MAX( m_flInitialSpeedMin, m_flInitialSpeedMax );
		m_flInitialRadius = MAX(0, m_flInitialRadius);
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR (C_INIT_RingWave, "Position Along Ring", OPERATOR_PI_POSITION);

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RingWave )
	DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "initial radius", "0", float, m_flInitialRadius )
	DMXELEMENT_UNPACK_FIELD( "thickness", "0", float, m_flThickness)
	DMXELEMENT_UNPACK_FIELD( "min initial speed", "0", float, m_flInitialSpeedMin)
	DMXELEMENT_UNPACK_FIELD( "max initial speed", "0", float, m_flInitialSpeedMax)
	DMXELEMENT_UNPACK_FIELD( "yaw", "0", float, m_flYaw)
	DMXELEMENT_UNPACK_FIELD( "roll", "0", float, m_flRoll)
	DMXELEMENT_UNPACK_FIELD( "pitch", "0", float, m_flPitch)
	DMXELEMENT_UNPACK_FIELD( "even distribution", "0", bool, m_bEvenDistribution)
	DMXELEMENT_UNPACK_FIELD( "even distribution count", "-1", float, m_flParticlesPerOrbit)
	DMXELEMENT_UNPACK_FIELD( "XY velocity only", "1", bool, m_bXYVelocityOnly)
	DMXELEMENT_UNPACK_FIELD( "Override CP (X/Y/Z *= Radius/Thickness/Speed)", "-1", int, m_nOverrideCP )
	DMXELEMENT_UNPACK_FIELD( "Override CP 2 (X/Y/Z *= Pitch/Yaw/Roll)", "-1", int, m_nOverrideCP2 )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RingWave )

void C_INIT_RingWave::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask,
	void *pContext) const
{
	RingWaveContext_t *pCtx=reinterpret_cast<RingWaveContext_t *>( pContext );
	if ( pCtx->m_nCurrentCount >= ( UINT_MAX - nParticleCount ) )
		pCtx->m_nCurrentCount = 0;

	float flRadius = m_flInitialRadius;
	float flThickness = m_flThickness;
	float flSpeedMin = m_flInitialSpeedMin;
	float flSpeedMax = m_flInitialSpeedMax;
	QAngle qatAngles( m_flPitch, m_flYaw, m_flRoll );

	if ( m_nOverrideCP != -1 )
	{
		Vector vecOverride = pParticles->GetControlPointAtCurrentTime( m_nOverrideCP );
		flRadius = vecOverride.x * m_flInitialRadius;
		flThickness = vecOverride.y * m_flThickness;
		flSpeedMin = vecOverride.z * m_flInitialSpeedMin;
		flSpeedMax = vecOverride.z * m_flInitialSpeedMax;
	}
	if ( m_nOverrideCP2 != -1 )
	{
		Vector vecOverride = pParticles->GetControlPointAtCurrentTime( m_nOverrideCP2 );
		qatAngles.x *= vecOverride.x;
		qatAngles.y *= vecOverride.y;
		qatAngles.z *= vecOverride.z;
	}

	float theta = 2 * M_PI / ( ( m_bEvenDistribution && m_flParticlesPerOrbit != -1 ) ? m_flParticlesPerOrbit : (float) nParticleCount );

	for( ; nParticleCount-- ; start_p++ )
	{
		pCtx->m_nCurrentCount++;
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		float sinTheta = 0, cosTheta = 0;
		float speed;
		Vector pointOnCircle;
		Vector randPos;
		Vector prevPos;
		Vector vecControlPoint;
		Vector vecTransformLocal;
		matrix3x4_t mat;

		pParticles->RandomVectorInUnitSphere( &randPos );
		pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *ct, &mat );
		MatrixGetColumn( mat, 3, vecControlPoint );
		SinCos( ( ( m_bEvenDistribution ) ? theta : pParticles->RandomFloat(0, 2*M_PI) )*( pCtx->m_nCurrentCount ), &sinTheta, &cosTheta );
		VectorRotate( ( Vector( cosTheta, sinTheta, 0 ) )*flRadius, qatAngles, pointOnCircle );
		speed = pParticles->RandomFloat( flSpeedMin, flSpeedMax );
		randPos = pointOnCircle + randPos*flThickness;

		VectorTransform( randPos, mat, vecTransformLocal );
		randPos = vecTransformLocal;
		prevPos = ( vecTransformLocal - vecControlPoint ).Normalized();
		prevPos *= speed * ( pParticles->m_flPreviousDt );
		prevPos = randPos - prevPos;
		xyz[0] = randPos.x;
		xyz[4] = randPos.y;
		xyz[8] = randPos.z;
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			pxyz[0] = prevPos.x;
			pxyz[4] = prevPos.y;
			pxyz[8] = (m_bXYVelocityOnly) ? randPos.z : prevPos.z;
		}
	}
}





class C_INIT_CreateInEpitrochoid : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateInEpitrochoid );

	int m_nComponent1;
	int m_nComponent2;
	int m_nControlPointNumber;
	int m_nScaleCP;
	float m_flParticleDensity;
	float m_flOffset;
	float m_flRadius1;
	float m_flRadius2;
	bool m_bUseCount;
	bool m_bUseLocalCoords;
	bool m_bOffsetExistingPos;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber | 1ULL << m_nScaleCP;
	}

	virtual uint64 GetNonPositionalControlPointMask() const
	{
		return 1ULL << m_nScaleCP;
	}

	struct CreateInEpitrochoidContext_t
	{
		unsigned int m_nCurrentCount;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( CreateInEpitrochoidContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		CreateInEpitrochoidContext_t *pCtx=reinterpret_cast<CreateInEpitrochoidContext_t *>( pContext );
		pCtx->m_nCurrentCount = 0;
	}

	bool InitMultipleOverride ( void ) 
	{
		if ( m_bOffsetExistingPos )
			return true; 
		else
			return false;
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
		m_nScaleCP = MAX( -1, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nScaleCP ) );
		m_flParticleDensity = MAX( FLT_EPSILON, MIN ( FLT_MAX, m_flParticleDensity ) );
		m_nComponent1 = MAX( -1, MIN ( 2, m_nComponent1 ) );
		m_nComponent2 = MAX( -1, MIN ( 2, m_nComponent2 ) );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR (C_INIT_CreateInEpitrochoid, "Position Along Epitrochoid", OPERATOR_GENERIC );
BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreateInEpitrochoid )
DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "first dimension 0-2 (-1 disables)", "0", int, m_nComponent1 )
DMXELEMENT_UNPACK_FIELD( "second dimension 0-2 (-1 disables)", "1", int, m_nComponent2 )
DMXELEMENT_UNPACK_FIELD( "radius 1", "40", float, m_flRadius1 )
DMXELEMENT_UNPACK_FIELD( "radius 2", "24", float, m_flRadius2)
DMXELEMENT_UNPACK_FIELD( "point offset", "4", float, m_flOffset)
DMXELEMENT_UNPACK_FIELD( "particle density", "10", float, m_flParticleDensity)
DMXELEMENT_UNPACK_FIELD( "use particle count instead of creation time", "0", bool, m_bUseCount )
DMXELEMENT_UNPACK_FIELD( "local space", "0", bool, m_bUseLocalCoords )
DMXELEMENT_UNPACK_FIELD( "offset from existing position", "0", bool, m_bOffsetExistingPos )
DMXELEMENT_UNPACK_FIELD( "scale from conrol point (radius 1/radius 2/offset)", "-1", int, m_nScaleCP )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateInEpitrochoid )

void C_INIT_CreateInEpitrochoid::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask,
	void *pContext) const
{
	CreateInEpitrochoidContext_t *pCtx=reinterpret_cast<CreateInEpitrochoidContext_t *>( pContext );

	Vector vecCPPos = pParticles->GetControlPointAtCurrentTime( m_nControlPointNumber );
	matrix3x4_t Mat;
	pParticles->GetControlPointTransformAtCurrentTime( m_nControlPointNumber, &Mat );

	// We'll precompute a few bits rather than per particle
	float flRadiiAdd;
	float flOffset;
	float flRadius2;

	if ( m_nScaleCP > -1 )	// Do we have a CP to scale from?
	{
		Vector vecScaleCP = pParticles->GetControlPointAtCurrentTime( m_nScaleCP );
		flRadius2 = m_flRadius2 * vecScaleCP.y + FLT_EPSILON;;
		flRadiiAdd = ( m_flRadius1 * vecScaleCP.x ) + flRadius2;
		flOffset = m_flOffset * vecScaleCP.z;
	}
	else	// if not, just use set values
	{
		flRadiiAdd = m_flRadius1 + m_flRadius2;
		flOffset = m_flOffset;
		flRadius2 = m_flRadius2 + FLT_EPSILON;
	}
	if ( flRadiiAdd != 0 && flRadius2 != 0 )
	{
		for( ; nParticleCount-- ; start_p++ )
		{
			const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

			float flTime;

			if ( m_bUseCount )
			{
				flTime = pCtx->m_nCurrentCount * m_flParticleDensity;
				pCtx->m_nCurrentCount++;
			}
			else
				flTime = *ct * m_flParticleDensity;

			Vector vecXYZ;
			Vector vecPXYZ;
			vecXYZ = vecPXYZ = vecCPPos;
			if ( m_bOffsetExistingPos )
			{
				SetVectorFromAttribute( vecXYZ, xyz );
				SetVectorFromAttribute( vecPXYZ, pxyz );
			}

			Vector vecEpitrochoid = vec3_origin;

			float flMultiplier = ( flRadiiAdd ) * ( flTime / flRadius2 );
			if ( m_nComponent1 != -1 )
				vecEpitrochoid[m_nComponent1] = ( flRadiiAdd ) * cos( flTime ) + flOffset * cos( flMultiplier );
			if ( m_nComponent2 != -1 )
				vecEpitrochoid[m_nComponent2] = ( flRadiiAdd ) * sin( flTime ) + flOffset * sin( flMultiplier );

			if ( m_bUseLocalCoords )
			{
				Vector vecWorld = vecEpitrochoid;
				VectorRotate( vecWorld, Mat, vecEpitrochoid );
			}

			vecXYZ += vecEpitrochoid;
			vecPXYZ += vecEpitrochoid;

			SetVectorAttribute( xyz, vecXYZ );
			SetVectorAttribute( pxyz, vecPXYZ );
		}
	}
}


class C_INIT_CreateOnModel : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateOnModel );

	int m_nControlPointNumber;
	int m_nForceInModel;
	int m_nDesiredHitbox;
	float m_flHitBoxScale;
	Vector m_vecDirectionBias;
	char m_HitboxSetName[128];

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | 
			PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK | PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;

	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateOnModel, "Position on Model Random", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreateOnModel ) 
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "force to be inside model", "0", int, m_nForceInModel )
	DMXELEMENT_UNPACK_FIELD( "model hitbox scale", "1.0", float, m_flHitBoxScale )
	DMXELEMENT_UNPACK_FIELD( "direction bias", "0 0 0", Vector, m_vecDirectionBias )
	DMXELEMENT_UNPACK_FIELD( "desired hitbox", "-1", int, m_nDesiredHitbox )
	DMXELEMENT_UNPACK_FIELD_STRING( "hitbox set", "effects", m_HitboxSetName )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateOnModel )

void C_INIT_CreateOnModel::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	pParticles->UpdateHitBoxInfo( m_nControlPointNumber, m_HitboxSetName );

	Vector vecCP = pParticles->GetControlPointAtCurrentTime( m_nControlPointNumber );
	Vector vecCPPrev;
	pParticles->GetControlPointAtPrevTime( m_nControlPointNumber, &vecCPPrev );
	Vector vecDelta = vecCPPrev - vecCP;
	float flPrevTime = pParticles->m_flCurTime - pParticles->m_flDt;

	while( nParticleCount )
	{
		Vector vecPnts[100];								// minimize stack usage
		Vector vecUVW[100];
		int nHitBoxIndex[100];
		int nToDo = MIN( ARRAYSIZE( vecPnts ), nParticleCount );

		Assert( m_nControlPointNumber <= pParticles->GetHighestControlPoint() );

		g_pParticleSystemMgr->Query()->GetRandomPointsOnControllingObjectHitBox( 
			pParticles, m_nControlPointNumber,
			nToDo, m_flHitBoxScale, m_nForceInModel, vecPnts, m_vecDirectionBias, vecUVW, 
			nHitBoxIndex, m_nDesiredHitbox, m_HitboxSetName );
		
		for( int i=0; i<nToDo; i++)
		{
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			float *pHitboxRelXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ, start_p );
			float *pCt = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
			int *pHitboxIndex = pParticles->GetIntAttributePtrForWrite( PARTICLE_ATTRIBUTE_HITBOX_INDEX, start_p );
			start_p++;

			Vector randpos = vecPnts[i];
			float flPerc = RemapValClamped( *pCt, flPrevTime, pParticles->m_flCurTime, 0.0f, 1.0f );
			VectorLerp( randpos - vecDelta, randpos, flPerc );
			xyz[0] = randpos.x;
			xyz[4] = randpos.y;
			xyz[8] = randpos.z;
			if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
			{
				pxyz[0] = randpos.x;
				pxyz[4] = randpos.y;
				pxyz[8] = randpos.z;
			}
			if ( pHitboxRelXYZ && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK ) )
			{
				pHitboxRelXYZ[0] = vecUVW[i].x;
				pHitboxRelXYZ[4] = vecUVW[i].y;
				pHitboxRelXYZ[8] = vecUVW[i].z;
			}
			if ( pHitboxIndex && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK ) )
			{
				*pHitboxIndex = nHitBoxIndex[i];
			}

		}
		nParticleCount -= nToDo;
	}
}



class C_INIT_SetHitboxToClosest : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_SetHitboxToClosest );

	int m_nControlPointNumber;
	int m_nForceInModel;
	int m_nDesiredHitbox;
	float m_flHitBoxScale;
	char m_HitboxSetName[128];

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK | PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}


	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_SetHitboxToClosest, "Set Hitbox to Closest Hitbox", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_SetHitboxToClosest ) 
DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "model hitbox scale", "1.0", float, m_flHitBoxScale )
DMXELEMENT_UNPACK_FIELD( "desired hitbox", "-1", int, m_nDesiredHitbox )
DMXELEMENT_UNPACK_FIELD_STRING( "hitbox set", "effects", m_HitboxSetName )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_SetHitboxToClosest )

void C_INIT_SetHitboxToClosest::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	pParticles->UpdateHitBoxInfo( m_nControlPointNumber, m_HitboxSetName );

	while( nParticleCount )
	{
		Vector vecPnts[100];								// minimize stack usage
		Vector vecUVW[100];
		int nHitBoxIndex[100];
		int nToDo = MIN( ARRAYSIZE( vecPnts ), nParticleCount );

		for( int i=0; i<nToDo; i++)
		{
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p + i );	
			SetVectorFromAttribute( vecPnts[i], xyz );
		}

		Assert( m_nControlPointNumber <= pParticles->GetHighestControlPoint() );

		g_pParticleSystemMgr->Query()->GetClosestControllingObjectHitBox( 
			pParticles, m_nControlPointNumber,
			nToDo, m_flHitBoxScale, vecPnts, vecUVW, 
			nHitBoxIndex, m_nDesiredHitbox, m_HitboxSetName );

		for( int i=0; i<nToDo; i++)
		{
			float *pHitboxRelXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ, start_p );
			int *pHitboxIndex = pParticles->GetIntAttributePtrForWrite( PARTICLE_ATTRIBUTE_HITBOX_INDEX, start_p );
			start_p++;

			if ( pHitboxRelXYZ && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK ) )
			{
				pHitboxRelXYZ[0] = vecUVW[i].x;
				pHitboxRelXYZ[4] = vecUVW[i].y;
				pHitboxRelXYZ[8] = vecUVW[i].z;
			}
			if ( pHitboxIndex && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK ) )
			{
				*pHitboxIndex = nHitBoxIndex[i];
			}

		}
		nParticleCount -= nToDo;
	}
}



class C_INIT_SetHitboxToModel : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_SetHitboxToModel );

	int m_nControlPointNumber;
	int m_nForceInModel;
	int m_nDesiredHitbox;
	float m_flHitBoxScale;
	Vector m_vecDirectionBias;
	bool m_bMaintainHitbox;
	char m_HitboxSetName[128];


	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK | PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK;

	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_SetHitboxToModel, "Set Hitbox Position on Model", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_SetHitboxToModel ) 
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "force to be inside model", "0", int, m_nForceInModel )
	DMXELEMENT_UNPACK_FIELD( "model hitbox scale", "1.0", float, m_flHitBoxScale )
	DMXELEMENT_UNPACK_FIELD( "direction bias", "0 0 0", Vector, m_vecDirectionBias )
	DMXELEMENT_UNPACK_FIELD( "desired hitbox", "-1", int, m_nDesiredHitbox )
	DMXELEMENT_UNPACK_FIELD_STRING( "hitbox set", "effects", m_HitboxSetName )
	DMXELEMENT_UNPACK_FIELD( "maintain existing hitbox", "0", bool, m_bMaintainHitbox )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_SetHitboxToModel )

void C_INIT_SetHitboxToModel::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	pParticles->UpdateHitBoxInfo( m_nControlPointNumber, m_HitboxSetName );
	while( nParticleCount )
	{
		Vector vecPnts[100];								// minimize stack usage
		Vector vecUVW[100];
		int nHitBoxIndex[100];
		int nToDo = MIN( ARRAYSIZE( vecPnts ), nParticleCount );
		int nDesiredHitbox = m_nDesiredHitbox;
		
		Assert( m_nControlPointNumber <= pParticles->GetHighestControlPoint() );

		if ( m_bMaintainHitbox )
		{
			nDesiredHitbox = *pParticles->GetIntAttributePtr( PARTICLE_ATTRIBUTE_HITBOX_INDEX, start_p );
		}

		g_pParticleSystemMgr->Query()->GetRandomPointsOnControllingObjectHitBox( 
			pParticles, m_nControlPointNumber,
			nToDo, m_flHitBoxScale, m_nForceInModel, vecPnts, m_vecDirectionBias, vecUVW, 
			nHitBoxIndex, nDesiredHitbox, m_HitboxSetName );

		for( int i=0; i<nToDo; i++)
		{
			float *pHitboxRelXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ, start_p );
			int *pHitboxIndex = pParticles->GetIntAttributePtrForWrite( PARTICLE_ATTRIBUTE_HITBOX_INDEX, start_p );
			start_p++;

			Vector randpos = vecPnts[i];
			if ( pHitboxRelXYZ && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK ) )
			{
				pHitboxRelXYZ[0] = vecUVW[i].x;
				pHitboxRelXYZ[4] = vecUVW[i].y;
				pHitboxRelXYZ[8] = vecUVW[i].z;
			}
			if ( pHitboxIndex && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_HITBOX_INDEX_MASK ) )
			{
				*pHitboxIndex = nHitBoxIndex[i];
			}

		}
		nParticleCount -= nToDo;
	}
}



static inline void RandomPointOnUnitSphere( int nRandContext, FourVectors &out )
{
	// generate 4 random points on the unit sphere. uses Marsaglia (1972) method from
	// http://mathworld.wolfram.com/SpherePointPicking.html

	fltx4 f4x1 = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	fltx4 f4x2 = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	fltx4 f4x1SQ = MulSIMD( f4x1, f4x1 );
	fltx4 f4x2SQ = MulSIMD( f4x2, f4x2 );
	bi32x4 badMask = CmpGeSIMD( AddSIMD( f4x1SQ, f4x2SQ ), Four_Ones );
	while( IsAnyTrue( badMask ) )
	{
		f4x1 = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), f4x1 );
		f4x2 = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), f4x2 );
		f4x1SQ = MulSIMD( f4x1, f4x1 );
		f4x2SQ = MulSIMD( f4x2, f4x2 );
		badMask = CmpGeSIMD( AddSIMD( f4x1SQ, f4x2SQ ), Four_Ones );
	}
	// now, we have 2 points on the unit circle
	fltx4 f4OuterArea = SqrtEstSIMD( SubSIMD( Four_Ones, SubSIMD( f4x1SQ, f4x2SQ ) ) );
	out.x = MulSIMD( AddSIMD( f4x1, f4x1 ), f4OuterArea );
	out.y = MulSIMD( AddSIMD( f4x2, f4x2 ), f4OuterArea );
	out.z = SubSIMD( Four_Ones, MulSIMD( Four_Twos, AddSIMD( f4x1, f4x2 ) ) );
}

static inline void RandomPointInUnitSphere( int nRandContext, FourVectors &out )
{
	// generate 4 random points inside the unit sphere. uses rejection method.
	out.x = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	out.y = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	out.z = SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ); // -1..1
	fltx4 f4xSQ = MulSIMD( out.x, out.x );
	fltx4 f4ySQ = MulSIMD( out.y, out.y );
	fltx4 f4zSQ = MulSIMD( out.z, out.z );
	bi32x4 badMask = CmpGtSIMD( AddSIMD( AddSIMD( f4xSQ, f4ySQ ), f4zSQ ), Four_Ones );
	while( IsAnyTrue( badMask ) )
	{
		out.x = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), out.x );
		out.y = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), out.y );
		out.z = MaskedAssign( badMask, SubSIMD( MulSIMD( Four_Twos, RandSIMD( nRandContext ) ), Four_Ones ), out.z );
		f4xSQ = MulSIMD( out.x, out.x );
		f4ySQ = MulSIMD( out.y, out.y );
		f4zSQ = MulSIMD( out.z, out.z );
		badMask = CmpGeSIMD( AddSIMD( AddSIMD( f4xSQ, f4ySQ ), f4zSQ ), Four_Ones );
	}
}



class C_INIT_CreateWithinSphere : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateWithinSphere );

	float m_fRadiusMin;
	float m_fRadiusMax;
	Vector m_vecDistanceBias, m_vecDistanceBiasAbs;
	int m_nControlPointNumber;
	int m_nScaleCP;
	float m_fSpeedMin;
	float m_fSpeedMax;
	float m_fSpeedRandExp;
	bool m_bLocalCoords;
	bool m_bDistanceBiasAbs;
	bool m_bUseHighestEndCP;
	bool m_bDistanceBias;
	float m_flEndCPGrowthTime;
	
	Vector m_LocalCoordinateSystemSpeedMin;
	Vector m_LocalCoordinateSystemSpeedMax;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = ( 1ULL << m_nControlPointNumber );
		if ( m_nScaleCP != -1 )
			nMask |= ( 1ULL << m_nScaleCP );
		if ( !m_bUseHighestEndCP )
			return nMask;
		return ~( ( 1ULL << m_nControlPointNumber ) - 1 );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
										int start_block, int n_blocks, int nAttributeWriteMask,
										void *pContext ) const;

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
		m_nScaleCP = MAX( -1, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nScaleCP ) );
		m_bDistanceBias = ( m_vecDistanceBias.x != 1.0f ) || ( m_vecDistanceBias.y != 1.0f ) || ( m_vecDistanceBias.z != 1.0f );
		m_bDistanceBiasAbs = ( m_vecDistanceBiasAbs.x != 0.0f ) || ( m_vecDistanceBiasAbs.y != 0.0f ) || ( m_vecDistanceBiasAbs.z != 0.0f );
	}

	void Render( CParticleCollection *pParticles ) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateWithinSphere, "Position Within Sphere Random", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreateWithinSphere ) 
	DMXELEMENT_UNPACK_FIELD( "distance_min", "0", float, m_fRadiusMin )
	DMXELEMENT_UNPACK_FIELD( "distance_max", "0", float, m_fRadiusMax )
	DMXELEMENT_UNPACK_FIELD( "distance_bias", "1 1 1", Vector, m_vecDistanceBias )
	DMXELEMENT_UNPACK_FIELD( "distance_bias_absolute_value", "0 0 0", Vector, m_vecDistanceBiasAbs )
	DMXELEMENT_UNPACK_FIELD( "bias in local system", "0", bool, m_bLocalCoords )
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "speed_min", "0", float, m_fSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "speed_max", "0", float, m_fSpeedMax )
	DMXELEMENT_UNPACK_FIELD( "speed_random_exponent", "1", float, m_fSpeedRandExp )
	DMXELEMENT_UNPACK_FIELD( "speed_in_local_coordinate_system_min", "0 0 0", Vector, m_LocalCoordinateSystemSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "speed_in_local_coordinate_system_max", "0 0 0", Vector, m_LocalCoordinateSystemSpeedMax )
	DMXELEMENT_UNPACK_FIELD( "randomly distribute to highest supplied Control Point", "0", bool, m_bUseHighestEndCP )
	DMXELEMENT_UNPACK_FIELD( "randomly distribution growth time", "0", float, m_flEndCPGrowthTime )
	DMXELEMENT_UNPACK_FIELD( "scale cp (distance/speed/local speed)", "-1", int, m_nScaleCP )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateWithinSphere )


ConVar r_sse_s( "r_sse_s", "1", 0, "sse ins for particle sphere create" );

void C_INIT_CreateWithinSphere::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	float flScaleDist = 1.0f;
	float flScaleSpeed = 1.0f;
	float flScaleLocalSpeed = 1.0f;
	if ( m_nScaleCP > -1 )
	{
		Vector vecScale = pParticles->GetControlPointAtCurrentTime( m_nScaleCP );
		flScaleDist = vecScale.x;
		flScaleSpeed = vecScale.y;
		flScaleLocalSpeed = vecScale.z;
	}
	CUtlVector< int > nValidControlPoints;
	if ( m_bUseHighestEndCP )
	{
		for ( int i = m_nControlPointNumber; i <= pParticles->GetHighestControlPoint(); i++ )
		{
			if ( pParticles->GetControlPointAtCurrentTime( i ) == vec3_invalid || pParticles->IsNonPositionalControlPoint( i ) )
				continue;
			nValidControlPoints.AddToTail( i );
		}
	}
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float ct = *( pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p ) );

		ct = MAX( ct, pParticles->m_flCurTime - pParticles->m_flDt );	// don't ask for times too far in the past. This can happen in systems that don't use creation_time.


		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		int nCurrentControlPoint = m_nControlPointNumber;
		float flStrength = 1.0f;
		if ( m_bUseHighestEndCP )
		{
			//hack for growth time instead of using strength as currently initializers don't support it.
			if ( m_flEndCPGrowthTime != 0.0f )
			{
				flStrength = MIN( pParticles->m_flCurTime, m_flEndCPGrowthTime ) / m_flEndCPGrowthTime;
			}
			if ( nValidControlPoints.Count() > 0 )
			{
				if ( flStrength < 1.0 )
				{
					int nHighestControlPoint = floor( RemapValClamped( flStrength, 0.0f, 1.0f, m_nControlPointNumber, MAX( m_nControlPointNumber, pParticles->GetHighestControlPoint() ) ) );
					int nIndex = nValidControlPoints.Find( pParticles->RandomInt( m_nControlPointNumber, nHighestControlPoint ) );
					if ( nIndex < 0 || nIndex >= nValidControlPoints.Count() )
					{
						nIndex = pParticles->RandomInt( 0, nValidControlPoints.Count() - 1 );
					}
					nCurrentControlPoint = nValidControlPoints[nIndex];
				}
				else
				{
					nCurrentControlPoint = nValidControlPoints[pParticles->RandomInt( 0, nValidControlPoints.Count() - 1 )];
				}
			}
			else
			{
				nCurrentControlPoint = m_nControlPointNumber;
			}
		}
		Vector randpos, randDir;

		float flLength = pParticles->RandomVectorInUnitSphere( &randpos );
		
		// Absolute value and biasing for creating hemispheres and ovoids.
		if ( m_bDistanceBiasAbs	)
		{
			if ( m_vecDistanceBiasAbs.x	!= 0.0f )
			{
				randpos.x = fabs(randpos.x);
			}
			if ( m_vecDistanceBiasAbs.y	!= 0.0f )
			{
				randpos.y = fabs(randpos.y);
			}
			if ( m_vecDistanceBiasAbs.z	!= 0.0f )
			{
				randpos.z = fabs(randpos.z);
			}
		}
		randpos *= m_vecDistanceBias;
		randpos.NormalizeInPlace();
		randDir = randpos;
		randpos *= Lerp( flLength, m_fRadiusMin, m_fRadiusMax );
		randpos *= flScaleDist;
		
		if ( !m_bDistanceBias || !m_bLocalCoords )
		{
			Vector vecControlPoint;
			pParticles->GetControlPointAtTime( nCurrentControlPoint, ct, &vecControlPoint );
			randpos += vecControlPoint;
		}
		else
		{
			matrix3x4_t mat;
			pParticles->GetControlPointTransformAtTime( nCurrentControlPoint, ct, &mat );
			Vector vecTransformLocal = vec3_origin;
			VectorTransform( randpos, mat, vecTransformLocal );
			randpos = vecTransformLocal;
		}
		
		xyz[0] = randpos.x;
		xyz[4] = randpos.y;
		xyz[8] = randpos.z;

		Assert( IsFinite( xyz[0] ) );

		// FIXME: Remove this into a speed setting initializer
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			Vector poffset(0,0,0);
			if ( m_fSpeedMax > 0.0 )
			{
				float rand_speed = pParticles->RandomFloatExp( m_fSpeedMin, m_fSpeedMax, m_fSpeedRandExp );
				rand_speed *= flScaleSpeed;
				poffset.x -= rand_speed * randDir.x;
				poffset.y -= rand_speed * randDir.y;
				poffset.z -= rand_speed * randDir.z;
			}
			poffset -=
				pParticles->RandomFloat( m_LocalCoordinateSystemSpeedMin.x, m_LocalCoordinateSystemSpeedMax.x )*
				pParticles->ControlPoint( nCurrentControlPoint ).m_ForwardVector * flScaleLocalSpeed;
			poffset -=
				pParticles->RandomFloat( m_LocalCoordinateSystemSpeedMin.y, m_LocalCoordinateSystemSpeedMax.y )*
				pParticles->ControlPoint( nCurrentControlPoint ).m_RightVector * flScaleLocalSpeed;
			poffset -=
				pParticles->RandomFloat( m_LocalCoordinateSystemSpeedMin.z, m_LocalCoordinateSystemSpeedMax.z )*
				pParticles->ControlPoint( nCurrentControlPoint ).m_UpVector * flScaleLocalSpeed;

			poffset *= pParticles->m_flPreviousDt;
			randpos += poffset;
			pxyz[0] = randpos.x;
			pxyz[4] = randpos.y;
			pxyz[8] = randpos.z;
		}
	}
}

void C_INIT_CreateWithinSphere::InitNewParticlesBlock( CParticleCollection *pParticles, 
													   int start_block, int n_blocks, int nAttributeWriteMask,
													   void *pContext ) const
{
	// sse-favorable settings
	bool bMustUseScalar = m_bUseHighestEndCP;
	if ( m_bDistanceBias && m_bLocalCoords )
		bMustUseScalar = true;

	if ( !bMustUseScalar ) //&& 
		 // (( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) == 0 ) &&
	{
		float flScaleDist = 1.0f;
		float flScaleSpeed = 1.0f;
		float flScaleLocalSpeed = 1.0f;
		if ( m_nScaleCP > -1 )
		{
			Vector vecScale = pParticles->GetControlPointAtCurrentTime( m_nScaleCP );
			flScaleDist = vecScale.x;
			flScaleSpeed = vecScale.y;
			flScaleLocalSpeed = vecScale.z;
		}

		C4VAttributeWriteIterator pXYZ( PARTICLE_ATTRIBUTE_XYZ, pParticles );
		pXYZ += start_block;
		C4VAttributeWriteIterator pPrevXYZ( PARTICLE_ATTRIBUTE_PREV_XYZ, pParticles );
		pPrevXYZ += start_block;
		CM128AttributeIterator pCT( PARTICLE_ATTRIBUTE_CREATION_TIME, pParticles );
		pCT += start_block;
		
		// now, calculate the terms we need for interpolating control points
		FourVectors v4PrevControlPointPosition;
		v4PrevControlPointPosition.DuplicateVector( pParticles->ControlPoint( m_nControlPointNumber ).m_PrevPosition );
		FourVectors v4ControlPointDelta;
		v4ControlPointDelta.DuplicateVector( pParticles->ControlPoint( m_nControlPointNumber ).m_Position );
		v4ControlPointDelta -= v4PrevControlPointPosition;

		float flOODT = ( pParticles->m_flDt > 0.0 ) ? ( 1.0 / pParticles->m_flDt ) : 0.0;
		fltx4 fl4OODt = ReplicateX4( flOODT );
		fltx4 fl4PrevTime = ReplicateX4( pParticles->m_flCurTime - pParticles->m_flDt );
		int nContext = GetSIMDRandContext();

		FourVectors v4DistanceBias;
		v4DistanceBias.DuplicateVector( m_vecDistanceBias );
		FourVectors v4ConditionalAbsMask;
		for( int nComp = 0 ; nComp < 3; nComp++ )
		{
			v4ConditionalAbsMask[nComp] = ( m_vecDistanceBiasAbs[nComp] > 0 ) ?
				LoadAlignedSIMD( ( const float *) g_SIMD_clear_signmask ) :
				LoadAlignedSIMD( ( const float *) g_SIMD_AllOnesMask );
		}
		fltx4 fl4RadiusMin = ReplicateX4( m_fRadiusMin * flScaleDist );
		fltx4 fl4RadiusSpread = ReplicateX4( ( m_fRadiusMax - m_fRadiusMin ) * flScaleDist );
		int nPowSSEMask = 4.0 * m_fSpeedRandExp;

		bool bDoRandSpeed =
			( m_fSpeedMax > 0. ) || 
			( m_LocalCoordinateSystemSpeedMax.x != 0 ) ||
			( m_LocalCoordinateSystemSpeedMax.y != 0 ) ||
			( m_LocalCoordinateSystemSpeedMax.z != 0 ) ||
			( m_LocalCoordinateSystemSpeedMin.x != 0 ) ||
			( m_LocalCoordinateSystemSpeedMin.y != 0 ) ||
			( m_LocalCoordinateSystemSpeedMin.z != 0 );


		fltx4 fl4SpeedMin = ReplicateX4( m_fSpeedMin * flScaleSpeed );
		fltx4 fl4SpeedRange = ReplicateX4( ( m_fSpeedMax - m_fSpeedMin ) * flScaleSpeed );

		fltx4 fl4LocalSpeedMinX = ReplicateX4( m_LocalCoordinateSystemSpeedMin.x  * flScaleLocalSpeed );
		fltx4 fl4LocalSpeedXSpread = ReplicateX4( ( m_LocalCoordinateSystemSpeedMax.x - 
												  m_LocalCoordinateSystemSpeedMin.x ) * flScaleLocalSpeed );
		fltx4 fl4LocalSpeedMinY = ReplicateX4( m_LocalCoordinateSystemSpeedMin.y * flScaleLocalSpeed );
		fltx4 fl4LocalSpeedYSpread = ReplicateX4( ( m_LocalCoordinateSystemSpeedMax.y - 
												  m_LocalCoordinateSystemSpeedMin.y ) * flScaleLocalSpeed );
		fltx4 fl4LocalSpeedMinZ = ReplicateX4( m_LocalCoordinateSystemSpeedMin.z * flScaleLocalSpeed );
		fltx4 fl4LocalSpeedZSpread = ReplicateX4( ( m_LocalCoordinateSystemSpeedMax.z - 
												  m_LocalCoordinateSystemSpeedMin.z ) * flScaleLocalSpeed );

		FourVectors v4CPForward;
		v4CPForward.DuplicateVector( pParticles->ControlPoint( m_nControlPointNumber ).m_ForwardVector );
		FourVectors v4CPUp;
		v4CPUp.DuplicateVector( pParticles->ControlPoint( m_nControlPointNumber ).m_UpVector );
		FourVectors v4CPRight;
		v4CPRight.DuplicateVector( pParticles->ControlPoint( m_nControlPointNumber ).m_RightVector );

		fltx4 fl4PreviousDt = ReplicateX4( pParticles->m_flPreviousDt );

		while( n_blocks-- )
		{
			FourVectors v4RandPos;
			RandomPointInUnitSphere( nContext, v4RandPos );

			fltx4 fl4Length = v4RandPos.length();

			// conditional absolute value
			v4RandPos.x = AndSIMD( v4RandPos.x, v4ConditionalAbsMask.x );
			v4RandPos.y = AndSIMD( v4RandPos.y, v4ConditionalAbsMask.y );
			v4RandPos.z = AndSIMD( v4RandPos.z, v4ConditionalAbsMask.z );

			v4RandPos *= v4DistanceBias;
			v4RandPos.VectorNormalizeFast();
			
			FourVectors v4randDir = v4RandPos;
			
			// lerp radius
			v4RandPos *= AddSIMD( fl4RadiusMin, MulSIMD( fl4Length, fl4RadiusSpread ) );
			v4RandPos += v4PrevControlPointPosition;

			FourVectors cpnt = v4ControlPointDelta;
			cpnt *= MulSIMD( SubSIMD( *pCT, fl4PrevTime ), fl4OODt );
			v4RandPos += cpnt;

			*(pXYZ) = v4RandPos;

			if ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK )
			{
				if ( bDoRandSpeed )
				{
					fltx4 fl4Rand_speed = Pow_FixedPoint_Exponent_SIMD( RandSIMD( nContext ), nPowSSEMask );
					fl4Rand_speed = AddSIMD( fl4SpeedMin, MulSIMD( fl4SpeedRange, fl4Rand_speed ) );
					v4randDir *= fl4Rand_speed;

					// local speed
					FourVectors v4LocalOffset = v4CPForward;
					v4LocalOffset *= AddSIMD( fl4LocalSpeedMinX, 
											  MulSIMD( fl4LocalSpeedXSpread, RandSIMD( nContext ) ) );
					v4randDir += v4LocalOffset;

					v4LocalOffset = v4CPRight;
					v4LocalOffset *= AddSIMD( fl4LocalSpeedMinY, 
											  MulSIMD( fl4LocalSpeedYSpread, RandSIMD( nContext ) ) );
					v4randDir += v4LocalOffset;


					v4LocalOffset = v4CPUp;
					v4LocalOffset *= AddSIMD( fl4LocalSpeedMinZ, 
											  MulSIMD( fl4LocalSpeedZSpread, RandSIMD( nContext ) ) );
					v4randDir += v4LocalOffset;
					v4randDir *= fl4PreviousDt;
					v4RandPos -= v4randDir;
				}
				*(pPrevXYZ) = v4RandPos;

			}



			++pXYZ;
			++pPrevXYZ;
			++pCT;
		}
		ReleaseSIMDRandContext( nContext );

	}
	else
		CParticleOperatorInstance::InitNewParticlesBlock( pParticles, start_block, n_blocks, nAttributeWriteMask, pContext );

}


//-----------------------------------------------------------------------------
// Render visualization
//-----------------------------------------------------------------------------
void C_INIT_CreateWithinSphere::Render( CParticleCollection *pParticles ) const
{					   
	Vector vecOrigin;
	pParticles->GetControlPointAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &vecOrigin );
	RenderWireframeSphere( vecOrigin, m_fRadiusMin, 16, 8, Color( 192, 192, 0, 255 ), false );
	RenderWireframeSphere( vecOrigin, m_fRadiusMax, 16, 8, Color( 128, 128, 0, 255 ), false );
}




class C_INIT_CreateWithinBox : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateWithinBox );

	Vector m_vecMin;
	Vector m_vecMax;
	int m_nControlPointNumber;
	bool m_bLocalSpace;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	void Render( CParticleCollection *pParticles ) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateWithinBox, "Position Within Box Random", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreateWithinBox ) 
	DMXELEMENT_UNPACK_FIELD( "min", "0 0 0", Vector, m_vecMin )
	DMXELEMENT_UNPACK_FIELD( "max", "0 0 0", Vector, m_vecMax )
	DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "use local space", "0", bool, m_bLocalSpace )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateWithinBox )


void C_INIT_CreateWithinBox::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	int nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		Vector randpos;
		pParticles->RandomVector( m_vecMin, m_vecMax, &randpos );
		if ( m_bLocalSpace )
		{
			matrix3x4_t mat;
			pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *ct, &mat );
			Vector vecTransformLocal = vec3_origin;
			VectorRotate( randpos, mat, vecTransformLocal );
			randpos = vecTransformLocal;			
		}
		else
		{
			Vector vecControlPoint;
			pParticles->GetControlPointAtTime( nControlPointNumber, *ct, &vecControlPoint );
			randpos += vecControlPoint;
		}


		xyz[0] = randpos.x;
		xyz[4] = randpos.y;
		xyz[8] = randpos.z;
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			pxyz[0] = randpos.x;
			pxyz[4] = randpos.y;
			pxyz[8] = randpos.z;
		}
	}
}

//-----------------------------------------------------------------------------
// Render visualization
//-----------------------------------------------------------------------------
void C_INIT_CreateWithinBox::Render( CParticleCollection *pParticles ) const
{					   
	Vector vecOrigin;
	pParticles->GetControlPointAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &vecOrigin );
	RenderWireframeBox( vecOrigin, vec3_angle, m_vecMin, m_vecMax, Color( 192, 192, 0, 255 ), false );
}



//-----------------------------------------------------------------------------
// Position Offset Initializer
// offsets initial position of particles within a random vector range,
// while still respecting spherical/conical spacial and velocity initialization
//-----------------------------------------------------------------------------
class C_INIT_PositionOffset : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_PositionOffset );

	Vector m_OffsetMin;
	Vector m_OffsetMax;
	int m_nControlPointNumber;
	bool m_bLocalCoords;
	bool m_bProportional;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }

	void Render( CParticleCollection *pParticles ) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_PositionOffset, "Position Modify Offset Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_PositionOffset ) 
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "offset min", "0 0 0", Vector, m_OffsetMin )
	DMXELEMENT_UNPACK_FIELD( "offset max", "0 0 0", Vector, m_OffsetMax )
	DMXELEMENT_UNPACK_FIELD( "offset in local space 0/1", "0", bool, m_bLocalCoords )
	DMXELEMENT_UNPACK_FIELD( "offset proportional to radius 0/1", "0", bool, m_bProportional )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_PositionOffset )


void C_INIT_PositionOffset::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		const float *radius = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, start_p );
		
		Vector randpos;
		
		if ( m_bProportional )
		{
			pParticles->RandomVector( (m_OffsetMin * *radius), (m_OffsetMax * *radius), &randpos );
		}
		else
		{
			pParticles->RandomVector( m_OffsetMin, m_OffsetMax, &randpos );
		}

		if ( m_bLocalCoords )
		{
			matrix3x4_t mat;
			pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *ct, &mat );
			Vector vecTransformLocal = vec3_origin;
			VectorRotate( randpos, mat, vecTransformLocal );
			randpos = vecTransformLocal;
		}

		xyz[0] += randpos.x;
		xyz[4] += randpos.y;
		xyz[8] += randpos.z;
		pxyz[0] += randpos.x;
		pxyz[4] += randpos.y;
		pxyz[8] += randpos.z;
	}
}


//-----------------------------------------------------------------------------
// Render visualization
//-----------------------------------------------------------------------------
void C_INIT_PositionOffset::Render( CParticleCollection *pParticles ) const
{					   
	Vector vecOrigin (0,0,0);
	Vector vecMinExtent = m_OffsetMin;
	Vector vecMaxExtent = m_OffsetMax;
	if ( m_bLocalCoords )
	{
		matrix3x4_t mat;
		pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &mat );
		VectorRotate( m_OffsetMin, mat, vecMinExtent );
		VectorRotate( m_OffsetMax, mat, vecMaxExtent ); 
	}
	else
	{
		pParticles->GetControlPointAtTime( m_nControlPointNumber, pParticles->m_flCurTime, &vecOrigin );
	}
	RenderWireframeBox( vecOrigin, vec3_angle, vecMinExtent , vecMaxExtent , Color( 192, 192, 0, 255 ), false );
}



//-----------------------------------------------------------------------------
// Position Modify Place on Ground Initializer
//-----------------------------------------------------------------------------
class C_INIT_PositionPlaceOnGround : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_PositionPlaceOnGround );

	float m_flOffset;
	float m_flMaxTraceLength;
	char m_CollisionGroupName[128];
	int m_nCollisionGroupNumber;
	unsigned int m_CollisionMask;
	bool m_bKill;
	bool m_bIncludeWater;
	bool m_bSetNormal;

	uint32 GetWrittenAttributes( void ) const
	{

		uint32 nMask = PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
		if ( m_bSetNormal )
			nMask = PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK | PARTICLE_ATTRIBUTE_NORMAL_MASK;
		return nMask;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nCollisionGroupNumber = g_pParticleSystemMgr->Query()->GetCollisionGroupFromName( m_CollisionGroupName );
		if ( m_bIncludeWater )
			m_CollisionMask = MASK_SHOT_HULL|MASK_SPLITAREAPORTAL;
		else
			m_CollisionMask = MASK_SHOT_HULL;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	bool InitMultipleOverride ( void ) { return true; }

};

DEFINE_PARTICLE_OPERATOR( C_INIT_PositionPlaceOnGround, "Position Modify Place On Ground", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_PositionPlaceOnGround ) 
DMXELEMENT_UNPACK_FIELD( "offset", "0", float, m_flOffset )
DMXELEMENT_UNPACK_FIELD( "kill on no collision", "0", bool, m_bKill )
DMXELEMENT_UNPACK_FIELD( "include water", "0", bool, m_bIncludeWater )
DMXELEMENT_UNPACK_FIELD( "set normal", "0", bool, m_bSetNormal )
DMXELEMENT_UNPACK_FIELD( "max trace length", "128", float, m_flMaxTraceLength )
DMXELEMENT_UNPACK_FIELD_STRING( "collision group", "NONE", m_CollisionGroupName )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_PositionPlaceOnGround )


void C_INIT_PositionPlaceOnGround::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	// Trace down
	Vector TraceDir=Vector(0, 0, -1);

	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		float *plife = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );

		Vector vecStartPos;
		SetVectorFromAttribute( vecStartPos, xyz );

		CBaseTrace tr;
		g_pParticleSystemMgr->Query()->TraceLine( vecStartPos, ( vecStartPos + ( TraceDir * m_flMaxTraceLength ) ), m_CollisionMask, NULL, m_nCollisionGroupNumber, &tr );
		if ( tr.fraction == 1.0  && m_bKill )
		{
			*plife = -1.0f;
		}
		else
		{
			float flGroundPos = ( tr.fraction * m_flMaxTraceLength ) - m_flOffset;
			xyz[8] -= flGroundPos;
			pxyz[8] -= flGroundPos;
		}
		if ( m_bSetNormal )
		{
			float *normal = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_NORMAL, start_p );
			SetVectorAttribute( normal, tr.plane.normal );
		}
	}
}


//-----------------------------------------------------------------------------
//
// Velocity-based Operators
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Random velocity initializer
//-----------------------------------------------------------------------------
class C_INIT_VelocityRandom : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_VelocityRandom );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		if ( m_bHasLocalSpeed )
			return 1ULL << m_nControlPointNumber;
		return 0;
	}

	virtual bool InitMultipleOverride() { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
		m_bHasLocalSpeed = ( m_LocalCoordinateSystemSpeedMin != vec3_origin ) || ( m_LocalCoordinateSystemSpeedMax != vec3_origin );  
		if ( m_fSpeedMax < m_fSpeedMin )
		{
			V_swap( m_fSpeedMin, m_fSpeedMax );
		}
	}

private:
	int m_nControlPointNumber;
	float m_fSpeedMin;
	float m_fSpeedMax;
	Vector m_LocalCoordinateSystemSpeedMin;
	Vector m_LocalCoordinateSystemSpeedMax;
	bool m_bHasLocalSpeed;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_VelocityRandom, "Velocity Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_VelocityRandom ) 
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "random_speed_min", "0", float, m_fSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "random_speed_max", "0", float, m_fSpeedMax )
	DMXELEMENT_UNPACK_FIELD( "speed_in_local_coordinate_system_min", "0 0 0", Vector, m_LocalCoordinateSystemSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "speed_in_local_coordinate_system_max", "0 0 0", Vector, m_LocalCoordinateSystemSpeedMax )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_VelocityRandom )


void C_INIT_VelocityRandom::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			
		Vector vecVelocity( 0.0f, 0.0f, 0.0f );
		if ( m_bHasLocalSpeed )
		{
			Vector vecRandomSpeed, vecForward, vecUp, vecRight;
			pParticles->RandomVector( m_LocalCoordinateSystemSpeedMin, m_LocalCoordinateSystemSpeedMax, &vecRandomSpeed );
			pParticles->GetControlPointOrientationAtTime( m_nControlPointNumber, *ct, &vecForward, &vecRight, &vecUp );
			VectorMA( vecVelocity, vecRandomSpeed.x, vecForward, vecVelocity );
			VectorMA( vecVelocity, -vecRandomSpeed.y, vecRight, vecVelocity );
			VectorMA( vecVelocity, vecRandomSpeed.z, vecUp, vecVelocity );
		}

		if ( m_fSpeedMax > 0.0f )
		{
			Vector vecRandomSpeed;
			pParticles->RandomVector( m_fSpeedMin, m_fSpeedMax, &vecRandomSpeed );
			vecVelocity += vecRandomSpeed;
		}

		vecVelocity *= pParticles->m_flPreviousDt;
		pxyz[0] -= vecVelocity.x;
		pxyz[4] -= vecVelocity.y;
		pxyz[8] -= vecVelocity.z;
	}
}


//-----------------------------------------------------------------------------
// Initial Velocity Noise Operator
//-----------------------------------------------------------------------------
class C_INIT_InitialVelocityNoise : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_InitialVelocityNoise );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK;
	}
	
	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;	

	void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const;

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	virtual bool InitMultipleOverride() { return true; }

	Vector	m_vecAbsVal, m_vecAbsValInv, m_vecOffsetLoc;
	float	m_flOffset;
	Vector	m_vecOutputMin;
	Vector	m_vecOutputMax;
	float	m_flNoiseScale, m_flNoiseScaleLoc;
	int		nRemainingBlocks, m_nControlPointNumber;
	bool	m_bLocalSpace;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_InitialVelocityNoise, "Velocity Noise", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_InitialVelocityNoise )
	DMXELEMENT_UNPACK_FIELD( "Control Point Number","0",int,m_nControlPointNumber)
	DMXELEMENT_UNPACK_FIELD( "Time Noise Coordinate Scale","1",float,m_flNoiseScale)
	DMXELEMENT_UNPACK_FIELD( "Spatial Noise Coordinate Scale","0.01",float,m_flNoiseScaleLoc)
	DMXELEMENT_UNPACK_FIELD( "Time Coordinate Offset","0", float, m_flOffset )
	DMXELEMENT_UNPACK_FIELD( "Spatial Coordinate Offset","0 0 0", Vector, m_vecOffsetLoc )
	DMXELEMENT_UNPACK_FIELD( "Absolute Value","0 0 0", Vector, m_vecAbsVal )
	DMXELEMENT_UNPACK_FIELD( "Invert Abs Value","0 0 0", Vector, m_vecAbsValInv )
	DMXELEMENT_UNPACK_FIELD( "output minimum","0 0 0", Vector, m_vecOutputMin )
	DMXELEMENT_UNPACK_FIELD( "output maximum","1 1 1", Vector, m_vecOutputMax )
	DMXELEMENT_UNPACK_FIELD( "Apply Velocity in Local Space (0/1)","0", bool, m_bLocalSpace )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_InitialVelocityNoise );


void C_INIT_InitialVelocityNoise::InitNewParticlesBlock( CParticleCollection *pParticles, 
								   int start_block, int n_blocks, int nAttributeWriteMask,
								   void *pContext ) const
{
	float		flAbsScaleX, flAbsScaleY, flAbsScaleZ;
	fltx4 		fl4AbsValX, fl4AbsValY, fl4AbsValZ;
	fl4AbsValX = ( fltx4 ) CmpEqSIMD( Four_Zeros, Four_Zeros ); 
	fl4AbsValY = fl4AbsValX;
	fl4AbsValZ = fl4AbsValX;
	flAbsScaleX = 0.5;
	flAbsScaleY = 0.5; 
	flAbsScaleZ = 0.5;

	// Set up single if check for absolute value inversion inside the loop
	bool m_bNoiseAbs = ( m_vecAbsValInv.x != 0.0f ) || ( m_vecAbsValInv.y != 0.0f ) || ( m_vecAbsValInv.z != 0.0f );
	// Set up values for more optimal absolute value calculations inside the loop
	if ( m_vecAbsVal.x	!= 0.0f )
	{
		fl4AbsValX = LoadAlignedSIMD( (float *) g_SIMD_clear_signmask );
		flAbsScaleX = 1.0;
	}
	if ( m_vecAbsVal.y	!= 0.0f )
	{
		fl4AbsValY = LoadAlignedSIMD( (float *) g_SIMD_clear_signmask );
		flAbsScaleY = 1.0;
	}
	if ( m_vecAbsVal.z	!= 0.0f )
	{
		fl4AbsValZ = LoadAlignedSIMD( (float *) g_SIMD_clear_signmask );
		flAbsScaleZ = 1.0;
	}

	float ValueScaleX, ValueScaleY, ValueScaleZ, ValueBaseX, ValueBaseY, ValueBaseZ;

	ValueScaleX = ( flAbsScaleX *(m_vecOutputMax.x-m_vecOutputMin.x ) );
	ValueBaseX = (m_vecOutputMin.x+ ( ( 1.0 - flAbsScaleX ) *( m_vecOutputMax.x-m_vecOutputMin.x ) ) );

	ValueScaleY = ( flAbsScaleY *(m_vecOutputMax.y-m_vecOutputMin.y ) );
	ValueBaseY = (m_vecOutputMin.y+ ( ( 1.0 - flAbsScaleY ) *( m_vecOutputMax.y-m_vecOutputMin.y ) ) );

	ValueScaleZ = ( flAbsScaleZ *(m_vecOutputMax.z-m_vecOutputMin.z ) );
	ValueBaseZ = (m_vecOutputMin.z+ ( ( 1.0 - flAbsScaleZ ) *( m_vecOutputMax.z-m_vecOutputMin.z ) ) );

	fltx4 fl4ValueBaseX = ReplicateX4( ValueBaseX );
	fltx4 fl4ValueBaseY = ReplicateX4( ValueBaseY );
	fltx4 fl4ValueBaseZ = ReplicateX4( ValueBaseZ );

	fltx4 fl4ValueScaleX = ReplicateX4( ValueScaleX );
	fltx4 fl4ValueScaleY = ReplicateX4( ValueScaleY );
	fltx4 fl4ValueScaleZ = ReplicateX4( ValueScaleZ );

	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

	Vector ofs_y = Vector( 100000.5, 300000.25, 9000000.75 );
	Vector ofs_z = Vector( 110000.25, 310000.75, 9100000.5 );

	size_t attr_stride;

	const FourVectors *xyz = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &attr_stride );
	xyz += attr_stride * start_block;
	FourVectors *pxyz = pParticles->Get4VAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, &attr_stride );
	pxyz += attr_stride * start_block;
	const fltx4 *pCreationTime = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &attr_stride );
	pCreationTime += attr_stride * start_block;

	// setup
	fltx4 fl4Offset = ReplicateX4( m_flOffset );
	FourVectors fvOffsetLoc;
	fvOffsetLoc.DuplicateVector( m_vecOffsetLoc );
	CParticleSIMDTransformation CPTransform;
	float flCreationTime = SubFloat( *pCreationTime, 0 );
	pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, flCreationTime, &CPTransform );

	while( n_blocks-- )
	{	
		FourVectors fvCoordLoc = *xyz;
		fvCoordLoc += fvOffsetLoc;

		FourVectors fvCoord;
		fvCoord.x = AddSIMD(*pCreationTime, fl4Offset);
		fvCoord.y = AddSIMD(*pCreationTime, fl4Offset);
		fvCoord.z = AddSIMD(*pCreationTime, fl4Offset);
		fvCoordLoc *= CoordScaleLoc;
		fvCoord *= CoordScale;
		fvCoord += fvCoordLoc;

		FourVectors fvCoord2 = fvCoord;
		FourVectors fvOffsetTemp;
		fvOffsetTemp.DuplicateVector( ofs_y );
		fvCoord2 +=  fvOffsetTemp;
		FourVectors fvCoord3 = fvCoord;
		fvOffsetTemp.DuplicateVector( ofs_z );
		fvCoord3 += fvOffsetTemp;

		fltx4 fl4NoiseX;
		fltx4 fl4NoiseY;
		fltx4 fl4NoiseZ;

		fl4NoiseX = NoiseSIMD( fvCoord );

		fl4NoiseY = NoiseSIMD( fvCoord2 );

		fl4NoiseZ = NoiseSIMD( fvCoord3 );

		fl4NoiseX = AndSIMD ( fl4NoiseX, fl4AbsValX );
		fl4NoiseY = AndSIMD ( fl4NoiseY, fl4AbsValY );
		fl4NoiseZ = AndSIMD ( fl4NoiseZ, fl4AbsValZ );

		if ( m_bNoiseAbs )
		{
			if ( m_vecAbsValInv.x	!= 0.0f )
			{
				fl4NoiseX = SubSIMD( Four_Ones, fl4NoiseX );
			}

			if ( m_vecAbsValInv.y	!= 0.0f )
			{											   
				fl4NoiseY = SubSIMD( Four_Ones, fl4NoiseY );
			}
			if ( m_vecAbsValInv.z	!= 0.0f )
			{
				fl4NoiseZ = SubSIMD( Four_Ones, fl4NoiseZ );
			}
		}

		FourVectors fvOffset;

		fvOffset.x = AddSIMD( fl4ValueBaseX, ( MulSIMD( fl4ValueScaleX , fl4NoiseX ) ) );
		fvOffset.y = AddSIMD( fl4ValueBaseY, ( MulSIMD( fl4ValueScaleY , fl4NoiseY ) ) );
		fvOffset.z = AddSIMD( fl4ValueBaseZ, ( MulSIMD( fl4ValueScaleZ , fl4NoiseZ ) ) );

		fvOffset *= pParticles->m_flPreviousDt;  

		if ( m_bLocalSpace )
		{
			CPTransform.VectorRotate( fvOffset );
		}

		*pxyz -= fvOffset;

		xyz += attr_stride;
		pxyz += attr_stride;
		pCreationTime += attr_stride;

	}
}


void C_INIT_InitialVelocityNoise::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	float	flAbsScaleX, flAbsScaleY, flAbsScaleZ;
	int		nAbsValX, nAbsValY, nAbsValZ;
	nAbsValX = 0xffffffff; 
	nAbsValY = 0xffffffff;
	nAbsValZ = 0xffffffff;
	flAbsScaleX = 0.5;
	flAbsScaleY = 0.5; 
	flAbsScaleZ = 0.5;
	// Set up single if check for absolute value inversion inside the loop
	bool m_bNoiseAbs = ( m_vecAbsValInv.x != 0.0f ) || ( m_vecAbsValInv.y != 0.0f ) || ( m_vecAbsValInv.z != 0.0f );
	// Set up values for more optimal absolute value calculations inside the loop
	if ( m_vecAbsVal.x	!= 0.0f )
	{
		nAbsValX = 0x7fffffff;
		flAbsScaleX = 1.0;
	}
	if ( m_vecAbsVal.y	!= 0.0f )
	{
		nAbsValY = 0x7fffffff;
		flAbsScaleY = 1.0;
	}
	if ( m_vecAbsVal.z	!= 0.0f )
	{
		nAbsValZ = 0x7fffffff;
		flAbsScaleZ = 1.0;
	}

	float ValueScaleX, ValueScaleY, ValueScaleZ, ValueBaseX, ValueBaseY, ValueBaseZ;

	ValueScaleX = ( flAbsScaleX *(m_vecOutputMax.x-m_vecOutputMin.x ) );
	ValueBaseX = (m_vecOutputMin.x+ ( ( 1.0 - flAbsScaleX ) *( m_vecOutputMax.x-m_vecOutputMin.x ) ) );

	ValueScaleY = ( flAbsScaleY *(m_vecOutputMax.y-m_vecOutputMin.y ) );
	ValueBaseY = (m_vecOutputMin.y+ ( ( 1.0 - flAbsScaleY ) *( m_vecOutputMax.y-m_vecOutputMin.y ) ) );

	ValueScaleZ = ( flAbsScaleZ *(m_vecOutputMax.z-m_vecOutputMin.z ) );
	ValueBaseZ = (m_vecOutputMin.z+ ( ( 1.0 - flAbsScaleZ ) *( m_vecOutputMax.z-m_vecOutputMin.z ) ) );


	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

	Vector ofs_y = Vector( 100000.5, 300000.25, 9000000.75 );
	Vector ofs_z = Vector( 110000.25, 310000.75, 9100000.5 );

	for( ; nParticleCount--; start_p++ )
	{	
		const float *xyz = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, start_p );		
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
	
		Vector Coord, Coord2, Coord3, CoordLoc;
		SetVectorFromAttribute( CoordLoc, xyz );
		CoordLoc += m_vecOffsetLoc;

		float Offset = m_flOffset;
		Coord = Vector ( (*pCreationTime + Offset), (*pCreationTime + Offset), (*pCreationTime + Offset) );

		Coord *= CoordScale;
		CoordLoc *= CoordScaleLoc;
		Coord += CoordLoc;

		Coord2 = ( Coord );
		Coord3 = ( Coord );

		fltx4 flNoise128;
		FourVectors fvNoise;

		fvNoise.DuplicateVector( Coord );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoiseX = SubFloat( flNoise128, 0 );

		fvNoise.DuplicateVector( Coord2 + ofs_y );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoiseY = SubFloat( flNoise128, 0 );

		fvNoise.DuplicateVector( Coord3 + ofs_z );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoiseZ = SubFloat( flNoise128, 0 );

		*( (int *) &flNoiseX)  &= nAbsValX;
		*( (int *) &flNoiseY)  &= nAbsValY;
		*( (int *) &flNoiseZ)  &= nAbsValZ;

		if ( m_bNoiseAbs )
		{
			if ( m_vecAbsValInv.x	!= 0.0f )
			{
				flNoiseX = 1.0 - flNoiseX;
			}

			if ( m_vecAbsValInv.y	!= 0.0f )
			{											   
				flNoiseY = 1.0 - flNoiseY;
			}
			if ( m_vecAbsValInv.z	!= 0.0f )
			{
				flNoiseZ = 1.0 - flNoiseZ;
			}
		}

		Vector poffset;
		poffset.x = ( ValueBaseX + ( ValueScaleX * flNoiseX ) );
		poffset.y = ( ValueBaseY + ( ValueScaleY * flNoiseY ) );
		poffset.z = ( ValueBaseZ + ( ValueScaleZ * flNoiseZ ) );

		poffset *= pParticles->m_flPreviousDt;  

		if ( m_bLocalSpace )
		{
			matrix3x4_t mat;
			pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *pCreationTime, &mat );
			Vector vecTransformLocal = vec3_origin;
			VectorRotate( poffset, mat, vecTransformLocal );
			poffset = vecTransformLocal;
		}
		pxyz[0] -= poffset.x;
		pxyz[4] -= poffset.y;
		pxyz[8] -= poffset.z;
	}
}




class C_INIT_RandomLifeTime : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomLifeTime );

	float m_fLifetimeMin;
	float m_fLifetimeMax;
	float m_fLifetimeRandExponent;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask, void *pContext ) const;

	void InitNewParticlesBlock( CParticleCollection *pParticles, 
										int start_block, int n_blocks, int nAttributeWriteMask,
										void *pContext ) const
	{
		InitScalarAttributeRandomRangeExpBlock( PARTICLE_ATTRIBUTE_LIFE_DURATION,
												m_fLifetimeMin, m_fLifetimeMax, m_fLifetimeRandExponent,
												pParticles, start_block, n_blocks );
	}

};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomLifeTime, "Lifetime Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomLifeTime ) 
	DMXELEMENT_UNPACK_FIELD( "lifetime_min", "0", float, m_fLifetimeMin )
	DMXELEMENT_UNPACK_FIELD( "lifetime_max", "0", float, m_fLifetimeMax )
	DMXELEMENT_UNPACK_FIELD( "lifetime_random_exponent", "1", float, m_fLifetimeRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomLifeTime )

void C_INIT_RandomLifeTime::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	InitScalarAttributeRandomRangeExpScalar( PARTICLE_ATTRIBUTE_LIFE_DURATION,
											 m_fLifetimeMin, m_fLifetimeMax, m_fLifetimeRandExponent,
											 pParticles, start_p, nParticleCount );
}



class C_INIT_RandomScalar : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomScalar );

	float m_flMin;
	float m_flMax;
	float m_flExponent;
	int m_nFieldOutput;

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask, void *pContext ) const;

	void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		InitScalarAttributeRandomRangeExpBlock( m_nFieldOutput,
			m_flMin, m_flMax, m_flExponent,
			pParticles, start_block, n_blocks );
	}

};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomScalar, "Scalar Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomScalar ) 
DMXELEMENT_UNPACK_FIELD( "min", "0", float, m_flMin )
DMXELEMENT_UNPACK_FIELD( "max", "0", float, m_flMax )
DMXELEMENT_UNPACK_FIELD( "exponent", "1", float, m_flExponent )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomScalar )

void C_INIT_RandomScalar::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	InitScalarAttributeRandomRangeExpScalar( m_nFieldOutput,
		m_flMin, m_flMax, m_flExponent,
		pParticles, start_p, nParticleCount );
}



class C_INIT_RandomVector : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomVector );

	Vector m_vecMin;
	Vector m_vecMax;
	int m_nFieldOutput;

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask, void *pContext ) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomVector, "Vector Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomVector ) 
DMXELEMENT_UNPACK_FIELD( "min", "0 0 0", Vector, m_vecMin )
DMXELEMENT_UNPACK_FIELD( "max", "0 0 0", Vector, m_vecMax )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "0", int, m_nFieldOutput, "intchoice particlefield_vector" )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomVector )

void C_INIT_RandomVector::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		// Setup
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		Vector vecOutput;
		pParticles->RandomVector( m_vecMin, m_vecMax, &vecOutput );
		SetVectorAttribute( pOutput, vecOutput );
	}
}


class C_INIT_RandomVectorComponent : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomVectorComponent );

	float m_flMin;
	float m_flMax;
	int m_nFieldOutput;
	int m_nComponent;

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nComponent = clamp( m_nComponent, 0, 2 );
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask, void *pContext ) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomVectorComponent, "Vector Component Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomVectorComponent ) 
DMXELEMENT_UNPACK_FIELD( "min", "0", float, m_flMin )
DMXELEMENT_UNPACK_FIELD( "max", "0", float, m_flMax )
DMXELEMENT_UNPACK_FIELD( "component 0/1/2 X/Y/Z", "0", int, m_nComponent )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "0", int, m_nFieldOutput, "intchoice particlefield_vector" )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomVectorComponent )

void C_INIT_RandomVectorComponent::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		const float *pInput = pParticles->GetFloatAttributePtr( m_nFieldOutput, start_p );
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		Vector vecOutput;
		if ( pInput )
			SetVectorFromAttribute( vecOutput, pInput );
		else
			vecOutput = vec3_origin;

		vecOutput[m_nComponent] = pParticles->RandomFloat( m_flMin, m_flMax );
		SetVectorAttribute( pOutput, vecOutput );
	}
}


//-----------------------------------------------------------------------------
// Random radius
//-----------------------------------------------------------------------------
class C_INIT_RandomRadius : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomRadius );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_RADIUS_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		InitScalarAttributeRandomRangeExpScalar( PARTICLE_ATTRIBUTE_RADIUS, m_flRadiusMin, m_flRadiusMax, 
												 m_flRadiusRandExponent, pParticles, start_p, nParticleCount );
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
										int start_block, int n_blocks, int nAttributeWriteMask,
										void *pContext ) const
	{
		InitScalarAttributeRandomRangeExpBlock( PARTICLE_ATTRIBUTE_RADIUS,
												m_flRadiusMin, m_flRadiusMax, m_flRadiusRandExponent,
												pParticles, start_block, n_blocks );
	}

	float m_flRadiusMin;
	float m_flRadiusMax;
	float m_flRadiusRandExponent;
};


DEFINE_PARTICLE_OPERATOR( C_INIT_RandomRadius, "Radius Random", OPERATOR_PI_RADIUS );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomRadius ) 
	DMXELEMENT_UNPACK_FIELD( "radius_min", "1", float, m_flRadiusMin )
	DMXELEMENT_UNPACK_FIELD( "radius_max", "1", float, m_flRadiusMax )
	DMXELEMENT_UNPACK_FIELD( "radius_random_exponent", "1", float, m_flRadiusRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomRadius )



//-----------------------------------------------------------------------------
// Random alpha
//-----------------------------------------------------------------------------
class C_INIT_RandomAlpha : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomAlpha );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_ALPHA_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
		m_flAlphaMin = m_nAlphaMin / 255.0f;
		m_flAlphaMax = m_nAlphaMax / 255.0f;
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		InitScalarAttributeRandomRangeExpBlock( PARTICLE_ATTRIBUTE_ALPHA,
												m_flAlphaMin, m_flAlphaMax, m_flAlphaRandExponent,
												pParticles, start_block, n_blocks );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		InitScalarAttributeRandomRangeExpScalar( PARTICLE_ATTRIBUTE_ALPHA,
												 m_flAlphaMin, m_flAlphaMax, m_flAlphaRandExponent,
												 pParticles, start_p, nParticleCount );
	}

	int m_nAlphaMin;
	int m_nAlphaMax;
	float m_flAlphaMin;
	float m_flAlphaMax;
	float m_flAlphaRandExponent;
};


DEFINE_PARTICLE_OPERATOR( C_INIT_RandomAlpha, "Alpha Random", OPERATOR_PI_ALPHA );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomAlpha ) 
	DMXELEMENT_UNPACK_FIELD( "alpha_min", "255", int, m_nAlphaMin )
	DMXELEMENT_UNPACK_FIELD( "alpha_max", "255", int, m_nAlphaMax )
	DMXELEMENT_UNPACK_FIELD( "alpha_random_exponent", "1", float, m_flAlphaRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomAlpha )


//-----------------------------------------------------------------------------
// Random rotation
//-----------------------------------------------------------------------------
class CGeneralRandomRotation : public CParticleInitializerOperatorInstance
{
protected:
	virtual int GetAttributeToInit( void ) const = 0;

	uint32 GetWrittenAttributes( void ) const
	{
		return (1 << GetAttributeToInit() );
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
		m_flRadians = m_flDegrees * ( M_PI / 180.0f );
		m_flRadiansMin = m_flDegreesMin * ( M_PI / 180.0f );
		m_flRadiansMax = m_flDegreesMax * ( M_PI / 180.0f );
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		InitScalarAttributeRandomRangeExpBlock(  GetAttributeToInit(),
												 m_flRadiansMin, m_flRadiansMax, m_flRotationRandExponent,
												 pParticles, start_block, n_blocks, m_bRandomlyFlipDirection );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		if ( !m_bRandomlyFlipDirection )
		{
			for( ; nParticleCount--; start_p++ )
			{
				float *pAttr = pParticles->GetFloatAttributePtrForWrite( GetAttributeToInit(), start_p );
				*pAttr = m_flRadians + pParticles->RandomFloatExp( m_flRadiansMin, m_flRadiansMax, m_flRotationRandExponent );
			}
		}
		else
		{
			for( ; nParticleCount--; start_p++ )
			{
				float *pAttr = pParticles->GetFloatAttributePtrForWrite( GetAttributeToInit(), start_p );
				float  flRot = m_flRadians + pParticles->RandomFloatExp( m_flRadiansMin, m_flRadiansMax, m_flRotationRandExponent );
				*pAttr = ( pParticles->RandomFloat( -1.0f, 1.0f ) >= 0.0f ) ? +flRot : -flRot;
			}
		}
	}

	// User-specified range
	float m_flDegreesMin;
	float m_flDegreesMax;
	float m_flDegrees;

	// Converted range
	float m_flRadiansMin;
	float m_flRadiansMax;
	float m_flRadians;
	float m_flRotationRandExponent;
	bool  m_bRandomlyFlipDirection;
};


//-----------------------------------------------------------------------------
// Random rotation
//-----------------------------------------------------------------------------
class C_INIT_RandomRotation : public CGeneralRandomRotation
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomRotation );

	virtual int GetAttributeToInit( void ) const
	{
		return PARTICLE_ATTRIBUTE_ROTATION;
	}
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomRotation, "Rotation Random", OPERATOR_PI_ROTATION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomRotation ) 
	DMXELEMENT_UNPACK_FIELD( "rotation_initial", "0", float, m_flDegrees )
	DMXELEMENT_UNPACK_FIELD( "rotation_offset_min", "0", float, m_flDegreesMin )
	DMXELEMENT_UNPACK_FIELD( "rotation_offset_max", "360", float, m_flDegreesMax )
	DMXELEMENT_UNPACK_FIELD( "rotation_random_exponent", "1", float, m_flRotationRandExponent )
	DMXELEMENT_UNPACK_FIELD( "randomly_flip_direction", "1", bool, m_bRandomlyFlipDirection )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomRotation )


//-----------------------------------------------------------------------------
// Random rotation speed
//-----------------------------------------------------------------------------
class C_INIT_RandomRotationSpeed : public CGeneralRandomRotation
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomRotationSpeed );

	virtual int GetAttributeToInit( void ) const
	{
		return PARTICLE_ATTRIBUTE_ROTATION_SPEED;
	}
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomRotationSpeed, "Rotation Speed Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomRotationSpeed ) 
	DMXELEMENT_UNPACK_FIELD( "rotation_speed_constant", "0", float, m_flDegrees )
	DMXELEMENT_UNPACK_FIELD( "rotation_speed_random_min", "0", float, m_flDegreesMin )
	DMXELEMENT_UNPACK_FIELD( "rotation_speed_random_max", "360", float, m_flDegreesMax )
	DMXELEMENT_UNPACK_FIELD( "rotation_speed_random_exponent", "1", float, m_flRotationRandExponent )
	DMXELEMENT_UNPACK_FIELD( "randomly_flip_direction", "1", bool, m_bRandomlyFlipDirection )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomRotationSpeed )


//-----------------------------------------------------------------------------
// Random yaw
//-----------------------------------------------------------------------------
class C_INIT_RandomYaw : public CGeneralRandomRotation
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomYaw );

	virtual int GetAttributeToInit( void ) const
	{
		return PARTICLE_ATTRIBUTE_YAW;
	}
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomYaw, "Rotation Yaw Random", OPERATOR_PI_YAW );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomYaw ) 
	DMXELEMENT_UNPACK_FIELD( "yaw_initial", "0", float, m_flDegrees )
	DMXELEMENT_UNPACK_FIELD( "yaw_offset_min", "0", float, m_flDegreesMin )
	DMXELEMENT_UNPACK_FIELD( "yaw_offset_max", "360", float, m_flDegreesMax )
	DMXELEMENT_UNPACK_FIELD( "yaw_random_exponent", "1", float, m_flRotationRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomYaw )

static void ComputeLitParticleColor( float *pColorToWrite, const float flBaseColor1[3], const float flBaseColor2[3], float flRandomValue, const Vector &vLightingColor, float flTintFraction, int nBlendMode )
{
	Vector vLightingTint;

	pColorToWrite[0] = flBaseColor1[0] + ( ( flBaseColor2[0] - flBaseColor1[0] ) * flRandomValue );
	pColorToWrite[4] = flBaseColor1[1] + ( ( flBaseColor2[1] - flBaseColor1[1] ) * flRandomValue );
	pColorToWrite[8] = flBaseColor1[2] + ( ( flBaseColor2[2] - flBaseColor1[2] ) * flRandomValue );

	switch ( nBlendMode )
	{
	case PARTICLEBLEND_DEFAULT:
		{
			vLightingTint = vLightingColor;
		}
		break;

	case PARTICLEBLEND_OVERLAY:
		{
			vLightingTint.x = ( vLightingColor.x < .5 ) ? ( 2 * pColorToWrite[0] * vLightingColor.x ) : ( 1 - ( 2 * ( 1 - pColorToWrite[0] ) * ( 1 - vLightingColor.x ) ) );
			vLightingTint.y = ( vLightingColor.y < .5 ) ? ( 2 * pColorToWrite[4] * vLightingColor.y ) : ( 1 - ( 2 * ( 1 - pColorToWrite[4] ) * ( 1 - vLightingColor.y ) ) );
			vLightingTint.z = ( vLightingColor.z < .5 ) ? ( 2 * pColorToWrite[8] * vLightingColor.z ) : ( 1 - ( 2 * ( 1 - pColorToWrite[8] ) * ( 1 - vLightingColor.z ) ) );
		}
		break;

	case PARTICLEBLEND_DARKEN:
		{
			vLightingTint.x = MIN( pColorToWrite[0], vLightingColor.x );
			vLightingTint.y = MIN( pColorToWrite[4], vLightingColor.y );
			vLightingTint.z = MIN( pColorToWrite[8], vLightingColor.z );
		}
		break;

	case PARTICLEBLEND_LIGHTEN:
		{
			vLightingTint.x = MAX( pColorToWrite[0], vLightingColor.x );
			vLightingTint.y = MAX( pColorToWrite[4], vLightingColor.y );
			vLightingTint.z = MAX( pColorToWrite[8], vLightingColor.z );
		}
		break;

	case PARTICLEBLEND_MULTIPLY:
		{
			vLightingTint.x = pColorToWrite[0] * vLightingColor.x;
			vLightingTint.y = pColorToWrite[4] * vLightingColor.y;
			vLightingTint.z = pColorToWrite[8] * vLightingColor.z;
		}
		break;

	}

	pColorToWrite[0] = Lerp( flTintFraction, pColorToWrite[0], vLightingTint.x );
	pColorToWrite[4] = Lerp( flTintFraction, pColorToWrite[4], vLightingTint.y );
	pColorToWrite[8] = Lerp( flTintFraction, pColorToWrite[8], vLightingTint.z );

	pColorToWrite[0] = MIN( pColorToWrite[0], 1.0f );
	pColorToWrite[4] = MIN( pColorToWrite[4], 1.0f );
	pColorToWrite[8] = MIN( pColorToWrite[8], 1.0f );
}

//-----------------------------------------------------------------------------
// Random color
//-----------------------------------------------------------------------------
class C_INIT_RandomColor : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomColor );

	uint32 GetWrittenAttributes( void ) const
	{
		return ( 1 << m_nFieldOutput );
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	struct C_OP_RandomColorContext_t
	{
		Vector m_vPrevPosition;
	};

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( C_OP_RandomColorContext_t );
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		C_OP_RandomColorContext_t *pCtx=reinterpret_cast<C_OP_RandomColorContext_t *>( pContext );
		pCtx->m_vPrevPosition = vec3_origin;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
		m_flNormColorMin[0] = ( float )m_ColorMin[0] / 255.0f;
		m_flNormColorMin[1] = ( float )m_ColorMin[1] / 255.0f;
		m_flNormColorMin[2] = ( float )m_ColorMin[2] / 255.0f;

		m_flNormColorMax[0] = ( float )m_ColorMax[0] / 255.0f;
		m_flNormColorMax[1] = ( float )m_ColorMax[1] / 255.0f;
		m_flNormColorMax[2] = ( float )m_ColorMax[2] / 255.0f;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		C_OP_RandomColorContext_t *pCtx=reinterpret_cast<C_OP_RandomColorContext_t *>( pContext );

		Color	tint( 255, 255, 255, 255 );

		float randomPerc;
		float *pColor;

		// If we're factoring in luminosity or tint, then get our lighting info for this position
		if ( m_flTintPerc )
		{
			if ( pParticles->m_pParent && pParticles->m_pParent->m_LocalLightingCP == m_nTintCP )
			{
				tint = pParticles->m_pParent->m_LocalLighting;
			}
			else
			{
				// Get our control point
				Vector vecOrigin;
				pParticles->GetControlPointAtTime( m_nTintCP, pParticles->m_flCurTime, &vecOrigin );

				if ( ( ( pCtx->m_vPrevPosition - vecOrigin ).Length() >= m_flUpdateThreshold ) || ( pParticles->m_LocalLightingCP == -1 ) )
					{
						g_pParticleSystemMgr->Query()->GetLightingAtPoint( vecOrigin, tint );
						pParticles->m_LocalLighting = tint;
						pParticles->m_LocalLightingCP = m_nTintCP;
						pCtx->m_vPrevPosition = vecOrigin;
					}
				else
					tint = pParticles->m_LocalLighting;

			}
			tint[0] = MAX( m_TintMin[0], MIN( tint[0], m_TintMax[0] ) );
			tint[1] = MAX( m_TintMin[1], MIN( tint[1], m_TintMax[1] ) );
			tint[2] = MAX( m_TintMin[2], MIN( tint[2], m_TintMax[2] ) );

			Vector vecLightingColor = Vector ( tint[0], tint[1], tint[2] );
			vecLightingColor *= m_flLightAmplification / 255.0f;

			for ( ; nParticleCount--; start_p++ )
			{
				pColor = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
				float flRandomFraction = pParticles->RandomFloat( 0.0f, 1.0f );

				ComputeLitParticleColor( pColor, m_flNormColorMin, m_flNormColorMax, flRandomFraction, vecLightingColor, m_flTintPerc, m_nTintBlendMode );
			}
		}
		else // If we don't tint, keep it simple.
		{
			for( ; nParticleCount--; start_p++ )
			{
				// Setup
				pColor = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
				randomPerc = pParticles->RandomFloat( 0.0f, 1.0f );
				// No Blend
				// No Tint
				// Randomly choose a range between the two colors
				pColor[0] = m_flNormColorMin[0] + ( ( m_flNormColorMax[0] - m_flNormColorMin[0] ) * randomPerc );
				pColor[4] = m_flNormColorMin[1] + ( ( m_flNormColorMax[1] - m_flNormColorMin[1] ) * randomPerc );
				pColor[8] = m_flNormColorMin[2] + ( ( m_flNormColorMax[2] - m_flNormColorMin[2] ) * randomPerc );
			}
		}
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		C_OP_RandomColorContext_t *pCtx=reinterpret_cast<C_OP_RandomColorContext_t *>( pContext );

		Color	tint( 255, 255, 255, 255 );

		size_t attr_stride;

		FourVectors *pColor = pParticles->Get4VAttributePtrForWrite( m_nFieldOutput, &attr_stride );
		
		pColor += attr_stride * start_block;
		
		FourVectors fvColorMin;
		fvColorMin.DuplicateVector( Vector (m_flNormColorMin[0], m_flNormColorMin[1], m_flNormColorMin[2] ) );
		FourVectors fvColorWidth;
		fvColorWidth.DuplicateVector( Vector (m_flNormColorMax[0] - m_flNormColorMin[0], m_flNormColorMax[1] - m_flNormColorMin[1], m_flNormColorMax[2] - m_flNormColorMin[2] ) );

		int nRandContext = GetSIMDRandContext();

		// If we're factoring in luminosity or tint, then get our lighting info for this position
		if ( m_flTintPerc )
		{
			if ( pParticles->m_pParent && pParticles->m_pParent->m_LocalLightingCP == m_nTintCP )
			{
				tint = pParticles->m_pParent->m_LocalLighting;
			}
			else
			{
				// FIXME: Really, we want the emission point for each particle, but for now, we do it more cheaply
				// Get our control point
				Vector vecOrigin;
				pParticles->GetControlPointAtTime( m_nTintCP, pParticles->m_flCurTime, &vecOrigin );

				if ( ( ( pCtx->m_vPrevPosition - vecOrigin ).Length() >= m_flUpdateThreshold ) || ( pParticles->m_LocalLightingCP == -1 ) )
				{
					g_pParticleSystemMgr->Query()->GetLightingAtPoint( vecOrigin, tint );
					pParticles->m_LocalLighting = tint;
					pParticles->m_LocalLightingCP = m_nTintCP;
					pCtx->m_vPrevPosition = vecOrigin;
				}
				else
					tint = pParticles->m_LocalLighting;
			}

			tint[0] = MAX( m_TintMin[0], MIN( tint[0], m_TintMax[0] ) );
			tint[1] = MAX( m_TintMin[1], MIN( tint[1], m_TintMax[1] ) );
			tint[2] = MAX( m_TintMin[2], MIN( tint[2], m_TintMax[2] ) );

			FourVectors fvTint;
			fvTint.DuplicateVector( Vector ( tint[0], tint[1], tint[2] ) );
			fltx4 fl4Divisor = ReplicateX4( m_flLightAmplification / 255.0f );
			fvTint *= fl4Divisor;
			fltx4 fl4TintPrc = ReplicateX4( m_flTintPerc );

			// A little ugly, but we keep the switch outside the loop
			switch( m_nTintBlendMode )
			{
			case PARTICLEBLEND_DEFAULT:
				{
					while( n_blocks-- )
					{
						FourVectors fvColor = fvColorWidth;
						FourVectors fvColor2 = fvTint;
						fvColor *= RandSIMD( nRandContext );
						fvColor += fvColorMin;
						//Do Tint
						fvColor2 -= fvColor;
						fvColor2 *= fl4TintPrc;
						fvColor2 += fvColor;
						*pColor = fvColor2;

						// Clamp color components to 1.0
						pColor->x = MinSIMD( pColor->x, Four_Ones );
						pColor->y = MinSIMD( pColor->y, Four_Ones );
						pColor->z = MinSIMD( pColor->z, Four_Ones );
						pColor += attr_stride;
					}
					break;
				}
			case PARTICLEBLEND_OVERLAY:
				{
					while( n_blocks-- )
					{
						FourVectors fvColor = fvColorWidth;
						FourVectors fvColor2 = fvTint;
						fvColor *= RandSIMD( nRandContext );
						fvColor += fvColorMin;
						//Do Blend
						bi32x4 fl4BlendMaskRed = CmpLtSIMD( fvTint.x, Four_PointFives );
						bi32x4 fl4BlendMaskGreen = CmpLtSIMD( fvTint.y, Four_PointFives );
						bi32x4 fl4BlendMaskBlue = CmpLtSIMD( fvTint.z, Four_PointFives );
						FourVectors fvBlend1;
						fvBlend1.x = MulSIMD( Four_Twos, MulSIMD( fvColor.x, fvColor2.x ) );
						fvBlend1.y = MulSIMD( Four_Twos, MulSIMD( fvColor.y, fvColor2.y ) );
						fvBlend1.z = MulSIMD( Four_Twos, MulSIMD( fvColor.z, fvColor2.z ) );
						FourVectors fvBlend2;
						fvBlend2.x = SubSIMD( Four_Ones, MulSIMD( Four_Twos, MulSIMD( SubSIMD( Four_Ones, fvColor.x), SubSIMD( Four_Ones, fvColor2.x ) ) ) );
						fvBlend2.y = SubSIMD( Four_Ones, MulSIMD( Four_Twos, MulSIMD( SubSIMD( Four_Ones, fvColor.y), SubSIMD( Four_Ones, fvColor2.y ) ) ) );
						fvBlend2.z = SubSIMD( Four_Ones, MulSIMD( Four_Twos, MulSIMD( SubSIMD( Four_Ones, fvColor.z), SubSIMD( Four_Ones, fvColor2.z ) ) ) );
						fvColor2 = fvBlend2;
						fvColor2.x = MaskedAssign( fl4BlendMaskRed, fvBlend1.x, fvColor2.x );
						fvColor2.y = MaskedAssign( fl4BlendMaskGreen, fvBlend1.y, fvColor2.y );
						fvColor2.z = MaskedAssign( fl4BlendMaskBlue, fvBlend1.z, fvColor2.z );
						//Do Tint
						fvColor2 -= fvColor;
						fvColor2 *= fl4TintPrc;
						fvColor2 += fvColor;
						*pColor = fvColor2;

						// Clamp color components to 1.0
						pColor->x = MinSIMD( pColor->x, Four_Ones );
						pColor->y = MinSIMD( pColor->y, Four_Ones );
						pColor->z = MinSIMD( pColor->z, Four_Ones );
						pColor += attr_stride;
					}
					break;
				}
			case PARTICLEBLEND_DARKEN:
				{
					while( n_blocks-- )
					{
						FourVectors fvColor = fvColorWidth;
						FourVectors fvColor2 = fvTint;
						fvColor *= RandSIMD( nRandContext );
						fvColor += fvColorMin;
						//Do Blend
						fvColor2.x = MinSIMD( fvColor.x, fvColor2.x );
						fvColor2.y = MinSIMD( fvColor.y, fvColor2.y );
						fvColor2.z = MinSIMD( fvColor.z, fvColor2.z );
						//Do Tint
						fvColor2 -= fvColor;
						fvColor2 *= fl4TintPrc;
						fvColor2 += fvColor;
						*pColor = fvColor2;

						// Clamp color components to 1.0
						pColor->x = MinSIMD( pColor->x, Four_Ones );
						pColor->y = MinSIMD( pColor->y, Four_Ones );
						pColor->z = MinSIMD( pColor->z, Four_Ones );
						pColor += attr_stride;
					}
					break;
				}
			case PARTICLEBLEND_LIGHTEN:
				{
					while( n_blocks-- )
					{
						FourVectors fvColor = fvColorWidth;
						FourVectors fvColor2 = fvTint;
						fvColor *= RandSIMD( nRandContext );
						fvColor += fvColorMin;
						//Do Blend
						fvColor2.x = MaxSIMD( fvColor.x, fvColor2.x );
						fvColor2.y = MaxSIMD( fvColor.y, fvColor2.y );
						fvColor2.z = MaxSIMD( fvColor.z, fvColor2.z );
						//Do Tint
						fvColor2 -= fvColor;
						fvColor2 *= fl4TintPrc;
						fvColor2 += fvColor;
						*pColor = fvColor2;

						// Clamp color components to 1.0
						pColor->x = MinSIMD( pColor->x, Four_Ones );
						pColor->y = MinSIMD( pColor->y, Four_Ones );
						pColor->z = MinSIMD( pColor->z, Four_Ones );
						pColor += attr_stride;
					}
					break;
				}

			case PARTICLEBLEND_MULTIPLY:
				{
					while( n_blocks-- )
					{
						FourVectors fvColor = fvColorWidth;
						FourVectors fvColor2 = fvTint;
						fvColor *= RandSIMD( nRandContext );
						fvColor += fvColorMin;
						//Do Modulate
						fvColor2 *= fvColor;
						//Do Tint
						fvColor2 -= fvColor;
						fvColor2 *= fl4TintPrc;
						fvColor2 += fvColor;
						*pColor = fvColor2;

						// Clamp color components to 1.0
						pColor->x = MinSIMD( pColor->x, Four_Ones );
						pColor->y = MinSIMD( pColor->y, Four_Ones );
						pColor->z = MinSIMD( pColor->z, Four_Ones );
						pColor += attr_stride;
					}
					break;
				}
			}
		}
		else
		{
			while( n_blocks-- )
			{
				FourVectors fvColor = fvColorWidth;
				fvColor *= RandSIMD( nRandContext );
				fvColor += fvColorMin;
				*pColor = fvColor;
				pColor += attr_stride;
			}
		}
		ReleaseSIMDRandContext( nRandContext );
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nTintCP;
	}

	float	m_flNormColorMin[3];
	float	m_flNormColorMax[3];
	Color	m_ColorMin;
	Color	m_ColorMax;
	Color	m_TintMin;
	Color	m_TintMax;
	float	m_flTintPerc;
	float	m_flUpdateThreshold;
	int		m_nTintCP;
	int		m_nFieldOutput;
	int		m_nTintBlendMode;
	float	m_flLightAmplification;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomColor, "Color Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomColor ) 
	DMXELEMENT_UNPACK_FIELD( "color1", "255 255 255 255", Color, m_ColorMin )
	DMXELEMENT_UNPACK_FIELD( "color2", "255 255 255 255", Color, m_ColorMax )
	DMXELEMENT_UNPACK_FIELD( "tint_perc", "0.0", float, m_flTintPerc )
	DMXELEMENT_UNPACK_FIELD( "tint control point", "0", int, m_nTintCP )
	DMXELEMENT_UNPACK_FIELD( "tint clamp min", "0 0 0 0", Color, m_TintMin )
	DMXELEMENT_UNPACK_FIELD( "tint clamp max", "255 255 255 255", Color, m_TintMax )
	DMXELEMENT_UNPACK_FIELD( "tint update movement threshold", "32", float, m_flUpdateThreshold )
	DMXELEMENT_UNPACK_FIELD( "tint blend mode", "0", int, m_nTintBlendMode )
	DMXELEMENT_UNPACK_FIELD( "light amplification amount", "1", float, m_flLightAmplification )
    DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "6", int, m_nFieldOutput, "intchoice particlefield_vector" )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomColor )



//-----------------------------------------------------------------------------
// Random color
//-----------------------------------------------------------------------------
class C_INIT_ColorLitPerParticle : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_ColorLitPerParticle );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_TINT_RGB_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
		m_flNormColorMin[0] = ( float )m_ColorMin[0] / 255.0f;
		m_flNormColorMin[1] = ( float )m_ColorMin[1] / 255.0f;
		m_flNormColorMin[2] = ( float )m_ColorMin[2] / 255.0f;

		m_flNormColorMax[0] = ( float )m_ColorMax[0] / 255.0f;
		m_flNormColorMax[1] = ( float )m_ColorMax[1] / 255.0f;
		m_flNormColorMax[2] = ( float )m_ColorMax[2] / 255.0f;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		Color	tint( 255, 255, 255, 255 );
		float *pColor;
		float *pXYZ;

		for( ; nParticleCount--; start_p++ )
		{
			pColor = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_TINT_RGB, start_p );
			pXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );

			Vector vecOrigin;
			SetVectorFromAttribute( vecOrigin, pXYZ );
			g_pParticleSystemMgr->Query()->GetLightingAtPoint( vecOrigin, tint );

			tint[0] = MAX( m_TintMin[0], MIN( tint[0], m_TintMax[0] ) );
			tint[1] = MAX( m_TintMin[1], MIN( tint[1], m_TintMax[1] ) );
			tint[2] = MAX( m_TintMin[2], MIN( tint[2], m_TintMax[2] ) );

			Vector vecLightingColor = Vector ( tint[0], tint[1], tint[2] );
			vecLightingColor *= m_flLightAmplification / 255.0f;

			float flRandomFraction = pParticles->RandomFloat( 0.0f, 1.0f );

			ComputeLitParticleColor( pColor, m_flNormColorMin, m_flNormColorMax, flRandomFraction, vecLightingColor, m_flTintPerc, m_nTintBlendMode );
		}
	}

	float	m_flNormColorMin[3];
	float	m_flNormColorMax[3];
	Color	m_ColorMin;
	Color	m_ColorMax;
	Color	m_TintMin;
	Color	m_TintMax;
	float	m_flTintPerc;
	int		m_nTintBlendMode;
	float	m_flLightAmplification;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_ColorLitPerParticle, "Color Lit Per Particle", OPERATOR_PI_TINT_RGB );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_ColorLitPerParticle ) 
	DMXELEMENT_UNPACK_FIELD( "color1", "255 255 255 255", Color, m_ColorMin )
	DMXELEMENT_UNPACK_FIELD( "color2", "255 255 255 255", Color, m_ColorMax )
	DMXELEMENT_UNPACK_FIELD( "light bias", "0.0", float, m_flTintPerc )
	DMXELEMENT_UNPACK_FIELD( "tint clamp min", "0 0 0 0", Color, m_TintMin )
	DMXELEMENT_UNPACK_FIELD( "tint clamp max", "255 255 255 255", Color, m_TintMax )
	DMXELEMENT_UNPACK_FIELD( "tint blend mode", "0", int, m_nTintBlendMode )
	DMXELEMENT_UNPACK_FIELD( "light amplification amount", "1", float, m_flLightAmplification )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_ColorLitPerParticle )


//-----------------------------------------------------------------------------
// Trail Length
//-----------------------------------------------------------------------------
class C_INIT_RandomTrailLength : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomTrailLength );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_TRAIL_LENGTH_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
	}

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		InitScalarAttributeRandomRangeExpBlock( PARTICLE_ATTRIBUTE_TRAIL_LENGTH,
												m_flMinLength, m_flMaxLength, m_flLengthRandExponent,
												pParticles, start_block, n_blocks );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		InitScalarAttributeRandomRangeExpScalar( PARTICLE_ATTRIBUTE_TRAIL_LENGTH,
												 m_flMinLength, m_flMaxLength, m_flLengthRandExponent,
												 pParticles, start_p, nParticleCount );
	}

	float m_flMinLength;
	float m_flMaxLength;
	float m_flLengthRandExponent;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomTrailLength, "Trail Length Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomTrailLength ) 
	DMXELEMENT_UNPACK_FIELD( "length_min", "0.1", float, m_flMinLength )
	DMXELEMENT_UNPACK_FIELD( "length_max", "0.1", float, m_flMaxLength )
	DMXELEMENT_UNPACK_FIELD( "length_random_exponent", "1", float, m_flLengthRandExponent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomTrailLength )

//-----------------------------------------------------------------------------
// Random sequence
//-----------------------------------------------------------------------------
class C_INIT_RandomSequence : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomSequence );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
	}


	struct SequenceContext_t
	{
		int nCurrent;
		int nList[64];
	};

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		if ( m_bShuffle || m_bLinear )
		{
			SequenceContext_t *pCtx = reinterpret_cast<SequenceContext_t *>( pContext );
			pCtx->nCurrent = m_nSequenceMin;
			for ( int i = m_nSequenceMin; i <= m_nSequenceMax; i++ )
			{
				pCtx->nList[i] = i;
			}
			if ( m_bShuffle )
			{
				for ( int i = m_nSequenceMax; i > m_nSequenceMin; i-- )
				{
					int nShuffle = pParticles->RandomInt( m_nSequenceMin, i );
					int nTemp = pCtx->nList[i];
					pCtx->nList[i] = pCtx->nList[nShuffle];
					pCtx->nList[nShuffle] = nTemp;
				}
			}
		}
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( SequenceContext_t );
	}
/*
	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
	{
		InitScalarAttributeRandomRangeBlock( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER,
			m_nSequenceMin, m_nSequenceMax,
			pParticles, start_block, n_blocks );
	}
*/
	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		float *pSequence;
		if ( m_bShuffle || m_bLinear )
		{
			SequenceContext_t *pCtx = reinterpret_cast<SequenceContext_t *>( pContext );
			for( ; nParticleCount--; start_p++ )
			{
				if ( pCtx->nCurrent > m_nSequenceMax )
				{
					if ( m_bShuffle )
					{
						for ( int i = m_nSequenceMax; i > m_nSequenceMin; i-- )
						{
							int nShuffle = pParticles->RandomInt( m_nSequenceMin, i );
							int nTemp = pCtx->nList[i];
							pCtx->nList[i] = pCtx->nList[nShuffle];
							pCtx->nList[nShuffle] = nTemp;
						}
					}
					pCtx->nCurrent = m_nSequenceMin;
				}
				pSequence = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, start_p );
				*pSequence = pCtx->nList[pCtx->nCurrent];
				pCtx->nCurrent++;
			}

		}
		else
		{
			for( ; nParticleCount--; start_p++ )
			{
				pSequence = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, start_p );
				*pSequence = pParticles->RandomInt( m_nSequenceMin, m_nSequenceMax );
			}
		}
	}

	int m_nSequenceMin;
	int m_nSequenceMax;
	bool m_bShuffle;
	bool m_bLinear;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomSequence, "Sequence Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomSequence ) 
	DMXELEMENT_UNPACK_FIELD_USERDATA( "sequence_min", "0", int, m_nSequenceMin, "sheetsequencepicker" )
	DMXELEMENT_UNPACK_FIELD_USERDATA( "sequence_max", "0", int, m_nSequenceMax, "sheetsequencepicker" )
	DMXELEMENT_UNPACK_FIELD( "shuffle", "0", bool, m_bShuffle )
	DMXELEMENT_UNPACK_FIELD( "linear", "0", bool, m_bLinear )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomSequence )

 



//-----------------------------------------------------------------------------
// Random sequence
//-----------------------------------------------------------------------------
class C_INIT_SequenceFromCP : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_SequenceFromCP );

	bool m_bKillUnused;
	bool m_bRadiusScale;
	int m_nCP;
	Vector m_vecOffset;

	uint32 GetWrittenAttributes( void ) const
	{
		int ret = PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK;
		if ( m_vecOffset != vec3_origin )
			ret |= PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
		if ( m_bKillUnused == true )
			ret |= PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
		return ret;
	}

	uint32 GetReadAttributes( void ) const
	{
		if ( m_vecOffset != vec3_origin )
			return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK;
		else
			return 0;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nCP;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_SequenceFromCP, "Sequence From Control Point", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_SequenceFromCP ) 
	DMXELEMENT_UNPACK_FIELD( "control point", "1", int, m_nCP )
	DMXELEMENT_UNPACK_FIELD( "per particle spatial offset", "0 0 0", Vector, m_vecOffset )
	DMXELEMENT_UNPACK_FIELD( "offset propotional to radius", "0", bool, m_bRadiusScale )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_SequenceFromCP )

void C_INIT_SequenceFromCP::InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	float *pSequence;
	Vector vecCP = pParticles->GetControlPointAtCurrentTime( m_nCP );
	int nComponent = 2;
	int nDesiredCount = MAX ( 0, int ( log10( vecCP.x * 10 ) ) );
	nDesiredCount += MAX ( 0, int ( log10( vecCP.y * 10 ) ) );
	nDesiredCount += MAX ( 0, int ( log10( vecCP.z * 10 ) ) );
	nDesiredCount = MIN( nDesiredCount, nParticleCount ) - 1;
	Vector vecOffsetPos = ( m_vecOffset * nDesiredCount ) / 2;

	for( ; nParticleCount--; start_p++ )
	{
		while ( vecCP[nComponent] < 1 && nComponent >= 0 )
			nComponent--;
		if ( vecCP[nComponent] > 0 && nComponent >= 0 )
		{
			pSequence = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, start_p );
			*pSequence = fmod( vecCP[nComponent], 10 ) + ( 10 * nComponent );
			vecCP[nComponent] = floor( vecCP[nComponent] / 10 );
			MAX ( vecCP[nComponent], 0 );

			if ( m_vecOffset != vec3_origin )
			{	
				float flRadius = 1;
				float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
				float *xyzprev = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
				if ( m_bRadiusScale )
				{
					const float *rad = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, start_p );
					flRadius = *rad;
				}
				Vector vecXYZ, vecXYZPrev;
				SetVectorFromAttribute( vecXYZ, xyz );
				SetVectorFromAttribute( vecXYZPrev, xyzprev );
				vecXYZ += vecOffsetPos * flRadius;
				vecXYZPrev += vecOffsetPos * flRadius;
				SetVectorAttribute( xyz, vecXYZ );
				SetVectorAttribute( xyzprev, vecXYZPrev);
				vecOffsetPos -= m_vecOffset;
			}
		}
		else
		{
			float *pLifeDuration = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
			*pLifeDuration = -1;
		}
	}
};





//-----------------------------------------------------------------------------
// Position Warp Initializer
// Scales initial position and velocity of particles within a random vector range
//-----------------------------------------------------------------------------
class C_INIT_PositionWarp : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_PositionOffset );

	Vector m_vecWarpMin;
	Vector m_vecWarpMax;
	int m_nControlPointNumber;
	float m_flWarpTime, m_flWarpStartTime;
	bool m_bInvertWarp;
	bool m_bUseCount;

	struct PositionWarpContext_t
	{
		float m_flStartTime;
		int m_nStartCount;
		unsigned int m_nCurrentCount;
	};

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		PositionWarpContext_t *pCtx = reinterpret_cast<PositionWarpContext_t *>( pContext );
		pCtx->m_flStartTime = pParticles->m_flCurTime;
		pCtx->m_nStartCount = 0;
		pCtx->m_nCurrentCount = 0;
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( PositionWarpContext_t );
	}

	virtual void Restart( CParticleCollection *pParticles, void *pContext )
	{
		PositionWarpContext_t *pCtx = reinterpret_cast<PositionWarpContext_t *>( pContext );
		pCtx->m_flStartTime = pParticles->m_flCurTime;
		pCtx->m_nStartCount = 0;
		pCtx->m_nCurrentCount = 0;
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }

};

DEFINE_PARTICLE_OPERATOR( C_INIT_PositionWarp, "Position Modify Warp Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_PositionWarp ) 
	DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "warp min", "1 1 1", Vector, m_vecWarpMin )
	DMXELEMENT_UNPACK_FIELD( "warp max", "1 1 1", Vector, m_vecWarpMax )
	DMXELEMENT_UNPACK_FIELD( "warp transition time (treats min/max as start/end sizes)", "0", float , m_flWarpTime )
	DMXELEMENT_UNPACK_FIELD( "warp transition start time", "0", float , m_flWarpStartTime )
	DMXELEMENT_UNPACK_FIELD( "reverse warp (0/1)", "0", bool , m_bInvertWarp )
	DMXELEMENT_UNPACK_FIELD( "use particle count instead of time", "0", bool , m_bUseCount )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_PositionWarp )


void C_INIT_PositionWarp::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	PositionWarpContext_t *pCtx = reinterpret_cast<PositionWarpContext_t *>( pContext );
	Vector vecWarpStart = m_vecWarpMin;
	Vector vecWarpEnd = m_vecWarpMax;

	if ( m_bInvertWarp )
	{
		vecWarpStart = m_vecWarpMax;
		vecWarpEnd = m_vecWarpMin;
	}

	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		
		Vector randpos;
		
		if ( m_flWarpTime != 0.0f )
		{ 
			if ( m_bUseCount )
			{
				float flWarpEnd = m_flWarpStartTime + m_flWarpTime + pCtx->m_nStartCount;
				float flPercentage = RemapValClamped( pCtx->m_nCurrentCount, m_flWarpStartTime + pCtx->m_nStartCount, flWarpEnd, 0.0, 1.0 );
				VectorLerp( vecWarpStart, vecWarpEnd, flPercentage, randpos );
				pCtx->m_nCurrentCount++;
			}
			else
			{
				float flWarpEnd = m_flWarpStartTime + m_flWarpTime + pCtx->m_flStartTime;
				float flPercentage = RemapValClamped( *ct, m_flWarpStartTime + pCtx->m_flStartTime, flWarpEnd, 0.0, 1.0 );
				VectorLerp( vecWarpStart, vecWarpEnd, flPercentage, randpos );
			}
		}
		else
		{
			pParticles->RandomVector( m_vecWarpMin, m_vecWarpMax, &randpos );
		}


		matrix3x4_t mat;
		pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *ct, &mat );
		Vector vecTransformLocal = vec3_origin;
		Vector vecParticlePosition, vecParticlePosition_prev ;
		SetVectorFromAttribute( vecParticlePosition, xyz ); 
		SetVectorFromAttribute( vecParticlePosition_prev, pxyz );
		// rotate particles from world space into local
		VectorITransform( vecParticlePosition, mat, vecTransformLocal );
		// multiply position by desired amount
		vecTransformLocal.x *= randpos.x;
		vecTransformLocal.y *= randpos.y;
		vecTransformLocal.z *= randpos.z;
		// rotate back into world space
		VectorTransform( vecTransformLocal, mat, vecParticlePosition );
		// rinse, repeat
		VectorITransform( vecParticlePosition_prev, mat, vecTransformLocal ); 
		vecTransformLocal.x *= randpos.x;
		vecTransformLocal.y *= randpos.y;
		vecTransformLocal.z *= randpos.z;
		VectorTransform( vecTransformLocal, mat, vecParticlePosition_prev );
		// set positions into floats
		SetVectorAttribute( xyz, vecParticlePosition ); 
		SetVectorAttribute( pxyz, vecParticlePosition_prev ); 
	}
}


//-----------------------------------------------------------------------------
// noise initializer
//-----------------------------------------------------------------------------
class C_INIT_CreationNoise : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreationNoise );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

	void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const;

	virtual bool IsScrubSafe() { return true; }
	int		m_nFieldOutput;
	bool	m_bAbsVal, m_bAbsValInv;
	float	m_flOffset;
	float	m_flOutputMin;
	float	m_flOutputMax;
	float	m_flNoiseScale, m_flNoiseScaleLoc;
	Vector  m_vecOffsetLoc;
	float   m_flWorldTimeScale;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreationNoise, "Remap Noise to Scalar", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreationNoise )
	DMXELEMENT_UNPACK_FIELD( "time noise coordinate scale","0.1",float,m_flNoiseScale)
	DMXELEMENT_UNPACK_FIELD( "spatial noise coordinate scale","0.001",float,m_flNoiseScaleLoc)
	DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
	DMXELEMENT_UNPACK_FIELD( "time coordinate offset","0", float, m_flOffset )
	DMXELEMENT_UNPACK_FIELD( "spatial coordinate offset","0 0 0", Vector, m_vecOffsetLoc )
	DMXELEMENT_UNPACK_FIELD( "absolute value","0", bool, m_bAbsVal )
	DMXELEMENT_UNPACK_FIELD( "invert absolute value","0", bool, m_bAbsValInv )
	DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
	DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
	DMXELEMENT_UNPACK_FIELD( "world time noise coordinate scale","0", float, m_flWorldTimeScale )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreationNoise );




void C_INIT_CreationNoise::InitNewParticlesBlock( CParticleCollection *pParticles, 
												 int start_block, int n_blocks, int nAttributeWriteMask,
												 void *pContext ) const
{
	float		flAbsScale;
	fltx4 		fl4AbsVal;
	fl4AbsVal = ( fltx4 ) CmpEqSIMD( Four_Zeros, Four_Zeros ); 
	flAbsScale = 0.5;

	// Set up values for more optimal absolute value calculations inside the loop
	if ( m_bAbsVal )
	{
		fl4AbsVal = LoadAlignedSIMD( (float *) g_SIMD_clear_signmask );
		flAbsScale = 1.0;
	}

	float fMin = m_flOutputMin;
	float fMax = m_flOutputMax;	

	if ( ATTRIBUTES_WHICH_ARE_ANGLES & (1 << m_nFieldOutput ) )
	{
		fMin *= ( M_PI / 180.0f );
		fMax *= ( M_PI / 180.0f );
	}	

	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

	float ValueScale, ValueBase;
	ValueScale = ( flAbsScale *( fMax - fMin ) );
	ValueBase = ( fMin+ ( ( 1.0 - flAbsScale ) *( fMax - fMin ) ) );

	fltx4 fl4ValueBase = ReplicateX4( ValueBase );
	fltx4 fl4ValueScale = ReplicateX4( ValueScale );

	size_t attr_stride;

	fltx4 *pAttr = pParticles->GetM128AttributePtrForWrite( m_nFieldOutput, &attr_stride );
	pAttr += attr_stride * start_block;
	const FourVectors *pxyz = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &attr_stride );
	pxyz += attr_stride * start_block;
	const fltx4 *pCreationTime = pParticles->GetM128AttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, &attr_stride );
	pCreationTime += attr_stride * start_block;

	//setup
	fltx4 fl4Offset = ReplicateX4( m_flOffset );
	FourVectors fvOffsetLoc;
	fvOffsetLoc.DuplicateVector( m_vecOffsetLoc );
	FourVectors fvCoordBase;
	fvCoordBase.x = AddSIMD(*pCreationTime, fl4Offset);
	fvCoordBase.y = AddSIMD(*pCreationTime, fl4Offset);
	fvCoordBase.z = AddSIMD(*pCreationTime, fl4Offset);
	fvCoordBase *= CoordScale;

	while( n_blocks-- )
	{	
		FourVectors fvCoordLoc = *pxyz;
		fvCoordLoc += fvOffsetLoc;
		FourVectors fvCoord = fvCoordBase;
		fvCoordLoc *= CoordScaleLoc;
		fvCoord += fvCoordLoc;

		fltx4 fl4Noise;

		fl4Noise = NoiseSIMD( fvCoord );

		fl4Noise = AndSIMD ( fl4Noise, fl4AbsVal );

		if ( m_bAbsValInv )
		{
			fl4Noise = SubSIMD( Four_Ones, fl4Noise );
		}

		fltx4 fl4InitialNoise;

		fl4InitialNoise = AddSIMD( fl4ValueBase, ( MulSIMD( fl4ValueScale, fl4Noise ) ) );

		if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & (1 << m_nFieldOutput ) )
		{
			fl4InitialNoise = MinSIMD( Four_Ones, fl4InitialNoise );
			fl4InitialNoise = MaxSIMD( Four_Zeros, fl4InitialNoise );
		}

		*( pAttr ) = fl4InitialNoise;

		pAttr += attr_stride;
		pxyz += attr_stride;

	}
}



void C_INIT_CreationNoise::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	float	flAbsScale;
	int		nAbsVal;
	nAbsVal = 0xffffffff; 
	flAbsScale = 0.5;
	if ( m_bAbsVal )
	{
		nAbsVal = 0x7fffffff;
		flAbsScale = 1.0;
	}

	float fMin = m_flOutputMin;
	float fMax = m_flOutputMax;

	if ( ATTRIBUTES_WHICH_ARE_ANGLES & (1 << m_nFieldOutput ) )
	{
		fMin *= ( M_PI / 180.0f );
		fMax *= ( M_PI / 180.0f );
	}

	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

    float ValueScale, ValueBase;
	ValueScale = ( flAbsScale *( fMax - fMin ) );
	ValueBase = ( fMin+ ( ( 1.0 - flAbsScale ) *( fMax - fMin ) ) );
	
	Vector CoordLoc, CoordWorldTime, CoordBase;
	const float *pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
	float Offset = m_flOffset;
	CoordBase = Vector ( (*pCreationTime + Offset), (*pCreationTime + Offset), (*pCreationTime + Offset) );
	CoordBase *= CoordScale;
	CoordWorldTime = Vector( (Plat_MSTime() * m_flWorldTimeScale), (Plat_MSTime() * m_flWorldTimeScale), (Plat_MSTime() * m_flWorldTimeScale) );
	CoordBase += CoordWorldTime;

	for( ; nParticleCount--; start_p++ )
	{	
		const float *pxyz = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pAttr = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );	

		Vector Coord = CoordBase;

		CoordLoc.x = pxyz[0]; 
		CoordLoc.y = pxyz[4];
		CoordLoc.z = pxyz[8];
		CoordLoc += m_vecOffsetLoc;

		CoordLoc *= CoordScaleLoc;
		Coord += CoordLoc;

		fltx4 flNoise128;
		FourVectors fvNoise;

		fvNoise.DuplicateVector( Coord );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoise = SubFloat( flNoise128, 0 );

		*( (int *) &flNoise)  &= nAbsVal;

		if ( m_bAbsValInv )
		{
			flNoise = 1.0 - flNoise;
		}
		    
		float flInitialNoise = ( ValueBase + ( ValueScale * flNoise ) );

		if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & (1 << m_nFieldOutput ) )
		{
			flInitialNoise = clamp(flInitialNoise, 0.0f, 1.0f );
		}

		*( pAttr ) = flInitialNoise;
	}
}






class C_INIT_CreateAlongPath : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateAlongPath );

	float m_fMaxDistance;
	struct CPathParameters m_PathParams;
	bool m_bUseRandomCPs;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nStartMask = ( 1ULL << m_PathParams.m_nStartControlPointNumber ) - 1;
		uint64 nEndMask = ( 1ULL << ( m_PathParams.m_nEndControlPointNumber + 1 ) ) - 1;
		return nEndMask & (~nStartMask);
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_PathParams.ClampControlPointIndices();
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateAlongPath, "Position Along Path Random", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreateAlongPath ) 
	DMXELEMENT_UNPACK_FIELD( "maximum distance", "0", float, m_fMaxDistance )
	DMXELEMENT_UNPACK_FIELD( "bulge", "0", float, m_PathParams.m_flBulge )
	DMXELEMENT_UNPACK_FIELD( "start control point number", "0", int, m_PathParams.m_nStartControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "end control point number", "0", int, m_PathParams.m_nEndControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "bulge control 0=random 1=orientation of start pnt 2=orientation of end point", "0", int, m_PathParams.m_nBulgeControl )
	DMXELEMENT_UNPACK_FIELD( "mid point position", "0.5", float, m_PathParams.m_flMidPoint )
	DMXELEMENT_UNPACK_FIELD( "randomly select sequential CP pairs between start and end points", "0", bool, m_bUseRandomCPs )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateAlongPath )


void C_INIT_CreateAlongPath::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		struct CPathParameters PathParams = m_PathParams;
		
		if ( m_bUseRandomCPs )
		{
			PathParams.m_nEndControlPointNumber = pParticles->RandomInt( PathParams.m_nStartControlPointNumber + 1, PathParams.m_nEndControlPointNumber );
			PathParams.m_nStartControlPointNumber = PathParams.m_nEndControlPointNumber - 1 ;
		}

		Vector StartPnt, MidP, EndPnt;
		pParticles->CalculatePathValues( PathParams, *ct, &StartPnt, &MidP, &EndPnt);

		float t=pParticles->RandomFloat( 0.0, 1.0 );
		
		Vector randpos;
		pParticles->RandomVector( -m_fMaxDistance, m_fMaxDistance, &randpos );

		// form delta terms needed for quadratic bezier
		Vector Delta0=MidP-StartPnt;
		Vector Delta1 = EndPnt-MidP;

		Vector L0 = StartPnt+t*Delta0;
		Vector L1 = MidP+t*Delta1;

		Vector Pnt = L0+(L1-L0)*t;

		Pnt+=randpos;

		xyz[0] = Pnt.x;
		xyz[4] = Pnt.y;
		xyz[8] = Pnt.z;
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			pxyz[0] = Pnt.x;
			pxyz[4] = Pnt.y;
			pxyz[8] = Pnt.z;
		}
	}
}





class C_INIT_MoveBetweenPoints : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_MoveBetweenPoints );

	float m_flSpeedMin, m_flSpeedMax;
	float m_flEndSpread;
	float m_flStartOffset;
	float m_flEndOffset;
	int m_nEndControlPointNumber;
	bool m_bTrailBias;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_TRAIL_LENGTH_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nEndControlPointNumber;
	}

	bool InitMultipleOverride ( void ) 
	{ 
		if ( m_bTrailBias )
			return true; 
		else
			return false;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_MoveBetweenPoints, "Move Particles Between 2 Control Points", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_MoveBetweenPoints ) 
	DMXELEMENT_UNPACK_FIELD( "minimum speed", "1", float, m_flSpeedMin )
	DMXELEMENT_UNPACK_FIELD( "maximum speed", "1", float, m_flSpeedMax )
	DMXELEMENT_UNPACK_FIELD( "end spread", "0", float, m_flEndSpread )
	DMXELEMENT_UNPACK_FIELD( "start offset", "0", float, m_flStartOffset )
	DMXELEMENT_UNPACK_FIELD( "end offset", "0", float, m_flEndOffset )
	DMXELEMENT_UNPACK_FIELD( "bias lifetime by trail length", "0", bool, m_bTrailBias )
	DMXELEMENT_UNPACK_FIELD( "end control point", "1", int, m_nEndControlPointNumber )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_MoveBetweenPoints )


void C_INIT_MoveBetweenPoints::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	bool bMoveStartPoint = ( m_flStartOffset != 0.0 );
	bool bMoveEndPoint = ( m_flEndOffset != 0.0 );
	for( ; nParticleCount--; start_p++ )
	{
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pPrevXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );

		float *dtime = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
		
		Vector StartPnt( pxyz[0], pxyz[4], pxyz[8] );

		Vector vecEndPoint;

		pParticles->GetControlPointAtTime( m_nEndControlPointNumber, *ct, &vecEndPoint );

		Vector randpos(0,0,0);

		if ( m_flEndSpread > 0.0 )
		{
			pParticles->RandomVectorInUnitSphere( &randpos );
			randpos *= m_flEndSpread;
		}
		
		vecEndPoint += randpos;

		Vector vDelta = vecEndPoint - StartPnt;
		float flLen = VectorLength( vDelta );

		if ( bMoveEndPoint )
		{
			vecEndPoint += ( m_flEndOffset / ( flLen + FLT_EPSILON ) ) * vDelta;
			vDelta = vecEndPoint - StartPnt;			
			flLen = VectorLength( vDelta );
		}

		if ( bMoveStartPoint )
		{
			StartPnt += ( m_flStartOffset/ ( flLen + FLT_EPSILON ) ) * vDelta;
			vDelta = vecEndPoint - StartPnt;			
			flLen = VectorLength( vDelta );
		}

		float flVel = pParticles->RandomFloat( m_flSpeedMin, m_flSpeedMax );

		if ( m_bTrailBias )
		{
			const float *pTrailLength = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_TRAIL_LENGTH, start_p );
			*dtime = ( flLen / ( flVel + FLT_EPSILON ) ) + *pTrailLength;
		}
		else
		{
			*dtime = flLen / ( flVel + FLT_EPSILON );
		}

		Vector poffset = vDelta * ( flVel / ( flLen + FLT_EPSILON ) );

		poffset *= pParticles->m_flPreviousDt;

		if ( bMoveStartPoint )
		{
			pxyz[0] = StartPnt.x;
			pxyz[1] = StartPnt.y;
			pxyz[2] = StartPnt.z;
		}

		pPrevXYZ[0] = pxyz[0] - poffset.x;
		pPrevXYZ[4] = pxyz[4] - poffset.y;
		pPrevXYZ[8] = pxyz[8] - poffset.z;
	}
}




//-----------------------------------------------------------------------------
// Remap Scalar Initializer
//-----------------------------------------------------------------------------
class C_INIT_RemapScalar : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapScalar );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 1 << m_nFieldInput;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		// clamp the result to 0 and 1 if it's alpha
		if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & ( 1 << m_nFieldOutput ) )
		{
			m_flOutputMin = clamp(m_flOutputMin, 0.0f, 1.0f );
			m_flOutputMax = clamp(m_flOutputMax, 0.0f, 1.0f );
		}
	}

	bool InitMultipleOverride ( void ) { return true; }
	
	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nFieldInput;
	int		m_nFieldOutput;
	float	m_flInputMin;
	float	m_flInputMax;
	float	m_flOutputMin;
	float	m_flOutputMax;
	float	m_flStartTime;
	float	m_flEndTime;
	bool	m_bScaleInitialRange;
	bool	m_bActiveRange;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapScalar, "Remap Initial Scalar", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RemapScalar )
	DMXELEMENT_UNPACK_FIELD( "emitter lifetime start time (seconds)", "-1", float, m_flStartTime )
	DMXELEMENT_UNPACK_FIELD( "emitter lifetime end time (seconds)", "-1", float, m_flEndTime )
	DMXELEMENT_UNPACK_FIELD_USERDATA( "input field", "8", int, m_nFieldInput, "intchoice particlefield_scalar" )
	DMXELEMENT_UNPACK_FIELD( "input minimum","0", float, m_flInputMin )
	DMXELEMENT_UNPACK_FIELD( "input maximum","1", float, m_flInputMax )
	DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
	DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
	DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
	DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
	DMXELEMENT_UNPACK_FIELD( "only active within specified input range","0", bool, m_bActiveRange )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapScalar )

void C_INIT_RemapScalar::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	const float *pCreationTime;
	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		// using raw creation time to map to emitter lifespan
		float flLifeTime = *pCreationTime;  

		float flInput;
		if ( ATTRIBUTES_WHICH_ARE_INTS & ( 1 << m_nFieldInput ) )
		{
			const int *pInput = pParticles->GetIntAttributePtr( m_nFieldInput, start_p );
			flInput = float( *pInput );
		}
		else
		{
			const float *pInput = pParticles->GetFloatAttributePtr( m_nFieldInput, start_p );
			flInput = *pInput;
		}

		// only use within start/end time frame and, if set, active input range
		if ( ( ( ( flLifeTime < m_flStartTime ) || ( flLifeTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) ) || ( m_bActiveRange && ( flInput < m_flInputMin || flInput > m_flInputMax ) ) )
			continue;

		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		float flOutput = RemapValClamped( flInput, m_flInputMin, m_flInputMax, m_flOutputMin, m_flOutputMax  );
		if ( m_bScaleInitialRange )
		{
			float flInitialValue = *pOutput;
			flOutput *= flInitialValue;
		}
		if ( ATTRIBUTES_WHICH_ARE_INTS & ( 1 << m_nFieldOutput ) )
		{
			*pOutput = int ( flOutput );
		}
		else
		{
			*pOutput = flOutput;
		}
	}
}


//-----------------------------------------------------------------------------
// Remap Particle Count to Scalar Initializer
//-----------------------------------------------------------------------------
struct RemapCountContext_t
{
	int		m_nCurrentParticleCount;
};
class C_INIT_RemapParticleCountToScalar : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapParticleCountToScalar );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		RemapCountContext_t *pCtx = reinterpret_cast<RemapCountContext_t *>( pContext );
		pCtx->m_nCurrentParticleCount = 0;
	}

	virtual void Restart( CParticleCollection *pParticles, void *pContext )
	{
		RemapCountContext_t *pCtx = reinterpret_cast<RemapCountContext_t *>( pContext );
		pCtx->m_nCurrentParticleCount = 0;
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( RemapCountContext_t );
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		// clamp the result to 0 and 1 if it's alpha
		if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & ( 1 << m_nFieldOutput ) )
		{
			m_flOutputMin = clamp(m_flOutputMin, 0.0f, 1.0f );
			m_flOutputMax = clamp(m_flOutputMax, 0.0f, 1.0f );
		}
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nFieldInput;
	int		m_nFieldOutput;
	int		m_nInputMin;
	int		m_nInputMax;
	float	m_flOutputMin;
	float	m_flOutputMax;
	bool	m_bScaleInitialRange;
	bool	m_bActiveRange;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapParticleCountToScalar, "Remap Particle Count to Scalar", OPERATOR_GENERIC );
BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RemapParticleCountToScalar )
DMXELEMENT_UNPACK_FIELD( "input minimum","0", int, m_nInputMin )
DMXELEMENT_UNPACK_FIELD( "input maximum","10", int, m_nInputMax )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
DMXELEMENT_UNPACK_FIELD( "only active within specified input range","0", bool, m_bActiveRange )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapParticleCountToScalar )

void C_INIT_RemapParticleCountToScalar::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	RemapCountContext_t *pCtx = reinterpret_cast<RemapCountContext_t *>( pContext );

	// if we're only working on a specified set, and we've exceeded it, early out
	if ( m_bActiveRange && pCtx->m_nCurrentParticleCount > m_nInputMax )
		return;

	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		if ( m_bActiveRange && ( pCtx->m_nCurrentParticleCount < m_nInputMin || pCtx->m_nCurrentParticleCount > m_nInputMax ) )
		{
			pCtx->m_nCurrentParticleCount++;
			continue;
		}
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		float flOutput = RemapValClamped( float( pCtx->m_nCurrentParticleCount ), float( m_nInputMin ), float( m_nInputMax ), m_flOutputMin, m_flOutputMax  );

		if ( m_bScaleInitialRange )
		{
			float flInitialValue = *pOutput;
			flOutput *= flInitialValue;
		}
		if ( ATTRIBUTES_WHICH_ARE_INTS & ( 1 << m_nFieldOutput ) )
		{
			*pOutput = int ( flOutput );
		}
		else
		{
			*pOutput = flOutput;
		}
		pCtx->m_nCurrentParticleCount++;
	}
}



//-----------------------------------------------------------------------------
// Inherit Velocity Initializer
// Causes particles to inherit the velocity of their CP at spawn
// 
//-----------------------------------------------------------------------------
class C_INIT_InheritVelocity : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_InheritVelocity );

	int m_nControlPointNumber;
	float m_flVelocityScale;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }

};

DEFINE_PARTICLE_OPERATOR( C_INIT_InheritVelocity, "Velocity Inherit from Control Point", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_InheritVelocity ) 
DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "velocity scale", "1", float, m_flVelocityScale )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_InheritVelocity )


void C_INIT_InheritVelocity::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	Vector vecControlPointPrev;
	pParticles->GetControlPointAtPrevTime( m_nControlPointNumber, &vecControlPointPrev );
	Vector vecDeltaPos = pParticles->GetControlPointAtCurrentTime( m_nControlPointNumber) - vecControlPointPrev;
	vecDeltaPos *= m_flVelocityScale;

	for( ; nParticleCount--; start_p++ )
	{
		float *prevxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		prevxyz[0] -= vecDeltaPos.x;
		prevxyz[4] -= vecDeltaPos.y;
		prevxyz[8] -= vecDeltaPos.z;
	}
}




//-----------------------------------------------------------------------------
// Velocity From CP Initializer
// Causes particles to inherit the velocity of their CP at spawn
// 
//-----------------------------------------------------------------------------
class C_INIT_VelocityFromCP : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_VelocityFromCP );

	int m_nControlPoint;
	int m_nControlPointCompare;
	int m_nControlPointLocal;
	float m_flVelocityScale;
	bool m_bDirectionOnly;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = ( 1ULL << m_nControlPoint );
		if ( m_nControlPointCompare != -1 )
		{
			nMask |= ( 1ULL << m_nControlPointCompare );
		}
		if ( m_nControlPointLocal != -1 )
		{
			nMask |= ( 1ULL << m_nControlPointLocal );
		}
		return nMask;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	bool InitMultipleOverride ( void ) { return true; }

};

DEFINE_PARTICLE_OPERATOR( C_INIT_VelocityFromCP, "Velocity Set from Control Point", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_VelocityFromCP ) 
DMXELEMENT_UNPACK_FIELD( "control point number", "0", int, m_nControlPoint )
DMXELEMENT_UNPACK_FIELD( "velocity scale", "1", float, m_flVelocityScale )
DMXELEMENT_UNPACK_FIELD( "comparison control point number", "-1", int, m_nControlPointCompare )
DMXELEMENT_UNPACK_FIELD( "local space control point number", "-1", int, m_nControlPointLocal )
DMXELEMENT_UNPACK_FIELD( "direction only", "0", bool, m_bDirectionOnly )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_VelocityFromCP )


void C_INIT_VelocityFromCP::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	Vector vecDeltaPos;
	if ( m_nControlPointCompare > -1 )
		vecDeltaPos = ( pParticles->GetControlPointAtCurrentTime( m_nControlPoint ) - pParticles->GetControlPointAtCurrentTime( m_nControlPointCompare ) );
	else
	{
		vecDeltaPos = pParticles->GetControlPointAtCurrentTime( m_nControlPoint );
	}

	if ( m_nControlPointLocal > -1 )
	{
		Vector vecDeltaPosInitial = vecDeltaPos;
		matrix3x4_t mat;
		pParticles->GetControlPointTransformAtCurrentTime( m_nControlPointLocal, &mat );
		VectorRotate( vecDeltaPosInitial, mat, vecDeltaPos );
	}

	if ( m_bDirectionOnly )
		vecDeltaPos.NormalizeInPlace();

	vecDeltaPos *= pParticles->m_flPreviousDt;

	vecDeltaPos.x *= m_flVelocityScale;
	vecDeltaPos.y *= m_flVelocityScale;
	vecDeltaPos.z *= m_flVelocityScale;

	for( ; nParticleCount--; start_p++ )
	{
		float *prevxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		prevxyz[0] -= vecDeltaPos.x;
		prevxyz[4] -= vecDeltaPos.y;
		prevxyz[8] -= vecDeltaPos.z;
	}
}

//-----------------------------------------------------------------------------
// Pre-Age Noise
// Sets particle creation time back to treat newly spawned particle as if 
// part of its life has already elapsed.
//-----------------------------------------------------------------------------
class C_INIT_AgeNoise : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_AgeNoise );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	bool InitMultipleOverride ( void ) { return true; }

	bool	m_bAbsVal, m_bAbsValInv;
	float	m_flOffset;
	float	m_flAgeMin;
	float	m_flAgeMax;
	float	m_flNoiseScale, m_flNoiseScaleLoc;
	Vector  m_vecOffsetLoc;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_AgeNoise, "Lifetime Pre-Age Noise", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_AgeNoise )
DMXELEMENT_UNPACK_FIELD( "time noise coordinate scale","1.0",float,m_flNoiseScale)
DMXELEMENT_UNPACK_FIELD( "spatial noise coordinate scale","1.0",float,m_flNoiseScaleLoc)
DMXELEMENT_UNPACK_FIELD( "time coordinate offset","0", float, m_flOffset )
DMXELEMENT_UNPACK_FIELD( "spatial coordinate offset","0 0 0", Vector, m_vecOffsetLoc )
DMXELEMENT_UNPACK_FIELD( "absolute value","0", bool, m_bAbsVal )
DMXELEMENT_UNPACK_FIELD( "invert absolute value","0", bool, m_bAbsValInv )
DMXELEMENT_UNPACK_FIELD( "start age minimum","0", float, m_flAgeMin )
DMXELEMENT_UNPACK_FIELD( "start age maximum","1", float, m_flAgeMax )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_AgeNoise );

void C_INIT_AgeNoise::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	float	flAbsScale;
	int		nAbsVal;
	nAbsVal = 0xffffffff; 
	flAbsScale = 0.5;
	if ( m_bAbsVal )
	{
		nAbsVal = 0x7fffffff;
		flAbsScale = 1.0;
	}

	float fMin = m_flAgeMin;
	float fMax = m_flAgeMax;

	float CoordScale = m_flNoiseScale;
	float CoordScaleLoc = m_flNoiseScaleLoc;

	for( ; nParticleCount--; start_p++ )
	{	
		const float *pxyz = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		const float *pLifespan = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
		float *pAttr = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );		

		float ValueScale, ValueBase;

		Vector Coord, CoordLoc;
		CoordLoc.x = pxyz[0]; 
		CoordLoc.y = pxyz[4];
		CoordLoc.z = pxyz[8];
		CoordLoc += m_vecOffsetLoc;

		float Offset = m_flOffset;
		Coord = Vector ( (*pCreationTime + Offset), (*pCreationTime + Offset), (*pCreationTime + Offset) );
		Coord *= CoordScale;
		CoordLoc *= CoordScaleLoc;
		Coord += CoordLoc;

		fltx4 flNoise128;
		FourVectors fvNoise;

		fvNoise.DuplicateVector( Coord );
		flNoise128 = NoiseSIMD( fvNoise );
		float flNoise = SubFloat( flNoise128, 0 );

		*( (int *) &flNoise)  &= nAbsVal;

		ValueScale = ( flAbsScale *( fMax - fMin ) );
		ValueBase = ( fMin+ ( ( 1.0 - flAbsScale ) *( fMax - fMin ) ) );

		if ( m_bAbsValInv )
		{
			flNoise = 1.0 - flNoise;
		}

		float flInitialNoise = ( ValueBase + ( ValueScale * flNoise ) );


		flInitialNoise = clamp(flInitialNoise, 0.0f, 1.0f );
		flInitialNoise *= *pLifespan;

		*( pAttr ) = *pCreationTime - flInitialNoise;
	}
}




//-----------------------------------------------------------------------------
// LifeTime Sequence Length
//-----------------------------------------------------------------------------
class C_INIT_SequenceLifeTime : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_SequenceLifeTime );

	float m_flFramerate;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER_MASK;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask, void *pContext ) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_SequenceLifeTime, "Lifetime From Sequence", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_SequenceLifeTime ) 
DMXELEMENT_UNPACK_FIELD( "Frames Per Second", "30", float, m_flFramerate )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_SequenceLifeTime )

void C_INIT_SequenceLifeTime::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( ( m_flFramerate != 0.0f ) && ( pParticles->m_Sheet() ) )
	{
		for( ; nParticleCount--; start_p++ )
		{
			const float *flSequence = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER, start_p );
			float *dtime = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
			int nSequence = *flSequence;

			if ( pParticles->m_Sheet()->m_SheetInfo[nSequence].m_flFrameSpan != 0 )
			{
				*dtime = pParticles->m_Sheet()->m_SheetInfo[nSequence].m_flFrameSpan / m_flFramerate;
			}
			else
			{
				*dtime = 1.0;
			}
		}
	}
}




//-----------------------------------------------------------------------------
// Create In Hierarchy
//-----------------------------------------------------------------------------
class C_INIT_CreateInHierarchy : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateInHierarchy );

	float m_fMaxDistance;
	float m_flGrowthTime;
	//float m_flTraceDist; 
	float m_flDesiredMidPoint;
	int m_nOrientation;
	float m_flBulgeFactor;
	int m_nDesiredEndPoint;
	int m_nDesiredStartPoint;
	bool m_bUseHighestEndCP;
	Vector m_vecDistanceBias, m_vecDistanceBiasAbs;
	bool m_bDistanceBias, m_bDistanceBiasAbs;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nStartMask = ( 1ULL << m_nDesiredStartPoint ) - 1;
		uint64 nEndMask = m_bUseHighestEndCP ? 0xFFFFFFFFFFFFFFFFll : ( 1ULL << ( m_nDesiredEndPoint + 1 ) ) - 1;
		return nEndMask & (~nStartMask);
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		//fixme - confirm CPs
		//		m_PathParams.ClampControlPointIndices();
		m_bDistanceBias = ( m_vecDistanceBias.x != 1.0f ) || ( m_vecDistanceBias.y != 1.0f ) || ( m_vecDistanceBias.z != 1.0f );
		m_bDistanceBiasAbs = ( m_vecDistanceBiasAbs.x != 0.0f ) || ( m_vecDistanceBiasAbs.y != 0.0f ) || ( m_vecDistanceBiasAbs.z != 0.0f );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
								 int nParticleCount, int nAttributeWriteMask,
								 void *pContext) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateInHierarchy, "Position In CP Hierarchy", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreateInHierarchy ) 
	DMXELEMENT_UNPACK_FIELD( "maximum distance", "0", float, m_fMaxDistance )
	DMXELEMENT_UNPACK_FIELD( "bulge", "0", float, m_flBulgeFactor )
	DMXELEMENT_UNPACK_FIELD( "start control point number", "0", int, m_nDesiredStartPoint )
	DMXELEMENT_UNPACK_FIELD( "end control point number", "1", int, m_nDesiredEndPoint )
	DMXELEMENT_UNPACK_FIELD( "bulge control 0=random 1=orientation of start pnt 2=orientation of end point", "0", int, m_nOrientation )
	DMXELEMENT_UNPACK_FIELD( "mid point position", "0.5", float, m_flDesiredMidPoint )
	DMXELEMENT_UNPACK_FIELD( "growth time", "0.0", float, m_flGrowthTime )
	//DMXELEMENT_UNPACK_FIELD( "trace distance for optional culling", "0.0", float, m_flTraceDist )
	DMXELEMENT_UNPACK_FIELD( "use highest supplied end point", "0", bool, m_bUseHighestEndCP )
	DMXELEMENT_UNPACK_FIELD( "distance_bias", "1 1 1", Vector, m_vecDistanceBias )
	DMXELEMENT_UNPACK_FIELD( "distance_bias_absolute_value", "0 0 0", Vector, m_vecDistanceBiasAbs )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateInHierarchy )


void C_INIT_CreateInHierarchy::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	int nEndCP;
	float flGrowth;
	struct CPathParameters PathParams;
	PathParams.m_flBulge = m_flBulgeFactor;
	PathParams.m_nBulgeControl = m_nOrientation;
	PathParams.m_flMidPoint = m_flDesiredMidPoint;
	int nRealEndPoint;

	if ( m_bUseHighestEndCP )
	{
		nRealEndPoint = pParticles->GetHighestControlPoint();
	}
	else
	{
		nRealEndPoint = m_nDesiredEndPoint;
	}

	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		if ( ( pParticles->m_flCurTime <= m_flGrowthTime ) && ( nRealEndPoint > 0 ) )
		{
			float nCurrentEndCP = RemapValClamped( *ct, 0.0f, m_flGrowthTime, MIN( m_nDesiredStartPoint + 1, nRealEndPoint ), nRealEndPoint );
			nEndCP =  pParticles->RandomInt( MIN( m_nDesiredStartPoint + 1, nCurrentEndCP ), nCurrentEndCP );

			// clamp growth to the appropriate values...
			float flEndTime = float(nCurrentEndCP) / float(nRealEndPoint) ;
			flGrowth = RemapValClamped( *ct, 0.0f, m_flGrowthTime, 0.0, flEndTime );
		}
		else
		{
			int nLowestStartPoint =  MIN( m_nDesiredStartPoint + 1, nRealEndPoint );
			nEndCP =  pParticles->RandomInt( nLowestStartPoint, nRealEndPoint );
			flGrowth = 1.0;
		}


		PathParams.m_nStartControlPointNumber = pParticles->ControlPoint( nEndCP ).m_nParent;
		PathParams.m_nEndControlPointNumber = nEndCP;
		Vector StartPnt, MidP, EndPnt;

		pParticles->CalculatePathValues( PathParams, *ct, &StartPnt, &MidP, &EndPnt);
		EndPnt *= flGrowth;

		float t=pParticles->RandomFloat( 0.0, 1.0 );
		
		Vector randpos;
		pParticles->RandomVector( -m_fMaxDistance, m_fMaxDistance, &randpos );

		if ( m_bDistanceBiasAbs	)
		{
			if ( m_vecDistanceBiasAbs.x	!= 0.0f )
			{
				randpos.x = fabs(randpos.x);
			}
			if ( m_vecDistanceBiasAbs.y	!= 0.0f )
			{
				randpos.y = fabs(randpos.y);
			}
			if ( m_vecDistanceBiasAbs.z	!= 0.0f )
			{
				randpos.z = fabs(randpos.z);
			}
		}
		randpos *= m_vecDistanceBias;

		// form delta terms needed for quadratic bezier
		Vector Delta0=MidP-StartPnt;
		Vector Delta1 = EndPnt-MidP;

		Vector L0 = StartPnt+t*Delta0;
		Vector L1 = MidP+t*Delta1;

		Vector Pnt = L0+(L1-L0)*t;

		Pnt+=randpos;
		// Optional Culling based on configurable trace distance.  Failing particle are destroyed
		//disabled for now.
		//if ( m_flTraceDist != 0.0f )
		//{
		//	// Trace down
		//	Vector TraceDir=Vector(0, 0, -1);
		//	// now set the trace distance
		//	// note - probably need to offset Pnt upwards for some fudge factor on irregular surfaces
		//	CBaseTrace tr;
		//	Vector RayStart=Pnt;
		//	float flRadius = m_flTraceDist;
		//	g_pParticleSystemMgr->Query()->TraceLine( RayStart, ( RayStart + ( TraceDir * flRadius ) ), MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );
		//	if ( tr.fraction == 1.0 )
		//	{
		//		//If the trace hit nothing, kill the particle.
		//		pParticles->KillParticle( start_p );
		//	}
		//	else
		//	{
		//		//If we hit something, set particle position to collision position
		//		Pnt += tr.endpos;
		//		//FIXME - if we add a concept of a particle normal (for example, aligned quads or decals, set it here)
		//	}
		//}

		xyz[0] = Pnt.x;
		xyz[4] = Pnt.y;
		xyz[8] = Pnt.z;
		if ( pxyz && ( nAttributeWriteMask & PARTICLE_ATTRIBUTE_PREV_XYZ_MASK ) )
		{
			pxyz[0] = Pnt.x;
			pxyz[4] = Pnt.y;
			pxyz[8] = Pnt.z;
		}
	}
}



//-----------------------------------------------------------------------------
// Remap initial Scalar to Vector Initializer
//-----------------------------------------------------------------------------
class C_INIT_RemapScalarToVector : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapScalarToVector );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 1 << m_nFieldInput;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nFieldInput;
	int		m_nFieldOutput;
	float	m_flInputMin;
	float	m_flInputMax;
	Vector	m_vecOutputMin;
	Vector	m_vecOutputMax;
	float	m_flStartTime;
	float	m_flEndTime;
	bool	m_bScaleInitialRange;
	int		m_nControlPointNumber;
	bool	m_bLocalCoords;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapScalarToVector, "Remap Scalar to Vector", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RemapScalarToVector )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime start time (seconds)", "-1", float, m_flStartTime )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime end time (seconds)", "-1", float, m_flEndTime )
DMXELEMENT_UNPACK_FIELD_USERDATA( "input field", "8", int, m_nFieldInput, "intchoice particlefield_scalar" )
DMXELEMENT_UNPACK_FIELD( "input minimum","0", float, m_flInputMin )
DMXELEMENT_UNPACK_FIELD( "input maximum","1", float, m_flInputMax )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "0", int, m_nFieldOutput, "intchoice particlefield_vector" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0 0 0", Vector, m_vecOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","1 1 1", Vector, m_vecOutputMax )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
DMXELEMENT_UNPACK_FIELD( "use local system", "1", bool, m_bLocalCoords )
DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapScalarToVector )

void C_INIT_RemapScalarToVector::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	const float *pCreationTime;
	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		// using raw creation time to map to emitter lifespan
		float flLifeTime = *pCreationTime;  

		// only use within start/end time frame
		if ( ( ( flLifeTime < m_flStartTime ) || ( flLifeTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) )
			continue;

		const float *pInput = pParticles->GetFloatAttributePtr( m_nFieldInput, start_p );
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		Vector vecOutput = vec3_origin;
		vecOutput.x = RemapValClamped( *pInput, m_flInputMin, m_flInputMax, m_vecOutputMin.x, m_vecOutputMax.x  );
		vecOutput.y = RemapValClamped( *pInput, m_flInputMin, m_flInputMax, m_vecOutputMin.y, m_vecOutputMax.y  );
		vecOutput.z = RemapValClamped( *pInput, m_flInputMin, m_flInputMax, m_vecOutputMin.z, m_vecOutputMax.z  );


		if ( m_nFieldOutput == 0 )
		{
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			if ( !m_bLocalCoords )
			{
				Vector vecControlPoint;
				pParticles->GetControlPointAtTime( m_nControlPointNumber, *pCreationTime, &vecControlPoint );
				vecOutput += vecControlPoint;
				Vector vecOutputPrev = vecOutput;
				if ( m_bScaleInitialRange )
				{
					Vector vecScaleInitial;
					Vector vecScaleInitialPrev;
					SetVectorFromAttribute ( vecScaleInitial, pOutput );
					SetVectorFromAttribute ( vecScaleInitialPrev, pxyz );
					vecOutput *= vecScaleInitial;
					vecOutputPrev *= vecScaleInitialPrev;
				}
				SetVectorAttribute( pOutput, vecOutput );
				SetVectorAttribute( pxyz, vecOutputPrev ); 
			}
			else
			{
				matrix3x4_t mat;
				pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *pCreationTime, &mat );
				Vector vecTransformLocal = vec3_origin;
				VectorTransform( vecOutput, mat, vecTransformLocal );
				vecOutput = vecTransformLocal;
				Vector vecOutputPrev = vecOutput;
				if ( m_bScaleInitialRange )
				{
					Vector vecScaleInitial;
					Vector vecScaleInitialPrev;
					SetVectorFromAttribute ( vecScaleInitial, pOutput );
					SetVectorFromAttribute ( vecScaleInitialPrev, pxyz );
					vecOutput *= vecScaleInitial;
					vecOutputPrev *= vecScaleInitialPrev;
				}
				SetVectorAttribute( pOutput, vecOutput );
				SetVectorAttribute( pxyz, vecOutput ); 
			}
		}
		else
		{
			if ( m_bScaleInitialRange )
			{
				Vector vecScaleInitial;
				SetVectorFromAttribute ( vecScaleInitial, pOutput );
				vecOutput *= vecScaleInitial;
			}
			SetVectorAttribute( pOutput, vecOutput ); 
		}
	}
}



//-----------------------------------------------------------------------------
// Offset Vector to Vector Initializer
//-----------------------------------------------------------------------------
class C_INIT_OffsetVectorToVector : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_OffsetVectorToVector );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 1 << m_nFieldInput;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nFieldInput;
	int		m_nFieldOutput;
	float	m_flInputMin;
	float	m_flInputMax;
	Vector	m_vecOutputMin;
	Vector	m_vecOutputMax;
	float	m_flStartTime;
	float	m_flEndTime;
	int		m_nControlPointNumber;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_OffsetVectorToVector, "Offset Vector to Vector", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_OffsetVectorToVector )
DMXELEMENT_UNPACK_FIELD_USERDATA( "input field", "0", int, m_nFieldInput, "intchoice particlefield_vector" )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "0", int, m_nFieldOutput, "intchoice particlefield_vector" )
DMXELEMENT_UNPACK_FIELD( "output offset minimum","0 0 0", Vector, m_vecOutputMin )
DMXELEMENT_UNPACK_FIELD( "output offset maximum","1 1 1", Vector, m_vecOutputMax )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_OffsetVectorToVector )

void C_INIT_OffsetVectorToVector::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		const float *pInput = pParticles->GetFloatAttributePtr( m_nFieldInput, start_p );
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		Vector vecInput;
		SetVectorFromAttribute( vecInput, pInput );
		Vector vecOutput;
		pParticles->RandomVector( m_vecOutputMin, m_vecOutputMax, &vecOutput );
		vecOutput += vecInput;
		SetVectorAttribute( pOutput, vecOutput );
	}
}


//-----------------------------------------------------------------------------
// Create particles sequentially along a path
//-----------------------------------------------------------------------------
struct SequentialPathContext_t
{
	int		m_nCPCountAmount;
	float	m_flParticleCount;
	float	m_flPerSegmentAmount;
	float	m_flCPCount;
	float	m_flStep;
	float	m_flCPStep;
};
class C_INIT_CreateSequentialPath : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateSequentialPath );

	float m_fMaxDistance;
	float m_flNumToAssign;
	bool m_bLoop;
	bool m_bCPPairs;
	bool m_bSaveOffset;
	struct CPathParameters m_PathParams;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ_MASK ;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nStartMask = ( 1ULL << m_PathParams.m_nStartControlPointNumber ) - 1;
		uint64 nEndMask = m_bCPPairs ? 0xFFFFFFFFFFFFFFFFULL : ( 1ULL << ( m_PathParams.m_nEndControlPointNumber + 1 ) ) - 1;
		return nEndMask & (~nStartMask);
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		SequentialPathContext_t *pCtx = reinterpret_cast<SequentialPathContext_t *>( pContext );
		pCtx->m_flParticleCount = 0;
		if ( m_flNumToAssign > 1.0f )
		{
			pCtx->m_flStep = 1.0f / ( m_flNumToAssign - 1 );
		}
		else
		{
			pCtx->m_flStep = 0.0f;
		}
		pCtx->m_flPerSegmentAmount = -1;

		if ( m_bCPPairs )
		{
			pCtx->m_flCPCount = m_PathParams.m_nStartControlPointNumber;
			if ( m_PathParams.m_nEndControlPointNumber - m_PathParams.m_nStartControlPointNumber > 1 && m_flNumToAssign > 1.0f )
			{
				pCtx->m_flStep *=  m_PathParams.m_nEndControlPointNumber - m_PathParams.m_nStartControlPointNumber;
				pCtx->m_flCPStep = pCtx->m_flStep ;
			}
			else
			{
				pCtx->m_flCPStep = 0.0f;
			}
		}
	}

	virtual void Restart( CParticleCollection *pParticles, void *pContext )
	{
		SequentialPathContext_t *pCtx = reinterpret_cast<SequentialPathContext_t *>( pContext );
		pCtx->m_flParticleCount = 0;
		if ( m_flNumToAssign > 1.0f )
		{
			pCtx->m_flStep = 1.0f / ( m_flNumToAssign - 1 );
		}
		else
		{
			pCtx->m_flStep = 0.0f;
		}
		pCtx->m_flPerSegmentAmount = -1;

		if ( m_bCPPairs )
		{
			pCtx->m_flCPCount = m_PathParams.m_nStartControlPointNumber;
			if ( m_PathParams.m_nEndControlPointNumber - m_PathParams.m_nStartControlPointNumber > 1 && m_flNumToAssign > 1.0f )
			{
				pCtx->m_flStep *=  m_PathParams.m_nEndControlPointNumber - m_PathParams.m_nStartControlPointNumber;
				pCtx->m_flCPStep = pCtx->m_flStep ;
			}
			else
			{
				pCtx->m_flCPStep = 0.0f;
			}
		}
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_PathParams.ClampControlPointIndices();


	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( SequentialPathContext_t );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateSequentialPath, "Position Along Path Sequential", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreateSequentialPath ) 
DMXELEMENT_UNPACK_FIELD( "maximum distance", "0", float, m_fMaxDistance )
DMXELEMENT_UNPACK_FIELD( "bulge", "0", float, m_PathParams.m_flBulge )
DMXELEMENT_UNPACK_FIELD( "start control point number", "0", int, m_PathParams.m_nStartControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "end control point number", "0", int, m_PathParams.m_nEndControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "bulge control 0=random 1=orientation of start pnt 2=orientation of end point", "0", int, m_PathParams.m_nBulgeControl )
DMXELEMENT_UNPACK_FIELD( "mid point position", "0.5", float, m_PathParams.m_flMidPoint )
DMXELEMENT_UNPACK_FIELD( "particles to map from start to end", "100", float, m_flNumToAssign )
DMXELEMENT_UNPACK_FIELD( "restart behavior (0 = bounce, 1 = loop )", "1", bool, m_bLoop )
DMXELEMENT_UNPACK_FIELD( "Use sequential CP pairs between start and end point", "0", bool, m_bCPPairs )
DMXELEMENT_UNPACK_FIELD( "Save Offset", "0", bool, m_bSaveOffset )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateSequentialPath )


void C_INIT_CreateSequentialPath::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	// NOTE: Using C_OP_ContinuousEmitter:: avoids a virtual function call
	SequentialPathContext_t *pCtx = reinterpret_cast<SequentialPathContext_t *>( pContext );

	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		struct CPathParameters PathParams = m_PathParams;

		int nTotalCPCount = 1;

		if ( m_bCPPairs )
		{
			nTotalCPCount = m_PathParams.m_nEndControlPointNumber - m_PathParams.m_nStartControlPointNumber;
		
			if ( pCtx->m_flCPCount > nTotalCPCount || pCtx->m_flCPCount < m_PathParams.m_nStartControlPointNumber )
			{
				if ( m_bLoop )
				{
					pCtx->m_flCPCount -= nTotalCPCount;
				}
				else
				{
					//Flip CP step direction.
					pCtx->m_flCPStep *= -1;
					pCtx->m_flCPCount += 2 * pCtx->m_flCPStep;
					// We flip individual particle step here because we've hit the end of the CP line.
					pCtx->m_flStep *= -1;
					pCtx->m_flPerSegmentAmount *= -1;
					pCtx->m_flParticleCount += 2 * pCtx->m_flStep;
				}
			}
			PathParams.m_nStartControlPointNumber = int ( pCtx->m_flCPCount );
			PathParams.m_nEndControlPointNumber = PathParams.m_nStartControlPointNumber + 1;
		}

		if ( pCtx->m_flParticleCount > 1 + FLT_EPSILON || pCtx->m_flParticleCount < 0 )
		{
			if ( m_bLoop ) 
			{
				pCtx->m_flParticleCount += pCtx->m_flPerSegmentAmount;
			}
			else 
			{
				if ( !m_bCPPairs ) // We're doing an 2 specific CPs and bouncing
				{
					pCtx->m_flStep *= -1;
					pCtx->m_flPerSegmentAmount *= -1;
					pCtx->m_flParticleCount += 2 * pCtx->m_flStep;
				}
				else  //  Multiple CP sets require bouncing to 0 or 1 + the remaining step amount.
				{
					pCtx->m_flParticleCount += pCtx->m_flPerSegmentAmount;
				}
			}
		}

		float t = pCtx->m_flParticleCount;

		Vector StartPnt, MidP, EndPnt;
		pParticles->CalculatePathValues( PathParams, *ct, &StartPnt, &MidP, &EndPnt);

		Vector randpos;
		pParticles->RandomVector( -m_fMaxDistance, m_fMaxDistance, &randpos );

		// form delta terms needed for quadratic bezier
		Vector Delta0=MidP-StartPnt;
		Vector Delta1 = EndPnt-MidP;

		Vector L0 = StartPnt+t*Delta0;
		Vector L1 = MidP+t*Delta1;
		
		Vector Pnt = L0+(L1-L0)*t;

		Pnt+=randpos;

		xyz[0] = Pnt.x;
		xyz[4] = Pnt.y;
		xyz[8] = Pnt.z;

		pxyz[0] = Pnt.x;
		pxyz[4] = Pnt.y;
		pxyz[8] = Pnt.z;

		if ( m_bSaveOffset )
		{
			float *pSavedPos = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_HITBOX_RELATIVE_XYZ, start_p );
			Vector vecSavedPos = Vector( t, PathParams.m_nStartControlPointNumber, PathParams.m_nEndControlPointNumber );
			SetVectorAttribute( pSavedPos, vecSavedPos );
		}

		pCtx->m_flParticleCount += pCtx->m_flStep;
		pCtx->m_flCPCount += pCtx->m_flCPStep;
	}
}


//-----------------------------------------------------------------------------
//   Initial Repulsion Velocity - repulses the particles from nearby surfaces 
//	 on spawn
//-----------------------------------------------------------------------------
class C_INIT_InitialRepulsionVelocity : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_InitialRepulsionVelocity );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK | PARTICLE_ATTRIBUTE_RADIUS_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		if ( m_bPerParticle )
			return 0;
		if ( m_bInherit )
			return ( 1ULL << m_nControlPointNumber ) | ( 1ULL << ( m_nControlPointNumber + 1 ) );
		return 1ULL << m_nControlPointNumber;
	}

	virtual uint64 GetNonPositionalControlPointMask() const
	{
		if ( m_bPerParticle || !m_bInherit )
			return 0;
		return ( 1ULL << m_nControlPointNumber ) | ( 1ULL << ( m_nControlPointNumber + 1 ) );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;	

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nCollisionGroupNumber = g_pParticleSystemMgr->Query()->GetCollisionGroupFromName( m_CollisionGroupName );
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }

	char m_CollisionGroupName[128];
	int m_nCollisionGroupNumber;
	Vector	m_vecOutputMin;
	Vector	m_vecOutputMax;
	int		nRemainingBlocks;
	int		m_nControlPointNumber;
	bool	m_bPerParticle;
	bool	m_bTranslate;
	bool	m_bProportional;
	float	m_flTraceLength;
	bool	m_bPerParticleTR;
	bool	m_bInherit;
	int		m_nChildCP;
	int		m_nChildGroupID;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_InitialRepulsionVelocity, "Velocity Repulse from World", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_InitialRepulsionVelocity )
DMXELEMENT_UNPACK_FIELD( "minimum velocity","0 0 0", Vector, m_vecOutputMin )
DMXELEMENT_UNPACK_FIELD( "maximum velocity","1 1 1", Vector, m_vecOutputMax )
DMXELEMENT_UNPACK_FIELD_STRING( "collision group", "NONE", m_CollisionGroupName )
DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "Per Particle World Collision Tests", "0", bool, m_bPerParticle )
DMXELEMENT_UNPACK_FIELD( "Use radius for Per Particle Trace Length", "0", bool, m_bPerParticleTR )
DMXELEMENT_UNPACK_FIELD( "Offset instead of accelerate", "0", bool, m_bTranslate )
DMXELEMENT_UNPACK_FIELD( "Offset proportional to radius 0/1", "0", bool, m_bProportional )
DMXELEMENT_UNPACK_FIELD( "Trace Length", "64.0", float, m_flTraceLength )
DMXELEMENT_UNPACK_FIELD( "Inherit from Parent", "0", bool, m_bInherit )
DMXELEMENT_UNPACK_FIELD( "control points to broadcast to children (n + 1)", "-1", int, m_nChildCP )
DMXELEMENT_UNPACK_FIELD( "Child Group ID to affect", "0", int, m_nChildGroupID )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_InitialRepulsionVelocity );


void C_INIT_InitialRepulsionVelocity::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{

	Vector	d[6];

	//All cardinal directions
	d[0] = Vector(  1,  0,  0 );
	d[1] = Vector( -1,  0,  0 );
	d[2] = Vector(  0,  1,  0 );
	d[3] = Vector(  0, -1,  0 );
	d[4] = Vector(  0,  0,  1 );
	d[5] = Vector(  0,  0, -1 );

	//Init the results
	Vector resultDirection;
	float resultForce;
	if ( m_bPerParticle )
	{
		for( ; nParticleCount--; start_p++ )
		{	

			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz_prev = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			const float *radius = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, start_p );
			Vector vecCurrentPos;
			SetVectorFromAttribute( vecCurrentPos, pxyz );

			resultDirection.Init();
			resultForce = 0.0f;

			//Get the aggregate force vector
			for ( int i = 0; i < 6; i++ )
			{
				//Press out
				float flTraceDistance = m_flTraceLength;
				if ( m_bPerParticleTR )
				{
					flTraceDistance = *radius;
				}
				Vector endpos = vecCurrentPos + ( d[i] * flTraceDistance );

				//Trace into the world
				CBaseTrace tr;
				g_pParticleSystemMgr->Query()->TraceLine( vecCurrentPos, endpos, CONTENTS_SOLID, NULL, m_nCollisionGroupNumber, &tr );

				//Push back a proportional amount to the probe
				d[i] = -d[i] * (1.0f-tr.fraction);

				assert(( 1.0f - tr.fraction ) >= 0.0f );

				resultForce += 1.0f-tr.fraction;
				resultDirection += d[i];
			}

			//If we've hit nothing, then point up
			if ( resultDirection == vec3_origin )
			{
				resultDirection = Vector( 0, 0, 1 );
				resultForce = 0.0f;
			}

			//Just return the direction
			VectorNormalize( resultDirection );
			resultDirection *= resultForce;

			Vector vecRepulsionAmount;

			vecRepulsionAmount.x = Lerp( resultForce, m_vecOutputMin.x, m_vecOutputMax.x );
			vecRepulsionAmount.y = Lerp( resultForce, m_vecOutputMin.y, m_vecOutputMax.y );
			vecRepulsionAmount.z = Lerp( resultForce, m_vecOutputMin.z, m_vecOutputMax.z );


			vecRepulsionAmount *= resultDirection;


			if ( m_bProportional )
			{
				vecRepulsionAmount *= *radius;
			}

			pxyz[0] += vecRepulsionAmount.x;
			pxyz[4] += vecRepulsionAmount.y;
			pxyz[8] += vecRepulsionAmount.z;

			if ( m_bTranslate )
			{
				pxyz_prev[0] += vecRepulsionAmount.x;
				pxyz_prev[4] += vecRepulsionAmount.y;
				pxyz_prev[8] += vecRepulsionAmount.z;
			}
		}
	}
	else
	{
		
		Vector vecRepulsionAmount;

		if ( m_bInherit )
		{
			float *ct = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
			pParticles->GetControlPointAtTime( m_nControlPointNumber, *ct, &resultDirection );
			Vector vecPassedForce;
			pParticles->GetControlPointAtTime( m_nControlPointNumber+1, *ct, &vecPassedForce );

			vecRepulsionAmount.x = Lerp( vecPassedForce.x, m_vecOutputMin.x, m_vecOutputMax.x );
			vecRepulsionAmount.y = Lerp( vecPassedForce.x, m_vecOutputMin.y, m_vecOutputMax.y );
			vecRepulsionAmount.z = Lerp( vecPassedForce.x, m_vecOutputMin.z, m_vecOutputMax.z );

			vecRepulsionAmount *= resultDirection;
		}
		else
		{
			float *ct = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
			Vector vecControlPoint;
			pParticles->GetControlPointAtTime( m_nControlPointNumber, *ct, &vecControlPoint );

			Vector vecCurrentPos = vecControlPoint;

			resultDirection.Init();
			resultForce = 0.0f;

			//Get the aggregate force vector
			for ( int i = 0; i < 6; i++ )
			{
				//Press out
				Vector endpos = vecCurrentPos + ( d[i] * m_flTraceLength );

				//Trace into the world
				CBaseTrace tr;
				g_pParticleSystemMgr->Query()->TraceLine( vecCurrentPos, endpos, CONTENTS_SOLID, NULL, m_nCollisionGroupNumber, &tr );

				//Push back a proportional amount to the probe
				d[i] = -d[i] * (1.0f-tr.fraction);

				assert(( 1.0f - tr.fraction ) >= 0.0f );

				resultForce += 1.0f-tr.fraction;
				resultDirection += d[i];
			}

			//If we've hit nothing, then point up
			if ( resultDirection == vec3_origin )
			{
				resultDirection = Vector( 0, 0, 1 );
				resultForce = 0.0f;
			}

			//Just return the direction
			VectorNormalize( resultDirection );
			resultDirection *= resultForce;

			vecRepulsionAmount.x = Lerp( resultForce, m_vecOutputMin.x, m_vecOutputMax.x );
			vecRepulsionAmount.y = Lerp( resultForce, m_vecOutputMin.y, m_vecOutputMax.y );
			vecRepulsionAmount.z = Lerp( resultForce, m_vecOutputMin.z, m_vecOutputMax.z );

			vecRepulsionAmount *= resultDirection;

			if ( m_nChildCP != -1 )
			{
				for( CParticleCollection *pChild = pParticles->m_Children.m_pHead; pChild; pChild = pChild->m_pNext )
				{
					if ( pChild->GetGroupID() == m_nChildGroupID )
					{
						Vector vecPassForce = Vector(resultForce, 0, 0);
						pChild->SetControlPoint( m_nChildCP, resultDirection );
						pChild->SetControlPoint( m_nChildCP+1, vecPassForce );
					}
				}
			}
		}
		
		for( ; nParticleCount--; start_p++ )
		{	

			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz_prev = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			const float *radius = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_RADIUS, start_p );

			if ( m_bProportional )
			{
				vecRepulsionAmount *= *radius;
			}

			pxyz[0] += vecRepulsionAmount.x;
			pxyz[4] += vecRepulsionAmount.y;
			pxyz[8] += vecRepulsionAmount.z;

			if ( m_bTranslate )
			{
				pxyz_prev[0] += vecRepulsionAmount.x;
				pxyz_prev[4] += vecRepulsionAmount.y;
				pxyz_prev[8] += vecRepulsionAmount.z;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Random Yaw Flip
//-----------------------------------------------------------------------------
class C_INIT_RandomYawFlip : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomYawFlip );
	
	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_YAW_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_YAW_MASK;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask, void *pContext ) const;

	float m_flPercent;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_RandomYawFlip, "Rotation Yaw Flip Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomYawFlip ) 
DMXELEMENT_UNPACK_FIELD( "Flip Percentage", ".5", float, m_flPercent )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomYawFlip )

void C_INIT_RandomYawFlip::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		float flChance = pParticles->RandomFloat( 0.0, 1.0 );
		if ( flChance < m_flPercent )
		{
			float *drot = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_YAW, start_p );
			float flRotation = fmodf( *drot , M_PI );
			*drot = M_PI + flRotation;
		}
	}
}



//-----------------------------------------------------------------------------
// Random second sequence
//-----------------------------------------------------------------------------
class C_INIT_RandomSecondSequence : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RandomSecondSequence );
 
	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1_MASK;
	}
 
	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}
 
	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
		// TODO: Validate the ranges here!
	}


	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p, int nParticleCount, int nAttributeWriteMask, void *pContext ) const
	{
		float *pSequence;
		for( ; nParticleCount--; start_p++ )
		{
			pSequence = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_SEQUENCE_NUMBER1, start_p );
			*pSequence = pParticles->RandomInt( m_nSequenceMin, m_nSequenceMax );
		}
	}
 
	int m_nSequenceMin;
	int m_nSequenceMax;
};
 
DEFINE_PARTICLE_OPERATOR( C_INIT_RandomSecondSequence, "Sequence Two Random", OPERATOR_GENERIC );
 
BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RandomSecondSequence ) 
	DMXELEMENT_UNPACK_FIELD_USERDATA( "sequence_min", "0", int, m_nSequenceMin, "sheetsequencepicker_second" )
	DMXELEMENT_UNPACK_FIELD_USERDATA( "sequence_max", "0", int, m_nSequenceMax, "sheetsequencepicker_second" )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RandomSecondSequence )



//-----------------------------------------------------------------------------
// Remap CP to Scalar Initializer
//-----------------------------------------------------------------------------
class C_INIT_RemapCPtoScalar : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapCPtoScalar );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nCPInput;
	}

	virtual uint64 GetNonPositionalControlPointMask() const
	{
		return 1ULL << m_nCPInput;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nField = int (clamp (m_nField, 0, 2));

		// clamp the result to 0 and 1 if it's alpha
		if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & ( 1 << m_nFieldOutput ) )
		{
			m_flOutputMin = clamp(m_flOutputMin, 0.0f, 1.0f );
			m_flOutputMax = clamp(m_flOutputMax, 0.0f, 1.0f );
		}
	}

	int		m_nCPInput;                                                             
	int		m_nFieldOutput;
	int		m_nField;
	float	m_flInputMin;
	float	m_flInputMax;
	float	m_flOutputMin;
	float	m_flOutputMax;
	float	m_flStartTime;
	float	m_flEndTime;
	bool	m_bScaleInitialRange;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapCPtoScalar, "Remap Control Point to Scalar", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RemapCPtoScalar )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime start time (seconds)", "-1", float, m_flStartTime )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime end time (seconds)", "-1", float, m_flEndTime )
DMXELEMENT_UNPACK_FIELD( "input control point number", "0", int, m_nCPInput )
DMXELEMENT_UNPACK_FIELD( "input minimum","0", float, m_flInputMin )
DMXELEMENT_UNPACK_FIELD( "input maximum","1", float, m_flInputMax )
DMXELEMENT_UNPACK_FIELD( "input field 0-2 X/Y/Z","0", int, m_nField )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapCPtoScalar )

void C_INIT_RemapCPtoScalar::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	const float *pCreationTime;

	Vector vecControlPoint;
	const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
	pParticles->GetControlPointAtTime( m_nCPInput, *ct, &vecControlPoint );

	float flInput = vecControlPoint[m_nField];

	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		// using raw creation time to map to emitter lifespan
		float flLifeTime = *pCreationTime;  

		// only use within start/end time frame
		if ( ( ( flLifeTime < m_flStartTime ) || ( flLifeTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) )
			continue;


		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		float flOutput = RemapValClamped( flInput, m_flInputMin, m_flInputMax, m_flOutputMin, m_flOutputMax  );
		if ( m_bScaleInitialRange )
		{

			flOutput = *pOutput * flOutput;
		}
		if ( ATTRIBUTES_WHICH_ARE_INTS & ( 1 << m_nFieldOutput ) )
		{
			*pOutput = int ( flOutput );
		}
		else
		{
			*pOutput = flOutput;
		}
	}
}



//-----------------------------------------------------------------------------
// Remap CP to Vector Initializer
//-----------------------------------------------------------------------------
class C_INIT_RemapCPtoVector : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapCPtoVector );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = ( 1ULL << m_nCPInput );
		if ( m_nLocalSpaceCP != -1 )
		{
			nMask |= ( 1ULL << m_nLocalSpaceCP );
		}
		return nMask;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nCPInput;                                                             
	int		m_nFieldOutput;
	Vector	m_vInputMin;
	Vector	m_vInputMax;
	Vector	m_vOutputMin;
	Vector	m_vOutputMax;
	float	m_flStartTime;
	float	m_flEndTime;
	bool	m_bScaleInitialRange;
	bool	m_bOffset;
	bool	m_bAccelerate;
	int		m_nLocalSpaceCP;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapCPtoVector, "Remap Control Point to Vector", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RemapCPtoVector )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime start time (seconds)", "-1", float, m_flStartTime )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime end time (seconds)", "-1", float, m_flEndTime )
DMXELEMENT_UNPACK_FIELD( "input control point number", "0", int, m_nCPInput )
DMXELEMENT_UNPACK_FIELD( "input minimum","0 0 0", Vector, m_vInputMin )
DMXELEMENT_UNPACK_FIELD( "input maximum","0 0 0", Vector, m_vInputMax )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "0", int, m_nFieldOutput, "intchoice particlefield_vector" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0 0 0", Vector, m_vOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","0 0 0", Vector, m_vOutputMax )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
DMXELEMENT_UNPACK_FIELD( "offset position","0", bool, m_bOffset )
DMXELEMENT_UNPACK_FIELD( "accelerate position","0", bool, m_bAccelerate )
DMXELEMENT_UNPACK_FIELD( "local space CP","-1", int, m_nLocalSpaceCP )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapCPtoVector )

void C_INIT_RemapCPtoVector::InitNewParticlesScalar(
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	Vector vecControlPoint;
	pParticles->GetControlPointAtTime( m_nCPInput, pParticles->m_flCurTime, &vecControlPoint );
	Vector vOutputMinLocal = m_vOutputMin;
	Vector vOutputMaxLocal = m_vOutputMax;
	if ( m_nLocalSpaceCP != -1 )
	{
		matrix3x4_t mat;
		pParticles->GetControlPointTransformAtTime( m_nLocalSpaceCP, pParticles->m_flCurTime, &mat );
		Vector vecTransformLocal = vec3_origin;
		VectorRotate( vOutputMinLocal, mat, vecTransformLocal );
		vOutputMinLocal = vecTransformLocal;
		VectorRotate( vOutputMaxLocal, mat, vecTransformLocal );
		vOutputMaxLocal = vecTransformLocal;
	}

	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		const float *pCreationTime = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		// using raw creation time to map to emitter lifespan
		float flLifeTime = *pCreationTime;  

		// only use within start/end time frame
		if ( ( ( flLifeTime < m_flStartTime ) || ( flLifeTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) )
			continue;


		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );

		Vector vOutput;
		vOutput.x = RemapValClamped( vecControlPoint.x, m_vInputMin.x, m_vInputMax.x, vOutputMinLocal.x, vOutputMaxLocal.x );
		vOutput.y = RemapValClamped( vecControlPoint.y, m_vInputMin.y, m_vInputMax.y, vOutputMinLocal.y, vOutputMaxLocal.y );
		vOutput.z = RemapValClamped( vecControlPoint.z, m_vInputMin.z, m_vInputMax.z, vOutputMinLocal.z, vOutputMaxLocal.z );		

		if ( m_bScaleInitialRange )
		{
			Vector vOrgValue;
			SetVectorFromAttribute ( vOrgValue, pOutput );
			vOutput *= vOrgValue;
		}
		if ( m_nFieldOutput == ATTRIBUTES_WHICH_ARE_COLOR_AND_OPACITY )
		{
			pOutput[0] = MAX( 0.0f, MIN( vOutput.x, 1.0f) );
			pOutput[4] = MAX( 0.0f, MIN( vOutput.y, 1.0f) );
			pOutput[8] = MAX( 0.0f, MIN( vOutput.z, 1.0f) );
		}
		else
		{
			float *pXYZ_Prev = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			Vector vXYZPrev;
			if ( m_bAccelerate )
			{
				if ( m_bOffset )
				{
					Vector vOrgValue;
					SetVectorFromAttribute ( vOrgValue, pOutput );
					SetVectorFromAttribute ( vXYZPrev, pXYZ_Prev );
					vOutput += vOrgValue;
					vXYZPrev += vOutput;
					vOutput += vOutput * pParticles->m_flDt;
					SetVectorAttribute ( pOutput, vOutput );
					SetVectorAttribute ( pXYZ_Prev, vXYZPrev );
				}
				else
				{
					vOutput *= pParticles->m_flDt;
					SetVectorAttribute ( pOutput, vOutput );
				}

			}
			else
			{
				vXYZPrev = vOutput;
				if ( m_bOffset )
				{
					Vector vOrgValue;
					SetVectorFromAttribute ( vOrgValue, pOutput );
					SetVectorFromAttribute ( vXYZPrev, pXYZ_Prev );
					vOutput += vOrgValue;
					vXYZPrev += vOutput;
					SetVectorAttribute ( pXYZ_Prev, vXYZPrev );

				}
				SetVectorAttribute ( pOutput, vOutput );
			}
		}
	}
}



class C_INIT_ChaoticAttractor : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_ChaoticAttractor );

	struct AttractorContext_t
	{
		Vector m_vecLastPosition;
	};

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nBaseCP;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		AttractorContext_t *pCtx = reinterpret_cast<AttractorContext_t *>( pContext );
		pCtx->m_vecLastPosition.Init();
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( AttractorContext_t );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	float m_flAParm;
	float m_flBParm;
	float m_flCParm;
	float m_flDParm;
	float m_flScale;
	float m_flSpeedMin;
	float m_flSpeedMax;
	int m_nBaseCP;
	bool m_bUniformSpeed;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_ChaoticAttractor, "Position From Chaotic Attractor", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_ChaoticAttractor )
DMXELEMENT_UNPACK_FIELD( "Pickover A Parameter", "-0.9629629", float, m_flAParm )
DMXELEMENT_UNPACK_FIELD( "Pickover B Parameter", "2.791139", float, m_flBParm )
DMXELEMENT_UNPACK_FIELD( "Pickover C Parameter", "1.85185185", float, m_flCParm )
DMXELEMENT_UNPACK_FIELD( "Pickover D Parameter", "1.5", float, m_flDParm )
DMXELEMENT_UNPACK_FIELD( "Speed Min", "0", float, m_flSpeedMin )
DMXELEMENT_UNPACK_FIELD( "Speed Max", "0", float, m_flSpeedMax )
DMXELEMENT_UNPACK_FIELD( "Uniform speed", "0", bool, m_bUniformSpeed )
DMXELEMENT_UNPACK_FIELD( "Relative Control point number", "0", int, m_nBaseCP )
DMXELEMENT_UNPACK_FIELD( "Scale", "1", float, m_flScale )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_ChaoticAttractor )


void C_INIT_ChaoticAttractor::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	AttractorContext_t *pCtx = reinterpret_cast<AttractorContext_t *>( pContext );

	Vector vecControlPoint1 = pParticles->GetControlPointAtCurrentTime( m_nBaseCP );
	Vector vecFwd, vecRight, vecUp;
	pParticles->GetControlPointOrientationAtCurrentTime( m_nBaseCP, &vecFwd, &vecRight, &vecUp );
	for( ; nParticleCount--; start_p++ )
	{
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		Vector vecScaledOffset = m_flScale * pCtx->m_vecLastPosition;
		Vector vecOut = vecControlPoint1 + vecScaledOffset.x * vecRight + vecScaledOffset.y * vecFwd + vecScaledOffset.z * vecUp;
		SetVectorAttribute( xyz, vecOut );
		Vector vecNewPnt;
		vecNewPnt.x = sin( m_flAParm * pCtx->m_vecLastPosition.y ) - pCtx->m_vecLastPosition.z * cos( m_flBParm * pCtx->m_vecLastPosition.x );
		vecNewPnt.z = sin( pCtx->m_vecLastPosition.x );
		vecNewPnt.y = pCtx->m_vecLastPosition.z * sin ( m_flCParm * pCtx->m_vecLastPosition.x ) - cos( m_flDParm * pCtx->m_vecLastPosition.y );
		if ( fabs( m_flSpeedMax ) > 0 )
		{
			Vector dx = vecNewPnt - pCtx->m_vecLastPosition;
			if (! m_bUniformSpeed )
			{
				float flLen = VectorLength( dx );
				if ( fabs( flLen ) > 0 )
				{
					dx *= 1. / flLen;
				}
			}
			dx *= pParticles->RandomInt( m_flSpeedMin, m_flSpeedMax );
			dx *= pParticles->m_flPreviousDt;
			SetVectorAttribute( pxyz, vecOut + dx );
		}
		else
			SetVectorAttribute( pxyz, vecOut );
		pCtx->m_vecLastPosition = vecNewPnt;
	}
}

class C_INIT_CreateFromParentParticles : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateFromParentParticles );

	struct ParentParticlesContext_t
	{
		int		m_nCurrentParentParticle;
	};

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		ParentParticlesContext_t *pCtx = reinterpret_cast<ParentParticlesContext_t *>( pContext );
		pCtx->m_nCurrentParentParticle = 0;
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( ParentParticlesContext_t );
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	float m_flVelocityScale;
	bool  m_bRandomDistribution;
	int m_nIncrement;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateFromParentParticles, "Position From Parent Particles", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreateFromParentParticles )
DMXELEMENT_UNPACK_FIELD( "Inherited Velocity Scale","0", float, m_flVelocityScale )
DMXELEMENT_UNPACK_FIELD( "Random Parent Particle Distribution","0", bool, m_bRandomDistribution )
DMXELEMENT_UNPACK_FIELD( "Particle Increment Amount","1", int, m_nIncrement )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateFromParentParticles )

void C_INIT_CreateFromParentParticles::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( !pParticles->m_pParent )
	{
		for( ; nParticleCount--; start_p++ )
		{
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

			SetVectorAttribute( xyz, vec3_origin );
			SetVectorAttribute( pxyz, vec3_origin );
		}
		return;
	}
	ParentParticlesContext_t *pCtx = reinterpret_cast<ParentParticlesContext_t *>( pContext );
	int nActiveParticles = pParticles->m_pParent->m_nActiveParticles;


	if ( nActiveParticles == 0 )
	{
		while( nParticleCount-- )
		{
			float *lifespan = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
			*lifespan = 0.0f;
			start_p++;
		}
		return;
	}		

	nActiveParticles = MAX( 0, nActiveParticles - 1 );

	for( ; nParticleCount--; start_p++ )
	{
		if ( m_bRandomDistribution )
		{
			pCtx->m_nCurrentParentParticle = pParticles->RandomInt( 0, nActiveParticles );
		}
		else if ( pCtx->m_nCurrentParentParticle > nActiveParticles )
		{
			pCtx->m_nCurrentParentParticle = 0;
		}
		float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		const float *pParent_xyz = pParticles->m_pParent->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, pCtx->m_nCurrentParentParticle );
		const float *pParent_pxyz = pParticles->m_pParent->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, pCtx->m_nCurrentParentParticle );

		Vector vecParentXYZ;
		Vector vecParentPrevXYZ;
		Vector vecScaledXYZ;

		float flPrevTime = pParticles->m_flCurTime - pParticles->m_flDt;
		float flSubFrame = RemapValClamped( *ct, flPrevTime, pParticles->m_flCurTime, 0, 1 );
		

		vecParentXYZ.x = pParent_xyz[0];
		vecParentXYZ.y = pParent_xyz[4];
		vecParentXYZ.z = pParent_xyz[8];
		vecParentPrevXYZ.x = pParent_pxyz[0];
		vecParentPrevXYZ.y = pParent_pxyz[4];
		vecParentPrevXYZ.z = pParent_pxyz[8];

		VectorLerp( vecParentPrevXYZ, vecParentXYZ, flSubFrame, vecParentXYZ );
		VectorLerp( vecParentXYZ, vecParentPrevXYZ, m_flVelocityScale, vecScaledXYZ );
		SetVectorAttribute( pxyz, vecScaledXYZ );
		SetVectorAttribute( xyz, vecParentXYZ );

		pCtx->m_nCurrentParentParticle += m_nIncrement;
	}
}






class C_INIT_InheritFromParentParticles : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_InheritFromParentParticles );

	struct ParentParticlesContext_t
	{
		int		m_nCurrentParentParticle;
	};

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	virtual void InitializeContextData( CParticleCollection *pParticles, void *pContext ) const
	{
		ParentParticlesContext_t *pCtx = reinterpret_cast<ParentParticlesContext_t *>( pContext );
		pCtx->m_nCurrentParentParticle = 0;
	}

	size_t GetRequiredContextBytes( void ) const
	{
		return sizeof( ParentParticlesContext_t );
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	float m_flScale;
	int m_nFieldOutput;
	int m_nIncrement;
	bool  m_bRandomDistribution;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_InheritFromParentParticles, "Inherit Initial Value From Parent Particle", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_InheritFromParentParticles )
DMXELEMENT_UNPACK_FIELD_USERDATA( "Inherited Field", "3", int, m_nFieldOutput, "intchoice particlefield" )
DMXELEMENT_UNPACK_FIELD( "Scale","1", float, m_flScale )
DMXELEMENT_UNPACK_FIELD( "Random Parent Particle Distribution","0", bool, m_bRandomDistribution )
DMXELEMENT_UNPACK_FIELD( "Particle Increment Amount","1", int, m_nIncrement )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_InheritFromParentParticles )

void C_INIT_InheritFromParentParticles::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( !pParticles->m_pParent )
	{
		return;
	}
	ParentParticlesContext_t *pCtx = reinterpret_cast<ParentParticlesContext_t *>( pContext );
	int nActiveParticles = pParticles->m_pParent->m_nActiveParticles;


	if ( nActiveParticles == 0 )
	{
		return;
	}		

	nActiveParticles = MAX( 0, nActiveParticles - 1 );

	for( ; nParticleCount--; start_p++ )
	{
		if ( m_bRandomDistribution )
		{
			pCtx->m_nCurrentParentParticle = pParticles->RandomInt( 0, nActiveParticles );
		}
		else if ( pCtx->m_nCurrentParentParticle > nActiveParticles )
		{
			pCtx->m_nCurrentParentParticle = 0;
		}
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		const float *pParentValue = pParticles->m_pParent->GetFloatAttributePtr( m_nFieldOutput, pCtx->m_nCurrentParentParticle );

		if ( ATTRIBUTES_WHICH_ARE_VEC3S_MASK & ( 1 << m_nFieldOutput ) )
		{
			Vector vecParentValue;
			SetVectorFromAttribute( vecParentValue, pParentValue );
			vecParentValue *= m_flScale;

			// Clamp to 0-1 if color
			if ( ATTRIBUTES_WHICH_ARE_COLOR_AND_OPACITY & ( 1 << m_nFieldOutput ) )
			{
				vecParentValue.Min( Vector( 1, 1, 1 ) );
				vecParentValue.Max( Vector( 0, 0, 0 ) );				
			}
			SetVectorAttribute( pOutput, vecParentValue );
		}
		else
		{
			float flOutput = *pParentValue * m_flScale;
			if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & ( 1 << m_nFieldOutput ) )
			{
				flOutput = clamp(flOutput, 0.0f, 1.0f );
			}
			*pOutput = flOutput;
		}

		pCtx->m_nCurrentParentParticle += m_nIncrement;
	}
}


//-----------------------------------------------------------------------------
// Distance to CP Initializer
//-----------------------------------------------------------------------------
class C_INIT_DistanceToCPInit : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_DistanceToCPInit );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nStartCP;
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nCollisionGroupNumber = g_pParticleSystemMgr->Query()->GetCollisionGroupFromName( m_CollisionGroupName );
		m_nStartCP = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nStartCP ) );

		// clamp the result to 0 and 1 if it's alpha
		if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & ( 1 << m_nFieldOutput ) )
		{
			m_flOutputMin = clamp(m_flOutputMin, 0.0f, 1.0f );
			m_flOutputMax = clamp(m_flOutputMax, 0.0f, 1.0f );
		}
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nFieldOutput;
	float	m_flInputMin;
	float	m_flInputMax;
	float	m_flOutputMin;
	float	m_flOutputMax;
	int		m_nStartCP;
	bool	m_bLOS;
	char	m_CollisionGroupName[128];
	int		m_nCollisionGroupNumber;
	float	m_flMaxTraceLength;
	float	m_flLOSScale;
	bool	m_bScaleInitialRange;
	bool	m_bActiveRange;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_DistanceToCPInit, "Remap Initial Distance to Control Point to Scalar", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_DistanceToCPInit )
DMXELEMENT_UNPACK_FIELD( "distance minimum","0", float, m_flInputMin )
DMXELEMENT_UNPACK_FIELD( "distance maximum","128", float, m_flInputMax )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
DMXELEMENT_UNPACK_FIELD( "control point","0", int, m_nStartCP )
DMXELEMENT_UNPACK_FIELD( "ensure line of sight","0", bool, m_bLOS )
DMXELEMENT_UNPACK_FIELD_STRING( "LOS collision group", "NONE", m_CollisionGroupName )
DMXELEMENT_UNPACK_FIELD( "Maximum Trace Length", "-1", float, m_flMaxTraceLength )
DMXELEMENT_UNPACK_FIELD( "LOS Failure Scalar", "0", float, m_flLOSScale )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
DMXELEMENT_UNPACK_FIELD( "only active within specified distance","0", bool, m_bActiveRange )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_DistanceToCPInit )

void C_INIT_DistanceToCPInit::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	Vector vecControlPoint1 = pParticles->GetControlPointAtCurrentTime( m_nStartCP );
	// FIXME: SSE-ize
	for( ; nParticleCount--; start_p++ )
	{
		Vector vecPosition2;
		const float *pXYZ = pParticles->GetFloatAttributePtr(PARTICLE_ATTRIBUTE_XYZ, start_p );
		vecPosition2 = Vector(pXYZ[0], pXYZ[4], pXYZ[8]); 
		Vector vecDelta = vecControlPoint1 - vecPosition2;
		float flDistance = vecDelta.Length();
		if ( m_bActiveRange && ( flDistance < m_flInputMin || flDistance > m_flInputMax ) )
		{
			continue;
		}
		if ( m_bLOS )
		{
			Vector vecEndPoint = vecPosition2;
			if ( m_flMaxTraceLength != -1.0f && m_flMaxTraceLength < flDistance )
			{
				VectorNormalize(vecEndPoint);
				vecEndPoint *= m_flMaxTraceLength;
				vecEndPoint += vecControlPoint1;
			}
			CBaseTrace tr;
			g_pParticleSystemMgr->Query()->TraceLine( vecControlPoint1, vecEndPoint, MASK_OPAQUE_AND_NPCS, NULL , m_nCollisionGroupNumber, &tr );
			if (tr.fraction != 1.0f)
			{
				flDistance *= tr.fraction * m_flLOSScale;
			}

		}

		float flOutput = RemapValClamped( flDistance, m_flInputMin, m_flInputMax, m_flOutputMin, m_flOutputMax  );
		if ( m_bScaleInitialRange )
		{
			const float *pInitialOutput = pParticles->GetFloatAttributePtr( m_nFieldOutput, start_p );
			flOutput = *pInitialOutput * flOutput;
		}
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );

		*pOutput = flOutput;
	}
}


class C_INIT_LifespanFromVelocity : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_LifespanFromVelocity );

	Vector m_vecComponentScale;
	float m_flTraceOffset;
	float m_flMaxTraceLength;
	float m_flTraceTolerance;
	int m_nCollisionGroupNumber;
	int m_nMaxPlanes;
	int m_nAllowedPlanes;
	char	m_CollisionGroupName[128];
	unsigned int m_CollisionMask;
	bool m_bIncludeWater;

	

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	void InitializeContextData( CParticleCollection *pParticles,
		void *pContext ) const
	{
	}

	size_t GetRequiredContextBytes( ) const
	{
		return sizeof( CWorldCollideContextData );
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nCollisionGroupNumber = g_pParticleSystemMgr->Query()->GetCollisionGroupFromName( m_CollisionGroupName );
		m_nAllowedPlanes = ( MIN( MAX_WORLD_PLANAR_CONSTRAINTS, m_nMaxPlanes ) - 1 );
		if ( m_bIncludeWater )
			m_CollisionMask = MASK_SHOT_HULL|MASK_SPLITAREAPORTAL;
		else
			m_CollisionMask = MASK_SHOT_HULL;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_LifespanFromVelocity, "Lifetime from Time to Impact", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_LifespanFromVelocity )
DMXELEMENT_UNPACK_FIELD_STRING( "trace collision group", "NONE", m_CollisionGroupName )
DMXELEMENT_UNPACK_FIELD( "maximum trace length", "1024", float, m_flMaxTraceLength )
DMXELEMENT_UNPACK_FIELD( "trace offset", "0", float, m_flTraceOffset )
DMXELEMENT_UNPACK_FIELD( "trace recycle tolerance", "64", float, m_flTraceTolerance )
DMXELEMENT_UNPACK_FIELD( "maximum points to cache", "16", int, m_nMaxPlanes )
DMXELEMENT_UNPACK_FIELD( "bias distance", "1 1 1", Vector, m_vecComponentScale )
DMXELEMENT_UNPACK_FIELD( "collide with water", "1", bool, m_bIncludeWater )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_LifespanFromVelocity )


void C_INIT_LifespanFromVelocity::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	CWorldCollideContextData **ppCtx;
	if ( pParticles->m_pParent )
		ppCtx = &( pParticles->m_pParent->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );
	else
		ppCtx = &( pParticles->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );

	CWorldCollideContextData *pCtx = NULL;
	if ( ! *ppCtx )
	{
		*ppCtx = new CWorldCollideContextData;
		(*ppCtx)->m_nActivePlanes = 0;
		(*ppCtx)->m_nActivePlanes = 0;
		(*ppCtx)->m_nNumFixedPlanes = 0;
	}
	pCtx = *ppCtx;

	float flTol = m_flTraceTolerance * m_flTraceTolerance;

	//Trace length takes the max trace and subtracts the offset to get the actual total.
	float flTotalTraceDist = m_flMaxTraceLength - m_flTraceOffset;

	//Offset percentage to account for if we've hit something within the offset (but not spawn) area
	float flOffsetPct = m_flMaxTraceLength / ( flTotalTraceDist + FLT_EPSILON );

	FourVectors v4ComponentScale;
	v4ComponentScale.DuplicateVector( m_vecComponentScale );
	fltx4 fl4TraceOffset = ReplicateX4( m_flTraceOffset );

	for( ; nParticleCount--; start_p++ )
	{
		float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pPrevXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

		float *dtime = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );

		if ( *dtime >= 0.0f )
		{
			Vector vecXYZ;
			Vector vecXYZ_Prev;
			SetVectorFromAttribute( vecXYZ, pxyz );
			SetVectorFromAttribute( vecXYZ_Prev, pPrevXYZ );

			//Calculate velocity and account for frame delta time
			Vector vDelta = ( vecXYZ - vecXYZ_Prev ) / pParticles->m_flPreviousDt;
			float flVelocity = VectorLength( vDelta );

			//Normalize the delta and get the offset to use from the normalized delta times the offset
			VectorNormalize( vDelta );
			Vector vecOffset = vDelta * m_flTraceOffset;

			Vector vecStartPnt = vecXYZ + vecOffset;
			Vector vecEndPnt = ( vDelta * flTotalTraceDist ) + vecStartPnt;

			// Use SIMD section to interface with plane cache, even though we're not SIMD here
			// Test versus existing Data
			FourVectors fvStartPnt;
			fvStartPnt.DuplicateVector( vecStartPnt );
			FourVectors fvEndPnt;
			fvEndPnt.DuplicateVector( vecEndPnt );
			FourVectors v4PointOnPlane;
			FourVectors v4PlaneNormal;
			FourVectors v4Delta;
			fltx4 fl4ClosestDist = Four_FLT_MAX;
			for( int i = 0 ; i < pCtx->m_nActivePlanes; i++ )
			{
				if ( pCtx->m_bPlaneActive[i] )
				{
					fltx4 fl4TrialDistance = MaxSIMD( 
						fvStartPnt.DistToSqr( pCtx->m_TraceStartPnt[i] ),
						fvEndPnt.DistToSqr( pCtx->m_TraceEndPnt[i] ) );
					//fltx4 fl4TrialDistance = MaxSIMD( 
					//	fvStartPnt.DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ),
					//	fvEndPnt.DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ) );
					// If the trial distance is closer than the existing closest, replace.
					if ( !IsAllGreaterThan( fl4TrialDistance, fl4ClosestDist ) )
					{
						fl4ClosestDist = fl4TrialDistance;
						v4PointOnPlane = pCtx->m_PointOnPlane[i];
					}
				}
			}
			// If we're outside the tolerance range, do a new trace and store it.
			if ( IsAllGreaterThan( fl4ClosestDist, ReplicateX4( flTol ) ) )
			{
				//replace this with fast raycaster when available
				CBaseTrace tr;
				g_pParticleSystemMgr->Query()->TraceLine( vecStartPnt, vecEndPnt, m_CollisionMask, NULL , m_nCollisionGroupNumber, &tr );

				Vector vNormal = tr.plane.normal;
				Vector vPointOnPlane =  tr.endpos;

				// If we hit nothing, invalidate our hit normal
				if ( tr.fraction == 1.0f )
					vNormal = vec3_invalid;
				// If we start solid, our trace distance is 0, or we hit within the offset area
				// cache off anyway, but set our impact and normal to be invalid
				if ( ( tr.fraction <= ( 1 - flOffsetPct ) ) || tr.startsolid || flTotalTraceDist == 0.0f || tr.allsolid )
				{
					vPointOnPlane = vec3_invalid;
					vNormal = vec3_invalid;
				}
				
				// Cache off trace
				int nIndex = pCtx->m_nNumFixedPlanes;
				pCtx->m_bPlaneActive[nIndex] = true;
				pCtx->m_PointOnPlane[nIndex].DuplicateVector( vPointOnPlane );
				pCtx->m_PlaneNormal[nIndex].DuplicateVector( vNormal );
				pCtx->m_TraceStartPnt[nIndex].DuplicateVector( vecStartPnt );
				pCtx->m_TraceEndPnt[nIndex].DuplicateVector( vecEndPnt );

				fvStartPnt.DuplicateVector( vecStartPnt );
				v4PointOnPlane.DuplicateVector( vPointOnPlane );

				pCtx->m_nNumFixedPlanes = pCtx->m_nNumFixedPlanes + 1;
				if ( pCtx->m_nNumFixedPlanes > m_nAllowedPlanes )
					pCtx->m_nNumFixedPlanes = 0;
				pCtx->m_nActivePlanes = MIN( m_nAllowedPlanes, pCtx->m_nActivePlanes + 1 );
			}
			
			// If we have an invalid trace, kill us, otherwise do the math.
			if ( v4PointOnPlane.Vec( 0 ) == vec3_invalid )
			{
				*dtime = -1.0f;
			}
			else
			{
				fvStartPnt -= v4PointOnPlane;
				//Scale components to remove undesired axis
				fvStartPnt *= v4ComponentScale;
				//Find the length of the trace
				//Need to use the adjusted value of the trace length and collision point to account for the offset
				fltx4 fl4Dist = AddSIMD ( fvStartPnt.length(), fl4TraceOffset );
				flVelocity += FLT_EPSILON;
				//Divide by Velocity to get Lifespan
				float flLifeUpdate;
				flLifeUpdate = SubFloat( fl4Dist, 0) / flVelocity;
				*dtime = flLifeUpdate;
			}
		}
	}
}


void C_INIT_LifespanFromVelocity::InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const
{
	CWorldCollideContextData **ppCtx;
	if ( pParticles->m_pParent )
		ppCtx = &( pParticles->m_pParent->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );
	else
		ppCtx = &( pParticles->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );

	CWorldCollideContextData *pCtx = NULL;
	if ( ! *ppCtx )
	{
		*ppCtx = new CWorldCollideContextData;
		(*ppCtx)->m_nActivePlanes = 0;
		(*ppCtx)->m_nActivePlanes = 0;
		(*ppCtx)->m_nNumFixedPlanes = 0;
	}
	pCtx = *ppCtx;

	float flTol = m_flTraceTolerance * m_flTraceTolerance;

	//Trace length takes the max trace and subtracts the offset to get the actual total.
	float flTotalTraceDist = m_flMaxTraceLength - m_flTraceOffset;
	fltx4 fl4TotalTraceDist = ReplicateX4( flTotalTraceDist );

	//Offset percentage to account for if we've hit something within the offset (but not spawn) area
	float flOffsetPct = m_flMaxTraceLength / ( flTotalTraceDist + FLT_EPSILON );

	FourVectors v4ComponentScale;
	v4ComponentScale.DuplicateVector( m_vecComponentScale );
	fltx4 fl4TraceOffset = ReplicateX4( m_flTraceOffset );

	fltx4 fl4PrevDT = ReplicateX4( pParticles->m_flPreviousDt );	


	size_t attr_stride;

	const FourVectors *pXYZ = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &attr_stride );
	pXYZ += attr_stride * start_block;
	const FourVectors *pPrev_XYZ = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, &attr_stride );
	pPrev_XYZ += attr_stride * start_block;
	fltx4 *pLifespan = pParticles->GetM128AttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, &attr_stride );
	pLifespan += attr_stride * start_block;

	while( n_blocks-- )
	{
		fltx4 fl4Life = *pLifespan;
		bi32x4 fl4PassMask = CmpGeSIMD( fl4Life, Four_Zeros );
		bi32x4 fl4SkipMask = CmpLtSIMD( fl4Life, Four_Zeros );
		if ( IsAnyTrue( fl4PassMask ) )
		{
			// Determine Velocity
			FourVectors fvDelta = *pXYZ;
			fvDelta -= *pPrev_XYZ;
			fvDelta *= ReciprocalSIMD ( fl4PrevDT );
			fltx4 fl4Velocity = fvDelta.length();
			//fl4Velocity = MulSIMD ( fl4Velocity, fl4PrevDT );
			//fl4Velocity = DivSIMD ( fl4Velocity, fl4PrevDT );

			//Normalize the delta and get the offset to use from the normalized delta times the offset
			FourVectors fvDeltaNormalized = fvDelta;
			fvDeltaNormalized.VectorNormalizeFast();
			FourVectors fvOffset = fvDeltaNormalized;
			fvOffset *= m_flTraceOffset;

			//Start/Endpoints for our traces
			FourVectors fvStartPnt = *pXYZ;
			fvStartPnt += fvOffset;
			FourVectors fvEndPnt = fvDeltaNormalized;
			fvEndPnt *= fl4TotalTraceDist;
			fvEndPnt += fvStartPnt;

			// Test versus existing Data
			FourVectors v4PointOnPlane;
			FourVectors v4PlaneNormal;
			fltx4 fl4ClosestDist = Four_FLT_MAX;

			fl4ClosestDist = MaskedAssign( fl4SkipMask, Four_Zeros, fl4ClosestDist );
			for( int i = 0 ; i < pCtx->m_nActivePlanes; i++ )
			{
				if ( pCtx->m_bPlaneActive[i] )
				{
					fltx4 fl4TrialDistance = MaxSIMD( 
						fvStartPnt.DistToSqr( pCtx->m_TraceStartPnt[i] ),
						fvEndPnt.DistToSqr( pCtx->m_TraceEndPnt[i] ) );
					//fltx4 fl4TrialDistance = MaxSIMD( 
					//	fvStartPnt.DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ),
					//	fvEndPnt.DistSqrToLineSegment( pCtx->m_TraceStartPnt[i], pCtx->m_TraceEndPnt[i] ) );
					bi32x4 fl4Nearestmask = CmpLeSIMD( fl4TrialDistance, fl4ClosestDist );
					fl4ClosestDist = MaskedAssign( fl4Nearestmask, fl4TrialDistance, fl4ClosestDist );
					v4PointOnPlane.x = MaskedAssign( fl4Nearestmask, pCtx->m_PointOnPlane[i].x, v4PointOnPlane.x );
					v4PointOnPlane.y = MaskedAssign( fl4Nearestmask, pCtx->m_PointOnPlane[i].y, v4PointOnPlane.y );
					v4PointOnPlane.z = MaskedAssign( fl4Nearestmask, pCtx->m_PointOnPlane[i].z, v4PointOnPlane.z );
				}
			}
			// If we're outside the tolerance range, do a new trace and store it.
			bi32x4 fl4OutOfRange = CmpGtSIMD( fl4ClosestDist, ReplicateX4( flTol ) );
			if ( IsAnyTrue( fl4OutOfRange ) )
			{
				int nMask = TestSignSIMD( fl4OutOfRange );
				for(int i=0; i < 4; i++ )
				{
					if ( nMask & ( 1 << i ) )
					{
						Vector start = fvStartPnt.Vec( i );
						Vector end = fvEndPnt.Vec( i );

						//replace this with fast raycaster when available
						CBaseTrace tr;
						g_pParticleSystemMgr->Query()->TraceLine( start, end, m_CollisionMask, NULL , m_nCollisionGroupNumber, &tr );  

						Vector vNormal = tr.plane.normal;
						Vector vPointOnPlane =  tr.endpos;

						// If we hit nothing, invalidate our hit normal
						if ( tr.fraction == 1.0f )
							vNormal = vec3_invalid;
						// If we start solid, our trace distance is 0, or we hit within the offset area
						// cache off anyway, but set our impact and normal to be invalid
						if ( ( tr.fraction <= ( 1 - flOffsetPct ) ) || tr.startsolid || flTotalTraceDist == 0.0f || tr.allsolid )
						{
							vPointOnPlane = vec3_invalid;
							vNormal = vec3_invalid;
						}

						SubFloat( v4PointOnPlane.x, i ) = vPointOnPlane.x;
						SubFloat( v4PointOnPlane.y, i ) = vPointOnPlane.y;
						SubFloat( v4PointOnPlane.z, i ) = vPointOnPlane.z;

						// Cache off trace
						int nIndex = pCtx->m_nNumFixedPlanes;
						pCtx->m_bPlaneActive[nIndex] = true;
						pCtx->m_PointOnPlane[nIndex].DuplicateVector( vPointOnPlane );
						pCtx->m_PlaneNormal[nIndex].DuplicateVector( vNormal );
						pCtx->m_TraceStartPnt[nIndex].DuplicateVector( start );
						pCtx->m_TraceEndPnt[nIndex].DuplicateVector( end );

						pCtx->m_nNumFixedPlanes = pCtx->m_nNumFixedPlanes + 1;
						if ( pCtx->m_nNumFixedPlanes > m_nAllowedPlanes )
							pCtx->m_nNumFixedPlanes = 0;
						pCtx->m_nActivePlanes = MIN( m_nAllowedPlanes, pCtx->m_nActivePlanes + 1 );
					}
				}
			}

			FourVectors fvInvalid;
			fvInvalid.DuplicateVector( vec3_invalid );
			bi32x4 fl4InvalidMask = CmpEqSIMD( v4PointOnPlane.x, fvInvalid.x );
			fl4InvalidMask = OrSIMD (fl4InvalidMask, CmpEqSIMD( v4PointOnPlane.y, fvInvalid.y ) );
			fl4InvalidMask = OrSIMD (fl4InvalidMask, CmpEqSIMD( v4PointOnPlane.z, fvInvalid.z ) );

			//Find the length of the trace
			fvStartPnt -= v4PointOnPlane;
			fvStartPnt *= v4ComponentScale;

			//Need to use the adjusted value of the trace length and collision point to account for the offset
			fltx4 fl4Dist = AddSIMD ( fvStartPnt.length(), fl4TraceOffset );
			fl4Velocity = AddSIMD( fl4Velocity, Four_Epsilons );

			//Divide by Velocity to get Lifespan
			fltx4 fl4Life = DivSIMD( fl4Dist, fl4Velocity );
			fl4Life = MaskedAssign( fl4InvalidMask, Four_Zeros, fl4Life );
			*pLifespan = MaskedAssign( fl4PassMask, fl4Life, *pLifespan );
		}
		pXYZ += attr_stride;
		pPrev_XYZ += attr_stride;
		pLifespan += attr_stride;
	}
}



class C_INIT_CreateFromPlaneCache : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_CreateFromPlaneCache );

	Vector m_vecOffsetMin;
	Vector m_vecOffsetMax;
	bool bLocalOffset;
	bool m_bUseNormal;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_LIFE_DURATION_MASK | PARTICLE_ATTRIBUTE_NORMAL_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	size_t GetRequiredContextBytes( ) const
	{
		return sizeof( CWorldCollideContextData );
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		bLocalOffset = m_vecOffsetMin != vec3_origin && m_vecOffsetMax != vec3_origin;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_CreateFromPlaneCache, "Position from Parent Cache", OPERATOR_PI_POSITION );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_CreateFromPlaneCache )
DMXELEMENT_UNPACK_FIELD( "Local Offset Min", "0 0 0", Vector, m_vecOffsetMin )
DMXELEMENT_UNPACK_FIELD( "Local Offset Max", "0 0 0", Vector, m_vecOffsetMax )
DMXELEMENT_UNPACK_FIELD( "Set Normal", "0", bool, m_bUseNormal )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_CreateFromPlaneCache )

void C_INIT_CreateFromPlaneCache::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( !pParticles->m_pParent )
	{
		for( ; nParticleCount--; start_p++ )
		{
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );

			SetVectorAttribute( xyz, vec3_origin );
			SetVectorAttribute( pxyz, vec3_origin );
		}
		return;
	}


	CWorldCollideContextData **ppCtx;
	if ( pParticles->m_pParent )
		ppCtx = &( pParticles->m_pParent->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );
	else
		ppCtx = &( pParticles->m_pCollisionCacheData[COLLISION_MODE_INITIAL_TRACE_DOWN] );

	CWorldCollideContextData *pCtx = NULL;
	if ( ! *ppCtx )
	{
		*ppCtx = new CWorldCollideContextData;
		(*ppCtx)->m_nActivePlanes = 0;
		(*ppCtx)->m_nNumFixedPlanes = 0;
		FourVectors fvEmpty;
		fvEmpty.DuplicateVector( vec3_origin );
		(*ppCtx)->m_PointOnPlane[0] = fvEmpty;
	}
	pCtx = *ppCtx;
	if ( pCtx->m_nActivePlanes > 0 )
	{
		for( ; nParticleCount--; start_p++ )
		{ 
			int nIndex = pParticles->RandomInt( 0, pCtx->m_nActivePlanes - 1 );
			Vector vecNormal = pCtx->m_PlaneNormal[nIndex].Vec( 0 );
			Vector vecPointOnPlane = pCtx->m_PointOnPlane[nIndex].Vec( 0 );
			if ( vecNormal == vec3_invalid || vecPointOnPlane == vec3_invalid )
			{
				float *plifespan = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
				*plifespan = 0.0f;
			}
			else
			{
				float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
				float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
				FourVectors fvPoint = pCtx->m_PointOnPlane[nIndex];
				Vector vPoint = fvPoint.Vec( 0 );
				if ( bLocalOffset )
				{
					Vector randpos;
					Vector OutputPos;
					QAngle LocalCoords;
					pParticles->RandomVector( m_vecOffsetMin, m_vecOffsetMax, &randpos );
					randpos = Vector( randpos.z, randpos.y, randpos.x );
					VectorAngles( vecNormal, LocalCoords );
					VectorRotate( randpos, LocalCoords, OutputPos );
					vPoint += OutputPos;
				}

				SetVectorAttribute( xyz, vPoint );
				SetVectorAttribute( pxyz, vPoint );
				if ( m_bUseNormal )
				{
					float *pNormal = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_NORMAL, start_p );
					SetVectorAttribute( pNormal, vecNormal );
				}
			}
		}
	}
	else
	{
		for( ; nParticleCount--; start_p++ )
		{ 
			float *xyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pxyz = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			SetVectorAttribute( xyz, vec3_origin );
			SetVectorAttribute( pxyz, vec3_origin );
		}
	}
}


//-----------------------------------------------------------------------------
// Model Cull Operator - cull particles inside or outside of a brush/animated model
//-----------------------------------------------------------------------------
class C_INIT_ModelCull : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_ModelCull );

	int m_nControlPointNumber;
	bool m_bBoundBox;
	bool m_bCullOutside;
	char m_HitboxSetName[128];

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_ModelCull , "Cull relative to model", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_ModelCull ) 
	DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
	DMXELEMENT_UNPACK_FIELD( "use only bounding box", "0", bool, m_bBoundBox )
	DMXELEMENT_UNPACK_FIELD( "cull outside instead of inside", "0", bool, m_bCullOutside )
	DMXELEMENT_UNPACK_FIELD_STRING( "hitbox set", "effects", m_HitboxSetName )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_ModelCull )

void C_INIT_ModelCull::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	pParticles->UpdateHitBoxInfo( m_nControlPointNumber, m_HitboxSetName );
	if ( pParticles->ControlPointHitBox( m_nControlPointNumber ).CurAndPrevValid() )
	{
		for( ; nParticleCount--; start_p++ )
		{
			float *pXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pLifespan = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
			Vector vecParticlePosition;

			SetVectorFromAttribute( vecParticlePosition, pXYZ );

			bool bInside = g_pParticleSystemMgr->Query()->IsPointInControllingObjectHitBox( pParticles, m_nControlPointNumber, vecParticlePosition, m_bBoundBox );
			if ( ( bInside && m_bCullOutside ) || ( !bInside && !m_bCullOutside ))
				continue;

			*pLifespan = -1.0f;
		}
	}
}


//-----------------------------------------------------------------------------
// RtEnv Cull Operator - cull particles inside or outside of a RayTraceEnvironment
//-----------------------------------------------------------------------------
class C_INIT_RtEnvCull : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RtEnvCull );

	Vector m_vecTestDir;
	Vector m_vecTestNormal;
	int m_nRtEnvNumber;
	bool m_bUseVelocity;
	bool m_bCullOnMiss;
	bool m_bLifeAdjust;
	char m_RtEnvName[128];

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_LIFE_DURATION;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nRtEnvNumber = g_pParticleSystemMgr->Query()->GetRayTraceEnvironmentFromName( m_RtEnvName );
	}

	bool InitMultipleOverride ( void ) { return true; }

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RtEnvCull , "Cull relative to Ray Trace Environment", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RtEnvCull ) 
DMXELEMENT_UNPACK_FIELD( "cull on miss", "0", bool, m_bCullOnMiss )
DMXELEMENT_UNPACK_FIELD( "velocity test adjust lifespan", "0", bool, m_bLifeAdjust )
DMXELEMENT_UNPACK_FIELD( "use velocity for test direction", "0", bool, m_bUseVelocity )
DMXELEMENT_UNPACK_FIELD( "test direction", "0 0 1", Vector, m_vecTestDir )
DMXELEMENT_UNPACK_FIELD( "cull normal", "0 0 0", Vector, m_vecTestNormal )
DMXELEMENT_UNPACK_FIELD_STRING( "ray trace environment name", "PRECIPITATION", m_RtEnvName )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RtEnvCull )

void C_INIT_RtEnvCull::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	int32 skipid = NULL;
	bool bUseNormal = m_vecTestNormal != vec3_origin;
	if ( m_bUseVelocity )
	{
		fltx4 fl4PrevDt = ReplicateX4( pParticles->m_flPreviousDt );
		for( ; nParticleCount--; start_p++ )
		{
			float *pXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pXYZPrev = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			float *pLifespan = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
			if ( *pLifespan == -1.0f )
				continue;

			Vector vecParticlePosition;
			Vector vecParticlePositionPrev;
			SetVectorFromAttribute( vecParticlePosition, pXYZ );
			SetVectorFromAttribute( vecParticlePositionPrev, pXYZPrev );

			FourVectors fvDirection;
			fvDirection.DuplicateVector( vecParticlePosition - vecParticlePositionPrev );
			fltx4 fl4Velocity = fvDirection.length();
			fl4Velocity = DivSIMD ( fl4Velocity, fl4PrevDt );

			fvDirection.VectorNormalizeFast();

			FourRays frTr;
			frTr.direction = fvDirection;
			frTr.origin.DuplicateVector( vecParticlePosition );
			fltx4 fl4TMin = Four_Zeros;
			fltx4 fl4Lifespan = ReplicateX4( *pLifespan );
			fltx4 fl4TMax = MulSIMD ( fl4Velocity, fl4Lifespan );
			RayTracingResult Result;

			g_pParticleSystemMgr->Query()->TraceAgainstRayTraceEnv( m_nRtEnvNumber, frTr, fl4TMin, fl4TMax, &Result, skipid );
			bool bHit = Result.HitIds[0] != -1;
			if ( bUseNormal )
			{
				bHit = bHit && Result.surface_normal.Vec( 0 ) == m_vecTestNormal;
			}
			if ( m_bLifeAdjust && bHit )
			{
				fl4Velocity = AddSIMD( fl4Velocity, Four_Epsilons );
				fl4Lifespan = DivSIMD( Result.HitDistance, fl4Velocity );
				//debug
				//if ( SubFloat( fl4Lifespan, 0) != -1.0f  && Result.HitIds[0] != -1 )
				//{
				//	g_pParticleSystemMgr->Query()->DebugDrawLine( frTr.origin.Vec( 0 ), frTr.origin.Vec( 0 ) + ( frTr.direction.Vec ( 0 ) * SubFloat( Result.HitDistance, 0 ) ), 0, 0, 255, true, -1 );
				//	Vector vecEnd = frTr.origin.Vec( 0 ) + ( frTr.direction.Vec( 0 ) * ( SubFloat( fl4Lifespan, 0 ) * SubFloat( fl4Velocity, 0 ) ) ) ;
				//	g_pParticleSystemMgr->Query()->DebugDrawLine( frTr.origin.Vec( 0 ), vecEnd, 0, 255, 0, true, -1 );
				//}
				*pLifespan = MIN( SubFloat( fl4Lifespan, 0), *pLifespan );
				continue;
			}
			if ( ( bHit && m_bCullOnMiss ) || ( !bHit && !m_bCullOnMiss ) )
				continue;

			*pLifespan = -1.0f;
		}
	}
	else
	{
		for( ; nParticleCount--; start_p++ )
		{
			float *pXYZ = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_XYZ, start_p );
			float *pLifespan = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, start_p );
			if ( *pLifespan == -1.0f )
				continue;
			Vector vecParticlePosition;
			SetVectorFromAttribute( vecParticlePosition, pXYZ );

			FourRays frTr;
			frTr.direction.DuplicateVector( m_vecTestDir );
			frTr.origin.DuplicateVector( vecParticlePosition );
			fltx4 fl4TMin = Four_Zeros;
			fltx4 fl4TMax = Four_FLT_MAX;

			RayTracingResult Result;


			g_pParticleSystemMgr->Query()->TraceAgainstRayTraceEnv( m_nRtEnvNumber, frTr, fl4TMin, fl4TMax, &Result, skipid );
			bool bHit = Result.HitIds[0] != -1;
			if ( bUseNormal )
			{
				bHit = bHit && Result.surface_normal.Vec( 0 ) == m_vecTestNormal;
			}

			if ( ( bHit && m_bCullOnMiss && !bUseNormal ) || ( !bHit && !m_bCullOnMiss ))
				continue;

			*pLifespan = -1.0f;
		}
	}
}


void C_INIT_RtEnvCull::InitNewParticlesBlock( CParticleCollection *pParticles, 
						int start_block, int n_blocks, int nAttributeWriteMask,
						void *pContext ) const
{
	int32 skipid = NULL;
	bool bUseNormal = m_vecTestNormal != vec3_origin;
	size_t attr_stride;

	const FourVectors *pXYZ = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &attr_stride );
	pXYZ += attr_stride * start_block;
	const FourVectors *pXYZPrev = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, &attr_stride );
	pXYZPrev += attr_stride * start_block;
	fltx4 *pLifespan = pParticles->GetM128AttributePtrForWrite( PARTICLE_ATTRIBUTE_LIFE_DURATION, &attr_stride );
	pLifespan += attr_stride * start_block;

	bi32x4 fl4HitMask;
	fltx4 fl4Lifespan;

	if ( m_bUseVelocity )
	{
		fltx4 fl4PrevDt = ReplicateX4( pParticles->m_flPreviousDt );
		while( n_blocks-- )
		{
			FourVectors fvDirection = *pXYZ;
			fvDirection -= *pXYZPrev;
			fltx4 fl4Velocity = fvDirection.length();
			fl4Velocity = DivSIMD ( fl4Velocity, fl4PrevDt );

			fl4Lifespan = *pLifespan;

			if ( IsAllEqual( fl4Lifespan, Four_NegativeOnes ) )
			{
				pXYZ += attr_stride;
				pXYZPrev += attr_stride;
				pLifespan += attr_stride;
				continue;
			}

			bi32x4 fl4SkipMask = CmpEqSIMD( fl4Lifespan, Four_NegativeOnes );

			fvDirection.VectorNormalize();

			FourRays frTr;
			frTr.direction = fvDirection;
			frTr.origin = *pXYZ;
			fltx4 fl4TMin = Four_Zeros;
			fltx4 fl4TMax = MulSIMD ( fl4Velocity, fl4Lifespan );

			RayTracingResult Result;

			g_pParticleSystemMgr->Query()->TraceAgainstRayTraceEnv( m_nRtEnvNumber, frTr, fl4TMin, fl4TMax, &Result, skipid );

			i32x4 in4HitIds = LoadAlignedIntSIMD( Result.HitIds );
			fltx4 fl4HitIds = SignedIntConvertToFltSIMD ( in4HitIds );
			
			if ( m_bCullOnMiss )
			{
				fl4HitMask = CmpLtSIMD( fl4HitIds, Four_Zeros);
				if ( bUseNormal )
				{
					FourVectors fvCullNormal;
					fvCullNormal.DuplicateVector( m_vecTestNormal );
					bi32x4 fl4NormalMask;
					fl4NormalMask = CmpEqSIMD( fvCullNormal.x, Result.surface_normal.x );
					fl4NormalMask = AndSIMD( fl4NormalMask, CmpEqSIMD( fvCullNormal.y, Result.surface_normal.y ) );
					fl4NormalMask = AndSIMD( fl4NormalMask, CmpEqSIMD( fvCullNormal.z, Result.surface_normal.z ) );
					fl4HitMask = OrSIMD( fl4HitMask, fl4NormalMask );
				}
			}
			else
			{
				fl4HitMask = CmpGeSIMD( fl4HitIds, Four_Zeros);
				if ( bUseNormal )
				{
					FourVectors fvCullNormal;
					fvCullNormal.DuplicateVector( m_vecTestNormal );
					bi32x4 fl4NormalMask;
					fl4NormalMask = CmpEqSIMD( fvCullNormal.x, Result.surface_normal.x );
					fl4NormalMask = AndSIMD( fl4NormalMask, CmpEqSIMD( fvCullNormal.y, Result.surface_normal.y ) );
					fl4NormalMask = AndSIMD( fl4NormalMask, CmpEqSIMD( fvCullNormal.z, Result.surface_normal.z ) );
					fl4HitMask = AndSIMD( fl4HitMask, fl4NormalMask );
				}
			}

			if ( m_bLifeAdjust )
			{
				if ( IsAnyTrue( fl4HitMask ) )
				{
					fl4Velocity = AddSIMD( fl4Velocity, Four_Epsilons );
					fl4Lifespan = DivSIMD( Result.HitDistance, fl4Velocity );
					//debug
					//for ( int i=0; i<4; ++i )
					//{
					//	if ( SubFloat( fl4Lifespan, i) != -1.0f  && Result.HitIds[i] != -1 )
					//	{
					//		g_pParticleSystemMgr->Query()->DebugDrawLine( frTr.origin.Vec( i ), frTr.origin.Vec( i ) + ( frTr.direction.Vec( i ) * SubFloat( Result.HitDistance, i ) ), 255, 0, 0, true, -1 );
					//		Vector vecEnd = frTr.origin.Vec( i ) + ( frTr.direction.Vec( i ) * ( SubFloat( fl4Lifespan, i ) * SubFloat( fl4Velocity, i ) ) ) ;
					//		g_pParticleSystemMgr->Query()->DebugDrawLine( frTr.origin.Vec( i ), vecEnd, 0, 255, 0, true, -1 );
					//	}
					//}
					//debug
					//Don't Mess with Particles we should skip
					fl4Lifespan = MaskedAssign( fl4SkipMask, Four_NegativeOnes, fl4Lifespan );
				}
			}
			else
			{
				fl4Lifespan = Four_NegativeOnes;
			}
			fltx4 fl4MinLife = *pLifespan;
			*pLifespan = MinSIMD( fl4MinLife, MaskedAssign( fl4HitMask, fl4Lifespan, *pLifespan ) );

			pXYZ += attr_stride;
			pXYZPrev += attr_stride;
			pLifespan += attr_stride;
		}
	}
	else
	{
		while( n_blocks-- )
		{
			FourRays frTr;
			frTr.direction.DuplicateVector( m_vecTestDir );
			frTr.origin = *pXYZ;
			fltx4 fl4TMin = Four_Zeros;
			fltx4 fl4TMax = Four_FLT_MAX;
			fl4Lifespan = *pLifespan;

			if ( IsAllEqual( fl4Lifespan, Four_NegativeOnes ) )
			{
				pXYZ += attr_stride;
				pXYZPrev += attr_stride;
				pLifespan += attr_stride;
				continue;
			}

			RayTracingResult Result;

			g_pParticleSystemMgr->Query()->TraceAgainstRayTraceEnv( m_nRtEnvNumber, frTr, fl4TMin, fl4TMax, &Result, skipid );

			i32x4 in4HitIds = LoadAlignedIntSIMD( Result.HitIds );
			fltx4 fl4HitIds = SignedIntConvertToFltSIMD ( in4HitIds );

			if ( m_bCullOnMiss )
			{
				fl4HitMask = CmpLtSIMD( fl4HitIds, Four_Zeros);
				if ( bUseNormal )
				{
					FourVectors fvCullNormal;
					fvCullNormal.DuplicateVector( m_vecTestNormal );
					bi32x4 fl4NormalMask;
					fl4NormalMask = CmpEqSIMD( fvCullNormal.x, Result.surface_normal.x );
					fl4NormalMask = AndSIMD( fl4NormalMask, CmpEqSIMD( fvCullNormal.y, Result.surface_normal.y ) );
					fl4NormalMask = AndSIMD( fl4NormalMask, CmpEqSIMD( fvCullNormal.z, Result.surface_normal.z ) );
					fl4HitMask = OrSIMD( fl4HitMask, fl4NormalMask );
				}
			}
			else
			{
				fl4HitMask = CmpGeSIMD( fl4HitIds, Four_Zeros);
				if ( bUseNormal )
				{
					FourVectors fvCullNormal;
					fvCullNormal.DuplicateVector( m_vecTestNormal );
					bi32x4 fl4NormalMask;
					fl4NormalMask = CmpEqSIMD( fvCullNormal.x, Result.surface_normal.x );
					fl4NormalMask = AndSIMD( fl4NormalMask, CmpEqSIMD( fvCullNormal.y, Result.surface_normal.y ) );
					fl4NormalMask = AndSIMD( fl4NormalMask, CmpEqSIMD( fvCullNormal.z, Result.surface_normal.z ) );
					fl4HitMask = AndSIMD( fl4HitMask, fl4NormalMask );
				}
			}

			fl4Lifespan = Four_NegativeOnes;

			*pLifespan = MaskedAssign( fl4HitMask, fl4Lifespan, *pLifespan );

			pXYZ += attr_stride;
			pXYZPrev += attr_stride;
			pLifespan += attr_stride;
		}
	}
}



//-----------------------------------------------------------------------------
// Set Normal to CP Forward Vector
//-----------------------------------------------------------------------------
class C_INIT_NormalAlignToCP : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_NormalAlignToCP );

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_NORMAL_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

private:
	int m_nControlPointNumber;
	Vector m_vOffsetMin;
	Vector m_vOffsetMax;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_NormalAlignToCP, "Normal Align to CP", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_NormalAlignToCP ) 
DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_NormalAlignToCP )


void C_INIT_NormalAlignToCP::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{

	for( ; nParticleCount--; start_p++ )
	{
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pNormal = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_NORMAL, start_p );
		Vector vecForward;
		Vector vecRight;
		Vector vecUp;
		pParticles->GetControlPointOrientationAtTime( m_nControlPointNumber, *ct, &vecForward, &vecRight, &vecUp );
		SetVectorAttribute( pNormal, vecForward );
	}
}




//-----------------------------------------------------------------------------
// Normal Offset Initializer
//-----------------------------------------------------------------------------
class C_INIT_NormalOffset : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_NormalOffset );

	Vector m_OffsetMin;
	Vector m_OffsetMax;
	int m_nControlPointNumber;
	bool m_bLocalCoords;
	bool m_bProportional;
	bool m_bNormalize;

	uint32 GetWrittenAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_NORMAL_MASK | PARTICLE_ATTRIBUTE_CREATION_TIME_MASK;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_NORMAL_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nControlPointNumber;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber ) );
	}

	bool InitMultipleOverride ( void ) { return true; }
};

DEFINE_PARTICLE_OPERATOR( C_INIT_NormalOffset, "Normal Modify Offset Random", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_NormalOffset ) 
DMXELEMENT_UNPACK_FIELD( "control_point_number", "0", int, m_nControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "offset min", "0 0 0", Vector, m_OffsetMin )
DMXELEMENT_UNPACK_FIELD( "offset max", "0 0 0", Vector, m_OffsetMax )
DMXELEMENT_UNPACK_FIELD( "offset in local space 0/1", "0", bool, m_bLocalCoords )
DMXELEMENT_UNPACK_FIELD( "normalize output 0/1", "0", bool, m_bNormalize )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_NormalOffset )


void C_INIT_NormalOffset::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	for( ; nParticleCount--; start_p++ )
	{
		const float *ct = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_CREATION_TIME, start_p );
		float *pNormal = pParticles->GetFloatAttributePtrForWrite( PARTICLE_ATTRIBUTE_NORMAL, start_p );

		Vector randpos;
		pParticles->RandomVector( m_OffsetMin, m_OffsetMax, &randpos );

		if ( m_bLocalCoords )
		{
			matrix3x4_t mat;
			pParticles->GetControlPointTransformAtTime( m_nControlPointNumber, *ct, &mat );
			Vector vecTransformLocal = vec3_origin;
			VectorRotate( randpos, mat, vecTransformLocal );
			randpos = vecTransformLocal;
		}
		Vector vecOffset;
		SetVectorFromAttribute( vecOffset, pNormal );
		vecOffset += randpos;
		if ( m_bNormalize )
			VectorNormalize( vecOffset );
		SetVectorAttribute( pNormal, vecOffset );
	}
}



//-----------------------------------------------------------------------------
// Remap Speed to Scalar Initializer
//-----------------------------------------------------------------------------
class C_INIT_RemapSpeedToScalar : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapSpeedToScalar );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return 1 << m_nFieldOutput | PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK;
	}

	uint32 GetFilter( void ) const
	{
		return FILTER_PARAMETER_REMAPPING_MASK;
	}

	void InitParams(CParticleSystemDefinition *pDef )
	{
		m_flInputMin = MAX(MIN_PARTICLE_SPEED, m_flInputMin);
		m_flInputMax = MAX(MIN_PARTICLE_SPEED, m_flInputMax);
		// clamp the result to 0 and 1 if it's alpha
		if ( ATTRIBUTES_WHICH_ARE_0_TO_1 & ( 1 << m_nFieldOutput ) )
		{
			m_flOutputMin = clamp(m_flOutputMin, 0.0f, 1.0f );
			m_flOutputMax = clamp(m_flOutputMax, 0.0f, 1.0f );
		}
	}

	bool InitMultipleOverride ( void ) { return true; }

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const;

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;


	int		m_nFieldOutput;
	int		m_nControlPointNumber;
	float	m_flStartTime;
	float	m_flEndTime;
	float	m_flInputMin;
	float	m_flInputMax;
	float	m_flOutputMin;
	float	m_flOutputMax;
	bool	m_bScaleInitialRange;
	bool	m_bPerParticle;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapSpeedToScalar, "Remap Speed to Scalar", OPERATOR_GENERIC );
BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RemapSpeedToScalar )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime start time (seconds)", "-1", float, m_flStartTime )
DMXELEMENT_UNPACK_FIELD( "emitter lifetime end time (seconds)", "-1", float, m_flEndTime )
DMXELEMENT_UNPACK_FIELD( "control point number (ignored if per particle)", "0", int, m_nControlPointNumber )
DMXELEMENT_UNPACK_FIELD( "per particle","0", bool, m_bPerParticle )
DMXELEMENT_UNPACK_FIELD( "input minimum","0", float, m_flInputMin )
DMXELEMENT_UNPACK_FIELD( "input maximum","1", float, m_flInputMax )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "3", int, m_nFieldOutput, "intchoice particlefield_scalar" )
DMXELEMENT_UNPACK_FIELD( "output minimum","0", float, m_flOutputMin )
DMXELEMENT_UNPACK_FIELD( "output maximum","1", float, m_flOutputMax )
DMXELEMENT_UNPACK_FIELD( "output is scalar of initial random range","0", bool, m_bScaleInitialRange )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapSpeedToScalar );


void C_INIT_RemapSpeedToScalar::InitNewParticlesBlock( CParticleCollection *pParticles, 
														 int start_block, int n_blocks, int nAttributeWriteMask,
														 void *pContext ) const
{
	if ( ( ( pParticles->m_flCurTime < m_flStartTime ) || ( pParticles->m_flCurTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) )
		return;
	size_t attr_stride;

	const FourVectors *pXYZ = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_XYZ, &attr_stride );
	pXYZ += attr_stride * start_block;
	const FourVectors *pPrevXYZ = pParticles->Get4VAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, &attr_stride );
	pPrevXYZ += attr_stride * start_block;
	fltx4 *pOutput = pParticles->GetM128AttributePtrForWrite( m_nFieldOutput, &attr_stride );
	pOutput += attr_stride * start_block;
	const fltx4 *pInitialOutput = pParticles->GetInitialM128AttributePtr( m_nFieldOutput, &attr_stride );
	pInitialOutput += attr_stride * start_block;

	fltx4 flMin = ReplicateX4( m_flOutputMin );
	fltx4 flMax = ReplicateX4( m_flOutputMax );

	fltx4 fl4Dt = ReplicateX4( pParticles->m_flPreviousDt );
	fltx4 fl4InputMin = ReplicateX4( m_flInputMin );
	fltx4 fl4InputMax = ReplicateX4( m_flInputMax );

	if ( m_bPerParticle )
	{
		while( n_blocks-- )
		{
			fltx4 fl4Speed = DivSIMD ( (*pXYZ - *pPrevXYZ).length(), fl4Dt );
			fltx4 fl4Output = RemapValClampedSIMD( fl4Speed, fl4InputMin, fl4InputMax, flMin, flMax  );
			if ( m_bScaleInitialRange )
			{
				fl4Output = MulSIMD( *pInitialOutput, fl4Output );
			}

			*( pOutput ) = fl4Output;

			pXYZ += attr_stride;
			pPrevXYZ += attr_stride;
			pOutput += attr_stride;
			pInitialOutput += attr_stride;
		}
	}
	else
	{
		Vector vecPrevPos;
		pParticles->GetControlPointAtPrevTime( m_nControlPointNumber, &vecPrevPos );
		FourVectors fvDelta;
		fvDelta.DuplicateVector( pParticles->GetControlPointAtCurrentTime( m_nControlPointNumber ) - vecPrevPos );
		fltx4 fl4Speed = DivSIMD ( fvDelta.length(), fl4Dt );
		fltx4 fl4Output = RemapValClampedSIMD( fl4Speed, fl4InputMin, fl4InputMax, flMin, flMax  );

		while( n_blocks-- )
		{
			if ( m_bScaleInitialRange )
			{
				fl4Output = MulSIMD( *pInitialOutput, fl4Output );
			}

			*( pOutput ) = fl4Output;

			pOutput += attr_stride;
			pInitialOutput += attr_stride;
		}
	}
}

void C_INIT_RemapSpeedToScalar::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( ( ( pParticles->m_flCurTime < m_flStartTime ) || ( pParticles->m_flCurTime >= m_flEndTime ) ) && ( ( m_flStartTime != -1.0f) && ( m_flEndTime != -1.0f) ) )
		return;

	if ( m_bPerParticle )
	{
		for( ; nParticleCount--; start_p++ )
		{
			const float *pXYZ = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, start_p );
			const float *pPrevXYZ = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_PREV_XYZ, start_p );
			float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
			Vector vecXYZ;
			Vector vecPrevXYZ;
			SetVectorFromAttribute ( vecXYZ, pXYZ );
			SetVectorFromAttribute ( vecPrevXYZ, pPrevXYZ );
			float flSpeed = (vecXYZ - vecPrevXYZ).Length() / pParticles->m_flPreviousDt;
			float flOutput = RemapValClamped( flSpeed, m_flInputMin, m_flInputMax, m_flOutputMin, m_flOutputMax  );
			if ( m_bScaleInitialRange )
			{
				const float *pInitialOutput = pParticles->GetInitialFloatAttributePtr( m_nFieldOutput, start_p );
				flOutput = *pInitialOutput * flOutput;
			}

			*pOutput = flOutput;
		}
	}
	else
	{
		Vector vecPrevPos;
		pParticles->GetControlPointAtPrevTime( m_nControlPointNumber, &vecPrevPos );
		Vector vecDelta;
		vecDelta = pParticles->GetControlPointAtCurrentTime( m_nControlPointNumber ) - vecPrevPos;
		float flSpeed = vecDelta.Length() / pParticles->m_flPreviousDt;
		float flOutput = RemapValClamped( flSpeed, m_flInputMin, m_flInputMax, m_flOutputMin, m_flOutputMax  );

		for( ; nParticleCount--; start_p++ )
		{
			float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
			if ( m_bScaleInitialRange )
			{
				const float *pInitialOutput = pParticles->GetInitialFloatAttributePtr( m_nFieldOutput, start_p );
				flOutput = *pInitialOutput * flOutput;
			}

			*pOutput = flOutput;
		}
	}
}


//-----------------------------------------------------------------------------
// CP Snapshot Initializer
//-----------------------------------------------------------------------------
class C_INIT_InitFromCPSnapshot : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_InitFromCPSnapshot );

	int m_nControlPointNumber;
	int m_nAttributeToRead;
	int m_nAttributeToWrite;
	int m_nLocalSpaceCP;

	virtual uint32 GetWrittenAttributes( void ) const
	{
		return ( ( m_nAttributeToWrite == -1 ) ? 0 : ( 1 << m_nAttributeToWrite ) );
	}

	virtual uint32 GetReadAttributes( void ) const
	{
		return 0;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		uint64 nMask = 0;
		if ( m_nControlPointNumber != -1 )
			nMask |= ( 1ULL << m_nControlPointNumber );
		if ( m_nLocalSpaceCP != -1 )
			nMask |= ( 1ULL << m_nLocalSpaceCP );
		return nMask;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef, CDmxElement *pElement )
	{
		m_nControlPointNumber = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nControlPointNumber    ) );
		m_nLocalSpaceCP       = MAX( 0, MIN( MAX_PARTICLE_CONTROL_POINTS-1, m_nLocalSpaceCP          ) );
		// If m_nAttributeToRead is not specified, just ask the snapshot for the written attribute
		if ( m_nAttributeToRead == -1 )
			m_nAttributeToRead = m_nAttributeToWrite;
	}

	// NOTE: InitNewParticlesBlock is not implemented, because InitNewParticlesScalar is just as efficient in this case
	virtual void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_InitFromCPSnapshot, "Init From CP Snapshot", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_InitFromCPSnapshot ) 
DMXELEMENT_UNPACK_FIELD(			"snapshot control point number",	"0",  int,	m_nControlPointNumber	)
DMXELEMENT_UNPACK_FIELD_USERDATA(	"field to write",					"0",  int,	m_nAttributeToWrite, "intchoice particlefield" )
DMXELEMENT_UNPACK_FIELD_USERDATA(	"field to read",					"-1", int,	m_nAttributeToRead,  "intchoice particlefield" )
DMXELEMENT_UNPACK_FIELD(			"local space control point number",	"-1", int,	m_nLocalSpaceCP			)
END_PARTICLE_OPERATOR_UNPACK( C_INIT_InitFromCPSnapshot )

void C_INIT_InitFromCPSnapshot::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( m_nAttributeToWrite == -1 )
		return;

	CParticleSnapshot *pSnapshot = pParticles->GetControlPointSnapshot( m_nControlPointNumber );
	EAttributeDataType nAttributeType = g_pParticleSystemMgr->GetParticleAttributeDataType( m_nAttributeToWrite );

	// NOTE: HasAttribute() returns false if the read/write attributes are not of the same datatype
	int nAttributeMask = ( 1 << m_nAttributeToWrite );
	if ( pSnapshot && ( nAttributeWriteMask & nAttributeMask ) && pSnapshot->HasAttribute( m_nAttributeToRead, nAttributeType ) )
	{
		matrix3x4_t cpTransform;
		int nTransformableAttributesMask = PARTICLE_ATTRIBUTE_XYZ_MASK | PARTICLE_ATTRIBUTE_PREV_XYZ_MASK | PARTICLE_ATTRIBUTE_NORMAL_MASK;
		bool bUseCPTransform = ( m_nLocalSpaceCP != -1 ) && ( nAttributeMask & nTransformableAttributesMask );
		if ( bUseCPTransform )
		{
			// Transform normals/positions as if they are specified in the local space of the given control point
			pParticles->GetControlPointTransformAtCurrentTime( m_nLocalSpaceCP, &cpTransform );
			if ( nAttributeMask == PARTICLE_ATTRIBUTE_NORMAL_MASK )
				cpTransform[0][3] = cpTransform[1][3] = cpTransform[2][3] = 0.0f;
		}

		// Check we can read/write the specified data range:
		Assert( pSnapshot->NumCols() >= ( start_p + nParticleCount ) );
		Assert( pParticles->GetFloatAttributePtrForWrite( m_nAttributeToWrite, 0 ) );
		if ( ( pSnapshot->NumCols() < ( start_p + nParticleCount ) ) ||
			 !pParticles->GetFloatAttributePtrForWrite( m_nAttributeToWrite, 0 ) )
			return;

		switch( nAttributeType )
		{
			case ATTRDATATYPE_FLOAT:
			{
				const float *pSrcAttributeData = pSnapshot->ElementPointer<float>( m_nAttributeToRead, start_p );
				float       *pDstAttributeData = pParticles->GetFloatAttributePtrForWrite( m_nAttributeToWrite, start_p );
				memcpy( pDstAttributeData, pSrcAttributeData, nParticleCount*sizeof( float ) );
				break;
			}

			case ATTRDATATYPE_INT:
			{
				const int *pSrcAttributeData = pSnapshot->ElementPointer<int>( m_nAttributeToRead, start_p );
				int       *pDstAttributeData = pParticles->GetIntAttributePtrForWrite( m_nAttributeToWrite, start_p );
				memcpy( pDstAttributeData, pSrcAttributeData, nParticleCount*sizeof( int ) );
				break;
			}
			case ATTRDATATYPE_4V:
			{
				// The source/dest data type is Vector, interleaved in FourVectors objects
				// TODO: OPTIMIZATION: if the source/dest indices have the same alignment modulo 4, and the range is contiguous, then memcpy the middle N complete FourVectors directly (and apply cpTransform to FourVectors instead of Vector)
				for( ; nParticleCount--; start_p++ )
				{
					const float *pSrcAttributeData = (const float *)pSnapshot->ElementPointer4V( m_nAttributeToRead, start_p );
					float       *pDstAttributeData = pParticles->GetFloatAttributePtrForWrite( m_nAttributeToWrite, start_p );
					Vector srcAttributeData, srcAttributeDataTmp;
					SetVectorFromAttribute( srcAttributeData, pSrcAttributeData );
					if ( bUseCPTransform )
					{
						VectorTransform( srcAttributeData, cpTransform, srcAttributeDataTmp );
						srcAttributeData = srcAttributeDataTmp;
					}
					SetVectorAttribute( pDstAttributeData, srcAttributeData );
				}
				break;
			}

			default:
				// Don't support ATTRDATATYPE_POINTER
				ExecuteNTimes( 10, Warning( "C_INIT_InitFromCPSnapshot: Unsupported attribute data type! (%d)\n", nAttributeType) );
				Assert( 0 );
				break;
		}
	}
}

//-----------------------------------------------------------------------------
// Parent Killed Particles Initializer
//-----------------------------------------------------------------------------
class C_INIT_InitFromParentKilled : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_InitFromParentKilled );

	int m_nAttributeToCopy;
	char m_AttributeCopyName[128];

	virtual uint32 GetWrittenAttributes( void ) const
	{
		return ( ( m_nAttributeToCopy == -1 ) ? 0 : ( 1 << m_nAttributeToCopy ) );
	}

	virtual uint32 GetReadAttributes( void ) const
	{
		// TODO: this refers to parent attributes.... errr....
		return ( ( m_nAttributeToCopy == -1 ) ? 0 : ( 1 << m_nAttributeToCopy ) );
	}

	virtual bool ShouldRun( bool bApplyingParentKillList ) const
	{
		return bApplyingParentKillList;
	}

	// TODO: implement InitNewParticlesBlock (requires 'gather' memory reads, though)
	virtual void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_InitFromParentKilled, "Init From Killed Parent Particle", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_InitFromParentKilled ) 
DMXELEMENT_UNPACK_FIELD_USERDATA(	"field to init",	"-1",  int,	m_nAttributeToCopy,	"intchoice particlefield" )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_InitFromParentKilled )

void C_INIT_InitFromParentKilled::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	if ( ( m_nAttributeToCopy == -1 ) || !( nAttributeWriteMask & ( 1 << m_nAttributeToCopy ) ) )
		return;

	int nNumParticlesToKill;
	KillListItem_t *pKillList = GetParentKillList( pParticles, nNumParticlesToKill );
	Assert( pKillList && ( nNumParticlesToKill > 0 ) );
	if ( !pKillList || ( nNumParticlesToKill <= 0 ) )
		return;

	Assert( HasAttribute( pParticles, m_nAttributeToCopy ) && HasAttribute( pParticles->m_pParent, m_nAttributeToCopy ) );
	int nKillListIndex = nNumParticlesToKill + ( start_p - pParticles->m_nActiveParticles );
	Assert( ( nKillListIndex >= 0 ) && ( ( nKillListIndex + nParticleCount ) <= nNumParticlesToKill ) );

	EAttributeDataType nAttributeType = g_pParticleSystemMgr->GetParticleAttributeDataType( m_nAttributeToCopy );
	for ( int nWriteIndex = start_p; nWriteIndex < ( start_p + nParticleCount ) ; nWriteIndex++ )
	{
		unsigned int nReadIndex = pKillList[ nKillListIndex++ ].nIndex;

		switch( nAttributeType )
		{
			case ATTRDATATYPE_FLOAT:
			{
				const float *pSrcAttributeData = pParticles->m_pParent->GetFloatAttributePtr( m_nAttributeToCopy, nReadIndex  );
				float       *pDstAttributeData = pParticles->GetFloatAttributePtrForWrite(    m_nAttributeToCopy, nWriteIndex );
				pDstAttributeData[0] = pSrcAttributeData[0];
				break;
			}

			case ATTRDATATYPE_INT:
			{
				const int *pSrcAttributeData = pParticles->m_pParent->GetIntAttributePtr( m_nAttributeToCopy, nReadIndex  );
				int       *pDstAttributeData = pParticles->GetIntAttributePtrForWrite(    m_nAttributeToCopy, nWriteIndex );
				pDstAttributeData[0] = pSrcAttributeData[0];
				break;
			}
			case ATTRDATATYPE_4V:
			{
				// The source/dest data type is Vector, interleaved in FourVectors objects
				const float *pSrcAttributeData = pParticles->m_pParent->GetFloatAttributePtr( m_nAttributeToCopy, nReadIndex  );
				float       *pDstAttributeData = pParticles->GetFloatAttributePtrForWrite(    m_nAttributeToCopy, nWriteIndex );
				Vector srcAttributeData;
				SetVectorFromAttribute( srcAttributeData, pSrcAttributeData );
				SetVectorAttribute( pDstAttributeData, srcAttributeData );
				break;
			}

			default:
				// Don't support ATTRDATATYPE_POINTER
				ExecuteNTimes( 10, Warning( "C_INIT_InitFromParentKilled: Unsupported attribute data type! (%d)\n", nAttributeType) );
				Assert( 0 );
				break;
		}
	}
}



class C_INIT_RemapInitialDirectionToCPToVector : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapInitialDirectionToCPToVector );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	virtual uint64 GetReadControlPointMask() const
	{
		return 1ULL << m_nCP;
	}

	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	virtual void InitNewParticlesBlock( CParticleCollection *pParticles, 
		int start_block, int n_blocks, int nAttributeWriteMask,
		void *pContext ) const;


	int		m_nCP;
	int		m_nFieldOutput;
	float m_flScale;
	float m_flOffsetRot;
	Vector m_vecOffsetAxis;
	bool m_bNormalize;
};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapInitialDirectionToCPToVector, "Remap Initial Direction to CP to Vector", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RemapInitialDirectionToCPToVector )
DMXELEMENT_UNPACK_FIELD( "control point","0", int, m_nCP )
DMXELEMENT_UNPACK_FIELD_USERDATA( "output field", "0", int, m_nFieldOutput, "intchoice particlefield_vector" )
DMXELEMENT_UNPACK_FIELD( "normalize","0", bool, m_bNormalize )
DMXELEMENT_UNPACK_FIELD( "offset axis","0 0 0", Vector, m_vecOffsetAxis )
DMXELEMENT_UNPACK_FIELD( "offset rotation","0", float, m_flOffsetRot )
DMXELEMENT_UNPACK_FIELD( "scale factor" , "1", float, m_flScale )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapInitialDirectionToCPToVector )

void C_INIT_RemapInitialDirectionToCPToVector::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	matrix3x4_t matRot;
	MatrixBuildRotationAboutAxis ( m_vecOffsetAxis, m_flOffsetRot, matRot );
	Vector vecCPPos = pParticles->GetControlPointAtCurrentTime( m_nCP );
	for( ; nParticleCount--; start_p++ )
	{
		const float *pXYZ = pParticles->GetFloatAttributePtr( PARTICLE_ATTRIBUTE_XYZ, start_p );
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		Vector vecXYZ, vecOutput;
		SetVectorFromAttribute ( vecXYZ, pXYZ );

		vecXYZ -= vecCPPos;
		VectorRotate( vecXYZ, matRot, vecOutput );

		if ( m_bNormalize )
			vecOutput.NormalizeInPlace();
		vecOutput *= m_flScale;

		SetVectorAttribute( pOutput, vecOutput );
	}
}

void C_INIT_RemapInitialDirectionToCPToVector::InitNewParticlesBlock( CParticleCollection *pParticles, 
	int start_block, int n_blocks, int nAttributeWriteMask,
	void *pContext ) const
{
	C4VAttributeIterator xyz( PARTICLE_ATTRIBUTE_XYZ, pParticles );
	C4VAttributeWriteIterator pOutField( m_nFieldOutput, pParticles) ;
	int nCtr = pParticles->m_nPaddedActiveParticles;
	FourVectors v4CPPosition;
	FourVectors v4Offset;

	matrix3x4_t matRot;
	MatrixBuildRotationAboutAxis ( m_vecOffsetAxis, m_flOffsetRot, matRot );
	v4CPPosition.DuplicateVector( pParticles->GetControlPointAtCurrentTime( m_nCP ) );

	if ( m_bNormalize )
	{
		fltx4 fl4Scale = ReplicateX4( m_flScale );
		do
		{
			FourVectors v4Vel = *xyz;
			v4Vel -= v4CPPosition;
			v4Vel.RotateBy( matRot );
			v4Vel.VectorNormalize();
			v4Vel *= fl4Scale;
			*pOutField = v4Vel;
			++pOutField;
			++xyz;
		} while( --nCtr );
	}
	else
	{
		fltx4 fl4Scale = ReplicateX4( m_flScale * 1.0 / ( MAX( 1.0e-20, pParticles->m_flPreviousDt ) ) );
		do
		{
			FourVectors v4Vel = *xyz;
			v4Vel -= v4CPPosition;
			v4Vel += v4Offset;
			v4Vel.RotateBy( matRot );
			v4Vel *= fl4Scale;
			*pOutField = v4Vel;
			++pOutField;
			++xyz;
		} while( --nCtr );
	}

}





class C_INIT_RemapInitialCPDirectionToRotation : public CParticleInitializerOperatorInstance
{
	DECLARE_PARTICLE_OPERATOR( C_INIT_RemapInitialCPDirectionToRotation );

	uint32 GetWrittenAttributes( void ) const
	{
		return 1 << m_nFieldOutput;
	}

	uint32 GetReadAttributes( void ) const
	{
		return PARTICLE_ATTRIBUTE_XYZ_MASK;
	}

	virtual void InitParams( CParticleSystemDefinition *pDef )
	{
		m_nAxis = MAX( 0, MIN( 2, m_nAxis ) );
		if ( m_nAxis == 0 )
		{
			m_nComponent1 = 1;
			m_nComponent2 = 0;
		}
		else if ( m_nAxis == 1 )
		{
			m_nComponent1 = 2;
			m_nComponent2 = 0;
		}
		else if ( m_nAxis == 2 )
		{
			m_nComponent1 = 2;
			m_nComponent2 = 1;
		}
	}


	void InitNewParticlesScalar( CParticleCollection *pParticles, int start_p,
		int nParticleCount, int nAttributeWriteMask,
		void *pContext) const;

	int		m_nCP;
	int		m_nFieldOutput;
	int		m_nAxis;
	float m_flScale;
	float m_flOffsetRot;
	int		m_nComponent1;
	int		m_nComponent2;

};

DEFINE_PARTICLE_OPERATOR( C_INIT_RemapInitialCPDirectionToRotation, "Remap CP Orientation to Rotation", OPERATOR_GENERIC );

BEGIN_PARTICLE_INITIALIZER_OPERATOR_UNPACK( C_INIT_RemapInitialCPDirectionToRotation )
DMXELEMENT_UNPACK_FIELD( "control point","0", int, m_nCP )
DMXELEMENT_UNPACK_FIELD_USERDATA( "rotation field", "0", int, m_nFieldOutput, "intchoice particlefield_rotation" )
DMXELEMENT_UNPACK_FIELD( "axis","0", int, m_nAxis )
DMXELEMENT_UNPACK_FIELD( "offset rotation","0", float, m_flOffsetRot )
END_PARTICLE_OPERATOR_UNPACK( C_INIT_RemapInitialCPDirectionToRotation )

void C_INIT_RemapInitialCPDirectionToRotation::InitNewParticlesScalar( 
	CParticleCollection *pParticles, int start_p,
	int nParticleCount, int nAttributeWriteMask, void *pContext ) const
{
	float flRotOffset = m_flOffsetRot * ( M_PI / 180.0f );
	Vector vecCPRotation[3];
	pParticles->GetControlPointOrientationAtCurrentTime( m_nCP, &vecCPRotation[0], &vecCPRotation[1], &vecCPRotation[2] );

	float flRot = atan2(vecCPRotation[m_nAxis][m_nComponent1], vecCPRotation[m_nAxis][m_nComponent2] ) + M_PI;

	flRot += flRotOffset;

	for( ; nParticleCount--; start_p++ )
	{
		float *pOutput = pParticles->GetFloatAttributePtrForWrite( m_nFieldOutput, start_p );
		*pOutput = flRot;
	}
}

//
//
//
//
 
//-----------------------------------------------------------------------------
// Purpose: Add all operators to be considered active, here
//-----------------------------------------------------------------------------
void AddBuiltInParticleInitializers( void )
{
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RingWave );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateAlongPath );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_MoveBetweenPoints );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateWithinSphere );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_VelocityRandom );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateOnModel );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateWithinBox );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomRotationSpeed );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomLifeTime );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomAlpha );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomRadius );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomRotation );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomYaw );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomColor );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_ColorLitPerParticle );	
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomTrailLength );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomSequence );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_PositionOffset );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_PositionWarp );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_PositionPlaceOnGround );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreationNoise );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_InitialVelocityNoise );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapScalar );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapParticleCountToScalar );	
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_InheritVelocity );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_AgeNoise ); 
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_SequenceLifeTime ); 
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateInHierarchy );  
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapScalarToVector );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_OffsetVectorToVector );	
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateSequentialPath );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_InitialRepulsionVelocity );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomYawFlip );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomSecondSequence );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapCPtoScalar );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapCPtoVector );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapSpeedToScalar );	
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateFromParentParticles );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_DistanceToCPInit );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_LifespanFromVelocity );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateFromPlaneCache );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_ChaoticAttractor );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_ModelCull );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RtEnvCull );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_NormalAlignToCP );	
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_NormalOffset );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_VelocityFromCP );		
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_SequenceFromCP );		
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_InitFromCPSnapshot );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_InitFromParentKilled );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_SetHitboxToModel );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_InheritFromParentParticles );	
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomVector );	
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomVectorComponent );		
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RandomScalar );	
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_CreateInEpitrochoid );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_SetHitboxToClosest );	
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapInitialDirectionToCPToVector );
	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_RemapInitialCPDirectionToRotation );	
	//	REGISTER_PARTICLE_OPERATOR( FUNCTION_INITIALIZER, C_INIT_OffsetToCP );	
}

