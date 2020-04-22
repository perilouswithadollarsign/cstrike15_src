//===== Copyright © 2005-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: build a sheet data file and a large image out of multiple images
//
//===========================================================================//

#include "amalg_texture_parser.h"
#include "tier0/platform.h"
#include "tier0/progressbar.h"
#include "mathlib/mathlib.h"
#include "tier2/tier2.h"

#include "filesystem.h"
#include "tier1/utlstringmap.h"
#include "tier1/strtools.h"
#include "tier1/utlmap.h"
#include "tier2/fileutils.h"
#include "stdlib.h"

#include "materialobjects/dmeamalgtexture.h"
#include "tier1/utlbuffer.h"
#include "tier2/utlstreambuffer.h"

#include "tier0/dbg.h"


static void ApplyMacros( char * in_buf )
{
	CUtlVector<char *> Words;
	V_SplitString( in_buf, " ", Words);
	if ( ( Words.Count() == 4 ) && (! stricmp( Words[0],"ga_frame") ) )
	{
		// ga_frame frm1 frm2 n -> frame frm1{r=a},frm1{g=a},frm1{b=a},frm2{a=a} n
		sprintf( in_buf, "frame %s{r=0},%s{g=a},%s{b=0},%s{a=a} %s",
				Words[1], Words[1], Words[1], Words[2], Words[3] );
	}
	Words.PurgeAndDeleteElements();
}
		 
static char *MoveToStart( char *pLineBuffer )
{
	// Kill newline	'\n'
	char *pChop = strchr( pLineBuffer, '\n' );
	if ( pChop )
		*pChop = 0;

	// Kill '//' remove comment lines.
	char *comment = Q_strstr( pLineBuffer, "//" );
	if ( comment )
		*comment = 0;

	// Move to start of non-whitespace
	char *in_str = pLineBuffer;
	while( ( in_str[0]==' ' ) || ( in_str[0]=='\t') )
		in_str++;

	return in_str;
}


CAmalgamatedTextureParser::CAmalgamatedTextureParser()
{
	m_pSourceFile = NULL;
	m_pShtFile = NULL;
	m_pTgaFile = NULL;
}

void CAmalgamatedTextureParser::Init( const char *pSourceFileName, const char *pTgaFileName, const char *pShtFileName )
{
	m_pSourceFile = pSourceFileName;
	m_pShtFile = pShtFileName;
	m_pTgaFile = pTgaFileName;
}

int CAmalgamatedTextureParser::ParsePackingMode( char *word )
{
	// Read in the packing mode requested.
	int eRequestedMode = PCKM_INVALID;

	if ( !stricmp( word, "flat" ) || !stricmp( word, "rgba" ) )
	{
		eRequestedMode = PCKM_FLAT;
	}
	else if ( !stricmp( word, "rgb+a" ) )
	{
		eRequestedMode = PCKM_RGB_A;
	}
	
	if ( eRequestedMode == PCKM_INVALID )
	{
		Warning( "*** line %d: invalid packmode specified, allowed values are 'rgba' or 'rgb+a'!\n", m_NumActualLinesRead );
	}	

	return eRequestedMode;
}  


int CAmalgamatedTextureParser::ParseSequenceType( char *word )
{
	int eMode;

	// Figure out the sequence type
	char const *szSeqType = StringAfterPrefix( word, "sequence" );
	if ( !stricmp( szSeqType, "" ) || !stricmp( szSeqType, "-rgba" ) )
	{
		eMode = SQM_RGBA;
	}
	else if ( !stricmp( szSeqType, "-rgb" ) )
	{
		eMode = SQM_RGB;
	}
	else if ( !stricmp( szSeqType, "-a" ) )
	{
		eMode = SQM_ALPHA;
	}
	else
	{
		Warning( "*** line %d: invalid sequence type '%s', allowed 'sequence-rgba' or 'sequence-rgb' or 'sequence-a'!\n", m_NumActualLinesRead, word ),
		exit( -1 );
	}

	return eMode;
}


void CAmalgamatedTextureParser::ParseFrameImages( CUtlVector<char *> &words, CUtlVector<char *> &outImageNames )
{
	for ( int i = 0; i < words.Count() - 2; i++ )
	{
		char *fnamebuf = words[i+1];
		outImageNames.AddToTail( fnamebuf );	
	}
}


