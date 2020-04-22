/*===========================================================================
  xmlpool.h
    Based heavily on alloc-pool.c in gcc (some old code)
  TODO
    - String interning (wraps xmlpool object?)
    - Automatically free blocks when all items are unused (in XMLPool_Free)
      Add itemsAllocatedThisBlock tracking var to headers?
===========================================================================*/
#ifndef XMLPOOL__H
#define XMLPOOL__H

#include <stddef.h>

#ifndef XMLAPI
#define XMLAPI
#endif

typedef struct tagLPXMLPOOLLIST 
{
	struct tagLPXMLPOOLLIST *next;
} *LPXMLPOOLLIST;

typedef struct tagLPXMLPOOL 
{
	int itemSize;
	int itemsPerBlock; 
	int itemsAllocated;
	int itemsFree;
	int blocksAllocated; 
	int blockSize; 
	LPXMLPOOLLIST freeList;
	LPXMLPOOLLIST blockList;
} *LPXMLPOOL;

#ifdef __cplusplus
   extern "C" {
#endif

LPXMLPOOL XMLAPI XMLPool_Create(int itemSize, int itemsPerBlock);
void XMLAPI XMLPool_FreePool(LPXMLPOOL pool);
void XMLAPI *XMLPool_Alloc(LPXMLPOOL pool);
void XMLAPI XMLPool_Free(LPXMLPOOL pool, void *ptr);

#ifdef __cplusplus
   }
#endif /* __cplusplus */
#endif /* XMLPOOL__H */


