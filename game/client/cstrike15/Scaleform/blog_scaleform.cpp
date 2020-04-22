//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "blog_scaleform.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CBlogScaleform* CBlogScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
	IMPLEMENT_HTML_SFUI_METHODS,
SFUI_END_GAME_API_DEF( CBlogScaleform, MainMenuBlogPanel );

void CBlogScaleform::LoadDialog( void )
{
	if ( !m_pInstance )
	{
		m_pInstance = new CBlogScaleform();
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CBlogScaleform, m_pInstance, MainMenuBlogPanel );
	}
}

void CBlogScaleform::UnloadDialog( void )
{
	if ( m_pInstance )
	{
		// TODO
	}
}

void CBlogScaleform::FlashReady( void )
{
	SFDevMsg("CBlogScaleform::FlashReady\n");

	//InitChromeHTML( "img://chrome_1", "ChromeHTML_UpdateBlog", "http://www.google.com", "" );
	InitChromeHTML( "http://store.steampowered.com/news/?appids=730", "" );
}

#endif // INCLUDE_SCALEFORM