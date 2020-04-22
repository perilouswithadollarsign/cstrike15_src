//===== Copyright © Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//================================================================//

#include "cbase.h"
#include "c_props.h"
#include "functionproxy.h"
#include "imaterialproxydict.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//
// Floor Button
//

class C_PropFloorButton : public C_DynamicProp
{
public:
	DECLARE_CLASS( C_PropFloorButton, C_DynamicProp );
	DECLARE_CLIENTCLASS();

	bool IsPressed();

	virtual void Spawn( void )
	{
		BaseClass::Spawn();
	}

private:
	CNetworkVar( bool, m_bButtonState );
};


bool C_PropFloorButton::IsPressed()
{
	return m_bButtonState;
}


//--------------------------------------------------------------------------------------------------------
class CLightedFloorButtonProxy : public CResultProxy
{
public:
	void OnBind( void *pC_BaseEntity );
};

//--------------------------------------------------------------------------------------------------------
void CLightedFloorButtonProxy::OnBind( void *pC_BaseEntity )
{
	if (!pC_BaseEntity)
		return;

	Assert( m_pResult );

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	C_PropFloorButton *button = dynamic_cast< C_PropFloorButton * >( pEntity );

	if ( button )
	{
		if ( button->IsPressed() )
		{
			SetFloatResult( 1.0f );
		}
		else
		{
			SetFloatResult( 1.0f );
		}
	}
	else
	{
		SetFloatResult( 0.0f );
	}
}

//--------------------------------------------------------------------------------------------------------
EXPOSE_MATERIAL_PROXY( CLightedFloorButtonProxy, LightedFloorButton );

//--------------------------------------------------------------------------------------------------------
void RecvProxy_ButtonStateChange( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	bool state = *((bool *)&pData->m_Value.m_Int);

	*(bool *)pOut = state;
}

IMPLEMENT_CLIENTCLASS_DT( C_PropFloorButton, DT_PropFloorButton, CPropFloorButton )

RecvPropInt( RECVINFO( m_bButtonState ), 0, RecvProxy_ButtonStateChange ),

END_RECV_TABLE()