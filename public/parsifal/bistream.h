/*===========================================================================
  bistream.h
    see parsifal.h for copyright info
===========================================================================*/
#ifndef BISTREAM__H
#define BISTREAM__H

#include <stddef.h>
#include <limits.h>
#ifdef ICONV_SUPPORT
#include "iconv.h"
#endif

#ifndef BYTE
 #define BYTE unsigned char
#endif

#ifndef COUNTBUFSIZE
#define COUNTBUFSIZE(cBytes, blocksize) \
((!(cBytes)) ? (blocksize) : (!( (cBytes) % (blocksize) ) ? (int)(cBytes) : (int)( (((cBytes) / (blocksize)) + 1) * (blocksize) )) )
#endif

#define BIS_DEFAULT_MAXBUFSIZE INT_MAX
#define BIS_DEFAULT_BLOCKSIZE 512
#define BIS_CHAR_MAX 16

enum BIS_ERRORS {	BIS_ERR_MEMALLOC = -40,			
					BIS_ERR_MAXBUF, 
					BIS_ERR_INVALIDARG,
					BIS_ERR_ENCODING,
					BIS_ERR_INPUT,
					BIS_EOF = 1 };

typedef int (*LPFNINPUTSRC)(BYTE *buf, int cBytes, int *cBytesActual, void *inputData);

typedef struct tagBUFFEREDISTREAM 
{
	BYTE *buf;
	BYTE *inbuf;
	int bufsize;
	int maxbufsize;
	int blocksize;
	int bytesavail;
	int pos;
	int eof;
	int err;
	int encerr;
	int inbufrest;
	void *userdata;
	void *inputData;
	LPFNINPUTSRC inputsrc;
	size_t (*encode) (struct tagBUFFEREDISTREAM *reader, const BYTE **inbuf, size_t *inbytesleft, BYTE **outbuf, size_t *outbytesleft);
#ifdef ICONV_SUPPORT
	iconv_t cd;
#endif
} BUFFEREDISTREAM, *LPBUFFEREDISTREAM;

typedef size_t (*LPFNENCODE) (LPBUFFEREDISTREAM r, const BYTE **inbuf, size_t *inbytesleft, BYTE **outbuf, size_t *outbytesleft);

#ifdef __cplusplus
   extern "C" {
#endif

int BufferedIStream_EncodeBuffer(LPBUFFEREDISTREAM r);

int BufferedIStream_Peek(LPBUFFEREDISTREAM r, 
						const BYTE *tok, 
						int len, 
						int offset);

int BufferedIStream_ResetBuf(LPBUFFEREDISTREAM r, 
						 int numBytes);

LPBUFFEREDISTREAM BufferedIStream_Init(LPBUFFEREDISTREAM r, 
						int blocksize);

int BufferedIStream_AppendBytes(LPBUFFEREDISTREAM r, 
						 const BYTE *bytes, 
						 int len);

void BufferedIStream_Free(LPBUFFEREDISTREAM r);

#ifdef __cplusplus
   }
#endif /* __cplusplus */
#endif /* BISTREAM__H */



