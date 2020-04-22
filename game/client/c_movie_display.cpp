//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//
//=====================================================================================//
#include "cbase.h"
#include "c_movie_display.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_MovieDisplay, DT_MovieDisplay, CMovieDisplay )
	RecvPropBool( RECVINFO( m_bEnabled ) ),
	RecvPropBool( RECVINFO( m_bLooping ) ),
	RecvPropString( RECVINFO( m_szMovieFilename ) ),
	RecvPropString( RECVINFO( m_szGroupName ) ),
	RecvPropBool( RECVINFO( m_bStretchToFill ) ),
	RecvPropBool( RECVINFO( m_bForcedSlave ) ),
	RecvPropBool( RECVINFO( m_bUseCustomUVs ) ),
	RecvPropFloat( RECVINFO( m_flUMin ) ),
	RecvPropFloat( RECVINFO( m_flUMax ) ),
	RecvPropFloat( RECVINFO( m_flVMin ) ),
	RecvPropFloat( RECVINFO( m_flVMax ) ),
END_RECV_TABLE()

C_MovieDisplay::C_MovieDisplay()
{
}

C_MovieDisplay::~C_MovieDisplay()
{
}

//-----------------------------------------------------------------------------
// Purpose: Recieve a message from the server
//-----------------------------------------------------------------------------
void C_MovieDisplay::ReceiveMessage( int classID, bf_read &msg )
{
	// Make sure our IDs match
	if ( classID != GetClientClass()->m_ClassID )
	{
		// Message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	// currently we only get one message - that we want to take over as the master
	m_bWantsToBeMaster = true;
}
