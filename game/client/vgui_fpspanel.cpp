//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "cbase.h"
#include "ifpspanel.h"
#include <vgui_controls/Panel.h>
#include "view.h"
#include <vgui/IVGui.h>
#include "VGuiMatSurface/IMatSystemSurface.h"
#include <vgui_controls/Controls.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IPanel.h>
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "filesystem.h"
#include "steam/steam_api.h"
#include "../common/xbox/xboxstubs.h"
#include "engineinterface.h"
#include "tier0/perfstats.h"
#include "tier0/cpumonitoring.h"
#ifdef PORTAL
#include "c_prop_portal.h"
#include "iextpropportallocator.h"
#include "matchmaking/imatchframework.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar cl_showfps( "cl_showfps", "0", FCVAR_RELEASE, "Draw fps meter (1 = fps, 2 = smooth, 3 = server, 4 = Show+LogToFile, 5 = Thread and wait times +10 = detailed )" );
static ConVar cl_showpos( "cl_showpos", "0", FCVAR_RELEASE, "Draw current position at top of screen" );
static ConVar cl_showbattery( "cl_showbattery", "0", 0, "Draw current battery level at top of screen when on battery power" );
static ConVar cl_showfps5_disp_time( "cl_showfps5_disp_time", "1.0", 0, "Time interval (s) at which thread and wait times are sampled and display is updated" );
static ConVar cl_showfps5_btlneck_disp_time( "cl_showfps5_btlneck_disp_time", "5.0", 0, "Time interval (s) for which main/render/gpu bottleneck times are displayed" );

extern unsigned int g_nNumBonesSetupBlendingRulesOnly;
extern unsigned int g_nNumBonesSetupAll;
ConVar cl_countbones( "cl_countbones", "0", FCVAR_CHEAT, "" );

#ifdef _GAMECONSOLE
static ConVar cl_showlowmemory( "cl_showlowmemory", "0", FCVAR_CHEAT, "Set to N to display a warning message if we have less than N MB of free memory (0 disables)." );
#endif

struct PerfStatRecord
{
	float m_lastUpdateTime;
	int	  m_fps;
	float m_mainThreadTime;
	float m_mainThreadWaitTime;
	float m_renderThreadTime;
	float m_renderThreadWaitTime;
};

template < int kNumSamples >
class FpsSpikesTracker_t
{
public:
	ApplicationPerformanceCountersInfo_t m_Samples[kNumSamples];
	ApplicationPerformanceCountersInfo_t m_min, m_max, m_avg, m_cur;
	int m_nSampleIdx;

	void AddSample( const ApplicationPerformanceCountersInfo_t& x )
	{
		m_Samples[ m_nSampleIdx ] = m_cur = x;
		m_nSampleIdx = ( m_nSampleIdx + 1 ) % kNumSamples;
		RecomputeData();
	}

	void RecomputeField( float ApplicationPerformanceCountersInfo_t::*pfl, ApplicationPerformanceCountersInfo_t const &x )
	{
		m_min.*pfl = MIN( m_min.*pfl, x.*pfl );
		m_max.*pfl = MAX( m_max.*pfl, x.*pfl );
		m_avg.*pfl += x.*pfl / kNumSamples;
	}

	void RecomputeData()
	{
		V_memset( &m_avg, 0, sizeof( m_avg ) );
		m_min = m_max = m_Samples[0];
		for ( int k = 0; k < kNumSamples; ++ k )
		{
			RecomputeField( &ApplicationPerformanceCountersInfo_t::msMain, m_Samples[k] );
			RecomputeField( &ApplicationPerformanceCountersInfo_t::msMST, m_Samples[k] );
			RecomputeField( &ApplicationPerformanceCountersInfo_t::msGPU, m_Samples[k] );
			RecomputeField( &ApplicationPerformanceCountersInfo_t::msFlip, m_Samples[k] );
			RecomputeField( &ApplicationPerformanceCountersInfo_t::msTotal, m_Samples[k] );
		}
	}
};

extern bool g_bDisplayParticlePerformance;
int GetParticlePerformance();

#define PERF_HISTOGRAM_BUCKET_SIZE 60

//-----------------------------------------------------------------------------
// Purpose: Framerate indicator panel
//-----------------------------------------------------------------------------
class CFPSPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CFPSPanel, vgui::Panel );

public:
	explicit		CFPSPanel( vgui::VPANEL parent );
	virtual			~CFPSPanel( void );

	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void	Paint();
	virtual void	OnTick( void );
	virtual void	DumpStats();

	virtual bool	ShouldDraw( void );


protected:
	MESSAGE_FUNC_INT_INT( OnScreenSizeChanged, "OnScreenSizeChanged", oldwide, oldtall );

