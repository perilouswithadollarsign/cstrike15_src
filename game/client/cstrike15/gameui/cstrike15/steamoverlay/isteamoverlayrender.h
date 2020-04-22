//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//

#ifndef ISTEAMOVERLAYRENDER_H
#define ISTEAMOVERLAYRENDER_H

/*
	Interfaces required for Steam overlay rendering
*/

struct SteamOverlayRenderInfo_t
{
	uint32 m_nScreenWidth;				// screen width in pixels
	uint32 m_nScreenHeight;				// screen height in pixels
	float m_flScreenAspectRatio;		// screen aspect ratio
	float m_flRefreshRate;				// screen refresh ratio in Hz
};

struct SteamOverlayColor_t
{
	SteamOverlayColor_t() : m_r(255), m_g(255), m_b(255), m_a(255) {}
	SteamOverlayColor_t( int r, int g, int b, int a ) : m_r( r ), m_g( g ), m_b( b ), m_a( a ) {}
	int m_r, m_g, m_b, m_a;
};

struct SteamOverlayRect_t
{
	SteamOverlayRect_t() : m_x0(0), m_y0(0), m_x1(0), m_y1(0) {}
	SteamOverlayRect_t( int x0, int y0, int x1, int y1 ) : m_x0(x0), m_y0(y0), m_x1(x1), m_y1(y1) {}
	int m_x0, m_y0, m_x1, m_y1;
};

typedef int SteamOverlayTextureHandle_t;
typedef uint32 SteamOverlayFontHandle_t;

class ISteamOverlayRenderHost
{
public:
	virtual void GetRenderInfo( SteamOverlayRenderInfo_t &info ) = 0;

	// Font Information
public:
	virtual SteamOverlayFontHandle_t FontGetHandle( char const *szFontName, bool bProportional = false ) = 0;
	virtual int  FontGetTall( SteamOverlayFontHandle_t hFont ) = 0;
	virtual int	 FontGetAscent( SteamOverlayFontHandle_t hFont, wchar_t wch ) = 0;
	virtual bool FontIsAdditive( SteamOverlayFontHandle_t hFont ) = 0;
	virtual void FontCharABCwide( SteamOverlayFontHandle_t hFont, wchar_t ch, int &a, int &b, int &c ) = 0;
	virtual int	 FontGetCharWidth( SteamOverlayFontHandle_t hFont, wchar_t ch ) = 0;
	virtual void FontGetTextSize( SteamOverlayFontHandle_t hFont, wchar_t const *text, int &wide, int &tall ) = 0;
	virtual void FontGetKernedCharWidth( SteamOverlayFontHandle_t hFont, wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &flabcA, float &abcC ) = 0;

	// Text Rendering
public:
	virtual void TextSetFont( SteamOverlayFontHandle_t hFont ) = 0;
	virtual void TextSetColor( SteamOverlayColor_t const &clr ) = 0;
	virtual void TextSetPos( int x, int y ) = 0;
	virtual void TextDrawStringW( wchar_t const *pStringW ) = 0;

	// Solid Rendering
public:
	virtual void FillSetColor( SteamOverlayColor_t const &clr ) = 0;
	virtual void FillRect( SteamOverlayRect_t const &rc ) = 0;
	virtual void FillRectFade( SteamOverlayRect_t const &rc, int a0, int a1, bool bHorizontalFade ) = 0;

	// Texture Rendering
public:
	virtual SteamOverlayTextureHandle_t TextureCreate() = 0;
	virtual void TextureSetRGBA( SteamOverlayTextureHandle_t hTexture, unsigned char const *pbRGBA, int nWidth, int nHeight ) = 0;
	virtual void TextureSetFile( SteamOverlayTextureHandle_t hTexture, char const *szFileName ) = 0;
	virtual void TextureBind( SteamOverlayTextureHandle_t hTexture ) = 0;
	virtual void TextureRect( SteamOverlayRect_t const &rc ) = 0;
	virtual void TextureGetSize( SteamOverlayTextureHandle_t hTexture, int &nWidth, int &nHeight ) = 0;
	virtual bool TextureIsValid( SteamOverlayTextureHandle_t hTexture ) = 0;
	virtual void TextureDestroy( SteamOverlayTextureHandle_t hTexture ) = 0;
};

class ISteamOverlayRender
{
public:
	virtual void Initialize( ISteamOverlayRenderHost *pSteamOverlayRenderHost ) = 0;
	virtual void Shutdown() = 0;

	virtual void Render() = 0;

	// Handle input event in the overlay.
	// Params:
	//		iCode = Joystick code including controller information
	//		iValue = 1 for button press, 0 for button release; or analog value for axis
	// Returns:
	//		true to keep the overlay active
	//		false to exit the overlay mode
	virtual bool HandleInputEvent( int iCode, int iValue ) = 0;
};


#endif // ISTEAMOVERLAYRENDER_H
