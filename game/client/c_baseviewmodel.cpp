//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side view model implementation. Responsible for drawing
//			the view model.
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "c_baseviewmodel.h"
#include "model_types.h"
#include "hud.h"
#include "view_shared.h"
#include "iviewrender.h"
#include "view.h"
#include "mathlib/vmatrix.h"
#include "cl_animevent.h"
#include "eventlist.h"
#include "tools/bonelist.h"
#include <keyvalues.h>
#include "hltvcamera.h"
#include "r_efx.h"
#include "dlight.h"
#include "clientalphaproperty.h"
#include "iinput.h"
#include "cs_shareddefs.h"
#include "c_cs_player.h"

#include "weapon_csbase.h"
#include "weapon_basecsgrenade.h"
#include "iclientmode.h"

#include "platforminputdevice.h"
#include "inputsystem/iinputsystem.h"

#include "materialsystem/imaterialvar.h"

#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CSTRIKE_DLL
	ConVar cl_righthand( "cl_righthand", "1", FCVAR_ARCHIVE | FCVAR_SS, "Use right-handed view models." );
	ConVar vm_draw_addon( "vm_draw_addon", "1" );
	SplitScreenConVarRef ss_righthand( "cl_righthand", true );
#endif
ConVar vm_debug( "vm_debug", "0", FCVAR_CHEAT );
ConVar vm_draw_always( "vm_draw_always", "0", FCVAR_CHEAT, "1 - Always draw view models, 2 - Never draw view models.  Should be done before map launches." );
ConVar vm_pointer_pitch_up_scale( "vm_pointer_pitch_up_scale", "0.25", FCVAR_DEVELOPMENTONLY, "Limit how much the view model follows the pointer in looking up." );

#ifdef _DEBUG
	ConVar stickers_enabled_firstperson( "stickers_enabled_firstperson", "1", FCVAR_DEVELOPMENTONLY, "Enable work-in-progress stickers on viewmodels." );
	ConVar stickers_debug_randomize( "stickers_debug_randomize", "0", FCVAR_DEVELOPMENTONLY, "All weapons fill all slots with random stickers." );
#endif

void PostToolMessage( HTOOLHANDLE hEntity, KeyValues *msg );
extern float g_flMuzzleFlashScale;
extern ConVar r_drawviewmodel;
extern ConVar cl_righthand;

ConVar mat_preview( "mat_preview", "", FCVAR_CLIENTDLL | FCVAR_CHEAT );

// [mlowrance] used for flame effect when pin pulled
#define MOLOTOV_PARTICLE_EFFECT_NAME "weapon_molotov_fp"

void FormatViewModelAttachment( C_BasePlayer *pPlayer, Vector &vOrigin, bool bInverse )
{
	int nSlot = 0;
	if ( pPlayer ) 
	{
		int nPlayerSlot = C_BasePlayer::GetSplitScreenSlotForPlayer( pPlayer );
		if ( nPlayerSlot == -1 )
		{
			nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		}
		else
		{
			nSlot = nPlayerSlot;
		}
	}

	Assert( nSlot != -1 );

	// Presumably, SetUpView has been called so we know our FOV and render origin.
	const CViewSetup *pViewSetup = view->GetPlayerViewSetup( nSlot );
	
	float worldx = tan( pViewSetup->fov * M_PI/360.0 );
	float viewx = tan( pViewSetup->fovViewmodel * M_PI/360.0 );

	// aspect ratio cancels out, so only need one factor
	// the difference between the screen coordinates of the 2 systems is the ratio
	// of the coefficients of the projection matrices (tan (fov/2) is that coefficient)
	float factorX = worldx / viewx;

	float factorY = factorX;
	
	// Get the coordinates in the viewer's space.
	Vector tmp = vOrigin - pViewSetup->origin;
	Vector vTransformed( MainViewRight(nSlot).Dot( tmp ), MainViewUp(nSlot).Dot( tmp ), MainViewForward(nSlot).Dot( tmp ) );

	// Now squash X and Y.
	if ( bInverse )
	{
		if ( factorX != 0 && factorY != 0 )
		{
			vTransformed.x /= factorX;
			vTransformed.y /= factorY;
		}
		else
		{
			vTransformed.x = 0.0f;
			vTransformed.y = 0.0f;
		}
	}
	else
	{
		vTransformed.x *= factorX;
		vTransformed.y *= factorY;
	}



	// Transform back to world space.
	Vector vOut = (MainViewRight(nSlot) * vTransformed.x) + (MainViewUp(nSlot) * vTransformed.y) + (MainViewForward(nSlot) * vTransformed.z);
	vOrigin = pViewSetup->origin + vOut;
}

void Precache( void )
{
	PrecacheParticleSystem( MOLOTOV_PARTICLE_EFFECT_NAME );

//	BaseClass::Precache();
}

void C_BaseViewModel::UpdateStatTrakGlow( void )
{
	//approach the ideal in 2 seconds
	m_flStatTrakGlowMultiplier = Approach( m_flStatTrakGlowMultiplierIdeal, m_flStatTrakGlowMultiplier, (gpGlobals->frametime * 0.5) );
}

void C_BaseViewModel::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	// [msmith] We want the split screen visibility of the particles to be the same as the visibility of the view model.
	uint32 visBits = m_VisibilityBits.Get(0, 0xff);
	int nSlot = -1;

	if ( visBits < 3 )
	{
		// If visBits is 3 then it is both slots 1 and 2 meaning all users can see this effect.
		// If visBits is less than 3, then it is either slot 1 or 2 (but zero based means slot 0 or 1).
		nSlot = visBits-1;
	}

	pNewParticleEffect->SetDrawOnlyForSplitScreenUser( nSlot );

	if ( FStrEq( pszParticleName, MOLOTOV_PARTICLE_EFFECT_NAME ) )
	{
		m_viewmodelParticleEffect = pNewParticleEffect;
	}
}

