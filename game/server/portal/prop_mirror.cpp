//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

//NOTE: Mirrors with models require an attachment named "MirrorSurface_Attach" with x facing out of the mirror plane. 
//They also require that the mirror surface be in a bodygroup by itself named "MirrorSurface" with the first index being the mirror, second being empty.
//Lastly, they require that all non-mirror geometry be in bodygroups that have the second entry as empty.
//It's a good idea to put a cubemap on the mirror surface material because they're not infinitely recursive

#include "cbase.h"
#include "baseanimating.h"
#include "pvs_extender.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//#define TEST_ANIMATION //uncomment to run "testanim" in a loop

class CProp_Mirror : public CBaseAnimating, CPVS_Extender
{
public:
	DECLARE_CLASS( CProp_Mirror, CBaseAnimating );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CProp_Mirror( void );
	virtual void Precache( void );
	virtual void Spawn( void );
	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }
	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() | (m_bPhysicsEnabled ? FCAP_IMPULSE_USE : 0); };
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void UpdateReflectionPlane( void );
	void UpdateReflectionPolygon( void );

	virtual CServerNetworkProperty *GetExtenderNetworkProp( void ) { return NetworkProp(); }
	virtual const edict_t	*GetExtenderEdict( void ) const { return edict(); }
	virtual Vector			GetExtensionPVSOrigin( void ) { return GetAbsOrigin(); }

	virtual bool			IsExtenderValid( void ) { return true; }

	int						ComputeFrustumThroughPolygon( const Vector &vVisOrigin, const VPlane *pInputFrustum, int iInputFrustumPlanes, VPlane *pOutputFrustum, int iOutputFrustumMaxPlanes );

	//This portal is decidedly visible, recursively extend the visibility problem
	virtual void			ComputeSubVisibility( CPVS_Extender **pExtenders, int iExtenderCount, unsigned char *outputPVS, int pvssize, const Vector &vVisOrigin, const VPlane *pVisFrustum, int iVisFrustumPlanes, VisExtensionChain_t *pVisChain, int iAreasNetworked[MAX_MAP_AREAS], int iMaxRecursionsLeft );

#if defined( TEST_ANIMATION )
	virtual void Think( void );
#endif

	Vector m_LocalSpaceReflectionPolygonVerts[10]; //best guess at the reflection polygon by intersecting the reflection plane with the local space OBB
	int m_LocalSpaceReflectionPolygonVertCount;

	struct ReflectPlaneCachedData_t
	{
		Vector vAttachmentOrigin;
		QAngle qAttachmentAngle;

		bool bModel;
		Vector vLocalSpaceAttachmentOrigin;
		QAngle qLocalSpaceAttachmentAngles;
		Vector vLocalOBB_Mins;
		Vector vLocalOBB_Maxs;
	};
	ReflectPlaneCachedData_t m_CachedReflectedData;

	VMatrix m_matReflection;

	CNetworkVar( float, m_fWidth );
	CNetworkVar( float, m_fHeight );
	int m_iMirrorFaceAttachment;
	bool m_bModel;
	bool m_bPhysicsEnabled;
};

