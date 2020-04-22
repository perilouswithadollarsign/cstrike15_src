//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the stick partner struct props use to keep track of
//			stick constraints.
//
//=============================================================================//

#include "cbase.h"

#include "stick_partner.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

StickPartner_t::StickPartner_t()
	: m_pConstraint( NULL ),
	  m_nLastContactCount( 0 )
{}

StickPartner_t::StickPartner_t( const StickPartner_t& rhs )
	: m_other( rhs.m_other ),
	  m_pConstraint( rhs.m_pConstraint )
{
	m_contacts = rhs.m_contacts;
}
