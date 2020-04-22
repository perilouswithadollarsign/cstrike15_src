//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef TGAIMAGEPANEL_H
#define TGAIMAGEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Panel.h"
#include "tier1/utlstring.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Displays a tga image
//-----------------------------------------------------------------------------
class CTGAImagePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CTGAImagePanel, vgui::Panel );

public:
	CTGAImagePanel( vgui::Panel *parent, const char *name );

	~CTGAImagePanel();

	void SetTGAFilename( const char *filename );
	char const *GetTGAFilename() const;
	void SetTGAFilenameNonMod( const char *filename );

	void SetShouldScaleImage( bool state );

	void SetImageColor( Color imageColor );

	int GetTextureID( void ) const { return m_iTextureID; }

	virtual void GetSettings(KeyValues *outResourceData);
	virtual void ApplySettings(KeyValues *inResourceData);

	virtual void Paint( void );

private:
	int m_iTextureID;
	int m_iImageWidth;
	int m_iImageHeight;
	Color m_ImageColor;
	CUtlString m_sTGAFilenameWithPath;
	CUtlString m_sTGAFilename;	
	
	bool m_bHasValidTexture : 1;
	bool m_bLoadedTexture : 1;
	bool m_bScaleImage : 1;

};

#endif //TGAIMAGEPANEL_H
