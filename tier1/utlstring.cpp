//========= Copyright (c) Valve Corporation, All rights reserved. =============
//
// Purpose: CUtlBinaryBlock and CUtlString implementation
//
//=============================================================================

#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "tier1/strtools.h"
#include <ctype.h>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static const int64 k_nMillion = 1000000;

//-----------------------------------------------------------------------------
// Purpose: Helper: Find s substring
//-----------------------------------------------------------------------------
static ptrdiff_t IndexOf(const char *pstrToSearch, const char *pstrTarget)
{
	const char *pstrHit = V_strstr(pstrToSearch, pstrTarget);
	if (pstrHit == NULL)
	{
		return -1;	// Not found.
	}
	return (pstrHit - pstrToSearch);
}


//-----------------------------------------------------------------------------
// Purpose: Helper: kill all whitespace.
//-----------------------------------------------------------------------------
static size_t RemoveWhitespace(char *pszString)
{
	if (pszString == NULL)
		return 0;

	char *pstrDest = pszString;
	size_t cRemoved = 0;
	for (char *pstrWalker = pszString; *pstrWalker != 0; pstrWalker++)
	{
		if (!V_isspace((unsigned char)*pstrWalker))
		{
			*pstrDest = *pstrWalker;
			pstrDest++;
		}
		else
			cRemoved += 1;
	}
	*pstrDest = 0;

	return cRemoved;
}

int V_vscprintf(const char *format, va_list params)
{
#ifdef _WIN32
	return _vscprintf(format, params);
#else
	return vsnprintf(NULL, 0, format, params);
#endif
}

//-----------------------------------------------------------------------------
// Base class, containing simple memory management
//-----------------------------------------------------------------------------
CUtlBinaryBlock::CUtlBinaryBlock( int growSize, int initSize ) 
{
	MEM_ALLOC_CREDIT();
	m_Memory.Init( growSize, initSize );

	m_nActualLength = 0;
}

CUtlBinaryBlock::CUtlBinaryBlock( void* pMemory, int nSizeInBytes, int nInitialLength ) : m_Memory( (unsigned char*)pMemory, nSizeInBytes )
{
	m_nActualLength = nInitialLength;
}

CUtlBinaryBlock::CUtlBinaryBlock( const void* pMemory, int nSizeInBytes ) : m_Memory( (const unsigned char*)pMemory, nSizeInBytes )
{
	m_nActualLength = nSizeInBytes;
}

CUtlBinaryBlock::CUtlBinaryBlock( const CUtlBinaryBlock& src )
{
	Set( src.Get(), src.Length() );
}

void CUtlBinaryBlock::Get( void *pValue, int nLen ) const
{
	Assert( nLen > 0 );
	if ( m_nActualLength < nLen )
	{
		nLen = m_nActualLength;
	}

	if ( nLen > 0 )
	{
		memcpy( pValue, m_Memory.Base(), nLen );
	}
}

void CUtlBinaryBlock::SetLength( int nLength )
{
	MEM_ALLOC_CREDIT();
	Assert( !m_Memory.IsReadOnly() );

	m_nActualLength = nLength;
	if ( nLength > m_Memory.NumAllocated() )
	{
		int nOverFlow = nLength - m_Memory.NumAllocated();
		m_Memory.Grow( nOverFlow );

		// If the reallocation failed, clamp length
		if ( nLength > m_Memory.NumAllocated() )
		{
			m_nActualLength = m_Memory.NumAllocated();
		}
	}

#ifdef _DEBUG
	if ( m_Memory.NumAllocated() > m_nActualLength )
	{
		memset( ( ( char * )m_Memory.Base() ) + m_nActualLength, 0xEB, m_Memory.NumAllocated() - m_nActualLength );
	}
#endif
}


void CUtlBinaryBlock::Set( const void *pValue, int nLen )
{
	Assert( !m_Memory.IsReadOnly() );

	if ( !pValue )
	{
		nLen = 0;
	}

	SetLength( nLen );

	if ( m_nActualLength )
	{
		if ( ( ( const char * )m_Memory.Base() ) >= ( ( const char * )pValue ) + nLen ||
			 ( ( const char * )m_Memory.Base() ) + m_nActualLength <= ( ( const char * )pValue ) )
		{
			memcpy( m_Memory.Base(), pValue, m_nActualLength );
		}
		else
		{
			memmove( m_Memory.Base(), pValue, m_nActualLength );
		}
	}
}


