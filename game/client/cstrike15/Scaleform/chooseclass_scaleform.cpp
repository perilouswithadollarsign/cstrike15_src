//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#if defined( INCLUDE_SCALEFORM )
#include <game/client/iviewport.h>
#include "chooseclass_scaleform.h"
#include "c_cs_playerresource.h"
#include "c_cs_player.h"
#include "c_team.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "cs_gamerules.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"
#include "gameui_util.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnOk ),
	SFUI_DECL_METHOD( OnCancel ),
	SFUI_DECL_METHOD( OnLeft ),
	SFUI_DECL_METHOD( OnRight ),
	SFUI_DECL_METHOD( OnAutoSelect ),
	SFUI_DECL_METHOD( OnShowScoreboard ),
SFUI_END_GAME_API_DEF( CChooseClassScaleform, ChooseClass );

//static ConVar player_classplayedlast_CT( "player_classplayedlast_CT", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS );
//static ConVar player_classplayedlast_T( "player_classplayedlast_T", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS );

CChooseClassScaleform::CChooseClassScaleform( CounterStrikeViewport* pViewPort ) :
	m_bVisible ( false ),
	m_bLoading ( false ),
	m_pViewPort ( pViewPort ),
	m_pDescriptionText ( NULL ),
	m_pModelText ( NULL ),
	m_pTeamText ( NULL ),
	m_nNumClasses ( 0 ),
	m_nClassSelection ( 0 ),
	m_nTeamSelection ( 0 ),
	m_bCanceled ( false )
{
	m_iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	ListenForGameEvent( "class_selected" );
	ListenForGameEvent( "cs_game_disconnected" );
}

CChooseClassScaleform::~CChooseClassScaleform()
{
}

void CChooseClassScaleform::FlashLoaded( void )
{
	// determine team
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer )
	{
		if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
		{
			m_nTeamSelection = TEAM_TERRORIST;
			m_nNumClasses = PlayerModelInfo::GetPtr()->GetNumTModels();
		}	
		else if ( pPlayer->GetTeamNumber() == TEAM_CT )
		{
			m_nTeamSelection = TEAM_CT;
			m_nNumClasses = PlayerModelInfo::GetPtr()->GetNumCTModels();
		}
		else
		{
			// no team selected
			Assert( 0 );
		}
	}

	SFVALUE panelValue = m_pScaleformUI->Value_GetMember( m_FlashAPI, "ChooseClass" );

	if ( panelValue )
	{
		SFVALUE navCharPanel = m_pScaleformUI->Value_GetMember( panelValue, "NavPanel" );

		if ( navCharPanel )
		{
			m_pDescriptionText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navCharPanel, "Desc_Text" );
			m_pModelText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navCharPanel, "Model_Text" );
			m_pTeamText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navCharPanel, "Team_Text" );

			// change the team label depending on previous player team selection
			if ( m_pTeamText )
			{
				if ( m_nTeamSelection == TEAM_TERRORIST )
				{
					m_pTeamText->SetText( "#SFUI_T_Label" );
				}	
				else if ( m_nTeamSelection == TEAM_CT )
				{
					m_pTeamText->SetText( "#SFUI_CT_Label" );
				}
			}

			m_pScaleformUI->ReleaseValue( navCharPanel );
		}

		m_pScaleformUI->ReleaseValue( panelValue );
	}
}

void CChooseClassScaleform::FlashReady( void )
{
	if ( m_FlashAPI && m_pScaleformUI )
	{	
		m_bLoading = false;

		if ( m_bVisible )
		{
			Show();
		}
		else
		{
			Hide();
		}
	}
}

void CChooseClassScaleform::Show( void )
{
	if ( !m_bLoading )
	{
		if ( FlashAPIIsValid() )
		{
			engine->ClientCmd( "overview_mode 0" );

			WITH_SLOT_LOCKED
			{
				//int nLastPlayed = 0;
				//if ( m_nTeamSelection == TEAM_CT )
				//{
				//	nLastPlayed = player_classplayedlast_CT.GetInt();
				//}
				//else if ( m_nTeamSelection == TEAM_TERRORIST )
				//{
				//	nLastPlayed = player_classplayedlast_T.GetInt();
				//}

				m_nClassSelection = 0;
				SetSelectedClass( m_nClassSelection );
				UpdateClassText();

				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", 0, NULL );
			}

			m_pViewPort->ShowBackGround( true );
		}
		else
		{
			m_bLoading = true;
			SFUI_REQUEST_ELEMENT( SF_SS_SLOT( m_iSplitScreenSlot ), g_pScaleformUI, CChooseClassScaleform, this, ChooseClass );
		}
	}

	m_bVisible = true;
}

void CChooseClassScaleform::Hide( bool bRemove )
{
	if ( !m_bLoading && FlashAPIIsValid() && m_bVisible )
	{
		if ( bRemove )
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanelAndRemove", 0, NULL );
			}
		}
		else
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", 0, NULL );
			}
		}

		m_pViewPort->ShowBackGround( false );
	}

	m_bVisible = false;
}

