//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/attributeslider.h"
#include "dme_controls/dmecontrols_utils.h"
#include "materialsystem/imesh.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmeexpressionoperator.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui_controls/TextImage.h"
#include "vgui_controls/subrectimage.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/Menu.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"
#include "dme_controls/BaseAnimationSetEditor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Enums
//-----------------------------------------------------------------------------
#define SLIDER_PIXEL_SPACING 3
#define UNDO_CHAIN_MOUSEWHEEL_ATTRIBUTE_SLIDER 9876
#define FRAC_PER_PIXEL 0.0025f


static ConVar ifm_attributeslider_sensitivity( "ifm_attributeslider_sensitivity", "3.0", 0 );
static ConVar ifm_attributeslider_legacy( "ifm_attributeslider_legacy", "0", 0, "Uses old style slider dragging." );


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static Color s_TextColor( 200, 200, 200, 192 );
static Color s_TextColorFocus( 208, 143, 40, 192 );
static Color s_TextColorDependent( 240, 50, 50, 192 );

static Color s_BarColor[2] = 
{
	Color( 45, 45, 45, 255 ), Color( 30, 255, 255, 80 )
};

static Color s_ZeroColor[2] = 
{
	Color( 33, 33, 33, 255 ), Color( 100, 80, 0, 255 )
};

static Color s_DraggingBarColor( 142, 142, 142, 255 );
static Color s_PreviewTickColor( 255, 164, 8, 255 );
static Color s_OldValueTickColor( 100, 100, 100, 63 );

static Color s_MidpointColor( 115, 115, 115, 255 );

//-----------------------------------------------------------------------------
// Blends flex values in left-right space instead of balance/value space
//-----------------------------------------------------------------------------
static void BlendFlexValues( AttributeValue_t *pResult, const AttributeValue_t &src, const AttributeValue_t &dest, float flBlend, float flBalanceFilter = 0.5f )
{
	float flLeftFilter, flRightFilter;
	ValueBalanceToLeftRight( &flLeftFilter, &flRightFilter, flBlend, flBalanceFilter, 0.0f );

	pResult->m_pValue[ANIM_CONTROL_VALUE      ] = src.m_pValue[ANIM_CONTROL_VALUE      ] + ( dest.m_pValue[ANIM_CONTROL_VALUE      ] - src.m_pValue[ANIM_CONTROL_VALUE      ] ) * flBlend;
	pResult->m_pValue[ANIM_CONTROL_VALUE_LEFT ] = src.m_pValue[ANIM_CONTROL_VALUE_LEFT ] + ( dest.m_pValue[ANIM_CONTROL_VALUE_LEFT ] - src.m_pValue[ANIM_CONTROL_VALUE_LEFT ] ) * flLeftFilter;
	pResult->m_pValue[ANIM_CONTROL_VALUE_RIGHT] = src.m_pValue[ANIM_CONTROL_VALUE_RIGHT] + ( dest.m_pValue[ANIM_CONTROL_VALUE_RIGHT] - src.m_pValue[ANIM_CONTROL_VALUE_RIGHT] ) * flRightFilter;
}

static void BlendTransformValues( AttributeValue_t *pResult, const AttributeValue_t &src, const AttributeValue_t &dest, float flBlend )
{
	pResult->m_Vector = Lerp( flBlend, src.m_Vector, dest.m_Vector );
	
	// TODO:  SHould this be a Slerp or a simple blend or some other op?
	QuaternionSlerp( src.m_Quaternion, dest.m_Quaternion, flBlend, pResult->m_Quaternion );
}

void BlendValues( bool bTransform, AttributeValue_t *pResult, const AttributeValue_t &src, const AttributeValue_t &dest, float flBlend, float flBalanceFilter = 0.5f )
{
	if ( bTransform )
	{
		BlendTransformValues( pResult, src, dest, flBlend );
	}
	else
	{
		BlendFlexValues( pResult, src, dest, flBlend, flBalanceFilter );
	}
}
//-----------------------------------------------------------------------------
// The panel used to do text entry when double-clicking in the slider
//-----------------------------------------------------------------------------
class CAttributeSliderTextEntry : public TextEntry
{
	DECLARE_CLASS_SIMPLE( CAttributeSliderTextEntry, TextEntry );

public:
	CAttributeSliderTextEntry( CAttributeSlider *slider, const char *panelName ) :
		BaseClass( (Panel *)slider, panelName ), m_pSlider( slider )
	{
		Assert( m_pSlider );
	}

	MESSAGE_FUNC_PARAMS( OnKillFocus, "KillFocus", kv );
	virtual void OnMouseWheeled( int delta );

private:
	CAttributeSlider *m_pSlider;
};



//-----------------------------------------------------------------------------
//
// CAttributeSlider begins here
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CAttributeSlider::CAttributeSlider( CBaseAnimSetAttributeSliderPanel *parent ) :
	BaseClass( (Panel *)parent, "" ), 
	m_pParent( parent ),
	m_flFaderAmount( 1.0f ),
	m_bDependent( false ),
	m_pTextField( 0 ),
	m_pRightTextField( 0 ),
	m_nVisibleComponents( LOG_COMPONENTS_ALL )
{
	SetPaintBackgroundEnabled( true );
	SetPaintBorderEnabled( false );
	SetBgColor( Color( 42, 42, 42, 255 ) );

	m_pName = new TextImage( "" );
	m_pValues[ 0 ] = new TextImage( "" );
	m_pValues[ 1 ] = new TextImage( "" );
	m_pValues[ 2 ] = new TextImage( "" );
	m_pValues[ 3 ] = new TextImage( "" );

	SetSize( 100, 20 );
}