private:
	void ComputeSize( void );
	void InitAverages()
	{
		m_AverageFPS = -1;
		m_lastRealTime = -1;
		m_high = -1;
		m_low = -1;
		memset( m_pServerTimes, 0, sizeof(m_pServerTimes) );
		memset( m_perfStats, 0, sizeof( m_perfStats ) );
	}

	enum { SERVER_TIME_HISTORY = 32 };

	vgui::HFont		m_hFont;
	float			m_AverageFPS;
	float			m_lastRealTime;
	float			m_pServerTimes[SERVER_TIME_HISTORY];
	int				m_nServerTimeIndex;
	int				m_high;
	int				m_low;
	bool			m_bLastDraw;

	int				m_nLinesNeeded;

	TimedEvent		m_tLogTimer;
	FileHandle_t	m_fhLog;
	char			m_szLevelname[32];
	int				m_nNumFramesTotal;
	int				m_nNumFramesBucket[PERF_HISTOGRAM_BUCKET_SIZE];

	int				m_BatteryPercent;
	float			m_lastBatteryPercent;

	PerfStatRecord	m_perfStats[4];
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CFPSPanel::CFPSPanel( vgui::VPANEL parent ) : BaseClass( NULL, "CFPSPanel" )
{
	memset( m_pServerTimes, 0, sizeof(m_pServerTimes) );
	m_nServerTimeIndex = 0;
	SetParent( parent );
	SetVisible( false );
	SetCursor( 0 );

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( false );
					    
	m_hFont = 0;
	m_nLinesNeeded = 5;		  
	m_BatteryPercent = -1;
	m_lastBatteryPercent = -1.0f;

	ComputeSize();

	vgui::ivgui()->AddTickSignal( GetVPanel(), 250 );
	m_bLastDraw = false;

	m_tLogTimer.Init( 6 );
	m_fhLog = FILESYSTEM_INVALID_HANDLE;
	m_nNumFramesTotal = 0;
	memset( m_nNumFramesBucket, 0, sizeof(m_nNumFramesBucket) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFPSPanel::~CFPSPanel( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: Updates panel to handle the new screen size
//-----------------------------------------------------------------------------
void CFPSPanel::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	BaseClass::OnScreenSizeChanged(iOldWide, iOldTall);
	ComputeSize();
}

//-----------------------------------------------------------------------------
// Purpose: Computes panel's desired size and position
//-----------------------------------------------------------------------------
void CFPSPanel::ComputeSize( void )
{
	int wide, tall;
	vgui::ipanel()->GetSize(GetVParent(), wide, tall );

	int x = 0;;
	int y = 0;
	if ( IsGameConsole() )
	{
		x += XBOX_MINBORDERSAFE * wide;
		y += XBOX_MINBORDERSAFE * tall;
	}
	SetPos( x, y );
	SetSize(  wide, ( m_nLinesNeeded + 2 ) * vgui::surface()->GetFontTall( m_hFont ) + 4 );
}

void CFPSPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

#if defined( _GAMECONSOLE )
	m_hFont = pScheme->GetFont( "CloseCaption_Normal" );
#else
	m_hFont = pScheme->GetFont( "MenuLarge" );
#endif
	Assert( m_hFont );

	ComputeSize();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFPSPanel::OnTick( void )
{
	bool bVisible = ShouldDraw();
	if ( IsVisible() != bVisible )
	{
		SetVisible( bVisible );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CFPSPanel::ShouldDraw( void )
{
	if ( g_bDisplayParticlePerformance )
		return true;
	if ( ( !cl_showfps.GetInt( ) || ( gpGlobals->absoluteframetime <= 0 ) ) && !cl_showpos.GetInt( ) 
#ifdef CSTRIKE15		 
		 && !cl_countbones.GetBool()
#endif
#ifdef _GAMECONSOLE
		&& !cl_showlowmemory.GetInt()
#endif
	)
	{
		m_bLastDraw = false;
		return false;
	}

	if ( !m_bLastDraw )
	{
		m_bLastDraw = true;
		InitAverages();
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GetFPSColor( int nFps, unsigned char ucColor[3] )
{
	ucColor[0] = 255; ucColor[1] = 0; ucColor[2] = 0;

	int nFPSThreshold1 = 20;
	int nFPSThreshold2 = 15;

	if ( IsPC() )
	{
		nFPSThreshold1 = 60;
		nFPSThreshold2 = 30;
	}
	else
	{
		if ( engine->IsSplitScreenActive() )
		{
			nFPSThreshold1 = 19; //20; // 19 shows up commonly when testing on the 360
			nFPSThreshold2 = 15;
		}
		else
		{
			nFPSThreshold1 = 59; //30; 29 shows up commonly when testing on the 360
			nFPSThreshold2 = 29;
		}
	}

	if ( nFps >= nFPSThreshold1 )
	{
		ucColor[0] = 10; 
		ucColor[1] = 200;
	}
	else if ( nFps >= nFPSThreshold2 )
	{
		ucColor[0] = 220;
		ucColor[1] = 220;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the color appropriately based on the CPU's frequency percentage
//-----------------------------------------------------------------------------
void GetCPUColor( float cpuPercentage, unsigned char ucColor[3] )
{
	// These colors are for poor CPU performance
	ucColor[0] = 255; ucColor[1] = 0; ucColor[2] = 0;

	if ( cpuPercentage >= kCPUMonitoringWarning1 )
	{
		// Excellent CPU performance
		ucColor[0] = 10; 
		ucColor[1] = 200;
	}
	else if ( cpuPercentage >= kCPUMonitoringWarning2 )
	{
		// Medium CPU performance
		ucColor[0] = 220;
		ucColor[1] = 220;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CFPSPanel::Paint() 
{
	int i = 0;
	int x = 2;

	int lineHeight = vgui::surface()->GetFontTall( m_hFont ) + 1;

	if ( g_bDisplayParticlePerformance )
	{
		int nPerf = GetParticlePerformance();
		if ( nPerf )
		{
			unsigned char ucColor[3]={ 0,255,0 };
			g_pMatSystemSurface->DrawColoredText(
				m_hFont, x, 42,
				ucColor[0], ucColor[1], ucColor[2],
				255, "Particle Performance Metric : %d", (nPerf+50)/100 );
		}
	}
	float realFrameTime = gpGlobals->realtime - m_lastRealTime;

	int nFPSMode = cl_showfps.GetInt()%10;

	ApplicationPerformanceCountersInfo_t apci = { 0, 0, 0, 0, 0 };
	ApplicationInstantCountersInfo_t aici = { 0, 0 };
	uint32 uiAPCI = 0;
	if ( cl_showfps.GetInt() >= 10 )
	{
		uiAPCI = g_pMaterialSystem->GetFrameTimestamps( apci, aici );
	}


	apci.msFlip = realFrameTime * 1000.0f;


	if ( nFPSMode == 3 && (!uiAPCI))
	{
		float flServerTime = engine->GetServerSimulationFrameTime();
		if ( flServerTime != 0.0f )
		{
			m_nServerTimeIndex = ( m_nServerTimeIndex + 1 ) & ( SERVER_TIME_HISTORY - 1 );
			m_pServerTimes[ m_nServerTimeIndex ] = flServerTime;
		}

		flServerTime = m_pServerTimes[ m_nServerTimeIndex ];

		float flTotalTime = 0.0f;
		float flPeakTime = 0.0f;
		for ( int i = 0; i < SERVER_TIME_HISTORY; ++i )
		{
			flTotalTime += m_pServerTimes[i];
			if ( flPeakTime < m_pServerTimes[i] )
			{
				flPeakTime = m_pServerTimes[i];
			}
		}
		flTotalTime /= SERVER_TIME_HISTORY;

		unsigned char ucColor[3];
		int nFps = static_cast<int>( 1.0f / ( flServerTime * 0.001f ) );
		GetFPSColor( nFps, ucColor );
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2, ucColor[0], ucColor[1], ucColor[2], 255, 
			"server %5.1f ms curr, %5.1f ave, %5.1f peak", flServerTime, flTotalTime, flPeakTime );
	}
	else if ( nFPSMode == 4 && m_lastRealTime > 0.0f && realFrameTime > 0.0f && engine->IsInGame() )
	{
		char levelName[32];
		Q_strncpy( levelName, engine->GetLevelNameShort(), sizeof( levelName ) );
		if ( Q_strcmp( m_szLevelname, levelName ) && m_fhLog != FILESYSTEM_INVALID_HANDLE )
		{
			DumpStats();
		}


		float flServerTime = engine->GetServerSimulationFrameTime();
		if ( flServerTime != 0.0f )
		{
			m_nServerTimeIndex = ( m_nServerTimeIndex + 1 ) & ( SERVER_TIME_HISTORY - 1 );
			m_pServerTimes[ m_nServerTimeIndex ] = flServerTime;
		}

		flServerTime = m_pServerTimes[ m_nServerTimeIndex ];

		const float NewWeight  = 0.1f;
		float NewFrame = 1.0f / realFrameTime;

		if ( m_AverageFPS <= 0.0f )
		{
			m_AverageFPS = NewFrame;
		} 
		else
		{				
			m_AverageFPS *= ( 1.0f - NewWeight ) ;
			m_AverageFPS += ( ( NewFrame ) * NewWeight );
		}

		int nAvgFps = static_cast<int>( m_AverageFPS );
		float flAverageMS = 1000.0f / m_AverageFPS;
		float flFrameMS = realFrameTime * 1000.0f;
		int	nFrameFps = static_cast<int>( 1.0f / realFrameTime );
		float flCurTime = gpGlobals->frametime;

		m_nNumFramesTotal++;
		int nBucket = MIN ( PERF_HISTOGRAM_BUCKET_SIZE, MAX ( 0, nFrameFps ) );
		m_nNumFramesBucket[nBucket]++;

		unsigned char ucColor[3];
		GetFPSColor( nAvgFps, ucColor );
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2, ucColor[0], ucColor[1], ucColor[2], 255, 
			"Avg FPS %3i, Frame MS %5.1f, Frame Server MS %5.1f", nAvgFps, flFrameMS, flServerTime );

		if ( m_fhLog == FILESYSTEM_INVALID_HANDLE )
		{
			Q_strncpy( m_szLevelname, levelName, sizeof( m_szLevelname ) );
			// Maximum 360 file name length
			char fileString[42];
			Q_snprintf( fileString, 42, "prof_%s.csv", m_szLevelname );
			m_fhLog = g_pFullFileSystem->Open( fileString, "w", "GAME" );
			g_pFullFileSystem->FPrintf( m_fhLog, "Time,Player 1 Position,Player 2 Position,Smooth FPS,Frame FPS,Smooth MS,Frame MS,Server Frame MS\n");
		}

		if ( ( m_tLogTimer.NextEvent( flCurTime ) && nFrameFps < 28.0f ) || nFrameFps < 15.0f )
		{
			// We always print data for two players so make sure the array always has room for at least two players.
			Vector vecOrigin[MAX_SPLITSCREEN_PLAYERS > 2 ? MAX_SPLITSCREEN_PLAYERS : 2] = {};
			QAngle angles[MAX_SPLITSCREEN_PLAYERS > 2 ? MAX_SPLITSCREEN_PLAYERS : 2] = {};
			FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
			{
				vecOrigin[hh] = MainViewOrigin(hh);
				angles[hh]= MainViewAngles(hh);

				C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer(hh);

				if ( pPlayer )
				{
					vecOrigin[hh] = pPlayer->GetAbsOrigin();
					angles[hh] = pPlayer->GetAbsAngles();
				}
			}

			char outputString[256];
			Q_snprintf( outputString, 256, "%5.1f,setpos %0.2f %0.2f %0.2f ; setang %0.2f %0.2f %0.2f,setpos %0.2f %0.2f %0.2f ; setang %0.2f %0.2f %0.2f,%3i,%3i,%4.1f,%4.1f,%5.1f\n", 
				gpGlobals->curtime,vecOrigin[0].x, vecOrigin[0].y, vecOrigin[0].z, angles[0].x, angles[0].y, angles[0].z, 
				vecOrigin[1].x, vecOrigin[1].y, vecOrigin[1].z, angles[1].x, angles[1].y, angles[1].z, nAvgFps, 
				nFrameFps, flAverageMS, flFrameMS, flServerTime );

			if ( m_fhLog != FILESYSTEM_INVALID_HANDLE )
			{
				g_pFullFileSystem->FPrintf( m_fhLog, "%s", outputString );
			}
		}

	}
	else if ( uiAPCI )
	{
		// Do not render old cl_showfps 2 if there's detailed perf information
		-- i;
	}
	else if ( nFPSMode && realFrameTime > 0.0 )
	{
		if ( nFPSMode == 5 )
		{
			char *perfStatTitle[4] = 
			{
				"Current",
				"Main    ",
				"Render ",
				"GPU    "
			};

			static Color perfStatClr[4];
			Color red( 255, 0, 0, 255 );
			Color blue( 0, 0, 255, 255 );
			Color black(0, 0, 0, 255 );

			float dt = gpGlobals->realtime - m_perfStats[0].m_lastUpdateTime;
			float eps = 0.015;	// ms

			if ( dt >= cl_showfps5_disp_time.GetFloat() )
			{
				m_perfStats[0].m_lastUpdateTime = gpGlobals->realtime;

				m_perfStats[0].m_mainThreadTime			=	g_PerfStats.m_Slots[PERF_STATS_SLOT_MAINTHREAD].m_PrevFrameTime.GetMillisecondsF();
				m_perfStats[0].m_mainThreadWaitTime		=	g_PerfStats.m_Slots[PERF_STATS_SLOT_END_FRAME].m_PrevFrameTime.GetMillisecondsF();
				m_perfStats[0].m_renderThreadTime		=	g_PerfStats.m_Slots[PERF_STATS_SLOT_RENDERTHREAD].m_PrevFrameTime.GetMillisecondsF();
				m_perfStats[0].m_renderThreadWaitTime	= 	g_PerfStats.m_Slots[PERF_STATS_SLOT_FORCE_HARDWARE_SYNC].m_PrevFrameTime.GetMillisecondsF();

				if ( m_perfStats[0].m_mainThreadWaitTime < eps )
				{
					 // We are main thread bound
					if ( m_perfStats[1].m_mainThreadTime < m_perfStats[0].m_mainThreadTime)
					{
						m_perfStats[1] = m_perfStats[0];
					}
				}
				else if ( m_perfStats[0].m_renderThreadWaitTime < eps )
				{
					// We are render thread bound
					if ( m_perfStats[2].m_renderThreadTime < m_perfStats[0].m_renderThreadTime )
					{
						m_perfStats[2] = m_perfStats[0];
					}
				}
				else
				{
					// We are gpu bound
					if ( m_perfStats[3].m_renderThreadWaitTime < m_perfStats[0].m_renderThreadWaitTime )
					{
						m_perfStats[3] = m_perfStats[0];
					}
				}

				// Determine perf stat clrs
				perfStatClr[0] = black;
				if ( m_perfStats[1].m_lastUpdateTime > m_perfStats[2].m_lastUpdateTime )
				{
					if ( m_perfStats[1].m_lastUpdateTime > m_perfStats[3].m_lastUpdateTime )
					{
						perfStatClr[1] = red;
						perfStatClr[2] = blue;
						perfStatClr[3] = blue;
					}
					else
					{
						perfStatClr[3] = red;
						perfStatClr[1] = blue;
						perfStatClr[2] = blue;
					}
				}
				else
				{
					perfStatClr[1] = blue;

					if ( m_perfStats[3].m_lastUpdateTime > m_perfStats[2].m_lastUpdateTime )
					{
						perfStatClr[3] = red;
						perfStatClr[2] = blue;
					}
					else
					{
						perfStatClr[2] = red;
						perfStatClr[3] = blue;
					}
				}
			}

			// Reset bottlenecks if time is up
			for ( int i = 1; i < 4; i ++)
			{
				dt = gpGlobals->realtime - m_perfStats[i].m_lastUpdateTime;
				if ( dt >= cl_showfps5_btlneck_disp_time.GetFloat() )
				{
					memset( &m_perfStats[i], 0, sizeof(m_perfStats[i]) );
				}
			}

			int iy = 2;

			g_pMatSystemSurface->DisableClipping( true );
		
			for ( int i = 0; i < 4; i++)
			{
				int fps = 0;
				if ( m_perfStats[i].m_mainThreadTime > 0.0f )
				{
					fps = (int)(1000.0f / m_perfStats[i].m_mainThreadTime);
				}

				char buff[256];
				Q_snprintf( buff, 256, "%s: Main: %6.2f, MainWt: %6.2f, Rdr: %6.2f, RdrWt: %6.2f (%3d fps) ", 
					perfStatTitle[i],
					m_perfStats[i].m_mainThreadTime, m_perfStats[i].m_mainThreadWaitTime,
					m_perfStats[i].m_renderThreadTime, m_perfStats[i].m_renderThreadWaitTime, fps );

				int len = g_pMatSystemSurface->DrawTextLen( m_hFont, buff );
				int ht = 15;

				g_pMatSystemSurface->DrawSetColor(0xff, 0xff, 0xff, 192 );
				g_pMatSystemSurface->DrawFilledRect( x, iy, x + len, iy + ht );
				g_pMatSystemSurface->DrawColoredText( m_hFont, x, iy, perfStatClr[i].r(), perfStatClr[i].g(), perfStatClr[i].b(), perfStatClr[i].a(), buff );

				iy += ht;
			}

			g_pMatSystemSurface->DisableClipping( false );
		}
		else if ( m_lastRealTime != -1.0f )
		{
			int nFps = -1;
			unsigned char ucColor[3];
			if ( nFPSMode == 2 )
			{
				const float NewWeight  = 0.1f;
				float NewFrame = 1.0f / realFrameTime;

				// If we're just below an integer boundary, we're good enough to call ourselves good WRT to coloration
				if ( (int)(NewFrame + 0.05) > (int )( NewFrame ) )
				{
					NewFrame = ceil( NewFrame );
				}

				if ( m_AverageFPS < 0.0f )
				{
					m_AverageFPS = NewFrame;
					m_high = (int)m_AverageFPS;
					m_low = (int)m_AverageFPS;
				} 
				else
				{				
					m_AverageFPS *= ( 1.0f - NewWeight ) ;
					m_AverageFPS += ( ( NewFrame ) * NewWeight );
				}

				int NewFrameInt = (int)NewFrame;
				if( NewFrameInt < m_low ) m_low = NewFrameInt;
				if( NewFrameInt > m_high ) m_high = NewFrameInt;	

				nFps = static_cast<int>( m_AverageFPS );
				float averageMS = 1000.0f / m_AverageFPS;
				float frameMS = realFrameTime * 1000.0f;
				GetFPSColor( nFps, ucColor );
				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2, ucColor[0], ucColor[1], ucColor[2], 255, "%3i fps (%3i, %3i) smth:%4.1f ms frm:%4.1f ms on %s", nFps, m_low, m_high, averageMS, frameMS, engine->GetLevelName() );
			}
			else
			{
				m_AverageFPS = -1;
				float flFps = ( 1.0f / realFrameTime );

				// If we're just below an integer boundary, we're good enough to call ourselves good WRT to coloration
				if ( (int)(flFps + 0.05) > (int )( flFps ) )
				{
					flFps = ceil( flFps );
				}
				nFps = static_cast<int>( flFps );
				GetFPSColor( nFps, ucColor );
				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2, ucColor[0], ucColor[1], ucColor[2], 255, "%3i fps on %s", nFps, engine->GetLevelName() );
			}

			const CPUFrequencyResults frequency = GetCPUFrequencyResults();
			double currentTime = Plat_FloatTime();
			const double displayTime = 5.0f; // Display frequency results for this long.
			if ( frequency.m_GHz > 0 && frequency.m_timeStamp + displayTime > currentTime )
			{
				// Optionally print out the CPU frequency monitoring data.
				GetCPUColor( frequency.m_percentage, ucColor );
				g_pMatSystemSurface->DrawColoredText( m_hFont, x, lineHeight + 2, ucColor[0], ucColor[1], ucColor[2], 255, "CPU frequency percent: %3.1f%%   Min percent: %3.1f%%", frequency.m_percentage, frequency.m_lowestPercentage );
			}
		}
	}
	else if ( m_fhLog != FILESYSTEM_INVALID_HANDLE )
	{
		DumpStats();
	}

	m_lastRealTime = gpGlobals->realtime;

	if ( uiAPCI )
	{
		static FpsSpikesTracker_t<100> s_tracker;
		s_tracker.AddSample( apci );

		struct FpsDetail_t { char *sz; float fl; };
		static ConVarRef mat_queue_mode( "mat_queue_mode" );
		static ConVarRef mat_vsync( "mat_vsync" );
		int nMSTmode = mat_queue_mode.GetInt();
		char *szMSTname = ( char * )( (nMSTmode == 2) ? "QMS2" : ( (nMSTmode == 1) ? "QMS1" : ( ( nMSTmode == -1 ) ? "QMS-" : "QMS0" ) ) );
		char *szFlipName = ( char * ) ( mat_vsync.GetBool() ? "TOTAL" : "TOTAL" );
		FpsDetail_t arrGrid[6][5] = {
			{ { "PERF/ms",0 },		{ "MAX",0 },					{ "AVG",0 },					{ "MIN",0 },					{ "CUR",0 } },
			{ { "MAIN",0 },			{ 0,s_tracker.m_max.msMain },	{ 0,s_tracker.m_avg.msMain },	{ 0,s_tracker.m_min.msMain },	{ 0,s_tracker.m_cur.msMain } },
			{ { szMSTname,0 },		{ 0,s_tracker.m_max.msMST },	{ 0,s_tracker.m_avg.msMST },	{ 0,s_tracker.m_min.msMST },	{ 0,s_tracker.m_cur.msMST } },
			{ { "GPU",0 },			{ 0,s_tracker.m_max.msGPU },	{ 0,s_tracker.m_avg.msGPU },	{ 0,s_tracker.m_min.msGPU },	{ 0,s_tracker.m_cur.msGPU } },
			{ { szFlipName,0 },		{ 0,s_tracker.m_max.msFlip },	{ 0,s_tracker.m_avg.msFlip },	{ 0,s_tracker.m_min.msFlip },	{ 0,s_tracker.m_cur.msFlip } },
			{ { "TOTAL",0 },		{ 0,s_tracker.m_max.msTotal },	{ 0,s_tracker.m_avg.msTotal },	{ 0,s_tracker.m_min.msTotal },	{ 0,s_tracker.m_cur.msTotal } }
		};
		int numRowsToDisplay = ( cl_showfps.GetInt() >= 20 ? 6 : 5 );
		if (IsPS3() || IsX360() )
		{
			if(cl_showfps.GetInt() > 100) 
			{
				numRowsToDisplay = 2;
				
				int32 row = cl_showfps.GetInt() - 100;
				row = MAX(row,1);
				row = MIN(row,3);

				V_memcpy(arrGrid[1], arrGrid[row], sizeof(arrGrid[1]));
			}
		}

		for ( int iRow = 0; iRow < numRowsToDisplay; ++ iRow )
		{
			if ( iRow == 2 && !nMSTmode )
				continue;
			i++;
			for ( int iCol = 0; iCol < 5; ++ iCol )
			{
				unsigned char ucColor[3] = { 255, 255, 255 };
				if ( !arrGrid[iRow][iCol].sz ) GetFPSColor( ( arrGrid[iRow][iCol].fl > 0.1 ) ? ( 1000.0f / arrGrid[iRow][iCol].fl ) : 1000, ucColor );
				g_pMatSystemSurface->DrawColoredText( m_hFont, x + iCol*lineHeight*6, 2 + i * lineHeight, 
					ucColor[0], ucColor[1], ucColor[2], 255, 
					arrGrid[iRow][iCol].sz ? arrGrid[iRow][iCol].sz : (char*)"%0.2f", 
					arrGrid[iRow][iCol].fl );
			}
			if ( iRow == 0 )
			{
				g_pMatSystemSurface->DrawColoredText( m_hFont, x + 5*lineHeight*6, 2 + i * lineHeight, 
					255, 255, 255, 255, 
					"%s @ %dfps", 
					engine->GetLevelNameShort(),
					( s_tracker.m_avg.msFlip > 0.1 ) ? int( ( 1000.0f / s_tracker.m_avg.msFlip ) + 0.2 ) : 1000 );
			}
		}

		i++;
	}
	if( aici.m_nDeferredWordsAllocated )
	{
		g_pMatSystemSurface->DrawColoredText( m_hFont, x + 6*lineHeight*6, 2 + 2 * lineHeight, 255, 255, 255, 255,
			"defer %u kB", ( aici.m_nDeferredWordsAllocated + 255 ) / 256 );
	}
	

	int nShowPosMode = cl_showpos.GetInt();
	if ( nShowPosMode > 0 )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			Vector vecOrigin = MainViewOrigin(hh);
			QAngle angles = MainViewAngles(hh);
			Vector vel( 0, 0, 0 );

			char szName[ 32 ] = { 0 };

			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer(hh);
			if ( pPlayer )
			{
				Q_strncpy( szName, pPlayer->GetPlayerName(), sizeof( szName ) );
				vel = pPlayer->GetLocalVelocity();
			}
			if ( !engine->IsConnected() )
			{
				V_sprintf_safe( szName, "unconnected" );
			}
			else if ( engine->GetDemoPlaybackParameters() && engine->GetDemoPlaybackParameters()->m_bAnonymousPlayerIdentity )
			{
				V_sprintf_safe( szName, "unknown" );
			}

			if ( nShowPosMode == 2 && pPlayer )
			{
				vecOrigin = pPlayer->GetAbsOrigin();
				angles = pPlayer->GetAbsAngles();
			}

			if ( uiAPCI )
			{
				i++;

				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2+ i * lineHeight, 
					255, 255, 255, 255, 
					"pos:  %.02f %.02f %.02f   (%s)", 
					vecOrigin.x, vecOrigin.y, vecOrigin.z, szName );
				i++;

				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
					255, 255, 255, 255, 
					"ang:  %.02f %.02f %.02f   (vel:  %.2f)", 
					angles.x, angles.y, angles.z, vel.Length2D() );
			}
			else
			{
				i++;

				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
														255, 255, 255, 255, 
														"name: %s", szName );
				i++;

				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2+ i * lineHeight, 
													  255, 255, 255, 255, 
													  "pos:  %.02f %.02f %.02f", 
													  vecOrigin.x, vecOrigin.y, vecOrigin.z );
				i++;

				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
													  255, 255, 255, 255, 
													  "ang:  %.02f %.02f %.02f", 
													  angles.x, angles.y, angles.z );
				i++;

				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
													  255, 255, 255, 255, 
													  "vel:  %.2f", 
													  vel.Length2D() );
			}
		}
		#ifdef PORTAL
		if ( uiAPCI )
		{
			static IPortalServerDllPropPortalLocator *s_pPortalLocator;
			if ( !s_pPortalLocator )
			{
				if ( g_pMatchFramework )
				{
					s_pPortalLocator = ( IPortalServerDllPropPortalLocator * )
						g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( IEXTPROPPORTALLOCATOR_INTERFACE_NAME );
				}
			}
			if ( s_pPortalLocator )
			{
				CUtlVector < IPortalServerDllPropPortalLocator::PortalInfo_t > arrPortals;
				arrPortals.EnsureCapacity( 4 );
				s_pPortalLocator->LocateAllPortals( arrPortals );

				i++;
				for ( int j = 0; j < arrPortals.Count(); ++ j )
				{
					IPortalServerDllPropPortalLocator::PortalInfo_t const &pi = arrPortals[j];
					i++;
					g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
						255, 255, 255, 255, 
						"P %d %d %.02f %.02f %.02f %.02f %.02f %.02f", 
						pi.iLinkageGroupId, pi.nPortal,
						pi.vecOrigin.x, pi.vecOrigin.y, pi.vecOrigin.z,
						pi.vecAngle.x, pi.vecAngle.y, pi.vecAngle.z
						);
				}
			}
		}
		#endif
	}

