//=========== (C) Copyright Valve, L.L.C. All rights reserved. ===========

#include "cbase.h"
#include "ui_nugget.h"

class CUiNuggetSemaphore : public CUiNuggetBase
{
	DECLARE_NUGGET_FN_MAP( CUiNuggetSemaphore, CUiNuggetBase );

	NUGGET_FN( Configure )
	{
		m_pUiNuggetData->MergeFrom( args, KeyValues::MERGE_KV_UPDATE );
		return NULL;
	}

	NUGGET_FN( Signal )
	{
		char const *szSemaphoreSignal = m_pUiNuggetData->GetString( "signal" );
		if ( !*szSemaphoreSignal )
			return NULL;

		KeyValues *pEvent = m_pUiNuggetData->MakeCopy();
		pEvent->SetName( szSemaphoreSignal );
		KeyValues::AutoDelete autodelete_pEvent( pEvent );

		char const *szEventData = m_pUiNuggetData->GetString( "eventdata" );
		if ( *szEventData )
		{
			if ( KeyValues *pSub = pEvent->FindKey( szEventData ) )
			{
				pEvent->RemoveSubKey( pSub );
				pSub->deleteThis();
			}
			KeyValues *pParams = args->MakeCopy();
			pParams->SetName( szEventData );
			pEvent->AddSubKey( pParams );
		}

		BroadcastEventToScreens( pEvent );
		return NULL;
	}

public:
	CUiNuggetSemaphore()
	{
		m_pUiNuggetData->SetString( "signal", "OnSemaphore" );
		m_pUiNuggetData->SetString( "eventdata", "eventdata" );
	}
};

UI_NUGGET_FACTORY_INSTANCES( CUiNuggetSemaphore, "semaphore" );
