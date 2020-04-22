//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "portal_physics_collisionevent.h"
#include "physicsshadowclone.h"
#include "prop_combine_ball.h"
#include "portal_player.h"
#include "portal/weapon_physcannon.h" //grab controller


#define DEBUG_COLLISION_RULES 0

#if (DEBUG_COLLISION_RULES == 1)
ConVar sv_watchcollision_1( "sv_watchcollision_1", "-1" );
ConVar sv_watchcollision_2( "sv_watchcollision_2", "-1" );
#endif

int CPortal_CollisionEvent::ShouldCollide( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1 )
{
	if ( !pGameData0 || !pGameData1 )
		return 1;

#if (DEBUG_COLLISION_RULES == 1)
	bool bBreak = false;
	if( ((((CBaseEntity *)pGameData0)->entindex() == sv_watchcollision_1.GetInt()) || (((CBaseEntity *)pGameData0)->entindex() == sv_watchcollision_2.GetInt())) &&
		((((CBaseEntity *)pGameData1)->entindex() == sv_watchcollision_1.GetInt()) || (((CBaseEntity *)pGameData1)->entindex() == sv_watchcollision_2.GetInt())) )
	{
		bBreak = true;
	}
#endif


	AssertOnce( pObj0 && pObj1 );
	uint nFlags0 = pObj0->GetGameFlags();
	uint nFlags1 = pObj1->GetGameFlags();
	uint nAllFlags = nFlags0 | nFlags1;

	bool bShadowClonesInvolved = ( nAllFlags & FVPHYSICS_IS_SHADOWCLONE) != 0;

	if( bShadowClonesInvolved )
	{
		//at least one shadow clone

		if( (nFlags0 & nFlags1) & FVPHYSICS_IS_SHADOWCLONE )
			return 0; //both are shadow clones

		if( nAllFlags & FVPHYSICS_PLAYER_HELD )
		{
			//at least one is held

			//don't let players collide with objects they're holding, they get kinda messed up sometimes
			if( pGameData0 && ((CBaseEntity *)pGameData0)->IsPlayer() && (GetPlayerHeldEntity( (CBasePlayer *)pGameData0 ) == (CBaseEntity *)pGameData1) )
				return 0;

			if( pGameData1 && ((CBaseEntity *)pGameData1)->IsPlayer() && (GetPlayerHeldEntity( (CBasePlayer *)pGameData1 ) == (CBaseEntity *)pGameData0) )
				return 0;
		}
	}



	//everything is in one environment. This means we must tightly control what collides with what
	if( pGameData0 != pGameData1 )
	{
		//this code only decides what CAN'T collide due to portal environment differences, things that should collide will pass through here to deeper ShouldCollide() code
		bool bStatic[2] = { pObj0->IsStatic(), pObj1->IsStatic() };
		AssertOnce( (bStatic[0] && bStatic[1]) == false ); //hopefully the system doesn't even call in for this, they're both static and can't collide
		if( bStatic[0] && bStatic[1] )
			return 0;

		CBaseEntity *pEntities[2] = { (CBaseEntity *)pGameData0, (CBaseEntity *)pGameData1 };
		IPhysicsObject *pPhysObjects[2] = { pObj0, pObj1 };
		CPortalSimulator *pSimulators[2];
		pSimulators[0] = CPortalSimulator::GetSimulatorThatOwnsEntity( (CBaseEntity *)pGameData0 );
		pSimulators[1] = CPortalSimulator::GetSimulatorThatOwnsEntity( (CBaseEntity *)pGameData1 );


#ifdef _DEBUG
		for( int i = 0; i != 2; ++i )
		{
			if( (pSimulators[i] != NULL) && CPhysicsShadowClone::IsShadowClone( pEntities[i] ) )
			{
				CPhysicsShadowClone *pClone = (CPhysicsShadowClone *)pEntities[i];
				CBaseEntity *pSource = pClone->GetClonedEntity();

				CPortalSimulator *pSourceSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pClone )->GetLinkedPortalSimulator();
				Assert( (pSimulators[i]->GetInternalData().Simulation.Dynamic.EntFlags[pClone->entindex()] & PSEF_IS_IN_PORTAL_HOLE) == (pSourceSimulator->GetInternalData().Simulation.Dynamic.EntFlags[pSource->entindex()] & PSEF_IS_IN_PORTAL_HOLE) );
			}
		}
#endif

		if( (pSimulators[0] == pSimulators[1]) ) //same simulator
		{
			if( pSimulators[0] != NULL ) //and not main world
			{
				if( bStatic[0] || bStatic[1] )
				{
					for( int i = 0; i != 2; ++i )
					{
						if( bStatic[i] )
						{
							if( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntities[i] ) )
							{
								PS_PhysicsObjectSourceType_t objectSource;
								if(	pSimulators[i]->CreatedPhysicsObject( pPhysObjects[i], &objectSource ) && 
									((objectSource == PSPOST_REMOTE_BRUSHES) || (objectSource == PSPOST_REMOTE_STATICPROPS)) )
								{
									if( (pSimulators[1-i]->GetInternalData().Simulation.Dynamic.EntFlags[pEntities[1-i]->entindex()] & PSEF_IS_IN_PORTAL_HOLE) == 0 )
										return 0; //require that the entity be in the portal hole before colliding with transformed geometry
									//FIXME: The above requirement might fail horribly for transformed collision blocking the portal from the other side and fast moving objects
								}	
							}
							break;
						}
					}
				}
				else if( bShadowClonesInvolved )
				{
					if( ((pSimulators[0]->GetInternalData().Simulation.Dynamic.EntFlags[pEntities[0]->entindex()] | 
						pSimulators[1]->GetInternalData().Simulation.Dynamic.EntFlags[pEntities[1]->entindex()]) &
						PSEF_IS_IN_PORTAL_HOLE) == 0 )
					{
						return 0; //neither entity was actually in the portal hole
					}
				}
			}
		}
		else //different simulators
		{
			if( bShadowClonesInvolved ) //entities can only collide with shadow clones "owned" by the same simulator.
				return 0;

			if( bStatic[0] || bStatic[1] )
			{
				for( int i = 0; i != 2; ++i )
				{
					if( bStatic[i] )
					{
						int j = 1-i;
						CPortalSimulator *pSimulator_Entity = pSimulators[j];

						if( pEntities[i]->IsWorld() )
						{
							Assert( CPortalSimulator::GetSimulatorThatCreatedPhysicsObject( pPhysObjects[i] ) == NULL );
							if( pSimulator_Entity )
								return 0;
						}
						else
						{
							CPortalSimulator *pSimulator_Static = CPortalSimulator::GetSimulatorThatCreatedPhysicsObject( pPhysObjects[i] ); //might have been a static prop which would yield a new simulator
														
							if( pSimulator_Static )
							{
								if(pSimulator_Static != pSimulator_Entity)
									return 0; //static collideable is from a different simulator
							}
							else if( pSimulator_Entity )
							{
								if( (pSimulator_Entity->GetInternalData().Simulation.Dynamic.EntFlags[pEntities[i]->entindex()] & PSEF_CLONES_ENTITY_FROM_MAIN) == 0 )
								{
									//entity is in a portal environment, static is not, static not cloned from main.
									if( !pPhysObjects[i]->IsTrigger() ) //we should probably do this with triggers too. But it breaks tractor beams in devtest when the cube portals, too late in the ship cycle to chase the sweater thread without a good reason
										return 0; 
								}
							}
						}
						break;
					}
				}
			}
			else
			{
				Assert( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntities[0] ) == false );
				Assert( CPSCollisionEntity::IsPortalSimulatorCollisionEntity( pEntities[1] ) == false );

				for( int i = 0; i != 2; ++i )
				{
					if( pSimulators[i] )
					{
						//entities in the physics environment only collide with statics created by the environment (handled above), entities in the same environment (also above), or entities that should be cloned from main to the same environment
						if( (pSimulators[i]->GetInternalData().Simulation.Dynamic.EntFlags[pEntities[1-i]->entindex()] & PSEF_CLONES_ENTITY_FROM_MAIN) == 0 ) //not cloned from main
							return 0;
					}
				}
			}

		}
	}

	return BaseClass::ShouldCollide( pObj0, pObj1, pGameData0, pGameData1 );
}


