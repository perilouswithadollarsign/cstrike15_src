//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Singleton dialog that generates and presents the entity report.
//
//===========================================================================//

#include "CommentaryPropertiesPanel.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "iregistry.h"
#include "vgui/ivgui.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/textentry.h"
#include "vgui_controls/checkbutton.h"
#include "vgui_controls/combobox.h"
#include "vgui_controls/radiobutton.h"
#include "vgui_controls/messagebox.h"
#include "vgui_controls/scrollbar.h"
#include "vgui_controls/scrollableeditablepanel.h"
#include "commeditdoc.h"
#include "commedittool.h"
#include "datamodel/dmelement.h"
#include "dmecommentarynodeentity.h"
#include "dme_controls/soundpicker.h"
#include "dme_controls/soundrecordpanel.h"
#include "matsys_controls/picker.h"
#include "vgui_controls/fileopendialog.h"
#include "filesystem.h"
#include "tier2/fileutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CCommentaryPropertiesPanel::CCommentaryPropertiesPanel( CCommEditDoc *pDoc, vgui::Panel* pParent )
	: BaseClass( pParent, "CommentaryPropertiesPanel" ), m_pDoc( pDoc )
{
	SetPaintBackgroundEnabled( true );
	SetKeyBoardInputEnabled( true );

	m_pCommentaryNode = new vgui::EditablePanel( (vgui::Panel*)NULL, "CommentaryNode" );

	m_pNodeName = new vgui::TextEntry( m_pCommentaryNode, "CommentaryNodeName" );
	m_pNodeName->AddActionSignalTarget( this );

	m_pSoundFilePicker = new vgui::Button( m_pCommentaryNode, "AudioFilePickerButton", "", this, "PickSound" );

	m_pSoundFileName = new vgui::TextEntry( m_pCommentaryNode, "AudioFileName" );
	m_pSoundFileName->AddActionSignalTarget( this );

	m_pRecordSound = new vgui::Button( m_pCommentaryNode, "RecordAudioButton", "", this, "Record" );

	m_pSpeakerName = new vgui::TextEntry( m_pCommentaryNode, "Speaker" );
	m_pSpeakerName->AddActionSignalTarget( this );

	m_pSynopsis = new vgui::TextEntry( m_pCommentaryNode, "Synopsis" );
	m_pSynopsis->AddActionSignalTarget( this );

	m_pViewPositionPicker = new vgui::Button( m_pCommentaryNode, "ViewPositionPickerButton", "", this, "PickViewPosition" );
	m_pViewTargetPicker = new vgui::Button( m_pCommentaryNode, "ViewTargetPickerButton", "", this, "PickViewTarget" );

	m_pViewPosition = new vgui::TextEntry( m_pCommentaryNode, "ViewPosition" );
	m_pViewPosition->AddActionSignalTarget( this );

	m_pViewTarget = new vgui::TextEntry( m_pCommentaryNode, "ViewTarget" );
	m_pViewTarget->AddActionSignalTarget( this );

	m_pPreventMovement = new vgui::CheckButton( m_pCommentaryNode, "PreventMovement", "" );
	m_pPreventMovement->SetCommand( "PreventMovementClicked" );
	m_pPreventMovement->AddActionSignalTarget( this );

	m_pStartCommands = new vgui::TextEntry( m_pCommentaryNode, "StartCommands" );
	m_pStartCommands->AddActionSignalTarget( this );

	m_pEndCommands = new vgui::TextEntry( m_pCommentaryNode, "EndCommands" );
	m_pEndCommands->AddActionSignalTarget( this );

 	m_pPosition[0] = new vgui::TextEntry( m_pCommentaryNode, "PositionX" );
	m_pPosition[0]->AddActionSignalTarget( this );
 	m_pPosition[1] = new vgui::TextEntry( m_pCommentaryNode, "PositionY" );
	m_pPosition[1]->AddActionSignalTarget( this );
 	m_pPosition[2] = new vgui::TextEntry( m_pCommentaryNode, "PositionZ" );
	m_pPosition[2]->AddActionSignalTarget( this );

 	m_pOrientation[0] = new vgui::TextEntry( m_pCommentaryNode, "Pitch" );
	m_pOrientation[0]->AddActionSignalTarget( this );
 	m_pOrientation[1] = new vgui::TextEntry( m_pCommentaryNode, "Yaw" );
	m_pOrientation[1]->AddActionSignalTarget( this );
 	m_pOrientation[2] = new vgui::TextEntry( m_pCommentaryNode, "Roll" );
	m_pOrientation[2]->AddActionSignalTarget( this );

	m_pCommentaryNode->LoadControlSettings( "resource/commentarypropertiessubpanel_node.res" );

	m_pInfoTarget = new vgui::EditablePanel( (vgui::Panel*)NULL, "InfoTarget" );

	m_pTargetName = new vgui::TextEntry( m_pInfoTarget, "TargetName" );
	m_pTargetName->AddActionSignalTarget( this );

	m_pTargetPosition[0] = new vgui::TextEntry( m_pInfoTarget, "PositionX" );
	m_pTargetPosition[0]->AddActionSignalTarget( this );
 	m_pTargetPosition[1] = new vgui::TextEntry( m_pInfoTarget, "PositionY" );
	m_pTargetPosition[1]->AddActionSignalTarget( this );
 	m_pTargetPosition[2] = new vgui::TextEntry( m_pInfoTarget, "PositionZ" );
	m_pTargetPosition[2]->AddActionSignalTarget( this );

 	m_pTargetOrientation[0] = new vgui::TextEntry( m_pInfoTarget, "Pitch" );
	m_pTargetOrientation[0]->AddActionSignalTarget( this );
 	m_pTargetOrientation[1] = new vgui::TextEntry( m_pInfoTarget, "Yaw" );
	m_pTargetOrientation[1]->AddActionSignalTarget( this );
 	m_pTargetOrientation[2] = new vgui::TextEntry( m_pInfoTarget, "Roll" );
	m_pTargetOrientation[2]->AddActionSignalTarget( this );

	m_pInfoTarget->LoadControlSettings( "resource/commentarypropertiessubpanel_target.res" );

	m_pInfoRemarkable = new vgui::EditablePanel( (vgui::Panel*)NULL, "InfoRemarkable" );

	m_pInfoRemarkableName = new vgui::TextEntry( m_pInfoRemarkable, "RemarkableName" );
	m_pInfoRemarkableName->AddActionSignalTarget( this );

	m_pRemarkablePosition[0] = new vgui::TextEntry( m_pInfoRemarkable, "PositionX" );
	m_pRemarkablePosition[0]->AddActionSignalTarget( this );
	m_pRemarkablePosition[1] = new vgui::TextEntry( m_pInfoRemarkable, "PositionY" );
	m_pRemarkablePosition[1]->AddActionSignalTarget( this );
	m_pRemarkablePosition[2] = new vgui::TextEntry( m_pInfoRemarkable, "PositionZ" );
	m_pRemarkablePosition[2]->AddActionSignalTarget( this );

	m_pInfoRemarkableSubject = new vgui::TextEntry( m_pInfoRemarkable, "RemarkableSubject" );
	m_pInfoRemarkableSubject->AddActionSignalTarget( this );

	m_pInfoRemarkable->LoadControlSettings( "resource/commentarypropertiessubpanel_remarkable.res" );

	m_pCommentaryNodeScroll = new vgui::ScrollableEditablePanel( this, m_pCommentaryNode, "CommentaryNodeScroll" );
	m_pInfoTargetScroll = new vgui::ScrollableEditablePanel( this, m_pInfoTarget, "InfoTargetScroll" );
	m_pInfoRemarkableScroll = new vgui::ScrollableEditablePanel( this, m_pInfoRemarkable, "InfoRemarkableScroll" );

	LoadControlSettings( "resource/commentarypropertiespanel.res" );

	m_pCommentaryNodeScroll->SetVisible( false );
	m_pInfoTargetScroll->SetVisible( false );
	m_pInfoRemarkableScroll->SetVisible( false );
}


