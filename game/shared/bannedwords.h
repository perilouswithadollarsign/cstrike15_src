//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef BANNEDWORDS_H
#define BANNEDWORDS_H
#ifdef _WIN32
#pragma once
#endif

//
// Implements censoring of bad words
//
class CBannedWordsDictionary;
class CBannedWords
{
public:
	CBannedWords() { m_pDictionary = NULL; }
	~CBannedWords();

	// Initializes dictionary from a buffer
	bool InitFromFile( char const *szFilename );
	bool BInitialized() const { return m_pDictionary != NULL; }

	// Censors banned words in the buffer, returns number of characters censored
	int CensorBannedWordsInplace( wchar_t *wsz ) const;
	int CensorBannedWordsInplace( char *sz ) const;

	// Censors external string that cannot be stomped, stores the censored version
	char const * CensorExternalString( uint64 ullKey, char const *szExternalString ) const;

private:
	// Private implementation details
	CBannedWordsDictionary *m_pDictionary;
};

extern CBannedWords g_BannedWords;

#endif // BANNEDWORDS_H
