//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "bitbuf.h"
#include "coordsize.h"
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"
#include "tier1/strtools.h"
#include "bitvec.h"

// FIXME: Can't use this until we get multithreaded allocations in tier0 working for tools
// This is used by VVIS and fails to link
// NOTE: This must be the last file included!!!
//#include "tier0/memdbgon.h"

#ifdef _X360
// mandatory ... wary of above comment and isolating, tier0 is built as MT though
#include "tier0/memdbgon.h"
#endif

#ifndef NDEBUG
static volatile char const *pDebugString;
#define DEBUG_LINK_CHECK pDebugString = "tier1.lib built debug!"
#else
#define DEBUG_LINK_CHECK
#endif

#if _WIN32
#define FAST_BIT_SCAN 1
#if defined( _X360 )
#define CountLeadingZeros(x) _CountLeadingZeros(x)
inline unsigned int CountTrailingZeros( unsigned int elem )
{
	// this implements CountTrailingZeros() / BitScanForward()
	unsigned int mask = elem-1;
	unsigned int comp = ~elem;
	elem = mask & comp;
	return (32 - _CountLeadingZeros(elem));
}
#else
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanForward)

inline unsigned int CountLeadingZeros(unsigned int x)
{
	unsigned long firstBit;
	if ( _BitScanReverse(&firstBit,x) )
		return 31 - firstBit;
	return 32;
}
inline unsigned int CountTrailingZeros(unsigned int elem)
{
	unsigned long out;
	if ( _BitScanForward(&out, elem) )
		return out;
	return 32;
}

#endif
#else
#define FAST_BIT_SCAN 0
#endif


static BitBufErrorHandler g_BitBufErrorHandler = 0;

inline int BitForBitnum(int bitnum)
{
	return GetBitForBitnum(bitnum);
}

void InternalBitBufErrorHandler( BitBufErrorType errorType, const char *pDebugName )
{
	if ( g_BitBufErrorHandler )
		g_BitBufErrorHandler( errorType, pDebugName );
}


void SetBitBufErrorHandler( BitBufErrorHandler fn )
{
	g_BitBufErrorHandler = fn;
}


// #define BB_PROFILING


// Precalculated bit masks for WriteUBitLong. Using these tables instead of 
// doing the calculations gives a 33% speedup in WriteUBitLong.
uint32 g_BitWriteMasks[32][33];

// (1 << i) - 1
uint32 g_ExtraMasks[32];

class CBitWriteMasksInit
{
public:
	CBitWriteMasksInit()
	{
		for( unsigned int startbit=0; startbit < 32; startbit++ )
		{
			for( unsigned int nBitsLeft=0; nBitsLeft < 33; nBitsLeft++ )
			{
				unsigned int endbit = startbit + nBitsLeft;
				g_BitWriteMasks[startbit][nBitsLeft] = BitForBitnum(startbit) - 1;
				if(endbit < 32)
					g_BitWriteMasks[startbit][nBitsLeft] |= ~(BitForBitnum(endbit) - 1);
			}
		}

		for ( unsigned int maskBit=0; maskBit < 32; maskBit++ )
			g_ExtraMasks[maskBit] = BitForBitnum(maskBit) - 1;
	}
};
CBitWriteMasksInit g_BitWriteMasksInit;


// ---------------------------------------------------------------------------------------- //
// bf_write
// ---------------------------------------------------------------------------------------- //

bf_write::bf_write()
{
	DEBUG_LINK_CHECK;
	m_pData = NULL;
	m_nDataBytes = 0;
	m_nDataBits = -1; // set to -1 so we generate overflow on any operation
	m_iCurBit = 0;
	m_bOverflow = false;
	m_bAssertOnOverflow = true;
	m_pDebugName = NULL;
}

bf_write::bf_write( const char *pDebugName, void *pData, int nBytes, int nBits )
{
	DEBUG_LINK_CHECK;
	m_bAssertOnOverflow = true;
	m_pDebugName = pDebugName;
	StartWriting( pData, nBytes, 0, nBits );
}

bf_write::bf_write( void *pData, int nBytes, int nBits )
{
	m_bAssertOnOverflow = true;
	m_pDebugName = NULL;
	StartWriting( pData, nBytes, 0, nBits );
}

void bf_write::StartWriting( void *pData, int nBytes, int iStartBit, int nBits )
{
	// Make sure it's dword aligned and padded.
	DEBUG_LINK_CHECK;
	Assert( (nBytes % 4) == 0 );
	Assert(((uintp)pData & 3) == 0);

	// The writing code will overrun the end of the buffer if it isn't dword aligned, so truncate to force alignment
	nBytes &= ~3;

	m_pData = (unsigned char*)pData;
	m_nDataBytes = nBytes;

	if ( nBits == -1 )
	{
		m_nDataBits = nBytes << 3;
	}
	else
	{
		Assert( nBits <= nBytes*8 );
		m_nDataBits = nBits;
	}

	m_iCurBit = iStartBit;
	m_bOverflow = false;
}

void bf_write::Reset()
{
	m_iCurBit = 0;
	m_bOverflow = false;
}


void bf_write::SetAssertOnOverflow( bool bAssert )
{
	m_bAssertOnOverflow = bAssert;
}


const char* bf_write::GetDebugName()
{
	return m_pDebugName;
}


void bf_write::SetDebugName( const char *pDebugName )
{
	m_pDebugName = pDebugName;
}


void bf_write::SeekToBit( int bitPos )
{
	m_iCurBit = bitPos;
}


