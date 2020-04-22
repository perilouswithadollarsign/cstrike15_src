//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Valve font compiler
//
//===========================================================================//

#include <stdlib.h>
#include <stdio.h>

#include "utlbuffer.h"
#include "valvefont.h"

int Usage()
{
	printf( "Usage:   vfont inputfont.ttf [outputfont.vfont]\n" );
	return -1;
}

int main( int argc, char ** argv )
{
	printf( "Valve Software - vfont.exe (" __DATE__ ")\n" );

	int iArg = 1;
	if ( iArg >= argc )
		return Usage();

	//
	//	Check if we are in decompiler mode
	//
#if VFONT_DECOMPILER
	bool bDecompiler = false;
	bDecompiler = !stricmp( argv[ iArg ], "-d" );
	if ( bDecompiler )
		++ iArg;
#endif

	if ( iArg >= argc )
		return Usage();

	//
	//	Input file
	//
	char const *szInput = argv[ iArg ];
	++ iArg;

	//
	//	Output file
	//
	char szOutput[ 512 ] = {0};

	if ( iArg >= argc )
	{
		int numBytes = strlen( szInput );
		strcpy( szOutput, szInput );
		
		char *pDot = strrchr( szOutput, '.' );
		char *pSlash1 = strrchr( szOutput, '/' );
		char *pSlash2 = strrchr( szOutput, '\\' );
		
		if ( !pDot )
		{
			pDot = szOutput + numBytes;
		}
		else if ( pDot < pSlash1 || pDot < pSlash2 )
		{
			pDot = szOutput + numBytes;
		}

		sprintf( pDot, ".vfont" );
	}
	else
	{
		strcpy( szOutput, argv[ iArg ] );
	}

	//
	//	Read the input
	//
	FILE *fin = fopen( szInput, "rb" );
	if ( !fin )
	{
		printf( "Error: cannot open input file '%s'!\n", szInput );
		return -2;
	}

	fseek( fin, 0, SEEK_END );
	long lSize = ftell( fin );
	fseek( fin, 0, SEEK_SET );

	CUtlBuffer buf;
	buf.EnsureCapacity( lSize );
	fread( buf.Base(), 1, lSize, fin );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, lSize );
	fclose( fin );

	//
	//	Compile
	//
#if VFONT_DECOMPILER
	if ( bDecompiler )
	{
		bool bResult = ValveFont::DecodeFont( buf );
		if ( !bResult )
		{
			printf( "Error: cannot decompile input file '%s'!\n", szInput );
			return 1;
		}
	}
	else
		ValveFont::EncodeFont( buf );
#else
	ValveFont::EncodeFont( buf );
#endif

	//
	//	Write the output
	//
	FILE *fout = fopen( szOutput, "wb" );
	if ( !fout )
	{
		printf( "Error: cannot open output file '%s'!\n", szOutput );
		return -3;
	}

	fwrite( buf.Base(), 1, buf.TellPut(), fout );
	fclose( fout );

	printf( "vfont successfully %scompiled '%s' as '%s'.\n",
#if VFONT_DECOMPILER
		bDecompiler ? "de" : "",
#else
		"",
#endif
		szInput, szOutput );
	return 0;
}
