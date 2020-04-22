//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ====
//
// Purpose:
//
//=============================================================================

#ifndef LOGIC_PLAYERPROXY_H
#define LOGIC_PLAYERPROXY_H
#pragma once

//-----------------------------------------------------------------------------
// Purpose: Used to relay outputs/inputs from the player to the world and vice versa
//-----------------------------------------------------------------------------
class CLogicPlayerProxy : public CLogicalEntity
{
	DECLARE_CLASS( CLogicPlayerProxy, CLogicalEntity );
	DECLARE_DATADESC();

public:
	// FIXME: Subclass
#ifdef PORTAL2
	COutputEvent m_OnStartSlowingTime;
	COutputEvent m_OnStopSlowingTime;
	COutputEvent m_OnCoopPing;

	COutputEvent m_OnPrimaryPortalPlaced;
	COutputEvent m_OnSecondaryPortalPlaced;
#endif // PORTAL2

#if defined( HL2_DLL )
	COutputEvent m_OnFlashlightOn;
	COutputEvent m_OnFlashlightOff;
	COutputEvent m_PlayerMissedAR2AltFire; // Player fired a combine ball which did not dissolve any enemies. 
#endif // HL2_DLL

	COutputEvent m_PlayerHasAmmo;
	COutputEvent m_PlayerHasNoAmmo;
	COutputEvent m_PlayerDied;

	COutputEvent m_OnDuck;
	COutputEvent m_OnUnDuck;
	COutputEvent m_OnJump;

	COutputInt m_RequestedPlayerHealth;

#if defined HL2_EPISODIC
	void InputSetFlashlightSlowDrain( inputdata_t &inputdata );
	void InputSetFlashlightNormalDrain( inputdata_t &inputdata );
	void InputLowerWeapon( inputdata_t &inputdata );
	void InputSetLocatorTargetEntity( inputdata_t &inputdata );
#endif // HL2_EPISODIC

	void InputRequestPlayerHealth( inputdata_t &inputdata );
	void InputSetPlayerHealth( inputdata_t &inputdata );
	void InputRequestAmmoState( inputdata_t &inputdata );
	void InputEnableCappedPhysicsDamage( inputdata_t &inputdata );
	void InputDisableCappedPhysicsDamage( inputdata_t &inputdata );

#if defined ( PORTAL2 )
	void InputAddPotatosToPortalgun( inputdata_t &inputdata );
	void InputRemovePotatosFromPortalgun( inputdata_t &inputdata );

	void InputSetDropEnabled( inputdata_t &inputdata );
	void InputForceVMGrabController( inputdata_t &inputdata );
	void InputForcePhysicsGrabController( inputdata_t &inputdata );
	void InputResetGrabControllerBehavior( inputdata_t &inputdata );
	void InputPaintPlayerWithPortalPaint( inputdata_t &inputdata );
	void InputSetMotionBlurAmount( inputdata_t &inputdata );
#endif

	void Activate( void );

	bool PassesDamageFilter( const CTakeDamageInfo &info );

	EHANDLE m_hPlayer;
};


#endif	// LOGIC_PLAYERPROXY_H