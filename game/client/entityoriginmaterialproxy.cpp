//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: A base class for all material proxies in the client dll
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
// identifier was truncated to '255' characters in the debug information
//#pragma warning(disable: 4786)

#include "proxyentity.h"
#include "materialsystem/imaterialvar.h"
#include "imaterialproxydict.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#include "c_cs_player.h" // for grenades

class CEntityOriginMaterialProxy : public CEntityMaterialProxy
{
public:
	CEntityOriginMaterialProxy()
	{
		m_pMaterial = NULL;
		m_pOriginVar = NULL;
	}
	virtual ~CEntityOriginMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;
		bool found;
		m_pOriginVar = m_pMaterial->FindVar( "$entityorigin", &found );
		if( !found )
		{
			m_pOriginVar = NULL;
			return false;
		}
		return true;
	}
	virtual void OnBind( C_BaseEntity *pC_BaseEntity )
	{
		const Vector &origin = pC_BaseEntity->GetAbsOrigin();
		m_pOriginVar->SetVecValue( origin.x, origin.y, origin.z );
	}

	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pOriginVar;
};

EXPOSE_MATERIAL_PROXY( CEntityOriginMaterialProxy, EntityOrigin );

/*
extern CUtlVector<EHANDLE> g_SmokeGrenadeHandles;

#define SMOKE_PROXY_RADIUS_SQUARED 30000 //approx smoke radius ^2
ConVar cl_debug_smoke_proxy( "cl_debug_smoke_proxy", "0", FCVAR_CHEAT );
class CSmokeOriginMaterialProxy : public CEntityMaterialProxy
{
public:
	CSmokeOriginMaterialProxy()
	{
		m_pMaterial = NULL;
		m_pOriginVar = NULL;

	}
	virtual ~CSmokeOriginMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;
		bool found;
		m_pOriginVar = m_pMaterial->FindVar( "$BlendWithSmokeGrenadePosEntity", &found );
		if( !found )
		{
			m_pOriginVar = NULL;
			return false;
		}
		m_pSmokeOriginVar = m_pMaterial->FindVar( "$BlendWithSmokeGrenadePosSmoke", &found );
		if( !found )
		{
			m_pSmokeOriginVar = NULL;
			return false;
		}
		return true;
	}
	virtual void OnBind( C_BaseEntity *pC_BaseEntity )
	{
		if ( !g_SmokeGrenadeHandles.Count() )
		{
			m_pSmokeOriginVar->SetVecValue( 0, 0, 0 );
			return;
		}

		C_BaseAnimating *pBaseAnimating = pC_BaseEntity ? pC_BaseEntity->GetBaseAnimating() : NULL;
		if ( pBaseAnimating )
		{
			CStudioHdr *pHdr = pBaseAnimating->GetModelPtr();
			if ( pHdr )
			{
				matrix3x4_t matTemp;
				AngleMatrix( pC_BaseEntity->GetAbsAngles(), matTemp );

				Vector vecWorldRelativeIllumPos;
				VectorRotate( pHdr->illumposition(), matTemp, vecWorldRelativeIllumPos );

				Vector origin = pC_BaseEntity->GetAbsOrigin() + vecWorldRelativeIllumPos;
				m_pOriginVar->SetVecValue( origin.x, origin.y, origin.z );

				Vector vecClosestGrenadePos;
				vecClosestGrenadePos.Init();

				//find closest smoke grenade
				float flClosestDist = SMOKE_PROXY_RADIUS_SQUARED;
				for ( int i = 0; i < g_SmokeGrenadeHandles.Count(); i++ )
				{
					CBaseCSGrenadeProjectile *pGrenade = static_cast< CBaseCSGrenadeProjectile* >( g_SmokeGrenadeHandles[i].Get() );

					if ( !pGrenade )
						continue;

					float flTemp = pGrenade->GetAbsOrigin().DistToSqr( origin - Vector(0,0,69) );
					if ( flTemp < flClosestDist )
					{
						flClosestDist = flTemp;
						vecClosestGrenadePos = pGrenade->GetAbsOrigin() + Vector(0,0,69);
					}
				}

				m_pSmokeOriginVar->SetVecValue( vecClosestGrenadePos.x, vecClosestGrenadePos.y, vecClosestGrenadePos.z );

				if ( cl_debug_smoke_proxy.GetBool() )
				{
					debugoverlay->AddBoxOverlay( origin, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 255,0,0,0, 0 );
					if ( flClosestDist < SMOKE_PROXY_RADIUS_SQUARED )
					{
						debugoverlay->AddLineOverlay( origin, vecClosestGrenadePos, 255,0,0, true, 0 );
					}
				}
			}
		}
	}

	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pOriginVar;
	IMaterialVar *m_pSmokeOriginVar;
};

EXPOSE_MATERIAL_PROXY( CSmokeOriginMaterialProxy, SmokeOrigin );
*/

//=================================================================================================================
// This is a last-minute hack to ship Orange Box on the 360!
//=================================================================================================================
class CEntityOriginAlyxMaterialProxy : public CEntityMaterialProxy
{
public:
	CEntityOriginAlyxMaterialProxy()
	{
		m_pMaterial = NULL;
		m_pOriginVar = NULL;
	}
	virtual ~CEntityOriginAlyxMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;
		bool found;
		m_pOriginVar = m_pMaterial->FindVar( "$entityorigin", &found );
		if( !found )
		{
			m_pOriginVar = NULL;
			return false;
		}
		return true;
	}
	virtual void OnBind( C_BaseEntity *pC_BaseEntity )
	{
		const Vector &origin = pC_BaseEntity->GetAbsOrigin();
		m_pOriginVar->SetVecValue( origin.x - 15.0f, origin.y, origin.z );
	}

	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pOriginVar;
};

EXPOSE_MATERIAL_PROXY( CEntityOriginAlyxMaterialProxy, EntityOriginAlyx );

//=================================================================================================================
// This is a last-minute hack to ship Orange Box on the 360!
//=================================================================================================================
class CEp1IntroVortRefractMaterialProxy : public CEntityMaterialProxy
{
public:
	CEp1IntroVortRefractMaterialProxy()
	{
		m_pMaterial = NULL;
		m_pOriginVar = NULL;
	}
	virtual ~CEp1IntroVortRefractMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;
		bool found;
		m_pOriginVar = m_pMaterial->FindVar( "$refractamount", &found );
		if( !found )
		{
			m_pOriginVar = NULL;
			return false;
		}
		return true;
	}
	virtual void OnBind( C_BaseEntity *pC_BaseEntity )
	{
		if ( m_pOriginVar != NULL)
		{
			float flTmp = ( 1.0f - m_pOriginVar->GetFloatValue() );
			flTmp *= flTmp;
			flTmp *= flTmp;
			flTmp = ( 1.0f - flTmp ) * 0.25f;
			m_pOriginVar->SetFloatValue( flTmp );
		}
	}

	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pOriginVar;
};

EXPOSE_MATERIAL_PROXY( CEp1IntroVortRefractMaterialProxy, Ep1IntroVortRefract );
