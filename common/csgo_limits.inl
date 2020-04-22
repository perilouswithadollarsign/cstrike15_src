//========= Copyright c 1996-2014, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

static inline void CopyStringTruncatingMalformedUTF8Tail( char *pchBuffer, char const *szSrc, int numBufferBytes )
{
	//
	// There's a bug in Steam client where the clan tag may be truncated
	// in the middle of UTF-8 sequence. Also we have a more restrictive
	// clan tags length in the game to not bloat the name string.
	//
	
	if ( numBufferBytes <= 0 )
	{
		Assert( numBufferBytes > 0 );
		return;
	}

	if ( numBufferBytes == 1 )
	{
		pchBuffer[0] = 0;
		return;
	}

	int nStrLen = V_strlen( szSrc );
	if ( nStrLen <= 0 )
	{
		pchBuffer[ 0 ] = 0;
		return;
	}

	// Now we know that input is non-empty string, and buffer can hold at least one character
	// let's figure out how many characters can fit?

	if ( nStrLen > numBufferBytes - 1 )
		nStrLen = numBufferBytes - 1;

	//
	// Check what the last character in the source string is?
	//
	// The last character is a UTF-8 sequence begin character or sequence continuation character
	// See how many characters must have been in the end of the string?
	for ( int numCheck = 0; numCheck < nStrLen; ++ numCheck )
	{
		int idxCheck = nStrLen - 1 - numCheck;
		if ( ( szSrc[ idxCheck ] & ( 0x80 | 0x40 ) ) == ( 0x80 | 0x40 ) )
		{
			// This is the start of UTF8 sequence
			int numCharactersSequenceLengthSeen = numCheck + 1;
			int numCharactersSequenceMarker = 2; // at least two bytes
			for ( uint8 uiMarker = uint8( uint8( szSrc[ idxCheck ] ) << 2 );
				( ( uiMarker & 0x80 ) != 0 );
				uiMarker <<= 1 )
				++ numCharactersSequenceMarker;
			
			// If the marker shows more characters than seen, discard the tail
			if ( numCharactersSequenceMarker != numCharactersSequenceLengthSeen )
				nStrLen -= numCharactersSequenceLengthSeen;
			break;
		}
		else if ( ( szSrc[ idxCheck ] & ( 0x80 | 0x40 ) ) == ( 0x80 ) )
		{
			// This is a continuation of UTF8 sequence
			if ( idxCheck )
				continue;	// Keep looking backwards
			else
			{	// Scanned all the way back to start of string and found no UTF8 sequence start character
				nStrLen = 0;
				break;
			}
		}
		else
		{
			// Looking at regular lower ASCII 0x00-0x7F character, valid for string termination
			nStrLen -= numCheck;
			break;
		}
	}

	if ( !nStrLen )
	{
		// This was an entirely invalid string, copy a questionmark
		pchBuffer[0] = '?';
		pchBuffer[1] = 0;
		return;
	}

	// Copy whatever we determined
	V_strncpy( pchBuffer, szSrc, nStrLen + 1 );
}