// Sign bit comes first
void bf_write::WriteSBitLong( int data, int numbits )
{
	// Do we have a valid # of bits to encode with?
	Assert( numbits >= 1 );

	// Note: it does this wierdness here so it's bit-compatible with regular integer data in the buffer.
	// (Some old code writes direct integers right into the buffer).
	if(data < 0)
	{
#ifdef _DEBUG
	if( numbits < 32 )
	{
		// Make sure it doesn't overflow.

		if( data < 0 )
		{
			Assert( data >= -(BitForBitnum(numbits-1)) );
		}
		else
		{
			Assert( data < (BitForBitnum(numbits-1)) );
		}
	}
#endif

		WriteUBitLong( (unsigned int)(0x80000000 + data), numbits - 1, false );
		WriteOneBit( 1 );
	}
	else
	{
		WriteUBitLong((unsigned int)data, numbits - 1);
		WriteOneBit( 0 );
	}
}

#if _WIN32
inline unsigned int BitCountNeededToEncode(unsigned int data)
{
#if defined(_X360)
	return (32 - CountLeadingZeros(data+1)) - 1;
#else
	unsigned long firstBit;
	_BitScanReverse(&firstBit,data+1);
	return firstBit;
#endif
}
#endif	// _WIN32

// writes an unsigned integer with variable bit length
void bf_write::WriteUBitVar( unsigned int n )
{
	if ( n < 16 )
		WriteUBitLong( n, 6 );
	else
		if ( n < 256 )
			WriteUBitLong( ( n & 15 ) | 16 | ( ( n & ( 128 | 64 | 32 | 16 ) ) << 2 ), 10 );
		else
			if ( n < 4096 )
				WriteUBitLong( ( n & 15 ) | 32 | ( ( n & ( 2048 | 1024 | 512 | 256 | 128 | 64 | 32 | 16 ) ) << 2 ), 14 );
			else
			{
				WriteUBitLong( ( n & 15 ) | 48, 6 );
				WriteUBitLong( ( n >> 4 ), 32 - 4 );
			}
}

void bf_write::WriteVarInt32( uint32 data )
{
	// Check if align and we have room, slow path if not
	if ( (m_iCurBit & 7) == 0 && (m_iCurBit + bitbuf::kMaxVarint32Bytes * 8 ) <= m_nDataBits)
	{
		uint8 *target = ((uint8*)m_pData) + (m_iCurBit>>3);

		target[0] = static_cast<uint8>(data | 0x80);
		if ( data >= (1 << 7) )
		{
			target[1] = static_cast<uint8>((data >>  7) | 0x80);
			if ( data >= (1 << 14) )
			{
				target[2] = static_cast<uint8>((data >> 14) | 0x80);
				if ( data >= (1 << 21) )
				{
					target[3] = static_cast<uint8>((data >> 21) | 0x80);
					if ( data >= (1 << 28) )
					{
						target[4] = static_cast<uint8>(data >> 28);
						m_iCurBit += 5 * 8;
						return;
					}
					else
					{
						target[3] &= 0x7F;
						m_iCurBit += 4 * 8;
						return;
					}
				}
				else
				{
					target[2] &= 0x7F;
					m_iCurBit += 3 * 8;
					return;
				}
			}
			else
			{
				target[1] &= 0x7F;
				m_iCurBit += 2 * 8;
				return;
			}
		}
		else
		{
			target[0] &= 0x7F;
			m_iCurBit += 1 * 8;
			return;
		}
	}
	else // Slow path
	{
		while ( data > 0x7F ) 
		{
			WriteUBitLong( (data & 0x7F) | 0x80, 8 );
			data >>= 7;
		}
		WriteUBitLong( data & 0x7F, 8 );
	}
}

void bf_write::WriteVarInt64( uint64 data )
{
	// Check if align and we have room, slow path if not
	if ( (m_iCurBit & 7) == 0 && (m_iCurBit + bitbuf::kMaxVarintBytes * 8 ) <= m_nDataBits )
	{
		uint8 *target = ((uint8*)m_pData) + (m_iCurBit>>3);

		// Splitting into 32-bit pieces gives better performance on 32-bit
		// processors.
		uint32 part0 = static_cast<uint32>(data      );
		uint32 part1 = static_cast<uint32>(data >> 28);
		uint32 part2 = static_cast<uint32>(data >> 56);

		int size;

		// Here we can't really optimize for small numbers, since the data is
		// split into three parts.  Cheking for numbers < 128, for instance,
		// would require three comparisons, since you'd have to make sure part1
		// and part2 are zero.  However, if the caller is using 64-bit integers,
		// it is likely that they expect the numbers to often be very large, so
		// we probably don't want to optimize for small numbers anyway.  Thus,
		// we end up with a hardcoded binary search tree...
		if ( part2 == 0 )
		{
			if ( part1 == 0 )
			{
				if ( part0 < (1 << 14) )
				{
					if ( part0 < (1 << 7) )
					{
						size = 1; goto size1;
					}
					else
					{
						size = 2; goto size2;
					}
				}
				else
				{
					if ( part0 < (1 << 21) )
					{
						size = 3; goto size3;
					}
					else
					{
						size = 4; goto size4;
					}
				}
			}
			else
			{
				if ( part1 < (1 << 14) )
				{
					if ( part1 < (1 << 7) )
					{
						size = 5; goto size5;
					}
					else
					{
						size = 6; goto size6;
					}
				}
				else
				{
					if ( part1 < (1 << 21) )
					{
						size = 7; goto size7;
					}
					else
					{
						size = 8; goto size8;
					}
				}
			}
		}
		else
		{
			if ( part2 < (1 << 7) )
			{
				size = 9; goto size9;
			}
			else
			{
				size = 10; goto size10;
			}
		}

		AssertFatalMsg( false, "Can't get here." );

		size10: target[9] = static_cast<uint8>((part2 >>  7) | 0x80);
		size9 : target[8] = static_cast<uint8>((part2      ) | 0x80);
		size8 : target[7] = static_cast<uint8>((part1 >> 21) | 0x80);
		size7 : target[6] = static_cast<uint8>((part1 >> 14) | 0x80);
		size6 : target[5] = static_cast<uint8>((part1 >>  7) | 0x80);
		size5 : target[4] = static_cast<uint8>((part1      ) | 0x80);
		size4 : target[3] = static_cast<uint8>((part0 >> 21) | 0x80);
		size3 : target[2] = static_cast<uint8>((part0 >> 14) | 0x80);
		size2 : target[1] = static_cast<uint8>((part0 >>  7) | 0x80);
		size1 : target[0] = static_cast<uint8>((part0      ) | 0x80);

		target[size-1] &= 0x7F;
		m_iCurBit += size * 8;
	}
	else // slow path
	{
		while ( data > 0x7F ) 
		{
			WriteUBitLong( (data & 0x7F) | 0x80, 8 );
			data >>= 7;
		}
		WriteUBitLong( data & 0x7F, 8 );
	}
}

