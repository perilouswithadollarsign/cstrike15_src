//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "baseviewmodel_shared.h"
#include "datacache/imdlcache.h"

#include "cs_shareddefs.h"

#if defined( CLIENT_DLL )
#include "iprediction.h"
#include "prediction.h"
#include "inputsystem/iinputsystem.h"
#include "iclientmode.h"

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
#include "weapon_basecsgrenade.h"
#endif

#else
#include "vguiscreen.h"
#endif

#if defined( CLIENT_DLL ) && defined( SIXENSE )
#include "sixense/in_sixense.h"
#include "sixense/sixense_convars_extern.h"
#endif

extern ConVar in_forceuser;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VIEWMODEL_ANIMATION_PARITY_BITS 3
#define SCREEN_OVERLAY_MATERIAL "vgui/screens/vgui_overlay"

#if defined( CLIENT_DLL )
	ConVar viewmodel_offset_x( "viewmodel_offset_x", "0.0", FCVAR_ARCHIVE );	 // the viewmodel offset from default in X
	ConVar viewmodel_offset_y( "viewmodel_offset_y", "0.0", FCVAR_ARCHIVE );	 // the viewmodel offset from default in Y
	ConVar viewmodel_offset_z( "viewmodel_offset_z", "0.0", FCVAR_ARCHIVE );	 // the viewmodel offset from default in Z

