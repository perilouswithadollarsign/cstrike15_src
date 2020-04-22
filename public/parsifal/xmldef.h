/*===========================================================================
  xmldef.h
    definitions, macros and function prototypes for parsifal.c
    see parsifal.h for copyright info
===========================================================================*/

#ifndef XMLDEF__H
#define XMLDEF__H

#include <assert.h>

#ifdef _MSC_VER
	#ifdef _DEBUG
		#include <crtdbg.h>
		#define _CRTDBG_MAP_ALLOC
	#endif
#define vsnprintf _vsnprintf
#endif

#define TOK_XMLNS "xmlns"
#define TOK_XML   "xml"   

#define UTF8_2BYTES		0xc0
#define UTF8_3BYTES		0xe0
#define UTF8_4BYTES		0xf0

#define UTF8LEN(c,o) \
if (!(*c & 0x80)) o = 1; \
else if ((unsigned int)*c <= 0xdf) o = 2; \
else if ((unsigned int)*c <= 0xef) o = 3; \
else if ((unsigned int)*c <= 0xf7) o = 4; \
else if ((unsigned int)*c <= 0xfb) o = 5; \
else o = 6;

#define XMLATT_NORMAL 0
#define XMLATT_WITHNS 1
#define XMLATT_PREFIXDECL 2
#define XMLATT_DEFAULTDECL 3
#define XMLATT_XMLPREFIXDECL 4

/* isspace gives us trouble trimming some utf-8 trailbytes... */
#ifdef isspace
#undef isspace
#endif
#define isspace(c) (ISMAPCH(whitespace, (c)))

#define SAFE_FREESTR(s) if (s) { free(s); s = (XMLCH*)NULL; }

/* BUFTOSTR. similar to BufferedIStream_ToString, but modifies 
(nul terminates) actual buffer. There's no bounds checking like in _ToString, 
also must be sure that returned string stays valid (no _Read operations)
Note that like in _ToString, last param is endPos, not length.
macro can be used when BufferedIStream is in pos ?> for example
and we can replace ? with \0 and use buffer as a string and
avoid memory/time consuming _ToString call. */
#define BUFTOSTR(buf,startPos,endPos) \
    ((!(startPos)) ? (*((buf)+(endPos))='\0', (buf)) : \
    (*((buf)+(endPos))='\0', ((buf)+(startPos))) )

/* ISMAPCH macro for comparing ascii map char */
#define ISMAPCH(map, byte) ((map)[(byte) >> 3] & (1 << ((byte) & 7)))
#define ISILLBYTE(c) (c < 32 && ISMAPCH(illByte, (c)))

/*
The code points U+D800 to U+DFFF will never be assigned to characters.
Other invalid sequences are code points 0xFFFF and 0xFFFE 
(EF,BF,BF and EF,BF,BE)

5.1 Single UTF-16 surrogates

5.1.1  U+D800 = ed a0 80
5.1.2  U+DB7F = ed ad bf
5.1.3  U+DB80 = ed ae 80
5.1.4  U+DBFF = ed af bf
5.1.5  U+DC00 = ed b0 80
5.1.6  U+DF80 = ed be 80
5.1.7  U+DFFF = ed bf bf
              
5.3 Other illegal code positions

5.3.1  U+FFFE = ef bf be
5.3.2  U+FFFF = ef bf bf

note:
sequence bytes c[1]-c[3] can never be < 0x80 (10000000)
and last byte can never be > 0xBF (10111111) 
these are ensured in UTF-8 conversion */

#define UTF8_ISILL3(c) \
((*c) == 0xEF && (c)[1] == 0xBF ? (c)[2] > 0xBD : \
(*c) == 0xED && (c)[1] > 0x9F)

/* see http://www.unicode.org/unicode/reports/tr28/ table 3.1B */
#define UTF8_ISILL4(c) ((*c) == 0xF4 && (c)[1] > 0x8F)

