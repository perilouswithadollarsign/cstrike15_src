//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef ILOCALIZE_H
#define ILOCALIZE_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/iappsystem.h"
#include <tier1/keyvalues.h>

// unicode character type
// for more unicode manipulation functions #include <wchar.h>
#if !defined( _WCHAR_T_DEFINED ) && !defined( _PS3 ) && !defined(__clang__)
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif


//-----------------------------------------------------------------------------
// Interface used to query text size so we can choose the longest one
//-----------------------------------------------------------------------------
abstract_class ILocalizeTextQuery
{
public:
	virtual int ComputeTextWidth( const wchar_t *pString ) = 0;
};


//-----------------------------------------------------------------------------
// Callback which is triggered when any localization string changes
// Is not called when a localization string is added
//-----------------------------------------------------------------------------
abstract_class ILocalizationChangeCallback
{
public:
	virtual void OnLocalizationChanged() = 0;
};


//-----------------------------------------------------------------------------
// Purpose: Handles localization of text
//			looks up string names and returns the localized unicode text
//-----------------------------------------------------------------------------
// direct references to localized strings
typedef uint32 LocalizeStringIndex_t;
const uint32 LOCALIZE_INVALID_STRING_INDEX = (LocalizeStringIndex_t)-1;

abstract_class ILocalize : public IAppSystem
{
public:
	// adds the contents of a file to the localization table
	virtual bool AddFile( const char *fileName, const char *pPathID = NULL, bool bIncludeFallbackSearchPaths = false ) = 0;

	// Remove all strings from the table
	virtual void RemoveAll() = 0;

	// Finds the localized text for tokenName. Returns NULL if none is found.
	virtual wchar_t *Find(const char *tokenName) = 0;

	// Like Find(), but as a failsafe, returns an error message instead of NULL if the string isn't found.  
	virtual const wchar_t *FindSafe(const char *tokenName) = 0;

	// converts an english string to unicode
	// returns the number of wchar_t in resulting string, including null terminator
	virtual int ConvertANSIToUnicode(const char *ansi, OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicode, int unicodeBufferSizeInBytes) = 0;

	// converts an unicode string to an english string
	// unrepresentable characters are converted to system default
	// returns the number of characters in resulting string, including null terminator
	virtual int ConvertUnicodeToANSI(const wchar_t *unicode, OUT_Z_CAP(ansiBufferSize) char *ansi, int ansiBufferSize) = 0;

	// finds the index of a token by token name, INVALID_STRING_INDEX if not found
	virtual LocalizeStringIndex_t FindIndex(const char *tokenName) = 0;

	// gets the values by the string index
	virtual const char *GetNameByIndex(LocalizeStringIndex_t index) = 0;
	virtual wchar_t *GetValueByIndex(LocalizeStringIndex_t index) = 0;

	///////////////////////////////////////////////////////////////////
	// the following functions should only be used by localization editors

	// iteration functions
	virtual LocalizeStringIndex_t GetFirstStringIndex() = 0;
	// returns the next index, or INVALID_STRING_INDEX if no more strings available
	virtual LocalizeStringIndex_t GetNextStringIndex(LocalizeStringIndex_t index) = 0;

	// adds a single name/unicode string pair to the table
	virtual void AddString( const char *tokenName, wchar_t *unicodeString, const char *fileName ) = 0;

	// changes the value of a string
	virtual void SetValueByIndex(LocalizeStringIndex_t index, wchar_t *newValue) = 0;

	// saves the entire contents of the token tree to the file
	virtual bool SaveToFile( const char *fileName ) = 0;

	// iterates the filenames
	virtual int GetLocalizationFileCount() = 0;
	virtual const char *GetLocalizationFileName(int index) = 0;

	// returns the name of the file the specified localized string is stored in
	virtual const char *GetFileNameByIndex(LocalizeStringIndex_t index) = 0;

	// for development only, reloads localization files
	virtual void ReloadLocalizationFiles( ) = 0;

	// need to replace the existing ConstructString with this
	virtual void ConstructString(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *localizationVariables) = 0;
	virtual void ConstructString(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *localizationVariables) = 0;

	// Used to install a callback to query which localized strings are the longest
	virtual void SetTextQuery( ILocalizeTextQuery *pQuery ) = 0;

	// Is called when any localization strings change
	virtual void InstallChangeCallback( ILocalizationChangeCallback *pCallback ) = 0;
	virtual void RemoveChangeCallback( ILocalizationChangeCallback *pCallback ) = 0;

	virtual wchar_t* GetAsianFrequencySequence( const char * pLanguage ) = 0;

	virtual const char *FindAsUTF8( const char *pchTokenName ) = 0;

	// builds a localized formatted string
	// uses the format strings first: %s1, %s2, ...  unicode strings (wchar_t *)
	template < typename T >
	void ConstructString(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) T *unicodeOuput, int unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, ...)
	{
		va_list argList;
		va_start( argList, numFormatParameters );

		ConstructStringVArgsInternal( unicodeOuput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );

		va_end( argList );
	}

	template < typename T >
	void ConstructStringVArgs(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) T *unicodeOuput, int unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, va_list argList)
	{
		ConstructStringVArgsInternal( unicodeOuput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
	}

	template < typename T >
	void ConstructString(OUT_Z_BYTECAP(unicodeBufferSizeInBytes) T *unicodeOutput, int unicodeBufferSizeInBytes, const T *formatString, KeyValues *localizationVariables)
	{
		ConstructStringKeyValuesInternal( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
	}

protected:
	// internal "interface"
	virtual void ConstructStringVArgsInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, va_list argList) = 0;
	virtual void ConstructStringVArgsInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, va_list argList) = 0;

	virtual void ConstructStringKeyValuesInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables) = 0;
	virtual void ConstructStringKeyValuesInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables) = 0;
};


#endif // ILOCALIZE_H
