//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#include <keyvalues.h>
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystem.h"
#include "functionproxy.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "predicted_viewmodel.h"
#include "cs_client_gamestats.h"
#include "econ/econ_item_schema.h"
#include "cstrike15_gcconstants.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Returns the proximity of the player to the entity
//-----------------------------------------------------------------------------

class CPlayerProximityProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerProximityProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	m_Factor = pKeyValues->GetFloat( "scale", 0.002 );
	return true;
}

void CPlayerProximityProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	// Find the distance between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Vector delta;
	VectorSubtract( pEntity->WorldSpaceCenter(), pPlayer->WorldSpaceCenter(), delta );

	Assert( m_pResult );
	SetFloatResult( delta.Length() * m_Factor );
}

EXPOSE_MATERIAL_PROXY( CPlayerProximityProxy, PlayerProximity );


//-----------------------------------------------------------------------------
// Returns true if the player's team matches that of the entity the proxy material is attached to
//-----------------------------------------------------------------------------

class CPlayerTeamMatchProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
};

bool CPlayerTeamMatchProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	return true;
}

void CPlayerTeamMatchProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	// Find the distance between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Assert( m_pResult );
	SetFloatResult( (pEntity->GetTeamNumber() == pPlayer->GetTeamNumber()) ? 1.0 : 0.0 );
}

EXPOSE_MATERIAL_PROXY( CPlayerTeamMatchProxy, PlayerTeamMatch );


//-----------------------------------------------------------------------------
// Returns the player view direction
//-----------------------------------------------------------------------------
class CPlayerViewProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerViewProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	m_Factor = pKeyValues->GetFloat( "scale", 2 );
	return true;
}

void CPlayerViewProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	// Find the view angle between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Vector delta;
	VectorSubtract( pEntity->WorldSpaceCenter(), pPlayer->WorldSpaceCenter(), delta );
	VectorNormalize( delta );

	Vector forward;
	AngleVectors( pPlayer->GetAbsAngles(), &forward );

	Assert( m_pResult );
	SetFloatResult( DotProduct( forward, delta ) * m_Factor );
}

EXPOSE_MATERIAL_PROXY( CPlayerViewProxy, PlayerView );


//-----------------------------------------------------------------------------
// Returns the player speed
//-----------------------------------------------------------------------------
class CPlayerSpeedProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerSpeedProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	m_Factor = pKeyValues->GetFloat( "scale", 0.005 );
	return true;
}

void CPlayerSpeedProxy::OnBind( void *pC_BaseEntity )
{
	// Find the player speed....
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	Assert( m_pResult );
	SetFloatResult( pPlayer->GetLocalVelocity().Length() * m_Factor );
}

EXPOSE_MATERIAL_PROXY( CPlayerSpeedProxy, PlayerSpeed );


//-----------------------------------------------------------------------------
// Returns the player position
//-----------------------------------------------------------------------------
class CPlayerPositionProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	float	m_Factor;
};

bool CPlayerPositionProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;
	  
	m_Factor = pKeyValues->GetFloat( "scale", 0.005 );
	return true;
}

void CPlayerPositionProxy::OnBind( void *pC_BaseEntity )
{
	// Find the player speed....
	C_BaseEntity* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	// This is actually a vector...
	Assert( m_pResult );
	Vector res;
	VectorMultiply( pPlayer->WorldSpaceCenter(), m_Factor, res ); 
	m_pResult->SetVecValue( res.Base(), 3 );
}

EXPOSE_MATERIAL_PROXY( CPlayerPositionProxy, PlayerPosition );


//-----------------------------------------------------------------------------
// Returns the entity speed
//-----------------------------------------------------------------------------
class CEntitySpeedProxy : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity );
};

void CEntitySpeedProxy::OnBind( void *pC_BaseEntity )
{
	// Find the view angle between the player and this entity....
	if (!pC_BaseEntity)
		return;

	// Find the view angle between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	Assert( m_pResult );
	m_pResult->SetFloatValue( pEntity->GetLocalVelocity().Length() );
}

EXPOSE_MATERIAL_PROXY( CEntitySpeedProxy, EntitySpeed );