void CAttributeSlider::Init( CDmElement *control, bool bOrientation )
{
	SetName( control->GetName() );

	m_SliderMode = SLIDER_MODE_NONE;
	m_hControl = control;
	m_bTransform = false;
	m_bOrientation = false;
	
	// Cache off control information since this state should never change
	// NOTE: If it ever does, just change the implementations of 
	// IsTransform + GetMidpoint to always read these values from the attributes
	CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( control );
	if ( pTransformControl )
	{	
		m_bTransform = true;
		m_bOrientation = bOrientation;
	}

	m_bStereo = IsStereoControl( control );

	m_nDragStartPosition[ 0 ] = m_nDragStartPosition[ 1 ] = 0;
	m_nAccum[ 0 ] =  m_nAccum[ 1 ] = 0;
	m_dragStartValues = GetValue();

	SetPaintBackgroundEnabled( true );

	if ( m_bTransform )
	{
		m_pName->SetText( CFmtStr( m_bOrientation ? "%s - rot" : "%s - pos", control->GetName() ) );
	}
	else
	{
		m_pName->SetText( control->GetName() );
	}
	m_pName->ResizeImageToContent();

	InitControls();
}

CAttributeSlider::~CAttributeSlider()
{
	delete m_pName;
	delete m_pValues[ 0 ];
	delete m_pValues[ 1 ];
	delete m_pValues[ 2 ];
	delete m_pValues[ 3 ];
}


//-----------------------------------------------------------------------------
// Scheme
//-----------------------------------------------------------------------------
void CAttributeSlider::ApplySchemeSettings( IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	m_pName->SetFont( scheme->GetFont( "Default" ) );
	m_pName->SetColor( s_TextColor );
	m_pName->ResizeImageToContent();

	m_pValues[ 0 ]->SetColor( s_TextColor );
	m_pValues[ 0 ]->SetFont( scheme->GetFont( "Default" ) );
	m_pValues[ 1 ]->SetColor( s_TextColorFocus );
	m_pValues[ 1 ]->SetFont( scheme->GetFont( "Default" ) );
	m_pValues[ 2 ]->SetColor( s_TextColor );
	m_pValues[ 2 ]->SetFont( scheme->GetFont( "Default" ) );
	m_pValues[ 3 ]->SetColor( s_TextColor );
	m_pValues[ 3 ]->SetFont( scheme->GetFont( "Default" ) );

	SetBgColor( Color( 42, 42, 42, 255 ) );
	SetFgColor( Color( 194, 120, 0, 255 ) );
}


void CAttributeSlider::InitControls()
{
	if ( m_bTransform )
	{
		CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( m_hControl );
		if ( pTransformControl )
		{			
			if ( m_bOrientation )
			{					
				SetValue( ANIM_CONTROL_TXFORM_ORIENTATION, pTransformControl->GetOrientation() );
			}
			else
			{
				SetValue( ANIM_CONTROL_TXFORM_POSITION, pTransformControl->GetPosition() );
			}
		}
	}
	else
	{
		if ( m_bStereo )
		{
			SetValue( ANIM_CONTROL_VALUE_LEFT,  m_hControl->GetValue< float >( "leftValue" ) );
			SetValue( ANIM_CONTROL_VALUE_RIGHT, m_hControl->GetValue< float >( "rightValue" ) );
		}
		else
		{
			SetValue( ANIM_CONTROL_VALUE, m_hControl->GetValue< float >( "value" ) );
		}
	}
}

void CAttributeSlider::SetValue( AnimationControlType_t type, float flValue )
{
	Assert( type < ANIM_CONTROL_COUNT );
	if ( m_Control.m_pValue[type] != flValue )
	{
		m_Control.m_pValue[type] = flValue;
	}
}

void CAttributeSlider::SetValue( const AttributeValue_t& value )
{
	for ( int i = 0; i < ANIM_CONTROL_COUNT; ++i )
	{
		SetValue( (AnimationControlType_t)i, value.m_pValue[i] );
	}
	SetValue( ANIM_CONTROL_TXFORM_POSITION, value.m_Vector );
	SetValue( ANIM_CONTROL_TXFORM_ORIENTATION, value.m_Quaternion );
}

void CAttributeSlider::SetValue( AnimationControlType_t type, const Vector &vec )
{
	Assert( type == ANIM_CONTROL_TXFORM_POSITION );
	if ( m_Control.m_Vector != vec )
	{
		m_Control.m_Vector = vec;
	}
}

void CAttributeSlider::SetValue( AnimationControlType_t type, const Quaternion &quat )
{
	Assert( type == ANIM_CONTROL_TXFORM_ORIENTATION );
	if ( m_Control.m_Quaternion != quat )
	{
		m_Control.m_Quaternion = quat;
	}
}

const AttributeValue_t& CAttributeSlider::GetValue() const
{
	return m_Control;
}

float CAttributeSlider::GetValue( AnimationControlType_t type ) const
{
	Assert( type < ANIM_CONTROL_COUNT );
	return m_Control.m_pValue[type];
}

void CAttributeSlider::GetValue( AnimationControlType_t type, Vector &out ) const
{
	Assert( type == ANIM_CONTROL_TXFORM_POSITION );
	out = m_Control.m_Vector;
}

void CAttributeSlider::GetValue( AnimationControlType_t type, Quaternion &out ) const
{
	Assert( type == ANIM_CONTROL_TXFORM_ORIENTATION );
	out = m_Control.m_Quaternion;
}

float CAttributeSlider::GetPreview( AnimationControlType_t type ) const
{
	Assert( type < ANIM_CONTROL_COUNT );
	return m_PreviewCurrent.m_pValue[type];
}

void CAttributeSlider::GetPreview( AnimationControlType_t type, Vector &out ) const
{
	Assert( type == ANIM_CONTROL_TXFORM_POSITION );
	out = m_PreviewCurrent.m_Vector;
}

void CAttributeSlider::GetPreview( AnimationControlType_t type, Quaternion &out ) const
{
	Assert( type == ANIM_CONTROL_TXFORM_ORIENTATION );
	out = m_PreviewCurrent.m_Quaternion;
}


//-----------------------------------------------------------------------------
// Purpose: Set the flags controlling which specific components of the slider
// are visible.
//-----------------------------------------------------------------------------
void CAttributeSlider::SetVisibleComponents( LogComponents_t componentFlags )
{
	m_nVisibleComponents = componentFlags;
}


