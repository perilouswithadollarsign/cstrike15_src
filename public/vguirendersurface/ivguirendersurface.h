//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: An extra interface implemented by the material system 
// implementation of vgui::ISurface
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef IVGUIRENDERSURFACE_H
#define IVGUIRENDERSURFACE_H
#ifdef _WIN32
#pragma once
#endif


#include <vgui/VGUI.h>
#include "vgui/ISurface.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class VMatrix;
class IMaterial2;
struct InputEvent_t;
FORWARD_DECLARE_HANDLE( InputContextHandle_t );


//-----------------------------------------------------------------------------
// Callbacks for mouse getting + setting
//-----------------------------------------------------------------------------
typedef void (*GetMouseCallback_t)(int &x, int &y);
typedef void (*SetMouseCallback_t)(int x, int y);

//-----------------------------------------------------------------------------
// Callbacks for sound playing
//-----------------------------------------------------------------------------
typedef void (*PlaySoundFunc_t)(const char *pFileName);


//-----------------------------------------------------------------------------
//
// An extra interface implemented by the material system implementation of vgui::ISurface
//
//-----------------------------------------------------------------------------
class IVGuiRenderSurface : public vgui::ISurface
{
public:
	// If the app drives the input (like the engine needs to do for VCR mode), 
	// it can call this, setting bLetAppDriveInput to true and call 
	// HandleInputEvent for the input events.
	virtual void SetAppDrivesInput( bool bLetAppDriveInput ) = 0;

	// Tells the surface to ignore windows messages
	virtual void EnableWindowsMessages( bool bEnable ) = 0;

	// Starts, ends 3D painting
	// NOTE: These methods should only be called from within the paint()
	// method of a panel.
	virtual void Begin3DPaint( int iLeft, int iTop, int iRight, int iBottom ) = 0;
	virtual void End3DPaint() = 0;

	// NOTE: This also should only be called from within the paint()
	// method of a panel. Use it to disable clipping for the rendering
	// of this panel.
	virtual void DisableClipping( bool bDisable ) = 0;

	// Prevents vgui from changing the cursor
	virtual bool IsCursorLocked() const = 0;

	// Sets the mouse get + set callbacks
	virtual void SetMouseCallbacks( GetMouseCallback_t getFunc, SetMouseCallback_t setFunc ) = 0;

	// Installs a function to play sounds
	virtual void InstallPlaySoundFunc( PlaySoundFunc_t soundFunc ) = 0;

	// Some drawing methods that cannot be accomplished under Win32
	virtual void DrawColoredCircle( int centerx, int centery, float radius, int r, int g, int b, int a ) = 0;
	virtual void DrawColoredText( vgui::HFont font, int x, int y, int r, int g, int b, int a, const char *fmt, ... ) = 0;

	// Draws text with current font at position and wordwrapped to the rect using color values specified
	virtual void DrawColoredTextRect( vgui::HFont font, int x, int y, int w, int h, int r, int g, int b, int a, const char *fmt, ... ) = 0;
	virtual void DrawTextHeight( vgui::HFont font, int w, int& h, char *fmt, ... ) = 0;

	// Returns the length of the text string in pixels
	virtual int	DrawTextLen( vgui::HFont font, const char *fmt, ... ) = 0;

	// Draws a panel in 3D space. Assumes view + projection are already set up
	// Also assumes the (x,y) coordinates of the panels are defined in 640xN coords
	// (N isn't necessary 480 because the panel may not be 4x3)
	// The width + height specified are the size of the panel in world coordinates
	virtual void DrawPanelIn3DSpace( vgui::VPANEL pRootPanel, const VMatrix &panelCenterToWorld, int nPixelWidth, int nPixelHeight, float flWorldWidth, float flWorldHeight ) = 0; 

	// Binds a material to a surface texture ID
	virtual void DrawSetTextureMaterial( int id, IMaterial2 *pMaterial ) = 0;

	// Handles an input event, returns true if the event should be filtered from the rest of the game
	virtual bool HandleInputEvent( const InputEvent_t &event ) = 0;

	virtual void Set3DPaintTempRenderTarget( const char *pRenderTargetName ) = 0;
	virtual void Reset3DPaintTempRenderTarget( void ) = 0;

	// Gets a material bound to a surface texture ID
	virtual IMaterial2 *DrawGetTextureMaterial( int id ) = 0;

	// Sets the VGui input context
	virtual void SetInputContext( InputContextHandle_t hContext ) = 0;
};


#endif // IVGUIRENDERSURFACE_H

