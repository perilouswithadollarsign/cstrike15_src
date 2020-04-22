//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// C runtime and standard library wrappers / equivalents / upgrades.
// Allows centralization of CRT and CRT-like code and consistent behavior across platforms.
// 
//==================================================================================================

#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "tier1/fmtstr.h"
#include "tier1/characterset.h"

// Prints out a memory dump where stuff that's ascii is human readable, etc.
void V_LogMultiline(bool input, char const *label, const char *data, size_t len, CUtlString &output);

// If the string above is really long, you can't pass it into varargs print routines, so this splits everything out into smaller chunks
void V_LogMultilineToArray( bool input, char const *label, const char *data, size_t len, CUtlVector< CUtlString, CUtlMemory<CUtlString, int> > &output )
{
	static const char HEX[] = "0123456789abcdef";
	const char * direction = (input ? " << " : " >> ");
	const size_t LINE_SIZE = 24;
	char hex_line[LINE_SIZE * 9 / 4 + 2], asc_line[LINE_SIZE + 1];
	while (len > 0) 
	{
		V_memset(asc_line, ' ', sizeof(asc_line));
		V_memset(hex_line, ' ', sizeof(hex_line));
		size_t line_len = MIN(len, LINE_SIZE);
		for (size_t i=0; i<line_len; ++i) {
			unsigned char ch = static_cast<unsigned char>(data[i]);
			asc_line[i] = ( V_isprint(ch) && !V_iscntrl(ch) ) ? data[i] : '.';
			hex_line[i*2 + i/4] = HEX[ch >> 4];
			hex_line[i*2 + i/4 + 1] = HEX[ch & 0xf];
		}
		asc_line[sizeof(asc_line)-1] = 0;
		hex_line[sizeof(hex_line)-1] = 0;
		CUtlString s;
		s.Format( "%s %s %s %s\n", label, direction, asc_line, hex_line );
		output.AddToTail( s );
		data += line_len;
		len -= line_len;
	}
}

char* AllocString(const char *pStr, int nMaxChars);


void V_SplitString2InPlace( char *pString, const char **pSeparators, int nSeparators, CUtlVector<const char *> &outStrings )
{
	// We must pass in an empty outStrings buffer or call outStrings.PurgeAndDeleteElements between
	// calls.
	Assert( outStrings.Count() == 0 );
	// This will make outStrings empty but it will not free any memory that the elements were pointing to.
	outStrings.Purge();
	char *pCurPos = pString;
	while ( 1 )
	{
		int iFirstSeparator = -1;
		char *pFirstSeparator = 0;
		for ( int i=0; i < nSeparators; i++ )
		{
			char *pTest = V_stristr_fast( pCurPos, pSeparators[i] );
			if ( pTest && (!pFirstSeparator || pTest < pFirstSeparator) )
			{
				iFirstSeparator = i;
				pFirstSeparator = pTest;
			}
		}

		if ( pFirstSeparator )
		{
			// Split on this separator and continue on.
			int separatorLen = V_strlen( pSeparators[iFirstSeparator] );
			V_memset( pFirstSeparator, 0, separatorLen );
			outStrings.AddToTail( pCurPos );

			pCurPos = pFirstSeparator + separatorLen;
		}
		else
		{
			// Copy the rest of the string
			if ( *pCurPos != '\0' )
			{
				outStrings.AddToTail( pCurPos );
			}
			return;
		}
	}
}

void V_SplitStringInPlace( IN_Z char *pString, IN_Z const char *pSeparator, CUtlVector<const char *> &outStrings )
{
	V_SplitString2InPlace( pString, &pSeparator, 1, outStrings );
}

