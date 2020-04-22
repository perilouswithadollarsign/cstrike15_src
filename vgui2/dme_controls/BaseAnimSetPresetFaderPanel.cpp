//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"
#include "dme_controls/DmePresetGroupEditorPanel.h"
#include "vgui_controls/InputDialog.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/TextEntry.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/PanelListPanel.h"
#include "movieobjects/dmeanimationset.h"
#include "tier1/KeyValues.h"
#include "dme_controls/dmecontrols_utils.h"
#include "vstdlib/random.h"
#include "vgui/IVgui.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "dme_controls/BaseAnimationSetEditor.h"
#include "movieobjects/dmetransform.h"
#include "vstdlib/jobthread.h"
#include "tier1/utlsymbollarge.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

class CPresetSlider;


const int PRESET_SLIDER_INIT = 400;

//-----------------------------------------------------------------------------
// preset and preset group name collection utilities
//-----------------------------------------------------------------------------
CDmePresetGroup *FindAnyPresetGroup( CDmeFilmClip *pFilmClip, const char *pPresetGroupName )
{
	CAnimSetGroupAnimSetTraversal traversal( pFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( pPresetGroup )
			return pPresetGroup;
	}
	return NULL;
}

CDmePreset *FindAnyPreset( CDmeFilmClip *pFilmClip, const char *pPresetGroupName, const char *pPresetName )
{
	CAnimSetGroupAnimSetTraversal traversal( pFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;

		CDmePreset *pPreset = pPresetGroup->FindPreset( pPresetName );
		if ( pPreset )
			return pPreset;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Utility dialog, used to let user type in some text
//-----------------------------------------------------------------------------
class CAddPresetDialog : public vgui::BaseInputDialog
{
	DECLARE_CLASS_SIMPLE( CAddPresetDialog, vgui::BaseInputDialog );

public:
	CAddPresetDialog( vgui::Panel *parent );

	void DoModal( CDmeFilmClip *pFilmClip, const char *pCurrentGroupName, KeyValues *pContextKeyValues = NULL );

protected:
	// command buttons
	virtual void OnCommand(const char *command);
	virtual void OnTick();

private:
	vgui::TextEntry		*m_pPresetName;
	vgui::ComboBox		*m_pPresetGroup;
	vgui::CheckButton	*m_pAnimated;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAddPresetDialog::CAddPresetDialog( vgui::Panel *parent ) : BaseClass( parent, "Enter Preset Name" )
{
	m_pPresetName = new TextEntry( this, "PresetName" );
	m_pPresetGroup = new vgui::ComboBox( this, "PresetGroup", 8, true );
	m_pAnimated = new vgui::CheckButton( this, "Animated", "Animated" );
	SetDeleteSelfOnClose( false );

	vgui::ivgui()->AddTickSignal( GetVPanel(), 0 );

	LoadControlSettings( "resource/addpresetdialog.res" );
}


void CAddPresetDialog::DoModal( CDmeFilmClip *pFilmClip, const char *pCurrentGroupName, KeyValues *pContextKeyValues )
{
	m_pPresetGroup->DeleteAllItems();

	CUtlVector< PresetGroupInfo_t > presetGroupInfo;
	CollectPresetGroupInfo( pFilmClip, presetGroupInfo, true );

	int nPresetGroups = presetGroupInfo.Count();
	for ( int i = 0; i < nPresetGroups; ++i )
	{
		const char *pPresetGroupName = presetGroupInfo[ i ].presetGroupSym.String();

		KeyValues *kv = new KeyValues( "entry" );
		kv->SetString( "presetGroupName", pPresetGroupName );

		int nItemID = m_pPresetGroup->AddItem( pPresetGroupName, kv );
		if ( pCurrentGroupName && !Q_stricmp( pPresetGroupName, pCurrentGroupName ) )
		{
			m_pPresetGroup->ActivateItem( nItemID );
		}
	}

	BaseClass::DoModal( pContextKeyValues );

	m_pPresetName->SetText( "" );
	m_pPresetName->RequestFocus();

	PlaceUnderCursor( );
}


//-----------------------------------------------------------------------------
// command handler
//-----------------------------------------------------------------------------
void CAddPresetDialog::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "OK" ) )
	{
		int nTextLength = m_pPresetName->GetTextLength() + 1;
		char* txt = (char*)_alloca( nTextLength * sizeof(char) );
		m_pPresetName->GetText( txt, nTextLength );

		nTextLength = m_pPresetGroup->GetTextLength() + 1;
		char* pPresetGroupName = (char*)_alloca( nTextLength * sizeof(char) );
		m_pPresetGroup->GetText( pPresetGroupName, nTextLength );

		bool bAnimated = m_pAnimated->IsSelected();

		KeyValues *kv = new KeyValues( "PresetNameSelected", "text", txt );
		kv->SetString( "presetGroupName", pPresetGroupName );
		kv->SetBool( "animated", bAnimated );

		if ( m_pContextKeyValues )
		{
			kv->AddSubKey( m_pContextKeyValues );
			m_pContextKeyValues = NULL;
		}
		PostActionSignal( kv );
		CloseModal();
		return;
	}

	if ( !Q_stricmp( command, "Cancel") )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( command );
}

