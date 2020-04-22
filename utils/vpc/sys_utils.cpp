//========= Copyright � 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

#ifdef STEAM
#include "tier1/utlintrusivelist.h"
#endif

#ifdef POSIX
#define _O_RDONLY O_RDONLY
#define _open open
#include <sys/errno.h>
#define _lseek lseek
#define _read read
#define _close close
#define _stat stat
#else
#include <io.h>
#include <ShellAPI.h>
#endif

CXMLWriter::CXMLWriter()
{
	m_fp = NULL;
	m_b2010Format = false;
}

bool CXMLWriter::Open( const char *pFilename, bool b2010Format )
{
	m_b2010Format = b2010Format;

	m_fp = fopen( pFilename, "wt" );
	if ( !m_fp )
		return false;

	if ( b2010Format )
	{
		Write( "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>" );
	}
	else
	{
		// 2005 format
		Write( "<?xml version=\"1.0\" encoding=\"Windows-1252\"?>\n" );
	}

	return true;
}

void CXMLWriter::Close()
{
	if ( !m_fp )
		return;
	fclose( m_fp );
	m_fp = NULL;
}

void CXMLWriter::PushNode( const char *pName )
{
	Indent();

	char *pNewName = strdup( pName );
	m_Nodes.Push( pNewName );
	
	fprintf( m_fp, "<%s%s\n", pName, m_Nodes.Count() == 2 ? ">" : "" );
}

void CXMLWriter::PushNode( const char *pName, const char *pString )
{
	Indent();

	char *pNewName = strdup( pName );
	m_Nodes.Push( pNewName );
	
	fprintf( m_fp, "<%s%s%s>\n", pName, pString ? " " : "", pString ? pString : "" );
}

void CXMLWriter::WriteLineNode( const char *pName, const char *pExtra, const char *pString )
{	
	Indent();

	fprintf( m_fp, "<%s%s>%s</%s>\n", pName, pExtra ?  pExtra : "", pString, pName );
}

void CXMLWriter::PopNode( bool bEmitLabel )
{
	char *pName;
	m_Nodes.Pop( pName );

	Indent();
	if ( bEmitLabel )
	{
		fprintf( m_fp, "</%s>\n", pName );
	}
	else
	{
		fprintf( m_fp, "/>\n", pName );
	}

	free( pName );
}

void CXMLWriter::Write( const char *p )
{
	if ( m_fp )
	{
		Indent();
		fprintf( m_fp, "%s\n", p );
	}
}

CUtlString CXMLWriter::FixupXMLString( const char *pInput )
{
	struct XMLFixup_t
	{
		const char *m_pFrom;
		const char *m_pTo;
		bool		m_b2010Only;	
	};

	// these tokens are not allowed in xml vcproj and be be escaped per msdev docs
	XMLFixup_t xmlFixups[] =
	{
		{"\"",					"&quot;",					false},
		{"\'",					"&apos;",					false},
		{"\n",					"&#x0D;&#x0A;",				false},
		{">",					"&gt;",						false},
		{"<",					"&lt;",						false},
		{"$(InputFileName)",	"%(Filename)%(Extension)",	true}, 
		{"$(InputName)",		"%(Filename)",				true},
		{"$(InputPath)",		"%(FullPath)",				true},
		{"$(InputDir)",			"%(RootDir)%(Directory)",	true},
	};

	bool bNeedsFixups = false;
	CUtlVector< bool > needsFixups;
	CUtlString outString;

	needsFixups.SetCount( ARRAYSIZE( xmlFixups ) );
	for ( int i = 0; i < ARRAYSIZE( xmlFixups ); i++ )
	{
		needsFixups[i] = false;

		if ( !m_b2010Format && xmlFixups[i].m_b2010Only )
			continue;

		if ( V_stristr( pInput, xmlFixups[i].m_pFrom ) )
		{
			needsFixups[i] = true;
			bNeedsFixups = true;
		}
	}

	if ( !bNeedsFixups )
	{
		outString = pInput;
	}
	else
	{
		int flip = 0;
		char bigBuffer[2][8192];
		V_strncpy( bigBuffer[flip], pInput, sizeof( bigBuffer[0] ) );

		for ( int i = 0; i < ARRAYSIZE( xmlFixups ); i++ )
		{
			if ( !needsFixups[i] )
				continue;

			if ( !V_StrSubst( bigBuffer[flip], xmlFixups[i].m_pFrom, xmlFixups[i].m_pTo, bigBuffer[flip ^ 1], sizeof( bigBuffer[0] ), false ) )
			{
				g_pVPC->VPCError( "XML overflow - Increase big buffer" );
			}
			flip ^= 1;
		}
		outString = bigBuffer[flip];
	}

	return outString;
}

void CXMLWriter::Indent()
{
	for ( int i = 0; i < m_Nodes.Count(); i++ )
	{
		if ( m_b2010Format )
		{
			fprintf( m_fp, "  " );
		}
		else
		{
			fprintf( m_fp, "\t" );
		}
	}
}