#ifdef CSTRIKE15

	if ( cl_countbones.GetBool() )
	{
		i++;
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight,
				255, 255, 255, 255,
				"All computed bones:  %i",
				g_nNumBonesSetupAll );
		i++;
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight,
				255, 255, 255, 255,
				"Blending rules only:  %i",
				g_nNumBonesSetupBlendingRulesOnly );
		
	}

#endif
	
	if ( cl_showbattery.GetInt() > 0 )
	{
		if ( steamapicontext && steamapicontext->SteamUtils() && 
			( m_lastBatteryPercent == -1.0f || (gpGlobals->realtime - m_lastBatteryPercent) > 10.0f ) )
		{
			m_BatteryPercent = steamapicontext->SteamUtils()->GetCurrentBatteryPower();
			m_lastBatteryPercent = gpGlobals->realtime;
		}
		
		if ( m_BatteryPercent > 0 )
		{
			if ( m_BatteryPercent == 255 )
			{
				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
													  255, 255, 255, 255,
													  "battery: On AC" );
			}
			else
			{
				g_pMatSystemSurface->DrawColoredText( m_hFont, x, 2 + i * lineHeight, 
													  255, 255, 255, 255,
													  "battery:  %d%%",
													  m_BatteryPercent );	
			}
		}
	}