void CAddPresetDialog::OnTick()
{
	bool bEnableOkayButton = m_pPresetName->GetTextLength() > 0 && m_pPresetGroup->GetTextLength() > 0;
	m_pOKButton->SetEnabled( bEnableOkayButton );

	BaseClass::OnTick();
}


//-----------------------------------------------------------------------------
//
// CPresetSlider: The actual preset slider itself!
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Static members
//-----------------------------------------------------------------------------
bool CPresetSlider::s_bResetMousePosOnMouseUp = false;
int CPresetSlider::s_nMousePosX;
int CPresetSlider::s_nMousePosY;


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CPresetSlider::CPresetSlider( Panel *parent, CBaseAnimSetPresetFaderPanel *pFaderPanel ) :
	BaseClass( (Panel *)parent, "preset" ), 
	m_pPresetFaderPanel( pFaderPanel ), 
	m_AttributeLookup( DefLessFunc( DmElementHandle_t ) ), 
	m_bIgnoreCursorMovedEvents( false ),
	m_presetGroupName( UTL_INVAL_SYMBOL_LARGE ),
	m_presetName( UTL_INVAL_SYMBOL_LARGE ),
	m_nProceduralType( PROCEDURAL_PRESET_NOT ),
	m_bReadOnly( false )
{
	SetRange( 0, 1000 );
	SetDragOnRepositionNob( true );
	SetPaintBackgroundEnabled( true );

	m_pName = new TextImage( "" );

	m_ZeroColor = Color( 69, 69, 69, 255 );
	m_GradientColor = Color( 194, 120, 0, 255 );

	m_TextColor = Color( 200, 200, 200, 255 );
	m_TextColorFocus = Color( 208, 143, 40, 255 );
}

CPresetSlider::~CPresetSlider()
{
	delete m_pName;
}

void CPresetSlider::Clear()
{
	m_nProceduralType = PROCEDURAL_PRESET_NOT;
	m_bReadOnly = false;
	m_presetGroupName = m_presetName = UTL_INVAL_SYMBOL_LARGE;
	m_AttributeLookup.RemoveAll();
}

void CPresetSlider::Init( const char *pPresetGroupName, const char *pPresetName )
{
	m_nProceduralType = PROCEDURAL_PRESET_NOT;
	m_bReadOnly = false;

	m_presetGroupName = g_pDataModel->GetSymbol( pPresetGroupName );
	m_presetName      = g_pDataModel->GetSymbol( pPresetName );

	static const CUtlSymbolLarge proceduralPresetGroupNameSym = g_pDataModel->GetSymbol( PROCEDURAL_PRESET_GROUP_NAME );
	if ( m_presetGroupName == proceduralPresetGroupNameSym )
	{
		m_nProceduralType = ProceduralTypeForPresetName( pPresetName );
		m_bReadOnly = true;
	}
	else
	{
		// HACK - we're assuming here that presets of a given name share their type and readonly values across all animationsets
		bool bFound = false;
		CAnimSetGroupAnimSetTraversal traversal( m_pPresetFaderPanel->GetController()->GetAnimationSetClip() );
		while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
		{
			CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
			if ( !pPresetGroup )
				continue;

			CDmePreset *pPreset = pPresetGroup->FindPreset( pPresetName );
			if ( !pPreset )
				continue;

			m_bReadOnly = pPreset->IsReadOnly();
			bFound = true;
			break;
		}
		Assert( bFound );
	}

	SetName( pPresetName );
	m_pName->SetText( pPresetName );
	m_pName->ResizeImageToContent();

	SetBgColor( Color( 128, 128, 128, 128 ) );
}

const char *CPresetSlider::GetPresetName()
{
	return m_presetName.String();
}

const char *CPresetSlider::GetPresetGroupName()
{
	return m_presetGroupName.String();
}

