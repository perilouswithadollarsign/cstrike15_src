//======= Copyright © 1996-2008, Valve Corporation, All rights reserved. ======
//
// Purpose: A 2D Slider
//
//=============================================================================


// Valve includes
#include <KeyValues.h>

#include <vgui/MouseCode.h>
#include <vgui/IBorder.h>
#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>

#include <vgui_controls/Controls.h>
#include <vgui_controls/TextImage.h>
#include <dme_controls/2DSlider.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


using namespace vgui;


//-----------------------------------------------------------------------------
// Statics
//-----------------------------------------------------------------------------
Color C2DSlider::s_TextColor( 208, 143, 40, 192 );
Color C2DSlider::s_NobColor( 0, 63, 98, 255 );
Color C2DSlider::s_TickColor( 0, 79, 182, 255 );
Color C2DSlider::s_TickFillXColor( 0, 63, 0, 255 );
Color C2DSlider::s_TickFillYColor( 0, 0, 98, 255 );
Color C2DSlider::s_TickFillColor( 0, 63, 98, 255 );
Color C2DSlider::s_TrackColor( 31, 31, 31, 255 );


//-----------------------------------------------------------------------------
// Purpose: Create a slider bar with ticks underneath it
//-----------------------------------------------------------------------------
C2DSlider::C2DSlider( Panel *pParent, const char *pName )
: BaseClass( pParent, pName )
{
	m_bDrawLabel = true;
	m_bIsDragOnRepositionNob = true;
	m_bDragging = false;

	m_fValue[ kXAxis ] = 0.0f;
	m_fValue[ kYAxis ] = 0.0f;

	m_fRange[ kXAxis ][ 0 ] = 0.0f;
	m_fRange[ kXAxis ][ 1 ] = 1.0f;
	m_fRange[ kYAxis ][ 0 ] = 1.0f;
	m_fRange[ kYAxis ][ 1 ] = 0.0f;

	m_pNobBorder = NULL;
	m_pInsetBorder = NULL;

	SetNobSize( 7, 7 );
	RecomputeNobPosFromValue();
	AddActionSignalTarget( this );
	SetBlockDragChaining( true );

	m_pLabel = new TextImage( pName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
C2DSlider::~C2DSlider()
{
	delete m_pLabel;
}


//-----------------------------------------------------------------------------
// Purpose: Set the value of the slider
//-----------------------------------------------------------------------------
void C2DSlider::SetValueX( float fValueX, bool bTriggerChangeMessage /* = true */ )
{
	SetValue( fValueX, GetValueY(), bTriggerChangeMessage );
}


//-----------------------------------------------------------------------------
// Purpose: Set the value of the slider
//-----------------------------------------------------------------------------
float C2DSlider::GetValueX() const
{
	return m_fValue[ kXAxis ];
}


//-----------------------------------------------------------------------------
// Purpose: Set the value of the slider
//-----------------------------------------------------------------------------
void C2DSlider::SetValueY( float fValueY, bool bTriggerChangeMessage /* = true */ )
{
	SetValue( GetValueX(), fValueY, bTriggerChangeMessage );
}


//-----------------------------------------------------------------------------
// Purpose: Set the value of the slider to one of the ticks.
//-----------------------------------------------------------------------------
float C2DSlider::GetValueY() const
{
	return m_fValue[ kYAxis ];
}


//-----------------------------------------------------------------------------
// Purpose: Set the value of the slider to one of the ticks.
//-----------------------------------------------------------------------------
void C2DSlider::SetValue( float fValueX, float fValueY, bool bTriggerChangeMessage )
{
	fValueX = RemapValClamped( fValueX, m_fRange[ kXAxis ][ 0 ], m_fRange[ kXAxis ][ 1 ], m_fRange[ kXAxis ][ 0 ], m_fRange[ kXAxis ][ 1 ] );
	fValueY = RemapValClamped( fValueY, m_fRange[ kYAxis ][ 0 ], m_fRange[ kYAxis ][ 1 ], m_fRange[ kYAxis ][ 0 ], m_fRange[ kYAxis ][ 1 ] );

	if ( fValueX != m_fValue[ kXAxis ] || fValueY != m_fValue[ kYAxis ] )
	{
		m_fValue[ kXAxis ] = fValueX;
		m_fValue[ kYAxis ] = fValueY;

		RecomputeNobPosFromValue();

		if ( bTriggerChangeMessage )
		{
			SendSliderMovedMessage();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Return the value of the slider
//-----------------------------------------------------------------------------
void C2DSlider::GetValue( float &fValueX, float &fValueY ) const
{
	fValueX = GetValueX();
	fValueY = GetValueY();
}


//-----------------------------------------------------------------------------
// Purpose: Set the range of the slider.
//-----------------------------------------------------------------------------
void C2DSlider::SetRange( float fMinX, float fMaxX, float fMinY, float fMaxY, bool bTriggerChangeMessage /* = true */ )
{
	m_fRange[ kXAxis ][ 0 ] = fMinX;
	m_fRange[ kXAxis ][ 1 ] = fMaxX;
	m_fRange[ kYAxis ][ 0 ] = fMinY;
	m_fRange[ kYAxis ][ 1 ] = fMaxY;

	SetValue( m_fValue[ kXAxis ], m_fValue[ kYAxis ], bTriggerChangeMessage );
}


//-----------------------------------------------------------------------------
// Purpose: Get the max and min values of the slider
//-----------------------------------------------------------------------------
void C2DSlider::GetRange( float &fMinX, float &fMaxX, float &fMinY, float &fMaxY ) const
{
	fMinX = m_fRange[ kXAxis ][ 0 ];
	fMaxX = m_fRange[ kXAxis ][ 1 ];
	fMinY = m_fRange[ kYAxis ][ 0 ];
	fMaxY = m_fRange[ kYAxis ][ 1 ];
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::SetLabelText( const char *pText )
{
	m_pLabel->SetText( pText );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::SetLabelText( const wchar_t *pText )
{
	m_pLabel->SetText( pText );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::GetLabelText( wchar_t *pBuffer, int nBufferLen ) const
{
	m_pLabel->GetText( pBuffer, nBufferLen );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::GetLabelUnlocalizedText( char *pBuffer, int nBufferLen ) const
{
	m_pLabel->GetUnlocalizedText( pBuffer, nBufferLen );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::SetDrawLabel( bool bState )
{
	m_bDrawLabel = bState;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool C2DSlider::IsDrawingLabel() const
{
	return m_bDrawLabel;
}


//-----------------------------------------------------------------------------
// Purpose: Get the nob's position ( the ends of each side of the nob )
//-----------------------------------------------------------------------------
void C2DSlider::GetNobPos( int &nX, int &nY )
{
	nX = m_nNobPos[ kXAxis ];
	nY = m_nNobPos[ kYAxis ];
}


//-----------------------------------------------------------------------------
// Purpose: Respond when the cursor is moved in our window if we are clicking
// and dragging.
//-----------------------------------------------------------------------------
void C2DSlider::OnCursorMoved( int nMouseX, int nMouseY )
{
	if ( !m_bDragging )
		return;

	input()->GetCursorPosition( nMouseX, nMouseY );
	ScreenToLocal( nMouseX, nMouseY );

	int nTrackX, nTrackY, nTrackWide, nTrackTall;
	GetTrackRect( nTrackX, nTrackY, nTrackWide, nTrackTall );

	m_nNobPos[ kXAxis ] = clamp( m_nNobDragStartPos[ kXAxis ] + nMouseX - m_nDragStartPos[ kXAxis ], nTrackX, nTrackX + nTrackWide - 1 );
	m_nNobPos[ kYAxis ] = clamp( m_nNobDragStartPos[ kYAxis ] + nMouseY - m_nDragStartPos[ kYAxis ], nTrackY, nTrackY + nTrackTall - 1 );

	RecomputeValueFromNobPos( false );

	Repaint();

	SendSliderMovedMessage();
}


//-----------------------------------------------------------------------------
// Purpose: Respond to mouse presses. Trigger Record staring positon.
//-----------------------------------------------------------------------------
void C2DSlider::OnMousePressed( MouseCode /* mouseCode */ )
{
	if ( !IsEnabled() )
		return;

	/*
	TODO: Do this in Maya
	if ( input()->IsKeyDown( KEY_LSHIFT ) || input()->IsKeyDown( KEY_RSHIFT ) )
		return;

	if ( input()->IsKeyDown( KEY_LCONTROL ) || input()->IsKeyDown( KEY_LCONTROL ) )
		return;
	*/

	int nMouseX;
	int nMouseY;
	input()->GetCursorPosition( nMouseX, nMouseY );

	ScreenToLocal( nMouseX, nMouseY );
	RequestFocus();

	bool bStartDragging = false;
	bool bPostDragStartSignal = false;

	if (
		nMouseX >= ( m_nNobPos[ kXAxis ] - m_nNobHalfSize[ kXAxis ] ) &&
		nMouseX <= ( m_nNobPos[ kXAxis ] + m_nNobHalfSize[ kXAxis ] ) &&
		nMouseY >= ( m_nNobPos[ kYAxis ] - m_nNobHalfSize[ kYAxis ] ) &&
		nMouseY <= ( m_nNobPos[ kYAxis ] + m_nNobHalfSize[ kYAxis ] ) )
	{
		bStartDragging = true;
		bPostDragStartSignal = true;
	}
	else
	{
		// we clicked elsewhere on the slider; move the nob to that position
		int nTrackX, nTrackY, nTrackWide, nTrackTall;
		GetTrackRect( nTrackX, nTrackY, nTrackWide, nTrackTall );

		m_nNobPos[ kXAxis ] = clamp( nMouseX, nTrackX, nTrackX + nTrackWide - 1 );
		m_nNobPos[ kYAxis ] = clamp( nMouseY, nTrackY, nTrackY + nTrackTall - 1 );

		RecomputeValueFromNobPos( false );

		Repaint();

		SendSliderMovedMessage();

		m_nNobDragStartPos[ kXAxis ] = nMouseX;
		m_nNobDragStartPos[ kYAxis ] = nMouseY;
		m_nDragStartPos[ kXAxis ] = nMouseX;
		m_nDragStartPos[ kYAxis ] = nMouseY;

		OnCursorMoved( nMouseX, nMouseY );

		bStartDragging = IsDragOnRepositionNob();

		if ( bStartDragging )
		{
			SendSliderDragStartMessage();
		}
	}

	if ( bStartDragging )
	{
		// drag the nob
		m_bDragging = true;
		input()->SetMouseCapture( GetVPanel() );

		m_nNobDragStartPos[ kXAxis ] = m_nNobPos[ kXAxis ];
		m_nNobDragStartPos[ kYAxis ] = m_nNobPos[ kYAxis ];
		m_nDragStartPos[ kXAxis ] = nMouseX;
		m_nDragStartPos[ kYAxis ] = nMouseY;
	}

	if ( bPostDragStartSignal )
	{
		SendSliderDragStartMessage();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Just handle double presses like mouse presses
//-----------------------------------------------------------------------------
void C2DSlider::OnMouseDoublePressed( MouseCode mouseCode )
{
	OnMousePressed( mouseCode );
}


//-----------------------------------------------------------------------------
// Purpose: Stop dragging when the mouse is released.
//-----------------------------------------------------------------------------
void C2DSlider::OnMouseReleased( MouseCode /* mouseCode */ )
{
	if ( m_bDragging )
	{
		m_bDragging = false;
		input()->SetMouseCapture( 0 );
		SendSliderDragEndMessage();
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::SetNobWidth( int nWidth )
{
	m_nNobHalfSize[ kXAxis ] = ( ( nWidth | 1 ) - 1 ) / 2;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int C2DSlider::GetNobWidth() const
{
	return m_nNobHalfSize[ kXAxis ] * 2 + 1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::SetNobTall( int nTall )
{
	m_nNobHalfSize[ kYAxis ] = ( ( nTall | 1 ) - 1 ) / 2;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int C2DSlider::GetNobTall() const
{
	return m_nNobHalfSize[ kYAxis ] * 2 + 1;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::SetNobSize( int nWidth, int nTall )
{
	SetNobWidth( nWidth );
	SetNobTall( nTall );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::GetNobSize( int &nHalfWidth, int &nHalfTall ) const
{
	nHalfWidth = GetNobWidth();
	nHalfTall = GetNobTall();
}


//-----------------------------------------------------------------------------
// Purpose: If you click on the slider outside of the nob, the nob jumps
// to the click position, and if this setting is enabled, the nob
// is then draggable from the new position until the mouse is released
// Input  : state - 
//-----------------------------------------------------------------------------
void C2DSlider::SetDragOnRepositionNob( bool bState )
{
	m_bIsDragOnRepositionNob = bState;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool C2DSlider::IsDragOnRepositionNob() const
{
	return m_bIsDragOnRepositionNob;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool C2DSlider::IsDragged() const
{
	return m_bDragging;
}


//-----------------------------------------------------------------------------
// Purpose: Set the size of the slider bar.
//			Warning less than 30 pixels tall and everything probably won't fit.
//-----------------------------------------------------------------------------
void C2DSlider::OnSizeChanged( int nWide, int nTall )
{
	BaseClass::OnSizeChanged( nWide, nTall );

	RecomputeNobPosFromValue();
}


//-----------------------------------------------------------------------------
// Purpose: Draw everything on screen
//-----------------------------------------------------------------------------
void C2DSlider::Paint()
{
	DrawNob();
}


//-----------------------------------------------------------------------------
// Purpose: Draw the slider track
//-----------------------------------------------------------------------------
void C2DSlider::PaintBackground()
{
	BaseClass::PaintBackground();
	
	int x, y;
	int wide,tall;

	GetTrackRect( x, y, wide, tall );

	surface()->DrawSetColor( s_TrackColor ); 
	surface()->DrawFilledRect( x, y, x + wide + 1, y + tall + 1 );
	if ( m_pInsetBorder )
	{
		m_pInsetBorder->Paint( x, y, x + wide, y + tall );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Layout the slider before drawing it on screen.
//-----------------------------------------------------------------------------
void C2DSlider::PerformLayout()
{
	BaseClass::PerformLayout();
	RecomputeNobPosFromValue();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C2DSlider::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	s_TextColor = pScheme->GetColor( "2DSlider.TextColor", s_TextColor );
	s_NobColor = pScheme->GetColor( "2DSlider.NobColor", s_NobColor );
	s_TickColor = pScheme->GetColor( "2DSlider.TickColor", s_TickColor );
	s_TickFillXColor = pScheme->GetColor( "2DSlider.TickFillXColor", s_TickFillXColor );
	s_TickFillYColor = pScheme->GetColor( "2DSlider.TickFillYColor", s_TickFillYColor );
	s_TickFillColor = pScheme->GetColor( "2DSlider.TickFillColor", s_TickFillColor );
	s_TrackColor = pScheme->GetColor( "2DSlider.TrackColor", s_TrackColor );

	m_pLabel->SetColor( s_TextColor );
	m_pLabel->SetFont( pScheme->GetFont( "Default" ) );
	m_pLabel->ResizeImageToContent();

	SetFgColor( s_NobColor );

	m_pNobBorder = pScheme->GetBorder( "ButtonBorder" );
	m_pInsetBorder = pScheme->GetBorder( "ButtonDepressedBorder" );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C2DSlider::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	SetNobWidth( inResourceData->GetInt( "nobWidth", GetNobWidth() ) );
	SetNobTall( inResourceData->GetInt( "nobTall", GetNobTall() ) );
	SetDrawLabel( inResourceData->GetBool( "drawLabel", IsDrawingLabel() ) );

	wchar_t buf[ BUFSIZ ];
	m_pLabel->GetText( buf, ARRAYSIZE( buf ) );
	SetLabelText( inResourceData->GetWString( "labelText", buf ) );

	SetDragOnRepositionNob( inResourceData->GetBool( "dragOnRepositionNob", IsDragOnRepositionNob() ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C2DSlider::GetSettings( KeyValues *outResourceData )
{
	BaseClass::GetSettings( outResourceData );

	outResourceData->SetInt( "nobWidth", GetNobWidth() );
	outResourceData->SetInt( "nobTall", GetNobTall() );
	outResourceData->SetBool( "drawLabel", IsDrawingLabel() );

	wchar_t buf[ BUFSIZ ];
	m_pLabel->GetText( buf, ARRAYSIZE( buf ) );
	outResourceData->SetWString( "labelText", buf );

	outResourceData->SetBool( "dragOnRepositionNob", IsDragOnRepositionNob() );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *C2DSlider::GetDescription()
{
	static char buf[ 1024 ];
	Q_snprintf( buf, sizeof( buf ), "%s, int nobWidth, int nobTall, bool dragOnRepositionNob", BaseClass::GetDescription() );
	return buf;
}


//-----------------------------------------------------------------------------
// Purpose: Handle key presses
//-----------------------------------------------------------------------------
void C2DSlider::OnKeyCodeTyped( KeyCode nKeyCode )
{
	switch ( nKeyCode )
	{
		// for now left and right arrows just open or close submenus if they are there.
        case KEY_LEFT:
			MoveNobRelative( -1,  0 );
			break;
    	case KEY_RIGHT:
			MoveNobRelative(  1,  0 );
			break;
        case KEY_DOWN:
			MoveNobRelative(  0,  1 );
			break;
        case KEY_UP:
			MoveNobRelative(  0, -1 );
			break;
        case KEY_HOME:
			SetValueX( m_fRange[ kXAxis ][ 0 ] );
   			break;
        case KEY_END:
			SetValueX( m_fRange[ kXAxis ][ 1 ] );
   			break;
        case KEY_PAGEUP:
			SetValueY( m_fRange[ kYAxis ][ 0 ] );
   			break;
        case KEY_PAGEDOWN:
			SetValueY( m_fRange[ kYAxis ][ 1 ] );
   			break;
    	default:
    		BaseClass::OnKeyCodeTyped( nKeyCode );
    		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw the nob part of the slider.
//-----------------------------------------------------------------------------
void C2DSlider::DrawNob()
{
	// horizontal nob
	int x, y;
	int wide,tall;
	GetTrackRect( x, y, wide, tall );

	surface()->DrawSetColor( s_TickFillXColor );
	surface()->DrawFilledRect( x, y, x + m_nNobPos[ kXAxis ], y + m_nNobPos[ kYAxis ] );

	surface()->DrawSetColor( s_TickFillYColor );
	surface()->DrawFilledRect( x + m_nNobPos[ kXAxis ] + 1, y + m_nNobPos[ kYAxis ] + 1, x + wide + 1, y + tall + 1 );

	surface()->DrawSetColor( s_TickFillColor );
	surface()->DrawFilledRect( x, y + m_nNobPos[ kYAxis ] + 1, x + m_nNobPos[ kXAxis ], y + tall + 1 );

	surface()->DrawSetColor( s_TickFillColor );
	surface()->DrawFilledRect( x, y + tall, m_nNobPos[ kXAxis ] + 1, m_nNobPos[ kYAxis ] );

	surface()->DrawSetColor( s_TickColor );
	surface()->DrawFilledRect( x, m_nNobPos[ kYAxis ], x + wide - 1, m_nNobPos[ kYAxis ] + 1 );
	surface()->DrawFilledRect( m_nNobPos[ kXAxis ], y, m_nNobPos[ kXAxis ] + 1, y + tall - 1 );

	// Redraw the inset border
	if ( m_pInsetBorder )
	{
		m_pInsetBorder->Paint( x, y, x + wide, y + tall );
	}

	surface()->DrawSetColor( s_NobColor );
	surface()->DrawFilledRect(
		m_nNobPos[ kXAxis ] - m_nNobHalfSize[ kXAxis ], m_nNobPos[ kYAxis ] - m_nNobHalfSize[ kYAxis ],
		m_nNobPos[ kXAxis ] + m_nNobHalfSize[ kXAxis ] + 1, m_nNobPos[ kYAxis ] + m_nNobHalfSize[ kYAxis ] + 1 );

	// border
	if ( m_pNobBorder )
	{
		m_pNobBorder->Paint(
			m_nNobPos[ kXAxis ] - m_nNobHalfSize[ kXAxis ], m_nNobPos[ kYAxis ] - m_nNobHalfSize[ kYAxis ],
			m_nNobPos[ kXAxis ] + m_nNobHalfSize[ kXAxis ] + 1, m_nNobPos[ kYAxis ] + m_nNobHalfSize[ kYAxis ] + 1 );
	}

	char buf[ BUFSIZ ];
	Q_snprintf( buf, ARRAYSIZE( buf ), "n %02d %02d v %3.1f %3.1f", m_nNobPos[ kXAxis ], m_nNobPos[ kYAxis ], m_fValue[ kXAxis ], m_fValue[ kYAxis ] );

	int nLabelWidth, nLabelTall;
	m_pLabel->GetContentSize( nLabelWidth, nLabelTall );
	//	m_pLabel->SetPos( 10, y + MAX( tall, tall - ( tall - nLabelTall ) / 2  ) );
	m_pLabel->SetPos( 10, y + ( MAX( 0, tall - nLabelTall ) ) / 2 );
	m_pLabel->Paint();
}


//-----------------------------------------------------------------------------
// Purpose: Get the rectangle to draw the slider track in.
//-----------------------------------------------------------------------------
void C2DSlider::GetTrackRect( int &x, int &y, int &w, int &t )
{
	x = 0;
	y = 0;

	GetPaintSize( w, t );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C2DSlider::MoveNobRelative( int nX, int nY )
{
	int x, y, wide, tall;
	GetTrackRect( x, y, wide, tall );

	m_nNobPos[ kXAxis ] = clamp( m_nNobPos[ kXAxis ] + nX, x, x + wide - 1 );
	m_nNobPos[ kYAxis ] = clamp( m_nNobPos[ kYAxis ] + nY, y, y + tall - 1 );

	RecomputeValueFromNobPos( true );
}


//-----------------------------------------------------------------------------
// Purpose: Move the nob on the slider in response to changing its value.
//-----------------------------------------------------------------------------
void C2DSlider::RecomputeNobPosFromValue()
{
	int x, y, wide, tall;
	GetTrackRect( x, y, wide, tall );

	m_nNobPos[ kXAxis ] = static_cast< int >( RemapValClamped( m_fValue[ kXAxis ], m_fRange[ kXAxis ][ 0 ], m_fRange[ kXAxis ][ 1 ], x, x + wide - 1 ) );
	m_nNobPos[ kYAxis ] = static_cast< int >( RemapValClamped( m_fValue[ kYAxis ], m_fRange[ kYAxis ][ 0 ], m_fRange[ kYAxis ][ 1 ], y, y + tall - 1 ) );
	
	Repaint();
}


//-----------------------------------------------------------------------------
// Purpose: Sync the slider's value up with the nob's position.
//-----------------------------------------------------------------------------
void C2DSlider::RecomputeValueFromNobPos( bool bTriggerChangeMessage /* = true */ )
{
	int x, y, wide, tall;
	GetTrackRect( x, y, wide, tall );

	const float fValueX = RemapValClamped( m_nNobPos[ kXAxis ], x, x + wide - 1, m_fRange[ kXAxis ][ 0 ], m_fRange[ kXAxis ][ 1 ] );
	const float fValueY = RemapValClamped( m_nNobPos[ kYAxis ], y, y + tall - 1, m_fRange[ kYAxis ][ 0 ], m_fRange[ kYAxis ][ 1 ] );

	SetValue( fValueX, fValueY, bTriggerChangeMessage );
}


//-----------------------------------------------------------------------------
// Purpose: Send a message to interested parties when the slider moves
//-----------------------------------------------------------------------------
void C2DSlider::SendSliderMovedMessage()
{	
	KeyValues *p2DSliderMoved = new KeyValues( "2DSliderMoved" );
	p2DSliderMoved->SetFloat( "valueX", m_fValue[ kXAxis ] );
	p2DSliderMoved->SetFloat( "valueY", m_fValue[ kYAxis ] );
	PostActionSignal( p2DSliderMoved );
}


//-----------------------------------------------------------------------------
// Purpose: Send a message to interested parties when the user begins dragging the slider
//-----------------------------------------------------------------------------
void C2DSlider::SendSliderDragStartMessage()
{	
	KeyValues *p2DSliderDragStart = new KeyValues( "2DSliderDragStart" );
	p2DSliderDragStart->SetFloat( "valueX", m_fValue[ kXAxis ] );
	p2DSliderDragStart->SetFloat( "valueY", m_fValue[ kYAxis ] );
	PostActionSignal( p2DSliderDragStart );
}


//-----------------------------------------------------------------------------
// Purpose: Send a message to interested parties when the user ends dragging the slider
//-----------------------------------------------------------------------------
void C2DSlider::SendSliderDragEndMessage()
{	
	KeyValues *p2DSliderDragEnd = new KeyValues( "2DSliderDragEnd" );
	p2DSliderDragEnd->SetFloat( "valueX", m_fValue[ kXAxis ] );
	p2DSliderDragEnd->SetFloat( "valueY", m_fValue[ kYAxis ] );
	PostActionSignal( p2DSliderDragEnd );
}