//-----------------------------------------------------------------------------
// Purpose: Get the set of flags specifying which components of the slider are
// visible.
//-----------------------------------------------------------------------------
LogComponents_t CAttributeSlider::VisibleComponents() const
{
	return m_nVisibleComponents;
}


void CAttributeSlider::OnCursorEntered()
{
	if ( IsDragging() )
		return;

	BaseClass::OnCursorEntered();
	m_pParent->GetController()->SetActiveAttributeSlider( this );
}
	
void CAttributeSlider::OnCursorExited()
{
	if ( IsDragging() )
		return;

	BaseClass::OnCursorExited();
	m_pParent->GetController()->SetActiveAttributeSlider( NULL );
}


void CAttributeSlider::SetToDefault()
{
	if ( m_bTransform )
	{
		if ( m_bOrientation )
		{
			const Quaternion &quat = m_hControl->GetValue< Quaternion >( DEFAULT_ORIENTATION_ATTR );
			SetValue( ANIM_CONTROL_TXFORM_ORIENTATION, quat );

			CUndoScopeGuard guard( "Set Slider Value To Default" );

			StampValueIntoLogs( ANIM_CONTROL_TXFORM_ORIENTATION, quat );
		}
		else
		{
			const Vector &vec = m_hControl->GetValue< Vector >( DEFAULT_POSITION_ATTR );
			SetValue( ANIM_CONTROL_TXFORM_POSITION, vec );

			CUndoScopeGuard guard( "Set Slider Value To Default" );

			StampValueIntoLogs( ANIM_CONTROL_TXFORM_POSITION, vec );
		}
	}
	else
	{
		float flDefaultValue = m_hControl->GetValue< float >( DEFAULT_FLOAT_ATTR );
		if ( m_bStereo )
		{
			SetValue( ANIM_CONTROL_VALUE_LEFT,  flDefaultValue );
			SetValue( ANIM_CONTROL_VALUE_RIGHT, flDefaultValue );

			CUndoScopeGuard guard( "Set Slider Value To Default" );

			StampValueIntoLogs( ANIM_CONTROL_VALUE_LEFT,  flDefaultValue );
			StampValueIntoLogs( ANIM_CONTROL_VALUE_RIGHT, flDefaultValue );
		}
		else
		{
			SetValue( ANIM_CONTROL_VALUE, flDefaultValue );

			CUndoScopeGuard guard( "Set Slider Value To Default" );

			StampValueIntoLogs( ANIM_CONTROL_VALUE, flDefaultValue );
		}
	}
}

void CAttributeSlider::OnSetToDefault()
{
	SetToDefault();
}

void CAttributeSlider::OnEditMinMaxDefault()
{
	CDmeChannel *pChannel = m_hControl->GetValueElement< CDmeChannel >( "channel" );
	if ( !pChannel )
		return;

	CDmeExpressionOperator *pExpr = CastElement< CDmeExpressionOperator >(  pChannel->GetToElement() );
	if ( !pExpr )
		return;

	if ( m_bStereo )
		return;

	float flMin = pExpr->GetValue< float >( "lo" );
	float flMax = pExpr->GetValue< float >( "hi" );
	float flDefault = m_hControl->GetValue< float >( "defaultValue" );
	flDefault = flMin + flDefault * ( flMax - flMin );

	MultiInputDialog *pDialog = new MultiInputDialog( this, "Edit Min/Max/Default" );
	pDialog->AddEntry( "min",     "Min:",     flMin );
	pDialog->AddEntry( "max",     "Max:",     flMax );
	pDialog->AddEntry( "default", "Default:", flDefault );
	pDialog->DoModal();
}

void CAttributeSlider::OnInputCompleted( KeyValues *params )
{
	CDmeChannel *pChannel = m_hControl->GetValueElement< CDmeChannel >( "channel" );
	if ( !pChannel )
		return;

	CDmeExpressionOperator *pExpr = CastElement< CDmeExpressionOperator >(  pChannel->GetToElement() );
	if ( !pExpr )
		return;

	m_pParent->GetController()->OnSliderRangeRemapped();

	float flOldMin = pExpr->GetValue< float >( "lo" );
	float flOldMax = pExpr->GetValue< float >( "hi" );
	float flOldValue = m_hControl->GetValue< float >( "value" );
	flOldValue = flOldMin + flOldValue * ( flOldMax - flOldMin );

	float flMin     = params->GetFloat( "min" );
	float flMax     = params->GetFloat( "max" );
	float flDefault = params->GetFloat( "default" );
	flDefault = ( flDefault - flMin ) / ( flMax - flMin );
	flOldValue = ( flOldValue - flMin ) / ( flMax - flMin );

	CUndoScopeGuard sg( "Set Control Min/Max/Default" );

	pExpr->SetValue( "lo", flMin );
	pExpr->SetValue( "hi", flMax );
	m_hControl->SetValue( "defaultValue", clamp( flDefault, 0.0f, 1.0f ) );
	m_hControl->SetValue( "value", clamp( flOldValue, 0.0f, 1.0f ) );

	float flBias = ( flOldMin - flMin ) / ( flMax - flMin );
	float flScale = ( flOldMax - flOldMin ) / ( flMax - flMin );
	RemapFloatLogValues( pChannel, flBias, flScale );
}

//-----------------------------------------------------------------------------
// Mouse event handlers
//-----------------------------------------------------------------------------
void CAttributeSlider::OnMousePressed( MouseCode code )
{
	if ( !IsEnabled() || IsInTextEntry() || IsDragging() )
		return;

	if ( code != MOUSE_LEFT )
		return;

	// Cache off the value at the click point
	// in case we end up receiving a double-click
	m_pParent->GetTypeInValueForControl( m_hControl, m_bOrientation, m_InitialTextEntryValue, m_Control );

	if ( m_bTransform )
		return;

	// Determine which control we clicked on
	int x,y;
	input()->GetCursorPosition( x, y );
	ScreenToLocal( x, y );

	// Enter drag mode
	m_SliderMode = SLIDER_MODE_DRAG_VALUE;
	m_nDragStartPosition[ 0 ] = x;
	m_nDragStartPosition[ 1 ] = y;
	m_nAccum[ 0 ] = m_nAccum[ 1 ] = 0;
	m_dragStartValues = GetValue();
	input()->SetMouseCapture( GetVPanel() );
	SetCursor( dc_blank );
}

