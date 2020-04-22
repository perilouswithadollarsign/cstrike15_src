//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VOptionsCloud.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "vgui_controls/Button.h"
#include "steamcloudsync.h"
#include "fmtstr.h"
#include "matchmaking/mm_helpers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

OptionsCloud::OptionsCloud( Panel *parent, const char *panelName ):
BaseClass( parent, panelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "#L4D360UI_CloudSettings_Title" );

	SetFooterEnabled( true );
	UpdateFooter();

	m_numSavesSelected = 3;
}

OptionsCloud::~OptionsCloud()
{
}

void OptionsCloud::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	UpdateFooter();

	GameSteamCloudPreferences_t gscp;
	g_pGameSteamCloudSync->GetPreferences( gscp );
	m_numSavesSelected = gscp.m_numSaveGamesToSync;

	if ( BaseModHybridButton *pDrpSaves = dynamic_cast< BaseModHybridButton * >( FindChildByName( "DrpCloudSaves" ) ) )
	{
		pDrpSaves->SetCurrentSelection( CFmtStr( "#L4D360UI_CloudSettings_%d", gscp.m_numSaveGamesToSync ) );
	}
}

void OptionsCloud::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		pFooter->SetButtons( FB_BBUTTON | FB_STEAM_SELECT );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Done" );
	}
}

void OptionsCloud::OnCommand( const char *pCommand )
{
	int iUserSlot = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	int iController = XBX_GetUserId( iUserSlot );

	if ( UI_IsDebug() )
	{
		Msg("[GAMEUI] Handling options menu command %s from user%d ctrlr%d\n", pCommand, iUserSlot, iController );
	}

	if ( char const *szSaveGames = StringAfterPrefix( pCommand, "#L4D360UI_CloudSettings_" ) )
	{
		Msg( "Steam cloud setting: %s\n", szSaveGames );
		m_numSavesSelected = atoi( szSaveGames );
	}

	BaseClass::OnCommand( pCommand );
}

void OptionsCloud::OnKeyCodePressed( vgui::KeyCode code )
{
	int iUserSlot = GetJoystickForCode( code );
	CBaseModPanel::GetSingleton().SetLastActiveUserId( iUserSlot );
	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_B:
		{
			GameSteamCloudPreferences_t gscp;
			g_pGameSteamCloudSync->GetPreferences( gscp );
			if ( m_numSavesSelected != gscp.m_numSaveGamesToSync )
			{
				TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
				if ( TitleDataFieldsDescription_t const *pCloudSaves = TitleDataFieldsDescriptionFindByString( fields, "CFG.sys.cloud_saves" ) )
				{
					TitleDataFieldsDescriptionSetValue<uint8>( pCloudSaves, g_pMatchFramework->GetMatchSystem()
						->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() ), m_numSavesSelected + 1 );

					// Force the Steam cloud manager to update with latest setting
					KeyValues *kvUpdate = new KeyValues( "OnProfileDataLoaded" );
					KeyValues::AutoDelete autodelete( kvUpdate );
					g_pGameSteamCloudSync->OnEvent( kvUpdate );
				}
			}
		}
		break;
	}
	BaseClass::OnKeyCodePressed( code );
}

void OptionsCloud::OnThink()
{
	// Update every frame while active in case another menu overwrites our footer
	UpdateFooter();

	BaseClass::OnThink();
}