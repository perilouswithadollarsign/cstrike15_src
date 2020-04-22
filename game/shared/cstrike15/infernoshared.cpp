//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=====================================================================================//

#include "cbase.h"

#if defined ( GAME_DLL )
	#include "Effects/inferno.h"
	#define INFERNOCLASS		CInferno
	#define FIRECRACKERBLASTCLASS	CFireCrackerBlast
#endif

#if defined( CLIENT_DLL )
	#include "Effects/clientinferno.h"
	#define INFERNOCLASS		C_Inferno
	#define FIRECRACKERBLASTCLASS	C_FireCrackerBlast
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//---------------------------------------------------------
const char *INFERNOCLASS::GetParticleEffectName()
{
	return "molotov_groundfire";
}
#if defined( GAME_DLL )
const char *INFERNOCLASS::GetImpactParticleEffectName()
{
	return "molotov_explosion";
}
#endif


//---------------------------------------------------------
const char *FIRECRACKERBLASTCLASS::GetParticleEffectName()
{
	return "firework_crate_ground_effect";
}
#if defined( GAME_DLL )
const char *FIRECRACKERBLASTCLASS::GetImpactParticleEffectName()
{
	return "firework_crate_explosion_01";
}
#endif
