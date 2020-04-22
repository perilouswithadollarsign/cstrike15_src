//========= Copyright © 1996-2010, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CFuncInstanceIoProxy : public CBaseEntity
{
public:
	DECLARE_CLASS( CFuncInstanceIoProxy, CBaseEntity );

	// Input handlers
	void InputProxyRelay1( inputdata_t &inputdata );
	void InputProxyRelay2( inputdata_t &inputdata );
	void InputProxyRelay3( inputdata_t &inputdata );
	void InputProxyRelay4( inputdata_t &inputdata );
	void InputProxyRelay5( inputdata_t &inputdata );
	void InputProxyRelay6( inputdata_t &inputdata );
	void InputProxyRelay7( inputdata_t &inputdata );
	void InputProxyRelay8( inputdata_t &inputdata );
	void InputProxyRelay9( inputdata_t &inputdata );
	void InputProxyRelay10( inputdata_t &inputdata );
	void InputProxyRelay11( inputdata_t &inputdata );
	void InputProxyRelay12( inputdata_t &inputdata );
	void InputProxyRelay13( inputdata_t &inputdata );
	void InputProxyRelay14( inputdata_t &inputdata );
	void InputProxyRelay15( inputdata_t &inputdata );
	void InputProxyRelay16( inputdata_t &inputdata );
	void InputProxyRelay17( inputdata_t &inputdata );
	void InputProxyRelay18( inputdata_t &inputdata );
	void InputProxyRelay19( inputdata_t &inputdata );
	void InputProxyRelay20( inputdata_t &inputdata );
	void InputProxyRelay21( inputdata_t &inputdata );
	void InputProxyRelay22( inputdata_t &inputdata );
	void InputProxyRelay23( inputdata_t &inputdata );
	void InputProxyRelay24( inputdata_t &inputdata );
	void InputProxyRelay25( inputdata_t &inputdata );
	void InputProxyRelay26( inputdata_t &inputdata );
	void InputProxyRelay27( inputdata_t &inputdata );
	void InputProxyRelay28( inputdata_t &inputdata );
	void InputProxyRelay29( inputdata_t &inputdata );
	void InputProxyRelay30( inputdata_t &inputdata );
;

	DECLARE_DATADESC();

private:

	COutputEvent m_OnProxyRelay1;
	COutputEvent m_OnProxyRelay2;
	COutputEvent m_OnProxyRelay3;
	COutputEvent m_OnProxyRelay4;
	COutputEvent m_OnProxyRelay5;
	COutputEvent m_OnProxyRelay6;
	COutputEvent m_OnProxyRelay7;
	COutputEvent m_OnProxyRelay8;
	COutputEvent m_OnProxyRelay9;
	COutputEvent m_OnProxyRelay10;
	COutputEvent m_OnProxyRelay11;
	COutputEvent m_OnProxyRelay12;
	COutputEvent m_OnProxyRelay13;
	COutputEvent m_OnProxyRelay14;
	COutputEvent m_OnProxyRelay15;
	COutputEvent m_OnProxyRelay16;
	COutputEvent m_OnProxyRelay17;
	COutputEvent m_OnProxyRelay18;
	COutputEvent m_OnProxyRelay19;
	COutputEvent m_OnProxyRelay20;
	COutputEvent m_OnProxyRelay21;
	COutputEvent m_OnProxyRelay22;
	COutputEvent m_OnProxyRelay23;
	COutputEvent m_OnProxyRelay24;
	COutputEvent m_OnProxyRelay25;
	COutputEvent m_OnProxyRelay26;
	COutputEvent m_OnProxyRelay27;
	COutputEvent m_OnProxyRelay28;
	COutputEvent m_OnProxyRelay29;
	COutputEvent m_OnProxyRelay30;

};

LINK_ENTITY_TO_CLASS( func_instance_io_proxy, CFuncInstanceIoProxy );

