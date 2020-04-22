//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
// CSheetEditorPanel - Tool panel for editing sprite sheet information
//
//===============================================================================

#include "dme_controls/sheeteditorpanel.h"
#include "dme_controls/dmepanel.h"
#include "movieobjects/dmeparticlesystemdefinition.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "matsys_controls/matsyscontrols.h"
#include "vgui/ivgui.h"
#include "vgui_controls/propertypage.h"
#include "vgui_controls/propertysheet.h"
#include "vgui_controls/textentry.h"
#include "vgui_controls/splitter.h"
#include "vgui_controls/checkbutton.h"
#include "matsys_controls/colorpickerpanel.h"
#include "particles/particles.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "tier2/renderutils.h"
#include "bitmap/psheet.h"
#include "matsys_controls/vmtpicker.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

using namespace vgui;

//-----------------------------------------------------------------------------
//
// CSheetEditorPanel
//
//-----------------------------------------------------------------------------

//IMPLEMENT_DMEPANEL_FACTORY( CParticleSystemDmePanel, DmeParticleSystemDefinition, "DmeParticleSystemDefinitionEditor", "Particle System Editor", true );

CSheetEditorPanel::CSheetEditorPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName ),
	m_pSheetInfo(NULL)
{
	m_pTitleLabel = new vgui::Label( this, "TestLabel", "<Title>" );
	m_pTitleLabel->SetPinCorner( Panel::PIN_TOPLEFT, 12, 12 );
	m_pTitleLabel->SetWide( 400 );

	m_pVMTPicker = new CVMTPicker( this, "SheetPreview" );
	m_pVMTPicker->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 200, 42, -12, -12 );
}

void CSheetEditorPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pTitleLabel->SetBgColor( Color( 200, 200, 200, 255 ) );
	m_pTitleLabel->SetFgColor( Color( 0, 0, 0, 255 ) );
	SetBgColor( Color( 100, 200, 100, 255 ) );
}

CSheetEditorPanel::~CSheetEditorPanel()
{
}

void CSheetEditorPanel::SetParticleSystem( CDmeParticleSystemDefinition *pParticleSystem )
{
	char strBuffer[ 256 ];
	const char *pSystemName = "<no system>";
	const char *pVMTName = "<no vmt>";
	
	if ( pParticleSystem != NULL )
	{
		pSystemName = pParticleSystem->GetName();
		if ( !pSystemName || !pSystemName[0] )
		{
			pSystemName = "<no name>";
		}
	}

	if ( pParticleSystem != NULL )
	{
		pVMTName = pParticleSystem->GetValueString( "material" );
	}

	Q_snprintf( strBuffer, sizeof(strBuffer), "%s: %s", pSystemName, pVMTName );
	m_pTitleLabel->SetText( strBuffer );
}