//-----------------------------------------------------------------------------
// Text to attribute...
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::TextEntryToAttribute( vgui::TextEntry *pEntry, const char *pAttributeName )
{
	int nLen = pEntry->GetTextLength();
	char *pBuf = (char*)_alloca( nLen+1 );
	pEntry->GetText( pBuf, nLen+1 );
	m_hEntity->SetValue( pAttributeName, pBuf );
}

void CCommentaryPropertiesPanel::TextEntriesToVector( vgui::TextEntry *pEntry[3], const char *pAttributeName )
{
	CUtlVectorFixedGrowable< char, 256 > buf;

	Vector vec;
	for ( int i = 0; i < 3; ++i )
	{
		int nLen = pEntry[i]->GetTextLength();
		buf.EnsureCount( nLen + 1 );
		pEntry[i]->GetText( buf.Base(), nLen+1 );
		vec[i] = atof( buf.Base() );
	}
	m_hEntity->SetValue( pAttributeName, vec );
	clienttools->MarkClientRenderableDirty( m_hEntity );
}


//-----------------------------------------------------------------------------
// Updates entity state when text fields change
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::UpdateCommentaryNode()
{
	if ( !m_hEntity.Get() )
		return;

	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Commentary Node Change", "Commentary Node Change" );
	TextEntryToAttribute( m_pNodeName, "targetname" );
	TextEntryToAttribute( m_pStartCommands, "precommands" );
	TextEntryToAttribute( m_pEndCommands, "postcommands" );
	TextEntryToAttribute( m_pViewPosition, "viewposition" );
	TextEntryToAttribute( m_pViewTarget, "viewtarget" );
	TextEntryToAttribute( m_pSynopsis, "synopsis" );
	TextEntryToAttribute( m_pSpeakerName, "speakers" );
	TextEntryToAttribute( m_pSoundFileName, "commentaryfile" );
	TextEntriesToVector( m_pPosition, "origin" );
	TextEntriesToVector( m_pOrientation, "angles" );
	m_hEntity->SetValue<int>( "prevent_movement", m_pPreventMovement->IsSelected() ? 1 : 0 );
	m_hEntity->MarkDirty();
}

