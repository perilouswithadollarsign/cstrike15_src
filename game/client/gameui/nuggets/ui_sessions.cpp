//=========== (C) Copyright Valve, L.L.C. All rights reserved. ===========

#include "cbase.h"
#include "ui_nugget.h"

class CUiNuggetSessions : public CUiNuggetBase
{
	DECLARE_NUGGET_FN_MAP( CUiNuggetSessions, CUiNuggetBase );

	NUGGET_FN( CloseSession )
	{
		g_pMatchFramework->CloseSession();
		return NULL;
	}

	NUGGET_FN( CreateSession )
	{
		args->SetName( "settings" );
		g_pMatchFramework->CreateSession( args );
		return NULL;
	}

	NUGGET_FN( MatchSession )
	{
		args->SetName( "settings" );
		g_pMatchFramework->MatchSession( args );
		return NULL;
	}

	NUGGET_FN( SessionSystemData )
	{
		if ( IMatchSession *p = g_pMatchFramework->GetMatchSession() )
		{
			KeyValues *kv = p->GetSessionSystemData();
			return kv ? kv->MakeCopy() : NULL;
		}
		return NULL;
	}

	NUGGET_FN( SessionSettings )
	{
		if ( IMatchSession *p = g_pMatchFramework->GetMatchSession() )
		{
			KeyValues *kv = p->GetSessionSettings();
			return kv ? kv->MakeCopy() : NULL;
		}
		return NULL;
	}

	NUGGET_FN( SessionUpdateSettings )
	{
		if ( IMatchSession *p = g_pMatchFramework->GetMatchSession() )
		{
			args->SetName( "settings" );
			p->UpdateSessionSettings( args );
		}
		return NULL;
	}

	NUGGET_FN( SessionCommand )
	{
		if ( IMatchSession *p = g_pMatchFramework->GetMatchSession() )
		{
			p->Command( args->GetFirstSubKey() );
		}
		return NULL;
	}
};

UI_NUGGET_FACTORY_SINGLETON( CUiNuggetSessions, "sessions" );