void C_BaseViewModel::OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect )
{
	BaseClass::OnParticleEffectDeleted( pParticleEffect );

	if ( m_viewmodelParticleEffect == pParticleEffect )
	{
		m_viewmodelParticleEffect = NULL;
	}
}

void C_BaseViewModel::UpdateParticles( int nSlot )
{
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	if ( !pPlayer )
		return;

	if ( pPlayer->IsPlayerDead() )
		return;

	// Otherwise pass the event to our associated weapon
	C_BaseCombatWeapon *pWeapon = GetOwningWeapon();
	if ( !pWeapon )
		return;

	CWeaponCSBase *pCSWeapon = ( CWeaponCSBase* )pPlayer->GetActiveWeapon();
	if ( !pCSWeapon )
		return;

	int iWeaponId = pCSWeapon->GetCSWeaponID();

	bool shouldDrawPlayer = ( pPlayer->GetPlayerRenderMode( nSlot ) == PLAYER_RENDER_THIRDPERSON );
	bool visible = r_drawviewmodel.GetBool() && pPlayer && !shouldDrawPlayer;

	if ( visible && iWeaponId == WEAPON_MOLOTOV )
	{
		CBaseCSGrenade *pGren = dynamic_cast<CBaseCSGrenade*>( pPlayer->GetActiveWeapon() );

		if ( pGren->IsPinPulled() )
		{
			//if ( !pGren->IsLoopingSoundPlaying() )
			//{
			//	pGren->SetLoopingSoundPlaying( true );
			//	EmitSound( "Molotov.IdleLoop" );
			//	//DevMsg( 1, "++++++++++>Playing Molotov.IdleLoop 2\n" );
			//}

			// TEST: [mlowrance] This is to test for attachment.
			int iAttachment = -1;
			if ( pWeapon && pWeapon->GetBaseAnimating() )
				iAttachment = pWeapon->GetBaseAnimating()->LookupAttachment( "Wick" );

			if ( iAttachment >= 0 )
			{
				if ( !m_viewmodelParticleEffect.IsValid() )
				{
					DispatchParticleEffect( MOLOTOV_PARTICLE_EFFECT_NAME, PATTACH_POINT_FOLLOW, this, "Wick" );
				}
			}
		}
	}
	else
	{
		if ( m_viewmodelParticleEffect.IsValid() )
		{
			StopSound( "Molotov.IdleLoop" );
			//DevMsg( 1, "---------->Stopping Molotov.IdleLoop 3\n" );
			m_viewmodelParticleEffect->StopEmission( false, true );
			m_viewmodelParticleEffect->SetRemoveFlag();
			m_viewmodelParticleEffect = NULL;
		}
	}
}

bool C_BaseViewModel::Simulate( void )
{
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );
	UpdateParticles( nSlot );
	UpdateStatTrakGlow();
	BaseClass::Simulate();
	return true;
}

void C_BaseViewModel::FormatViewModelAttachment( int nAttachment, matrix3x4_t &attachmentToWorld )
{
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	Vector vecOrigin;
	MatrixPosition( attachmentToWorld, vecOrigin );
	::FormatViewModelAttachment( pPlayer, vecOrigin, false );
	PositionMatrix( vecOrigin, attachmentToWorld );
}

void C_BaseViewModel::UncorrectViewModelAttachment( Vector &vOrigin )
{
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	// Unformat the attachment.
	::FormatViewModelAttachment( pPlayer, vOrigin, true );
}


//-----------------------------------------------------------------------------
// Purpose
//-----------------------------------------------------------------------------
void C_BaseViewModel::FireEvent( const Vector& origin, const QAngle& angles, int eventNum, const char *options )
{
	// We override sound requests so that we can play them locally on the owning player
	if ( ( eventNum == AE_CL_PLAYSOUND ) || ( eventNum == CL_EVENT_SOUND ) )
	{
		// Only do this if we're owned by someone
		if ( GetOwner() != NULL && GetOwner()->IsAlive() )
		{
			// playing the same sound near-instantly is assumed to be an error and duplicates are skipped. This does NOT apply to weapon firing sounds.
			if ( !IsSoundSameAsPreviousSound( options, 0.1f ) )
			{
				CLocalPlayerFilter filter;
				EmitSound( filter, GetOwner()->GetSoundSourceIndex(), options, &GetAbsOrigin() );
				SetPreviousSoundStr( options );
			}
			ResetTimeSincePreviousSound();
			return;
		}
	}

	C_BasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( pOwner );

	// Otherwise pass the event to our associated weapon
	C_BaseCombatWeapon *pWeapon = pOwner->GetActiveWeapon();
	if ( pWeapon )
	{
		bool bResult = pWeapon->OnFireEvent( this, origin, angles, eventNum, options );
		if ( !bResult )
		{
			if ( eventNum == AE_CLIENT_EFFECT_ATTACH && ::input->CAM_IsThirdPerson() )
				return;

			BaseClass::FireEvent( origin, angles, eventNum, options );
		}
	}
}