/* BISREADERDATA is put into parser->reader->userdata.
Each parsed entity has its own reader and its own
BISREADERDATA. The "main parser"'s
BISREADERDATA is allocated in Parser_Create and
initialized in Parser_Parse (see also InitEntityReader) */
typedef struct tagBISREADERDATA
{
	LPXMLPARSER parser;
	LPXMLENTITY curEnt; /* current entity (NULL for doc entity and internal entities) */
	int iCurPE;
	int line;
	int col;
	int stackLevel;
	int lEndian; /* flag to indicate that parser's using little-endian encoding, might 
				 not be correct when byte order mark isn't present */
	int context; /* reader specific context flags */
	XMLCH EncodingName[20];
} BISREADERDATA, *LPBISREADERDATA;

#define XMLREADERCTX_PE_SPACE 0x1

/* DECLATT represents DTD declared attribute. Array of DECLATTs
is stored in declAttTable hashtable for expansion of default attribute 
values (value member !NULL) or special normalization rules; if type
isn't CDATA normalization follow rules specified in XMLSPEC 
3.3.3 Attribute-Value Normalization */
struct tagDECLATT
{
	XMLCH *name, *value;
	int nameLen, prefixLen, valueLen, type, defaultDecl;
};

struct tagDTD
{
	int expandPEs, expandPEsaved, inLiteral, inclSect;
	LPXMLVECTOR peStack;
	XMLSTRINGBUF sbuf;
	XMLRUNTIMEATT att;
	LPBUFFEREDISTREAM refReader;
	LPXMLPOOL pePool;
};

/* STACK macro wrapper around Vector; can be used as tag stack in our
case, even though Vector is optimized for sequential index access.
Tag stack isn't likely to grow/shrink that much in xml parsing when
using CapacityIncrement 16 x RUNTIMETAG (tag nesting level).
STACK_POP always removes last item from Vector and that is efficient
too. Note: Vector handles bounds checking in _Get and in _Remove */
#define STACK_PUSH(stack,item) (XMLVector_Append((stack), (item)))
#define STACK_PEEK(stack) (XMLVector_Get((stack),(stack)->length-1))
#define STACK_REMOVE(stack) (XMLVector_Remove((stack), (stack)->length-1))
#define STACK_POP(stack,item) \
( ((stack)->length) ? (memcpy((item), STACK_PEEK((stack)), (stack)->itemSize), \
STACK_REMOVE((stack)), (item)) : NULL)

extern size_t Latin1ToUtf8 (LPBUFFEREDISTREAM r, const BYTE **inbuf, size_t *inbytesleft, BYTE **outbuf, size_t *outbytesleft);
extern size_t Utf8ToUtf8 (LPBUFFEREDISTREAM r, const BYTE **inbuf, size_t *inbytesleft, BYTE **outbuf, size_t *outbytesleft);
#ifdef ICONV_SUPPORT
extern size_t iconvWrapper (LPBUFFEREDISTREAM r, const BYTE **inbuf, size_t *inbytesleft, BYTE **outbuf, size_t *outbytesleft);
#endif

static XMLCH EmptyStr[1] = {'\0'};
static XMLCH *uriXMLNS = "http://www.w3.org/2000/xmlns/";
static XMLCH *uriXML = "http://www.w3.org/XML/1998/namespace";

#define EINPUT(r) (((r) < -2) ? (SetReaderFatal(((LPXMLPARSER)parser), (r)), 1) : 0)
#define ISXMLPREFIX(s) ((*(s) == 'x' && s[1] == 'm' && s[2] == 'l'))
#define ISXMLNSPREFIX(s) ((ISXMLPREFIX((s)) && s[3] == 'n' && s[4] == 's'))
#define ISQUOTE(c) ((c)=='\"' || (c)=='\'')
#define REQUIRE_WS(r) (((r = SkipWS(parser))==0) ? \
  ErP_(parser, ERR_XMLP_WS_REQUIRED, 0) : ((r==-1) ? 0 : r))