BEGIN_DATADESC( CProp_Mirror )
	DEFINE_KEYFIELD( m_fWidth, FIELD_FLOAT, "Width" ),
	DEFINE_KEYFIELD( m_fHeight, FIELD_FLOAT, "Height" ),
	DEFINE_FIELD( m_iMirrorFaceAttachment, FIELD_INTEGER ),
	DEFINE_FIELD( m_bModel, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_bPhysicsEnabled, FIELD_BOOLEAN, "PhysicsEnabled" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CProp_Mirror, DT_Prop_Mirror )
	SendPropFloat( SENDINFO(m_fWidth) ),
	SendPropFloat( SENDINFO(m_fHeight) ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( prop_mirror, CProp_Mirror );


CProp_Mirror::CProp_Mirror( void )
{
	m_matReflection.m[3][0] = 0.0f;
	m_matReflection.m[3][1] = 0.0f;
	m_matReflection.m[3][2] = 0.0f;
	m_matReflection.m[3][3] = 1.0f;

	m_CachedReflectedData.vAttachmentOrigin.Invalidate();
	m_CachedReflectedData.qAttachmentAngle.Invalidate();
	m_CachedReflectedData.vLocalSpaceAttachmentOrigin.Invalidate();
	m_CachedReflectedData.qLocalSpaceAttachmentAngles.Invalidate();
	m_CachedReflectedData.vLocalOBB_Maxs.Invalidate();
	m_CachedReflectedData.vLocalOBB_Mins.Invalidate();
}

void CProp_Mirror::Precache( void )
{
	BaseClass::Precache();
	if( (m_ModelName.ToCStr() != NULL) && (m_ModelName.ToCStr()[0] != '\0') )
	{
		PrecacheModel( m_ModelName.ToCStr() );
	}
}

void CProp_Mirror::Spawn( void )
{
	Precache();
	BaseClass::Spawn();
		
	if( m_ModelName.ToCStr() != NULL && m_ModelName.ToCStr()[0] != '\0' )
	{
		SetModel( m_ModelName.ToCStr() );
		SetSolid( SOLID_VPHYSICS );
		SetCollisionGroup( COLLISION_GROUP_INTERACTIVE );
		
		if( m_bPhysicsEnabled )
		{
			SetMoveType( MOVETYPE_VPHYSICS );
			VPhysicsInitNormal( GetSolid(), GetSolidFlags(), false );
		}
		else
		{
			SetMoveType( MOVETYPE_NONE );
		}

#if defined( TEST_ANIMATION )
		ResetSequence( LookupSequence( "testanim" ) );
		ResetSequenceInfo();
		SetPlaybackRate( 0.1f );	
		
		SetNextThink( gpGlobals->curtime + 1.0f );
#endif

		m_iMirrorFaceAttachment = LookupAttachment( "MirrorSurface_Attach" );
		m_bModel = ( m_iMirrorFaceAttachment > 0 ); //0 is an invalid attachment index according to LookupAttachment()
	}
	else
	{
		Vector vExtent( 2.0f, m_fWidth/2.0f, m_fHeight/2.0f );
		SetSize( -vExtent, vExtent );
		m_bModel = false;
	}
}

void CProp_Mirror::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( m_bPhysicsEnabled )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pActivator );
		if ( pPlayer )
		{
			pPlayer->PickupObject( this );
		}
	}
	else
	{
		BaseClass::Use( pActivator, pCaller, useType, value );
	}
}

void CProp_Mirror::UpdateReflectionPlane( void )
{
	Vector vMirrorAttachmentOrigin;
	QAngle qMirrorAttachmentAngles;
	if( m_bModel )
	{
		GetAttachment( m_iMirrorFaceAttachment, vMirrorAttachmentOrigin, qMirrorAttachmentAngles );
	}
	else
	{
		vMirrorAttachmentOrigin = GetAbsOrigin();
		qMirrorAttachmentAngles = GetAbsAngles();
	}

	if( (m_CachedReflectedData.vAttachmentOrigin != vMirrorAttachmentOrigin) || (m_CachedReflectedData.qAttachmentAngle != qMirrorAttachmentAngles) )
	{
		m_CachedReflectedData.vAttachmentOrigin = vMirrorAttachmentOrigin;
		m_CachedReflectedData.qAttachmentAngle = qMirrorAttachmentAngles;		

		Vector vOrigin = m_CachedReflectedData.vAttachmentOrigin;
		Vector vForward, vRight, vUp;
		AngleVectors( qMirrorAttachmentAngles, &vForward, &vRight, &vUp );
		Vector vToOrigin( vOrigin.Dot( vForward ), vOrigin.Dot( vRight ), -vOrigin.Dot( vUp ) );

		//generate mirroring matrix. Move mirror to origin using base vectors, flip on forward axis, move back to position and orientation
		{
			m_matReflection.m[0][0] = (-vForward.x * vForward.x) + (vRight.x * vRight.x) + (vUp.x * vUp.x);
			m_matReflection.m[0][1] = (-vForward.x * vForward.y) + (vRight.x * vRight.y) + (vUp.x * vUp.y);
			m_matReflection.m[0][2] = (-vForward.x * vForward.z) + (vRight.x * vRight.z) + (vUp.x * vUp.z);
			m_matReflection.m[0][3] = (vToOrigin.x * vForward.x) - (vToOrigin.y * vRight.x) + (vToOrigin.z * vUp.x) + vOrigin.x;
			m_matReflection.m[1][0] = m_matReflection.m[0][1]; //rotation portion of the matrix is equal to it's own transpose
			m_matReflection.m[1][1] = (-vForward.y * vForward.y) + (vRight.y * vRight.y) + (vUp.y * vUp.y);
			m_matReflection.m[1][2] = (-vForward.y * vForward.z) + (vRight.y * vRight.z) + (vUp.y * vUp.z);
			m_matReflection.m[1][3] = (vToOrigin.x * vForward.y) - (vToOrigin.y * vRight.y) + (vToOrigin.z * vUp.y) + vOrigin.y;
			m_matReflection.m[2][0] = m_matReflection.m[0][2]; //rotation portion of the matrix is equal to it's own transpose
			m_matReflection.m[2][1] = m_matReflection.m[1][2]; //rotation portion of the matrix is equal to it's own transpose
			m_matReflection.m[2][2] = (-vForward.z * vForward.z) + (vRight.z * vRight.z) + (vUp.z * vUp.z);	
			m_matReflection.m[2][3] = (vToOrigin.x * vForward.z) - (vToOrigin.y * vRight.z) + (vToOrigin.z * vUp.z) + vOrigin.z;
		}

		UpdateReflectionPolygon();
	}	
}

