 //===== Copyright Âc 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: static_prop - don't move, don't animate, don't do anything.
//			physics_prop - move, take damage, but don't animate
//
//===========================================================================//


#include "cbase.h"
#include "prop_physics2.h"
#include "tier1/tier1.h"
#include "vphysics2_interface.h"
#include "datacache/imdlcache.h"
#include <vphysics2_interface_flags.h>
#include "phzfile.h"

extern IPhysics2World      *g_pPhys2World;

ConVar g_cv_phys2_shoot_speed("phys2_shoot_speed", "250");

//bool g_bTestPhysics2PropSphere = false;

void CPhysics2Prop::Spawn( void )
{
	SetNetworkQuantizeOriginAngAngles( true );

	if ( FClassnameIs( this, "physics2_prop" ) )
	{
		SetClassname( "prop_physics2" );
	}
	CBaseEntity::Spawn();
	if ( IsMarkedForDeletion() )
		return;

	m_pCookedInertia = NULL;
	m_pInertia = NULL;
	m_pShape = NULL;

	const Physics2PropCollision_t *propCollision = modelinfo->GetPhysics2PropCollision(GetModelIndex());
	m_pShape = propCollision->m_pShape;
	m_pInertia = propCollision->m_pInertia;

	if(!m_pShape)
		m_pShape = g_pPhysics2ResourceManager->GetStockShape(PHYSICS2_STOCK_SPHERE_1M);
	
	if(!m_pInertia)
	{
		m_pCookedInertia = g_pPhysics2->GetCook()->CookInertia(m_pShape);
		m_pInertia = m_pCookedInertia->GetInertia();
	}
	m_pActor = g_pPhys2World->AddActor(m_pShape, m_pInertia);
	fltx4 pos = LoadUnaligned3SIMD(&GetAbsOrigin().x);
	m_pActor->SetPosition(pos);
	m_pActor->SetUserData((uintp)static_cast<CBaseEntity*>(this));
}

CPhysics2Prop::~CPhysics2Prop()
{
	if(m_pCookedInertia)
		g_pPhysics2->GetCook()->Destroy(m_pCookedInertia);
	m_pActor->SetUserData(0);
	g_pPhys2World->Destroy(m_pActor);
	//g_pPhys2World->Destroy(m_pBoxActor);
	//g_pPhysics2ResourceManager->Release(m_pBoxShape);
}


void CPhysics2Prop::Precache( void )
{
	if ( GetModelName() == NULL_STRING )
	{
		Msg( "%s at (%.3f, %.3f, %.3f) has no model name!\n", GetClassname(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z );
	}
	else
	{
		PrecacheModel( STRING( GetModelName() ) );
		CBaseEntity::Precache();
	}
}

void CPhysics2Prop::VPhysicsUpdate( IPhysicsObject *pPhysicsNull )
{
	Assert(!pPhysicsNull);
	union 
	{
		struct{float x,y,z,w;}f;
		fltx4 v4;
	}temp;
	Assert(m_pActor->GetUserData() == (uintp)this);
	temp.v4 = m_pActor->GetPosition();
	SetAbsOrigin(Vector(temp.f.x,temp.f.y,temp.f.z));

	QAngle angles;
	QuaternionAngles(m_pActor->GetOrientation(), angles);
	SetAbsAngles(angles);
}




BEGIN_DATADESC( CPhysics2Prop )
END_DATADESC()

// IMPLEMENT_SERVERCLASS_ST( CPhysics2Prop, DT_Physics2Prop )
// END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( prop_physics2, CPhysics2Prop );

CON_COMMAND_F( phys2_shoot, "<tank|boomer|smoker|witch|hunter|mob|common> <auto> <ragdoll> <area>.  Shoots a phys2 object.", FCVAR_CHEAT )
{
	CBasePlayer *commandIssuer = UTIL_GetCommandClient();
	if( commandIssuer == NULL )
		return ;
	Vector spawnOffset(0,0,24);
	Vector forward;
	commandIssuer->EyeVectors( &forward );

	trace_t tr;
	UTIL_TraceLine( commandIssuer->EyePosition(), commandIssuer->EyePosition() + forward * 100, MASK_ZOMBIESOLID, commandIssuer, COLLISION_GROUP_NONE, &tr );

	
	//CBaseEntity *pTemp = CreateEntityByName("prop_physics2");
	Vector velocity = forward * g_cv_phys2_shoot_speed.GetFloat();
	Vector spawnPos = tr.endpos;
	if ( tr.fraction != 1.0 )
		spawnPos += spawnOffset;

	CBaseEntity *pEntity = CBaseEntity::CreateNoSpawn("prop_physics2", spawnPos, QAngle(0,0,0));
	pEntity->SetModelName(/*castable_string_t*/MAKE_STRING("models/props_crates/supply_crate01.mdl"));
	pEntity->Precache();
	pEntity->SetModel("models/props_crates/supply_crate01.mdl");
	pEntity->Spawn();
	static_cast<CPhysics2Prop*>(pEntity)->m_pActor->SetVelocity(LoadUnaligned3SIMD(&velocity.x));
	//static_cast<CPhysics2Prop*>(pEntity)->m_pBoxActor->;
}