bool C_BaseViewModel::Interpolate( float currentTime )
{
	CStudioHdr *pStudioHdr = GetModelPtr();
	// Make sure we reset our animation information if we've switch sequences
	UpdateAnimationParity();

	bool bret = BaseClass::Interpolate( currentTime );

	// Hack to extrapolate cycle counter for view model
	float elapsed_time = currentTime - m_flAnimTime;
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	// Predicted viewmodels have fixed up interval
	if ( GetPredictable() || IsClientCreated() )
	{
		Assert( pPlayer );
		float curtime = pPlayer ? pPlayer->GetFinalPredictedTime() : gpGlobals->curtime;
		elapsed_time = curtime - m_flAnimTime;
		// Adjust for interpolated partial frame
		if ( !engine->IsPaused() )
		{
			elapsed_time += ( gpGlobals->interpolation_amount * TICK_INTERVAL );
		}
	}

	// Prediction errors?	
	if ( elapsed_time < 0 )
	{
		elapsed_time = 0;
	}

	float dt = elapsed_time * (GetPlaybackRate() * GetSequenceCycleRate( pStudioHdr, GetSequence() )) + m_fCycleOffset;

	if ( dt < 0.0f )
	{
		dt = 0.0f;
	}

	if ( dt >= 1.0f )
	{
		if ( !IsSequenceLooping( GetSequence() ) )
		{
			dt = 0.999f;
		}
		else
		{
			dt = fmod( dt, 1.0f );
		}
	}

	SetCycle( dt );
	return bret;
}


bool C_BaseViewModel::ShouldFlipModel()
{
#ifdef CSTRIKE_DLL
	// If cl_righthand is set, then we want them all right-handed.
	CBaseCombatWeapon *pWeapon = m_hWeapon.Get();
	if ( pWeapon )
	{
		const FileWeaponInfo_t *pInfo = &pWeapon->GetWpnData();
		if ( pInfo->m_bAllowFlipping )
		{
			if ( !ss_righthand.IsValid() )
			{
				ss_righthand.Init( "cl_righthand", true );
			}

			return  pInfo->m_bBuiltRightHanded != ss_righthand.GetBool( GET_ACTIVE_SPLITSCREEN_SLOT() );
		}
	}
#endif
	
	return false;
}


void C_BaseViewModel::ApplyBoneMatrixTransform( matrix3x4_t& transform )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );

	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	bool bUsingMotionController = pPlayer && PlatformInputDevice::IsInputDeviceAPointer( g_pInputSystem->GetCurrentInputDevice() );

	if ( ShouldFlipModel() || bUsingMotionController )
	{
		matrix3x4_t viewMatrix, viewMatrixInverse;

		// We could get MATERIAL_VIEW here, but this is called sometimes before the renderer
		// has set that matrix. Luckily, this is called AFTER the CViewSetup has been initialized.
		const CViewSetup *pSetup = view->GetPlayerViewSetup();
		AngleMatrix( pSetup->angles, pSetup->origin, viewMatrixInverse );
		MatrixInvert( viewMatrixInverse, viewMatrix );

		// Transform into view space.
		matrix3x4_t temp;
		ConcatTransforms( viewMatrix, transform, temp );
		

		if ( ShouldFlipModel() )
		{
			// Flip it along X.

			// (This is the slower way to do it, and it equates to negating the top row).
			//matrix3x4_t mScale;
			//SetIdentityMatrix( mScale );
			//mScale[0][0] = 1;
			//mScale[1][1] = -1;
			//mScale[2][2] = 1;
			//ConcatTransforms( mScale, temp, temp );
			temp[1][0] = -temp[1][0];
			temp[1][1] = -temp[1][1];
			temp[1][2] = -temp[1][2];
			temp[1][3] = -temp[1][3];

		}

		if ( bUsingMotionController )
		{
			matrix3x4_t localAngleMatrix;
			QAngle localAngles = pPlayer->GetEyeAngleOffset();

			// We want to use negative angles if they're closer to zero. If any of the angles are greater than 180, subtract off 360 to give a negative version of it.
			if ( localAngles[YAW] > 180.0f )
			{
				localAngles[YAW] -= 360.0f;
			}
			if ( localAngles[PITCH] > 180.0f )
			{
				localAngles[PITCH] -= 360.0f;
			}

			// If we're looking up ( negative pitch ), we want to scale back the pitch so that we don't cover too much of the screen or reveal parts of the arms we don't want the player to see.
			if ( localAngles[PITCH] < 0.0f )
			{
				localAngles[PITCH] *= vm_pointer_pitch_up_scale.GetFloat();
			}

			// Since the world and view model FOVs don't match up, scale down the offset angles so they point in the same direction as the cursor in the world.
			float viewmodel_fov_ratio = GetClientMode()->GetViewModelFOV() / pPlayer->GetFOV();
			localAngles *= viewmodel_fov_ratio;

			AngleMatrix(localAngles, localAngleMatrix);
			// We want to tweak the transform so that we include the angles for the pointer.
			// Do this by pre multiplying the angle tweaks.
			ConcatTransforms( localAngleMatrix, temp, temp );
		}

		// Transform back out of view space.
		ConcatTransforms( viewMatrixInverse, temp, transform );
	}
}

//-----------------------------------------------------------------------------
// Purpose: check if weapon viewmodel should be drawn
//-----------------------------------------------------------------------------
bool C_BaseViewModel::ShouldDraw()
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );

	if ( g_bEngineIsHLTV )
	{
		return ( HLTVCamera()->GetMode() == OBS_MODE_IN_EYE &&
				 HLTVCamera()->GetPrimaryTarget() == GetOwner()	);
	}