void CProp_Mirror::UpdateReflectionPolygon( void )
{
	if( m_bModel != m_CachedReflectedData.bModel )
	{
		m_CachedReflectedData.qAttachmentAngle.Invalidate();
		m_CachedReflectedData.vLocalSpaceAttachmentOrigin.Invalidate();
		m_CachedReflectedData.qLocalSpaceAttachmentAngles.Invalidate();
		m_CachedReflectedData.vLocalOBB_Maxs.Invalidate();
		m_CachedReflectedData.bModel = m_bModel;
	}

	if( m_bModel )
	{
		Vector vMins, vMaxs;
		vMins = WorldAlignMins();
		vMaxs = WorldAlignMaxs();

		Vector vLocalAttachmentOrigin;
		QAngle qLocalAttachmentAngles;
		GetAttachmentLocal( m_iMirrorFaceAttachment, vLocalAttachmentOrigin, qLocalAttachmentAngles );

		if( (vMins == m_CachedReflectedData.vLocalOBB_Mins) && (vMaxs == m_CachedReflectedData.vLocalOBB_Maxs) && 
			(vLocalAttachmentOrigin == m_CachedReflectedData.vLocalSpaceAttachmentOrigin) && (qLocalAttachmentAngles == m_CachedReflectedData.qLocalSpaceAttachmentAngles) )
		{
			return; //nothing to update
		}

		m_CachedReflectedData.vLocalOBB_Mins = vMins;
		m_CachedReflectedData.vLocalOBB_Maxs = vMaxs;
		m_CachedReflectedData.vLocalSpaceAttachmentOrigin = vLocalAttachmentOrigin;
		m_CachedReflectedData.qLocalSpaceAttachmentAngles = qLocalAttachmentAngles;

		Vector vAttachmentVectors[3];
		AngleVectors( qLocalAttachmentAngles, &vAttachmentVectors[0], &vAttachmentVectors[1], &vAttachmentVectors[2] );
		float fLargestOBBDiff = vMaxs.x - vMins.x;
		for( int i = 1; i != 3; ++i )
		{
			float fDiff = vMaxs[i] - vMins[i];
			if( fDiff > fLargestOBBDiff )
			{
				fLargestOBBDiff = fDiff;
			}
		}
		fLargestOBBDiff *= 4.0f; //to easily cover diagonal intersection and then some

		Vector vClipBuffers[2][10]; //4 starting points, possible to create 1 extra point per cut, 6 cuts
		vClipBuffers[0][0] = vLocalAttachmentOrigin + (vAttachmentVectors[1] * fLargestOBBDiff) + (vAttachmentVectors[2] * fLargestOBBDiff);
		vClipBuffers[0][1] = vLocalAttachmentOrigin - (vAttachmentVectors[1] * fLargestOBBDiff) + (vAttachmentVectors[2] * fLargestOBBDiff);
		vClipBuffers[0][2] = vLocalAttachmentOrigin - (vAttachmentVectors[1] * fLargestOBBDiff) - (vAttachmentVectors[2] * fLargestOBBDiff);
		vClipBuffers[0][3] = vLocalAttachmentOrigin + (vAttachmentVectors[1] * fLargestOBBDiff) - (vAttachmentVectors[2] * fLargestOBBDiff);
		int iVertCount = 4;

		VPlane vClipPlanes[6];
		vClipPlanes[0].Init( Vector( 1.0f, 0.0f, 0.0f ), vMins.x );
		vClipPlanes[1].Init( Vector( -1.0f, 0.0f, 0.0f ), -vMaxs.x );
		vClipPlanes[2].Init( Vector( 0.0f, 1.0f, 0.0f ), vMins.y );
		vClipPlanes[3].Init( Vector( 0.0f, -1.0f, 0.0f ), -vMaxs.y );
		vClipPlanes[4].Init( Vector( 0.0f, 0.0f, 1.0f ), vMins.z );
		vClipPlanes[5].Init( Vector( 0.0f, 0.0f, -1.0f ), -vMaxs.z );

		for( int i = 0; i != 6; ++i )
		{
			iVertCount = ClipPolyToPlane( vClipBuffers[i & 1], iVertCount, vClipBuffers[(i & 1) ^ 1], vClipPlanes[i].m_Normal, vClipPlanes[i].m_Dist, 0.01f ); 
		}
		Assert( iVertCount >= 3 );

		m_LocalSpaceReflectionPolygonVertCount = iVertCount;
		memcpy( m_LocalSpaceReflectionPolygonVerts, vClipBuffers[0], sizeof( Vector ) * iVertCount );
	}
	else
	{
		if( (m_CachedReflectedData.vLocalOBB_Maxs.x == m_fWidth) && (m_CachedReflectedData.vLocalOBB_Maxs.y == m_fHeight) )
			return;

		m_LocalSpaceReflectionPolygonVertCount = 4;
		float fHalfWidth = m_fWidth / 2.0f;
		float fHalfHeight = m_fHeight / 2.0f;
		m_LocalSpaceReflectionPolygonVerts[0].Init( 0.0f, fHalfWidth, fHalfHeight );
		m_LocalSpaceReflectionPolygonVerts[1].Init( 0.0f, -fHalfWidth, fHalfHeight );
		m_LocalSpaceReflectionPolygonVerts[2].Init( 0.0f, -fHalfWidth, -fHalfHeight );
		m_LocalSpaceReflectionPolygonVerts[3].Init( 0.0f, fHalfWidth, -fHalfHeight );
	}
}