void CCommentaryPropertiesPanel::UpdateInfoTarget()
{
	if ( !m_hEntity.Get() )
		return;

	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Info Target Change", "Info Target Change" );
	TextEntryToAttribute( m_pTargetName, "targetname" );
	TextEntriesToVector( m_pTargetPosition, "origin" );
	TextEntriesToVector( m_pTargetOrientation, "angles" );
	m_hEntity->MarkDirty();
}

void CCommentaryPropertiesPanel::UpdateInfoRemarkable()
{
	if ( !m_hEntity.Get() )
		return;

	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Info Remarkable Change", "Info Remarkable Change" );
	TextEntryToAttribute( m_pInfoRemarkableName, "targetname" );
	TextEntryToAttribute( m_pInfoRemarkableSubject, "contextsubject" );
	TextEntriesToVector( m_pRemarkablePosition, "origin" );
	m_hEntity->MarkDirty();
}

//-----------------------------------------------------------------------------
// Populates the commentary node fields
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::PopulateCommentaryNodeFields()
{
	if ( !m_hEntity.Get() )
		return;

	m_pNodeName->SetText( m_hEntity->GetTargetName() );
	m_pStartCommands->SetText( m_hEntity->GetValueString( "precommands" ) );
	m_pEndCommands->SetText( m_hEntity->GetValueString( "postcommands" ) );
	m_pViewPosition->SetText( m_hEntity->GetValueString( "viewposition" ) );
	m_pViewTarget->SetText( m_hEntity->GetValueString( "viewtarget" ) );
	m_pSynopsis->SetText( m_hEntity->GetValueString( "synopsis" ) );
	m_pSpeakerName->SetText( m_hEntity->GetValueString( "speakers" ) );
	m_pSoundFileName->SetText( m_hEntity->GetValueString( "commentaryfile" ) );

	Vector vecPosition = m_hEntity->GetRenderOrigin();
	QAngle vecAngles = m_hEntity->GetRenderAngles();

	for ( int i = 0; i < 3; ++i )
	{
		char pTemp[512];
		Q_snprintf( pTemp, sizeof(pTemp), "%.2f", vecPosition[i] );
		m_pPosition[i]->SetText( pTemp );

		Q_snprintf( pTemp, sizeof(pTemp), "%.2f", vecAngles[i] );
		m_pOrientation[i]->SetText( pTemp );
	}

	m_pPreventMovement->SetSelected( m_hEntity->GetValue<int>( "prevent_movement" ) != 0 );
}


