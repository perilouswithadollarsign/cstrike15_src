//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Proxy for D3DX routines
//
// $NoKeywords: $
//
//=============================================================================//
//
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>

#include <vector>
#include <string>
#include <algorithm>

#include "../../dx10sdk/include/d3dx10.h"

typedef D3D10_SHADER_MACRO D3DXMACRO;
typedef LPD3D10INCLUDE LPD3DXINCLUDE;
typedef ID3D10Include ID3DXInclude;
typedef D3D10_INCLUDE_TYPE D3DXINCLUDE_TYPE;
typedef ID3D10Blob* LPD3DXBUFFER;
typedef void* LPD3DXCONSTANTTABLE;

#include "filememcache.h"
#include "dxincludeimpl.h"

#include "cgc.h"
#include "SCEShaderPerf.h"

typedef unsigned int uint;
typedef unsigned __int64 uint64;
#include "../../public/ps3shaderoptimizer/ps3optimalschedulesfmt.h"

const int g_nRandSched[] =
{
	// List of 17 good scheduler settings, found empirically by Sony.
	8, 10, 15, 4,
	32, 2, 1, 64,
	13, 14, 16, 17, 18, 19,
	128, 256, 512,

	// Extra 6 scheduler settings
	6, 100, 192, 3, 384, 24
};

#define NUM_RANDOM_SCHEDULE_VALUES	ARRAYSIZE( g_nRandSched )
#define NUM_RANDOM_SCHEDULE_SEEDS	12

// Faster settings, for testing purposes (currently takes around 11 minutes):
//#define NUM_RANDOM_SCHEDULE_VALUES		8
//#define NUM_RANDOM_SCHEDULE_SEEDS		1

//#define NUM_RANDOM_SCHEDULE_VALUES	1
//#define NUM_RANDOM_SCHEDULE_SEEDS		1

#define CGC_COMPILER_OPTIMIZATION_LEVEL 1

// Aux function prototype
const char * WINAPI GetDllVersion( void );
void* CgMalloc( void* arg, size_t size );  // Memory allocation callback
void CgFree( void* arg, void* ptr );    // Memory freeing callback

HANDLE g_mutexDebug = NULL;

void DebugLog( const char * pMsg, ...) 
{
	(void)pMsg;
#ifdef _DEBUG
	FILE * f = fopen( "c:\\dx_proxy_ps3.log", "at" );
	if( f )
	{
		if( g_mutexDebug )
			WaitForSingleObject( g_mutexDebug, INFINITE );

		va_list args;
		va_start(args,pMsg);

		SYSTEMTIME lt;
		GetLocalTime( &lt );
		fprintf( f, "%02d:%02d:%02d.%04d[%d.%d]", lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds,
			GetCurrentProcessId(), GetCurrentThreadId() );
		vfprintf(f, pMsg, args );
		fputs( "\n", f );
		va_end( args );

		fclose( f );

		if( g_mutexDebug )
			ReleaseMutex( g_mutexDebug );
	}
#endif
}

//
// ExtractDependencies
//
// Retrieves all the additional required binaries from the resources and
// places them to a temporary location. Then the binaries are mapped into
// the address space of the calling process.
//
static BOOL ExtractDependencies( void )
{
	return TRUE;
}

class CgContextWrapper
{
public:
	CGCcontext *m_cgc;
	CgContextWrapper()
	{
		CGCmem mem;
		mem.malloc = CgMalloc;
		mem.free = CgFree;
		m_cgc = sceCgcNewContext( &mem );
	}
	~CgContextWrapper()
	{
		sceCgcDeleteContext( m_cgc );
	}
	operator CGCcontext * () { return m_cgc ; }
};




// DLL entry point: DllMain
BOOL WINAPI DllMain(
					HINSTANCE hinstDLL,
					DWORD fdwReason,
					LPVOID lpvReserved
					)
{
	/*UNUSED_ALWAYS*/( hinstDLL );
	/*UNUSED_ALWAYS*/( lpvReserved );

	switch ( fdwReason )
	{
	case DLL_PROCESS_ATTACH:
		{
			g_mutexDebug  = CreateMutex( NULL, FALSE, "DxProxyPs3DebugLog" );
		}
		// Process is attaching - make sure it can find the dependencies
		return ExtractDependencies();
	case DLL_PROCESS_DETACH:
		if( g_mutexDebug )
			CloseHandle( g_mutexDebug );
		break;
	}

	return TRUE;
}