//-----------------------------------------------------------------------------
// Returns a random # from 0 - 1 specific to the entity it's applied to
//-----------------------------------------------------------------------------
class CEntityRandomProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_Factor;
};

bool CEntityRandomProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_Factor.Init( pMaterial, pKeyValues, "scale", 1 ))
		return false;

	return true;
}

void CEntityRandomProxy::OnBind( void *pC_BaseEntity )
{
	// Find the view angle between the player and this entity....
	if (!pC_BaseEntity)
		return;

	// Find the view angle between the player and this entity....
	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	Assert( m_pResult );
	m_pResult->SetFloatValue( pEntity->ProxyRandomValue() * m_Factor.GetFloat() );
}

EXPOSE_MATERIAL_PROXY( CEntityRandomProxy, EntityRandom );

#include "utlrbtree.h"

//-----------------------------------------------------------------------------
// StatTrak 'kill odometer' support: given a numerical value expressed as a string, pick a texture frame to represent a given digit
//-----------------------------------------------------------------------------
class CStatTrakDigitProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

	virtual bool HelperOnBindGetStatTrakScore( void *pC_BaseEntity, int *piScore );

private:
	CFloatInput	m_flDisplayDigit; // the particular digit we want to display
	CFloatInput	m_flTrimZeros;
};


bool CStatTrakDigitProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_flDisplayDigit.Init( pMaterial, pKeyValues, "displayDigit", 0 ))
		return false;

	if (!m_flTrimZeros.Init( pMaterial, pKeyValues, "trimZeros", 0 ))
		return false;

	return true;
}

#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "predicted_viewmodel.h"

bool CStatTrakDigitProxy::HelperOnBindGetStatTrakScore( void *pC_BaseEntity, int *piScore )
{
	if ( !pC_BaseEntity )
		return false;

	if ( !piScore )
		return false;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		// StatTrak modules are children of their accompanying viewmodels
		C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel* >( pEntity->GetMoveParent() );
		if ( pViewModel )
		{
			C_CSPlayer *pPlayer = ToCSPlayer( pViewModel->GetPredictionOwner() );
			if ( pPlayer )
			{
				CWeaponCSBase *pWeap = pPlayer->GetActiveCSWeapon();
				if ( pWeap )
				{
					if ( CEconItemView *pItemView = pWeap->GetEconItemView() )
					{
						// Always get headshot-trak(TM)
						*piScore = pItemView->GetKillEaterValueByType( 0 );
					}
				}
			}
		}
	}
	return true;
}

void CStatTrakDigitProxy::OnBind( void *pC_BaseEntity )
{
	int nKillEaterAltScore = 0;
	bool bHasScoreToDisplay = HelperOnBindGetStatTrakScore( pC_BaseEntity, &nKillEaterAltScore );
	if ( !bHasScoreToDisplay )
	{	// Force flashing numbers
		SetFloatResult( (int) fmod( gpGlobals->curtime, 10.0f ) );
		return;
	}

	int iDesiredDigit = (int)m_flDisplayDigit.GetFloat();

	// trim preceding zeros
	if ( m_flTrimZeros.GetFloat() > 0 )
	{
		if ( pow( 10.0f, iDesiredDigit ) > nKillEaterAltScore )
		{
			SetFloatResult( 10.0f ); //assumed blank frame
			return;
		}
	}

	// get the [0-9] value of the digit we want
	int iDigitCount = MIN( iDesiredDigit, 10 );
	for ( int i=0; i<iDigitCount; i++ )
	{
		nKillEaterAltScore /= 10;
	}
	nKillEaterAltScore %= 10;

	SetFloatResult( nKillEaterAltScore );
}

EXPOSE_MATERIAL_PROXY( CStatTrakDigitProxy, StatTrakDigit );


//-----------------------------------------------------------------------------
// StatTrak 'kill odometer' support: given a numerical value expressed as a string, pick a texture frame to represent a given digit
//-----------------------------------------------------------------------------
class CStatTrakDigitProxyForModelWeaponPreviewPanel : public CStatTrakDigitProxy
{
public:
	virtual bool HelperOnBindGetStatTrakScore( void *pC_BaseEntity, int *puiScore ) OVERRIDE
	{
		/* Removed for partner depot */
		return false;
	}
};
EXPOSE_MATERIAL_PROXY( CStatTrakDigitProxyForModelWeaponPreviewPanel, StatTrakDigitForModelWeaponPreview );

