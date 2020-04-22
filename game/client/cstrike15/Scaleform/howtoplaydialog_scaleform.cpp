//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )
#include "basepanel.h"
#include "howtoplaydialog_scaleform.h"

#include "matchmaking/imatchframework.h"
#include "gameui_interface.h"

using namespace vgui;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CHowToPlayDialogScaleform* CHowToPlayDialogScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( CHowToPlayDialogScaleform, HowToPlay );

void CHowToPlayDialogScaleform::LoadDialog()
{
	if ( !m_pInstance )
	{
		m_pInstance = new CHowToPlayDialogScaleform();
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CHowToPlayDialogScaleform, m_pInstance, HowToPlay );
	}
}

void CHowToPlayDialogScaleform::UnloadDialog()
{
	if ( m_pInstance )
	{
		m_pInstance->RemoveFlashElement();
	}
}

CHowToPlayDialogScaleform::CHowToPlayDialogScaleform()
{
}

void CHowToPlayDialogScaleform::FlashLoaded()
{
	m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "InitDialogData", NULL, 0 );
}

void CHowToPlayDialogScaleform::FlashReady()
{
	Show();
}

void CHowToPlayDialogScaleform::PostUnloadFlash()
{
	if ( GameUI().IsInLevel() )
	{
		BasePanel()->RestorePauseMenu();
	}
	else
	{
		BasePanel()->RestoreMainMenuScreen();
	}

	m_pInstance = NULL;
	delete this;
}

void CHowToPlayDialogScaleform::Show()
{
	WITH_SLOT_LOCKED
	{
		ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
	}
}

void CHowToPlayDialogScaleform::Hide()
{
	WITH_SLOT_LOCKED
	{
		ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
	}
}

#endif // INCLUDE_SCALEFORM
