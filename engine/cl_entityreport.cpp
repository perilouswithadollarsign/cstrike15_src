//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "client_pch.h"
#include "ivideomode.h"
#include "characterset.h"
#include <ctype.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_entityreport( "cl_entityreport", "0", FCVAR_CHEAT, "For debugging, draw entity states to console" );

static ConVar er_colwidth( "er_colwidth", "100", 0 );
static ConVar er_maxname( "er_maxname", "14", 0 );
static ConVar er_graphwidthfrac( "er_graphwidthfrac", "0.2", 0 );

// How quickly to move rolling average for entityreport
#define BITCOUNT_AVERAGE 0.95f
// How long to flush item when something important happens
#define EFFECT_TIME  1.5f
// How long to latch peak bit count for item
#define PEAK_LATCH_TIME 2.0f;

//-----------------------------------------------------------------------------
// Purpose: Entity report event types
//-----------------------------------------------------------------------------
enum
{
	FENTITYBITS_ZERO = 0,
	FENTITYBITS_ADD = 0x01,
	FENTITYBITS_LEAVEPVS = 0x02,
	FENTITYBITS_DELETE = 0x04,
};

//-----------------------------------------------------------------------------
// Purpose: Data about an entity
//-----------------------------------------------------------------------------
class CEntityBits
{
public:
	CEntityBits() :
		bits( 0 ),
		average( 0.0f ),
		peak( 0 ),
		peaktime( 0.0f ),
		flags( 0 ),
		effectfinishtime( 0.0f ),
		deletedclientclass( NULL )
	{
	}

	// Bits used for last message
	int				bits;
	// Rolling average of bits used
	float			average;
	// Last bit peak
	int				peak;
	// Time at which peak was last reset
	float			peaktime;
	// Event info
	int				flags;
	// If doing effect, when it will finish
	float			effectfinishtime;
	// If event was deletion, remember client class for a little bit
	ClientClass		*deletedclientclass;
};

class CEntityReportManager
{
public:

	void Reset();
	void Record( int entnum, int bitcount );
	void Add( int entnum );
	void LeavePVS( int entnum );
	void DeleteEntity( int entnum, ClientClass *pclass );

	int  Count();
	CEntityBits *Base();

private:
	CUtlVector< CEntityBits > m_EntityBits;
};

static CEntityReportManager g_EntityReportMgr;

void CL_ResetEntityBits( void )
{
	g_EntityReportMgr.Reset();
}

void CL_RecordAddEntity( int entnum )
{
	g_EntityReportMgr.Add( entnum );
}

void CL_RecordEntityBits( int entnum, int bitcount )
{
	g_EntityReportMgr.Record( entnum, bitcount );
}

void CL_RecordLeavePVS( int entnum )
{
	g_EntityReportMgr.LeavePVS( entnum );
}

void CL_RecordDeleteEntity( int entnum, ClientClass *pclass )
{
	g_EntityReportMgr.DeleteEntity( entnum, pclass );
}

