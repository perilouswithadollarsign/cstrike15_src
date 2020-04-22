//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "html_base_scaleform.h"

#include "html_control_scaleform.h"
#include "messagebox_scaleform.h"
#include "overwatchresolution_scaleform.h"
#include "materialsystem/materialsystem_config.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CHtmlBaseScaleform::CHtmlBaseScaleform()
:
	m_pChromeHTML( NULL ),
	m_bLastCanGoBack( true ),
	m_bLastCanGoForward( true ),
	m_bLastIsLoadingState( true )
{
}

CHtmlBaseScaleform::~CHtmlBaseScaleform()
{
	if ( m_pChromeHTML )
	{
		delete m_pChromeHTML;
		m_pChromeHTML = NULL;
	}
}

void CHtmlBaseScaleform::InitChromeHTML( const char* pszBaseURL, const char *pszPostData )
{
	if ( !m_pChromeHTML )
	{
		m_pChromeHTML = new CHtmlControlScaleform();
		m_pChromeHTML->Init( this );
		m_pChromeHTML->OpenURL( pszBaseURL, pszPostData );
	}
}

void CHtmlBaseScaleform::Update()
{
	if ( m_pChromeHTML )
	{
		m_pChromeHTML->Update();

		if ( FlashAPIIsValid() )
		{
			WITH_SLOT_LOCKED
			{
				if ( m_bLastIsLoadingState != m_pChromeHTML->IsLoadingPage() )
				{
					m_bLastIsLoadingState = m_pChromeHTML->IsLoadingPage();

					SFVALUEARRAY args = m_pScaleformUI->CreateValueArray( 1 );
					g_pScaleformUI->ValueArray_SetElement( args, 0, m_bLastIsLoadingState );
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setPageLoadState", args, 1 );
					g_pScaleformUI->ReleaseValueArray( args, 1 );
				}

				if ( m_bLastCanGoBack != m_pChromeHTML->BCanGoBack() )
				{
					m_bLastCanGoBack = m_pChromeHTML->BCanGoBack();

					SFVALUEARRAY args = m_pScaleformUI->CreateValueArray( 1 );
					g_pScaleformUI->ValueArray_SetElement( args, 0, m_bLastCanGoBack );
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setBackButtonEnabled", args, 1 );
					g_pScaleformUI->ReleaseValueArray( args, 1 );
				}

				if ( m_bLastCanGoForward != m_pChromeHTML->BCanGoFoward() )
				{
					m_bLastCanGoForward = m_pChromeHTML->BCanGoFoward();

					SFVALUEARRAY args = m_pScaleformUI->CreateValueArray( 1 );
					g_pScaleformUI->ValueArray_SetElement( args, 0, m_bLastCanGoForward );
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setForwardButtonEnabled", args, 1 );
					g_pScaleformUI->ReleaseValueArray( args, 1 );
				}
			}
		}
	}
}

void CHtmlBaseScaleform::PostUnloadFlash()
{
	// remove chrome browser
	if ( m_pChromeHTML )
	{
		delete m_pChromeHTML;
		m_pChromeHTML = NULL;
	}
}

void CHtmlBaseScaleform::InitChromeHTMLRenderTarget( const char* pszTextureName )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			const int nParams = 1;
			SFVALUEARRAY args = m_pScaleformUI->CreateValueArray( nParams );
			m_pScaleformUI->ValueArray_SetElement( args, 0, pszTextureName );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "InitChromeHTMLRenderTarget", args, nParams );
			m_pScaleformUI->ReleaseValueArray( args, nParams );
		}
	}
}

void CHtmlBaseScaleform::UpdateHTMLScrollbar( int iScroll, int iTall, int iMax, bool bVisible, bool bVert )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			const int nParams = 5;
			SFVALUEARRAY args = m_pScaleformUI->CreateValueArray( nParams );
			m_pScaleformUI->ValueArray_SetElement( args, 0, iScroll );
			m_pScaleformUI->ValueArray_SetElement( args, 1, iTall );
			m_pScaleformUI->ValueArray_SetElement( args, 2, iMax );
			m_pScaleformUI->ValueArray_SetElement( args, 3, bVisible );
			m_pScaleformUI->ValueArray_SetElement( args, 4, bVert );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "UpdateHTMLScrollbar", args, nParams );
			m_pScaleformUI->ReleaseValueArray( args, nParams );
		}
	}
}

bool CHtmlBaseScaleform::BShouldPreventInputRouting()
{
	// Disable mouse input if a message box is active
	if ( CMessageBoxScaleform::GetLastMessageBoxCreated() )
		return true;

	// Disable mouse input if Overwatch verdict is up
	if ( SFHudOverwatchResolutionPanel::GetInstance() )
		return true;

	// Otherwise mouse input is OK
	return false;
}

void CHtmlBaseScaleform::OnHTMLMouseDown( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// Disable mouse input if a message box is active
	if ( BShouldPreventInputRouting() )
		return;
	
	if ( pui->Params_GetNumArgs( obj ) != 3 )
		return;

	int x = pui->Params_GetArgAsNumber( obj, 0 );
	int y = pui->Params_GetArgAsNumber( obj, 1 );
	int buttonIndex = pui->Params_GetArgAsNumber( obj, 2 );

	if ( m_pChromeHTML )
	{
		if ( buttonIndex == 0 )
		{
			m_pChromeHTML->OnMouseDown( MOUSE_LEFT, x, y );
		}
		else if ( buttonIndex == 3 )	// MOUSE_4
		{
			m_pChromeHTML->GoBack();
		}
		else if ( buttonIndex == 4 )	// MOUSE_5
		{
			m_pChromeHTML->GoForward();
		}
	}
}

