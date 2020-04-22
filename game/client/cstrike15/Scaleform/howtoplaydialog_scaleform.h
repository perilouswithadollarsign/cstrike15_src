#if defined( INCLUDE_SCALEFORM )
//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HOWTOPLAY_SCALEFORM_H
#define HOWTOPLAY_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "scaleformui/scaleformui.h"

class CHowToPlayDialogScaleform : public ScaleformFlashInterface
{
protected:
	static CHowToPlayDialogScaleform *m_pInstance;

	CHowToPlayDialogScaleform();

public:
	static void LoadDialog();
	static void UnloadDialog();

	void Show();
	void Hide();

protected:
	virtual void FlashReady();
	virtual void PostUnloadFlash();
	virtual void FlashLoaded();
};

#endif // HOWTOPLAY_SCALEFORM_H
#endif // INCLUDE_SCALEFORM