void bf_write::WriteSignedVarInt32( int32 data )
{
	WriteVarInt32( bitbuf::ZigZagEncode32( data ) );
}

void bf_write::WriteSignedVarInt64( int64 data )
{
	WriteVarInt64( bitbuf::ZigZagEncode64( data ) );
}

int	bf_write::ByteSizeVarInt32( uint32 data )
{
	int size = 1;
	while ( data > 0x7F ) {
		size++;
		data >>= 7;
	}
	return size;
}

int	bf_write::ByteSizeVarInt64( uint64 data )
{
	int size = 1;
	while ( data > 0x7F ) {
		size++;
		data >>= 7;
	}
	return size;
}

int bf_write::ByteSizeSignedVarInt32( int32 data )
{
	return ByteSizeVarInt32( bitbuf::ZigZagEncode32( data ) );
}

int bf_write::ByteSizeSignedVarInt64( int64 data )
{
	return ByteSizeVarInt64( bitbuf::ZigZagEncode64( data ) );
}

void bf_write::WriteBitLong(unsigned int data, int numbits, bool bSigned)
{
	if(bSigned)
		WriteSBitLong((int)data, numbits);
	else
		WriteUBitLong(data, numbits);
}

bool bf_write::WriteBits(const void *pInData, int nBits)
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::WriteBits" );
#endif

	unsigned char *pIn = (unsigned char*)pInData;
	int nBitsLeft = nBits;

	// Bounds checking..
	if ( (m_iCurBit+nBits) > m_nDataBits )
	{
		SetOverflowFlag();
		CallErrorHandler( BITBUFERROR_BUFFER_OVERRUN, GetDebugName() );
		return false;
	}

	// Align input to dword boundary
	while (((uintp)pIn & 3) != 0 && nBitsLeft >= 8)
	{
		WriteUBitLong( *pIn, 8, false );
		++pIn;
		nBitsLeft -= 8;
	}
	
	if ( nBitsLeft >= 32 ) 
	{
		if ( (m_iCurBit & 7) == 0 )
		{
			// current bit is byte aligned, do block copy
			int numbytes = nBitsLeft >> 3; 
			int numbits = numbytes << 3;

			Q_memcpy( m_pData+(m_iCurBit>>3), pIn, numbytes );
			pIn += numbytes;
			nBitsLeft -= numbits;
			m_iCurBit += numbits;
		}
		else 
		{
			const uint32 iBitsRight = (m_iCurBit & 31);
			Assert( iBitsRight > 0 ); // should not be aligned, otherwise it would have been handled before
			const uint32 iBitsLeft = 32 - iBitsRight; 	
			const int iBitsChanging = 32 + iBitsLeft; // how many bits are changed during one step (not necessary written meaningful)
			unsigned int iDWord = m_iCurBit >> 5;

			uint32 outWord = LoadLittleDWord( (uint32*)m_pData, iDWord );
			outWord &= g_BitWriteMasks[iBitsRight][32]; // clear rest of beginning DWORD 

			// copy in DWORD blocks
			while(nBitsLeft >= iBitsChanging )
			{
				uint32 curData = LittleDWord( *(uint32*)pIn );
				pIn += sizeof(uint32);

				outWord |= curData << iBitsRight;
				StoreLittleDWord( (uint32*)m_pData, iDWord, outWord );

				++iDWord;
				outWord = curData >> iBitsLeft;

				nBitsLeft -= 32;
				m_iCurBit += 32;
			}

			// store last word
			StoreLittleDWord( (uint32*)m_pData, iDWord, outWord );

			// write remaining DWORD 
			if( nBitsLeft >= 32 )
			{
				WriteUBitLong( LittleDWord(*((uint32*)pIn)), 32, false );
				pIn += sizeof(uint32);
				nBitsLeft -= 32;
			}
		}
	}

	// write remaining bytes
	while ( nBitsLeft >= 8 )
	{
		WriteUBitLong( *pIn, 8, false );
		++pIn;
		nBitsLeft -= 8;
	}
	
	// write remaining bits
	if ( nBitsLeft )
	{
		WriteUBitLong( *pIn, nBitsLeft, false );
	}

	return !IsOverflowed();
}