#ifdef IRONSIGHT
//-----------------------------------------------------------------------------
// IronSightAmount proxy
//-----------------------------------------------------------------------------
class CIronSightAmountProxy : public CResultProxy
{
public:
	virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
	virtual void OnBind(void *pC_BaseEntity);
private:
	bool bInvert;
};


bool CIronSightAmountProxy::Init(IMaterial *pMaterial, KeyValues *pKeyValues)
{
	if (!CResultProxy::Init(pMaterial, pKeyValues))
		return false;

	bInvert = false;
	CFloatInput	m_flInvert;
	if ( m_flInvert.Init( pMaterial, pKeyValues, "invert" ) )
		bInvert = ( m_flInvert.GetFloat() > 0 );

	return true;
}

void CIronSightAmountProxy::OnBind(void *pC_BaseEntity)
{

	if (!pC_BaseEntity)
		return;
	
	C_BaseEntity *pEntity = BindArgToEntity(pC_BaseEntity);
	if (pEntity)
	{
		C_BaseViewModel *pViewModel = dynamic_cast<C_BaseViewModel*>(pEntity);
		if (pViewModel)
		{
			C_CSPlayer *pPlayer = ToCSPlayer(pViewModel->GetPredictionOwner());
			if (pPlayer)
			{
				CWeaponCSBase *pWeapon = pPlayer->GetActiveCSWeapon();
				if ( pWeapon && pWeapon->GetIronSightController() )
				{
					if ( bInvert )
					{
						SetFloatResult(Bias( 1.0f - pWeapon->GetIronSightController()->GetIronSightAmount(), 0.2f));
					}
					else
					{
						SetFloatResult(Bias(pWeapon->GetIronSightController()->GetIronSightAmount(), 0.2f));
					}
				}
			}
		}
	}

}


EXPOSE_MATERIAL_PROXY(CIronSightAmountProxy, IronSightAmount);
#endif //IRONSIGHT

//-----------------------------------------------------------------------------
// StatTrakIllum proxy
//-----------------------------------------------------------------------------
class CStatTrakIllumProxy : public CResultProxy
{
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );

private:
	CFloatInput	m_flMinVal;
	CFloatInput	m_flMaxVal;
};


bool CStatTrakIllumProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if (!CResultProxy::Init( pMaterial, pKeyValues ))
		return false;

	if (!m_flMinVal.Init( pMaterial, pKeyValues, "minVal", 0.5 ))
		return false;

	if (!m_flMaxVal.Init( pMaterial, pKeyValues, "maxVal", 1 ))
		return false;

	return true;
}

void CStatTrakIllumProxy::OnBind( void *pC_BaseEntity )
{

	if (!pC_BaseEntity)
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		// StatTrak modules are children of their accompanying viewmodels
		C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel* >( pEntity->GetMoveParent() );
		if ( pViewModel )
		{
			SetFloatResult( Lerp( pViewModel->GetStatTrakGlowMultiplier(), m_flMinVal.GetFloat(), m_flMaxVal.GetFloat() ) );
			return;
		}
	}

}


EXPOSE_MATERIAL_PROXY( CStatTrakIllumProxy, StatTrakIllum );


//-----------------------------------------------------------------------------
// WeaponLabelTextProxy
//-----------------------------------------------------------------------------
class CWeaponLabelTextProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
	virtual bool HelperOnBindGetLabel( void *pC_BaseEntity, const char **p_szLabel );

private:
	CFloatInput	m_flDisplayDigit;
	IMaterialVar *m_pTextureOffsetVar;
};

bool CWeaponLabelTextProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{

	if (!m_flDisplayDigit.Init( pMaterial, pKeyValues, "displayDigit", 0 ))
		return false;

	bool foundVar;
	m_pTextureOffsetVar = pMaterial->FindVar( "$basetexturetransform", &foundVar, false );
	if( !foundVar )
		return false;

	return true;
}