BEGIN_DATADESC( CFuncInstanceIoProxy )

	// Inputs
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay1",  InputProxyRelay1 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay2",  InputProxyRelay2 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay3",  InputProxyRelay3 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay4",  InputProxyRelay4 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay5",  InputProxyRelay5 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay6",  InputProxyRelay6 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay7",  InputProxyRelay7 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay8",  InputProxyRelay8 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay9",  InputProxyRelay9 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay10",  InputProxyRelay10 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay11",  InputProxyRelay11 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay12",  InputProxyRelay12 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay13",  InputProxyRelay13 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay14",  InputProxyRelay14 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay15",  InputProxyRelay15 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay16",  InputProxyRelay16 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay17",  InputProxyRelay17 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay18",  InputProxyRelay18 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay19",  InputProxyRelay19 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay20",  InputProxyRelay20 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay21",  InputProxyRelay21 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay22",  InputProxyRelay22 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay23",  InputProxyRelay23 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay24",  InputProxyRelay24 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay25",  InputProxyRelay25 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay26",  InputProxyRelay26 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay27",  InputProxyRelay27 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay28",  InputProxyRelay28 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay29",  InputProxyRelay29 ),
	DEFINE_INPUTFUNC( FIELD_STRING, "OnProxyRelay30",  InputProxyRelay30 ),

	// Outputs
	DEFINE_OUTPUT( m_OnProxyRelay1, "OnProxyRelay1" ),
	DEFINE_OUTPUT( m_OnProxyRelay2, "OnProxyRelay2" ),
	DEFINE_OUTPUT( m_OnProxyRelay3, "OnProxyRelay3" ),
	DEFINE_OUTPUT( m_OnProxyRelay4, "OnProxyRelay4" ),
	DEFINE_OUTPUT( m_OnProxyRelay5, "OnProxyRelay5" ),
	DEFINE_OUTPUT( m_OnProxyRelay6, "OnProxyRelay6" ),
	DEFINE_OUTPUT( m_OnProxyRelay7, "OnProxyRelay7" ),
	DEFINE_OUTPUT( m_OnProxyRelay8, "OnProxyRelay8" ),
	DEFINE_OUTPUT( m_OnProxyRelay9, "OnProxyRelay9" ),
	DEFINE_OUTPUT( m_OnProxyRelay10, "OnProxyRelay10" ),
	DEFINE_OUTPUT( m_OnProxyRelay11, "OnProxyRelay11" ),
	DEFINE_OUTPUT( m_OnProxyRelay12, "OnProxyRelay12" ),
	DEFINE_OUTPUT( m_OnProxyRelay13, "OnProxyRelay13" ),
	DEFINE_OUTPUT( m_OnProxyRelay14, "OnProxyRelay14" ),
	DEFINE_OUTPUT( m_OnProxyRelay15, "OnProxyRelay15" ),
	DEFINE_OUTPUT( m_OnProxyRelay16, "OnProxyRelay16" ),	
	DEFINE_OUTPUT( m_OnProxyRelay16, "OnProxyRelay16" ),
	DEFINE_OUTPUT( m_OnProxyRelay17, "OnProxyRelay17" ),
	DEFINE_OUTPUT( m_OnProxyRelay18, "OnProxyRelay18" ),
	DEFINE_OUTPUT( m_OnProxyRelay19, "OnProxyRelay19" ),
	DEFINE_OUTPUT( m_OnProxyRelay20, "OnProxyRelay20" ),
	DEFINE_OUTPUT( m_OnProxyRelay21, "OnProxyRelay21" ),
	DEFINE_OUTPUT( m_OnProxyRelay22, "OnProxyRelay22" ),
	DEFINE_OUTPUT( m_OnProxyRelay23, "OnProxyRelay23" ),
	DEFINE_OUTPUT( m_OnProxyRelay24, "OnProxyRelay24" ),
	DEFINE_OUTPUT( m_OnProxyRelay25, "OnProxyRelay25" ),
	DEFINE_OUTPUT( m_OnProxyRelay26, "OnProxyRelay26" ),
	DEFINE_OUTPUT( m_OnProxyRelay27, "OnProxyRelay27" ),
	DEFINE_OUTPUT( m_OnProxyRelay28, "OnProxyRelay28" ),
	DEFINE_OUTPUT( m_OnProxyRelay29, "OnProxyRelay29" ),
	DEFINE_OUTPUT( m_OnProxyRelay30, "OnProxyRelay30" ),

END_DATADESC()

//------------------------------------------------------------------------------
// Purpose : Route the incomming to the outgoing proxy messages. 
//------------------------------------------------------------------------------
void CFuncInstanceIoProxy::InputProxyRelay1( inputdata_t &inputdata )
{
	m_OnProxyRelay1.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay2( inputdata_t &inputdata )
{
	m_OnProxyRelay2.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay3( inputdata_t &inputdata )
{
	m_OnProxyRelay3.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay4( inputdata_t &inputdata )
{
	m_OnProxyRelay4.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay5( inputdata_t &inputdata )
{
	m_OnProxyRelay5.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay6( inputdata_t &inputdata )
{
	m_OnProxyRelay6.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay7( inputdata_t &inputdata )
{
	m_OnProxyRelay7.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay8( inputdata_t &inputdata )
{
	m_OnProxyRelay8.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay9( inputdata_t &inputdata )
{
	m_OnProxyRelay9.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay10( inputdata_t &inputdata )
{
	m_OnProxyRelay10.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay11( inputdata_t &inputdata )
{
	m_OnProxyRelay11.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay12( inputdata_t &inputdata )
{
	m_OnProxyRelay12.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay13( inputdata_t &inputdata )
{
	m_OnProxyRelay13.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay14( inputdata_t &inputdata )
{
	m_OnProxyRelay14.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay15( inputdata_t &inputdata )
{
	m_OnProxyRelay15.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay16( inputdata_t &inputdata )
{
	m_OnProxyRelay16.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay17( inputdata_t &inputdata )
{
	m_OnProxyRelay17.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay18( inputdata_t &inputdata )
{
	m_OnProxyRelay18.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay19( inputdata_t &inputdata )
{
	m_OnProxyRelay19.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay20( inputdata_t &inputdata )
{
	m_OnProxyRelay20.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay21( inputdata_t &inputdata )
{
	m_OnProxyRelay21.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay22( inputdata_t &inputdata )
{
	m_OnProxyRelay22.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay23( inputdata_t &inputdata )
{
	m_OnProxyRelay23.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay24( inputdata_t &inputdata )
{
	m_OnProxyRelay24.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay25( inputdata_t &inputdata )
{
	m_OnProxyRelay25.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay26( inputdata_t &inputdata )
{
	m_OnProxyRelay26.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay27( inputdata_t &inputdata )
{
	m_OnProxyRelay27.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay28( inputdata_t &inputdata )
{
	m_OnProxyRelay28.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay29( inputdata_t &inputdata )
{
	m_OnProxyRelay29.FireOutput( inputdata.pActivator, inputdata.pCaller );
}

void CFuncInstanceIoProxy::InputProxyRelay30( inputdata_t &inputdata )
{
	m_OnProxyRelay30.FireOutput( inputdata.pActivator, inputdata.pCaller );
	DevWarning( "Maximun Proxy Messages used - ask a programmer for more.\n" );
}

