//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Simple encoder/decoder
//
//===========================================================================//

#ifndef COMMON_VALVE_FONT
#define COMMON_VALVE_FONT

#include "simplecodec.h"
#include <time.h>

namespace ValveFont
{

//
// Encodes the font buffer, buffer size is increased automatically
//
inline void EncodeFont( CUtlBuffer &buffer )
{
	srand( (unsigned) time( NULL ) );

	int numBytes = buffer.TellPut();
	int numSultBytes = 0x10 + ( rand() % 0x10 );
	
	char const *szTag = "VFONT1";
	int numTagBytes = strlen( szTag );

	for ( int k = 0; k < numSultBytes; ++ k )
		buffer.PutUnsignedChar( k );
	for ( int k = 0; k < numTagBytes; ++ k )
		buffer.PutUnsignedChar( szTag[ k ] );

	SimpleCodec::EncodeBuffer( ( unsigned char * ) buffer.Base(), numBytes, numSultBytes );
}

//
// Decodes font buffer, returns true if successful, buffer's put pointer is positioned
// at the end of the font data.
// Returns false on failure.
//
inline bool DecodeFont( CUtlBuffer &buffer )
{
	int numTotalBytes = buffer.TellPut();

	char const *szTag = "VFONT1";
	int numTagBytes = strlen( szTag );
	
	if ( numTotalBytes <= numTagBytes )
		return false;

	if ( memcmp( (( unsigned char * ) buffer.Base() ) + numTotalBytes - numTagBytes,
		szTag, numTagBytes ) )
		return false;

	numTotalBytes -= numTagBytes;
	SimpleCodec::DecodeBuffer( ( unsigned char * ) buffer.Base(), numTotalBytes );

	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, numTotalBytes );
	return true;
}

}; // namespace ValveFont

#endif // COMMON_VALVE_FONT