CUtlBinaryBlock &CUtlBinaryBlock::operator=( const CUtlBinaryBlock &src )
{
	Assert( !m_Memory.IsReadOnly() );
	Set( src.Get(), src.Length() );
	return *this;
}


bool CUtlBinaryBlock::operator==( const CUtlBinaryBlock &src ) const
{
	if ( src.Length() != Length() )
		return false;

	return !memcmp( src.Get(), Get(), Length() );
}


//-----------------------------------------------------------------------------
// Simple string class. 
//-----------------------------------------------------------------------------
CUtlString::CUtlString()
{
}

CUtlString::CUtlString( const char *pString )
{
	Set( pString );
}

CUtlString::CUtlString( const CUtlString& string )
{
	Set( string.Get() );
}

// Attaches the string to external memory. Useful for avoiding a copy
CUtlString::CUtlString( void* pMemory, int nSizeInBytes, int nInitialLength ) : m_Storage( pMemory, nSizeInBytes, nInitialLength )
{
}

CUtlString::CUtlString( const void* pMemory, int nSizeInBytes ) : m_Storage( pMemory, nSizeInBytes )
{
}


//-----------------------------------------------------------------------------
// Purpose: Set directly and don't look for a null terminator in pValue.
//-----------------------------------------------------------------------------
void CUtlString::SetDirect( const char *pValue, int nChars )
{
	if ( nChars > 0 )
	{
		m_Storage.SetLength( nChars+1 );
		m_Storage.Set( pValue, nChars+1 );
		m_Storage[nChars] = 0;
	}
	else
	{
		m_Storage.SetLength( 0 );
	}
}


void CUtlString::Set( const char *pValue )
{
	Assert( !m_Storage.IsReadOnly() );
	int nLen = pValue ? Q_strlen(pValue) + 1 : 0;
	m_Storage.Set( pValue, nLen );
}


// Returns strlen
int CUtlString::Length() const
{
	return m_Storage.Length() ? m_Storage.Length() - 1 : 0;
}

// Sets the length (used to serialize into the buffer )
void CUtlString::SetLength( int nLen )
{
	Assert( !m_Storage.IsReadOnly() );

	// Add 1 to account for the NULL
	m_Storage.SetLength( nLen > 0 ? nLen + 1 : 0 );
}

const char *CUtlString::Get( ) const
{
	if ( m_Storage.Length() == 0 )
	{
		return "";
	}

	return reinterpret_cast< const char* >( m_Storage.Get() );
}

char *CUtlString::Get()
{
	Assert( !m_Storage.IsReadOnly() );

	if ( m_Storage.Length() == 0 )
	{
		// In general, we optimise away small mallocs for empty strings
		// but if you ask for the non-const bytes, they must be writable
		// so we can't return "" here, like we do for the const version - jd
		m_Storage.SetLength( 1 );
		m_Storage[ 0 ] = '\0';
	}

	return reinterpret_cast< char* >( m_Storage.Get() );
}

char *CUtlString::GetForModify()
{
	return Get();
}

void CUtlString::Purge()
{
	m_Storage.Purge();
}

void CUtlString::ToUpper()
{
	for (int nLength = Length() - 1; nLength >= 0; nLength--)
	{
		m_Storage[nLength] = toupper(m_Storage[nLength]);
	}
}

void CUtlString::ToLower()
{
	for( int nLength = Length() - 1; nLength >= 0; nLength-- )
	{
		m_Storage[ nLength ] = tolower( m_Storage[ nLength ] );
	}
}


CUtlString &CUtlString::operator=( const CUtlString &src )
{
	Assert( !m_Storage.IsReadOnly() );
	m_Storage = src.m_Storage;
	return *this;
}

CUtlString &CUtlString::operator=( const char *src )
{
	Assert( !m_Storage.IsReadOnly() );
	Set( src );
	return *this;
}

bool CUtlString::operator==( const CUtlString &src ) const
{
	return m_Storage == src.m_Storage;
}

bool CUtlString::operator==( const char *src ) const
{
	return ( strcmp( Get(), src ) == 0 );
}

CUtlString &CUtlString::operator+=( const CUtlString &rhs )
{
	Assert( !m_Storage.IsReadOnly() );

	const int lhsLength( Length() );
	const int rhsLength( rhs.Length() );
	const int requestedLength( lhsLength + rhsLength );

	SetLength( requestedLength );
	const int allocatedLength( Length() );
	const int copyLength( allocatedLength - lhsLength < rhsLength ? allocatedLength - lhsLength : rhsLength );
	memcpy( Get() + lhsLength, rhs.Get(), copyLength );
	m_Storage[ allocatedLength ] = '\0';

	return *this;
}

