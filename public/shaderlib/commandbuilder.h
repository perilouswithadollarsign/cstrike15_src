//===== Copyright (c) Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
// Utility class for building command buffers into memory
//==================================================================//

#ifndef COMMANDBUILDER_H
#define COMMANDBUILDER_H


#ifdef _WIN32
#pragma once
#endif


#include "shaderapi/commandbuffer.h"
#include "shaderapi/ishaderapi.h"
#include "shaderlib/BaseShader.h"
#include "tier1/convar.h"

#ifdef _PS3
#include "ps3gcm\gcmdrawstate.h"
#include "ps3gcm\gcmtexture.h"
#endif

#ifdef DBGFLAG_ASSERT
#define TRACK_STORAGE 1
#else
#define TRACK_STORAGE 0
#endif

//-----------------------------------------------------------------------------
// Buffer for storing commands into
//-----------------------------------------------------------------------------
template<int N> class CFixedCommandStorageBuffer
{
public:
	uint8 m_Data[N];

	uint8 *m_pDataOut;
#if TRACK_STORAGE
	size_t m_nNumBytesRemaining;
#endif
	
	FORCEINLINE CFixedCommandStorageBuffer( void )
	{
		m_pDataOut = m_Data;
#if TRACK_STORAGE
		m_nNumBytesRemaining = N;
#endif

    }

	FORCEINLINE void EnsureCapacity( size_t sz )
	{
#if TRACK_STORAGE
		if ( m_nNumBytesRemaining < sz + 32 )
			Error( "getting scary\n" );
#endif
		Assert( m_nNumBytesRemaining >= sz );
	}

	template<class T> FORCEINLINE void Put( T const &nValue )
	{
		EnsureCapacity( sizeof( T ) );
		*( reinterpret_cast<T *>( m_pDataOut ) ) = nValue;
		m_pDataOut += sizeof( nValue );
#if TRACK_STORAGE
		m_nNumBytesRemaining -= sizeof( nValue );
#endif
	}

	FORCEINLINE void PutInt( int nValue )
	{
		Put( nValue );
	}

	FORCEINLINE void PutFloat( float nValue )
	{
		Put( nValue );
	}

	FORCEINLINE void PutPtr( void * pPtr )
	{
		Put( pPtr );
	}
	
	FORCEINLINE void PutMemory( const void *pMemory, size_t nBytes )
	{
		EnsureCapacity( nBytes );
		memcpy( m_pDataOut, pMemory, nBytes );
		m_pDataOut += nBytes;
#if TRACK_STORAGE
		m_nNumBytesRemaining -= nBytes;
#endif
	}

	FORCEINLINE uint8 *Base( void )
	{
		return m_Data;
	}

	FORCEINLINE void Reset( void )
	{
		m_pDataOut = m_Data;
#if TRACK_STORAGE
		m_nNumBytesRemaining = N;
#endif
	}

	FORCEINLINE size_t Size( void ) const
	{
		return m_pDataOut - m_Data;
	}

};

#ifdef _PS3

class CDynamicCommandStorageBuffer
{
public:
	uint8 *m_Data;
	uint8 *m_pDataOut;
#if TRACK_STORAGE_PS3
	size_t m_nNumBytesRemaining;
#endif

	FORCEINLINE CDynamicCommandStorageBuffer()
	{
		m_Data = gpGcmDrawState->OpenDynECB();
		m_pDataOut = m_Data;
#if TRACK_STORAGE_PS3
		m_nNumBytesRemaining = 0x1000;
#endif
	}

	FORCEINLINE void EnsureCapacity( size_t sz )
	{
#if TRACK_STORAGE_PS3
		if ( m_nNumBytesRemaining < sz + 32 )
			Error( "getting scary\n" );
		Assert( m_nNumBytesRemaining >= sz );
#endif
	}

	template<class T> FORCEINLINE void Put( T const &nValue )
	{
		EnsureCapacity( sizeof( T ) );
		*( reinterpret_cast<T *>( m_pDataOut ) ) = nValue;
		m_pDataOut += sizeof( nValue );
#if TRACK_STORAGE_PS3
		m_nNumBytesRemaining -= sizeof( nValue );
#endif
	}