bool bf_write::WriteBitsFromBuffer( bf_read *pIn, int nBits )
{
	// This could be optimized a little by
	while ( nBits > 32 )
	{
		WriteUBitLong( pIn->ReadUBitLong( 32 ), 32 );
		nBits -= 32;
	}

	WriteUBitLong( pIn->ReadUBitLong( nBits ), nBits );
	return !IsOverflowed() && !pIn->IsOverflowed();
}


void bf_write::WriteBitAngle( float fAngle, int numbits )
{
	int d;
	unsigned int mask;
	unsigned int shift;

	shift = BitForBitnum(numbits);
	mask = shift - 1;

	d = (int)( (fAngle / 360.0) * shift );
	d &= mask;

	WriteUBitLong((unsigned int)d, numbits);
}

void bf_write::WriteBitCoordMP( const float f, EBitCoordType coordType )
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::WriteBitCoordMP" );
#endif
	bool bIntegral = ( coordType == kCW_Integral );
	bool bLowPrecision = ( coordType == kCW_LowPrecision );  

	int		signbit = (f <= -( bLowPrecision ? COORD_RESOLUTION_LOWPRECISION : COORD_RESOLUTION ));
	int		intval = (int)abs(f);
	int		fractval = bLowPrecision ? 
		( abs((int)(f*COORD_DENOMINATOR_LOWPRECISION)) & (COORD_DENOMINATOR_LOWPRECISION-1) ) :
		( abs((int)(f*COORD_DENOMINATOR)) & (COORD_DENOMINATOR-1) );


	bool    bInBounds = intval < (1 << COORD_INTEGER_BITS_MP );

	WriteOneBit( bInBounds );

	if ( bIntegral )
	{
		// Send the sign bit
		WriteOneBit( intval );
		if ( intval )
		{
			WriteOneBit( signbit );
			// Send the integer if we have one.
			// Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
			intval--;
			if ( bInBounds )
			{
				WriteUBitLong( (unsigned int)intval, COORD_INTEGER_BITS_MP );
			}
			else
			{
				WriteUBitLong( (unsigned int)intval, COORD_INTEGER_BITS );
			}
		}
	}
	else
	{
		// Send the bit flags that indicate whether we have an integer part and/or a fraction part.
		WriteOneBit( intval );
		// Send the sign bit
		WriteOneBit( signbit );

		if ( intval )
		{
			// Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
			intval--;
			if ( bInBounds )
			{
				WriteUBitLong( (unsigned int)intval, COORD_INTEGER_BITS_MP );
			}
			else
			{
				WriteUBitLong( (unsigned int)intval, COORD_INTEGER_BITS );
			}
		}
		WriteUBitLong( (unsigned int)fractval, bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS );
	}
}

void bf_write::WriteBitCellCoord( const float f, int bits, EBitCoordType coordType )
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::WriteBitCellCoord" );
#endif
	Assert( f >= 0.0f ); // cell coords can't be negative
	Assert( f < ( 1 << bits ) );

	bool bIntegral = ( coordType == kCW_Integral );
	bool bLowPrecision = ( coordType == kCW_LowPrecision );  

	int		intval = (int)abs(f);
	int		fractval = bLowPrecision ? 
		( abs((int)(f*COORD_DENOMINATOR_LOWPRECISION)) & (COORD_DENOMINATOR_LOWPRECISION-1) ) :
		( abs((int)(f*COORD_DENOMINATOR)) & (COORD_DENOMINATOR-1) );

	if ( bIntegral )
	{
		WriteUBitLong( (unsigned int)intval, bits );
	}
	else
	{
		WriteUBitLong( (unsigned int)intval, bits );
		WriteUBitLong( (unsigned int)fractval, bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS );
	}
}


void bf_write::WriteBitCoord(const float f)
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::WriteBitCoord" );
#endif
	int		signbit = (f <= -COORD_RESOLUTION);
	int		intval = (int)abs(f);
	int		fractval = abs((int)(f*COORD_DENOMINATOR)) & (COORD_DENOMINATOR-1);


	// Send the bit flags that indicate whether we have an integer part and/or a fraction part.
	WriteOneBit( intval );
	WriteOneBit( fractval );

	if ( intval || fractval )
	{
		// Send the sign bit
		WriteOneBit( signbit );

		// Send the integer if we have one.
		if ( intval )
		{
			// Adjust the integers from [1..MAX_COORD_VALUE] to [0..MAX_COORD_VALUE-1]
			intval--;
			WriteUBitLong( (unsigned int)intval, COORD_INTEGER_BITS );
		}
		
		// Send the fraction if we have one
		if ( fractval )
		{
			WriteUBitLong( (unsigned int)fractval, COORD_FRACTIONAL_BITS );
		}
	}
}

void bf_write::WriteBitFloat(float val)
{
	int32 intVal;

	Assert(sizeof(int32) == sizeof(float));
	Assert(sizeof(float) == 4);

	intVal = *((int32*)&val);
	WriteUBitLong( intVal, 32 );
}

