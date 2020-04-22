//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side implementation of CBaseCombatWeapon.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "history_resource.h"
#include "iclientmode.h"
#include "iinput.h"
#include "weapon_selection.h"
#include "hud_crosshair.h"
#include "engine/ivmodelinfo.h"
#include "tier0/vprof.h"
#include "hltvcamera.h"
#include "tier1/keyvalues.h"
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#if defined( CSTRIKE15 )
#include "c_cs_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::SetDormant( bool bDormant )
{
	// If I'm going from active to dormant and I'm carried by another player, holster me.
	if ( !IsDormant() && bDormant && GetOwner() && !IsCarriedByLocalPlayer() )
	{
		Holster( NULL );
	}

	BaseClass::SetDormant( bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit(state);

	if (state == SHOULDTRANSMIT_END)
	{
		if (m_iState == WEAPON_IS_ACTIVE)
		{
			m_iState = WEAPON_IS_CARRIED_BY_PLAYER;
		}
	}
	else if( state == SHOULDTRANSMIT_START )
	{
		// We don't check the value of m_iState because if the weapon is active, we need the state to match reguardless
		// of what it was.
		if( GetOwner() && GetOwner()->GetActiveWeapon() == this )
		{
			// Restore the Activeness of the weapon if we client-twiddled it off in the first case above.
			m_iState = WEAPON_IS_ACTIVE;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: To wrap PORTAL mod specific functionality into one place
//-----------------------------------------------------------------------------
static inline bool ShouldDrawLocalPlayer( C_BasePlayer *pl )
{
#if defined( PORTAL )
	return true;
#else
	Assert( pl );
	return pl->ShouldDrawLocalPlayer();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::OnRestore()
{
	BaseClass::OnRestore();

	// if the player is holding this weapon, 
	// mark it as just restored so it won't show as a new pickup
	if ( C_BasePlayer::IsLocalPlayer( GetOwner() ) )
	{
		m_bJustRestored = true;
	}
}

void C_BaseCombatWeapon::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::OnDataChanged( DataUpdateType_t updateType )
{
	// Must update visibility prior to calling OnDataChanged so that
	// shadows are dealt with correctly.
	UpdateVisibility();

	BaseClass::OnDataChanged( updateType );

	// let the world model know we're updating, in case it wants to as well
	CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();
	if ( pWeaponWorldModel )
	{
		pWeaponWorldModel->OnDataChanged( updateType );
	}

	// If it's being carried by the *local* player, on the first update,
	// find the registered weapon for this ID

	C_BaseCombatCharacter *pOwner = GetOwner();
	C_BasePlayer *pPlayer = ToBasePlayer( pOwner );

#if defined( CSTRIKE15 )
	C_CSPlayer *pObservedPlayer = GetHudPlayer();
	// check if weapon was dropped by local player or the player we are observing
	if ( pObservedPlayer == pPlayer && pObservedPlayer->State_Get() == STATE_ACTIVE )
#else
	// check if weapon is carried by local player
	bool bIsLocalPlayer = C_BasePlayer::IsLocalPlayer( pPlayer );
	if ( bIsLocalPlayer )
#endif
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( C_BasePlayer::GetSplitScreenSlotForPlayer( pPlayer ) );

		// If I was just picked up, or created & immediately carried, add myself to this client's list of weapons
		if ( m_iState != WEAPON_NOT_CARRIED && m_iOldState == WEAPON_NOT_CARRIED )
		{
			// Tell the HUD this weapon's been picked up
			if ( ShouldDrawPickup() )
			{
				CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
				if ( pHudSelection )
				{
					pHudSelection->OnWeaponPickup( this );
				}
			}
		}
	}
	
	UpdateVisibility();

	m_iOldState = m_iState;

	m_bJustRestored = false;

}

//-----------------------------------------------------------------------------
// Is anyone carrying it?
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsBeingCarried() const
{
	return ( m_hOwner.Get() != NULL );
}

//-----------------------------------------------------------------------------
// Is the carrier alive?
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsCarrierAlive() const
{
	if ( !m_hOwner.Get() )
		return false;

	return m_hOwner.Get()->GetHealth() > 0;
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_BaseCombatWeapon::ShadowCastType()
{
	if ( IsEffectActive( /*EF_NODRAW |*/ EF_NOSHADOW ) )
		return SHADOWS_NONE;

	if (!IsBeingCarried())
		return SHADOWS_RENDER_TO_TEXTURE;

	if (IsCarriedByLocalPlayer())
		return SHADOWS_NONE;

	return (m_iState != WEAPON_IS_CARRIED_BY_PLAYER) ? SHADOWS_RENDER_TO_TEXTURE : SHADOWS_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: This weapon is the active weapon, and it should now draw anything
//			it wants to. This gets called every frame.
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::Redraw()
{
	if ( GetClientMode()->ShouldDrawCrosshair() )
	{
		DrawCrosshair();
	}

	// ammo drawing has been moved into hud_ammo.cpp
}
//-----------------------------------------------------------------------------
// Purpose: Draw the weapon's crosshair
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::DrawCrosshair()
{
#ifndef INFESTED_DLL
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return;

	Color clr = GetHud().m_clrNormal;
/*

	// TEST: if the thing under your crosshair is on a different team, light the crosshair with a different color.
	Vector vShootPos, vShootAngles;
	GetShootPosition( vShootPos, vShootAngles );

	Vector vForward;
	AngleVectors( vShootAngles, &vForward );
	
	
	// Change the color depending on if we're looking at a friend or an enemy.
	CPartitionFilterListMask filter( PARTITION_ALL_CLIENT_EDICTS );	
	trace_t tr;
	traceline->TraceLine( vShootPos, vShootPos + vForward * 10000, COLLISION_GROUP_NONE, MASK_SHOT, &tr, true, ~0, &filter );

	if ( tr.index != 0 && tr.index != INVALID_CLIENTENTITY_HANDLE )
	{
		C_BaseEntity *pEnt = ClientEntityList().GetBaseEntityFromHandle( tr.index );
		if ( pEnt )
		{
			if ( pEnt->GetTeamNumber() != player->GetTeamNumber() )
			{
				g = b = 0;
			}
		}
	}		 
*/

	CHudCrosshair *crosshair = GET_HUDELEMENT( CHudCrosshair );
	if ( !crosshair )
		return;

	// Find out if this weapon's auto-aimed onto a target
	bool bOnTarget = ( m_iState == WEAPON_IS_ACTIVE ) && player->m_fOnTarget;
	
	if ( player->GetFOV() >= 90 )
	{ 
		// normal crosshairs
		if ( bOnTarget && GetWpnData().iconAutoaim )
		{
			clr[3] = 255;

			crosshair->SetCrosshair( GetWpnData().iconAutoaim, clr );
		}
		else if ( GetWpnData().iconCrosshair )
		{
			clr[3] = 255;
			crosshair->SetCrosshair( GetWpnData().iconCrosshair, clr );
		}
		else
		{
			crosshair->ResetCrosshair();
		}
	}
	else
	{ 
		Color white( 255, 255, 255, 255 );

		// zoomed crosshairs
		if (bOnTarget && GetWpnData().iconZoomedAutoaim)
			crosshair->SetCrosshair(GetWpnData().iconZoomedAutoaim, white);
		else if ( GetWpnData().iconZoomedCrosshair )
			crosshair->SetCrosshair( GetWpnData().iconZoomedCrosshair, white );
		else
			crosshair->ResetCrosshair();
	}
	#endif
}
//-----------------------------------------------------------------------------
// Purpose: This weapon is the active weapon, and the viewmodel for it was just drawn.
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::ViewModelDrawn( int nFlags, C_BaseViewModel *pViewModel )
{
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this client's carrying this weapon
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsCarriedByLocalPlayer( void )
{
	if ( !GetOwner() )
		return false;

	return ( C_BasePlayer::IsLocalPlayer( GetOwner() ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this weapon is the local client's currently wielded weapon
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::IsActiveByLocalPlayer( void )
{
	if ( IsCarriedByLocalPlayer() )
	{
		return (m_iState == WEAPON_IS_ACTIVE);
	}

	return false;
}

bool C_BaseCombatWeapon::GetShootPosition( Vector &vOrigin, QAngle &vAngles )
{
	// Get the entity because the weapon doesn't have the right angles.
	C_BaseCombatCharacter *pEnt = ToBaseCombatCharacter( GetOwner() );
	if ( pEnt )
	{
		if ( pEnt == C_BasePlayer::GetLocalPlayer() )
		{
			vAngles = pEnt->EyeAngles();
		}
		else
		{
			vAngles = pEnt->GetRenderAngles();	
		}
	}
	else
	{
		vAngles.Init();
	}

	C_BasePlayer *player = ToBasePlayer( pEnt );
	bool bUseViewModel = false;
	if ( C_BasePlayer::IsLocalPlayer( pEnt ) )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( pEnt );
		bUseViewModel = !player->ShouldDrawLocalPlayer();
	}

	QAngle vDummy;
	if ( IsActiveByLocalPlayer() && bUseViewModel )
	{
		C_BaseViewModel *vm = player ? player->GetViewModel( 0 ) : NULL;
		if ( vm )
		{
			int iAttachment = vm->LookupAttachment( "muzzle" );
			if ( vm->GetAttachment( iAttachment, vOrigin, vDummy ) )
			{
				return true;
			}
		}
	}
	else
	{
		// Thirdperson
		int iAttachment = LookupAttachment( "muzzle" );
		if ( GetAttachment( iAttachment, vOrigin, vDummy ) )
		{
			return true;
		}
	}

	vOrigin = GetRenderOrigin();
	return false;
}

bool C_BaseCombatWeapon::ShouldSuppressForSplitScreenPlayer( int nSlot )
{
	if ( BaseClass::ShouldSuppressForSplitScreenPlayer( nSlot ) )
	{
		return true;
	}
	
	C_BaseCombatCharacter *pOwner = GetOwner();
	
	// If the owner of this weapon is not allowed to draw in this split screen slot, then don't draw the weapon either.
	if ( pOwner && pOwner->ShouldSuppressForSplitScreenPlayer( nSlot ) )
	{
		return true;
	}
	
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( nSlot );

	// Carried by local player?
	// Only draw the weapon if we're in some kind of 3rd person mode because the viewmodel will do that otherwise.
	if ( pOwner == pLocalPlayer && !pLocalPlayer->ShouldDrawLocalPlayer() )
	{
		return true;
	}
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::ShouldDraw( void )
{

	if ( IsEffectActive( EF_NODRAW ) )
		return false;

	// weapon has no owner, always draw it
	if ( !GetOwner() && !IsViewModel() )
	{
		return true;
	}
	else
	{
		CBaseWeaponWorldModel *pWeaponWorldModel = GetWeaponWorldModel();
		if ( pWeaponWorldModel )
		{
			return false; // the weapon world model will render in our place if it exists
		}
		else
		{
			// render only if equipped. The holstered model will render for weapons that aren't equipped
			return ( GetOwner() && GetOwner()->GetActiveWeapon() == this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if a weapon-pickup icon should be displayed when this weapon is received
//-----------------------------------------------------------------------------
bool C_BaseCombatWeapon::ShouldDrawPickup( void )
{
	if ( GetWeaponFlags() & ITEM_FLAG_NOITEMPICKUP )
		return false;

	if ( m_bJustRestored )
		return false;

	return true;
}

bool C_BaseCombatWeapon::IsFirstPersonSpectated( void )
{
	// check if local player chases owner of this weapon in first person
	C_BasePlayer *localplayer = C_BasePlayer::GetLocalPlayer();
	if ( localplayer && localplayer->IsObserver() && GetOwner() )
	{
		// don't draw weapon if chasing this guy as spectator
		// we don't check that in ShouldDraw() since this may change
		// without notification 
		if ( localplayer->GetObserverMode() == OBS_MODE_IN_EYE &&
			localplayer->GetObserverTarget() == GetOwner() &&
			localplayer->GetObserverInterpState() != C_BasePlayer::OBSERVER_INTERP_TRAVELING ) 
			return true;
	}

	return false;
}
		   

//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
IClientModelRenderable*	C_BaseCombatWeapon::GetClientModelRenderable()
{
	if ( !m_bReadyToDraw )
		return 0;

	if( IsFirstPersonSpectated() )
		return NULL;

	VerifyAndSetContextSensitiveWeaponModel();

	return BaseClass::GetClientModelRenderable();
}


//-----------------------------------------------------------------------------
// Purpose: Render the weapon. Draw the Viewmodel if the weapon's being carried
//			by this player, otherwise draw the worldmodel.
//-----------------------------------------------------------------------------
int C_BaseCombatWeapon::DrawModel( int flags, const RenderableInstance_t &instance )
{
	VPROF_BUDGET( "C_BaseCombatWeapon::DrawModel", VPROF_BUDGETGROUP_MODEL_RENDERING );
	if ( !m_bReadyToDraw )
		return 0;

	if ( !IsVisible() )
		return 0;

	if( IsFirstPersonSpectated() )
		return 0;

	return BaseClass::DrawModel( flags, instance );
}

#ifdef _DEBUG
	ConVar stickers_enabled_thirdperson( "stickers_enabled_thirdperson", "1", FCVAR_DEVELOPMENTONLY, "Enable work-in-progress stickers on worldmodels." );

	ConVar stickers_debug_thirdperson_constant( "stickers_debug_thirdperson_constant", "0", FCVAR_DEVELOPMENTONLY, "Continually apply stickers on worldmodels. SLOW" );
	ConVar stickers_debug_thirdperson_ray( "stickers_debug_thirdperson_ray", "0", FCVAR_DEVELOPMENTONLY, "Show worldmodel sticker projection position and direction." );
	ConVar stickers_debug_thirdperson_ray_duration( "stickers_debug_thirdperson_ray_duration", "0.1", FCVAR_DEVELOPMENTONLY, "Duration to show sticker projection lines." );
	ConVar stickers_debug_thirdperson_size( "stickers_debug_thirdperson_size", "0", FCVAR_DEVELOPMENTONLY, "Override worldmodel sticker size." );

	extern ConVar stickers_debug_randomize;
#endif

void C_BaseCombatWeapon::ApplyThirdPersonStickers( C_BaseAnimating *pWeaponModelTargetOverride )
{
#ifdef _DEBUG
	if (stickers_enabled_thirdperson.GetBool() == 0)
		return;
#endif

	if ( !IsAbsQueriesValid() )
		return;

	CEconItemView *pItem = GetEconItemView();
	if (!pItem)
		return;

	if ( pItem->GetNumSupportedStickerSlots() == 0 )
		return;
	
	C_BaseAnimating *pTargetModel = pWeaponModelTargetOverride ? pWeaponModelTargetOverride : this;

	if ( !pTargetModel->ShouldDraw() )
		return;
	
	MDLCACHE_CRITICAL_SECTION();

	pTargetModel->CreateModelInstance();
	if ( pTargetModel->GetModelInstance() != MODEL_INSTANCE_INVALID )
	{
		if ( modelrender->ModelHasDecals( pTargetModel->GetModelInstance() ) &&
#ifdef _DEBUG
			!stickers_debug_thirdperson_constant.GetBool() &&
#endif
			pItem->ItemHasAnyStickersApplied() )
		{
			return; //decals are already applied
		}
		modelrender->RemoveAllDecals( pTargetModel->GetModelInstance() );
	}
	else
	{
		return; //no model instance
	}

	matrix3x4_t weaponBoneTransform;
	
	const char* const szStickerBoneLookupTable[] = {
		"sticker_a",
		"sticker_b",
		"sticker_c",
		"sticker_d",
		"sticker_e"
	};

	for (int i=0; i<pItem->GetNumSupportedStickerSlots(); i++ )
	{
		IMaterial *pStickerMaterialThirdPerson = pItem->GetStickerIMaterialBySlotIndex(i, true);

		if ( !pStickerMaterialThirdPerson )
		{
#ifdef _DEBUG
			if ( stickers_debug_randomize.GetBool() )
			{
				pItem->GenerateStickerMaterials();
				pStickerMaterialThirdPerson = pItem->GetStickerIMaterialBySlotIndex(i, true);
			}
			else
			{
				continue;
			}
#else
			continue; //sticker material is not valid
#endif
		}

		//if the weapon model has world model sticker projection points, use those
		//otherwise use schema values
		
		Vector vecWorldRayOrigin, vecWorldRayDirection;

		bool bWeaponProvidedStickerAttachments = false;

		int nStickerAttachmentBoneIndex = pTargetModel->LookupBone( szStickerBoneLookupTable[i] );
		if ( nStickerAttachmentBoneIndex != -1 )
		{
			pTargetModel->GetBoneTransform( nStickerAttachmentBoneIndex, weaponBoneTransform );
			MatrixPosition( weaponBoneTransform, vecWorldRayOrigin );
			Vector a,b;
			MatrixVectors( weaponBoneTransform, &vecWorldRayDirection, &a, &b );
			bWeaponProvidedStickerAttachments = true;
		}
		else
		{

			int nBIndex = pTargetModel->LookupBone( pItem->GetStickerWorldModelBoneParentNameBySlotIndex(i) );
			if ( nBIndex == -1 )
				continue; //couldn't find the parent bone this sticker slot wanted

			pTargetModel->GetBoneTransform( nBIndex, weaponBoneTransform );

			Ray_t stickerRayLocal;
			stickerRayLocal.Init( pItem->GetStickerSlotWorldProjectionStartBySlotIndex( i ), 
									pItem->GetStickerSlotWorldProjectionEndBySlotIndex( i ) );

			VectorTransform( stickerRayLocal.m_Start, weaponBoneTransform, vecWorldRayOrigin );
			VectorRotate( stickerRayLocal.m_Delta, weaponBoneTransform, vecWorldRayDirection );

		}

#ifdef _DEBUG
		if ( stickers_debug_thirdperson_ray.GetBool() )
		{
			debugoverlay->AddBoxOverlay( vecWorldRayOrigin, Vector(-0.25,-0.25,-0.25), Vector(0.5,0.5,0.5), QAngle(0,0,0), 255, 0, 0, 180, stickers_debug_thirdperson_ray_duration.GetFloat() );
			debugoverlay->AddLineOverlay( vecWorldRayOrigin, vecWorldRayOrigin + vecWorldRayDirection, 0,255,0, true, stickers_debug_thirdperson_ray_duration.GetFloat() );
		}
#endif

		Ray_t stickerRayWorld;
		stickerRayWorld.Init( vecWorldRayOrigin, vecWorldRayOrigin + (3*vecWorldRayDirection) );

		VMatrix vmatrix_weaponBoneTransform( weaponBoneTransform );

		Vector vecStickerUp = vmatrix_weaponBoneTransform.GetLeft();
		if ( bWeaponProvidedStickerAttachments )
		{
			//content defined sticker attachments are z-up
			vecStickerUp = -vmatrix_weaponBoneTransform.GetUp();
		}

#ifdef _DEBUG
		float flStickerSize = (stickers_debug_thirdperson_size.GetFloat() == 0) ? stickers_debug_thirdperson_size.GetFloat() : 1.2f;
		pTargetModel->GetBaseEntity()->AddStudioMaterialDecal( stickerRayWorld, pStickerMaterialThirdPerson, flStickerSize, vecStickerUp );
#else
		pTargetModel->GetBaseEntity()->AddStudioMaterialDecal( stickerRayWorld, pStickerMaterialThirdPerson, 1.2f, vecStickerUp );
#endif
		
	}

	//Msg( "Applied stickers to: %s\n", this->GetName() );
	
}

//-----------------------------------------------------------------------------
// tool recording
//-----------------------------------------------------------------------------
void C_BaseCombatWeapon::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	Assert(false);
}