void CAttributeSlider::OnCursorMoved( int x, int y )
{
	if ( !IsEnabled() || !IsDragging() || m_bTransform )
		return;

	// NOTE: This works because we always slam the mouse to be back at the start position
	// at the end of this function

	// Accumulate the total mouse movement
	int dx = x - m_nDragStartPosition[ 0 ];
	m_nAccum[ 0 ] += dx;

	float flFactor = 1.0f;
	if ( ifm_attributeslider_legacy.GetBool() )
	{
		flFactor = FRAC_PER_PIXEL * ifm_attributeslider_sensitivity.GetFloat();
	}
	else
	{
		Rect_t rect;
		GetControlRect( &rect ); // assumes controls are same width

		if ( rect.width > 0 )
		{
			flFactor = 1.0f / (float)rect.width;
		}
	}

	bool bInRecordMode = m_pParent->GetEditor()->GetController()->GetRecordingState() == AS_RECORD;
	float flMinVal = bInRecordMode ? -1.0f : 0.0f;
	float flMaxVal = bInRecordMode ?  2.0f : 1.0f;

	if ( m_bStereo )
	{
		float flBalance = m_pParent->GetBalanceSliderValue();

		float flLeftValue  = m_dragStartValues.m_pValue[ ANIM_CONTROL_VALUE_LEFT ];
		float flRightValue = m_dragStartValues.m_pValue[ ANIM_CONTROL_VALUE_RIGHT ];

		int nMinVal = floor( ( flMinVal - MAX( flLeftValue, flRightValue ) ) / flFactor );
		int nMaxVal = ceil ( ( flMaxVal - MIN( flLeftValue, flRightValue ) ) / flFactor );
		m_nAccum[ 0 ] = clamp( m_nAccum[ 0 ], nMinVal, nMaxVal );
		float flDelta = flFactor * m_nAccum[ 0 ];

		float flLeftDelta, flRightDelta;
		ValueBalanceToLeftRight( &flLeftDelta, &flRightDelta, flDelta, flBalance, 0.0f );

		flLeftValue  = clamp( flLeftValue  + flLeftDelta,  flMinVal, flMaxVal );
		flRightValue = clamp( flRightValue + flRightDelta, flMinVal, flMaxVal );

		SetValue( ANIM_CONTROL_VALUE_LEFT,  flLeftValue );
		SetValue( ANIM_CONTROL_VALUE_RIGHT, flRightValue );
	}
	else
	{
		AnimationControlType_t type = ANIM_CONTROL_VALUE;

		float flValue = m_dragStartValues.m_pValue[ type ];
		int nMinVal = floor( ( flMinVal - flValue ) / flFactor );
		int nMaxVal = ceil ( ( flMaxVal - flValue ) / flFactor );
		m_nAccum[ 0 ] = clamp( m_nAccum[ 0 ], nMinVal, nMaxVal );
		float flDelta = flFactor * m_nAccum[ 0 ];

		flValue = clamp( flValue + flDelta, flMinVal, flMaxVal );
		SetValue( type, flValue );
	}

	// Slam the cursor back to the drag start point
	if ( x != m_nDragStartPosition[ 0 ] || y != m_nDragStartPosition[ 1 ] )
	{
		x = m_nDragStartPosition[ 0 ];
		y = m_nDragStartPosition[ 1 ];
		LocalToScreen( x, y );
		input()->SetCursorPos( x, y );
	}
}

void CAttributeSlider::OnMouseReleased( MouseCode code )
{
	if ( !IsEnabled() || m_bTransform )
		return;

	if ( code == MOUSE_RIGHT )
	{
		if ( m_hContextMenu.Get() )
		{
			delete m_hContextMenu.Get();
			m_hContextMenu = NULL;
		}

		m_hContextMenu = new Menu( this, "ActionMenu" );

		int x,y;
		input()->GetCursorPosition( x, y );
		ScreenToLocal( x, y );
		m_hContextMenu->AddMenuItem( "Set To Default", new KeyValues( "SetToDefault" ), this );

		if ( CDmeChannel *pChannel = m_hControl->GetValueElement< CDmeChannel >( "channel" ) )
		{
			CDmElement *pToElement = pChannel->GetToElement();
			if ( pToElement && pToElement->IsA< CDmeExpressionOperator >() )
			{
				m_hContextMenu->AddMenuItem( "Edit Min/Max/Default...", new KeyValues( "EditMinMaxDefault" ), this );
			}
		}

		Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
		return;
	}

	if ( !IsDragging() )
		return;

	m_SliderMode = SLIDER_MODE_NONE;
	input()->SetMouseCapture( NULL );
	SetCursor( dc_arrow );

	m_pParent->UpdatePreview( "Attribute Slider Released" );
}



//-----------------------------------------------------------------------------
//
// Methods related to text entry mode
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Called by the text entry code to enter the value into the logs
//-----------------------------------------------------------------------------
void CAttributeSlider::StampValueIntoLogs( AnimationControlType_t type, float flValue )
{
	Assert( !m_bTransform );
	Assert( type < ANIM_CONTROL_COUNT );
	m_pParent->StampValueIntoLogs( m_hControl, type, flValue );
}

void  CAttributeSlider::StampValueIntoLogs( AnimationControlType_t type, const Vector &vecValue )
{
	Assert( m_bTransform );
	Assert( type == ANIM_CONTROL_TXFORM_POSITION || type == ANIM_CONTROL_TXFORM_ORIENTATION );
	m_pParent->StampValueIntoLogs( m_hControl, type, vecValue );
}

void  CAttributeSlider::StampValueIntoLogs( AnimationControlType_t type, const Quaternion &qValue )
{
	Assert( m_bTransform );
	Assert( type == ANIM_CONTROL_TXFORM_ORIENTATION );
	m_pParent->StampValueIntoLogs( m_hControl, type, qValue );
}