bool CAmalgamatedTextureParser::ReadFile( CDmeAmalgamatedTexture &amalgTex )
{
	// FIXME handle file not found.
	CRequiredInputTextFile f( m_pSourceFile );
	
	char linebuffer[4096];
	m_NumActualLinesRead = 0;
	while ( f.ReadLine( linebuffer, sizeof(linebuffer) ) )
	{
		++m_NumActualLinesRead;

		char *in_str = MoveToStart( linebuffer );
		if ( in_str[0] == NULL )
			continue;

		strlwr( in_str ); // send string to lowercase.
		ApplyMacros( in_str );
		CUtlVector<char *> words;
		V_SplitString( in_str, " ", words);
		if ( ( words.Count() == 1) && (! stricmp( words[0],"loop" ) ) )
		{
			amalgTex.SetCurrentSequenceClamp( false );
		}
		else if ( ( words.Count() == 2 ) && (! stricmp( words[0], "packmode" ) ) )
		{
			// Read in the packing mode requested.
			int eRequestedMode = ParsePackingMode( words[1] );
			if ( eRequestedMode == PCKM_INVALID )
			{
				Msg( "Invalid packing mode." );
				return false;
			}
			amalgTex.SetPackingMode( eRequestedMode );
		}
		else if ( ( words.Count() == 2) && StringHasPrefix( words[0], "sequence" ) )
		{
			int seq_no = atoi( words[1] );
			if ( seq_no != amalgTex.GetSequenceCount() )
			{
				Warning( "Sequence number mismatch.\n" );
			}
			
			// Figure out the sequence type
			int mode = ParseSequenceType( words[0] );
			amalgTex.CreateNewSequence( mode );
		}
		else if  ( ( words.Count() >= 3) && (! stricmp( words[0],"frame" ) ) )
		{
			if ( amalgTex.CurrentSequenceExists() )
			{
				float ftime = atof( words[ words.Count() - 1 ] );
				
				CUtlVector<char *> imageNames;
				ParseFrameImages( words, imageNames );

				amalgTex.CreateFrame( imageNames, ftime );
			}
		}
		else
		{
			Warning( "*** line %d: Bad command \"%s\"!\n", m_NumActualLinesRead, in_str ),
			exit( -1 );
		}
		words.PurgeAndDeleteElements();
	}

	return true;
}


bool CAmalgamatedTextureParser::ReadFileUsingBuffer( CDmeAmalgamatedTexture &amalgTex )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::READ_ONLY | CUtlBuffer::TEXT_BUFFER );

	if ( !g_pFullFileSystem->ReadFile( m_pSourceFile, NULL, buf ) )
	{
		Warning( "CAmalgamatedTextureParser: Unable to open file '%s'\n", m_pSourceFile );
		return false;
	}
	
	char linebuffer[4096];
	m_NumActualLinesRead = 0;
	
	while ( buf.IsValid() )
	{
		buf.GetLine( linebuffer, sizeof(linebuffer) );
		++m_NumActualLinesRead;

		char *in_str = MoveToStart( linebuffer );
		if ( in_str[0] == NULL )
			continue;

		strlwr( in_str ); // send string to lowercase.
		ApplyMacros( in_str );
		CUtlVector<char *> words;
		V_SplitString( in_str, " ", words);
		if ( ( words.Count() == 1) && (! stricmp( words[0],"loop" ) ) )
		{
			amalgTex.SetCurrentSequenceClamp( false );
		}
		else if ( ( words.Count() == 2 ) && (! stricmp( words[0], "packmode" ) ) )
		{
			// Read in the packing mode requested.
			int eRequestedMode = ParsePackingMode( words[1] );
			amalgTex.SetPackingMode( eRequestedMode );
		}
		else if ( ( words.Count() == 2) && StringHasPrefix( words[0], "sequence" ) )
		{
			int seq_no = atoi( words[1] );
			if ( seq_no != amalgTex.GetSequenceCount() )
			{
				Warning( "Sequence number mismatch.\n" );
			}
			
			// Figure out the sequence type
			int mode = ParseSequenceType( words[0] );
			amalgTex.CreateNewSequence( mode );
		}
		else if  ( ( words.Count() >= 3) && (! stricmp( words[0],"frame" ) ) )
		{
			if ( amalgTex.CurrentSequenceExists() )
			{
				float ftime = atof( words[ words.Count() - 1 ] );
				
				CUtlVector<char *> imageNames;
				ParseFrameImages( words, imageNames );

				amalgTex.CreateFrame( imageNames, ftime );
			}
		}
		else
		{
			printf( "*** line %d: Bad command \"%s\"!\n", m_NumActualLinesRead, in_str ),
			exit( -1 );
		}
		words.PurgeAndDeleteElements();
	}

	return true;
}




