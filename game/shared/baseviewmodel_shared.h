//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef BASEVIEWMODEL_SHARED_H
#define BASEVIEWMODEL_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "predictable_entity.h"
#include "utlvector.h"
#include "baseplayer_shared.h"
#include "shared_classnames.h"
#include "ihasowner.h"

#ifdef CSTRIKE15
#include "cs_shareddefs.h"
#endif

class CBaseCombatWeapon;
class CBaseCombatCharacter;
class CVGuiScreen;

#if defined( CLIENT_DLL )
class C_ViewmodelAttachmentModel;
class C_CSPlayer;
#define CBaseViewModel C_BaseViewModel
#undef CBaseCombatWeapon
#define CBaseCombatWeapon C_BaseCombatWeapon
#define NUM_UID_CHARS 20

class C_BaseViewModel;

//--------------------------------------------------------------------------------------------------------
class C_ViewmodelAttachmentModel : public C_BaseAnimating
{
	DECLARE_CLASS( C_ViewmodelAttachmentModel, C_BaseAnimating );
public:

	bool InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels );

	void SetViewmodel( C_BaseViewModel *pVM );
	virtual int InternalDrawModel( int flags, const RenderableInstance_t &instance );

private:
	CHandle< C_BaseViewModel > m_hViewmodel;
};

#endif

#define VIEWMODEL_INDEX_BITS 1

class CBaseViewModel : public CBaseAnimating, public IHasOwner
{
	DECLARE_CLASS( CBaseViewModel, CBaseAnimating );
public:

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

#if !defined( CLIENT_DLL )
	DECLARE_DATADESC();
#endif

							CBaseViewModel( void );
							~CBaseViewModel( void );


	bool IsViewable(void) { return false; }

	virtual void					UpdateOnRemove( void );

	// Weapon client handling
	virtual void			SendViewModelMatchingSequence( int sequence );
	virtual void			SetWeaponModel( const char *pszModelname, CBaseCombatWeapon *weapon );

	virtual void			CalcViewModelLag( Vector& origin, QAngle& angles, QAngle& original_angles );
	virtual void			CalcViewModelView( CBasePlayer *owner, const Vector& eyePosition, 
								const QAngle& eyeAngles );
	virtual void			AddViewModelBob( CBasePlayer *owner, Vector& eyePosition, QAngle& eyeAngles ) {};

	void					CalcViewModelUnzoom( CBasePlayer *owner, Vector& origin, QAngle& angles );

	// Initializes the viewmodel for use							
	void					SetOwner( CBaseEntity *pEntity );
	void					SetIndex( int nIndex );
	// Returns which viewmodel it is
	int						ViewModelIndex( ) const;

	virtual void			Precache( void );

	virtual void			Spawn( void );

	virtual CBaseEntity *GetOwner( void ) { return m_hOwner; };

	virtual void			AddEffects( int nEffects );
	virtual void			RemoveEffects( int nEffects );

	void					SpawnControlPanels();
	void					DestroyControlPanels();
	void					SetControlPanelsActive( bool bState );
	void					ShowControlPanells( bool show );

	virtual CBaseCombatWeapon *GetOwningWeapon( void );
	
	virtual CBaseEntity	*GetOwnerViaInterface( void ) { return GetOwner(); }

	virtual bool			IsSelfAnimating()
	{
		return true;
	}

	Vector					m_vecLastFacing;

	virtual bool			IsViewModel() const { return true; }
	virtual bool			IsViewModelOrAttachment() const { return true; }

	void					UpdateAllViewmodelAddons( void );

#if defined ( CLIENT_DLL )
	C_ViewmodelAttachmentModel *AddViewmodelArmModel( const char *pszModel, int nSkintoneIndex = -1 );
	C_ViewmodelAttachmentModel* FindArmModelForLoadoutPosition( loadout_positions_t nPosition ) const;
#endif
	void					AddViewmodelLabel( CEconItemView *pItem );
	void					AddViewmodelStatTrak( CEconItemView *pItem, int nStatTrakType, int nWeaponID, AccountID_t holderAcctId );
	void					AddViewmodelStickers( CEconItemView *pItem, int nWeaponID );
	bool					ViewmodelStickersAreValid( int nWeaponID );

	void					RemoveViewmodelArmModels( void );
	void					RemoveViewmodelLabel( void );
	void					RemoveViewmodelStatTrak( void );
	void					RemoveViewmodelStickers( void );

	CNetworkVar(bool, m_bShouldIgnoreOffsetAndAccuracy );
	virtual void			SetShouldIgnoreOffsetAndAccuracy( bool bIgnore ) { m_bShouldIgnoreOffsetAndAccuracy = bIgnore; }

#if !defined( CLIENT_DLL )
	virtual int				UpdateTransmitState( void );
	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );
	virtual void			SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
