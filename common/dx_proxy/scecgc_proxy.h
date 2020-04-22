//====== Copyright © , Valve Corporation, All rights reserved. =======//
#ifndef SCECGC_PROXY_HDR
#define SCECGC_PROXY_HDR

#include "cg/cgc.h"

class SceCgcProxyModule
{
public:
	SceCgcProxyModule();
	~SceCgcProxyModule();

	/// Loads the module and acquires function pointers, returns if the module was
	/// loaded successfully.
	/// If the module was already loaded the call has no effect and returns TRUE.
	BOOL Load( void );
	/// Frees the loaded module.
	void Free( void );
private:
	// see C:\usr\local\cell\host-win32\Cg\include\cgc.h  for the function signatures
	enum Func {
		sceCgcCapCalcHash_Fletcher32,
		sceCgcCapCalcHash_Fletcher32_Enhanced,
		sceCgcCapCalcHash_MD5,
		sceCgcCapGetShaderHash,
		sceCgcCapGetShaderHash_header,
		compile_program_from_string,
		free_compiled_program,
		sceCgcCompileFile,
		sceCgcCompileString,
		sceCgcDebugTraceBin,
		sceCgcDeleteBin,
		sceCgcDeleteContext,
		sceCgcGetBinData,
		sceCgcGetBinSize,
		sceCgcNewBin,
		sceCgcNewContext,
		sceCgcStoreBinData,
		fnTotal
	};
	HMODULE m_hModule;				//!< The handle of the loaded dx_proxy.dll
	FARPROC m_arrFuncs[fnTotal];	//!< The array of loaded function pointers

	typedef CGCcontext* (*sceCgcNewContext_t)( CGCmem* pool );
	typedef void	    (*sceCgcDeleteContext_t)( CGCcontext* context );

	typedef CGCbin*     (*sceCgcNewBin_t)( CGCmem* pool );
	typedef void*       (*sceCgcGetBinData_t)( CGCbin* bin );
	typedef size_t      (*sceCgcGetBinSize_t)( CGCbin* bin );
	typedef CGCstatus   (*sceCgcStoreBinData_t)( CGCbin* bin, void* data, size_t size );
	typedef void	    (*sceCgcDeleteBin_t)( CGCbin* bin );

	typedef CGCstatus   (*sceCgcCompileString_t)( CGCcontext*   context,
		const char*   sourceString,
		const char*   profile,
		const char*   entry,
		const char**  options,
		CGCbin*       shaderBinary,
		CGCbin*       messages  ,
		CGCbin*       asciiOutput  ,
		CGCinclude*   includeHandler  );
		
	sceCgcNewContext_t m_sceCgcNewContext; 
	sceCgcDeleteContext_t m_sceCgcDeleteContext;
	sceCgcNewBin_t m_sceCgcNewBin;
	sceCgcGetBinData_t m_sceCgcGetBinData;
	sceCgcGetBinSize_t m_sceCgcGetBinSize;
	sceCgcStoreBinData_t m_sceCgcStoreBinData;
	sceCgcDeleteBin_t m_sceCgcDeleteBin;

	sceCgcCompileString_t m_sceCgcCompileString;

	///
	/// Interface functions calling into DirectX proxy
	///
public:

