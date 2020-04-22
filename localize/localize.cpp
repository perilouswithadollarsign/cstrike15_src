//======= Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//


#pragma warning( disable: 4018 ) // '==' : signed/unsigned mismatch in rbtree
#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <vadefs.h>
#elif defined( _PS3 )


#elif defined( POSIX )
#include <iconv.h>
#endif

#include <wchar.h>

#include "filesystem.h"

#include "localize/ilocalize.h"
#include "tier1/utlvector.h"
#include "tier1/utlrbtree.h"
#include "tier1/utlsymbol.h"
#include "tier1/utlstring.h"
#include "UnicodeFileHelpers.h"
#include "tier0/icommandline.h"
#include "byteswap.h"
#include "exprevaluator.h"
#include "iregistry.h"
#include <vstdlib/vstrtools.h>
#include "vgui/ISystem.h"
#include "vgui_controls/Controls.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_LOCALIZED_CHARS	4096


//-----------------------------------------------------------------------------
// 
// Internal implementation
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Purpose: Maps token names to localized unicode strings
//-----------------------------------------------------------------------------
class CLocalize : public CTier2AppSystem< ILocalize >
{
	typedef CTier2AppSystem< ILocalize > BaseClass;

	// Methods of IAppSystem
public:
	virtual InitReturnVal_t Init();

	// ILocalize overrides
public:
	virtual bool AddFile( const char *fileName, const char *pPathID, bool bIncludeFallbackSearchPaths );
	virtual void RemoveAll();
	virtual wchar_t *Find(const char *pName);
	virtual const wchar_t *FindSafe(const char *tokenName);
	virtual int ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSizeInBytes);
	virtual int ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize);
	virtual LocalizeStringIndex_t FindIndex(const char *pName);
	virtual const char *GetNameByIndex(LocalizeStringIndex_t index);
	virtual wchar_t *GetValueByIndex(LocalizeStringIndex_t index);
	virtual LocalizeStringIndex_t GetFirstStringIndex();
	virtual LocalizeStringIndex_t GetNextStringIndex(LocalizeStringIndex_t index);
	virtual void AddString(const char *tokenName, wchar_t *unicodeString, const char *fileName);
	virtual void SetValueByIndex(LocalizeStringIndex_t index, wchar_t *newValue);
	virtual bool SaveToFile( const char *fileName );
	virtual int GetLocalizationFileCount();
	virtual const char *GetLocalizationFileName(int index);
	virtual const char *GetFileNameByIndex(LocalizeStringIndex_t index);
	virtual void ReloadLocalizationFiles( );
	virtual void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *dialogVariables);
	virtual void ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *dialogVariables);
	virtual void SetTextQuery( ILocalizeTextQuery *pQuery );
	virtual void InstallChangeCallback( ILocalizationChangeCallback *pCallback );
	virtual void RemoveChangeCallback( ILocalizationChangeCallback *pCallback );
	virtual const char *FindAsUTF8( const char *pchTokenName );
	virtual wchar_t* GetAsianFrequencySequence( const char * pLanguage );

protected:
	// internal "interface"
	virtual void ConstructStringVArgsInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, va_list argList);
	virtual void ConstructStringVArgsInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, va_list argList);

	virtual void ConstructStringKeyValuesInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables);
	virtual void ConstructStringKeyValuesInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables);

	// Other public methods
public:
	CLocalize();
	virtual ~CLocalize();

	// returns whether a file has already been loaded
	bool LocalizationFileIsLoaded( const char *name );

private:
	struct localizedstring_t
	{
		LocalizeStringIndex_t nameIndex;
		// nameIndex == LOCALIZE_INVALID_STRING_INDEX is used only for searches and implies
		// that pszValueString will be used from union fields.
		union
		{
			LocalizeStringIndex_t valueIndex;		// Used when nameIndex != LOCALIZE_INVALID_STRING_INDEX
			const char * pszValueString;	// Used only if nameIndex == LOCALIZE_INVALID_STRING_INDEX
		};
		CUtlSymbol filename;
	};

	struct LocalizationFileInfo_t
	{
		CUtlSymbol	symName;
		CUtlSymbol	symPathID;
		bool		bIncludeFallbacks;

		static bool LessFunc( const LocalizationFileInfo_t& lhs, const LocalizationFileInfo_t& rhs )
		{
			int iresult = Q_stricmp( lhs.symPathID.String(), rhs.symPathID.String() );
			if ( iresult != 0 )
			{
				return iresult == -1;
			}

			return Q_stricmp( lhs.symName.String(), rhs.symName.String() ) < 0;
		}
	};

	struct fastvalue_t
	{
		int				valueindex;
		const wchar_t	*search;
		static CLocalize	*s_pTable;
	};

private:
	bool AddAllLanguageFiles( const char *baseFileName );
	void BuildFastValueLookup();
	void DiscardFastValueLookup();
	int FindExistingValueIndex( const wchar_t *value );
	bool ReadLocalizationFile( const char *pRelativePath, const char *pPathID );
	void InvokeChangeCallbacks( );
	virtual int ConvertANSIToUCS2(const char *ansi, OUT_Z_BYTECAP(unicodeBufferSizeInBytes) ucs2 *unicode, int unicodeBufferSizeInBytes);
	virtual int ConvertUCS2ToANSI(const ucs2 *unicode, OUT_Z_BYTECAP(ansiBufferSize) char *ansi, int ansiBufferSize);
#if defined ( POSIX ) && !defined( _PS3 )
	virtual void AddString(const char *tokenName, ucs2 *unicodeString, const char *fileName);
#endif
	char m_szLanguage[64];
	bool m_bUseOnlyLongestLanguageString;
	bool m_bSuppressChangeCallbacks;
	bool m_bQueuedChangeCallback;

	// Stores the symbol lookup
	CUtlRBTree<localizedstring_t, LocalizeStringIndex_t> m_Lookup;
	
	// stores the string data
	CUtlVector<char> m_Names;
	CUtlVector<wchar_t> m_Values;
	CUtlSymbol m_CurrentFile;
	CUtlVector< LocalizationFileInfo_t > m_LocalizationFiles;
	CUtlRBTree< fastvalue_t, int >	m_FastValueLookup;
	ILocalizeTextQuery *m_pQuery;
	static CLocalize *s_pTable;
	CUtlVector< ILocalizationChangeCallback* > m_ChangeCallbacks;

	CUtlBuffer m_bufAsianFrequencySequence;
	bool m_bAsianFrequencySequenceLoaded;

	// Less function, for sorting strings
	static bool SymLess( localizedstring_t const& i1, localizedstring_t const& i2 );
	static bool FastValueLessFunc( const fastvalue_t& lhs, const fastvalue_t& rhs );
};

