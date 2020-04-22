//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#undef PROTECTED_THINGS_ENABLE
#include "vertexdecl.h" // this includes <windows.h> inside the dx headers
#define PROTECTED_THINGS_ENABLE
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "shaderapidx8_global.h"
#include "tier0/dbg.h"
#include "utlrbtree.h"
#include "recording.h"
#include "tier1/strtools.h"
#include "tier0/vprof.h"
#include "materialsystem/imesh.h"
#include "shaderdevicedx8.h"
#include "convar.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Computes the DX8 vertex specification
//-----------------------------------------------------------------------------
static const char *DeclTypeToString( BYTE type )
{
	switch( type )
	{
	case D3DDECLTYPE_FLOAT1:
		return "D3DDECLTYPE_FLOAT1";
	case D3DDECLTYPE_FLOAT2:
		return "D3DDECLTYPE_FLOAT2";
	case D3DDECLTYPE_FLOAT3:
		return "D3DDECLTYPE_FLOAT3";
	case D3DDECLTYPE_FLOAT4:
		return "D3DDECLTYPE_FLOAT4";
	case D3DDECLTYPE_D3DCOLOR:
		return "D3DDECLTYPE_D3DCOLOR";
	case D3DDECLTYPE_UBYTE4:
		return "D3DDECLTYPE_UBYTE4";
	case D3DDECLTYPE_SHORT2:
		return "D3DDECLTYPE_SHORT2";
	case D3DDECLTYPE_SHORT4:
		return "D3DDECLTYPE_SHORT4";
	case D3DDECLTYPE_UBYTE4N:
		return "D3DDECLTYPE_UBYTE4N";
	case D3DDECLTYPE_SHORT2N:
		return "D3DDECLTYPE_SHORT2N";
	case D3DDECLTYPE_SHORT4N:
		return "D3DDECLTYPE_SHORT4N";
	case D3DDECLTYPE_USHORT2N:
		return "D3DDECLTYPE_USHORT2N";
	case D3DDECLTYPE_USHORT4N:
		return "D3DDECLTYPE_USHORT4N";
	case D3DDECLTYPE_UDEC3:
		return "D3DDECLTYPE_UDEC3";
	case D3DDECLTYPE_DEC3N:
		return "D3DDECLTYPE_DEC3N";
	case D3DDECLTYPE_FLOAT16_2:
		return "D3DDECLTYPE_FLOAT16_2";
	case D3DDECLTYPE_FLOAT16_4:
		return "D3DDECLTYPE_FLOAT16_4";
	default:
		Assert( 0 );
		return "ERROR";
	}
}

static const char *DeclMethodToString( BYTE method )
{
	switch( method )
	{
	case D3DDECLMETHOD_DEFAULT:
		return "D3DDECLMETHOD_DEFAULT";
	case D3DDECLMETHOD_PARTIALU:
		return "D3DDECLMETHOD_PARTIALU";
	case D3DDECLMETHOD_PARTIALV:
		return "D3DDECLMETHOD_PARTIALV";
	case D3DDECLMETHOD_CROSSUV:
		return "D3DDECLMETHOD_CROSSUV";
	case D3DDECLMETHOD_UV:
		return "D3DDECLMETHOD_UV";
	case D3DDECLMETHOD_LOOKUP:
		return "D3DDECLMETHOD_LOOKUP";
	case D3DDECLMETHOD_LOOKUPPRESAMPLED:
		return "D3DDECLMETHOD_LOOKUPPRESAMPLED";
	default:
		Assert( 0 );
		return "ERROR";
	}
}

