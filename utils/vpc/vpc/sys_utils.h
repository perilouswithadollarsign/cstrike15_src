//======= Copyright 1996-2016, Valve Corporation, All rights reserved. ======//
//
//	File Utilities.
//
//===========================================================================//

#pragma once

#define _CRT_SECURE_NO_DEPRECATE 1

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#ifdef POSIX
#include <unistd.h>
#endif

#if defined( LINUX ) || defined( _LINUX )
	#include <sys/io.h>
#endif

#include "tier0/platform.h"
#include "../vpccrccheck/crccheck_shared.h"

template< class T, class NullType, int nMax >
class CSimplePointerStack
{
public:
	inline CSimplePointerStack()
	{
		m_nCount = 0;
	}

	inline void Purge()
	{
		for ( int i=0; i < m_nCount; i++ )
			m_Values[i] = (NullType)NULL;
		m_nCount = 0;
	}

	inline int Count()
	{
		return m_nCount;
	}

	inline T& Top()
	{
		Assert( m_nCount > 0 );
		return m_Values[m_nCount-1];
	}

	inline void Pop( T &val )
	{
		Assert( m_nCount > 0 );
		--m_nCount;
		val = m_Values[m_nCount];
		m_Values[m_nCount] = (NullType)NULL;
	}

	inline void Pop()
	{
		Assert( m_nCount > 0 );
		--m_nCount;
		m_Values[m_nCount] = (NullType)NULL;
	}

	inline void Push( T &val )
	{
		Assert( m_nCount+1 < nMax );
		m_Values[m_nCount] = val;
		++m_nCount;
	}

public:
	T m_Values[nMax];
	int m_nCount;
};

enum XMLOmitIfEmpty_t
{
	XMLOIE_NEVER_OMIT,
	XMLOIE_OMIT_ON_EMPTY_CONTENTS,
	XMLOIE_OMIT_ON_EMPTY_EVERYTHING,
};

class CXMLWriter
{
public:
	CXMLWriter();

	bool		Open( const char *pFilename, bool bIs2010Format, bool bForceWrite );
	bool		Close();

	void		PushNode( const char *pName, XMLOmitIfEmpty_t omitSetting = XMLOIE_NEVER_OMIT );
	void		PopNode( void );

	void		AddNodeProperty( const char *pPropertyName, const char *pValue );
	void		AddNodeProperty( const char *pString );

	void		WriteLineNode( const char *pName, const char *pExtra, const char *pString, XMLOmitIfEmpty_t omitSetting = XMLOIE_NEVER_OMIT );
	void		PushNode( const char *pName, const char *pString );

	void		Write( const char *p );
	CUtlString	FixupXMLString( const char *pInput );

private:
	void	Indent( int nReduceDepth = 0 );
	bool	FinishPush( bool bCloseAngleBracket );

	bool				m_b2010Format;
	bool				m_bForceWrite;
	CUtlBuffer			m_WriteBuffer;

	CUtlString			m_FilenameString;

	struct Node_t
	{
		Node_t( void ) : m_bHasFinishedPush( false ), m_omitOption( XMLOIE_NEVER_OMIT ) {}

		CUtlString m_Name;
		CUtlVector< CUtlString > m_PropertyStrings;
		bool m_bHasFinishedPush;
		XMLOmitIfEmpty_t m_omitOption;
	};

	CUtlStack< Node_t > m_Nodes;
};


// CUtlStringCI: uses case-insensitive equality tests
class CUtlStringCI : public CUtlString
{
public:
	CUtlStringCI( const char *pSrc ) : CUtlString( pSrc ) {}
	bool operator==( const CUtlStringCI &src ) const { return  IsEqual_CaseInsensitive( src.Get() ); }
	bool operator!=( const CUtlStringCI &src ) const { return !IsEqual_CaseInsensitive( src.Get() ); }
};
inline bool operator==( const char *pString, const CUtlStringCI &utlString ) { return  utlString.IsEqual_CaseInsensitive( pString ); }
inline bool operator!=( const char *pString, const CUtlStringCI &utlString ) { return !utlString.IsEqual_CaseInsensitive( pString ); }
inline bool operator==( const CUtlStringCI &utlString, const char *pString ) { return  utlString.IsEqual_CaseInsensitive( pString ); }
inline bool operator!=( const CUtlStringCI &utlString, const char *pString ) { return !utlString.IsEqual_CaseInsensitive( pString ); }