#define DPOS(bytes) \
  PREADER->pos-=(bytes); \
  PREADERDATA->col-=(bytes);
#define IPOS(bytes) \
  PREADER->pos+=(bytes); \
  PREADERDATA->col+=(bytes);
#define DPOS_LF(bytes) \
  if (PREADER->buf[PREADER->pos-1] == 0x0A) \
    PREADERDATA->line--; \
  DPOS(bytes);

#define DTDTOK_START(PEs) (\
  RT->dtd->expandPEsaved = RT->dtd->expandPEs, \
  RT->dtd->expandPEs = PEs \
)
#define DTDTOK_END \
  if (RT->dtd->expandPEs != RT->dtd->expandPEsaved) \
    RT->dtd->expandPEs = RT->dtd->expandPEsaved

/* some shortcuts: */
#define RT parser->prt
#define PREADER ((LPBUFFEREDISTREAM)parser->reader)
#define PREADERDATA ((LPBISREADERDATA)PREADER->userdata)
#define HANDLER(n) parser->n##Handler
#define PEEKINPUT(str,len) BufferedIStream_Peek(PREADER,(str),(len),0)
#define CURCHAR (assert(PREADER->pos<PREADER->bytesavail), PREADER->buf[PREADER->pos])

/* programmatically generated trie tables (reTRIEval algorithm). Trie algo
 suits our needs very well - brute force strcmp can infact
 be more efficient for small dictionarys (usually strcmp is well optimized and
 fast) but since we must use ReadCh (in TrieTok()) and memcmp (in TrieRaw() 
 via BufferedIStream_Peek) we're getting perfomance gain. Especially DTD token
 parsing benefits from tries - no excessive ReadCh calls and UTF-8 checks
 anymore etc. See TrieTok and TrieRaw */

struct trie {
	const char c; /* current char to test */
	const struct trie *n; /* next char/trie or token number if c is '\0' */
	const struct trie *sib; /* choice or NULL if there is none */
};

#define T_N_ TRxmlTok
static struct trie const T_N_[] = {
/* 0   */ {'!', T_N_+1, T_N_+9},
/* 1   */ {'[', T_N_+2, T_N_+11},
/* 2   */ {'C', T_N_+3, NULL},
/* 3   */ {'D', T_N_+4, NULL},
/* 4   */ {'A', T_N_+5, NULL},
/* 5   */ {'T', T_N_+6, NULL},
/* 6   */ {'A', T_N_+7, NULL},
/* 7   */ {'[', T_N_+8, NULL},
/* 8   */ {'\0', (struct trie*)1, NULL},
/* 9   */ {'?', T_N_+10, T_N_+14},
/* 10  */ {'\0', (struct trie*)2, NULL},
/* 11  */ {'-', T_N_+12, T_N_+16},
/* 12  */ {'-', T_N_+13, NULL},
/* 13  */ {'\0', (struct trie*)3, NULL},
/* 14  */ {'/', T_N_+15, NULL},
/* 15  */ {'\0', (struct trie*)4, NULL},
/* 16  */ {'D', T_N_+17, NULL},
/* 17  */ {'O', T_N_+18, NULL},
/* 18  */ {'C', T_N_+19, NULL},
/* 19  */ {'T', T_N_+20, NULL},
/* 20  */ {'Y', T_N_+21, NULL},
/* 21  */ {'P', T_N_+22, NULL},
/* 22  */ {'E', T_N_+23, NULL},
/* 23  */ {'\0', (struct trie*)5, NULL}
};
#undef T_N_

