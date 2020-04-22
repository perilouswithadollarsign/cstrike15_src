//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef EXT_KEYVALUES_H
#define EXT_KEYVALUES_H
#ifdef _WIN32
#pragma once
#endif

#include "mm_framework.h"

#include "matchmaking/mm_helpers.h"

#define KeyValuesAddDefaultValue( kv, szKeyName, defValue, fnSet ) \
	( (kv)->FindKey( szKeyName ) ? (1) : ( (kv)->fnSet( szKeyName, defValue ), 1 ) )

#define KeyValuesAddDefaultString( kv, szKeyName, defValue ) \
	KeyValuesAddDefaultValue( kv, szKeyName, defValue, SetString )


//
// ContextValue_t allows for quick mapping from strings to title-defined values
// in title specific code.
//
// static ContextValue_t values[] = {
// 	{ "=INGAME",	CONTEXT_GAME_STATE_INGAME },
// 	{ "=INFINALE",	CONTEXT_GAME_STATE_INFINALE },
// 	{ NULL,			CONTEXT_GAME_STATE_INLOBBY },
// };
// SetAllUsersContext( CONTEXT_GAME_STATE, values->ScanValues( szValue ) );

struct ContextValue_t
{
	char const *m_szValue;
	unsigned int m_dwValue;

	inline unsigned int ScanValues( char const *szValue )
	{
		ContextValue_t const *p = this;
		for ( ; p->m_szValue; ++ p )
		{
			if ( !Q_stricmp( p->m_szValue, szValue ) )
				break;
		}
		return p->m_dwValue;
	}
};


//
// Rule evaluation
//

class IPropertyRule
{
public:
	virtual bool ApplyRuleUint64( uint64 uiBase, uint64 &uiNew ) = 0;
	virtual bool ApplyRuleFloat( float flBase, float &flNew ) = 0;
};

IPropertyRule * GetRuleByName( char const *szRuleName );



#endif
