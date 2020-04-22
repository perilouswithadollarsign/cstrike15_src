
#include "tier0/platform.h"
#include "crccheck_shared.h"
#include "tier1/checksum_crc.h"
#include "tier1/strtools.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#undef _stricmp
#define V_stricmp_fast _stricmp
#include <process.h>
#else
#include <stdlib.h>
// stricmp is defined to a Valve routine in platform.h
// which currently we don't want to use because of
// the no-dependency nature of this code.
// When this folds back into vpc this should go away
// and the platform.h definition should be used.
#undef strcasecmp
#define V_stricmp_fast strcasecmp
#endif

#if defined( POSIX )
#include <fcntl.h>
#include <sys/stat.h>
#endif

#pragma warning( disable : 4996 )
#pragma warning( disable : 4127 )

#define MAX_INCLUDE_STACK_DEPTH 10

//-----------------------------------------------------------------------------
//	Sys_Error
//
//-----------------------------------------------------------------------------
void Sys_Error( const char* format, ... )
{
	va_list argptr;

	va_start( argptr,format );
	vfprintf( stderr, format, argptr );
	va_end( argptr );

	exit( 1 );
}

//-----------------------------------------------------------------------------
//	Returns TRUE if file exists.
//-----------------------------------------------------------------------------
bool CRCCheck_FileExists( const char *filename )
{
	FILE *test;

	if ( ( test = fopen( filename, "rb" ) ) == NULL )
		return ( false );

	fclose( test );

	return true;
}

int CRCCheck_LoadFile( const char *filename, void **bufferptr, bool bText )
{
	int	handle;
	long	length;
	char*	buffer;

	*bufferptr = NULL;

	if ( !CRCCheck_FileExists( filename ) )
		return ( -1 );

	int flags = _O_RDONLY;
#if !defined( POSIX )
	flags |= (bText ? _O_TEXT : _O_BINARY);
#endif
	handle = _open( filename, flags );
	if ( handle == -1 )
		Sys_Error( "CRCCheck_LoadFile(): Error opening %s: %s", filename, strerror( errno ) );

	length = _lseek( handle, 0, SEEK_END );
	_lseek( handle, 0, SEEK_SET );
	buffer = new char[length + 1];

	int bytesRead = _read( handle, buffer, length );
	if ( !bText && ( bytesRead != length ) )
		Sys_Error( "CRCCheck_LoadFile(): read truncated failure" );

	_close( handle );

	// text mode is truncated, add null for parsing
	buffer[bytesRead] = '\0';

	*bufferptr = ( void* )buffer;

	return ( bytesRead );
}

void SafeSnprintf( char *pOut, int nOutLen, const char *pFormat, ... )
{
	va_list marker;
	va_start( marker, pFormat );
	_vsnprintf( pOut, nOutLen, pFormat, marker );
	va_end( marker );

	pOut[nOutLen-1] = 0;
}

void FixSlashes( const char *pIn, char *pOut, int nOutLen )
{
	if ( nOutLen <= 0 )
		return;

	strncpy( pOut, pIn, nOutLen - 1 );
	pOut[nOutLen - 1] = '\0';

	int nCount = (int)strlen( pOut );
	for ( int i = 0; i < nCount; i++ )
	{
		if ( ( pOut[i] == '\\' || pOut[i] == '/' ) && pOut[i] != CORRECT_PATH_SEPARATOR )
		{
			pOut[i] = CORRECT_PATH_SEPARATOR;
		}
	}
}

// for linked lists of strings
struct StringNode_t 
{
	StringNode_t *m_pNext;
	char m_Text[1];											// the string data
};