bool CWeaponLabelTextProxy::HelperOnBindGetLabel( void *pC_BaseEntity, const char **p_szLabel )
{
	if ( !pC_BaseEntity )
		return false;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );
	if ( pEntity )
	{
		// uid modules are children of their accompanying viewmodels
		C_BaseViewModel *pViewModel = dynamic_cast< C_BaseViewModel* >( pEntity->GetMoveParent() );
		if ( pViewModel )
		{
			CBaseCombatWeapon *pWeapon = pViewModel->GetWeapon();
			if ( pWeapon )
			{
				CEconItemView *pItem = pWeapon->GetEconItemView();
				if ( pItem )
				{
					*p_szLabel = pItem->GetCustomName();
					return true;
				}
			}
		}
	}

	return false;
}

void CWeaponLabelTextProxy::OnBind( void *pC_BaseEntity )
{

	const char *p_szLabel = NULL;
	bool bHasLabel = HelperOnBindGetLabel( pC_BaseEntity, &p_szLabel );
	if (!bHasLabel || !p_szLabel)
		return;

	//get the digit index we need to display
	int nDigit = (int)m_flDisplayDigit.GetFloat();

	//center the text within NUM_UID_CHARS
	int nStrLen = (int)strlen( p_szLabel );
	int nPrependSpaces = ( NUM_UID_CHARS - nStrLen ) / 2;
	nDigit -= nPrependSpaces;

	int nCharIndex = 0;
	if ( nDigit >= 0 && nDigit < nStrLen )
	{
		nCharIndex = p_szLabel[nDigit] - 32;
	}

	int nIndexHoriz = fmod( nCharIndex, 12.0f );
	int nIndexVertical = nCharIndex / 12;

	float flOffsetX = 0.083333f * nIndexHoriz;
	float flOffsetY =    0.125f * nIndexVertical;

	VMatrix mat( 1.0f,	0.0f,	0.0f,	flOffsetX,
		0.0f,	1.0f,	0.0f,	flOffsetY,
		0.0f,	0.0f,	1.0f,	0.0f,
		0.0f,	0.0f,	0.0f,	1.0f );

	m_pTextureOffsetVar->SetMatrixValue( mat );
}

EXPOSE_MATERIAL_PROXY( CWeaponLabelTextProxy, WeaponLabelText );


class CWeaponLabelTextProxyForModelWeaponPreviewPanel : public CWeaponLabelTextProxy
{
public:
	virtual bool HelperOnBindGetLabel( void *pC_BaseEntity, const char **p_szLabel )
	{
		/* Removed for partner depot */
		return false;
	}
};
EXPOSE_MATERIAL_PROXY( CWeaponLabelTextProxyForModelWeaponPreviewPanel, WeaponLabelTextPreview );


int g_HighlightedSticker = -1;
int g_PeelSticker = -1;

void CC_HighlightSticker(const CCommand& args)
{
	int nParam = atoi(args[1]);
	if ( nParam >= 0 && nParam <= 4 )
	{
		g_HighlightedSticker = nParam;
	}
}
static ConCommand highlight_sticker("highlight_sticker", CC_HighlightSticker, "", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );

void CC_PeelSticker(const CCommand& args)
{
	int nParam = atoi(args[1]);
	if (nParam >= 0 && nParam <= 4)
	{
		g_PeelSticker = nParam;
	}
}
static ConCommand peel_sticker("peel_sticker", CC_PeelSticker, "", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY);

//-----------------------------------------------------------------------------
// Sticker selection proxy
//-----------------------------------------------------------------------------
class CStickerSelectionProxy : public CResultProxy
{
public:
	virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
	virtual void OnBind(void *pC_BaseEntity);
	virtual void CheckMyGlobal( void );
	
	int m_nStickerIndex;
	float m_flSelectedness;
};

