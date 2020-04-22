//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//
//
//==================================================================================================

#include "vpc.h"

#define MAX_SCRIPT_STACK_SIZE	32

CScript::CScript()
{
	m_ScriptName = "(empty)";
	m_nScriptLine = 0;
	m_pScriptData = NULL;
	m_pScriptLine = &m_nScriptLine;
    m_nInPrivilegedScript = 0;
	m_bScriptNameIsAFile = false;

	m_DefaultToken.EnsureCapacity( 10000 );
    m_pTokenBuf = &m_DefaultToken;
	m_PeekToken.EnsureCapacity( m_DefaultToken.Capacity() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CScript::SkipWhitespace( const char *data, bool *pHasNewLines, int* pNumLines ) 
{
	int c;

	// Avoid crash hit while trying to VPC @VPC
	if ( !data )
		return NULL;

	while ( ( c = *data ) <= ' ' ) 
	{
		if ( c == '\n' ) 
		{
			if ( pNumLines )
			{
				(*pNumLines)++;
			}

			if ( pHasNewLines )
			{
				*pHasNewLines = true;
			}
		}
		else if ( !c )
		{
			return ( NULL );
		}

		data++;
	}

	return data;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CScript::SkipToValidToken( const char *data, bool *pHasNewLines, int* pNumLines ) 
{
	int c;

	for ( ;; )
	{
		data = SkipWhitespace( data, pHasNewLines, pNumLines );
		if ( !data )
			break;

		c = *data;
		if ( !c )
		{
			break;
		}

		if ( c == '/' && data[1] == '/' )
		{
			// skip double slash comments
			data += 2;
			while ( *data && *data != '\n' )
			{
				data++;
			}
			if ( *data && *data == '\n' )
			{
				data++;
				if ( pNumLines )
				{
					(*pNumLines)++;
				}
				if ( pHasNewLines )
				{
					*pHasNewLines = true;
				}
			}
		}
		else if ( c == '/' && data[1] == '*' ) 
		{
			// skip /* */ comments
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				if ( *data == '\n' )
				{
					if ( pNumLines )
					{
						(*pNumLines)++;
					}
					if ( pHasNewLines )
					{
						*pHasNewLines = true;
					}
				}
				data++;
			}

			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	return data;
}

//-----------------------------------------------------------------------------
//	The next token should be an open brace.
//	Skips until a matching close brace is found.
//	Internal brace depths are properly skipped.
//-----------------------------------------------------------------------------
void CScript::SkipBracedSection( const char** dataptr, int* numlines, int nInitialDepth ) 
{
	const char*	token;
	int	depth;

	depth = nInitialDepth;
	do 
	{
		token = GetToken( dataptr, true, numlines );
		if ( token[1] == '\0' ) 
		{
			if ( token[0] == '{' )
				depth++;
			else if ( token[0] == '}' )
				depth--;
		}
	}
	while( depth && *dataptr );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CScript::SkipRestOfLine( const char** dataptr, int* numlines ) 
{
	const char*	p;
	int	c;

	p = *dataptr;
	while ( ( c = *p++ ) != '\0' ) 
	{
		if ( c == '\n' ) 
		{
			if ( numlines )
				( *numlines )++;
			break;
		}
	}
	*dataptr = p;
}

//-----------------------------------------------------------------------------
// Does not corrupt results obtained with GetToken().
//-----------------------------------------------------------------------------
const char* CScript::PeekNextToken( const char *dataptr, bool bAllowLineBreaks )
{
	// Switch to the peek token buffer so that the current
    // token is unchanged.
    CUtlStringBuilder *pOldTokenBuf = SetTokenBuffer( &m_PeekToken );

	const char *pSaved = dataptr;
	GetToken( &pSaved, bAllowLineBreaks, NULL );

	// restore
    SetTokenBuffer( pOldTokenBuf );

	return m_PeekToken.Get();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
const char *CScript::GetToken( const char **dataptr, bool allowLineBreaks, int *pNumLines )
{
	char		c;
	char		endSymbol;
	bool		hasNewLines;
	const char*		data;
    CUtlStringBuilder *pTokenBuf = m_pTokenBuf;

	c           = 0;
	data        = *dataptr;
	pTokenBuf->SetLength( 0 );
	hasNewLines = false;

	// make sure incoming data is valid
	if ( !data )
	{
		*dataptr = NULL;
		return pTokenBuf->Get();
	}

	for ( ;; )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines, pNumLines );
		if ( !data )
		{
			*dataptr = NULL;
			return pTokenBuf->Get();
		}
		
		if ( hasNewLines && !allowLineBreaks )
		{
			*dataptr = data;
			return pTokenBuf->Get();
		}

		c = *data;

		if ( c == '/' && data[1] == '/' )
		{
			// skip double slash comments
			data += 2;
			while ( *data && *data != '\n' )
			{
				data++;
			}
			if ( *data && *data == '\n' )
			{
				if ( !allowLineBreaks )
					continue;

				data++;
				if ( pNumLines )
				{
					(*pNumLines)++;
				}
			}
		}
		else if ( c =='/' && data[1] == '*' ) 
		{
			// skip /* */ comments
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				if ( *data == '\n' && pNumLines )
				{
					(*pNumLines)++;
				}
				data++;
			}

			if ( *data ) 
			{
				data += 2;
			}
		}
		else
			break;
	}

	// handle scoped strings "???" <???> [???]
	if ( c == '\"' || c == '<' || c == '[')
	{
		bool bConditionalExpression = false;
		endSymbol = '\0';
		switch ( c )
		{
		case '\"':
			endSymbol = '\"';
			break;
		case '<':
			endSymbol = '>';
			break;
		case '[':
			bConditionalExpression = true;
			endSymbol = ']';
			break;
		}

		// want to preserve entire conditional expession [blah...blah...blah]
		// maintain a conditional's open/close scope characters
		if ( !bConditionalExpression )
		{
			// skip past scope character
			data++;
		}

		for ( ;; )
		{
			c = *data++;

			if ( c == endSymbol || !c )
			{
				if ( c == endSymbol && bConditionalExpression )
				{
					// keep end symbol
					pTokenBuf->AppendChar( c );
				}
			
				*dataptr = (char*)data;
				return pTokenBuf->Get();
			}

            pTokenBuf->AppendChar( c );
		}
	}

	// parse a regular word
	do
	{
        pTokenBuf->AppendChar( c );
		data++;
		c = *data;
	} 
	while ( c > ' ' );

	*dataptr = (char*)data;
	
	return pTokenBuf->Get();
}

void CScript::UpdateThisVpc()
{
	const char *szScriptName = nullptr;
	if ( m_bScriptNameIsAFile )
	{
		szScriptName = m_ScriptName.Get();
	}
	else
	{
		//crawl down the stack looking for the most recently pushed script with an actual filename
		for ( int nScriptStackIndex = m_ScriptStack.Count(); --nScriptStackIndex >= 0; )
		{
			if ( m_ScriptStack[nScriptStackIndex].IsScriptNameAFile() )
			{
				szScriptName = m_ScriptStack[nScriptStackIndex].GetName();
				break;
			}
		}
	}
	
	if ( szScriptName == nullptr )
	{
		g_pVPC->SetScriptMacro( "THISVPC", "" );
		g_pVPC->SetScriptMacro( "THISVPCFILE", "" );
		return;
	}

    char absFile[MAX_FIXED_PATH];
	char tempPath[2][MAX_FIXED_PATH];

    V_MakeAbsolutePath( absFile, ARRAYSIZE( absFile ), szScriptName, NULL, k_bVPCForceLowerCase );

	V_strcpy_safe( tempPath[0], absFile );
    V_StripFilename( tempPath[0] );
    g_pVPC->SetScriptMacro( "THISVPC", tempPath[0] );

	V_strcpy_safe( tempPath[0], absFile );
	V_MakeRelativePath( tempPath[0], g_pVPC->GetProjectPath(), tempPath[1], ARRAYSIZE( tempPath[1] ) );
	
	g_pVPC->SetScriptMacro( "THISVPCFILE", tempPath[1] );
}

void CScript::PushScript( const char *pFilename, bool bAddScriptToCRCCheck )
{
	// parse the text script
	if ( !Sys_Exists( pFilename ) )
	{
		g_pVPC->VPCError( "Cannot open %s", pFilename );	
	}

	char *pScriptBuffer;
	int nScriptLength = Sys_LoadFile( pFilename, (void**)&pScriptBuffer, true );
	if ( nScriptLength < 0 )
	{
		// Unexpected due to existence check
		g_pVPC->VPCError( "Cannot open %s", pFilename );
	}
	
	g_pVPC->AddScriptToParsedList( pFilename, bAddScriptToCRCCheck, bAddScriptToCRCCheck ? CRC32_ProcessSingleBuffer( pScriptBuffer, nScriptLength ) : 0 );

	PushScript( pFilename, pScriptBuffer, 1, true, true );
}

void CScript::PushScript( const char *pScriptName, const char *pScriptData, int nScriptLine, bool bFreeScriptAtPop, bool bScriptNameIsAFile )
{
	if ( m_ScriptStack.Count() > MAX_SCRIPT_STACK_SIZE )
	{
		g_pVPC->VPCError( "PushScript( scriptname=%s ) - stack overflow\n", pScriptName );
	}

	// Push the current state onto the stack.
	m_ScriptStack.Push( GetCurrentScript() );

	// Set their state as the current state.
	m_ScriptName = pScriptName;
	m_pScriptData = pScriptData;
	m_nScriptLine = nScriptLine;
	m_bFreeScriptAtPop = bFreeScriptAtPop;
	m_bScriptNameIsAFile = bScriptNameIsAFile;

    UpdateThisVpc();
}

void CScript::PushCurrentScript()
{
    // We always push false for free-at-pop so that only the least nested
    // push will actually free the pointer.
	PushScript( m_ScriptName.Get(), m_pScriptData, m_nScriptLine, false, m_bScriptNameIsAFile );
}

CScriptSource CScript::GetCurrentScript()
{
	return CScriptSource( m_ScriptName.Get(), m_pScriptData, m_nScriptLine, m_bFreeScriptAtPop, m_nInPrivilegedScript, m_bScriptNameIsAFile );
}

void CScript::RestoreScript( const CScriptSource &scriptSource )
{
	m_ScriptName = scriptSource.GetName();
	m_pScriptData = scriptSource.GetData();
	m_nScriptLine = scriptSource.GetLine();
	m_bFreeScriptAtPop = scriptSource.IsFreeScriptAtPop();
    m_nInPrivilegedScript = scriptSource.GetInPrivilegedScript();
	m_bScriptNameIsAFile = scriptSource.IsScriptNameAFile();

    UpdateThisVpc();
}

void CScript::PopScript()
{
	if ( m_ScriptStack.Count() == 0 )
	{
		g_pVPC->VPCError( "PopScript(): stack is empty" );
	}

	if ( m_bFreeScriptAtPop && m_pScriptData )
	{
		delete [] m_pScriptData;
	}

	// Restore the top entry on the stack and pop it off.
	const CScriptSource &state = m_ScriptStack.Top();
	m_ScriptName = state.GetName();
	m_pScriptData = state.GetData();
	m_nScriptLine = state.GetLine();
	m_bFreeScriptAtPop = state.IsFreeScriptAtPop();
	m_bScriptNameIsAFile = state.IsScriptNameAFile();

    UpdateThisVpc();

	m_ScriptStack.Pop();
}

void CScript::EnsureScriptStackEmpty()
{
	if ( m_ScriptStack.Count() != 0 )
	{
		g_pVPC->VPCError( "EnsureScriptStackEmpty(): script stack is not empty!" );
	}
}

void CScript::SpewScriptStack( bool bDueToError )
{
	if ( m_ScriptStack.Count() )
	{
		CUtlString str;

		// user really want to see absolute paths, but all sorts of code was written with relative versions
#if defined( POSIX )
		char fullPath[PATH_MAX];
#else
		char fullPath[MAX_FIXED_PATH];
#endif
		fullPath[0] = '\0';
		if ( !m_ScriptName.IsEmpty() && !V_IsAbsolutePath( m_ScriptName.Get() ) )
		{
#if defined( PLATFORM_WINDOWS )
			_fullpath( fullPath, m_ScriptName.Get(), sizeof( fullPath ) );
#elif defined( POSIX )
			if ( realpath( m_ScriptName.Get(), fullPath ) == NULL )
            {
                fullPath[0] = '\0';
            }
#else
			V_strncpy( fullPath, m_ScriptName.Get(), sizeof( fullPath ) );
#endif
		}		

		// emit stack with current at top
		str += "Script Stack:\n";
		str += CFmtStr( "   %s Line:%d\n", fullPath[0] ? fullPath : m_ScriptName.Get(), m_nScriptLine );

		for ( int i = m_ScriptStack.Count() - 1; i >= 0; i-- )
		{
			if ( i == 0 && !m_ScriptStack[i].GetData() && m_ScriptStack[i].GetLine() <= 0 )
			{
				// ignore empty bottom of stack
				break;
			}

			// user really want to see absolute paths, but all sorts of code was written with relative versions
			fullPath[0] = '\0';
			if ( m_ScriptStack[i].GetName()[0] && !V_IsAbsolutePath( m_ScriptStack[i].GetName() ) )
			{
#if defined( PLATFORM_WINDOWS )
				_fullpath( fullPath, m_ScriptStack[i].GetName(), sizeof( fullPath ) );
#elif defined( POSIX )
				if ( realpath( m_ScriptStack[i].GetName(), fullPath ) == NULL )
                {
                    fullPath[0] = '\0';
                }
#else
				V_strncpy( fullPath, m_ScriptStack[i].GetName(), sizeof( fullPath ) );
#endif
			}		

			str += CFmtStr( "   %s Line:%d\n", fullPath[0] ? fullPath : m_ScriptStack[i].GetName(), m_ScriptStack[i].GetLine() );
		}
		str += "\n";		

		if ( bDueToError )
		{
			Log_Warning( LOG_VPC, Color( 255, 0, 0, 255 ), "%s", str.String() );
		}
		else
		{
			Log_Msg( LOG_VPC, "%s", str.String() );
		}
	}
}

const char *CScript::GetToken( bool bAllowLineBreaks )
{
	return GetToken( &m_pScriptData, bAllowLineBreaks, m_pScriptLine );
}

const char *CScript::PeekNextToken( bool bAllowLineBreaks )
{
	return PeekNextToken( m_pScriptData, bAllowLineBreaks );
}

void CScript::SkipRestOfLine() 
{
	SkipRestOfLine( &m_pScriptData, m_pScriptLine );
}

void CScript::SkipBracedSection( int nInitialDepth ) 
{
	SkipBracedSection( &m_pScriptData, m_pScriptLine, nInitialDepth );
}

void CScript::SkipToValidToken() 
{
	m_pScriptData = SkipToValidToken( m_pScriptData, NULL, m_pScriptLine );
}

//-----------------------------------------------------------------------------
//	Handles expressions of the form <$BASE> <xxx> ... <xxx> [condition]
//	Output is a concatenated string.
//
//	Returns true if expression should be used, false if it should be ignored due
//	to an optional condition that evaluated false.
//-----------------------------------------------------------------------------
bool CScript::ParsePropertyValue( const char *pBaseString, CUtlStringBuilder *pOutBuff )
{
	if ( !pBaseString )
	{
		// $BASE default
		pBaseString = "";
	}

    pOutBuff->SetLength( 0 );
    
	const char **pScriptData = &m_pScriptData;
	int *pScriptLine = m_pScriptLine; 

	bool bAllowNextLine = false;
	bool bResult = true;
    bool bFoundReservedEmptyTokenOnly = true;
	while ( 1 )
	{
		const char *pToken = GetToken( pScriptData, bAllowNextLine, pScriptLine );
		if ( !pToken || !pToken[0] )
		{
			g_pVPC->VPCSyntaxError();
		}

		const char *pNextToken = PeekNextToken( *pScriptData, false );
		if ( ( !pNextToken || !pNextToken[0] ) && pToken[0] == '[' )
		{
			// current token is last token
			// last token can be optional conditional, need to identify
			// backup and reparse up to last token
			// last token is an optional conditional
			bResult = g_pVPC->EvaluateConditionalExpression( pToken );
			break;
		}
		else if ( pToken[0] == '[' && pNextToken && pNextToken[0] == '[' )
		{
			g_pVPC->VPCSyntaxError( "Bad conditional syntax. Use C style boolean expression operators to express compound conditionals." );
		}
		else if ( bFoundReservedEmptyTokenOnly )
        {
			bFoundReservedEmptyTokenOnly = !V_stricmp_fast( pToken, "$EMPTY" );
		}

		bAllowNextLine = CharStrEq( pToken, '\\' );
		if ( bAllowNextLine )
			continue;

		if ( pToken[0] == '\\' &&
             ( pToken[1] == 'n' || pToken[1] == 'N' ) &&
             pToken[2] == 0 )
		{
			pToken = "\n";
		}

		if ( pToken[0] )
		{
			// handle reserved replacements
            CUtlStringBuilder *pStrBuf = g_pVPC->GetMacroReplaceBuffer();
            pStrBuf->Set( pToken );
            pStrBuf->ReplaceFastCaseless( "$BASE", pBaseString );
			g_pVPC->ResolveMacrosInString( NULL, pStrBuf );

            pOutBuff->Append( pStrBuf->Get() );
		}

		pToken = PeekNextToken( *pScriptData, false );
		if ( !pToken || !pToken[0] || CharStrEq( pNextToken, '}' ) )
			break;
	}

	if ( pOutBuff->IsEmpty() && bResult )
	{
		// The property value resolved empty and was not conditionally disabled.
		// Check the tokens scanned to quietly allow an explicit "$EMPTY".
		// Allow quietly a usage of "$EMPTY" or "$EMPTY" [$Condition]
		// This provides a no-op state where properties can be explicitly purged by using $EMPTY.
		if ( !bFoundReservedEmptyTokenOnly )
		{
			// error due to unexpected fully empty state
			g_pVPC->VPCSyntaxError( "Unexpected empty value." );
		}
	}

	return bResult;
}
