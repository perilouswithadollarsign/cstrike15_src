//=========== (C) Copyright Valve, L.L.C. All rights reserved. ===========

#include "cbase.h"
#include "ui_nugget.h"
#include "matchmaking/imatchframework.h"

class CUiNuggetMatchEvents : public CUiNuggetBase, IMatchEventsSink
{
	DECLARE_NUGGET_FN_MAP( CUiNuggetMatchEvents, CUiNuggetBase );

	virtual void OnEvent( KeyValues *pEvent )
	{
		BroadcastEventToScreens( pEvent );
		return;

#if SCRIPTS_NEED_EVENT_NAMES_WITH_BASIC_CHARSET
		// In case scripts cannot define function names
		// associations with any characters, then we'll
		// need to do some work to make event names conform
		// with those scripts.
		//
		// In lua we can easily do any member function names:
		/*
		mainmenu["Command::Game::SampleCmd"] = function(self, params)
			print( "Command::Game::SampleCmd" )
			print( self.extraPieceOfData )
		end
		*/
		//
		// Fix the event name for scripting:
		char chEventName[256] = {0}, *pch = chEventName;
		Q_snprintf( chEventName, ARRAYSIZE( chEventName ), "%s", pEvent->GetName() );
		for ( ; *pch; ++ pch )
		{
			bool bValidChar = (
				( *pch >= 'a' && *pch <= 'z' ) ||
				( *pch >= 'A' && *pch <= 'Z' ) ||
				( *pch >= '0' && *pch <= '9' )
				);
			if ( !bValidChar )
				*pch = '_';
		}
		
		// New event for scripting
		KeyValues *pEventCopy = pEvent->MakeCopy();
		KeyValues::AutoDelete autodelete_pEventCopy( pEventCopy );
		pEventCopy->SetName( chEventName );
		BroadcastEventToScreens( pEventCopy );
#endif
	}

public:
	CUiNuggetMatchEvents()
	{
		g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
	}
	~CUiNuggetMatchEvents()
	{
		g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
	}
};

UI_NUGGET_FACTORY_SINGLETON( CUiNuggetMatchEvents, "matchevents" );