//-----------------------------------------------------------------------------
// Key typed key handler
//-----------------------------------------------------------------------------
void CAttributeSlider::OnKeyCodeTyped( KeyCode code )
{
	if ( !IsInTextEntry() )
	{
		BaseClass::OnKeyCodeTyped( code );
		return;
	}

	switch ( code )
	{
	default:
		BaseClass::OnKeyCodeTyped( code );
		break;

	case KEY_ESCAPE:
		DiscardTextEntryValue();
		break;

	case KEY_ENTER:
		AcceptTextEntryValue();
		break;
	}
}

void CAttributeSlider::SetupTextFieldForTextEntryMode( CAttributeSliderTextEntry *&pTextField, const char *pText, bool bRequestFocus )
{
	if ( !pTextField )
	{
		pTextField = new CAttributeSliderTextEntry( this, GetName() );
		pTextField->SetVisible( false );
		pTextField->SetEnabled( false );
		pTextField->SelectAllOnFocusAlways( true );
		InvalidateLayout();
	}

	pTextField->SetVisible( true );
	pTextField->SetEnabled( true );

	pTextField->SetText( pText );

	pTextField->GotoTextEnd();
	if ( bRequestFocus )
	{
		pTextField->RequestFocus();
	}
}

//-----------------------------------------------------------------------------
// Methods to entry text entry mode
//-----------------------------------------------------------------------------
void CAttributeSlider::EnterTextEntryMode( bool bRelatchValues )
{
	m_SliderMode = SLIDER_MODE_TEXT;

	// For double-clicking, ignore the value set by the first single mouse click
	if ( !bRelatchValues )
	{
		SetValue( m_InitialTextEntryValue );
	}

	if ( !IsTransform() )
	{
		if ( m_bStereo )
		{
			char val[ 64 ];
			V_snprintf( val, sizeof( val ), "%f", m_InitialTextEntryValue.m_pValue[ ANIM_CONTROL_VALUE_LEFT ] );
			SetupTextFieldForTextEntryMode( m_pTextField, val, true );

			V_snprintf( val, sizeof( val ), "%f", m_InitialTextEntryValue.m_pValue[ ANIM_CONTROL_VALUE_RIGHT ] );
			SetupTextFieldForTextEntryMode( m_pRightTextField, val, false );
		}
		else
		{
			char val[ 64 ];
			Q_snprintf( val, sizeof( val ), "%f", m_InitialTextEntryValue.m_pValue[ ANIM_CONTROL_VALUE ] );
			SetupTextFieldForTextEntryMode( m_pTextField, val, true );
		}
	}
	else
	{
		char val[ 128 ];
		Q_snprintf( val, sizeof( val ), "%f %f %f", VectorExpand( m_InitialTextEntryValue.m_Vector ) );
		SetupTextFieldForTextEntryMode( m_pTextField, val, true );
	}
}


//-----------------------------------------------------------------------------
// Methods to accept or discard the value in the text entry field
//-----------------------------------------------------------------------------
void CAttributeSlider::AcceptTextEntryValue()
{
	if ( !IsInTextEntry() )
		return;

	// Get the value in the text entry field
	char buf[ 64 ];
	m_pTextField->GetText( buf, sizeof( buf ) );
	// Hide the text entry
	m_pTextField->SetVisible( false );
	m_pTextField->SetEnabled( false );

	CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( m_hControl );

	if ( !IsTransform() )
	{
		float flValue = Q_atof( buf );

		if ( m_bStereo )
		{
			float flLeftValue = flValue;
			SetValue( ANIM_CONTROL_VALUE_LEFT, flLeftValue );
			StampValueIntoLogs( ANIM_CONTROL_VALUE_LEFT, flLeftValue );

			// Get the value in the text entry field
			char buf[ 64 ];
			m_pRightTextField->GetText( buf, sizeof( buf ) );
			float flRightValue = Q_atof( buf );

			// Hide the text entry
			m_pRightTextField->SetVisible( false );
			m_pRightTextField->SetEnabled( false );

			SetValue( ANIM_CONTROL_VALUE_RIGHT, flRightValue );
			StampValueIntoLogs( ANIM_CONTROL_VALUE_RIGHT, flRightValue );
		}
		else
		{
			SetValue( ANIM_CONTROL_VALUE, flValue );
			StampValueIntoLogs( ANIM_CONTROL_VALUE, flValue );
		}
	}
	else if ( pTransformControl )
	{
		Vector vecValue;
		if ( 3 == sscanf( buf, "%f %f %f", &vecValue.x, &vecValue.y, &vecValue.z ) )
		{
			if ( !m_bOrientation )
			{
				SetValue( ANIM_CONTROL_TXFORM_POSITION, vecValue );
				StampValueIntoLogs( ANIM_CONTROL_TXFORM_POSITION, vecValue );
			}
			else
			{
				StampValueIntoLogs( ANIM_CONTROL_TXFORM_ORIENTATION, vecValue );
				SetValue( ANIM_CONTROL_TXFORM_ORIENTATION, pTransformControl->GetOrientation() );

				/*
				QAngle angValue;
				if ( 3 == sscanf( buf, "%f %f %f", &angValue.x, &angValue.y, &angValue.z ) )
				{
					// Convert back to a quat
					Quaternion qValue;
					AngleQuaternion( angValue, qValue );

					SetValue( ANIM_CONTROL_TXFORM_ORIENTATION, qValue );
					StampValueIntoLogs( ANIM_CONTROL_TXFORM_ORIENTATION, qValue );
				}
				*/
			}
		}
		

		m_pParent->UpdatePreview( "AcceptTextEntryValue\n" );
	}

	m_SliderMode = SLIDER_MODE_NONE;
	RequestFocus();
}

void CAttributeSlider::DiscardTextEntryValue()
{
	if ( !IsInTextEntry() )
		return;

	// Hide the text entry
	m_pTextField->SetVisible( false );
	m_pTextField->SetEnabled( false );

	if ( m_bStereo )
	{
		m_pRightTextField->SetVisible( false );
		m_pRightTextField->SetEnabled( false );
	}

	m_SliderMode = SLIDER_MODE_NONE;
	RequestFocus();
}


