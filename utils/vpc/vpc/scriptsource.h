//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
// This module manages a stack of "script sources".
//
//==================================================================================================

#pragma once

#define MAX_SYSPRINTMSG		4096

class CScriptSource
{
public:
	CScriptSource()
	{
		Set( "", NULL, 0, false, 0, false );
	}

	CScriptSource( const char *pScriptName, const char *pScriptData, int nScriptLine, bool bFreeScriptAtPop, int nInPrivilegedScript, bool bScriptNameIsAFile )
	{
		Set( pScriptName, pScriptData, nScriptLine, bFreeScriptAtPop, nInPrivilegedScript, bScriptNameIsAFile );
	}

	void Set( const char *pScriptName, const char *pScriptData, int nScriptLine, bool bFreeScriptAtPop, int nInPrivilegedScript, bool bScriptNameIsAFile )
	{
		m_ScriptName = pScriptName;
		m_pScriptData = pScriptData;
		m_nScriptLine = nScriptLine;
		m_bFreeScriptAtPop = bFreeScriptAtPop;
        m_nInPrivilegedScript = nInPrivilegedScript;
		m_bScriptNameIsAFile = bScriptNameIsAFile;
	}

	const char *GetName() const			{ return m_ScriptName.Get(); }
	const char *GetData() const			{ return m_pScriptData; }
	int GetLine() const					{ return m_nScriptLine; }
	bool IsFreeScriptAtPop() const		{ return m_bFreeScriptAtPop; }
    int GetInPrivilegedScript() const   { return m_nInPrivilegedScript; }
	bool IsScriptNameAFile() const		{ return m_bScriptNameIsAFile; }

private:
	CUtlString	m_ScriptName;
	const char	*m_pScriptData;
	int			m_nScriptLine;
	bool		m_bFreeScriptAtPop;
    int         m_nInPrivilegedScript;
	bool		m_bScriptNameIsAFile;
};

class CScript
{
public:
	CScript();

	void			PushScript( const char *pFilename, bool bAddScriptToCRCCheck = false );
	void 			PushScript( const char *pScriptName, const char *ppScriptData, int nScriptLine, bool bFreeScriptAtPop, bool bScriptNameIsAFile );
	void 			PushCurrentScript();
	void 			PopScript();
	CScriptSource	GetCurrentScript();
	void			RestoreScript( const CScriptSource &scriptSource );
	void			EnsureScriptStackEmpty();
	void			SpewScriptStack( bool bDueToError );

	const char		*GetName() const		{ return m_ScriptName.Get(); }
	const char		*GetData() const 		{ return m_pScriptData; }
	int				GetLine() const			{ return m_nScriptLine; }

    bool            IsInPrivilegedScript()  { return m_nInPrivilegedScript > 0; }
    void            EnterPrivilegedScript() { m_nInPrivilegedScript++; }
    void            LeavePrivilegedScript() { m_nInPrivilegedScript--; }
    
    CUtlStringBuilder *SetTokenBuffer( CUtlStringBuilder *pBuf )
    {
        CUtlStringBuilder *pCur = m_pTokenBuf;
        m_pTokenBuf = pBuf;
        return pCur;
    }

	const char		*GetToken( bool bAllowLineBreaks );
	const char		*PeekNextToken( bool bAllowLineBreaks );
	void			SkipRestOfLine();
	void			SkipBracedSection( int nInitialDepth = 0 ); 
	void			SkipToValidToken(); 

	bool			ParsePropertyValue( const char *pBaseString, CUtlStringBuilder *pOutBuff );

private:
	const char		*SkipWhitespace( const char *data, bool *pHasNewLines, int *pNumLines );
	const char		*SkipToValidToken( const char *data, bool *pHasNewLines, int *pNumLines );
	void			SkipBracedSection( const char **dataptr, int *numlines, int nInitialDepth );
	void			SkipRestOfLine( const char **dataptr, int *numlines );
	const char		*PeekNextToken( const char *dataptr, bool bAllowLineBreaks );
	const char		*GetToken( const char **dataptr, bool allowLineBreaks, int *pNumLines );

    void			UpdateThisVpc();

	CUtlStack< CScriptSource >	m_ScriptStack;

	int							m_nScriptLine;
	int							*m_pScriptLine;
	const char					*m_pScriptData;
	CUtlString					m_ScriptName;
	bool						m_bFreeScriptAtPop;
    int                         m_nInPrivilegedScript;
	bool						m_bScriptNameIsAFile;

    CUtlStringBuilder           *m_pTokenBuf;
    CUtlStringBuilder			m_DefaultToken;
    CUtlStringBuilder			m_PeekToken;
};