//-----------------------------------------------------------------------------
// Reads the sliders, sets control values into the attribute dictionary
//-----------------------------------------------------------------------------
void CPresetSlider::SetControlValues()
{
	m_AttributeLookup.RemoveAll();

	if ( GetProceduralPresetType() != PROCEDURAL_PRESET_NOT )
		return; // this will be taken care of in the application

	CAnimSetGroupAnimSetTraversal traversal( m_pPresetFaderPanel->GetController()->GetAnimationSetClip() );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( GetPresetGroupName() );
		if ( !pPresetGroup )
			continue;

		CDmePreset *pPreset = pPresetGroup->FindPreset( GetPresetName() );
		if ( !pPreset )
			continue;

		CDmrElementArray< CDmElement > values = pPreset->GetControlValues();

		int nControlValueCount = values.Count();
		m_AttributeLookup.EnsureCapacity( m_AttributeLookup.Count() + nControlValueCount );
		for ( int i = 0; i < nControlValueCount; ++i )
		{
			CDmElement *v = values[ i ];
			if ( !v )
				continue;

			CDmElement *pControl = pAnimSet->FindControl( v->GetName() );
			if ( !pControl )
				continue;

			DmElementHandle_t handle = pControl->GetHandle();
			int idx = m_AttributeLookup.Find( handle );
			if ( idx == m_AttributeLookup.InvalidIndex() )
			{
				idx = m_AttributeLookup.Insert( handle );
			}
			AnimationControlAttributes_t &val = m_AttributeLookup[ idx ];

			// Zero everything out
			val.Clear();

			CDmAttribute *pValueAttribute = v->GetAttribute( "value" );
			if ( pValueAttribute )
			{
				val.m_pValueAttribute[ANIM_CONTROL_VALUE] = pValueAttribute;
				val.m_pValue         [ANIM_CONTROL_VALUE] = pValueAttribute->GetValue< float >(); 
			}
			else
			{
				val.m_pValueAttribute[ANIM_CONTROL_VALUE] = v->GetAttribute( AS_VALUES_ATTR );
				val.m_pTimesAttribute[ANIM_CONTROL_VALUE] = v->GetAttribute( AS_TIMES_ATTR );
			}

			CDmAttribute *pLeftValueAttribute = v->GetAttribute( "leftValue" );
			if ( pLeftValueAttribute )
			{
				val.m_pValueAttribute[ANIM_CONTROL_VALUE_LEFT] = pLeftValueAttribute;
				val.m_pValue         [ANIM_CONTROL_VALUE_LEFT] = pLeftValueAttribute->GetValue< float >();
			}
			else
			{
				val.m_pValueAttribute[ANIM_CONTROL_VALUE_LEFT] = v->GetAttribute( AS_VALUES_LEFT_ATTR );
				val.m_pTimesAttribute[ANIM_CONTROL_VALUE_LEFT] = v->GetAttribute( AS_TIMES_LEFT_ATTR );
			}

			CDmAttribute *pRightValueAttribute = v->GetAttribute( "rightValue" );
			if ( pRightValueAttribute )
			{
				val.m_pValueAttribute[ANIM_CONTROL_VALUE_RIGHT] = pRightValueAttribute;
				val.m_pValue         [ANIM_CONTROL_VALUE_RIGHT] = pRightValueAttribute->GetValue< float >();
			}
			else
			{
				val.m_pValueAttribute[ANIM_CONTROL_VALUE_RIGHT] = v->GetAttribute( AS_VALUES_RIGHT_ATTR );
				val.m_pTimesAttribute[ANIM_CONTROL_VALUE_RIGHT] = v->GetAttribute( AS_TIMES_RIGHT_ATTR );
			}

			CDmAttribute *pPositionAttribute = v->GetAttribute( "valuePosition" );
			if ( pPositionAttribute )
			{
				val.m_pValueAttribute[ANIM_CONTROL_TXFORM_POSITION] = pPositionAttribute;
				val.m_Vector = pPositionAttribute->GetValue< Vector >();
			}
			else
			{
				val.m_pValueAttribute[ANIM_CONTROL_TXFORM_POSITION] = v->GetAttribute( AS_VALUES_POSITION_ATTR );
				val.m_pTimesAttribute[ANIM_CONTROL_TXFORM_POSITION] = v->GetAttribute( AS_TIMES_POSITION_ATTR );
			}

			CDmAttribute *pQuaternionAttribute = v->GetAttribute( "valueOrientation" );
			if ( pQuaternionAttribute )
			{
				val.m_pValueAttribute[ANIM_CONTROL_TXFORM_ORIENTATION] = pQuaternionAttribute;
				val.m_Quaternion = pQuaternionAttribute->GetValue< Quaternion >();
			}
			else
			{
				val.m_pValueAttribute[ANIM_CONTROL_TXFORM_ORIENTATION] = v->GetAttribute( AS_VALUES_ORIENTATION_ATTR );
				val.m_pTimesAttribute[ANIM_CONTROL_TXFORM_ORIENTATION] = v->GetAttribute( AS_TIMES_ORIENTATION_ATTR );
			}
		}
	}
}


AttributeDict_t *CPresetSlider::GetAttributeDict()
{
	return &m_AttributeLookup;
}

void CPresetSlider::OnMousePressed(MouseCode code)
{
	m_bIgnoreCursorMovedEvents = false; // reset this just in case it was left on

	m_pPresetFaderPanel->SetActivePresetSlider( this );

	if ( code != MOUSE_LEFT )
		return;

	if ( _dragging )
	{
		OnMouseReleased( MOUSE_LEFT );
		return;
	}

	BaseClass::OnMousePressed( code );

	if ( !_dragging )
		return;

	SetCursor( dc_blank );
	int mx, my;
	input()->GetCursorPos( mx, my );
	int tx, ty, tw, th;
	GetTrackRect( tx, ty, tw, th );

	LocalToScreen( tx, ty );

	if ( mx < tx + tw )
	{
		if ( mx >= tx )
		{
			Assert( !s_bResetMousePosOnMouseUp );
			s_bResetMousePosOnMouseUp = true;
			s_nMousePosX = mx;
			s_nMousePosY = my;

			input()->SetCursorPos( tx, my );
		}
		SetPos( 0 );
	}
}

