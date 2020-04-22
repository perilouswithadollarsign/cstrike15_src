//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef VGUIMATERIALS_H
#define VGUIMATERIALS_H
#ifdef _WIN32
#pragma once
#endif


class ITGARenderer
{
public:
	virtual void Init( int screenWidth, int screenHeight, int numTiles ) = 0;
	virtual void Render() = 0;
	virtual void Shutdown() = 0;
};

extern ITGARenderer *g_pTGARenderer;

#endif // VGUIMATERIALS_H