template<int nLocalChars>
class CUtlStringHolder
{
public:
    CUtlStringHolder()
    {
        Set( NULL, 0 );
    }
    CUtlStringHolder( const char *pStr, int nChars = -1 )
    {
        Set( pStr, nChars );
    }
    CUtlStringHolder( const char *pStr1, int nChars1,
                      const char *pStr2, int nChars2 )
    {
        Set( pStr1, nChars2, pStr2, nChars2 );
    }
    CUtlStringHolder( const char *pStr1, const char *pStr2 )
    {
        Set( pStr1, pStr2 );
    }
    CUtlStringHolder( const char *pStr1, const char *pStr2, const char *pStr3 )
    {
        Set( pStr1, pStr2, pStr3 );
    }
    CUtlStringHolder( const CUtlStringHolder<nLocalChars>& other )
    {
        Set( other.Get() );
    }

    CUtlStringHolder& operator=( const char *pStr )
    {
        Set( pStr );
    }
    CUtlStringHolder& operator=( const CUtlStringHolder<nLocalChars>& other )
    {
        Set( other.Get() );
    }
    
    void Purge()
    {
        m_Heap.Purge();
        m_Local[0] = 0;
    }
    void Clear()
    {
        // We don't track string length so clearing the
        // string is the same as purging.
        Purge();
    }

    bool IsEmpty() const
    {
        return m_Local[0] == 0 && m_Heap.IsEmpty();
    }
    
    const char *Get() const
    {
        return m_Heap.Length() > 0 ? m_Heap.Get() : m_Local;
    }
    operator const char *() const
    {
        return Get();
    }
    char *GetForModify()
    {
        return (char*)Get();
    }
    // Ignores/destroys any existing content.
    // Ensures space for a terminator at [nLength] but
    // does not actually write one.
    char *GetBufferForModify( int nLength )
    {
        if ( nLength >= nLocalChars )
        {
            m_Heap.SetLength( nLength );
            return m_Heap.Access();
        }
        else
        {
            m_Heap.Purge();
            return m_Local;
        }
    }

    int Length() const
    {
        return V_strlen( Get() );
    }

    const char *Concat( int nStrs, const char **ppStrs, int *pLens )
    {
        int nTotal = 0;
        for ( int i = 0; i < nStrs; i++ )
        {
            if ( pLens[i] < 0 )
            {
                pLens[i] = ppStrs[i] ? V_strlen( ppStrs[i] ) : 0;
            }
            
            nTotal += pLens[i];
        }
        
        char *pBuf = GetBufferForModify( nTotal );
        for ( int i = 0; i < nStrs; i++ )
        {
            memcpy( pBuf, ppStrs[i], pLens[i] );
            pBuf += pLens[i];
        }
        *pBuf = 0;
        
        return Get();
    }
    // Takes a series of string pointers with NULL at the end.
    const char *VConcat( const char *pStr1, const char *pStr2, va_list args )
    {
        int nTotal = V_strlen( pStr1 ) + V_strlen( pStr2 );

        // Scope ReuseArgs.
        {
            CReuseVaList ReuseArgs( args );
            for (;;)
            {
                const char *pStr = va_arg( ReuseArgs.m_ReuseList, char * );
                if ( !pStr )
                {
                    break;
                }

                nTotal += V_strlen( pStr );
            }
        }

        char *pBuf = GetBufferForModify( nTotal );
        int nLen = V_strlen( pStr1 );
        memcpy( pBuf, pStr1, nLen );
        pBuf += nLen;
        nLen = V_strlen( pStr2 );
        memcpy( pBuf, pStr2, nLen );
        pBuf += nLen;
        
        for (;;)
        {
            const char *pStr = va_arg( args, char * );
            if ( !pStr )
            {
                break;
            }

            nLen = V_strlen( pStr );
            memcpy( pBuf, pStr, nLen );
            pBuf += nLen;
        }
        *pBuf = 0;
        
        return Get();
    }
    const char *Concat( const char *pStr1, const char *pStr2, ... )
    {
        va_list args;
        va_start( args, pStr2 );
        VConcat( pStr1, pStr2, args );
        va_end( args );
        return Get();
    }
        
