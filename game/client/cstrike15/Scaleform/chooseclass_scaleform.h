//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if defined( INCLUDE_SCALEFORM )

#if !defined( __CHOOSECLASS_SCALEFORM_H__ )
#define __CHOOSECLASS_SCALEFORM_H__

#include "../VGUI/counterstrikeviewport.h"
#include "messagebox_scaleform.h"
#include "GameEventListener.h"


class CChooseClassScaleform : public ScaleformFlashInterface, public IViewPortPanel, public CGameEventListener
{
public:
	explicit CChooseClassScaleform( CounterStrikeViewport* pViewPort );
	virtual ~CChooseClassScaleform( );

	/********************************************
	* CGameEventListener methods
	*/

	virtual void FireGameEvent( IGameEvent *event );

	/************************************
	 * callbacks from scaleform
	 */

	void OnOk( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnCancel( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnLeft( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnRight( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnAutoSelect( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnShowScoreboard( SCALEFORM_CALLBACK_ARGS_DECL );

	/************************************************************
	 *  Flash Interface methods
	 */

	virtual void FlashReady( void );
	virtual void FlashLoaded( void );

	void Show( void );

	// if bRemove, then remove all elements after hide animation completes
	void Hide( bool bRemove = false );

	bool PreUnloadFlash( void );
	void PostUnloadFlash( void );

	/*************************************************************
	 * IViewPortPanel interface
	 */

	virtual const char *GetName( void );
	virtual void SetData( KeyValues *data ) {}

	virtual void Reset( void ) {}  // hibernate
	virtual void Update( void ) {}	// updates all ( size, position, content, etc )
	virtual bool NeedsUpdate( void ) { return false; } // query panel if content needs to be updated
	virtual bool HasInputElements( void ) { return true; }
	virtual void ReloadScheme( void ) {}
	virtual bool CanReplace( const char *panelName ) const { return true; } // returns true if this panel can appear on top of the given panel
	virtual bool CanBeReopened( void ) const { return true; } // returns true if this panel can be re-opened after being hidden by another panel

	virtual void ShowPanel( bool state );

	// VGUI functions:
	virtual vgui::VPANEL GetVPanel( void ) { return 0; } // returns VGUI panel handle
	virtual bool IsVisible( void ) { return m_bVisible; }  // true if panel is visible
	virtual void SetParent( vgui::VPANEL parent ) {}

	virtual bool WantsBackgroundBlurred( void ) { return false; }

protected:
	CounterStrikeViewport * m_pViewPort;

	// text elements
	ISFTextObject *			m_pDescriptionText;
	ISFTextObject *			m_pModelText;
	ISFTextObject *			m_pTeamText;
	
	bool					m_bVisible;
	bool					m_bLoading;
	int						m_iSplitScreenSlot;
	bool					m_bCanceled;
	int						m_nNumClasses;
	int						m_nClassSelection;
	int						m_nTeamSelection;

	// Updates the model name and class description fields based on the currently
	// selected class
	void UpdateClassText( void );

	// Sets the currently selected class. The change occurs instantly in flash, no call back for animation.
	void SetSelectedClass( int classID );

	// Saves the selected class to the player profile and performs the joinclass command 
	void SaveAndJoin( void );
};

#endif // __CHOOSECLASS_SCALEFORM_H__

#endif // INCLUDE_SCALEFORM
