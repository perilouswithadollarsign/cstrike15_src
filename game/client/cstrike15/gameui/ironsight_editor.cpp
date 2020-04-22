//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifdef DEBUG

#include "cbase.h"

#include "ienginevgui.h"
#include "gameui_interface.h"
#include "basepanel.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/FileOpenDialog.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/RadioButton.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/MessageBox.h"
#include "matsys_controls/colorpickerpanel.h"
#include "matsys_controls/vtfpreviewpanel.h"
#include "filesystem.h"
#include "keyvalues.h"
#include "engineinterface.h"
#include "gameui_interface.h"
#include "vstdlib/random.h"

#include "ironsight_editor.h"

#ifdef IRONSIGHT

CIronSightDialog *g_pIronSightDialog = NULL;

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar ironsight_position;
extern ConVar ironsight_angle;
extern ConVar ironsight_fov;
extern ConVar ironsight_override;
extern ConVar ironsight_pivot_forward;
extern ConVar ironsight_looseness;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CIronSightDialog::CIronSightDialog(vgui::Panel *parent) : BaseClass(parent, "IronSightDialog")
{
	SetDeleteSelfOnClose(true);
	SetSizeable( false );

	MoveToCenterOfScreen();

	m_pSliderPosX = new vgui::Slider( this, "SliderPosX" );
	m_pSliderPosX->AddActionSignalTarget( this );
	m_pSliderPosX->SetRange( -100, 100 );
	m_pSliderPosX->SetDragOnRepositionNob( true );

	m_pSliderPosY = new vgui::Slider( this, "SliderPosY" );
	m_pSliderPosY->AddActionSignalTarget( this );
	m_pSliderPosY->SetRange( -100, 100 );
	m_pSliderPosY->SetDragOnRepositionNob( true );

	m_pSliderPosZ = new vgui::Slider( this, "SliderPosZ" );
	m_pSliderPosZ->AddActionSignalTarget( this );
	m_pSliderPosZ->SetRange( -100, 100 );
	m_pSliderPosZ->SetDragOnRepositionNob( true );

	m_pSliderRotX = new vgui::Slider( this, "SliderRotX" );
	m_pSliderRotX->AddActionSignalTarget( this );
	m_pSliderRotX->SetRange( -100, 100 );
	m_pSliderRotX->SetDragOnRepositionNob( true );

	m_pSliderRotY = new vgui::Slider( this, "SliderRotY" );
	m_pSliderRotY->AddActionSignalTarget( this );
	m_pSliderRotY->SetRange( -100, 100 );
	m_pSliderRotY->SetDragOnRepositionNob( true );

	m_pSliderRotZ = new vgui::Slider( this, "SliderRotZ" );
	m_pSliderRotZ->AddActionSignalTarget( this );
	m_pSliderRotZ->SetRange( -100, 100 );
	m_pSliderRotZ->SetDragOnRepositionNob( true );

	m_pSliderFOV = new vgui::Slider( this, "SliderFOV" );
	m_pSliderFOV->AddActionSignalTarget( this );
	m_pSliderFOV->SetRange( 10, 90 );
	m_pSliderFOV->SetDragOnRepositionNob( true );
	m_pSliderFOV->SetValue(90);

	m_pSliderPivotForward = new vgui::Slider( this, "SliderPivotForward" );
	m_pSliderPivotForward->AddActionSignalTarget( this );
	m_pSliderPivotForward->SetRange( -20, 20 );
	m_pSliderPivotForward->SetDragOnRepositionNob( true );
	m_pSliderPivotForward->SetValue(0);

	m_pSliderLooseness = new vgui::Slider( this, "SliderLooseness" );
	m_pSliderLooseness->AddActionSignalTarget( this );
	m_pSliderLooseness->SetRange( 0, 100 );
	m_pSliderLooseness->SetDragOnRepositionNob( true );
	m_pSliderLooseness->SetValue(100);
	
	m_pSchemaText = new vgui::TextEntry( this, "SchemaText" );
	m_pSchemaText->AddActionSignalTarget( this );
	m_pSchemaText->SetMultiline( true );

	LoadControlSettings("Resource/ironsight_dialog.res");
	
	m_vecPosBackup = Vector(0,0,0);
	m_vecRotBackup = Vector(0,0,0);


	sscanf( ironsight_position.GetString(), "%f %f %f", &m_vecPosBackup.x, &m_vecPosBackup.y, &m_vecPosBackup.z );
	m_pSliderPosX->SetValue(0);
	m_pSliderPosY->SetValue(0);
	m_pSliderPosZ->SetValue(0);

	sscanf( ironsight_angle.GetString(), "%f %f %f", &m_vecRotBackup.x, &m_vecRotBackup.y, &m_vecRotBackup.z );
	m_pSliderRotX->SetValue(0);
	m_pSliderRotY->SetValue(0);
	m_pSliderRotZ->SetValue(0);

	m_pSliderFOV->SetValue( ironsight_fov.GetFloat() );

	m_pSliderPivotForward->SetValue( ironsight_pivot_forward.GetFloat() );

	m_pSliderLooseness->SetValue( ironsight_looseness.GetFloat() * 100.0f );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CIronSightDialog::~CIronSightDialog()
{
	delete m_pSliderPosX;
	delete m_pSliderPosY;
	delete m_pSliderPosZ;

	delete m_pSliderRotX;
	delete m_pSliderRotY;
	delete m_pSliderRotZ;

	delete m_pSliderFOV;

	delete m_pSliderPivotForward;

	delete m_pSchemaText;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CIronSightDialog::OnClose( void )
{
	BaseClass::OnClose();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
void CIronSightDialog::OnMessage( const KeyValues *pParams, vgui::VPANEL fromPanel )
{
	

	if ( !Q_strcmp( "SliderMoved", pParams->GetName() ) )
	{
		char szTemp[128];

		V_snprintf( szTemp, 128, "%f %f %f", 
			float( m_vecPosBackup.x + m_pSliderPosX->GetValue() * 0.01f ),
			float( m_vecPosBackup.y + m_pSliderPosY->GetValue() * 0.01f ),
			float( m_vecPosBackup.z + m_pSliderPosZ->GetValue() * 0.01f ) );

		//Msg( "ironsight_position: %s\n", szTemp );

		ironsight_position.SetValue( szTemp );

		V_snprintf( szTemp, 128, "%f %f %f", 
			float( m_vecRotBackup.x + m_pSliderRotX->GetValue() * 0.01f ),
			float( m_vecRotBackup.y + m_pSliderRotY->GetValue() * 0.01f ),
			float( m_vecRotBackup.z + m_pSliderRotZ->GetValue() * 0.01f ) );

		//Msg( "ironsight_angle: %s\n", szTemp );

		ironsight_angle.SetValue( szTemp );

		ironsight_fov.SetValue( m_pSliderFOV->GetValue() );

		ironsight_pivot_forward.SetValue( m_pSliderPivotForward->GetValue() );

		ironsight_looseness.SetValue( m_pSliderLooseness->GetValue() * 0.01f );

		char szSchemaText[512];
		V_snprintf(szSchemaText, 512, "\"ironsight eye pos\"    \"%s\"\n\"ironsight pivot angle\"    \"%s\"\n\"ironsight fov\"    \"%s\"\n\"ironsight pivot forward\"    \"%s\"\n\"ironsight looseness\"    \"%f\"",
			ironsight_position.GetString(),
			ironsight_angle.GetString(),
			ironsight_fov.GetString(),
			ironsight_pivot_forward.GetString(),
			ironsight_looseness.GetFloat()
			);

		m_pSchemaText->SetText( szSchemaText );

	}
	else if ( !Q_strcmp( "SliderDragEnd", pParams->GetName() ) )
	{
		sscanf( ironsight_position.GetString(), "%f %f %f", &m_vecPosBackup.x, &m_vecPosBackup.y, &m_vecPosBackup.z );
		m_pSliderPosX->SetValue(0);
		m_pSliderPosY->SetValue(0);
		m_pSliderPosZ->SetValue(0);

		sscanf( ironsight_angle.GetString(), "%f %f %f", &m_vecRotBackup.x, &m_vecRotBackup.y, &m_vecRotBackup.z );
		m_pSliderRotX->SetValue(0);
		m_pSliderRotY->SetValue(0);
		m_pSliderRotZ->SetValue(0);
	}
	else if ( !Q_strcmp( "CloseFrameButtonPressed", pParams->GetName() ) )
	{
		Close();
	}
	else if ( !Q_strcmp( "CheckButtonChecked", pParams->GetName() ) )
	{
		ironsight_override.SetValue( pParams->MakeCopy()->GetInt("state") );
	}
	else
	{
		BaseClass::OnMessage( pParams, fromPanel );
	}

	/*
	//don't spew these
	if ( Q_strcmp( "OnMouseFocusTicked", pParams->GetName() ) && 
		 Q_strcmp( "KeyFocusTicked", pParams->GetName() ) &&
		 Q_strcmp( "MouseFocusTicked", pParams->GetName() ) &&
		 Q_strcmp( "CursorMoved", pParams->GetName() ) &&
		 Q_strcmp( "CursorExited", pParams->GetName() ) &&
		 Q_strcmp( "CursorEntered", pParams->GetName() ) &&
		 Q_strcmp( "OnNavigateTo", pParams->GetName() ) &&
		 Q_strcmp( "OnNavigateFrom", pParams->GetName() ) )
	{
		Msg( "=========================" );
		KeyValuesDumpAsDevMsg( pParams->MakeCopy() );
	}
	*/

}


//void CIronSightDialog::OnCommand( const char *command )
//{
//	if ( !Q_strcmp( command, "refresh_scheme" ) )
//	{
//		LoadControlSettings("Resource/ironsight_dialog.res");
//	}	
//}


CON_COMMAND_F( ironsight_editor, "Edit ironsights.", FCVAR_DONTRECORD | FCVAR_DEVELOPMENTONLY )
{
	g_pIronSightDialog = new CIronSightDialog(NULL);

	vgui::VPANEL parentPanel = enginevgui->GetPanel( PANEL_INGAMESCREENS );
	g_pIronSightDialog->SetParent( parentPanel );

	g_pIronSightDialog->SetVisible( true );
	g_pIronSightDialog->Repaint();
	g_pIronSightDialog->MoveToFront();
}
#endif

#endif
