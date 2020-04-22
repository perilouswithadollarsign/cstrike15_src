//========= Copyright © 2008, Valve Corporation, All rights reserved. ============//

#ifndef __TOKENSET_H
#define __TOKENSET_H

#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// 
// This class is a handy way to match a series of values to stings and vice
// versa.  It should be initalized as static like an array, for example:
// 
// const tokenset_t<int> tokens[] = {
//     { "token1", 1 },
//     { "token2", 2 },
//     { "token3", 3 },
//     { NULL,    -1 }
// };
// 
// Then you can call the operators on it by using:
// 
// int t = tokens->GetToken( s );
// 
// If s is "token1" it returns 1, etc.  Invalid string returns the last NULL
// value.  GetNameByToken() returns "__UNKNOWN__" when passed a mismatched 
// token. GetNameByToken() returns szMismatchResult when passed a mismatched 
// token and a string to return in case of mismatch.
// 
//-----------------------------------------------------------------------------

template <class _T>
struct tokenset_t
{
	const char *name;
	_T token;

	_T GetToken( const char *s ) const;
	_T GetTokenI( const char *s ) const;
	const char *GetNameByToken( _T token ) const;
	const char *GetNameByToken( _T token, const char *szMismatchResult ) const;
};

template <class _T>
inline _T tokenset_t< _T >::GetToken( const char *s ) const
{
	const tokenset_t< _T > *c;

	for ( c = this; c->name; ++c )
	{
		if ( !s )
		{
			continue; // Loop to the last NULL value
		}

		if ( Q_strcmp( s, c->name ) == 0 )
		{
			return c->token;
		}
	}

	return c->token; // c points to the last NULL value
}

template <class _T>
inline _T tokenset_t< _T >::GetTokenI( const char *s ) const
{
	const tokenset_t< _T > *c;

	for ( c = this; c->name; ++c )
	{
		if ( !s )
		{
			continue; // Loop to the last NULL value
		}

		if ( Q_stricmp( s, c->name ) == 0 )
		{
			return c->token;
		}
	}

	return c->token; // c points to the last NULL value
}

template <class _T>
inline const char *tokenset_t< _T >::GetNameByToken( _T token ) const
{
	static const char *unknown = "__UNKNOWN__";

	const tokenset_t< _T > *c;

	for ( c = this; c->name; ++c )
	{
		if ( c->token == token )
		{
			return c->name;
		}
	}

	return unknown;
}

template <class _T>
inline const char *tokenset_t< _T >::GetNameByToken( _T token, char const *szMismatchResult ) const
{
	const tokenset_t< _T > *c;

	for ( c = this; c->name; ++c )
	{
		if ( c->token == token )
		{
			return c->name;
		}
	}

	return szMismatchResult;
}

#endif //__TOKENSET_H