void CPresetSlider::OnMouseReleased(MouseCode code)
{
	m_bIgnoreCursorMovedEvents = false; // reset this just in case it was left on

	if ( code == MOUSE_RIGHT )
	{
		OnShowContextMenu();
		return;
	}

	if ( code != MOUSE_LEFT )
		return;

	float flLastValue = GetCurrent();
	bool bWasDragging = _dragging;
	BaseClass::OnMouseReleased( code );
	if ( bWasDragging )
	{
		m_pPresetFaderPanel->GetController()->ApplyPreset( flLastValue, m_AttributeLookup );
		SetCursor( dc_arrow );
	}

	if ( s_bResetMousePosOnMouseUp )
	{
		s_bResetMousePosOnMouseUp = false;
		input()->SetCursorPos( s_nMousePosX, s_nMousePosY );
	}
}

void CPresetSlider::OnCursorEntered()
{
	if ( _dragging )
		return;

	m_pPresetFaderPanel->SetActivePresetSlider( this );
}

void CPresetSlider::OnCursorExited()
{
	if ( _dragging )
		return;

	m_pPresetFaderPanel->SetActivePresetSlider( NULL );
}

void CPresetSlider::OnRename()
{
	InputDialog *pDialog = new InputDialog( this, "Rename Preset", "Name:", GetName() );
	Assert( pDialog );
	if ( pDialog )
	{
		pDialog->SetSmallCaption( true );
		pDialog->SetMultiline( false );
		pDialog->DoModal( new KeyValues( "RenamePreset" ) );
	}
}

void CPresetSlider::OnRenameCompleted( const char *pText, KeyValues *pContextKeyValues )
{
	if ( !pText || !*pText )
	{
		Warning( "Can't rename preset for %s to an empty name\n", GetName() );
		return;
	}

	// No change( case sensitive)
	if ( !Q_strcmp( GetName(), pText ) )
		return;

	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Rename Preset" );

	SetName( pText );
	m_pName->SetText( pText );
	m_pName->ResizeImageToContent();

	m_presetName = g_pDataModel->GetSymbol( pText );

	CAnimSetGroupAnimSetTraversal traversal( m_pPresetFaderPanel->GetController()->GetAnimationSetClip() );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( GetPresetGroupName() );
		Assert( pPresetGroup );
		CDmePreset *pPreset = pPresetGroup->FindPreset( GetPresetName() );
		Assert( pPreset );

		pPreset->SetName( pText );
	}
}

void CPresetSlider::OnInputCompleted( KeyValues *pParams )
{
	const char *pText = pParams->GetString( "text", NULL );

	KeyValues *pContextKeyValues = pParams->FindKey( "RenamePreset" );
	if ( pContextKeyValues )
	{
		OnRenameCompleted( pText, pContextKeyValues );
		return;
	}

	Assert( 0 );
}

void CPresetSlider::OnDelete()
{
	char sz[ 256 ];
	Q_snprintf( sz, sizeof( sz ), "Delete '%s'?", GetName() );

	MessageBox *pConfirm = new MessageBox( "Delete Preset", sz, this );
	Assert( pConfirm );
	if ( pConfirm )
	{
		pConfirm->SetCancelButtonVisible( true );
		pConfirm->SetCancelButtonText( "#VGui_Cancel" );
		pConfirm->SetCommand( new KeyValues( "OnDeleteConfirmed" ) );
		pConfirm->AddActionSignalTarget( this );
		pConfirm->DoModal();
	}
}

void CPresetSlider::OnDeleteConfirmed()
{
	m_pPresetFaderPanel->OnDeletePreset( GetPresetName() );
}

void CPresetSlider::OnShowContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}

	m_hContextMenu = new Menu( this, "ActionMenu" );

	if ( !m_bReadOnly )
	{
		m_hContextMenu->AddMenuItem( "Rename...", new KeyValues( "OnRename" ), this );
		if ( Q_stricmp( GetName(), "Default" ) )
		{
			m_hContextMenu->AddMenuItem( "Delete...", new KeyValues( "OnDelete" ), this );
		}

		m_hContextMenu->AddSeparator();
	}

	int nItem = m_hContextMenu->AddMenuItem( "Add...", new KeyValues( "ShowAddPresetDialog" ), m_pPresetFaderPanel );
	if ( m_pPresetFaderPanel->GetController()->GetMostRecentlySelectedControl() == NULL )
	{
		m_hContextMenu->SetItemEnabled( nItem, false );
	}

	m_hContextMenu->AddMenuItem( "Manage...", new KeyValues( "ManagePresets" ), m_pPresetFaderPanel );

	Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}

void CPresetSlider::UpdateTickPos( int x, int y )
{
	if ( m_bIgnoreCursorMovedEvents )
		return;

	int tx, ty, tw, th;
	GetTrackRect( tx, ty, tw, th );

	float previewValue = 0.0f;
	if ( x > tx )
	{
		if ( x >= ( tx + tw ) || tw <= 0 )
		{
			previewValue = 1.0f;
		}
		else
		{
			previewValue = (float)( x - tx ) / (float)tw;
		}
	}

	SetPos( previewValue );
}

void CPresetSlider::OnCursorMoved(int x, int y)
{
	UpdateTickPos( x, y );
}

bool CPresetSlider::IsDragging()
{
	return _dragging;
}

