
#include "cbase.h"
#include "localization_provider.h"

enum { kScratchBufferSize = 1024 };

CLocalizationProvider *GLocalizationProvider() 
{
	static CVGUILocalizationProvider g_VGUILocalizationProvider;
	return &g_VGUILocalizationProvider;
}

// vgui localization implementation

CVGUILocalizationProvider::CVGUILocalizationProvider()
{

}

locchar_t *CVGUILocalizationProvider::Find( const char *pchKey )
{
	return (locchar_t*)g_pVGuiLocalize->Find( pchKey );
}

void CVGUILocalizationProvider::ConstructString( locchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const locchar_t *formatString, int numFormatParameters, ... )
{
	va_list argList;
	va_start(argList, numFormatParameters);
	g_pVGuiLocalize->ConstructStringVArgs( unicodeOutput, unicodeBufferSizeInBytes, formatString, numFormatParameters, argList);
	va_end(argList);
}

void CVGUILocalizationProvider::ConstructString( OUT_Z_BYTECAP(unicodeBufferSizeInBytes) locchar_t *unicodeOutput, int unicodeBufferSizeInBytes, const locchar_t *formatString, KeyValues *localizationVariables )
{
	g_pVGuiLocalize->ConstructString( unicodeOutput, unicodeBufferSizeInBytes, formatString, localizationVariables );
}

void CVGUILocalizationProvider::ConvertLoccharToANSI( const locchar_t *loc_In, CUtlConstString *out_ansi ) const
{
	char ansi_Scratch[kScratchBufferSize];

	g_pVGuiLocalize->ConvertUnicodeToANSI( loc_In, ansi_Scratch, kScratchBufferSize );
	*out_ansi = ansi_Scratch;
}

void CVGUILocalizationProvider::ConvertLoccharToUnicode( const locchar_t *loc_In, CUtlConstWideString *out_unicode ) const
{
	*out_unicode = loc_In;
}

void CVGUILocalizationProvider::ConvertUTF8ToLocchar( const char *utf8_In, CUtlConstStringBase<locchar_t> *out_loc ) const
{
	locchar_t loc_Scratch[kScratchBufferSize];

	V_UTF8ToUnicode( utf8_In, loc_Scratch, sizeof( loc_Scratch ) );
	*out_loc = loc_Scratch;
}

void CVGUILocalizationProvider::ConvertUTF8ToLocchar( const char *utf8, locchar_t *locchar, int cubDestSizeInBytes )
{
	V_UTF8ToUnicode( utf8, locchar, cubDestSizeInBytes );
}

int CVGUILocalizationProvider::ConvertLoccharToANSI( const locchar_t *loc, char *ansi, int ansiBufferSize )
{
	return g_pVGuiLocalize->ConvertUnicodeToANSI( loc, ansi, ansiBufferSize );
}

int CVGUILocalizationProvider::ConvertLoccharToUnicode( const locchar_t *loc, wchar_t *unicode, int unicodeBufferSizeInBytes )
{
	Q_wcsncpy( unicode, loc, unicodeBufferSizeInBytes );
	return 0;
}