#define T_N_ TRstdEnt
static struct trie const T_N_[] = {
/* 0   */ {'g', T_N_+1, T_N_+4},
/* 1   */ {'t', T_N_+2, NULL},
/* 2   */ {';', T_N_+3, NULL},
/* 3   */ {'\0', (struct trie*)1, NULL},
/* 4   */ {'l', T_N_+5, T_N_+8},
/* 5   */ {'t', T_N_+6, NULL},
/* 6   */ {';', T_N_+7, NULL},
/* 7   */ {'\0', (struct trie*)2, NULL},
/* 8   */ {'a', T_N_+9, T_N_+18},
/* 9   */ {'m', T_N_+10, T_N_+13},
/* 10  */ {'p', T_N_+11, NULL},
/* 11  */ {';', T_N_+12, NULL},
/* 12  */ {'\0', (struct trie*)3, NULL},
/* 13  */ {'p', T_N_+14, NULL},
/* 14  */ {'o', T_N_+15, NULL},
/* 15  */ {'s', T_N_+16, NULL},
/* 16  */ {';', T_N_+17, NULL},
/* 17  */ {'\0', (struct trie*)4, NULL},
/* 18  */ {'q', T_N_+19, NULL},
/* 19  */ {'u', T_N_+20, NULL},
/* 20  */ {'o', T_N_+21, NULL},
/* 21  */ {'t', T_N_+22, NULL},
/* 22  */ {';', T_N_+23, NULL},
/* 23  */ {'\0', (struct trie*)5, NULL}
};
#undef T_N_

#define T_N_ TRxmlDecl
static struct trie const T_N_[] = {
/* 0   */ {'v', T_N_+1, T_N_+8},
/* 1   */ {'e', T_N_+2, NULL},
/* 2   */ {'r', T_N_+3, NULL},
/* 3   */ {'s', T_N_+4, NULL},
/* 4   */ {'i', T_N_+5, NULL},
/* 5   */ {'o', T_N_+6, NULL},
/* 6   */ {'n', T_N_+7, NULL},
/* 7   */ {'\0', (struct trie*)1, NULL},
/* 8   */ {'e', T_N_+9, T_N_+17},
/* 9   */ {'n', T_N_+10, NULL},
/* 10  */ {'c', T_N_+11, NULL},
/* 11  */ {'o', T_N_+12, NULL},
/* 12  */ {'d', T_N_+13, NULL},
/* 13  */ {'i', T_N_+14, NULL},
/* 14  */ {'n', T_N_+15, NULL},
/* 15  */ {'g', T_N_+16, NULL},
/* 16  */ {'\0', (struct trie*)2, NULL},
/* 17  */ {'s', T_N_+18, NULL},
/* 18  */ {'t', T_N_+19, NULL},
/* 19  */ {'a', T_N_+20, NULL},
/* 20  */ {'n', T_N_+21, NULL},
/* 21  */ {'d', T_N_+22, NULL},
/* 22  */ {'a', T_N_+23, NULL},
/* 23  */ {'l', T_N_+24, NULL},
/* 24  */ {'o', T_N_+25, NULL},
/* 25  */ {'n', T_N_+26, NULL},
/* 26  */ {'e', T_N_+27, NULL},
/* 27  */ {'\0', (struct trie*)3, NULL}
};
#undef T_N_

#define T_N_ TRexternalID
struct trie T_N_[] = {
/* 0   */ {'P', T_N_+1, T_N_+7},
/* 1   */ {'U', T_N_+2, NULL},
/* 2   */ {'B', T_N_+3, NULL},
/* 3   */ {'L', T_N_+4, NULL},
/* 4   */ {'I', T_N_+5, NULL},
/* 5   */ {'C', T_N_+6, NULL},
/* 6   */ {'\0', (struct trie*)1, NULL},
/* 7   */ {'S', T_N_+8, NULL},
/* 8   */ {'Y', T_N_+9, NULL},
/* 9   */ {'S', T_N_+10, NULL},
/* 10  */ {'T', T_N_+11, NULL},
/* 11  */ {'E', T_N_+12, NULL},
/* 12  */ {'M', T_N_+13, NULL},
/* 13  */ {'\0', (struct trie*)2, NULL}
};
#undef T_N_