// Obtain DLL version
#pragma comment(linker, "/EXPORT:GetDllVersionLong=?GetDllVersionLong@@YGPBDXZ")
const char * WINAPI GetDllVersionLong( void )
{
#if defined( _DEBUG )
	return "{DX_PROXY for PS3_V00_PC DEBUG}";
#else
	return "{DX_PROXY for PS3_V00_PC RELEASE}";
#endif
}


#pragma comment(linker, "/EXPORT:GetDllVersion=?GetDllVersion@@YGPBDXZ")
const char * WINAPI GetDllVersion( void )
{
#ifdef _DEBUG
	return "DXPRX_PS3_V00_d";
#else
	return "DXPRX_PS3_V00_r";
#endif
}

LPD3DXINCLUDE                   g_pInclude = NULL;

uint g_nCgAllocated = 0;


int CgcIncludeOpen( SCECGC_INCLUDE_TYPE type,
			const char* filename,
			char** data, size_t* size )
{
	D3DXINCLUDE_TYPE typeD3d = D3D10_INCLUDE_LOCAL;
	if( type == SCECGC_SYSTEM_INCLUDE )
		typeD3d = D3D10_INCLUDE_SYSTEM;
	
	HRESULT hr = g_pInclude->Open( typeD3d, filename, NULL, (LPCVOID*)data, size );
	
	
	return ( S_OK == hr );
}


void* CgMalloc( void* arg, size_t size )  // Memory allocation callback
{
	g_nCgAllocated += size;	
	uint * pData = (uint*)malloc( size + sizeof( uint ) );
	*pData = size;
	//DebugLog("alloc %d->%p", size, pData+1);
	return pData + 1;
}

void CgFree( void* arg, void* ptr )    // Memory freeing callback
{
	uint * pData = ( ( uint* ) ptr ) - 1;
	//if( *pData > 0x1000000 && IsDebuggerPresent() )
	//	_asm{int 3 ;};		
	
	//DebugLog("free %p->%u", ptr, *pData);
	
	g_nCgAllocated -= *pData;
	free( pData );
}

//
// return values:
// 1 - Include file successfully closed.
// 
// 0 - Failure closing an include file. 
//

int CgcIncludeClose( const char* data )
{
	HRESULT hr = g_pInclude->Close( data );
	return ( S_OK == hr );
}


class BlobAdaptor: public ID3D10Blob
{
public:
	uint m_nRefCount;
	CGCbin *m_bin;
	char * m_pMemory;
	uint m_nSize;
	
	BlobAdaptor( ID3D10Blob * pLeft, ID3D10Blob * pRight )
	{
		m_bin = NULL;
		m_nRefCount = 1;
		m_nSize = pLeft->GetBufferSize( ) + pRight->GetBufferSize() ;
		m_pMemory = new char [m_nSize + 1];
		memcpy(m_pMemory, pLeft->GetBufferPointer(), pLeft->GetBufferSize( ));
		memcpy(m_pMemory + pLeft->GetBufferSize(), pRight->GetBufferPointer(), pRight->GetBufferSize( ) );
		m_pMemory[m_nSize] = '\0';
	}

	BlobAdaptor()
	{
		m_pMemory = NULL;
		m_nSize = 0;
		CGCmem mem;
		mem.malloc = CgMalloc;
		mem.free = CgFree;
		m_bin = sceCgcNewBin( &mem );
		m_nRefCount = 1;
	}
	~BlobAdaptor()
	{
		if( m_bin )
			sceCgcDeleteBin( m_bin );
		if( m_pMemory )
			delete[]m_pMemory;
	}
	
	void Bake()
	{
		if( m_bin )
		{
			m_nSize = sceCgcGetBinSize( m_bin );
			m_pMemory = new char [m_nSize + 1];
			memcpy( m_pMemory, sceCgcGetBinData( m_bin ), m_nSize );
			m_pMemory[m_nSize] = '\0';
			
			sceCgcDeleteBin( m_bin );
			m_bin = NULL;
		}
	}