void bf_write::WriteBitVec3Coord( const Vector& fa )
{
	int		xflag, yflag, zflag;

	xflag = (fa[0] >= COORD_RESOLUTION) || (fa[0] <= -COORD_RESOLUTION);
	yflag = (fa[1] >= COORD_RESOLUTION) || (fa[1] <= -COORD_RESOLUTION);
	zflag = (fa[2] >= COORD_RESOLUTION) || (fa[2] <= -COORD_RESOLUTION);

	WriteOneBit( xflag );
	WriteOneBit( yflag );
	WriteOneBit( zflag );

	if ( xflag )
		WriteBitCoord( fa[0] );
	if ( yflag )
		WriteBitCoord( fa[1] );
	if ( zflag )
		WriteBitCoord( fa[2] );
}

void bf_write::WriteBitNormal( float f )
{
	int	signbit = (f <= -NORMAL_RESOLUTION);

	// NOTE: Since +/-1 are valid values for a normal, I'm going to encode that as all ones
	unsigned int fractval = abs( (int)(f*NORMAL_DENOMINATOR) );

	// clamp..
	if (fractval > NORMAL_DENOMINATOR)
		fractval = NORMAL_DENOMINATOR;

	// Send the sign bit
	WriteOneBit( signbit );

	// Send the fractional component
	WriteUBitLong( fractval, NORMAL_FRACTIONAL_BITS );
}

void bf_write::WriteBitVec3Normal( const Vector& fa )
{
	int		xflag, yflag;

	xflag = (fa[0] >= NORMAL_RESOLUTION) || (fa[0] <= -NORMAL_RESOLUTION);
	yflag = (fa[1] >= NORMAL_RESOLUTION) || (fa[1] <= -NORMAL_RESOLUTION);

	WriteOneBit( xflag );
	WriteOneBit( yflag );

	if ( xflag )
		WriteBitNormal( fa[0] );
	if ( yflag )
		WriteBitNormal( fa[1] );
	
	// Write z sign bit
	int	signbit = (fa[2] <= -NORMAL_RESOLUTION);
	WriteOneBit( signbit );
}

void bf_write::WriteBitAngles( const QAngle& fa )
{
	// FIXME:
	Vector tmp( fa.x, fa.y, fa.z );
	WriteBitVec3Coord( tmp );
}

void bf_write::WriteChar(int val)
{
	WriteSBitLong(val, sizeof(char) << 3);
}

void bf_write::WriteByte( unsigned int val )
{
	WriteUBitLong(val, sizeof(unsigned char) << 3);
}

void bf_write::WriteShort(int val)
{
	WriteSBitLong(val, sizeof(short) << 3);
}

void bf_write::WriteWord( unsigned int val )
{
	WriteUBitLong(val, sizeof(unsigned short) << 3);
}

void bf_write::WriteLong(int32 val)
{
	WriteSBitLong(val, sizeof(int32) << 3);
}

void bf_write::WriteLongLong(int64 val)
{
	uint *pLongs = (uint*)&val;

	// Insert the two DWORDS according to network endian
	const short endianIndex = 0x0100;
	byte *idx = (byte*)&endianIndex;
	WriteUBitLong(pLongs[*idx++], sizeof(int32) << 3);
	WriteUBitLong(pLongs[*idx], sizeof(int32) << 3);
}

void bf_write::WriteFloat(float val)
{
	// Pre-swap the float, since WriteBits writes raw data
	LittleFloat( &val, &val );

	WriteBits(&val, sizeof(val) << 3);
}

bool bf_write::WriteBytes( const void *pBuf, int nBytes )
{
	return WriteBits(pBuf, nBytes << 3);
}

bool bf_write::WriteString(const char *pStr)
{
	if(pStr)
	{
		do
		{
			WriteChar( *pStr );
			++pStr;
		} while( *(pStr-1) != 0 );
	}
	else
	{
		WriteChar( 0 );
	}

	return !IsOverflowed();
}

bool bf_write::WriteString(const wchar_t *pStr)
{
	if(pStr)
	{
		do
		{
			WriteShort( *pStr );
			++pStr;
		} while( *(pStr-1) != 0 );
	}
	else
	{
		WriteShort( 0 );
	}

	return !IsOverflowed();
}

// ---------------------------------------------------------------------------------------- //
// old_bf_read
// ---------------------------------------------------------------------------------------- //

old_bf_read::old_bf_read()
{
	DEBUG_LINK_CHECK;
	m_pData = NULL;
	m_nDataBytes = 0;
	m_nDataBits = -1; // set to -1 so we overflow on any operation
	m_iCurBit = 0;
	m_bOverflow = false;
	m_bAssertOnOverflow = true;
	m_pDebugName = NULL;
}

old_bf_read::old_bf_read( const void *pData, int nBytes, int nBits )
{
	m_bAssertOnOverflow = true;
	StartReading( pData, nBytes, 0, nBits );
}

old_bf_read::old_bf_read( const char *pDebugName, const void *pData, int nBytes, int nBits )
{
	m_bAssertOnOverflow = true;
	m_pDebugName = pDebugName;
	StartReading( pData, nBytes, 0, nBits );
}

void old_bf_read::StartReading( const void *pData, int nBytes, int iStartBit, int nBits )
{
	// Make sure we're dword aligned.
	Assert(((uintp)pData & 3) == 0);

	m_pData = (unsigned char*)pData;
	m_nDataBytes = nBytes;

	if ( nBits == -1 )
	{
		m_nDataBits = m_nDataBytes << 3;
	}
	else
	{
		Assert( nBits <= nBytes*8 );
		m_nDataBits = nBits;
	}

	m_iCurBit = iStartBit;
	m_bOverflow = false;
}

void old_bf_read::Reset()
{
	m_iCurBit = 0;
	m_bOverflow = false;
}

void old_bf_read::SetAssertOnOverflow( bool bAssert )
{
	m_bAssertOnOverflow = bAssert;
}