//-----------------------------------------------------------------------------
// Populates the info_target fields
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::PopulateInfoTargetFields()
{
	if ( !m_hEntity.Get() )
		return;

	m_pTargetName->SetText( m_hEntity->GetTargetName() );

	Vector vecPosition = m_hEntity->GetRenderOrigin();
	QAngle vecAngles = m_hEntity->GetRenderAngles();

	for ( int i = 0; i < 3; ++i )
	{
		char pTemp[512];
		Q_snprintf( pTemp, sizeof(pTemp), "%.2f", vecPosition[i] );
		m_pTargetPosition[i]->SetText( pTemp );

		Q_snprintf( pTemp, sizeof(pTemp), "%.2f", vecAngles[i] );
		m_pTargetOrientation[i]->SetText( pTemp );
	}
}




//-----------------------------------------------------------------------------
// Populates the info_target fields
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::PopulateInfoRemarkableFields()
{
	if ( !m_hEntity.Get() )
		return;

	m_pInfoRemarkableName->SetText( m_hEntity->GetTargetName() );

	Vector vecPosition = m_hEntity->GetRenderOrigin();
	QAngle vecAngles = m_hEntity->GetRenderAngles();

	for ( int i = 0; i < 3; ++i )
	{
		char pTemp[512];
		Q_snprintf( pTemp, sizeof(pTemp), "%.2f", vecPosition[i] );
		m_pRemarkablePosition[i]->SetText( pTemp );
	}

	m_pInfoRemarkableSubject->SetText( m_hEntity->GetValueString( "contextsubject" ) );
}