static const char *DeclUsageToString( BYTE usage )
{
	switch( usage )
	{
	case D3DDECLUSAGE_POSITION:
		return "D3DDECLUSAGE_POSITION";
	case D3DDECLUSAGE_BLENDWEIGHT:
		return "D3DDECLUSAGE_BLENDWEIGHT";
	case D3DDECLUSAGE_BLENDINDICES:
		return "D3DDECLUSAGE_BLENDINDICES";
	case D3DDECLUSAGE_NORMAL:
		return "D3DDECLUSAGE_NORMAL";
	case D3DDECLUSAGE_PSIZE:
		return "D3DDECLUSAGE_PSIZE";
	case D3DDECLUSAGE_COLOR:
		return "D3DDECLUSAGE_COLOR";
	case D3DDECLUSAGE_TEXCOORD:
		return "D3DDECLUSAGE_TEXCOORD";
	case D3DDECLUSAGE_TANGENT:
		return "D3DDECLUSAGE_TANGENT";
	case D3DDECLUSAGE_BINORMAL:
		return "D3DDECLUSAGE_BINORMAL";
	case D3DDECLUSAGE_TESSFACTOR:
		return "D3DDECLUSAGE_TESSFACTOR";
//	case D3DDECLUSAGE_POSITIONTL:
//		return "D3DDECLUSAGE_POSITIONTL";
	default:
		Assert( 0 );
		return "ERROR";
	}
}

static D3DDECLTYPE VertexElementToDeclType( VertexElement_t element, VertexCompressionType_t compressionType )
{
	Detect_VertexElement_t_Changes( element );

	if ( compressionType == VERTEX_COMPRESSION_ON )
	{
		// Compressed-vertex element sizes
		switch ( element )
		{
#if		( COMPRESSED_NORMALS_TYPE == COMPRESSED_NORMALS_SEPARATETANGENTS_SHORT2 )
			case VERTEX_ELEMENT_NORMAL:			return D3DDECLTYPE_SHORT2;
			case VERTEX_ELEMENT_USERDATA4:		return D3DDECLTYPE_SHORT2;
#else //( COMPRESSED_NORMALS_TYPE == COMPRESSED_NORMALS_COMBINEDTANGENTS_UBYTE4 ) 
			case VERTEX_ELEMENT_NORMAL:			return D3DDECLTYPE_UBYTE4;
			case VERTEX_ELEMENT_USERDATA4:		return D3DDECLTYPE_UBYTE4;
#endif
			case VERTEX_ELEMENT_BONEWEIGHTS1:	return D3DDECLTYPE_SHORT2;
			case VERTEX_ELEMENT_BONEWEIGHTS2:	return D3DDECLTYPE_SHORT2;
			default:
				break;
		}
	}

	// Uncompressed-vertex element sizes
	switch ( element )
	{
		case VERTEX_ELEMENT_POSITION:		return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_POSITION4D:		return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_NORMAL:			return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_NORMAL4D:		return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_COLOR:			return D3DDECLTYPE_D3DCOLOR;
		case VERTEX_ELEMENT_SPECULAR:		return D3DDECLTYPE_D3DCOLOR;
		case VERTEX_ELEMENT_TANGENT_S:		return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TANGENT_T:		return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_WRINKLE:
			// Wrinkle is packed into Position.W, it is not specified as a separate vertex element
			Assert( 0 );
			return D3DDECLTYPE_UNUSED;
#if !defined( _X360 )
		case VERTEX_ELEMENT_BONEINDEX:		return D3DDECLTYPE_D3DCOLOR;
#else
		// UBYTE4 comes in as [0,255] in the shader, which is ideal for bone indices
		// (unfortunately, UBYTE4 is not universally supported on PC DX8 GPUs)
		case VERTEX_ELEMENT_BONEINDEX:		return D3DDECLTYPE_UBYTE4;
#endif
		case VERTEX_ELEMENT_BONEWEIGHTS1:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_BONEWEIGHTS2:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_BONEWEIGHTS3:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_BONEWEIGHTS4:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_USERDATA1:		return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_USERDATA2:		return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_USERDATA3:		return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_USERDATA4:		return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD1D_0:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_1:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_2:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_3:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_4:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_5:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_6:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD1D_7:	return D3DDECLTYPE_FLOAT1;
		case VERTEX_ELEMENT_TEXCOORD2D_0:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_1:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_2:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_3:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_4:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_5:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_6:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD2D_7:	return D3DDECLTYPE_FLOAT2;
		case VERTEX_ELEMENT_TEXCOORD3D_0:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_1:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_2:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_3:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_4:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_5:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_6:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD3D_7:	return D3DDECLTYPE_FLOAT3;
		case VERTEX_ELEMENT_TEXCOORD4D_0:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_1:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_2:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_3:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_4:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_5:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_6:	return D3DDECLTYPE_FLOAT4;
		case VERTEX_ELEMENT_TEXCOORD4D_7:	return D3DDECLTYPE_FLOAT4;
		default:
			Assert(0);
			return D3DDECLTYPE_UNUSED;
	};
}

