//========= Copyright � 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#ifndef ECON_ENTITY_H
#define ECON_ENTITY_H
#ifdef _WIN32
#pragma once
#endif

#include <utlsortvector.h>
#include <utlhashtable.h>
#include "ihasattributes.h"
#include "ihasowner.h"
#include "attribute_manager.h"

#ifdef DOTA_DLL
#include "dota_econ_item_string_table.h"
#endif

#if defined( CLIENT_DLL )
#define CEconEntity				C_EconEntity
#define CBaseAttributableItem	C_BaseAttributableItem
#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEconEntity : public CBaseFlex, public IHasAttributes
{
	DECLARE_CLASS( CEconEntity, CBaseFlex );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();
	CEconEntity();
	~CEconEntity();

	void					InitializeAttributes( void );
	void					DebugDescribe( void );
	Activity				TranslateViewmodelHandActivity( Activity actBase );
	virtual void			UpdateOnRemove( void );

	virtual CStudioHdr *	OnNewModel();

	bool					HasCustomParticleSystems( void );

#if !defined( CLIENT_DLL )
	virtual void 			GiveTo( CBaseEntity *pOther ) {}
	void					OnOwnerClassChange( void );
	void					UpdateModelToClass( void );
	void					PlayAnimForPlaybackEvent( wearableanimplayback_t iPlayback );
	virtual int				CalculateVisibleClassFor( CBaseCombatCharacter *pPlayer );
	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );

#else
	virtual void			SetDormant( bool bDormant );
	virtual void			OnPreDataChanged( DataUpdateType_t type );
	virtual void			OnDataChanged( DataUpdateType_t updateType );
	virtual bool			ShouldShowToolTip( void	) { return true; }
	virtual bool			InitializeAsClientEntity( const char *pszModelName, bool bRenderWithViewModels );
	virtual int				InternalDrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool			OnInternalDrawModel( ClientModelRenderInfo_t *pInfo );
	virtual void			FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );
	virtual bool			OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
	bool					InternalFireEvent( const Vector& origin, const QAngle& angles, int event, const char *options );

	// Custom flex controllers
	virtual bool			UsesFlexDelayedWeights( void );
	virtual	void			SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights );
	float					m_flFlexDelayTime;
	float *					m_flFlexDelayedWeight;

	// Custom particle attachments
	void					UpdateParticleSystems( void );
	virtual bool			ShouldDrawParticleSystems( void );
	void					SetParticleSystemsVisible( bool bVisible );
	void					UpdateSingleParticleSystem( bool bVisible, attachedparticlesystem_t *pSystem, const char *pszAttachmentName = NULL );
	virtual void			UpdateAttachmentModels( void );
	virtual bool			AttachmentModelsShouldBeVisible( void ) { return true; }

	// Viewmodel overriding
	virtual bool			ViewModel_IsTransparent( void );
	virtual bool			ViewModel_IsUsingFBTexture( void );
	virtual bool			IsOverridingViewmodel( void );
	virtual int				DrawOverriddenViewmodel( C_BaseViewModel *pViewmodel, int flags );

	// Attachments
	bool					WantsToOverrideViewmodelAttachments( void ) { return (m_hViewmodelAttachment != nullptr); }
	virtual int				LookupAttachment( const char *pAttachmentName );
	virtual bool			GetAttachment( const char *szName, Vector &absOrigin ) { return BaseClass::GetAttachment(szName,absOrigin); }
	virtual bool			GetAttachment( const char *szName, Vector &absOrigin, QAngle &absAngles ) { return BaseClass::GetAttachment(szName,absOrigin,absAngles); }
	virtual bool			GetAttachment( int number, matrix3x4_t &matrix );
	virtual bool			GetAttachment( int number, Vector &origin );
	virtual	bool			GetAttachment( int number, Vector &origin, QAngle &angles );
	virtual bool			GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel );

	C_BaseAnimating			*GetViewmodelAttachment( void ) { return m_hViewmodelAttachment.Get(); }
	virtual void			ViewModelAttachmentBlending( CStudioHdr *hdr, Vector pos[], Quaternion q[], float currentTime, int boneMask ) {}

	void					SetWaitingToLoad( bool bWaiting );

	virtual bool			ValidateEntityAttachedToPlayer( bool &bShouldRetry );

	virtual void			SetMaterialOverride( const char *pszMaterial );
	virtual void			SetMaterialOverride( CMaterialReference &ref );


	bool					m_bAttributesInitialized;