CUtlString &CUtlString::operator+=( const char *rhs )
{
	Assert( !m_Storage.IsReadOnly() );

	const int lhsLength( Length() );
	const int rhsLength( Q_strlen( rhs ) );
	const int requestedLength( lhsLength + rhsLength );

	SetLength( requestedLength );
	const int allocatedLength( Length() );
	const int copyLength( allocatedLength - lhsLength < rhsLength ? allocatedLength - lhsLength : rhsLength );
	memcpy( Get() + lhsLength, rhs, copyLength );
	m_Storage[ allocatedLength ] = '\0';

	return *this;
}

CUtlString &CUtlString::operator+=( char c )
{
	Assert( !m_Storage.IsReadOnly() );

	int nLength = Length();
	SetLength( nLength + 1 );
	m_Storage[ nLength ] = c;
	m_Storage[ nLength+1 ] = '\0';
	return *this;
}

CUtlString &CUtlString::operator+=( int rhs )
{
	Assert( !m_Storage.IsReadOnly() );
	Assert( sizeof( rhs ) == 4 );

	char tmpBuf[ 12 ];	// Sufficient for a signed 32 bit integer [ -2147483648 to +2147483647 ]
	Q_snprintf( tmpBuf, sizeof( tmpBuf ), "%d", rhs );
	tmpBuf[ sizeof( tmpBuf ) - 1 ] = '\0';

	return operator+=( tmpBuf );
}

CUtlString &CUtlString::operator+=( double rhs )
{
	Assert( !m_Storage.IsReadOnly() );

	char tmpBuf[ 256 ];	// How big can doubles be???  Dunno.
	Q_snprintf( tmpBuf, sizeof( tmpBuf ), "%lg", rhs );
	tmpBuf[ sizeof( tmpBuf ) - 1 ] = '\0';

	return operator+=( tmpBuf );
}

bool CUtlString::MatchesPattern( const CUtlString &Pattern, int nFlags )
{
	const char *pszSource = String();
	const char *pszPattern = Pattern.String();

	return V_StringMatchesPattern( pszSource, pszPattern, nFlags );
}


int CUtlString::Format( const char *pFormat, ... )
{
	va_list marker;

	va_start( marker, pFormat );
	int len = FormatV( pFormat, marker );
	va_end( marker );

	return len;
}

//--------------------------------------------------------------------------------------------------
// This can be called from functions that take varargs.
//--------------------------------------------------------------------------------------------------

int CUtlString::FormatV( const char *pFormat, va_list marker )
{
	char tmpBuf[ 4096 ];	//< Nice big 4k buffer, as much memory as my first computer had, a Radio Shack Color Computer

	//va_start( marker, pFormat );
	int len = V_vsprintf_safe( tmpBuf, pFormat, marker );
	//va_end( marker );
	Set( tmpBuf );
	return len;
}

//-----------------------------------------------------------------------------
// Strips the trailing slash
//-----------------------------------------------------------------------------
void CUtlString::StripTrailingSlash()
{
	if ( IsEmpty() )
		return;

	int nLastChar = Length() - 1;
	char c = m_Storage[ nLastChar ];
	if ( c == '\\' || c == '/' )
	{
		m_Storage[ nLastChar ] = 0;
		m_Storage.SetLength( m_Storage.Length() - 1 );
	}
}

CUtlString CUtlString::Slice( int32 nStart, int32 nEnd )
{
	if ( nStart < 0 )
		nStart = Length() - (-nStart % Length());
	else if ( nStart >= Length() )
		nStart = Length();

	if ( nEnd == INT32_MAX )
		nEnd = Length();
	else if ( nEnd < 0 )
		nEnd = Length() - (-nEnd % Length());
	else if ( nEnd >= Length() )
		nEnd = Length();
	
	if ( nStart >= nEnd )
		return CUtlString( "" );

	const char *pIn = String();

	CUtlString ret;
	ret.m_Storage.SetLength( nEnd - nStart + 1 );
	char *pOut = (char*)ret.m_Storage.Get();

	memcpy( ret.m_Storage.Get(), &pIn[nStart], nEnd - nStart );
	pOut[nEnd - nStart] = 0;

	return ret;
}

// Grab a substring starting from the left or the right side.
CUtlString CUtlString::Left( int32 nChars )
{
	return Slice( 0, nChars );
}

CUtlString CUtlString::Right( int32 nChars )
{
	return Slice( -nChars );
}

// Get a string with the specified substring removed

