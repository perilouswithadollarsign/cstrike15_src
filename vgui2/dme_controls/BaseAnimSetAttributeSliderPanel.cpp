//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeanimationset.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/PanelListPanel.h"
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"
#include "dme_controls/BaseAnimationSetEditor.h"
#include "dme_controls/attributeslider.h"
#include "dme_controls/dmecontrols_utils.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IInput.h"
#include "vgui/IVgui.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define ANIMATION_SET_EDITOR_ATTRIBUTESLIDERS_BUTTONTRAY_HEIGHT 32

const int FREE_SLIDER_LIST = 1500;

//-----------------------------------------------------------------------------
//
// CPresetSideFilterSlider class begins
//
//-----------------------------------------------------------------------------
class CPresetSideFilterSlider : public Slider
{
	DECLARE_CLASS_SIMPLE( CPresetSideFilterSlider, Slider );

public:
	CPresetSideFilterSlider( CBaseAnimSetAttributeSliderPanel *pParent, const char *panelName );
	virtual ~CPresetSideFilterSlider();

	float		GetPos();
	void		SetPos( float frac );

protected:
	virtual void Paint();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( IScheme *scheme );
	virtual void GetTrackRect( int &x, int &y, int &w, int &h );
	virtual void OnMousePressed(MouseCode code);
	virtual void OnMouseDoublePressed(MouseCode code);

private:
	CBaseAnimSetAttributeSliderPanel	*m_pParent;

	Color			m_ZeroColor;
	Color			m_TextColor;
	Color			m_TextColorFocus;
	TextImage		*m_pName;
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CPresetSideFilterSlider::CPresetSideFilterSlider( CBaseAnimSetAttributeSliderPanel *parent, const char *panelName ) :
	BaseClass( (Panel *)parent, panelName ), m_pParent( parent )
{
	SetRange( 0, 1000 );
	SetDragOnRepositionNob( true );
	SetPos( 0.5f );
	SetPaintBackgroundEnabled( true );

	m_pName = new TextImage( "Preset Side Filter" );

	SetBgColor( Color( 128, 128, 128, 128 ) );

	m_ZeroColor = Color( 33, 33, 33, 255 );
	m_TextColor = Color( 200, 200, 200, 255 );
	m_TextColorFocus = Color( 208, 143, 40, 255 );
}

CPresetSideFilterSlider::~CPresetSideFilterSlider()
{
	delete m_pName;
}

void CPresetSideFilterSlider::OnMousePressed(MouseCode code)
{
	if ( code == MOUSE_RIGHT )
	{
		SetPos( 0.5f );
		return;
	}

	BaseClass::OnMousePressed( code );
}

void CPresetSideFilterSlider::OnMouseDoublePressed(MouseCode code)
{
	if ( code == MOUSE_LEFT )
	{
		SetPos( 0.5f );
		return;
	}

	BaseClass::OnMouseDoublePressed( code );
}

float CPresetSideFilterSlider::GetPos()
{
	return GetValue() * 0.001f;
}

void CPresetSideFilterSlider::SetPos( float frac )
{
	SetValue( (int)( frac * 1000.0f + 0.5f ), false );
}

void CPresetSideFilterSlider::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pName->SetFont( scheme->GetFont( "DefaultBold" ) );
	m_pName->SetColor( m_TextColor );
	m_pName->ResizeImageToContent();

	SetFgColor( Color( 194, 120, 0, 255 ) );
	SetThumbWidth( 3 );
}

void CPresetSideFilterSlider::GetTrackRect( int &x, int &y, int &w, int &h )
{
	GetSize( w, h );
	x = 0;
	y = 2;
	h -= 4;
}

