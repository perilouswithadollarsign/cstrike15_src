//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Class to control 'aim-down-sights' aka "IronSight" weapon functionality
//
//=====================================================================================//


#include "cbase.h"
#include "cs_shareddefs.h"

#ifdef IRONSIGHT
#include "weapon_csbase.h"
#include "weapon_ironsightcontroller.h"

#if defined( CLIENT_DLL )
	#include "c_cs_player.h"
	#include "view_scene.h"
	#include "shaderapi/ishaderapi.h"
	#include "materialsystem/imaterialvar.h"
	#include "prediction.h"
#else
	#include "cs_player.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define IRONSIGHT_VIEWMODEL_BOB_MULT_X 0.05
#define IRONSIGHT_VIEWMODEL_BOB_PERIOD_X 6
#define IRONSIGHT_VIEWMODEL_BOB_MULT_Y 0.1
#define IRONSIGHT_VIEWMODEL_BOB_PERIOD_Y 10

#ifdef DEBUG
	ConVar ironsight_override(				"ironsight_override",			"0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_position(				"ironsight_position",			"0 0 0",	FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_angle(					"ironsight_angle",				"0 0 0",	FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_fov(					"ironsight_fov",				"60",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_pivot_forward(			"ironsight_pivot_forward",		"0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_looseness(				"ironsight_looseness",			"0.1",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_speed_bringup(			"ironsight_speed_bringup",		"4.0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_speed_putdown(			"ironsight_speed_putdown",		"2.0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	
	ConVar ironsight_catchupspeed(			"ironsight_catchupspeed",		"60.0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
	ConVar ironsight_running_looseness(		"ironsight_running_looseness",	"0.3",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );

	ConVar ironsight_spew_amount(			"ironsight_spew_amount",		"0",		FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
#else
	#define ironsight_catchupspeed			60.0f
#endif

CIronSightController::CIronSightController( void )
{
	m_bIronSightAvailable			= false;
	m_angPivotAngle.Init();
	m_vecEyePos.Init();
	m_flIronSightAmount				= 0.0f;
	m_flIronSightPullUpSpeed		= 8.0f;
	m_flIronSightPutDownSpeed		= 4.0f;
	m_flIronSightFOV				= 80.0f;
	m_flIronSightPivotForward		= 10.0f;
	m_flIronSightLooseness			= 0.5f;
	m_pAttachedWeapon				= NULL;	

#ifdef CLIENT_DLL
	m_angViewLast.Init();
	for ( int i=0; i<IRONSIGHT_ANGLE_AVERAGE_SIZE; i++ )
	{
		m_angDeltaAverage[i].Init();
	}
	m_vecDotCoords.Init();
	m_flDotBlur						= 0.0f;
	m_flSpeedRatio					= 0.0f;
#endif
}

void CIronSightController::SetState( CSIronSightMode newState )
{
	if ( newState == IronSight_viewmodel_is_deploying || newState == IronSight_weapon_is_dropped )
	{
		m_flIronSightAmount = 0.0f;
	}
	if ( m_pAttachedWeapon )
	{
		m_pAttachedWeapon->m_iIronSightMode = newState;
	}
}

bool CIronSightController::IsApproachingSighted(void)
{
	return (m_pAttachedWeapon && m_pAttachedWeapon->m_iIronSightMode == IronSight_should_approach_sighted);
}

bool CIronSightController::IsApproachingUnSighted(void)
{
	return (m_pAttachedWeapon && m_pAttachedWeapon->m_iIronSightMode == IronSight_should_approach_unsighted);
}

bool CIronSightController::IsDeploying(void)
{
	return (m_pAttachedWeapon && m_pAttachedWeapon->m_iIronSightMode == IronSight_viewmodel_is_deploying);
}

bool CIronSightController::IsDropped(void)
{
	return (m_pAttachedWeapon && m_pAttachedWeapon->m_iIronSightMode == IronSight_weapon_is_dropped);
}

void CIronSightController::UpdateIronSightAmount( void )
{

	if (!m_pAttachedWeapon || IsDeploying() || IsDropped())
	{
		//ignore and discard any lingering ironsight amount.
		m_flIronSightAmount = 0.0f;
		m_flIronSightAmountGained = 0.0f;
		return;
	}

	//first determine if we are going into or out of ironsights, and set m_flIronSightAmount accordingly
	float flIronSightAmountTarget = IsApproachingSighted() ? 1.0f : 0.0f;
	float flIronSightUpdOrDownSpeed = IsApproachingSighted() ? m_flIronSightPullUpSpeed : m_flIronSightPutDownSpeed;

#ifdef DEBUG
	if (ironsight_override.GetBool())
		flIronSightUpdOrDownSpeed = IsApproachingSighted() ? ironsight_speed_bringup.GetFloat() : ironsight_speed_putdown.GetFloat();
#endif

	m_flIronSightAmount = Approach(flIronSightAmountTarget, m_flIronSightAmount, gpGlobals->frametime * flIronSightUpdOrDownSpeed);

	m_flIronSightAmountGained = Gain( m_flIronSightAmount, 0.8f );
	m_flIronSightAmountBiased = Bias( m_flIronSightAmount, 0.2f );

#ifdef DEBUG
	if ( ironsight_spew_amount.GetBool() )
		DevMsg( "Ironsight amount: %f, Gained: %f, Biased: %f\n", m_flIronSightAmount, m_flIronSightAmountGained, m_flIronSightAmountBiased );
#endif

}

#ifdef CLIENT_DLL

void CIronSightController::IncreaseDotBlur(float flAmount)
{
	if ( IsInIronSight() && prediction->IsFirstTimePredicted() )
		m_flDotBlur = clamp(m_flDotBlur + flAmount, 0, 1);
}

float CIronSightController::GetDotBlur(void)
{
	return Bias(1.0f - Max(m_flDotBlur, m_flSpeedRatio * 0.5f), 0.2f);
}

float CIronSightController::GetDotWidth(void)
{
	return (32 + (256 * Max(m_flDotBlur, m_flSpeedRatio * 0.3f)));

}
Vector2D CIronSightController::GetDotCoords(void)
{
	return m_vecDotCoords;
}

bool CIronSightController::ShouldHideCrossHair( void )
{
	return ( (IsApproachingSighted() || IsApproachingUnSighted()) && GetIronSightAmount() > IRONSIGHT_HIDE_CROSSHAIR_THRESHOLD );
}

const char *CIronSightController::GetDotMaterial( void )
{
	//TODO: convert to schema attribute
	if ( m_pAttachedWeapon && m_pAttachedWeapon->GetCSWeaponID() == WEAPON_SG556 )
		return "models/weapons/shared/scope/scope_dot_red";

	return "models/weapons/shared/scope/scope_dot_green";
}

QAngle CIronSightController::QAngleDiff( QAngle &angTarget, QAngle &angSrc )
{
	return QAngle(	AngleDiff( angTarget.x, angSrc.x ),
					AngleDiff( angTarget.y, angSrc.y ),
					AngleDiff( angTarget.z, angSrc.z ) );
}

QAngle CIronSightController::GetAngleAverage( void )
{
	QAngle temp;
	temp.Init();

	if (GetIronSightAmount() < 1.0f)
		return temp;

	for ( int i=0; i<IRONSIGHT_ANGLE_AVERAGE_SIZE; i++ )
	{
		temp += m_angDeltaAverage[i];
	}

	return ( temp * IRONSIGHT_ANGLE_AVERAGE_DIVIDE );
}

void CIronSightController::AddToAngleAverage( QAngle newAngle )
{

	if (GetIronSightAmount() < 1.0f)
		return;

	newAngle.x = clamp( newAngle.x, -2, 2 );
	newAngle.y = clamp( newAngle.y, -2, 2 );
	newAngle.z = clamp( newAngle.z, -2, 2 );

	for ( int i=IRONSIGHT_ANGLE_AVERAGE_SIZE-1; i>0; i-- )
	{
		m_angDeltaAverage[i] = m_angDeltaAverage[i-1];
	}

	m_angDeltaAverage[0] = newAngle;
}

extern ConVar cl_righthand;
void CIronSightController::ApplyIronSightPositioning( Vector &vecPosition, QAngle &angAngle, const Vector &vecBobbedEyePosition, const QAngle &angBobbedEyeAngle )
{
	UpdateIronSightAmount();

	if ( m_flIronSightAmount == 0 )
		return;

	//check if the player is moving and save off a usable movement speed ratio for later
	if ( m_pAttachedWeapon->GetOwner() )
	{
		CBaseCombatCharacter *pPlayer = m_pAttachedWeapon->GetOwner();
		m_flSpeedRatio = Approach( pPlayer->GetAbsVelocity().Length() / m_pAttachedWeapon->GetMaxSpeed(), m_flSpeedRatio, gpGlobals->frametime * 10.0f );
	}

	//if we're more than 10% ironsighted, apply looseness.
	if ( m_flIronSightAmount > 0.1f )
	{
		//get the difference between current angles and last angles
		QAngle angDelta = QAngleDiff( m_angViewLast, angAngle );
	
		//dampen the delta to simulate 'looseness', but the faster we move, the more looseness approaches ironsight_running_looseness.GetFloat(), which is as waggly as possible
#ifdef DEBUG
		if ( ironsight_override.GetBool() )
		{
			AddToAngleAverage( angDelta * Lerp(m_flSpeedRatio, ironsight_looseness.GetFloat(), ironsight_running_looseness.GetFloat()) );
		}
		else
#endif
		{
			AddToAngleAverage( angDelta * Lerp(m_flSpeedRatio, m_flIronSightLooseness, 0.3f ) );
		}

		//m_angViewLast tries to catch up to angAngle
#ifdef DEBUG
		m_angViewLast -= angDelta * clamp( gpGlobals->frametime * ironsight_catchupspeed.GetFloat(), 0, 1 );
#else
		m_angViewLast -= angDelta * clamp( gpGlobals->frametime * ironsight_catchupspeed, 0, 1 );
#endif

	}
	else
	{
		m_angViewLast = angAngle;
	}
		
	
	//now the fun part - move the viewmodel to look down the sights


	//create a working matrix at the current eye position and angles
	VMatrix matIronSightMatrix = SetupMatrixOrgAngles( vecPosition, angAngle );


	//offset the matrix by the ironsight eye position
#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		Vector vecTemp; //when overridden use convar values instead of schema driven ones so we can iterate on the values quickly while authoring
		if ( sscanf( ironsight_position.GetString(), "%f %f %f", &vecTemp.x, &vecTemp.y, &vecTemp.z ) == 3 )
			MatrixTranslate( matIronSightMatrix, (-vecTemp) * GetIronSightAmountGained() );
	}
	else
#endif
	{
		//use schema defined offset
		MatrixTranslate( matIronSightMatrix, (-m_vecEyePos) * GetIronSightAmountGained() );
	}
	
	//additionally offset by the ironsight origin of rotation, the weapon will pivot around this offset from the eye
#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		MatrixTranslate( matIronSightMatrix, Vector( ironsight_pivot_forward.GetFloat(), 0, 0 ) );
	}
	else
#endif
	{
		MatrixTranslate( matIronSightMatrix, Vector( m_flIronSightPivotForward, 0, 0 ) );
	}

	QAngle angDeltaAverage = GetAngleAverage();

	if ( !cl_righthand.GetBool() )
		angDeltaAverage = -angDeltaAverage;

	//apply ironsight eye rotation
#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		QAngle angTemp;  //when overridden use convar values instead of schema driven ones so we can iterate on the values quickly while authoring
		if ( sscanf( ironsight_angle.GetString(), "%f %f %f", &angTemp.x, &angTemp.y, &angTemp.z ) == 3 )
		{
			MatrixRotate( matIronSightMatrix, Vector( 1, 0, 0 ), (angDeltaAverage.z + angTemp.z) * GetIronSightAmountGained() );
			MatrixRotate( matIronSightMatrix, Vector( 0, 1, 0 ), (angDeltaAverage.x + angTemp.x) * GetIronSightAmountGained() );
			MatrixRotate( matIronSightMatrix, Vector( 0, 0, 1 ), (angDeltaAverage.y + angTemp.y) * GetIronSightAmountGained() );
		}
	}
	else
#endif
	{
		
		//use schema defined angles
		MatrixRotate( matIronSightMatrix, Vector( 1, 0, 0 ), (angDeltaAverage.z + m_angPivotAngle.z) * GetIronSightAmountGained() );
		MatrixRotate( matIronSightMatrix, Vector( 0, 1, 0 ), (angDeltaAverage.x + m_angPivotAngle.x) * GetIronSightAmountGained() );
		MatrixRotate( matIronSightMatrix, Vector( 0, 0, 1 ), (angDeltaAverage.y + m_angPivotAngle.y) * GetIronSightAmountGained() );

	}
	
	if ( !cl_righthand.GetBool() )
		angDeltaAverage = -angDeltaAverage;

	//move the weapon back to the ironsight eye position
#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		MatrixTranslate( matIronSightMatrix, Vector( -ironsight_pivot_forward.GetFloat(), 0, 0 ) );
	}
	else
#endif
	{
		MatrixTranslate( matIronSightMatrix, Vector( -m_flIronSightPivotForward, 0, 0 ) );
	}	


	//if the player is moving, pull down and re-bob the weapon
	if ( m_pAttachedWeapon->GetOwner() )
	{
		//magic bob value, replace me
		Vector vecIronSightBob = Vector(
			1,
			IRONSIGHT_VIEWMODEL_BOB_MULT_X * sin( gpGlobals->curtime * IRONSIGHT_VIEWMODEL_BOB_PERIOD_X ),			
			IRONSIGHT_VIEWMODEL_BOB_MULT_Y * sin( gpGlobals->curtime * IRONSIGHT_VIEWMODEL_BOB_PERIOD_Y ) - IRONSIGHT_VIEWMODEL_BOB_MULT_Y
			);
	
		m_vecDotCoords.x = -vecIronSightBob.y;
		m_vecDotCoords.y = -vecIronSightBob.z;
		m_vecDotCoords *= 0.1f;
		m_vecDotCoords.x -= angDeltaAverage.y * 0.03f;
		m_vecDotCoords.y += angDeltaAverage.x * 0.03f;
		m_vecDotCoords *= m_flSpeedRatio;

		if ( !cl_righthand.GetBool() )
			vecIronSightBob.y = -vecIronSightBob.y;

		MatrixTranslate( matIronSightMatrix, vecIronSightBob * m_flSpeedRatio );
	}


	//extract the final position and angles and apply them as differences from the passed in values
	vecPosition -= ( vecPosition - matIronSightMatrix.GetTranslation() );

	QAngle angIronSightAngles;
	MatrixAngles( matIronSightMatrix.As3x4(), angIronSightAngles );
	angAngle -= QAngleDiff( angAngle, angIronSightAngles );

	//dampen dot blur
	m_flDotBlur = Approach(0.0f, m_flDotBlur, gpGlobals->frametime * 2.0f);

}

static void SetRenderTargetAndViewPort(ITexture *rt)
{
	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->SetRenderTarget(rt);
	if (rt)
	{
		pRenderContext->Viewport(0, 0, rt->GetActualWidth(), rt->GetActualHeight());
	}
}

#ifdef DEBUG
ConVar r_ironsight_scope_effect("r_ironsight_scope_effect", "1", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
ConVar ironsight_laser_dot_render_tweak1("ironsight_laser_dot_render_tweak1", "61", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
ConVar ironsight_laser_dot_render_tweak2("ironsight_laser_dot_render_tweak2", "64", FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT);
#endif

bool CIronSightController::PrepareScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup )
{
#ifdef DEBUG
	if (!r_ironsight_scope_effect.GetBool())
		return false;
#endif

	if (!IsInIronSight())
		return false;

	Rect_t actualRect;
	UpdateScreenEffectTexture(0, x, y, w, h, false, &actualRect);
	ITexture *pRtFullFrame = GetFullFrameFrameBufferTexture(0);

	CMatRenderContextPtr pRenderContext(materials);
	pRenderContext->PushRenderTargetAndViewport();

	// DOWNSAMPLE _rt_FullFrameFB TO _rt_SmallFB0
	IMaterial *pMatDownsample = materials->FindMaterial("dev/scope_downsample", TEXTURE_GROUP_OTHER, true);
	ITexture *pRtQuarterSize0 = materials->FindTexture("_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET);
	SetRenderTargetAndViewPort(pRtQuarterSize0);

	int nSrcWidth = pViewSetup->width;
	int nSrcHeight = pViewSetup->height;

	pRenderContext->DrawScreenSpaceRectangle(pMatDownsample, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
		0, 0, nSrcWidth - 4, nSrcHeight - 4,
		pRtFullFrame->GetActualWidth(), pRtFullFrame->GetActualHeight());

	//horizontally blur pRtQuarterSize0 over to pRtQuarterSize1
	IMaterial *pMatBlurX = materials->FindMaterial("dev/scope_blur_x", TEXTURE_GROUP_OTHER, true);
	ITexture *pRtQuarterSize1 = materials->FindTexture("_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET);
	SetRenderTargetAndViewPort(pRtQuarterSize1);

	pRenderContext->DrawScreenSpaceRectangle(pMatBlurX, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
		0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
		pRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight());

	//vertically blur pRtQuarterSize1 over to pRtQuarterSize0
	IMaterial *pMatBlurY = materials->FindMaterial("dev/scope_blur_y", TEXTURE_GROUP_OTHER, true);
	SetRenderTargetAndViewPort(pRtQuarterSize0);

	pRenderContext->DrawScreenSpaceRectangle(pMatBlurY, 0, 0, nSrcWidth / 4, nSrcHeight / 4,
		0, 0, nSrcWidth / 4 - 1, nSrcHeight / 4 - 1,
		pRtQuarterSize0->GetActualWidth(), pRtQuarterSize0->GetActualHeight());

	pRenderContext->PopRenderTargetAndViewport();

	//Prepare to render the scope lens mask shape into the stencil buffer.
	//The weapon itself will take care of using the correct blend mode and override material.

	ShaderStencilState_t stencilState;
	stencilState.m_bEnable = true;
	stencilState.m_nReferenceValue = 1;
	stencilState.m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;

	stencilState.m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
	stencilState.m_FailOp = SHADER_STENCILOP_KEEP;
	stencilState.m_ZFailOp = SHADER_STENCILOP_KEEP;

	pRenderContext->SetStencilState(stencilState);

	return true;
}

void CIronSightController::RenderScopeEffect( int x, int y, int w, int h, CViewSetup *pViewSetup )
{

#ifdef DEBUG
	if ( !r_ironsight_scope_effect.GetBool() )
		return;
#endif

	if ( !IsInIronSight() )
		return;

	// apply the blur effect to the screen while masking out the scope lens

	CMatRenderContextPtr pRenderContext(materials);

	ShaderStencilState_t stencilState_skip_scope_lens_pixels;
	stencilState_skip_scope_lens_pixels.m_bEnable = true;
	stencilState_skip_scope_lens_pixels.m_nReferenceValue = 1;
	stencilState_skip_scope_lens_pixels.m_CompareFunc = SHADER_STENCILFUNC_NOTEQUAL;
	stencilState_skip_scope_lens_pixels.m_PassOp = SHADER_STENCILOP_KEEP;
	stencilState_skip_scope_lens_pixels.m_FailOp = SHADER_STENCILOP_KEEP;
	stencilState_skip_scope_lens_pixels.m_ZFailOp = SHADER_STENCILOP_KEEP;
	pRenderContext->SetStencilState(stencilState_skip_scope_lens_pixels);

	// RENDER _rt_SmallFB0 to screen
	IMaterial *pBlurOverlayMaterial = materials->FindMaterial("dev/scope_bluroverlay", TEXTURE_GROUP_OTHER, true);

	//set alpha to the amount of ironsightedness
	IMaterialVar *pAlphaVar = pBlurOverlayMaterial->FindVar("$alpha", 0);
	if (pAlphaVar != NULL)
	{
		pAlphaVar->SetFloatValue(Bias( GetIronSightAmount(), 0.2f));
	}
	pRenderContext->DrawScreenSpaceQuad(pBlurOverlayMaterial);


	// now draw the laser dot, masked to ONLY render on the lens

	Vector2D dotCoords = GetDotCoords();
	dotCoords.x *= engine->GetScreenAspectRatio(w, h);

	//CMatRenderContextPtr pRenderContext(materials);
	IMaterial *pMatDot = materials->FindMaterial(GetDotMaterial(), TEXTURE_GROUP_OTHER, true);

	pRenderContext->OverrideDepthEnable(true, false, false);

	ShaderStencilState_t stencilState_use_only_scope_lens_pixels;
	stencilState_use_only_scope_lens_pixels.m_bEnable = true;
	stencilState_use_only_scope_lens_pixels.m_nReferenceValue = 1;
	stencilState_use_only_scope_lens_pixels.m_CompareFunc = SHADER_STENCILFUNC_EQUAL;
	stencilState_use_only_scope_lens_pixels.m_PassOp = SHADER_STENCILOP_KEEP;
	stencilState_use_only_scope_lens_pixels.m_FailOp = SHADER_STENCILOP_KEEP;
	stencilState_use_only_scope_lens_pixels.m_ZFailOp = SHADER_STENCILOP_KEEP;
	pRenderContext->SetStencilState(stencilState_use_only_scope_lens_pixels);

	int iWidth = GetDotWidth();

	IMaterialVar *pAlphaVar2 = pMatDot->FindVar("$alpha", 0);
	if (pAlphaVar2 != NULL)
	{
		pAlphaVar2->SetFloatValue(GetDotBlur());
	}

	dotCoords.x += 0.5f;
	dotCoords.y += 0.5f;
#ifdef DEBUG
	pRenderContext->DrawScreenSpaceRectangle(pMatDot, (w * dotCoords.x) - (iWidth / 2), (h * dotCoords.y) - (iWidth / 2), iWidth, iWidth,
		0, 0, ironsight_laser_dot_render_tweak1.GetInt(), ironsight_laser_dot_render_tweak1.GetInt(),
		ironsight_laser_dot_render_tweak2.GetInt(), ironsight_laser_dot_render_tweak2.GetInt());
#else
	pRenderContext->DrawScreenSpaceRectangle(pMatDot, (w * dotCoords.x) - (iWidth / 2), (h * dotCoords.y) - (iWidth / 2), iWidth, iWidth,
		0, 0, 61, 61,
		64, 64);
#endif

	pRenderContext->OverrideDepthEnable(false, true);


	// restore a disabled stencil state

	ShaderStencilState_t stencilStateDisable;
	stencilStateDisable.m_bEnable = false;
	pRenderContext->SetStencilState(stencilStateDisable);

	//clean up stencil buffer once we're done so render elements like the glow pass draw correctly
	pRenderContext->ClearBuffers(false, false, true);

}

#endif //CLIENT_DLL

bool CIronSightController::IsInIronSight( void )
{
	if ( m_pAttachedWeapon )
	{
		if ( IsDeploying() ||
			 IsDropped() ||
			 m_pAttachedWeapon->m_bInReload || 
			 m_pAttachedWeapon->IsSwitchingSilencer() )
			return false;

		CCSPlayer *pPlayer = ToCSPlayer( m_pAttachedWeapon->GetOwner() );
		if ( pPlayer && pPlayer->IsLookingAtWeapon() )
			return false;

#if defined ( CLIENT_DLL )
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetObserverTarget() == pPlayer && pLocalPlayer->GetObserverInterpState() == C_CSPlayer::OBSERVER_INTERP_TRAVELING )
			return false;
#endif

		if ( GetIronSightAmount() > 0  && (IsApproachingSighted() || IsApproachingUnSighted()) )
			return true;
	}

	return false;
}

float CIronSightController::GetIronSightFOV( float flDefaultFOV, bool bUseBiasedValue /*= false*/ )
{
	//sets biased value between the current FOV and the ideal IronSight FOV based on how 'ironsighted' the weapon currently is
	
	if ( !IsInIronSight() )
		return flDefaultFOV;

	float flIronSightFOVAmount = bUseBiasedValue ? GetIronSightAmountBiased() : GetIronSightAmount();

#ifdef DEBUG
	if ( ironsight_override.GetBool() )
	{
		return Lerp( flIronSightFOVAmount, flDefaultFOV, ironsight_fov.GetFloat() );
	}
#endif
	
	return Lerp( flIronSightFOVAmount, flDefaultFOV, GetIronSightIdealFOV() );
}

bool CIronSightController::Init( CWeaponCSBase *pWeaponToMonitor )
{
	if ( IsInitializedAndAvailable() )
	{
		return true;
	}
	else if( pWeaponToMonitor )
	{
		// currently only the following hardcoded weapon types support ironsights:
		if (pWeaponToMonitor->GetCSWeaponID() != WEAPON_AUG &&
			pWeaponToMonitor->GetCSWeaponID() != WEAPON_SG556)
		{
			return false;
		}

		if ( pWeaponToMonitor->GetCSWeaponID() == WEAPON_AUG )
		{
			m_pAttachedWeapon = pWeaponToMonitor;

			m_bIronSightAvailable			= true;
			m_flIronSightLooseness			= 0.03f;
			m_flIronSightPullUpSpeed		= 10.0f;
			m_flIronSightPutDownSpeed		= 8.0f;
			m_flIronSightFOV				= 45.0f;
			m_flIronSightPivotForward		= 10.0f;
			m_vecEyePos						= Vector( -1.56, -3.6, -0.07 );
			m_angPivotAngle					= QAngle( 0.78, -0.1, -0.03 );
			return true;
		}
		else if (pWeaponToMonitor->GetCSWeaponID() == WEAPON_SG556)
		{
			m_pAttachedWeapon = pWeaponToMonitor;

			m_bIronSightAvailable			= true;
			m_flIronSightLooseness			= 0.03f;
			m_flIronSightPullUpSpeed		= 10.0f;
			m_flIronSightPutDownSpeed		= 8.0f;
			m_flIronSightFOV				= 45.0f;
			m_flIronSightPivotForward		= 8.0f;
			m_vecEyePos						= Vector(0.72, -5.12, -1.33);
			m_angPivotAngle					= QAngle(0.52, 0.04, 0.72);
			return true;
		}

		CEconItemView *pEconItem = ( (CWeaponCSBase *)pWeaponToMonitor )->GetEconItemView();

		if ( pEconItem && pEconItem->IsValid() )
		{
			m_pAttachedWeapon = pWeaponToMonitor;

			float flTemp;
			uint32 unTemp;

			static CSchemaAttributeDefHandle pAttrbIronSightCapable("aimsight capable");
			if (pEconItem->FindAttribute(pAttrbIronSightCapable, &unTemp))
			{
				m_bIronSightAvailable = (unTemp != 0);
				if (m_bIronSightAvailable)
				{
					static CSchemaAttributeDefHandle pAttrflIronSightLooseness("aimsight looseness");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightLooseness, &flTemp);
					m_flIronSightLooseness = flTemp;

					static CSchemaAttributeDefHandle pAttrflIronSightPullUpSpeed("aimsight speed up");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightPullUpSpeed, &flTemp);
					m_flIronSightPullUpSpeed = flTemp;

					static CSchemaAttributeDefHandle pAttrflIronSightPutDownSpeed("aimsight speed down");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightPutDownSpeed, &flTemp);
					m_flIronSightPutDownSpeed = flTemp;

					static CSchemaAttributeDefHandle pAttrflIronSightFOV("aimsight fov");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightFOV, &flTemp);
					m_flIronSightFOV = flTemp;

					static CSchemaAttributeDefHandle pAttrflIronSightPivotForward("aimsight pivot forward");
					FindAttribute_UnsafeBitwiseCast<attrib_value_t>(pEconItem, pAttrflIronSightPivotForward, &flTemp);
					m_flIronSightPivotForward = flTemp;

					static CSchemaAttributeDefHandle pAttrDef_IronSightEyePos("aimsight eye pos");
					pEconItem->FindAttribute(pAttrDef_IronSightEyePos, &m_vecEyePos);

					static CSchemaAttributeDefHandle pAttrDef_IronSightPivotAngle("aimsight pivot angle");
					Vector temp;
					pEconItem->FindAttribute(pAttrDef_IronSightPivotAngle, &temp);
					m_angPivotAngle.x = temp.x;
					m_angPivotAngle.y = temp.y;
					m_angPivotAngle.z = temp.z;

					return true;
				}

			}

		}

	}
	
	return false;
}

#endif //IRONSIGHT