static StringNode_t *MakeStrNode( char const *pStr )
{
	size_t nLen = strlen( pStr );
	StringNode_t *nRet = ( StringNode_t * ) new unsigned char[sizeof( StringNode_t ) + nLen ];
	strcpy( nRet->m_Text, pStr );
	return nRet;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int Sys_LoadTextFileWithIncludes( const char* filename, char** bufferptr, bool bInsertFileMacroExpansion )
{
	FILE *pFileStack[MAX_INCLUDE_STACK_DEPTH];
	int nSP = MAX_INCLUDE_STACK_DEPTH;
	
	StringNode_t *pFileLines = NULL;		// tail ptr for fast adds
	
	size_t nTotalFileBytes = 0;
	FILE *handle = fopen( filename, "r" );
	if ( !handle )
		return -1;

	pFileStack[--nSP] = handle;								// push
	while ( nSP < MAX_INCLUDE_STACK_DEPTH )
	{
		// read lines
		for (;;)
		{
			char lineBuffer[4096];
			char *ln = fgets( lineBuffer, sizeof( lineBuffer ), pFileStack[nSP] );
			if ( !ln ) 
				break;										// out of text
			
			ln += strspn( ln, "\t " );						// skip white space

			if ( memcmp( ln, "#include", 8 ) == 0 )
			{
				// omg, an include
				ln += 8;
				ln += strspn( ln, " \t\"<" );				// skip whitespace, ", and <
				
				size_t nPathNameLength = strcspn( ln, " \t\">\n" );
				if ( !nPathNameLength )
				{
					Sys_Error( "bad include %s via %s\n", lineBuffer, filename );
				}
				ln[nPathNameLength] = 0;					// kill everything after end of filename
				
				char fixedIncludePath[512];
				FixSlashes( ln, fixedIncludePath, sizeof( fixedIncludePath ) );

				FILE *inchandle = fopen( fixedIncludePath, "r" );
				if ( !inchandle )
				{
					Sys_Error( "can't open #include of %s\n", fixedIncludePath );
				}
				if ( !nSP )
				{
					Sys_Error( "include nesting too deep via %s", filename );
				}
				pFileStack[--nSP] = inchandle;
			}
			else
			{
				size_t nLen = strlen( ln );
				nTotalFileBytes += nLen;
				StringNode_t *pNewLine = MakeStrNode( ln );

				pNewLine->m_pNext = pFileLines;
				pFileLines = pNewLine;
			}
		}
		fclose( pFileStack[nSP] );
		nSP++;												// pop stack
	}
	
	// Reverse the pFileLines list so it goes the right way.
	StringNode_t *pPrev = NULL;
	StringNode_t *pCur;
	for( pCur = pFileLines; pCur; )
	{
		StringNode_t *pNext = pCur->m_pNext;
		pCur->m_pNext = pPrev;
		pPrev = pCur;
		pCur = pNext;
	}
	pFileLines = pPrev;

	// Now dump all the lines out into a single buffer.
	char *buffer = new char[nTotalFileBytes + 1];			// and null
	*bufferptr = buffer;									// tell caller

	// copy all strings and null terminate
	int nLine = 0;
	StringNode_t *pNext;
	for( pCur=pFileLines; pCur; pCur=pNext )
	{
		pNext = pCur->m_pNext;
		size_t nLen = strlen( pCur->m_Text );
		memcpy( buffer, pCur->m_Text, nLen );
		buffer += nLen;
		nLine++;

		// Cleanup the line..
		delete [] (unsigned char*)pCur;
	}
	*( buffer++ ) = 0;										// null

	return (int)nTotalFileBytes;
}

// Just like fgets() but it removes trailing newlines.
char* ChompLineFromFile( char *pOut, int nOutBytes, FILE *fp )
{
	char *pReturn = fgets( pOut, nOutBytes, fp );
	if ( pReturn )
	{
		int len = (int)strlen( pReturn );
		if ( len > 0 && pReturn[len-1] == '\n' )
		{
			pReturn[len-1] = 0;
			if ( len > 1 && pReturn[len-2] == '\r' )
				pReturn[len-2] = 0;
		}
	}

	return pReturn;
}

bool CheckSupplementalString( const char *pSupplementalString, const char *pReferenceSupplementalString )
{
	// The supplemental string is only checked while VPC is determining if a project file is stale or not. 
	// It's not used by the pre-build event's CRC check.
	// The supplemental string contains various options that tell how the project was built. It's generated in VPC_GenerateCRCOptionString.
	//
	// If there's no reference supplemental string (which is the case if we're running vpccrccheck.exe), then we ignore it and continue.
	if ( !pReferenceSupplementalString )
		return true;
	
	return ( pSupplementalString && pReferenceSupplementalString && V_stricmp_fast( pSupplementalString, pReferenceSupplementalString ) == 0 );
}

bool VPC_CheckProjectDependencyCRCs( const char *szCRCFile, const char *pReferenceSupplementalString, char *pErrorString, int nErrorStringLength )
{
	// Open it up.
	FILE *fp = fopen( szCRCFile, "rt" );
	if ( !fp )
	{
		SafeSnprintf( pErrorString, nErrorStringLength, "Unable to load %s to check CRC strings", szCRCFile );
		return false;
	}

	bool bReturnValue = false;
	char lineBuffer[2048];

	// Check the version of the CRC file.
	const char *pVersionString = ChompLineFromFile( lineBuffer, sizeof( lineBuffer ), fp );
	if ( pVersionString && V_stricmp_fast( pVersionString, VPCCRCCHECK_FILE_VERSION_STRING ) == 0 )
	{
		// Check the supplemental CRC string.
		const char *pSupplementalString = ChompLineFromFile( lineBuffer, sizeof( lineBuffer ), fp );
		if ( CheckSupplementalString( pSupplementalString, pReferenceSupplementalString ) )
		{
			// Skip over one line of additional metadata used by the VS add-in
			ChompLineFromFile( lineBuffer, sizeof( lineBuffer ), fp );

			// Now read each line. Each line has a CRC and a filename on it.
			while ( 1 )
			{
				char *pLine = ChompLineFromFile( lineBuffer, sizeof( lineBuffer ), fp );
				if ( !pLine )
				{
					// We got all the way through the file without a CRC error, so all's well.
					bReturnValue = true;
					break;
				}

				// resolve type of file to CRC (binary or text script with includes)
				bool bFileIsBinary = false;
				if ( !strncmp( lineBuffer, "BIN ", 4 ) )
				{
					// file is binary
					bFileIsBinary = true;

					// move past the tag
					pLine += 4;
				}

				// expecting <crc> <filename>
				char *pSpace = strchr( pLine, ' ' );
				if ( !pSpace )
				{
					SafeSnprintf( pErrorString, nErrorStringLength, "Invalid line ('%s') in %s", pLine, szCRCFile );
					break;
				}

				// Null-terminate it so we have the CRC by itself and the filename follows the space.
				*pSpace = 0;
				const char *pVPCFilename = pSpace + 1;

				char fixedVPCFilename[512];
				FixSlashes( pVPCFilename, fixedVPCFilename, sizeof( fixedVPCFilename ) );

				// Parse the CRC out.
				unsigned int nReferenceCRC;
				sscanf( pLine, "%x", &nReferenceCRC );

				// Calculate the CRC from the contents of the file.
				char *pBuffer = NULL;
				int nTotalFileBytes = CRCCheck_LoadFile( fixedVPCFilename, (void**)&pBuffer, !bFileIsBinary );
				if ( nTotalFileBytes < 0 )
				{
					SafeSnprintf( pErrorString, nErrorStringLength, "Unable to load %s for CRC comparison.", fixedVPCFilename );
					break;
				}

				CRC32_t nCRCFromContents = CRC32_ProcessSingleBuffer( pBuffer, nTotalFileBytes );
				delete [] pBuffer;

				// Compare them.
				if ( nCRCFromContents != nReferenceCRC )
				{
					SafeSnprintf( pErrorString, nErrorStringLength, "This VCXPROJ is out of sync with its VPC scripts.\n  %s mismatches (0x%x vs 0x%x).\n  Please use VPC to re-generate!\n  \n", fixedVPCFilename, nReferenceCRC, nCRCFromContents );
					break;
				}
			}
		}
		else
		{
			SafeSnprintf( pErrorString, nErrorStringLength, "Supplemental string mismatch." );
		}
	}
	else
	{
		SafeSnprintf( pErrorString, nErrorStringLength, "CRC file %s has an invalid version string ('%s')", szCRCFile, pVersionString ? pVersionString : "[null]" );
	}

	fclose( fp );
	return bReturnValue;
}

int VPC_OldeStyleCRCChecks( int argc, char **argv )
{
	for ( int i=1; (i+2) < argc; )
	{
		const char *pTestArg = argv[i];
		if ( V_stricmp_fast( pTestArg, "-crc" ) != 0 )
		{
			++i;
			continue;
		}

		const char *pVPCFilename = argv[i+1];

		char fixedVPCFilename[512];
		FixSlashes( pVPCFilename, fixedVPCFilename, sizeof( fixedVPCFilename ) );

		// Get the CRC value on the command line.
		const char *pTestCRC = argv[i+2];
		unsigned int nCRCFromCommandLine;
		sscanf( pTestCRC, "%x", &nCRCFromCommandLine );

		// Calculate the CRC from the contents of the file.
		char *pBuffer;
		int nTotalFileBytes = Sys_LoadTextFileWithIncludes( fixedVPCFilename, &pBuffer, true );
		if ( nTotalFileBytes == -1 )
		{
			Sys_Error( "Unable to load %s for CRC comparison.", fixedVPCFilename );
		}

		CRC32_t nCRCFromTextContents = CRC32_ProcessSingleBuffer( pBuffer, nTotalFileBytes );
		delete [] pBuffer;
		
		// Compare them.
		if ( nCRCFromTextContents != nCRCFromCommandLine )
		{
			Sys_Error( "  \n  This VCXPROJ is out of sync with its VPC scripts.\n  %s mismatches (0x%x vs 0x%x).\n  Please use VPC to regenerate!\n  \n", fixedVPCFilename, nCRCFromCommandLine, nCRCFromTextContents );
		}

		i += 2;
	}

	return 0;
}

int VPC_CommandLineCRCChecks( int argc, char **argv )
{
	if ( argc < 2 )
	{
		fprintf( stderr, "%s (Build: %s %s)\n", VPCCRCCHECK_EXE_FILENAME, __DATE__, __TIME__ );
		fprintf( stderr, "Invalid arguments to " VPCCRCCHECK_EXE_FILENAME ". Format: " VPCCRCCHECK_EXE_FILENAME " [project or script filename]\n" );
		return 1;
	}

	const char *pFirstCRC = argv[1];

	// If the first argument starts with -crc but is not -crc2, then this is an old CRC check command line with all the CRCs and filenames
	// directly on the command line. The new format puts all that in a separate file.
	if ( pFirstCRC[0] == '-' && pFirstCRC[1] == 'c' && pFirstCRC[2] == 'r' && pFirstCRC[3] == 'c' && pFirstCRC[4] != '2' )
	{
		return VPC_OldeStyleCRCChecks( argc, argv );
	}

	if ( V_stricmp_fast( pFirstCRC, "-crc2" ) != 0 )
	{
		fprintf( stderr, "Missing -crc2 parameter on vpc CRC check command line." );
		return 1;
	}

	const char *pProjectFilename = argv[2];

	// Build the xxxxx.vcproj.vpc_crc filename
	char szCRCFilename[512];
	SafeSnprintf( szCRCFilename, sizeof( szCRCFilename ), "%s.%s", pProjectFilename, VPCCRCCHECK_FILE_EXTENSION );

	char errorString[1024];
	bool bCRCsValid = VPC_CheckProjectDependencyCRCs( szCRCFilename, NULL, errorString, sizeof( errorString ) );

	if ( bCRCsValid )
	{
		return 0;
	}
	else
	{
		fprintf( stderr, "%s", errorString );
		return 1;
	}
}

