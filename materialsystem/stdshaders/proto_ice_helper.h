//========= Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ============//

#ifndef PROTO_ICE_HELPER_H
#define PROTO_ICE_HELPER_H
#ifdef _WIN32
#pragma once
#endif

#include <string.h>

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CBaseVSShader;
class IMaterialVar;
class IShaderDynamicAPI;
class IShaderShadow;

//-----------------------------------------------------------------------------
// Init params/ init/ draw methods
//-----------------------------------------------------------------------------
struct ProtoIceVars_t
{
	ProtoIceVars_t() { memset( this, 0xFF, sizeof(ProtoIceVars_t) ); }

	int m_nBaseTexture;
	int m_nBaseTextureFrame;

	int m_nBumpmap;
	int m_nBumpFrame;

	int m_nSsBump;
};

void InitParamsProtoIce( CBaseVSShader *pShader, IMaterialVar** params, const char *pMaterialName, ProtoIceVars_t &info );
void InitProtoIce( CBaseVSShader *pShader, IMaterialVar** params, ProtoIceVars_t &info );
void DrawProtoIce( CBaseVSShader *pShader, IMaterialVar** params, IShaderDynamicAPI *pShaderAPI,
					IShaderShadow* pShaderShadow, ProtoIceVars_t &info, VertexCompressionType_t vertexCompression );

#endif // PROTO_ICE_HELPER_H