#ifdef DTD_SUPPORT

#define T_N_ TRdtdTok
static struct trie const T_N_[] = {
/* 0   */ {'<', T_N_+1, NULL},
/* 1   */ {'!', T_N_+2, T_N_+7},
/* 2   */ {'-', T_N_+3, T_N_+5},
/* 3   */ {'-', T_N_+4, NULL},
/* 4   */ {'\0', (struct trie*)1, NULL},
/* 5   */ {'[', T_N_+6, T_N_+9},
/* 6   */ {'\0', (struct trie*)2, NULL},
/* 7   */ {'?', T_N_+8, NULL},
/* 8   */ {'\0', (struct trie*)3, NULL},
/* 9   */ {'E', T_N_+10, T_N_+23},
/* 10  */ {'N', T_N_+11, T_N_+16},
/* 11  */ {'T', T_N_+12, NULL},
/* 12  */ {'I', T_N_+13, NULL},
/* 13  */ {'T', T_N_+14, NULL},
/* 14  */ {'Y', T_N_+15, NULL},
/* 15  */ {'\0', (struct trie*)4, NULL},
/* 16  */ {'L', T_N_+17, NULL},
/* 17  */ {'E', T_N_+18, NULL},
/* 18  */ {'M', T_N_+19, NULL},
/* 19  */ {'E', T_N_+20, NULL},
/* 20  */ {'N', T_N_+21, NULL},
/* 21  */ {'T', T_N_+22, NULL},
/* 22  */ {'\0', (struct trie*)5, NULL},
/* 23  */ {'A', T_N_+24, T_N_+31},
/* 24  */ {'T', T_N_+25, NULL},
/* 25  */ {'T', T_N_+26, NULL},
/* 26  */ {'L', T_N_+27, NULL},
/* 27  */ {'I', T_N_+28, NULL},
/* 28  */ {'S', T_N_+29, NULL},
/* 29  */ {'T', T_N_+30, NULL},
/* 30  */ {'\0', (struct trie*)6, NULL},
/* 31  */ {'N', T_N_+32, NULL},
/* 32  */ {'O', T_N_+33, NULL},
/* 33  */ {'T', T_N_+34, NULL},
/* 34  */ {'A', T_N_+35, NULL},
/* 35  */ {'T', T_N_+36, NULL},
/* 36  */ {'I', T_N_+37, NULL},
/* 37  */ {'O', T_N_+38, NULL},
/* 38  */ {'N', T_N_+39, NULL},
/* 39  */ {'\0', (struct trie*)7, NULL}
};
#undef T_N_