#ifdef _GAMECONSOLE
	// Display a warning message if free memory dips below a certain threshold
	size_t nUsedMem = 0, nFreeMem = 0, nMemoryThreshold = 1024*1024*cl_showlowmemory.GetInt();
	if ( nMemoryThreshold )
	{
		g_pMemAlloc->GlobalMemoryStatus( &nUsedMem, &nFreeMem );
		if ( nFreeMem < nMemoryThreshold )
		{
			i += 2;
			g_pMatSystemSurface->DrawColoredText(	m_hFont, x, 2 + i * lineHeight, 255, 150, 40, 255,
													"WARNING: low memory! (%3.1fMB)  Report if seen at a test station...\n", nFreeMem/(float)(1024*1024) );
		}
	}
#endif // _GAMECONSOLE

	if ( m_nLinesNeeded != i )
	{
	    m_nLinesNeeded = i;
		ComputeSize();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Outputs the frame rate histogram and closes the file
//-----------------------------------------------------------------------------
void CFPSPanel::DumpStats() 
{
	Assert ( m_fhLog != FILESYSTEM_INVALID_HANDLE );
	if ( m_nNumFramesTotal	> 0 )
	{
		g_pFullFileSystem->FPrintf( m_fhLog, "\n\nTotal Frames : %3i\n\n", m_nNumFramesTotal );
		g_pFullFileSystem->FPrintf( m_fhLog, "Frame Rate, Number of Frames, Percent of Frames\n" );

		for ( int i = 0; i < PERF_HISTOGRAM_BUCKET_SIZE; i++ )
		{
			float flPercent = m_nNumFramesBucket[i];
			flPercent /= m_nNumFramesTotal;
			flPercent *= 100.0f;
			g_pFullFileSystem->FPrintf( m_fhLog, "%3i, %3i, %5.1f\n", i, m_nNumFramesBucket[i], flPercent );
			m_nNumFramesBucket[i] = 0;
		}
	}
	g_pFullFileSystem->Close( m_fhLog );
	m_fhLog = FILESYSTEM_INVALID_HANDLE;
	m_nNumFramesTotal = 0;
}

class CFPS : public IFPSPanel
{
private:
	CFPSPanel *fpsPanel;
public:
	CFPS( void )
	{
		fpsPanel = NULL;
	}

	void Create( vgui::VPANEL parent )
	{
		fpsPanel = new CFPSPanel( parent );
	}

	void Destroy( void )
	{
		if ( fpsPanel )
		{
			fpsPanel->SetParent( (vgui::Panel *)NULL );
			delete fpsPanel;
		}
	}
};

static CFPS g_FPSPanel;
IFPSPanel *fps = ( IFPSPanel * )&g_FPSPanel;

#if defined( TRACK_BLOCKING_IO )

static ConVar cl_blocking_threshold( "cl_blocking_threshold", "0.000", 0, "If file ops take more than this amount of time, add to 'spewblocking' history list" );

void ShowBlockingChanged( ConVar *var, char const *pOldString )
{
	filesystem->EnableBlockingFileAccessTracking( var->GetBool() );
}

static ConVar cl_showblocking( "cl_showblocking", "0", 0, "Show blocking i/o on top of fps panel", ShowBlockingChanged );
static ConVar cl_blocking_recentsize( "cl_blocking_recentsize", "40", 0, "Number of items to store in recent spew history." );

//-----------------------------------------------------------------------------
// Purpose: blocking i/o indicator
//-----------------------------------------------------------------------------
class CBlockingFileIOPanel : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
	CBlockingFileIOPanel( vgui::VPANEL parent );
	virtual			~CBlockingFileIOPanel( void );

	virtual void	ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void	Paint();
	virtual void	OnTick( void );

	virtual bool	ShouldDraw( void );

	void			SpewRecent();

private:
	void			DrawIOTime( int x, int y, int w, int h, int slot, char const *label, const Color& clr );

	vgui::HFont		m_hFont;

	struct Graph_t
	{
		float			m_flCurrent;

		float			m_flHistory;
		float			m_flHistorySpike;
		float			m_flLatchTime;
		CUtlSymbol		m_LastFile;
	};

	Graph_t			m_History[ FILESYSTEM_BLOCKING_NUMBINS ];

	struct RecentPeaks_t
	{
		float		time;
		CUtlSymbol	fileName;
		float		elapsed;
		byte		reason;
		byte		ioType;
	};

	CUtlLinkedList< RecentPeaks_t, unsigned short >	m_Recent;

	void			SpewItem( const RecentPeaks_t& item );
};