	FORCEINLINE void PutInt( int nValue )
	{
		Put( nValue );
	}

	FORCEINLINE void PutFloat( float nValue )
	{
		Put( nValue );
	}

	FORCEINLINE void PutPtr( void * pPtr )
	{
		Put( pPtr );
	}

	FORCEINLINE void PutMemory( const void *pMemory, size_t nBytes )
	{
		EnsureCapacity( nBytes );
		memcpy( m_pDataOut, pMemory, nBytes );
		m_pDataOut += nBytes;
#if TRACK_STORAGE_PS3
		m_nNumBytesRemaining -= nBytes;
#endif
	}

	FORCEINLINE uint8 *Base( void )
	{
		return m_Data;
	}

	FORCEINLINE void Reset( void )
	{
		m_pDataOut = m_Data;
#if TRACK_STORAGE_PS3
		m_nNumBytesRemaining = N;
#endif
	}

	FORCEINLINE size_t Size( void ) const
	{
		return m_pDataOut - m_Data;
	}

};


#endif

//-----------------------------------------------------------------------------
// Base class used to build up command buffers
//-----------------------------------------------------------------------------
template<class S> class CBaseCommandBufferBuilder
{
public:
#ifdef _PS3
	ALIGN16 S m_Storage ALIGN16_POST;
#else
    S m_Storage;
#endif

	FORCEINLINE void End( void )
	{
		m_Storage.PutInt( CBCMD_END );
	}

	FORCEINLINE IMaterialVar *Param( int nVar ) const
	{
		return CBaseShader::s_ppParams[nVar];
	}

	FORCEINLINE void Reset( void )
	{
		m_Storage.Reset();
	}

	FORCEINLINE size_t Size( void ) const
	{
		return m_Storage.Size();
	}

	FORCEINLINE uint8 *Base( void )
	{
		return m_Storage.Base();
	}

	FORCEINLINE void OutputConstantData( float const *pSrcData )
	{
		m_Storage.PutFloat( pSrcData[0] );
		m_Storage.PutFloat( pSrcData[1] );
		m_Storage.PutFloat( pSrcData[2] );
		m_Storage.PutFloat( pSrcData[3] );
	}

	FORCEINLINE void OutputConstantData4( float flVal0, float flVal1, float flVal2, float flVal3 )
	{
		m_Storage.PutFloat( flVal0 );
		m_Storage.PutFloat( flVal1 );
		m_Storage.PutFloat( flVal2 );
		m_Storage.PutFloat( flVal3 );
	}
};


//-----------------------------------------------------------------------------
// Used by SetPixelShaderFlashlightState
//-----------------------------------------------------------------------------
struct CBCmdSetPixelShaderFlashlightState_t
{
	Sampler_t m_LightSampler;
	Sampler_t m_DepthSampler;
	Sampler_t m_ShadowNoiseSampler;
	int m_nColorConstant;
	int m_nAttenConstant;
	int m_nOriginConstant;
	int m_nDepthTweakConstant;
	int m_nScreenScaleConstant;
	int m_nWorldToTextureConstant;
	bool m_bFlashlightNoLambert;
	bool m_bSinglePassFlashlight;
};