#define T_N_ TRattType
static struct trie const T_N_[] = {
/* 0   */ {'C', T_N_+1, T_N_+6},
/* 1   */ {'D', T_N_+2, NULL},
/* 2   */ {'A', T_N_+3, NULL},
/* 3   */ {'T', T_N_+4, NULL},
/* 4   */ {'A', T_N_+5, NULL},
/* 5   */ {'\0', (struct trie*)1, NULL},
/* 6   */ {'I', T_N_+7, T_N_+15},
/* 7   */ {'D', T_N_+8, NULL},
/* 8   */ {'\0', (struct trie*)2, T_N_+9},
/* 9   */ {'R', T_N_+10, NULL},
/* 10  */ {'E', T_N_+11, NULL},
/* 11  */ {'F', T_N_+12, NULL},
/* 12  */ {'\0', (struct trie*)3, T_N_+13},
/* 13  */ {'S', T_N_+14, NULL},
/* 14  */ {'\0', (struct trie*)4, NULL},
/* 15  */ {'E', T_N_+16, T_N_+26},
/* 16  */ {'N', T_N_+17, NULL},
/* 17  */ {'T', T_N_+18, NULL},
/* 18  */ {'I', T_N_+19, NULL},
/* 19  */ {'T', T_N_+20, NULL},
/* 20  */ {'Y', T_N_+21, T_N_+22},
/* 21  */ {'\0', (struct trie*)5, NULL},
/* 22  */ {'I', T_N_+23, NULL},
/* 23  */ {'E', T_N_+24, NULL},
/* 24  */ {'S', T_N_+25, NULL},
/* 25  */ {'\0', (struct trie*)6, NULL},
/* 26  */ {'N', T_N_+27, NULL},
/* 27  */ {'M', T_N_+28, T_N_+36},
/* 28  */ {'T', T_N_+29, NULL},
/* 29  */ {'O', T_N_+30, NULL},
/* 30  */ {'K', T_N_+31, NULL},
/* 31  */ {'E', T_N_+32, NULL},
/* 32  */ {'N', T_N_+33, NULL},
/* 33  */ {'\0', (struct trie*)7, T_N_+34},
/* 34  */ {'S', T_N_+35, NULL},
/* 35  */ {'\0', (struct trie*)8, NULL},
/* 36  */ {'O', T_N_+37, NULL},
/* 37  */ {'T', T_N_+38, NULL},
/* 38  */ {'A', T_N_+39, NULL},
/* 39  */ {'T', T_N_+40, NULL},
/* 40  */ {'I', T_N_+41, NULL},
/* 41  */ {'O', T_N_+42, NULL},
/* 42  */ {'N', T_N_+43, NULL},
/* 43  */ {'\0', (struct trie*)9, NULL}
};
#undef T_N_

#define T_N_ TRdefDecls
static struct trie const T_N_[] = {
/* 0   */ {'#', T_N_+1, NULL},
/* 1   */ {'F', T_N_+2, T_N_+7},
/* 2   */ {'I', T_N_+3, NULL},
/* 3   */ {'X', T_N_+4, NULL},
/* 4   */ {'E', T_N_+5, NULL},
/* 5   */ {'D', T_N_+6, NULL},
/* 6   */ {'\0', (struct trie*)1, NULL},
/* 7   */ {'R', T_N_+8, T_N_+16},
/* 8   */ {'E', T_N_+9, NULL},
/* 9   */ {'Q', T_N_+10, NULL},
/* 10  */ {'U', T_N_+11, NULL},
/* 11  */ {'I', T_N_+12, NULL},
/* 12  */ {'R', T_N_+13, NULL},
/* 13  */ {'E', T_N_+14, NULL},
/* 14  */ {'D', T_N_+15, NULL},
/* 15  */ {'\0', (struct trie*)2, NULL},
/* 16  */ {'I', T_N_+17, NULL},
/* 17  */ {'M', T_N_+18, NULL},
/* 18  */ {'P', T_N_+19, NULL},
/* 19  */ {'L', T_N_+20, NULL},
/* 20  */ {'I', T_N_+21, NULL},
/* 21  */ {'E', T_N_+22, NULL},
/* 22  */ {'D', T_N_+23, NULL},
/* 23  */ {'\0', (struct trie*)3, NULL}
};
#undef T_N_

#define T_N_ TRelTypes
static struct trie const T_N_[] = {
/* 0   */ {'E', T_N_+1, T_N_+6},
/* 1   */ {'M', T_N_+2, NULL},
/* 2   */ {'P', T_N_+3, NULL},
/* 3   */ {'T', T_N_+4, NULL},
/* 4   */ {'Y', T_N_+5, NULL},
/* 5   */ {'\0', (struct trie*)1, NULL},
/* 6   */ {'A', T_N_+7, T_N_+10},
/* 7   */ {'N', T_N_+8, NULL},
/* 8   */ {'Y', T_N_+9, NULL},
/* 9   */ {'\0', (struct trie*)2, NULL},
/* 10  */ {'(', T_N_+11, NULL},
/* 11  */ {'\0', (struct trie*)3, NULL}
};
#undef T_N_

#endif /* ifdef DTD_SUPPORT */

#endif /* XMLDEF__H */
