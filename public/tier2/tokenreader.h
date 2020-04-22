//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TOKENREADER_H
#define TOKENREADER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/basetypes.h"
#include "tier2/utlstreambuffer.h"
#include <assert.h>


typedef enum
{
	TOKENSTRINGTOOLONG = -4,
	TOKENERROR = -3,
	TOKENNONE = -2,
	TOKENEOF = -1,
	OPERATOR,
	INTEGER,
	STRING,
	IDENT
} trtoken_t;


#define IsToken(s1, s2)	!strcmpi(s1, s2)

#define MAX_TOKEN 128 + 1
#define MAX_IDENT 64 + 1
#define MAX_STRING 128 + 1


class TokenReader
{
public:

	TokenReader();

	bool Open(const char *pszFilename);
	trtoken_t NextToken(char *pszStore, int nSize);
	trtoken_t NextTokenDynamic(char **ppszStore);
	void Close();

	void IgnoreTill(trtoken_t ttype, const char *pszToken);
	void Stuff(trtoken_t ttype, const char *pszToken);
	bool Expecting(trtoken_t ttype, const char *pszToken);
	const char *Error(char *error, ...);
	trtoken_t PeekTokenType(char* = NULL, int maxlen = 0);

	inline int GetErrorCount(void);

private:

	trtoken_t GetString(char *pszStore, int nSize);
	bool SkipWhiteSpace(void);

	CUtlStreamBuffer m_file;
	int m_nLine;
	int m_nErrorCount;

	char m_szFilename[128];
	char m_szStuffed[128];
	bool m_bStuffed;
	trtoken_t m_eStuffed;
};


//-----------------------------------------------------------------------------
// Purpose: Returns the total number of parsing errors since this file was opened.
//-----------------------------------------------------------------------------
int TokenReader::GetErrorCount(void)
{
	return(m_nErrorCount);
}


#endif // TOKENREADER_H
