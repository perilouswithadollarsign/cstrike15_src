//  Copyright Electonic Arts(C) 2006 - All Rights Reserved

#ifndef TLFONT_H
#define TLFONT_H

#include "../materialsystem/ifont.h"

// Forward declerations
class IFileSystem;
class CTLFont;
namespace TLFont
{
    class FusionRasterizer;
}
namespace TLFontAux
{
    class FontAuxTexture;
}

// Cache TTF font file
#define MAX_FONTDATACACHE 20

struct sFontDataCache
{
	char				m_dataName[256];
	unsigned char *		m_dataMem;
	unsigned int		m_dataSize;
	int					m_refCount;
};

// Font manager
class CTLFontManager
{
public:

    CTLFontManager(IFileSystem *pFileSystem);
    ~CTLFontManager();

    CTLFont*     CreateFont(const char *pName, const char *pFontPath, int tall, int weight);
    void         DestroyFont(CTLFont *pFont);

private:

    unsigned char* LoadFontFile(const char *pFontPath, unsigned int *pDataSize);

    IFileSystem                *m_pFileSystem;

	sFontDataCache				m_fontDataCache[MAX_FONTDATACACHE];
};

// Font instance
class CTLFont : public IFont
{
public:

    CTLFont(const char *pName, unsigned char *pData, unsigned int dataSize, int tall, int weight);
    ~CTLFont();

    virtual void RenderToBuffer(int ch, int offsetx, int width, int height, unsigned char *pBuffer);
    virtual bool GetCharABCWidth(int ch, int &a, int &b, int &c);
    virtual int  GetMaxHeight();
    virtual int  GetMaxWidth();
    virtual int  GetAscent();
	virtual void* GetData(size_t * pSizeOut=NULL);
	virtual const char * GetName();

private:

    // Convert a logical size to a point size
    static float LSToPoint(float ls);

    static const int          MAX_NAME = 128;
    char                      m_name[MAX_NAME];

    int                       m_tall;
    int                       m_weight;
    float                     m_pointSize;

    // TLFont variables
    TLFont::FusionRasterizer  *m_pRasterizer;

    // The TTF file
    unsigned char             *m_pData;
    unsigned int              m_dataSize;

    // Required for our Marlett hack
    wchar_t                   m_charOffset;
};

#endif