	STDMETHOD(QueryInterface)(THIS_ REFIID iid, __deref_out LPVOID *ppv) 
	{
		if( iid == IID_IUnknown || iid == IID_ID3D10Blob )
		{
			AddRef();
			*ppv = this;
			return S_OK;
		}
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	
	STDMETHOD_( ULONG, AddRef )(THIS) 
	{
		return ++m_nRefCount;
	}
	
	STDMETHOD_( ULONG, Release )(THIS)
	{
		if( --m_nRefCount )
			return m_nRefCount;
		delete this;
		return 0;
	}

	// ID3DXBuffer
	STDMETHOD_(__out LPVOID, GetBufferPointer)(THIS)
	{
		if( m_bin )
			return sceCgcGetBinData( m_bin );
		else
			return m_pMemory;
	}
	
	STDMETHOD_(DWORD, GetBufferSize)(THIS) 
	{
		if( m_bin )
			return sceCgcGetBinSize( m_bin );
		else
			return m_nSize;
	}
};

static inline bool operator< ( const SceSpMeasurementResult& target, const SceSpMeasurementResult& reference )
{
	if ( target.nResult != SCESP_OK )
		return false;

	if ( target.nCycles < reference.nCycles )
		return true;
	else if ( target.nCycles == reference.nCycles )
	{
		if ( target.nRRegisters < reference.nRRegisters )
			return true;
		else
			return false;
	}
	else
		return false;
}

// Use the Win32 crypto API to create a 64-bit GUID. (This sucks, but it avoids creating dependencies against tier0/tier1 into a DLL that is not expected to have such dependencies.)
static uint64 CreateGUID64()
{
	uint64 nResult = 0;

	HCRYPTPROV hCryptProv; 
	if ( CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET ) )
	{
		CryptGenRandom( hCryptProv, sizeof( nResult ), (BYTE*)&nResult );
		
		CryptReleaseContext( hCryptProv, 0 );
	}
		
	return nResult;
}

static bool HashBuffer( const void *pBuf, uint nLen, uint64 &nHashLow, uint64 &nHashHigh )
{
	bool bResult = false;
	nHashLow = 0;
	nHashHigh = 0;
		
	HCRYPTPROV hCryptProv; 
	if ( CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_MACHINE_KEYSET ) )
	{
		HCRYPTHASH hHash;
		if ( CryptCreateHash( hCryptProv, CALG_MD5, 0, 0, &hHash ) ) 
		{
			if ( CryptHashData( hHash, static_cast< const BYTE * >( pBuf ), nLen, 0 ) )
			{
				BYTE bHash[16];
				DWORD dwHashLen = 16;
				if ( CryptGetHashParam( hHash, HP_HASHVAL, bHash, &dwHashLen, 0 ) )
				{
					nHashLow = *reinterpret_cast< uint64 * >( &bHash[0] );
					nHashHigh = *reinterpret_cast< uint64 * >( &bHash[8] );
					bResult = true;
				}
			}

			CryptDestroyHash( hHash );
		}

		CryptReleaseContext( hCryptProv, 0 );
	}

	return bResult;
}

static uint64 ComputeComboHash( LPCSTR pSrcFile, CONST D3DXMACRO *pDefines, LPCSTR pFunctionName )
{
	std::vector< std::string > defines;
	CONST D3DXMACRO *pCurDefine = pDefines;
	while ( ( pCurDefine->Name ) && ( pCurDefine->Definition ) )
	{
		char buf[1024];
		sprintf_s( buf, sizeof( buf ), "%s=%s", pCurDefine->Name, pCurDefine->Definition );
		defines.push_back( std::string( buf ) );

		pCurDefine++;
	}
	std::sort( defines.begin(), defines.end() );

	std::vector< uint8 > shaderSigBuf;

	shaderSigBuf.reserve( 1024 );
	shaderSigBuf.insert( shaderSigBuf.end(), (uint8 *)pSrcFile, (uint8 *)pSrcFile + strlen( pSrcFile ) );
	shaderSigBuf.insert( shaderSigBuf.end(), (uint8 *)pFunctionName, (uint8 *)pFunctionName + strlen( pFunctionName ) );

	for ( uint i = 0; i < defines.size(); ++i )
	{
		const char *pDefineStr = defines[i].c_str();
		shaderSigBuf.insert( shaderSigBuf.end(), (uint8 *)pDefineStr, (uint8 *)pDefineStr + strlen( pDefineStr ) );
	}

	uint64 nHashLow = 0, nHashHigh = 0;
	if ( shaderSigBuf.size() )
	{
		HashBuffer( &shaderSigBuf[0], shaderSigBuf.size(), nHashLow, nHashHigh );
	}

	return nHashLow ^ nHashHigh;
}

