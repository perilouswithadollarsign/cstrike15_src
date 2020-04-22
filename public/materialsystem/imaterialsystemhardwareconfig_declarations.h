//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//===========================================================================//

#ifndef IMATERIALSYSTEMHARDWARECONFIG_DECLARATIONS_H
#define IMATERIALSYSTEMHARDWARECONFIG_DECLARATIONS_H

#ifdef _WIN32
#pragma once
#endif

// HDRFIXME NOTE: must match common_ps_fxc.h
enum HDRType_t
{
	HDR_TYPE_NONE,
	HDR_TYPE_INTEGER,
	HDR_TYPE_FLOAT,
};



enum VertexCompressionType_t
{
	// This indicates an uninitialized VertexCompressionType_t value
	VERTEX_COMPRESSION_INVALID = 0xFFFFFFFF,

	// 'VERTEX_COMPRESSION_NONE' means that no elements of a vertex are compressed
	VERTEX_COMPRESSION_NONE = 0,

	// Currently (more stuff may be added as needed), 'VERTEX_COMPRESSION_FULL' means:
	//  - if a vertex contains VERTEX_ELEMENT_NORMAL, this is compressed
	//    (see CVertexBuilder::CompressedNormal3f)
	//  - if a vertex contains VERTEX_ELEMENT_USERDATA4 (and a normal - together defining a tangent
	//    frame, with the binormal reconstructed in the vertex shader), this is compressed
	//    (see CVertexBuilder::CompressedUserData)
	//  - if a vertex contains VERTEX_ELEMENT_BONEWEIGHTSx, this is compressed
	//    (see CVertexBuilder::CompressedBoneWeight3fv)
	//  - if a vertex contains VERTEX_ELEMENT_TEXCOORD2D_0, this is compressed
	//    (see CVertexBuilder::CompressedTexCoord2fv)
	VERTEX_COMPRESSION_FULL = (1 << 0),
	VERTEX_COMPRESSION_ON = VERTEX_COMPRESSION_FULL,
	// VERTEX_COMPRESSION_NOUV is the same as VERTEX_COMPRESSION_FULL, but does not compress
	// texture coordinates. Some assets use very large texture coordinates, so these cannot be
	// compressed - but the rest of the vertex data can be.
	VERTEX_COMPRESSION_NOUV = (1 << 1),

	VERTEX_COMPRESSION_MASK = ( VERTEX_COMPRESSION_FULL |
	VERTEX_COMPRESSION_NOUV ),
};



#endif // IMATERIALSYSTEMHARDWARECONFIG_H
