//==== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======//
//
// Purpose: A color format which works on 360 + PC
//
//===========================================================================//

#ifndef VERTEXCOLOR_H
#define VERTEXCOLOR_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "tier0/platform.h"


//-----------------------------------------------------------------------------
// The challenge here is to make a color struct that works both on the 360
// and PC, since the 360 is big-endian vs the PC which is little endian.
//-----------------------------------------------------------------------------
struct VertexColor_t
{
	// NOTE: This constructor is explicitly here to disallow initializers
	// with initializer lists i.e. 
	//     VertexColor_t color = { 255,   0,   0, 255 };
	// which will totally fail on the 360.
	VertexColor_t() {};
	VertexColor_t( const VertexColor_t &src );
	VertexColor_t( uint8 ir, uint8 ig, uint8 ib, uint8 ia );

	// assign and copy by using the whole register rather than byte-by-byte copy. 
	// (No, the compiler is not smart enough to do this for you. /FAcs if you 
	// don't believe me.)
	uint32 AsUint32() const; 
	uint32 *AsUint32Ptr();
	const uint32 *AsUint32Ptr() const; 

	// assignment
	VertexColor_t &operator=( const VertexColor_t &src );
	VertexColor_t &operator=( const color32 &src );

	// comparison
	bool operator==( const VertexColor_t &src ) const;
	bool operator!=( const VertexColor_t &src ) const;

#ifdef PLATFORM_X360
	// 360 is little endian
	uint8 a, b, g, r;
#else
	uint8 r, g, b, a;
#endif
};


//-----------------------------------------------------------------------------
// Constructors
//-----------------------------------------------------------------------------
inline VertexColor_t::VertexColor_t( const VertexColor_t &src )
{
	*AsUint32Ptr() = src.AsUint32();
}

inline VertexColor_t::VertexColor_t( uint8 ir, uint8 ig, uint8 ib, uint8 ia ) : r(ir), g(ig), b(ib), a(ia)
{
}


//-----------------------------------------------------------------------------
// Cast to int
//-----------------------------------------------------------------------------
inline uint32 VertexColor_t::AsUint32() const
{ 
	return *reinterpret_cast<const uint32*>( this ); 
}

inline uint32 *VertexColor_t::AsUint32Ptr() 
{ 
	return reinterpret_cast<uint32*>( this ); 
}

inline const uint32 *VertexColor_t::AsUint32Ptr() const 
{ 
	return reinterpret_cast<const uint32*>( this ); 
} 


//-----------------------------------------------------------------------------
// assignment
//-----------------------------------------------------------------------------
inline VertexColor_t &VertexColor_t::operator=( const VertexColor_t &src )
{
	*AsUint32Ptr() = src.AsUint32();
	return *this;
}

inline VertexColor_t &VertexColor_t::operator=( const color32 &src )
{
	r = src.r;
	g = src.g;
	b = src.b;
	a = src.a;
	return *this;
}


//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------
inline bool VertexColor_t::operator==( const VertexColor_t &src ) const
{
	return AsUint32() == src.AsUint32();
}

inline bool VertexColor_t::operator!=( const VertexColor_t &src ) const
{
	return AsUint32() != src.AsUint32();
}



#endif // VERTEXCOLOR_H