static void WriteToCompileLogFile( const char *pMsg )
{
	char szLogFilename[MAX_PATH];
	if ( !GetEnvironmentVariableA( "PS3COMPILELOG", szLogFilename, sizeof( szLogFilename ) ) )
		return;

	HANDLE hMutex = CreateMutex( NULL, FALSE, "PS3COMPILELOGMUTEX" );
	if ( ( hMutex == NULL ) || ( WaitForSingleObject( hMutex, 10000 ) != WAIT_OBJECT_0 ) )
		return;
	
	FILE *pFile = fopen( szLogFilename, "a+" );
	if ( !pFile )
	{
		ReleaseMutex( hMutex );
		return;
	}
	
	fputs( pMsg, pFile );

	fclose( pFile );

	ReleaseMutex( hMutex );
}

static void UpdateCompileLogFile(
	LPCSTR pSrcFile,
	uint64 nComboHash,
	const SceSpMeasurementResult &origStatistics,
	const SceSpMeasurementResult &bestStatistics,
	int nBestSchedule, uint nBestSeed, 
	int nShaderSchedulerSourceIndex,
	int nDbgStatusIndex )
{
	char szComputerName[512];
	DWORD nSize = sizeof( szComputerName );
	GetComputerNameA( szComputerName, &nSize );
				
	uint64 nGUID = CreateGUID64();

	char msg[1024];
	sprintf_s( msg, sizeof( msg ), "%s,%016I64X,\"%s\",%016I64X,%u,%u,%u,%u,%i,%i,%i,%i\n", 
		szComputerName,
		nGUID, 
		pSrcFile, 
		nComboHash,
		origStatistics.nCycles, origStatistics.nRRegisters, 
		bestStatistics.nCycles, bestStatistics.nRRegisters,
		nBestSchedule, nBestSeed,
		nShaderSchedulerSourceIndex,
		nDbgStatusIndex );

	WriteToCompileLogFile( msg );
}

class COptimalComboFile
{
public:
	COptimalComboFile() :
	  g_bTriedToLoadOptimalCombos( false )
	{
		InitializeCriticalSection( &m_CS );
	}

	~COptimalComboFile()
	{
		DeleteCriticalSection( &m_CS );
	}

	bool Load( const char *pFilename )
	{
		Lock();

		if ( g_OptimalCombos.empty() )
		{
			if ( g_bTriedToLoadOptimalCombos )
			{
				Unlock();
				return false;
			}
			g_bTriedToLoadOptimalCombos = true;

			FILE *pFile = fopen( pFilename, "rb" );
			if ( !pFile )
			{
				Unlock();
				return false;
			}

			fseek( pFile, 0, SEEK_END );
			const uint nFilesize = ftell( pFile );
			fseek( pFile, 0, SEEK_SET );

			g_OptimalCombos.resize( nFilesize );

			if ( fread( &g_OptimalCombos[0], nFilesize, 1, pFile) != 1 )
			{
				fclose( pFile );

				g_OptimalCombos.clear();

				Unlock();
				return false;
			}

			fclose( pFile );

			const OptimalComboScheduleFileHeader_t *pHeader = reinterpret_cast< const OptimalComboScheduleFileHeader_t * >( &g_OptimalCombos[0] );
			if ( ( pHeader->m_nID != OPTIMAL_COMBO_SCHEDULE_FILE_HEADER_ID ) || ( !pHeader->m_nNumCombos ) )
			{
				g_OptimalCombos.clear();

				Unlock();
				return false;
			}
		}

		Unlock();
		return true;
	}

	bool GetOptimalScheduleForCombo( uint64 nComboHash, int &nBestSchedule, int &nBestSeed, SceSpMeasurementResult &bestStatistics )
	{
		if ( g_OptimalCombos.empty() )
			return false;

		const OptimalComboScheduleFileHeader_t *pHeader = reinterpret_cast< const OptimalComboScheduleFileHeader_t * >( &g_OptimalCombos[0] );
		const OptimalComboScheduleFileRecord_t *pCombos = reinterpret_cast< const OptimalComboScheduleFileRecord_t * >( &g_OptimalCombos[sizeof( OptimalComboScheduleFileHeader_t )] );

		int low = 0;
		int high = pHeader->m_nNumCombos - 1;

		while ( low <= high )
		{
			const int mid = ( low + high ) >> 1;

			const OptimalComboScheduleFileRecord_t &combo = pCombos[mid];

			if ( nComboHash == combo.m_nComboHash )
			{
				if ( combo.m_nOptSchedule == OptimalComboScheduleFileRecord_t::cDefaultScheduleIndex )
				{
					nBestSchedule = -1;
					nBestSeed = 0;
				}
				else
				{
					nBestSchedule = combo.m_nOptSchedule;
					nBestSeed = combo.m_nOptSeed;
				}
				bestStatistics.nResult = SCESP_OK;
				bestStatistics.nCycles = combo.m_nOptCycles;
				bestStatistics.nRRegisters = 100; // bogus value - shouldn't matter
				bestStatistics.nThroughput = 1; // bogus value - shouldn't matter
				return true;
			}
			else if ( nComboHash < combo.m_nComboHash )
			{
				high = mid - 1;
			}
			else
			{
				low = mid + 1;
			}
		}

		return false;
	}
	
private:
	void Lock() { EnterCriticalSection( &m_CS ); }
	void Unlock() { LeaveCriticalSection( &m_CS ); }
	
