
#include "cbase.h"
#include "econ_item_description.h"
#include "econ_item_interface.h"
//#include "econ_holidays.h"
#include "vgui/ILocalize.h"
#include "localization_provider.h"

#if defined( DOTA_DLL ) || defined( DOTA_GC_DLL )
#include "dota_sharedfuncs.h"
#endif

	#ifndef EXTERNALTESTS_DLL
		#include "econ_item_inventory.h"
	#endif

#ifdef PROJECT_TF
	#include "tf_duel_summary.h"
	#include "econ_contribution.h"
	#include "tf_player_info.h"
#endif

#ifdef VPROF_ENABLED
	static const char *g_pszEconDescriptionVprofGroup = _T("Econ Description");
#endif

char *(g_pchWearAmountStrings[]) =
{
	"#SFUI_InvTooltip_Wear_Amount_0",
	"#SFUI_InvTooltip_Wear_Amount_1",
	"#SFUI_InvTooltip_Wear_Amount_2",
	"#SFUI_InvTooltip_Wear_Amount_3",
	"#SFUI_InvTooltip_Wear_Amount_4"
};

char *( g_pchQuestOperationalPoints[] ) =
{
	"", // 1-based
	"#Quest_OperationalPoints_1",	
	"#Quest_OperationalPoints_2",	
	"#Quest_OperationalPoints_3",	
};

ConVar cl_show_quest_info( "cl_show_quest_info", "0", FCVAR_DEVELOPMENTONLY );

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void IEconItemDescription::YieldingFillOutEconItemDescription( IEconItemDescription *out_pDescription, CLocalizationProvider *pLocalizationProvider, const IEconItemInterface *pEconItem )
{
	VPROF_BUDGET( "IEconItemDescription::YieldingFillOutEconItemDescription()", g_pszEconDescriptionVprofGroup );

	Assert( out_pDescription );
	Assert( pLocalizationProvider );
	Assert( pEconItem );

	out_pDescription->YieldingCacheDescriptionData( pLocalizationProvider, pEconItem );
	out_pDescription->GenerateDescriptionLines( pLocalizationProvider, pEconItem );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const econ_item_description_line_t *IEconItemDescription::GetFirstLineWithMetaType( uint32 unMetaTypeSearchFlags ) const
{
	for ( unsigned int i = 0; i < GetLineCount(); i++ )
	{
		const econ_item_description_line_t& pLine = GetLine(i);
		if ( (pLine.unMetaType & unMetaTypeSearchFlags) == unMetaTypeSearchFlags )
			return &pLine;
	}

	return NULL;
}