void PrintVertexDeclaration( const D3DVERTEXELEMENT9 *pDecl )
{
	int i;
	static D3DVERTEXELEMENT9 declEnd = D3DDECL_END();
	for ( i = 0; ; i++ )
	{
		if ( memcmp( &pDecl[i], &declEnd, sizeof( declEnd ) ) == 0 )
		{
			Warning( "D3DDECL_END\n" );
			break;
		}
		Msg( "%d: Stream: %d, Offset: %d, Type: %s, Method: %s, Usage: %s, UsageIndex: %d\n",
			i, ( int )pDecl[i].Stream, ( int )pDecl[i].Offset,
			DeclTypeToString( pDecl[i].Type ),
			DeclMethodToString( pDecl[i].Method ),
			DeclUsageToString( pDecl[i].Usage ),
			( int )pDecl[i].UsageIndex );
	}
}

//-----------------------------------------------------------------------------
// Converts format to a vertex decl
//-----------------------------------------------------------------------------
void ComputeVertexSpec( VertexFormat_t fmt, D3DVERTEXELEMENT9 *pDecl,
					    bool bStaticLit, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch,
					    VertexStreamSpec_t *pStreamSpec )
{
	int i = 0;
	int iStream;
	int offset;

	VertexCompressionType_t compressionType = CompressionType( fmt );

	if ( IsX360() )
	{
		// On 360, there's a performance penalty for reading more than 2 streams in the vertex shader
		// (we don't do this yet, but we should be aware if we start doing it)

		// As an extra clarification - perf difference is only observed in bandwidth-bound cases,
		// so it is safe to use extra streams as long as we aren't bandwidth-bound,
		// the assertion is safe to be removed.

#ifdef _DEBUG
		int numStreams = 1 + ( bStaticLit ? 1 : 0 ) + ( bUsingFlex ? 1 : 0 ) + ( bUsingMorph ? 1 : 0 );
		numStreams;
		// Assert( numStreams <= 2 );
#endif
	}

	if( bUsingPreTessPatch )
	{
		// Special case for Pre-Tessellated Patches
		// Ignore all of the inputs and create a very custom vertex declaration

		// patch stream
		pDecl[i].Stream = 0;
		pDecl[i].Offset = 0;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 0;
		pDecl[i].Type = D3DDECLTYPE_FLOAT2;
		i ++;
		pDecl[i].Stream = 0;
		pDecl[i].Offset = 8;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 0;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		pDecl[i].Stream = 0;
		pDecl[i].Offset = 24;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 1;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		// Quad stream
		//0
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 0;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 1;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 16;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 2;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 32;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 3;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		//1
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 48;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 2;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 64;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 4;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 80;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 5;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		//2
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 96;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 3;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 112;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 6;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 128;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 7;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		//3
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 144;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 4;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset =160;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 8;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_SUBDQUADS;
		pDecl[i].Offset = 176;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 9;
		pDecl[i].Type = D3DDECLTYPE_FLOAT4;
		i ++;
		// patch ID
		pDecl[i].Stream = VertexStreamSpec_t::STREAM_MORPH;
		pDecl[i].Offset = 0;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = 10;
		pDecl[i].Type = D3DDECLTYPE_FLOAT1;
		i ++;

		static D3DVERTEXELEMENT9 declEnd = D3DDECL_END();
		pDecl[i] = declEnd;

		return;
	}

	//
	// Stream 0
	//
	iStream = 0;
	offset = 0;

	if ( fmt & VERTEX_POSITION )
	{
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 0;

		// Handle 4D positions
		VertexElement_t posElement = fmt & VERTEX_FORMAT_PAD_POS_NORM ? VERTEX_ELEMENT_POSITION4D : VERTEX_ELEMENT_POSITION;

		pDecl[i].Type = VertexElementToDeclType( posElement, compressionType );
		offset += GetVertexElementSize( posElement, compressionType );
		++i;
	}

	int numBones = NumBoneWeights(fmt);
	if ( numBones > 0 )
	{
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_BLENDWEIGHT;
		pDecl[i].UsageIndex = 0;

		// Always exactly two weights
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_BONEWEIGHTS2, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_BONEWEIGHTS2, compressionType );
		++i;
	}

	if ( fmt & VERTEX_BONE_INDEX )
	{
		// this isn't FVF!!!!!
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_BLENDINDICES;
		pDecl[i].UsageIndex = 0;
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_BONEINDEX, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_BONEINDEX, compressionType );
		++i;
	}

	int normalOffset = -1;
	if ( fmt & VERTEX_NORMAL )
	{
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		normalOffset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_NORMAL;
		pDecl[i].UsageIndex = 0;

		// Handle 4D positions
		VertexElement_t normalElement = fmt & VERTEX_FORMAT_PAD_POS_NORM ? VERTEX_ELEMENT_NORMAL4D : VERTEX_ELEMENT_NORMAL;

		pDecl[i].Type = VertexElementToDeclType( normalElement, compressionType );
		offset += GetVertexElementSize( normalElement, compressionType );
		++i;
	}

	if ( fmt & VERTEX_COLOR )
	{
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_COLOR;
		pDecl[i].UsageIndex = 0;
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_COLOR, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_COLOR, compressionType );
		++i;
	}

	if ( fmt & VERTEX_SPECULAR )
	{
		Assert( !bStaticLit );
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_COLOR;
		pDecl[i].UsageIndex = 1; // SPECULAR goes in the second COLOR slot
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_SPECULAR, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_SPECULAR, compressionType );
		++i;
	}

	VertexElement_t texCoordDimensions[4] = {	VERTEX_ELEMENT_TEXCOORD1D_0,
												VERTEX_ELEMENT_TEXCOORD2D_0,
												VERTEX_ELEMENT_TEXCOORD3D_0,
												VERTEX_ELEMENT_TEXCOORD4D_0 };
	for ( int j = 0; j < VERTEX_MAX_TEXTURE_COORDINATES; ++j )
	{
		int nCoordSize = TexCoordSize( j, fmt );
		if ( nCoordSize <= 0 )
			continue;
		Assert( nCoordSize <= 4 );

		// Check if the texcoord goes as a separate stream spec
		if ( VertexStreamSpec_t *pSpecEntry = FindVertexStreamSpec( VERTEX_TEXCOORD_SIZE( j, nCoordSize ), pStreamSpec ) )
		{
			if ( pSpecEntry->iStreamSpec != VertexStreamSpec_t::STREAM_DEFAULT )
				// Special streams for TexCoordN are handled later
				continue;
		}

		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = j;
		VertexElement_t texCoordElement = (VertexElement_t)( texCoordDimensions[ nCoordSize - 1 ] + j );
		pDecl[i].Type = VertexElementToDeclType( texCoordElement, compressionType );
		offset += GetVertexElementSize( texCoordElement, compressionType );
		++i;
	}

	if ( fmt & VERTEX_TANGENT_S )
	{
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TANGENT;
		pDecl[i].UsageIndex = 0;
		// NOTE: this is currently *not* compressed
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_TANGENT_S, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_TANGENT_S, compressionType );
		++i;
	}

	if ( fmt & VERTEX_TANGENT_T )
	{
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage =   D3DDECLUSAGE_BINORMAL;
		pDecl[i].UsageIndex = 0;
		// NOTE: this is currently *not* compressed
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_TANGENT_T, compressionType );
		offset += GetVertexElementSize( VERTEX_ELEMENT_TANGENT_T, compressionType );
		++i;
	}

	int userDataSize = UserDataSize(fmt);
	if ( userDataSize > 0 )
	{
		Assert( userDataSize == 4 ); // This is actually only ever used for tangents
		pDecl[i].Stream = iStream;
		if ( ( compressionType == VERTEX_COMPRESSION_ON ) &&
			 ( COMPRESSED_NORMALS_TYPE == COMPRESSED_NORMALS_COMBINEDTANGENTS_UBYTE4 ) )
		{
			// FIXME: Normals and tangents are packed together into a single UBYTE4 element,
			//        so just point this back at the same data while we're testing UBYTE4 out.
			pDecl[i].Offset = normalOffset;
		}
		else
		{
			pDecl[i].Offset = offset;
		}
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TANGENT;
		pDecl[i].UsageIndex = 0;
		VertexElement_t userDataElement = (VertexElement_t)( VERTEX_ELEMENT_USERDATA1 + ( userDataSize - 1 ) );
		pDecl[i].Type = VertexElementToDeclType( userDataElement, compressionType );
		offset += GetVertexElementSize( userDataElement, compressionType );
		++i;
	}

	//
	// Stream 1
	//
	++ iStream;
	offset = 0;

	if ( bStaticLit )
	{
		// r_staticlight_streams (from engine.dll)
		static ConVarRef r_staticlight_streams( "r_staticlight_streams", true );
		int iColorSemanticSlot = 1; // Static lighting goes into COLOR1 semantic (and COLOR2, COLOR3, ...)
		int numStaticColorSamples = r_staticlight_streams.GetInt(); // NUM_BUMP_VECTS;

		for ( int iColorSample = 0; iColorSample < numStaticColorSamples; ++ iColorSample )
		{
			// force stream to have specular color in it, which is used for baked static lighting
			pDecl[i].Stream = iStream;
			pDecl[i].Offset = offset;
			pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
			pDecl[i].Usage = D3DDECLUSAGE_COLOR;
			pDecl[i].UsageIndex = iColorSemanticSlot ++;
			pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_SPECULAR, compressionType );
			offset += GetVertexElementSize( VERTEX_ELEMENT_SPECULAR, compressionType );
			++i;
		}
	}

	for ( VertexStreamSpec_t *pSpecEntry = pStreamSpec;
		  pSpecEntry && pSpecEntry->iVertexDataElement != VERTEX_FORMAT_UNKNOWN;
		  ++ pSpecEntry )
	{
		if ( pSpecEntry->iStreamSpec != VertexStreamSpec_t::STREAM_SPECULAR1 )
			continue;

		// Supporting only 2D texcoords to be passed with VHV
		int idxTexCoord = 0;
		int nCoordSize = 2;
		for ( ; idxTexCoord < VERTEX_MAX_TEXTURE_COORDINATES; ++ idxTexCoord )
		{
			if ( pSpecEntry->iVertexDataElement == VERTEX_TEXCOORD_SIZE( idxTexCoord, nCoordSize ) )
				break;
		}
		Assert( pSpecEntry->iVertexDataElement == VERTEX_TEXCOORD_SIZE( idxTexCoord, nCoordSize ) );
		if ( pSpecEntry->iVertexDataElement != VERTEX_TEXCOORD_SIZE( idxTexCoord, nCoordSize ) )
		{
			Warning( " ERROR: Cannot compute vertex spec for fmt 0x%08llX requesting 0x%08llX to be passed on STREAM_SPECULAR1!\n",
					 fmt, pSpecEntry->iVertexDataElement);
		}

		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
		pDecl[i].UsageIndex = idxTexCoord;
		VertexElement_t texCoordElement = (VertexElement_t)( texCoordDimensions[ nCoordSize - 1 ] + idxTexCoord );
		pDecl[i].Type = VertexElementToDeclType( texCoordElement, compressionType );
		offset += GetVertexElementSize( texCoordElement, compressionType );
		++i;
	}

	//
	// Stream 2
	//
	++ iStream;
	offset = 0;

	{
		// FIXME: There needs to be a better way of doing this
		// In 2.0b, assume position is 4d, storing wrinkle in pos.w.
		bool bUseWrinkle = bUsingFlex && ( HardwareConfig()->GetDXSupportLevel() >= 92 );

		// Force stream 2 to have flex deltas in it
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 1;
		// FIXME: unify this with VertexElementToDeclType():
		pDecl[i].Type = bUseWrinkle ? D3DDECLTYPE_FLOAT4 : D3DDECLTYPE_FLOAT3;
		++i;

		int normalOffset = GetVertexElementSize( VERTEX_ELEMENT_POSITION, compressionType );
		if ( bUseWrinkle )
		{
			normalOffset += GetVertexElementSize( VERTEX_ELEMENT_WRINKLE, compressionType );
		}

		// Normal deltas
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = normalOffset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_NORMAL;
		pDecl[i].UsageIndex = 1;
		// NOTE: this is currently *not* compressed
		// If we are using vertex compression and not actually morphing (ie. binding the primary stream again for the morph channel), then make sure we are setting the vertexdecl up for the compression format so that we don't get 0 * NaN in the vertex shader and get badness.
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_NORMAL, bUsingFlex ? VERTEX_COMPRESSION_NONE : compressionType );
		++i;
	}

	//
	// Stream 3
	//
	++ iStream;
	offset = 0;

	if ( bUsingMorph )
	{
		// force stream 3 to have vertex index in it, which is used for doing vertex texture reads
		pDecl[i].Stream = iStream;
		pDecl[i].Offset = offset;
		pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
		pDecl[i].Usage = D3DDECLUSAGE_POSITION;
		pDecl[i].UsageIndex = 2;
		pDecl[i].Type = VertexElementToDeclType( VERTEX_ELEMENT_USERDATA1, compressionType );
		++i;
	}

	//
	// Unique Streams
	//
	for ( int iUniqueStreamSpec = VertexStreamSpec_t::STREAM_UNIQUE_A;
		  iUniqueStreamSpec < VertexStreamSpec_t::STREAM_UNIQUE_A + VertexStreamSpec_t::MAX_UNIQUE_STREAMS;
		  ++ iUniqueStreamSpec )
	{
		++ iStream;
		offset = 0;

		for ( VertexStreamSpec_t *pSpecEntry = pStreamSpec;
			  pSpecEntry && pSpecEntry->iVertexDataElement != VERTEX_FORMAT_UNKNOWN;
			  ++ pSpecEntry )
		{
			if ( pSpecEntry->iStreamSpec != iUniqueStreamSpec )
				continue;

			// Supporting only 2D texcoords to be passed as unique streams
			int idxTexCoord = 0;
			int nCoordSize = 2;
			for ( ; idxTexCoord < VERTEX_MAX_TEXTURE_COORDINATES; ++ idxTexCoord )
			{
				if ( pSpecEntry->iVertexDataElement == VERTEX_TEXCOORD_SIZE( idxTexCoord, nCoordSize ) )
					break;
			}
			Assert( pSpecEntry->iVertexDataElement == VERTEX_TEXCOORD_SIZE( idxTexCoord, nCoordSize ) );
			if ( pSpecEntry->iVertexDataElement != VERTEX_TEXCOORD_SIZE( idxTexCoord, nCoordSize ) )
			{
				Warning( " ERROR: Cannot compute vertex spec for fmt 0x%08llX requesting 0x%08llX to be passed on STREAM%d!\n",
						 fmt, pSpecEntry->iVertexDataElement, iUniqueStreamSpec );
			}

			pDecl[i].Stream = iStream;
			pDecl[i].Offset = offset;
			pDecl[i].Method = D3DDECLMETHOD_DEFAULT;
			pDecl[i].Usage = D3DDECLUSAGE_TEXCOORD;
			pDecl[i].UsageIndex = idxTexCoord;
			VertexElement_t texCoordElement = (VertexElement_t)( texCoordDimensions[ nCoordSize - 1 ] + idxTexCoord );
			pDecl[i].Type = VertexElementToDeclType( texCoordElement, compressionType );
			offset += GetVertexElementSize( texCoordElement, compressionType );
			++i;
		}
	}

	static D3DVERTEXELEMENT9 declEnd = D3DDECL_END();
	pDecl[i] = declEnd;

	//PrintVertexDeclaration( pDecl );
}

