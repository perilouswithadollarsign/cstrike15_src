//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Clang utility code
//
//=====================================================================================//

#ifndef CLANG_UTILS_H
#define CLANG_UTILS_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"

class CUtlString;


// Generate the appropriate Clang command for the given file/platform/compiler/defines/include-paths/flags
//  - 'DLLArguments' fills in an ICommandLine which can be used with libclang.dll (clang_parseTranslationUnit, etc)
//    'CommandLine' generates a string which can be used as input to clang.exe on the command-line.
//  - 'pchName' specifies the name of the PCH file to include/create (empty means no PCH),
//    'bBuildPCH' specifies that this file builds a .pch file (Clang_GenerateCommandLine will generate the output file <pchName>.pch),
//  - Supported values for pPlatform: "WIN32", "WIN64", "X360"
//    Supported values for pCompiler: "VS2005", "VS2010"
extern bool Clang_GenerateDLLArguments(	ICommandLine *pArguments, const char *pFilename, const char *pPlatform, const char *pCompiler,
										const CUtlVector< CUtlString > &defines, const CUtlVector< CUtlString > &includePaths,
										const CUtlString &pchName, bool bBuildPCH = false, int parseFlags = 0 );
extern bool Clang_GenerateCommandLine(	CUtlString &command, const char *pFilename, const char *pPlatform, const char *pCompiler,
										const CUtlVector< CUtlString > &defines, const CUtlVector< CUtlString > &includePaths,
										const CUtlString &pchName, bool bBuildPCH = false, int parseFlags = 0 );

// Does this have a recognized source file extension? (.cpp, .c)
extern bool Clang_IsSourceFile( const char *pFilename );

// Does this have a recognized header file extension? (.h, .inl)
extern bool Clang_IsHeaderFile( const char *pFilename );

#endif // CLANG_UTILS_H