void CPresetSideFilterSlider::Paint()
{
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

void CPresetSideFilterSlider::PaintBackground()
{
	int w, h;
	GetSize( w, h );
			   
	int tx, ty, tw, th;
	GetTrackRect( tx, ty, tw, th );
	surface()->DrawSetColor( m_ZeroColor );
	surface()->DrawFilledRect( tx, ty, tx + tw, ty + th );

	int cw, ch;
	m_pName->SetColor( _dragging ? m_TextColorFocus : m_TextColor );
	m_pName->GetContentSize( cw, ch );
	m_pName->SetPos( ( w - cw ) * 0.5f, ( h - ch ) * 0.5f );
	m_pName->Paint();
}


//-----------------------------------------------------------------------------
//
// CBaseAnimSetAttributeSliderPanel begins
//
//-----------------------------------------------------------------------------
CBaseAnimSetAttributeSliderPanel::CBaseAnimSetAttributeSliderPanel( vgui::Panel *parent, const char *className, CBaseAnimationSetEditor *editor ) :
	BaseClass( parent, className ),
	m_pController( NULL )
{
	m_hEditor = editor;

	m_pLeftRightBoth[ 0 ] = new Button( this, "AttributeSliderLeftOnly", "", this, "OnLeftOnly" );
	m_pLeftRightBoth[ 1 ] = new Button( this, "AttributeSliderRightOnly", "", this, "OnRightOnly" );
	m_pPresetSideFilter = new CPresetSideFilterSlider( this, "PresetSideFilter" );
	m_Sliders = new PanelListPanel( this, "AttributeSliders" );
	m_Sliders->SetFirstColumnWidth( 0 );
	m_Sliders->SetAutoResize
		( 
		Panel::PIN_TOPLEFT, 
		Panel::AUTORESIZE_DOWNANDRIGHT,
		0, ANIMATION_SET_EDITOR_ATTRIBUTESLIDERS_BUTTONTRAY_HEIGHT,
		0, 0
		);
	m_Sliders->SetVerticalBufferPixels( 0 );

	m_pController = editor->GetController();
	m_pController->AddControlSelectionChangedListener( this );

	ivgui()->AddTickSignal( GetVPanel(), 0 );

	InitFreeSliderList( FREE_SLIDER_LIST );
}

void CBaseAnimSetAttributeSliderPanel::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "OnLeftOnly" ) )
	{
		m_pPresetSideFilter->SetPos( 0.0f );
		return;
	}

	if ( !Q_stricmp( pCommand, "OnRightOnly" ) )
	{
		m_pPresetSideFilter->SetPos( 1.0f );
		return;
	}

	BaseClass::OnCommand( pCommand );
}

void CBaseAnimSetAttributeSliderPanel::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );
	m_Sliders->SetBgColor( Color( 42, 42, 42, 255 ) );
}

void CBaseAnimSetAttributeSliderPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	int availH = ANIMATION_SET_EDITOR_ATTRIBUTESLIDERS_BUTTONTRAY_HEIGHT;
				 	 
	int btnSize = 9;
	m_pLeftRightBoth[ 0 ]->SetBounds( 15, ( availH - btnSize ) / 2, btnSize, btnSize );
	m_pLeftRightBoth[ 1 ]->SetBounds( w - 15, ( availH - btnSize ) / 2, btnSize, btnSize );
	m_pPresetSideFilter->SetBounds( 23 + btnSize, 4, w - 38 - 2 * btnSize, availH - 8 );
}

void CBaseAnimSetAttributeSliderPanel::OnTick()
{
	BaseClass::OnTick();
	bool bVisible = IsVisible();
	if ( bVisible )
	{
		// chain up and see if any parent panel is hiding us
		VPANEL p = GetVParent();
		while ( p )
		{
			if ( !ipanel()->IsVisible(p) )
			{
				bVisible = false;
				break;
			}

			p = ipanel()->GetParent(p);
		}
	}
	if ( !bVisible )
	{
		OnThink();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Determines:
//  a) are we holding the ctrl key still, if so
//     figures out the crossfade amount of each preset slider with non-zero influence
//  b) not holding control, then just see if we are previewing whichever preset the mouse is over
// Input  :  - 
//-----------------------------------------------------------------------------
void CBaseAnimSetAttributeSliderPanel::OnThink()
{
	BaseClass::OnThink();

	m_pController->UpdatePreviewSliderValues();

	m_pController->UpdatePreviewSliderTimes();

	ApplySliderValues( false );

	UpdateSliderDependencyFlags();
}

void CBaseAnimSetAttributeSliderPanel::ChangeAnimationSetClip( CDmeFilmClip *pFilmClip )
{
	RebuildSliderLists();
}

void CBaseAnimSetAttributeSliderPanel::RebuildSliderLists()
{
	int c = m_SliderList.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		FreeSlider( m_SliderList[ i ] );
	}
	m_SliderList.RemoveAll();
	m_Sliders->RemoveAll();

	CUtlVector< CDmElement * > controlList;

	CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		const CDmaElementArray< CDmElement > &controls = pAnimSet->GetControls();

		// Now create sliders for all known controls, nothing visible by default
		int controlCount = controls.Count();
		for ( int ci = 0 ; ci < controlCount; ++ci )
		{
			CDmElement *control = controls[ ci ];
			if ( !control )
				continue;

			controlList.AddToTail( control );
		}
	}

	for ( int i = 0; i < controlList.Count(); ++i )
	{
		CDmElement *control = controlList[ i ];

		CAttributeSlider *slider = AllocateSlider();
		slider->Init( control, false );
		slider->SetVisible( false );
		m_SliderList.AddToTail( slider );

		// Transform controls get two separate sliders, one for position and one for rotation. 
		// This is something of an artifact of having separate controls for position and rotation, 
		// but it is useful for value type in to have a separate slider for each.
		CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( control );
		if ( pTransformControl )
		{
			CAttributeSlider *pOrientationSlider = AllocateSlider();
			pOrientationSlider->Init( control, true );
			pOrientationSlider->SetVisible( false );
			m_SliderList.AddToTail( pOrientationSlider );
		}
	}
}

