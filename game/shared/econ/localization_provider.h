//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: provide a layer of abstraction between GC and vgui localization systems
//
//=============================================================================

#ifndef LOCALIZATION_PROVIDER_H
#define LOCALIZATION_PROVIDER_H
#ifdef _WIN32
#pragma once
#endif

#include "language.h"

typedef wchar_t locchar_t;

//#define loc_snprintf	V_snwprintf
#define loc_sprintf_safe V_swprintf_safe
//#define loc_sncat		V_wcsncat
#define loc_scat_safe	V_wcscat_safe
#define loc_sncpy		Q_wcsncpy
#define loc_scpy_safe	V_wcscpy_safe
#define loc_strlen		Q_wcslen
#define loc_strchr		wcschr
#define LOCCHAR(x)		L ## x


// interface matches a subset of VGUI functions
class CLocalizationProvider
{
public:
	virtual locchar_t *Find( const char *pchKey ) = 0;

	virtual void ConstructString( OUT_Z_BYTECAP(unicodeBufferSizeInBytes) locchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const locchar_t *formatString, int numFormatParameters, ... ) = 0;
	virtual void ConstructString( OUT_Z_BYTECAP(unicodeBufferSizeInBytes) locchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const locchar_t *formatString, KeyValues *localizationVariables) = 0;

	virtual void ConvertLoccharToANSI	( const locchar_t *loc_In, CUtlConstString *out_ansi ) const = 0;
	virtual void ConvertLoccharToUnicode( const locchar_t *loc_In, CUtlConstWideString *out_unicode ) const = 0;
	virtual void ConvertUTF8ToLocchar	( const char *utf8_In, CUtlConstStringBase<locchar_t> *out_loc ) const = 0;

	virtual int ConvertLoccharToANSI( const locchar_t *loc, OUT_Z_CAP(ansiBufferSize) char *ansi, int ansiBufferSize ) = 0;
	virtual int ConvertLoccharToUnicode( const locchar_t *loc, OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicode, int unicodeBufferSizeInBytes ) = 0;
	virtual void ConvertUTF8ToLocchar( const char *utf8, OUT_Z_BYTECAP(cubDestSizeInBytes) locchar_t *locchar, int cubDestSizeInBytes ) = 0;

	virtual ELanguage GetELang() = 0;
};
CLocalizationProvider *GLocalizationProvider();

#include "vgui/ILocalize.h"
extern vgui::ILocalize				*g_pVGuiLocalize;

// Game localization is handled by vgui
class CVGUILocalizationProvider : public CLocalizationProvider
{
public:
	CVGUILocalizationProvider();

	virtual locchar_t *Find( const char *pchKey );

	virtual void ConstructString( OUT_Z_BYTECAP(unicodeBufferSizeInBytes) locchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const locchar_t *formatString, int numFormatParameters, ... );
	virtual void ConstructString( OUT_Z_BYTECAP(unicodeBufferSizeInBytes) locchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const locchar_t *formatString, KeyValues *localizationVariables );

	virtual void ConvertLoccharToANSI	( const locchar_t *loc_In, CUtlConstString *out_ansi ) const;
	virtual void ConvertLoccharToUnicode( const locchar_t *loc_In, CUtlConstWideString *out_unicode ) const;
	virtual void ConvertUTF8ToLocchar	( const char *utf8_In, CUtlConstStringBase<locchar_t> *out_loc ) const;

	virtual int ConvertLoccharToANSI( const locchar_t *loc, OUT_Z_CAP(ansiBufferSize) char *ansi, int ansiBufferSize );
	virtual int ConvertLoccharToUnicode( const locchar_t *loc, OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicode, int unicodeBufferSizeInBytes );
	virtual void ConvertUTF8ToLocchar( const char *utf8, OUT_Z_BYTECAP(cubDestSizeInBytes) locchar_t *locchar, int cubDestSizeInBytes );

	virtual ELanguage GetELang() { return k_Lang_None; }
};

// --------------------------------------------------------------------------
// Purpose: CLocalizedStringArg<> is a class that will take a variable of any
//			arbitary type and convert it to a string of whatever character type
//			we're using for localization (locchar_t).
//
//			Independently it isn't very useful, though it can be used to sort-of-
//			intelligently fill out the correct format string. It's designed to be
//			used for the arguments of CConstructLocalizedString, which can be of
//			arbitrary number and type.
//
//			If you pass in a (non-specialized) pointer, the code will assume that
//			you meant that pointer to be used as a localized string. This will
//			still fail to compile if some non-string type is passed in, but will
//			handle weird combinations of const/volatile/whatever automatically.
// --------------------------------------------------------------------------

// The base implementation doesn't do anything except fail to compile if you
// use it. Getting an "incomplete type" error here means that you tried to construct
// a localized string with a type that doesn't have a specialization.
template < typename T >
class CLocalizedStringArg;

// --------------------------------------------------------------------------

template < typename T >
class CLocalizedStringArgStringImpl
{
public:
	enum { kIsValid = true };

	CLocalizedStringArgStringImpl( const locchar_t *pStr ) : m_pStr( pStr ) { }

	const locchar_t *GetLocArg() const { return m_pStr; }

private:
	const locchar_t *m_pStr;
};

// --------------------------------------------------------------------------

