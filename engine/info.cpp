//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#include "quakedef.h"
#include <stdlib.h>
#include <ctype.h>
#include "filesystem.h"
#include "filesystem_engine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_KV_LEN 127
/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
===============
*/
const char *Info_ValueForKey ( const char *s, const char *key )
{
	char	pkey[512];
	static	char value[4][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;
	
	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!Q_strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey ( char *s, const char *key )
{
	char	*start;
	char	pkey[512];
	char	value[512];
	char	*o;

	if (Q_strstr (key, "\\"))
	{
		ConMsg ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!Q_strcmp (key, pkey) )
		{
			// Copies onto self, so okay to not use Q_strncpy???
			Q_strcpy (start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}

}

void Info_RemovePrefixedKeys (char *start, char prefix)
{
	char	*s;
	char	pkey[512];
	char	value[512];
	char	*o;

	s = start;

	while (1)
	{
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix)
		{
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}
}

bool Info_IsKeyImportant( const char *key )
{
	if ( key[0] == '*' )
		return true;
	if ( !Q_strcmp( key, "name" ) )
		return true;
	if ( !Q_strcmp( key, "model" ) )
		return true;
	if ( !Q_strcmp( key, "rate" ) )
		return true;
	if ( !Q_strcmp( key, "cl_updaterate" ) )
		return true;
	if ( !Q_strcmp( key, "cl_lw" ) )
		return true;
	if ( !Q_strcmp( key, "cl_lc" ) )
		return true;
	if ( !Q_strcmp( key, "tracker" ) )
		return true;

	return false;
}

char *Info_FindLargestKey( char *s, int maxsize )
{
	char	key[256];
	char	value[256];
	char	*o;
	int		l;
	static char largest_key[256];
	int     largest_size = 0;

	*largest_key = 0;

	if (*s == '\\')
		s++;
	while (*s)
	{
		int size = 0;

		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		*o = 0;
		size = Q_strlen( key );

		if (!*s)
		{
			return largest_key;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;

		size += Q_strlen( value );

		if ( (size > largest_size) && !Info_IsKeyImportant(key) )
		{
			largest_size = size;
			Q_strncpy( largest_key, key, sizeof( largest_key ) );
		}
	}

	return largest_key;
}

void Info_SetValueForStarKey ( char *s, const char *key, const char *value, int maxsize )
{
	char	newArray[1024], *v;
	int		c;

	if (Q_strstr (key, "\\") || Q_strstr (value, "\\") )
	{
		ConMsg ("Can't use keys or values with a \\\n");
		return;
	}

	if (Q_strstr (key, "\"") || Q_strstr (value, "\"") )
	{
		ConMsg ("Can't use keys or values with a \"\n");
		return;
	}

	if (Q_strlen(key) > MAX_KV_LEN || Q_strlen(value) > MAX_KV_LEN)
	{
		ConMsg ("Keys and values must be < %i characters.\n", MAX_KV_LEN + 1 );
		return;
	}
	Info_RemoveKey (s, key);
	if (!value || !Q_strlen(value))
		return;

	Q_snprintf (newArray, sizeof( newArray ), "\\%s\\%s", key, value);

 	if ( (int)(Q_strlen(newArray) + Q_strlen(s)) >= maxsize)
	{
		// no more room in buffer to add key/value
		if ( Info_IsKeyImportant( key ) )
		{
			// keep removing the largest key/values until we have room
			char *largekey;
			do {
				largekey = Info_FindLargestKey( s, maxsize );
				Info_RemoveKey( s, largekey );
			} while ( ((int)(Q_strlen(newArray) + Q_strlen(s)) >= maxsize) && *largekey != 0 );

			if ( largekey[0] == 0 )
			{
				// no room to add setting
				ConMsg ("Info string length exceeded\n");
				return;
			}
		}
		else
		{
			// no room to add setting
			ConMsg ("Info string length exceeded\n");
			return;
		}
	}

	// only copy ascii values
	s += Q_strlen(s);
	v = newArray;
	while (*v)
	{
		c = (unsigned char)*v++;
		// client only allows highbits on name
		if (stricmp(key, "name") != 0) {
			c &= 127;
			if (c < 32 || c > 127)
				continue;
			// auto lowercase team
			if (stricmp(key, "team") == 0)
				c = tolower(c);
		}
		if (c > 13)
			*s++ = c;
	}
	*s = 0;
}

void Info_SetValueForKey (char *s, const char *key, const char *value, int maxsize)
{
	if (key[0] == '*')
	{
		ConMsg ("Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}

void Info_Print ( const char *s )
{
	char	key[512];
	char	value[512];
	char	*o;
	int		l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < 20)
		{
			Q_memset (o, ' ', 20-l);
			key[20] = 0;
		}
		else
			*o = 0;
		ConMsg ("%s", key);

		if (!*s)
		{
			ConMsg ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;
		ConMsg ("%s\n", value);
	}
}

/*
==============
Info_WriteVars

==============
*/
void Info_WriteVars( char *s, FileHandle_t  fp )
{
	const ConVar *pcvar;

	char	pkey[512];
	static	char value[4][512];	// use two buffers so compares
								// work without stomping on each other
	static	int	valueindex;
	char	*o;

	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;
	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		pcvar = g_pCVar->FindVar( pkey );
		if ( !pcvar && pkey[0] != '*' )  // Don't store out * keys
		{
			g_pFileSystem->FPrintf( fp, "setinfo \"%s\" \"%s\"\n", 
				pkey, value[valueindex] );
		}

		if (!*s)
			return;
		s++;
	}
}