#if defined( REPLAY_ENABLED )
	else if ( engine->IsReplay() )
	{
		return ( ReplayCamera()->GetMode() == OBS_MODE_IN_EYE &&
				 ReplayCamera()->GetPrimaryTarget() == GetOwner() );
	}
#endif
	else
	{
		Assert(	GetRenderMode() != kRenderNone );

		if ( vm_draw_always.GetBool() )
			return true;

		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		C_BasePlayer *pObsTarget = NULL;

		if ( pLocalPlayer )
		{
			pObsTarget = ToBasePlayer( pLocalPlayer->GetObserverTarget() );
		}

		if ( pLocalPlayer )
			if ( ( pLocalPlayer->GetObserverMode() != OBS_MODE_IN_EYE ) && ( GetOwner() != pLocalPlayer ) )
				return false;

		return BaseClass::ShouldDraw();
	}
}

bool C_BaseViewModel::ShouldSuppressForSplitScreenPlayer( int nSlot )
{
	if ( vm_draw_always.GetBool() )
	{
		if ( vm_draw_always.GetInt() == 1 )
		{
			return false;
		}
		return true;
	}

	if ( BaseClass::ShouldSuppressForSplitScreenPlayer( nSlot ) )
	{
		return true;
	}
	
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( nSlot );
	C_BasePlayer *pOwner = ToBasePlayer( GetOwner() );

	// We supress viewing of the view model if we are not looking through the eyes of the player who owns that view model.
	// We still need to call animation updates on this view model.
	if ( pOwner == pLocalPlayer )
	{
		return false;
	}

	C_BasePlayer *pObserverTarget = ToBasePlayer( pLocalPlayer->GetObserverTarget() );

	if ( pOwner == pObserverTarget )
	{
		// [msmith] We only ever want to draw viewmodels if the player who owns this split screen is in the OBS_MODE_IN_EYE observer mode for view models.
		return ( OBS_MODE_IN_EYE != pLocalPlayer->GetObserverMode() );
	}

	return true;

}

//-----------------------------------------------------------------------------
// Purpose: Render the weapon. Draw the Viewmodel if the weapon's being carried
//			by this player, otherwise draw the worldmodel.
//-----------------------------------------------------------------------------
int C_BaseViewModel::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( !m_bReadyToDraw )
		return 0;

	if ( !UpdateBlending( flags, instance ) )
		return 0;
	
	
	
	CMatRenderContextPtr pRenderContext( materials );
	


	if ( (flags & STUDIO_RENDER) && mat_preview.GetString()[0] )
	{
		int nMatRet = 0;
		if ( m_pMaterialPreviewShape == NULL )
		{
			MDLCACHE_CRITICAL_SECTION();
			const char *pszMatLibraryModel = "models/matlibrary/matlibrary_default.mdl";
			CBaseAnimating *pPreviewShape = new class CBaseAnimating;
			if ( pPreviewShape && pPreviewShape->InitializeAsClientEntity( pszMatLibraryModel, false ) )
			{
				m_pMaterialPreviewShape = pPreviewShape;
				pPreviewShape->SetParent( this );
				pPreviewShape->SetLocalOrigin( Vector(25,0,-4) );
				pPreviewShape->UpdatePartitionListEntry();
				pPreviewShape->CollisionProp()->MarkPartitionHandleDirty();
				pPreviewShape->UpdateVisibility();
				pPreviewShape->SetUseParentLightingOrigin( true );

				pPreviewShape->SetEFlags( EF_NODRAW );
				pPreviewShape->SetEFlags( EF_NOCSM );
			}
			else
			{
				DevWarning( "Couldn't load material library preview model: %s\n", pszMatLibraryModel );
			}
		}
		else
		{
			IMaterial *pMatPreview = materials->FindMaterial( mat_preview.GetString(), TEXTURE_GROUP_OTHER, true );

			//if ( !pMatPreview )
			//{
			//	for ( MaterialHandle_t i = materials->FirstMaterial(); i != materials->InvalidMaterial(); i = materials->NextMaterial(i) )
			//	{
			//		char const *szMatName = materials->GetMaterial(i)->GetName();
			//		if ( V_stristr( szMatName, mat_preview.GetString() ) )
			//		{
			//			pMatPreview = materials->GetMaterial(i);
			//			break;
			//		}
			//	}
			//}
			
			if ( pMatPreview )
			{
				g_pStudioRender->ForcedMaterialOverride( pMatPreview );

				m_pMaterialPreviewShape->SetAbsAngles( QAngle(0,gpGlobals->curtime*8.0f,0) );
				nMatRet = m_pMaterialPreviewShape->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );

				g_pStudioRender->ForcedMaterialOverride( NULL );
			}
		}

		return nMatRet;
	}



	if ( ShouldFlipModel() )
		pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	int ret = 0;
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	C_BaseCombatWeapon *pWeapon = GetOwningWeapon();

	if ( pWeapon )
	{
		// clear first because number of materials may differ (or be 0)
		ClearCustomMaterials();

		// set our custom materials to the ones on the world model (they are actually view model res because of the owner being local)
		for ( int i = 0; i < pWeapon->GetCustomMaterialCount(); i++ )
		{
			SetCustomMaterial( pWeapon->GetCustomMaterial( i ), i );
		}
	}