// global instance of table
static CLocalize s_Localize;

// expose the interface
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CLocalize, ILocalize, LOCALIZE_INTERFACE_VERSION, s_Localize);


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLocalize::CLocalize() : 
	m_Lookup( 0, 0, SymLess ), m_Names( 1024 ), m_Values( 2048 ), m_FastValueLookup( 0, 0, FastValueLessFunc )
{
	m_bUseOnlyLongestLanguageString = false;
	m_bSuppressChangeCallbacks = false;
	m_bQueuedChangeCallback = false;
	m_pQuery = NULL;
	m_bAsianFrequencySequenceLoaded = false;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLocalize::~CLocalize()
{
	m_Names.Purge();
	m_Values.Purge();
	m_LocalizationFiles.Purge();
}


//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
InitReturnVal_t CLocalize::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	m_bUseOnlyLongestLanguageString = ( CommandLine()->FindParm("-all_languages") > 0 );
	return INIT_OK;
}


//-----------------------------------------------------------------------------
// Sets the callback used to check length of a localization string
//-----------------------------------------------------------------------------
void CLocalize::SetTextQuery( ILocalizeTextQuery *pQuery )
{
	m_pQuery = pQuery;
}


//-----------------------------------------------------------------------------
// Add, remove, invoke localization string change callbacks
//-----------------------------------------------------------------------------
void CLocalize::InstallChangeCallback( ILocalizationChangeCallback *pCallback )
{
	if ( m_ChangeCallbacks.Find( pCallback ) != m_ChangeCallbacks.InvalidIndex() )
	{
		Warning( "CLocalize::InstallChangeCallback: Attempted to add the same callback twice!\n" );
		return;
	}

	m_ChangeCallbacks.AddToTail( pCallback );
}

void CLocalize::RemoveChangeCallback( ILocalizationChangeCallback *pCallback )
{
	m_ChangeCallbacks.FindAndRemove( pCallback );
}


//-----------------------------------------------------------------------------
// Purpose: Finds a string in the table
//-----------------------------------------------------------------------------
const char *CLocalize::FindAsUTF8( const char *pchTokenName )
{
	wchar_t *pwch = Find( pchTokenName );
	if ( !pwch )
		return pchTokenName;

	static char rgchT[2048];
	Q_UnicodeToUTF8( pwch, rgchT, sizeof( rgchT ) );
	return rgchT;
}


void CLocalize::InvokeChangeCallbacks( )
{
	// This is to prevent a ton of change callbacks while loading using -all_languages
	if ( m_bSuppressChangeCallbacks )
	{
		m_bQueuedChangeCallback = true;
		return;
	}

	int nCount = m_ChangeCallbacks.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_ChangeCallbacks[i]->OnLocalizationChanged();
	}
}


int DistanceToEndOfLine( ucs2 *start )
{
	int nResult = 0;

	if ( !*start )
	{
		return nResult;
	}

	while ( *start )
	{
		if ( *start == 0x0D || *start== 0x0A )
		{
			break;
		}

		start++;
		nResult++;
	}

	while ( *start == 0x0D || *start== 0x0A )
	{
		start++;
		nResult++;
	}

	return nResult;
}

