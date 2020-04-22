//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if defined( INCLUDE_SCALEFORM )

#if !defined( __UPSELL_SCALEFORM_H__ )
#define __UPSELL_SCALEFORM_H__
#ifdef _WIN32
#pragma once
#endif


class CUpsellScaleform : public ScaleformFlashInterface
{
private:
	static CUpsellScaleform *m_pInstance;

protected:
	CUpsellScaleform( );
	virtual ~CUpsellScaleform( );

public:
	/************************************
	 * callbacks from scaleform
	 */
	void OnQuitPressed( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnBackPressed( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnUnlockPressed( SCALEFORM_CALLBACK_ARGS_DECL );
	// Called to trigger commands on the BasePanel, like opening other dialogs, configuring options, etc.
	//  See the CBaseModPanel::RunMenuCommand function for specific available commands.
	void OnBasePanelRunCommand( SCALEFORM_CALLBACK_ARGS_DECL );

	// Construction and Destruction
	static void LoadDialog( void );
	static void UnloadDialog( void );

	static void ShowMenu( bool bShow );
	static bool IsActive() { return m_pInstance != NULL; }
	static bool IsVisible() { return ( m_pInstance != NULL && m_pInstance->m_bVisible ); }

protected:
	/************************************************************
	 *  Flash Interface methods
	 */

	virtual void FlashReady( void );
	virtual void FlashLoaded( void );

	virtual bool PreUnloadFlash( void );
	virtual void PostUnloadFlash( void );

	void Show( void );

	// Hides the panel and passes szPostHideCommand to OnBasePanelRunCommand after hide is complete
	void Hide( const char * szPostHideCommand = "None" );

	// Populates the dialog with achievement icons and sets the progress bar ** Not thread safe. Do not invoke outside of flash callbacks **
	void PopulateAchievements( void );

protected:
	
	bool					m_bVisible;				// Visibility flag
	bool					m_bLoading;				// Loading flag
	ISFTextObject *			m_pTextMedalsCount;		// Numerical tally of earned medals
};

#endif // __UPSELL_SCALEFORM_H__

#endif // INCLUDE_SCALEFORM
