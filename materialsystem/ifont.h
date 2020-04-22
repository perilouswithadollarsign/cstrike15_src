//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IFONT_H
#define IFONT_H

#include "platform.h"

// font interface
class IFont
{
public:

    virtual void RenderToBuffer(int ch, int offsetx, int width, int height, unsigned char *pBuffer) = 0;
    virtual bool GetCharABCWidth(int ch, int &a, int &b, int &c) = 0;
    virtual int  GetMaxHeight() = 0;
    virtual int  GetMaxWidth() = 0;
    virtual int  GetAscent() = 0;
};

#endif
