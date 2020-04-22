//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: [jason] Creates the Start Screen in Scaleform.
//
// $NoKeywords: $
//=============================================================================//
#if defined( INCLUDE_SCALEFORM )

#ifndef CREATELEGALANIM_SCALEFORM_H
#define CREATELEGALANIM_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "scaleformui/scaleformui.h"

class CCreateLegalAnimScaleform : public ScaleformFlashInterface
{
protected:
	static CCreateLegalAnimScaleform* m_pInstance;

	explicit CCreateLegalAnimScaleform( const char* commandNameOrNULL );

public:
	static void CreateIntroMovie( void );
	static void CreateCreditsMovie( void );
	static void UnloadDialog( void );
	static bool IsActive( void ) { return m_pInstance != NULL && !m_pInstance->m_bAnimationCompleted; }
    static void DismissAnimation( void );

  	void AnimationCompleted( SCALEFORM_CALLBACK_ARGS_DECL );
  	void PlayAudio( SCALEFORM_CALLBACK_ARGS_DECL );

	void GetRatingsBoardForLegals( SCALEFORM_CALLBACK_ARGS_DECL );
protected:
	virtual void PostUnloadFlash( void );
    virtual void FlashLoadError( SCALEFORM_CALLBACK_ARGS_DECL );
    void InnerDismissAnimation( void );
    void FinishAnimation( void );

    void StopAudio( void );

	void Show( void );
	void Hide( void );

    bool m_bAnimationCompleted;
    int m_iCurrentSoundPlaying;

    char m_MenuCommandToRunOnCompletion[256];
};

#endif // CREATESTARTSCREEN_SCALEFORM_H
#endif // include scaleform