void CPresetSlider::IgnoreCursorMovedEvents( bool bIgnore )
{
	m_bIgnoreCursorMovedEvents = bIgnore;
}

void CPresetSlider::Deactivate()
{
	if ( m_pPresetFaderPanel->GetActivePresetSlider() == this )
	{
		m_pPresetFaderPanel->SetActivePresetSlider( NULL );
	}
}

float CPresetSlider::GetCurrent()
{
	return GetValue() * 0.001f;
}

void CPresetSlider::SetPos( float frac )
{
	SetValue( (int)( frac * 1000.0f + 0.5f ), false );
}

void CPresetSlider::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pName->SetFont( scheme->GetFont( "DefaultBold" ) );
	m_pName->SetColor( m_TextColor );
	m_pName->ResizeImageToContent();

	SetFgColor( Color( 194, 120, 0, 255 ) );
	SetThumbWidth( 3 );
}

void CPresetSlider::GetTrackRect( int &x, int &y, int &w, int &h )
{
	GetSize( w, h );
	x = 2;
	y = 2;
	w -= 4;
	h -= 4;
}

void CPresetSlider::Paint()
{
	if ( m_pPresetFaderPanel->GetActivePresetSlider() != this )
		return;

	bool bIsShiftKeyDown = vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
	if ( !IsDragging() && !bIsShiftKeyDown )
		return;

	int mx, my;
	input()->GetCursorPos( mx, my );
	ScreenToLocal( mx, my );
	UpdateTickPos( mx, my );

	// horizontal nob
	int x, y;
	int wide,tall;
	GetTrackRect( x, y, wide, tall );

	Color col = GetFgColor();
	surface()->DrawSetColor( col );
	surface()->DrawFilledRect( _nobPos[0], 1, _nobPos[1], GetTall() - 1 );
 	surface()->DrawSetColor( m_ZeroColor );
 	surface()->DrawFilledRect( _nobPos[0] - 1, y + 1,  _nobPos[0], y + tall - 1 );
}

void CPresetSlider::PaintBackground()
{
	int w, h;
	GetSize( w, h );

	bool hasFocus = m_pPresetFaderPanel->GetActivePresetSlider() == this;
	bool bIsShiftKeyDown = vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT );
	bool bIsAltKeyDown = vgui::input()->IsKeyDown( KEY_LALT ) || vgui::input()->IsKeyDown( KEY_RALT );
	if ( hasFocus && ( IsDragging() || bIsShiftKeyDown ) )
	{   
		int tx, ty, tw, th;
		GetTrackRect( tx, ty, tw, th );

		surface()->DrawSetColor( Color( 0, 0, 0,  255 ) );
		surface()->DrawOutlinedRect( tx, ty, tx + tw, ty + th );
		surface()->DrawSetColor( m_GradientColor );

		++tx;
		++ty;
		tw -= 2;
		th -= 2;

		// Gradient fill rectangle
		int fillw = (int)( (float)tw * GetCurrent() + 0.5f );

		int minAlpha = 15;
		float alphaTarget = 255.0f;

		int curAlpha = MAX( GetCurrent() * alphaTarget, minAlpha );
		 
		if ( _dragging )
		{
			surface()->DrawFilledRectFade( tx, ty, tx + fillw, ty + th, minAlpha, curAlpha, true );
			surface()->DrawSetColor( m_ZeroColor );
			surface()->DrawFilledRect( tx + fillw + 1, ty, tx + tw, ty + th );
		}
		else
		{												
			surface()->DrawSetColor( bIsAltKeyDown ? m_GradientColor : m_ZeroColor );
			surface()->DrawFilledRect( tx, ty, tx + tw, ty + th );
		}
	}

	int cw, ch;
	m_pName->SetColor( hasFocus ? m_TextColorFocus : m_TextColor );
	m_pName->GetContentSize( cw, ch );
	m_pName->SetPos( ( w - cw ) * 0.5f, ( h - ch ) * 0.5f );
	m_pName->Paint();
}

//-----------------------------------------------------------------------------
// Manipulate in/out curve types
//-----------------------------------------------------------------------------
void CPresetSlider::OnCurve1()
{
	m_pPresetFaderPanel->DispatchCurve( 1 );
}

void CPresetSlider::OnCurve2()
{
	m_pPresetFaderPanel->DispatchCurve( 2 );
}

void CPresetSlider::OnCurve3()
{
	m_pPresetFaderPanel->DispatchCurve( 3 );
}

void CPresetSlider::OnCurve4()
{
	m_pPresetFaderPanel->DispatchCurve( 4 );
}

//-----------------------------------------------------------------------------
// Slider list panel
//-----------------------------------------------------------------------------
class CSliderListPanel : public PanelListPanel
{
	DECLARE_CLASS_SIMPLE( CSliderListPanel, vgui::PanelListPanel );

public:
	CSliderListPanel( CBaseAnimSetPresetFaderPanel *parent, vgui::Panel *pParent, const char *panelName );

