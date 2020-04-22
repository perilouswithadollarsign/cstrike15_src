//========= Copyright � 1996-2016, Valve Corporation, All rights reserved. ============//
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
#include <glob.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <ShellAPI.h>
#endif

#ifdef OSX
#include <mach-o/dyld.h>
#endif

CXMLWriter::CXMLWriter()
{
	m_WriteBuffer.SetBufferType( true, false );
	m_b2010Format = false;
}

bool CXMLWriter::Open( const char *pFilename, bool b2010Format, bool bForceWrite )
{
	m_FilenameString = pFilename;
	m_b2010Format = b2010Format;
	m_bForceWrite = bForceWrite;

	m_WriteBuffer.Clear();

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

bool CXMLWriter::Close()
{
	bool bUpToDate = !m_bForceWrite && !Sys_FileChanged( m_FilenameString.Get(), m_WriteBuffer, true );

    if ( !bUpToDate )
    {
        Sys_WriteFile( m_FilenameString.Get(), m_WriteBuffer, true );
        Sys_CopyToMirror( m_FilenameString.Get() );
    }

	m_FilenameString = NULL;
	return !bUpToDate;
}

void CXMLWriter::PushNode( const char *pName, XMLOmitIfEmpty_t omitOption /*= XMLOIE_NEVER_OMIT*/ )
{
	FinishPush( true );

	Node_t &pushed = m_Nodes[m_Nodes.Push()];
	pushed.m_Name = pName;
	pushed.m_omitOption = omitOption;
}

void CXMLWriter::PushNode( const char *pName, const char *pString )
{
	PushNode( pName );
	AddNodeProperty( pString );
}

void CXMLWriter::WriteLineNode( const char *pName, const char *pExtra, const char *pString, XMLOmitIfEmpty_t omitOption /*= XMLOIE_NEVER_OMIT*/ )
{
	if ( !pExtra || pExtra[0] == '\0' )
	{
		pExtra = nullptr;
	}

	if ( !pString || pString[0] == '\0' )
	{
		pString = nullptr;
	}

	switch ( omitOption )
	{
		case XMLOIE_NEVER_OMIT:
			break;

		case XMLOIE_OMIT_ON_EMPTY_CONTENTS:
		{
			if ( !pString )
				return;

			break;
		}

		case XMLOIE_OMIT_ON_EMPTY_EVERYTHING:
		{
			if ( !pExtra && !pString )
				return;

			break;
		}
		NO_DEFAULT;
	};
	
	FinishPush( true );
	Indent();	

	m_WriteBuffer.Printf( "<%s%s%s>", pName, (pExtra ?  pExtra : ""), (pString ? "" : " /") );
	if ( pString )
	{
		m_WriteBuffer.PutString( pString );
		m_WriteBuffer.Printf( "</%s>\n", pName );
	}
}

void CXMLWriter::PopNode( void )
{
	Assert( m_Nodes.Count() != 0 );

	Node_t &currentNode = m_Nodes.Top();

	switch ( currentNode.m_omitOption )
	{
		case XMLOIE_NEVER_OMIT:
			break;

		case XMLOIE_OMIT_ON_EMPTY_CONTENTS:
		{
			if ( !currentNode.m_bHasFinishedPush )
			{
				m_Nodes.Pop();
				return;
			}

			break;
		}

		case XMLOIE_OMIT_ON_EMPTY_EVERYTHING:
		{
			if ( !currentNode.m_PropertyStrings.Count() && !currentNode.m_bHasFinishedPush )
			{
				m_Nodes.Pop();
				return;
			}

			break;
		}
		NO_DEFAULT;
	};

	if ( FinishPush( false ) )
	{
		//we didn't write any child data, so the FinishPush() call left the angle brackets open ended for us
		m_Nodes.Pop();
		m_WriteBuffer.PutString( " />\n" );
	}
	else
	{
		CUtlString nameCopy = m_Nodes.Top().m_Name;
		m_Nodes.Pop();
		Indent();
		m_WriteBuffer.Printf( "</%s>\n", nameCopy.Get() );
	}
}

void CXMLWriter::AddNodeProperty( const char *pPropertyName, const char *pValue )
{
	CUtlString propertyString = pPropertyName;
	propertyString += "=\"";
	propertyString += pValue;
	propertyString += "\"";

	AddNodeProperty( propertyString.Get() );
}

void CXMLWriter::AddNodeProperty( const char *pString )
{
	if ( m_Nodes.Count() == 0 )
	{
		g_pVPC->VPCError( "No nodes to add a property to" );
		UNREACHABLE();
	}

	Node_t &top = m_Nodes.Top();
	if ( top.m_bHasFinishedPush )
	{
		g_pVPC->VPCError( "Node has already finished pushing. You must add all properties before writing child data" );
		UNREACHABLE();
	}

	top.m_PropertyStrings.AddToTail( pString );
}

void CXMLWriter::Write( const char *p )
{
	Indent();

	//broken up to avoid running pString through CUtlBuffer::Printf because external strings can easily overflow the 8KB CUtlBuffer::Printf() buffer
	m_WriteBuffer.PutString( p );
	m_WriteBuffer.PutString( "\n" );
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
		{"&",					"&amp;",					false},
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

		if ( V_stristr_fast( pInput, xmlFixups[i].m_pFrom ) )
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

void CXMLWriter::Indent( int nReduceDepth /*= 0*/ )
{
	FinishPush( true );
	for ( int i = nReduceDepth; i < m_Nodes.Count(); i++ )
	{
		if ( m_b2010Format )
		{
			m_WriteBuffer.Printf( "  " );
		}
		else
		{
			m_WriteBuffer.Printf( "\t" );
		}
	}
}

bool CXMLWriter::FinishPush( bool bCloseAngleBracket )
{
	if ( m_Nodes.Count() == 0 )
		return false;

	Node_t &currentNode = m_Nodes.Top();
	if ( currentNode.m_bHasFinishedPush )
		return false;

	currentNode.m_bHasFinishedPush = true;

	Indent( 1 );
	m_WriteBuffer.Printf( "<%s", currentNode.m_Name.Get() );

	for ( int i = 0; i < currentNode.m_PropertyStrings.Count(); ++i )
	{
		m_WriteBuffer.PutString( " " );
		m_WriteBuffer.PutString( currentNode.m_PropertyStrings[i].Get() );
	}

	if ( bCloseAngleBracket )
	{
		m_WriteBuffer.PutString( ">\n" );		
	}
	return true;
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
	buffer = new char[length + 1];

	int bytesRead = _read( handle, buffer, length );
	if ( !bText && ( bytesRead != length ) )
		Sys_Error( "Sys_LoadFile(): read truncated failure" );

	_close( handle );

	// text mode is truncated, add null for parsing
	buffer[bytesRead] = '\0';

	*bufferptr = ( void* )buffer;

	return bytesRead;
}


//-----------------------------------------------------------------------------
//	Sys_LoadFileIntoBuffer
//
//-----------------------------------------------------------------------------
bool Sys_LoadFileIntoBuffer( const char *pFilename, CUtlBuffer &buf, bool bText )
{
	// NOTE: This strips CRLFs from text files on load (nBytesRead may be < statBuf.st_size)
	struct stat statBuf;
	if ( stat( pFilename, &statBuf ) != 0 )
		return false;
	buf.Clear();
	buf.SetBufferType( bText, false );
	buf.EnsureCapacity( (int)(statBuf.st_size + 1) );
	if ( !buf.IsValid() )
		return false;

	FILE *fp = fopen( pFilename, bText ? "rt" : "rb" );
	if ( !fp )
		return false;

	char *pBuffer = (char*)buf.Base();
	size_t nBytesRead = fread( pBuffer, 1, statBuf.st_size, fp );
	fclose( fp );
	buf.SeekPut( CUtlBuffer::SEEK_HEAD, (int)nBytesRead ); // the buffer will be NULL-terminated after this call
	return ( nBytesRead || !statBuf.st_size );
}


//-----------------------------------------------------------------------------
//	Sys_RemoveBufferCRLFs
//
//-----------------------------------------------------------------------------
bool Sys_RemoveBufferCRLFs( CUtlBuffer &buffer )
{
	Assert( buffer.IsText() );
	CUtlBuffer crlfTemp( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( buffer.ConvertCRLF( crlfTemp ) )
	{
		buffer.AssumeMemory( crlfTemp.Base(), crlfTemp.TellMaxPut(), crlfTemp.TellMaxPut(), crlfTemp.GetFlags() );
		crlfTemp.DetachMemory();
		return true;
	}
	return false; // No conversion necessary
}


//-----------------------------------------------------------------------------
//	Sys_LoadFileAsLines
//
//-----------------------------------------------------------------------------
bool Sys_LoadFileAsLines( const char *pFilename, CUtlVector< CUtlString > &lines )
{
	// Load the file and remove CRLFs
	CUtlBuffer fileBuffer;
	if ( !Sys_LoadFileIntoBuffer( pFilename, fileBuffer, true ) )
		return false;

	// Split the file into lines
	CSplitString linesSplitString( (const char *)fileBuffer.Base(), "\n" );
	for ( int i = 0; i < linesSplitString.Count(); i++ ) lines.AddToTail( linesSplitString[i] );
	return true;
}

//-----------------------------------------------------------------------------
//	Sys_FileChanged
//
//-----------------------------------------------------------------------------
bool Sys_FileChanged( const char *pFilename, const CUtlBuffer &newData, bool bText )
{
	if ( Sys_Exists( pFilename ) )
	{
		CUtlBuffer oldData;
		if ( Sys_LoadFileIntoBuffer( pFilename, oldData, bText ) )
		{
			// Data should be non-CRLF in memory (Sys_LoadFileIntoBuffer ensures this)
			Assert( !oldData.ContainsCRLF() && !newData.ContainsCRLF() );
			if ( oldData.TellMaxPut() == newData.TellMaxPut() )
			{
				if ( !V_memcmp( oldData.Base(), newData.Base(), newData.TellMaxPut() ) )
					return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
//	Sys_WriteFile
//
//-----------------------------------------------------------------------------
bool Sys_WriteFile( const char *pFilename, const CUtlBuffer &newData, bool bText )
{
	FILE *file = fopen( pFilename, ( bText ? "wt" : "wb" ) );
	if ( file )
	{
		fwrite( newData.Base(), 1, newData.TellMaxPut(), file );
		fclose( file );
		return true;
	}
#if defined( PLATFORM_WINDOWS )
	g_pVPC->VPCWarning( "Could not write file \"%s\", GetLastError() = %d", pFilename, GetLastError() );
#else
	g_pVPC->VPCWarning( "Could not write file %s", pFilename );
#endif

	Assert( file );
	return false;
}


//-----------------------------------------------------------------------------
//	Sys_WriteFileIfChanged
//
//-----------------------------------------------------------------------------
bool Sys_WriteFileIfChanged( const char *pFilename, const CUtlBuffer &newData, bool bText )
{
	Sys_CopyToMirror( pFilename );

	// If the file exists and is identical, don't bother writing it
	// (prevents timestamps updating and causing needless rebuilds)
	if ( !Sys_FileChanged( pFilename, newData, bText ) )
		return false;

	return Sys_WriteFile( pFilename, newData, bText );
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
void Sys_StripPath( const char* inpath, char* outpath, int nOutPathSize )
{
	const char*	src;

	src = inpath + strlen( inpath );
	while ( ( src != inpath ) && ( *( src-1 ) != '\\' ) && ( *( src-1 ) != '/' ) && ( *( src-1 ) != ':' ) )
		src--;

	V_strncpy( outpath, src, nOutPathSize );
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
//	Sys_Touch
//
//	Returns TRUE if the file could be accessed for write
//-----------------------------------------------------------------------------
bool Sys_Touch( const char* filename )
{
   FILE*	test;

   if ( ( test = fopen( filename, "wb" ) ) == NULL )
      return ( false );

   fclose( test );

   return ( true );
}

//-----------------------------------------------------------------------------
//	Sys_FileInfo
//-----------------------------------------------------------------------------
bool Sys_FileInfo( const char *pFilename, int64 &nFileSize, int64 &nModifyTime, bool &bIsReadOnly )
{
	struct _stat statData;
	int rt = _stat( pFilename, &statData );
	if ( rt != 0 )
		return false;

	nFileSize = statData.st_size;
	nModifyTime = statData.st_mtime;
	bIsReadOnly = !( statData.st_mode & S_IWRITE );
	return true;
}

//-----------------------------------------------------------------------------
//	Ignores allowable trailing characters.
//-----------------------------------------------------------------------------
bool Sys_StringToBool( const char *pString, bool bAssumeTrueIfAmbiguous /*= false*/ )
{
	if ( !V_strnicmp( pString, "no", 2 ) || 
		!V_strnicmp_fast( pString, "off", 3 ) || 
		!V_strnicmp_fast( pString, "false", 5 ) || 
		!V_strnicmp_fast( pString, "not set", 7 ) || 
		!V_strnicmp_fast( pString, "disabled", 8 ) || 
		!V_strnicmp_fast( pString, "0", 1 ) )
	{
		// false
		return false;
	}
	else if ( !V_strnicmp_fast( pString, "yes", 3 ) || 
			!V_strnicmp_fast( pString, "on", 2 ) || 
			!V_strnicmp_fast( pString, "true", 4  ) || 
			!V_strnicmp_fast( pString, "set", 3 ) || 
			!V_strnicmp_fast( pString, "enabled", 7 ) || 
			!V_strnicmp_fast( pString, "1", 1 ) )
	{
		// true
		return true;
	}
	else
	{
		if ( bAssumeTrueIfAmbiguous )
		{
			return true;
		}
		// unknown boolean expression
		g_pVPC->VPCSyntaxError( "Unknown boolean expression '%s'", pString );
	}

	// assume false
	return false;
}

bool Sys_ReplaceString( const char *pStream, const char *pSearch, const char *pReplace, char *pOutBuff, int nOutBuffSize )
{
	const char	*pFind;
	const char	*pStart = pStream;
	char		*pOut   = pOutBuff;
	size_t		len;
	bool		bReplaced = false;
	size_t		nRemainingBytes = (int)nOutBuffSize;

	while ( 1 )
	{
		// find sub string
		pFind = V_stristr( pStart, pSearch );
		if ( !pFind )
		{
			// end of string
			len = V_strlen( pStart );
			pFind = pStart + len;
			if ( len > nRemainingBytes )
			{
				// prevent the destructive copy
				g_pVPC->VPCError( "Sys_ReplaceString: Unexpected Buffer Overflow" );
			}
			V_memcpy( pOut, pStart, len );
			pOut += len;
			nRemainingBytes -= len;
			break;
		}
		else
		{
			bReplaced = true;
		}

		// copy up to sub string
		len = pFind - pStart;
		if ( len > nRemainingBytes )
		{
			// prevent the destructive copy
			g_pVPC->VPCError( "Sys_ReplaceString: Unexpected Buffer Overflow" );
		}
		V_memcpy( pOut, pStart, len );
		pOut += len;
		nRemainingBytes -= len;

		// substitute new string
		len = V_strlen( pReplace );
		if ( len > nRemainingBytes )
		{
			// prevent the destructive copy
			g_pVPC->VPCError( "Sys_ReplaceString: Unexpected Buffer Overflow" );
		}
		V_memcpy( pOut, pReplace, len );
		pOut += len;
		nRemainingBytes -= len;

		// advance past sub string
		pStart = pFind + V_strlen( pSearch );
	}

	if ( nRemainingBytes < 1 )
	{
		// prevent the destructive terminate
		g_pVPC->VPCError( "Sys_ReplaceString: Unexpected Buffer Overflow" );
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

//--------------------------------------------------------------------------------
// Does the line containing the given cursor start with a '//' comment?
//--------------------------------------------------------------------------------
bool Sys_IsSingleLineComment( const char *pSearchPos, const char *pFileStart )
{
	// Rewind to line start
	while( pSearchPos > pFileStart )
	{
		if ( ( pSearchPos[-1] == '\n' ) || ( pSearchPos[-1] == '\r' ) )
			break;
		pSearchPos--;
	}
	// Skip past whitespace
	while( V_isspace( pSearchPos[0] ) ) pSearchPos++;
	// Return true if the first non-whitespace characters on the line are '//'
	return ( pSearchPos[0] == '/' ) && ( pSearchPos[1] == '/' );
}

const char *Sys_EvaluateEnvironmentExpression( const char *pExpression, const char *pDefault )
{
	bool bEnvDefinedMacro = false;
	char *pEnvVarName = (char*)StringAfterPrefix( pExpression, "$env(" );
	if ( !pEnvVarName )
	{
		// not an environment specification
		pEnvVarName = (char*)StringAfterPrefix( pExpression, "$envdefined(" );
		if ( !pEnvVarName )
		{
			return NULL;
		}
		bEnvDefinedMacro = true;
	}
	
	char *pLastChar = &pEnvVarName[ V_strlen( pEnvVarName ) - 1 ];
	if ( !*pEnvVarName || *pLastChar != ')' )
	{
		g_pVPC->VPCSyntaxError( "%s must have a closing ')' in \"%s\"\n", bEnvDefinedMacro ? "$envdefined()" : "$env()", pExpression );
	}

	// get the contents of the $env( blah..blah ) expressions
	// handles expresions that could have whitepsaces
	g_pVPC->GetScript().PushScript( pExpression, pEnvVarName, 1, false, false );
	const char *pToken = g_pVPC->GetScript().GetToken( false );
	g_pVPC->GetScript().PopScript();

	if ( pToken && pToken[0] )
	{
		const char *pResolve = getenv( pToken );
        if ( bEnvDefinedMacro )
		{
			return pResolve ? "1" : "0";
		}
		else
		{
			if ( pResolve )
			{
				return pResolve;
			}
		}
    }

    return pDefault;
}

bool Sys_ExpandFilePattern( const char *pPattern, CUtlVector< CUtlString > &vecResults )
{
#if defined( _WIN32 )
	CUtlPathStringHolder pathPart( pPattern );
	pathPart.StripFilename();
    pathPart.AppendSlash();

	WIN32_FIND_DATA findData;
	HANDLE hFind = FindFirstFile( pPattern, &findData );
	if ( hFind != INVALID_HANDLE_VALUE )
	{
		vecResults.AddToTail( g_pVPC->FormatTemp1( "%s%s", pathPart.Get(), findData.cFileName ) );
		BOOL bMore = TRUE;
		while ( bMore )
		{
			bMore = FindNextFile( hFind, &findData );
			if ( bMore )
				vecResults.AddToTail( g_pVPC->FormatTemp1( "%s%s", pathPart.Get(), findData.cFileName ) );
		}
		FindClose( hFind );
	}
#elif defined( POSIX )
	glob_t gr;
	if ( glob( pPattern, 0, NULL, &gr ) == 0 )
	{
		for ( int i = 0; i < gr.gl_pathc; i++ )
		{
			vecResults.AddToTail( gr.gl_pathv[i] );
		}
		globfree( &gr );
	}
#else
#error
#endif
	return vecResults.Count() > 0;
}

bool Sys_GetExecutablePath( char *pBuf, int cbBuf )
{
#if defined( _WIN32 )
	return ( 0 != GetModuleFileNameA( NULL, pBuf, cbBuf ) );
#elif defined(OSX)
	uint32_t _nBuff = cbBuf;
	bool bSuccess = _NSGetExecutablePath(pBuf, &_nBuff) == 0;
	pBuf[cbBuf-1] = '\0';
	return bSuccess;
#elif defined LINUX
	ssize_t nRead = readlink("/proc/self/exe", pBuf, cbBuf-1 );
	if ( nRead != -1 )
	{
		pBuf[ nRead ] = 0;
		return true;
	}

	pBuf[0] = 0;
	return false;
#else
#error Sys_GetExecutablePath
#endif
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Sys_CreatePath( const char *path )
{
	char pFullPath[MAX_FIXED_PATH];
	V_MakeAbsolutePath( pFullPath, sizeof(pFullPath), path, NULL, k_bVPCForceLowerCase );

	// If Sys_CreatePath is called with a filename, all is well.
	// If it is called with a folder name, it must have a trailing slash:
	if ( !V_GetFileExtension( pFullPath ) )
		V_AppendSlash( pFullPath, sizeof(pFullPath) );

	char *ptr;

	// skip past the drive path, but don't strip
	if ( pFullPath[1] == ':' )
	{
		ptr = strchr( pFullPath, CORRECT_PATH_SEPARATOR );
	}
	else
	{
		ptr = pFullPath;
	}
	while ( ptr )
	{             
		ptr = strchr( ptr+1, CORRECT_PATH_SEPARATOR );
		if ( ptr )
		{
			*ptr = '\0';

#if defined( PLATFORM_WINDOWS )
			CreateDirectory( pFullPath, NULL );
#else
			mkdir( pFullPath, 0777 );
#endif
			*ptr = CORRECT_PATH_SEPARATOR;
		}
	}
}

//-----------------------------------------------------------------------------
// Enforce a minimal relative path.
//
// Need to do a repair on filenames that are valid to the os, but malformed according to msdev.
// This manifested in modules in MSDEV whose property pages in the UI were blank.
// MSDEV wants filenames that are exactly relative, i.e. no overlap.
//
//-----------------------------------------------------------------------------
bool Sys_ForceToMinimalRelativePath( const char *pBasePath, const char *pRelativeFilename, char *pOutputBuffer, int nOutputBufferSize )
{
	if ( V_IsAbsolutePath( pRelativeFilename ) )
	{
		// not a valid candidate for this fixup
		if ( pOutputBuffer != pRelativeFilename )
		{
			// provide as-is to caller for less outer logic
			V_strncpy( pOutputBuffer, pRelativeFilename, nOutputBufferSize );
		}
		return false;
	}

	// Form the expected full absolute path that may have redundant overlap.
	CUtlPathStringHolder expandedFilename;
	expandedFilename.ComposeFileName( pBasePath, pRelativeFilename );

	// Collapse any redundancy.
	expandedFilename.FixSlashesAndDotSlashes();

	// Re-relativize the path. This should yield the minimal relative path as desired.
	char newRelativeFilename[MAX_FIXED_PATH];
	V_MakeRelativePath( expandedFilename, g_pVPC->GetProjectPath(), newRelativeFilename, sizeof( newRelativeFilename ) );

	bool bFixed = ( V_stricmp_fast( pRelativeFilename, newRelativeFilename ) != 0 );
	if ( bFixed && g_pVPC->IsShowFixedPaths() )
	{
		g_pVPC->VPCWarning( "Fixed Redundant Pathing: %s -> %s", pRelativeFilename, newRelativeFilename );
	}

	V_strncpy( pOutputBuffer, newRelativeFilename, nOutputBufferSize );

	return bFixed;
}

//-----------------------------------------------------------------------------
// Given some arbitrary case filename, provides what the OS thinks it is.
// Windows specific. Returns false if file cannot be resolved (i.e. does not exist).
//-----------------------------------------------------------------------------
bool Sys_GetActualFilenameCase( const char *pFilename, char *pOutputBuffer, int nOutputBufferSize )
{
	V_strncpy( pOutputBuffer, pFilename, nOutputBufferSize );

#if defined( _WINDOWS )
	char filenameBuffer[MAX_FIXED_PATH];
	V_strncpy( filenameBuffer, pFilename, sizeof( filenameBuffer ) );
	V_RemoveDotSlashes( filenameBuffer );
	int nFilenameLength = V_strlen( filenameBuffer );

	CUtlString actualFilename;

	// march along filename, resolving up to next separator
	int nLastComponentStart = 0;
	bool bAddSeparator = false;
	int i = 0;
	while ( i < nFilenameLength )
	{
		// cannot resolve these, emit as-is		
		if ( !V_strnicmp( filenameBuffer + i, ".\\", 2 ) )
		{
			i += 2;
			actualFilename.Append( ".\\" );
			continue;
		}

		// cannot resolve these, emit as-is		
		if ( !V_strnicmp( filenameBuffer + i, "..\\", 3 ) )
		{
			i += 3;
			actualFilename.Append( "..\\" );
			continue;
		}

		// skip until path separator
		while ( i < nFilenameLength && filenameBuffer[i] != '\\' )
		{
			++i;
		}

		bool bFoundSeparator = ( i < nFilenameLength );

		// truncate at seperator, windows resolves each component in pieces
		filenameBuffer[i] = 0;
				
		SHFILEINFOA info = {0};
		HRESULT hr = SHGetFileInfoA( filenameBuffer, 0, &info, sizeof( info ), SHGFI_DISPLAYNAME );
		if ( SUCCEEDED( hr ) )
		{
			// reassemble based on actual component
			if ( bAddSeparator )
			{
				actualFilename.Append( '\\' );
			}
			actualFilename.Append( info.szDisplayName );
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
	// remove confusing ./ and normalize the provided filename separators for later comparison
	CUtlString filename = pFilename;
	filename.RemoveDotSlashes();

	if ( !Sys_GetActualFilenameCase( filename.Get(), pOutputBuffer, nOutputBufferSize ) )
		return false;

	if ( !V_strcmp( filename.Get(), pOutputBuffer ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool Sys_CopyToMirror( const char *pFilename )
{
	if ( !pFilename || !pFilename[0] )
		return false;

	const char *pMirrorPath = g_pVPC->GetOutputMirrorPath();
	if ( !pMirrorPath || !pMirrorPath[0] )
		return false;

	CUtlPathStringHolder absolutePathToOriginal;
	if ( V_IsAbsolutePath( pFilename ) )
	{
		absolutePathToOriginal.Set( pFilename );
	}
	else
	{
		// need to determine where file resides for mirroring
		char currentDirectory[MAX_FIXED_PATH];
		V_GetCurrentDirectory( currentDirectory, sizeof( currentDirectory ) );
		absolutePathToOriginal.ComposeFileName( currentDirectory, pFilename );
	}

	if ( !Sys_Exists( absolutePathToOriginal ) )
	{
		g_pVPC->VPCWarning( "Cannot mirror '%s', cannot resolve to expected '%s'", pFilename, absolutePathToOriginal.Get() );
		return false;
	}

	const char *pTargetPath = StringAfterPrefix( absolutePathToOriginal, g_pVPC->GetSourcePath() );
	if ( !pTargetPath || !pTargetPath[0] )
	{
		g_pVPC->VPCWarning( "Cannot mirror '%s', missing expected prefix '%s' in '%s'", pFilename, g_pVPC->GetSourcePath(), absolutePathToOriginal.Get() );
		return false;
	}

	// supply the mirror path head
	CUtlPathStringHolder absolutePathToMirror;
	if ( pTargetPath[0] == CORRECT_PATH_SEPARATOR )
	{
		pTargetPath++;
	}
	absolutePathToMirror.ComposeFileName( pMirrorPath, pTargetPath );

	Sys_CreatePath( absolutePathToMirror );

#ifdef _WIN32
	if ( !CopyFile( absolutePathToOriginal, absolutePathToMirror, FALSE ) )
	{
		g_pVPC->VPCWarning( "Cannot mirror '%s' to '%s'", absolutePathToOriginal.Get(), absolutePathToMirror.Get() );
		return false;
	}
	else
	{
		g_pVPC->VPCStatus( true, "Mirror: '%s' to '%s'", absolutePathToOriginal.Get(), absolutePathToMirror.Get() );
	}
#endif

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static const char* CModuleFileTypes[] =
{
	"c", "cpp", "cxx", "cc", GENERATED_CPP_FILE_EXTENSION
};
bool IsCFileExtension( const char *pExtension )
{
	if ( !pExtension || !pExtension[0] )
		return false;

	for ( int i = 0; i < V_ARRAYSIZE( CModuleFileTypes ); i++ )
	{
		if ( !V_stricmp_fast( CModuleFileTypes[i], pExtension ) )
			return true;
	}

	return false;
}

int GetNumCFileExtensions()
{
	return V_ARRAYSIZE( CModuleFileTypes );
}

const char *GetCFileExtension( int nIndex )
{
	if ( nIndex >= GetNumCFileExtensions() )
		return NULL;

	return CModuleFileTypes[ nIndex ];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static const char* HModuleFileTypes[] =
{
	"h", "hpp", "hxx", "hh"
};
bool IsHFileExtension( const char *pExtension )
{
	if ( !pExtension || !pExtension[0] )
		return false;

	for ( int i = 0; i < V_ARRAYSIZE( HModuleFileTypes ); i++ )
	{
		if ( !V_stricmp_fast( HModuleFileTypes[i], pExtension ) )
			return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
static bool IsSharedLibraryFile( const char *pFilename )
{
	const char *pExt = V_GetFileExtension( pFilename );
	if ( pExt && ( V_stricmp_fast( pExt, "so" ) == 0 || V_stricmp_fast( pExt, "dylib" ) == 0 || V_stricmp_fast( pExt, "dll" ) == 0 ) ) 
	{
		return true;
	}
	else
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool IsLibraryFile( const char *pFilename )
{
	const char *pExt = V_GetFileExtension( pFilename );
	return ( IsSharedLibraryFile( pFilename ) || ( pExt && ( V_stricmp_fast( pExt, "lib" ) == 0 || V_stricmp_fast( pExt, "a" ) == 0 ) ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool IsSourceFile( const char *pFilename )
{
	const char *pExt = V_GetFileExtension( pFilename );
	return ( pExt && ( IsCFileExtension( pExt ) || IsHFileExtension( pExt ) ||
					!V_stricmp_fast( pExt, "rc" ) || !V_stricmp_fast( pExt, "inc" ) ) );
}
