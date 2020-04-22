//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Declares the stick partner struct props use to keep track of
//			stick constraints.
//
//=============================================================================//
#ifndef STICK_PARTNER_H
#define STICK_PARTNER_H

struct StickPartner_t
{
	StickPartner_t();
	StickPartner_t( const StickPartner_t& );

	CBaseHandle m_other;
	CUtlVector< Vector > m_contacts;	// Where we are touching the other entity
	IPhysicsConstraint* m_pConstraint;	// The constraint holding this object to the other object
	int m_nLastContactCount;			// Number of contacts we had last time we checked
};

#endif // ifndef STICK_PARTNER_H