static bool s_bPenetrationSolvingDisabled = false;
static CUtlStack<bool> s_DisablePenetatrationSolving;

void CPortal_CollisionEvent::DisablePenetrationSolving_Push( bool bDisable )
{
	s_DisablePenetatrationSolving.Push( bDisable );
	s_bPenetrationSolvingDisabled = bDisable;
}

void CPortal_CollisionEvent::DisablePenetrationSolving_Pop( void )
{
	s_DisablePenetatrationSolving.Pop();
	if( s_DisablePenetatrationSolving.Count() > 0 )
	{
		s_bPenetrationSolvingDisabled = s_DisablePenetatrationSolving.Top();
	}
	else
	{
		s_bPenetrationSolvingDisabled = false;
	}
}


int CPortal_CollisionEvent::ShouldSolvePenetration( IPhysicsObject *pObj0, IPhysicsObject *pObj1, void *pGameData0, void *pGameData1, float dt )
{
	if( s_bPenetrationSolvingDisabled )
		return 0;

	if( (pGameData0 == NULL) || (pGameData1 == NULL) )
		return 0;

	// For portal, don't solve penetrations on combine balls
	if( FClassnameIs( (CBaseEntity *)pGameData0, "prop_energy_ball" ) ||
		FClassnameIs( (CBaseEntity *)pGameData1, "prop_energy_ball" ) )
		return 0;

	if( (pObj0->GetGameFlags() | pObj1->GetGameFlags()) & FVPHYSICS_PLAYER_HELD )
	{
		//at least one is held
		CBaseEntity *pHeld;
		CBaseEntity *pOther;
		IPhysicsObject *pPhysHeld;
		IPhysicsObject *pPhysOther;
		if( pObj0->GetGameFlags() & FVPHYSICS_PLAYER_HELD )
		{
			pHeld = (CBaseEntity *)pGameData0;
			pPhysHeld = pObj0;
			pOther = (CBaseEntity *)pGameData1;
			pPhysOther = pObj1;
		}
		else
		{
			pHeld = (CBaseEntity *)pGameData1;
			pPhysHeld = pObj1;
			pOther = (CBaseEntity *)pGameData0;
			pPhysOther = pObj0;
		}

		//don't let players collide with objects they're holding, they get kinda messed up sometimes
		if( pOther->IsPlayer() && GetPlayerHeldEntity( (CBasePlayer *)pOther ) == pHeld )
		{
			return 0;
		}

		//held objects are clipping into other objects when travelling across a portal. We're close to ship, so this seems to be the
		//most localized way to make a fix.
		//Note that we're not actually going to change whether it should solve, we're just going to tack on some hacks
		CPortal_Player *pHoldingPlayer = (CPortal_Player *)GetPlayerHoldingEntity( pHeld );
		if( !pHoldingPlayer && CPhysicsShadowClone::IsShadowClone( pHeld ) )
		{
			pHoldingPlayer = (CPortal_Player *)GetPlayerHoldingEntity( ((CPhysicsShadowClone *)pHeld)->GetClonedEntity() );
		}
		
		Assert( pHoldingPlayer );
		if ( pHoldingPlayer && !pHoldingPlayer->IsUsingVMGrab() )
		{
			CGrabController *pGrabController = GetGrabControllerForPlayer( pHoldingPlayer );

			if ( !pGrabController )
				pGrabController = GetGrabControllerForPhysCannon( pHoldingPlayer->GetActiveWeapon() );

			Assert( pGrabController );
			if( pGrabController )
			{
				GrabController_SetPortalPenetratingEntity( pGrabController, pOther );
			}

			//NDebugOverlay::EntityBounds( pHeld, 0, 0, 255, 16, 1.0f );
			//NDebugOverlay::EntityBounds( pOther, 255, 0, 0, 16, 1.0f );
			//pPhysOther->Wake();
			//FindClosestPassableSpace( pOther, Vector( 0.0f, 0.0f, 1.0f ) );
		}
	}


	if( (pObj0->GetGameFlags() | pObj1->GetGameFlags()) & FVPHYSICS_IS_SHADOWCLONE ) 
	{
		//at least one shadowclone is involved

		if( (pObj0->GetGameFlags() & pObj1->GetGameFlags()) & FVPHYSICS_IS_SHADOWCLONE ) //don't solve between two shadowclones, they're just going to resync in a frame anyways
			return 0;



		IPhysicsObject * const pObjects[2] = { pObj0, pObj1 };

		for( int i = 0; i != 2; ++i )
		{
			if( pObjects[i]->GetGameFlags() & FVPHYSICS_IS_SHADOWCLONE )
			{
				int j = 1 - i;
				if( !pObjects[j]->IsMoveable() )
					return 0; //don't solve between shadow clones and statics

				if( ((CPhysicsShadowClone *)(pObjects[i]->GetGameData()))->GetClonedEntity() == (pObjects[j]->GetGameData()) )
					return 0; //don't solve between a shadow clone and its source entity
			}
		}	
	}

	return BaseClass::ShouldSolvePenetration( pObj0, pObj1, pGameData0, pGameData1, dt );
}



