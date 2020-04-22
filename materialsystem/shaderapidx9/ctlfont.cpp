//  Copyright Electonic Arts(C) 2006 - All Rights Reserved

#include "filesystem.h"

#include "CTLFont.h"

#include "t2k.h"
#include "tlfont/fusionrasterizer.h"
#include <FontAux/AllocatorAdapters.h>
#include "MemMgr/inc/MemMgr.h"

// CTLFontManager and CTLFont act as a wrapper for Font Fusion. It uses the TLFont 
// wrapper for rasterizing with Font Fusion. We ignore all the other systems provided
// by TLFont (caching, drawlists, rendering, etc).

// MARLETT:
// Marlett causes some problems with Font Fusion. It renders the glyphs correctly,
// but can return incorrect metric information. In particular, the descent is always
// 0 and ascent is the height of the glyph; the maximum height calculated from
// the ascent and descent does not always indicate the highest glyph - it is possible
// to render a glyph heigher than (ascent+descent).
//
// For some reason, you need to offset any marlett character codes by 0xf000 otherwise
// Font Fusion will just render invalid glyphs.

using namespace TLFont;

//#define DISABLE_FONT

CTLFontManager::CTLFontManager(IFileSystem *pFileSystem)
{
#ifndef DISABLE_FONT
    // The allocator that uses MemMgr
    static MemMgr_ICoreAllocator_Adapter memMgrAdapter;
    SetAllocator(&memMgrAdapter); 
    
    FontFusionMemObject::SetAllocatorCallbacks(FontFusionAlloc, FontFusionFree, 0);	

    // Load the filesystem
    m_pFileSystem = pFileSystem;
    ASSERT(m_pFileSystem);

	// Reset the font data cache
	for (int i = 0; i < MAX_FONTDATACACHE; ++i)
	{
		memset(m_fontDataCache[i].m_dataName, 0, 256);
		m_fontDataCache[i].m_dataMem = NULL;
		m_fontDataCache[i].m_dataSize = 0;
		m_fontDataCache[i].m_refCount = 0;
	}
#endif
}

CTLFontManager::~CTLFontManager()
{
	// Clean the cache
	for (int i = 0; i < MAX_FONTDATACACHE; ++i)
	{
		if ( m_fontDataCache[i].m_dataMem )
		{
			//delete m_fontDataCache[i].m_dataMem;//Now allocated permanently
			m_fontDataCache[i].m_dataMem = NULL;
			m_fontDataCache[i].m_refCount = 0;
		}
	}
}

unsigned char* CTLFontManager::LoadFontFile(const char *pFontPath, unsigned int *pDataSize)
{
#if 1
		Assert(!"<Sergiy> - temporarily disabling this");
		return NULL;
#else
#ifndef DISABLE_FONT
    unsigned char *pData = NULL;
	unsigned int dataSize = 0;

	MEM_ALLOC_CREDIT_("CTLFontManager::LoadFontFile");

    // Load a new font file
	FileHandle_t hFont = m_pFileSystem->Open(pFontPath, "rb");

    if(hFont == NULL)
    {
        ASSERT(hFont);
        return NULL;
    }

    dataSize = m_pFileSystem->Size(hFont);
    if(dataSize == 0)
    {
        // error getting the file size
        ASSERT(dataSize > 0);
        return NULL;
    }

    //pData = new unsigned char[dataSize];
	pData= ( unsigned char * )gMemMgr.PermanentAlloc(dataSize);// This is an alloc thats never freed (gives us some flexibility to reuse scraps of memory)
    ASSERT(pData);
//printf("LoadFontFile(%s) allocating %d",pFontPath,dataSize);

    int ret = m_pFileSystem->Read(pData, dataSize, hFont);

    if(pDataSize)
    {
        *pDataSize = dataSize;
    }

    m_pFileSystem->Close(hFont);

    return pData;
#else
	return NULL;
#endif
#endif
}