#ifdef IRONSIGHT
	// try to render the scope lens mask stencil shape
	if ( flags && (pWeapon && GetScopeStencilMaskMode() == true) )
	{
		// first create and bonemerge a new scope lens mask stencil shape if we don't have one
		if ( !m_viewmodelScopeStencilMask )
		{
			if ( pWeapon )
			{
				CEconItemView *pItem = pWeapon->GetEconItemView();
				if ( pItem && pItem->GetScopeLensMaskModel() )
				{
					C_ViewmodelAttachmentModel *pScopeStencilMask = new class C_ViewmodelAttachmentModel;
					if ( pScopeStencilMask && pScopeStencilMask->InitializeAsClientEntity( pItem->GetScopeLensMaskModel(), true ) )
					{
						m_viewmodelScopeStencilMask = pScopeStencilMask;
						pScopeStencilMask->SetParent( this );
						pScopeStencilMask->SetLocalOrigin( vec3_origin );
						pScopeStencilMask->UpdatePartitionListEntry();
						pScopeStencilMask->UpdateVisibility();
						pScopeStencilMask->SetViewmodel( this );
					}
				}
			}
		}

		// now render the scope lens mask stencil shape if we have one
		if ( m_viewmodelScopeStencilMask && m_viewmodelScopeStencilMask.Get() )
		{
			render->SetBlend( 0.0f );
			pRenderContext->OverrideColorWriteEnable( true, false );
			pRenderContext->OverrideDepthEnable( true, false, true );

			IMaterial *pMatScopeDummyMaterial = materials->FindMaterial( "dev/scope_mask", TEXTURE_GROUP_OTHER, true );
			g_pStudioRender->ForcedMaterialOverride( pMatScopeDummyMaterial );
			
			m_viewmodelScopeStencilMask->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );

			g_pStudioRender->ForcedMaterialOverride( NULL );
		}
	}
	else
#endif
	{
		//otherwise, render the viewmodel normally

		// If the local player's overriding the viewmodel rendering, let him do it
		if ( pPlayer && pPlayer->IsOverridingViewmodel() )
		{
			ret = pPlayer->DrawOverriddenViewmodel( this, flags, instance );
		}
		else if ( pWeapon && pWeapon->IsOverridingViewmodel() )
		{
			ret = pWeapon->DrawOverriddenViewmodel( this, flags, instance );
		}
		else
		{
			ret = BaseClass::DrawModel( flags, instance );
		}
	}

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	// Now that we've rendered, reset the animation restart flag
	if ( flags & STUDIO_RENDER )
	{
		if ( m_nOldAnimationParity != m_nAnimationParity )
		{
			m_nOldAnimationParity = m_nAnimationParity;
		}

		// Tell the weapon itself that we've rendered, in case it wants to do something
		if ( pWeapon )
		{
			pWeapon->ViewModelDrawn( flags, this );
		}

		if ( vm_debug.GetBool() )
		{
			MDLCACHE_CRITICAL_SECTION();

			int line = 16;
			CStudioHdr *hdr = GetModelPtr();
			engine->Con_NPrintf( line++, "%s: %s(%d), cycle: %.2f cyclerate: %.2f playbackrate: %.2f\n", 
				(hdr)?hdr->pszName():"(null)",
				GetSequenceName( GetSequence() ),
				GetSequence(),
				GetCycle(), 
				GetSequenceCycleRate( hdr, GetSequence() ),
				GetPlaybackRate()
				);
			if ( hdr )
			{
				for( int i=0; i < hdr->GetNumPoseParameters(); ++i )
				{
					const mstudioposeparamdesc_t &Pose = hdr->pPoseParameter( i );
					engine->Con_NPrintf( line++, "pose_param %s: %f",
						Pose.pszName(), GetPoseParameter( i ) );
				}
			}

			// Determine blending amount and tell engine
			float blend = (float)( instance.m_nAlpha / 255.0f );
			float color[3];
			GetColorModulation( color );
			engine->Con_NPrintf( line++, "blend=%f, color=%f,%f,%f", blend, color[0], color[1], color[2] );
			engine->Con_NPrintf( line++, "GetRenderMode()=%d", GetRenderMode() );
			engine->Con_NPrintf( line++, "m_nRenderFX=0x%8.8X", GetRenderFX() );

			color24 c = GetRenderColor();
			unsigned char a = GetRenderAlpha();
			engine->Con_NPrintf( line++, "rendercolor=%d,%d,%d,%d", c.r, c.g, c.b, a );

			engine->Con_NPrintf( line++, "origin=%f, %f, %f", GetRenderOrigin().x, GetRenderOrigin().y, GetRenderOrigin().z );
			engine->Con_NPrintf( line++, "angles=%f, %f, %f", GetRenderAngles()[0], GetRenderAngles()[1], GetRenderAngles()[2] );

			if ( IsEffectActive( EF_NODRAW ) )
			{
				engine->Con_NPrintf( line++, "EF_NODRAW" );
			}
		}
	}


	if ( flags && vm_draw_addon.GetBool() 
#ifdef IRONSIGHT
		&& (GetScopeStencilMaskMode() == false) 
#endif
		)
	{
		FOR_EACH_VEC( m_vecViewmodelArmModels, i )
		{
			if ( m_vecViewmodelArmModels[i] )
			{

				if ( m_vecViewmodelArmModels[i]->GetMoveParent() != this )
				{
					m_vecViewmodelArmModels[i]->SetEFlags( EF_BONEMERGE );
					m_vecViewmodelArmModels[i]->SetParent( this );
				}

				m_vecViewmodelArmModels[i]->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );
			}
		}
		for ( int i=0; i < m_hStickerModelAddons.Count(); ++i )
		{
			if ( m_hStickerModelAddons[i] )
			{
				m_hStickerModelAddons[i]->DrawModel( flags, instance );
			}
		}
		if ( m_viewmodelStatTrakAddon )
		{
			m_viewmodelStatTrakAddon->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );
		}
		if ( m_viewmodelUidAddon )
		{
			m_viewmodelUidAddon->DrawModel( flags | STUDIO_DONOTMODIFYSTENCILSTATE, instance );
		}
	}
	
