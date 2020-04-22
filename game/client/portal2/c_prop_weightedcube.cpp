//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_prop_weightedcube.h"
#include "portal_grabcontroller_shared.h"
#include "c_portal_player.h"


IMPLEMENT_CLIENTCLASS_DT( C_PropWeightedCube, DT_PropWeightedCube, CPropWeightedCube )
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( prop_weighted_cube, C_PropWeightedCube );

CUtlVector<C_PropWeightedCube *> C_PropWeightedCube::s_AllWeightedCubes;

extern void ComputePlayerMatrix( CBasePlayer *pPlayer, matrix3x4_t &out );

QAngle C_PropWeightedCube::PreferredCarryAngles( void )
{
	static QAngle s_prefAngles;
	s_prefAngles = (m_qPreferredPlayerCarryAngles.x < FLT_MAX) ? m_qPreferredPlayerCarryAngles : vec3_angle;

	CBasePlayer *pPlayer = GetPlayerHoldingEntity( this );
	if ( pPlayer )
	{
		Vector vecRight;
		AngleVectors( pPlayer->EyeAngles(), NULL, &vecRight, NULL );

		Quaternion qRotation;
		AxisAngleQuaternion( vecRight, pPlayer->EyeAngles().x, qRotation );

		matrix3x4_t tmp;
		ComputePlayerMatrix( pPlayer, tmp );

		QAngle qTemp = TransformAnglesToWorldSpace( s_prefAngles, tmp );
		Quaternion qExisting;
		AngleQuaternion( qTemp, qExisting );
		Quaternion qFinal;
		QuaternionMult( qRotation, qExisting, qFinal );

		QuaternionAngles( qFinal, qTemp );
		s_prefAngles = TransformAnglesToLocalSpace( qTemp, tmp );
	}

	return s_prefAngles;
}

const Vector& C_PropWeightedCube::GetRenderOrigin( void )
{
	if( GetPredictable() )
	{
		C_Portal_Player *pPlayer = (C_Portal_Player *)GetPlayerHoldingEntity( this );
		if( pPlayer && pPlayer->GetGrabController().GetAttached() == this )
		{
			//predicted grab controllers will almost never get the prediction correct. Which nukes our interpolation histories, resulting in jittery movement
			//workaround this by using the nuke-safe interpolation history directly in the grab controller
			return pPlayer->GetGrabController().GetHeldObjectRenderOrigin();
		}
	}

	return BaseClass::GetRenderOrigin();
}


void C_PropWeightedCube::UpdateOnRemove( void )
{
	s_AllWeightedCubes.FindAndFastRemove( this );
	BaseClass::UpdateOnRemove();
}

void C_PropWeightedCube::Spawn( void )
{
	BaseClass::Spawn();
	s_AllWeightedCubes.AddToTail( this );
}

//At some point it would be good to generalize this function to handle all MOVETYPE_VPHYSICS entities
//But we're close to cert for Portal 2. So the scope at the moment is kept small.
void MoveUnpredictedPhysicsNearPlayerToNetworkedPosition( CBasePlayer *pPlayer )
{
	Vector vPlayerCenter = pPlayer->WorldSpaceCenter();

	for( int i = 0; i != C_PropWeightedCube::s_AllWeightedCubes.Count(); ++i )
	{
		C_PropWeightedCube *pCube = C_PropWeightedCube::s_AllWeightedCubes[i];
		if( !pCube->GetPredictable() && ((vPlayerCenter - pCube->WorldSpaceCenter()).LengthSqr() < (512.0f * 512.0f)) )
		{
			pCube->MoveToLastReceivedPosition();
		}
	}
}