//-----------------------------------------------------------------------------
// Methods of the text entry widget
//-----------------------------------------------------------------------------
void CAttributeSliderTextEntry::OnKillFocus( KeyValues *pParams )
{
	Assert( m_pSlider );

	SelectNone();

	VPANEL hPanel = (VPANEL)pParams->GetPtr( "newPanel" );
	if ( hPanel != INVALID_PANEL && vgui::ipanel()->GetParent( hPanel ) == m_pSlider->GetVPanel() )
		return;

	m_pSlider->AcceptTextEntryValue();
}

void CAttributeSliderTextEntry::OnMouseWheeled( int delta )
{
	if ( m_pSlider->IsTransform() )
		return;

	if ( m_pSlider->IsStereo() )
		return;

	float deltaFactor;
	if ( input()->IsKeyDown(KEY_LSHIFT) )
	{
		deltaFactor = ((float)delta) * 10.0f;
	}
	else if ( input()->IsKeyDown(KEY_LCONTROL) )
	{
		deltaFactor = ((float)delta) / 100.0;
	}
	else
	{
		deltaFactor = ((float)delta) / 10.0;
	}

	char sz[ 64 ];
	GetText( sz, sizeof( sz ) );

	float val = Q_atof( sz ) + deltaFactor;
	if ( input()->IsKeyDown(KEY_LALT) )
	{
		val = clamp( val, 0.0f, 1.0f );
	}

	Q_snprintf( sz, sizeof( sz ), "%f", val );

	SetText( sz );
	m_pSlider->SetValue( ANIM_CONTROL_VALUE, val );

	CUndoScopeGuard guard( UNDO_CHAIN_MOUSEWHEEL_ATTRIBUTE_SLIDER, "Set Slider Value" );

	m_pSlider->StampValueIntoLogs( ANIM_CONTROL_VALUE, val );
}

void CAttributeSlider::OnMouseDoublePressed( MouseCode code )
{
	if ( !IsEnabled() || IsDragging() )
		return;

	if ( code != MOUSE_LEFT )
		return;

	int x,y;
	input()->GetCursorPosition( x, y );
	ScreenToLocal( x, y );
	EnterTextEntryMode( false );
}


//-----------------------------------------------------------------------------
//
// Methods related to preview
//
//-----------------------------------------------------------------------------
void CAttributeSlider::UpdateFaderAmount( float flAmount )
{
	m_flFaderAmount = flAmount;
	BlendValues( m_bTransform, &m_PreviewCurrent, GetValue(), m_PreviewFull, flAmount );
}

void CAttributeSlider::SetPreview( const AttributeValue_t &value, const AttributeValue_t &full )
{
	m_PreviewCurrent = value;
	m_PreviewFull = full;
}

const AttributeValue_t &CAttributeSlider::GetPreview() const
{
	return m_PreviewCurrent;
}

const AttributeValue_t &CAttributeSlider::GetPreviewFull() const
{
	return m_PreviewFull;
}


// Estimates the value of the control given a local coordinate
float CAttributeSlider::EstimateValueAtPos( int nLocalX, int nLocalY ) const
{
	Rect_t rect;
	GetControlRect( &rect ); // assumes controls are same width

	float flFactor = rect.width > 1 ? (float)( nLocalX - rect.x ) / (float)( rect.width - 1 ) : 0.5f;
	flFactor = clamp( flFactor, 0.0f, 1.0f );
	return flFactor;
}


//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------
void CAttributeSlider::PerformLayout()
{
	BaseClass::PerformLayout();

	if ( !m_pTextField )
		return;

	Rect_t rect;
	GetControlRect( &rect );

	// Place the text entry along the main attribute track rectangle
	if ( m_bStereo )
	{
		m_pTextField     ->SetBounds( rect.x,                  rect.y, rect.width / 2, rect.height );
		m_pRightTextField->SetBounds( rect.x + rect.width / 2, rect.y, rect.width / 2, rect.height );
	}
	else
	{
		m_pTextField->SetBounds( rect.x, rect.y, rect.width, rect.height );
	}
}

void CAttributeSlider::GetControlRect( Rect_t *pRect ) const
{
	int sw, sh;
	const_cast<CAttributeSlider*>( this )->GetSize( sw, sh );

	pRect->x = 2 * SLIDER_PIXEL_SPACING;
	pRect->y = SLIDER_PIXEL_SPACING;
	pRect->width = sw - pRect->x * 2;
	pRect->height = MAX( 0, sh - SLIDER_PIXEL_SPACING * 2 );
}


//-----------------------------------------------------------------------------
//
// Methods related to painting start here
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Used to control how fader-driven ticks look
//-----------------------------------------------------------------------------
float CAttributeSlider::GetPreviewAlphaScale() const
{
	return MAX( m_flFaderAmount, 0.1f );
}


//-----------------------------------------------------------------------------
// Draws a tick on the main control
//-----------------------------------------------------------------------------
void DrawTick( float flValue, int xbase, int nTotalWidth, int nTickWidth, int y, int nHeight )
{
	int x = xbase + (int)( flValue * (float)nTotalWidth + 0.5f ) - nTickWidth / 2;
	x = clamp( x, xbase, xbase + nTotalWidth - nTickWidth );
	surface()->DrawFilledRect( x, y, x + nTickWidth, y + nHeight );
}

void CAttributeSlider::DrawTick( const Color& clr, const AttributeValue_t &value, int width, int inset )
{
	surface()->DrawSetColor( clr );

	// Get the control position
	Rect_t rect;
	GetControlRect( &rect );

	// Inset by 1 pixel
	rect.x++; rect.y++; rect.width -= 2; rect.height -= 2;

	int previewtall = rect.height - 2 * inset;
	int ypos = rect.y + ( rect.height - previewtall ) / 2;

	if ( m_bStereo )
	{
		previewtall /= 2;
		::DrawTick( value.m_pValue[ ANIM_CONTROL_VALUE_LEFT  ], rect.x, rect.width, width, ypos, previewtall );
		::DrawTick( value.m_pValue[ ANIM_CONTROL_VALUE_RIGHT ], rect.x, rect.width, width, ypos + previewtall, previewtall );
	}
	else
	{
		::DrawTick( value.m_pValue[ ANIM_CONTROL_VALUE ], rect.x, rect.width, width, ypos + previewtall, previewtall );
	}
}