	virtual void OnMouseReleased( vgui::MouseCode code );
	virtual void OnMousePressed( vgui::MouseCode code );

private:
	MESSAGE_FUNC( OnShowContextMenu, "OnShowContextMenu" );
	vgui::DHANDLE< vgui::Menu >	m_hContextMenu;
	CBaseAnimSetPresetFaderPanel *m_pPresetFaderPanel;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CSliderListPanel::CSliderListPanel( CBaseAnimSetPresetFaderPanel *pFader, vgui::Panel *pParent, const char *panelName ) :
	BaseClass( pParent, panelName )
{
	m_pPresetFaderPanel = pFader;
}


//-----------------------------------------------------------------------------
// Context menu!
//-----------------------------------------------------------------------------
void CSliderListPanel::OnMousePressed( MouseCode code )
{
	if ( code == MOUSE_RIGHT )
		return;

	BaseClass::OnMousePressed( code );
}

void CSliderListPanel::OnMouseReleased( MouseCode code )
{
	if ( code == MOUSE_RIGHT )
	{
		OnShowContextMenu();
		return;
	}

	BaseClass::OnMouseReleased( code );
}


//-----------------------------------------------------------------------------
// Shows the slider context menu
//-----------------------------------------------------------------------------
void CSliderListPanel::OnShowContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}

	m_hContextMenu = new Menu( this, "ActionMenu" );

	int nItem = m_hContextMenu->AddMenuItem( "Add...", new KeyValues( "ShowAddPresetDialog" ), m_pPresetFaderPanel );
	if ( m_pPresetFaderPanel->GetController()->GetMostRecentlySelectedControl() == NULL )
	{
		m_hContextMenu->SetItemEnabled( nItem, false );
	}
	m_hContextMenu->AddMenuItem( "Manage...", new KeyValues( "ManagePresets" ), m_pPresetFaderPanel );

	Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBaseAnimSetPresetFaderPanel::CBaseAnimSetPresetFaderPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor ) :
	BaseClass( parent, className )
{
	m_pController = editor->GetController();

	m_pSheet = new PropertySheet( this, "presetPropertySheet" );
	m_pSheet->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
	m_pSheet->SetSmallTabs( true );

	m_pSliders = new CSliderListPanel( this, NULL, "PresetSliders" );
	m_pSliders->SetFirstColumnWidth( 0 );
	m_pSliders->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
	m_pSliders->SetPos( 0, 0 );
	m_pSliders->SetVerticalBufferPixels( 0 );

	// Prepopulate it with a few
	m_presetSliders.EnsureCapacity( PRESET_SLIDER_INIT );
	for ( int i = 0; i < PRESET_SLIDER_INIT; ++i )
	{
		m_presetSliders.AddToTail( new CPresetSlider( NULL, this ) );
	}
}

void CBaseAnimSetPresetFaderPanel::AddPreset( const char *pPresetGroupName, const char *pPresetName, bool bAnimated )
{
	if ( !pPresetName || !*pPresetName )
	{
		vgui::MessageBox *pError = new MessageBox( "Add Preset Error", "Can't add preset with an empty name\n", this );
		pError->SetDeleteSelfOnClose( true );
		pError->DoModal();
		return;
	}

	CDmeFilmClip *pFilmClip = m_pController->GetAnimationSetClip();
	if ( !pFilmClip )
		return;

	CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		if ( CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName ) )
		{
			SelectionState_t selection = m_pController->GetSelectionState( pAnimSet );
			if ( selection == SEL_NONE || selection == SEL_EMPTY )
				continue; // we're not adding a preset to this animationset, so no checks necessary...

			if ( pPresetGroup->m_bIsReadOnly )
			{
				vgui::MessageBox *pError = new MessageBox( "Add Preset Error", "Can't add preset to a read-only preset group!\n", this );
				pError->SetDeleteSelfOnClose( true );
				pError->DoModal();
				return;
			}

			if ( pPresetGroup->FindPreset( pPresetName ) != NULL )
			{
				vgui::MessageBox *pError = new MessageBox( "Add Preset Error", "A preset with that name already exists!\n", this );
				pError->SetDeleteSelfOnClose( true );
				pError->DoModal();
				return;
			}
		}
	}

	m_pController->AddPreset( pPresetGroupName, pPresetName, bAnimated );
}

//-----------------------------------------------------------------------------
// Called from the add preset name dialogs used by this class
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnPresetNameSelected( KeyValues *pParams )
{
	const char *pPresetName = pParams->GetString( "text", NULL );
	const char *pPresetGroupName = pParams->GetString( "presetGroupName" );
	bool bAnimated = pParams->GetBool( "animated" );

	AddPreset( pPresetGroupName, pPresetName, bAnimated );
	PopulatePresetList( false );
}


//-----------------------------------------------------------------------------
// The 'add preset' context menu option
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnShowAddPresetDialog()
{
	if ( !m_pController->GetAnimationSetClip() )
		return;

	CAddPresetDialog *pAddPresetDialog = new CAddPresetDialog( this );
	pAddPresetDialog->AddActionSignalTarget( this );

	const char *pCurrentGroupName = m_pSheet->GetActivePage() ? m_pSheet->GetActivePage()->GetName() : "";

	pAddPresetDialog->DoModal( m_pController->GetAnimationSetClip(), pCurrentGroupName, NULL );
}


