//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Defines gc-specific convars that integrate their AppIDs so that
//			two GC's in the same shell don' overwrite each other
//
//=============================================================================

#ifndef GCCONVAR_H
#define GCCONVAR_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/convar.h"


//-----------------------------------------------------------------------------
// Purpose: GC specifc ConVar
//-----------------------------------------------------------------------------
class GCConVar : public ConVar
{
public:
	GCConVar( const char *pName, const char *pDefaultValue, int flags = 0)
		: ConVar( pName, pDefaultValue, flags) {}
	GCConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString )
		: ConVar( pName, pDefaultValue, flags, pHelpString ) {}
	GCConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax )
		: ConVar( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax ) {}
	GCConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, FnChangeCallback_t callback )
		: ConVar( pName, pDefaultValue, flags, pHelpString, callback ) {}
	GCConVar( const char *pName, const char *pDefaultValue, int flags, const char *pHelpString, bool bMin, float fMin, bool bMax, float fMax, FnChangeCallback_t callback )
		: ConVar( pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax, callback ) {}

	virtual const char *GetName( void ) const;
	const char *GetBaseName() const { GetName(); return m_pchBaseName; } // returns the name without the appID suffix

protected:
	mutable CUtlString m_strGCName;
	mutable const char *m_pchBaseName;
};


//-----------------------------------------------------------------------------
// Purpose: GC specific ConCommand
//-----------------------------------------------------------------------------
class GCConCommand : public ConCommand
{
public:
	GCConCommand( const char *pName, FnCommandCallbackV1_t callback, const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 )
		: ConCommand( pName, callback, pHelpString, flags, completionFunc ) {}
	GCConCommand( const char *pName, FnCommandCallback_t callback, const char *pHelpString = 0, int flags = 0, FnCommandCompletionCallback completionFunc = 0 )
		: ConCommand( pName, callback, pHelpString, flags, completionFunc ) {}
	GCConCommand( const char *pName, ICommandCallback *pCallback, const char *pHelpString = 0, int flags = 0, ICommandCompletionCallback *pCommandCompletionCallback = 0 )
		: ConCommand( pName, pCallback, pHelpString, flags, pCommandCompletionCallback ) {}

	virtual const char *GetName( void ) const;
	const char *GetBaseName() const { GetName(); return m_pchBaseName; } // returns the name without the appID suffix

protected:
	mutable CUtlString m_strGCName;
	mutable const char *m_pchBaseName;
};

#define GC_CON_COMMAND( name, description ) \
	static void name( const CCommand &args ); \
	static GCConCommand name##_command( #name, name, description ); \
	static void name( const CCommand &args )

#define AUTO_CONFIRM_CON_COMMAND()						\
{														\
	static RTime32 rtimeLastRan = 0;					\
	if ( CRTime::RTime32TimeCur() - rtimeLastRan > 3 )	\
	{													\
		rtimeLastRan = CRTime::RTime32TimeCur();		\
		EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "Auto-confirm: Please repeat command within 3 seconds to confirm.\n" ); \
		return;											\
	}													\
	else												\
	{													\
		rtimeLastRan = 0;								\
	}													\
}														

#endif
