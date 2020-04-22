//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Interface to composite texture objects
//
// $NoKeywords: $
//=============================================================================//

#ifndef I_COMPOSITETEXTURE_H
#define I_COMPOSITETEXTURE_H

class ICompositeTexture
{
public:
	virtual bool IsReady() const = 0;
	virtual bool GenerationComplete() const = 0;
	virtual IVTFTexture *GetResultVTF() = 0;
	virtual	const char *GetName() = 0;
};

#endif // I_COMPOSITETEXTURE_H