//-----------------------------------------------------------------------------
// Purpose: Wipe structure ( level transition/startup )
//-----------------------------------------------------------------------------
void CEntityReportManager::Reset()
{
	m_EntityBits.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Record activity
// Input  : entnum - 
//			bitcount - 
//-----------------------------------------------------------------------------
void CEntityReportManager::Record( int entnum, int bitcount )
{
	if ( entnum < 0 || entnum >= MAX_EDICTS ) 
	{
		return;
	}

	m_EntityBits.EnsureCount( entnum + 1 );

	CEntityBits *slot = &m_EntityBits[ entnum ];

	slot->bits = bitcount;
	// Update average
	slot->average = ( BITCOUNT_AVERAGE ) * slot->average + ( 1.f - BITCOUNT_AVERAGE ) * bitcount;

	// Recompute peak
	if ( realtime >= slot->peaktime )
	{
		slot->peak = 0.0f;
		slot->peaktime = realtime + PEAK_LATCH_TIME;
	}

	// Store off peak
	if ( bitcount > slot->peak )
	{
		slot->peak = bitcount;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Record entity add event
// Input  : entnum - 
//-----------------------------------------------------------------------------
void CEntityReportManager::Add( int entnum )
{
	if ( !cl_entityreport.GetBool() || entnum < 0 || entnum >= MAX_EDICTS )
	{
		return;
	}

	m_EntityBits.EnsureCount( entnum + 1 );

	CEntityBits *slot = &m_EntityBits[ entnum ];
	slot->flags = FENTITYBITS_ADD;
	slot->effectfinishtime = realtime + EFFECT_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: record entity leave event
// Input  : entnum - 
//-----------------------------------------------------------------------------
void CEntityReportManager::LeavePVS( int entnum )
{
	if ( !cl_entityreport.GetBool() || entnum < 0 || entnum >= MAX_EDICTS )
	{
		return;
	}

	m_EntityBits.EnsureCount( entnum + 1 );

	CEntityBits *slot = &m_EntityBits[ entnum ];
	slot->flags = FENTITYBITS_LEAVEPVS;
	slot->effectfinishtime = realtime + EFFECT_TIME;
}

//-----------------------------------------------------------------------------
// Purpose: record entity deletion event
// Input  : entnum - 
//			*pclass - 
//-----------------------------------------------------------------------------
void CEntityReportManager::DeleteEntity( int entnum, ClientClass *pclass )
{
	if ( !cl_entityreport.GetBool() || entnum < 0 || entnum >= MAX_EDICTS )
	{
		return;
	}

	m_EntityBits.EnsureCount( entnum + 1 );

	CEntityBits *slot = &m_EntityBits[ entnum ];
	slot->flags = FENTITYBITS_DELETE;
	slot->effectfinishtime = realtime + EFFECT_TIME;
	slot->deletedclientclass = pclass;
}

int CEntityReportManager::Count()
{
	return m_EntityBits.Count();
}

CEntityBits *CEntityReportManager::Base()
{
	return m_EntityBits.Base();
}

//-----------------------------------------------------------------------------
// Purpose: Shows entity status report if cl_entityreport cvar is set
//-----------------------------------------------------------------------------
class CEntityReportPanel : public CBasePanel
{
	typedef CBasePanel BaseClass;
public:
	// Construction
					CEntityReportPanel( vgui::Panel *parent );
	virtual			~CEntityReportPanel( void );

	// Refresh
	virtual void	Paint();
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool	ShouldDraw( void );

	// Helpers
	virtual void	ApplyEffect( CEntityBits *entry, int& r, int& g, int& b );

private:

	char const		*MaybeTruncateName( int maxname, char const *pchName );

	// Font to use for drawing
	vgui::HFont		m_hFont;

	characterset_t		m_BreakSetVowels;
};

static CEntityReportPanel *g_pEntityReportPanel = NULL;

//-----------------------------------------------------------------------------
// Purpose: Creates the CEntityReportPanel VGUI panel
// Input  : *parent - 
//-----------------------------------------------------------------------------
void CL_CreateEntityReportPanel( vgui::Panel *parent )
{
	g_pEntityReportPanel = new CEntityReportPanel( parent );
}

//-----------------------------------------------------------------------------
// Purpose: Instances the entity report panel
// Input  : *parent - 
//-----------------------------------------------------------------------------
CEntityReportPanel::CEntityReportPanel( vgui::Panel *parent ) :
	CBasePanel( parent, "CEntityReportPanel" )
{
	// Need parent here, before loading up textures, so getSurfaceBase 
	//  will work on this panel ( it's 0 otherwise )
	int nWidth = videomode->GetModeWidth();
	int nHeight = videomode->GetModeHeight();

	if ( IsGameConsole() )
	{
		SetSize( ( int )( nWidth * 0.9f ), ( int )( nHeight * 0.9f ) );
		SetPos( ( int )( nWidth * 0.05f ), ( int )( nHeight * 0.05f ) );
	}
	else
	{
		SetSize( nWidth, nHeight );
		SetPos( 0, 0 );
	}
	
	SetVisible( true );
	SetCursor( 0 );

	m_hFont = vgui::INVALID_FONT;

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( false );
	SetPaintBorderEnabled(false);

	CharacterSetBuild( &m_BreakSetVowels, "aeiou" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEntityReportPanel::~CEntityReportPanel( void )
{
}

void CEntityReportPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// If you change this font, be sure to mark it with
	// $use_in_fillrate_mode in its .vmt file
	if ( IsGameConsole() )
	{
		// This is one of the few fonts we have loaded in shipping console builds
		m_hFont = pScheme->GetFont( "DebugFixed", false );
	}
	else
	{
		m_hFont = pScheme->GetFont( "DefaultVerySmall", false );
	}
	
	Assert( m_hFont );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEntityReportPanel::ShouldDraw( void )
{
	if ( !cl_entityreport.GetInt() )
	{
		return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Helper to flash colors
// Input  : cycle - 
//			value - 
// Output : static int
//-----------------------------------------------------------------------------
static int MungeColorValue( float cycle, int& value )
{
	int midpoint;
	int remaining;
	bool invert = false;

	if ( value < 128 )
	{
		invert = true;
		value = 255 - value;
	}

	midpoint = value / 2;

	remaining = value - midpoint;
	midpoint = midpoint + remaining / 2;
		
	value = midpoint + ( remaining / 2 ) * cycle;
	if ( invert )
	{
		value = 255 - value;
	}

	value = MAX( 0, value );
	value = MIN( 255, value );
	return value;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frac - 
//			r - 
//			g - 
//			b - 
//-----------------------------------------------------------------------------
void CEntityReportPanel::ApplyEffect( CEntityBits *entry, int& r, int& g, int& b )
{
	bool effectactive = ( realtime <= entry->effectfinishtime ) ? true : false;
	if ( !effectactive )
		return;

	float frequency = 3.0f;

	float frac = ( EFFECT_TIME - ( entry->effectfinishtime - realtime ) ) / EFFECT_TIME;
	frac = MIN( 1.0, frac );
	frac = MAX( 0.0, frac );

	frac *= 2.0 * M_PI;
	frac = sin( frequency * frac );

	if ( entry->flags & FENTITYBITS_LEAVEPVS )
	{
		r = MungeColorValue( frac, r );
	}
	else if ( entry->flags & FENTITYBITS_ADD )
	{
		g = MungeColorValue( frac, g );
	}
	else if ( entry->flags & FENTITYBITS_DELETE )
	{
		r = MungeColorValue( frac, r );
		g = MungeColorValue( frac, g );
		b = MungeColorValue( frac, b );
	}
}

char const *CEntityReportPanel::MaybeTruncateName( int maxname, char const *pchName )
{
	static char truncated[ 64 ];

	int len = Q_strlen( pchName );

	if ( *pchName == 'C' )
	{
		--len;
		++pchName;
	}

	int toRemove = len - maxname;

	char const *in = pchName;
	char *out = truncated;
	int outlen = 1;
	// Strip the vowels and lower case the rest
	while ( *in && outlen < sizeof( truncated ) )
	{
		char check = tolower( *in );

		if ( toRemove >= 0 &&
			IN_CHARACTERSET( m_BreakSetVowels, check ) )
		{
			++in;
			--toRemove;
			continue;
		}

		++outlen;
		*out++ = check;
		++in;
	}

	*out = 0;

	return truncated;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEntityReportPanel::Paint() 
{
	VPROF( "CEntityReportPanel::Paint" );

	if ( !m_hFont )
		return;

	if ( !GetBaseLocalClient().IsActive() )
		return;

	if ( !entitylist )
		return;

	int top = 5;
	int left = 5;
	int row = 0;
	int col = 0;
	int colwidth = er_colwidth.GetInt();
	int maxname = er_maxname.GetInt();
	int rowheight = vgui::surface()->GetFontTall( m_hFont );
	int screenw = videomode->GetModeWidth();
	int screenh = videomode->GetModeHeight();

	if ( IsGameConsole() )
	{
		screenw = ( int )( screenw * 0.9f );
		screenh = ( int )( screenh * 0.9f );
	}

	float graphfrac = clamp( er_graphwidthfrac.GetFloat(), 0.1f, 1.0f );

	IClientNetworkable *pNet;
	ClientClass			*pClientClass;
	bool				inpvs;
	int					r, g, b, a;
	bool				effectactive;
	CEntityBits			*entry;

	int lastused = g_EntityReportMgr.Count()-1;
	CEntityBits			*list = g_EntityReportMgr.Base();

	while ( lastused > 0 )
	{
		pNet	= entitylist->GetClientNetworkable( lastused );

		entry = &list[ lastused ];

		effectactive = ( realtime <= entry->effectfinishtime ) ? true : false;

		if ( pNet && pNet->GetClientClass() )
		{
			break;
		}

		if ( effectactive )
			break;

		lastused--;
	}

 	int start = 0;
 	if ( cl_entityreport.GetInt() > 1 )
 	{
 		start = cl_entityreport.GetInt();
 	}

	for ( int i = start; i <= lastused; i++ )
	{
		pNet	= entitylist->GetClientNetworkable( i );

		entry = &list[ i ];

		effectactive = ( realtime <= entry->effectfinishtime ) ? true : false;

		if ( pNet && ((pClientClass = pNet->GetClientClass())) != NULL )
		{
			inpvs = !pNet->IsDormant();
			if ( inpvs )
			{
 				if ( entry->average >= 5 )
 				{
 					r = 200; g = 200; b = 250;
 					a = 255;
 				}
 				else
 				{
 					r = 200; g = 255; b = 100;
 					a = 255;
 				}
			}
			else
			{
				r = 255; g = 150; b = 100;
				a = 255;
			}

			ApplyEffect( entry, r, g, b );

			char	text[256];
			wchar_t unicode[ 256 ];

			Q_snprintf( text, sizeof(text), "%i %s", i, MaybeTruncateName( maxname, pClientClass->m_pNetworkName ) );
			
			g_pVGuiLocalize->ConvertANSIToUnicode( text, unicode, sizeof( unicode ) );

			DrawColoredText( m_hFont, left + col * colwidth, top + row * rowheight, r, g, b, a, unicode );

			if ( inpvs )
			{
				float fracs[ 3 ];
				fracs[ 0 ] = (float)( entry->bits >> 3 ) / 100.0f;
				fracs[ 1 ] = (float)( entry->peak >> 3 ) / 100.0f;
				fracs[ 2 ] = (float)( (int)entry->average >> 3 ) / 100.0f;

				for ( int j = 0; j < 3; j++ )
				{
					fracs[ j ] = MAX( 0.0f, fracs[ j ] );
					fracs[ j ] = MIN( 1.0f, fracs[ j ] );
				}

				int rcright =  left + col * colwidth + colwidth-2;
				int wide = MAX( 1, colwidth * graphfrac );
				int rcleft = rcright - wide;
				int rctop = top + row * rowheight;
				int rcbottom = rctop + rowheight - 1;

				vgui::surface()->DrawSetColor( 63, 63, 63, 127 );
 				vgui::surface()->DrawFilledRect( rcleft, rctop, rcright, rcbottom );

				// draw a box around it
				vgui::surface()->DrawSetColor( 200, 200, 200, 127 );
				vgui::surface()->DrawOutlinedRect( rcleft, rctop, rcright, rcbottom );

				// Draw current as a filled rect
				vgui::surface()->DrawSetColor( 200, 255, 100, 192 );
				vgui::surface()->DrawFilledRect( rcleft, rctop + rowheight / 2, rcleft + wide * fracs[ 0 ], rcbottom - 1 );

				// Draw average a vertical bar
				vgui::surface()->DrawSetColor( 192, 192, 192, 255 );
				vgui::surface()->DrawFilledRect( rcleft + wide * fracs[ 2 ], rctop + rowheight / 2, rcleft + wide * fracs[ 2 ] + 1, rcbottom - 1 );

				// Draw peak as a vertical red tick
				vgui::surface()->DrawSetColor( 192, 0, 0, 255 );
				vgui::surface()->DrawFilledRect( rcleft + wide * fracs[ 1 ], rctop + 1, rcleft + wide * fracs[ 1 ] + 1, rctop + rowheight / 2 );
			}

		}
		
		row++;
		if ( top + row * rowheight > screenh - rowheight )
		{
			row = 0;
			col++;
			// No more space anyway, give up
			if ( left + ( col + 1 ) * colwidth > screenw )
				return;
		}
	}
}