#define IO_PANEL_WIDTH		400
#define IO_DECAY_FRAC		0.95f

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//-----------------------------------------------------------------------------
CBlockingFileIOPanel::CBlockingFileIOPanel( vgui::VPANEL parent ) : BaseClass( NULL, "CBlockingFileIOPanel" )
{
	SetParent( parent );
	int wide, tall;
	vgui::ipanel()->GetSize( parent, wide, tall );

	int x = 2;
	int y = 100;
	if ( IsGameConsole() )
	{
		x += XBOX_MAXBORDERSAFE * wide;
		y += XBOX_MAXBORDERSAFE * tall;
	}
	SetPos( x, y );

	SetSize( IO_PANEL_WIDTH, 140 );

	SetVisible( false );
	SetCursor( 0 );

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( false );

	m_hFont = 0;

	vgui::ivgui()->AddTickSignal( GetVPanel(), 250 );
	SetZPos( 1000 );
	Q_memset( m_History, 0, sizeof( m_History ) );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled( false );
	MakePopup();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBlockingFileIOPanel::~CBlockingFileIOPanel( void )
{
}

void CBlockingFileIOPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "Default" );
	Assert( m_hFont );

	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBlockingFileIOPanel::OnTick( void )
{
	bool bVisible = ShouldDraw();
	if ( IsVisible() != bVisible )
	{
		SetVisible( bVisible );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBlockingFileIOPanel::ShouldDraw( void )
{
	if ( !cl_showblocking.GetInt() )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void CBlockingFileIOPanel::Paint() 
{
	int x = 2;
	
	int maxRecent = clamp( 0, cl_blocking_recentsize.GetInt(), 1000 );
	int bval = cl_showblocking.GetInt();
	if ( bval > 0 )
	{
		IBlockingFileItemList *list = filesystem->RetrieveBlockingFileAccessInfo();
		if ( list )
		{
			int i;
			int c = ARRAYSIZE( m_History );
			for ( i = 0; i < c; ++i )
			{
				m_History[ i ].m_flCurrent = 0.0f;
			}

			// Grab mutex (prevents async thread from filling in even more data...)
			list->LockMutex();
		{
			for ( int j = list->First() ; j != list->InvalidIndex(); j = list->Next( j ) )
			{
				const FileBlockingItem& item = list->Get( j );

				m_History[ item.m_ItemType ].m_flCurrent += item.m_flElapsed;

				RecentPeaks_t recent;
				recent.time = gpGlobals->realtime;
				recent.elapsed = item.m_flElapsed;
				recent.fileName = item.GetFileName();
				recent.reason = item.m_ItemType;
				recent.ioType = item.m_nAccessType;
				while ( m_Recent.Count() > maxRecent )
				{
					m_Recent.Remove( m_Recent.Head() );
				}

				m_Recent.AddToTail( recent );

				m_History[ item.m_ItemType ].m_LastFile = item.GetFileName();

				// Only care about time consuming synch or async blocking calls
				if ( item.m_ItemType == FILESYSTEM_BLOCKING_SYNCHRONOUS ||
					 item.m_ItemType == FILESYSTEM_BLOCKING_ASYNCHRONOUS_BLOCK )
				{
					if ( item.m_flElapsed > cl_blocking_threshold.GetFloat() )
					{
						SpewItem( recent );
					}
				}
			}
			list->Reset();
		}
		// Finished
		list->UnlockMutex();

		// Now draw some bars...
		int itemHeight = ( vgui::surface()->GetFontTall( m_hFont ) + 2 );

		int y = 2;
		int w = GetWide();

		DrawIOTime( x, y, w, itemHeight, FILESYSTEM_BLOCKING_SYNCHRONOUS, "Synchronous", Color( 255, 0, 0, 255 ) );
		y += 2*( itemHeight + 2 );
		DrawIOTime( x, y, w, itemHeight, FILESYSTEM_BLOCKING_ASYNCHRONOUS_BLOCK, "Async Block", Color( 255, 100, 0, 255 ) );
		y += 2*( itemHeight + 2 );
		DrawIOTime( x, y, w, itemHeight, FILESYSTEM_BLOCKING_CALLBACKTIMING, "Callback", Color( 255, 255, 0, 255 ) );
		y += 2*( itemHeight + 2 );
		DrawIOTime( x, y, w, itemHeight, FILESYSTEM_BLOCKING_ASYNCHRONOUS, "Asynchronous", Color( 0, 255, 0, 255 ) );

		for ( i = 0; i < c; ++i )
		{
			if ( m_History[ i ].m_flCurrent > m_History[ i ].m_flHistory )
			{
				m_History[ i ].m_flHistory = m_History[ i ].m_flCurrent;
				m_History[ i ].m_flHistorySpike = m_History[ i ].m_flCurrent;
				m_History[ i ].m_flLatchTime = gpGlobals->realtime;
			}
			else
			{
				// After this long, start to decay the previous history value
				if ( gpGlobals->realtime > m_History[ i ].m_flLatchTime + 1.0f )
				{
					m_History[ i ].m_flHistory = m_History[ i ].m_flHistory * IO_DECAY_FRAC + ( 1.0f - IO_DECAY_FRAC ) * m_History[ i ].m_flCurrent;
				}
			}
		}
		}
	}
}

static ConVar cl_blocking_msec( "cl_blocking_msec", "100", 0, "Vertical scale of blocking graph in milliseconds" );

static const char *GetBlockReason( int reason )
{
	switch ( reason )
	{
		case FILESYSTEM_BLOCKING_SYNCHRONOUS:
			return "Synchronous";
		case FILESYSTEM_BLOCKING_ASYNCHRONOUS:
			return "Asynchronous";
		case FILESYSTEM_BLOCKING_CALLBACKTIMING:
			return "Async Callback";
		case FILESYSTEM_BLOCKING_ASYNCHRONOUS_BLOCK:
			return "Async Blocked";
	}
	return "???";
}

static const char *GetIOType( int iotype )
{
	if ( FileBlockingItem::FB_ACCESS_APPEND == iotype )
	{
		return "Append";
	}
	else if ( FileBlockingItem::FB_ACCESS_CLOSE == iotype )
	{
		return "Close";
	}
	else if ( FileBlockingItem::FB_ACCESS_OPEN == iotype)
	{
		return "Open";
	}
	else if ( FileBlockingItem::FB_ACCESS_READ == iotype)
	{
		return "Read";
	}
	else if ( FileBlockingItem::FB_ACCESS_SIZE == iotype)
	{
		return "Size";
	}
	else if ( FileBlockingItem::FB_ACCESS_WRITE == iotype)
	{
		return "Write";
	}
	return "???";
}

void CBlockingFileIOPanel::SpewItem( const RecentPeaks_t& item )
{
	switch ( item.reason )
	{
		default:
			Assert( 0 );
			// break; -- intentionally fall through
		case FILESYSTEM_BLOCKING_ASYNCHRONOUS:
		case FILESYSTEM_BLOCKING_CALLBACKTIMING:
			Msg( "%8.3f %16.16s i/o [%6.6s] took %8.3f msec:  %33.33s\n", 
				 item.time, 
				 GetBlockReason( item.reason ), 
				 GetIOType( item.ioType ),
				 item.elapsed * 1000.0f, 
				 item.fileName.String()
				);
			break;
		case FILESYSTEM_BLOCKING_SYNCHRONOUS:
		case FILESYSTEM_BLOCKING_ASYNCHRONOUS_BLOCK:
			Warning( "%8.3f %16.16s i/o [%6.6s] took %8.3f msec:  %33.33s\n", 
					 item.time, 
					 GetBlockReason( item.reason ), 
					 GetIOType( item.ioType ),
					 item.elapsed * 1000.0f, 
					 item.fileName.String()
				);
			break;
	}
}

void CBlockingFileIOPanel::SpewRecent()
{
	FOR_EACH_LL( m_Recent, i )
	{
		const RecentPeaks_t& item = m_Recent[ i ];
		SpewItem( item );
	}
}

void  CBlockingFileIOPanel::DrawIOTime( int x, int y, int w, int h, int slot, char const *label, const Color& clr )
{
	float t = m_History[ slot ].m_flCurrent;
	float history = m_History[ slot ].m_flHistory;
	float latchedtime = m_History[ slot ].m_flLatchTime;
	float historyspike = m_History[ slot ].m_flHistorySpike;

	// 250 msec is considered a huge spike
	float maxTime = cl_blocking_msec.GetFloat() * 0.001f;
	if ( maxTime < 0.000001f )
		return;
	float frac = clamp( t / maxTime, 0.0f, 1.0f );
	float hfrac = clamp( history / maxTime, 0.0f, 1.0f );
	float spikefrac = clamp( historyspike / maxTime, 0.0f, 1.0f );

	g_pMatSystemSurface->DrawColoredText( m_hFont, x + 2, y + 1, 
										  clr[0], clr[1], clr[2], clr[3], 
										  "%s", 
										  label );

	int textWidth = 95;

	x += textWidth;
	w -= ( textWidth + 5 );

	int prevFileWidth = 140;
	w -= prevFileWidth;

	bool bDrawHistorySpike = false;

	if ( m_History[ slot ].m_LastFile.IsValid() && 
		 ( gpGlobals->realtime < latchedtime + 10.0f ) )
	{
		bDrawHistorySpike = true;
		g_pMatSystemSurface->DrawColoredText( m_hFont, x + w + 5, y + 1, 
											  255, 255, 255, 200, "[%8.3f ms]", m_History[ slot ].m_flHistorySpike * 1000.0f );
		g_pMatSystemSurface->DrawColoredText( m_hFont, x, y + h + 1, 
											  255, 255, 255, 200, "%s", m_History[ slot ].m_LastFile.String() );
	}

	y += 2;
	h -= 4;

	int barWide = ( int )( w * frac + 0.5f );
	int historyWide = ( int ) ( w * hfrac + 0.5f );
	int spikeWide = ( int ) ( w * spikefrac + 0.5f );

	int useWide = MAX( barWide, historyWide );

	vgui::surface()->DrawSetColor( Color( 0, 0, 0, 31 ) );
	vgui::surface()->DrawFilledRect( x, y, x + w, y + h );
	vgui::surface()->DrawSetColor( Color( 255, 255, 255, 128 ) );
	vgui::surface()->DrawOutlinedRect( x, y, x + w, y + h );
	vgui::surface()->DrawSetColor( clr );
	vgui::surface()->DrawFilledRect( x+1, y+1, x + useWide, y + h -1 );
	if ( bDrawHistorySpike )
	{
		vgui::surface()->DrawSetColor( Color( 255, 255, 255, 192 ) );
		vgui::surface()->DrawFilledRect( x + spikeWide, y + 1, x + spikeWide + 1, y + h - 1 );
	}
}

class CBlockingFileIO : public IShowBlockingPanel
{
private:
	CBlockingFileIOPanel *ioPanel;
public:
	CBlockingFileIO( void )
	{
		ioPanel = NULL;
	}

	void Create( vgui::VPANEL parent )
	{
		ioPanel = new CBlockingFileIOPanel( parent );
	}

	void Destroy( void )
	{
		if ( ioPanel )
		{
			ioPanel->SetParent( (vgui::Panel *)NULL );
			delete ioPanel;
		}
	}

	void Spew()
	{
		if ( ioPanel )
		{
			ioPanel->SpewRecent();
		}
	}
};

static CBlockingFileIO g_IOPanel;
IShowBlockingPanel *iopanel = ( IShowBlockingPanel * )&g_IOPanel;

CON_COMMAND( spewblocking, "Spew current blocking file list." )
{
	g_IOPanel.Spew();
}

#endif // TRACK_BLOCKING_IO
