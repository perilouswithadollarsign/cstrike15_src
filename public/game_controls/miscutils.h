//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//


#ifndef GAMECONTROLS_MISCUTILS_H
#define GAMECONTROLS_MISCUTILS_H
#ifdef _WIN32
#pragma once
#endif



abstract_class IGameUIMiscUtils
{
public:
	virtual bool PointTriangleHitTest( Vector2D tringleVert0, Vector2D tringleVert1, Vector2D tringleVert2, Vector2D point ) = 0;
};



enum SublayerTypes_t 
{
	SUBLAYER_STATIC,
	SUBLAYER_DYNAMIC,
	SUBLAYER_FONT,
	SUBLAYER_MAX,
};




#endif // GAMECONTROLS_MISCUTILS_H
