//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: dll-agnostic routines (no dll dependencies here)
//
// $NoKeywords: $
//=============================================================================//

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#ifndef SHARED_UTIL_H
#define SHARED_UTIL_H

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//--------------------------------------------------------------------------------------------------------
/**
 * Returns the token parsed by SharedParse()
 */
char *SharedGetToken( void );

//--------------------------------------------------------------------------------------------------------
/**
 * Sets the character used to delimit quoted strings.  Default is '\"'.  Be sure to set it back when done.
 */
void SharedSetQuoteChar( char c );

//--------------------------------------------------------------------------------------------------------
/**
 * Parse a token out of a string
 */
const char *SharedParse( const char *data );

//--------------------------------------------------------------------------------------------------------
/**
 * Returns true if additional data is waiting to be processed on this line
 */
bool SharedTokenWaiting( const char *buffer );

//--------------------------------------------------------------------------------------------------------
/**
 * Simple utility function to allocate memory and duplicate a string
 */
/*
inline char *CloneString( const char *str )
{
	char *cloneStr = new char [ strlen(str)+1 ];
	strcpy( cloneStr, str );
	return cloneStr;
}*/


//--------------------------------------------------------------------------------------------------------
/**
 * Simple utility function to allocate memory and duplicate a wide string
 */
#ifdef _WIN32
extern inline wchar_t *CloneWString( const wchar_t *str );
// {
// 	wchar_t *cloneStr = new wchar_t [ wcslen(str)+1 ];
// 	wcscpy( cloneStr, str );
// 	return cloneStr;
// }
#endif

//--------------------------------------------------------------------------------------------------------------
/**
 *  snprintf-alike that allows multiple prints into a buffer
 */
char * BufPrintf(char *buf, int& len, PRINTF_FORMAT_STRING const char *fmt, ...);

//--------------------------------------------------------------------------------------------------------------
/**
 *  wide char version of BufPrintf
 */
#ifdef _WIN32
wchar_t * BufWPrintf(wchar_t *buf, int& len, PRINTF_FORMAT_STRING const wchar_t *fmt, ...);
#endif

//--------------------------------------------------------------------------------------------------------------
/**
 *  convenience function that prints an int into a static wchar_t*
 */
#ifdef _WIN32
const wchar_t * NumAsWString( int val );
#endif
// dgoodenough - PS3 needs this guy as well.
// PS3_BUILDFIX
#ifdef _PS3
const wchar_t * NumAsWString( int val );
#endif
//--------------------------------------------------------------------------------------------------------------
/**
 *  convenience function that prints an int into a static char*
 */
const char * NumAsString( int val );

//--------------------------------------------------------------------------------------------------------------
/**
 *  convenience function that composes a string into a static char*
 */
char * SharedVarArgs(PRINTF_FORMAT_STRING const char *format, ...);

#include "tier0/memdbgoff.h"

#endif // SHARED_UTIL_H