    const char *Set( const char *pStr, int nChars = -1 )
    {
        if ( nChars < 0 )
        {
            nChars = pStr ? V_strlen( pStr ) : 0;
        }
        if ( nChars >= nLocalChars )
        {
            m_Heap.SetDirect( pStr, nChars );
        }
        else
        {
            m_Heap.Purge();
            if ( pStr )
            {
                memcpy( m_Local, pStr, nChars );
            }
            m_Local[nChars] = 0;
        }
        return Get();
    }
    const char *Set( const char *pStr1, int nChars1,
                     const char *pStr2, int nChars2 )
    {
        const char *ppStrs[2] = { pStr1, pStr2 };
        int pLens[2] = { nChars1, nChars2 };
        return Concat( 2, ppStrs, pLens );
    }
    const char *Set( const char *pStr1, const char *pStr2 )
    {
        return Set( pStr1, -1, pStr2, -1 );
    }
    const char *Set( const char *pStr1, int nChars1,
                     const char *pStr2, int nChars2,
                     const char *pStr3, int nChars3 )
    {
        const char *ppStrs[3] = { pStr1, pStr2, pStr3 };
        int pLens[3] = { nChars1, nChars2, nChars3 };
        return Concat( 3, ppStrs, pLens );
    }
    const char *Set( const char *pStr1, const char *pStr2, const char *pStr3 )
    {
        return Set( pStr1, -1, pStr2, -1, pStr3, -1 );
    }
    const char *Append( const char *pStr, int nChars = -1 )
    {
        if ( nChars < 0 )
        {
            nChars = pStr ? V_strlen( pStr ) : 0;
        }
        if ( nChars <= 0 )
        {
            return Get();
        }

        int nOldLen = Length();
        if ( nOldLen <= 0 )
        {
            return Set( pStr, nChars );
        }
        
        if ( nOldLen + nChars >= nLocalChars )
        {
            if ( m_Heap.Length() == 0 )
            {
                m_Heap.SetLength( nOldLen + nChars );
                memcpy( m_Heap.GetForModify(), m_Local, nOldLen );
                memcpy( m_Heap.GetForModify() + nOldLen, pStr, nChars );
                *( m_Heap.GetForModify() + nOldLen + nChars ) = 0;
            }
            else
            {
				const int nLen = V_strlen(pStr);
				if (nChars < 0 || nChars >= nLen)
				{
					m_Heap.Append(pStr);
				}
				else
				{
					char* pDup = V_strdup(pStr);
					pDup[nLen] = 0;
					m_Heap.Append(pDup);
					delete[] pDup;
				}
            }
        }
        else
        {
            memcpy( m_Local + nOldLen, pStr, nChars );
            m_Local[nOldLen + nChars] = 0;
        }
        return Get();
    }

    const char *ComposeFileName( const char *pPrefix, const char *pSuffix )
    {
        const char *ppStrs[3];
        int pLens[3];
        int nStrs = 1;

        ppStrs[0] = pPrefix;
        pLens[0] = pPrefix ? V_strlen( pPrefix ) : 0;
        if ( pLens[0] > 0 && !PATHSEPARATOR( pPrefix[pLens[0] - 1] ) )
        {
            ppStrs[1] = CORRECT_PATH_SEPARATOR_S;
            pLens[1] = 1;
            nStrs++;
        }
        
        ppStrs[nStrs] = pSuffix;
        pLens[nStrs] = -1;
        nStrs++;

        Concat( nStrs, ppStrs, pLens );

        V_FixSlashes( GetForModify() );
        V_FixDoubleSlashes( GetForModify() );
        return Get();
    }
    const char *ExtractFilePath( const char *pFullFilename )
    {
        // The path will be a subset of the full filename so size
        // the output buffer from the filename.
        int nFullLen = V_strlen( pFullFilename );
		if ( !V_ExtractFilePath( pFullFilename, GetBufferForModify( nFullLen ), nFullLen + 1 ) )
        {
            return NULL;
        }
        return Get();
    }
    
    void FixSlashes( char separator = CORRECT_PATH_SEPARATOR )
    {
        V_FixSlashes( GetForModify(), separator );
    }
    void FixSlashesAndDotSlashes( char separator = CORRECT_PATH_SEPARATOR )
    {
        V_FixSlashes( GetForModify() );
        V_RemoveDotSlashes( GetForModify(), separator );
    }
    void FixAllSlashes( char separator = CORRECT_PATH_SEPARATOR )
    {
        V_FixSlashes( GetForModify() );
        V_FixDoubleSlashes( GetForModify() );
        V_RemoveDotSlashes( GetForModify(), separator );
    }
    void FixupPathName( bool bLowercaseName = true )
    {
        FixAllSlashes();
        if ( bLowercaseName )
        {
            V_strlower( GetForModify() );
        }
    }
    
