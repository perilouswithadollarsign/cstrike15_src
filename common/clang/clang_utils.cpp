//========= Copyright 1996-2016, Valve Corporation, All rights reserved. ============//
//
// Purpose: Clang utility code
//
//=====================================================================================//

#include "clang/clang_utils.h"

#include "tier1/strtools.h"
#include "tier1/utlstring.h"
#include "tier1/utlvector.h"
#include "environment_utils.h"

// Include the main Clang interface header:
#include "clang-c/Index.h"


CUtlString & TransformArgumentForCommandLineUsage( CUtlString &argument, bool bEscapeContainedQuotes )
{
	bool bHasSpace = V_containsWhitespace( argument.Get() );

	if ( V_strstr( argument.Get(), "\"" ) )
	{
		if ( bEscapeContainedQuotes )
		{
			// Transform " to \"
			char temp[ MAX_PATH ];
			bool bResult = V_StrSubst( argument.Get(), "\"", "\\\"", temp, ARRAYSIZE( temp ), true );
			NOTE_UNUSED( bResult );
			Assert( bResult );
			argument = temp;
		}

		// TODO: support arguments containing both quotes and spaces, e.g:  -Dmystring="hi there"
		Assert( !bHasSpace );
	}
	if ( bHasSpace )
	{
		// Wrap the argument with quotes
		argument = CUtlString( "\"" ) + argument + "\"";
	}

	// Append a space to separate arguments
	argument += " ";

	return argument;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool Clang_IsHeaderFile( const char *pFilename )
{
	// These extensions are used for header files in VPCs in the Source2 tree:
	// TODO: move this util into VPC common code... optimize it
	if ( V_striEndsWith( pFilename, ".h"   ) ) return true;
	if ( V_striEndsWith( pFilename, ".inl" ) ) return true;
	return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool Clang_IsSourceFile( const char *pFilename )
{
	// These extensions are used for source files in VPCs in the Source2 tree:
	// TODO: move this util into VPC common code... optimize it
	if ( V_striEndsWith( pFilename, ".cpp" ) ) return true;
	if ( V_striEndsWith( pFilename, ".cc"  ) ) return true;
	if ( V_striEndsWith( pFilename, ".c"   ) ) return true; 
	return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool Clang_AddCompilerOptions( CUtlVector< CUtlString > &arguments, const char *pCompiler )
{
	if ( !V_stricmp_fast( pCompiler, "VS2005" ) || !V_stricmp_fast( pCompiler, "VS2010" ) )
	{
		// Add Microsoft extensions/compatibility
		arguments.AddToTail( "-fms-extensions" );
		arguments.AddToTail( "-fms-compatibility" );
		arguments.AddToTail( "-mms-bitfields" );

		// Omit '_MSC_VER' (we'll get this in VPC's list of defines)
		arguments.AddToTail( "-fmsc-version=0" );

		// Define the appropriate C/C++ standard
		if ( !V_stricmp_fast( pCompiler, "VS2005" ) )
		{
			arguments.AddToTail( "-std=c++98" );
			return true;
		}
		if ( !V_stricmp_fast( pCompiler, "VS2010" ) )
		{
// [sergiy 2012/10/09] this used to crash for src/particles/builtin_constraints.cpp (get the latest Clang drop and/or submit a minimal repro)
//                     I recompiled clang and it doesn't seem to crash, so I'm swithing it to C++11 to get Rubikon ( which uses c++11 features) to compile. But we can switch back any time
			arguments.AddToTail( "-std=c++11" );
			//arguments.AddToTail( "-std=c++98" );
			arguments.AddToTail( "-fmsc-version=1600" );
			return true;
		}
	}

	// NOTE: clang defaults to the 'gnu99' C/C++ standard
	//       we may want to use '-std=gnu++11' when we add mac+linux support
	arguments.AddToTail( "-std=gnu99" );

	AssertMsg1( false, "ERROR: Clang_GenerateOptions not implemented for this compiler yet! (%s)\n", pCompiler );
	return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool Clang_AddPlatformOptions( CUtlVector< CUtlString > &arguments, const char *pPlatform )
{
	// See LLVM\include\llvm\ADT\Triple.h and LLVM\tools\clang\lib\Basic\Targets.cpp
	// for information about supported targets... the target 'triple' is of the form:
	//
	//	  architecture-vendor-os[-environment]
	//
	// The fourth part is optional. Some of the supported values:
	//
	//	  [x86|x86_64|ppc|ppc64] - [pc|apple|scei|unknown] - [win32|mingw32|cygwin|macosx|lv2|linux] - [gnu|ANDROIDEABI]

	if ( !V_stricmp_fast( pPlatform, "WIN32" ) )
	{
		// TODO: might get better support with 'mingw32' or 'cygwin' than 'win32' (wtf are these things??)
		arguments.AddToTail( "-target" );
		arguments.AddToTail( "i686-pc-win32" );
		return true;
	}
	if ( !V_stricmp_fast( pPlatform, "WIN64" ) )
	{
		// Need to tell Clang if this is a 64-bit CPU (or it will complain about usage of size_t in operator new)
		// [ NOTE: the option '-m64' will internally transform i686-pc-win32 into x86_64-pc-win32 ]
		arguments.AddToTail( "-target" );
		arguments.AddToTail( "x86_64-pc-win32" );
		// Define away unsupported intrinsics (breaks Rubikon due to __debugbreak being declared in system header intrin.h). We can put it back any time
		//arguments.AddToTail( "-D__debugbreak(...)=__asm { int 3 }" );
		return true;
	}
	if ( !V_stricmp_fast( pPlatform, "OSX" ) )
	{
		// TODO: for OSX/iOS, the os string should include a version number, e.g 'macosx10.8.0'
		AssertMsg( 0, "Untested!\n" );
		arguments.AddToTail( "-target" );
		arguments.AddToTail( "ppc-apple-macosx" );
		return true;
	}
	if ( !V_stricmp_fast( pPlatform, "X360" ) )
	{
		// TODO: figure out which target triple to use... we want to pretend we're using PPC for Win32 (Clang doesn't know about XBox)
		AssertMsg( 0, "Untested!\n" );
		arguments.AddToTail( "-target" );
		arguments.AddToTail( "ppc-pc-win32" );
		return true;
	}
	if ( !V_stricmp_fast( pPlatform, "PS3" ) )
	{
		// TODO: PS3 uses 'lv2' for OS and 'scei' for the vendor, but for some reason 'ppc64-scei-lv2' doesn't work.
		// NOTE: SPU compilation is also supported, via 'cellspu-scei-lv2'
		AssertMsg( 0, "Untested!\n" );
		arguments.AddToTail( "-target" );
		arguments.AddToTail( "ppc64-scei-lv2" );
		return true;
	}

	AssertMsg1( false, "ERROR: Clang_GenerateOptions not implemented for this platform yet! (%s)\n", pPlatform );
	return false;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool Clang_AddSystemIncludePaths( CUtlVector< CUtlString > &arguments, const char *pPlatform, const char *pCompiler )
{
	CUtlVector< CUtlString > systemPaths;
	if ( !GetSystemIncludePaths( systemPaths, pPlatform, pCompiler ) )
	{
		AssertMsg2( false, "ERROR: Clang_GenerateOptions not implemented for this platform/compiler yet! (%s/%s)\n", pPlatform, pCompiler );
		return false;
	}

	// System include paths (added with '-isystem') will be searched *after* all the regular '-I' includes
	// [http://gcc.gnu.org/onlinedocs/cpp/System-Headers.html]
	CUtlString includePrefix( "-isystem" );
	for ( int i=0; i < systemPaths.Count(); i++ )
	{
		arguments.AddToTail( includePrefix + systemPaths[i] );
	}
	return true;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool Clang_GenerateCommandString(	CUtlString &command, bool bForLibClangDLL, const char *pFilename, const char *pPlatform, const char *pCompiler,
									const CUtlVector< CUtlString > &defines, const CUtlVector< CUtlString > &includes,
									const CUtlString &pchName, bool bBuildPCH, int parseFlags )
{
	CUtlVector< CUtlString > arguments;
	arguments.EnsureCapacity( 512 ); // Avoid needless string copying
	command.Clear();

	// Detect file type (source or header)
	bool bIsHeader = Clang_IsHeaderFile( pFilename );
	if ( !bIsHeader && !Clang_IsSourceFile( pFilename ) )
	{
		AssertMsg2( Clang_IsHeaderFile( pFilename ) || Clang_IsSourceFile( pFilename ), "ERROR (%s): unrecognised file type '%s'!\n", __FUNCTION__, pFilename );
		return false;
	}
	if ( bIsHeader )
	{
		// By default, Clang interprets '.h' files as C, not C++:
// TODO: should we just always specify "c++"? (i.e should we parse ".c" files as C++? and is "c++-header" actually better for headers?)
		arguments.AddToTail( "-x" );							//			"Treat subsequent input files as having type <language>"
		arguments.AddToTail( "c++-header" );
	}
	if ( !bBuildPCH )
	{
		// TODO: -fsyntax-only yields invalid PCHs, but building without it causes clang to crash on other files (e.g src/client/c_baseanimating.cpp)
		//       need to drill into this (build clang in debug, make a minimal repro), so we can submit bug reports
		arguments.AddToTail( "-fsyntax-only" );					//			"Check the code for syntax errors, but don't do anything beyond that"
	}

	// Add standard options
	arguments.AddToTail( "-c" );								//[ignored]	"Compile or assemble the source files, but do not link."
	arguments.AddToTail( "-nostdinc" );							//			"Disable standard system #include directories"
																// NOTE: -nostdinc works and overrides -nobuiltininc, while
																//       -nostdsysteminc and nostdinc++ do not work (had to GREP the LLVM source code to determine this !)

	// These help to diagnose parse errors:
	arguments.AddToTail( "-ferror-limit=100" );					//			"Max errors to allow before parsing terminates"
	arguments.AddToTail( "-fmacro-backtrace-limit=0" );			//			"Max macro expansion depth in diagnostic spew"
	//arguments.AddToTail( "-v" );								//			"Show commands to run and use verbose output"

	// Add options corresponding to the passed parse flags:
	if ( parseFlags & CXTranslationUnit_DetailedPreprocessingRecord )
	{
		arguments.AddToTail( "-Xclang" );						//			"Pass <arg> to the clang compiler"
		arguments.AddToTail( "-detailed-preprocessing-record" );//			"Include pre-processor data in the AST"
	}

	// Suppress warnings we don't want to see:
// TODO: can these be removed now I've added the VS2010 options?
	arguments.AddToTail( "-Wno-microsoft" );					//			"XXX is a microsoft extension"
	arguments.AddToTail( "-Wno-c++11-extensions" );				//			"warnings about using C++11 features as extensions in earlier C++ versions"
	// TODO: these suppress warnings we should actually fix later:
	arguments.AddToTail( "-Wno-return-type-c-linkage" );		//			"XXX has C-linkage specified, but returns user-defined type YYY which is incompatible with C"
	arguments.AddToTail( "-Wno-invalid-token-paste" );			//			"pasting formed 'XXX', an invalid preprocessing token"

	// __FUNCTION__ is implemented by Clang in the GCC style; it's a compiler-generated variable, not
	// a preprocessor-generated string literal, so you can't do things like:  "Error in " __FUNCTION__ "!\n"
	// [ http://stackoverflow.com/questions/11142779/function-and-friends-act-weird-in-xcode ]
	arguments.AddToTail( "-D__FUNCTION__=__FILE__" );

	if ( bIsHeader && pchName.IsEmpty() )
	{
		// Clang cannot parse many of our standalone .h files, since they don't include everything they
		// need (e.g baseschema.h), so if we're not using a PCH then define away schema stuff:
// TODO: remove this once we port Schemacompiler to use Clang!
		arguments.AddToTail( "-Dschema=" );
		arguments.AddToTail( "-Dnoschema=" );
		arguments.AddToTail( "-Dschema_pragma(...)=" );
		arguments.AddToTail( "-DMETA(...)=" );
		arguments.AddToTail( "-DTYPEMETA(...)=" );
		arguments.AddToTail( "-DSCHEMA_IGNORE_NEXT_PARENS(...)=" );
		arguments.AddToTail( "-DMETA_ADD(...)=SCHEMA_IGNORE_NEXT_PARENS" );
		arguments.AddToTail( "-DMETA_REMOVE(...)=SCHEMA_IGNORE_NEXT_PARENS" );
		arguments.AddToTail( "-DMETA_OVERRIDE(...)=SCHEMA_IGNORE_NEXT_PARENS" );
		arguments.AddToTail( "-DMETA_REMOVE_ALL(...)=SCHEMA_IGNORE_NEXT_PARENS" );
	}

	// __FUNCTION__ is implemented by Clang in the GCC style; it's a compiler-generated variable, not
	// a preprocessor-generated string literal, so you can't do things like:  "Error in " __FUNCTION__ "!\n"
	// [ http://stackoverflow.com/questions/11142779/function-and-friends-act-weird-in-xcode ]
	arguments.AddToTail( "-D__FUNCTION__=__FILE__" );


	// Add platform-specific options:
	if ( !Clang_AddPlatformOptions( arguments, pPlatform ) )
		return false;

	// Add compiler-specific options:
	if ( !Clang_AddCompilerOptions( arguments, pCompiler ) )
		return false;

	// Add system include dirs explicitly (options above disable Clang's built-in include paths)
	if ( !Clang_AddSystemIncludePaths( arguments, pPlatform, pCompiler ) )
		return false;


	// Add the caller's preprocessor defines and include directories:
	CUtlString definePrefix( "-D" ), includePrefix( "-I" );
	for ( int i=0; i < defines.Count();  i++ ) arguments.AddToTail( definePrefix  + defines[i]  );
	for ( int i=0; i < includes.Count(); i++ ) arguments.AddToTail( includePrefix + includes[i] );

	if ( bBuildPCH )
	{
		// This file is used as a PCH, so output the file <pchName>.pch:
		arguments.AddToTail( "-emit-ast" );
		arguments.AddToTail( "-o" );
		arguments.AddToTail( pchName + ".pch" );
	}
	else if ( !pchName.IsEmpty() )
	{
		// If the file uses a PCH, add  "-include <pchName>"
		// NOTE: this is safely ignored for files which do not actually include the PCH file (e.g headers)
		arguments.AddToTail( "-include" );
		arguments.AddToTail( pchName );
	}

	// Put the filename last, after all the options:
	arguments.AddToTail( pFilename );

	// Transform each argument as appropriate for its intended usage
	// (either passing via the libclang.dll API, or passing via the command-line):
	for ( int i = 0; i < arguments.Count(); i++ )
	{
		command += TransformArgumentForCommandLineUsage( arguments[i], !bForLibClangDLL );
	}

	return true;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool Clang_GenerateDLLArguments(	ICommandLine *pArguments, const char *pFilename, const char *pPlatform, const char *pCompiler,
									const CUtlVector< CUtlString > &defines, const CUtlVector< CUtlString > &includes,
									const CUtlString &pchName, bool bBuildPCH, int parseFlags )
{
	CUtlString command;
	bool bResult = Clang_GenerateCommandString( command, true, pFilename, pPlatform, pCompiler, defines, includes, pchName, bBuildPCH, parseFlags );
	pArguments->CreateCmdLine( command.Get() );
	return bResult;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
bool Clang_GenerateCommandLine(		CUtlString &command, const char *pFilename, const char *pPlatform, const char *pCompiler,
									const CUtlVector< CUtlString > &defines, const CUtlVector< CUtlString > &includes,
									const CUtlString &pchName, bool bBuildPCH, int parseFlags )
{
	return Clang_GenerateCommandString( command, false, pFilename, pPlatform, pCompiler, defines, includes, pchName, bBuildPCH, parseFlags );
}