//-----------------------------------------------------------------------------
//	Sys_LoadFile
//
//-----------------------------------------------------------------------------
int Sys_LoadFile( const char* filename, void** bufferptr, bool bText )
{
	int	handle;
	long	length;
	char*	buffer;

	*bufferptr = NULL;

	if ( !Sys_Exists( filename ) )
		return ( -1 );

	int flags = _O_RDONLY;
#if !defined( POSIX )
	flags |= (bText ? _O_TEXT : _O_BINARY);
#endif
	handle = _open( filename, flags );
	if ( handle == -1 )
		Sys_Error( "Sys_LoadFile(): Error opening %s: %s", filename, strerror( errno ) );

	length = _lseek( handle, 0, SEEK_END );
	_lseek( handle, 0, SEEK_SET );
	buffer = ( char* )malloc( length+1 );

	int bytesRead = _read( handle, buffer, length );
	if ( !bText && ( bytesRead != length ) )
		Sys_Error( "Sys_LoadFile(): read truncated failure" );

	_close( handle );

	// text mode is truncated, add null for parsing
	buffer[bytesRead] = '\0';

	*bufferptr = ( void* )buffer;

	return ( length );
}


//-----------------------------------------------------------------------------
//	Sys_FileLength
//-----------------------------------------------------------------------------
long Sys_FileLength( const char* filename, bool bText )
{
	long	length;

	if ( filename )
	{
		int flags = _O_RDONLY;
#if !defined( POSIX )
		flags |= (bText ? _O_TEXT : _O_BINARY);
#endif
		int handle = _open( filename, flags );
		if ( handle == -1 )
		{
			// file does not exist
			return ( -1 );
		}

		length = _lseek( handle, 0, SEEK_END );
		_close( handle );
	}
	else
	{
		return ( -1 );
	}

	return ( length );
}

//-----------------------------------------------------------------------------
//	Sys_StripPath
//
//	Removes path portion from a fully qualified name, leaving filename and extension.
//-----------------------------------------------------------------------------
void Sys_StripPath( const char* inpath, char* outpath )
{
	const char*	src;

	src = inpath + strlen( inpath );
	while ( ( src != inpath ) && ( *( src-1 ) != '\\' ) && ( *( src-1 ) != '/' ) && ( *( src-1 ) != ':' ) )
		src--;

	strcpy( outpath,src );
}

//-----------------------------------------------------------------------------
//	Sys_Exists
//
//	Returns TRUE if file exists.
//-----------------------------------------------------------------------------
bool Sys_Exists( const char* filename )
{
   FILE*	test;

   if ( ( test = fopen( filename, "rb" ) ) == NULL )
      return ( false );

   fclose( test );

   return ( true );
}

//-----------------------------------------------------------------------------
//	Sys_FileInfo
//-----------------------------------------------------------------------------
bool Sys_FileInfo( const char *pFilename, int64 &nFileSize, int64 &nModifyTime )
{
	struct _stat statData;
	int rt = _stat( pFilename, &statData );
	if ( rt != 0 )
		return false;

	nFileSize = statData.st_size;
	nModifyTime = statData.st_mtime;
	return true;
}

//-----------------------------------------------------------------------------
//	Ignores allowable trailing characters.
//-----------------------------------------------------------------------------
bool Sys_StringToBool( const char *pString )
{
	if ( !V_strnicmp( pString, "no", 2 ) || 
		!V_strnicmp( pString, "off", 3 ) || 
		!V_strnicmp( pString, "false", 5 ) || 
		!V_strnicmp( pString, "not set", 7 ) || 
		!V_strnicmp( pString, "disabled", 8 ) || 
		!V_strnicmp( pString, "0", 1 ) )
	{
		// false
		return false;
	}
	else if ( !V_strnicmp( pString, "yes", 3 ) || 
			!V_strnicmp( pString, "on", 2 ) || 
			!V_strnicmp( pString, "true", 4  ) || 
			!V_strnicmp( pString, "set", 3 ) || 
			!V_strnicmp( pString, "enabled", 7 ) || 
			!V_strnicmp( pString, "1", 1 ) )
	{
		// true
		return true;
	}
	else
	{
		// unknown boolean expression
		g_pVPC->VPCSyntaxError( "Unknown boolean expression '%s'", pString );
	}

	// assume false
	return false;
}

bool Sys_ReplaceString( const char *pStream, const char *pSearch, const char *pReplace, char *pOutBuff, int outBuffSize )
{
	const char	*pFind;
	const char	*pStart = pStream;
	char		*pOut   = pOutBuff;
	int			len;
	bool		bReplaced = false;

	while ( 1 )
	{
		// find sub string
		pFind = V_stristr( pStart, pSearch );
		if ( !pFind )
		{
			/// end of string
			len = strlen( pStart );
			pFind = pStart + len;
			memcpy( pOut, pStart, len );
			pOut += len;
			break;
		}
		else
		{
			bReplaced = true;
		}

		// copy up to sub string
		len = pFind - pStart;
		memcpy( pOut, pStart, len );
		pOut += len;

		// substitute new string
		len = strlen( pReplace );
		memcpy( pOut, pReplace, len );
		pOut += len;

		// advance past sub string
		pStart = pFind + strlen( pSearch );
	}

	*pOut = '\0';

	return bReplaced;
}

