//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
//=============================================================================

#include "textdisplay.h"
#include "gameuisystemmgr.h"
#include "gameuisystem.h"
#include "gametext.h"
#include "tier1/utlbuffer.h"


#define VERTICAL_SPACING 20

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CTextDisplay::CTextDisplay()
{
	m_nXOffset = 10;
	m_nYOffset = 10;

	m_nXStaticOffset = 10;
	m_nYStaticOffset = 10;
	m_bIsInitialized = false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CTextDisplay::Init( IGameUISystem *pMenu )
{
	if ( m_bIsInitialized )
		return;

	Vector2D stageSize(0, 0);
	m_pMenu = ( CGameUISystem * ) pMenu;
	Assert( m_pMenu );
	m_pMenu->GetStageSize( stageSize );

	m_nXOffset = (-stageSize.x/2) + 10;
	m_nYOffset = (-stageSize.y/2) + 10;

	m_nXStaticOffset = (-stageSize.x/2) + 500;
	m_nYStaticOffset = (-stageSize.y/2) + 10;

	m_bIsInitialized = true;
}

//-----------------------------------------------------------------------------
// Text that doesn't change every frame.
//-----------------------------------------------------------------------------
void CTextDisplay::AddStaticText( const char* pFmt, ... )
{
	va_list args;
	CUtlBuffer message;

	va_start( args, pFmt );
	message.VaPrintf(	pFmt, args );
	va_end( args );

	char strMessage[255];
	message.GetString( strMessage, 255 );

	CGameText *pNewInfo = new CGameText( "staticText" );
	pNewInfo->SetFont( "Default" );
	pNewInfo->SetText( strMessage );
	pNewInfo->SetCenter( m_nXStaticOffset, m_nYStaticOffset );
	m_nYStaticOffset += VERTICAL_SPACING;

	m_pStaticText.AddToTail( pNewInfo );
	if ( m_pMenu )
	{
		m_pMenu->Definition().AddGraphicToLayer( pNewInfo, SUBLAYER_FONT );
	}	
}


//-----------------------------------------------------------------------------
// Text that doesn't change every frame.
//-----------------------------------------------------------------------------
void CTextDisplay::AddStaticText( int xPos, int yPos, const char* pFmt, ... )
{
	va_list args;
	CUtlBuffer message;

	va_start( args, pFmt );
	message.VaPrintf(	pFmt, args );
	va_end( args );

	char strMessage[255];
	message.GetString( strMessage, 255 );

	CGameText *pNewInfo = new CGameText( "staticText" );
	pNewInfo->SetFont( "Default" );
	pNewInfo->SetText( strMessage );
	pNewInfo->SetCenter( xPos, yPos );

	m_pStaticText.AddToTail( pNewInfo );
	if ( m_pMenu )
	{
		m_pMenu->Definition().AddGraphicToLayer( pNewInfo, SUBLAYER_FONT );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CTextDisplay::PrintMsg( const char* pFmt, ... )
{
	va_list args;
	CUtlBuffer message;

	va_start( args, pFmt );
	message.VaPrintf(	pFmt, args );
	va_end( args );

	char strMessage[255];
	message.GetString( strMessage, 255 );

	CGameText *pNewInfo = new CGameText( "msgText" );
	pNewInfo->SetFont( "Default" );
	pNewInfo->SetText( strMessage );
	pNewInfo->SetCenter( m_nXOffset, m_nYOffset );
	m_nYOffset += VERTICAL_SPACING;

	m_pStatsText.AddToTail( pNewInfo );
	if ( m_pMenu )
	{
		m_pMenu->Definition().AddGraphicToLayer( pNewInfo, SUBLAYER_FONT );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CTextDisplay::PrintMsg( int xPos, int yPos, const char* pFmt, ... )
{
	va_list args;
	CUtlBuffer message;

	va_start( args, pFmt );
	message.VaPrintf(	pFmt, args );
	va_end( args );

	char strMessage[255];
	message.GetString( strMessage, 255 );

	CGameText *pNewInfo = new CGameText( "msgText" );
	pNewInfo->SetFont( "Default" );
	pNewInfo->SetText( strMessage );
	pNewInfo->SetCenter( xPos, yPos );

	m_pStatsText.AddToTail( pNewInfo );
	if ( m_pMenu )
	{
		m_pMenu->Definition().AddGraphicToLayer( pNewInfo, SUBLAYER_FONT );
	}
}



//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CTextDisplay::ClearStaticText()
{
	if ( m_pMenu )
	{
		for ( int i = 0; i < m_pStaticText.Count(); ++i )
		{
			m_pMenu->Definition().RemoveGraphic( m_pStaticText[i] );
		}

		m_pStaticText.RemoveAll();

		Vector2D stageSize(0, 0);
		m_pMenu->GetStageSize( stageSize );
		m_nYStaticOffset = (-stageSize.y/2) + 10;

	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CTextDisplay::FinishFrame()
{
	if ( m_pMenu )
	{
		for ( int i = 0; i < m_pStatsText.Count(); ++i )
		{
			m_pMenu->Definition().RemoveGraphic( m_pStatsText[i] );
			delete m_pStatsText[i];
			m_pStatsText[i] = NULL;
		}

		m_pStatsText.RemoveAll();

		Vector2D stageSize(0, 0);
		m_pMenu->GetStageSize( stageSize );
		m_nYOffset = (-stageSize.y/2) + 10;
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CTextDisplay::Shutdown()
{
	FinishFrame();

	ClearStaticText();

	m_bIsInitialized = false;
}