#ifdef IRONSIGHT
	//Scope stencil mask mode is automatically turned off after rendering. It needs to be explicitly enabled before each draw.
	if ( flags )
		SetScopeStencilMaskMode( false );
#endif

	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Called by the player when the player's overriding the viewmodel drawing. Avoids infinite recursion.
//-----------------------------------------------------------------------------
int C_BaseViewModel::DrawOverriddenViewmodel( C_BaseViewModel *pViewmodel, int flags, const RenderableInstance_t &instance )
{
	return BaseClass::DrawModel( flags, instance );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
uint8 C_BaseViewModel::OverrideAlphaModulation( uint8 nAlpha )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );

	// See if the local player wants to override the viewmodel's rendering
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer && pPlayer->IsOverridingViewmodel() )
		return pPlayer->AlphaProp()->ComputeRenderAlpha();
	
	C_BaseCombatWeapon *pWeapon = GetOwningWeapon();
	if ( pWeapon && pWeapon->IsOverridingViewmodel() )
		return pWeapon->AlphaProp()->ComputeRenderAlpha();

	return nAlpha;

}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
RenderableTranslucencyType_t C_BaseViewModel::ComputeTranslucencyType( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );

	// See if the local player wants to override the viewmodel's rendering
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer && pPlayer->IsOverridingViewmodel() )
		return pPlayer->ComputeTranslucencyType();

	C_BaseCombatWeapon *pWeapon = GetOwningWeapon();
	if ( pWeapon && pWeapon->IsOverridingViewmodel() )
		return pWeapon->ComputeTranslucencyType();

	return BaseClass::ComputeTranslucencyType();

}


//-----------------------------------------------------------------------------
// Purpose: If the animation parity of the weapon has changed, we reset cycle to avoid popping
//-----------------------------------------------------------------------------
void C_BaseViewModel::UpdateAnimationParity( void )
{
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	
	// If we're predicting, then we don't use animation parity because we change the animations on the clientside
	// while predicting. When not predicting, only the server changes the animations, so a parity mismatch
	// tells us if we need to reset the animation.
	if ( ( m_nOldAnimationParity != m_nAnimationParity && !GetPredictable() ) )
	{
		float curtime = (pPlayer && IsIntermediateDataAllocated()) ? pPlayer->GetFinalPredictedTime() : gpGlobals->curtime;
		// FIXME: this is bad
		// Simulate a networked m_flAnimTime and m_flCycle
		// FIXME:  Do we need the magic 0.1?
		SetCycle( 0.0f ); // GetSequenceCycleRate( GetSequence() ) * 0.1;
		m_flAnimTime = curtime;
		m_fCycleOffset = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update global map state based on data received
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseViewModel::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		AlphaProp()->EnableAlphaModulationOverride( true );
	}

	SetPredictionEligible( true );
	BaseClass::OnDataChanged(updateType);
}
void C_BaseViewModel::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate(updateType);
	OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
}

//-----------------------------------------------------------------------------
// Purpose: Return the player who will predict this entity
//-----------------------------------------------------------------------------
CBasePlayer *C_BaseViewModel::GetPredictionOwner()
{
	return ToBasePlayer( GetOwner() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseViewModel::GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS])
{
	BaseClass::GetBoneControllers( controllers );

	// Tell the weapon itself that we've rendered, in case it wants to do something
	C_BasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( pWeapon )
	{
		pWeapon->GetViewmodelBoneControllers( this, controllers );
	}
}