CTLFont *CTLFontManager::CreateFont(const char *pName, const char *pFontPath, int tall, int weight)
{
#ifndef DISABLE_FONT
	unsigned int dataSize = 0;
	unsigned char *pData = NULL;

	// Check the cache if we have already loaded this font
	for (int i = 0; i < MAX_FONTDATACACHE; ++i)
	{
		if ( m_fontDataCache[i].m_dataMem )
		{
			if ( strncmp(pName, m_fontDataCache[i].m_dataName, strlen(pName)) == 0 )
			{
//printf("CTLFontManager::CreateFont(%s) - found in cache\n",pName);
				pData = m_fontDataCache[i].m_dataMem;
				dataSize = m_fontDataCache[i].m_dataSize;
				m_fontDataCache[i].m_refCount++;
				break;
			}
		}
		else
		{
			// Load the TTF font from disk
//printf("CTLFontManager::CreateFont(%s) - loading from disk\n",pName);
			m_fontDataCache[i].m_dataMem = LoadFontFile(pFontPath, &m_fontDataCache[i].m_dataSize);
			pData = m_fontDataCache[i].m_dataMem;
			dataSize = m_fontDataCache[i].m_dataSize;
			Q_strncpy(m_fontDataCache[i].m_dataName, pName, strlen(pName)+1);
			m_fontDataCache[i].m_refCount=1;
			break;
		}
	}

    if(pData == NULL || dataSize == 0)
    {
        Assert(0);
		return NULL;
    }

    // Simply allocate a new font and return it. One could, potentially, have a more complex way
    // of handling the fonts memory management.
    CTLFont *pFont = new CTLFont(pName, pData, dataSize, tall, weight);
    ASSERT(pFont);
    
    return pFont;
#else
	return NULL;
#endif
}

void CTLFontManager::DestroyFont(CTLFont *pFont)
{
#ifndef DISABLE_FONT
	size_t size;
	void* pData;
	pData=pFont->GetData(&size);
//printf("CTLFontManager::DestroyFont(%s)\n",pFont->GetName());

	//Delete CTLFont
    delete pFont;

	//Update refcount in m_fontDataCache
	for (int i = 0; i < MAX_FONTDATACACHE; ++i)
	{
		if (pData==m_fontDataCache[i].m_dataMem)
		{
			m_fontDataCache[i].m_refCount--;
			break;
		}
	}	
	
#endif
}

CTLFont::CTLFont(const char *pName, unsigned char *pData, unsigned int dataSize, int tall, int weight)
{
    // Font name
    int len = strlen(pName);
    if(len >= MAX_NAME)
    {
        memcpy(m_name, pName, (MAX_NAME-2));
        m_name[MAX_NAME-1] = 0;
    }
    else
    {
        // include the null terminator
        memcpy(m_name, pName, len+1);
    }

    // Dimensions
    //note: Point and Logical Size values should be the same, unless you start scaling things.
    m_tall = tall;
    m_weight = weight;
	// empirically derived factor to achieve desired cell height
    m_pointSize = m_tall * 0.82f;

    // Load the TTF file into memory
    m_pData = pData;
    m_dataSize = dataSize;
    ASSERT(m_pData);
    ASSERT(m_dataSize > 0);

    // The rasterizer
#ifndef DISABLE_FONT
    m_pRasterizer = new FusionRasterizer(m_pData,
                                         m_dataSize,    
                                         FONTFILE_TTF,
                                         NULL,
                                         0.0f,
                                         1.0f,
                                         72, 72,
                                         0,     // padding must be zero
                                         m_weight);
    ASSERT(m_pRasterizer);
#endif

    // Gross marlett hack!
    m_charOffset = 0;
    if(stricmp(m_name, "Marlett") == 0)
    {
        m_charOffset = 0xf000;
    }
}

CTLFont::~CTLFont()
{
    delete m_pRasterizer;
}