// Data for energy ball vs held item mass swapping hack
static float s_fSavedMass[2];
static bool s_bChangedMass[2] = { false, false };
static bool s_bUseUnshadowed[2] = { false, false };
static IPhysicsObject *s_pUnshadowed[2] = { NULL, NULL };


static void ModifyWeight_PreCollision( vcollisionevent_t *pEvent )
{
	Assert( (pEvent->pObjects[0] != NULL) && (pEvent->pObjects[1] != NULL) );

	CBaseEntity *pUnshadowedEntities[2];
	IPhysicsObject *pUnshadowedObjects[2];

	for( int i = 0; i != 2; ++i )
	{
		if( pEvent->pObjects[i]->GetGameFlags() & FVPHYSICS_IS_SHADOWCLONE )
		{
			CPhysicsShadowClone *pClone = ((CPhysicsShadowClone *)pEvent->pObjects[i]->GetGameData());
			pUnshadowedEntities[i] = pClone->GetClonedEntity();
		
			if( pUnshadowedEntities[i] == NULL )
				return;

            pUnshadowedObjects[i] = pClone->TranslatePhysicsToClonedEnt( pEvent->pObjects[i] );

			if( pUnshadowedObjects[i] == NULL )
				return;
		}
		else
		{
			pUnshadowedEntities[i] = (CBaseEntity *)pEvent->pObjects[i]->GetGameData();
			pUnshadowedObjects[i] = pEvent->pObjects[i];
		}		
	}

	// HACKHACK: Reduce mass for combine ball vs movable brushes so the collision
	// appears fully elastic regardless of mass ratios
	for( int i = 0; i != 2; ++i )
	{
		int j = 1-i;

		// One is a combine ball, if the other is a movable brush, reduce the combine ball mass
		if ( pUnshadowedEntities[i] != NULL && FClassnameIs( pUnshadowedEntities[j], "prop_combine_ball") )
		{
			if ( pUnshadowedEntities[i]->GetMoveType() == MOVETYPE_PUSH )
			{
				s_bChangedMass[j] = true;
				s_fSavedMass[j] = pUnshadowedObjects[j]->GetMass();
				pEvent->pObjects[j]->SetMass( VPHYSICS_MIN_MASS );
				if( pUnshadowedObjects[j] != pEvent->pObjects[j] )
				{
					s_bUseUnshadowed[j] = true;
					s_pUnshadowed[j] = pUnshadowedObjects[j];

					pUnshadowedObjects[j]->SetMass( VPHYSICS_MIN_MASS );
				}
			}

			//HACKHACK: last minute problem knocking over turrets with energy balls, up the mass of the ball by a lot
			if( FClassnameIs( pUnshadowedEntities[i], "npc_portal_turret_floor" ) )
			{
				pUnshadowedObjects[j]->SetMass( pUnshadowedEntities[i]->VPhysicsGetObject()->GetMass() );
			}
		}
	}

	for( int i = 0; i != 2; ++i )
	{
		if( ( pUnshadowedObjects[i] && pUnshadowedObjects[i]->GetGameFlags() & FVPHYSICS_PLAYER_HELD ) )
		{
			int j = 1-i;
			if( FClassnameIs( pUnshadowedEntities[j], "prop_combine_ball") )
			{			
				// [j] is the combine ball, set mass low
				// if the above ball vs brush entity check didn't already change the mass, change the mass
				if ( !s_bChangedMass[j] )
				{
					s_bChangedMass[j] = true;
					s_fSavedMass[j] = pUnshadowedObjects[j]->GetMass();
					pEvent->pObjects[j]->SetMass( VPHYSICS_MIN_MASS );
					if( pUnshadowedObjects[j] != pEvent->pObjects[j] )
					{
						s_bUseUnshadowed[j] = true;
						s_pUnshadowed[j] = pUnshadowedObjects[j];

						pUnshadowedObjects[j]->SetMass( VPHYSICS_MIN_MASS );
					}
				}
				
				// [i] is the held object, set mass high
				s_bChangedMass[i] = true;
				s_fSavedMass[i] = pUnshadowedObjects[i]->GetMass();
				pEvent->pObjects[i]->SetMass( VPHYSICS_MAX_MASS );
				if( pUnshadowedObjects[i] != pEvent->pObjects[i] )
				{
					s_bUseUnshadowed[i] = true;
					s_pUnshadowed[i] = pUnshadowedObjects[i];

					pUnshadowedObjects[i]->SetMass( VPHYSICS_MAX_MASS );
				}
			}
			else if( pEvent->pObjects[j]->GetGameFlags() & FVPHYSICS_IS_SHADOWCLONE )
			{
				//held object vs shadow clone, set held object mass back to grab controller saved mass
				
				// [i] is the held object
				s_bChangedMass[i] = true;
				s_fSavedMass[i] = pUnshadowedObjects[i]->GetMass();

				CGrabController *pGrabController = NULL;
				CBaseEntity *pLookingForEntity = (CBaseEntity*)pEvent->pObjects[i]->GetGameData();
				CBasePlayer *pHoldingPlayer = GetPlayerHoldingEntity( pLookingForEntity );
				if( pHoldingPlayer )
					pGrabController = GetGrabControllerForPlayer( pHoldingPlayer );

				float fSavedMass, fSavedRotationalDamping;

				AssertMsg( pGrabController, "Physics object is held, but we can't find the holding controller." );
				GetSavedParamsForCarriedPhysObject( pGrabController, pUnshadowedObjects[i], &fSavedMass, &fSavedRotationalDamping );

				pEvent->pObjects[i]->SetMass( fSavedMass );
				if( pUnshadowedObjects[i] != pEvent->pObjects[i] )
				{
					s_bUseUnshadowed[i] = true;
					s_pUnshadowed[i] = pUnshadowedObjects[i];

					pUnshadowedObjects[i]->SetMass( fSavedMass );
				}
			}
		}
	}
}



