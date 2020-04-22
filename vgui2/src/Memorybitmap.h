//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MEMORYBITMAP_H
#define MEMORYBITMAP_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/vgui.h>
#include <vgui/IImage.h>
#include <color.h>

namespace vgui
{

typedef unsigned long HTexture;

//-----------------------------------------------------------------------------
// Purpose: Holds a single image created from a chunk of memory, internal to vgui only
//-----------------------------------------------------------------------------
class MemoryBitmap: public IImage
{
public:
	MemoryBitmap(unsigned char *texture,int wide, int tall);
	~MemoryBitmap();

	// IImage implementation
	virtual void Paint();
	virtual void GetSize(int &wide, int &tall);
	virtual void GetContentSize(int &wide, int &tall);
	virtual void SetPos(int x, int y);
	virtual void SetSize(int x, int y);
	virtual void SetColor(Color col);

	virtual bool Evict() OVERRIDE { return false; }
	virtual int GetNumFrames() OVERRIDE { return 0; }
	virtual void SetFrame(int nFrame) OVERRIDE {}
	virtual HTexture GetID();		// returns the texture id
	virtual void SetRotation(int iRotation) OVERRIDE { return; }

	// methods
	void ForceUpload(unsigned char *texture,int wide, int tall);	// ensures the bitmap has been uploaded
	const char *GetName();
	bool IsValid()
	{
		return _valid;
	}

private:
	HTexture    _id;
	bool        _uploaded;
	bool		_valid;
	unsigned char		*_texture;
	int			_pos[2];
	Color		_color;
	int			_w,_h; // size of the texture
};

} // namespace vgui

#endif // MEMORYBITMAP_H
