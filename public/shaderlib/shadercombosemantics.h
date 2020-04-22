//==== Copyright (c) Valve Corporation, All rights reserved. =======//
//
// Vertex/Pixel Shaders
//
//===========================================================================//

#ifndef SHADERCOMBOSEMANTICS_H
#define SHADERCOMBOSEMANTICS_H

#ifdef _WIN32
#pragma once
#endif

struct ShaderComboInformation_t
{
	const char *m_pComboName;
	int m_nComboMin;
	int m_nComboMax;
};

struct ShaderComboSemantics_t
{
	const char *pShaderName;
	const ShaderComboInformation_t *pDynamicShaderComboArray;
	int nDynamicShaderComboArrayCount;
	const ShaderComboInformation_t *pStaticShaderComboArray;
	int nStaticShaderComboArrayCount;
};

#endif // SHADERCOMBOSEMANTICS_H