#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseViewModel::CBaseViewModel()
{
#if defined( CLIENT_DLL )
	// NOTE: We do this here because the color is never transmitted for the view model.
	m_nOldAnimationParity = 0;
	m_EntClientFlags |= ENTCLIENTFLAG_ALWAYS_INTERPOLATE;
	RenderWithViewModels( true );
	m_flStatTrakGlowMultiplier = 0.0f;
	m_flStatTrakGlowMultiplierIdeal = 0.0f;
	m_szLastSound[0] = '\0';
	m_flLastSoundTime = 0.0f;

	m_flCamDriverAppliedTime = 0;
	m_flCamDriverWeight = 0;
	m_vecCamDriverLastPos.Init();
	m_angCamDriverLastAng.Init();

#ifdef IRONSIGHT
	m_bScopeStencilMaskModeEnabled = false;
#endif

#endif

	SetRenderColor( 255, 255, 255 );
	SetRenderAlpha( 255 );

	// View model of this weapon
	m_sVMName			= NULL_STRING;		
	// Prefix of the animations that should be used by the player carrying this weapon
	m_sAnimationPrefix	= NULL_STRING;

	m_nViewModelIndex	= 0;

	m_nAnimationParity	= 0;

	m_bShouldIgnoreOffsetAndAccuracy = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseViewModel::~CBaseViewModel()
{
}

void CBaseViewModel::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	DestroyControlPanels();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::Spawn( void )
{
	Precache( );
	SetSize( Vector( -8, -4, -2), Vector(8, 4, 2) );
	SetSolid( SOLID_NONE );
}


#if defined ( CSTRIKE_DLL ) && !defined ( CLIENT_DLL )
#define VGUI_CONTROL_PANELS
#endif

#if defined ( TF_DLL )
#define VGUI_CONTROL_PANELS
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetControlPanelsActive( bool bState )
{
#if defined( VGUI_CONTROL_PANELS )
	// Activate control panel screens
	for ( int i = m_hScreens.Count(); --i >= 0; )
	{
		if (m_hScreens[i].Get())
		{
			m_hScreens[i]->SetActive( bState );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// This is called by the base object when it's time to spawn the control panels
//-----------------------------------------------------------------------------
void CBaseViewModel::SpawnControlPanels()
{
#if defined( VGUI_CONTROL_PANELS )
	char buf[64];

	// Destroy existing panels
	DestroyControlPanels();

	CBaseCombatWeapon *weapon = m_hWeapon.Get();

	if ( weapon == NULL )
	{
		return;
	}

	MDLCACHE_CRITICAL_SECTION();

	// FIXME: Deal with dynamically resizing control panels?

	// If we're attached to an entity, spawn control panels on it instead of use
	CBaseAnimating *pEntityToSpawnOn = this;
	char *pOrgLL = "controlpanel%d_ll";
	char *pOrgUR = "controlpanel%d_ur";
	char *pAttachmentNameLL = pOrgLL;
	char *pAttachmentNameUR = pOrgUR;
	/*
	if ( IsBuiltOnAttachment() )
	{
		pEntityToSpawnOn = dynamic_cast<CBaseAnimating*>((CBaseEntity*)m_hBuiltOnEntity.Get());
		if ( pEntityToSpawnOn )
		{
			char sBuildPointLL[64];
			char sBuildPointUR[64];
			Q_snprintf( sBuildPointLL, sizeof( sBuildPointLL ), "bp%d_controlpanel%%d_ll", m_iBuiltOnPoint );
			Q_snprintf( sBuildPointUR, sizeof( sBuildPointUR ), "bp%d_controlpanel%%d_ur", m_iBuiltOnPoint );
			pAttachmentNameLL = sBuildPointLL;
			pAttachmentNameUR = sBuildPointUR;
		}
		else
		{
			pEntityToSpawnOn = this;
		}
	}
	*/

	Assert( pEntityToSpawnOn );

	// Lookup the attachment point...
	int nPanel;
	for ( nPanel = 0; true; ++nPanel )
	{
		Q_snprintf( buf, sizeof( buf ), pAttachmentNameLL, nPanel );
		int nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nLLAttachmentIndex <= 0)
		{
			// Try and use my panels then
			pEntityToSpawnOn = this;
			Q_snprintf( buf, sizeof( buf ), pOrgLL, nPanel );
			nLLAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nLLAttachmentIndex <= 0)
				return;
		}

		Q_snprintf( buf, sizeof( buf ), pAttachmentNameUR, nPanel );
		int nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
		if (nURAttachmentIndex <= 0)
		{
			// Try and use my panels then
			Q_snprintf( buf, sizeof( buf ), pOrgUR, nPanel );
			nURAttachmentIndex = pEntityToSpawnOn->LookupAttachment(buf);
			if (nURAttachmentIndex <= 0)
				return;
		}

		const char *pScreenName;
		weapon->GetControlPanelInfo( nPanel, pScreenName );
		if (!pScreenName)
			continue;

		const char *pScreenClassname;
		weapon->GetControlPanelClassName( nPanel, pScreenClassname );
		if ( !pScreenClassname )
			continue;

		// Compute the screen size from the attachment points...
		matrix3x4_t	panelToWorld;
		pEntityToSpawnOn->GetAttachment( nLLAttachmentIndex, panelToWorld );

		matrix3x4_t	worldToPanel;
		MatrixInvert( panelToWorld, worldToPanel );

		// Now get the lower right position + transform into panel space
		Vector lr, lrlocal;
		pEntityToSpawnOn->GetAttachment( nURAttachmentIndex, panelToWorld );
		MatrixGetColumn( panelToWorld, 3, lr );
		VectorTransform( lr, worldToPanel, lrlocal );

		// Not sure why, but the transform for the vgui panel to the world is improperly scaling.
		// We add a fudge value here to compensate.
		const float SCALE_FUDGE = 1.6f;
		float flWidth = fabs( lrlocal.x ) * SCALE_FUDGE;
		float flHeight = fabs( lrlocal.y ) * SCALE_FUDGE;

		CVGuiScreen *pScreen = CreateVGuiScreen( pScreenClassname, pScreenName, pEntityToSpawnOn, this, nLLAttachmentIndex );
		pScreen->ChangeTeam( GetTeamNumber() );
		pScreen->SetActualSize( flWidth, flHeight );
		pScreen->SetActive( false );
		pScreen->MakeVisibleOnlyToTeammates( false );
	
		pScreen->SetAttachedToViewModel( true );
		int nScreen = m_hScreens.AddToTail( );
		m_hScreens[nScreen].Set( pScreen );
	}
#endif
}

void CBaseViewModel::DestroyControlPanels()
{
#if defined( VGUI_CONTROL_PANELS )
	// Kill the control panels
	int i;
	for ( i = m_hScreens.Count(); --i >= 0; )
	{
		DestroyVGuiScreen( m_hScreens[i].Get() );
	}
	m_hScreens.RemoveAll();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pEntity - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetOwner( CBaseEntity *pEntity )
{
	m_hOwner = pEntity;
#if !defined( CLIENT_DLL )
	// Make sure we're linked into hierarchy
	//SetParent( pEntity );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nIndex - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetIndex( int nIndex )
{
	m_nViewModelIndex = nIndex;
	Assert( m_nViewModelIndex < (1 << VIEWMODEL_INDEX_BITS) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseViewModel::ViewModelIndex( ) const
{
	return m_nViewModelIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Pass our visibility on to our child screens
//-----------------------------------------------------------------------------
void CBaseViewModel::AddEffects( int nEffects )
{
	if ( nEffects & EF_NODRAW )
	{
		SetControlPanelsActive( false );
	}

	BaseClass::AddEffects( nEffects );
}

//-----------------------------------------------------------------------------
// Purpose: Pass our visibility on to our child screens
//-----------------------------------------------------------------------------
void CBaseViewModel::RemoveEffects( int nEffects )
{
	if ( nEffects & EF_NODRAW )
	{
		SetControlPanelsActive( true );
	}

	BaseClass::RemoveEffects( nEffects );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *modelname - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SetWeaponModel( const char *modelname, CBaseCombatWeapon *weapon )
{
	m_hWeapon = weapon;

#if defined( CLIENT_DLL )
	SetModel( modelname );
#else
	string_t str;
	if ( modelname != NULL )
	{
		str = MAKE_STRING( modelname );
	}
	else
	{
		str = NULL_STRING;
	}

	if ( str != m_sVMName )
	{
		// Msg( "SetWeaponModel %s at %f\n", modelname, gpGlobals->curtime );
		m_sVMName = str;
		SetModel( STRING( m_sVMName ) );

		// Create any vgui control panels associated with the weapon
		SpawnControlPanels();

		bool showControlPanels = weapon && weapon->ShouldShowControlPanels();
		SetControlPanelsActive( showControlPanels );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CBaseCombatWeapon
//-----------------------------------------------------------------------------
CBaseCombatWeapon *CBaseViewModel::GetOwningWeapon( void )
{
	return m_hWeapon.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sequence - 
//-----------------------------------------------------------------------------
void CBaseViewModel::SendViewModelMatchingSequence( int sequence )
{
	// since all we do is send a sequence number down to the client, 
	// set this here so other weapons code knows which sequence is playing.
	SetSequence( sequence );

	m_nAnimationParity = ( m_nAnimationParity + 1 ) & ( (1<<VIEWMODEL_ANIMATION_PARITY_BITS) - 1 );

#if defined( CLIENT_DLL )
	m_nOldAnimationParity = m_nAnimationParity;

	// Force frame interpolation to start at exactly frame zero
	m_flAnimTime			= gpGlobals->curtime;
#else
	CBaseCombatWeapon *weapon = m_hWeapon.Get();
	bool showControlPanels = weapon && weapon->ShouldShowControlPanels();
	SetControlPanelsActive( showControlPanels );
#endif

	// Restart animation at frame 0
	SetCycle( 0 );
	ResetSequenceInfo();
}

#if defined( CLIENT_DLL )
#include "ivieweffects.h"
#endif

#ifdef CLIENT_DLL
void CBaseViewModel::PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] )
{
	int nCamDriverBone = LookupBone( "cam_driver" );
	if ( nCamDriverBone != -1 )
	{
		m_flCamDriverAppliedTime = gpGlobals->curtime;		
		VectorCopy( pos[nCamDriverBone], m_vecCamDriverLastPos );
		QuaternionAngles( q[nCamDriverBone], m_angCamDriverLastAng );

		if ( ShouldFlipModel() )
		{
			m_angCamDriverLastAng[YAW] = -m_angCamDriverLastAng[YAW];
			m_vecCamDriverLastPos.y = -m_vecCamDriverLastPos.y;
		}

	}
}
#endif

void CBaseViewModel::CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, const QAngle& eyeAngles )
{

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
#ifdef CLIENT_DLL
	// apply viewmodel pose param
	if ( owner )
	{
		CBaseCSGrenade* pGrenade = dynamic_cast<CBaseCSGrenade*>( owner->GetActiveWeapon() );
		if ( pGrenade )
		{
			int iPoseParam = LookupPoseParameter( "throwcharge" );
			if ( iPoseParam != -1 )
				SetPoseParameter( iPoseParam, clamp(pGrenade->ApproachThrownStrength(), 0.0f, 1.0f) );
		}
	}
#endif
#endif

	// UNDONE: Calc this on the server?  Disabled for now as it seems unnecessary to have this info on the server
#if defined( CLIENT_DLL )
	QAngle vmangoriginal = eyeAngles;
	QAngle vmangles = eyeAngles;
	Vector vmorigin = eyePosition;

	Vector vecRight;
	Vector vecUp;
	Vector vecForward;
	AngleVectors( vmangoriginal, &vecForward, &vecRight, &vecUp );
	//Vector vecOffset = Vector( viewmodel_offset_x.GetFloat(), viewmodel_offset_y.GetFloat(), viewmodel_offset_z.GetFloat() ); 
	if ( !m_bShouldIgnoreOffsetAndAccuracy )
	{
#ifdef IRONSIGHT
		CWeaponCSBase *pIronSightWeapon = (CWeaponCSBase*)owner->GetActiveWeapon();
		if ( pIronSightWeapon )
		{
			CIronSightController* pIronSightController = pIronSightWeapon->GetIronSightController();
			if ( pIronSightController && pIronSightController->IsInIronSight() )
			{
				float flInvIronSightAmount = ( 1.0f - pIronSightController->GetIronSightAmount() );

				vecForward *= flInvIronSightAmount;
				vecUp *= flInvIronSightAmount;
				vecRight *=	flInvIronSightAmount;
			}
		}	
#endif
		vmorigin += (vecForward * viewmodel_offset_y.GetFloat()) + (vecUp * viewmodel_offset_z.GetFloat()) + (vecRight * viewmodel_offset_x.GetFloat());
	}

	// TrackIR
	if ( IsHeadTrackingEnabled() )
	{
		vmorigin = owner->EyePosition();
		VectorAngles( owner->GetAutoaimVector( AUTOAIM_5DEGREES ), vmangoriginal );
		vmangles = vmangoriginal;
	}
	// TrackIR

	CBaseCombatWeapon *pWeapon = m_hWeapon.Get();
	//Allow weapon lagging
	if ( pWeapon != NULL )
	{
		if ( !prediction->InPrediction() )
		{
			// add weapon-specific bob 
			pWeapon->AddViewmodelBob( this, vmorigin, vmangles );
#if defined ( CSTRIKE_DLL )
			CalcViewModelLag( vmorigin, vmangles, vmangoriginal );
#endif
		}
	}
	// Add model-specific bob even if no weapon associated (for head bob for off hand models)
	AddViewModelBob( owner, vmorigin, vmangles );
#if !defined ( CSTRIKE_DLL )
	// This was causing weapon jitter when rotating in updated CS:S; original Source had this in above InPrediction block  07/14/10
	// Add lag
	CalcViewModelLag( vmorigin, vmangles, vmangoriginal );
#endif

	if ( !prediction->InPrediction() )
	{
		// Let the viewmodel shake at about 10% of the amplitude of the player's view
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( GetOwner() );
		GetViewEffects()->ApplyShake( vmorigin, vmangles, 0.1 );	
	}

	SetLocalOrigin( vmorigin );
	SetLocalAngles( vmangles );

#endif //#if defined( CLIENT_DLL )

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseViewModel::CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles )
{
	Vector vOriginalOrigin = origin;
	QAngle vOriginalAngles = angles;

	// Calculate our drift
	Vector	forward;
	AngleVectors( angles, &forward, NULL, NULL );

	if ( gpGlobals->frametime != 0.0f )
	{
		Vector vDifference;
		VectorSubtract( forward, m_vecLastFacing, vDifference );

		float flSpeed = 5.0f;

		// If we start to lag too far behind, we'll increase the "catch up" speed.  Solves the problem with fast cl_yawspeed, m_yaw or joysticks
		//  rotating quickly.  The old code would slam lastfacing with origin causing the viewmodel to pop to a new position
		float flDiff = vDifference.Length();
		if ( flDiff > 1.5f )
		{
			float flScale = flDiff / 1.5f;
			flSpeed *= flScale;
		}

		// FIXME:  Needs to be predictable?
		VectorMA( m_vecLastFacing, flSpeed * gpGlobals->frametime, vDifference, m_vecLastFacing );
		// Make sure it doesn't grow out of control!!!
		VectorNormalize( m_vecLastFacing );
		VectorMA( origin, 5.0f, vDifference * -1.0f, origin );

		Assert( m_vecLastFacing.IsValid() );
	}

#if !defined( PORTAL ) //floor/wall floor/floor portals cause a sudden and large pitch change, causing a pop unless we write a bunch of interpolation code. Easier to just disable this
	Vector right, up;
	AngleVectors( original_angles, &forward, &right, &up );

	float pitch = original_angles[ PITCH ];
	if ( pitch > 180.0f )
		pitch -= 360.0f;
	else if ( pitch < -180.0f )
		pitch += 360.0f;

	//FIXME: These are the old settings that caused too many exposed polys on some models
	VectorMA( origin, -pitch * 0.035f,	forward,	origin );
	VectorMA( origin, -pitch * 0.03f,		right,	origin );
	VectorMA( origin, -pitch * 0.02f,		up,		origin);
#endif
}

//-----------------------------------------------------------------------------
// Stub to keep networking consistent for DEM files
//-----------------------------------------------------------------------------
#if defined( CLIENT_DLL )
  extern void RecvProxy_EffectFlags( const CRecvProxyData *pData, void *pStruct, void *pOut );
 void RecvProxy_ViewmodelSequenceNum( const CRecvProxyData *pData, void *pStruct, void *pOut );
 void RecvProxy_Viewmodel( const CRecvProxyData *pData, void *pStruct, void *pOut );
#endif

//-----------------------------------------------------------------------------
// Purpose: Resets anim cycle when the server changes the weapon on us
//-----------------------------------------------------------------------------
#if defined( CLIENT_DLL )
static void RecvProxy_Weapon( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseViewModel *pViewModel = ((CBaseViewModel*)pStruct);
	CBaseCombatWeapon *pOldWeapon = pViewModel->GetOwningWeapon();
	bool bViewModelWasVisible = pViewModel->IsVisible();

	// Chain through to the default recieve proxy ...
	RecvProxy_IntToEHandle( pData, pStruct, pOut );

	// ... and reset our cycle index if the server is switching weapons on us
	CBaseCombatWeapon *pNewWeapon = pViewModel->GetOwningWeapon();
	if ( pNewWeapon != pOldWeapon || !bViewModelWasVisible )
	{
		// Restart animation at frame 0
		pViewModel->SetCycle( 0 );
		pViewModel->m_flAnimTime = gpGlobals->curtime;
	}
}

static void RecvProxy_Owner( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseViewModel *pViewModel = ( ( CBaseViewModel* )pStruct );
	//Msg( "BaseViewModel changed from (%d)%x", ( pViewModel->m_hOwner.GetForModify().GetSerialNumber(), pViewModel->m_hOwner.GetForModify().GetEntryIndex() ) );

	// Chain through to the default recieve proxy ...
	RecvProxy_IntToEHandle( pData, pStruct, pOut );
	
	//Msg( " to (%d)%x\n", ( pViewModel->m_hOwner.GetForModify().GetSerialNumber(), pViewModel->m_hOwner.GetForModify().GetEntryIndex() ) );
	pViewModel->UpdateVisibility(); // visibility of a viewmodel is owner-dependant, and other events like SetDormant() may happen out of order with setting owner, especially when doing full frame update after spectator mode, which happens most often (pretty much exclusively) after HLTV replay ends.
}
#endif


IMPLEMENT_NETWORKCLASS_ALIASED( BaseViewModel, DT_BaseViewModel )
LINK_ENTITY_TO_CLASS_ALIASED( viewmodel, BaseViewModel );

BEGIN_NETWORK_TABLE_NOBASE(CBaseViewModel, DT_BaseViewModel)
#if !defined( CLIENT_DLL )
	SendPropModelIndex(SENDINFO(m_nModelIndex)),
	SendPropEHandle (SENDINFO(m_hWeapon)),
	SendPropInt		(SENDINFO(m_nBody), ANIMATION_BODY_BITS ), // increased to 32 bits to support number of bits equal to number of bodygroups
	SendPropInt		(SENDINFO(m_nSkin), 10),
	SendPropInt		(SENDINFO(m_nSequence),	8, SPROP_UNSIGNED),
	SendPropInt		(SENDINFO(m_nViewModelIndex), VIEWMODEL_INDEX_BITS, SPROP_UNSIGNED),
	SendPropFloat	(SENDINFO(m_flPlaybackRate),	8,	SPROP_ROUNDUP,	-4.0,	12.0f),
	SendPropInt		(SENDINFO(m_fEffects),		EF_MAX_BITS, SPROP_UNSIGNED),
	SendPropInt		(SENDINFO(m_nAnimationParity), 3, SPROP_UNSIGNED ),
	SendPropEHandle (SENDINFO(m_hOwner)),

	SendPropInt( SENDINFO( m_nNewSequenceParity ), EF_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nResetEventsParity ), EF_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO( m_nMuzzleFlashParity ), EF_MUZZLEFLASH_BITS, SPROP_UNSIGNED ),

	SendPropBool( SENDINFO( m_bShouldIgnoreOffsetAndAccuracy ) ),
#else
	RecvPropInt		(RECVINFO(m_nModelIndex), 0, RecvProxy_Viewmodel ),
	RecvPropEHandle (RECVINFO(m_hWeapon), RecvProxy_Weapon ),
	RecvPropInt		(RECVINFO(m_nSkin)),
	RecvPropInt		(RECVINFO(m_nBody)),
	RecvPropInt		(RECVINFO(m_nSequence), 0, RecvProxy_ViewmodelSequenceNum ),
	RecvPropInt		(RECVINFO(m_nViewModelIndex)),
	RecvPropFloat	(RECVINFO(m_flPlaybackRate)),
	RecvPropInt		(RECVINFO(m_fEffects), 0, RecvProxy_EffectFlags ),
	RecvPropInt		(RECVINFO(m_nAnimationParity)),
	RecvPropEHandle (RECVINFO(m_hOwner), RecvProxy_Owner ),

	RecvPropInt( RECVINFO( m_nNewSequenceParity )),
	RecvPropInt( RECVINFO( m_nResetEventsParity )),
	RecvPropInt( RECVINFO( m_nMuzzleFlashParity )),

	RecvPropBool( RECVINFO( m_bShouldIgnoreOffsetAndAccuracy ) ),

#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL

BEGIN_PREDICTION_DATA( CBaseViewModel )

	// Networked
	DEFINE_PRED_FIELD( m_nModelIndex, FIELD_SHORT, FTYPEDESC_INSENDTABLE | FTYPEDESC_MODELINDEX ),
	DEFINE_PRED_FIELD( m_nSkin, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nBody, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nViewModelIndex, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flPlaybackRate, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.125f ),
	DEFINE_PRED_FIELD( m_fEffects, FIELD_INTEGER, FTYPEDESC_INSENDTABLE | FTYPEDESC_OVERRIDE ),
	DEFINE_PRED_FIELD( m_nAnimationParity, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hWeapon, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flAnimTime, FIELD_FLOAT, 0 ),

	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT ),
	DEFINE_FIELD( m_Activity, FIELD_INTEGER ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_PRIVATE | FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),

END_PREDICTION_DATA()

// This needed to be done as a proxy for the surrounding box auto update when animations change.
// This doesn't have to be done for view models as they don't affect the bounding box and it was
// causing some timing problems with our world to view model under the covers swap.

// [msmith] Added back in for CS:GO because without this the m_nSequence number gets reset during prediction causing
// view model animations to freeze up.  This issue is probably caused by the fact that prediction doesn't fix up
// m_nSequence, but this fixes it and makes it consistent with CS:S ... which also has the same prediction issues.
void RecvProxy_ViewmodelSequenceNum( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	CBaseViewModel *model = (CBaseViewModel *)pStruct;
	if (pData->m_Value.m_Int != model->GetSequence())
	{
		MDLCACHE_CRITICAL_SECTION();
		model->SetSequence(pData->m_Value.m_Int);
		model->m_flAnimTime = gpGlobals->curtime;
		model->SetCycle(0);
	}
}

void RecvProxy_Viewmodel( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// We assign the model index via the SetModelByIndex function so that the model pointer gets updated as soon as we change the model index.
	// This is necessary since this new model may be accessed with frame.
	// An example is the SetSequence code in RecvProxy_ViewmodelSequenceNum that checks to make sure the sequence number is in range of those available in
	// model.
	CBaseViewModel *model = (CBaseViewModel *)pStruct;
	if ( model )
	{
		MDLCACHE_CRITICAL_SECTION();
		model->SetModelByIndex( pData->m_Value.m_Int );
	}
	
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CBaseViewModel::LookupAttachment( const char *pAttachmentName )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->LookupAttachment( pAttachmentName );

	return BaseClass::LookupAttachment( pAttachmentName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachment( int number, matrix3x4_t &matrix )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachment( number, matrix );

	return BaseClass::GetAttachment( number, matrix );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachment( int number, Vector &origin )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachment( number, origin );

	return BaseClass::GetAttachment( number, origin );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachment( number, origin, angles );

	return BaseClass::GetAttachment( number, origin, angles );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseViewModel::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
{
	if ( m_hWeapon.Get() && m_hWeapon.Get()->WantsToOverrideViewmodelAttachments() )
		return m_hWeapon.Get()->GetAttachmentVelocity( number, originVel, angleVel );

	return BaseClass::GetAttachmentVelocity( number, originVel, angleVel );
}

#endif
