//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======
//
// Baked Bitmap fonts
//
//===========================================================================

#ifndef _BITMAPFONT_H_
#define _BITMAPFONT_H_

#include "vguifont.h"
#include "BitmapFontFile.h"

class ITexture;

//-----------------------------------------------------------------------------
// Purpose: encapsulates a windows font
//-----------------------------------------------------------------------------
class CBitmapFont : public font_t
{
public:
	CBitmapFont();
	~CBitmapFont();

	// creates the font.  returns false if the compiled font does not exist.
	virtual bool Create(const char *windowsFontName, float scalex, float scaley, int flags);

	// returns true if the font is equivalent to that specified
	virtual bool IsEqualTo(const char *windowsFontName, float scalex, float scaley, int flags);

	// gets the abc widths for a character
	virtual void GetCharABCWidths(int ch, int &a, int &b, int &c);
	virtual void GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC );

	// gets the texture coords in the compiled texture page
	void GetCharCoords( int ch, float *left, float *top, float *right, float *bottom );

	// sets the scale of the font.
	void SetScale( float sx, float sy );

	// gets the compiled texture page
	ITexture *GetTexturePage();

private:
	int				m_bitmapFontHandle;
	float			m_scalex;
	float			m_scaley;
};

#endif	// _BITMAPFONT_H
