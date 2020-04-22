/*===========================================================================
  isrcmem.h
    Provides helper macros and typedefs for parsing memory buffers
    see parsifal.h for copyright info
  
  USAGE
    declare inputsource handler function (note param names
    must be exactly buf, cBytes etc.):
    
    int MemInputsrc(BYTE *buf, int cBytes, int *cBytesActual, void *inputData);
    int MemInputsrc(BYTE *buf, int cBytes, int *cBytesActual, void *inputData)
    {   
        XMLMEMINPUTSRC_HANDLE
    }
    
    .....
    
    LPXMLPARSER parser;     
    XMLMEMINPUTSRC meminput;
    char *xml = "<root><child1/><child2>text</child2></root>";
    
    XMLMEMINPUTSRC_INIT(&meminput, xml, strlen(xml));
    XMLParser_Create(&parser);
    XMLParser_Parse(parser, MemInputsrc, &meminput, NULL);
    
    .....
===========================================================================*/

#ifndef ISRCMEM__H
#define ISRCMEM__H

typedef struct tagXMLMEMINPUTSRC
{
    BYTE *pBuf;
    unsigned long cTotal;
    unsigned long cBytes;
} XMLMEMINPUTSRC, *LPXMLMEMINPUTSRC;

#define XMLMEMINPUTSRC_INIT(lpMemISrc,buf,size) \
    (((LPXMLMEMINPUTSRC)lpMemISrc)->cTotal=(size), \
    ((LPXMLMEMINPUTSRC)lpMemISrc)->cBytes=0, \
    ((LPXMLMEMINPUTSRC)lpMemISrc)->pBuf=(buf))

#define XMLMEMINPUTSRC_HANDLE \
    if ((((LPXMLMEMINPUTSRC)inputData)->cBytes + cBytes) < ((LPXMLMEMINPUTSRC)inputData)->cTotal) { \
        memcpy(buf, ((LPXMLMEMINPUTSRC)inputData)->pBuf+((LPXMLMEMINPUTSRC)inputData)->cBytes, cBytes); \
        *cBytesActual = cBytes; \
        ((LPXMLMEMINPUTSRC)inputData)->cBytes += cBytes; \
        return 0; \
    } \
    else { \
        *cBytesActual = ((LPXMLMEMINPUTSRC)inputData)->cTotal - ((LPXMLMEMINPUTSRC)inputData)->cBytes; \
        if (*cBytesActual) { \
            memcpy(buf, ((LPXMLMEMINPUTSRC)inputData)->pBuf+((LPXMLMEMINPUTSRC)inputData)->cBytes, *cBytesActual); \
            ((LPXMLMEMINPUTSRC)inputData)->cBytes += *cBytesActual; } \
        return 1; \
    }

#endif /* ISRCMEM__H */