void C_BaseViewModel::UpdateAllViewmodelAddons( void )
{
	C_CSPlayer *pPlayer = ToCSPlayer( GetOwner() );

	// Remove any view model add ons if we're spectating.
	if ( !pPlayer )
	{
		RemoveViewmodelArmModels();
		RemoveViewmodelLabel();
		RemoveViewmodelStatTrak();
		RemoveViewmodelStickers();
		return;
	}

	C_BaseCombatWeapon *pWeapon = pPlayer->GetActiveWeapon();
	if ( !pWeapon )
	{
		RemoveViewmodelArmModels();
		RemoveViewmodelLabel();
		RemoveViewmodelStatTrak();
		RemoveViewmodelStickers();
		return;
	}
	CWeaponCSBase* pCSWeapon = dynamic_cast<CWeaponCSBase*>( pWeapon );
	if ( !pCSWeapon )
	{
		RemoveViewmodelArmModels();
		RemoveViewmodelLabel();
		RemoveViewmodelStatTrak();
		RemoveViewmodelStickers();
		return;
	}

	int weaponID = pCSWeapon->GetCSWeaponID();

	// Note: only arms race (gun game) knives change their bodygroup to indicate their team. This will soon go away.
	if ( weaponID == WEAPON_KNIFE_GG )
	{
		int bodyPartID = ( pPlayer->GetTeamNumber() == TEAM_TERRORIST ) ? 0 : 1;
		SetBodygroup( 0 , bodyPartID );
	}


	if ( pPlayer->m_pViewmodelArmConfig == NULL )
	{
		RemoveViewmodelArmModels();

		CStudioHdr *pHdr = pPlayer->GetModelPtr();
		if ( pHdr )
		{
			pPlayer->m_pViewmodelArmConfig = GetPlayerViewmodelArmConfigForPlayerModel( pHdr->pszName() );
		}
	}
	
	// add gloves and sleeves
	if ( pPlayer->m_pViewmodelArmConfig != NULL && m_vecViewmodelArmModels.Count() == 0 )
	{
		{
			AddViewmodelArmModel( pPlayer->m_pViewmodelArmConfig->szAssociatedGloveModel, atoi(pPlayer->m_pViewmodelArmConfig->szSkintoneIndex) );
			AddViewmodelArmModel( pPlayer->m_pViewmodelArmConfig->szAssociatedSleeveModel );
		}
	}


	// econ-related addons follow, so bail out if we can't get at the econitemview
	CEconItemView *pItem = pWeapon->GetEconItemView();
	if ( !pItem )
	{
		RemoveViewmodelLabel();
		RemoveViewmodelStatTrak();
		RemoveViewmodelStickers();
		return;
	}

	// verify weapon label and add if necessary
	AddViewmodelLabel( pItem );

	// verify stattrak module and add if necessary
	CUtlSortVector<uint32> vTypes;

	pItem->GetKillEaterTypes( vTypes );
	if ( (vTypes.Count() > 0) && ( pItem->GetKillEaterValueByType( vTypes[ vTypes.Count() - 1] ) >= 0 ) )
	{
		CSteamID HolderSteamID;
		pPlayer->GetSteamID( &HolderSteamID );
		AddViewmodelStatTrak( pItem, vTypes[ vTypes.Count() - 1], weaponID, HolderSteamID.GetAccountID() );
	}
	else
	{
		RemoveViewmodelStatTrak();
	}
	
	// add viewmodel stickers
	AddViewmodelStickers( pItem, weaponID );

#ifdef IRONSIGHT
	if ( m_viewmodelScopeStencilMask )
	{
		C_ViewmodelAttachmentModel *pScopeMask = m_viewmodelScopeStencilMask.Get();
		if ( pScopeMask )
			pScopeMask->Remove();
	}
#endif

}

C_ViewmodelAttachmentModel* C_BaseViewModel::FindArmModelForLoadoutPosition( loadout_positions_t nPosition ) const
{
	/* Removed for partner depot */
	return NULL;
}

//--------------------------------------------------------------------------------------------------------
C_ViewmodelAttachmentModel* C_BaseViewModel::AddViewmodelArmModel( const char *pszArmsModel, int nSkintoneIndex )
{
	if ( pszArmsModel == NULL || pszArmsModel[ 0 ] == '\0' || modelinfo->GetModelIndex( pszArmsModel ) == -1 )
	{
		//pszArmsModel = //g_pGameTypes->GetCTViewModelArmsForMap( engine->GetLevelNameShort() );
		C_CSPlayer *pPlayer = ToCSPlayer( GetOwner() );
		pszArmsModel = pPlayer->m_szArmsModel.Get();
	}

	// Only create the view model attachment if we have a valid arm model
	if ( pszArmsModel == NULL || pszArmsModel[0] == '\0' || modelinfo->GetModelIndex( pszArmsModel ) == -1 )
		return NULL;

	C_ViewmodelAttachmentModel *pEnt = new class C_ViewmodelAttachmentModel;
	if ( pEnt && pEnt->InitializeAsClientEntity( pszArmsModel, true ) )
	{
		m_vecViewmodelArmModels[ m_vecViewmodelArmModels.AddToTail() ] = pEnt;

		if ( nSkintoneIndex != -1 )
			pEnt->SetSkin( nSkintoneIndex );

		pEnt->SetParent( this );
		pEnt->SetLocalOrigin( vec3_origin );
		pEnt->UpdatePartitionListEntry();
		pEnt->CollisionProp()->MarkPartitionHandleDirty();
		pEnt->UpdateVisibility();
		pEnt->SetViewmodel( this );
		pEnt->SetUseParentLightingOrigin( true );

		RemoveEffects( EF_NODRAW );
		return pEnt;
	}	

	return NULL;
}

void C_BaseViewModel::AddViewmodelLabel( CEconItemView *pItem )
{
	if ( !pItem || !pItem->GetCustomName() )
	{
		RemoveViewmodelLabel();
		return;
	} else if ( m_viewmodelUidAddon && m_viewmodelUidAddon.Get() && m_viewmodelUidAddon->GetMoveParent() )
	{
		return;
	}
	
	RemoveViewmodelLabel();	

	C_ViewmodelAttachmentModel *pUidEnt = new class C_ViewmodelAttachmentModel;
	if ( pUidEnt && pUidEnt->InitializeAsClientEntity( pItem->GetUidModel(), true ) )
	{
		m_viewmodelUidAddon = pUidEnt;
		pUidEnt->SetParent( this );
		pUidEnt->SetLocalOrigin( vec3_origin );
		pUidEnt->UpdatePartitionListEntry();
		pUidEnt->CollisionProp()->MarkPartitionHandleDirty();
		pUidEnt->UpdateVisibility();
		pUidEnt->SetViewmodel( this );
		pUidEnt->SetUseParentLightingOrigin( true );

		if ( !cl_righthand.GetBool() )
		{
			pUidEnt->SetBodygroup( 0, 1 ); // use a special mirror-image that appears correct for lefties
		}
	
		RemoveEffects( EF_NODRAW );
	}
}