void CBaseAnimSetAttributeSliderPanel::OnControlsAddedOrRemoved()
{
	bool changed = false;
	int nSliderIndex = 0;

	CAnimSetGroupAnimSetTraversal traversal( m_pController->GetAnimationSetClip() );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		// See if every slider is the same as before
		const CDmaElementArray< CDmElement > &controls = pAnimSet->GetControls();
		int controlCount = controls.Count();
		for ( int i = 0 ; i < controlCount; ++i )
		{
			CDmElement *control = controls[ i ];
			if ( !control )
				continue;

			if ( nSliderIndex >= m_SliderList.Count() )
			{
				changed = true;
				break;
			}

			CAttributeSlider *pSlider = m_SliderList[ nSliderIndex++ ];
			if ( pSlider->GetControl() != control )
			{
				changed = true;
				break;
			}
			
			if ( IsTransformControl( control ) )
			{
				// Transform controls add two sliders so make sure the 
				// next slider refers to the transform control as well.
				pSlider = m_SliderList[ nSliderIndex++ ];
				if ( pSlider->GetControl() != control )
				{
					changed = true;
					break;
				}
				continue;
			}

			if ( pSlider->IsStereo() != IsStereoControl( control ) )
			{
				changed = true;
				break;
			}
		}
	}

	changed = changed || nSliderIndex != m_SliderList.Count();

	if ( !changed )
		return;

	RebuildSliderLists();
}

void CBaseAnimSetAttributeSliderPanel::OnControlSelectionChanged()
{
	bool visibleSlidersChanged = false;

	// Walk through all sliders and show only those in the symbol table
	int c = m_SliderList.Count();
	for ( int i = 0; i < c; ++i )
	{
		CAttributeSlider *pSlider = m_SliderList[ i ];
		CDmElement* pSliderControl = pSlider->GetControl();

		TransformComponent_t nComponentFlags = m_pController->GetSelectionComponentFlags( pSliderControl );

		// If the slider is transform control slider determine if it is the 
		// slider for the position or rotation and mask the flags accordingly.
		LogComponents_t nLogComponentFlags = SelectionInfo_t::ConvertTransformFlagsToLogFlags( nComponentFlags, pSlider->IsOrientation() );
		
		bool bShowSlider = nComponentFlags != 0;

		if ( pSlider->IsVisible() != bShowSlider ) 
		{
			pSlider->SetVisible( bShowSlider );
			visibleSlidersChanged = true;
		}

		if ( pSlider->VisibleComponents() != nLogComponentFlags )
		{
			pSlider->SetVisibleComponents( nLogComponentFlags );
		}
	}

	// If nothing changed then nothing else needs to be done
	if ( !visibleSlidersChanged )
		return;

	// If the visibility of entire sliders changed then 
	// the slider visibility list must be updated.
	m_Sliders->RemoveAll();
	for ( int i = 0; i < c; ++i )
	{
		CAttributeSlider *slider = m_SliderList[ i ];
		if ( slider->IsVisible() )
		{
			m_Sliders->AddItem( NULL, slider );
		}
	}
}