CUtlString CUtlString::Remove( char const *pTextToRemove, bool bCaseSensitive ) const
{
	int nTextToRemoveLength = pTextToRemove ? V_strlen( pTextToRemove ) : 0;
	CUtlString outputString;
	const char *pSrc = Get();
	if ( pSrc )
	{
		while ( *pSrc )
		{
			char const *pNextOccurrence = bCaseSensitive ? V_strstr( pSrc, pTextToRemove ) : V_stristr( pSrc, pTextToRemove );
			if ( !pNextOccurrence )
			{
				// append remaining string
				outputString += pSrc;
				break;
			}

			int nNumCharsToCopy = pNextOccurrence - pSrc;
			if ( nNumCharsToCopy )
			{
				// append up to the undesired substring
				CUtlString temp = pSrc;
				temp = temp.Left( nNumCharsToCopy );
				outputString += temp;
			}

			// skip past undesired substring
			pSrc = pNextOccurrence + nTextToRemoveLength;
		}
	}

	return outputString;
}

CUtlString CUtlString::Replace( char const *pchFrom, const char *pchTo, bool bCaseSensitive /*= false*/ ) const
{
	if ( !pchTo )
	{
		return Remove( pchFrom, bCaseSensitive );
	}

	int nTextToReplaceLength = pchFrom ? V_strlen( pchFrom ) : 0;
	CUtlString outputString;
	const char *pSrc = Get();
	if ( pSrc )
	{
		while ( *pSrc )
		{
			char const *pNextOccurrence = bCaseSensitive ? V_strstr( pSrc, pchFrom ) : V_stristr( pSrc, pchFrom );
			if ( !pNextOccurrence )
			{
				// append remaining string
				outputString += pSrc;
				break;
			}

			int nNumCharsToCopy = pNextOccurrence - pSrc;
			if ( nNumCharsToCopy )
			{
				// append up to the undesired substring
				CUtlString temp = pSrc;
				temp = temp.Left( nNumCharsToCopy );
				outputString += temp;
			}

			// Append the replacement
			outputString += pchTo;

			// skip past undesired substring
			pSrc = pNextOccurrence + nTextToReplaceLength;
		}
	}

	return outputString;

}



CUtlString CUtlString::Replace( char cFrom, char cTo )
{
	CUtlString ret = *this;
	int len = ret.Length();
	for ( int i=0; i < len; i++ )
	{
		if ( ret.m_Storage[i] == cFrom )
			ret.m_Storage[i] = cTo;
	}

	return ret;
}

void CUtlString::RemoveDotSlashes(char separator)
{
	V_RemoveDotSlashes(Get(), separator);
}

void CUtlString::Swap( CUtlString &src )
{
	CUtlString tmp = src;
	src = *this;
	*this = tmp;
}


CUtlString CUtlString::AbsPath( const char *pStartingDir ) const
{
	char szNew[MAX_PATH];
	V_MakeAbsolutePath( szNew, sizeof( szNew ), this->String(), pStartingDir );
	return CUtlString( szNew );
}

CUtlString CUtlString::UnqualifiedFilename() const
{
	const char *pFilename = V_UnqualifiedFileName( this->String() );
	return CUtlString( pFilename );
}

CUtlString CUtlString::DirName()
{
	CUtlString ret( this->String() );
	V_StripLastDir( (char*)ret.m_Storage.Get(), ret.m_Storage.Length() );
	V_StripTrailingSlash( (char*)ret.m_Storage.Get() );
	return ret;
}

CUtlString CUtlString::StripExtension() const
{
	char szTemp[MAX_FILEPATH];
	V_StripExtension( String(), szTemp, sizeof( szTemp ) );
	return CUtlString( szTemp );
}

CUtlString CUtlString::StripFilename() const
{
	const char *pFilename = V_UnqualifiedFileName( Get() ); // NOTE: returns 'Get()' on failure, never NULL
	int nCharsToCopy = pFilename - Get();
	CUtlString result;
	result.SetDirect( Get(), nCharsToCopy );
	result.StripTrailingSlash();
	return result;
}

CUtlString CUtlString::GetBaseFilename() const
{
	char szTemp[MAX_FILEPATH];
	V_FileBase( String(), szTemp, sizeof( szTemp ) );
	return CUtlString( szTemp );
}

CUtlString CUtlString::GetExtension() const
{
	char szTemp[MAX_FILEPATH];
	V_ExtractFileExtension( String(), szTemp, sizeof( szTemp ) );
	return CUtlString( szTemp );
}


