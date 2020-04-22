//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMETEXT_H
#define GAMETEXT_H

#ifdef _WIN32
#pragma once
#endif


#include "vgui_surfacelib/ifontsurface.h"
#include "vgui/ilocalize.h"
#include "gamegraphic.h"
#include "dmxloader/dmxelement.h"
#include "animdata.h"
#include "gameuisystemmgr.h"


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CGameText : public CGameGraphic
{
	DECLARE_DMXELEMENT_UNPACK()

public:

	CGameText( const char *pName );
	virtual ~CGameText();


	bool Unserialize( CDmxElement *pGraphic );

	virtual KeyValues * HandleScriptCommand( KeyValues *args );

	void SetFont( const char *pFontName );
	void SetText( const char *text );
	
	// where text should go relative to center position.
	enum Justification_e
	{
		JUSTIFICATION_LEFT,
		JUSTIFICATION_CENTER,
		JUSTIFICATION_RIGHT,
	};
	void SetJustification( Justification_e justify );

	void GetTextSize( int &wide, int &tall );

	// from CGameGraphic
	virtual void UpdateGeometry();
	virtual void UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex );
	virtual void DrawExtents( CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex );

	virtual void SetColor( color32 c );	

	virtual bool HitTest( int x, int y );

private:
	CGameText();

	void SetText( const wchar_t *unicode, bool bClearUnlocalizedSymbol = false );

	void GetStartingTextPosition( int &x, int &y );
	CRenderGeometry *GetGeometryEntry( CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex, int fontTextureID );
	void SetupVertexColors();

	int GetTextRenderWidth();
	int GetTextRenderHeight();

	void SetFont( FontHandle_t font );
	FontHandle_t GetFont();

	void GetRenderInfo( CUtlVector< FontCharRenderInfo > &renderInfoList );

	wchar_t *m_UnicodeText;	// unicode version of the text	
	short m_TextBufferLen;	// size of the text buffer
	CUtlString m_CharText;	// unlocalized version of the text
	vgui::StringIndex_t m_UnlocalizedTextSymbol;

	CUtlString m_FontName;
	FontHandle_t m_Font;

	bool m_bAllCaps;
	int m_Justification;

};


#endif // GAMETEXT_H
