//===== Copyright  Valve Corporation, All rights reserved. ======//
//
//  Portal leaderboard graph panel
//
//================================================================//
#include "cbase.h"

#include "vportalchallengestatspanel.h"
#include <vgui_controls/Label.h>
#include <vgui/isurface.h>


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

CPortalChallengeStatsPanel::CPortalChallengeStatsPanel( Panel *parent, const char *name ) : EditablePanel( parent, name )
{
	SetScheme("basemodui_scheme");

	SetProportional( true );
	SetPaintBackgroundEnabled( true );

	m_pTitleLabel = NULL;
	m_pPortalsLabel = NULL;
	m_pTimeLabel = NULL;
	m_pPortalScore = NULL;
	m_pTimeScore = NULL;
}


CPortalChallengeStatsPanel::~CPortalChallengeStatsPanel()
{

}


void CPortalChallengeStatsPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/UI/portal_challenge_stats_panel.res" );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );

	m_pTitleLabel = static_cast< Label *>( FindChildByName( "TitleLabel" ) );
	m_pPortalsLabel = static_cast< Label *>( FindChildByName( "PortalsLabel" ) );
	m_pTimeLabel = static_cast< Label *>( FindChildByName( "TimeLabel" ) );
	m_pPortalScore = static_cast< Label *>( FindChildByName( "PortalScoreLabel" ) );
	m_pTimeScore = static_cast< Label *>( FindChildByName( "TimeScoreLabel" ) );
}


void CPortalChallengeStatsPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	
}


void CPortalChallengeStatsPanel::PaintBackground()
{
	BaseClass::PaintBackground();
}


void CPortalChallengeStatsPanel::SetTitleText( const char *pTitle )
{
	if ( m_pTitleLabel )
	{
		m_pTitleLabel->SetText( pTitle );
	}
}


void CPortalChallengeStatsPanel::SetPortalScore( int nPortals )
{
	char szLabel[32];

	if ( nPortals < 0 )
	{
		m_pPortalScore->SetText( "-" );
	}
	else
	{
		V_snprintf( szLabel, sizeof(szLabel), "%d", nPortals );
		if ( m_pPortalScore )
		{
			m_pPortalScore->SetText( szLabel );
		}
	}

	
}


void CPortalChallengeStatsPanel::SetTimeScore( float flTotalSeconds )
{
	if ( flTotalSeconds < 0 )
	{
		m_pTimeScore->SetText( "-" );
		return;
	}

	int nSeconds = flTotalSeconds;
	int nMiliseconds = (flTotalSeconds - nSeconds) * 100;
	int nMinutes = nSeconds / 60;
	nSeconds = nSeconds % 60;

	// update the label
	char szTime[32];
	V_snprintf( szTime, sizeof(szTime), "%02d:%02d.%02d", nMinutes, nSeconds, nMiliseconds );

	m_pTimeScore->SetText( szTime );
}


void CPortalChallengeStatsPanel::SetTimeScore( int nTotalSeconds )
{
	if ( nTotalSeconds < 0 )
	{
		m_pTimeScore->SetText( "-" );
		return;
	}

	char szTime[32];
	int32 nMilliseconds = nTotalSeconds % 100;
	int32 nSeconds = nTotalSeconds / 100;
	int32 nMinutes = nSeconds / 60;
	nSeconds %= 60;
	Q_snprintf( szTime, sizeof( szTime ), "%02d:%02d.%02d", nMinutes, nSeconds, nMilliseconds );

	m_pTimeScore->SetText( szTime );
}