//-----------------------------------------------------------------------------
// Called by the preset group editor panel when it changes presets
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnPresetsChanged()
{
	PopulatePresetList( false );
}


//-----------------------------------------------------------------------------
// Brings up the preset manager
//-----------------------------------------------------------------------------
void CBaseAnimSetPresetFaderPanel::OnManagePresets()
{
	if ( !m_hPresetEditor.Get() )
	{
		m_hPresetEditor = new CDmePresetGroupEditorFrame( this, "Manage Presets" );
		m_hPresetEditor->AddActionSignalTarget( this );
		m_hPresetEditor->SetVisible( false );
		m_hPresetEditor->SetDeleteSelfOnClose( false );
		m_hPresetEditor->MoveToCenterOfScreen();
	}

	m_hPresetEditor->SetAnimationSetClip( m_pController->GetAnimationSetClip() );
	m_hPresetEditor->DoModal();
}

void CBaseAnimSetPresetFaderPanel::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_pSliders->SetBgColor( Color( 42, 42, 42, 255 ) );
}

void CBaseAnimSetPresetFaderPanel::GetPreviewFader( FaderPreview_t& fader )
{
	Q_memset( &fader, 0, sizeof( fader ) );

	fader.isbeingdragged = false;
	fader.holdingPreviewKey = false;

	CPresetSlider *slider = GetActivePresetSlider();
	if ( !slider )
		return;

	fader.isbeingdragged = slider->IsDragging();

	fader.holdingPreviewKey = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	int mx, my;
	input()->GetCursorPos( mx, my );
	if ( !IsWithin( mx, my ) )
	{
		fader.holdingPreviewKey = false;
	}

	fader.name	 = slider->GetName();
	fader.amount = slider->GetCurrent();
	fader.values = slider->GetAttributeDict();
	fader.nProceduralType = slider->GetProceduralPresetType();
}

void CBaseAnimSetPresetFaderPanel::UpdateProceduralPresetSlider( AttributeDict_t *values )
{
	CPresetSlider *slider = GetActivePresetSlider();
	if ( !slider )
		return;

	int nProceduralType = slider->GetProceduralPresetType();
	switch ( nProceduralType )
	{
	case PROCEDURAL_PRESET_IN_CROSSFADE:
	case PROCEDURAL_PRESET_OUT_CROSSFADE:
	case PROCEDURAL_PRESET_HEAD_CROSSFADE:
	case PROCEDURAL_PRESET_DEFAULT_CROSSFADE:
	case PROCEDURAL_PRESET_ZERO_CROSSFADE:
	case PROCEDURAL_PRESET_HALF_CROSSFADE:
	case PROCEDRUAL_PRESET_ONE_CROSSFADE:
		GetController()->ProceduralPreset_UpdateCrossfade( values, nProceduralType );
		break;
	}
}

static void Parallel_UpdateControlValues( CPresetSlider *&slider )
{
	slider->SetControlValues();
}

static ConVar ifm_threaded_updatecontrolvalues( "ifm_threaded_updatecontrolvalues", "1", 0 );

void CBaseAnimSetPresetFaderPanel::UpdateControlValues( bool bVisibleOnly /*=true*/ )
{
	CUtlVectorFixedGrowable< CPresetSlider*, 100 > workItems;
	workItems.EnsureCapacity( m_pSliders->GetItemCount() );
	for ( int i = m_pSliders->FirstItem(); i != m_pSliders->InvalidItemID(); i = m_pSliders->NextItem(i) )
	{
		CPresetSlider *pSlider = static_cast< CPresetSlider * >( m_pSliders->GetItemPanel( i ) );
		if ( !pSlider )
			continue;

		const char *pPresetName = pSlider->GetPresetName();
		if ( !pPresetName || !*pPresetName )
			continue;

		if ( bVisibleOnly && !pSlider->IsVisible() )
			continue;

		workItems.AddToTail( pSlider );
	}

	if ( ifm_threaded_updatecontrolvalues.GetBool() )
	{
		ParallelProcess( workItems.Base(), workItems.Count(), Parallel_UpdateControlValues );
	}
	else
	{
		FOR_EACH_VEC( workItems, i )
		{
			workItems[ i ]->SetControlValues();
		}
	}
}

CPresetSlider *CBaseAnimSetPresetFaderPanel::FindPresetSlider( const char *pName )
{
	for ( int itemID = m_pSliders->FirstItem(); itemID != m_pSliders->InvalidItemID(); itemID = m_pSliders->NextItem( itemID ) )
	{
		CPresetSlider *pSlider = static_cast< CPresetSlider * >( m_pSliders->GetItemPanel( itemID ) );
		if ( !pSlider )
			continue;

		const char *pPresetName = pSlider->GetPresetName();
		if ( !pPresetName || !*pPresetName )
			continue;

		if ( !V_strcmp( pSlider->GetName(), pName ) )
			return pSlider;
	}

	return NULL;
}

void CBaseAnimSetPresetFaderPanel::SetActivePresetSlider( CPresetSlider *pSlider )
{
	m_hActivePresetSlider = pSlider;
}

CPresetSlider *CBaseAnimSetPresetFaderPanel::GetActivePresetSlider()
{
	return m_hActivePresetSlider;
}