bool CStickerSelectionProxy::Init(IMaterial *pMaterial, KeyValues *pKeyValues)
{
	if (!CResultProxy::Init(pMaterial, pKeyValues))
		return false;

	CFloatInput flStickerIndex;
	if (!flStickerIndex.Init(pMaterial, pKeyValues, "stickerindex", 0))
		return false;
	m_nStickerIndex = (int)flStickerIndex.GetFloat();

	m_flSelectedness = 0.0f;

	return true;
}

void CStickerSelectionProxy::CheckMyGlobal( void )
{
	if (g_HighlightedSticker > -1 && m_nStickerIndex == g_HighlightedSticker)
	{
		m_flSelectedness = 1.0f;
		g_HighlightedSticker = -1;
	}
}

void CStickerSelectionProxy::OnBind(void *pC_BaseEntity)
{

	//if (!pC_BaseEntity)
	//	return;

	CheckMyGlobal();

	if ( m_flSelectedness > 0.01f )
	{
		m_flSelectedness = Approach( 0.0f, m_flSelectedness, gpGlobals->frametime * 0.5f );
		SetFloatResult( m_flSelectedness );
	}
	else if ( m_flSelectedness > 0.0f )
	{
		SetFloatResult( 0.0f );
	}

}

EXPOSE_MATERIAL_PROXY(CStickerSelectionProxy, StickerSelection);

class CStickerPeelProxy : public CStickerSelectionProxy
{
public:
	virtual void CheckMyGlobal( void );
};

void CStickerPeelProxy::CheckMyGlobal(void)
{
	if (g_PeelSticker > -1 && m_nStickerIndex == g_PeelSticker)
	{
		m_flSelectedness = 1.0f;
		g_PeelSticker = -1;
	}
}

EXPOSE_MATERIAL_PROXY(CStickerPeelProxy, StickerPeel);


//-----------------------------------------------------------------------------
// CrosshairColor proxy
//-----------------------------------------------------------------------------
extern ConVar cl_crosshaircolor_r;
extern ConVar cl_crosshaircolor_g;
extern ConVar cl_crosshaircolor_b;
class CCrossHairColorProxy : public CResultProxy
{
public:
	virtual bool Init(IMaterial *pMaterial, KeyValues *pKeyValues);
	virtual void OnBind(void *pC_BaseEntity);

	Vector m_vecLocalCrossHairColor;
};

bool CCrossHairColorProxy::Init(IMaterial *pMaterial, KeyValues *pKeyValues)
{
	if (!CResultProxy::Init(pMaterial, pKeyValues))
		return false;
	m_vecLocalCrossHairColor.Init();
	return true;
}

void CCrossHairColorProxy::OnBind(void *pC_BaseEntity)
{
	if ( m_vecLocalCrossHairColor.x != cl_crosshaircolor_r.GetFloat() || 
		 m_vecLocalCrossHairColor.y != cl_crosshaircolor_g.GetFloat() || 
		 m_vecLocalCrossHairColor.z != cl_crosshaircolor_b.GetFloat() )
	{

		m_vecLocalCrossHairColor.x = cl_crosshaircolor_r.GetFloat();
		m_vecLocalCrossHairColor.y = cl_crosshaircolor_g.GetFloat();
		m_vecLocalCrossHairColor.z = cl_crosshaircolor_b.GetFloat();

		SetVecResult(   (float)m_vecLocalCrossHairColor.x * 0.0039,
						(float)m_vecLocalCrossHairColor.y * 0.0039,
						(float)m_vecLocalCrossHairColor.z * 0.0039, 1);
	}
}

EXPOSE_MATERIAL_PROXY(CCrossHairColorProxy, CrossHairColor);


float g_flEconInspectPreviewTime = 0;

class CEconInspectPreviewTimeProxy : public CResultProxy
{
public:
	bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	void OnBind( void *pC_BaseEntity );
};

bool CEconInspectPreviewTimeProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	if ( !CResultProxy::Init( pMaterial, pKeyValues ) )
		return false;

	return true;
}

void CEconInspectPreviewTimeProxy::OnBind( void *pC_BaseEntity )
{
	Assert( m_pResult );
	SetFloatResult( gpGlobals->curtime - g_flEconInspectPreviewTime );
}

EXPOSE_MATERIAL_PROXY( CEconInspectPreviewTimeProxy, EconInspectPreviewTime );