	CRITICAL_SECTION m_CS;
	std::vector< uint8 > g_OptimalCombos;
	bool g_bTriedToLoadOptimalCombos;
};

class CCompiledShader
{
	// Purposely undefined.
	CCompiledShader( const CCompiledShader & );
	CCompiledShader& operator= ( const CCompiledShader & );

public:
	CCompiledShader() : 
		m_pShader( NULL ), 
		m_pErrorMsgs( NULL ),
		m_last_hres( E_FAIL ),
		m_nSchedule( -1 ),
		m_nSeed( 0 ),
		m_nOptLevel( 1 )
	{
		memset( &m_Statistics, 0, sizeof( m_Statistics ) );
		m_Statistics.nResult = SCESP_ERROR_UNKNOWN;
	}
	
	~CCompiledShader()
	{
		Clear();
	}
	
	void Clear()
	{
		if ( m_pShader )
		{
			m_pShader->Release();
			m_pShader = NULL;
		}

		if ( m_pErrorMsgs )
		{
			m_pErrorMsgs->Release();
			m_pErrorMsgs = NULL;
		}

		memset( &m_Statistics, 0, sizeof( m_Statistics ) );
		m_Statistics.nResult = SCESP_ERROR_UNKNOWN;

		m_last_hres = E_FAIL;

		m_nSchedule = -1;
		m_nSeed = 0;
		m_nOptLevel = 1;
	}
	
	LPD3DXBUFFER GetShader() { return m_pShader; }
	LPD3DXBUFFER GetErrorMsgs() { return m_pErrorMsgs; }
	
	LPD3DXBUFFER GetShaderAndReleaseOwnership() { LPD3DXBUFFER pShader = m_pShader; m_pShader = NULL; return pShader; }
	LPD3DXBUFFER GetErrorMsgsAndReleaseOwnership() { LPD3DXBUFFER pErrorMsgs = m_pErrorMsgs; m_pErrorMsgs = NULL; return pErrorMsgs; }

	const SceSpMeasurementResult &GetStatistics() const { return m_Statistics; }
	HRESULT GetLastHRESULT() const { return m_last_hres; }

	int GetSchedule() const { return m_nSchedule; }
	int GetSeed() const { return m_nSeed; }
	int GetOptLevel() const { return m_nOptLevel; }
				