const char* old_bf_read::GetDebugName()
{
	return m_pDebugName;
}

void old_bf_read::SetDebugName( const char *pName )
{
	m_pDebugName = pName;
}

unsigned int old_bf_read::CheckReadUBitLong(int numbits)
{
	// Ok, just read bits out.
	int i, nBitValue;
	unsigned int r = 0;

	for(i=0; i < numbits; i++)
	{
		nBitValue = ReadOneBitNoCheck();
		r |= nBitValue << i;
	}
	m_iCurBit -= numbits;
	
	return r;
}

void old_bf_read::ReadBits(void *pOutData, int nBits)
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::ReadBits" );
#endif

	unsigned char *pOut = (unsigned char*)pOutData;
	int nBitsLeft = nBits;

	
	// align output to dword boundary
	while( ((uintp)pOut & 3) != 0 && nBitsLeft >= 8 )
	{
		*pOut = (unsigned char)ReadUBitLong(8);
		++pOut;
		nBitsLeft -= 8;
	}

	// X360TBD: Can't read dwords in ReadBits because they'll get swapped
	if ( IsPC() )
	{
		// read dwords
		while ( nBitsLeft >= 32 )
		{
			*((uint32*)pOut) = ReadUBitLong(32);
			pOut += sizeof(uint32);
			nBitsLeft -= 32;
		}
	}

	// read remaining bytes
	while ( nBitsLeft >= 8 )
	{
		*pOut = ReadUBitLong(8);
		++pOut;
		nBitsLeft -= 8;
	}
	
	// read remaining bits
	if ( nBitsLeft )
	{
		*pOut = ReadUBitLong(nBitsLeft);
	}

}

float old_bf_read::ReadBitAngle( int numbits )
{
	float fReturn;
	int i;
	float shift;

	shift = (float)( BitForBitnum(numbits) );

	i = ReadUBitLong( numbits );
	fReturn = (float)i * (360.0 / shift);

	return fReturn;
}

unsigned int old_bf_read::PeekUBitLong( int numbits )
{
	unsigned int r;
	int i, nBitValue;
#ifdef BIT_VERBOSE
	int nShifts = numbits;
#endif

	old_bf_read savebf;

	savebf = *this;  // Save current state info

	r = 0;
	for(i=0; i < numbits; i++)
	{
		nBitValue = ReadOneBit();

		// Append to current stream
		if ( nBitValue )
		{
			r |= BitForBitnum(i);
		}
	}
	
	*this = savebf;

#ifdef BIT_VERBOSE
	Con_Printf( "PeekBitLong:  %i %i\n", nShifts, (unsigned int)r );
#endif

	return r;
}

// Append numbits least significant bits from data to the current bit stream
int old_bf_read::ReadSBitLong( int numbits )
{
	int r, sign;

	r = ReadUBitLong(numbits - 1);

	// Note: it does this wierdness here so it's bit-compatible with regular integer data in the buffer.
	// (Some old code writes direct integers right into the buffer).
	sign = ReadOneBit();
	if(sign)
		r = -((BitForBitnum(numbits-1)) - r);

	return r;
}

const byte g_BitMask[8] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80};
const byte g_TrailingMask[8] = {0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80};

inline int old_bf_read::CountRunOfZeros()
{
	int bits = 0;
	if ( m_iCurBit + 32 < m_nDataBits )
	{
#if !FAST_BIT_SCAN
		while (true)
		{
			int value = (m_pData[m_iCurBit >> 3] & g_BitMask[m_iCurBit & 7]);
			++m_iCurBit;
			if ( value )
				return bits;
			++bits;
		}
#else
		while (true)
		{
			int value = (m_pData[m_iCurBit >> 3] & g_TrailingMask[m_iCurBit & 7]);
			if ( !value )
			{
				int zeros = (8-(m_iCurBit&7));
				bits += zeros;
				m_iCurBit += zeros;
			}
			else
			{
				int zeros = CountTrailingZeros(value) - (m_iCurBit & 7);
				m_iCurBit += zeros + 1;
				bits += zeros;
				return bits;
			}
		}
#endif
	}
	else
	{
		while ( ReadOneBit() == 0 )
			bits++;
	}
	return bits;
}

unsigned int old_bf_read::ReadUBitVar()
{
	unsigned int ret = ReadUBitLong( 6 );
	switch( ret & ( 16 | 32 ) )
	{
		case 16:
			ret = ( ret & 15 ) | ( ReadUBitLong( 4 ) << 4 );
			Assert( ret >= 16);
			break;
				
		case 32:
			ret = ( ret & 15 ) | ( ReadUBitLong( 8 ) << 4 );
			Assert( ret >= 256);
			break;
		case 48:
			ret = ( ret & 15 ) | ( ReadUBitLong( 32 - 4 ) << 4 );
			Assert( ret >= 4096 );
			break;
	}
	return ret;
}


// Read 1-5 bytes in order to extract a 32-bit unsigned value from the
// stream. 7 data bits are extracted from each byte with the 8th bit used
// to indicate whether the loop should continue.
// This allows variable size numbers to be stored with tolerable
// efficiency. Numbers sizes that can be stored for various numbers of
// encoded bits are:
//  8-bits: 0-127
// 16-bits: 128-16383
// 24-bits: 16384-2097151
// 32-bits: 2097152-268435455
// 40-bits: 268435456-0xFFFFFFFF
uint32 old_bf_read::ReadVarInt32()
{
	uint32 result = 0;
	int count = 0;
	uint32 b;

	do 
	{
		if ( count == bitbuf::kMaxVarint32Bytes ) 
		{
			// If we get here it means that the fifth bit had its
			// high bit set, which implies corrupt data.
			Assert( 0 );
			return result;
		}
		b = ReadUBitLong( 8 );
		result |= (b & 0x7F) << (7 * count);
		++count;
	} while (b & 0x80);

	return result;
}

