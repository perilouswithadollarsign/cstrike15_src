/*===========================================================================
  xmlsbuf.h
    see parsifal.h for copyright info
===========================================================================*/
#ifndef XMLSBUF__H
#define XMLSBUF__H

#include <stddef.h>
#include "xmlpool.h"

#ifndef XMLAPI
#define XMLAPI
#endif

#ifndef BYTE
 #define BYTE unsigned char 
#endif

#ifndef XMLCH_DEFINED
	#define XMLCH_DEFINED
	typedef unsigned char XMLCH;
#endif

#ifndef COUNTBUFSIZE
#define COUNTBUFSIZE(cBytes, blocksize) \
((!(cBytes)) ? (blocksize) : (!( (cBytes) % (blocksize) ) ? (int)(cBytes) : (int)( (((cBytes) / (blocksize)) + 1) * (blocksize) )) )
#endif

typedef struct tagXMLSTRINGBUF
{
	int capacity;
	int blocksize;
	int len;
	int usePool;
	XMLCH *str;
	LPXMLPOOL pool;
} XMLSTRINGBUF, *LPXMLSTRINGBUF;

#ifdef __cplusplus
   extern "C" {
#endif

XMLCH XMLAPI *XMLStringbuf_Append(LPXMLSTRINGBUF sbuf, XMLCH *str, int len);
XMLCH XMLAPI *XMLStringbuf_AppendCh(LPXMLSTRINGBUF sbuf, XMLCH c);
XMLCH XMLAPI *XMLStringbuf_Init(LPXMLSTRINGBUF sbuf, int blockSize, int initSize);
XMLCH XMLAPI *XMLStringbuf_InitUsePool(LPXMLSTRINGBUF sbuf, int blockSize, int initSize, LPXMLPOOL pool);
XMLCH XMLAPI *XMLStringbuf_ToString(LPXMLSTRINGBUF sbuf);
int XMLAPI XMLStringbuf_SetLength(LPXMLSTRINGBUF sbuf, int len);
void XMLAPI XMLStringbuf_Free(LPXMLSTRINGBUF sbuf);

#ifdef __cplusplus
   }
#endif /* __cplusplus */
#endif /* XMLSBUF__H */