//-----------------------------------------------------------------------------
// Used to build a per-pass command buffer
//-----------------------------------------------------------------------------
template<class S> class CCommandBufferBuilder : public CBaseCommandBufferBuilder<S>
{
	typedef CBaseCommandBufferBuilder<S> PARENT;

#ifdef _PS3
	uint32 m_numPs3Tex;
#endif

public:

#ifdef _PS3
	FORCEINLINE CCommandBufferBuilder()
	{
		// For PS3, command buffers begin with up to four Std textures

		m_numPs3Tex = 0;

 		this->m_Storage.PutInt(CBCMD_LENGTH);
 		this->m_Storage.PutInt(0);

		this->m_Storage.PutInt(CBCMD_PS3TEX);
		for(int i = 0; i < CBCMD_MAX_PS3TEX; i++) this->m_Storage.PutInt(0);
	
	}

	FORCEINLINE void Reset()
	{
		this->m_Storage.Reset();

		m_numPs3Tex = 0;

		this->m_Storage.PutInt(CBCMD_LENGTH);
		this->m_Storage.PutInt(0);

		this->m_Storage.PutInt(CBCMD_PS3TEX);
		for(int i = 0; i < CBCMD_MAX_PS3TEX; i++) this->m_Storage.PutInt(0);
	}

	FORCEINLINE int* GetPs3Textures()
	{
		return (int*) (this->m_Storage.Base() + sizeof(int) + 2*sizeof(int));
	}

#endif

	FORCEINLINE void End( void )
	{
		this->m_Storage.PutInt( CBCMD_END );

#ifdef _PS3
		uint32 len = this->m_Storage.Size();

		if ( (this->m_Storage.m_Data >= g_aDynECB) && (this->m_Storage.m_Data < &g_aDynECB[sizeof(g_aDynECB)]) )
		{
			gpGcmDrawState->CloseDynECB(len);
		}

 		uint32* pLength = (uint32*)(this->m_Storage.m_Data + 4);
 		if (pLength[-1] != CBCMD_LENGTH) Error("Length missing\n");
 		*pLength = len;
#endif
	}

	FORCEINLINE void SetPixelShaderConstants( int nFirstConstant, int nConstants )
	{
		this->m_Storage.PutInt( CBCMD_SET_PIXEL_SHADER_FLOAT_CONST );
		this->m_Storage.PutInt( nFirstConstant );
		this->m_Storage.PutInt( nConstants );
	}

	FORCEINLINE void OutputConstantData( float const *pSrcData )
	{
		this->m_Storage.PutFloat( pSrcData[0] );
		this->m_Storage.PutFloat( pSrcData[1] );
		this->m_Storage.PutFloat( pSrcData[2] );
		this->m_Storage.PutFloat( pSrcData[3] );
	}
	
	FORCEINLINE void OutputConstantData4( float flVal0, float flVal1, float flVal2, float flVal3 )
	{
		this->m_Storage.PutFloat( flVal0 );
		this->m_Storage.PutFloat( flVal1 );
		this->m_Storage.PutFloat( flVal2 );
		this->m_Storage.PutFloat( flVal3 );
	}

	FORCEINLINE void SetPixelShaderConstant( int nFirstConstant, float const *pSrcData, int nNumConstantsToSet )
	{
		SetPixelShaderConstants( nFirstConstant, nNumConstantsToSet );
		this->m_Storage.PutMemory( pSrcData, 4 * sizeof( float ) * nNumConstantsToSet );
	}

	FORCEINLINE void SetPixelShaderConstant( int nFirstConstant, int nVar )
	{
		SetPixelShaderConstant( nFirstConstant, this->Param( nVar )->GetVecValue() );
	}

	void SetPixelShaderConstantGammaToLinear( int pixelReg, int constantVar )
	{
		float val[4];
		this->Param(constantVar)->GetVecValue( val, 3 );
		val[0] = val[0] > 1.0f ? val[0] : GammaToLinear( val[0] );
		val[1] = val[1] > 1.0f ? val[1] : GammaToLinear( val[1] );
		val[2] = val[2] > 1.0f ? val[2] : GammaToLinear( val[2] );
		val[3] = 1.0;
		SetPixelShaderConstant( pixelReg, val );
	}

	FORCEINLINE void SetPixelShaderConstant( int nFirstConstant, float const *pSrcData )
	{
		SetPixelShaderConstants( nFirstConstant, 1 );
		OutputConstantData( pSrcData );
	}

	FORCEINLINE void SetPixelShaderConstant4( int nFirstConstant, float flVal0, float flVal1, float flVal2, float flVal3 )
	{
		SetPixelShaderConstants( nFirstConstant, 1 );
		OutputConstantData4( flVal0, flVal1, flVal2, flVal3 );
	}

	FORCEINLINE void SetPixelShaderConstant_W( int pixelReg, int constantVar, float fWValue )
	{
		if ( constantVar != -1 )
		{
			float val[3];
			this->Param(constantVar)->GetVecValue( val, 3);
			SetPixelShaderConstant4( pixelReg, val[0], val[1], val[2], fWValue );
		}
	}

	void SetPixelShaderTextureTransform( int vertexReg, int transformVar )
	{
		Vector4D transformation[2];
		IMaterialVar* pTransformationVar = ( transformVar >= 0 ) ? this->Param( transformVar ) : NULL;
		if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
		{
			const VMatrix &mat = pTransformationVar->GetMatrixValue();
			transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
			transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
		}
		else
		{
			transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
			transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
		}
		SetPixelShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
	}

	FORCEINLINE void SetVertexShaderConstant( int nFirstConstant, float const *pSrcData )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_FLOAT_CONST );
		this->m_Storage.PutInt( nFirstConstant );
		this->m_Storage.PutInt( 1 );
		OutputConstantData( pSrcData );
	}

	FORCEINLINE void SetVertexShaderConstant( int nFirstConstant, float const *pSrcData, int nConsts )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_FLOAT_CONST );
		this->m_Storage.PutInt( nFirstConstant );
		this->m_Storage.PutInt( nConsts );
		this->m_Storage.PutMemory( pSrcData, 4 * nConsts * sizeof( float ) );
	}


	FORCEINLINE void SetVertexShaderConstant4( int nFirstConstant, float flVal0, float flVal1, float flVal2, float flVal3 )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_FLOAT_CONST );
		this->m_Storage.PutInt( nFirstConstant );
		this->m_Storage.PutInt( 1 );
		this->m_Storage.PutFloat( flVal0 );
		this->m_Storage.PutFloat( flVal1 );
		this->m_Storage.PutFloat( flVal2 );
		this->m_Storage.PutFloat( flVal3 );
	}

	void SetVertexShaderTextureTransform( int vertexReg, int transformVar )
	{
		Vector4D transformation[2];
		IMaterialVar* pTransformationVar = ( transformVar >= 0 ) ? this->Param( transformVar ) : NULL;
		if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
		{
			const VMatrix &mat = pTransformationVar->GetMatrixValue();
			transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
			transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
		}
		else
		{
			transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
			transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
		}
		SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
	}


	void SetVertexShaderTextureScaledTransformRotate( int vertexReg, int transformVar, int scaleVar, int rotateVar )
	{
		Vector2D scale( 1, 1 );
		IMaterialVar* pScaleVar = this->Param( scaleVar );
		if (pScaleVar)
		{
			if (pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
				pScaleVar->GetVecValue( scale.Base(), 2 );
			else if (pScaleVar->IsDefined())
				scale[0] = scale[1] = pScaleVar->GetFloatValue();
		}

		float flRotateVar = 0.0f;
		IMaterialVar* pRotateVar = this->Param( rotateVar );
		if ( pRotateVar && pRotateVar->IsDefined() )
		{
			flRotateVar = pRotateVar->GetFloatValue();
		}

		Vector4D transformation[2];
		IMaterialVar* pTransformationVar = this->Param( transformVar );
		if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
		{
			VMatrix matRot = pTransformationVar->GetMatrixValue();
			MatrixTranslate( matRot, Vector( 0.5, 0.5, 0 ) );
			MatrixRotate(	 matRot, Vector( 0, 0, 1), flRotateVar );
			MatrixTranslate( matRot, Vector( -0.5 * scale[0], -0.5 * scale[1], 0 ) );
			matRot = matRot.Scale( Vector(scale[0], scale[1], 1) );

			transformation[0].Init( matRot[0][0], matRot[0][1], matRot[0][2], matRot[0][3] );
			transformation[1].Init( matRot[1][0], matRot[1][1], matRot[1][2], matRot[1][3] );

			SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
		}
	}


	void SetVertexShaderTextureScaledTransform( int vertexReg, int transformVar, int scaleVar )
	{
		Vector4D transformation[2];
		IMaterialVar* pTransformationVar = this->Param( transformVar );
		if (pTransformationVar && (pTransformationVar->GetType() == MATERIAL_VAR_TYPE_MATRIX))
		{
			const VMatrix &mat = pTransformationVar->GetMatrixValue();
			transformation[0].Init( mat[0][0], mat[0][1], mat[0][2], mat[0][3] );
			transformation[1].Init( mat[1][0], mat[1][1], mat[1][2], mat[1][3] );
		}
		else
		{
			transformation[0].Init( 1.0f, 0.0f, 0.0f, 0.0f );
			transformation[1].Init( 0.0f, 1.0f, 0.0f, 0.0f );
		}

		Vector2D scale( 1, 1 );
		IMaterialVar* pScaleVar = this->Param( scaleVar );
		if (pScaleVar)
		{
			if (pScaleVar->GetType() == MATERIAL_VAR_TYPE_VECTOR)
				pScaleVar->GetVecValue( scale.Base(), 2 );
			else if (pScaleVar->IsDefined())
				scale[0] = scale[1] = pScaleVar->GetFloatValue();
		}
		
		// Apply the scaling
		transformation[0][0] *= scale[0];
		transformation[0][1] *= scale[1];
		transformation[1][0] *= scale[0];
		transformation[1][1] *= scale[1];
		transformation[0][3] *= scale[0];
		transformation[1][3] *= scale[1];
		SetVertexShaderConstant( vertexReg, transformation[0].Base(), 2 ); 
	}

	FORCEINLINE void SetEnvMapTintPixelShaderDynamicState( int pixelReg, int tintVar )
	{
		if( g_pConfig->bShowSpecular && g_pConfig->nFullbright != 2 )
		{
			SetPixelShaderConstant( pixelReg, this->Param( tintVar)->GetVecValue() );
		}
		else
		{
			SetPixelShaderConstant4( pixelReg, 0.0, 0.0, 0.0, 0.0 );
		}
	}

	FORCEINLINE void SetEnvMapTintPixelShaderDynamicStateGammaToLinear( int pixelReg, int tintVar, float fAlphaVal = 1.0f )
	{
		if( g_pConfig->bShowSpecular && g_pConfig->nFullbright != 2 )
		{
			float color[4];
			color[3] = fAlphaVal;

			//this->Param( tintVar)->GetLinearVecValue( color, 3 );
			// (wills) converted this line to the following so that envmaptint can be over-driven beyond 0-1 range

			this->Param( tintVar)->GetVecValue( color, 3 );
			color[0] = GammaToLinearFullRange( color[0] );
			color[1] = GammaToLinearFullRange( color[1] );
			color[2] = GammaToLinearFullRange( color[2] );

			SetPixelShaderConstant( pixelReg, color );
		}
		else
		{
			SetPixelShaderConstant4( pixelReg, 0.0, 0.0, 0.0, fAlphaVal );
		}
	}

	FORCEINLINE void StoreEyePosInPixelShaderConstant( int nConst, float wValue = 1.0f )
	{
		this->m_Storage.PutInt( CBCMD_STORE_EYE_POS_IN_PSCONST );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.PutFloat( wValue );
	}

	FORCEINLINE void SetPixelShaderFogParams( int nReg )
	{
		this->m_Storage.PutInt( CBCMD_SETPIXELSHADERFOGPARAMS );
		this->m_Storage.PutInt( nReg );
	}

#ifndef _PS3

	FORCEINLINE void BindStandardTexture( Sampler_t nSampler, TextureBindFlags_t nBindFlags, StandardTextureId_t nTextureId )
	{
		this->m_Storage.PutInt( CBCMD_BIND_STANDARD_TEXTURE );
		this->m_Storage.PutInt( nSampler | nBindFlags );
		this->m_Storage.PutInt( nTextureId );
	}

	FORCEINLINE void BindTexture( CBaseShader *pShader, Sampler_t nSampler, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrame )
	{
		ShaderAPITextureHandle_t hTexture = pShader->GetShaderAPITextureBindHandle( pTexture, nFrame );
		Assert( hTexture != INVALID_SHADERAPI_TEXTURE_HANDLE );
		this->m_Storage.PutInt( CBCMD_BIND_SHADERAPI_TEXTURE_HANDLE );
		this->m_Storage.PutInt( nSampler | nBindFlags );
		this->m_Storage.Put( hTexture );
	}

FORCEINLINE void BindTexture( Sampler_t nSampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t hTexture )
	{
		Assert( hTexture != INVALID_SHADERAPI_TEXTURE_HANDLE );
		this->m_Storage.PutInt( CBCMD_BIND_SHADERAPI_TEXTURE_HANDLE );
		this->m_Storage.PutInt( nSampler | nBindFlags );
		this->m_Storage.Put( hTexture );
	}


#else

	FORCEINLINE void BindTexture( Sampler_t nSampler, TextureBindFlags_t nBindFlags, ShaderAPITextureHandle_t hTexture )
	{	
		Assert( hTexture != INVALID_SHADERAPI_TEXTURE_HANDLE );

		if (m_numPs3Tex >= CBCMD_MAX_PS3TEX)
		{
			Error("Too many textures in single draw ECB\n");
		}

		int* pOffset = GetPs3Textures() + m_numPs3Tex;

		CPs3BindParams_t tex;
		tex.m_sampler = nSampler;
		tex.m_nBindFlags = nBindFlags >> 24;			// Top byte only
		tex.m_hTexture = hTexture;

		tex.m_boundStd = -1;

        tex.m_nBindTexIndex = m_numPs3Tex;

		this->m_Storage.PutInt( CBCMD_BIND_PS3_TEXTURE );
		*pOffset = (this->m_Storage.m_pDataOut - this->m_Storage.m_Data);	
		this->m_Storage.PutMemory(&tex, sizeof(tex)) ;

		m_numPs3Tex++;


	}

	FORCEINLINE void BindStandardTexture( Sampler_t nSampler, TextureBindFlags_t nBindFlags, StandardTextureId_t nTextureId )
	{
		if (m_numPs3Tex >= CBCMD_MAX_PS3TEX)
		{
			Error("Too many textures in single draw ECB\n");
		}

		int* pOffset = GetPs3Textures() + m_numPs3Tex;
		
		CPs3BindParams_t tex;
		tex.m_sampler = nSampler;
		tex.m_nBindFlags = nBindFlags >> 24;
		tex.m_boundStd = nTextureId;

		tex.m_hTexture = -1;

        tex.m_nBindTexIndex = m_numPs3Tex;


		this->m_Storage.PutInt( CBCMD_BIND_PS3_STANDARD_TEXTURE );
		*pOffset = (this->m_Storage.m_pDataOut - this->m_Storage.m_Data);
		this->m_Storage.PutMemory(&tex, sizeof(tex)) ;
		
		m_numPs3Tex++;
	}

	FORCEINLINE void BindTexture( CBaseShader *pShader, Sampler_t nSampler, TextureBindFlags_t nBindFlags, ITexture *pTexture, int nFrame )
	{
		ShaderAPITextureHandle_t hTexture = pShader->GetShaderAPITextureBindHandle( pTexture, nFrame );
		BindTexture(nSampler, nBindFlags, hTexture);
	}

#endif

	FORCEINLINE void BindTexture( CBaseShader *pShader, Sampler_t nSampler, TextureBindFlags_t nBindFlags, int nTextureVar, int nFrameVar = -1 )
	{
		ShaderAPITextureHandle_t hTexture = pShader->GetShaderAPITextureBindHandle( nTextureVar, nFrameVar );
		BindTexture( nSampler, nBindFlags, hTexture );
	}

	// Same as BindTexture, except it checks to see if the texture handle is actually the "internal" env_cubemap. If so, it binds it as a standard texture so the proper texture bind flags are 
	// recorded during instance rendering in CShaderAPIDX8.
	FORCEINLINE void BindEnvCubemapTexture( CBaseShader *pShader, Sampler_t nSampler, TextureBindFlags_t nBindFlags, int nTextureVar, int nFrameVar = -1 )
	{
		Assert( nTextureVar != -1 );
		Assert( CBaseShader::GetPPParams() );
		if ( CBaseShader::GetPPParams()[nTextureVar]->IsTextureValueInternalEnvCubemap() )
		{
			BindStandardTexture( nSampler, nBindFlags, TEXTURE_LOCAL_ENV_CUBEMAP );
		}
		else
		{
			ShaderAPITextureHandle_t hTexture = pShader->GetShaderAPITextureBindHandle( nTextureVar, nFrameVar );
			BindTexture( nSampler, nBindFlags, hTexture );
		}
	}

	FORCEINLINE void BindMultiTexture( CBaseShader *pShader, Sampler_t nSampler1, Sampler_t nSampler2, TextureBindFlags_t nBindFlags, int nTextureVar, int nFrameVar )
	{
		ShaderAPITextureHandle_t hTexture = pShader->GetShaderAPITextureBindHandle( nTextureVar, nFrameVar, 0 );
		BindTexture( nSampler1, nBindFlags, hTexture );
		hTexture = pShader->GetShaderAPITextureBindHandle( nTextureVar, nFrameVar, 1 );
		BindTexture( nSampler2, nBindFlags, hTexture );
	}

	FORCEINLINE void SetPixelShaderIndex( int nIndex )
	{
		this->m_Storage.PutInt( CBCMD_SET_PSHINDEX );
		this->m_Storage.PutInt( nIndex );
	}

	FORCEINLINE void SetVertexShaderIndex( int nIndex )
	{
		this->m_Storage.PutInt( CBCMD_SET_VSHINDEX );
		this->m_Storage.PutInt( nIndex );
	}

	FORCEINLINE void SetDepthFeatheringShaderConstants( int iConstant, float fDepthBlendScale )
	{
		this->m_Storage.PutInt( CBCMD_SET_DEPTH_FEATHERING_CONST );
		this->m_Storage.PutInt( iConstant );
		this->m_Storage.PutFloat( fDepthBlendScale );
	}

	FORCEINLINE void SetVertexShaderFlashlightState( int iConstant )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_FLASHLIGHT_STATE );
		this->m_Storage.PutInt( iConstant );
	}

	FORCEINLINE void SetPixelShaderFlashlightState( const CBCmdSetPixelShaderFlashlightState_t &state )
	{
		this->m_Storage.PutInt( CBCMD_SET_PIXEL_SHADER_FLASHLIGHT_STATE );
		this->m_Storage.PutInt( state.m_LightSampler );
		this->m_Storage.PutInt( state.m_DepthSampler );
		this->m_Storage.PutInt( state.m_ShadowNoiseSampler );
		this->m_Storage.PutInt( state.m_nColorConstant );
		this->m_Storage.PutInt( state.m_nAttenConstant );
		this->m_Storage.PutInt( state.m_nOriginConstant );
		this->m_Storage.PutInt( state.m_nDepthTweakConstant );
		this->m_Storage.PutInt( state.m_nScreenScaleConstant );
		this->m_Storage.PutInt( state.m_nWorldToTextureConstant );
		this->m_Storage.PutInt( state.m_bFlashlightNoLambert );
		this->m_Storage.PutInt( state.m_bSinglePassFlashlight );
	}

	FORCEINLINE void SetPixelShaderUberLightState( int iEdge0Const, int iEdge1Const, int iEdgeOOWConst, int iShearRoundConst, int iAABBConst, int iWorldToLightConst )
	{
		this->m_Storage.PutInt( CBCMD_SET_PIXEL_SHADER_UBERLIGHT_STATE );
		this->m_Storage.PutInt( iEdge0Const );
		this->m_Storage.PutInt( iEdge1Const );
		this->m_Storage.PutInt( iEdgeOOWConst );
		this->m_Storage.PutInt( iShearRoundConst );
		this->m_Storage.PutInt( iAABBConst );
		this->m_Storage.PutInt( iWorldToLightConst );
	}

	FORCEINLINE void Goto( uint8 *pCmdBuf )
	{
		this->m_Storage.PutInt( CBCMD_JUMP );
		this->m_Storage.PutPtr( pCmdBuf );
	}

	FORCEINLINE void Call( uint8 *pCmdBuf )
	{
		this->m_Storage.PutInt( CBCMD_JSR );
		this->m_Storage.PutPtr( pCmdBuf );
	}