void C_BaseViewModel::AddViewmodelStatTrak( CEconItemView *pItem, int nStatTrakType, int nWeaponID, AccountID_t holderAcctId )
{
	if ( m_viewmodelStatTrakAddon && m_viewmodelStatTrakAddon.Get() && m_viewmodelStatTrakAddon->GetMoveParent() )
		return;

	RemoveViewmodelStatTrak();

	if (!pItem)
		return;

	C_ViewmodelAttachmentModel *pStatTrakEnt = new class C_ViewmodelAttachmentModel;
	if ( pStatTrakEnt && pStatTrakEnt->InitializeAsClientEntity( pItem->GetStatTrakModelByType( nStatTrakType ), true ) )
	{
		m_viewmodelStatTrakAddon = pStatTrakEnt;
		pStatTrakEnt->SetParent( this );
		pStatTrakEnt->SetLocalOrigin( vec3_origin );
		pStatTrakEnt->UpdatePartitionListEntry();
		pStatTrakEnt->CollisionProp()->MarkPartitionHandleDirty();
		pStatTrakEnt->UpdateVisibility();
		pStatTrakEnt->SetViewmodel( this );
		pStatTrakEnt->SetUseParentLightingOrigin( true );

		if ( !cl_righthand.GetBool() )
		{
			pStatTrakEnt->SetBodygroup( 0, 1 ); // use a special mirror-image stattrak module that appears correct for lefties
		}

		// this stat trak weapon doesn't belong to the current holder, display error message on the digital display. This is impossible for knives
		if ( nWeaponID != WEAPON_KNIFE && nWeaponID != WEAPON_KNIFE_GG )
		{
			if ( pItem->GetAccountID() != holderAcctId )
			{
				pStatTrakEnt->SetBodygroup( 1, cl_righthand.GetBool() ? 1 : 2 ); // show the error screen bodygroup
			}
		}

		RemoveEffects( EF_NODRAW );
	}
}

bool C_BaseViewModel::ViewmodelStickersAreValid( int nWeaponID )
{
	if ( m_hStickerModelAddons.Count() == 0 )
	{
		return false;
	}
	// returns true if all viewmodel sticker handles are non-null and appropriate for the given weapon id
	for ( int i=0; i < m_hStickerModelAddons.Count(); ++i )
	{
		if ( !m_hStickerModelAddons[i] || !m_hStickerModelAddons[i].Get() || !m_hStickerModelAddons[i]->GetMoveParent() )
		{
			return false;
		}
	}
	return true;
}

void C_BaseViewModel::AddViewmodelStickers( CEconItemView *pItem, int nWeaponID )
{
	/* Removed for partner depot */
}

void C_BaseViewModel::RemoveViewmodelArmModels( void )
{
	FOR_EACH_VEC_BACK( m_vecViewmodelArmModels, i )
	{
		C_ViewmodelAttachmentModel *pEnt = m_vecViewmodelArmModels[i].Get();
		if ( pEnt )
		{
			pEnt->Remove();
		}
	}
	m_vecViewmodelArmModels.RemoveAll();
}

void C_BaseViewModel::RemoveViewmodelLabel( void )
{
	C_ViewmodelAttachmentModel *pUidEnt = m_viewmodelUidAddon.Get();
	if ( pUidEnt )
	{
		pUidEnt->Remove();
	}
}

void C_BaseViewModel::RemoveViewmodelStatTrak( void )
{
	C_ViewmodelAttachmentModel *pStatTrakEnt = m_viewmodelStatTrakAddon.Get();
	if ( pStatTrakEnt )
	{
		pStatTrakEnt->Remove();
	}
}

void C_BaseViewModel::RemoveViewmodelStickers( void )
{
	for ( int i=0; i < m_hStickerModelAddons.Count(); ++i )
	{
		C_ViewmodelAttachmentModel *pStickerAddon = m_hStickerModelAddons[i];
		if ( pStickerAddon )
		{
			pStickerAddon->Remove();
		}
	}
	m_hStickerModelAddons.RemoveAll();
}

#if defined (_GAMECONSOLE)

//C_ViewmodelAttachmentModel
//C_ViewmodelAttachmentModel
//C_ViewmodelAttachmentModel
//C_ViewmodelAttachmentModel
//--------------------------------------------------------------------------------------------------------  
bool C_ViewmodelAttachmentModel::InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels )
{
	if ( !BaseClass::InitializeAsClientEntity( pszModelName, bRenderWithViewModels ) )
		return false;

	AddEffects( EF_BONEMERGE );
	AddEffects( EF_BONEMERGE_FASTCULL );
	AddEffects( EF_NODRAW );
	return true;
}

void C_ViewmodelAttachmentModel::SetViewmodel( C_BaseViewModel *pVM )
{
	m_hViewmodel = pVM;
}

int C_ViewmodelAttachmentModel::InternalDrawModel( int flags, const RenderableInstance_t &instance )
{
	CMatRenderContextPtr pRenderContext( materials );

	C_BaseViewModel *pViewmodel = m_hViewmodel;

	if ( pViewmodel && pViewmodel->ShouldFlipModel() )
		pRenderContext->CullMode( MATERIAL_CULLMODE_CW );

	bool bValidMaterialOverride = (m_MaterialOverrides != NULL) && (m_MaterialOverrides->IsErrorMaterial() == false);

	if (bValidMaterialOverride)
		modelrender->ForcedMaterialOverride( m_MaterialOverrides );

	int ret = BaseClass::InternalDrawModel( flags, instance );

	if (bValidMaterialOverride)
		modelrender->ForcedMaterialOverride( NULL );

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );

	return ret;
}

#endif