//-----------------------------------------------------------------------------
// Paints ticks
//-----------------------------------------------------------------------------
void CAttributeSlider::Paint()
{
	if ( IsTransform() )
		return;

	DrawTick( s_OldValueTickColor, GetValue(), 1, 0 );

	bool shiftDown = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
	bool bPreviewingAttributeSlider = m_pParent->GetController()->GetActiveAttributeSlider() == this && !IsDragging() && shiftDown;
	bool bMouseOverPreviewSlider = m_pParent->GetEditor()->GetPresetFader()->GetActivePresetSlider() ? true : false;
	if ( bPreviewingAttributeSlider || bMouseOverPreviewSlider )
	{
		Color col = s_PreviewTickColor;
		col[ 3 ] *= GetPreviewAlphaScale();

		DrawTick( col, m_PreviewFull, 2, 2 );
	}
}


//-----------------------------------------------------------------------------
// Draws the min, current, and max values for the slider
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawValueLabel()
{
	if ( IsTransform() )
		return;

	float flMinVal = 0.0f;
	float flMaxVal = 1.0f;

	Rect_t rect;
	GetControlRect( &rect );

	int cw, ch;
	char sz[ 32 ];
	Q_snprintf( sz, sizeof( sz ), "%.1f", flMinVal );
	m_pValues[ 0 ]->SetText( sz );
	m_pValues[ 0 ]->ResizeImageToContent();
	m_pValues[ 0 ]->GetContentSize( cw, ch );
	m_pValues[ 0 ]->SetPos( rect.x + 5, rect.y + ( rect.height - ch ) * 0.5f );
	m_pValues[ 0 ]->Paint();

	Q_snprintf( sz, sizeof( sz ), "%.1f", flMaxVal );
	m_pValues[ 1 ]->SetText( sz );
	m_pValues[ 1 ]->ResizeImageToContent();
	m_pValues[ 1 ]->GetContentSize( cw, ch );
	m_pValues[ 1 ]->SetPos( rect.x + rect.width - cw - 5, rect.y + ( rect.height - ch ) * 0.5f );
	m_pValues[ 1 ]->Paint();

	if ( m_bStereo )
	{
		float flLeftValue = clamp( GetValue().m_pValue[ ANIM_CONTROL_VALUE_LEFT ], flMinVal, flMaxVal );
		Q_snprintf( sz, sizeof( sz ), "%.3f", flLeftValue );
		m_pValues[ 2 ]->SetText( sz );
		m_pValues[ 2 ]->ResizeImageToContent();
		m_pValues[ 2 ]->GetContentSize( cw, ch );
		m_pValues[ 2 ]->SetPos( rect.x + ( rect.width - cw ) * 0.4f, rect.y + ( rect.height - ch ) * 0.5f );
		m_pValues[ 2 ]->Paint();

		float flRightValue = clamp( GetValue().m_pValue[ ANIM_CONTROL_VALUE_RIGHT ], flMinVal, flMaxVal );
		Q_snprintf( sz, sizeof( sz ), "%.3f", flRightValue );
		m_pValues[ 3 ]->SetText( sz );
		m_pValues[ 3 ]->ResizeImageToContent();
		m_pValues[ 3 ]->GetContentSize( cw, ch );
		m_pValues[ 3 ]->SetPos( rect.x + ( rect.width - cw ) * 0.6f, rect.y + ( rect.height - ch ) * 0.5f );
		m_pValues[ 3 ]->Paint();
	}
	else
	{
		float flValue = clamp( GetValue().m_pValue[ ANIM_CONTROL_VALUE ], flMinVal, flMaxVal );
		Q_snprintf( sz, sizeof( sz ), "%.3f", flValue );
		m_pValues[ 2 ]->SetText( sz );
		m_pValues[ 2 ]->ResizeImageToContent();
		m_pValues[ 2 ]->GetContentSize( cw, ch );
		m_pValues[ 2 ]->SetPos( rect.x + ( rect.width - cw ) * 0.5f, rect.y + ( rect.height - ch ) * 0.5f );
		m_pValues[ 2 ]->Paint();
	}
}


//-----------------------------------------------------------------------------
// Draws the text for the slider. It's either the slider name, or its value if dragging is happening
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawNameLabel()
{
	if ( IsDragging() )
	{
		DrawValueLabel();
		return;
	}

	if ( IsInTextEntry() )
		return;

	if ( !m_pName )
		return;

	int cw, ch;

	Color clr = s_TextColor;
	if ( m_pParent->GetController()->GetActiveAttributeSlider() == this )
	{
		clr = s_TextColorFocus;
	} 
	else if ( m_bDependent )
	{
		clr = s_TextColorDependent;
	}

	m_pName->SetColor( clr );
	m_pName->GetContentSize( cw, ch );

	Rect_t rect;
	GetControlRect( &rect );

	m_pName->SetPos( rect.x + ( rect.width - cw ) * 0.5f, rect.y + ( rect.height - ch ) * 0.5f );
	m_pName->Paint();
}


//-----------------------------------------------------------------------------
// Draws the midpoint value for the slider
//-----------------------------------------------------------------------------
void CAttributeSlider::DrawMidpoint( int x, int ty, int ttall )
{
	if ( IsTransform() )
		return;
	surface()->DrawSetColor( s_MidpointColor );
	surface()->DrawFilledRect( x, ty, x + 1, ty + ttall );
}