CPresetSlider *CBaseAnimSetPresetFaderPanel::GetSliderForRow( int nSlot )
{
	return assert_cast< CPresetSlider * >( m_pSliders->GetItemPanel( m_pSliders->GetItemIDFromRow( nSlot ) ) );
}

void CBaseAnimSetPresetFaderPanel::UpdateOrCreatePresetSlider( int nSlot, const char *pPresetGroupName, const char *pPresetName )
{
	if ( !pPresetName )
		return;

	CPresetSlider *pSlider = NULL;
	if ( nSlot >= m_pSliders->GetItemCount() )
	{
		pSlider = new CPresetSlider( m_pSliders, this );
		m_pSliders->AddItem( NULL, pSlider );
	}
	else
	{
		pSlider = GetSliderForRow( nSlot );
	}

	if ( pSlider )
	{
		pSlider->Init( pPresetGroupName, pPresetName );
		pSlider->SetPos( 0 );
		pSlider->SetSize( 100, 20 );
		m_pSliders->SetItemVisible( m_pSliders->GetItemIDFromRow( nSlot ), true );
	}
}

void CBaseAnimSetPresetFaderPanel::RebuildPresetSliders( const char *pPresetGroupName, const CUtlVector< CUtlSymbolLarge > &presetNames )
{
	m_pSliders->HideAllItems();

	int nPresets = presetNames.Count();
	for ( int i = 0; i < nPresets; ++i )
	{
		UpdateOrCreatePresetSlider( i, pPresetGroupName, presetNames[ i ].String() );
	}

	int nSliders = m_pSliders->GetItemCount();
	for ( int i = nPresets; i < nSliders; ++i )
	{
		CPresetSlider *pSlider = GetSliderForRow( i );
		if ( !pSlider )
			continue;

		pSlider->Clear();
	}

	UpdateControlValues( false );
}

void CBaseAnimSetPresetFaderPanel::OnPageChanged()
{
	PopulatePresetList( true );
}

void CBaseAnimSetPresetFaderPanel::PopulatePresetList( bool bChanged )
{
	CDmeFilmClip *pFilmClip = m_pController->GetAnimationSetClip();

	int nCurrentGroupIndex = m_pSheet->GetActivePageNum();
	bool bCurrentGroupIsProceduralGroup = nCurrentGroupIndex == 0;
	const char *pCurrentGroupName = m_pSheet->GetActivePage() ? m_pSheet->GetActivePage()->GetName() : "";

	CUtlVector< PresetGroupInfo_t > presetGroupInfo;
	presetGroupInfo.AddToTail( PresetGroupInfo_t( g_pDataModel->GetSymbol( PROCEDURAL_PRESET_GROUP_NAME ) ) );
	CollectPresetGroupInfo( pFilmClip, presetGroupInfo, false, true );

	int nPresetGroups = presetGroupInfo.Count();
	int nPresetGroupPages = m_presetGroupPages.Count();

	for ( int i = 0; i < nPresetGroupPages; ++i )
	{
		vgui::PropertyPage *pPage = m_presetGroupPages[ i ];
		if ( !pPage )
			continue;

		if ( i < nPresetGroups )
		{
			const char *pName = presetGroupInfo[ i ].presetGroupSym.String();
			const char *pPageName = pPage->GetName();
			if ( !*pPageName )
			{
				m_pSheet->AddPage( pPage, pName );
			}
			else
			{
				m_pSheet->SetPageTitle( pPage, pName );
			}

			pPage->SetName( pName );
		}
		else
		{
			m_pSheet->RemovePage( pPage );
			pPage->SetName( "" );
		}
	}

	for ( int i = nPresetGroupPages; i < nPresetGroups; ++i )
	{
		const char *pName = presetGroupInfo[ i ].presetGroupSym.String();
		vgui::PropertyPage *pPage = new vgui::PropertyPage( this, pName );
		m_presetGroupPages.AddToTail( pPage );
		m_pSheet->AddPage( pPage, pName );
	}

	if ( nCurrentGroupIndex < 0 )
	{
		m_pSliders->HideAllItems();
		return;
	}

	vgui::PropertyPage *pCurrentGroupPage = m_presetGroupPages[ nCurrentGroupIndex ];
	m_pSheet->SetActivePage( pCurrentGroupPage );
	m_pSliders->SetParent( pCurrentGroupPage );

	CUtlVector< CUtlSymbolLarge > presetNames;
	if ( bCurrentGroupIsProceduralGroup )
	{
		CollectProceduralPresetNames( presetNames );
	}
	else
	{
		CollectPresetNamesForGroup( pFilmClip, pCurrentGroupName, presetNames );
	}

	RebuildPresetSliders( pCurrentGroupName, presetNames );
}

void CBaseAnimSetPresetFaderPanel::OnDeletePreset( const char *pPresetName )
{
	CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Delete Preset" );
	{
		CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
		while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
		{
			// Delete it from various things
			pAnimSet->RemovePreset( pPresetName );
		}
	}

	PopulatePresetList( false );
}

void CBaseAnimSetPresetFaderPanel::DispatchCurve( int nCurveType )
{
	// Nothing, must be handled by SFM
}
