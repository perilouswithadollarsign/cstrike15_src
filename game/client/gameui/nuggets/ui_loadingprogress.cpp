//=========== (C) Copyright Valve, L.L.C. All rights reserved. ===========

#include "cbase.h"
#include "ui_nugget.h"

class CUiNuggetLoadingProgress : public CUiNuggetBase
{
public:
	CUiNuggetLoadingProgress()
	{
		m_pUiNuggetData->SetString( "map", "" );
		m_pUiNuggetData->SetFloat( "progress", 0.0f );
	}

	DECLARE_NUGGET_FN_MAP( CUiNuggetLoadingProgress, CUiNuggetBase );

	NUGGET_BROADCAST_FN( OnLevelLoadingProgress )
	{
		m_pUiNuggetData->MergeFrom( args, KeyValues::MERGE_KV_BORROW );
		return true;
	}

	virtual bool ShouldDeleteOnLastScreenDisconnect() 
	{ 
		// bad to try to delete a static.
		return false; 
	}
};

static CUiNuggetLoadingProgress g_nugget_loadingprogress;
UI_NUGGET_FACTORY_GLOBAL_INSTANCE( CUiNuggetLoadingProgress, &g_nugget_loadingprogress, "loadingprogress" );
