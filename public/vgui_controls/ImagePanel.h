//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IMAGEPANEL_H
#define IMAGEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/vgui.h>
#include <vgui_controls/Panel.h>

namespace vgui
{

class IImage;

//-----------------------------------------------------------------------------
// Purpose: Panel that holds a single image
//-----------------------------------------------------------------------------
class ImagePanel : public Panel
{
	DECLARE_CLASS_SIMPLE( ImagePanel, Panel );
public:
	ImagePanel(Panel *parent, const char *name);
	~ImagePanel();

	virtual void SetImage(IImage *image);
	virtual void SetImage(const char *imageName);
	virtual IImage *GetImage();
	char *GetImageName();

	// sets whether or not the image should scale to fit the size of the ImagePanel (defaults to false)
	void SetShouldScaleImage( bool state );
	bool GetShouldScaleImage();
	void SetScaleAmount( float scale );
	float GetScaleAmount( void );

	// set the color to fill with, if no image is specified
	void SetFillColor( Color col );
	Color GetFillColor();

	virtual Color GetDrawColor( void );
	virtual void SetDrawColor( Color color );

	virtual void ApplySettings(KeyValues *inResourceData);

	// unhooks and evicts image if possible, caller must re-establish
	bool EvictImage();
	
	int GetNumFrames();
	void SetFrame( int nFrame );
	int GetFrame() const;

	void SetRotation( int iRotation ) { m_iRotation = iRotation; }

protected:
	virtual void PaintBackground();
	virtual void GetSettings(KeyValues *outResourceData);
	virtual const char *GetDescription();
	virtual void OnSizeChanged(int newWide, int newTall);
	virtual void ApplySchemeSettings( IScheme *pScheme );

private:
	IImage *m_pImage;
	char *m_pszImageName;
	char *m_pszFillColorName;
	char *m_pszDrawColorName;
	bool m_bScaleImage;
	bool m_bTileImage;
	bool m_bTileHorizontally;
	bool m_bTileVertically;
	float m_fScaleAmount;
	Color m_FillColor;
	Color m_DrawColor;
	int m_iRotation;

	bool m_bFrameImage;
	int m_iFrame;

	// To do correct tiling, we need floating point proportional tile sizes
	int	 m_iTileHorizontalOverride;
	int	 m_iTileVerticalOverride;
	float m_flTileHorizontalOverride;
	float m_flTileVerticalOverride;
};

} // namespace vgui

#endif // IMAGEPANEL_H