    void AppendSlash()
    {
        int nLen = Length();
        if ( nLen < 1 ||
             PATHSEPARATOR( Get()[nLen - 1] ) )
        {
            return;
        }

        Append( CORRECT_PATH_SEPARATOR_S );
    }

    void StripExtension()
    {
        // V_StripExtension explicitly handles in-place
        // truncation so we can safely strip in our existing buffer.
        V_StripExtension( Get(), GetForModify(), Length() + 1 );
    }
    void StripFilename()
    {
        V_StripFilename( GetForModify() );
    }
    void StripLastDir()
    {
        V_StripLastDir( GetForModify(), Length() + 1 );
    }
    
private:
    CUtlString m_Heap;
    char m_Local[nLocalChars];
};

// Simple typedef to have a consistent type for passing around paths in holders.
// Note that this deliberately doesn't use MAX_PATH since things will grow
// if necessary.
typedef CUtlStringHolder<200> CUtlPathStringHolder;


class CSplitStringInPlace
{
public:
    CSplitStringInPlace( char *pWritableStr, char splitChar )
    {
        Set( pWritableStr, splitChar );
    }

    void Set( char *pWritableStr )
    {
        m_pScan = pWritableStr;
    }
    void Set( char *pWritableStr, char splitChar )
    {
        m_pScan = pWritableStr;
        m_splitChar = splitChar;
    }

    bool HasAnySplit() const
    {
        return strchr( m_pScan, m_splitChar ) != NULL;
    }
    
    bool HasNext() const
    {
        return m_pScan != NULL;
    }
    char *GetNext()
    {
        char *pCur = m_pScan;
        m_pScan = strchr( m_pScan, m_splitChar );
        if ( m_pScan )
        {
            do
            {
                *m_pScan++ = 0;
            }
            while ( *m_pScan == m_splitChar );
        }
        return pCur;
    }

private:
    char *m_pScan;
    char m_splitChar;
};


long	Sys_FileLength( const char* filename, bool bText = false );
int		Sys_LoadFile( const char *filename, void **bufferptr, bool bText = false );
bool	Sys_LoadFileIntoBuffer( const char *pFilename, CUtlBuffer &buf, bool bText );
bool	Sys_LoadFileAsLines( const char *pFilename, CUtlVector< CUtlString > &lines );
bool	Sys_RemoveBufferCRLFs( CUtlBuffer &buffer );
bool	Sys_FileChanged( const char *pFilename, const CUtlBuffer &buffer, bool bText );
bool	Sys_WriteFile( const char *pFilename, const CUtlBuffer &buffer, bool bText );
bool	Sys_WriteFileIfChanged( const char *pFilename, const CUtlBuffer &buffer, bool bText );
void	Sys_StripPath( const char *path, char *outpath, int nOutPathSize );
bool	Sys_Exists( const char *filename );
bool	Sys_Touch( const char *filename );
bool	Sys_FileInfo( const char *pFilename, int64 &nFileSize, int64 &nModifyTime, bool &bIsReadOnly );

bool	Sys_StringToBool( const char *pString, bool bAssumeTrueIfAmbiguous = false );
bool	Sys_ReplaceString( const char *pStream, const char *pSearch, const char *pReplace, char *pOutBuff, int outBuffSize );
bool	Sys_StringPatternMatch( char const *pSrcPattern, char const *pString );
bool	Sys_IsSingleLineComment( const char *pSearchPos, const char *pFileStart );

const char *Sys_EvaluateEnvironmentExpression( const char *pExpression, const char *pDefault );

bool	Sys_ExpandFilePattern( const char *pPattern, CUtlVector< CUtlString > &vecResults );
bool	Sys_GetExecutablePath( char *pBuf, int cbBuf );

void	Sys_CreatePath( const char *path );

bool	Sys_ForceToMinimalRelativePath( const char *pBasePath, const char *pRelativeFilename, char *pOutputBuffer, int nOutputBufferSize );

bool	Sys_GetActualFilenameCase( const char *pFilename, char *pOutputBuffer, int nOutputBufferSize );

bool	Sys_IsFilenameCaseConsistent( const char *pFilename, char *pOutputBuffer, int nOutputBufferSize );

bool	Sys_CopyToMirror( const char *pFilename );

#define GENERATED_CPP_FILE_EXTENSION "gen_cpp"

bool IsCFileExtension( const char *pExtension );
bool IsHFileExtension( const char *pExtension );

int GetNumCFileExtensions();
const char *GetCFileExtension( int nIndex );

bool IsLibraryFile( const char *pFilename );
bool IsSourceFile( const char *pFilename );