void CTLFont::RenderToBuffer(int ch, int offsetx, int width, int height, unsigned char *pBuffer)
{
#ifndef DISABLE_FONT
    IRasterizer::RasterizationResult res;
    FixedAngle angle(0.0f);
    GlyphImage *pImage = m_pRasterizer->Rasterize(ch+m_charOffset, 1.0f, m_pointSize, angle, res);

    // Return if we try and render a character we don't understand. This can include white spaces
    // and other special control characters.
    if(pImage == NULL || 
       pImage->GetFormat() == GlyphImage::GLYPHIMAGE_INVALID ||
       res == IRasterizer::RASTERIZE_FAILURE)
    {
        memset(pBuffer, 0x00, (width*height*4));
        return;
    }

    // Image dimensions
    unsigned int imageWidth, imageHeight;
    pImage->GetGlyphDimensions(imageWidth, imageHeight);

    // Marlett can be slightly bigger than the maximum height calculated by adding
    // the ascent and descent values together.
    if(imageHeight > height)
    {
        imageHeight = height;
    }

    // The offset from the baseline..
    int xOffset, yOffset;
    pImage->GetGlyphOffset(xOffset, yOffset);

    // Determine the baseline of the image (using the ascent and descent values)
    int ascent = (int)ceil(m_pRasterizer->GetAscent(m_pointSize));
    int descent = (int)ceil(m_pRasterizer->GetDescent(m_pointSize));
    int maxHeight = (descent + ascent);

    int baseOffset = maxHeight - (descent + yOffset);
    if(baseOffset < 0)
    {
        // Marlett can produce a negative offset, which is BAD, so we correct this.
        baseOffset = 0;
    }

    ASSERT((imageHeight+baseOffset) <= height);

    // We only support copying of an alpha-only image
    ASSERT(pImage->GetFormat() == GlyphImage::GLYPHIMAGE_A8);

	if(pImage->GetFormat() == GlyphImage::GLYPHIMAGE_A8)
	{
		// The rasterized image is stored as single bytes; we need to copy
		// this into a 32-bit image.
		for(int h = 0; h < imageHeight; h++)
		{
			unsigned char *pSrc = (unsigned char *)pImage->GetBitmap() + h*imageWidth;
			unsigned int *pDst = (unsigned int *)pBuffer + ( (baseOffset+h) * width );

			for(int w = 0; w < imageWidth; w++)
			{
				unsigned char val = pSrc[w];

				pDst[w + offsetx] = (0xff << 24) | (0xff << 16) | (0xff << 8) | val;
			}
		}
	}

    delete pImage;
#endif
}

// This is a slow function. You should use it with care; possibly implement a basic
// caching system to prevent it being called all the time.
bool CTLFont::GetCharABCWidth(int ch, int &a, int &b, int &c)
{
#ifndef DISABLE_FONT
    IRasterizer::RasterizationResult res;
    FixedAngle angle(0.0f);
    GlyphImage *pImage = m_pRasterizer->Rasterize(ch+m_charOffset, 1.0f, m_pointSize, angle, res);

    // Return if we try and render a character we don't understand. This can include white spaces
    // and other special control characters.
	if(pImage == NULL || 
       res == IRasterizer::RASTERIZE_FAILURE)
    {
        a = 0;
        b = 0;
        c = 0;
        return false;
    }

    unsigned int width, height;
    pImage->GetGlyphDimensions(width, height);

    int offsetX, offsetY;
    pImage->GetGlyphOffset(offsetX, offsetY);

    int advance = pImage->GetGlyphAdvance();

    ASSERT((int)width >= 0);
    
    // We simply provide the advance distance as the glyph width, because 'a' and 'c'
    // have some special meaning to the Source engine..
    b = width;
    a = offsetX;
    c = advance - ((int)width + offsetX);
    
    // In case the advance value is smaller than the glyph width
    if(c < 0)
    {
        c = 0;
    }

    delete pImage;

    return true;
#else
	return false;
#endif
}

int CTLFont::GetMaxHeight()
{
#ifndef DISABLE_FONT
    float ascent = m_pRasterizer->GetAscent(m_pointSize);
    float descent = m_pRasterizer->GetDescent(m_pointSize);

    return (int)(ceil(ascent) + ceil(descent));
#else
	return 0;
#endif
}

int CTLFont::GetMaxWidth()
{
    // Unimplemented
    //ASSERT(0);

    return 0;
}

int CTLFont::GetAscent()
{
#ifndef DISABLE_FONT
	float ascent = m_pRasterizer->GetAscent(m_pointSize);

	return (int)ceil(ascent);
#else
	return 0;
#endif
}

void* CTLFont::GetData(size_t * pSizeOut)
{
	if (pSizeOut) *pSizeOut=m_dataSize;
	return m_pData;
}

const char * CTLFont::GetName()
{
	return m_name;
}