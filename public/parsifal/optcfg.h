/*===========================================================================
  optcfg.h
    This header contains optimization settings for Parsifal library
  TODO
    A lot. Currently contains only inline versions of some routines to be 
    selectively replaced in the critical areas of code. And yes, these
    critical areas are identified by running profiler ;-)
===========================================================================*/

#ifdef _MSC_VER
	#define INLINE __forceinline
#else
	#ifdef __GNUC__
		#define INLINE __inline__ __attribute__((always_inline))
	#else
		#define INLINE inline
	#endif
#endif

#define GROWSBUF_OPT(l) \
if (sbuf->usePool) { \
  if ((sbuf->len + (l)) > sbuf->pool->itemSize) { \
    XMLCH *ts = sbuf->str; \
    sbuf->usePool = 0; \
    sbuf->capacity = COUNTBUFSIZE((sbuf->len + (l)), sbuf->blocksize); \
    sbuf->str = (XMLCH*)malloc(sbuf->capacity * sizeof(XMLCH)); \
    if (!sbuf->str) return ((XMLCH*)NULL); \
    memcpy(sbuf->str, ts, sbuf->len); \
    XMLPool_Free(sbuf->pool, ts); \
  } \
} \
else if ((sbuf->len + (l)) > sbuf->capacity) { \
  sbuf->capacity = COUNTBUFSIZE((sbuf->len + (l)), sbuf->blocksize); \
  sbuf->str = (XMLCH*)realloc(sbuf->str, sbuf->capacity * sizeof(XMLCH)); \
  if (!sbuf->str) return ((XMLCH*)NULL); \
}

static INLINE XMLCH *XMLStringbuf_Append_Opt(LPXMLSTRINGBUF sbuf, XMLCH *str, int len)
{	
	GROWSBUF_OPT(len);
	if (len == 1) /* gives a slight performance gain */
		sbuf->str[sbuf->len++] = *str;
	else {
		memcpy(sbuf->str+sbuf->len, str, len);
		sbuf->len += len;
	}
	return (sbuf->str);
}


static INLINE XMLCH *ReadCh_Opt(LPXMLPARSER parser, int *chSize)
{	
	XMLCH *c;
	int ret;

	if (!PREADER->buf || PREADER->pos >= PREADER->bytesavail) {
		ret = PEEKINPUT((const BYTE*)NULL, 1);
		if (EINPUT(ret) || ret) {
			#ifdef DTD_SUPPORT
			if (RT->dtd && !parser->ErrorCode && 
				RT->dtd->peStack->length &&
				((*((LPBUFFEREDISTREAM*)STACK_PEEK(RT->dtd->peStack)))) == PREADER)
				return ReadPERefEnd(parser, chSize);
			else {
			#endif
				*chSize = 0;
				return (XMLCH*)NULL;
			#ifdef DTD_SUPPORT
			}
			#endif
		}
	}
	c = PREADER->buf+PREADER->pos;
	UTF8LEN(c,*chSize);

	if (*chSize == 1) {
		if (ISILLBYTE(*c)) {
			*chSize = 0;
			ErP_(parser, ERR_XMLP_ILLEGAL_CHAR, 1);
			return (XMLCH*)NULL;
		}

		PREADER->pos++;
		if (*c == 0xD) {	
			PREADERDATA->line++;
			PREADERDATA->col=0;
			PREADER->buf[PREADER->pos-1] = 0xA;
			if (PREADER->pos >= PREADER->bytesavail) {
				ret = PEEKINPUT((const BYTE*)NULL, 1);
				if (EINPUT(ret)) {
					*chSize = 0;
					return (XMLCH*)NULL;
				}
				c = PREADER->buf+(PREADER->pos-1);
				if (ret) return(c);
			}
			if (CURCHAR == 0xA) PREADER->pos++;
		}
		else if (*c == 0xA) {
			PREADERDATA->line++;
			PREADERDATA->col=0;
		}
		#ifdef DTD_SUPPORT
		else if (*c == '%' && RT->dtd && RT->dtd->expandPEs) {
			PREADERDATA->col++;		
			c = ReadPERefStart(parser, chSize);
		}
		#endif
		else PREADERDATA->col++;
	}
	else {
		if (*chSize == 3 && UTF8_ISILL3(c)) {
			*chSize = 0;
			ErP_(parser, ERR_XMLP_ILLEGAL_CHAR, 0);
			return (XMLCH*)NULL;
		}
		else if (*chSize == 4 && UTF8_ISILL4(c)) {
			*chSize = 0;
			ErP_(parser, ERR_XMLP_ILLEGAL_CHAR, 0);
			return (XMLCH*)NULL;
		}
		PREADER->pos += *chSize;
		PREADERDATA->col += *chSize;
	}
	return(c);		
}

