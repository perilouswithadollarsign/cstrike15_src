/*===========================================================================
  xmlcfg.h
    Global/platform spesific definitions + configuration for 
    GNU libiconv support etc. 
===========================================================================*/
#ifndef XMLCFG__H
#define XMLCFG__H

#ifdef ICONV_SUPPORT
	#include "iconv.h"
#else
/*   definitions for BufferedIStream encoding callback's encerr error values 
    (same as errnos set by iconv function) */
	#include <errno.h>
	#ifndef E2BIG
		/* there isn't enough space in outbuf for multibyte char -
			request for enlarge the buffer */
		#define E2BIG 7
	#endif
	#ifndef EINVAL
		/* partial character in inbuf */
		#define EINVAL 22
	#endif
	#ifndef EILSEQ
		/* illegal character in inbuf */
		#define EILSEQ 42
	#endif
#endif /* ICONV_SUPPORT */

/* util routines: */
#ifndef HAS_STRICMP
extern int stricmp(const char *s1, const char *s2);
#endif
extern unsigned char* xmlMemdup(unsigned char* buf, int len);

/* <stdint.h> would do the trick... but: */
#if (defined(__GNUC__) && defined(__i386__) && !defined(UINT16))
#define UINT16          unsigned short int
#define INT16           short int
#define UINT32          unsigned long int
#define INT32           long int
#endif

#if (defined(_MSC_VER) && !defined(UINT16))
#define UINT16          unsigned short int
#define INT16           short int
#define UINT32          unsigned long int
#define INT32           long int
#endif

#if (defined(__BORLANDC__) && !defined(UINT16))
#define UINT16          unsigned short int
#define INT16           short int
#define UINT32          unsigned long int
#define INT32           long int
#endif

#if (defined(VAXC) && !defined(UINT16))
#define UINT16          unsigned short int
#define INT16           short int
#define UINT32          unsigned long int
#define INT32           long int
#endif

#if (defined(_IBMR2) && !defined(UINT16))
#define UINT16          unsigned short int
#define INT16           short int
#define UINT32          unsigned long int
#define INT32           long int
#endif

#if (defined(sparc) && !defined(UINT16))
#define UINT16          unsigned short int
#define INT16           short int
#define UINT32          unsigned int
#define INT32           int
#endif

#if (defined(__mips) && !defined(UINT16))
#define UINT16          unsigned short int
#define INT16           short int
#define UINT32          unsigned int
#define INT32           int
#endif

#ifndef UINT32
#error "Error: UINT32 cannot be determined for your platform - You must define it manually."
#endif

#endif /* XMLCFG__H */