void CBaseAnimSetAttributeSliderPanel::GetTypeInValueForControl( CDmElement *pControl, bool bOrientation, AttributeValue_t &controlValue, const AttributeValue_t &sliderValue )
{
	CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pControl );
	
	if ( pTransformControl && bOrientation )
	{
		const Quaternion &q = sliderValue.m_Quaternion;
		QAngle ang;
		QuaternionAngles( q, ang );
		controlValue.m_Vector.x = ang.x;
		controlValue.m_Vector.y = ang.y;
		controlValue.m_Vector.z = ang.z;
	}
	else
	{
		controlValue = sliderValue;
	}
}


void CBaseAnimSetAttributeSliderPanel::UpdatePreview( char const *pchFormat, ... )
{
}

bool CBaseAnimSetAttributeSliderPanel::ApplySliderValues( bool bForce )
{
	bool bValuesChanged = m_pController->ApplySliderValues( bForce );
	if ( bValuesChanged )
	{
		UpdatePreview( "ApplySliderValues\n" );
	}
	return bValuesChanged;
}


//-----------------------------------------------------------------------------
// Purpose: Update the slider flags specifying if a slider is dependency of the
// currently active slider.
//-----------------------------------------------------------------------------
void CBaseAnimSetAttributeSliderPanel::UpdateSliderDependencyFlags() const
{
	bool ctrlDown = input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_RCONTROL );
	CAttributeSlider *pPrimarySlider = ctrlDown ? m_pController->GetActiveAttributeSlider() : NULL;

	int nSliders = m_SliderList.Count();
	for ( int iSlider = 0; iSlider < nSliders; ++iSlider )
	{
		CAttributeSlider *pSlider = m_SliderList[ iSlider ];
		if ( !pSlider )
			continue;

		pSlider->SetDependent( pPrimarySlider ? pPrimarySlider->IsDependent( pSlider ) : false );
	}
}

void CBaseAnimSetAttributeSliderPanel::SetupForPreset( FaderPreview_t &fader )
{
	// Nothing special here
}

float CBaseAnimSetAttributeSliderPanel::GetBalanceSliderValue()
{
	return m_pPresetSideFilter->GetPos();
}

int CBaseAnimSetAttributeSliderPanel::FindSliderIndexForControl( const CDmElement *control )
{
	int c = m_SliderList.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( m_SliderList[ i ]->GetControl() == control )
			return i;
	}

	return -1;
}

CAttributeSlider *CBaseAnimSetAttributeSliderPanel::FindSliderForControl( const CDmElement *control )
{
	int i = FindSliderIndexForControl( control );
	if ( i < 0 )
		return NULL;

	return m_SliderList[ i ];
}

bool CBaseAnimSetAttributeSliderPanel::GetSliderValues( AttributeValue_t *pValue, int nIndex )
{
	Assert( pValue );
	Assert( nIndex >= 0 && nIndex < m_SliderList.Count() );

	CAttributeSlider *pSlider = m_SliderList[ nIndex ];

	CAttributeSlider *pAttrSlider = m_pController->GetActiveAttributeSlider();
	bool shiftDown = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	bool bPreviewingAttrSlider = pAttrSlider == pSlider && shiftDown;
	bool bGetPreview = bPreviewingAttrSlider || m_pController->WasPreviouslyHoldingPresetPreviewKey() || m_pController->IsPresetFaderBeingDragged();

	*pValue = bGetPreview ? pSlider->GetPreview() : pSlider->GetValue();
	return pSlider->IsVisible();
}

void CBaseAnimSetAttributeSliderPanel::DispatchCurve( int nCurveType )
{
	// Nothing, handled by SFM
}

void CBaseAnimSetAttributeSliderPanel::InitFreeSliderList( int nCount )
{
	for ( int i = 0; i < nCount; ++i )
	{
		CAttributeSlider *slider = new CAttributeSlider( this );
		slider->SetVisible( false );
		m_FreeSliderList.AddToTail( slider );
	}
}

CAttributeSlider *CBaseAnimSetAttributeSliderPanel::AllocateSlider()
{
	int c = m_FreeSliderList.Count();
	if ( c  > 0 )
	{
		CAttributeSlider *slider = m_FreeSliderList[ c - 1 ];
		m_FreeSliderList.Remove( c - 1 );
		slider->SetVisible( true );
		return slider;
	}

	// Add a new one
	CAttributeSlider *slider = new CAttributeSlider( this );
	slider->SetVisible( true );
	return slider;
}

void CBaseAnimSetAttributeSliderPanel::FreeSlider( CAttributeSlider *slider )
{
	slider->SetVisible( false );
	m_FreeSliderList.AddToTail( slider );
}