//====== Copyright 1996-2013, Valve Corporation, All rights reserved. =======
//
// Purpose: interface to allow apps to temporarily prevent steam from updating
//
//=============================================================================

#ifndef ISTEAMAPPDISABLEUPDATE_H
#define ISTEAMAPPDISABLEUPDATE_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Interface to allow an app to temporarily prevent steam from updating
//-----------------------------------------------------------------------------
class ISteamAppDisableUpdate
{
public:

	/// Set amount of time that steam will be blocked from updating the app.
	/// Pass zero to indicate that it safe to update again.  Note that your
	/// timeout value may be clamped, as a safety precaution to prevent
	/// a bug from putting an app into a wedge state.
	virtual void SetAppUpdateDisabledSecondsRemaining( uint32 nSeconds ) = 0;
};

#define STEAMAPPDISABLEUPDATE_INTERFACE_VERSION "SteamAppDisableUpdate001"

#endif // ISTEAMCONTROLLER_H