//-----------------------------------------------------------------------------
// Gets the declspec associated with a vertex format
//-----------------------------------------------------------------------------
struct VertexDeclLookup_t
{
	enum LookupFlags_t
	{
		STATIC_LIT = 0x1,
		USING_MORPH = 0x2,
		USING_FLEX = 0x4,
		USING_PRE_TESS_PATCHES = 0x8,
		STATIC_LIT3 = 0x10,
	};

	VertexFormat_t				m_VertexFormat;
	int							m_nFlags;
	IDirect3DVertexDeclaration9 *m_pDecl;

	bool operator==( const VertexDeclLookup_t &src ) const
	{
		return ( m_VertexFormat == src.m_VertexFormat ) && ( m_nFlags == src.m_nFlags );
	}
};


//-----------------------------------------------------------------------------
// Dictionary of vertex decls
// FIXME: stick this in the class?
// FIXME: Does anything cause this to get flushed?
//-----------------------------------------------------------------------------
static bool VertexDeclLessFunc( const VertexDeclLookup_t &src1, const VertexDeclLookup_t &src2 )
{
	if ( src1.m_nFlags == src2.m_nFlags )
		return src1.m_VertexFormat < src2.m_VertexFormat;

	return ( src1.m_nFlags < src2.m_nFlags );
}