#else

	virtual void			FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );

	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual void			PostDataUpdate( DataUpdateType_t updateType );

	virtual C_BasePlayer	*GetPredictionOwner( void );

	virtual bool			Interpolate( float currentTime );

	virtual bool			ShouldFlipModel( void );
	void					UpdateAnimationParity( void );

	virtual void			PostBuildTransformations( CStudioHdr *pStudioHdr, BoneVector *pos, BoneQuaternion q[] );
	Vector m_vecCamDriverLastPos;
	QAngle m_angCamDriverLastAng;
	float m_flCamDriverAppliedTime;
	float m_flCamDriverWeight;

	virtual void			ApplyBoneMatrixTransform( matrix3x4_t& transform );

	virtual bool			ShouldDraw();
	virtual bool			ShouldSuppressForSplitScreenPlayer( int nSlot );
	virtual int				DrawModel( int flags, const RenderableInstance_t &instance );
	virtual int				DrawOverriddenViewmodel( C_BaseViewModel *pViewmodel, int flags, const RenderableInstance_t &instance );
	virtual uint8			OverrideAlphaModulation( uint8 nAlpha );
	RenderableTranslucencyType_t ComputeTranslucencyType( void );

	// Should this object cast shadows?
	virtual ShadowType_t	ShadowCastType() { return SHADOWS_NONE; }

	// Should this object receive shadows?
	virtual bool			ShouldReceiveProjectedTextures( int flags )
	{
		return false;
	}

	virtual void			GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS]);

	// See C_StudioModel's definition of this.
	virtual void			UncorrectViewModelAttachment( Vector &vOrigin );

	// (inherited from C_BaseAnimating)
	virtual void			FormatViewModelAttachment( int nAttachment, matrix3x4_t &attachmentToWorld );

	CBaseCombatWeapon		*GetWeapon() const { return m_hWeapon.Get(); }


	virtual bool			ShouldResetSequenceOnNewModel( void ) { return false; }

	// Attachments
	virtual int				LookupAttachment( const char *pAttachmentName );
	virtual bool			GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool			GetAttachment( int number, Vector &origin );
	virtual	bool			GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool			GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel );

	virtual bool 			Simulate( void );

private:
	CBaseViewModel( const CBaseViewModel & ); // not defined, not accessible

	void					UpdateParticles( int nSlot );

	virtual void 			OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void 			OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

	CUtlReference<CNewParticleEffect> m_viewmodelParticleEffect;

#endif

#ifdef PORTAL2
	// We need to always transition because we handle our transition volumes in a different manner
	virtual int				ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_FORCE_TRANSITION; }
#endif // PORTAL2

private:
	typedef CHandle< CBaseCombatWeapon > CBaseCombatWeaponHandle;
// FTYPEDESC_INSENDTABLE STUFF
	CNetworkVar( int, m_nViewModelIndex );		// Which viewmodel is it?
	// Used to force restart on client, only needs a few bits
	CNetworkVar( int, m_nAnimationParity );
	CNetworkVar( CBaseCombatWeaponHandle, m_hWeapon );
// FTYPEDESC_INSENDTABLE STUFF (end)

	CNetworkHandle( CBaseEntity, m_hOwner );				// Player or AI carrying this weapon

	// soonest time Update will call WeaponIdle
	float					m_flTimeWeaponIdle;							

	Activity				m_Activity;

	// Weapon art
	string_t				m_sVMName;			// View model of this weapon
	string_t				m_sAnimationPrefix;		// Prefix of the animations that should be used by the player carrying this weapon

#if defined( CLIENT_DLL )
	int						m_nOldAnimationParity;

public:
	float					m_fCycleOffset;

	void					UpdateStatTrakGlow( void );
	void					SetStatTrakGlowMultiplier( float flNewIdealGlow ) { m_flStatTrakGlowMultiplierIdeal = flNewIdealGlow; }
	const float				GetStatTrakGlowMultiplier( void ){ return m_flStatTrakGlowMultiplier; }

#ifdef IRONSIGHT
	void					SetScopeStencilMaskMode( bool bEnabled ) { m_bScopeStencilMaskModeEnabled = bEnabled; }
	bool					GetScopeStencilMaskMode( void ) { return m_bScopeStencilMaskModeEnabled; }
#endif

private:

#ifdef IRONSIGHT
	bool					m_bScopeStencilMaskModeEnabled;
	CHandle< C_ViewmodelAttachmentModel > m_viewmodelScopeStencilMask;
#endif

	CUtlVector< CHandle< C_ViewmodelAttachmentModel > > m_vecViewmodelArmModels; // gloves, sleeves, etc
	CHandle< C_ViewmodelAttachmentModel > m_viewmodelStatTrakAddon;
	CHandle< C_ViewmodelAttachmentModel > m_viewmodelUidAddon;
	int						m_iAddOnPlayerClass;
	int						m_iAddOnWeaponID;
	float					m_flStatTrakGlowMultiplierIdeal;
	float					m_flStatTrakGlowMultiplier;

	//stickers
	typedef CHandle<C_ViewmodelAttachmentModel>	StickerHandle_t;
	CUtlVector<StickerHandle_t>	m_hStickerModelAddons;
	CBaseAnimating* m_pMaterialPreviewShape;

	char					m_szLastSound[64];
	float					m_flLastSoundTime;
	bool					IsSoundSameAsPreviousSound( const char* soundName, float flPastTimeThreshold ) { return ( gpGlobals->curtime - m_flLastSoundTime < flPastTimeThreshold ) && !V_strcmp( soundName, m_szLastSound ); }
	void					ResetTimeSincePreviousSound( void ) { m_flLastSoundTime = gpGlobals->curtime; }
	void					SetPreviousSoundStr( const char* soundName ) { V_strcpy( m_szLastSound, soundName ); }
		
#endif

	// Control panel
	typedef CHandle<CVGuiScreen>	ScreenHandle_t;
	CUtlVector<ScreenHandle_t>	m_hScreens;
};

inline CBaseViewModel *ToBaseViewModel( CBaseAnimating *pAnim )
{
	if ( pAnim && pAnim->IsViewModel() )
		return assert_cast<CBaseViewModel *>(pAnim);
	return NULL;
}

inline CBaseViewModel *ToBaseViewModel( CBaseEntity *pEntity )
{
	if ( !pEntity )
		return NULL;
	return ToBaseViewModel(pEntity->GetBaseAnimating());
}

#endif // BASEVIEWMODEL_SHARED_H
