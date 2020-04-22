//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Simple encoder/decoder
//
//===========================================================================//

#ifndef COMMON_SIMPLE_CODEC
#define COMMON_SIMPLE_CODEC

#include <stdlib.h>

namespace SimpleCodec
{

//
// Encodes the buffer in place
//	pvBuffer		pointer to the base of the buffer, buffer length is "numBytes + numSultBytes"
//	numBytes		number of real data bytes already in the buffer
//	numSultBytes	number of sult bytes to be placed after the buffer data
//
inline void EncodeBuffer( unsigned char *pvBuffer, int numBytes, unsigned char numSultBytes )
{
	unsigned char xx = 0xA7;

	// Add some sult in the very end
	Assert( numSultBytes > 0 );
	pvBuffer[ numBytes + numSultBytes - 1 ] = numSultBytes;
	-- numSultBytes;

	// Store sult data
	for ( unsigned char *pvSult = pvBuffer + numBytes;
		  numSultBytes -- > 0; ++ pvSult )
	{
		*pvSult = rand() % 0x100;
		xx ^= ( *pvSult + 0xA7 ) % 0x100;
	}

	// Hash the buffer
	for ( ; numBytes -- > 0; ++ pvBuffer )
	{
		unsigned char v = *pvBuffer;
		v ^= xx;
		xx = ( v + 0xA7 ) % 0x100;
		*pvBuffer = v;
	}
}

//
// Decodes the buffer in place
//	pvBuffer		pointer to the base of the encoded buffer
//	numBytes		number of buffer bytes on input
//					on return contains the number of data bytes decoded (excludes sult)
//
inline void DecodeBuffer( unsigned char *pvBuffer, int &numBytes )
{
	unsigned char xx = 0xA7;

	// Discover the number of sult bytes
	unsigned char numSultBytes = pvBuffer[ numBytes - 1 ];
	numBytes -= numSultBytes;
	-- numSultBytes;

	// Recover sult data
	for ( unsigned char *pvSult = pvBuffer + numBytes;
		  numSultBytes -- > 0; ++ pvSult )
	{
		xx ^= ( *pvSult + 0xA7 ) % 0x100;
	}

	// Hash the buffer
	for ( int numBufBytes = numBytes; numBufBytes -- > 0; ++ pvBuffer )
	{
		unsigned char v = *pvBuffer;
		v ^= xx;
		xx = ( *pvBuffer + 0xA7 ) % 0x100;
		*pvBuffer = v;
	}
}


}; // namespace SimpleCodec

#endif // COMMON_SIMPLE_CODEC