template < typename T >
class CLocalizedStringArg<T *> : public CLocalizedStringArgStringImpl<T>
{
public:
	CLocalizedStringArg( const locchar_t *pStr ) : CLocalizedStringArgStringImpl<T>( pStr ) { }
};

// --------------------------------------------------------------------------

template < typename T >
class CLocalizedStringArgPrintfImpl
{
public:
	enum { kIsValid = true };

	CLocalizedStringArgPrintfImpl( T value, const locchar_t *loc_Format ) { loc_sprintf_safe( m_cBuffer, loc_Format, value ); }

	const locchar_t *GetLocArg() const { return m_cBuffer; }

private:
	enum { kBufferSize = 128, };
	locchar_t m_cBuffer[ kBufferSize ];
};

// --------------------------------------------------------------------------

template < >
class CLocalizedStringArg<uint32> : public CLocalizedStringArgPrintfImpl<uint32>
{
public:
	CLocalizedStringArg( uint32 unValue ) : CLocalizedStringArgPrintfImpl<uint32>( unValue, LOCCHAR("%u") ) { }
};

// --------------------------------------------------------------------------

template < >
class CLocalizedStringArg<float> : public CLocalizedStringArgPrintfImpl<float>
{
public:
	// Display one decimal point if we've got a value less than one, and no point
	// if we're greater.
	CLocalizedStringArg( float fValue )
		: CLocalizedStringArgPrintfImpl<float>( fValue,
		fabsf( fValue ) < 1.0f ? LOCCHAR("%.1f") : LOCCHAR("%.0f") )
	{
		//
	}
};

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
class CConstructLocalizedString
{
public:
	template < typename T >
	CConstructLocalizedString( CLocalizationProvider* pLocalizationProvider, const locchar_t *loc_Format, T arg0 )
	{
		Assert( CLocalizedStringArg<T>::kIsValid );

		m_loc_Buffer[0] = '\0';

		if ( loc_Format )
		{
			pLocalizationProvider->ConstructString( m_loc_Buffer, sizeof( m_loc_Buffer ), loc_Format, 1, CLocalizedStringArg<T>( arg0 ).GetLocArg() );
		}
	}

	template < typename T, typename U >
	CConstructLocalizedString( CLocalizationProvider* pLocalizationProvider, const locchar_t *loc_Format, T arg0, U arg1 )
	{
		Assert( CLocalizedStringArg<T>::kIsValid );
		Assert( CLocalizedStringArg<U>::kIsValid );

		m_loc_Buffer[0] = '\0';

		if ( loc_Format )
		{
			pLocalizationProvider->ConstructString( m_loc_Buffer, sizeof( m_loc_Buffer ), loc_Format, 2, CLocalizedStringArg<T>( arg0 ).GetLocArg(), CLocalizedStringArg<U>( arg1 ).GetLocArg() );
		}
	}

	template < typename T, typename U, typename V >
	CConstructLocalizedString( CLocalizationProvider* pLocalizationProvider, const locchar_t *loc_Format, T arg0, U arg1, V arg2 )
	{
		Assert( CLocalizedStringArg<T>::kIsValid );
		Assert( CLocalizedStringArg<U>::kIsValid );
		Assert( CLocalizedStringArg<V>::kIsValid );

		m_loc_Buffer[0] = '\0';

		if ( loc_Format )
		{
			pLocalizationProvider->ConstructString( m_loc_Buffer, sizeof( m_loc_Buffer ), loc_Format, 3,
				CLocalizedStringArg<T>( arg0 ).GetLocArg(), 
				CLocalizedStringArg<U>( arg1 ).GetLocArg(),
				CLocalizedStringArg<V>( arg2 ).GetLocArg() );
		}
	}

	template < typename T, typename U, typename V, typename W >
	CConstructLocalizedString( CLocalizationProvider* pLocalizationProvider, const locchar_t *loc_Format, T arg0, U arg1, V arg2, W arg3 )
	{
		Assert( CLocalizedStringArg<T>::kIsValid );
		Assert( CLocalizedStringArg<U>::kIsValid );
		Assert( CLocalizedStringArg<V>::kIsValid );
		Assert( CLocalizedStringArg<W>::kIsValid );

		m_loc_Buffer[0] = '\0';

		if ( loc_Format )
		{
			pLocalizationProvider->ConstructString( m_loc_Buffer,
				sizeof( m_loc_Buffer ),
				loc_Format,
				4,
				CLocalizedStringArg<T>( arg0 ).GetLocArg(),
				CLocalizedStringArg<U>( arg1 ).GetLocArg(),
				CLocalizedStringArg<V>( arg2 ).GetLocArg(),
				CLocalizedStringArg<W>( arg3 ).GetLocArg() );
		}
	}

	CConstructLocalizedString( CLocalizationProvider* pLocalizationProvider, const locchar_t *loc_Format, KeyValues *pKeyValues )
	{
		m_loc_Buffer[0] = '\0';

		if ( loc_Format && pKeyValues )
		{
			pLocalizationProvider->ConstructString( m_loc_Buffer, sizeof( m_loc_Buffer ), loc_Format, pKeyValues );
		}
	}

	operator const locchar_t *() const
	{
		return Get();
	}

	const locchar_t *Get() const
	{
		return m_loc_Buffer;
	}

private:
	enum { kBufferSize = 512, };
	locchar_t m_loc_Buffer[ kBufferSize ];
};

#endif // LOCALIZATION_PROVIDER_H