bool CChooseClassScaleform::PreUnloadFlash( void )
{
	StopListeningForAllEvents();

	SafeReleaseSFTextObject( m_pDescriptionText );
	SafeReleaseSFTextObject( m_pModelText );
	SafeReleaseSFTextObject( m_pTeamText );

	return ScaleformFlashInterface::PreUnloadFlash();
}

void CChooseClassScaleform::PostUnloadFlash( void )
{
	m_bLoading = false;

	if ( m_bCanceled )
	{
		m_bCanceled = false;

		IViewPortPanel *pTeamScreen = m_pViewPort->FindPanelByName( "team" );

		if ( pTeamScreen )
		{
			// Go back to spectating and reset the team info
			engine->ClientCmd( "resetteam" );
			m_pViewPort->ShowPanel( pTeamScreen, true );
		}
	}
	else 
	{
		m_pViewPort->SetChoseTeamAndClass( true );
	}
}

void CChooseClassScaleform::ShowPanel( bool bShow )
{
	if ( bShow != m_bVisible )
	{
		if ( bShow )
		{
			Show();
			engine->CheckPoint( "ClassMenu" );
		}
		else
		{
			Hide();
		}
	}
}

void CChooseClassScaleform::OnAutoSelect( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SaveAndJoin();
}

void CChooseClassScaleform::OnOk( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SaveAndJoin();
}


void CChooseClassScaleform::SaveAndJoin( void )
{
	// [jason] For updating the last played class in the main menu; written out for each player
	static CGameUIConVarRef s_player_classplayedlast( "player_classplayedlast" );

	//player_classplayedlast_CT.SetValue( m_nTeamSelection );
	//s_player_classplayedlast.SetValue( m_nTeamSelection );

	char szCommand[ 64 ];
	V_strcpy( szCommand, "joinclass" );
	engine->ClientCmd( szCommand );
	Hide( true );
}


void CChooseClassScaleform::OnCancel( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// reset selection
	m_nClassSelection = 0;
	UpdateClassText();

	m_bCanceled = true;
	Hide( true );
}

void CChooseClassScaleform::UpdateClassText()
{
	char szClassModel[255];
	szClassModel[0] = '\0';
	char szClassDesc[1024];
	szClassDesc[0] = '\0';

	V_snprintf( szClassModel, sizeof(szClassModel), "#SFUI_Obsolete_Name" );
	V_snprintf( szClassDesc, sizeof(szClassDesc), "#SFUI_Obsolete_Label" );

	WITH_SLOT_LOCKED
	{
		if ( m_pModelText )
		{
			m_pModelText->SetText( szClassModel );
		}

		if ( m_pDescriptionText )
		{
			m_pDescriptionText->SetText( szClassDesc );
		}
	}
}

void CChooseClassScaleform::OnLeft( SCALEFORM_CALLBACK_ARGS_DECL )
{
	--m_nClassSelection;

	if ( m_nClassSelection < 0 )
	{
		m_nClassSelection = m_nNumClasses - 1;
	}

	SFVALUEARRAY data = m_pScaleformUI->CreateValueArray( 1 );

	m_pScaleformUI->ValueArray_SetElement( data, 0, m_nClassSelection );

	WITH_SLOT_LOCKED
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onLeft", data, 1 );
	}

	m_pScaleformUI->ReleaseValueArray( data, 1 );

	UpdateClassText();
}

void CChooseClassScaleform::OnRight( SCALEFORM_CALLBACK_ARGS_DECL )
{
	++m_nClassSelection;

	if ( m_nClassSelection > m_nNumClasses - 1 )
	{
		m_nClassSelection = 0;
	}

	SFVALUEARRAY data = m_pScaleformUI->CreateValueArray( 1 );

	m_pScaleformUI->ValueArray_SetElement( data, 0, m_nClassSelection );

	WITH_SLOT_LOCKED
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onRight", data, 1 );
	}

	m_pScaleformUI->ReleaseValueArray( data, 1 );

	UpdateClassText();
}

const char * CChooseClassScaleform::GetName( void )
{
	 return PANEL_CHOOSE_CLASS;
}

void CChooseClassScaleform::SetSelectedClass( int nClassID )
{
	SFVALUEARRAY data = m_pScaleformUI->CreateValueArray( 2 );

	m_pScaleformUI->ValueArray_SetElement( data, 0, nClassID );
	m_pScaleformUI->ValueArray_SetElement( data, 1, m_nTeamSelection );

	WITH_SLOT_LOCKED
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onSetSelectedClass", data, 2 );
	}

	m_pScaleformUI->ReleaseValueArray( data, 2 );
}

void CChooseClassScaleform::OnShowScoreboard( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( GetViewPortInterface() )
	{
		GetViewPortInterface()->ShowPanel( PANEL_SCOREBOARD, true );
	}
}

void CChooseClassScaleform::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();

	if( !Q_strcmp( type, "class_selected" ) || !Q_strcmp( type, "cs_game_disconnected" ))
	{
		if ( m_bVisible ) 
		{
			Hide( true );
		}
		else
		{
			RemoveFlashElement();
		}
	} 
}

#endif // INCLUDE_SCALEFORM