uint64 old_bf_read::ReadVarInt64()
{
	uint64 result = 0;
	int count = 0;
	uint64 b;

	do 
	{
		if ( count == bitbuf::kMaxVarintBytes ) 
		{
			return result;
		}
		b = ReadUBitLong( 8 );
		result |= static_cast<uint64>(b & 0x7F) << (7 * count);
		++count;
	} while (b & 0x80);

	return result;
}

unsigned int old_bf_read::ReadBitLong(int numbits, bool bSigned)
{
	if(bSigned)
		return (unsigned int)ReadSBitLong(numbits);
	else
		return ReadUBitLong(numbits);
}


// Basic Coordinate Routines (these contain bit-field size AND fixed point scaling constants)
float old_bf_read::ReadBitCoord (void)
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::ReadBitCoord" );
#endif
	int		intval=0,fractval=0,signbit=0;
	float	value = 0.0;


	// Read the required integer and fraction flags
	intval = ReadOneBit();
	fractval = ReadOneBit();

	// If we got either parse them, otherwise it's a zero.
	if ( intval || fractval )
	{
		// Read the sign bit
		signbit = ReadOneBit();

		// If there's an integer, read it in
		if ( intval )
		{
			// Adjust the integers from [0..MAX_COORD_VALUE-1] to [1..MAX_COORD_VALUE]
			intval = ReadUBitLong( COORD_INTEGER_BITS ) + 1;
		}

		// If there's a fraction, read it in
		if ( fractval )
		{
			fractval = ReadUBitLong( COORD_FRACTIONAL_BITS );
		}

		// Calculate the correct floating point value
		value = intval + ((float)fractval * COORD_RESOLUTION);

		// Fixup the sign if negative.
		if ( signbit )
			value = -value;
	}

	return value;
}

float old_bf_read::ReadBitCoordMP( EBitCoordType coordType )
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::ReadBitCoordMP" );
#endif
	bool bIntegral = ( coordType == kCW_Integral );
	bool bLowPrecision = ( coordType == kCW_LowPrecision );  

	int		intval=0,fractval=0,signbit=0;
	float	value = 0.0;


	bool bInBounds = ReadOneBit() ? true : false;

	if ( bIntegral )
	{
		// Read the required integer and fraction flags
		intval = ReadOneBit();
		// If we got either parse them, otherwise it's a zero.
		if ( intval )
		{
			// Read the sign bit
			signbit = ReadOneBit();

			// If there's an integer, read it in
			// Adjust the integers from [0..MAX_COORD_VALUE-1] to [1..MAX_COORD_VALUE]
			if ( bInBounds )
			{
				value = ReadUBitLong( COORD_INTEGER_BITS_MP ) + 1;
			}
			else
			{
				value = ReadUBitLong( COORD_INTEGER_BITS ) + 1;
			}
		}
	}
	else
	{
		// Read the required integer and fraction flags
		intval = ReadOneBit();

		// Read the sign bit
		signbit = ReadOneBit();

		// If we got either parse them, otherwise it's a zero.
		if ( intval )
		{
			if ( bInBounds )
			{
				intval = ReadUBitLong( COORD_INTEGER_BITS_MP ) + 1;
			}
			else
			{
				intval = ReadUBitLong( COORD_INTEGER_BITS ) + 1;
			}
		}

		// If there's a fraction, read it in
		fractval = ReadUBitLong( bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS );

		// Calculate the correct floating point value
		value = intval + ((float)fractval * ( bLowPrecision ? COORD_RESOLUTION_LOWPRECISION : COORD_RESOLUTION ) );
	}

	// Fixup the sign if negative.
	if ( signbit )
		value = -value;

	return value;
}

float old_bf_read::ReadBitCellCoord( int bits, EBitCoordType coordType )
{
#if defined( BB_PROFILING )
	VPROF( "bf_write::ReadBitCoordMP" );
#endif
	bool bIntegral = ( coordType == kCW_Integral );
	bool bLowPrecision = ( coordType == kCW_LowPrecision );  

	int		intval=0,fractval=0;
	float	value = 0.0;

	if ( bIntegral )
	{
		value = ReadUBitLong( bits );
	}
	else
	{
		intval = ReadUBitLong( bits );

		// If there's a fraction, read it in
		fractval = ReadUBitLong( bLowPrecision ? COORD_FRACTIONAL_BITS_MP_LOWPRECISION : COORD_FRACTIONAL_BITS );

		// Calculate the correct floating point value
		value = intval + ((float)fractval * ( bLowPrecision ? COORD_RESOLUTION_LOWPRECISION : COORD_RESOLUTION ) );
	}

	return value;
}


void old_bf_read::ReadBitVec3Coord( Vector& fa )
{
	int		xflag, yflag, zflag;

	// This vector must be initialized! Otherwise, If any of the flags aren't set, 
	// the corresponding component will not be read and will be stack garbage.
	fa.Init( 0, 0, 0 );

	xflag = ReadOneBit();
	yflag = ReadOneBit(); 
	zflag = ReadOneBit();

	if ( xflag )
		fa[0] = ReadBitCoord();
	if ( yflag )
		fa[1] = ReadBitCoord();
	if ( zflag )
		fa[2] = ReadBitCoord();
}