#ifndef _PS3
	FORCEINLINE void Reset( void )
	{
		this->m_Storage.Reset();
	}
#endif

	FORCEINLINE size_t Size( void ) const
	{
		return this->m_Storage.Size();
	}

	FORCEINLINE uint8 *Base( void )
	{
		return this->m_Storage.Base();
	}

	FORCEINLINE void SetVertexShaderNearAndFarZ( int iRegNum )
	{
		this->m_Storage.PutInt( CBCMD_SET_VERTEX_SHADER_NEARZFARZ_STATE );
		this->m_Storage.PutInt( iRegNum );
	}
};


//-----------------------------------------------------------------------------
// Builds a command buffer specifically for per-instance state setting
//-----------------------------------------------------------------------------
template<class S> class CInstanceCommandBufferBuilder : public CBaseCommandBufferBuilder< S >
{
	typedef CBaseCommandBufferBuilder< S > PARENT;

public:
	FORCEINLINE void End( void )
	{
		this->m_Storage.PutInt( CBICMD_END );
	}

	FORCEINLINE void SetPixelShaderLocalLighting( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETPIXELSHADERLOCALLIGHTING );
		this->m_Storage.PutInt( nConst );
	}

	FORCEINLINE void SetPixelShaderAmbientLightCube( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBE );
		this->m_Storage.PutInt( nConst );
	}

	FORCEINLINE void SetVertexShaderLocalLighting( )
	{
		this->m_Storage.PutInt( CBICMD_SETVERTEXSHADERLOCALLIGHTING );
	}

	FORCEINLINE void SetVertexShaderAmbientLightCube( void )
	{
		this->m_Storage.PutInt( CBICMD_SETVERTEXSHADERAMBIENTLIGHTCUBE );
	}

	FORCEINLINE void SetSkinningMatrices( void )
	{
		this->m_Storage.PutInt( CBICMD_SETSKINNINGMATRICES );
	}

	FORCEINLINE void SetPixelShaderAmbientLightCubeLuminance( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBELUMINANCE );
		this->m_Storage.PutInt( nConst );
	}

	FORCEINLINE void SetPixelShaderGlintDamping( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETPIXELSHADERGLINTDAMPING );
		this->m_Storage.PutInt( nConst );
	}

	FORCEINLINE void SetModulationPixelShaderDynamicState_LinearColorSpace_LinearScale( int nConst, const Vector &vecGammaSpaceColor2Factor, float scale )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE_LINEARSCALE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( 1.0 );					// pad for vector4
		this->m_Storage.PutFloat( scale );
	}

	FORCEINLINE void SetModulationPixelShaderDynamicState_LinearScale( int nConst, const Vector &vecGammaSpaceColor2Factor, float scale )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( 1.0 );					// alpha modulation wants 1 1.0 here for simd
		this->m_Storage.PutFloat( scale );
	}

	FORCEINLINE void SetModulationPixelShaderDynamicState_LinearScale_ScaleInW( int nConst, const Vector &vecGammaSpaceColor2Factor, float scale )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE_SCALEINW );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( scale );
	}

	FORCEINLINE void SetModulationPixelShaderDynamicState_LinearColorSpace( int nConst, const Vector &vecGammaSpaceColor2Factor )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( 1.0 );						// pad with a 1 for vector4d simd access. Important that this be a 1 because alpha is multipled by it.
	}

	FORCEINLINE void SetModulationPixelShaderDynamicState( int nConst, const Vector &vecGammaSpaceColor2Factor )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
	}

	FORCEINLINE void SetModulationPixelShaderDynamicState_Identity( int nConst )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_IDENTITY );
		this->m_Storage.PutInt( nConst );
	}

	FORCEINLINE void SetModulationVertexShaderDynamicState( int nConst, const Vector &vecGammaSpaceColor2Factor )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
	}
	
	FORCEINLINE void SetModulationVertexShaderDynamicState_LinearScale( int nConst, const Vector &vecGammaSpaceColor2Factor, float flScale )
	{
		this->m_Storage.PutInt( CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE_LINEARSCALE );
		this->m_Storage.PutInt( nConst );
		this->m_Storage.Put( vecGammaSpaceColor2Factor );
		this->m_Storage.PutFloat( flScale );
	}
};


#endif // commandbuilder_h
