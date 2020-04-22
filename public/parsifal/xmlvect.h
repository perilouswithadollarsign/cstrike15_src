/*===========================================================================
  xmlvect.h
    see parsifal.h for copyright info
===========================================================================*/
#ifndef XMLVECTOR__H
#define XMLVECTOR__H

#include <stddef.h>

#ifndef XMLAPI
#define XMLAPI
#endif

#ifndef BYTE
 #define BYTE unsigned char 
#endif

#ifndef COUNTBUFSIZE
#define COUNTBUFSIZE(cBytes, blocksize) \
((!(cBytes)) ? (blocksize) : (!( (cBytes) % (blocksize) ) ? (int)(cBytes) : (int)( (((cBytes) / (blocksize)) + 1) * (blocksize) )) )
#endif

typedef struct tagXMLVECTOR
{
	int length;
	int capacity;
	int capacityIncrement;
	int itemSize;
	BYTE *array;
} XMLVECTOR, *LPXMLVECTOR;

#ifdef __cplusplus
   extern "C" {
#endif

LPXMLVECTOR XMLAPI XMLVector_Create(LPXMLVECTOR *vector, int initialCapacity, int itemSize);
void XMLAPI *XMLVector_Replace(LPXMLVECTOR vector, int index, void *item);
int XMLAPI XMLVector_Remove(LPXMLVECTOR vector, int index);
void XMLAPI *XMLVector_Get(LPXMLVECTOR vector, int index);
int XMLAPI XMLVector_Resize(LPXMLVECTOR vector, int newsize);
void XMLAPI *XMLVector_Append(LPXMLVECTOR vector, void *item);
void XMLAPI *XMLVector_InsertBefore(LPXMLVECTOR vector, int index, void *item);
void XMLAPI XMLVector_Free(LPXMLVECTOR vector);

#define _XMLVector_RemoveAll(v) (XMLVector_Resize((v), 0))
#define _XMLVector_Get(v,index) \
    (((index) < 0 || (index) > ((v)->length - 1)) ? NULL : (((v)->array+((index)*(v)->itemSize))))
#define _XMLVector_GetP(vect,i,ptype) (*((ptype##**)XMLVector_Get(((LPXMLVECTOR)vect), ((int)i))))
/* e.g. _XMLVector_GetP(v, 0, FILE); expands to *((FILE**)XMLVector_Get(v, 0)) */
#define _XMLVector_GetIterP(v, iterP) ( (iterP) = (void*)((LPXMLVECTOR)v)->array, \
  ((LPXMLVECTOR)v)->array + (((LPXMLVECTOR)v)->length*((LPXMLVECTOR)v)->itemSize) )
/* GetIterP returns pointer to past the end of v->array, param 2 sets pointer to start */

#ifdef __cplusplus
   }
#endif /* __cplusplus */
#endif /* XMLVECTOR__H */