//--------------------------------------------------------------------------------
// string match with wildcards.
// '?' = match any char
//--------------------------------------------------------------------------------
bool Sys_StringPatternMatch( char const *pSrcPattern, char const *pString )
{
	for (;;)
	{
		char nPat = *(pSrcPattern++);
		char nString= *(pString++);
		if ( !( ( nPat == nString ) || ( ( nPat == '?' ) && nString ) ) )
			return false;
		if ( !nString )
			return true;	// end of string
	}
}

bool Sys_EvaluateEnvironmentExpression( const char *pExpression, const char *pDefault, char *pOutBuff, int nOutBuffSize )
{
	char *pEnvVarName = (char*)StringAfterPrefix( pExpression, "$env(" );
	if ( !pEnvVarName )
	{
		// not an environment specification
		return false;
	}
	
	char *pLastChar = &pEnvVarName[ V_strlen( pEnvVarName ) - 1 ];
	if ( !*pEnvVarName || *pLastChar != ')' )
	{
		g_pVPC->VPCSyntaxError( "$env() must have a closing ')' in \"%s\"\n", pExpression );
	}

	// get the contents of the $env( blah..blah ) expressions
	// handles expresions that could have whitepsaces
	g_pVPC->GetScript().PushScript( pExpression, pEnvVarName );
	const char *pToken = g_pVPC->GetScript().GetToken( false );
	g_pVPC->GetScript().PopScript();

	if ( pToken && pToken[0] )
	{
		const char *pResolve = getenv( pToken );
		if ( !pResolve )
		{
			// not defined, use default
			pResolve = pDefault ? pDefault : "";
		}
	
		V_strncpy( pOutBuff, pResolve, nOutBuffSize );
	}

	return true;
}
//-----------------------------------------------------------------------------
// Given some arbitrary case filename, provides what the OS thinks it is.
// Windows specific. Returns false if file cannot be resolved (i.e. does not exist).
//-----------------------------------------------------------------------------
bool Sys_GetActualFilenameCase( const char *pFilename, char *pOutputBuffer, int nOutputBufferSize )
{
#if defined( _WINDOWS )
	char filenameBuffer[MAX_PATH];
	V_strncpy( filenameBuffer, pFilename, sizeof( filenameBuffer ) );
	V_FixSlashes( filenameBuffer );
	V_RemoveDotSlashes( filenameBuffer );

	int nFilenameLength = V_strlen( filenameBuffer );

	CUtlString actualFilename;

	// march along filename, resolving up to next seperator
	int nLastComponentStart = 0;
	bool bAddSeparator = false;
	int i = 0;
	while ( i < nFilenameLength )
	{
		// cannot resolve these, emit as-is		
		if ( !V_strnicmp( filenameBuffer + i, ".\\", 2 ) )
		{
			i += 2;
			actualFilename += CUtlString( ".\\" );
			continue;
		}

		// cannot resolve these, emit as-is		
		if ( !V_strnicmp( filenameBuffer + i, "..\\", 3 ) )
		{
			i += 3;
			actualFilename += CUtlString( "..\\" );
			continue;
		}

		// skip until path separator
		while ( i < nFilenameLength && filenameBuffer[i] != '\\' )
		{
			++i;
		}

		bool bFoundSeparator = ( i < nFilenameLength );

		// truncate at separator, windows resolves each component in pieces
		filenameBuffer[i] = 0;

		SHFILEINFOA info = {0};
		HRESULT hr = SHGetFileInfoA( filenameBuffer, 0, &info, sizeof( info ), SHGFI_DISPLAYNAME );
		if ( SUCCEEDED( hr ) )
		{
			// reassemble based on actual component
			if ( bAddSeparator )
			{
				actualFilename += CUtlString( "\\" );
			}
			actualFilename += CUtlString( info.szDisplayName );
		}
		else
		{
			return false;
		}

		// restore path separator
		if ( bFoundSeparator )
		{
			filenameBuffer[i] = '\\';
		}

		++i;
		nLastComponentStart = i;
		bAddSeparator = true;
	}

	V_strncpy( pOutputBuffer, actualFilename.Get(), nOutputBufferSize );
	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
// Given some arbitrary case filename, determine if OS version matches.
//-----------------------------------------------------------------------------
bool Sys_IsFilenameCaseConsistent( const char *pFilename, char *pOutputBuffer, int nOutputBufferSize )
{
	V_strncpy( pOutputBuffer, pFilename, nOutputBufferSize );

	// normalize the provided filename separators
	CUtlString filename = pFilename;
	V_FixSlashes( filename.Get() );
	V_RemoveDotSlashes( filename.Get() );

	if ( !Sys_GetActualFilenameCase( filename.Get(), pOutputBuffer, nOutputBufferSize ) )
		return false;

	if ( !V_strcmp( filename.Get(), pOutputBuffer ) )
		return true;

	return false;
}