void CHtmlBaseScaleform::OnHTMLMouseUp( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// Disable mouse input if a message box is active
	if ( BShouldPreventInputRouting() )
		return;

	if ( pui->Params_GetNumArgs( obj ) != 2 )
		return;

	int x = pui->Params_GetArgAsNumber( obj, 0 );
	int y = pui->Params_GetArgAsNumber( obj, 1 );

	if ( m_pChromeHTML )
	{
		m_pChromeHTML->OnMouseUp( MOUSE_LEFT, x, y );
	}
}

void CHtmlBaseScaleform::OnHTMLMouseMove( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// Disable mouse input if a message box is active
	if ( BShouldPreventInputRouting() )
		return;

	if ( pui->Params_GetNumArgs( obj ) != 2 )
		return;

	int x = pui->Params_GetArgAsNumber( obj, 0 );
	int y = pui->Params_GetArgAsNumber( obj, 1 );

	if ( m_pChromeHTML )
	{
		m_pChromeHTML->OnMouseMoved( x, y );
	}
}

void CHtmlBaseScaleform::OnHTMLMouseWheel( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// Disable mouse input if a message box is active
	if ( BShouldPreventInputRouting() )
		return;

	if ( pui->Params_GetNumArgs( obj ) != 1 )
		return;

	int delta = pui->Params_GetArgAsNumber( obj, 0 );

	if ( m_pChromeHTML )
	{
		m_pChromeHTML->OnMouseWheeled( delta );
	}
}

void CHtmlBaseScaleform::OnHTMLKeyDown( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// Disable mouse input if a message box is active
	if ( BShouldPreventInputRouting() )
		return;

	if ( pui->Params_GetNumArgs( obj ) != 1 )
		return;

	ButtonCode_t iKey = ( ButtonCode_t )( int )pui->Params_GetArgAsNumber( obj, 0 );

	if ( m_pChromeHTML && IsKeyCode( iKey ) )
	{
		m_pChromeHTML->OnKeyDown( iKey );
	}
}

void CHtmlBaseScaleform::OnHTMLKeyUp( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// Disable mouse input if a message box is active
	if ( BShouldPreventInputRouting() )
		return;

	if ( pui->Params_GetNumArgs( obj ) != 1 )
		return;

	ButtonCode_t iKey = ( ButtonCode_t )( int )pui->Params_GetArgAsNumber( obj, 0 );

	if ( m_pChromeHTML && IsKeyCode( iKey ) )
	{
		m_pChromeHTML->OnKeyUp( iKey );
	}
}

void CHtmlBaseScaleform::OnHTMLKeyTyped( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// Disable mouse input if a message box is active
	if ( BShouldPreventInputRouting() )
		return;

	if ( pui->Params_GetNumArgs( obj ) != 1 )
		return;

	wchar_t typed = pui->Params_GetArgAsNumber( obj, 0 );

	if ( m_pChromeHTML )
	{
		m_pChromeHTML->OnKeyTyped( typed );
	}
}

void CHtmlBaseScaleform::SetHTMLBrowserSize( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( pui->Params_GetNumArgs( obj ) != 4 )
		return;

	int iWidth = pui->Params_GetArgAsNumber( obj, 0 );
	int iHeight = pui->Params_GetArgAsNumber( obj, 1 );
	int iWidthStage = pui->Params_GetArgAsNumber( obj, 2 );
	int iHeightStage = pui->Params_GetArgAsNumber( obj, 3 );
	if ( m_pChromeHTML )
	{
		m_pChromeHTML->SetBrowserBaseSize( iWidth, iHeight, iWidthStage, iHeightStage );
	}
}

void CHtmlBaseScaleform::OnHTMLScrollBarChanged( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( pui->Params_GetNumArgs( obj ) != 2 )
		return;
	
	if ( m_pChromeHTML )
	{
		int iPosition = pui->Params_GetArgAsNumber( obj, 0 );
		bool bVert = pui->Params_GetArgAsBool( obj, 1 );
		m_pChromeHTML->OnHTMLScrollBarMoved( iPosition, bVert );
	}
}

void CHtmlBaseScaleform::OnHTMLBackButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( m_pChromeHTML )
	{
		m_pChromeHTML->GoBack();
	}
}

void CHtmlBaseScaleform::OnHTMLForwardButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( m_pChromeHTML )
	{
		m_pChromeHTML->GoForward();
	}
}

void CHtmlBaseScaleform::OnHTMLRefreshButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( m_pChromeHTML )
	{
		m_pChromeHTML->Refresh();
	}
}

void CHtmlBaseScaleform::OnHTMLStopButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( m_pChromeHTML )
	{
		m_pChromeHTML->StopLoading();
	}
}

void CHtmlBaseScaleform::OnHTMLExternalBrowserButtonClicked( SCALEFORM_CALLBACK_ARGS_DECL )
{
	/* Removed for partner depot */
}

#endif // INCLUDE_SCALEFORM