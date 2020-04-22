//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __TRANSITIONPANEL_H__
#define __TRANSITIONPANEL_H__

#include "basemodui.h"
#include "vgui/ISurface.h"

namespace BaseModUI
{

struct RectParms_t
{
	RectParms_t()
	{
		m_flLerp = 0;
	}

	Vector		m_Position0;
	Vector2D	m_TexCoord0;

	Vector		m_Position1;
	Vector2D	m_TexCoord1;

	Vector		m_Position2;
	Vector2D	m_TexCoord2;

	Vector		m_Position3;
	Vector2D	m_TexCoord3;

	Vector		m_Center;

	float		m_flLerp;
};

struct TileQuad_t
{
	TileQuad_t()
	{
		m_flStartTime = 0;
		m_flEndTime = 0;
		m_bDirty = false;
		m_nCurrentWindow = WT_NONE;
		m_nPreviousWindow = WT_NONE;
		m_nFrameCount = 0;
	}

	RectParms_t	m_RectParms;

	float	m_flStartTime;
	float	m_flEndTime;
	bool	m_bDirty;

	WINDOW_TYPE		m_nCurrentWindow;
	WINDOW_TYPE		m_nPreviousWindow;
	int				m_nFrameCount;
};

class CBaseModTransitionPanel : public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CBaseModTransitionPanel, vgui::EditablePanel );

	CBaseModTransitionPanel( const char *pPanelName );
	~CBaseModTransitionPanel();

	bool IsEffectEnabled();
	bool IsEffectActive();

	void MarkTile( int x, int y, WINDOW_TYPE wt, bool bForce = false );
	void MarkTilesInRect( int x, int y, int wide, int tall, WINDOW_TYPE wt, bool bForce = false );

	void TerminateEffect();

	// instantly stop any transitions, restore performs a reset
	// used for very harsh context switching (i.e. exiting the main menu) where screen stability is not possible
	void PreventTransitions( bool bPrevent );

	// temporarily inhibit transitions that can be safely resumed
	// used for a known context that need to fix the startup auto dialogs that clock through messages and then go away
	// these need to either maintain their layout size or suspend as the transition logic by design cannot do
	// a transition that happens during a transition
	void SuspendTransitions( bool bSuspend );

	void SetInitialState();

	void SetExpectedDirection( bool bForward, WINDOW_TYPE wt );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint();
	virtual void PostChildPaint();
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	int		GetTileIndex( int x, int y );
	void	TouchTile( int nTile, WINDOW_TYPE wt, bool bForce );
	void	SaveCurrentScreen( ITexture *pRenderTarget );
	void	BuildTiles();
	void	DrawEffect();
	void	ScanTilesForTransition();
	void	StartPaint3D();
	void	EndPaint3D();
	void	DrawBackground3D();
	void	DrawTiles3D();
	void	ShortenEffectDuration();

	ITexture *m_pFromScreenRT;
	ITexture *m_pCurrentScreenRT;

	int		m_nFrameCount;
	int		m_nNumTransitions;

	IMaterial	*m_pFromScreenMaterial;
	IMaterial	*m_pCurrentScreenMaterial;

	int		m_nPinFromBottom;
	int		m_nPinFromLeft;
	int		m_nTileWidth; 
	int		m_nTileHeight;

	int		m_nNumRows;
	int		m_nNumColumns;
	int		m_nXOffset;
	int		m_nYOffset;

	bool	m_bTransitionActive;
	bool	m_bForwardHint;
	bool	m_bAllowTransitions;
	bool	m_bQuietTransitions;

	WINDOW_TYPE	m_WindowTypeHint;
	WINDOW_TYPE	m_PreviousWindowTypeHint;

	CUtlVector< TileQuad_t > m_Tiles;
	
	CUtlVector< int >	m_Sounds;

	float	m_flDirection;
};

};

#endif