//-----------------------------------------------------------------------------
// Paints the slider
//-----------------------------------------------------------------------------
void CAttributeSlider::PaintBackground()
{
	Rect_t rect;
	GetControlRect( &rect );

	// Paint the border
	surface()->DrawSetColor( Color( 24, 24, 24, 255 ) );
	// top and left
	surface()->DrawOutlinedRect( rect.x, rect.y, rect.x + rect.width, rect.y + 1 );
	surface()->DrawOutlinedRect( rect.x, rect.y, rect.x + 1, rect.y + rect.height );
	// right
	surface()->DrawSetColor( Color( 33, 33, 33, 255 ) );
	surface()->DrawOutlinedRect( rect.x + rect.width - 1, rect.y, rect.x + rect.width, rect.y + rect.height );
	// bottom
	surface()->DrawSetColor( Color( 56, 56, 56, 255 ) );
	surface()->DrawOutlinedRect( rect.x, rect.y + rect.height - 1, rect.x + rect.width, rect.y + rect.height );

	// Inset the rect by 1 pixel
	++rect.x; ++rect.y; rect.width -= 2; rect.height -= 2;

	int y0 = rect.y;
	int y1 = rect.y + rect.height / 2;
	int y2 = rect.y + rect.height;

	bool bIsLogPreviewControl = ( m_pParent->GetController()->GetActiveAttributeSlider() == this );

	// Draw the main bar background
	surface()->DrawSetColor( s_ZeroColor[ bIsLogPreviewControl ] );
	surface()->DrawFilledRect( rect.x, y0, rect.x + rect.width, y2 );

	if ( IsInTextEntry() )
		return;

	if ( !IsTransform() )
	{
		CBaseAnimationSetControl *pController = m_pParent->GetController();
		bool shiftDown = input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT );
		bool bPreviewingAttributeSlider = pController->GetActiveAttributeSlider() == this && !IsDragging() && shiftDown;

		bool bDraggingPreviewSlider = pController->IsPresetFaderBeingDragged();
		bool bPreviewingPreset = pController->WasPreviouslyHoldingPresetPreviewKey();

		bool bUsePreview = bPreviewingAttributeSlider || bDraggingPreviewSlider || bPreviewingPreset;

		float flDefaultValue = m_hControl->GetValue< float >( DEFAULT_FLOAT_ATTR );
		int nMidPoint = (int)( (float)rect.width * clamp( flDefaultValue, 0.0f, 1.0f ) + 0.5f );

		if ( m_bStereo )
		{
			const AttributeValue_t &value = bUsePreview ? m_PreviewCurrent : GetValue();
			float flLeftValue  = value.m_pValue[ ANIM_CONTROL_VALUE_LEFT  ];
			float flRightValue = value.m_pValue[ ANIM_CONTROL_VALUE_RIGHT ];

			int nLeftValue  = (int)( (float)rect.width * clamp( flLeftValue,  0.0f, 1.0f ) + 0.5f );
			int nRightValue = (int)( (float)rect.width * clamp( flRightValue, 0.0f, 1.0f ) + 0.5f );

			// Draw the current value as a bar from the midpoint
			surface()->DrawSetColor( IsDragging() ? s_DraggingBarColor : s_BarColor[ bIsLogPreviewControl ] );
			surface()->DrawFilledRect( rect.x + MIN( nLeftValue,  nMidPoint ), y0, rect.x + MAX( nLeftValue,  nMidPoint ), y1 );
			surface()->DrawFilledRect( rect.x + MIN( nRightValue, nMidPoint ), y1, rect.x + MAX( nRightValue, nMidPoint ), y2 );
		}
		else
		{
			const AttributeValue_t &value = bUsePreview ? m_PreviewCurrent : GetValue();
			float flValue = value.m_pValue[ ANIM_CONTROL_VALUE ];
			int nValue = (int)( (float)rect.width * clamp( flValue, 0.0f, 1.0f ) + 0.5f );

			// Draw the current value as a bar from the midpoint
			surface()->DrawSetColor( IsDragging() ? s_DraggingBarColor : s_BarColor[ bIsLogPreviewControl ] );
			surface()->DrawFilledRect( rect.x + MIN( nValue, nMidPoint ), y0, rect.x + MAX( nValue, nMidPoint ), y2 );
		}

		// Draw the midpoint over the top of the current value
		DrawMidpoint( rect.x + nMidPoint, rect.y, rect.height );
	}

	// Draw the name or value over the top of that
	DrawNameLabel();
}

//-----------------------------------------------------------------------------
// Manipulate in/out curve types
//-----------------------------------------------------------------------------
void CAttributeSlider::OnCurve1()
{
	m_pParent->DispatchCurve( 1 );
}

void CAttributeSlider::OnCurve2()
{
	m_pParent->DispatchCurve( 2 );
}

void CAttributeSlider::OnCurve3()
{
	m_pParent->DispatchCurve( 3 );
}

void CAttributeSlider::OnCurve4()
{
	m_pParent->DispatchCurve( 4 );
}

//-----------------------------------------------------------------------------
//
// Slider dependency functions, provide management and information about the
// other sliders on which the function of this slider depends.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Clear the list of sliders that this slider is dependent on
//-----------------------------------------------------------------------------
void CAttributeSlider::ClearDependencies()
{
	m_Dependenices.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Add a slider to the list of sliders this slider is dependent on
//-----------------------------------------------------------------------------
bool CAttributeSlider::AddDependency( const CAttributeSlider* pSlider )
{
	if ( pSlider == NULL )
		return false;

	// Make sure the slider is not already in the dependency list
	if ( m_Dependenices.Find( pSlider ) != m_Dependenices.InvalidIndex() )
		return false;

	m_Dependenices.AddToTail( pSlider );
	return true;	
}


//-----------------------------------------------------------------------------
// Purpose: Check the dependency list to see the operation of this slider is 
// dependent of the specified slider.
//-----------------------------------------------------------------------------
bool CAttributeSlider::IsDependent( const CAttributeSlider* pSlider ) const
{
	return ( m_Dependenices.Find( pSlider) != m_Dependenices.InvalidIndex() );
}


//-----------------------------------------------------------------------------
// Purpose: Set the flag indicating that the operation of the slider is 
// dependent on the currently selected slider.
//-----------------------------------------------------------------------------
void CAttributeSlider::SetDependent( bool dependent )
{
	m_bDependent = dependent;
}
