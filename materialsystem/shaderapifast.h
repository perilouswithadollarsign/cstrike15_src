//====== Copyright  1996-2008, Valve Corporation, All rights reserved. =====//
#ifndef _SHADERAPIFAST_H_
#define _SHADERAPIFAST_H_

#if defined( _PS3 ) || defined( _OSX )
#include "shaderapidx9/shaderapidx8.h"
#include "shaderapidx9/shaderapidx8_global.h"
#define ShaderApiFast( pShaderAPI ) ShaderAPI()
#else
#define ShaderApiFast( pShaderAPI ) pShaderAPI
#endif

#endif // _SHADERAPIFAST_H_