void CPortal_CollisionEvent::PreCollision( vcollisionevent_t *pEvent )
{
	ModifyWeight_PreCollision( pEvent );
	return BaseClass::PreCollision( pEvent );
}


static void ModifyWeight_PostCollision( vcollisionevent_t *pEvent )
{
	for( int i = 0; i != 2; ++i )
	{
		if( s_bChangedMass[i] )
		{
			pEvent->pObjects[i]->SetMass( s_fSavedMass[i] );
			if( s_bUseUnshadowed[i] )
			{
				s_pUnshadowed[i]->SetMass( s_fSavedMass[i] );
				s_bUseUnshadowed[i] = false;
			}
			s_bChangedMass[i] = false;
		}
	}
}

void CPortal_CollisionEvent::PostCollision( vcollisionevent_t *pEvent )
{
	ModifyWeight_PostCollision( pEvent );

	return BaseClass::PostCollision( pEvent );
}

void CPortal_CollisionEvent::PostSimulationFrame()
{
	//this actually happens once per physics environment simulation, and we don't want that, so do nothing and we'll get a different version manually called
}

void CPortal_CollisionEvent::PortalPostSimulationFrame( void )
{
	BaseClass::PostSimulationFrame();
}


void CPortal_CollisionEvent::AddDamageEvent( CBaseEntity *pEntity, const CTakeDamageInfo &info, IPhysicsObject *pInflictorPhysics, bool bRestoreVelocity, const Vector &savedVel, const AngularImpulse &savedAngVel )
{
	const CTakeDamageInfo *pPassDownInfo = &info;
	CTakeDamageInfo ReplacementDamageInfo; //only used some of the time

	if( (info.GetDamageType() & DMG_CRUSH) &&
		(pInflictorPhysics->GetGameFlags() & FVPHYSICS_IS_SHADOWCLONE) &&
		(!info.BaseDamageIsValid()) &&
		(info.GetDamageForce().LengthSqr() > (20000.0f * 20000.0f)) 
		)
	{
		//VERY likely this was caused by the penetration solver. Since a shadow clone is involved we're going to ignore it becuase it causes more problems than it solves in this case
		ReplacementDamageInfo = info;
		ReplacementDamageInfo.SetDamage( 0.0f );
		pPassDownInfo = &ReplacementDamageInfo;
	}

	BaseClass::AddDamageEvent( pEntity, *pPassDownInfo, pInflictorPhysics, bRestoreVelocity, savedVel, savedAngVel );
}























