
//========= Copyright � 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#if !defined( ENGINE_HELPER_PS3_H_ )
#define ENGINE_HELPER_PS3_H_

#if defined( _PS3 )

class EngineHelperPS3
{
	public:
		static int GetAgeRestrictionByRegion( void );
		static bool PS3_IsUserRestrictedFromOnline( void );
		static bool PS3_PendingInvitesFound( void );
		static void PS3_ShowInviteOverlay( void );
		static bool PS3_IsUserRestrictedFromChat( void );
};

#endif

#endif // define guard