//-----------------------------------------------------------------------------
// Purpose:Reads the contents of a file
//-----------------------------------------------------------------------------
bool CLocalize::ReadLocalizationFile( const char *pRelativePath, const char *pPathID )
{
	FileHandle_t file = g_pFullFileSystem->Open( pRelativePath, "rb", pPathID );
	if ( FILESYSTEM_INVALID_HANDLE == file )
		return false;

	// this is an optimization so that the filename string doesn't have to get converted to a symbol for each key/value
	m_CurrentFile = pRelativePath;

	// read into a memory block
	int fileSize = g_pFullFileSystem->Size(file);
	int bufferSize = g_pFullFileSystem->GetOptimalReadSize( file, fileSize + sizeof(wchar_t) );
	ucs2 *memBlock = (ucs2 *)g_pFullFileSystem->AllocOptimalReadBuffer(file, bufferSize);
	bool bReadOK = ( g_pFullFileSystem->ReadEx(memBlock, bufferSize, fileSize, file) != 0 );

	// finished with file
	g_pFullFileSystem->Close(file);

	// null-terminate the stream
	memBlock[fileSize / sizeof(ucs2)] = 0x0000;

	// check the first character, make sure this a little-endian unicode file
	ucs2 *data = memBlock;
	ucs2 signature = LittleShort( data[0] );
	if ( !bReadOK || signature != 0xFEFF )
	{
		Msg( "Ignoring non-unicode close caption file %s\n", pRelativePath );
		g_pFullFileSystem->FreeOptimalReadBuffer( memBlock );
		m_CurrentFile = UTL_INVAL_SYMBOL;
		return false;
	}

	// ensure little-endian unicode reads correctly on all platforms
	CByteswap byteSwap;
	byteSwap.SetTargetBigEndian( false );
	byteSwap.SwapBufferToTargetEndian( data, data, fileSize / sizeof(ucs2) );

	// skip past signature
	data++;

	// parse out a token at a time
	enum states_e
	{
		STATE_BASE,		// looking for base settings
		STATE_TOKENS,	// reading in unicode tokens
	};

	bool bQuoted;
	bool bEnglishFile = false;
	if ( Q_stristr(pRelativePath, "_english.txt") )
	{
		bEnglishFile = true;
	}

	bool spew = false;
	if ( CommandLine()->FindParm( "-ccsyntax" ) )
	{
		spew = true;
	}

	BuildFastValueLookup();

	CExpressionEvaluator ExpressionHandler;

	states_e state = STATE_BASE;
	while (1)
	{
		// read the key and the value
		ucs2 keytoken[128];
		data = ReadUnicodeToken(data, keytoken, 128, bQuoted);
		if (!keytoken[0])
			break;	// we've hit the null terminator

		// convert the token to a string
		char key[128];
		ConvertUCS2ToANSI(keytoken, key, sizeof(key));

		// if we have a C++ style comment, read to end of line and continue
		if (!strnicmp(key, "//", 2))
		{
			data = ReadToEndOfLine(data);
			continue;
		}

		if ( spew )
		{
			Msg( "%s\n", key );
		}

		ucs2 valuetoken[ MAX_LOCALIZED_CHARS ];

		bool bEnoughCapacity = true;

		if ( DistanceToEndOfLine( data ) > ( MAX_LOCALIZED_CHARS - 1 ) )
		{
			Warning( "Error: Localization key value exceeds MAX_LOCALIZED_CHARS. Problem key: %s\n", key );
			bEnoughCapacity = false;
		}

		data = ReadUnicodeToken(data, valuetoken, MAX_LOCALIZED_CHARS, bQuoted);
		if (!valuetoken[0] && !bQuoted)
			break;	// we've hit the null terminator

		if (state == STATE_BASE)
		{
			if (!stricmp(key, "Language"))
			{
				// copy out our language setting
				char value[MAX_LOCALIZED_CHARS];
				ConvertUCS2ToANSI(valuetoken, value, sizeof(value));
				strncpy(m_szLanguage, value, sizeof(m_szLanguage) - 1);
			}
			else if (!stricmp(key, "Tokens"))
			{
				state = STATE_TOKENS;
			}
			else if (!stricmp(key, "}"))
			{
				// we've hit the end
				break;
			}
		}
		else if (state == STATE_TOKENS)
		{
			if (!stricmp(key, "}"))
			{
				// end of tokens
				state = STATE_BASE;
			}
			else
			{
				// skip our [english] beginnings (in non-english files)
				if ( (bEnglishFile) || (!bEnglishFile && strnicmp(key, "[english]", 9)))
				{
					// Check for a conditional tag
					bool bAccepted = true;
					ucs2 conditional[ MAX_LOCALIZED_CHARS ];
					ucs2 *tempData = ReadUnicodeToken(data, conditional, MAX_LOCALIZED_CHARS, bQuoted);
					char cond[MAX_LOCALIZED_CHARS];
 					V_UCS2ToUTF8( conditional, cond, sizeof(cond) );
					if ( !bQuoted && (strstr( cond, "[$" )||strstr( cond, "[!$" )) )
					{
						// Evaluate the conditional tag
						char cond[MAX_LOCALIZED_CHARS];
						ConvertUCS2ToANSI( conditional, cond, sizeof( cond ) );
						ExpressionHandler.Evaluate( bAccepted, cond );
						data = tempData;
					}
					if ( bAccepted && bEnoughCapacity )
					{
						// add the string to the table
						AddString(key, valuetoken, NULL);
					}
				}
			}
		}
	}

	g_pFullFileSystem->FreeOptimalReadBuffer( memBlock );
	m_CurrentFile = UTL_INVAL_SYMBOL;
	DiscardFastValueLookup();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Adds the contents of a file 
//-----------------------------------------------------------------------------
bool CLocalize::AddFile( const char *szFileName, const char *pPathID, bool bIncludeFallbackSearchPaths )
{
	// use the correct file based on the chosen language
	static const char *const LANGUAGE_STRING = "%language%";
	static const char *const ENGLISH_STRING = "english";
	static const int MAX_LANGUAGE_NAME_LENGTH = 64;
	int offs = 0;
	bool success = false;

	char language[MAX_LANGUAGE_NAME_LENGTH];
	memset( language, 0, sizeof(language) );

	if ( Q_IsAbsolutePath( szFileName ) )
	{
		Warning( "Full paths not allowed in localization file specificaton %s\n", szFileName );
		return false;
	}

	const char *langptr = strstr(szFileName, LANGUAGE_STRING);
	if (langptr)
	{
		// LOAD THE ENGLISH FILE FIRST
		// always load the file to make sure we're not missing any strings
		// copy out the initial part of the string
		offs = langptr - szFileName;
		char fileName[MAX_PATH];
		strncpy(fileName, szFileName, offs);
		fileName[offs] = 0;

		if ( m_bUseOnlyLongestLanguageString )
		{
			return AddAllLanguageFiles( fileName );
		}

		// append "english" as our default language
		Q_strncat(fileName, ENGLISH_STRING, sizeof( fileName ), COPY_ALL_CHARACTERS );

		// append the end of the initial string
		offs += strlen(LANGUAGE_STRING);
		Q_strncat(fileName, szFileName + offs, sizeof( fileName ), COPY_ALL_CHARACTERS);

		success = AddFile( fileName, pPathID, bIncludeFallbackSearchPaths );

		bool bValid = true;
		if ( IsPC() )
		{
			if ( CommandLine()->CheckParm( "-language" ) )
			{
				Q_strncpy( language, CommandLine()->ParmValue( "-language", "english" ), sizeof( language ) );
				bValid = true;
			}
			else
			{
				bValid = vgui::system()->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", language, sizeof(language)-1 );
			}
			if ( bValid && !Q_stricmp( language, "unknown" ) )
			{
				// Fall back to english
				bValid = false;
			}
		}
		else
		{
#ifdef _GAMECONSOLE
			Q_strncpy( language, XBX_GetLanguageString(), sizeof( language ) );
#endif
		}

		// LOAD THE LOCALIZED FILE IF IT'S NOT ENGLISH
		// append the language
		if ( bValid )
		{
			if ( strlen(language) != 0 && stricmp(language, ENGLISH_STRING) != 0 )
			{
				// copy out the initial part of the string
				offs = langptr - szFileName;
				strncpy(fileName, szFileName, offs);
				fileName[offs] = 0;

				Q_strncat(fileName, language, sizeof( fileName ), COPY_ALL_CHARACTERS);

				// append the end of the initial string
				offs += strlen(LANGUAGE_STRING);
				Q_strncat(fileName, szFileName + offs, sizeof( fileName ), COPY_ALL_CHARACTERS );

				success &= AddFile( fileName, pPathID, bIncludeFallbackSearchPaths );
			}
		}
		return success;
	}

	// store the localization file name if it doesn't already exist
	LocalizationFileInfo_t search;
	search.symName = szFileName;
	search.symPathID = pPathID ? pPathID : "";
	search.bIncludeFallbacks = false;

	int lfc = m_LocalizationFiles.Count();
	for ( int lf = 0; lf < lfc; ++lf )
	{
		LocalizationFileInfo_t& entry = m_LocalizationFiles[ lf ];
		if ( !Q_stricmp( entry.symName.String(), szFileName ) )
		{
			m_LocalizationFiles.Remove( lf );
			break;
		}
	}

	m_LocalizationFiles.AddToTail( search );

	bool bOk = ReadLocalizationFile( szFileName, pPathID );
	if ( !bOk )
	{
		DevWarning( "ILocalize::AddFile() failed to load file \"%s\".\n", szFileName );
	}

	return bOk;
}

//-----------------------------------------------------------------------------
// Purpose: Load all the localized language strings, and uses the longest string from each language
//-----------------------------------------------------------------------------
bool CLocalize::AddAllLanguageFiles( const char *baseFileName )
{
	bool bSuccess = true;

	// Each new language load could potentially change the string value
	// This will suppress callbacks until we're done.
	m_bSuppressChangeCallbacks = true;

	if ( IsX360() )
	{
#ifdef _X360
		// xbox cannot support FindFirst/FindNext due to zips
		const char *pLanguageString = NULL;
		while ( 1 )
		{
			pLanguageString = XBX_GetNextSupportedLanguage( pLanguageString, NULL );
			if ( !pLanguageString )
			{
				// end of list
				break;
			}

			// re-add in the search path
			char szFile[MAX_PATH];
			V_snprintf( szFile, sizeof( szFile ), "%s%s.txt", baseFileName, pLanguageString );

			// add the file
			bSuccess &= AddFile( szFile, NULL, true );
		}
#endif
	}
	else
	{
		// work out the path the files are in
		char szFilePath[MAX_PATH];
		Q_strncpy( szFilePath, baseFileName, sizeof(szFilePath) );
		char *pLastSlash = strrchr( szFilePath, '\\' );
		if ( !pLastSlash )
		{
			pLastSlash = strrchr( szFilePath, '/' );
		}
		if ( pLastSlash )
		{
			pLastSlash[1] = 0;
		}
		else
		{
			szFilePath[0] = 0;
		}

		// iterate through and add all the languages (for development)
		// the longest string out of all the languages will be used
		char szSearchPath[MAX_PATH];
		Q_snprintf( szSearchPath, sizeof(szSearchPath), "%s*.txt", baseFileName );

		FileFindHandle_t hFind = NULL;
		const char *file = g_pFullFileSystem->FindFirst( szSearchPath, &hFind );
		while ( file )
		{
			// re-add in the search path
			char szFile[MAX_PATH];
			V_snprintf( szFile, sizeof(szFile), "%s%s", szFilePath, file );

			// add the file
			bSuccess &= AddFile( szFile, NULL, true );

			// next file
			file = g_pFullFileSystem->FindNext( hFind );
		}
		g_pFullFileSystem->FindClose( hFind );
	}

	m_bSuppressChangeCallbacks = false;
	if ( m_bQueuedChangeCallback )
	{
		m_bQueuedChangeCallback = false;
		InvokeChangeCallbacks();
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: saves the entire contents of the token tree to the file
//-----------------------------------------------------------------------------
bool CLocalize::SaveToFile( const char *szFileName )
{
	// parse out the file
	FileHandle_t file = g_pFullFileSystem->Open(szFileName, "wb");
	if (!file)
		return false;

	// only save the symbols relevant to this file
	CUtlSymbol fileName = szFileName;

	// write litte-endian unicode marker
	unsigned short marker = 0xFEFF;
	marker = LittleShort( marker );
	g_pFullFileSystem->Write(&marker, sizeof( marker ), file);

	const char *startStr = "\"lang\"\r\n{\r\n\"Language\" \"English\"\r\n\"Tokens\"\r\n{\r\n";
	const char *endStr = "}\r\n}\r\n";

	// write out the first string
	static ucs2 unicodeString[1024];
	int strLength = ConvertANSIToUCS2(startStr, unicodeString, sizeof(unicodeString));
	if (!strLength)
		return false;

	g_pFullFileSystem->Write(unicodeString, strlen(startStr) * sizeof(ucs2), file);

	// convert our spacing characters to unicode
//	wchar_t unicodeSpace = L' '; 
	ucs2 unicodeQuote = L'\"'; 
	ucs2 unicodeCR = L'\r'; 
	ucs2 unicodeNewline = L'\n'; 
	ucs2 unicodeTab = L'\t';

	// write out all the key/value pairs
	for (LocalizeStringIndex_t idx = GetFirstStringIndex(); idx != LOCALIZE_INVALID_STRING_INDEX; idx = GetNextStringIndex(idx))
	{
		// only write strings that belong in this file
		if (fileName != m_Lookup[idx].filename)
			continue;

		const char *name = GetNameByIndex(idx);
		wchar_t *value = GetValueByIndex(idx);

		// convert the name to a unicode string
		ConvertANSIToUCS2(name, unicodeString, sizeof(unicodeString));

		g_pFullFileSystem->Write(&unicodeTab, sizeof(ucs2), file);

		// write out
		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);
		g_pFullFileSystem->Write(unicodeString, strlen(name) * sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);

		g_pFullFileSystem->Write(&unicodeTab, sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeTab, sizeof(ucs2), file);

		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);
#ifdef POSIX
		ucs2 ucs2Value[MAX_LOCALIZED_CHARS];
		V_UnicodeToUCS2( value, wcslen(value)*sizeof(wchar_t), (char *)ucs2Value, sizeof(ucs2Value) );
		g_pFullFileSystem->Write(ucs2Value, wcslen(value) * sizeof(ucs2), file);
#else
		g_pFullFileSystem->Write(value, wcslen(value) * sizeof(ucs2), file);
#endif
		g_pFullFileSystem->Write(&unicodeQuote, sizeof(ucs2), file);

		g_pFullFileSystem->Write(&unicodeCR, sizeof(ucs2), file);
		g_pFullFileSystem->Write(&unicodeNewline, sizeof(ucs2), file);
	}

	// write end string
	strLength = ConvertANSIToUCS2(endStr, unicodeString, sizeof(unicodeString));
	g_pFullFileSystem->Write(unicodeString, strLength * sizeof(ucs2), file);

	g_pFullFileSystem->Close(file);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: for development, reloads localization files
//-----------------------------------------------------------------------------
void CLocalize::ReloadLocalizationFiles( )
{
	// re-add all the localization files
	for (int i = 0; i < m_LocalizationFiles.Count(); i++)
	{
		LocalizationFileInfo_t& entry = m_LocalizationFiles[ i ];
		AddFile
		(
			entry.symName.String(), 
			entry.symPathID.String()[0] ? entry.symPathID.String() : NULL,
			entry.bIncludeFallbacks 
		);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Used to sort strings
//-----------------------------------------------------------------------------
bool CLocalize::SymLess(localizedstring_t const &i1, localizedstring_t const &i2)
{
	const char *str1 = (i1.nameIndex == LOCALIZE_INVALID_STRING_INDEX) ? i1.pszValueString :
											&s_Localize.m_Names[i1.nameIndex];
	const char *str2 = (i2.nameIndex == LOCALIZE_INVALID_STRING_INDEX) ? i2.pszValueString :
											&s_Localize.m_Names[i2.nameIndex];
	
	return stricmp(str1, str2) < 0;
}


//-----------------------------------------------------------------------------
// Purpose: Finds a string in the table
//-----------------------------------------------------------------------------
wchar_t *CLocalize::Find(const char *pName)
{	
	LocalizeStringIndex_t idx = FindIndex(pName);
	if (idx == LOCALIZE_INVALID_STRING_INDEX)
		return NULL;

	return &m_Values[m_Lookup[idx].valueIndex];
}


// Like Find(), but as a failsafe, returns an error message instead of NULL if the string isn't found.  
const wchar_t *CLocalize::FindSafe(const char *pName)
{
#ifdef _CERT
	const wchar_t *failsafe = L"";
#else
	const wchar_t *failsafe = L"#FIXME_LOCALIZATION_FAIL_MISSING_STRING";
#endif

	const wchar_t *locstr = Find( pName );

	if ( !locstr )
	{
		DevMsg( "CLocalize::FindSafe failed to localize: %s\n", pName );
		return failsafe;
	}
	else
	{
		return locstr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: finds the index of a token by token name
//-----------------------------------------------------------------------------
LocalizeStringIndex_t CLocalize::FindIndex(const char *pName)
{
	if (!pName)
		return LOCALIZE_INVALID_STRING_INDEX;

	// strip the pound character (which is used elsewhere to indicate that it's a string that should be translated)
	if (pName[0] == '#')
	{
		pName++;
	}
	
	// Passing this special invalid symbol makes the comparison function
	// use the string passed in the context
	localizedstring_t invalidItem;
	invalidItem.nameIndex = LOCALIZE_INVALID_STRING_INDEX;
	invalidItem.pszValueString = pName;
	return m_Lookup.Find( invalidItem );
}

#if defined( POSIX ) && !defined( _PS3 )
void CLocalize::AddString(const char *pString, ucs2 *pUCS2Value, const char *fileName)
{
	if (!pString || !pUCS2Value ) 
		return;
	wchar_t pValue[2048];
	V_UCS2ToUnicode( pUCS2Value, pValue, sizeof(pValue) );

	AddString( pString, pValue, fileName );
}
#endif

//-----------------------------------------------------------------------------
// Finds and/or creates a symbol based on the string
//-----------------------------------------------------------------------------
void CLocalize::AddString(const char *pString, wchar_t *pValue, const char *fileName)
{
	if (!pString) 
		return;

	MEM_ALLOC_CREDIT();

	// see if the value is already in our string table
	int valueIndex = FindExistingValueIndex( pValue );
	if ( valueIndex == LOCALIZE_INVALID_STRING_INDEX )
	{
		int len = wcslen( pValue ) + 1;
		valueIndex = m_Values.AddMultipleToTail( len );
		memcpy( &m_Values[valueIndex], pValue, len * sizeof(wchar_t) );
	}

	// see if the key is already in the table
	LocalizeStringIndex_t stridx = FindIndex( pString );
	localizedstring_t item;
	item.nameIndex = stridx;

	if ( stridx == LOCALIZE_INVALID_STRING_INDEX )
	{
		// didn't find, insert the string into the vector.
		int len = strlen(pString) + 1;
		stridx = m_Names.AddMultipleToTail( len );
		memcpy( &m_Names[stridx], pString, len * sizeof(char) );

		item.nameIndex = stridx;
		item.valueIndex = valueIndex;
		item.filename = fileName ? fileName : m_CurrentFile;

		m_Lookup.Insert( item );
	}
	else
	{
		// it's already in the table
		if ( m_bUseOnlyLongestLanguageString )
		{
			// check which string is longer
			wchar_t *newValue = pValue;
			wchar_t *oldValue = GetValueByIndex( stridx );

			// get the width of the string, using just the first font
			if ( m_pQuery )
			{
				int newWide = m_pQuery->ComputeTextWidth( newValue );
				int oldWide = m_pQuery->ComputeTextWidth( oldValue );
				
				// if the new one is shorter, don't let it be added
				if (newWide < oldWide)
					return;
			}
		}

		// replace the current item
		item.nameIndex = GetNameByIndex( stridx ) - &m_Names[ 0 ];
		item.valueIndex = valueIndex;
		item.filename = fileName ? fileName : m_CurrentFile;
		m_Lookup[ stridx ] = item;

		InvokeChangeCallbacks();
	}
}

//-----------------------------------------------------------------------------
// Remove all symbols in the table.
//-----------------------------------------------------------------------------
void CLocalize::RemoveAll()
{
	m_Lookup.RemoveAll();
	m_Names.RemoveAll();
	m_Values.RemoveAll();
	m_LocalizationFiles.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: iteration functions
//-----------------------------------------------------------------------------
LocalizeStringIndex_t CLocalize::GetFirstStringIndex()
{
	return m_Lookup.FirstInorder();
}

//-----------------------------------------------------------------------------
// Purpose: returns the next index, or INVALID_STRING_INDEX if no more strings available
//-----------------------------------------------------------------------------
LocalizeStringIndex_t CLocalize::GetNextStringIndex(LocalizeStringIndex_t index)
{
	LocalizeStringIndex_t idx = m_Lookup.NextInorder(index);
	if (idx == m_Lookup.InvalidIndex())
		return LOCALIZE_INVALID_STRING_INDEX;
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: gets the name of the localization string by index
//-----------------------------------------------------------------------------
const char *CLocalize::GetNameByIndex(LocalizeStringIndex_t index)
{
	localizedstring_t &lstr = m_Lookup[index];
	return &m_Names[lstr.nameIndex];
}

//-----------------------------------------------------------------------------
// Purpose: gets the localized string value by index
//-----------------------------------------------------------------------------
wchar_t *CLocalize::GetValueByIndex(LocalizeStringIndex_t index)
{
	if (index == LOCALIZE_INVALID_STRING_INDEX)
		return NULL;

	localizedstring_t &lstr = m_Lookup[index];
	return &m_Values[lstr.valueIndex];
}


CLocalize *CLocalize::s_pTable = NULL;

bool CLocalize::FastValueLessFunc( const fastvalue_t& lhs, const fastvalue_t& rhs )
{
	Assert( s_pTable );

	const wchar_t *w1 = lhs.search ? lhs.search : &s_pTable->m_Values[ lhs.valueindex ];
	const wchar_t *w2 = rhs.search ? rhs.search : &s_pTable->m_Values[ rhs.valueindex ];

	return ( wcscmp( w1, w2 ) < 0 ) ? true : false;
}

void CLocalize::BuildFastValueLookup()
{
	m_FastValueLookup.RemoveAll();
	s_pTable = this;

	// Build it
	int c = m_Lookup.Count();
	for ( int i = 0; i < c; ++i )
	{
		fastvalue_t val;
		val.valueindex = m_Lookup[ i ].valueIndex;
		val.search = NULL;

		m_FastValueLookup.Insert( val );
	}
}

void CLocalize::DiscardFastValueLookup()
{
	m_FastValueLookup.RemoveAll();
	s_pTable = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CLocalize::FindExistingValueIndex( const wchar_t *value )
{
	if ( !s_pTable )
		return (int)LOCALIZE_INVALID_STRING_INDEX;

	fastvalue_t val;
	val.valueindex = -1;
	val.search = value;

	int idx = m_FastValueLookup.Find( val );
	if ( idx != m_FastValueLookup.InvalidIndex() )
	{
		return m_FastValueLookup[ idx ].valueindex;
	}
	return (int)LOCALIZE_INVALID_STRING_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: returns which file a string was loaded from
//-----------------------------------------------------------------------------
const char *CLocalize::GetFileNameByIndex(LocalizeStringIndex_t index)
{
	localizedstring_t &lstr = m_Lookup[index];
	return lstr.filename.String();
}

//-----------------------------------------------------------------------------
// Purpose: sets the value in the index
//-----------------------------------------------------------------------------
void CLocalize::SetValueByIndex(LocalizeStringIndex_t index, wchar_t *newValue)
{
	// get the existing string
	localizedstring_t &lstr = m_Lookup[index];
	wchar_t *wstr = &m_Values[lstr.valueIndex];

	// see if the new string will fit within the old memory
	int newLen = wcslen(newValue);
	int oldLen = wcslen(wstr);

	if (newLen > oldLen)
	{
		// it won't fit, so allocate new memory - this is wasteful, but only happens in edit mode
		lstr.valueIndex = m_Values.AddMultipleToTail(newLen + 1);
		memcpy(&m_Values[lstr.valueIndex], newValue, (newLen + 1) * sizeof(wchar_t));
	}
	else
	{
		// copy the string into the old position
		wcscpy(wstr, newValue);		
	}

	InvokeChangeCallbacks();
}

//-----------------------------------------------------------------------------
// Purpose: returns number of localization files currently loaded
//-----------------------------------------------------------------------------
int CLocalize::GetLocalizationFileCount()
{
	return m_LocalizationFiles.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns localization filename by index
//-----------------------------------------------------------------------------
const char *CLocalize::GetLocalizationFileName(int index)
{
	return m_LocalizationFiles[index].symName.String();
}

//-----------------------------------------------------------------------------
// Purpose: returns whether a localization file has been loaded already
//-----------------------------------------------------------------------------
bool CLocalize::LocalizationFileIsLoaded(const char *name)
{
	int c = m_LocalizationFiles.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( !Q_stricmp( m_LocalizationFiles[ i ].symName.String(), name ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: converts an english string to unicode
//-----------------------------------------------------------------------------
int CLocalize::ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSizeInBytes)
{
#ifdef POSIX
	return V_UTF8ToUnicode(ansi, unicode, unicodeBufferSizeInBytes);
#else
	int chars = ::MultiByteToWideChar(CP_UTF8, 0, ansi, -1, unicode, unicodeBufferSizeInBytes / sizeof(wchar_t));
	unicode[(unicodeBufferSizeInBytes / sizeof(wchar_t)) - 1] = 0;
	return chars;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: converts an unicode string to an english string
//-----------------------------------------------------------------------------
int CLocalize::ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize)
{
#ifdef POSIX
	return V_UnicodeToUTF8(unicode, ansi, ansiBufferSize);
#else
	int result = ::WideCharToMultiByte(CP_UTF8, 0, unicode, -1, ansi, ansiBufferSize, NULL, NULL);
	ansi[ansiBufferSize - 1] = 0;
	return result;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: converts an english string to unicode
//-----------------------------------------------------------------------------
int CLocalize::ConvertANSIToUCS2(const char *ansi, OUT_Z_BYTECAP(unicodeBufferSizeInBytes) ucs2 *unicode, int unicodeBufferSizeInBytes)
{
#ifdef POSIX
	return V_UTF8ToUCS2(ansi, strlen(ansi)*sizeof(char), unicode, unicodeBufferSizeInBytes);
#else
	int chars = ::MultiByteToWideChar(CP_UTF8, 0, ansi, -1, unicode, unicodeBufferSizeInBytes / sizeof(wchar_t));
	unicode[(unicodeBufferSizeInBytes / sizeof(wchar_t)) - 1] = 0;
	return chars;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: converts an unicode string to an english string
//-----------------------------------------------------------------------------
int CLocalize::ConvertUCS2ToANSI(const ucs2 *unicode, OUT_Z_BYTECAP(ansiBufferSize) char *ansi, int ansiBufferSize)
{
#ifdef POSIX
	return V_UCS2ToUTF8(unicode, ansi, ansiBufferSize);
#else
	int result = ::WideCharToMultiByte(CP_UTF8, 0, unicode, -1, ansi, ansiBufferSize, NULL, NULL);
	ansi[ansiBufferSize - 1] = 0;
	return result;
#endif
}



//-----------------------------------------------------------------------------
// Purpose: Constructs a string, inserting variables where necessary
//-----------------------------------------------------------------------------
void CLocalize::ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const char *tokenName, KeyValues *localizationVariables)
{
	LocalizeStringIndex_t index = FindIndex(tokenName);

	if (index != LOCALIZE_INVALID_STRING_INDEX)
	{
		ConstructString(unicodeOutput, unicodeBufferSizeInBytes, index, localizationVariables);
	}
	else
	{
		// string not found, just return the token name
		ConvertANSIToUnicode(tokenName, unicodeOutput, unicodeBufferSizeInBytes);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Constructs a string, inserting variables where necessary
//-----------------------------------------------------------------------------
void CLocalize::ConstructString(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, LocalizeStringIndex_t unlocalizedTextSymbol, KeyValues *localizationVariables)
{
	if (unicodeBufferSizeInBytes < 1)
		return;

	unicodeOutput[0] = 0;
	const wchar_t *searchPos = GetValueByIndex(unlocalizedTextSymbol);
	if (!searchPos)
	{
		wcsncpy(unicodeOutput, L"[unknown string]", unicodeBufferSizeInBytes / sizeof(wchar_t));
		return;
	}

	wchar_t *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(wchar_t);

	while ( *searchPos != '\0' && unicodeBufferSize > 0 )
	{
		bool shouldAdvance = true;

		if ( *searchPos == '%' )
		{
			// this is an escape sequence that specifies a variable name
			if ( searchPos[1] == 's' && searchPos[2] >= '0' && searchPos[2] <= '9' )
			{
				shouldAdvance = false;

				char variableName[3];
				variableName[0] = searchPos[1];
				variableName[1] = searchPos[2];
				variableName[2] = 0;

				// Handle this as a valid, fixed substitution string
				// look up the variable name
				const wchar_t *value = localizationVariables->GetWString( variableName, L"[unknown]" );

				int paramSize = wcslen(value);
				if (paramSize >= unicodeBufferSize)
				{
					paramSize = MAX( 0, unicodeBufferSize - 1 );
				}

				wcsncpy(outputPos, value, paramSize);

				unicodeBufferSize -= paramSize;
				outputPos += paramSize;
				searchPos += 3;
			}
			else if ( searchPos[1] == '%' )
			{
				// just a '%' char, just write the second one
				searchPos++;
			}
			else if ( localizationVariables )
			{
				// get out the variable name
				const wchar_t *varStart = searchPos + 1;

				// first letter of a valid variable MUST be alphanumeric, otherwise this isn't a variable
				if ( iswalnum(*varStart) )
				{
					const wchar_t *varEnd = wcschr( varStart, '%' );

					if ( varEnd && *varEnd == '%' )
					{
						shouldAdvance = false;

						// assume variable names must be ascii, do a quick convert
						char variableName[32];
						char *vset = variableName;
						for ( const wchar_t *pws = varStart; pws < varEnd && (vset < variableName + sizeof(variableName) - 1); ++pws, ++vset )
						{
							*vset = (char)*pws;
						}
						*vset = 0;

						// look up the variable name
						const wchar_t *value = localizationVariables->GetWString( variableName, L"[unknown]" );
					
						int paramSize = wcslen(value);
						if (paramSize >= unicodeBufferSize)
						{
							paramSize = MAX( 0, unicodeBufferSize - 1 );
						}

						wcsncpy(outputPos, value, paramSize);

						unicodeBufferSize -= paramSize;
						outputPos += paramSize;
						searchPos = varEnd + 1;
					}
				}
			}
		}

		if (shouldAdvance)
		{
			//copy it over, char by char
			*outputPos = *searchPos;

			outputPos++;
			unicodeBufferSize--;

			searchPos++;
		}		
	}

	// ensure null termination
	*outputPos = '\0';
}


wchar_t* CLocalize::GetAsianFrequencySequence( const char * pLanguage )
{
	if( !m_bAsianFrequencySequenceLoaded )
	{
		m_bAsianFrequencySequenceLoaded = true;
		char szFileName[128];
		V_snprintf( szFileName, sizeof( szFileName ), "resource/%s_frequency.txt", pLanguage );
		g_pFullFileSystem->ReadFile( szFileName, "GAME", m_bufAsianFrequencySequence );
		uint nSize = m_bufAsianFrequencySequence.TellPut() / sizeof( wchar_t );
		m_bufAsianFrequencySequence.PutUnsignedShort( 0 ); // 0-terminate

		wchar_t * pAsianFrequencySequence = (wchar_t*) m_bufAsianFrequencySequence.Base();
		// transcode from LT Unicode to GT Unicode
		if( pAsianFrequencySequence[0] == 0xFFFE )
		{
			// switch from little-endian
			for( uint i = 0; i < nSize; ++i )
			{
				wchar_t &refChar = pAsianFrequencySequence[i];
				refChar = ( refChar >> 8 ) | ( refChar << 8 );
			}
		}
	}	

	if( m_bufAsianFrequencySequence.TellPut() > 2 )
	{
		wchar_t * pAsianFrequencySequence = (wchar_t*) m_bufAsianFrequencySequence.Base();

		if( pAsianFrequencySequence[0] == 0xFEFF )
		{
			return pAsianFrequencySequence + 1;
		}
		return pAsianFrequencySequence;
	}
	return NULL;
}


#if defined( GNUC ) || defined( _WIN64 )
#define _INTSIZEOF(n)   ((sizeof(n) + sizeof(intp) - 1) & ~(sizeof(intp) - 1)) 
#endif

#define va_argByIndex(ap,t,i)    ( *(t *)(ap + i * _INTSIZEOF(t)) )

//-----------------------------------------------------------------------------
// Purpose: construct string helper
//-----------------------------------------------------------------------------
template < typename T >
void ConstructStringVArgsInternal_Impl(T *unicodeOutput, int unicodeBufferSizeInBytes, const T *formatString, int numFormatParameters, va_list argList)
{
	// Safety check
	if ( unicodeOutput == NULL || unicodeBufferSizeInBytes < 1 )
	{
		return;
	}
	if (!formatString)
	{
		unicodeOutput[0] = 0;
		return;
	}

	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(T);
	const T *searchPos = formatString;
	T *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	int formatLength = StringFuncs<T>::Length( formatString );

#ifdef PLATFORM_64BITS
	// On 64 bits, va_list does not just point to a contiguous blob of parameters
	// so extract into an array here.
	// TODO: this code is probably fast enough and efficient enough to use
	// on all platforms, so consider enabling it everywhere.
	T** arguments = (T**)stackalloc( sizeof(T*)*numFormatParameters );
	if ( IsPC() )
	{
		for ( int i = 0; i < numFormatParameters; ++i )
		{
			arguments[i] = va_arg( argList, T* );
		}
	}
	
#endif

#ifdef _DEBUG
	int curArgIdx = 0;
#endif

	while ( searchPos[0] != '\0' && unicodeBufferSize > 1 )
	{
		if ( formatLength >= 3 && searchPos[0] == '%' && searchPos[1] == 's' )
		{
			//this is an escape sequence - %s1, %s2 etc, up to %s9

			int argindex = ( searchPos[2] ) - '0' - 1;

			if ( argindex < 0 || argindex > 9 )
			{
				Warning( "Bad format string in CLocalizeStringTable::ConstructString\n" );
				*outputPos = '\0';
				return;
			}

			if ( argindex < numFormatParameters )
			{
				T *param = NULL;
				if ( IsPC() )
				{
#if !defined( _PS3 )
#ifdef PLATFORM_64BITS
					param = arguments[ argindex ];
#else
					param = va_argByIndex( argList, T *, argindex );
#endif
#endif // !_PS3
				}
				else
				{
					// X360TBD: convert string to new %var% format if this assert hits
					Assert( argindex == curArgIdx++ );
					param = va_arg( argList, T* );
				}

				if (!param)
				{
					Assert( !("ConstructStringVArgsInternal_Impl() - Found a %s# escape sequence whose index was more than the number of args.") );
					*outputPos = '\0';
					return;
				}


				int paramSize = StringFuncs<T>::Length(param);
				if (paramSize >= unicodeBufferSize)
				{
					paramSize = unicodeBufferSize - 1;
				}

				memcpy(outputPos, param, paramSize * sizeof(T));

				unicodeBufferSize -= paramSize;
				outputPos += paramSize;

				searchPos += 3;
				formatLength -= 3;
			}
			else
			{
				//copy it over, char by char
				*outputPos = *searchPos;

				outputPos++;
				unicodeBufferSize--;

				searchPos++;
				formatLength--;
			}
		}
		else
		{
			//copy it over, char by char
			*outputPos = *searchPos;

			outputPos++;
			unicodeBufferSize--;

			searchPos++;
			formatLength--;
		}
	}

	// ensure null termination
	Assert( outputPos - unicodeOutput < unicodeBufferSizeInBytes/sizeof(T) );
	*outputPos = L'\0';
}

void CLocalize::ConstructStringVArgsInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, int numFormatParameters, va_list argList)
{
	ConstructStringVArgsInternal_Impl<char>( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

void CLocalize::ConstructStringVArgsInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, int numFormatParameters, va_list argList)
{
	ConstructStringVArgsInternal_Impl<wchar_t>( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList );
}

//-----------------------------------------------------------------------------
// Purpose: construct string helper
//-----------------------------------------------------------------------------
template < typename T >
const T *GetTypedKeyValuesString( KeyValues *pKeyValues, const char *pKeyName );

template < >
const char *GetTypedKeyValuesString<char>( KeyValues *pKeyValues, const char *pKeyName )
{
	return pKeyValues->GetString( pKeyName, "[unknown]" );
}

template < >
const wchar_t *GetTypedKeyValuesString<wchar_t>( KeyValues *pKeyValues, const char *pKeyName )
{
	return pKeyValues->GetWString( pKeyName, L"[unknown]" );
}

template < typename T >
void ConstructStringKeyValuesInternal_Impl( T *unicodeOutput, int unicodeBufferSizeInBytes, const T *formatString, KeyValues *localizationVariables )
{
	T *outputPos = unicodeOutput;

	//assumes we can't have %s10
	//assume both are 0 terminated?
	int unicodeBufferSize = unicodeBufferSizeInBytes / sizeof(T);

	while ( *formatString != '\0' && unicodeBufferSize > 0 )
	{
		bool shouldAdvance = true;

		if ( *formatString == '%' )
		{
			// this is an escape sequence that specifies a variable name
			if ( formatString[1] == 's' && formatString[2] >= '0' && formatString[2] <= '9' )
			{
				// old style escape sequence, ignore
			}
			else if ( formatString[1] == '%' )
			{
				// just a '%' char, just write the second one
				formatString++;
			}
			else if ( localizationVariables )
			{
				// get out the variable name
				const T *varStart = formatString + 1;
				const T *varEnd = StringFuncs<T>::FindChar( varStart, '%' );

				if ( varEnd && *varEnd == '%' )
				{
					shouldAdvance = false;

					// assume variable names must be ascii, do a quick convert
					char variableName[32];
					char *vset = variableName;
					for ( const T *pws = varStart; pws < varEnd && (vset < variableName + sizeof(variableName) - 1); ++pws, ++vset )
					{
						*vset = (char)*pws;
					}
					*vset = 0;

					// look up the variable name
					const T *value = GetTypedKeyValuesString<T>( localizationVariables, variableName );

					int paramSize = StringFuncs<T>::Length( value );
					if (paramSize >= unicodeBufferSize)
					{
						paramSize = MAX( 0, unicodeBufferSize - 1 );
					}

					StringFuncs<T>::Copy( outputPos, value, paramSize );

					unicodeBufferSize -= paramSize;
					outputPos += paramSize;
					formatString = varEnd + 1;
				}
			}
		}

		if (shouldAdvance)
		{
			//copy it over, char by char
			*outputPos = *formatString;

			outputPos++;
			unicodeBufferSize--;

			formatString++;
		}		
	}

	// ensure null termination
	*outputPos = '\0';
}

void CLocalize::ConstructStringKeyValuesInternal(char *unicodeOutput, int unicodeBufferSizeInBytes, const char *formatString, KeyValues *localizationVariables)
{
	ConstructStringKeyValuesInternal_Impl<char>( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
}

void CLocalize::ConstructStringKeyValuesInternal(wchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const wchar_t *formatString, KeyValues *localizationVariables)
{
	ConstructStringKeyValuesInternal_Impl<wchar_t>( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
}
