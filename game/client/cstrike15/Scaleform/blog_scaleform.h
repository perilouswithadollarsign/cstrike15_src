//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: [thomask] Displays blog in Scaleform.
//
// $NoKeywords: $
//=============================================================================//
#if defined( INCLUDE_SCALEFORM )

#ifndef BLOG_SCALEFORM_H
#define BLOG_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "html_base_scaleform.h"

class CBlogScaleform : public CHtmlBaseScaleform
{
private:
	static CBlogScaleform* m_pInstance;

public:

	/************************************
	 * Construction and Destruction
	 */
	static void LoadDialog( void );
	static void UnloadDialog( void );

	static bool IsActive() { return m_pInstance != NULL; }

	/************************************************************
	 *  Flash Interface methods
	 */
	virtual void FlashReady( void );
};

#endif // BLOG_SCALEFORM_H
#endif // include scaleform