static CUtlRBTree<VertexDeclLookup_t, int> s_VertexDeclDict( 0, 256, VertexDeclLessFunc );

//-----------------------------------------------------------------------------
// Gets the declspec associated with a vertex format
//-----------------------------------------------------------------------------
IDirect3DVertexDeclaration9 *FindOrCreateVertexDecl( VertexFormat_t fmt, bool bStaticLit, bool bUsingFlex, bool bUsingMorph, bool bUsingPreTessPatch, VertexStreamSpec_t *pStreamSpec )
{
	MEM_ALLOC_D3D_CREDIT();

	VertexDeclLookup_t lookup;
	lookup.m_VertexFormat = fmt;
	lookup.m_nFlags = 0;
	if ( bStaticLit )
	{
		// r_staticlight_streams (from engine.dll)
		static ConVarRef r_staticlight_streams( "r_staticlight_streams", true );

		if ( r_staticlight_streams.GetInt() == 3 )
		{
			lookup.m_nFlags |= VertexDeclLookup_t::STATIC_LIT3;
		}
		else
		{
			lookup.m_nFlags |= VertexDeclLookup_t::STATIC_LIT;
		}
	}
	if ( bUsingMorph )
	{
		lookup.m_nFlags |= VertexDeclLookup_t::USING_MORPH;
	}
	if ( bUsingFlex )
	{
		lookup.m_nFlags |= VertexDeclLookup_t::USING_FLEX;
	}
	if( bUsingPreTessPatch )
	{
		lookup.m_nFlags |= VertexDeclLookup_t::USING_PRE_TESS_PATCHES;
	}

	int i = s_VertexDeclDict.Find( lookup );
	if ( i != s_VertexDeclDict.InvalidIndex() )
	{
		// found
		return s_VertexDeclDict[i].m_pDecl;
	}

	D3DVERTEXELEMENT9 decl[32];
	ComputeVertexSpec( fmt, decl, bStaticLit, bUsingFlex, bUsingMorph, bUsingPreTessPatch, pStreamSpec );

	HRESULT hr = 
		Dx9Device()->CreateVertexDeclaration( decl, &lookup.m_pDecl );

	// NOTE: can't record until we have m_pDecl!
	RECORD_COMMAND( DX8_CREATE_VERTEX_DECLARATION, 2 );
	RECORD_INT( ( int )lookup.m_pDecl );
	RECORD_STRUCT( decl, sizeof( decl ) );
	COMPILE_TIME_ASSERT( sizeof( decl ) == sizeof( D3DVERTEXELEMENT9 ) * 32 );

	Assert( hr == D3D_OK );
	if ( hr != D3D_OK )
	{
		Warning( " ERROR: failed to create vertex decl for vertex format %x! You'll probably see messed-up mesh rendering - to diagnose, build shaderapidx9.dll in debug.\n", (int)fmt );
	}

	s_VertexDeclDict.Insert( lookup );
	return lookup.m_pDecl;
}


//-----------------------------------------------------------------------------
// Clears out all declspecs
//-----------------------------------------------------------------------------
void ReleaseAllVertexDecl()
{
	int i = s_VertexDeclDict.FirstInorder();
	while ( i != s_VertexDeclDict.InvalidIndex() )
	{
		s_VertexDeclDict[i].m_pDecl->Release();
		i = s_VertexDeclDict.NextInorder( i );
	}
}