CUtlString CUtlString::PathJoin( const char *pStr1, const char *pStr2 )
{
	char szPath[MAX_PATH];
	V_ComposeFileName( pStr1, pStr2, szPath, sizeof( szPath ) );
	return CUtlString( szPath );
}


//-----------------------------------------------------------------------------
// Purpose: concatenate the provided string to our current content
//-----------------------------------------------------------------------------
void CUtlString::Append( const char *pchAddition )
{
	(*this) += pchAddition;
}

void CUtlString::Append(const char *pchAddition, int nMaxChars)
{
	const int nLen = V_strlen(pchAddition);
	if (nMaxChars < 0 || nLen <= nMaxChars)
	{
		Append(pchAddition);
	}
	else
	{
		char* pchAdditionDup = V_strdup(pchAddition);
		pchAdditionDup[nMaxChars] = 0;
		Append(pchAdditionDup);
		delete[] pchAdditionDup;
	}
}

//--------------------------------------------------------------------------------------------------
// Trim
//--------------------------------------------------------------------------------------------------

void CUtlString::TrimLeft( const char *szTargets )
{
	int i;

	if ( IsEmpty() )
	{
		return;
	}

	char* pSrc = Get();

	for ( i = 0; pSrc[ i ] != 0; i++ )
	{
		bool bWhitespace = false;

		for ( int j = 0; szTargets[ j ] != 0; j++ )
		{
			if ( pSrc[ i ] == szTargets[ j ] )
			{
				bWhitespace = true;
				break;
			}
		}

		if ( !bWhitespace )
		{
			break;
		}
	}

	// We have some whitespace to remove
	if ( i > 0 )
	{
		memmove( pSrc, &pSrc[ i ], Length() - i );
		SetLength( Length() - i );
		m_Storage[ Length() ] = '\0';
	}
}

void CUtlString::TrimRight( const char *szTargets )
{
	const int nLastCharIndex = Length() - 1;
	int i;

	char* pSrc = Get();

	for ( i = nLastCharIndex; i >= 0; i-- )
	{
		bool bWhitespace = false;

		for ( int j = 0; szTargets[ j ] != 0; j++ )
		{
			if ( pSrc[ i ] == szTargets[ j ] )
			{
				bWhitespace = true;
				break;
			}
		}

		if ( !bWhitespace )
		{
			break;
		}
	}

	// We have some whitespace to remove
	if ( i < nLastCharIndex )
	{
		pSrc[ i + 1 ] = 0;
		SetLength( i + 1 );
	}
}

void CUtlString::Trim( const char *szTargets )
{
	TrimLeft( szTargets );
	TrimRight( szTargets );
}