#if defined( TEST_ANIMATION )
void CProp_Mirror::Think( void )
{	
	StudioFrameAdvance();
	DispatchAnimEvents( this );

	if (IsSequenceFinished() && !SequenceLoops())
	{
		// ResetSequenceInfo();
		// hack to avoid reloading model every frame
		m_flAnimTime = gpGlobals->curtime;
		m_flPlaybackRate = 0.1f;
		m_bSequenceFinished = false;
		m_flLastEventCheck = 0;
		m_flCycle = 0;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}
#endif


int CProp_Mirror::ComputeFrustumThroughPolygon( const Vector &vVisOrigin, const VPlane *pInputFrustum, int iInputFrustumPlanes, VPlane *pOutputFrustum, int iOutputFrustumMaxPlanes )
{
	Vector vTransformedPolyVerts[10];
	const matrix3x4_t &matLocalToWorld = CollisionProp()->CollisionToWorldTransform();
	for( int i = 0; i != m_LocalSpaceReflectionPolygonVertCount; ++i )
	{
		VectorTransform( &m_LocalSpaceReflectionPolygonVerts[i].x, matLocalToWorld, &vTransformedPolyVerts[i].x );
	}

	int iReturnedPlanes = UTIL_CalcFrustumThroughConvexPolygon( vTransformedPolyVerts, m_LocalSpaceReflectionPolygonVertCount, vVisOrigin, pInputFrustum, iInputFrustumPlanes, pOutputFrustum, iOutputFrustumMaxPlanes, 0 );

	if( (iReturnedPlanes < iOutputFrustumMaxPlanes) && (iReturnedPlanes != 0) )
	{
		Vector vForward;
		AngleVectors( m_CachedReflectedData.qAttachmentAngle, &vForward );
		vForward = -vForward;

		//add the reflection plane as a near plane
		pOutputFrustum[iReturnedPlanes].Init( vForward, vForward.Dot( m_CachedReflectedData.vAttachmentOrigin ) );
		++iReturnedPlanes;
	}

	return iReturnedPlanes;
}

void CProp_Mirror::ComputeSubVisibility( CPVS_Extender **pExtenders, int iExtenderCount, unsigned char *outputPVS, int pvssize, const Vector &vVisOrigin, const VPlane *pVisFrustum, int iVisFrustumPlanes, VisExtensionChain_t *pVisChain, int iAreasNetworked[MAX_MAP_AREAS], int iMaxRecursionsLeft )
{
	if( iAreasNetworked[MAX_MAP_AREAS - 1] != -1 ) //early out, can't add any more data if we wanted to
		return;

	UpdateReflectionPlane();

	Vector vForward;
	AngleVectors( m_CachedReflectedData.qAttachmentAngle, &vForward );
	if( vForward.Dot( vVisOrigin ) < vForward.Dot( m_CachedReflectedData.vAttachmentOrigin ) )
		return; //vis origin is behind the reflection plane	

	//both test if the portal is within the view frustum, and calculate the new one at the same time
	int iFrustumPlanesMax = (iVisFrustumPlanes + m_LocalSpaceReflectionPolygonVertCount + 1);
	VPlane *pNewFrustum = (VPlane *)stackalloc( sizeof( VPlane ) * iFrustumPlanesMax );

	int iNewFrustumPlanes = ComputeFrustumThroughPolygon( vVisOrigin, pVisFrustum, iVisFrustumPlanes, pNewFrustum, iFrustumPlanesMax );
	if( iNewFrustumPlanes == 0 )
	{
		return;
	}

	//NDebugOverlay::EntityBounds( this, 0, 255, 0, 100, 0.0f );

	int iArea = NetworkProp()->AreaNum();

	unsigned char *pPVS = m_pExtenderData->iPVSBits;

	if( !m_pExtenderData->bAddedToPVSAlready )
	{
		bool bFound = false;
		for( int i = 0; i != MAX_MAP_AREAS; ++i )
		{
			if( iAreasNetworked[i] == iArea )
			{
				bFound = true;
				break;
			}

			if( iAreasNetworked[i] == -1 )
			{
				bFound = true; //we found it by adding it
				iAreasNetworked[i] = iArea;
				int iOutputPVSIntSize = pvssize / sizeof( unsigned int );
				for( int j = 0; j != iOutputPVSIntSize; ++j )
				{
					((unsigned int *)outputPVS)[j] |= ((unsigned int *)pPVS)[j];
				}
				for( int j = iOutputPVSIntSize * sizeof( unsigned int ); j != pvssize; ++j )
				{
					outputPVS[j] |= pPVS[j];
				}
				break;
			}
		}

		Vector vForward;
		AngleVectors( m_CachedReflectedData.qAttachmentAngle, &vForward, NULL, NULL );
		engine->AddOriginToPVS( m_CachedReflectedData.vAttachmentOrigin + vForward );
		m_pExtenderData->bAddedToPVSAlready = true;
	}

	--iMaxRecursionsLeft;
	if( iMaxRecursionsLeft == 0 )
		return;

	edict_t *myEdict = edict();

	VisExtensionChain_t chainNode;
	chainNode.m_nArea = iArea;
	chainNode.pParentChain = pVisChain;


	//transform vis origin to linked space
	Vector vTransformedVisOrigin = m_matReflection * vVisOrigin;

	Vector vTranslation = m_matReflection.GetTranslation();

	//transform the planes into the linked portal space
	for( int i = 0; i != iNewFrustumPlanes; ++i )
	{
		pNewFrustum[i].m_Normal = m_matReflection.ApplyRotation( pNewFrustum[i].m_Normal );
		pNewFrustum[i].m_Dist += pNewFrustum[i].m_Normal.Dot( vTranslation );
	}

	Assert( pPVS != NULL );

	//extend the vis by what the linked portal can see
	for( int i = 0; i != iExtenderCount; ++i )
	{
		CPVS_Extender *pExtender = pExtenders[i];

		if ( pExtender->GetExtenderEdict() == myEdict )
			continue;

		if ( pExtender->GetExtenderNetworkProp()->IsInPVS( myEdict, pPVS, (MAX_MAP_LEAFS/8) ) ) //test against linked portal PVS, not aggregate PVS
		{
			chainNode.pExtender = pExtender;
			pExtender->ComputeSubVisibility( pExtenders, iExtenderCount, outputPVS, pvssize, vTransformedVisOrigin, pNewFrustum, iNewFrustumPlanes, &chainNode, iAreasNetworked, iMaxRecursionsLeft );			
		}
	}
}
