//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/IMaterialVar.h"
#include "materialsystem/IMaterial.h"
#include "portalrenderable_flatbasic.h"
#include "c_prop_portal.h"
#include <KeyValues.h>

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPortalStaticProxy : public IMaterialProxy
{
protected:
	IMaterialVar *m_StaticOutput;
public:
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pBind );
	virtual void Release( void ) { delete this; }

	virtual IMaterial *	GetMaterial() { return ( m_StaticOutput ) ? m_StaticOutput->GetOwningMaterial() : NULL; }
};

bool CPortalStaticProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pszResultVar = pKeyValues->GetString( "resultVar" );
	if( !pszResultVar )
		return false;

	bool foundVar;
	m_StaticOutput = pMaterial->FindVar( pszResultVar, &foundVar, false );
	if( !foundVar )
		return false;

	if ( !Q_stricmp( pszResultVar, "$alpha" ) )
	{
		pMaterial->SetMaterialVarFlag( MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY, true );
	}

	return true;
}

void CPortalStaticProxy::OnBind( void *pBind )
{
	if ( pBind == NULL )
		return;
	
	float flStaticAmount;
	IClientRenderable *pRenderable = (IClientRenderable*)pBind;
	CPortalRenderable *pRecordedPortal = g_pPortalRender->FindRecordedPortal( pRenderable );

	if ( pRecordedPortal )
	{
		C_Prop_Portal *pRecordedFlatBasic = dynamic_cast<C_Prop_Portal *>(pRecordedPortal);
		if ( !pRecordedFlatBasic )
			return;

		flStaticAmount = pRecordedFlatBasic->ComputeStaticAmountForRendering();
	}
	else
	{
		C_Prop_Portal *pFlatBasic = dynamic_cast<C_Prop_Portal*>( pRenderable );
		if ( !pFlatBasic )
			return;

		flStaticAmount = pFlatBasic->ComputeStaticAmountForRendering();
	}

	m_StaticOutput->SetFloatValue( flStaticAmount );
}

EXPOSE_MATERIAL_PROXY( CPortalStaticProxy, PortalStaticModel );


class CPortalStaticPortalProxy : public CPortalStaticProxy
{
public:
	virtual void OnBind( void *pBind );
};

void CPortalStaticPortalProxy::OnBind( void *pBind )
{
	if ( pBind == NULL )
		return;

	IClientRenderable *pRenderable = (IClientRenderable*)( pBind );
	C_Prop_Portal *pPortal = (C_Prop_Portal *)pRenderable;

	float flStaticAmount = pPortal->ComputeStaticAmountForRendering();
	m_StaticOutput->SetFloatValue( flStaticAmount );
}

EXPOSE_MATERIAL_PROXY( CPortalStaticPortalProxy, PortalStatic );


class CPortalOpenAmountProxy : public CPortalStaticProxy
{
public:
	virtual void OnBind( void *pBind );
};

void CPortalOpenAmountProxy::OnBind( void *pBind )
{
	if ( pBind == NULL )
		return;

	IClientRenderable *pRenderable = (IClientRenderable*)( pBind );
	C_Prop_Portal *pPortal = (C_Prop_Portal *)pRenderable;

	m_StaticOutput->SetFloatValue( pPortal->m_fOpenAmount );
}

EXPOSE_MATERIAL_PROXY( CPortalOpenAmountProxy, PortalOpenAmount );