	HRESULT CompileShaderFromFile(
		LPCSTR                          pSrcFile,
		CONST D3DXMACRO*                pDefines,
		LPD3DXINCLUDE                   pInclude,
		LPCSTR                          pFunctionName,
		LPCSTR                          pProfile,
		DWORD                           Flags,
		LPD3DXBUFFER*                   ppShader,
		LPD3DXBUFFER*                   ppErrorMsgs,
		LPD3DXCONSTANTTABLE*            ppConstantTable
	);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
//     IMPLEMENTATION
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


inline SceCgcProxyModule::SceCgcProxyModule( void )
{
	m_hModule = NULL;
	ZeroMemory( m_arrFuncs, sizeof( m_arrFuncs ) );
}

inline SceCgcProxyModule::~SceCgcProxyModule( void )
{
	Free();
}

inline BOOL SceCgcProxyModule::Load( void )
{
	if ( (m_hModule == NULL) &&
		 ( ( m_hModule = ::LoadLibrary( "libcgc.dll" ) ) != NULL
		 //||( m_hModule = ::LoadLibrary( "\\usr\\local\\cell\\host-win32\\Cg\\bin\\libcgc.dll" ) ) != NULL 
		 ||( m_hModule = ::LoadLibrary( "C:\\usr\\local\\cell\\host-win32\\Cg\\bin\\libcgc.dll" ) ) != NULL 
		 )
	   )
	{
		// Requested function names array
		LPCSTR const arrFuncNames[fnTotal] = {
			"sceCgcCapCalcHash_Fletcher32",
			"sceCgcCapCalcHash_Fletcher32_Enhanced",
			"sceCgcCapCalcHash_MD5",
			"sceCgcCapGetShaderHash",
			"sceCgcCapGetShaderHash_header",
			"compile_program_from_string",
			"free_compiled_program",
			"sceCgcCompileFile",
			"sceCgcCompileString",
			"sceCgcDebugTraceBin",
			"sceCgcDeleteBin",
			"sceCgcDeleteContext",
			"sceCgcGetBinData",
			"sceCgcGetBinSize",
			"sceCgcNewBin",
			"sceCgcNewContext",
			"sceCgcStoreBinData"
		};

		// Acquire the functions
		for ( int k = 0; k < fnTotal; ++ k )
		{
			m_arrFuncs[k] = ::GetProcAddress( m_hModule, arrFuncNames[k] );
		}
		
		// regexp: replace   {[(a-z)|(A-Z)]*}_t m_[(a-z)|(A-Z)]*;  with  m_\1 = (\1_t)m_arrFuncs[\1];
		m_sceCgcNewContext = (sceCgcNewContext_t)m_arrFuncs[sceCgcNewContext];
		m_sceCgcDeleteContext = (sceCgcDeleteContext_t) m_arrFuncs[sceCgcNewContext];
		
		m_sceCgcNewBin = (sceCgcNewBin_t)m_arrFuncs[sceCgcNewBin];
		m_sceCgcGetBinData = (sceCgcGetBinData_t)m_arrFuncs[sceCgcGetBinData];
		m_sceCgcGetBinSize = (sceCgcGetBinSize_t)m_arrFuncs[sceCgcGetBinSize];
		m_sceCgcStoreBinData = (sceCgcStoreBinData_t)m_arrFuncs[sceCgcStoreBinData];
		m_sceCgcDeleteBin = (sceCgcDeleteBin_t)m_arrFuncs[sceCgcDeleteBin];

		m_sceCgcCompileString = (sceCgcCompileString_t)m_arrFuncs[sceCgcCompileString];
	}

	return !!m_hModule;
}

inline void SceCgcProxyModule::Free( void )
{
	if ( m_hModule )
	{
		::FreeLibrary( m_hModule );
		m_hModule = NULL;
		ZeroMemory( m_arrFuncs, sizeof( m_arrFuncs ) );
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
//     INTERFACE FUNCTIONS IMPLEMENTATION
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////



inline HRESULT SceCgcProxyModule::CompileShaderFromFile(
	LPCSTR                          pSrcFile,
	CONST D3DXMACRO*                pDefines,
	LPD3DXINCLUDE                   pInclude,
	LPCSTR                          pFunctionName,
	LPCSTR                          pProfile,
	DWORD                           Flags,
	LPD3DXBUFFER*                   ppShader,
	LPD3DXBUFFER*                   ppErrorMsgs,
	LPD3DXCONSTANTTABLE*            ppConstantTable )
{
	if ( !Load() )
		return MAKE_HRESULT( SEVERITY_ERROR, FACILITY_ITF, 1 );
	if ( !m_arrFuncs[sceCgcCompileFile] )
		return MAKE_HRESULT( SEVERITY_ERROR, FACILITY_ITF, 2 );
	// TODO: call m_sceCgcCompileFile
	return S_OK;
}


#endif 