float old_bf_read::ReadBitNormal (void)
{
	// Read the sign bit
	int	signbit = ReadOneBit();

	// Read the fractional part
	unsigned int fractval = ReadUBitLong( NORMAL_FRACTIONAL_BITS );

	// Calculate the correct floating point value
	float value = (float)fractval * NORMAL_RESOLUTION;

	// Fixup the sign if negative.
	if ( signbit )
		value = -value;

	return value;
}

void old_bf_read::ReadBitVec3Normal( Vector& fa )
{
	int xflag = ReadOneBit();
	int yflag = ReadOneBit(); 

	if (xflag)
		fa[0] = ReadBitNormal();
	else
		fa[0] = 0.0f;

	if (yflag)
		fa[1] = ReadBitNormal();
	else
		fa[1] = 0.0f;

	// The first two imply the third (but not its sign)
	int znegative = ReadOneBit();

	float fafafbfb = fa[0] * fa[0] + fa[1] * fa[1];
	if (fafafbfb < 1.0f)
		fa[2] = sqrt( 1.0f - fafafbfb );
	else
		fa[2] = 0.0f;

	if (znegative)
		fa[2] = -fa[2];
}

void old_bf_read::ReadBitAngles( QAngle& fa )
{
	Vector tmp;
	ReadBitVec3Coord( tmp );
	fa.Init( tmp.x, tmp.y, tmp.z );
}

int old_bf_read::ReadChar()
{
	return ReadSBitLong(sizeof(char) << 3);
}

int old_bf_read::ReadByte()
{
	return ReadUBitLong(sizeof(unsigned char) << 3);
}

int old_bf_read::ReadShort()
{
	return ReadSBitLong(sizeof(short) << 3);
}

int old_bf_read::ReadWord()
{
	return ReadUBitLong(sizeof(unsigned short) << 3);
}

int32 old_bf_read::ReadLong()
{
	return ReadSBitLong(sizeof(int32) << 3);
}

int64 old_bf_read::ReadLongLong()
{
	int64 retval;
	uint *pLongs = (uint*)&retval;

	// Read the two DWORDs according to network endian
	const short endianIndex = 0x0100;
	byte *idx = (byte*)&endianIndex;
	pLongs[*idx++] = ReadUBitLong(sizeof(int32) << 3);
	pLongs[*idx] = ReadUBitLong(sizeof(int32) << 3);

	return retval;
}

float old_bf_read::ReadFloat()
{
	float ret;
	Assert( sizeof(ret) == 4 );
	ReadBits(&ret, 32);

	// Swap the float, since ReadBits reads raw data
	LittleFloat( &ret, &ret );
	return ret;
}

bool old_bf_read::ReadBytes(void *pOut, int nBytes)
{
	ReadBits(pOut, nBytes << 3);
	return !IsOverflowed();
}

bool old_bf_read::ReadString( char *pStr, int maxLen, bool bLine, int *pOutNumChars )
{
	Assert( maxLen != 0 );

	bool bTooSmall = false;
	int iChar = 0;
	while(1)
	{
		char val = ReadChar();
		if ( val == 0 )
			break;
		else if ( bLine && val == '\n' )
			break;

		if ( iChar < (maxLen-1) )
		{
			pStr[iChar] = val;
			++iChar;
		}
		else
		{
			bTooSmall = true;
		}
	}

	// Make sure it's null-terminated.
	Assert( iChar < maxLen );
	pStr[iChar] = 0;

	if ( pOutNumChars )
		*pOutNumChars = iChar;

	return !IsOverflowed() && !bTooSmall;
}

bool old_bf_read::ReadWString( wchar_t *pStr, int maxLen, bool bLine, int *pOutNumChars )
{
	Assert( maxLen != 0 );

	bool bTooSmall = false;
	int iChar = 0;
	while(1)
	{
		wchar val = ReadShort();
		if ( val == 0 )
			break;
		else if ( bLine && val == L'\n' )
			break;

		if ( iChar < (maxLen-1) )
		{
			pStr[iChar] = val;
			++iChar;
		}
		else
		{
			bTooSmall = true;
		}
	}

	// Make sure it's null-terminated.
	Assert( iChar < maxLen );
	pStr[iChar] = 0;

	if ( pOutNumChars )
		*pOutNumChars = iChar;

	return !IsOverflowed() && !bTooSmall;
}


char* old_bf_read::ReadAndAllocateString( bool *pOverflow )
{
	char str[2048];
	
	int nChars;
	bool bOverflow = !ReadString( str, sizeof( str ), false, &nChars );
	if ( pOverflow )
		*pOverflow = bOverflow;

	// Now copy into the output and return it;
	char *pRet = new char[ nChars + 1 ];
	for ( int i=0; i <= nChars; i++ )
		pRet[i] = str[i];

	return pRet;
}

void old_bf_read::ExciseBits( int startbit, int bitstoremove )
{
	int endbit = startbit + bitstoremove;
	int remaining_to_end = m_nDataBits - endbit;

	bf_write temp;
	temp.StartWriting( (void *)m_pData, m_nDataBits << 3, startbit );

	Seek( endbit );

	for ( int i = 0; i < remaining_to_end; i++ )
	{
		temp.WriteOneBit( ReadOneBit() );
	}

	Seek( startbit );
	
	m_nDataBits -= bitstoremove;
	m_nDataBytes = m_nDataBits >> 3;
}


