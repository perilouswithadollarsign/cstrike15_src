//======= Copyright © 1996-2008, Valve Corporation, All rights reserved. ======
//
// Purpose: A 2D Slider
//
//=============================================================================

#ifndef C2DSLIDER_H
#define C2DSLIDER_H

#include <vgui_controls/Panel.h>

class vgui::TextImage;

//-----------------------------------------------------------------------------
// Purpose: A 2D Slider
//-----------------------------------------------------------------------------
class C2DSlider : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( C2DSlider, vgui::Panel );

public:

	C2DSlider( Panel *pParent, const char *pName );

	virtual ~C2DSlider();

	// interface
	virtual void SetValueX( float fValueX, bool bTriggerChangeMessage = true );
	virtual float GetValueX() const;

	virtual void SetValueY( float fValueY, bool bTriggerChangeMessage = true );
	virtual float GetValueY() const;

	virtual void SetValue( float fValueX, float fValueY, bool bTriggerChangeMessage = true );
	virtual void GetValue( float &fValueX, float &fValueY ) const;

    virtual void SetRange( float fMinX, float fMaxX, float fMinY, float fMaxY, bool bTriggerChangeMessage = true );
	virtual void GetRange( float &fMinX, float &fMaxX, float &fMinY, float &fMaxY ) const;

	virtual void SetLabelText( const char *pText );
	virtual void SetLabelText( const wchar_t *pText );
	virtual void GetLabelText( wchar_t *pBuffer, int nBufferLen ) const;
	virtual void GetLabelUnlocalizedText( char *pBuffer, int nBufferLen ) const;

	virtual void SetDrawLabel( bool bState );
	virtual bool IsDrawingLabel() const;

	virtual void GetNobPos( int &nX, int &nY );

	virtual void OnCursorMoved( int x, int y );
	virtual void OnMousePressed( vgui::MouseCode mouseCode );
	virtual void OnMouseDoublePressed( vgui::MouseCode mouseCode );
	virtual void OnMouseReleased( vgui::MouseCode mouseCode );

	virtual void SetNobWidth( int nWidth );
	virtual int GetNobWidth() const;

	virtual void SetNobTall( int nTall );
	virtual int GetNobTall() const;

	virtual void SetNobSize( int nWidth, int nTall );
	virtual void GetNobSize( int &nWidth, int &nTall ) const;

	// If you click on the slider outside of the nob, the nob jumps
	// to the click position, and if this setting is enabled, the nob
	// is then draggable from the new position until the mouse is released
	virtual void SetDragOnRepositionNob( bool bState );
	virtual bool IsDragOnRepositionNob() const;

	// Get if the slider nob is being dragged by user, usually the application
	// should refuse from forcefully setting slider value if it is being dragged
	// by user since the next frame the nob will pop back to mouse position
	virtual bool IsDragged() const;

protected:
	virtual void OnSizeChanged( int nWide, int nTall );
	virtual void Paint();
	virtual void PaintBackground();
	virtual void PerformLayout();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void ApplySettings( KeyValues *pInResourceData );
	virtual void GetSettings( KeyValues *pOutResourceData );
	virtual const char *GetDescription();
	virtual void OnKeyCodeTyped( vgui::KeyCode nKeyCode );

	virtual void DrawNob();

	virtual void GetTrackRect( int &x, int &y, int &w, int &t );

	virtual void MoveNobRelative( int nX, int nY );
	virtual void RecomputeNobPosFromValue();
	virtual void RecomputeValueFromNobPos( bool bTriggerChangeMessage = true );
	
	virtual void SendSliderMovedMessage();
	virtual void SendSliderDragStartMessage();
	virtual void SendSliderDragEndMessage();

	enum Axis_t
	{
		kXAxis = 0,
		kYAxis = 1,
		kAxisCount = 2
	};

	int m_nNobPos[ kAxisCount ];			// Position of the center of the nob in client space pixels
	int m_nNobDragStartPos[ kAxisCount ];
	int m_nDragStartPos[ kAxisCount ];
	float m_fRange[ kAxisCount ][ 2 ];
	float m_fValue[ kAxisCount ];			// the position of the Slider, in coordinates as specified by SetRange
	vgui::IBorder *m_pNobBorder;
	vgui::IBorder *m_pInsetBorder;
	int m_nNobHalfSize[ kAxisCount ];		// The number of pixels on each side of the nob center, can be 0 for a 1x1 pixel nob.  nob size = m_nNobHalfSize * 2 + 1

	static Color s_TextColor;
	static Color s_NobColor;
	static Color s_TickColor;
	static Color s_TickFillXColor;
	static Color s_TickFillYColor;
	static Color s_TickFillColor;
	static Color s_TrackColor;

	bool m_bDrawLabel : 1;
	bool m_bIsDragOnRepositionNob : 1;

	bool m_bDragging : 1;

	vgui::TextImage *m_pLabel;
};

#endif // C2DSLIDER_H