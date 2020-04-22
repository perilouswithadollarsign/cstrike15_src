//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef AMALGAMATEDTEXTUREPARSER_H
#define AMALGAMATEDTEXTUREPARSER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"

class CAmalgamatedTexture;
class CDmeAmalgamatedTexture;

struct SequenceFrame;

// For parsing tool from the command line.
class CAmalgamatedTextureParser
{
public:
	CAmalgamatedTextureParser();
	void Init( const char *pSourceFileName, const char *pTgaFileName, const char *pShtFileName );
	bool ReadFile( CDmeAmalgamatedTexture &amalgTex );
	bool ReadFileUsingBuffer( CDmeAmalgamatedTexture &amalgTex );

	void ParseFrameImages( CUtlVector<char *> &words, CUtlVector<char *> &outImageNames );

private:

	int ParsePackingMode( char *word );
	int ParseSequenceType( char *word );	

	void ValidateFramePacking( SequenceFrame *pBitmap, char *fileName );

	const char *m_pSourceFile;
	const char *m_pShtFile;
	const char *m_pTgaFile;

	int m_NumActualLinesRead;
};


#endif // AMALGAMATEDTEXTUREPARSER_H