	// Proxied routines
	HRESULT Compile( LPCSTR pSrcFile,
		CONST D3DXMACRO* pDefines,
		LPD3DXINCLUDE pInclude,
		LPCSTR pFunctionName,
		LPCSTR pProfile,
		DWORD Flags,
		int nRandSched = -1, 
		int nRandSeed = -1, 
		int nOptLevel = 1,
		int *pDbgStatusIndex = NULL )
	{
		Clear();

		m_nSchedule = nRandSched;
		m_nSeed = nRandSeed;
		m_nOptLevel = nOptLevel;
		
		LPD3DXBUFFER *ppShader = &m_pShader;
		LPD3DXBUFFER *ppErrorMsgs = &m_pErrorMsgs;

		bool bFragmentShader = false;
		const char * pRsxProfile = pProfile;
		if ( *pProfile == 'v' ) // guessing it's a vertex shader profile
		{
			pRsxProfile = "sce_vp_rsx";
		}
		else if ( *pProfile == 'p' ) // guessing it's a pixel shader profile
		{
			pRsxProfile = "sce_fp_rsx";
			bFragmentShader = true;
		}

		if ( !pInclude )
			pInclude = &s_incDxImpl;

		// Open the top-level file via our include interface
		LPCVOID lpcvData;
		UINT numBytes;
		HRESULT hr = pInclude->Open( ( D3DXINCLUDE_TYPE ) 0, pSrcFile, NULL, &lpcvData, &numBytes );
		if ( FAILED( hr ) )
		{
			m_last_hres = hr;
			return hr;
		}

		LPCSTR pShaderData = ( LPCSTR ) lpcvData;

		g_pInclude = pInclude;
		CGCinclude incWrap;
		incWrap.close = CgcIncludeClose;
		incWrap.open = CgcIncludeOpen;

		std::vector<std::string> options;
		if ( pDefines )
		{
			for ( const D3DXMACRO * pMacro = pDefines; pMacro->Name; pMacro++ )
			{
				std::string strOpt = "-D";
				strOpt += pMacro->Name;
				if( pMacro->Definition && *pMacro->Definition )
				{
					if ( !strncmp( pMacro->Name, "PS3REGCOUNT", 11 ) )
					{
						options.push_back( "-regcount" );
						options.push_back( pMacro->Name + 11 );
						continue;
					}

					// Common case:
					strOpt += "=";
					strOpt += pMacro->Definition;
				}
				options.push_back( strOpt );
			}
		}

		char buf[512];

		if ( ( bFragmentShader ) && ( nRandSched >= 1 ) )
		{
			options.push_back( "-po" );
			sprintf( buf, "randomSched=%i", nRandSched );
			options.push_back( std::string( buf ) );

			options.push_back( "-po" );
			sprintf( buf, "randomSeed=%i", nRandSeed );
			options.push_back( std::string( buf ) );
		}

		options.push_back( "-inline" );
		options.push_back( "all" );

		options.push_back( "-fastmath" );

		sprintf( buf, "-O%i", nOptLevel );
		options.push_back( buf );

		const char ** ppOptions = (const char**)stackalloc( sizeof(char*) * ( options.size() + 1 ) );
		for( uint i = 0; i < options.size(); ++i )
			ppOptions[i] = options[i].c_str();
		ppOptions[options.size()] = NULL;

		DebugLog("%s:%s/%s", pSrcFile, pProfile, pRsxProfile );	

		CgContextWrapper cgcc;
		BlobAdaptor *pCompiledShader = new BlobAdaptor(), *pMessages = new BlobAdaptor(), *asciiOutput = new BlobAdaptor();

		int status = sceCgcCompileString( cgcc, pShaderData, pRsxProfile, pFunctionName, ppOptions, pCompiledShader->m_bin, pMessages->m_bin, asciiOutput->m_bin, &incWrap );

		if ( ( !status ) && ( pCompiledShader ) && ( pCompiledShader->m_bin ) )
		{
			const char* optStr[] = { NULL };
			char *pBinData = static_cast< char * >( sceCgcGetBinData( pCompiledShader->m_bin ) );
			int nBinSize = sceCgcGetBinSize( pCompiledShader->m_bin );

			SceSpResult res = sceShaderPerfMeasure( pBinData, nBinSize, optStr, &m_Statistics );
			if ( res != SCESP_OK )
			{
				DebugLog( "sceShaderPerfMeasure failed with status %i", res );
				if ( pDbgStatusIndex ) 
				{
					*pDbgStatusIndex = -1;
				}
			}
		}

		pCompiledShader->Bake();
		*ppShader = pCompiledShader;

		*ppErrorMsgs = new BlobAdaptor( pMessages, asciiOutput );

#ifdef _DEBUG
		if( status )
			DebugLog( "Error %d:\n%s\n%s", status, pMessages->GetBufferPointer(), asciiOutput->GetBufferPointer() );
		else
			DebugLog( "Success %d bytes", pCompiledShader->GetBufferSize() );
#endif

		pMessages->Release();
		asciiOutput->Release();	

		hr = ( status == SCECGC_OK ? S_OK : 0x80000005 );

		// Close the file
		pInclude->Close( lpcvData );
		m_last_hres = hr;
		return hr;
	}