#endif

public:
	// IHasAttributes
	CAttributeManager		*GetAttributeManager( void ) { return m_AttributeManager.Get(); }
	CAttributeContainer		*GetAttributeContainer( void ) { return m_AttributeManager.Get(); }
	const CAttributeContainer		*GetAttributeContainer( void ) const { return m_AttributeManager.Get(); }
	CBaseEntity				*GetAttributeOwner( void ) { return GetOwnerEntity(); }
	CAttributeList			*GetAttributeList( void ) { return m_AttributeManager.GetItem()->GetAttributeList(); }

	loadout_positions_t		GetLoadoutPosition( int iTeam = 0 ) const;
	const CEconItemView*	GetEconItemView( void ) const;

	virtual void SetOriginalOwnerXuid( uint32 nLow, uint32 nHigh ) { m_OriginalOwnerXuidLow = nLow; m_OriginalOwnerXuidHigh = nHigh; }
	virtual uint64 GetOriginalOwnerXuid( void ) const;

	// Obsolete, but used for demo backward compat
	virtual int GetFallbackPaintKit( void ) const { return m_nFallbackPaintKit; }
	virtual int GetFallbackSeed( void ) const { return m_nFallbackSeed; }
	virtual int GetFallbackWear( void ) const { return m_flFallbackWear; }
	virtual int GetFallbackStatTrak( void ) const { return m_nFallbackStatTrak; }

	virtual void			ReapplyProvision( void );

	virtual bool			UpdateBodygroups( CBaseCombatCharacter* pOwner, int iState );


protected:
	virtual Activity		TranslateViewmodelHandActivityInternal( Activity actBase ) { return actBase; }

protected:
	CNetworkVarEmbedded(	CAttributeContainer, m_AttributeManager );

	CNetworkVar( uint32, m_OriginalOwnerXuidLow );
	CNetworkVar( uint32, m_OriginalOwnerXuidHigh );
	
	// Obsolete, but used for demo backward compat
	CNetworkVar( int, m_nFallbackPaintKit );
	CNetworkVar( int, m_nFallbackSeed );
	CNetworkVar( float, m_flFallbackWear );
	CNetworkVar( int, m_nFallbackStatTrak );

#ifdef CLIENT_DLL
	bool					m_bClientside;
	bool					m_bParticleSystemsCreated;
	CUtlVector<int>			m_vecAttachedParticles;
	CMaterialReference		m_MaterialOverrides;
	CHandle<C_BaseAnimating>	m_hViewmodelAttachment;
	int						m_iOldTeam;
	bool					m_bAttachmentDirty;
	int						m_nUnloadedModelIndex;
	int						m_iNumOwnerValidationRetries;
#endif


	EHANDLE					m_hOldProvidee;

#ifdef GAME_DLL
	int						m_iOldOwnerClass; // Used to detect class changes on items that have per-class models
#endif

protected:
#ifdef CLIENT_DLL

public:
	// Additional attachments.
	struct AttachedModelData_t
	{
		const model_t *m_pModel;
		int m_iModelDisplayFlags;
	};

	CUtlVector<AttachedModelData_t> m_vecAttachedModels;
#endif // CLIENT_DLL
};

#define ITEM_PICKUP_BOX_BLOAT		24

#define VIEWMODEL_CLASS_RESTRICTION_DEFAULT_NONE -1

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CBaseAttributableItem : public CEconEntity
{
	DECLARE_CLASS( CBaseAttributableItem, CBaseAnimating );
public:
	DECLARE_NETWORKCLASS();
	DECLARE_DATADESC();

	CBaseAttributableItem();
};

#endif // ECON_ENTITY_H