//-----------------------------------------------------------------------------
// Purpose: spill routine for making sure our buffer is big enough for an
//			incoming string set/modify.
//-----------------------------------------------------------------------------
char *CUtlStringBuilder::InternalPrepareBuffer(size_t nChars, bool bCopyOld, size_t nMinCapacity)
{
	Assert(nMinCapacity > Capacity());
	Assert(nMinCapacity >= nChars);
	// Don't use this class if you want a single 2GB+ string.
	static const size_t k_nMaxStringSize = 0x7FFFFFFFu;
	Assert(nMinCapacity <= k_nMaxStringSize);

	if (nMinCapacity > k_nMaxStringSize)
	{
		SetError();
		return NULL;
	}

	bool bWasHeap = m_data.IsHeap();
	// add this to whatever we are going to grow so we don't start out too slow
	char *pszString = NULL;
	if (nMinCapacity > MAX_STACK_STRLEN)
	{
		// Allocate 1.5 times what is requested, plus a small initial ramp
		// value so we don't spend too much time re-allocating tiny buffers.
		// A good allocator will prevent this anyways, but this makes it safer.
		// We cap it at +1 million to not get crazy.  Code actually avoides
		// computing power of two numbers since allocations almost always
		// have header/bookkeeping overhead. Don't do the dynamic sizing
		// if the user asked for a specific capacity.
		static const int k_nInitialMinRamp = 32;
		size_t nNewSize;
		if (nMinCapacity > nChars)
			nNewSize = nMinCapacity;
		else
			nNewSize = nChars + Min<size_t>((nChars >> 1) + k_nInitialMinRamp, k_nMillion);

		char *pszOld = m_data.Access();
		size_t nLenOld = m_data.Length();

		// order of operations is very important per comment
		// above. Make sure we copy it before changing m_data
		// in any way
		if (bWasHeap && bCopyOld)
		{
			// maybe we'll get lucky and get the same buffer back.
			pszString = (char*)realloc(pszOld, nNewSize + 1);
			if (!pszString)
			{
				SetError();
				return NULL;
			}
		}
		else // Either it's already on the stack; or we don't need to copy
		{
			// if the current pointer is on the heap, we aren't doing a copy
			// (or we would have used the previous realloc code. So
			// if we aren't doing a copy, don't use realloc since it will
			// copy the data if it needs to make a new allocation.
			if (bWasHeap)
				free(pszOld);

			pszString = (char*)malloc(nNewSize + 1);
			if (!pszString)
			{
				SetError();
				return NULL;
			}

			// still need to do the copy if we are going from small buffer to large
			if (bCopyOld)
				memcpy(pszString, pszOld, nLenOld); // null will be added at end of func.
		}

		// just in case the user grabs .Access() and scribbles over the terminator at
		// 'length', make sure they don't run off the rails as long as they obey Capacity.
		// We don't offer this protection for the 'on stack' string.
		pszString[nNewSize] = '\0';

		m_data.Heap.m_pchString = pszString;
		m_data.Heap.m_nCapacity = (uint32)nNewSize; // capacity is the max #chars, not including the null.
		m_data.Heap.m_nLength = (uint32)nChars;
		m_data.Heap.sentinel = STRING_TYPE_SENTINEL;
	}
	else
	{
		// Rare case. Only happens if someone did a SetPtr with a length
		// less than MAX_STACK_STRLEN, or maybe a .Replace() shrunk the
		// length down.
		pszString = m_data.Stack.m_szString;
		m_data.Stack.SetBytesLeft(MAX_STACK_STRLEN - (uint8)nChars);

		if (bWasHeap)
		{
			char *pszOldString = m_data.Heap.m_pchString;
			if (bCopyOld)
				memcpy(pszString, pszOldString, nChars); // null will be added at end of func.

			free(pszOldString);
		}
	}

	pszString[nChars] = '\0';
	return pszString;
}


//-----------------------------------------------------------------------------
// Purpose: replace all occurrences of one string with another
//			replacement string may be NULL or "" to remove target string
//-----------------------------------------------------------------------------
size_t CUtlStringBuilder::Replace(const char *pstrTarget, const char *pstrReplacement)
{
	return ReplaceInternal(pstrTarget, pstrReplacement, (const char *(*)(const char *, const char *))_V_strstr);
}


//-----------------------------------------------------------------------------
// Purpose: replace all occurrences of one string with another
//			replacement string may be NULL or "" to remove target string
//-----------------------------------------------------------------------------
size_t CUtlStringBuilder::ReplaceFastCaseless(const char *pstrTarget, const char *pstrReplacement)
{
	return ReplaceInternal(pstrTarget, pstrReplacement, V_stristr_fast);
}