	CCompiledShader &TakeOwnership( CCompiledShader &src )
	{
		if ( this == &src )
			return *this;

		Clear();

		m_last_hres = src.m_last_hres;

		m_pShader = src.m_pShader; 
		src.m_pShader = NULL;

		m_pErrorMsgs = src.m_pErrorMsgs; 
		src.m_pErrorMsgs = NULL;

		m_Statistics = src.m_Statistics;
		m_nSchedule = src.m_nSchedule;
		m_nSeed = src.m_nSeed;
		m_nOptLevel = src.m_nOptLevel;

		return *this;
	}
		
private:
	HRESULT m_last_hres;
	LPD3DXBUFFER m_pShader;
	LPD3DXBUFFER m_pErrorMsgs;
	SceSpMeasurementResult m_Statistics;
	int m_nSchedule;
	int m_nSeed;
	int m_nOptLevel;
};

COptimalComboFile g_OptimalComboFile;

// Proxied routines
#pragma comment(linker, "/EXPORT:Proxy_D3DXCompileShaderFromFile=?Proxy_D3DXCompileShaderFromFile@@YGJPBDPBU_D3D_SHADER_MACRO@@PAUID3DInclude@@00KPAPAUID3D10Blob@@3PAPAX@Z")
HRESULT WINAPI
Proxy_D3DXCompileShaderFromFile(LPCSTR                          pSrcFile,
								CONST D3DXMACRO*                pDefines,
								LPD3DXINCLUDE                   pInclude,
								LPCSTR                          pFunctionName,
								LPCSTR                          pProfile,
								DWORD                           Flags,
								LPD3DXBUFFER*                   ppShader,
								LPD3DXBUFFER*                   ppErrorMsgs,
								LPD3DXCONSTANTTABLE*            ppConstantTable )
{
	*ppShader = NULL;
	*ppErrorMsgs = NULL;
	if ( ppConstantTable ) *ppConstantTable = NULL;

	static bool bInitializedShaderPerfLib;
	if ( !bInitializedShaderPerfLib )
	{
		bInitializedShaderPerfLib = true;
		sceShaderPerfInit();
	}

	if ( *pProfile == 'v' )
	{
		CCompiledShader compiledShader;
		HRESULT hres = compiledShader.Compile( pSrcFile, pDefines, pInclude, pFunctionName, pProfile, Flags, -1, -1, 1 );
		if ( FAILED( hres ) )
		{
			*ppErrorMsgs = compiledShader.GetErrorMsgsAndReleaseOwnership();
			return hres;
		}

		*ppShader = compiledShader.GetShaderAndReleaseOwnership();
		return S_OK;
	}

	const uint nStartTime = GetTickCount();
	
	const uint64 nComboHash = ComputeComboHash( pSrcFile, pDefines, pFunctionName );

	char szOptimalScheduleFile[MAX_PATH];
	const bool bUseOptimalSchedulingFile = GetEnvironmentVariableA( "PS3OPTIMALSCHEDULESFILE", szOptimalScheduleFile, sizeof( szOptimalScheduleFile ) ) && szOptimalScheduleFile[0];

	char szFindOptimalSchedulesValue[MAX_PATH];
	const bool bFindOptimalScheduling = !bUseOptimalSchedulingFile && ( GetEnvironmentVariableA( "PS3FINDOPTIMALSCHEDULES", szFindOptimalSchedulesValue, sizeof( szFindOptimalSchedulesValue ) ) && ( szFindOptimalSchedulesValue[0] == '1' ) );
		
	ShaderSchedulerParamSource_t nShaderSchedulerSourceIndex = SHADER_SCHEDULER_PARAM_SOURCE_UNOPTIMIZED;
	
	SceSpMeasurementResult trainedScheduleResults;
	memset( &trainedScheduleResults, 0, sizeof( trainedScheduleResults ) );
	int nTrainedSchedule = -1;
	int nTrainedSeed = 0;
	int nDbgStatusIndex = 0;
	
	if ( ( bUseOptimalSchedulingFile ) && ( g_OptimalComboFile.Load( szOptimalScheduleFile ) ) )
	{
		if ( g_OptimalComboFile.GetOptimalScheduleForCombo( nComboHash, nTrainedSchedule, nTrainedSeed, trainedScheduleResults ) )
		{
			nShaderSchedulerSourceIndex = SHADER_SCHEDULER_PARAM_SOURCE_FROM_SCHEDULER_FILE;
			nDbgStatusIndex = 1;
		}
	}

	uint nTotalCompiles = 0;
	
	CCompiledShader defaultShader;
	nTotalCompiles++;
	HRESULT hres = defaultShader.Compile( pSrcFile, pDefines, pInclude, pFunctionName, pProfile, Flags, nTrainedSchedule, nTrainedSeed, CGC_COMPILER_OPTIMIZATION_LEVEL, &nDbgStatusIndex );
	if ( FAILED( hres ) )
	{
		*ppErrorMsgs = defaultShader.GetErrorMsgsAndReleaseOwnership();
		return hres;
	}
	
	CCompiledShader bestShader;
	bestShader.TakeOwnership( defaultShader );
	
	if ( ( nShaderSchedulerSourceIndex == SHADER_SCHEDULER_PARAM_SOURCE_FROM_SCHEDULER_FILE ) && ( defaultShader.GetStatistics().nCycles > trainedScheduleResults.nCycles ) )
	{
		// The optimal schedule params stored in the ps3optimalschedules.bin file didn't produce the expected results (the shader was modified since the 
		// schedules where optimized), so try falling back to the compiler's default scheduling. (Which may not be any better, but at least we'll never get worse than the default schedule.)

		nTotalCompiles++;
		
		CCompiledShader alternateShader;
		HRESULT hres = alternateShader.Compile( pSrcFile, pDefines, pInclude, pFunctionName, pProfile, Flags, -1, 0, CGC_COMPILER_OPTIMIZATION_LEVEL, &nDbgStatusIndex );
		if ( FAILED( hres ) )
		{
			*ppErrorMsgs = alternateShader.GetErrorMsgsAndReleaseOwnership();
			return hres;
		}

		if ( alternateShader.GetStatistics() < bestShader.GetStatistics() )
		{
			nShaderSchedulerSourceIndex = SHADER_SCHEDULER_PARAM_SOURCE_UNOPTIMIZED_FALLBACK;
			bestShader.TakeOwnership( alternateShader );
			
			nDbgStatusIndex = 2;
		}
	}
	
	SceSpMeasurementResult origStatistics( bestShader.GetStatistics() );
		
	// Don't bother trying to optimize tiny shaders, the potential gain is not worth it (and they're probably fill bound anyway).
	if ( ( bFindOptimalScheduling ) && ( ( bestShader.GetStatistics().nCycles > 5 ) || ( bestShader.GetStatistics().nRRegisters > 2 ) ) )
	{
		// Important: Watch the ranges of rand_schedule and rand_seed. See COMBO_SEED_BITS and COMBO_SCHEDULE_BITS.
		for ( int nRandSchedIndex = 0; nRandSchedIndex < NUM_RANDOM_SCHEDULE_VALUES; ++nRandSchedIndex )
		{
			const int nRandSched = g_nRandSched[nRandSchedIndex];

			for ( int nTrial = 0; nTrial < NUM_RANDOM_SCHEDULE_SEEDS; ++nTrial )
			{
				const int nRandSeed = 10 + nTrial * 8;

				nTotalCompiles++;

				CCompiledShader trialShader;
				HRESULT hres = trialShader.Compile( pSrcFile, pDefines, pInclude, pFunctionName, pProfile, Flags, nRandSched, nRandSeed, CGC_COMPILER_OPTIMIZATION_LEVEL, &nDbgStatusIndex );
				if ( FAILED( hres ) )
				{
					*ppErrorMsgs = trialShader.GetErrorMsgsAndReleaseOwnership();
					
					return hres;
				}

				if ( trialShader.GetStatistics() < bestShader.GetStatistics() )
				{
					bestShader.TakeOwnership( trialShader );
					
					nShaderSchedulerSourceIndex = SHADER_SCHEDULER_PARAM_SOURCE_FOUND_OPTIMAL;
					
					nDbgStatusIndex = 3;
				}
			}
		}
	}

	*ppShader = bestShader.GetShaderAndReleaseOwnership();

	const uint nEndTime = GetTickCount();
	double flTotalTime = ( nEndTime - nStartTime ) * .001f;
	flTotalTime;
	
	UpdateCompileLogFile( pSrcFile, nComboHash, origStatistics, bestShader.GetStatistics(), bestShader.GetSchedule(), bestShader.GetSeed(), nShaderSchedulerSourceIndex, nDbgStatusIndex );
	
#if 0
	printf( "Orig cycles/registers: %u (%u), Optimized cycles/registers: %u (%u), Total compiles: %u, ms per compile: %f\n", 
		origStatistics.nCycles, origStatistics.nRRegisters, 
		bestShader.GetStatistics().nCycles, bestShader.GetStatistics().nRRegisters,
		nTotalCompiles, 
		1000.0f * ( flTotalTime / nTotalCompiles ) );
#endif
	
	return S_OK;
}