//-----------------------------------------------------------------------------
// Sets the object to look at
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::SetObject( CDmeCommentaryNodeEntity *pEntity )
{
	m_hEntity = pEntity;
	m_pCommentaryNodeScroll->SetVisible( false );
	m_pInfoTargetScroll->SetVisible( false );
	m_pInfoRemarkableScroll->SetVisible( false );

	if ( pEntity )
	{
		if ( !Q_stricmp( pEntity->GetClassName(), "info_target" ) )
		{
			PopulateInfoTargetFields();
			m_pInfoTargetScroll->SetVisible( true );
			m_pTargetName->RequestFocus();
			return;
		}

		if ( !Q_stricmp( pEntity->GetClassName(), "info_remarkable" ) )
		{
			PopulateInfoRemarkableFields();
			m_pInfoRemarkableScroll->SetVisible( true );
			m_pInfoRemarkableName->RequestFocus();
			return;
		}

		if ( !Q_stricmp( pEntity->GetClassName(), "point_commentary_node" ) )
		{
			PopulateCommentaryNodeFields();
			m_pCommentaryNodeScroll->SetVisible( true );
			m_pNodeName->RequestFocus();
			return;
		}
	}
}

	
//-----------------------------------------------------------------------------
// Called when text is changed
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::OnTextChanged( KeyValues *pParams )
{
	vgui::Panel *pPanel = (vgui::Panel*)pParams->GetPtr( "panel" );
	if ( pPanel->GetParent() == m_pCommentaryNode )
	{
		UpdateCommentaryNode();
		return;
	}

	if ( pPanel->GetParent() == m_pInfoTarget )
	{
		UpdateInfoTarget();
		return;
	}

	if ( pPanel->GetParent() == m_pInfoRemarkable )
	{
		UpdateInfoRemarkable();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when the audio picker has picked something
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::OnSoundSelected( KeyValues *pParams )
{
	const char *pAssetName = pParams->GetString( "wav" );
	m_pSoundFileName->SetText( pAssetName );
	UpdateCommentaryNode();
}


//-----------------------------------------------------------------------------
// Called when the audio picker button is selected
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::PickSound()
{
	CSoundPickerFrame *pSoundPickerDialog = new CSoundPickerFrame( g_pCommEditTool->GetRootPanel(), "Select commentary audio file", CSoundPicker::PICK_WAVFILES );
	pSoundPickerDialog->AddActionSignalTarget( this );
	pSoundPickerDialog->DoModal( CSoundPicker::PICK_NONE, NULL );
}

			
//-----------------------------------------------------------------------------
// Called when the string picker has picked something
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::OnPicked( KeyValues *pParams )
{
	const char *pInfoTargetName = pParams->GetString( "choice" );
	KeyValues *pContextKeyValues = pParams->FindKey( "context" );
	vgui::TextEntry *pTextEntry = (vgui::TextEntry *)pContextKeyValues->GetPtr( "widget" );
	pTextEntry->SetText( pInfoTargetName );
	UpdateCommentaryNode();
}


//-----------------------------------------------------------------------------
// Called when the audio picker button is selected
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::PickInfoTarget( vgui::TextEntry *pControl )
{
	CDmrCommentaryNodeEntityList entities( m_pDoc->GetEntityList() );
	int nCount = entities.Count();
	PickerList_t vec( 0, nCount+1 );
	int j = vec.AddToTail( );
	vec[j].m_pChoiceString = "<no target>";
	vec[j].m_pChoiceValue = "";
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeCommentaryNodeEntity *pNode = entities[ i ];
		const char *pTargetName = pNode->GetTargetName();
		if ( !pTargetName || !pTargetName[0] )
			continue;

		if ( !Q_stricmp( pNode->GetClassName(), "info_target" ) )
		{
			j = vec.AddToTail( ); 
			vec[j].m_pChoiceString = pTargetName;
			vec[j].m_pChoiceValue = pTargetName;
		}
	}

	CPickerFrame *pInfoTargetPickerDialog = new CPickerFrame( g_pCommEditTool->GetRootPanel(), "Select Target", "InfoTarget", NULL );
	KeyValues *pContextKeyValues = new KeyValues( "context" );
	pContextKeyValues->SetPtr( "widget", pControl );
	pInfoTargetPickerDialog->AddActionSignalTarget( this );
	pInfoTargetPickerDialog->DoModal( vec, pContextKeyValues );
}


//-----------------------------------------------------------------------------
// Called when a sound is successfully recorded
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::OnSoundRecorded( KeyValues *pKeyValues )
{
	const char *pFileName = pKeyValues->GetString( "relativepath" );
	m_pSoundFileName->SetText( pFileName );
	UpdateCommentaryNode();
}


//-----------------------------------------------------------------------------
// Used to open a particular file in perforce, and deal with all the lovely dialogs
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::OnFileSelected( KeyValues *pKeyValues )
{
	const char *pFileName = pKeyValues->GetString( "fullpath" );
	if ( g_pFileSystem->FileExists( pFileName ) )
	{
		char pBuf[1024];
		Q_snprintf( pBuf, sizeof(pBuf), "File %s already exists!\nRecording audio will not overwrite existing files.", pFileName ); 
		vgui::MessageBox *pMessageBox = new vgui::MessageBox( "File already Exists!\n", pBuf, g_pCommEditTool );
		pMessageBox->DoModal( );
		return;
	}

	CSoundRecordPanel *pSoundRecordPanel = new CSoundRecordPanel( g_pCommEditTool->GetRootPanel(), "Record Commentary" );
	pSoundRecordPanel->AddActionSignalTarget( this );
	pSoundRecordPanel->DoModal( pFileName );
}


//-----------------------------------------------------------------------------
// Called when sound recording is requested
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::RecordSound( )
{
	char pStartingDir[ MAX_PATH ];
	GetModSubdirectory( "sound", pStartingDir, sizeof(pStartingDir) );

	vgui::FileOpenDialog *pDialog = new vgui::FileOpenDialog( this, "Save As", false );
	pDialog->SetTitle( "Enter New Audio File", true );
	pDialog->SetStartDirectoryContext( "commedit_audio_record", pStartingDir );
	pDialog->AddFilter( "*.wav", "Audio File (*.wav)", true );
	pDialog->AddActionSignalTarget( this );
	pDialog->DoModal( true );
}


//-----------------------------------------------------------------------------
// Called when buttons are clicked
//-----------------------------------------------------------------------------
void CCommentaryPropertiesPanel::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "PickSound" ) )
	{
		PickSound();
		return;
	}

	if ( !Q_stricmp( pCommand, "Record" ) )
	{
		RecordSound();
		return;
	}

	if ( !Q_stricmp( pCommand, "PickViewPosition" ) )
	{
		PickInfoTarget( m_pViewPosition );
		return;
	}

	if ( !Q_stricmp( pCommand, "PickViewTarget" ) )
	{
		PickInfoTarget( m_pViewTarget );
		return;
	}

	if ( !Q_stricmp( pCommand, "PreventMovementClicked" ) )
	{
		UpdateCommentaryNode();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


