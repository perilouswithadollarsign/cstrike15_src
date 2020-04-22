//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_CLOSECAPTION_H
#define HUD_CLOSECAPTION_H
#ifdef _WIN32
#pragma once
#endif

#include "hudelement.h"
#include <vgui_controls/Panel.h>
#include "closedcaptions.h"
#include "usermessages.h"

class CSentence;
class C_BaseFlex;
class CCloseCaptionItem;
struct WorkUnitParams;
class CAsyncCaption;
struct AsyncCaptionData_t;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHudCloseCaption : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudCloseCaption, vgui::Panel );
public:
	DECLARE_MULTIPLY_INHERITED();

	explicit 		CHudCloseCaption( const char *pElementName );
	virtual 		~CHudCloseCaption();

	// Expire lingering items
	virtual void	OnTick( void );

	virtual void	LevelInit( void );

	virtual void	LevelShutdown( void )
	{
		m_bLevelShutDown = true;
		Reset();
		m_bLevelShutDown = false;
	}

	// Painting methods
	virtual void	Paint();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	
	bool MsgFunc_CloseCaption(const CCSUsrMsg_CloseCaption &msg);
	bool MsgFunc_CloseCaptionDirect(const CCSUsrMsg_CloseCaptionDirect &msg);

	// Clear all CC data
	void			Reset( void );
	void			Process( const wchar_t *stream, float duration, bool fromplayer, bool direct = false );
	
	bool			ProcessCaption( char const *tokenname, float duration, bool fromplayer = false, bool direct = false );
	bool			ProcessCaptionByHash( unsigned int hash, float duration, bool fromplayer, bool direct = false );
	void			ProcessCaptionDirect( char const *tokenname, float duration, bool fromplayer = false );

	void			ProcessSentenceCaptionStream( char const *tokenstream );
	void			PlayRandomCaption();

	void			InitCaptionDictionary( char const *language, bool bForce = false );
	bool 			AddFileToCaptionDictionary( const char *filename );
	void			OnFinishAsyncLoad( int nFileIndex, int nBlockNum, AsyncCaptionData_t *pData );

	void			Flush();
	void			TogglePaintDebug();

	enum
	{
		CCFONT_NORMAL = 0,
		CCFONT_ITALIC,
		CCFONT_BOLD,
		CCFONT_ITALICBOLD,
		CCFONT_CONSOLE,
		CCFONT_MAX
	};

	static int		GetFontNumber( bool bold, bool italic );

	void			Lock( void );
	void			Unlock( void );

	void			FindSound( char const *pchANSI );

	void			ClearCurrentLanguage();

	struct CaptionRepeat
	{
		CaptionRepeat() :
			m_nTokenIndex( 0 ),
			m_flLastEmitTime( 0 ),
			m_flInterval( 0 ),
			m_nLastEmitTick( 0 )
		{
		}
		int		m_nTokenIndex;
		int		m_nLastEmitTick;
		float	m_flLastEmitTime;
		float	m_flInterval;
	};

	CUserMessageBinder m_UMCMsgCloseCaption;
	CUserMessageBinder m_UMCMsgCloseCaptionDirect;

private:

	void ClearAsyncWork();
	void ProcessAsyncWork();
	bool AddAsyncWork( char const *tokenstream, bool bIsStream, float duration, bool fromplayer, bool direct = false );
	bool AddAsyncWorkByHash( unsigned int hash, float duration, bool fromplayer, bool direct = false );

	void _ProcessSentenceCaptionStream( int wordCount, char const *tokenstream, const wchar_t *caption_full );
	void _ProcessCaption( const wchar_t *caption, unsigned int hash, float duration, bool fromplayer, bool direct = false );

	CUtlLinkedList< CAsyncCaption *, unsigned short >	m_AsyncWork;

	CUtlRBTree< CaptionRepeat, int >	m_CloseCaptionRepeats;

	static bool CaptionTokenLessFunc( const CaptionRepeat &lhs, const CaptionRepeat &rhs );

	void	DrawStream( wrect_t& rect, wrect_t &rcWindow, CCloseCaptionItem *item, int iFadeLine, float flFadeLineAlpha ); 
	void	ComputeStreamWork( int available_width, CCloseCaptionItem *item );
	bool	SplitCommand( wchar_t const **ppIn, wchar_t *cmd, wchar_t *args ) const;

	bool	StreamHasCommand( const wchar_t *stream, const wchar_t *findcmd ) const;
	bool	GetFloatCommandValue( const wchar_t *stream, const wchar_t *findcmd, float& value ) const;

	bool	GetNoRepeatValue( const wchar_t *caption, float &retval );

	void	ParseCloseCaptionStream( const wchar_t *in, int available_width );
	bool	StreamHasCommand( const wchar_t *stream, const wchar_t *search );

	void	DumpWork( CCloseCaptionItem *item );

	void	AddWorkUnit( CCloseCaptionItem *item, WorkUnitParams& params );

	void	CreateFonts( void );

	void	LoadColorMap( const char *pFilename );
	bool	FindColorForTag( wchar_t *pTag, Color &tagColor );

	CUtlVector< CCloseCaptionItem * > m_Items;

	void SetUseAsianWordWrapping();

	vgui::HFont		m_hFonts[CCFONT_MAX];

	int			m_nLineHeight;

	int			m_nGoalHeight;
	int			m_nCurrentHeight;
	float		m_flGoalAlpha;
	float		m_flCurrentAlpha;
	float		m_flGoalHeightStartTime;
	float		m_flGoalHeightFinishTime;

	CPanelAnimationVar( float, m_flBackgroundAlpha, "BgAlpha", "192" );
	CPanelAnimationVar( float, m_flGrowTime, "GrowTime", "0.25" );
	CPanelAnimationVar( float, m_flItemHiddenTime, "ItemHiddenTime", "0.2" );
	CPanelAnimationVar( float, m_flItemFadeInTime, "ItemFadeInTime", "0.15" );
	CPanelAnimationVar( float, m_flItemFadeOutTime, "ItemFadeOutTime", "0.3" );
	CPanelAnimationVar( int, m_nTopOffset, "topoffset", "40" );

	CUtlVector< AsyncCaption_t > m_AsyncCaptions;
	bool		m_bLocked;
	bool		m_bVisibleDueToDirect;
	bool		m_bPaintDebugInfo;
	CUtlSymbol	m_CurrentLanguage;

	CUtlDict< Color, int >	m_ColorMap;
	bool		m_bLevelShutDown;

	bool		m_bUseAsianWordWrapping;
};

#endif // HUD_CLOSECAPTION_H
