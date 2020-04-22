#ifndef TIER1_STRTOOLS_INLINES_H
#define TIER1_STRTOOLS_INLINES_H

inline int	_V_strlen_inline( const char *str )
{
#ifdef POSIX
	if ( !str )
		return 0;
#endif
	return strlen( str );
}

inline char *_V_strrchr_inline( const char *s, char c )
{
	int len = _V_strlen_inline(s);
	s += len;
	while (len--)
		if (*--s == c) return (char *)s;
	return 0;
}

inline int _V_wcscmp_inline( const wchar_t *s1, const wchar_t *s2 )
{
	while (1)
	{
		if (*s1 != *s2)
			return -1;              // strings not equal    
		if (!*s1)
			return 0;               // strings are equal
		s1++;
		s2++;
	}

	return -1;
}

#define STRTOOLS_TOLOWERC( x )  (( ( x >= 'A' ) && ( x <= 'Z' ) )?( x + 32 ) : x )
inline int	_V_stricmp_inline( const char *s1, const char *s2 )
{
#ifdef POSIX
	if ( s1 == NULL && s2 == NULL )
		return 0;
	if ( s1 == NULL )
		return -1;
	if ( s2 == NULL )
		return 1;

	return stricmp( s1, s2 );
#else	
	// THIS BLOCK ISN'T USED ON THE PS3 SINCE IT IS POSIX!!!  Would be a code bloat concern otherwise.
	uint8 const *pS1 = ( uint8 const * ) s1;
	uint8 const *pS2 = ( uint8 const * ) s2;
	for(;;)
	{
		int c1 = *( pS1++ );
		int c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			if ( !c1 ) return 0;
		}
		else
		{
			if ( ! c2 )
			{
				return c1 - c2;
			}
			c1 = TOLOWERC( c1 );
			c2 = TOLOWERC( c2 );
			if ( c1 != c2 )
			{
				return c1 - c2;
			}
		}
		c1 = *( pS1++ );
		c2 = *( pS2++ );
		if ( c1 == c2 )
		{
			if ( !c1 ) return 0;
		}
		else
		{
			if ( ! c2 )
			{
				return c1 - c2;
			}
			c1 = STRTOOLS_TOLOWERC( c1 );
			c2 = STRTOOLS_TOLOWERC( c2 );
			if ( c1 != c2 )
			{
				return c1 - c2;
			}
		}
	}
#endif
}

inline char *_V_strstr_inline( const char *s1, const char *search )
{
#if defined( _X360 )
	return (char *)strstr( (char *)s1, search );
#else
	return (char *)strstr( s1, search );
#endif
}

#endif // TIER1_STRTOOLS_INLINES_H
