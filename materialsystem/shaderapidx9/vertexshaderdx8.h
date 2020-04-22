//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================

#ifndef VERTEXSHADERDX8_H
#define VERTEXSHADERDX8_H

#ifdef _WIN32
#pragma once
#endif

#include "shaderapi/ishaderapi.h"
#include "datacache/idatacache.h"
#include "locald3dtypes.h"


// uncomment to get dynamic compilation for HLSL shaders
// **IMPORTANT**: if you are trying to use DYNAMIC_SHADER_COMPILE in a RELEASE build, you *must* modify the .vpc file to re-enable the /FC compiler option, otherwise the __FILE__ macro won't work properly in
// GetShaderSourcePath() and shader files won't be found.
//#define DYNAMIC_SHADER_COMPILE

// uncomment to get spew about what combos are being compiled.
//#define DYNAMIC_SHADER_COMPILE_VERBOSE

// uncomment to hide combos that are 0 (requires VERBOSE above)
//#define DYNAMIC_SHADER_COMPILE_THIN

// Uncomment to use remoteshadercompiler.exe as a shader compile server
// Must also set mat_remoteshadercompile to remote shader compile machine name
//#define REMOTE_DYNAMIC_SHADER_COMPILE

// uncomment and fill in with a path to use a specific set of shader source files. Meant for network use.
//		PC path format is of style "\\\\somemachine\\sourcetreeshare\\materialsystem\\stdshaders"
//		Mac path format is of style "/Volumes/jasonm/portal2/staging/src/materialsystem/stdshaders"
//		Linux path format is of style "/home/mariod/p4/csgo/trunk/src/materialsystem/stdshaders"
//		Xbox is not supported. Xbox's ability to see PC is not supported by XDK in Vista.
//#define DYNAMIC_SHADER_COMPILE_CUSTOM_PATH "/Volumes/Data/p4/csgo/staging/src/materialsystem/stdshaders"

//#define SHADER_COMBO_SPEW_VERBOSE 1

// uncomment to get disassembled (asm) shader code in your game dir as *.asm
//#define DYNAMIC_SHADER_COMPILE_WRITE_ASSEMBLY

// uncomment to get disassembled (asm) shader code in your game dir as *.asm
//#define WRITE_ASSEMBLY

#if defined( DYNAMIC_SHADER_COMPILE ) && defined( _X360 ) && !defined( X360_LINK_WITH_SHADER_COMPILE )
// automatically turn on X360_LINK_WITH_SHADER_COMPILE with dynamic shader compile
#define X360_LINK_WITH_SHADER_COMPILE 1
#endif

#if defined( _X360 )
// Define this to link shader compilation code from D3DX9.LIB
//#define X360_LINK_WITH_SHADER_COMPILE 1
#endif
#if defined( X360_LINK_WITH_SHADER_COMPILE ) && defined( _CERT )
#error "Don't ship with X360_LINK_WITH_SHADER_COMPILE defined!! It causes 2MB+ DLL bloat. Only define it while revving XDKs."
#endif

//-----------------------------------------------------------------------------
// Vertex + pixel shader manager
//-----------------------------------------------------------------------------
abstract_class IShaderManager
{
protected:

	// The current vertex and pixel shader index
	int m_nVertexShaderIndex;
	int m_nPixelShaderIndex;

public:
	// Initialize, shutdown
	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	// Compiles vertex shaders
	virtual IShaderBuffer *CompileShader( const char *pProgram, size_t nBufLen, const char *pShaderVersion ) = 0;

	// New version of these methods	[dx10 port]
	virtual VertexShaderHandle_t CreateVertexShader( IShaderBuffer* pShaderBuffer ) = 0;
	virtual void DestroyVertexShader( VertexShaderHandle_t hShader ) = 0;
	virtual PixelShaderHandle_t CreatePixelShader( IShaderBuffer* pShaderBuffer ) = 0;
	virtual void DestroyPixelShader( PixelShaderHandle_t hShader ) = 0;

	// Creates vertex, pixel shaders
	virtual VertexShader_t CreateVertexShader( const char *pVertexShaderFile, int nStaticVshIndex = 0, char *debugLabel = NULL  ) = 0;
	virtual PixelShader_t CreatePixelShader( const char *pPixelShaderFile, int nStaticPshIndex = 0, char *debugLabel = NULL ) = 0;

	// Sets which dynamic version of the vertex + pixel shader to use
	FORCEINLINE void SetVertexShaderIndex( int vshIndex );
	FORCEINLINE void SetPixelShaderIndex( int pshIndex );

	// Sets the vertex + pixel shader render state
	virtual void SetVertexShader( VertexShader_t shader ) = 0;
	virtual void SetPixelShader( PixelShader_t shader ) = 0;

	virtual void SetPixelShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader  ) = 0;
	virtual void SetVertexShaderState_Internal( HardwareShader_t shader, DataCacheHandle_t hCachedShader ) = 0;

	// Resets the vertex + pixel shader state
	virtual void ResetShaderState() = 0;

	// Flushes all shaders so they are reloaded+recompiled (does nothing unless dynamic shader compile is enabled)
	virtual void FlushShaders() = 0;

	// Returns the current vertex + pixel shaders
	virtual void *GetCurrentVertexShader() = 0;
	virtual void *GetCurrentPixelShader() = 0;

	virtual void ClearVertexAndPixelShaderRefCounts() = 0;
	virtual void PurgeUnusedVertexAndPixelShaders() = 0;

	// The low-level dx call to set the vertex shader state
	virtual void BindVertexShader( VertexShaderHandle_t shader ) = 0;
	virtual void BindPixelShader( PixelShaderHandle_t shader ) = 0;

	virtual void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics ) = 0;

#if defined( _X360 )
	virtual const char *GetActiveVertexShaderName() = 0;
	virtual const char *GetActivePixelShaderName() = 0;
#endif

#if defined( DX_TO_GL_ABSTRACTION ) && !defined( _GAMECONSOLE )
	virtual void DoStartupShaderPreloading() = 0;
#endif

	virtual HardwareShader_t GetVertexShader( VertexShader_t vs, int dynIdx ) = 0;
	virtual HardwareShader_t GetPixelShader( PixelShader_t ps, int dynIdx ) = 0;
};

//-----------------------------------------------------------------------------
//
// Methods related to setting vertex + pixel shader state
//
//-----------------------------------------------------------------------------
FORCEINLINE void IShaderManager::SetVertexShaderIndex( int vshIndex )
{
	m_nVertexShaderIndex = vshIndex;
}

FORCEINLINE void IShaderManager::SetPixelShaderIndex( int pshIndex )
{
	m_nPixelShaderIndex = pshIndex;
}

extern void DestroyAllVertexAndPixelShaders( void );

#endif // VERTEXSHADERDX8_H
