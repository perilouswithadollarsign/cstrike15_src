//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_basetempentity.h"
#include "c_cs_player.h"
#include "radio_status.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Kills Player Attachments
//-----------------------------------------------------------------------------
class C_TERadioIcon : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TERadioIcon, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TERadioIcon( void );
	virtual			~C_TERadioIcon( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	int m_iAttachToClient;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TERadioIcon::C_TERadioIcon( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TERadioIcon::~C_TERadioIcon( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TERadioIcon::PostDataUpdate( DataUpdateType_t updateType )
{
	//Flash them on the radar
	//this could be in a better place.
	C_CSPlayer *pPlayer = static_cast<C_CSPlayer*>( cl_entitylist->GetEnt(m_iAttachToClient) );

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// Create the flashy above player's head
		RadioManager()->UpdateRadioStatus( m_iAttachToClient, 1.5f );
	}
	
}

IMPLEMENT_CLIENTCLASS_EVENT_DT(C_TERadioIcon, DT_TERadioIcon, CTERadioIcon)
	RecvPropInt( RECVINFO(m_iAttachToClient)),
END_RECV_TABLE()
