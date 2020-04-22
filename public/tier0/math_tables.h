//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MATH_TABLES_H
#define MATH_TABLES_H

#define SIN_TABLE_SIZE 256

#ifdef BUILD_TABLES

	DLL_EXPORT float SinCosTable[SIN_TABLE_SIZE];
	DLL_EXPORT float g_Mathlib_power2_n[256]; 
	DLL_EXPORT float g_Mathlib_lineartovertex[4096];	
	DLL_EXPORT unsigned char g_Mathlib_lineartolightmap[4096];	

#else

	DLL_IMPORT float SinCosTable[SIN_TABLE_SIZE];
	DLL_IMPORT float g_Mathlib_power2_n[256]; 
	DLL_IMPORT float g_Mathlib_lineartovertex[4096];	
	DLL_IMPORT unsigned char g_Mathlib_lineartolightmap[4096];	

#endif


#endif // MATH_TABLES_H
