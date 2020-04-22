//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TGAPICKER_H
#define TGAPICKER_H
#ifdef _WIN32
#pragma once
#endif

#include "matsys_controls/baseassetpicker.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CTGAPreviewPanel;

namespace vgui
{
	class Splitter;
}


//-----------------------------------------------------------------------------
// Purpose: Base class for choosing raw assets
//-----------------------------------------------------------------------------
class CTGAPicker : public CBaseAssetPicker
{
	DECLARE_CLASS_SIMPLE( CTGAPicker, CBaseAssetPicker );

public:
	CTGAPicker( vgui::Panel *pParent );
	virtual ~CTGAPicker();

private:
	// Derived classes have this called when the previewed asset changes
	virtual void OnSelectedAssetPicked( const char *pAssetName );

	CTGAPreviewPanel *m_pTGAPreview;
	vgui::Splitter *m_pPreviewSplitter;
};


//-----------------------------------------------------------------------------
// Purpose: Modal dialog for asset picker
//-----------------------------------------------------------------------------
class CTGAPickerFrame : public CBaseAssetPickerFrame
{
	DECLARE_CLASS_SIMPLE( CTGAPickerFrame, CBaseAssetPickerFrame );

public:
	CTGAPickerFrame( vgui::Panel *pParent, const char *pTitle );
};


#endif // TGAPICKER_H