//-----------------------------------------------------------------------------
// Purpose: replace all occurrences of one string with another
//			replacement string may be NULL or "" to remove target string
//-----------------------------------------------------------------------------
size_t CUtlStringBuilder::ReplaceInternal(const char *pstrTarget, const char *pstrReplacement, const char *pfnCompare(const char*, const char*))
{
	if (HasError())
		return 0;

	if (pstrReplacement == NULL)
		pstrReplacement = "";

	size_t nTargetLength = V_strlen(pstrTarget);
	size_t nReplacementLength = V_strlen(pstrReplacement);

	CUtlVector<const char *> vecMatches;
	vecMatches.EnsureCapacity(8);

	if (!IsEmpty() && pstrTarget && *pstrTarget)
	{
		char *pszString = Access();

		// walk the string counting hits
		const char *pstrHit = pszString;
		for (pstrHit = pfnCompare(pstrHit, pstrTarget); pstrHit != NULL && *pstrHit != 0; /* inside */)
		{
			vecMatches.AddToTail(pstrHit);
			// look for the next target and keep looping
			pstrHit = pfnCompare(pstrHit + nTargetLength, pstrTarget);
		}

		// if we didn't miss, get to work
		if (vecMatches.Count() > 0)
		{
			// reallocate only once; how big will we need?
			size_t nOldLength = Length();
			size_t nNewLength = nOldLength + (vecMatches.Count() * (int)(nReplacementLength - nTargetLength));

			if (nNewLength == 0)
			{
				// shortcut simple case, even if rare
				m_data.Clear();
			}
			else if (nNewLength > nOldLength)
			{
				// New string will be bigger than the old, but don't re-alloc unless
				// it is also larger than capacity.  If it fits in capacity, we will
				// be adjusting the string 'in place'.  The replacement string is larger
				// than the target string, so if we copied front to back we would screw up
				// the existing data in the 'in place' case.
				char *pstrNew;
				if (nNewLength > Capacity())
				{
					pstrNew = (char*)malloc(nNewLength + 1);
					if (!pstrNew)
					{
						SetError();
						return 0;
					}
				}
				else
				{
					pstrNew = PrepareBuffer(nNewLength);
					Assert(pstrNew == pszString);
				}

				const char *pstrPreviousHit = pszString + nOldLength; // end of original string
				char *pstrDestination = pstrNew + nNewLength; // end of target
				*pstrDestination = '\0';
				// Go backwards as noted above.
				FOR_EACH_VEC_BACK(vecMatches, i)
				{
					pstrHit = vecMatches[i];
					size_t nRemainder = pstrPreviousHit - (pstrHit + nTargetLength);
					// copy the bit after the match, back up the destination and move forward from the hit
					memmove(pstrDestination - nRemainder, pstrPreviousHit - nRemainder, nRemainder);
					pstrDestination -= (nRemainder + nReplacementLength);

					// push the replacement string in
					memcpy(pstrDestination, pstrReplacement, nReplacementLength);
					pstrPreviousHit = pstrHit;
				}

				// copy trailing stuff
				size_t nRemainder = pstrPreviousHit - pszString;
				pstrDestination -= nRemainder;
				if (pstrDestination != pszString)
				{
					memmove(pstrDestination, pszString, nRemainder);
				}

				Assert(pstrNew == pstrDestination);

				// Need to set the pointer if we did were larger than capacity.
				if (pstrNew != pszString)
					SetPtr(pstrNew, nNewLength);
			}
			else // new is shorter than or same length as old, move in place
			{
				char *pstrNew = Access();
				char *pstrPreviousHit = pstrNew;
				char *pstrDestination = pstrNew;
				FOR_EACH_VEC(vecMatches, i)
				{
					pstrHit = vecMatches[i];
					if (pstrDestination != pstrPreviousHit)
					{
						// memmove very important as it is ok with overlaps.
						memmove(pstrDestination, pstrPreviousHit, pstrHit - pstrPreviousHit);
					}
					pstrDestination += (pstrHit - pstrPreviousHit);
					memcpy(pstrDestination, pstrReplacement, nReplacementLength);
					pstrDestination += nReplacementLength;
					pstrPreviousHit = const_cast<char*>(pstrHit)+nTargetLength;
				}

				// copy trailing stuff
				if (pstrDestination != pstrPreviousHit)
				{
					// memmove very important as it is ok with overlaps.
					size_t nRemainder = (pstrNew + nOldLength) - pstrPreviousHit;
					memmove(pstrDestination, pstrPreviousHit, nRemainder);
				}

				Verify(PrepareBuffer(nNewLength) == pstrNew);
			}

		}
	}

	return vecMatches.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Indicates if the target string exists in this instance.
//			The index is negative if the target string is not found, otherwise it is the index in the string.
//-----------------------------------------------------------------------------
ptrdiff_t CUtlStringBuilder::IndexOf(const char *pstrTarget) const
{
	return ::IndexOf(String(), pstrTarget);
}


//-----------------------------------------------------------------------------
// Purpose: 
//			remove whitespace -- anything that is isspace() -- from the string
//-----------------------------------------------------------------------------
size_t CUtlStringBuilder::RemoveWhitespace()
{
	if (HasError())
		return 0;

	char *pstrDest = m_data.Access();
	size_t cRemoved = ::RemoveWhitespace(pstrDest);

	size_t nNewLength = m_data.Length() - cRemoved;

	if (cRemoved)
		m_data.SetLength(nNewLength);

	Assert(pstrDest[nNewLength] == '\0'); // SetLength should have set this

	return cRemoved;
}


//-----------------------------------------------------------------------------
// Purpose:	Allows setting the size to anything under the current
//			capacity.  Typically should not be used unless there was a specific
//			reason to scribble on the string. Will not touch the string contents,
//			but will append a NULL. Returns true if the length was changed.
//-----------------------------------------------------------------------------
bool CUtlStringBuilder::SetLength(size_t nLen)
{
	return m_data.SetLength(nLen) != NULL;
}


//-----------------------------------------------------------------------------
// Purpose:	Convert to heap string if needed, and give it away.
//-----------------------------------------------------------------------------
char *CUtlStringBuilder::TakeOwnership(size_t *pnLen, size_t *pnCapacity)
{
	size_t nLen = 0;
	size_t nCapacity = 0;
	char *psz = m_data.TakeOwnership(nLen, nCapacity);

	if (pnLen)
		*pnLen = nLen;

	if (pnCapacity)
		*pnCapacity = nCapacity;

	return psz;
}


//-----------------------------------------------------------------------------
// Purpose: 
//			trim whitespace from front and back of string
//-----------------------------------------------------------------------------
size_t CUtlStringBuilder::TrimWhitespace()
{
	if (HasError())
		return 0;

	char *pchString = m_data.Access();
	int cChars = V_StrTrim(pchString);

	if (cChars)
		m_data.SetLength(cChars);

	return cChars;
}

//-----------------------------------------------------------------------------
// Purpose: adjust length and add null terminator, within capacity bounds
//-----------------------------------------------------------------------------
char *CUtlStringBuilder::Data::SetLength(size_t nChars)
{
	// heap/stack must be set correctly, and will not
	// be changed by this routine.
	if (IsHeap())
	{
		if (!Heap.m_pchString || nChars > Heap.m_nCapacity)
			return NULL;
		Heap.m_nLength = (uint32)nChars;
		Heap.m_pchString[nChars] = '\0';
		return Heap.m_pchString;
	}
	if (nChars > MAX_STACK_STRLEN)
		return NULL;
	Stack.m_szString[nChars] = '\0';
	Stack.SetBytesLeft(MAX_STACK_STRLEN - (uint8)nChars);
	return Stack.m_szString;
}

//-----------------------------------------------------------------------------
// Purpose:	Allows setting the raw pointer and taking ownership
//-----------------------------------------------------------------------------
void CUtlStringBuilder::Data::SetPtr(char *pchString, size_t nLength)
{
	// We don't care about the error state since we are totally replacing
	// the string.

	// ok, length may be small enough to fit in our short buffer
	// but we've already got a dynamically allocated string, so let
	// it be in the heap buffer anyways.
	Heap.m_pchString = pchString;
	Heap.m_nCapacity = (uint32)nLength;
	Heap.m_nLength = (uint32)nLength;
	Heap.sentinel = STRING_TYPE_SENTINEL;

	// their buffer must have room for the null
	Heap.m_pchString[nLength] = '\0';
}


//-----------------------------------------------------------------------------
// Purpose:	Enable the error state, moving the string to the heap if
//			it isn't there.
//-----------------------------------------------------------------------------
void CUtlStringBuilder::Data::SetError(bool bEnableAssert)
{
	if (HasError())
		return;

	// This is not meant to be used as a status bit. Setting the error state should
	// mean something very unexpected happened that you would want a call stack for.
	// That is why this asserts unconditionally when the state is being flipped.
	if (bEnableAssert)
		AssertMsg(false, "Error State on string being set.");

	MoveToHeap();

	Heap.sentinel = (STRING_TYPE_SENTINEL | STRING_TYPE_ERROR);
}


//-----------------------------------------------------------------------------
// Purpose:	Set string to empty state
//-----------------------------------------------------------------------------
void CUtlStringBuilder::Data::ClearError()
{
	if (HasError())
	{
		Heap.sentinel = STRING_TYPE_SENTINEL;
		Clear();
	}
}


//-----------------------------------------------------------------------------
// Purpose:	If the string is on the stack, move it to the heap.
//			create a null heap string if memory can't be allocated.
//			Callers of this /need/ the string to be in the heap state
//			when done.
//-----------------------------------------------------------------------------
bool CUtlStringBuilder::Data::MoveToHeap()
{
	bool bSuccess = true;

	if (!IsHeap())
	{
		// try to recover the string at the point of failure, to help with debugging
		size_t nLen = Length();
		char *pszHeapString = (char*)malloc(nLen + 1);
		if (pszHeapString)
		{
			// get the string copy before corrupting the stack union
			char *pszStackString = Access();
			memcpy(pszHeapString, pszStackString, nLen);
			pszHeapString[nLen] = 0;

			Heap.m_pchString = pszHeapString;
			Heap.m_nLength = (uint32)nLen;
			Heap.m_nCapacity = (uint32)nLen;
			Heap.sentinel = STRING_TYPE_SENTINEL;
		}
		else
		{
			Heap.m_pchString = NULL;
			Heap.m_nLength = 0;
			Heap.m_nCapacity = 0;
			bSuccess = false;
			Heap.sentinel = (STRING_TYPE_SENTINEL | STRING_TYPE_ERROR);
		}

	}

	return bSuccess;
}
