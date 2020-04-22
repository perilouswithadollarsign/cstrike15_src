//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
// An interface that should not ever be accessed directly from shaders
// but instead is visible only to shaderlib.
//===========================================================================//

#ifndef ISHADERSYSTEM_H
#define ISHADERSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"
#include <materialsystem/IShader.h>
#include <materialsystem/ishadersystem_declarations.h>
#include "shaderlib/shadercombosemantics.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
enum Sampler_t;
class ITexture;
class IShader;
struct ShaderComboSemantics_t;

//-----------------------------------------------------------------------------
// The Shader system interface version
//-----------------------------------------------------------------------------
#define SHADERSYSTEM_INTERFACE_VERSION		"ShaderSystem002"



//-----------------------------------------------------------------------------
// The shader system (a singleton)
//-----------------------------------------------------------------------------
abstract_class IShaderSystem
{
public:
	virtual ShaderAPITextureHandle_t GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrameVar, int nTextureChannel = 0 ) =0;

	// Binds a texture
	virtual void BindTexture( Sampler_t sampler1, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar = 0 ) = 0;
	virtual void BindTexture( Sampler_t sampler1, Sampler_t sampler2, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar = 0 ) = 0;

	// Takes a snapshot
	virtual void TakeSnapshot( ) = 0;

	// Draws a snapshot
	virtual void DrawSnapshot( const unsigned char *pInstanceCommandBuffer, bool bMakeActualDrawCall = true ) = 0;

	// Are we using graphics?
	virtual bool IsUsingGraphics() const = 0;

	// Are editor materials enabled?
	virtual bool CanUseEditorMaterials() const = 0;

	// Bind vertex texture
	virtual void BindVertexTexture( VertexTextureSampler_t vtSampler, ITexture *pTexture, int nFrameVar = 0 ) = 0;

	virtual void AddShaderComboInformation( const ShaderComboSemantics_t *pSemantics ) = 0;
};


//-----------------------------------------------------------------------------
// The Shader plug-in DLL interface version
//-----------------------------------------------------------------------------
#define SHADER_DLL_INTERFACE_VERSION	"ShaderDLL004"


//-----------------------------------------------------------------------------
// The Shader interface versions
//-----------------------------------------------------------------------------
abstract_class IShaderDLLInternal
{
public:
	// Here's where the app systems get to learn about each other 
	virtual bool Connect( CreateInterfaceFn factory, bool bIsMaterialSystem ) = 0;
	virtual void Disconnect( bool bIsMaterialSystem ) = 0;

	// Returns the number of shaders defined in this DLL
	virtual int ShaderCount() const = 0;

	// Returns information about each shader defined in this DLL
	virtual IShader *GetShader( int nShader ) = 0;

	// Deals with all of the shader combo semantics from inc files.
	virtual int ShaderComboSemanticsCount() const = 0;
	virtual const ShaderComboSemantics_t *GetComboSemantics( int n ) = 0;
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
IShaderDLLInternal *GetShaderDLLInternal();


#ifdef _PS3
//////////////////////////////////////////////////////////////////////////
//
// PS3 non-virtual implementation proxy
//
// cat ishadersystem.h | nonvirtualscript.pl > shadersystem_ps3nonvirt.inl
struct CPs3NonVirt_IShaderSystem
{
//NONVIRTUALSCRIPTBEGIN
//NONVIRTUALSCRIPT/PROXY/CPs3NonVirt_IShaderSystem
//NONVIRTUALSCRIPT/DELEGATE/s_ShaderSystem.CShaderSystem::

	//
	// IShaderSystem
	//
	static ShaderAPITextureHandle_t GetShaderAPITextureBindHandle( ITexture *pTexture, int nFrameVar, int nTextureChannel = 0 );
	static void BindTexture( Sampler_t sampler1, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar = 0 );
	static void BindTexture( Sampler_t sampler1, Sampler_t sampler2, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrameVar = 0 );
	static void TakeSnapshot();
	static void DrawSnapshot( const unsigned char *pInstanceCommandBuffer, bool bMakeActualDrawCall = true );
	static bool IsUsingGraphics();
	static bool CanUseEditorMaterials();
	static void BindVertexTexture( VertexTextureSampler_t vtSampler, ITexture *pTexture, int nFrameVar = 0 );

//NONVIRTUALSCRIPTEND
};
#endif


#endif // ISHADERSYSTEM_H
